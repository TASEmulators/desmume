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

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include <assert.h>
#include "matrix.h"
#include "MMU.h"


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
	
	memcpy(mtx, mtxIdentity, sizeof(s32)*16);
}

void MatrixIdentity(float (&mtx)[16])
{
	static const CACHE_ALIGN float mtxIdentity[16] = {
		1.0f, 0.0f, 0.0f, 0.0f,
		0.0f, 1.0f, 0.0f, 0.0f,
		0.0f, 0.0f, 1.0f, 0.0f,
		0.0f, 0.0f, 0.0f, 1.0f
	};
	
	memcpy(mtx, mtxIdentity, sizeof(float)*16);
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
	memcpy(mtxDst, mtxSrc, sizeof(s32)*16);
}

void MatrixCopy(float (&__restrict mtxDst)[16], const float (&__restrict mtxSrc)[16])
{
	memcpy(mtxDst, mtxSrc, sizeof(float)*16);
}

void MatrixCopy(float (&__restrict mtxDst)[16], const s32 (&__restrict mtxSrc)[16])
{
	mtxDst[ 0] = (float)mtxSrc[ 0] / 4096.0f;
	mtxDst[ 1] = (float)mtxSrc[ 1] / 4096.0f;
	mtxDst[ 2] = (float)mtxSrc[ 2] / 4096.0f;
	mtxDst[ 3] = (float)mtxSrc[ 3] / 4096.0f;
	
	mtxDst[ 4] = (float)mtxSrc[ 4] / 4096.0f;
	mtxDst[ 5] = (float)mtxSrc[ 5] / 4096.0f;
	mtxDst[ 6] = (float)mtxSrc[ 6] / 4096.0f;
	mtxDst[ 7] = (float)mtxSrc[ 7] / 4096.0f;
	
	mtxDst[ 8] = (float)mtxSrc[ 8] / 4096.0f;
	mtxDst[ 9] = (float)mtxSrc[ 9] / 4096.0f;
	mtxDst[10] = (float)mtxSrc[10] / 4096.0f;
	mtxDst[11] = (float)mtxSrc[11] / 4096.0f;
	
	mtxDst[12] = (float)mtxSrc[12] / 4096.0f;
	mtxDst[13] = (float)mtxSrc[13] / 4096.0f;
	mtxDst[14] = (float)mtxSrc[14] / 4096.0f;
	mtxDst[15] = (float)mtxSrc[15] / 4096.0f;
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
	
	const size_t iMod = index % 4;
	const size_t iDiv = (index >> 2) << 2;
	
	const s32 temp = sfx32_shiftdown( fx32_mul(mtxA[iMod  ], mtxB[iDiv  ]) + fx32_mul(mtxA[iMod+ 4], mtxB[iDiv+1]) + fx32_mul(mtxA[iMod+8], mtxB[iDiv+2]) + fx32_mul(mtxA[iMod+12], mtxB[iDiv+3]) );
	return temp;
}

float MatrixGetMultipliedIndex(const u32 index, const float (&__restrict mtxA)[16], const float (&__restrict mtxB)[16])
{
	assert(index < 16);
	
	const size_t iMod = index % 4;
	const size_t iDiv = (index >> 2) << 2;
	
	const float temp = (mtxA[iMod  ] * mtxB[iDiv  ]) + (mtxA[iMod+ 4] * mtxB[iDiv+1]) + (mtxA[iMod+8] * mtxB[iDiv+2]) + (mtxA[iMod+12] * mtxB[iDiv+3]);
	return temp;
}

template<MatrixMode MODE>
void MatrixStackInit(MatrixStack<MODE> *stack)
{
	for (size_t i = 0; i < MatrixStack<MODE>::size; i++)
	{
		MatrixInit(stack->matrix[i]);
	}
	
	stack->position = 0;
}

template<MatrixMode MODE>
s32* MatrixStackGet(MatrixStack<MODE> *stack)
{
	return stack->matrix[stack->position];
}

template void MatrixStackInit(MatrixStack<MATRIXMODE_PROJECTION> *stack);
template void MatrixStackInit(MatrixStack<MATRIXMODE_POSITION> *stack);
template void MatrixStackInit(MatrixStack<MATRIXMODE_POSITION_VECTOR> *stack);
template void MatrixStackInit(MatrixStack<MATRIXMODE_TEXTURE> *stack);

template s32* MatrixStackGet(MatrixStack<MATRIXMODE_PROJECTION> *stack);
template s32* MatrixStackGet(MatrixStack<MATRIXMODE_POSITION> *stack);
template s32* MatrixStackGet(MatrixStack<MATRIXMODE_POSITION_VECTOR> *stack);
template s32* MatrixStackGet(MatrixStack<MATRIXMODE_TEXTURE> *stack);

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

void _MatrixMultVec4x4_NoSIMD(const s32 (&__restrict mtx)[16], float (&__restrict vec)[4])
{
	const CACHE_ALIGN float mtxFloat[16] = {
		mtx[ 0] / 4096.0f,
		mtx[ 1] / 4096.0f,
		mtx[ 2] / 4096.0f,
		mtx[ 3] / 4096.0f,
		
		mtx[ 4] / 4096.0f,
		mtx[ 5] / 4096.0f,
		mtx[ 6] / 4096.0f,
		mtx[ 7] / 4096.0f,
		
		mtx[ 8] / 4096.0f,
		mtx[ 9] / 4096.0f,
		mtx[10] / 4096.0f,
		mtx[11] / 4096.0f,
		
		mtx[12] / 4096.0f,
		mtx[13] / 4096.0f,
		mtx[14] / 4096.0f,
		mtx[15] / 4096.0f
	};
	
	const float x = vec[0];
	const float y = vec[1];
	const float z = vec[2];
	const float w = vec[3];
	
	vec[0] = (x * mtxFloat[0]) + (y * mtxFloat[4]) + (z * mtxFloat[ 8]) + (w * mtxFloat[12]);
	vec[1] = (x * mtxFloat[1]) + (y * mtxFloat[5]) + (z * mtxFloat[ 9]) + (w * mtxFloat[13]);
	vec[2] = (x * mtxFloat[2]) + (y * mtxFloat[6]) + (z * mtxFloat[10]) + (w * mtxFloat[14]);
	vec[3] = (x * mtxFloat[3]) + (y * mtxFloat[7]) + (z * mtxFloat[11]) + (w * mtxFloat[15]);
}

