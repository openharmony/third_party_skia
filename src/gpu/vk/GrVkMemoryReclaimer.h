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

#ifndef GrVkMemoryReclaimer_DEFINED
#define GrVkMemoryReclaimer_DEFINED

#include "include/gpu/vk/GrVkTypes.h"

#include "include/core/SkExecutor.h"

#include "src/gpu/vk/GrVkImage.h"
#include "src/gpu/vk/GrVkImageView.h"
#include "src/gpu/vk/GrVkBuffer.h"

enum class ItemType {
    BUFFER,
    IMAGE,
    IMAGE_VIEW
};

class GrVkMemoryReclaimer {
public:
    struct WaitQueueItem {
        const GrVkGpu* fGpu;
        GrVkAlloc fAlloc;
        ItemType fType;
        union {
            VkBuffer fBuffer;
            VkImage fImage;
            VkImageView fImageView;
        };
    };

    SkExecutor& getThreadPool();
    GrVkMemoryReclaimer(bool enabled, const std::function<void()>& setThreadPriority);
    bool addMemoryToWaitQueue(const GrVkGpu* gpu, const GrVkAlloc& alloc, const VkBuffer& buffer);
    bool addMemoryToWaitQueue(const GrVkGpu* gpu, const GrVkAlloc& alloc, const VkImage& image);
    bool addMemoryToWaitQueue(const GrVkGpu* gpu, const VkImageView& imageView);
    void flushGpuMemoryInWaitQueue();
private:
    void invokeParallelReclaiming();

    bool fEnabled = false;
    const int fMemoryCountThreshold = 50;

    std::vector<WaitQueueItem> fWaitQueues;

    const std::function<void()> fSetThreadPriority;
};

#endif
