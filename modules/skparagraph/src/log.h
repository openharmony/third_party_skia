/*
 * Copyright (c) 2024 Huawei Device Co., Ltd.
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

#ifndef LOG_DEFINED
#define LOG_DEFINED

#ifdef USE_SKIA_TXT
#include <hilog/log.h>

#undef LOG_DOMAIN
#define LOG_DOMAIN 0xD001408
#undef LOG_TAG
#define LOG_TAG "Text"

#define TEXT_LOG_LIMIT_HOURS 3600
#define TEXT_LOG_LIMIT_MINUTE 60
#define TEXT_LOG_LIMIT_PRINT_FREQUENCY 3

#define LOGD(fmt, ...) HILOG_DEBUG(LOG_CORE, "%{public}s: " fmt, __func__, ##__VA_ARGS__)
#define LOGI(fmt, ...) HILOG_INFO(LOG_CORE, "%{public}s: " fmt, __func__, ##__VA_ARGS__)
#define LOGW(fmt, ...) HILOG_WARN(LOG_CORE, "%{public}s: " fmt, __func__, ##__VA_ARGS__)
#define LOGE(fmt, ...) HILOG_ERROR(LOG_CORE, "%{public}s: " fmt, __func__, ##__VA_ARGS__)

#define TEXT_LOGD(fmt, ...) HILOG_DEBUG(LOG_CORE, "%{public}s: " fmt, __func__, ##__VA_ARGS__)
#define TEXT_LOGI(fmt, ...) HILOG_INFO(LOG_CORE, "%{public}s: " fmt, __func__, ##__VA_ARGS__)
#define TEXT_LOGW(fmt, ...) HILOG_WARN(LOG_CORE, "%{public}s: " fmt, __func__, ##__VA_ARGS__)
#define TEXT_LOGE(fmt, ...) HILOG_ERROR(LOG_CORE, "%{public}s: " fmt, __func__, ##__VA_ARGS__)

#define TEXT_PRINT_LIMIT(type, level, intervals, canPrint, frequency)                       \
    do {                                                                                    \
        static auto last = std::chrono::time_point<std::chrono::system_clock, std::chrono::seconds>(); \
        static uint32_t supressed = 0;                                                      \
        static int printCount = 0;                                                          \
        auto now = std::chrono::time_point_cast<std::chrono::seconds>(std::chrono::system_clock::now()); \
        auto duration = now - last;                                                         \
        if (duration.count() >= (intervals)) {                                              \
            last = now;                                                                     \
            uint32_t supressedCnt = supressed;                                              \
            supressed = 0;                                                                  \
            printCount = 1;                                                                 \
            if (supressedCnt != 0) {                                                        \
                ((void)HILOG_IMPL((type), (level), LOG_DOMAIN, LOG_TAG,                     \
                    "%{public}s log suppressed cnt %{public}u", __func__, supressedCnt));   \
            }                                                                               \
            (canPrint) = true;                                                              \
        } else {                                                                            \
            if ((printCount++) < (frequency)) {                                             \
                (canPrint) = true;                                                          \
            } else {                                                                        \
                supressed++;                                                                \
                (canPrint) = false;                                                         \
            }                                                                               \
        }                                                                                   \
    } while (0)

#define TEXT_LOGI_LIMIT3_HOUR(fmt, ...)                                      \
    do {                                                                     \
        bool canPrint = true;                                                \
        LOGD(fmt, ##__VA_ARGS__);                                            \
        TEXT_PRINT_LIMIT(LOG_CORE, LOG_INFO, TEXT_LOG_LIMIT_HOURS, canPrint, TEXT_LOG_LIMIT_PRINT_FREQUENCY); \
        if (canPrint) {                                                      \
            LOGI(fmt, ##__VA_ARGS__);                                        \
        }                                                                    \
    } while (0)

#define TEXT_LOGW_LIMIT3_HOUR(fmt, ...)                                      \
    do {                                                                     \
        bool canPrint = true;                                                \
        LOGD(fmt, ##__VA_ARGS__);                                            \
        TEXT_PRINT_LIMIT(LOG_CORE, LOG_WARN, TEXT_LOG_LIMIT_HOURS, canPrint, TEXT_LOG_LIMIT_PRINT_FREQUENCY); \
        if (canPrint) {                                                      \
            LOGW(fmt, ##__VA_ARGS__);                                        \
        }                                                                    \
    } while (0)

#define TEXT_LOGE_LIMIT3_HOUR(fmt, ...)                                      \
    do {                                                                     \
        bool canPrint = true;                                                \
        LOGD(fmt, ##__VA_ARGS__);                                            \
        TEXT_PRINT_LIMIT(LOG_CORE, LOG_ERROR, TEXT_LOG_LIMIT_HOURS, canPrint, TEXT_LOG_LIMIT_PRINT_FREQUENCY); \
        if (canPrint) {                                                      \
            LOGE(fmt, ##__VA_ARGS__);                                        \
        }                                                                    \
    } while (0)

#define TEXT_LOGI_LIMIT3_MIN(fmt, ...)                                       \
    do {                                                                     \
        bool canPrint = true;                                                \
        LOGD(fmt, ##__VA_ARGS__);                                            \
        TEXT_PRINT_LIMIT(LOG_CORE, LOG_INFO, TEXT_LOG_LIMIT_MINUTE, canPrint, TEXT_LOG_LIMIT_PRINT_FREQUENCY); \
        if (canPrint) {                                                      \
            LOGI(fmt, ##__VA_ARGS__);                                        \
        }                                                                    \
    } while (0)

#define TEXT_LOGW_LIMIT3_MIN(fmt, ...)                                       \
    do {                                                                     \
        bool canPrint = true;                                                \
        LOGD(fmt, ##__VA_ARGS__);                                            \
        TEXT_PRINT_LIMIT(LOG_CORE, LOG_WARN, TEXT_LOG_LIMIT_MINUTE, canPrint, TEXT_LOG_LIMIT_PRINT_FREQUENCY); \
        if (canPrint) {                                                      \
            LOGW(fmt, ##__VA_ARGS__);                                        \
        }                                                                    \
    } while (0)

#define TEXT_LOGE_LIMIT3_MIN(fmt, ...)                                       \
    do {                                                                     \
        bool canPrint = true;                                                \
        LOGD(fmt, ##__VA_ARGS__);                                            \
        TEXT_PRINT_LIMIT(LOG_CORE, LOG_ERROR, TEXT_LOG_LIMIT_MINUTE, canPrint, TEXT_LOG_LIMIT_PRINT_FREQUENCY); \
        if (canPrint) {                                                      \
            LOGE(fmt, ##__VA_ARGS__);                                        \
        }                                                                    \
    } while (0)

#else

#define LOGD(fmt, ...)
#define LOGI(fmt, ...)
#define LOGW(fmt, ...)
#define LOGE(fmt, ...)

#define TEXT_LOGD(fmt, ...)
#define TEXT_LOGI(fmt, ...)
#define TEXT_LOGW(fmt, ...)
#define TEXT_LOGE(fmt, ...)

#define TEXT_LOGI_LIMIT3_HOUR(fmt, ...)
#define TEXT_LOGW_LIMIT3_HOUR(fmt, ...)
#define TEXT_LOGE_LIMIT3_HOUR(fmt, ...)

#define TEXT_LOGI_LIMIT3_MIN(fmt, ...)
#define TEXT_LOGW_LIMIT3_MIN(fmt, ...)
#define TEXT_LOGE_LIMIT3_MIN(fmt, ...)

#endif // USE_SKIA_TXT
#endif // LOG_DEFINED
