/*  
	Copyright (C) 2006-2007 shash
	Copyright (C) 2007-2018 DeSmuME team

	This file is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 2 of the License, or
	(at your option) any later version.

	This file is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with the this software.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef MATRIX_H
#define MATRIX_H

#include <math.h>
#include <string.h>

#include "types.h"
#include "mem.h"

#ifdef ENABLE_SSE
#include <xmmintrin.h>
#endif

#ifdef ENABLE_SSE2
#include <emmintrin.h>
#endif

struct MatrixStack
{
	MatrixStack(int size, int type);
	s32		*matrix;
	u32		position;
	s32		size;
	u8		type;
};

void	MatrixInit				(float *matrix);
void	MatrixInit				(s32 *matrix);

//In order to conditionally use these asm optimized functions in visual studio
//without having to make new build types to exclude the assembly files.
//a bit sloppy, but there aint much to it

float	MatrixGetMultipliedIndex	(const u32 index, float *matrix, float *rightMatrix);
s32	MatrixGetMultipliedIndex	(const u32 index, s32 *matrix, s32 *rightMatrix);
void	MatrixSet				(s32 *matrix, int x, int y, s32 value);
void	MatrixCopy				(s32 * matrixDST, const s32 * matrixSRC);
int		MatrixCompare				(const s32 * matrixDST, const float * matrixSRC);
void	MatrixIdentity			(s32 *matrix);

void	MatrixStackInit				(MatrixStack *stack);
void	MatrixStackSetMaxSize		(MatrixStack *stack, int size);
s32*	MatrixStackGetPos			(MatrixStack *stack, const size_t pos);
s32*	MatrixStackGet				(MatrixStack *stack);
void	MatrixStackLoadMatrix		(MatrixStack *stack, const size_t pos, const s32 *ptr);

void Vector2Copy(float *dst, const float *src);
void Vector2Add(float *dst, const float *src);
void Vector2Subtract(float *dst, const float *src);
float Vector2Dot(const float *a, const float *b);
float Vector2Cross(const float *a, const float *b);

float Vector3Dot(const float *a, const float *b);
void Vector3Cross(float* dst, const float *a, const float *b);
float Vector3Length(const float *a);
void Vector3Add(float *dst, const float *src);
void Vector3Subtract(float *dst, const float *src);
void Vector3Scale(float *dst, const float scale);
void Vector3Copy(float *dst, const float *src);
void Vector3Normalize(float *dst);

void Vector4Copy(float *dst, const float *src);


void _MatrixMultVec4x4_NoSIMD(const s32 *__restrict mtxPtr, float *__restrict vecPtr);

void MatrixMultVec4x4(const s32 *__restrict mtxPtr, float *__restrict vecPtr);
void MatrixMultVec3x3(const s32 *__restrict mtxPtr, float *__restrict vecPtr);
void MatrixTranslate(float *__restrict mtxPtr, const float *__restrict vecPtr);
void MatrixScale(float *__restrict mtxPtr, const float *__restrict vecPtr);
void MatrixMultiply(float *__restrict mtxPtrA, const s32 *__restrict mtxPtrB);

template<size_t NUM_ROWS> FORCEINLINE void vector_fix2float(float *mtxPtr, const float divisor);

void MatrixMultVec4x4(const s32 *__restrict mtxPtr, s32 *__restrict vecPtr);
void MatrixMultVec3x3(const s32 *__restrict mtxPtr, s32 *__restrict vecPtr);
void MatrixTranslate(s32 *__restrict mtxPtr, const s32 *__restrict vecPtr);
void MatrixScale(s32 *__restrict mtxPtr, const s32 *__restrict vecPtr);
void MatrixMultiply(s32 *__restrict mtxPtrA, const s32 *__restrict mtxPtrB);

//these functions are an unreliable, inaccurate floor.
//it should only be used for positive numbers
//this isnt as fast as it could be if we used a visual c++ intrinsic, but those appear not to be universally available
FORCEINLINE u32 u32floor(float f)
{
#ifdef ENABLE_SSE2
	return (u32)_mm_cvtt_ss2si(_mm_set_ss(f));
#else
	return (u32)f;
#endif
}
FORCEINLINE u32 u32floor(double d)
{
#ifdef ENABLE_SSE2
	return (u32)_mm_cvttsd_si32(_mm_set_sd(d));
#else
	return (u32)d;
#endif
}

//same as above but works for negative values too.
//be sure that the results are the same thing as floorf!
FORCEINLINE s32 s32floor(float f)
{
#ifdef ENABLE_SSE2
	return _mm_cvtss_si32( _mm_add_ss(_mm_set_ss(-0.5f),_mm_add_ss(_mm_set_ss(f), _mm_set_ss(f))) ) >> 1;
#else
	return (s32)floorf(f);
#endif
}
FORCEINLINE s32 s32floor(double d)
{
	return s32floor((float)d);
}

// SIMD Functions
//-------------
#if defined(ENABLE_AVX2)

static void memset_u16(void *dst, const u16 val, const size_t elementCount)
{
	v256u16 *dst_vec256 = (v256u16 *)dst;
	const size_t length_vec256 = elementCount / (sizeof(v256u16) / sizeof(u16));
	
	const v256u16 val_vec256 = _mm256_set1_epi16(val);
	for (size_t i = 0; i < length_vec256; i++)
		_mm256_stream_si256(dst_vec256 + i, val_vec256);
}

template <size_t ELEMENTCOUNT>
static void memset_u16_fast(void *dst, const u16 val)
{
	v256u16 *dst_vec256 = (v256u16 *)dst;
	
	const v256u16 val_vec256 = _mm256_set1_epi16(val);
	MACRODO_N(ELEMENTCOUNT / (sizeof(v256u16) / sizeof(u16)), _mm256_store_si256(dst_vec256 + (X), val_vec256));
}

static void memset_u32(void *dst, const u32 val, const size_t elementCount)
{
	v256u32 *dst_vec256 = (v256u32 *)dst;
	const size_t length_vec256 = elementCount / (sizeof(v256u32) / sizeof(u32));
	
	const v256u32 val_vec256 = _mm256_set1_epi32(val);
	for (size_t i = 0; i < length_vec256; i++)
		_mm256_stream_si256(dst_vec256 + i, val_vec256);
}

template <size_t ELEMENTCOUNT>
static void memset_u32_fast(void *dst, const u32 val)
{
	v256u32 *dst_vec256 = (v256u32 *)dst;
	
	const v256u32 val_vec256 = _mm256_set1_epi32(val);
	MACRODO_N(ELEMENTCOUNT / (sizeof(v256u32) / sizeof(u32)), _mm256_store_si256(dst_vec256 + (X), val_vec256));
}

#elif defined(ENABLE_SSE2)

static void memset_u16(void *dst, const u16 val, const size_t elementCount)
{
	v128u16 *dst_vec128 = (v128u16 *)dst;
	const size_t length_vec128 = elementCount / (sizeof(v128u16) / sizeof(u16));
	
	const v128u16 val_vec128 = _mm_set1_epi16(val);
	for (size_t i = 0; i < length_vec128; i++)
		_mm_stream_si128(dst_vec128 + i, val_vec128);
}

template <size_t ELEMENTCOUNT>
static void memset_u16_fast(void *dst, const u16 val)
{
	v128u16 *dst_vec128 = (v128u16 *)dst;
	
	const v128u16 val_vec128 = _mm_set1_epi16(val);
	MACRODO_N(ELEMENTCOUNT / (sizeof(v128u16) / sizeof(u16)), _mm_store_si128(dst_vec128 + (X), val_vec128));
}

static void memset_u32(void *dst, const u32 val, const size_t elementCount)
{
	v128u32 *dst_vec128 = (v128u32 *)dst;
	const size_t length_vec128 = elementCount / (sizeof(v128u32) / sizeof(u32));
	
	const v128u32 val_vec128 = _mm_set1_epi32(val);
	for (size_t i = 0; i < length_vec128; i++)
		_mm_stream_si128(dst_vec128 + i, val_vec128);
}

template <size_t ELEMENTCOUNT>
static void memset_u32_fast(void *dst, const u32 val)
{
	v128u32 *dst_vec128 = (v128u32 *)dst;
	
	const v128u32 val_vec128 = _mm_set1_epi32(val);
	MACRODO_N(ELEMENTCOUNT / (sizeof(v128u32) / sizeof(u32)), _mm_store_si128(dst_vec128 + (X), val_vec128));
}

#elif defined(ENABLE_ALTIVEC)

static void memset_u16(void *dst, const u16 val, const size_t elementCount)
{
	v128u16 *dst_vec128 = (v128u16 *)dst;
	const size_t length_vec128 = elementCount / (sizeof(v128u16) / sizeof(u16));
	
	const v128u16 val_vec128 = vec_splat_u16(val);
	for (size_t i = 0; i < length_vec128; i++)
		vec_st(val_vec128, 0, dst_vec128 + i);
}

template <size_t ELEMENTCOUNT>
static void memset_u16_fast(void *dst, const u16 val)
{
	v128u16 *dst_vec128 = (v128u16 *)dst;
	
	const v128u16 val_vec128 = vec_splat_u16(val);
	MACRODO_N(ELEMENTCOUNT / (sizeof(v128u16) / sizeof(u16)), vec_st(val_vec128, 0, dst_vec128 + (X)));
}

static void memset_u32(void *dst, const u32 val, const size_t elementCount)
{
	v128u32 *dst_vec128 = (v128u32 *)dst;
	const size_t length_vec128 = elementCount / (sizeof(v128u32) / sizeof(u32));
	
	const v128u32 val_vec128 = vec_splat_u32(val);
	for (size_t i = 0; i < length_vec128; i++)
		vec_st(val_vec128, 0, dst_vec128 + i);
}

template <size_t ELEMENTCOUNT>
static void memset_u32_fast(void *dst, const u32 val)
{
	v128u32 *dst_vec128 = (v128u32 *)dst;
	
	const v128u32 val_vec128 = vec_splat_u32(val);
	MACRODO_N(ELEMENTCOUNT / (sizeof(v128u32) / sizeof(u32)), vec_st(val_vec128, 0, dst_vec128 + (X)));
}

#else // No SIMD

static void memset_u16(void *dst, const u16 val, const size_t elementCount)
{
#ifdef HOST_64
	u64 *dst_u64 = (u64 *)dst;
	const u64 val_u64 = ((u64)val << 48) | ((u64)val << 32) | ((u64)val << 16) | (u64)val;
	const size_t length_u64 = elementCount / (sizeof(val_u64) / sizeof(val));
	
	for (size_t i = 0; i < length_u64; i++)
		dst_u64[i] = val_u64;
#else
	for (size_t i = 0; i < elementCount; i++)
		((u16 *)dst)[i] = val;
#endif
}

template <size_t ELEMENTCOUNT>
static void memset_u16_fast(void *dst, const u16 val)
{
#ifdef HOST_64
	u64 *dst_u64 = (u64 *)dst;
	const u64 val_u64 = ((u64)val << 48) | ((u64)val << 32) | ((u64)val << 16) | (u64)val;
	MACRODO_N(ELEMENTCOUNT / (sizeof(val_u64) / sizeof(val)), (dst_u64[(X)] = val_u64));
#else
	for (size_t i = 0; i < ELEMENTCOUNT; i++)
		((u16 *)dst)[i] = val;
#endif
}

static void memset_u32(void *dst, const u32 val, const size_t elementCount)
{
#ifdef HOST_64
	u64 *dst_u64 = (u64 *)dst;
	const u64 val_u64 = ((u64)val << 32) | (u64)val;
	const size_t length_u64 = elementCount / (sizeof(val_u64) / sizeof(val));
	
	for (size_t i = 0; i < length_u64; i++)
		dst_u64[i] = val_u64;
#else
	for (size_t i = 0; i < elementCount; i++)
		((u32 *)dst)[i] = val;
#endif
}

template <size_t ELEMENTCOUNT>
static void memset_u32_fast(void *dst, const u32 val)
{
#ifdef HOST_64
	u64 *dst_u64 = (u64 *)dst;
	const u64 val_u64 = ((u64)val << 32) | (u64)val;
	MACRODO_N(ELEMENTCOUNT / (sizeof(val_u64) / sizeof(val)), (dst_u64[(X)] = val_u64));
#else
	for (size_t i = 0; i < ELEMENTCOUNT; i++)
		((u16 *)dst)[i] = val;
#endif
}

#endif // SIMD Functions

#endif // MATRIX_H
