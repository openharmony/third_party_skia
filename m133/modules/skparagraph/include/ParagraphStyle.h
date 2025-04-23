// Copyright 2019 Google LLC.
#ifndef ParagraphStyle_DEFINED
#define ParagraphStyle_DEFINED

#include "include/core/SkFontStyle.h"
#include "include/core/SkScalar.h"
#include "include/core/SkString.h"
#include "modules/skparagraph/include/DartTypes.h"
#include "modules/skparagraph/include/TextStyle.h"
#ifdef ENABLE_DRAWING_ADAPTER
#include "drawing.h"
#endif

#include <stddef.h>
#include <algorithm>
#include <limits>
#include <string>
#include <utility>
#include <vector>

namespace skia {
namespace textlayout {

#ifdef ENABLE_TEXT_ENHANCE
struct TextTabs {
    TextAlign alignment;
    SkScalar location;
    bool operator==(const TextTabs& other) const {
        return (alignment == other.alignment) && nearlyEqual(location, other.location);
    }
};

enum class WordBreakType {
    NORMAL,     // to be done.
    BREAK_ALL,  // break occur after any characters.
    BREAK_WORD, // break only occur after word.
    BREAK_HYPHEN,
};

enum class LineBreakStrategy {
    GREEDY,        // faster and linear.
    HIGH_QUALITY,  // breaking tries to split the lines as efficiently as possible
    BALANCED,      // breaking tries to make the line lengths even
};
#endif
struct StrutStyle {
    StrutStyle();

    const std::vector<SkString>& getFontFamilies() const { return fFontFamilies; }
    void setFontFamilies(std::vector<SkString> families) { fFontFamilies = std::move(families); }

#ifndef ENABLE_DRAWING_ADAPTER
    SkFontStyle getFontStyle() const { return fFontStyle; }
    void setFontStyle(SkFontStyle fontStyle) { fFontStyle = fontStyle; }
#else
    RSFontStyle getFontStyle() const { return fFontStyle; }
    void setFontStyle(RSFontStyle fontStyle) { fFontStyle = fontStyle; }
#endif

    SkScalar getFontSize() const { return fFontSize; }
    void setFontSize(SkScalar size) { fFontSize = size; }

    void setHeight(SkScalar height) { fHeight = height; }
    SkScalar getHeight() const { return fHeight; }

    void setLeading(SkScalar Leading) { fLeading = Leading; }
    SkScalar getLeading() const { return fLeading; }

    bool getStrutEnabled() const { return fEnabled; }
    void setStrutEnabled(bool v) { fEnabled = v; }

    bool getForceStrutHeight() const { return fForceHeight; }
    void setForceStrutHeight(bool v) { fForceHeight = v; }

    bool getHeightOverride() const { return fHeightOverride; }
    void setHeightOverride(bool v) { fHeightOverride = v; }

    void setHalfLeading(bool halfLeading) { fHalfLeading = halfLeading; }
    bool getHalfLeading() const { return fHalfLeading; }

#ifdef ENABLE_TEXT_ENHANCE
    void setWordBreakType(const WordBreakType& wordBreakType) { fWordBreakType = wordBreakType; }
    WordBreakType getWordBreakType() const { return fWordBreakType; }

    void setLineBreakStrategy(const LineBreakStrategy& lineBreakStrategy) { fLineBreakStrategy = lineBreakStrategy; }
    LineBreakStrategy getLineBreakStrategy() const { return fLineBreakStrategy; }
#endif

