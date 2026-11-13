/**
 * @file kokuapanelprefernceopensim.cpp
 * @author Frederick Martian
 * @brief The OpenSimulator preference panel
 *
 * $LicenseInfo:firstyear=2025&license=viewerlgpl$
 * Second Life Viewer Source Code
 * Copyright (C) 2025, Linden Research, Inc.
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
 *
 * Linden Research, Inc., 945 Battery Street, San Francisco, CA  94111  USA
 * $/LicenseInfo$
 */
// Precompiled header
#include "llviewerprecompiledheaders.h"

#include "kokuapanelpreferenceopensim.h"

#include "lllineeditor.h"
#include "llviewernetwork.h"
#include "llscrolllistctrl.h"
#include "llcombobox.h"
#include "llnotificationsutil.h"
#include "llpanellogin.h"
#include "lllogininstance.h"
#include "llctrlselectioninterface.h"

#if OPENSIM
// <FS:AW  opensim search support>
static bool callback_clear_debug_search(const LLSD& notification, const LLSD& response)
{
    S32 option = LLNotificationsUtil::getSelectedOption(notification, response);
    if (option == 0) // YES
    {
        gSavedSettings.setString("SearchURLDebug", "");
    }
    return false;
}

static bool callback_pick_debug_search(const LLSD& notification, const LLSD& response)
{
    S32 option = LLNotificationsUtil::getSelectedOption(notification, response);
    if (option == 0) // YES
    {
        std::string url;
// TODO: FM is this needed
//      if (LFSimFeatureHandler::instanceExists())
//      {
//          url = LFSimFeatureHandler::getInstance()->searchURL();
//      }
//      else
        {
            if (LLGridManager::getInstance()->isInOpenSim())
            {
                LLLoginInstance* loginHdl = LLLoginInstance::getInstance();
                url = loginHdl->hasResponse(GRID_SEARCH)
                          ? loginHdl->getResponse(GRID_SEARCH).asString()
                          : gSavedSettings.getString("SearchURLOpenSim");
            }
            else
            {
                // not in OpenSim means we are in SL or SL beta
                url = gSavedSettings.getString("SearchURL");
            }
        }
        gSavedSettings.setString("SearchURLDebug", url);
    }
    return false;
}

static LLPanelInjector<LLPanelPreferenceOpensim> t_pref_opensim("panel_preference_opensim");

LLPanelPreferenceOpensim::LLPanelPreferenceOpensim() :
    LLPanelPreference(),
    mGridListControl(NULL),
    mGridListChangedCallbackConnection(),
    mGridAddedCallbackConnection()
{
    mCommitCallbackRegistrar.add("Pref.ClearDebugSearchURL", boost::bind(&LLPanelPreferenceOpensim::onClickClearDebugSearchURL, this));
    mCommitCallbackRegistrar.add("Pref.PickDebugSearchURL", boost::bind(&LLPanelPreferenceOpensim::onClickPickDebugSearchURL, this));
    mCommitCallbackRegistrar.add("Pref.AddGrid", boost::bind(&LLPanelPreferenceOpensim::onClickAddGrid, this));
    mCommitCallbackRegistrar.add("Pref.ClearGrid", boost::bind(&LLPanelPreferenceOpensim::onClickClearGrid, this));
    mCommitCallbackRegistrar.add("Pref.RefreshGrid", boost::bind(&LLPanelPreferenceOpensim::onClickRefreshGrid, this));
    mCommitCallbackRegistrar.add("Pref.RemoveGrid", boost::bind(&LLPanelPreferenceOpensim::onClickRemoveGrid, this));
    mCommitCallbackRegistrar.add("Pref.UpdateGrid", boost::bind(&LLPanelPreferenceOpensim::onClickUpdateGrid, this));
}

LLPanelPreferenceOpensim::~LLPanelPreferenceOpensim()
{
    if (mGridListChangedCallbackConnection.connected())
    {
        mGridListChangedCallbackConnection.disconnect();
    }

    if (mGridAddedCallbackConnection.connected())
    {
        mGridAddedCallbackConnection.disconnect();
    }
}

