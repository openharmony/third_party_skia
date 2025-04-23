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

#ifndef SKPARAGRAPH_INCLUDE_RUN_BASE_H
#define SKPARAGRAPH_INCLUDE_RUN_BASE_H
#ifdef ENABLE_TEXT_ENHANCE
#include "modules/skparagraph/include/ParagraphPainter.h"

namespace skia {
namespace textlayout {
class RunBase {
public:

    virtual ~RunBase() = default;
#ifdef ENABLE_DRAWING_ADAPTER
    virtual const RSFont& font() const = 0;
#else
    virtual const SkFont& font() const = 0;
#endif // ENABLE_DRAWING_ADAPTER

    virtual size_t size() const = 0;
    virtual std::vector<uint16_t> getGlyphs() const = 0;
    virtual std::vector<RSPoint> getPositions() const = 0;
    virtual std::vector<RSPoint> getOffsets() const = 0;

    virtual std::vector<uint16_t> getGlyphs(int64_t start, int64_t length) const = 0;
    virtual void getStringRange(uint64_t* location, uint64_t* length) const = 0;
    virtual std::vector<uint64_t> getStringIndices(int64_t start, int64_t length) const = 0;
    virtual float getTypographicBounds(float* ascent, float* descent, float* leading) const = 0;

#ifdef ENABLE_DRAWING_ADAPTER
    virtual RSRect getImageBounds() const = 0;
    virtual std::vector<RSPoint> getPositions(int64_t start, int64_t length) const = 0;
#else
    virtual SkRect getImageBounds() const = 0;
    virtual std::vector<SkPoint> getPositions(int64_t start, int64_t length) const = 0;
#endif // ENABLE_DRAWING_ADAPTER
    virtual void paint(ParagraphPainter* painter, SkScalar x, SkScalar y) = 0;
};
}  // namespace textlayout
}  // namespace skia
#endif // ENABLE_TEXT_ENHANCE
#endif  // SKPARAGRAPH_INCLUDE_RUN_BASE_H