    bool operator==(const StrutStyle& rhs) const {
#ifdef ENABLE_TEXT_ENHANCE
        return this->fEnabled == rhs.fEnabled &&
               this->fHeightOverride == rhs.fHeightOverride &&
               this->fForceHeight == rhs.fForceHeight &&
               this->fHalfLeading == rhs.fHalfLeading &&
               nearlyEqual(this->fLeading, rhs.fLeading) &&
               nearlyEqual(this->fHeight, rhs.fHeight) &&
               nearlyEqual(this->fFontSize, rhs.fFontSize) &&
               this->fFontStyle == rhs.fFontStyle &&
               this->fFontFamilies == rhs.fFontFamilies &&
               this->fWordBreakType == rhs.fWordBreakType &&
               this->fLineBreakStrategy == rhs.fLineBreakStrategy;
#else
        return this->fEnabled == rhs.fEnabled &&
               this->fHeightOverride == rhs.fHeightOverride &&
               this->fForceHeight == rhs.fForceHeight &&
               this->fHalfLeading == rhs.fHalfLeading &&
               nearlyEqual(this->fLeading, rhs.fLeading) &&
               nearlyEqual(this->fHeight, rhs.fHeight) &&
               nearlyEqual(this->fFontSize, rhs.fFontSize) &&
               this->fFontStyle == rhs.fFontStyle &&
               this->fFontFamilies == rhs.fFontFamilies;
#endif
    }

private:

    std::vector<SkString> fFontFamilies;
#ifndef ENABLE_DRAWING_ADAPTER
    SkFontStyle fFontStyle;
#else
    RSFontStyle fFontStyle;
#endif
    SkScalar fFontSize;
    SkScalar fHeight;
    SkScalar fLeading;
    bool fForceHeight;
    bool fEnabled;
    bool fHeightOverride;
    // true: half leading.
    // false: scale ascent/descent with fHeight.
    bool fHalfLeading;
#ifdef ENABLE_TEXT_ENHANCE
    WordBreakType fWordBreakType;
    LineBreakStrategy fLineBreakStrategy { LineBreakStrategy::GREEDY };
#endif
};

struct ParagraphStyle {
    ParagraphStyle();

    bool operator==(const ParagraphStyle& rhs) const {
#ifdef ENABLE_TEXT_ENHANCE
        return this->fHeight == rhs.fHeight &&
               this->fEllipsis == rhs.fEllipsis &&
               this->fEllipsisUtf16 == rhs.fEllipsisUtf16 &&
               this->fTextDirection == rhs.fTextDirection && this->fTextAlign == rhs.fTextAlign &&
               this->fDefaultTextStyle == rhs.fDefaultTextStyle &&
               this->fEllipsisModal == rhs.fEllipsisModal &&
               this->fTextOverflower == rhs.fTextOverflower &&
               this->fReplaceTabCharacters == rhs.fReplaceTabCharacters &&
               this->fTextTab == rhs.fTextTab &&
               this->fParagraphSpacing == rhs.fParagraphSpacing &&
               this->fIsEndAddParagraphSpacing == rhs.fIsEndAddParagraphSpacing &&
               nearlyEqual(this->fTextSplitRatio, rhs.fTextSplitRatio);
#else
        return this->fHeight == rhs.fHeight &&
               this->fEllipsis == rhs.fEllipsis &&
               this->fEllipsisUtf16 == rhs.fEllipsisUtf16 &&
               this->fTextDirection == rhs.fTextDirection && this->fTextAlign == rhs.fTextAlign &&
               this->fDefaultTextStyle == rhs.fDefaultTextStyle &&
               this->fReplaceTabCharacters == rhs.fReplaceTabCharacters;
#endif
    }

#ifdef ENABLE_TEXT_ENHANCE
    StrutStyle& exportStrutStyle() { return fStrutStyle; }
    TextStyle& exportTextStyle() { return fDefaultTextStyle; }
#endif

    const StrutStyle& getStrutStyle() const { return fStrutStyle; }
    void setStrutStyle(StrutStyle strutStyle) { fStrutStyle = std::move(strutStyle); }

    const TextStyle& getTextStyle() const { return fDefaultTextStyle; }
    void setTextStyle(const TextStyle& textStyle) { fDefaultTextStyle = textStyle; }

    TextDirection getTextDirection() const { return fTextDirection; }
    void setTextDirection(TextDirection direction) { fTextDirection = direction; }