static std::string getPlatformString(LLSD grid_info)
{
    std::string platform;
    if (grid_info.has(GRID_IS_SYSTEM_GRID_VALUE) && grid_info[GRID_IS_SYSTEM_GRID_VALUE].asBoolean())
    {
        platform = "SecondLife";
    }
    else if (grid_info.has(GRID_PLATFORM))
    {
        platform = grid_info[GRID_PLATFORM].asString();
    }
    else if (grid_info.has(GRID_GATEKEEPER))
    {
        platform = "OpenSim";
    }
    else
    {
        platform = "OpenSim";
    }
    return platform;
}

bool LLPanelPreferenceOpensim::postBuild()
{
    mButtonRemove      = findChild<LLButton>("remove_grid");
    mButtonUpdate      = findChild<LLButton>("update_grid");
    mEditorGridName    = findChild<LLLineEditor>("name_edit");
    mEditorGridURI     = findChild<LLLineEditor>("grid_uri_edit");
    mEditorLoginPage   = findChild<LLLineEditor>("login_page_edit");
    mEditorHelperURI   = findChild<LLLineEditor>("helper_uri_edit");
    mEditorWebsite     = findChild<LLLineEditor>("website_edit");
    mEditorSupport     = findChild<LLLineEditor>("support_edit");
    mEditorRegister    = findChild<LLLineEditor>("register_edit");
    mEditorPassword    = findChild<LLLineEditor>("password_edit");
    mEditorSearch      = findChild<LLLineEditor>("search_edit");
    mEditorGridMessage = findChild<LLLineEditor>("message_edit");
    mComboPlatform     = findChild<LLComboBox>("platform_select");
    mGridListControl   = getChild<LLScrollListCtrl>("grid_list");
    mGridListControl->setCommitCallback(boost::bind(&LLPanelPreferenceOpensim::onSelectGrid, this));
    mGridListChangedCallbackConnection =
        LLGridManager::getInstance()->addGridListChangedCallback(boost::bind(&LLPanelPreferenceOpensim::refreshGridList, this, _1));
    // the refresh is only triggered when the list changes, so do a refresh ourself
    refreshGridList();

    return LLPanelPreference::postBuild();
}

void LLPanelPreferenceOpensim::onOpen(const LLSD& key)
{
    mCurrentGrid = LLGridManager::getInstance()->getGrid();
    if (mComboPlatform)
    {
        mComboPlatform ->clear();
    }
    mEditorGridName->clear();
    mEditorGridURI->clear();
    mEditorLoginPage->clear();
    mEditorHelperURI->clear();
    mEditorWebsite->clear();
    mEditorSupport->clear();
    mEditorRegister->clear();
    mEditorPassword->clear();
    mEditorSearch->clear();
    mEditorGridMessage->clear();
}

void LLPanelPreferenceOpensim::onSelectGrid()
{
    LLGridManager* gridMgr = LLGridManager::getInstance();
    LLSD grid_info;
    if (!mSelectedGrid.empty())
    {
        grid_info = gridMgr->getGridData(mSelectedGrid);
        if (grid_info[GRID_LABEL_VALUE] != mEditorGridName->getText())
        {
            grid_info[GRID_LABEL_VALUE] = mEditorGridName->getText();
            gridMgr->updateGrid(mSelectedGrid, grid_info);
        }
    }
    mSelectedGrid = mGridListControl->getSelectedValue();
    grid_info     = gridMgr->getGridData(mSelectedGrid);

    // We do not allow to edit or remove system grids or the currently connected grid
    bool disabled = gridMgr->isSystemGrid(mSelectedGrid) ||
                    mSelectedGrid == gridMgr->getGrid() &&
                    LLLoginInstance::getInstance()->authSuccess();
    mButtonRemove->setEnabled(!disabled);
    mButtonUpdate->setEnabled(!disabled);

    mEditorGridName->setText(grid_info[GRID_LABEL_VALUE].asString());
    mComboPlatform->setSimple(getPlatformString(grid_info));
    mEditorGridURI->setText(grid_info[GRID_LOGIN_URI_VALUE][0].asString());
    mEditorLoginPage->setText(grid_info[GRID_LOGIN_PAGE_VALUE].asString());
    mEditorHelperURI->setText(grid_info[GRID_HELPER_URI_VALUE].asString());
    mEditorWebsite->setText(grid_info[GRID_ABOUT].asString());
    mEditorSupport->setText(grid_info[GRID_HELP].asString());
    mEditorRegister->setText(grid_info[GRID_REGISTER_NEW_ACCOUNT].asString());
    mEditorPassword->setText(grid_info[GRID_FORGOT_PASSWORD].asString());
    mEditorSearch->setText(grid_info[GRID_SEARCH].asString());
    mEditorGridMessage->setText(grid_info[GRID_MESSAGE].asString());
}

