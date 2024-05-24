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
    SkV2 wh = {srcRRect.width(), srcRRect.height()};
    SkVector rr = srcRRect.getSimpleRadii();
    float r = rr.x();

    static auto effect = SkMakeRuntimeEffect(SkRuntimeEffect::MakeForShader,
        "uniform half blurRadius;"
        "uniform vec2 wh;"
        "uniform half r;"

        "float myRoundBoxSDF(vec2 p, vec2 a, float r) {"
            "vec2 q = abs(p)-a + r;"
            "return length(max(q, 0.0)) + min(max(q.x, q.y), 0.0) - r;"
        "}"

        "half4 main(float2 pos) {"
            "vec2 a = vec2(wh.x / 2, wh.y / 2);"
            "float d = myRoundBoxSDF(pos, a, r);"
            "float alpha = smoothstep( blurRadius / 2, -blurRadius / 2, d );"

            "return half4(alpha);"
        "}"
    );

    std::unique_ptr<GrFragmentProcessor> fp = GrSkSLFP::Make(effect, "RRectSDFBlur", nullptr,
        GrSkSLFP::OptFlags::kCompatibleWithCoverageAsAlpha, "blurRadius", blurRadius, "wh", wh, "r", r);
    
    if (!fp) {
        return nullptr;
    }

    SkMatrix matrix;
    matrix.setTranslateX(-noxFormedSigma - srcRRect.rect().fLeft - srcRRect.width() / kHalfFactor);
    matrix.setTranslateY(-noxFormedSigma - srcRRect.rect().fTop - srcRRect.height() / kHalfFactor);

    fp = GrMatrixEffect::Make(matrix, std::move(fp));

    return fp;
}