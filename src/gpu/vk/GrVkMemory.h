/*
* Copyright 2015 Google Inc.
*
* Use of this source code is governed by a BSD-style license that can be
* found in the LICENSE file.
*/

#ifndef GrVkMemory_DEFINED
#define GrVkMemory_DEFINED

#include <map>
#include <mutex>

#include "include/gpu/vk/GrVkMemoryAllocator.h"
#include "include/gpu/vk/GrVkTypes.h"
#include "include/private/GrTypesPriv.h"
#include "include/private/SkTArray.h"

class GrVkGpu;

namespace GrVkMemory {
    class AsyncFreeVMAMemoryManager {
    public:
        struct WaitQueueItem {
            WaitQueueItem(const GrVkGpu* gpu, const GrVkAlloc& alloc, bool isBuffer)
                    : fGpu(gpu), fAlloc(alloc), fIsBuffer(isBuffer) {}

            const GrVkGpu* fGpu;
            const GrVkAlloc fAlloc;
            bool fIsBuffer = false;
        };
        struct FreeVMAMemoryWaitQueue {
            uint64_t fTotalFreedMemorySize = 0;
            std::vector<WaitQueueItem> fQueue;
        };

        static AsyncFreeVMAMemoryManager& GetInstance();
        AsyncFreeVMAMemoryManager(const AsyncFreeVMAMemoryManager&) = delete;
        AsyncFreeVMAMemoryManager& operator=(const AsyncFreeVMAMemoryManager&) = delete;

        void FreeMemoryInWaitQueue(std::function<bool(void)> nextFrameHasArrived);
        bool AddMemoryToWaitQueue(const GrVkGpu* gpu, const GrVkAlloc& alloc, bool isBuffer);

    private:
        AsyncFreeVMAMemoryManager();
        ~AsyncFreeVMAMemoryManager() = default;
        bool fAsyncFreedMemoryEnabled = false;
        const uint64_t fLimitFreedMemorySize = 15728640;
        const uint64_t fThresholdFreedMemorySize = 65536;
        std::vector<std::pair<pid_t, FreeVMAMemoryWaitQueue>> fWaitQueues;
        std::mutex fWaitQueuesLock;
    };
    /**
    * Allocates vulkan device memory and binds it to the gpu's device for the given object.
    * Returns true if allocation succeeded.
    */
    bool AllocAndBindBufferMemory(GrVkGpu* gpu,
                                  VkBuffer buffer,
                                  GrVkMemoryAllocator::BufferUsage,
                                  GrVkAlloc* alloc);

    bool ImportAndBindBufferMemory(GrVkGpu* gpu,
                                   OH_NativeBuffer *nativeBuffer,
                                   VkBuffer buffer,
                                   GrVkAlloc* alloc);

    void FreeBufferMemory(const GrVkGpu* gpu, const GrVkAlloc& alloc);

    bool AllocAndBindImageMemory(GrVkGpu* gpu,
                                 VkImage image,
                                 GrMemoryless,
                                 GrVkAlloc* alloc);
    void FreeImageMemory(const GrVkGpu* gpu, const GrVkAlloc& alloc);

    // Maps the entire GrVkAlloc and returns a pointer to the start of the allocation. Underneath
    // the hood, we may map more than the range of the GrVkAlloc (e.g. the entire VkDeviceMemory),
    // but the pointer returned will always be to the start of the GrVkAlloc. The caller should also
    // never assume more than the GrVkAlloc block has been mapped.
    void* MapAlloc(GrVkGpu* gpu, const GrVkAlloc& alloc);
    void UnmapAlloc(const GrVkGpu* gpu, const GrVkAlloc& alloc);

    // For the Flush and Invalidate calls, the offset should be relative to the GrVkAlloc. Thus this
    // will often be 0. The client does not need to make sure the offset and size are aligned to the
    // nonCoherentAtomSize, the internal calls will handle that.
    void FlushMappedAlloc(GrVkGpu* gpu, const GrVkAlloc& alloc, VkDeviceSize offset,
                          VkDeviceSize size);
    void InvalidateMappedAlloc(GrVkGpu* gpu, const GrVkAlloc& alloc, VkDeviceSize offset,
                               VkDeviceSize size);

    // Helper for aligning and setting VkMappedMemoryRange for flushing/invalidating noncoherent
    // memory.
    void GetNonCoherentMappedMemoryRange(const GrVkAlloc&, VkDeviceSize offset, VkDeviceSize size,
                                         VkDeviceSize alignment, VkMappedMemoryRange*);

    void AsyncFreeVMAMemoryBetweenFrames(std::function<bool(void)> nextFrameHasArrived);
}  // namespace GrVkMemory

#endif
