
/* filter_neon_intrinsics.c - NEON optimised filter functions
 *
 * Copyright (c) 2018 Cosmin Truta
 * Copyright (c) 2014,2016 Glenn Randers-Pehrson
 * Written by James Yu <james.yu at linaro.org>, October 2013.
 * Based on filter_neon.S, written by Mans Rullgard, 2011.
 *
 * This code is released under the libpng license.
 * For conditions of distribution and use, see the disclaimer
 * and license in png.h
 */

#include "../pngpriv.h"

#ifdef PNG_READ_SUPPORTED

/* This code requires -mfpu=neon on the command line: */
#if PNG_ARM_NEON_IMPLEMENTATION == 1 /* intrinsics code from pngpriv.h */

#if defined(_MSC_VER) && !defined(__clang__) && defined(_M_ARM64)
#  include <arm64_neon.h>
#else
#  include <arm_neon.h>
#endif

/* libpng row pointers are not necessarily aligned to any particular boundary,
 * however this code will only work with appropriate alignment.  arm/arm_init.c
 * checks for this (and will not compile unless it is done). This code uses
 * variants of png_aligncast to avoid compiler warnings.
 */
#define png_ptr(type,pointer) png_aligncast(type *,pointer)
#define png_ptrc(type,pointer) png_aligncastconst(const type *,pointer)

/* The following relies on a variable 'temp_pointer' being declared with type
 * 'type'.  This is written this way just to hide the GCC strict aliasing
 * warning; note that the code is safe because there never is an alias between
 * the input and output pointers.
 *
 * When compiling with MSVC ARM64, the png_ldr macro can't be passed directly
 * to vst4_lane_u32, because of an internal compiler error inside MSVC.
 * To avoid this compiler bug, we use a temporary variable (vdest_val) to store
 * the result of png_ldr.
 */
#define png_ldr(type,pointer)\
   (temp_pointer = png_ptr(type,pointer), *temp_pointer)

#if PNG_ARM_NEON_OPT > 0

#ifndef PNG_MULTY_LINE_ENABLE
void
png_read_filter_row_up_neon(png_row_infop row_info, png_bytep row,
   png_const_bytep prev_row)
{
   png_bytep rp = row;
   png_bytep rp_stop = row + row_info->rowbytes;
   png_const_bytep pp = prev_row;

   png_debug(1, "in png_read_filter_row_up_neon");

   for (; rp < rp_stop; rp += 16, pp += 16)
   {
      uint8x16_t qrp, qpp;

      qrp = vld1q_u8(rp);
      qpp = vld1q_u8(pp);
      qrp = vaddq_u8(qrp, qpp);
      vst1q_u8(rp, qrp);
   }
}

void
png_read_filter_row_sub3_neon(png_row_infop row_info, png_bytep row,
   png_const_bytep prev_row)
{
   png_bytep rp = row;
   png_bytep rp_stop = row + row_info->rowbytes;

   uint8x16_t vtmp = vld1q_u8(rp);
   uint8x8x2_t *vrpt = png_ptr(uint8x8x2_t, &vtmp);
   uint8x8x2_t vrp = *vrpt;

   uint8x8x4_t vdest;
   vdest.val[3] = vdup_n_u8(0);

   png_debug(1, "in png_read_filter_row_sub3_neon");

   for (; rp < rp_stop;)
   {
      uint8x8_t vtmp1, vtmp2;
      uint32x2_t *temp_pointer;

      vtmp1 = vext_u8(vrp.val[0], vrp.val[1], 3);
      vdest.val[0] = vadd_u8(vdest.val[3], vrp.val[0]);
      vtmp2 = vext_u8(vrp.val[0], vrp.val[1], 6);
      vdest.val[1] = vadd_u8(vdest.val[0], vtmp1);

      vtmp1 = vext_u8(vrp.val[1], vrp.val[1], 1);
      vdest.val[2] = vadd_u8(vdest.val[1], vtmp2);
      vdest.val[3] = vadd_u8(vdest.val[2], vtmp1);

      vtmp = vld1q_u8(rp + 12);
      vrpt = png_ptr(uint8x8x2_t, &vtmp);
      vrp = *vrpt;

      vst1_lane_u32(png_ptr(uint32_t,rp), png_ldr(uint32x2_t,&vdest.val[0]), 0);
      rp += 3;
      vst1_lane_u32(png_ptr(uint32_t,rp), png_ldr(uint32x2_t,&vdest.val[1]), 0);
      rp += 3;
      vst1_lane_u32(png_ptr(uint32_t,rp), png_ldr(uint32x2_t,&vdest.val[2]), 0);
      rp += 3;
      vst1_lane_u32(png_ptr(uint32_t,rp), png_ldr(uint32x2_t,&vdest.val[3]), 0);
      rp += 3;
   }

   PNG_UNUSED(prev_row)
}

void
png_read_filter_row_sub4_neon(png_row_infop row_info, png_bytep row,
   png_const_bytep prev_row)
{
   png_bytep rp = row;
   png_bytep rp_stop = row + row_info->rowbytes;

   uint8x8x4_t vdest;
   vdest.val[3] = vdup_n_u8(0);

   png_debug(1, "in png_read_filter_row_sub4_neon");

   for (; rp < rp_stop; rp += 16)
   {
      uint32x2x4_t vtmp = vld4_u32(png_ptr(uint32_t,rp));
      uint8x8x4_t *vrpt = png_ptr(uint8x8x4_t,&vtmp);
      uint8x8x4_t vrp = *vrpt;
      uint32x2x4_t *temp_pointer;
      uint32x2x4_t vdest_val;

      vdest.val[0] = vadd_u8(vdest.val[3], vrp.val[0]);
      vdest.val[1] = vadd_u8(vdest.val[0], vrp.val[1]);
      vdest.val[2] = vadd_u8(vdest.val[1], vrp.val[2]);
      vdest.val[3] = vadd_u8(vdest.val[2], vrp.val[3]);

      vdest_val = png_ldr(uint32x2x4_t, &vdest);
      vst4_lane_u32(png_ptr(uint32_t,rp), vdest_val, 0);
   }

   PNG_UNUSED(prev_row)
}

void
png_read_filter_row_avg3_neon(png_row_infop row_info, png_bytep row,
   png_const_bytep prev_row)
{
   png_bytep rp = row;
   png_const_bytep pp = prev_row;
   png_bytep rp_stop = row + row_info->rowbytes;

   uint8x16_t vtmp;
   uint8x8x2_t *vrpt;
   uint8x8x2_t vrp;
   uint8x8x4_t vdest;
   vdest.val[3] = vdup_n_u8(0);

   vtmp = vld1q_u8(rp);
   vrpt = png_ptr(uint8x8x2_t,&vtmp);
   vrp = *vrpt;

   png_debug(1, "in png_read_filter_row_avg3_neon");

   for (; rp < rp_stop; pp += 12)
   {
      uint8x8_t vtmp1, vtmp2, vtmp3;

      uint8x8x2_t *vppt;
      uint8x8x2_t vpp;

      uint32x2_t *temp_pointer;

      vtmp = vld1q_u8(pp);
      vppt = png_ptr(uint8x8x2_t,&vtmp);
      vpp = *vppt;

      vtmp1 = vext_u8(vrp.val[0], vrp.val[1], 3);
      vdest.val[0] = vhadd_u8(vdest.val[3], vpp.val[0]);
      vdest.val[0] = vadd_u8(vdest.val[0], vrp.val[0]);

      vtmp2 = vext_u8(vpp.val[0], vpp.val[1], 3);
      vtmp3 = vext_u8(vrp.val[0], vrp.val[1], 6);
      vdest.val[1] = vhadd_u8(vdest.val[0], vtmp2);
      vdest.val[1] = vadd_u8(vdest.val[1], vtmp1);

      vtmp2 = vext_u8(vpp.val[0], vpp.val[1], 6);
      vtmp1 = vext_u8(vrp.val[1], vrp.val[1], 1);

      vtmp = vld1q_u8(rp + 12);
      vrpt = png_ptr(uint8x8x2_t,&vtmp);
      vrp = *vrpt;

      vdest.val[2] = vhadd_u8(vdest.val[1], vtmp2);
      vdest.val[2] = vadd_u8(vdest.val[2], vtmp3);

      vtmp2 = vext_u8(vpp.val[1], vpp.val[1], 1);

      vdest.val[3] = vhadd_u8(vdest.val[2], vtmp2);
      vdest.val[3] = vadd_u8(vdest.val[3], vtmp1);

      vst1_lane_u32(png_ptr(uint32_t,rp), png_ldr(uint32x2_t,&vdest.val[0]), 0);
      rp += 3;
      vst1_lane_u32(png_ptr(uint32_t,rp), png_ldr(uint32x2_t,&vdest.val[1]), 0);
      rp += 3;
      vst1_lane_u32(png_ptr(uint32_t,rp), png_ldr(uint32x2_t,&vdest.val[2]), 0);
      rp += 3;
      vst1_lane_u32(png_ptr(uint32_t,rp), png_ldr(uint32x2_t,&vdest.val[3]), 0);
      rp += 3;
   }
}

void
png_read_filter_row_avg4_neon(png_row_infop row_info, png_bytep row,
   png_const_bytep prev_row)
{
   png_bytep rp = row;
   png_bytep rp_stop = row + row_info->rowbytes;
   png_const_bytep pp = prev_row;

   uint8x8x4_t vdest;
   vdest.val[3] = vdup_n_u8(0);

   png_debug(1, "in png_read_filter_row_avg4_neon");

   for (; rp < rp_stop; rp += 16, pp += 16)
   {
      uint32x2x4_t vtmp;
      uint8x8x4_t *vrpt, *vppt;
      uint8x8x4_t vrp, vpp;
      uint32x2x4_t *temp_pointer;
      uint32x2x4_t vdest_val;

      vtmp = vld4_u32(png_ptr(uint32_t,rp));
      vrpt = png_ptr(uint8x8x4_t,&vtmp);
      vrp = *vrpt;
      vtmp = vld4_u32(png_ptrc(uint32_t,pp));
      vppt = png_ptr(uint8x8x4_t,&vtmp);
      vpp = *vppt;

      vdest.val[0] = vhadd_u8(vdest.val[3], vpp.val[0]);
      vdest.val[0] = vadd_u8(vdest.val[0], vrp.val[0]);
      vdest.val[1] = vhadd_u8(vdest.val[0], vpp.val[1]);
      vdest.val[1] = vadd_u8(vdest.val[1], vrp.val[1]);
      vdest.val[2] = vhadd_u8(vdest.val[1], vpp.val[2]);
      vdest.val[2] = vadd_u8(vdest.val[2], vrp.val[2]);
      vdest.val[3] = vhadd_u8(vdest.val[2], vpp.val[3]);
      vdest.val[3] = vadd_u8(vdest.val[3], vrp.val[3]);

      vdest_val = png_ldr(uint32x2x4_t, &vdest);
      vst4_lane_u32(png_ptr(uint32_t,rp), vdest_val, 0);
   }
}

