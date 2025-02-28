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

#include "include/gpu/vk/GrVulkanTracker.h"
#include "include/gpu/vk/GrVulkanTrackerInterface.h"
#include "src/gpu/vk/GrVkImage.h"
#include <deque>
#include <parameters.h>

static thread_local ParallelDebug::VkImageInvokeRecord CALLER_;
static thread_local std::deque<ParallelDebug::VkImageDestroyRecord> DELETE_;
static std::atomic<int64_t> count;

static inline int64_t GetNanoSecords()
{
    struct timespec ts {};
    clock_gettime(CLOCK_REALTIME, &ts);
    return ts.tv_sec * 100000000LL + ts.tv_nsec;
}

bool ParallelDebug::IsVkImageDfxEnabled()
{
    static const bool dfxEnabled =
        std::atoi(OHOS::system::GetParameter("persist.sys.graphic.openVkImageMemoryDfx", "0").c_str()) != 0;
    return dfxEnabled;
}

uint64_t ParallelDebug::GetNodeId()
{
    return CALLER_.nodeId_;
}

void ParallelDebug::RecordNodeId(uint64_t nodeId)
{
    CALLER_.nodeId_ = nodeId;
}

ParallelDebug::VkImageInvokeRecord* ParallelDebug::GenerateVkImageInvokeRecord()
{
    auto caller = new ParallelDebug::VkImageInvokeRecord();
    caller->nodeId_ = CALLER_.nodeId_;
    return caller;
}

void ParallelDebug::DestroyVkImageInvokeRecord(ParallelDebug::VkImageInvokeRecord* vkImageInvokeRecord)
{
    delete vkImageInvokeRecord;
}

void ParallelDebug::VkImageInvokeRecord::Dump(std::stringstream& ss)
{
    ss << (nodeId_ ? ", nodeId: " + std::to_string(nodeId_) : "");
}

void ParallelDebug::VkImageDestroyRecord::Record(VkImage image,
                                                bool borrow,
                                                const ParallelDebug::VkImageInvokeRecord* call,
                                                VkDeviceMemory fMemory)
{
    static constexpr size_t N = 100;
    DELETE_.push_back({image, borrow, *call, fMemory, GetNanoSecords()});
    if (DELETE_.size() > N) {
        DELETE_.pop_front();
    }
}

void ParallelDebug::DumpAllDestroyVkImage(std::stringstream &ss)
{
    for (auto& del : DELETE_) {
        del.Dump(ss);
        ss << "\n";
    }
}

void ParallelDebug::VkImageDestroyRecord::Dump(std::stringstream& ss)
{
    char timeStr[20];
    struct tm timeInfo;
    time_t seconds = timeStamp / 1000000000LL;
    localtime_r(&seconds, &timeInfo);
    std::strftime(timeStr, sizeof(timeStr), "%Y-%m-%d %H:%M:%S", &timeInfo);
    ss << timeStr << "VkImage: " << image_ << ", " << borrowed_ << ", " << memory_ << " ";
    caller_.Dump(ss);
}

namespace RealAllocConfig {
// OH ISSUE: isRealAlloc indicates whether the Vulkan memory(external and proxy)
// in the current thread context should be calculated.
static thread_local bool isRealAlloc = false;

bool GetRealAllocStatus()
{
    return isRealAlloc;
}

void SetRealAllocStatus(bool ret)
{
    isRealAlloc = ret;
}
}