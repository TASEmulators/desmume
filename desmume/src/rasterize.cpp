/*
	Copyright (C) 2009-2019 DeSmuME team

	This file is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 2 of the License, or
	(at your option) any later version.

	This file is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with the this software.  If not, see <http://www.gnu.org/licenses/>.
*/

//nothing in this file should be assumed to be accurate
//
//the shape rasterizers contained herein are based on code supplied by Chris Hecker from 
//http://chrishecker.com/Miscellaneous_Technical_Articles


//TODO - due to a late change of a y-coord flipping, our winding order is wrong
//this causes us to have to flip the verts for every front-facing poly.
//a performance improvement would be to change the winding order logic
//so that this is done less frequently

//Shadow test cases
// * SM64 -- Standing near signs and blocks. Note: shadows-behind-trees looks bad in that game.
// * MKDS -- Shadows under cart in kart selector (the shadows in mario kart are complicated concave shapes)
// * MKDS -- no junk beneath platform in kart selector / no shadow beneath grate floor in bowser stage
// * MKDS -- 150cc mirror cup turns all the models inside out
// * The Wizard of Oz: Beyond the Yellow Brick Road -- shadows under Dorothy and Toto
// * Kingdom Hearts Re:coded -- shadow under Sora
// * Golden Sun: Dark Dawn -- shadow under the main character

#include "rasterize.h"

#include <algorithm>
#include <assert.h>
#include <math.h>
#include <string.h>

#if defined(_MSC_VER) && _MSC_VER == 1600
#define SLEEP_HACK_2011
#endif

#ifdef SLEEP_HACK_2011
#include <Windows.h>
#endif

#ifndef _MSC_VER 
#include <stdint.h>
#endif

#ifdef ENABLE_SSE2
#include <emmintrin.h>
#endif

#include "matrix.h"
#include "render3D.h"
#include "MMU.h"
#include "NDSSystem.h"
#include "utils/bits.h"
#include "utils/task.h"
#include "filter/filter.h"
#include "filter/xbrz.h"

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

static u8 modulate_table[64][64];
static u8 decal_table[32][64][64];

////optimized float floor useful in limited cases
////from http://www.stereopsis.com/FPU.html#convert
////(unfortunately, it relies on certain FPU register settings)
//int Real2Int(double val)
//{
//	const double _double2fixmagic = 68719476736.0*1.5;     //2^36 * 1.5,  (52-_shiftamt=36) uses limited precisicion to floor
//	const int _shiftamt        = 16;                    //16.16 fixed point representation,
//
//	#ifdef MSB_FIRST
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


//INLINE static void SubmitVertex(int vert_index, VERT& rawvert)
//{
//	verts[vert_index] = &rawvert;
//}

static FORCEINLINE int iround(float f) {
	return (int)f; //lol
}


typedef int fixed28_4;

// handle floor divides and mods correctly 
static FORCEINLINE void FloorDivMod(long Numerator, long Denominator, long &Floor, long &Mod, bool& failure)
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

