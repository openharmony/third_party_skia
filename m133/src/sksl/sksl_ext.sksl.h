/*
 * Copyright (c) 2025 Huawei Device Co., Ltd.
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

#include <unordered_map>
#include <string>

static std::unordered_map<std::string, std::string> extModuleData = {
    {"sksl_frag.sksl", R"(
// defines built-in interfaces supported by SkSL fragment shaders

// See "enum SpvBuiltIn_" in ./spirv.h
layout(builtin=15) in float4 sk_FragCoord;
layout(builtin=17) in bool sk_Clockwise;  // Similar to gl_FrontFacing, but defined in device space.
layout(builtin=20) in uint sk_SampleMaskIn;
layout(builtin=10020) out uint sk_SampleMask;

layout(location=0,index=0,builtin=10001) out half4 sk_FragColor;
layout(builtin=10008) in half4 sk_LastFragColor;
layout(location=0,index=1,builtin=10012) out half4 sk_SecondaryFragColor;
)"},
    {"sksl_vert.sksl", R"(
// defines built-in interfaces supported by SkSL vertex shaders

out sk_PerVertex {
    layout(builtin=0) float4 sk_Position;
    layout(builtin=1) float sk_PointSize;
};

layout(builtin=42) in int sk_VertexID;
layout(builtin=43) in int sk_InstanceID;
)"},
    {"sksl_gpu.sksl", R"(
// Not exposed in shared module

$pure $genIType mix($genIType x, $genIType y, $genBType a);
$pure $genBType mix($genBType x, $genBType y, $genBType a);
$pure $genType fma($genType a, $genType b, $genType c);
$pure $genHType fma($genHType a, $genHType b, $genHType c);
      $genType frexp($genType x, out $genIType exp);
      $genHType frexp($genHType x, out $genIType exp);
$pure $genType ldexp($genType x, in $genIType exp);
$pure $genHType ldexp($genHType x, in $genIType exp);

$pure uint packSnorm2x16(float2 v);
$pure uint packUnorm4x8(float4 v);
$pure uint packSnorm4x8(float4 v);
$pure float2 unpackSnorm2x16(uint p);
$pure float4 unpackUnorm4x8(uint p);
$pure float4 unpackSnorm4x8(uint p);
$pure uint packHalf2x16(float2 v);
$pure float2 unpackHalf2x16(uint v);

$pure $genIType bitCount($genIType value);
$pure $genIType bitCount($genUType value);
$pure $genIType findLSB($genIType value);
$pure $genIType findLSB($genUType value);
$pure $genIType findMSB($genIType value);
$pure $genIType findMSB($genUType value);

$pure half4 sample(sampler2D s, float2 P);
$pure half4 sample(sampler2D s, float3 P);
$pure half4 sample(sampler2D s, float3 P, float bias);

$pure half4 sample(samplerExternalOES s, float2 P);
$pure half4 sample(samplerExternalOES s, float2 P, float bias);

$pure half4 sample(sampler2DRect s, float2 P);
$pure half4 sample(sampler2DRect s, float3 P);

$pure half4 sampleLod(sampler2D s, float2 P, float lod);
$pure half4 sampleLod(sampler2D s, float3 P, float lod);

$pure half4 sampleGrad(sampler2D s, float2, float2 dPdx, float2 dPdy);

// Currently we do not support the generic types of loading subpassInput so we have some explicit
// versions that we currently use
$pure half4 subpassLoad(subpassInput subpass);
$pure half4 subpassLoad(subpassInputMS subpass, int sample);

/** Atomically loads the value from `a` and returns it. */
$pure uint atomicLoad(atomicUint a);

/** Atomically stores the value of `value` to `a` */
void atomicStore(atomicUint a, uint value);

/**
 * Performs an atomic addition of `value` to the contents of `a` and returns the original contents
 * of `a` from before the addition occurred.
 */
uint atomicAdd(atomicUint a, uint value);

// Definitions of functions implementing all of the SkBlendMode blends.

$pure half4 blend_clear(half4 src, half4 dst) { return half4(0); }

$pure half4 blend_src(half4 src, half4 dst) { return src; }

$pure half4 blend_dst(half4 src, half4 dst) { return dst; }

$pure half4 blend_src_over(half4 src, half4 dst) { return src + (1 - src.a)*dst; }

$pure half4 blend_dst_over(half4 src, half4 dst) { return (1 - dst.a)*src + dst; }

$pure half4 blend_src_in(half4 src, half4 dst) { return src*dst.a; }

$pure half4 blend_dst_in(half4 src, half4 dst) { return dst*src.a; }

$pure half4 blend_src_out(half4 src, half4 dst) { return (1 - dst.a)*src; }

$pure half4 blend_dst_out(half4 src, half4 dst) { return (1 - src.a)*dst; }

$pure half4 blend_src_atop(half4 src, half4 dst) { return dst.a*src + (1 - src.a)*dst; }

$pure half4 blend_dst_atop(half4 src, half4 dst)  { return  (1 - dst.a) * src + src.a*dst; }

$pure half4 blend_xor(half4 src, half4 dst) { return (1 - dst.a)*src + (1 - src.a)*dst; }

// This multi-purpose Porter-Duff blend function can perform any of the twelve blends above,
// when passed one of the following values for BlendOp:
// - Clear:          0*src +        0*dst = (0 +  0*dstA)*src + (0 +  0*srcA)*dst = (0,  0,  0,  0)
// - Src:            1*src +        0*dst = (1 +  0*dstA)*src + (0 +  0*srcA)*dst = (1,  0,  0,  0)
// - Dst:            0*src +        1*dst = (0 +  0*dstA)*src + (1 +  0*srcA)*dst = (0,  1,  0,  0)
// - SrcOver:        1*src + (1-srcA)*dst = (1 +  0*dstA)*src + (1 + -1*srcA)*dst = (1,  1,  0, -1)
// - DstOver: (1-dstA)*src +        1*dst = (1 + -1*dstA)*src + (1 +  0*srcA)*dst = (1,  1, -1,  0)
// - SrcIn:       dstA*src +        0*dst = (0 +  1*dstA)*src + (0 +  0*srcA)*dst = (0,  0,  1,  0)
// - DstIn:          0*src +     srcA*dst = (0 +  0*dstA)*src + (0 +  1*srcA)*dst = (0,  0,  0,  1)
// - SrcOut:  (1-dstA)*src +        0*dst = (1 + -1*dstA)*src + (0 +  0*srcA)*dst = (1,  0, -1,  0)
// - DstOut:         0*src + (1-srcA)*dst = (0 +  0*dstA)*src + (1 + -1*srcA)*dst = (0,  1,  0, -1)
// - SrcATop:     dstA*src + (1-srcA)*dst = (0 +  1*dstA)*src + (1 + -1*srcA)*dst = (0,  1,  1, -1)
// - DstATop: (1-dstA)*src +     srcA*dst = (1 + -1*dstA)*src + (0 +  1*srcA)*dst = (1,  0, -1,  1)
// - Xor:     (1-dstA)*src + (1-srcA)*dst = (1 + -1*dstA)*src + (1 + -1*srcA)*dst = (1,  1, -1, -1)
$pure half4 blend_porter_duff(half4 blendOp, half4 src, half4 dst) {
    // The supported blend modes all have coefficients that are of the form (C + S*alpha), where
    // alpha is the other color's alpha channel. C can be 0 or 1, S can be -1, 0, or 1.
    half2 coeff = blendOp.xy + blendOp.zw * half2(dst.a, src.a);
    return src * coeff.x + dst * coeff.y;
}

