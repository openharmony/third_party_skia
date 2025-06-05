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

#ifdef OHOS_SUPPORT
#include "ParagraphImpl.h"
#endif
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
#ifdef OHOS_SUPPORT
    size_t visitorGlobalPos,
    size_t trailSpaces,
#endif
    size_t visitorSize)
    : fBlob(blob),
    fOffset(offset),
    fClippingNeeded(clippingNeeded),
    fClipRect(clipRect),
    fVisitorRun(visitorRun),
    fVisitorPos(visitorPos),
#ifdef OHOS_SUPPORT
    fVisitorGlobalPos(visitorGlobalPos),
    fTrailSpaces(trailSpaces),
#endif
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
        return fFont;
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

#ifdef OHOS_SUPPORT
std::vector<uint16_t> RunBaseImpl::getGlyphs(int64_t start, int64_t length) const
{
    if (!fVisitorRun) {
        return {};
    }
    uint64_t actualLength = calculateActualLength(start, length);
    if (actualLength == 0) {
        return {};
    }
    SkSpan<const SkGlyphID> glyphIdSpan = fVisitorRun->glyphs();
    SkSpan<const SkGlyphID> runGlyphIdSpan = glyphIdSpan.subspan(fVisitorPos + start, actualLength);
    std::vector<uint16_t> glyphs;
    for (size_t i = 0; i < runGlyphIdSpan.size(); i++) {
        glyphs.emplace_back(runGlyphIdSpan[i]);
    }

    return glyphs;
}

#ifndef USE_SKIA_TXT
std::vector<SkPoint> RunBaseImpl::getPositions(int64_t start, int64_t length) const
#else
std::vector<RSPoint> RunBaseImpl::getPositions(int64_t start, int64_t length) const
#endif
{
    if (!fVisitorRun) {
        return {};
    }
    uint64_t actualLength = calculateActualLength(start, length);
    if (actualLength == 0) {
        return {};
    }
    SkSpan<const SkPoint> positionSpan = fVisitorRun->positions();
    SkSpan<const SkPoint> runPositionSpan = positionSpan.subspan(fVisitorPos + start, actualLength);
#ifndef USE_SKIA_TXT
    std::vector<SkPoint> positions;
#else
    std::vector<RSPoint> positions;
#endif
    for (size_t i = 0; i < runPositionSpan.size(); i++) {
#ifndef USE_SKIA_TXT
        positions.emplace_back(SkPoint::Make(runPositionSpan[i].fX, runPositionSpan[i].fY));
#else
        positions.emplace_back(runPositionSpan[i].fX, runPositionSpan[i].fY);
#endif
    }

    return positions;
}

std::vector<RSPoint> RunBaseImpl::getAdvances(uint32_t start, uint32_t length) const
{
    if (fVisitorRun == nullptr) {
        return {};
    }
    uint64_t actualLength = calculateActualLength(start, length);
    if (actualLength == 0) {
        return {};
    }
    SkSpan<const SkPoint> advanceSpan = fVisitorRun->advances();
    SkSpan<const SkPoint> runAdvancesSpan = advanceSpan.subspan(fVisitorPos + start, actualLength);
    std::vector<RSPoint> advances;
    for (size_t i = 0; i < runAdvancesSpan.size(); i++) {
        advances.emplace_back(runAdvancesSpan[i].fX, runAdvancesSpan[i].fY);
    }
    return advances;
}

TextDirection RunBaseImpl::getTextDirection() const
{
    if (fVisitorRun == nullptr) {
        return TextDirection::kLtr;
    }
    return fVisitorRun->getTextDirection();
}

void RunBaseImpl::getStringRange(uint64_t* location, uint64_t* length) const
{
    if (location == nullptr || length == nullptr) {
        return;
    } else if (!fVisitorRun) {
        *location = 0;
        *length = 0;
        return;
    }
    *location = fVisitorGlobalPos;
    *length = fVisitorSize;
}

std::vector<uint64_t> RunBaseImpl::getStringIndices(int64_t start, int64_t length) const
{
    if (!fVisitorRun) {
        return {};
    }
    uint64_t actualLength = calculateActualLength(start, length);
    if (actualLength == 0) {
        return {};
    }
    std::vector<uint64_t> indices;
    for (size_t i = 0; i < actualLength; i++) {
        indices.emplace_back(fVisitorGlobalPos + start + i);
    }

    return indices;
}

