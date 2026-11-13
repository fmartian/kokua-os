/**
 * @file kokuaviewernetwork.cpp
 * @author Frederick Martian
 * @brief Networking constants and methods for viewer with OpenSim support.
 *
 * $LicenseInfo:firstyear=2010&license=viewerlgpl$
 * Kokua Viewer Source Code
 * Copyright (C) 2025, Frederick Martian
 * Copyright (C) 2012, arminweatherwax@lavabit.com
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

#include "llviewerprecompiledheaders.h"

#if OPENSIM
#include "llnotificationsutil.h"
#include "llviewernetwork.h"
#include "llmaterialtable.h"
#include "fscorehttputil.h"
#include "llsdserialize.h"
#include "llsecapi.h"

const std::string FLAG_SET_CURRENT          = "FLAG_CURRENT";
const std::string FLAG_TEMPORARY            = "FLAG_TEMPORARY";
const std::string DEPRECATED                = "DEPRECATED";

const std::string GRID_SEARCH               = "search";
const std::string GRID_MESSAGE              = "message";
const std::string GRID_PLATFORM             = "platform";
const std::string GRID_GATEKEEPER           = "gatekeeper";
const std::string GRID_REGISTER_NEW_ACCOUNT = "register";
const std::string GRID_FORGOT_PASSWORD      = "password";
const std::string GRID_NICK_VALUE           = "gridnick";
const std::string DOMAIN_LINDENLAB_COM      = "lindenlab.com";

const char* DEFAULT_HOP_BASE = "hop://%s/"; // <AW: hop:// protocol>

// this assumes that there is anyway only one uri saved
std::string LLGridManager::getLoginURI(LLSD& grid_info)
{
    if (grid_info.has(GRID_LOGIN_URI_VALUE) && grid_info[GRID_LOGIN_URI_VALUE].isArray())
    {
        return grid_info[GRID_LOGIN_URI_VALUE].beginArray()->asString();
    }
    return std::string("");
}


std::string LLGridManager::getLoginURI(const std::string& grid_name)
{
    std::string grid = getGrid(grid_name);
    if (!grid.empty())
    {
        return getLoginURI(mGridList[grid]);
    }
    return std::string("");
}

void LLGridManager::saveGridList()
{
    // filter out just those which are not hardcoded anyway
    LLSD output_grid_list = LLSD::emptyMap();
    if (mGridList[mGrid].has(FLAG_TEMPORARY))
    {
        mGridList[mGrid].erase(FLAG_TEMPORARY);
    }
    for (LLSD::map_iterator grid_iter = mGridList.beginMap(); grid_iter != mGridList.endMap(); grid_iter++)
    {
        if (!(grid_iter->first.empty() || grid_iter->second.isUndefined() || grid_iter->second.has(GRID_IS_SYSTEM_GRID_VALUE) ||
              grid_iter->second.has(FLAG_TEMPORARY) || grid_iter->second.has(DEPRECATED)))
        {
            output_grid_list[grid_iter->first] = grid_iter->second;
        }
    }
    llofstream llsd_xml;
    llsd_xml.open(mGridFile.c_str(), std::ios::out | std::ios::binary);
    LLSDSerialize::toPrettyXML(output_grid_list, llsd_xml);
    llsd_xml.close();
}

bool LLGridManager::isReadyToLogin()
{
    return true;
}

void LLGridManager::resetGridList()
{
    initialize(mGridFile);
}

LLSD LLGridManager::getGridData(const std::string& grid)
{
    std::string grid_name = getGrid(grid);
    return mGridList[grid_name];
}

void LLGridManager::downloadGridInfo(const std::string& loginuri)
{
    LLSD        grid_info = LLSD::emptyMap();
    std::string grid_name = utf8str_tolower(loginuri);
    // grid should be in the form of a dns address
    // but also support localhost:9000 or localhost:9000/login
    if (!grid_name.empty() && grid_name.find_first_not_of("abcdefghijklmnopqrstuvwxyz1234567890-_.:/@% ") != std::string::npos)
    {
        LLSD args;
        args["GRID"] = grid_name;
        LLNotificationsUtil::add("InvalidGrid", args);
        return;
    }

    // trim last slash
    size_t pos = grid_name.find_last_of("/");
    if ((grid_name.length() - 1) == pos)
    {
        if (!mGridList.has(grid_name)) // deal with hand edited entries *sigh*
        {
            grid_name.erase(pos);
        }
    }

    // trim region from hypergrid uris
    std::string grid_trimmed = trimHypergrid(grid_name);
    if (grid_trimmed != grid_name)
    {
        grid_name       = grid_trimmed;
        grid_info["HG"] = true;
    }

    // FIRE-24068 allow https support - based on patch by unregi resident
    std::string uri;
    std::string match  = "://";
    size_t find_scheme = grid_name.find(match);
    if (std::string::npos != find_scheme)
    {
        uri = grid_name;                                  // assign the full URI
        grid_name.erase(0, find_scheme + match.length()); // trim the protocol
    }
    else
    {
        // no protocol was specified let's assume http
        // GRID_VALUE remains unchanged
        uri = "http://" + grid_name;
    }
    grid_info[GRID_VALUE] = grid_name;
    grid_info[GRID_LOGIN_URI_VALUE] = LLSD::emptyArray();
    grid_info[GRID_LOGIN_URI_VALUE].append(uri);

    if (std::string::npos != uri.find(DOMAIN_LINDENLAB_COM))
    {
        grid_info[GRID_IS_SYSTEM_GRID_VALUE] = true;
    }

    grid_info[FLAG_SET_CURRENT] = (grid_name == getGrid());
    fetchGrid(grid_info, false);
}

void LLGridManager::addGrid(LLSD& grid_info, LLXMLNodePtr xmlData, LLDate& lastModified)
{
    for (LLXMLNode* node = xmlData->getFirstChild(); node != NULL; node = node->getNextSibling())
    {
        std::string check;
        check = "login";
        if (node->hasName(check))
        {
            // // allow redirect but not spoofing
            // LLURI uri (node->getTextContents());
            // std::string authority = uri.authority();
            // if(! authority.empty())
            // {
            //   grid_info[GRID_VALUE] = authority;
            // }

            grid_info[GRID_LOGIN_URI_VALUE] = LLSD::emptyArray();
            grid_info[GRID_LOGIN_URI_VALUE].append(node->getTextContents());
            LL_DEBUGS("GridManager") << "[\"" << check << "\"]: " << grid_info[GRID_LOGIN_URI_VALUE] << LL_ENDL;
            continue;
        }
        check = "gridname";
        if (node->hasName(check))
        {
            grid_info[GRID_LABEL_VALUE] = node->getTextContents();
            LL_DEBUGS("GridManager") << "[\"" << check << "\"]: " << grid_info[GRID_LABEL_VALUE] << LL_ENDL;
            continue;
        }
        check = GRID_NICK_VALUE;
        if (node->hasName(check))
        {
            grid_info[check] = node->getTextContents();
            LL_DEBUGS("GridManager") << "[\"" << check << "\"]: " << grid_info[check] << LL_ENDL;
            continue;
        }
        check = GRID_GATEKEEPER;
        if (node->hasName(check))
        {
            LLURI gatekeeper(node->getTextContents());
            grid_info[check] = gatekeeper.authority();

            LL_DEBUGS("GridManager") << "[\"" << check << "\"]: " << grid_info[check] << LL_ENDL;
            continue;
        }
        check = "welcome";
        if (node->hasName(check))
        {
            grid_info[GRID_LOGIN_PAGE_VALUE] = node->getTextContents();
            LL_DEBUGS("GridManager") << "[\"" << check << "\"]: " << grid_info[GRID_LOGIN_PAGE_VALUE] << LL_ENDL;
            continue;
        }
        check = GRID_REGISTER_NEW_ACCOUNT;
        if (node->hasName(check))
        {
            grid_info[check] = node->getTextContents();
            LL_DEBUGS("GridManager") << "[\"" << check << "\"]: " << grid_info[check] << LL_ENDL;
            continue;
        }
        check = GRID_FORGOT_PASSWORD;
        if (node->hasName(check))
        {
            grid_info[check] = node->getTextContents();
            LL_DEBUGS("GridManager") << "[\"" << check << "\"]: " << grid_info[check] << LL_ENDL;
            continue;
        }
        check = GRID_HELP;
        if (node->hasName(check))
        {
            grid_info[check] = node->getTextContents();
            LL_DEBUGS("GridManager") << "[\"" << check << "\"]: " << grid_info[check] << LL_ENDL;
            continue;
        }
        check = GRID_ABOUT;
        if (node->hasName(check))
        {
            grid_info[check] = node->getTextContents();
            LL_DEBUGS("GridManager") << "[\"" << check << "\"]: " << grid_info[check] << LL_ENDL;
            continue;
        }
        check = GRID_SEARCH;
        if (node->hasName(check))
        {
            grid_info[check] = node->getTextContents();
            LL_DEBUGS("GridManager") << "[\"" << check << "\"]: " << grid_info[check] << LL_ENDL;
            continue;
        }
        check = GRID_WEB_PROFILE_VALUE;
        if (node->hasName(check))
        {
            grid_info[check] = node->getTextContents();
            LL_DEBUGS("GridManager") << "[\"" << check << "\"]: " << grid_info[check] << LL_ENDL;
            continue;
        }
        check = "profileuri";
        if (node->hasName(check))
        {
            grid_info[GRID_WEB_PROFILE_VALUE] = node->getTextContents();
            LL_DEBUGS("GridManager") << "[\"" << check << "\"]: " << grid_info[check] << LL_ENDL;
            continue;
        }
        check = GRID_SENDGRIDINFO;
        if (node->hasName(check))
        {
            grid_info[check] = node->getTextContents();
            LL_DEBUGS("GridManager") << "[\"" << check << "\"]: " << grid_info[check] << LL_ENDL;
            continue;
        }
        check = GRID_DIRECTORY_FEE;
        if (node->hasName(check))
        {
            grid_info[check] = node->getTextContents();
            LL_DEBUGS("GridManager") << "[\"" << check << "\"]: " << grid_info[check] << LL_ENDL;
            continue;
        }
        check = GRID_PLATFORM;
        if (node->hasName(check))
        {
            grid_info[check] = node->getTextContents();
            LL_DEBUGS("GridManager") << "[\"" << check << "\"]: " << grid_info[check] << LL_ENDL;
            continue;
        }
        check = GRID_MESSAGE;
        if (node->hasName(check))
        {
            grid_info[check] = node->getTextContents();
            LL_DEBUGS("GridManager") << "[\"" << check << "\"]: " << grid_info[check] << LL_ENDL;
            continue;
        }
        check = "helperuri";
        if (node->hasName(check))
        {
            grid_info[GRID_HELPER_URI_VALUE] = node->getTextContents();
            LL_DEBUGS("GridManager") << "[\"" << check << "\"]: " << grid_info[GRID_HELPER_URI_VALUE] << LL_ENDL;
            // don't continue, also check the next
        }
        check = "economy";
        if (node->hasName(check))
        { // sic! economy and helperuri is 2 names for the same
            grid_info[GRID_HELPER_URI_VALUE] = node->getTextContents();
            LL_DEBUGS("GridManager") << "[\"" << check << "\"]: " << grid_info[GRID_HELPER_URI_VALUE] << LL_ENDL;
        }
    }

    std::string grid_name = grid_info[GRID_VALUE].asString();
    std::string slurl_base(llformat(DEFAULT_HOP_BASE, grid_name.c_str())); // <AW: hop:// protocol>
    grid_info[GRID_SLURL_BASE] = slurl_base;

    if (lastModified.notNull())
    {
        grid_info["LastModified"] = lastModified;
    }
    addGrid(grid_info, false);
}

static void gridDownloadError(const LLSD& aData, LLGridManager* mOwner, LLSD& grid_info, LLGridManager::AddState mState)
{
    LLCore::HttpStatus status = LLCoreHttpUtil::HttpCoroutineAdapter::getStatusFromLLSD(aData);

    if (HTTP_GATEWAY_TIME_OUT == status.getType()) // gateway timeout ... well ... retry once >_>
    {
        if (LLGridManager::FETCH == mState)
        {
            // retry one time
            mOwner->fetchGrid(grid_info, true);
        }
    }
    else if (status.getType() == HTTP_NOT_MODIFIED && LLGridManager::LEGACY != mState) // not modified
    {
        mOwner->addGrid(grid_info, false);
    }
    else if (HTTP_INTERNAL_ERROR == status.getType() && LLGridManager::LOCAL == mState) // add localhost even if its not up
    {
        mOwner->addGrid(grid_info, false);
        // since we know now that its not up we cold also start it
    }
    else if (LLGridManager::LEGACY == mState) // we did TRYLEGACY and failed
    {
        LLSD args;
        args["GRID"] = grid_info[GRID_VALUE];
        // Could not add [GRID] to the grid list.
        std::string reason_dialog = "Server didn't provide grid info: ";
        reason_dialog.append(grid_info["last_http_error"]);
        reason_dialog.append("\nPlease check if the loginuri is correct and");
        args["REASON"] = reason_dialog;
        //[REASON] contact support of [GRID].
        LLNotificationsUtil::add("CantAddGrid", args);

        LL_WARNS() << "No legacy login page. Giving up for " << grid_info[GRID_VALUE] << LL_ENDL;
        mOwner->addGrid(grid_info, true);
    }
    else
    {
        // remember the error we got when trying to get grid info where we expect it
        std::ostringstream last_error;
        last_error << status.getType() << " " << status.getMessage();
        grid_info["last_http_error"] = last_error.str();

        mOwner->fetchGridLegacy(grid_info);
    }
}

static void gridDownloadComplete(const LLSD& aData, LLGridManager* mOwner, LLSD& grid_info, LLGridManager::AddState mState)
{
    LLSD header = aData[LLCoreHttpUtil::HttpCoroutineAdapter::HTTP_RESULTS][LLCoreHttpUtil::HttpCoroutineAdapter::HTTP_RESULTS_HEADERS];
    LLCore::HttpStatus status =
        LLCoreHttpUtil::HttpCoroutineAdapter::getStatusFromLLSD(aData[LLCoreHttpUtil::HttpCoroutineAdapter::HTTP_RESULTS]);

    const LLSD::Binary& rawData = aData[LLCoreHttpUtil::HttpCoroutineAdapter::HTTP_RESULTS_RAW].asBinary();

    // LL_DEBUGS("GridManager") << mData->grid[GRID_VALUE] << " status: " << getStatus() << " reason: " << getReason() << LL_ENDL;
    if (HTTP_OK == status.getType()) // OK
    {
        if (LLGridManager::LEGACY == mState)
        {
            grid_info[GRID_IS_SYSTEM_GRID_VALUE] = true;
            mOwner->addGrid(grid_info, false);
        }
        else
        {
            LL_DEBUGS("GridManager") << "Parsing gridinfo xml file from " << grid_info[GRID_VALUE] << LL_ENDL;

            LLXMLNodePtr xmlData;
            bool retval = LLXMLNode::parseBuffer((const char *)rawData.data(), (U64)rawData.size(), xmlData, "fetch");
            if (retval)
            {
                LLDate lastModified = LLDate::now();
                bool   isMap = header.isMap();
                if (isMap)
                {
                    bool hasLast = header.has(HTTP_OUT_HEADER_LAST_MODIFIED);
                    if (hasLast)
                    {
                        lastModified = header[HTTP_OUT_HEADER_LAST_MODIFIED].asDate();
                    }
                }
                mOwner->addGrid(grid_info, xmlData, lastModified);
            }
            else
            {
                LLSD args;
                args["GRID"] = grid_info[GRID_VALUE];
                // Could not add [GRID] to the grid list.
                args["REASON"] = "Server provided broken grid info xml. Please";
                //[REASON] contact support of [GRID].
                LLNotificationsUtil::add("CantAddGrid", args);

                LL_WARNS() << " Could not parse grid info xml from server." << grid_info[GRID_VALUE] << " skipping." << LL_ENDL;
                mOwner->addGrid(grid_info, true);
            }
        }
    }
    else if (HTTP_INTERNAL_ERROR == status.getType() && LLGridManager::LOCAL == mState) // add localhost even if its not up
    {
        mOwner->addGrid(grid_info, false);
    }
    else
    {
        gridDownloadError(aData[LLCoreHttpUtil::HttpCoroutineAdapter::HTTP_RESULTS], mOwner, grid_info, mState);
    }
}

/// LLGridManager::addGrid - add a grid to the grid list, populating the needed values
/// if they're not populated yet.
void LLGridManager::addGrid(LLSD& grid_info, bool failed)
{
    if (!failed)
    {
        std::string grid_name = utf8str_tolower(grid_info[GRID_VALUE]);
        if (!grid_name.empty())
        {
            prepareCommon(grid_name, grid_info);
            if (isSystemGrid(grid_info))
            {
                prepareSystem(grid_name, grid_info);
            }
            else if (!grid_info.has(GRID_LOGIN_IDENTIFIER_TYPES))
            {
                // non system grids and grids that haven't already been configured with values
                // get both types of credentials.
                grid_info[GRID_LOGIN_IDENTIFIER_TYPES] = LLSD::emptyArray();
                grid_info[GRID_LOGIN_IDENTIFIER_TYPES].append(CRED_IDENTIFIER_TYPE_AGENT);
                grid_info[GRID_LOGIN_IDENTIFIER_TYPES].append(CRED_IDENTIFIER_TYPE_ACCOUNT);
            }

            bool is_current = false;
            if (grid_info.has(FLAG_SET_CURRENT))
            {
                is_current = grid_info[FLAG_SET_CURRENT];
                grid_info.erase(FLAG_SET_CURRENT);
            }

            bool list_changed = finalizeGrid(grid_name, grid_info);
            if (is_current)
            {
                LL_DEBUGS("GridManager") << "Selected grid is " << grid_name << LL_ENDL;
                setGridChoice(grid_name);
            }

            if (list_changed)
            {
                mGridListChangedSignal(true);
            }
        }
        else
        {
            failed = true;
        }
    }

    if (failed)
    {
        mGridListChangedSignal(false);
    }
}

void LLGridManager::prepareCommon(const std::string &grid, LLSD& grid_info)
{
    // populate the other values if they don't exist
    if (!grid_info.has(GRID_LABEL_VALUE))
    {
        grid_info[GRID_LABEL_VALUE] = grid;
        LL_WARNS() << "No \"gridname\" found in grid info, setting to " << grid_info[GRID_LABEL_VALUE].asString() << LL_ENDL;
    }

    if (!grid_info.has(GRID_NICK_VALUE))
    {
        grid_info[GRID_NICK_VALUE] = grid;
        LL_WARNS() << "No \"gridnick\" found in grid info, setting to " << grid_info[GRID_NICK_VALUE].asString() << LL_ENDL;
    }
}

void LLGridManager::prepareSystem(const std::string& grid, LLSD& grid_info)
{
    // if the grid data doesn't include any of the URIs, then
    // generate them from the grid
    if (!grid_info.has(GRID_LOGIN_URI_VALUE))
    {
        grid_info[GRID_LOGIN_URI_VALUE] = LLSD::emptyArray();
        grid_info[GRID_LOGIN_URI_VALUE].append(std::string("https://") + grid + "/cgi-bin/login.cgi");
        LL_WARNS() << "Adding Legacy Login Service at:" << grid_info[GRID_LOGIN_URI_VALUE].asString() << LL_ENDL;
    }

    // Populate to the default values
    if (!grid_info.has(GRID_LOGIN_PAGE_VALUE))
    {
        grid_info[GRID_LOGIN_PAGE_VALUE] = std::string("http://") + grid + "/app/login/";
        LL_WARNS() << "Adding Legacy Login Screen at:" << grid_info[GRID_LOGIN_PAGE_VALUE].asString() << LL_ENDL;
    }
    if (!grid_info.has(GRID_HELPER_URI_VALUE))
    {
        LL_WARNS() << "Adding Legacy Economy at:" << grid_info[GRID_HELPER_URI_VALUE].asString() << LL_ENDL;
        grid_info[GRID_HELPER_URI_VALUE] = std::string("https://") + grid + "/helpers/";
    }
}

bool LLGridManager::finalizeGrid(const std::string& grid, LLSD& grid_info)
{
    bool list_changed = false;
    if (!mGridList.has(grid)) // new grid
    {
        LL_DEBUGS("GridManager") << "new grid" << LL_ENDL;
        if (!grid_info.has("USER_DELETED") && !grid_info.has("DEPRECATED"))
        {
            // finally add the grid \o/
            mGridList[grid] = grid_info;
            list_changed    = true;
            LL_DEBUGS("GridManager") << "Adding new entry: " << grid << LL_ENDL;
        }
        else if (grid_info.has("DEPRECATED"))
        {
            // add the deprecated entry but hide it
            // so it doesn't get used from the user list
            mGridList[grid] = grid_info;
            LL_DEBUGS("GridManager") << "Marking entry as deprecated : " << grid << LL_ENDL;
        }
        else
        {
            LL_DEBUGS("GridManager") << "Removing entry marked for deletion: " << grid << LL_ENDL;
        }
    }
    else
    {
        LL_DEBUGS("GridManager") << "existing grid" << LL_ENDL;

        LLSD existing_grid = mGridList[grid];
        if (existing_grid.has("DEPRECATED"))
        {
            LL_DEBUGS("GridManager") << "Removing entry marked as deprecated in the fallback list: " << grid << LL_ENDL;
            mGridList[grid] = grid_info;
            list_changed    = true;
        }
        else if (grid_info.has("USER_DELETED"))
        {
            // entries from the fallback list can't be deleted
            // hide them instead
            mGridList[grid] = grid_info;
            list_changed    = true;
            LL_DEBUGS("GridManager") << "Entry marked for deletion: " << grid << LL_ENDL;
        }
        else if (!existing_grid.has("LastModified"))
        {
            // lack of "LastModified" means existing_grid is from fallback list,
            // assume its anyway older and override with the new entry

            mGridList[grid] = grid_info;
            list_changed    = true;
            LL_DEBUGS("GridManager") << "Using custom entry: " << grid << LL_ENDL;
        }
        else if (grid_info.has("LastModified"))
        {
            LLDate testing_newer = grid_info["LastModified"];
            LLDate existing      = existing_grid["LastModified"];

            LL_DEBUGS("GridManager") << "testing_newer " << testing_newer << " existing " << existing << LL_ENDL;

            if (testing_newer.secondsSinceEpoch() > existing.secondsSinceEpoch())
            {
                // existing_grid is older, override.

                mGridList[grid] = grid_info;
                list_changed    = true;
                LL_DEBUGS("GridManager") << "Updating entry: " << grid << LL_ENDL;
            }
        }
        else
        {
            LL_DEBUGS("GridManager") << "Same or newer entry already present: " << grid << LL_ENDL;
        }
    }
    return list_changed;
}

void LLGridManager::fetchGrid(LLSD& grid_info, bool retry)
{
    AddState state = LOCAL;
    std::string grid_name = utf8str_tolower(grid_info[GRID_VALUE]);
    std::string uri = getLoginURI(grid_info);
    if (std::string::npos != uri.find("127.0.0.1") || std::string::npos != uri.find("localhost"))
    {
        char host_name[255];
        host_name[254] = '\0';
        gethostname(host_name, 254);
        if (std::string::npos == uri.find(host_name))
        {
            state = retry ? RETRY : FETCH;
        }
    }
    if (state == LOCAL)
    {
        LL_DEBUGS("GridManager") << "state = LOCAL" << LL_ENDL;
    }

    size_t pos = uri.find_last_of("/");
    if ((uri.length() - 1) != pos)
    {
        uri.append("/");
    }
    uri.append("get_grid_info");

    LL_DEBUGS("GridManager") << "get_grid_info uri: " << uri << LL_ENDL;

    LLCore::HttpOptions::ptr_t httpOpts(new LLCore::HttpOptions);
    if (grid_info.has("LastModified"))
    {
        LLDate saved_value = grid_info["LastModified"];
        httpOpts->setLastModified((time_t)saved_value.secondsSinceEpoch());
    }
    httpOpts->setTimeout(5);
    FSCoreHttpUtil::callbackHttpGetRaw(uri, boost::bind(gridDownloadComplete, _1, this, grid_info, state),
                                       boost::bind(gridDownloadError, _1, this, grid_info, state),
                                       LLCore::HttpHeaders::ptr_t(), httpOpts);
}

void LLGridManager::fetchGridLegacy(LLSD& grid_info)
{
    std::string grid = utf8str_tolower(grid_info[GRID_VALUE]);
    std::string uri  = "https://";
    uri.append(grid);
    size_t pos = uri.find_last_of("/");
    if ((uri.length() - 1) != pos)
    {
        uri.append("/");
    }
    uri.append("cgi-bin/login.cgi");

    LL_WARNS() << "No gridinfo found. Trying if legacy login page exists: " << uri << LL_ENDL;
    LLCore::HttpOptions::ptr_t httpOpts(new LLCore::HttpOptions);
    httpOpts->setTimeout(5);

    FSCoreHttpUtil::callbackHttpGetRaw(uri, boost::bind(gridDownloadComplete, _1, this, grid_info, LEGACY),
                                       boost::bind(gridDownloadError, _1, this, grid_info, LEGACY),
                                       LLCore::HttpHeaders::ptr_t(), httpOpts);
}

void LLGridManager::removeGrid(const std::string& grid)
{
    std::string grid_name = getGrid(grid);
    bool can_delete = mGridList.has(grid_name) && !isSystemGrid(grid_name);
    if (can_delete)
    {
        mGridList.erase(grid_name);
    }
    mGridListChangedSignal(can_delete);
}

void LLGridManager::updateGrid(const std::string& grid, LLSD grid_info)
{
    std::string grid_name = getGrid(grid);
    bool do_update = !grid_name.empty();
    if (do_update)
    {
        mGridList[grid_name] = grid_info;
    }
    mGridListChangedSignal(do_update);
}

void LLGridManager::setWebProfileUrl(const std::string webProfileUrl)
{
    std::string grid_name = getGrid(mGrid);
    if (!grid_name.empty() && !mGridList[grid_name].isUndefined())
    {
        mGridList[grid_name][GRID_WEB_PROFILE_VALUE] = webProfileUrl;
    }
}

void LLGridManager::updateGridPlatform(void)
{
    EGridPlatform = GP_NOTSET;

    std::vector<std::string> uris;
    getLoginURIs(uris);
    if (uris.empty())
    {
        LL_DEBUGS("GridManager") << "uri is empty, setting grid platform to NOTHING." << LL_ENDL;
        return;
    }

    LLStringUtil::toLower(uris[0]);
    LLURI login_uri = LLURI(uris[0]);

    // Test for full name to prevent spoofing
    if (login_uri.authority().find("login.agni.lindenlab.com") == 0)
    {
        LL_DEBUGS("GridManager") << "uri: " << login_uri.authority() << " setting grid platform to SL MAIN" << LL_ENDL;
        EGridPlatform = GP_SLMAIN;
        return;
    }
    else if (login_uri.authority().find(DOMAIN_LINDENLAB_COM) != std::string::npos) // here is no real money
    {
        LL_DEBUGS("GridManager") << "uri: " << login_uri.authority() << " setting grid platform to SL BETA" << LL_ENDL;
        EGridPlatform = GP_SLBETA;
        return;
    }

    LLSD gridData = getGridData(mGrid);
    if (gridData.has(GRID_PLATFORM))
    {
        if (gridData[GRID_PLATFORM].asString() == "OpenSim")
        {
            LL_DEBUGS("GridManager") << "uri: " << uris[0] << "setting grid platform to OpenSim" << LL_ENDL;
            EGridPlatform = GP_OPENSIM;
            return;
        }
        else if (gridData[GRID_PLATFORM].asString() == "Aurora")
        {
            LL_DEBUGS("GridManager") << "uri: " << uris[0] << "setting grid platform to Aurora" << LL_ENDL;
            EGridPlatform = GP_AURORA;
            return;
        }
    }

    LLURI login_page = LLURI(getLoginPage());
    if (login_page.authority().find(DOMAIN_LINDENLAB_COM) != std::string::npos)
    {
        setGridChoice(MAINGRID);
        return;
    }

    LL_DEBUGS("GridManager") << "uri: " << login_uri.authority() << " setting grid platform to OPENSIM" << LL_ENDL;
    EGridPlatform = GP_OPENSIM;

    // <FS:Beq> FIRE-31628 Use OpenSim collision sounds when in OpenSim
    LLMaterialTable::basic.replaceCollsionSounds(isInOpenSim());
}

std::string LLGridManager::getGridByProbing(const std::string& probe_for, bool case_sensitive) const
{
    std::string ret;
    ret = getGridByHostName(probe_for, case_sensitive);
    if (ret.empty())
    {
        ret = getGridByGridNick(probe_for, case_sensitive);
    }
    if (ret.empty())
    {
        ret = getGridByLabel(probe_for, case_sensitive);
    }
    return ret;
}

std::string LLGridManager::getGridByLabel(const std::string& grid_label, bool case_sensitive) const
{
    return grid_label.empty() ? std::string() : getGridByAttribute(GRID_LABEL_VALUE, grid_label, case_sensitive);
}

std::string LLGridManager::getGridByGridNick(const std::string& grid_nick, bool case_sensitive) const
{
    return grid_nick.empty() ? std::string() : getGridByAttribute(GRID_NICK_VALUE, grid_nick, case_sensitive);
}

std::string LLGridManager::getGridByHostName(const std::string& host_name, bool case_sensitive) const
{
    return host_name.empty() ? std::string() : getGridByAttribute(GRID_VALUE, host_name, case_sensitive);
}

std::string LLGridManager::getGridByAttribute(const std::string& attribute, const std::string& attribute_value, bool case_sensitive) const
{
    if (attribute.empty() || attribute_value.empty())
    {
        return std::string();
    }

    for (LLSD::map_const_iterator grid_iter = mGridList.beginMap(); grid_iter != mGridList.endMap(); grid_iter++)
    {
        if (grid_iter->second.has(attribute))
        {
            if (0 == (case_sensitive ? LLStringUtil::compareStrings(attribute_value, grid_iter->second[attribute].asString())
                                     : LLStringUtil::compareInsensitive(attribute_value, grid_iter->second[attribute].asString())))
            {
                return grid_iter->first;
            }
        }
    }
    return std::string();
}

//<AW opensim>
std::string LLGridManager::trimHypergrid(const std::string& trim)
{
    std::size_t pos;
    std::string grid = trim;

    pos = grid.find_last_of(":");
    if (pos != std::string::npos)
    {
        std::string part = grid.substr(pos + 1, grid.length() - 1);
        // in hope numbers only is a good guess for it's a port number
        if (std::string::npos != part.find_first_not_of("1234567890/"))
        {
            // and erase if not
            grid.erase(pos, grid.length() - 1);
        }
    }

    return grid;
}

boost::signals2::connection LLGridManager::addGridListChangedCallback(grid_list_changed_callback_t cb)
{
    return mGridListChangedSignal.connect(cb);
}
#endif
