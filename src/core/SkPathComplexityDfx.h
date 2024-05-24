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

#ifndef SK_PATH_COMPEXITY_DFX_H
#define SK_PATH_COMPEXITY_DFX_H

#include "include/core/SkCanvas.h"
#include "include/core/SkFont.h"
#include "include/core/SkPaint.h"
#include "include/core/SkPath.h"
#include "include/core/SkScalar.h"

#ifdef SK_ENABLE_PATH_COMPLEXITY_DFX
#include "hitrace_meter.h"
#include <parameters.h>

class SkPathComplexityDfx {
public:
    static void AddPathComplexityTrace(SkScalar complexity);
    static void ShowPathComplexityDfx(SkCanvas* canvas, const SkPath& path);
};
#endif

void compute_complexity(const SkPath& path, SkScalar& avgLength, SkScalar& complexity);

#endif
