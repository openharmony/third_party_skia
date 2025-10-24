/*
 * Copyright 2014 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "src/gpu/ganesh/GrResourceCache.h"

#include "include/core/SkString.h"
#include "include/gpu/ganesh/GrDirectContext.h"
#include "include/gpu/ganesh/GrTypes.h"
#include "include/private/base/SingleOwner.h"
#include "include/private/base/SkNoncopyable.h"
#include "include/private/base/SkTo.h"
#include "src/base/SkMathPriv.h"
#include "src/base/SkRandom.h"
#include "src/base/SkTSort.h"
#include "src/core/SkStringUtils.h"
#include "src/core/SkMessageBus.h"
#include "src/core/SkTraceEvent.h"
#include "src/gpu/ganesh/GrDirectContextPriv.h"
#include "src/gpu/ganesh/GrGpuResourceCacheAccess.h"
#include "src/gpu/ganesh/GrProxyProvider.h"
#ifdef SKIA_OHOS
#include "src/gpu/ganesh/GrPerfMonitorReporter.h"
#endif
#include "src/gpu/ganesh/GrThreadSafeCache.h"

#include <algorithm>
#include <chrono>
#include <cstring>
#include <vector>
#ifdef SKIA_DFX_FOR_OHOS
#include <sstream>
#include <iostream>
#endif

using namespace skia_private;

DECLARE_SKMESSAGEBUS_MESSAGE(skgpu::UniqueKeyInvalidatedMessage, uint32_t, true)

DECLARE_SKMESSAGEBUS_MESSAGE(GrResourceCache::UnrefResourceMessage,
                             GrDirectContext::DirectContextID,
                             /*AllowCopyableMessage=*/false)

#if defined(SKIA_OHOS_SINGLE_OWNER)
#define ASSERT_SINGLE_OWNER SKGPU_ASSERT_SINGLE_OWNER_OHOS(fSingleOwner)
#else
#define ASSERT_SINGLE_OWNER SKGPU_ASSERT_SINGLE_OWNER(fSingleOwner)
#endif

//////////////////////////////////////////////////////////////////////////////

class GrResourceCache::AutoValidate : ::SkNoncopyable {
public:
    AutoValidate(GrResourceCache* cache) : fCache(cache) { cache->validate(); }
    ~AutoValidate() { fCache->validate(); }
private:
    GrResourceCache* fCache;
};

//////////////////////////////////////////////////////////////////////////////

GrResourceCache::GrResourceCache(skgpu::SingleOwner* singleOwner,
                                 GrDirectContext::DirectContextID owningContextID,
                                 uint32_t familyID)
        : fInvalidUniqueKeyInbox(familyID)
        , fUnrefResourceInbox(owningContextID)
        , fOwningContextID(owningContextID)
        , fContextUniqueID(familyID)
        , fSingleOwner(singleOwner) {
    SkASSERT(owningContextID.isValid());
    SkASSERT(familyID != SK_InvalidUniqueID);
}

GrResourceCache::~GrResourceCache() {
    this->releaseAll();
}

void GrResourceCache::setLimit(size_t bytes) {
    fMaxBytes = bytes;
    this->purgeAsNeeded();
}

#ifdef SKIA_DFX_FOR_OHOS
static constexpr int MB = 1024 * 1024;

#ifdef SKIA_OHOS
bool GrResourceCache::purgeUnlocakedResTraceEnabled_ =
#ifndef SKIA_OHOS_DEBUG
    false;
#else
    std::atoi((OHOS::system::GetParameter("sys.graphic.skia.cache.debug", "0").c_str())) == 1;
#endif
#endif

void GrResourceCache::dumpInfo(SkString* out) {
    if (out == nullptr) {
        SkDebugf("OHOS GrResourceCache::dumpInfo outPtr is nullptr!");
        return;
    }
    auto info = cacheInfo();
    constexpr uint8_t STEP_INDEX = 1;
    TArray<SkString> lines;
    SkStrSplit(info.substr(STEP_INDEX, info.length() - STEP_INDEX).c_str(), ";", &lines);
    for (int i = 0; i < lines.size(); ++i) {
        out->appendf("    %s\n", lines[i].c_str());
    }
}

std::string GrResourceCache::cacheInfo()
{
    auto fPurgeableQueueInfoStr = cacheInfoPurgeableQueue();
    auto fNonpurgeableResourcesInfoStr = cacheInfoNoPurgeableQueue();

    std::ostringstream cacheInfoStream;
    cacheInfoStream << "[fPurgeableQueueInfoStr.count : " << fPurgeableQueue.count()
        << "; fNonpurgeableResources.count : " << fNonpurgeableResources.size()
        << "; fBudgetedBytes : " << fBudgetedBytes
        << "(" << static_cast<size_t>(fBudgetedBytes / MB)
        << " MB) / " << fMaxBytes
        << "(" << static_cast<size_t>(fMaxBytes / MB)
        << " MB); fBudgetedCount : " << fBudgetedCount
        << "; fBytes : " << fBytes
        << "(" << static_cast<size_t>(fBytes / MB)
        << " MB); fPurgeableBytes : " << fPurgeableBytes
        << "(" << static_cast<size_t>(fPurgeableBytes / MB)
        << " MB); fAllocImageBytes : " << fAllocImageBytes
        << "(" << static_cast<size_t>(fAllocImageBytes / MB)
        << " MB); fAllocBufferBytes : " << fAllocBufferBytes
        << "(" << static_cast<size_t>(fAllocBufferBytes / MB)
        << " MB); fTimestamp : " << fTimestamp
        << "; " << fPurgeableQueueInfoStr << "; " << fNonpurgeableResourcesInfoStr;
    return cacheInfoStream.str();
}

#ifdef SKIA_OHOS
void GrResourceCache::traceBeforePurgeUnlockRes(const std::string& method, SimpleCacheInfo& simpleCacheInfo)
{
    if (purgeUnlocakedResTraceEnabled_) {
        StartTrace(HITRACE_TAG_GRAPHIC_AGP, method + " begin cacheInfo = " + cacheInfo());
    } else {
        simpleCacheInfo.fPurgeableQueueCount = fPurgeableQueue.count();
        simpleCacheInfo.fNonpurgeableResourcesCount = fNonpurgeableResources.size();
        simpleCacheInfo.fPurgeableBytes = fPurgeableBytes;
        simpleCacheInfo.fBudgetedCount = fBudgetedCount;
        simpleCacheInfo.fBudgetedBytes = fBudgetedBytes;
        simpleCacheInfo.fAllocImageBytes = fAllocImageBytes;
        simpleCacheInfo.fAllocBufferBytes = fAllocBufferBytes;
    }
}

void GrResourceCache::traceAfterPurgeUnlockRes(const std::string& method, const SimpleCacheInfo& simpleCacheInfo)
{
#ifdef SKIA_OHOS_FOR_OHOS_TRACE
    if (purgeUnlocakedResTraceEnabled_) {
        HITRACE_OHOS_NAME_FMT_ALWAYS("%s end cacheInfo = %s", method.c_str(), cacheInfo().c_str());
        FinishTrace(HITRACE_TAG_GRAPHIC_AGP);
    } else {
        HITRACE_OHOS_NAME_FMT_ALWAYS("%s end cacheInfo = %s",
            method.c_str(), cacheInfoComparison(simpleCacheInfo).c_str());
    }
#endif
}

std::string GrResourceCache::cacheInfoComparison(const SimpleCacheInfo& simpleCacheInfo)
{
    std::ostringstream cacheInfoComparison;
    cacheInfoComparison << "PurgeableCount : " << simpleCacheInfo.fPurgeableQueueCount
        << " / " << fPurgeableQueue.count()
        << "; NonpurgeableCount : " << simpleCacheInfo.fNonpurgeableResourcesCount
        << " / " << fNonpurgeableResources.size()
        << "; PurgeableBytes : " << simpleCacheInfo.fPurgeableBytes << " / " << fPurgeableBytes
        << "; BudgetedCount : " << simpleCacheInfo.fBudgetedCount << " / " << fBudgetedCount
        << "; BudgetedBytes : " << simpleCacheInfo.fBudgetedBytes << " / " << fBudgetedBytes
        << "; AllocImageBytes : " << simpleCacheInfo.fAllocImageBytes << " / " << fAllocImageBytes
        << "; AllocBufferBytes : " << simpleCacheInfo.fAllocBufferBytes << " / " << fAllocBufferBytes;
    return cacheInfoComparison.str();
}
#endif // SKIA_OHOS

