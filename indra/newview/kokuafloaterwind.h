/**
 * @file kokuafloaterwind.h
 * @author Kentron Katana
 * @brief A floater to show the wind at the avatar's position
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

#ifndef KOKUA_KOKUAFLOATERWIND_H
#define KOKUA_KOKUAFLOATERWIND_H

#include "llfloater.h"

class LLTextBox;

class KokuaFloaterWind : public LLFloater
{
    friend class LLFloaterReg;
public:
    bool postBuild() override;
    void draw() override;

private:
    KokuaFloaterWind(const LLSD& key);
    virtual ~KokuaFloaterWind() {}

    // Wind at the avatar's position in m/s, false when there is no region to ask
    bool getWind(LLVector3& wind) const;
    void setText(LLTextBox* textbox, const std::string& text);
    // When pinned, stay in front of the other floaters without taking the focus
    void keepOnTop();

    LLView*     mArrowArea;
    LLTextBox*  mSpeedText;
    LLTextBox*  mDirectionText;
    std::vector<std::string> mCompassPoints;
};

#endif
