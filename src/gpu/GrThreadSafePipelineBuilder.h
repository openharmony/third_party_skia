/*
 * Copyright 2021 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef GrThreadSafePipelineBuilder_Base_DEFINED
#define GrThreadSafePipelineBuilder_Base_DEFINED

#include "include/core/SkRefCnt.h"
#include "include/gpu/GrConfig.h"
#include <array>
#include <atomic>

#if GR_TEST_UTILS
#include "include/private/SkTArray.h"
class SkString;
#endif

class GrThreadSafePipelineBuilder : public SkRefCnt {
public:
    GrThreadSafePipelineBuilder() = default;

    class Stats {
    public:
        enum class ProgramCacheResult {
            kHit,       // the program was found in the cache
            kMiss,      // the program was not found in the cache (and was, thus, compiled)
            kPartial,   // a precompiled version was found in the persistent cache

            kLast = kPartial
        };

#if GR_GPU_STATS
        static const int kNumProgramCacheResults = (int)ProgramCacheResult::kLast + 1;

        Stats() = default;

        int shaderCompilations() const { return fShaderCompilations; }
        void incShaderCompilations() { fShaderCompilations++; }

        int numInlineCompilationFailures() const { return fNumInlineCompilationFailures; }
        void incNumInlineCompilationFailures() { ++fNumInlineCompilationFailures; }

#ifdef SKIA_DFX_FOR_OHOS
        void updatePipelineCount(int count) { fPipelineCount = count; }
        void setPipelineCountMax(int max) { fPipelineCountMax = max; }
#endif

        int numInlineProgramCacheResult(ProgramCacheResult stat) const {
            return fInlineProgramCacheStats[(int) stat];
        }
        void incNumInlineProgramCacheResult(ProgramCacheResult stat) {
            ++fInlineProgramCacheStats[(int) stat];
        }

        int numPreCompilationFailures() const { return fNumPreCompilationFailures; }
        void incNumPreCompilationFailures() { ++fNumPreCompilationFailures; }

        int numPreProgramCacheResult(ProgramCacheResult stat) const {
            return fPreProgramCacheStats[(int) stat];
        }
        void incNumPreProgramCacheResult(ProgramCacheResult stat) {
            ++fPreProgramCacheStats[(int) stat];
        }

        int numCompilationFailures() const { return fNumCompilationFailures; }
        void incNumCompilationFailures() { ++fNumCompilationFailures; }

        int numPartialCompilationSuccesses() const { return fNumPartialCompilationSuccesses; }
        void incNumPartialCompilationSuccesses() { ++fNumPartialCompilationSuccesses; }

        int numCompilationSuccesses() const { return fNumCompilationSuccesses; }
        void incNumCompilationSuccesses() { ++fNumCompilationSuccesses; }

#if GR_TEST_UTILS
        void dump(SkString*);
        void dumpKeyValuePairs(SkTArray<SkString>* keys, SkTArray<double>* values);
#endif

    private:
        std::atomic<int> fShaderCompilations{0};
#ifdef SKIA_DFX_FOR_OHOS
        std::atomic<int> fPipelineCount{0};
        std::atomic<int> fPipelineCountMax{0};
#endif
        std::atomic<int> fNumInlineCompilationFailures{0};
        std::array<std::atomic<int>, kNumProgramCacheResults> fInlineProgramCacheStats = {};

        std::atomic<int> fNumPreCompilationFailures{0};
        std::array<std::atomic<int>, kNumProgramCacheResults> fPreProgramCacheStats = {};

        std::atomic<int> fNumCompilationFailures{0};
        std::atomic<int> fNumPartialCompilationSuccesses{0};
        std::atomic<int> fNumCompilationSuccesses{0};

#else
        void incShaderCompilations() {}
        void incNumInlineCompilationFailures() {}
        void incNumInlineProgramCacheResult(ProgramCacheResult stat) {}
        void incNumPreCompilationFailures() {}
        void incNumPreProgramCacheResult(ProgramCacheResult stat) {}
        void incNumCompilationFailures() {}
        void incNumPartialCompilationSuccesses() {}
        void incNumCompilationSuccesses() {}

#if GR_TEST_UTILS
        void dump(SkString*) {}
        void dumpKeyValuePairs(SkTArray<SkString>*, SkTArray<double>*) {}
#endif

#endif // GR_GPU_STATS
    };

    Stats* stats() { return &fStats; }

protected:
    Stats fStats;
};

#endif
