/*  
	Copyright (C) 2006-2007 shash
	Copyright (C) 2007-2021 DeSmuME team

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

#ifdef ENABLE_SSE4_1
#include <smmintrin.h>
#endif

enum MatrixMode
{
	MATRIXMODE_PROJECTION		= 0,
	MATRIXMODE_POSITION			= 1,
	MATRIXMODE_POSITION_VECTOR	= 2,
	MATRIXMODE_TEXTURE			= 3
};

template<MatrixMode MODE>
struct MatrixStack
{
	static const size_t size = ((MODE == MATRIXMODE_PROJECTION) || (MODE == MATRIXMODE_TEXTURE)) ? 1 : 32;
	static const MatrixMode type = MODE;
	
	s32 matrix[size][16];
	u32 position;
};

void MatrixInit(s32 (&mtx)[16]);
void MatrixInit(float (&mtx)[16]);

void MatrixIdentity(s32 (&mtx)[16]);
void MatrixIdentity(float (&mtx)[16]);

void MatrixSet(s32 (&mtx)[16], const size_t x, const size_t y, const s32 value);
void MatrixSet(float (&mtx)[16], const size_t x, const size_t y, const float value);
void MatrixSet(float (&mtx)[16], const size_t x, const size_t y, const s32 value);

void MatrixCopy(s32 (&__restrict mtxDst)[16], const s32 (&__restrict mtxSrc)[16]);
void MatrixCopy(float (&__restrict mtxDst)[16], const float (&__restrict mtxSrc)[16]);
void MatrixCopy(float (&__restrict mtxDst)[16], const s32 (&__restrict mtxSrc)[16]);

int MatrixCompare(const s32 (&__restrict mtxDst)[16], const s32 (&__restrict mtxSrc)[16]);
int MatrixCompare(const float (&__restrict mtxDst)[16], const float (&__restrict mtxSrc)[16]);

s32	MatrixGetMultipliedIndex(const u32 index, const s32 (&__restrict mtxA)[16], const s32 (&__restrict mtxB)[16]);
float MatrixGetMultipliedIndex(const u32 index, const float (&__restrict mtxA)[16], const float (&__restrict mtxB)[16]);

template<MatrixMode MODE> void MatrixStackInit(MatrixStack<MODE> *stack);
template<MatrixMode MODE> s32* MatrixStackGet(MatrixStack<MODE> *stack);

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


void _MatrixMultVec4x4_NoSIMD(const s32 (&__restrict mtx)[16], float (&__restrict vec)[4]);

void MatrixMultVec4x4(const s32 (&__restrict mtx)[16], float (&__restrict vec)[4]);
void MatrixMultVec3x3(const s32 (&__restrict mtx)[16], float (&__restrict vec)[4]);
void MatrixTranslate(float (&__restrict mtx)[16], const float (&__restrict vec)[4]);
void MatrixScale(float (&__restrict mtx)[16], const float (&__restrict vec)[4]);
void MatrixMultiply(float (&__restrict mtxA)[16], const s32 (&__restrict mtxB)[16]);

template<size_t NUM_ROWS> FORCEINLINE void vector_fix2float(float (&mtx)[16], const float divisor);

void MatrixMultVec4x4(const s32 (&__restrict mtx)[16], s32 (&__restrict vec)[4]);
void MatrixMultVec3x3(const s32 (&__restrict mtx)[16], s32 (&__restrict vec)[4]);
void MatrixTranslate(s32 (&__restrict mtx)[16], const s32 (&__restrict vec)[4]);
void MatrixScale(s32 (&__restrict mtx)[16], const s32 (&__restrict vec)[4]);
void MatrixMultiply(s32 (&__restrict mtxA)[16], const s32 (&__restrict mtxB)[16]);

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

FORCEINLINE s32 sfx32_shiftdown(const s64 a)
{
	//TODO: replace me with direct calls to sfx32_shiftdown
	return fx32_shiftdown(a);
}

// SIMD Functions
//-------------
#if defined(ENABLE_AVX512_0)

static void memset_u16(void *dst, const u16 val, const size_t elementCount)
{
	v512u16 *dst_vec512 = (v512u16 *)dst;
	const size_t length_vec512 = elementCount / (sizeof(v512u16) / sizeof(u16));
	
	const v512u16 val_vec512 = _mm512_set1_epi16(val);
	for (size_t i = 0; i < length_vec512; i++)
		_mm512_stream_si512(dst_vec512 + i, val_vec512);
}

template <size_t ELEMENTCOUNT>
static void memset_u16_fast(void *dst, const u16 val)
{
	v512u16 *dst_vec512 = (v512u16 *)dst;
	
	const v512u16 val_vec512 = _mm512_set1_epi16(val);
	MACRODO_N(ELEMENTCOUNT / (sizeof(v512u16) / sizeof(u16)), _mm512_store_si512(dst_vec512 + (X), val_vec512));
}

static void memset_u32(void *dst, const u32 val, const size_t elementCount)
{
	v512u32 *dst_vec512 = (v512u32 *)dst;
	const size_t length_vec512 = elementCount / (sizeof(v512u32) / sizeof(u32));
	
	const v512u32 val_vec512 = _mm512_set1_epi32(val);
	for (size_t i = 0; i < length_vec512; i++)
		_mm512_stream_si512(dst_vec512 + i, val_vec512);
}

template <size_t ELEMENTCOUNT>
static void memset_u32_fast(void *dst, const u32 val)
{
	v512u32 *dst_vec512 = (v512u32 *)dst;
	
	const v512u32 val_vec512 = _mm512_set1_epi32(val);
	MACRODO_N(ELEMENTCOUNT / (sizeof(v512u32) / sizeof(u32)), _mm512_store_si512(dst_vec512 + (X), val_vec512));
}

template <size_t LENGTH>
static void stream_copy_fast(void *__restrict dst, void *__restrict src)
{
	MACRODO_N( LENGTH / sizeof(v512s8), _mm512_stream_si512((v512s8 *)dst + (X), _mm512_stream_load_si512((v512s8 *)src + (X))) );
}

template <size_t LENGTH>
static void buffer_copy_fast(void *__restrict dst, void *__restrict src)
{
	MACRODO_N( LENGTH / sizeof(v512s8), _mm512_store_si512((v512s8 *)dst + (X), _mm512_load_si512((v512s8 *)src + (X))) );
}

template <size_t VECLENGTH>
static void __buffer_copy_or_constant_fast(void *__restrict dst, const void *__restrict src, const __m512i &c_vec)
{
	MACRODO_N( VECLENGTH / sizeof(v512s8), _mm512_store_si512((v512s8 *)dst + (X), _mm512_or_si512(_mm512_load_si512((v512s8 *)src + (X)),c_vec)) );
}

static void __buffer_copy_or_constant(void *__restrict dst, const void *__restrict src, const size_t vecLength, const __m512i &c_vec)
{
	switch (vecLength)
	{
		case 128: __buffer_copy_or_constant_fast<128>(dst, src, c_vec); break;
		case 256: __buffer_copy_or_constant_fast<256>(dst, src, c_vec); break;
		case 512: __buffer_copy_or_constant_fast<512>(dst, src, c_vec); break;
		case 768: __buffer_copy_or_constant_fast<768>(dst, src, c_vec); break;
		case 1024: __buffer_copy_or_constant_fast<1024>(dst, src, c_vec); break;
		case 2048: __buffer_copy_or_constant_fast<2048>(dst, src, c_vec); break;
		case 2304: __buffer_copy_or_constant_fast<2304>(dst, src, c_vec); break;
		case 4096: __buffer_copy_or_constant_fast<4096>(dst, src, c_vec); break;
		case 4608: __buffer_copy_or_constant_fast<4608>(dst, src, c_vec); break;
		case 8192: __buffer_copy_or_constant_fast<8192>(dst, src, c_vec); break;
		case 9216: __buffer_copy_or_constant_fast<9216>(dst, src, c_vec); break;
		case 16384: __buffer_copy_or_constant_fast<16384>(dst, src, c_vec); break;
			
		default:
		{
			for (size_t i = 0; i < vecLength; i+=sizeof(v512s8))
			{
				_mm512_store_si512((v512s8 *)((s8 *)dst + i), _mm512_or_si512( _mm512_load_si512((v512s8 *)((s8 *)src + i)), c_vec ) );
			}
			break;
		}
	}
}

static void buffer_copy_or_constant_s8(void *__restrict dst, const void *__restrict src, const size_t vecLength, const s8 c)
{
	const v512s8 c_vec = _mm512_set1_epi8(c);
	__buffer_copy_or_constant(dst, src, vecLength, c_vec);
}

template <size_t VECLENGTH>
static void buffer_copy_or_constant_s8_fast(void *__restrict dst, const void *__restrict src, const s8 c)
{
	const v512s8 c_vec = _mm512_set1_epi8(c);
	__buffer_copy_or_constant_fast<VECLENGTH>(dst, src, c_vec);
}

template <bool NEEDENDIANSWAP>
static void buffer_copy_or_constant_s16(void *__restrict dst, const void *__restrict src, const size_t vecLength, const s16 c)
{
	const v512s16 c_vec = _mm512_set1_epi16(c);
	__buffer_copy_or_constant(dst, src, vecLength, c_vec);
}

template <size_t VECLENGTH, bool NEEDENDIANSWAP>
static void buffer_copy_or_constant_s16_fast(void *__restrict dst, const void *__restrict src, const s16 c)
{
	const v512s16 c_vec = _mm512_set1_epi16(c);
	__buffer_copy_or_constant_fast<VECLENGTH>(dst, src, c_vec);
}

template <bool NEEDENDIANSWAP>
static void buffer_copy_or_constant_s32(void *__restrict dst, const void *__restrict src, const size_t vecLength, const s32 c)
{
	const v512s32 c_vec = _mm512_set1_epi32(c);
	__buffer_copy_or_constant(dst, src, vecLength, c_vec);
}

template <size_t VECLENGTH, bool NEEDENDIANSWAP>
static void buffer_copy_or_constant_s32_fast(void *__restrict dst, const void *__restrict src, const s32 c)
{
	const v512s32 c_vec = _mm512_set1_epi32(c);
	__buffer_copy_or_constant_fast<VECLENGTH>(dst, src, c_vec);
}

#elif defined(ENABLE_AVX2)

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

template <size_t VECLENGTH>
static void stream_copy_fast(void *__restrict dst, const void *__restrict src)
{
	MACRODO_N( VECLENGTH / sizeof(v256s8), _mm256_stream_si256((v256s8 *)dst + (X), _mm256_stream_load_si256((v256s8 *)src + (X))) );
}

template <size_t VECLENGTH>
static void buffer_copy_fast(void *__restrict dst, const void *__restrict src)
{
	MACRODO_N( VECLENGTH / sizeof(v256s8), _mm256_store_si256((v256s8 *)dst + (X), _mm256_load_si256((v256s8 *)src + (X))) );
}

template <size_t VECLENGTH>
static void __buffer_copy_or_constant_fast(void *__restrict dst, const void *__restrict src, const __m256i &c_vec)
{
	MACRODO_N( VECLENGTH / sizeof(v256s8), _mm256_store_si256((v256s8 *)dst + (X), _mm256_or_si256(_mm256_load_si256((v256s8 *)src + (X)),c_vec)) );
}

static void __buffer_copy_or_constant(void *__restrict dst, const void *__restrict src, const size_t vecLength, const __m256i &c_vec)
{
	switch (vecLength)
	{
		case 128: __buffer_copy_or_constant_fast<128>(dst, src, c_vec); break;
		case 256: __buffer_copy_or_constant_fast<256>(dst, src, c_vec); break;
		case 512: __buffer_copy_or_constant_fast<512>(dst, src, c_vec); break;
		case 768: __buffer_copy_or_constant_fast<768>(dst, src, c_vec); break;
		case 1024: __buffer_copy_or_constant_fast<1024>(dst, src, c_vec); break;
		case 2048: __buffer_copy_or_constant_fast<2048>(dst, src, c_vec); break;
		case 2304: __buffer_copy_or_constant_fast<2304>(dst, src, c_vec); break;
		case 4096: __buffer_copy_or_constant_fast<4096>(dst, src, c_vec); break;
		case 4608: __buffer_copy_or_constant_fast<4608>(dst, src, c_vec); break;
		case 8192: __buffer_copy_or_constant_fast<8192>(dst, src, c_vec); break;
		
		default:
		{
			for (size_t i = 0; i < vecLength; i+=sizeof(v256s8))
			{
				_mm256_store_si256((v256s8 *)((s8 *)dst + i), _mm256_or_si256( _mm256_load_si256((v256s8 *)((s8 *)src + i)), c_vec ) );
			}
			break;
		}
	}
}

static void buffer_copy_or_constant_s8(void *__restrict dst, const void *__restrict src, const size_t vecLength, const s8 c)
{
	if (c != 0)
	{
		const v256s8 c_vec = _mm256_set1_epi8(c);
		__buffer_copy_or_constant(dst, src, vecLength, c_vec);
	}
	else
	{
		memcpy(dst, src, vecLength);
	}
}

template <size_t VECLENGTH>
static void buffer_copy_or_constant_s8_fast(void *__restrict dst, const void *__restrict src, const s8 c)
{
	if (c != 0)
	{
		const v256s8 c_vec = _mm256_set1_epi8(c);
		__buffer_copy_or_constant_fast<VECLENGTH>(dst, src, c_vec);
	}
	else
	{
		buffer_copy_fast<VECLENGTH>(dst, src);
	}
}

template <bool NEEDENDIANSWAP>
static void buffer_copy_or_constant_s16(void *__restrict dst, const void *__restrict src, const size_t vecLength, const s16 c)
{
	if (c != 0)
	{
		const v256s16 c_vec = _mm256_set1_epi16(c);
		__buffer_copy_or_constant(dst, src, vecLength, c_vec);
	}
	else
	{
		memcpy(dst, src, vecLength);
	}
}

template <size_t VECLENGTH, bool NEEDENDIANSWAP>
static void buffer_copy_or_constant_s16_fast(void *__restrict dst, const void *__restrict src, const s16 c)
{
	if (c != 0)
	{
		const v256s16 c_vec = _mm256_set1_epi16(c);
		__buffer_copy_or_constant_fast<VECLENGTH>(dst, src, c_vec);
	}
	else
	{
		buffer_copy_fast<VECLENGTH>(dst, src);
	}
}

template <bool NEEDENDIANSWAP>
static void buffer_copy_or_constant_s32(void *__restrict dst, const void *__restrict src, const size_t vecLength, const s32 c)
{
	if (c != 0)
	{
		const v256s32 c_vec = _mm256_set1_epi32(c);
		__buffer_copy_or_constant(dst, src, vecLength, c_vec);
	}
	else
	{
		memcpy(dst, src, vecLength);
	}
}

template <size_t VECLENGTH, bool NEEDENDIANSWAP>
static void buffer_copy_or_constant_s32_fast(void *__restrict dst, const void *__restrict src, const s32 c)
{
	if (c != 0)
	{
		const v256s32 c_vec = _mm256_set1_epi32(c);
		__buffer_copy_or_constant_fast<VECLENGTH>(dst, src, c_vec);
	}
	else
	{
		buffer_copy_fast<VECLENGTH>(dst, src);
	}
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

template <size_t VECLENGTH>
static void stream_copy_fast(void *__restrict dst, void *__restrict src)
{
#ifdef ENABLE_SSE4_1
	MACRODO_N( VECLENGTH / sizeof(v128s8), _mm_stream_si128((v128s8 *)dst + (X), _mm_stream_load_si128((v128s8 *)src + (X))) );
#else
	MACRODO_N( VECLENGTH / sizeof(v128s8), _mm_stream_si128((v128s8 *)dst + (X), _mm_load_si128((v128s8 *)src + (X))) );
#endif
}

template <size_t VECLENGTH>
static void buffer_copy_fast(void *__restrict dst, void *__restrict src)
{
	MACRODO_N( VECLENGTH / sizeof(v128s8), _mm_store_si128((v128s8 *)dst + (X), _mm_load_si128((v128s8 *)src + (X))) );
}

template <size_t VECLENGTH>
static void __buffer_copy_or_constant_fast(void *__restrict dst, const void *__restrict src, const __m128i &c_vec)
{
	MACRODO_N( VECLENGTH / sizeof(v128s8), _mm_store_si128((v128s8 *)dst + (X), _mm_or_si128(_mm_load_si128((v128s8 *)src + (X)),c_vec)) );
}

static void __buffer_copy_or_constant(void *__restrict dst, const void *__restrict src, const size_t vecLength, const __m128i &c_vec)
{
	switch (vecLength)
	{
		case 128: __buffer_copy_or_constant_fast<128>(dst, src, c_vec); break;
		case 256: __buffer_copy_or_constant_fast<256>(dst, src, c_vec); break;
		case 512: __buffer_copy_or_constant_fast<512>(dst, src, c_vec); break;
		case 768: __buffer_copy_or_constant_fast<768>(dst, src, c_vec); break;
		case 1024: __buffer_copy_or_constant_fast<1024>(dst, src, c_vec); break;
		case 2048: __buffer_copy_or_constant_fast<2048>(dst, src, c_vec); break;
		case 2304: __buffer_copy_or_constant_fast<2304>(dst, src, c_vec); break;
		case 4096: __buffer_copy_or_constant_fast<4096>(dst, src, c_vec); break;
			
		default:
		{
			for (size_t i = 0; i < vecLength; i+=sizeof(v128s8))
			{
				_mm_store_si128((v128s8 *)((s8 *)dst + i), _mm_or_si128( _mm_load_si128((v128s8 *)((s8 *)src + i)), c_vec ) );
			}
			break;
		}
	}
}

static void buffer_copy_or_constant_s8(void *__restrict dst, const void *__restrict src, const size_t vecLength, const s8 c)
{
	const v128s8 c_vec = _mm_set1_epi8(c);
	__buffer_copy_or_constant(dst, src, vecLength, c_vec);
}

template <size_t VECLENGTH>
static void buffer_copy_or_constant_s8_fast(void *__restrict dst, const void *__restrict src, const s8 c)
{
	const v128s8 c_vec = _mm_set1_epi8(c);
	__buffer_copy_or_constant_fast<VECLENGTH>(dst, src, c_vec);
}

template <bool NEEDENDIANSWAP>
static void buffer_copy_or_constant_s16(void *__restrict dst, const void *__restrict src, const size_t vecLength, const s16 c)
{
	const v128s16 c_vec = _mm_set1_epi16(c);
	__buffer_copy_or_constant(dst, src, vecLength, c_vec);
}

template <size_t VECLENGTH, bool NEEDENDIANSWAP>
static void buffer_copy_or_constant_s16_fast(void *__restrict dst, const void *__restrict src, const s16 c)
{
	const v128s16 c_vec = _mm_set1_epi16(c);
	__buffer_copy_or_constant_fast<VECLENGTH>(dst, src, c_vec);
}

template <bool NEEDENDIANSWAP>
static void buffer_copy_or_constant_s32(void *__restrict dst, const void *__restrict src, const size_t vecLength, const s32 c)
{
	const v128s32 c_vec = _mm_set1_epi32(c);
	__buffer_copy_or_constant(dst, src, vecLength, c_vec);
}

template <size_t VECLENGTH, bool NEEDENDIANSWAP>
static void buffer_copy_or_constant_s32_fast(void *__restrict dst, const void *__restrict src, const s32 c)
{
	const v128s32 c_vec = _mm_set1_epi32(c);
	__buffer_copy_or_constant_fast<VECLENGTH>(dst, src, c_vec);
}

#elif defined(ENABLE_ALTIVEC)

static void memset_u16(void *dst, const u16 val, const size_t elementCount)
{
	v128u16 *dst_vec128 = (v128u16 *)dst;
	const size_t length_vec128 = elementCount / (sizeof(v128u16) / sizeof(u16));
	
	const v128u16 val_vec128 = (v128u16){val,val,val,val,val,val,val,val};
	for (size_t i = 0; i < length_vec128; i++)
		vec_st(val_vec128, 0, dst_vec128 + i);
}

template <size_t ELEMENTCOUNT>
static void memset_u16_fast(void *dst, const u16 val)
{
	v128u16 *dst_vec128 = (v128u16 *)dst;
	
	const v128u16 val_vec128 = (v128u16){val,val,val,val,val,val,val,val};
	MACRODO_N(ELEMENTCOUNT / (sizeof(v128u16) / sizeof(u16)), vec_st(val_vec128, 0, dst_vec128 + (X)));
}

static void memset_u32(void *dst, const u32 val, const size_t elementCount)
{
	v128u32 *dst_vec128 = (v128u32 *)dst;
	const size_t length_vec128 = elementCount / (sizeof(v128u32) / sizeof(u32));
	
	const v128u32 val_vec128 = (v128u32){val,val,val,val};
	for (size_t i = 0; i < length_vec128; i++)
		vec_st(val_vec128, 0, dst_vec128 + i);
}

template <size_t ELEMENTCOUNT>
static void memset_u32_fast(void *dst, const u32 val)
{
	v128u32 *dst_vec128 = (v128u32 *)dst;
	
	const v128u32 val_vec128 = (v128u32){val,val,val,val};
	MACRODO_N(ELEMENTCOUNT / (sizeof(v128u32) / sizeof(u32)), vec_st(val_vec128, 0, dst_vec128 + (X)));
}

template <size_t VECLENGTH>
static void stream_copy_fast(void *__restrict dst, void *__restrict src)
{
	memcpy(dst, src, VECLENGTH);
}

template <size_t VECLENGTH>
static void buffer_copy_fast(void *__restrict dst, void *__restrict src)
{
	MACRODO_N( VECLENGTH / sizeof(v128s8), vec_st(vec_ld((X)*sizeof(v128s8),src), (X)*sizeof(v128s8), dst) );
}

template <class T, size_t VECLENGTH, bool NEEDENDIANSWAP>
static void __buffer_copy_or_constant_fast(void *__restrict dst, const void *__restrict src, const T &c_vec)
{
	MACRODO_N( VECLENGTH / sizeof(v128s8), vec_st(vec_or(vec_ld((X)*sizeof(v128s8),(T*)src),c_vec), (X)*sizeof(v128s8), dst) );
}

template <class T, bool NEEDENDIANSWAP>
static void __buffer_copy_or_constant(void *__restrict dst, const void *__restrict src, const size_t vecLength, const T &c_vec)
{
	switch (vecLength)
	{
		case 128: __buffer_copy_or_constant_fast<T, 128, NEEDENDIANSWAP>(dst, src, c_vec); break;
		case 256: __buffer_copy_or_constant_fast<T, 256, NEEDENDIANSWAP>(dst, src, c_vec); break;
		case 512: __buffer_copy_or_constant_fast<T, 512, NEEDENDIANSWAP>(dst, src, c_vec); break;
		case 768: __buffer_copy_or_constant_fast<T, 768, NEEDENDIANSWAP>(dst, src, c_vec); break;
		case 1024: __buffer_copy_or_constant_fast<T, 1024, NEEDENDIANSWAP>(dst, src, c_vec); break;
		case 2048: __buffer_copy_or_constant_fast<T, 2048, NEEDENDIANSWAP>(dst, src, c_vec); break;
		case 2304: __buffer_copy_or_constant_fast<T, 2304, NEEDENDIANSWAP>(dst, src, c_vec); break;
		case 4096: __buffer_copy_or_constant_fast<T, 4096, NEEDENDIANSWAP>(dst, src, c_vec); break;
			
		default:
		{
			for (size_t i = 0; i < vecLength; i+=sizeof(T))
			{
				vec_st(vec_or(vec_ld(i,(T*)src),c_vec), i, dst);
			}
			break;
		}
	}
}

static void buffer_copy_or_constant_s8(void *__restrict dst, const void *__restrict src, const size_t vecLength, const s8 c)
{
	const v128s8 c_vec = {c,c,c,c,c,c,c,c,c,c,c,c,c,c,c,c};
	__buffer_copy_or_constant<v128s8, false>(dst, src, vecLength, c_vec);
}

template <size_t VECLENGTH>
static void buffer_copy_or_constant_s8_fast(void *__restrict dst, void *__restrict src, const s8 c)
{
	const v128s8 c_vec = {c,c,c,c,c,c,c,c,c,c,c,c,c,c,c,c};
	__buffer_copy_or_constant_fast<v128s8, VECLENGTH>(dst, src, c_vec);
}

template <bool NEEDENDIANSWAP>
static void buffer_copy_or_constant_s16(void *__restrict dst, const void *__restrict src, const size_t vecLength, const s16 c)
{
	const s16 c_16 = (NEEDENDIANSWAP) ? LE_TO_LOCAL_16(c) : c;
	const v128s16 c_vec = {c_16, c_16, c_16, c_16, c_16, c_16, c_16, c_16};
	__buffer_copy_or_constant<v128s16, NEEDENDIANSWAP>(dst, src, vecLength, c_vec);
}

template <size_t VECLENGTH, bool NEEDENDIANSWAP>
static void buffer_copy_or_constant_s16_fast(void *__restrict dst, void *__restrict src, const s16 c)
{
	const s16 c_16 = (NEEDENDIANSWAP) ? LE_TO_LOCAL_16(c) : c;
	const v128s16 c_vec = {c_16, c_16, c_16, c_16, c_16, c_16, c_16, c_16};
	__buffer_copy_or_constant_fast<v128s16, VECLENGTH, NEEDENDIANSWAP>(dst, src, c_vec);
}

template <bool NEEDENDIANSWAP>
static void buffer_copy_or_constant_s32(void *__restrict dst, const void *__restrict src, const size_t vecLength, const s32 c)
{
	const s32 c_32 = (NEEDENDIANSWAP) ? LE_TO_LOCAL_32(c) : c;
	const v128s32 c_vec = {c_32, c_32, c_32, c_32};
	__buffer_copy_or_constant<v128s32, NEEDENDIANSWAP>(dst, src, vecLength, c_vec);
}

template <size_t VECLENGTH, bool NEEDENDIANSWAP>
static void buffer_copy_or_constant_s32_fast(void *__restrict dst, void *__restrict src, const s32 c)
{
	const s32 c_32 = (NEEDENDIANSWAP) ? LE_TO_LOCAL_32(c) : c;
	const v128s32 c_vec = {c_32, c_32, c_32, c_32};
	__buffer_copy_or_constant_fast<v128s32, VECLENGTH, NEEDENDIANSWAP>(dst, src, c_vec);
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

// The difference between buffer_copy_fast() and stream_copy_fast() is that
// buffer_copy_fast() assumes that both src and dst buffers can be used
// immediately after the copy operation and that dst will be cached, while
// stream_copy_fast() assumes that both src and dst buffers will NOT be used
// immediately after the copy operation and that dst will NOT be cached.
//
// In the ANSI-C implementation, we just call memcpy() for both functions,
// but for the manually vectorized implementations, we use the specific
// vector intrinsics to control the temporal/caching behavior.

template <size_t VECLENGTH>
static void stream_copy_fast(void *__restrict dst, void *__restrict src)
{
	memcpy(dst, src, VECLENGTH);
}

template <size_t VECLENGTH>
static void buffer_copy_fast(void *__restrict dst, void *__restrict src)
{
	memcpy(dst, src, VECLENGTH);
}

static void buffer_copy_or_constant_s8(void *__restrict dst, const void *__restrict src, const size_t vecLength, const s8 c)
{
#ifdef HOST_64
	s64 *src_64 = (s64 *)src;
	s64 *dst_64 = (s64 *)dst;
	const s64 c_64 = ((s64)c << 56) | ((s64)c << 48) | ((s64)c << 40) | ((s64)c << 32) | ((s64)c << 24) | ((s64)c << 16) | ((s64)c << 8) | (s64)c;
	
	for (size_t i = 0; i < vecLength; i+=sizeof(s64))
		dst_64[i] = src_64[i] | c_64;
#else
	s32 *src_32 = (s32 *)src;
	s32 *dst_32 = (s32 *)dst;
	const s32 c_32 = ((s32)c << 24) | ((s32)c << 16) | ((s32)c << 8) | (s32)c;
	
	for (size_t i = 0; i < vecLength; i+=sizeof(s32))
		dst_32[i] = src_32[i] | c_32;
#endif
}

template <size_t VECLENGTH>
static void buffer_copy_or_constant_s8_fast(void *__restrict dst, void *__restrict src, const s8 c)
{
#ifdef HOST_64
	s64 *src_64 = (s64 *)src;
	s64 *dst_64 = (s64 *)dst;
	const s64 c_64 = ((s64)c << 56) | ((s64)c << 48) | ((s64)c << 40) | ((s64)c << 32) | ((s64)c << 24) | ((s64)c << 16) | ((s64)c << 8) | (s64)c;
	
	for (size_t i = 0; i < VECLENGTH; i+=sizeof(s64))
		dst_64[i] = src_64[i] | c_64;
#else
	s32 *src_32 = (s32 *)src;
	s32 *dst_32 = (s32 *)dst;
	const s32 c_32 = ((s32)c << 24) | ((s32)c << 16) | ((s32)c << 8) | (s32)c;
	
	for (size_t i = 0; i < VECLENGTH; i+=sizeof(s32))
		dst_32[i] = src_32[i] | c_32;
#endif
}

template <bool NEEDENDIANSWAP>
static void buffer_copy_or_constant_s16(void *__restrict dst, const void *__restrict src, const size_t vecLength, const s16 c)
{
#ifdef HOST_64
	s64 *src_64 = (s64 *)src;
	s64 *dst_64 = (s64 *)dst;
	const s64 c_16x4 = (NEEDENDIANSWAP) ? (s64)LE_TO_LOCAL_16(c) : (s64)c;
	const s64 c_64 = (c_16x4 << 48) | (c_16x4 << 32) | (c_16x4 << 16) | c_16x4;
	
	for (size_t i = 0; i < vecLength; i+=sizeof(s64))
	{
		if (NEEDENDIANSWAP)
		{
			dst_64[i] = ( ((src_64[i] & 0xFF00FF00FF00FF00ULL) >> 8) | ((src_64[i] & 0x00FF00FF00FF00FFULL) << 8) ) | c_64;
		}
		else
		{
			dst_64[i] = src_64[i] | c_64;
		}
	}
#else
	s32 *src_32 = (s32 *)src;
	s32 *dst_32 = (s32 *)dst;
	const s32 c_16x2 = (NEEDENDIANSWAP) ? (s32)LE_TO_LOCAL_16(c) : (s32)c;
	const s32 c_32 = (c_16x2 << 16) | c_16x2;
	
	for (size_t i = 0; i < vecLength; i+=sizeof(s32))
	{
		if (NEEDENDIANSWAP)
		{
			dst_32[i] = ( ((src_32[i] & 0x00FF00FF) << 8) | ((src_32[i] & 0xFF00FF00) >> 8) ) | c_32;
		}
		else
		{
			dst_32[i] = src_32[i] | c_32;
		}
	}
#endif
}

template <size_t VECLENGTH, bool NEEDENDIANSWAP>
static void buffer_copy_or_constant_s16_fast(void *__restrict dst, void *__restrict src, const s16 c)
{
#ifdef HOST_64
	s64 *src_64 = (s64 *)src;
	s64 *dst_64 = (s64 *)dst;
	const s64 c_16x4 = (NEEDENDIANSWAP) ? (s64)LE_TO_LOCAL_16(c) : (s64)c;
	const s64 c_64 = (c_16x4 << 48) | (c_16x4 << 32) | (c_16x4 << 16) | c_16x4;
	
	for (size_t i = 0; i < VECLENGTH; i+=sizeof(s64))
	{
		if (NEEDENDIANSWAP)
		{
			dst_64[i] = ( ((src_64[i] & 0x00FF00FF00FF00FFULL) << 8) | ((src_64[i] & 0xFF00FF00FF00FF00ULL) >> 8) ) | c_64;
		}
		else
		{
			dst_64[i] = src_64[i] | c_64;
		}
	}
#else
	s32 *src_32 = (s32 *)src;
	s32 *dst_32 = (s32 *)dst;
	const s32 c_16x2 = (NEEDENDIANSWAP) ? (s32)LE_TO_LOCAL_16(c) : (s32)c;
	const s32 c_32 = (c_16x2 << 16) | c_16x2;
	
	for (size_t i = 0; i < VECLENGTH; i+=sizeof(s32))
	{
		if (NEEDENDIANSWAP)
		{
			dst_32[i] = ( ((src_32[i] & 0x00FF00FF) << 8) | ((src_32[i] & 0xFF00FF00) >> 8) ) | c_32;
		}
		else
		{
			dst_32[i] = src_32[i] | c_32;
		}
	}
#endif
}

template <bool NEEDENDIANSWAP>
static void buffer_copy_or_constant_s32(void *__restrict dst, const void *__restrict src, const size_t vecLength, const s32 c)
{
#ifdef HOST_64
	s64 *src_64 = (s64 *)src;
	s64 *dst_64 = (s64 *)dst;
	const s64 c_32x2 = (NEEDENDIANSWAP) ? (s32)LE_TO_LOCAL_32(c) : (s32)c;
	const s64 c_64 = (c_32x2 << 32) | c_32x2;
	
	for (size_t i = 0; i < vecLength; i+=sizeof(s64))
	{
		if (NEEDENDIANSWAP)
		{
			dst_64[i] = ( ((src_64[i] & 0x000000FF000000FFULL) << 24) | ((src_64[i] & 0x0000FF000000FF00ULL) << 8) | ((src_64[i] & 0x00FF000000FF0000ULL) >> 8) | ((src_64[i] & 0xFF000000FF000000ULL) >> 24) ) | c_64;
		}
		else
		{
			dst_64[i] = src_64[i] | c_64;
		}
	}
#else
	s32 *src_32 = (s32 *)src;
	s32 *dst_32 = (s32 *)dst;
	
	for (size_t i = 0; i < vecLength; i+=sizeof(s32))
		dst_32[i] = (NEEDENDIANSWAP) ? LOCAL_TO_LE_32(src_32[i] | c) : src_32[i] | c;
#endif
}

template <size_t VECLENGTH, bool NEEDENDIANSWAP>
static void buffer_copy_or_constant_s32_fast(void *__restrict dst, void *__restrict src, const s32 c)
{
#ifdef HOST_64
	s64 *src_64 = (s64 *)src;
	s64 *dst_64 = (s64 *)dst;
	const s64 c_32x2 = (NEEDENDIANSWAP) ? (s32)LE_TO_LOCAL_32(c) : (s32)c;
	const s64 c_64 = (c_32x2 << 32) | c_32x2;
	
	for (size_t i = 0; i < VECLENGTH; i+=sizeof(s64))
	{
		if (NEEDENDIANSWAP)
		{
			dst_64[i] = ( ((src_64[i] & 0x000000FF000000FFULL) << 24) | ((src_64[i] & 0x0000FF000000FF00ULL) << 8) | ((src_64[i] & 0x00FF000000FF0000ULL) >> 8) | ((src_64[i] & 0xFF000000FF000000ULL) >> 24) ) | c_64;
		}
		else
		{
			dst_64[i] = src_64[i] | c_64;
		}
	}
#else
	s32 *src_32 = (s32 *)src;
	s32 *dst_32 = (s32 *)dst;
	
	for (size_t i = 0; i < VECLENGTH; i+=sizeof(s32))
		dst_32[i] = (NEEDENDIANSWAP) ? LOCAL_TO_LE_32(src_32[i] | c) : src_32[i] | c;
#endif
}

#endif // SIMD Functions

#endif // MATRIX_H
