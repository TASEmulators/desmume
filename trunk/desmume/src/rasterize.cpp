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

#include "rasterize.h"

#include <algorithm>
#include <assert.h>
#include <math.h>
#include <string.h>

#include "bits.h"
#include "common.h"
#include "matrix.h"
#include "render3D.h"
#include "gfx3d.h"
#include "texcache.h"

using std::min;
using std::max;

template<typename T> T _min(T a, T b, T c) { return min(min(a,b),c); }
template<typename T> T _max(T a, T b, T c) { return max(max(a,b),c); }

static int polynum;

static u8 modulate_table[32][32];

//----texture cache---

//TODO - the texcache could ask for a buffer to generate into
//that would avoid us ever having to buffercopy..
struct TextureBuffers
{
	static const int numTextures = MAX_TEXTURE+1;
	u8* buffers[numTextures];

	void clear() { memset(buffers,0,sizeof(buffers)); }

	TextureBuffers()
	{
		clear();
	}

	void free()
	{
		for(int i=0;i<numTextures;i++)
			delete[] buffers[i];
		clear();
	}

	~TextureBuffers() {
		free();
	}

	void setCurrent(int num)
	{
		currentData = buffers[num];
	}

	void create(int num, u8* data)
	{
		delete[] buffers[num];
		int size = texcache[num].sizeX * texcache[num].sizeY * 4;
		buffers[num] = new u8[size];
		setCurrent(num);
		memcpy(currentData,data,size);
	}
	u8* currentData;
} textures;

//called from the texture cache to change the active texture
static void BindTexture(u32 tx)
{
	textures.setCurrent(tx);
}

//caled from the texture cache to change to a new texture
static void BindTextureData(u32 tx, u8* data)
{
	textures.create(tx,data);
}

//---------------

struct PolyAttr
{
	u32 val;

	bool decalMode;
	bool translucentDepthWrite;
	bool drawBackPlaneIntersectingPolys;
	u8 polyid;
	u8 alpha;

	bool isVisible(bool backfacing) 
	{
		switch((val>>6)&3) {
			case 0: return false;
			case 1: return backfacing;
			case 2: return !backfacing;
			case 3: return true;
			default: assert(false); return false;
		}
	}

	void setup(u32 polyAttr)
	{
		val = polyAttr;
		decalMode = BIT14(val);
		translucentDepthWrite = BIT11(val);
		polyid = (polyAttr>>24)&0x3F;
		alpha = (polyAttr>>16)&0x1F;
		drawBackPlaneIntersectingPolys = BIT12(val);
	}

} polyAttr;

struct Fragment
{
	union Color {
		u32 color;
		struct {
			//#ifdef WORDS_BIGENDIAN ?
			u8 r,g,b,a;
		} components;
	} color;

	u32 depth;

	struct {
		u8 opaque, translucent;
	} polyid;
	u8 pad[6];
};

static VERT* verts[3];

INLINE static void SubmitVertex(int vert_index, VERT* rawvert)
{
	verts[vert_index] = rawvert;
}

static Fragment screen[256*192];

FORCEINLINE int iround(float f) {
	return (int)f; //lol
}

static struct Sampler
{
	int width, height;
	int wmask, hmask;
	int wrap;
	int wshift;
	int texFormat;
	void setup(u32 texParam)
	{
		texFormat = (texParam>>26)&7;
		wshift = ((texParam>>20)&0x07) + 3;
		width=(1 << wshift);
		height=(8 << ((texParam>>23)&0x07));
		wmask = width-1;
		hmask = height-1;
		wrap = (texParam>>16)&0xF;
	}

	FORCEINLINE void clamp(int &val, const int size, const int sizemask){
		if(val<0) val = 0;
		if(val>sizemask) val = sizemask;
	}
	FORCEINLINE void hclamp(int &val) { clamp(val,width,wmask); }
	FORCEINLINE void vclamp(int &val) { clamp(val,height,hmask); }

