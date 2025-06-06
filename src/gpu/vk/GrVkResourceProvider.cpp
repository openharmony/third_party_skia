/*
* Copyright 2016 Google Inc.
*
* Use of this source code is governed by a BSD-style license that can be
* found in the LICENSE file.
*/

#include "src/gpu/vk/GrVkResourceProvider.h"

#include "include/gpu/GrDirectContext.h"
#include "src/core/SkTaskGroup.h"
#include "src/core/SkTraceEvent.h"
#include "src/gpu/GrDirectContextPriv.h"
#include "src/gpu/GrSamplerState.h"
#include "src/gpu/GrStencilSettings.h"
#include "src/gpu/vk/GrVkCommandBuffer.h"
#include "src/gpu/vk/GrVkCommandPool.h"
#include "src/gpu/vk/GrVkGpu.h"
#include "src/gpu/vk/GrVkPipeline.h"
#include "src/gpu/vk/GrVkRenderTarget.h"
#include "src/gpu/vk/GrVkUtil.h"

GrVkResourceProvider::GrVkResourceProvider(GrVkGpu* gpu)
    : fGpu(gpu)
    , fPipelineCache(VK_NULL_HANDLE)
    , fPipelineCacheSize(0) {
    fPipelineStateCache = sk_make_sp<PipelineStateCache>(gpu);
}

GrVkResourceProvider::~GrVkResourceProvider() {
    SkASSERT(0 == fRenderPassArray.count());
    SkASSERT(0 == fExternalRenderPasses.count());
    SkASSERT(0 == fMSAALoadPipelines.count());
    SkASSERT(VK_NULL_HANDLE == fPipelineCache);
}

VkPipelineCache GrVkResourceProvider::pipelineCache() {
    if (fPipelineCache == VK_NULL_HANDLE) {
        VkPipelineCacheCreateInfo createInfo;
        memset(&createInfo, 0, sizeof(VkPipelineCacheCreateInfo));
        createInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO;
        createInfo.pNext = nullptr;
        createInfo.flags = 0;

        auto persistentCache = fGpu->getContext()->priv().getPersistentCache();
        sk_sp<SkData> cached;
        if (persistentCache) {
            uint32_t key = GrVkGpu::kPipelineCache_PersistentCacheKeyType;
            sk_sp<SkData> keyData = SkData::MakeWithoutCopy(&key, sizeof(uint32_t));
            cached = persistentCache->load(*keyData);
        }
        bool usedCached = false;
        if (cached) {
            uint32_t* cacheHeader = (uint32_t*)cached->data();
            if (cacheHeader[1] == VK_PIPELINE_CACHE_HEADER_VERSION_ONE) {
                // For version one of the header, the total header size is 16 bytes plus
                // VK_UUID_SIZE bytes. See Section 9.6 (Pipeline Cache) in the vulkan spec to see
                // the breakdown of these bytes.
                SkASSERT(cacheHeader[0] == 16 + VK_UUID_SIZE);
                const VkPhysicalDeviceProperties& devProps = fGpu->physicalDeviceProperties();
                const uint8_t* supportedPipelineCacheUUID = devProps.pipelineCacheUUID;
                if (cacheHeader[2] == devProps.vendorID && cacheHeader[3] == devProps.deviceID &&
                    !memcmp(&cacheHeader[4], supportedPipelineCacheUUID, VK_UUID_SIZE)) {
                    createInfo.initialDataSize = cached->size();
                    createInfo.pInitialData = cached->data();
                    usedCached = true;
                }
            }
        }
        if (!usedCached) {
            createInfo.initialDataSize = 0;
            createInfo.pInitialData = nullptr;
        }

        VkResult result;
        GR_VK_CALL_RESULT(fGpu, result, CreatePipelineCache(fGpu->device(), &createInfo, nullptr,
                                                            &fPipelineCache));
        if (VK_SUCCESS != result) {
            fPipelineCache = VK_NULL_HANDLE;
        }
    }
    return fPipelineCache;
}

