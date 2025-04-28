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
#include "ParagraphLineFetcherImpl.h"
#ifdef ENABLE_TEXT_ENHANCE
namespace skia {
namespace textlayout {
ParagraphLineFetcherImpl::ParagraphLineFetcherImpl(std::unique_ptr<Paragraph>&& paragraph)
        : fRootParagraph(move(paragraph)) {
    fRootParagraph->initUnicodeText();
    fUnicodeSize = fRootParagraph->unicodeText().size();
}

ParagraphLineFetcherImpl::~ParagraphLineFetcherImpl() = default;

size_t ParagraphLineFetcherImpl::getLineBreak(size_t startIndex, SkScalar width) const {
    if (startIndex >= fRootParagraph->unicodeText().size()) {
        return 0;
    }
    std::unique_ptr<Paragraph> newParagraph = fRootParagraph->createCroppedCopy(startIndex);
    if (newParagraph == nullptr) {
        return 0;
    }
    newParagraph->layout(width);
    TextRange textRange = newParagraph->getActualTextRange(0, true);
    if (textRange == EMPTY_TEXT) {
        return 0;
    }
    size_t count = newParagraph->getUnicodeIndex(textRange.end);
    const std::vector<SkUnichar> unicodeText = newParagraph->unicodeText();
    if (count < unicodeText.size() && unicodeText[count] == '\n') {
        count++;
    }
    return count;
}

std::unique_ptr<TextLineBase> ParagraphLineFetcherImpl::createLine(size_t startIndex,
                                                                   size_t count) {
    size_t unicodeSize = fRootParagraph->unicodeText().size();
    if (startIndex >= unicodeSize) {
        return nullptr;
    }
    if (startIndex + count >= unicodeSize || count == 0) {
        count = unicodeSize - startIndex;
    }
    fTempParagraph = fRootParagraph->createCroppedCopy(startIndex, count);
    if (fTempParagraph == nullptr) {
        return nullptr;
    }
    fTempParagraph->layout(std::numeric_limits<float>::max());
    auto textLine = fTempParagraph->GetTextLines();
    if (textLine.empty()) {
        return nullptr;
    }
    return move(textLine.front());
}
}  // namespace textlayout
}  // namespace skia
#endif // ENABLE_TEXT_ENHANCE
