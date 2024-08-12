/*
 * Copyright (c) 2024 Huawei Device Co., Ltd.
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef BLUROP_DEFINED
#define BLUROP_DEFINED

#include "include/core/SkBlurTypes.h"
#include "include/gpu/GrBackendSurface.h"
#include "include/gpu/GrTypes.h"
#include "src/gpu/ops/GrOp.h"

class GrOpFlushState;
class GrRecordingContext;

namespace skgpu::v1 {

class BlurOp final : public GrOp {
public:
    DEFINE_OP_CLASS_ID

    const char* name() const override { return "Blur"; }

    void visitProxies(const GrVisitProxyFunc& func) const override
    {
        func(fProxyView.proxy(), GrMipmapped(false));
    }
private:
    friend class GrOp; // for ctors

    BlurOp(GrSurfaceProxyView proxyView, const SkBlurArg& blurArg);

    CombineResult onCombineIfPossible(GrOp* t, SkArenaAlloc*, const GrCaps& caps) override;

    void onPrePrepare(GrRecordingContext*, const GrSurfaceProxyView& writeView, GrAppliedClip*,
                      const GrDstProxyView&, GrXferBarrierFlags renderPassXferBarriers,
                      GrLoadOp colorLoadOp) override {}

    void onPrepare(GrOpFlushState*) override {}

    void onExecute(GrOpFlushState* state, const SkRect& chainBounds) override;

    GrSurfaceProxyView    fProxyView;
    SkBlurArg             skBlurArg;
};

} // namespace skgpu::v1

#endif // BLUROP_DEFINED