$pure half4 blend_plus(half4 src, half4 dst) { return min(src + dst, 1); }

$pure half4 blend_modulate(half4 src, half4 dst) { return src*dst; }

$pure half4 blend_screen(half4 src, half4 dst) { return src + (1 - src)*dst; }

$pure half $blend_overlay_component(half2 s, half2 d) {
    return (2*d.x <= d.y) ? 2*s.x*d.x
                          : s.y*d.y - 2*(d.y - d.x)*(s.y - s.x);
}

$pure half4 blend_overlay(half4 src, half4 dst) {
    half4 result = half4($blend_overlay_component(src.ra, dst.ra),
                         $blend_overlay_component(src.ga, dst.ga),
                         $blend_overlay_component(src.ba, dst.ba),
                         src.a + (1 - src.a)*dst.a);
    result.rgb += dst.rgb*(1 - src.a) + src.rgb*(1 - dst.a);
    return result;
}

$pure half4 blend_overlay(half flip, half4 a, half4 b) {
    return blend_overlay(bool(flip) ? b : a, bool(flip) ? a : b);
}

$pure half4 blend_lighten(half4 src, half4 dst) {
    half4 result = blend_src_over(src, dst);
    result.rgb = max(result.rgb, (1 - dst.a)*src.rgb + dst.rgb);
    return result;
}

$pure half4 blend_darken(half mode /* darken: 1, lighten: -1 */, half4 src, half4 dst) {
    half4 a = blend_src_over(src, dst);
    half3 b = (1 - dst.a) * src.rgb + dst.rgb;  // DstOver.rgb
    a.rgb = mode * min(a.rgb * mode, b.rgb * mode);
    return a;
}

$pure half4 blend_darken(half4 src, half4 dst) {
   return blend_darken(1, src, dst);
}

// A useful constant to check against when dividing a half-precision denominator.
// Denormal half floats (values less than this) will compare not-equal to 0 but can easily cause the
// division to overflow to infinity. Even regular values can overflow given the low maximum value.
// For instance, any value x > ~3.998 will overflow when divided by $kMinNormalHalf. This is a
// reasonable value even for wide gamut colors being input to these blend functions, but the
// most correct denominator check is to treat anything with `denom < x/F16_MAX` as division by 0.
const half $kMinNormalHalf = 1.0 / (1 << 14);

const half $kGuardedDivideEpsilon = sk_Caps.mustGuardDivisionEvenAfterExplicitZeroCheck
                                        ? 0.00000001
                                        : 0.0;

$pure inline half $guarded_divide(half n, half d) {
    return n / (d + $kGuardedDivideEpsilon);
}

$pure inline half3 $guarded_divide(half3 n, half d) {
    return n / (d + $kGuardedDivideEpsilon);
}

$pure half $color_dodge_component(half2 s, half2 d) {
    // The following is a single flow of control implementation of:
    //     if (d.x == 0) {
    //         return s.x*(1 - d.y);
    //     } else {
    //         half delta = s.y - s.x;
    //         if (delta == 0) {
    //             return s.y*d.y + s.x*(1 - d.y) + d.x*(1 - s.y);
    //         } else {
    //             delta = min(d.y, $guarded_divide(d.x*s.y, delta));
    //             return delta*s.y + s.x*(1 - d.y) + d.x*(1 - s.y);
    //         }
    //     }
    //
    // When d.x == 0, then dxScale forces delta to 0 and simplifying the return value to s.x*(1-d.y)
    // When s.y-s.x == 0, the mix selects d.y and min(d.y, d.y) leaves delta = d.y
    // Otherwise the mix selects the delta expression in the final else branch.
    half dxScale = d.x == 0 ? 0 : 1;
    half delta = dxScale * min(d.y, abs(s.y-s.x) >= $kMinNormalHalf
                                            ? $guarded_divide(d.x*s.y, s.y-s.x)
                                            : d.y);
    return delta*s.y + s.x*(1 - d.y) + d.x*(1 - s.y);
}

$pure half4 blend_color_dodge(half4 src, half4 dst) {
    return half4($color_dodge_component(src.ra, dst.ra),
                 $color_dodge_component(src.ga, dst.ga),
                 $color_dodge_component(src.ba, dst.ba),
                 src.a + (1 - src.a)*dst.a);
}

$pure half $color_burn_component(half2 s, half2 d) {
    half dyTerm = d.y == d.x ? d.y : 0;
    half delta = abs(s.x) >= $kMinNormalHalf
                        ? d.y - min(d.y, $guarded_divide((d.y - d.x)*s.y, s.x))
                        : dyTerm;
    return delta*s.y + s.x*(1 - d.y) + d.x*(1 - s.y);
}

$pure half4 blend_color_burn(half4 src, half4 dst) {
    return half4($color_burn_component(src.ra, dst.ra),
                 $color_burn_component(src.ga, dst.ga),
                 $color_burn_component(src.ba, dst.ba),
                 src.a + (1 - src.a)*dst.a);
}

$pure half4 blend_hard_light(half4 src, half4 dst) {
    return blend_overlay(dst, src);
}

$pure half $soft_light_component(half2 s, half2 d) {
    if (2*s.x <= s.y) {
        return $guarded_divide(d.x*d.x*(s.y - 2*s.x), d.y) + (1 - d.y)*s.x + d.x*(-s.y + 2*s.x + 1);
    } else if (4.0 * d.x <= d.y) {
        half DSqd = d.x*d.x;
        half DCub = DSqd*d.x;
        half DaSqd = d.y*d.y;
        half DaCub = DaSqd*d.y;
        return $guarded_divide(DaSqd*(s.x - d.x*(3*s.y - 6*s.x - 1)) + 12*d.y*DSqd*(s.y - 2*s.x)
                               - 16*DCub * (s.y - 2*s.x) - DaCub*s.x, DaSqd);
    } else {
        return d.x*(s.y - 2*s.x + 1) + s.x - sqrt(d.y*d.x)*(s.y - 2*s.x) - d.y*s.x;
    }
}

$pure half4 blend_soft_light(half4 src, half4 dst) {
    return (dst.a == 0) ? src : half4($soft_light_component(src.ra, dst.ra),
                                      $soft_light_component(src.ga, dst.ga),
                                      $soft_light_component(src.ba, dst.ba),
                                      src.a + (1 - src.a)*dst.a);
}

$pure half4 blend_difference(half4 src, half4 dst) {
    return half4(src.rgb + dst.rgb - 2*min(src.rgb*dst.a, dst.rgb*src.a),
                 src.a + (1 - src.a)*dst.a);
}

