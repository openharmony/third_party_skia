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

#include "src/gpu/ops/BlurOp.h"

#include "src/gpu/GrOpFlushState.h"
#include "src/gpu/GrOpsRenderPass.h"

namespace skgpu::v1 {

BlurOp::BlurOp(GrSurfaceProxyView proxyView, const SkBlurArg& blurArg)
    : GrOp(ClassID()),
      fProxyView(proxyView),
      skBlurArg(blurArg){
    this->setBounds(skBlurArg.srcRect, HasAABloat::kNo, IsHairline::kNo);
}

GrOp::CombineResult BlurOp::onCombineIfPossible(GrOp* t, SkArenaAlloc*, const GrCaps& caps) {
    return CombineResult::kCannotCombine;
}

void BlurOp::onExecute(GrOpFlushState* state, const SkRect& chainBounds) {
    SkASSERT(state->opsRenderPass());
    state->opsRenderPass()->drawBlurImage(fProxyView.proxy(), skBlurArg);
}

} // namespace skgpu::v1
