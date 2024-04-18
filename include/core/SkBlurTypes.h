/*
 * Copyright 2014 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef SkBlurTypes_DEFINED
#define SkBlurTypes_DEFINED

#include "include/core/SkRect.h"
#include "include/core/SkScalar.h"
#include "include/core/SkTypes.h"

enum SkBlurStyle : int {
    kNormal_SkBlurStyle,  //!< fuzzy inside and outside
    kSolid_SkBlurStyle,   //!< solid inside, fuzzy outside
    kOuter_SkBlurStyle,   //!< nothing inside, fuzzy outside
    kInner_SkBlurStyle,   //!< fuzzy inside, nothing outside

    kLastEnum_SkBlurStyle = kInner_SkBlurStyle,
};

struct SkBlurArg {
    SkRect srcRect;
    SkRect dstRect;
    SkScalar sigma { 1E-6 };
    float saturation { 1.0 };
    float brightness { 1.0 };
    SkBlurArg(const SkRect& srcRect, const SkRect& dstRect, const SkScalar& sigma,
        float saturation, float brightness)
    {
        this->srcRect = srcRect;
        this->dstRect = dstRect;
        this->sigma = sigma;
        this->saturation = saturation;
        this->brightness = brightness;
    }
};

#endif
