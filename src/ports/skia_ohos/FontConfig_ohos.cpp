// Copyright (c) 2023 Huawei Device Co., Ltd. All rights reserved
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "FontConfig_ohos.h"

#include <algorithm>
#include <array>
#include <cstring>
#include <dirent.h>
#include <fstream>
#include <functional>
#include <libgen.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#if defined(SK_BUILD_FONT_MGR_FOR_PREVIEW_WIN) or defined(SK_BUILD_FONT_MGR_FOR_PREVIEW_MAC) or \
    defined(SK_BUILD_FONT_MGR_FOR_PREVIEW_LINUX)
#define SK_BUILD_FONT_MGR_FOR_PREVIEW
#endif

#ifdef SK_BUILD_FONT_MGR_FOR_OHOS
#include <parameters.h>
#endif

#include "SkFontStyle.h"
#include "SkString.h"
#include "securec.h"

using namespace ErrorCode;
static const char* PRODUCT_DEFAULT_CONFIG = "/system/etc/productfontconfig.json";
static const char* DEFAULT_CONFIG = "/system/etc/fontconfig_ohos.json";

#ifdef SK_BUILD_FONT_MGR_FOR_OHOS
static const bool G_IS_HMSYMBOL_ENABLE =
    (std::atoi(OHOS::system::GetParameter("persist.sys.graphic.hmsymbolcfg.enable", "1").c_str()) != 0);
#else
static const bool G_IS_HMSYMBOL_ENABLE = true;
#endif

#ifdef SK_BUILD_FONT_MGR_FOR_PREVIEW
static const char* OHOS_DEFAULT_CONFIG = "fontconfig_ohos.json";
/*! Constructor
 * \param fontScanner the scanner to get the font information from a font file
 * \param fname the full name of system font configuration document.
 *     \n The default value is '/system/etc/fontconfig.json', if fname is given null
 */
FontConfig_OHOS::FontConfig_OHOS(const SkTypeface_FreeType::Scanner& fontScanner, const char* fname)
    : fFontScanner(fontScanner)
{
    int err = parseConfig(fname);
    if (err != NO_ERROR) {
        return;
    }
    loadHMSymbol();
}
#else
/*! Constructor
 * \param fontScanner the scanner to get the font information from a font file
 * \param fname the full name of system font configuration document.
 *     \n The default value is '/system/etc/fontconfig.json', if fname is given null
 */
FontConfig_OHOS::FontConfig_OHOS(const SkTypeface_FreeType::Scanner& fontScanner, const char* fname)
    : fFontScanner(fontScanner)
{
    int err = checkProductFile(fname);
    if (err != NO_ERROR) {
        return;
    }
    loadHMSymbol();
}
#endif

/*! To get the count of font style sets supported in the system
 *  \return The count of font style sets in generic family
 */
int FontConfig_OHOS::getFamilyCount() const
{
    return fFontCollection.fGeneric.size();
}

/*! To match the fallback typeface by the given style and character
 *  this function will traverse all the fallback typefaces
 *  \param character the character to be matched
 *  \param style the style to be matched
 *  \return the matched typeface
*/
SkTypeface* FontConfig_OHOS::matchFallback(SkUnichar character, const SkFontStyle& style) const
{
    for (auto& f : fFontCollection.fFallback) {
        const auto& typefaces = f.typefaces;
        if (!typefaces.empty() && typefaces[0]->unicharToGlyph(character)) {
            auto typeface = matchFontStyle(typefaces, style);
            return SkSafeRef(typeface.get());
        }
    }
    return nullptr;
}


/*! To match the fallback typeface by the given index style and character
 *  this function only traverse the fallback typefaces in the given index
 *  \param index the index of fallback typefaces
 *  \param character the character to be matched
 *  \param style the style to be matched
 *  \return the matched typeface
*/
SkTypeface* FontConfig_OHOS::matchFallback(size_t index, SkUnichar character, const SkFontStyle& style) const
{
    if (index >= fFontCollection.fFallback.size()) {
        return nullptr;
    }
    const auto& typefaces = fFontCollection.fFallback[index].typefaces;
    if (!typefaces.empty() && typefaces[0]->unicharToGlyph(character)) {
        auto typeface = matchFontStyle(typefaces, style);
        return SkSafeRef(typeface.get());
    }
    return nullptr;
}