	FORCEINLINE void repeat(int &val, const int size, const int sizemask) {
		val &= sizemask;
	}
	FORCEINLINE void hrepeat(int &val) { repeat(val,width,wmask); }
	FORCEINLINE void vrepeat(int &val) { repeat(val,height,hmask); }

	FORCEINLINE void flip(int &val, const int size, const int sizemask) {
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
		int iu = (int)floorf(u);
		int iv = (int)floorf(v);
		dowrap(iu,iv);

		Fragment::Color color;
		u32 col32 = ((u32*)textures.currentData)[(iv<<wshift)+iu];
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
		//if there is no texture set, then set to the mode which doesnt even use a texture
		if(sampler.texFormat == 0)
			mode = 4;
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
			dst.color.components.r = modulate_table[texColor.components.r][materialColor.components.r];
			dst.color.components.g = modulate_table[texColor.components.g][materialColor.components.g];
			dst.color.components.b = modulate_table[texColor.components.b][materialColor.components.b];
			dst.color.components.a = modulate_table[texColor.components.a][materialColor.components.a];
			//dst.color = materialColor;
			//dst.color.color = polynum*8+8;
			//dst.color.components.a = 31;
			break;
		case 1: //decal
		case 2:
		case 3: //..and everything else, for now
			u = invu/invw;
			v = invv/invw;
			texColor = sampler.sample(u,v);
			dst.color = texColor;
			break;
		case 4: //except for our own special mode which only uses the material color (for when texturing is disabled)
			dst.color = materialColor;
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
	
	Interpolator(float x1, float x2, float x3, float y1, float y2, float y3, float z1, float z2, float z3)
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

static void alphaBlend(Fragment & dst, const Fragment & src)
{
	if(gfx3d.enableAlphaBlending)
	{
		if(src.color.components.a == 0)
		{
			dst.color.components.a = max(src.color.components.a,dst.color.components.a);	
		}
		else if(src.color.components.a == 31 || dst.color.components.a == 0)
		{
			dst.color = src.color;
			dst.color.components.a = max(src.color.components.a,dst.color.components.a);
		}
		else
		{
			u8 alpha = src.color.components.a+1;
			u8 invAlpha = 32 - alpha;
			dst.color.components.r = (alpha*src.color.components.r + invAlpha*dst.color.components.r)>>5;
			dst.color.components.g = (alpha*src.color.components.g + invAlpha*dst.color.components.g)>>5;
			dst.color.components.b = (alpha*src.color.components.b + invAlpha*dst.color.components.b)>>5;
			dst.color.components.a = max(src.color.components.a,dst.color.components.a);
		}
	}
	else
	{
		if(src.color.components.a == 0)
		{
			//do nothing; the fragment is totally transparent
		}
		else
		{
			dst.color = src.color;
		}
	}
}

//http://www.devmaster.net/forums/showthread.php?t=1884&page=1
//todo - change to the tile-based renderer and try to apply some optimizations from that thread
static void triangle_from_devmaster()
{
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
    int minx = (_min(X1, X2, X3) + 0xF) >> 4;
    int maxx = (_max(X1, X2, X3) + 0xF) >> 4;
    int miny = (_min(Y1, Y2, Y3) + 0xF) >> 4;
    int maxy = (_max(Y1, Y2, Y3) + 0xF) >> 4;

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

	float fx1 = verts[0]->coord[0], fy1 = verts[0]->coord[1], fz1 = verts[0]->coord[2];
	float fx2 = verts[1]->coord[0], fy2 = verts[1]->coord[1], fz2 = verts[1]->coord[2];
	float fx3 = verts[2]->coord[0], fy3 = verts[2]->coord[1], fz3 = verts[2]->coord[2];
	u8 r1 = verts[0]->color[0], g1 = verts[0]->color[1], b1 = verts[0]->color[2], a1 = verts[0]->color[3];
	u8 r2 = verts[1]->color[0], g2 = verts[1]->color[1], b2 = verts[1]->color[2], a2 = verts[1]->color[3];
	u8 r3 = verts[2]->color[0], g3 = verts[2]->color[1], b3 = verts[2]->color[2], a3 = verts[2]->color[3];
	float u1 = verts[0]->texcoord[0], v1 = verts[0]->texcoord[1];
	float u2 = verts[1]->texcoord[0], v2 = verts[1]->texcoord[1];
	float u3 = verts[2]->texcoord[0], v3 = verts[2]->texcoord[1];
	float w1 = verts[0]->coord[3], w2 = verts[1]->coord[3], w3 = verts[2]->coord[3];

	Interpolator i_color_r(fx1,fx2,fx3,fy1,fy2,fy3,r1,r2,r3);
	Interpolator i_color_g(fx1,fx2,fx3,fy1,fy2,fy3,g1,g2,g3);
	Interpolator i_color_b(fx1,fx2,fx3,fy1,fy2,fy3,b1,b2,b3);
	Interpolator i_tex_invu(fx1,fx2,fx3,fy1,fy2,fy3,u1,u2,u3);
	Interpolator i_tex_invv(fx1,fx2,fx3,fy1,fy2,fy3,v1,v2,v3);
	Interpolator i_w(fx1,fx2,fx3,fy1,fy2,fy3,w1,w2,w3);
	Interpolator i_z(fx1,fx2,fx3,fy1,fy2,fy3,fz1,fz2,fz3);
	Interpolator i_invw(fx1,fx2,fx3,fy1,fy2,fy3,1.0f/w1,1.0f/w2,1.0f/w3);
	

	i_color_r.init(minx,miny);
	i_color_g.init(minx,miny);
	i_color_b.init(minx,miny);
	i_tex_invu.init(minx,miny);
	i_tex_invv.init(minx,miny);
	i_w.init(minx,miny); i_z.init(minx,miny);
	i_invw.init(minx,miny);

    for(int y = miny; y < maxy; y++)
    {
		//HACK - bad screen clipping
        
		int CX1 = CY1;
        int CX2 = CY2;
        int CX3 = CY3;

		bool done = false;
		i_color_r.push(); i_color_g.push(); i_color_b.push();
		i_tex_invu.push(); i_tex_invv.push();
		i_w.push(); i_invw.push(); i_z.push();

		if(y>=0 && y<192)
		{			
			int xaccum = 0;
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
					if(xaccum==1) {
						i_color_r.incx(); i_color_g.incx(); i_color_b.incx();
						i_tex_invu.incx(); i_tex_invv.incx();
						i_w.incx(); i_invw.incx(); i_z.incx();
					} else {
						i_color_r.incx(xaccum); i_color_g.incx(xaccum); i_color_b.incx(xaccum);
						i_tex_invu.incx(xaccum); i_tex_invv.incx(xaccum);
						i_w.incx(xaccum); i_invw.incx(xaccum); i_z.incx(xaccum);
					}

					xaccum = 0;

					int adr = (y<<8)+x;
					Fragment &destFragment = screen[adr];

					//depth test
					int depth;
					if(gfx3d.wbuffer)
						//not sure about this
						//this value was chosen to make the skybox, castle window decals, and water level render correctly in SM64
						depth = 4096/i_invw.Z; 
					else
					{
						float z = i_z.Z;
						depth = z*0x7FFF; //not sure about this
					}
					if(polyAttr.decalMode)
					{
						if(depth != destFragment.depth)
						{
							goto rejected_fragment;
						}
					}
					else
					{
						if(depth>=destFragment.depth) 
						{
							goto rejected_fragment;
						}
					}
					
					shader.invw = i_invw.Z;
					shader.invu = i_tex_invu.Z;
					shader.invv = i_tex_invv.Z;
					//this is a HACK: 
					//we are being very sloppy with our interpolation precision right now
					//and rather than fix it, i just want to clamp it
					shader.materialColor.components.a = polyAttr.alpha;
					shader.materialColor.components.r = max(0,min(31,i_color_r.cur()));
					shader.materialColor.components.g = max(0,min(31,i_color_g.cur()));
					shader.materialColor.components.b = max(0,min(31,i_color_b.cur()));

					//pixel shader
					Fragment shaderOutput;
					shader.shade(shaderOutput);

					//alpha test
					if(gfx3d.enableAlphaTest)
					{
						if(shaderOutput.color.components.a < gfx3d.alphaTestRef)
							goto rejected_fragment;
					}

					//handle polyids
					bool isOpaquePixel = shaderOutput.color.components.a == 31;
					if(isOpaquePixel)
					{
						destFragment.polyid.opaque = polyAttr.polyid;
					}
					else
					{
						//dont overwrite pixels on translucent polys with the same polyids
						if(destFragment.polyid.translucent == polyAttr.polyid)
							goto rejected_fragment;
						destFragment.polyid.translucent = polyAttr.polyid;						
					}


					//we shouldnt do any of this if we generated a totally transparent pixel
					if(shaderOutput.color.components.a != 0)
					{
						//alpha blending and write to framebuffer
						alphaBlend(destFragment, shaderOutput);

						//depth writing
						if(isOpaquePixel || polyAttr.translucentDepthWrite)
							destFragment.depth = depth;
					}

				} else if(done) break;
			
			rejected_fragment:
				xaccum++;

				CX1 -= FDY12;
				CX2 -= FDY23;
				CX3 -= FDY31;
			}
		} //end of y inbounds check
		i_color_r.pop(); i_color_r.incy();
		i_color_g.pop(); i_color_g.incy();
		i_color_b.pop(); i_color_b.incy();
		i_tex_invu.pop(); i_tex_invu.incy();
		i_tex_invv.pop(); i_tex_invv.incy();
		i_w.pop(); i_w.incy();
		i_z.pop(); i_z.incy();
		i_invw.pop(); i_invw.incy();


        CY1 += FDX12;
        CY2 += FDX23;
        CY3 += FDX31;

		desty++;
    }
}