std::string GrResourceCache::cacheInfoPurgeableQueue()
{
    std::map<uint64_t, size_t> purgSizeInfoWid;
    std::map<uint64_t, int> purgCountInfoWid;
    std::map<uint64_t, std::string> purgNameInfoWid;
    std::map<uint64_t, int> purgPidInfoWid;

    std::map<uint32_t, size_t> purgSizeInfoPid;
    std::map<uint32_t, int> purgCountInfoPid;
    std::map<uint32_t, std::string> purgNameInfoPid;

    std::map<uint32_t, size_t> purgSizeInfoFid;
    std::map<uint32_t, int> purgCountInfoFid;
    std::map<uint32_t, std::string> purgNameInfoFid;

    int purgCountUnknown = 0;
    size_t purgSizeUnknown = 0;

    for (int i = 0; i < fPurgeableQueue.count(); i++) {
        auto resource = fPurgeableQueue.at(i);
        auto resourceTag = resource->getResourceTag();
        if (resourceTag.fWid != 0) {
            updatePurgeableWidMap(resource, purgNameInfoWid, purgSizeInfoWid, purgPidInfoWid, purgCountInfoWid);
        } else if (resourceTag.fFid != 0) {
            updatePurgeableFidMap(resource, purgNameInfoFid, purgSizeInfoFid, purgCountInfoFid);
            if (resourceTag.fPid != 0) {
                updatePurgeablePidMap(resource, purgNameInfoPid, purgSizeInfoPid, purgCountInfoPid);
            }
        } else {
            purgCountUnknown++;
            purgSizeUnknown += resource->gpuMemorySize();
        }
    }

    std::string infoStr;
    if (purgSizeInfoWid.size() > 0) {
        infoStr += ";PurgeableInfo_Node:[";
        updatePurgeableWidInfo(infoStr, purgNameInfoWid, purgSizeInfoWid, purgPidInfoWid, purgCountInfoWid);
    }
    if (purgSizeInfoPid.size() > 0) {
        infoStr += ";PurgeableInfo_Pid:[";
        updatePurgeablePidInfo(infoStr, purgNameInfoPid, purgSizeInfoPid, purgCountInfoPid);
    }
    if (purgSizeInfoFid.size() > 0) {
        infoStr += ";PurgeableInfo_Fid:[";
        updatePurgeableFidInfo(infoStr, purgNameInfoFid, purgSizeInfoFid, purgCountInfoFid);
    }
    updatePurgeableUnknownInfo(infoStr, ";PurgeableInfo_Unknown:", purgCountUnknown, purgSizeUnknown);
    return infoStr;
}

std::string GrResourceCache::cacheInfoNoPurgeableQueue()
{
    std::map<uint64_t, size_t> noPurgSizeInfoWid;
    std::map<uint64_t, int> noPurgCountInfoWid;
    std::map<uint64_t, std::string> noPurgNameInfoWid;
    std::map<uint64_t, int> noPurgPidInfoWid;

    std::map<uint32_t, size_t> noPurgSizeInfoPid;
    std::map<uint32_t, int> noPurgCountInfoPid;
    std::map<uint32_t, std::string> noPurgNameInfoPid;

    std::map<uint32_t, size_t> noPurgSizeInfoFid;
    std::map<uint32_t, int> noPurgCountInfoFid;
    std::map<uint32_t, std::string> noPurgNameInfoFid;

    int noPurgCountUnknown = 0;
    size_t noPurgSizeUnknown = 0;

    for (int i = 0; i < fNonpurgeableResources.size(); i++) {
        auto resource = fNonpurgeableResources[i];
        if (resource == nullptr) {
            continue;
        }
        auto resourceTag = resource->getResourceTag();
        if (resourceTag.fWid != 0) {
            updatePurgeableWidMap(resource, noPurgNameInfoWid, noPurgSizeInfoWid, noPurgPidInfoWid, noPurgCountInfoWid);
        } else if (resourceTag.fFid != 0) {
            updatePurgeableFidMap(resource, noPurgNameInfoFid, noPurgSizeInfoFid, noPurgCountInfoFid);
            if (resourceTag.fPid != 0) {
                updatePurgeablePidMap(resource, noPurgNameInfoPid, noPurgSizeInfoPid, noPurgCountInfoPid);
            }
        } else {
            noPurgCountUnknown++;
            noPurgSizeUnknown += resource->gpuMemorySize();
        }
    }

    std::string infoStr;
    if (noPurgSizeInfoWid.size() > 0) {
        infoStr += ";NonPurgeableInfo_Node:[";
        updatePurgeableWidInfo(infoStr, noPurgNameInfoWid, noPurgSizeInfoWid, noPurgPidInfoWid, noPurgCountInfoWid);
    }
    if (noPurgSizeInfoPid.size() > 0) {
        infoStr += ";NonPurgeableInfo_Pid:[";
        updatePurgeablePidInfo(infoStr, noPurgNameInfoPid, noPurgSizeInfoPid, noPurgCountInfoPid);
    }
    if (noPurgSizeInfoFid.size() > 0) {
        infoStr += ";NonPurgeableInfo_Fid:[";
        updatePurgeableFidInfo(infoStr, noPurgNameInfoFid, noPurgSizeInfoFid, noPurgCountInfoFid);
    }
    updatePurgeableUnknownInfo(infoStr, ";NonPurgeableInfo_Unknown:", noPurgCountUnknown, noPurgSizeUnknown);
    return infoStr;
}

void GrResourceCache::updatePurgeableWidMap(GrGpuResource* resource,
                                            std::map<uint64_t, std::string>& nameInfoWid,
                                            std::map<uint64_t, size_t>& sizeInfoWid,
                                            std::map<uint64_t, int>& pidInfoWid,
                                            std::map<uint64_t, int>& countInfoWid)
{
    auto resourceTag = resource->getResourceTag();
    auto it = sizeInfoWid.find(resourceTag.fWid);
    if (it != sizeInfoWid.end()) {
        sizeInfoWid[resourceTag.fWid] = it->second + resource->gpuMemorySize();
        countInfoWid[resourceTag.fWid]++;
    } else {
        sizeInfoWid[resourceTag.fWid] = resource->gpuMemorySize();
        nameInfoWid[resourceTag.fWid] = resourceTag.fName;
        pidInfoWid[resourceTag.fWid] = resourceTag.fPid;
        countInfoWid[resourceTag.fWid] = 1;
    }
}

void GrResourceCache::updatePurgeablePidMap(GrGpuResource* resource,
                                            std::map<uint32_t, std::string>& nameInfoPid,
                                            std::map<uint32_t, size_t>& sizeInfoPid,
                                            std::map<uint32_t, int>& countInfoPid)
{
    auto resourceTag = resource->getResourceTag();
    auto it = sizeInfoPid.find(resourceTag.fPid);
    if (it != sizeInfoPid.end()) {
        sizeInfoPid[resourceTag.fPid] = it->second + resource->gpuMemorySize();
        countInfoPid[resourceTag.fPid]++;
    } else {
        sizeInfoPid[resourceTag.fPid] = resource->gpuMemorySize();
        nameInfoPid[resourceTag.fPid] = resourceTag.fName;
        countInfoPid[resourceTag.fPid] = 1;
    }
}

void GrResourceCache::updatePurgeableFidMap(GrGpuResource* resource,
                                            std::map<uint32_t, std::string>& nameInfoFid,
                                            std::map<uint32_t, size_t>& sizeInfoFid,
                                            std::map<uint32_t, int>& countInfoFid)
{
    auto resourceTag = resource->getResourceTag();
    auto it = sizeInfoFid.find(resourceTag.fFid);
    if (it != sizeInfoFid.end()) {
        sizeInfoFid[resourceTag.fFid] = it->second + resource->gpuMemorySize();
        countInfoFid[resourceTag.fFid]++;
    } else {
        sizeInfoFid[resourceTag.fFid] = resource->gpuMemorySize();
        nameInfoFid[resourceTag.fFid] = resourceTag.fName;
        countInfoFid[resourceTag.fFid] = 1;
    }
}

void GrResourceCache::updatePurgeableWidInfo(std::string& infoStr,
                                             std::map<uint64_t, std::string>& nameInfoWid,
                                             std::map<uint64_t, size_t>& sizeInfoWid,
                                             std::map<uint64_t, int>& pidInfoWid,
                                             std::map<uint64_t, int>& countInfoWid)
{
    for (auto it = sizeInfoWid.begin(); it != sizeInfoWid.end(); it++) {
        infoStr += "[" + nameInfoWid[it->first] +
            ",pid=" + std::to_string(pidInfoWid[it->first]) +
            ",NodeId=" + std::to_string(it->first) +
            ",count=" + std::to_string(countInfoWid[it->first]) +
            ",size=" + std::to_string(it->second) +
            "(" + std::to_string(it->second / MB) + " MB)],";
    }
    infoStr += ']';
}

