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

#ifndef SKPARAGRAPH_SRC_TEXT_TAB_ALIGN_H
#define SKPARAGRAPH_SRC_TEXT_TAB_ALIGN_H

#ifdef ENABLE_TEXT_ENHANCE

#include <string>
#include "include/ParagraphStyle.h"
#include "include/core/SkSpan.h"
#include "modules/skparagraph/src/TextLine.h"
#include "modules/skparagraph/src/TextWrapper.h"

namespace skia {
namespace textlayout {
class TextTabAlign {
public:
    explicit TextTabAlign(const TextTabs& other) : fTabAlignMode(other.alignment), fTabPosition(other.location) {}
    void init(SkScalar maxWidth, Cluster* endOfClusters);

    bool processTab(TextWrapper::TextStretch& words, TextWrapper::TextStretch& clusters, Cluster* currentCluster,
        SkScalar totalFakeSpacing)
    {
        if ((fTextTabFuncs != nullptr) && (this->fTextTabFuncs->processTabFunc != nullptr) &&
            (currentCluster != nullptr)) {
            return (this->*(fTextTabFuncs->processTabFunc))(words, clusters, currentCluster, totalFakeSpacing);
        }
        return false;
    }

    bool processEndofWord(TextWrapper::TextStretch& words, TextWrapper::TextStretch& clusters, Cluster* currentCluster,
        SkScalar totalFakeSpacing)
    {
        if ((fTextTabFuncs != nullptr) && (this->fTextTabFuncs->processEndofWordFunc != nullptr) &&
            (currentCluster != nullptr)) {
            return (this->*(fTextTabFuncs->processEndofWordFunc))(words, clusters, currentCluster, totalFakeSpacing);
        }
        return false;
    }

    bool processEndofLine(TextWrapper::TextStretch& words, TextWrapper::TextStretch& clusters, Cluster* currentCluster,
        SkScalar totalFakeSpacing)
    {
        if ((fTextTabFuncs != nullptr) && (this->fTextTabFuncs->processEndofLineFunc != nullptr) &&
            (currentCluster != nullptr)) {
            return (this->*(fTextTabFuncs->processEndofLineFunc))(words, clusters, currentCluster, totalFakeSpacing);
        }
        return false;
    }

    bool processCluster(TextWrapper::TextStretch& words, TextWrapper::TextStretch& clusters, Cluster* currentCluster,
        SkScalar totalFakeSpacing)
    {
        if ((fTextTabFuncs != nullptr) && (this->fTextTabFuncs->processClusterFunc != nullptr) &&
            (currentCluster != nullptr)) {
            return (this->*(fTextTabFuncs->processClusterFunc))(words, clusters, currentCluster, totalFakeSpacing);
        }
        return false;
    }

private:
    void expendTabCluster(SkScalar width);

    bool leftAlignProcessTab(TextWrapper::TextStretch& words, TextWrapper::TextStretch& clusters,
        Cluster* currentCluster, SkScalar totalFakeSpacing);
    bool leftAlignProcessEndofWord(TextWrapper::TextStretch& words, TextWrapper::TextStretch& clusters,
        Cluster* currentCluster, SkScalar totalFakeSpacing);
    bool leftAlignProcessEndofLine(TextWrapper::TextStretch& words, TextWrapper::TextStretch& clusters,
        Cluster* currentCluster, SkScalar totalFakeSpacing);
    bool leftAlignProcessCluster(TextWrapper::TextStretch& words, TextWrapper::TextStretch& clusters,
        Cluster* currentCluster, SkScalar totalFakeSpacing);

    void rightAlignProcessTabBlockEnd(TextWrapper::TextStretch& words, TextWrapper::TextStretch& clusters);
    bool rightAlignProcessTab(TextWrapper::TextStretch& words, TextWrapper::TextStretch& clusters,
        Cluster* currentCluster, SkScalar totalFakeSpacing);
    bool rightAlignProcessEndofWord(TextWrapper::TextStretch& words, TextWrapper::TextStretch& clusters,
        Cluster* currentCluster, SkScalar totalFakeSpacing);
    bool rightAlignProcessEndofLine(TextWrapper::TextStretch& words, TextWrapper::TextStretch& clusters,
        Cluster* currentCluster, SkScalar totalFakeSpacing);
    bool rightAlignProcessCluster(TextWrapper::TextStretch& words, TextWrapper::TextStretch& clusters,
        Cluster* currentCluster, SkScalar totalFakeSpacing);

    bool centerAlignProcessTabBlockEnd(TextWrapper::TextStretch& words, TextWrapper::TextStretch& clusters);
    bool centerAlignProcessTab(TextWrapper::TextStretch& words, TextWrapper::TextStretch& clusters,
        Cluster* currentCluster, SkScalar totalFakeSpacing);
    bool centerAlignProcessEndofWord(TextWrapper::TextStretch& words, TextWrapper::TextStretch& clusters,
        Cluster* currentCluster, SkScalar totalFakeSpacing);
    bool centerAlignProcessEndofLine(TextWrapper::TextStretch& words, TextWrapper::TextStretch& clusters,
        Cluster* currentCluster, SkScalar totalFakeSpacing);
    bool centerAlignProcessCluster(TextWrapper::TextStretch& words, TextWrapper::TextStretch& clusters,
        Cluster* currentCluster, SkScalar totalFakeSpacing);

    TextAlign fTabAlignMode;
    SkScalar fTabPosition{0.0};
    bool fAlreadyInTab{false};
    SkScalar fTabStartPos{0.0};
    SkScalar fTabEndPos{0.0};
    SkScalar fTabShift{0.0};
    int fTabIndex{0};
    int fMaxTabIndex{0};
    Cluster* fTabBlockEnd{nullptr};
    Cluster* fEndOfClusters{nullptr};
    SkScalar fMaxWidth{0.0f};
    Cluster* fTabCluster{nullptr};

    using TextTabFunc = bool (TextTabAlign::*)(TextWrapper::TextStretch&, TextWrapper::TextStretch&, Cluster*, SkScalar);
    struct TextTabFuncs {
        TextTabFunc processTabFunc;
        TextTabFunc processEndofWordFunc;
        TextTabFunc processEndofLineFunc;
        TextTabFunc processClusterFunc;
    };
    constexpr static size_t textAlignCount = static_cast<size_t>(TextAlign::kCenter) + 1;
    static TextTabFuncs fTextTabFuncsTable[textAlignCount];
    TextTabFuncs* fTextTabFuncs{nullptr};

};
}  // namespace textlayout
}  // namespace skia

#endif // ENABLE_TEXT_ENHANCE
#endif  // SKPARAGRAPH_SRC_TEXT_TAB_ALIGN_H