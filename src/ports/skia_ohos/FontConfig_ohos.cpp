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
    int16_t index = charRangeIndex(character);
    if (index < 0) {
        return nullptr;
    }
    for (const auto& i : fFontCollection.fRangeToIndex[index]) {
        const auto& typefaces = fFontCollection.fFallback[i].typefaces;
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
 *  \param match the judge func, if the func return -1, it means the language tag is not matched
 *  \return the matched fallback typefaces' index set
*/
std::vector<size_t> FontConfig_OHOS::matchFallbackByBCP47(std::function<int(const std::string&)> match) const
{
    std::vector<size_t> res;
    for (size_t i = 0; i < fFontCollection.fFallback.size(); i += 1) {
        if (match(fFontCollection.fFallback[i].lang) != -1) {
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


template<>
bool Json::Value::is<UnicodeRange>() const
{
    if (!isArray() || size() != RANGE_SIZE) {
        return false;
    }
    return std::all_of(begin(), end(), [](const Value& item) { return item.isUInt64(); });
}


template<>
UnicodeRange Json::Value::as<UnicodeRange>() const
{
    UnicodeRange res;
    size_t index = 0;
    for (const auto& item : *this) {
        res[index++] = item.asUInt64();
    }
    return res;
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
            { "alias", GEN_GET_FONT_FUNC(f, alias) }, { "range", GEN_GET_FONT_FUNC(f, range)} };

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
int FontConfig_OHOS::loadFont(const char* fname, FontJson& info, sk_sp<SkTypeface_OHOS>& typeface) {
    std::unique_ptr<SkStreamAsset> stream = SkStream::MakeFromFile(fname);
    if (stream == nullptr) {
        return ERROR_FONT_NOT_EXIST;
    }
    FontInfo font(fname, info.index);
    if (!fFontScanner.scanFont(stream.get(), info.index, &font.familyName, &font.style, &font.isFixedWidth, nullptr)) {
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

FontConfig_OHOS::Font::Font(FontJson& info)
    : slant(info.slant), weight(info.weight), alias(info.alias), family(info.family), lang(info.lang)
{
    this->type = info.type >= FontType::NumOfFontType ? FontType::Generic : static_cast<FontType>(info.type);
}

bool FontConfig_OHOS::containRange(const UnicodeRange& range, size_t index)
{
    // because the range is 6 64-bit, so we need to / 64 means >> 6
    int16_t i = index >> 6;
    // get the bit position by mod 64 which means & 63
    int16_t bit = index & 63;
    return ((range[i] >> bit) & 1) != 0;
}

void FontConfig_OHOS::FontCollection::emplaceFont(FontJson&& fj, sk_sp<SkTypeface_OHOS>&& typeface)
{
    if (fj.family.empty()) {
        return;
    }
    Font f(fj);
    auto& targetVec = (f.type == FontType::Generic) ? fGeneric : fFallback;
    auto& targetName = (f.type == FontType::Generic) ? f.alias : f.family;
    // generic must have alias
    auto exist = fIndexMap.find(targetName);
    // if not found, insert directly
    if (exist == fIndexMap.end()) {
        for (size_t i = 0; i < UNICODE_RANGE_SIZE && f.type == FontType::Fallback; i += 1) {
            if (containRange(fj.range, i)) {
                fRangeToIndex[i].push_back(targetVec.size());
            }
        }
        fIndexMap.emplace(targetName, std::make_pair(targetVec.size(), f.type));
        targetVec.emplace_back(f);
        targetVec.back().typefaces.emplace_back(typeface);
    } else {
        // if exist, add the typeface to the font
        targetVec[exist->second.first].typefaces.emplace_back(typeface);
    }
}

bool FontConfig_OHOS::FontCollection::getIndexByFamilyName(
    const std::string& family, std::pair<size_t, FontType>& res) const
{
    if (fIndexMap.count(family)) {
        res = fIndexMap.at(family);
        return true;
    }
    return false;
}

const std::vector<FontConfig_OHOS::Font>& FontConfig_OHOS::FontCollection::getSet(bool isFallback) const
{
    return isFallback ? fFallback : fGeneric;
}

void FontConfig_OHOS::FontCollection::forAll(std::function<void(FontConfig_OHOS::Font&)> func)
{
    for (auto& f : fFallback) {
        func(f);
    }
    for (auto& f : fGeneric) {
        func(f);
    }
}


int16_t FontConfig_OHOS::charRangeIndex(SkUnichar unicode)
{
    auto it = gRangeMap.upper_bound({ unicode, UINT64_MAX });
    if (it != gRangeMap.begin()) {
        --it;
    }
    if (unicode >= it->first.first && unicode <= it->first.second) {
        return it->second;
    }
    return -1;
}

const std::map<std::pair<uint32_t, uint32_t>, int16_t> FontConfig_OHOS::gRangeMap {
    { { 0x0, 0x7F }, 0 },
    { { 0x80, 0xFF }, 1 },
    { { 0x100, 0x17F }, 2 },
    { { 0x180, 0x24F }, 3 },
    { { 0x250, 0x2AF }, 4 },
    { { 0x2B0, 0x2FF }, 5 },
    { { 0x300, 0x36F }, 6 },
    { { 0x370, 0x3FF }, 7 },
    { { 0x400, 0x4FF }, 8 },
    { { 0x500, 0x52F }, 9 },
    { { 0x530, 0x58F }, 10 },
    { { 0x590, 0x5FF }, 11 },
    { { 0x600, 0x6FF }, 12 },
    { { 0x700, 0x74F }, 13 },
    { { 0x750, 0x77F }, 14 },
    { { 0x780, 0x7BF }, 15 },
    { { 0x7C0, 0x7FF }, 16 },
    { { 0x800, 0x83F }, 17 },
    { { 0x840, 0x85F }, 18 },
    { { 0x860, 0x86F }, 19 },
    { { 0x870, 0x89F }, 20 },
    { { 0x8A0, 0x8FF }, 21 },
    { { 0x900, 0x97F }, 22 },
    { { 0x980, 0x9FF }, 23 },
    { { 0xA00, 0xA7F }, 24 },
    { { 0xA80, 0xAFF }, 25 },
    { { 0xB00, 0xB7F }, 26 },
    { { 0xB80, 0xBFF }, 27 },
    { { 0xC00, 0xC7F }, 28 },
    { { 0xC80, 0xCFF }, 29 },
    { { 0xD00, 0xD7F }, 30 },
    { { 0xD80, 0xDFF }, 31 },
    { { 0xE00, 0xE7F }, 32 },
    { { 0xE80, 0xEFF }, 33 },
    { { 0xF00, 0xFFF }, 34 },
    { { 0x1000, 0x109F }, 35 },
    { { 0x10A0, 0x10FF }, 36 },
    { { 0x1100, 0x11FF }, 37 },
    { { 0x1200, 0x137F }, 38 },
    { { 0x1380, 0x139F }, 39 },
    { { 0x13A0, 0x13FF }, 40 },
    { { 0x1400, 0x167F }, 41 },
    { { 0x1680, 0x169F }, 42 },
    { { 0x16A0, 0x16FF }, 43 },
    { { 0x1700, 0x171F }, 44 },
    { { 0x1720, 0x173F }, 45 },
    { { 0x1740, 0x175F }, 46 },
    { { 0x1760, 0x177F }, 47 },
    { { 0x1780, 0x17FF }, 48 },
    { { 0x1800, 0x18AF }, 49 },
    { { 0x18B0, 0x18FF }, 50 },
    { { 0x1900, 0x194F }, 51 },
    { { 0x1950, 0x197F }, 52 },
    { { 0x1980, 0x19DF }, 53 },
    { { 0x19E0, 0x19FF }, 54 },
    { { 0x1A00, 0x1A1F }, 55 },
    { { 0x1A20, 0x1AAF }, 56 },
    { { 0x1AB0, 0x1AFF }, 57 },
    { { 0x1B00, 0x1B7F }, 58 },
    { { 0x1B80, 0x1BBF }, 59 },
    { { 0x1BC0, 0x1BFF }, 60 },
    { { 0x1C00, 0x1C4F }, 61 },
    { { 0x1C50, 0x1C7F }, 62 },
    { { 0x1C80, 0x1C8F }, 63 },
    { { 0x1C90, 0x1CBF }, 64 },
    { { 0x1CC0, 0x1CCF }, 65 },
    { { 0x1CD0, 0x1CFF }, 66 },
    { { 0x1D00, 0x1D7F }, 67 },
    { { 0x1D80, 0x1DBF }, 68 },
    { { 0x1DC0, 0x1DFF }, 69 },
    { { 0x1E00, 0x1EFF }, 70 },
    { { 0x1F00, 0x1FFF }, 71 },
    { { 0x2000, 0x206F }, 72 },
    { { 0x2070, 0x209F }, 73 },
    { { 0x20A0, 0x20CF }, 74 },
    { { 0x20D0, 0x20FF }, 75 },
    { { 0x2100, 0x214F }, 76 },
    { { 0x2150, 0x218F }, 77 },
    { { 0x2190, 0x21FF }, 78 },
    { { 0x2200, 0x22FF }, 79 },
    { { 0x2300, 0x23FF }, 80 },
    { { 0x2400, 0x243F }, 81 },
    { { 0x2440, 0x245F }, 82 },
    { { 0x2460, 0x24FF }, 83 },
    { { 0x2500, 0x257F }, 84 },
    { { 0x2580, 0x259F }, 85 },
    { { 0x25A0, 0x25FF }, 86 },
    { { 0x2600, 0x26FF }, 87 },
    { { 0x2700, 0x27BF }, 88 },
    { { 0x27C0, 0x27EF }, 89 },
    { { 0x27F0, 0x27FF }, 90 },
    { { 0x2800, 0x28FF }, 91 },
    { { 0x2900, 0x297F }, 92 },
    { { 0x2980, 0x29FF }, 93 },
    { { 0x2A00, 0x2AFF }, 94 },
    { { 0x2B00, 0x2BFF }, 95 },
    { { 0x2C00, 0x2C5F }, 96 },
    { { 0x2C60, 0x2C7F }, 97 },
    { { 0x2C80, 0x2CFF }, 98 },
    { { 0x2D00, 0x2D2F }, 99 },
    { { 0x2D30, 0x2D7F }, 100 },
    { { 0x2D80, 0x2DDF }, 101 },
    { { 0x2DE0, 0x2DFF }, 102 },
    { { 0x2E00, 0x2E7F }, 103 },
    { { 0x2E80, 0x2EFF }, 104 },
    { { 0x2F00, 0x2FDF }, 105 },
    { { 0x2FF0, 0x2FFF }, 106 },
    { { 0x3000, 0x303F }, 107 },
    { { 0x3040, 0x309F }, 108 },
    { { 0x30A0, 0x30FF }, 109 },
    { { 0x3100, 0x312F }, 110 },
    { { 0x3130, 0x318F }, 111 },
    { { 0x3190, 0x319F }, 112 },
    { { 0x31A0, 0x31BF }, 113 },
    { { 0x31C0, 0x31EF }, 114 },
    { { 0x31F0, 0x31FF }, 115 },
    { { 0x3200, 0x32FF }, 116 },
    { { 0x3300, 0x33FF }, 117 },
    { { 0x3400, 0x4DBF }, 118 },
    { { 0x4DC0, 0x4DFF }, 119 },
    { { 0x4E00, 0x9FFF }, 120 },
    { { 0xA000, 0xA48F }, 121 },
    { { 0xA490, 0xA4CF }, 122 },
    { { 0xA4D0, 0xA4FF }, 123 },
    { { 0xA500, 0xA63F }, 124 },
    { { 0xA640, 0xA69F }, 125 },
    { { 0xA6A0, 0xA6FF }, 126 },
    { { 0xA700, 0xA71F }, 127 },
    { { 0xA720, 0xA7FF }, 128 },
    { { 0xA800, 0xA82F }, 129 },
    { { 0xA830, 0xA83F }, 130 },
    { { 0xA840, 0xA87F }, 131 },
    { { 0xA880, 0xA8DF }, 132 },
    { { 0xA8E0, 0xA8FF }, 133 },
    { { 0xA900, 0xA92F }, 134 },
    { { 0xA930, 0xA95F }, 135 },
    { { 0xA960, 0xA97F }, 136 },
    { { 0xA980, 0xA9DF }, 137 },
    { { 0xA9E0, 0xA9FF }, 138 },
    { { 0xAA00, 0xAA5F }, 139 },
    { { 0xAA60, 0xAA7F }, 140 },
    { { 0xAA80, 0xAADF }, 141 },
    { { 0xAAE0, 0xAAFF }, 142 },
    { { 0xAB00, 0xAB2F }, 143 },
    { { 0xAB30, 0xAB6F }, 144 },
    { { 0xAB70, 0xABBF }, 145 },
    { { 0xABC0, 0xABFF }, 146 },
    { { 0xAC00, 0xD7AF }, 147 },
    { { 0xD7B0, 0xD7FF }, 148 },
    { { 0xD800, 0xDB7F }, 149 },
    { { 0xDB80, 0xDBFF }, 150 },
    { { 0xDC00, 0xDFFF }, 151 },
    { { 0xE000, 0xF8FF }, 152 },
    { { 0xF900, 0xFAFF }, 153 },
    { { 0xFB00, 0xFB4F }, 154 },
    { { 0xFB50, 0xFDFF }, 155 },
    { { 0xFE00, 0xFE0F }, 156 },
    { { 0xFE10, 0xFE1F }, 157 },
    { { 0xFE20, 0xFE2F }, 158 },
    { { 0xFE30, 0xFE4F }, 159 },
    { { 0xFE50, 0xFE6F }, 160 },
    { { 0xFE70, 0xFEFF }, 161 },
    { { 0xFF00, 0xFFEF }, 162 },
    { { 0xFFF0, 0xFFFF }, 163 },
    { { 0x10000, 0x1007F }, 164 },
    { { 0x10080, 0x100FF }, 165 },
    { { 0x10100, 0x1013F }, 166 },
    { { 0x10140, 0x1018F }, 167 },
    { { 0x10190, 0x101CF }, 168 },
    { { 0x101D0, 0x101FF }, 169 },
    { { 0x10280, 0x1029F }, 170 },
    { { 0x102A0, 0x102DF }, 171 },
    { { 0x102E0, 0x102FF }, 172 },
    { { 0x10300, 0x1032F }, 173 },
    { { 0x10330, 0x1034F }, 174 },
    { { 0x10350, 0x1037F }, 175 },
    { { 0x10380, 0x1039F }, 176 },
    { { 0x103A0, 0x103DF }, 177 },
    { { 0x10400, 0x1044F }, 178 },
    { { 0x10450, 0x1047F }, 179 },
    { { 0x10480, 0x104AF }, 180 },
    { { 0x104B0, 0x104FF }, 181 },
    { { 0x10500, 0x1052F }, 182 },
    { { 0x10530, 0x1056F }, 183 },
    { { 0x10570, 0x105BF }, 184 },
    { { 0x10600, 0x1077F }, 185 },
    { { 0x10780, 0x107BF }, 186 },
    { { 0x10800, 0x1083F }, 187 },
    { { 0x10840, 0x1085F }, 188 },
    { { 0x10860, 0x1087F }, 189 },
    { { 0x10880, 0x108AF }, 190 },
    { { 0x108E0, 0x108FF }, 191 },
    { { 0x10900, 0x1091F }, 192 },
    { { 0x10920, 0x1093F }, 193 },
    { { 0x10980, 0x1099F }, 194 },
    { { 0x109A0, 0x109FF }, 195 },
    { { 0x10A00, 0x10A5F }, 196 },
    { { 0x10A60, 0x10A7F }, 197 },
    { { 0x10A80, 0x10A9F }, 198 },
    { { 0x10AC0, 0x10AFF }, 199 },
    { { 0x10B00, 0x10B3F }, 200 },
    { { 0x10B40, 0x10B5F }, 201 },
    { { 0x10B60, 0x10B7F }, 202 },
    { { 0x10B80, 0x10BAF }, 203 },
    { { 0x10C00, 0x10C4F }, 204 },
    { { 0x10C80, 0x10CFF }, 205 },
    { { 0x10D00, 0x10D3F }, 206 },
    { { 0x10E60, 0x10E7F }, 207 },
    { { 0x10E80, 0x10EBF }, 208 },
    { { 0x10EC0, 0x10EFF }, 209 },
    { { 0x10F00, 0x10F2F }, 210 },
    { { 0x10F30, 0x10F6F }, 211 },
    { { 0x10F70, 0x10FAF }, 212 },
    { { 0x10FB0, 0x10FDF }, 213 },
    { { 0x10FE0, 0x10FFF }, 214 },
    { { 0x11000, 0x1107F }, 215 },
    { { 0x11080, 0x110CF }, 216 },
    { { 0x110D0, 0x110FF }, 217 },
    { { 0x11100, 0x1114F }, 218 },
    { { 0x11150, 0x1117F }, 219 },
    { { 0x11180, 0x111DF }, 220 },
    { { 0x111E0, 0x111FF }, 221 },
    { { 0x11200, 0x1124F }, 222 },
    { { 0x11280, 0x112AF }, 223 },
    { { 0x112B0, 0x112FF }, 224 },
    { { 0x11300, 0x1137F }, 225 },
    { { 0x11400, 0x1147F }, 226 },
    { { 0x11480, 0x114DF }, 227 },
    { { 0x11580, 0x115FF }, 228 },
    { { 0x11600, 0x1165F }, 229 },
    { { 0x11660, 0x1167F }, 230 },
    { { 0x11680, 0x116CF }, 231 },
    { { 0x116D0, 0x116FF }, 232 },
    { { 0x11700, 0x1174F }, 233 },
    { { 0x11800, 0x1184F }, 234 },
    { { 0x118A0, 0x118FF }, 235 },
    { { 0x11900, 0x1195F }, 236 },
    { { 0x119A0, 0x119FF }, 237 },
    { { 0x11A00, 0x11A4F }, 238 },
    { { 0x11A50, 0x11AAF }, 239 },
    { { 0x11AB0, 0x11ABF }, 240 },
    { { 0x11AC0, 0x11AFF }, 241 },
    { { 0x11B00, 0x11B5F }, 242 },
    { { 0x11C00, 0x11C6F }, 243 },
    { { 0x11C70, 0x11CBF }, 244 },
    { { 0x11D00, 0x11D5F }, 245 },
    { { 0x11D60, 0x11DAF }, 246 },
    { { 0x11EE0, 0x11EFF }, 247 },
    { { 0x11F00, 0x11F5F }, 248 },
    { { 0x11FB0, 0x11FBF }, 249 },
    { { 0x11FC0, 0x11FFF }, 250 },
    { { 0x12000, 0x123FF }, 251 },
    { { 0x12400, 0x1247F }, 252 },
    { { 0x12480, 0x1254F }, 253 },
    { { 0x12F90, 0x12FFF }, 254 },
    { { 0x13000, 0x1342F }, 255 },
    { { 0x13430, 0x1345F }, 256 },
    { { 0x14400, 0x1467F }, 257 },
    { { 0x16800, 0x16A3F }, 258 },
    { { 0x16A40, 0x16A6F }, 259 },
    { { 0x16A70, 0x16ACF }, 260 },
    { { 0x16AD0, 0x16AFF }, 261 },
    { { 0x16B00, 0x16B8F }, 262 },
    { { 0x16D40, 0x16D7F }, 263 },
    { { 0x16E40, 0x16E9F }, 264 },
    { { 0x16F00, 0x16F9F }, 265 },
    { { 0x16FE0, 0x16FFF }, 266 },
    { { 0x17000, 0x187FF }, 267 },
    { { 0x18800, 0x18AFF }, 268 },
    { { 0x18B00, 0x18CFF }, 269 },
    { { 0x18D00, 0x18D7F }, 270 },
    { { 0x1AFF0, 0x1AFFF }, 271 },
    { { 0x1B000, 0x1B0FF }, 272 },
    { { 0x1B100, 0x1B12F }, 273 },
    { { 0x1B130, 0x1B16F }, 274 },
    { { 0x1B170, 0x1B2FF }, 275 },
    { { 0x1BC00, 0x1BC9F }, 276 },
    { { 0x1BCA0, 0x1BCAF }, 277 },
    { { 0x1CC00, 0x1CEBF }, 278 },
    { { 0x1CF00, 0x1CFCF }, 279 },
    { { 0x1D000, 0x1D0FF }, 280 },
    { { 0x1D100, 0x1D1FF }, 281 },
    { { 0x1D200, 0x1D24F }, 282 },
    { { 0x1D2C0, 0x1D2DF }, 283 },
    { { 0x1D2E0, 0x1D2FF }, 284 },
    { { 0x1D300, 0x1D35F }, 285 },
    { { 0x1D360, 0x1D37F }, 286 },
    { { 0x1D400, 0x1D7FF }, 287 },
    { { 0x1D800, 0x1DAAF }, 288 },
    { { 0x1DF00, 0x1DFFF }, 289 },
    { { 0x1E000, 0x1E02F }, 290 },
    { { 0x1E030, 0x1E08F }, 291 },
    { { 0x1E100, 0x1E14F }, 292 },
    { { 0x1E290, 0x1E2BF }, 293 },
    { { 0x1E2C0, 0x1E2FF }, 294 },
    { { 0x1E4D0, 0x1E4FF }, 295 },
    { { 0x1E5D0, 0x1E5FF }, 296 },
    { { 0x1E7E0, 0x1E7FF }, 297 },
    { { 0x1E800, 0x1E8DF }, 298 },
    { { 0x1E900, 0x1E95F }, 299 },
    { { 0x1EC70, 0x1ECBF }, 300 },
    { { 0x1ED00, 0x1ED4F }, 301 },
    { { 0x1EE00, 0x1EEFF }, 302 },
    { { 0x1F000, 0x1F02F }, 303 },
    { { 0x1F030, 0x1F09F }, 304 },
    { { 0x1F0A0, 0x1F0FF }, 305 },
    { { 0x1F100, 0x1F1FF }, 306 },
    { { 0x1F200, 0x1F2FF }, 307 },
    { { 0x1F300, 0x1F5FF }, 308 },
    { { 0x1F600, 0x1F64F }, 309 },
    { { 0x1F650, 0x1F67F }, 310 },
    { { 0x1F680, 0x1F6FF }, 311 },
    { { 0x1F700, 0x1F77F }, 312 },
    { { 0x1F780, 0x1F7FF }, 313 },
    { { 0x1F800, 0x1F8FF }, 314 },
    { { 0x1F900, 0x1F9FF }, 315 },
    { { 0x1FA00, 0x1FA6F }, 316 },
    { { 0x1FA70, 0x1FAFF }, 317 },
    { { 0x1FB00, 0x1FBFF }, 318 },
    { { 0x20000, 0x2A6DF }, 319 },
    { { 0x2A700, 0x2B73F }, 320 },
    { { 0x2B740, 0x2B81F }, 321 },
    { { 0x2B820, 0x2CEAF }, 322 },
    { { 0x2CEB0, 0x2EBEF }, 323 },
    { { 0x2EBF0, 0x2EE5F }, 324 },
    { { 0x2F800, 0x2FA1F }, 325 },
    { { 0x30000, 0x3134F }, 326 },
    { { 0x31350, 0x323AF }, 327 },
    { { 0xE0000, 0xE007F }, 328 },
    { { 0xE0100, 0xE01EF }, 329 },
    { { 0xF0000, 0xFFFFF }, 330 },
    { { 0x100000, 0x10FFFF }, 331 },
};
