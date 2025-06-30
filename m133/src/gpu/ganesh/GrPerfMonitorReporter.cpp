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

#include "src/gpu/ganesh/GrPerfMonitorReporter.h"
#ifdef NOT_BUILD_FOR_OHOS_SDK
#include <parameters.h>
#endif
#include "src/core/SkTraceEvent.h"

#ifdef SKIA_OHOS
const int32_t COUNTER_SIZE = 4;
const int32_t CACHE_SIZE = 5;
#endif

GrPerfMonitorReporter& GrPerfMonitorReporter::GetInstance() {
    static GrPerfMonitorReporter instance;
    return instance;
}

std::map<std::string, std::vector<uint16_t>> GrPerfMonitorReporter::getTextureStatsData() {
    std::lock_guard<std::mutex> guard(mtx);
    return fStatsTexture;
}

std::map<std::string, std::vector<uint16_t>> GrPerfMonitorReporter::getBlurStatsData() {
    std::lock_guard<std::mutex> guard(mtx);
    return fStatsBlur;
}

std::map<std::string, TextureEvent> GrPerfMonitorReporter::getTexturePerfEventData() {
    std::lock_guard<std::mutex> guard(mtx);
    return fTextureEvent;
}

std::map<std::string, BlurEvent> GrPerfMonitorReporter::getBlurPerfEventData() {
    std::lock_guard<std::mutex> guard(mtx);
    return fBlurEvent;
}

std::set<std::string> GrPerfMonitorReporter::getStatsCacheData() {
    std::lock_guard<std::mutex> guard(mtx);
    return fStatsCache;
}

void GrPerfMonitorReporter::resetStatsData() {
    std::lock_guard<std::mutex> guard(mtx);
    fStatsTexture.clear();
    fStatsBlur.clear();
    fStatsCache.clear();
}

void GrPerfMonitorReporter::resetPerfEventData() {
    std::lock_guard<std::mutex> guard(mtx);
    fTextureEvent.clear();
    fBlurEvent.clear();
}

void GrPerfMonitorReporter::recordTexturePerfEvent(const std::string& nodeName,
                                                   int32_t pid,
                                                   int32_t maxBytes,
                                                   int32_t budgetedBytes,
                                                   int64_t allocTime) {
#ifdef SKIA_OHOS
    if (!isOpenPerf() || nodeName.empty() || allocTime < COUNTER_MS_THIRD_TYPE) {
        return;
    }
    TextureEvent tEvent;
    tEvent.initEvent(pid, maxBytes, budgetedBytes, allocTime);
    tEvent.fClearCache = isClearCacheLastTime(nodeName);
    std::lock_guard<std::mutex> guard(mtx);
    fTextureEvent[nodeName] = tEvent;
#endif
}

void GrPerfMonitorReporter::recordBlurPerfEvent(const std::string& nodeName,
                                                int32_t pid,
                                                uint16_t filterType,
                                                float blurRadius,
                                                int32_t width,
                                                int32_t height,
                                                int64_t blurTime) {
#ifdef SKIA_OHOS
    if (!isOpenPerf() || nodeName.empty() || blurTime < COUNTER_MS_THIRD_TYPE) {
        return;
    }
    BlurEvent bEvent;
    bEvent.initEvent(pid, filterType, blurRadius, width, height, blurTime);
    std::lock_guard<std::mutex> guard(mtx);
    fBlurEvent[nodeName] = bEvent;
#endif
}

void GrPerfMonitorReporter::recordTextureNode(const std::string& nodeName, int64_t duration) {
#ifdef SKIA_OHOS
    if (!isOpenPerf() || nodeName.empty()) {
        return;
    }
    COUNTER_TYPE cType = static_cast<COUNTER_TYPE>(getSplitRange(duration));
    if (cType <= COUNTER_INVALID_TYPE) {
        return;
    }
    std::lock_guard<std::mutex> guard(mtx);
    if (fStatsTexture.find(nodeName) == fStatsTexture.end()) {
        fStatsTexture[nodeName] = std::vector<uint16_t>(COUNTER_SIZE, 0);
    }
    fStatsTexture[nodeName][cType]++;
#endif
}

void GrPerfMonitorReporter::recordBlurNode(const std::string& nodeName, int64_t duration) {
#ifdef SKIA_OHOS
    if (!isOpenPerf() || nodeName.empty()) {
        return;
    }
    COUNTER_TYPE cType = static_cast<COUNTER_TYPE>(getSplitRange(duration));
    if (cType <= COUNTER_INVALID_TYPE) {
        return;
    }
    std::lock_guard<std::mutex> guard(mtx);
    if (fStatsBlur.find(nodeName) == fStatsBlur.end()) {
        fStatsBlur[nodeName] = std::vector<uint16_t>(COUNTER_SIZE, 0);
    }
    fStatsBlur[nodeName][cType]++;
#endif
}

void GrPerfMonitorReporter::recordTextureCache(const std::string& nodeName) {
#ifdef SKIA_OHOS
    if (!isOpenPerf() || nodeName.empty()) {
        return;
    }
    std::lock_guard<std::mutex> guard(mtx);
    if (fStatsCache.size() >= CACHE_SIZE) {
        fStatsCache.erase(fStatsCache.begin());
    }
    fStatsCache.insert(nodeName);
#endif
}

int64_t GrPerfMonitorReporter::getCurrentTime() {
#ifdef SKIA_OHOS
    if (!isOpenPerf()) {
        return 0;
    }
    timespec t = {};
    clock_gettime(CLOCK_MONOTONIC, &t);
    return int64_t(t.tv_sec) * 1000000000LL + t.tv_nsec; // 1000000000ns == 1s
#else
    return 0;
#endif
}

int16_t GrPerfMonitorReporter::getSplitRange(int64_t duration) {
    if (duration < COUNTER_MS_FIRST_TYPE) {
        return COUNTER_INVALID_TYPE;
    }
    if (duration < COUNTER_MS_SECOND_TYPE) {
        return COUNTER_FIRST_TYPE;
    }
    if (duration < COUNTER_MS_THIRD_TYPE) {
        return COUNTER_SECOND_TYPE;
    }
    if (duration < COUNTER_MS_FORTH_TYPE) {
        return COUNTER_THIRD_TYPE;
    }
    return COUNTER_FORTH_TYPE;
}

bool GrPerfMonitorReporter::isOpenPerf() {
#ifdef NOT_BUILD_FOR_OHOS_SDK
    static bool isOpenPerf =
        OHOS::system::GetParameter("const.logsystem.versiontype", "beta") == "beta" &&
        OHOS::system::GetParameter("const.product.devicetype", "phone") == "phone";
    return isOpenPerf;
#else
    return false;
#endif
}

bool GrPerfMonitorReporter::isClearCacheLastTime(const std::string& nodeName) {
#ifdef SKIA_OHOS
    std::lock_guard<std::mutex> guard(mtx);
    return fStatsCache.find(nodeName) != fStatsCache.end();
#else
    return false;
#endif
}