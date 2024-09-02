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

#include "src/gpu/vk/GrVkMemoryReclaimer.h"

#include <parameters.h>

#include "GrVkBuffer.h"
#include "GrVkCommandBuffer.h"
#include "src/gpu/vk/GrVkGpu.h"
#include "src/gpu/vk/GrVkImageView.h"
#include "src/gpu/vk/GrVkMemory.h"
#include "src/gpu/vk/GrVkBuffer.h"
#include "src/gpu/vk/GrVkTexture.h"
#include "src/gpu/vk/GrVkUtil.h"

#define VK_CALL(GPU, X) GR_VK_CALL(GPU->vkInterface(), X)

bool GrVkMemoryReclaimer::addMemoryToWaitQueue(const GrVkGpu* gpu, const GrVkAlloc& alloc, const VkBuffer& buffer)
{
    if (!fEnabled) {
        return false;
    }
    fWaitQueues.emplace_back(WaitQueueItem{.fGpu = gpu, .fAlloc = alloc, .fType = ItemType::BUFFER, .fBuffer = buffer});
    if (fWaitQueues.size() > fMemoryCountThreshold) {
        invokeParallelReclaiming();
    }
    return true;
}

bool GrVkMemoryReclaimer::addMemoryToWaitQueue(const GrVkGpu* gpu, const GrVkAlloc& alloc, GrVkImage& image)
{
    if (!fEnabled) {
        return false;
    }
    fWaitQueues.emplace_back(WaitQueueItem{.fGpu = gpu, .fAlloc = alloc, .fType = ItemType::IMAGE, .fImage = image});
    if (fWaitQueues.size() > fMemoryCountThreshold) {
        invokeParallelReclaiming();
    }
    return true;
}

void GrVkMemoryReclaimer::flushGpuMemoryInWaitQueue()
{
    if (!fEnabled) {
        return;
    }
    if (!fWaitQueues.size()) {
        return;
    }
    invokeParallelReclaiming();
}

void GrVkMemoryReclaimer::nvokeParallelReclaiming()
{
    fExecutor->add([freeQueues {std::move(fWaitQueues)}] {
        for (auto& item : freeQueues) {
            if (item.fType == ItemType::BUFFER) {
                GrVkBuffer::DestroyAndFreeBufferMemory(item.fGpu, item.fAlloc, item.fBuffer);
            } else {
                GrVkImage::DestroyAndFreeImageMemory(item.fGpu, item.fAlloc, item.fImage);
            }
        }
    });
}
