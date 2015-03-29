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

typedef struct MatrixStack
{
	float	*matrix;
	int		position;
	int		size;
} MatrixStack;

void	MatrixInit				(float *matrix);
void	MatrixMultVec3x3		(float *matrix, float *vecPtr);
void	MatrixMultVec4x4		(float *matrix, float *vecPtr);
void	MatrixIdentity			(float *matrix);
void	MatrixMultiply			(float *matrix, float *rightMatrix);
float	MatrixGetMultipliedIndex(int index, float *matrix, float *rightMatrix);
void	MatrixSet				(float *matrix, int x, int y, float value);
void	MatrixCopy				(float *matrixDST, float *matrixSRC);
void	MatrixTranslate			(float *matrix, float *ptr);
void	MatrixScale				(float *matrix, float *ptr);

void	MatrixStackInit				(MatrixStack *stack);
void	MatrixStackSetMaxSize		(MatrixStack *stack, int size);
void	MatrixStackSetStackPosition (MatrixStack *stack, int pos);
void	MatrixStackPushMatrix		(MatrixStack *stack, float *ptr);
float*	MatrixStackPopMatrix		(MatrixStack *stack, int size);
float*	MatrixStackGetPos			(MatrixStack *stack, int pos);
float*	MatrixStackGet				(MatrixStack *stack);
void	MatrixStackLoadMatrix		(MatrixStack *stack, int pos, float *ptr);

#endif
