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

#ifdef ENABLE_TEXT_ENHANCE

#include "modules/skparagraph/src/TextLineBaseImpl.h"

namespace skia {
namespace textlayout {
#ifdef ENABLE_TEXT_ENHANCE
TextLineBaseImpl::TextLineBaseImpl(std::unique_ptr<TextLine> visitorTextLine)
                : fVisitorTextLine(std::move(visitorTextLine))
#else
TextLineBaseImpl::TextLineBaseImpl(TextLine* visitorTextLine) : fVisitorTextLine(visitorTextLine)
#endif
{
}

size_t TextLineBaseImpl::getGlyphCount() const
{
    if (!fVisitorTextLine) {
        return 0;
    }
    fVisitorTextLine->ensureTextBlobCachePopulated();
    return fVisitorTextLine->getGlyphCount();
}

std::vector<std::unique_ptr<RunBase>> TextLineBaseImpl::getGlyphRuns() const
{
    if (!fVisitorTextLine) {
        return {};
    }
    fVisitorTextLine->ensureTextBlobCachePopulated();
    return fVisitorTextLine->getGlyphRuns();
}

SkRange<size_t> TextLineBaseImpl::getTextRange() const
{
    if (!fVisitorTextLine) {
        return {};
    }
    return fVisitorTextLine->text();
}

void TextLineBaseImpl::paint(ParagraphPainter* painter, SkScalar x, SkScalar y)
{
    if (!fVisitorTextLine || !painter) {
        return;
    }
    return fVisitorTextLine->paint(painter, x, y);
}

#ifdef ENABLE_TEXT_ENHANCE
std::unique_ptr<TextLineBase> TextLineBaseImpl::createTruncatedLine(double width, EllipsisModal ellipsisMode,
    const std::string& ellipsisStr)
{
    if (!fVisitorTextLine) {
        return nullptr;
    }

    return fVisitorTextLine->createTruncatedLine(width, ellipsisMode, ellipsisStr);
}

double TextLineBaseImpl::getTypographicBounds(double* ascent, double* descent, double* leading) const
{
    if (!fVisitorTextLine) {
        return 0.0;
    }

    return fVisitorTextLine->getTypographicBounds(ascent, descent, leading);
}

RSRect TextLineBaseImpl::getImageBounds() const
{
    if (!fVisitorTextLine) {
        return {};
    }

    return fVisitorTextLine->getImageBounds();
}

double TextLineBaseImpl::getTrailingSpaceWidth() const
{
    if (!fVisitorTextLine) {
        return 0.0;
    }

    return fVisitorTextLine->getTrailingSpaceWidth();
}

int32_t TextLineBaseImpl::getStringIndexForPosition(SkPoint point) const
{
    if (!fVisitorTextLine) {
        return 0;
    }

    return fVisitorTextLine->getStringIndexForPosition(point);
}

double TextLineBaseImpl::getOffsetForStringIndex(int32_t index) const
{
    if (!fVisitorTextLine) {
        return 0.0;
    }

    return fVisitorTextLine->getOffsetForStringIndex(index);
}

std::map<int32_t, double> TextLineBaseImpl::getIndexAndOffsets(bool& isHardBreak) const
{
    if (!fVisitorTextLine) {
        return {};
    }

    return fVisitorTextLine->getIndexAndOffsets(isHardBreak);
}

double TextLineBaseImpl::getAlignmentOffset(double alignmentFactor, double alignmentWidth) const
{
    if (!fVisitorTextLine) {
        return 0.0;
    }

    return fVisitorTextLine->getAlignmentOffset(alignmentFactor, alignmentWidth);
}
#endif
}  // namespace textlayout
}  // namespace skia

#endif // ENABLE_TEXT_ENHANCE