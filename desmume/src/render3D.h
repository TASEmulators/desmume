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

#ifndef GPU_3D
#define GPU_3D

#ifdef _MSC_VER
#define CALL_CONVENTION __cdecl
#else
#define CALL_CONVENTION
#endif

/*
enum DRIVER_3D
{
	DRIVER_NULL = 0,
	DRIVER_OPENGL
};
*/

typedef struct GPU3DInterface
{
	char (CALL_CONVENTION*  NDS_3D_Init)					(void);
	void (CALL_CONVENTION*  NDS_3D_Reset)					(void);
	void (CALL_CONVENTION*  NDS_3D_Close)					(void);
	void (CALL_CONVENTION*  NDS_3D_ViewPort)				(unsigned long v);
	void (CALL_CONVENTION*  NDS_3D_ClearColor)			(unsigned long v);
	void (CALL_CONVENTION*  NDS_3D_FogColor)				(unsigned long v);
	void (CALL_CONVENTION*  NDS_3D_FogOffset)			(unsigned long v);
	void (CALL_CONVENTION*  NDS_3D_ClearDepth)			(unsigned long v);
	void (CALL_CONVENTION*  NDS_3D_MatrixMode)			(unsigned long v);
	void (CALL_CONVENTION*  NDS_3D_LoadIdentity)			(void);
	void (CALL_CONVENTION*  NDS_3D_LoadMatrix4x4)		(signed long v);
	void (CALL_CONVENTION*  NDS_3D_LoadMatrix4x3)		(signed long v);
	void (CALL_CONVENTION*  NDS_3D_StoreMatrix)			(unsigned long v);
	void (CALL_CONVENTION*  NDS_3D_RestoreMatrix)		(unsigned long v);
	void (CALL_CONVENTION*  NDS_3D_PushMatrix)			(void);
	void (CALL_CONVENTION*  NDS_3D_PopMatrix)			(signed long i);
	void (CALL_CONVENTION*  NDS_3D_Translate)			(signed long v);
	void (CALL_CONVENTION*  NDS_3D_Scale)				(signed long v);
	void (CALL_CONVENTION*  NDS_3D_MultMatrix3x3)		(signed long v);
	void (CALL_CONVENTION*  NDS_3D_MultMatrix4x3)		(signed long v);
	void (CALL_CONVENTION*  NDS_3D_MultMatrix4x4)		(signed long v);
	void (CALL_CONVENTION*  NDS_3D_Begin)				(unsigned long v);
	void (CALL_CONVENTION*  NDS_3D_End)					(void);
	void (CALL_CONVENTION*  NDS_3D_Color3b)				(unsigned long v);
	void (CALL_CONVENTION*  NDS_3D_Vertex16b)			(unsigned int v);
	void (CALL_CONVENTION*  NDS_3D_Vertex10b)			(unsigned long v);
	void (CALL_CONVENTION*  NDS_3D_Vertex3_cord)			(unsigned int one, unsigned int two, unsigned int v);
	void (CALL_CONVENTION*  NDS_3D_Vertex_rel)			(unsigned long v);
	void (CALL_CONVENTION*  NDS_3D_SwapScreen)			(unsigned int screen);
	int  (CALL_CONVENTION*  NDS_3D_GetNumPolys)			(void); // THIS IS A HACK :D
	int  (CALL_CONVENTION*  NDS_3D_GetNumVertex)			(void);
	void (CALL_CONVENTION*  NDS_3D_Flush)				(unsigned long v);
	void (CALL_CONVENTION*  NDS_3D_PolygonAttrib)		(unsigned long val);
	void (CALL_CONVENTION*  NDS_3D_Material0)			(unsigned long val);
	void (CALL_CONVENTION*  NDS_3D_Material1)			(unsigned long val);
	void (CALL_CONVENTION*  NDS_3D_Shininess)			(unsigned long val);
	void (CALL_CONVENTION*  NDS_3D_TexImage)				(unsigned long val);
	void (CALL_CONVENTION*  NDS_3D_TexPalette)			(unsigned long val);
	void (CALL_CONVENTION*  NDS_3D_TexCoord)				(unsigned long val);
	void (CALL_CONVENTION*  NDS_3D_LightDirection)		(unsigned long v);
	void (CALL_CONVENTION*  NDS_3D_LightColor)			(unsigned long v);
	void (CALL_CONVENTION*  NDS_3D_AlphaFunc)			(unsigned long v);
	void (CALL_CONVENTION*  NDS_3D_Control)				(unsigned long v);
	void (CALL_CONVENTION*  NDS_3D_Normal)				(unsigned long v);
	void (CALL_CONVENTION*  NDS_3D_CallList)				(unsigned long v);

	long (CALL_CONVENTION*  NDS_3D_GetClipMatrix)		(unsigned int index);
	long (CALL_CONVENTION*  NDS_3D_GetDirectionalMatrix)	(unsigned int index);
	void (CALL_CONVENTION*  NDS_3D_GetLine)				(int line, unsigned short * DST);

	//////////////////////////////////////////////////////////////////////////////
	// NDS_3D_GetMatrix
	//
	// mode:	0 = projection
	//			1 = coordinate
	//			2 = direction
	//			3 = texture
	//
	// index:	the matrix stack index or -1 for current
	//
	// dest:	pointer to the destination float[16] buffer
	//////////////////////////////////////////////////////////////////////////////
	void (CALL_CONVENTION*  NDS_3D_GetMatrix)			(unsigned int mode, unsigned int index, float* dest);

	//////////////////////////////////////////////////////////////////////////////
	// NDS_glGetLightDirection
	//
	// index:	light index
	//
	// dest:	pointer to the destination variable
	//////////////////////////////////////////////////////////////////////////////
	void (CALL_CONVENTION*  NDS_glGetLightDirection)	(unsigned int index, unsigned int* dest);

	//////////////////////////////////////////////////////////////////////////////
	// NDS_glGetLightColor
	//
	// index:	light index
	//
	// dest:	pointer to the destination variable
	//////////////////////////////////////////////////////////////////////////////
	void (CALL_CONVENTION*  NDS_glGetLightColor)	(unsigned int index, unsigned int* dest);
	
	void (CALL_CONVENTION*  NDS_3D_BoxTest)				(unsigned long v);
	void (CALL_CONVENTION*  NDS_3D_PosTest)				(unsigned long v);
	void (CALL_CONVENTION*  NDS_3D_VecTest)				(unsigned long v);
	long (CALL_CONVENTION*  NDS_3D_GetPosRes)			(unsigned int index);
	long (CALL_CONVENTION*  NDS_3D_GetVecRes)			(unsigned int index);


} GPU3DInterface;

// gpu 3D core list, per port
extern GPU3DInterface *core3DList[];

// Default null plugin
#define GPU3D_NULL 0
extern GPU3DInterface gpu3DNull;

// Extern pointer
extern GPU3DInterface *gpu3D;

void NDS_3D_SetDriver (int core3DIndex);

#endif
