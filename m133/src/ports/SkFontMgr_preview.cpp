/*
 * Copyright 2014 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 * 2021.9.9 SkFontMgr on previewer of ohos.
 *           Copyright (c) 2021 Huawei Device Co., Ltd. All rights reserved.
 */

#include "src/ports/SkFontMgr_preview.h"

#include "src/base/SkTSearch.h"
#include "include/private/base/SkTDArray.h"
#include "src/ports/SkFontScanner_FreeType_priv.h"

SkFontMgr_Preview::SkFontMgr_Preview()
{
    SkTDArray<FontFamily*> families;
    SkFontMgr_Config_Parser::GetInstance().GetSystemFontFamilies(families);
    this->buildNameToFamilyMap(families);
    this->findDefaultStyleSet();
    families.reset();
}

int SkFontMgr_Preview::onCountFamilies() const
{
    return fNameToFamilyMap.size();
}

void SkFontMgr_Preview::onGetFamilyName(int index, SkString* familyName) const
{
    if (index < 0 || fNameToFamilyMap.size() <= index) {
        familyName->reset();
        return;
    }
    familyName->set(fNameToFamilyMap[index].name);
}

sk_sp<SkFontStyleSet> SkFontMgr_Preview::onCreateStyleSet(int index) const
{
    if (index < 0 || fNameToFamilyMap.size() <= index) {
        return nullptr;
    }
    return static_cast<sk_sp<SkFontStyleSet>>(SkRef(fNameToFamilyMap[index].styleSet));
}

sk_sp<SkFontStyleSet> SkFontMgr_Preview::onMatchFamily(const char familyName[]) const
{
    if (!familyName) {
        return nullptr;
    }
    SkAutoAsciiToLC tolc(familyName);
    for (int i = 0; i < fNameToFamilyMap.size(); ++i) {
        if (fNameToFamilyMap[i].name.equals(tolc.lc())) {
            return static_cast<sk_sp<SkFontStyleSet>>(SkRef(fNameToFamilyMap[i].styleSet));
        }
    }
    // TODO: eventually we should not need to name fallback families.
    for (int i = 0; i < fFallbackNameToFamilyMap.size(); ++i) {
        if (fFallbackNameToFamilyMap[i].name.equals(tolc.lc())) {
            return static_cast<sk_sp<SkFontStyleSet>>(SkRef(fFallbackNameToFamilyMap[i].styleSet));
        }
    }
    return nullptr;
}

sk_sp<SkTypeface> SkFontMgr_Preview::onMatchFamilyStyle(const char familyName[], const SkFontStyle& style) const
{
    sk_sp<SkFontStyleSet> sset(this->matchFamily(familyName));
    return sset->matchStyle(style);
}

sk_sp<SkTypeface> SkFontMgr_Preview::onMatchFaceStyle(const SkTypeface* typeface, const SkFontStyle& style) const
{
    for (int i = 0; i < fStyleSets.size(); ++i) {
        for (int j = 0; j < fStyleSets[i]->fStyles.size(); ++j) {
            if (fStyleSets[i]->fStyles[j].get() == typeface) {
                return fStyleSets[i]->matchStyle(style);
            }
        }
    }
    return nullptr;
}

sk_sp<SkTypeface_PreviewSystem> SkFontMgr_Preview::find_family_style_character(
    const SkString& familyName,
    const skia_private::TArray<NameToFamily, true>& fallbackNameToFamilyMap,
    const SkFontStyle& style, bool elegant,
    const SkString& langTag, SkUnichar character)
{
    for (int i = 0; i < fallbackNameToFamilyMap.size(); ++i) {
        SkFontStyleSet_Preview* family = fallbackNameToFamilyMap[i].styleSet;
        if (familyName != family->fFallbackFor) {
            continue;
        }
        sk_sp<SkTypeface_PreviewSystem> face(static_cast<SkTypeface_PreviewSystem*>(SkRef(family->matchStyle(style).get())));

        if (!langTag.isEmpty() &&
            std::none_of(face->fLang.begin(), face->fLang.end(), [&](SkLanguage lang) {
                return lang.getTag().startsWith(langTag.c_str()); })) {
            continue;
        }

        if (SkToBool(face->fVariantStyle & kElegant_FontVariant) != elegant) {
            continue;
        }

        if (face->unicharToGlyph(character) != 0) {
            return face;
        }
    }
    return nullptr;
}

sk_sp<SkTypeface> SkFontMgr_Preview::onMatchFamilyStyleCharacter(const char familyName[],
                                                       const SkFontStyle& style,
                                                       const char* bcp47[],
                                                       int bcp47Count,
                                                       SkUnichar character) const
{
    // The variant 'elegant' is 'not squashed', 'compact' is 'stays in ascent/descent'.
    // The variant 'default' means 'compact and elegant'.
    // As a result, it is not possible to know the variant context from the font alone.
    // TODO: add 'is_elegant' and 'is_compact' bits to 'style' request.
    SkString familyNameString(familyName);
    for (const SkString& currentFamilyName : { familyNameString, SkString() }) {
        // The first time match anything elegant, second time anything not elegant.
        for (int elegant = 2; elegant-- > 0;) {
            for (int bcp47Index = bcp47Count; bcp47Index-- > 0;) {
                SkLanguage lang(bcp47[bcp47Index]);
                while (!lang.getTag().isEmpty()) {
                    sk_sp<SkTypeface_PreviewSystem> matchingTypeface =
                        find_family_style_character(currentFamilyName, fFallbackNameToFamilyMap,
                                                    style, SkToBool(elegant),
                                                    lang.getTag(), character);
                    if (matchingTypeface) {
                        return static_cast<sk_sp<SkTypeface>>(matchingTypeface.release());
                    }
                    lang = lang.getParent();
                }
            }
            sk_sp<SkTypeface_PreviewSystem> matchingTypeface =
                find_family_style_character(currentFamilyName, fFallbackNameToFamilyMap,
                                            style, SkToBool(elegant),
                                            SkString(), character);
            if (matchingTypeface) {
                return static_cast<sk_sp<SkTypeface>>(matchingTypeface.release());
            }
        }
    }
    return nullptr;
}

