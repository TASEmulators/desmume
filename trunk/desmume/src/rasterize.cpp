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
//
//the shape rasterizers contained herein are based on code supplied by Chris Hecker from 
//http://chrishecker.com/Miscellaneous_Technical_Articles

//The worst case we've managed to think of so far would be a viewport zoomed in a little bit 
//on a diamond, with cut-out bits in all four corners
#define MAX_CLIPPED_VERTS 8

//TODO - due to a late change of a y-coord flipping, our winding order is wrong
//this causes us to have to flip the verts for every front-facing poly.
//a performance improvement would be to change the winding order logic
//so that this is done less frequently

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

//#undef FORCEINLINE
//#define FORCEINLINE
//#undef INLINE
//#define INLINE

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
static u8 index_lookup_table[65];
static u8 index_start_table[8];

static struct TClippedPoly
{
	int type;
	POLY* poly;
	VERT clipVerts[MAX_CLIPPED_VERTS];
} *clippedPolys = NULL;
static int clippedPolyCounter;

TClippedPoly tempClippedPoly;
TClippedPoly outClippedPoly;

////optimized float floor useful in limited cases
////from http://www.stereopsis.com/FPU.html#convert
////(unfortunately, it relies on certain FPU register settings)
//int Real2Int(double val)
//{
//	const double _double2fixmagic = 68719476736.0*1.5;     //2^36 * 1.5,  (52-_shiftamt=36) uses limited precisicion to floor
//	const int _shiftamt        = 16;                    //16.16 fixed point representation,
//
//	#ifdef WORDS_BIGENDIAN
//		#define iman_				1
//	#else
//		#define iman_				0
//	#endif
//
//	val		= val + _double2fixmagic;
//	return ((int*)&val)[iman_] >> _shiftamt; 
//}

//// this probably relies on rounding settings..
//int Real2Int(float val)
//{
//	//val -= 0.5f;
//	//int temp;
//	//__asm {
//	//	fld val;
//	//	fistp temp;
//	//}
//	//return temp;
//	return 0;
//}


//doesnt work yet
static FORCEINLINE int fastFloor(float f)
{
	float temp = f + 1.f;
	int ret = (*((u32*)&temp))&0x7FFFFF;
	return ret;
}



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
	bool backfacing;

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

union FragmentColor {
	u32 color;
	struct {
#ifdef WORDS_BIGENDIAN
		u8 a,b,g,r;
#else
		u8 r,g,b,a;
#endif
	};
};

struct Fragment
{
	u32 depth;

	struct {
		u8 opaque, translucent;
	} polyid;

	u8 stencil;

	u8 pad;
};

static VERT* verts[MAX_CLIPPED_VERTS];

INLINE static void SubmitVertex(int vert_index, VERT& rawvert)
{
	verts[vert_index] = &rawvert;
}