static char SoftRastInit(void)
{
	for(int i=0;i<32;i++)
		for(int j=0;j<32;j++)
			modulate_table[i][j] = ((i+1)*(j+1)-1)>>5;	

	TexCache_Reset();
	TexCache_BindTexture = BindTexture;
	TexCache_BindTextureData = BindTextureData;

	return 1;
}

static void SoftRastReset() {}

static void SoftRastClose()
{
}

static void SoftRastVramReconfigureSignal() {
	TexCache_Invalidate();
}

static void SoftRastGetLine(int line, u16* dst, u8* dstAlpha)
{
	Fragment* src = screen+((191-line)<<8);
	for(int i=0;i<256;i++)
	{
		const bool testRenderAlpha = false;
		u8 r = src->color.components.r;
		u8 g = src->color.components.g;
		u8 b = src->color.components.b;
		*dst = R5G5B5TORGB15(r,g,b);
		if(src->color.components.a > 0) 
			*dst |= 0x8000;
		*dstAlpha = alpha_5bit_to_4bit[src->color.components.a];

		if(testRenderAlpha)
		{
			*dst = 0x8000 | R5G5B5TORGB15(src->color.components.a,src->color.components.a,src->color.components.a);
			*dstAlpha = 16;
		}

		src++;
		dst++;
		dstAlpha++;
	}

}

