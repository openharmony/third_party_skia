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

#include "modules/skparagraph/include/RunBase.h"
#include "modules/skparagraph/src/Run.h"

namespace skia {
namespace textlayout {
class RunBaseImpl : public RunBase {
public:
    RunBaseImpl(
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
        size_t visitorSize
    );

#ifndef USE_SKIA_TXT
    const SkFont& font() const override;
#else
    const RSFont& font() const override;
#endif

    size_t size() const override;
    std::vector<uint16_t> getGlyphs() const override;
    std::vector<RSPoint> getPositions() const override;
    std::vector<RSPoint> getOffsets() const override;

    void paint(ParagraphPainter* painter, SkScalar x, SkScalar y) override;

    size_t getVisitorPos() const;
    size_t getVisitorSize() const;

private:

#ifndef USE_SKIA_TXT
    sk_sp<SkTextBlob> fBlob;
#else
    std::shared_ptr<RSTextBlob> fBlob;
#endif
    SkPoint fOffset = SkPoint::Make(0.0f, 0.0f);
    ParagraphPainter::SkPaintOrID fPaint;
    bool fClippingNeeded = false;
    SkRect fClipRect = SkRect::MakeEmpty();

    const Run* fVisitorRun;
    size_t     fVisitorPos;
    size_t     fVisitorSize;
#ifndef USE_SKIA_TXT
    SkFont fFont;
#else
    RSFont fFont;
#endif
};
}  // namespace textlayout
}  // namespace skia

#endif  // SKPARAGRAPH_SRC_RUN_BASE_IMPL_H