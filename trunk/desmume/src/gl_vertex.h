/* gl_vertex.h - this file is part of DeSmuME
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

#include <stdio.h>
#include "types.h"

#define MTX_MODE_PROJECTION 0
#define MTX_MODE_POSITION   1
#define MTX_MODE_POS_VECTOR 2
#define MTX_MODE_TEXTURE    3
#define BEGIN_GL_TRIANGLES       0
#define BEGIN_GL_QUADS           1
#define BEGIN_GL_TRIANGLE_STRIP  2
#define BEGIN_GL_QUAD_STRIP      3
		
typedef union {
	u32 val;
	float fval;
} _MTX_val ;
typedef union {
	u32 val;
	struct { 
        // 12 bit fractionnal for _VTX_16 
        // 11 bit fractionnal for _TEXCOORD 
	signed low:16;
	signed high:16;
	} bits;
} _VTX_16, _TEXCOORD ;
typedef union {
	u32 val;
	struct { // 6 bit fractionnal
	signed low:10;
	signed mid:10;
	signed high:10;
	signed :2;
	} bits;
} _VTX_10 ;


void gl_MTX_IDENTITY ();
void gl_MTX_LOAD_4x4 (u32 val);
void gl_MTX_LOAD_4x3 (u32 val);
void gl_MTX_LOAD_3x3 (u32 val);

void gl_TEXCOORD(u32 val);

void gl_VTX_begin(u32 val);
//see 4000500h - Cmd 40h - BEGIN_VTXS - Start of Vertex List (W)
void gl_VTX_end();
//see 4000504h - Cmd 41h - END_VTXS - End of Vertex List (W)

void gl_VTX_16 (u32 val);
//see 400048Ch - Cmd 23h - VTX_16 - Set Vertex XYZ Coordinates (W)
void gl_VTX_10 (u32 xyz);
//see 4000490h - Cmd 24h - VTX_10 - Set Vertex XYZ Coordinates (W)

void gl_VTX_XY (u32 xy);
//see 4000494h - Cmd 25h - VTX_XY - Set Vertex XY Coordinates (W)
void gl_VTX_XZ (u32 xz);
//see 4000498h - Cmd 26h - VTX_XZ - Set Vertex XZ Coordinates (W)
void gl_VTX_YZ (u32 yz);
//see 400049Ch - Cmd 27h - VTX_YZ - Set Vertex YZ Coordinates (W)

void gl_VTX_DIFF (u32 diff);
//see 40004A0h - Cmd 28h - VTX_DIFF - Set Relative Vertex Coordinates (W)
