/*
 * Copyright 2014 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "include/core/SkRect.h"
#include "src/gpu/GrFragmentProcessor.h"
#include "src/gpu/effects/GrOvalEffect.h"

GrFPResult GrOvalEffect::Make(std::unique_ptr<GrFragmentProcessor> inputFP, GrClipEdgeType edgeType,
                              const SkRect& oval, const GrShaderCaps& caps) {
    SkScalar w = oval.width();
    SkScalar h = oval.height();
    if (SkScalarNearlyEqual(w, h)) {
        w /= 2;
#ifdef SKIA_OHOS
        return GrFragmentProcessor::CircleSDF(std::move(inputFP), edgeType,
            SkPoint::Make(oval.fLeft + w, oval.fTop + w), w);
#else
        return GrFragmentProcessor::Circle(std::move(inputFP), edgeType,
                                           SkPoint::Make(oval.fLeft + w, oval.fTop + w), w);
#endif
    } else {
        w /= 2;
        h /= 2;
        return GrFragmentProcessor::Ellipse(std::move(inputFP), edgeType,
                                            SkPoint::Make(oval.fLeft + w, oval.fTop + h),
                                            SkPoint::Make(w, h), caps);
    }
    SkUNREACHABLE;
}