void GrVkResourceProvider::init() {
    // Init uniform descriptor objects
    GrVkDescriptorSetManager* dsm = GrVkDescriptorSetManager::CreateUniformManager(fGpu);
    fDescriptorSetManagers.emplace_back(dsm);
    SkASSERT(1 == fDescriptorSetManagers.count());
    fUniformDSHandle = GrVkDescriptorSetManager::Handle(0);
    dsm = GrVkDescriptorSetManager::CreateInputManager(fGpu);
    fDescriptorSetManagers.emplace_back(dsm);
    SkASSERT(2 == fDescriptorSetManagers.count());
    fInputDSHandle = GrVkDescriptorSetManager::Handle(1);
}

sk_sp<const GrVkPipeline> GrVkResourceProvider::makePipeline(
        const GrProgramInfo& programInfo,
        VkPipelineShaderStageCreateInfo* shaderStageInfo,
        int shaderStageCount,
        VkRenderPass compatibleRenderPass,
        VkPipelineLayout layout,
        uint32_t subpass) {
    return GrVkPipeline::Make(fGpu, programInfo, shaderStageInfo, shaderStageCount,
                              compatibleRenderPass, layout, this->pipelineCache(), subpass);
}

// To create framebuffers, we first need to create a simple RenderPass that is
// only used for framebuffer creation. When we actually render we will create
// RenderPasses as needed that are compatible with the framebuffer.
const GrVkRenderPass*
GrVkResourceProvider::findCompatibleRenderPass(GrVkRenderTarget* target,
                                               CompatibleRPHandle* compatibleHandle,
                                               bool withResolve,
                                               bool withStencil,
                                               SelfDependencyFlags selfDepFlags,
                                               LoadFromResolve loadFromResolve) {
    // Get attachment information from render target. This includes which attachments the render
    // target has (color, stencil) and the attachments format and sample count.
    GrVkRenderPass::AttachmentFlags attachmentFlags;
    GrVkRenderPass::AttachmentsDescriptor attachmentsDesc;
    target->getAttachmentsDescriptor(&attachmentsDesc, &attachmentFlags, withResolve, withStencil);

    return this->findCompatibleRenderPass(&attachmentsDesc, attachmentFlags, selfDepFlags,
                                          loadFromResolve, compatibleHandle);
}

const GrVkRenderPass*
GrVkResourceProvider::findCompatibleRenderPass(GrVkRenderPass::AttachmentsDescriptor* desc,
                                               GrVkRenderPass::AttachmentFlags attachmentFlags,
                                               SelfDependencyFlags selfDepFlags,
                                               LoadFromResolve loadFromResolve,
                                               CompatibleRPHandle* compatibleHandle) {
    for (int i = 0; i < fRenderPassArray.count(); ++i) {
        if (fRenderPassArray[i].isCompatible(*desc, attachmentFlags, selfDepFlags,
                                             loadFromResolve)) {
            const GrVkRenderPass* renderPass = fRenderPassArray[i].getCompatibleRenderPass();
            renderPass->ref();
            if (compatibleHandle) {
                *compatibleHandle = CompatibleRPHandle(i);
            }
            return renderPass;
        }
    }

    GrVkRenderPass* renderPass = GrVkRenderPass::CreateSimple(fGpu, desc, attachmentFlags,
                                                              selfDepFlags, loadFromResolve);
    if (!renderPass) {
        return nullptr;
    }
    fRenderPassArray.emplace_back(renderPass);

    if (compatibleHandle) {
        *compatibleHandle = CompatibleRPHandle(fRenderPassArray.count() - 1);
    }
    return renderPass;
}

const GrVkRenderPass* GrVkResourceProvider::findCompatibleExternalRenderPass(
        VkRenderPass renderPass, uint32_t colorAttachmentIndex) {
    for (int i = 0; i < fExternalRenderPasses.count(); ++i) {
        if (fExternalRenderPasses[i]->isCompatibleExternalRP(renderPass)) {
            fExternalRenderPasses[i]->ref();
#ifdef SK_DEBUG
            uint32_t cachedColorIndex;
            SkASSERT(fExternalRenderPasses[i]->colorAttachmentIndex(&cachedColorIndex));
            SkASSERT(cachedColorIndex == colorAttachmentIndex);
#endif
            return fExternalRenderPasses[i];
        }
    }

    const GrVkRenderPass* newRenderPass = new GrVkRenderPass(fGpu, renderPass,
                                                             colorAttachmentIndex);
    fExternalRenderPasses.push_back(newRenderPass);
    newRenderPass->ref();
    return newRenderPass;
}

