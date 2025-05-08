// Copyright (c) 2023 Huawei Device Co., Ltd. All rights reserved
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifdef ENABLE_TEXT_ENHANCE
#include "SkFontMgr_ohos.h"

#include <string>

#include "SkTypeface_ohos.h"
#include "include/core/SkData.h"

using namespace ErrorCode;

/*! Constructor
 * \param path the full path of system font configuration document
 */
SkFontMgr_OHOS::SkFontMgr_OHOS(const char* path)
{
    fFontConfig = std::make_shared<FontConfig_OHOS>(fFontScanner, path);
    fFamilyCount = fFontConfig->getFamilyCount();
}

/*! To get the count of families
 * \return The count of families in the system
 */
int SkFontMgr_OHOS::onCountFamilies() const
{
    return fFamilyCount;
}

/*! To get the family name for a font style set
 * \param index the index of a font style set
 * \param[out] familyName the family name returned to the caller
 * \n          The family name will be reset to "", if index is out of range
 */
void SkFontMgr_OHOS::onGetFamilyName(int index, SkString* familyName) const
{
    if (fFontConfig == nullptr || familyName == nullptr) {
        return;
    }
    fFontConfig->getFamilyName(static_cast<size_t>(index), *familyName);
}

/*! To create an object of SkFontStyleSet
 * \param index the index of a font style set
 * \return The pointer of SkFontStyleSet
 * \n      Return null, if index is out of range
 * \note   The caller must call unref() on the returned object if it's not null
 */
sk_sp<SkFontStyleSet> SkFontMgr_OHOS::onCreateStyleSet(int index) const
{
    if (fFontConfig == nullptr) {
        return nullptr;
    }
    if (index < 0 || index >= this->countFamilies()) {
        return nullptr;
    }
    return sk_sp<SkFontStyleSet>(new SkFontStyleSet_OHOS(fFontConfig, index));
}

/*! To get a matched object of SkFontStyleSet
 * \param familyName the family name of a font style set
 * \return The pointer of SkFontStyleSet
 * \n      Return the default font style set, if family name is null
 * \n      Return null, if family name is not found
 * \note   The caller must call unref() on the returned object if it's not null
 */
sk_sp<SkFontStyleSet> SkFontMgr_OHOS::onMatchFamily(const char familyName[]) const
{
    if (fFontConfig == nullptr) {
        return nullptr;
    }
    // return default system font when familyName is null
    if (familyName == nullptr) {
        return sk_sp<SkFontStyleSet>(new SkFontStyleSet_OHOS(fFontConfig, 0));
    }

    bool isFallback = false;
    size_t index = 0;
    if (!fFontConfig->getStyleIndex(familyName, isFallback, index)) {
        return nullptr;
    }
    return sk_sp<SkFontStyleSet>(new SkFontStyleSet_OHOS(fFontConfig, index, isFallback));
}

/*! To get a matched typeface
 * \param familyName the family name of a font style set
 * \param style the font style to be matched
 * \return An object of typeface which is closest matching to 'style'
 * \n      Return the typeface in the default font style set, if family name is null
 * \n      Return null, if family name is not found
 * \note   The caller must call unref() on the returned object if it's not null
 */
sk_sp<SkTypeface> SkFontMgr_OHOS::onMatchFamilyStyle(const char familyName[], const SkFontStyle& style) const
{
    if (fFontConfig == nullptr) {
        return nullptr;
    }
    bool isFallback = false;
    size_t styleIndex = 0;
    if (!fFontConfig->getStyleIndex(familyName, isFallback, styleIndex)) {
        return nullptr;
    }
    return fFontConfig->getTypeface(styleIndex, style, isFallback);
}

struct SpecialUnicodeFamilyName {
    SkUnichar unicode;
    std::string familyName;
};

sk_sp<SkTypeface> SkFontMgr_OHOS::findSpecialTypeface(SkUnichar character, const SkFontStyle& style) const
{
    // The key values in this list are Unicode that support the identification characters
    // of several high-frequency languages in the fallback list corresponding to Chinese, Uyghur, and Tibetan
    static std::vector<SpecialUnicodeFamilyName> specialLists = {
        {0x0626, "HarmonyOS Sans Naskh Arabic UI"},
        {0x0F56, "Noto Serif Tibetan"}
    };

    std::string name;

    // base chinese unicode range is 0x4E00-0x9FA5
    if (character >= 0x4E00 && character <= 0x9FA5) {
        name.assign("HarmonyOS Sans SC");
    }

    for (int i = 0; i < specialLists.size() && name.empty(); i++) {
        if (character == specialLists[i].unicode) {
            name.assign(specialLists[i].familyName);
            break;
        }
    }

    if (name.empty()) {
        return nullptr;
    }

    SkString fname(name.c_str());
    sk_sp<SkTypeface_OHOS> typeface = fFontConfig->getFallbackTypeface(fname, style);
    return typeface;
}

