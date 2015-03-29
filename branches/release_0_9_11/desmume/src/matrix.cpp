/*
	Copyright (C) 2006-2007 shash
	Copyright (C) 2007-2012 DeSmuME team

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

void _NOSSE_MatrixMultVec4x4 (const float *matrix, float *vecPtr)
{
	float x = vecPtr[0];
	float y = vecPtr[1];
	float z = vecPtr[2];
	float w = vecPtr[3];

	vecPtr[0] = x * matrix[0] + y * matrix[4] + z * matrix[ 8] + w * matrix[12];
	vecPtr[1] = x * matrix[1] + y * matrix[5] + z * matrix[ 9] + w * matrix[13];
	vecPtr[2] = x * matrix[2] + y * matrix[6] + z * matrix[10] + w * matrix[14];
	vecPtr[3] = x * matrix[3] + y * matrix[7] + z * matrix[11] + w * matrix[15];
}

void MatrixMultVec4x4 (const s32 *matrix, s32 *vecPtr)
{
	const s32 x = vecPtr[0];
	const s32 y = vecPtr[1];
	const s32 z = vecPtr[2];
	const s32 w = vecPtr[3];

	vecPtr[0] = fx32_shiftdown(fx32_mul(x,matrix[0]) + fx32_mul(y,matrix[4]) + fx32_mul(z,matrix [8]) + fx32_mul(w,matrix[12]));
	vecPtr[1] = fx32_shiftdown(fx32_mul(x,matrix[1]) + fx32_mul(y,matrix[5]) + fx32_mul(z,matrix[ 9]) + fx32_mul(w,matrix[13]));
	vecPtr[2] = fx32_shiftdown(fx32_mul(x,matrix[2]) + fx32_mul(y,matrix[6]) + fx32_mul(z,matrix[10]) + fx32_mul(w,matrix[14]));
	vecPtr[3] = fx32_shiftdown(fx32_mul(x,matrix[3]) + fx32_mul(y,matrix[7]) + fx32_mul(z,matrix[11]) + fx32_mul(w,matrix[15]));
}

void MatrixMultVec3x3_fixed(const s32 *matrix, s32 *vecPtr)
{
	const s32 x = vecPtr[0];
	const s32 y = vecPtr[1];
	const s32 z = vecPtr[2];

	vecPtr[0] = fx32_shiftdown(fx32_mul(x,matrix[0]) + fx32_mul(y,matrix[4]) + fx32_mul(z,matrix[8]));
	vecPtr[1] = fx32_shiftdown(fx32_mul(x,matrix[1]) + fx32_mul(y,matrix[5]) + fx32_mul(z,matrix[9]));
	vecPtr[2] = fx32_shiftdown(fx32_mul(x,matrix[2]) + fx32_mul(y,matrix[6]) + fx32_mul(z,matrix[10]));
}

//-------------------------
//switched SSE functions: implementations for no SSE
#ifndef ENABLE_SSE
void MatrixMultVec4x4 (const float *matrix, float *vecPtr)
{
	_NOSSE_MatrixMultVec4x4(matrix, vecPtr);
}


void MatrixMultVec3x3 (const float *matrix, float *vecPtr)
{
	float x = vecPtr[0];
	float y = vecPtr[1];
	float z = vecPtr[2];

	vecPtr[0] = x * matrix[0] + y * matrix[4] + z * matrix[ 8];
	vecPtr[1] = x * matrix[1] + y * matrix[5] + z * matrix[ 9];
	vecPtr[2] = x * matrix[2] + y * matrix[6] + z * matrix[10];
}

void MatrixMultiply (float *matrix, const float *rightMatrix)
{
	float tmpMatrix[16];

	tmpMatrix[0]  = (matrix[0]*rightMatrix[0])+(matrix[4]*rightMatrix[1])+(matrix[8]*rightMatrix[2])+(matrix[12]*rightMatrix[3]);
	tmpMatrix[1]  = (matrix[1]*rightMatrix[0])+(matrix[5]*rightMatrix[1])+(matrix[9]*rightMatrix[2])+(matrix[13]*rightMatrix[3]);
	tmpMatrix[2]  = (matrix[2]*rightMatrix[0])+(matrix[6]*rightMatrix[1])+(matrix[10]*rightMatrix[2])+(matrix[14]*rightMatrix[3]);
	tmpMatrix[3]  = (matrix[3]*rightMatrix[0])+(matrix[7]*rightMatrix[1])+(matrix[11]*rightMatrix[2])+(matrix[15]*rightMatrix[3]);

	tmpMatrix[4]  = (matrix[0]*rightMatrix[4])+(matrix[4]*rightMatrix[5])+(matrix[8]*rightMatrix[6])+(matrix[12]*rightMatrix[7]);
	tmpMatrix[5]  = (matrix[1]*rightMatrix[4])+(matrix[5]*rightMatrix[5])+(matrix[9]*rightMatrix[6])+(matrix[13]*rightMatrix[7]);
	tmpMatrix[6]  = (matrix[2]*rightMatrix[4])+(matrix[6]*rightMatrix[5])+(matrix[10]*rightMatrix[6])+(matrix[14]*rightMatrix[7]);
	tmpMatrix[7]  = (matrix[3]*rightMatrix[4])+(matrix[7]*rightMatrix[5])+(matrix[11]*rightMatrix[6])+(matrix[15]*rightMatrix[7]);

	tmpMatrix[8]  = (matrix[0]*rightMatrix[8])+(matrix[4]*rightMatrix[9])+(matrix[8]*rightMatrix[10])+(matrix[12]*rightMatrix[11]);
	tmpMatrix[9]  = (matrix[1]*rightMatrix[8])+(matrix[5]*rightMatrix[9])+(matrix[9]*rightMatrix[10])+(matrix[13]*rightMatrix[11]);
	tmpMatrix[10] = (matrix[2]*rightMatrix[8])+(matrix[6]*rightMatrix[9])+(matrix[10]*rightMatrix[10])+(matrix[14]*rightMatrix[11]);
	tmpMatrix[11] = (matrix[3]*rightMatrix[8])+(matrix[7]*rightMatrix[9])+(matrix[11]*rightMatrix[10])+(matrix[15]*rightMatrix[11]);

	tmpMatrix[12] = (matrix[0]*rightMatrix[12])+(matrix[4]*rightMatrix[13])+(matrix[8]*rightMatrix[14])+(matrix[12]*rightMatrix[15]);
	tmpMatrix[13] = (matrix[1]*rightMatrix[12])+(matrix[5]*rightMatrix[13])+(matrix[9]*rightMatrix[14])+(matrix[13]*rightMatrix[15]);
	tmpMatrix[14] = (matrix[2]*rightMatrix[12])+(matrix[6]*rightMatrix[13])+(matrix[10]*rightMatrix[14])+(matrix[14]*rightMatrix[15]);
	tmpMatrix[15] = (matrix[3]*rightMatrix[12])+(matrix[7]*rightMatrix[13])+(matrix[11]*rightMatrix[14])+(matrix[15]*rightMatrix[15]);

	memcpy (matrix, tmpMatrix, sizeof(float)*16);
}

void MatrixTranslate	(float *matrix, const float *ptr)
{
	matrix[12] += (matrix[0]*ptr[0])+(matrix[4]*ptr[1])+(matrix[ 8]*ptr[2]);
	matrix[13] += (matrix[1]*ptr[0])+(matrix[5]*ptr[1])+(matrix[ 9]*ptr[2]);
	matrix[14] += (matrix[2]*ptr[0])+(matrix[6]*ptr[1])+(matrix[10]*ptr[2]);
	matrix[15] += (matrix[3]*ptr[0])+(matrix[7]*ptr[1])+(matrix[11]*ptr[2]);
}

void MatrixScale (float *matrix, const float *ptr)
{
	matrix[0]  *= ptr[0];
	matrix[1]  *= ptr[0];
	matrix[2]  *= ptr[0];
	matrix[3]  *= ptr[0];

	matrix[4]  *= ptr[1];
	matrix[5]  *= ptr[1];
	matrix[6]  *= ptr[1];
	matrix[7]  *= ptr[1];

	matrix[8] *= ptr[2];
	matrix[9] *= ptr[2];
	matrix[10] *= ptr[2];
	matrix[11] *= ptr[2];
}

#endif //switched c/asm functions
//-----------------------------------------

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

s32 MatrixGetMultipliedIndex (int index, s32 *matrix, s32 *rightMatrix)
{
	int iMod = index%4, iDiv = (index>>2)<<2;

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

	stack->size = (size + 1);

	if (stack->matrix != NULL) {
		free (stack->matrix);
	}
	stack->matrix = new s32[stack->size*16*sizeof(s32)];

	for (i = 0; i < stack->size; i++)
	{
		MatrixInit (&stack->matrix[i*16]);
	}

	stack->size--;
}


MatrixStack::MatrixStack(int size, int type)
{
	MatrixStackSetMaxSize(this,size);
	this->type = type;
}

static void MatrixStackSetStackPosition (MatrixStack *stack, int pos)
{
	stack->position += pos;

	if((stack->position < 0) || (stack->position > stack->size))
		MMU_new.gxstat.se = 1;

	//once upon a time, we tried clamping to the size.
	//this utterly broke sims 2 apartment pets.
	//changing to wrap around made it work perfectly
	stack->position = ((u32)stack->position) & stack->size;
}

void MatrixStackPushMatrix (MatrixStack *stack, const s32 *ptr)
{
	//printf("Push %i pos %i\n", stack->type, stack->position);
	if ((stack->type == 0) || (stack->type == 3))
		MatrixCopy (&stack->matrix[0], ptr);
	else
		MatrixCopy (&stack->matrix[stack->position*16], ptr);
	MatrixStackSetStackPosition (stack, 1);
}

void MatrixStackPopMatrix (s32 *mtxCurr, MatrixStack *stack, int size)
{
	//printf("Pop %i pos %i (change %d)\n", stack->type, stack->position, -size);
	MatrixStackSetStackPosition(stack, -size);
	if ((stack->type == 0) || (stack->type == 3))
		MatrixCopy (mtxCurr, &stack->matrix[0]);
	else
		MatrixCopy (mtxCurr, &stack->matrix[stack->position*16]);
}

s32 * MatrixStackGetPos (MatrixStack *stack, int pos)
{
	assert(pos<31);
	return &stack->matrix[pos*16];
}

s32 * MatrixStackGet (MatrixStack *stack)
{
	return &stack->matrix[stack->position*16];
}

void MatrixStackLoadMatrix (MatrixStack *stack, int pos, const s32 *ptr)
{
	assert(pos<31);
	MatrixCopy (&stack->matrix[pos*16], ptr);
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


void MatrixMultiply (s32 *matrix, const s32 *rightMatrix)
{
	s32 tmpMatrix[16];

	tmpMatrix[0]  = fx32_shiftdown(fx32_mul(matrix[0],rightMatrix[0])+fx32_mul(matrix[4],rightMatrix[1])+fx32_mul(matrix[8],rightMatrix[2])+fx32_mul(matrix[12],rightMatrix[3]));
	tmpMatrix[1]  = fx32_shiftdown(fx32_mul(matrix[1],rightMatrix[0])+fx32_mul(matrix[5],rightMatrix[1])+fx32_mul(matrix[9],rightMatrix[2])+fx32_mul(matrix[13],rightMatrix[3]));
	tmpMatrix[2]  = fx32_shiftdown(fx32_mul(matrix[2],rightMatrix[0])+fx32_mul(matrix[6],rightMatrix[1])+fx32_mul(matrix[10],rightMatrix[2])+fx32_mul(matrix[14],rightMatrix[3]));
	tmpMatrix[3]  = fx32_shiftdown(fx32_mul(matrix[3],rightMatrix[0])+fx32_mul(matrix[7],rightMatrix[1])+fx32_mul(matrix[11],rightMatrix[2])+fx32_mul(matrix[15],rightMatrix[3]));

	tmpMatrix[4]  = fx32_shiftdown(fx32_mul(matrix[0],rightMatrix[4])+fx32_mul(matrix[4],rightMatrix[5])+fx32_mul(matrix[8],rightMatrix[6])+fx32_mul(matrix[12],rightMatrix[7]));
	tmpMatrix[5]  = fx32_shiftdown(fx32_mul(matrix[1],rightMatrix[4])+fx32_mul(matrix[5],rightMatrix[5])+fx32_mul(matrix[9],rightMatrix[6])+fx32_mul(matrix[13],rightMatrix[7]));
	tmpMatrix[6]  = fx32_shiftdown(fx32_mul(matrix[2],rightMatrix[4])+fx32_mul(matrix[6],rightMatrix[5])+fx32_mul(matrix[10],rightMatrix[6])+fx32_mul(matrix[14],rightMatrix[7]));
	tmpMatrix[7]  = fx32_shiftdown(fx32_mul(matrix[3],rightMatrix[4])+fx32_mul(matrix[7],rightMatrix[5])+fx32_mul(matrix[11],rightMatrix[6])+fx32_mul(matrix[15],rightMatrix[7]));

	tmpMatrix[8]  = fx32_shiftdown(fx32_mul(matrix[0],rightMatrix[8])+fx32_mul(matrix[4],rightMatrix[9])+fx32_mul(matrix[8],rightMatrix[10])+fx32_mul(matrix[12],rightMatrix[11]));
	tmpMatrix[9]  = fx32_shiftdown(fx32_mul(matrix[1],rightMatrix[8])+fx32_mul(matrix[5],rightMatrix[9])+fx32_mul(matrix[9],rightMatrix[10])+fx32_mul(matrix[13],rightMatrix[11]));
	tmpMatrix[10] = fx32_shiftdown(fx32_mul(matrix[2],rightMatrix[8])+fx32_mul(matrix[6],rightMatrix[9])+fx32_mul(matrix[10],rightMatrix[10])+fx32_mul(matrix[14],rightMatrix[11]));
	tmpMatrix[11] = fx32_shiftdown(fx32_mul(matrix[3],rightMatrix[8])+fx32_mul(matrix[7],rightMatrix[9])+fx32_mul(matrix[11],rightMatrix[10])+fx32_mul(matrix[15],rightMatrix[11]));

	tmpMatrix[12] = fx32_shiftdown(fx32_mul(matrix[0],rightMatrix[12])+fx32_mul(matrix[4],rightMatrix[13])+fx32_mul(matrix[8],rightMatrix[14])+fx32_mul(matrix[12],rightMatrix[15]));
	tmpMatrix[13] = fx32_shiftdown(fx32_mul(matrix[1],rightMatrix[12])+fx32_mul(matrix[5],rightMatrix[13])+fx32_mul(matrix[9],rightMatrix[14])+fx32_mul(matrix[13],rightMatrix[15]));
	tmpMatrix[14] = fx32_shiftdown(fx32_mul(matrix[2],rightMatrix[12])+fx32_mul(matrix[6],rightMatrix[13])+fx32_mul(matrix[10],rightMatrix[14])+fx32_mul(matrix[14],rightMatrix[15]));
	tmpMatrix[15] = fx32_shiftdown(fx32_mul(matrix[3],rightMatrix[12])+fx32_mul(matrix[7],rightMatrix[13])+fx32_mul(matrix[11],rightMatrix[14])+fx32_mul(matrix[15],rightMatrix[15]));

	memcpy(matrix,tmpMatrix,sizeof(s32)*16);
}

void MatrixScale(s32 *matrix, const s32 *ptr)
{
	//zero 21-sep-2010 - verified unrolling seems faster on my cpu
	MACRODO_N(12,
		matrix[X] = fx32_shiftdown(fx32_mul(matrix[X],ptr[X>>2]))
		);
}

void MatrixTranslate(s32 *matrix, const s32 *ptr)
{
	MACRODO_N(4,
	{
		s64 temp = fx32_shiftup(matrix[X+12]);
		temp += fx32_mul(matrix[X+0],ptr[0]);
		temp += fx32_mul(matrix[X+4],ptr[1]);
		temp += fx32_mul(matrix[X+8],ptr[2]);
		matrix[X+12] = fx32_shiftdown(temp);
	});
}

void MatrixMultVec4x4_M2(const s32 *matrix, s32 *vecPtr)
{
	MatrixMultVec4x4(matrix+16,vecPtr);
	MatrixMultVec4x4(matrix,vecPtr);
}
