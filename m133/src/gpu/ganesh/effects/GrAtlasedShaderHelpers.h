/*
 * Copyright 2017 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef GrAtlasedShaderHelpers_DEFINED
#define GrAtlasedShaderHelpers_DEFINED

#include "include/private/base/SkAssert.h"
#include "src/core/SkSLTypeShared.h"
#include "src/gpu/ganesh/GrGeometryProcessor.h"
#include "src/gpu/ganesh/GrShaderCaps.h"
#include "src/gpu/ganesh/glsl/GrGLSLFragmentShaderBuilder.h"
#include "src/gpu/ganesh/glsl/GrGLSLVarying.h"
#include "src/gpu/ganesh/glsl/GrGLSLVertexGeoBuilder.h"

static inline void append_index_uv_varyings(GrGeometryProcessor::ProgramImpl::EmitArgs& args,
                                            int numTextureSamplers,
                                            const char* inTexCoordsName,
                                            const char* atlasDimensionsInvName,
                                            GrGLSLVarying* uv,
                                            GrGLSLVarying* texIdx,
                                            GrGLSLVarying* st) {
    using Interpolation = GrGLSLVaryingHandler::Interpolation;
    // This extracts the texture index and texel coordinates from the same variable
    // Packing structure: texel coordinates have the 2-bit texture page encoded in bits 13 & 14 of
    // the x coordinate. It would be nice to use bits 14 and 15, but iphone6 has problem with those
    // bits when in gles. Iphone6 works fine with bits 14 and 15 in metal.
    if (args.fShaderCaps->fIntegerSupport) {
        if (numTextureSamplers <= 1) {
            args.fVertBuilder->codeAppendf(
                "int texIdx = 0;"
                "float2 unormTexCoords = float2(%s.x, %s.y);"
           , inTexCoordsName, inTexCoordsName);
        } else {
#ifdef SK_ENABLE_SMALL_PAGE
            args.fVertBuilder->codeAppendf(R"code(
                int2 coords = int2(%s.x, %s.y);
                int texIdx = ((coords.x >> 12) & 0xc) + ((coords.y >> 14) & 0x3);
                float2 unormTexCoords = float2(coords.x & 0x3FFF, coords.y & 0x3FFF);
            )code", inTexCoordsName, inTexCoordsName);
#else
            args.fVertBuilder->codeAppendf(
                "int2 coords = int2(%s.x, %s.y);"
                "int texIdx = coords.x >> 13;"
                "float2 unormTexCoords = float2(coords.x & 0x1FFF, coords.y);"
            , inTexCoordsName, inTexCoordsName);
#endif
        }
    } else {
        if (numTextureSamplers <= 1) {
            args.fVertBuilder->codeAppendf(
                "float texIdx = 0;"
                "float2 unormTexCoords = float2(%s.x, %s.y);"
            , inTexCoordsName, inTexCoordsName);
        } else {
#ifdef SK_ENABLE_SMALL_PAGE
            args.fVertBuilder->codeAppendf(R"code(
               float2 coord = float2(%s.x, %s.y);
               float diff0 = floor(coord.x * exp2(-14));
               float diff1 = floor(coord.y * exp2(-14));
               float texIdx = 4 * diff0 + diff1;
               float2 unormTexCoords = float2(coord.x - diff0, coord.y - diff1);
           )code", inTexCoordsName, inTexCoordsName);
#else
            args.fVertBuilder->codeAppendf(
                "float2 coord = float2(%s.x, %s.y);"
                "float texIdx = floor(coord.x * exp2(-13));"
                "float2 unormTexCoords = float2(coord.x - texIdx * exp2(13), coord.y);"
            , inTexCoordsName, inTexCoordsName);
#endif
        }
    }

    // Multiply by 1/atlasDimensions to get normalized texture coordinates
    uv->reset(SkSLType::kFloat2);
    args.fVaryingHandler->addVarying("TextureCoords", uv);
    args.fVertBuilder->codeAppendf(
            "%s = unormTexCoords * %s;", uv->vsOut(), atlasDimensionsInvName);

    // On ANGLE there is a significant cost to using an int varying. We don't know of any case where
    // it is worse to use a float so for now we always do.
    texIdx->reset(SkSLType::kFloat);
    // If we computed the local var "texIdx" as an int we will need to cast it to float
    const char* cast = args.fShaderCaps->fIntegerSupport ? "float" : "";
    args.fVaryingHandler->addVarying("TexIndex", texIdx, Interpolation::kCanBeFlat);
    args.fVertBuilder->codeAppendf("%s = %s(texIdx);", texIdx->vsOut(), cast);

    if (st) {
        st->reset(SkSLType::kFloat2);
        args.fVaryingHandler->addVarying("IntTextureCoords", st);
        args.fVertBuilder->codeAppendf("%s = unormTexCoords;", st->vsOut());
    }
}

static inline void append_multitexture_lookup(GrGeometryProcessor::ProgramImpl::EmitArgs& args,
                                              int numTextureSamplers,
                                              const GrGLSLVarying& texIdx,
                                              const char* coordName,
                                              const char* colorName) {
    SkASSERT(numTextureSamplers > 0);
    // This shouldn't happen, but will avoid a crash if it does
    if (numTextureSamplers <= 0) {
        args.fFragBuilder->codeAppendf("%s = float4(1);", colorName);
        return;
    }

    // conditionally load from the indexed texture sampler
    for (int i = 0; i < numTextureSamplers-1; ++i) {
        args.fFragBuilder->codeAppendf("if (%s == %d) { %s = ", texIdx.fsIn(), i, colorName);
        args.fFragBuilder->appendTextureLookup(args.fTexSamplers[i],
                                               coordName);
        args.fFragBuilder->codeAppend("; } else ");
    }
    args.fFragBuilder->codeAppendf("{ %s = ", colorName);
    args.fFragBuilder->appendTextureLookup(args.fTexSamplers[numTextureSamplers - 1],
                                           coordName);
    args.fFragBuilder->codeAppend("; }");
}

// Special lookup function for sdf lcd -- avoids duplicating conditional logic three times
static inline void append_multitexture_lookup_lcd(GrGeometryProcessor::ProgramImpl::EmitArgs& args,
                                                  int numTextureSamplers,
                                                  const GrGLSLVarying& texIdx,
                                                  const char* coordName,
                                                  const char* offsetName,
                                                  const char* distanceName) {
    SkASSERT(numTextureSamplers > 0);
    // This shouldn't happen, but will avoid a crash if it does
    if (numTextureSamplers <= 0) {
        args.fFragBuilder->codeAppendf("%s = half3(1);", distanceName);
        return;
    }

    // conditionally load from the indexed texture sampler
    for (int i = 0; i < numTextureSamplers; ++i) {
        args.fFragBuilder->codeAppendf("if (%s == %d) {", texIdx.fsIn(), i);

        // green is distance to uv center
        args.fFragBuilder->codeAppendf("%s.y = ", distanceName);
        args.fFragBuilder->appendTextureLookup(args.fTexSamplers[i], coordName);
        args.fFragBuilder->codeAppend(".r;");

        // red is distance to left offset
        args.fFragBuilder->codeAppendf("half2 uv_adjusted = half2(%s) - %s;",
                                       coordName, offsetName);
        args.fFragBuilder->codeAppendf("%s.x = ", distanceName);
        args.fFragBuilder->appendTextureLookup(args.fTexSamplers[i], "uv_adjusted");
        args.fFragBuilder->codeAppend(".r;");

        // blue is distance to right offset
        args.fFragBuilder->codeAppendf("uv_adjusted = half2(%s) + %s;", coordName, offsetName);
        args.fFragBuilder->codeAppendf("%s.z = ", distanceName);
        args.fFragBuilder->appendTextureLookup(args.fTexSamplers[i], "uv_adjusted");
        args.fFragBuilder->codeAppend(".r;");

        if (i < numTextureSamplers-1) {
            args.fFragBuilder->codeAppend("} else ");
        } else {
            args.fFragBuilder->codeAppend("}");
        }
    }
}

#endif