/*! To get a matched typeface
 * \n Use the system fallback to find a typeface for the given character.
 * \param familyName the family name which the typeface is fallback For
 * \param style the font style to be matched
 * \param bcp47 an array of languages which indicate the language of 'character'
 * \param bcp47Count the array size of bcp47
 * \param character a UTF8 value to be matched
 * \return An object of typeface which is for the given character
 * \return Return the typeface in the default fallback set, if familyName is null
 * \return Return null, if the typeface is not found for the given character
 * \note The caller must call unref() on the returned object if it's not null
 */
sk_sp<SkTypeface> SkFontMgr_OHOS::onMatchFamilyStyleCharacter(const char familyName[], const SkFontStyle& style,
    const char* bcp47[], int bcp47Count, SkUnichar character) const
{
    if (fFontConfig == nullptr) {
        return nullptr;
    }

    auto res = findTypeface(style, bcp47, bcp47Count, character);
    if (res != nullptr) {
        return res;
    }
    res = findSpecialTypeface(character, style);
    if (res != nullptr) {
        return res;
    }
    return fFontConfig->matchFallback(character, style);
}

/*! To find the matched typeface for the given parameters
 * \n Use the system fallback to find a typeface for the given character.
 * \param fallbackItem the fallback items in which to find the typeface
 * \param style the font style to be matched
 * \param bcp47 an array of languages which indicate the language of 'character'
 * \param bcp47Count the array size of bcp47
 * \param character a UTF8 value to be matched
 * \return An object of typeface which is for the given character
 * \return Return null, if the typeface is not found for the given character
 */
sk_sp<SkTypeface> SkFontMgr_OHOS::findTypeface(const SkFontStyle& style,
    const char* bcp47[], int bcp47Count, SkUnichar character) const
{
    if (bcp47Count == 0 || fFontConfig == nullptr) {
        return nullptr;
    }

    std::vector<std::function<int(const std::string& langs)>> funcs = {
        [&bcp47, &bcp47Count](const std::string& langs) -> int {
            for (int i = 0; i < bcp47Count; i++) {
                if (langs == bcp47[i]) {
                    return i;
                }
            }
            return -1;
        },
        [&bcp47, &bcp47Count](const std::string& langs) -> int {
            return SkFontMgr_OHOS::compareLangs(langs, bcp47, bcp47Count);
        }
    };

    for (auto& func : funcs) {
        auto set = fFontConfig->matchFallbackByBCP47(func);
        for (auto& index : set) {
            auto res = fFontConfig->matchFallback(index, character, style);
            if (res != nullptr) {
                return res;
            }
        }
    }

    return nullptr;
}

/*! To compare the languages of an typeface with a bcp47 list
 * \param langs the supported languages by an typeface
 * \param bcp47 the array of bcp47 language to be matching
 * \param bcp47Count the array size of bcp47
 * \return The index of language in bcp47, if matching happens
 * \n      Return -1, if no language matching happens
 */
int SkFontMgr_OHOS::compareLangs(const std::string& langs, const char* bcp47[], int bcp47Count)
{
    /*
     * zh-Hans : ('zh' : iso639 code, 'Hans' : iso15924 code)
     */
    if (bcp47 == nullptr || bcp47Count == 0) {
        return -1;
    }
    for (int i = bcp47Count - 1; i >= 0; i--) {
        if (langs.find(bcp47[i]) != -1) {
            return i;
        } else {
            const char* iso15924 = strrchr(bcp47[i], '-');
            if (iso15924 == nullptr) {
                continue;
            }
            iso15924++;
            int len = iso15924 - 1 - bcp47[i];
            SkString country(bcp47[i], len);
            if (langs.find(iso15924) != std::string::npos ||
                (strncmp(bcp47[i], "und", strlen("und")) != 0 && langs.find(country.c_str()) != -1)) {
                return i + bcp47Count;
            }
        }
    }
    return -1;
}