static uint8x8_t
paeth(uint8x8_t a, uint8x8_t b, uint8x8_t c)
{
   uint8x8_t d, e;
   uint16x8_t p1, pa, pb, pc;

   p1 = vaddl_u8(a, b); /* a + b */
   pc = vaddl_u8(c, c); /* c * 2 */
   pa = vabdl_u8(b, c); /* pa */
   pb = vabdl_u8(a, c); /* pb */
   pc = vabdq_u16(p1, pc); /* pc */

   p1 = vcleq_u16(pa, pb); /* pa <= pb */
   pa = vcleq_u16(pa, pc); /* pa <= pc */
   pb = vcleq_u16(pb, pc); /* pb <= pc */

   p1 = vandq_u16(p1, pa); /* pa <= pb && pa <= pc */

   d = vmovn_u16(pb);
   e = vmovn_u16(p1);

   d = vbsl_u8(d, b, c);
   e = vbsl_u8(e, a, d);

   return e;
}

void
png_read_filter_row_paeth3_neon(png_row_infop row_info, png_bytep row,
   png_const_bytep prev_row)
{
   png_bytep rp = row;
   png_const_bytep pp = prev_row;
   png_bytep rp_stop = row + row_info->rowbytes;

   uint8x16_t vtmp;
   uint8x8x2_t *vrpt;
   uint8x8x2_t vrp;
   uint8x8_t vlast = vdup_n_u8(0);
   uint8x8x4_t vdest;
   vdest.val[3] = vdup_n_u8(0);

   vtmp = vld1q_u8(rp);
   vrpt = png_ptr(uint8x8x2_t,&vtmp);
   vrp = *vrpt;

   png_debug(1, "in png_read_filter_row_paeth3_neon");

   for (; rp < rp_stop; pp += 12)
   {
      uint8x8x2_t *vppt;
      uint8x8x2_t vpp;
      uint8x8_t vtmp1, vtmp2, vtmp3;
      uint32x2_t *temp_pointer;

      vtmp = vld1q_u8(pp);
      vppt = png_ptr(uint8x8x2_t,&vtmp);
      vpp = *vppt;

      vdest.val[0] = paeth(vdest.val[3], vpp.val[0], vlast);
      vdest.val[0] = vadd_u8(vdest.val[0], vrp.val[0]);

      vtmp1 = vext_u8(vrp.val[0], vrp.val[1], 3);
      vtmp2 = vext_u8(vpp.val[0], vpp.val[1], 3);
      vdest.val[1] = paeth(vdest.val[0], vtmp2, vpp.val[0]);
      vdest.val[1] = vadd_u8(vdest.val[1], vtmp1);

      vtmp1 = vext_u8(vrp.val[0], vrp.val[1], 6);
      vtmp3 = vext_u8(vpp.val[0], vpp.val[1], 6);
      vdest.val[2] = paeth(vdest.val[1], vtmp3, vtmp2);
      vdest.val[2] = vadd_u8(vdest.val[2], vtmp1);

      vtmp1 = vext_u8(vrp.val[1], vrp.val[1], 1);
      vtmp2 = vext_u8(vpp.val[1], vpp.val[1], 1);

      vtmp = vld1q_u8(rp + 12);
      vrpt = png_ptr(uint8x8x2_t,&vtmp);
      vrp = *vrpt;

      vdest.val[3] = paeth(vdest.val[2], vtmp2, vtmp3);
      vdest.val[3] = vadd_u8(vdest.val[3], vtmp1);

      vlast = vtmp2;

      vst1_lane_u32(png_ptr(uint32_t,rp), png_ldr(uint32x2_t,&vdest.val[0]), 0);
      rp += 3;
      vst1_lane_u32(png_ptr(uint32_t,rp), png_ldr(uint32x2_t,&vdest.val[1]), 0);
      rp += 3;
      vst1_lane_u32(png_ptr(uint32_t,rp), png_ldr(uint32x2_t,&vdest.val[2]), 0);
      rp += 3;
      vst1_lane_u32(png_ptr(uint32_t,rp), png_ldr(uint32x2_t,&vdest.val[3]), 0);
      rp += 3;
   }
}

void
png_read_filter_row_paeth4_neon(png_row_infop row_info, png_bytep row,
   png_const_bytep prev_row)
{
   png_bytep rp = row;
   png_bytep rp_stop = row + row_info->rowbytes;
   png_const_bytep pp = prev_row;

   uint8x8_t vlast = vdup_n_u8(0);
   uint8x8x4_t vdest;
   vdest.val[3] = vdup_n_u8(0);

   png_debug(1, "in png_read_filter_row_paeth4_neon");

   for (; rp < rp_stop; rp += 16, pp += 16)
   {
      uint32x2x4_t vtmp;
      uint8x8x4_t *vrpt, *vppt;
      uint8x8x4_t vrp, vpp;
      uint32x2x4_t *temp_pointer;
      uint32x2x4_t vdest_val;

      vtmp = vld4_u32(png_ptr(uint32_t,rp));
      vrpt = png_ptr(uint8x8x4_t,&vtmp);
      vrp = *vrpt;
      vtmp = vld4_u32(png_ptrc(uint32_t,pp));
      vppt = png_ptr(uint8x8x4_t,&vtmp);
      vpp = *vppt;

      vdest.val[0] = paeth(vdest.val[3], vpp.val[0], vlast);
      vdest.val[0] = vadd_u8(vdest.val[0], vrp.val[0]);
      vdest.val[1] = paeth(vdest.val[0], vpp.val[1], vpp.val[0]);
      vdest.val[1] = vadd_u8(vdest.val[1], vrp.val[1]);
      vdest.val[2] = paeth(vdest.val[1], vpp.val[2], vpp.val[1]);
      vdest.val[2] = vadd_u8(vdest.val[2], vrp.val[2]);
      vdest.val[3] = paeth(vdest.val[2], vpp.val[3], vpp.val[2]);
      vdest.val[3] = vadd_u8(vdest.val[3], vrp.val[3]);

      vlast = vpp.val[3];

      vdest_val = png_ldr(uint32x2x4_t, &vdest);
      vst4_lane_u32(png_ptr(uint32_t,rp), vdest_val, 0);
   }
}
#else
// 根据rowbytes定义，row_info->rowbytes = row_width * row_info->channels
// 输入filter的rowbytes一定是channels(3或4)的倍数，因此针对向量化运算:
// RGB一次处理12个字节，尾部字节数只能是3，6，9
// RGBA一次处理16或8个字节，尾部字节数只能是4
// filter算子都是内部函数，row_info，row等指针，外部已判断非空，此处不需要判断
#define STEP_RGB (12) // 3通道RGB图像，迭代步长12个字节
#define TAIL_RGB3 (9) // 尾部3个像素9个字节
#define TAIL_RGB2 (6) // 尾部2个像素6个字节
#define TAIL_RGB1 (3) // 尾部1个像素3个字节
#define STEP_RGBA (16) // 4通道RGBA图像，迭代步长16个字节
#define STEP_RGBA_HALF (8) // 4通道RGBA图像，迭代步长8个字节
#define TAIL_RGBA (4) // 尾部1个像素4个字节
#define IND3 (3) // 下标3
#define IND2 (2) // 下标2
#define OFFSET3 (3) // RGB图像偏移3个字节
#define OFFSET6 (6) // RGB图像偏移6个字节
void png_read_filter_row_up_neon(png_row_infop row_info, png_bytep row,
   png_const_bytep prev_row)
{
   png_bytep rp = row;
   png_const_bytep pp = prev_row;
   int count = row_info->rowbytes;

   png_debug(1, "in png_read_filter_row_up_neon");

   uint8x16_t qrp, qpp;
   while (count >= STEP_RGBA) {
      qrp = vld1q_u8(rp);
      qpp = vld1q_u8(pp);
      qrp = vaddq_u8(qrp, qpp);
      vst1q_u8(rp, qrp);
      rp += STEP_RGBA;
      pp += STEP_RGBA;
      count -= STEP_RGBA;
   }

   if (count >= STEP_RGBA_HALF) {
      uint8x8_t qrp1, qpp1;
      qrp1 = vld1_u8(rp);
      qpp1 = vld1_u8(pp);
      qrp1 = vadd_u8(qrp1, qpp1);
      vst1_u8(rp, qrp1);
      rp += STEP_RGBA_HALF;
      pp += STEP_RGBA_HALF;
      count -= STEP_RGBA_HALF;
   }

   for (int i = 0; i < count; i++) {
      *rp = (png_byte)(((int)(*rp) + (int)(*pp++)) & 0xff);
      rp++;
   }
}

void png_read_filter_row_up_x2_neon(png_row_infop row_info, png_bytep row,
   png_const_bytep prev_row)
{
   png_bytep rp = row;
   png_const_bytep pp = prev_row;
   int count = row_info->rowbytes;
   png_bytep np = row + row_info->rowbytes + 1;

   png_debug(1, "in png_read_filter_row_up_x2_neon");

   uint8x16_t qrp, qpp, qnp;
   while (count >= STEP_RGBA) {
      qrp = vld1q_u8(rp);
      qpp = vld1q_u8(pp);
      qnp = vld1q_u8(np);
      qrp = vaddq_u8(qrp, qpp);
      qnp = vaddq_u8(qnp, qrp);
      vst1q_u8(rp, qrp);
      vst1q_u8(np, qnp);
      rp += STEP_RGBA;
      pp += STEP_RGBA;
      np += STEP_RGBA;
      count -= STEP_RGBA;
   }

   if (count >= STEP_RGBA_HALF) {
      uint8x8_t qrp1, qpp1, qnp1;
      qrp1 = vld1_u8(rp);
      qpp1 = vld1_u8(pp);
      qnp1 = vld1_u8(np);
      qrp1 = vadd_u8(qrp1, qpp1);
      qnp1 = vadd_u8(qnp1, qrp1);
      vst1_u8(rp, qrp1);
      vst1_u8(np, qnp1);
      rp += STEP_RGBA_HALF;
      pp += STEP_RGBA_HALF;
      np += STEP_RGBA_HALF;
      count -= STEP_RGBA_HALF;
   }

   for (int i = 0; i < count; i++) {
      *rp = (png_byte)(((int)(*rp) + (int)(*pp++)) & 0xff);
      *np = (png_byte)(((int)(*np) + (int)(*rp++)) & 0xff);
      np++;
   }
}

