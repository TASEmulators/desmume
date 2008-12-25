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

#ifndef MATRIX_H
#define MATRIX_H

#include "types.h"

extern "C" {

typedef struct MatrixStack
{
	float	*matrix;
	int		position;
	int		size;
} MatrixStack;

void	MatrixInit				(float *matrix);

#if defined(_MSC_VER) || defined(__INTEL_COMPILER)
#define MATRIXFASTCALL __fastcall
#else
#define MATRIXFASTCALL
#endif

void	MATRIXFASTCALL MatrixMultVec3x3		(const float * matrix, float * vecPtr);
void	MATRIXFASTCALL MatrixMultVec4x4		(const float * matrix, float * vecPtr);
void	MATRIXFASTCALL MatrixMultiply		(float * matrix, const float * rightMatrix);
void	MATRIXFASTCALL MatrixTranslate		(float *matrix, const float *ptr);
void	MATRIXFASTCALL MatrixScale			(float * matrix, const float * ptr);
float	MATRIXFASTCALL MatrixGetMultipliedIndex	(int index, float *matrix, float *rightMatrix);
void	MATRIXFASTCALL MatrixSet				(float *matrix, int x, int y, float value);
void	MATRIXFASTCALL MatrixCopy				(float * matrixDST, const float * matrixSRC);
void	MATRIXFASTCALL MatrixIdentity			(float *matrix);

void	MatrixTranspose				(float *matrix);
void	MatrixStackInit				(MatrixStack *stack);
void	MatrixStackSetMaxSize		(MatrixStack *stack, int size);
void	MatrixStackSetStackPosition (MatrixStack *stack, int pos);
void	MatrixStackPushMatrix		(MatrixStack *stack, const float *ptr);
float*	MatrixStackPopMatrix		(MatrixStack *stack, int size);
float*	MatrixStackGetPos			(MatrixStack *stack, int pos);
float*	MatrixStackGet				(MatrixStack *stack);
void	MatrixStackLoadMatrix		(MatrixStack *stack, int pos, const float *ptr);

float Vector3Dot(const float *a, const float *b);
float Vector3Length(const float *a);
void Vector3Add(float *dst, const float *src);
void Vector3Scale(float *dst, const float scale);
void Vector3Copy(float *dst, const float *src);
void Vector3Normalize(float *dst);

void Vector4Copy(float *dst, const float *src);

} //extern "C"

#endif
