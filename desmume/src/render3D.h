/*
	Copyright (C) 2006-2007 shash
	Copyright (C) 2007-2019 DeSmuME team

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

#ifndef RENDER3D_H
#define RENDER3D_H

#include "types.h"
#include "gfx3d.h"
#include "texcache.h"
#include "./filter/filter.h"

#define kUnsetTranslucentPolyID 255
#define DEPTH_EQUALS_TEST_TOLERANCE 255

class Render3D;

typedef struct Render3DInterface
{
	const char *name;				// The name of the renderer.
	Render3D* (*NDS_3D_Init)();		// Called when the renderer is created.
	void (*NDS_3D_Close)();			// Called when the renderer is destroyed.
	
} GPU3DInterface;

extern int cur3DCore;

// gpu 3D core list, per port
extern GPU3DInterface *core3DList[];

// Default null plugin
extern GPU3DInterface gpu3DNull;

// Extern pointer
extern Render3D *BaseRenderer;
extern Render3D *CurrentRenderer;
extern GPU3DInterface *gpu3D;

Render3D* Render3DBaseCreate();
void Render3DBaseDestroy();

void Render3D_Init();
void Render3D_DeInit();

enum RendererID
{
	RENDERID_NULL				= 0,
	RENDERID_SOFTRASTERIZER		= 1,
	RENDERID_OPENGL_AUTO		= 1000,
	RENDERID_OPENGL_LEGACY		= 1001,
	RENDERID_OPENGL_3_2			= 1002,
	RENDERID_METAL				= 2000
};

enum Render3DErrorCode
{
	RENDER3DERROR_NOERR = 0
};

enum PolyFacing
{
	PolyFacing_Unwritten = 0,
	PolyFacing_Front     = 1,
	PolyFacing_Back      = 2
};

typedef int Render3DError;

struct FragmentAttributes
{
	u32 depth;
	u8 opaquePolyID;
	u8 translucentPolyID;
	u8 stencil;
	u8 isFogged;
	u8 isTranslucentPoly;
	u8 polyFacing;
};

struct FragmentAttributesBuffer
{
	size_t count;
	u32 *depth;
	u8 *opaquePolyID;
	u8 *translucentPolyID;
	u8 *stencil;
	u8 *isFogged;
	u8 *isTranslucentPoly;
	u8 *polyFacing;
	
	FragmentAttributesBuffer(size_t newCount);
	~FragmentAttributesBuffer();
	
	void SetAtIndex(const size_t index, const FragmentAttributes &attr);
};

struct Render3DDeviceInfo
{
	RendererID renderID;
	std::string renderName;
	
	bool isTexturingSupported;
	bool isEdgeMarkSupported;
	bool isFogSupported;
	bool isTextureSmoothingSupported;
	
	float maxAnisotropy;
	u8 maxSamples;
};

class Render3DTexture : public TextureStore
{
protected:
	bool _isSamplingEnabled;
	bool _useDeposterize;
	size_t _scalingFactor;
	SSurface _deposterizeSrcSurface;
	SSurface _deposterizeDstSurface;
	
	template<size_t SCALEFACTOR> void _Upscale(const u32 *__restrict src, u32 *__restrict dst);
	
public:
	Render3DTexture(TEXIMAGE_PARAM texAttributes, u32 palAttributes);
	
	bool IsSamplingEnabled() const;
	void SetSamplingEnabled(bool isEnabled);
		
	bool IsUsingDeposterize() const;
	void SetUseDeposterize(bool willDeposterize);
	
	size_t GetScalingFactor() const;
	void SetScalingFactor(size_t scalingFactor);
};

class Render3D
{
protected:
	Render3DDeviceInfo _deviceInfo;
	
	size_t _framebufferWidth;
	size_t _framebufferHeight;
	size_t _framebufferPixCount;
	size_t _framebufferSIMDPixCount;
	size_t _framebufferColorSizeBytes;
	FragmentColor *_framebufferColor;
	
	FragmentColor _clearColor6665;
	FragmentAttributes _clearAttributes;
	
	NDSColorFormat _internalRenderingFormat;
	NDSColorFormat _outputFormat;
	bool _renderNeedsFinish;
	bool _renderNeedsFlushMain;
	bool _renderNeedsFlush16;
	bool _isPoweredOn;
	
	bool _enableEdgeMark;
	bool _enableFog;
	bool _enableTextureSampling;
	bool _enableTextureDeposterize;
	bool _enableTextureSmoothing;
	size_t _textureScalingFactor;
	
	bool _prevEnableTextureSampling;
	bool _prevEnableTextureDeposterize;
	size_t _prevTextureScalingFactor;
	
	SSurface _textureDeposterizeSrcSurface;
	SSurface _textureDeposterizeDstSurface;
	
	u32 *_textureUpscaleBuffer;
	Render3DTexture *_textureList[POLYLIST_SIZE];
	
	size_t _clippedPolyCount;
	size_t _clippedPolyOpaqueCount;
	CPoly *_clippedPolyList;
	
	CACHE_ALIGN u16 clearImageColor16Buffer[GPU_FRAMEBUFFER_NATIVE_WIDTH * GPU_FRAMEBUFFER_NATIVE_HEIGHT];
	CACHE_ALIGN u32 clearImageDepthBuffer[GPU_FRAMEBUFFER_NATIVE_WIDTH * GPU_FRAMEBUFFER_NATIVE_HEIGHT];
	CACHE_ALIGN u8 clearImageFogBuffer[GPU_FRAMEBUFFER_NATIVE_WIDTH * GPU_FRAMEBUFFER_NATIVE_HEIGHT];
	
	virtual void _ClearImageBaseLoop(const u16 *__restrict inColor16, const u16 *__restrict inDepth16, u16 *__restrict outColor16, u32 *__restrict outDepth24, u8 *__restrict outFog);
	template<bool ISCOLORBLANK, bool ISDEPTHBLANK> void _ClearImageScrolledLoop(const u8 xScroll, const u8 yScroll, const u16 *__restrict inColor16, const u16 *__restrict inDepth16,
																				u16 *__restrict outColor16, u32 *__restrict outDepth24, u8 *__restrict outFog);
	
	
	virtual Render3DError BeginRender(const GFX3D &engine);
	virtual Render3DError RenderGeometry();
	virtual Render3DError PostprocessFramebuffer();
	virtual Render3DError EndRender();
	virtual Render3DError FlushFramebuffer(const FragmentColor *__restrict srcFramebuffer, FragmentColor *__restrict dstFramebufferMain, u16 *__restrict dstFramebuffer16);
	
	virtual Render3DError ClearUsingImage(const u16 *__restrict colorBuffer, const u32 *__restrict depthBuffer, const u8 *__restrict fogBuffer, const u8 opaquePolyID);
	virtual Render3DError ClearUsingValues(const FragmentColor &clearColor6665, const FragmentAttributes &clearAttributes);
	
	virtual Render3DError SetupTexture(const POLY &thePoly, size_t polyRenderIndex);
	virtual Render3DError SetupViewport(const u32 viewportValue);
	
public:
	static void* operator new(size_t size);
	static void operator delete(void *p);
	Render3D();
	~Render3D();
	
	const Render3DDeviceInfo& GetDeviceInfo();
	RendererID GetRenderID();
	std::string GetName();
	
	size_t GetFramebufferWidth();
	size_t GetFramebufferHeight();
	bool IsFramebufferNativeSize();
	
	virtual Render3DError ClearFramebuffer(const GFX3D_State &renderState);
	
	virtual Render3DError ApplyRenderingSettings(const GFX3D_State &renderState);
	
	virtual Render3DError Reset();						// Called when the emulator resets.
	
	virtual Render3DError RenderPowerOff();				// Called when the renderer needs to handle a power-off condition by clearing its framebuffers.
	
	virtual Render3DError Render(const GFX3D &engine);	// Called when the renderer should do its job and render the current display lists.
	
	virtual Render3DError RenderFinish();				// Called whenever 3D rendering needs to finish. This function should block the calling thread
														// and only release the block when 3D rendering is finished. (Before reading the 3D layer, be
														// sure to always call this function.)
	
	virtual Render3DError RenderFlush(bool willFlushBuffer32, bool willFlushBuffer16);	// Called whenever the emulator needs the flushed results of the 3D renderer. Before calling this,
																						// the 3D renderer must be finished using RenderFinish() or confirmed already finished using
																						// GetRenderNeedsFinish().
	
	virtual Render3DError VramReconfigureSignal();		// Called when the emulator reconfigures its VRAM. You may need to invalidate your texture cache.
	
	virtual Render3DError SetFramebufferSize(size_t w, size_t h);	// Called whenever the output framebuffer size changes.
	
	virtual NDSColorFormat RequestColorFormat(NDSColorFormat colorFormat);	// Called whenever the output framebuffer color format changes. The framebuffer
																			// output by the 3D renderer is expected to match the requested format. If the
																			// internal color format of the 3D renderer doesn't natively match the requested
																			// format, then a colorspace conversion will be required in order to match. The
																			// only exception to this rule is if the requested output format is RGBA5551. In
																			// this particular case, the 3D renderer is expected to output a framebuffer in
																			// RGBA6665 color format. Again, if the internal color format does not match this,
																			// then a colorspace conversion will be required for RGBA6665.
	
	virtual NDSColorFormat GetColorFormat() const;							// The output color format of the 3D renderer.
	
	virtual FragmentColor* GetFramebuffer();
	
	bool GetRenderNeedsFinish() const;
	void SetRenderNeedsFinish(const bool renderNeedsFinish);
	
	bool GetRenderNeedsFlushMain() const;
	bool GetRenderNeedsFlush16() const;
	
	void SetTextureProcessingProperties();
	Render3DTexture* GetTextureByPolygonRenderIndex(size_t polyRenderIndex) const;
	
	virtual ClipperMode GetPreferredPolygonClippingMode() const;
	const CPoly& GetClippedPolyByIndex(size_t index) const;
	size_t GetClippedPolyCount() const;
};

template <size_t SIMDBYTES>
class Render3D_SIMD : public Render3D
{
public:
	Render3D_SIMD();
	
	virtual Render3DError SetFramebufferSize(size_t w, size_t h);
};

#if defined(ENABLE_AVX2)

class Render3D_AVX2 : public Render3D_SIMD<32>
{
public:
	virtual void _ClearImageBaseLoop(const u16 *__restrict inColor16, const u16 *__restrict inDepth16, u16 *__restrict outColor16, u32 *__restrict outDepth24, u8 *__restrict outFog);
};

#elif defined(ENABLE_SSE2)

class Render3D_SSE2 : public Render3D_SIMD<16>
{
public:
	virtual void _ClearImageBaseLoop(const u16 *__restrict inColor16, const u16 *__restrict inDepth16, u16 *__restrict outColor16, u32 *__restrict outDepth24, u8 *__restrict outFog);
};

#elif defined(ENABLE_ALTIVEC)

class Render3D_AltiVec : public Render3D_SIMD<16>
{
public:
	virtual void _ClearImageBaseLoop(const u16 *__restrict inColor16, const u16 *__restrict inDepth16, u16 *__restrict outColor16, u32 *__restrict outDepth24, u8 *__restrict outFog);
};

#endif

#endif // RENDER3D_H
