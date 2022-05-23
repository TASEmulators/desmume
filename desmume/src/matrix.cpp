/*
	Copyright (C) 2006-2007 shash
	Copyright (C) 2007-2022 DeSmuME team

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

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include <assert.h>
#include "matrix.h"
#include "MMU.h"


// The following floating-point functions exist for historical reasons and are deprecated.
// They should be obsoleted and removed as more of the geometry engine moves to fixed-point.
static FORCEINLINE void __mtx4_copy_mtx4_float(float (&__restrict outMtx)[16], const s32 (&__restrict inMtx)[16])
{
	outMtx[ 0] = (float)inMtx[ 0];
	outMtx[ 1] = (float)inMtx[ 1];
	outMtx[ 2] = (float)inMtx[ 2];
	outMtx[ 3] = (float)inMtx[ 3];
	
	outMtx[ 4] = (float)inMtx[ 4];
	outMtx[ 5] = (float)inMtx[ 5];
	outMtx[ 6] = (float)inMtx[ 6];
	outMtx[ 7] = (float)inMtx[ 7];
	
	outMtx[ 8] = (float)inMtx[ 8];
	outMtx[ 9] = (float)inMtx[ 9];
	outMtx[10] = (float)inMtx[10];
	outMtx[11] = (float)inMtx[11];
	
	outMtx[12] = (float)inMtx[12];
	outMtx[13] = (float)inMtx[13];
	outMtx[14] = (float)inMtx[14];
	outMtx[15] = (float)inMtx[15];
}

static FORCEINLINE void __mtx4_copynormalize_mtx4_float(float (&__restrict outMtx)[16], const s32 (&__restrict inMtx)[16])
{
	outMtx[ 0] = (float)inMtx[ 0] / 4096.0f;
	outMtx[ 1] = (float)inMtx[ 1] / 4096.0f;
	outMtx[ 2] = (float)inMtx[ 2] / 4096.0f;
	outMtx[ 3] = (float)inMtx[ 3] / 4096.0f;
	
	outMtx[ 4] = (float)inMtx[ 4] / 4096.0f;
	outMtx[ 5] = (float)inMtx[ 5] / 4096.0f;
	outMtx[ 6] = (float)inMtx[ 6] / 4096.0f;
	outMtx[ 7] = (float)inMtx[ 7] / 4096.0f;
	
	outMtx[ 8] = (float)inMtx[ 8] / 4096.0f;
	outMtx[ 9] = (float)inMtx[ 9] / 4096.0f;
	outMtx[10] = (float)inMtx[10] / 4096.0f;
	outMtx[11] = (float)inMtx[11] / 4096.0f;
	
	outMtx[12] = (float)inMtx[12] / 4096.0f;
	outMtx[13] = (float)inMtx[13] / 4096.0f;
	outMtx[14] = (float)inMtx[14] / 4096.0f;
	outMtx[15] = (float)inMtx[15] / 4096.0f;
}

static FORCEINLINE float __vec4_dotproduct_vec4_float(const float (&__restrict vecA)[4], const float (&__restrict vecB)[4])
{
	return (vecA[0] * vecB[0]) + (vecA[1] * vecB[1]) + (vecA[2] * vecB[2]) + (vecA[3] * vecB[3]);
}

static FORCEINLINE void __vec4_multiply_mtx4_float(float (&__restrict inoutVec)[4], const s32 (&__restrict inMtx)[16])
{
	const CACHE_ALIGN float v[4] = {inoutVec[0], inoutVec[1], inoutVec[2], inoutVec[3]};
	
	CACHE_ALIGN float m[16];
	__mtx4_copynormalize_mtx4_float(m, inMtx);
	
	inoutVec[0] = (m[0] * v[0]) + (m[4] * v[1]) + (m[ 8] * v[2]) + (m[12] * v[3]);
	inoutVec[1] = (m[1] * v[0]) + (m[5] * v[1]) + (m[ 9] * v[2]) + (m[13] * v[3]);
	inoutVec[2] = (m[2] * v[0]) + (m[6] * v[1]) + (m[10] * v[2]) + (m[14] * v[3]);
	inoutVec[3] = (m[3] * v[0]) + (m[7] * v[1]) + (m[11] * v[2]) + (m[15] * v[3]);
}

static FORCEINLINE void __vec3_multiply_mtx3_float(float (&__restrict inoutVec)[4], const s32 (&__restrict inMtx)[16])
{
	const CACHE_ALIGN float v[4] = {inoutVec[0], inoutVec[1], inoutVec[2], inoutVec[3]};
	
	CACHE_ALIGN float m[16];
	__mtx4_copynormalize_mtx4_float(m, inMtx);
	
	inoutVec[0] = (m[0] * v[0]) + (m[4] * v[1]) + (m[ 8] * v[2]);
	inoutVec[1] = (m[1] * v[0]) + (m[5] * v[1]) + (m[ 9] * v[2]);
	inoutVec[2] = (m[2] * v[0]) + (m[6] * v[1]) + (m[10] * v[2]);
	inoutVec[3] = v[3];
}

static FORCEINLINE void __mtx4_multiply_mtx4_float(float (&__restrict mtxA)[16], const s32 (&__restrict mtxB)[16])
{
	CACHE_ALIGN float a[16];
	CACHE_ALIGN float b[16];
	
	MatrixCopy(a, mtxA);
	MatrixCopy(b, mtxB);
	
	mtxA[ 0] = (a[ 0] * b[ 0]) + (a[ 4] * b[ 1]) + (a[ 8] * b[ 2]) + (a[12] * b[ 3]);
	mtxA[ 1] = (a[ 1] * b[ 0]) + (a[ 5] * b[ 1]) + (a[ 9] * b[ 2]) + (a[13] * b[ 3]);
	mtxA[ 2] = (a[ 2] * b[ 0]) + (a[ 6] * b[ 1]) + (a[10] * b[ 2]) + (a[14] * b[ 3]);
	mtxA[ 3] = (a[ 3] * b[ 0]) + (a[ 7] * b[ 1]) + (a[11] * b[ 2]) + (a[15] * b[ 3]);
	
	mtxA[ 4] = (a[ 0] * b[ 4]) + (a[ 4] * b[ 5]) + (a[ 8] * b[ 6]) + (a[12] * b[ 7]);
	mtxA[ 5] = (a[ 1] * b[ 4]) + (a[ 5] * b[ 5]) + (a[ 9] * b[ 6]) + (a[13] * b[ 7]);
	mtxA[ 6] = (a[ 2] * b[ 4]) + (a[ 6] * b[ 5]) + (a[10] * b[ 6]) + (a[14] * b[ 7]);
	mtxA[ 7] = (a[ 3] * b[ 4]) + (a[ 7] * b[ 5]) + (a[11] * b[ 6]) + (a[15] * b[ 7]);
	
	mtxA[ 8] = (a[ 0] * b[ 8]) + (a[ 4] * b[ 9]) + (a[ 8] * b[10]) + (a[12] * b[11]);
	mtxA[ 9] = (a[ 1] * b[ 8]) + (a[ 5] * b[ 9]) + (a[ 9] * b[10]) + (a[13] * b[11]);
	mtxA[10] = (a[ 2] * b[ 8]) + (a[ 6] * b[ 9]) + (a[10] * b[10]) + (a[14] * b[11]);
	mtxA[11] = (a[ 3] * b[ 8]) + (a[ 7] * b[ 9]) + (a[11] * b[10]) + (a[15] * b[11]);
	
	mtxA[12] = (a[ 0] * b[12]) + (a[ 4] * b[13]) + (a[ 8] * b[14]) + (a[12] * b[15]);
	mtxA[13] = (a[ 1] * b[12]) + (a[ 5] * b[13]) + (a[ 9] * b[14]) + (a[13] * b[15]);
	mtxA[14] = (a[ 2] * b[12]) + (a[ 6] * b[13]) + (a[10] * b[14]) + (a[14] * b[15]);
	mtxA[15] = (a[ 3] * b[12]) + (a[ 7] * b[13]) + (a[11] * b[14]) + (a[15] * b[15]);
}

static FORCEINLINE void __mtx4_scale_vec3_float(float (&__restrict inoutMtx)[16], const float (&__restrict inVec)[4])
{
	inoutMtx[ 0] *= inVec[0];
	inoutMtx[ 1] *= inVec[0];
	inoutMtx[ 2] *= inVec[0];
	inoutMtx[ 3] *= inVec[0];
	
	inoutMtx[ 4] *= inVec[1];
	inoutMtx[ 5] *= inVec[1];
	inoutMtx[ 6] *= inVec[1];
	inoutMtx[ 7] *= inVec[1];
	
	inoutMtx[ 8] *= inVec[2];
	inoutMtx[ 9] *= inVec[2];
	inoutMtx[10] *= inVec[2];
	inoutMtx[11] *= inVec[2];
}

static FORCEINLINE void __mtx4_translate_vec3_float(float (&__restrict inoutMtx)[16], const float (&__restrict inVec)[4])
{
	inoutMtx[12] = (inoutMtx[0] * inVec[0]) + (inoutMtx[4] * inVec[1]) + (inoutMtx[ 8] * inVec[2]);
	inoutMtx[13] = (inoutMtx[1] * inVec[0]) + (inoutMtx[5] * inVec[1]) + (inoutMtx[ 9] * inVec[2]);
	inoutMtx[14] = (inoutMtx[2] * inVec[0]) + (inoutMtx[6] * inVec[1]) + (inoutMtx[10] * inVec[2]);
	inoutMtx[15] = (inoutMtx[3] * inVec[0]) + (inoutMtx[7] * inVec[1]) + (inoutMtx[11] * inVec[2]);
}

// These SIMD functions may look fancy, but they still operate using floating-point, and therefore
// need to be obsoleted and removed. They exist for historical reasons, one of which is that they
// run on very old CPUs through plain ol' SSE. However, future geometry engine work will only be
// moving towards using the native NDS 20.12 fixed-point math, and so the fixed-point equivalent
// functions shall take precendence over these.

#ifdef ENABLE_SSE

#ifdef ENABLE_SSE2
static FORCEINLINE void __mtx4_copy_mtx4_float_SSE2(float (&__restrict outMtx)[16], const s32 (&__restrict inMtx)[16])
{
	_mm_store_ps( outMtx + 0, _mm_cvtepi32_ps( _mm_load_si128((v128s32 *)inMtx+0) ) );
	_mm_store_ps( outMtx + 4, _mm_cvtepi32_ps( _mm_load_si128((v128s32 *)inMtx+1) ) );
	_mm_store_ps( outMtx + 8, _mm_cvtepi32_ps( _mm_load_si128((v128s32 *)inMtx+2) ) );
	_mm_store_ps( outMtx +12, _mm_cvtepi32_ps( _mm_load_si128((v128s32 *)inMtx+3) ) );
}
#endif // ENABLE_SSE2

static FORCEINLINE void __mtx4_copynormalize_mtx4_float_SSE(float (&__restrict outMtx)[16], const s32 (&__restrict inMtx)[16])
{
#ifdef ENABLE_SSE2
	__m128 row[4] = {
		_mm_cvtepi32_ps( _mm_load_si128((v128s32 *)inMtx+0) ),
		_mm_cvtepi32_ps( _mm_load_si128((v128s32 *)inMtx+1) ),
		_mm_cvtepi32_ps( _mm_load_si128((v128s32 *)inMtx+2) ),
		_mm_cvtepi32_ps( _mm_load_si128((v128s32 *)inMtx+3) )
	};
#else
	__m128 row[4] = {
		_mm_setr_ps((float)inMtx[ 0], (float)inMtx[ 1], (float)inMtx[ 2], (float)inMtx[ 3]),
		_mm_setr_ps((float)inMtx[ 4], (float)inMtx[ 5], (float)inMtx[ 6], (float)inMtx[ 7]),
		_mm_setr_ps((float)inMtx[ 8], (float)inMtx[ 9], (float)inMtx[10], (float)inMtx[11]),
		_mm_setr_ps((float)inMtx[12], (float)inMtx[13], (float)inMtx[14], (float)inMtx[15])
	};
#endif // ENABLE_SSE2
	
	const __m128 normalize = _mm_set1_ps(1.0f/4096.0f);
	
	row[0] = _mm_mul_ps(row[0], normalize);
	_mm_store_ps(outMtx + 0, row[0]);
	row[1] = _mm_mul_ps(row[1], normalize);
	_mm_store_ps(outMtx + 4, row[1]);
	row[2] = _mm_mul_ps(row[2], normalize);
	_mm_store_ps(outMtx + 8, row[2]);
	row[3] = _mm_mul_ps(row[3], normalize);
	_mm_store_ps(outMtx +12, row[3]);
}

#ifdef ENABLE_SSE4_1
static FORCEINLINE float __vec4_dotproduct_vec4_float_SSE4(const float (&__restrict vecA)[4], const float (&__restrict vecB)[4])
{
	const __m128 a = _mm_load_ps(vecA);
	const __m128 b = _mm_load_ps(vecB);
	const __m128 sum = _mm_dp_ps(a, b, 0xF1);
	
	return _mm_cvtss_f32(sum);
}
#endif // ENABLE_SSE4_1

static FORCEINLINE void __vec4_multiply_mtx4_float_SSE(float (&__restrict inoutVec)[4], const s32 (&__restrict inMtx)[16])
{
#ifdef ENABLE_SSE2
	__m128 row[4] = {
		_mm_cvtepi32_ps( _mm_load_si128((v128s32 *)inMtx+0) ),
		_mm_cvtepi32_ps( _mm_load_si128((v128s32 *)inMtx+1) ),
		_mm_cvtepi32_ps( _mm_load_si128((v128s32 *)inMtx+2) ),
		_mm_cvtepi32_ps( _mm_load_si128((v128s32 *)inMtx+3) )
	};
#else
	__m128 row[4] = {
		_mm_setr_ps((float)inMtx[ 0], (float)inMtx[ 1], (float)inMtx[ 2], (float)inMtx[ 3]),
		_mm_setr_ps((float)inMtx[ 4], (float)inMtx[ 5], (float)inMtx[ 6], (float)inMtx[ 7]),
		_mm_setr_ps((float)inMtx[ 8], (float)inMtx[ 9], (float)inMtx[10], (float)inMtx[11]),
		_mm_setr_ps((float)inMtx[12], (float)inMtx[13], (float)inMtx[14], (float)inMtx[15])
	};
#endif // ENABLE_SSE2
	
	const __m128 normalize = _mm_set1_ps(1.0f/4096.0f);
	row[0] = _mm_mul_ps(row[0], normalize);
	row[1] = _mm_mul_ps(row[1], normalize);
	row[2] = _mm_mul_ps(row[2], normalize);
	row[3] = _mm_mul_ps(row[3], normalize);
	
	const __m128 inVec = _mm_load_ps(inoutVec);
	const __m128 v[4] = {
		_mm_shuffle_ps(inVec, inVec, 0x00),
		_mm_shuffle_ps(inVec, inVec, 0x55),
		_mm_shuffle_ps(inVec, inVec, 0xAA),
		_mm_shuffle_ps(inVec, inVec, 0xFF)
	};
	
	__m128 outVec;
	outVec =                     _mm_mul_ps(row[0], v[0]);
	outVec = _mm_add_ps( outVec, _mm_mul_ps(row[1], v[1]) );
	outVec = _mm_add_ps( outVec, _mm_mul_ps(row[2], v[2]) );
	outVec = _mm_add_ps( outVec, _mm_mul_ps(row[3], v[3]) );
	
	_mm_store_ps(inoutVec, outVec);
}

static FORCEINLINE void __vec3_multiply_mtx3_float_SSE(float (&__restrict inoutVec)[4], const s32 (&__restrict inMtx)[16])
{
#ifdef ENABLE_SSE2
	__m128 row[3] = {
		_mm_cvtepi32_ps( _mm_load_si128((v128s32 *)inMtx+0) ),
		_mm_cvtepi32_ps( _mm_load_si128((v128s32 *)inMtx+1) ),
		_mm_cvtepi32_ps( _mm_load_si128((v128s32 *)inMtx+2) )
	};
#else
	__m128 row[3] = {
		_mm_setr_ps((float)inMtx[ 0], (float)inMtx[ 1], (float)inMtx[ 2], (float)inMtx[ 3]),
		_mm_setr_ps((float)inMtx[ 4], (float)inMtx[ 5], (float)inMtx[ 6], (float)inMtx[ 7]),
		_mm_setr_ps((float)inMtx[ 8], (float)inMtx[ 9], (float)inMtx[10], (float)inMtx[11])
	};
#endif // ENABLE_SSE2
	
	const __m128 normalize = _mm_set1_ps(1.0f/4096.0f);
	row[0] = _mm_mul_ps(row[0], normalize);
	row[1] = _mm_mul_ps(row[1], normalize);
	row[2] = _mm_mul_ps(row[2], normalize);
	
	const __m128 inVec = _mm_load_ps(inoutVec);
	const __m128 v[3] = {
		_mm_shuffle_ps(inVec, inVec, 0x00),
		_mm_shuffle_ps(inVec, inVec, 0x55),
		_mm_shuffle_ps(inVec, inVec, 0xAA)
	};
	
	__m128 outVec;
	outVec =                     _mm_mul_ps(row[0], v[0]);
	outVec = _mm_add_ps( outVec, _mm_mul_ps(row[1], v[1]) );
	outVec = _mm_add_ps( outVec, _mm_mul_ps(row[2], v[2]) );
	
	const float retainedElement = inoutVec[3];
	_mm_store_ps(inoutVec, outVec);
	inoutVec[3] = retainedElement;
}

static FORCEINLINE void __mtx4_multiply_mtx4_float_SSE(float (&__restrict mtxA)[16], const s32 (&__restrict mtxB)[16])
{
#ifdef ENABLE_SSE2
	__m128 rowB[4] = {
		_mm_cvtepi32_ps( _mm_load_si128((v128s32 *)mtxB + 0) ),
		_mm_cvtepi32_ps( _mm_load_si128((v128s32 *)mtxB + 1) ),
		_mm_cvtepi32_ps( _mm_load_si128((v128s32 *)mtxB + 2) ),
		_mm_cvtepi32_ps( _mm_load_si128((v128s32 *)mtxB + 3) )
	};
#else
	__m128 rowB[4] = {
		_mm_setr_ps((float)mtxB[ 0], (float)mtxB[ 1], (float)mtxB[ 2], (float)mtxB[ 3]),
		_mm_setr_ps((float)mtxB[ 4], (float)mtxB[ 5], (float)mtxB[ 6], (float)mtxB[ 7]),
		_mm_setr_ps((float)mtxB[ 8], (float)mtxB[ 9], (float)mtxB[10], (float)mtxB[11]),
		_mm_setr_ps((float)mtxB[12], (float)mtxB[13], (float)mtxB[14], (float)mtxB[15])
	};
#endif // ENABLE_SSE2
	
	const __m128 normalize = _mm_set1_ps(1.0f/4096.0f);
	rowB[0] = _mm_mul_ps(rowB[0], normalize);
	rowB[1] = _mm_mul_ps(rowB[1], normalize);
	rowB[2] = _mm_mul_ps(rowB[2], normalize);
	rowB[3] = _mm_mul_ps(rowB[3], normalize);
	
	const __m128 rowA[4] = {
		_mm_load_ps(mtxA + 0),
		_mm_load_ps(mtxA + 4),
		_mm_load_ps(mtxA + 8),
		_mm_load_ps(mtxA +12)
	};
	
	__m128 vecB[4];
	__m128 outRow;
	
#define CALCULATE_MATRIX_ROW_FLOAT_SSE(indexRowB) \
	vecB[0] = _mm_shuffle_ps(rowB[(indexRowB)], rowB[(indexRowB)], 0x00);\
	vecB[1] = _mm_shuffle_ps(rowB[(indexRowB)], rowB[(indexRowB)], 0x55);\
	vecB[2] = _mm_shuffle_ps(rowB[(indexRowB)], rowB[(indexRowB)], 0xAA);\
	vecB[3] = _mm_shuffle_ps(rowB[(indexRowB)], rowB[(indexRowB)], 0xFF);\
	outRow =                     _mm_mul_ps(rowA[0], vecB[0]);\
	outRow = _mm_add_ps( outRow, _mm_mul_ps(rowA[1], vecB[1]) );\
	outRow = _mm_add_ps( outRow, _mm_mul_ps(rowA[2], vecB[2]) );\
	outRow = _mm_add_ps( outRow, _mm_mul_ps(rowA[3], vecB[3]) );
	
	CALCULATE_MATRIX_ROW_FLOAT_SSE(0);
	_mm_store_ps(mtxA +  0, outRow);
	
	CALCULATE_MATRIX_ROW_FLOAT_SSE(1);
	_mm_store_ps(mtxA +  4, outRow);
	
	CALCULATE_MATRIX_ROW_FLOAT_SSE(2);
	_mm_store_ps(mtxA +  8, outRow);
	
	CALCULATE_MATRIX_ROW_FLOAT_SSE(3);
	_mm_store_ps(mtxA + 12, outRow);
}

static FORCEINLINE void __mtx4_scale_vec3_float_SSE(float (&__restrict inoutMtx)[16], const float (&__restrict inVec)[4])
{
	const __m128 inVec_m128 = _mm_load_ps(inVec);
	const __m128 v[3] = {
		_mm_shuffle_ps(inVec_m128, inVec_m128, 0x00),
		_mm_shuffle_ps(inVec_m128, inVec_m128, 0x55),
		_mm_shuffle_ps(inVec_m128, inVec_m128, 0xAA)
	};
	
	_mm_store_ps( inoutMtx, _mm_mul_ps( _mm_load_ps(inoutMtx+0), v[0] ) );
	_mm_store_ps( inoutMtx, _mm_mul_ps( _mm_load_ps(inoutMtx+4), v[1] ) );
	_mm_store_ps( inoutMtx, _mm_mul_ps( _mm_load_ps(inoutMtx+8), v[2] ) );
}

static FORCEINLINE void __mtx4_translate_vec3_float_SSE(float (&__restrict inoutMtx)[16], const float (&__restrict inVec)[4])
{
	const __m128 inVec_m128 = _mm_load_ps(inVec);
	const __m128 v[3] = {
		_mm_shuffle_ps(inVec_m128, inVec_m128, 0x00),
		_mm_shuffle_ps(inVec_m128, inVec_m128, 0x55),
		_mm_shuffle_ps(inVec_m128, inVec_m128, 0xAA)
	};
	
	const __m128 row[3] = {
		_mm_load_ps(inoutMtx + 0),
		_mm_load_ps(inoutMtx + 4),
		_mm_load_ps(inoutMtx + 8),
	};
	
	__m128 outVec;
	outVec =                     _mm_mul_ps(row[0], v[0]);
	outVec = _mm_add_ps( outVec, _mm_mul_ps(row[1], v[1]) );
	outVec = _mm_add_ps( outVec, _mm_mul_ps(row[2], v[2]) );
	
	_mm_store_ps(inoutMtx+12, outVec);
}

#endif // ENABLE_SSE

#ifdef ENABLE_NEON_A64

static FORCEINLINE void __mtx4_copy_mtx4_float_NEON(float (&__restrict outMtx)[16], const s32 (&__restrict inMtx)[16])
{
	int32x4x4_t m = vld1q_s32_x4(inMtx);
	float32x4x4_t f;
	
	f.val[0] = vcvtq_f32_s32(m.val[0]);
	f.val[1] = vcvtq_f32_s32(m.val[1]);
	f.val[2] = vcvtq_f32_s32(m.val[2]);
	f.val[3] = vcvtq_f32_s32(m.val[3]);
	
	vst1q_f32_x4(outMtx, f);
}

static FORCEINLINE void __mtx4_copynormalize_mtx4_float_NEON(float (&__restrict outMtx)[16], const s32 (&__restrict inMtx)[16])
{
	const int32x4x4_t m = vld1q_s32_x4(inMtx);
	float32x4x4_t f;
	
	f.val[0] = vcvtq_n_f32_s32(m.val[0], 12);
	f.val[1] = vcvtq_n_f32_s32(m.val[1], 12);
	f.val[2] = vcvtq_n_f32_s32(m.val[2], 12);
	f.val[3] = vcvtq_n_f32_s32(m.val[3], 12);
	
	vst1q_f32_x4(outMtx, f);
}

static FORCEINLINE float __vec4_dotproduct_vec4_float_NEON(const float (&__restrict vecA)[4], const float (&__restrict vecB)[4])
{
	const float32x4_t a = vld1q_f32(vecA);
	const float32x4_t b = vld1q_f32(vecB);
	const float32x4_t mul = vmulq_f32(a, b);
	const float sum = vaddvq_f32(mul);
	
	return sum;
}

static FORCEINLINE void __vec4_multiply_mtx4_float_NEON(float (&__restrict inoutVec)[4], const s32 (&__restrict inMtx)[16])
{
	const int32x4x4_t m = vld1q_s32_x4(inMtx);
	
	float32x4x4_t row;
	row.val[0] = vcvtq_n_f32_s32(m.val[0], 12);
	row.val[1] = vcvtq_n_f32_s32(m.val[1], 12);
	row.val[2] = vcvtq_n_f32_s32(m.val[2], 12);
	row.val[3] = vcvtq_n_f32_s32(m.val[3], 12);
	
	const float32x4_t inVec = vld1q_f32(inoutVec);
	const float32x4_t v[4] = {
		vdupq_laneq_f32(inVec, 0),
		vdupq_laneq_f32(inVec, 1),
		vdupq_laneq_f32(inVec, 2),
		vdupq_laneq_f32(inVec, 3)
	};
	
	float32x4_t outVec;
	outVec =         vmulq_f32( row.val[0], v[0] );
	outVec = vfmaq_f32( outVec, row.val[1], v[1] );
	outVec = vfmaq_f32( outVec, row.val[2], v[2] );
	outVec = vfmaq_f32( outVec, row.val[3], v[3] );
	
	vst1q_f32(inoutVec, outVec);
}

static FORCEINLINE void __vec3_multiply_mtx3_float_NEON(float (&__restrict inoutVec)[4], const s32 (&__restrict inMtx)[16])
{
	const int32x4x3_t m = vld1q_s32_x3(inMtx);
	
	float32x4x3_t row;
	row.val[0] = vcvtq_n_f32_s32(m.val[0], 12);
	row.val[1] = vcvtq_n_f32_s32(m.val[1], 12);
	row.val[2] = vcvtq_n_f32_s32(m.val[2], 12);
	
	const float32x4_t inVec = vld1q_f32(inoutVec);
	const float32x4_t v[3] = {
		vdupq_laneq_f32(inVec, 0),
		vdupq_laneq_f32(inVec, 1),
		vdupq_laneq_f32(inVec, 2)
	};
	
	float32x4_t outVec;
	outVec =         vmulq_f32( row.val[0], v[0] );
	outVec = vfmaq_f32( outVec, row.val[1], v[1] );
	outVec = vfmaq_f32( outVec, row.val[2], v[2] );
	outVec = vcopyq_laneq_f32(outVec, 3, inVec, 3);
	
	vst1q_f32(inoutVec, outVec);
}

static FORCEINLINE void __mtx4_multiply_mtx4_float_NEON(float (&__restrict mtxA)[16], const s32 (&__restrict mtxB)[16])
{
	const int32x4x4_t b = vld1q_s32_x4(mtxB);
	
	float32x4x4_t rowB;
	rowB.val[0] = vcvtq_n_f32_s32(b.val[0], 12);
	rowB.val[1] = vcvtq_n_f32_s32(b.val[1], 12);
	rowB.val[2] = vcvtq_n_f32_s32(b.val[2], 12);
	rowB.val[3] = vcvtq_n_f32_s32(b.val[3], 12);
	
	float32x4x4_t rowA = vld1q_f32_x4(mtxA);
	
	float32x4_t vecB[4];
	float32x4_t outRow;
	
#define CALCULATE_MATRIX_ROW_FLOAT_NEON(indexRowB) \
	vecB[0] = vdupq_laneq_f32(rowB.val[(indexRowB)], 0);\
	vecB[1] = vdupq_laneq_f32(rowB.val[(indexRowB)], 1);\
	vecB[2] = vdupq_laneq_f32(rowB.val[(indexRowB)], 2);\
	vecB[3] = vdupq_laneq_f32(rowB.val[(indexRowB)], 3);\
	outRow =         vmulq_f32( rowA.val[0], vecB[0] );\
	outRow = vfmaq_f32( outRow, rowA.val[1], vecB[1] );\
	outRow = vfmaq_f32( outRow, rowA.val[2], vecB[2] );\
	outRow = vfmaq_f32( outRow, rowA.val[3], vecB[3] );
	
	CALCULATE_MATRIX_ROW_FLOAT_NEON(0);
	vst1q_f32(mtxA + 0, outRow);
	
	CALCULATE_MATRIX_ROW_FLOAT_NEON(1);
	vst1q_f32(mtxA + 4, outRow);
	
	CALCULATE_MATRIX_ROW_FLOAT_NEON(2);
	vst1q_f32(mtxA + 8, outRow);
	
	CALCULATE_MATRIX_ROW_FLOAT_NEON(3);
	vst1q_f32(mtxA +12, outRow);
}

static FORCEINLINE void __mtx4_scale_vec3_float_NEON(float (&__restrict inoutMtx)[16], const float (&__restrict inVec)[4])
{
	const float32x4_t inVec128 = vld1q_f32(inVec);
	const float32x4_t v[3] = {
		vdupq_laneq_f32(inVec128, 0),
		vdupq_laneq_f32(inVec128, 1),
		vdupq_laneq_f32(inVec128, 2)
	};
	
	float32x4x3_t row = vld1q_f32_x3(inoutMtx);
	row.val[0] = vmulq_f32(row.val[0], v[0]);
	row.val[1] = vmulq_f32(row.val[1], v[1]);
	row.val[2] = vmulq_f32(row.val[2], v[2]);
	
	vst1q_f32_x3(inoutMtx, row);
}

static FORCEINLINE void __mtx4_translate_vec3_float_NEON(float (&__restrict inoutMtx)[16], const float (&__restrict inVec)[4])
{
	const float32x4_t inVec128 = vld1q_f32(inVec);
	const float32x4_t v[3] = {
		vdupq_laneq_f32(inVec128, 0),
		vdupq_laneq_f32(inVec128, 1),
		vdupq_laneq_f32(inVec128, 2)
	};
	
	const float32x4x3_t row = vld1q_f32_x3(inoutMtx);
	
	float32x4_t outVec;
	outVec =         vmulq_f32( row.val[0], v[0] );
	outVec = vfmaq_f32( outVec, row.val[1], v[1] );
	outVec = vfmaq_f32( outVec, row.val[2], v[2] );
	
	vst1q_f32(inoutMtx + 12, outVec);
}

#endif // ENABLE_NEON_A64

static FORCEINLINE s32 ___s32_saturate_shiftdown_accum64_fixed(s64 inAccum)
{
	if (inAccum > (s64)0x000007FFFFFFFFFFULL)
	{
		return (s32)0x7FFFFFFFU;
	}
	else if (inAccum < (s64)0xFFFFF80000000000ULL)
	{
		return (s32)0x80000000U;
	}
	
	return sfx32_shiftdown(inAccum);
}

static FORCEINLINE s32 __vec4_dotproduct_vec4_fixed(const s32 (&__restrict vecA)[4], const s32 (&__restrict vecB)[4])
{
	return ___s32_saturate_shiftdown_accum64_fixed( fx32_mul(vecA[0],vecB[0]) + fx32_mul(vecA[1],vecB[1]) + fx32_mul(vecA[2],vecB[2]) + fx32_mul(vecA[3],vecB[3]) );
}

static FORCEINLINE void __vec4_multiply_mtx4_fixed(s32 (&__restrict inoutVec)[4], const s32 (&__restrict inMtx)[16])
{
	//saturation logic is most carefully tested by:
	//+ spectrobes beyond the portals excavation blower and drill tools: sets very large overflowing +x,+y in the modelview matrix to push things offscreen
	//You can see this happening quite clearly: vertices will get translated to extreme values and overflow from a 7FFF-like to an 8000-like
	//but if it's done wrongly, you can get bugs in:
	//+ kingdom hearts re-coded: first conversation with cast characters will place them oddly with something overflowing to about 0xA???????
	
	//other test cases that cropped up during this development, but are probably not actually related to this after all
	//+ SM64: outside castle skybox
	//+ NSMB: mario head screen wipe
	
	const CACHE_ALIGN s32 v[4] = {inoutVec[0], inoutVec[1], inoutVec[2], inoutVec[3]};
	
	inoutVec[0] = ___s32_saturate_shiftdown_accum64_fixed( fx32_mul(inMtx[0],v[0]) + fx32_mul(inMtx[4],v[1]) + fx32_mul(inMtx[ 8],v[2]) + fx32_mul(inMtx[12],v[3]) );
	inoutVec[1] = ___s32_saturate_shiftdown_accum64_fixed( fx32_mul(inMtx[1],v[0]) + fx32_mul(inMtx[5],v[1]) + fx32_mul(inMtx[ 9],v[2]) + fx32_mul(inMtx[13],v[3]) );
	inoutVec[2] = ___s32_saturate_shiftdown_accum64_fixed( fx32_mul(inMtx[2],v[0]) + fx32_mul(inMtx[6],v[1]) + fx32_mul(inMtx[10],v[2]) + fx32_mul(inMtx[14],v[3]) );
	inoutVec[3] = ___s32_saturate_shiftdown_accum64_fixed( fx32_mul(inMtx[3],v[0]) + fx32_mul(inMtx[7],v[1]) + fx32_mul(inMtx[11],v[2]) + fx32_mul(inMtx[15],v[3]) );
}

static FORCEINLINE void __vec3_multiply_mtx3_fixed(s32 (&__restrict inoutVec)[4], const s32 (&__restrict inMtx)[16])
{
	const CACHE_ALIGN s32 v[4] = {inoutVec[0], inoutVec[1], inoutVec[2], inoutVec[3]};
	
	inoutVec[0] = ___s32_saturate_shiftdown_accum64_fixed( fx32_mul(inMtx[0],v[0]) + fx32_mul(inMtx[4],v[1]) + fx32_mul(inMtx[ 8],v[2]) );
	inoutVec[1] = ___s32_saturate_shiftdown_accum64_fixed( fx32_mul(inMtx[1],v[0]) + fx32_mul(inMtx[5],v[1]) + fx32_mul(inMtx[ 9],v[2]) );
	inoutVec[2] = ___s32_saturate_shiftdown_accum64_fixed( fx32_mul(inMtx[2],v[0]) + fx32_mul(inMtx[6],v[1]) + fx32_mul(inMtx[10],v[2]) );
	inoutVec[3] = v[3];
}

static FORCEINLINE void __mtx4_multiply_mtx4_fixed(s32 (&__restrict mtxA)[16], const s32 (&__restrict mtxB)[16])
{
	CACHE_ALIGN s32 a[16];
	MatrixCopy(a, mtxA);
	
	// We can't saturate the accumulated results here because it breaks
	// character conversations in "Kingdom Hears: Re-coded", causing the
	// characters to disappear. Therefore, we will simply do a standalone
	// shiftdown, and that's it.
	
	mtxA[ 0] = sfx32_shiftdown( fx32_mul(a[ 0],mtxB[ 0]) + fx32_mul(a[ 4],mtxB[ 1]) + fx32_mul(a[ 8],mtxB[ 2]) + fx32_mul(a[12],mtxB[ 3]) );
	mtxA[ 1] = sfx32_shiftdown( fx32_mul(a[ 1],mtxB[ 0]) + fx32_mul(a[ 5],mtxB[ 1]) + fx32_mul(a[ 9],mtxB[ 2]) + fx32_mul(a[13],mtxB[ 3]) );
	mtxA[ 2] = sfx32_shiftdown( fx32_mul(a[ 2],mtxB[ 0]) + fx32_mul(a[ 6],mtxB[ 1]) + fx32_mul(a[10],mtxB[ 2]) + fx32_mul(a[14],mtxB[ 3]) );
	mtxA[ 3] = sfx32_shiftdown( fx32_mul(a[ 3],mtxB[ 0]) + fx32_mul(a[ 7],mtxB[ 1]) + fx32_mul(a[11],mtxB[ 2]) + fx32_mul(a[15],mtxB[ 3]) );
	
	mtxA[ 4] = sfx32_shiftdown( fx32_mul(a[ 0],mtxB[ 4]) + fx32_mul(a[ 4],mtxB[ 5]) + fx32_mul(a[ 8],mtxB[ 6]) + fx32_mul(a[12],mtxB[ 7]) );
	mtxA[ 5] = sfx32_shiftdown( fx32_mul(a[ 1],mtxB[ 4]) + fx32_mul(a[ 5],mtxB[ 5]) + fx32_mul(a[ 9],mtxB[ 6]) + fx32_mul(a[13],mtxB[ 7]) );
	mtxA[ 6] = sfx32_shiftdown( fx32_mul(a[ 2],mtxB[ 4]) + fx32_mul(a[ 6],mtxB[ 5]) + fx32_mul(a[10],mtxB[ 6]) + fx32_mul(a[14],mtxB[ 7]) );
	mtxA[ 7] = sfx32_shiftdown( fx32_mul(a[ 3],mtxB[ 4]) + fx32_mul(a[ 7],mtxB[ 5]) + fx32_mul(a[11],mtxB[ 6]) + fx32_mul(a[15],mtxB[ 7]) );
	
	mtxA[ 8] = sfx32_shiftdown( fx32_mul(a[ 0],mtxB[ 8]) + fx32_mul(a[ 4],mtxB[ 9]) + fx32_mul(a[ 8],mtxB[10]) + fx32_mul(a[12],mtxB[11]) );
	mtxA[ 9] = sfx32_shiftdown( fx32_mul(a[ 1],mtxB[ 8]) + fx32_mul(a[ 5],mtxB[ 9]) + fx32_mul(a[ 9],mtxB[10]) + fx32_mul(a[13],mtxB[11]) );
	mtxA[10] = sfx32_shiftdown( fx32_mul(a[ 2],mtxB[ 8]) + fx32_mul(a[ 6],mtxB[ 9]) + fx32_mul(a[10],mtxB[10]) + fx32_mul(a[14],mtxB[11]) );
	mtxA[11] = sfx32_shiftdown( fx32_mul(a[ 3],mtxB[ 8]) + fx32_mul(a[ 7],mtxB[ 9]) + fx32_mul(a[11],mtxB[10]) + fx32_mul(a[15],mtxB[11]) );
	
	mtxA[12] = sfx32_shiftdown( fx32_mul(a[ 0],mtxB[12]) + fx32_mul(a[ 4],mtxB[13]) + fx32_mul(a[ 8],mtxB[14]) + fx32_mul(a[12],mtxB[15]) );
	mtxA[13] = sfx32_shiftdown( fx32_mul(a[ 1],mtxB[12]) + fx32_mul(a[ 5],mtxB[13]) + fx32_mul(a[ 9],mtxB[14]) + fx32_mul(a[13],mtxB[15]) );
	mtxA[14] = sfx32_shiftdown( fx32_mul(a[ 2],mtxB[12]) + fx32_mul(a[ 6],mtxB[13]) + fx32_mul(a[10],mtxB[14]) + fx32_mul(a[14],mtxB[15]) );
	mtxA[15] = sfx32_shiftdown( fx32_mul(a[ 3],mtxB[12]) + fx32_mul(a[ 7],mtxB[13]) + fx32_mul(a[11],mtxB[14]) + fx32_mul(a[15],mtxB[15]) );
}

static FORCEINLINE void __mtx4_scale_vec3_fixed(s32 (&__restrict inoutMtx)[16], const s32 (&__restrict inVec)[4])
{
	inoutMtx[ 0] = ___s32_saturate_shiftdown_accum64_fixed( fx32_mul(inoutMtx[ 0], inVec[0]) );
	inoutMtx[ 1] = ___s32_saturate_shiftdown_accum64_fixed( fx32_mul(inoutMtx[ 1], inVec[0]) );
	inoutMtx[ 2] = ___s32_saturate_shiftdown_accum64_fixed( fx32_mul(inoutMtx[ 2], inVec[0]) );
	inoutMtx[ 3] = ___s32_saturate_shiftdown_accum64_fixed( fx32_mul(inoutMtx[ 3], inVec[0]) );
	
	inoutMtx[ 4] = ___s32_saturate_shiftdown_accum64_fixed( fx32_mul(inoutMtx[ 4], inVec[1]) );
	inoutMtx[ 5] = ___s32_saturate_shiftdown_accum64_fixed( fx32_mul(inoutMtx[ 5], inVec[1]) );
	inoutMtx[ 6] = ___s32_saturate_shiftdown_accum64_fixed( fx32_mul(inoutMtx[ 6], inVec[1]) );
	inoutMtx[ 7] = ___s32_saturate_shiftdown_accum64_fixed( fx32_mul(inoutMtx[ 7], inVec[1]) );
	
	inoutMtx[ 8] = ___s32_saturate_shiftdown_accum64_fixed( fx32_mul(inoutMtx[ 8], inVec[2]) );
	inoutMtx[ 9] = ___s32_saturate_shiftdown_accum64_fixed( fx32_mul(inoutMtx[ 9], inVec[2]) );
	inoutMtx[10] = ___s32_saturate_shiftdown_accum64_fixed( fx32_mul(inoutMtx[10], inVec[2]) );
	inoutMtx[11] = ___s32_saturate_shiftdown_accum64_fixed( fx32_mul(inoutMtx[11], inVec[2]) );
}

static FORCEINLINE void __mtx4_translate_vec3_fixed(s32 (&__restrict inoutMtx)[16], const s32 (&__restrict inVec)[4])
{
	inoutMtx[12] = ___s32_saturate_shiftdown_accum64_fixed( fx32_mul(inoutMtx[0], inVec[0]) + fx32_mul(inoutMtx[4], inVec[1]) + fx32_mul(inoutMtx[ 8], inVec[2]) + fx32_shiftup(inoutMtx[12]) );
	inoutMtx[13] = ___s32_saturate_shiftdown_accum64_fixed( fx32_mul(inoutMtx[1], inVec[0]) + fx32_mul(inoutMtx[5], inVec[1]) + fx32_mul(inoutMtx[ 9], inVec[2]) + fx32_shiftup(inoutMtx[13]) );
	inoutMtx[14] = ___s32_saturate_shiftdown_accum64_fixed( fx32_mul(inoutMtx[2], inVec[0]) + fx32_mul(inoutMtx[6], inVec[1]) + fx32_mul(inoutMtx[10], inVec[2]) + fx32_shiftup(inoutMtx[14]) );
	inoutMtx[15] = ___s32_saturate_shiftdown_accum64_fixed( fx32_mul(inoutMtx[3], inVec[0]) + fx32_mul(inoutMtx[7], inVec[1]) + fx32_mul(inoutMtx[11], inVec[2]) + fx32_shiftup(inoutMtx[15]) );
}

#ifdef ENABLE_SSE4_1

static FORCEINLINE void ___s32_saturate_shiftdown_accum64_fixed_SSE4(__m128i &inoutAccum)
{
	v128u8 outVecMask;
	
#if defined(ENABLE_SSE4_2)
	outVecMask = _mm_cmpgt_epi64( inoutAccum, _mm_set1_epi64x((s64)0x000007FFFFFFFFFFULL) );
	inoutAccum = _mm_blendv_epi8( inoutAccum, _mm_set1_epi64x((s64)0x000007FFFFFFFFFFULL), outVecMask );
	
	outVecMask = _mm_cmpgt_epi64( _mm_set1_epi64x((s64)0xFFFFF80000000000ULL), inoutAccum );
	inoutAccum = _mm_blendv_epi8( inoutAccum, _mm_set1_epi64x((s64)0xFFFFF80000000000ULL), outVecMask );
#else
	const v128u8 outVecSignMask = _mm_cmpeq_epi64( _mm_and_si128(inoutAccum, _mm_set1_epi64x((s64)0x8000000000000000ULL)), _mm_setzero_si128() );
	
	outVecMask = _mm_cmpeq_epi64( _mm_and_si128(inoutAccum, _mm_set1_epi64x((s64)0x7FFFF80000000000ULL)), _mm_setzero_si128() );
	const v128u32 outVecPos = _mm_blendv_epi8( _mm_set1_epi64x((s64)0x000007FFFFFFFFFFULL), inoutAccum, outVecMask );
	
	const v128u32 outVecFlipped = _mm_xor_si128(inoutAccum, _mm_set1_epi8(0xFF));
	outVecMask = _mm_cmpeq_epi64( _mm_and_si128(outVecFlipped, _mm_set1_epi64x((s64)0x7FFFF80000000000ULL)), _mm_setzero_si128() );
	const v128u32 outVecNeg = _mm_blendv_epi8( _mm_set1_epi64x((s64)0xFFFFF80000000000ULL), inoutAccum, outVecMask );
	
	inoutAccum = _mm_blendv_epi8(outVecNeg, outVecPos, outVecSignMask);
#endif // ENABLE_SSE4_2
	
	inoutAccum = _mm_srli_epi64(inoutAccum, 12);
	inoutAccum = _mm_shuffle_epi32(inoutAccum, 0xD8);
}

static FORCEINLINE void __vec4_multiply_mtx4_fixed_SSE4(s32 (&__restrict inoutVec)[4], const s32 (&__restrict inMtx)[16])
{
	const v128s32 inVec = _mm_load_si128((v128s32 *)inoutVec);
	
	const v128s32 v[4] = {
		_mm_shuffle_epi32(inVec, 0x00),
		_mm_shuffle_epi32(inVec, 0x55),
		_mm_shuffle_epi32(inVec, 0xAA),
		_mm_shuffle_epi32(inVec, 0xFF)
	};
	
	const v128s32 row[4] = {
		_mm_load_si128((v128s32 *)inMtx + 0),
		_mm_load_si128((v128s32 *)inMtx + 1),
		_mm_load_si128((v128s32 *)inMtx + 2),
		_mm_load_si128((v128s32 *)inMtx + 3)
	};
	
	const v128s32 rowLo[4] = {
		_mm_shuffle_epi32(row[0], 0x50),
		_mm_shuffle_epi32(row[1], 0x50),
		_mm_shuffle_epi32(row[2], 0x50),
		_mm_shuffle_epi32(row[3], 0x50)
	};
	
	const v128s32 rowHi[4] = {
		_mm_shuffle_epi32(row[0], 0xFA),
		_mm_shuffle_epi32(row[1], 0xFA),
		_mm_shuffle_epi32(row[2], 0xFA),
		_mm_shuffle_epi32(row[3], 0xFA)
	};
	
	v128s32 outVecLo =                  _mm_mul_epi32(rowLo[0], v[0]);
	outVecLo = _mm_add_epi64( outVecLo, _mm_mul_epi32(rowLo[1], v[1]) );
	outVecLo = _mm_add_epi64( outVecLo, _mm_mul_epi32(rowLo[2], v[2]) );
	outVecLo = _mm_add_epi64( outVecLo, _mm_mul_epi32(rowLo[3], v[3]) );
	___s32_saturate_shiftdown_accum64_fixed_SSE4(outVecLo);
	
	v128s32 outVecHi =                  _mm_mul_epi32(rowHi[0], v[0]);
	outVecHi = _mm_add_epi64( outVecHi, _mm_mul_epi32(rowHi[1], v[1]) );
	outVecHi = _mm_add_epi64( outVecHi, _mm_mul_epi32(rowHi[2], v[2]) );
	outVecHi = _mm_add_epi64( outVecHi, _mm_mul_epi32(rowHi[3], v[3]) );
	___s32_saturate_shiftdown_accum64_fixed_SSE4(outVecHi);
	
	_mm_store_si128( (v128s32 *)inoutVec, _mm_unpacklo_epi64(outVecLo, outVecHi) );
}

static FORCEINLINE void __vec3_multiply_mtx3_fixed_SSE4(s32 (&__restrict inoutVec)[4], const s32 (&__restrict inMtx)[16])
{
	const v128s32 inVec = _mm_load_si128((v128s32 *)inoutVec);
	
	const v128s32 v[3] = {
		_mm_shuffle_epi32(inVec, 0x00),
		_mm_shuffle_epi32(inVec, 0x55),
		_mm_shuffle_epi32(inVec, 0xAA)
	};
	
	const v128s32 row[3] = {
		_mm_load_si128((v128s32 *)inMtx + 0),
		_mm_load_si128((v128s32 *)inMtx + 1),
		_mm_load_si128((v128s32 *)inMtx + 2)
	};
	
	const v128s32 rowLo[4] = {
		_mm_shuffle_epi32(row[0], 0x50),
		_mm_shuffle_epi32(row[1], 0x50),
		_mm_shuffle_epi32(row[2], 0x50)
	};
	
	const v128s32 rowHi[4] = {
		_mm_shuffle_epi32(row[0], 0xFA),
		_mm_shuffle_epi32(row[1], 0xFA),
		_mm_shuffle_epi32(row[2], 0xFA)
	};
	
	v128s32 outVecLo =                  _mm_mul_epi32(rowLo[0], v[0]);
	outVecLo = _mm_add_epi64( outVecLo, _mm_mul_epi32(rowLo[1], v[1]) );
	outVecLo = _mm_add_epi64( outVecLo, _mm_mul_epi32(rowLo[2], v[2]) );
	___s32_saturate_shiftdown_accum64_fixed_SSE4(outVecLo);
	
	v128s32 outVecHi =                  _mm_mul_epi32(rowHi[0], v[0]);
	outVecHi = _mm_add_epi64( outVecHi, _mm_mul_epi32(rowHi[1], v[1]) );
	outVecHi = _mm_add_epi64( outVecHi, _mm_mul_epi32(rowHi[2], v[2]) );
	___s32_saturate_shiftdown_accum64_fixed_SSE4(outVecHi);
	
	v128s32 outVec = _mm_unpacklo_epi64(outVecLo, outVecHi);
	outVec = _mm_blend_epi16(outVec, inVec, 0xC0);
	
	_mm_store_si128((v128s32 *)inoutVec, outVec);
}

static FORCEINLINE void __mtx4_multiply_mtx4_fixed_SSE4(s32 (&__restrict mtxA)[16], const s32 (&__restrict mtxB)[16])
{
	const v128s32 rowA[4] = {
		_mm_load_si128((v128s32 *)(mtxA + 0)),
		_mm_load_si128((v128s32 *)(mtxA + 4)),
		_mm_load_si128((v128s32 *)(mtxA + 8)),
		_mm_load_si128((v128s32 *)(mtxA +12))
	};
	
	const v128s32 rowB[4] = {
		_mm_load_si128((v128s32 *)(mtxB + 0)),
		_mm_load_si128((v128s32 *)(mtxB + 4)),
		_mm_load_si128((v128s32 *)(mtxB + 8)),
		_mm_load_si128((v128s32 *)(mtxB +12))
	};
	
	const v128s32 rowLo[4] = {
		_mm_shuffle_epi32(rowA[0], 0x50),
		_mm_shuffle_epi32(rowA[1], 0x50),
		_mm_shuffle_epi32(rowA[2], 0x50),
		_mm_shuffle_epi32(rowA[3], 0x50)
	};
	
	const v128s32 rowHi[4] = {
		_mm_shuffle_epi32(rowA[0], 0xFA),
		_mm_shuffle_epi32(rowA[1], 0xFA),
		_mm_shuffle_epi32(rowA[2], 0xFA),
		_mm_shuffle_epi32(rowA[3], 0xFA)
	};
	
	v128s32 outVecLo;
	v128s32 outVecHi;
	v128s32 v[4];
	
#define CALCULATE_MATRIX_ROW_FIXED_SSE4(indexRowB) \
	v[0] = _mm_shuffle_epi32(rowB[(indexRowB)], 0x00);\
	v[1] = _mm_shuffle_epi32(rowB[(indexRowB)], 0x55);\
	v[2] = _mm_shuffle_epi32(rowB[(indexRowB)], 0xAA);\
	v[3] = _mm_shuffle_epi32(rowB[(indexRowB)], 0xFF);\
	outVecLo =                          _mm_mul_epi32(rowLo[0], v[0]);\
	outVecLo = _mm_add_epi64( outVecLo, _mm_mul_epi32(rowLo[1], v[1]) );\
	outVecLo = _mm_add_epi64( outVecLo, _mm_mul_epi32(rowLo[2], v[2]) );\
	outVecLo = _mm_add_epi64( outVecLo, _mm_mul_epi32(rowLo[3], v[3]) );\
	outVecLo = _mm_srli_epi64(outVecLo, 12);\
	outVecLo = _mm_shuffle_epi32(outVecLo, 0xD8);\
	outVecHi =                          _mm_mul_epi32(rowHi[0], v[0]);\
	outVecHi = _mm_add_epi64( outVecHi, _mm_mul_epi32(rowHi[1], v[1]) );\
	outVecHi = _mm_add_epi64( outVecHi, _mm_mul_epi32(rowHi[2], v[2]) );\
	outVecHi = _mm_add_epi64( outVecHi, _mm_mul_epi32(rowHi[3], v[3]) );\
	outVecHi = _mm_srli_epi64(outVecHi, 12);\
	outVecHi = _mm_shuffle_epi32(outVecHi, 0xD8);
	
	CALCULATE_MATRIX_ROW_FIXED_SSE4(0);
	_mm_store_si128( (v128s32 *)(mtxA + 0), _mm_unpacklo_epi64(outVecLo, outVecHi) );
	
	CALCULATE_MATRIX_ROW_FIXED_SSE4(1);
	_mm_store_si128( (v128s32 *)(mtxA + 4), _mm_unpacklo_epi64(outVecLo, outVecHi) );
	
	CALCULATE_MATRIX_ROW_FIXED_SSE4(2);
	_mm_store_si128( (v128s32 *)(mtxA + 8), _mm_unpacklo_epi64(outVecLo, outVecHi) );
	
	CALCULATE_MATRIX_ROW_FIXED_SSE4(3);
	_mm_store_si128( (v128s32 *)(mtxA +12), _mm_unpacklo_epi64(outVecLo, outVecHi) );
}

static FORCEINLINE void __mtx4_scale_vec3_fixed_SSE4(s32 (&__restrict inoutMtx)[16], const s32 (&__restrict inVec)[4])
{
	const v128s32 inVec_v128 = _mm_load_si128((v128s32 *)inVec);
	const v128s32 v[3] = {
		_mm_shuffle_epi32(inVec_v128, 0x00),
		_mm_shuffle_epi32(inVec_v128, 0x55),
		_mm_shuffle_epi32(inVec_v128, 0xAA)
	};
	
	v128s32 row[3] = {
		_mm_load_si128((v128s32 *)inoutMtx + 0),
		_mm_load_si128((v128s32 *)inoutMtx + 1),
		_mm_load_si128((v128s32 *)inoutMtx + 2)
	};
	
	v128s32 rowLo;
	v128s32 rowHi;
	
	rowLo = _mm_shuffle_epi32(row[0], 0x50);
	rowLo = _mm_mul_epi32(rowLo, v[0]);
	___s32_saturate_shiftdown_accum64_fixed_SSE4(rowLo);
	
	rowHi = _mm_shuffle_epi32(row[0], 0xFA);
	rowHi = _mm_mul_epi32(rowHi, v[0]);
	___s32_saturate_shiftdown_accum64_fixed_SSE4(rowHi);
	_mm_store_si128( (v128s32 *)inoutMtx + 0, _mm_unpacklo_epi64(rowLo, rowHi) );
	
	rowLo = _mm_shuffle_epi32(row[1], 0x50);
	rowLo = _mm_mul_epi32(rowLo, v[1]);
	___s32_saturate_shiftdown_accum64_fixed_SSE4(rowLo);
	
	rowHi = _mm_shuffle_epi32(row[1], 0xFA);
	rowHi = _mm_mul_epi32(rowHi, v[1]);
	___s32_saturate_shiftdown_accum64_fixed_SSE4(rowHi);
	_mm_store_si128( (v128s32 *)inoutMtx + 1, _mm_unpacklo_epi64(rowLo, rowHi) );
	
	rowLo = _mm_shuffle_epi32(row[2], 0x50);
	rowLo = _mm_mul_epi32(rowLo, v[2]);
	___s32_saturate_shiftdown_accum64_fixed_SSE4(rowLo);
	
	rowHi = _mm_shuffle_epi32(row[2], 0xFA);
	rowHi = _mm_mul_epi32(rowHi, v[2]);
	___s32_saturate_shiftdown_accum64_fixed_SSE4(rowHi);
	_mm_store_si128( (v128s32 *)inoutMtx + 2, _mm_unpacklo_epi64(rowLo, rowHi) );
}

static FORCEINLINE void __mtx4_translate_vec3_fixed_SSE4(s32 (&__restrict inoutMtx)[16], const s32 (&__restrict inVec)[4])
{
	const v128s32 tempVec = _mm_load_si128((v128s32 *)inVec);
	
	const v128s32 v[3] = {
		_mm_shuffle_epi32(tempVec, 0x00),
		_mm_shuffle_epi32(tempVec, 0x55),
		_mm_shuffle_epi32(tempVec, 0xAA)
	};
	
	const v128s32 row[4] = {
		_mm_load_si128((v128s32 *)(inoutMtx + 0)),
		_mm_load_si128((v128s32 *)(inoutMtx + 4)),
		_mm_load_si128((v128s32 *)(inoutMtx + 8)),
		_mm_load_si128((v128s32 *)(inoutMtx +12))
	};
	
	// Notice how we use pmovsxdq for the 4th row instead of pshufd. This is
	// because the dot product calculation for the 4th row involves adding a
	// 12-bit shift up (psllq) instead of adding a pmuldq. When using SSE
	// vectors as 64x2, pmuldq ignores the high 32 bits, while psllq needs
	// those high bits in case of a negative number. pmovsxdq does preserve
	// the sign bits, while pshufd does not.
	
	const v128s32 rowLo[4] = {
		_mm_shuffle_epi32(row[0], 0x50),
		_mm_shuffle_epi32(row[1], 0x50),
		_mm_shuffle_epi32(row[2], 0x50),
		_mm_cvtepi32_epi64(row[3])
	};
	
	const v128s32 rowHi[4] = {
		_mm_shuffle_epi32(row[0], 0xFA),
		_mm_shuffle_epi32(row[1], 0xFA),
		_mm_shuffle_epi32(row[2], 0xFA),
		_mm_cvtepi32_epi64( _mm_srli_si128(row[3],8) )
	};
	
	v128s32 outVecLo;
	v128s32 outVecHi;
	
	outVecLo =                          _mm_mul_epi32(rowLo[0], v[0]);
	outVecLo = _mm_add_epi64( outVecLo, _mm_mul_epi32(rowLo[1], v[1]) );
	outVecLo = _mm_add_epi64( outVecLo, _mm_mul_epi32(rowLo[2], v[2]) );
	outVecLo = _mm_add_epi64( outVecLo, _mm_slli_epi64(rowLo[3], 12) );
	___s32_saturate_shiftdown_accum64_fixed_SSE4(outVecLo);
	
	outVecHi =                          _mm_mul_epi32(rowHi[0], v[0]);
	outVecHi = _mm_add_epi64( outVecHi, _mm_mul_epi32(rowHi[1], v[1]) );
	outVecHi = _mm_add_epi64( outVecHi, _mm_mul_epi32(rowHi[2], v[2]) );
	outVecHi = _mm_add_epi64( outVecHi, _mm_slli_epi64(rowHi[3], 12) );
	___s32_saturate_shiftdown_accum64_fixed_SSE4(outVecHi);
	
	_mm_store_si128( (v128s32 *)(inoutMtx + 12), _mm_unpacklo_epi64(outVecLo, outVecHi) );
}

#endif // ENABLE_SSE4_1

#if defined(ENABLE_NEON_A64)

static FORCEINLINE void ___s32_saturate_shiftdown_accum64_fixed_NEON(int64x2_t &inoutAccum)
{
	int64x2_t outVecMask;
	
	outVecMask = vcgtq_s64( inoutAccum, vdupq_n_s64((s64)0x000007FFFFFFFFFFULL) );
	inoutAccum = vbslq_s64( outVecMask, vdupq_n_s64((s64)0x000007FFFFFFFFFFULL), inoutAccum );
	
	outVecMask = vcltq_s64( inoutAccum, vdupq_n_s64((s64)0xFFFFF80000000000ULL) );
	inoutAccum = vbslq_s64( outVecMask, vdupq_n_s64((s64)0xFFFFF80000000000ULL), inoutAccum );
	
	inoutAccum = vshrq_n_s64(inoutAccum, 12);
	inoutAccum = vreinterpretq_s64_s32( vuzp1q_s32(vreinterpretq_s32_s64(inoutAccum), vdupq_n_s32(0)) );
}

static FORCEINLINE s32 __vec4_dotproduct_vec4_fixed_NEON(const s32 (&__restrict vecA)[4], const s32 (&__restrict vecB)[4])
{
	const v128s32 a = vld1q_s32(vecA);
	const v128s32 b = vld1q_s32(vecB);
	const int64x2_t lo = vmull_s32( vget_low_s32(a), vget_low_s32(b) );
	const int64x2_t hi = vmull_s32( vget_high_s32(a), vget_high_s32(b) );
	const s64 sum = vaddvq_s64( vpaddq_s64(lo,hi) );
	
	return ___s32_saturate_shiftdown_accum64_fixed(sum);
}

static FORCEINLINE void __vec4_multiply_mtx4_fixed_NEON(s32 (&__restrict inoutVec)[4], const s32 (&__restrict inMtx)[16])
{
	const v128s32 inVec128 = vld1q_s32(inoutVec);
	
	const int32x2_t v[4] = {
		vdup_laneq_s32(inVec128, 0),
		vdup_laneq_s32(inVec128, 1),
		vdup_laneq_s32(inVec128, 2),
		vdup_laneq_s32(inVec128, 3),
	};
	
	const int32x4x4_t row = vld1q_s32_x4(inMtx);
	
	int64x2_t outVecLo = vmull_s32( vget_low_s32(row.val[0]), v[0] );
	outVecLo = vmlal_s32( outVecLo, vget_low_s32(row.val[1]), v[1] );
	outVecLo = vmlal_s32( outVecLo, vget_low_s32(row.val[2]), v[2] );
	outVecLo = vmlal_s32( outVecLo, vget_low_s32(row.val[3]), v[3] );
	___s32_saturate_shiftdown_accum64_fixed_NEON(outVecLo);
	
	int64x2_t outVecHi = vmull_s32( vget_high_s32(row.val[0]), v[0] );
	outVecHi = vmlal_s32( outVecHi, vget_high_s32(row.val[1]), v[1] );
	outVecHi = vmlal_s32( outVecHi, vget_high_s32(row.val[2]), v[2] );
	outVecHi = vmlal_s32( outVecHi, vget_high_s32(row.val[3]), v[3] );
	___s32_saturate_shiftdown_accum64_fixed_NEON(outVecHi);
	
	vst1q_s32( inoutVec, vreinterpretq_s32_s64(vzip1q_s64(outVecLo, outVecHi)) );
}

static FORCEINLINE void __vec3_multiply_mtx3_fixed_NEON(s32 (&__restrict inoutVec)[4], const s32 (&__restrict inMtx)[16])
{
	const v128s32 inVec = vld1q_s32(inoutVec);
	
	const int32x2_t v[3] = {
		vdup_laneq_s32(inVec, 0),
		vdup_laneq_s32(inVec, 1),
		vdup_laneq_s32(inVec, 2)
	};
	
	const int32x4x3_t row = vld1q_s32_x3(inMtx);
	
	int64x2_t outVecLo = vmull_s32( vget_low_s32(row.val[0]), v[0] );
	outVecLo = vmlal_s32( outVecLo, vget_low_s32(row.val[1]), v[1] );
	outVecLo = vmlal_s32( outVecLo, vget_low_s32(row.val[2]), v[2] );
	___s32_saturate_shiftdown_accum64_fixed_NEON(outVecLo);
	
	int64x2_t outVecHi = vmull_s32( vget_high_s32(row.val[0]), v[0] );
	outVecHi = vmlal_s32( outVecHi, vget_high_s32(row.val[1]), v[1] );
	outVecHi = vmlal_s32( outVecHi, vget_high_s32(row.val[2]), v[2] );
	___s32_saturate_shiftdown_accum64_fixed_NEON(outVecHi);
	
	v128s32 outVec = vreinterpretq_s32_s64( vzip1q_s64(outVecLo, outVecHi) );
	outVec = vcopyq_laneq_s32(outVec, 3, inVec, 3);
	
	vst1q_s32(inoutVec, outVec);
}

static FORCEINLINE void __mtx4_multiply_mtx4_fixed_NEON(s32 (&__restrict mtxA)[16], const s32 (&__restrict mtxB)[16])
{
	const int32x4x4_t rowA = vld1q_s32_x4(mtxA);
	const int32x4x4_t rowB = vld1q_s32_x4(mtxB);
	
	int64x2_t outVecLo;
	int64x2_t outVecHi;
	int32x2_t v[4];
	
#define CALCULATE_MATRIX_ROW_FIXED_NEON(indexRowB) \
	v[0] = vdup_laneq_s32(rowB.val[(indexRowB)], 0);\
	v[1] = vdup_laneq_s32(rowB.val[(indexRowB)], 1);\
	v[2] = vdup_laneq_s32(rowB.val[(indexRowB)], 2);\
	v[3] = vdup_laneq_s32(rowB.val[(indexRowB)], 3);\
	outVecLo =           vmull_s32( vget_low_s32(rowA.val[0]), v[0] );\
	outVecLo = vmlal_s32( outVecLo, vget_low_s32(rowA.val[1]), v[1] );\
	outVecLo = vmlal_s32( outVecLo, vget_low_s32(rowA.val[2]), v[2] );\
	outVecLo = vmlal_s32( outVecLo, vget_low_s32(rowA.val[3]), v[3] );\
	outVecLo = vshrq_n_s64(outVecLo, 12);\
	outVecLo = vreinterpretq_s64_s32( vuzp1q_s32(vreinterpretq_s32_s64(outVecLo), vdupq_n_s32(0)) );\
	outVecHi =           vmull_s32( vget_high_s32(rowA.val[0]), v[0] );\
	outVecHi = vmlal_s32( outVecHi, vget_high_s32(rowA.val[1]), v[1] );\
	outVecHi = vmlal_s32( outVecHi, vget_high_s32(rowA.val[2]), v[2] );\
	outVecHi = vmlal_s32( outVecHi, vget_high_s32(rowA.val[3]), v[3] );\
	outVecHi = vshrq_n_s64(outVecHi, 12);\
	outVecHi = vreinterpretq_s64_s32( vuzp1q_s32(vreinterpretq_s32_s64(outVecHi), vdupq_n_s32(0)) );
	
	CALCULATE_MATRIX_ROW_FIXED_NEON(0);
	vst1q_s32( mtxA + 0, vreinterpretq_s32_s64(vzip1q_s64(outVecLo, outVecHi)) );
	
	CALCULATE_MATRIX_ROW_FIXED_NEON(1);
	vst1q_s32( mtxA + 4, vreinterpretq_s32_s64(vzip1q_s64(outVecLo, outVecHi)) );
	
	CALCULATE_MATRIX_ROW_FIXED_NEON(2);
	vst1q_s32( mtxA + 8, vreinterpretq_s32_s64(vzip1q_s64(outVecLo, outVecHi)) );
	
	CALCULATE_MATRIX_ROW_FIXED_NEON(3);
	vst1q_s32( mtxA +12, vreinterpretq_s32_s64(vzip1q_s64(outVecLo, outVecHi)) );
}

static FORCEINLINE void __mtx4_scale_vec3_fixed_NEON(s32 (&__restrict inoutMtx)[16], const s32 (&__restrict inVec)[4])
{
	const v128s32 inVec128 = vld1q_s32(inVec);
	
	const int32x2_t v[3] = {
		vdup_laneq_s32(inVec128, 0),
		vdup_laneq_s32(inVec128, 1),
		vdup_laneq_s32(inVec128, 2)
	};
	
	const int32x4x3_t row = vld1q_s32_x3(inoutMtx);
	
	int64x2_t outVecLo;
	int64x2_t outVecHi;
	
	outVecLo = vmull_s32(vget_low_s32(row.val[0]), v[0]);
	___s32_saturate_shiftdown_accum64_fixed_NEON(outVecLo);
	
	outVecHi = vmull_s32(vget_high_s32(row.val[0]), v[0]);
	___s32_saturate_shiftdown_accum64_fixed_NEON(outVecHi);
	vst1q_s32( inoutMtx + 0, vreinterpretq_s32_s64(vzip1q_s64(outVecLo, outVecHi)) );
	
	outVecLo = vmull_s32(vget_low_s32(row.val[1]), v[1]);
	___s32_saturate_shiftdown_accum64_fixed_NEON(outVecLo);
	
	outVecHi = vmull_s32(vget_high_s32(row.val[1]), v[1]);
	___s32_saturate_shiftdown_accum64_fixed_NEON(outVecHi);
	vst1q_s32( inoutMtx + 4, vreinterpretq_s32_s64(vzip1q_s64(outVecLo, outVecHi)) );
	
	outVecLo = vmull_s32(vget_low_s32(row.val[2]), v[2]);
	___s32_saturate_shiftdown_accum64_fixed_NEON(outVecLo);
	
	outVecHi = vmull_s32(vget_high_s32(row.val[2]), v[2]);
	___s32_saturate_shiftdown_accum64_fixed_NEON(outVecHi);
	vst1q_s32( inoutMtx + 8, vreinterpretq_s32_s64(vzip1q_s64(outVecLo, outVecHi)) );
}

static FORCEINLINE void __mtx4_translate_vec3_fixed_NEON(s32 (&__restrict inoutMtx)[16], const s32 (&__restrict inVec)[4])
{
	const v128s32 inVec128 = vld1q_s32(inVec);
	
	const int32x2_t v[3] = {
		vdup_laneq_s32(inVec128, 0),
		vdup_laneq_s32(inVec128, 1),
		vdup_laneq_s32(inVec128, 2)
	};
	
	const int32x4x4_t row = vld1q_s32_x4(inoutMtx);
	
	int64x2_t outVecLo = vshlq_n_s64( vmovl_s32(vget_low_s32(row.val[3])), 12 );
	outVecLo = vmlal_s32( outVecLo, vget_low_s32(row.val[0]), v[0] );
	outVecLo = vmlal_s32( outVecLo, vget_low_s32(row.val[1]), v[1] );
	outVecLo = vmlal_s32( outVecLo, vget_low_s32(row.val[2]), v[2] );
	___s32_saturate_shiftdown_accum64_fixed_NEON(outVecLo);
	
	int64x2_t outVecHi = vshlq_n_s64( vmovl_s32(vget_high_s32(row.val[3])), 12 );
	outVecHi = vmlal_s32( outVecHi, vget_high_s32(row.val[0]), v[0] );
	outVecHi = vmlal_s32( outVecHi, vget_high_s32(row.val[1]), v[1] );
	outVecHi = vmlal_s32( outVecHi, vget_high_s32(row.val[2]), v[2] );
	___s32_saturate_shiftdown_accum64_fixed_NEON(outVecHi);
	
	vst1q_s32( inoutMtx + 12, vreinterpretq_s32_s64(vzip1q_s64(outVecLo, outVecHi)) );
}

#endif // ENABLE_NEON_A64

void MatrixInit(s32 (&mtx)[16])
{
	MatrixIdentity(mtx);
}

void MatrixInit(float (&mtx)[16])
{
	MatrixIdentity(mtx);
}

void MatrixIdentity(s32 (&mtx)[16])
{
	static const CACHE_ALIGN s32 mtxIdentity[16] = {
		(1 << 12), 0, 0, 0,
		0, (1 << 12), 0, 0,
		0, 0, (1 << 12), 0,
		0, 0, 0, (1 << 12)
	};
	
	MatrixCopy(mtx, mtxIdentity);
}

void MatrixIdentity(float (&mtx)[16])
{
	static const CACHE_ALIGN float mtxIdentity[16] = {
		1.0f, 0.0f, 0.0f, 0.0f,
		0.0f, 1.0f, 0.0f, 0.0f,
		0.0f, 0.0f, 1.0f, 0.0f,
		0.0f, 0.0f, 0.0f, 1.0f
	};
	
	MatrixCopy(mtx, mtxIdentity);
}

void MatrixSet(s32 (&mtx)[16], const size_t x, const size_t y, const s32 value)
{
	mtx[x+(y<<2)] = value;
}

void MatrixSet(float (&mtx)[16], const size_t x, const size_t y, const float value)
{
	mtx[x+(y<<2)] = value;
}

void MatrixSet(float (&mtx)[16], const size_t x, const size_t y, const s32 value)
{
	mtx[x+(y<<2)] = (float)value / 4096.0f;
}

void MatrixCopy(s32 (&__restrict mtxDst)[16], const s32 (&__restrict mtxSrc)[16])
{
	buffer_copy_fast<sizeof(s32)*16>((s32 *)mtxDst, (s32 *)mtxSrc);
}

void MatrixCopy(float (&__restrict mtxDst)[16], const float (&__restrict mtxSrc)[16])
{
	// Can't use buffer_copy_fast() here because it assumes the copying of integers,
	// so just use regular memcpy() for copying the floats.
	memcpy(mtxDst, mtxSrc, sizeof(float)*16);
}

void MatrixCopy(float (&__restrict mtxDst)[16], const s32 (&__restrict mtxSrc)[16])
{
#if defined(ENABLE_SSE)
	__mtx4_copynormalize_mtx4_float_SSE(mtxDst, mtxSrc);
#elif defined(ENABLE_NEON_A64)
	__mtx4_copynormalize_mtx4_float_NEON(mtxDst, mtxSrc);
#else
	__mtx4_copynormalize_mtx4_float(mtxDst, mtxSrc);
#endif
}

int MatrixCompare(const s32 (&__restrict mtxDst)[16], const s32 (&__restrict mtxSrc)[16])
{
	return memcmp(mtxDst, mtxSrc, sizeof(s32)*16);
}

int MatrixCompare(const float (&__restrict mtxDst)[16], const float (&__restrict mtxSrc)[16])
{
	return memcmp(mtxDst, mtxSrc, sizeof(float)*16);
}

s32 MatrixGetMultipliedIndex(const u32 index, const s32 (&__restrict mtxA)[16], const s32 (&__restrict mtxB)[16])
{
	assert(index < 16);
	
	const u32 col = index & 0x00000003;
	const u32 row = index & 0x0000000C;
	
	const s32 vecA[4] = { mtxA[col+0], mtxA[col+4], mtxA[col+8], mtxA[col+12] };
	const s32 vecB[4] = { mtxB[row+0], mtxB[row+1], mtxB[row+2], mtxB[row+3] };
	
#if defined(ENABLE_NEON_A64)
	return __vec4_dotproduct_vec4_fixed_NEON(vecA, vecB);
#else
	return __vec4_dotproduct_vec4_fixed(vecA, vecB);
#endif
}

float MatrixGetMultipliedIndex(const u32 index, const float (&__restrict mtxA)[16], const float (&__restrict mtxB)[16])
{
	assert(index < 16);
	
	const u32 col = index & 0x00000003;
	const u32 row = index & 0x0000000C;
	
	const float vecA[4] = { mtxA[col+0], mtxA[col+4], mtxA[col+8], mtxA[col+12] };
	const float vecB[4] = { mtxB[row+0], mtxB[row+1], mtxB[row+2], mtxB[row+3] };
	
#if defined(ENABLE_SSE4_1)
	return __vec4_dotproduct_vec4_float_SSE4(vecA, vecB);
#elif defined(ENABLE_NEON_A64)
	return __vec4_dotproduct_vec4_float_NEON(vecA, vecB);
#else
	return __vec4_dotproduct_vec4_float(vecA, vecB);
#endif
}

// TODO: All of these float-based vector functions are obsolete and should be deleted.
void Vector2Copy(float *dst, const float *src)
{
	dst[0] = src[0];
	dst[1] = src[1];
}

void Vector2Add(float *dst, const float *src)
{
	dst[0] += src[0];
	dst[1] += src[1];
}

void Vector2Subtract(float *dst, const float *src)
{
	dst[0] -= src[0];
	dst[1] -= src[1];
}

float Vector2Dot(const float *a, const float *b)
{
	return (a[0]*b[0]) + (a[1]*b[1]);
}

/* http://www.gamedev.net/community/forums/topic.asp?topic_id=289972 */
float Vector2Cross(const float *a, const float *b)
{
	return (a[0]*b[1]) - (a[1]*b[0]);
}

