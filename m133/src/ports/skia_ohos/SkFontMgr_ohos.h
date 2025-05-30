/*
 * Copyright 2015 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 * 2023.4.23 SkFontMgr on ohos.
 *           Copyright (c) 2023 Huawei Device Co., Ltd. All rights reserved.
 */

#ifndef SKFONTMGR_OHOS_H
#define SKFONTMGR_OHOS_H

#ifdef ENABLE_TEXT_ENHANCE
#include "FontConfig_ohos.h"
#include "SkFontDescriptor.h"
#include "SkFontMgr.h"
#include "SkFontStyleSet_ohos.h"

/*!
 * \brief To implement the SkFontMgr for ohos platform
 */
class SK_API SkFontMgr_OHOS : public SkFontMgr {
public:
    explicit SkFontMgr_OHOS(const char* path = nullptr);
    ~SkFontMgr_OHOS() override = default;

    int GetFontFullName(int fontFd, std::vector<SkByteArray> &fullnameVec) override;
protected:
    int onCountFamilies() const override;
    void onGetFamilyName(int index, SkString* familyName) const override;
    SkFontStyleSet* onCreateStyleSet(int index)const override;

    SkFontStyleSet* onMatchFamily(const char familyName[]) const override;

    SkTypeface* onMatchFamilyStyle(const char familyName[],
                                           const SkFontStyle& style) const override;
    SkTypeface* onMatchFamilyStyleCharacter(const char familyName[], const SkFontStyle& style,
                                                    const char* bcp47[], int bcp47Count,
                                                    SkUnichar character) const override;

    SkTypeface* onMatchFaceStyle(const SkTypeface* typeface,
                                         const SkFontStyle& style) const override;

    sk_sp<SkTypeface> onMakeFromData(sk_sp<SkData> data, int ttcIndex) const override;
    sk_sp<SkTypeface> onMakeFromStreamIndex(std::unique_ptr<SkStreamAsset> stream,
                                                    int ttcIndex) const override;
    sk_sp<SkTypeface> onMakeFromStreamArgs(std::unique_ptr<SkStreamAsset> stream,
                                                   const SkFontArguments& args) const override;
    sk_sp<SkTypeface> onMakeFromFile(const char path[], int ttcIndex) const override;

    sk_sp<SkTypeface> onLegacyMakeTypeface(const char familyName[], SkFontStyle style) const override;

    std::vector<sk_sp<SkTypeface>> onGetSystemFonts() const override;

private:
    std::shared_ptr<FontConfig_OHOS> fFontConfig = nullptr; // the pointer of FontConfig_OHOS
    SkFontScanner_FreeType fFontScanner; // the scanner to parse a font file
    int fFamilyCount = 0; // the count of font style sets in generic family list

    static int compareLangs(const std::string& langs, const char* bcp47[], int bcp47Count);
    sk_sp<SkTypeface> makeTypeface(std::unique_ptr<SkStreamAsset> stream,
                                    const SkFontArguments& args, const char path[]) const;
    SkTypeface* findTypeface(const SkFontStyle& style,
                             const char* bcp47[], int bcp47Count,
                             SkUnichar character) const;
    SkTypeface* findSpecialTypeface(SkUnichar character, const SkFontStyle& style) const;
};

SK_API sk_sp<SkFontMgr> SkFontMgr_New_OHOS(const char* path);
SK_API sk_sp<SkFontMgr> SkFontMgr_New_OHOS() {
    return SkFontMgr_New_OHOS("/system/etc/fontconfig_ohos.json");
}

#endif // ENABLE_TEXT_ENHANCE
#endif /* SKFONTMGR_OHOS_H */
