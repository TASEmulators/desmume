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

//nothing in this file should be assumed to be accurate
//please check everything carefully, and sign off on it when you think it is accurate
//if you change it, erase other signatures.
//if you optimize it and think it is risky, erase other signatures

#include "rasterize.h"

#include <algorithm>
#include <assert.h>
#include <string.h>

#include "bits.h"
#include "common.h"
#include "matrix.h"
#include "render3D.h"
#include "gfx3d.h"
#include "texcache.h"

using std::min;
using std::max;

template<typename T> T min(T a, T b, T c) { return min(min(a,b),c); }
template<typename T> T max(T a, T b, T c) { return max(max(a,b),c); }

static u8 modulate_table[32][32];

struct Fragment
{
	union Color {
		u32 color;
		struct {
			//#ifdef WORDS_BIGENDIAN ?
			u8 r,g,b,a;
		} components;
	} color;

	u8 polyid;
	u32 depth;
};

struct Vertex
{
	VERT* vert;
	int w;
} verts[3];

void SubmitVertex(VERT* rawvert)
{
	static int vert_index = 0;
	Vertex &vert = verts[vert_index++];
	if(vert_index==3) vert_index = 0;

	vert.vert = rawvert;
	vert.w = rawvert->coord[3] * 4096; //not sure about this
}

static Fragment screen[256*192];

FORCEINLINE int iround(float f) {
	return (int)f; //lol
}

static struct
{
	int width, height;
	int wmask, hmask;
	int wrap;
	int wshift;
	void setup(u32 format)
	{
		wshift = ((format>>20)&0x07) + 3;
		width=(1 << wshift);
		height=(8 << ((format>>23)&0x07));
		wmask = width-1;
		hmask = height-1;
		wrap = (format>>16)&0xF;
	}

	FORCEINLINE void clamp(int &val, int size, int sizemask){
		if(val<0) val = 0;
		if(val>sizemask) val = sizemask;
	}
	FORCEINLINE void hclamp(int &val) { clamp(val,width,wmask); }
	FORCEINLINE void vclamp(int &val) { clamp(val,height,hmask); }

	FORCEINLINE void repeat(int &val, int size, int sizemask) {
		val &= sizemask;
	}
	FORCEINLINE void hrepeat(int &val) { repeat(val,width,wmask); }
	FORCEINLINE void vrepeat(int &val) { repeat(val,height,hmask); }

	FORCEINLINE void flip(int &val, int size, int sizemask) {
		val &= ((size<<1)-1);
		if(val>=size) val = (size<<1)-val-1;
	}
	FORCEINLINE void hflip(int &val) { flip(val,width,wmask); }
	FORCEINLINE void vflip(int &val) { flip(val,height,hmask); }

	FORCEINLINE void dowrap(int& iu, int& iv)
	{
		switch(wrap) {
			//flip none
			case 0x0: hclamp(iu); vclamp(iv); break;
			case 0x1: hrepeat(iu); vclamp(iv); break;
			case 0x2: hclamp(iu); vrepeat(iv); break;
			case 0x3: hrepeat(iu); vrepeat(iv); break;
			//flip S
			case 0x4: hclamp(iu); vclamp(iv); break;
			case 0x5: hflip(iu); vclamp(iv); break;
			case 0x6: hclamp(iu); vrepeat(iv); break;
			case 0x7: hflip(iu); vrepeat(iv); break;
			//flip T
			case 0x8: hclamp(iu); vclamp(iv); break;
			case 0x9: hrepeat(iu); vclamp(iv); break;
			case 0xA: hclamp(iu); vflip(iv); break;
			case 0xB: hrepeat(iu); vflip(iv); break;
			//flip both
			case 0xC: hclamp(iu); vclamp(iv); break;
			case 0xD: hflip(iu); vclamp(iv); break;
			case 0xE: hclamp(iu); vflip(iv); break;
			case 0xF: hflip(iu); vflip(iv); break;
		}
	}

	Fragment::Color sample(float u, float v)
	{
		int iu = iround(u);
		int iv = iround(v);
		dowrap(iu,iv);

		Fragment::Color color;
		u32 col32 = ((u32*)TexCache_texMAP)[(iv<<wshift)+iu];
		//todo - teach texcache how to provide these already in 5555
		col32 >>= 3;
		col32 &= 0x1F1F1F1F;
		color.color = col32;
		return color;
	}

} sampler;

struct Shader
{
	u8 mode;
	void setup(u32 polyattr)
	{
		mode = (polyattr>>4)&0x3;
	}