#ifdef ENABLE_SSE

void MatrixMultVec4x4(const s32 (&__restrict mtx)[16], float (&__restrict vec)[4])
{
	const __m128 loadedVec = _mm_load_ps(vec);
	const __m128 convertScalar = _mm_set1_ps(1.0f/4096.0f);
	
#ifdef ENABLE_SSE2
	__m128 row[4] = {
		_mm_cvtepi32_ps( _mm_load_si128((__m128i *)(mtx +  0)) ),
		_mm_cvtepi32_ps( _mm_load_si128((__m128i *)(mtx +  4)) ),
		_mm_cvtepi32_ps( _mm_load_si128((__m128i *)(mtx +  8)) ),
		_mm_cvtepi32_ps( _mm_load_si128((__m128i *)(mtx + 12)) )
	};
#else
	const CACHE_ALIGN float mtxFloat[16] = {
		(float)mtx[0],
		(float)mtx[1],
		(float)mtx[2],
		(float)mtx[3],
		
		(float)mtx[4],
		(float)mtx[5],
		(float)mtx[6],
		(float)mtx[7],
		
		(float)mtx[8],
		(float)mtx[9],
		(float)mtx[10],
		(float)mtx[11],
		
		(float)mtx[12],
		(float)mtx[13],
		(float)mtx[14],
		(float)mtx[15]
	};
	
	__m128 row[4] = {
		_mm_load_ps(mtxFloat +  0),
		_mm_load_ps(mtxFloat +  4),
		_mm_load_ps(mtxFloat +  8),
		_mm_load_ps(mtxFloat + 12)
	};
#endif
	
	row[0] = _mm_mul_ps(row[0], convertScalar);
	row[1] = _mm_mul_ps(row[1], convertScalar);
	row[2] = _mm_mul_ps(row[2], convertScalar);
	row[3] = _mm_mul_ps(row[3], convertScalar);
	
	const __m128 scalar[4] = {
		_mm_shuffle_ps(loadedVec, loadedVec, 0x00),
		_mm_shuffle_ps(loadedVec, loadedVec, 0x55),
		_mm_shuffle_ps(loadedVec, loadedVec, 0xAA),
		_mm_shuffle_ps(loadedVec, loadedVec, 0xFF)
	};
	
	const __m128 calcVec = _mm_add_ps( _mm_mul_ps(row[0], scalar[0]), _mm_add_ps(_mm_mul_ps(row[1], scalar[1]), _mm_add_ps(_mm_mul_ps(row[2], scalar[2]), _mm_mul_ps(row[3], scalar[3]))) );
	_mm_store_ps(vec, calcVec);
}

void MatrixMultVec3x3(const s32 (&__restrict mtx)[16], float (&__restrict vec)[4])
{
	const __m128 loadedVec = _mm_load_ps(vec);
	const __m128 convertScalar = _mm_set1_ps(1.0f/4096.0f);
	
#ifdef ENABLE_SSE2
	__m128 row[3] = {
		_mm_cvtepi32_ps( _mm_load_si128((__m128i *)(mtx + 0)) ),
		_mm_cvtepi32_ps( _mm_load_si128((__m128i *)(mtx + 4)) ),
		_mm_cvtepi32_ps( _mm_load_si128((__m128i *)(mtx + 8)) )
	};
#else
	const CACHE_ALIGN float mtxFloat[16] = {
		(float)mtx[0],
		(float)mtx[1],
		(float)mtx[2],
		(float)mtx[3],
		
		(float)mtx[4],
		(float)mtx[5],
		(float)mtx[6],
		(float)mtx[7],
		
		(float)mtx[8],
		(float)mtx[9],
		(float)mtx[10],
		(float)mtx[11],
		
		(float)mtx[12],
		(float)mtx[13],
		(float)mtx[14],
		(float)mtx[15]
	};
	
	__m128 row[3] = {
		_mm_load_ps(mtxFloat + 0),
		_mm_load_ps(mtxFloat + 4),
		_mm_load_ps(mtxFloat + 8)
	};
#endif
	
	row[0] = _mm_mul_ps(row[0], convertScalar);
	row[1] = _mm_mul_ps(row[1], convertScalar);
	row[2] = _mm_mul_ps(row[2], convertScalar);
	
	const __m128 scalar[3] = {
		_mm_shuffle_ps(loadedVec, loadedVec, 0x00),
		_mm_shuffle_ps(loadedVec, loadedVec, 0x55),
		_mm_shuffle_ps(loadedVec, loadedVec, 0xAA)
	};
	
	const __m128 calcVec = _mm_add_ps( _mm_mul_ps(row[0], scalar[0]), _mm_add_ps(_mm_mul_ps(row[1], scalar[1]), _mm_mul_ps(row[2], scalar[2])) );
	_mm_store_ps(vec, calcVec);
}

void MatrixTranslate(float (&__restrict mtx)[16], const float (&__restrict vec)[4])
{
	__m128 xmm4 = _mm_load_ps(vec);
	__m128 xmm5 = _mm_shuffle_ps(xmm4, xmm4, B8(01010101));
	__m128 xmm6 = _mm_shuffle_ps(xmm4, xmm4, B8(10101010));
	xmm4 = _mm_shuffle_ps(xmm4, xmm4, B8(00000000));
	
	xmm4 = _mm_mul_ps(xmm4,_mm_load_ps(mtx));
	xmm5 = _mm_mul_ps(xmm5,_mm_load_ps(mtx+4));
	xmm6 = _mm_mul_ps(xmm6,_mm_load_ps(mtx+8));
	xmm4 = _mm_add_ps(xmm4,xmm5);
	xmm4 = _mm_add_ps(xmm4,xmm6);
	xmm4 = _mm_add_ps(xmm4,_mm_load_ps(mtx+12));
	_mm_store_ps(mtx+12,xmm4);
}