void LLPanelPreferenceOpensim::apply()
{
    LLGridManager* gridMgr = LLGridManager::getInstance();
    if (!mSelectedGrid.empty())
    {
        LLSD grid_info              = gridMgr->getGridData(mSelectedGrid);
        grid_info[GRID_LABEL_VALUE] = mEditorGridName->getText();
    }
    gridMgr->saveGridList();
    LLPanelLogin::updateServer();
}

void LLPanelPreferenceOpensim::cancel(const std::vector<std::string> settings_to_skip)
{
    LLGridManager* gridMgr = LLGridManager::getInstance();
    gridMgr->resetGridList();
    gridMgr->setGridChoice(mCurrentGrid);
    LLPanelLogin::updateServer();
}

void LLPanelPreferenceOpensim::onClickAddGrid()
{
    std::string new_grid = getChild<LLLineEditor>("add_grid")->getText();

    if (!new_grid.empty())
    {
        LLGridManager* gridMgr = LLGridManager::getInstance();
        getChild<LLUICtrl>("grid_management_panel")->setEnabled(false);
        if (mGridAddedCallbackConnection.connected())
        {
            mGridAddedCallbackConnection.disconnect();
        }
        mUpdateGrid.clear();
        gridMgr->addGridListChangedCallback(boost::bind(&LLPanelPreferenceOpensim::addedGrid, this, _1));
        gridMgr->downloadGridInfo(new_grid);
    }
}

void LLPanelPreferenceOpensim::addedGrid(bool success)
{
    if (mGridAddedCallbackConnection.connected())
    {
        mGridAddedCallbackConnection.disconnect();
    }

    if (success)
    {
        const std::string& new_grid = getChild<LLLineEditor>("add_grid")->getText();

        for (auto row : mGridListControl->getAllData())
        {
            if (new_grid.find(row->getColumn(1)->getValue().asString()) != std::string::npos)
            {
                row->setSelected(true);
                mGridListControl->scrollToShowSelected();
                onSelectGrid();
                break;
            }
        }
        onClickClearGrid();
    }
}

void LLPanelPreferenceOpensim::onClickClearGrid()
{
    getChild<LLLineEditor>("add_grid")->clear();
}

void LLPanelPreferenceOpensim::onClickRefreshGrid()
{
    std::string grid_name = mGridListControl->getSelectedValue();
    getChild<LLUICtrl>("grid_management_panel")->setEnabled(false);
    mUpdateGrid = grid_name;
    LLGridManager::getInstance()->downloadGridInfo(grid_name);
}

void LLPanelPreferenceOpensim::onClickRemoveGrid()
{
    LLGridManager* gridMgr   = LLGridManager::getInstance();
    std::string    grid_name = mGridListControl->getSelectedValue();
    LLSD args;

    if (grid_name != gridMgr->getGrid())
    {
        args["REMOVE_GRID"] = grid_name;
        LLSD payload        = grid_name;
        LLNotificationsUtil::add("ConfirmRemoveGrid", args, payload, boost::bind(&LLPanelPreferenceOpensim::removedGrid, this, _1, _2));
    }
    else
    {
        args["REMOVE_GRID"] = gridMgr->getGridLabel();
        LLNotificationsUtil::add("CanNotRemoveConnectedGrid", args);
    }
}

bool LLPanelPreferenceOpensim::removedGrid(const LLSD& notification, const LLSD& response)
{
    const S32 option = LLNotificationsUtil::getSelectedOption(notification, response);
    if (0 == option)
    {
        std::string grid = notification["payload"].asString();
        getChild<LLUICtrl>("grid_management_panel")->setEnabled(false);
        mEditorGridName->clear();
        mComboPlatform->clear();
        mEditorGridURI->clear();
        mEditorLoginPage->clear();
        mEditorHelperURI->clear();
        mEditorWebsite->clear();
        mEditorSupport->clear();
        mEditorRegister->clear();
        mEditorPassword->clear();
        mEditorSearch->clear();
        mEditorGridMessage->clear();
        LLGridManager::getInstance()->removeGrid(grid);
        LLPanelLogin::updateServer();
    }
    return false;
}

