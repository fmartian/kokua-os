/**
 * @file kokuafloaterwind.cpp
 * @author Kentron Katana
 * @brief A floater to show the wind at the avatar's position
 *
 * $LicenseInfo:firstyear=2008&license=viewerlgpl$
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
#include "llviewerprecompiledheaders.h"
#include "kokuafloaterwind.h"

#include "llagent.h"
#include "llbutton.h"
#include "llcallbacklist.h"
#include "llmodaldialog.h"
#include "llrender2dutils.h"
#include "lltextbox.h"
#include "lluicolortable.h"
#include "llviewercontrol.h"
#include "llviewerobject.h"
#include "llviewerregion.h"
#include "llvoavatarself.h"

const F32 MS_TO_KNOTS = 1.943844f;
// Below this the direction is just noise
const F32 CALM_SPEED = 0.05f;

KokuaFloaterWind::KokuaFloaterWind(const LLSD& key)
:   LLFloater(key),
    mArrowArea(NULL),
    mSpeedText(NULL),
    mDirectionText(NULL)
{
}

bool KokuaFloaterWind::postBuild()
{
    mArrowArea = getChild<LLView>("arrow_area");
    mSpeedText = getChild<LLTextBox>("speed_text");
    mDirectionText = getChild<LLTextBox>("direction_text");

    // The pin goes in the title bar, left of the close and minimize buttons
    static LLUICachedControl<S32> close_box_size("UIFloaterCloseBoxSize", 0);
    static LLUICachedControl<S32> close_box_from_top("UICloseBoxFromTop", 0);
    const S32 TITLE_BUTTONS = 2;
    LLRect pin_rect;
    pin_rect.setLeftTopAndSize(getRect().getWidth() - LLPANEL_BORDER_WIDTH - (close_box_size + 1) * (TITLE_BUTTONS + 1),
                               getRect().getHeight() - close_box_from_top,
                               close_box_size, close_box_size);
    getChild<LLButton>("pin_btn")->setRect(pin_rect);

    // N, NNE, NE... clockwise from north
    LLStringUtil::getTokens(getString("compass_points"), mCompassPoints, ", \n\t");

    return true;
}

bool KokuaFloaterWind::getWind(LLVector3& wind) const
{
    LLViewerRegion* regionp = gAgent.getRegion();
    if (!regionp)
    {
        return false;
    }

    wind = regionp->mWind.getVelocity(regionp->getPosRegionFromGlobal(gAgent.getPositionGlobal()));

    static LLCachedControl<bool> apparent(gSavedSettings, "KokuaWindShowApparent");
    if (apparent && isAgentAvatarValid())
    {
        // A seated avatar doesn't move by itself, what moves is what it sits on
        LLViewerObject* root = gAgentAvatarp->isSitting() ? (LLViewerObject*)gAgentAvatarp->getRoot() : NULL;
        wind -= root ? root->getVelocity() : gAgentAvatarp->getVelocity();
    }

    // Only the horizontal wind is shown
    wind.mV[VZ] = 0.f;
    return true;
}

void KokuaFloaterWind::setText(LLTextBox* textbox, const std::string& text)
{
    if (textbox->getText() != text)
    {
        textbox->setText(text);
    }
}

void KokuaFloaterWind::keepOnTop()
{
    static LLCachedControl<bool> pinned(gSavedSettings, "KokuaWindPinned");
    if (!pinned || !gFloaterView || getParent() != gFloaterView)
    {
        return;
    }

    for (LLView* viewp : *gFloaterView->getChildList())
    {
        LLFloater* floaterp = dynamic_cast<LLFloater*>(viewp);
        if (!floaterp || !floaterp->getVisible())
        {
            continue;
        }

        // The first visible floater is the one in front. Modal dialogs are left alone.
        if (floaterp != this && !dynamic_cast<LLModalDialog*>(floaterp))
        {
            // Not from here, the floater view is going through its children to draw them
            LLHandle<LLFloater> handle = getHandle();
            doOnIdleOneTime([handle]()
            {
                LLFloater* self = handle.get();
                if (self && gFloaterView && self->getParent() == gFloaterView)
                {
                    gFloaterView->sendChildToFront(self);
                }
            });
        }
        return;
    }
}

void KokuaFloaterWind::draw()
{
    keepOnTop();

    LLVector3 wind;
    bool have_wind = getWind(wind);
    F32 speed = have_wind ? wind.magVec() : 0.f;
    bool calm = speed < CALM_SPEED;

    // Compass bearing the wind blows towards: 0 is north (+Y), 90 is east (+X)
    F32 bearing_to = calm ? 0.f : atan2f(wind.mV[VX], wind.mV[VY]);

    if (!have_wind)
    {
        setText(mSpeedText, getString("no_wind_data"));
        setText(mDirectionText, LLStringUtil::null);
    }
    else
    {
        LLStringUtil::format_map_t args;
        args["[MS]"] = llformat("%.1f", speed);
        args["[KNOTS]"] = llformat("%.1f", speed * MS_TO_KNOTS);
        setText(mSpeedText, getString("speed", args));

        if (calm || mCompassPoints.empty())
        {
            setText(mDirectionText, getString("calm"));
        }
        else
        {
            // Shown the way sailors and forecasts do: where the wind comes from
            S32 degrees_from = ll_round(bearing_to * RAD_TO_DEG + 180.f) % 360;
            S32 points = (S32)mCompassPoints.size();
            S32 point = ll_round((F32)degrees_from * (F32)points / 360.f) % points;
            args["[DEGREES]"] = llformat("%d", degrees_from);
            args["[POINT]"] = mCompassPoints[point];
            setText(mDirectionText, getString("direction_from", args));
        }
    }

    LLFloater::draw();

    if (isMinimized())
    {
        return;
    }

    // North is up; the arrow flies with the wind
    static LLUIColor dial_color = LLUIColorTable::instance().getColor("LabelDisabledColor", LLColor4::grey);
    static LLUIColor arrow_color = LLUIColorTable::instance().getColor("LabelTextColor", LLColor4::white);

    LLRect area = mArrowArea->getRect();
    F32 center_x = (F32)area.getCenterX();
    F32 center_y = (F32)area.getCenterY();
    F32 radius = 0.5f * (F32)llmin(area.getWidth(), area.getHeight()) - 2.f;

    gGL.getTexUnit(0)->unbind(LLTexUnit::TT_TEXTURE);
    gGL.color4fv(dial_color.get().mV);
    gl_circle_2d(center_x, center_y, radius, 48, false);

    if (!calm)
    {
        const F32 SHAFT_HALF_WIDTH = 2.f;
        const F32 HEAD_HALF_WIDTH = 8.f;
        const F32 HEAD_LENGTH = 16.f;
        F32 half_length = radius * 0.8f;

        // Unit vector along the arrow and its perpendicular, in screen space
        F32 dir_x = sinf(bearing_to);
        F32 dir_y = cosf(bearing_to);
        F32 side_x = dir_y;
        F32 side_y = -dir_x;

        F32 tip_x = center_x + dir_x * half_length;
        F32 tip_y = center_y + dir_y * half_length;
        F32 neck_x = tip_x - dir_x * HEAD_LENGTH;
        F32 neck_y = tip_y - dir_y * HEAD_LENGTH;
        F32 tail_x = center_x - dir_x * half_length;
        F32 tail_y = center_y - dir_y * half_length;

        const LLColor4& color = arrow_color.get();
        // Head
        gl_triangle_2d(ll_round(tip_x), ll_round(tip_y),
                       ll_round(neck_x - side_x * HEAD_HALF_WIDTH), ll_round(neck_y - side_y * HEAD_HALF_WIDTH),
                       ll_round(neck_x + side_x * HEAD_HALF_WIDTH), ll_round(neck_y + side_y * HEAD_HALF_WIDTH),
                       color, true);
        // Shaft
        gl_triangle_2d(ll_round(neck_x + side_x * SHAFT_HALF_WIDTH), ll_round(neck_y + side_y * SHAFT_HALF_WIDTH),
                       ll_round(neck_x - side_x * SHAFT_HALF_WIDTH), ll_round(neck_y - side_y * SHAFT_HALF_WIDTH),
                       ll_round(tail_x - side_x * SHAFT_HALF_WIDTH), ll_round(tail_y - side_y * SHAFT_HALF_WIDTH),
                       color, true);
        gl_triangle_2d(ll_round(neck_x + side_x * SHAFT_HALF_WIDTH), ll_round(neck_y + side_y * SHAFT_HALF_WIDTH),
                       ll_round(tail_x - side_x * SHAFT_HALF_WIDTH), ll_round(tail_y - side_y * SHAFT_HALF_WIDTH),
                       ll_round(tail_x + side_x * SHAFT_HALF_WIDTH), ll_round(tail_y + side_y * SHAFT_HALF_WIDTH),
                       color, true);
    }
}
