/*
 * Copyright 2011 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "src/gpu/GrAttachment.h"

#include "include/private/GrResourceKey.h"
#include "src/gpu/GrBackendUtils.h"
#include "src/gpu/GrCaps.h"
#include "src/gpu/GrDataUtils.h"
#include "src/gpu/GrGpu.h"

size_t GrAttachment::onGpuMemorySize() const {
    // The GrTexture[RenderTarget] is built up by a bunch of attachments each of which are their
    // own GrGpuResource. Ideally the GrRenderTarget would not be a GrGpuResource and the GrTexture
    // would just merge with the new GrSurface/Attachment world. Then we could just depend on each
    // attachment to give its own size since we don't have GrGpuResources owning other
    // GrGpuResources. Until we get to that point we need to live in some hybrid world. We will let
    // the msaa and stencil attachments track their own size because they do get cached separately.
    // For all GrTexture* based things we will continue to to use the GrTexture* to report size and
    // the owned attachments will have no size and be uncached.
    if (!(fSupportedUsages & UsageFlags::kTexture) && fMemoryless == GrMemoryless::kNo) {
        GrBackendFormat format = this->backendFormat();
        SkImage::CompressionType compression = GrBackendFormatToCompressionType(format);

        uint64_t size = GrNumBlocks(compression, this->dimensions());
        size *= GrBackendFormatBytesPerBlock(this->backendFormat());
        size *= this->numSamples();
        return size;
    }
    return 0;
}

void GrAttachment::dumpMemoryStatistics(SkTraceMemoryDump* traceMemoryDump) const {
    SkString type;
    if (fSupportedUsages == UsageFlags::kStencilAttachment) {
        type = "StencilAttachment";
    }

    if (fSupportedUsages & UsageFlags::kTexture) {
        // This return since the memory should all be handled by the textures
        return;
    } else if (fSupportedUsages & UsageFlags::kColorAttachment) {
        type = "ColorAttachment";
    }

    SkString resourceName = this->getResourceName();
    resourceName.append("/attachment");
    this->dumpMemoryStatisticsPriv(traceMemoryDump, resourceName, type.c_str(), onGpuMemorySize());
}

static void build_key(GrResourceKey::Builder* builder,
                      const GrCaps& caps,
                      const GrBackendFormat& format,
                      SkISize dimensions,
                      GrAttachment::UsageFlags requiredUsage,
                      int sampleCnt,
                      GrMipmapped mipmapped,
                      GrProtected isProtected,
                      GrMemoryless memoryless) {
    SkASSERT(!dimensions.isEmpty());

    SkASSERT(static_cast<uint32_t>(isProtected) <= 1);
    SkASSERT(static_cast<uint32_t>(memoryless) <= 1);
    SkASSERT(static_cast<uint32_t>(requiredUsage) < (1u << 8));
    SkASSERT(static_cast<uint32_t>(sampleCnt) < (1u << (32 - 10)));

    uint64_t formatKey = caps.computeFormatKey(format);
    (*builder)[0] = dimensions.width();
    (*builder)[1] = dimensions.height();
    (*builder)[2] = formatKey & 0xFFFFFFFF;
    (*builder)[3] = (formatKey >> 32) & 0xFFFFFFFF;
    (*builder)[4] = (static_cast<uint32_t>(isProtected) << 0) |
                    (static_cast<uint32_t>(memoryless) << 1) |
                    (static_cast<uint32_t>(requiredUsage) << 2) |
                    (static_cast<uint32_t>(sampleCnt) << 10);
}

void GrAttachment::ComputeSharedAttachmentUniqueKey(const GrCaps& caps,
                                                    const GrBackendFormat& format,
                                                    SkISize dimensions,
                                                    UsageFlags requiredUsage,
                                                    int sampleCnt,
                                                    GrMipmapped mipmapped,
                                                    GrProtected isProtected,
                                                    GrMemoryless memoryless,
                                                    GrUniqueKey* key) {
    static const GrUniqueKey::Domain kDomain = GrUniqueKey::GenerateDomain();

    GrUniqueKey::Builder builder(key, kDomain, 5);
    build_key(&builder, caps, format, dimensions, requiredUsage, sampleCnt, mipmapped, isProtected,
              memoryless);
}

void GrAttachment::ComputeScratchKey(const GrCaps& caps,
                                     const GrBackendFormat& format,
                                     SkISize dimensions,
                                     UsageFlags requiredUsage,
                                     int sampleCnt,
                                     GrMipmapped mipmapped,
                                     GrProtected isProtected,
                                     GrMemoryless memoryless,
                                     GrScratchKey* key) {
    static const GrScratchKey::ResourceType kType = GrScratchKey::GenerateResourceType();

    GrScratchKey::Builder builder(key, kType, 5);
    build_key(&builder, caps, format, dimensions, requiredUsage, sampleCnt, mipmapped, isProtected,
              memoryless);
}

void GrAttachment::computeScratchKey(GrScratchKey* key) const {
    if (!SkToBool(fSupportedUsages & UsageFlags::kStencilAttachment)) {
        auto isProtected = this->isProtected() ? GrProtected::kYes : GrProtected::kNo;
        ComputeScratchKey(*this->getGpu()->caps(),
                          this->backendFormat(),
                          this->dimensions(),
                          fSupportedUsages,
                          this->numSamples(),
                          this->mipmapped(),
                          isProtected,
                          fMemoryless,
                          key);
    }
}