float Vector3Dot(const float *a, const float *b) 
{
	return a[0]*b[0] + a[1]*b[1] + a[2]*b[2];
}

void Vector3Cross(float* dst, const float *a, const float *b) 
{
	dst[0] = a[1]*b[2] - a[2]*b[1];
	dst[1] = a[2]*b[0] - a[0]*b[2];
	dst[2] = a[0]*b[1] - a[1]*b[0];
}

float Vector3Length(const float *a)
{
	float lengthSquared = Vector3Dot(a,a);
	float length = sqrt(lengthSquared);
	return length;
}

void Vector3Add(float *dst, const float *src)
{
	dst[0] += src[0];
	dst[1] += src[1];
	dst[2] += src[2];
}

void Vector3Subtract(float *dst, const float *src)
{
	dst[0] -= src[0];
	dst[1] -= src[1];
	dst[2] -= src[2];
}

void Vector3Scale(float *dst, const float scale)
{
	dst[0] *= scale;
	dst[1] *= scale;
	dst[2] *= scale;
}

void Vector3Copy(float *dst, const float *src)
{
	dst[0] = src[0];
	dst[1] = src[1];
	dst[2] = src[2];
}

void Vector3Normalize(float *dst)
{
	float length = Vector3Length(dst);
	Vector3Scale(dst,1.0f/length);
}