void png_read_filter_row_sub3_neon(png_row_infop row_info, png_bytep row,
   png_const_bytep prev_row)
{
   png_bytep rp = row;
   png_bytep rp_stop = row + row_info->rowbytes;

   uint8x16_t vtmp = vld1q_u8(rp);
   uint8x8x2_t *vrpt = png_ptr(uint8x8x2_t, &vtmp);
   uint8x8x2_t vrp = *vrpt;

   uint8x8x4_t vdest;
   vdest.val[IND3] = vdup_n_u8(0);

   uint8x8_t vtmp1, vtmp2;
   uint32x2_t *temp_pointer;

   png_debug(1, "in png_read_filter_row_sub3_neon");

   size_t tail_bytes = row_info->rowbytes % STEP_RGB;
   png_byte last_byte = *rp_stop;
   png_bytep rp_stop_new = rp_stop - tail_bytes;
   for (; rp < rp_stop_new;)
   {
      vtmp1 = vext_u8(vrp.val[0], vrp.val[1], OFFSET3);
      vdest.val[0] = vadd_u8(vdest.val[IND3], vrp.val[0]);
      vtmp2 = vext_u8(vrp.val[0], vrp.val[1], OFFSET6);
      vdest.val[1] = vadd_u8(vdest.val[0], vtmp1);

      vtmp1 = vext_u8(vrp.val[1], vrp.val[1], 1);
      vdest.val[IND2] = vadd_u8(vdest.val[1], vtmp2);
      vdest.val[IND3] = vadd_u8(vdest.val[IND2], vtmp1);

      vtmp = vld1q_u8(rp + STEP_RGB);
      vrpt = png_ptr(uint8x8x2_t, &vtmp);
      vrp = *vrpt;

      vst1_lane_u32(png_ptr(uint32_t, rp), png_ldr(uint32x2_t, &vdest.val[0]), 0);
      rp += OFFSET3;
      vst1_lane_u32(png_ptr(uint32_t, rp), png_ldr(uint32x2_t, &vdest.val[1]), 0);
      rp += OFFSET3;
      vst1_lane_u32(png_ptr(uint32_t, rp), png_ldr(uint32x2_t, &vdest.val[IND2]), 0);
      rp += OFFSET3;
      vst1_lane_u32(png_ptr(uint32_t, rp), png_ldr(uint32x2_t, &vdest.val[IND3]), 0);
      rp += OFFSET3;
   }

   if (tail_bytes == TAIL_RGB1) {
      vdest.val[0] = vadd_u8(vdest.val[IND3], vrp.val[0]);
      vst1_lane_u32(png_ptr(uint32_t, rp), png_ldr(uint32x2_t, &vdest.val[0]), 0);
   } else if (tail_bytes == TAIL_RGB2) {
      vtmp1 = vext_u8(vrp.val[0], vrp.val[1], OFFSET3);
      vdest.val[0] = vadd_u8(vdest.val[IND3], vrp.val[0]);
      vdest.val[1] = vadd_u8(vdest.val[0], vtmp1);

      vst1_lane_u32(png_ptr(uint32_t, rp), png_ldr(uint32x2_t, &vdest.val[0]), 0);
      rp += OFFSET3;
      vst1_lane_u32(png_ptr(uint32_t, rp), png_ldr(uint32x2_t, &vdest.val[1]), 0);
   } else if (tail_bytes == TAIL_RGB3) {
      vtmp1 = vext_u8(vrp.val[0], vrp.val[1], OFFSET3);
      vdest.val[0] = vadd_u8(vdest.val[IND3], vrp.val[0]);
      vtmp2 = vext_u8(vrp.val[0], vrp.val[1], OFFSET6);
      vdest.val[1] = vadd_u8(vdest.val[0], vtmp1);
      vdest.val[IND2] = vadd_u8(vdest.val[1], vtmp2);

      vst1_lane_u32(png_ptr(uint32_t, rp), png_ldr(uint32x2_t, &vdest.val[0]), 0);
      rp += OFFSET3;
      vst1_lane_u32(png_ptr(uint32_t, rp), png_ldr(uint32x2_t, &vdest.val[1]), 0);
      rp += OFFSET3;
      vst1_lane_u32(png_ptr(uint32_t, rp), png_ldr(uint32x2_t, &vdest.val[IND2]), 0);
   }
   *rp_stop = last_byte;

   PNG_UNUSED(prev_row)
}

void png_read_filter_row_sub4_neon(png_row_infop row_info, png_bytep row,
   png_const_bytep prev_row)
{
   png_bytep rp = row;
   int count = row_info->rowbytes;

   uint8x8x4_t vdest;
   vdest.val[IND3] = vdup_n_u8(0);

   png_debug(1, "in png_read_filter_row_sub4_neon");

   uint32x2x4_t vtmp;
   uint8x8x4_t *vrpt;
   uint8x8x4_t vrp;
   uint32x2x4_t vdest_val;
   while (count >= STEP_RGBA) {
      uint32x2x4_t *temp_pointer;
      vtmp = vld4_u32(png_ptr(uint32_t, rp));
      vrpt = png_ptr(uint8x8x4_t, &vtmp);
      vrp = *vrpt;

      vdest.val[0] = vadd_u8(vdest.val[IND3], vrp.val[0]);
      vdest.val[1] = vadd_u8(vdest.val[0], vrp.val[1]);
      vdest.val[IND2] = vadd_u8(vdest.val[1], vrp.val[IND2]);
      vdest.val[IND3] = vadd_u8(vdest.val[IND2], vrp.val[IND3]);

      vdest_val = png_ldr(uint32x2x4_t, &vdest);
      vst4_lane_u32(png_ptr(uint32_t, rp), vdest_val, 0);

      rp += STEP_RGBA;
      count -= STEP_RGBA;
   }

   if (count >= STEP_RGBA_HALF) {
      uint32x2x2_t vtmp1 = vld2_u32(png_ptr(uint32_t, rp));
      uint8x8x2_t *vrpt1 = png_ptr(uint8x8x2_t, &vtmp1);
      uint8x8x2_t vrp1 = *vrpt1;
      uint32x2x2_t *temp_pointer;
      uint32x2x2_t vdest_val1;

      vdest.val[0] = vadd_u8(vdest.val[IND3], vrp1.val[0]);
      vdest.val[1] = vadd_u8(vdest.val[0], vrp1.val[1]);

      vdest_val1 = png_ldr(uint32x2x2_t, &vdest);
      vst2_lane_u32(png_ptr(uint32_t, rp), vdest_val1, 0);

      rp += STEP_RGBA_HALF;
      count -= STEP_RGBA_HALF;
   }

   if (count == 0) {
      return;
   }

   uint32x2_t vtmp2 = vld1_u32(png_ptr(uint32_t, rp));
   uint8x8_t *vrpt2 = png_ptr(uint8x8_t, &vtmp2);
   uint8x8_t vrp2 = *vrpt2;
   uint32x2_t *temp_pointer;
   uint32x2_t vdest_val2;

   vdest.val[0] = vadd_u8(vdest.val[1], vrp2);
   vdest_val2 = png_ldr(uint32x2_t, &vdest);
   vst1_lane_u32(png_ptr(uint32_t, rp), vdest_val2, 0);

   PNG_UNUSED(prev_row)
}

void png_read_filter_row_avg3_neon(png_row_infop row_info, png_bytep row,
   png_const_bytep prev_row)
{
   png_bytep rp = row;
   png_const_bytep pp = prev_row;
   png_bytep rp_stop = row + row_info->rowbytes;

   uint8x16_t vtmp;
   uint8x8x2_t *vrpt;
   uint8x8x2_t vrp;
   uint8x8x4_t vdest;
   vdest.val[IND3] = vdup_n_u8(0);

   vtmp = vld1q_u8(rp);
   vrpt = png_ptr(uint8x8x2_t, &vtmp);
   vrp = *vrpt;

   png_debug(1, "in png_read_filter_row_avg3_neon");

   uint8x8_t vtmp1, vtmp2, vtmp3;
   uint8x8x2_t *vppt;
   uint8x8x2_t vpp;
   uint32x2_t *temp_pointer;

   size_t tail_bytes = row_info->rowbytes % STEP_RGB;
   png_byte last_byte = *rp_stop;
   png_bytep rp_stop_new = rp_stop - tail_bytes;
   for (; rp < rp_stop_new; pp += STEP_RGB)
   {
      vtmp = vld1q_u8(pp);
      vppt = png_ptr(uint8x8x2_t, &vtmp);
      vpp = *vppt;

      vtmp1 = vext_u8(vrp.val[0], vrp.val[1], OFFSET3);
      vdest.val[0] = vhadd_u8(vdest.val[IND3], vpp.val[0]);
      vdest.val[0] = vadd_u8(vdest.val[0], vrp.val[0]);

      vtmp2 = vext_u8(vpp.val[0], vpp.val[1], OFFSET3);
      vtmp3 = vext_u8(vrp.val[0], vrp.val[1], OFFSET6);
      vdest.val[1] = vhadd_u8(vdest.val[0], vtmp2);
      vdest.val[1] = vadd_u8(vdest.val[1], vtmp1);

      vtmp2 = vext_u8(vpp.val[0], vpp.val[1], OFFSET6);
      vtmp1 = vext_u8(vrp.val[1], vrp.val[1], 1);

      vtmp = vld1q_u8(rp + STEP_RGB);
      vrpt = png_ptr(uint8x8x2_t, &vtmp);
      vrp = *vrpt;

      vdest.val[IND2] = vhadd_u8(vdest.val[1], vtmp2);
      vdest.val[IND2] = vadd_u8(vdest.val[IND2], vtmp3);

      vtmp2 = vext_u8(vpp.val[1], vpp.val[1], 1);

      vdest.val[IND3] = vhadd_u8(vdest.val[IND2], vtmp2);
      vdest.val[IND3] = vadd_u8(vdest.val[IND3], vtmp1);

      vst1_lane_u32(png_ptr(uint32_t, rp), png_ldr(uint32x2_t, &vdest.val[0]), 0);
      rp += OFFSET3;
      vst1_lane_u32(png_ptr(uint32_t, rp), png_ldr(uint32x2_t, &vdest.val[1]), 0);
      rp += OFFSET3;
      vst1_lane_u32(png_ptr(uint32_t, rp), png_ldr(uint32x2_t, &vdest.val[IND2]), 0);
      rp += OFFSET3;
      vst1_lane_u32(png_ptr(uint32_t, rp), png_ldr(uint32x2_t, &vdest.val[IND3]), 0);
      rp += OFFSET3;
   }

   vtmp = vld1q_u8(pp);
   vppt = png_ptr(uint8x8x2_t, &vtmp);
   vpp = *vppt;

   if (tail_bytes == TAIL_RGB1) {
      vdest.val[0] = vhadd_u8(vdest.val[IND3], vpp.val[0]);
      vdest.val[0] = vadd_u8(vdest.val[0], vrp.val[0]);
      vst1_lane_u32(png_ptr(uint32_t, rp), png_ldr(uint32x2_t, &vdest.val[0]), 0);
   } else if (tail_bytes == TAIL_RGB2) {
      vtmp1 = vext_u8(vrp.val[0], vrp.val[1], OFFSET3);
      vdest.val[0] = vhadd_u8(vdest.val[IND3], vpp.val[0]);
      vdest.val[0] = vadd_u8(vdest.val[0], vrp.val[0]);

      vtmp2 = vext_u8(vpp.val[0], vpp.val[1], OFFSET3);
      vdest.val[1] = vhadd_u8(vdest.val[0], vtmp2);
      vdest.val[1] = vadd_u8(vdest.val[1], vtmp1);

      vst1_lane_u32(png_ptr(uint32_t, rp), png_ldr(uint32x2_t, &vdest.val[0]), 0);
      rp += OFFSET3;
      vst1_lane_u32(png_ptr(uint32_t, rp), png_ldr(uint32x2_t, &vdest.val[1]), 0);
   } else if (tail_bytes == TAIL_RGB3) {
      vtmp1 = vext_u8(vrp.val[0], vrp.val[1], OFFSET3);
      vdest.val[0] = vhadd_u8(vdest.val[IND3], vpp.val[0]);
      vdest.val[0] = vadd_u8(vdest.val[0], vrp.val[0]);

      vtmp2 = vext_u8(vpp.val[0], vpp.val[1], OFFSET3);
      vtmp3 = vext_u8(vrp.val[0], vrp.val[1], OFFSET6);
      vdest.val[1] = vhadd_u8(vdest.val[0], vtmp2);
      vdest.val[1] = vadd_u8(vdest.val[1], vtmp1);

      vtmp2 = vext_u8(vpp.val[0], vpp.val[1], OFFSET6);

      vdest.val[IND2] = vhadd_u8(vdest.val[1], vtmp2);
      vdest.val[IND2] = vadd_u8(vdest.val[IND2], vtmp3);

      vst1_lane_u32(png_ptr(uint32_t, rp), png_ldr(uint32x2_t, &vdest.val[0]), 0);
      rp += OFFSET3;
      vst1_lane_u32(png_ptr(uint32_t, rp), png_ldr(uint32x2_t, &vdest.val[1]), 0);
      rp += OFFSET3;
      vst1_lane_u32(png_ptr(uint32_t, rp), png_ldr(uint32x2_t, &vdest.val[IND2]), 0);
   }
   *rp_stop = last_byte;
}

