// Copyright 2019 Google LLC.
#ifndef TextWrapper_DEFINED
#define TextWrapper_DEFINED

#include <string>
#ifdef ENABLE_TEXT_ENHANCE
#include "include/ParagraphStyle.h"
#include "src/Run.h"
#endif
#include "include/core/SkSpan.h"
#include "modules/skparagraph/src/TextLine.h"

#ifdef ENABLE_TEXT_ENHANCE
#include <list>
#include <vector>

#include "include/TextStyle.h"
#include "modules/skparagraph/src/ParagraphImpl.h"
#include "SkScalar.h"
#endif

namespace skia {
namespace textlayout {

#ifdef ENABLE_TEXT_ENHANCE
const size_t STRATEGY_START_POS{2};
const size_t MIN_COST_POS{2};
const size_t MAX_LINES_LIMIT{1000000000};
#endif

class ParagraphImpl;
#ifdef ENABLE_TEXT_ENHANCE
class TextTabAlign;
#endif

class TextWrapper {
    class ClusterPos {
    public:
        ClusterPos() : fCluster(nullptr), fPos(0) {}
        ClusterPos(Cluster* cluster, size_t pos) : fCluster(cluster), fPos(pos) {}
        inline Cluster* cluster() const { return fCluster; }
        inline size_t position() const { return fPos; }
        inline void setPosition(size_t pos) { fPos = pos; }
        void clean() {
            fCluster = nullptr;
            fPos = 0;
        }
        void move(bool up) {
            fCluster += up ? 1 : -1;
            fPos = up ? 0 : fCluster->endPos();
        }
#ifdef ENABLE_TEXT_ENHANCE
        void setCluster(Cluster* cluster) { fCluster = cluster; }
#endif

    private:
        Cluster* fCluster;
        size_t fPos;
    };
    class TextStretch {
    public:
        TextStretch() : fStart(), fEnd(), fWidth(0), fWidthWithGhostSpaces(0) {}
        TextStretch(Cluster* s, Cluster* e, bool forceStrut)
                : fStart(s, 0), fEnd(e, e->endPos()), fMetrics(forceStrut), fWidth(0), fWidthWithGhostSpaces(0) {
            for (auto c = s; c <= e; ++c) {
                if (auto r = c->runOrNull()) {
                    fMetrics.add(r);
                }
                if (c < e) {
                    fWidth += c->width();
                }
            }
            fWidthWithGhostSpaces = fWidth;
        }

#ifdef ENABLE_TEXT_ENHANCE
        TextStretch(Cluster* c, bool forceStrut)
                : fStart(c, 0), fEnd(c, c->endPos()), fMetrics(forceStrut), fWidth(0), fWidthWithGhostSpaces(0) {
            if (auto r = c->runOrNull()) {
                fMetrics.add(r);
            }
            fWidth = c->width();
            fWidthWithGhostSpaces = fWidth;
        }

        std::vector<TextStretch> split() {
            ParagraphImpl* owner = fStart.cluster()->getOwner();
            Cluster* cluster = fStart.cluster();
            std::vector<TextStretch> result{};
            while(cluster <= fEnd.cluster()) {
                auto endIndex = (cluster)->textRange().end;
                Cluster* endCluster = &owner->cluster(
                    std::min(owner->fClustersIndexFromCodeUnit[endIndex], owner->clusters().size() - 1));
                TextStretch singleClusterStretch = TextStretch(cluster, metrics().getForceStrut());
                result.push_back(singleClusterStretch);
                cluster = endCluster;
            }
            return result;
        }

        void setStartCluster(Cluster* cluster) { fStart.setCluster(cluster); }
#endif

        inline SkScalar width() const { return fWidth; }
        SkScalar widthWithGhostSpaces() const { return fWidthWithGhostSpaces; }
        inline Cluster* startCluster() const { return fStart.cluster(); }
        inline Cluster* endCluster() const { return fEnd.cluster(); }
        inline Cluster* breakCluster() const { return fBreak.cluster(); }
        inline InternalLineMetrics& metrics() { return fMetrics; }
        inline size_t startPos() const { return fStart.position(); }
        inline size_t endPos() const { return fEnd.position(); }
        bool endOfCluster() { return fEnd.position() == fEnd.cluster()->endPos(); }
        bool endOfWord() {
            return endOfCluster() &&
                   (fEnd.cluster()->isHardBreak() || fEnd.cluster()->isSoftBreak());
        }

