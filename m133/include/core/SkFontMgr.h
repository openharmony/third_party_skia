/*
 * Copyright 2013 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef SkFontMgr_DEFINED
#define SkFontMgr_DEFINED

#include "include/core/SkRefCnt.h"
#include "include/core/SkTypes.h"
#if defined(SK_BUILD_FONT_MGR_FOR_PREVIEW_WIN) or defined(SK_BUILD_FONT_MGR_FOR_PREVIEW_MAC) or \
    defined(SK_BUILD_FONT_MGR_FOR_PREVIEW_LINUX)
#include <string>
#endif

#include <memory>
#include <string>

#ifdef ENABLE_TEXT_ENHANCE
#include <vector>
enum FontCheckCode {
    SUCCESSED                  = 0, /** no error */
    ERROR_PARSE_CONFIG_FAILED  = 1, /** failed to parse the JSON configuration file */
    ERROR_TYPE_OTHER           = 2  /** other reasons, such as empty input parameters or other internal reasons */
};

struct SkByteArray {
    std::unique_ptr<uint8_t[]> strData = nullptr; // A byte array in UTF-16BE encoding
    uint32_t strLen = 0;
};
#endif

class SkData;
class SkFontStyle;
class SkStreamAsset;
class SkString;
class SkTypeface;
struct SkFontArguments;

class SK_API SkFontStyleSet : public SkRefCnt {
public:
    virtual int count() = 0;
    virtual void getStyle(int index, SkFontStyle*, SkString* style) = 0;
    virtual sk_sp<SkTypeface> createTypeface(int index) = 0;
    virtual sk_sp<SkTypeface> matchStyle(const SkFontStyle& pattern) = 0;

    static sk_sp<SkFontStyleSet> CreateEmpty();

protected:
    sk_sp<SkTypeface> matchStyleCSS3(const SkFontStyle& pattern);
};

class SK_API SkFontMgr : public SkRefCnt {
public:
    int countFamilies() const;
    void getFamilyName(int index, SkString* familyName) const;
    sk_sp<SkFontStyleSet> createStyleSet(int index) const;
#if defined(SK_BUILD_FONT_MGR_FOR_PREVIEW_WIN) or defined(SK_BUILD_FONT_MGR_FOR_PREVIEW_MAC) or \
    defined(SK_BUILD_FONT_MGR_FOR_PREVIEW_LINUX) or defined(CROSS_PLATFORM)
    /**
     * OHOS_Container font base path. It is empty when using OpenHarmony fonts.
     */
    static std::string containerFontPath;
    /**
     * Indicate the runtimeOS of preview(OHOS_Container and OHOS)
     */
    static std::string runtimeOS;
#endif
    /**
     *  The caller must call unref() on the returned object.
     *  Never returns NULL; will return an empty set if the name is not found.
     *
     *  Passing nullptr as the parameter will return the default system family.
     *  Note that most systems don't have a default system family, so passing nullptr will often
     *  result in the empty set.
     *
     *  It is possible that this will return a style set not accessible from
     *  createStyleSet(int) due to hidden or auto-activated fonts.
     */
    sk_sp<SkFontStyleSet> matchFamily(const char familyName[]) const;

    /**
     *  Find the closest matching typeface to the specified familyName and style
     *  and return a ref to it. The caller must call unref() on the returned
     *  object. Will return nullptr if no 'good' match is found.
     *
     *  Passing |nullptr| as the parameter for |familyName| will return the
     *  default system font.
     *
     *  It is possible that this will return a style set not accessible from
     *  createStyleSet(int) or matchFamily(const char[]) due to hidden or
     *  auto-activated fonts.
     */
    sk_sp<SkTypeface> matchFamilyStyle(const char familyName[], const SkFontStyle&) const;

    /**
     *  Use the system fallback to find a typeface for the given character.
     *  Note that bcp47 is a combination of ISO 639, 15924, and 3166-1 codes,
     *  so it is fine to just pass a ISO 639 here.
     *
     *  Will return NULL if no family can be found for the character
     *  in the system fallback.
     *
     *  Passing |nullptr| as the parameter for |familyName| will return the
     *  default system font.
     *
     *  bcp47[0] is the least significant fallback, bcp47[bcp47Count-1] is the
     *  most significant. If no specified bcp47 codes match, any font with the
     *  requested character will be matched.
     */
    sk_sp<SkTypeface> matchFamilyStyleCharacter(const char familyName[], const SkFontStyle&,
                                                const char* bcp47[], int bcp47Count,
                                                SkUnichar character) const;

