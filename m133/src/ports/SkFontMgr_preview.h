/*
 * Copyright 2015 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 * 2021.9.9 SkFontMgr on previewer of ohos.
 *           Copyright (c) 2021 Huawei Device Co., Ltd. All rights reserved.
 */

#ifndef SkFontMgr_preview_DEFINED
#define SkFontMgr_preview_DEFINED

#include "include/core/SkFontMgr.h"
#include "include/core/SkData.h"
#include "include/core/SkFontStyle.h"
#include "include/core/SkRefCnt.h"
#include "include/core/SkStream.h"
#include "include/core/SkString.h"
#include "include/core/SkTypeface.h"
#include "src/core/SkFontDescriptor.h"
#include "src/core/SkOSFile.h"
#include "src/ports/SkFontHost_FreeType_common.h"
#include "src/ports/SkFontMgr_config_parser.h"
#include "SkTypeface_FreeType.h"
#include "src/ports/SkFontScanner_FreeType_priv.h"
#include "include/private/base/SkTArray.h"
#include "include/private/base/SkTDArray.h"
#include "include/private/base/SkFixed.h"
#include "include/private/base/SkTemplates.h"

class SkData;

class SkTypeface_Preview : public SkTypeface_FreeType {
public:
    SkTypeface_Preview(const SkFontStyle& style,
                   bool isFixedPitch,
                   const SkString& familyName)
        : INHERITED(style, isFixedPitch)
        , fFamilyName(familyName)
    { }

    ~SkTypeface_Preview() override = default;

protected:
    void onGetFamilyName(SkString* familyName) const override
    {
        *familyName = fFamilyName;
    }

    SkString fFamilyName;

private:
    typedef SkTypeface_FreeType INHERITED;
};

class SkTypeface_PreviewSystem : public SkTypeface_Preview {
public:
    SkTypeface_PreviewSystem(const SkString& pathName,
                         int index,
                         const SkFixed* axes, int axesCount,
                         const SkFontStyle& style,
                         bool isFixedPitch,
                         const SkString& familyName,
                         const skia_private::TArray<SkLanguage, true>& lang,
                         FontVariant variantStyle)
        : INHERITED(style, isFixedPitch, familyName)
        , fPathName(pathName)
        , fIndex(index)
        , fAxes(axes, axesCount)
        , fLang(lang)
        , fVariantStyle(variantStyle)
        , fFile(nullptr)
    { }

    ~SkTypeface_PreviewSystem() override = default;

    std::unique_ptr<SkStreamAsset> makeStream() const
    {
        if (fFile) {
            sk_sp<SkData> data(SkData::MakeFromFILE(fFile));
            return data ? std::make_unique<SkMemoryStream>(std::move(data)) : nullptr;
        }
        return SkStream::MakeFromFile(fPathName.c_str());
    }

    virtual void onGetFontDescriptor(SkFontDescriptor* desc, bool* serialize) const override
    {
        SkASSERT(desc);
        SkASSERT(serialize);
        desc->setFamilyName(fFamilyName.c_str());
        desc->setStyle(this->fontStyle());
        *serialize = false;
    }

    std::unique_ptr<SkStreamAsset> onOpenStream(int* ttcIndex) const override
    {
        *ttcIndex = fIndex;
        return this->makeStream();
    }

    std::unique_ptr<SkFontData> onMakeFontData() const override
    {
        return std::make_unique<SkFontData>(this->makeStream(), fIndex, 0, fAxes.begin(), fAxes.size(), nullptr, 0);
    }

    sk_sp<SkTypeface> onMakeClone(const SkFontArguments& args) const override
    {
        SkFontStyle style = this->fontStyle();
        std::unique_ptr<SkFontData> data = this->cloneFontData(args, &style);
        if (!data) {
            return nullptr;
        }
        return sk_make_sp<SkTypeface_PreviewSystem>(fPathName,
                                                fIndex,
                                                data->getAxis(),
                                                data->getAxisCount(),
                                                this->fontStyle(),
                                                this->isFixedPitch(),
                                                fFamilyName,
                                                fLang,
                                                fVariantStyle);
    }

    const SkString fPathName;
    int fIndex;
    const skia_private::STArray<4, SkFixed, true> fAxes;
    const skia_private::STArray<4, SkLanguage, true> fLang;
    const FontVariant fVariantStyle;
    SkAutoTCallVProc<FILE, sk_fclose> fFile;

    typedef SkTypeface_Preview INHERITED;
};