void LLPanelPreferenceOpensim::onClickUpdateGrid()
{
    std::string grid_name = mGridListControl->getSelectedValue();
    if (!grid_name.empty())
    {
        LLGridManager* gridMgr   = LLGridManager::getInstance();
        LLSD           grid_info = gridMgr->getGridData(mSelectedGrid);

        grid_info[GRID_LABEL_VALUE] = mEditorGridName->getText();
//      grid_info[GRID_PLATFORM] = mComboPlatform->getSelectedValue();
        grid_info[GRID_LOGIN_URI_VALUE][0] = mEditorGridURI->getText();
        grid_info[GRID_LOGIN_PAGE_VALUE] = mEditorLoginPage->getText();
        grid_info[GRID_HELPER_URI_VALUE] = mEditorHelperURI->getText();
        grid_info[GRID_ABOUT] = mEditorWebsite->getText();
        grid_info[GRID_HELP] = mEditorSupport->getText();
        grid_info[GRID_REGISTER_NEW_ACCOUNT] = mEditorRegister->getText();
        grid_info[GRID_FORGOT_PASSWORD] = mEditorPassword->getText();
        grid_info[GRID_SEARCH] = mEditorSearch->getText();
        grid_info[GRID_MESSAGE] = mEditorGridMessage->getText();

        gridMgr->updateGrid(mSelectedGrid, grid_info);
    }
}

void LLPanelPreferenceOpensim::refreshGridList(bool success)
{
    LLPanelLogin::updateServer();

    getChild<LLUICtrl>("grid_management_panel")->setEnabled(true);

    if (!mGridListControl)
    {
        LL_WARNS() << "No GridListControl - bug or out of memory" << LL_ENDL;
        return;
    }

    std::string selection = mUpdateGrid.empty() ? mGridListControl->getSelectedValue().asString() : mUpdateGrid;
    mGridListControl->operateOnAll(LLCtrlListInterface::OP_DELETE);
    mGridListControl->sortByColumnIndex(0, true);

    LLGridManager* gridMgr = LLGridManager::getInstance();
    LLSD           element;
    const std::string connected_grid = gridMgr->getGrid();
    std::map<std::string, std::string> known_grids = gridMgr->getKnownGrids();
    std::set<std::string> known_platforms({ "SecondLife", "OpenSim" });

    std::map<std::string, std::string>::iterator grid_iter;
    for (grid_iter = known_grids.begin(); grid_iter != known_grids.end(); grid_iter++)
    {
        if (!grid_iter->first.empty() && !grid_iter->second.empty())
        {
            LLURI login_uri = LLURI(gridMgr->getLoginURI(grid_iter->first));
            const std::string font = (connected_grid == grid_iter->first) ? "SansSerifBold" : "SansSerif";

            int col                           = 0;
            element["id"]                     = grid_iter->first;
            element["columns"][col]["column"] = "grid_label";
            element["columns"][col]["value"]  = grid_iter->second;
            element["columns"][col]["font"]   = font;
            col++;
            element["columns"][col]["column"] = "login_uri";
            element["columns"][col]["value"]  = login_uri.authority();
            element["columns"][col]["font"]   = font;

            mGridListControl->addElement(element);

            LLSD grid_info = gridMgr->getGridData(grid_iter->first);
            known_platforms.insert(getPlatformString(grid_info));
        }
    }

    mComboPlatform->clearRows();
    std::set<std::string>::iterator platform_iter;
    for (platform_iter = known_platforms.begin(); platform_iter != known_platforms.end(); platform_iter++)
    {
        mComboPlatform->add(*platform_iter);
    }

    if (!selection.empty())
    {
        mGridListControl->deselectAllItems();
        mGridListControl->selectByValue(selection);
    }
}

void LLPanelPreferenceOpensim::onClickClearDebugSearchURL()
{
    LLNotificationsUtil::add("ConfirmClearDebugSearchURL", LLSD(), LLSD(), callback_clear_debug_search);
}

void LLPanelPreferenceOpensim::onClickPickDebugSearchURL()
{
    LLNotificationsUtil::add("ConfirmPickDebugSearchURL", LLSD(), LLSD(), callback_pick_debug_search);
}
#endif