void png_read_filter_row_avg3_x2_neon(png_row_infop row_info, png_bytep row,
   png_const_bytep prev_row)
{
   png_bytep rp = row;
   png_const_bytep pp = prev_row;
   png_bytep rp_stop = row + row_info->rowbytes;
   png_bytep np = rp_stop + 1;

   uint8x16_t vtmp;
   uint8x8x2_t *vrpt;
   uint8x8x2_t vrp;
   uint8x8x4_t vdest;
   vdest.val[IND3] = vdup_n_u8(0);

   vtmp = vld1q_u8(rp);
   vrpt = png_ptr(uint8x8x2_t, &vtmp);
   vrp = *vrpt;

   uint8x8x2_t *vnpt;
   uint8x8x2_t vnp;
   uint8x8x4_t vdestN;
   vdestN.val[IND3] = vdup_n_u8(0);

   vtmp = vld1q_u8(np);
   vnpt = png_ptr(uint8x8x2_t, &vtmp);
   vnp = *vnpt;

   png_debug(1, "in png_read_filter_row_x2_avg3_neon");

   uint8x8_t vtmp1, vtmp2, vtmp3;
   uint8x8x2_t *vppt;
   uint8x8x2_t vpp;
   uint32x2_t *temp_pointer;

   size_t tail_bytes = row_info->rowbytes % STEP_RGB;
   png_byte last_byte = *rp_stop;
   png_byte last_byte_next = *(rp_stop + row_info->rowbytes + 1);
   png_bytep rp_stop_new = rp_stop - tail_bytes;
   for (; rp < rp_stop_new; pp += STEP_RGB)
   {
      vtmp = vld1q_u8(pp);
      vppt = png_ptr(uint8x8x2_t, &vtmp);
      vpp = *vppt;

      vtmp1 = vext_u8(vrp.val[0], vrp.val[1], OFFSET3);
      vdest.val[0] = vhadd_u8(vdest.val[IND3], vpp.val[0]);
      vdest.val[0] = vadd_u8(vdest.val[0], vrp.val[0]);

      vtmp2 = vext_u8(vpp.val[0], vpp.val[1], OFFSET3);
      vtmp3 = vext_u8(vrp.val[0], vrp.val[1], OFFSET6);
      vdest.val[1] = vhadd_u8(vdest.val[0], vtmp2);
      vdest.val[1] = vadd_u8(vdest.val[1], vtmp1);

      vtmp2 = vext_u8(vpp.val[0], vpp.val[1], OFFSET6);
      vtmp1 = vext_u8(vrp.val[1], vrp.val[1], 1);

      vtmp = vld1q_u8(rp + STEP_RGB);
      vrpt = png_ptr(uint8x8x2_t, &vtmp);
      vrp = *vrpt;

      vdest.val[IND2] = vhadd_u8(vdest.val[1], vtmp2);
      vdest.val[IND2] = vadd_u8(vdest.val[IND2], vtmp3);

      vtmp2 = vext_u8(vpp.val[1], vpp.val[1], 1);

      vdest.val[IND3] = vhadd_u8(vdest.val[IND2], vtmp2);
      vdest.val[IND3] = vadd_u8(vdest.val[IND3], vtmp1);

      vst1_lane_u32(png_ptr(uint32_t, rp), png_ldr(uint32x2_t, &vdest.val[0]), 0);
      rp += OFFSET3;
      vst1_lane_u32(png_ptr(uint32_t, rp), png_ldr(uint32x2_t, &vdest.val[1]), 0);
      rp += OFFSET3;
      vst1_lane_u32(png_ptr(uint32_t, rp), png_ldr(uint32x2_t, &vdest.val[IND2]), 0);
      rp += OFFSET3;
      vst1_lane_u32(png_ptr(uint32_t, rp), png_ldr(uint32x2_t, &vdest.val[IND3]), 0);
      rp += OFFSET3;

      vtmp1 = vext_u8(vnp.val[0], vnp.val[1], OFFSET3);
      vdestN.val[0] = vhadd_u8(vdestN.val[IND3], vdest.val[0]);
      vdestN.val[0] = vadd_u8(vdestN.val[0], vnp.val[0]);

      vtmp3 = vext_u8(vnp.val[0], vnp.val[1], OFFSET6);
      vdestN.val[1] = vhadd_u8(vdestN.val[0], vdest.val[1]);
      vdestN.val[1] = vadd_u8(vdestN.val[1], vtmp1);

      vtmp1 = vext_u8(vnp.val[1], vnp.val[1], 1);

      vtmp = vld1q_u8(np + STEP_RGB);
      vnpt = png_ptr(uint8x8x2_t, &vtmp);
      vnp = *vnpt;

      vdestN.val[IND2] = vhadd_u8(vdestN.val[1], vdest.val[IND2]);
      vdestN.val[IND2] = vadd_u8(vdestN.val[IND2], vtmp3);

      vdestN.val[IND3] = vhadd_u8(vdestN.val[IND2], vdest.val[IND3]);
      vdestN.val[IND3] = vadd_u8(vdestN.val[IND3], vtmp1);

      vst1_lane_u32(png_ptr(uint32_t, np), png_ldr(uint32x2_t, &vdestN.val[0]), 0);
      np += OFFSET3;
      vst1_lane_u32(png_ptr(uint32_t, np), png_ldr(uint32x2_t, &vdestN.val[1]), 0);
      np += OFFSET3;
      vst1_lane_u32(png_ptr(uint32_t, np), png_ldr(uint32x2_t, &vdestN.val[IND2]), 0);
      np += OFFSET3;
      vst1_lane_u32(png_ptr(uint32_t, np), png_ldr(uint32x2_t, &vdestN.val[IND3]), 0);
      np += OFFSET3;
   }

   vtmp = vld1q_u8(pp);
   vppt = png_ptr(uint8x8x2_t, &vtmp);
   vpp = *vppt;

   if (tail_bytes == TAIL_RGB1) {
      vdest.val[0] = vhadd_u8(vdest.val[IND3], vpp.val[0]);
      vdest.val[0] = vadd_u8(vdest.val[0], vrp.val[0]);
      vst1_lane_u32(png_ptr(uint32_t, rp), png_ldr(uint32x2_t, &vdest.val[0]), 0);

      vdestN.val[0] = vhadd_u8(vdestN.val[IND3], vdest.val[0]);
      vdestN.val[0] = vadd_u8(vdestN.val[0], vnp.val[0]);
      vst1_lane_u32(png_ptr(uint32_t, np), png_ldr(uint32x2_t, &vdestN.val[0]), 0);
   } else if (tail_bytes == TAIL_RGB2) {
      vtmp1 = vext_u8(vrp.val[0], vrp.val[1], OFFSET3);
      vdest.val[0] = vhadd_u8(vdest.val[IND3], vpp.val[0]);
      vdest.val[0] = vadd_u8(vdest.val[0], vrp.val[0]);

      vtmp2 = vext_u8(vpp.val[0], vpp.val[1], OFFSET3);
      vdest.val[1] = vhadd_u8(vdest.val[0], vtmp2);
      vdest.val[1] = vadd_u8(vdest.val[1], vtmp1);

      vst1_lane_u32(png_ptr(uint32_t, rp), png_ldr(uint32x2_t, &vdest.val[0]), 0);
      rp += OFFSET3;
      vst1_lane_u32(png_ptr(uint32_t, rp), png_ldr(uint32x2_t, &vdest.val[1]), 0);

      vtmp1 = vext_u8(vnp.val[0], vnp.val[1], OFFSET3);
      vdestN.val[0] = vhadd_u8(vdestN.val[IND3], vdest.val[0]);
      vdestN.val[0] = vadd_u8(vdestN.val[0], vnp.val[0]);

      vdestN.val[1] = vhadd_u8(vdestN.val[0], vdest.val[1]);
      vdestN.val[1] = vadd_u8(vdestN.val[1], vtmp1);

      vst1_lane_u32(png_ptr(uint32_t, np), png_ldr(uint32x2_t, &vdestN.val[0]), 0);
      np += OFFSET3;
      vst1_lane_u32(png_ptr(uint32_t, np), png_ldr(uint32x2_t, &vdestN.val[1]), 0);
   } else if (tail_bytes == TAIL_RGB3) {
      vtmp1 = vext_u8(vrp.val[0], vrp.val[1], OFFSET3);
      vdest.val[0] = vhadd_u8(vdest.val[IND3], vpp.val[0]);
      vdest.val[0] = vadd_u8(vdest.val[0], vrp.val[0]);

      vtmp2 = vext_u8(vpp.val[0], vpp.val[1], OFFSET3);
      vtmp3 = vext_u8(vrp.val[0], vrp.val[1], OFFSET6);
      vdest.val[1] = vhadd_u8(vdest.val[0], vtmp2);
      vdest.val[1] = vadd_u8(vdest.val[1], vtmp1);

      vtmp2 = vext_u8(vpp.val[0], vpp.val[1], OFFSET6);

      vdest.val[IND2] = vhadd_u8(vdest.val[1], vtmp2);
      vdest.val[IND2] = vadd_u8(vdest.val[IND2], vtmp3);

      vst1_lane_u32(png_ptr(uint32_t, rp), png_ldr(uint32x2_t, &vdest.val[0]), 0);
      rp += OFFSET3;
      vst1_lane_u32(png_ptr(uint32_t, rp), png_ldr(uint32x2_t, &vdest.val[1]), 0);
      rp += OFFSET3;
      vst1_lane_u32(png_ptr(uint32_t, rp), png_ldr(uint32x2_t, &vdest.val[IND2]), 0);

      vtmp1 = vext_u8(vnp.val[0], vnp.val[1], OFFSET3);
      vdestN.val[0] = vhadd_u8(vdestN.val[IND3], vdest.val[0]);
      vdestN.val[0] = vadd_u8(vdestN.val[0], vnp.val[0]);

      vtmp3 = vext_u8(vnp.val[0], vnp.val[1], OFFSET6);
      vdestN.val[1] = vhadd_u8(vdestN.val[0], vdest.val[1]);
      vdestN.val[1] = vadd_u8(vdestN.val[1], vtmp1);

      vdestN.val[IND2] = vhadd_u8(vdestN.val[1], vdest.val[IND2]);
      vdestN.val[IND2] = vadd_u8(vdestN.val[IND2], vtmp3);

      vst1_lane_u32(png_ptr(uint32_t, np), png_ldr(uint32x2_t, &vdestN.val[0]), 0);
      np += OFFSET3;
      vst1_lane_u32(png_ptr(uint32_t, np), png_ldr(uint32x2_t, &vdestN.val[1]), 0);
      np += OFFSET3;
      vst1_lane_u32(png_ptr(uint32_t, np), png_ldr(uint32x2_t, &vdestN.val[IND2]), 0);
   }
   *rp_stop = last_byte;
   *(rp_stop + row_info->rowbytes + 1) = last_byte_next;
}