class SkTypeface_PreviewStream : public SkTypeface_Preview {
public:
    SkTypeface_PreviewStream(std::unique_ptr<SkFontData> data,
                         const SkFontStyle& style,
                         bool isFixedPitch,
                         const SkString& familyName)
        : INHERITED(style, isFixedPitch, familyName)
        , fData(std::move(data))
    { }

    ~SkTypeface_PreviewStream() override = default;

    virtual void onGetFontDescriptor(SkFontDescriptor* desc, bool* serialize) const override
    {
        SkASSERT(desc);
        SkASSERT(serialize);
        desc->setFamilyName(fFamilyName.c_str());
        *serialize = true;
    }

    std::unique_ptr<SkStreamAsset> onOpenStream(int* ttcIndex) const override
    {
        *ttcIndex = fData->getIndex();
        return fData->getStream()->duplicate();
    }

    std::unique_ptr<SkFontData> onMakeFontData() const override
    {
        return std::make_unique<SkFontData>(*fData);
    }

    sk_sp<SkTypeface> onMakeClone(const SkFontArguments& args) const override
    {
        SkFontStyle style = this->fontStyle();
        std::unique_ptr<SkFontData> data = this->cloneFontData(args, &style);
        if (!data) {
            return nullptr;
        }
        return sk_make_sp<SkTypeface_PreviewStream>(std::move(data), this->fontStyle(), this->isFixedPitch(), fFamilyName);
    }

private:
    const std::unique_ptr<const SkFontData> fData;
    typedef SkTypeface_Preview INHERITED;
};

class SkFontStyleSet_Preview : public SkFontStyleSet {
    typedef SkFontScanner_FreeType Scanner;

public:
    explicit SkFontStyleSet_Preview(const FontFamily& family, const Scanner& scanner)
    {
        const SkString* cannonicalFamilyName = nullptr;
        if (family.fNames.size() > 0) {
            cannonicalFamilyName = &family.fNames[0];
        }
        fFallbackFor = family.fFallbackFor;

        // TODO? make this lazy
        for (int i = 0; i < family.fFonts.size(); ++i) {
            const FontFileInfo& fontFile = family.fFonts[i];

            SkString pathName(family.fBasePath);
            pathName.append(fontFile.fFileName);

            std::unique_ptr<SkStreamAsset> stream = SkStream::MakeFromFile(pathName.c_str());
            if (!stream) {
                SkDEBUGF("Requested font file %s does not exist or cannot be opened.\n", pathName.c_str());
                continue;
            }

            const int ttcIndex = fontFile.fIndex;
            SkString familyName;
            SkFontStyle style;
            bool isFixedWidth = true;
            Scanner::AxisDefinitions axisDefinitions;

            if (!scanner.scanFont(stream.get(), ttcIndex,
                &familyName, &style, &isFixedWidth, &axisDefinitions)) {
                SkDEBUGF("Requested font file %s exists, but is not a valid font.\n",
                         pathName.c_str());
                continue;
            }

            int weight = fontFile.fWeight != 0 ? fontFile.fWeight : style.weight();
            SkFontStyle::Slant slant = style.slant();
            switch (fontFile.fStyle) {
                case FontFileInfo::Style::kAuto:
                    slant = style.slant();
                    break;
                case FontFileInfo::Style::kNormal:
                    slant = SkFontStyle::kUpright_Slant;
                    break;
                case FontFileInfo::Style::kItalic:
                    slant = SkFontStyle::kItalic_Slant;
                    break;
                default:
                    SkASSERT(false);
                    break;
            }
            style = SkFontStyle(weight, style.width(), slant);

            uint32_t variant = family.fVariant;
            if (kDefault_FontVariant == variant) {
                variant = kCompact_FontVariant | kElegant_FontVariant;
            }

            // The first specified family name overrides the family name found in the font.
            // TODO: SkTypeface_PreviewSystem::onCreateFamilyNameIterator should return
            // all of the specified family names in addition to the names found in the font.
            if (cannonicalFamilyName != nullptr) {
                familyName = *cannonicalFamilyName;
            }

            SkAutoSTMalloc<4, SkFixed> axisValues(axisDefinitions.size());
            SkFontScanner::VariationPosition current;
            const SkFontArguments::VariationPosition currentPos{current.data(), current.size()};
            SkFontArguments::VariationPosition position = {
                fontFile.fVariationDesignPosition.begin(),
                fontFile.fVariationDesignPosition.size()
            };
            Scanner::computeAxisValues(axisDefinitions, currentPos, position, axisValues, familyName, &style);

            fStyles.push_back().reset(new SkTypeface_PreviewSystem(pathName, ttcIndex, axisValues.get(),
                                                               axisDefinitions.size(), style, isFixedWidth,
                                                               familyName, family.fLanguages, variant));
        }
    }