void MatrixScale(float (&__restrict mtx)[16], const float (&__restrict vec)[4])
{
	__m128 xmm4 = _mm_load_ps(vec);
	__m128 xmm5 = _mm_shuffle_ps(xmm4, xmm4, B8(01010101));
	__m128 xmm6 = _mm_shuffle_ps(xmm4, xmm4, B8(10101010));
	xmm4 = _mm_shuffle_ps(xmm4, xmm4, B8(00000000));
	
	xmm4 = _mm_mul_ps(xmm4,_mm_load_ps(mtx));
	xmm5 = _mm_mul_ps(xmm5,_mm_load_ps(mtx+4));
	xmm6 = _mm_mul_ps(xmm6,_mm_load_ps(mtx+8));
	_mm_store_ps(mtx,xmm4);
	_mm_store_ps(mtx+4,xmm5);
	_mm_store_ps(mtx+8,xmm6);
}

void MatrixMultiply(float (&__restrict mtxA)[16], const s32 (&__restrict mtxB)[16])
{
	const __m128 convertScale = _mm_set1_ps(1.0f/4096.0f);
	
#ifdef ENABLE_SSE2
	__m128 rowB[4] = {
		_mm_cvtepi32_ps( _mm_load_si128((__m128i *)(mtxB +  0)) ),
		_mm_cvtepi32_ps( _mm_load_si128((__m128i *)(mtxB +  4)) ),
		_mm_cvtepi32_ps( _mm_load_si128((__m128i *)(mtxB +  8)) ),
		_mm_cvtepi32_ps( _mm_load_si128((__m128i *)(mtxB + 12)) )
	};
#else
	const CACHE_ALIGN float mtxFloatB[16] = {
		(float)mtxB[ 0],
		(float)mtxB[ 1],
		(float)mtxB[ 2],
		(float)mtxB[ 3],
		
		(float)mtxB[ 4],
		(float)mtxB[ 5],
		(float)mtxB[ 6],
		(float)mtxB[ 7],
		
		(float)mtxB[ 8],
		(float)mtxB[ 9],
		(float)mtxB[10],
		(float)mtxB[11],
		
		(float)mtxB[12],
		(float)mtxB[13],
		(float)mtxB[14],
		(float)mtxB[15]
	};
	
	__m128 rowB[4] = {
		_mm_load_ps(mtxFloatB +  0),
		_mm_load_ps(mtxFloatB +  4),
		_mm_load_ps(mtxFloatB +  8),
		_mm_load_ps(mtxFloatB + 12)
	};
#endif
	
	rowB[0] = _mm_mul_ps(rowB[0], convertScale);
	rowB[1] = _mm_mul_ps(rowB[1], convertScale);
	rowB[2] = _mm_mul_ps(rowB[2], convertScale);
	rowB[3] = _mm_mul_ps(rowB[3], convertScale);
	
	__m128 rowA[4] = {
		_mm_load_ps(mtxA +  0),
		_mm_load_ps(mtxA +  4),
		_mm_load_ps(mtxA +  8),
		_mm_load_ps(mtxA + 12)
	};
	
	__m128 vecB[4];
	__m128 calcRow;
	
	vecB[0] = _mm_shuffle_ps(rowB[0], rowB[0], 0x00);
	vecB[1] = _mm_shuffle_ps(rowB[0], rowB[0], 0x55);
	vecB[2] = _mm_shuffle_ps(rowB[0], rowB[0], 0xAA);
	vecB[3] = _mm_shuffle_ps(rowB[0], rowB[0], 0xFF);
	calcRow = _mm_add_ps( _mm_mul_ps(rowA[0], vecB[0]), _mm_add_ps(_mm_mul_ps(rowA[1], vecB[1]), _mm_add_ps(_mm_mul_ps(rowA[2], vecB[2]), _mm_mul_ps(rowA[3], vecB[3]))) );
	_mm_store_ps(mtxA +  0, calcRow);
	
	vecB[0] = _mm_shuffle_ps(rowB[1], rowB[1], 0x00);
	vecB[1] = _mm_shuffle_ps(rowB[1], rowB[1], 0x55);
	vecB[2] = _mm_shuffle_ps(rowB[1], rowB[1], 0xAA);
	vecB[3] = _mm_shuffle_ps(rowB[1], rowB[1], 0xFF);
	calcRow = _mm_add_ps( _mm_mul_ps(rowA[0], vecB[0]), _mm_add_ps(_mm_mul_ps(rowA[1], vecB[1]), _mm_add_ps(_mm_mul_ps(rowA[2], vecB[2]), _mm_mul_ps(rowA[3], vecB[3]))) );
	_mm_store_ps(mtxA +  4, calcRow);
	
	vecB[0] = _mm_shuffle_ps(rowB[2], rowB[2], 0x00);
	vecB[1] = _mm_shuffle_ps(rowB[2], rowB[2], 0x55);
	vecB[2] = _mm_shuffle_ps(rowB[2], rowB[2], 0xAA);
	vecB[3] = _mm_shuffle_ps(rowB[2], rowB[2], 0xFF);
	calcRow = _mm_add_ps( _mm_mul_ps(rowA[0], vecB[0]), _mm_add_ps(_mm_mul_ps(rowA[1], vecB[1]), _mm_add_ps(_mm_mul_ps(rowA[2], vecB[2]), _mm_mul_ps(rowA[3], vecB[3]))) );
	_mm_store_ps(mtxA +  8, calcRow);
	
	vecB[0] = _mm_shuffle_ps(rowB[3], rowB[3], 0x00);
	vecB[1] = _mm_shuffle_ps(rowB[3], rowB[3], 0x55);
	vecB[2] = _mm_shuffle_ps(rowB[3], rowB[3], 0xAA);
	vecB[3] = _mm_shuffle_ps(rowB[3], rowB[3], 0xFF);
	calcRow = _mm_add_ps( _mm_mul_ps(rowA[0], vecB[0]), _mm_add_ps(_mm_mul_ps(rowA[1], vecB[1]), _mm_add_ps(_mm_mul_ps(rowA[2], vecB[2]), _mm_mul_ps(rowA[3], vecB[3]))) );
	_mm_store_ps(mtxA + 12, calcRow);
}

