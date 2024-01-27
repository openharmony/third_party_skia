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

#ifndef Drawing_DEFINED
#define Drawing_DEFINED

#ifdef USE_ROSEN_DRAWING

#include "draw/path.h"
#include "effect/mask_filter.h"
#include "effect/path_effect.h"
#include "recording/recording_canvas.h"
#include "text/font.h"
#include "text/font_metrics.h"
#include "text/font_mgr.h"
#include "text/font_style.h"
#include "text/font_style_set.h"
#include "text/text_blob.h"
#include "text/text_blob_builder.h"
#include "text/typeface.h"

namespace RSDrawing = OHOS::Rosen::Drawing;
using RSBlurType = RSDrawing::BlurType;
using RSBrush = RSDrawing::Brush;
using RSCanvas = RSDrawing::Canvas;
using RSColor = RSDrawing::Color;
using RSFilter = RSDrawing::Filter;
using RSFont = RSDrawing::Font;
using RSFontEdging = RSDrawing::FontEdging;
using RSFontHinting = RSDrawing::FontHinting;
using RSFontMetrics = RSDrawing::FontMetrics;
using RSFontMgr = RSDrawing::FontMgr;
using RSFontStyle = RSDrawing::FontStyle;
using RSFontStyleSet = RSDrawing::FontStyleSet;
using RSPath = RSDrawing::Path;
using RSPen = RSDrawing::Pen;
using RSPaint = RSDrawing::Paint;
using RSPoint = RSDrawing::Point;
using RSPathEffect = RSDrawing::PathEffect;
using RSMaskFilter = RSDrawing::MaskFilter;
using RSRect = RSDrawing::Rect;
using RSRoundRect = RSDrawing::RoundRect;
using RSScalar = RSDrawing::scalar;
using RSTextBlob = RSDrawing::TextBlob;
using RSTextBlobBuilder = RSDrawing::TextBlobBuilder;
using RSTypeface = RSDrawing::Typeface;

#endif

#endif  // Drawing_DEFINED