static Fragment screen[256*192];
static FragmentColor screenColor[256*192];
static FragmentColor toonTable[32];


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

	FORCEINLINE FragmentColor sample(float u, float v)
	{
		//finally, we can use floor here. but, it is slower than we want.
		//the best solution is probably to wait until the pipeline is full of fixed point
		s32 iu = s32floor(u);
		s32 iv = s32floor(v);
		dowrap(iu,iv);

		FragmentColor color;
		color.color = ((u32*)textures.currentData)[(iv<<wshift)+iu];
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

	float invu, invv, w;
	FragmentColor materialColor;
	
	FORCEINLINE void shade(FragmentColor& dst)
	{
		FragmentColor texColor;
		float u,v;

		switch(mode)
		{
		case 0: //modulate
			u = invu*w;
			v = invv*w;
			texColor = sampler.sample(u,v);
			dst.r = modulate_table[texColor.r][materialColor.r];
			dst.g = modulate_table[texColor.g][materialColor.g];
			dst.b = modulate_table[texColor.b][materialColor.b];
			dst.a = modulate_table[texColor.a][materialColor.a];
			//dst.color.components.a = 31;
			//#ifdef _MSC_VER
			//if(GetAsyncKeyState(VK_SHIFT)) {
			//	//debugging tricks
			//	dst = materialColor;
			//	if(GetAsyncKeyState(VK_TAB)) {
			//		u8 alpha = dst.a;
			//		dst.color = polynum*8+8;
			//		dst.a = alpha;
			//	}
			//}
			//#endif
			break;
		case 1: //decal
			u = invu*w;
			v = invv*w;
			texColor = sampler.sample(u,v);
			dst.r = decal_table[texColor.a][texColor.r][materialColor.r];
			dst.g = decal_table[texColor.a][texColor.g][materialColor.g];
			dst.b = decal_table[texColor.a][texColor.b][materialColor.b];
			dst.a = materialColor.a;
			break;
		case 2: //toon/highlight shading
			{
				u = invu*w;
				v = invv*w;
				texColor = sampler.sample(u,v);
				if(texColor.r != 0x1f || texColor.g != 0x1f || texColor.b != 0x1f) {
					int zzz=9;
				}
				FragmentColor toonColor = toonTable[materialColor.r];
				if(sampler.texFormat == 0)
				{
					//if no texture is set then we dont need to modulate texture with toon 
					//but rather just use toon directly
					dst = toonColor;
					dst.a = materialColor.a;
				}
				else
				{
					if(gfx3d.shading == GFX3D::HIGHLIGHT)
					{
						dst.r = modulate_table[texColor.r][materialColor.r];
						dst.g = modulate_table[texColor.g][materialColor.r];
						dst.b = modulate_table[texColor.b][materialColor.r];
						dst.a = modulate_table[texColor.a][materialColor.a];

						dst.r = min<u8>(31, (dst.r + toonColor.r));
						dst.g = min<u8>(31, (dst.g + toonColor.g));
						dst.b = min<u8>(31, (dst.b + toonColor.b));
					}
					else
					{
						dst.r = modulate_table[texColor.r][toonColor.r];
						dst.g = modulate_table[texColor.g][toonColor.g];
						dst.b = modulate_table[texColor.b][toonColor.b];
						dst.a = modulate_table[texColor.a][materialColor.a];
					}
				}
			}
			break;
		case 3: //shadows
			//is this right? only with the material color?
			dst = materialColor;
			break;
		case 4: //our own special mode which only uses the material color (for when texturing is disabled)
			dst = materialColor;
			break;

		}
	}

} shader;

static FORCEINLINE void alphaBlend(FragmentColor & dst, const FragmentColor & src)
{
	if(gfx3d.enableAlphaBlending)
	{
		if(src.a == 0)
		{
			dst.a = max(src.a,dst.a);	
		}
		else if(src.a == 31 || dst.a == 0)
		{
			dst = src;
			dst.a = max(src.a,dst.a);
		}
		else
		{
			u8 alpha = src.a+1;
			u8 invAlpha = 32 - alpha;
			dst.r = (alpha*src.r + invAlpha*dst.r)>>5;
			dst.g = (alpha*src.g + invAlpha*dst.g)>>5;
			dst.b = (alpha*src.b + invAlpha*dst.b)>>5;
			dst.a = max(src.a,dst.a);
		}
	}
	else
	{
		if(src.a == 0)
		{
			//do nothing; the fragment is totally transparent
		}
		else
		{
			dst = src;
		}
	}
}