template<size_t NUM_ROWS>
FORCEINLINE void vector_fix2float(float (&mtx)[16], const float divisor)
{
	const __m128 divisor_v128 = _mm_set1_ps(divisor);
	
	for (size_t i = 0; i < NUM_ROWS * 4; i+=4)
	{
		_mm_store_ps( mtx + i, _mm_div_ps(_mm_load_ps(mtx + i), divisor_v128) );
	}
}

#else

void MatrixMultVec4x4(const s32 (&__restrict mtx)[16], float (&__restrict vec)[4])
{
	_MatrixMultVec4x4_NoSIMD(mtx, vec);
}

void MatrixMultVec3x3(const s32 (&__restrict mtx)[16], float (&__restrict vec)[4])
{
	const CACHE_ALIGN float mtxFloat[16] = {
		mtx[ 0] / 4096.0f,
		mtx[ 1] / 4096.0f,
		mtx[ 2] / 4096.0f,
		mtx[ 3] / 4096.0f,
		
		mtx[ 4] / 4096.0f,
		mtx[ 5] / 4096.0f,
		mtx[ 6] / 4096.0f,
		mtx[ 7] / 4096.0f,
		
		mtx[ 8] / 4096.0f,
		mtx[ 9] / 4096.0f,
		mtx[10] / 4096.0f,
		mtx[11] / 4096.0f,
		
		mtx[12] / 4096.0f,
		mtx[13] / 4096.0f,
		mtx[14] / 4096.0f,
		mtx[15] / 4096.0f
	};
	
	const float x = vec[0];
	const float y = vec[1];
	const float z = vec[2];
	
	vec[0] = (x * mtxFloat[0]) + (y * mtxFloat[4]) + (z * mtxFloat[ 8]);
	vec[1] = (x * mtxFloat[1]) + (y * mtxFloat[5]) + (z * mtxFloat[ 9]);
	vec[2] = (x * mtxFloat[2]) + (y * mtxFloat[6]) + (z * mtxFloat[10]);
}

void MatrixTranslate(float (&__restrict mtx)[16], const float (&__restrict vec)[4])
{
	mtx[12] += (mtx[0] * vec[0]) + (mtx[4] * vec[1]) + (mtx[ 8] * vec[2]);
	mtx[13] += (mtx[1] * vec[0]) + (mtx[5] * vec[1]) + (mtx[ 9] * vec[2]);
	mtx[14] += (mtx[2] * vec[0]) + (mtx[6] * vec[1]) + (mtx[10] * vec[2]);
	mtx[15] += (mtx[3] * vec[0]) + (mtx[7] * vec[1]) + (mtx[11] * vec[2]);
}

void MatrixScale(float (&__restrict mtx)[16], const float (&__restrict vec)[4])
{
	mtx[ 0] *= vec[0];
	mtx[ 1] *= vec[0];
	mtx[ 2] *= vec[0];
	mtx[ 3] *= vec[0];
	
	mtx[ 4] *= vec[1];
	mtx[ 5] *= vec[1];
	mtx[ 6] *= vec[1];
	mtx[ 7] *= vec[1];
	
	mtx[ 8] *= vec[2];
	mtx[ 9] *= vec[2];
	mtx[10] *= vec[2];
	mtx[11] *= vec[2];
}

void MatrixMultiply(float (&__restrict mtxA)[16], const s32 (&__restrict mtxB)[16])
{
	const CACHE_ALIGN float mtxFloatB[16] = {
		(float)mtxB[ 0],
		(float)mtxB[ 1],
		(float)mtxB[ 2],
		(float)mtxB[ 3],
		
		(float)mtxB[ 4],
		(float)mtxB[ 5],
		(float)mtxB[ 6],
		(float)mtxB[ 7],
		
		(float)mtxB[ 8],
		(float)mtxB[ 9],
		(float)mtxB[10],
		(float)mtxB[11],
		
		(float)mtxB[12],
		(float)mtxB[13],
		(float)mtxB[14],
		(float)mtxB[15]
	};
	
	CACHE_ALIGN float tmpMatrix[16];
	
	tmpMatrix[0]  = (mtxA[ 0] * mtxFloatB[ 0]) + (mtxA[ 4] * mtxFloatB[ 1]) + (mtxA[ 8] * mtxFloatB[ 2]) + (mtxA[12] * mtxFloatB[ 3]);
	tmpMatrix[1]  = (mtxA[ 1] * mtxFloatB[ 0]) + (mtxA[ 5] * mtxFloatB[ 1]) + (mtxA[ 9] * mtxFloatB[ 2]) + (mtxA[13] * mtxFloatB[ 3]);
	tmpMatrix[2]  = (mtxA[ 2] * mtxFloatB[ 0]) + (mtxA[ 6] * mtxFloatB[ 1]) + (mtxA[10] * mtxFloatB[ 2]) + (mtxA[14] * mtxFloatB[ 3]);
	tmpMatrix[3]  = (mtxA[ 3] * mtxFloatB[ 0]) + (mtxA[ 7] * mtxFloatB[ 1]) + (mtxA[11] * mtxFloatB[ 2]) + (mtxA[15] * mtxFloatB[ 3]);
	
	tmpMatrix[4]  = (mtxA[ 0] * mtxFloatB[ 4]) + (mtxA[ 4] * mtxFloatB[ 5]) + (mtxA[ 8] * mtxFloatB[ 6]) + (mtxA[12] * mtxFloatB[ 7]);
	tmpMatrix[5]  = (mtxA[ 1] * mtxFloatB[ 4]) + (mtxA[ 5] * mtxFloatB[ 5]) + (mtxA[ 9] * mtxFloatB[ 6]) + (mtxA[13] * mtxFloatB[ 7]);
	tmpMatrix[6]  = (mtxA[ 2] * mtxFloatB[ 4]) + (mtxA[ 6] * mtxFloatB[ 5]) + (mtxA[10] * mtxFloatB[ 6]) + (mtxA[14] * mtxFloatB[ 7]);
	tmpMatrix[7]  = (mtxA[ 3] * mtxFloatB[ 4]) + (mtxA[ 7] * mtxFloatB[ 5]) + (mtxA[11] * mtxFloatB[ 6]) + (mtxA[15] * mtxFloatB[ 7]);
	
	tmpMatrix[8]  = (mtxA[ 0] * mtxFloatB[ 8]) + (mtxA[ 4] * mtxFloatB[ 9]) + (mtxA[ 8] * mtxFloatB[10]) + (mtxA[12] * mtxFloatB[11]);
	tmpMatrix[9]  = (mtxA[ 1] * mtxFloatB[ 8]) + (mtxA[ 5] * mtxFloatB[ 9]) + (mtxA[ 9] * mtxFloatB[10]) + (mtxA[13] * mtxFloatB[11]);
	tmpMatrix[10] = (mtxA[ 2] * mtxFloatB[ 8]) + (mtxA[ 6] * mtxFloatB[ 9]) + (mtxA[10] * mtxFloatB[10]) + (mtxA[14] * mtxFloatB[11]);
	tmpMatrix[11] = (mtxA[ 3] * mtxFloatB[ 8]) + (mtxA[ 7] * mtxFloatB[ 9]) + (mtxA[11] * mtxFloatB[10]) + (mtxA[15] * mtxFloatB[11]);
	
	tmpMatrix[12] = (mtxA[ 0] * mtxFloatB[12]) + (mtxA[ 4] * mtxFloatB[13]) + (mtxA[ 8] * mtxFloatB[14]) + (mtxA[12] * mtxFloatB[15]);
	tmpMatrix[13] = (mtxA[ 1] * mtxFloatB[12]) + (mtxA[ 5] * mtxFloatB[13]) + (mtxA[ 9] * mtxFloatB[14]) + (mtxA[13] * mtxFloatB[15]);
	tmpMatrix[14] = (mtxA[ 2] * mtxFloatB[12]) + (mtxA[ 6] * mtxFloatB[13]) + (mtxA[10] * mtxFloatB[14]) + (mtxA[14] * mtxFloatB[15]);
	tmpMatrix[15] = (mtxA[ 3] * mtxFloatB[12]) + (mtxA[ 7] * mtxFloatB[13]) + (mtxA[11] * mtxFloatB[14]) + (mtxA[15] * mtxFloatB[15]);
	
	memcpy(mtxA, tmpMatrix, sizeof(float)*16);
}

