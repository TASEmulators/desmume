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
#include "common.h"
#include "render3D.h"
#include "gfx3d.h"
#include <algorithm>

using std::min;
using std::max;

template<typename T> T min(T a, T b, T c) { return min(min(a,b),c); }
template<typename T> T max(T a, T b, T c) { return max(max(a,b),c); }


static u16 screen[256*192];

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

int iround(float f) {
	return (int)f; //lol
}

void triangle_from_devmaster(int x1, int y1, int x2, int y2, int x3, int y3, u16 color)
{
	int desty = 0;
	struct Vertex {
		int x,y;
	} v1, v2, v3;
	v1.x = x1;
	v1.y = y1;
	v2.x = x2;
	v2.y = y2;
	v3.x = x3;
	v3.y = y3;

    // 28.4 fixed-point coordinates
    const int Y1 = iround(16.0f * v1.y);
    const int Y2 = iround(16.0f * v2.y);
    const int Y3 = iround(16.0f * v3.y);

    const int X1 = iround(16.0f * v1.x);
    const int X2 = iround(16.0f * v2.x);
    const int X3 = iround(16.0f * v3.x);

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

    // Block size, standard 8x8 (must be power of two)
    const int q = 8;

    // Start in corner of 8x8 block
    minx &= ~(q - 1);
    miny &= ~(q - 1);

    //(char*&)colorBuffer += miny * stride;
	desty = miny;

    // Half-edge constants
    int C1 = DY12 * X1 - DX12 * Y1;
    int C2 = DY23 * X2 - DX23 * Y2;
    int C3 = DY31 * X3 - DX31 * Y3;

    // Correct for fill convention
    if(DY12 < 0 || (DY12 == 0 && DX12 > 0)) C1++;
    if(DY23 < 0 || (DY23 == 0 && DX23 > 0)) C2++;
    if(DY31 < 0 || (DY31 == 0 && DX31 > 0)) C3++;

    // Loop through blocks
    for(int y = miny; y < maxy; y += q)
    {
        for(int x = minx; x < maxx; x += q)
        {
            // Corners of block
            int x0 = x << 4;
            int x1 = (x + q - 1) << 4;
            int y0 = y << 4;
            int y1 = (y + q - 1) << 4;

            // Evaluate half-space functions
            bool a00 = C1 + DX12 * y0 - DY12 * x0 > 0;
            bool a10 = C1 + DX12 * y0 - DY12 * x1 > 0;
            bool a01 = C1 + DX12 * y1 - DY12 * x0 > 0;
            bool a11 = C1 + DX12 * y1 - DY12 * x1 > 0;
            int a = (a00 << 0) | (a10 << 1) | (a01 << 2) | (a11 << 3);
    
            bool b00 = C2 + DX23 * y0 - DY23 * x0 > 0;
            bool b10 = C2 + DX23 * y0 - DY23 * x1 > 0;
            bool b01 = C2 + DX23 * y1 - DY23 * x0 > 0;
            bool b11 = C2 + DX23 * y1 - DY23 * x1 > 0;
            int b = (b00 << 0) | (b10 << 1) | (b01 << 2) | (b11 << 3);
    
            bool c00 = C3 + DX31 * y0 - DY31 * x0 > 0;
            bool c10 = C3 + DX31 * y0 - DY31 * x1 > 0;
            bool c01 = C3 + DX31 * y1 - DY31 * x0 > 0;
            bool c11 = C3 + DX31 * y1 - DY31 * x1 > 0;
            int c = (c00 << 0) | (c10 << 1) | (c01 << 2) | (c11 << 3);

            // Skip block when outside an edge
            if(a == 0x0 || b == 0x0 || c == 0x0) continue;

            //unsigned int *buffer = colorBuffer;
			int _desty = desty;

            // Accept whole block when totally covered
            if(a == 0xF && b == 0xF && c == 0xF)
            {
                for(int iy = 0; iy < q; iy++)
                {
                    for(int ix = x; ix < x + q; ix++)
                    {
                        //buffer[ix] = 0x00007F00;<< // Green
						set_pixel(ix,_desty,color);
                    }

                    //(char*&)buffer += stride;
					_desty++;

                }
            }
            else // Partially covered block
            {
                int CY1 = C1 + DX12 * y0 - DY12 * x0;
                int CY2 = C2 + DX23 * y0 - DY23 * x0;
                int CY3 = C3 + DX31 * y0 - DY31 * x0;

                for(int iy = y; iy < y + q; iy++)
                {
                    int CX1 = CY1;
                    int CX2 = CY2;
                    int CX3 = CY3;

                    for(int ix = x; ix < x + q; ix++)
                    {
                        if(CX1 > 0 && CX2 > 0 && CX3 > 0)
                        {
                            //buffer[ix] = 0x0000007F;<< // Blue
							set_pixel(ix,_desty,color);
                        }

                        CX1 -= FDY12;
                        CX2 -= FDY23;
                        CX3 -= FDY31;
                    }

                    CY1 += FDX12;
                    CY2 += FDX23;
                    CY3 += FDX31;

                    //(char*&)buffer += stride;
					_desty ++;
                }
            }
        }

        //(char*&)colorBuffer += q * stride;
		desty += q;
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

		//perspective division
		vert.coord[0] = (vert.coord[0] + vert.coord[3]) / 2 / vert.coord[3];
		vert.coord[1] = (vert.coord[1] + vert.coord[3]) / 2 / vert.coord[3];
		vert.coord[2] = (vert.coord[2] + vert.coord[3]) / 2 / vert.coord[3];
		vert.coord[3] = 1;

		//transform to viewport. this is badly broken
		vert.coord[0] = (vert.coord[0])*128;
		vert.coord[1] = (vert.coord[1])*96;

		int zzz=9;
	}


	
	//iterate over gfx3d.polylist and gfx3d.vertlist
	for(int i=0;i<gfx3d.polylist->count;i++) {
		POLY *poly = &gfx3d.polylist->list[gfx3d.indexlist[i]];
		int type = poly->type;


		if(type == 3) {
			VERT* vert[3] = {
				&gfx3d.vertlist->list[poly->vertIndexes[0]],
				&gfx3d.vertlist->list[poly->vertIndexes[1]],
				&gfx3d.vertlist->list[poly->vertIndexes[2]],
			};
			u16 color = vert[0]->color[0] | (vert[0]->color[1]<<5) | (vert[0]->color[2]<<10);


			triangle_from_devmaster(
				vert[0]->coord[0],vert[0]->coord[1],
				vert[1]->coord[0],vert[1]->coord[1],
				vert[2]->coord[0],vert[2]->coord[1],
				color);
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
