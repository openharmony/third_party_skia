// Copyright 2020 Google LLC.
#ifndef Decorations_DEFINED
#define Decorations_DEFINED

#include "include/core/SkPath.h"
#include "modules/skparagraph/include/ParagraphPainter.h"
#include "modules/skparagraph/include/TextStyle.h"
#include "modules/skparagraph/src/TextLine.h"

#ifdef ENABLE_TEXT_ENHANCE
#include "include/ParagraphStyle.h"
#include "SkScalar.h"
#endif

namespace skia {
namespace textlayout {

class Decorations {
    public:
    void paint(ParagraphPainter* painter, const TextStyle& textStyle, const TextLine::ClipContext& context, SkScalar baseline);
#ifdef ENABLE_TEXT_ENHANCE
    SkScalar calculateThickness(const TextStyle& textStyle, const TextLine::ClipContext& context);
    void setThickness(SkScalar thickness) {
        fThickness = thickness;
    }
    void setDecorationContext(DecorationContext context) {
        fDecorationContext = context;
        setThickness(fDecorationContext.thickness);
    }

    void setVerticalAlignment(TextVerticalAlign verticalAlignment) { fVerticalAlignment = verticalAlignment; }
    TextVerticalAlign getVerticalAlignment() const { return fVerticalAlignment; }
#endif

    private:
#ifdef ENABLE_TEXT_ENHANCE
    constexpr static float UNDER_LINE_THICKNESS_RATIO = (1.0f / 18.0f);
    constexpr static float LINE_THROUGH_OFFSET = (-6.0f / 21.0f);
    constexpr static float LINE_THROUGH_TOP = LINE_THROUGH_OFFSET - 0.5f * UNDER_LINE_THICKNESS_RATIO;

    void calculateThickness(TextStyle textStyle, std::shared_ptr<RSTypeface> typeface);
    void updateDecorationPosition(TextDecoration decorationMode, SkScalar baselineShift,
        const TextLine::ClipContext& context, SkScalar& positionY);
    void calculatePosition(TextDecoration decoration, SkScalar ascent, const TextStyle& textStyle,
        SkScalar textBaselineShift, const Run& run);
    void calculateAvoidanceWaves(const TextStyle& textStyle, SkRect clip);
    void calculateGaps(const TextLine::ClipContext& context, const SkRect& rect, SkScalar baseline,
        SkScalar halo, const TextStyle& textStyle);

    DecorationContext fDecorationContext;
    RSFontMetrics fFontMetrics;
    ParagraphPainter::DecorationStyle fDecorStyle;
    RSPath fPath;
#else
    void calculateThickness(TextStyle textStyle, sk_sp<SkTypeface> typeface);
    void calculatePosition(TextDecoration decoration, SkScalar ascent);
    void calculateGaps(const TextLine::ClipContext& context, const SkRect& rect, SkScalar baseline, SkScalar halo);
    SkFontMetrics fFontMetrics;
    ParagraphPainter::DecorationStyle fDecorStyle;
    SkPath fPath;
#endif

    void calculatePaint(const TextStyle& textStyle);
    void calculateWaves(const TextStyle& textStyle, SkRect clip);

    SkScalar fThickness;
    SkScalar fPosition;
#ifdef ENABLE_TEXT_ENHANCE
    TextVerticalAlign fVerticalAlignment{TextVerticalAlign::BASELINE};
#endif
};
}  // namespace textlayout
}  // namespace skia
#endif
