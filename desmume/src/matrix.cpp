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


void MatrixInit  (s32 *matrix)
{
	memset (matrix, 0, sizeof(s32)*16);
	matrix[0] = matrix[5] = matrix[10] = matrix[15] = 1<<12;
}

void MatrixInit  (float *matrix)
{
	memset (matrix, 0, sizeof(s32)*16);
	matrix[0] = matrix[5] = matrix[10] = matrix[15] = 1.f;
}

void	MatrixIdentity			(s32 *matrix)
{
	matrix[1] = matrix[2] = matrix[3] = matrix[4] = 0;
	matrix[6] = matrix[7] = matrix[8] = matrix[9] = 0;
	matrix[11] = matrix[12] = matrix[13] = matrix[14] = 0;
	matrix[0] = matrix[5] = matrix[10] = matrix[15] = 1<<12;
}

s32 MatrixGetMultipliedIndex(const u32 index, s32 *matrix, s32 *rightMatrix)
{
	const size_t iMod = index%4, iDiv = (index>>2)<<2;

	s64 temp = ((s64)matrix[iMod  ]*rightMatrix[iDiv  ])+((s64)matrix[iMod+ 4]*rightMatrix[iDiv+1])+
			((s64)matrix[iMod+8]*rightMatrix[iDiv+2])+((s64)matrix[iMod+12]*rightMatrix[iDiv+3]);

	return (s32)(temp>>12);
}

void MatrixSet (s32 *matrix, int x, int y, s32 value)
{
	matrix [x+(y<<2)] = value;
}

void MatrixCopy (float* matrixDST, const float* matrixSRC)
{
	matrixDST[0] = matrixSRC[0];
	matrixDST[1] = matrixSRC[1];
	matrixDST[2] = matrixSRC[2];
	matrixDST[3] = matrixSRC[3];
	matrixDST[4] = matrixSRC[4];
	matrixDST[5] = matrixSRC[5];
	matrixDST[6] = matrixSRC[6];
	matrixDST[7] = matrixSRC[7];
	matrixDST[8] = matrixSRC[8];
	matrixDST[9] = matrixSRC[9];
	matrixDST[10] = matrixSRC[10];
	matrixDST[11] = matrixSRC[11];
	matrixDST[12] = matrixSRC[12];
	matrixDST[13] = matrixSRC[13];
	matrixDST[14] = matrixSRC[14];
	matrixDST[15] = matrixSRC[15];

}

void MatrixCopy (s32* matrixDST, const s32* matrixSRC)
{
	memcpy(matrixDST,matrixSRC,sizeof(s32)*16);
}

int MatrixCompare (const s32* matrixDST, const s32* matrixSRC)
{
	return memcmp((void*)matrixDST, matrixSRC, sizeof(s32)*16);
}

void MatrixStackInit(MatrixStack *stack)
{
	for (int i = 0; i < stack->size; i++)
	{
		MatrixInit(&stack->matrix[i*16]);
	}
	stack->position = 0;
}

void MatrixStackSetMaxSize (MatrixStack *stack, int size)
{
	int i;

	stack->size = size;

	if (stack->matrix != NULL) {
		free (stack->matrix);
	}
	stack->matrix = new s32[stack->size*16*sizeof(s32)];

	for (i = 0; i < stack->size; i++)
	{
		MatrixInit (&stack->matrix[i*16]);
	}
}


MatrixStack::MatrixStack(int size, int type)
{
	MatrixStackSetMaxSize(this,size);
	this->type = type;
}


s32* MatrixStackGetPos(MatrixStack *stack, const size_t pos)
{
	assert(pos < stack->size);
	return &stack->matrix[pos*16];
}

s32* MatrixStackGet (MatrixStack *stack)
{
	return &stack->matrix[stack->position*16];
}

void MatrixStackLoadMatrix (MatrixStack *stack, const size_t pos, const s32 *ptr)
{
	assert(pos < stack->size);
	MatrixCopy(&stack->matrix[pos*16], ptr);
}

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