void GrResourceCache::updatePurgeablePidInfo(std::string& infoStr,
                                             std::map<uint32_t, std::string>& nameInfoPid,
                                             std::map<uint32_t, size_t>& sizeInfoPid,
                                             std::map<uint32_t, int>& countInfoPid)
{
    for (auto it = sizeInfoPid.begin(); it != sizeInfoPid.end(); it++) {
        infoStr += "[" + nameInfoPid[it->first] +
            ",pid=" + std::to_string(it->first) +
            ",count=" + std::to_string(countInfoPid[it->first]) +
            ",size=" + std::to_string(it->second) +
            "(" + std::to_string(it->second / MB) + " MB)],";
    }
    infoStr += ']';
}

void GrResourceCache::updatePurgeableFidInfo(std::string& infoStr,
                                             std::map<uint32_t, std::string>& nameInfoFid,
                                             std::map<uint32_t, size_t>& sizeInfoFid,
                                             std::map<uint32_t, int>& countInfoFid)
{
    for (auto it = sizeInfoFid.begin(); it != sizeInfoFid.end(); it++) {
        infoStr += "[" + nameInfoFid[it->first] +
            ",typeid=" + std::to_string(it->first) +
            ",count=" + std::to_string(countInfoFid[it->first]) +
            ",size=" + std::to_string(it->second) +
            "(" + std::to_string(it->second / MB) + " MB)],";
    }
    infoStr += ']';
}

void GrResourceCache::updatePurgeableUnknownInfo(
    std::string& infoStr, const std::string& unknownPrefix, const int countUnknown, const size_t sizeUnknown)
{
    if (countUnknown > 0) {
        infoStr += unknownPrefix +
            "[count=" + std::to_string(countUnknown) +
            ",size=" + std::to_string(sizeUnknown) +
            "(" + std::to_string(sizeUnknown / MB) + "MB)]";
    }
}
#endif

void GrResourceCache::insertResource(GrGpuResource* resource)
{
    ASSERT_SINGLE_OWNER
    SkASSERT(resource);
    SkASSERT(!this->isInCache(resource));
    SkASSERT(!resource->wasDestroyed());
    SkASSERT(!resource->resourcePriv().isPurgeable());

    // We must set the timestamp before adding to the array in case the timestamp wraps and we wind
    // up iterating over all the resources that already have timestamps.
    resource->cacheAccess().setTimestamp(this->getNextTimestamp());

    this->addToNonpurgeableArray(resource);

    size_t size = resource->gpuMemorySize();
    SkDEBUGCODE(++fCount;)
    fBytes += size;

    // OH ISSUE: memory count
    auto pid = resource->getResourceTag().fPid;
    if (pid && resource->isRealAlloc()) {
        auto& pidSize = fBytesOfPid[pid];
        pidSize += size;
        fUpdatedBytesOfPid[pid] = pidSize;
        if (pidSize >= fMemoryControl_ && fExitedPid_.find(pid) == fExitedPid_.end() && fMemoryOverflowCallback_) {
            fMemoryOverflowCallback_(pid, pidSize, true);
            fExitedPid_.insert(pid);
            SkDebugf("OHOS resource overflow! pid[%{public}d], size[%{public}zu]", pid, pidSize);
#ifdef SKIA_OHOS_FOR_OHOS_TRACE
            HITRACE_OHOS_NAME_FMT_ALWAYS("OHOS gpu resource overflow: pid(%u), size:(%u)", pid, pidSize);
#endif
        }
    }

#if GR_CACHE_STATS
    fHighWaterCount = std::max(this->getResourceCount(), fHighWaterCount);
    fHighWaterBytes = std::max(fBytes, fHighWaterBytes);
#endif
    if (GrBudgetedType::kBudgeted == resource->resourcePriv().budgetedType()) {
        ++fBudgetedCount;
        fBudgetedBytes += size;
        TRACE_COUNTER2("skia.gpu.cache", "skia budget", "used",
                       fBudgetedBytes, "free", fMaxBytes - fBudgetedBytes);
#if GR_CACHE_STATS
        fBudgetedHighWaterCount = std::max(fBudgetedCount, fBudgetedHighWaterCount);
        fBudgetedHighWaterBytes = std::max(fBudgetedBytes, fBudgetedHighWaterBytes);
#endif
    }
    SkASSERT(!resource->cacheAccess().isUsableAsScratch());
#ifdef SKIA_OHOS_FOR_OHOS_TRACE
    if (fBudgetedBytes >= fMaxBytes || fPurgeableQueue.count() >= fPurgeableMaxCount) {
        HITRACE_OHOS_NAME_FMT_ALWAYS("cache over fBudgetedBytes:(%u),fMaxBytes:(%u), purgeableCount(%u)",
            fBudgetedBytes, fMaxBytes, fPurgeableQueue.count());
#ifdef SKIA_DFX_FOR_OHOS
        SimpleCacheInfo simpleCacheInfo;
        traceBeforePurgeUnlockRes("insertResource", simpleCacheInfo);
#endif
        this->purgeAsNeeded();
#ifdef SKIA_DFX_FOR_OHOS
        traceAfterPurgeUnlockRes("insertResource", simpleCacheInfo);
#endif
    } else {
        this->purgeAsNeeded();
    }
#else
    this->purgeAsNeeded();
#endif
}

void GrResourceCache::removeResource(GrGpuResource* resource) {
    ASSERT_SINGLE_OWNER
    this->validate();
    SkASSERT(this->isInCache(resource));

    size_t size = resource->gpuMemorySize();
    if (resource->resourcePriv().isPurgeable()) {
        fPurgeableQueue.remove(resource);
        fPurgeableBytes -= size;
    } else {
        this->removeFromNonpurgeableArray(resource);
    }

    SkDEBUGCODE(--fCount;)
    fBytes -= size;

    // OH ISSUE: memory count
    auto pid = resource->getResourceTag().fPid;
    if (pid && resource->isRealAlloc()) {
        auto& pidSize = fBytesOfPid[pid];
        pidSize -= size;
        fUpdatedBytesOfPid[pid] = pidSize;
        if (pidSize == 0) {
            fBytesOfPid.erase(pid);
        }
    }

    if (GrBudgetedType::kBudgeted == resource->resourcePriv().budgetedType()) {
        --fBudgetedCount;
        fBudgetedBytes -= size;
        TRACE_COUNTER2("skia.gpu.cache", "skia budget", "used",
                       fBudgetedBytes, "free", fMaxBytes - fBudgetedBytes);
    }

    if (resource->cacheAccess().isUsableAsScratch()) {
        fScratchMap.remove(resource->resourcePriv().getScratchKey(), resource);
    }
    if (resource->getUniqueKey().isValid()) {
        fUniqueHash.remove(resource->getUniqueKey());
    }
    this->validate();
}

void GrResourceCache::abandonAll() {
    AutoValidate av(this);

    while (!fNonpurgeableResources.empty()) {
        GrGpuResource* back = *(fNonpurgeableResources.end() - 1);
        SkASSERT(!back->wasDestroyed());
        back->cacheAccess().abandon();
    }

    while (fPurgeableQueue.count()) {
        GrGpuResource* top = fPurgeableQueue.peek();
        SkASSERT(!top->wasDestroyed());
        top->cacheAccess().abandon();
    }

    fThreadSafeCache->dropAllRefs();

    SkASSERT(!fScratchMap.count());
    SkASSERT(!fUniqueHash.count());
    SkASSERT(!fCount);
    SkASSERT(!this->getResourceCount());
    SkASSERT(!fBytes);
    SkASSERT(!fBudgetedCount);
    SkASSERT(!fBudgetedBytes);
    SkASSERT(!fPurgeableBytes);
}

void GrResourceCache::releaseAll() {
    AutoValidate av(this);

    fThreadSafeCache->dropAllRefs();

    this->processFreedGpuResources();

    SkASSERT(fProxyProvider); // better have called setProxyProvider
    SkASSERT(fThreadSafeCache); // better have called setThreadSafeCache too

    // We must remove the uniqueKeys from the proxies here. While they possess a uniqueKey
    // they also have a raw pointer back to this class (which is presumably going away)!
    fProxyProvider->removeAllUniqueKeys();

    while (!fNonpurgeableResources.empty()) {
        GrGpuResource* back = *(fNonpurgeableResources.end() - 1);
        SkASSERT(!back->wasDestroyed());
        back->cacheAccess().release();
    }

    while (fPurgeableQueue.count()) {
        GrGpuResource* top = fPurgeableQueue.peek();
        SkASSERT(!top->wasDestroyed());
        top->cacheAccess().release();
    }

    SkASSERT(!fScratchMap.count());
    SkASSERT(!fUniqueHash.count());
    SkASSERT(!fCount);
    SkASSERT(!this->getResourceCount());
    SkASSERT(!fBytes);
    SkASSERT(!fBudgetedCount);
    SkASSERT(!fBudgetedBytes);
    SkASSERT(!fPurgeableBytes);
}

