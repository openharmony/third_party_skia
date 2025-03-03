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

#include "include/gpu/vk/GrVkGraphicCoreTrace.h"
#include "src/gpu/vk/GrVkImage.h"

#include <parameters.h>

namespace GraphicCoreTrace {

static thread_local GpuResourceDfx VkImageCaller_;

void GpuResourceDfx::Dump(std::stringstream& ss)
{
    ss << " " << BitsetToHex();
}

void GpuResourceDfx::Reset()
{
    coreTrace_.reset();
}

std::string GpuResourceDfx::BitsetToHex()
{
    constexpr int bitsPerHexChar = 4;
    constexpr int hexBaseOffset = 10;
    static_assert(MAX_CORE_FUNCTION_TYPES % bitsPerHexChar == 0,
                  "MAX_CORE_FUNCTION_TYPES must be a multiple of 4.");
    std::string hexStr;
    unsigned int value;

    for (int i = MAX_CORE_FUNCTION_TYPES - 1; i >= 0; i -= bitsPerHexChar) {
        value = 0;
        for (int j = 0; j < bitsPerHexChar; j++) {
            if (coreTrace_[i - j]) {
                value |= (1 << j);
            }
        }
        if (value < hexBaseOffset) {
            hexStr = static_cast<char>('0' + value) + hexStr;
        } else {
            hexStr = static_cast<char>('A' + value - hexBaseOffset) + hexStr;
        }
    }
    return hexStr;
}

void RecordCoreTrace(int functionType)
{
    VkImageCaller_.coreTrace_.set(functionType);
}

void RecordCoreTrace(int functionType, uint64_t nodeId)
{
    VkImageCaller_.coreTrace_.set(functionType);
    VkImageCaller_.nodeId_ = nodeId;
}

GpuResourceDfx* GenerateGpuResourceDfx()
{
    auto caller = new GpuResourceDfx();
    caller->coreTrace_ = VkImageCaller_.coreTrace_;
    caller->nodeId_ = VkImageCaller_.nodeId_;
    VkImageCaller_.Reset();
    return caller;
}

uint64_t GetNodeId()
{
    return VkImageCaller_.nodeId_;
}

CoreTrace GetCoreTrace()
{
    return VkImageCaller_.coreTrace_;
}

void RecordEntireCoreTrace(CoreTrace coretrace, uint64_t nodeId)
{
    VkImageCaller_.coreTrace_ = coretrace;
    VkImageCaller_.nodeId_ = nodeId;
}

void DestroyGpuResourceDfx(GpuResourceDfx* gpuResourceDfx)
{
    delete gpuResourceDfx;
}

}