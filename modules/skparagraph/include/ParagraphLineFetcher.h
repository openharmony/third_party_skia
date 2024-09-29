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

#ifndef ParagraphLineFetcher_DEFINED
#define ParagraphLineFetcher_DEFINED
#include "include/core/SkScalar.h"
#include "modules/skparagraph/include/Paragraph.h"
namespace skia {
namespace textlayout {
class ParagraphLineFetcher {
public:
    ParagraphLineFetcher() = default;
    virtual ~ParagraphLineFetcher() = default;
    virtual size_t getLineBreak(size_t startIndex, SkScalar width) const = 0;
    virtual std::unique_ptr<TextLineBase> createLine(size_t startIndex, size_t count) = 0;
    virtual std::unique_ptr<Paragraph> getTempParagraph() = 0;
    virtual size_t getUnicodeSize() const = 0;
};
}  // namespace textlayout
}  // namespace skia
#endif // ParagraphLineFetcher_DEFINED