void Vector4Copy(float *dst, const float *src)
{
	dst[0] = src[0];
	dst[1] = src[1];
	dst[2] = src[2];
	dst[3] = src[3];
}

void MatrixMultVec4x4(const s32 (&__restrict mtx)[16], s32 (&__restrict vec)[4])
{
#if defined(ENABLE_SSE4_1)
	__vec4_multiply_mtx4_fixed_SSE4(vec, mtx);
#elif defined(ENABLE_NEON_A64)
	__vec4_multiply_mtx4_fixed_NEON(vec, mtx);
#else
	__vec4_multiply_mtx4_fixed(vec, mtx);
#endif
}

void MatrixMultVec4x4(const s32 (&__restrict mtx)[16], float (&__restrict vec)[4])
{
#if defined(ENABLE_SSE)
	__vec4_multiply_mtx4_float_SSE(vec, mtx);
#elif defined(ENABLE_NEON_A64)
	__vec4_multiply_mtx4_float_NEON(vec, mtx);
#else
	__vec4_multiply_mtx4_float(vec, mtx);
#endif
}

void MatrixMultVec3x3(const s32 (&__restrict mtx)[16], s32 (&__restrict vec)[4])
{
#if defined(ENABLE_SSE4_1)
	__vec3_multiply_mtx3_fixed_SSE4(vec, mtx);
#elif defined(ENABLE_NEON_A64)
	__vec3_multiply_mtx3_fixed_NEON(vec, mtx);
#else
	__vec3_multiply_mtx3_fixed(vec, mtx);
#endif
}