sk_sp<SkTypeface> SkFontMgr_Preview::onMakeFromData(sk_sp<SkData> data, int ttcIndex) const
{
    return this->makeFromStream(std::unique_ptr<SkStreamAsset>(new SkMemoryStream(std::move(data))),
                                ttcIndex);
}

sk_sp<SkTypeface> SkFontMgr_Preview::onMakeFromFile(const char path[], int ttcIndex) const
{
    std::unique_ptr<SkStreamAsset> stream = SkStream::MakeFromFile(path);
    return stream.get() ? this->makeFromStream(std::move(stream), ttcIndex) : nullptr;
}

sk_sp<SkTypeface> SkFontMgr_Preview::onMakeFromStreamIndex(std::unique_ptr<SkStreamAsset> stream, int ttcIndex) const
{
    bool isFixedPitch;
    SkFontStyle style;
    SkString name;
    if (!fScanner.scanFont(stream.get(), ttcIndex, &name, &style, &isFixedPitch, nullptr)) {
        return nullptr;
    }
    auto data = std::make_unique<SkFontData>(std::move(stream), ttcIndex, 0, nullptr, 0, nullptr, 0);
    return sk_sp<SkTypeface>(new SkTypeface_PreviewStream(std::move(data), style, isFixedPitch, name));
}

sk_sp<SkTypeface> SkFontMgr_Preview::onMakeFromStreamArgs(std::unique_ptr<SkStreamAsset> stream,
                                                      const SkFontArguments& args) const
{
    using Scanner = SkFontScanner_FreeType;
    bool isFixedPitch;
    SkFontStyle style;
    SkString name;
    Scanner::AxisDefinitions axisDefinitions;
    if (!fScanner.scanFont(stream.get(), args.getCollectionIndex(),
                           &name, &style, &isFixedPitch, &axisDefinitions)) {
        return nullptr;
    }
    SkAutoSTMalloc<4, SkFixed> axisValues(axisDefinitions.size());
    SkFontScanner::VariationPosition current;
    const SkFontArguments::VariationPosition currentPos{current.data(), current.size()};
    Scanner::computeAxisValues(axisDefinitions, currentPos, args.getVariationDesignPosition(),
                               axisValues, name, &style);
    auto data = std::make_unique<SkFontData>(std::move(stream), args.getCollectionIndex(), 0,
                                             axisValues.get(), axisDefinitions.size(), nullptr, 0);
    return sk_sp<SkTypeface>(new SkTypeface_PreviewStream(std::move(data), style, isFixedPitch, name));
}

sk_sp<SkTypeface> SkFontMgr_Preview::onLegacyMakeTypeface(const char familyName[], SkFontStyle style) const
{
    if (familyName) {
        return sk_sp<SkTypeface>(this->onMatchFamilyStyle(familyName, style));
    }
    return sk_sp<SkTypeface>(fDefaultStyleSet->matchStyle(style));
}

void SkFontMgr_Preview::addFamily(FontFamily& family, int familyIndex)
{
    skia_private::TArray<NameToFamily, true>* nameToFamily = &fNameToFamilyMap;
    if (family.fIsFallbackFont) {
        nameToFamily = &fFallbackNameToFamilyMap;
        if (0 == family.fNames.size()) {
            SkString& fallbackName = family.fNames.push_back();
            fallbackName.printf("%.2x##fallback", familyIndex);
        }
    }
    sk_sp<SkFontStyleSet_Preview> newSet =
        sk_make_sp<SkFontStyleSet_Preview>(family, fScanner);
    if (0 == newSet->count()) {
        return;
    }
    for (const SkString& name : family.fNames) {
        nameToFamily->emplace_back(NameToFamily { name, newSet.get() });
    }
    fStyleSets.emplace_back(std::move(newSet));
}

void SkFontMgr_Preview::buildNameToFamilyMap(SkTDArray<FontFamily*> families)
{
    int familyIndex = 0;
    for (FontFamily* family : families) {
        addFamily(*family, familyIndex++);
        family->fallbackFamilies.foreach([this, &familyIndex]
            (SkString, std::unique_ptr<FontFamily>* fallbackFamily) {
                addFamily(*(*fallbackFamily).get(), familyIndex++);
            }
        );
    }
}

void SkFontMgr_Preview::findDefaultStyleSet()
{
    SkASSERT(!fStyleSets.empty());
    static const char* defaultNames[] = { "sans-serif" };
    for (const char* defaultName : defaultNames) {
        fDefaultStyleSet = this->onMatchFamily(defaultName);
        if (fDefaultStyleSet) {
            break;
        }
    }
    if (nullptr == fDefaultStyleSet) {
        fDefaultStyleSet = fStyleSets[0];
    }
    SkASSERT(fDefaultStyleSet);
}

sk_sp<SkFontMgr> SkFontMgr_New_Preview()
{
    return sk_make_sp<SkFontMgr_Preview>();
}