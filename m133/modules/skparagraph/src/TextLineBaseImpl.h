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

#ifndef SKPARAGRAPH_SRC_TEXT_LINE_BASE_IMPL_H
#define SKPARAGRAPH_SRC_TEXT_LINE_BASE_IMPL_H

#ifdef ENABLE_TEXT_ENHANCE

#include "modules/skparagraph/include/TextLineBase.h"
#include "modules/skparagraph/src/TextLine.h"

namespace skia {
namespace textlayout {
class TextLineBaseImpl : public TextLineBase {
public:
#ifdef ENABLE_TEXT_ENHANCE
    TextLineBaseImpl(std::unique_ptr<TextLine> visitorTextLine);
#else
    TextLineBaseImpl(TextLine* visitorTextLine);
#endif
    size_t getGlyphCount() const override;
    std::vector<std::unique_ptr<RunBase>> getGlyphRuns() const override;
    SkRange<size_t> getTextRange() const override;
    void paint(ParagraphPainter* painter, SkScalar x, SkScalar y) override;

    std::unique_ptr<TextLineBase> createTruncatedLine(double width, EllipsisModal ellipsisMode,
        const std::string& ellipsisStr) override;
    double getTypographicBounds(double* ascent, double* descent, double* leading) const override;
    RSRect getImageBounds() const override;
    double getTrailingSpaceWidth() const override;
    int32_t getStringIndexForPosition(SkPoint point) const override;
    double getOffsetForStringIndex(int32_t index) const override;
    std::map<int32_t, double> getIndexAndOffsets(bool& isHardBreak) const override;
    double getAlignmentOffset(double alignmentFactor, double alignmentWidth) const override;


private:
#ifdef ENABLE_TEXT_ENHANCE
    std::unique_ptr<TextLine> fVisitorTextLine;
#else
    TextLine* fVisitorTextLine;
#endif
};
}  // namespace textlayout
}  // namespace skia

#endif // ENABLE_TEXT_ENHANCE
#endif  // SKPARAGRAPH_SRC_TEXT_LINE_BASE_IMPL_H