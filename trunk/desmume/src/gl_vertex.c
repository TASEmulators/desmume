/* gl_vertex.c - this file is part of DeSmuME
 *
 * Copyright (C) 2007 Damien Nozay (damdoum)
 * Author: damdoum at users.sourceforge.net
 *
 * based on http://nocash.emubase.de/gbatek.htm
 *
 * This file is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * This file is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; see the file COPYING.  If not, write to
 * the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include "gl_vertex.h"


/*
    credits goes to :
    http://nocash.emubase.de/gbatek.htm#ds3dvideo

    missing functions
    
Geometry Commands (can be invoked by Port Address, or by Command ID)
Table shows Port Address, Command ID, Number of Parameters, and Clock Cycles.

  Address  Cmd Pa.Cy.
  N/A      00h -  -   NOP - No Operation (for padding packed GXFIFO commands)

  4000444h 11h -  17  MTX_PUSH - Push Current Matrix on Stack (W)
  4000448h 12h 1  36  MTX_POP - Pop Current Matrix from Stack (W)
  400044Ch 13h 1  17  MTX_STORE - Store Current Matrix on Stack (W)
  4000450h 14h 1  36  MTX_RESTORE - Restore Current Matrix from Stack (W)

  400046Ch 1Bh 3  22  MTX_SCALE - Multiply Current Matrix by Scale Matrix (W)
  4000470h 1Ch 3  22* MTX_TRANS - Mult. Curr. Matrix by Translation Matrix (W)
  4000480h 20h 1  1   COLOR - Directly Set Vertex Color (W)
  4000484h 21h 1      NORMAL - Set Normal Vector (W)

  40004A4h 29h 1  1   POLYGON_ATTR - Set Polygon Attributes (W)
  40004A8h 2Ah 1  1   TEXIMAGE_PARAM - Set Texture Parameters (W)
  40004ACh 2Bh 1  1   PLTT_BASE - Set Texture Palette Base Address (W)
  40004C0h 30h 1  4   DIF_AMB - MaterialColor0 - Diffuse/Ambient Reflect. (W)
  40004C4h 31h 1  4   SPE_EMI - MaterialColor1 - Specular Ref. & Emission (W)
  40004C8h 32h 1  6   LIGHT_VECTOR - Set Light's Directional Vector (W)
  40004CCh 33h 1  1   LIGHT_COLOR - Set Light Color (W)
  40004D0h 34h 32 32  SHININESS - Specular Reflection Shininess Table (W)
  4000540h 50h 1  392 SWAP_BUFFERS - Swap Rendering Engine Buffer (W)
  4000580h 60h 1  1   VIEWPORT - Set Viewport (W)
  40005C0h 70h 3  103 BOX_TEST - Test if Cuboid Sits inside View Volume (W)
  40005C4h 71h 2  9   POS_TEST - Set Position Coordinates for Test (W)
  40005C8h 72h 1  5   VEC_TEST - Set Directional Vector for Test (W)
  
*/

#define print(a) printf a
//#define print(a)

BOOL attempted_3D_op=FALSE;

int mtx_mode=0;

void gl_MTX_MODE (u32 val) {
CHECK_3D_ATTEMPT
    mtx_mode = val;
    switch(val) {
    case MTX_MODE_PROJECTION:
        print(("MTX MODE PROJECTION\n"));
        break;
    case MTX_MODE_POSITION:
        print(("MTX MODE POSITION\n"));
        break;
    case MTX_MODE_POS_VECTOR:
        print(("MTX MODE POSITION & VECTOR\n"));
        break;
    case MTX_MODE_TEXTURE:
        print(("MTX MODE TEXTURE\n"));
        break;
    }
}


/******************************************************************/
//      MTX_LOAD* - cmd 15h-1Ah
/******************************************************************/
#define MMM 0.0
static float mCurrent[16];
static float mUnit[16]= 
    {1.0, 0.0, 0.0, 0.0,
     0.0, 1.0, 0.0, 0.0,
     0.0, 0.0, 1.0, 0.0,
     0.0, 0.0, 0.0, 1.0};
static float m4x4[16]= 
    {MMM, MMM, MMM, MMM,
     MMM, MMM, MMM, MMM,
     MMM, MMM, MMM, MMM,
     MMM, MMM, MMM, MMM};
static float m4x3[16]= 
    {MMM, MMM, MMM, 0.0,
     MMM, MMM, MMM, 0.0,
     MMM, MMM, MMM, 0.0,
     MMM, MMM, MMM, 1.0};
static float m3x3[16]= 
    {MMM, MMM, MMM, 0.0,
     MMM, MMM, MMM, 0.0,
     MMM, MMM, MMM, 0.0,
     0.0, 0.0, 0.0, 1.0};
     
