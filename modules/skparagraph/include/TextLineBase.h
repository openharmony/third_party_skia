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

#ifndef TextLineBase_DEFINED
#define TextLineBase_DEFINED

#include <stddef.h>
#include "RunBase.h"
#include "modules/skparagraph/include/DartTypes.h"

namespace skia {
namespace textlayout {

class TextLineBase {
public:
    virtual ~TextLineBase() = default;
    virtual size_t getGlyphCount() const = 0;
    virtual std::vector<std::unique_ptr<RunBase>> getGlyphRuns() const = 0;
    virtual SkRange<size_t> getTextRange() const = 0;
    virtual void paint(ParagraphPainter* painter, SkScalar x, SkScalar y) = 0;
};
}  // namespace textlayout
}  // namespace skia

#endif  // TextLineBase_DEFINED