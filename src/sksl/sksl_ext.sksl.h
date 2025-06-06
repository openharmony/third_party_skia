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
// defines built-in interfaces supported by SkiaSL fragment shaders

// See "enum SpvBuiltIn_" in ./spirv.h
layout(builtin=15) in float4 sk_FragCoord;
layout(builtin=17) in bool sk_Clockwise;  // Similar to gl_FrontFacing, but defined in device space.

layout(location=0,index=0,builtin=10001) out half4 sk_FragColor;
layout(builtin=10008) half4 sk_LastFragColor;
layout(builtin=10012) out half4 sk_SecondaryFragColor;
)"},
    {"sksl_vert.sksl", R"(
// defines built-in interfaces supported by SkiaSL vertex shaders

out sk_PerVertex {
    layout(builtin=0) float4 sk_Position;
    layout(builtin=1) float sk_PointSize;
};

layout(builtin=42) in int sk_VertexID;
layout(builtin=43) in int sk_InstanceID;
)"},
    {"sksl_gpu.sksl", R"(
// defines built-in functions supported by SkSL when running on a GPU

$genType radians($genType degrees);
$genType degrees($genType radians);
$genType sin($genType angle);
$genType cos($genType angle);
$genType tan($genType angle);
$genType asin($genType x);
$genType acos($genType x);
$genType atan($genType y, $genType x);
$genType atan($genType y_over_x);
$genType sinh($genType x);
$genType cosh($genType x);
$genType tanh($genType x);
$genType asinh($genType x);
$genType acosh($genType x);
$genType atanh($genType x);
$genType pow($genType x, $genType y);
$genType exp($genType x);
$genType log($genType x);
$genType exp2($genType x);
$genType log2($genType x);
$genType sqrt($genType x);
$genHType radians($genHType degrees);
$genHType degrees($genHType radians);
$genHType sin($genHType angle);
$genHType cos($genHType angle);
$genHType tan($genHType angle);
$genHType asin($genHType x);
$genHType acos($genHType x);
$genHType atan($genHType y, $genHType x);
$genHType atan($genHType y_over_x);
$genHType sinh($genHType x);
$genHType cosh($genHType x);
$genHType tanh($genHType x);
$genHType asinh($genHType x);
$genHType acosh($genHType x);
$genHType atanh($genHType x);
$genHType pow($genHType x, $genHType y);
$genHType exp($genHType x);
$genHType log($genHType x);
$genHType exp2($genHType x);
$genHType log2($genHType x);
$genHType sqrt($genHType x);
$genType inversesqrt($genType x);
$genHType inversesqrt($genHType x);
$genType abs($genType x);
$genHType abs($genHType x);
$genIType abs($genIType x);
$genType sign($genType x);
$genHType sign($genHType x);
$genIType sign($genIType x);
$genType floor($genType x);
$genHType floor($genHType x);
$genType trunc($genType x);
$genHType trunc($genHType x);
$genType round($genType x);
$genHType round($genHType x);
$genType roundEven($genType x);
$genHType roundEven($genHType x);
$genType ceil($genType x);
$genHType ceil($genHType x);
$genType fract($genType x);
$genHType fract($genHType x);
$genType mod($genType x, float y);
$genType mod($genType x, $genType y);
$genHType mod($genHType x, half y);
$genHType mod($genHType x, $genHType y);
$genType modf($genType x, out $genType i);
$genHType modf($genHType x, out $genHType i);
$genType min($genType x, $genType y);
$genType min($genType x, float y);
$genHType min($genHType x, $genHType y);
$genHType min($genHType x, half y);
$genIType min($genIType x, $genIType y);
$genIType min($genIType x, int y);
$genType max($genType x, $genType y);
$genType max($genType x, float y);
$genHType max($genHType x, $genHType y);
$genHType max($genHType x, half y);
$genIType max($genIType x, $genIType y);
$genIType max($genIType x, int y);
$genType clamp($genType x, $genType minVal, $genType maxVal);
$genType clamp($genType x, float minVal, float maxVal);
$genHType clamp($genHType x, $genHType minVal, $genHType maxVal);
$genHType clamp($genHType x, half minVal, half maxVal);
$genIType clamp($genIType x, $genIType minVal, $genIType maxVal);
$genIType clamp($genIType x, int minVal, int maxVal);
$genUType clamp($genUType x, $genUType minVal, $genUType maxVal);
$genUType clamp($genUType x, uint minVal, uint maxVal);
$genType saturate($genType x);
$genHType saturate($genHType x);
$genType mix($genType x, $genType y, $genType a);
$genType mix($genType x, $genType y, float a);
$genHType mix($genHType x, $genHType y, $genHType a);
$genHType mix($genHType x, $genHType y, half a);
$genType mix($genType x, $genType y, $genBType a);
$genHType mix($genHType x, $genHType y, $genBType a);
$genIType mix($genIType x, $genIType y, $genBType a);
$genBType mix($genBType x, $genBType y, $genBType a);
$genType step($genType edge, $genType x);
$genType step(float edge, $genType x);
$genHType step($genHType edge, $genHType x);
$genHType step(half edge, $genHType x);
$genType smoothstep($genType edge0, $genType edge1, $genType x);
$genType smoothstep(float edge0, float edge1, $genType x);
$genHType smoothstep($genHType edge0, $genHType edge1, $genHType x);
$genHType smoothstep(half edge0, half edge1, $genHType x);
$genBType isnan($genType x);
$genBType isinf($genType x);
$genIType floatBitsToInt($genType value);
$genUType floatBitsToUint($genType value);
$genType intBitsToFloat($genIType value);
$genType uintBitsToFloat($genUType value);
$genType fma($genType a, $genType b, $genType c);
$genHType fma($genHType a, $genHType b, $genHType c);
sk_has_side_effects $genType frexp($genType x, out $genIType exp);
sk_has_side_effects $genHType frexp($genHType x, out $genIType exp);
$genType ldexp($genType x, in $genIType exp);
$genHType ldexp($genHType x, in $genIType exp);
uint packUnorm2x16(float2 v);
uint packSnorm2x16(float2 v);
uint packUnorm4x8(float4 v);
uint packSnorm4x8(float4 v);
float2 unpackUnorm2x16(uint p);
float2 unpackSnorm2x16(uint p);
float4 unpackUnorm4x8(uint p);
float4 unpackSnorm4x8(uint p);
uint packHalf2x16(float2 v);
float2 unpackHalf2x16(uint v);
float length($genType x);
half length($genHType x);
float distance($genType p0, $genType p1);
half distance($genHType p0, $genHType p1);
float dot($genType x, $genType y);
half dot($genHType x, $genHType y);
float3 cross(float3 x, float3 y);
half3 cross(half3 x, half3 y);
$genType normalize($genType x);
$genHType normalize($genHType x);
$genType faceforward($genType N, $genType I, $genType Nref);
$genHType faceforward($genHType N, $genHType I, $genHType Nref);
$genType reflect($genType I, $genType N);
$genHType reflect($genHType I, $genHType N);
$genType refract($genType I, $genType N, float eta);
$genHType refract($genHType I, $genHType N, half eta);
$mat matrixCompMult($mat x, $mat y);
$hmat matrixCompMult($hmat x, $hmat y);
$squareMat outerProduct($vec c, $vec r);
float2x3 outerProduct(float3 c, float2 r);
float3x2 outerProduct(float2 c, float3 r);
float2x4 outerProduct(float4 c, float2 r);
float4x2 outerProduct(float2 c, float4 r);
float3x4 outerProduct(float4 c, float3 r);
float4x3 outerProduct(float3 c, float4 r);
$squareHMat outerProduct($hvec c, $hvec r);
half2x3 outerProduct(half3 c, half2 r);
half3x2 outerProduct(half2 c, half3 r);
half2x4 outerProduct(half4 c, half2 r);
half4x2 outerProduct(half2 c, half4 r);
half3x4 outerProduct(half4 c, half3 r);
half4x3 outerProduct(half3 c, half4 r);
$squareMat transpose($squareMat m);
float2x3 transpose(float3x2 m);
float3x2 transpose(float2x3 m);
float2x4 transpose(float4x2 m);
float4x2 transpose(float2x4 m);
float3x4 transpose(float4x3 m);
float4x3 transpose(float3x4 m);
$squareHMat transpose($squareHMat m);
half2x3 transpose(half3x2 m);
half3x2 transpose(half2x3 m);
half2x4 transpose(half4x2 m);
half4x2 transpose(half2x4 m);
half3x4 transpose(half4x3 m);
half4x3 transpose(half3x4 m);
float determinant($squareMat m);
half determinant($squareHMat m);
$squareMat inverse($squareMat m);
$squareHMat inverse($squareHMat m);
$bvec lessThan($vec x, $vec y);
$bvec lessThan($hvec x, $hvec y);
$bvec lessThan($ivec x, $ivec y);
$bvec lessThan($svec x, $svec y);
$bvec lessThan($usvec x, $usvec y);
$bvec lessThan($uvec x, $uvec y);
$bvec lessThanEqual($vec x, $vec y);
$bvec lessThanEqual($hvec x, $hvec y);
$bvec lessThanEqual($ivec x, $ivec y);
$bvec lessThanEqual($uvec x, $uvec y);
$bvec lessThanEqual($svec x, $svec y);
$bvec lessThanEqual($usvec x, $usvec y);
$bvec greaterThan($vec x, $vec y);
$bvec greaterThan($hvec x, $hvec y);
$bvec greaterThan($ivec x, $ivec y);
$bvec greaterThan($uvec x, $uvec y);
$bvec greaterThan($svec x, $svec y);
$bvec greaterThan($usvec x, $usvec y);
$bvec greaterThanEqual($vec x, $vec y);
$bvec greaterThanEqual($hvec x, $hvec y);
$bvec greaterThanEqual($ivec x, $ivec y);
$bvec greaterThanEqual($uvec x, $uvec y);
$bvec greaterThanEqual($svec x, $svec y);
$bvec greaterThanEqual($usvec x, $usvec y);
$bvec equal($vec x, $vec y);
$bvec equal($hvec x, $hvec y);
$bvec equal($ivec x, $ivec y);
$bvec equal($uvec x, $uvec y);
$bvec equal($svec x, $svec y);
$bvec equal($usvec x, $usvec y);
$bvec equal($bvec x, $bvec y);
$bvec notEqual($vec x, $vec y);
$bvec notEqual($hvec x, $hvec y);
$bvec notEqual($ivec x, $ivec y);
$bvec notEqual($uvec x, $uvec y);
$bvec notEqual($svec x, $svec y);
$bvec notEqual($usvec x, $usvec y);
$bvec notEqual($bvec x, $bvec y);
bool any($bvec x);
bool all($bvec x);
$bvec not($bvec x);

$genIType bitCount($genIType value);
$genIType bitCount($genUType value);
$genIType findLSB($genIType value);
$genIType findLSB($genUType value);
$genIType findMSB($genIType value);
$genIType findMSB($genUType value);

sampler2D makeSampler2D(texture2D texture, sampler s);
int2 textureSize(sampler2DRect s);

half4 sample(sampler1D s, float P);
half4 sample(sampler1D s, float P, float bias);
half4 sample(sampler2D s, float2 P);
int4 sample(isampler2D s, float2 P);
half4 sample(samplerExternalOES s, float2 P, float bias);
half4 sample(samplerExternalOES s, float2 P);

half4 sample(sampler2DRect s, float2 P);
half4 sample(sampler2DRect s, float3 P);

// Currently we do not support the generic types of loading subpassInput so we have some explicit
// versions that we currently use
half4 subpassLoad(subpassInput subpass);
half4 subpassLoad(subpassInputMS subpass, int sample);

half4 sample(sampler1D s, float2 P);
half4 sample(sampler1D s, float2 P, float bias);
half4 sample(sampler2D s, float3 P);
half4 sample(sampler2D s, float3 P, float bias);

$genType dFdx($genType p);
$genType dFdy($genType p);
$genHType dFdx($genHType p);
$genHType dFdy($genHType p);
$genType fwidth($genType p);
$genHType fwidth($genHType p);
float interpolateAtSample(float interpolant, int sample);
float2 interpolateAtSample(float2 interpolant, int sample);
float3 interpolateAtSample(float3 interpolant, int sample);
float4 interpolateAtSample(float4 interpolant, int sample);
float interpolateAtOffset(float interpolant, float2 offset);
float2 interpolateAtOffset(float2 interpolant, float2 offset);
float3 interpolateAtOffset(float3 interpolant, float2 offset);
float4 interpolateAtOffset(float4 interpolant, float2 offset);

// Definitions of functions implementing all of the SkBlendMode blends.

half4 blend_clear(half4 src, half4 dst) { return half4(0); }

half4 blend_src(half4 src, half4 dst) { return src; }

half4 blend_dst(half4 src, half4 dst) { return dst; }

half4 blend_src_over(half4 src, half4 dst) { return src + (1 - src.a)*dst; }

half4 blend_dst_over(half4 src, half4 dst) { return (1 - dst.a)*src + dst; }

half4 blend_src_in(half4 src, half4 dst) { return src*dst.a; }

half4 blend_dst_in(half4 src, half4 dst) { return dst*src.a; }

half4 blend_src_out(half4 src, half4 dst) { return (1 - dst.a)*src; }

half4 blend_dst_out(half4 src, half4 dst) { return (1 - src.a)*dst; }

half4 blend_src_atop(half4 src, half4 dst) { return dst.a*src + (1 - src.a)*dst; }

half4 blend_dst_atop(half4 src, half4 dst)  { return  (1 - dst.a) * src + src.a*dst; }

half4 blend_xor(half4 src, half4 dst) { return (1 - dst.a)*src + (1 - src.a)*dst; }

half4 blend_plus(half4 src, half4 dst) { return min(src + dst, 1); }

half4 blend_modulate(half4 src, half4 dst) { return src*dst; }

half4 blend_screen(half4 src, half4 dst) { return src + (1 - src)*dst; }

half _blend_overlay_component(half2 s, half2 d) {
    return (2*d.x <= d.y)
            ? 2*s.x*d.x
            : s.y*d.y - 2*(d.y - d.x)*(s.y - s.x);
}

half4 blend_overlay(half4 src, half4 dst) {
    half4 result = half4(_blend_overlay_component(src.ra, dst.ra),
                         _blend_overlay_component(src.ga, dst.ga),
                         _blend_overlay_component(src.ba, dst.ba),
                         src.a + (1 - src.a)*dst.a);
    result.rgb += dst.rgb*(1 - src.a) + src.rgb*(1 - dst.a);
    return result;
}

half4 blend_darken(half4 src, half4 dst) {
   half4 result = blend_src_over(src, dst);
   result.rgb = min(result.rgb, (1 - dst.a)*src.rgb + dst.rgb);
   return result;
}

half4 blend_lighten(half4 src, half4 dst) {
    half4 result = blend_src_over(src, dst);
    result.rgb = max(result.rgb, (1 - dst.a)*src.rgb + dst.rgb);
    return result;
}

half _guarded_divide(half n, half d) {
    return sk_Caps.mustGuardDivisionEvenAfterExplicitZeroCheck
            ? n/(d + 0.00000001)
            : n/d;
}

half3 _guarded_divide(half3 n, half d) {
    return sk_Caps.mustGuardDivisionEvenAfterExplicitZeroCheck
            ? n/(d + 0.00000001)
            : n/d;
}

half _color_dodge_component(half2 s, half2 d) {
    if (d.x == 0) {
        return s.x*(1 - d.y);
    } else {
        half delta = s.y - s.x;
        if (delta == 0) {
             return s.y*d.y + s.x*(1 - d.y) + d.x*(1 - s.y);
        } else {
            delta = min(d.y, _guarded_divide(d.x*s.y, delta));
            return delta*s.y + s.x*(1 - d.y) + d.x*(1 - s.y);
        }
    }
}

half4 blend_color_dodge(half4 src, half4 dst) {
    return half4(_color_dodge_component(src.ra, dst.ra),
                 _color_dodge_component(src.ga, dst.ga),
                 _color_dodge_component(src.ba, dst.ba),
                 src.a + (1 - src.a)*dst.a);
}

half _color_burn_component(half2 s, half2 d) {
    if (d.y == d.x) {
        return s.y*d.y + s.x*(1 - d.y) + d.x*(1 - s.y);
    } else if (s.x == 0) {
        return d.x*(1 - s.y);
    } else {
        half delta = max(0, d.y - _guarded_divide((d.y - d.x)*s.y, s.x));
        return delta*s.y + s.x*(1 - d.y) + d.x*(1 - s.y);
    }
}

half4 blend_color_burn(half4 src, half4 dst) {
    return half4(_color_burn_component(src.ra, dst.ra),
                 _color_burn_component(src.ga, dst.ga),
                 _color_burn_component(src.ba, dst.ba),
                 src.a + (1 - src.a)*dst.a);
}

half4 blend_hard_light(half4 src, half4 dst) { return blend_overlay(dst, src); }

half _soft_light_component(half2 s, half2 d) {
    if (2*s.x <= s.y) {
        return _guarded_divide(d.x*d.x*(s.y - 2*s.x), d.y) + (1 - d.y)*s.x + d.x*(-s.y + 2*s.x + 1);
    } else if (4.0 * d.x <= d.y) {
        half DSqd = d.x*d.x;
        half DCub = DSqd*d.x;
        half DaSqd = d.y*d.y;
        half DaCub = DaSqd*d.y;
        return _guarded_divide(DaSqd*(s.x - d.x*(3*s.y - 6*s.x - 1)) + 12*d.y*DSqd*(s.y - 2*s.x)
                               - 16*DCub * (s.y - 2*s.x) - DaCub*s.x, DaSqd);
    } else {
        return d.x*(s.y - 2*s.x + 1) + s.x - sqrt(d.y*d.x)*(s.y - 2*s.x) - d.y*s.x;
    }
}

half4 blend_soft_light(half4 src, half4 dst) {
    return (dst.a == 0) ? src : half4(_soft_light_component(src.ra, dst.ra),
                                      _soft_light_component(src.ga, dst.ga),
                                      _soft_light_component(src.ba, dst.ba),
                                      src.a + (1 - src.a)*dst.a);
}

half4 blend_difference(half4 src, half4 dst) {
    return half4(src.rgb + dst.rgb - 2*min(src.rgb*dst.a, dst.rgb*src.a),
                 src.a + (1 - src.a)*dst.a);
}

half4 blend_exclusion(half4 src, half4 dst) {
    return half4(dst.rgb + src.rgb - 2*dst.rgb*src.rgb, src.a + (1 - src.a)*dst.a);
}

half4 blend_multiply(half4 src, half4 dst) {
    return half4((1 - src.a)*dst.rgb + (1 - dst.a)*src.rgb + src.rgb*dst.rgb,
                 src.a + (1 - src.a)*dst.a);
}

half _blend_color_luminance(half3 color) { return dot(half3(0.3, 0.59, 0.11), color); }

half3 _blend_set_color_luminance(half3 hueSatColor, half alpha, half3 lumColor) {
    half lum = _blend_color_luminance(lumColor);
    half3 result = lum - _blend_color_luminance(hueSatColor) + hueSatColor;
    half minComp = min(min(result.r, result.g), result.b);
    half maxComp = max(max(result.r, result.g), result.b);
    if (minComp < 0 && lum != minComp) {
        result = lum + (result - lum) * _guarded_divide(lum, (lum - minComp));
    }
    if (maxComp > alpha && maxComp != lum) {
        return lum + _guarded_divide((result - lum) * (alpha - lum), (maxComp - lum));
    } else {
        return result;
    }
}

half _blend_color_saturation(half3 color) {
    return max(max(color.r, color.g), color.b) - min(min(color.r, color.g), color.b);
}

half3 _blend_set_color_saturation_helper(half3 minMidMax, half sat) {
    if (minMidMax.r < minMidMax.b) {
        return half3(0,
                    _guarded_divide(sat*(minMidMax.g - minMidMax.r), (minMidMax.b - minMidMax.r)),
                    sat);
    } else {
        return half3(0);
    }
}

half3 _blend_set_color_saturation(half3 hueLumColor, half3 satColor) {
    half sat = _blend_color_saturation(satColor);
    if (hueLumColor.r <= hueLumColor.g) {
        if (hueLumColor.g <= hueLumColor.b) {
            return _blend_set_color_saturation_helper(hueLumColor.rgb, sat);
        } else if (hueLumColor.r <= hueLumColor.b) {
            return _blend_set_color_saturation_helper(hueLumColor.rbg, sat).rbg;
        } else {
            return _blend_set_color_saturation_helper(hueLumColor.brg, sat).gbr;
        }
    } else if (hueLumColor.r <= hueLumColor.b) {
       return _blend_set_color_saturation_helper(hueLumColor.grb, sat).grb;
    } else if (hueLumColor.g <= hueLumColor.b) {
       return _blend_set_color_saturation_helper(hueLumColor.gbr, sat).brg;
    } else {
       return _blend_set_color_saturation_helper(hueLumColor.bgr, sat).bgr;
    }
}

half4 blend_hue(half4 src, half4 dst) {
    half alpha = dst.a*src.a;
    half3 sda = src.rgb*dst.a;
    half3 dsa = dst.rgb*src.a;
    return half4(_blend_set_color_luminance(_blend_set_color_saturation(sda, dsa), alpha, dsa) +
                 dst.rgb - dsa + src.rgb - sda,
                 src.a + dst.a - alpha);
}

half4 blend_saturation(half4 src, half4 dst) {
    half alpha = dst.a*src.a;
    half3 sda = src.rgb*dst.a;
    half3 dsa = dst.rgb*src.a;
    return half4(_blend_set_color_luminance(_blend_set_color_saturation(dsa, sda), alpha, dsa) +
                 dst.rgb - dsa + src.rgb - sda,
                 src.a + dst.a - alpha);
}

half4 blend_color(half4 src, half4 dst)  {
    half alpha = dst.a*src.a;
    half3 sda = src.rgb*dst.a;
    half3 dsa = dst.rgb*src.a;
    return half4(_blend_set_color_luminance(sda, alpha, dsa) + dst.rgb - dsa + src.rgb - sda,
                 src.a + dst.a - alpha);
}

half4 blend_luminosity(half4 src, half4 dst) {
    half alpha = dst.a*src.a;
    half3 sda = src.rgb*dst.a;
    half3 dsa = dst.rgb*src.a;
    return half4(_blend_set_color_luminance(dsa, alpha, sda) + dst.rgb - dsa + src.rgb - sda,
                 src.a + dst.a - alpha);
}

// The max() guards against division by zero when the incoming color is transparent black
half4  unpremul(half4 color)  { return half4(color.rgb / max(color.a, 0.0001), color.a); }
float4 unpremul(float4 color) { return float4(color.rgb / max(color.a, 0.0001), color.a); }

float2 proj(float3 p) { return p.xy / p.z; }

// Implement cross() as a determinant to communicate our intent more clearly to the compiler.
// NOTE: Due to precision issues, it might be the case that cross(a, a) != 0.
float cross(float2 a, float2 b) {
    return sk_Caps.builtinDeterminantSupport ? determinant(float2x2(a, b))
                                             : a.x*b.y - a.y*b.x;
}

half cross(half2 a, half2 b) {
    return sk_Caps.builtinDeterminantSupport ? determinant(half2x2(a, b))
                                             : a.x*b.y - a.y*b.x;
}

// add the support of textureSize
int2 textureSize(sampler2D x, int y);

// add the support of nonuniformEXT
uint nonuniformEXT(uint x);
)"}
};