SkRect RunBaseImpl::getAllGlyphRectInfo(SkSpan<const SkGlyphID>& runGlyphIdSpan, size_t startNotWhiteSpaceIndex,
    SkScalar startWhiteSpaceWidth, size_t endWhiteSpaceNum, SkScalar endAdvance) const
{
    SkRect rect = {0.0, 0.0, 0.0, 0.0};
    SkScalar runNotWhiteSpaceWidth = 0.0;
#ifndef USE_SKIA_TXT
    SkRect joinRect{0.0, 0.0, 0.0, 0.0};
    SkRect endRect {0.0, 0.0, 0.0, 0.0};
    SkRect startRect {0.0, 0.0, 0.0, 0.0};
#else
    RSRect joinRect{0.0, 0.0, 0.0, 0.0};
    RSRect endRect {0.0, 0.0, 0.0, 0.0};
    RSRect startRect {0.0, 0.0, 0.0, 0.0};
#endif
    size_t end = runGlyphIdSpan.size() - endWhiteSpaceNum;
    for (size_t i = startNotWhiteSpaceIndex; i < end; i++) {
    // Get the bounds of each glyph
#ifndef USE_SKIA_TXT
        SkRect glyphBounds;
        fVisitorRun->font().getBounds(&runGlyphIdSpan[i], 1, &glyphBounds, nullptr);
#else
        RSRect glyphBounds;
        fVisitorRun->font().GetWidths(&runGlyphIdSpan[i], 1, nullptr, &glyphBounds);
#endif
        // Record the first non-blank glyph boundary
        if (i == startNotWhiteSpaceIndex) {
            startRect = glyphBounds;
        }
        if (i == end - 1) {
            endRect = glyphBounds;
        }
        // Stitching removes glyph boundaries at the beginning and end of lines
        joinRect.Join(glyphBounds);
        auto& cluster = fVisitorRun->owner()->cluster(fVisitorGlobalPos + i);
        // Calculates the width of the glyph with the beginning and end of the line removed
        runNotWhiteSpaceWidth += cluster.width();
    }
#ifndef USE_SKIA_TXT
    // If the first glyph of run is a blank glyph, you need to add startWhitespaceWidth
    SkScalar x = fClipRect.fLeft + startRect.x() + startWhiteSpaceWidth;
    SkScalar y = joinRect.bottom();
    SkScalar width = runNotWhiteSpaceWidth - (endAdvance - endRect.x() - endRect.width()) - startRect.x();
    SkScalar height = joinRect.height();
#else
    SkScalar x = fClipRect.fLeft + startRect.GetLeft() + startWhiteSpaceWidth;
    SkScalar y = joinRect.GetBottom();
    SkScalar width = runNotWhiteSpaceWidth - (endAdvance - endRect.GetLeft() - endRect.GetWidth()) - startRect.GetLeft();
    SkScalar height = joinRect.GetHeight();
#endif
     rect.setXYWH(x, y, width, height);
     return rect;
}

#ifndef USE_SKIA_TXT
SkRect RunBaseImpl::getImageBounds() const
#else
RSRect RunBaseImpl::getImageBounds() const
#endif
{
    if (!fVisitorRun) {
        return {};
    }
    SkSpan<const SkGlyphID> glyphIdSpan = fVisitorRun->glyphs();
    SkSpan<const SkGlyphID> runGlyphIdSpan = glyphIdSpan.subspan(fVisitorPos, fVisitorSize);
    if (runGlyphIdSpan.size() == 0) {
        return {};
    }
    SkScalar endAdvance = 0.0;
    SkScalar startWhiteSpaceWidth = 0.0;
    size_t endWhiteSpaceNum = 0;
    size_t startNotWhiteSpaceIndex = 0;
    // Gets the width of the first non-blank glyph at the end
    for (size_t i = runGlyphIdSpan.size() - 1; i >= 0; --i) {
        auto& cluster = fVisitorRun->owner()->cluster(fVisitorGlobalPos + i);
        if (!cluster.isWhitespaceBreak()) {
            endAdvance = cluster.width();
            break;
        }
        ++endWhiteSpaceNum;
        if (i == 0) {
            break;
        }
    }
    // Gets the width of the first non-blank glyph at the end
    for (size_t i = 0; i < runGlyphIdSpan.size(); ++i) {
        auto& cluster = fVisitorRun->owner()->cluster(fVisitorGlobalPos + i);
        if (!cluster.isWhitespaceBreak()) {
            break;
        }
        startWhiteSpaceWidth += cluster.width();
        ++startNotWhiteSpaceIndex;
    }
    SkRect rect = getAllGlyphRectInfo(runGlyphIdSpan, startNotWhiteSpaceIndex, startWhiteSpaceWidth, endWhiteSpaceNum, endAdvance);
    return {rect.fLeft, rect.fTop, rect.fRight, rect.fBottom};
}

float RunBaseImpl::getTypographicBounds(float* ascent, float* descent, float* leading) const
{
    if (ascent == nullptr || descent == nullptr || leading == nullptr) {
        return 0.0;
    }
    if (!fVisitorRun) {
        *ascent = 0.0;
        *descent = 0.0;
        *leading = 0.0;
        return 0.0;
    }
    *ascent = fVisitorRun->ascent();
    *descent = fVisitorRun->descent();
    *leading = fVisitorRun->leading();
    return fClipRect.width() + calculateTrailSpacesWidth();
}

float RunBaseImpl::calculateTrailSpacesWidth() const
{
    // Calculates the width of the whitespace character at the end of the line
    if (!fVisitorRun || fTrailSpaces == 0) {
        return 0.0;
    }
    SkScalar spaceWidth = 0;
    for (size_t i = 0; i < fTrailSpaces; i++) {
        auto& cluster = fVisitorRun->owner()->cluster(fVisitorGlobalPos + fVisitorSize + i);
        // doesn't calculate the width of a hard line wrap at the end of a line
        if (cluster.isHardBreak()) {
            break;
        }
        spaceWidth += cluster.width();
    }

    return spaceWidth;
}

uint64_t RunBaseImpl::calculateActualLength(int64_t start, int64_t length) const
{
    // Calculate the actual size of the run,
    // start and length equal to 0 means that the data is obtained from start to end, so no filtering is required
    if (start < 0 || length < 0 || static_cast<size_t>(start) >= fVisitorSize) {
        return 0;
    }
    uint64_t actualLength = static_cast<uint64_t>(fVisitorSize - static_cast<size_t>(start));
    actualLength = (actualLength > static_cast<uint64_t>(length)) ? static_cast<uint64_t>(length)
                                                                  : actualLength;
    // If length is equal to 0, the end of the line is obtained
    if (start >= 0 && length == 0) {
        return fVisitorSize - static_cast<size_t>(start);
    }

    return actualLength;
}
#endif
}  // namespace textlayout
}  // namespace skia