void png_read_filter_row_avg4_neon(png_row_infop row_info, png_bytep row,
   png_const_bytep prev_row)
{
   png_bytep rp = row;
   png_const_bytep pp = prev_row;
   int count = row_info->rowbytes;

   uint8x8x4_t vdest;
   vdest.val[IND3] = vdup_n_u8(0);

   png_debug(1, "in png_read_filter_row_avg4_neon");

   uint32x2x4_t vtmp;
   uint8x8x4_t *vrpt, *vppt;
   uint8x8x4_t vrp, vpp;
   uint32x2x4_t vdest_val;
   while (count >= STEP_RGBA) {
      uint32x2x4_t *temp_pointer;
      vtmp = vld4_u32(png_ptr(uint32_t, rp));
      vrpt = png_ptr(uint8x8x4_t, &vtmp);
      vrp = *vrpt;
      vtmp = vld4_u32(png_ptrc(uint32_t, pp));
      vppt = png_ptr(uint8x8x4_t, &vtmp);
      vpp = *vppt;

      vdest.val[0] = vhadd_u8(vdest.val[IND3], vpp.val[0]);
      vdest.val[0] = vadd_u8(vdest.val[0], vrp.val[0]);
      vdest.val[1] = vhadd_u8(vdest.val[0], vpp.val[1]);
      vdest.val[1] = vadd_u8(vdest.val[1], vrp.val[1]);
      vdest.val[IND2] = vhadd_u8(vdest.val[1], vpp.val[IND2]);
      vdest.val[IND2] = vadd_u8(vdest.val[IND2], vrp.val[IND2]);
      vdest.val[IND3] = vhadd_u8(vdest.val[IND2], vpp.val[IND3]);
      vdest.val[IND3] = vadd_u8(vdest.val[IND3], vrp.val[IND3]);

      vdest_val = png_ldr(uint32x2x4_t, &vdest);
      vst4_lane_u32(png_ptr(uint32_t, rp), vdest_val, 0);

      rp += STEP_RGBA;
      pp += STEP_RGBA;
      count -= STEP_RGBA;
   }

   if (count >= STEP_RGBA_HALF) {
      uint32x2x2_t vtmp1;
      uint8x8x2_t *vrpt1, *vppt1;
      uint8x8x2_t vrp1, vpp1;
      uint32x2x2_t *temp_pointer;
      uint32x2x2_t vdest_val1;

      vtmp1 = vld2_u32(png_ptr(uint32_t, rp));
      vrpt1 = png_ptr(uint8x8x2_t, &vtmp1);
      vrp1 = *vrpt1;
      vtmp1 = vld2_u32(png_ptrc(uint32_t, pp));
      vppt1 = png_ptr(uint8x8x2_t, &vtmp1);
      vpp1 = *vppt1;

      vdest.val[0] = vhadd_u8(vdest.val[IND3], vpp1.val[0]);
      vdest.val[0] = vadd_u8(vdest.val[0], vrp1.val[0]);
      vdest.val[1] = vhadd_u8(vdest.val[0], vpp1.val[1]);
      vdest.val[1] = vadd_u8(vdest.val[1], vrp1.val[1]);

      vdest_val1 = png_ldr(uint32x2x2_t, &vdest);
      vst2_lane_u32(png_ptr(uint32_t, rp), vdest_val1, 0);

      rp += STEP_RGBA_HALF;
      pp += STEP_RGBA_HALF;
      count -= STEP_RGBA_HALF;
   }

   if (count == 0) {
      return;
   }

   uint32x2_t vtmp2;
   uint8x8_t *vrpt2, *vppt2;
   uint8x8_t vrp2, vpp2;
   uint32x2_t *temp_pointer;
   uint32x2_t vdest_val2;

   vtmp2 = vld1_u32(png_ptr(uint32_t, rp));
   vrpt2 = png_ptr(uint8x8_t, &vtmp2);
   vrp2 = *vrpt2;
   vtmp2 = vld1_u32(png_ptrc(uint32_t, pp));
   vppt2 = png_ptr(uint8x8_t, &vtmp2);
   vpp2 = *vppt2;

   vdest.val[0] = vhadd_u8(vdest.val[1], vpp2);
   vdest.val[0] = vadd_u8(vdest.val[0], vrp2);

   vdest_val2 = png_ldr(uint32x2_t, &vdest);
   vst1_lane_u32(png_ptr(uint32_t, rp), vdest_val2, 0);
}

void png_read_filter_row_avg4_x2_neon(png_row_infop row_info, png_bytep row,
   png_const_bytep prev_row)
{
   png_bytep rp = row;
   png_const_bytep pp = prev_row;
   int count = row_info->rowbytes;
   png_bytep np = row + count + 1;

   uint8x8x4_t vdest;
   vdest.val[IND3] = vdup_n_u8(0);

   png_debug(1, "in png_read_filter_row_avg4_x2_neon");

   uint32x2x4_t vtmp;
   uint8x8x4_t *vrpt, *vppt;
   uint8x8x4_t vrp, vpp;
   uint32x2x4_t vdest_val;

   uint8x8x4_t *vnpt;
   uint8x8x4_t vnp;
   uint8x8x4_t vdestN;
   vdestN.val[IND3] = vdup_n_u8(0);

   while (count >= STEP_RGBA) {
      uint32x2x4_t *temp_pointer;
      vtmp = vld4_u32(png_ptr(uint32_t, rp));
      vrpt = png_ptr(uint8x8x4_t, &vtmp);
      vrp = *vrpt;
      vtmp = vld4_u32(png_ptrc(uint32_t, pp));
      vppt = png_ptr(uint8x8x4_t, &vtmp);
      vpp = *vppt;
      vtmp = vld4_u32(png_ptrc(uint32_t, np));
      vnpt = png_ptr(uint8x8x4_t, &vtmp);
      vnp = *vnpt;

      vdest.val[0] = vhadd_u8(vdest.val[IND3], vpp.val[0]);
      vdest.val[0] = vadd_u8(vdest.val[0], vrp.val[0]);
      vdest.val[1] = vhadd_u8(vdest.val[0], vpp.val[1]);
      vdest.val[1] = vadd_u8(vdest.val[1], vrp.val[1]);
      vdest.val[IND2] = vhadd_u8(vdest.val[1], vpp.val[IND2]);
      vdest.val[IND2] = vadd_u8(vdest.val[IND2], vrp.val[IND2]);
      vdest.val[IND3] = vhadd_u8(vdest.val[IND2], vpp.val[IND3]);
      vdest.val[IND3] = vadd_u8(vdest.val[IND3], vrp.val[IND3]);

      vdest_val = png_ldr(uint32x2x4_t, &vdest);
      vst4_lane_u32(png_ptr(uint32_t, rp), vdest_val, 0);

      vdestN.val[0] = vhadd_u8(vdestN.val[IND3], vdest.val[0]);
      vdestN.val[0] = vadd_u8(vdestN.val[0], vnp.val[0]);
      vdestN.val[1] = vhadd_u8(vdestN.val[0], vdest.val[1]);
      vdestN.val[1] = vadd_u8(vdestN.val[1], vnp.val[1]);
      vdestN.val[IND2] = vhadd_u8(vdestN.val[1], vdest.val[IND2]);
      vdestN.val[IND2] = vadd_u8(vdestN.val[IND2], vnp.val[IND2]);
      vdestN.val[IND3] = vhadd_u8(vdestN.val[IND2], vdest.val[IND3]);
      vdestN.val[IND3] = vadd_u8(vdestN.val[IND3], vnp.val[IND3]);

      vdest_val = png_ldr(uint32x2x4_t, &vdestN);
      vst4_lane_u32(png_ptr(uint32_t, np), vdest_val, 0);

      rp += STEP_RGBA;
      pp += STEP_RGBA;
      np += STEP_RGBA;
      count -= STEP_RGBA;
   }

   if (count >= STEP_RGBA_HALF) {
      uint32x2x2_t vtmp1;
      uint8x8x2_t *vrpt1, *vppt1, *vnpt1;
      uint8x8x2_t vrp1, vpp1, vnp1;
      uint32x2x2_t *temp_pointer;
      uint32x2x2_t vdest_val1;

      vtmp1 = vld2_u32(png_ptr(uint32_t, rp));
      vrpt1 = png_ptr(uint8x8x2_t, &vtmp1);
      vrp1 = *vrpt1;
      vtmp1 = vld2_u32(png_ptrc(uint32_t, pp));
      vppt1 = png_ptr(uint8x8x2_t, &vtmp1);
      vpp1 = *vppt1;
      vtmp1 = vld2_u32(png_ptrc(uint32_t, np));
      vnpt1 = png_ptr(uint8x8x2_t, &vtmp1);
      vnp1 = *vnpt1;

      vdest.val[0] = vhadd_u8(vdest.val[IND3], vpp1.val[0]);
      vdest.val[0] = vadd_u8(vdest.val[0], vrp1.val[0]);
      vdest.val[1] = vhadd_u8(vdest.val[0], vpp1.val[1]);
      vdest.val[1] = vadd_u8(vdest.val[1], vrp1.val[1]);

      vdest_val1 = png_ldr(uint32x2x2_t, &vdest);
      vst2_lane_u32(png_ptr(uint32_t, rp), vdest_val1, 0);

      vdestN.val[0] = vhadd_u8(vdestN.val[IND3], vdest.val[0]);
      vdestN.val[0] = vadd_u8(vdestN.val[0], vnp1.val[0]);
      vdestN.val[1] = vhadd_u8(vdestN.val[0], vdest.val[1]);
      vdestN.val[1] = vadd_u8(vdestN.val[1], vnp1.val[1]);

      vdest_val1 = png_ldr(uint32x2x2_t, &vdestN);
      vst2_lane_u32(png_ptr(uint32_t, np), vdest_val1, 0);

      rp += STEP_RGBA_HALF;
      pp += STEP_RGBA_HALF;
      np += STEP_RGBA_HALF;
      count -= STEP_RGBA_HALF;
   }

   if (count == 0) {
      return;
   }

   uint32x2_t vtmp2;
   uint8x8_t *vrpt2, *vppt2, *vnpt2;
   uint8x8_t vrp2, vpp2, vnp2;
   uint32x2_t *temp_pointer;
   uint32x2_t vdest_val2;

   vtmp2 = vld1_u32(png_ptr(uint32_t, rp));
   vrpt2 = png_ptr(uint8x8_t, &vtmp2);
   vrp2 = *vrpt2;
   vtmp2 = vld1_u32(png_ptrc(uint32_t, pp));
   vppt2 = png_ptr(uint8x8_t, &vtmp2);
   vpp2 = *vppt2;
   vtmp2 = vld1_u32(png_ptrc(uint32_t, np));
   vnpt2 = png_ptr(uint8x8_t, &vtmp2);
   vnp2 = *vnpt2;

   vdest.val[0] = vhadd_u8(vdest.val[1], vpp2);
   vdest.val[0] = vadd_u8(vdest.val[0], vrp2);

   vdest_val2 = png_ldr(uint32x2_t, &vdest);
   vst1_lane_u32(png_ptr(uint32_t, rp), vdest_val2, 0);

   vdestN.val[0] = vhadd_u8(vdestN.val[1], vdest.val[0]);
   vdestN.val[0] = vadd_u8(vdestN.val[0], vnp2);

   vdest_val2 = png_ldr(uint32x2_t, &vdestN);
   vst1_lane_u32(png_ptr(uint32_t, np), vdest_val2, 0);
}

