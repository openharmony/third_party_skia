/*
* Copyright 2015 Google Inc.
*
* Use of this source code is governed by a BSD-style license that can be
* found in the LICENSE file.
*/
#include "include/core/SkExecutor.h"
#include "src/gpu/vk/GrVkMemory.h"

#include "src/gpu/vk/GrVkGpu.h"
#include "src/gpu/vk/GrVkUtil.h"

#ifdef SKIA_OHOS_FOR_OHOS_TRACE
#include "hitrace_meter.h"
#endif

#ifdef NOT_BUILD_FOR_OHOS_SDK
#include <parameters.h>
#endif

#define VK_CALL(GPU, X) GR_VK_CALL((GPU)->vkInterface(), X)

using AllocationPropertyFlags = GrVkMemoryAllocator::AllocationPropertyFlags;
using BufferUsage = GrVkMemoryAllocator::BufferUsage;

static std::unique_ptr<SkExecutor> executor = SkExecutor::MakeFIFOThreadPool(1, false);

static bool FindMemoryType(GrVkGpu *gpu, uint32_t typeFilter, VkMemoryPropertyFlags properties, uint32_t &typeIndex)
{
    VkPhysicalDevice physicalDevice = gpu->physicalDevice();
    VkPhysicalDeviceMemoryProperties memProperties{};
    VK_CALL(gpu, GetPhysicalDeviceMemoryProperties(physicalDevice, &memProperties));

    bool hasFound = false;
    for (uint32_t i = 0; i < memProperties.memoryTypeCount && !hasFound; ++i) {
        if (typeFilter & (1 << i)) {
            uint32_t supportedFlags = memProperties.memoryTypes[i].propertyFlags & properties;
            if (supportedFlags == properties) {
                typeIndex = i;
                hasFound = true;
            }
        }
    }

    return hasFound;
}

bool GrVkMemory::AllocAndBindBufferMemory(GrVkGpu* gpu,
                                          VkBuffer buffer,
                                          BufferUsage usage,
                                          GrVkAlloc* alloc) {
    GrVkMemoryAllocator* allocator = gpu->memoryAllocator();
    GrVkBackendMemory memory = 0;

    AllocationPropertyFlags propFlags;
    bool shouldPersistentlyMapCpuToGpu = gpu->vkCaps().shouldPersistentlyMapCpuToGpuBuffers();
    if (usage == BufferUsage::kTransfersFromCpuToGpu ||
        (usage == BufferUsage::kCpuWritesGpuReads && shouldPersistentlyMapCpuToGpu)) {
        // In general it is always fine (and often better) to keep buffers always mapped that we are
        // writing to on the cpu.
        propFlags = AllocationPropertyFlags::kPersistentlyMapped;
    } else {
        propFlags = AllocationPropertyFlags::kNone;
    }

    VkResult result = allocator->allocateBufferMemory(buffer, usage, propFlags, &memory);
    if (!gpu->checkVkResult(result)) {
        return false;
    }
    allocator->getAllocInfo(memory, alloc);

    // Bind buffer
    VkResult err;
    GR_VK_CALL_RESULT(gpu, err, BindBufferMemory(gpu->device(), buffer, alloc->fMemory,
                                                 alloc->fOffset));
    if (err) {
        FreeBufferMemory(gpu, *alloc);
        return false;
    }

    return true;
}