static void SoftRastGetLineCaptured(int line, u16* dst) {
	Fragment* src = screen+((191-line)<<8);
	for(int i=0;i<256;i++)
	{
		const bool testRenderAlpha = false;
		u8 r = src->color.components.r;
		u8 g = src->color.components.g;
		u8 b = src->color.components.b;
		*dst = R5G5B5TORGB15(r,g,b);
		if(src->color.components.a > 0) 
			*dst |= 0x8000;
		src++;
		dst++;
	}
}

static struct TClippedPoly
{
	int type;
	POLY* poly;
	VERT clipVerts[16]; //how many? i cant imagine having more than 6
} clippedPolys[POLYLIST_SIZE*2];
static int clippedPolyCounter;

TClippedPoly tempClippedPoly;
TClippedPoly outClippedPoly;

template<typename T>
static T interpolate(const float ratio, const T& x0, const T& x1) {
	return x0 + (float)(x1-x0) * (ratio);
}


//http://www.cs.berkeley.edu/~ug/slide/pipeline/assignments/as6/discussion.shtml
static VERT clipPoint(VERT* inside, VERT* outside, int coord, int which)
{
	VERT ret;

	float coord_inside = inside->coord[coord];
	float coord_outside = outside->coord[coord];
	float w_inside = inside->coord[3];
	float w_outside = outside->coord[3];

	float t;

	if(which==-1) {
		w_outside = -w_outside;
		w_inside = -w_inside;
	}
	
	t = (coord_inside - w_inside)/ ((w_outside-w_inside) - (coord_outside-coord_inside));
	

#define INTERP(X) ret . X = interpolate(t, inside-> X ,outside-> X )
	INTERP(color[0]); INTERP(color[1]); INTERP(color[2]);
	INTERP(coord[0]); INTERP(coord[1]); INTERP(coord[2]); INTERP(coord[3]);
	INTERP(texcoord[0]); INTERP(texcoord[1]);

	//this seems like a prudent measure to make sure that math doesnt make a point pop back out
	//of the clip volume through interpolation
	if(which==-1)
		ret.coord[coord] = -ret.coord[3];
	else
		ret.coord[coord] = ret.coord[3];

	return ret;
}

