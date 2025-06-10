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
 *     \n The default value is '/system/etc/fontconfig_ohos.json', if fname is given null
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
static const char* OHOS_DEFAULT_CONFIG = "/system/etc/fontconfig_ohos.json";
/*! Constructor
 * \param fontScanner the scanner to get the font information from a font file
 * \param fname the full name of system font configuration document.
 *     \n The default value is '/system/etc/fontconfig_ohos.json', if fname is given null
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
    // find the fallback typeface at emoji first
    const auto& emoji = getFallbackTypeface(SkString("HMOS Color Emoji"), style);
    if (emoji && emoji->unicharToGlyph(character)) {
        return SkSafeRef(emoji.get());
    }

    for (auto& f : fFontCollection.fFallback) {
        const auto& typefaces = f.typefaces;
        // for the compatibility to the old version
        if (!typefaces.empty() && (f.containChar(character) || f.family.find("JP") != std::string::npos)) {
            if (!typefaces[0]->unicharToGlyph(character)) {
                continue;
            }
            return SkSafeRef(matchFontStyle(typefaces, style).get());
        }
    }

    // find the fallback typeface at emoji flags third
    const auto& flags = getFallbackTypeface(SkString("HMOS Color Emoji Flags"), style);
    if (flags && flags->unicharToGlyph(character)) {
        return SkSafeRef(flags.get());
    }

    for (auto& f : fFontCollection.fFallback) {
        const auto& typefaces = f.typefaces;
        if (!typefaces.empty() && typefaces[0]->unicharToGlyph(character)) {
            return SkSafeRef(matchFontStyle(typefaces, style).get());
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
    return (styleIndex < set.size()) ? set[styleIndex].typefaces.size() : 0;
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
    auto& fontSet = fFontCollection.getSet(isFallback);
    if (styleIndex <= fontSet.size()) {
        // if index less than typefaces' size, return the ptr
        return (index < fontSet[styleIndex].typefaces.size()) ? fontSet[styleIndex].typefaces[index] : nullptr;
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
    auto& fontSet = fFontCollection.getSet(isFallback);
    if (styleIndex >= fontSet.size()) {
        return nullptr;
    }

    const std::vector<sk_sp<SkTypeface_OHOS>>& pSet = fontSet[styleIndex].typefaces;
    sk_sp<SkTypeface_OHOS> tp = matchFontStyle(pSet, style);
    return tp.get();
}

/*! To get the index of a font style set
 *  \param familyName the family name of the font style set
 *  \n     get the index of default font style set, if 'familyName' is null
 *  \param[out] isFallback to tell if the family is from generic or fallback to the caller.
 *  \n          isFallback is false, if the font style is from generic family list
 *  \n          isFallback is true, if the font style is from fallback family list
 *  \param[out] index the index of the font set
 *  \return The index of the font style set
 *  \n      Return false, if 'familyName' is not found in the system
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
    if (fname == nullptr) {
        fname = OHOS_DEFAULT_CONFIG;
    }
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
            std::array<uint32_t, 4> range{};
            if (loadFont(path.c_str(), f, typeface, range) == 0) {
                fFontCollection.emplaceFont(std::move(f), std::move(typeface), range);
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
            dir = (path.asString() != "/system/fonts/") ? path.asString() : "fonts/";
        } else {
            dir = (path.asString() != "/system/fonts/")
                ? path.asString() : "../../../../hms/previewer/resources/fonts/";
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
int FontConfig_OHOS::loadFont(
    const char* fname, FontJson& info, sk_sp<SkTypeface_OHOS>& typeface, std::array<uint32_t, 4>& range)
{
    std::unique_ptr<SkStreamAsset> stream = SkStream::MakeFromFile(fname);
    if (stream == nullptr) {
        return ERROR_FONT_NOT_EXIST;
    }
    FontInfo font(fname, info.index);
    SkTypeface_FreeType::Scanner::FontInfo fontInfo{ info.index, font.familyName, font.style, font.isFixedWidth };
    if (!fFontScanner.scanFont(stream.get(), fontInfo, range)) {
        return ERROR_FONT_INVALID_STREAM;
    }

    const char* temp = (info.type == FontType::Generic) ? info.alias.c_str() : "";
    SkString family(temp);
    typeface = sk_make_sp<SkTypeface_OHOS>(family, font);
    return NO_ERROR;
}

void FontConfig_OHOS::loadHMSymbol()
{
    if (!G_IS_HMSYMBOL_ENABLE) {
        return;
    }
    for (auto& dir : fFontDir) {
        if (skia::text::HmSymbolConfig_OHOS::LoadSymbolConfig(
            "hm_symbol_config_next.json", SkString(dir.c_str())) == NO_ERROR) {
            return;
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
            if (len < suffixLen ||
                (strncmp(fileName + len - suffixLen, ".ttf", suffixLen) != 0 &&
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
        err = parseConfig(OHOS_DEFAULT_CONFIG);
    }
    return err;
}

static const std::map<std::pair<uint32_t, uint32_t>, int8_t> gRangeMap {
    { { 0x0000, 0x007F }, 0 },
    { { 0x0080, 0x00FF }, 1 },
    { { 0x0100, 0x017F }, 2 },
    { { 0x0180, 0x024F }, 3 },
    { { 0x0250, 0x02AF }, 4 },
    { { 0x1D00, 0x1DBF }, 4 },
    { { 0x02B0, 0x02FF }, 5 },
    { { 0xA700, 0xA71F }, 5 },
    { { 0x0300, 0x036F }, 6 },
    { { 0x1DC0, 0x1DFF }, 6 },
    { { 0x0370, 0x03FF }, 7 },
    { { 0x2C80, 0x2CFF }, 8 },
    { { 0x0400, 0x052F }, 9 },
    { { 0x2DE0, 0x2DFF }, 9 },
    { { 0xA640, 0xA69F }, 9 },
    { { 0x0530, 0x058F }, 10 },
    { { 0x0590, 0x05FF }, 11 },
    { { 0xA500, 0xA63F }, 12 },
    { { 0x0600, 0x06FF }, 13 },
    { { 0x0750, 0x077F }, 13 },
    { { 0x07C0, 0x07FF }, 14 },
    { { 0x0900, 0x097F }, 15 },
    { { 0x0980, 0x09FF }, 16 },
    { { 0x0A00, 0x0A7F }, 17 },
    { { 0x0A80, 0x0AFF }, 18 },
    { { 0x0B00, 0x0B7F }, 19 },
    { { 0x0B80, 0x0BFF }, 20 },
    { { 0x0C00, 0x0C7F }, 21 },
    { { 0x0C80, 0x0CFF }, 22 },
    { { 0x0D00, 0x0D7F }, 23 },
    { { 0x0E00, 0x0E7F }, 24 },
    { { 0x0E80, 0x0EFF }, 25 },
    { { 0x10A0, 0x10FF }, 26 },
    { { 0x2D00, 0x2D2F }, 26 },
    { { 0x1B00, 0x1B7F }, 27 },
    { { 0x1100, 0x11FF }, 28 },
    { { 0x1E00, 0x1EFF }, 29 },
    { { 0x2C60, 0x2C7F }, 29 },
    { { 0xA720, 0xA7FF }, 29 },
    { { 0x1F00, 0x1FFF }, 30 },
    { { 0x2000, 0x206F }, 31 },
    { { 0x2E00, 0x2E7F }, 31 },
    { { 0x2070, 0x209F }, 32 },
    { { 0x20A0, 0x20CF }, 33 },
    { { 0x20D0, 0x20FF }, 34 },
    { { 0x2100, 0x214F }, 35 },
    { { 0x2150, 0x218F }, 36 },
    { { 0x2190, 0x21FF }, 37 },
    { { 0x27F0, 0x27FF }, 37 },
    { { 0x2900, 0x297F }, 37 },
    { { 0x2B00, 0x2BFF }, 37 },
    { { 0x2200, 0x22FF }, 38 },
    { { 0x2A00, 0x2AFF }, 38 },
    { { 0x27C0, 0x27EF }, 38 },
    { { 0x2980, 0x29FF }, 38 },
    { { 0x2300, 0x23FF }, 39 },
    { { 0x2400, 0x243F }, 40 },
    { { 0x2440, 0x245F }, 41 },
    { { 0x2460, 0x24FF }, 42 },
    { { 0x2500, 0x257F }, 43 },
    { { 0x2580, 0x259F }, 44 },
    { { 0x25A0, 0x25FF }, 45 },
    { { 0x2600, 0x26FF }, 46 },
    { { 0x2700, 0x27BF }, 47 },
    { { 0x3000, 0x303F }, 48 },
    { { 0x3040, 0x309F }, 49 },
    { { 0x30A0, 0x30FF }, 50 },
    { { 0x31F0, 0x31FF }, 50 },
    { { 0x3100, 0x312F }, 51 },
    { { 0x31A0, 0x31BF }, 51 },
    { { 0x3130, 0x318F }, 52 },
    { { 0xA840, 0xA87F }, 53 },
    { { 0x3200, 0x32FF }, 54 },
    { { 0x3300, 0x33FF }, 55 },
    { { 0xAC00, 0xD7AF }, 56 },
    // Ignore Non-Plane 0 (57), since this is not a real range.
    { { 0x10900, 0x1091F }, 58 },
    { { 0x4E00, 0x9FFF }, 59 },
    { { 0x2E80, 0x2FDF }, 59 },
    { { 0x2FF0, 0x2FFF }, 59 },
    { { 0x3400, 0x4DBF }, 59 },
    { { 0x20000, 0x2A6DF }, 59 },
    { { 0x3190, 0x319F }, 59 },
    { { 0xE000, 0xF8FF }, 60 },
    { { 0x31C0, 0x31EF }, 61 },
    { { 0xF900, 0xFAFF }, 61 },
    { { 0x2F800, 0x2FA1F }, 61 },
    { { 0xFB00, 0xFB4F }, 62 },
    { { 0xFB50, 0xFDFF }, 63 },
    { { 0xFE20, 0xFE2F }, 64 },
    { { 0xFE10, 0xFE1F }, 65 },
    { { 0xFE30, 0xFE4F }, 65 },
    { { 0xFE50, 0xFE6F }, 66 },
    { { 0xFE70, 0xFEFF }, 67 },
    { { 0xFF00, 0xFFEF }, 68 },
    { { 0xFFF0, 0xFFFF }, 69 },
    { { 0x0F00, 0x0FFF }, 70 },
    { { 0x0700, 0x074F }, 71 },
    { { 0x0780, 0x07BF }, 72 },
    { { 0x0D80, 0x0DFF }, 73 },
    { { 0x1000, 0x109F }, 74 },
    { { 0x1200, 0x139F }, 75 },
    { { 0x2D80, 0x2DDF }, 75 },
    { { 0x13A0, 0x13FF }, 76 },
    { { 0x1400, 0x167F }, 77 },
    { { 0x1680, 0x169F }, 78 },
    { { 0x16A0, 0x16FF }, 79 },
    { { 0x1780, 0x17FF }, 80 },
    { { 0x19E0, 0x19FF }, 80 },
    { { 0x1800, 0x18AF }, 81 },
    { { 0x2800, 0x28FF }, 82 },
    { { 0xA000, 0xA48F }, 83 },
    { { 0xA490, 0xA4CF }, 83 },
    { { 0x1700, 0x177F }, 84 },
    { { 0x10300, 0x1032F }, 85 },
    { { 0x10330, 0x1034F }, 86 },
    { { 0x10400, 0x1044F }, 87 },
    { { 0x1D000, 0x1D24F }, 88 },
    { { 0x1D400, 0x1D7FF }, 89 },
    { { 0xF0000, 0xFFFFD }, 90 },
    { { 0x100000, 0x10FFFD }, 90 },
    { { 0xFE00, 0xFE0F }, 91 },
    { { 0xE0100, 0xE01EF }, 91 },
    { { 0xE0000, 0xE007F }, 92 },
    { { 0x1900, 0x194F }, 93 },
    { { 0x1950, 0x197F }, 94 },
    { { 0x1980, 0x19DF }, 95 },
    { { 0x1A00, 0x1A1F }, 96 },
    { { 0x2C00, 0x2C5F }, 97 },
    { { 0x2D30, 0x2D7F }, 98 },
    { { 0x4DC0, 0x4DFF }, 99 },
    { { 0xA800, 0xA82F }, 100 },
    { { 0x10000, 0x1013F }, 101 },
    { { 0x10140, 0x1018F }, 102 },
    { { 0x10380, 0x1039F }, 103 },
    { { 0x103A0, 0x103DF }, 104 },
    { { 0x10450, 0x1047F }, 105 },
    { { 0x10480, 0x104AF }, 106 },
    { { 0x10800, 0x1083F }, 107 },
    { { 0x10A00, 0x10A5F }, 108 },
    { { 0x1D300, 0x1D35F }, 109 },
    { { 0x12000, 0x123FF }, 110 },
    { { 0x12400, 0x1247F }, 110 },
    { { 0x1D360, 0x1D37F }, 111 },
    { { 0x1B80, 0x1BBF }, 112 },
    { { 0x1C00, 0x1C4F }, 113 },
    { { 0x1C50, 0x1C7F }, 114 },
    { { 0xA880, 0xA8DF }, 115 },
    { { 0xA900, 0xA92F }, 116 },
    { { 0xA930, 0xA95F }, 117 },
    { { 0xAA00, 0xAA5F }, 118 },
    { { 0x10190, 0x101CF }, 119 },
    { { 0x101D0, 0x101FF }, 120 },
    { { 0x102A0, 0x102DF }, 121 },
    { { 0x10280, 0x1029F }, 121 },
    { { 0x10920, 0x1093F }, 121 },
    { { 0x1F030, 0x1F09F }, 122 },
    { { 0x1F000, 0x1F02F }, 122 },
};

static int8_t charRangeIndex(SkUnichar unicode)
{
    auto it = gRangeMap.upper_bound({ unicode, INT32_MAX });
    if (it != gRangeMap.begin()) {
        --it;
    }
    if (unicode >= it->first.first && unicode <= it->first.second) {
        return it->second;
    }
    return -1;
}

bool FontConfig_OHOS::Font::containChar(SkUnichar unicode) const
{
    int8_t r = charRangeIndex(unicode);
    if (r < 0) {
        return false;
    }
    // because the range is 128-bit, so we need to split it into 4 32-bit, / 32 means >> 5
    int8_t i = r >> 5;
    // get the bit position by mod 32 which means & 31
    int8_t bit = r & 31;
    return ((range[i] >> bit) & 1) != 0;
}