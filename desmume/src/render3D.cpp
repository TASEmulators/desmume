/*
	Copyright (C) 2006-2007 shash
	Copyright (C) 2008-2021 DeSmuME team

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
#include "NDSSystem.h"
#include "./filter/filter.h"
#include "./filter/xbrz.h"


int cur3DCore = RENDERID_NULL;

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
		cur3DCore = RENDERID_NULL;
		CurrentRenderer = BaseRenderer;
	}
}

void Render3D_DeInit()
{
	gpu3D->NDS_3D_Close();
	delete BaseRenderer;
	BaseRenderer = NULL;
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
	polyFacing = (u8 *)malloc_alignedCacheLine(count * sizeof(u8));
}

FragmentAttributesBuffer::~FragmentAttributesBuffer()
{
	free_aligned(depth);
	free_aligned(opaquePolyID);
	free_aligned(translucentPolyID);
	free_aligned(stencil);
	free_aligned(isFogged);
	free_aligned(isTranslucentPoly);
	free_aligned(polyFacing);
}

void FragmentAttributesBuffer::SetAtIndex(const size_t index, const FragmentAttributes &attr)
{
	this->depth[index]				= attr.depth;
	this->opaquePolyID[index]		= attr.opaquePolyID;
	this->translucentPolyID[index]	= attr.translucentPolyID;
	this->stencil[index]			= attr.stencil;
	this->isFogged[index]			= attr.isFogged;
	this->isTranslucentPoly[index]	= attr.isTranslucentPoly;
	this->polyFacing[index]			= attr.polyFacing;
}

Render3DTexture::Render3DTexture(TEXIMAGE_PARAM texAttributes, u32 palAttributes) : TextureStore(texAttributes, palAttributes)
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
	_framebufferPixCount = _framebufferWidth * _framebufferHeight;
	_framebufferSIMDPixCount = 0;
	_framebufferColorSizeBytes = _framebufferWidth * _framebufferHeight * sizeof(FragmentColor);
	_framebufferColor = NULL;
	
	_internalRenderingFormat = NDSColorFormat_BGR666_Rev;
	_outputFormat = NDSColorFormat_BGR666_Rev;
	_renderNeedsFinish = false;
	_renderNeedsFlushMain = false;
	_renderNeedsFlush16 = false;
	_isPoweredOn = false;
	
	_textureUpscaleBuffer = NULL;
	
	_enableEdgeMark = true;
	_enableFog = true;
	_enableTextureSmoothing = false;
	
	_enableTextureSampling = true;
	_prevEnableTextureSampling = _enableTextureSampling;
	
	_enableTextureDeposterize = false;
	_prevEnableTextureDeposterize = _enableTextureDeposterize;
	
	_textureScalingFactor = 1;
	_prevTextureScalingFactor = _textureScalingFactor;
	
	memset(&_textureDeposterizeSrcSurface, 0, sizeof(_textureDeposterizeSrcSurface));
	memset(&_textureDeposterizeDstSurface, 0, sizeof(_textureDeposterizeDstSurface));
	
	_textureDeposterizeSrcSurface.Width = _textureDeposterizeDstSurface.Width = 1;
	_textureDeposterizeSrcSurface.Height = _textureDeposterizeDstSurface.Height = 1;
	_textureDeposterizeSrcSurface.Pitch = _textureDeposterizeDstSurface.Pitch = 1;
	
	for (size_t i = 0; i < POLYLIST_SIZE; i++)
	{
		_textureList[i] = NULL;
	}
	
	memset(this->clearImageColor16Buffer, 0, sizeof(this->clearImageColor16Buffer));
	memset(this->clearImageDepthBuffer, 0, sizeof(this->clearImageDepthBuffer));
	memset(this->clearImageFogBuffer, 0, sizeof(this->clearImageFogBuffer));
	
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
	this->_framebufferPixCount = w * h;
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

void Render3D::SetTextureProcessingProperties()
{
	bool needTextureReload = false;
	
	if (this->_enableTextureSampling && !this->_prevEnableTextureSampling)
	{
		needTextureReload = true;
	}
	
	if (this->_enableTextureDeposterize && !this->_prevEnableTextureDeposterize)
	{
		// 1024x1024 texels is the largest possible texture size.
		// We need two buffers, one for each deposterize stage.
		const size_t bufferSize = 1024 * 1024 * 2 * sizeof(u32);
		
		this->_textureDeposterizeDstSurface.Surface = (unsigned char *)malloc_alignedCacheLine(bufferSize);
		this->_textureDeposterizeDstSurface.workingSurface[0] = (unsigned char *)((u32 *)this->_textureDeposterizeDstSurface.Surface + (1024 * 1024));
		memset(this->_textureDeposterizeDstSurface.Surface, 0, bufferSize);
		
		needTextureReload = true;
	}
	else if (!this->_enableTextureDeposterize && this->_prevEnableTextureDeposterize)
	{
		free_aligned(this->_textureDeposterizeDstSurface.Surface);
		this->_textureDeposterizeDstSurface.Surface = NULL;
		this->_textureDeposterizeDstSurface.workingSurface[0] = NULL;
		
		needTextureReload = true;
	}
	
	if (this->_textureScalingFactor != this->_prevTextureScalingFactor)
	{
		u32 *oldTextureBuffer = this->_textureUpscaleBuffer;
		u32 *newTextureBuffer = (u32 *)malloc_alignedCacheLine( (1024 * this->_textureScalingFactor) * (1024 * this->_textureScalingFactor) * sizeof(u32) );
		this->_textureUpscaleBuffer = newTextureBuffer;
		free_aligned(oldTextureBuffer);
		
		needTextureReload = true;
	}
	
	if (needTextureReload)
	{
		texCache.ForceReloadAllTextures();
	}
}

Render3DTexture* Render3D::GetTextureByPolygonRenderIndex(size_t polyRenderIndex) const
{
	return this->_textureList[polyRenderIndex];
}

ClipperMode Render3D::GetPreferredPolygonClippingMode() const
{
	return ClipperMode_DetermineClipOnly;
}

const CPoly& Render3D::GetClippedPolyByIndex(size_t index) const
{
	return this->_clippedPolyList[index];
}

size_t Render3D::GetClippedPolyCount() const
{
	return this->_clippedPolyCount;
}

Render3DError Render3D::ApplyRenderingSettings(const GFX3D_State &renderState)
{
	this->_enableEdgeMark = (CommonSettings.GFX3D_EdgeMark) ? renderState.enableEdgeMarking : false;
	this->_enableFog = (CommonSettings.GFX3D_Fog) ? renderState.enableFog : false;
	this->_enableTextureSmoothing = CommonSettings.GFX3D_Renderer_TextureSmoothing;
	
	this->_prevEnableTextureSampling = this->_enableTextureSampling;
	this->_enableTextureSampling = (CommonSettings.GFX3D_Texture) ? renderState.enableTexturing : false;
	
	this->_prevEnableTextureDeposterize = this->_enableTextureDeposterize;
	this->_enableTextureDeposterize = CommonSettings.GFX3D_Renderer_TextureDeposterize;
	
	this->_prevTextureScalingFactor = this->_textureScalingFactor;
	size_t newScalingFactor = (size_t)CommonSettings.GFX3D_Renderer_TextureScalingFactor;
	
	const bool isScaleValid = ( (newScalingFactor == 2) || (newScalingFactor == 4) );
	if (!isScaleValid)
	{
		newScalingFactor = 1;
	}
	
	this->_textureScalingFactor = newScalingFactor;
	
	return RENDER3DERROR_NOERR;
}

Render3DError Render3D::BeginRender(const GFX3D &engine)
{
	return RENDER3DERROR_NOERR;
}

Render3DError Render3D::RenderGeometry()
{
	return RENDER3DERROR_NOERR;
}

Render3DError Render3D::PostprocessFramebuffer()
{
	return RENDER3DERROR_NOERR;
}

Render3DError Render3D::EndRender()
{
	return RENDER3DERROR_NOERR;
}

Render3DError Render3D::FlushFramebuffer(const FragmentColor *__restrict srcFramebuffer, FragmentColor *__restrict dstFramebufferMain, u16 *__restrict dstFramebuffer16)
{
	if ( (dstFramebufferMain == NULL) && (dstFramebuffer16 == NULL) )
	{
		return RENDER3DERROR_NOERR;
	}
	
	if (dstFramebufferMain != NULL)
	{
		if ( (this->_internalRenderingFormat == NDSColorFormat_BGR888_Rev) && (this->_outputFormat == NDSColorFormat_BGR666_Rev) )
		{
			ColorspaceConvertBuffer8888To6665<false, false>((u32 *)srcFramebuffer, (u32 *)dstFramebufferMain, this->_framebufferPixCount);
		}
		else if ( (this->_internalRenderingFormat == NDSColorFormat_BGR666_Rev) && (this->_outputFormat == NDSColorFormat_BGR888_Rev) )
		{
			ColorspaceConvertBuffer6665To8888<false, false>((u32 *)srcFramebuffer, (u32 *)dstFramebufferMain, this->_framebufferPixCount);
		}
		else if ( ((this->_internalRenderingFormat == NDSColorFormat_BGR666_Rev) && (this->_outputFormat == NDSColorFormat_BGR666_Rev)) ||
		          ((this->_internalRenderingFormat == NDSColorFormat_BGR888_Rev) && (this->_outputFormat == NDSColorFormat_BGR888_Rev)) )
		{
			memcpy(dstFramebufferMain, srcFramebuffer, this->_framebufferPixCount * sizeof(FragmentColor));
		}
		
		this->_renderNeedsFlushMain = false;
	}
	
	if (dstFramebuffer16 != NULL)
	{
		if (this->_outputFormat == NDSColorFormat_BGR666_Rev)
		{
			ColorspaceConvertBuffer6665To5551<false, false>((u32 *)srcFramebuffer, dstFramebuffer16, this->_framebufferPixCount);
		}
		else if (this ->_outputFormat == NDSColorFormat_BGR888_Rev)
		{
			ColorspaceConvertBuffer8888To5551<false, false>((u32 *)srcFramebuffer, dstFramebuffer16, this->_framebufferPixCount);
		}
		
		this->_renderNeedsFlush16 = false;
	}
	
	return RENDER3DERROR_NOERR;
}

void Render3D::_ClearImageBaseLoop(const u16 *__restrict inColor16, const u16 *__restrict inDepth16, u16 *__restrict outColor16, u32 *__restrict outDepth24, u8 *__restrict outFog)
{
	for (size_t i = 0; i < GPU_FRAMEBUFFER_NATIVE_WIDTH * GPU_FRAMEBUFFER_NATIVE_HEIGHT; i++)
	{
		outColor16[i] = LE_TO_LOCAL_16(inColor16[i]);
		outDepth24[i] = DS_DEPTH15TO24( LE_TO_LOCAL_16(inDepth16[i]) );
		outFog[i] = BIT15( LE_TO_LOCAL_16(inDepth16[i]) );
	}
}

template <bool ISCOLORBLANK, bool ISDEPTHBLANK>
void Render3D::_ClearImageScrolledLoop(const u8 xScroll, const u8 yScroll, const u16 *__restrict inColor16, const u16 *__restrict inDepth16,
									   u16 *__restrict outColor16, u32 *__restrict outDepth24, u8 *__restrict outFog)
{
	if (ISCOLORBLANK && ISDEPTHBLANK)
	{
		memset(outColor16, 0, GPU_FRAMEBUFFER_NATIVE_WIDTH * GPU_FRAMEBUFFER_NATIVE_HEIGHT * sizeof(u16));
		memset(outDepth24, 0, GPU_FRAMEBUFFER_NATIVE_WIDTH * GPU_FRAMEBUFFER_NATIVE_HEIGHT * sizeof(u32));
		memset(outFog, 0, GPU_FRAMEBUFFER_NATIVE_WIDTH * GPU_FRAMEBUFFER_NATIVE_HEIGHT * sizeof(u8));
	}
	else
	{
		if (ISCOLORBLANK)
		{
			// Hint to when the clear color image pointer is pointing to blank memory.
			// In this case, just do a simple zero fill for speed.
			//
			// Test cases:
			// - Sonic Chronicles: The Dark Brotherhood
			// - The Chronicles of Narnia: The Lion, the Witch and the Wardrobe
			memset(outColor16, 0, GPU_FRAMEBUFFER_NATIVE_WIDTH * GPU_FRAMEBUFFER_NATIVE_HEIGHT * sizeof(u16));
		}
		
		if (ISDEPTHBLANK)
		{
			memset(outDepth24, 0, GPU_FRAMEBUFFER_NATIVE_WIDTH * GPU_FRAMEBUFFER_NATIVE_HEIGHT * sizeof(u32));
			memset(outFog, 0, GPU_FRAMEBUFFER_NATIVE_WIDTH * GPU_FRAMEBUFFER_NATIVE_HEIGHT * sizeof(u8));
		}
		
		for (size_t dstIndex = 0, iy = 0; iy < GPU_FRAMEBUFFER_NATIVE_HEIGHT; iy++)
		{
			const size_t y = ((iy + yScroll) & 0xFF) << 8;
			
			for (size_t ix = 0; ix < GPU_FRAMEBUFFER_NATIVE_WIDTH; dstIndex++, ix++)
			{
				const size_t x = (ix + xScroll) & 0xFF;
				const size_t srcIndex = y | x;
				
				// Clear image color buffer in RGBA5551 format.
				//
				// Test cases:
				// - Harry Potter and the Order of Phoenix
				// - Blazer Drive
				if (!ISCOLORBLANK)
				{
					outColor16[dstIndex] = LE_TO_LOCAL_16(inColor16[srcIndex]);
				}
				
				// Clear image depth buffer, where the first 15 bits are converted to
				// 24-bit depth, and the remaining MSB is the fog flag.
				//
				// Test cases:
				// - Harry Potter and the Order of Phoenix
				// - Blazer Drive
				// - Sonic Chronicles: The Dark Brotherhood
				// - The Chronicles of Narnia: The Lion, the Witch and the Wardrobe
				if (!ISDEPTHBLANK)
				{
					outDepth24[dstIndex] = DS_DEPTH15TO24( LE_TO_LOCAL_16(inDepth16[srcIndex]) );
					outFog[dstIndex] = BIT15( LE_TO_LOCAL_16(inDepth16[srcIndex]) );
				}
			}
		}
	}
}

Render3DError Render3D::ClearFramebuffer(const GFX3D_State &renderState)
{
	Render3DError error = RENDER3DERROR_NOERR;
	
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
			this->_ClearImageBaseLoop(clearColorBuffer, clearDepthBuffer, this->clearImageColor16Buffer, this->clearImageDepthBuffer, this->clearImageFogBuffer);
		}
		else
		{
			const bool isClearColorBlank = (clearColorBuffer >= (u16 *)MMU.blank_memory);
			const bool isClearDepthBlank = (clearDepthBuffer >= (u16 *)MMU.blank_memory);
			
			if (!isClearColorBlank && !isClearDepthBlank)
			{
				this->_ClearImageScrolledLoop<false, false>(xScroll, yScroll, clearColorBuffer, clearDepthBuffer,
															this->clearImageColor16Buffer, this->clearImageDepthBuffer, this->clearImageFogBuffer);
			}
			else if (isClearColorBlank)
			{
				this->_ClearImageScrolledLoop< true, false>(xScroll, yScroll, clearColorBuffer, clearDepthBuffer,
															this->clearImageColor16Buffer, this->clearImageDepthBuffer, this->clearImageFogBuffer);
			}
			else if (isClearDepthBlank)
			{
				this->_ClearImageScrolledLoop<false,  true>(xScroll, yScroll, clearColorBuffer, clearDepthBuffer,
															this->clearImageColor16Buffer, this->clearImageDepthBuffer, this->clearImageFogBuffer);
			}
			else
			{
				this->_ClearImageScrolledLoop< true,  true>(xScroll, yScroll, clearColorBuffer, clearDepthBuffer,
															this->clearImageColor16Buffer, this->clearImageDepthBuffer, this->clearImageFogBuffer);
			}
		}
		
		error = this->ClearUsingImage(this->clearImageColor16Buffer, this->clearImageDepthBuffer, this->clearImageFogBuffer, this->_clearAttributes.opaquePolyID);
		if (error != RENDER3DERROR_NOERR)
		{
			error = this->ClearUsingValues(this->_clearColor6665, this->_clearAttributes);
		}
	}
	else
	{
		error = this->ClearUsingValues(this->_clearColor6665, this->_clearAttributes);
	}
	
	return error;
}

Render3DError Render3D::ClearUsingImage(const u16 *__restrict colorBuffer, const u32 *__restrict depthBuffer, const u8 *__restrict fogBuffer, const u8 opaquePolyID)
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
	
	this->_clearColor6665.color = 0;
	memset(&this->_clearAttributes, 0, sizeof(FragmentAttributes));
	
	this->_renderNeedsFinish = false;
	this->_renderNeedsFlushMain = false;
	this->_renderNeedsFlush16 = false;
	this->_isPoweredOn = false;
	
	texCache.Reset();
	
	return RENDER3DERROR_NOERR;
}

Render3DError Render3D::RenderPowerOff()
{
	if (!this->_isPoweredOn)
	{
		return RENDER3DERROR_NOERR;
	}
	
	this->_isPoweredOn = false;
	memset(GPU->GetEngineMain()->Get3DFramebufferMain(), 0, this->_framebufferColorSizeBytes);
	memset(GPU->GetEngineMain()->Get3DFramebuffer16(), 0, this->_framebufferPixCount * sizeof(u16));
	
	return RENDER3DERROR_NOERR;
}

Render3DError Render3D::Render(const GFX3D &engine)
{
	Render3DError error = RENDER3DERROR_NOERR;
	this->_isPoweredOn = true;
	
	const u32 clearColorSwapped = LE_TO_LOCAL_32(engine.renderState.clearColor);
	this->_clearColor6665.color = LE_TO_LOCAL_32( COLOR555TO6665(clearColorSwapped & 0x7FFF, (clearColorSwapped >> 16) & 0x1F) );
	
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
		this->EndRender();
		return error;
	}
	
	error = this->ClearFramebuffer(engine.renderState);
	if (error != RENDER3DERROR_NOERR)
	{
		this->EndRender();
		return error;
	}
	
	error = this->RenderGeometry();
	if (error != RENDER3DERROR_NOERR)
	{
		this->EndRender();
		return error;
	}
	
	error = this->PostprocessFramebuffer();
	if (error != RENDER3DERROR_NOERR)
	{
		this->EndRender();
		return error;
	}
	
	return this->EndRender();
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

template <size_t SIMDBYTES>
Render3D_SIMD<SIMDBYTES>::Render3D_SIMD()
{
	_framebufferSIMDPixCount = (SIMDBYTES > 0) ? _framebufferPixCount - (_framebufferPixCount % SIMDBYTES) : _framebufferPixCount;
}

template <size_t SIMDBYTES>
Render3DError Render3D_SIMD<SIMDBYTES>::SetFramebufferSize(size_t w, size_t h)
{
	Render3DError error = this->Render3D::SetFramebufferSize(w, h);
	if (error != RENDER3DERROR_NOERR)
	{
		return RENDER3DERROR_NOERR;
	}
	
	this->_framebufferSIMDPixCount = (SIMDBYTES > 0) ? this->_framebufferPixCount - (this->_framebufferPixCount % SIMDBYTES) : _framebufferPixCount;
	
	return error;
}

#if defined(ENABLE_AVX2)
void Render3D_AVX2::_ClearImageBaseLoop(const u16 *__restrict inColor16, const u16 *__restrict inDepth16, u16 *__restrict outColor16, u32 *__restrict outDepth24, u8 *__restrict outFog)
{
	const __m256i calcDepthConstants = _mm256_set1_epi32(0x01FF0200);
	
	for (size_t i = 0; i < GPU_FRAMEBUFFER_NATIVE_WIDTH * GPU_FRAMEBUFFER_NATIVE_HEIGHT; i+=sizeof(v256u16))
	{
		// Copy the colors to the color buffer.
		_mm256_store_si256( (__m256i *)(outColor16 + i) + 0, _mm256_load_si256((__m256i *)(inColor16 + i) + 0) );
		_mm256_store_si256( (__m256i *)(outColor16 + i) + 1, _mm256_load_si256((__m256i *)(inColor16 + i) + 1) );
		
		// Write the depth values to the depth buffer using the following formula from GBATEK.
		// 15-bit to 24-bit depth formula from http://problemkaputt.de/gbatek.htm#ds3drearplane
		//    D24 = (D15 * 0x0200) + (((D15 + 1) >> 15) * 0x01FF);
		//
		// For now, let's forget GBATEK (which could be wrong) and try using a simpified formula:
		//    D24 = (D15 * 0x0200) + 0x01FF;
		const __m256i clearDepthLo = _mm256_load_si256((__m256i *)(inDepth16 + i) + 0);
		const __m256i clearDepthHi = _mm256_load_si256((__m256i *)(inDepth16 + i) + 1);
		
		const __m256i clearDepthValueLo = _mm256_permute4x64_epi64( _mm256_and_si256(clearDepthLo, _mm256_set1_epi16(0x7FFF)), 0xD8 );
		const __m256i clearDepthValueHi = _mm256_permute4x64_epi64( _mm256_and_si256(clearDepthHi, _mm256_set1_epi16(0x7FFF)), 0xD8 );
		
		__m256i calcDepth0 = _mm256_unpacklo_epi16(clearDepthValueLo, _mm256_set1_epi16(1));
		__m256i calcDepth1 = _mm256_unpackhi_epi16(clearDepthValueLo, _mm256_set1_epi16(1));
		__m256i calcDepth2 = _mm256_unpacklo_epi16(clearDepthValueHi, _mm256_set1_epi16(1));
		__m256i calcDepth3 = _mm256_unpackhi_epi16(clearDepthValueHi, _mm256_set1_epi16(1));
		
		calcDepth0 = _mm256_madd_epi16(calcDepth0, calcDepthConstants);
		calcDepth1 = _mm256_madd_epi16(calcDepth1, calcDepthConstants);
		calcDepth2 = _mm256_madd_epi16(calcDepth2, calcDepthConstants);
		calcDepth3 = _mm256_madd_epi16(calcDepth3, calcDepthConstants);
		
		_mm256_store_si256((__m256i *)(outDepth24 + i) + 0, calcDepth0);
		_mm256_store_si256((__m256i *)(outDepth24 + i) + 1, calcDepth1);
		_mm256_store_si256((__m256i *)(outDepth24 + i) + 2, calcDepth2);
		_mm256_store_si256((__m256i *)(outDepth24 + i) + 3, calcDepth3);
		
		// Write the fog flags to the fog flag buffer.
		const __m256i clearFogLo = _mm256_srli_epi16(clearDepthLo, 15);
		const __m256i clearFogHi = _mm256_srli_epi16(clearDepthHi, 15);
		_mm256_store_si256( (__m256i *)(outFog + i), _mm256_permute4x64_epi64(_mm256_packus_epi16(clearFogLo, clearFogHi), 0xD8) );
	}
}
#elif defined(ENABLE_SSE2)
void Render3D_SSE2::_ClearImageBaseLoop(const u16 *__restrict inColor16, const u16 *__restrict inDepth16, u16 *__restrict outColor16, u32 *__restrict outDepth24, u8 *__restrict outFog)
{
	const __m128i calcDepthConstants = _mm_set1_epi32(0x01FF0200);
	
	for (size_t i = 0; i < GPU_FRAMEBUFFER_NATIVE_WIDTH * GPU_FRAMEBUFFER_NATIVE_HEIGHT; i+=sizeof(v128u16))
	{
		// Copy the colors to the color buffer.
		_mm_store_si128( (__m128i *)(outColor16 + i) + 0, _mm_load_si128((__m128i *)(inColor16 + i) + 0) );
		_mm_store_si128( (__m128i *)(outColor16 + i) + 1, _mm_load_si128((__m128i *)(inColor16 + i) + 1) );
		
		// Write the depth values to the depth buffer using the following formula from GBATEK.
		// 15-bit to 24-bit depth formula from http://problemkaputt.de/gbatek.htm#ds3drearplane
		//    D24 = (D15 * 0x0200) + (((D15 + 1) >> 15) * 0x01FF);
		//
		// For now, let's forget GBATEK (which could be wrong) and try using a simpified formula:
		//    D24 = (D15 * 0x0200) + 0x01FF;
		const __m128i clearDepthLo = _mm_load_si128((__m128i *)(inDepth16 + i) + 0);
		const __m128i clearDepthHi = _mm_load_si128((__m128i *)(inDepth16 + i) + 1);
		
		const __m128i clearDepthValueLo = _mm_and_si128(clearDepthLo, _mm_set1_epi16(0x7FFF));
		const __m128i clearDepthValueHi = _mm_and_si128(clearDepthHi, _mm_set1_epi16(0x7FFF));
		
		__m128i calcDepth0 = _mm_unpacklo_epi16(clearDepthValueLo, _mm_set1_epi16(1));
		__m128i calcDepth1 = _mm_unpackhi_epi16(clearDepthValueLo, _mm_set1_epi16(1));
		__m128i calcDepth2 = _mm_unpacklo_epi16(clearDepthValueHi, _mm_set1_epi16(1));
		__m128i calcDepth3 = _mm_unpackhi_epi16(clearDepthValueHi, _mm_set1_epi16(1));
		
		calcDepth0 = _mm_madd_epi16(calcDepth0, calcDepthConstants);
		calcDepth1 = _mm_madd_epi16(calcDepth1, calcDepthConstants);
		calcDepth2 = _mm_madd_epi16(calcDepth2, calcDepthConstants);
		calcDepth3 = _mm_madd_epi16(calcDepth3, calcDepthConstants);
		
		_mm_store_si128((__m128i *)(outDepth24 + i) + 0, calcDepth0);
		_mm_store_si128((__m128i *)(outDepth24 + i) + 1, calcDepth1);
		_mm_store_si128((__m128i *)(outDepth24 + i) + 2, calcDepth2);
		_mm_store_si128((__m128i *)(outDepth24 + i) + 3, calcDepth3);
		
		// Write the fog flags to the fog flag buffer.
		const __m128i clearFogLo = _mm_srli_epi16(clearDepthLo, 15);
		const __m128i clearFogHi = _mm_srli_epi16(clearDepthHi, 15);
		_mm_store_si128((__m128i *)(outFog + i), _mm_packs_epi16(clearFogLo, clearFogHi));
	}
}
#elif defined(ENABLE_ALTIVEC)
void Render3D_AltiVec::_ClearImageBaseLoop(const u16 *__restrict inColor16, const u16 *__restrict inDepth16, u16 *__restrict outColor16, u32 *__restrict outDepth24, u8 *__restrict outFog)
{
	const v128u16 calcDepthMul = ((v128u16){0x0200,0,0x0200,0,0x0200,0,0x0200,0});
	const v128u32 calcDepthAdd = ((v128u32){0x01FF,0x01FF,0x01FF,0x01FF});
	
	for (size_t i = 0; i < GPU_FRAMEBUFFER_NATIVE_WIDTH * GPU_FRAMEBUFFER_NATIVE_HEIGHT; i+=sizeof(v128u16))
	{
		// Copy the colors to the color buffer.
		v128u16 inColor16SwappedLo = vec_ld( 0, inColor16 + i);
		v128u16 inColor16SwappedHi = vec_ld(16, inColor16 + i);
		
		inColor16SwappedLo = vec_perm(inColor16SwappedLo, inColor16SwappedLo, ((v128u8){1,0, 3,2, 5,4, 7,6, 9,8, 11,10, 13,12, 15,14}));
		inColor16SwappedHi = vec_perm(inColor16SwappedHi, inColor16SwappedHi, ((v128u8){1,0, 3,2, 5,4, 7,6, 9,8, 11,10, 13,12, 15,14}));
		
		vec_st(inColor16SwappedLo,  0, outColor16 + i);
		vec_st(inColor16SwappedHi, 16, outColor16 + i);
		
		// Write the depth values to the depth buffer using the following formula from GBATEK.
		// 15-bit to 24-bit depth formula from http://problemkaputt.de/gbatek.htm#ds3drearplane
		//    D24 = (D15 * 0x0200) + (((D15 + 1) >> 15) * 0x01FF);
		//
		// For now, let's forget GBATEK (which could be wrong) and try using a simpified formula:
		//    D24 = (D15 * 0x0200) + 0x01FF;
		v128u16 clearDepthLo = vec_ld( 0, inDepth16 + i);
		v128u16 clearDepthHi = vec_ld(16, inDepth16 + i);
		
		clearDepthLo = vec_perm(clearDepthLo, clearDepthLo, ((v128u8){1,0, 3,2, 5,4, 7,6, 9,8, 11,10, 13,12, 15,14}));
		clearDepthHi = vec_perm(clearDepthHi, clearDepthHi, ((v128u8){1,0, 3,2, 5,4, 7,6, 9,8, 11,10, 13,12, 15,14}));
		
		const v128u16 clearDepthValueLo = vec_and(clearDepthLo, ((v128u16){0x7FFF,0x7FFF,0x7FFF,0x7FFF,0x7FFF,0x7FFF,0x7FFF,0x7FFF}));
		const v128u16 clearDepthValueHi = vec_and(clearDepthHi, ((v128u16){0x7FFF,0x7FFF,0x7FFF,0x7FFF,0x7FFF,0x7FFF,0x7FFF,0x7FFF}));
		
		const v128u16 calcDepth0 = vec_perm(((v128u8){0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0}), clearDepthValueLo, ((v128u8){0x10,0x11,0,0,  0x12,0x13,0,0,  0x14,0x15,0,0,  0x16,0x17,0,0}));
		const v128u16 calcDepth1 = vec_perm(((v128u8){0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0}), clearDepthValueLo, ((v128u8){0x18,0x19,0,0,  0x1A,0x1B,0,0,  0x1C,0x1D,0,0,  0x1E,0x1F,0,0}));
		const v128u16 calcDepth2 = vec_perm(((v128u8){0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0}), clearDepthValueHi, ((v128u8){0x10,0x11,0,0,  0x12,0x13,0,0,  0x14,0x15,0,0,  0x16,0x17,0,0}));
		const v128u16 calcDepth3 = vec_perm(((v128u8){0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0}), clearDepthValueHi, ((v128u8){0x18,0x19,0,0,  0x1A,0x1B,0,0,  0x1C,0x1D,0,0,  0x1E,0x1F,0,0}));
		
		vec_st( vec_msum(calcDepth0, calcDepthMul, calcDepthAdd),  0, outDepth24 + i);
		vec_st( vec_msum(calcDepth1, calcDepthMul, calcDepthAdd), 16, outDepth24 + i);
		vec_st( vec_msum(calcDepth2, calcDepthMul, calcDepthAdd), 32, outDepth24 + i);
		vec_st( vec_msum(calcDepth3, calcDepthMul, calcDepthAdd), 48, outDepth24 + i);
		
		// Write the fog flags to the fog flag buffer.
		const v128u16 clearFogLo = vec_sr(clearDepthLo, ((v128u16){15,15,15,15,15,15,15,15}));
		const v128u16 clearFogHi = vec_sr(clearDepthHi, ((v128u16){15,15,15,15,15,15,15,15}));
		vec_st( vec_pack(clearFogLo, clearFogHi), 0, outFog + i );
	}
}
#endif

template Render3D_SIMD<16>::Render3D_SIMD();
template Render3D_SIMD<32>::Render3D_SIMD();
