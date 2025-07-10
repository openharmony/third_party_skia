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

#ifndef ParagraphLineFetcherImpl_DEFINED
#define ParagraphLineFetcherImpl_DEFINED
#include <iostream>
#include "include/core/SkScalar.h"
#include "modules/skparagraph/include/Paragraph.h"
#include "modules/skparagraph/include/ParagraphLineFetcher.h"
namespace skia {
namespace textlayout {
class ParagraphLineFetcherImpl final : public ParagraphLineFetcher {
public:
    ParagraphLineFetcherImpl() = delete;
    explicit ParagraphLineFetcherImpl(std::unique_ptr<Paragraph>&& paragraph);
    ~ParagraphLineFetcherImpl() override;
    size_t getLineBreak(size_t startIndex, SkScalar width) const override;
    std::unique_ptr<TextLineBase> createLine(size_t startIndex, size_t count) override;
    std::unique_ptr<Paragraph> getTempParagraph() override { return std::move(fTempParagraph); };
    size_t getUnicodeSize() const override { return fUnicodeSize; };
private:
    std::unique_ptr<Paragraph> fTempParagraph = nullptr;
    std::unique_ptr<Paragraph> fRootParagraph = nullptr;
    size_t fUnicodeSize;
};
}  // namespace textlayout
}  // namespace skia
#endif // ParagraphLineFetcherImpl_DEFINED