const GrVkRenderPass* GrVkResourceProvider::findRenderPass(
        GrVkRenderTarget* target,
        const GrVkRenderPass::LoadStoreOps& colorOps,
        const GrVkRenderPass::LoadStoreOps& resolveOps,
        const GrVkRenderPass::LoadStoreOps& stencilOps,
        CompatibleRPHandle* compatibleHandle,
        bool withResolve,
        bool withStencil,
        SelfDependencyFlags selfDepFlags,
        LoadFromResolve loadFromResolve) {
    GrVkResourceProvider::CompatibleRPHandle tempRPHandle;
    GrVkResourceProvider::CompatibleRPHandle* pRPHandle = compatibleHandle ? compatibleHandle
                                                                           : &tempRPHandle;
    *pRPHandle = target->compatibleRenderPassHandle(withResolve, withStencil, selfDepFlags,
                                                    loadFromResolve);
    if (!pRPHandle->isValid()) {
        return nullptr;
    }

    return this->findRenderPass(*pRPHandle, colorOps, resolveOps, stencilOps);
}

const GrVkRenderPass*
GrVkResourceProvider::findRenderPass(const CompatibleRPHandle& compatibleHandle,
                                     const GrVkRenderPass::LoadStoreOps& colorOps,
                                     const GrVkRenderPass::LoadStoreOps& resolveOps,
                                     const GrVkRenderPass::LoadStoreOps& stencilOps) {
    SkASSERT(compatibleHandle.isValid() && compatibleHandle.toIndex() < fRenderPassArray.count());
    CompatibleRenderPassSet& compatibleSet = fRenderPassArray[compatibleHandle.toIndex()];
    const GrVkRenderPass* renderPass = compatibleSet.getRenderPass(fGpu,
                                                                   colorOps,
                                                                   resolveOps,
                                                                   stencilOps);
    if (!renderPass) {
        return nullptr;
    }
    renderPass->ref();
    return renderPass;
}

GrVkDescriptorPool* GrVkResourceProvider::findOrCreateCompatibleDescriptorPool(
                                                            VkDescriptorType type, uint32_t count) {
    return GrVkDescriptorPool::Create(fGpu, type, count);
}

GrVkSampler* GrVkResourceProvider::findOrCreateCompatibleSampler(
        GrSamplerState params, const GrVkYcbcrConversionInfo& ycbcrInfo) {
    GrVkSampler* sampler = fSamplers.find(GrVkSampler::GenerateKey(params, ycbcrInfo));
    if (!sampler) {
        sampler = GrVkSampler::Create(fGpu, params, ycbcrInfo);
        if (!sampler) {
            return nullptr;
        }
        fSamplers.add(sampler);
    }
    SkASSERT(sampler);
    sampler->ref();
    return sampler;
}

GrVkSamplerYcbcrConversion* GrVkResourceProvider::findOrCreateCompatibleSamplerYcbcrConversion(
        const GrVkYcbcrConversionInfo& ycbcrInfo) {
    GrVkSamplerYcbcrConversion* ycbcrConversion =
            fYcbcrConversions.find(GrVkSamplerYcbcrConversion::GenerateKey(ycbcrInfo));
    if (!ycbcrConversion) {
        ycbcrConversion = GrVkSamplerYcbcrConversion::Create(fGpu, ycbcrInfo);
        if (!ycbcrConversion) {
            return nullptr;
        }
        fYcbcrConversions.add(ycbcrConversion);
    }
    SkASSERT(ycbcrConversion);
    ycbcrConversion->ref();
    return ycbcrConversion;
}

GrVkPipelineState* GrVkResourceProvider::findOrCreateCompatiblePipelineState(
        GrRenderTarget* renderTarget,
        const GrProgramInfo& programInfo,
        VkRenderPass compatibleRenderPass,
        bool overrideSubpassForResolveLoad) {
    return fPipelineStateCache->findOrCreatePipelineState(renderTarget, programInfo,
                                                          compatibleRenderPass,
                                                          overrideSubpassForResolveLoad);
}

