/*
 * Copyright (c) 2025 Huawei Device Co., Ltd.
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifdef ENABLE_TEXT_ENHANCE
#include "src/ports/SkFontScanner_FreeType_priv.h"

#include <freetype/freetype.h>
#include <freetype/ftmm.h>
#include <freetype/ftsnames.h>
#include <freetype/ftsystem.h>
#include <freetype/ttnameid.h>
#include <freetype/tttables.h>
#include <freetype/t1tables.h>
#include <securec.h>
#include "src/base/SkTSearch.h"
#include "src/ports/SkFontHost_FreeType_common.h"

using namespace skia_private;

namespace {
using SkUniqueFTFace = std::unique_ptr<FT_FaceRec, SkFunctionObject<FT_Done_Face>>;
}

static const struct {
    const char* const name;
    int const weight;
} gCommonWeights[] = {
    // There are probably more common names, but these are known to exist.
    {"all", SkFontStyle::kNormal_Weight},  // Multiple Masters usually default to normal.
    {"black", SkFontStyle::kBlack_Weight},
    {"bold", SkFontStyle::kBold_Weight},
    {"book", (SkFontStyle::kNormal_Weight + SkFontStyle::kLight_Weight) / 2},
    {"demi", SkFontStyle::kSemiBold_Weight},
    {"demibold", SkFontStyle::kSemiBold_Weight},
    {"extra", SkFontStyle::kExtraBold_Weight},
    {"extrabold", SkFontStyle::kExtraBold_Weight},
    {"extralight", SkFontStyle::kExtraLight_Weight},
    {"hairline", SkFontStyle::kThin_Weight},
    {"heavy", SkFontStyle::kBlack_Weight},
    {"light", SkFontStyle::kLight_Weight},
    {"medium", SkFontStyle::kMedium_Weight},
    {"normal", SkFontStyle::kNormal_Weight},
    {"plain", SkFontStyle::kNormal_Weight},
    {"regular", SkFontStyle::kNormal_Weight},
    {"roman", SkFontStyle::kNormal_Weight},
    {"semibold", SkFontStyle::kSemiBold_Weight},
    {"standard", SkFontStyle::kNormal_Weight},
    {"thin", SkFontStyle::kThin_Weight},
    {"ultra", SkFontStyle::kExtraBold_Weight},
    {"ultrablack", SkFontStyle::kExtraBlack_Weight},
    {"ultrabold", SkFontStyle::kExtraBold_Weight},
    {"ultraheavy", SkFontStyle::kExtraBlack_Weight},
    {"ultralight", SkFontStyle::kExtraLight_Weight},
};

static SkScalar SkFT_FixedToScalar(FT_Fixed x)
{
    return SkFixedToScalar(x);
}

static bool GetAxes(FT_Face face, SkFontScanner::AxisDefinitions* axes)
{
    SkASSERT(face && axes);
    if (face->face_flags & FT_FACE_FLAG_MULTIPLE_MASTERS) {
        FT_MM_Var* variations = nullptr;
        FT_Error err = FT_Get_MM_Var(face, &variations);
        if (err) {
            return false;
        }
        UniqueVoidPtr autoFreeVariations(variations);

        axes->reset(variations->num_axis);
        for (FT_UInt i = 0; i < variations->num_axis; ++i) {
            const FT_Var_Axis& ftAxis = variations->axis[i];
            (*axes)[i].tag = ftAxis.tag;
            (*axes)[i].min = SkFT_FixedToScalar(ftAxis.minimum);
            (*axes)[i].def = SkFT_FixedToScalar(ftAxis.def);
            (*axes)[i].max = SkFT_FixedToScalar(ftAxis.maximum);
        }
    }
    return true;
}

bool SkFontScanner_FreeType::scanFont(
    SkStreamAsset* stream, int ttcIndex,
    SkString* name, SkFontStyle* style, bool* isFixedPitch, AxisDefinitions* axes) const
{
    SkAutoMutexExclusive libraryLock(fLibraryMutex);

    FT_StreamRec streamRec;
    SkUniqueFTFace face(this->openFace(stream, ttcIndex, &streamRec));
    if (!face) {
        return false;
    }

    int weight = SkFontStyle::kNormal_Weight;
    int width = SkFontStyle::kNormal_Width;
    SkFontStyle::Slant slant = SkFontStyle::kUpright_Slant;
    if (face->style_flags & FT_STYLE_FLAG_BOLD) {
        weight = SkFontStyle::kBold_Weight;
    }
    if (face->style_flags & FT_STYLE_FLAG_ITALIC) {
        slant = SkFontStyle::kItalic_Slant;
    }

    PS_FontInfoRec psFontInfo;
    TT_OS2* os2 = static_cast<TT_OS2*>(FT_Get_Sfnt_Table(face.get(), ft_sfnt_os2));
    if (os2 && os2->version != 0xffff) {
        weight = os2->usWeightClass;
        width = os2->usWidthClass;

        // OS/2::fsSelection bit 9 indicates oblique.
        if (SkToBool(os2->fsSelection & (1u << 9))) {
            slant = SkFontStyle::kOblique_Slant;
        }
    } else if (0 == FT_Get_PS_Font_Info(face.get(), &psFontInfo) && psFontInfo.weight) {
        int const index = SkStrLCSearch(&gCommonWeights[0].name, std::size(gCommonWeights),
                                        psFontInfo.weight, sizeof(gCommonWeights[0]));
        if (index >= 0) {
            weight = gCommonWeights[index].weight;
        }
    }

    if (name != nullptr) {
        name->set(face->family_name);
    }
    if (style != nullptr) {
        *style = SkFontStyle(weight, width, slant);
    }
    if (isFixedPitch != nullptr) {
        *isFixedPitch = FT_IS_FIXED_WIDTH(face);
    }

    if (axes != nullptr && !GetAxes(face.get(), axes)) {
        return false;
    }
    return true;
}

bool SkFontScanner_FreeType::scanFont(SkStreamAsset* stream, FontInfo& info, std::array<uint32_t, 4>& range) const
{
    SkAutoMutexExclusive libraryLock(fLibraryMutex);

    FT_StreamRec streamRec;
    SkUniqueFTFace face(this->openFace(stream, info.index, &streamRec));
    if (!face) {
        return false;
    }

    int weight = face->style_flags & FT_STYLE_FLAG_BOLD ? SkFontStyle::kBold_Weight : SkFontStyle::kNormal_Weight;
    int width = SkFontStyle::kNormal_Width;
    SkFontStyle::Slant slant = face->style_flags & FT_STYLE_FLAG_ITALIC
                                    ? SkFontStyle::kItalic_Slant
                                    : SkFontStyle::kUpright_Slant;
    PS_FontInfoRec psFontInfo;
    TT_OS2* os2 = static_cast<TT_OS2*>(FT_Get_Sfnt_Table(face.get(), ft_sfnt_os2));
    if (os2 && os2->version != 0xffff) {
        weight = os2->usWeightClass;
        width = os2->usWidthClass;
        // OS/2::ulUnicodeRange is bigendian, so we need to swap it
        range[0] = os2->ulUnicodeRange1;
        range[1] = os2->ulUnicodeRange2;
        range[2] = os2->ulUnicodeRange3;  // the 3rd range at index 2
        range[3] = os2->ulUnicodeRange4;  // the 4th range at index 3

        // OS/2::fsSelection bit 9 indicates oblique.
        if (SkToBool(os2->fsSelection & (1u << 9))) {
            slant = SkFontStyle::kOblique_Slant;
        }
    } else if (FT_Get_PS_Font_Info(face.get(), &psFontInfo) == 0 && psFontInfo.weight) {
        int const index = SkStrLCSearch(&gCommonWeights[0].name, std::size(gCommonWeights),
                                        psFontInfo.weight, sizeof(gCommonWeights[0]));
        if (index >= 0) {
            weight = gCommonWeights[index].weight;
        }
    }
    info.familyName.set(face->family_name);
    info.style = SkFontStyle(weight, width, slant);
    info.isFixedWidth = FT_IS_FIXED_WIDTH(face);
    return true;
}

/**
 *  Gets fullname from stream, true means success
 */