void _MatrixMultVec4x4_NoSIMD(const s32 *__restrict mtxPtr, float *__restrict vecPtr)
{
	const CACHE_ALIGN float mtxFloat[16] = {
		mtxPtr[ 0] / 4096.0f,
		mtxPtr[ 1] / 4096.0f,
		mtxPtr[ 2] / 4096.0f,
		mtxPtr[ 3] / 4096.0f,
		
		mtxPtr[ 4] / 4096.0f,
		mtxPtr[ 5] / 4096.0f,
		mtxPtr[ 6] / 4096.0f,
		mtxPtr[ 7] / 4096.0f,
		
		mtxPtr[ 8] / 4096.0f,
		mtxPtr[ 9] / 4096.0f,
		mtxPtr[10] / 4096.0f,
		mtxPtr[11] / 4096.0f,
		
		mtxPtr[12] / 4096.0f,
		mtxPtr[13] / 4096.0f,
		mtxPtr[14] / 4096.0f,
		mtxPtr[15] / 4096.0f
	};
	
	const float x = vecPtr[0];
	const float y = vecPtr[1];
	const float z = vecPtr[2];
	const float w = vecPtr[3];
	
	vecPtr[0] = (x * mtxFloat[0]) + (y * mtxFloat[4]) + (z * mtxFloat[ 8]) + (w * mtxFloat[12]);
	vecPtr[1] = (x * mtxFloat[1]) + (y * mtxFloat[5]) + (z * mtxFloat[ 9]) + (w * mtxFloat[13]);
	vecPtr[2] = (x * mtxFloat[2]) + (y * mtxFloat[6]) + (z * mtxFloat[10]) + (w * mtxFloat[14]);
	vecPtr[3] = (x * mtxFloat[3]) + (y * mtxFloat[7]) + (z * mtxFloat[11]) + (w * mtxFloat[15]);
}

#ifdef ENABLE_SSE

