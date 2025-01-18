/*
 * Copyright 2014 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef GrGpuResourceCacheAccess_DEFINED
#define GrGpuResourceCacheAccess_DEFINED

#include "src/gpu/GrGpuResource.h"
#include "src/gpu/GrGpuResourcePriv.h"

namespace skiatest {
    class Reporter;
}  // namespace skiatest

static inline bool IsValidAddress(GrGpuResource* ptr)
{
#if defined(__aarch64__)
    static constexpr uint64_t HWASAN_HEADER = 0xFF00000000000000u;
    static constexpr uint64_t HIGH_BOUND = 0x8000000000u;
    static constexpr uint64_t LOW_BOUND = 0x1000u;
    uint64_t memory = reinterpret_cast<uint64_t>(ptr);
    uint64_t real = (memory & ~HWASAN_HEADER);
    return (LOW_BOUND < real) && (real < HIGH_BOUND) && ptr->checkMagic();
#else
    return true;
#endif
}

/**
 * This class allows GrResourceCache increased privileged access to GrGpuResource objects.
 */
class GrGpuResource::CacheAccess {
private:
    /** The cache is allowed to go from no refs to 1 ref. */
    void ref() { if(IsValidAddress(fResource)) fResource->addInitialRef(); }

    /**
     * Is the resource currently cached as scratch? This means it is cached, has a valid scratch
     * key, and does not have a unique key.
     */
    bool isScratch() const {
        return IsValidAddress(fResource) && !fResource->getUniqueKey().isValid() && fResource->fScratchKey.isValid() &&
               GrBudgetedType::kBudgeted == fResource->resourcePriv().budgetedType();
    }

    bool isUsableAsScratch() const {
        return this->isScratch() && !fResource->internalHasRef();
    }

    /**
     * Called by the cache to delete the resource under normal circumstances.
     */
    void release() {
        if (!IsValidAddress(fResource)) {
            return;
        }
        fResource->release();
        if (!fResource->hasRef() && fResource->hasNoCommandBufferUsages()) {
            if (!IsValidAddress(fResource)) {
                return;
            }
            delete fResource;
            fResource = nullptr;
        }
    }

    /**
     * Called by the cache to delete the resource when the backend 3D context is no longer valid.
     */
    void abandon() {
        if (!IsValidAddress(fResource)) {
            return;
        }
        fResource->abandon();
        if (!fResource->hasRef() && fResource->hasNoCommandBufferUsages()) {
            if (!IsValidAddress(fResource)) {
                return;
            }
            delete fResource;
        }
    }

    /** Called by the cache to assign a new unique key. */
    void setUniqueKey(const GrUniqueKey& key) { if (IsValidAddress(fResource)) fResource->fUniqueKey = key; }

    /** Is the resource ref'ed */
    bool hasRef() const { return IsValidAddress(fResource) && fResource->hasRef(); }
    bool hasRefOrCommandBufferUsage() const {
        return this->hasRef() || (IsValidAddress(fResource) && !fResource->hasNoCommandBufferUsages());
    }

    /** Called by the cache to make the unique key invalid. */
    void removeUniqueKey() {if (IsValidAddress(fResource)) fResource->fUniqueKey.reset(); }

    uint32_t timestamp() const { return IsValidAddress(fResource) ? fResource->fTimestamp : 0; }
    void setTimestamp(uint32_t ts) { if(IsValidAddress(fResource)) fResource->fTimestamp = ts; }

    void setTimeWhenResourceBecomePurgeable() {
        SkASSERT(fResource->isPurgeable());
        if (IsValidAddress(fResource)) {
            fResource->fTimeWhenBecamePurgeable = GrStdSteadyClock::now();
        }
    }
    /**
     * Called by the cache to determine whether this resource should be purged based on the length
     * of time it has been available for purging.
     */
    GrStdSteadyClock::time_point timeWhenResourceBecamePurgeable() {
        SkASSERT(fResource->isPurgeable());
        return IsValidAddress(fResource) ? fResource->fTimeWhenBecamePurgeable : GrStdSteadyClock::now();
    }

    int* accessCacheIndex() const {
        thread_local int IN_VALID_INDEX = 0;
        return IsValidAddress(fResource) ? &fResource->fCacheArrayIndex : &IN_VALID_INDEX;
    }

    CacheAccess(GrGpuResource* resource) : fResource(resource) {}
    CacheAccess(const CacheAccess& that) : fResource(that.fResource) {}
    CacheAccess& operator=(const CacheAccess&) = delete;

    // No taking addresses of this type.
    const CacheAccess* operator&() const = delete;
    CacheAccess* operator&() = delete;

    GrGpuResource* fResource;

    friend class GrGpuResource; // to construct/copy this type.
    friend class GrResourceCache; // to use this type
    friend void test_unbudgeted_to_scratch(skiatest::Reporter* reporter); // for unit testing
};

inline GrGpuResource::CacheAccess GrGpuResource::cacheAccess() { return CacheAccess(this); }

inline const GrGpuResource::CacheAccess GrGpuResource::cacheAccess() const {  // NOLINT(readability-const-return-type)
    return CacheAccess(const_cast<GrGpuResource*>(this));
}

#endif
