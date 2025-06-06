/*
 * Copyright 2015 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef ClearOp_DEFINED
#define ClearOp_DEFINED

#include "include/gpu/GrTypes.h"
#include "src/gpu/GrScissorState.h"
#include "src/gpu/ops/GrOp.h"

#ifdef SK_ENABLE_STENCIL_CULLING_OHOS
#include "src/gpu/GrOpFlushState.h"
#else
class GrOpFlushState;
#endif

class GrRecordingContext;

namespace skgpu::v1 {

class ClearOp final : public GrOp {
public:
    DEFINE_OP_CLASS_ID

    // A fullscreen or scissored clear, depending on the clip and proxy dimensions
    static GrOp::Owner MakeColor(GrRecordingContext* context,
                                 const GrScissorState& scissor,
                                 std::array<float, 4> color);

    static GrOp::Owner MakeStencilClip(GrRecordingContext* context,
                                       const GrScissorState& scissor,
                                       bool insideMask);
#ifdef SK_ENABLE_STENCIL_CULLING_OHOS
    static GrOp::Owner MakeStencil(GrRecordingContext* context,
                                   const GrScissorState& scissor,
                                   uint32_t stencilVal);
#endif
    const char* name() const override { return "Clear"; }

    const std::array<float, 4>& color() const { return fColor; }
    bool stencilInsideMask() const { return fStencilInsideMask; }
private:
    friend class GrOp; // for ctors

    enum class Buffer {
        kColor       = 0b01,
        kStencilClip = 0b10,

        kBoth        = 0b11,
#ifdef SK_ENABLE_STENCIL_CULLING_OHOS
        kStencil        = 0b100,
#endif
    };
    GR_DECL_BITFIELD_CLASS_OPS_FRIENDS(Buffer);

    ClearOp(Buffer buffer,
            const GrScissorState& scissor,
            std::array<float, 4> color,
            bool stencil);
#ifdef SK_ENABLE_STENCIL_CULLING_OHOS
    ClearOp(Buffer buffer,
            const GrScissorState& scissor,
            uint32_t stencilVal,
            bool stencil);
#endif

    CombineResult onCombineIfPossible(GrOp* t, SkArenaAlloc*, const GrCaps& caps) override;

    void onPrePrepare(GrRecordingContext*, const GrSurfaceProxyView& writeView, GrAppliedClip*,
                      const GrDstProxyView&, GrXferBarrierFlags renderPassXferBarriers,
                      GrLoadOp colorLoadOp) override {}

    void onPrepare(GrOpFlushState* flushState) override {
#ifdef SK_ENABLE_STENCIL_CULLING_OHOS
        fShouldDisableStencilCulling = flushState->fDisableStencilCulling;
#endif
    }

    void onExecute(GrOpFlushState* state, const SkRect& chainBounds) override;
#if GR_TEST_UTILS
    SkString onDumpInfo() const override {
        SkString string("Scissor [ ");
        if (fScissor.enabled()) {
            const SkIRect& r = fScissor.rect();
            string.appendf("L: %d, T: %d, R: %d, B: %d", r.fLeft, r.fTop, r.fRight, r.fBottom);
        } else {
            string.append("disabled");
        }
        string.appendf("], Color: {%g, %g, %g, %g}\n", fColor[0], fColor[1], fColor[2], fColor[3]);
        return string;
    }
#endif

    GrScissorState       fScissor;
    std::array<float, 4> fColor;
#ifdef SK_ENABLE_STENCIL_CULLING_OHOS
    uint32_t             fStencilVal;
#endif
    bool                 fStencilInsideMask;
    Buffer               fBuffer;
};

GR_MAKE_BITFIELD_CLASS_OPS(ClearOp::Buffer)

} // namespace skgpu::v1

#endif // ClearOp_DEFINED
