/*
	Copyright (C) 2009-2023 DeSmuME team

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
#include "./utils/colorspacehandler/colorspacehandler.h"
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



static FORCEINLINE void alphaBlend(const bool isAlphaBlendingEnabled, const Color4u8 inSrc, Color4u8 &outDst)
{
	if (inSrc.a == 0)
	{
		return;
	}
	
	if (inSrc.a == 31 || outDst.a == 0 || !isAlphaBlendingEnabled)
	{
		outDst = inSrc;
	}
	else
	{
		const u8 alpha = inSrc.a + 1;
		const u8 invAlpha = 32 - alpha;
		outDst.r = (alpha*inSrc.r + invAlpha*outDst.r) >> 5;
		outDst.g = (alpha*inSrc.g + invAlpha*outDst.g) >> 5;
		outDst.b = (alpha*inSrc.b + invAlpha*outDst.b) >> 5;
		outDst.a = max(inSrc.a, outDst.a);
	}
}

static FORCEINLINE void EdgeBlend(Color4u8 &dst, const Color4u8 src)
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
FORCEINLINE Color4u8 RasterizerUnit<RENDERER>::_sample(const Vector2f32 &texCoord)
{
	//finally, we can use floor here. but, it is slower than we want.
	//the best solution is probably to wait until the pipeline is full of fixed point
	const float texScalingFactor = (float)this->_currentTexture->GetScalingFactor();
	const Vector2f32 texCoordScaled = {
		texCoord.u * texScalingFactor,
		texCoord.v * texScalingFactor
	};
	
	Vector2s32 sampleCoord = { 0, 0 };
	
	if (!this->_softRender->_enableFragmentSamplingHack)
	{
		sampleCoord.u = s32floor(texCoordScaled.u);
		sampleCoord.v = s32floor(texCoordScaled.v);
	}
	else
	{
		sampleCoord.u = this->_round_s(texCoordScaled.u);
		sampleCoord.v = this->_round_s(texCoordScaled.v);
	}
	
	const u32 *textureData = this->_currentTexture->GetRenderData();
	this->_currentTexture->GetRenderSamplerCoordinates(this->_textureWrapMode, sampleCoord);
	
	Color4u8 color;
	color.value = textureData[( sampleCoord.v << this->_currentTexture->GetRenderWidthShift() ) + sampleCoord.u];
	
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
FORCEINLINE void RasterizerUnit<RENDERER>::_shade(const PolygonMode polygonMode, const Color4u8 vtxColor, const Vector2f32 &texCoord, Color4u8 &outColor)
{
	if (ISSHADOWPOLYGON)
	{
		outColor = vtxColor;
		return;
	}
	
	const GFX3D_State &renderState = *this->_softRender->currentRenderState;
	
	static const Color4u8 colorWhite = { 0x3F, 0x3F, 0x3F, 0x1F };
	const Color4u8 mainTexColor = (this->_currentTexture->IsSamplingEnabled()) ? this->_sample(texCoord) : colorWhite;
	
	switch (polygonMode)
	{
		case POLYGON_MODE_MODULATE:
		{
			outColor.r = modulate_table[mainTexColor.r][vtxColor.r];
			outColor.g = modulate_table[mainTexColor.g][vtxColor.g];
			outColor.b = modulate_table[mainTexColor.b][vtxColor.b];
			outColor.a = modulate_table[GFX3D_5TO6_LOOKUP(mainTexColor.a)][GFX3D_5TO6_LOOKUP(vtxColor.a)] >> 1;
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
		}
			
		case POLYGON_MODE_DECAL:
		{
			if (this->_currentTexture->IsSamplingEnabled())
			{
				outColor.r = decal_table[mainTexColor.a][mainTexColor.r][vtxColor.r];
				outColor.g = decal_table[mainTexColor.a][mainTexColor.g][vtxColor.g];
				outColor.b = decal_table[mainTexColor.a][mainTexColor.b][vtxColor.b];
				outColor.a = vtxColor.a;
			}
			else
			{
				outColor = vtxColor;
			}
			break;
		}
			
		case POLYGON_MODE_TOONHIGHLIGHT:
		{
			const Color4u8 toonColor = this->_softRender->toonColor32LUT[vtxColor.r >> 1];
			
			if (renderState.DISP3DCNT.PolygonShading == PolygonShadingMode_Highlight)
			{
				// Tested in the "Shadows of Almia" logo in the Pokemon Ranger: Shadows of Almia title screen.
				// Also tested in Advance Wars: Dual Strike and Advance Wars: Days of Ruin when tiles highlight
				// during unit selection.
				outColor.r = modulate_table[mainTexColor.r][vtxColor.r];
				outColor.g = modulate_table[mainTexColor.g][vtxColor.r];
				outColor.b = modulate_table[mainTexColor.b][vtxColor.r];
				outColor.a = modulate_table[GFX3D_5TO6_LOOKUP(mainTexColor.a)][GFX3D_5TO6_LOOKUP(vtxColor.a)] >> 1;
				
				outColor.r = min<u8>(0x3F, (outColor.r + toonColor.r));
				outColor.g = min<u8>(0x3F, (outColor.g + toonColor.g));
				outColor.b = min<u8>(0x3F, (outColor.b + toonColor.b));
			}
			else
			{
				outColor.r = modulate_table[mainTexColor.r][toonColor.r];
				outColor.g = modulate_table[mainTexColor.g][toonColor.g];
				outColor.b = modulate_table[mainTexColor.b][toonColor.b];
				outColor.a = modulate_table[GFX3D_5TO6_LOOKUP(mainTexColor.a)][GFX3D_5TO6_LOOKUP(vtxColor.a)] >> 1;
			}
			break;
		}
			
		case POLYGON_MODE_SHADOW:
			//is this right? only with the material color?
			outColor = vtxColor;
			break;
	}
}

template<bool RENDERER> template<bool ISFRONTFACING, bool ISSHADOWPOLYGON>
FORCEINLINE void RasterizerUnit<RENDERER>::_pixel(const POLYGON_ATTR polyAttr, const bool isTranslucent, const size_t fragmentIndex, Color4u8 &dstColor, const Color4f32 &vtxColorFloat, float invu, float invv, float z, float w)
{
	const GFX3D_State &renderState = *this->_softRender->currentRenderState;
	
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
	const u32 newDepth = (renderState.SWAP_BUFFERS.DepthMode) ? u32floor(w * 4096.0f) : u32floor(z * 4194303.0f) << 2;
	
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
	else if ( (ISFRONTFACING && (dstAttributePolyFacing == PolyFacing_Back)) && (dstColor.a == 0x1F) )
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
	
	//this is a HACK:
	//we are being very sloppy with our interpolation precision right now
	//and rather than fix it, i just want to clamp it
	const Color4u8 vtxColor = {
		max<u8>( 0x00, min<u32>(0x3F, u32floor( (vtxColorFloat.r * w) + 0.5f )) ),
		max<u8>( 0x00, min<u32>(0x3F, u32floor( (vtxColorFloat.g * w) + 0.5f )) ),
		max<u8>( 0x00, min<u32>(0x3F, u32floor( (vtxColorFloat.b * w) + 0.5f )) ),
		polyAttr.Alpha
	};
	
	//pixel shader
	const Vector2f32 texCoordFloat = { invu * w, invv * w };
	Color4u8 shaderOutput;
	this->_shade<ISSHADOWPOLYGON>((PolygonMode)polyAttr.Mode, vtxColor, texCoordFloat, shaderOutput);
	
	// handle alpha test
	if ( shaderOutput.a == 0 ||
	    (renderState.DISP3DCNT.EnableAlphaTest && (shaderOutput.a < renderState.alphaTestRef)) )
	{
		return;
	}
	
	// write pixel values to the framebuffer
	const bool isOpaquePixel = (shaderOutput.a == 0x1F);
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
		alphaBlend((renderState.DISP3DCNT.EnableAlphaBlending != 0), shaderOutput, dstColor);
		
		dstAttributeIsFogged = (dstAttributeIsFogged && polyAttr.Fog_Enable);
	}
	
	dstAttributePolyFacing = (ISFRONTFACING) ? PolyFacing_Front : PolyFacing_Back;
	
	//depth writing
	if (isOpaquePixel || polyAttr.TranslucentDepthWrite_Enable)
		dstAttributeDepth = newDepth;
}

//draws a single scanline
template<bool RENDERER> template<bool ISFRONTFACING, bool ISSHADOWPOLYGON, bool USELINEHACK>
FORCEINLINE void RasterizerUnit<RENDERER>::_drawscanline(const POLYGON_ATTR polyAttr, const bool isTranslucent, Color4u8 *dstColor, const size_t framebufferWidth, const size_t framebufferHeight, edge_fx_fl *pLeft, edge_fx_fl *pRight)
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
	
	CACHE_ALIGN Color4f32 vtxColorFloat = {
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
	
	const CACHE_ALIGN Color4f32 vtxColorFloat_dx = {
		(pRight->color[0].curr - vtxColorFloat.r) * invWidth,
		(pRight->color[1].curr - vtxColorFloat.g) * invWidth,
		(pRight->color[2].curr - vtxColorFloat.b) * invWidth,
		0.0f * invWidth
	};

	size_t adr = (pLeft->Y*framebufferWidth)+XStart;

	//CONSIDER: in case some other math is wrong (shouldve been clipped OK), we might go out of bounds here.
	//better check the Y value.
	if ( (pLeft->Y < 0) || (pLeft->Y >= framebufferHeight) )
	{
		const float gpuScalingFactorHeight = (float)framebufferHeight / (float)GPU_FRAMEBUFFER_NATIVE_HEIGHT;
		printf("rasterizer rendering at y=%d! oops! (x%.1f)\n", pLeft->Y, gpuScalingFactorHeight);
		return;
	}

	s32 x = XStart;

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
		
		vtxColorFloat.r += vtxColorFloat_dx.r * negativeX;
		vtxColorFloat.g += vtxColorFloat_dx.g * negativeX;
		vtxColorFloat.b += vtxColorFloat_dx.b * negativeX;
		vtxColorFloat.a += vtxColorFloat_dx.a * negativeX;
		
		adr += -x;
		width -= -x;
		x = 0;
	}
	
	// Normally, if an out-of-bounds write were to occur, this would cause an error to get logged.
	// However, due to rounding errors associated with floating-point conversions, it is common that
	// rendering at custom resolutions may cause the rendering width to be off by 1. Therefore, we
	// will treat this case as an exception and suppress the error through the use of this flag.
	bool customResolutionOutOfBoundsSuppress = false;
	
	if (x+width > framebufferWidth)
	{
		if (RENDERER && !USELINEHACK)
		{
			if ( (framebufferWidth != GPU_FRAMEBUFFER_NATIVE_WIDTH) && (x+width == framebufferWidth+1) )
			{
				customResolutionOutOfBoundsSuppress = true;
			}
			else
			{
				const float gpuScalingFactorWidth = (float)framebufferWidth / (float)GPU_FRAMEBUFFER_NATIVE_WIDTH;
				printf("rasterizer rendering at x=%d! oops! (x%.2f)\n", x+width-1, gpuScalingFactorWidth);
				return;
			}
		}
		
		if (customResolutionOutOfBoundsSuppress)
		{
			width -= 1;
		}
		else
		{
			width = (s32)framebufferWidth - x;
		}
	}
	
	while (width-- > 0)
	{
		this->_pixel<ISFRONTFACING, ISSHADOWPOLYGON>(polyAttr, isTranslucent, adr, dstColor[adr], vtxColorFloat, coord[0], coord[1], coord[2], 1.0f/coord[3]);
		adr++;
		x++;

		coord[0] += coord_dx[0];
		coord[1] += coord_dx[1];
		coord[2] += coord_dx[2];
		coord[3] += coord_dx[3];
		
		vtxColorFloat.r += vtxColorFloat_dx.r;
		vtxColorFloat.g += vtxColorFloat_dx.g;
		vtxColorFloat.b += vtxColorFloat_dx.b;
		vtxColorFloat.a += vtxColorFloat_dx.a;
	}
}

#ifdef ENABLE_SSE2

template<bool RENDERER> template<bool ISFRONTFACING, bool ISSHADOWPOLYGON>
FORCEINLINE void RasterizerUnit<RENDERER>::_pixel_SSE2(const POLYGON_ATTR polyAttr, const bool isTranslucent, const size_t fragmentIndex, Color4u8 &dstColor, const __m128 &vtxColorFloat, float invu, float invv, float z, float w)
{
	const GFX3D_State &renderState = *this->_softRender->currentRenderState;
	
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
	const u32 newDepth = (renderState.SWAP_BUFFERS.DepthMode) ? u32floor(w * 4096.0f) : u32floor(z * 4194303.0f) << 2;
	
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
	__m128 newVtxColorf = _mm_add_ps( _mm_mul_ps(vtxColorFloat, perspective), _mm_set1_ps(0.5f) );
	newVtxColorf = _mm_max_ps(newVtxColorf, _mm_setzero_ps());
	
	__m128i newVtxColori = _mm_cvttps_epi32(newVtxColorf);
	newVtxColori = _mm_min_epu8(newVtxColori, _mm_set_epi32(0x1F, 0x3F, 0x3F, 0x3F));
	newVtxColori = _mm_packus_epi16(newVtxColori, _mm_setzero_si128());
	newVtxColori = _mm_packus_epi16(newVtxColori, _mm_setzero_si128());
	
	Color4u8 vtxColor;
	vtxColor.value = _mm_cvtsi128_si32(newVtxColori);
	
	//pixel shader
	const Vector2f32 texCoordFloat = { invu * w, invv * w };
	Color4u8 shaderOutput;
	this->_shade<ISSHADOWPOLYGON>((PolygonMode)polyAttr.Mode, vtxColor, texCoordFloat, shaderOutput);
	
	// handle alpha test
	if ( shaderOutput.a == 0 ||
	    (renderState.DISP3DCNT.EnableAlphaTest && (shaderOutput.a < renderState.alphaTestRef)) )
	{
		return;
	}
	
	// write pixel values to the framebuffer
	const bool isOpaquePixel = (shaderOutput.a == 0x1F);
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
		alphaBlend((renderState.DISP3DCNT.EnableAlphaBlending != 0), shaderOutput, dstColor);
		
		dstAttributeIsFogged = (dstAttributeIsFogged && polyAttr.Fog_Enable);
	}
	
	dstAttributePolyFacing = (ISFRONTFACING) ? PolyFacing_Front : PolyFacing_Back;
	
	//depth writing
	if (isOpaquePixel || polyAttr.TranslucentDepthWrite_Enable)
		dstAttributeDepth = newDepth;
}

//draws a single scanline
template<bool RENDERER> template<bool ISFRONTFACING, bool ISSHADOWPOLYGON, bool USELINEHACK>
FORCEINLINE void RasterizerUnit<RENDERER>::_drawscanline_SSE2(const POLYGON_ATTR polyAttr, const bool isTranslucent, Color4u8 *dstColor, const size_t framebufferWidth, const size_t framebufferHeight, edge_fx_fl *pLeft, edge_fx_fl *pRight)
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
	
	__m128 vtxColorFloat = _mm_setr_ps(pLeft->color[0].curr,
	                                   pLeft->color[1].curr,
	                                   pLeft->color[2].curr,
	                                   (float)polyAttr.Alpha / 31.0f);
	
	//our dx values are taken from the steps up until the right edge
	const __m128 invWidth = _mm_set1_ps(1.0f / (float)width);
	const __m128 coord_dx = _mm_mul_ps(_mm_setr_ps(pRight->u.curr - pLeft->u.curr, pRight->v.curr - pLeft->v.curr, pRight->z.curr - pLeft->z.curr, pRight->invw.curr - pLeft->invw.curr), invWidth);
	const __m128 vtxColorFloat_dx = _mm_mul_ps(_mm_setr_ps(pRight->color[0].curr - pLeft->color[0].curr, pRight->color[1].curr - pLeft->color[1].curr, pRight->color[2].curr - pLeft->color[2].curr, 0.0f), invWidth);
	
	size_t adr = (pLeft->Y*framebufferWidth)+XStart;
	
	//CONSIDER: in case some other math is wrong (shouldve been clipped OK), we might go out of bounds here.
	//better check the Y value.
	if ( (pLeft->Y < 0) || (pLeft->Y >= framebufferHeight) )
	{
		const float gpuScalingFactorHeight = (float)framebufferHeight / (float)GPU_FRAMEBUFFER_NATIVE_HEIGHT;
		printf("rasterizer rendering at y=%d! oops! (x%.1f)\n", pLeft->Y, gpuScalingFactorHeight);
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
		vtxColorFloat = _mm_add_ps(vtxColorFloat, _mm_mul_ps(vtxColorFloat_dx, negativeX));
		
		adr += -x;
		width -= -x;
		x = 0;
	}
	
	// Normally, if an out-of-bounds write were to occur, this would cause an error to get logged.
	// However, due to rounding errors associated with floating-point conversions, it is common that
	// rendering at custom resolutions may cause the rendering width to be off by 1. Therefore, we
	// will treat this case as an exception and suppress the error through the use of this flag.
	bool customResolutionOutOfBoundsSuppress = false;
	
	if (x+width > framebufferWidth)
	{
		if (RENDERER && !USELINEHACK)
		{
			if ( (framebufferWidth != GPU_FRAMEBUFFER_NATIVE_WIDTH) && (x+width == framebufferWidth+1) )
			{
				customResolutionOutOfBoundsSuppress = true;
			}
			else
			{
				const float gpuScalingFactorWidth = (float)framebufferWidth / (float)GPU_FRAMEBUFFER_NATIVE_WIDTH;
				printf("rasterizer rendering at x=%d! oops! (x%.2f)\n", x+width-1, gpuScalingFactorWidth);
				return;
			}
		}
		
		if (customResolutionOutOfBoundsSuppress)
		{
			width -= 1;
		}
		else
		{
			width = (s32)framebufferWidth - x;
		}
	}
	
	CACHE_ALIGN float coord_s[4];
	
	while (width-- > 0)
	{
		_mm_store_ps(coord_s, coord);
		
		this->_pixel_SSE2<ISFRONTFACING, ISSHADOWPOLYGON>(polyAttr, isTranslucent, adr, dstColor[adr], vtxColorFloat, coord_s[0], coord_s[1], coord_s[2], 1.0f/coord_s[3]);
		adr++;
		x++;
		
		coord = _mm_add_ps(coord, coord_dx);
		vtxColorFloat = _mm_add_ps(vtxColorFloat, vtxColorFloat_dx);
	}
}

#endif // ENABLE_SSE2

//runs several scanlines, until an edge is finished
template<bool RENDERER> template<bool SLI, bool ISFRONTFACING, bool ISSHADOWPOLYGON, bool USELINEHACK>
void RasterizerUnit<RENDERER>::_runscanlines(const POLYGON_ATTR polyAttr, const bool isTranslucent, Color4u8 *dstColor, const size_t framebufferWidth, const size_t framebufferHeight, const bool isHorizontal, edge_fx_fl *left, edge_fx_fl *right)
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
		
		const size_t xl = left->X;
		const size_t xr = right->X;
		const size_t y  = left->Y;
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
					const size_t nxl = left->X;
					const size_t nxr = right->X;
					if (top)
					{
						const size_t xs = min(xl, xr);
						const size_t xe = max(xl, xr);
						for (size_t x = xs; x <= xe; x++)
						{
							const size_t adr = (y * framebufferWidth) + x;
							dstColor[adr].r = 63;
							dstColor[adr].g = 0;
							dstColor[adr].b = 0;
						}
					}
					else if (bottom)
					{
						const size_t xs = min(xl, xr);
						const size_t xe = max(xl, xr);
						for (size_t x = xs; x <= xe; x++)
						{
							const size_t adr = (y * framebufferWidth) + x;
							dstColor[adr].r = 63;
							dstColor[adr].g = 0;
							dstColor[adr].b = 0;
						}
					}
					else
					{
						size_t xs = min(xl, nxl);
						size_t xe = max(xl, nxl);
						for (size_t x = xs; x <= xe; x++)
						{
							const size_t adr = (y * framebufferWidth) + x;
							dstColor[adr].r = 63;
							dstColor[adr].g = 0;
							dstColor[adr].b = 0;
						}
						
						xs = min(xr, nxr);
						xe = max(xr, nxr);
						for (size_t x = xs; x <= xe; x++)
						{
							const size_t adr = (y * framebufferWidth) + x;
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
void RasterizerUnit<RENDERER>::_shape_engine(const POLYGON_ATTR polyAttr, const bool isTranslucent, Color4u8 *dstColor, const size_t framebufferWidth, const size_t framebufferHeight, int type)
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
	const size_t clippedPolyCount = this->_softRender->GetClippedPolyCount();
	if (clippedPolyCount == 0)
	{
		return;
	}
	
	Color4u8 *dstColor = this->_softRender->GetFramebuffer();
	const size_t dstWidth = this->_softRender->GetFramebufferWidth();
	const size_t dstHeight = this->_softRender->GetFramebufferHeight();
	
	const POLY *rawPolyList = this->_softRender->GetRawPolyList();
	const CPoly &firstClippedPoly = this->_softRender->GetClippedPolyByIndex(0);
	const POLY &firstPoly = rawPolyList[firstClippedPoly.index];
	POLYGON_ATTR polyAttr = firstPoly.attribute;
	TEXIMAGE_PARAM lastTexParams = firstPoly.texParam;
	u32 lastTexPalette = firstPoly.texPalette;
	
	this->_SetupTexture(firstPoly, 0);

	//iterate over polys
	for (size_t i = 0; i < clippedPolyCount; i++)
	{
		if (!RENDERER) _debug_thisPoly = (i == this->_softRender->_debug_drawClippedUserPoly);
		
		const CPoly &clippedPoly = this->_softRender->GetClippedPolyByIndex(i);
		const POLY &rawPoly = rawPolyList[clippedPoly.index];
		const size_t vertCount = (size_t)clippedPoly.type;
		const bool useLineHack = USELINEHACK && (rawPoly.vtxFormat & 4);
		
		polyAttr = rawPoly.attribute;
		const bool isTranslucent = GFX3D_IsPolyTranslucent(rawPoly);
		
		if (lastTexParams.value != rawPoly.texParam.value || lastTexPalette != rawPoly.texPalette)
		{
			lastTexParams = rawPoly.texParam;
			lastTexPalette = rawPoly.texPalette;
			this->_SetupTexture(rawPoly, i);
		}
		
		for (size_t j = 0; j < vertCount; j++)
			this->_verts[j] = &clippedPoly.clipVerts[j];
		for (size_t j = vertCount; j < MAX_CLIPPED_VERTS; j++)
			this->_verts[j] = NULL;
		
		if (!clippedPoly.isPolyBackFacing)
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

static void* SoftRasterizer_RunGetAndLoadAllTextures(void *arg)
{
	SoftRasterizerRenderer *softRender = (SoftRasterizerRenderer *)arg;
	softRender->GetAndLoadAllTextures();
	
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
#elif defined(ENABLE_NEON_A64)
	return new SoftRasterizerRenderer_NEON;
#elif defined(ENABLE_ALTIVEC)
	return new SoftRasterizerRenderer_AltiVec;
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
#elif defined(ENABLE_NEON_A64)
		SoftRasterizerRenderer_NEON *oldRenderer = (SoftRasterizerRenderer_NEON *)CurrentRenderer;
#elif defined(ENABLE_ALTIVEC)
		SoftRasterizerRenderer_AltiVec *oldRenderer = (SoftRasterizerRenderer_AltiVec *)CurrentRenderer;
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

FORCEINLINE void SoftRasterizerTexture::GetRenderSamplerCoordinates(const u8 wrapMode, Vector2s32 &sampleCoordInOut) const
{
	switch (wrapMode)
	{
		//flip none
		case 0x0: _hclamp(sampleCoordInOut.u);  _vclamp(sampleCoordInOut.v);  break;
		case 0x1: _hrepeat(sampleCoordInOut.u); _vclamp(sampleCoordInOut.v);  break;
		case 0x2: _hclamp(sampleCoordInOut.u);  _vrepeat(sampleCoordInOut.v); break;
		case 0x3: _hrepeat(sampleCoordInOut.u); _vrepeat(sampleCoordInOut.v); break;
		//flip S
		case 0x4: _hclamp(sampleCoordInOut.u);  _vclamp(sampleCoordInOut.v);  break;
		case 0x5: _hflip(sampleCoordInOut.u);   _vclamp(sampleCoordInOut.v);  break;
		case 0x6: _hclamp(sampleCoordInOut.u);  _vrepeat(sampleCoordInOut.v); break;
		case 0x7: _hflip(sampleCoordInOut.u);   _vrepeat(sampleCoordInOut.v); break;
		//flip T
		case 0x8: _hclamp(sampleCoordInOut.u);  _vclamp(sampleCoordInOut.v);  break;
		case 0x9: _hrepeat(sampleCoordInOut.u); _vclamp(sampleCoordInOut.v);  break;
		case 0xA: _hclamp(sampleCoordInOut.u);  _vflip(sampleCoordInOut.v);   break;
		case 0xB: _hrepeat(sampleCoordInOut.u); _vflip(sampleCoordInOut.v);   break;
		//flip both
		case 0xC: _hclamp(sampleCoordInOut.u);  _vclamp(sampleCoordInOut.v);  break;
		case 0xD: _hflip(sampleCoordInOut.u);   _vclamp(sampleCoordInOut.v);  break;
		case 0xE: _hclamp(sampleCoordInOut.u);  _vflip(sampleCoordInOut.v);   break;
		case 0xF: _hflip(sampleCoordInOut.u);   _vflip(sampleCoordInOut.v);   break;
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
	
	s32 newWidth = (s32)(this->_sizeS * scalingFactor);
	s32 newHeight = (s32)(this->_sizeT * scalingFactor);
	
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
	
	_clippedPolyList = (CPoly *)malloc_alignedCacheLine(CLIPPED_POLYLIST_SIZE * sizeof(CPoly));
	
	_task = NULL;
	
	_debug_drawClippedUserPoly = -1;
	
	_renderGeometryNeedsFinish = false;
	_framebufferAttributes = NULL;
	
	_enableHighPrecisionColorInterpolation = CommonSettings.GFX3D_HighResolutionInterpolateColor;
	_enableLineHack = CommonSettings.GFX3D_LineHack;
	_enableFragmentSamplingHack = CommonSettings.GFX3D_TXTHack;
	
	_HACK_viewer_rasterizerUnit.SetSLI(0, (u32)_framebufferHeight, false);
	
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
		
		_rasterizerUnit[0].SetSLI((u32)_threadPostprocessParam[0].startLine, (u32)_threadPostprocessParam[0].endLine, false);
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
			
			_rasterizerUnit[i].SetSLI((u32)_threadPostprocessParam[i].startLine, (u32)_threadPostprocessParam[i].endLine, false);
			_rasterizerUnit[i].SetRenderer(this);
			
			char name[16];
			snprintf(name, 16, "rasterizer %d", (int)i);
#ifdef DESMUME_COCOA
			// The Cocoa port takes advantage of hand-optimized thread priorities
			// to help stabilize performance when running SoftRasterizer.
			_task[i].start(false, 43, name);
#else
			_task[i].start(false, 0, name);
#endif
		}
	}
	
	__InitTables();
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
	
	free_aligned(this->_clippedPolyList);
	this->_clippedPolyList = NULL;
}

void SoftRasterizerRenderer::__InitTables()
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
}

ClipperMode SoftRasterizerRenderer::GetPreferredPolygonClippingMode() const
{
	return (this->_enableHighPrecisionColorInterpolation) ? ClipperMode_FullColorInterpolate : ClipperMode_Full;
}

void SoftRasterizerRenderer::GetAndLoadAllTextures()
{
	const POLY *rawPolyList = this->_rawPolyList;
	
	for (size_t i = 0; i < this->_clippedPolyCount; i++)
	{
		const CPoly &clippedPoly = this->_clippedPolyList[i];
		const POLY &rawPoly = rawPolyList[clippedPoly.index];
		
		//make sure all the textures we'll need are cached
		//(otherwise on a multithreaded system there will be multiple writers--
		//this SHOULD be read-only, although some day the texcache may collect statistics or something
		//and then it won't be safe.
		this->_textureList[i] = this->GetLoadedTextureFromPolygon(rawPoly, this->_enableTextureSampling);
	}
}

Render3DError SoftRasterizerRenderer::ApplyRenderingSettings(const GFX3D_State &renderState)
{
	this->_enableHighPrecisionColorInterpolation = CommonSettings.GFX3D_HighResolutionInterpolateColor;
	this->_enableLineHack = CommonSettings.GFX3D_LineHack;
	this->_enableFragmentSamplingHack = CommonSettings.GFX3D_TXTHack;
	
	return Render3D::ApplyRenderingSettings(renderState);
}

Render3DError SoftRasterizerRenderer::BeginRender(const GFX3D_State &renderState, const GFX3D_GeometryList &renderGList)
{
	// Force all threads to finish before rendering with new data
	for (size_t i = 0; i < this->_threadCount; i++)
	{
		this->_task[i].finish();
	}
	
	// Keep the current render states for later use
	this->currentRenderState = (GFX3D_State *)&renderState;
	this->_clippedPolyCount = renderGList.clippedPolyCount;
	this->_clippedPolyOpaqueCount = renderGList.clippedPolyOpaqueCount;
	memcpy(this->_clippedPolyList, renderGList.clippedPolyList, this->_clippedPolyCount * sizeof(CPoly));
	this->_rawPolyList = (POLY *)renderGList.rawPolyList;
	
	const bool doMultithreadedStateSetup = (this->_threadCount >= 2);
	
	if (doMultithreadedStateSetup)
	{
		this->_task[0].execute(&SoftRasterizer_RunGetAndLoadAllTextures, this);
	}
	else
	{
		this->GetAndLoadAllTextures();
	}
	
	// Convert the toon table colors
	ColorspaceConvertBuffer555To6665Opaque<false, false, BESwapDst>(renderState.toonTable16, (u32 *)this->toonColor32LUT, 32);
	
	if (this->_enableEdgeMark)
	{
		this->_UpdateEdgeMarkColorTable(renderState.edgeMarkColorTable);
	}
	
	if (this->_enableFog)
	{
		this->_UpdateFogTable(renderState.fogDensityTable);
	}
	
	if (doMultithreadedStateSetup)
	{
		this->_task[1].finish();
		this->_task[0].finish();
	}
	
	return RENDER3DERROR_NOERR;
}

Render3DError SoftRasterizerRenderer::RenderGeometry()
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

void SoftRasterizerRenderer::_UpdateEdgeMarkColorTable(const u16 *edgeMarkColorTable)
{
	//TODO: need to test and find out whether these get grabbed at flush time, or at render time
	//we can do this by rendering a 3d frame and then freezing the system, but only changing the edge mark colors
	for (size_t i = 0; i < 8; i++)
	{
		this->_edgeMarkTable[i].value = LE_TO_LOCAL_32( COLOR555TO6665(edgeMarkColorTable[i] & 0x7FFF, (this->currentRenderState->DISP3DCNT.EnableAntialiasing) ? 0x10 : 0x1F) );
		
		//zero 20-jun-2013 - this doesnt make any sense. at least, it should be related to the 0x8000 bit. if this is undocumented behaviour, lets write about which scenario proves it here, or which scenario is requiring this code.
		//// this seems to be the only thing that selectively disables edge marking
		//_edgeMarkDisabled[i] = (col == 0x7FFF);
		this->_edgeMarkDisabled[i] = false;
	}
}

void SoftRasterizerRenderer::_UpdateFogTable(const u8 *fogDensityTable)
{
	const s32 fogOffset = min<s32>( max<s32>((s32)this->currentRenderState->fogOffset, 0), 32768 );
	const s32 fogStep = 0x400 >> this->currentRenderState->fogShift;
	
	if (fogStep <= 0)
	{
		const s32 iMin = min<s32>( max<s32>(fogOffset, 0), 32768);
		const s32 iMax = min<s32>( max<s32>(fogOffset + 1, 0), 32768);
		
		// If the fog factor is 127, then treat it as 128.
		u8 fogWeight = (fogDensityTable[0] >= 127) ? 128 : fogDensityTable[0];
		memset(this->_fogTable, fogWeight, iMin);
		
		fogWeight = (fogDensityTable[31] >= 127) ? 128 : fogDensityTable[31];
		memset(this->_fogTable+iMax, fogWeight, 32768-iMax);
		
		return;
	}
	
	const s32 fogShiftInv = 10 - this->currentRenderState->fogShift;
	const s32 iMin = min<s32>( max<s32>((( 1 + 1) << fogShiftInv) + fogOffset + 1 - fogStep, 0), 32768);
	const s32 iMax = min<s32>( max<s32>(((32 + 1) << fogShiftInv) + fogOffset + 1 - fogStep, 0), 32768);
	assert(iMin <= iMax);
	
	// If the fog factor is 127, then treat it as 128.
	u8 fogWeight = (fogDensityTable[0] >= 127) ? 128 : fogDensityTable[0];
	memset(this->_fogTable, fogWeight, iMin);
	
	for (s32 depth = iMin; depth < iMax; depth++)
	{
#if 0
		// TODO: this might be a little slow;
		// We might need to hash all the variables and only recompute this when something changes.
		// But let's keep a naive version of this loop for the sake of clarity.
		if (depth < fogOffset)
		{
			this->_fogTable[depth] = fogDensityTable[0];
		}
		else
		{
			this->_fogTable[depth] = fogDensityTable[31];
			
			for (size_t idx = 0; idx < 32; idx++)
			{
				s32 value = fogOffset + (fogStep * (idx+1));
				if (depth <= value)
				{
					this->_fogTable[depth] = (idx == 0) ? fogDensityTable[0] : ((value-depth)*(fogDensityTable[idx-1]) + (fogStep-(value-depth))*(fogDensityTable[idx])) / fogStep;
					break;
				}
			}
		}
#else
		// this should behave exactly the same as the previous loop,
		// except much faster. (because it's not a 2d loop and isn't so branchy either)
		// maybe it's fast enough to not need to be cached, now.
		const s32 diff = depth - fogOffset + (fogStep - 1);
		const s32 interp = (diff & ~(fogStep - 1)) + fogOffset - depth;
		
		const size_t idx = (diff >> fogShiftInv) - 1;
		assert( (idx >= 1) && (idx < 32) );
		
		fogWeight = (u8)( ( (interp * fogDensityTable[idx-1]) + ((fogStep - interp) * fogDensityTable[idx]) ) >> fogShiftInv );
		this->_fogTable[depth] = (fogWeight >= 127) ? 128 : fogWeight;
#endif
	}
	
	fogWeight = (fogDensityTable[31] >= 127) ? 128 : fogDensityTable[31];
	memset(this->_fogTable+iMax, fogWeight, 32768-iMax);
}

Render3DError SoftRasterizerRenderer::RenderEdgeMarkingAndFog(const SoftRasterizerPostProcessParams &param)
{
	for (size_t i = param.startLine * this->_framebufferWidth, y = param.startLine; y < param.endLine; y++)
	{
		for (size_t x = 0; x < this->_framebufferWidth; x++, i++)
		{
			Color4u8 &dstColor = this->_framebufferColor[i];
			const u32 depth = this->_framebufferAttributes->depth[i];
			const u8 polyID = this->_framebufferAttributes->opaquePolyID[i];
			
			if (param.enableEdgeMarking)
			{
				// a good test case for edge marking is Sonic Rush:
				// - the edges are completely sharp/opaque on the very brief title screen intro,
				// - the level-start intro gets a pseudo-antialiasing effect around the silhouette,
				// - the character edges in-level are clearly transparent, and also show well through shield powerups.
				
				if (!this->_edgeMarkDisabled[polyID>>3] && this->_framebufferAttributes->isTranslucentPoly[i] == 0)
				{
					const bool isEdgeMarkingClearValues = ((polyID != this->_clearAttributes.opaquePolyID) && (depth < this->_clearAttributes.depth));
					
					const bool right = (x >= this->_framebufferWidth-1)  ? isEdgeMarkingClearValues : ((polyID != this->_framebufferAttributes->opaquePolyID[i+1])                       && (depth >= this->_framebufferAttributes->depth[i+1]));
					const bool down  = (y >= this->_framebufferHeight-1) ? isEdgeMarkingClearValues : ((polyID != this->_framebufferAttributes->opaquePolyID[i+this->_framebufferWidth]) && (depth >= this->_framebufferAttributes->depth[i+this->_framebufferWidth]));
					const bool left  = (x < 1)                           ? isEdgeMarkingClearValues : ((polyID != this->_framebufferAttributes->opaquePolyID[i-1])                       && (depth >= this->_framebufferAttributes->depth[i-1]));
					const bool up    = (y < 1)                           ? isEdgeMarkingClearValues : ((polyID != this->_framebufferAttributes->opaquePolyID[i-this->_framebufferWidth]) && (depth >= this->_framebufferAttributes->depth[i-this->_framebufferWidth]));
					
					Color4u8 edgeMarkColor = this->_edgeMarkTable[this->_framebufferAttributes->opaquePolyID[i] >> 3];
					
					if (right)
					{
						if (x < this->_framebufferWidth - 1)
						{
							edgeMarkColor = this->_edgeMarkTable[this->_framebufferAttributes->opaquePolyID[i+1] >> 3];
						}
					}
					else if (down)
					{
						if (y < this->_framebufferHeight - 1)
						{
							edgeMarkColor = this->_edgeMarkTable[this->_framebufferAttributes->opaquePolyID[i+this->_framebufferWidth] >> 3];
						}
					}
					else if (left)
					{
						if (x > 0)
						{
							edgeMarkColor = this->_edgeMarkTable[this->_framebufferAttributes->opaquePolyID[i-1] >> 3];
						}
					}
					else if (up)
					{
						if (y > 0)
						{
							edgeMarkColor = this->_edgeMarkTable[this->_framebufferAttributes->opaquePolyID[i-this->_framebufferWidth] >> 3];
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
				Color4u8 fogColor;
				fogColor.value = LE_TO_LOCAL_32( COLOR555TO6665(param.fogColor & 0x7FFF, (param.fogColor>>16) & 0x1F) );
				
				const size_t fogIndex = depth >> 9;
				assert(fogIndex < 32768);
				const u8 fogWeight = (this->_framebufferAttributes->isFogged[i] != 0) ? this->_fogTable[fogIndex] : 0;
				
				if (!param.fogAlphaOnly)
				{
					dstColor.r = ( (128-fogWeight)*dstColor.r + fogColor.r*fogWeight ) >> 7;
					dstColor.g = ( (128-fogWeight)*dstColor.g + fogColor.g*fogWeight ) >> 7;
					dstColor.b = ( (128-fogWeight)*dstColor.b + fogColor.b*fogWeight ) >> 7;
				}
				
				dstColor.a = ( (128-fogWeight)*dstColor.a + fogColor.a*fogWeight ) >> 7;
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
			
			this->_framebufferColor[iw].value = LE_TO_LOCAL_32( COLOR555TO6665(colorBuffer[ir] & 0x7FFF, (colorBuffer[ir] >> 15) * 0x1F) );
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

Render3DError SoftRasterizerRenderer::ClearUsingValues(const Color4u8 &clearColor6665, const FragmentAttributes &clearAttributes)
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

Render3DError SoftRasterizerRenderer::EndRender()
{
	// If we're not multithreaded, then just do the post-processing steps now.
	if (!this->_renderGeometryNeedsFinish)
	{
		if (this->_enableEdgeMark || this->_enableFog)
		{
			this->_threadPostprocessParam[0].enableEdgeMarking = this->_enableEdgeMark;
			this->_threadPostprocessParam[0].enableFog = this->_enableFog;
			this->_threadPostprocessParam[0].fogColor = this->currentRenderState->fogColor;
			this->_threadPostprocessParam[0].fogAlphaOnly = (this->currentRenderState->DISP3DCNT.FogOnlyAlpha != 0);
			
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
				this->_threadPostprocessParam[i].fogAlphaOnly = (this->currentRenderState->DISP3DCNT.FogOnlyAlpha != 0);
				
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
	
	Color4u8 *framebufferMain = (willFlushBuffer32 && (this->_outputFormat == NDSColorFormat_BGR888_Rev)) ? GPU->GetEngineMain()->Get3DFramebufferMain() : NULL;
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
		
		this->_rasterizerUnit[0].SetSLI((u32)this->_threadPostprocessParam[0].startLine, (u32)this->_threadPostprocessParam[0].endLine, false);
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
			
			this->_rasterizerUnit[i].SetSLI((u32)this->_threadPostprocessParam[i].startLine, (u32)this->_threadPostprocessParam[i].endLine, false);
		}
	}
	
	return RENDER3DERROR_NOERR;
}

#if defined(ENABLE_AVX) || defined(ENABLE_SSE2) || defined(ENABLE_NEON_A64) || defined(ENABLE_ALTIVEC)

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
Render3DError SoftRasterizer_SIMD<SIMDBYTES>::ClearUsingValues(const Color4u8 &clearColor6665, const FragmentAttributes &clearAttributes)
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

#endif // defined(ENABLE_AVX) || defined(ENABLE_SSE2) || defined(ENABLE_NEON_A64) || defined(ENABLE_ALTIVEC)

#if defined(ENABLE_AVX2)

void SoftRasterizerRenderer_AVX2::LoadClearValues(const Color4u8 &clearColor6665, const FragmentAttributes &clearAttributes)
{
	this->_clearColor_v256u32					= _mm256_set1_epi32(clearColor6665.value);
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
	for (size_t i = startPixel; i < endPixel; i+=sizeof(v256u8))
	{
		_mm256_stream_si256((v256u32 *)(this->_framebufferColor + i) + 0, this->_clearColor_v256u32);
		_mm256_stream_si256((v256u32 *)(this->_framebufferColor + i) + 1, this->_clearColor_v256u32);
		_mm256_stream_si256((v256u32 *)(this->_framebufferColor + i) + 2, this->_clearColor_v256u32);
		_mm256_stream_si256((v256u32 *)(this->_framebufferColor + i) + 3, this->_clearColor_v256u32);
		
		_mm256_stream_si256((v256u32 *)(this->_framebufferAttributes->depth + i) + 0, this->_clearDepth_v256u32);
		_mm256_stream_si256((v256u32 *)(this->_framebufferAttributes->depth + i) + 1, this->_clearDepth_v256u32);
		_mm256_stream_si256((v256u32 *)(this->_framebufferAttributes->depth + i) + 2, this->_clearDepth_v256u32);
		_mm256_stream_si256((v256u32 *)(this->_framebufferAttributes->depth + i) + 3, this->_clearDepth_v256u32);
		
		_mm256_stream_si256((v256u8 *)(this->_framebufferAttributes->opaquePolyID + i), this->_clearAttrOpaquePolyID_v256u8);
		_mm256_stream_si256((v256u8 *)(this->_framebufferAttributes->translucentPolyID + i), this->_clearAttrTranslucentPolyID_v256u8);
		_mm256_stream_si256((v256u8 *)(this->_framebufferAttributes->stencil + i), this->_clearAttrStencil_v256u8);
		_mm256_stream_si256((v256u8 *)(this->_framebufferAttributes->isFogged + i), this->_clearAttrIsFogged_v256u8);
		_mm256_stream_si256((v256u8 *)(this->_framebufferAttributes->isTranslucentPoly + i), this->_clearAttrIsTranslucentPoly_v256u8);
		_mm256_stream_si256((v256u8 *)(this->_framebufferAttributes->polyFacing + i), this->_clearAttrPolyFacing_v256u8);
	}
}

#elif defined(ENABLE_SSE2)

void SoftRasterizerRenderer_SSE2::LoadClearValues(const Color4u8 &clearColor6665, const FragmentAttributes &clearAttributes)
{
	this->_clearColor_v128u32					= _mm_set1_epi32(clearColor6665.value);
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
	for (size_t i = startPixel; i < endPixel; i+=sizeof(v128u8))
	{
		_mm_stream_si128((v128u32 *)(this->_framebufferColor + i) + 0, this->_clearColor_v128u32);
		_mm_stream_si128((v128u32 *)(this->_framebufferColor + i) + 1, this->_clearColor_v128u32);
		_mm_stream_si128((v128u32 *)(this->_framebufferColor + i) + 2, this->_clearColor_v128u32);
		_mm_stream_si128((v128u32 *)(this->_framebufferColor + i) + 3, this->_clearColor_v128u32);
		
		_mm_stream_si128((v128u32 *)(this->_framebufferAttributes->depth + i) + 0, this->_clearDepth_v128u32);
		_mm_stream_si128((v128u32 *)(this->_framebufferAttributes->depth + i) + 1, this->_clearDepth_v128u32);
		_mm_stream_si128((v128u32 *)(this->_framebufferAttributes->depth + i) + 2, this->_clearDepth_v128u32);
		_mm_stream_si128((v128u32 *)(this->_framebufferAttributes->depth + i) + 3, this->_clearDepth_v128u32);
		
		_mm_stream_si128((v128u8 *)(this->_framebufferAttributes->opaquePolyID + i), this->_clearAttrOpaquePolyID_v128u8);
		_mm_stream_si128((v128u8 *)(this->_framebufferAttributes->translucentPolyID + i), this->_clearAttrTranslucentPolyID_v128u8);
		_mm_stream_si128((v128u8 *)(this->_framebufferAttributes->stencil + i), this->_clearAttrStencil_v128u8);
		_mm_stream_si128((v128u8 *)(this->_framebufferAttributes->isFogged + i), this->_clearAttrIsFogged_v128u8);
		_mm_stream_si128((v128u8 *)(this->_framebufferAttributes->isTranslucentPoly + i), this->_clearAttrIsTranslucentPoly_v128u8);
		_mm_stream_si128((v128u8 *)(this->_framebufferAttributes->polyFacing + i), this->_clearAttrPolyFacing_v128u8);
	}
}

#elif defined(ENABLE_NEON_A64)

void SoftRasterizerRenderer_NEON::LoadClearValues(const Color4u8 &clearColor6665, const FragmentAttributes &clearAttributes)
{
	this->_clearColor_v128u32x4.val[0]                = vdupq_n_u32(clearColor6665.value);
	this->_clearColor_v128u32x4.val[1]                = this->_clearColor_v128u32x4.val[0];
	this->_clearColor_v128u32x4.val[2]                = this->_clearColor_v128u32x4.val[0];
	this->_clearColor_v128u32x4.val[3]                = this->_clearColor_v128u32x4.val[0];
	
	this->_clearDepth_v128u32x4.val[0]                = vdupq_n_u32(clearAttributes.depth);
	this->_clearDepth_v128u32x4.val[1]                = this->_clearDepth_v128u32x4.val[0];
	this->_clearDepth_v128u32x4.val[2]                = this->_clearDepth_v128u32x4.val[0];
	this->_clearDepth_v128u32x4.val[3]                = this->_clearDepth_v128u32x4.val[0];
	
	this->_clearAttrOpaquePolyID_v128u8x4.val[0]      = vdupq_n_u8(clearAttributes.opaquePolyID);
	this->_clearAttrOpaquePolyID_v128u8x4.val[1]      = this->_clearAttrOpaquePolyID_v128u8x4.val[0];
	this->_clearAttrOpaquePolyID_v128u8x4.val[2]      = this->_clearAttrOpaquePolyID_v128u8x4.val[0];
	this->_clearAttrOpaquePolyID_v128u8x4.val[3]      = this->_clearAttrOpaquePolyID_v128u8x4.val[0];
	
	this->_clearAttrTranslucentPolyID_v128u8x4.val[0] = vdupq_n_u8(clearAttributes.translucentPolyID);
	this->_clearAttrTranslucentPolyID_v128u8x4.val[1] = this->_clearAttrTranslucentPolyID_v128u8x4.val[0];
	this->_clearAttrTranslucentPolyID_v128u8x4.val[2] = this->_clearAttrTranslucentPolyID_v128u8x4.val[0];
	this->_clearAttrTranslucentPolyID_v128u8x4.val[3] = this->_clearAttrTranslucentPolyID_v128u8x4.val[0];
	
	this->_clearAttrStencil_v128u8x4.val[0]           = vdupq_n_u8(clearAttributes.stencil);
	this->_clearAttrStencil_v128u8x4.val[1]           = this->_clearAttrStencil_v128u8x4.val[0];
	this->_clearAttrStencil_v128u8x4.val[2]           = this->_clearAttrStencil_v128u8x4.val[0];
	this->_clearAttrStencil_v128u8x4.val[3]           = this->_clearAttrStencil_v128u8x4.val[0];
	
	this->_clearAttrIsFogged_v128u8x4.val[0]          = vdupq_n_u8(clearAttributes.isFogged);
	this->_clearAttrIsFogged_v128u8x4.val[1]          = this->_clearAttrIsFogged_v128u8x4.val[0];
	this->_clearAttrIsFogged_v128u8x4.val[2]          = this->_clearAttrIsFogged_v128u8x4.val[0];
	this->_clearAttrIsFogged_v128u8x4.val[3]          = this->_clearAttrIsFogged_v128u8x4.val[0];
	
	this->_clearAttrIsTranslucentPoly_v128u8x4.val[0] = vdupq_n_u8(clearAttributes.isTranslucentPoly);
	this->_clearAttrIsTranslucentPoly_v128u8x4.val[1] = this->_clearAttrIsTranslucentPoly_v128u8x4.val[0];
	this->_clearAttrIsTranslucentPoly_v128u8x4.val[2] = this->_clearAttrIsTranslucentPoly_v128u8x4.val[0];
	this->_clearAttrIsTranslucentPoly_v128u8x4.val[3] = this->_clearAttrIsTranslucentPoly_v128u8x4.val[0];
	
	this->_clearAttrPolyFacing_v128u8x4.val[0]        = vdupq_n_u8(clearAttributes.polyFacing);
	this->_clearAttrPolyFacing_v128u8x4.val[1]        = this->_clearAttrPolyFacing_v128u8x4.val[0];
	this->_clearAttrPolyFacing_v128u8x4.val[2]        = this->_clearAttrPolyFacing_v128u8x4.val[0];
	this->_clearAttrPolyFacing_v128u8x4.val[3]        = this->_clearAttrPolyFacing_v128u8x4.val[0];
}

void SoftRasterizerRenderer_NEON::ClearUsingValues_Execute(const size_t startPixel, const size_t endPixel)
{
	for (size_t i = startPixel; i < endPixel; i+=(sizeof(v128u8)*4))
	{
		vst1q_u32_x4((u32 *)(this->_framebufferColor + i) +  0, this->_clearColor_v128u32x4);
		vst1q_u32_x4((u32 *)(this->_framebufferColor + i) + 16, this->_clearColor_v128u32x4);
		vst1q_u32_x4((u32 *)(this->_framebufferColor + i) + 32, this->_clearColor_v128u32x4);
		vst1q_u32_x4((u32 *)(this->_framebufferColor + i) + 48, this->_clearColor_v128u32x4);
		
		vst1q_u32_x4((this->_framebufferAttributes->depth + i) +  0, this->_clearDepth_v128u32x4);
		vst1q_u32_x4((this->_framebufferAttributes->depth + i) + 16, this->_clearDepth_v128u32x4);
		vst1q_u32_x4((this->_framebufferAttributes->depth + i) + 32, this->_clearDepth_v128u32x4);
		vst1q_u32_x4((this->_framebufferAttributes->depth + i) + 48, this->_clearDepth_v128u32x4);
		
		vst1q_u8_x4((this->_framebufferAttributes->opaquePolyID + i),      this->_clearAttrOpaquePolyID_v128u8x4);
		vst1q_u8_x4((this->_framebufferAttributes->translucentPolyID + i), this->_clearAttrTranslucentPolyID_v128u8x4);
		vst1q_u8_x4((this->_framebufferAttributes->stencil + i),           this->_clearAttrStencil_v128u8x4);
		vst1q_u8_x4((this->_framebufferAttributes->isFogged + i),          this->_clearAttrIsFogged_v128u8x4);
		vst1q_u8_x4((this->_framebufferAttributes->isTranslucentPoly + i), this->_clearAttrIsTranslucentPoly_v128u8x4);
		vst1q_u8_x4((this->_framebufferAttributes->polyFacing + i),        this->_clearAttrPolyFacing_v128u8x4);
	}
}

#elif defined(ENABLE_ALTIVEC)

void SoftRasterizerRenderer_AltiVec::LoadClearValues(const Color4u8 &clearColor6665, const FragmentAttributes &clearAttributes)
{
	this->_clearColor_v128u32					= (v128u32){clearColor6665.value,clearColor6665.value,clearColor6665.value,clearColor6665.value};
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

void SoftRasterizerRenderer_AltiVec::ClearUsingValues_Execute(const size_t startPixel, const size_t endPixel)
{
	for (size_t i = startPixel; i < endPixel; i+=sizeof(v128u8))
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