bool GrVkMemory::ImportAndBindBufferMemory(GrVkGpu* gpu,
                                           OH_NativeBuffer *nativeBuffer,
                                           VkBuffer buffer,
                                           GrVkAlloc* alloc) {
#ifdef SKIA_OHOS_FOR_OHOS_TRACE
    HITRACE_METER_FMT(HITRACE_TAG_GRAPHIC_AGP, "ImportAndBindBufferMemory");
#endif
    VkDevice device = gpu->device();
    VkMemoryRequirements memReqs{};
    VK_CALL(gpu, GetBufferMemoryRequirements(device, buffer, &memReqs));

    uint32_t typeIndex = 0;
    bool hasFound = FindMemoryType(gpu, memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, typeIndex);
    if (!hasFound) {
        return false;
    }

    // Import external memory
    VkImportNativeBufferInfoOHOS importInfo{};
    importInfo.sType = VK_STRUCTURE_TYPE_IMPORT_NATIVE_BUFFER_INFO_OHOS;
    importInfo.pNext = nullptr;
    importInfo.buffer = nativeBuffer;

    VkMemoryDedicatedAllocateInfo dedicatedAllocInfo{};
    dedicatedAllocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_DEDICATED_ALLOCATE_INFO;
    dedicatedAllocInfo.pNext = &importInfo;
    dedicatedAllocInfo.image = VK_NULL_HANDLE;
    dedicatedAllocInfo.buffer = buffer;

    VkMemoryAllocateInfo allocateInfo{};
    allocateInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocateInfo.pNext = &dedicatedAllocInfo;
    allocateInfo.allocationSize = memReqs.size;
    allocateInfo.memoryTypeIndex = typeIndex;

    VkResult err;
    VkDeviceMemory memory;
    GR_VK_CALL_RESULT(gpu, err, AllocateMemory(device, &allocateInfo, nullptr, &memory));
    if (err) {
        return false;
    }

    // Bind buffer
    GR_VK_CALL_RESULT(gpu, err, BindBufferMemory(device, buffer, memory, 0));
    if (err) {
        VK_CALL(gpu, FreeMemory(device, memory, nullptr));
        return false;
    }

    alloc->fMemory = memory;
    alloc->fOffset = 0;
    alloc->fSize = memReqs.size;
    alloc->fFlags = 0;
    alloc->fIsExternalMemory = true;

    return true;
}

void GrVkMemory::FreeBufferMemory(const GrVkGpu* gpu, const GrVkAlloc& alloc) {
    #ifdef NOT_BUILD_FOR_OHOS_SDK
    static bool asyncFreeVkMemoryEnabled = 
        (std::atoi(system::GetParameter("persist.sys.graphic.mem.async_free_enabled", "0").c_str()) != 0);
    #else
    static bool asyncFreeVkMemoryEnabled = false;
    #endif
    if (asyncFreeVkMemoryEnabled) {
        SkASSERT(alloc.fBackendMemory);
        executor->add([allocator = gpu->memoryAllocator(), backedMem = alloc.fBackendMemory] {
            allocator->freeMemory(backedMem);
        });
    } else {
        if (alloc.fIsExternalMemory) {
            VK_CALL(gpu, FreeMemory(gpu->device(), alloc.fMemory, nullptr));
        } else {
            SkASSERT(alloc.fBackendMemory);
            GrVkMemoryAllocator *allocator = gpu->memoryAllocator();
            allocator->freeMemory(alloc.fBackendMemory);
        }
    }
}

bool GrVkMemory::AllocAndBindImageMemory(GrVkGpu* gpu,
                                         VkImage image,
                                         GrMemoryless memoryless,
                                         GrVkAlloc* alloc) {
    GrVkMemoryAllocator* allocator = gpu->memoryAllocator();
    GrVkBackendMemory memory = 0;

    VkMemoryRequirements memReqs;
#ifdef SKIA_OHOS_FOR_OHOS_TRACE
    HITRACE_METER_FMT(HITRACE_TAG_GRAPHIC_AGP, "AllocAndBindImageMemory");
#endif
    GR_VK_CALL(gpu->vkInterface(), GetImageMemoryRequirements(gpu->device(), image, &memReqs));

    AllocationPropertyFlags propFlags;
    // If we ever find that our allocator is not aggressive enough in using dedicated image
    // memory we can add a size check here to force the use of dedicate memory. However for now,
    // we let the allocators decide. The allocator can query the GPU for each image to see if the
    // GPU recommends or requires the use of dedicated memory.
    if (gpu->vkCaps().shouldAlwaysUseDedicatedImageMemory()) {
        propFlags = AllocationPropertyFlags::kDedicatedAllocation;
    } else {
        propFlags = AllocationPropertyFlags::kNone;
    }

    if (gpu->protectedContext()) {
        propFlags |= AllocationPropertyFlags::kProtected;
    }

    if (memoryless == GrMemoryless::kYes) {
        propFlags |= AllocationPropertyFlags::kLazyAllocation;
    }

    VkResult result = allocator->allocateImageMemory(image, propFlags, &memory);
    if (!gpu->checkVkResult(result)) {
        return false;
    }

    allocator->getAllocInfo(memory, alloc);

    // Bind buffer
    VkResult err;
    GR_VK_CALL_RESULT(gpu, err, BindImageMemory(gpu->device(), image, alloc->fMemory,
                                                alloc->fOffset));
    if (err) {
        FreeImageMemory(gpu, *alloc);
        return false;
    }

    return true;
}

