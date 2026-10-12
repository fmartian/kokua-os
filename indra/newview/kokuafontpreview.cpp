/**
 * @file kokuafontpreview.cpp
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
#include "llviewerprecompiledheaders.h"
#include "kokuafontpreview.h"

#include "lldir.h"
#include "llfontgl.h"
#include "llfontregistry.h"
#include "lllocalcliprect.h"
#include "llrender2dutils.h"
#include "lltrans.h"
#include "lluicolortable.h"
#include "lluictrlfactory.h"
#include "llviewercontrol.h"

static LLDefaultChildRegistry::Register<KokuaFontPreview> r("kokua_font_preview");

std::set<KokuaFontPreview*> KokuaFontPreview::sInstances;

const S32 PREVIEW_PAD = 6;
const S32 PREVIEW_LABEL_WIDTH = 78;
const S32 PREVIEW_ROW_SPACING = 2;

KokuaFontPreview::Params::Params()
{
}

KokuaFontPreview::KokuaFontPreview(const Params& p)
:   LLView(p),
    mRegistry(NULL),
    mLoadedSizeAdjustment(0.f),
    mLoadFailed(false)
{
    sInstances.insert(this);
}

KokuaFontPreview::~KokuaFontPreview()
{
    sInstances.erase(this);
    releaseFonts();
}

// static
void KokuaFontPreview::releaseAll()
{
    for (KokuaFontPreview* preview : sInstances)
    {
        preview->releaseFonts();
    }
}

void KokuaFontPreview::releaseFonts()
{
    // The registry owns the fonts, and the fonts their textures
    mRows.clear();
    delete mRegistry;
    mRegistry = NULL;
    mLoadedFile.clear();
    mLoadFailed = false;
}

void KokuaFontPreview::onVisibilityChange(bool new_visibility)
{
    LLView::onVisibilityChange(new_visibility);
    if (!new_visibility)
    {
        // Preferences was closed or moved to another tab, nothing to keep loaded
        releaseFonts();
    }
}

bool KokuaFontPreview::updateFonts()
{
    static LLCachedControl<std::string> font_file(gSavedSettings, "FSFontSettingsFile");
    static LLCachedControl<F32> size_adjustment(gSavedSettings, "FSFontSizeAdjustment");

    const std::string file = font_file;
    const F32 adjustment = size_adjustment;
    if (file == mLoadedFile && adjustment == mLoadedSizeAdjustment && (mRegistry || mLoadFailed))
    {
        return !mLoadFailed;
    }

    releaseFonts();
    mLoadedFile = file;
    mLoadedSizeAdjustment = adjustment;

    // Same places and order as LLFontGL::initClass()
    mRegistry = new LLFontRegistry(true, adjustment);
    if (!mRegistry->parseFontInfo(gDirUtilp->getExpandedFilename(LL_PATH_FONTS, "", file)) &&
        !mRegistry->parseFontInfo(gDirUtilp->getExpandedFilename(LL_PATH_USER_SETTINGS, "fonts", file)))
    {
        delete mRegistry;
        mRegistry = NULL;
        mLoadFailed = true;
        return false;
    }

    struct RowDef
    {
        const char* mLabel;
        const char* mSample;
        const char* mName;
        const char* mSize;
        U8          mStyle;
    };
    static const RowDef ROWS[] =
    {
        { "FontPreviewRegular",   "FontPreviewPangram", "SansSerif", "Medium",    LLFontGL::NORMAL },
        { "",                     "FontPreviewAccents", "SansSerif", "Medium",    LLFontGL::NORMAL },
        { "FontPreviewBold",      "FontPreviewPangram", "SansSerif", "Medium",    LLFontGL::BOLD },
        { "FontPreviewItalic",    "FontPreviewAccents", "SansSerif", "Medium",    LLFontGL::ITALIC },
        { "FontPreviewSmall",     "FontPreviewPangram", "SansSerif", "Small",     LLFontGL::NORMAL },
        { "FontPreviewLarge",     "FontPreviewShort",   "SansSerif", "Large",     LLFontGL::NORMAL },
        { "FontPreviewMonospace", "FontPreviewCode",    "Monospace", "Monospace", LLFontGL::NORMAL },
    };

    // Only the fonts asked for here get loaded, the registry creates them on demand
    for (const RowDef& def : ROWS)
    {
        LLFontGL* font = mRegistry->getFont(LLFontDescriptor(def.mName, def.mSize, def.mStyle));
        if (font)
        {
            Row row;
            row.mLabel = def.mLabel[0] ? LLTrans::getString(def.mLabel) : LLStringUtil::null;
            row.mSample = LLTrans::getString(def.mSample);
            row.mFont = font;
            row.mStyle = def.mStyle;
            mRows.push_back(row);
        }
    }

    return true;
}

void KokuaFontPreview::draw()
{
    static LLUIColor bg_color = LLUIColorTable::instance().getColor("Black_50", LLColor4(0.f, 0.f, 0.f, 0.5f));
    static LLUIColor label_color = LLUIColorTable::instance().getColor("LabelDisabledColor", LLColor4::grey);
    static LLUIColor text_color = LLUIColorTable::instance().getColor("LabelTextColor", LLColor4::white);

    LLRect local_rect = getLocalRect();
    gl_rect_2d(local_rect, bg_color.get(), true);

    // Drawn with the fonts in use, the ones the rest of Preferences is drawn with
    const LLFontGL* label_font = LLFontGL::getFontSansSerifSmall();

    if (!updateFonts())
    {
        label_font->renderUTF8(LLTrans::getString("FontPreviewUnavailable"), 0,
                               (F32)PREVIEW_PAD, (F32)(local_rect.mTop - PREVIEW_PAD),
                               label_color.get(), LLFontGL::LEFT, LLFontGL::TOP, LLFontGL::NORMAL, LLFontGL::NO_SHADOW);
    }
    else
    {
        LLLocalClipRect clip(local_rect);

        S32 sample_left = PREVIEW_PAD + PREVIEW_LABEL_WIDTH;
        S32 sample_width = local_rect.getWidth() - sample_left - PREVIEW_PAD;
        S32 y = local_rect.mTop - PREVIEW_PAD;

        for (const Row& row : mRows)
        {
            y -= llmax(row.mFont->getLineHeight(), label_font->getLineHeight());
            F32 baseline = (F32)y + row.mFont->getDescenderHeight();

            if (!row.mLabel.empty())
            {
                label_font->renderUTF8(row.mLabel, 0, (F32)PREVIEW_PAD, baseline,
                                       label_color.get(), LLFontGL::LEFT, LLFontGL::BASELINE, LLFontGL::NORMAL, LLFontGL::NO_SHADOW);
            }
            row.mFont->renderUTF8(row.mSample, 0, (F32)sample_left, baseline,
                                  text_color.get(), LLFontGL::LEFT, LLFontGL::BASELINE, row.mStyle, LLFontGL::NO_SHADOW,
                                  S32_MAX, sample_width, NULL, true);

            y -= PREVIEW_ROW_SPACING;
        }
    }

    LLView::draw();
}