        void extend(TextStretch& stretch) {
            fMetrics.add(stretch.fMetrics);
            fEnd = stretch.fEnd;
            fWidth += stretch.fWidth;
            stretch.clean();
        }

        bool empty() { return fStart.cluster() == fEnd.cluster() &&
                              fStart.position() == fEnd.position(); }

        void setMetrics(const InternalLineMetrics& metrics) { fMetrics = metrics; }

        void extend(Cluster* cluster) {
            if (fStart.cluster() == nullptr) {
                fStart = ClusterPos(cluster, cluster->startPos());
            }
            fEnd = ClusterPos(cluster, cluster->endPos());
            // TODO: Make sure all the checks are correct and there are no unnecessary checks
            auto& r = cluster->run();
            if (!cluster->isHardBreak() && !r.isPlaceholder()) {
                // We ignore metrics for \n as the Flutter does
                fMetrics.add(&r);
            }
            fWidth += cluster->width();
        }

        void extend(Cluster* cluster, size_t pos) {
            fEnd = ClusterPos(cluster, pos);
            if (auto r = cluster->runOrNull()) {
                fMetrics.add(r);
            }
        }

        void startFrom(Cluster* cluster, size_t pos) {
            fStart = ClusterPos(cluster, pos);
            fEnd = ClusterPos(cluster, pos);
            if (auto r = cluster->runOrNull()) {
                // In case of placeholder we should ignore the default text style -
                // we will pick up the correct one from the placeholder
                if (!r->isPlaceholder()) {
                    fMetrics.add(r);
                }
            }
            fWidth = 0;
        }

        void saveBreak() {
            fWidthWithGhostSpaces = fWidth;
            fBreak = fEnd;
        }

        void restoreBreak() {
            fWidth = fWidthWithGhostSpaces;
            fEnd = fBreak;
        }

        void shiftBreak() {
            fBreak.move(true);
        }

        void trim() {

            if (fEnd.cluster() != nullptr &&
                fEnd.cluster()->owner() != nullptr &&
                fEnd.cluster()->runOrNull() != nullptr &&
                fEnd.cluster()->run().placeholderStyle() == nullptr &&
                fWidth > 0) {
                fWidth -= (fEnd.cluster()->width() - fEnd.cluster()->trimmedWidth(fEnd.position()));
            }
        }

        void trim(Cluster* cluster) {
            SkASSERT(fEnd.cluster() == cluster);
            if (fEnd.cluster() > fStart.cluster()) {
                fEnd.move(false);
                fWidth -= cluster->width();
            } else {
                fEnd.setPosition(fStart.position());
                fWidth = 0;
            }
        }

        void clean() {
            fStart.clean();
            fEnd.clean();
            fWidth = 0;
            fMetrics.clean();
        }

#ifdef ENABLE_TEXT_ENHANCE
        void shiftWidth(SkScalar width) {
            fWidth += width;
        }
#endif
    private:
        ClusterPos fStart;
        ClusterPos fEnd;
        ClusterPos fBreak;
        InternalLineMetrics fMetrics;
        SkScalar fWidth;
        SkScalar fWidthWithGhostSpaces;
    };

public:
    TextWrapper() {
         fLineNumber = 1;
         fHardLineBreak = false;
         fExceededMaxLines = false;
    }