void gl_MTX_show(float*m) {
	print(("\t[[%+.5f %+.5f %+.5f %+.5f]\n", m[0] ,m[1] ,m[2] ,m[3]));
	print(("\t [%+.5f %+.5f %+.5f %+.5f]\n", m[4] ,m[5] ,m[6] ,m[7]));
	print(("\t [%+.5f %+.5f %+.5f %+.5f]\n", m[8] ,m[9] ,m[10],m[11]));
	print(("\t [%+.5f %+.5f %+.5f %+.5f]]\n",m[12],m[13],m[14],m[15]));        
}

void gl_MTX_load(float* m) {
    int i;
    for (i=0;i<16;i++)
        mCurrent[i]=m[i];
    gl_MTX_show(mCurrent);
}
void gl_MTX_mult(float* m) {
    int i;
    for (i=0;i<16;i++)
        mCurrent[i]*=m[i];
    gl_MTX_show(mCurrent);
}

void gl_MTX_IDENTITY () {
CHECK_3D_ATTEMPT
   	print(("MTX_IDENTITY\n"));
    gl_MTX_load(mUnit);
}

void gl_MTX_LOAD_4x4 (u32 val) {
	static int mtx_nbparams = 0;
	_MTX_val mval;
CHECK_3D_ATTEMPT
    mval.val = val;
	switch(mtx_nbparams) {
	case  0: case  1: case  2: case  3:
	case  4: case  5: case  6: case  7:
    case  8: case  9: case 10: case 11:
    case 12: case 13: case 14:
		m4x4[mtx_nbparams]=mval.fval;
		mtx_nbparams++;
		break;
	case 15:
		m4x4[mtx_nbparams]=mval.fval;
		mtx_nbparams=0;
    	print(("MTX_LOAD_4x4\n"));
		gl_MTX_load(m4x4);
		break;
	default:
		break;
	}
}
void gl_MTX_MULT_4x4 (u32 val) {
	static int mtx_nbparams = 0;
	_MTX_val mval;
CHECK_3D_ATTEMPT
    mval.val = val;
	switch(mtx_nbparams) {
	case  0: case  1: case  2: case  3:
	case  4: case  5: case  6: case  7:
    case  8: case  9: case 10: case 11:
    case 12: case 13: case 14:
		m4x4[mtx_nbparams]=mval.fval;
		mtx_nbparams++;
		break;
	case 15:
		m4x4[mtx_nbparams]=mval.fval;
		mtx_nbparams=0;
    	print(("MTX_MULT_4x4\n"));
		gl_MTX_mult(m4x4);
		break;
	default:
		break;
	}
}


void gl_MTX_LOAD_4x3 (u32 val) {
	static int mtx_nbparams = 0;
	_MTX_val mval;
CHECK_3D_ATTEMPT
    mval.val = val;
	switch(mtx_nbparams) {
    case  3: case  7: case 11:
        mtx_nbparams++;
	case  0: case  1: case  2:
	case  4: case  5: case  6:
    case  8: case  9: case 10:
    case 12: case 13:
		m4x3[mtx_nbparams]=mval.fval;
		mtx_nbparams++;
		break;
	case 14:
		m4x3[mtx_nbparams]=mval.fval;
		mtx_nbparams=0;
    	print(("MTX_LOAD_4x3\n"));
		gl_MTX_load(m4x3);
		break;
	default:
		break;
	}
}
void gl_MTX_MULT_4x3 (u32 val) {
	static int mtx_nbparams = 0;
	_MTX_val mval;
CHECK_3D_ATTEMPT
    mval.val = val;
	switch(mtx_nbparams) {
    case  3: case  7: case 11:
        mtx_nbparams++;
	case  0: case  1: case  2:
	case  4: case  5: case  6:
    case  8: case  9: case 10:
    case 12: case 13:
		m4x3[mtx_nbparams]=mval.fval;
		mtx_nbparams++;
		break;
	case 14:
		m4x3[mtx_nbparams]=mval.fval;
		mtx_nbparams=0;
    	print(("MTX_MULT_4x3\n"));
		gl_MTX_mult(m4x3);
		break;
	default:
		break;
	}
}

void gl_MTX_LOAD_3x3 (u32 val) {
	static int mtx_nbparams = 0;
	_MTX_val mval;
CHECK_3D_ATTEMPT
    mval.val = val;
	switch(mtx_nbparams) {
    case  3: case  7: case 11:
        mtx_nbparams++;
	case  0: case  1: case  2:
	case  4: case  5: case  6:
    case  8: case  9:
		m3x3[mtx_nbparams]=mval.fval;
		mtx_nbparams++;
		break;
	case 10:
		m3x3[mtx_nbparams]=mval.fval;
		mtx_nbparams=0;
    	print(("MTX_LOAD_3x3\n"));
		gl_MTX_load(m3x3);
		break;
	default:
		break;
	}
}
void gl_MTX_MULT_3x3 (u32 val) {
	static int mtx_nbparams = 0;
	_MTX_val mval;
CHECK_3D_ATTEMPT
    mval.val = val;
	switch(mtx_nbparams) {
    case  3: case  7: case 11:
        mtx_nbparams++;
	case  0: case  1: case  2:
	case  4: case  5: case  6:
    case  8: case  9:
		m3x3[mtx_nbparams]=mval.fval;
		mtx_nbparams++;
		break;
	case 10:
		m3x3[mtx_nbparams]=mval.fval;
		mtx_nbparams=0;
    	print(("MTX_MULT_3x3\n"));
		gl_MTX_mult(m3x3);
		break;
	default:
		break;
	}
}
/******************************************************************/
//      TEXCOORD - cmd 22h
/******************************************************************/