/*! To match the fallback typeface by the given function
 *  this function will traverse all the fallback typefaces
 *  \param func the judge func, if the func return -1, it means the language tag is not matched
 *  \return the matched fallback typefaces' index set
*/
std::vector<size_t> FontConfig_OHOS::matchFallbackByBCP47(std::function<int(const std::string&)> func) const
{
    std::vector<size_t> res;
    for (size_t i = 0; i < fFontCollection.fFallback.size(); i += 1) {
        if (func(fFontCollection.fFallback[i].lang) != -1) {
            res.push_back(i);
        }
    }
    return res;
}

/*! To get a typeface by the given family name and style
 *  this function will traverse both the fallback and general typefaces
 *  \param familyName the family name of the fallback typeface
 *  \param style the style of the fallback typeface
 *  \return the matched typeface
*/
sk_sp<SkTypeface_OHOS> FontConfig_OHOS::getFallbackTypeface(const SkString& familyName, const SkFontStyle& style) const
{
    std::pair<size_t, FontType> res;
    if (!fFontCollection.getIndexByFamilyName(familyName.c_str(), res)) {
        return nullptr;
    }

    const auto& tpSet = fFontCollection.getSet(true)[res.first].typefaces;
    if (tpSet.size() == 0) {
        return nullptr;
    }
    return FontConfig_OHOS::matchFontStyle(tpSet, style);
}

/*! To get the family name of the default font style set
 *  \param[out] familyName a pointer of SkString object, to which the family value will be set.
 *  \return The count of typeface in this font style set
 *  \n Return -1, if there is no any font style set in the system.
 */
int FontConfig_OHOS::getDefaultFamily(SkString& familyName) const
{
    return getFamilyName(0, familyName);
}

/*! To get the family name of a font style set
 * \param index the index of a font style set in generic family
 * \param[out] familyName a pointer of SkString object, to which the family value will be set
 * \return The count of typeface in the font style set
 * \n      Return -1, if the 'index' is out of range
 */
int FontConfig_OHOS::getFamilyName(size_t index, SkString& familyName) const
{
    if (index >= getFamilyCount()) {
        familyName = "";
        return -1;
    }
    familyName = fFontCollection.fGeneric[index].alias.c_str();
    return fFontCollection.fGeneric[index].typefaces.size();
}

/*! To get the count of a font style set
 * \param styleIndex the index of a font style set
 * \param isFallback to indicate the font style set is from generic family or fallback family
 * \n                 false , the font style set is from generic family list
 * \n                 true, the font style set is from fallback family list
 * \return The count of typeface in the font style set
 */
size_t FontConfig_OHOS::getTypefaceCount(size_t styleIndex, bool isFallback) const
{
    auto& set = fFontCollection.getSet(isFallback);
    return styleIndex < set.size() ? set[styleIndex].typefaces.size() : 0;
}

/*! To get a typeface
 * \param styleIndex the index of a font style set
 * \param index the index of a typeface in its style set
 * \param isFallback false, the font style set is generic
 * \n          true, the font style set is fallback
 * \return The pointer of a typeface
 * \n       Return null, if 'styleIndex' or 'index' is out of range
 */
SkTypeface_OHOS* FontConfig_OHOS::getTypeface(size_t styleIndex, size_t index, bool isFallback) const
{
    sk_sp<SkTypeface_OHOS> typeface = getTypefaceSP(styleIndex, index, isFallback);
    return (typeface == nullptr) ? nullptr : typeface.get();
}

sk_sp<SkTypeface_OHOS> FontConfig_OHOS::getTypefaceSP(size_t styleIndex, size_t index, bool isFallback) const
{
    auto& set = fFontCollection.getSet(isFallback);
    if (styleIndex <= set.size()) {
        // if index less than typefaces' size, return the ptr
        return index < set[styleIndex].typefaces.size() ? set[styleIndex].typefaces[index] : nullptr;
    }
    return nullptr;
}

/*! To get a typeface
 * \param styleIndex the index a font style set
 * \param style the font style to be matching
 * \param isFallback false, the font style set is generic
 * \n                true, the font style set is fallback
 * \return An object of typeface whose font style is the closest matching to 'style'
 * \n      Return null, if 'styleIndex' is out of range
 */
SkTypeface_OHOS* FontConfig_OHOS::getTypeface(size_t styleIndex, const SkFontStyle& style, bool isFallback) const
{
    auto& set = fFontCollection.getSet(isFallback);
    if (styleIndex >= set.size()) {
        return nullptr;
    }

    const std::vector<sk_sp<SkTypeface_OHOS>>& pSet = set[styleIndex].typefaces;
    sk_sp<SkTypeface_OHOS> tp = matchFontStyle(pSet, style);
    return tp.get();
}

