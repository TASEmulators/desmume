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
#include "matrix.h"

void MatrixInit  (float *matrix)
{
	memset (matrix, 0, sizeof(float)*16);

	matrix[0] = matrix[5] = matrix[10] = matrix[15] = 1.f;
}

void MatrixMultVec4x4 (float *matrix, float *vecPtr)
{
	float x = vecPtr[0];
	float y = vecPtr[1];
	float z = vecPtr[2];

	vecPtr[0] = x * matrix[0] + y * matrix[4] + z * matrix[ 8] + matrix[12];
	vecPtr[1] = x * matrix[1] + y * matrix[5] + z * matrix[ 9] + matrix[13];
	vecPtr[2] = x * matrix[2] + y * matrix[6] + z * matrix[10] + matrix[14];
}

void MatrixMultVec3x3 (float *matrix, float *vecPtr)
{
	float x = vecPtr[0];
	float y = vecPtr[1];
	float z = vecPtr[2];

	vecPtr[0] = x * matrix[0] + y * matrix[4] + z * matrix[ 8];
	vecPtr[1] = x * matrix[1] + y * matrix[5] + z * matrix[ 9];
	vecPtr[2] = x * matrix[2] + y * matrix[6] + z * matrix[10];
}

void MatrixIdentity	(float *matrix)
{
	memset (matrix, 0, sizeof(float)*16);

	matrix[0] = matrix[5] = matrix[10] = matrix[15] = 1.f;
}

void MatrixMultiply (float *matrix, float *rightMatrix)
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
/*
void MatrixMulti (float* right)
{
	float tmpMatrix[16];

	tmpMatrix[0]  = (matrix[0]*right[0])+(matrix[4]*right[1])+(matrix[8]*right[2])+(matrix[12]*right[3]);
	tmpMatrix[1]  = (matrix[1]*right[0])+(matrix[5]*right[1])+(matrix[9]*right[2])+(matrix[13]*right[3]);
	tmpMatrix[2]  = (matrix[2]*right[0])+(matrix[6]*right[1])+(matrix[10]*right[2])+(matrix[14]*right[3]);
	tmpMatrix[3]  = (matrix[3]*right[0])+(matrix[7]*right[1])+(matrix[11]*right[2])+(matrix[15]*right[3]);

	tmpMatrix[4]  = (matrix[0]*right[4])+(matrix[4]*right[5])+(matrix[8]*right[6])+(matrix[12]*right[7]);
	tmpMatrix[5]  = (matrix[1]*right[4])+(matrix[5]*right[5])+(matrix[9]*right[6])+(matrix[13]*right[7]);
	tmpMatrix[6]  = (matrix[2]*right[4])+(matrix[6]*right[5])+(matrix[10]*right[6])+(matrix[14]*right[7]);
	tmpMatrix[7]  = (matrix[3]*right[4])+(matrix[7]*right[5])+(matrix[11]*right[6])+(matrix[15]*right[7]);

	tmpMatrix[8]  = (matrix[0]*right[8])+(matrix[4]*right[9])+(matrix[8]*right[10])+(matrix[12]*right[11]);
	tmpMatrix[9]  = (matrix[1]*right[8])+(matrix[5]*right[9])+(matrix[9]*right[10])+(matrix[13]*right[11]);
	tmpMatrix[10] = (matrix[2]*right[8])+(matrix[6]*right[9])+(matrix[10]*right[10])+(matrix[14]*right[11]);
	tmpMatrix[11] = (matrix[3]*right[8])+(matrix[7]*right[9])+(matrix[11]*right[10])+(matrix[15]*right[11]);

	tmpMatrix[12] = (matrix[0]*right[12])+(matrix[4]*right[13])+(matrix[8]*right[14])+(matrix[12]*right[15]);
	tmpMatrix[13] = (matrix[1]*right[12])+(matrix[5]*right[13])+(matrix[9]*right[14])+(matrix[13]*right[15]);
	tmpMatrix[14] = (matrix[2]*right[12])+(matrix[6]*right[13])+(matrix[10]*right[14])+(matrix[14]*right[15]);
	tmpMatrix[15] = (matrix[3]*right[12])+(matrix[7]*right[13])+(matrix[11]*right[14])+(matrix[15]*right[15]);

	memcpy (matrix, tmpMatrix, sizeof(float)*16);
}


float*	Matrix::Get		(void)
{
	return matrix;
}

float MatrixGet (float *matrix, int index)
{
	return matrix[index];
}
*/

float MatrixGetMultipliedIndex (int index, float *matrix, float *rightMatrix)
{
	int iMod = index%4, iDiv = (index>>2)<<2;

	return	(matrix[iMod  ]*rightMatrix[iDiv  ])+(matrix[iMod+ 4]*rightMatrix[iDiv+1])+
			(matrix[iMod+8]*rightMatrix[iDiv+2])+(matrix[iMod+12]*rightMatrix[iDiv+3]);
}

void MatrixSet (float *matrix, int x, int y, float value)
{
	matrix [x+(y<<2)] = value;
}
/*
void	Matrix::Set		(int pos, float value)
{
	matrix [pos] = value;
}
*/
void MatrixCopy (float *matrixDST, float *matrixSRC)
{
	memcpy (matrixDST, matrixSRC, sizeof(float)*16);
}

void MatrixTranslate	(float *matrix, float *ptr)
{
	matrix[12] += (matrix[0]*ptr[0])+(matrix[4]*ptr[1])+(matrix[ 8]*ptr[2]);
	matrix[13] += (matrix[1]*ptr[0])+(matrix[5]*ptr[1])+(matrix[ 9]*ptr[2]);
	matrix[14] += (matrix[2]*ptr[0])+(matrix[6]*ptr[1])+(matrix[10]*ptr[2]);
	matrix[15] += (matrix[3]*ptr[0])+(matrix[7]*ptr[1])+(matrix[11]*ptr[2]);
}

void MatrixScale (float *matrix, float *ptr)
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
/*
void	Matrix::Set	(float a11, float a21, float a31, float a41,
					 float a12, float a22, float a32, float a42,
					 float a13, float a23, float a33, float a43,
					 float a14, float a24, float a34, float a44)
{
}
*/


//-----------------------------------------

void MatrixStackInit (MatrixStack *stack)
{
	stack->matrix	= NULL;
	stack->position	= 0;
	stack->size		= 0;
}

void MatrixStackSetMaxSize (MatrixStack *stack, int size)
{
	int i = 0;

	stack->size = size;

	if (stack->matrix == NULL)
	{
		stack->matrix = (float*) malloc (stack->size*16*sizeof(float));
	}
	else
	{
		free (stack->matrix);
		stack->matrix = (float*) malloc (stack->size*16*sizeof(float));
	}

	for (i = 0; i < stack->size; i++)
	{
		MatrixInit (&stack->matrix[i*16]);
	}

	stack->size--;
}


void MatrixStackSetStackPosition (MatrixStack *stack, int pos)
{
	stack->position += pos;

	if (stack->position < 0)
		stack->position = 0;
	else if (stack->position > stack->size)	
		stack->position = stack->size;
}

void MatrixStackPushMatrix (MatrixStack *stack, float *ptr)
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
	return &stack->matrix[pos*16];
}

float * MatrixStackGet (MatrixStack *stack)
{
	return &stack->matrix[stack->position*16];
}

void MatrixStackLoadMatrix (MatrixStack *stack, int pos, float *ptr)
{
	MatrixCopy (&stack->matrix[pos*16], ptr);
}