void GrVkMemory::FreeImageMemory(const GrVkGpu* gpu, const GrVkAlloc& alloc) {
    SkASSERT(alloc.fBackendMemory);
    #ifdef NOT_BUILD_FOR_OHOS_SDK
    static bool asyncFreeVkMemoryEnabled = 
        (std::atoi(system::GetParameter("persist.sys.graphic.mem.async_free_enabled", "0").c_str()) != 0);
    #else
    static bool asyncFreeVkMemoryEnabled = false;
    #endif
    if (asyncFreeVkMemoryEnabled) {
        executor->add([allocator = gpu->memoryAllocator(), backedMem = alloc.fBackendMemory] {
            allocator->freeMemory(backedMem);
        });
    } else {
        GrVkMemoryAllocator* allocator = gpu->memoryAllocator();
        allocator->freeMemory(alloc.fBackendMemory);
    }
}

void* GrVkMemory::MapAlloc(GrVkGpu* gpu, const GrVkAlloc& alloc) {
    SkASSERT(GrVkAlloc::kMappable_Flag & alloc.fFlags);
    SkASSERT(alloc.fBackendMemory);
    GrVkMemoryAllocator* allocator = gpu->memoryAllocator();
    void* mapPtr;
    VkResult result = allocator->mapMemory(alloc.fBackendMemory, &mapPtr);
    if (!gpu->checkVkResult(result)) {
        return nullptr;
    }
    return mapPtr;
}

void GrVkMemory::UnmapAlloc(const GrVkGpu* gpu, const GrVkAlloc& alloc) {
    SkASSERT(alloc.fBackendMemory);
    GrVkMemoryAllocator* allocator = gpu->memoryAllocator();
    allocator->unmapMemory(alloc.fBackendMemory);
}

void GrVkMemory::GetNonCoherentMappedMemoryRange(const GrVkAlloc& alloc, VkDeviceSize offset,
                                                 VkDeviceSize size, VkDeviceSize alignment,
                                                 VkMappedMemoryRange* range) {
    SkASSERT(alloc.fFlags & GrVkAlloc::kNoncoherent_Flag);
    offset = offset + alloc.fOffset;
    VkDeviceSize offsetDiff = offset & (alignment -1);
    offset = offset - offsetDiff;
    size = (size + alignment - 1) & ~(alignment - 1);
#ifdef SK_DEBUG
    SkASSERT(offset >= alloc.fOffset);
    SkASSERT(offset + size <= alloc.fOffset + alloc.fSize);
    SkASSERT(0 == (offset & (alignment-1)));
    SkASSERT(size > 0);
    SkASSERT(0 == (size & (alignment-1)));
#endif

    memset(range, 0, sizeof(VkMappedMemoryRange));
    range->sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE;
    range->memory = alloc.fMemory;
    range->offset = offset;
    range->size = size;
}

void GrVkMemory::FlushMappedAlloc(GrVkGpu* gpu, const GrVkAlloc& alloc, VkDeviceSize offset,
                                  VkDeviceSize size) {
    if (alloc.fFlags & GrVkAlloc::kNoncoherent_Flag) {
        SkASSERT(offset == 0);
        SkASSERT(size <= alloc.fSize);
        SkASSERT(alloc.fBackendMemory);
        GrVkMemoryAllocator* allocator = gpu->memoryAllocator();
        VkResult result = allocator->flushMemory(alloc.fBackendMemory, offset, size);
        gpu->checkVkResult(result);
    }
}

void GrVkMemory::InvalidateMappedAlloc(GrVkGpu* gpu, const GrVkAlloc& alloc,
                                       VkDeviceSize offset, VkDeviceSize size) {
    if (alloc.fFlags & GrVkAlloc::kNoncoherent_Flag) {
        SkASSERT(offset == 0);
        SkASSERT(size <= alloc.fSize);
        SkASSERT(alloc.fBackendMemory);
        GrVkMemoryAllocator* allocator = gpu->memoryAllocator();
        VkResult result = allocator->invalidateMemory(alloc.fBackendMemory, offset, size);
        gpu->checkVkResult(result);
    }
}

