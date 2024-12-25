// Copyright (c) 2023 Huawei Device Co., Ltd. All rights reserved
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SKTYPEFACE_OHOS_H
#define SKTYPEFACE_OHOS_H

#include "SkFontHost_FreeType_common.h"
#include "SkFontStyle.h"
#include "SkStream.h"

#include "FontInfo_ohos.h"

/*!
 * \brief The implementation of SkTypeface for ohos platform
 */
class SK_API SkTypeface_OHOS : public SkTypeface_FreeType {
public:
    SkTypeface_OHOS(const SkString& specifiedName, FontInfo& info);
    explicit SkTypeface_OHOS(FontInfo& info);
    ~SkTypeface_OHOS() override = default;
    const FontInfo* getFontInfo() const;
protected:
    std::unique_ptr<SkStreamAsset> onOpenStream(int* ttcIndex) const override;
    std::unique_ptr<SkFontData> onMakeFontData() const override;
    void onGetFontDescriptor(SkFontDescriptor* descriptor, bool* isLocal) const override;
    void onGetFamilyName(SkString* familyName) const override;

#ifdef OHOS_SUPPORT
    void onGetFontPath(SkString* path) const override;
#endif

    sk_sp<SkTypeface> onMakeClone(const SkFontArguments& args) const override;
private:
    SkString specifiedName; // specified family name which is defined in the configuration file
    std::unique_ptr<FontInfo> fontInfo; // the font information of this typeface
};

#endif /* SKTYPEFACE_OHOS_H */
