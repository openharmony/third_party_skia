// Copyright 2019 Google LLC.
#include "include/core/SkColor.h"
#include "include/core/SkFontStyle.h"
#include "modules/skparagraph/include/TextStyle.h"
#ifdef ENABLE_TEXT_ENHANCE
#include "modules/skparagraph/src/Run.h"
#endif

namespace skia {
namespace textlayout {

#ifdef ENABLE_TEXT_ENHANCE
struct SkStringHash {
    size_t operator()(const SkString& s) const {
        size_t hash = 0;
        for (size_t i = 0; i < s.size(); ++i) {
            hash ^= std::hash<char>()(s.c_str()[i]);
        }
        return hash;
    }
};

const std::unordered_map<SkString, SkString, SkStringHash> GENERIC_FAMILY_NAME_MAP = {
    { SkString{"HarmonyOS Sans"}, SkString{"HarmonyOS-Sans"} },
    { SkString{"HarmonyOS Sans Condensed"}, SkString{"HarmonyOS-Sans-Condensed"} },
    { SkString{"HarmonyOS Sans Digit"}, SkString{"HarmonyOS-Sans-Digit"} },
    { SkString{"Noto Serif"}, SkString{"serif"} },
    { SkString{"Noto Sans Mono"}, SkString{"monospace"} }
};
#endif

const std::vector<SkString>* TextStyle::kDefaultFontFamilies =
        new std::vector<SkString>{SkString(DEFAULT_FONT_FAMILY)};

TextStyle TextStyle::cloneForPlaceholder() {
    TextStyle result;
    result.fColor = fColor;
    result.fFontSize = fFontSize;
    result.fFontFamilies = fFontFamilies;
    result.fDecoration = fDecoration;
    result.fHasBackground = fHasBackground;
    result.fHasForeground = fHasForeground;
    result.fBackground = fBackground;
    result.fForeground = fForeground;
    result.fHeightOverride = fHeightOverride;
    result.fIsPlaceholder = true;
    result.fFontFeatures = fFontFeatures;
    result.fHalfLeading = fHalfLeading;
    result.fBaselineShift = fBaselineShift;
    result.fFontArguments = fFontArguments;
#ifdef ENABLE_TEXT_ENHANCE
    result.fBackgroundRect = fBackgroundRect;
    result.fStyleId = fStyleId;
    result.fTextStyleUid = fTextStyleUid;
#endif
    return result;
}

#ifdef ENABLE_TEXT_ENHANCE
bool TextStyle::equalsByTextShadow(const TextStyle& other) const {
    if (fTextShadows.size() != other.fTextShadows.size()) {
        return false;
    }
    for (size_t i = 0; i < fTextShadows.size(); ++i) {
        if (fTextShadows[i] != other.fTextShadows[i]) {
            return false;
        }
    }
    return true;
}

bool TextStyle::equalsByFontFeatures(const TextStyle& other) const {
    if (fFontFeatures.size() != other.fFontFeatures.size()) {
        return false;
    }
    for (size_t i = 0; i < fFontFeatures.size(); ++i) {
        if (!(fFontFeatures[i] == other.fFontFeatures[i])) {
            return false;
        }
    }
    return true;
}

bool TextStyle::equalsByShape(const TextStyle& other) const {
    return fFontStyle == other.fFontStyle &&
           fLocale == other.fLocale &&
           fFontFamilies == other.fFontFamilies &&
           getCorrectFontSize() == other.getCorrectFontSize() &&
           fHeightOverride == other.fHeightOverride &&
           fHeight == other.fHeight &&
           fHalfLeading == other.fHalfLeading &&
           nearlyEqual(getTotalVerticalShift(), other.getTotalVerticalShift()) &&
           fFontArguments == other.fFontArguments &&
           fStyleId == other.fStyleId &&
           fBackgroundRect == other.fBackgroundRect &&
           nearlyEqual(fBaselineShift, other.fBaselineShift) &&
           nearlyEqual(fMaxLineHeight, other.fMaxLineHeight) &&
           nearlyEqual(fMinLineHeight, other.fMinLineHeight) &&
           fLineHeightStyle == other.fLineHeightStyle &&
           fBadgeType == other.fBadgeType;
}

bool TextStyle::equals(const TextStyle& other) const {
    if (fIsPlaceholder || other.fIsPlaceholder) {
        return false;
    }

    return fColor == other.fColor &&
           fDecoration == other.fDecoration &&
           nearlyEqual(fLetterSpacing, other.fLetterSpacing) &&
           nearlyEqual(fWordSpacing, other.fWordSpacing) &&
           fHasForeground == other.fHasForeground &&
           fForeground == other.fForeground &&
           fHasBackground == other.fHasBackground &&
           fBackground == other.fBackground &&
           equalsByFontFeatures(other) &&
           equalsByTextShadow(other) &&
           equalsByShape(other);
}
#else
bool TextStyle::equals(const TextStyle& other) const {

    if (fIsPlaceholder || other.fIsPlaceholder) {
        return false;
    }

    if (fColor != other.fColor) {
        return false;
    }
    if (!(fDecoration == other.fDecoration)) {
        return false;
    }
    if (!(fFontStyle == other.fFontStyle)) {
        return false;
    }
    if (fFontFamilies != other.fFontFamilies) {
        return false;
    }
    if (fLetterSpacing != other.fLetterSpacing) {
        return false;
    }
    if (fWordSpacing != other.fWordSpacing) {
        return false;
    }
    if (fHeight != other.fHeight) {
        return false;
    }
    if (fHeightOverride != other.fHeightOverride) {
        return false;
    }
    if (fHalfLeading != other.fHalfLeading) {
        return false;
    }
    if (fBaselineShift != other.fBaselineShift) {
        return false;
    }
    if (fFontSize != other.fFontSize) {
        return false;
    }
    if (fLocale != other.fLocale) {
        return false;
    }
    if (fHasForeground != other.fHasForeground || fForeground != other.fForeground) {
        return false;
    }
    if (fHasBackground != other.fHasBackground || fBackground != other.fBackground) {
        return false;
    }
    if (fTextShadows.size() != other.fTextShadows.size()) {
        return false;
    }
    for (size_t i = 0; i < fTextShadows.size(); ++i) {
        if (fTextShadows[i] != other.fTextShadows[i]) {
            return false;
        }
    }
    if (fFontFeatures.size() != other.fFontFeatures.size()) {
        return false;
    }
    for (size_t i = 0; i < fFontFeatures.size(); ++i) {
        if (!(fFontFeatures[i] == other.fFontFeatures[i])) {
            return false;
        }
    }
    if (fFontArguments != other.fFontArguments) {
        return false;
    }
    return true;
}
#endif

bool TextStyle::equalsByFonts(const TextStyle& that) const {

    return !fIsPlaceholder && !that.fIsPlaceholder &&
           fFontStyle == that.fFontStyle &&
           fFontFamilies == that.fFontFamilies &&
           fFontFeatures == that.fFontFeatures &&
           fFontArguments == that.getFontArguments() &&
           nearlyEqual(fLetterSpacing, that.fLetterSpacing) &&
           nearlyEqual(fWordSpacing, that.fWordSpacing) &&
           nearlyEqual(fHeight, that.fHeight) &&
           nearlyEqual(fBaselineShift, that.fBaselineShift) &&
           nearlyEqual(fFontSize, that.fFontSize) &&
#ifdef ENABLE_TEXT_ENHANCE
           fLocale == that.fLocale &&
           fStyleId == that.fStyleId &&
           fBackgroundRect == that.fBackgroundRect;
#else
           fLocale == that.fLocale;
#endif
}

#ifdef ENABLE_TEXT_ENHANCE
bool TextStyle::matchOneAttribute(StyleType styleType, const TextStyle& other) const {
    switch (styleType) {
        case kForeground:
            return (!fHasForeground && !other.fHasForeground && fColor == other.fColor) ||
                   (fHasForeground &&  other.fHasForeground && fForeground == other.fForeground);
        case kBackground:
            return (fBackgroundRect == other.fBackgroundRect) &&
                ((!fHasBackground && !other.fHasBackground) ||
                (fHasBackground &&  other.fHasBackground && fBackground == other.fBackground));
        case kShadow:
            return equalsByTextShadow(other);
        case kDecorations:
            return this->fDecoration == other.fDecoration;
        case kLetterSpacing:
            return fLetterSpacing == other.fLetterSpacing;
        case kWordSpacing:
            return fWordSpacing == other.fWordSpacing;
        case kAllAttributes:
            return this->equals(other);
        case kFont:
            return equalsByShape(other);
        default:
            SkASSERT(false);
            return false;
    }
}
#else
bool TextStyle::matchOneAttribute(StyleType styleType, const TextStyle& other) const {
    switch (styleType) {
        case kForeground:
            return (!fHasForeground && !other.fHasForeground && fColor == other.fColor) ||
                   ( fHasForeground &&  other.fHasForeground && fForeground == other.fForeground);

        case kBackground:
            return (!fHasBackground && !other.fHasBackground) ||
                   ( fHasBackground &&  other.fHasBackground && fBackground == other.fBackground);

        case kShadow:
            if (fTextShadows.size() != other.fTextShadows.size()) {
                return false;
            }

            for (int32_t i = 0; i < SkToInt(fTextShadows.size()); ++i) {
                if (fTextShadows[i] != other.fTextShadows[i]) {
                    return false;
                }
            }
            return true;

        case kDecorations:
            return this->fDecoration == other.fDecoration;

        case kLetterSpacing:
            return fLetterSpacing == other.fLetterSpacing;

        case kWordSpacing:
            return fWordSpacing == other.fWordSpacing;

        case kAllAttributes:
            return this->equals(other);

        case kFont:
            // TODO: should not we take typefaces in account?
            return fFontStyle == other.fFontStyle &&
                   fLocale == other.fLocale &&
                   fFontFamilies == other.fFontFamilies &&
                   fFontSize == other.fFontSize &&
                   fHeight == other.fHeight &&
                   fHalfLeading == other.fHalfLeading &&
                   fBaselineShift == other.fBaselineShift &&
                   fFontArguments == other.fFontArguments;
        default:
            SkASSERT(false);
            return false;
    }
}
#endif

#ifdef ENABLE_TEXT_ENHANCE
void TextStyle::getFontMetrics(RSFontMetrics* metrics) const {
#else
void TextStyle::getFontMetrics(SkFontMetrics* metrics) const {
#endif
#ifdef ENABLE_TEXT_ENHANCE
    RSFont font(fTypeface, fFontSize, 1, 0);
    font.SetEdging(RSDrawing::FontEdging::ANTI_ALIAS);
    font.SetHinting(RSDrawing::FontHinting::SLIGHT);
    font.SetSubpixel(true);
    auto compressFont = font;
    scaleFontWithCompressionConfig(compressFont, ScaleOP::COMPRESS);
    compressFont.GetMetrics(metrics);
    metricsIncludeFontPadding(metrics, font);
#else
    SkFont font(fTypeface, fFontSize);
    font.setEdging(SkFont::Edging::kAntiAlias);
    font.setSubpixel(true);
    font.setHinting(SkFontHinting::kSlight);
    font.getMetrics(metrics);
#endif
    if (fHeightOverride) {
        auto multiplier = fHeight * fFontSize;
        auto height = metrics->fDescent - metrics->fAscent + metrics->fLeading;
        metrics->fAscent = (metrics->fAscent - metrics->fLeading / 2) * multiplier / height;
        metrics->fDescent = (metrics->fDescent + metrics->fLeading / 2) * multiplier / height;

    } else {
        metrics->fAscent = (metrics->fAscent - metrics->fLeading / 2);
        metrics->fDescent = (metrics->fDescent + metrics->fLeading / 2);
    }
    // If we shift the baseline we need to make sure the shifted text fits the line
    metrics->fAscent += fBaselineShift;
    metrics->fDescent += fBaselineShift;
}

void TextStyle::setFontArguments(const std::optional<SkFontArguments>& args) {
    if (!args) {
        fFontArguments.reset();
        return;
    }

    fFontArguments.emplace(*args);
}

bool PlaceholderStyle::equals(const PlaceholderStyle& other) const {
    return nearlyEqual(fWidth, other.fWidth) &&
           nearlyEqual(fHeight, other.fHeight) &&
           fAlignment == other.fAlignment &&
           fBaseline == other.fBaseline &&
           (fAlignment != PlaceholderAlignment::kBaseline ||
            nearlyEqual(fBaselineOffset, other.fBaselineOffset));
}

#ifdef ENABLE_TEXT_ENHANCE
void TextStyle::setFontFamilies(std::vector<SkString> families) {
    std::for_each(families.begin(), families.end(), [](SkString& familyName) {
        if (GENERIC_FAMILY_NAME_MAP.count(familyName)) {
            familyName = GENERIC_FAMILY_NAME_MAP.at(familyName);
        }
    });
    fFontFamilies = std::move(families);
}

SkScalar TextStyle::getBadgeBaseLineShift() const {
    if (getTextBadgeType() == TextBadgeType::BADGE_NONE) {
        return 0.0;
    }

    SkScalar actualFontSize = getFontSize() * TEXT_BADGE_FONT_SIZE_SCALE;
    return getTextBadgeType() == TextBadgeType::SUPERSCRIPT ? actualFontSize * SUPERSCRIPT_BASELINE_SHIFT_SCALE :
        actualFontSize * SUBSCRIPT_BASELINE_SHIFT_SCALE;
}

SkScalar TextStyle::getCorrectFontSize() const {
    if (getTextBadgeType() == TextBadgeType::BADGE_NONE) {
        return getFontSize();
    }

    return getFontSize() * TEXT_BADGE_FONT_SIZE_SCALE;
};
#endif

}  // namespace textlayout
}  // namespace skia
