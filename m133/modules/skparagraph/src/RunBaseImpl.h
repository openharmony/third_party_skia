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

#ifndef SKPARAGRAPH_SRC_RUN_BASE_IMPL_H
#define SKPARAGRAPH_SRC_RUN_BASE_IMPL_H

#ifdef ENABLE_TEXT_ENHANCE
#include "modules/skparagraph/include/RunBase.h"
#include "modules/skparagraph/src/Run.h"

namespace skia {
namespace textlayout {
class RunBaseImpl : public RunBase {
public:
    RunBaseImpl(
        std::shared_ptr<RSTextBlob> blob,
        SkPoint offset,
        ParagraphPainter::SkPaintOrID paint,
        bool clippingNeeded,
        SkRect clipRect,
        const Run* visitorRun,
        size_t visitorPos,
        size_t visitorGlobalPos,
        size_t trailSpaces,
        size_t visitorSize
    );

    const RSFont& font() const override;
    size_t size() const override;
    std::vector<uint16_t> getGlyphs() const override;
    std::vector<RSPoint> getPositions() const override;
    std::vector<RSPoint> getOffsets() const override;

    std::vector<uint16_t> getGlyphs(int64_t start, int64_t length) const override;
    void getStringRange(uint64_t* location, uint64_t* length) const override;
    std::vector<uint64_t> getStringIndices(int64_t start, int64_t length) const override;
    float getTypographicBounds(float* ascent, float* descent, float* leading) const override;
    std::vector<RSPoint> getAdvances(uint32_t start, uint32_t length) const override;
    TextDirection getTextDirection() const override;
    RSRect getImageBounds() const override;
    std::vector<RSPoint> getPositions(int64_t start, int64_t length) const override;
    void paint(ParagraphPainter* painter, SkScalar x, SkScalar y) override;

    size_t getVisitorPos() const;
    size_t getVisitorSize() const;

private:
    float calculateTrailSpacesWidth() const;
    uint64_t calculateActualLength(int64_t start, int64_t length) const;
    SkRect getAllGlyphRectInfo(SkSpan<const SkGlyphID>& runGlyphIdSpan, size_t startNotWhiteSpaceIndex,
        SkScalar startWhiteSpaceWidth, size_t endWhiteSpaceNum, SkScalar endAdvance) const;
    RSFont fFont;
    std::shared_ptr<RSTextBlob> fBlob;
    SkPoint fOffset = SkPoint::Make(0.0f, 0.0f);
    ParagraphPainter::SkPaintOrID fPaint;
    bool fClippingNeeded = false;
    SkRect fClipRect = SkRect::MakeEmpty();

    const Run* fVisitorRun;
    size_t     fVisitorPos;
    size_t     fVisitorGlobalPos = 0;
    size_t     fTrailSpaces = 0;
    size_t     fVisitorSize;
};
}  // namespace textlayout
}  // namespace skia
#endif // ENABLE_TEXT_ENHANCE
#endif  // SKPARAGRAPH_SRC_RUN_BASE_IMPL_H