template<size_t NUM_ROWS>
FORCEINLINE void vector_fix2float(float (&mtx)[16], const float divisor)
{
	for (size_t i = 0; i < NUM_ROWS * 4; i+=4)
	{
		mtx[i+0] /= divisor;
		mtx[i+1] /= divisor;
		mtx[i+2] /= divisor;
		mtx[i+3] /= divisor;
	}
}

#endif

#ifdef ENABLE_SSE4_1

FORCEINLINE void _Vec4_MultiplyByMatrix(__m128i &outVec,
										const __m128i &c0, const __m128i &c1, const __m128i &c2, const __m128i &c3,
										const __m128i &rowLo0, const __m128i &rowLo1, const __m128i &rowLo2, const __m128i &rowLo3,
										const __m128i &rowHi0, const __m128i &rowHi1, const __m128i &rowHi2, const __m128i &rowHi3)
{
	__m128i outVecLo = _mm_add_epi64( _mm_add_epi64(_mm_mul_epi32(rowLo0, c0), _mm_mul_epi32(rowLo1, c1)), _mm_add_epi64(_mm_mul_epi32(rowLo2, c2), _mm_mul_epi32(rowLo3, c3)) );
	outVecLo = _mm_srli_epi64(outVecLo, 12);
	outVecLo = _mm_shuffle_epi32(outVecLo, 0xD8);
	
	__m128i outVecHi = _mm_add_epi64( _mm_add_epi64(_mm_mul_epi32(rowHi0, c0), _mm_mul_epi32(rowHi1, c1)), _mm_add_epi64(_mm_mul_epi32(rowHi2, c2), _mm_mul_epi32(rowHi3, c3)) );
	outVecHi = _mm_srli_epi64(outVecHi, 12);
	outVecHi = _mm_shuffle_epi32(outVecHi, 0x8D);
	
	outVec = _mm_blendv_epi8(outVecLo, outVecHi, _mm_set_epi32(0xFFFFFFFF, 0xFFFFFFFF, 0, 0));
}

FORCEINLINE void _Vec3_MultiplyByMatrix(__m128i &outVec,
										const __m128i &c0, const __m128i &c1, const __m128i &c2,
										const __m128i &rowLo0, const __m128i &rowLo1, const __m128i &rowLo2,
										const __m128i &rowHi0, const __m128i &rowHi1, const __m128i &rowHi2)
{
	__m128i outVecLo = _mm_add_epi64( _mm_mul_epi32(rowLo0, c0), _mm_add_epi64(_mm_mul_epi32(rowLo1, c1), _mm_mul_epi32(rowLo2, c2)) );
	outVecLo = _mm_srli_epi64(outVecLo, 12);
	outVecLo = _mm_shuffle_epi32(outVecLo, 0xD8);
	
	__m128i outVecHi = _mm_add_epi64( _mm_mul_epi32(rowHi0, c0), _mm_add_epi64(_mm_mul_epi32(rowHi1, c1), _mm_mul_epi32(rowHi2, c2)) );
	outVecHi = _mm_srli_epi64(outVecHi, 12);
	outVecHi = _mm_shuffle_epi32(outVecHi, 0x8D);
	
	outVec = _mm_blendv_epi8(outVecLo, outVecHi, _mm_set_epi32(0xFFFFFFFF, 0xFFFFFFFF, 0, 0));
}

