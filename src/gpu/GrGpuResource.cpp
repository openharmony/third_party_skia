/*
 * Copyright 2011 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "include/core/SkTraceMemoryDump.h"
#include "include/gpu/GrDirectContext.h"
#include "include/gpu/vk/GrVulkanTrackerInterface.h"
#include "src/gpu/GrDirectContextPriv.h"
#include "src/gpu/GrGpu.h"
#include "src/gpu/GrGpuResource.h"
#include "src/gpu/GrGpuResourcePriv.h"
#include "src/gpu/GrResourceCache.h"
#include <atomic>
#include "include/core/SkTypes.h"

static inline GrResourceCache* get_resource_cache(GrGpu* gpu) {
    SkASSERT(gpu);
    SkASSERT(gpu->getContext());
    SkASSERT(gpu->getContext()->priv().getResourceCache());
    return gpu->getContext()->priv().getResourceCache();
}

GrGpuResource::GrGpuResource(GrGpu* gpu) : fGpu(gpu), fUniqueID(CreateUniqueID()) {
    SkDEBUGCODE(fCacheArrayIndex = -1);
    auto cache = get_resource_cache(fGpu);
    if (cache) {
        fGrResourceTag = cache->resourceAccess().getCurrentGrResourceTag();
#ifdef SKIA_DFX_FOR_RECORD_VKIMAGE
        if (fGrResourceTag.fCid == 0) {
            fGrResourceTag.fCid = ParallelDebug::GetNodeId();
            if (fGrResourceTag.fWid == 0 && fGrResourceTag.fCid != 0) {
                int pidBits = 32;
                fGrResourceTag.fPid = static_cast<uint32_t>(fGrResourceTag.fCid >> pidBits);
            }
        }
#endif
    }
}

void GrGpuResource::registerWithCache(SkBudgeted budgeted) {
    SkASSERT(fBudgetedType == GrBudgetedType::kUnbudgetedUncacheable);
    fBudgetedType = budgeted == SkBudgeted::kYes ? GrBudgetedType::kBudgeted
                                                 : GrBudgetedType::kUnbudgetedUncacheable;
    this->computeScratchKey(&fScratchKey);
    get_resource_cache(fGpu)->resourceAccess().insertResource(this);
}

void GrGpuResource::registerWithCacheWrapped(GrWrapCacheable wrapType) {
    SkASSERT(fBudgetedType == GrBudgetedType::kUnbudgetedUncacheable);
    // Resources referencing wrapped objects are never budgeted. They may be cached or uncached.
    fBudgetedType = wrapType == GrWrapCacheable::kNo ? GrBudgetedType::kUnbudgetedUncacheable
                                                     : GrBudgetedType::kUnbudgetedCacheable;
    fRefsWrappedObjects = true;
    get_resource_cache(fGpu)->resourceAccess().insertResource(this);
}

GrGpuResource::~GrGpuResource() {
    // The cache should have released or destroyed this resource.
#ifdef SKIA_OHOS_SINGLE_OWNER
    static const bool isEnabledSkiaSingleOwner = GetEnableSkiaSingleOwner();
    static const int SIGNAL_FOR_OCEAN = 42;
    if (isEnabledSkiaSingleOwner && fTid != pthread_self()) {
        SK_LOGE("GrResource fTid[%{public}d]", fTid);
        raise(SIGNAL_FOR_OCEAN);
    }
#endif
    SkASSERT(this->wasDestroyed());
}

void GrGpuResource::release() {
    SkASSERT(fGpu);
    this->onRelease();
    get_resource_cache(fGpu)->resourceAccess().removeResource(this);
    fGpu = nullptr;
    fGpuMemorySize = 0;
}

void GrGpuResource::abandon() {
    if (this->wasDestroyed()) {
        return;
    }
    SkASSERT(fGpu);
    this->onAbandon();
    get_resource_cache(fGpu)->resourceAccess().removeResource(this);
    fGpu = nullptr;
    fGpuMemorySize = 0;
}

void GrGpuResource::dumpMemoryStatistics(SkTraceMemoryDump* traceMemoryDump) const {
    if (this->fRefsWrappedObjects && !traceMemoryDump->shouldDumpWrappedObjects()) {
        return;
    }

    this->dumpMemoryStatisticsPriv(traceMemoryDump, this->getResourceName(),
                                   this->getResourceType(), this->gpuMemorySize());
}

void GrGpuResource::dumpMemoryStatisticsPriv(SkTraceMemoryDump* traceMemoryDump,
                                             const SkString& resourceName,
                                             const char* type, size_t size) const {
    const char* tag = "Scratch";
    if (fUniqueKey.isValid()) {
        tag = (fUniqueKey.tag() != nullptr) ? fUniqueKey.tag() : "Other";
    }

    traceMemoryDump->dumpNumericValue(resourceName.c_str(), "size", "bytes", size);
    traceMemoryDump->dumpNumericValue(resourceName.c_str(), "source", "sourcetype", this->getResourceTag().fSid);
    traceMemoryDump->dumpStringValue(resourceName.c_str(), "type", type);
    traceMemoryDump->dumpStringValue(resourceName.c_str(), "category", tag);
    if (this->isPurgeable()) {
        traceMemoryDump->dumpNumericValue(resourceName.c_str(), "purgeable_size", "bytes", size);
    }
    if (traceMemoryDump->shouldDumpWrappedObjects()) {
        traceMemoryDump->dumpWrappedState(resourceName.c_str(), fRefsWrappedObjects);
    }

    this->setMemoryBacking(traceMemoryDump, resourceName);
}

bool GrGpuResource::isPurgeable() const {
    // Resources in the kUnbudgetedCacheable state are never purgeable when they have a unique
    // key. The key must be removed/invalidated to make them purgeable.
    return !this->hasRef() &&
           this->hasNoCommandBufferUsages() &&
           !(fBudgetedType == GrBudgetedType::kUnbudgetedCacheable && fUniqueKey.isValid());
}

bool GrGpuResource::hasRef() const { return this->internalHasRef(); }

bool GrGpuResource::hasNoCommandBufferUsages() const {
    return this->internalHasNoCommandBufferUsages();
}

SkString GrGpuResource::getResourceName() const {
    // Dump resource as "skia/gpu_resources/resource_#".
    SkString resourceName("skia/gpu_resources/resource_");
    resourceName.appendU32(this->uniqueID().asUInt());
    return resourceName;
}

const GrDirectContext* GrGpuResource::getContext() const {
    if (fGpu) {
        return fGpu->getContext();
    } else {
        return nullptr;
    }
}

GrDirectContext* GrGpuResource::getContext() {
    if (fGpu) {
        return fGpu->getContext();
    } else {
        return nullptr;
    }
}

void GrGpuResource::removeUniqueKey() {
    if (this->wasDestroyed()) {
        return;
    }
    SkASSERT(fUniqueKey.isValid());
    get_resource_cache(fGpu)->resourceAccess().removeUniqueKey(this);
}

void GrGpuResource::setUniqueKey(const GrUniqueKey& key) {
    SkASSERT(this->internalHasRef());
    SkASSERT(key.isValid());

    // Uncached resources can never have a unique key, unless they're wrapped resources. Wrapped
    // resources are a special case: the unique keys give us a weak ref so that we can reuse the
    // same resource (rather than re-wrapping). When a wrapped resource is no longer referenced,
    // it will always be released - it is never converted to a scratch resource.
    if (this->resourcePriv().budgetedType() != GrBudgetedType::kBudgeted &&
        !this->fRefsWrappedObjects) {
        return;
    }

    if (this->wasDestroyed()) {
        return;
    }

    get_resource_cache(fGpu)->resourceAccess().changeUniqueKey(this, key);
}

void GrGpuResource::notifyARefCntIsZero(LastRemovedRef removedRef) const {
    if (this->wasDestroyed()) {
        if (this->hasNoCommandBufferUsages() && !this->hasRef()) {
            // We've already been removed from the cache. Goodbye cruel world!
            delete this;
        }
        return;
    }

    GrGpuResource* mutableThis = const_cast<GrGpuResource*>(this);

    get_resource_cache(fGpu)->resourceAccess().notifyARefCntReachedZero(mutableThis, removedRef);
}

void GrGpuResource::removeScratchKey() {
    if (!this->wasDestroyed() && fScratchKey.isValid()) {
        get_resource_cache(fGpu)->resourceAccess().willRemoveScratchKey(this);
        fScratchKey.reset();
    }
}

void GrGpuResource::makeBudgeted() {
    // We should never make a wrapped resource budgeted.
    SkASSERT(!fRefsWrappedObjects);
    // Only wrapped resources can be in the kUnbudgetedCacheable state.
    SkASSERT(fBudgetedType != GrBudgetedType::kUnbudgetedCacheable);
    if (!this->wasDestroyed() && fBudgetedType == GrBudgetedType::kUnbudgetedUncacheable) {
        // Currently resources referencing wrapped objects are not budgeted.
        fBudgetedType = GrBudgetedType::kBudgeted;
        get_resource_cache(fGpu)->resourceAccess().didChangeBudgetStatus(this);
    }
}

void GrGpuResource::makeUnbudgeted() {
    if (!this->wasDestroyed() && fBudgetedType == GrBudgetedType::kBudgeted &&
        !fUniqueKey.isValid()) {
        fBudgetedType = GrBudgetedType::kUnbudgetedUncacheable;
        get_resource_cache(fGpu)->resourceAccess().didChangeBudgetStatus(this);
    }
}

void GrGpuResource::userRegisterResource()
{
    if (this->wasDestroyed()) {
        return;
    }
    SkASSERT(!fScratchKey.isValid());
    SkASSERT(!fUniqueKey.isValid());
    if (fCacheArrayIndex >= 0 && fBudgetedType == GrBudgetedType::kUnbudgetedUncacheable) {
        this->computeScratchKey(&fScratchKey);
        makeBudgeted();
    }
}

uint32_t GrGpuResource::CreateUniqueID() {
    static std::atomic<uint32_t> nextID{1};
    uint32_t id;
    do {
        id = nextID.fetch_add(1, std::memory_order_relaxed);
    } while (id == SK_InvalidUniqueID);
    return id;
}

void GrGpuResource::setResourceTag(const GrGpuResourceTag tag, bool curRealAlloc)
{
    int32_t pid = fGrResourceTag.fPid;
    fGrResourceTag = tag;
    if ((pid == tag.fPid) && (fRealAlloc == curRealAlloc)) {
        return;
    }
    size_t size = this->gpuMemorySize();
    get_resource_cache(fGpu)->resourceAccess().changeByteOfPid(pid, tag.fPid, size, fRealAlloc, curRealAlloc);
    fRealAlloc = curRealAlloc;
}

//////////////////////////////////////////////////////////////////////////////

void GrGpuResource::ProxyAccess::ref(GrResourceCache* cache) {
    SkASSERT(cache == fResource->getContext()->priv().getResourceCache());
    cache->resourceAccess().refResource(fResource);
}