void MatrixMultVec3x3(const s32 (&__restrict mtx)[16], float (&__restrict vec)[4])
{
#if defined(ENABLE_SSE)
	__vec3_multiply_mtx3_float_SSE(vec, mtx);
#elif defined(ENABLE_NEON_A64)
	__vec3_multiply_mtx3_float_NEON(vec, mtx);
#else
	__vec3_multiply_mtx3_float(vec, mtx);
#endif
}

void MatrixTranslate(s32 (&__restrict mtx)[16], const s32 (&__restrict vec)[4])
{
#if defined(ENABLE_SSE4_1)
	__mtx4_translate_vec3_fixed_SSE4(mtx, vec);
#elif defined(ENABLE_NEON_A64)
	__mtx4_translate_vec3_fixed_NEON(mtx, vec);
#else
	__mtx4_translate_vec3_fixed(mtx, vec);
#endif
}

void MatrixTranslate(float (&__restrict mtx)[16], const float (&__restrict vec)[4])
{
#if defined(ENABLE_SSE)
	__mtx4_translate_vec3_float_SSE(mtx, vec);
#elif defined(ENABLE_NEON_A64)
	__mtx4_translate_vec3_float_NEON(mtx, vec);
#else
	__mtx4_translate_vec3_float(mtx, vec);
#endif
}

