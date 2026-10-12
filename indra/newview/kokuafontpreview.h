/**
 * @file kokuafontpreview.h
 * @author Kentron Katana
 * @brief Shows sample text in the font scheme picked in Preferences
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

#ifndef KOKUA_KOKUAFONTPREVIEW_H
#define KOKUA_KOKUAFONTPREVIEW_H

#include "llview.h"

class LLFontGL;
class LLFontRegistry;

// The font scheme only takes effect after a restart, so the sample is drawn with fonts
// from a registry of its own. The fonts of the interface are never touched.
class KokuaFontPreview : public LLView
{
public:
    struct Params : public LLInitParam::Block<Params, LLView::Params>
    {
        Params();
    };

    virtual ~KokuaFontPreview();

    void draw() override;
    void onVisibilityChange(bool new_visibility) override;

    // The viewer calls this when it rebuilds its fonts or loses the GL context:
    // the sample fonts hold textures and were sized for the old interface scale
    static void releaseAll();

protected:
    KokuaFontPreview(const Params& p);
    friend class LLUICtrlFactory;

private:
    struct Row
    {
        std::string mLabel;
        std::string mSample;
        LLFontGL*   mFont;
        U8          mStyle;
    };

    // Loads the selected scheme if it isn't the one loaded, false when it can't be read
    bool updateFonts();
    void releaseFonts();

    LLFontRegistry*     mRegistry;
    std::vector<Row>    mRows;
    std::string         mLoadedFile;
    F32                 mLoadedSizeAdjustment;
    bool                mLoadFailed;

    static std::set<KokuaFontPreview*> sInstances;
};

#endif