FORCEINLINE void _Vec4_Translate(__m128i &outVec,
								 const __m128i &c0, const __m128i &c1, const __m128i &c2,
								 const __m128i &rowLo0, const __m128i &rowLo1, const __m128i &rowLo2, const __m128i &rowLo3,
								 const __m128i &rowHi0, const __m128i &rowHi1, const __m128i &rowHi2, const __m128i &rowHi3)
{
	__m128i outVecLo = _mm_add_epi64( _mm_add_epi64(_mm_mul_epi32(rowLo0, c0), _mm_mul_epi32(rowLo1, c1)), _mm_add_epi64(_mm_mul_epi32(rowLo2, c2), _mm_slli_epi64(rowLo3, 12)) );
	outVecLo = _mm_srli_epi64(outVecLo, 12);
	outVecLo = _mm_shuffle_epi32(outVecLo, 0xD8);
	
	__m128i outVecHi = _mm_add_epi64( _mm_add_epi64(_mm_mul_epi32(rowHi0, c0), _mm_mul_epi32(rowHi1, c1)), _mm_add_epi64(_mm_mul_epi32(rowHi2, c2), _mm_slli_epi64(rowHi3, 12)) );
	outVecHi = _mm_srli_epi64(outVecHi, 12);
	outVecHi = _mm_shuffle_epi32(outVecHi, 0x8D);
	
	outVec = _mm_blendv_epi8(outVecLo, outVecHi, _mm_set_epi32(0xFFFFFFFF, 0xFFFFFFFF, 0, 0));
}

FORCEINLINE void _Vec4_Scale(__m128i &inoutVec, const __m128i &scalar)
{
	__m128i outVecLo = _mm_cvtepu32_epi64(inoutVec);
	__m128i outVecHi = _mm_cvtepu32_epi64( _mm_srli_si128(inoutVec, 8) );
	
	outVecLo = _mm_mul_epi32(outVecLo, scalar);
	outVecLo = _mm_srli_epi64(outVecLo, 12);
	outVecLo = _mm_shuffle_epi32(outVecLo, 0xD8);
	
	outVecHi = _mm_mul_epi32(outVecHi, scalar);
	outVecHi = _mm_srli_epi64(outVecHi, 12);
	outVecHi = _mm_shuffle_epi32(outVecHi, 0x8D);
	
	inoutVec = _mm_blendv_epi8(outVecLo, outVecHi, _mm_set_epi32(0xFFFFFFFF, 0xFFFFFFFF, 0, 0));
}

void MatrixMultVec4x4(const s32 (&__restrict mtx)[16], s32 (&__restrict vec)[4])
{
	const __m128i inVec = _mm_load_si128((__m128i *)vec);
	
	const __m128i scalar[4] = {
		_mm_shuffle_epi32(inVec, 0x00),
		_mm_shuffle_epi32(inVec, 0x55),
		_mm_shuffle_epi32(inVec, 0xAA),
		_mm_shuffle_epi32(inVec, 0xFF)
	};
	
	const __m128i row[4] = {
		_mm_load_si128((__m128i *)(mtx +  0)),
		_mm_load_si128((__m128i *)(mtx +  4)),
		_mm_load_si128((__m128i *)(mtx +  8)),
		_mm_load_si128((__m128i *)(mtx + 12))
	};
	
	const __m128i rowLo[4] = {
		_mm_cvtepu32_epi64(row[0]),
		_mm_cvtepu32_epi64(row[1]),
		_mm_cvtepu32_epi64(row[2]),
		_mm_cvtepu32_epi64(row[3])
	};
	
	const __m128i rowHi[4] = {
		_mm_cvtepu32_epi64( _mm_srli_si128(row[0], 8)),
		_mm_cvtepu32_epi64( _mm_srli_si128(row[1], 8)),
		_mm_cvtepu32_epi64( _mm_srli_si128(row[2], 8)),
		_mm_cvtepu32_epi64( _mm_srli_si128(row[3], 8))
	};
	
	__m128i outVec;
	_Vec4_MultiplyByMatrix(outVec,
						   scalar[0], scalar[1], scalar[2], scalar[3],
						   rowLo[0], rowLo[1], rowLo[2], rowLo[3],
						   rowHi[0], rowHi[1], rowHi[2], rowHi[3]);
	
	_mm_store_si128((__m128i *)vec, outVec);
}

void MatrixMultVec3x3(const s32 (&__restrict mtx)[16], s32 (&__restrict vec)[4])
{
	const __m128i inVec = _mm_load_si128((__m128i *)vec);
	
	const __m128i scalar[3] = {
		_mm_shuffle_epi32(inVec, 0x00),
		_mm_shuffle_epi32(inVec, 0x55),
		_mm_shuffle_epi32(inVec, 0xAA)
	};
	
	const __m128i row[3] = {
		_mm_load_si128((__m128i *)(mtx + 0)),
		_mm_load_si128((__m128i *)(mtx + 4)),
		_mm_load_si128((__m128i *)(mtx + 8))
	};
	
	const __m128i rowLo[3] = {
		_mm_cvtepu32_epi64(row[0]),
		_mm_cvtepu32_epi64(row[1]),
		_mm_cvtepu32_epi64(row[2])
	};
	
	const __m128i rowHi[3] = {
		_mm_cvtepu32_epi64( _mm_srli_si128(row[0], 8)),
		_mm_cvtepu32_epi64( _mm_srli_si128(row[1], 8)),
		_mm_cvtepu32_epi64( _mm_srli_si128(row[2], 8))
	};
	
	__m128i outVec;
	_Vec3_MultiplyByMatrix(outVec,
						   scalar[0], scalar[1], scalar[2],
						   rowLo[0], rowLo[1], rowLo[2],
						   rowHi[0], rowHi[1], rowHi[2]);
	
	outVec = _mm_blend_epi16(outVec, inVec, 0xC0);
	_mm_store_si128((__m128i *)vec, outVec);
}

