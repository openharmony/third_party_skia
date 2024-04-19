/*
 * Copyright (c) 2024 Huawei Device Co., Ltd.. All rights reserved.
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "modules/skparagraph/src/RunBaseImpl.h"

namespace skia {
namespace textlayout {
RunBaseImpl::RunBaseImpl(
#ifndef USE_SKIA_TXT
    sk_sp<SkTextBlob> blob,
#else
    std::shared_ptr<RSTextBlob> blob,
#endif
    SkPoint offset,
    ParagraphPainter::SkPaintOrID paint,
    bool clippingNeeded,
    SkRect clipRect,
    const Run* visitorRun,
    size_t visitorPos,
    size_t visitorSize)
    : fBlob(blob),
    fOffset(offset),
    fClippingNeeded(clippingNeeded),
    fClipRect(clipRect),
    fVisitorRun(visitorRun),
    fVisitorPos(visitorPos),
    fVisitorSize(visitorSize)
{
    if (std::holds_alternative<SkPaint>(paint)) {
        fPaint = std::get<SkPaint>(paint);
    } else if (std::holds_alternative<ParagraphPainter::PaintID>(paint)) {
        fPaint = std::get<ParagraphPainter::PaintID>(paint);
    }

}

#ifndef USE_SKIA_TXT
const SkFont& RunBaseImpl::font() const
#else
const RSFont& RunBaseImpl::font() const
#endif
{
    if (!fVisitorRun) {
        return {};
    }
    return fVisitorRun->font();
}

size_t RunBaseImpl::size() const
{
    return fVisitorSize;
}

std::vector<uint16_t> RunBaseImpl::getGlyphs() const
{
    if (!fVisitorRun) {
        return {};
    }
    SkSpan<const SkGlyphID> glyphIDSpan = fVisitorRun->glyphs();
    SkSpan<const SkGlyphID> runGlyphIDSpan = glyphIDSpan.subspan(fVisitorPos, fVisitorSize);
    return std::vector<uint16_t>(runGlyphIDSpan.begin(), runGlyphIDSpan.end());
}

std::vector<RSPoint> RunBaseImpl::getPositions() const
{
    if (!fVisitorRun) {
        return {};
    }
    SkSpan<const SkPoint> positionSpan = fVisitorRun->positions();
    SkSpan<const SkPoint> runPositionSpan = positionSpan.subspan(fVisitorPos, fVisitorSize);
    std::vector<RSPoint> positions;
    for (size_t i = 0; i < runPositionSpan.size(); i++) {
        RSPoint point(runPositionSpan[i].fX, runPositionSpan[i].fY);
        positions.emplace_back(point);
    }

    return positions;

}

std::vector<RSPoint> RunBaseImpl::getOffsets() const
{
    if (!fVisitorRun) {
        return {};
    }
    SkSpan<const SkPoint> offsetSpan = fVisitorRun->offsets();
    SkSpan<const SkPoint> runOffsetSpan = offsetSpan.subspan(fVisitorPos, fVisitorSize);
    std::vector<RSPoint> offsets;
    for (size_t i = 0; i < runOffsetSpan.size(); i++) {
        RSPoint point(runOffsetSpan[i].fX, runOffsetSpan[i].fY);
        offsets.emplace_back(point);
    }

    return offsets;

}

void RunBaseImpl::paint(ParagraphPainter* painter, SkScalar x, SkScalar y)
{
    if (!painter) {
        return;
    }
    if (fClippingNeeded) {
        painter->save();
        painter->clipRect(fClipRect.makeOffset(x, y));
    }
    painter->drawTextBlob(fBlob, x + fOffset.x(), y + fOffset.y(), fPaint);
    if (fClippingNeeded) {
        painter->restore();
    }
}

size_t RunBaseImpl::getVisitorPos() const
{
    return fVisitorPos;
}

size_t RunBaseImpl::getVisitorSize() const
{
    return fVisitorSize;
}

}  // namespace textlayout
}  // namespace skia