$pure half4 blend_exclusion(half4 src, half4 dst) {
    return half4(dst.rgb + src.rgb - 2*dst.rgb*src.rgb, src.a + (1 - src.a)*dst.a);
}

$pure half4 blend_multiply(half4 src, half4 dst) {
    return half4((1 - src.a)*dst.rgb + (1 - dst.a)*src.rgb + src.rgb*dst.rgb,
                 src.a + (1 - src.a)*dst.a);
}

$pure half $blend_color_luminance(half3 color) { return dot(half3(0.3, 0.59, 0.11), color); }

$pure half3 $blend_set_color_luminance(half3 hueSatColor, half alpha, half3 lumColor) {
    half lum = $blend_color_luminance(lumColor);
    half3 result = lum - $blend_color_luminance(hueSatColor) + hueSatColor;
    half minComp = min(min(result.r, result.g), result.b);
    half maxComp = max(max(result.r, result.g), result.b);
    if (minComp < 0 && lum != minComp) {
        result = lum + (result - lum) * $guarded_divide(lum, (lum - minComp) + $kMinNormalHalf);
    }
    if (maxComp > alpha && maxComp != lum) {
        result = lum +
                 $guarded_divide((result - lum) * (alpha - lum), (maxComp - lum) + $kMinNormalHalf);
    }
    return result;
}

$pure half $blend_color_saturation(half3 color) {
    return max(max(color.r, color.g), color.b) - min(min(color.r, color.g), color.b);
}

$pure half3 $blend_set_color_saturation(half3 color, half3 satColor) {
    half mn = min(min(color.r, color.g), color.b);
    half mx = max(max(color.r, color.g), color.b);

    return (mx > mn) ? ((color - mn) * $blend_color_saturation(satColor)) / (mx - mn)
                     : half3(0);
}

$pure half4 blend_hslc(half2 flipSat, half4 src, half4 dst) {
    half alpha = dst.a * src.a;
    half3 sda = src.rgb * dst.a;
    half3 dsa = dst.rgb * src.a;
    half3 l = bool(flipSat.x) ? dsa : sda;
    half3 r = bool(flipSat.x) ? sda : dsa;
    if (bool(flipSat.y)) {
        l = $blend_set_color_saturation(l, r);
        r = dsa;
    }
    return half4($blend_set_color_luminance(l, alpha, r) + dst.rgb - dsa + src.rgb - sda,
                 src.a + dst.a - alpha);
}

$pure half4 blend_hue(half4 src, half4 dst) {
    return blend_hslc(half2(0, 1), src, dst);
}

$pure half4 blend_saturation(half4 src, half4 dst) {
    return blend_hslc(half2(1), src, dst);
}

$pure half4 blend_color(half4 src, half4 dst)  {
    return blend_hslc(half2(0), src, dst);
}

$pure half4 blend_luminosity(half4 src, half4 dst) {
    return blend_hslc(half2(1, 0), src, dst);
}

$pure float2 proj(float3 p) { return p.xy / p.z; }

// Implement cross() as a determinant to communicate our intent more clearly to the compiler.
// NOTE: Due to precision issues, it might be the case that cross(a, a) != 0.
$pure float cross_length_2d(float2 a, float2 b) {
    return determinant(float2x2(a, b));
}

$pure half cross_length_2d(half2 a, half2 b) {
    return determinant(half2x2(a, b));
}

$pure float2 perp(float2 v) {
    return float2(-v.y, v.x);
}

$pure half2 perp(half2 v) {
    return half2(-v.y, v.x);
}

// Returns a bias given a scale factor, such that 'scale * (dist + bias)' converts the distance to
// a per-pixel coverage value, automatically widening the visible coverage ramp for subpixel
// dimensions. The 'scale' must already be equal to the narrowest dimension of the shape and clamped
// to [0, 1.0].
$pure float coverage_bias(float scale) {
    return 1.0 - 0.5 * scale;
}

// add the support of textureSize
int2 textureSize(sampler2D x, int y);

// add the support of textureGather
half4 sampleGather(sampler2D s, float2 p);
half4 sampleGather(sampler2D s, float2 p, int comp);

