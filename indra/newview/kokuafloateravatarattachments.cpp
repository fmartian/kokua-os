/**
 * @file kokuafloateravatarattachments.cpp
 * @author Kentron Katana
 * @brief A floater to list the attachments worn by an avatar
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation;
 * version 2.1 of the License only.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */
#include "llviewerprecompiledheaders.h"
#include "kokuafloateravatarattachments.h"

#include "llagent.h"
#include "llavataractions.h"
#include "llavatarnamecache.h"
#include "llfloaterreg.h"
#include "llscrolllistctrl.h"
#include "llscrolllistitem.h"
#include "llselectmgr.h"
#include "lltoolgrab.h"
#include "lltrans.h"
#include "llviewerjointattachment.h"
#include "llviewerobject.h"
#include "llviewerobjectlist.h"
#include "llviewerregion.h"
#include "llvoavatar.h"
#include "message.h"
#include "RRInterface.h"

// Column order in floater_avatar_attachments.xml
const S32 COLUMN_NAME = 0;
const S32 COLUMN_CREATOR = 1;

KokuaFloaterAvatarAttachments::KokuaFloaterAvatarAttachments(const LLSD& key)
:   LLFloater(key),
    mAttachmentList(NULL)
{
}

bool KokuaFloaterAvatarAttachments::postBuild()
{
    mAttachmentList = getChild<LLScrollListCtrl>("attachment_list");
    mAttachmentList->setCommitOnSelectionChange(true);
    mAttachmentList->setCommitCallback(boost::bind(&KokuaFloaterAvatarAttachments::updateButtons, this));
    mAttachmentList->setDoubleClickCallback(boost::bind(&KokuaFloaterAvatarAttachments::onClickObjectProfile, this));
    childSetAction("refresh_btn", boost::bind(&KokuaFloaterAvatarAttachments::refreshList, this));
    childSetAction("creator_profile_btn", boost::bind(&KokuaFloaterAvatarAttachments::onClickCreatorProfile, this));
    childSetAction("object_profile_btn", boost::bind(&KokuaFloaterAvatarAttachments::onClickObjectProfile, this));
    childSetAction("touch_btn", boost::bind(&KokuaFloaterAvatarAttachments::onClickTouch, this));

    return true;
}

void KokuaFloaterAvatarAttachments::onOpen(const LLSD& key)
{
    mAvatarID = key.asUUID();
    LLAvatarNameCache::get(mAvatarID, boost::bind(&KokuaFloaterAvatarAttachments::onAvatarNameCallback, getDerivedHandle<KokuaFloaterAvatarAttachments>(), _2));
    refreshList();
}