void GrResourceCache::releaseByTag(const GrGpuResourceTag& tag) {
    AutoValidate av(this);
    this->processFreedGpuResources();
    SkASSERT(fProxyProvider); // better have called setProxyProvider
    std::vector<GrGpuResource*> recycleVector;
    for (int i = 0; i < fNonpurgeableResources.size(); i++) {
        GrGpuResource* resource = fNonpurgeableResources[i];
        if (tag.filter(resource->getResourceTag())) {
            recycleVector.emplace_back(resource);
            if (resource->getUniqueKey().isValid()) {
                fProxyProvider->processInvalidUniqueKey(resource->getUniqueKey(), nullptr,
                                                        GrProxyProvider::InvalidateGPUResource::kNo);
            }
        }
    }

    for (int i = 0; i < fPurgeableQueue.count(); i++) {
        GrGpuResource* resource = fPurgeableQueue.at(i);
        if (tag.filter(resource->getResourceTag())) {
            recycleVector.emplace_back(resource);
            if (resource->getUniqueKey().isValid()) {
                fProxyProvider->processInvalidUniqueKey(resource->getUniqueKey(), nullptr,
                                                        GrProxyProvider::InvalidateGPUResource::kNo);
            }
        }
    }

    for (auto resource : recycleVector) {
        SkASSERT(!resource->wasDestroyed());
        resource->cacheAccess().release();
    }
}

void GrResourceCache::setCurrentGrResourceTag(const GrGpuResourceTag& tag) {
    if (tag.isGrTagValid()) {
        grResourceTagCacheStack.push(tag);
        return;
    }
    if (!grResourceTagCacheStack.empty()) {
        grResourceTagCacheStack.pop();
    }
}

void GrResourceCache::popGrResourceTag()
{
    if (!grResourceTagCacheStack.empty()) {
        grResourceTagCacheStack.pop();
    }
}

GrGpuResourceTag GrResourceCache::getCurrentGrResourceTag() const {
    if (grResourceTagCacheStack.empty()) {
        return{};
    }
    return grResourceTagCacheStack.top();
}

std::set<GrGpuResourceTag> GrResourceCache::getAllGrGpuResourceTags() const {
    std::set<GrGpuResourceTag> result;
    for (int i = 0; i < fNonpurgeableResources.size(); ++i) {
        auto tag = fNonpurgeableResources[i]->getResourceTag();
        result.insert(tag);
    }
    return result;
}

#ifdef SKIA_OHOS
// OH ISSUE: set purgeable resource max count limit.
void GrResourceCache::setPurgeableResourceLimit(int purgeableMaxCount)
{
    fPurgeableMaxCount = purgeableMaxCount;
}
#endif

// OH ISSUE: get the memory information of the updated pid.
void GrResourceCache::getUpdatedMemoryMap(std::unordered_map<int32_t, size_t> &out)
{
    fUpdatedBytesOfPid.swap(out);
}

// OH ISSUE: init gpu memory limit.
void GrResourceCache::initGpuMemoryLimit(MemoryOverflowCallback callback, uint64_t size)
{
    if (fMemoryOverflowCallback_ == nullptr) {
        fMemoryOverflowCallback_ = callback;
        fMemoryControl_ = size;
    }
}

// OH ISSUE: check whether the PID is abnormal.
bool GrResourceCache::isPidAbnormal() const
{
    return fExitedPid_.find(getCurrentGrResourceTag().fPid) != fExitedPid_.end();
}

// OH ISSUE: change the fbyte when the resource tag changes.
void GrResourceCache::changeByteOfPid(int32_t beforePid, int32_t afterPid,
    size_t bytes, bool beforeRealAlloc, bool afterRealAlloc)
{
    if (beforePid && beforeRealAlloc) {
        auto& pidSize = fBytesOfPid[beforePid];
        pidSize -= bytes;
        fUpdatedBytesOfPid[beforePid] = pidSize;
        if (pidSize == 0) {
            fBytesOfPid.erase(beforePid);
        }
    }
    if (afterPid && afterRealAlloc) {
        auto& size = fBytesOfPid[afterPid];
        size += bytes;
        fUpdatedBytesOfPid[afterPid] = size;
    }
}

void GrResourceCache::refResource(GrGpuResource* resource) {
    SkASSERT(resource);
    SkASSERT(resource->getContext()->priv().getResourceCache() == this);
    if (resource->cacheAccess().hasRef()) {
        resource->ref();
    } else {
        this->refAndMakeResourceMRU(resource);
    }
    this->validate();
}

GrGpuResource* GrResourceCache::findAndRefScratchResource(const skgpu::ScratchKey& scratchKey) {
    SkASSERT(scratchKey.isValid());

    GrGpuResource* resource = fScratchMap.find(scratchKey);
    if (resource) {
        fScratchMap.remove(scratchKey, resource);
        this->refAndMakeResourceMRU(resource);
        this->validate();
    }
    return resource;
}

void GrResourceCache::willRemoveScratchKey(const GrGpuResource* resource) {
    ASSERT_SINGLE_OWNER
    SkASSERT(resource->resourcePriv().getScratchKey().isValid());
    if (resource->cacheAccess().isUsableAsScratch()) {
        fScratchMap.remove(resource->resourcePriv().getScratchKey(), resource);
    }
}

void GrResourceCache::removeUniqueKey(GrGpuResource* resource) {
    ASSERT_SINGLE_OWNER
    // Someone has a ref to this resource in order to have removed the key. When the ref count
    // reaches zero we will get a ref cnt notification and figure out what to do with it.
    if (resource->getUniqueKey().isValid()) {
        SkASSERT(resource == fUniqueHash.find(resource->getUniqueKey()));
        fUniqueHash.remove(resource->getUniqueKey());
    }
    resource->cacheAccess().removeUniqueKey();
    if (resource->cacheAccess().isUsableAsScratch()) {
        fScratchMap.insert(resource->resourcePriv().getScratchKey(), resource);
    }

    // Removing a unique key from a kUnbudgetedCacheable resource would make the resource
    // require purging. However, the resource must be ref'ed to get here and therefore can't
    // be purgeable. We'll purge it when the refs reach zero.
    SkASSERT(!resource->resourcePriv().isPurgeable());
    this->validate();
}

void GrResourceCache::changeUniqueKey(GrGpuResource* resource, const skgpu::UniqueKey& newKey) {
    ASSERT_SINGLE_OWNER
    SkASSERT(resource);
    SkASSERT(this->isInCache(resource));

    // If another resource has the new key, remove its key then install the key on this resource.
    if (newKey.isValid()) {
        if (GrGpuResource* old = fUniqueHash.find(newKey)) {
            // If the old resource using the key is purgeable and is unreachable, then remove it.
            if (!old->resourcePriv().getScratchKey().isValid() &&
                old->resourcePriv().isPurgeable()) {
                old->cacheAccess().release();
            } else {
                // removeUniqueKey expects an external owner of the resource.
                this->removeUniqueKey(sk_ref_sp(old).get());
            }
        }
        SkASSERT(nullptr == fUniqueHash.find(newKey));

        // Remove the entry for this resource if it already has a unique key.
        if (resource->getUniqueKey().isValid()) {
            SkASSERT(resource == fUniqueHash.find(resource->getUniqueKey()));
            fUniqueHash.remove(resource->getUniqueKey());
            SkASSERT(nullptr == fUniqueHash.find(resource->getUniqueKey()));
        } else {
            // 'resource' didn't have a valid unique key before so it is switching sides. Remove it
            // from the ScratchMap. The isUsableAsScratch call depends on us not adding the new
            // unique key until after this check.
            if (resource->cacheAccess().isUsableAsScratch()) {
                fScratchMap.remove(resource->resourcePriv().getScratchKey(), resource);
            }
        }

        resource->cacheAccess().setUniqueKey(newKey);
        fUniqueHash.add(resource);
    } else {
        this->removeUniqueKey(resource);
    }

    this->validate();
}

void GrResourceCache::refAndMakeResourceMRU(GrGpuResource* resource) {
    ASSERT_SINGLE_OWNER
    SkASSERT(resource);
    SkASSERT(this->isInCache(resource));

    if (resource->resourcePriv().isPurgeable()) {
        // It's about to become unpurgeable.
        fPurgeableBytes -= resource->gpuMemorySize();
        fPurgeableQueue.remove(resource);
        this->addToNonpurgeableArray(resource);
    } else if (!resource->cacheAccess().hasRefOrCommandBufferUsage() &&
               resource->resourcePriv().budgetedType() == GrBudgetedType::kBudgeted) {
        SkASSERT(fNumBudgetedResourcesFlushWillMakePurgeable > 0);
        fNumBudgetedResourcesFlushWillMakePurgeable--;
    }
    resource->cacheAccess().ref();

    resource->cacheAccess().setTimestamp(this->getNextTimestamp());
    this->validate();
}