bool SkFontScanner_FreeType::GetTypefaceFullname(SkStreamAsset* stream,
    int ttcIndex, SkByteArray& fullname) const
{
    if (stream == nullptr) {
        return false;
    }
    SkAutoMutexExclusive libraryLock(fLibraryMutex);
    FT_StreamRec streamRec;
    SkUniqueFTFace face(this->openFace(stream, ttcIndex, &streamRec));
    if (!face) {
        return false;
    }

    constexpr FT_UShort EN_LANGUAGE_ID = 1033;
    FT_SfntName sfntName;
    FT_UInt nameCount = FT_Get_Sfnt_Name_Count(face.get());
    for (FT_UInt i = 0; i < nameCount; ++i) {
        if (FT_Get_Sfnt_Name(face.get(), i, &sfntName) != 0) {
            continue;
        }
        if (sfntName.name_id != TT_NAME_ID_FULL_NAME) {
            continue;
        }

        if (fullname.strData != nullptr && sfntName.language_id != EN_LANGUAGE_ID) {
            continue;
        }
        fullname.strData = std::make_unique<uint8_t[]>(sfntName.string_len);
        if (memcpy_s(fullname.strData.get(), sfntName.string_len, sfntName.string, sfntName.string_len) == EOK) {
            fullname.strLen = sfntName.string_len;
            if (sfntName.language_id == EN_LANGUAGE_ID) {
                return true;
            }
        }
    }
    return fullname.strData != nullptr;
}
 #endif