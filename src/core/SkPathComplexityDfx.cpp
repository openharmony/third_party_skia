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

#include "src/core/SkPathComplexityDfx.h"
#include "src/core/SkTraceEvent.h"

#ifdef SK_ENABLE_PATH_COMPLEXITY_DFX
constexpr int PATH_TRACE_LEVEL = 1;

int GetDebugTraceLevel()
{
    static int openDebugTraceLevel =
        std::atoi((OHOS::system::GetParameter("persist.sys.graphic.openDebugTrace", "0")).c_str());
    return openDebugTraceLevel;
}

bool IsShowPathComplexityEnabled()
{
    static bool enabled =
        std::atoi((OHOS::system::GetParameter("persist.sys.graphic.showPathComplexity", "0")).c_str()) != 0;
    return enabled;
}


void SkPathComplexityDfx::AddPathComplexityTrace(SkScalar complexity)
{
    if (GetDebugTraceLevel() >= PATH_TRACE_LEVEL) {
        HITRACE_OHOS_NAME_FMT_ALWAYS("Path Complexity Debug: %f", complexity);
    }
}
void SkPathComplexityDfx::ShowPathComplexityDfx(SkCanvas* canvas, const SkPath& path)
{
    if (!canvas) {
        return;
    }

    if (IsShowPathComplexityEnabled()) {
        constexpr size_t MESSAGE_SIZE = 4;
        constexpr SkScalar MESSAGE_FONT_SIZE = 30.0f;
        constexpr SkScalar MARGIN_LENGTH = 10.0f;

        SkScalar complexity = 0.f;
        SkScalar avgLength = 0.f;
        compute_complexity(path, avgLength, complexity);
        std::string message = std::to_string(complexity).substr(0, MESSAGE_SIZE);

        SkFont tempFont;
        tempFont.setSize(MESSAGE_FONT_SIZE / (std::abs(canvas->getTotalMatrix().get(0)) + 1e-3));
        SkPaint tempPaint;
        tempPaint.setColor(SK_ColorRED);
        canvas->drawSimpleText(message.c_str(), MESSAGE_SIZE, SkTextEncoding::kUTF8,
            0, MARGIN_LENGTH, tempFont, tempPaint);
    }
}
#endif
