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
#include "registers.h"

#define CHECK_3D_ATTEMPT attempted_3D_op=TRUE;
BOOL attempted_3D_op;

#define MTX_MODE_PROJECTION 0
#define MTX_MODE_POSITION   1
#define MTX_MODE_POS_VECTOR 2
#define MTX_MODE_TEXTURE    3
#define BEGIN_GL_TRIANGLES       0
#define BEGIN_GL_QUADS           1
#define BEGIN_GL_TRIANGLE_STRIP  2
#define BEGIN_GL_QUAD_STRIP      3

#define TEXCOORD_to_float(t) (((float)(t)) / 2048.)
#define u32_to_float(t)      (((float)(t)) / 4096.)
#define s16_to_float(t)      (((float)(t)) / 4096.)
#define VTX16_to_s16(t)      (t)
#define VTX10_to_s16(t)      (t << 6)
#define VTXDIFF_to_s16(t)    (t << 3)

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

void gl_MTX_MODE (u32 val);
void gl_MTX_IDENTITY ();
void gl_MTX_LOAD_4x4 (u32 val);
void gl_MTX_LOAD_4x3 (u32 val);
void gl_MTX_LOAD_3x3 (u32 val);
void gl_MTX_MULT_4x4 (u32 val);
void gl_MTX_MULT_4x3 (u32 val);
void gl_MTX_MULT_3x3 (u32 val);

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


#define GL_CMD_NAME(n)	\
	case n : printf("cmd " #n "\n"); break;

INLINE static void gl_print_cmd(u32 adr) {
CHECK_3D_ATTEMPT
#if 0
	switch (adr) {
		GL_CMD_NAME(eng_3D_RDLINES_COUNT   )
		GL_CMD_NAME(eng_3D_EDGE_COLOR      )
		GL_CMD_NAME(eng_3D_ALPHA_TEST_REF  )
		GL_CMD_NAME(eng_3D_CLEAR_COLOR     )
		GL_CMD_NAME(eng_3D_CLEAR_DEPTH     )
		GL_CMD_NAME(eng_3D_CLRIMAGE_OFFSET )
		GL_CMD_NAME(eng_3D_FOG_COLOR       )
		GL_CMD_NAME(eng_3D_FOG_OFFSET      )
		GL_CMD_NAME(eng_3D_FOG_TABLE       )
		GL_CMD_NAME(eng_3D_TOON_TABLE      )
		GL_CMD_NAME(eng_3D_GXFIFO          )
		
		GL_CMD_NAME(cmd_3D_MTX_MODE        )
		GL_CMD_NAME(cmd_3D_MTX_PUSH        )
		GL_CMD_NAME(cmd_3D_MTX_POP         )
		GL_CMD_NAME(cmd_3D_MTX_STORE       )
		GL_CMD_NAME(cmd_3D_MTX_RESTORE     )
		GL_CMD_NAME(cmd_3D_MTX_IDENTITY    )
		GL_CMD_NAME(cmd_3D_MTX_LOAD_4x4    )
		GL_CMD_NAME(cmd_3D_MTX_LOAD_4x3    )
		GL_CMD_NAME(cmd_3D_MTX_MULT_4x4    )
		GL_CMD_NAME(cmd_3D_MTX_MULT_4x3    )
		GL_CMD_NAME(cmd_3D_MTX_MULT_3x3    )
		GL_CMD_NAME(cmd_3D_MTX_SCALE       )
		GL_CMD_NAME(cmd_3D_MTX_TRANS       )
		GL_CMD_NAME(cmd_3D_COLOR           )
		GL_CMD_NAME(cmd_3D_NORMA           )
		GL_CMD_NAME(cmd_3D_TEXCOORD        )
		GL_CMD_NAME(cmd_3D_VTX_16          )
		GL_CMD_NAME(cmd_3D_VTX_10          )
		GL_CMD_NAME(cmd_3D_VTX_XY          )
		GL_CMD_NAME(cmd_3D_VTX_XZ          )
		GL_CMD_NAME(cmd_3D_VTX_YZ          )
		GL_CMD_NAME(cmd_3D_VTX_DIFF        )
		GL_CMD_NAME(cmd_3D_POLYGON_ATTR    )
		GL_CMD_NAME(cmd_3D_TEXIMAGE_PARAM  )
		GL_CMD_NAME(cmd_3D_PLTT_BASE       )
		GL_CMD_NAME(cmd_3D_DIF_AMB         )
		GL_CMD_NAME(cmd_3D_SPE_EMI         )
		GL_CMD_NAME(cmd_3D_LIGHT_VECTOR    )
		GL_CMD_NAME(cmd_3D_LIGHT_COLOR     )
		GL_CMD_NAME(cmd_3D_SHININESS       )
		GL_CMD_NAME(cmd_3D_BEGIN_VTXS      )
		GL_CMD_NAME(cmd_3D_END_VTXS        )
		GL_CMD_NAME(cmd_3D_SWAP_BUFFERS    )
		GL_CMD_NAME(cmd_3D_VIEWPORT        )
		GL_CMD_NAME(cmd_3D_BOX_TEST        )
		GL_CMD_NAME(cmd_3D_POS_TEST        )
		GL_CMD_NAME(cmd_3D_VEC_TEST        )
		
		GL_CMD_NAME(eng_3D_GXSTAT          )
		GL_CMD_NAME(eng_3D_RAM_COUNT       )
		GL_CMD_NAME(eng_3D_DISP_1DOT_DEPTH )
		GL_CMD_NAME(eng_3D_POS_RESULT      )
		GL_CMD_NAME(eng_3D_VEC_RESULT      )
		GL_CMD_NAME(eng_3D_CLIPMTX_RESULT  )
		GL_CMD_NAME(eng_3D_VECMTX_RESULT   )
		default: break;
	}
#endif
}
