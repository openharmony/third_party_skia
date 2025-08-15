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

#ifndef GrVulkanTrackerInterface_DEFINED 
#define GrVulkanTrackerInterface_DEFINED
#include <sstream>

#if defined (SK_VULKAN) && defined (SKIA_DFX_FOR_RECORD_VKIMAGE)
#define RECORD_GPU_RESOURCE_DRAWABLE_CALLER(NodeId) ParallelDebug::RecordNodeId(NodeId)
#else
#define RECORD_GPU_RESOURCE_DRAWABLE_CALLER(NodeId)
#endif
namespace ParallelDebug {
struct VkImageInvokeRecord;
SK_API bool IsVkImageDfxEnabled();
SK_API void RecordNodeId(uint64_t id);
void DestroyVkImageInvokeRecord(VkImageInvokeRecord*);
void DumpAllDestroyVkImage(std::stringstream& ss);
VkImageInvokeRecord* GenerateVkImageInvokeRecord();
uint64_t GetNodeId();
}

// OH ISSUE if real alloc status is setted to true, vulkan memory will be accounted for MemorySnapshot
// Supported interfaces: makeImageSnapshot, MakeFromBackendTexture
// Constraint: Only effective in the current thread context.
#if defined (SK_VULKAN) && defined (SKIA_DFX_FOR_OHOS)
#define REAL_ALLOC_CONFIG_SET_STATUS(stat) RealAllocConfig::SetRealAllocStatus(stat)
#else
#define REAL_ALLOC_CONFIG_SET_STATUS(stat)
#endif

namespace RealAllocConfig {
SK_API bool GetRealAllocStatus();
SK_API void SetRealAllocStatus(bool ret);
}
#endif