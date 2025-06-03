/*
 * Copyright 2017 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef VkTestUtils_DEFINED
#define VkTestUtils_DEFINED

#include "include/core/SkTypes.h"

#ifdef SK_VULKAN

#include "include/gpu/vk/VulkanBackendContext.h"
#include "tools/gpu/vk/GrVulkanDefines.h"
#include <functional>

namespace skgpu {
struct VulkanBackendContext;
class VulkanExtensions;
}

namespace sk_gpu_test {
    bool SK_API LoadVkLibraryAndGetProcAddrFuncs(PFN_vkGetInstanceProcAddr*);

    using CanPresentFn = std::function<bool(VkInstance, VkPhysicalDevice,
                                            uint32_t queueFamilyIndex)>;

    bool SK_API CreateVkBackendContext(PFN_vkGetInstanceProcAddr getInstProc,
                                skgpu::VulkanBackendContext* ctx,
                                skgpu::VulkanExtensions*,
                                VkPhysicalDeviceFeatures2*,
                                VkDebugReportCallbackEXT* debugCallback,
                                uint32_t* presentQueueIndexPtr = nullptr,
                                const CanPresentFn& canPresent = CanPresentFn(),
                                bool isProtected = false);

    void SK_API FreeVulkanFeaturesStructs(const VkPhysicalDeviceFeatures2*);

}  // namespace sk_gpu_test

#endif
#endif
