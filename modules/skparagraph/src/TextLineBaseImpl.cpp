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

#include "modules/skparagraph/src/TextLineBaseImpl.h"

namespace skia {
namespace textlayout {
TextLineBaseImpl::TextLineBaseImpl(TextLine* visitorTextLine) : fVisitorTextLine(visitorTextLine)
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

}  // namespace textlayout
}  // namespace skia