void GrResourceCache::notifyARefCntReachedZero(GrGpuResource* resource,
                                               GrGpuResource::LastRemovedRef removedRef) {
    ASSERT_SINGLE_OWNER
    SkASSERT(resource);
    SkASSERT(!resource->wasDestroyed());
    SkASSERT(this->isInCache(resource));
    // This resource should always be in the nonpurgeable array when this function is called. It
    // will be moved to the queue if it is newly purgeable.
    SkASSERT(fNonpurgeableResources[*resource->cacheAccess().accessCacheIndex()] == resource);

    if (removedRef == GrGpuResource::LastRemovedRef::kMainRef) {
        if (resource->cacheAccess().isUsableAsScratch()) {
            fScratchMap.insert(resource->resourcePriv().getScratchKey(), resource);
        }
    }

    if (resource->cacheAccess().hasRefOrCommandBufferUsage()) {
        this->validate();
        return;
    }

#ifdef SK_DEBUG
    // When the timestamp overflows validate() is called. validate() checks that resources in
    // the nonpurgeable array are indeed not purgeable. However, the movement from the array to
    // the purgeable queue happens just below in this function. So we mark it as an exception.
    if (resource->resourcePriv().isPurgeable()) {
        fNewlyPurgeableResourceForValidation = resource;
    }
#endif
    resource->cacheAccess().setTimestamp(this->getNextTimestamp());
    SkDEBUGCODE(fNewlyPurgeableResourceForValidation = nullptr);

    if (!resource->resourcePriv().isPurgeable() &&
        resource->resourcePriv().budgetedType() == GrBudgetedType::kBudgeted) {
        ++fNumBudgetedResourcesFlushWillMakePurgeable;
    }

    if (!resource->resourcePriv().isPurgeable()) {
        this->validate();
        return;
    }

    this->removeFromNonpurgeableArray(resource);
    fPurgeableQueue.insert(resource);
    resource->cacheAccess().setTimeWhenResourceBecomePurgeable();
    fPurgeableBytes += resource->gpuMemorySize();

    bool hasUniqueKey = resource->getUniqueKey().isValid();

    GrBudgetedType budgetedType = resource->resourcePriv().budgetedType();

    if (budgetedType == GrBudgetedType::kBudgeted) {
        // Purge the resource immediately if we're over budget
        // Also purge if the resource has neither a valid scratch key nor a unique key.
        bool hasKey = resource->resourcePriv().getScratchKey().isValid() || hasUniqueKey;
        if (!this->overBudget() && hasKey) {
            return;
        }
    } else {
        // We keep unbudgeted resources with a unique key in the purgeable queue of the cache so
        // they can be reused again by the image connected to the unique key.
        if (hasUniqueKey && budgetedType == GrBudgetedType::kUnbudgetedCacheable) {
            return;
        }
        // Check whether this resource could still be used as a scratch resource.
        if (!resource->resourcePriv().refsWrappedObjects() &&
            resource->resourcePriv().getScratchKey().isValid()) {
            // We won't purge an existing resource to make room for this one.
            if (this->wouldFit(resource->gpuMemorySize())) {
                resource->resourcePriv().makeBudgeted();
                return;
            }
        }
    }

    SkDEBUGCODE(int beforeCount = this->getResourceCount();)
    resource->cacheAccess().release();
    // We should at least free this resource, perhaps dependent resources as well.
    SkASSERT(this->getResourceCount() < beforeCount);
    this->validate();
}

void GrResourceCache::didChangeBudgetStatus(GrGpuResource* resource) {
    ASSERT_SINGLE_OWNER
    SkASSERT(resource);
    SkASSERT(this->isInCache(resource));

    size_t size = resource->gpuMemorySize();
    // Changing from BudgetedType::kUnbudgetedCacheable to another budgeted type could make
    // resource become purgeable. However, we should never allow that transition. Wrapped
    // resources are the only resources that can be in that state and they aren't allowed to
    // transition from one budgeted state to another.
    SkDEBUGCODE(bool wasPurgeable = resource->resourcePriv().isPurgeable());
    if (resource->resourcePriv().budgetedType() == GrBudgetedType::kBudgeted) {
        ++fBudgetedCount;
        fBudgetedBytes += size;
#if GR_CACHE_STATS
        fBudgetedHighWaterBytes = std::max(fBudgetedBytes, fBudgetedHighWaterBytes);
        fBudgetedHighWaterCount = std::max(fBudgetedCount, fBudgetedHighWaterCount);
#endif
        if (!resource->resourcePriv().isPurgeable() &&
            !resource->cacheAccess().hasRefOrCommandBufferUsage()) {
            ++fNumBudgetedResourcesFlushWillMakePurgeable;
        }
        if (resource->cacheAccess().isUsableAsScratch()) {
            fScratchMap.insert(resource->resourcePriv().getScratchKey(), resource);
        }
        this->purgeAsNeeded();
    } else {
        SkASSERT(resource->resourcePriv().budgetedType() != GrBudgetedType::kUnbudgetedCacheable);
#ifdef SKIA_OHOS
        GrPerfMonitorReporter::GetInstance().recordTextureCache(resource->getResourceTag().fName);
#endif
        --fBudgetedCount;
        fBudgetedBytes -= size;
        if (!resource->resourcePriv().isPurgeable() &&
            !resource->cacheAccess().hasRefOrCommandBufferUsage()) {
            --fNumBudgetedResourcesFlushWillMakePurgeable;
        }
        if (!resource->cacheAccess().hasRef() && !resource->getUniqueKey().isValid() &&
            resource->resourcePriv().getScratchKey().isValid()) {
            fScratchMap.remove(resource->resourcePriv().getScratchKey(), resource);
        }
    }
    SkASSERT(wasPurgeable == resource->resourcePriv().isPurgeable());
    TRACE_COUNTER2("skia.gpu.cache", "skia budget", "used",
                   fBudgetedBytes, "free", fMaxBytes - fBudgetedBytes);

    this->validate();
}

void GrResourceCache::purgeAsNeeded() {
    TArray<skgpu::UniqueKeyInvalidatedMessage> invalidKeyMsgs;
    fInvalidUniqueKeyInbox.poll(&invalidKeyMsgs);
    if (!invalidKeyMsgs.empty()) {
        SkASSERT(fProxyProvider);

        for (int i = 0; i < invalidKeyMsgs.size(); ++i) {
            if (invalidKeyMsgs[i].inThreadSafeCache()) {
                fThreadSafeCache->remove(invalidKeyMsgs[i].key());
                SkASSERT(!fThreadSafeCache->has(invalidKeyMsgs[i].key()));
            } else {
                fProxyProvider->processInvalidUniqueKey(
                                                    invalidKeyMsgs[i].key(), nullptr,
                                                    GrProxyProvider::InvalidateGPUResource::kYes);
                SkASSERT(!this->findAndRefUniqueResource(invalidKeyMsgs[i].key()));
            }
        }
    }

    this->processFreedGpuResources();

    bool stillOverbudget = this->overBudget();
    while (stillOverbudget && fPurgeableQueue.count()) {
        GrGpuResource* resource = fPurgeableQueue.peek();
        SkASSERT(resource->resourcePriv().isPurgeable());
        resource->cacheAccess().release();
        stillOverbudget = this->overBudget();
    }

    if (stillOverbudget) {
        fThreadSafeCache->dropUniqueRefs(this);

        stillOverbudget = this->overBudget();
        while (stillOverbudget && fPurgeableQueue.count()) {
            GrGpuResource* resource = fPurgeableQueue.peek();
            SkASSERT(resource->resourcePriv().isPurgeable());
            resource->cacheAccess().release();
            stillOverbudget = this->overBudget();
        }
    }

    this->validate();
}