    ~SkFontStyleSet_Preview() override = default;

    int count() override
    {
        return fStyles.size();
    }

    void getStyle(int index, SkFontStyle* style, SkString* name) override
    {
        if (index < 0 || fStyles.size() <= index) {
            return;
        }
        if (style) {
            *style = fStyles[index]->fontStyle();
        }
        if (name) {
            name->reset();
        }
    }

    sk_sp<SkTypeface> createTypeface(int index) override
    {
        if (index < 0 || fStyles.size() <= index) {
            return nullptr;
        }
        return sk_sp<SkTypeface>(SkRef(fStyles[index].get()));
    }

    sk_sp<SkTypeface> matchStyle(const SkFontStyle& pattern) override
    {
        return static_cast<sk_sp<SkTypeface>>(this->matchStyleCSS3(pattern));
    }

private:
    skia_private::TArray<sk_sp<SkTypeface_PreviewSystem>> fStyles;
    SkString fFallbackFor;

    friend struct NameToFamily;
    friend class SkFontMgr_Preview;

    typedef SkFontStyleSet INHERITED;
};

/** A single family can have many names, but our API assumes unique names.
 *  Map names to the back end so that all names for a given family refer to the same
 *  (non-replicated) set of typefaces.
 *  SkTDict<> doesn't let us do index-based lookup, so we write our own mapping.
 */
struct NameToFamily {
    SkString name;
    SkFontStyleSet_Preview* styleSet;
};

SK_API sk_sp<SkFontMgr> SkFontMgr_New_Preview();

class SkFontMgr_Preview : public SkFontMgr {
public:
    SkFontMgr_Preview();
    ~SkFontMgr_Preview() override = default;

protected:
    int onCountFamilies() const override;
    void onGetFamilyName(int index, SkString* familyName) const override;
    sk_sp<SkFontStyleSet> onCreateStyleSet(int index) const override;
    sk_sp<SkFontStyleSet> onMatchFamily(const char familyName[]) const override;
    virtual sk_sp<SkTypeface> onMatchFamilyStyle(const char familyName[],
                                           const SkFontStyle& style) const override;
    virtual sk_sp<SkTypeface> onMatchFaceStyle(const SkTypeface* typeface,
                                         const SkFontStyle& style) const override;

    static sk_sp<SkTypeface_PreviewSystem> find_family_style_character(
            const SkString& familyName,
            const skia_private::TArray<NameToFamily, true>& fallbackNameToFamilyMap,
            const SkFontStyle& style, bool elegant,
            const SkString& langTag, SkUnichar character);

    virtual sk_sp<SkTypeface> onMatchFamilyStyleCharacter(const char familyName[],
                                                    const SkFontStyle& style,
                                                    const char* bcp47[],
                                                    int bcp47Count,
                                                    SkUnichar character) const override;
    sk_sp<SkTypeface> onMakeFromData(sk_sp<SkData> data, int ttcIndex) const override;
    sk_sp<SkTypeface> onMakeFromFile(const char path[], int ttcIndex) const override;
    sk_sp<SkTypeface> makeTypeface(std::unique_ptr<SkStreamAsset> stream,
                                   const SkFontArguments& args, const char path[]) const;
    sk_sp<SkTypeface> onMakeFromStreamIndex(std::unique_ptr<SkStreamAsset> stream, int ttcIndex) const override;
    sk_sp<SkTypeface> onMakeFromStreamArgs(std::unique_ptr<SkStreamAsset> stream,
                                           const SkFontArguments& args) const override;
    sk_sp<SkTypeface> onLegacyMakeTypeface(const char familyName[], SkFontStyle style) const override;

private:
    void buildNameToFamilyMap(SkTDArray<FontFamily*> families);
    void findDefaultStyleSet();
    void addFamily(FontFamily& family, int familyIndex);

    SkFontScanner_FreeType fScanner;

    sk_sp<SkFontStyleSet> fDefaultStyleSet;
    skia_private::TArray<sk_sp<SkFontStyleSet_Preview>> fStyleSets;
    skia_private::TArray<NameToFamily, true> fNameToFamilyMap;
    skia_private::TArray<NameToFamily, true> fFallbackNameToFamilyMap;

    typedef SkFontMgr INHERITED;
};

#endif // SkFontMgr_preview_DEFINED