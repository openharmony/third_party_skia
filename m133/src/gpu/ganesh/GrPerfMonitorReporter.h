/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
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

#ifndef GrPerMonitorReporter_DEFINED
#define GrPerMonitorReporter_DEFINED

#include <ctime>
#include <map>
#include <set>
#include <vector>
#include <mutex>

#include "include/private/base/SkNoncopyable.h"

enum COUNTER_TYPE {
    COUNTER_INVALID_TYPE= -1,
    COUNTER_FIRST_TYPE,
    COUNTER_SECOND_TYPE,
    COUNTER_THIRD_TYPE,
    COUNTER_FORTH_TYPE,
};
enum COUNTER_TIME_LIMIT {
    COUNTER_MS_FIRST_TYPE = 1000000,
    COUNTER_MS_SECOND_TYPE = 3000000,
    COUNTER_MS_THIRD_TYPE = 5000000,
    COUNTER_MS_FORTH_TYPE = 8000000,
};

struct TextureEvent {
    void initEvent (int32_t pid, int32_t maxBytes, int32_t budgetedBytes, int64_t allocTime) {
        fPid = pid;
        fMaxBytes = maxBytes;
        fBudgetedBytes = budgetedBytes;
        fAllocTime = allocTime;
    }
    int32_t fPid {0};
    int32_t fMaxBytes {0};
    int32_t fBudgetedBytes {0};
    int64_t fAllocTime {0};
    bool fClearCache {false};
};

struct BlurEvent {
    void initEvent (int32_t pid, uint16_t filterType, float blurRadius, int32_t width,
        int32_t height, int64_t blurTime) {
        fPid = pid;
        fFilterType = filterType;
        fBlurRadius = blurRadius;
        fWidth = width;
        fHeight = height;
        fBlurTime = blurTime;
    }
    int32_t fPid {0};
    uint16_t fFilterType {0};
    float fBlurRadius {0.0};
    int32_t fWidth {0};
    int32_t fHeight {0};
    int64_t fBlurTime {0};
};

class GrPerfMonitorReporter {
public:
    SK_API static GrPerfMonitorReporter& GetInstance();

    SK_API std::map<std::string, std::vector<uint16_t>> getTextureStatsData();

    SK_API std::map<std::string, std::vector<uint16_t>> getBlurStatsData();

    SK_API std::map<std::string, TextureEvent> getTexturePerfEventData();

    SK_API std::map<std::string, BlurEvent> getBlurPerfEventData();

    SK_API std::set<std::string> getStatsCacheData();

    SK_API void resetPerfEventData();

    SK_API void resetStatsData();

    void recordTexturePerfEvent(const std::string& nodeName,
                                int32_t pid,
                                int32_t maxBytes,
                                int32_t budgetedBytes,
                                int64_t allocTime);
    void recordBlurPerfEvent(const std::string& nodeName,
                             int32_t pid,
                             uint16_t filterType,
                             float blurRadius,
                             int32_t width,
                             int32_t height,
                             int64_t blurTime);

    void recordTextureNode(const std::string& nodeName, int64_t duration);

    void recordBlurNode(const std::string& nodeName, int64_t duration);

    void recordTextureCache(const std::string& nodeName);

    SK_API static int64_t getCurrentTime();

    SK_API static int16_t getSplitRange(int64_t duration);

    SK_API static bool isOpenPerf();

private:
    bool isClearCacheLastTime(const std::string& nodeName);

    std::map<std::string, std::vector<uint16_t>> fStatsTexture;

    std::map<std::string, std::vector<uint16_t>> fStatsBlur;

    std::map<std::string, TextureEvent> fTextureEvent;

    std::map<std::string, BlurEvent> fBlurEvent;

    std::set<std::string> fStatsCache;

    std::mutex mtx;
};
#endif