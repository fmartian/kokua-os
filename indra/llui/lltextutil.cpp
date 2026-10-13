/**
 * @file lltextutil.cpp
 * @brief Misc text-related auxiliary methods
 *
 * $LicenseInfo:firstyear=2009&license=viewerlgpl$
 * Second Life Viewer Source Code
 * Copyright (C) 2010, Linden Research, Inc.
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

#include "lltextutil.h"

#include "lluicolor.h"
#include "lltextbox.h"
#include "llui.h"
#include "lluiimage.h"
#include "llurlmatch.h"

std::function<bool(LLUrlMatch*, LLTextBase*)> LLTextUtil::TextHelpers::iconCallbackCreationFunction = nullptr;

void LLTextUtil::textboxSetHighlightedVal(LLTextBox *txtbox, const LLStyle::Params& normal_style, const std::string& text, const std::string& hl)
{
    static LLUIColor sFilterTextColor = LLUIColorTable::instance().getColor("FilterTextColor", LLColor4::green);

    std::string text_uc = text;
    LLStringUtil::toUpper(text_uc);

    size_t hl_begin = 0, hl_len = hl.size();

    if (hl_len == 0 || (hl_begin = text_uc.find(hl)) == std::string::npos)
    {
        txtbox->setText(text, normal_style);
        return;
    }

    LLStyle::Params hl_style = normal_style;
    hl_style.color = sFilterTextColor;

    txtbox->setText(LLStringUtil::null); // clear text
    txtbox->appendText(text.substr(0, hl_begin),        false, normal_style);
    txtbox->appendText(text.substr(hl_begin, hl_len),   false, hl_style);
    txtbox->appendText(text.substr(hl_begin + hl_len),  false, normal_style);
}

void LLTextUtil::drawHighlightBox(LLTextBox *txtbox, const std::string& hl, S32 h_pad, S32 v_pad)
{
    if (!txtbox || hl.empty() || !txtbox->getVisible())
    {
        return;
    }

    static LLUIColor sFilterBGColor = LLUIColorTable::instance().getColor("FilterBackgroundColor", LLColor4::black);
    static LLPointer<LLUIImage> sBoxImage = LLUI::getUIImage("Rounded_Square");
    if (sBoxImage.isNull())
    {
        return;
    }

    LLWString text = txtbox->getWText();
    LLWStringUtil::toUpper(text);
    const LLWString match = utf8str_to_wstring(hl);
    size_t begin = text.find(match);
    if (begin == LLWString::npos)
    {
        return;
    }
    size_t end = begin + match.size();

    // The bounding rect is where the text is drawn, alignment included. The characters
    // are placed inside it by how far the text box lays them out from the first one.
    LLRect text_rect = txtbox->getTextBoundingRect();
    S32 text_start = txtbox->getDocRectFromDocIndex(0).mLeft;
    S32 left = text_rect.mLeft + txtbox->getDocRectFromDocIndex((S32)begin).mLeft - text_start;
    S32 right = (end < text.size()) ? text_rect.mLeft + txtbox->getDocRectFromDocIndex((S32)end).mLeft - text_start
                                    : text_rect.mRight;

    LLRect box_rect(left - h_pad, text_rect.mTop + v_pad, right + h_pad, text_rect.mBottom - v_pad);
    // Text that doesn't fit is cut short
    box_rect.mRight = llmin(box_rect.mRight, txtbox->getLocalRect().mRight);
    if (box_rect.mLeft >= box_rect.mRight)
    {
        return;
    }

    // To the coordinates of the view doing the drawing
    box_rect.translate(txtbox->getRect().mLeft, txtbox->getRect().mBottom);
    sBoxImage->draw(box_rect, sFilterBGColor.get());
}

void LLTextUtil::textboxSetGreyedVal(LLTextBox *txtbox, const LLStyle::Params& normal_style, const std::string& text, const std::string& greyed)
{
    static LLUIColor sGreyedTextColor = LLUIColorTable::instance().getColor("Gray", LLColor4::grey);

    size_t greyed_begin = 0, greyed_len = greyed.size();

    if (greyed_len == 0 || (greyed_begin = text.find(greyed)) == std::string::npos)
    {
        txtbox->setText(text, normal_style);
        return;
    }

    LLStyle::Params greyed_style = normal_style;
    greyed_style.color = sGreyedTextColor;
    txtbox->setText(LLStringUtil::null); // clear text
    txtbox->appendText(text.substr(0, greyed_begin),        false, normal_style);
    txtbox->appendText(text.substr(greyed_begin, greyed_len),   false, greyed_style);
    txtbox->appendText(text.substr(greyed_begin + greyed_len),  false, normal_style);
}

bool LLTextUtil::processUrlMatch(LLUrlMatch* match,LLTextBase* text_base, bool is_content_trusted)
{
    if (match == 0 || text_base == 0)
        return false;

    if(match->getID() != LLUUID::null && TextHelpers::iconCallbackCreationFunction)
    {
        bool segment_created = TextHelpers::iconCallbackCreationFunction(match,text_base);
        if(segment_created)
            return true;
    }

    // output an optional icon before the Url
    if (is_content_trusted && !match->getIcon().empty() )
    {
        LLUIImagePtr image = LLUI::getUIImage(match->getIcon());
        if (image)
        {
            LLStyle::Params icon;
            icon.image = image;
            // Text will be replaced during rendering with the icon,
            // but string cannot be empty or the segment won't be
            // added (or drawn).
            text_base->appendImageSegment(icon);

            return true;
        }
    }

    return false;
}

// EOF
