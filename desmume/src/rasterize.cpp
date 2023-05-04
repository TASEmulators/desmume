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


using std::min;
using std::max;
using std::swap;

template<typename T> T _min(T a, T b, T c) { return min(min(a,b),c); }
template<typename T> T _max(T a, T b, T c) { return max(max(a,b),c); }
template<typename T> T _min(T a, T b, T c, T d) { return min(_min(a,b,d),c); }
template<typename T> T _max(T a, T b, T c, T d) { return max(_max(a,b,d),c); }

static u8 modulate_table[64][64];
static u8 decal_table[32][64][64];

typedef s32 fixed28_4;

// handle floor divides and mods correctly 
static FORCEINLINE void FloorDivMod(const s64 inNumerator, const s64 inDenominator, s64 &outFloor, s64 &outMod, bool &outFailure)
{
	//These must be caused by invalid or degenerate shapes.. not sure yet.
	//check it out in the mario face intro of SM64
	//so, we have to take out the assert.
	//I do know that we handle SOME invalid shapes without crashing,
	//since I see them acting poppy in a way that doesnt happen in the HW.. so alas it is also incorrect.
	//This particular incorrectness is not likely ever to get fixed!

	//assert(inDenominator > 0);

	//but we have to bail out since our handling for these cases currently steps scanlines 
	//the wrong way and goes totally nuts (freezes)
	if (inDenominator <= 0)
	{
		outFailure = true;
	}

	if (inNumerator >= 0)
	{
		// positive case, C is okay
		outFloor = inNumerator / inDenominator;
		outMod = inNumerator % inDenominator;
	}
	else
	{
		// Numerator is negative, do the right thing
		outFloor = -((-inNumerator) / inDenominator);
		outMod = (-inNumerator) % inDenominator;
		if (outMod)
		{
			// there is a remainder
			outFloor--;
			outMod = inDenominator - outMod;
		}
	}
}

static FORCEINLINE s32 Ceil16_16(s32 fixed_16_16)
{
	s32 ReturnValue;
	s32 Numerator = fixed_16_16 - 1 + 65536;
	
	if (Numerator >= 0)
	{
		ReturnValue = Numerator / 65536;
	}
	else
	{
		// deal with negative numerators correctly
		ReturnValue = -((-Numerator) / 65536);
		ReturnValue -= ((-Numerator) % 65536) ? 1 : 0;
	}
	
	return ReturnValue;
}

static FORCEINLINE s32 Ceil28_4(fixed28_4 Value)
{
	s32 ReturnValue;
	s32 Numerator = Value - 1 + 16;
	
	if (Numerator >= 0)
	{
		ReturnValue = Numerator / 16;
	}
	else
	{
		// deal with negative numerators correctly
		ReturnValue = -((-Numerator) / 16);
		ReturnValue -= ((-Numerator) % 16) ? 1 : 0;
	}
	
	return ReturnValue;
}

struct edge_fx_fl
{
	edge_fx_fl() {}
	edge_fx_fl(const s32 top, const s32 bottom, NDSVertex **vtx, SoftRasterizerPrecalculation **precalc, bool &failure);
	FORCEINLINE s32 Step();
	
	NDSVertex **vtx;
	
	s32 x, xStep; // DDA info for x
	s64 errorTerm, numerator, denominator; // DDA info for x
	
	s32 y; // Current Y
	s32 height; // Vertical count
	
	struct InterpolantFloat
	{
		float curr;
		float step;
		float stepExtra;
		
		FORCEINLINE void doStep() { this->curr += this->step; }
		FORCEINLINE void doStepExtra() { this->curr += this->stepExtra; }
		
		FORCEINLINE void initialize(const float top)
		{
			this->curr = top;
			this->step = 0.0f;
			this->stepExtra = 0.0f;
		}
		
		FORCEINLINE void initialize(const float top,
		                            const float bottom,
		                            float dx,
		                            float dy,
		                            const float inXStep,
		                            const float xPrestep,
		                            const float yPrestep)
		{
			dx = 0.0f;
			dy *= (bottom-top);
			
			this->curr = top + (yPrestep * dy) + (xPrestep * dx);
			this->step = (inXStep * dx) + dy;
			this->stepExtra = dx;
		}
	};
	
	struct Interpolant
	{
		s64 curr;
		s64 step;
		s64 stepExtra;
		
		FORCEINLINE void doStep() { this->curr += this->step; }
		FORCEINLINE void doStepExtra() { this->curr += this->stepExtra; }
		
		FORCEINLINE void initialize(const s64 top) // fixed
		{
			this->curr = top; // fixed
			this->step = 0; // fixed
			this->stepExtra = 0; // fixed
		}
		
		FORCEINLINE void initialize(const s64 top, // fixed
		                            const s64 bottom, // fixed
		                            const s64 dxN, const s64 dxD, // 16.16
		                            const s64 dyN, const s64 dyD, // 16.16
		                            const s64 inXStep, // 32.0
		                            const s64 xPrestepN, const s64 xPrestepD, // 48.16
		                            const s64 yPrestepN, const s64 yPrestepD) // 48.16
		{
			const s64 dx = 0; // fixed
			
			if ( (dyD != 0) && (yPrestepD != 0) )
			{
				const s64 dy = dyN * (bottom - top) / dyD; // 16.16 * (fixed - fixed) / 16.16 = fixed
				
				this->curr = top + (yPrestepN * dy / yPrestepD) + (xPrestepN * dx / xPrestepD);
				// fixed + ((48.16 * fixed) / 48.16) + ((48.16 * fixed) / 48.16) = fixed
				this->step = (inXStep * dx) + dy; // (32.0 * fixed) + fixed = fixed
				this->stepExtra = dx; // fixed
			}
			else
			{
				this->curr = top; // fixed
				this->step = (inXStep * dx); // 32.0 * fixed = fixed
				this->stepExtra = dx; // fixed
			}
		}
	};
	
#define NUM_INTERPOLANTS 7
	union
	{
		struct
		{
			InterpolantFloat invWFloat, zFloat, sFloat, tFloat, colorFloat[3];
		};
		InterpolantFloat interpolantsFloat[NUM_INTERPOLANTS];
	};
	
