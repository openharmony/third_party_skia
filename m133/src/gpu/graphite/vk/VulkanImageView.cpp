/*
* Copyright 2023 Google Inc.
*
* Use of this source code is governed by a BSD-style license that can be
* found in the LICENSE file.
*/

#include "src/gpu/graphite/vk/VulkanImageView.h"

#include "src/gpu/graphite/vk/VulkanGraphiteUtilsPriv.h"
#include "src/gpu/graphite/vk/VulkanResourceProvider.h"
#include "src/gpu/graphite/vk/VulkanSharedContext.h"
#include "src/gpu/graphite/vk/VulkanYcbcrConversion.h"

namespace skgpu::graphite {

std::unique_ptr<const VulkanImageView> VulkanImageView::Make(
        const VulkanSharedContext* sharedCtx,
        VkImage image,
        VkFormat format,
        Usage usage,
        uint32_t miplevels,
        sk_sp<VulkanYcbcrConversion> ycbcrConversion) {

    void* pNext = nullptr;
    VkSamplerYcbcrConversionInfo conversionInfo;
    if (ycbcrConversion) {
        conversionInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_YCBCR_CONVERSION_INFO;
        conversionInfo.pNext = nullptr;
        conversionInfo.conversion = ycbcrConversion->ycbcrConversion();
        pNext = &conversionInfo;
    }

    VkImageViewASTCDecodeModeEXT astcDecodeMode;
    if (format == VK_FORMAT_ASTC_4x4_UNORM_BLOCK || format == VK_FORMAT_ASTC_6x6_UNORM_BLOCK ||
        format == VK_FORMAT_ASTC_8x8_UNORM_BLOCK) {
        astcDecodeMode.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_ASTC_DECODE_MODE_EXT;
        astcDecodeMode.pNext = nullptr;
        astcDecodeMode.decodeMode = VK_FORMAT_R8G8B8A8_UNORM;
        pNext = &astcDecodeMode;
    }

    VkImageView imageView;
    // Create the VkImageView
    VkImageAspectFlags aspectFlags;
    if (Usage::kAttachment == usage) {
        switch (format) {
        case VK_FORMAT_S8_UINT:
            aspectFlags = VK_IMAGE_ASPECT_STENCIL_BIT;
            break;
        case VK_FORMAT_D16_UNORM:
        case VK_FORMAT_D32_SFLOAT:
            aspectFlags = VK_IMAGE_ASPECT_DEPTH_BIT;
            break;
        case VK_FORMAT_D24_UNORM_S8_UINT:
        case VK_FORMAT_D32_SFLOAT_S8_UINT:
            aspectFlags = VK_IMAGE_ASPECT_STENCIL_BIT | VK_IMAGE_ASPECT_DEPTH_BIT;
            break;
        default:
            aspectFlags = VK_IMAGE_ASPECT_COLOR_BIT;
            break;
        }
        // Attachments can only expose the top level MIP
        miplevels = 1;
    } else {
        aspectFlags = VK_IMAGE_ASPECT_COLOR_BIT;
    }
    VkImageViewCreateInfo viewInfo = {
        VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,               // sType
        pNext,                                                  // pNext
        0,                                                      // flags
        image,                                                  // image
        VK_IMAGE_VIEW_TYPE_2D,                                  // viewType
        format,                                                 // format
        { VK_COMPONENT_SWIZZLE_IDENTITY,
          VK_COMPONENT_SWIZZLE_IDENTITY,
          VK_COMPONENT_SWIZZLE_IDENTITY,
          VK_COMPONENT_SWIZZLE_IDENTITY },                      // components
        { aspectFlags, 0, miplevels, 0, 1 },                    // subresourceRange
    };

    VkResult result;
    VULKAN_CALL_RESULT(sharedCtx,
                       result,
                       CreateImageView(sharedCtx->device(), &viewInfo, nullptr, &imageView));
    if (result != VK_SUCCESS) {
        return nullptr;
    }

    return std::unique_ptr<VulkanImageView>(new VulkanImageView(sharedCtx, imageView, usage,
                                                                ycbcrConversion));
}

VulkanImageView::VulkanImageView(const VulkanSharedContext* sharedContext,
                                 VkImageView imageView,
                                 Usage usage,
                                 sk_sp<VulkanYcbcrConversion> ycbcrConversion)
    : fSharedContext(sharedContext)
    , fImageView(imageView)
    , fUsage(usage)
    , fYcbcrConversion(std::move(ycbcrConversion)) {}

VulkanImageView::~VulkanImageView() {
    VULKAN_CALL(fSharedContext->interface(),
                DestroyImageView(fSharedContext->device(), fImageView, nullptr));
}

}  // namespace skgpu::graphite
