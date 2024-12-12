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

#include "src/gpu/effects/GrSDFBlurEffect.h"
#include "src/gpu/effects/GrMatrixEffect.h"

std::unique_ptr<GrFragmentProcessor> GrSDFBlurEffect::Make(GrRecordingContext* context,
    float noxFormedSigma, const SkRRect& srcRRect)
{
    float blurRadius = noxFormedSigma;
    float r = srcRRect.getSimpleRadii().x();
    float areaLen = std::max(std::min({srcRRect.width(), srcRRect.height(), blurRadius}) * SK_ScalarHalf, r);

    static auto effect = SkMakeRuntimeEffect(SkRuntimeEffect::MakeForShader,
        "uniform half blurRadius;"
        "uniform half areaLen;"
        "uniform half r;"

        "float myRoundBoxSDF(vec2 p, float a, float r) {"
            "vec2 q = p -a + r;"
            "return length(max(q, 0.0)) + min(max(q.x, q.y), 0.0) - r;"
        "}"

        "half4 main(float2 pos) {"
            "float d = myRoundBoxSDF(pos, areaLen, r);"
            "float alpha = smoothstep( blurRadius / 2, -blurRadius / 2, d );"

            "return half4(alpha);"
        "}"
    );

    std::unique_ptr<GrFragmentProcessor> fp = GrSkSLFP::Make(effect, "RRectSDFBlur", nullptr,
        GrSkSLFP::OptFlags::kCompatibleWithCoverageAsAlpha, "blurRadius", blurRadius, "areaLen", areaLen, "r", r);
    
    if (!fp) {
        return nullptr;
    }
    return fp;
}