void GrResourceCache::purgeUnlockedResources(const skgpu::StdSteadyClock::time_point* purgeTime,
                                             GrPurgeResourceOptions opts) {
#if defined (SKIA_OHOS) && defined (SKIA_DFX_FOR_OHOS)
    SimpleCacheInfo simpleCacheInfo;
    traceBeforePurgeUnlockRes("purgeUnlockedResources", simpleCacheInfo);
#endif
    if (opts == GrPurgeResourceOptions::kAllResources) {
        if (purgeTime) {
            fThreadSafeCache->dropUniqueRefsOlderThan(*purgeTime);
        } else {
            fThreadSafeCache->dropUniqueRefs(nullptr);
        }

        // We could disable maintaining the heap property here, but it would add a lot of
        // complexity. Moreover, this is rarely called.
        while (fPurgeableQueue.count()) {
            GrGpuResource* resource = fPurgeableQueue.peek();

            const skgpu::StdSteadyClock::time_point resourceTime =
                    resource->cacheAccess().timeWhenResourceBecamePurgeable();
            if (purgeTime && resourceTime >= *purgeTime) {
                // Resources were given both LRU timestamps and tagged with a frame number when
                // they first became purgeable. The LRU timestamp won't change again until the
                // resource is made non-purgeable again. So, at this point all the remaining
                // resources in the timestamp-sorted queue will have a frame number >= to this
                // one.
                break;
            }

            SkASSERT(resource->resourcePriv().isPurgeable());
            resource->cacheAccess().release();
        }
    } else {
        SkASSERT(opts == GrPurgeResourceOptions::kScratchResourcesOnly);
        // Early out if the very first item is too new to purge to avoid sorting the queue when
        // nothing will be deleted.
        if (purgeTime && fPurgeableQueue.count() &&
            fPurgeableQueue.peek()->cacheAccess().timeWhenResourceBecamePurgeable() >= *purgeTime) {
#if defined (SKIA_OHOS) && defined (SKIA_DFX_FOR_OHOS)
            traceAfterPurgeUnlockRes("purgeUnlockedResources", simpleCacheInfo);
#endif
            return;
        }

        // Sort the queue
        fPurgeableQueue.sort();

        // Make a list of the scratch resources to delete
        SkTDArray<GrGpuResource*> scratchResources;
        for (int i = 0; i < fPurgeableQueue.count(); i++) {
            GrGpuResource* resource = fPurgeableQueue.at(i);

            const skgpu::StdSteadyClock::time_point resourceTime =
                    resource->cacheAccess().timeWhenResourceBecamePurgeable();
            if (purgeTime && resourceTime >= *purgeTime) {
                // scratch or not, all later iterations will be too recently used to purge.
                break;
            }
            SkASSERT(resource->resourcePriv().isPurgeable());
            if (!resource->getUniqueKey().isValid()) {
                *scratchResources.append() = resource;
            }
        }

        // Delete the scratch resources. This must be done as a separate pass
        // to avoid messing up the sorted order of the queue
        for (int i = 0; i < scratchResources.size(); i++) {
            scratchResources[i]->cacheAccess().release();
        }
    }

    this->validate();
#if defined (SKIA_OHOS) && defined (SKIA_DFX_FOR_OHOS)
    traceAfterPurgeUnlockRes("purgeUnlockedResources", simpleCacheInfo);
#endif
}

void GrResourceCache::purgeUnlockAndSafeCacheGpuResources() {
#if defined (SKIA_OHOS) && defined (SKIA_DFX_FOR_OHOS)
    SimpleCacheInfo simpleCacheInfo;
    traceBeforePurgeUnlockRes("purgeUnlockAndSafeCacheGpuResources", simpleCacheInfo);
#endif
    fThreadSafeCache->dropUniqueRefs(nullptr);
    // Sort the queue
    fPurgeableQueue.sort();

    // Make a list of the scratch resources to delete
    SkTDArray<GrGpuResource*> scratchResources;
    for (int i = 0; i < fPurgeableQueue.count(); i++) {
        GrGpuResource* resource = fPurgeableQueue.at(i);
        if (!resource) {
            continue;
        }
        SkASSERT(resource->resourcePriv().isPurgeable());
        if (!resource->getUniqueKey().isValid()) {
            *scratchResources.append() = resource;
        }
    }

    //Delete the scatch resource. This must be done as a separate pass
    //to avoid messing up the sorted order of the queue
    for (int i = 0; i < scratchResources.size(); i++) {
        scratchResources[i]->cacheAccess().release();
    }

    this->validate();
#if defined (SKIA_OHOS) && defined (SKIA_DFX_FOR_OHOS)
    traceAfterPurgeUnlockRes("purgeUnlockAndSafeCacheGpuResources", simpleCacheInfo);
#endif
}

void GrResourceCache::purgeUnlockedResourcesByPid(bool scratchResourceOnly, const std::set<int>& exitedPidSet) {
#if defined (SKIA_OHOS) && defined (SKIA_DFX_FOR_OHOS)
    SimpleCacheInfo simpleCacheInfo;
    traceBeforePurgeUnlockRes("purgeUnlockedResourcesByPid", simpleCacheInfo);
#endif
    // Sort the queue
    fPurgeableQueue.sort();

    //Make lists of the need purged resources to delete
    fThreadSafeCache->dropUniqueRefs(nullptr);
    SkTDArray<GrGpuResource*> exitPidResources;
    SkTDArray<GrGpuResource*> scratchResources;
    for (int i = 0; i < fPurgeableQueue.count(); i++) {
        GrGpuResource* resource = fPurgeableQueue.at(i);
        SkASSERT(resource->resourcePriv().isPurgeable());
        if (exitedPidSet.count(resource->getResourceTag().fPid)) {
            *exitPidResources.append() = resource;
        } else if (!resource->getUniqueKey().isValid()) {
            *scratchResources.append() = resource;
        }
    }

    //Delete the exited pid and scatch resource. This must be done as a separate pass
    //to avoid messing up the sorted order of the queue
    for (int i = 0; i < exitPidResources.size(); i++) {
        exitPidResources[i]->cacheAccess().release();
    }
    for (int i = 0; i < scratchResources.size(); i++) {
        scratchResources[i]->cacheAccess().release();
    }

    for (auto pid : exitedPidSet) {
        fExitedPid_.erase(pid);
    }

    this->validate();
#if defined (SKIA_OHOS) && defined (SKIA_DFX_FOR_OHOS)
    traceAfterPurgeUnlockRes("purgeUnlockedResourcesByPid", simpleCacheInfo);
#endif
}

void GrResourceCache::purgeUnlockedResourcesByTag(bool scratchResourcesOnly, const GrGpuResourceTag& tag) {
    // Sort the queue
    fPurgeableQueue.sort();

    //Make a list of the scratch resources to delete
    SkTDArray<GrGpuResource*> scratchResources;
    for (int i = 0; i < fPurgeableQueue.count(); i++) {
        GrGpuResource* resource = fPurgeableQueue.at(i);
        SkASSERT(resource->resourcePriv().isPurgeable());
        if (tag.filter(resource->getResourceTag()) && (!scratchResourcesOnly || !resource->getUniqueKey().isValid())) {
            *scratchResources.append() = resource;
        }
    }

    //Delete the scatch resource. This must be done as a separate pass
    //to avoid messing up the sorted order of the queue
    for (int i = 0; i <scratchResources.size(); i++) {
        scratchResources[i]->cacheAccess().release();
    }

    this->validate();
}

bool GrResourceCache::purgeToMakeHeadroom(size_t desiredHeadroomBytes) {
    AutoValidate av(this);
    if (desiredHeadroomBytes > fMaxBytes) {
        return false;
    }
    if (this->wouldFit(desiredHeadroomBytes)) {
        return true;
    }
    fPurgeableQueue.sort();

    size_t projectedBudget = fBudgetedBytes;
    int purgeCnt = 0;
    for (int i = 0; i < fPurgeableQueue.count(); i++) {
        GrGpuResource* resource = fPurgeableQueue.at(i);
        if (GrBudgetedType::kBudgeted == resource->resourcePriv().budgetedType()) {
            projectedBudget -= resource->gpuMemorySize();
        }
        if (projectedBudget + desiredHeadroomBytes <= fMaxBytes) {
            purgeCnt = i + 1;
            break;
        }
    }
    if (purgeCnt == 0) {
        return false;
    }

    // Success! Release the resources.
    // Copy to array first so we don't mess with the queue.
    std::vector<GrGpuResource*> resources;
    resources.reserve(purgeCnt);
    for (int i = 0; i < purgeCnt; i++) {
        resources.push_back(fPurgeableQueue.at(i));
    }
    for (GrGpuResource* resource : resources) {
        resource->cacheAccess().release();
    }
    return true;
}

void GrResourceCache::purgeUnlockedResources(size_t bytesToPurge, bool preferScratchResources) {

    const size_t tmpByteBudget = std::max((size_t)0, fBytes - bytesToPurge);
    bool stillOverbudget = tmpByteBudget < fBytes;

    if (preferScratchResources && bytesToPurge < fPurgeableBytes) {
        // Sort the queue
        fPurgeableQueue.sort();

        // Make a list of the scratch resources to delete
        SkTDArray<GrGpuResource*> scratchResources;
        size_t scratchByteCount = 0;
        for (int i = 0; i < fPurgeableQueue.count() && stillOverbudget; i++) {
            GrGpuResource* resource = fPurgeableQueue.at(i);
            SkASSERT(resource->resourcePriv().isPurgeable());
            if (!resource->getUniqueKey().isValid()) {
                *scratchResources.append() = resource;
                scratchByteCount += resource->gpuMemorySize();
                stillOverbudget = tmpByteBudget < fBytes - scratchByteCount;
            }
        }

        // Delete the scratch resources. This must be done as a separate pass
        // to avoid messing up the sorted order of the queue
        for (int i = 0; i < scratchResources.size(); i++) {
            scratchResources[i]->cacheAccess().release();
        }
        stillOverbudget = tmpByteBudget < fBytes;

        this->validate();
    }

    // Purge any remaining resources in LRU order
    if (stillOverbudget) {
        const size_t cachedByteCount = fMaxBytes;
        fMaxBytes = tmpByteBudget;
        this->purgeAsNeeded();
        fMaxBytes = cachedByteCount;
    }
}