GrVkPipelineState* GrVkResourceProvider::findOrCreateCompatiblePipelineState(
        const GrProgramDesc& desc,
        const GrProgramInfo& programInfo,
        VkRenderPass compatibleRenderPass,
        GrThreadSafePipelineBuilder::Stats::ProgramCacheResult* stat) {

    auto tmp =  fPipelineStateCache->findOrCreatePipelineState(desc, programInfo,
                                                               compatibleRenderPass, stat);
    if (!tmp) {
        fPipelineStateCache->stats()->incNumPreCompilationFailures();
    } else {
        fPipelineStateCache->stats()->incNumPreProgramCacheResult(*stat);
    }

    return tmp;
}

sk_sp<const GrVkPipeline> GrVkResourceProvider::findOrCreateMSAALoadPipeline(
        const GrVkRenderPass& renderPass,
        int numSamples,
        VkPipelineShaderStageCreateInfo* shaderStageInfo,
        VkPipelineLayout pipelineLayout) {
    // Find or Create a compatible pipeline
    sk_sp<const GrVkPipeline> pipeline;
    for (int i = 0; i < fMSAALoadPipelines.count() && !pipeline; ++i) {
        if (fMSAALoadPipelines[i].fRenderPass->isCompatible(renderPass)) {
            pipeline = fMSAALoadPipelines[i].fPipeline;
        }
    }
    if (!pipeline) {
        pipeline = GrVkPipeline::Make(
                fGpu,
                /*vertexAttribs=*/GrGeometryProcessor::AttributeSet(),
                /*instanceAttribs=*/GrGeometryProcessor::AttributeSet(),
                GrPrimitiveType::kTriangleStrip,
                kTopLeft_GrSurfaceOrigin,
                GrStencilSettings(),
                numSamples,
                /*isHWantialiasState=*/false,
                GrXferProcessor::BlendInfo(),
                /*isWireframe=*/false,
                /*useConservativeRaster=*/false,
                /*subpass=*/0,
                shaderStageInfo,
                /*shaderStageCount=*/2,
                renderPass.vkRenderPass(),
                pipelineLayout,
                /*ownsLayout=*/false,
                this->pipelineCache());
        if (!pipeline) {
            return nullptr;
        }
        fMSAALoadPipelines.push_back({pipeline, &renderPass});
    }
    SkASSERT(pipeline);
    return pipeline;
}

void GrVkResourceProvider::getZeroSamplerDescriptorSetHandle(
        GrVkDescriptorSetManager::Handle* handle) {
    SkASSERT(handle);
    for (int i = 0; i < fDescriptorSetManagers.count(); ++i) {
        if (fDescriptorSetManagers[i]->isZeroSampler()) {
            *handle = GrVkDescriptorSetManager::Handle(i);
            return;
        }
    }

    GrVkDescriptorSetManager* dsm =
            GrVkDescriptorSetManager::CreateZeroSamplerManager(fGpu);
    fDescriptorSetManagers.emplace_back(dsm);
    *handle = GrVkDescriptorSetManager::Handle(fDescriptorSetManagers.count() - 1);
}

void GrVkResourceProvider::getSamplerDescriptorSetHandle(VkDescriptorType type,
                                                         const GrVkUniformHandler& uniformHandler,
                                                         GrVkDescriptorSetManager::Handle* handle) {
    SkASSERT(handle);
    SkASSERT(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER == type ||
             VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER == type);
    for (int i = 0; i < fDescriptorSetManagers.count(); ++i) {
        if (fDescriptorSetManagers[i]->isCompatible(type, &uniformHandler)) {
           *handle = GrVkDescriptorSetManager::Handle(i);
           return;
        }
    }

    GrVkDescriptorSetManager* dsm = GrVkDescriptorSetManager::CreateSamplerManager(fGpu, type,
                                                                                   uniformHandler);
    fDescriptorSetManagers.emplace_back(dsm);
    *handle = GrVkDescriptorSetManager::Handle(fDescriptorSetManagers.count() - 1);
}

VkDescriptorSetLayout GrVkResourceProvider::getUniformDSLayout() const {
    SkASSERT(fUniformDSHandle.isValid());
    return fDescriptorSetManagers[fUniformDSHandle.toIndex()]->layout();
}