    using AddLineToParagraph = std::function<void(TextRange textExcludingSpaces,
                                                  TextRange text,
                                                  TextRange textIncludingNewlines,
                                                  ClusterRange clusters,
                                                  ClusterRange clustersWithGhosts,
                                                  SkScalar AddLineToParagraph,
                                                  size_t startClip,
                                                  size_t endClip,
                                                  SkVector offset,
                                                  SkVector advance,
                                                  InternalLineMetrics metrics,
#ifdef ENABLE_TEXT_ENHANCE
                                                  bool addEllipsis,
                                                  SkScalar lineIndent,
                                                  SkScalar noIndentWidth)>;
#else
                                                  bool addEllipsis)>;
#endif
    void breakTextIntoLines(ParagraphImpl* parent,
                            SkScalar maxWidth,
                            const AddLineToParagraph& addLine);
#ifdef ENABLE_TEXT_ENHANCE
    void updateMetricsWithPlaceholder(std::vector<Run*>& runs, bool iterateByCluster);
    bool brokeLineWithHyphen() const { return fBrokeLineWithHyphen; }
#endif
    SkScalar height() const { return fHeight; }
    SkScalar minIntrinsicWidth() const { return fMinIntrinsicWidth; }
    SkScalar maxIntrinsicWidth() const { return fMaxIntrinsicWidth; }
    bool exceededMaxLines() const { return fExceededMaxLines; }

private:
#ifdef ENABLE_TEXT_ENHANCE
    struct FormattingContext {
        bool unlimitedLines{false};
        bool endlessLine{false};
        bool hasEllipsis{false};
        bool disableFirstAscent{false};
        bool disableLastDescent{false};
        size_t maxLines{0};
        TextAlign align{TextAlign::kLeft};
        bool needLineSpacing{false};
        SkScalar lineSpacing{0.0f};
    };

    struct LineTextRanges {
        TextRange textExcludingSpaces;
        TextRange text;
        TextRange textIncludingNewlines;
        ClusterRange clusters;
        ClusterRange clustersWithGhosts;
    };

    friend TextTabAlign;
#endif
    TextStretch fWords;
    TextStretch fClusters;
    TextStretch fClip;
    TextStretch fEndLine;
    size_t fLineNumber;
    bool fTooLongWord;
    bool fTooLongCluster;

    bool fHardLineBreak;
    bool fExceededMaxLines;

#ifdef ENABLE_TEXT_ENHANCE
    SkScalar fHeight{0};
    SkScalar fMinIntrinsicWidth{std::numeric_limits<SkScalar>::min()};
    SkScalar fMaxIntrinsicWidth{std::numeric_limits<SkScalar>::min()};
    bool fBrokeLineWithHyphen{false};
    std::vector<TextStretch> fWordStretches;
    std::vector<TextStretch> fLineStretches;
    std::vector<SkScalar> fWordWidthGroups;
    std::vector<std::vector<TextStretch>> fWordStretchesBatch;
    std::vector<std::vector<SkScalar>> fWordWidthGroupsBatch;
#else
    SkScalar fHeight;
    SkScalar fMinIntrinsicWidth;
    SkScalar fMaxIntrinsicWidth;
#endif

    void reset() {
        fWords.clean();
        fClusters.clean();
        fClip.clean();
        fTooLongCluster = false;
        fTooLongWord = false;
        fHardLineBreak = false;
#ifdef ENABLE_TEXT_ENHANCE
        fBrokeLineWithHyphen = false;
        fWordStretches.clear();
        fLineStretches.clear();
        fStart = nullptr;
        fEnd = nullptr;
#endif
    }

#ifdef ENABLE_TEXT_ENHANCE
    void lookAhead(SkScalar maxWidth, Cluster* endOfClusters, bool applyRoundingHack, WordBreakType wordBreakType,
                   bool needEllipsis);
    void moveForward(bool hasEllipsis, bool breakAll); // breakAll = true, break occurs after each character
    bool lookAheadByHyphen(Cluster* endOfClusters, SkScalar widthBeforeCluster, SkScalar maxWidth);
    uint64_t CalculateBestScore(std::vector<SkScalar>& widthOut,
        SkScalar maxWidth, ParagraphImpl* parent, size_t maxLines);
    static size_t tryBreakWord(Cluster* startCluster,
                               Cluster* endOfClusters,
                               SkScalar widthBeforeCluster,
                               SkScalar maxWidth);