// add the support of nonuniformEXT
uint nonuniformEXT(uint x);
)"},
    {"sksl_public.sksl", R"(
// SkSL intrinsics that are not part of GLSL

// Color space transformation, between the working (destination) space and fixed (known) spaces:
$pure half3 toLinearSrgb(half3 color);
$pure half3 fromLinearSrgb(half3 color);

// SkSL intrinsics that reflect Skia's C++ object model:
      half4 $eval(float2 coords, shader s);
      half4 $eval(half4 color, colorFilter f);
      half4 $eval(half4 src, half4 dst, blender b);
)"},
    {"sksl_rt_shader.sksl", R"(
layout(builtin=15) float4 sk_FragCoord;

//--- Luma ------------------------------------------------------------------------

half4 sk_luma(half3 color) {
    return saturate(dot(half3(0.2126, 0.7152, 0.0722), color)).000r;
}

//--- Decal ------------------------------------------------------------------------

half4 sk_decal(shader image, float2 coord, float4 decalBounds) {
    half4 d = half4(decalBounds - coord.xyxy) * half4(-1, -1, 1, 1);
    d = saturate(d + 0.5);
    return (d.x * d.y * d.z * d.w) * image.eval(coord);
}

//--- Displacement -----------------------------------------------------------------

half4 sk_displacement(shader displMap,
                      shader colorMap,
                      float2 coord,
                      half2 scale,
                      half4 xSelect,  // Only one of RGBA will be 1, the rest are 0
                      half4 ySelect) {
    half4 displColor = unpremul(displMap.eval(coord));
    half2 displ = half2(dot(displColor, xSelect), dot(displColor, ySelect));
    displ = scale * (displ - 0.5);
    return colorMap.eval(coord + displ);
}

//--- Magnifier --------------------------------------------------------------------

half4 sk_magnifier(shader src, float2 coord, float4 lensBounds, float4 zoomXform,
                   float2 invInset) {
    float2 zoomCoord = zoomXform.xy + zoomXform.zw*coord;
    // edgeInset is the smallest distance to the lens bounds edges, in units of "insets".
    float2 edgeInset = min(coord - lensBounds.xy, lensBounds.zw - coord) * invInset;

    // The equations for 'weight' ensure that it is 0 along the outside of
    // lensBounds so it seams with any un-zoomed, un-filtered content. The zoomed
    // content fills a rounded rectangle that is 1 "inset" in from lensBounds with
    // circular corners with radii equal to the inset distance. Outside of this
    // region, there is a non-linear weighting to compress the un-zoomed content
    // to the zoomed content. The critical zone about each corner is limited
    // to 2x"inset" square.
    float weight = all(lessThan(edgeInset, float2(2.0)))
        // Circular distortion weighted by distance to inset corner
        ? (2.0 - length(2.0 - edgeInset))
        // Linear zoom, or single-axis compression outside of the inset
        // area (if delta < 1)
        : min(edgeInset.x, edgeInset.y);

    // Saturate before squaring so that negative weights are clamped to 0
    // before squaring
    weight = saturate(weight);
    return src.eval(mix(coord, zoomCoord, weight*weight));
}

//--- High Contrast ----------------------------------------------------------------

$pure half3 $high_contrast_rgb_to_hsl(half3 c) {
    half mx = max(max(c.r,c.g),c.b),
         mn = min(min(c.r,c.g),c.b),
          d = mx-mn,
       invd = 1.0 / d,
     g_lt_b = c.g < c.b ? 6.0 : 0.0;

    // We'd prefer to write these tests like `mx == c.r`, but on some GPUs, max(x,y) is
    // not always equal to either x or y. So we use long form, c.r >= c.g && c.r >= c.b.
    half h = (1/6.0) * (mx == mn                 ? 0.0 :
        /*mx==c.r*/     c.r >= c.g && c.r >= c.b ? invd * (c.g - c.b) + g_lt_b :
        /*mx==c.g*/     c.g >= c.b               ? invd * (c.b - c.r) + 2.0
        /*mx==c.b*/                              : invd * (c.r - c.g) + 4.0);
    half sum = mx+mn,
           l = sum * 0.5,
           s = mx == mn ? 0.0
                        : d / (l > 0.5 ? 2.0 - sum : sum);
    return half3(h,s,l);
}

half3 sk_high_contrast(half3 color, half grayscale, half invertStyle, half contrast) {
    if (grayscale == 1) {
        color = dot(half3(0.2126, 0.7152, 0.0722), color).rrr;
    }
    if (invertStyle == 1) {  // brightness
        color = 1.0 - color;
    } else if (invertStyle == 2) {  // lightness
        color = $high_contrast_rgb_to_hsl(color);
        color.b = 1 - color.b;
        color = $hsl_to_rgb(color);
    }
    return saturate(mix(half3(0.5), color, contrast));
}

//--- Normal -----------------------------------------------------------------------

$pure half3 $normal_filter(half3 alphaC0, half3 alphaC1, half3 alphaC2, half negSurfaceDepth) {
    // The right column (or bottom row) terms of the Sobel filter. The left/top is just the
    // negative, and the middle row/column is all 0s so those instructions are skipped.
    const half3 kSobel = 0.25 * half3(1,2,1);
    half3 alphaR0 = half3(alphaC0.x, alphaC1.x, alphaC2.x);
    half3 alphaR2 = half3(alphaC0.z, alphaC1.z, alphaC2.z);
    half nx = dot(kSobel, alphaC2) - dot(kSobel, alphaC0);
    half ny = dot(kSobel, alphaR2) - dot(kSobel, alphaR0);
    return normalize(half3(negSurfaceDepth * half2(nx, ny), 1));
}

half4 sk_normal(shader alphaMap, float2 coord, float4 edgeBounds, half negSurfaceDepth) {
   half3 alphaC0 = half3(
        alphaMap.eval(clamp(coord + float2(-1,-1), edgeBounds.LT, edgeBounds.RB)).a,
        alphaMap.eval(clamp(coord + float2(-1, 0), edgeBounds.LT, edgeBounds.RB)).a,
        alphaMap.eval(clamp(coord + float2(-1, 1), edgeBounds.LT, edgeBounds.RB)).a);
   half3 alphaC1 = half3(
        alphaMap.eval(clamp(coord + float2( 0,-1), edgeBounds.LT, edgeBounds.RB)).a,
        alphaMap.eval(clamp(coord + float2( 0, 0), edgeBounds.LT, edgeBounds.RB)).a,
        alphaMap.eval(clamp(coord + float2( 0, 1), edgeBounds.LT, edgeBounds.RB)).a);
   half3 alphaC2 = half3(
        alphaMap.eval(clamp(coord + float2( 1,-1), edgeBounds.LT, edgeBounds.RB)).a,
        alphaMap.eval(clamp(coord + float2( 1, 0), edgeBounds.LT, edgeBounds.RB)).a,
        alphaMap.eval(clamp(coord + float2( 1, 1), edgeBounds.LT, edgeBounds.RB)).a);

   half mainAlpha = alphaC1.y; // offset = (0,0)
   return half4($normal_filter(alphaC0, alphaC1, alphaC2, negSurfaceDepth), mainAlpha);
}

//--- Lighting ---------------------------------------------------------------------

$pure half3 $surface_to_light(half lightType, half3 lightPos, half3 lightDir, half3 coord) {
    // Spot (> 0) and point (== 0) have the same equation
    return lightType >= 0 ? normalize(lightPos - coord)
                          : lightDir;
}

$pure half $spotlight_scale(half3 lightDir, half3 surfaceToLight, half cosCutoffAngle,
                            half spotFalloff) {
    const half kConeAAThreshold = 0.016;
    const half kConeScale = 1.0 / kConeAAThreshold;

    half cosAngle = -dot(surfaceToLight, lightDir);
    if (cosAngle < cosCutoffAngle) {
        return 0.0;
    } else {
        half scale = pow(cosAngle, spotFalloff);
        return (cosAngle < cosCutoffAngle + kConeAAThreshold)
                    ? scale * (cosAngle - cosCutoffAngle) * kConeScale
                    : scale;
    }
}

$pure half4 $compute_lighting(half3 color, half shininess, half materialType, half lightType,
                              half3 normal, half3 lightDir, half3 surfaceToLight,
                              half cosCutoffAngle, half spotFalloff) {
    // Point and distant light color contributions are constant, but
    // spotlights fade based on the angle away from its direction.
    if (lightType > 0) {
        color *= $spotlight_scale(lightDir, surfaceToLight, cosCutoffAngle, spotFalloff);
    }

    // Diffuse and specular reflections scale the light's color differently
    if (materialType == 0) {
        half coeff = dot(normal, surfaceToLight);
        color = saturate(coeff * color);
        return half4(color, 1.0);
    } else {
        half3 halfDir = normalize(surfaceToLight + half3(0, 0, 1));
        half coeff = pow(dot(normal, halfDir), shininess);
        color = saturate(coeff * color);
        return half4(color, max(max(color.r, color.g), color.b));
    }
}

half4 sk_lighting(shader normalMap, float2 coord,
                  half depth, half shininess, half materialType, half lightType,
                  half3 lightPos, half spotFalloff,
                  half3 lightDir, half cosCutoffAngle,
                  half3 lightColor) {
    half4 normalAndA = normalMap.eval(coord);
    half3 surfaceToLight = $surface_to_light(lightType, lightPos, lightDir,
                                             half3(coord, depth * normalAndA.a));
    return $compute_lighting(lightColor, shininess, materialType, lightType, normalAndA.xyz,
                             lightDir, surfaceToLight, cosCutoffAngle, spotFalloff);
}

//--- Arithmetic Blend -------------------------------------------------------------

half4 sk_arithmetic_blend(half4 src, half4 dst, half4 k, half pmClamp) {
    half4 color = saturate(k.x * src * dst + k.y * src + k.z * dst + k.w);
    color.rgb = min(color.rgb, max(color.a, pmClamp));
    return color;
}

//--- Sparse Morphology ------------------------------------------------------------

half4 sk_sparse_morphology(shader child, float2 coord, half2 offset, half flip) {
    half4 aggregate = max(flip * child.eval(coord + offset),
                          flip * child.eval(coord - offset));
    return flip * aggregate;
}

//--- Linear Morphology ------------------------------------------------------------

half4 sk_linear_morphology(shader child, float2 coord, half2 offset, half flip, int radius) {

    // KEEP IN SYNC WITH CONSTANT IN `SkMorphologyImageFilter.cpp`
    const int kMaxLinearRadius = 14;

    half4 aggregate = flip * child.eval(coord); // case 0 only needs a single sample
    half2 delta = offset;
    for (int i = 1; i <= kMaxLinearRadius; ++i) {
        if (i > radius) break;
        aggregate = max(aggregate, max(flip * child.eval(coord + delta),
                                       flip * child.eval(coord - delta)));
        delta += offset;
    }
    return flip * aggregate;
}

//--- Overdraw ---------------------------------------------------------------------

half4 sk_overdraw(half  alpha,  half4 color0, half4 color1, half4 color2,
                  half4 color3, half4 color4, half4 color5) {
   return alpha < (0.5 / 255.) ? color0
        : alpha < (1.5 / 255.) ? color1
        : alpha < (2.5 / 255.) ? color2
        : alpha < (3.5 / 255.) ? color3
        : alpha < (4.5 / 255.) ? color4
                               : color5;
}
)"},
    {"sksl_shared.sksl", R"(
// Intrinsics that are available to public SkSL (SkRuntimeEffect)

// See "The OpenGL ES Shading Language, Section 8"

// 8.1 : Angle and Trigonometry Functions
$pure $genType  radians($genType  degrees);
$pure $genHType radians($genHType degrees);
$pure $genType  degrees($genType  radians);
$pure $genHType degrees($genHType radians);

$pure $genType  sin($genType  angle);
$pure $genHType sin($genHType angle);
$pure $genType  cos($genType  angle);
$pure $genHType cos($genHType angle);
$pure $genType  tan($genType  angle);
$pure $genHType tan($genHType angle);

$pure $genType  asin($genType  x);
$pure $genHType asin($genHType x);
$pure $genType  acos($genType  x);
$pure $genHType acos($genHType x);
$pure $genType  atan($genType  y, $genType  x);
$pure $genHType atan($genHType y, $genHType x);
$pure $genType  atan($genType  y_over_x);
$pure $genHType atan($genHType y_over_x);

// 8.1 : Angle and Trigonometry Functions (GLSL ES 3.0)
$pure $es3 $genType  sinh($genType x);
$pure $es3 $genHType sinh($genHType x);
$pure $es3 $genType  cosh($genType x);
$pure $es3 $genHType cosh($genHType x);
$pure $es3 $genType  tanh($genType x);
$pure $es3 $genHType tanh($genHType x);
$pure $es3 $genType  asinh($genType x);
$pure $es3 $genHType asinh($genHType x);
$pure $es3 $genType  acosh($genType x);
$pure $es3 $genHType acosh($genHType x);
$pure $es3 $genType  atanh($genType x);
$pure $es3 $genHType atanh($genHType x);

// 8.2 : Exponential Functions
$pure $genType  pow($genType  x, $genType  y);
$pure $genHType pow($genHType x, $genHType y);
$pure $genType  exp($genType  x);
$pure $genHType exp($genHType x);
$pure $genType  log($genType  x);
$pure $genHType log($genHType x);
$pure $genType  exp2($genType  x);
$pure $genHType exp2($genHType x);
$pure $genType  log2($genType  x);
$pure $genHType log2($genHType x);

$pure $genType  sqrt($genType  x);
$pure $genHType sqrt($genHType x);
$pure $genType  inversesqrt($genType  x);
$pure $genHType inversesqrt($genHType x);

// 8.3 : Common Functions
$pure $genType  abs($genType  x);
$pure $genHType abs($genHType x);
$pure $genType  sign($genType  x);
$pure $genHType sign($genHType x);
$pure $genType  floor($genType  x);
$pure $genHType floor($genHType x);
$pure $genType  ceil($genType  x);
$pure $genHType ceil($genHType x);
$pure $genType  fract($genType  x);
$pure $genHType fract($genHType x);
$pure $genType  mod($genType  x, float     y);
$pure $genType  mod($genType  x, $genType  y);
$pure $genHType mod($genHType x, half      y);
$pure $genHType mod($genHType x, $genHType y);

$pure $genType  min($genType  x, $genType  y);
$pure $genType  min($genType  x, float     y);
$pure $genHType min($genHType x, $genHType y);
$pure $genHType min($genHType x, half      y);
$pure $genType  max($genType  x, $genType  y);
$pure $genType  max($genType  x, float     y);
$pure $genHType max($genHType x, $genHType y);
$pure $genHType max($genHType x, half      y);
$pure $genType  clamp($genType  x, $genType  minVal, $genType  maxVal);
$pure $genType  clamp($genType  x, float     minVal, float     maxVal);
$pure $genHType clamp($genHType x, $genHType minVal, $genHType maxVal);
$pure $genHType clamp($genHType x, half      minVal, half      maxVal);
$pure $genType  saturate($genType  x);  // SkSL extension
$pure $genHType saturate($genHType x);  // SkSL extension
$pure $genType  mix($genType  x, $genType  y, $genType a);
$pure $genType  mix($genType  x, $genType  y, float a);
$pure $genHType mix($genHType x, $genHType y, $genHType a);
$pure $genHType mix($genHType x, $genHType y, half a);
$pure $genType  step($genType  edge, $genType x);
$pure $genType  step(float     edge, $genType x);
$pure $genHType step($genHType edge, $genHType x);
$pure $genHType step(half      edge, $genHType x);
$pure $genType  smoothstep($genType  edge0, $genType  edge1, $genType  x);
$pure $genType  smoothstep(float     edge0, float     edge1, $genType  x);
$pure $genHType smoothstep($genHType edge0, $genHType edge1, $genHType x);
$pure $genHType smoothstep(half      edge0, half      edge1, $genHType x);

// 8.3 : Common Functions (GLSL ES 3.0)
$pure $es3 $genIType abs($genIType x);
$pure $es3 $genIType sign($genIType x);
$pure $es3 $genIType floatBitsToInt ($genType  value);
$pure $es3 $genUType floatBitsToUint($genType  value);
$pure $es3 $genType  intBitsToFloat ($genIType value);
$pure $es3 $genType  uintBitsToFloat($genUType value);
$pure $es3 $genType  trunc($genType  x);
$pure $es3 $genHType trunc($genHType x);
$pure $es3 $genType  round($genType  x);
$pure $es3 $genHType round($genHType x);
$pure $es3 $genType  roundEven($genType  x);
$pure $es3 $genHType roundEven($genHType x);
$pure $es3 $genIType min($genIType x, $genIType y);
$pure $es3 $genIType min($genIType x, int y);
$pure $es3 $genUType min($genUType x, $genUType y);
$pure $es3 $genUType min($genUType x, uint y);
$pure $es3 $genIType max($genIType x, $genIType y);
$pure $es3 $genIType max($genIType x, int y);
$pure $es3 $genUType max($genUType x, $genUType y);
$pure $es3 $genUType max($genUType x, uint y);
$pure $es3 $genIType clamp($genIType x, $genIType minVal, $genIType maxVal);
$pure $es3 $genIType clamp($genIType x, int minVal, int maxVal);
$pure $es3 $genUType clamp($genUType x, $genUType minVal, $genUType maxVal);
$pure $es3 $genUType clamp($genUType x, uint minVal, uint maxVal);
$pure $es3 $genType  mix($genType  x, $genType  y, $genBType a);
$pure $es3 $genHType mix($genHType x, $genHType y, $genBType a);

// 8.3 : Common Functions (GLSL ES 3.0) -- cannot be used in constant-expressions
$pure $es3 $genBType isnan($genType  x);
$pure $es3 $genBType isnan($genHType x);
$pure $es3 $genBType isinf($genType  x);
$pure $es3 $genBType isinf($genHType x);
      $es3 $genType  modf($genType  x, out $genType  i);
      $es3 $genHType modf($genHType x, out $genHType i);

// 8.4 : Floating-Point Pack and Unpack Functions (GLSL ES 3.0)
$pure $es3 uint packUnorm2x16(float2 v);
$pure $es3 float2 unpackUnorm2x16(uint p);

// 8.5 : Geometric Functions
$pure float length($genType  x);
$pure half  length($genHType x);
$pure float distance($genType  p0, $genType  p1);
$pure half  distance($genHType p0, $genHType p1);
$pure float dot($genType  x, $genType  y);
$pure half  dot($genHType x, $genHType y);
$pure float3 cross(float3 x, float3 y);
$pure half3  cross(half3  x, half3  y);
$pure $genType  normalize($genType  x);
$pure $genHType normalize($genHType x);
$pure $genType  faceforward($genType  N, $genType  I, $genType  Nref);
$pure $genHType faceforward($genHType N, $genHType I, $genHType Nref);
$pure $genType  reflect($genType  I, $genType  N);
$pure $genHType reflect($genHType I, $genHType N);
$pure $genType  refract($genType  I, $genType  N, float eta);
$pure $genHType refract($genHType I, $genHType N, half eta);

// 8.6 : Matrix Functions
$pure $squareMat  matrixCompMult($squareMat  x, $squareMat  y);
$pure $squareHMat matrixCompMult($squareHMat x, $squareHMat y);
$pure $es3 $mat   matrixCompMult($mat x, $mat y);
$pure $es3 $hmat  matrixCompMult($hmat x, $hmat y);

// 8.6 : Matrix Functions (GLSL 1.4, poly-filled by SkSL as needed)
$pure $squareMat  inverse($squareMat  m);
$pure $squareHMat inverse($squareHMat m);

// 8.6 : Matrix Functions (GLSL ES 3.0)
$pure $es3 float       determinant($squareMat m);
$pure $es3 half        determinant($squareHMat m);
$pure $es3 $squareMat  transpose($squareMat  m);
$pure $es3 $squareHMat transpose($squareHMat m);
$pure $es3 float2x3    transpose(float3x2 m);
$pure $es3 half2x3     transpose(half3x2  m);
$pure $es3 float2x4    transpose(float4x2 m);
$pure $es3 half2x4     transpose(half4x2  m);
$pure $es3 float3x2    transpose(float2x3 m);
$pure $es3 half3x2     transpose(half2x3  m);
$pure $es3 float3x4    transpose(float4x3 m);
$pure $es3 half3x4     transpose(half4x3  m);
$pure $es3 float4x2    transpose(float2x4 m);
$pure $es3 half4x2     transpose(half2x4  m);
$pure $es3 float4x3    transpose(float3x4 m);
$pure $es3 half4x3     transpose(half3x4  m);
$pure $es3 $squareMat  outerProduct($vec   c, $vec   r);
$pure $es3 $squareHMat outerProduct($hvec  c, $hvec  r);
$pure $es3 float2x3    outerProduct(float3 c, float2 r);
$pure $es3 half2x3     outerProduct(half3  c, half2  r);
$pure $es3 float3x2    outerProduct(float2 c, float3 r);
$pure $es3 half3x2     outerProduct(half2  c, half3  r);
$pure $es3 float2x4    outerProduct(float4 c, float2 r);
$pure $es3 half2x4     outerProduct(half4  c, half2  r);
$pure $es3 float4x2    outerProduct(float2 c, float4 r);
$pure $es3 half4x2     outerProduct(half2  c, half4  r);
$pure $es3 float3x4    outerProduct(float4 c, float3 r);
$pure $es3 half3x4     outerProduct(half4  c, half3  r);
$pure $es3 float4x3    outerProduct(float3 c, float4 r);
$pure $es3 half4x3     outerProduct(half3  c, half4  r);

// 8.7 : Vector Relational Functions
$pure $bvec lessThan($vec  x, $vec  y);
$pure $bvec lessThan($hvec x, $hvec y);
$pure $bvec lessThan($ivec x, $ivec y);
$pure $bvec lessThan($svec x, $svec y);
$pure $bvec lessThanEqual($vec  x, $vec  y);
$pure $bvec lessThanEqual($hvec x, $hvec y);
$pure $bvec lessThanEqual($ivec x, $ivec y);
$pure $bvec lessThanEqual($svec x, $svec y);
$pure $bvec greaterThan($vec  x, $vec  y);
$pure $bvec greaterThan($hvec x, $hvec y);
$pure $bvec greaterThan($ivec x, $ivec y);
$pure $bvec greaterThan($svec x, $svec y);
$pure $bvec greaterThanEqual($vec  x, $vec  y);
$pure $bvec greaterThanEqual($hvec x, $hvec y);
$pure $bvec greaterThanEqual($ivec x, $ivec y);
$pure $bvec greaterThanEqual($svec x, $svec y);
$pure $bvec equal($vec  x, $vec  y);
$pure $bvec equal($hvec x, $hvec y);
$pure $bvec equal($ivec x, $ivec y);
$pure $bvec equal($svec x, $svec y);
$pure $bvec equal($bvec x, $bvec y);
$pure $bvec notEqual($vec  x, $vec  y);
$pure $bvec notEqual($hvec x, $hvec y);
$pure $bvec notEqual($ivec x, $ivec y);
$pure $bvec notEqual($svec x, $svec y);
$pure $bvec notEqual($bvec x, $bvec y);

$pure $es3 $bvec lessThan($usvec x, $usvec y);
$pure $es3 $bvec lessThan($uvec x, $uvec y);
$pure $es3 $bvec lessThanEqual($uvec x, $uvec y);
$pure $es3 $bvec lessThanEqual($usvec x, $usvec y);
$pure $es3 $bvec greaterThan($uvec x, $uvec y);
$pure $es3 $bvec greaterThan($usvec x, $usvec y);
$pure $es3 $bvec greaterThanEqual($uvec x, $uvec y);
$pure $es3 $bvec greaterThanEqual($usvec x, $usvec y);
$pure $es3 $bvec equal($uvec x, $uvec y);
$pure $es3 $bvec equal($usvec x, $usvec y);
$pure $es3 $bvec notEqual($uvec x, $uvec y);
$pure $es3 $bvec notEqual($usvec x, $usvec y);

$pure bool  any($bvec x);
$pure bool  all($bvec x);
$pure $bvec not($bvec x);

// 8.9 : Fragment Processing Functions (GLSL ES 3.0)
$pure $es3 $genType  dFdx($genType p);
$pure $es3 $genType  dFdy($genType p);
$pure $es3 $genHType dFdx($genHType p);
$pure $es3 $genHType dFdy($genHType p);
$pure $es3 $genType  fwidth($genType p);
$pure $es3 $genHType fwidth($genHType p);


// SkSL utility functions

// The max() guards against division by zero when the incoming color is transparent black
$pure half4  unpremul(half4  color) { return half4 (color.rgb / max(color.a, 0.0001), color.a); }
$pure float4 unpremul(float4 color) { return float4(color.rgb / max(color.a, 0.0001), color.a); }

// Similar, but used for polar-space CSS colors
$export $pure half4 $unpremul_polar(half4 color) {
    return half4(color.r, color.gb / max(color.a, 0.0001), color.a);
}

// Convert RGBA -> HSLA (including unpremul).
//
// Based on work by Sam Hocevar, Emil Persson, and Ian Taylor [1][2][3].  High-level ideas:
//
//   - minimize the number of branches by sorting and computing the hue phase in parallel (vec4s)
//
//   - trade the third sorting branch for a potentially faster std::min and leaving 2nd/3rd
//     channels unsorted (based on the observation that swapping both the channels and the bias sign
//     has no effect under abs)
//
//   - use epsilon offsets for denominators, to avoid explicit zero-checks
//
// An additional trick we employ is deferring premul->unpremul conversion until the very end: the
// alpha factor gets naturally simplified for H and S, and only L requires a dedicated unpremul
// division (so we trade three divs for one).
//
// [1] http://lolengine.net/blog/2013/01/13/fast-rgb-to-hsv
// [2] http://lolengine.net/blog/2013/07/27/rgb-to-hsv-in-glsl
// [3] http://www.chilliant.com/rgb2hsv.html

$export $pure half4 $rgb_to_hsl(half3 c, half a) {
    half4 p = (c.g < c.b) ? half4(c.bg, -1,  2/3.0)
                          : half4(c.gb,  0, -1/3.0);
    half4 q = (c.r < p.x) ? half4(p.x, c.r, p.yw)
                          : half4(c.r, p.x, p.yz);

    // q.x  -> max channel value
    // q.yz -> 2nd/3rd channel values (unsorted)
    // q.w  -> bias value dependent on max channel selection

    const half kEps = 0.0001;
    half pmV = q.x;
    half pmC = pmV - min(q.y, q.z);
    half pmL = pmV - pmC * 0.5;
    half   H = abs(q.w + (q.y - q.z) / (pmC * 6 + kEps));
    half   S = pmC / (a + kEps - abs(pmL * 2 - a));
    half   L = pmL / (a + kEps);

    return half4(H, S, L, a);
}

// Convert HSLA -> RGBA (including clamp and premul).
//
// Based on work by Sam Hocevar, Emil Persson, and Ian Taylor [1][2][3].
//
// [1] http://lolengine.net/blog/2013/01/13/fast-rgb-to-hsv
// [2] http://lolengine.net/blog/2013/07/27/rgb-to-hsv-in-glsl
// [3] http://www.chilliant.com/rgb2hsv.html

$export $pure half3 $hsl_to_rgb(half3 hsl) {
    half      C = (1 - abs(2 * hsl.z - 1)) * hsl.y;
    half3     p = hsl.xxx + half3(0, 2/3.0, 1/3.0);
    half3     q = saturate(abs(fract(p) * 6 - 3) - 1);

    return (q - 0.5) * C + hsl.z;
}

$export $pure half4 $hsl_to_rgb(half3 hsl, half a) {
    return saturate(half4($hsl_to_rgb(hsl) * a, a));
}

// Color conversion functions used in gradient interpolation, based on
// https://www.w3.org/TR/css-color-4/#color-conversion-code
// TODO(skia:13108): For all of these, we can eliminate any linear math at the beginning
// (by removing the corresponding linear math at the end of the CPU code).
$export $pure half3 $css_lab_to_xyz(half3 lab) {
    const half k = 24389 / 27.0;
    const half e = 216 / 24389.0;

    half3 f;
    f[1] = (lab[0] + 16) / 116;
    f[0] = (lab[1] / 500) + f[1];
    f[2] = f[1] - (lab[2] / 200);

    half3 f_cubed = pow(f, half3(3));

    half3 xyz = half3(
        f_cubed[0] > e ? f_cubed[0] : (116 * f[0] - 16) / k,
        lab[0] > k * e ? f_cubed[1] : lab[0] / k,
        f_cubed[2] > e ? f_cubed[2] : (116 * f[2] - 16) / k
    );

    const half3 D50 = half3(0.3457 / 0.3585, 1.0, (1.0 - 0.3457 - 0.3585) / 0.3585);
    return xyz * D50;
}

// Skia stores all polar colors with hue in the first component, so this "LCH -> Lab" transform
// actually takes "HCL". This is also used to do the same polar transform for OkHCL to OkLAB.
// See similar comments & logic in SkGradientShaderBase.cpp.
$pure half3 $css_hcl_to_lab(half3 hcl) {
    return half3(
        hcl[2],
        hcl[1] * cos(radians(hcl[0])),
        hcl[1] * sin(radians(hcl[0]))
    );
}

$export $pure half3 $css_hcl_to_xyz(half3 hcl) {
    return $css_lab_to_xyz($css_hcl_to_lab(hcl));
}

$export $pure half3 $css_oklab_to_linear_srgb(half3 oklab) {
    half l_ = oklab.x + 0.3963377774 * oklab.y + 0.2158037573 * oklab.z,
         m_ = oklab.x - 0.1055613458 * oklab.y - 0.0638541728 * oklab.z,
         s_ = oklab.x - 0.0894841775 * oklab.y - 1.2914855480 * oklab.z;

    half l = l_*l_*l_,
         m = m_*m_*m_,
         s = s_*s_*s_;

    return half3(
        +4.0767416621 * l - 3.3077115913 * m + 0.2309699292 * s,
        -1.2684380046 * l + 2.6097574011 * m - 0.3413193965 * s,
        -0.0041960863 * l - 0.7034186147 * m + 1.7076147010 * s
    );
}

$export $pure half3 $css_okhcl_to_linear_srgb(half3 okhcl) {
    return $css_oklab_to_linear_srgb($css_hcl_to_lab(okhcl));
}

$export $pure half3 $css_oklab_gamut_map_to_linear_srgb(half3 oklab) {
    // Constants for the normal vector of the plane formed by white, black, and
    // the specified vertex of the gamut.
    const half2 normal_R = half2(0.409702, -0.912219);
    const half2 normal_M = half2(-0.397919, -0.917421);
    const half2 normal_B = half2(-0.906800, 0.421562);
    const half2 normal_C = half2(-0.171122, 0.985250);
    const half2 normal_G = half2(0.460276, 0.887776);
    const half2 normal_Y = half2(0.947925, 0.318495);
)"
R"(
    // For the triangles formed by white (W) or black (K) with the vertices
    // of Yellow and Red (YR), Red and Magenta (RM), etc, the constants to be
    // used to compute the intersection of a line of constant hue and luminance
    // with that plane.
    const half  c0_YR = 0.091132;
    const half2 cW_YR = half2(0.070370, 0.034139);
    const half2 cK_YR = half2(0.018170, 0.378550);
    const half  c0_RM = 0.113902;
    const half2 cW_RM = half2(0.090836, 0.036251);
    const half2 cK_RM = half2(0.226781, 0.018764);
    const half  c0_MB = 0.161739;
    const half2 cW_MB = half2(-0.008202, -0.264819);
    const half2 cK_MB = half2( 0.187156, -0.284304);
    const half  c0_BC = 0.102047;
    const half2 cW_BC = half2(-0.014804, -0.162608);
    const half2 cK_BC = half2(-0.276786,  0.004193);
    const half  c0_CG = 0.092029;
    const half2 cW_CG = half2(-0.038533, -0.001650);
    const half2 cK_CG = half2(-0.232572, -0.094331);
    const half  c0_GY = 0.081709;
    const half2 cW_GY = half2(-0.034601, -0.002215);
    const half2 cK_GY = half2( 0.012185,  0.338031);

    half2 ab = oklab.yz;

    // Find the planes to intersect with and set the constants based on those
    // planes.
    half c0;
    half2 cW;
    half2 cK;
    if (dot(ab, normal_R) < 0.0) {
        if (dot(ab, normal_G) < 0.0) {
            if (dot(ab, normal_C) < 0.0) {
                c0 = c0_BC; cW = cW_BC; cK = cK_BC;
            } else {
                c0 = c0_CG; cW = cW_CG; cK = cK_CG;
            }
        } else {
            if (dot(ab, normal_Y) < 0.0) {
                c0 = c0_GY; cW = cW_GY; cK = cK_GY;
            } else {
                c0 = c0_YR; cW = cW_YR; cK = cK_YR;
            }
        }
    } else {
        if (dot(ab, normal_B) < 0.0) {
            if (dot(ab, normal_M) < 0.0) {
                c0 = c0_RM; cW = cW_RM; cK = cK_RM;
            } else {
                c0 = c0_MB; cW = cW_MB; cK = cK_MB;
            }
        } else {
            c0 = c0_BC; cW = cW_BC; cK = cK_BC;
        }
    }

    // Perform the intersection.
    half alpha = 1.0;

    // Intersect with the plane with white.
    half w_denom = dot(cW, ab);
    if (w_denom > 0.0) {
        half one_minus_L = 1.0 - oklab.r;
        half w_num = c0*one_minus_L;
        if (w_num < w_denom) {
            alpha = min(alpha, w_num / w_denom);
        }
    }

    // Intersect with the plane with black.
    half k_denom = dot(cK, ab);
    if (k_denom > 0.0) {
        half L = oklab.r;
        half k_num = c0*L;
        if (k_num < k_denom) {
            alpha = min(alpha,  k_num / k_denom);
        }
    }

    // Attenuate the ab coordinate by alpha.
    oklab.yz *= alpha;

    return $css_oklab_to_linear_srgb(oklab);
}

