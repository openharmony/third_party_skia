/*
# Copyright (c) 2024 Huawei Device Co., Ltd.
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
*/

#ifndef GrVkMemoryReclaimer_DEFINED
#define GrVkMemoryReclaimer_DEFINED

#include "include/core/SkExecutor.h"
#include "include/gpu/vk/GrVkBackendContext.h"
#include "include/gpu/vk/GrVkTypes.h"
#include "src/gpu/GrGpu.h"
#include "src/gpu/GrStagingBufferManager.h"
#include "src/gpu/vk/GrVkCaps.h"
#include "src/gpu/vk/GrVkMSAALoadManager.h"
#include "src/gpu/vk/GrVkMemory.h"
#include "src/gpu/vk/GrVkResourceProvider.h"
#include "src/gpu/vk/GrVkSemaphore.h"
#include "src/gpu/vk/GrVkUtil.h"


class GrVkGpu;

enum class ItemType {
    BUFFER,
    IMAGE
}

class GrVkMemoryReclaimer {
public:
    struct WaitQueueItem {
        const GrVkGpu* fGpu;
        GrVkAlloc fAlloc;
        ItemType fType;
        union {
            GrVkBuffer fBuffer;
            GrVkImage fImage;
        };
    };
    GrVkMemoryReclaimer(): fExecutor(SkExecutor::MakeFIFOThreadPool(1, false)) {}
    ~GrVkMemoryReclaimer() = default;
    GrVkMemoryReclaimer(const GrVkMemoryReclaimer&) = delete;
    GrVkMemoryReclaimer& operator=(const GrVkMemoryReclaimer&) = delete;

    void setGpuCacheSuppressWindowSwitch(bool enabled);
    bool addMemoryToWaitQueue(const GrVkGpu* gpu, const GrVkAlloc& alloc, const VkBuffer& buffer);
    bool addMemoryToWaitQueue(const GrVkGpu* gpu, const GrVkAlloc& alloc, const GrVkImage& image);
    void flushGpuMemoryInWaitQueue();
private:
    void invokeParallelReclaiming();

    bool fEnabled = false;
    const int fMemoryCountThreshold = 50;

    std::vector<WaitQueueItem> fWaitQueues;

    std::unique_ptr<SkExecutor> fExecutor;

};

#endif