/*! To get the index of a font style set
 *  \param familyName the family name of the font style set
 *  \n     get the index of default font style set, if 'familyName' is null
 *  \param[out] isFallback to tell if the family is from generic or fallback to the caller.
 *  \n          isFallback is false, if the font style is from generic family list
 *  \n          isFallback is true, if the font style is from fallback family list
 *  \return The index of the font style set
 *  \n      Return -1, if 'familyName' is not found in the system
 */
bool FontConfig_OHOS::getStyleIndex(const char* familyName, bool& isFallback, size_t& index) const
{
    if (familyName == nullptr) {
        isFallback = false;
        return true;
    }

    std::lock_guard<std::mutex> lock(fFontMutex);
    std::pair<size_t, FontType> res;
    if (fFontCollection.getIndexByFamilyName(familyName, res)) {
        isFallback = res.second == FontType::Fallback;
        index = res.first;
        return true;
    }
    return false;
}

/*! Find the closest matching typeface
 * \param typefaceSet a typeface set belonging to the same font style set
 * \param pattern the font style to be matching
 * \return The typeface object which is the closest matching to 'pattern'
 * \n      Return null, if the count of typeface is 0
 */
sk_sp<SkTypeface_OHOS> FontConfig_OHOS::matchFontStyle(
    const std::vector<sk_sp<SkTypeface_OHOS>>& typefaceSet, const SkFontStyle& pattern)
{
    int count = typefaceSet.size();
    if (count == 1) {
        return typefaceSet[0];
    }
    sk_sp<SkTypeface_OHOS> res = nullptr;
    uint32_t minDiff = 0xFFFFFFFF;
    for (int i = 0; i < count; i++) {
        const SkFontStyle& fontStyle = typefaceSet[i]->fontStyle();
        uint32_t diff = getFontStyleDifference(pattern, fontStyle);
        if (diff < minDiff) {
            minDiff = diff;
            res = typefaceSet[i];
        }
    }
    return res;
}

/*! To get the difference between a font style and the matching font style
 * \param dstStyle the style to be matching
 * \param srcStyle a font style
 * \return The difference value of a specified style with the matching style
 */
uint32_t FontConfig_OHOS::getFontStyleDifference(const SkFontStyle& dstStyle,
    const SkFontStyle& srcStyle)
{
    int normalWidth = SkFontStyle::kNormal_Width;
    int dstWidth = dstStyle.width();
    int srcWidth = srcStyle.width();

    uint32_t widthDiff = 0;
    // The maximum font width is kUltraExpanded_Width i.e. '9'.
    // If dstWidth <= kNormal_Width (5), first check narrower values, then wider values.
    // If dstWidth > kNormal_Width, first check wider values, then narrower values.
    // When dstWidth and srcWidth are at different side of kNormal_Width,
    // the width difference between them should be more than 5 (9/2+1)
    constexpr int kWidthDiffThreshold = 9 / 2 + 1;
    if (dstWidth <= normalWidth) {
        widthDiff = (srcWidth <= dstWidth) ? (dstWidth - srcWidth)
                                           : (srcWidth - dstWidth + kWidthDiffThreshold);
    } else {
        widthDiff = (srcWidth >= dstWidth) ? (srcWidth - dstWidth)
                                           : (dstWidth - srcWidth + kWidthDiffThreshold);
    }

    constexpr int SLANT_RANGE = 3;
    int diffSlantValue[SLANT_RANGE][SLANT_RANGE] = {
        {0, 2, 1},
        {2, 0, 1},
        {2, 1, 0}
    };
    if (dstStyle.slant() < 0 || dstStyle.slant() >= SLANT_RANGE ||
        srcStyle.slant() < 0 || srcStyle.slant() >= SLANT_RANGE) {
        return 0;
    }
    uint32_t slantDiff = diffSlantValue[dstStyle.slant()][srcStyle.slant()];

    int dstWeight = dstStyle.weight();
    int srcWeight = srcStyle.weight();
    uint32_t weightDiff = 0;

    // The maximum weight is kExtraBlack_Weight (1000), when dstWeight and srcWeight are at the different
    // side of kNormal_Weight, the weight difference between them should be more than 500 (1000/2)
    constexpr int kWeightDiffThreshold = 1000 / 2;
    if ((dstWeight == SkFontStyle::kNormal_Weight && srcWeight == SkFontStyle::kMedium_Weight) ||
        (dstWeight == SkFontStyle::kMedium_Weight && srcWeight == SkFontStyle::kNormal_Weight)) {
        weightDiff = 50;
    } else if (dstWeight <= SkFontStyle::kNormal_Weight) {
        weightDiff = (srcWeight <= dstWeight) ? (dstWeight - srcWeight)
                                              : (srcWeight - dstWeight + kWeightDiffThreshold);
    } else if (dstWeight > SkFontStyle::kNormal_Weight) {
        weightDiff = (srcWeight >= dstWeight) ? (srcWeight - dstWeight)
                                              : (dstWeight - srcWeight + kWeightDiffThreshold);
    }
    // The first 2 bytes to save weight difference, the third byte to save slant difference,
    // and the fourth byte to save width difference
    uint32_t diff = (widthDiff << 24) + (slantDiff << 16) + weightDiff;
    return diff;
}

