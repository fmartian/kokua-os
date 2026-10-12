/**
 * @file kokuafloateravatarattachments.h
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

#ifndef KOKUA_KOKUAFLOATERAVATARATTACHMENTS_H
#define KOKUA_KOKUAFLOATERAVATARATTACHMENTS_H

#include "llfloater.h"

class LLAvatarName;
class LLMessageSystem;
class LLScrollListCtrl;

class KokuaFloaterAvatarAttachments : public LLFloater
{
    friend class LLFloaterReg;
public:
    bool postBuild() override;
    void onOpen(const LLSD& key) override;

    // Called from process_object_properties() for every ObjectProperties reply
    static void processObjectProperties(LLMessageSystem* msg);

private:
    KokuaFloaterAvatarAttachments(const LLSD& key);
    virtual ~KokuaFloaterAvatarAttachments() {}

    void refreshList();
    void updateButtons();
    void onClickCreatorProfile();
    void onClickObjectProfile();
    void setCell(const LLUUID& object_id, S32 column, const std::string& value);

    static void onAvatarNameCallback(LLHandle<KokuaFloaterAvatarAttachments> handle, const LLAvatarName& av_name);
    static void onCreatorNameCallback(LLHandle<KokuaFloaterAvatarAttachments> handle, const LLUUID& object_id, const LLUUID& creator_id, const LLAvatarName& av_name);

    LLUUID              mAvatarID;
    LLScrollListCtrl*   mAttachmentList;
    // Attachments in the list and the creator we are showing for each of them
    std::map<LLUUID, LLUUID> mCreators;
};

#endif
