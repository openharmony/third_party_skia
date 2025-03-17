// Copyright (c) 2023 Huawei Device Co., Ltd. All rights reserved
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FONTCONFIG_OHOS_H
#define FONTCONFIG_OHOS_H

#include <json/json.h>
#include <mutex>
#include <vector>

#include "FontInfo_ohos.h"
#include "HmSymbolConfig_ohos.h"
#include "SkFontDescriptor.h"
#include "SkFontHost_FreeType_common.h"
#include "SkFontStyle.h"
#include "SkStream.h"
#include "SkString.h"
#include "SkTypeface_ohos.h"
#include "SkTypes.h"

/*!
 * Error code definition
 */
namespace ErrorCode {

enum {
    NO_ERROR = 0,                      // no error
    ERROR_CONFIG_NOT_FOUND,            // the configuration document is not found
    ERROR_CONFIG_FORMAT_NOT_SUPPORTED, // the formation of configuration is not supported
    ERROR_CONFIG_MISSING_TAG,          // missing tag in the configuration
    ERROR_CONFIG_INVALID_VALUE_TYPE,   // invalid value type in the configuration
    ERROR_FONT_NOT_EXIST,              // the font file is not exist
    ERROR_FONT_INVALID_STREAM,         // the stream is not recognized
    ERROR_FONT_NO_STREAM,              // no stream in the font data
    ERROR_FAMILY_NOT_FOUND,            // the family name is not found in the system
    ERROR_NO_AVAILABLE_FAMILY,         // no available family in the system
    ERROR_DIR_NOT_FOUND,               // the directory is not exist
    ERROR_CONFIG_FUN_NOT_DEFINED,      // the symbol load config func is not register

    ERROR_TYPE_COUNT,
};
} /* namespace ErrorCode */

constexpr size_t RANGE_SIZE = 6;
constexpr size_t UNICODE_RANGE_SIZE = 332;
using UnicodeRange = std::array<uint64_t, RANGE_SIZE>;

/*!
 * \brief To parse the font configuration document and manage the system fonts
 */
class FontConfig_OHOS {
public:
    enum FontType : uint32_t { Generic = 0, Fallback, NumOfFontType };

    // to map the json document's font object
    struct FontJson {
        uint32_t type = 0;
        uint32_t slant = 0;
        uint32_t index = 0;
        uint32_t weight = 400;
        std::string alias;
        std::string family;
        std::string lang;
        std::string file;
        UnicodeRange range { UINT64_MAX, UINT64_MAX, UINT64_MAX, UINT64_MAX, UINT64_MAX, UINT64_MAX };
    };

    struct Font {
        FontType type = FontType::Generic;
        // slant: 0 - normal, 1 - italic, 2 - oblique
        uint32_t slant = 0;
        // the ttc font index, only valid for ttc font
        uint32_t index = 0;
        // only valid for the font with alias
        uint32_t weight = 0;
        std::string alias;
        // essential for the every font
        std::string family;
        // only valid for the fallback font
        std::string lang;
        // all the typefaces of this font
        std::vector<sk_sp<SkTypeface_OHOS>> typefaces;

        // may be redundant move
        explicit Font(FontJson& info);
    };

    explicit FontConfig_OHOS(const SkTypeface_FreeType::Scanner& fontScanner, const char* fname = nullptr);
    virtual ~FontConfig_OHOS() = default;
    SkTypeface* matchFallback(SkUnichar character, const SkFontStyle& style) const;
    SkTypeface* matchFallback(size_t index, SkUnichar character, const SkFontStyle& style) const;
    std::vector<size_t> matchFallbackByBCP47(std::function<int(const std::string&)>) const;
    int getFamilyCount() const;
    int getDefaultFamily(SkString& familyName) const;
    int getFamilyName(size_t index, SkString& familyName) const;
    size_t getTypefaceCount(size_t styleIndex, bool isFallback = false) const;
    bool getStyleIndex(const char* familyName, bool& isFallback, size_t& index) const;
    sk_sp<SkTypeface_OHOS> getFallbackTypeface(const SkString& familyName, const SkFontStyle& style) const;
    void forAll(std::function<void(Font&)> func)
    {
        fFontCollection.forAll(func);
    }

    sk_sp<SkTypeface_OHOS> getTypefaceSP(size_t styleIndex, size_t index, bool isFallback = false) const;
    SkTypeface_OHOS* getTypeface(size_t styleIndex, size_t index, bool isFallback = false) const;
    SkTypeface_OHOS* getTypeface(size_t styleIndex, const SkFontStyle& style, bool isFallback = false) const;

    static sk_sp<SkTypeface_OHOS> matchFontStyle(
        const std::vector<sk_sp<SkTypeface_OHOS>>& typefaceSet, const SkFontStyle& pattern);

private:
    struct FontCollection {
        std::vector<Font> fFallback;
        std::vector<Font> fGeneric;
        std::unordered_map<std::string, std::pair<size_t, FontType>> fIndexMap;
        std::array<std::vector<size_t>, UNICODE_RANGE_SIZE> fRangeToIndex;

        void emplaceFont(FontJson&& fj, sk_sp<SkTypeface_OHOS>&& typeface);
        bool getIndexByFamilyName(const std::string& family, std::pair<size_t, FontType>& res) const;
        const std::vector<Font>& getSet(bool isFallback) const;
        void forAll(std::function<void(Font&)> func);
    } fFontCollection;

    std::vector<std::string> fFontDir; // the directories where the fonts are
    const SkTypeface_FreeType::Scanner& fFontScanner;
    mutable std::mutex fFontMutex;
    static const std::map<std::pair<uint32_t, uint32_t>, int16_t> gRangeMap;

    int parseConfig(const char* fname);
    int checkConfigFile(const char* fname, Json::Value& root);
    int parseFontDir(const char* fname, const Json::Value& root);
    int parseFonts(const Json::Value& root);

    int loadFont(const char* fname, FontJson& font, sk_sp<SkTypeface_OHOS>& typeface);
    void loadHMSymbol();
    static bool containRange(const UnicodeRange& range, size_t index);
    static void sortTypefaceSet(std::vector<sk_sp<SkTypeface_OHOS>>& typefaceSet);
    static uint32_t getFontStyleDifference(const SkFontStyle& style1, const SkFontStyle& style2);
    static int16_t charRangeIndex(SkUnichar unicode);
    FontConfig_OHOS(const FontConfig_OHOS&) = delete;
    FontConfig_OHOS& operator=(const FontConfig_OHOS&) = delete;
    FontConfig_OHOS(FontConfig_OHOS&&) = delete;
    FontConfig_OHOS& operator=(FontConfig_OHOS&&) = delete;
    int checkProductFile(const char* fname);
    bool judgeFileExist();
};

#endif /* FONTCONFIG_OHOS_H */
