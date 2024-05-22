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

#ifndef Gr_SDF_Blur_Effect_DEFINED
#define Gr_SDF_Blur_Effect_DEFINED

#include "include/core/SkPoint.h"
#include "src/core/SkRuntimeEffectPriv.h"
#include "src/gpu/effects/GrSkSLFP.h"
#include "src/gpu/effects/GrMatrixEffect.h"
#include "src/gpu/GrFragmentProcessor.h"
#include "include/effects/SkRuntimeEffect.h"

class GrSDFBlurEffect : public GrFragmentProcessor {
public:
    inline static constexpr float kHalfFactor = 2.f;

    static std::unique_ptr<GrFragmentProcessor> Make(GrRecordingContext* context, float noxFormedSigma,
        const SkRRect& srcRRect);
};
#endif