	union
	{
		struct
		{
			Interpolant invW, z, s, t, color[3];
		};
		Interpolant interpolants[NUM_INTERPOLANTS];
	};
	
	void FORCEINLINE doStepInterpolants()
	{
		for (int i = 0; i < NUM_INTERPOLANTS; i++)
		{
			interpolants[i].doStep();
			interpolantsFloat[i].doStep();
		}
	}
	
	void FORCEINLINE doStepExtraInterpolants()
	{
		for (int i = 0; i < NUM_INTERPOLANTS; i++)
		{
			interpolants[i].doStepExtra();
			interpolantsFloat[i].doStepExtra();
		}
	}
#undef NUM_INTERPOLANTS
};

FORCEINLINE edge_fx_fl::edge_fx_fl(const s32 top, const s32 bottom, NDSVertex **vtx, SoftRasterizerPrecalculation **precalc, bool &failure)
{
	s64 x_64 = precalc[top]->positionCeil.x;
	s64 y_64 = precalc[top]->positionCeil.y;
	s64 xStep_64;
	
	this->vtx = vtx;
	
	this->y = (s32)y_64; // 16.16 to 16.0
	const s32 yEnd = (s32)precalc[bottom]->positionCeil.y; // 16.16 to 16.0
	this->height = yEnd - this->y; // 16.0
	
	this->x = (s32)x_64; // 16.16 to 16.0
	const s32 xEnd =  (s32)precalc[bottom]->positionCeil.x; // 16.16 to 16.0
	const s32 width = xEnd - this->x; // 16.0, can be negative
	
	// even if Height == 0, give some info for horizontal line poly
	if ( (this->height != 0) || (width != 0) )
	{
		s64 dN = (s64)vtx[bottom]->position.y - (s64)vtx[top]->position.y; // 16.16
		s64 dM = (s64)vtx[bottom]->position.x - (s64)vtx[top]->position.x; // 16.16
		
		if (dN != 0)
		{
			const s64 InitialNumerator = (s64)(dM*65536*y_64 - dM*vtx[top]->position.y + dN*vtx[top]->position.x - 1 + dN*65536); // 32.32
			FloorDivMod(InitialNumerator, dN * (1LL << 16), x_64, this->errorTerm, failure); // 32.32, 32.32; floor is 64.0, mod is 32.32
			FloorDivMod(dM * (1LL << 16), dN * (1LL << 16), xStep_64, this->numerator, failure); // 32.32, 32.32; floor is 64.0, mod is 32.32
			
			this->x = (s32)x_64; // 32.0
			this->xStep = (s32)xStep_64; // 32.0
			this->denominator = dN * (1LL << 16); // 16.16 to 32.32
		}
		else
		{
			this->errorTerm = 0; // 0.32
			this->xStep = width; // 32.0
			this->numerator = 0; // 0.32
			this->denominator = 1; // 0.32
			dN = 1; // 0.32
		}
		
		// Note that we can't precalculate xPrestep because x_64 can be modified by an earlier call to FloorDivMod().
		const s64 xPrestepFixedN = (x_64 * (1LL << 16)) - (s64)vtx[top]->position.x; // 48.16 - 16.16
		const s64 xPrestepFixedD = (1LL << 16); // 1.16
		const s64 yPrestepFixedN = precalc[top]->yPrestep; // 48.16 - 16.16
		const s64 yPrestepFixedD = (1LL << 16); // 1.16
		
		const s64 dxFixedN = (1LL << 16); // 1.16
		const s64 dxFixedD = dM; // 16.16
		const s64 dyFixedN = (1LL << 16); // 1.16
		const s64 dyFixedD = dN; // 16.16
		
		const float xPrestepFloat = (float)( (double)(x_64*65536 - vtx[top]->position.x) / 65536.0 ); // 16.16, 16.16; result is normalized
		const float yPrestepFloat = precalc[top]->yPrestepNormalized; // 16.16, 16.16; result is normalized
		const float dxFloat = 65536.0f / (float)dM; // 16.16; result is normalized
		const float dyFloat = 65536.0f / (float)dN; // 16.16; result is normalized
		
#define W_INTEGERBITS 20 // W coordinates should be coming in as 20.12.
#define W_PRECISION (64 - W_INTEGERBITS) // We need to calculate the inverse of W, and so we want to maintain as much precision as possible for this.
#define Z_EXTRAPRECISION 12 // Z is calculated in 64-bit and we have the extra bits, so use them to increase our precision. Mostly used for interpolating Z in higher-than-native resolution rendering.
#define TC_PRECISION (W_PRECISION - 4) // Texture coordinate precision is based on W_PRECISION, but needs a 16x multiplier to map correctly.
		
		invW.initialize(precalc[top]->invWPosition,
						precalc[bottom]->invWPosition,
		                dxFixedN, dxFixedD,
		                dyFixedN, dyFixedD,
		                this->xStep,
		                xPrestepFixedN, xPrestepFixedD,
		                yPrestepFixedN, yPrestepFixedD);
		
		z.initialize(precalc[top]->zPosition,
					 precalc[bottom]->zPosition,
		             dxFixedN, dxFixedD,
		             dyFixedN, dyFixedD,
		             this->xStep,
		             xPrestepFixedN, xPrestepFixedD,
		             yPrestepFixedN, yPrestepFixedD);
		
		s.initialize(precalc[top]->texCoord.s,
					 precalc[bottom]->texCoord.s,
		             dxFixedN, dxFixedD,
		             dyFixedN, dyFixedD,
		             this->xStep,
		             xPrestepFixedN, xPrestepFixedD,
		             yPrestepFixedN, yPrestepFixedD);
		
		t.initialize(precalc[top]->texCoord.t,
					 precalc[bottom]->texCoord.t,
		             dxFixedN, dxFixedD,
		             dyFixedN, dyFixedD,
		             this->xStep,
		             xPrestepFixedN, xPrestepFixedD,
		             yPrestepFixedN, yPrestepFixedD);
		
		invWFloat.initialize(precalc[top]->invWPositionNormalized,
		                     precalc[bottom]->invWPositionNormalized,
		                     dxFloat,
		                     dyFloat,
		                     (float)this->xStep,
		                     xPrestepFloat,
		                     yPrestepFloat);
		
		zFloat.initialize(precalc[top]->zPositionNormalized,
		                  precalc[bottom]->zPositionNormalized,
		                  dxFloat,
		                  dyFloat,
		                  (float)this->xStep,
		                  xPrestepFloat,
		                  yPrestepFloat);
		
		sFloat.initialize(precalc[top]->texCoordNormalized.s,
		                  precalc[bottom]->texCoordNormalized.s,
		                  dxFloat,
		                  dyFloat,
		                  (float)this->xStep,
		                  xPrestepFloat,
		                  yPrestepFloat);
		
		tFloat.initialize(precalc[top]->texCoordNormalized.t,
		                  precalc[bottom]->texCoordNormalized.t,
		                  dxFloat,
		                  dyFloat,
		                  (float)this->xStep,
		                  xPrestepFloat,
		                  yPrestepFloat);
		
		for (size_t i = 0; i < 3; i++)
		{
			color[i].initialize(precalc[top]->color.component[i],
								precalc[bottom]->color.component[i],
					            dxFixedN, dxFixedD,
					            dyFixedN, dyFixedD,
					            this->xStep,
					            xPrestepFixedN, xPrestepFixedD,
					            yPrestepFixedN, yPrestepFixedD);
			
			colorFloat[i].initialize(precalc[top]->colorNormalized.component[i],
									 precalc[bottom]->colorNormalized.component[i],
									 dxFloat,
									 dyFloat,
									 (float)this->xStep,
									 xPrestepFloat,
									 yPrestepFloat);
		}
	}
	else
	{
		// even if Width == 0 && Height == 0, give some info for pixel poly
		// example: Castlevania Portrait of Ruin, warp stone
		this->xStep = 1;
		this->numerator = 0;
		this->denominator = 1;
		this->errorTerm = 0;
		
		invW.initialize(precalc[top]->invWPosition);
		z.initialize(precalc[top]->zPosition);
		s.initialize(precalc[top]->texCoord.s);
		t.initialize(precalc[top]->texCoord.t);
		
		invWFloat.initialize(precalc[top]->invWPositionNormalized);
		zFloat.initialize(precalc[top]->zPositionNormalized);
		sFloat.initialize(precalc[top]->texCoordNormalized.s);
		tFloat.initialize(precalc[top]->texCoordNormalized.t);
		
		for (size_t i = 0; i < 3; i++)
		{
			color[i].initialize(precalc[top]->color.component[i]);
			colorFloat[i].initialize(precalc[top]->colorNormalized.component[i]);
		}
	}
}