    /**
     *  Create a typeface for the specified data and TTC index (pass 0 for none)
     *  or NULL if the data is not recognized. The caller must call unref() on
     *  the returned object if it is not null.
     */
    sk_sp<SkTypeface> makeFromData(sk_sp<SkData>, int ttcIndex = 0) const;

    /**
     *  Create a typeface for the specified stream and TTC index
     *  (pass 0 for none) or NULL if the stream is not recognized. The caller
     *  must call unref() on the returned object if it is not null.
     */
    sk_sp<SkTypeface> makeFromStream(std::unique_ptr<SkStreamAsset>, int ttcIndex = 0) const;

    /* Experimental, API subject to change. */
    sk_sp<SkTypeface> makeFromStream(std::unique_ptr<SkStreamAsset>, const SkFontArguments&) const;

    /**
     *  Create a typeface for the specified fileName and TTC index
     *  (pass 0 for none) or NULL if the file is not found, or its contents are
     *  not recognized. The caller must call unref() on the returned object
     *  if it is not null.
     */
    sk_sp<SkTypeface> makeFromFile(const char path[], int ttcIndex = 0) const;

    sk_sp<SkTypeface> legacyMakeTypeface(const char familyName[], SkFontStyle style) const;

#ifdef ENABLE_TEXT_ENHANCE
    // this method is never called -- will be removed
    virtual sk_sp<SkTypeface> onMatchFaceStyle(const SkTypeface*,
                                               const SkFontStyle&) const {
        return nullptr;
    }

    std::vector<sk_sp<SkTypeface>> getSystemFonts();

    /** Return the default fontmgr. */
    static sk_sp<SkFontMgr> RefDefault();
#endif

    /* Returns an empty font manager without any typeface dependencies */
    static sk_sp<SkFontMgr> RefEmpty();

#if defined(SK_BUILD_FONT_MGR_FOR_PREVIEW_WIN) or defined(SK_BUILD_FONT_MGR_FOR_PREVIEW_MAC) or \
    defined(SK_BUILD_FONT_MGR_FOR_PREVIEW_LINUX) or defined(CROSS_PLATFORM)
    /** Set the runtimeOS and container font base path */
    static void SetFontMgrConfig(const std::string runtime, const std::string containerFontBasePath)
    {
        containerFontPath = containerFontBasePath;
        runtimeOS = runtime;
    }
#endif
#ifdef ENABLE_TEXT_ENHANCE
    /**
     *  Adding a base class interface function to a subclass, generally doesn't go here
     *  0 means valid
     */
    virtual int GetFontFullName(int fontFd, std::vector<SkByteArray> &fullnameVec)
    {
        return ERROR_TYPE_OTHER;
    }
    /**
     *  Adding a base class interface function to a subclass, generally doesn't go here
     *  0 means success
     */
    virtual int ParseInstallFontConfig(const std::string& configPath, std::vector<std::string>& fontPathVec)
    {
        return ERROR_PARSE_CONFIG_FAILED;
    }
#endif

protected:
    virtual int onCountFamilies() const = 0;
    virtual void onGetFamilyName(int index, SkString* familyName) const = 0;
    virtual sk_sp<SkFontStyleSet> onCreateStyleSet(int index)const  = 0;

    /** May return NULL if the name is not found. */
    virtual sk_sp<SkFontStyleSet> onMatchFamily(const char familyName[]) const = 0;

    virtual sk_sp<SkTypeface> onMatchFamilyStyle(const char familyName[],
                                                 const SkFontStyle&) const = 0;
    virtual sk_sp<SkTypeface> onMatchFamilyStyleCharacter(const char familyName[],
                                                          const SkFontStyle&,
                                                          const char* bcp47[], int bcp47Count,
                                                          SkUnichar character) const = 0;

    virtual sk_sp<SkTypeface> onMakeFromData(sk_sp<SkData>, int ttcIndex) const = 0;
    virtual sk_sp<SkTypeface> onMakeFromStreamIndex(std::unique_ptr<SkStreamAsset>,
                                                    int ttcIndex) const = 0;
    virtual sk_sp<SkTypeface> onMakeFromStreamArgs(std::unique_ptr<SkStreamAsset>,
                                                   const SkFontArguments&) const = 0;
    virtual sk_sp<SkTypeface> onMakeFromFile(const char path[], int ttcIndex) const = 0;

    virtual sk_sp<SkTypeface> onLegacyMakeTypeface(const char familyName[], SkFontStyle) const = 0;

#ifdef ENABLE_TEXT_ENHANCE
    virtual std::vector<sk_sp<SkTypeface>> onGetSystemFonts() const;
#endif

private:

    /** Implemented by porting layer to return the default factory. */
    static sk_sp<SkFontMgr> Factory();
};

#endif