void MatrixScale(s32 (&__restrict mtx)[16], const s32 (&__restrict vec)[4])
{
#if defined(ENABLE_SSE4_1)
	__mtx4_scale_vec3_fixed_SSE4(mtx, vec);
#elif defined(ENABLE_NEON_A64)
	__mtx4_scale_vec3_fixed_NEON(mtx, vec);
#else
	__mtx4_scale_vec3_fixed(mtx, vec);
#endif
}

void MatrixScale(float (&__restrict mtx)[16], const float (&__restrict vec)[4])
{
#if defined(ENABLE_SSE)
	__mtx4_scale_vec3_float_SSE(mtx, vec);
#elif defined(ENABLE_NEON_A64)
	__mtx4_scale_vec3_float_NEON(mtx, vec);
#else
	__mtx4_scale_vec3_float(mtx, vec);
#endif
}

void MatrixMultiply(s32 (&__restrict mtxA)[16], const s32 (&__restrict mtxB)[16])
{
#if defined(ENABLE_SSE4_1)
	__mtx4_multiply_mtx4_fixed_SSE4(mtxA, mtxB);
#elif defined(ENABLE_NEON_A64)
	__mtx4_multiply_mtx4_fixed_NEON(mtxA, mtxB);
#else
	__mtx4_multiply_mtx4_fixed(mtxA, mtxB);
#endif
}

void MatrixMultiply(float (&__restrict mtxA)[16], const s32 (&__restrict mtxB)[16])
{
#if defined(ENABLE_SSE)
	__mtx4_multiply_mtx4_float_SSE(mtxA, mtxB);
#elif defined(ENABLE_NEON_A64)
	__mtx4_multiply_mtx4_float_NEON(mtxA, mtxB);
#else
	__mtx4_multiply_mtx4_float(mtxA, mtxB);
#endif
}