void MatrixMultVec4x4(const s32 *__restrict mtxPtr, float *__restrict vecPtr)
{
	const __m128 loadedVecPtr = _mm_load_ps(vecPtr);
	const __m128 convertScalar = _mm_set1_ps(1.0f/4096.0f);
	
#ifdef ENABLE_SSE2
	__m128 row[4] = {
		_mm_cvtepi32_ps( _mm_load_si128((__m128i *)(mtxPtr +  0)) ),
		_mm_cvtepi32_ps( _mm_load_si128((__m128i *)(mtxPtr +  4)) ),
		_mm_cvtepi32_ps( _mm_load_si128((__m128i *)(mtxPtr +  8)) ),
		_mm_cvtepi32_ps( _mm_load_si128((__m128i *)(mtxPtr + 12)) )
	};
#else
	const CACHE_ALIGN float mtxFloat[16] = {
		(float)mtxPtr[0],
		(float)mtxPtr[1],
		(float)mtxPtr[2],
		(float)mtxPtr[3],
		
		(float)mtxPtr[4],
		(float)mtxPtr[5],
		(float)mtxPtr[6],
		(float)mtxPtr[7],
		
		(float)mtxPtr[8],
		(float)mtxPtr[9],
		(float)mtxPtr[10],
		(float)mtxPtr[11],
		
		(float)mtxPtr[12],
		(float)mtxPtr[13],
		(float)mtxPtr[14],
		(float)mtxPtr[15]
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
	
	const __m128 vec[4] = {
		_mm_shuffle_ps(loadedVecPtr, loadedVecPtr, 0x00),
		_mm_shuffle_ps(loadedVecPtr, loadedVecPtr, 0x55),
		_mm_shuffle_ps(loadedVecPtr, loadedVecPtr, 0xAA),
		_mm_shuffle_ps(loadedVecPtr, loadedVecPtr, 0xFF)
	};
	
	const __m128 calcVec = _mm_add_ps( _mm_mul_ps(row[0], vec[0]), _mm_add_ps(_mm_mul_ps(row[1], vec[1]), _mm_add_ps(_mm_mul_ps(row[2], vec[2]), _mm_mul_ps(row[3], vec[3]))) );
	_mm_store_ps(vecPtr, calcVec);
}

void MatrixMultVec3x3(const s32 *__restrict mtxPtr, float *__restrict vecPtr)
{
	const __m128 loadedVecPtr = _mm_load_ps(vecPtr);
	const __m128 convertScalar = _mm_set1_ps(1.0f/4096.0f);
	
#ifdef ENABLE_SSE2
	__m128 row[3] = {
		_mm_cvtepi32_ps( _mm_load_si128((__m128i *)(mtxPtr + 0)) ),
		_mm_cvtepi32_ps( _mm_load_si128((__m128i *)(mtxPtr + 4)) ),
		_mm_cvtepi32_ps( _mm_load_si128((__m128i *)(mtxPtr + 8)) )
	};
#else
	const CACHE_ALIGN float mtxFloat[16] = {
		(float)mtxPtr[0],
		(float)mtxPtr[1],
		(float)mtxPtr[2],
		(float)mtxPtr[3],
		
		(float)mtxPtr[4],
		(float)mtxPtr[5],
		(float)mtxPtr[6],
		(float)mtxPtr[7],
		
		(float)mtxPtr[8],
		(float)mtxPtr[9],
		(float)mtxPtr[10],
		(float)mtxPtr[11],
		
		(float)mtxPtr[12],
		(float)mtxPtr[13],
		(float)mtxPtr[14],
		(float)mtxPtr[15]
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
	
	const __m128 vec[3] = {
		_mm_shuffle_ps(loadedVecPtr, loadedVecPtr, 0x00),
		_mm_shuffle_ps(loadedVecPtr, loadedVecPtr, 0x55),
		_mm_shuffle_ps(loadedVecPtr, loadedVecPtr, 0xAA)
	};
	
	const __m128 calcVec = _mm_add_ps( _mm_mul_ps(row[0], vec[0]), _mm_add_ps(_mm_mul_ps(row[1], vec[1]), _mm_mul_ps(row[2], vec[2])) );
	_mm_store_ps(vecPtr, calcVec);
}

void MatrixTranslate(float *__restrict mtxPtr, const float *__restrict vecPtr)
{
	__m128 xmm4 = _mm_load_ps(vecPtr);
	__m128 xmm5 = _mm_shuffle_ps(xmm4, xmm4, B8(01010101));
	__m128 xmm6 = _mm_shuffle_ps(xmm4, xmm4, B8(10101010));
	xmm4 = _mm_shuffle_ps(xmm4, xmm4, B8(00000000));
	
	xmm4 = _mm_mul_ps(xmm4,_mm_load_ps(mtxPtr));
	xmm5 = _mm_mul_ps(xmm5,_mm_load_ps(mtxPtr+4));
	xmm6 = _mm_mul_ps(xmm6,_mm_load_ps(mtxPtr+8));
	xmm4 = _mm_add_ps(xmm4,xmm5);
	xmm4 = _mm_add_ps(xmm4,xmm6);
	xmm4 = _mm_add_ps(xmm4,_mm_load_ps(mtxPtr+12));
	_mm_store_ps(mtxPtr+12,xmm4);
}

void MatrixScale(float *__restrict mtxPtr, const float *__restrict vecPtr)
{
	__m128 xmm4 = _mm_load_ps(vecPtr);
	__m128 xmm5 = _mm_shuffle_ps(xmm4, xmm4, B8(01010101));
	__m128 xmm6 = _mm_shuffle_ps(xmm4, xmm4, B8(10101010));
	xmm4 = _mm_shuffle_ps(xmm4, xmm4, B8(00000000));
	
	xmm4 = _mm_mul_ps(xmm4,_mm_load_ps(mtxPtr));
	xmm5 = _mm_mul_ps(xmm5,_mm_load_ps(mtxPtr+4));
	xmm6 = _mm_mul_ps(xmm6,_mm_load_ps(mtxPtr+8));
	_mm_store_ps(mtxPtr,xmm4);
	_mm_store_ps(mtxPtr+4,xmm5);
	_mm_store_ps(mtxPtr+8,xmm6);
}

void MatrixMultiply(float *__restrict mtxPtrA, const s32 *__restrict mtxPtrB)
{
	const __m128 convertScale = _mm_set1_ps(1.0f/4096.0f);
	
#ifdef ENABLE_SSE2
	__m128 rowB[4] = {
		_mm_cvtepi32_ps( _mm_load_si128((__m128i *)(mtxPtrB +  0)) ),
		_mm_cvtepi32_ps( _mm_load_si128((__m128i *)(mtxPtrB +  4)) ),
		_mm_cvtepi32_ps( _mm_load_si128((__m128i *)(mtxPtrB +  8)) ),
		_mm_cvtepi32_ps( _mm_load_si128((__m128i *)(mtxPtrB + 12)) )
	};
#else
	const CACHE_ALIGN float mtxFloatB[16] = {
		(float)mtxPtrB[0],
		(float)mtxPtrB[1],
		(float)mtxPtrB[2],
		(float)mtxPtrB[3],
		
		(float)mtxPtrB[4],
		(float)mtxPtrB[5],
		(float)mtxPtrB[6],
		(float)mtxPtrB[7],
		
		(float)mtxPtrB[8],
		(float)mtxPtrB[9],
		(float)mtxPtrB[10],
		(float)mtxPtrB[11],
		
		(float)mtxPtrB[12],
		(float)mtxPtrB[13],
		(float)mtxPtrB[14],
		(float)mtxPtrB[15]
	};
	
	__m128 rowB[4] = {
		_mm_load_ps(mtxFloatB + 0),
		_mm_load_ps(mtxFloatB + 4),
		_mm_load_ps(mtxFloatB + 8),
		_mm_load_ps(mtxFloatB + 12)
	};
#endif
	
	rowB[0] = _mm_mul_ps(rowB[0], convertScale);
	rowB[1] = _mm_mul_ps(rowB[1], convertScale);
	rowB[2] = _mm_mul_ps(rowB[2], convertScale);
	rowB[3] = _mm_mul_ps(rowB[3], convertScale);
	
	__m128 rowA[4] = {
		_mm_load_ps(mtxPtrA +  0),
		_mm_load_ps(mtxPtrA +  4),
		_mm_load_ps(mtxPtrA +  8),
		_mm_load_ps(mtxPtrA + 12)
	};
	
	__m128 vecB[4];
	__m128 calcRow;
	
	vecB[0] = _mm_shuffle_ps(rowB[0], rowB[0], 0x00);
	vecB[1] = _mm_shuffle_ps(rowB[0], rowB[0], 0x55);
	vecB[2] = _mm_shuffle_ps(rowB[0], rowB[0], 0xAA);
	vecB[3] = _mm_shuffle_ps(rowB[0], rowB[0], 0xFF);
	calcRow = _mm_add_ps( _mm_mul_ps(rowA[0], vecB[0]), _mm_add_ps(_mm_mul_ps(rowA[1], vecB[1]), _mm_add_ps(_mm_mul_ps(rowA[2], vecB[2]), _mm_mul_ps(rowA[3], vecB[3]))) );
	_mm_store_ps(mtxPtrA +  0, calcRow);
	
	vecB[0] = _mm_shuffle_ps(rowB[1], rowB[1], 0x00);
	vecB[1] = _mm_shuffle_ps(rowB[1], rowB[1], 0x55);
	vecB[2] = _mm_shuffle_ps(rowB[1], rowB[1], 0xAA);
	vecB[3] = _mm_shuffle_ps(rowB[1], rowB[1], 0xFF);
	calcRow = _mm_add_ps( _mm_mul_ps(rowA[0], vecB[0]), _mm_add_ps(_mm_mul_ps(rowA[1], vecB[1]), _mm_add_ps(_mm_mul_ps(rowA[2], vecB[2]), _mm_mul_ps(rowA[3], vecB[3]))) );
	_mm_store_ps(mtxPtrA +  4, calcRow);
	
	vecB[0] = _mm_shuffle_ps(rowB[2], rowB[2], 0x00);
	vecB[1] = _mm_shuffle_ps(rowB[2], rowB[2], 0x55);
	vecB[2] = _mm_shuffle_ps(rowB[2], rowB[2], 0xAA);
	vecB[3] = _mm_shuffle_ps(rowB[2], rowB[2], 0xFF);
	calcRow = _mm_add_ps( _mm_mul_ps(rowA[0], vecB[0]), _mm_add_ps(_mm_mul_ps(rowA[1], vecB[1]), _mm_add_ps(_mm_mul_ps(rowA[2], vecB[2]), _mm_mul_ps(rowA[3], vecB[3]))) );
	_mm_store_ps(mtxPtrA +  8, calcRow);
	
	vecB[0] = _mm_shuffle_ps(rowB[3], rowB[3], 0x00);
	vecB[1] = _mm_shuffle_ps(rowB[3], rowB[3], 0x55);
	vecB[2] = _mm_shuffle_ps(rowB[3], rowB[3], 0xAA);
	vecB[3] = _mm_shuffle_ps(rowB[3], rowB[3], 0xFF);
	calcRow = _mm_add_ps( _mm_mul_ps(rowA[0], vecB[0]), _mm_add_ps(_mm_mul_ps(rowA[1], vecB[1]), _mm_add_ps(_mm_mul_ps(rowA[2], vecB[2]), _mm_mul_ps(rowA[3], vecB[3]))) );
	_mm_store_ps(mtxPtrA + 12, calcRow);
}

template<size_t NUM_ROWS>
FORCEINLINE void vector_fix2float(float *mtxPtr, const float divisor)
{
	const __m128 divisor_v128 = _mm_set1_ps(divisor);
	
	for (size_t i = 0; i < NUM_ROWS * 4; i+=4)
	{
		_mm_store_ps( mtxPtr + i, _mm_div_ps(_mm_load_ps(mtxPtr + i), divisor_v128) );
	}
}

#else

void MatrixMultVec4x4(const s32 *__restrict mtxPtr, float *__restrict vecPtr)
{
	_MatrixMultVec4x4_NoSIMD(mtxPtr, vecPtr);
}

void MatrixMultVec3x3(const s32 *__restrict mtxPtr, float *__restrict vecPtr)
{
	const CACHE_ALIGN float mtxFloat[16] = {
		mtxPtr[ 0] / 4096.0f,
		mtxPtr[ 1] / 4096.0f,
		mtxPtr[ 2] / 4096.0f,
		mtxPtr[ 3] / 4096.0f,
		
		mtxPtr[ 4] / 4096.0f,
		mtxPtr[ 5] / 4096.0f,
		mtxPtr[ 6] / 4096.0f,
		mtxPtr[ 7] / 4096.0f,
		
		mtxPtr[ 8] / 4096.0f,
		mtxPtr[ 9] / 4096.0f,
		mtxPtr[10] / 4096.0f,
		mtxPtr[11] / 4096.0f,
		
		mtxPtr[12] / 4096.0f,
		mtxPtr[13] / 4096.0f,
		mtxPtr[14] / 4096.0f,
		mtxPtr[15] / 4096.0f
	};
	
	const float x = vecPtr[0];
	const float y = vecPtr[1];
	const float z = vecPtr[2];
	
	vecPtr[0] = (x * mtxFloat[0]) + (y * mtxFloat[4]) + (z * mtxFloat[ 8]);
	vecPtr[1] = (x * mtxFloat[1]) + (y * mtxFloat[5]) + (z * mtxFloat[ 9]);
	vecPtr[2] = (x * mtxFloat[2]) + (y * mtxFloat[6]) + (z * mtxFloat[10]);
}

void MatrixTranslate(float *__restrict mtxPtr, const float *__restrict vecPtr)
{
	mtxPtr[12] += (mtxPtr[0] * vecPtr[0]) + (mtxPtr[4] * vecPtr[1]) + (mtxPtr[ 8] * vecPtr[2]);
	mtxPtr[13] += (mtxPtr[1] * vecPtr[0]) + (mtxPtr[5] * vecPtr[1]) + (mtxPtr[ 9] * vecPtr[2]);
	mtxPtr[14] += (mtxPtr[2] * vecPtr[0]) + (mtxPtr[6] * vecPtr[1]) + (mtxPtr[10] * vecPtr[2]);
	mtxPtr[15] += (mtxPtr[3] * vecPtr[0]) + (mtxPtr[7] * vecPtr[1]) + (mtxPtr[11] * vecPtr[2]);
}

void MatrixScale(float *__restrict mtxPtr, const float *__restrict vecPtr)
{
	mtxPtr[ 0] *= vecPtr[0];
	mtxPtr[ 1] *= vecPtr[0];
	mtxPtr[ 2] *= vecPtr[0];
	mtxPtr[ 3] *= vecPtr[0];
	
	mtxPtr[ 4] *= vecPtr[1];
	mtxPtr[ 5] *= vecPtr[1];
	mtxPtr[ 6] *= vecPtr[1];
	mtxPtr[ 7] *= vecPtr[1];
	
	mtxPtr[ 8] *= vecPtr[2];
	mtxPtr[ 9] *= vecPtr[2];
	mtxPtr[10] *= vecPtr[2];
	mtxPtr[11] *= vecPtr[2];
}

void MatrixMultiply(float *__restrict mtxPtrA, const s32 *__restrict mtxPtrB)
{
	const CACHE_ALIGN float mtxFloatB[16] = {
		(float)mtxPtrB[ 0],
		(float)mtxPtrB[ 1],
		(float)mtxPtrB[ 2],
		(float)mtxPtrB[ 3],
		
		(float)mtxPtrB[ 4],
		(float)mtxPtrB[ 5],
		(float)mtxPtrB[ 6],
		(float)mtxPtrB[ 7],
		
		(float)mtxPtrB[ 8],
		(float)mtxPtrB[ 9],
		(float)mtxPtrB[10],
		(float)mtxPtrB[11],
		
		(float)mtxPtrB[12],
		(float)mtxPtrB[13],
		(float)mtxPtrB[14],
		(float)mtxPtrB[15]
	};
	
	float tmpMatrix[16];
	
	tmpMatrix[0]  = (mtxPtrA[ 0] * mtxFloatB[ 0]) + (mtxPtrA[ 4] * mtxFloatB[ 1]) + (mtxPtrA[ 8] * mtxFloatB[ 2]) + (mtxPtrA[12] * mtxFloatB[ 3]);
	tmpMatrix[1]  = (mtxPtrA[ 1] * mtxFloatB[ 0]) + (mtxPtrA[ 5] * mtxFloatB[ 1]) + (mtxPtrA[ 9] * mtxFloatB[ 2]) + (mtxPtrA[13] * mtxFloatB[ 3]);
	tmpMatrix[2]  = (mtxPtrA[ 2] * mtxFloatB[ 0]) + (mtxPtrA[ 6] * mtxFloatB[ 1]) + (mtxPtrA[10] * mtxFloatB[ 2]) + (mtxPtrA[14] * mtxFloatB[ 3]);
	tmpMatrix[3]  = (mtxPtrA[ 3] * mtxFloatB[ 0]) + (mtxPtrA[ 7] * mtxFloatB[ 1]) + (mtxPtrA[11] * mtxFloatB[ 2]) + (mtxPtrA[15] * mtxFloatB[ 3]);
	
	tmpMatrix[4]  = (mtxPtrA[ 0] * mtxFloatB[ 4]) + (mtxPtrA[ 4] * mtxFloatB[ 5]) + (mtxPtrA[ 8] * mtxFloatB[ 6]) + (mtxPtrA[12] * mtxFloatB[ 7]);
	tmpMatrix[5]  = (mtxPtrA[ 1] * mtxFloatB[ 4]) + (mtxPtrA[ 5] * mtxFloatB[ 5]) + (mtxPtrA[ 9] * mtxFloatB[ 6]) + (mtxPtrA[13] * mtxFloatB[ 7]);
	tmpMatrix[6]  = (mtxPtrA[ 2] * mtxFloatB[ 4]) + (mtxPtrA[ 6] * mtxFloatB[ 5]) + (mtxPtrA[10] * mtxFloatB[ 6]) + (mtxPtrA[14] * mtxFloatB[ 7]);
	tmpMatrix[7]  = (mtxPtrA[ 3] * mtxFloatB[ 4]) + (mtxPtrA[ 7] * mtxFloatB[ 5]) + (mtxPtrA[11] * mtxFloatB[ 6]) + (mtxPtrA[15] * mtxFloatB[ 7]);
	
	tmpMatrix[8]  = (mtxPtrA[ 0] * mtxFloatB[ 8]) + (mtxPtrA[ 4] * mtxFloatB[ 9]) + (mtxPtrA[ 8] * mtxFloatB[10]) + (mtxPtrA[12] * mtxFloatB[11]);
	tmpMatrix[9]  = (mtxPtrA[ 1] * mtxFloatB[ 8]) + (mtxPtrA[ 5] * mtxFloatB[ 9]) + (mtxPtrA[ 9] * mtxFloatB[10]) + (mtxPtrA[13] * mtxFloatB[11]);
	tmpMatrix[10] = (mtxPtrA[ 2] * mtxFloatB[ 8]) + (mtxPtrA[ 6] * mtxFloatB[ 9]) + (mtxPtrA[10] * mtxFloatB[10]) + (mtxPtrA[14] * mtxFloatB[11]);
	tmpMatrix[11] = (mtxPtrA[ 3] * mtxFloatB[ 8]) + (mtxPtrA[ 7] * mtxFloatB[ 9]) + (mtxPtrA[11] * mtxFloatB[10]) + (mtxPtrA[15] * mtxFloatB[11]);
	
	tmpMatrix[12] = (mtxPtrA[ 0] * mtxFloatB[12]) + (mtxPtrA[ 4] * mtxFloatB[13]) + (mtxPtrA[ 8] * mtxFloatB[14]) + (mtxPtrA[12] * mtxFloatB[15]);
	tmpMatrix[13] = (mtxPtrA[ 1] * mtxFloatB[12]) + (mtxPtrA[ 5] * mtxFloatB[13]) + (mtxPtrA[ 9] * mtxFloatB[14]) + (mtxPtrA[13] * mtxFloatB[15]);
	tmpMatrix[14] = (mtxPtrA[ 2] * mtxFloatB[12]) + (mtxPtrA[ 6] * mtxFloatB[13]) + (mtxPtrA[10] * mtxFloatB[14]) + (mtxPtrA[14] * mtxFloatB[15]);
	tmpMatrix[15] = (mtxPtrA[ 3] * mtxFloatB[12]) + (mtxPtrA[ 7] * mtxFloatB[13]) + (mtxPtrA[11] * mtxFloatB[14]) + (mtxPtrA[15] * mtxFloatB[15]);
	
	memcpy(mtxPtrA, tmpMatrix, sizeof(float)*16);
}

template<size_t NUM_ROWS>
FORCEINLINE void vector_fix2float(float *mtxPtr, const float divisor)
{
	for (size_t i = 0; i < NUM_ROWS * 4; i+=4)
	{
		mtxPtr[i+0] /= divisor;
		mtxPtr[i+1] /= divisor;
		mtxPtr[i+2] /= divisor;
		mtxPtr[i+3] /= divisor;
	}
}

#endif

void MatrixMultVec4x4(const s32 *__restrict mtxPtr, s32 *__restrict vecPtr)
{
	const s32 x = vecPtr[0];
	const s32 y = vecPtr[1];
	const s32 z = vecPtr[2];
	const s32 w = vecPtr[3];
	
	vecPtr[0] = sfx32_shiftdown( fx32_mul(x,mtxPtr[0]) + fx32_mul(y,mtxPtr[4]) + fx32_mul(z,mtxPtr[ 8]) + fx32_mul(w,mtxPtr[12]) );
	vecPtr[1] = sfx32_shiftdown( fx32_mul(x,mtxPtr[1]) + fx32_mul(y,mtxPtr[5]) + fx32_mul(z,mtxPtr[ 9]) + fx32_mul(w,mtxPtr[13]) );
	vecPtr[2] = sfx32_shiftdown( fx32_mul(x,mtxPtr[2]) + fx32_mul(y,mtxPtr[6]) + fx32_mul(z,mtxPtr[10]) + fx32_mul(w,mtxPtr[14]) );
	vecPtr[3] = sfx32_shiftdown( fx32_mul(x,mtxPtr[3]) + fx32_mul(y,mtxPtr[7]) + fx32_mul(z,mtxPtr[11]) + fx32_mul(w,mtxPtr[15]) );
}

void MatrixMultVec3x3(const s32 *__restrict mtxPtr, s32 *__restrict vecPtr)
{
	const s32 x = vecPtr[0];
	const s32 y = vecPtr[1];
	const s32 z = vecPtr[2];
	
	vecPtr[0] = sfx32_shiftdown( fx32_mul(x,mtxPtr[0]) + fx32_mul(y,mtxPtr[4]) + fx32_mul(z,mtxPtr[ 8]) );
	vecPtr[1] = sfx32_shiftdown( fx32_mul(x,mtxPtr[1]) + fx32_mul(y,mtxPtr[5]) + fx32_mul(z,mtxPtr[ 9]) );
	vecPtr[2] = sfx32_shiftdown( fx32_mul(x,mtxPtr[2]) + fx32_mul(y,mtxPtr[6]) + fx32_mul(z,mtxPtr[10]) );
}

void MatrixTranslate(s32 *__restrict mtxPtr, const s32 *__restrict vecPtr)
{
	mtxPtr[12] = sfx32_shiftdown( fx32_mul(mtxPtr[0], vecPtr[0]) + fx32_mul(mtxPtr[4], vecPtr[1]) + fx32_mul(mtxPtr[ 8], vecPtr[2]) + fx32_shiftup(mtxPtr[12]) );
	mtxPtr[13] = sfx32_shiftdown( fx32_mul(mtxPtr[1], vecPtr[0]) + fx32_mul(mtxPtr[5], vecPtr[1]) + fx32_mul(mtxPtr[ 9], vecPtr[2]) + fx32_shiftup(mtxPtr[13]) );
	mtxPtr[14] = sfx32_shiftdown( fx32_mul(mtxPtr[2], vecPtr[0]) + fx32_mul(mtxPtr[6], vecPtr[1]) + fx32_mul(mtxPtr[10], vecPtr[2]) + fx32_shiftup(mtxPtr[14]) );
	mtxPtr[15] = sfx32_shiftdown( fx32_mul(mtxPtr[3], vecPtr[0]) + fx32_mul(mtxPtr[7], vecPtr[1]) + fx32_mul(mtxPtr[11], vecPtr[2]) + fx32_shiftup(mtxPtr[15]) );
}

void MatrixScale(s32 *__restrict mtxPtr, const s32 *__restrict vecPtr)
{
	mtxPtr[ 0] = sfx32_shiftdown( fx32_mul(mtxPtr[ 0], vecPtr[0]) );
	mtxPtr[ 1] = sfx32_shiftdown( fx32_mul(mtxPtr[ 1], vecPtr[0]) );
	mtxPtr[ 2] = sfx32_shiftdown( fx32_mul(mtxPtr[ 2], vecPtr[0]) );
	mtxPtr[ 3] = sfx32_shiftdown( fx32_mul(mtxPtr[ 3], vecPtr[0]) );
	
	mtxPtr[ 4] = sfx32_shiftdown( fx32_mul(mtxPtr[ 4], vecPtr[1]) );
	mtxPtr[ 5] = sfx32_shiftdown( fx32_mul(mtxPtr[ 5], vecPtr[1]) );
	mtxPtr[ 6] = sfx32_shiftdown( fx32_mul(mtxPtr[ 6], vecPtr[1]) );
	mtxPtr[ 7] = sfx32_shiftdown( fx32_mul(mtxPtr[ 7], vecPtr[1]) );
	
	mtxPtr[ 8] = sfx32_shiftdown( fx32_mul(mtxPtr[ 8], vecPtr[2]) );
	mtxPtr[ 9] = sfx32_shiftdown( fx32_mul(mtxPtr[ 9], vecPtr[2]) );
	mtxPtr[10] = sfx32_shiftdown( fx32_mul(mtxPtr[10], vecPtr[2]) );
	mtxPtr[11] = sfx32_shiftdown( fx32_mul(mtxPtr[11], vecPtr[2]) );
}

void MatrixMultiply(s32 *__restrict mtxPtrA, const s32 *__restrict mtxPtrB)
{
	s32 tmpMatrix[16];
	
	tmpMatrix[ 0] = sfx32_shiftdown( fx32_mul(mtxPtrA[0],mtxPtrB[ 0])+fx32_mul(mtxPtrA[4],mtxPtrB[ 1])+fx32_mul(mtxPtrA[ 8],mtxPtrB[ 2])+fx32_mul(mtxPtrA[12],mtxPtrB[ 3]) );
	tmpMatrix[ 1] = sfx32_shiftdown( fx32_mul(mtxPtrA[1],mtxPtrB[ 0])+fx32_mul(mtxPtrA[5],mtxPtrB[ 1])+fx32_mul(mtxPtrA[ 9],mtxPtrB[ 2])+fx32_mul(mtxPtrA[13],mtxPtrB[ 3]) );
	tmpMatrix[ 2] = sfx32_shiftdown( fx32_mul(mtxPtrA[2],mtxPtrB[ 0])+fx32_mul(mtxPtrA[6],mtxPtrB[ 1])+fx32_mul(mtxPtrA[10],mtxPtrB[ 2])+fx32_mul(mtxPtrA[14],mtxPtrB[ 3]) );
	tmpMatrix[ 3] = sfx32_shiftdown( fx32_mul(mtxPtrA[3],mtxPtrB[ 0])+fx32_mul(mtxPtrA[7],mtxPtrB[ 1])+fx32_mul(mtxPtrA[11],mtxPtrB[ 2])+fx32_mul(mtxPtrA[15],mtxPtrB[ 3]) );
	
	tmpMatrix[ 4] = sfx32_shiftdown( fx32_mul(mtxPtrA[0],mtxPtrB[ 4])+fx32_mul(mtxPtrA[4],mtxPtrB[ 5])+fx32_mul(mtxPtrA[ 8],mtxPtrB[ 6])+fx32_mul(mtxPtrA[12],mtxPtrB[ 7]) );
	tmpMatrix[ 5] = sfx32_shiftdown( fx32_mul(mtxPtrA[1],mtxPtrB[ 4])+fx32_mul(mtxPtrA[5],mtxPtrB[ 5])+fx32_mul(mtxPtrA[ 9],mtxPtrB[ 6])+fx32_mul(mtxPtrA[13],mtxPtrB[ 7]) );
	tmpMatrix[ 6] = sfx32_shiftdown( fx32_mul(mtxPtrA[2],mtxPtrB[ 4])+fx32_mul(mtxPtrA[6],mtxPtrB[ 5])+fx32_mul(mtxPtrA[10],mtxPtrB[ 6])+fx32_mul(mtxPtrA[14],mtxPtrB[ 7]) );
	tmpMatrix[ 7] = sfx32_shiftdown( fx32_mul(mtxPtrA[3],mtxPtrB[ 4])+fx32_mul(mtxPtrA[7],mtxPtrB[ 5])+fx32_mul(mtxPtrA[11],mtxPtrB[ 6])+fx32_mul(mtxPtrA[15],mtxPtrB[ 7]) );
	
	tmpMatrix[ 8] = sfx32_shiftdown( fx32_mul(mtxPtrA[0],mtxPtrB[ 8])+fx32_mul(mtxPtrA[4],mtxPtrB[ 9])+fx32_mul(mtxPtrA[ 8],mtxPtrB[10])+fx32_mul(mtxPtrA[12],mtxPtrB[11]) );
	tmpMatrix[ 9] = sfx32_shiftdown( fx32_mul(mtxPtrA[1],mtxPtrB[ 8])+fx32_mul(mtxPtrA[5],mtxPtrB[ 9])+fx32_mul(mtxPtrA[ 9],mtxPtrB[10])+fx32_mul(mtxPtrA[13],mtxPtrB[11]) );
	tmpMatrix[10] = sfx32_shiftdown( fx32_mul(mtxPtrA[2],mtxPtrB[ 8])+fx32_mul(mtxPtrA[6],mtxPtrB[ 9])+fx32_mul(mtxPtrA[10],mtxPtrB[10])+fx32_mul(mtxPtrA[14],mtxPtrB[11]) );
	tmpMatrix[11] = sfx32_shiftdown( fx32_mul(mtxPtrA[3],mtxPtrB[ 8])+fx32_mul(mtxPtrA[7],mtxPtrB[ 9])+fx32_mul(mtxPtrA[11],mtxPtrB[10])+fx32_mul(mtxPtrA[15],mtxPtrB[11]) );
	
	tmpMatrix[12] = sfx32_shiftdown( fx32_mul(mtxPtrA[0],mtxPtrB[12])+fx32_mul(mtxPtrA[4],mtxPtrB[13])+fx32_mul(mtxPtrA[ 8],mtxPtrB[14])+fx32_mul(mtxPtrA[12],mtxPtrB[15]) );
	tmpMatrix[13] = sfx32_shiftdown( fx32_mul(mtxPtrA[1],mtxPtrB[12])+fx32_mul(mtxPtrA[5],mtxPtrB[13])+fx32_mul(mtxPtrA[ 9],mtxPtrB[14])+fx32_mul(mtxPtrA[13],mtxPtrB[15]) );
	tmpMatrix[14] = sfx32_shiftdown( fx32_mul(mtxPtrA[2],mtxPtrB[12])+fx32_mul(mtxPtrA[6],mtxPtrB[13])+fx32_mul(mtxPtrA[10],mtxPtrB[14])+fx32_mul(mtxPtrA[14],mtxPtrB[15]) );
	tmpMatrix[15] = sfx32_shiftdown( fx32_mul(mtxPtrA[3],mtxPtrB[12])+fx32_mul(mtxPtrA[7],mtxPtrB[13])+fx32_mul(mtxPtrA[11],mtxPtrB[14])+fx32_mul(mtxPtrA[15],mtxPtrB[15]) );
	
	memcpy(mtxPtrA, tmpMatrix, sizeof(s32)*16);
}
