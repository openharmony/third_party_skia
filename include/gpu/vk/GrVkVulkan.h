/*
 * Copyright 2018 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef GrVkVulkan_DEFINED
#define GrVkVulkan_DEFINED

#include "include/core/SkTypes.h"
#include "third_party/vulkan-headers/include/vulkan/vulkan_core.h"

#ifdef VK_USE_PLATFORM_OHOS
#include <vulkan/vulkan_ohos.h>
#endif

#ifdef SKIA_USE_XEG
#include "third_party/vulkan-headers/include/vulkan/vulkan_xeg.h"
#endif

#ifdef SK_BUILD_FOR_ANDROID
// This is needed to get android extensions for external memory
#if SKIA_IMPLEMENTATION || !defined(SK_VULKAN)
#include "include/third_party/vulkan/vulkan/vulkan_android.h"
#else
// For google3 builds we don't set SKIA_IMPLEMENTATION so we need to make sure that the vulkan
// headers stay up to date for our needs
#include <vulkan/vulkan_android.h>
#endif
#endif

#include "src/gpu/vk/vulkan_header_ext_huawei.h"

#endif