/*! To get a matched typeface
 * \param typeface the given typeface with which the returned object should be in the same style set
 * \param style the font style to be matching
 * \return The object of typeface which is closest matching to the given 'style'
 * \n      Return null, if the family name of the given typeface is not found in the system
 * \note The caller must call unref() on the returned object if it's not null
 */
sk_sp<SkTypeface> SkFontMgr_OHOS::onMatchFaceStyle(const SkTypeface* typeface, const SkFontStyle& style) const
{
    if (typeface == nullptr) {
        return nullptr;
    }
    SkString familyName;
    typeface->getFamilyName(&familyName);
    return this->onMatchFamilyStyle(familyName.c_str(), style);
}

/*! To create a typeface from the specified data and TTC index
 * \param data the data to be parsed
 * \param index the index of typeface. 0 for none
 * \return The object of typeface, if successful
 * \n      Return null if the data is not recognized.
 * \note The caller must call unref() on the returned object if it's not null
 */
sk_sp<SkTypeface> SkFontMgr_OHOS::onMakeFromData(sk_sp<SkData> data, int ttcIndex) const
{
    if (data == nullptr) {
        return nullptr;
    }
    std::unique_ptr<SkMemoryStream> memoryStream = std::make_unique<SkMemoryStream>(data);
    SkFontArguments args;
    args.setCollectionIndex(ttcIndex);
    return this->makeTypeface(std::move(memoryStream), args, nullptr);
}

/*! To create a typeface from the specified stream and TTC index
 * \param data the stream to be parsed
 * \param index the index of typeface. 0 for none
 * \return The object of typeface, if successful
 * \n      Return null if the stream is not recognized.
 * \note The caller must call unref() on the returned object if it's not null
 */
sk_sp<SkTypeface> SkFontMgr_OHOS::onMakeFromStreamIndex(std::unique_ptr<SkStreamAsset> stream,
    int ttcIndex) const
{
    if (stream == nullptr) {
        return nullptr;
    }
    SkFontArguments args;
    args.setCollectionIndex(ttcIndex);
    return this->makeTypeface(std::move(stream), args, nullptr);
}

/*! To create a typeface from the specified stream and font arguments
 * \param data the stream to be parsed
 * \param args the arguments of font
 * \return The object of typeface, if successful
 * \n      Return null if the stream is not recognized.
 * \note The caller must call unref() on the returned object if it's not null
 */
sk_sp<SkTypeface> SkFontMgr_OHOS::onMakeFromStreamArgs(std::unique_ptr<SkStreamAsset> stream,
    const SkFontArguments& args) const
{
    if (stream == nullptr) {
        return nullptr;
    }

    return this->makeTypeface(std::move(stream), args, nullptr);
}

/*! To create a typeface from the specified font file and TTC index
 * \param path the full path of the given font file
 * \param ttcIndex the index of typeface in a ttc font file. 0 means none.
 * \return The object of typeface, if successful
 * \n      Return null if the font file is not found or the content of file is not recognized.
 * \note The caller must call unref() on the returned object if it's not null
 */
sk_sp<SkTypeface> SkFontMgr_OHOS::onMakeFromFile(const char path[], int ttcIndex) const
{
    if (fFontConfig == nullptr) {
        return nullptr;
    }

    std::unique_ptr<SkStreamAsset> stream = SkStreamAsset::MakeFromFile(path);
    if (stream == nullptr) {
        return nullptr;
    }
    SkFontArguments args;
    args.setCollectionIndex(ttcIndex);
    return this->makeTypeface(std::move(stream), args, path);
}

/*! To get a typeface matching the specified family and style
 * \param familyName the specified name to be matching
 * \param style the specified style to be matching
 * \return The object of typeface which is the closest matching 'style' when the familyName is found
 * \return Return a typeface from the default family, if familyName is not found
 * \return Return null, if there is no any typeface in the system
 * \note The caller must caller unref() on the returned object is it's not null
 */
sk_sp<SkTypeface> SkFontMgr_OHOS::onLegacyMakeTypeface(const char familyName[], SkFontStyle style) const
{
    sk_sp<SkTypeface> typeface = this->onMatchFamilyStyle(familyName, style);
    // if familyName is not found, then try the default family
    if (typeface == nullptr && familyName != nullptr) {
        typeface = this->onMatchFamilyStyle(nullptr, style);
    }

    if (typeface) {
        return typeface;
    }
    return nullptr;
}