	float invu, invv, invw;
	Fragment::Color materialColor;
	
	void shade(Fragment& dst)
	{
		Fragment::Color texColor;
		float u,v;

		switch(mode)
		{
		case 0: //modulate
			u = invu/invw;
			v = invv/invw;
			texColor = sampler.sample(u,v);
 			assert(texColor.components.r<32); assert(texColor.components.g<32); assert(texColor.components.b<32);assert(texColor.components.a<32);
			assert(materialColor.components.r<32); assert(materialColor.components.g<32); assert(materialColor.components.b<32);assert(materialColor.components.a<32);
			dst.color.components.r = modulate_table[texColor.components.r][materialColor.components.r];
			dst.color.components.g = modulate_table[texColor.components.g][materialColor.components.g];
			dst.color.components.b = modulate_table[texColor.components.b][materialColor.components.b];
			dst.color.components.a = modulate_table[texColor.components.a][materialColor.components.a];
			break;
		case 1: //decal
		case 2:
		case 3: //..and everything else, for now
			u = invu/invw;
			v = invv/invw;
			texColor = sampler.sample(u,v);
			dst.color = texColor;
			break;
		}
	}

} shader;


struct Interpolator
{
	float dx, dy;
	float Z, pZ;

	struct {
		float x,y,z;
	} point0;
	
	Interpolator(int x1, int x2, int x3, int y1, int y2, int y3, float z1, float z2, float z3)
	{
		float A = (z3 - z1) * (y2 - y1) - (z2 - z1) * (y3 - y1);
		float B = (x3 - x1) * (z2 - z1) - (x2 - x1) * (z3 - z1);
		float C = (x2 - x1) * (y3 - y1) - (x3 - x1) * (y2 - y1);
		dx = -(float)A / C;
		dy = -(float)B / C;
		point0.x = x1;
		point0.y = y1;
		point0.z = z1;
	}

