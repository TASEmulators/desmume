/*
	Copyright (C) 2006-2007 shash
	Copyright (C) 2008-2016 DeSmuME team

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

#include "utils/bits.h"
#include "MMU.h"
#include "./filter/filter.h"
#include "./filter/xbrz.h"


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
	const bool didRenderBegin = CurrentRenderer->GetRenderNeedsFinish();
	CurrentRenderer->RenderFinish();
	gpu3D->NDS_3D_Close();
	gpu3D = &gpu3DNull;
	cur3DCore = GPU3D_NULL;
	BaseRenderer->SetRenderNeedsFinish(didRenderBegin);
	CurrentRenderer = BaseRenderer;
	
	Render3D *newRenderer = newRenderInterface->NDS_3D_Init();
	if (newRenderer == NULL)
	{
		return result;
	}
	
	newRenderer->RequestColorFormat(GPU->GetDisplayInfo().colorFormat);
	
	Render3DError error = newRenderer->SetFramebufferSize(GPU->GetCustomFramebufferWidth(), GPU->GetCustomFramebufferHeight());
	if (error != RENDER3DERROR_NOERR)
	{
		return result;
	}
	
	gpu3D = newRenderInterface;
	cur3DCore = newCore;
	newRenderer->SetRenderNeedsFinish( BaseRenderer->GetRenderNeedsFinish() );
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
		Render3D *oldRenderer = CurrentRenderer;
		CurrentRenderer = BaseRenderer;
		delete oldRenderer;
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
	size_t i = 0;
	
#ifdef ENABLE_SSE2
	const __m128i attrDepth_vec128				= _mm_set1_epi32(attr.depth);
	const __m128i attrOpaquePolyID_vec128		= _mm_set1_epi8(attr.opaquePolyID);
	const __m128i attrTranslucentPolyID_vec128	= _mm_set1_epi8(attr.translucentPolyID);
	const __m128i attrStencil_vec128			= _mm_set1_epi8(attr.stencil);
	const __m128i attrIsFogged_vec128			= _mm_set1_epi8(attr.isFogged);
	const __m128i attrIsTranslucentPoly_vec128	= _mm_set1_epi8(attr.isTranslucentPoly);
	
	const size_t sseCount = count - (count % 16);
	for (; i < sseCount; i += 16)
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
#endif
	
#ifdef ENABLE_SSE2
#pragma LOOPVECTORIZE_DISABLE
#endif
	for (; i < count; i++)
	{
		this->SetAtIndex(i, attr);
	}
}

Render3DTexture::Render3DTexture(u32 texAttributes, u32 palAttributes) : TextureStore(texAttributes, palAttributes)
{
	_isSamplingEnabled = true;
	_useDeposterize = false;
	_scalingFactor = 1;
	
	memset(&_deposterizeSrcSurface, 0, sizeof(_deposterizeSrcSurface));
	memset(&_deposterizeDstSurface, 0, sizeof(_deposterizeDstSurface));
	
	_deposterizeSrcSurface.Width = _deposterizeDstSurface.Width = _sizeS;
	_deposterizeSrcSurface.Height = _deposterizeDstSurface.Height = _sizeT;
	_deposterizeSrcSurface.Pitch = _deposterizeDstSurface.Pitch = 1;
}

template <size_t SCALEFACTOR>
void Render3DTexture::_Upscale(const u32 *__restrict src, u32 *__restrict dst)
{
	if ( (SCALEFACTOR != 2) && (SCALEFACTOR != 4) )
	{
		return;
	}
	
	if ( (this->_packFormat == TEXMODE_A3I5) || (this->_packFormat == TEXMODE_A5I3) )
	{
		xbrz::scale<SCALEFACTOR, xbrz::ColorFormatARGB>(src, dst, this->_sizeS, this->_sizeT);
	}
	else
	{
		xbrz::scale<SCALEFACTOR, xbrz::ColorFormatARGB_1bitAlpha>(src, dst, this->_sizeS, this->_sizeT);
	}
}

bool Render3DTexture::IsSamplingEnabled() const
{
	return this->_isSamplingEnabled;
}

void Render3DTexture::SetSamplingEnabled(bool isEnabled)
{
	this->_isSamplingEnabled = isEnabled;
}

bool Render3DTexture::IsUsingDeposterize() const
{
	return this->_useDeposterize;
}

void Render3DTexture::SetUseDeposterize(bool willDeposterize)
{
	this->_useDeposterize = willDeposterize;
}

size_t Render3DTexture::GetScalingFactor() const
{
	return this->_scalingFactor;
}

void Render3DTexture::SetScalingFactor(size_t scalingFactor)
{
	this->_scalingFactor = ( (scalingFactor == 2) || (scalingFactor == 4) ) ? scalingFactor : 1;
}

template void Render3DTexture::_Upscale<2>(const u32 *__restrict src, u32 *__restrict dst);
template void Render3DTexture::_Upscale<4>(const u32 *__restrict src, u32 *__restrict dst);

void* Render3D::operator new(size_t size)
{
	void *newPtr = malloc_alignedCacheLine(size);
	if (newPtr == NULL)
	{
		throw std::bad_alloc();
	}
	
	return newPtr;
}

void Render3D::operator delete(void *ptr)
{
	free_aligned(ptr);
}

Render3D::Render3D()
{
	_deviceInfo.renderID = RENDERID_NULL;
	_deviceInfo.renderName = "None";
	_deviceInfo.isTexturingSupported = false;
	_deviceInfo.isEdgeMarkSupported = false;
	_deviceInfo.isFogSupported = false;
	_deviceInfo.isTextureSmoothingSupported = false;
	_deviceInfo.maxAnisotropy = 1.0f;
	_deviceInfo.maxSamples = 0;
	
	_framebufferWidth = GPU_FRAMEBUFFER_NATIVE_WIDTH;
	_framebufferHeight = GPU_FRAMEBUFFER_NATIVE_HEIGHT;
	_framebufferColorSizeBytes = 0;
	_framebufferColor = NULL;
	
	_internalRenderingFormat = NDSColorFormat_BGR666_Rev;
	_outputFormat = NDSColorFormat_BGR666_Rev;
	_renderNeedsFinish = false;
	_renderNeedsFlushMain = false;
	_renderNeedsFlush16 = false;
	
	_textureScalingFactor = 1;
	_textureDeposterize = false;
	_textureSmooth = false;
	_textureUpscaleBuffer = NULL;
	
	memset(&_textureDeposterizeSrcSurface, 0, sizeof(_textureDeposterizeSrcSurface));
	memset(&_textureDeposterizeDstSurface, 0, sizeof(_textureDeposterizeDstSurface));
	
	_textureDeposterizeSrcSurface.Width = _textureDeposterizeDstSurface.Width = 1;
	_textureDeposterizeSrcSurface.Height = _textureDeposterizeDstSurface.Height = 1;
	_textureDeposterizeSrcSurface.Pitch = _textureDeposterizeDstSurface.Pitch = 1;
	
	Reset();
}

Render3D::~Render3D()
{
	if (this->_textureDeposterizeDstSurface.Surface != NULL)
	{
		free_aligned(this->_textureDeposterizeDstSurface.Surface);
		this->_textureDeposterizeDstSurface.Surface = NULL;
		this->_textureDeposterizeDstSurface.workingSurface[0] = NULL;
	}
}

const Render3DDeviceInfo& Render3D::GetDeviceInfo()
{
	return this->_deviceInfo;
}

RendererID Render3D::GetRenderID()
{
	return this->_deviceInfo.renderID;
}

std::string Render3D::GetName()
{
	return this->_deviceInfo.renderName;
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

bool Render3D::IsFramebufferNativeSize()
{
	return ( (this->_framebufferWidth == GPU_FRAMEBUFFER_NATIVE_WIDTH) && (this->_framebufferHeight == GPU_FRAMEBUFFER_NATIVE_HEIGHT) );
}

Render3DError Render3D::SetFramebufferSize(size_t w, size_t h)
{
	if (w < GPU_FRAMEBUFFER_NATIVE_WIDTH || h < GPU_FRAMEBUFFER_NATIVE_HEIGHT)
	{
		return RENDER3DERROR_NOERR;
	}
	
	this->_framebufferWidth = w;
	this->_framebufferHeight = h;
	this->_framebufferColorSizeBytes = w * h * sizeof(FragmentColor);
	this->_framebufferColor = GPU->GetEngineMain()->Get3DFramebufferMain(); // Just use the buffer that is already present on the main GPU engine
	
	return RENDER3DERROR_NOERR;
}

NDSColorFormat Render3D::RequestColorFormat(NDSColorFormat colorFormat)
{
	this->_outputFormat = (colorFormat == NDSColorFormat_BGR555_Rev) ? NDSColorFormat_BGR666_Rev : colorFormat;
	return this->_outputFormat;
}

NDSColorFormat Render3D::GetColorFormat() const
{
	return this->_outputFormat;
}

bool Render3D::GetRenderNeedsFinish() const
{
	return this->_renderNeedsFinish;
}

void Render3D::SetRenderNeedsFinish(const bool renderNeedsFinish)
{
	this->_renderNeedsFinish = renderNeedsFinish;
}

bool Render3D::GetRenderNeedsFlushMain() const
{
	return this->_renderNeedsFlushMain;
}

bool Render3D::GetRenderNeedsFlush16() const
{
	return this->_renderNeedsFlush16;
}

void Render3D::SetTextureProcessingProperties(size_t scalingFactor, bool willDeposterize, bool willSmooth)
{
	const bool isScaleValid = ( (scalingFactor == 2) || (scalingFactor == 4) );
	const size_t newScalingFactor = (isScaleValid) ? scalingFactor : 1;
	bool needTextureReload = false;
	
	if ( willDeposterize && !this->_textureDeposterize)
	{
		// 1024x1024 texels is the largest possible texture size.
		// We need two buffers, one for each deposterize stage.
		const size_t bufferSize = 1024 * 1024 * 2 * sizeof(u32);
		
		this->_textureDeposterizeDstSurface.Surface = (unsigned char *)malloc_alignedCacheLine(bufferSize);
		this->_textureDeposterizeDstSurface.workingSurface[0] = (unsigned char *)((u32 *)this->_textureDeposterizeDstSurface.Surface + (1024 * 1024));
		
		memset(this->_textureDeposterizeDstSurface.Surface, 0, bufferSize);
		
		this->_textureDeposterize = true;
		needTextureReload = true;
	}
	else if (!willDeposterize && this->_textureDeposterize)
	{
		free_aligned(this->_textureDeposterizeDstSurface.Surface);
		this->_textureDeposterizeDstSurface.Surface = NULL;
		this->_textureDeposterizeDstSurface.workingSurface[0] = NULL;
		
		this->_textureDeposterize = false;
		needTextureReload = true;
	}
	
	if (newScalingFactor != this->_textureScalingFactor)
	{
		u32 *oldTextureBuffer = this->_textureUpscaleBuffer;
		u32 *newTextureBuffer = (u32 *)malloc_alignedCacheLine( (1024 * newScalingFactor) * (1024 * newScalingFactor) * sizeof(u32) );
		this->_textureScalingFactor = newScalingFactor;
		this->_textureUpscaleBuffer = newTextureBuffer;
		free_aligned(oldTextureBuffer);
		
		needTextureReload = true;
	}
	
	this->_textureSmooth = willSmooth;
	
	if (needTextureReload)
	{
		texCache.ForceReloadAllTextures();
	}
}

Render3DTexture* Render3D::GetTextureByPolygonRenderIndex(size_t polyRenderIndex) const
{
	return this->_textureList[polyRenderIndex];
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

Render3DError Render3D::FlushFramebuffer(const FragmentColor *__restrict srcFramebuffer, FragmentColor *__restrict dstFramebufferMain, u16 *__restrict dstFramebuffer16)
{
	if ( (dstFramebufferMain == NULL) && (dstFramebuffer16 == NULL) )
	{
		return RENDER3DERROR_NOERR;
	}
	
	const size_t pixCount = this->_framebufferWidth * this->_framebufferHeight;
	
	if (dstFramebufferMain != NULL)
	{
		if ( (this->_internalRenderingFormat == NDSColorFormat_BGR888_Rev) && (this->_outputFormat == NDSColorFormat_BGR666_Rev) )
		{
			ColorspaceConvertBuffer8888To6665<false, false>((u32 *)srcFramebuffer, (u32 *)dstFramebufferMain, pixCount);
		}
		else if ( (this->_internalRenderingFormat == NDSColorFormat_BGR666_Rev) && (this->_outputFormat == NDSColorFormat_BGR888_Rev) )
		{
			ColorspaceConvertBuffer6665To8888<false, false>((u32 *)srcFramebuffer, (u32 *)dstFramebufferMain, pixCount);
		}
		else if ( ((this->_internalRenderingFormat == NDSColorFormat_BGR666_Rev) && (this->_outputFormat == NDSColorFormat_BGR666_Rev)) ||
		          ((this->_internalRenderingFormat == NDSColorFormat_BGR888_Rev) && (this->_outputFormat == NDSColorFormat_BGR888_Rev)) )
		{
			memcpy(dstFramebufferMain, srcFramebuffer, pixCount * sizeof(FragmentColor));
		}
		
		this->_renderNeedsFlushMain = false;
	}
	
	if (dstFramebuffer16 != NULL)
	{
		if (this->_outputFormat == NDSColorFormat_BGR666_Rev)
		{
			ColorspaceConvertBuffer6665To5551<false, false>((u32 *)srcFramebuffer, dstFramebuffer16, pixCount);
		}
		else if (this ->_outputFormat == NDSColorFormat_BGR888_Rev)
		{
			ColorspaceConvertBuffer8888To5551<false, false>((u32 *)srcFramebuffer, dstFramebuffer16, pixCount);
		}
		
		this->_renderNeedsFlush16 = false;
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
	
	const u32 clearColorSwapped = LE_TO_LOCAL_32(renderState.clearColor);
	FragmentColor clearColor6665;
	clearColor6665.color = COLOR555TO6665(clearColorSwapped & 0x7FFF, (clearColorSwapped >> 16) & 0x1F);
	
	FragmentAttributes clearFragment;
	clearFragment.opaquePolyID = (clearColorSwapped >> 24) & 0x3F;
	//special value for uninitialized translucent polyid. without this, fires in spiderman2 dont display
	//I am not sure whether it is right, though. previously this was cleared to 0, as a guess,
	//but in spiderman2 some fires with polyid 0 try to render on top of the background
	clearFragment.translucentPolyID = kUnsetTranslucentPolyID;
	clearFragment.depth = renderState.clearDepth;
	clearFragment.stencil = 0;
	clearFragment.isTranslucentPoly = 0;
	clearFragment.isFogged = BIT15(clearColorSwapped);
	
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
			for (size_t i = 0; i < GPU_FRAMEBUFFER_NATIVE_WIDTH * GPU_FRAMEBUFFER_NATIVE_HEIGHT; i++)
			{
				this->clearImageColor16Buffer[i] = clearColorBuffer[i];
				this->clearImageDepthBuffer[i] = DS_DEPTH15TO24(clearDepthBuffer[i]);
				this->clearImageFogBuffer[i] = BIT15(clearDepthBuffer[i]);
				this->clearImagePolyIDBuffer[i] = clearFragment.opaquePolyID;
			}
		}
		else
		{
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
					this->clearImageDepthBuffer[dstIndex] = DS_DEPTH15TO24(clearDepthBuffer[srcIndex]);
					
					this->clearImageFogBuffer[dstIndex] = BIT15(clearDepthBuffer[srcIndex]);
					this->clearImagePolyIDBuffer[dstIndex] = clearFragment.opaquePolyID;
				}
			}
		}
		
		error = this->ClearUsingImage(this->clearImageColor16Buffer, this->clearImageDepthBuffer, this->clearImageFogBuffer, this->clearImagePolyIDBuffer);
		if (error != RENDER3DERROR_NOERR)
		{
			error = this->ClearUsingValues(clearColor6665, clearFragment);
		}
	}
	else
	{
		error = this->ClearUsingValues(clearColor6665, clearFragment);
	}
	
	return error;
}

Render3DError Render3D::ClearUsingImage(const u16 *__restrict colorBuffer, const u32 *__restrict depthBuffer, const u8 *__restrict fogBuffer, const u8 *__restrict polyIDBuffer)
{
	return RENDER3DERROR_NOERR;
}

Render3DError Render3D::ClearUsingValues(const FragmentColor &clearColor6665, const FragmentAttributes &clearAttributes)
{
	return RENDER3DERROR_NOERR;
}

Render3DError Render3D::SetupTexture(const POLY &thePoly, size_t polyRenderIndex)
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
	}
	
	memset(this->clearImageColor16Buffer, 0, sizeof(this->clearImageColor16Buffer));
	memset(this->clearImageDepthBuffer, 0, sizeof(this->clearImageDepthBuffer));
	memset(this->clearImagePolyIDBuffer, 0, sizeof(this->clearImagePolyIDBuffer));
	memset(this->clearImageFogBuffer, 0, sizeof(this->clearImageFogBuffer));
	
	this->_renderNeedsFinish = false;
	this->_renderNeedsFlushMain = false;
	this->_renderNeedsFlush16 = false;
	
	texCache.Reset();
	
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

	this->EndRender(engine.render3DFrameCount);
	
	return error;
}

Render3DError Render3D::RenderFinish()
{
	return RENDER3DERROR_NOERR;
}

Render3DError Render3D::RenderFlush(bool willFlushBuffer32, bool willFlushBuffer16)
{
	return RENDER3DERROR_NOERR;
}

Render3DError Render3D::VramReconfigureSignal()
{
	texCache.Invalidate();
	return RENDER3DERROR_NOERR;
}

#ifdef ENABLE_SSE2

Render3DError Render3D_SSE2::ClearFramebuffer(const GFX3D_State &renderState)
{
	Render3DError error = RENDER3DERROR_NOERR;
	
	FragmentColor clearColor6665;
	clearColor6665.color = COLOR555TO6665(renderState.clearColor & 0x7FFF, (renderState.clearColor >> 16) & 0x1F);
	
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
		
		const __m128i opaquePolyID_vec128 = _mm_set1_epi8(clearFragment.opaquePolyID);
		const __m128i calcDepthConstants = _mm_set1_epi32(0x01FF0200);
		
		if (xScroll == 0 && yScroll == 0)
		{
			for (size_t i = 0; i < GPU_FRAMEBUFFER_NATIVE_WIDTH * GPU_FRAMEBUFFER_NATIVE_HEIGHT; i += 16)
			{
				// Copy the colors to the color buffer.
				_mm_store_si128( (__m128i *)(this->clearImageColor16Buffer + i + 0), _mm_load_si128((__m128i *)(clearColorBuffer + i + 0)) );
				_mm_store_si128( (__m128i *)(this->clearImageColor16Buffer + i + 8), _mm_load_si128((__m128i *)(clearColorBuffer + i + 8)) );
				
				// Write the depth values to the depth buffer using the following formula from GBATEK.
				// 15-bit to 24-bit depth formula from http://problemkaputt.de/gbatek.htm#ds3drearplane
				//    D24 = (D15 * 0x0200) + (((D15 + 1) >> 15) * 0x01FF);
				const __m128i clearDepthLo = _mm_load_si128((__m128i *)(clearDepthBuffer + i + 0));
				const __m128i clearDepthHi = _mm_load_si128((__m128i *)(clearDepthBuffer + i + 8));
				
				const __m128i clearDepthValueLo = _mm_and_si128(clearDepthLo, _mm_set1_epi16(0x7FFF));
				const __m128i clearDepthValueHi = _mm_and_si128(clearDepthHi, _mm_set1_epi16(0x7FFF));
				const __m128i highestDepthBitLo = _mm_srli_epi16( _mm_adds_epu16(clearDepthValueLo, _mm_set1_epi16(1)), 15);
				const __m128i highestDepthBitHi = _mm_srli_epi16( _mm_adds_epu16(clearDepthValueHi, _mm_set1_epi16(1)), 15);
				
				__m128i calcDepth0 = _mm_unpacklo_epi16(clearDepthValueLo, highestDepthBitLo);
				__m128i calcDepth1 = _mm_unpackhi_epi16(clearDepthValueLo, highestDepthBitLo);
				__m128i calcDepth2 = _mm_unpacklo_epi16(clearDepthValueHi, highestDepthBitHi);
				__m128i calcDepth3 = _mm_unpackhi_epi16(clearDepthValueHi, highestDepthBitHi);
				
				calcDepth0 = _mm_madd_epi16(calcDepth0, calcDepthConstants);
				calcDepth1 = _mm_madd_epi16(calcDepth1, calcDepthConstants);
				calcDepth2 = _mm_madd_epi16(calcDepth2, calcDepthConstants);
				calcDepth3 = _mm_madd_epi16(calcDepth3, calcDepthConstants);
				
				_mm_store_si128((__m128i *)(this->clearImageDepthBuffer + i +  0), calcDepth0);
				_mm_store_si128((__m128i *)(this->clearImageDepthBuffer + i +  4), calcDepth1);
				_mm_store_si128((__m128i *)(this->clearImageDepthBuffer + i +  8), calcDepth2);
				_mm_store_si128((__m128i *)(this->clearImageDepthBuffer + i + 12), calcDepth3);
				
				// Write the fog flags to the fog flag buffer.
				const __m128i clearFogLo = _mm_srli_epi16(clearDepthLo, 15);
				const __m128i clearFogHi = _mm_srli_epi16(clearDepthHi, 15);
				_mm_store_si128((__m128i *)(this->clearImageFogBuffer + i), _mm_packs_epi16(clearFogLo, clearFogHi));
				
				// The one is easy. Just set the values in the polygon ID buffer.
				_mm_store_si128((__m128i *)(this->clearImagePolyIDBuffer + i), opaquePolyID_vec128);
			}
		}
		else
		{
			const size_t shiftCount = xScroll & 0x07;
			
			for (size_t dstIndex = 0, iy = 0; iy < GPU_FRAMEBUFFER_NATIVE_HEIGHT; iy++)
			{
				const size_t y = ((iy + yScroll) & 0xFF) << 8;
				
				for (size_t ix = 0; ix < GPU_FRAMEBUFFER_NATIVE_WIDTH; dstIndex += 8, ix += 8)
				{
					const size_t x = (ix + xScroll) & 0xFF;
					
					__m128i clearColor;
					__m128i clearDepth_vec128;
					
					if (shiftCount == 0)
					{
						const size_t srcIndex = y | x;
						
						clearColor = _mm_load_si128((__m128i *)(clearColorBuffer + srcIndex));
						clearDepth_vec128 = _mm_load_si128((__m128i *)(clearDepthBuffer + srcIndex));
					}
					else
					{
						const size_t x1 = x & 0xF8;
						const size_t x0 = (x1 == 0) ? (GPU_FRAMEBUFFER_NATIVE_WIDTH - 8) : x1 - 8;
						const size_t srcIndex0 = y | x0;
						const size_t srcIndex1 = y | x1;
						
						const __m128i clearColor0 = _mm_load_si128((__m128i *)(clearColorBuffer + srcIndex0));
						const __m128i clearColor1 = _mm_load_si128((__m128i *)(clearColorBuffer + srcIndex1));
						const __m128i clearDepth0 = _mm_load_si128((__m128i *)(clearDepthBuffer + srcIndex0));
						const __m128i clearDepth1 = _mm_load_si128((__m128i *)(clearDepthBuffer + srcIndex1));
						
						switch (shiftCount)
						{
							case 1:
								clearColor        = _mm_alignr_epi8(clearColor1, clearColor0, 14);
								clearDepth_vec128 = _mm_alignr_epi8(clearDepth1, clearDepth0, 14);
								break;
								
							case 2:
								clearColor        = _mm_alignr_epi8(clearColor1, clearColor0, 12);
								clearDepth_vec128 = _mm_alignr_epi8(clearDepth1, clearDepth0, 12);
								break;
								
							case 3:
								clearColor        = _mm_alignr_epi8(clearColor1, clearColor0, 10);
								clearDepth_vec128 = _mm_alignr_epi8(clearDepth1, clearDepth0, 10);
								break;
								
							case 4:
								clearColor        = _mm_alignr_epi8(clearColor1, clearColor0, 8);
								clearDepth_vec128 = _mm_alignr_epi8(clearDepth1, clearDepth0, 8);
								break;
								
							case 5:
								clearColor        = _mm_alignr_epi8(clearColor1, clearColor0, 6);
								clearDepth_vec128 = _mm_alignr_epi8(clearDepth1, clearDepth0, 6);
								break;
								
							case 6:
								clearColor        = _mm_alignr_epi8(clearColor1, clearColor0, 4);
								clearDepth_vec128 = _mm_alignr_epi8(clearDepth1, clearDepth0, 4);
								break;
								
							case 7:
								clearColor        = _mm_alignr_epi8(clearColor1, clearColor0, 2);
								clearDepth_vec128 = _mm_alignr_epi8(clearDepth1, clearDepth0, 2);
								break;
								
							default:
								clearColor        = _mm_setzero_si128();
								clearDepth_vec128 = _mm_setzero_si128();
								break;
						}
					}
					
					const __m128i clearDepthValue = _mm_and_si128(clearDepth_vec128, _mm_set1_epi16(0x7FFF));
					const __m128i depthPlusOne = _mm_srli_epi16( _mm_adds_epu16(clearDepthValue, _mm_set1_epi16(1)), 15);
					const __m128i clearFog = _mm_srli_epi16(clearDepth_vec128, 15);
					
					__m128i calcDepth0 = _mm_unpacklo_epi16(clearDepthValue, depthPlusOne);
					__m128i calcDepth1 = _mm_unpackhi_epi16(clearDepthValue, depthPlusOne);
					calcDepth0 = _mm_madd_epi16(calcDepth0, calcDepthConstants);
					calcDepth1 = _mm_madd_epi16(calcDepth1, calcDepthConstants);
					
					_mm_store_si128((__m128i *)(this->clearImageColor16Buffer + dstIndex), clearColor);
					_mm_store_si128((__m128i *)(this->clearImageDepthBuffer + dstIndex + 0), calcDepth0);
					_mm_store_si128((__m128i *)(this->clearImageDepthBuffer + dstIndex + 4), calcDepth1);
					_mm_storel_epi64((__m128i *)(this->clearImageFogBuffer + dstIndex), _mm_packs_epi16(clearFog, _mm_setzero_si128()));
					_mm_storel_epi64((__m128i *)(this->clearImagePolyIDBuffer + dstIndex), opaquePolyID_vec128);
				}
			}
		}
		
		error = this->ClearUsingImage(this->clearImageColor16Buffer, this->clearImageDepthBuffer, this->clearImageFogBuffer, this->clearImagePolyIDBuffer);
		if (error != RENDER3DERROR_NOERR)
		{
			error = this->ClearUsingValues(clearColor6665, clearFragment);
		}
	}
	else
	{
		error = this->ClearUsingValues(clearColor6665, clearFragment);
	}
	
	return error;
}

#endif // ENABLE_SSE2