void MatrixTranslate(s32 (&__restrict mtx)[16], const s32 (&__restrict vec)[4])
{
	const __m128i inVec = _mm_load_si128((__m128i *)vec);
	
	const __m128i scalar[3] = {
		_mm_shuffle_epi32(inVec, 0x00),
		_mm_shuffle_epi32(inVec, 0x55),
		_mm_shuffle_epi32(inVec, 0xAA)
	};
	
	const __m128i row[4] = {
		_mm_load_si128((__m128i *)(mtx +  0)),
		_mm_load_si128((__m128i *)(mtx +  4)),
		_mm_load_si128((__m128i *)(mtx +  8)),
		_mm_load_si128((__m128i *)(mtx + 12))
	};
	
	const __m128i rowLo[4] = {
		_mm_cvtepu32_epi64(row[0]),
		_mm_cvtepu32_epi64(row[1]),
		_mm_cvtepu32_epi64(row[2]),
		_mm_cvtepu32_epi64(row[3])
	};
	
	const __m128i rowHi[4] = {
		_mm_cvtepu32_epi64( _mm_srli_si128(row[0], 8)),
		_mm_cvtepu32_epi64( _mm_srli_si128(row[1], 8)),
		_mm_cvtepu32_epi64( _mm_srli_si128(row[2], 8)),
		_mm_cvtepu32_epi64( _mm_srli_si128(row[3], 8))
	};
	
	__m128i outVec;
	_Vec4_Translate(outVec,
					scalar[0], scalar[1], scalar[2],
					rowLo[0], rowLo[1], rowLo[2], rowLo[3],
					rowHi[0], rowHi[1], rowHi[2], rowHi[3]);
	
	_mm_store_si128((__m128i *)(mtx + 12), outVec);
}

void MatrixScale(s32 (&__restrict mtx)[16], const s32 (&__restrict vec)[4])
{
	const __m128i inVec = _mm_load_si128((__m128i *)vec);
	const __m128i scalar[3] = {
		_mm_shuffle_epi32(inVec, 0x00),
		_mm_shuffle_epi32(inVec, 0x55),
		_mm_shuffle_epi32(inVec, 0xAA)
	};
	
	__m128i row[3] = {
		_mm_load_si128((__m128i *)(mtx + 0)),
		_mm_load_si128((__m128i *)(mtx + 4)),
		_mm_load_si128((__m128i *)(mtx + 8))
	};
	
	_Vec4_Scale(row[0], scalar[0]);
	_mm_store_si128((__m128i *)(mtx + 0), row[0]);
	
	_Vec4_Scale(row[1], scalar[1]);
	_mm_store_si128((__m128i *)(mtx + 4), row[1]);
	
	_Vec4_Scale(row[2], scalar[2]);
	_mm_store_si128((__m128i *)(mtx + 8), row[2]);
}

void MatrixMultiply(s32 (&__restrict mtxA)[16], const s32 (&__restrict mtxB)[16])
{
	const __m128i rowA[4] = {
		_mm_load_si128((__m128i *)(mtxA +  0)),
		_mm_load_si128((__m128i *)(mtxA +  4)),
		_mm_load_si128((__m128i *)(mtxA +  8)),
		_mm_load_si128((__m128i *)(mtxA + 12))
	};
	
	const __m128i rowB[4] = {
		_mm_load_si128((__m128i *)(mtxB +  0)),
		_mm_load_si128((__m128i *)(mtxB +  4)),
		_mm_load_si128((__m128i *)(mtxB +  8)),
		_mm_load_si128((__m128i *)(mtxB + 12))
	};
	
	const __m128i rowLo[4] = {
		_mm_cvtepu32_epi64(rowA[0]),
		_mm_cvtepu32_epi64(rowA[1]),
		_mm_cvtepu32_epi64(rowA[2]),
		_mm_cvtepu32_epi64(rowA[3])
	};
	
	const __m128i rowHi[4] = {
		_mm_cvtepu32_epi64( _mm_srli_si128(rowA[0], 8)),
		_mm_cvtepu32_epi64( _mm_srli_si128(rowA[1], 8)),
		_mm_cvtepu32_epi64( _mm_srli_si128(rowA[2], 8)),
		_mm_cvtepu32_epi64( _mm_srli_si128(rowA[3], 8))
	};
	
	__m128i outVec;
	__m128i scalar[4];
	
	scalar[0] = _mm_shuffle_epi32(rowB[0], 0x00);
	scalar[1] = _mm_shuffle_epi32(rowB[0], 0x55);
	scalar[2] = _mm_shuffle_epi32(rowB[0], 0xAA);
	scalar[3] = _mm_shuffle_epi32(rowB[0], 0xFF);
	_Vec4_MultiplyByMatrix(outVec,
						   scalar[0], scalar[1], scalar[2], scalar[3],
						   rowLo[0], rowLo[1], rowLo[2], rowLo[3],
						   rowHi[0], rowHi[1], rowHi[2], rowHi[3]);
	_mm_store_si128((__m128i *)(mtxA +  0), outVec);
	
	scalar[0] = _mm_shuffle_epi32(rowB[1], 0x00);
	scalar[1] = _mm_shuffle_epi32(rowB[1], 0x55);
	scalar[2] = _mm_shuffle_epi32(rowB[1], 0xAA);
	scalar[3] = _mm_shuffle_epi32(rowB[1], 0xFF);
	_Vec4_MultiplyByMatrix(outVec,
						   scalar[0], scalar[1], scalar[2], scalar[3],
						   rowLo[0], rowLo[1], rowLo[2], rowLo[3],
						   rowHi[0], rowHi[1], rowHi[2], rowHi[3]);
	_mm_store_si128((__m128i *)(mtxA +  4), outVec);
	
	scalar[0] = _mm_shuffle_epi32(rowB[2], 0x00);
	scalar[1] = _mm_shuffle_epi32(rowB[2], 0x55);
	scalar[2] = _mm_shuffle_epi32(rowB[2], 0xAA);
	scalar[3] = _mm_shuffle_epi32(rowB[2], 0xFF);
	_Vec4_MultiplyByMatrix(outVec,
						   scalar[0], scalar[1], scalar[2], scalar[3],
						   rowLo[0], rowLo[1], rowLo[2], rowLo[3],
						   rowHi[0], rowHi[1], rowHi[2], rowHi[3]);
	_mm_store_si128((__m128i *)(mtxA +  8), outVec);
	
	scalar[0] = _mm_shuffle_epi32(rowB[3], 0x00);
	scalar[1] = _mm_shuffle_epi32(rowB[3], 0x55);
	scalar[2] = _mm_shuffle_epi32(rowB[3], 0xAA);
	scalar[3] = _mm_shuffle_epi32(rowB[3], 0xFF);
	_Vec4_MultiplyByMatrix(outVec,
						   scalar[0], scalar[1], scalar[2], scalar[3],
						   rowLo[0], rowLo[1], rowLo[2], rowLo[3],
						   rowHi[0], rowHi[1], rowHi[2], rowHi[3]);
	_mm_store_si128((__m128i *)(mtxA + 12), outVec);
}