FORCEINLINE s32 edge_fx_fl::Step()
{
	this->x += this->xStep;
	this->y++;
	this->height--;
	
	this->doStepInterpolants();

	this->errorTerm += this->numerator;
	if (this->errorTerm >= this->denominator)
	{
		this->x++;
		this->errorTerm -= this->denominator;
		this->doStepExtraInterpolants();
	}
	
	return this->height;
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
FORCEINLINE Color4u8 RasterizerUnit<RENDERER>::_sample(const Vector2s32 &texCoord)
{
	Vector2s32 sampleCoord = texCoord;
	const u32 *textureData = this->_currentTexture->GetRenderData();
	this->_currentTexture->GetRenderSamplerCoordinates(this->_textureWrapMode, sampleCoord);
	
	Color4u8 color;
	color.value = textureData[( sampleCoord.v << this->_currentTexture->GetRenderWidthShift() ) + sampleCoord.u];
	
	return color;
}

//round function - tkd3
template<bool RENDERER>
FORCEINLINE float RasterizerUnit<RENDERER>::_round_s(const float val)
{
	if (val > 0.0f)
	{
		return floorf(val*256.0f+0.5f)/256.0f; //this value(256.0) is good result.(I think)
	}
	else
	{
		return -1.0f*floorf(fabsf(val)*256.0f+0.5f)/256.0f;
	}
}

template<bool RENDERER> template<bool ISSHADOWPOLYGON>
FORCEINLINE void RasterizerUnit<RENDERER>::_shade(const PolygonMode polygonMode, const Color4u8 vtxColor, const Vector2s32 &texCoord, Color4u8 &outColor)
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
FORCEINLINE void RasterizerUnit<RENDERER>::_pixel(const POLYGON_ATTR polyAttr, const bool isPolyTranslucent, const u32 depth, const Color4u8 &vtxColor, const Vector2s32 texCoord, const size_t fragmentIndex, Color4u8 &dstColor)
{
	const GFX3D_State &renderState = *this->_softRender->currentRenderState;
	
	u32 &dstAttributeDepth				= this->_softRender->_framebufferAttributes->depth[fragmentIndex];
	u8 &dstAttributeOpaquePolyID		= this->_softRender->_framebufferAttributes->opaquePolyID[fragmentIndex];
	u8 &dstAttributeTranslucentPolyID	= this->_softRender->_framebufferAttributes->translucentPolyID[fragmentIndex];
	u8 &dstAttributeStencil				= this->_softRender->_framebufferAttributes->stencil[fragmentIndex];
	u8 &dstAttributeIsFogged			= this->_softRender->_framebufferAttributes->isFogged[fragmentIndex];
	u8 &dstAttributeIsTranslucentPoly	= this->_softRender->_framebufferAttributes->isTranslucentPoly[fragmentIndex];
	u8 &dstAttributePolyFacing			= this->_softRender->_framebufferAttributes->polyFacing[fragmentIndex];
	
	// run the depth test
	bool depthFail = false;
	
	if (polyAttr.DepthEqualTest_Enable)
	{
		// The EQUAL depth test is used if the polygon requests it. Note that the NDS doesn't perform
		// a true EQUAL test -- there is a set tolerance to it that makes it easier for pixels to
		// pass the depth test.
		const u32 minDepth = (u32)max<s32>(0x00000000, (s32)dstAttributeDepth - DEPTH_EQUALS_TEST_TOLERANCE);
		const u32 maxDepth = min<u32>(0x00FFFFFF, dstAttributeDepth + DEPTH_EQUALS_TEST_TOLERANCE);
		
		if (depth < minDepth || depth > maxDepth)
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
		if (depth > dstAttributeDepth)
		{
			depthFail = true;
		}
	}
	else
	{
		// The LESS depth test is the default type of depth test for all other conditions.
		if (depth >= dstAttributeDepth)
		{
			depthFail = true;
		}
	}

	if (depthFail)
	{
		//shadow mask polygons set stencil bit here
		if (ISSHADOWPOLYGON && polyAttr.PolygonID == 0)
		{
			dstAttributeStencil = 1;
		}
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
	
	Color4u8 shaderOutput;
	this->_shade<ISSHADOWPOLYGON>((PolygonMode)polyAttr.Mode, vtxColor, texCoord, shaderOutput);
	
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
		dstAttributeIsTranslucentPoly = isPolyTranslucent;
		dstAttributeIsFogged = polyAttr.Fog_Enable;
		dstColor = shaderOutput;
	}
	else
	{
		//dont overwrite pixels on translucent polys with the same polyids
		if (dstAttributeTranslucentPolyID == polyAttr.PolygonID)
		{
			return;
		}
		
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
	{
		dstAttributeDepth = depth;
	}
}

//draws a single scanline
template<bool RENDERER> template<bool ISFRONTFACING, bool ISSHADOWPOLYGON, bool USELINEHACK>
FORCEINLINE void RasterizerUnit<RENDERER>::_drawscanline(const POLYGON_ATTR polyAttr, const bool isPolyTranslucent, Color4u8 *dstColor, const size_t framebufferWidth, const size_t framebufferHeight, const edge_fx_fl &pLeft, const edge_fx_fl &pRight)
{
	const s32 xStart = pLeft.x;
	s32 rasterWidth = pRight.x - xStart;

	if (rasterWidth == 0)
	{
		if (USELINEHACK)
		{
			// HACK: workaround for vertical/slant line poly
			s32 leftWidth = pLeft.xStep;
			if (pLeft.errorTerm + pLeft.numerator >= pLeft.denominator)
			{
				leftWidth++;
			}
			
			s32 rightWidth = pRight.xStep;
			if (pRight.errorTerm + pRight.numerator >= pRight.denominator)
			{
				rightWidth++;
			}
			
			rasterWidth = max(1, max(abs(leftWidth), abs(rightWidth)));
		}
		else
		{
			return;
		}
	}

	//these are the starting values, taken from the left edge
	s64 invWInterpolated = pLeft.invW.curr;
	s64 zInterpolated = pLeft.z.curr;
	
	CACHE_ALIGN Vector2s64 texCoordInterpolated = {
		pLeft.s.curr,
		pLeft.t.curr
	};
	
	CACHE_ALIGN Color4s64 vtxColorInterpolated = {
		pLeft.color[0].curr,
		pLeft.color[1].curr,
		pLeft.color[2].curr,
		polyAttr.Alpha
	};
	
	float invWFloatInterpolated = pLeft.invWFloat.curr;
	float zFloatInterpolated = pLeft.zFloat.curr;
	
	CACHE_ALIGN Vector2f32 texCoordFloatInterpolated = {
		pLeft.sFloat.curr,
		pLeft.tFloat.curr
	};
	
	CACHE_ALIGN Color4f32 vtxColorFloatInterpolated = {
		pLeft.colorFloat[0].curr,
		pLeft.colorFloat[1].curr,
		pLeft.colorFloat[2].curr,
		(float)polyAttr.Alpha / 31.0f
	};
	
	//our dx values are taken from the steps up until the right edge
	const s64 rasterWidth_dx = rasterWidth;
	
	const s64 invWInterpolated_dx = (pRight.invW.curr - invWInterpolated) / rasterWidth_dx;
	const s64 zInterpolated_dx = (pRight.z.curr - zInterpolated) / rasterWidth_dx;
	
	const CACHE_ALIGN Vector2s64 texCoordInterpolated_dx = {
		(pRight.s.curr - texCoordInterpolated.s) / rasterWidth_dx,
		(pRight.t.curr - texCoordInterpolated.t) / rasterWidth_dx
	};
	
	const CACHE_ALIGN Color4s64 vtxColorInterpolated_dx = {
		(pRight.color[0].curr - vtxColorInterpolated.r) / rasterWidth_dx,
		(pRight.color[1].curr - vtxColorInterpolated.g) / rasterWidth_dx,
		(pRight.color[2].curr - vtxColorInterpolated.b) / rasterWidth_dx,
		0 / rasterWidth_dx
	};
	
	const float invWFloatInterpolated_dx = (pRight.invWFloat.curr - invWFloatInterpolated) / (float)rasterWidth_dx;
	const float zFloatInterpolated_dx = (pRight.zFloat.curr - zFloatInterpolated) / (float)rasterWidth_dx;
	
	const CACHE_ALIGN Vector2f32 texCoordFloatInterpolated_dx = {
		(pRight.sFloat.curr - texCoordFloatInterpolated.s) / rasterWidth_dx,
		(pRight.tFloat.curr - texCoordFloatInterpolated.t) / rasterWidth_dx
	};
	
	const CACHE_ALIGN Color4f32 vtxColorFloatInterpolated_dx = {
		(pRight.colorFloat[0].curr - vtxColorFloatInterpolated.r) / (float)rasterWidth_dx,
		(pRight.colorFloat[1].curr - vtxColorFloatInterpolated.g) / (float)rasterWidth_dx,
		(pRight.colorFloat[2].curr - vtxColorFloatInterpolated.b) / (float)rasterWidth_dx,
		0.0f / (float)rasterWidth_dx
	};

	//CONSIDER: in case some other math is wrong (shouldve been clipped OK), we might go out of bounds here.
	//better check the Y value.
	if ( (pLeft.y < 0) || (pLeft.y >= framebufferHeight) )
	{
		const float gpuScalingFactorHeight = (float)framebufferHeight / (float)GPU_FRAMEBUFFER_NATIVE_HEIGHT;
		printf("rasterizer rendering at y=%d! oops! (x%.1f)\n", pLeft.y, gpuScalingFactorHeight);
		return;
	}
	
	size_t adr = (pLeft.y * framebufferWidth) + xStart;
	s32 x = xStart;

	if (x < 0)
	{
		if (RENDERER && !USELINEHACK)
		{
			printf("rasterizer rendering at x=%d! oops!\n",x);
			return;
		}
		
		invWInterpolated += invWInterpolated_dx * -x;
		zInterpolated += zInterpolated_dx * -x;
		
		texCoordInterpolated.s += texCoordInterpolated_dx.s * -x;
		texCoordInterpolated.t += texCoordInterpolated_dx.t * -x;
		
		vtxColorInterpolated.r += vtxColorInterpolated_dx.r * -x;
		vtxColorInterpolated.g += vtxColorInterpolated_dx.g * -x;
		vtxColorInterpolated.b += vtxColorInterpolated_dx.b * -x;
		vtxColorInterpolated.a += vtxColorInterpolated_dx.a * -x;
		
		invWFloatInterpolated += invWFloatInterpolated_dx * (float)-x;
		zFloatInterpolated += zFloatInterpolated_dx * (float)-x;
		texCoordFloatInterpolated.s += texCoordFloatInterpolated_dx.s * (float)-x;
		texCoordFloatInterpolated.t += texCoordFloatInterpolated_dx.t * (float)-x;
		
		vtxColorFloatInterpolated.r += vtxColorFloatInterpolated_dx.r * (float)-x;
		vtxColorFloatInterpolated.g += vtxColorFloatInterpolated_dx.g * (float)-x;
		vtxColorFloatInterpolated.b += vtxColorFloatInterpolated_dx.b * (float)-x;
		vtxColorFloatInterpolated.a += vtxColorFloatInterpolated_dx.a * (float)-x;
		
		adr += -x;
		rasterWidth -= -x;
		x = 0;
	}
	
	if (x+rasterWidth > framebufferWidth)
	{
		if (RENDERER && !USELINEHACK)
		{
			const float gpuScalingFactorWidth = (float)framebufferWidth / (float)GPU_FRAMEBUFFER_NATIVE_WIDTH;
			printf("rasterizer rendering at x=%d! oops! (x%.2f)\n", x+rasterWidth-1, gpuScalingFactorWidth);
			return;
		}
		
		rasterWidth = (s32)framebufferWidth - x;
	}
	
	const s64 texScalingFactor = (s64)this->_currentTexture->GetScalingFactor();
	
	u32 depth;
	Color4u8 vtxColor;
	Vector2s32 texCoord;
	
	while (rasterWidth-- > 0)
	{
		if (this->_softRender->currentRenderState->SWAP_BUFFERS.DepthMode)
		{
			// not sure about the w-buffer depth value: this value was chosen to make the skybox, castle window decals, and water level render correctly in SM64
			depth = (u32)((1LL << W_PRECISION) / invWInterpolated);
		}
		else
		{
			// When using Z-depth, be sure to test against the following test cases:
			// - "Dragon Quest IV: Chapters of the Chosen" - Overworld map
			// - "Advance Wars: Days of Ruin" - Campaign map / In-game unit drawing on map
			// - "Etrian Odyssey III: The Drowned City" - Main menu, when the cityscape transitions between day and night
			// - "Super Monkey Ball: Touch & Roll" - 3D rendered horizon
			// - "Pokemon Diamond/Pearl" - Main map mode
			
			if (this->_softRender->_enableFragmentSamplingHack)
			{
				depth = (u32)((s32)this->_round_s(zFloatInterpolated * (float)0x00FFFFFF));
			}
			else
			{
				depth = (u32)(zInterpolated / (1LL << (Z_EXTRAPRECISION + 7)));
			}
			
			// TODO: Hack - Drop the LSB so that the overworld map in Dragon Quest IV shows up correctly.
			depth &= 0xFFFFFFFE;
		}
		
		if (this->_softRender->_enableFragmentSamplingHack)
		{
			vtxColor.r = max<u8>( 0x00, (u8)min<float>(63.0f, vtxColorFloatInterpolated.r / invWFloatInterpolated) );
			vtxColor.g = max<u8>( 0x00, (u8)min<float>(63.0f, vtxColorFloatInterpolated.g / invWFloatInterpolated) );
			vtxColor.b = max<u8>( 0x00, (u8)min<float>(63.0f, vtxColorFloatInterpolated.b / invWFloatInterpolated) );
			vtxColor.a = vtxColorInterpolated.a;
			
			texCoord.s = (s32)(texCoordFloatInterpolated.s * (float)texScalingFactor / invWFloatInterpolated);
			texCoord.t = (s32)(texCoordFloatInterpolated.t * (float)texScalingFactor / invWFloatInterpolated);
		}
		else
		{
			vtxColor.r = max<u8>( 0x00, (u8)min<s64>(0x3F, vtxColorInterpolated.r / invWInterpolated) );
			vtxColor.g = max<u8>( 0x00, (u8)min<s64>(0x3F, vtxColorInterpolated.g / invWInterpolated) );
			vtxColor.b = max<u8>( 0x00, (u8)min<s64>(0x3F, vtxColorInterpolated.b / invWInterpolated) );
			vtxColor.a = vtxColorInterpolated.a;
			
			texCoord.s = (s32)(texCoordInterpolated.s * texScalingFactor / invWInterpolated);
			texCoord.t = (s32)(texCoordInterpolated.t * texScalingFactor / invWInterpolated);
		}
		
		this->_pixel<ISFRONTFACING, ISSHADOWPOLYGON>(polyAttr,
													 isPolyTranslucent,
		                                             depth,
		                                             vtxColor,
		                                             texCoord,
		                                             adr,
		                                             dstColor[adr]);
		adr++;
		x++;
		
		invWInterpolated += invWInterpolated_dx;
		zInterpolated += zInterpolated_dx;
		
		texCoordInterpolated.s += texCoordInterpolated_dx.s;
		texCoordInterpolated.t += texCoordInterpolated_dx.t;
		
		vtxColorInterpolated.r += vtxColorInterpolated_dx.r;
		vtxColorInterpolated.g += vtxColorInterpolated_dx.g;
		vtxColorInterpolated.b += vtxColorInterpolated_dx.b;
		vtxColorInterpolated.a += vtxColorInterpolated_dx.a;
		
		invWFloatInterpolated += invWFloatInterpolated_dx;
		zFloatInterpolated += zFloatInterpolated_dx;
		texCoordFloatInterpolated.s += texCoordFloatInterpolated_dx.s;
		texCoordFloatInterpolated.t += texCoordFloatInterpolated_dx.t;
		
		vtxColorFloatInterpolated.r += vtxColorFloatInterpolated_dx.r;
		vtxColorFloatInterpolated.g += vtxColorFloatInterpolated_dx.g;
		vtxColorFloatInterpolated.b += vtxColorFloatInterpolated_dx.b;
		vtxColorFloatInterpolated.a += vtxColorFloatInterpolated_dx.a;
	}
}

//runs several scanlines, until an edge is finished
template<bool RENDERER> template<bool SLI, bool ISFRONTFACING, bool ISSHADOWPOLYGON, bool USELINEHACK>
void RasterizerUnit<RENDERER>::_runscanlines(const POLYGON_ATTR polyAttr, const bool isPolyTranslucent, Color4u8 *dstColor, const size_t framebufferWidth, const size_t framebufferHeight, const bool isHorizontal, edge_fx_fl &left, edge_fx_fl &right)
{
	//oh lord, hack city for edge drawing

	//do not overstep either of the edges
	s32 rasterHeight = min(left.height, right.height);
	bool first = true;

	//HACK: special handling for horizontal line poly
	if ( USELINEHACK && (left.height == 0) && (right.height == 0) && (left.y < framebufferHeight) && (left.y >= 0) )
	{
		const bool draw = ( !SLI || ((left.y >= this->_SLI_startLine) && (left.y < this->_SLI_endLine)) );
		if (draw)
		{
			this->_drawscanline<ISFRONTFACING, ISSHADOWPOLYGON, USELINEHACK>(polyAttr, isPolyTranslucent, dstColor, framebufferWidth, framebufferHeight, left, right);
		}
	}

	while (rasterHeight--)
	{
		const bool draw = ( !SLI || ((left.y >= this->_SLI_startLine) && (left.y < this->_SLI_endLine)) );
		if (draw)
		{
			this->_drawscanline<ISFRONTFACING, ISSHADOWPOLYGON, USELINEHACK>(polyAttr, isPolyTranslucent, dstColor, framebufferWidth, framebufferHeight, left, right);
		}
		
		const size_t xl = left.x;
		const size_t xr = right.x;
		const size_t y  = left.y;
		left.Step();
		right.Step();

		if (!RENDERER && _debug_thisPoly)
		{
			//debug drawing
			bool top = (isHorizontal && first);
			bool bottom = (!rasterHeight && isHorizontal);
			if (rasterHeight || top || bottom)
			{
				if (draw)
				{
					const size_t nxl = left.x;
					const size_t nxr = right.x;
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
#define ROTSWAP(X) if(TYPE>X) swap(this->_currentVtx[X-1],this->_currentVtx[X]);
	ROTSWAP(1); ROTSWAP(2); ROTSWAP(3); ROTSWAP(4);
	ROTSWAP(5); ROTSWAP(6); ROTSWAP(7); ROTSWAP(8); ROTSWAP(9);
#undef ROTSWAP
	
#define ROTSWAP(X) if(TYPE>X) swap(this->_currentPrecalc[X-1],this->_currentPrecalc[X]);
	ROTSWAP(1); ROTSWAP(2); ROTSWAP(3); ROTSWAP(4);
	ROTSWAP(5); ROTSWAP(6); ROTSWAP(7); ROTSWAP(8); ROTSWAP(9);
#undef ROTSWAP
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
	{
		for (size_t i = 0; i < TYPE/2; i++)
		{
			swap(this->_currentVtx[i], this->_currentVtx[TYPE-i-1]);
			swap(this->_currentPrecalc[i], this->_currentPrecalc[TYPE-i-1]);
		}
	}

	for (;;)
	{
		//this was the only way we could get this to unroll
		#define CHECKY(X) if(TYPE>X) if(this->_currentVtx[0]->position.y > this->_currentVtx[X]->position.y) goto doswap;
		CHECKY(1); CHECKY(2); CHECKY(3); CHECKY(4);
		CHECKY(5); CHECKY(6); CHECKY(7); CHECKY(8); CHECKY(9);
		break;
		
	doswap:
		this->_rot_verts<TYPE>();
	}
	
	while ( (this->_currentVtx[0]->position.y == this->_currentVtx[1]->position.y) &&
	        (this->_currentVtx[0]->position.x  > this->_currentVtx[1]->position.x) )
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
void RasterizerUnit<RENDERER>::_shape_engine(const POLYGON_ATTR polyAttr, const bool isPolyTranslucent, Color4u8 *dstColor, const size_t framebufferWidth, const size_t framebufferHeight, int type)
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
		
		if (step_left)
		{
			left = edge_fx_fl(_lv, lv-1, (NDSVertex **)&this->_currentVtx, (SoftRasterizerPrecalculation **)&this->_currentPrecalc, failure);
		}
		
		if (step_right)
		{
			right = edge_fx_fl(rv, rv+1, (NDSVertex **)&this->_currentVtx, (SoftRasterizerPrecalculation **)&this->_currentPrecalc, failure);
		}
		
		step_left = step_right = false;

		//handle a failure in the edge setup due to nutty polys
		if (failure)
		{
			return;
		}

		const bool isHorizontal = (left.y == right.y);
		this->_runscanlines<SLI, ISFRONTFACING, ISSHADOWPOLYGON, USELINEHACK>(polyAttr, isPolyTranslucent, dstColor, framebufferWidth, framebufferHeight, isHorizontal, left, right);
		
		//if we ran out of an edge, step to the next one
		if (right.height == 0)
		{
			step_right = true;
			rv++;
		}
		if (left.height == 0)
		{
			step_left = true;
			lv--;
		}

		//this is our completion condition: when our stepped edges meet in the middle
		if (lv <= rv+1)
		{
			break;
		}
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
	
	const SoftRasterizerPrecalculation *softRastPrecalc = this->_softRender->GetPrecalculationList();
	
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
		const bool isPolyTranslucent = GFX3D_IsPolyTranslucent(rawPoly);
		
		if (lastTexParams.value != rawPoly.texParam.value || lastTexPalette != rawPoly.texPalette)
		{
			lastTexParams = rawPoly.texParam;
			lastTexPalette = rawPoly.texPalette;
			this->_SetupTexture(rawPoly, i);
		}
		
		for (size_t j = 0; j < vertCount; j++)
		{
			this->_currentVtx[j] = &clippedPoly.vtx[j];
			this->_currentPrecalc[j] = &softRastPrecalc[(i * MAX_CLIPPED_VERTS) + j];
		}
		
		for (size_t j = vertCount; j < MAX_CLIPPED_VERTS; j++)
		{
			this->_currentVtx[j] = NULL;
			this->_currentPrecalc[j] = NULL;
		}
		
		if (!clippedPoly.isPolyBackFacing)
		{
			if (polyAttr.Mode == POLYGON_MODE_SHADOW)
			{
				if (useLineHack)
				{
					this->_shape_engine<SLI, true, true, true>(polyAttr, isPolyTranslucent, dstColor, dstWidth, dstHeight, vertCount);
				}
				else
				{
					this->_shape_engine<SLI, true, true, false>(polyAttr, isPolyTranslucent, dstColor, dstWidth, dstHeight, vertCount);
				}
			}
			else
			{
				if (useLineHack)
				{
					this->_shape_engine<SLI, true, false, true>(polyAttr, isPolyTranslucent, dstColor, dstWidth, dstHeight, vertCount);
				}
				else
				{
					this->_shape_engine<SLI, true, false, false>(polyAttr, isPolyTranslucent, dstColor, dstWidth, dstHeight, vertCount);
				}
			}
		}
		else
		{
			if (polyAttr.Mode == POLYGON_MODE_SHADOW)
			{
				if (useLineHack)
				{
					this->_shape_engine<SLI, false, true, true>(polyAttr, isPolyTranslucent, dstColor, dstWidth, dstHeight, vertCount);
				}
				else
				{
					this->_shape_engine<SLI, false, true, false>(polyAttr, isPolyTranslucent, dstColor, dstWidth, dstHeight, vertCount);
				}
			}
			else
			{
				if (useLineHack)
				{
					this->_shape_engine<SLI, false, false, true>(polyAttr, isPolyTranslucent, dstColor, dstWidth, dstHeight, vertCount);
				}
				else
				{
					this->_shape_engine<SLI, false, false, false>(polyAttr, isPolyTranslucent, dstColor, dstWidth, dstHeight, vertCount);
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

static void* SoftRasterizer_RunRasterizerPrecalculate(void *arg)
{
	SoftRasterizerRenderer *softRender = (SoftRasterizerRenderer *)arg;
	softRender->RasterizerPrecalculate();
	
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
	_precalc = (SoftRasterizerPrecalculation *)malloc_alignedCacheLine(CLIPPED_POLYLIST_SIZE * MAX_CLIPPED_VERTS * sizeof(SoftRasterizerPrecalculation));
	
	_task = NULL;
	
	_debug_drawClippedUserPoly = 0;
	
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
	
	free_aligned(this->_precalc);
	this->_precalc = NULL;
	
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

void SoftRasterizerRenderer::RasterizerPrecalculate()
{
	for (size_t i = 0, precalcIdx = 0; i < this->_clippedPolyCount; i++)
	{
		const CPoly &cPoly = this->_clippedPolyList[i];
		const size_t polyType = cPoly.type;
		
		precalcIdx = i * MAX_CLIPPED_VERTS;
		
		for (size_t j = 0; j < polyType; j++)
		{
			const NDSVertex &vtx = cPoly.vtx[j];
			SoftRasterizerPrecalculation &precalc = this->_precalc[precalcIdx];
			
			const s32 x = Ceil16_16(vtx.position.x);
			const s32 y = Ceil16_16(vtx.position.y);
			precalc.positionCeil.x = (s64)x;
			precalc.positionCeil.y = (s64)y;
			
			precalc.zPosition = (s64)vtx.position.z * (1LL << Z_EXTRAPRECISION);
			precalc.yPrestep = ((s64)y * (1LL << 16)) - (s64)vtx.position.y; // 48.16 - 16.16
			
			precalc.zPositionNormalized = (float)( (double)vtx.position.z / (double)(0x7FFFFFFF) );
			precalc.yPrestepNormalized = (float)( (double)precalc.yPrestep / 65536.0 );
			
			if (vtx.position.w != 0)
			{
				precalc.invWPosition = (1LL << W_PRECISION) / (s64)vtx.position.w;
				precalc.texCoord.s = (s64)vtx.texCoord.s * (1LL << TC_PRECISION) / (s64)vtx.position.w;
				precalc.texCoord.t = (s64)vtx.texCoord.t * (1LL << TC_PRECISION) / (s64)vtx.position.w;
				precalc.color.r = (s64)vtx.color.r * (1LL << W_PRECISION) / (s64)vtx.position.w;
				precalc.color.g = (s64)vtx.color.g * (1LL << W_PRECISION) / (s64)vtx.position.w;
				precalc.color.b = (s64)vtx.color.b * (1LL << W_PRECISION) / (s64)vtx.position.w;
				
				const float invWTCFloat = 256.0f / (float)vtx.position.w;
				const float invWFloat = 4096.0f / (float)vtx.position.w;
				precalc.invWPositionNormalized = invWFloat;
				precalc.texCoordNormalized.u = (float)vtx.texCoord.u * invWTCFloat;
				precalc.texCoordNormalized.v = (float)vtx.texCoord.v * invWTCFloat;
				precalc.colorNormalized.r = (float)vtx.color.r * invWFloat;
				precalc.colorNormalized.g = (float)vtx.color.g * invWFloat;
				precalc.colorNormalized.b = (float)vtx.color.b * invWFloat;
			}
			else
			{
				precalc.invWPosition = (1LL << W_PRECISION) / (1LL << 12);
				precalc.texCoord.s = (s64)vtx.texCoord.s * (1LL << TC_PRECISION) / (1LL << 12);
				precalc.texCoord.t = (s64)vtx.texCoord.t * (1LL << TC_PRECISION) / (1LL << 12);
				precalc.color.r = (s64)vtx.color.r * (1LL << W_PRECISION) / (1LL << 12);
				precalc.color.g = (s64)vtx.color.g * (1LL << W_PRECISION) / (1LL << 12);
				precalc.color.b = (s64)vtx.color.b * (1LL << W_PRECISION) / (1LL << 12);
				
				precalc.invWPositionNormalized = 1.0f;
				precalc.texCoordNormalized.u = (float)vtx.texCoord.u / 16.0f;
				precalc.texCoordNormalized.v = (float)vtx.texCoord.v / 16.0f;
				precalc.colorNormalized.r = (float)vtx.color.r;
				precalc.colorNormalized.g = (float)vtx.color.g;
				precalc.colorNormalized.b = (float)vtx.color.b;
			}
			
			precalcIdx++;
		}
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
		this->_task[1].execute(&SoftRasterizer_RunRasterizerPrecalculate, this);
	}
	else
	{
		this->GetAndLoadAllTextures();
		this->RasterizerPrecalculate();
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

const SoftRasterizerPrecalculation* SoftRasterizerRenderer::GetPrecalculationList() const
{
	return this->_precalc;
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
	memset(this->_precalc, 0, CLIPPED_POLYLIST_SIZE * MAX_CLIPPED_VERTS * sizeof(SoftRasterizerPrecalculation));
	
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
