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

#include <math.h>

#include "types.h"

extern "C" {

struct MatrixStack
{
	MatrixStack(int size);
	float	*matrix;
	int		position;
	int		size;
};

void	MatrixInit				(float *matrix);

#if defined(_MSC_VER) || defined(__INTEL_COMPILER)
#define MATRIXFASTCALL __fastcall
#else
#define MATRIXFASTCALL
#endif

//In order to conditionally use these asm optimized functions in visual studio
//without having to make new build types to exclude the assembly files.
//a bit sloppy, but there aint much to it
#ifndef NOSSE2
#define SSE2_FUNC(X) _sse2_##X
#define MatrixMultVec4x4 _sse2_MatrixMultVec4x4
#define MatrixMultVec3x3 _sse2_MatrixMultVec3x3
#define MatrixMultiply _sse2_MatrixMultiply
#define MatrixTranslate _sse2_MatrixTranslate
#define MatrixScale _sse2_MatrixScale
#else
#define SSE2_FUNC(X) X
#endif

void	MATRIXFASTCALL SSE2_FUNC(MatrixMultVec3x3)		(const float * matrix, float * vecPtr);
void	MATRIXFASTCALL SSE2_FUNC(MatrixMultVec4x4)		(const float * matrix, float * vecPtr);
void	MATRIXFASTCALL SSE2_FUNC(MatrixMultiply)		(float * matrix, const float * rightMatrix);
void	MATRIXFASTCALL SSE2_FUNC(MatrixTranslate)		(float *matrix, const float *ptr);
void	MATRIXFASTCALL SSE2_FUNC(MatrixScale)			(float * matrix, const float * ptr);



float	MATRIXFASTCALL MatrixGetMultipliedIndex	(int index, float *matrix, float *rightMatrix);
void	MATRIXFASTCALL MatrixSet				(float *matrix, int x, int y, float value);
void	MATRIXFASTCALL MatrixCopy				(float * matrixDST, const float * matrixSRC);
int		MATRIXFASTCALL MatrixCompare				(const float * matrixDST, const float * matrixSRC);
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

} //extern "C"

//these functions are an unreliable, inaccurate floor.
//it should only be used for positive numbers
//this isnt as fast as it could be if we used a visual c++ intrinsic, but those appear not to be universally available
FORCEINLINE u32 u32floor(float f)
{
#ifndef NOSSE2
	__asm cvttss2si eax, f;
#else
	return (u32)f;
#endif
}
FORCEINLINE u32 u32floor(double d)
{
#ifndef NOSSE2
	__asm cvttsd2si eax, d;
#else
	return (u32)d;
#endif
}

//same as above but works for negative values too.
//be sure that the results are the same thing as floorf!
FORCEINLINE s32 s32floor(float f)
{
#ifndef NOSSE2
	static const float c = -0.5f;
	__asm
	{
		movss xmm0, f;
		addss xmm0, xmm0;
		addss xmm0, c;
		cvtss2si eax, xmm0
		sar eax, 1
	}
#else
	return (s32)floorf(f);
#endif
}


#endif
