/*  Copyright 2009 DeSmuME team

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
#include "utils/task.h"

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

static const int kUnsetTranslucentPolyID = 255;

static u8 modulate_table[64][64];
static u8 decal_table[32][64][64];
static u8 index_lookup_table[65];
static u8 index_start_table[8];

static GFX3D_Clipper clipper;
static GFX3D_Clipper::TClippedPoly *clippedPolys = NULL;
static TexCacheItem* polyTexKeys[POLYLIST_SIZE];
static bool polyVisible[POLYLIST_SIZE];
static bool polyBackfacing[POLYLIST_SIZE];
static int clippedPolyCounter;



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




union FragmentColor {
	u32 color;
	struct {
		u8 r,g,b,a;
	};
};

struct Fragment
{
	u32 depth;

	struct {
		u8 opaque, translucent;
	} polyid;

	u8 stencil;

	struct {
		u8 isTranslucentPoly:1;
		u8 fogged:1;
	};
};

//INLINE static void SubmitVertex(int vert_index, VERT& rawvert)
//{
//	verts[vert_index] = &rawvert;
//}

static Fragment screen[256*192];
static FragmentColor screenColor[256*192];
static FragmentColor toonTable[32];
static u8 fogTable[32768];

FORCEINLINE int iround(float f) {
	return (int)f; //lol
}


typedef int fixed28_4;

// handle floor divides and mods correctly 
FORCEINLINE void FloorDivMod(long Numerator, long Denominator, long &Floor, long &Mod, bool& failure)
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

FORCEINLINE fixed28_4 FloatToFixed28_4( float Value ) {
	return (fixed28_4)(Value * 16);
}
FORCEINLINE float Fixed28_4ToFloat( fixed28_4 Value ) {
	return Value / 16.0f;
}
//inline fixed16_16 FloatToFixed16_16( float Value ) {
//	return (fixed16_6)(Value * 65536);
//}
//inline float Fixed16_16ToFloat( fixed16_16 Value ) {
//	return Value / 65536.0;
//}
FORCEINLINE fixed28_4 Fixed28_4Mul( fixed28_4 A, fixed28_4 B ) {
	// could make this asm to prevent overflow
	return (A * B) / 16;	// 28.4 * 28.4 = 24.8 / 16 = 28.4
}
FORCEINLINE int Ceil28_4( fixed28_4 Value ) {
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
	edge_fx_fl(int Top, int Bottom, VERT** verts, bool& failure);
	FORCEINLINE int Step();

	VERT** verts;
	long X, XStep, Numerator, Denominator;			// DDA info for x
	long ErrorTerm;
	int Y, Height;					// current y and vertical count
	
	struct Interpolant {
		float curr, step, stepExtra;
		FORCEINLINE void doStep() { curr += step; }
		FORCEINLINE void doStepExtra() { curr += stepExtra; }
		FORCEINLINE void initialize(float top, float bottom, float dx, float dy, long XStep, float XPrestep, float YPrestep) {
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

FORCEINLINE edge_fx_fl::edge_fx_fl(int Top, int Bottom, VERT** verts, bool& failure) {
	this->verts = verts;
	Y = Ceil28_4((fixed28_4)verts[Top]->y);
	int YEnd = Ceil28_4((fixed28_4)verts[Bottom]->y);
	Height = YEnd - Y;

	if(Height)
	{
		long dN = long(verts[Bottom]->y - verts[Top]->y);
		long dM = long(verts[Bottom]->x - verts[Top]->x);
	
		long InitialNumerator = (long)(dM*16*Y - dM*verts[Top]->y + dN*verts[Top]->x - 1 + dN*16);
		FloorDivMod(InitialNumerator,dN*16,X,ErrorTerm,failure);
		FloorDivMod(dM*16,dN*16,XStep,Numerator,failure);
		Denominator = dN*16;
	
		float YPrestep = Fixed28_4ToFloat((fixed28_4)(Y*16 - verts[Top]->y));
		float XPrestep = Fixed28_4ToFloat((fixed28_4)(X*16 - verts[Top]->x));

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



static FORCEINLINE void alphaBlend(FragmentColor & dst, const FragmentColor & src)
{
	if(gfx3d.enableAlphaBlending)
	{
		if(src.a == 0 || dst.a == 0)
		{
			dst = src;
		}
		else
		{
			u8 alpha = src.a+1;
			u8 invAlpha = 32 - alpha;
			dst.r = (alpha*src.r + invAlpha*dst.r)>>5;
			dst.g = (alpha*src.g + invAlpha*dst.g)>>5;
			dst.b = (alpha*src.b + invAlpha*dst.b)>>5;
		}

		dst.a = max(src.a,dst.a);
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



class RasterizerUnit
{
public:

	int SLI_MASK, SLI_VALUE;

	RasterizerUnit()
		: sampler()
		, shader(sampler)
	{
		sampler.unit = this;
	}

	TexCacheItem* lastTexKey;
	
	VERT* verts[MAX_CLIPPED_VERTS];

	struct PolyAttr
	{
		u32 val;

		bool decalMode;
		bool translucentDepthWrite;
		bool drawBackPlaneIntersectingPolys;
		u8 polyid;
		u8 alpha;
		bool backfacing;
		bool translucent;
		u8 fogged;

		bool isVisible(bool backfacing) 
		{
			//this was added after adding multi-bit stencil buffer
			//it seems that we also need to prevent drawing back faces of shadow polys for rendering
			u32 mode = (val>>4)&0x3;
			if(mode==3 && polyid !=0) return !backfacing;
			//another reasonable possibility is that we should be forcing back faces to draw (mariokart doesnt use them)
			//and then only using a single bit buffer (but a cursory test of this doesnt actually work)

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
			fogged = BIT15(val);
		}

	} polyAttr;


	struct Sampler
	{
		Sampler() {}

		RasterizerUnit* unit;
			
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
			color.color = ((u32*)unit->lastTexKey->decoded)[(iv<<wshift)+iu];
			return color;
		}

	} sampler;

	struct Shader
	{
		Shader(Sampler& _sampler)
			:sampler(_sampler)
		{}
		Sampler& sampler;
		u8 mode;
		void setup(u32 polyattr)
		{
			mode = (polyattr>>4)&0x3;
			//if there is no texture set, then set to the mode which doesnt even use a texture
			//(no texture makes sense for toon/highlight mode)
			if(sampler.texFormat == 0 && (mode == 0 || mode == 1))
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
				dst.a = modulate_table[GFX3D_5TO6(texColor.a)][GFX3D_5TO6(materialColor.a)]>>1;
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
					FragmentColor toonColor = toonTable[materialColor.r>>1];
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
							dst.a = modulate_table[GFX3D_5TO6(texColor.a)][GFX3D_5TO6(materialColor.a)]>>1;

							dst.r = min<u8>(63, (dst.r + toonColor.r));
							dst.g = min<u8>(63, (dst.g + toonColor.g));
							dst.b = min<u8>(63, (dst.b + toonColor.b));
						}
						else
						{
							dst.r = modulate_table[texColor.r][toonColor.r];
							dst.g = modulate_table[texColor.g][toonColor.g];
							dst.b = modulate_table[texColor.b][toonColor.b];
							dst.a = modulate_table[GFX3D_5TO6(texColor.a)][GFX3D_5TO6(materialColor.a)]>>1;
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

	FORCEINLINE void pixel(int adr,float r, float g, float b, float invu, float invv, float w, float z)
	{
		Fragment &destFragment = screen[adr];
		FragmentColor &destFragmentColor = screenColor[adr];

		u32 depth;
		if(gfx3d.wbuffer)
		{
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

		//handle shadow polys
		if(shader.mode == 3)
		{
			if(polyAttr.polyid == 0)
			{
				//that's right! stencil buffers, despite reports to the contrary, must be more than 1 bit
				//this is necessary to make these cases work all at once.
				//1. sm64 (standing near signs and blocks)
				//2. mariokart (no glitches in shadow shape in kart selector)
				//3. mariokart (no junk beneath platform in kart selector / no shadow beneath grate floor in bowser stage)
				//(specifically, the shadows in mario kart are complicated concave shapes)
				destFragment.stencil++;
				goto rejected_fragment;
			}
			else
			{
				if(destFragment.stencil)
				{
					destFragment.stencil--;
					goto rejected_fragment;
				}	

				//shadow polys have a special check here to keep from self-shadowing when user
				//has tried to prevent it from happening
				//if this isnt here, then the vehicle select in mariokart will look terrible
				if(destFragment.polyid.opaque == polyAttr.polyid)
					goto rejected_fragment;
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
		shader.materialColor.r = max(0U,min(63U,u32floor(r)));
		shader.materialColor.g = max(0U,min(63U,u32floor(g)));
		shader.materialColor.b = max(0U,min(63U,u32floor(b)));

		shader.materialColor.a = polyAttr.alpha;

		//pixel shader
		FragmentColor shaderOutput;
		shader.shade(shaderOutput);

		//we shouldnt do any of this if we generated a totally transparent pixel
		if(shaderOutput.a != 0)
		{
			//alpha test (don't have any test cases for this...? is it in the right place...?)
			if(gfx3d.enableAlphaTest)
			{
				if(shaderOutput.a < gfx3d.alphaTestRef)
					goto rejected_fragment;
			}

			//handle polyids
			bool isOpaquePixel = shaderOutput.a == 31;
			if(isOpaquePixel)
			{
				destFragment.polyid.opaque = polyAttr.polyid;
				destFragment.isTranslucentPoly = polyAttr.translucent?1:0;
				destFragment.fogged = polyAttr.fogged;
				destFragmentColor = shaderOutput;
			}
			else
			{
				//dont overwrite pixels on translucent polys with the same polyids
				if(destFragment.polyid.translucent == polyAttr.polyid)
					goto rejected_fragment;
			
				//originally we were using a test case of shadows-behind-trees in sm64ds
				//but, it looks bad in that game. this is actually correct
				//if this isnt correct, then complex shape cart shadows in mario kart don't work right
				destFragment.polyid.translucent = polyAttr.polyid;

				//alpha blending and write color
				alphaBlend(destFragmentColor, shaderOutput);

				destFragment.fogged &= polyAttr.fogged;
			}

			//depth writing
			if(isOpaquePixel || polyAttr.translucentDepthWrite)
				destFragment.depth = depth;

		}

		depth_fail:
		rejected_fragment:
		;
	}

	//draws a single scanline
	FORCEINLINE void drawscanline(edge_fx_fl *pLeft, edge_fx_fl *pRight)
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
	template<bool SLI>
	void runscanlines(edge_fx_fl *left, edge_fx_fl *right)
	{
		//do not overstep either of the edges
		int Height = min(left->Height,right->Height);
		while(Height--) {
			if(!SLI || (left->Y & SLI_MASK) == SLI_VALUE)
				drawscanline(left,right);
			left->Step(); 
			right->Step();
		}
	}

	
	//rotates verts counterclockwise
	template<int type>
	INLINE void rot_verts() {
		#define ROTSWAP(X) if(type>X) swap(verts[X-1],verts[X]);
		ROTSWAP(1); ROTSWAP(2); ROTSWAP(3); ROTSWAP(4);
		ROTSWAP(5); ROTSWAP(6); ROTSWAP(7); ROTSWAP(8); ROTSWAP(9);
	}

	//rotate verts until vert0.y is minimum, and then vert0.x is minimum in case of ties
	//this is a necessary precondition for our shape engine
	template<int type>
	void sort_verts(bool backwards) {
		//if the verts are backwards, reorder them first
		if(backwards)
			for(int i=0;i<type/2;i++)
				swap(verts[i],verts[type-i-1]);

		for(;;)
		{
			//this was the only way we could get this to unroll
			#define CHECKY(X) if(type>X) if(verts[0]->y > verts[X]->y) goto doswap;
			CHECKY(1); CHECKY(2); CHECKY(3); CHECKY(4);
			CHECKY(5); CHECKY(6); CHECKY(7); CHECKY(8); CHECKY(9);
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
	template<bool SLI>
	void shape_engine(int type, bool backwards)
	{
		bool failure = false;

		switch(type) {
			case 3: sort_verts<3>(backwards); break;
			case 4: sort_verts<4>(backwards); break;
			case 5: sort_verts<5>(backwards); break;
			case 6: sort_verts<6>(backwards); break;
			case 7: sort_verts<7>(backwards); break;
			case 8: sort_verts<8>(backwards); break;
			case 9: sort_verts<9>(backwards); break;
			case 10: sort_verts<10>(backwards); break;
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
			if(step_left) left = edge_fx_fl(_lv,lv-1,(VERT**)&verts, failure);
			if(step_right) right = edge_fx_fl(rv,rv+1,(VERT**)&verts, failure);
			step_left = step_right = false;

			//handle a failure in the edge setup due to nutty polys
			if(failure) 
				return;
			
			runscanlines<SLI>(&left,&right);

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

	template<bool SLI>
	void mainLoop()
	{
		lastTexKey = NULL;

		//a counter for how many polys got culled
		int culled = 0;

		u32 lastPolyAttr = 0;
		u32 lastTextureFormat = 0, lastTexturePalette = 0;

		//iterate over polys
		for(int i=0;i<clippedPolyCounter;i++)
		{
			if(!polyVisible[i]) continue;

			GFX3D_Clipper::TClippedPoly &clippedPoly = clippedPolys[i];
			POLY *poly = clippedPoly.poly;
			int type = clippedPoly.type;

			//if(i == 0 || lastPolyAttr != poly->polyAttr)
			{
				polyAttr.setup(poly->polyAttr);
				polyAttr.translucent = poly->isTranslucent();
				lastPolyAttr = poly->polyAttr;
			}


			//if(i == 0 || lastTextureFormat != poly->texParam || lastTexturePalette != poly->texPalette)
			{
				sampler.setup(poly->texParam);
				lastTextureFormat = poly->texParam;
				lastTexturePalette = poly->texPalette;
			}

			lastTexKey = polyTexKeys[i];

			//hmm... shader gets setup every time because it depends on sampler which may have just changed
			shader.setup(poly->polyAttr);

			for(int j=0;j<type;j++)
				this->verts[j] = &clippedPoly.clipVerts[j];
			for(int j=type;j<MAX_CLIPPED_VERTS;j++)
				this->verts[j] = NULL;

			polyAttr.backfacing = polyBackfacing[i];

			shape_engine<SLI>(type,!polyAttr.backfacing);
		}
	}


}; //rasterizerUnit


static Task rasterizerUnitTask[4];
static RasterizerUnit rasterizerUnit[4];
static int rasterizerCores;
static bool rasterizerUnitTasksInited = false;

static void* execRasterizerUnit(void* arg)
{
	intptr_t which = (intptr_t)arg;
	rasterizerUnit[which].mainLoop<true>();
	return 0;
}

static char SoftRastInit(void)
{
	if(!rasterizerUnitTasksInited)
	{
		rasterizerUnitTasksInited = true;

		if(CommonSettings.num_cores>=4)
		{
			rasterizerCores = 4;
			rasterizerUnit[0].SLI_MASK = 3;
			rasterizerUnit[1].SLI_MASK = 3;
			rasterizerUnit[2].SLI_MASK = 3;
			rasterizerUnit[3].SLI_MASK = 3;
			rasterizerUnit[0].SLI_VALUE = 0;
			rasterizerUnit[1].SLI_VALUE = 1;
			rasterizerUnit[2].SLI_VALUE = 2;
			rasterizerUnit[3].SLI_VALUE = 3;
			rasterizerUnitTask[0].start(false);
			rasterizerUnitTask[1].start(false);
			rasterizerUnitTask[2].start(false);
			rasterizerUnitTask[3].start(false);
		} else if(CommonSettings.num_cores>1)
		{
			rasterizerCores = 2;
			rasterizerUnit[0].SLI_MASK = 1;
			rasterizerUnit[1].SLI_MASK = 1;
			rasterizerUnit[0].SLI_VALUE = 0;
			rasterizerUnit[1].SLI_VALUE = 1;
			rasterizerUnitTask[0].start(false);
			rasterizerUnitTask[1].start(false);
		} else {
			rasterizerCores = 1;
			rasterizerUnit[0].SLI_MASK = 0;
			rasterizerUnit[0].SLI_VALUE = 0;
		}
	}

	static bool tables_generated = false;
	if(!tables_generated)
	{
		tables_generated = true;

		clipper.clippedPolys = clippedPolys = new GFX3D_Clipper::TClippedPoly[POLYLIST_SIZE*2];

		for(int i=0;i<64;i++)
		{
			for(int j=0;j<64;j++)
			{
				modulate_table[i][j] = ((i+1) * (j+1) - 1) >> 6;	
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

	printf("SoftRast Initialized\n");
	return 1;
}

static void SoftRastReset() {
	TexCache_Reset();
}

static void SoftRastClose()
{
	for(int i=0;i<4;i++)
		rasterizerUnitTask[i].shutdown();
	rasterizerUnitTasksInited = false;
}

static void SoftRastVramReconfigureSignal() {
	TexCache_Invalidate();
}

static void SoftRastFramebufferProcess()
{
	// this looks ok although it's still pretty much a hack,
	// it needs to be redone with low-level accuracy at some point,
	// but that should probably wait until the shape renderer is more accurate.
	// a good test case for edge marking is Sonic Rush:
	// - the edges are completely sharp/opaque on the very brief title screen intro,
	// - the level-start intro gets a pseudo-antialiasing effect around the silhouette,
	// - the character edges in-level are clearly transparent, and also show well through shield powerups.
	if(gfx3d.enableEdgeMarking && CommonSettings.GFX3D_EdgeMark)
	{ 
		//TODO - need to test and find out whether these get grabbed at flush time, or at render time
		//we can do this by rendering a 3d frame and then freezing the system, but only changing the edge mark colors
		FragmentColor edgeMarkColors[8];
		int edgeMarkDisabled[8];

		for(int i=0;i<8;i++)
		{
			u16 col = T1ReadWord(MMU.MMU_MEM[ARMCPU_ARM9][0x40], 0x330+i*2);
			edgeMarkColors[i].color = RGB15TO5555(col,0x0F);

			// this seems to be the only thing that selectively disables edge marking
			edgeMarkDisabled[i] = (col == 0x7FFF);
		}

		for(int i=0,y=0;y<192;y++)
		{
			for(int x=0;x<256;x++,i++)
			{
				Fragment destFragment = screen[i];
				u8 self = destFragment.polyid.opaque;
				if(edgeMarkDisabled[self>>3]) continue;
				if(destFragment.isTranslucentPoly) continue;

				// > is used instead of != to prevent double edges
				// between overlapping polys of different IDs.
				// also note that the edge generally goes on the outside, not the inside, (maybe needs to change later)
				// and that polys with the same edge color can make edges against each other.

				FragmentColor edgeColor = edgeMarkColors[self>>3];

#define PIXOFFSET(dx,dy) ((dx)+(256*(dy)))
#define ISEDGE(dx,dy) ((x+(dx)!=256) && (x+(dx)!=-1) && (y+(dy)!=192) && (y+(dy)!=-1) && self > screen[i+PIXOFFSET(dx,dy)].polyid.opaque)
#define DRAWEDGE(dx,dy) alphaBlend(screenColor[i+PIXOFFSET(dx,dy)], edgeColor)

				bool upleft    = ISEDGE(-1,-1);
				bool up        = ISEDGE( 0,-1);
				bool upright   = ISEDGE( 1,-1);
				bool left      = ISEDGE(-1, 0);
				bool right     = ISEDGE( 1, 0);
				bool downleft  = ISEDGE(-1, 1);
				bool down      = ISEDGE( 0, 1);
				bool downright = ISEDGE( 1, 1);

				if(upleft && upright && downleft && !downright)
					DRAWEDGE(-1,-1);
				if(up && !down)
					DRAWEDGE(0,-1);
				if(upleft && upright && !downleft && downright)
					DRAWEDGE(1,-1);
				if(left && !right)
					DRAWEDGE(-1,0);
				if(right && !left)
					DRAWEDGE(1,0);
				if(upleft && !upright && downleft && downright)
					DRAWEDGE(-1,1);
				if(down && !up)
					DRAWEDGE(0,1);
				if(!upleft && upright && downleft && downright)
					DRAWEDGE(1,1);

#undef PIXOFFSET
#undef ISEDGE
#undef DRAWEDGE

			}
		}
	}

	if(gfx3d.enableFog && CommonSettings.GFX3D_Fog)
	{
		u32 r = GFX3D_5TO6((gfx3d.fogColor)&0x1F);
		u32 g = GFX3D_5TO6((gfx3d.fogColor>>5)&0x1F);
		u32 b = GFX3D_5TO6((gfx3d.fogColor>>10)&0x1F);
		u32 a = (gfx3d.fogColor>>16)&0x1F;
		for(int i=0;i<256*192;i++)
		{
			Fragment &destFragment = screen[i];
			if(!destFragment.fogged) continue;
			FragmentColor &destFragmentColor = screenColor[i];
			u32 fogIndex = destFragment.depth>>9;
			assert(fogIndex<32768);
			u8 fog = fogTable[fogIndex];
			if(fog==127) fog=128;
			if(!gfx3d.enableFogAlphaOnly)
			{
				destFragmentColor.r = ((128-fog)*destFragmentColor.r + r*fog)>>7;
				destFragmentColor.g = ((128-fog)*destFragmentColor.g + g*fog)>>7;
				destFragmentColor.b = ((128-fog)*destFragmentColor.b + b*fog)>>7;
			}
			destFragmentColor.a = ((128-fog)*destFragmentColor.a + a*fog)>>7;
		}
	}
}

static void SoftRastConvertFramebuffer()
{
	memcpy(gfx3d_convertedScreen,screenColor,256*192*4);
}

static void SoftRastRender()
{
	Fragment clearFragment;
	FragmentColor clearFragmentColor;
	clearFragment.isTranslucentPoly = 0;
	clearFragmentColor.r = GFX3D_5TO6(gfx3d.clearColor&0x1F);
	clearFragmentColor.g = GFX3D_5TO6((gfx3d.clearColor>>5)&0x1F);
	clearFragmentColor.b = GFX3D_5TO6((gfx3d.clearColor>>10)&0x1F);
	clearFragmentColor.a = ((gfx3d.clearColor>>16)&0x1F);
	clearFragment.polyid.opaque = (gfx3d.clearColor>>24)&0x3F;
	//special value for uninitialized translucent polyid. without this, fires in spiderman2 dont display
	//I am not sure whether it is right, though. previously this was cleared to 0, as a guess,
	//but in spiderman2 some fires with polyid 0 try to render on top of the background
	clearFragment.polyid.translucent = kUnsetTranslucentPolyID; 
	clearFragment.depth = gfx3d.clearDepth;
	clearFragment.stencil = 0;
	clearFragment.isTranslucentPoly = 0;
	clearFragment.fogged = BIT15(gfx3d.clearColor);
	for(int i=0;i<256*192;i++)
		screen[i] = clearFragment;

	if(gfx3d.enableClearImage)
	{
		u16* clearImage = (u16*)MMU.texInfo.textureSlotAddr[2];
		u16* clearDepth = (u16*)MMU.texInfo.textureSlotAddr[3];

		//the lion, the witch, and the wardrobe (thats book 1, suck it you new-school numberers)
		//uses the scroll registers in the main game engine
		u16 scroll = T1ReadWord(MMU.ARM9_REG,0x356); //CLRIMAGE_OFFSET
		u16 xscroll = scroll&0xFF;
		u16 yscroll = (scroll>>8)&0xFF;

		FragmentColor *dstColor = screenColor;
		Fragment *dst = screen;

		for(int iy=0;iy<192;iy++) {
			int y = ((iy + yscroll)&255)<<8;
			for(int ix=0;ix<256;ix++) {
				int x = (ix + xscroll)&255;
				int adr = y + x;
				
				//this is tested by harry potter and the order of the phoenix.
				//TODO (optimization) dont do this if we are mapped to blank memory (such as in sonic chronicles)
				//(or use a special zero fill in the bulk clearing above)
				u16 col = clearImage[adr];
				dstColor->color = RGB15TO6665(col,31*(col>>15));
				
				//this is tested quite well in the sonic chronicles main map mode
				//where depth values are used for trees etc you can walk behind
				u32 depth = clearDepth[adr];
				dst->fogged = BIT15(depth);
				//TODO - might consider a lookup table for this
				dst->depth = gfx3d_extendDepth_15_to_24(depth&0x7FFF);

				dstColor++;
				dst++;
			}
		}
	}
	else 
		for(int i=0;i<256*192;i++)
			screenColor[i] = clearFragmentColor;

	//convert the toon colors
	//TODO for a slight speedup this could be cached in gfx3d (oglrenderer could benefit as well)
	for(int i=0;i<32;i++) {
		toonTable[i].r = GFX3D_5TO6((gfx3d.u16ToonTable[i])&0x1F);
		toonTable[i].g = GFX3D_5TO6((gfx3d.u16ToonTable[i]>>5)&0x1F);
		toonTable[i].b = GFX3D_5TO6((gfx3d.u16ToonTable[i]>>10)&0x1F);
	}

	//setup fog variables (but only if fog is enabled)
	if(gfx3d.enableFog && CommonSettings.GFX3D_Fog)
	{
		u8* fogDensity = MMU.MMU_MEM[ARMCPU_ARM9][0x40] + 0x360;
#if 0
		//TODO - this might be a little slow; 
		//we might need to hash all the variables and only recompute this when something changes
		const int increment = (0x400 >> gfx3d.fogShift);
		for(u32 i=0;i<32768;i++) {
			if(i<gfx3d.fogOffset) {
				fogTable[i] = fogDensity[0];
				continue;
			}
			for(int j=0;j<32;j++) {
				u32 value = gfx3d.fogOffset + increment*(j+1);
				if(i<=value) {
					if(j==0) {
						fogTable[i] = fogDensity[0];
						goto done;
					} else {
						fogTable[i] = ((value-i)*fogDensity[j-1] + (increment-(value-i))*fogDensity[j])/increment;
						goto done;
					}
				}
			}
			fogTable[i] = fogDensity[31];
			done: ;
		}
#else
		// this should behave exactly the same as the previous loop,
		// except much faster. (because it's not a 2d loop and isn't so branchy either)
		// maybe it's fast enough to not need to be cached, now.
		const int increment = ((1 << 10) >> gfx3d.fogShift);
		const int incrementDivShift = 10 - gfx3d.fogShift;
		u32 fogOffset = min<u32>(max<u32>(gfx3d.fogOffset, 0), 32768);
		u32 iMin = min<u32>(32768, (( 1 + 1) << incrementDivShift) + fogOffset + 1 - increment);
		u32 iMax = min<u32>(32768, ((32 + 1) << incrementDivShift) + fogOffset + 1 - increment);
		assert(iMin <= iMax);
		memset(fogTable, fogDensity[0], iMin);
		for(u32 i = iMin; i < iMax; i++) {
			int num = (i - fogOffset + (increment-1));
			int j = (num >> incrementDivShift) - 1;
			u32 value = (num & ~(increment-1)) + fogOffset;
			u32 diff = value - i;
			assert(j >= 1 && j < 32);
			fogTable[i] = ((diff*fogDensity[j-1] + (increment-diff)*fogDensity[j]) >> incrementDivShift);
		}
		memset(fogTable+iMax, fogDensity[31], 32768-iMax);
#endif
	}
	//convert colors to float to get more precision in case we need it
	for(int i=0;i<gfx3d.vertlist->count;i++)
		gfx3d.vertlist->list[i].color_to_float();

	//submit all polys to clipper
	clipper.clippedPolyCounter = 0;
	for(int i=0;i<gfx3d.polylist->count;i++)
	{
		POLY* poly = &gfx3d.polylist->list[gfx3d.indexlist[i]];
		VERT* clipVerts[4] = {
			&gfx3d.vertlist->list[poly->vertIndexes[0]],
			&gfx3d.vertlist->list[poly->vertIndexes[1]],
			&gfx3d.vertlist->list[poly->vertIndexes[2]],
			poly->type==4
				?&gfx3d.vertlist->list[poly->vertIndexes[3]]
				:NULL
		};

		clipper.clipPoly(poly,clipVerts);

	}
	clippedPolyCounter = clipper.clippedPolyCounter;

	//printf("%d %d %d %d\n",gfx3d.viewport.x,gfx3d.viewport.y,gfx3d.viewport.width,gfx3d.viewport.height);
//			printf("%f\n",vert.coord[0]);

	//viewport transforms
	for(int i=0;i<clippedPolyCounter;i++)
	{
		GFX3D_Clipper::TClippedPoly &poly = clippedPolys[i];
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

	TexCacheItem* lastTexKey = NULL;
	u32 lastTextureFormat = 0, lastTexturePalette = 0;
	bool needInitTexture = true;
	for(int i=0;i<clippedPolyCounter;i++)
	{
		GFX3D_Clipper::TClippedPoly &clippedPoly = clippedPolys[i];
		POLY *poly = clippedPoly.poly;
		int type = clippedPoly.type;

		VERT* verts = &clippedPoly.clipVerts[0];


		RasterizerUnit::PolyAttr polyAttr;
		polyAttr.setup(poly->polyAttr);

		//HACK: backface culling
		//this should be moved to gfx3d, but first we need to redo the way the lists are built
		//because it is too convoluted right now.
		//(must we throw out verts if a poly gets backface culled? if not, then it might be easier)
#if 0
		//TODO - is this good enough for quads and other shapes? we think so.
		float ab[2], ac[2];
		Vector2Copy(ab, verts[1].coord);
		Vector2Copy(ac, verts[2].coord);
		Vector2Subtract(ab, verts[0].coord);
		Vector2Subtract(ac, verts[0].coord);
		float cross = Vector2Cross(ab, ac);
		polyAttr.backfacing = (cross>0);
#else
		// actually, the answer to the above question seems to be NO, it's not good enough,
		// because we have to support somewhat non-convex polygons (see NSMB world map 1st screen).
		// this version should handle those cases better.
		int n = type - 1;
		float facing = (verts[0].y + verts[n].y) * (verts[0].x - verts[n].x)
					 + (verts[1].y + verts[0].y) * (verts[1].x - verts[0].x)
					 + (verts[2].y + verts[1].y) * (verts[2].x - verts[1].x);
		for(int j = 2; j < n; j++)
			facing += (verts[j+1].y + verts[j].y) * (verts[j+1].x - verts[j].x);
		polyBackfacing[i] = polyAttr.backfacing = (facing < 0);
#endif

		if(!polyAttr.isVisible(polyAttr.backfacing)) {
			polyVisible[i] = false;
			continue;
		}

		polyVisible[i] = true;

		//here is a hack which needs to be removed.
		//at some point our shape engine needs these to be converted to "fixed point"
		//which is currently just a float
		for(int j=0;j<type;j++)
			for(int k=0;k<2;k++)
				verts[j].coord[k] = (float)iround(16.0f * verts[j].coord[k]);

		//make sure all the textures we'll need are cached
		if(needInitTexture || lastTextureFormat != poly->texParam || lastTexturePalette != poly->texPalette)
		{
			lastTexKey = TexCache_SetTexture(TexFormat_15bpp,poly->texParam,poly->texPalette);
			lastTextureFormat = poly->texParam;
			lastTexturePalette = poly->texPalette;
			needInitTexture = false;
		}

		//printf("%08X %d\n",poly->texParam,rasterizerUnit[0].textures.currentNum);
		polyTexKeys[i] = lastTexKey;
	}

	if(rasterizerCores==1)
	{
		rasterizerUnit[0].mainLoop<false>();
	}
	else
	{
		for(int i=0;i<rasterizerCores;i++) rasterizerUnitTask[i].execute(execRasterizerUnit,(void*)i);
		for(int i=0;i<rasterizerCores;i++) rasterizerUnitTask[i].finish();
	}

	TexCache_EvictFrame();

	SoftRastFramebufferProcess();

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