$export $pure half3 $css_okhcl_gamut_map_to_linear_srgb(half3 okhcl) {
    return $css_oklab_gamut_map_to_linear_srgb($css_hcl_to_lab(okhcl));
}

// TODO(skia:13108): Use our optimized version (though it has different range)
// Doing so might require fixing (re-deriving?) the math for the HWB version below
$export $pure half3 $css_hsl_to_srgb(half3 hsl) {
    hsl.x = mod(hsl.x, 360);
    if (hsl.x < 0) {
        hsl.x += 360;
    }

    hsl.yz /= 100;

    half3 k = mod(half3(0, 8, 4) + hsl.x/30, 12);
    half a = hsl.y * min(hsl.z, 1 - hsl.z);
    return hsl.z - a * clamp(min(k - 3, 9 - k), -1, 1);
}

$export $pure half3 $css_hwb_to_srgb(half3 hwb) {
    half3 rgb;
    hwb.yz /= 100;
    if (hwb.y + hwb.z >= 1) {
        // Emit grayscale
        rgb = half3(hwb.y / (hwb.y + hwb.z));
    } else {
        rgb = $css_hsl_to_srgb(half3(hwb.x, 100, 50));
        rgb *= (1 - hwb.y - hwb.z);
        rgb += hwb.y;
    }
    return rgb;
}

