// Copyright (c) 2023 Huawei Device Co., Ltd. All rights reserved
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef ENABLE_TEXT_ENHANCE
#include "SkTypeface_ohos.h"

#include "securec.h"

#include "SkFontDescriptor.h"
#include "SkFontHost_FreeType_common.h"
#include "SkFontScanner_FreeType_priv.h"
#include "SkTArray.h"

/*! Constructor
 * \param familyName the specified family name for the typeface
 * \param info the font information for the typeface
 */
SkTypeface_OHOS::SkTypeface_OHOS(const SkString& familyName, const FontInfo& info)
    : SkTypeface_FreeType(info.style, info.isFixedWidth),
      specifiedName(familyName)
{
    fontInfo = std::make_unique<FontInfo>(std::move(info));
}

/*! Constructor
 * \param info the font information for the typeface
 */
SkTypeface_OHOS::SkTypeface_OHOS(const FontInfo& info)
    : SkTypeface_FreeType(info.style, info.isFixedWidth)
{
    specifiedName.reset();
    fontInfo = std::make_unique<FontInfo>(std::move(info));
}

void SkTypeface_OHOS::readStreamFromFile(const std::unique_ptr<FontInfo>& fontInfo) const
{
    if (fontInfo->stream != nullptr) {
        return;
    }
    std::lock_guard<std::mutex> lock(mutex_);
    if (fontInfo->stream == nullptr) {
        fontInfo->stream = SkStream::MakeFromFile(fontInfo->fname.c_str());
    }
}

/*! To get stream of the typeface
 * \param[out] ttcIndex the index of the typeface in a ttc file returned to the caller
 * \return The stream object of the typeface
 */
std::unique_ptr<SkStreamAsset> SkTypeface_OHOS::onOpenStream(int* ttcIndex) const
{
    if (fontInfo) {
        if (ttcIndex) {
            *ttcIndex = fontInfo->index;
        }
        readStreamFromFile(fontInfo);
        if (fontInfo->stream) {
            return fontInfo->stream->duplicate();
        }
    }
    return nullptr;
}

/*! To make font data from the typeface
 * \return The object of SkFontData
 */
std::unique_ptr<SkFontData> SkTypeface_OHOS::onMakeFontData() const
{
    if (fontInfo == nullptr) {
        return nullptr;
    }
    readStreamFromFile(fontInfo);
    if (fontInfo->stream == nullptr) {
        return nullptr;
    }
    return std::make_unique<SkFontData>(fontInfo->stream->duplicate(), fontInfo->index,
               0, fontInfo->axisSet.axis.data(), fontInfo->axisSet.axis.size(), nullptr, 0);
}

/*! To get the font descriptor of the typeface
 * \param[out] descriptor the font descriptor returned to the caller
 * \param[out] isLocal the false to the caller
 */
void SkTypeface_OHOS::onGetFontDescriptor(SkFontDescriptor* descriptor, bool* isLocal) const
{
    if (isLocal) {
        *isLocal = false;
    }
    if (descriptor) {
        // used for choosing SkTypeface_FreeType decoder during deserialization
        descriptor->setFactoryId(SkTypeface_FreeType::FactoryId);
        SkString familyName;
        onGetFamilyName(&familyName);
        descriptor->setFamilyName(familyName.c_str());
        descriptor->setStyle(this->fontStyle());
        descriptor->setVariationCoordinates(fontInfo->axisSet.axis.size());
        for (int i = 0; i < fontInfo->axisSet.axis.size(); i++) {
            descriptor->getVariation()[i].axis = fontInfo->axisSet.range[i].tag;
            // The axis actual value need to dealt by SkFixedToFloat because the real-time value was
            // changed in SkFontScanner_FreeType::computeAxisValues
            descriptor->getVariation()[i].value = SkFixedToFloat(fontInfo->axisSet.axis[i]);
        }
    }
}

/*! To get the family name of the typeface
 * \param[out] familyName the family name returned to the caller
 */
void SkTypeface_OHOS::onGetFamilyName(SkString* familyName) const
{
    if (familyName == nullptr) {
        return;
    }
    if (specifiedName.size() > 0) {
        *familyName = specifiedName;
    } else {
        if (fontInfo) {
            *familyName = fontInfo->familyName;
        }
    }
}

void SkTypeface_OHOS::onGetFontPath(SkString* path) const
{
    if (path == nullptr) {
        return;
    }
    if (fontInfo != nullptr) {
        *path = fontInfo->fname;
    }
}

/*! To clone a typeface from this typeface
 * \param args the specified font arguments from which the new typeface is created
 * \return The object of a new typeface
 * \note The caller must call unref() on the returned object
 */
sk_sp<SkTypeface> SkTypeface_OHOS::onMakeClone(const SkFontArguments& args) const
{
    int ttcIndex = args.getCollectionIndex();
    auto stream = openStream(&ttcIndex);

    FontInfo info(*(fontInfo.get()));
    unsigned int axisCount = args.getVariationDesignPosition().coordinateCount;
    if (axisCount > 0) {
        SkFontScanner_FreeType fontScanner;
        SkFontScanner_FreeType::AxisDefinitions axisDefs;
        if (!fontScanner.scanFont(stream.get(), ttcIndex, &info.familyName, &info.style,
            &info.isFixedWidth, &axisDefs)) {
            return nullptr;
        }
        if (axisDefs.size() > 0) {
            SkFixed axis[axisDefs.size()];
            fontScanner.computeAxisValues(
                axisDefs,
                SkFontArguments::VariationPosition{nullptr, 0},
                args.getVariationDesignPosition(),
                axis, info.familyName, nullptr);
            info.setAxisSet(axisCount, axis, axisDefs.data());
            info.style = info.computeFontStyle();
            return sk_make_sp<SkTypeface_OHOS>(specifiedName, info);
        }
    }

    return sk_ref_sp(this);
}

/*! To get the font information of the typeface
 * \return The object of FontInfo
 */
const FontInfo* SkTypeface_OHOS::getFontInfo() const
{
    return fontInfo.get();
}

#endif // ENABLE_TEXT_ENHANCE
