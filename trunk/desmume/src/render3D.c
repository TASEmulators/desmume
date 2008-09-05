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

#include "render3D.h"

char NDS_nullFunc1		(void){ return 1; }
void NDS_nullFunc2		(void){}
void NDS_nullFunc3		(unsigned long v){}
void NDS_nullFunc4		(signed long v)	{}
void NDS_nullFunc5		(unsigned int v){}
void NDS_nullFunc6		(unsigned int one, unsigned int two, unsigned int v){}
int  NDS_nullFunc7		(void) {return 0;}
long NDS_nullFunc8		(unsigned int index){ return 0; }
void NDS_nullFunc9		(int line, unsigned short * DST) { };
void NDS_nullFunc10		(unsigned int mode, unsigned int index, float* dest) {}; // NDS_3D_GetMatrix
void NDS_nullFunc11		(unsigned int index, unsigned int* dest) {}; // NDS_glGetLightDirection

GPU3DInterface gpu3DNull = { 
						NDS_nullFunc1,	// NDS_3D_Init
						NDS_nullFunc2,	// NDS_3D_Reset
						NDS_nullFunc2,	// NDS_3D_Close
						NDS_nullFunc3,	// NDS_3D_ViewPort
						NDS_nullFunc3,	// NDS_3D_ClearColor
						NDS_nullFunc3,	// NDS_3D_FogColor
						NDS_nullFunc3,	// NDS_3D_FogOffset
						NDS_nullFunc3,	// NDS_3D_ClearDepth
						NDS_nullFunc3,	// NDS_3D_MatrixMode
						NDS_nullFunc2,	// NDS_3D_LoadIdentity
						NDS_nullFunc4,	// NDS_3D_LoadMatrix4x4
						NDS_nullFunc4,	// NDS_3D_LoadMatrix4x3
						NDS_nullFunc3,	// NDS_3D_StoreMatrix
						NDS_nullFunc3,	// NDS_3D_RestoreMatrix
						NDS_nullFunc2,	// NDS_3D_PushMatrix
						NDS_nullFunc4,	// NDS_3D_PopMatrix
						NDS_nullFunc4,	// NDS_3D_Translate
						NDS_nullFunc4,	// NDS_3D_Scale
						NDS_nullFunc4,	// NDS_3D_MultMatrix3x3
						NDS_nullFunc4,	// NDS_3D_MultMatrix4x3
						NDS_nullFunc4,	// NDS_3D_MultMatrix4x4
						NDS_nullFunc3,	// NDS_3D_Begin
						NDS_nullFunc2,	// NDS_3D_End
						NDS_nullFunc3,	// NDS_3D_Color3b
						NDS_nullFunc5,	// NDS_3D_Vertex16b
						NDS_nullFunc3,	// NDS_3D_Vertex10b
						NDS_nullFunc6,	// NDS_3D_Vertex3_cord
						NDS_nullFunc3,	// NDS_3D_Vertex_rel
						NDS_nullFunc5,	// NDS_3D_SwapScreen
						NDS_nullFunc7,	// NDS_3D_GetNumPolys
						NDS_nullFunc7,	// NDS_3D_GetNumVertex
						NDS_nullFunc3,	// NDS_3D_Flush
						NDS_nullFunc3,	// NDS_3D_PolygonAttrib
						NDS_nullFunc3,	// NDS_3D_Material0
						NDS_nullFunc3,	// NDS_3D_Material1
						NDS_nullFunc3,	// NDS_3D_Shininess
						NDS_nullFunc3,	// NDS_3D_TexImage
						NDS_nullFunc3,	// NDS_3D_TexPalette
						NDS_nullFunc3,	// NDS_3D_TexCoord
						NDS_nullFunc3,	// NDS_3D_LightDirection
						NDS_nullFunc3,	// NDS_3D_LightColor
						NDS_nullFunc3,	// NDS_3D_AlphaFunc
						NDS_nullFunc3,	// NDS_3D_Control
						NDS_nullFunc3,	// NDS_3D_Normal
						NDS_nullFunc3,	// NDS_3D_CallList
						
						NDS_nullFunc8,	// NDS_3D_GetClipMatrix
						NDS_nullFunc8,	// NDS_3D_GetDirectionalMatrix
						NDS_nullFunc9,	// NDS_3D_GetLine

						NDS_nullFunc10,	// NDS_3D_GetMatrix
						NDS_nullFunc11,	// NDS_glGetLightDirection
						NDS_nullFunc11,	// NDS_glGetLightColor
						
						NDS_nullFunc3,	// NDS_3D_BoxTest
						NDS_nullFunc3,	// NDS_3D_PosTest
						NDS_nullFunc3,	// NDS_3D_VecTest

						NDS_nullFunc8,	// NDS_3D_GetPosRes
						NDS_nullFunc8	// NDS_3D_GetVecRes
};

GPU3DInterface *gpu3D = &gpu3DNull;

void NDS_3D_SetDriver (int core3DIndex)
{
	gpu3D = core3DList[core3DIndex];
}