/*
 * The actual output color space of this function depends on the input color space
 * (it might be sRGB, linear sRGB, or linear XYZ). The actual space is what's stored
 * in the gradient/SkColor4fXformer's fIntermediateColorSpace.
 */
$export $pure half4 $interpolated_to_rgb_unpremul(half4 color, int colorSpace, int doUnpremul) {
    const int kDestination   = 0;
    const int kSRGBLinear    = 1;
    const int kLab           = 2;
    const int kOKLab         = 3;
    const int kOKLabGamutMap = 4;
    const int kLCH           = 5;
    const int kOKLCH         = 6;
    const int kOKLCHGamutMap = 7;
    const int kSRGB          = 8;
    const int kHSL           = 9;
    const int kHWB           = 10;

    if (bool(doUnpremul)) {
        switch (colorSpace) {
            case kLab:
            case kOKLab:
            case kOKLabGamutMap: color = unpremul(color); break;
            case kLCH:
            case kOKLCH:
            case kOKLCHGamutMap:
            case kHSL:
            case kHWB: color = $unpremul_polar(color); break;
        }
    }
    switch (colorSpace) {
        case kLab:           color.rgb = $css_lab_to_xyz(color.rgb); break;
        case kOKLab:         color.rgb = $css_oklab_to_linear_srgb(color.rgb); break;
        case kOKLabGamutMap: color.rgb = $css_oklab_gamut_map_to_linear_srgb(color.rgb); break;
        case kLCH:           color.rgb = $css_hcl_to_xyz(color.rgb); break;
        case kOKLCH:         color.rgb = $css_okhcl_to_linear_srgb(color.rgb); break;
        case kOKLCHGamutMap: color.rgb = $css_okhcl_gamut_map_to_linear_srgb(color.rgb); break;
        case kHSL:           color.rgb = $css_hsl_to_srgb(color.rgb); break;
        case kHWB:           color.rgb = $css_hwb_to_srgb(color.rgb); break;
    }
    return color;
}
)"}
};