static FORCEINLINE void pixel(int adr,float r, float g, float b, float invu, float invv, float w, float z) {
	Fragment &destFragment = screen[adr];
	FragmentColor &destFragmentColor = screenColor[adr];

	//depth test
	u32 depth;
	if(gfx3d.wbuffer) {
		//not sure about this
		//this value was chosen to make the skybox, castle window decals, and water level render correctly in SM64
		depth = u32floor(4096*w);
	}
	else
	{
		depth = u32floor(z*0x7FFF);
		depth <<= 9;
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
	
	shader.w = w;
	shader.invu = invu;
	shader.invv = invv;

	//perspective-correct the colors
	r = (r * w) + 0.5f;
	g = (g * w) + 0.5f;
	b = (b * w) + 0.5f;

	//this is a HACK: 
	//we are being very sloppy with our interpolation precision right now
	//and rather than fix it, i just want to clamp it
	shader.materialColor.r = max(0U,min(31U,u32floor(r)));
	shader.materialColor.g = max(0U,min(31U,u32floor(g)));
	shader.materialColor.b = max(0U,min(31U,u32floor(b)));

	shader.materialColor.a = polyAttr.alpha;

	//pixel shader
	FragmentColor shaderOutput;
	shader.shade(shaderOutput);

	//alpha test
	if(gfx3d.enableAlphaTest)
	{
		if(shaderOutput.a < gfx3d.alphaTestRef)
			goto rejected_fragment;
	}

	//we shouldnt do any of this if we generated a totally transparent pixel
	if(shaderOutput.a != 0)
	{
		//handle shadow polys
		if(shader.mode == 3)
		{
			if(polyAttr.polyid == 0)
			{
				destFragment.stencil = 1;
				goto rejected_fragment;
			}
			else
			{
				//some shadow volumes are rendering two-sided
				//and this is the only way to fix them.
				//rendering two-sided makes no sense, since the back side
				//will clear the stencil bit and cause the shadow to get drawn in
				//empty space.
				//but yet, it is done, and gbatek even suggests that we should do it
				if(polyAttr.backfacing)
					goto rejected_fragment;

				//now, if we arent supposed to draw shadow here, then bail out
				if(destFragment.stencil == 1)
				{
					destFragment.stencil = 0;
					goto rejected_fragment;
				}

				destFragment.stencil = 0;
			}

			//reset the shadow flag to keep the shadow from getting drawn more than once
			//if(polyAttr.polyid == 0)
			//	destFragment.stencil = 1;
		}

		//handle polyids
		bool isOpaquePixel = shaderOutput.a == 31;
		if(isOpaquePixel)
		{
			destFragment.polyid.opaque = polyAttr.polyid;
		}
		else
		{

			//shadow polys have a special check here to keep from self-shadowing when user
			//has tried to prevent it from happening
			//if this isnt here, then the vehicle select in mariokart will look terrible
			if(shader.mode == 3)
			{
				if(destFragment.polyid.opaque == polyAttr.polyid)
					goto rejected_fragment;
			}
		
			//dont overwrite pixels on translucent polys with the same polyids
			if(destFragment.polyid.translucent == polyAttr.polyid)
				goto rejected_fragment;
		
			//originally we were using a test case of shadows-behind-trees in sm64ds
			//but, it looks bad in that game. this is actually correct
			//if this isnt correct, then complex shape cart shadows in mario kart don't work right
			destFragment.polyid.translucent = polyAttr.polyid;
		}

		//alpha blending and write color
		alphaBlend(destFragmentColor, shaderOutput);

		//depth writing
		if(isOpaquePixel || polyAttr.translucentDepthWrite)
			destFragment.depth = depth;

	}

	goto done_with_pixel;

	depth_fail:
	//handle stencil-writing in shadow mode
/*	if((shader.mode == 3) && (polyAttr.polyid == 0))
	{
		destFragment.stencil = 1;
	}*/

	rejected_fragment:
	done_with_pixel:
	;
}


typedef int fixed28_4;

static bool failure;

// handle floor divides and mods correctly 
INLINE void FloorDivMod(long Numerator, long Denominator, long &Floor, long &Mod)
{
	//These must be caused by invalid or degenerate shapes.. not sure yet.
	//check it out in the mario face intro of SM64
	//so, we have to take out the assert.
	//I do know that we handle SOME invalid shapes without crashing,
	//since I see them acting poppy in a way that doesnt happen in the HW.. so alas it is also incorrect.
	//This particular incorrectness is not likely ever to get fixed!

	//assert(Denominator > 0);		

	//but we have to bail out since our handling for these cases currently steps scanlines 
	//the wrong way and goes totally nuts (freezes)
	if(Denominator<=0) 
		failure = true;

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

INLINE fixed28_4 FloatToFixed28_4( float Value ) {
	return (fixed28_4)(Value * 16);
}
INLINE float Fixed28_4ToFloat( fixed28_4 Value ) {
	return Value / 16.0;
}
//inline fixed16_16 FloatToFixed16_16( float Value ) {
//	return (fixed16_6)(Value * 65536);
//}
//inline float Fixed16_16ToFloat( fixed16_16 Value ) {
//	return Value / 65536.0;
//}
INLINE fixed28_4 Fixed28_4Mul( fixed28_4 A, fixed28_4 B ) {
	// could make this asm to prevent overflow
	return (A * B) / 16;	// 28.4 * 28.4 = 24.8 / 16 = 28.4
}
INLINE int Ceil28_4( fixed28_4 Value ) {
	int ReturnValue;
	int Numerator = Value - 1 + 16;
	if(Numerator >= 0) {
		ReturnValue = Numerator/16;
	} else {
		// deal with negative numerators correctly
		ReturnValue = -((-Numerator)/16);
		ReturnValue -= ((-Numerator) % 16) ? 1 : 0;
	}
	return ReturnValue;
}

struct edge_fx_fl {
	edge_fx_fl() {}
	edge_fx_fl(int Top, int Bottom);
	FORCEINLINE int Step();

	long X, XStep, Numerator, Denominator;			// DDA info for x
	long ErrorTerm;
	int Y, Height;					// current y and vertical count
	
	struct Interpolant {
		float curr, step, stepExtra;
		FORCEINLINE void doStep() { curr += step; }
		FORCEINLINE void doStepExtra() { curr += stepExtra; }
		void initialize(float top, float bottom, float dx, float dy, long XStep, float XPrestep, float YPrestep) {
			dx = 0;
			dy *= (bottom-top);
			curr = top + YPrestep * dy + XPrestep * dx;
			step = XStep * dx + dy;
			stepExtra = dx;
		}
	};
	
	static const int NUM_INTERPOLANTS = 7;
	union {
		struct {
			Interpolant invw,z,u,v,color[3];
		};
		Interpolant interpolants[NUM_INTERPOLANTS];
	};
	void FORCEINLINE doStepInterpolants() { for(int i=0;i<NUM_INTERPOLANTS;i++) interpolants[i].doStep(); }
	void FORCEINLINE doStepExtraInterpolants() { for(int i=0;i<NUM_INTERPOLANTS;i++) interpolants[i].doStepExtra(); }
};

FORCEINLINE edge_fx_fl::edge_fx_fl(int Top, int Bottom) {
	Y = Ceil28_4(verts[Top]->y);
	int YEnd = Ceil28_4(verts[Bottom]->y);
	Height = YEnd - Y;

	if(Height)
	{
		long dN = long(verts[Bottom]->y - verts[Top]->y);
		long dM = long(verts[Bottom]->x - verts[Top]->x);
	
		long InitialNumerator = dM*16*Y - dM*verts[Top]->y + dN*verts[Top]->x - 1 + dN*16;
		FloorDivMod(InitialNumerator,dN*16,X,ErrorTerm);
		FloorDivMod(dM*16,dN*16,XStep,Numerator);
		Denominator = dN*16;
	
		float YPrestep = Fixed28_4ToFloat(Y*16 - verts[Top]->y);
		float XPrestep = Fixed28_4ToFloat(X*16 - verts[Top]->x);

		float dy = 1/Fixed28_4ToFloat(dN);
		float dx = 1/Fixed28_4ToFloat(dM);
		
		invw.initialize(1/verts[Top]->w,1/verts[Bottom]->w,dx,dy,XStep,XPrestep,YPrestep);
		u.initialize(verts[Top]->u,verts[Bottom]->u,dx,dy,XStep,XPrestep,YPrestep);
		v.initialize(verts[Top]->v,verts[Bottom]->v,dx,dy,XStep,XPrestep,YPrestep);
		z.initialize(verts[Top]->z,verts[Bottom]->z,dx,dy,XStep,XPrestep,YPrestep);
		for(int i=0;i<3;i++)
			color[i].initialize(verts[Top]->fcolor[i],verts[Bottom]->fcolor[i],dx,dy,XStep,XPrestep,YPrestep);
	}
}

FORCEINLINE int edge_fx_fl::Step() {
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

//draws a single scanline
static void drawscanline(edge_fx_fl *pLeft, edge_fx_fl *pRight)
{
	int XStart = pLeft->X;
	int width = pRight->X - XStart;

	//these are the starting values, taken from the left edge
	float invw = pLeft->invw.curr;
	float u = pLeft->u.curr;
	float v = pLeft->v.curr;
	float z = pLeft->z.curr;
	float color[3] = {
		pLeft->color[0].curr,
		pLeft->color[1].curr,
		pLeft->color[2].curr };

	//our dx values are taken from the steps up until the right edge
	float invWidth = 1.0f / width;
	float dinvw_dx = (pRight->invw.curr - invw) * invWidth;
	float du_dx = (pRight->u.curr - u) * invWidth;
	float dv_dx = (pRight->v.curr - v) * invWidth;
	float dz_dx = (pRight->z.curr - z) * invWidth;
	float dc_dx[3] = {
		(pRight->color[0].curr - color[0]) * invWidth,
		(pRight->color[1].curr - color[1]) * invWidth,
		(pRight->color[2].curr - color[2]) * invWidth };

	int adr = (pLeft->Y<<8)+XStart;

	//CONSIDER: in case some other math is wrong (shouldve been clipped OK), we might go out of bounds here.
	//better check the Y value.
	if(pLeft->Y<0 || pLeft->Y>191) {
		printf("rasterizer rendering at y=%d! oops!\n",pLeft->Y);
		return;
	}

	int x = XStart;

	while(width-- > 0)
	{
		if(x<0 || x>255) {
			printf("rasterizer rendering at x=%d! oops!\n",x);
			return;
		}
		pixel(adr,color[0],color[1],color[2],u,v,1.0f/invw,z);
		adr++;
		x++;

		invw += dinvw_dx;
		u += du_dx;
		v += dv_dx;
		z += dz_dx;
		color[0] += dc_dx[0];
		color[1] += dc_dx[1];
		color[2] += dc_dx[2];
	}
}

//runs several scanlines, until an edge is finished
static void runscanlines(edge_fx_fl *left, edge_fx_fl *right)
{
	//do not overstep either of the edges
	int Height = min(left->Height,right->Height);
	while(Height--) {
		drawscanline(left,right);
		left->Step(); 
		right->Step();
	}
}

//rotates verts counterclockwise
template<int type>
INLINE static void rot_verts() {
	#define ROTSWAP(X) if(type>X) swap(verts[X-1],verts[X]);
	ROTSWAP(1); ROTSWAP(2); ROTSWAP(3); ROTSWAP(4);
	ROTSWAP(5); ROTSWAP(6); ROTSWAP(7);
}

//rotate verts until vert0.y is minimum, and then vert0.x is minimum in case of ties
//this is a necessary precondition for our shape engine
template<int type>
static void sort_verts(bool backwards) {
	//if the verts are backwards, reorder them first
	if(backwards)
		for(int i=0;i<type/2;i++)
			swap(verts[i],verts[type-i-1]);

	for(;;)
	{
		//this was the only way we could get this to unroll
		#define CHECKY(X) if(type>X) if(verts[0]->y > verts[X]->y) goto doswap;
		CHECKY(1); CHECKY(2); CHECKY(3); CHECKY(4);
		CHECKY(5); CHECKY(6); CHECKY(7);
		break;
		
	doswap:
		rot_verts<type>();
	}
	
	while(verts[0]->y == verts[1]->y && verts[0]->x > verts[1]->x)
		rot_verts<type>();
	
}

//This function can handle any convex N-gon up to octagons
//verts must be clockwise.
//I didnt reference anything for this algorithm but it seems like I've seen it somewhere before.
static void shape_engine(int type, bool backwards)
{
	failure = false;

	switch(type) {
		case 3: sort_verts<3>(backwards); break;
		case 4: sort_verts<4>(backwards); break;
		case 5: sort_verts<5>(backwards); break;
		case 6: sort_verts<6>(backwards); break;
		case 7: sort_verts<7>(backwards); break;
		case 8: sort_verts<8>(backwards); break;
		default: printf("skipping type %d\n",type); return;
	}
	
	//we are going to step around the polygon in both directions starting from vert 0.
	//right edges will be stepped over clockwise and left edges stepped over counterclockwise.
	//these variables track that stepping, but in order to facilitate wrapping we start extra high
	//for the counter we're decrementing.
	int lv = type, rv = 0;

	edge_fx_fl left, right;
	bool step_left = true, step_right = true;
	for(;;) {
		//generate new edges if necessary. we must avoid regenerating edges when they are incomplete
		//so that they can be continued on down the shape
		assert(rv != type);
		int _lv = lv==type?0:lv; //make sure that we ask for vert 0 when the variable contains the starting value
		if(step_left) left = edge_fx_fl(_lv,lv-1);
		if(step_right) right = edge_fx_fl(rv,rv+1);
		step_left = step_right = false;

		//handle a failure in the edge setup due to nutty polys
		if(failure) 
			return;

		

		runscanlines(&left,&right);
		
		//if we ran out of an edge, step to the next one
		if(right.Height == 0) {
			step_right = true;
			rv++;
		} 
		if(left.Height == 0) {
			step_left = true;
			lv--;
		}

		//this is our completion condition: when our stepped edges meet in the middle
		if(lv<=rv+1) break;
	}

}

static char SoftRastInit(void)
{
	static bool tables_generated = false;
	if(!tables_generated)
	{
		tables_generated = true;

		clippedPolys = new TClippedPoly[POLYLIST_SIZE*2];

		for(int i=0;i<32;i++)
		{
			for(int j=0;j<32;j++)
			{
				modulate_table[i][j] = ((i+1) * (j+1) - 1) >> 5;	
				for(int a=0;a<32;a++)
					decal_table[a][i][j] = ((i*a) + (j*(31-a))) >> 5;
			}
		}

		//these tables are used to increment through vert lists without having to do wrapping logic/math
		int idx=0;
		for(int i=3;i<=8;i++)
		{
			index_start_table[i-3] = idx;
			for(int j=0;j<i;j++) {
				int a = j;
				int b = j+1;
				if(b==i) b = 0;
				index_lookup_table[idx++] = a;
				index_lookup_table[idx++] = b;
			}
		}
	}

	TexCache_Reset();
	TexCache_BindTexture = BindTexture;
	TexCache_BindTextureData = BindTextureData;

	printf("SoftRast Initialized\n");
	return 1;
}

static void SoftRastReset() {
}

static void SoftRastClose()
{
}

static void SoftRastVramReconfigureSignal() {
	TexCache_Invalidate();
}

static void SoftRastConvertFramebuffer()
{
	FragmentColor* src = screenColor;
	u16* dst = gfx3d_convertedScreen;
	u8* dstAlpha = gfx3d_convertedAlpha;

	//in an effort to speed this up, the misc pixel buffers and the color buffer were separated.

	for(int i=0,y=0;y<192;y++)
	{
		#ifndef NOSSE2
			u8* wanx = (u8*)&src[i];
			#define ASS(X,Y) __asm { prefetchnta [wanx+32*0x##X##Y] }
			#define PUNK(X) ASS(X,0) ASS(X,1) ASS(X,2) ASS(X,3) ASS(X,4) ASS(X,5) ASS(X,6) ASS(X,7) ASS(X,8) ASS(X,9) ASS(X,A) ASS(X,B) ASS(X,C) ASS(X,D) ASS(X,E) ASS(X,F) 
			PUNK(0); PUNK(1);
		#endif

		for(int x=0;x<256;x++,i++)
		{
			const u8 r = src[i].r;
			const u8 g = src[i].g;
			const u8 b = src[i].b;
			const u8 a = src[i].a;
			dst[i] = R5G5B5TORGB15(r,g,b) | alpha_lookup[a];
			dstAlpha[i] = alpha_5bit_to_4bit[a];
		}
	}
}


template<typename T>
static T interpolate(const float ratio, const T& x0, const T& x1) {
	return x0 + (float)(x1-x0) * (ratio);
}


//http://www.cs.berkeley.edu/~ug/slide/pipeline/assignments/as6/discussion.shtml
static FORCEINLINE VERT clipPoint(VERT* inside, VERT* outside, int coord, int which)
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

static FORCEINLINE void clipSegmentVsPlane(VERT** verts, const int coord, int which)
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

static FORCEINLINE void clipPolyVsPlane(const int coord, int which)
{
	outClippedPoly.type = 0;
	CLIPLOG2("Clipping coord %d against %f\n",coord,x);
	for(int i=0;i<tempClippedPoly.type;i++)
	{
		VERT* testverts[2] = {&tempClippedPoly.clipVerts[i],&tempClippedPoly.clipVerts[(i+1)%tempClippedPoly.type]};
		clipSegmentVsPlane(testverts, coord, which);
	}

	//this doesnt seem to help any. leaving it until i decide to get rid of it
	//int j = index_start_table[tempClippedPoly.type-3];
	//for(int i=0;i<tempClippedPoly.type;i++,j+=2)
	//{
	//	VERT* testverts[2] = {&tempClippedPoly.clipVerts[index_lookup_table[j]],&tempClippedPoly.clipVerts[index_lookup_table[j+1]]};
	//	clipSegmentVsPlane(testverts, coord, which);
	//}

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
	else
	{
		//TODO - build right in this list instead of copying
		clippedPolys[clippedPolyCounter] = tempClippedPoly;
		clippedPolys[clippedPolyCounter].poly = poly;
		clippedPolyCounter++;
	}

}

static void SoftRastRender()
{
	Fragment clearFragment;
	FragmentColor clearFragmentColor;
	clearFragment.pad = 0;
	clearFragmentColor.r = gfx3d.clearColor&0x1F;
	clearFragmentColor.g = (gfx3d.clearColor>>5)&0x1F;
	clearFragmentColor.b = (gfx3d.clearColor>>10)&0x1F;
	clearFragmentColor.a = (gfx3d.clearColor>>16)&0x1F;
	clearFragment.polyid.opaque = (gfx3d.clearColor>>24)&0x3F;
	//special value for uninitialized translucent polyid. without this, fires in spiderman2 dont display
	//I am not sure whether it is right, though. previously this was cleared to 0, as a guess,
	//but in spiderman2 some fires with polyid 0 try to render on top of the background
	clearFragment.polyid.translucent = 255; 
	clearFragment.depth = gfx3d.clearDepth;
	clearFragment.stencil = 0;
	for(int i=0;i<256*192;i++)
		screen[i] = clearFragment;

	if(gfx3d.enableClearImage)
	{
		u16* clearImage = (u16*)ARM9Mem.texInfo.textureSlotAddr[2];
		u16* clearDepth = (u16*)ARM9Mem.texInfo.textureSlotAddr[3];

		//not emulated until we actually have a test case
		//(since in the meantime it would just slow us down)
		//(and when we find a case, it will be obvious that it isnt scrolling, instead of possibly obscured by a bug)
		u16 scroll = T1ReadWord(ARM9Mem.ARM9_REG,0x356); //CLRIMAGE_OFFSET
		u16 xscroll = scroll&0xFF;
		u16 yscroll = (scroll>>8)&0xFF;

		FragmentColor *dstColor = screenColor;
		Fragment *dst = screen;

		for(int y=0;y<192;y++)
			for(int x=0;x<256;x++) {
				
				//this hasnt been tested
				//TODO - dont do this if we are mapped to blank memory (such as in sonic chronicles)
				//(or use a special zero fill in the bulk clearing above)
				u16 col = *clearImage;
				dstColor->color = RGB15TO32(255*(col>>15),col);
				
				//this is tested quite well in the sonic chronicles main map mode
				//where depth values are used for trees etc you can walk behind
				u32 depth = *clearDepth;
				//masking off fog for now
				depth &= 0x7FFF;
				//TODO - might consider a lookup table for this
				dst->depth = gfx3d_extendDepth_15_to_24(depth);

				clearDepth++;
				clearImage++;
				dstColor++;
				dst++;
			}
	}
	else 
		for(int i=0;i<256*192;i++)
			screenColor[i] = clearFragmentColor;

	//convert the toon colors
	//TODO for a slight speedup this could be cached in gfx3d (oglrenderer could benefit as well)
	for(int i=0;i<32;i++) {
		toonTable[i].r = gfx3d.u16ToonTable[i]&0x1F;
		toonTable[i].g = (gfx3d.u16ToonTable[i]>>5)&0x1F;
		toonTable[i].b = (gfx3d.u16ToonTable[i]>>10)&0x1F;
	}

	//convert colors to float to get more precision in case we need it
	for(int i=0;i<gfx3d.vertlist->count;i++)
		gfx3d.vertlist->list[i].color_to_float();

	//submit all polys to clipper
	clippedPolyCounter = 0;
	for(int i=0;i<gfx3d.polylist->count;i++)
	{
		clipPoly(&gfx3d.polylist->list[gfx3d.indexlist[i]]);
	}

	//printf("%d %d %d %d\n",gfx3d.viewport.x,gfx3d.viewport.y,gfx3d.viewport.width,gfx3d.viewport.height);

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
			VIEWPORT viewport;
			viewport.decode(poly.poly->viewport);
			vert.coord[0] *= viewport.width;
			vert.coord[0] += viewport.x;
			vert.coord[1] *= viewport.height;
			vert.coord[1] += viewport.y;
			vert.coord[1] = 192 - vert.coord[1];

			//well, i guess we need to do this to keep Princess Debut from rendering huge polys.
			//there must be something strange going on
			vert.coord[0] = max(0.0f,min(256.0f,vert.coord[0]));
			vert.coord[1] = max(0.0f,min(192.0f,vert.coord[1]));
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

		VERT* verts = &clippedPoly.clipVerts[0];

		if(i == 0 || lastPolyAttr != poly->polyAttr)
		{
			polyAttr.setup(poly->polyAttr);
			lastPolyAttr = poly->polyAttr;
		}

		//HACK: backface culling
		//this should be moved to gfx3d, but first we need to redo the way the lists are built
		//because it is too convoluted right now.
		//(must we throw out verts if a poly gets backface culled? if not, then it might be easier)
		//TODO - is this good enough for quads and other shapes? we think so.
		float ab[2], ac[2];
		Vector2Copy(ab, verts[1].coord);
		Vector2Copy(ac, verts[2].coord);
		Vector2Subtract(ab, verts[0].coord);
		Vector2Subtract(ac, verts[0].coord);
		float cross = Vector2Cross(ab, ac);
		polyAttr.backfacing = (cross>0);

		if(!polyAttr.isVisible(polyAttr.backfacing)) {
			culled++;
			continue;
		}
	
		if(needInitTexture || lastTextureFormat != poly->texParam || lastTexturePalette != poly->texPalette)
		{
			TexCache_SetTexture<TexFormat_15bpp>(poly->texParam,poly->texPalette);
			sampler.setup(poly->texParam);
			lastTextureFormat = poly->texParam;
			lastTexturePalette = poly->texPalette;
			needInitTexture = false;
		}

		//here is a hack which needs to be removed.
		//at some point our shape engine needs these to be converted to "fixed point"
		//which is currently just a float
		for(int j=0;j<type;j++)
			for(int k=0;k<2;k++)
				verts[j].coord[k] = iround(16.0f * verts[j].coord[k]);

		//hmm... shader gets setup every time because it depends on sampler which may have just changed
		shader.setup(poly->polyAttr);

		for(int j=0;j<MAX_CLIPPED_VERTS;j++)
			::verts[j] = &verts[j];

		shape_engine(type,!polyAttr.backfacing);
	}

	//	printf("rendered %d of %d polys after backface culling\n",gfx3d.polylist->count-culled,gfx3d.polylist->count);
	SoftRastConvertFramebuffer();
}

GPU3DInterface gpu3DRasterize = {
	"SoftRasterizer",
	SoftRastInit,
	SoftRastReset,
	SoftRastClose,
	SoftRastRender,
	SoftRastVramReconfigureSignal,
};