static FORCEINLINE fixed28_4 FloatToFixed28_4( float Value ) {
	return (fixed28_4)(Value * 16);
}
static FORCEINLINE float Fixed28_4ToFloat( fixed28_4 Value ) {
	return Value / 16.0f;
}
//inline fixed16_16 FloatToFixed16_16( float Value ) {
//	return (fixed16_6)(Value * 65536);
//}
//inline float Fixed16_16ToFloat( fixed16_16 Value ) {
//	return Value / 65536.0;
//}
static FORCEINLINE fixed28_4 Fixed28_4Mul( fixed28_4 A, fixed28_4 B ) {
	// could make this asm to prevent overflow
	return (A * B) / 16;	// 28.4 * 28.4 = 24.8 / 16 = 28.4
}
static FORCEINLINE int Ceil28_4( fixed28_4 Value ) {
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
		FORCEINLINE void initialize(float value) {
			curr = value;
			step = 0;
			stepExtra = 0;
		}
		FORCEINLINE void initialize(float top, float bottom, float dx, float dy, long inXStep, float XPrestep, float YPrestep) {
			dx = 0;
			dy *= (bottom-top);
			curr = top + YPrestep * dy + XPrestep * dx;
			step = inXStep * dx + dy;
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
	X = Ceil28_4((fixed28_4)verts[Top]->x);
	int XEnd = Ceil28_4((fixed28_4)verts[Bottom]->x);
	int Width = XEnd - X; // can be negative

	// even if Height == 0, give some info for horizontal line poly
	if(Height != 0 || Width != 0)
	{
		long dN = long(verts[Bottom]->y - verts[Top]->y);
		long dM = long(verts[Bottom]->x - verts[Top]->x);
		if (dN != 0)
		{
			long InitialNumerator = (long)(dM*16*Y - dM*verts[Top]->y + dN*verts[Top]->x - 1 + dN*16);
			FloorDivMod(InitialNumerator,dN*16,X,ErrorTerm,failure);
			FloorDivMod(dM*16,dN*16,XStep,Numerator,failure);
			Denominator = dN*16;
		}
		else
		{
			XStep = Width;
			Numerator = 0;
			ErrorTerm = 0;
			Denominator = 1;
			dN = 1;
		}
	
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
	else
	{
		// even if Width == 0 && Height == 0, give some info for pixel poly
		// example: Castlevania Portrait of Ruin, warp stone
		XStep = 1;
		Numerator = 0;
		Denominator = 1;
		ErrorTerm = 0;
		invw.initialize(1/verts[Top]->w);
		u.initialize(verts[Top]->u);
		v.initialize(verts[Top]->v);
		z.initialize(verts[Top]->z);
		for(int i=0;i<3;i++)
			color[i].initialize(verts[Top]->fcolor[i]);
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



static FORCEINLINE void alphaBlend(FragmentColor &dst, const FragmentColor src)
{
	if (src.a == 0)
	{
		return;
	}
	
	if (src.a == 31 || dst.a == 0 || !gfx3d.renderState.enableAlphaBlending)
	{
		dst = src;
	}
	else
	{
		const u8 alpha = src.a + 1;
		const u8 invAlpha = 32 - alpha;
		dst.r = (alpha*src.r + invAlpha*dst.r) >> 5;
		dst.g = (alpha*src.g + invAlpha*dst.g) >> 5;
		dst.b = (alpha*src.b + invAlpha*dst.b) >> 5;
		dst.a = max(src.a, dst.a);
	}
}

static FORCEINLINE void EdgeBlend(FragmentColor &dst, const FragmentColor src)
{
	if (src.a == 31 || dst.a == 0)
	{
		dst = src;
	}
	else
	{
		const u8 alpha = src.a + 1;
		const u8 invAlpha = 32 - alpha;
		dst.r = (alpha*src.r + invAlpha*dst.r) >> 5;
		dst.g = (alpha*src.g + invAlpha*dst.g) >> 5;
		dst.b = (alpha*src.b + invAlpha*dst.b) >> 5;
		dst.a = max(src.a, dst.a);
	}
}

template<bool RENDERER>
Render3DError RasterizerUnit<RENDERER>::_SetupTexture(const POLY &thePoly, size_t polyRenderIndex)
{
	SoftRasterizerTexture *theTexture = (SoftRasterizerTexture *)this->_softRender->GetTextureByPolygonRenderIndex(polyRenderIndex);
	this->_currentTexture = theTexture;
	
	if (!theTexture->IsSamplingEnabled())
	{
		return RENDER3DERROR_NOERR;
	}
	
	this->_textureWrapMode = thePoly.texParam.TextureWrapMode;
	
	theTexture->ResetCacheAge();
	theTexture->IncreaseCacheUsageCount(1);
	
	return RENDER3DERROR_NOERR;
}

template<bool RENDERER>
FORCEINLINE FragmentColor RasterizerUnit<RENDERER>::_sample(const float u, const float v)
{
	//finally, we can use floor here. but, it is slower than we want.
	//the best solution is probably to wait until the pipeline is full of fixed point
	const float fu = u * (float)this->_currentTexture->GetRenderWidth()  / (float)this->_currentTexture->GetWidth();
	const float fv = v * (float)this->_currentTexture->GetRenderHeight() / (float)this->_currentTexture->GetHeight();
	s32 iu = 0;
	s32 iv = 0;
	
	if (!this->_softRender->_enableFragmentSamplingHack)
	{
		iu = s32floor(fu);
		iv = s32floor(fv);
	}
	else
	{
		iu = this->_round_s(fu);
		iv = this->_round_s(fv);
	}
	
	const u32 *textureData = this->_currentTexture->GetRenderData();
	this->_currentTexture->GetRenderSamplerCoordinates(this->_textureWrapMode, iu, iv);
	
	FragmentColor color;
	color.color = textureData[( iv << this->_currentTexture->GetRenderWidthShift() ) + iu];
	
	return color;
}

//round function - tkd3
template<bool RENDERER>
FORCEINLINE float RasterizerUnit<RENDERER>::_round_s(double val)
{
	if (val > 0.0)
	{
		return floorf(val*256.0f+0.5f)/256.0f; //this value(256.0) is good result.(I think)
	}
	else
	{
		return -1.0*floorf(fabs(val)*256.0f+0.5f)/256.0f;
	}
}

template<bool RENDERER> template<bool ISSHADOWPOLYGON>
FORCEINLINE void RasterizerUnit<RENDERER>::_shade(const PolygonMode polygonMode, const FragmentColor src, FragmentColor &dst, const float texCoordU, const float texCoordV)
{
	if (ISSHADOWPOLYGON)
	{
		dst = src;
		return;
	}
	
	static const FragmentColor colorWhite = MakeFragmentColor(0x3F, 0x3F, 0x3F, 0x1F);
	const FragmentColor mainTexColor = (this->_currentTexture->IsSamplingEnabled()) ? this->_sample(texCoordU, texCoordV) : colorWhite;
	
	switch (polygonMode)
	{
		case POLYGON_MODE_MODULATE:
			dst.r = modulate_table[mainTexColor.r][src.r];
			dst.g = modulate_table[mainTexColor.g][src.g];
			dst.b = modulate_table[mainTexColor.b][src.b];
			dst.a = modulate_table[GFX3D_5TO6_LOOKUP(mainTexColor.a)][GFX3D_5TO6_LOOKUP(src.a)]>>1;
			//dst.a = 28;
			//#ifdef _MSC_VER
			//if(GetAsyncKeyState(VK_SHIFT)) {
			//	//debugging tricks
			//	dst = shader.materialColor;
			//	if(GetAsyncKeyState(VK_TAB)) {
			//		u8 alpha = dst.a;
			//		dst.color = polynum*8+8;
			//		dst.a = alpha;
			//	}
			//}
			//#endif
			break;
			
		case POLYGON_MODE_DECAL:
		{
			if (this->_currentTexture->IsSamplingEnabled())
			{
				dst.r = decal_table[mainTexColor.a][mainTexColor.r][src.r];
				dst.g = decal_table[mainTexColor.a][mainTexColor.g][src.g];
				dst.b = decal_table[mainTexColor.a][mainTexColor.b][src.b];
				dst.a = src.a;
			}
			else
			{
				dst = src;
			}
		}
			break;
			
		case POLYGON_MODE_TOONHIGHLIGHT:
		{
			const FragmentColor toonColor = this->_softRender->toonColor32LUT[src.r >> 1];
			
			if (gfx3d.renderState.shading == PolygonShadingMode_Highlight)
			{
				// Tested in the "Shadows of Almia" logo in the Pokemon Ranger: Shadows of Almia title screen.
				// Also tested in Advance Wars: Dual Strike and Advance Wars: Days of Ruin when tiles highlight
				// during unit selection.
				dst.r = modulate_table[mainTexColor.r][src.r];
				dst.g = modulate_table[mainTexColor.g][src.r];
				dst.b = modulate_table[mainTexColor.b][src.r];
				dst.a = modulate_table[GFX3D_5TO6_LOOKUP(mainTexColor.a)][GFX3D_5TO6_LOOKUP(src.a)] >> 1;
				
				dst.r = min<u8>(0x3F, (dst.r + toonColor.r));
				dst.g = min<u8>(0x3F, (dst.g + toonColor.g));
				dst.b = min<u8>(0x3F, (dst.b + toonColor.b));
			}
			else
			{
				dst.r = modulate_table[mainTexColor.r][toonColor.r];
				dst.g = modulate_table[mainTexColor.g][toonColor.g];
				dst.b = modulate_table[mainTexColor.b][toonColor.b];
				dst.a = modulate_table[GFX3D_5TO6_LOOKUP(mainTexColor.a)][GFX3D_5TO6_LOOKUP(src.a)] >> 1;
			}
		}
			break;
			
		case POLYGON_MODE_SHADOW:
			//is this right? only with the material color?
			dst = src;
			break;
	}
}

template<bool RENDERER> template<bool ISFRONTFACING, bool ISSHADOWPOLYGON>
FORCEINLINE void RasterizerUnit<RENDERER>::_pixel(const POLYGON_ATTR polyAttr, const bool isTranslucent, const size_t fragmentIndex, FragmentColor &dstColor, float r, float g, float b, float invu, float invv, float z, float w)
{
	FragmentColor newDstColor32;
	FragmentColor shaderOutput;
	bool isOpaquePixel;
	
	u32 &dstAttributeDepth				= this->_softRender->_framebufferAttributes->depth[fragmentIndex];
	u8 &dstAttributeOpaquePolyID		= this->_softRender->_framebufferAttributes->opaquePolyID[fragmentIndex];
	u8 &dstAttributeTranslucentPolyID	= this->_softRender->_framebufferAttributes->translucentPolyID[fragmentIndex];
	u8 &dstAttributeStencil				= this->_softRender->_framebufferAttributes->stencil[fragmentIndex];
	u8 &dstAttributeIsFogged			= this->_softRender->_framebufferAttributes->isFogged[fragmentIndex];
	u8 &dstAttributeIsTranslucentPoly	= this->_softRender->_framebufferAttributes->isTranslucentPoly[fragmentIndex];
	u8 &dstAttributePolyFacing			= this->_softRender->_framebufferAttributes->polyFacing[fragmentIndex];
	
	// not sure about the w-buffer depth value: this value was chosen to make the skybox, castle window decals, and water level render correctly in SM64
	//
	// When using z-depth, be sure to test against the following test cases:
	// - The drawing of the overworld map in Dragon Quest IV
	// - The drawing of all units on the map in Advance Wars: Days of Ruin

	// Note that an IEEE-754 single-precision float uses a 23-bit significand. Therefore, we will multiply the
	// Z-depth by a 22-bit significand for safety.
	const u32 newDepth = (gfx3d.renderState.wbuffer) ? u32floor(w * 4096.0f) : u32floor(z * 4194303.0f) << 2;
	
	// run the depth test
	bool depthFail = false;
	
	if (polyAttr.DepthEqualTest_Enable)
	{
		// The EQUAL depth test is used if the polygon requests it. Note that the NDS doesn't perform
		// a true EQUAL test -- there is a set tolerance to it that makes it easier for pixels to
		// pass the depth test.
		const u32 minDepth = (u32)max<s32>(0x00000000, (s32)dstAttributeDepth - DEPTH_EQUALS_TEST_TOLERANCE);
		const u32 maxDepth = min<u32>(0x00FFFFFF, dstAttributeDepth + DEPTH_EQUALS_TEST_TOLERANCE);
		
		if (newDepth < minDepth || newDepth > maxDepth)
		{
			depthFail = true;
		}
	}
	else if ( (ISFRONTFACING && (dstAttributePolyFacing == PolyFacing_Back)) && (dstColor.a == 0x1F))
	{
		// The LEQUAL test is used in the special case where an incoming front-facing polygon's pixel
		// is to be drawn on top of a back-facing polygon's opaque pixel.
		//
		// Test case: The Customize status screen in Sands of Destruction requires this type of depth
		// test in order to correctly show the animating characters.
		if (newDepth > dstAttributeDepth)
		{
			depthFail = true;
		}
	}
	else
	{
		// The LESS depth test is the default type of depth test for all other conditions.
		if (newDepth >= dstAttributeDepth)
		{
			depthFail = true;
		}
	}

	if (depthFail)
	{
		//shadow mask polygons set stencil bit here
		if (ISSHADOWPOLYGON && polyAttr.PolygonID == 0)
			dstAttributeStencil=1;
		return;
	}
	
	//handle shadow polys
	if (ISSHADOWPOLYGON)
	{
		if (polyAttr.PolygonID == 0)
		{
			//shadow mask polygons only affect the stencil buffer, and then only when they fail depth test
			//if we made it here, the shadow mask polygon fragment needs to be trashed
			return;
		}
		else
		{
			//shadow color polygon conditions
			if (dstAttributeStencil == 0)
			{
				//draw only where stencil bit is set
				return;
			}
			if (dstAttributeOpaquePolyID == polyAttr.PolygonID)
			{
				//draw only when polygon ID differs
				//TODO: are we using the right dst polyID?
				return;
			}

			//once drawn, stencil bit is always cleared
			dstAttributeStencil = 0;
		}
	}

	//perspective-correct the colors
	r = (r * w) + 0.5f;
	g = (g * w) + 0.5f;
	b = (b * w) + 0.5f;
	
	//this is a HACK:
	//we are being very sloppy with our interpolation precision right now
	//and rather than fix it, i just want to clamp it
	newDstColor32 = MakeFragmentColor(max<u8>(0x00, min<u32>(0x3F, u32floor(r))),
									  max<u8>(0x00, min<u32>(0x3F, u32floor(g))),
									  max<u8>(0x00, min<u32>(0x3F, u32floor(b))),
									  polyAttr.Alpha);
	
	//pixel shader
	this->_shade<ISSHADOWPOLYGON>((PolygonMode)polyAttr.Mode, newDstColor32, shaderOutput, invu * w, invv * w);
	
	// handle alpha test
	if ( shaderOutput.a == 0 ||
		(this->_softRender->currentRenderState->enableAlphaTest && shaderOutput.a < this->_softRender->currentRenderState->alphaTestRef) )
	{
		return;
	}
	
	// write pixel values to the framebuffer
	isOpaquePixel = (shaderOutput.a == 0x1F);
	if (isOpaquePixel)
	{
		dstAttributeOpaquePolyID = polyAttr.PolygonID;
		dstAttributeIsTranslucentPoly = isTranslucent;
		dstAttributeIsFogged = polyAttr.Fog_Enable;
		dstColor = shaderOutput;
	}
	else
	{
		//dont overwrite pixels on translucent polys with the same polyids
		if (dstAttributeTranslucentPolyID == polyAttr.PolygonID)
			return;
		
		//originally we were using a test case of shadows-behind-trees in sm64ds
		//but, it looks bad in that game. this is actually correct
		//if this isnt correct, then complex shape cart shadows in mario kart don't work right
		dstAttributeTranslucentPolyID = polyAttr.PolygonID;
		
		//alpha blending and write color
		alphaBlend(dstColor, shaderOutput);
		
		dstAttributeIsFogged = (dstAttributeIsFogged && polyAttr.Fog_Enable);
	}
	
	dstAttributePolyFacing = (ISFRONTFACING) ? PolyFacing_Front : PolyFacing_Back;
	
	//depth writing
	if (isOpaquePixel || polyAttr.TranslucentDepthWrite_Enable)
		dstAttributeDepth = newDepth;
}

//draws a single scanline
template<bool RENDERER> template<bool ISFRONTFACING, bool ISSHADOWPOLYGON, bool USELINEHACK>
FORCEINLINE void RasterizerUnit<RENDERER>::_drawscanline(const POLYGON_ATTR polyAttr, const bool isTranslucent, FragmentColor *dstColor, const size_t framebufferWidth, const size_t framebufferHeight, edge_fx_fl *pLeft, edge_fx_fl *pRight)
{
	const int XStart = pLeft->X;
	int width = pRight->X - XStart;

	// HACK: workaround for vertical/slant line poly
	if (USELINEHACK && width == 0)
	{
		int leftWidth = pLeft->XStep;
		if (pLeft->ErrorTerm + pLeft->Numerator >= pLeft->Denominator)
			leftWidth++;
		int rightWidth = pRight->XStep;
		if (pRight->ErrorTerm + pRight->Numerator >= pRight->Denominator)
			rightWidth++;
		width = max(1, max(abs(leftWidth), abs(rightWidth)));
	}

	//these are the starting values, taken from the left edge
	CACHE_ALIGN float coord[4] = {
		pLeft->u.curr,
		pLeft->v.curr,
		pLeft->z.curr,
		pLeft->invw.curr
	};
	
	CACHE_ALIGN float color[4] = {
		pLeft->color[0].curr,
		pLeft->color[1].curr,
		pLeft->color[2].curr,
		(float)polyAttr.Alpha / 31.0f
	};
	
	//our dx values are taken from the steps up until the right edge
	const float invWidth = 1.0f / (float)width;
	
	const CACHE_ALIGN float coord_dx[4] = {
		(pRight->u.curr - coord[0]) * invWidth,
		(pRight->v.curr - coord[1]) * invWidth,
		(pRight->z.curr - coord[2]) * invWidth,
		(pRight->invw.curr - coord[3]) * invWidth
	};
	
	const CACHE_ALIGN float color_dx[4] = {
		(pRight->color[0].curr - color[0]) * invWidth,
		(pRight->color[1].curr - color[1]) * invWidth,
		(pRight->color[2].curr - color[2]) * invWidth,
		0.0f * invWidth
	};

	size_t adr = (pLeft->Y*framebufferWidth)+XStart;

	//CONSIDER: in case some other math is wrong (shouldve been clipped OK), we might go out of bounds here.
	//better check the Y value.
	if (RENDERER && (pLeft->Y < 0 || pLeft->Y > (framebufferHeight - 1)))
	{
		printf("rasterizer rendering at y=%d! oops!\n",pLeft->Y);
		return;
	}
	if (!RENDERER && (pLeft->Y < 0 || pLeft->Y >= framebufferHeight))
	{
		printf("rasterizer rendering at y=%d! oops!\n",pLeft->Y);
		return;
	}

	int x = XStart;

	if (x < 0)
	{
		if (RENDERER && !USELINEHACK)
		{
			printf("rasterizer rendering at x=%d! oops!\n",x);
			return;
		}
		
		const float negativeX = (float)-x;
		
		coord[0] += coord_dx[0] * negativeX;
		coord[1] += coord_dx[1] * negativeX;
		coord[2] += coord_dx[2] * negativeX;
		coord[3] += coord_dx[3] * negativeX;
		
		color[0] += color_dx[0] * negativeX;
		color[1] += color_dx[1] * negativeX;
		color[2] += color_dx[2] * negativeX;
		color[3] += color_dx[3] * negativeX;
		
		adr += -x;
		width -= -x;
		x = 0;
	}
	if (x+width > framebufferWidth)
	{
		if (RENDERER && !USELINEHACK && framebufferWidth == GPU_FRAMEBUFFER_NATIVE_WIDTH)
		{
			printf("rasterizer rendering at x=%d! oops!\n",x+width-1);
			return;
		}
		width = framebufferWidth - x;
	}
	
	while (width-- > 0)
	{
		this->_pixel<ISFRONTFACING, ISSHADOWPOLYGON>(polyAttr, isTranslucent, adr, dstColor[adr], color[0], color[1], color[2], coord[0], coord[1], coord[2], 1.0f/coord[3]);
		adr++;
		x++;

		coord[0] += coord_dx[0];
		coord[1] += coord_dx[1];
		coord[2] += coord_dx[2];
		coord[3] += coord_dx[3];
		
		color[0] += color_dx[0];
		color[1] += color_dx[1];
		color[2] += color_dx[2];
		color[3] += color_dx[3];
	}
}

#ifdef ENABLE_SSE2

template<bool RENDERER> template<bool ISFRONTFACING, bool ISSHADOWPOLYGON>
FORCEINLINE void RasterizerUnit<RENDERER>::_pixel_SSE2(const POLYGON_ATTR polyAttr, const bool isTranslucent, const size_t fragmentIndex, FragmentColor &dstColor, const __m128 &srcColorf, float invu, float invv, float z, float w)
{
	FragmentColor newDstColor32;
	FragmentColor shaderOutput;
	bool isOpaquePixel;
	
	u32 &dstAttributeDepth				= this->_softRender->_framebufferAttributes->depth[fragmentIndex];
	u8 &dstAttributeOpaquePolyID		= this->_softRender->_framebufferAttributes->opaquePolyID[fragmentIndex];
	u8 &dstAttributeTranslucentPolyID	= this->_softRender->_framebufferAttributes->translucentPolyID[fragmentIndex];
	u8 &dstAttributeStencil				= this->_softRender->_framebufferAttributes->stencil[fragmentIndex];
	u8 &dstAttributeIsFogged			= this->_softRender->_framebufferAttributes->isFogged[fragmentIndex];
	u8 &dstAttributeIsTranslucentPoly	= this->_softRender->_framebufferAttributes->isTranslucentPoly[fragmentIndex];
	u8 &dstAttributePolyFacing			= this->_softRender->_framebufferAttributes->polyFacing[fragmentIndex];
	
	// not sure about the w-buffer depth value: this value was chosen to make the skybox, castle window decals, and water level render correctly in SM64
	//
	// When using z-depth, be sure to test against the following test cases:
	// - The drawing of the overworld map in Dragon Quest IV
	// - The drawing of all units on the map in Advance Wars: Days of Ruin
	
	// Note that an IEEE-754 single-precision float uses a 23-bit significand. Therefore, we will multiply the
	// Z-depth by a 22-bit significand for safety.
	const u32 newDepth = (gfx3d.renderState.wbuffer) ? u32floor(w * 4096.0f) : u32floor(z * 4194303.0f) << 2;
	
	// run the depth test
	bool depthFail = false;
	
	if (polyAttr.DepthEqualTest_Enable)
	{
		// The EQUAL depth test is used if the polygon requests it. Note that the NDS doesn't perform
		// a true EQUAL test -- there is a set tolerance to it that makes it easier for pixels to
		// pass the depth test.
		const u32 minDepth = (u32)max<s32>(0x00000000, (s32)dstAttributeDepth - DEPTH_EQUALS_TEST_TOLERANCE);
		const u32 maxDepth = min<u32>(0x00FFFFFF, dstAttributeDepth + DEPTH_EQUALS_TEST_TOLERANCE);
		
		if (newDepth < minDepth || newDepth > maxDepth)
		{
			depthFail = true;
		}
	}
	else if ( (ISFRONTFACING && (dstAttributePolyFacing == PolyFacing_Back)) && (dstColor.a == 0x1F))
	{
		// The LEQUAL test is used in the special case where an incoming front-facing polygon's pixel
		// is to be drawn on top of a back-facing polygon's opaque pixel.
		//
		// Test case: The Customize status screen in Sands of Destruction requires this type of depth
		// test in order to correctly show the animating characters.
		if (newDepth > dstAttributeDepth)
		{
			depthFail = true;
		}
	}
	else
	{
		// The LESS depth test is the default type of depth test for all other conditions.
		if (newDepth >= dstAttributeDepth)
		{
			depthFail = true;
		}
	}
	
	if (depthFail)
	{
		//shadow mask polygons set stencil bit here
		if (ISSHADOWPOLYGON && polyAttr.PolygonID == 0)
			dstAttributeStencil=1;
		return;
	}
	
	//handle shadow polys
	if (ISSHADOWPOLYGON)
	{
		if (polyAttr.PolygonID == 0)
		{
			//shadow mask polygons only affect the stencil buffer, and then only when they fail depth test
			//if we made it here, the shadow mask polygon fragment needs to be trashed
			return;
		}
		else
		{
			//shadow color polygon conditions
			if (dstAttributeStencil == 0)
			{
				//draw only where stencil bit is set
				return;
			}
			if (dstAttributeOpaquePolyID == polyAttr.PolygonID)
			{
				//draw only when polygon ID differs
				//TODO: are we using the right dst polyID?
				return;
			}
			
			//once drawn, stencil bit is always cleared
			dstAttributeStencil = 0;
		}
	}
	
	//perspective-correct the colors
	const __m128 perspective = _mm_set_ps(31.0f, w, w, w);
	__m128 newColorf = _mm_add_ps( _mm_mul_ps(srcColorf, perspective), _mm_set1_ps(0.5f) );
	newColorf = _mm_max_ps(newColorf, _mm_setzero_ps());
	
	__m128i cvtColor32 = _mm_cvttps_epi32(newColorf);
	cvtColor32 = _mm_min_epu8(cvtColor32, _mm_set_epi32(0x1F, 0x3F, 0x3F, 0x3F));
	cvtColor32 = _mm_packus_epi16(cvtColor32, _mm_setzero_si128());
	cvtColor32 = _mm_packus_epi16(cvtColor32, _mm_setzero_si128());
	
	newDstColor32.color = _mm_cvtsi128_si32(cvtColor32);
	
	//pixel shader
	this->_shade<ISSHADOWPOLYGON>((PolygonMode)polyAttr.Mode, newDstColor32, shaderOutput, invu * w, invv * w);
	
	// handle alpha test
	if ( shaderOutput.a == 0 || (this->_softRender->currentRenderState->enableAlphaTest && shaderOutput.a < this->_softRender->currentRenderState->alphaTestRef) )
	{
		return;
	}
	
	// write pixel values to the framebuffer
	isOpaquePixel = (shaderOutput.a == 0x1F);
	if (isOpaquePixel)
	{
		dstAttributeOpaquePolyID = polyAttr.PolygonID;
		dstAttributeIsTranslucentPoly = isTranslucent;
		dstAttributeIsFogged = polyAttr.Fog_Enable;
		dstColor = shaderOutput;
	}
	else
	{
		//dont overwrite pixels on translucent polys with the same polyids
		if (dstAttributeTranslucentPolyID == polyAttr.PolygonID)
			return;
		
		//originally we were using a test case of shadows-behind-trees in sm64ds
		//but, it looks bad in that game. this is actually correct
		//if this isnt correct, then complex shape cart shadows in mario kart don't work right
		dstAttributeTranslucentPolyID = polyAttr.PolygonID;
		
		//alpha blending and write color
		alphaBlend(dstColor, shaderOutput);
		
		dstAttributeIsFogged = (dstAttributeIsFogged && polyAttr.Fog_Enable);
	}
	
	dstAttributePolyFacing = (ISFRONTFACING) ? PolyFacing_Front : PolyFacing_Back;
	
	//depth writing
	if (isOpaquePixel || polyAttr.TranslucentDepthWrite_Enable)
		dstAttributeDepth = newDepth;
}

//draws a single scanline
template<bool RENDERER> template<bool ISFRONTFACING, bool ISSHADOWPOLYGON, bool USELINEHACK>
FORCEINLINE void RasterizerUnit<RENDERER>::_drawscanline_SSE2(const POLYGON_ATTR polyAttr, const bool isTranslucent, FragmentColor *dstColor, const size_t framebufferWidth, const size_t framebufferHeight, edge_fx_fl *pLeft, edge_fx_fl *pRight)
{
	const int XStart = pLeft->X;
	int width = pRight->X - XStart;
	
	// HACK: workaround for vertical/slant line poly
	if (USELINEHACK && width == 0)
	{
		int leftWidth = pLeft->XStep;
		if (pLeft->ErrorTerm + pLeft->Numerator >= pLeft->Denominator)
			leftWidth++;
		int rightWidth = pRight->XStep;
		if (pRight->ErrorTerm + pRight->Numerator >= pRight->Denominator)
			rightWidth++;
		width = max(1, max(abs(leftWidth), abs(rightWidth)));
	}
	
	//these are the starting values, taken from the left edge
	__m128 coord = _mm_setr_ps(pLeft->u.curr,
							   pLeft->v.curr,
							   pLeft->z.curr,
							   pLeft->invw.curr);
	
	__m128 color = _mm_setr_ps(pLeft->color[0].curr,
							   pLeft->color[1].curr,
							   pLeft->color[2].curr,
							   (float)polyAttr.Alpha / 31.0f);
	
	//our dx values are taken from the steps up until the right edge
	const __m128 invWidth = _mm_set1_ps(1.0f / (float)width);
	const __m128 coord_dx = _mm_mul_ps(_mm_setr_ps(pRight->u.curr - pLeft->u.curr, pRight->v.curr - pLeft->v.curr, pRight->z.curr - pLeft->z.curr, pRight->invw.curr - pLeft->invw.curr), invWidth);
	const __m128 color_dx = _mm_mul_ps(_mm_setr_ps(pRight->color[0].curr - pLeft->color[0].curr, pRight->color[1].curr - pLeft->color[1].curr, pRight->color[2].curr - pLeft->color[2].curr, 0.0f), invWidth);
	
	size_t adr = (pLeft->Y*framebufferWidth)+XStart;
	
	//CONSIDER: in case some other math is wrong (shouldve been clipped OK), we might go out of bounds here.
	//better check the Y value.
	if (RENDERER && (pLeft->Y < 0 || pLeft->Y > (framebufferHeight - 1)))
	{
		printf("rasterizer rendering at y=%d! oops!\n",pLeft->Y);
		return;
	}
	if (!RENDERER && (pLeft->Y < 0 || pLeft->Y >= framebufferHeight))
	{
		printf("rasterizer rendering at y=%d! oops!\n",pLeft->Y);
		return;
	}
	
	int x = XStart;
	
	if (x < 0)
	{
		if (RENDERER && !USELINEHACK)
		{
			printf("rasterizer rendering at x=%d! oops!\n",x);
			return;
		}
		
		const __m128 negativeX = _mm_cvtepi32_ps(_mm_set1_epi32(-x));
		coord = _mm_add_ps(coord, _mm_mul_ps(coord_dx, negativeX));
		color = _mm_add_ps(color, _mm_mul_ps(color_dx, negativeX));
		
		adr += -x;
		width -= -x;
		x = 0;
	}
	if (x+width > framebufferWidth)
	{
		if (RENDERER && !USELINEHACK && framebufferWidth == GPU_FRAMEBUFFER_NATIVE_WIDTH)
		{
			printf("rasterizer rendering at x=%d! oops!\n",x+width-1);
			return;
		}
		width = framebufferWidth - x;
	}
	
	CACHE_ALIGN float coord_s[4];
	
	while (width-- > 0)
	{
		_mm_store_ps(coord_s, coord);
		
		this->_pixel_SSE2<ISFRONTFACING, ISSHADOWPOLYGON>(polyAttr, isTranslucent, adr, dstColor[adr], color, coord_s[0], coord_s[1], coord_s[2], 1.0f/coord_s[3]);
		adr++;
		x++;
		
		coord = _mm_add_ps(coord, coord_dx);
		color = _mm_add_ps(color, color_dx);
	}
}

#endif // ENABLE_SSE2

//runs several scanlines, until an edge is finished
template<bool RENDERER> template<bool SLI, bool ISFRONTFACING, bool ISSHADOWPOLYGON, bool USELINEHACK>
void RasterizerUnit<RENDERER>::_runscanlines(const POLYGON_ATTR polyAttr, const bool isTranslucent, FragmentColor *dstColor, const size_t framebufferWidth, const size_t framebufferHeight, const bool isHorizontal, edge_fx_fl *left, edge_fx_fl *right)
{
	//oh lord, hack city for edge drawing

	//do not overstep either of the edges
	int Height = min(left->Height,right->Height);
	bool first = true;

	//HACK: special handling for horizontal line poly
	if ( USELINEHACK && (left->Height == 0) && (right->Height == 0) && (left->Y < framebufferHeight) && (left->Y >= 0) )
	{
		const bool draw = ( !SLI || ((left->Y >= this->_SLI_startLine) && (left->Y < this->_SLI_endLine)) );
		if (draw)
		{
#ifdef ENABLE_SSE2
			this->_drawscanline_SSE2<ISFRONTFACING, ISSHADOWPOLYGON, USELINEHACK>(polyAttr, isTranslucent, dstColor, framebufferWidth, framebufferHeight, left, right);
#else
			this->_drawscanline<ISFRONTFACING, ISSHADOWPOLYGON, USELINEHACK>(polyAttr, isTranslucent, dstColor, framebufferWidth, framebufferHeight, left, right);
#endif
		}
	}

	while (Height--)
	{
		const bool draw = ( !SLI || ((left->Y >= this->_SLI_startLine) && (left->Y < this->_SLI_endLine)) );
		if (draw)
		{
#ifdef ENABLE_SSE2
			this->_drawscanline_SSE2<ISFRONTFACING, ISSHADOWPOLYGON, USELINEHACK>(polyAttr, isTranslucent, dstColor, framebufferWidth, framebufferHeight, left, right);
#else
			this->_drawscanline<ISFRONTFACING, ISSHADOWPOLYGON, USELINEHACK>(polyAttr, isTranslucent, dstColor, framebufferWidth, framebufferHeight, left, right);
#endif
		}
		
		const int xl = left->X;
		const int xr = right->X;
		const int y = left->Y;
		left->Step();
		right->Step();

		if (!RENDERER && _debug_thisPoly)
		{
			//debug drawing
			bool top = (isHorizontal && first);
			bool bottom = (!Height && isHorizontal);
			if (Height || top || bottom)
			{
				if (draw)
				{
					int nxl = left->X;
					int nxr = right->X;
					if (top)
					{
						int xs = min(xl,xr);
						int xe = max(xl,xr);
						for (int x = xs; x <= xe; x++)
						{
							int adr = (y*framebufferWidth)+x;
							dstColor[adr].r = 63;
							dstColor[adr].g = 0;
							dstColor[adr].b = 0;
						}
					}
					else if (bottom)
					{
						int xs = min(xl,xr);
						int xe = max(xl,xr);
						for (int x = xs; x <= xe; x++)
						{
							int adr = (y*framebufferWidth)+x;
							dstColor[adr].r = 63;
							dstColor[adr].g = 0;
							dstColor[adr].b = 0;
						}
					}
					else
					{
						int xs = min(xl,nxl);
						int xe = max(xl,nxl);
						for (int x = xs; x <= xe; x++)
						{
							int adr = (y*framebufferWidth)+x;
							dstColor[adr].r = 63;
							dstColor[adr].g = 0;
							dstColor[adr].b = 0;
						}
						xs = min(xr,nxr);
						xe = max(xr,nxr);
						for (int x = xs; x <= xe; x++)
						{
							int adr = (y*framebufferWidth)+x;
							dstColor[adr].r = 63;
							dstColor[adr].g = 0;
							dstColor[adr].b = 0;
						}
					}

				}
			}
			first = false;
		}
	}
}


//rotates verts counterclockwise
template<bool RENDERER> template<int TYPE>
FORCEINLINE void RasterizerUnit<RENDERER>::_rot_verts()
{
	#define ROTSWAP(X) if(TYPE>X) swap(this->_verts[X-1],this->_verts[X]);
	ROTSWAP(1); ROTSWAP(2); ROTSWAP(3); ROTSWAP(4);
	ROTSWAP(5); ROTSWAP(6); ROTSWAP(7); ROTSWAP(8); ROTSWAP(9);
}

//rotate verts until vert0.y is minimum, and then vert0.x is minimum in case of ties
//this is a necessary precondition for our shape engine
template<bool RENDERER> template<bool ISFRONTFACING, int TYPE>
void RasterizerUnit<RENDERER>::_sort_verts()
{
	//if the verts are backwards, reorder them first
	//
	// At least... that's what the last comment says. But historically, we've
	// always been using front-facing polygons the entire time, and so the
	// comment should actually read, "if the verts are front-facing, reorder
	// them first". So what is the real behavior for this? - rogerman, 2018/08/01
	if (ISFRONTFACING)
		for (size_t i = 0; i < TYPE/2; i++)
			swap(this->_verts[i],this->_verts[TYPE-i-1]);

	for (;;)
	{
		//this was the only way we could get this to unroll
		#define CHECKY(X) if(TYPE>X) if(this->_verts[0]->y > this->_verts[X]->y) goto doswap;
		CHECKY(1); CHECKY(2); CHECKY(3); CHECKY(4);
		CHECKY(5); CHECKY(6); CHECKY(7); CHECKY(8); CHECKY(9);
		break;
		
	doswap:
		this->_rot_verts<TYPE>();
	}
	
	while (this->_verts[0]->y == this->_verts[1]->y && this->_verts[0]->x > this->_verts[1]->x)
	{
		this->_rot_verts<TYPE>();
		// hack for VC++ 2010 (bug in compiler optimization?)
		// freeze on 3D
		// TODO: study it
		#ifdef SLEEP_HACK_2011
			Sleep(0); // nop
		#endif
	}
	
}

//This function can handle any convex N-gon up to octagons
//verts must be clockwise.
//I didnt reference anything for this algorithm but it seems like I've seen it somewhere before.
//Maybe it is like crow's algorithm
template<bool RENDERER> template<bool SLI, bool ISFRONTFACING, bool ISSHADOWPOLYGON, bool USELINEHACK>
void RasterizerUnit<RENDERER>::_shape_engine(const POLYGON_ATTR polyAttr, const bool isTranslucent, FragmentColor *dstColor, const size_t framebufferWidth, const size_t framebufferHeight, int type)
{
	bool failure = false;

	switch (type)
	{
		case 3: this->_sort_verts<ISFRONTFACING, 3>(); break;
		case 4: this->_sort_verts<ISFRONTFACING, 4>(); break;
		case 5: this->_sort_verts<ISFRONTFACING, 5>(); break;
		case 6: this->_sort_verts<ISFRONTFACING, 6>(); break;
		case 7: this->_sort_verts<ISFRONTFACING, 7>(); break;
		case 8: this->_sort_verts<ISFRONTFACING, 8>(); break;
		case 9: this->_sort_verts<ISFRONTFACING, 9>(); break;
		case 10: this->_sort_verts<ISFRONTFACING, 10>(); break;
		default: printf("skipping type %d\n", type); return;
	}

	//we are going to step around the polygon in both directions starting from vert 0.
	//right edges will be stepped over clockwise and left edges stepped over counterclockwise.
	//these variables track that stepping, but in order to facilitate wrapping we start extra high
	//for the counter we're decrementing.
	int lv = type, rv = 0;

	edge_fx_fl left, right;
	bool step_left = true, step_right = true;
	for (;;)
	{
		//generate new edges if necessary. we must avoid regenerating edges when they are incomplete
		//so that they can be continued on down the shape
		assert(rv != type);
		int _lv = (lv == type) ? 0 : lv; //make sure that we ask for vert 0 when the variable contains the starting value
		if (step_left) left = edge_fx_fl(_lv,lv-1,(VERT**)&this->_verts, failure);
		if (step_right) right = edge_fx_fl(rv,rv+1,(VERT**)&this->_verts, failure);
		step_left = step_right = false;

		//handle a failure in the edge setup due to nutty polys
		if (failure)
			return;

		const bool isHorizontal = (left.Y == right.Y);
		this->_runscanlines<SLI, ISFRONTFACING, ISSHADOWPOLYGON, USELINEHACK>(polyAttr, isTranslucent, dstColor, framebufferWidth, framebufferHeight, isHorizontal, &left, &right);
		
		//if we ran out of an edge, step to the next one
		if (right.Height == 0)
		{
			step_right = true;
			rv++;
		}
		if (left.Height == 0)
		{
			step_left = true;
			lv--;
		}

		//this is our completion condition: when our stepped edges meet in the middle
		if (lv <= rv+1) break;
	}
}

template<bool RENDERER>
void RasterizerUnit<RENDERER>::SetSLI(u32 startLine, u32 endLine, bool debug)
{
	this->_debug_thisPoly = debug;
	this->_SLI_startLine = startLine;
	this->_SLI_endLine = endLine;
}

template<bool RENDERER>
void RasterizerUnit<RENDERER>::SetRenderer(SoftRasterizerRenderer *theRenderer)
{
	this->_softRender = theRenderer;
}

template<bool RENDERER> template <bool SLI, bool USELINEHACK>
FORCEINLINE void RasterizerUnit<RENDERER>::Render()
{
	const size_t polyCount = this->_softRender->GetClippedPolyCount();
	if (polyCount == 0)
	{
		return;
	}
	
	FragmentColor *dstColor = this->_softRender->GetFramebuffer();
	const size_t dstWidth = this->_softRender->GetFramebufferWidth();
	const size_t dstHeight = this->_softRender->GetFramebufferHeight();
	
	const CPoly &firstClippedPoly = this->_softRender->GetClippedPolyByIndex(0);
	const POLY &firstPoly = *firstClippedPoly.poly;
	POLYGON_ATTR polyAttr = firstPoly.attribute;
	TEXIMAGE_PARAM lastTexParams = firstPoly.texParam;
	u32 lastTexPalette = firstPoly.texPalette;
	
	this->_SetupTexture(firstPoly, 0);

	//iterate over polys
	for (size_t i = 0; i < polyCount; i++)
	{
		if (!RENDERER) _debug_thisPoly = (i == this->_softRender->_debug_drawClippedUserPoly);
		if (!this->_softRender->polyVisible[i]) continue;
		this->_polynum = i;

		const CPoly &clippedPoly = this->_softRender->GetClippedPolyByIndex(i);
		const POLY &thePoly = *clippedPoly.poly;
		const int vertCount = clippedPoly.type;
		const bool useLineHack = USELINEHACK && (thePoly.vtxFormat & 4);
		
		polyAttr = thePoly.attribute;
		const bool isTranslucent = thePoly.isTranslucent();
		
		if (lastTexParams.value != thePoly.texParam.value || lastTexPalette != thePoly.texPalette)
		{
			lastTexParams = thePoly.texParam;
			lastTexPalette = thePoly.texPalette;
			this->_SetupTexture(thePoly, i);
		}
		
		for (size_t j = 0; j < vertCount; j++)
			this->_verts[j] = &clippedPoly.clipVerts[j];
		for (size_t j = vertCount; j < MAX_CLIPPED_VERTS; j++)
			this->_verts[j] = NULL;
		
		if (!this->_softRender->polyBackfacing[i])
		{
			if (polyAttr.Mode == POLYGON_MODE_SHADOW)
			{
				if (useLineHack)
				{
					this->_shape_engine<SLI, true, true, true>(polyAttr, isTranslucent, dstColor, dstWidth, dstHeight, vertCount);
				}
				else
				{
					this->_shape_engine<SLI, true, true, false>(polyAttr, isTranslucent, dstColor, dstWidth, dstHeight, vertCount);
				}
			}
			else
			{
				if (useLineHack)
				{
					this->_shape_engine<SLI, true, false, true>(polyAttr, isTranslucent, dstColor, dstWidth, dstHeight, vertCount);
				}
				else
				{
					this->_shape_engine<SLI, true, false, false>(polyAttr, isTranslucent, dstColor, dstWidth, dstHeight, vertCount);
				}
			}
		}
		else
		{
			if (polyAttr.Mode == POLYGON_MODE_SHADOW)
			{
				if (useLineHack)
				{
					this->_shape_engine<SLI, false, true, true>(polyAttr, isTranslucent, dstColor, dstWidth, dstHeight, vertCount);
				}
				else
				{
					this->_shape_engine<SLI, false, true, false>(polyAttr, isTranslucent, dstColor, dstWidth, dstHeight, vertCount);
				}
			}
			else
			{
				if (useLineHack)
				{
					this->_shape_engine<SLI, false, false, true>(polyAttr, isTranslucent, dstColor, dstWidth, dstHeight, vertCount);
				}
				else
				{
					this->_shape_engine<SLI, false, false, false>(polyAttr, isTranslucent, dstColor, dstWidth, dstHeight, vertCount);
				}
			}
		}
	}
}

template <bool USELINEHACK>
void* _HACK_Viewer_ExecUnit(void *arg)
{
	RasterizerUnit<false> *unit = (RasterizerUnit<false> *)arg;
	unit->Render<false, USELINEHACK>();
	
	return 0;
}

template <bool USELINEHACK>
void* SoftRasterizer_RunRasterizerUnit(void *arg)
{
	RasterizerUnit<true> *unit = (RasterizerUnit<true> *)arg;
	unit->Render<true, USELINEHACK>();
	
	return 0;
}

static void* SoftRasterizer_RunCalculateVertices(void *arg)
{
	SoftRasterizerRenderer *softRender = (SoftRasterizerRenderer *)arg;
	softRender->performViewportTransforms();
	softRender->performBackfaceTests();
	softRender->performCoordAdjustment();
	
	return NULL;
}

static void* SoftRasterizer_RunGetAndLoadAllTextures(void *arg)
{
	SoftRasterizerRenderer *softRender = (SoftRasterizerRenderer *)arg;
	softRender->GetAndLoadAllTextures();
	
	return NULL;
}

static void* SoftRasterizer_RunUpdateTables(void *arg)
{
	SoftRasterizerRenderer *softRender = (SoftRasterizerRenderer *)arg;
	softRender->UpdateToonTable(softRender->currentRenderState->u16ToonTable);
	softRender->UpdateFogTable(softRender->currentRenderState->fogDensityTable);
	softRender->UpdateEdgeMarkColorTable(softRender->currentRenderState->edgeMarkColorTable);
	
	return NULL;
}

static void* SoftRasterizer_RunClearFramebuffer(void *arg)
{
	SoftRasterizerRenderer *softRender = (SoftRasterizerRenderer *)arg;
	softRender->ClearFramebuffer(*softRender->currentRenderState);
	
	return NULL;
}

static void* SoftRasterizer_RunRenderEdgeMarkAndFog(void *arg)
{
	SoftRasterizerPostProcessParams *params = (SoftRasterizerPostProcessParams *)arg;
	params->renderer->RenderEdgeMarkingAndFog(*params);
	
	return NULL;
}

static void* SoftRasterizer_RunClearUsingValues(void *arg)
{
	SoftRasterizerClearParam *param = (SoftRasterizerClearParam *)arg;
	param->renderer->ClearUsingValues_Execute(param->startPixel, param->endPixel);
	
	return NULL;
}

static Render3D* SoftRasterizerRendererCreate()
{
#if defined(ENABLE_AVX2)
	return new SoftRasterizerRenderer_AVX2;
#elif defined(ENABLE_SSE2)
	return new SoftRasterizerRenderer_SSE2;
#elif defined(ENABLE_ALTIVEC)
	return new SoftRasterizerRenderer_Altivec;
#else
	return new SoftRasterizerRenderer;
#endif
}

static void SoftRasterizerRendererDestroy()
{
	if (CurrentRenderer != BaseRenderer)
	{
#if defined(ENABLE_AVX2)
		SoftRasterizerRenderer_AVX2 *oldRenderer = (SoftRasterizerRenderer_AVX2 *)CurrentRenderer;
#elif defined(ENABLE_SSE2)
		SoftRasterizerRenderer_SSE2 *oldRenderer = (SoftRasterizerRenderer_SSE2 *)CurrentRenderer;
#elif defined(ENABLE_ALTIVEC)
		SoftRasterizerRenderer_Altivec *oldRenderer = (SoftRasterizerRenderer_Altivec *)CurrentRenderer;
#else
		SoftRasterizerRenderer *oldRenderer = (SoftRasterizerRenderer *)CurrentRenderer;
#endif
		
		CurrentRenderer = BaseRenderer;
		delete oldRenderer;
	}
}

SoftRasterizerTexture::SoftRasterizerTexture(TEXIMAGE_PARAM texAttributes, u32 palAttributes) : Render3DTexture(texAttributes, palAttributes)
{
	_cacheSize = GetUnpackSizeUsingFormat(TexFormat_15bpp);
	_unpackData = (u32 *)malloc_alignedCacheLine(_cacheSize);
	
	_customBuffer = NULL;
	
	_renderData = _unpackData;
	_renderWidth = _sizeS;
	_renderHeight = _sizeT;
	_renderWidthMask = _renderWidth - 1;
	_renderHeightMask = _renderHeight - 1;
	_renderWidthShift = 0;
	
	_deposterizeSrcSurface.Surface = (unsigned char *)_unpackData;
	
	u32 tempWidth = _renderWidth;
	while ( (tempWidth & 1) == 0)
	{
		tempWidth >>= 1;
		_renderWidthShift++;
	}
}

SoftRasterizerTexture::~SoftRasterizerTexture()
{
	free_aligned(this->_unpackData);
	free_aligned(this->_deposterizeDstSurface.Surface);
	free_aligned(this->_customBuffer);
}

FORCEINLINE void SoftRasterizerTexture::_clamp(s32 &val, const int size, const s32 sizemask) const
{
	if(val<0) val = 0;
	if(val>sizemask) val = sizemask;
}

FORCEINLINE void SoftRasterizerTexture::_hclamp(s32 &val) const
{
	_clamp(val, this->_renderWidth, this->_renderWidthMask);
}

FORCEINLINE void SoftRasterizerTexture::_vclamp(s32 &val) const
{
	_clamp(val, this->_renderHeight, this->_renderHeightMask);
}

FORCEINLINE void SoftRasterizerTexture::_repeat(s32 &val, const int size, const s32 sizemask) const
{
	val &= sizemask;
}

FORCEINLINE void SoftRasterizerTexture::_hrepeat(s32 &val) const
{
	_repeat(val, this->_renderWidth, this->_renderWidthMask);
}

FORCEINLINE void SoftRasterizerTexture::_vrepeat(s32 &val) const
{
	_repeat(val, this->_renderHeight, this->_renderHeightMask);
}

FORCEINLINE void SoftRasterizerTexture::_flip(s32 &val, const int size, const s32 sizemask) const
{
	val &= ((size<<1)-1);
	if(val>=size) val = (size<<1)-val-1;
}

FORCEINLINE void SoftRasterizerTexture::_hflip(s32 &val) const
{
	_flip(val, this->_renderWidth, this->_renderWidthMask);
}

FORCEINLINE void SoftRasterizerTexture::_vflip(s32 &val) const
{
	_flip(val, this->_renderHeight, this->_renderHeightMask);
}

void SoftRasterizerTexture::Load()
{
	if (this->_scalingFactor == 1 && !this->_useDeposterize)
	{
		this->Unpack<TexFormat_15bpp>((u32 *)this->_renderData);
	}
	else
	{
		u32 *textureSrc = this->_unpackData;
		
		this->Unpack<TexFormat_32bpp>(textureSrc);
		
		if (this->_useDeposterize)
		{
			RenderDeposterize(this->_deposterizeSrcSurface, this->_deposterizeDstSurface);
			textureSrc = (u32 *)this->_deposterizeDstSurface.Surface;
		}
		
		switch (this->_scalingFactor)
		{
			case 2:
				this->_Upscale<2>(textureSrc, this->_customBuffer);
				break;
				
			case 4:
				this->_Upscale<4>(textureSrc, this->_customBuffer);
				break;
				
			default:
				break;
		}
		
		ColorspaceConvertBuffer8888To6665<false, false>(this->_renderData, this->_renderData, this->_renderWidth * this->_renderHeight);
	}
	
	this->_isLoadNeeded = false;
}

u32* SoftRasterizerTexture::GetUnpackData()
{
	return this->_unpackData;
}

u32* SoftRasterizerTexture::GetRenderData()
{
	return this->_renderData;
}

s32 SoftRasterizerTexture::GetRenderWidth() const
{
	return this->_renderWidth;
}

s32 SoftRasterizerTexture::GetRenderHeight() const
{
	return this->_renderHeight;
}

s32 SoftRasterizerTexture::GetRenderWidthMask() const
{
	return this->_renderWidthMask;
}

s32 SoftRasterizerTexture::GetRenderHeightMask() const
{
	return this->_renderHeightMask;
}

u32 SoftRasterizerTexture::GetRenderWidthShift() const
{
	return this->_renderWidthShift;
}

FORCEINLINE void SoftRasterizerTexture::GetRenderSamplerCoordinates(const u8 wrapMode, s32 &iu, s32 &iv) const
{
	switch (wrapMode)
	{
		//flip none
		case 0x0: _hclamp(iu);  _vclamp(iv);  break;
		case 0x1: _hrepeat(iu); _vclamp(iv);  break;
		case 0x2: _hclamp(iu);  _vrepeat(iv); break;
		case 0x3: _hrepeat(iu); _vrepeat(iv); break;
		//flip S
		case 0x4: _hclamp(iu);  _vclamp(iv);  break;
		case 0x5: _hflip(iu);   _vclamp(iv);  break;
		case 0x6: _hclamp(iu);  _vrepeat(iv); break;
		case 0x7: _hflip(iu);   _vrepeat(iv); break;
		//flip T
		case 0x8: _hclamp(iu);  _vclamp(iv);  break;
		case 0x9: _hrepeat(iu); _vclamp(iv);  break;
		case 0xA: _hclamp(iu);  _vflip(iv);   break;
		case 0xB: _hrepeat(iu); _vflip(iv);   break;
		//flip both
		case 0xC: _hclamp(iu);  _vclamp(iv);  break;
		case 0xD: _hflip(iu);   _vclamp(iv);  break;
		case 0xE: _hclamp(iu);  _vflip(iv);   break;
		case 0xF: _hflip(iu);   _vflip(iv);   break;
	}
}

void SoftRasterizerTexture::SetUseDeposterize(bool willDeposterize)
{
	this->_useDeposterize = willDeposterize;
	
	if ( (this->_deposterizeDstSurface.Surface == NULL) && willDeposterize )
	{
		this->_deposterizeDstSurface.Surface = (unsigned char *)malloc_alignedCacheLine(this->_cacheSize * 2);
		this->_deposterizeDstSurface.workingSurface[0] = this->_deposterizeDstSurface.Surface + this->_cacheSize;
	}
	else if ( (this->_deposterizeDstSurface.Surface != NULL) && !willDeposterize )
	{
		free_aligned(this->_deposterizeDstSurface.Surface);
		this->_deposterizeDstSurface.Surface = NULL;
	}
	
	if (this->_scalingFactor == 1)
	{
		if (this->_useDeposterize)
		{
			this->_renderData = (u32 *)this->_deposterizeDstSurface.Surface;
		}
		else
		{
			this->_renderData = this->_unpackData;
		}
	}
	else
	{
		this->_renderData = this->_customBuffer;
	}
}

void SoftRasterizerTexture::SetScalingFactor(size_t scalingFactor)
{
	if (  (scalingFactor != 2) && (scalingFactor != 4) )
	{
		scalingFactor = 1;
	}
	
	u32 newWidth = this->_sizeS * scalingFactor;
	u32 newHeight = this->_sizeT * scalingFactor;
	
	if (this->_renderWidth != newWidth || this->_renderHeight != newHeight)
	{
		u32 *oldBuffer = this->_customBuffer;
		this->_customBuffer = (u32 *)malloc_alignedCacheLine(newWidth * newHeight * sizeof(u32));
		free_aligned(oldBuffer);
	}
	
	this->_scalingFactor = scalingFactor;
	this->_renderWidth = newWidth;
	this->_renderHeight = newHeight;
	this->_renderWidthMask = newWidth - 1;
	this->_renderHeightMask = newHeight - 1;
	this->_renderWidthShift = 0;
	
	u32 tempWidth = newWidth;
	while ( (tempWidth & 1) == 0)
	{
		tempWidth >>= 1;
		this->_renderWidthShift++;
	}
	
	if (this->_scalingFactor == 1)
	{
		if (this->_useDeposterize)
		{
			this->_renderData = (u32 *)this->_deposterizeDstSurface.Surface;
		}
		else
		{
			this->_renderData = this->_unpackData;
		}
	}
	else
	{
		this->_renderData = this->_customBuffer;
	}
}

GPU3DInterface gpu3DRasterize = {
	"SoftRasterizer",
	SoftRasterizerRendererCreate,
	SoftRasterizerRendererDestroy
};

SoftRasterizerRenderer::SoftRasterizerRenderer()
{
	_deviceInfo.renderID = RENDERID_SOFTRASTERIZER;
	_deviceInfo.renderName = "SoftRasterizer";
	_deviceInfo.isTexturingSupported = true;
	_deviceInfo.isEdgeMarkSupported = true;
	_deviceInfo.isFogSupported = true;
	_deviceInfo.isTextureSmoothingSupported = false;
	_deviceInfo.maxAnisotropy = 1.0f;
	_deviceInfo.maxSamples = 0;
	
	_task = NULL;
	
	_debug_drawClippedUserPoly = -1;
	
	_renderGeometryNeedsFinish = false;
	_framebufferAttributes = NULL;
	
	_enableHighPrecisionColorInterpolation = CommonSettings.GFX3D_HighResolutionInterpolateColor;
	_enableLineHack = CommonSettings.GFX3D_LineHack;
	_enableFragmentSamplingHack = CommonSettings.GFX3D_TXTHack;
	
	_HACK_viewer_rasterizerUnit.SetSLI(0, _framebufferHeight, false);
	
	const size_t coreCount = CommonSettings.num_cores;
	_threadCount = coreCount;
	
	if (_threadCount > SOFTRASTERIZER_MAX_THREADS)
	{
		_threadCount = SOFTRASTERIZER_MAX_THREADS;
	}
	
	if (_threadCount < 2)
	{
		_threadCount = 0;
		
		_nativeLinesPerThread = GPU_FRAMEBUFFER_NATIVE_HEIGHT;
		_nativePixelsPerThread = GPU_FRAMEBUFFER_NATIVE_WIDTH * GPU_FRAMEBUFFER_NATIVE_HEIGHT;
		_customLinesPerThread = _framebufferHeight;
		_customPixelsPerThread = _framebufferPixCount;
		
		_threadPostprocessParam[0].renderer = this;
		_threadPostprocessParam[0].startLine = 0;
		_threadPostprocessParam[0].endLine = _framebufferHeight;
		_threadPostprocessParam[0].enableEdgeMarking = true;
		_threadPostprocessParam[0].enableFog = true;
		_threadPostprocessParam[0].fogColor = 0x80FFFFFF;
		_threadPostprocessParam[0].fogAlphaOnly = false;
		
		_threadClearParam[0].renderer = this;
		_threadClearParam[0].startPixel = 0;
		_threadClearParam[0].endPixel = _framebufferPixCount;
		
		_rasterizerUnit[0].SetSLI(_threadPostprocessParam[0].startLine, _threadPostprocessParam[0].endLine, false);
		_rasterizerUnit[0].SetRenderer(this);
	}
	else
	{
		_task = new Task[_threadCount];
		
		_nativeLinesPerThread = GPU_FRAMEBUFFER_NATIVE_HEIGHT / _threadCount;
		_nativePixelsPerThread = (GPU_FRAMEBUFFER_NATIVE_WIDTH * GPU_FRAMEBUFFER_NATIVE_HEIGHT)  / _threadCount;
		_customLinesPerThread = _framebufferHeight / _threadCount;
		_customPixelsPerThread = _framebufferPixCount / _threadCount;
		
		for (size_t i = 0; i < _threadCount; i++)
		{
			_threadPostprocessParam[i].renderer = this;
			_threadPostprocessParam[i].startLine = i * _customLinesPerThread;
			_threadPostprocessParam[i].endLine = (i < _threadCount - 1) ? (i + 1) * _customLinesPerThread : _framebufferHeight;
			_threadPostprocessParam[i].enableEdgeMarking = true;
			_threadPostprocessParam[i].enableFog = true;
			_threadPostprocessParam[i].fogColor = 0x80FFFFFF;
			_threadPostprocessParam[i].fogAlphaOnly = false;
			
			_threadClearParam[i].renderer = this;
			_threadClearParam[i].startPixel = i * _customPixelsPerThread;
			_threadClearParam[i].endPixel = (i < _threadCount - 1) ? (i + 1) * _customPixelsPerThread : _framebufferPixCount;
			
			_rasterizerUnit[i].SetSLI(_threadPostprocessParam[i].startLine, _threadPostprocessParam[i].endLine, false);
			_rasterizerUnit[i].SetRenderer(this);
			
#ifdef DESMUME_COCOA
			// The Cocoa port takes advantage of hand-optimized thread priorities
			// to help stabilize performance when running SoftRasterizer.
			_task[i].start(false, 43);
#else
			_task[i].start(false);
#endif
		}
	}
	
	InitTables();
	Reset();
	
	if (_threadCount == 0)
	{
		printf("SoftRasterizer: Running directly on the emulation thread. (Multithreading disabled.)\n");
	}
	else
	{
		printf("SoftRasterizer: Running using %d additional %s. (Multithreading enabled.)\n",
			   (int)_threadCount, (_threadCount == 1) ? "thread" : "threads");
	}
}

SoftRasterizerRenderer::~SoftRasterizerRenderer()
{
	for (size_t i = 0; i < this->_threadCount; i++)
	{
		this->_task[i].finish();
		this->_task[i].shutdown();
	}
	
	delete[] this->_task;
	this->_task = NULL;
	
	delete this->_framebufferAttributes;
	this->_framebufferAttributes = NULL;
}

Render3DError SoftRasterizerRenderer::InitTables()
{
	static bool needTableInit = true;
	
	if (needTableInit)
	{
		for (size_t i = 0; i < 64; i++)
		{
			for (size_t j = 0; j < 64; j++)
			{
				modulate_table[i][j] = ((i+1) * (j+1) - 1) >> 6;
				for (size_t a = 0; a < 32; a++)
					decal_table[a][i][j] = ((i*a) + (j*(31-a))) >> 5;
			}
		}
		
		needTableInit = false;
	}
	
	return RENDER3DERROR_NOERR;
}

ClipperMode SoftRasterizerRenderer::GetPreferredPolygonClippingMode() const
{
	return (this->_enableHighPrecisionColorInterpolation) ? ClipperMode_FullColorInterpolate : ClipperMode_Full;
}

void SoftRasterizerRenderer::performViewportTransforms()
{
	const float wScalar = (float)this->_framebufferWidth  / (float)GPU_FRAMEBUFFER_NATIVE_WIDTH;
	const float hScalar = (float)this->_framebufferHeight / (float)GPU_FRAMEBUFFER_NATIVE_HEIGHT;
	
	//viewport transforms
	for (size_t i = 0; i < this->_clippedPolyCount; i++)
	{
		CPoly &poly = this->_clippedPolyList[i];
		for (size_t j = 0; j < poly.type; j++)
		{
			VERT &vert = poly.clipVerts[j];
			
			// TODO: Possible divide by zero with the w-coordinate.
			// Is the vertex being read correctly? Is 0 a valid value for w?
			// If both of these questions answer to yes, then how does the NDS handle a NaN?
			// For now, simply prevent w from being zero.
			//
			// Test case: Dance scenes in Princess Debut can generate undefined vertices
			// when the -ffast-math option (relaxed IEEE754 compliance) is used.
			const float vertw = (vert.coord[3] != 0.0f) ? vert.coord[3] : 0.00000001f;
			
			//homogeneous divide
			vert.coord[0] = (vert.coord[0]+vertw) / (2*vertw);
			vert.coord[1] = (vert.coord[1]+vertw) / (2*vertw);
			vert.coord[2] = (vert.coord[2]+vertw) / (2*vertw);
			vert.texcoord[0] /= vertw;
			vert.texcoord[1] /= vertw;
			
			//CONSIDER: do we need to guarantee that these are in bounds? perhaps not.
			//vert.coord[0] = max(0.0f,min(1.0f,vert.coord[0]));
			//vert.coord[1] = max(0.0f,min(1.0f,vert.coord[1]));
			//vert.coord[2] = max(0.0f,min(1.0f,vert.coord[2]));
			
			//perspective-correct the colors
			vert.fcolor[0] /= vertw;
			vert.fcolor[1] /= vertw;
			vert.fcolor[2] /= vertw;
			
			//viewport transformation
			VIEWPORT viewport;
			viewport.decode(poly.poly->viewport);
			vert.coord[0] *= viewport.width;
			vert.coord[0] += viewport.x;
			
			// The maximum viewport y-value is 191. Values above 191 need to wrap
			// around and go negative.
			//
			// Test case: The Homie Rollerz character select screen sets the y-value
			// to 253, which then wraps around to -2.
			vert.coord[1] *= viewport.height;
			vert.coord[1] += (viewport.y > 191) ? (viewport.y - 0xFF) : viewport.y;
			vert.coord[1] = 192 - vert.coord[1];

			vert.coord[0] *= wScalar;
			vert.coord[1] *= hScalar;
		}
	}
}

void SoftRasterizerRenderer::performCoordAdjustment()
{
	for (size_t i = 0; i < this->_clippedPolyCount; i++)
	{
		CPoly &clippedPoly = this->_clippedPolyList[i];
		const PolygonType type = clippedPoly.type;
		VERT *verts = &clippedPoly.clipVerts[0];
		
		//here is a hack which needs to be removed.
		//at some point our shape engine needs these to be converted to "fixed point"
		//which is currently just a float
		for (size_t j = 0; j < type; j++)
			for (size_t k = 0; k < 2; k++)
				verts[j].coord[k] = (float)iround(16.0f * verts[j].coord[k]);
	}
}

void SoftRasterizerRenderer::GetAndLoadAllTextures()
{
	for (size_t i = 0; i < this->_clippedPolyCount; i++)
	{
		const CPoly &clippedPoly = this->_clippedPolyList[i];
		const POLY &thePoly = *clippedPoly.poly;
		
		//make sure all the textures we'll need are cached
		//(otherwise on a multithreaded system there will be multiple writers--
		//this SHOULD be read-only, although some day the texcache may collect statistics or something
		//and then it won't be safe.
		this->_textureList[i] = this->GetLoadedTextureFromPolygon(thePoly, this->_enableTextureSampling);
	}
}

void SoftRasterizerRenderer::performBackfaceTests()
{
	for (size_t i = 0; i < this->_clippedPolyCount; i++)
	{
		const CPoly &clippedPoly = this->_clippedPolyList[i];
		const POLY &thePoly = *clippedPoly.poly;
		const PolygonType type = clippedPoly.type;
		const VERT *verts = &clippedPoly.clipVerts[0];
		const u8 cullingMode = thePoly.attribute.SurfaceCullingMode;
		
		//HACK: backface culling
		//this should be moved to gfx3d, but first we need to redo the way the lists are built
		//because it is too convoluted right now.
		//(must we throw out verts if a poly gets backface culled? if not, then it might be easier)
		
		//an older approach
		//(not good enough for quads and other shapes)
		//float ab[2], ac[2]; Vector2Copy(ab, verts[1].coord); Vector2Copy(ac, verts[2].coord); Vector2Subtract(ab, verts[0].coord);
		//Vector2Subtract(ac, verts[0].coord); float cross = Vector2Cross(ab, ac); polyAttr.backfacing = (cross>0);
		
		//a better approach
		// we have to support somewhat non-convex polygons (see NSMB world map 1st screen).
		// this version should handle those cases better.
		const size_t n = type - 1;
		float facing = (verts[0].y + verts[n].y) * (verts[0].x - verts[n].x)
		+ (verts[1].y + verts[0].y) * (verts[1].x - verts[0].x)
		+ (verts[2].y + verts[1].y) * (verts[2].x - verts[1].x);
		for (size_t j = 2; j < n; j++)
			facing += (verts[j+1].y + verts[j].y) * (verts[j+1].x - verts[j].x);
		
		polyBackfacing[i] = (facing < 0);
		
		static const bool visibleFunction[2][4] = {
			//always false, backfacing, !backfacing, always true
			{ false, false, true, true },
			{ false, true, false, true }
		};
		
		polyVisible[i] = visibleFunction[polyBackfacing[i]][cullingMode];
	}
}

Render3DError SoftRasterizerRenderer::ApplyRenderingSettings(const GFX3D_State &renderState)
{
	this->_enableHighPrecisionColorInterpolation = CommonSettings.GFX3D_HighResolutionInterpolateColor;
	this->_enableLineHack = CommonSettings.GFX3D_LineHack;
	this->_enableFragmentSamplingHack = CommonSettings.GFX3D_TXTHack;
	
	return Render3D::ApplyRenderingSettings(renderState);
}

Render3DError SoftRasterizerRenderer::BeginRender(const GFX3D &engine)
{
	// Force all threads to finish before rendering with new data
	for (size_t i = 0; i < this->_threadCount; i++)
	{
		this->_task[i].finish();
	}
	
	// Keep the current render states for later use
	this->currentRenderState = (GFX3D_State *)&engine.renderState;
	this->_clippedPolyCount = engine.clippedPolyCount;
	this->_clippedPolyOpaqueCount = engine.clippedPolyOpaqueCount;
	this->_clippedPolyList = engine.clippedPolyList;
	
	const bool doMultithreadedStateSetup = (this->_threadCount >= 2);
	
	if (doMultithreadedStateSetup)
	{
		this->_task[0].execute(&SoftRasterizer_RunGetAndLoadAllTextures, this);
		this->_task[1].execute(&SoftRasterizer_RunCalculateVertices, this);
	}
	else
	{
		this->GetAndLoadAllTextures();
		this->performViewportTransforms();
		this->performBackfaceTests();
		this->performCoordAdjustment();
	}
	
	this->UpdateToonTable(engine.renderState.u16ToonTable);
	
	if (this->_enableEdgeMark)
	{
		this->UpdateEdgeMarkColorTable(this->currentRenderState->edgeMarkColorTable);
	}
	
	if (this->_enableFog)
	{
		this->UpdateFogTable(this->currentRenderState->fogDensityTable);
	}
	
	if (doMultithreadedStateSetup)
	{
		this->_task[1].finish();
		this->_task[0].finish();
	}
	
	this->ClearFramebuffer(engine.renderState);
	
	return RENDER3DERROR_NOERR;
}

Render3DError SoftRasterizerRenderer::RenderGeometry(const GFX3D_State &renderState, const POLYLIST *polyList, const INDEXLIST *indexList)
{
	// Render the geometry
	if (this->_threadCount > 0)
	{
		if (this->_enableLineHack)
		{
			for (size_t i = 0; i < this->_threadCount; i++)
			{
				this->_task[i].execute(&SoftRasterizer_RunRasterizerUnit<true>, &this->_rasterizerUnit[i]);
			}
		}
		else
		{
			for (size_t i = 0; i < this->_threadCount; i++)
			{
				this->_task[i].execute(&SoftRasterizer_RunRasterizerUnit<false>, &this->_rasterizerUnit[i]);
			}
		}
		
		this->_renderGeometryNeedsFinish = true;
	}
	else
	{
		if (this->_enableLineHack)
		{
			SoftRasterizer_RunRasterizerUnit<true>(&this->_rasterizerUnit[0]);
		}
		else
		{
			SoftRasterizer_RunRasterizerUnit<false>(&this->_rasterizerUnit[0]);
		}
		
		this->_renderGeometryNeedsFinish = false;
		texCache.Evict(); // Since we're finishing geometry rendering here and now, also check the texture cache now.
	}
	
	//	printf("rendered %d of %d polys after backface culling\n",gfx3d.polylist->count-culled,gfx3d.polylist->count);
	
	return RENDER3DERROR_NOERR;
}

Render3DError SoftRasterizerRenderer::UpdateEdgeMarkColorTable(const u16 *edgeMarkColorTable)
{
	//TODO: need to test and find out whether these get grabbed at flush time, or at render time
	//we can do this by rendering a 3d frame and then freezing the system, but only changing the edge mark colors
	for (size_t i = 0; i < 8; i++)
	{
		this->edgeMarkTable[i].color = COLOR555TO6665(edgeMarkColorTable[i] & 0x7FFF, (this->currentRenderState->enableAntialiasing) ? 0x10 : 0x1F);
		
		//zero 20-jun-2013 - this doesnt make any sense. at least, it should be related to the 0x8000 bit. if this is undocumented behaviour, lets write about which scenario proves it here, or which scenario is requiring this code.
		//// this seems to be the only thing that selectively disables edge marking
		//edgeMarkDisabled[i] = (col == 0x7FFF);
		this->edgeMarkDisabled[i] = false;
	}
	
	return RENDER3DERROR_NOERR;
}

Render3DError SoftRasterizerRenderer::UpdateFogTable(const u8 *fogDensityTable)
{
#if 0
	//TODO - this might be a little slow;
	//we might need to hash all the variables and only recompute this when something changes
	const int increment = (0x400 >> shift);
	for (u32 i = 0; i < 32768; i++)
	{
		if (i < offset)
		{
			fogTable[i] = densityTable[0];
			continue;
		}
		
		for (int j = 0; j < 32; j++)
		{
			u32 value = offset + increment*(j+1);
			if (i <= value)
			{
				if (j == 0)
				{
					fogTable[i] = densityTable[0];
					goto done;
				}
				else
				{
					fogTable[i] = ((value-i)*(densityTable[j-1]) + (increment-(value-i))*(densityTable[j]))/increment;
					goto done;
				}
			}
		}
		fogTable[i] = (densityTable[31]);
	done: ;
	}
#else
	// this should behave exactly the same as the previous loop,
	// except much faster. (because it's not a 2d loop and isn't so branchy either)
	// maybe it's fast enough to not need to be cached, now.
	const int increment = ((1 << 10) >> this->currentRenderState->fogShift);
	const int incrementDivShift = 10 - this->currentRenderState->fogShift;
	u32 fogOffset = min<u32>(max<u32>(this->currentRenderState->fogOffset, 0), 32768);
	u32 iMin = min<u32>(32768, (( 1 + 1) << incrementDivShift) + fogOffset + 1 - increment);
	u32 iMax = min<u32>(32768, ((32 + 1) << incrementDivShift) + fogOffset + 1 - increment);
	assert(iMin <= iMax);
	
	// If the fog factor is 127, then treat it as 128.
	u8 fogFactor = (fogDensityTable[0] == 127) ? 128 : fogDensityTable[0];
	memset(this->fogTable, fogFactor, iMin);
	
	for(u32 i = iMin; i < iMax; i++)
	{
		int num = (i - fogOffset + (increment-1));
		int j = (num >> incrementDivShift) - 1;
		u32 value = (num & ~(increment-1)) + fogOffset;
		u32 diff = value - i;
		assert(j >= 1 && j < 32);
		fogFactor = ((diff*(fogDensityTable[j-1]) + (increment-diff)*(fogDensityTable[j])) >> incrementDivShift);
		this->fogTable[i] = (fogFactor == 127) ? 128 : fogFactor;
	}
	
	fogFactor = (fogDensityTable[31] == 127) ? 128 : fogDensityTable[31];
	memset(this->fogTable+iMax, fogFactor, 32768-iMax);
#endif
	
	return RENDER3DERROR_NOERR;
}

Render3DError SoftRasterizerRenderer::RenderEdgeMarkingAndFog(const SoftRasterizerPostProcessParams &param)
{
	for (size_t i = param.startLine * this->_framebufferWidth, y = param.startLine; y < param.endLine; y++)
	{
		for (size_t x = 0; x < this->_framebufferWidth; x++, i++)
		{
			FragmentColor &dstColor = this->_framebufferColor[i];
			const u32 depth = this->_framebufferAttributes->depth[i];
			const u8 polyID = this->_framebufferAttributes->opaquePolyID[i];
			
			if (param.enableEdgeMarking)
			{
				// a good test case for edge marking is Sonic Rush:
				// - the edges are completely sharp/opaque on the very brief title screen intro,
				// - the level-start intro gets a pseudo-antialiasing effect around the silhouette,
				// - the character edges in-level are clearly transparent, and also show well through shield powerups.
				
				if (!this->edgeMarkDisabled[polyID>>3] && this->_framebufferAttributes->isTranslucentPoly[i] == 0)
				{
					const bool isEdgeMarkingClearValues = ((polyID != this->_clearAttributes.opaquePolyID) && (depth < this->_clearAttributes.depth));
					
					const bool right = (x >= this->_framebufferWidth-1)  ? isEdgeMarkingClearValues : ((polyID != this->_framebufferAttributes->opaquePolyID[i+1])                       && (depth >= this->_framebufferAttributes->depth[i+1]));
					const bool down  = (y >= this->_framebufferHeight-1) ? isEdgeMarkingClearValues : ((polyID != this->_framebufferAttributes->opaquePolyID[i+this->_framebufferWidth]) && (depth >= this->_framebufferAttributes->depth[i+this->_framebufferWidth]));
					const bool left  = (x < 1)                           ? isEdgeMarkingClearValues : ((polyID != this->_framebufferAttributes->opaquePolyID[i-1])                       && (depth >= this->_framebufferAttributes->depth[i-1]));
					const bool up    = (y < 1)                           ? isEdgeMarkingClearValues : ((polyID != this->_framebufferAttributes->opaquePolyID[i-this->_framebufferWidth]) && (depth >= this->_framebufferAttributes->depth[i-this->_framebufferWidth]));
					
					FragmentColor edgeMarkColor = this->edgeMarkTable[this->_framebufferAttributes->opaquePolyID[i] >> 3];
					
					if (right)
					{
						if (x < this->_framebufferWidth - 1)
						{
							edgeMarkColor = this->edgeMarkTable[this->_framebufferAttributes->opaquePolyID[i+1] >> 3];
						}
					}
					else if (down)
					{
						if (y < this->_framebufferHeight - 1)
						{
							edgeMarkColor = this->edgeMarkTable[this->_framebufferAttributes->opaquePolyID[i+this->_framebufferWidth] >> 3];
						}
					}
					else if (left)
					{
						if (x > 0)
						{
							edgeMarkColor = this->edgeMarkTable[this->_framebufferAttributes->opaquePolyID[i-1] >> 3];
						}
					}
					else if (up)
					{
						if (y > 0)
						{
							edgeMarkColor = this->edgeMarkTable[this->_framebufferAttributes->opaquePolyID[i-this->_framebufferWidth] >> 3];
						}
					}
					
					if (right || down || left || up)
					{
						EdgeBlend(dstColor, edgeMarkColor);
					}
				}
			}
			
			if (param.enableFog)
			{
				FragmentColor fogColor;
				fogColor.color = COLOR555TO6665( param.fogColor & 0x7FFF, (param.fogColor>>16) & 0x1F );
				
				const size_t fogIndex = depth >> 9;
				assert(fogIndex < 32768);
				const u8 fog = (this->_framebufferAttributes->isFogged[i] != 0) ? this->fogTable[fogIndex] : 0;
				
				if (!param.fogAlphaOnly)
				{
					dstColor.r = ( (128-fog)*dstColor.r + fogColor.r*fog ) >> 7;
					dstColor.g = ( (128-fog)*dstColor.g + fogColor.g*fog ) >> 7;
					dstColor.b = ( (128-fog)*dstColor.b + fogColor.b*fog ) >> 7;
				}
				
				dstColor.a = ( (128-fog)*dstColor.a + fogColor.a*fog ) >> 7;
			}
		}
	}
	
	return RENDER3DERROR_NOERR;
}

SoftRasterizerTexture* SoftRasterizerRenderer::GetLoadedTextureFromPolygon(const POLY &thePoly, bool enableTexturing)
{
	SoftRasterizerTexture *theTexture = (SoftRasterizerTexture *)texCache.GetTexture(thePoly.texParam, thePoly.texPalette);
	if (theTexture == NULL)
	{
		theTexture = new SoftRasterizerTexture(thePoly.texParam, thePoly.texPalette);
		texCache.Add(theTexture);
	}
	
	const NDSTextureFormat packFormat = theTexture->GetPackFormat();
	const bool isTextureEnabled = ( (packFormat != TEXMODE_NONE) && enableTexturing );
	
	theTexture->SetSamplingEnabled(isTextureEnabled);
	
	if (theTexture->IsLoadNeeded() && isTextureEnabled)
	{
		theTexture->SetUseDeposterize(this->_enableTextureDeposterize);
		theTexture->SetScalingFactor(this->_textureScalingFactor);
		theTexture->Load();
	}
	
	return theTexture;
}

Render3DError SoftRasterizerRenderer::UpdateToonTable(const u16 *toonTableBuffer)
{
	//convert the toon colors
	for (size_t i = 0; i < 32; i++)
	{
		this->toonColor32LUT[i].color = COLOR555TO666(toonTableBuffer[i] & 0x7FFF);
	}
	
	return RENDER3DERROR_NOERR;
}

Render3DError SoftRasterizerRenderer::ClearUsingImage(const u16 *__restrict colorBuffer, const u32 *__restrict depthBuffer, const u8 *__restrict fogBuffer, const u8 opaquePolyID)
{
	const size_t xRatio = (size_t)((GPU_FRAMEBUFFER_NATIVE_WIDTH << 16) / this->_framebufferWidth) + 1;
	const size_t yRatio = (size_t)((GPU_FRAMEBUFFER_NATIVE_HEIGHT << 16) / this->_framebufferHeight) + 1;
	
	for (size_t y = 0, iw = 0; y < this->_framebufferHeight; y++)
	{
		const size_t readLine = (size_t)(((y * yRatio) >> 16) * GPU_FRAMEBUFFER_NATIVE_WIDTH);
		
		for (size_t x = 0; x < this->_framebufferWidth; x++, iw++)
		{
			const size_t ir = readLine + ((x * xRatio) >> 16);
			
			this->_framebufferColor[iw].color = COLOR555TO6665(colorBuffer[ir] & 0x7FFF, (colorBuffer[ir] >> 15) * 0x1F);
			this->_framebufferAttributes->depth[iw] = depthBuffer[ir];
			this->_framebufferAttributes->isFogged[iw] = fogBuffer[ir];
			this->_framebufferAttributes->opaquePolyID[iw] = opaquePolyID;
			this->_framebufferAttributes->translucentPolyID[iw] = kUnsetTranslucentPolyID;
			this->_framebufferAttributes->isTranslucentPoly[iw] = 0;
			this->_framebufferAttributes->polyFacing[iw] = PolyFacing_Unwritten;
			this->_framebufferAttributes->stencil[iw] = 0;
		}
	}
	
	return RENDER3DERROR_NOERR;
}

void SoftRasterizerRenderer::ClearUsingValues_Execute(const size_t startPixel, const size_t endPixel)
{
	for (size_t i = startPixel; i < endPixel; i++)
	{
		this->_framebufferColor[i] = this->_clearColor6665;
		this->_framebufferAttributes->SetAtIndex(i, this->_clearAttributes);
	}
}

Render3DError SoftRasterizerRenderer::ClearUsingValues(const FragmentColor &clearColor6665, const FragmentAttributes &clearAttributes)
{
	const bool doMultithreadedClear = (this->_threadCount > 0);
	
	if (doMultithreadedClear)
	{
		for (size_t threadIndex = 0; threadIndex < this->_threadCount; threadIndex++)
		{
			this->_task[threadIndex].execute(&SoftRasterizer_RunClearUsingValues, &this->_threadClearParam[threadIndex]);
		}
	}
	else
	{
		this->ClearUsingValues_Execute(0, this->_framebufferPixCount);
	}
	
	if (doMultithreadedClear)
	{
		for (size_t threadIndex = 0; threadIndex < this->_threadCount; threadIndex++)
		{
			this->_task[threadIndex].finish();
		}
	}
	
	return RENDER3DERROR_NOERR;
}

Render3DError SoftRasterizerRenderer::Reset()
{
	if (this->_threadCount > 0)
	{
		for (size_t i = 0; i < this->_threadCount; i++)
		{
			this->_task[i].finish();
		}
	}
	
	this->_renderGeometryNeedsFinish = false;
	
	texCache.Reset();
	
	return RENDER3DERROR_NOERR;
}

Render3DError SoftRasterizerRenderer::Render(const GFX3D &engine)
{
	Render3DError error = RENDER3DERROR_NOERR;
	this->_isPoweredOn = true;
	
	const u32 clearColorSwapped = LE_TO_LOCAL_32(engine.renderState.clearColor);
	this->_clearColor6665.color = COLOR555TO6665(clearColorSwapped & 0x7FFF, (clearColorSwapped >> 16) & 0x1F);
	
	this->_clearAttributes.opaquePolyID = (clearColorSwapped >> 24) & 0x3F;
	//special value for uninitialized translucent polyid. without this, fires in spiderman2 dont display
	//I am not sure whether it is right, though. previously this was cleared to 0, as a guess,
	//but in spiderman2 some fires with polyid 0 try to render on top of the background
	this->_clearAttributes.translucentPolyID = kUnsetTranslucentPolyID;
	this->_clearAttributes.depth = engine.renderState.clearDepth;
	this->_clearAttributes.stencil = 0;
	this->_clearAttributes.isTranslucentPoly = 0;
	this->_clearAttributes.polyFacing = PolyFacing_Unwritten;
	this->_clearAttributes.isFogged = BIT15(clearColorSwapped);
	
	error = this->BeginRender(engine);
	if (error != RENDER3DERROR_NOERR)
	{
		return error;
	}
	
	this->RenderGeometry(engine.renderState, engine.polylist, &engine.indexlist);
	this->EndRender(engine.render3DFrameCount);
	
	return error;
}

Render3DError SoftRasterizerRenderer::EndRender(const u64 frameCount)
{
	// If we're not multithreaded, then just do the post-processing steps now.
	if (!this->_renderGeometryNeedsFinish)
	{
		if (this->_enableEdgeMark || this->_enableFog)
		{
			this->_threadPostprocessParam[0].enableEdgeMarking = this->_enableEdgeMark;
			this->_threadPostprocessParam[0].enableFog = this->_enableFog;
			this->_threadPostprocessParam[0].fogColor = this->currentRenderState->fogColor;
			this->_threadPostprocessParam[0].fogAlphaOnly = this->currentRenderState->enableFogAlphaOnly;
			
			this->RenderEdgeMarkingAndFog(this->_threadPostprocessParam[0]);
		}
	}
	
	return RENDER3DERROR_NOERR;
}

Render3DError SoftRasterizerRenderer::RenderFinish()
{
	if (!this->_renderNeedsFinish)
	{
		return RENDER3DERROR_NOERR;
	}
	
	if (this->_renderGeometryNeedsFinish)
	{
		// Allow for the geometry rendering to finish.
		this->_renderGeometryNeedsFinish = false;
		for (size_t i = 0; i < this->_threadCount; i++)
		{
			this->_task[i].finish();
		}
		
		// Now that geometry rendering is finished on all threads, check the texture cache.
		texCache.Evict();
		
		// Do multithreaded post-processing.
		if (this->_enableEdgeMark || this->_enableFog)
		{
			for (size_t i = 0; i < this->_threadCount; i++)
			{
				this->_threadPostprocessParam[i].enableEdgeMarking = this->_enableEdgeMark;
				this->_threadPostprocessParam[i].enableFog = this->_enableFog;
				this->_threadPostprocessParam[i].fogColor = this->currentRenderState->fogColor;
				this->_threadPostprocessParam[i].fogAlphaOnly = this->currentRenderState->enableFogAlphaOnly;
				
				this->_task[i].execute(&SoftRasterizer_RunRenderEdgeMarkAndFog, &this->_threadPostprocessParam[i]);
			}
			
			// Allow for post-processing to finish.
			for (size_t i = 0; i < this->_threadCount; i++)
			{
				this->_task[i].finish();
			}
		}
	}
	
	this->_renderNeedsFlushMain = true;
	this->_renderNeedsFlush16 = true;
	
	return RENDER3DERROR_NOERR;
}

Render3DError SoftRasterizerRenderer::RenderFlush(bool willFlushBuffer32, bool willFlushBuffer16)
{
	if (!this->_isPoweredOn)
	{
		return RENDER3DERROR_NOERR;
	}
	
	FragmentColor *framebufferMain = (willFlushBuffer32 && (this->_outputFormat == NDSColorFormat_BGR888_Rev)) ? GPU->GetEngineMain()->Get3DFramebufferMain() : NULL;
	u16 *framebuffer16 = (willFlushBuffer16) ? GPU->GetEngineMain()->Get3DFramebuffer16() : NULL;
	this->FlushFramebuffer(this->_framebufferColor, framebufferMain, framebuffer16);
	
	return RENDER3DERROR_NOERR;
}

Render3DError SoftRasterizerRenderer::SetFramebufferSize(size_t w, size_t h)
{
	Render3DError error = Render3D::SetFramebufferSize(w, h);
	if (error != RENDER3DERROR_NOERR)
	{
		return RENDER3DERROR_NOERR;
	}
	
	delete this->_framebufferAttributes;
	this->_framebufferAttributes = new FragmentAttributesBuffer(w * h);
	
	const size_t pixCount = (this->_framebufferSIMDPixCount > 0) ? this->_framebufferSIMDPixCount : this->_framebufferPixCount;
	
	if (this->_threadCount == 0)
	{
		this->_customLinesPerThread = h;
		this->_customPixelsPerThread = pixCount;
		
		this->_threadPostprocessParam[0].startLine = 0;
		this->_threadPostprocessParam[0].endLine = h;
		
		this->_threadClearParam[0].startPixel = 0;
		this->_threadClearParam[0].endPixel = pixCount;
		
		this->_rasterizerUnit[0].SetSLI(this->_threadPostprocessParam[0].startLine, this->_threadPostprocessParam[0].endLine, false);
	}
	else
	{
		this->_customLinesPerThread = h / this->_threadCount;
		this->_customPixelsPerThread = pixCount / this->_threadCount;
		
		for (size_t i = 0; i < this->_threadCount; i++)
		{
			this->_threadPostprocessParam[i].startLine = i * this->_customLinesPerThread;
			this->_threadPostprocessParam[i].endLine = (i < this->_threadCount - 1) ? (i + 1) * this->_customLinesPerThread : h;
			
			this->_threadClearParam[i].startPixel = i * this->_customPixelsPerThread;
			this->_threadClearParam[i].endPixel = (i < this->_threadCount - 1) ? (i + 1) * this->_customPixelsPerThread : pixCount;
			
			this->_rasterizerUnit[i].SetSLI(this->_threadPostprocessParam[i].startLine, this->_threadPostprocessParam[i].endLine, false);
		}
	}
	
	return RENDER3DERROR_NOERR;
}

#if defined(ENABLE_AVX2) || defined(ENABLE_SSE2) || defined(ENABLE_ALTIVEC)

template <size_t SIMDBYTES>
SoftRasterizer_SIMD<SIMDBYTES>::SoftRasterizer_SIMD()
{
	if (_threadCount == 0)
	{
		_threadClearParam[0].renderer = this;
		_threadClearParam[0].startPixel = 0;
		_threadClearParam[0].endPixel = _framebufferSIMDPixCount;
	}
	else
	{
		const size_t pixelsPerThread = ((_framebufferSIMDPixCount / SIMDBYTES) / _threadCount) * SIMDBYTES;
		
		for (size_t i = 0; i < _threadCount; i++)
		{
			_threadClearParam[i].renderer = this;
			_threadClearParam[i].startPixel = i * pixelsPerThread;
			_threadClearParam[i].endPixel = (i < _threadCount - 1) ? (i + 1) * pixelsPerThread : _framebufferSIMDPixCount;
		}
	}
}

template <size_t SIMDBYTES>
Render3DError SoftRasterizer_SIMD<SIMDBYTES>::ClearUsingValues(const FragmentColor &clearColor6665, const FragmentAttributes &clearAttributes)
{
	this->LoadClearValues(clearColor6665, clearAttributes);
	
	const bool doMultithreadedClear = (this->_threadCount > 0);
	
	if (doMultithreadedClear)
	{
		for (size_t threadIndex = 0; threadIndex < this->_threadCount; threadIndex++)
		{
			this->_task[threadIndex].execute(&SoftRasterizer_RunClearUsingValues, &this->_threadClearParam[threadIndex]);
		}
	}
	else
	{
		this->ClearUsingValues_Execute(0, this->_framebufferSIMDPixCount);
	}
	
#pragma LOOPVECTORIZE_DISABLE
	for (size_t i = this->_framebufferSIMDPixCount; i < this->_framebufferPixCount; i++)
	{
		this->_framebufferColor[i] = clearColor6665;
		this->_framebufferAttributes->SetAtIndex(i, clearAttributes);
	}
	
	if (doMultithreadedClear)
	{
		for (size_t threadIndex = 0; threadIndex < this->_threadCount; threadIndex++)
		{
			this->_task[threadIndex].finish();
		}
	}
	
	return RENDER3DERROR_NOERR;
}

template <size_t SIMDBYTES>
Render3DError SoftRasterizer_SIMD<SIMDBYTES>::SetFramebufferSize(size_t w, size_t h)
{
	Render3DError error = Render3D_SIMD<SIMDBYTES>::SetFramebufferSize(w, h);
	if (error != RENDER3DERROR_NOERR)
	{
		return RENDER3DERROR_NOERR;
	}
	
	delete this->_framebufferAttributes;
	this->_framebufferAttributes = new FragmentAttributesBuffer(w * h);
	
	const size_t pixCount = (this->_framebufferSIMDPixCount > 0) ? this->_framebufferSIMDPixCount : this->_framebufferPixCount;
	
	if (this->_threadCount == 0)
	{
		this->_customLinesPerThread = h;
		this->_customPixelsPerThread = pixCount;
		
		this->_threadPostprocessParam[0].startLine = 0;
		this->_threadPostprocessParam[0].endLine = h;
		
		this->_threadClearParam[0].startPixel = 0;
		this->_threadClearParam[0].endPixel = pixCount;
		
		this->_rasterizerUnit[0].SetSLI(this->_threadPostprocessParam[0].startLine, this->_threadPostprocessParam[0].endLine, false);
	}
	else
	{
		const size_t pixelsPerThread = ((pixCount / SIMDBYTES) / this->_threadCount) * SIMDBYTES;
		
		this->_customLinesPerThread = h / this->_threadCount;
		this->_customPixelsPerThread = pixelsPerThread / this->_threadCount;
		
		for (size_t i = 0; i < this->_threadCount; i++)
		{
			this->_threadPostprocessParam[i].startLine = i * this->_customLinesPerThread;
			this->_threadPostprocessParam[i].endLine = (i < this->_threadCount - 1) ? (i + 1) * this->_customLinesPerThread : h;
			
			this->_threadClearParam[i].startPixel = i * pixelsPerThread;
			this->_threadClearParam[i].endPixel = (i < this->_threadCount - 1) ? (i + 1) * pixelsPerThread : pixCount;
			
			this->_rasterizerUnit[i].SetSLI(this->_threadPostprocessParam[i].startLine, this->_threadPostprocessParam[i].endLine, false);
		}
	}
	
	return RENDER3DERROR_NOERR;
}

#endif

#if defined(ENABLE_AVX2)

void SoftRasterizerRenderer_AVX2::LoadClearValues(const FragmentColor &clearColor6665, const FragmentAttributes &clearAttributes)
{
	this->_clearColor_v256u32					= _mm256_set1_epi32(clearColor6665.color);
	this->_clearDepth_v256u32					= _mm256_set1_epi32(clearAttributes.depth);
	this->_clearAttrOpaquePolyID_v256u8			= _mm256_set1_epi8(clearAttributes.opaquePolyID);
	this->_clearAttrTranslucentPolyID_v256u8	= _mm256_set1_epi8(clearAttributes.translucentPolyID);
	this->_clearAttrStencil_v256u8				= _mm256_set1_epi8(clearAttributes.stencil);
	this->_clearAttrIsFogged_v256u8				= _mm256_set1_epi8(clearAttributes.isFogged);
	this->_clearAttrIsTranslucentPoly_v256u8	= _mm256_set1_epi8(clearAttributes.isTranslucentPoly);
	this->_clearAttrPolyFacing_v256u8			= _mm256_set1_epi8(clearAttributes.polyFacing);
}

void SoftRasterizerRenderer_AVX2::ClearUsingValues_Execute(const size_t startPixel, const size_t endPixel)
{
	for (size_t i = startPixel; i < endPixel; i+=32)
	{
		_mm256_stream_si256((v256u32 *)(this->_framebufferColor + i +  0), this->_clearColor_v256u32);
		_mm256_stream_si256((v256u32 *)(this->_framebufferColor + i +  8), this->_clearColor_v256u32);
		_mm256_stream_si256((v256u32 *)(this->_framebufferColor + i + 16), this->_clearColor_v256u32);
		_mm256_stream_si256((v256u32 *)(this->_framebufferColor + i + 24), this->_clearColor_v256u32);
		
		_mm256_stream_si256((v256u32 *)(this->_framebufferAttributes->depth + i +  0), this->_clearDepth_v256u32);
		_mm256_stream_si256((v256u32 *)(this->_framebufferAttributes->depth + i +  8), this->_clearDepth_v256u32);
		_mm256_stream_si256((v256u32 *)(this->_framebufferAttributes->depth + i + 16), this->_clearDepth_v256u32);
		_mm256_stream_si256((v256u32 *)(this->_framebufferAttributes->depth + i + 24), this->_clearDepth_v256u32);
		
		_mm256_stream_si256((v256u8 *)(this->_framebufferAttributes->opaquePolyID + i), this->_clearAttrOpaquePolyID_v256u8);
		_mm256_stream_si256((v256u8 *)(this->_framebufferAttributes->translucentPolyID + i), this->_clearAttrTranslucentPolyID_v256u8);
		_mm256_stream_si256((v256u8 *)(this->_framebufferAttributes->stencil + i), this->_clearAttrStencil_v256u8);
		_mm256_stream_si256((v256u8 *)(this->_framebufferAttributes->isFogged + i), this->_clearAttrIsFogged_v256u8);
		_mm256_stream_si256((v256u8 *)(this->_framebufferAttributes->isTranslucentPoly + i), this->_clearAttrIsTranslucentPoly_v256u8);
		_mm256_stream_si256((v256u8 *)(this->_framebufferAttributes->polyFacing + i), this->_clearAttrPolyFacing_v256u8);
	}
}

#elif defined(ENABLE_SSE2)

void SoftRasterizerRenderer_SSE2::LoadClearValues(const FragmentColor &clearColor6665, const FragmentAttributes &clearAttributes)
{
	this->_clearColor_v128u32					= _mm_set1_epi32(clearColor6665.color);
	this->_clearDepth_v128u32					= _mm_set1_epi32(clearAttributes.depth);
	this->_clearAttrOpaquePolyID_v128u8			= _mm_set1_epi8(clearAttributes.opaquePolyID);
	this->_clearAttrTranslucentPolyID_v128u8	= _mm_set1_epi8(clearAttributes.translucentPolyID);
	this->_clearAttrStencil_v128u8				= _mm_set1_epi8(clearAttributes.stencil);
	this->_clearAttrIsFogged_v128u8				= _mm_set1_epi8(clearAttributes.isFogged);
	this->_clearAttrIsTranslucentPoly_v128u8	= _mm_set1_epi8(clearAttributes.isTranslucentPoly);
	this->_clearAttrPolyFacing_v128u8			= _mm_set1_epi8(clearAttributes.polyFacing);
}

void SoftRasterizerRenderer_SSE2::ClearUsingValues_Execute(const size_t startPixel, const size_t endPixel)
{
	for (size_t i = startPixel; i < endPixel; i+=16)
	{
		_mm_stream_si128((v128u32 *)(this->_framebufferColor + i +  0), this->_clearColor_v128u32);
		_mm_stream_si128((v128u32 *)(this->_framebufferColor + i +  4), this->_clearColor_v128u32);
		_mm_stream_si128((v128u32 *)(this->_framebufferColor + i +  8), this->_clearColor_v128u32);
		_mm_stream_si128((v128u32 *)(this->_framebufferColor + i + 12), this->_clearColor_v128u32);
		
		_mm_stream_si128((v128u32 *)(this->_framebufferAttributes->depth + i +  0), this->_clearDepth_v128u32);
		_mm_stream_si128((v128u32 *)(this->_framebufferAttributes->depth + i +  4), this->_clearDepth_v128u32);
		_mm_stream_si128((v128u32 *)(this->_framebufferAttributes->depth + i +  8), this->_clearDepth_v128u32);
		_mm_stream_si128((v128u32 *)(this->_framebufferAttributes->depth + i + 12), this->_clearDepth_v128u32);
		
		_mm_stream_si128((v128u8 *)(this->_framebufferAttributes->opaquePolyID + i), this->_clearAttrOpaquePolyID_v128u8);
		_mm_stream_si128((v128u8 *)(this->_framebufferAttributes->translucentPolyID + i), this->_clearAttrTranslucentPolyID_v128u8);
		_mm_stream_si128((v128u8 *)(this->_framebufferAttributes->stencil + i), this->_clearAttrStencil_v128u8);
		_mm_stream_si128((v128u8 *)(this->_framebufferAttributes->isFogged + i), this->_clearAttrIsFogged_v128u8);
		_mm_stream_si128((v128u8 *)(this->_framebufferAttributes->isTranslucentPoly + i), this->_clearAttrIsTranslucentPoly_v128u8);
		_mm_stream_si128((v128u8 *)(this->_framebufferAttributes->polyFacing + i), this->_clearAttrPolyFacing_v128u8);
	}
}

#elif defined(ENABLE_ALTIVEC)

void SoftRasterizerRenderer_Altivec::LoadClearValues(const FragmentColor &clearColor6665, const FragmentAttributes &clearAttributes)
{
	this->_clearColor_v128u32					= (v128u32){clearColor6665.color,clearColor6665.color,clearColor6665.color,clearColor6665.color};
	this->_clearDepth_v128u32					= (v128u32){clearAttributes.depth,clearAttributes.depth,clearAttributes.depth,clearAttributes.depth};
	
	this->_clearAttrOpaquePolyID_v128u8			= (v128u8){clearAttributes.opaquePolyID,clearAttributes.opaquePolyID,clearAttributes.opaquePolyID,clearAttributes.opaquePolyID,
												           clearAttributes.opaquePolyID,clearAttributes.opaquePolyID,clearAttributes.opaquePolyID,clearAttributes.opaquePolyID,
												           clearAttributes.opaquePolyID,clearAttributes.opaquePolyID,clearAttributes.opaquePolyID,clearAttributes.opaquePolyID,
												           clearAttributes.opaquePolyID,clearAttributes.opaquePolyID,clearAttributes.opaquePolyID,clearAttributes.opaquePolyID};
	
	this->_clearAttrTranslucentPolyID_v128u8	= (v128u8){clearAttributes.translucentPolyID,clearAttributes.translucentPolyID,clearAttributes.translucentPolyID,clearAttributes.translucentPolyID,
												           clearAttributes.translucentPolyID,clearAttributes.translucentPolyID,clearAttributes.translucentPolyID,clearAttributes.translucentPolyID,
												           clearAttributes.translucentPolyID,clearAttributes.translucentPolyID,clearAttributes.translucentPolyID,clearAttributes.translucentPolyID,
												           clearAttributes.translucentPolyID,clearAttributes.translucentPolyID,clearAttributes.translucentPolyID,clearAttributes.translucentPolyID};
	
	this->_clearAttrStencil_v128u8	= (v128u8){clearAttributes.stencil,clearAttributes.stencil,clearAttributes.stencil,clearAttributes.stencil,
									           clearAttributes.stencil,clearAttributes.stencil,clearAttributes.stencil,clearAttributes.stencil,
									           clearAttributes.stencil,clearAttributes.stencil,clearAttributes.stencil,clearAttributes.stencil,
									           clearAttributes.stencil,clearAttributes.stencil,clearAttributes.stencil,clearAttributes.stencil};
	
	this->_clearAttrIsFogged_v128u8	= (v128u8){clearAttributes.isFogged,clearAttributes.isFogged,clearAttributes.isFogged,clearAttributes.isFogged,
									           clearAttributes.isFogged,clearAttributes.isFogged,clearAttributes.isFogged,clearAttributes.isFogged,
									           clearAttributes.isFogged,clearAttributes.isFogged,clearAttributes.isFogged,clearAttributes.isFogged,
									           clearAttributes.isFogged,clearAttributes.isFogged,clearAttributes.isFogged,clearAttributes.isFogged};
	
	this->_clearAttrIsTranslucentPoly_v128u8	= (v128u8){clearAttributes.isTranslucentPoly,clearAttributes.isTranslucentPoly,clearAttributes.isTranslucentPoly,clearAttributes.isTranslucentPoly,
												           clearAttributes.isTranslucentPoly,clearAttributes.isTranslucentPoly,clearAttributes.isTranslucentPoly,clearAttributes.isTranslucentPoly,
												           clearAttributes.isTranslucentPoly,clearAttributes.isTranslucentPoly,clearAttributes.isTranslucentPoly,clearAttributes.isTranslucentPoly,
												           clearAttributes.isTranslucentPoly,clearAttributes.isTranslucentPoly,clearAttributes.isTranslucentPoly,clearAttributes.isTranslucentPoly};
	
	this->_clearAttrPolyFacing_v128u8	= (v128u8){clearAttributes.polyFacing,clearAttributes.polyFacing,clearAttributes.polyFacing,clearAttributes.polyFacing,
										           clearAttributes.polyFacing,clearAttributes.polyFacing,clearAttributes.polyFacing,clearAttributes.polyFacing,
										           clearAttributes.polyFacing,clearAttributes.polyFacing,clearAttributes.polyFacing,clearAttributes.polyFacing,
										           clearAttributes.polyFacing,clearAttributes.polyFacing,clearAttributes.polyFacing,clearAttributes.polyFacing};
}

void SoftRasterizerRenderer_Altivec::ClearUsingValues_Execute(const size_t startPixel, const size_t endPixel)
{
	for (size_t i = startPixel; i < endPixel; i+=16)
	{
		vec_st(this->_clearColor_v128u32, (i * 4) +  0, this->_framebufferColor);
		vec_st(this->_clearColor_v128u32, (i * 4) + 16, this->_framebufferColor);
		vec_st(this->_clearColor_v128u32, (i * 4) + 32, this->_framebufferColor);
		vec_st(this->_clearColor_v128u32, (i * 4) + 48, this->_framebufferColor);
		
		vec_st(this->_clearDepth_v128u32, (i * 4) +  0, this->_framebufferAttributes->depth);
		vec_st(this->_clearDepth_v128u32, (i * 4) + 16, this->_framebufferAttributes->depth);
		vec_st(this->_clearDepth_v128u32, (i * 4) + 32, this->_framebufferAttributes->depth);
		vec_st(this->_clearDepth_v128u32, (i * 4) + 48, this->_framebufferAttributes->depth);
		
		vec_st(this->_clearAttrOpaquePolyID_v128u8, i, this->_framebufferAttributes->opaquePolyID);
		vec_st(this->_clearAttrTranslucentPolyID_v128u8, i, this->_framebufferAttributes->translucentPolyID);
		vec_st(this->_clearAttrStencil_v128u8, i, this->_framebufferAttributes->stencil);
		vec_st(this->_clearAttrIsFogged_v128u8, i, this->_framebufferAttributes->isFogged);
		vec_st(this->_clearAttrIsTranslucentPoly_v128u8, i, this->_framebufferAttributes->isTranslucentPoly);
		vec_st(this->_clearAttrPolyFacing_v128u8, i, this->_framebufferAttributes->polyFacing);
	}
}

#endif