static uint8x8_t paeth(uint8x8_t a, uint8x8_t b, uint8x8_t c)
{
   uint8x8_t d, e;
   uint16x8_t p1, pa, pb, pc;

   p1 = vaddl_u8(a, b); /* a + b */
   pc = vaddl_u8(c, c); /* c * 2 */
   pa = vabdl_u8(b, c); /* pa */
   pb = vabdl_u8(a, c); /* pb */
   pc = vabdq_u16(p1, pc); /* pc */

   p1 = vcleq_u16(pa, pb); /* pa <= pb */
   pa = vcleq_u16(pa, pc); /* pa <= pc */
   pb = vcleq_u16(pb, pc); /* pb <= pc */

   p1 = vandq_u16(p1, pa); /* pa <= pb && pa <= pc */

   d = vmovn_u16(pb);
   e = vmovn_u16(p1);

   d = vbsl_u8(d, b, c);
   e = vbsl_u8(e, a, d);

   return e;
}

void png_read_filter_row_paeth3_neon(png_row_infop row_info, png_bytep row,
   png_const_bytep prev_row)
{
   png_bytep rp = row;
   png_const_bytep pp = prev_row;
   png_bytep rp_stop = row + row_info->rowbytes;

   uint8x16_t vtmp;
   uint8x8x2_t *vrpt;
   uint8x8x2_t vrp;
   uint8x8_t vlast = vdup_n_u8(0);
   uint8x8x4_t vdest;
   vdest.val[IND3] = vdup_n_u8(0);

   vtmp = vld1q_u8(rp);
   vrpt = png_ptr(uint8x8x2_t, &vtmp);
   vrp = *vrpt;

   uint8x8x2_t *vppt;
   uint8x8x2_t vpp;
   uint8x8_t vtmp1, vtmp2, vtmp3;
   uint32x2_t *temp_pointer;

   png_debug(1, "in png_read_filter_row_paeth3_neon");

   size_t tail_bytes = row_info->rowbytes % STEP_RGB;
   png_byte last_byte = *rp_stop;
   png_bytep rp_stop_new = rp_stop - tail_bytes;
   for (; rp < rp_stop_new; pp += STEP_RGB)
   {
      vtmp = vld1q_u8(pp);
      vppt = png_ptr(uint8x8x2_t, &vtmp);
      vpp = *vppt;

      vdest.val[0] = paeth(vdest.val[IND3], vpp.val[0], vlast);
      vdest.val[0] = vadd_u8(vdest.val[0], vrp.val[0]);

      vtmp1 = vext_u8(vrp.val[0], vrp.val[1], OFFSET3);
      vtmp2 = vext_u8(vpp.val[0], vpp.val[1], OFFSET3);
      vdest.val[1] = paeth(vdest.val[0], vtmp2, vpp.val[0]);
      vdest.val[1] = vadd_u8(vdest.val[1], vtmp1);

      vtmp1 = vext_u8(vrp.val[0], vrp.val[1], OFFSET6);
      vtmp3 = vext_u8(vpp.val[0], vpp.val[1], OFFSET6);
      vdest.val[IND2] = paeth(vdest.val[1], vtmp3, vtmp2);
      vdest.val[IND2] = vadd_u8(vdest.val[IND2], vtmp1);

      vtmp1 = vext_u8(vrp.val[1], vrp.val[1], 1);
      vtmp2 = vext_u8(vpp.val[1], vpp.val[1], 1);

      vtmp = vld1q_u8(rp + STEP_RGB);
      vrpt = png_ptr(uint8x8x2_t, &vtmp);
      vrp = *vrpt;

      vdest.val[IND3] = paeth(vdest.val[IND2], vtmp2, vtmp3);
      vdest.val[IND3] = vadd_u8(vdest.val[IND3], vtmp1);

      vlast = vtmp2;

      vst1_lane_u32(png_ptr(uint32_t, rp), png_ldr(uint32x2_t, &vdest.val[0]), 0);
      rp += OFFSET3;
      vst1_lane_u32(png_ptr(uint32_t, rp), png_ldr(uint32x2_t, &vdest.val[1]), 0);
      rp += OFFSET3;
      vst1_lane_u32(png_ptr(uint32_t, rp), png_ldr(uint32x2_t, &vdest.val[IND2]), 0);
      rp += OFFSET3;
      vst1_lane_u32(png_ptr(uint32_t, rp), png_ldr(uint32x2_t, &vdest.val[IND3]), 0);
      rp += OFFSET3;
   }

   vtmp = vld1q_u8(pp);
   vppt = png_ptr(uint8x8x2_t, &vtmp);
   vpp = *vppt;

   if (tail_bytes == TAIL_RGB1) {
      vdest.val[0] = paeth(vdest.val[IND3], vpp.val[0], vlast);
      vdest.val[0] = vadd_u8(vdest.val[0], vrp.val[0]);
      vst1_lane_u32(png_ptr(uint32_t, rp), png_ldr(uint32x2_t, &vdest.val[0]), 0);
   } else if (tail_bytes == TAIL_RGB2) {
      vdest.val[0] = paeth(vdest.val[IND3], vpp.val[0], vlast);
      vdest.val[0] = vadd_u8(vdest.val[0], vrp.val[0]);

      vtmp1 = vext_u8(vrp.val[0], vrp.val[1], OFFSET3);
      vtmp2 = vext_u8(vpp.val[0], vpp.val[1], OFFSET3);
      vdest.val[1] = paeth(vdest.val[0], vtmp2, vpp.val[0]);
      vdest.val[1] = vadd_u8(vdest.val[1], vtmp1);

      vst1_lane_u32(png_ptr(uint32_t, rp), png_ldr(uint32x2_t, &vdest.val[0]), 0);
      rp += OFFSET3;
      vst1_lane_u32(png_ptr(uint32_t, rp), png_ldr(uint32x2_t, &vdest.val[1]), 0);
   } else if (tail_bytes == TAIL_RGB3) {
      vdest.val[0] = paeth(vdest.val[IND3], vpp.val[0], vlast);
      vdest.val[0] = vadd_u8(vdest.val[0], vrp.val[0]);

      vtmp1 = vext_u8(vrp.val[0], vrp.val[1], OFFSET3);
      vtmp2 = vext_u8(vpp.val[0], vpp.val[1], OFFSET3);
      vdest.val[1] = paeth(vdest.val[0], vtmp2, vpp.val[0]);
      vdest.val[1] = vadd_u8(vdest.val[1], vtmp1);

      vtmp1 = vext_u8(vrp.val[0], vrp.val[1], OFFSET6);
      vtmp3 = vext_u8(vpp.val[0], vpp.val[1], OFFSET6);
      vdest.val[IND2] = paeth(vdest.val[1], vtmp3, vtmp2);
      vdest.val[IND2] = vadd_u8(vdest.val[IND2], vtmp1);

      vst1_lane_u32(png_ptr(uint32_t, rp), png_ldr(uint32x2_t, &vdest.val[0]), 0);
      rp += OFFSET3;
      vst1_lane_u32(png_ptr(uint32_t, rp), png_ldr(uint32x2_t, &vdest.val[1]), 0);
      rp += OFFSET3;
      vst1_lane_u32(png_ptr(uint32_t, rp), png_ldr(uint32x2_t, &vdest.val[IND2]), 0);
   }
   *rp_stop = last_byte;
}

