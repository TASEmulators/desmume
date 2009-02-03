/*  Copyright (C) 2006 yopyop
    yopyop156@ifrance.com
    yopyop156.ifrance.com

	Copyright 2009 DeSmuME team

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
    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301  USA
*/

#include "Rasterize.h"

#include <algorithm>

#include "common.h"
#include "render3D.h"
#include "gfx3d.h"
#include "texcache.h"

using std::min;
using std::max;

template<typename T> T min(T a, T b, T c) { return min(min(a,b),c); }
template<typename T> T max(T a, T b, T c) { return max(max(a,b),c); }

static u16 screen[256*192];

static struct
{
	int width, height;
} Texture;

void set_pixel(int x, int y, u16 color)
{
	if(x<0 || y<0 || x>=256 || y>=192) return;
	screen[y*256+x] = color | 0x8000;
}

void hline(int x, int y, int xe, u16 color)
{
	for(int i=x;i<=xe;i++)
		set_pixel(x,y,color);
}

//http://www.devmaster.net/forums/showthread.php?t=1884

#if defined(_MSC_VER)
inline int iround(float x)
{
  int t;

  __asm
  {
    fld  x
    fistp t
  }

  return t;
}
#else
int iround(float f) {
	return (int)f; //lol
}
#endif

struct Interpolator
{
	int A,B,C;
	float dx, dy;
	float Z, pZ;

	struct {
		int x,y,z;
	} point0;
	
	Interpolator(int x1, int x2, int x3, int y1, int y2, int y3, int z1, int z2, int z3)
	{
		A = (z3 - z1) * (y2 - y1) - (z2 - z1) * (y3 - y1);
		B = (x3 - x1) * (z2 - z1) - (x2 - x1) * (z3 - z1);
		C = (x2 - x1) * (y3 - y1) - (x3 - x1) * (y2 - y1);
		dx = -(float)A / C;
		dy = -(float)B / C;
		point0.x = x1;
		point0.y = y1;
		point0.z = z1;
	}

	void init(int x, int y)
	{
		Z = point0.z + dx * (x-point0.x) + dy * (y-point0.y);
	}

	FORCEINLINE int cur() { return iround(Z); }
	
	FORCEINLINE void push() { pZ = Z; }
	FORCEINLINE void pop() { Z = pZ; }
	FORCEINLINE void incy() { Z += dy; }
	FORCEINLINE void incx() { Z += dx; }
};

//http://www.devmaster.net/forums/showthread.php?t=1884&page=1
//todo - change to the tile-based renderer and try to apply some optimizations from that thread
void triangle_from_devmaster(VERT** verts)
{
	u16 color =0x7FFF;

	// 28.4 fixed-point coordinates
    const int Y1 = iround(16.0f * verts[0]->coord[1]);
    const int Y2 = iround(16.0f * verts[1]->coord[1]);
    const int Y3 = iround(16.0f * verts[2]->coord[1]);

    const int X1 = iround(16.0f * verts[0]->coord[0]);
    const int X2 = iround(16.0f * verts[1]->coord[0]);
    const int X3 = iround(16.0f * verts[2]->coord[0]);

    // Deltas
    const int DX12 = X1 - X2;
    const int DX23 = X2 - X3;
    const int DX31 = X3 - X1;

    const int DY12 = Y1 - Y2;
    const int DY23 = Y2 - Y3;
    const int DY31 = Y3 - Y1;

    // Fixed-point deltas
    const int FDX12 = DX12 << 4;
    const int FDX23 = DX23 << 4;
    const int FDX31 = DX31 << 4;

    const int FDY12 = DY12 << 4;
    const int FDY23 = DY23 << 4;
    const int FDY31 = DY31 << 4;

    // Bounding rectangle
    int minx = (min(X1, X2, X3) + 0xF) >> 4;
    int maxx = (max(X1, X2, X3) + 0xF) >> 4;
    int miny = (min(Y1, Y2, Y3) + 0xF) >> 4;
    int maxy = (max(Y1, Y2, Y3) + 0xF) >> 4;

	int desty = miny;

    // Half-edge constants
    int C1 = DY12 * X1 - DX12 * Y1;
    int C2 = DY23 * X2 - DX23 * Y2;
    int C3 = DY31 * X3 - DX31 * Y3;

    // Correct for fill convention
    if(DY12 < 0 || (DY12 == 0 && DX12 > 0)) C1++;
    if(DY23 < 0 || (DY23 == 0 && DX23 > 0)) C2++;
    if(DY31 < 0 || (DY31 == 0 && DX31 > 0)) C3++;

    int CY1 = C1 + DX12 * (miny << 4) - DY12 * (minx << 4);
    int CY2 = C2 + DX23 * (miny << 4) - DY23 * (minx << 4);
    int CY3 = C3 + DX31 * (miny << 4) - DY31 * (minx << 4);

	float fx1 = verts[0]->coord[0], fy1 = verts[0]->coord[1];
	float fx2 = verts[1]->coord[0], fy2 = verts[1]->coord[1];
	float fx3 = verts[2]->coord[0], fy3 = verts[2]->coord[1];
	u8 r1 = verts[0]->color[0], g1 = verts[0]->color[1], b1 = verts[0]->color[2];
	u8 r2 = verts[1]->color[0], g2 = verts[1]->color[1], b2 = verts[1]->color[2];
	u8 r3 = verts[2]->color[0], g3 = verts[2]->color[1], b3 = verts[2]->color[2];
	int u1 = verts[0]->texcoord[0], v1 = verts[0]->texcoord[1];
	int u2 = verts[1]->texcoord[0], v2 = verts[1]->texcoord[1];
	int u3 = verts[2]->texcoord[0], v3 = verts[2]->texcoord[1];

	Interpolator i_color_r(fx1,fx2,fx3,fy1,fy2,fy3,r1,r2,r3);
	Interpolator i_color_g(fx1,fx2,fx3,fy1,fy2,fy3,g1,g2,g3);
	Interpolator i_color_b(fx1,fx2,fx3,fy1,fy2,fy3,b1,b2,b3);
	Interpolator i_tex_u(fx1,fx2,fx3,fy1,fy2,fy3,u1,u2,u3);
	Interpolator i_tex_v(fx1,fx2,fx3,fy1,fy2,fy3,v1,v2,v3);
	

	i_color_r.init(minx,miny);
	i_color_g.init(minx,miny);
	i_color_b.init(minx,miny);
	i_tex_u.init(minx,miny);
	i_tex_v.init(minx,miny);

    for(int y = miny; y < maxy; y++)
    {
        int CX1 = CY1;
        int CX2 = CY2;
        int CX3 = CY3;

		bool done = false;
		i_color_r.push(); i_color_g.push(); i_color_b.push(); 
		i_tex_u.push(); i_tex_v.push();
        for(int x = minx; x < maxx; x++)
        {
            if(CX1 > 0 && CX2 > 0 && CX3 > 0)
            {
				//material color
				//color = R5G5B5TORGB15(i_color_r.cur(),i_color_g.cur(),i_color_b.cur());
				
				//texture
				int u = i_tex_u.cur();
				int v = i_tex_v.cur();
				if(u<0) u = 0;
				if(v<0) v = 0;
				u32 color32 = ((u32*)TexCache_texMAP)[v*Texture.width+u];
				color32>>=3;
				color32 &= 0x1F1F1F1F;
				u8* color8 = (u8*)&color32;
				color = (color8[0] | (color8[1] << 5) | (color8[2] << 10));
	
				//hack: for testing, dont render non-opaque textures
				if(color8[3] < 0x1F) return;
				
				set_pixel(x,desty,color);

				done = true;
            } else if(done) break;

			i_color_r.incx(); i_color_g.incx(); i_color_b.incx();
			i_tex_u.incx(); i_tex_v.incx();

            CX1 -= FDY12;
            CX2 -= FDY23;
            CX3 -= FDY31;
        }
		i_color_r.pop(); i_color_r.incy();
		i_color_g.pop(); i_color_g.incy();
		i_color_b.pop(); i_color_b.incy();
		i_tex_u.pop(); i_tex_u.incy();
		i_tex_v.pop(); i_tex_v.incy();


        CY1 += FDX12;
        CY2 += FDX23;
        CY3 += FDX31;

		desty++;
    }
}