bool GrResourceCache::requestsFlush() const {
    return this->overBudget() && !fPurgeableQueue.count() &&
           fNumBudgetedResourcesFlushWillMakePurgeable > 0;
}

void GrResourceCache::processFreedGpuResources() {
    TArray<UnrefResourceMessage> msgs;
    fUnrefResourceInbox.poll(&msgs);
    // We don't need to do anything other than let the messages delete themselves and call unref.
}

void GrResourceCache::addToNonpurgeableArray(GrGpuResource* resource) {
    int index = fNonpurgeableResources.size();
    *fNonpurgeableResources.append() = resource;
    *resource->cacheAccess().accessCacheIndex() = index;
}

void GrResourceCache::removeFromNonpurgeableArray(GrGpuResource* resource) {
    int* index = resource->cacheAccess().accessCacheIndex();
    // Fill the hole we will create in the array with the tail object, adjust its index, and
    // then pop the array
    GrGpuResource* tail = *(fNonpurgeableResources.end() - 1);
    SkASSERT(fNonpurgeableResources[*index] == resource);
    fNonpurgeableResources[*index] = tail;
    *tail->cacheAccess().accessCacheIndex() = *index;
    fNonpurgeableResources.pop_back();
    SkDEBUGCODE(*index = -1);
}

uint32_t GrResourceCache::getNextTimestamp() {
    // If we wrap then all the existing resources will appear older than any resources that get
    // a timestamp after the wrap.
    if (0 == fTimestamp) {
        int count = this->getResourceCount();
        if (count) {
            // Reset all the timestamps. We sort the resources by timestamp and then assign
            // sequential timestamps beginning with 0. This is O(n*lg(n)) but it should be extremely
            // rare.
            SkTDArray<GrGpuResource*> sortedPurgeableResources;
            sortedPurgeableResources.reserve(fPurgeableQueue.count());

            while (fPurgeableQueue.count()) {
                *sortedPurgeableResources.append() = fPurgeableQueue.peek();
                fPurgeableQueue.pop();
            }

            SkTQSort(fNonpurgeableResources.begin(), fNonpurgeableResources.end(),
                     CompareTimestamp);

            // Pick resources out of the purgeable and non-purgeable arrays based on lowest
            // timestamp and assign new timestamps.
            int currP = 0;
            int currNP = 0;
            while (currP < sortedPurgeableResources.size() &&
                   currNP < fNonpurgeableResources.size()) {
                uint32_t tsP = sortedPurgeableResources[currP]->cacheAccess().timestamp();
                uint32_t tsNP = fNonpurgeableResources[currNP]->cacheAccess().timestamp();
                SkASSERT(tsP != tsNP);
                if (tsP < tsNP) {
                    sortedPurgeableResources[currP++]->cacheAccess().setTimestamp(fTimestamp++);
                } else {
                    // Correct the index in the nonpurgeable array stored on the resource post-sort.
                    *fNonpurgeableResources[currNP]->cacheAccess().accessCacheIndex() = currNP;
                    fNonpurgeableResources[currNP++]->cacheAccess().setTimestamp(fTimestamp++);
                }
            }

            // The above loop ended when we hit the end of one array. Finish the other one.
            while (currP < sortedPurgeableResources.size()) {
                sortedPurgeableResources[currP++]->cacheAccess().setTimestamp(fTimestamp++);
            }
            while (currNP < fNonpurgeableResources.size()) {
                *fNonpurgeableResources[currNP]->cacheAccess().accessCacheIndex() = currNP;
                fNonpurgeableResources[currNP++]->cacheAccess().setTimestamp(fTimestamp++);
            }

            // Rebuild the queue.
            for (int i = 0; i < sortedPurgeableResources.size(); ++i) {
                fPurgeableQueue.insert(sortedPurgeableResources[i]);
            }

            this->validate();
            SkASSERT(count == this->getResourceCount());

            // count should be the next timestamp we return.
            SkASSERT(fTimestamp == SkToU32(count));
        }
    }
    return fTimestamp++;
}

#ifdef SKIA_DFX_FOR_RECORD_VKIMAGE
void GrResourceCache::dumpAllResource(std::stringstream& dump) const {
    if (getResourceCount() == 0) {
        return;
    }
    dump << "Purgeable: " << fPurgeableQueue.count() << std::endl;
    for (size_t i = 0; i < fPurgeableQueue.count(); ++i) {
        GrGpuResource* resource = fPurgeableQueue.at(i);
        if (resource == nullptr) {
            continue;
        }
        if (strcmp(resource->getResourceType(), "VkImage") != 0) {
            continue;
        }
        resource->dumpVkImageInfo(dump);
    }
    dump << "Non-Purgeable: " << fNonpurgeableResources.size() << std::endl;
    for (size_t i = 0; i < fNonpurgeableResources.size(); ++i) {
        GrGpuResource* resource = fNonpurgeableResources[i];
        if (resource == nullptr) {
            continue;
        }
        if (strcmp(resource->getResourceType(), "VkImage") != 0) {
            continue;
        }
        resource->dumpVkImageInfo(dump);
    }
#ifdef SK_VULKAN
    dump << "Destroy Record: " << std::endl;
    ParallelDebug::DumpAllDestroyVkImage(dump);
#endif
}

void GrResourceCache::dumpResourceByObjHandle(std::stringstream& dump, uint64_t objHandle) const {
    if (getResourceCount() == 0) {
        return;
    }
    dump << "Purgeable: " << fPurgeableQueue.count() << std::endl;
    for (size_t i = 0; i < fPurgeableQueue.count(); ++i) {
        GrGpuResource* resource = fPurgeableQueue.at(i);
        if (resource == nullptr) {
            continue;
        }
        if (strcmp(resource->getResourceType(), "VkImage") != 0) {
            continue;
        }
        resource->dumpVkImageInfoByObjHandle(dump, objHandle);
    }
    dump << "Non-Purgeable: " << fNonpurgeableResources.size() << std::endl;
    for (size_t i = 0; i < fNonpurgeableResources.size(); ++i) {
        GrGpuResource* resource = fNonpurgeableResources[i];
        if (resource == nullptr) {
            continue;
        }
        if (strcmp(resource->getResourceType(), "VkImage") != 0) {
            continue;
        }
        resource->dumpVkImageInfoByObjHandle(dump, objHandle);
    }
#ifdef SK_VULKAN
    dump << "Destroy Record: " << std::endl;
    ParallelDebug::DumpDestroyVkImageByObjHandle(dump, objHandle);
#endif
}
#endif

void GrResourceCache::dumpMemoryStatistics(SkTraceMemoryDump* traceMemoryDump) const {
    for (int i = 0; i < fNonpurgeableResources.size(); ++i) {
        fNonpurgeableResources[i]->dumpMemoryStatistics(traceMemoryDump);
    }
    for (int i = 0; i < fPurgeableQueue.count(); ++i) {
        fPurgeableQueue.at(i)->dumpMemoryStatistics(traceMemoryDump);
    }
}

void GrResourceCache::dumpMemoryStatistics(SkTraceMemoryDump* traceMemoryDump, GrGpuResourceTag& tag) const {
    for (int i = 0; i < fNonpurgeableResources.size(); ++i) {
        if (tag.filter(fNonpurgeableResources[i]->getResourceTag())) {
            fNonpurgeableResources[i]->dumpMemoryStatistics(traceMemoryDump);
        }
    }
    for (int i = 0; i < fPurgeableQueue.count(); ++i) {
        if (tag.filter(fPurgeableQueue.at(i)->getResourceTag())) {
            fPurgeableQueue.at(i)->dumpMemoryStatistics(traceMemoryDump);
        }
    }
}

#if GR_CACHE_STATS
void GrResourceCache::getStats(Stats* stats) const {
    stats->reset();

    stats->fTotal = this->getResourceCount();
    stats->fNumNonPurgeable = fNonpurgeableResources.size();
    stats->fNumPurgeable = fPurgeableQueue.count();

    for (int i = 0; i < fNonpurgeableResources.size(); ++i) {
        stats->update(fNonpurgeableResources[i]);
    }
    for (int i = 0; i < fPurgeableQueue.count(); ++i) {
        stats->update(fPurgeableQueue.at(i));
    }
}