VkDescriptorSetLayout GrVkResourceProvider::getInputDSLayout() const {
    SkASSERT(fInputDSHandle.isValid());
    return fDescriptorSetManagers[fInputDSHandle.toIndex()]->layout();
}

VkDescriptorSetLayout GrVkResourceProvider::getSamplerDSLayout(
        const GrVkDescriptorSetManager::Handle& handle) const {
    SkASSERT(handle.isValid());
    return fDescriptorSetManagers[handle.toIndex()]->layout();
}

const GrVkDescriptorSet* GrVkResourceProvider::getUniformDescriptorSet() {
    SkASSERT(fUniformDSHandle.isValid());
    return fDescriptorSetManagers[fUniformDSHandle.toIndex()]->getDescriptorSet(fGpu,
                                                                                fUniformDSHandle);
}

const GrVkDescriptorSet* GrVkResourceProvider::getInputDescriptorSet() {
    SkASSERT(fInputDSHandle.isValid());
    return fDescriptorSetManagers[fInputDSHandle.toIndex()]->getDescriptorSet(fGpu, fInputDSHandle);
}

const GrVkDescriptorSet* GrVkResourceProvider::getSamplerDescriptorSet(
        const GrVkDescriptorSetManager::Handle& handle) {
    SkASSERT(handle.isValid());
    return fDescriptorSetManagers[handle.toIndex()]->getDescriptorSet(fGpu, handle);
}

void GrVkResourceProvider::recycleDescriptorSet(const GrVkDescriptorSet* descSet,
                                                const GrVkDescriptorSetManager::Handle& handle) {
    SkASSERT(descSet);
    SkASSERT(handle.isValid());
    int managerIdx = handle.toIndex();
    SkASSERT(managerIdx < fDescriptorSetManagers.count());
    fDescriptorSetManagers[managerIdx]->recycleDescriptorSet(descSet);
}

GrVkCommandPool* GrVkResourceProvider::findOrCreateCommandPool() {
    SkAutoMutexExclusive lock(fBackgroundMutex);
    GrVkCommandPool* result;
    if (fAvailableCommandPools.count()) {
        result = fAvailableCommandPools.back();
        fAvailableCommandPools.pop_back();
    } else {
        result = GrVkCommandPool::Create(fGpu);
        if (!result) {
            return nullptr;
        }
    }
    SkASSERT(result->unique());
    SkDEBUGCODE(
        for (const GrVkCommandPool* pool : fActiveCommandPools) {
            SkASSERT(pool != result);
        }
        for (const GrVkCommandPool* pool : fAvailableCommandPools) {
            SkASSERT(pool != result);
        }
    )
    fActiveCommandPools.push_back(result);
    result->ref();
    return result;
}

void GrVkResourceProvider::checkCommandBuffers() {
    // When resetting a command buffer it can trigger client provided procs (e.g. release or
    // finished) to be called. During these calls the client could trigger us to abandon the vk
    // context, e.g. if we are in a DEVICE_LOST state. When we abandon the vk context we will
    // unref all the fActiveCommandPools and reset the array. Since this can happen in the middle
    // of the loop here, we need to additionally check that fActiveCommandPools still has pools on
    // each iteration.
    //
    // TODO: We really need to have a more robust way to protect us from client proc calls that
    // happen in the middle of us doing work. This may be just one of many potential pitfalls that
    // could happen from the client triggering GrDirectContext changes during a proc call.
    for (int i = fActiveCommandPools.count() - 1; fActiveCommandPools.count() && i >= 0; --i) {
        GrVkCommandPool* pool = fActiveCommandPools[i];
        if (!pool->isOpen()) {
            GrVkPrimaryCommandBuffer* buffer = pool->getPrimaryCommandBuffer();
            if (buffer->finished(fGpu)) {
                fActiveCommandPools.removeShuffle(i);
                // This passes ownership of the pool to the backgroundReset call. The pool should
                // not be used again from this function.
                // TODO: We should see if we can use sk_sps here to make this more explicit.
                this->backgroundReset(pool);
            }
        }
    }
    if (fActiveCommandPools.count() >= 3000) {
#ifdef SKIA_OHOS
        SK_LOGE("GrVkResourceProvider::checkCommandBuffers: fActiveCommandPools size [%{public}d]", fActiveCommandPools.count());
#endif
    }
}

