// Copyright (c) 2023 Huawei Device Co., Ltd. All rights reserved
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SKTYPEFACE_OHOS_H
#define SKTYPEFACE_OHOS_H

#ifdef ENABLE_TEXT_ENHANCE
#include <mutex>

#include "SkFontHost_FreeType_common.h"
#include "SkFontStyle.h"
#include "SkStream.h"

#include "FontInfo_ohos.h"

/*!
 * \brief The implementation of SkTypeface for ohos platform
 */
class SK_API SkTypeface_OHOS : public SkTypeface_FreeType {
public:
    SkTypeface_OHOS(const SkString& specifiedName, const FontInfo& info);
    explicit SkTypeface_OHOS(const FontInfo& info);
    ~SkTypeface_OHOS() override = default;
    const FontInfo* getFontInfo() const;
    void updateStream(std::unique_ptr<SkStreamAsset> stream) override;
    int32_t getFontIndex() const override;
protected:
    std::unique_ptr<SkStreamAsset> onOpenStream(int* ttcIndex) const override;
    std::unique_ptr<SkFontData> onMakeFontData() const override;
    void onGetFontDescriptor(SkFontDescriptor* descriptor, bool* isLocal) const override;
    void onGetFamilyName(SkString* familyName) const override;

    void onGetFontPath(SkString* path) const override;

    sk_sp<SkTypeface> onMakeClone(const SkFontArguments& args) const override;
private:
    mutable std::mutex mutex_;
    void readStreamFromFile(const std::unique_ptr<FontInfo>& fontInfo) const;
    SkString specifiedName; // specified family name which is defined in the configuration file
    std::unique_ptr<FontInfo> fontInfo; // the font information of this typeface
};

#endif // ENABLE_TEXT_ENHANCE
#endif /* SKTYPEFACE_OHOS_H */