int FontConfig_OHOS::parseConfig(const char* fname)
{
    Json::Value root;
    int err = checkConfigFile(fname, root);
    if (err != NO_ERROR) {
        return err;
    }

    for (const auto& key : root.getMemberNames()) {
        if (root[key].isArray() && key == "font_dir") {
            parseFontDir(fname, root[key]);
        } else if (root[key].isArray() && key == "fonts") {
            parseFonts(root[key]);
        }
    }

    return NO_ERROR;
}

template<class T>
void GetValue(T& v, const Json::Value& root)
{
    if (root.is<T>()) {
        v = std::move(root.as<T>());
    }
}

#define GEN_GET_FONT_FUNC(f, member) ([](FontJson&(f), const Json::Value& value) { GetValue((f).member, value); })


/*! parse the font item from the json document
 * \param array the font array from the json document
 * \return NO_ERROR successful
 */
int FontConfig_OHOS::parseFonts(const Json::Value& array)
{
    const std::unordered_map<std::string, std::function<void(FontConfig_OHOS::FontJson&, const Json::Value&)>>
        funcMap = { { "type", GEN_GET_FONT_FUNC(f, type) }, { "family", GEN_GET_FONT_FUNC(f, family) },
            { "index", GEN_GET_FONT_FUNC(f, index) }, { "weight", GEN_GET_FONT_FUNC(f, weight) },
            { "lang", GEN_GET_FONT_FUNC(f, lang) }, { "file", GEN_GET_FONT_FUNC(f, file) },
            { "alias", GEN_GET_FONT_FUNC(f, alias) } };

    std::vector<FontJson> fonts;

    for (const auto& font : array) {
        if (!font.isObject()) {
            continue;
        }

        FontJson f;
        for (const auto& key : font.getMemberNames()) {
            if (funcMap.count(key)) {
                funcMap.at(key)(f, font[key]);
            }
        }

        sk_sp<SkTypeface_OHOS> typeface = nullptr;
        for (auto& dir : fFontDir) {
            std::string path = dir + f.file;
            if (loadFont(path.c_str(), f, typeface) == 0) {
                fFontCollection.emplaceFont(std::move(f), std::move(typeface));
                break;
            }
        }
    }
    forAll([this](Font& f) { sortTypefaceSet(f.typefaces); });
    return NO_ERROR;
}

/*! check the system font configuration document
 * \param fname the full name of the font configuration document
 * \return NO_ERROR successful
 * \return ERROR_CONFIG_NOT_FOUND config document is not found
 * \return ERROR_CONFIG_FORMAT_NOT_SUPPORTED config document format is not supported
 */
int FontConfig_OHOS::checkConfigFile(const char* fname, Json::Value& root)
{
    std::ifstream fstream(fname);

    if (!fstream.is_open()) {
        return ERROR_CONFIG_NOT_FOUND;
    }

    Json::Reader reader;
    bool isJson = reader.parse(fstream, root, false);
    if (!isJson) {
        return ERROR_CONFIG_FORMAT_NOT_SUPPORTED;
    }
    return NO_ERROR;
}

/*! To parse 'fontdir' attribute
 * \param root the root node of 'fontdir'
 * \return NO_ERROR successful
 * \return ERROR_CONFIG_INVALID_VALUE_TYPE invalid value type
 */