void gl_TEXCOORD(u32 val) {
    _TEXCOORD tx;
    float s,t;
CHECK_3D_ATTEMPT
	tx.val = val;
	s = tx.bits.low  / 2048.;
	t = tx.bits.high / 2048.;
	print(("\tTEX [%+.5f %+.5f]\n",s,t));
}


/******************************************************************/
//      VTX - cmd 23h-28h, 40h-41h 
/******************************************************************/

static s16 vx=0,vy=0,vz=0;

INLINE void gl_VTX_one() {
	float vfx,vfy,vfz;
CHECK_3D_ATTEMPT
	vfx = vx / 4096.;
	vfy = vy / 4096.;
	vfz = vz / 4096.;
	print(("\tVTX [%+.5f %+.5f %+.5f]\n",vfx,vfy,vfz));
}

void gl_VTX_begin(u32 val) {
//see 4000500h - Cmd 40h - BEGIN_VTXS - Start of Vertex List (W)
CHECK_3D_ATTEMPT
	vx=vy=vz=0;
	print(("VTX_begin : "));
	switch(val) {
	case BEGIN_GL_TRIANGLES : // separate triangles (3 vertices for each triangle)
		print(("GL_TRIANGLES\n"));
		break;
	case BEGIN_GL_QUADS : // separate quads (4 vertices for each triangle)
		print(("GL_QUADS\n"));
		break;
	// strips : 1st triangle or quad defined by all vertices
	//          next ones share a segment (so 2 vertices less)
	case BEGIN_GL_TRIANGLE_STRIP : // triangle strips (1st : 3, next : 1)
		print(("GL_TRIANGLE_STRIP\n"));
		break;
	case BEGIN_GL_QUAD_STRIP : // quad strips (1st : 4, next : 2)
		print(("GL_QUAD_STRIP\n"));
		break;
	default :
        print(("unknown %d\n",val));
	}
}
void gl_VTX_end() {
//see 4000504h - Cmd 41h - END_VTXS - End of Vertex List (W)
CHECK_3D_ATTEMPT
	print(("VTX_end.\n"));
}

void gl_VTX_16 (u32 val) {
//see 400048Ch - Cmd 23h - VTX_16 - Set Vertex XYZ Coordinates (W)
	_VTX_16 vval;
	static int vtx_16_nbparams = 0;

CHECK_3D_ATTEMPT
	vval.val = val;
	switch(vtx_16_nbparams) {
	case 0:
		vx = vval.bits.low ;
		vy = vval.bits.high;
		vtx_16_nbparams++;
		break;
	case 1:
		vz = vval.bits.low ;
		gl_VTX_one();
		vtx_16_nbparams=0;
		break;
	default:
		break;
	}
}

void gl_VTX_10 (u32 xyz) {
//see 4000490h - Cmd 24h - VTX_10 - Set Vertex XYZ Coordinates (W)
	_VTX_10 vt;
CHECK_3D_ATTEMPT
	vt.val = xyz;
	vx = vt.bits.low  << 6;
	vy = vt.bits.mid  << 6;
	vz = vt.bits.high << 6;
	gl_VTX_one();
}


void gl_VTX_XY (u32 xy) {
//see 4000494h - Cmd 25h - VTX_XY - Set Vertex XY Coordinates (W)
	_VTX_16 vt;
CHECK_3D_ATTEMPT
	vt.val = xy;
	vx = vt.bits.low ;
	vy = vt.bits.high;
	gl_VTX_one();
}
void gl_VTX_XZ (u32 xz) {
//see 4000498h - Cmd 26h - VTX_XZ - Set Vertex XZ Coordinates (W)
	_VTX_16 vt;
CHECK_3D_ATTEMPT
	vt.val = xz;
	vx = vt.bits.low ;
	vz = vt.bits.high;
	gl_VTX_one();
}
void gl_VTX_YZ (u32 yz) {
//see 400049Ch - Cmd 27h - VTX_YZ - Set Vertex YZ Coordinates (W)
	_VTX_16 vt;
CHECK_3D_ATTEMPT
	vt.val = yz;
	vy = vt.bits.low ;
	vz = vt.bits.high;
	gl_VTX_one();
}


void gl_VTX_DIFF (u32 diff) {
//see 40004A0h - Cmd 28h - VTX_DIFF - Set Relative Vertex Coordinates (W)
	_VTX_10 vt;
CHECK_3D_ATTEMPT
	vt.val = diff;
	vx += vt.bits.low  << 3;
	vy += vt.bits.mid  << 3;
	vz += vt.bits.high << 3;
	gl_VTX_one();
}
