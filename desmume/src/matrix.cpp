/*  
	Copyright (C) 2006-2007 shash

    This file is part of DeSmuME

    DeSmuME is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    DeSmuME is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with DeSmuME; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
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

void MatrixInit  (float *matrix)
{
	memset (matrix, 0, sizeof(float)*16);
	matrix[0] = matrix[5] = matrix[10] = matrix[15] = 1.f;
}

void MatrixTranspose(float *matrix)
{
	float temp;
#define swap(A,B) temp = matrix[A];matrix[A] = matrix[B]; matrix[B] = temp;
	swap(1,4);
	swap(2,8);
	swap(3,0xC);
	swap(6,9);
	swap(7,0xD);
	swap(0xB,0xE);
#undef swap
}

void MatrixIdentity	(float *matrix)
{
	matrix[1] = matrix[2] = matrix[3] = matrix[4] = 0.0f;
	matrix[6] = matrix[7] = matrix[8] = matrix[9] = 0.0f;
	matrix[11] = matrix[12] = matrix[13] = matrix[14] = 0.0f;
	matrix[0] = matrix[5] = matrix[10] = matrix[15] = 1.f;
}

float MatrixGetMultipliedIndex (int index, float *matrix, float *rightMatrix)
{
	int iMod = index%4, iDiv = (index>>2)<<2;

	return	(matrix[iMod  ]*rightMatrix[iDiv  ])+(matrix[iMod+ 4]*rightMatrix[iDiv+1])+
			(matrix[iMod+8]*rightMatrix[iDiv+2])+(matrix[iMod+12]*rightMatrix[iDiv+3]);
}

void MatrixSet (float *matrix, int x, int y, float value)	// TODO
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

int MatrixCompare (const float* matrixDST, const float* matrixSRC)
{
	return memcmp((void*)matrixDST, matrixSRC, sizeof(float)*16);
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
	stack->matrix = (float*) malloc (stack->size*16*sizeof(float));

	for (i = 0; i < stack->size; i++)
	{
		MatrixInit (&stack->matrix[i*16]);
	}

	stack->size--;
}


MatrixStack::MatrixStack(int size)
{
	MatrixStackSetMaxSize(this,size);
}

void MatrixStackSetStackPosition (MatrixStack *stack, int pos)
{
	//printf("SetPosition: %d by %d",stack->position,pos);
	stack->position += pos;

	//this wraparound behavior fixed sims apartment pets which was constantly going up to 32
	s32 newpos = stack->position;
	stack->position &= (stack->size);

	if(newpos != stack->position)
		MMU_new.gxstat.se = 1;

	//printf(" to %d (size %d)\n",stack->position,stack->size);
}

void MatrixStackPushMatrix (MatrixStack *stack, const float *ptr)
{
	MatrixCopy (&stack->matrix[stack->position*16], ptr);

	MatrixStackSetStackPosition (stack, 1);
}

float * MatrixStackPopMatrix (MatrixStack *stack, int size)
{
	MatrixStackSetStackPosition(stack, -size);

	return &stack->matrix[stack->position*16];
}

float * MatrixStackGetPos (MatrixStack *stack, int pos)
{
	assert(pos<31);
	return &stack->matrix[pos*16];
}

float * MatrixStackGet (MatrixStack *stack)
{
	return &stack->matrix[stack->position*16];
}

void MatrixStackLoadMatrix (MatrixStack *stack, int pos, const float *ptr)
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


