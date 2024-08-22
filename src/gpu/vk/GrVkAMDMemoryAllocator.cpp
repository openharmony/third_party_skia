/*
 * Copyright 2018 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "src/gpu/vk/GrVkAMDMemoryAllocator.h"

#include "include/gpu/vk/GrVkExtensions.h"
#include "src/core/SkTraceEvent.h"
#include "src/gpu/vk/GrVkInterface.h"
#include "src/gpu/vk/GrVkMemory.h"
#include "src/gpu/vk/GrVkUtil.h"
#include "src/core/SkUtils.h"

#ifndef SK_USE_VMA
sk_sp<GrVkMemoryAllocator> GrVkAMDMemoryAllocator::Make(VkInstance instance,
                                                        VkPhysicalDevice physicalDevice,
                                                        VkDevice device,
                                                        uint32_t physicalDeviceVersion,
                                                        const GrVkExtensions* extensions,
                                                        sk_sp<const GrVkInterface> interface,
                                                        const GrVkCaps* caps,
                                                        bool cacheFlag,
                                                        size_t maxBlockCount) {
    return nullptr;
}
#else

sk_sp<GrVkMemoryAllocator> GrVkAMDMemoryAllocator::Make(VkInstance instance,
                                                        VkPhysicalDevice physicalDevice,
                                                        VkDevice device,
                                                        uint32_t physicalDeviceVersion,
                                                        const GrVkExtensions* extensions,
                                                        sk_sp<const GrVkInterface> interface,
                                                        const GrVkCaps* caps,
                                                        bool cacheFlag,
                                                        size_t maxBlockCount) {
#define GR_COPY_FUNCTION(NAME) functions.vk##NAME = interface->fFunctions.f##NAME
#define GR_COPY_FUNCTION_KHR(NAME) functions.vk##NAME##KHR = interface->fFunctions.f##NAME

    VmaVulkanFunctions functions;
    GR_COPY_FUNCTION(GetPhysicalDeviceProperties);
    GR_COPY_FUNCTION(GetPhysicalDeviceMemoryProperties);
    GR_COPY_FUNCTION(AllocateMemory);
    GR_COPY_FUNCTION(FreeMemory);
    GR_COPY_FUNCTION(MapMemory);
    GR_COPY_FUNCTION(UnmapMemory);
    GR_COPY_FUNCTION(FlushMappedMemoryRanges);
    GR_COPY_FUNCTION(InvalidateMappedMemoryRanges);
    GR_COPY_FUNCTION(BindBufferMemory);
    GR_COPY_FUNCTION(BindImageMemory);
    GR_COPY_FUNCTION(GetBufferMemoryRequirements);
    GR_COPY_FUNCTION(GetImageMemoryRequirements);
    GR_COPY_FUNCTION(CreateBuffer);
    GR_COPY_FUNCTION(DestroyBuffer);
    GR_COPY_FUNCTION(CreateImage);
    GR_COPY_FUNCTION(DestroyImage);
    GR_COPY_FUNCTION(CmdCopyBuffer);
    GR_COPY_FUNCTION_KHR(GetBufferMemoryRequirements2);
    GR_COPY_FUNCTION_KHR(GetImageMemoryRequirements2);
    GR_COPY_FUNCTION_KHR(BindBufferMemory2);
    GR_COPY_FUNCTION_KHR(BindImageMemory2);
    GR_COPY_FUNCTION_KHR(GetPhysicalDeviceMemoryProperties2);

    VmaAllocatorCreateInfo info;
    info.flags = VMA_ALLOCATOR_CREATE_EXTERNALLY_SYNCHRONIZED_BIT;
    if (physicalDeviceVersion >= VK_MAKE_VERSION(1, 1, 0) ||
        (extensions->hasExtension(VK_KHR_DEDICATED_ALLOCATION_EXTENSION_NAME, 1) &&
         extensions->hasExtension(VK_KHR_GET_MEMORY_REQUIREMENTS_2_EXTENSION_NAME, 1))) {
        info.flags |= VMA_ALLOCATOR_CREATE_KHR_DEDICATED_ALLOCATION_BIT;
    }

    info.physicalDevice = physicalDevice;
    info.device = device;
    // 4MB was picked for the size here by looking at memory usage of Android apps and runs of DM.
    // It seems to be a good compromise of not wasting unused allocated space and not making too
    // many small allocations. The AMD allocator will start making blocks at 1/8 the max size and
    // builds up block size as needed before capping at the max set here.
    if (cacheFlag) {
        info.preferredLargeHeapBlockSize = SkGetVmaBlockSizeMB() * 1024 * 1024;
    } else {
        info.preferredLargeHeapBlockSize = 4 * 1024 * 1024;
    }
    info.maxBlockCount = maxBlockCount;
    info.pAllocationCallbacks = nullptr;
    info.pDeviceMemoryCallbacks = nullptr;
    info.frameInUseCount = 0;
    info.pHeapSizeLimit = nullptr;
    info.pVulkanFunctions = &functions;
    info.pRecordSettings = nullptr;
    info.instance = instance;
    info.vulkanApiVersion = physicalDeviceVersion;

    VmaAllocator allocator;
    vmaCreateAllocator(&info, &allocator);

    return sk_sp<GrVkAMDMemoryAllocator>(new GrVkAMDMemoryAllocator(
            allocator, std::move(interface), caps->mustUseCoherentHostVisibleMemory()));
}

GrVkAMDMemoryAllocator::GrVkAMDMemoryAllocator(VmaAllocator allocator,
                                               sk_sp<const GrVkInterface> interface,
                                               bool mustUseCoherentHostVisibleMemory)
        : fAllocator(allocator)
        , fInterface(std::move(interface))
        , fMustUseCoherentHostVisibleMemory(mustUseCoherentHostVisibleMemory) {}

GrVkAMDMemoryAllocator::~GrVkAMDMemoryAllocator() {
    vmaDestroyAllocator(fAllocator);
    fAllocator = VK_NULL_HANDLE;
}

VkResult GrVkAMDMemoryAllocator::allocateImageMemory(VkImage image, AllocationPropertyFlags flags,
                                                     GrVkBackendMemory* backendMemory) {
    TRACE_EVENT0("skia.gpu", TRACE_FUNC);
    VmaAllocationCreateInfo info;
    info.flags = 0;
    info.usage = VMA_MEMORY_USAGE_UNKNOWN;
    info.requiredFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
    info.preferredFlags = 0;
    info.memoryTypeBits = 0;
    info.pool = VK_NULL_HANDLE;
    info.pUserData = nullptr;

    if (AllocationPropertyFlags::kDedicatedAllocation & flags) {
        info.flags |= VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT;
    }

    if (AllocationPropertyFlags::kLazyAllocation & flags) {
        info.requiredFlags |= VK_MEMORY_PROPERTY_LAZILY_ALLOCATED_BIT;
    }

    if (AllocationPropertyFlags::kProtected & flags) {
        info.requiredFlags |= VK_MEMORY_PROPERTY_PROTECTED_BIT;
    }

    VmaAllocation allocation;
    VkResult result = vmaAllocateMemoryForImage(fAllocator, image, &info, &allocation, nullptr);
    if (VK_SUCCESS == result) {
        *backendMemory = (GrVkBackendMemory)allocation;
    }
    return result;
}

VkResult GrVkAMDMemoryAllocator::allocateBufferMemory(VkBuffer buffer, BufferUsage usage,
                                                      AllocationPropertyFlags flags,
                                                      GrVkBackendMemory* backendMemory) {
    TRACE_EVENT0("skia.gpu", TRACE_FUNC);
    VmaAllocationCreateInfo info;
    info.flags = 0;
    info.usage = VMA_MEMORY_USAGE_UNKNOWN;
    info.memoryTypeBits = 0;
    info.pool = VK_NULL_HANDLE;
    info.pUserData = nullptr;

    switch (usage) {
        case BufferUsage::kGpuOnly:
            info.requiredFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
            info.preferredFlags = 0;
            break;
        case BufferUsage::kCpuWritesGpuReads:
            // When doing cpu writes and gpu reads the general rule of thumb is to use coherent
            // memory. Though this depends on the fact that we are not doing any cpu reads and the
            // cpu writes are sequential. For sparse writes we'd want cpu cached memory, however we
            // don't do these types of writes in Skia.
            //
            // TODO: In the future there may be times where specific types of memory could benefit
            // from a coherent and cached memory. Typically these allow for the gpu to read cpu
            // writes from the cache without needing to flush the writes throughout the cache. The
            // reverse is not true and GPU writes tend to invalidate the cache regardless. Also
            // these gpu cache read access are typically lower bandwidth than non-cached memory.
            // For now Skia doesn't really have a need or want of this type of memory. But if we
            // ever do we could pass in an AllocationPropertyFlag that requests the cached property.
            info.requiredFlags =
                    VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
            info.preferredFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
            break;
        case BufferUsage::kTransfersFromCpuToGpu:
            info.requiredFlags =
                    VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
            break;
        case BufferUsage::kTransfersFromGpuToCpu:
            info.requiredFlags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;
            info.preferredFlags = VK_MEMORY_PROPERTY_HOST_CACHED_BIT;
            break;
    }

    if (fMustUseCoherentHostVisibleMemory &&
        (info.requiredFlags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT)) {
        info.requiredFlags |= VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
    }

    if (AllocationPropertyFlags::kDedicatedAllocation & flags) {
        info.flags |= VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT;
    }

    if ((AllocationPropertyFlags::kLazyAllocation & flags) && BufferUsage::kGpuOnly == usage) {
        info.preferredFlags |= VK_MEMORY_PROPERTY_LAZILY_ALLOCATED_BIT;
    }

    if (AllocationPropertyFlags::kPersistentlyMapped & flags) {
        SkASSERT(BufferUsage::kGpuOnly != usage);
        info.flags |= VMA_ALLOCATION_CREATE_MAPPED_BIT;
    }

    VmaAllocation allocation;
    VkResult result = vmaAllocateMemoryForBuffer(fAllocator, buffer, &info, &allocation, nullptr);
    if (VK_SUCCESS == result) {
        *backendMemory = (GrVkBackendMemory)allocation;
    }

    return result;
}

void GrVkAMDMemoryAllocator::freeMemory(const GrVkBackendMemory& memoryHandle) {
    TRACE_EVENT0("skia.gpu", TRACE_FUNC);
    const VmaAllocation allocation = (const VmaAllocation)memoryHandle;
    vmaFreeMemory(fAllocator, allocation);
}

void GrVkAMDMemoryAllocator::getAllocInfo(const GrVkBackendMemory& memoryHandle,
                                          GrVkAlloc* alloc) const {
    const VmaAllocation allocation = (const VmaAllocation)memoryHandle;
    VmaAllocationInfo vmaInfo;
    vmaGetAllocationInfo(fAllocator, allocation, &vmaInfo);

    VkMemoryPropertyFlags memFlags;
    vmaGetMemoryTypeProperties(fAllocator, vmaInfo.memoryType, &memFlags);

    uint32_t flags = 0;
    if (VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT & memFlags) {
        flags |= GrVkAlloc::kMappable_Flag;
    }
    if (!SkToBool(VK_MEMORY_PROPERTY_HOST_COHERENT_BIT & memFlags)) {
        flags |= GrVkAlloc::kNoncoherent_Flag;
    }
    if (VK_MEMORY_PROPERTY_LAZILY_ALLOCATED_BIT & memFlags) {
        flags |= GrVkAlloc::kLazilyAllocated_Flag;
    }

    alloc->fMemory        = vmaInfo.deviceMemory;
    alloc->fOffset        = vmaInfo.offset;
    alloc->fSize          = vmaInfo.size;
    alloc->fFlags         = flags;
    alloc->fBackendMemory = memoryHandle;
    alloc->fAllocator     = (GrVkMemoryAllocator *)this;
}

VkResult GrVkAMDMemoryAllocator::mapMemory(const GrVkBackendMemory& memoryHandle, void** data) {
    TRACE_EVENT0("skia.gpu", TRACE_FUNC);
    const VmaAllocation allocation = (const VmaAllocation)memoryHandle;
    return vmaMapMemory(fAllocator, allocation, data);
}

void GrVkAMDMemoryAllocator::unmapMemory(const GrVkBackendMemory& memoryHandle) {
    TRACE_EVENT0("skia.gpu", TRACE_FUNC);
    const VmaAllocation allocation = (const VmaAllocation)memoryHandle;
    vmaUnmapMemory(fAllocator, allocation);
}

VkResult GrVkAMDMemoryAllocator::flushMemory(const GrVkBackendMemory& memoryHandle,
                                             VkDeviceSize offset, VkDeviceSize size) {
    TRACE_EVENT0("skia.gpu", TRACE_FUNC);
    const VmaAllocation allocation = (const VmaAllocation)memoryHandle;
    return vmaFlushAllocation(fAllocator, allocation, offset, size);
}

VkResult GrVkAMDMemoryAllocator::invalidateMemory(const GrVkBackendMemory& memoryHandle,
                                                  VkDeviceSize offset, VkDeviceSize size) {
    TRACE_EVENT0("skia.gpu", TRACE_FUNC);
    const VmaAllocation allocation = (const VmaAllocation)memoryHandle;
    return vmaInvalidateAllocation(fAllocator, allocation, offset, size);
}

uint64_t GrVkAMDMemoryAllocator::totalUsedMemory() const {
    VmaStats stats;
    vmaCalculateStats(fAllocator, &stats);
    return stats.total.usedBytes;
}

uint64_t GrVkAMDMemoryAllocator::totalAllocatedMemory() const {
    VmaStats stats;
    vmaCalculateStats(fAllocator, &stats);
    return stats.total.usedBytes + stats.total.unusedBytes;
}

void GrVkAMDMemoryAllocator::dumpVmaStats(SkString *out, const char *sep) const
{
    constexpr int MB = 1024 * 1024;
    if (out == nullptr || sep == nullptr) {
        return;
    }
    bool flag = SkGetMemoryOptimizedFlag();
    out->appendf("vma_flag: %d %s", flag, sep);
    if (!flag) {
        return;
    }
    VmaStats stats;
    vmaCalculateStats(fAllocator, &stats);
    uint64_t free = stats.total.unusedBytes;
    uint64_t used = stats.total.usedBytes;
    uint64_t total = free + used;
    auto maxBlockCount = SkGetVmaBlockCountMax();
    out->appendf("vma_free: %llu (%d MB)%s", free, free / MB, sep);
    out->appendf("vma_used: %llu (%d MB)%s", used, used / MB, sep);
    out->appendf("vma_total: %llu (%d MB)%s", total, total / MB, sep);
    out->appendf("vma_cacheBlockSize: %d MB%s", SkGetVmaBlockSizeMB(), sep);
    out->appendf("vma_cacheBlockCount: %llu / %llu%s",
        stats.total.blockCount <= maxBlockCount ? stats.total.blockCount : maxBlockCount, maxBlockCount, sep);
    out->appendf("vma_dedicatedBlockCount: %llu%s",
        stats.total.blockCount <= maxBlockCount ? 0 : stats.total.blockCount - maxBlockCount, sep);
    out->appendf("vma_allocationCount: %u%s", stats.total.allocationCount, sep);
    out->appendf("vma_unusedRangeCount: %u%s", stats.total.unusedRangeCount, sep);
    out->appendf("vma_allocationSize: %llu / %llu / %llu%s",
        stats.total.allocationSizeMin, stats.total.allocationSizeAvg, stats.total.allocationSizeMax, sep);
    out->appendf("vma_unusedRangeSize: %llu / %llu / %llu%s",
        stats.total.unusedRangeSizeMin, stats.total.unusedRangeSizeAvg, stats.total.unusedRangeSizeMax, sep);
}

void GrVkAMDMemoryAllocator::vmaDefragment()
{
    bool flag = SkGetVmaDefragmentOn();
    if (!flag) {
        return;
    }
    bool debugFlag = SkGetVmaDebugFlag();
    if (!debugFlag) {
        vmaFreeEmptyBlock(fAllocator);
        return;
    }

    // dfx
    SkString debugInfo;
    dumpVmaStats(&debugInfo);
    SkDebugf("GrVkAMDMemoryAllocator::vmaDefragment() before: %s",
        debugInfo.c_str());
    HITRACE_OHOS_NAME_FMT_ALWAYS("GrVkAMDMemoryAllocator::vmaDefragment() before: %s", debugInfo.c_str());

    vmaFreeEmptyBlock(fAllocator);

    // dfx
    debugInfo = "";
    dumpVmaStats(&debugInfo);
    SkDebugf("GrVkAMDMemoryAllocator::vmaDefragment() after: %s",
        debugInfo.c_str());
    HITRACE_OHOS_NAME_FMT_ALWAYS("GrVkAMDMemoryAllocator::vmaDefragment() after: %s", debugInfo.c_str());
}

#endif // SK_USE_VMA
