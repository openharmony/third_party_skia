// Copyright 2020 Google LLC.
#ifndef Decorations_DEFINED
#define Decorations_DEFINED

#include "include/core/SkPath.h"
#include "modules/skparagraph/include/ParagraphPainter.h"
#include "modules/skparagraph/include/TextStyle.h"
#include "modules/skparagraph/src/TextLine.h"

namespace skia {
namespace textlayout {

class Decorations {
    public:
    void paint(ParagraphPainter* painter, const TextStyle& textStyle, const TextLine::ClipContext& context, SkScalar baseline);
#ifdef ENABLE_DRAWING_ADAPTER
    SkScalar calculateThickness(const TextStyle& textStyle, const TextLine::ClipContext& context);
#endif
#ifdef ENABLE_TEXT_ENHANCE
    void setThickness(SkScalar thickness)
    {
        fThickness = thickness;
    }
    void setDecorationContext(DecorationContext context)
    {
        fDecorationContext = context;
        setThickness(fDecorationContext.thickness);
    }
#endif

    private:
#ifdef ENABLE_TEXT_ENHANCE
    constexpr static float UNDER_LINE_THICKNESS_RATIO = (1.0f / 18.0f);
    constexpr static float LINE_THROUGH_OFFSET = (-6.0f / 21.0f);
    constexpr static float LINE_THROUGH_TOP = LINE_THROUGH_OFFSET - 0.5f * UNDER_LINE_THICKNESS_RATIO;
#endif

#ifdef ENABLE_DRAWING_ADAPTER
    void calculateThickness(TextStyle textStyle, std::shared_ptr<RSTypeface> typeface);
#else
    void calculateThickness(TextStyle textStyle, sk_sp<SkTypeface> typeface);
#endif

#ifdef ENABLE_TEXT_ENHANCE
    void calculatePosition(TextDecoration decoration, SkScalar ascent, const TextDecorationStyle textDecorationStyle,
        SkScalar textBaselineShift, const SkScalar& fontSize);
#else
    void calculatePosition(TextDecoration decoration, SkScalar ascent);
#endif
    void calculatePaint(const TextStyle& textStyle);
    void calculateWaves(const TextStyle& textStyle, SkRect clip);

#ifdef ENABLE_TEXT_ENHANCE
    void calculateAvoidanceWaves(const TextStyle& textStyle, SkRect clip);
#endif

#ifdef ENABLE_DRAWING_ADAPTER
    void calculateGaps(const TextLine::ClipContext& context, const SkRect& rect, SkScalar baseline,
        SkScalar halo, const TextStyle& textStyle);
#else
    void calculateGaps(const TextLine::ClipContext& context, const SkRect& rect, SkScalar baseline, SkScalar halo);
#endif

    SkScalar fThickness;
    SkScalar fPosition;
#ifdef ENABLE_TEXT_ENHANCE
    DecorationContext fDecorationContext;
#endif

#ifdef ENABLE_DRAWING_ADAPTER
    RSFontMetrics fFontMetrics;
    ParagraphPainter::DecorationStyle fDecorStyle;
    RSPath fPath;
#else
    SkFontMetrics fFontMetrics;
    ParagraphPainter::DecorationStyle fDecorStyle;
    SkPath fPath;
#endif
};
}  // namespace textlayout
}  // namespace skia
#endif