    TextAlign getTextAlign() const { return fTextAlign; }
    void setTextAlign(TextAlign align) { fTextAlign = align; }

    size_t getMaxLines() const { return fLinesLimit; }
    void setMaxLines(size_t maxLines) { fLinesLimit = maxLines; }

    SkString getEllipsis() const { return fEllipsis; }
    std::u16string getEllipsisUtf16() const { return fEllipsisUtf16; }
    void setEllipsis(const std::u16string& ellipsis) {  fEllipsisUtf16 = ellipsis; }
    void setEllipsis(const SkString& ellipsis) { fEllipsis = ellipsis; }

    SkScalar getHeight() const { return fHeight; }
    void setHeight(SkScalar height) { fHeight = height; }

    TextHeightBehavior getTextHeightBehavior() const { return fTextHeightBehavior; }
    void setTextHeightBehavior(TextHeightBehavior v) { fTextHeightBehavior = v; }

    bool unlimited_lines() const {
        return fLinesLimit == std::numeric_limits<size_t>::max();
    }
    bool ellipsized() const { return !fEllipsis.isEmpty() || !fEllipsisUtf16.empty(); }
    TextAlign effective_align() const;
    bool hintingIsOn() const { return fHintingIsOn; }
    void turnHintingOff() { fHintingIsOn = false; }

    bool getReplaceTabCharacters() const { return fReplaceTabCharacters; }
    void setReplaceTabCharacters(bool value) { fReplaceTabCharacters = value; }

#ifdef ENABLE_TEXT_ENHANCE
    skia::textlayout::EllipsisModal getEllipsisMod() const { return fEllipsisModal; }
    void setEllipsisMod(skia::textlayout::EllipsisModal ellipsisModel) { fEllipsisModal = ellipsisModel; }
    SkScalar getTextSplitRatio() const { return fTextSplitRatio; }
    void setTextSplitRatio(SkScalar textSplitRatio) { fTextSplitRatio = textSplitRatio; }

    bool getTextOverflower() const { return fTextOverflower; }
    void setTextOverflower(bool textOverflowerFlag) { fTextOverflower = textOverflowerFlag; }
    const TextTabs& getTextTab() const { return fTextTab; }
    void setTextTab(const TextTabs& textTab) { fTextTab = textTab; }
    SkScalar getParagraphSpacing() const { return fParagraphSpacing; }
    void setParagraphSpacing(SkScalar paragraphSpacing)
    {
        fParagraphSpacing = paragraphSpacing;
    }
    bool getIsEndAddParagraphSpacing() const { return fIsEndAddParagraphSpacing; }
    void setIsEndAddParagraphSpacing(bool isEndAddParagraphSpacing)
    {
        fIsEndAddParagraphSpacing = isEndAddParagraphSpacing;
    }
#endif
    bool getApplyRoundingHack() const { return fApplyRoundingHack; }
    void setApplyRoundingHack(bool value) { fApplyRoundingHack = value; }

private:
    StrutStyle fStrutStyle;
    TextStyle fDefaultTextStyle;
    TextAlign fTextAlign;
    TextDirection fTextDirection;
    size_t fLinesLimit;
    std::u16string fEllipsisUtf16;
    SkString fEllipsis;
    SkScalar fHeight;
    TextHeightBehavior fTextHeightBehavior;
    bool fHintingIsOn;
    bool fReplaceTabCharacters;
#ifdef ENABLE_TEXT_ENHANCE
    bool fTextOverflower;
    skia::textlayout::EllipsisModal fEllipsisModal;
    SkScalar fTextSplitRatio = 0.5f;
    TextTabs fTextTab;
    SkScalar fParagraphSpacing { 0.0f };
    bool fIsEndAddParagraphSpacing { false };
#endif
    bool fApplyRoundingHack = true;
};
}  // namespace textlayout
}  // namespace skia

#endif  // ParagraphStyle_DEFINED