int FontConfig_OHOS::parseFontDir(const char* fname, const Json::Value& root)
{
    for (auto& path : root) {
        if (!path.isString()) {
            continue;
        }
        std::string dir;
#ifdef SK_BUILD_FONT_MGR_FOR_PREVIEW
        if (strcmp(fname, OHOS_DEFAULT_CONFIG) == 0) {
            dir = path.asString() != "/system/fonts/" ? path.asString() : "fonts";
        } else {
            dir = path.asString() != "/system/fonts/" ? path.asString() : "../../../../hms/previewer/resources/fonts";
        }
#else
        dir = path.asString();
#endif
        fFontDir.push_back(std::move(dir));
    }
    return NO_ERROR;
}

/*! To load font information from a font file
 * \param fname the full name of a font file
 * \param info the font information read from json document
 * \param typeface the typeface to be loaded
 * \return NO_ERROR successful
 * \return ERROR_FONT_NOT_EXIST font file is not exist
 * \return ERROR_FONT_INVALID_STREAM the stream is not recognized
 */
int FontConfig_OHOS::loadFont(const char* fname, FontJson& info, sk_sp<SkTypeface_OHOS>& typeface)
{
    std::unique_ptr<SkStreamAsset> stream = SkStream::MakeFromFile(fname);
    if (stream == nullptr) {
        return ERROR_FONT_NOT_EXIST;
    }
    int count = 1;
    FontInfo font(fname, info.index);
    if (!fFontScanner.recognizedFont(stream.get(), &count) ||
        !fFontScanner.scanFont(stream.get(), info.index, &font.familyName, &font.style, &font.isFixedWidth, nullptr)) {
        return ERROR_FONT_INVALID_STREAM;
    }

    const char* temp = info.type == FontType::Generic ? info.alias.c_str() : "";
    SkString family(temp);
    typeface = sk_make_sp<SkTypeface_OHOS>(family, font);
    return NO_ERROR;
}


void FontConfig_OHOS::loadHMSymbol()
{
    for (auto& dir : fFontDir) {
        if (G_IS_HMSYMBOL_ENABLE &&
            HmSymbolConfig_OHOS::GetInstance()->ParseConfigOfHmSymbol(
                "hm_symbol_config_next.json", SkString(dir.c_str())) ==
                NO_ERROR) {
            break;
        }
    }
}

/*! To sort the typeface set
 * \param typefaceSet the typeface set to be sorted
 */
void FontConfig_OHOS::sortTypefaceSet(std::vector<sk_sp<SkTypeface_OHOS>>& typefaceSet)
{
    std::sort(typefaceSet.begin(), typefaceSet.end(), [](const auto& a, const auto& b) {
        return (a->fontStyle().weight() < b->fontStyle().weight()) ||
               (a->fontStyle().weight() == b->fontStyle().weight() && a->fontStyle().slant() < b->fontStyle().slant());
    });
}


bool FontConfig_OHOS::judgeFileExist()
{
    bool haveFile = false;
    for (const auto& path : fFontDir) {
        DIR* dir = opendir(path.c_str());
        if (dir == nullptr) {
            continue;
        }
        struct dirent* node = nullptr;
#if defined(SK_BUILD_FONT_MGR_FOR_PREVIEW_WIN)
        struct stat fileStat;
#endif
        while ((node = readdir(dir))) {
#if defined(SK_BUILD_FONT_MGR_FOR_PREVIEW_WIN)
            stat(node->d_name, &fileStat);
            if (S_ISDIR(fileStat.st_mode)) {
                continue;
            }
#else
            if (node->d_type != DT_REG) {
                continue;
            }
#endif
            const char* fileName = node->d_name;
            int len = strlen(fileName);
            int suffixLen = strlen(".ttf");
            if (len < suffixLen || (strncmp(fileName + len - suffixLen, ".ttf", suffixLen) != 0 &&
                                       strncmp(fileName + len - suffixLen, ".otf", suffixLen) != 0 &&
                                       strncmp(fileName + len - suffixLen, ".ttc", suffixLen) != 0 &&
                                       strncmp(fileName + len - suffixLen, ".otc", suffixLen) != 0)) {
                continue;
            }
            haveFile = true;
            break;
        }
        (void)closedir(dir);
        if (haveFile) {
            break;
        }
    }
    return haveFile;
}

int FontConfig_OHOS::checkProductFile(const char* fname)
{
    std::lock_guard<std::mutex> lock(fFontMutex);
    int err = parseConfig(PRODUCT_DEFAULT_CONFIG);
    if (err != NO_ERROR || !judgeFileExist()) {
        err = parseConfig(DEFAULT_CONFIG);
    }
    return err;
}