void KokuaFloaterAvatarAttachments::refreshList()
{
    mAttachmentList->deleteAllItems();
    mCreators.clear();
    updateButtons();

    LLViewerObject* objectp = gObjectList.findObject(mAvatarID);
    LLVOAvatar* avatarp = objectp ? objectp->asAvatar() : NULL;
    if (!avatarp)
    {
        mAttachmentList->setCommentText(getString("avatar_not_found"));
        return;
    }

    // The name and the creator aren't part of the object updates, so they have to be
    // requested from the region, which only sends them for objects being selected
    typedef std::map<LLViewerRegion*, std::vector<U32> > request_map_t;
    request_map_t requests;

    for (LLVOAvatar::attachment_map_t::iterator iter = avatarp->mAttachmentPoints.begin();
         iter != avatarp->mAttachmentPoints.end(); ++iter)
    {
        LLViewerJointAttachment* attachment = iter->second;
        if (!attachment)
        {
            continue;
        }

        for (LLViewerJointAttachment::attachedobjs_vec_t::iterator attachment_iter = attachment->mAttachedObjects.begin();
             attachment_iter != attachment->mAttachedObjects.end(); ++attachment_iter)
        {
            LLViewerObject* attached_object = attachment_iter->get();
            if (!attached_object || attached_object->isDead() || !attached_object->getRegion())
            {
                continue;
            }

            LLSD row;
            row["id"] = attached_object->getID();
            row["columns"][0]["column"] = "name";
            row["columns"][0]["type"] = "text";
            row["columns"][0]["value"] = LLTrans::getString("RetrievingData");
            row["columns"][1]["column"] = "creator";
            row["columns"][1]["type"] = "text";
            row["columns"][1]["value"] = LLTrans::getString("RetrievingData");
            row["columns"][2]["column"] = "attach_point";
            row["columns"][2]["type"] = "text";
            row["columns"][2]["value"] = LLTrans::getString(attachment->getName());
            mAttachmentList->addElement(row);

            mCreators[attached_object->getID()] = LLUUID::null;
            requests[attached_object->getRegion()].push_back(attached_object->getLocalID());
        }
    }

    if (mCreators.empty())
    {
        mAttachmentList->setCommentText(getString("no_attachments"));
        return;
    }

    LLMessageSystem* msg = gMessageSystem;
    for (request_map_t::iterator iter = requests.begin(); iter != requests.end(); ++iter)
    {
        // Select and deselect straight away, the reply to the select is all we want
        for (S32 pass = 0; pass < 2; ++pass)
        {
            bool start_new_message = true;
            for (std::vector<U32>::iterator id_iter = iter->second.begin(); id_iter != iter->second.end(); ++id_iter)
            {
                if (start_new_message)
                {
                    msg->newMessageFast(pass == 0 ? _PREHASH_ObjectSelect : _PREHASH_ObjectDeselect);
                    msg->nextBlockFast(_PREHASH_AgentData);
                    msg->addUUIDFast(_PREHASH_AgentID, gAgent.getID());
                    msg->addUUIDFast(_PREHASH_SessionID, gAgent.getSessionID());
                    start_new_message = false;
                }

                msg->nextBlockFast(_PREHASH_ObjectData);
                msg->addU32Fast(_PREHASH_ObjectLocalID, *id_iter);

                if (msg->isSendFull(NULL))
                {
                    msg->sendReliable(iter->first->getHost());
                    start_new_message = true;
                }
            }

            if (!start_new_message)
            {
                msg->sendReliable(iter->first->getHost());
            }
        }
    }
}

void KokuaFloaterAvatarAttachments::updateButtons()
{
    LLUUID object_id = mAttachmentList->getCurrentID();
    std::map<LLUUID, LLUUID>::iterator found = mCreators.find(object_id);
    getChildView("creator_profile_btn")->setEnabled(found != mCreators.end() && found->second.notNull());
    getChildView("object_profile_btn")->setEnabled(found != mCreators.end());

    getChildView("touch_btn")->setEnabled(canTouchSelected());
}

void KokuaFloaterAvatarAttachments::onClickCreatorProfile()
{
    std::map<LLUUID, LLUUID>::iterator found = mCreators.find(mAttachmentList->getCurrentID());
    if (found != mCreators.end() && found->second.notNull())
    {
        LLAvatarActions::showProfile(found->second);
    }
}

void KokuaFloaterAvatarAttachments::onClickObjectProfile()
{
    LLViewerObject* objectp = gObjectList.findObject(mAttachmentList->getCurrentID());
    if (objectp && !objectp->isDead())
    {
        // Object Profile shows whatever is selected
        LLSelectMgr::getInstance()->deselectAll();
        LLSelectMgr::getInstance()->selectObjectAndFamily(objectp, false, true);
        LLFloaterReg::showInstance("task_properties");
    }
}

LLViewerObject* KokuaFloaterAvatarAttachments::getSelectedAttachment() const
{
    LLViewerObject* objectp = gObjectList.findObject(mAttachmentList->getCurrentID());
    return (objectp && !objectp->isDead()) ? objectp : NULL;
}

// Same test as the hand cursor in LLToolPie: some prim of the attachment has a touch handler
static bool handles_touch(LLViewerObject* objectp)
{
    if (objectp->flagHandleTouch())
    {
        return true;
    }
    for (LLViewerObject* child : objectp->getChildren())
    {
        if (child && handles_touch(child))
        {
            return true;
        }
    }
    return false;
}

