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

//#define USE_HECKER

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
#include "NDSSystem.h"

using std::min;
using std::max;
using std::swap;

template<typename T> T _min(T a, T b, T c) { return min(min(a,b),c); }
template<typename T> T _max(T a, T b, T c) { return max(max(a,b),c); }
template<typename T> T _min(T a, T b, T c, T d) { return min(_min(a,b,d),c); }
template<typename T> T _max(T a, T b, T c, T d) { return max(_max(a,b,d),c); }

static int polynum;

static u8 modulate_table[32][32];
static u8 decal_table[32][32][32];

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

	u8 stencil;

	u8 pad[5];
};

static VERT* verts[3];

INLINE static void SubmitVertex(int vert_index, VERT* rawvert)
{
	//HACK - reverse winding
	#ifdef USE_HECKER
		verts[2-vert_index] = rawvert;
	#else
		verts[vert_index] = rawvert;
	#endif
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
		//should we use floor here? I still think so, but now I am not so sure.
		//note that I changed away from floor for accuracy reasons, not performance, even though it would be faster without the floor.
		//however, I am afraid this could be masking some other texturing precision issue. in fact, I am pretty sure of this.
		//there are still lingering issues in 2d games
		//int iu = (int)floorf(u);
		//int iv = (int)floorf(v);
		int iu = (int)(u);
		int iv = (int)(v);
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
		if(sampler.texFormat == 0 && mode == 0)
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
			#ifdef _MSC_VER
			if(GetAsyncKeyState(VK_SHIFT)) {
				//debugging tricks
				dst.color = materialColor;
				if(GetAsyncKeyState(VK_TAB)) {
					u8 alpha = dst.color.components.a;
					dst.color.color = polynum*8+8;
					dst.color.components.a = alpha;
				}
			}
			#endif
			break;
		case 1: //decal
			u = invu/invw;
			v = invv/invw;
			texColor = sampler.sample(u,v);
			dst.color.components.r = decal_table[texColor.components.a][texColor.components.r][materialColor.components.r];
			dst.color.components.g = decal_table[texColor.components.a][texColor.components.g][materialColor.components.g];
			dst.color.components.b = decal_table[texColor.components.a][texColor.components.b][materialColor.components.b];
			dst.color.components.a = materialColor.components.a;
			break;
		case 2: //toon/highlight shading
			u = invu/invw;
			v = invv/invw;
			texColor = sampler.sample(u,v);
			u32 toonColorVal; toonColorVal = gfx3d.rgbToonTable[materialColor.components.r];
			Fragment::Color toonColor;
			toonColor.components.r = ((toonColorVal & 0x0000FF) >>  3);
			toonColor.components.g = ((toonColorVal & 0x00FF00) >> 11);
			toonColor.components.b = ((toonColorVal & 0xFF0000) >> 19);
			dst.color.components.r = modulate_table[texColor.components.r][toonColor.components.r];
			dst.color.components.g = modulate_table[texColor.components.g][toonColor.components.g];
			dst.color.components.b = modulate_table[texColor.components.b][toonColor.components.b];
			dst.color.components.a = modulate_table[texColor.components.a][materialColor.components.a];
			if(gfx3d.shading == GFX3D::HIGHLIGHT)
			{
				dst.color.components.r = min<u8>(31, (dst.color.components.r + toonColor.components.r));
				dst.color.components.g = min<u8>(31, (dst.color.components.g + toonColor.components.g));
				dst.color.components.b = min<u8>(31, (dst.color.components.b + toonColor.components.b));
			}
			break;
		case 3: //shadows
			//is this right? only with the material color?
			dst.color = materialColor;
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

static void alphaBlend(Fragment::Color & dst, const Fragment::Color & src)
{
	if(gfx3d.enableAlphaBlending)
	{
		if(src.components.a == 0)
		{
			dst.components.a = max(src.components.a,dst.components.a);	
		}
		else if(src.components.a == 31 || dst.components.a == 0)
		{
			dst.color = src.color;
			dst.components.a = max(src.components.a,dst.components.a);
		}
		else
		{
			u8 alpha = src.components.a+1;
			u8 invAlpha = 32 - alpha;
			dst.components.r = (alpha*src.components.r + invAlpha*dst.components.r)>>5;
			dst.components.g = (alpha*src.components.g + invAlpha*dst.components.g)>>5;
			dst.components.b = (alpha*src.components.b + invAlpha*dst.components.b)>>5;
			dst.components.a = max(src.components.a,dst.components.a);
		}
	}
	else
	{
		if(src.components.a == 0)
		{
			//do nothing; the fragment is totally transparent
		}
		else
		{
			dst.color = src.color;
		}
	}
}

void pixel(int adr,float r, float g, float b, float invu, float invv, float invw, float z) {
	Fragment &destFragment = screen[adr];

	//depth test
	int depth;
	if(gfx3d.wbuffer)
		//not sure about this
		//this value was chosen to make the skybox, castle window decals, and water level render correctly in SM64
		depth = 4096/invw;
	else
	{
		depth = z*0x7FFF; //not sure about this
	}
	if(polyAttr.decalMode)
	{
		if(depth != destFragment.depth)
		{
			goto depth_fail;
		}
	}
	else
	{
		if(depth>=destFragment.depth) 
		{
			goto depth_fail;
		}
	}
	
	shader.invw = invw;
	shader.invu = invu;
	shader.invv = invv;

	//perspective-correct the colors
	r = (r / invw) + 0.5f;
	g = (g / invw) + 0.5f;
	b = (b / invw) + 0.5f;

	//this is a HACK: 
	//we are being very sloppy with our interpolation precision right now
	//and rather than fix it, i just want to clamp it
	shader.materialColor.components.r = max(0,min(31,(int)r));
	shader.materialColor.components.g = max(0,min(31,(int)g));
	shader.materialColor.components.b = max(0,min(31,(int)b));

	shader.materialColor.components.a = polyAttr.alpha;

	//pixel shader
	Fragment shaderOutput;
	shader.shade(shaderOutput);

	//alpha test
	if(gfx3d.enableAlphaTest)
	{
		if(shaderOutput.color.components.a < gfx3d.alphaTestRef)
			goto rejected_fragment;
	}

	//we shouldnt do any of this if we generated a totally transparent pixel
	if(shaderOutput.color.components.a != 0)
	{
		//handle shadow polys
		if(shader.mode == 3)
		{
			//now, if we arent supposed to draw shadow here, then bail out
			if(destFragment.stencil == 0)
				goto rejected_fragment;

			//reset the shadow flag to keep the shadow from getting drawn more than once
			destFragment.stencil = 0;
		}

		//handle polyids
		bool isOpaquePixel = shaderOutput.color.components.a == 31;
		if(isOpaquePixel)
		{
			destFragment.polyid.opaque = polyAttr.polyid;
		}
		else
		{
			//interesting that we have to check for mode 3 here. not very straightforward, but then nothing about the shadows are
			//this was the result of testing trauma center, SPP area menu, and SM64 with yoshi's red feet behind translucent trees
			//as well as the intermittent bug in ff4 where your shadow only appears under other shadows
			if(shader.mode != 3)
			{
				//dont overwrite pixels on translucent polys with the same polyids
				if(destFragment.polyid.translucent == polyAttr.polyid)
					goto rejected_fragment;
			
				destFragment.polyid.translucent = polyAttr.polyid;
			}
		}

		//alpha blending and write color
		alphaBlend(destFragment.color, shaderOutput.color);

		//depth writing
		if(isOpaquePixel || polyAttr.translucentDepthWrite)
			destFragment.depth = depth;

	}

	goto done_with_pixel;

	depth_fail:
	//handle stencil-writing in shadow mode
	if(shader.mode == 3)
	{
		destFragment.stencil = 1;
	}

	rejected_fragment:
	done_with_pixel:
	;
}

void scanline(int y, int xstart, int xend, 
			  float r0, float g0, float b0, float invu0, float invv0, float invw0, float z0,
			  float r1, float g1, float b1, float invu1, float invv1, float invw1, float z1)
{
	float dx = xend-xstart+1;
	float dr_dx = (r1-r0)/dx;
	float dg_dx = (g1-g0)/dx;
	float db_dx = (b1-b0)/dx;
	float du_dx = (invu1-invu0)/dx;
	float dv_dx = (invv1-invv0)/dx;
	float dw_dx = (invw1-invw0)/dx;
	float dz_dx = (z1-z0)/dx;
	for(int x=xstart;x<=xend;x++) {
		int adr = (y<<8)+x;
		pixel(adr,r0,g0,b0,invu0,invv0,invw0,z0);
		r0 += dr_dx;
		g0 += dg_dx;
		b0 += db_dx;
		invu0 += du_dx;
		invv0 += dv_dx;
		invw0 += dw_dx;
		z0 += dz_dx;
	}
}

typedef int fixed28_4;

inline fixed28_4 FloatToFixed28_4( float Value ) {
	return Value * 16;
}
inline float Fixed28_4ToFloat( fixed28_4 Value ) {
	return Value / 16.0;
}
//inline fixed16_16 FloatToFixed16_16( float Value ) {
//	return Value * 65536;
//}
//inline float Fixed16_16ToFloat( fixed16_16 Value ) {
//	return Value / 65536.0;
//}
inline fixed28_4 Fixed28_4Mul( fixed28_4 A, fixed28_4 B ) {
	// could make this asm to prevent overflow
	return (A * B) / 16;	// 28.4 * 28.4 = 24.8 / 16 = 28.4
}
inline long Ceil28_4( fixed28_4 Value ) {
	long ReturnValue;
	long Numerator = Value - 1 + 16;
	if(Numerator >= 0) {
		ReturnValue = Numerator/16;
	} else {
		// deal with negative numerators correctly
		ReturnValue = -((-Numerator)/16);
		ReturnValue -= ((-Numerator) % 16) ? 1 : 0;
	}
	return ReturnValue;
}


struct Derivatives {
	float dx, dy;
	void calculate(float v0, float v1, float v2, float invdx, float invdy, const VERT** pVertices)
	{
		dx = invdx * (
		  ((v1 - v2) * Fixed28_4ToFloat(pVertices[0]->y - pVertices[2]->y)) 
		- ((v0 - v2) * Fixed28_4ToFloat(pVertices[1]->y - pVertices[2]->y))
		);

		dy = invdy * (
		  ((v1 - v2) * Fixed28_4ToFloat(pVertices[0]->x - pVertices[2]->x)) 
		- ((v0 - v2) * Fixed28_4ToFloat(pVertices[1]->x - pVertices[2]->x))
		);
	}
};

struct gradients_fx_fl {
	gradients_fx_fl( const VERT **pVertices );
	float invw[3];
	struct GradientDerivatives {
		Derivatives invw, z, u, v, color[3];
	} d;
};

struct edge_fx_fl {
	edge_fx_fl(gradients_fx_fl const &Gradients, VERT **pVertices, int Top,
			int Bottom );
	inline int Step( void );

	long X, XStep, Numerator, Denominator;			// DDA info for x
	long ErrorTerm;
	int Y, Height;					// current y and vertical count
	
	struct Interpolant {
		float curr, step, stepExtra;
		void doStep() { curr += step; }
		void doStepExtra() { curr += stepExtra; }
		void initialize(float val, long XStep, float XPrestep, float YPrestep, const Derivatives& gradient) {
			curr = val + YPrestep * gradient.dy
						+ XPrestep * gradient.dx;
			step = XStep * gradient.dx + gradient.dy;
			stepExtra = gradient.dx;
		}
	};
	
	static const int NUM_INTERPOLANTS = 7;
	union {
		struct {
			Interpolant invw,z,u,v,color[3];
		};
		Interpolant interpolants[NUM_INTERPOLANTS];
	};
	void doStepInterpolants() { for(int i=0;i<NUM_INTERPOLANTS;i++) interpolants[i].doStep(); }
	void doStepExtraInterpolants() { for(int i=0;i<NUM_INTERPOLANTS;i++) interpolants[i].doStepExtra(); }
};

inline int edge_fx_fl::Step( void ) {
	X += XStep; Y++; Height--;
	doStepInterpolants();

	ErrorTerm += Numerator;
	if(ErrorTerm >= Denominator) {
		X++;
		ErrorTerm -= Denominator;
		doStepExtraInterpolants();
	}
	return Height;
}	


gradients_fx_fl::gradients_fx_fl( const VERT **verts )
{
	int Counter;

	fixed28_4 X1Y0 = Fixed28_4Mul(verts[1]->x - verts[2]->x,
							verts[0]->y - verts[2]->y);
	fixed28_4 X0Y1 = Fixed28_4Mul(verts[0]->x - verts[2]->x,
							verts[1]->y - verts[2]->y);
	float invdx = 1.0 / Fixed28_4ToFloat(X1Y0 - X0Y1);

	float invdy = -invdx;

	for(int i=0;i<3;i++) invw[i] = 1/verts[i]->w;


	d.invw.calculate(invw[0],invw[1],invw[2],invdx,invdy,verts);
	d.u.calculate(verts[0]->u,verts[1]->u,verts[2]->u,invdx,invdy,verts);
	d.v.calculate(verts[0]->v,verts[1]->v,verts[2]->v,invdx,invdy,verts);
	d.z.calculate(verts[0]->z,verts[1]->z,verts[2]->z,invdx,invdy,verts);
	for(int i=0;i<3;i++)
		d.color[i].calculate(verts[0]->fcolor[i],verts[1]->fcolor[i],verts[2]->fcolor[i],invdx,invdy,verts);
}


/********** handle floor divides and mods correctly ***********/

inline void FloorDivMod( long Numerator, long Denominator, long &Floor,
				long &Mod )
{
	assert(Denominator > 0);		// we assume it's positive
	if(Numerator >= 0) {
		// positive case, C is okay
		Floor = Numerator / Denominator;
		Mod = Numerator % Denominator;
	} else {
		// Numerator is negative, do the right thing
		Floor = -((-Numerator) / Denominator);
		Mod = (-Numerator) % Denominator;
		if(Mod) {
			// there is a remainder
			Floor--; Mod = Denominator - Mod;
		}
	}
}

/********** edge_fx_fl constructor ***********/

edge_fx_fl::edge_fx_fl( gradients_fx_fl const &Gradients, VERT **verts, int Top,
		int Bottom )
{
	Y = Ceil28_4(verts[Top]->y);
	int YEnd = Ceil28_4(verts[Bottom]->y);
	Height = YEnd - Y;

	if(Height)
	{
		long dN = verts[Bottom]->y - verts[Top]->y;
		long dM = verts[Bottom]->x - verts[Top]->x;
	
		long InitialNumerator = dM*16*Y - dM*verts[Top]->y + dN*verts[Top]->x - 1 + dN*16;
		FloorDivMod(InitialNumerator,dN*16,X,ErrorTerm);
		FloorDivMod(dM*16,dN*16,XStep,Numerator);
		Denominator = dN*16;
	
		float YPrestep = Fixed28_4ToFloat(Y*16 - verts[Top]->y);
		float XPrestep = Fixed28_4ToFloat(X*16 - verts[Top]->x);
	
		invw.initialize(Gradients.invw[Top],XStep,XPrestep,YPrestep,Gradients.d.invw);
		u.initialize(verts[Top]->u,XStep,XPrestep,YPrestep,Gradients.d.u);
		v.initialize(verts[Top]->v,XStep,XPrestep,YPrestep,Gradients.d.v);
		z.initialize(verts[Top]->z,XStep,XPrestep,YPrestep,Gradients.d.z);
		for(int i=0;i<3;i++)
			color[i].initialize(verts[Top]->fcolor[i],XStep,XPrestep,YPrestep,Gradients.d.color[i]);
	}
}


void hecker_DrawScanLine( gradients_fx_fl const &Gradients,edge_fx_fl *pLeft, edge_fx_fl *pRight )
{
	int XStart = pLeft->X;
	int Width = pRight->X - XStart;

	//char unsigned *pDestBits = Dest.pBits;
	//char unsigned * const pTextureBits = Texture.pBits;
	//pDestBits += pLeft->Y * Dest.DeltaScan + XStart;
	//long TextureDeltaScan = Texture.DeltaScan;

	float invw = pLeft->invw.curr;
	float u = pLeft->u.curr;
	float v = pLeft->v.curr;
	float z = pLeft->z.curr;
	float color[3] = {
		pLeft->color[0].curr,
		pLeft->color[1].curr,
		pLeft->color[2].curr };

	//scanline(pLeft->Y,pLeft->X,pRight->X,31,31,31,0,0,0,1/pLeft->OneOverZ,31,31,31,0,0,

	int adr = (pLeft->Y<<8)+XStart;

	while(Width-- > 0)
	{
		float W = 1/invw;

		pixel(adr,color[0],color[1],color[2],u,v,invw,z);
		adr++;

		invw += Gradients.d.invw.dx;
		u += Gradients.d.u.dx;
		v += Gradients.d.v.dx;
		z += Gradients.d.z.dx;
		for(int i=0;i<3;i++) color[i] += Gradients.d.color[i].dx;
	}
}

static void runscanline(gradients_fx_fl & gradients, edge_fx_fl *left, edge_fx_fl *right)
{
	int Height = min(left->Height,right->Height);
	while(Height--) {
		hecker_DrawScanLine(gradients,left,right);
		left->Step(); right->Step();
	}
}

//http://chrishecker.com/Miscellaneous_Technical_Articles
static void triangle_from_hecker()
{
	VERT* v[3] = {verts[0],verts[1],verts[2]};

	//rotate verts until vert0.y is minimum, and then vert0.x is minimum in case of ties
	//this will reduce the complexity of our logic
	while(v[0]->y > v[1]->y || v[0]->y > v[2]->y) {
		swap(v[0],v[1]);
		swap(v[1],v[2]);
	}
	while(v[0]->y == v[1]->y && v[0]->x > v[1]->x) {
		swap(v[0],v[1]);
		swap(v[1],v[2]);
	}

	//wants clockwise

	fixed28_4 Y0 = v[0]->y, Y1 = v[1]->y, Y2 = v[2]->y;

	gradients_fx_fl Gradients((const VERT**)v);

	if(Y0 == Y1)
	{
		//if the first two points have the same y-coord, then there is only one pair of edges
		edge_fx_fl edge1 = edge_fx_fl(Gradients,v,0,2);
		edge_fx_fl edge2 = edge_fx_fl(Gradients,v,1,2);
		runscanline(Gradients,&edge1,&edge2);
	}
	else if(Y1 == Y2)
	{
		//if the last two points have the same y-coord then there is only one pair of edges
		edge_fx_fl edge1 = edge_fx_fl(Gradients,v,0,2);
		edge_fx_fl edge2 = edge_fx_fl(Gradients,v,0,1);
		runscanline(Gradients,&edge1,&edge2);
	}
	else
	{
		//there are two pairs of edges
		if(Y1<Y2)
		{
			//a triangle like 
			// 0
			//   1
			//2
			edge_fx_fl edge1 = edge_fx_fl(Gradients,v,0,2);
			edge_fx_fl edge2 = edge_fx_fl(Gradients,v,0,1);
			runscanline(Gradients,&edge1,&edge2);
			edge2 = edge_fx_fl(Gradients,v,1,2);
			runscanline(Gradients,&edge1,&edge2);
		}
		else
		{
			//a triangle like 
			// 0
			//2   
			//   1
			edge_fx_fl edge1 = edge_fx_fl(Gradients,v,0,2);
			edge_fx_fl edge2 = edge_fx_fl(Gradients,v,0,1);
			runscanline(Gradients,&edge1,&edge2);
			edge1 = edge_fx_fl(Gradients,v,2,1);
			runscanline(Gradients,&edge1,&edge2);
		}
	}

}


//http://www.devmaster.net/forums/showthread.php?t=1884&page=1
static void triangle_from_devmaster()
{
#ifdef USE_HECKER
	{
		triangle_from_hecker();
		return;
	}
#endif

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
	float r1 = verts[0]->fcolor[0], g1 = verts[0]->fcolor[1], b1 = verts[0]->fcolor[2];
	float r2 = verts[1]->fcolor[0], g2 = verts[1]->fcolor[1], b2 = verts[1]->fcolor[2];
	float r3 = verts[2]->fcolor[0], g3 = verts[2]->fcolor[1], b3 = verts[2]->fcolor[2];
	float u1 = verts[0]->texcoord[0], v1 = verts[0]->texcoord[1];
	float u2 = verts[1]->texcoord[0], v2 = verts[1]->texcoord[1];
	float u3 = verts[2]->texcoord[0], v3 = verts[2]->texcoord[1];
	float w1 = verts[0]->coord[3], w2 = verts[1]->coord[3], w3 = verts[2]->coord[3];


	Interpolator i_color_r(fx1,fx2,fx3,fy1,fy2,fy3,r1,r2,r3);
	Interpolator i_color_g(fx1,fx2,fx3,fy1,fy2,fy3,g1,g2,g3);
	Interpolator i_color_b(fx1,fx2,fx3,fy1,fy2,fy3,b1,b2,b3);
	Interpolator i_tex_invu(fx1,fx2,fx3,fy1,fy2,fy3,u1,u2,u3);
	Interpolator i_tex_invv(fx1,fx2,fx3,fy1,fy2,fy3,v1,v2,v3);
	Interpolator i_z(fx1,fx2,fx3,fy1,fy2,fy3,fz1,fz2,fz3);
	Interpolator i_invw(fx1,fx2,fx3,fy1,fy2,fy3,1.0f/w1,1.0f/w2,1.0f/w3);
	

	i_color_r.init(minx,miny);
	i_color_g.init(minx,miny);
	i_color_b.init(minx,miny);
	i_tex_invu.init(minx,miny);
	i_tex_invv.init(minx,miny);
	i_z.init(minx,miny);
	i_invw.init(minx,miny);

    for(int y = miny; y < maxy; y++)
    {
		int CX1 = CY1;
        int CX2 = CY2;
        int CX3 = CY3;

		bool done = false;
		i_color_r.push(); i_color_g.push(); i_color_b.push();
		i_tex_invu.push(); i_tex_invv.push();
		i_invw.push(); i_z.push();

		assert(y>=0 && y<192); //I dont think we need this bounds check, so it is only here as an assert
		int adr = (y<<8)+minx;

		int xstart = -1, xend;

		//determine the current scanline, which will be in xstart <= x < xend
		for(int x = minx; x < maxx; x++, adr++)
		{
			if(CX1 > 0 && CX2 > 0 && CX3 > 0)
			{
				xend = x;
				if(!done)
					xstart = x;
				done = true;

			} else if(done) break;
		
			CX1 -= FDY12;
			CX2 -= FDY23;
			CX3 -= FDY31;
		}

		//---------
		//render the scanline
		if(xstart != -1)
		{
			//seed the interpolators with the distance to the start of this scanline
			//this is crappy and we need to redo it now that we've changing the main interpolation method to bilinear
			int xaccum = xstart-minx;
			i_color_r.incx(xaccum); i_color_g.incx(xaccum); i_color_b.incx(xaccum);
			i_tex_invu.incx(xaccum); i_tex_invv.incx(xaccum);
			i_invw.incx(xaccum); i_z.incx(xaccum);
			float rr = i_color_r.Z;
			float gg = i_color_g.Z;
			float bb = i_color_b.Z;
			float uu = i_tex_invu.Z;
			float vv = i_tex_invv.Z;
			float ww = i_invw.Z;
			float zz = i_z.Z;
			//now boost the interpolators to the end of this scanline
			i_color_r.pop();i_color_g.pop();i_color_b.pop();
			i_tex_invu.pop();i_tex_invv.pop();
			i_z.pop();i_invw.pop();
			xaccum = xend-minx;
			i_color_r.incx(xaccum); i_color_g.incx(xaccum); i_color_b.incx(xaccum);
			i_tex_invu.incx(xaccum); i_tex_invv.incx(xaccum);
			i_invw.incx(xaccum); i_z.incx(xaccum);
			scanline(y, xstart, xend, rr,gg,bb,uu,vv,ww,zz,i_color_r.Z, i_color_g.Z, i_color_b.Z, i_tex_invu.Z, i_tex_invv.Z, i_invw.Z, i_z.Z);
		}
		//----------

		i_color_r.pop(); i_color_r.incy();
		i_color_g.pop(); i_color_g.incy();
		i_color_b.pop(); i_color_b.incy();
		i_tex_invu.pop(); i_tex_invu.incy();
		i_tex_invv.pop(); i_tex_invv.incy();
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
	{
		for(int j=0;j<32;j++)
		{
			modulate_table[i][j] = ((i+1) * (j+1) - 1) >> 5;	
			for(int a=0;a<32;a++)
				decal_table[a][i][j] = ((i*a) + (j*(31-a))) >> 5;
		}
	}

	TexCache_Reset();
	TexCache_BindTexture = BindTexture;
	TexCache_BindTextureData = BindTextureData;

	printf("SoftRast Initialized\n");
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
	
	INTERP(coord[0]); INTERP(coord[1]); INTERP(coord[2]); INTERP(coord[3]);
	INTERP(texcoord[0]); INTERP(texcoord[1]);

	if(CommonSettings.HighResolutionInterpolateColor)
	{
		INTERP(fcolor[0]); INTERP(fcolor[1]); INTERP(fcolor[2]);
	}
	else
	{
		INTERP(color[0]); INTERP(color[1]); INTERP(color[2]);
		ret.color_to_float();
	}

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
	clearFragment.stencil = 0;
	for(int i=0;i<256*192;i++)
		screen[i] = clearFragment;

	//convert colors to float to get more precision in case we need it
	for(int i=0;i<gfx3d.vertlist->count;i++)
		gfx3d.vertlist->list[i].color_to_float();

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

			//perspective-correct the colors
			vert.fcolor[0] /= vert.coord[3];
			vert.fcolor[1] /= vert.coord[3];
			vert.fcolor[2] /= vert.coord[3];

			//viewport transformation
			vert.coord[0] *= gfx3d.viewport.width;
			vert.coord[0] += gfx3d.viewport.x;
			vert.coord[1] *= gfx3d.viewport.height;
			vert.coord[1] += gfx3d.viewport.y;
		}
	}

	//a counter for how many polys got culled
	int culled = 0;

	u32 lastTextureFormat = 0, lastTexturePalette = 0, lastPolyAttr = 0;
	
	//iterate over polys
	bool needInitTexture = true;
	polynum = 0;
	for(int i=0;i<clippedPolyCounter;i++)
	{
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

#ifdef USE_HECKER
			for(int i=0;i<type;i++)
				for(int j=0;j<2;j++)
					verts[i]->coord[j] = iround(16.0f * verts[i]->coord[j]);
#endif

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
				triangle_from_devmaster();polynum++;

				SubmitVertex(0,verts[2]);
				SubmitVertex(1,verts[3]);
				SubmitVertex(2,verts[0]);
				triangle_from_devmaster();polynum++;
			}
			else
			{
				SubmitVertex(0,verts[2]);
				SubmitVertex(1,verts[1]);
				SubmitVertex(2,verts[0]);
				triangle_from_devmaster();polynum++;

				SubmitVertex(0,verts[0]);
				SubmitVertex(1,verts[3]);
				SubmitVertex(2,verts[2]);
				triangle_from_devmaster();polynum++;
			}
		}
		else if(type == 3)
		{
			if(backfacing)
			{
				SubmitVertex(0,verts[0]);
				SubmitVertex(1,verts[1]);
				SubmitVertex(2,verts[2]);
				triangle_from_devmaster();polynum++;
			}
			else
			{
				SubmitVertex(0,verts[2]);
				SubmitVertex(1,verts[1]);
				SubmitVertex(2,verts[0]);
				triangle_from_devmaster();polynum++;
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
