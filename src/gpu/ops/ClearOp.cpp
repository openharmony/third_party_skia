/*
 * Copyright 2017 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "src/gpu/ops/ClearOp.h"

#include "include/gpu/GrRecordingContext.h"
#include "src/gpu/GrMemoryPool.h"
#include "src/gpu/GrOpFlushState.h"
#include "src/gpu/GrOpsRenderPass.h"
#include "src/gpu/GrProxyProvider.h"
#include "src/gpu/GrRecordingContextPriv.h"

namespace {

bool contains_scissor(const GrScissorState& a, const GrScissorState& b) {
    return !a.enabled() || (b.enabled() && a.rect().contains(b.rect()));
}

} // anonymous namespace

namespace skgpu::v1 {

GrOp::Owner ClearOp::MakeColor(GrRecordingContext* context,
                               const GrScissorState& scissor,
                               std::array<float, 4> color) {
    return GrOp::Make<ClearOp>(context, Buffer::kColor, scissor, color, false);
}

GrOp::Owner ClearOp::MakeStencilClip(GrRecordingContext* context,
                                     const GrScissorState& scissor,
                                     bool insideMask) {
    return GrOp::Make<ClearOp>(context,
                               Buffer::kStencilClip,
                               scissor,
                               std::array<float, 4>(),
                               insideMask);
}

#ifdef SK_ENABLE_STENCIL_CULLING_OHOS
GrOp::Owner ClearOp::MakeStencil(GrRecordingContext* context,
                                 const GrScissorState& scissor,
                                 uint32_t stencilVal) {
    return GrOp::Make<ClearOp>(context,
                               Buffer::kStencil,
                               scissor,
                               stencilVal,
                               true);
}

ClearOp::ClearOp(Buffer buffer,
                 const GrScissorState& scissor,
                 uint32_t stencilVal,
                 bool insideMask)
        : GrOp(ClassID())
        , fScissor(scissor)
        , fStencilVal(stencilVal)
        , fStencilInsideMask(insideMask)
        , fBuffer(buffer) {
    this->setBounds(SkRect::Make(scissor.rect()), HasAABloat::kNo, IsHairline::kNo);
}
#endif

ClearOp::ClearOp(Buffer buffer,
                 const GrScissorState& scissor,
                 std::array<float, 4> color,
                 bool insideMask)
        : GrOp(ClassID())
        , fScissor(scissor)
        , fColor(color)
        , fStencilInsideMask(insideMask)
        , fBuffer(buffer) {
    this->setBounds(SkRect::Make(scissor.rect()), HasAABloat::kNo, IsHairline::kNo);
}

GrOp::CombineResult ClearOp::onCombineIfPossible(GrOp* t, SkArenaAlloc*, const GrCaps& caps) {
    auto other = t->cast<ClearOp>();
#ifdef SK_ENABLE_STENCIL_CULLING_OHOS
    if (fBuffer == Buffer::kStencil || other->fBuffer == Buffer::kStencil) {
        return CombineResult::kCannotCombine;
    }
#endif
    if (other->fBuffer == fBuffer) {
        // This could be much more complicated. Currently we look at cases where the new clear
        // contains the old clear, or when the new clear is a subset of the old clear and they clear
        // to the same value (color or stencil mask depending on target).
        if (contains_scissor(other->fScissor, fScissor)) {
            fScissor = other->fScissor;
            fColor = other->fColor;
            fStencilInsideMask = other->fStencilInsideMask;
            return CombineResult::kMerged;
        } else if (other->fColor == fColor && other->fStencilInsideMask == fStencilInsideMask &&
                   contains_scissor(fScissor, other->fScissor)) {
            return CombineResult::kMerged;
        }
    } else if (other->fScissor == fScissor) {
        // When the scissors are the exact same but the buffers are different, we can combine and
        // clear both stencil and clear together in onExecute().
        if (other->fBuffer & Buffer::kColor) {
            fColor = other->fColor;
        }
        if (other->fBuffer & Buffer::kStencilClip) {
            fStencilInsideMask = other->fStencilInsideMask;
        }
        fBuffer = Buffer::kBoth;
        return CombineResult::kMerged;
    }
    return CombineResult::kCannotCombine;
}

void ClearOp::onExecute(GrOpFlushState* state, const SkRect& chainBounds) {
    SkASSERT(state->opsRenderPass());
    if (fBuffer & Buffer::kColor) {
        state->opsRenderPass()->clear(fScissor, fColor);
    }

    if (fBuffer & Buffer::kStencilClip) {
        state->opsRenderPass()->clearStencilClip(fScissor, fStencilInsideMask);
    }
#ifdef SK_ENABLE_STENCIL_CULLING_OHOS
    if (fBuffer == Buffer::kStencil && !fShouldDisableStencilCulling) {
        TRACE_EVENT0("skia", "StencilCullingOpt ClearOp::onExecute with stencil");
        state->opsRenderPass()->clearStencil(fScissor, fStencilVal);
    }
#endif
}

} // namespace skgpu::v1