	Interpolator(int x1, int x2, int x3, int y1, int y2, int y3, int z1, int z2, int z3)
	{
		int A = (z3 - z1) * (y2 - y1) - (z2 - z1) * (y3 - y1);
		int B = (x3 - x1) * (z2 - z1) - (x2 - x1) * (z3 - z1);
		int C = (x2 - x1) * (y3 - y1) - (x3 - x1) * (y2 - y1);
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
	FORCEINLINE void incx(int count) { Z += dx*count; }
};

//http://www.devmaster.net/forums/showthread.php?t=1884&page=1
//todo - change to the tile-based renderer and try to apply some optimizations from that thread
void triangle_from_devmaster()
{
	// 28.4 fixed-point coordinates
    const int Y1 = iround(16.0f * verts[0].vert->coord[1]);
    const int Y2 = iround(16.0f * verts[1].vert->coord[1]);
    const int Y3 = iround(16.0f * verts[2].vert->coord[1]);

    const int X1 = iround(16.0f * verts[0].vert->coord[0]);
    const int X2 = iround(16.0f * verts[1].vert->coord[0]);
    const int X3 = iround(16.0f * verts[2].vert->coord[0]);

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

	float fx1 = verts[0].vert->coord[0], fy1 = verts[0].vert->coord[1];
	float fx2 = verts[1].vert->coord[0], fy2 = verts[1].vert->coord[1];
	float fx3 = verts[2].vert->coord[0], fy3 = verts[2].vert->coord[1];
	u8 r1 = verts[0].vert->color[0], g1 = verts[0].vert->color[1], b1 = verts[0].vert->color[2], a1 = verts[0].vert->color[3];
	u8 r2 = verts[1].vert->color[0], g2 = verts[1].vert->color[1], b2 = verts[1].vert->color[2], a2 = verts[1].vert->color[3];
	u8 r3 = verts[2].vert->color[0], g3 = verts[2].vert->color[1], b3 = verts[2].vert->color[2], a3 = verts[2].vert->color[3];
	int u1 = verts[0].vert->texcoord[0], v1 = verts[0].vert->texcoord[1];
	int u2 = verts[1].vert->texcoord[0], v2 = verts[1].vert->texcoord[1];
	int u3 = verts[2].vert->texcoord[0], v3 = verts[2].vert->texcoord[1];
	int w1 = verts[0].w, w2 = verts[1].w, w3 = verts[2].w;

	Interpolator i_color_r(fx1,fx2,fx3,fy1,fy2,fy3,r1,r2,r3);
	Interpolator i_color_g(fx1,fx2,fx3,fy1,fy2,fy3,g1,g2,g3);
	Interpolator i_color_b(fx1,fx2,fx3,fy1,fy2,fy3,b1,b2,b3);
	Interpolator i_color_a(fx1,fx2,fx3,fy1,fy2,fy3,a1,a2,a3);
	Interpolator i_tex_invu(fx1,fx2,fx3,fy1,fy2,fy3,u1*4096.0f/w1,u2*4096.0f/w2,u3*4096.0f/w3);
	Interpolator i_tex_invv(fx1,fx2,fx3,fy1,fy2,fy3,v1*4096.0f/w1,v2*4096.0f/w2,v3*4096.0f/w3);
	Interpolator i_w(fx1,fx2,fx3,fy1,fy2,fy3,w1,w2,w3);
	Interpolator i_invw(fx1,fx2,fx3,fy1,fy2,fy3,4096.0f/w1,4096.0f/w2,4096.0f/w3);
	

	i_color_r.init(minx,miny);
	i_color_g.init(minx,miny);
	i_color_b.init(minx,miny);
	i_color_a.init(minx,miny);
	i_tex_invu.init(minx,miny);
	i_tex_invv.init(minx,miny);
	i_w.init(minx,miny);
	i_invw.init(minx,miny);

    for(int y = miny; y < maxy; y++)
    {
		//HACK - bad screen clipping
        
		int CX1 = CY1;
        int CX2 = CY2;
        int CX3 = CY3;

		bool done = false;
		i_color_r.push(); i_color_g.push(); i_color_b.push(); i_color_a.push(); 
		i_tex_invu.push(); i_tex_invv.push();
		i_w.push(); i_invw.push();

		if(y>=0 && y<192)
		{			
			int xaccum = 1;
			for(int x = minx; x < maxx; x++)
			{
				if(CX1 > 0 && CX2 > 0 && CX3 > 0)
				{
					done = true;

					//reject out of bounds pixels
					if(x<0 || x>=256) goto rejected_fragment;

					//execute interpolators.
					//HACK: we defer this until we know we need it, and accumulate the number of deltas which are necessary.
					//this is just a temporary measure until we do proper clipping against the clip frustum.
					//since we dont, we are receiving waaay out of bounds polys and so unless we do this we spend a lot of time calculating
					//out of bounds pixels
					i_color_r.incx(xaccum); i_color_g.incx(xaccum); i_color_b.incx(xaccum); i_color_a.incx(xaccum);
					i_tex_invu.incx(xaccum); i_tex_invv.incx(xaccum);
					i_w.incx(xaccum); i_invw.incx(xaccum);
					xaccum = 0;

					int adr = (y<<8)+x;
					Fragment &destFragment = screen[adr];

					//w-buffer depth test
					int w = i_w.cur();
					if(w>destFragment.depth) 
						goto rejected_fragment;
					
					shader.invw = i_invw.Z;
					shader.invu = i_tex_invu.Z;
					shader.invv = i_tex_invv.Z;
					shader.materialColor.components.a = i_color_a.cur();
					shader.materialColor.components.r = i_color_r.cur();
					shader.materialColor.components.g = i_color_g.cur();
					shader.materialColor.components.b = i_color_b.cur();
					Fragment shaderOutput;
					shader.shade(shaderOutput);

					//alpha blend
					if(shaderOutput.color.components.a == 31)
					{
						destFragment.color = shaderOutput.color;
					}
					else
					{
						u8 alpha = shaderOutput.color.components.a+1;
						u8 invAlpha = 32 - alpha;
						destFragment.color.components.r = (alpha*shaderOutput.color.components.r + invAlpha*destFragment.color.components.r)>>5;
						destFragment.color.components.g = (alpha*shaderOutput.color.components.g + invAlpha*destFragment.color.components.g)>>5;
						destFragment.color.components.b = (alpha*shaderOutput.color.components.b + invAlpha*destFragment.color.components.b)>>5;
						destFragment.color.components.a = max(shaderOutput.color.components.a,destFragment.color.components.a);
					}

					destFragment.depth = w;

				} else if(done) break;
			rejected_fragment:
				xaccum++;

				CX1 -= FDY12;
				CX2 -= FDY23;
				CX3 -= FDY31;
			}
		} //end of y inbounds check
		i_color_a.pop(); i_color_a.incy();
		i_color_r.pop(); i_color_r.incy();
		i_color_g.pop(); i_color_g.incy();
		i_color_b.pop(); i_color_b.incy();
		i_tex_invu.pop(); i_tex_invu.incy();
		i_tex_invv.pop(); i_tex_invv.incy();
		i_w.pop(); i_w.incy();
		i_invw.pop(); i_invw.incy();


        CY1 += FDX12;
        CY2 += FDX23;
        CY3 += FDX31;

		desty++;
    }
}

static char Init(void)
{
	for(int i=0;i<32;i++)
		for(int j=0;j<32;j++)
			modulate_table[i][j] = ((i+1)*(j+1)-1)>>5;	

	return 1;
}

static void Reset() {}

static void Close() {}

static void VramReconfigureSignal() {
	TexCache_Invalidate();
}

static void GetLine(int line, u16* dst, u8* dstAlpha)
{
	Fragment* src = screen+((191-line)<<8);
	for(int i=0;i<256;i++)
	{

		u8 r = src->color.components.r;
		u8 g = src->color.components.g;
		u8 b = src->color.components.b;
		*dst = R5G5B5TORGB15(r,g,b);
		if(src->color.components.a > 0) 
			*dst |= 0x8000;
		*dstAlpha = alpha_5bit_to_4bit[src->color.components.a];
		src++;
		dst++;
		dstAlpha++;
	}

}

static void GetLineCaptured(int line, u16* dst) {}

static void Render()
{
	//transform verts and polys
	//which order?
	//A. clip
	//B. backface cull
	//C. transforms

	memset(screen,0,sizeof(screen));
	for(int i=0;i<256*192;i++)
		screen[i].depth = 0x007FFFFF;

	for(int i=0;i<gfx3d.vertlist->count;i++)
	{
		VERT &vert = gfx3d.vertlist->list[i];

		//perspective division and viewport transform
		vert.coord[0] = (vert.coord[0]+vert.coord[3])*256 / (2*vert.coord[3]) + 0;
		vert.coord[1] = (vert.coord[1]+vert.coord[3])*192 / (2*vert.coord[3]) + 0;
	}

	//a counter for how many polys got culled
	int culled = 0;

	u32 lastTextureFormat = 0, lastTexturePalette = 0, lastPolyAttr = 0;
	
	//iterate over polys
	for(int i=0;i<gfx3d.polylist->count;i++)
	{
		POLY *poly = &gfx3d.polylist->list[gfx3d.indexlist[i]];
		int type = poly->type;

		VERT* verts[4] = {
			&gfx3d.vertlist->list[poly->vertIndexes[0]],
			&gfx3d.vertlist->list[poly->vertIndexes[1]],
			&gfx3d.vertlist->list[poly->vertIndexes[2]],
			type==4?&gfx3d.vertlist->list[poly->vertIndexes[3]]:0
		};

		//HACK: backface culling
		//this should be moved to gfx3d, but first we need to redo the way the lists are built
		//because it is too convoluted right now.
		//(must we throw out verts if a poly gets backface culled? if not, then it might be easier)
		//TODO - use some freaking matrix and vector classes
		float ab[3], ac[3];
		Vector3Copy(ab,verts[0]->coord);
		Vector3Copy(ac,verts[0]->coord);
		Vector3Subtract(ab,verts[1]->coord);
		Vector3Subtract(ac,verts[2]->coord);
		float cross[3];
		Vector3Cross(cross,ab,ac);
		float view[3] = {0,0,1};
		float dot = Vector3Dot(view,cross);
		if(dot<0)  {
			culled++;
			continue;
		}

		if(i==0 || lastTextureFormat != poly->texParam || lastTexturePalette != poly->texPalette || lastPolyAttr != poly->polyAttr)
		{
			TexCache_SetTexture(poly->texParam,poly->texPalette);
			sampler.setup(poly->texParam);
			lastTextureFormat = poly->texParam;
			lastTexturePalette = poly->texPalette;
			lastPolyAttr = poly->polyAttr;
		}


		//note that when we build our triangle vert lists, we reorder them for our renderer.
		//we should probably fix the renderer so we dont have to do this;
		//but then again, what does it matter?
		if(type == 4) {

			SubmitVertex(verts[2]);
			SubmitVertex(verts[1]);
			SubmitVertex(verts[0]);

			triangle_from_devmaster();

			SubmitVertex(verts[0]);
			SubmitVertex(verts[3]);
			SubmitVertex(verts[2]);

			triangle_from_devmaster();

		}
		if(type == 3) {
			SubmitVertex(verts[2]);
			SubmitVertex(verts[1]);
			SubmitVertex(verts[0]);

			triangle_from_devmaster();
		}

	}

	//printf("rendered %d of %d polys after backface culling\n",gfx3d.polylist->count-culled,gfx3d.polylist->count);
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