static char Init(void)
{
	return 1;
}

static void Reset() {}

static void Close() {}

static void VramReconfigureSignal() {}

static void GetLine(int line, u16* dst, u8* dstAlpha)
{
	memcpy(dst,screen+(191-line)*256,512);
	memset(dstAlpha,16,256);
}

static void GetLineCaptured(int line, u16* dst) {}

static void Render()
{
	//transform verts and polys
	//which order?
	//A. clip
	//B. backface cull
	//C. transforms

	memset(screen,0,256*192*2);

	for(int i=0;i<gfx3d.vertlist->count;i++)
	{
		VERT &vert = gfx3d.vertlist->list[i];

		//perspective division and viewport transform
		vert.coord[0] = (vert.coord[0]+vert.coord[3])*256 / (2*vert.coord[3]) + 0;
		vert.coord[1] = (vert.coord[1]+vert.coord[3])*192 / (2*vert.coord[3]) + 0;
	}


	
	//iterate over gfx3d.polylist and gfx3d.vertlist
	for(int i=0;i<gfx3d.polylist->count;i++) {
		POLY *poly = &gfx3d.polylist->list[gfx3d.indexlist[i]];
		int type = poly->type;

		TexCache_SetTexture(poly->texParam,poly->texPalette);
		if(TexCache_Curr())
			Texture.width = TexCache_Curr()->sizeX;

		//note that when we build our triangle vert lists, we reorder them for our renderer.
		//we should probably fix the renderer so we dont have to do this;
		//but then again, what does it matter?
		if(type == 4) {

			VERT* vertA[3] = {
				&gfx3d.vertlist->list[poly->vertIndexes[0]],
				&gfx3d.vertlist->list[poly->vertIndexes[2]],
				&gfx3d.vertlist->list[poly->vertIndexes[1]],
			};

			triangle_from_devmaster(vertA);

			VERT* vertB[3] = {
				&gfx3d.vertlist->list[poly->vertIndexes[0]],
				&gfx3d.vertlist->list[poly->vertIndexes[3]],
				&gfx3d.vertlist->list[poly->vertIndexes[2]],
			};

			triangle_from_devmaster(vertB);

		}
		if(type == 3) {
			VERT* vert[3] = {
				&gfx3d.vertlist->list[poly->vertIndexes[2]],
				&gfx3d.vertlist->list[poly->vertIndexes[1]],
				&gfx3d.vertlist->list[poly->vertIndexes[0]],
			};

			triangle_from_devmaster(vert);
		}

	}
				
}


GPU3DInterface gpu3DRasterize = {
	"SoftRasterizer",
	Init,
	Reset,
	Close,
	Render,
	VramReconfigureSignal,
	GetLine,
	GetLineCaptured
};
