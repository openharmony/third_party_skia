/*
 * Copyright (c) 2025 Huawei Device Co., Ltd.
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

#include "src/gpu/ganesh/vk/GrVulkanTracker.h"

#include "include/gpu/vk/GrVulkanTrackerInterface.h"

#include <deque>
#include <parameters.h>

static thread_local ParallelDebug::VkImageInvokeRecord g_caller;
static thread_local std::deque<ParallelDebug::VkImageDestroyRecord> g_delete;
static constexpr size_t DESTROY_RECORD_CAPACITY = 100;

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
    return g_caller.nodeId_;
}

void ParallelDebug::RecordNodeId(uint64_t nodeId)
{
    g_caller.nodeId_ = nodeId;
}

ParallelDebug::VkImageInvokeRecord* ParallelDebug::GenerateVkImageInvokeRecord()
{
    auto caller = new ParallelDebug::VkImageInvokeRecord();
    caller->nodeId_ = g_caller.nodeId_;
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
                                                VkDeviceMemory memory)
{
    if (call == nullptr) {
        return;
    }
    g_delete.push_back({image, borrow, *call, memory, GetNanoSecords()});
    if (g_delete.size() > DESTROY_RECORD_CAPACITY) {
        g_delete.pop_front();
    }
}

void ParallelDebug::DumpAllDestroyVkImage(std::stringstream &ss)
{
    for (auto& del : g_delete) {
        del.Dump(ss);
        ss << "\n";
    }
}

void ParallelDebug::VkImageDestroyRecord::Dump(std::stringstream& ss)
{
    char timeStr[20];
    struct tm timeInfo;
    time_t seconds = timeStamp_ / 1000000000LL;
    localtime_r(&seconds, &timeInfo);
    std::strftime(timeStr, sizeof(timeStr), "%Y-%m-%d %H:%M:%S", &timeInfo);
    ss << timeStr << "VkImage: " << image_ << ", " << borrowed_ << ", " << memory_ << " ";
    caller_.Dump(ss);
}
