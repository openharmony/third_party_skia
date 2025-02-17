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

namespace RealAllocConfig {
// OH ISSUE: isReAlloc indicates whether the Vulkan memory(external and proxy)
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