    static void matchHyphenResult(const std::vector<uint8_t>& result, ParagraphImpl* owner, size_t& pos,
                                  SkScalar maxWidth, SkScalar length);
    static std::vector<uint8_t> findBreakPositions(Cluster* startCluster,
                                                   Cluster* endOfClusters,
                                                   SkScalar widthBeforeCluster,
                                                   SkScalar maxWidth);
    void initParent(ParagraphImpl* parent) { fParent = parent; }
    void pushToWordStretches();
    void pushToWordStretchesBatch();
    void layoutLinesBalanced(
        ParagraphImpl* parent, SkScalar maxWidth, const AddLineToParagraph& addLine);
    void layoutLinesSimple(ParagraphImpl* parent, SkScalar maxWidth, const AddLineToParagraph& addLine);
    bool isNewWidthToBeSetMax();
    std::vector<SkScalar> generateWordsWidthInfo(const std::vector<TextStretch>& wordStretches);
    std::vector<std::pair<size_t, size_t>> generateLinesGroupInfo(
        const std::vector<float>& clustersWidth, SkScalar maxWidth);
    void generateWordStretches(const SkSpan<Cluster>& span, WordBreakType wordBreakType);
    void generateLineStretches(const std::vector<std::pair<size_t, size_t>>& linesGroupInfo,
        std::vector<TextStretch>& wordStretches);
    void preProcessingForLineStretches();
    void handleMultiLineEllipsis(size_t maxLines);
    void extendCommonCluster(Cluster* cluster, TextTabAlign& textTabAlign,
        SkScalar& totalFakeSpacing, WordBreakType wordBreakType);
    SkScalar getTextStretchTrimmedEndSpaceWidth(const TextStretch& stretch);
    void formalizedClusters(std::vector<TextStretch>& clusters, SkScalar limitWidth);
    void generateTextLines(SkScalar maxWidth,
                            const AddLineToParagraph& addLine,
                            const SkSpan<Cluster>& span);
    void initializeFormattingState(SkScalar maxWidth, const SkSpan<Cluster>& span);
    void processLineStretches(SkScalar maxWidth, const AddLineToParagraph& addLine);
    void finalizeTextLayout(const AddLineToParagraph& addLine);
    void prepareLineForFormatting(TextStretch& line);
    void formatCurrentLine(const AddLineToParagraph& addLine);
    bool determineIfEllipsisNeeded();
    void trimLineSpaces();
    void handleSpecialCases(bool needEllipsis);
    void updateLineMetrics();
    void updatePlaceholderMetrics();
    void adjustLineMetricsForFirstLastLine();
    void applyStrutMetrics();
    LineTextRanges calculateLineTextRanges();
    SkScalar calculateLineHeight();
    void addFormattedLineToParagraph(const AddLineToParagraph& addLine, bool needEllipsis);
    void updateIntrinsicWidths();
    bool shouldBreakFormattingLoop();
    bool isLastLine() const;
    void advanceToNextLine();
    void prepareForNextLine();
    void processRemainingClusters();
    void handleHardBreak(float& lastWordLength);
    void handleWhitespaceBreak(Cluster* cluster, float& lastWordLength);
    void handlePlaceholder(Cluster* cluster, float& lastWordLength);
    void handleRegularCluster(Cluster* cluster, float& lastWordLength);
    void adjustMetricsForEmptyParagraph();
    void addFinalLineBreakIfNeeded(const AddLineToParagraph& addLine);
    void adjustFirstLastLineMetrics();
#else
    void lookAhead(SkScalar maxWidth, Cluster* endOfClusters, bool applyRoundingHack);
    void moveForward(bool hasEllipsis);
#endif
    void trimEndSpaces(TextAlign align);
    std::tuple<Cluster*, size_t, SkScalar> trimStartSpaces(Cluster* endOfClusters);
    SkScalar getClustersTrimmedWidth();

#ifdef ENABLE_TEXT_ENHANCE
    ParagraphImpl* fParent{nullptr};
    FormattingContext fFormattingContext;
    InternalLineMetrics fMaxRunMetrics;
    SkScalar fSoftLineMaxIntrinsicWidth{0.0f};
    SkScalar fCurrentLineWidthWithSpaces{0.0f};
    SkScalar fNoIndentWidth{0.0f};
    bool fFirstLine{false};
    Cluster* fCurrentStartLine{nullptr};
    size_t fCurrentStartPos{0};
    Cluster* fStart{nullptr};
    Cluster* fEnd{nullptr};
    bool fIsLastLine{false};
#endif
};
}  // namespace textlayout
}  // namespace skia

#endif  // TextWrapper_DEFINED
