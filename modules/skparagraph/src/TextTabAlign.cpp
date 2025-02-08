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

#include "modules/skparagraph/src/ParagraphImpl.h"
#include "modules/skparagraph/src/TextTabAlign.h"
#include "log.h"

namespace skia {
namespace textlayout {

TextTabAlign::TextTabFuncs TextTabAlign::fTextTabFuncsTable[TextTabAlign::textAlignCount] = {
    {
        &TextTabAlign::leftAlignProcessTab,
        &TextTabAlign::leftAlignProcessEndofWord,
        &TextTabAlign::leftAlignProcessEndofLine,
        &TextTabAlign::leftAlignProcessCluster
    },
    {
        &TextTabAlign::rightAlignProcessTab,
        &TextTabAlign::rightAlignProcessEndofWord,
        &TextTabAlign::rightAlignProcessEndofLine,
        &TextTabAlign::rightAlignProcessCluster
    },
    {
        &TextTabAlign::centerAlignProcessTab,
        &TextTabAlign::centerAlignProcessEndofWord,
        &TextTabAlign::centerAlignProcessEndofLine,
        &TextTabAlign::centerAlignProcessCluster
    },
};

void TextTabAlign::init(SkScalar maxWidth, Cluster* endOfClusters)
{
    fMaxWidth = maxWidth;
    fEndOfClusters = endOfClusters;
    if (fTabPosition < 1.0 || fTabAlignMode < TextAlign::kLeft || TextAlign::kCenter < fTabAlignMode ||
        endOfClusters == nullptr) {
        return;
    }
    fMaxTabIndex = fMaxWidth / fTabPosition;

    // If textAlign is configured, textTabAlign does not take effect
    if (endOfClusters->getOwner()->paragraphStyle().getTextAlign() != TextAlign::kStart) {
        TEXT_LOGD("textAlign is configured, textTabAlign does not take effect");
        return;
    }

    // If ellipsis is configured, textTabAlign does not take effect
    if (endOfClusters->getOwner()->paragraphStyle().ellipsized()) {
        TEXT_LOGD("ellipsis is configured, textTabAlign does not take effect");
        return;
    }

    TextAlign tabAlignMode = fTabAlignMode;
    if (endOfClusters->getOwner()->paragraphStyle().getTextDirection() == TextDirection::kRtl) {
        if (tabAlignMode == TextAlign::kLeft) {
            tabAlignMode = TextAlign::kRight;
        } else if (tabAlignMode == TextAlign::kRight) {
            tabAlignMode = TextAlign::kLeft;
        }
    }
    fTextTabFuncs = &(fTextTabFuncsTable[static_cast<size_t>(tabAlignMode)]);
}

void TextTabAlign::expendTabCluster(SkScalar width)
{
    fTabCluster->run().extendClusterWidth(fTabCluster, width);
    TEXT_LOGD("tabCluster(%zu, %zu) expend %f", fTabCluster->textRange().start, fTabCluster->textRange().end, width);
}

bool TextTabAlign::leftAlignProcessTab(TextWrapper::TextStretch& words, TextWrapper::TextStretch& clusters,
    Cluster* currentCluster, SkScalar totalFakeSpacing)
{
    fAlreadyInTab = true;
    fTabCluster = currentCluster;
    fTabBlockEnd = fTabCluster;
    fTabStartPos = words.width() + clusters.width() + totalFakeSpacing;
    do {
        fTabIndex++;
    } while ((fTabPosition * fTabIndex) < fTabStartPos);

    if (fTabIndex > fMaxTabIndex) {
        expendTabCluster(0 - fTabCluster->width());
        clusters.extend(currentCluster);
        words.extend(clusters);
        return true;
    }

    fTabEndPos = fTabStartPos;
    fTabShift = fTabPosition * fTabIndex - fTabStartPos;
    expendTabCluster(fTabShift - fTabCluster->width());
    return false;
}

bool TextTabAlign::leftAlignProcessEndofWord(TextWrapper::TextStretch& words, TextWrapper::TextStretch& clusters,
    Cluster* currentCluster, SkScalar totalFakeSpacing)
{
    if (fAlreadyInTab) {
        fTabBlockEnd = currentCluster;
    }
    return false;
}

bool TextTabAlign::leftAlignProcessEndofLine(TextWrapper::TextStretch& words, TextWrapper::TextStretch& clusters,
    Cluster* currentCluster, SkScalar totalFakeSpacing)
{
    if (fAlreadyInTab && (fTabBlockEnd == fTabCluster)) {
        words.shiftWidth(0 - fTabCluster->width());
        expendTabCluster(0 - fTabCluster->width());
    }
    return false;
}

bool TextTabAlign::leftAlignProcessCluster(TextWrapper::TextStretch& words, TextWrapper::TextStretch& clusters,
    Cluster* currentCluster, SkScalar totalFakeSpacing)
{
    if (fAlreadyInTab && (currentCluster->getOwner()->getWordBreakType() == WordBreakType::BREAK_ALL)) {
        fTabBlockEnd = currentCluster;
    }
    return false;
}

void TextTabAlign::rightAlignProcessTabBlockEnd(TextWrapper::TextStretch& words, TextWrapper::TextStretch& clusters)
{
    if ((fTabBlockEnd != fTabCluster) && ((fTabPosition * fTabIndex) > fTabEndPos)) {
        fTabShift = fTabPosition * fTabIndex - fTabEndPos;
        expendTabCluster(fTabShift);
        words.shiftWidth(fTabShift);
    }
}

bool TextTabAlign::rightAlignProcessTab(TextWrapper::TextStretch& words, TextWrapper::TextStretch& clusters,
    Cluster* currentCluster, SkScalar totalFakeSpacing)
{
    if (fAlreadyInTab) {
        fTabBlockEnd = currentCluster;
        fTabEndPos = words.width() + clusters.width() + totalFakeSpacing;
        rightAlignProcessTabBlockEnd(words, clusters);
    }

    fAlreadyInTab = true;
    fTabCluster = currentCluster;
    fTabBlockEnd = fTabCluster;
    expendTabCluster(0 - fTabCluster->width());
    
    fTabStartPos = words.width() + clusters.width() + totalFakeSpacing;
    do {
        fTabIndex++;
    } while ((fTabPosition * fTabIndex) < fTabStartPos);

    if (fTabIndex > fMaxTabIndex) {
        clusters.extend(currentCluster);
        words.extend(clusters);
        return true;
    }
    fTabEndPos = fTabStartPos;
    return false;
}

bool TextTabAlign::rightAlignProcessEndofWord(TextWrapper::TextStretch& words, TextWrapper::TextStretch& clusters,
    Cluster* currentCluster, SkScalar totalFakeSpacing)
{
    if (!fAlreadyInTab) {
        return false;
    }

    fTabEndPos = words.width() + clusters.width() + totalFakeSpacing;
    fTabBlockEnd = currentCluster;
    if (currentCluster + 1 == fEndOfClusters) {
        rightAlignProcessTabBlockEnd(words, clusters);
        return false;
    }

    if (currentCluster->isHardBreak()) {
        fTabEndPos -= currentCluster->width();
        return rightAlignProcessEndofLine(words, clusters, currentCluster, totalFakeSpacing);
    }

    return false;
}

bool TextTabAlign::rightAlignProcessEndofLine(TextWrapper::TextStretch& words, TextWrapper::TextStretch& clusters,
    Cluster* currentCluster, SkScalar totalFakeSpacing)
{
    if (!fAlreadyInTab) {
        return false;
    }

    rightAlignProcessTabBlockEnd(words, clusters);
    return false;
}

bool TextTabAlign::rightAlignProcessCluster(TextWrapper::TextStretch& words, TextWrapper::TextStretch& clusters,
    Cluster* currentCluster, SkScalar totalFakeSpacing)
{
    if (fAlreadyInTab && (currentCluster->getOwner()->getWordBreakType() == WordBreakType::BREAK_ALL)) {
        fTabEndPos = words.width() + clusters.width() + totalFakeSpacing;
        fTabBlockEnd = currentCluster;
    }

    return false;
}

bool TextTabAlign::centerAlignProcessTabBlockEnd(TextWrapper::TextStretch& words, TextWrapper::TextStretch& clusters)
{
    if ((fTabPosition * fTabIndex + ((fTabEndPos - fTabStartPos) / 2)) > fMaxWidth) {
        return true;
    }

    if ((fTabBlockEnd != fTabCluster) &&
        ((fTabPosition * fTabIndex) > (fTabStartPos + ((fTabEndPos - fTabStartPos) / 2)))) {
        fTabShift = fTabPosition * fTabIndex - (fTabStartPos + ((fTabEndPos - fTabStartPos) / 2));
        expendTabCluster(fTabShift);
        words.shiftWidth(fTabShift);
    }
    return false;
}

bool TextTabAlign::centerAlignProcessTab(TextWrapper::TextStretch& words, TextWrapper::TextStretch& clusters,
    Cluster* currentCluster, SkScalar totalFakeSpacing)
{
    if (fAlreadyInTab) {
        fTabBlockEnd = currentCluster;
        fTabEndPos = words.width() + clusters.width() + totalFakeSpacing;
        if (centerAlignProcessTabBlockEnd(words, clusters)) {
            clusters.extend(currentCluster);
            return true;
        }
    }

    fAlreadyInTab = true;
    fTabCluster = currentCluster;
    fTabBlockEnd = fTabCluster;
    expendTabCluster(0 - fTabCluster->width());

    fTabStartPos = words.width() + clusters.width() + totalFakeSpacing;
    do {
        fTabIndex++;
    } while ((fTabPosition * fTabIndex) < fTabStartPos);

    if (fTabIndex > fMaxTabIndex) {
        clusters.extend(currentCluster);
        words.extend(clusters);
        return true;
    }

    fTabEndPos = fTabStartPos;
    return false;
}

bool TextTabAlign::centerAlignProcessEndofWord(TextWrapper::TextStretch& words, TextWrapper::TextStretch& clusters,
    Cluster* currentCluster, SkScalar totalFakeSpacing)
{
    if (!fAlreadyInTab) {
        return false;
    }

    SkScalar tabEndPosTmp = words.width() + clusters.width() + totalFakeSpacing;
    if ((fTabPosition * fTabIndex + ((tabEndPosTmp - fTabStartPos) / 2)) > fMaxWidth) {
        centerAlignProcessTabBlockEnd(words, clusters);
        return true;
    }

    fTabEndPos = tabEndPosTmp;
    fTabBlockEnd = currentCluster;

    if (currentCluster + 1 == fEndOfClusters) {
        return centerAlignProcessTabBlockEnd(words, clusters);
    }

    if (currentCluster->isHardBreak()) {
        fTabEndPos -= currentCluster->width();
        return centerAlignProcessEndofLine(words, clusters, currentCluster, totalFakeSpacing);
    }

    return false;
}

bool TextTabAlign::centerAlignProcessEndofLine(TextWrapper::TextStretch& words, TextWrapper::TextStretch& clusters,
    Cluster* currentCluster, SkScalar totalFakeSpacing)
{
    if (!fAlreadyInTab) {
        return false;
    }

    centerAlignProcessTabBlockEnd(words, clusters);
    return false;
}

bool TextTabAlign::centerAlignProcessCluster(TextWrapper::TextStretch& words, TextWrapper::TextStretch& clusters,
    Cluster* currentCluster, SkScalar totalFakeSpacing)
{
    if ((!fAlreadyInTab) || (currentCluster->getOwner()->getWordBreakType() != WordBreakType::BREAK_ALL)) {
        return false;
    }

    SkScalar tabEndPosTmp = words.width() + clusters.width() + totalFakeSpacing;
    if (((tabEndPosTmp - fTabStartPos) / 2) > (fMaxWidth - fTabPosition * fTabIndex)) {
        centerAlignProcessTabBlockEnd(words, clusters);
        return true;
    }

    fTabEndPos = tabEndPosTmp;
    fTabBlockEnd = currentCluster;
    return false;
}

}  // namespace textlayout
}  // namespace skia