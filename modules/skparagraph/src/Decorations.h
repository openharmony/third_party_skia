// Copyright 2020 Google LLC.
#ifndef Decorations_DEFINED
#define Decorations_DEFINED

#include "include/core/SkPath.h"
#include "modules/skparagraph/include/ParagraphPainter.h"
#include "modules/skparagraph/include/TextStyle.h"
#include "modules/skparagraph/src/TextLine.h"

#ifdef OHOS_SUPPORT
#include "include/ParagraphStyle.h"
#include "SkScalar.h"
#endif

namespace skia {
namespace textlayout {

class Decorations {
    public:
    void paint(ParagraphPainter* painter, const TextStyle& textStyle, const TextLine::ClipContext& context, SkScalar baseline);
    SkScalar calculateThickness(const TextStyle& textStyle, const TextLine::ClipContext& context);
    void setThickness(SkScalar thickness)
    {
        fThickness = thickness;
    }
    void setDecorationContext(DecorationContext context)
    {
        fDecorationContext = context;
        setThickness(fDecorationContext.thickness);
    }
#ifdef OHOS_SUPPORT
    void setVerticalAlignment(TextVerticalAlign verticalAlignment) { fVerticalAlignment = verticalAlignment; }
    TextVerticalAlign getVerticalAlignment() const { return fVerticalAlignment; }
#endif

    private:
#ifdef OHOS_SUPPORT
    constexpr static float UNDER_LINE_THICKNESS_RATIO = (1.0f / 18.0f);
    constexpr static float LINE_THROUGH_OFFSET = (-6.0f / 21.0f);
    constexpr static float LINE_THROUGH_TOP = LINE_THROUGH_OFFSET - 0.5f * UNDER_LINE_THICKNESS_RATIO;
#endif

#ifndef USE_SKIA_TXT
    void calculateThickness(TextStyle textStyle, sk_sp<SkTypeface> typeface);
#else
    void calculateThickness(TextStyle textStyle, std::shared_ptr<RSTypeface> typeface);
#endif
#ifdef OHOS_SUPPORT
    void updateDecorationPosition(TextDecoration decorationMode, SkScalar baselineShift,
        const TextLine::ClipContext& context, SkScalar& positionY);
    void calculatePosition(TextDecoration decoration, SkScalar ascent, const TextDecorationStyle textDecorationStyle,
        SkScalar textBaselineShift, const SkScalar& fontSize);
#else
    void calculatePosition(TextDecoration decoration, SkScalar ascent, const TextDecorationStyle textDecorationStyle,
        SkScalar textBaselineShift);
#endif
    void calculatePaint(const TextStyle& textStyle);
    void calculateWaves(const TextStyle& textStyle, SkRect clip);
    void calculateAvoidanceWaves(const TextStyle& textStyle, SkRect clip);
    void calculateGaps(const TextLine::ClipContext& context, const SkRect& rect, SkScalar baseline,
        SkScalar halo, const TextStyle& textStyle);

    SkScalar fThickness;
    SkScalar fPosition;
    DecorationContext fDecorationContext;

#ifndef USE_SKIA_TXT
    SkFontMetrics fFontMetrics;
    ParagraphPainter::DecorationStyle fDecorStyle;
    SkPath fPath;
#else
    RSFontMetrics fFontMetrics;
    ParagraphPainter::DecorationStyle fDecorStyle;
    RSPath fPath;
#endif
#ifdef OHOS_SUPPORT
    TextVerticalAlign fVerticalAlignment{TextVerticalAlign::BASELINE};
#endif
};
}  // namespace textlayout
}  // namespace skia
#endif