void png_read_filter_row_paeth3_x2_neon(png_row_infop row_info, png_bytep row,
   png_const_bytep prev_row)
{
   png_bytep rp = row;
   png_const_bytep pp = prev_row;
   png_bytep rp_stop = row + row_info->rowbytes;
   png_bytep np = rp_stop + 1;

   uint8x16_t vtmp;
   uint8x8x2_t *vrpt;
   uint8x8x2_t vrp;
   uint8x8_t vlast = vdup_n_u8(0);
   uint8x8x4_t vdest;
   vdest.val[IND3] = vdup_n_u8(0);

   vtmp = vld1q_u8(rp);
   vrpt = png_ptr(uint8x8x2_t, &vtmp);
   vrp = *vrpt;

   uint8x8x2_t *vppt;
   uint8x8x2_t vpp;
   uint8x8_t vtmp1, vtmp2, vtmp3;
   uint32x2_t *temp_pointer;

   uint8x8x2_t *vnpt;
   uint8x8x2_t vnp;
   uint8x8_t vlastN = vdup_n_u8(0);
   uint8x8x4_t vdestN;
   vdestN.val[IND3] = vdup_n_u8(0);

   vtmp = vld1q_u8(np);
   vnpt = png_ptr(uint8x8x2_t, &vtmp);
   vnp = *vnpt;

   png_debug(1, "in png_read_filter_row_paeth3_x2_neon");

   size_t tail_bytes = row_info->rowbytes % STEP_RGB;
   png_byte last_byte = *rp_stop;
   png_byte last_byte_next = *(rp_stop + row_info->rowbytes + 1);
   png_bytep rp_stop_new = rp_stop - tail_bytes;

   for (; rp < rp_stop_new; pp += STEP_RGB)
   {
      vtmp = vld1q_u8(pp);
      vppt = png_ptr(uint8x8x2_t, &vtmp);
      vpp = *vppt;

      vdest.val[0] = paeth(vdest.val[IND3], vpp.val[0], vlast);
      vdest.val[0] = vadd_u8(vdest.val[0], vrp.val[0]);

      vtmp1 = vext_u8(vrp.val[0], vrp.val[1], OFFSET3);
      vtmp2 = vext_u8(vpp.val[0], vpp.val[1], OFFSET3);
      vdest.val[1] = paeth(vdest.val[0], vtmp2, vpp.val[0]);
      vdest.val[1] = vadd_u8(vdest.val[1], vtmp1);

      vtmp1 = vext_u8(vrp.val[0], vrp.val[1], OFFSET6);
      vtmp3 = vext_u8(vpp.val[0], vpp.val[1], OFFSET6);
      vdest.val[IND2] = paeth(vdest.val[1], vtmp3, vtmp2);
      vdest.val[IND2] = vadd_u8(vdest.val[IND2], vtmp1);

      vtmp1 = vext_u8(vrp.val[1], vrp.val[1], 1);
      vtmp2 = vext_u8(vpp.val[1], vpp.val[1], 1);

      vtmp = vld1q_u8(rp + STEP_RGB);
      vrpt = png_ptr(uint8x8x2_t, &vtmp);
      vrp = *vrpt;

      vdest.val[IND3] = paeth(vdest.val[IND2], vtmp2, vtmp3);
      vdest.val[IND3] = vadd_u8(vdest.val[IND3], vtmp1);

      vlast = vtmp2;

      vst1_lane_u32(png_ptr(uint32_t, rp), png_ldr(uint32x2_t, &vdest.val[0]), 0);
      rp += OFFSET3;
      vst1_lane_u32(png_ptr(uint32_t, rp), png_ldr(uint32x2_t, &vdest.val[1]), 0);
      rp += OFFSET3;
      vst1_lane_u32(png_ptr(uint32_t, rp), png_ldr(uint32x2_t, &vdest.val[IND2]), 0);
      rp += OFFSET3;
      vst1_lane_u32(png_ptr(uint32_t, rp), png_ldr(uint32x2_t, &vdest.val[IND3]), 0);
      rp += OFFSET3;

      vdestN.val[0] = paeth(vdestN.val[IND3], vdest.val[0], vlastN);
      vdestN.val[0] = vadd_u8(vdestN.val[0], vnp.val[0]);

      vtmp1 = vext_u8(vnp.val[0], vnp.val[1], OFFSET3);
      vdestN.val[1] = paeth(vdestN.val[0], vdest.val[1], vdest.val[0]);
      vdestN.val[1] = vadd_u8(vdestN.val[1], vtmp1);

      vtmp1 = vext_u8(vnp.val[0], vnp.val[1], OFFSET6);
      vdestN.val[IND2] = paeth(vdestN.val[1], vdest.val[IND2], vdest.val[1]);
      vdestN.val[IND2] = vadd_u8(vdestN.val[IND2], vtmp1);

      vtmp1 = vext_u8(vnp.val[1], vnp.val[1], 1);

      vtmp = vld1q_u8(np + STEP_RGB);
      vnpt = png_ptr(uint8x8x2_t, &vtmp);
      vnp = *vnpt;

      vdestN.val[IND3] = paeth(vdestN.val[IND2], vdest.val[IND3], vdest.val[IND2]);
      vdestN.val[IND3] = vadd_u8(vdestN.val[IND3], vtmp1);

      vlastN = vdest.val[IND3];

      vst1_lane_u32(png_ptr(uint32_t, np), png_ldr(uint32x2_t, &vdestN.val[0]), 0);
      np += OFFSET3;
      vst1_lane_u32(png_ptr(uint32_t, np), png_ldr(uint32x2_t, &vdestN.val[1]), 0);
      np += OFFSET3;
      vst1_lane_u32(png_ptr(uint32_t, np), png_ldr(uint32x2_t, &vdestN.val[IND2]), 0);
      np += OFFSET3;
      vst1_lane_u32(png_ptr(uint32_t, np), png_ldr(uint32x2_t, &vdestN.val[IND3]), 0);
      np += OFFSET3;
   }

   vtmp = vld1q_u8(pp);
   vppt = png_ptr(uint8x8x2_t, &vtmp);
   vpp = *vppt;

   if (tail_bytes == TAIL_RGB1) {
      vdest.val[0] = paeth(vdest.val[IND3], vpp.val[0], vlast);
      vdest.val[0] = vadd_u8(vdest.val[0], vrp.val[0]);
      vst1_lane_u32(png_ptr(uint32_t, rp), png_ldr(uint32x2_t, &vdest.val[0]), 0);

      vdestN.val[0] = paeth(vdestN.val[IND3], vdest.val[0], vlastN);
      vdestN.val[0] = vadd_u8(vdestN.val[0], vnp.val[0]);
      vst1_lane_u32(png_ptr(uint32_t, np), png_ldr(uint32x2_t, &vdestN.val[0]), 0);
   } else if (tail_bytes == TAIL_RGB2) {
      vdest.val[0] = paeth(vdest.val[IND3], vpp.val[0], vlast);
      vdest.val[0] = vadd_u8(vdest.val[0], vrp.val[0]);

      vtmp1 = vext_u8(vrp.val[0], vrp.val[1], OFFSET3);
      vtmp2 = vext_u8(vpp.val[0], vpp.val[1], OFFSET3);
      vdest.val[1] = paeth(vdest.val[0], vtmp2, vpp.val[0]);
      vdest.val[1] = vadd_u8(vdest.val[1], vtmp1);

      vst1_lane_u32(png_ptr(uint32_t, rp), png_ldr(uint32x2_t, &vdest.val[0]), 0);
      rp += OFFSET3;
      vst1_lane_u32(png_ptr(uint32_t, rp), png_ldr(uint32x2_t, &vdest.val[1]), 0);

      vdestN.val[0] = paeth(vdestN.val[IND3], vdest.val[0], vlastN);
      vdestN.val[0] = vadd_u8(vdestN.val[0], vnp.val[0]);

      vtmp1 = vext_u8(vnp.val[0], vnp.val[1], OFFSET3);
      vdestN.val[1] = paeth(vdestN.val[0], vdest.val[1], vdest.val[0]);
      vdestN.val[1] = vadd_u8(vdestN.val[1], vtmp1);

      vst1_lane_u32(png_ptr(uint32_t, np), png_ldr(uint32x2_t, &vdestN.val[0]), 0);
      np += OFFSET3;
      vst1_lane_u32(png_ptr(uint32_t, np), png_ldr(uint32x2_t, &vdestN.val[1]), 0);
   } else if (tail_bytes == TAIL_RGB3) {
      vdest.val[0] = paeth(vdest.val[IND3], vpp.val[0], vlast);
      vdest.val[0] = vadd_u8(vdest.val[0], vrp.val[0]);

      vtmp1 = vext_u8(vrp.val[0], vrp.val[1], OFFSET3);
      vtmp2 = vext_u8(vpp.val[0], vpp.val[1], OFFSET3);
      vdest.val[1] = paeth(vdest.val[0], vtmp2, vpp.val[0]);
      vdest.val[1] = vadd_u8(vdest.val[1], vtmp1);

      vtmp1 = vext_u8(vrp.val[0], vrp.val[1], OFFSET6);
      vtmp3 = vext_u8(vpp.val[0], vpp.val[1], OFFSET6);
      vdest.val[IND2] = paeth(vdest.val[1], vtmp3, vtmp2);
      vdest.val[IND2] = vadd_u8(vdest.val[IND2], vtmp1);

      vst1_lane_u32(png_ptr(uint32_t, rp), png_ldr(uint32x2_t, &vdest.val[0]), 0);
      rp += OFFSET3;
      vst1_lane_u32(png_ptr(uint32_t, rp), png_ldr(uint32x2_t, &vdest.val[1]), 0);
      rp += OFFSET3;
      vst1_lane_u32(png_ptr(uint32_t, rp), png_ldr(uint32x2_t, &vdest.val[IND2]), 0);

      vdestN.val[0] = paeth(vdestN.val[IND3], vdest.val[0], vlastN);
      vdestN.val[0] = vadd_u8(vdestN.val[0], vnp.val[0]);

      vtmp1 = vext_u8(vnp.val[0], vnp.val[1], OFFSET3);
      vdestN.val[1] = paeth(vdestN.val[0], vdest.val[1], vdest.val[0]);
      vdestN.val[1] = vadd_u8(vdestN.val[1], vtmp1);

      vtmp1 = vext_u8(vnp.val[0], vnp.val[1], OFFSET6);
      vdestN.val[IND2] = paeth(vdestN.val[1], vdest.val[IND2], vdest.val[1]);
      vdestN.val[IND2] = vadd_u8(vdestN.val[IND2], vtmp1);

      vst1_lane_u32(png_ptr(uint32_t, np), png_ldr(uint32x2_t, &vdestN.val[0]), 0);
      np += OFFSET3;
      vst1_lane_u32(png_ptr(uint32_t, np), png_ldr(uint32x2_t, &vdestN.val[1]), 0);
      np += OFFSET3;
      vst1_lane_u32(png_ptr(uint32_t, np), png_ldr(uint32x2_t, &vdestN.val[IND2]), 0);
   }
   *rp_stop = last_byte;
   *(rp_stop + row_info->rowbytes + 1) = last_byte_next;
}