//#define CLIPLOG(X) printf(X);
//#define CLIPLOG2(X,Y,Z) printf(X,Y,Z);
#define CLIPLOG(X)
#define CLIPLOG2(X,Y,Z)

static void clipSegmentVsPlane(VERT** verts, const int coord, int which)
{
	bool out0, out1;
	if(which==-1) 
		out0 = verts[0]->coord[coord] < -verts[0]->coord[3];
	else 
		out0 = verts[0]->coord[coord] > verts[0]->coord[3];
	if(which==-1) 
		out1 = verts[1]->coord[coord] < -verts[1]->coord[3];
	else
		out1 = verts[1]->coord[coord] > verts[1]->coord[3];

	//CONSIDER: should we try and clip things behind the eye? does this code even successfully do it? not sure.
	//if(coord==2 && which==1) {
	//	out0 = verts[0]->coord[2] < 0;
	//	out1 = verts[1]->coord[2] < 0;
	//}

	//both outside: insert no points
	if(out0 && out1) {
		CLIPLOG(" both outside\n");
	}

	//both inside: insert the first point
	if(!out0 && !out1) 
	{
		CLIPLOG(" both inside\n");
		outClippedPoly.clipVerts[outClippedPoly.type++] = *verts[1];
	}

	//exiting volume: insert the clipped point and the first (interior) point
	if(!out0 && out1)
	{
		CLIPLOG(" exiting\n");
		outClippedPoly.clipVerts[outClippedPoly.type++] = clipPoint(verts[0],verts[1], coord, which);
	}

	//entering volume: insert clipped point
	if(out0 && !out1) {
		CLIPLOG(" entering\n");
		outClippedPoly.clipVerts[outClippedPoly.type++] = clipPoint(verts[1],verts[0], coord, which);
		outClippedPoly.clipVerts[outClippedPoly.type++] = *verts[1];

	}
}

static void clipPolyVsPlane(const int coord, int which)
{
	outClippedPoly.type = 0;
	CLIPLOG2("Clipping coord %d against %f\n",coord,x);
	for(int i=0;i<tempClippedPoly.type;i++)
	{
		VERT* testverts[2] = {&tempClippedPoly.clipVerts[i],&tempClippedPoly.clipVerts[(i+1)%tempClippedPoly.type]};
		clipSegmentVsPlane(testverts, coord, which);
	}
	tempClippedPoly = outClippedPoly;
}