void GrVkResourceProvider::forceSyncAllCommandBuffers() {
    for (int i = fActiveCommandPools.count() - 1; fActiveCommandPools.count() && i >= 0; --i) {
        GrVkCommandPool* pool = fActiveCommandPools[i];
        if (!pool->isOpen()) {
            GrVkPrimaryCommandBuffer* buffer = pool->getPrimaryCommandBuffer();
            buffer->forceSync(fGpu);
        }
    }
}

void GrVkResourceProvider::addFinishedProcToActiveCommandBuffers(
        sk_sp<GrRefCntedCallback> finishedCallback) {
    for (int i = 0; i < fActiveCommandPools.count(); ++i) {
        GrVkCommandPool* pool = fActiveCommandPools[i];
        GrVkPrimaryCommandBuffer* buffer = pool->getPrimaryCommandBuffer();
        buffer->addFinishedProc(finishedCallback);
    }
}

void GrVkResourceProvider::destroyResources() {
    SkTaskGroup* taskGroup = fGpu->getContext()->priv().getTaskGroup();
    if (taskGroup) {
        taskGroup->wait();
    }

    // Release all msaa load pipelines
    fMSAALoadPipelines.reset();

    // loop over all render pass sets to make sure we destroy all the internal VkRenderPasses
    for (int i = 0; i < fRenderPassArray.count(); ++i) {
        fRenderPassArray[i].releaseResources();
    }
    fRenderPassArray.reset();

    for (int i = 0; i < fExternalRenderPasses.count(); ++i) {
        fExternalRenderPasses[i]->unref();
    }
    fExternalRenderPasses.reset();

    // Iterate through all store GrVkSamplers and unref them before resetting the hash table.
    fSamplers.foreach([&](auto* elt) { elt->unref(); });
    fSamplers.reset();

    fYcbcrConversions.foreach([&](auto* elt) { elt->unref(); });
    fYcbcrConversions.reset();

    fPipelineStateCache->release();

    GR_VK_CALL(fGpu->vkInterface(), DestroyPipelineCache(fGpu->device(), fPipelineCache, nullptr));
    fPipelineCache = VK_NULL_HANDLE;

    for (GrVkCommandPool* pool : fActiveCommandPools) {
        SkASSERT(pool->unique());
        pool->unref();
    }
    fActiveCommandPools.reset();

    {
        SkAutoMutexExclusive lock(fBackgroundMutex);
        for (GrVkCommandPool* pool : fAvailableCommandPools) {
            SkASSERT(pool->unique());
            pool->unref();
        }
        fAvailableCommandPools.reset();
    }

    // We must release/destroy all command buffers and pipeline states before releasing the
    // GrVkDescriptorSetManagers. Additionally, we must release all uniform buffers since they hold
    // refs to GrVkDescriptorSets.
    for (int i = 0; i < fDescriptorSetManagers.count(); ++i) {
        fDescriptorSetManagers[i]->release(fGpu);
    }
    fDescriptorSetManagers.reset();

}

void GrVkResourceProvider::releaseUnlockedBackendObjects() {
    SkAutoMutexExclusive lock(fBackgroundMutex);
    for (GrVkCommandPool* pool : fAvailableCommandPools) {
        SkASSERT(pool->unique());
        pool->unref();
    }
    fAvailableCommandPools.reset();
}

void GrVkResourceProvider::backgroundReset(GrVkCommandPool* pool) {
    TRACE_EVENT0("skia.gpu", TRACE_FUNC);
    SkASSERT(pool->unique());
    pool->releaseResources();
    // After releasing resources we may have called a client callback proc which may have
    // disconnected the GrVkGpu. In that case we do not want to push the pool back onto the cache,
    // but instead just drop the pool.
    if (fGpu->disconnected()) {
        pool->unref();
        return;
    }
    SkTaskGroup* taskGroup = fGpu->getContext()->priv().getTaskGroup();
    if (taskGroup) {
        taskGroup->add([this, pool]() {
            this->reset(pool);
        });
    } else {
        this->reset(pool);
    }
}

