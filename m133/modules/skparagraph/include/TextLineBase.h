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

#ifndef TEXT_LINE_BASE_DEFINED
#define TEXT_LINE_BASE_DEFINED

#ifdef ENABLE_TEXT_ENHANCE
#include <stddef.h>

#include "modules/skparagraph/include/DartTypes.h"
#include "RunBase.h"

namespace skia {
namespace textlayout {

class TextLineBase {
public:
    virtual ~TextLineBase() = default;
    virtual size_t getGlyphCount() const = 0;
    virtual std::vector<std::unique_ptr<RunBase>> getGlyphRuns() const = 0;
    virtual SkRange<size_t> getTextRange() const = 0;
    virtual void paint(ParagraphPainter* painter, SkScalar x, SkScalar y) = 0;

#ifdef ENABLE_TEXT_ENHANCE
    virtual std::unique_ptr<TextLineBase> createTruncatedLine(double width, EllipsisModal ellipsisMode,
        const std::string& ellipsisStr) = 0;
    virtual double getTypographicBounds(double* ascent, double* descent, double* leading) const = 0;
    virtual RSRect getImageBounds() const = 0;
    virtual double getTrailingSpaceWidth() const = 0;
    virtual int32_t getStringIndexForPosition(SkPoint point) const = 0;
    virtual double getOffsetForStringIndex(int32_t index) const = 0;
    virtual std::map<int32_t, double> getIndexAndOffsets(bool& isHardBreak) const = 0;
    virtual double getAlignmentOffset(double alignmentFactor, double alignmentWidth) const = 0;
#endif
};
}  // namespace textlayout
}  // namespace skia

#endif // ENABLE_TEXT_ENHANCE
#endif  // TEXT_LINE_BASE_DEFINED