#if defined(GPU_TEST_UTILS)
void GrResourceCache::dumpStats(SkString* out) const {
    this->validate();

    Stats stats;

    this->getStats(&stats);

    float byteUtilization = (100.f * fBudgetedBytes) / fMaxBytes;

    out->appendf("Budget: %d bytes\n", (int)fMaxBytes);
    out->appendf("\t\tEntry Count: current %d"
                 " (%d budgeted, %d wrapped, %d locked, %d scratch), high %d\n",
                 stats.fTotal, fBudgetedCount, stats.fWrapped, stats.fNumNonPurgeable,
                 stats.fScratch, fHighWaterCount);
    out->appendf("\t\tEntry Bytes: current %d (budgeted %d, %.2g%% full, %d unbudgeted) high %d\n",
                 SkToInt(fBytes), SkToInt(fBudgetedBytes), byteUtilization,
                 SkToInt(stats.fUnbudgetedSize), SkToInt(fHighWaterBytes));
}

void GrResourceCache::dumpStatsKeyValuePairs(TArray<SkString>* keys,
                                             TArray<double>* values) const {
    this->validate();

    Stats stats;
    this->getStats(&stats);

    keys->push_back(SkString("gpu_cache_purgable_entries")); values->push_back(stats.fNumPurgeable);
}
#endif // defined(GPU_TEST_UTILS)
#endif // GR_CACHE_STATS

#ifdef SK_DEBUG
void GrResourceCache::validate() const {
    // Reduce the frequency of validations for large resource counts.
    static SkRandom gRandom;
    int mask = (SkNextPow2(fCount + 1) >> 5) - 1;
    if (~mask && (gRandom.nextU() & mask)) {
        return;
    }

    struct Stats {
        size_t fBytes;
        int fBudgetedCount;
        size_t fBudgetedBytes;
        int fLocked;
        int fScratch;
        int fCouldBeScratch;
        int fContent;
        const ScratchMap* fScratchMap;
        const UniqueHash* fUniqueHash;

        Stats(const GrResourceCache* cache) {
            memset(this, 0, sizeof(*this));
            fScratchMap = &cache->fScratchMap;
            fUniqueHash = &cache->fUniqueHash;
        }

        void update(GrGpuResource* resource) {
            fBytes += resource->gpuMemorySize();

            if (!resource->resourcePriv().isPurgeable()) {
                ++fLocked;
            }

            const skgpu::ScratchKey& scratchKey = resource->resourcePriv().getScratchKey();
            const skgpu::UniqueKey& uniqueKey = resource->getUniqueKey();

            if (resource->cacheAccess().isUsableAsScratch()) {
                SkASSERT(!uniqueKey.isValid());
                SkASSERT(GrBudgetedType::kBudgeted == resource->resourcePriv().budgetedType());
                SkASSERT(!resource->cacheAccess().hasRef());
                ++fScratch;
                SkASSERT(fScratchMap->countForKey(scratchKey));
                SkASSERT(!resource->resourcePriv().refsWrappedObjects());
            } else if (scratchKey.isValid()) {
                SkASSERT(GrBudgetedType::kBudgeted != resource->resourcePriv().budgetedType() ||
                         uniqueKey.isValid() || resource->cacheAccess().hasRef());
                SkASSERT(!resource->resourcePriv().refsWrappedObjects());
                SkASSERT(!fScratchMap->has(resource, scratchKey));
            }
            if (uniqueKey.isValid()) {
                ++fContent;
                SkASSERT(fUniqueHash->find(uniqueKey) == resource);
                SkASSERT(GrBudgetedType::kBudgeted == resource->resourcePriv().budgetedType() ||
                         resource->resourcePriv().refsWrappedObjects());
            }

            if (GrBudgetedType::kBudgeted == resource->resourcePriv().budgetedType()) {
                ++fBudgetedCount;
                fBudgetedBytes += resource->gpuMemorySize();
            }
        }
    };

    {
        int count = 0;
        fScratchMap.foreach([&](const GrGpuResource& resource) {
            SkASSERT(resource.cacheAccess().isUsableAsScratch());
            count++;
        });
        SkASSERT(count == fScratchMap.count());
    }

    Stats stats(this);
    size_t purgeableBytes = 0;
    int numBudgetedResourcesFlushWillMakePurgeable = 0;

    for (int i = 0; i < fNonpurgeableResources.size(); ++i) {
        SkASSERT(!fNonpurgeableResources[i]->resourcePriv().isPurgeable() ||
                 fNewlyPurgeableResourceForValidation == fNonpurgeableResources[i]);
        SkASSERT(*fNonpurgeableResources[i]->cacheAccess().accessCacheIndex() == i);
        SkASSERT(!fNonpurgeableResources[i]->wasDestroyed());
        if (fNonpurgeableResources[i]->resourcePriv().budgetedType() == GrBudgetedType::kBudgeted &&
            !fNonpurgeableResources[i]->cacheAccess().hasRefOrCommandBufferUsage() &&
            fNewlyPurgeableResourceForValidation != fNonpurgeableResources[i]) {
            ++numBudgetedResourcesFlushWillMakePurgeable;
        }
        stats.update(fNonpurgeableResources[i]);
    }
    for (int i = 0; i < fPurgeableQueue.count(); ++i) {
        SkASSERT(fPurgeableQueue.at(i)->resourcePriv().isPurgeable());
        SkASSERT(*fPurgeableQueue.at(i)->cacheAccess().accessCacheIndex() == i);
        SkASSERT(!fPurgeableQueue.at(i)->wasDestroyed());
        stats.update(fPurgeableQueue.at(i));
        purgeableBytes += fPurgeableQueue.at(i)->gpuMemorySize();
    }

    SkASSERT(fCount == this->getResourceCount());
    SkASSERT(fBudgetedCount <= fCount);
    SkASSERT(fBudgetedBytes <= fBytes);
    SkASSERT(stats.fBytes == fBytes);
    SkASSERT(fNumBudgetedResourcesFlushWillMakePurgeable ==
             numBudgetedResourcesFlushWillMakePurgeable);
    SkASSERT(stats.fBudgetedBytes == fBudgetedBytes);
    SkASSERT(stats.fBudgetedCount == fBudgetedCount);
    SkASSERT(purgeableBytes == fPurgeableBytes);
#if GR_CACHE_STATS
    SkASSERT(fBudgetedHighWaterCount <= fHighWaterCount);
    SkASSERT(fBudgetedHighWaterBytes <= fHighWaterBytes);
    SkASSERT(fBytes <= fHighWaterBytes);
    SkASSERT(fCount <= fHighWaterCount);
    SkASSERT(fBudgetedBytes <= fBudgetedHighWaterBytes);
    SkASSERT(fBudgetedCount <= fBudgetedHighWaterCount);
#endif
    SkASSERT(stats.fContent == fUniqueHash.count());
    SkASSERT(stats.fScratch == fScratchMap.count());

    // This assertion is not currently valid because we can be in recursive notifyCntReachedZero()
    // calls. This will be fixed when subresource registration is explicit.
    // bool overBudget = budgetedBytes > fMaxBytes || budgetedCount > fMaxCount;
    // SkASSERT(!overBudget || locked == count || fPurging);
}

bool GrResourceCache::isInCache(const GrGpuResource* resource) const {
    int index = *resource->cacheAccess().accessCacheIndex();
    if (index < 0) {
        return false;
    }
    if (index < fPurgeableQueue.count() && fPurgeableQueue.at(index) == resource) {
        return true;
    }
    if (index < fNonpurgeableResources.size() && fNonpurgeableResources[index] == resource) {
        return true;
    }
    SkDEBUGFAIL("Resource index should be -1 or the resource should be in the cache.");
    return false;
}

#endif // SK_DEBUG

#if defined(GPU_TEST_UTILS)

int GrResourceCache::countUniqueKeysWithTag(const char* tag) const {
    int count = 0;
    fUniqueHash.foreach([&](const GrGpuResource& resource){
        if (0 == strcmp(tag, resource.getUniqueKey().tag())) {
            ++count;
        }
    });
    return count;
}

void GrResourceCache::changeTimestamp(uint32_t newTimestamp) {
    fTimestamp = newTimestamp;
}

void GrResourceCache::visitSurfaces(
        const std::function<void(const GrSurface*, bool purgeable)>& func) const {

    for (int i = 0; i < fNonpurgeableResources.size(); ++i) {
        if (const GrSurface* surf = fNonpurgeableResources[i]->asSurface()) {
            func(surf, /* purgeable= */ false);
        }
    }
    for (int i = 0; i < fPurgeableQueue.count(); ++i) {
        if (const GrSurface* surf = fPurgeableQueue.at(i)->asSurface()) {
            func(surf, /* purgeable= */ true);
        }
    }
}

#endif // defined(GPU_TEST_UTILS)