void GrVkResourceProvider::reset(GrVkCommandPool* pool) {
    TRACE_EVENT0("skia.gpu", TRACE_FUNC);
    SkASSERT(pool->unique());
    pool->reset(fGpu);
    SkAutoMutexExclusive lock(fBackgroundMutex);
    fAvailableCommandPools.push_back(pool);
}

void GrVkResourceProvider::storePipelineCacheData() {
    if (this->pipelineCache() == VK_NULL_HANDLE) {
        return;
    }
    size_t dataSize = 0;
    VkResult result;
    GR_VK_CALL_RESULT(fGpu, result, GetPipelineCacheData(fGpu->device(), this->pipelineCache(),
                                                         &dataSize, nullptr));
    if (result != VK_SUCCESS) {
        return;
    }
    // store VkPipelineCache when cache size update
    if (dataSize == fPipelineCacheSize) {
        return;
    }
    std::unique_ptr<uint8_t[]> data(new uint8_t[dataSize]);

    GR_VK_CALL_RESULT(fGpu, result, GetPipelineCacheData(fGpu->device(), this->pipelineCache(),
                                                         &dataSize, (void*)data.get()));
    if (result != VK_SUCCESS) {
        return;
    }
    fPipelineCacheSize = dataSize;
    uint32_t key = GrVkGpu::kPipelineCache_PersistentCacheKeyType;
    sk_sp<SkData> keyData = SkData::MakeWithoutCopy(&key, sizeof(uint32_t));

    SkDebugf("store vkPipelineCache, data size:%zu", fPipelineCacheSize);
    fGpu->getContext()->priv().getPersistentCache()->store(
            *keyData, *SkData::MakeWithoutCopy(data.get(), dataSize), SkString("VkPipelineCache"));
}

////////////////////////////////////////////////////////////////////////////////

GrVkResourceProvider::CompatibleRenderPassSet::CompatibleRenderPassSet(GrVkRenderPass* renderPass)
        : fLastReturnedIndex(0) {
    renderPass->ref();
    fRenderPasses.push_back(renderPass);
}

bool GrVkResourceProvider::CompatibleRenderPassSet::isCompatible(
        const GrVkRenderPass::AttachmentsDescriptor& attachmentsDescriptor,
        GrVkRenderPass::AttachmentFlags attachmentFlags,
        SelfDependencyFlags selfDepFlags,
        LoadFromResolve loadFromResolve) const {
    // The first GrVkRenderpass should always exists since we create the basic load store
    // render pass on create
    SkASSERT(fRenderPasses[0]);
    return fRenderPasses[0]->isCompatible(attachmentsDescriptor, attachmentFlags, selfDepFlags,
                                          loadFromResolve);
}

GrVkRenderPass* GrVkResourceProvider::CompatibleRenderPassSet::getRenderPass(
        GrVkGpu* gpu,
        const GrVkRenderPass::LoadStoreOps& colorOps,
        const GrVkRenderPass::LoadStoreOps& resolveOps,
        const GrVkRenderPass::LoadStoreOps& stencilOps) {
    for (int i = 0; i < fRenderPasses.count(); ++i) {
        int idx = (i + fLastReturnedIndex) % fRenderPasses.count();
        if (fRenderPasses[idx]->equalLoadStoreOps(colorOps, resolveOps, stencilOps)) {
            fLastReturnedIndex = idx;
            return fRenderPasses[idx];
        }
    }
    GrVkRenderPass* renderPass = GrVkRenderPass::Create(gpu, *this->getCompatibleRenderPass(),
                                                        colorOps, resolveOps, stencilOps);
    if (!renderPass) {
        return nullptr;
    }
    fRenderPasses.push_back(renderPass);
    fLastReturnedIndex = fRenderPasses.count() - 1;
    return renderPass;
}

void GrVkResourceProvider::CompatibleRenderPassSet::releaseResources() {
    for (int i = 0; i < fRenderPasses.count(); ++i) {
        if (fRenderPasses[i]) {
            fRenderPasses[i]->unref();
            fRenderPasses[i] = nullptr;
        }
    }
}

