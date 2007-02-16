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

static u16 vx=0,vy=0,vz=0;

//#define print(a) printf a
#define print(a)

INLINE void gl_VTX_one() {
	float vfx,vfy,vfz;
	vfx = vx / 4096.;
	vfy = vy / 4096.;
	vfz = vz / 4096.;
	print(("\tVTX (x=%.12f,y=%.12f,z=%.12f)\n",vfx,vfy,vfz));
}

void gl_VTX_begin(u32 val) {
//see 4000500h - Cmd 40h - BEGIN_VTXS - Start of Vertex List (W)
	vx=vy=vz=0;
	print(("VTX_begin : "));
	switch(val) {
	case 0 : // separate triangles (3 vertices for each triangle)
		print(("GL_TRIANGLES\n"));
		break;
	case 1 : // separate quads (4 vertices for each triangle)
		print(("GL_QUADS\n"));
		break;
	// strips : 1st triangle or quad defined by all vertices
	//          next ones share a segment (so 2 vertices less)
	case 2 : // triangle strips (1st : 3, next : 1)
		print(("GL_TRIANGLE_STRIP\n"));
		break;
	case 3 : // quad strips (1st : 4, next : 2)
		print(("GL_QUAD_STRIP\n"));
		break;
	}
}
void gl_VTX_end() {
//see 4000504h - Cmd 41h - END_VTXS - End of Vertex List (W)
	print(("VTX_end.\n"));
}


void gl_VTX_16 (u32 xxyy, u32 zz__) {
//see 400048Ch - Cmd 23h - VTX_16 - Set Vertex XYZ Coordinates (W)
	_VTX_16 xy, z_;
	xy.val = xxyy;
	z_.val = zz__;
	vx = xy.bits.low ;
	vy = xy.bits.high;
	vz = z_.bits.low ;
	gl_VTX_one();
}
void gl_VTX_10 (u32 xyz) {
//see 4000490h - Cmd 24h - VTX_10 - Set Vertex XYZ Coordinates (W)
	_VTX_10 vt;
	vt.val = xyz;
	vx = vt.bits.low  << 6;
	vy = vt.bits.mid  << 6;
	vz = vt.bits.high << 6;
	gl_VTX_one();
}


void gl_VTX_XY (u32 xy) {
//see 4000494h - Cmd 25h - VTX_XY - Set Vertex XY Coordinates (W)
	_VTX_16 vt;
	vt.val = xy;
	vx = vt.bits.low ;
	vy = vt.bits.high;
	gl_VTX_one();
}
void gl_VTX_XZ (u32 xz) {
//see 4000498h - Cmd 26h - VTX_XZ - Set Vertex XZ Coordinates (W)
	_VTX_16 vt;
	vt.val = xz;
	vx = vt.bits.low ;
	vz = vt.bits.high;
	gl_VTX_one();
}
void gl_VTX_YZ (u32 yz) {
//see 400049Ch - Cmd 27h - VTX_YZ - Set Vertex YZ Coordinates (W)
	_VTX_16 vt;
	vt.val = yz;
	vy = vt.bits.low ;
	vz = vt.bits.high;
	gl_VTX_one();
}


void gl_VTX_DIFF (u32 diff) {
//see 40004A0h - Cmd 28h - VTX_DIFF - Set Relative Vertex Coordinates (W)
	_VTX_10 vt;
	vt.val = diff;
	vx += vt.bits.low  << 3;
	vy += vt.bits.mid  << 3;
	vz += vt.bits.high << 3;
	gl_VTX_one();
}
