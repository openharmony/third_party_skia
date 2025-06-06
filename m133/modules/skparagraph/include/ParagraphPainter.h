// Copyright 2019 Google LLC.
#ifndef ParagraphPainter_DEFINED
#define ParagraphPainter_DEFINED

#include "include/core/SkPaint.h"
#include "include/core/SkTextBlob.h"
#ifdef ENABLE_TEXT_ENHANCE
#include "include/core/SkRRect.h"
#include "drawing.h"
#endif

#include <optional>
#include <variant>

namespace skia {
namespace textlayout {

class ParagraphPainter {
public:
    typedef int PaintID;
    typedef std::variant<SkPaint, PaintID> SkPaintOrID;

    struct DashPathEffect {
        DashPathEffect(SkScalar onLength, SkScalar offLength);

        SkScalar fOnLength;
        SkScalar fOffLength;
    };

    class DecorationStyle {
    public:
        DecorationStyle();
        DecorationStyle(SkColor color, SkScalar strokeWidth,
                        std::optional<DashPathEffect> dashPathEffect);

        SkColor getColor() const { return fColor; }
        SkScalar getStrokeWidth() const { return fStrokeWidth; }
        std::optional<DashPathEffect> getDashPathEffect() const { return fDashPathEffect; }
        const SkPaint& skPaint() const { return fPaint; }

    private:
        SkColor fColor;
        SkScalar fStrokeWidth;
        std::optional<DashPathEffect> fDashPathEffect;
        SkPaint fPaint;
    };

    virtual ~ParagraphPainter() = default;

#ifdef ENABLE_TEXT_ENHANCE
    virtual void drawTextBlob(
        const std::shared_ptr<RSTextBlob>& blob, SkScalar x, SkScalar y, const SkPaintOrID& paint) = 0;
    virtual void drawTextShadow(
        const std::shared_ptr<RSTextBlob>& blob, SkScalar x, SkScalar y, SkColor color, SkScalar blurSigma) = 0;
    virtual void drawRRect(const SkRRect& rrect, const SkColor color) = 0;
    virtual void drawPath(const RSPath& path, const DecorationStyle& decorStyle) = 0;
#else
    virtual void drawTextBlob(const sk_sp<SkTextBlob>& blob, SkScalar x, SkScalar y, const SkPaintOrID& paint) = 0;
    virtual void drawTextShadow(const sk_sp<SkTextBlob>& blob, SkScalar x, SkScalar y, SkColor color, SkScalar blurSigma) = 0;
    virtual void drawPath(const SkPath& path, const DecorationStyle& decorStyle) = 0;
#endif
    virtual void drawRect(const SkRect& rect, const SkPaintOrID& paint) = 0;
    virtual void drawFilledRect(const SkRect& rect, const DecorationStyle& decorStyle) = 0;
    virtual void drawLine(SkScalar x0, SkScalar y0, SkScalar x1, SkScalar y1, const DecorationStyle& decorStyle) = 0;

    virtual void clipRect(const SkRect& rect) = 0;
    virtual void translate(SkScalar dx, SkScalar dy) = 0;

    virtual void save() = 0;
    virtual void restore() = 0;
};

}  // namespace textlayout
}  // namespace skia

#endif  // ParagraphPainter_DEFINED
