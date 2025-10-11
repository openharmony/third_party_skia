/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
 * Description: DFX for Image.
 */

#ifndef skgpu_ImageStat_DEFINED
#define skgpu_ImageStat_DEFINED

#include <csignal>
#include <map>
#ifdef SKIA_OHOS
#include <sys/types.h>
#include <unistd.h>
#endif
#include "include/core/SkImage.h"
#include "include/core/SkLog.h"
#include "include/private/base/SkAssert.h"
#include "include/private/base/SkDebug.h"
#include "include/private/base/SkMutex.h"

namespace skgpu {
#ifdef SKIA_OHOS
static const bool isBeta = GetEnableSkiaSingleOwner();
#else
static const bool isBeta = false;
#endif
static const int SIGNAL_FOR_OCEAN = 42;
class ImageStat {
public:
    ImageStat(const ImageStat&) = delete;
    ImageStat& operator=(const ImageStat&) = delete;

    static ImageStat& GetInstance()
    {
        static ImageStat instance;
        return instance;
    }

    void CheckAndInsert(const SkImage* image)
    {
        SkAutoMutexExclusive lock(fMutex);
#ifdef SKIA_OHOS
        int currentTid = gettid();
#else
        int currentTid = 0;
#endif
        auto it = imageMap.find(image);
        if (it != imageMap.end()) {
            SK_LOGE("Duplicate SkImage detected! Current TID: %{public}d, Previous TID: %{public}d",
                currentTid, it->second);
            if (isBeta) {
                raise(SIGNAL_FOR_OCEAN);
            }
        } else {
            imageMap[image] = currentTid;
        }
    }

    void Erase(const SkImage* image)
    {
        SkAutoMutexExclusive lock(fMutex);
        imageMap.erase(image);
    }
private:
    ImageStat() = default;

    SkMutex fMutex;
    std::map<const SkImage*, int> imageMap;
};
} // namespace skgpu
#endif