bool KokuaFloaterAvatarAttachments::canTouchSelected() const
{
    LLViewerObject* objectp = getSelectedAttachment();
    if (!objectp || !handles_touch(objectp))
    {
        return false;
    }
#if 0
//MK
    // Grey the button out when a restriction would stop the touch anyway
    if (gRRenabled && !gAgent.mRRInterface.canTouch(objectp, objectp->getPositionAgent()))
    {
        return false;
    }
//mk
#endif
    return true;
}

void KokuaFloaterAvatarAttachments::onClickTouch()
{
    LLViewerObject* objectp = getSelectedAttachment();
    if (!objectp)
    {
        return;
    }

    // There is no point on the surface to report, the list was clicked rather than the object
    LLPickInfo pick;
    pick.mObjectID = objectp->getID();
    pick.mIntersection = objectp->getPositionAgent();
    pick.mPosGlobal = objectp->getPositionGlobal();

#if 0
//MK
    if (gRRenabled && !gAgent.mRRInterface.canTouch(objectp, pick.mIntersection))
    {
        return;
    }
//mk
#endif

    send_ObjectGrab_message(objectp, pick, LLVector3::zero);
    send_ObjectDeGrab_message(objectp, pick);
}

void KokuaFloaterAvatarAttachments::setCell(const LLUUID& object_id, S32 column, const std::string& value)
{
    LLScrollListItem* item = mAttachmentList->getItem(LLSD(object_id));
    if (item && item->getColumn(column))
    {
        item->getColumn(column)->setValue(value);
        mAttachmentList->setNeedsSort();
    }
}

// static
void KokuaFloaterAvatarAttachments::processObjectProperties(LLMessageSystem* msg)
{
    LLFloaterReg::const_instance_list_t& instances = LLFloaterReg::getFloaterList("avatar_attachments");
    if (instances.empty())
    {
        return;
    }

    S32 count = msg->getNumberOfBlocksFast(_PREHASH_ObjectData);
    for (S32 i = 0; i < count; i++)
    {
        LLUUID object_id;
        msg->getUUIDFast(_PREHASH_ObjectData, _PREHASH_ObjectID, object_id, i);

        for (LLFloaterReg::const_instance_list_t::const_iterator iter = instances.begin(); iter != instances.end(); ++iter)
        {
            KokuaFloaterAvatarAttachments* floater = dynamic_cast<KokuaFloaterAvatarAttachments*>(*iter);
            if (!floater)
            {
                continue;
            }

            std::map<LLUUID, LLUUID>::iterator found = floater->mCreators.find(object_id);
            if (found == floater->mCreators.end())
            {
                continue;
            }

            LLUUID creator_id;
            std::string name;
            msg->getUUIDFast(_PREHASH_ObjectData, _PREHASH_CreatorID, creator_id, i);
            msg->getStringFast(_PREHASH_ObjectData, _PREHASH_Name, name, i);

            found->second = creator_id;
            floater->setCell(object_id, COLUMN_NAME, name);
            floater->updateButtons();
            LLAvatarNameCache::get(creator_id, boost::bind(&KokuaFloaterAvatarAttachments::onCreatorNameCallback, floater->getDerivedHandle<KokuaFloaterAvatarAttachments>(), object_id, _1, _2));
        }
    }
}

// static
void KokuaFloaterAvatarAttachments::onAvatarNameCallback(LLHandle<KokuaFloaterAvatarAttachments> handle, const LLAvatarName& av_name)
{
    KokuaFloaterAvatarAttachments* floater = handle.get();
    if (floater)
    {
        LLStringUtil::format_map_t args;
        args["[NAME]"] = av_name.getCompleteName();
        floater->setTitle(floater->getString("title_with_name", args));
    }
}

// static
void KokuaFloaterAvatarAttachments::onCreatorNameCallback(LLHandle<KokuaFloaterAvatarAttachments> handle, const LLUUID& object_id, const LLUUID& creator_id, const LLAvatarName& av_name)
{
    KokuaFloaterAvatarAttachments* floater = handle.get();
    if (!floater)
    {
        return;
    }

    // The list may have been refreshed while the name was on its way
    std::map<LLUUID, LLUUID>::iterator found = floater->mCreators.find(object_id);
    if (found != floater->mCreators.end() && found->second == creator_id)
    {
        floater->setCell(object_id, COLUMN_CREATOR, av_name.getCompleteName());
    }
}
