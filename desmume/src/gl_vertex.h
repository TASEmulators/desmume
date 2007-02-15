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

typedef union {
	u32 val;
	struct {
	unsigned primitive:2;
	unsigned :30;
	} bits;
} _VTX_BEGIN_cmd ;
typedef union {
	u32 val;
	struct { // 12 bit fractionnal
	signed low:16;
	signed high:16;
	} bits;
} _VTX_16 ;
typedef union {
	u32 val;
	struct { // 6 bit fractionnal
	signed low:10;
	signed mid:10;
	signed high:10;
	signed :2;
	} bits;
} _VTX_10 ;

void gl_VTX_begin(u32 val);
//see 4000500h - Cmd 40h - BEGIN_VTXS - Start of Vertex List (W)
void gl_VTX_end();
//see 4000504h - Cmd 41h - END_VTXS - End of Vertex List (W)

void gl_VTX_16 (u32 xxyy, u32 zz__);
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
