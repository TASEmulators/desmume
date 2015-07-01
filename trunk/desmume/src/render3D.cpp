/*
	Copyright (C) 2006-2007 shash
	Copyright (C) 2008-2015 DeSmuME team

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

#include "render3D.h"

#include <string.h>

#ifdef ENABLE_SSE2
#include <emmintrin.h>
#endif

#ifdef ENABLE_SSSE3
#include <tmmintrin.h>
#endif

#include "bits.h"
#include "common.h"
#include "gfx3d.h"
#include "MMU.h"
#include "texcache.h"


static CACHE_ALIGN u32 dsDepthToD24_LUT[32768] = {0};
int cur3DCore = GPU3D_NULL;

GPU3DInterface gpu3DNull = { 
	"None",
	Render3DBaseCreate,
	Render3DBaseDestroy
};

GPU3DInterface *gpu3D = &gpu3DNull;
Render3D *BaseRenderer = NULL;
Render3D *CurrentRenderer = NULL;

void Render3D_Init()
{
	if (BaseRenderer == NULL)
	{
		BaseRenderer = new Render3D;
	}
	
	if (CurrentRenderer == NULL)
	{
		gpu3D = &gpu3DNull;
		cur3DCore = GPU3D_NULL;
		CurrentRenderer = BaseRenderer;
	}
}

void Render3D_DeInit()
{
	gpu3D->NDS_3D_Close();
	delete BaseRenderer;
	BaseRenderer = NULL;
}

bool NDS_3D_ChangeCore(int newCore)
{
	bool result = false;
		
	Render3DInterface *newRenderInterface = core3DList[newCore];
	if (newRenderInterface->NDS_3D_Init == NULL)
	{
		return result;
	}
	
	// Some resources are shared between renderers, such as the texture cache,
	// so we need to shut down the current renderer now to ensure that any
	// shared resources aren't in use.
	CurrentRenderer->RenderFinish();
	gpu3D->NDS_3D_Close();
	gpu3D = &gpu3DNull;
	cur3DCore = GPU3D_NULL;
	CurrentRenderer = BaseRenderer;
	
	Render3D *newRenderer = newRenderInterface->NDS_3D_Init();
	if (newRenderer == NULL)
	{
		return result;
	}
	
	Render3DError error = newRenderer->SetFramebufferSize(GPU_GetFramebufferWidth(), GPU_GetFramebufferHeight());
	if (error != RENDER3DERROR_NOERR)
	{
		return result;
	}
	
	gpu3D = newRenderInterface;
	cur3DCore = newCore;
	CurrentRenderer = newRenderer;
	
	result = true;
	return result;
}

Render3D* Render3DBaseCreate()
{
	BaseRenderer->Reset();
	return BaseRenderer;
}

void Render3DBaseDestroy()
{
	if (CurrentRenderer != BaseRenderer)
	{
		delete CurrentRenderer;
		CurrentRenderer = BaseRenderer;
	}
}

FragmentAttributesBuffer::FragmentAttributesBuffer(size_t newCount)
{
	count = newCount;
	
	depth = (u32 *)malloc_alignedCacheLine(count * sizeof(u32));
	opaquePolyID = (u8 *)malloc_alignedCacheLine(count * sizeof(u8));
	translucentPolyID = (u8 *)malloc_alignedCacheLine(count * sizeof(u8));
	stencil = (u8 *)malloc_alignedCacheLine(count * sizeof(u8));
	isFogged = (u8 *)malloc_alignedCacheLine(count * sizeof(u8));
	isTranslucentPoly = (u8 *)malloc_alignedCacheLine(count * sizeof(u8));
}

FragmentAttributesBuffer::~FragmentAttributesBuffer()
{
	free_aligned(depth);
	free_aligned(opaquePolyID);
	free_aligned(translucentPolyID);
	free_aligned(stencil);
	free_aligned(isFogged);
	free_aligned(isTranslucentPoly);
}

void FragmentAttributesBuffer::SetAtIndex(const size_t index, const FragmentAttributes &attr)
{
	this->depth[index]				= attr.depth;
	this->opaquePolyID[index]		= attr.opaquePolyID;
	this->translucentPolyID[index]	= attr.translucentPolyID;
	this->stencil[index]			= attr.stencil;
	this->isFogged[index]			= attr.isFogged;
	this->isTranslucentPoly[index]	= attr.isTranslucentPoly;
}

void FragmentAttributesBuffer::SetAll(const FragmentAttributes &attr)
{
#ifdef ENABLE_SSE2
	const size_t sseCount = count - (count % 16);
	
	const __m128i attrDepth_vec128				= _mm_set1_epi32(attr.depth);
	const __m128i attrOpaquePolyID_vec128		= _mm_set1_epi8(attr.opaquePolyID);
	const __m128i attrTranslucentPolyID_vec128	= _mm_set1_epi8(attr.translucentPolyID);
	const __m128i attrStencil_vec128			= _mm_set1_epi8(attr.stencil);
	const __m128i attrIsFogged_vec128			= _mm_set1_epi8(attr.isFogged);
	const __m128i attrIsTranslucentPoly_vec128	= _mm_set1_epi8(attr.isTranslucentPoly);
	
	for (size_t i = 0; i < sseCount; i += 16)
	{
		_mm_stream_si128((__m128i *)(this->depth +  0), attrDepth_vec128);
		_mm_stream_si128((__m128i *)(this->depth +  4), attrDepth_vec128);
		_mm_stream_si128((__m128i *)(this->depth +  8), attrDepth_vec128);
		_mm_stream_si128((__m128i *)(this->depth + 12), attrDepth_vec128);
		
		_mm_stream_si128((__m128i *)this->opaquePolyID, attrOpaquePolyID_vec128);
		_mm_stream_si128((__m128i *)this->translucentPolyID, attrTranslucentPolyID_vec128);
		_mm_stream_si128((__m128i *)this->stencil, attrStencil_vec128);
		_mm_stream_si128((__m128i *)this->isFogged, attrIsFogged_vec128);
		_mm_stream_si128((__m128i *)this->isTranslucentPoly, attrIsTranslucentPoly_vec128);
	}
	
	for (size_t i = sseCount; i < count; i++)
	{
		this->SetAtIndex(i, attr);
	}
#else
	for (size_t i = 0; i < count; i++)
	{
		this->SetAtIndex(i, attr);
	}
#endif
}

Render3D::Render3D()
{
	_renderID = RENDERID_NULL;
	_renderName = "None";
	
	static bool needTableInit = true;
	
	if (needTableInit)
	{
		for (size_t i = 0; i < 32768; i++)
		{
			dsDepthToD24_LUT[i] = (u32)DS_DEPTH15TO24(i);
		}
		
		needTableInit = false;
	}
	
	_framebufferWidth = GPU_FRAMEBUFFER_NATIVE_WIDTH;
	_framebufferHeight = GPU_FRAMEBUFFER_NATIVE_HEIGHT;
	_framebufferColorSizeBytes = 0;
	_framebufferColor = NULL;
	
	Reset();
}

Render3D::~Render3D()
{
	free_aligned(_framebufferColor);
	TexCache_Reset();
}

RendererID Render3D::GetRenderID()
{
	return this->_renderID;
}

std::string Render3D::GetName()
{
	return this->_renderName;
}

FragmentColor* Render3D::GetFramebuffer()
{
	return this->_framebufferColor;
}

size_t Render3D::GetFramebufferWidth()
{
	return this->_framebufferWidth;
}

size_t Render3D::GetFramebufferHeight()
{
	return this->_framebufferHeight;
}

Render3DError Render3D::SetFramebufferSize(size_t w, size_t h)
{
	if (w < GPU_FRAMEBUFFER_NATIVE_WIDTH || h < GPU_FRAMEBUFFER_NATIVE_HEIGHT)
	{
		return RENDER3DERROR_NOERR;
	}
	
	const size_t newFramebufferColorSizeBytes = w * h * sizeof(FragmentColor);
	FragmentColor *oldFramebufferColor = this->_framebufferColor;
	FragmentColor *newFramebufferColor = (FragmentColor *)malloc_alignedCacheLine(newFramebufferColorSizeBytes);
	
	this->_framebufferWidth = w;
	this->_framebufferHeight = h;
	this->_framebufferColorSizeBytes = newFramebufferColorSizeBytes;
	this->_framebufferColor = newFramebufferColor;
	
	free_aligned(oldFramebufferColor);
	
	return RENDER3DERROR_NOERR;
}

Render3DError Render3D::BeginRender(const GFX3D &engine)
{
	return RENDER3DERROR_NOERR;
}

Render3DError Render3D::RenderGeometry(const GFX3D_State &renderState, const POLYLIST *polyList, const INDEXLIST *indexList)
{
	return RENDER3DERROR_NOERR;
}

Render3DError Render3D::RenderEdgeMarking(const u16 *colorTable, const bool useAntialias)
{
	return RENDER3DERROR_NOERR;
}

Render3DError Render3D::RenderFog(const u8 *densityTable, const u32 color, const u32 offset, const u8 shift, const bool alphaOnly)
{
	return RENDER3DERROR_NOERR;
}

Render3DError Render3D::EndRender(const u64 frameCount)
{
	return RENDER3DERROR_NOERR;
}

Render3DError Render3D::FlushFramebuffer(FragmentColor *__restrict dstRGBA6665, u16 *__restrict dstRGBA5551)
{
	memcpy(dstRGBA6665, this->_framebufferColor, this->_framebufferColorSizeBytes);
	
	// Convert to RGBA5551
	for (size_t i = 0; i < (this->_framebufferWidth * this->_framebufferHeight); i++)
	{
		dstRGBA5551[i] = R6G6B6TORGB15(this->_framebufferColor[i].r, this->_framebufferColor[i].g, this->_framebufferColor[i].b) | ((this->_framebufferColor[i].a == 0) ? 0x0000 : 0x8000);
	}
	
	return RENDER3DERROR_NOERR;
}

Render3DError Render3D::UpdateToonTable(const u16 *toonTableBuffer)
{
	return RENDER3DERROR_NOERR;
}

Render3DError Render3D::ClearFramebuffer(const GFX3D_State &renderState)
{
	Render3DError error = RENDER3DERROR_NOERR;
	
	FragmentColor clearColor;
	clearColor.r =  renderState.clearColor & 0x1F;
	clearColor.g = (renderState.clearColor >> 5) & 0x1F;
	clearColor.b = (renderState.clearColor >> 10) & 0x1F;
	clearColor.a = (renderState.clearColor >> 16) & 0x1F;
	
	FragmentAttributes clearFragment;
	clearFragment.opaquePolyID = (renderState.clearColor >> 24) & 0x3F;
	//special value for uninitialized translucent polyid. without this, fires in spiderman2 dont display
	//I am not sure whether it is right, though. previously this was cleared to 0, as a guess,
	//but in spiderman2 some fires with polyid 0 try to render on top of the background
	clearFragment.translucentPolyID = kUnsetTranslucentPolyID;
	clearFragment.depth = renderState.clearDepth;
	clearFragment.stencil = 0;
	clearFragment.isTranslucentPoly = 0;
	clearFragment.isFogged = BIT15(renderState.clearColor);
	
	if (renderState.enableClearImage)
	{
		//the lion, the witch, and the wardrobe (thats book 1, suck it you new-school numberers)
		//uses the scroll registers in the main game engine
		const u16 *__restrict clearColorBuffer = (u16 *__restrict)MMU.texInfo.textureSlotAddr[2];
		const u16 *__restrict clearDepthBuffer = (u16 *__restrict)MMU.texInfo.textureSlotAddr[3];
		const u16 scrollBits = T1ReadWord(MMU.ARM9_REG, 0x356); //CLRIMAGE_OFFSET
		const u8 xScroll = scrollBits & 0xFF;
		const u8 yScroll = (scrollBits >> 8) & 0xFF;
		
		for (size_t dstIndex = 0, iy = 0; iy < GPU_FRAMEBUFFER_NATIVE_HEIGHT; iy++)
		{
			const size_t y = ((iy + yScroll) & 0xFF) << 8;
			
			for (size_t ix = 0; ix < GPU_FRAMEBUFFER_NATIVE_WIDTH; dstIndex++, ix++)
			{
				const size_t x = (ix + xScroll) & 0xFF;
				const size_t srcIndex = y | x;
				
				//this is tested by harry potter and the order of the phoenix.
				//TODO (optimization) dont do this if we are mapped to blank memory (such as in sonic chronicles)
				//(or use a special zero fill in the bulk clearing above)
				this->clearImageColor16Buffer[dstIndex] = clearColorBuffer[srcIndex];
				
				//this is tested quite well in the sonic chronicles main map mode
				//where depth values are used for trees etc you can walk behind
				this->clearImageDepthBuffer[dstIndex] = dsDepthToD24_LUT[clearDepthBuffer[srcIndex] & 0x7FFF];
				
				this->clearImageFogBuffer[dstIndex] = BIT15(clearDepthBuffer[srcIndex]);
				this->clearImagePolyIDBuffer[dstIndex] = clearFragment.opaquePolyID;
			}
		}
		
		error = this->ClearUsingImage(this->clearImageColor16Buffer, this->clearImageDepthBuffer, this->clearImageFogBuffer, this->clearImagePolyIDBuffer);
		if (error != RENDER3DERROR_NOERR)
		{
			error = this->ClearUsingValues(clearColor, clearFragment);
		}
	}
	else
	{
		error = this->ClearUsingValues(clearColor, clearFragment);
	}
	
	return error;
}

Render3DError Render3D::ClearUsingImage(const u16 *__restrict colorBuffer, const u32 *__restrict depthBuffer, const u8 *__restrict fogBuffer, const u8 *__restrict polyIDBuffer)
{
	return RENDER3DERROR_NOERR;
}

Render3DError Render3D::ClearUsingValues(const FragmentColor &clearColor, const FragmentAttributes &clearAttributes) const
{
	return RENDER3DERROR_NOERR;
}

Render3DError Render3D::SetupPolygon(const POLY &thePoly)
{
	return RENDER3DERROR_NOERR;
}

Render3DError Render3D::SetupTexture(const POLY &thePoly, bool enableTexturing)
{
	return RENDER3DERROR_NOERR;
}

Render3DError Render3D::SetupViewport(const u32 viewportValue)
{
	return RENDER3DERROR_NOERR;
}

Render3DError Render3D::Reset()
{
	if (this->_framebufferColor != NULL)
	{
		memset(this->_framebufferColor, 0, this->_framebufferColorSizeBytes);
		this->FlushFramebuffer(gfx3d_colorRGBA6665, gfx3d_colorRGBA5551);
	}
	
	memset(this->clearImageColor16Buffer, 0, sizeof(this->clearImageColor16Buffer));
	memset(this->clearImageDepthBuffer, 0, sizeof(this->clearImageDepthBuffer));
	memset(this->clearImagePolyIDBuffer, 0, sizeof(this->clearImagePolyIDBuffer));
	memset(this->clearImageFogBuffer, 0, sizeof(this->clearImageFogBuffer));
	
	TexCache_Reset();
	
	return RENDER3DERROR_NOERR;
}

Render3DError Render3D::Render(const GFX3D &engine)
{
	Render3DError error = RENDER3DERROR_NOERR;
	
	error = this->BeginRender(engine);
	if (error != RENDER3DERROR_NOERR)
	{
		return error;
	}
	
	this->UpdateToonTable(engine.renderState.u16ToonTable);
	this->ClearFramebuffer(engine.renderState);
	
	this->RenderGeometry(engine.renderState, engine.polylist, &engine.indexlist);
	
	if (engine.renderState.enableEdgeMarking)
	{
		this->RenderEdgeMarking(engine.renderState.edgeMarkColorTable, engine.renderState.enableAntialiasing);
	}
	
	if (engine.renderState.enableFog)
	{
		this->RenderFog(engine.renderState.fogDensityTable, engine.renderState.fogColor, engine.renderState.fogOffset, engine.renderState.fogShift, engine.renderState.enableFogAlphaOnly);
	}

	this->EndRender(engine.frameCtr);
	
	return error;
}

Render3DError Render3D::RenderFinish()
{
	return RENDER3DERROR_NOERR;
}

Render3DError Render3D::VramReconfigureSignal()
{
	TexCache_Invalidate();	
	return RENDER3DERROR_NOERR;
}

#ifdef ENABLE_SSE2

Render3DError Render3D_SSE2::FlushFramebuffer(FragmentColor *__restrict dstRGBA6665, u16 *__restrict dstRGBA5551)
{
	static const __m128i zeroColor = _mm_set1_epi32(0);
	const size_t pixCount = this->_framebufferWidth * this->_framebufferHeight;
	const size_t ssePixCount = pixCount - (pixCount % 4);
	
	for (size_t i = 0; i < ssePixCount; i += 4)
	{
		// Copy the framebufferColor buffer
		__m128i color = _mm_load_si128((__m128i *)(this->_framebufferColor + i));
		_mm_store_si128((__m128i *)(dstRGBA6665 + i), color);
		
		// Convert to RGBA5551
		__m128i r = _mm_and_si128(color, _mm_set1_epi32(0x0000003E));	// Read from R
		r = _mm_srli_epi32(r, 1);										// Shift to R
		
		__m128i g = _mm_and_si128(color, _mm_set1_epi32(0x00003E00));	// Read from G
		g = _mm_srli_epi32(g, 4);										// Shift in G
		
		__m128i b = _mm_and_si128(color, _mm_set1_epi32(0x003E0000));	// Read from B
		b = _mm_srli_epi32(b, 7);										// Shift to B
		
		__m128i a = _mm_and_si128(color, _mm_set1_epi32(0xFF000000));	// Read from A
		a = _mm_cmpgt_epi32(a, _mm_set1_epi32(0x00000000));				// Determine A
		
		// From here on, we're going to do an SSE2 trick to pack 32-bit down to unsigned
		// 16-bit. Since SSE2 only has packssdw (signed 16-bit pack), then the alpha bit
		// may be undefined. Now if we were using SSE4.1's packusdw (unsigned 16-bit pack),
		// we  wouldn't have to go through this hassle. But not everyone has an SSE4.1-capable
		// CPU, so doing this the SSE2 way is more guaranteed to work an everyone's CPU.
		//
		// To use packssdw, we take a bit one position lower for the alpha bit, run
		// packssdw, then shift the bit back to its original position. Then we por the
		// alpha vector with the post-packed color vector to get the final color.
		
		a = _mm_and_si128(a, _mm_set1_epi32(0x00004000));				// Mask out the bit before A
		a = _mm_packs_epi32(a, zeroColor);								// Pack 32-bit down to 16-bit
		a = _mm_slli_epi16(a, 1);										// Shift the A bit back to where it needs to be
		
		// Assemble the RGB colors
		color = _mm_or_si128(r, g);
		color = _mm_or_si128(color, b);
		
		// Pack the 32-bit color into a signed 16-bit color, then por the alpha bit back in.
		color = _mm_packs_epi32(color, zeroColor);
		color = _mm_or_si128(color, a);
		
		_mm_storel_epi64((__m128i *)(dstRGBA5551 + i), color);
	}
	
	for (size_t i = ssePixCount; i < pixCount; i++)
	{
		dstRGBA6665[i] = this->_framebufferColor[i];
		dstRGBA5551[i] = R6G6B6TORGB15(this->_framebufferColor[i].r, this->_framebufferColor[i].g, this->_framebufferColor[i].b) | ((this->_framebufferColor[i].a == 0) ? 0x0000 : 0x8000);
	}
	
	return RENDER3DERROR_NOERR;
}

Render3DError Render3D_SSE2::ClearFramebuffer(const GFX3D_State &renderState)
{
	Render3DError error = RENDER3DERROR_NOERR;
	
	FragmentColor clearColor;
	clearColor.r =  renderState.clearColor & 0x1F;
	clearColor.g = (renderState.clearColor >> 5) & 0x1F;
	clearColor.b = (renderState.clearColor >> 10) & 0x1F;
	clearColor.a = (renderState.clearColor >> 16) & 0x1F;
	
	FragmentAttributes clearFragment;
	clearFragment.opaquePolyID = (renderState.clearColor >> 24) & 0x3F;
	//special value for uninitialized translucent polyid. without this, fires in spiderman2 dont display
	//I am not sure whether it is right, though. previously this was cleared to 0, as a guess,
	//but in spiderman2 some fires with polyid 0 try to render on top of the background
	clearFragment.translucentPolyID = kUnsetTranslucentPolyID;
	clearFragment.depth = renderState.clearDepth;
	clearFragment.stencil = 0;
	clearFragment.isTranslucentPoly = 0;
	clearFragment.isFogged = BIT15(renderState.clearColor);
	
	if (renderState.enableClearImage)
	{
		//the lion, the witch, and the wardrobe (thats book 1, suck it you new-school numberers)
		//uses the scroll registers in the main game engine
		const u16 *__restrict clearColorBuffer = (u16 *__restrict)MMU.texInfo.textureSlotAddr[2];
		const u16 *__restrict clearDepthBuffer = (u16 *__restrict)MMU.texInfo.textureSlotAddr[3];
		const u16 scrollBits = T1ReadWord(MMU.ARM9_REG, 0x356); //CLRIMAGE_OFFSET
		const u8 xScroll = scrollBits & 0xFF;
		const u8 yScroll = (scrollBits >> 8) & 0xFF;
				
		if (xScroll == 0 && yScroll == 0)
		{
			const __m128i opaquePolyID_vec128 = _mm_set1_epi8(clearFragment.opaquePolyID);
			
			for (size_t i = 0; i < GPU_FRAMEBUFFER_NATIVE_WIDTH * GPU_FRAMEBUFFER_NATIVE_HEIGHT; i += 16)
			{
				static const __m128i depthBitMask_vec128 = _mm_set1_epi16(0x7FFF);
				static const __m128i fogBufferBitMask_vec128 = _mm_set1_epi16(BIT(15));
				
				// Copy the colors to the color buffer. Since we can only copy 8 elements at once,
				// we need to load-store twice.
				_mm_store_si128( (__m128i *)(this->clearImageColor16Buffer + i + 8), _mm_load_si128((__m128i *)(clearColorBuffer + i + 8)) );
				_mm_store_si128( (__m128i *)(this->clearImageColor16Buffer + i), _mm_load_si128((__m128i *)(clearColorBuffer + i)) );
				
				// Write the depth values to the depth buffer.
				__m128i clearDepth_vec128 = _mm_load_si128((__m128i *)(clearDepthBuffer + i + 8));
				clearDepth_vec128 = _mm_and_si128(clearDepth_vec128, depthBitMask_vec128);
				
				__m128i depthValue_vec128 = _mm_set_epi32(dsDepthToD24_LUT[_mm_extract_epi16(clearDepth_vec128, 7)],
														  dsDepthToD24_LUT[_mm_extract_epi16(clearDepth_vec128, 6)],
														  dsDepthToD24_LUT[_mm_extract_epi16(clearDepth_vec128, 5)],
														  dsDepthToD24_LUT[_mm_extract_epi16(clearDepth_vec128, 4)]);
				_mm_store_si128((__m128i *)(this->clearImageDepthBuffer + i + 12), depthValue_vec128);
				
				depthValue_vec128 = _mm_set_epi32(dsDepthToD24_LUT[_mm_extract_epi16(clearDepth_vec128, 3)],
												  dsDepthToD24_LUT[_mm_extract_epi16(clearDepth_vec128, 2)],
												  dsDepthToD24_LUT[_mm_extract_epi16(clearDepth_vec128, 1)],
												  dsDepthToD24_LUT[_mm_extract_epi16(clearDepth_vec128, 0)]);
				_mm_store_si128((__m128i *)(this->clearImageDepthBuffer + i + 8), depthValue_vec128);
				
				clearDepth_vec128 = _mm_load_si128((__m128i *)(clearDepthBuffer + i));
				clearDepth_vec128 = _mm_and_si128(clearDepth_vec128, depthBitMask_vec128);
				
				depthValue_vec128 = _mm_set_epi32(dsDepthToD24_LUT[_mm_extract_epi16(clearDepth_vec128, 7)],
												  dsDepthToD24_LUT[_mm_extract_epi16(clearDepth_vec128, 6)],
												  dsDepthToD24_LUT[_mm_extract_epi16(clearDepth_vec128, 5)],
												  dsDepthToD24_LUT[_mm_extract_epi16(clearDepth_vec128, 4)]);
				_mm_store_si128((__m128i *)(this->clearImageDepthBuffer + i + 4), depthValue_vec128);
				
				depthValue_vec128 = _mm_set_epi32(dsDepthToD24_LUT[_mm_extract_epi16(clearDepth_vec128, 3)],
												  dsDepthToD24_LUT[_mm_extract_epi16(clearDepth_vec128, 2)],
												  dsDepthToD24_LUT[_mm_extract_epi16(clearDepth_vec128, 1)],
												  dsDepthToD24_LUT[_mm_extract_epi16(clearDepth_vec128, 0)]);
				_mm_store_si128((__m128i *)(this->clearImageDepthBuffer + i), depthValue_vec128);
				
				// Write the fog flags to the fog flag buffer.
				clearDepth_vec128 = _mm_load_si128((__m128i *)(clearDepthBuffer + i + 8)); // Read the upper values
				clearDepth_vec128 = _mm_and_si128(clearDepth_vec128, fogBufferBitMask_vec128);
				const __m128i clearDepthFogBit_vec128 = _mm_srli_epi16(clearDepth_vec128, 15); // Save the upper bits in another register
				
				clearDepth_vec128 = _mm_load_si128((__m128i *)(clearDepthBuffer + i)); // Read the lower values
				clearDepth_vec128 = _mm_and_si128(clearDepth_vec128, fogBufferBitMask_vec128);
				clearDepth_vec128 = _mm_srli_epi16(clearDepth_vec128, 15); // These are the lower bits
				
				_mm_store_si128((__m128i *)(this->clearImageFogBuffer + i), _mm_packus_epi16(clearDepth_vec128, clearDepthFogBit_vec128));
				
				// The one is easy. Just set the values in the polygon ID buffer.
				_mm_store_si128((__m128i *)(this->clearImagePolyIDBuffer + i), opaquePolyID_vec128);
			}
		}
		else
		{
			static const __m128i addrOffset = _mm_set_epi16(7, 6, 5, 4, 3, 2, 1, 0);
			static const __m128i addrRolloverMask = _mm_set1_epi16(0x00FF);
			const __m128i opaquePolyID_vec128 = _mm_set1_epi8(clearFragment.opaquePolyID);
			
			for (size_t dstIndex = 0, iy = 0; iy < GPU_FRAMEBUFFER_NATIVE_HEIGHT; iy++)
			{
				const size_t y = ((iy + yScroll) & 0xFF) << 8;
				__m128i y_vec128 = _mm_set1_epi16(y);
				
				for (size_t ix = 0; ix < GPU_FRAMEBUFFER_NATIVE_WIDTH; dstIndex += 8, ix += 8)
				{
					__m128i addr_vec128 = _mm_set1_epi16(ix + xScroll);
					addr_vec128 = _mm_add_epi16(addr_vec128, addrOffset);
					addr_vec128 = _mm_and_si128(addr_vec128, addrRolloverMask);
					addr_vec128 = _mm_or_si128(addr_vec128, y_vec128);
					
					this->clearImageColor16Buffer[dstIndex+7] = clearColorBuffer[_mm_extract_epi16(addr_vec128, 7)];
					this->clearImageColor16Buffer[dstIndex+6] = clearColorBuffer[_mm_extract_epi16(addr_vec128, 6)];
					this->clearImageColor16Buffer[dstIndex+5] = clearColorBuffer[_mm_extract_epi16(addr_vec128, 5)];
					this->clearImageColor16Buffer[dstIndex+4] = clearColorBuffer[_mm_extract_epi16(addr_vec128, 4)];
					this->clearImageColor16Buffer[dstIndex+3] = clearColorBuffer[_mm_extract_epi16(addr_vec128, 3)];
					this->clearImageColor16Buffer[dstIndex+2] = clearColorBuffer[_mm_extract_epi16(addr_vec128, 2)];
					this->clearImageColor16Buffer[dstIndex+1] = clearColorBuffer[_mm_extract_epi16(addr_vec128, 1)];
					this->clearImageColor16Buffer[dstIndex+0] = clearColorBuffer[_mm_extract_epi16(addr_vec128, 0)];
					
					this->clearImageDepthBuffer[dstIndex+7] = dsDepthToD24_LUT[clearDepthBuffer[_mm_extract_epi16(addr_vec128, 7)] & 0x7FFF];
					this->clearImageDepthBuffer[dstIndex+6] = dsDepthToD24_LUT[clearDepthBuffer[_mm_extract_epi16(addr_vec128, 6)] & 0x7FFF];
					this->clearImageDepthBuffer[dstIndex+5] = dsDepthToD24_LUT[clearDepthBuffer[_mm_extract_epi16(addr_vec128, 5)] & 0x7FFF];
					this->clearImageDepthBuffer[dstIndex+4] = dsDepthToD24_LUT[clearDepthBuffer[_mm_extract_epi16(addr_vec128, 4)] & 0x7FFF];
					this->clearImageDepthBuffer[dstIndex+3] = dsDepthToD24_LUT[clearDepthBuffer[_mm_extract_epi16(addr_vec128, 3)] & 0x7FFF];
					this->clearImageDepthBuffer[dstIndex+2] = dsDepthToD24_LUT[clearDepthBuffer[_mm_extract_epi16(addr_vec128, 2)] & 0x7FFF];
					this->clearImageDepthBuffer[dstIndex+1] = dsDepthToD24_LUT[clearDepthBuffer[_mm_extract_epi16(addr_vec128, 1)] & 0x7FFF];
					this->clearImageDepthBuffer[dstIndex+0] = dsDepthToD24_LUT[clearDepthBuffer[_mm_extract_epi16(addr_vec128, 0)] & 0x7FFF];
					
					this->clearImageFogBuffer[dstIndex+7] = BIT15( clearDepthBuffer[_mm_extract_epi16(addr_vec128, 7)] );
					this->clearImageFogBuffer[dstIndex+6] = BIT15( clearDepthBuffer[_mm_extract_epi16(addr_vec128, 6)] );
					this->clearImageFogBuffer[dstIndex+5] = BIT15( clearDepthBuffer[_mm_extract_epi16(addr_vec128, 5)] );
					this->clearImageFogBuffer[dstIndex+4] = BIT15( clearDepthBuffer[_mm_extract_epi16(addr_vec128, 4)] );
					this->clearImageFogBuffer[dstIndex+3] = BIT15( clearDepthBuffer[_mm_extract_epi16(addr_vec128, 3)] );
					this->clearImageFogBuffer[dstIndex+2] = BIT15( clearDepthBuffer[_mm_extract_epi16(addr_vec128, 2)] );
					this->clearImageFogBuffer[dstIndex+1] = BIT15( clearDepthBuffer[_mm_extract_epi16(addr_vec128, 1)] );
					this->clearImageFogBuffer[dstIndex+0] = BIT15( clearDepthBuffer[_mm_extract_epi16(addr_vec128, 0)] );
					
					_mm_storel_epi64((__m128i *)(this->clearImagePolyIDBuffer + dstIndex), opaquePolyID_vec128);
				}
			}
		}
		
		error = this->ClearUsingImage(this->clearImageColor16Buffer, this->clearImageDepthBuffer, this->clearImageFogBuffer, this->clearImagePolyIDBuffer);
		if (error != RENDER3DERROR_NOERR)
		{
			error = this->ClearUsingValues(clearColor, clearFragment);
		}
	}
	else
	{
		error = this->ClearUsingValues(clearColor, clearFragment);
	}
	
	return error;
}

#endif // ENABLE_SSE2