static void clipPoly(POLY* poly)
{
	int type = poly->type;

	CLIPLOG("==Begin poly==\n");

	tempClippedPoly.clipVerts[0] = gfx3d.vertlist->list[poly->vertIndexes[0]];
	tempClippedPoly.clipVerts[1] = gfx3d.vertlist->list[poly->vertIndexes[1]];
	tempClippedPoly.clipVerts[2] = gfx3d.vertlist->list[poly->vertIndexes[2]];
	if(type==4)
		tempClippedPoly.clipVerts[3] = gfx3d.vertlist->list[poly->vertIndexes[3]];

	
	tempClippedPoly.type = type;

	clipPolyVsPlane(0, -1); 
	clipPolyVsPlane(0, 1);
	clipPolyVsPlane(1, -1);
	clipPolyVsPlane(1, 1);
	clipPolyVsPlane(2, -1);
	clipPolyVsPlane(2, 1);

	//TODO - we need to parameterize back plane clipping

	if(tempClippedPoly.type < 3)
	{
		//a totally clipped poly. discard it.
		//or, a degenerate poly. we're not handling these right now
	}
	else if(tempClippedPoly.type < 5) 
	{
		clippedPolys[clippedPolyCounter] = tempClippedPoly;
		clippedPolys[clippedPolyCounter].poly = poly;
		clippedPolyCounter++;
	}
	else
	{
		//turn into a triangle fan. no point even trying to make quads since those would have to get split anyway.
		//well, maybe it could end up being faster.
		for(int i=0;i<tempClippedPoly.type-2;i++)
		{
			clippedPolys[clippedPolyCounter].type = 3;
			clippedPolys[clippedPolyCounter].poly = poly;
			clippedPolys[clippedPolyCounter].clipVerts[0] = tempClippedPoly.clipVerts[0];
			clippedPolys[clippedPolyCounter].clipVerts[1] = tempClippedPoly.clipVerts[i+1];
			clippedPolys[clippedPolyCounter].clipVerts[2] = tempClippedPoly.clipVerts[i+2];
			clippedPolyCounter++;
		}
	}
}