void png_read_filter_row_paeth4_neon(png_row_infop row_info, png_bytep row,
   png_const_bytep prev_row)
{
   png_bytep rp = row;
   int count = row_info->rowbytes;
   png_const_bytep pp = prev_row;

   uint8x8_t vlast = vdup_n_u8(0);
   uint8x8x4_t vdest;
   vdest.val[IND3] = vdup_n_u8(0);

   png_debug(1, "in png_read_filter_row_paeth4_neon");

   uint32x2x4_t vtmp;
   uint8x8x4_t *vrpt, *vppt;
   uint8x8x4_t vrp, vpp;
   uint32x2x4_t vdest_val;
   while (count >= STEP_RGBA) {
      uint32x2x4_t *temp_pointer;
      vtmp = vld4_u32(png_ptr(uint32_t, rp));
      vrpt = png_ptr(uint8x8x4_t, &vtmp);
      vrp = *vrpt;
      vtmp = vld4_u32(png_ptrc(uint32_t, pp));
      vppt = png_ptr(uint8x8x4_t, &vtmp);
      vpp = *vppt;

      vdest.val[0] = paeth(vdest.val[IND3], vpp.val[0], vlast);
      vdest.val[0] = vadd_u8(vdest.val[0], vrp.val[0]);
      vdest.val[1] = paeth(vdest.val[0], vpp.val[1], vpp.val[0]);
      vdest.val[1] = vadd_u8(vdest.val[1], vrp.val[1]);
      vdest.val[IND2] = paeth(vdest.val[1], vpp.val[IND2], vpp.val[1]);
      vdest.val[IND2] = vadd_u8(vdest.val[IND2], vrp.val[IND2]);
      vdest.val[IND3] = paeth(vdest.val[IND2], vpp.val[IND3], vpp.val[IND2]);
      vdest.val[IND3] = vadd_u8(vdest.val[IND3], vrp.val[IND3]);

      vlast = vpp.val[IND3];

      vdest_val = png_ldr(uint32x2x4_t, &vdest);
      vst4_lane_u32(png_ptr(uint32_t, rp), vdest_val, 0);

      rp += STEP_RGBA;
      pp += STEP_RGBA;
      count -= STEP_RGBA;
   }

   if (count >= STEP_RGBA_HALF) {
      uint32x2x2_t vtmp1;
      uint8x8x2_t *vrpt1, *vppt1;
      uint8x8x2_t vrp1, vpp1;
      uint32x2x2_t *temp_pointer;
      uint32x2x2_t vdest_val1;

      vtmp1 = vld2_u32(png_ptr(uint32_t, rp));
      vrpt1 = png_ptr(uint8x8x2_t, &vtmp1);
      vrp1 = *vrpt1;
      vtmp1 = vld2_u32(png_ptrc(uint32_t, pp));
      vppt1 = png_ptr(uint8x8x2_t, &vtmp1);
      vpp1 = *vppt1;

      vdest.val[0] = paeth(vdest.val[IND3], vpp1.val[0], vlast);
      vdest.val[0] = vadd_u8(vdest.val[0], vrp1.val[0]);
      vdest.val[1] = paeth(vdest.val[0], vpp1.val[1], vpp1.val[0]);
      vdest.val[1] = vadd_u8(vdest.val[1], vrp1.val[1]);
      vlast = vpp1.val[1];

      vdest_val1 = png_ldr(uint32x2x2_t, &vdest);
      vst2_lane_u32(png_ptr(uint32_t, rp), vdest_val1, 0);
      vdest.val[IND3] = vdest.val[1];

      rp += STEP_RGBA_HALF;
      pp += STEP_RGBA_HALF;
      count -= STEP_RGBA_HALF;
   }

   if (count == 0) {
      return;
   }

   uint32x2_t vtmp2;
   uint8x8_t *vrpt2, *vppt2;
   uint8x8_t vrp2, vpp2;
   uint32x2_t *temp_pointer;
   uint32x2_t vdest_val2;

   vtmp2 = vld1_u32(png_ptr(uint32_t, rp));
   vrpt2 = png_ptr(uint8x8_t, &vtmp2);
   vrp2 = *vrpt2;
   vtmp2 = vld1_u32(png_ptrc(uint32_t, pp));
   vppt2 = png_ptr(uint8x8_t, &vtmp2);
   vpp2 = *vppt2;

   vdest.val[0] = paeth(vdest.val[IND3], vpp2, vlast);
   vdest.val[0] = vadd_u8(vdest.val[0], vrp2);

   vdest_val2 = png_ldr(uint32x2_t, &vdest);
   vst1_lane_u32(png_ptr(uint32_t, rp), vdest_val2, 0);
}

void png_read_filter_row_paeth4_x2_neon(png_row_infop row_info, png_bytep row,
   png_const_bytep prev_row)
{
   png_bytep rp = row;
   int count = row_info->rowbytes;
   png_const_bytep pp = prev_row;
   png_bytep np = row + row_info->rowbytes + 1;

   uint8x8_t vlast = vdup_n_u8(0);
   uint8x8x4_t vdest;
   vdest.val[IND3] = vdup_n_u8(0);

   png_debug(1, "in png_read_filter_row_paeth4_x2_neon");

   uint32x2x4_t vtmp;
   uint8x8x4_t *vrpt, *vppt;
   uint8x8x4_t vrp, vpp;
   uint32x2x4_t vdest_val;

   uint8x8x4_t *vnpt;
   uint8x8x4_t vnp;
   uint8x8_t vlastN = vdup_n_u8(0);
   uint8x8x4_t vdestN;
   vdestN.val[IND3] = vdup_n_u8(0);

   while (count >= STEP_RGBA) {
      uint32x2x4_t *temp_pointer;
      vtmp = vld4_u32(png_ptr(uint32_t, rp));
      vrpt = png_ptr(uint8x8x4_t, &vtmp);
      vrp = *vrpt;
      vtmp = vld4_u32(png_ptrc(uint32_t, pp));
      vppt = png_ptr(uint8x8x4_t, &vtmp);
      vpp = *vppt;
      vtmp = vld4_u32(png_ptrc(uint32_t, np));
      vnpt = png_ptr(uint8x8x4_t, &vtmp);
      vnp = *vnpt;

      vdest.val[0] = paeth(vdest.val[IND3], vpp.val[0], vlast);
      vdest.val[0] = vadd_u8(vdest.val[0], vrp.val[0]);
      vdest.val[1] = paeth(vdest.val[0], vpp.val[1], vpp.val[0]);
      vdest.val[1] = vadd_u8(vdest.val[1], vrp.val[1]);
      vdest.val[IND2] = paeth(vdest.val[1], vpp.val[IND2], vpp.val[1]);
      vdest.val[IND2] = vadd_u8(vdest.val[IND2], vrp.val[IND2]);
      vdest.val[IND3] = paeth(vdest.val[IND2], vpp.val[IND3], vpp.val[IND2]);
      vdest.val[IND3] = vadd_u8(vdest.val[IND3], vrp.val[IND3]);

      vlast = vpp.val[IND3];

      vdest_val = png_ldr(uint32x2x4_t, &vdest);
      vst4_lane_u32(png_ptr(uint32_t, rp), vdest_val, 0);

      vdestN.val[0] = paeth(vdestN.val[IND3], vdest.val[0], vlastN);
      vdestN.val[0] = vadd_u8(vdestN.val[0], vnp.val[0]);
      vdestN.val[1] = paeth(vdestN.val[0], vdest.val[1], vdest.val[0]);
      vdestN.val[1] = vadd_u8(vdestN.val[1], vnp.val[1]);
      vdestN.val[IND2] = paeth(vdestN.val[1], vdest.val[IND2], vdest.val[1]);
      vdestN.val[IND2] = vadd_u8(vdestN.val[IND2], vnp.val[IND2]);
      vdestN.val[IND3] = paeth(vdestN.val[IND2], vdest.val[IND3], vdest.val[IND2]);
      vdestN.val[IND3] = vadd_u8(vdestN.val[IND3], vnp.val[IND3]);

      vlastN = vdest.val[IND3];

      vdest_val = png_ldr(uint32x2x4_t, &vdestN);
      vst4_lane_u32(png_ptr(uint32_t, np), vdest_val, 0);

      rp += STEP_RGBA;
      pp += STEP_RGBA;
      np += STEP_RGBA;
      count -= STEP_RGBA;
   }

   if (count >= STEP_RGBA_HALF) {
      uint32x2x2_t vtmp1;
      uint8x8x2_t *vrpt1, *vppt1, *vnpt1;
      uint8x8x2_t vrp1, vpp1, vnp1;
      uint32x2x2_t *temp_pointer;
      uint32x2x2_t vdest_val1;

      vtmp1 = vld2_u32(png_ptr(uint32_t, rp));
      vrpt1 = png_ptr(uint8x8x2_t, &vtmp1);
      vrp1 = *vrpt1;
      vtmp1 = vld2_u32(png_ptrc(uint32_t, pp));
      vppt1 = png_ptr(uint8x8x2_t, &vtmp1);
      vpp1 = *vppt1;
      vtmp1 = vld2_u32(png_ptrc(uint32_t, np));
      vnpt1 = png_ptr(uint8x8x2_t, &vtmp1);
      vnp1 = *vnpt1;

      vdest.val[0] = paeth(vdest.val[IND3], vpp1.val[0], vlast);
      vdest.val[0] = vadd_u8(vdest.val[0], vrp1.val[0]);
      vdest.val[1] = paeth(vdest.val[0], vpp1.val[1], vpp1.val[0]);
      vdest.val[1] = vadd_u8(vdest.val[1], vrp1.val[1]);

      vlast = vpp1.val[1];

      vdest_val1 = png_ldr(uint32x2x2_t, &vdest);
      vst2_lane_u32(png_ptr(uint32_t, rp), vdest_val1, 0);

      vdest.val[IND3] = vdest.val[1];

      vdestN.val[0] = paeth(vdestN.val[IND3], vdest.val[0], vlastN);
      vdestN.val[0] = vadd_u8(vdestN.val[0], vnp1.val[0]);
      vdestN.val[1] = paeth(vdestN.val[0], vdest.val[1], vdest.val[0]);
      vdestN.val[1] = vadd_u8(vdestN.val[1], vnp1.val[1]);

      vlastN = vdest.val[1];

      vdest_val1 = png_ldr(uint32x2x2_t, &vdestN);
      vst2_lane_u32(png_ptr(uint32_t, np), vdest_val1, 0);

      vdestN.val[IND3] = vdestN.val[1];

      rp += STEP_RGBA_HALF;
      pp += STEP_RGBA_HALF;
      np += STEP_RGBA_HALF;
      count -= STEP_RGBA_HALF;
   }

   if (count == 0) {
      return;
   }

   uint32x2_t vtmp2;
   uint8x8_t *vrpt2, *vppt2, *vnpt2;
   uint8x8_t vrp2, vpp2, vnp2;
   uint32x2_t *temp_pointer;
   uint32x2_t vdest_val2;

   vtmp2 = vld1_u32(png_ptr(uint32_t, rp));
   vrpt2 = png_ptr(uint8x8_t, &vtmp2);
   vrp2 = *vrpt2;
   vtmp2 = vld1_u32(png_ptrc(uint32_t, pp));
   vppt2 = png_ptr(uint8x8_t, &vtmp2);
   vpp2 = *vppt2;
   vtmp2 = vld1_u32(png_ptrc(uint32_t, np));
   vnpt2 = png_ptr(uint8x8_t, &vtmp2);
   vnp2 = *vnpt2;

   vdest.val[0] = paeth(vdest.val[IND3], vpp2, vlast);
   vdest.val[0] = vadd_u8(vdest.val[0], vrp2);

   vdest_val2 = png_ldr(uint32x2_t, &vdest);
   vst1_lane_u32(png_ptr(uint32_t, rp), vdest_val2, 0);

   vdestN.val[0] = paeth(vdestN.val[IND3], vdest.val[0], vlastN);
   vdestN.val[0] = vadd_u8(vdestN.val[0], vnp2);

   vdest_val2 = png_ldr(uint32x2_t, &vdestN);
   vst1_lane_u32(png_ptr(uint32_t, np), vdest_val2, 0);
}
#endif /* PNG_MULTY_LINE_ENABLE */
#endif
#endif /* PNG_ARM_NEON_IMPLEMENTATION == 1 (intrinsics) */
#endif /* READ */