#ifdef ENABLE_TEXT_ENHANCE
std::vector<sk_sp<SkTypeface>> SkFontMgr_OHOS::onGetSystemFonts() const
{
    if (fFontConfig == nullptr) {
        return {};
    }
    std::vector<sk_sp<SkTypeface>> skTypefaces;
    fFontConfig->forAll([&skTypefaces](const auto& f) {
        for (auto& iter : f.typefaces) {
            skTypefaces.emplace_back(iter);
        }
    });

    return skTypefaces;
}
#endif

/*! To make a typeface from the specified stream and font arguments
 * \param stream the specified stream to be parsed to get font information
 * \param args the arguments of index or axis values
 * \param path the fullname of font file
 * \return The object of typeface if successful
 * \n      Return null, if the stream is not recognized
 */
sk_sp<SkTypeface> SkFontMgr_OHOS::makeTypeface(std::unique_ptr<SkStreamAsset> stream,
    const SkFontArguments& args, const char path[]) const
{
    FontInfo fontInfo;
    int ttcIndex = args.getCollectionIndex();
    int axisCount = args.getVariationDesignPosition().coordinateCount;

    if (path) {
        fontInfo.fname.set(path);
    }
    if (axisCount == 0) {
        if (!fFontScanner.scanFont(stream.get(), ttcIndex, &fontInfo.familyName,
                                   &fontInfo.style, &fontInfo.isFixedWidth, nullptr)) {
            return nullptr;
        }
    } else {
        SkFontScanner_FreeType::AxisDefinitions axisDef;
        if (!fFontScanner.scanFont(stream.get(), ttcIndex, &fontInfo.familyName,
                                   &fontInfo.style, &fontInfo.isFixedWidth, &axisDef)) {
            return nullptr;
        }
        if (axisDef.size() > 0) {
            SkFixed axis[axisDef.size()];
            SkFontScanner_FreeType::computeAxisValues(
                axisDef,
                SkFontArguments::VariationPosition{nullptr, 0},
                args.getVariationDesignPosition(),
                axis, fontInfo.familyName, nullptr);
            fontInfo.setAxisSet(axisCount, axis, axisDef.data());
            fontInfo.style = fontInfo.computeFontStyle();
        }
    }

    fontInfo.stream = std::move(stream);
    fontInfo.index = ttcIndex;
    return sk_make_sp<SkTypeface_OHOS>(fontInfo);
}

/*! Get the fullname of font
 * \param fontFd      The file descriptor for the font file
 * \param fullnameVec Read the font fullname list
 * \return Returns Whether the fullnameVec was successfully obtained, 0 means success, see FontCheckCode for details
 */
int SkFontMgr_OHOS::GetFontFullName(int fontFd, std::vector<SkByteArray> &fullnameVec)
{
    std::unique_ptr<SkMemoryStream> stream = std::make_unique<SkMemoryStream>(SkData::MakeFromFD(fontFd));
    int errorCode = SUCCESSED;
    int numFaces = 0;
    if (!fFontScanner.scanFile(stream.get(), &numFaces)) {
        return ERROR_TYPE_OTHER;
    }
    for (int faceIndex = 0; faceIndex < numFaces; ++faceIndex) {
        bool isFixedPitch = false;
        SkString realname;
        SkFontStyle style = SkFontStyle(); // avoid uninitialized warning
        if (!fFontScanner.scanFont(stream.get(), faceIndex, &realname, &style, &isFixedPitch, nullptr)) {
            errorCode = ERROR_TYPE_OTHER;
            break;
        }
        SkByteArray skFullName = {nullptr, 0};
        if (!fFontScanner.GetTypefaceFullname(stream.get(), faceIndex, skFullName)) {
            errorCode = ERROR_TYPE_OTHER;
            break;
        } else {
            fullnameVec.push_back(std::move(skFullName));
        }
    }
    return errorCode;
}

/*! To create SkFontMgr object for Harmony platform
 * \param fname the full name of system font configuration documents
 * \return The object of SkFontMgr_OHOS
 */
sk_sp<SkFontMgr> SkFontMgr_New_OHOS(const char* fname)
{
    return sk_make_sp<SkFontMgr_OHOS>(fname);
}

#endif // ENABLE_TEXT_ENHANCE