static void SoftRastRender()
{
	Fragment clearFragment;
	clearFragment.color.components.r = gfx3d.clearColor&0x1F;
	clearFragment.color.components.g = (gfx3d.clearColor>>5)&0x1F;
	clearFragment.color.components.b = (gfx3d.clearColor>>10)&0x1F;
	clearFragment.color.components.a = (gfx3d.clearColor>>16)&0x1F;
	clearFragment.polyid.opaque = clearFragment.polyid.translucent = (gfx3d.clearColor>>24)&0x3F;
	clearFragment.depth = gfx3d.clearDepth;
	for(int i=0;i<256*192;i++)
		screen[i] = clearFragment;

	//printf("%d\n",gfx3d.wbuffer?1:0);

	//submit all polys to clipper
	clippedPolyCounter = 0;
	for(int i=0;i<gfx3d.polylist->count;i++)
	{
		clipPoly(&gfx3d.polylist->list[gfx3d.indexlist[i]]);
	}

	//viewport transforms
	for(int i=0;i<clippedPolyCounter;i++)
	{
		TClippedPoly &poly = clippedPolys[i];
		for(int j=0;j<poly.type;j++)
		{
			VERT &vert = poly.clipVerts[j];

			//homogeneous divide
			vert.coord[0] = (vert.coord[0]+vert.coord[3]) / (2*vert.coord[3]);
			vert.coord[1] = (vert.coord[1]+vert.coord[3]) / (2*vert.coord[3]);
			vert.coord[2] = (vert.coord[2]+vert.coord[3]) / (2*vert.coord[3]);
			vert.texcoord[0] /= vert.coord[3];
			vert.texcoord[1] /= vert.coord[3];

			//CONSIDER: do we need to guarantee that these are in bounds? perhaps not.
			//vert.coord[0] = max(0.0f,min(1.0f,vert.coord[0]));
			//vert.coord[1] = max(0.0f,min(1.0f,vert.coord[1]));
			//vert.coord[2] = max(0.0f,min(1.0f,vert.coord[2]));

			//viewport transformation
			vert.coord[0] *= 256;
			vert.coord[1] *= 192;
			//vert.coord[3] *= 4096; //not sure about this, sometimes I think we might need this here
		}
	}

	//a counter for how many polys got culled
	int culled = 0;

	u32 lastTextureFormat = 0, lastTexturePalette = 0, lastPolyAttr = 0;
	
	//iterate over polys
	bool needInitTexture = true;
	for(int i=0;i<clippedPolyCounter;i++)
	{
		polynum = i;

		TClippedPoly &clippedPoly = clippedPolys[i];
		POLY *poly = clippedPoly.poly;
		int type = clippedPoly.type;

		VERT* verts[4] = {
			&clippedPoly.clipVerts[0],
			&clippedPoly.clipVerts[1],
			&clippedPoly.clipVerts[2],
			type==4?&clippedPoly.clipVerts[3]:0
		};

		if(i == 0 || lastPolyAttr != poly->polyAttr)
		{
			polyAttr.setup(poly->polyAttr);
			lastPolyAttr = poly->polyAttr;
		}

		//HACK: backface culling
		//this should be moved to gfx3d, but first we need to redo the way the lists are built
		//because it is too convoluted right now.
		//(must we throw out verts if a poly gets backface culled? if not, then it might be easier)
		float ab[2], ac[2];
		Vector2Copy(ab, verts[1]->coord);
		Vector2Copy(ac, verts[2]->coord);
		Vector2Subtract(ab, verts[0]->coord);
		Vector2Subtract(ac, verts[0]->coord);
		float cross = Vector2Cross(ab, ac);
		bool backfacing = (cross<0);


		if(!polyAttr.isVisible(backfacing)) {
			culled++;
			continue;
		}
	
		if(needInitTexture || lastTextureFormat != poly->texParam || lastTexturePalette != poly->texPalette)
		{
			TexCache_SetTexture(poly->texParam,poly->texPalette);
			sampler.setup(poly->texParam);
			lastTextureFormat = poly->texParam;
			lastTexturePalette = poly->texPalette;
			needInitTexture = false;
		}

		//hmm... shader gets setup every time because it depends on sampler which may have just changed
		shader.setup(poly->polyAttr);

		//note that when we build our triangle vert lists, we reorder them for our renderer.
		//we should probably fix the renderer so we dont have to do this;
		//but then again, what does it matter?
		if(type == 4)
		{
			if(backfacing)
			{
				SubmitVertex(0,verts[0]);
				SubmitVertex(1,verts[1]);
				SubmitVertex(2,verts[2]);
				triangle_from_devmaster();

				SubmitVertex(0,verts[2]);
				SubmitVertex(1,verts[3]);
				SubmitVertex(2,verts[0]);
				triangle_from_devmaster();
			}
			else
			{
				SubmitVertex(0,verts[2]);
				SubmitVertex(1,verts[1]);
				SubmitVertex(2,verts[0]);
				triangle_from_devmaster();

				SubmitVertex(0,verts[0]);
				SubmitVertex(1,verts[3]);
				SubmitVertex(2,verts[2]);
				triangle_from_devmaster();
			}
		}
		else if(type == 3)
		{
			if(backfacing)
			{
				SubmitVertex(0,verts[0]);
				SubmitVertex(1,verts[1]);
				SubmitVertex(2,verts[2]);
				triangle_from_devmaster();
			}
			else
			{
				SubmitVertex(0,verts[2]);
				SubmitVertex(1,verts[1]);
				SubmitVertex(2,verts[0]);
				triangle_from_devmaster();
			}
		} else printf("skipping type %d\n",type);

	}

	//	printf("rendered %d of %d polys after backface culling\n",gfx3d.polylist->count-culled,gfx3d.polylist->count);

}

GPU3DInterface gpu3DRasterize = {
	"SoftRasterizer",
	SoftRastInit,
	SoftRastReset,
	SoftRastClose,
	SoftRastRender,
	SoftRastVramReconfigureSignal,
	SoftRastGetLine,
	SoftRastGetLineCaptured
};