#else

FORCEINLINE void _Vec4_MultiplyByMatrix(s32 (&__restrict outVec)[4], const s32 (&__restrict inVec)[4], const s32 (&__restrict mtx)[16])
{
	outVec[0] = sfx32_shiftdown( fx32_mul(mtx[0],inVec[0]) + fx32_mul(mtx[4],inVec[1]) + fx32_mul(mtx[ 8],inVec[2]) + fx32_mul(mtx[12],inVec[3]) );
	outVec[1] = sfx32_shiftdown( fx32_mul(mtx[1],inVec[0]) + fx32_mul(mtx[5],inVec[1]) + fx32_mul(mtx[ 9],inVec[2]) + fx32_mul(mtx[13],inVec[3]) );
	outVec[2] = sfx32_shiftdown( fx32_mul(mtx[2],inVec[0]) + fx32_mul(mtx[6],inVec[1]) + fx32_mul(mtx[10],inVec[2]) + fx32_mul(mtx[14],inVec[3]) );
	outVec[3] = sfx32_shiftdown( fx32_mul(mtx[3],inVec[0]) + fx32_mul(mtx[7],inVec[1]) + fx32_mul(mtx[11],inVec[2]) + fx32_mul(mtx[15],inVec[3]) );
}

FORCEINLINE void _Vec3_MultiplyByMatrix(s32 (&__restrict outVec)[4], const s32 (&__restrict inVec)[3], const s32 (&__restrict mtx)[16])
{
	outVec[0] = sfx32_shiftdown( fx32_mul(mtx[0],inVec[0]) + fx32_mul(mtx[4],inVec[1]) + fx32_mul(mtx[ 8],inVec[2]) );
	outVec[1] = sfx32_shiftdown( fx32_mul(mtx[1],inVec[0]) + fx32_mul(mtx[5],inVec[1]) + fx32_mul(mtx[ 9],inVec[2]) );
	outVec[2] = sfx32_shiftdown( fx32_mul(mtx[2],inVec[0]) + fx32_mul(mtx[6],inVec[1]) + fx32_mul(mtx[10],inVec[2]) );
}

FORCEINLINE void _Vec4_Scale(s32 (&inoutVec)[4], const s32 scalar)
{
	inoutVec[0] = sfx32_shiftdown( fx32_mul(inoutVec[0], scalar) );
	inoutVec[1] = sfx32_shiftdown( fx32_mul(inoutVec[1], scalar) );
	inoutVec[2] = sfx32_shiftdown( fx32_mul(inoutVec[2], scalar) );
	inoutVec[3] = sfx32_shiftdown( fx32_mul(inoutVec[3], scalar) );
}

void MatrixMultVec4x4(const s32 (&__restrict mtx)[16], s32 (&__restrict vec)[4])
{
	const CACHE_ALIGN s32 tmpVec[4] = {
		vec[0], vec[1], vec[2], vec[3]
	};
	
	_Vec4_MultiplyByMatrix(vec, tmpVec, mtx);
}

void MatrixMultVec3x3(const s32 (&__restrict mtx)[16], s32 (&__restrict vec)[4])
{
	const CACHE_ALIGN s32 tmpVec[3] = {
		vec[0], vec[1], vec[2]
	};
	
	_Vec3_MultiplyByMatrix(vec, tmpVec, mtx);
}

void MatrixTranslate(s32 (&__restrict mtx)[16], const s32 (&__restrict vec)[4])
{
	mtx[12] = sfx32_shiftdown( fx32_mul(mtx[0], vec[0]) + fx32_mul(mtx[4], vec[1]) + fx32_mul(mtx[ 8], vec[2]) + fx32_shiftup(mtx[12]) );
	mtx[13] = sfx32_shiftdown( fx32_mul(mtx[1], vec[0]) + fx32_mul(mtx[5], vec[1]) + fx32_mul(mtx[ 9], vec[2]) + fx32_shiftup(mtx[13]) );
	mtx[14] = sfx32_shiftdown( fx32_mul(mtx[2], vec[0]) + fx32_mul(mtx[6], vec[1]) + fx32_mul(mtx[10], vec[2]) + fx32_shiftup(mtx[14]) );
	mtx[15] = sfx32_shiftdown( fx32_mul(mtx[3], vec[0]) + fx32_mul(mtx[7], vec[1]) + fx32_mul(mtx[11], vec[2]) + fx32_shiftup(mtx[15]) );
}

void MatrixScale(s32 (&__restrict mtx)[16], const s32 (&__restrict vec)[4])
{
	_Vec4_Scale((s32 (&__restrict)[4])mtx[0], vec[0]);
	_Vec4_Scale((s32 (&__restrict)[4])mtx[4], vec[1]);
	_Vec4_Scale((s32 (&__restrict)[4])mtx[8], vec[2]);
}

void MatrixMultiply(s32 (&__restrict mtxA)[16], const s32 (&__restrict mtxB)[16])
{
	const CACHE_ALIGN s32 tmpMtxA[16] = {
		mtxA[ 0], mtxA[ 1], mtxA[ 2], mtxA[ 3],
		mtxA[ 4], mtxA[ 5], mtxA[ 6], mtxA[ 7],
		mtxA[ 8], mtxA[ 9], mtxA[10], mtxA[11],
		mtxA[12], mtxA[13], mtxA[14], mtxA[15]
	};
	
	_Vec4_MultiplyByMatrix((s32 (&__restrict)[4])mtxA[ 0], (s32 (&__restrict)[4])mtxB[ 0], tmpMtxA);
	_Vec4_MultiplyByMatrix((s32 (&__restrict)[4])mtxA[ 4], (s32 (&__restrict)[4])mtxB[ 4], tmpMtxA);
	_Vec4_MultiplyByMatrix((s32 (&__restrict)[4])mtxA[ 8], (s32 (&__restrict)[4])mtxB[ 8], tmpMtxA);
	_Vec4_MultiplyByMatrix((s32 (&__restrict)[4])mtxA[12], (s32 (&__restrict)[4])mtxB[12], tmpMtxA);
}

#endif
