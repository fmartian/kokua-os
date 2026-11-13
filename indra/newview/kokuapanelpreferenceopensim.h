/**
 * @file kokuapanelpreferenceopensim.h
 * @brief preference panel for opensim
 *
 * $LicenseInfo:firstyear=2018&license=viewerlgpl$
 * Second Life Viewer Source Code
 * Copyright (C) 2018, Linden Research, Inc.
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

#ifndef KOKUAFLOATERPREFERENCEOPENSIM_H
#define KOKUAFLOATERPREFERENCEOPENSIM_H

#if OPENSIM
#include "llcontrol.h"
#include "llfloaterpreference.h"

// <FS:AW  opensim preferences>
class LLPanelPreferenceOpensim : public LLPanelPreference
{
    LOG_CLASS(LLPanelPreferenceOpensim);

public:
    LLPanelPreferenceOpensim();
    ~LLPanelPreferenceOpensim();

    /*virtual*/ void apply();
    /*virtual*/ void cancel(const std::vector<std::string> settings_to_skip = {});

protected:
    boost::signals2::connection mGridListChangedCallbackConnection;
    boost::signals2::connection mGridAddedCallbackConnection;

    void onOpen(const LLSD& key);
    /*virtual*/ bool postBuild();

    void onSelectGrid();
    void onClickClearGrid();
    void onClickRefreshGrid();
    void onClickAddGrid();
    void addedGrid(bool success);
    void onClickRemoveGrid();
    void onClickUpdateGrid();
    bool removedGrid(const LLSD& notification, const LLSD& response);
    void onClickClearDebugSearchURL();
    void onClickPickDebugSearchURL();

    void refreshGridList(bool success = true);
    LLScrollListCtrl* mGridListControl;

private:
    LLLineEditor* mEditorGridName;
    LLLineEditor* mEditorGridURI;
    LLLineEditor* mEditorLoginPage;
    LLLineEditor* mEditorHelperURI;
    LLLineEditor* mEditorWebsite;
    LLLineEditor* mEditorSupport;
    LLLineEditor* mEditorRegister;
    LLLineEditor* mEditorPassword;
    LLLineEditor* mEditorSearch;
    LLLineEditor* mEditorGridMessage;
    LLComboBox*   mComboPlatform;
    LLButton*     mButtonRemove;
    LLButton*     mButtonUpdate;

    std::string mCurrentGrid;
    std::string mUpdateGrid;
    std::string mSelectedGrid;
};
#endif // OPENSIM
#endif
