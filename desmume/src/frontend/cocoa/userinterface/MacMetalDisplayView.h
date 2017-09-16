/*
	Copyright (C) 2017 DeSmuME team

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

#ifndef _MAC_METALDISPLAYVIEW_H
#define _MAC_METALDISPLAYVIEW_H

#import <Cocoa/Cocoa.h>
#import <Metal/Metal.h>
#include <libkern/OSAtomic.h>

#import "DisplayViewCALayer.h"
#import "../cocoa_GPU.h"
#include "../ClientDisplayView.h"

#ifdef BOOL
#undef BOOL
#endif

class MacMetalFetchObject;
class MacMetalDisplayView;

struct DisplayViewShaderProperties
{
	float width;
	float height;
	float rotation;
	float viewScale;
	uint32_t lowerHUDMipMapLevel;
};
typedef DisplayViewShaderProperties DisplayViewShaderProperties;

@interface MetalDisplayViewSharedData : MacClientSharedObject
{
	id<MTLDevice> device;
	id<MTLCommandQueue> commandQueue;
	id<MTLLibrary> defaultLibrary;
	
	id<MTLComputePipelineState> _fetch555Pipeline;
	id<MTLComputePipelineState> _fetch555ConvertOnlyPipeline;
	id<MTLComputePipelineState> _fetch666Pipeline;
	id<MTLComputePipelineState> _fetch888Pipeline;
	id<MTLComputePipelineState> deposterizePipeline;
	id<MTLRenderPipelineState> hudPipeline;
	
	id<MTLBlitCommandEncoder> _fetchEncoder;
	
	id<MTLSamplerState> samplerHUDBox;
	id<MTLSamplerState> samplerHUDText;
	
	id<MTLBuffer> hudIndexBuffer;
	id<MTLBuffer> _bufDisplayFetchNative[2][2];
	id<MTLBuffer> _bufDisplayFetchCustom[2][2];
	
	id<MTLBuffer> _bufMasterBrightMode[2];
	id<MTLBuffer> _bufMasterBrightIntensity[2];
	
	id<MTLTexture> texDisplayFetch16NativeMain;
	id<MTLTexture> texDisplayFetch16NativeTouch;
	id<MTLTexture> texDisplayFetch32NativeMain;
	id<MTLTexture> texDisplayFetch32NativeTouch;
	id<MTLTexture> texDisplayFetch16CustomMain;
	id<MTLTexture> texDisplayFetch16CustomTouch;
	id<MTLTexture> texDisplayFetch32CustomMain;
	id<MTLTexture> texDisplayFetch32CustomTouch;
	
	id<MTLTexture> texDisplayPostprocessNativeMain;
	id<MTLTexture> texDisplayPostprocessCustomMain;
	id<MTLTexture> texDisplayPostprocessNativeTouch;
	id<MTLTexture> texDisplayPostprocessCustomTouch;
	
	id<MTLTexture> texDisplaySrcTargetMain;
	id<MTLTexture> texDisplaySrcTargetTouch;
	
	id<MTLTexture> texLQ2xLUT;
	id<MTLTexture> texHQ2xLUT;
	id<MTLTexture> texHQ3xLUT;
	id<MTLTexture> texHQ4xLUT;
	id<MTLTexture> texCurrentHQnxLUT;
	
	MTLSize fetchThreadsPerGroup;
	MTLSize fetchThreadGroupsPerGridNative;
	MTLSize fetchThreadGroupsPerGridCustom;
	MTLSize deposterizeThreadsPerGroup;
	MTLSize deposterizeThreadGroupsPerGrid;
	
	size_t displayFetchNativeBufferSize;
	size_t displayFetchCustomBufferSize;
	
	pthread_mutex_t _mutexFetch;
}

@property (readonly, nonatomic) id<MTLDevice> device;
@property (readonly, nonatomic) id<MTLCommandQueue> commandQueue;
@property (readonly, nonatomic) id<MTLLibrary> defaultLibrary;

@property (readonly, nonatomic) id<MTLComputePipelineState> deposterizePipeline;
@property (readonly, nonatomic) id<MTLRenderPipelineState> hudPipeline;
@property (readonly, nonatomic) id<MTLSamplerState> samplerHUDBox;
@property (readonly, nonatomic) id<MTLSamplerState> samplerHUDText;

@property (readonly, nonatomic) id<MTLBuffer> hudIndexBuffer;

@property (readonly, nonatomic) id<MTLTexture> texDisplayFetch16NativeMain;
@property (readonly, nonatomic) id<MTLTexture> texDisplayFetch16NativeTouch;
@property (readonly, nonatomic) id<MTLTexture> texDisplayFetch32NativeMain;
@property (readonly, nonatomic) id<MTLTexture> texDisplayFetch32NativeTouch;
@property (retain) id<MTLTexture> texDisplayFetch16CustomMain;
@property (retain) id<MTLTexture> texDisplayFetch16CustomTouch;
@property (retain) id<MTLTexture> texDisplayFetch32CustomMain;
@property (retain) id<MTLTexture> texDisplayFetch32CustomTouch;

@property (retain) id<MTLTexture> texDisplayPostprocessNativeMain;
@property (retain) id<MTLTexture> texDisplayPostprocessCustomMain;
@property (retain) id<MTLTexture> texDisplayPostprocessNativeTouch;
@property (retain) id<MTLTexture> texDisplayPostprocessCustomTouch;

@property (retain) id<MTLTexture> texDisplaySrcTargetMain;
@property (retain) id<MTLTexture> texDisplaySrcTargetTouch;

@property (readonly, nonatomic) id<MTLTexture> texLQ2xLUT;
@property (readonly, nonatomic) id<MTLTexture> texHQ2xLUT;
@property (readonly, nonatomic) id<MTLTexture> texHQ3xLUT;
@property (readonly, nonatomic) id<MTLTexture> texHQ4xLUT;
@property (retain) id<MTLTexture> texCurrentHQnxLUT;

@property (assign) size_t displayFetchNativeBufferSize;
@property (assign) size_t displayFetchCustomBufferSize;

@property (readonly, nonatomic) MTLSize fetchThreadsPerGroup;
@property (readonly, nonatomic) MTLSize fetchThreadGroupsPerGridNative;
@property (assign) MTLSize fetchThreadGroupsPerGridCustom;
@property (readonly, nonatomic) MTLSize deposterizeThreadsPerGroup;
@property (readonly, nonatomic) MTLSize deposterizeThreadGroupsPerGrid;

- (void) setFetchBuffersWithDisplayInfo:(const NDSDisplayInfo &)dispInfo;
- (void) fetchFromBufferIndex:(const u8)index;
- (void) fetchNativeDisplayByID:(const NDSDisplayID)displayID bufferIndex:(const u8)bufferIndex;
- (void) fetchCustomDisplayByID:(const NDSDisplayID)displayID bufferIndex:(const u8)bufferIndex;

@end

@interface DisplayViewMetalLayer : CAMetalLayer <DisplayViewCALayer>
{
	MacMetalDisplayView *_cdv;
	MetalDisplayViewSharedData *sharedData;
	
	MTLRenderPassDescriptor *_outputRenderPassDesc;
	MTLRenderPassColorAttachmentDescriptor *colorAttachment0Desc;
	id<MTLComputePipelineState> pixelScalePipeline;
	id<MTLRenderPipelineState> displayOutputPipeline;
	
	id<MTLBuffer> _cdvPropertiesBuffer;
	id<MTLBuffer> _displayVtxPositionBuffer;
	id<MTLBuffer> _displayTexCoordBuffer;
	id<MTLBuffer> _hudVtxPositionBuffer;
	id<MTLBuffer> _hudVtxColorBuffer;
	id<MTLBuffer> _hudTexCoordBuffer;
	id<MTLBuffer> bufCPUFilterSrcMain;
	id<MTLBuffer> bufCPUFilterSrcTouch;
	id<MTLBuffer> bufCPUFilterDstMain;
	id<MTLBuffer> bufCPUFilterDstTouch;
	
	id<MTLTexture> _texDisplaySrcDeposterize[2][2];
	id<MTLTexture> texDisplayPixelScaleMain;
	id<MTLTexture> texDisplayPixelScaleTouch;
	id<MTLTexture> _texDisplayOutput[2];
	id<MTLTexture> texHUDCharMap;
	
	MTLSize _pixelScalerThreadsPerGroup;
	MTLSize _pixelScalerThreadGroupsPerGrid;
	
	BOOL needsViewportUpdate;
	BOOL needsRotationScaleUpdate;
	BOOL needsScreenVerticesUpdate;
	BOOL needsHUDVerticesUpdate;
	
	pthread_mutex_t _mutexDisplayTextureUpdate;
	pthread_mutex_t _mutexBufferUpdate;
	bool _needEncodeViewport;
	MTLViewport _newViewport;
	bool _willDrawDisplays;
	bool _willDrawHUD;
	bool _willDrawHUDInput;
	size_t _hudStringLength;
	size_t _hudTouchLineLength;
}

@property (assign, nonatomic) MetalDisplayViewSharedData *sharedData;
@property (readonly, nonatomic) MTLRenderPassColorAttachmentDescriptor *colorAttachment0Desc;
@property (retain) id<MTLComputePipelineState> pixelScalePipeline;
@property (retain) id<MTLRenderPipelineState> displayOutputPipeline;
@property (retain) id<MTLBuffer> bufCPUFilterSrcMain;
@property (retain) id<MTLBuffer> bufCPUFilterSrcTouch;
@property (retain) id<MTLBuffer> bufCPUFilterDstMain;
@property (retain) id<MTLBuffer> bufCPUFilterDstTouch;
@property (retain) id<MTLTexture> texDisplayPixelScaleMain;
@property (retain) id<MTLTexture> texDisplayPixelScaleTouch;
@property (retain) id<MTLTexture> texHUDCharMap;
@property (assign) BOOL needsViewportUpdate;
@property (assign) BOOL needsRotationScaleUpdate;
@property (assign) BOOL needsScreenVerticesUpdate;
@property (assign) BOOL needsHUDVerticesUpdate;
@property (assign, nonatomic) VideoFilterTypeID pixelScaler;
@property (assign, nonatomic) OutputFilterTypeID outputFilter;

- (id<MTLCommandBuffer>) newCommandBuffer;
- (void) setupLayer;
- (void) resizeCPUPixelScalerUsingFilterID:(const VideoFilterTypeID)filterID;
- (void) copyHUDFontUsingFace:(const FT_Face &)fontFace size:(const size_t)glyphSize tileSize:(const size_t)glyphTileSize info:(GlyphInfo *)glyphInfo;
- (void) processDisplays;
- (void) updateRenderBuffers;
- (void) renderAndFlushDrawable;

@end

#pragma mark -

class MacMetalFetchObject : public GPUClientFetchObject
{
protected:
	bool _useDirectToCPUFilterPipeline;
	uint32_t *_srcNativeCloneMaster;
	uint32_t *_srcNativeClone[2][2];
	pthread_rwlock_t _srcCloneRWLock[2][2];
	
	virtual void _FetchNativeDisplayByID(const NDSDisplayID displayID, const u8 bufferIndex);
	virtual void _FetchCustomDisplayByID(const NDSDisplayID displayID, const u8 bufferIndex);
	
public:
	MacMetalFetchObject();
	virtual ~MacMetalFetchObject();
	
	virtual void Init();
	virtual void CopyFromSrcClone(uint32_t *dstBufferPtr, const NDSDisplayID displayID, const u8 bufferIndex);
	virtual void SetFetchBuffers(const NDSDisplayInfo &currentDisplayInfo);
	virtual void FetchFromBufferIndex(const u8 index);
};

#pragma mark -

class MacMetalDisplayView : public ClientDisplay3DView, public DisplayViewCALayerInterface
{
protected:
	pthread_mutex_t *_mutexProcessPtr;
	OSSpinLock _spinlockViewNeedsFlush;
	
	virtual void _UpdateNormalSize();
	virtual void _UpdateOrder();
	virtual void _UpdateRotation();
	virtual void _UpdateClientSize();
	virtual void _UpdateViewScale();
	virtual void _LoadNativeDisplayByID(const NDSDisplayID displayID);
	virtual void _ResizeCPUPixelScaler(const VideoFilterTypeID filterID);
		
public:
	MacMetalDisplayView();
	virtual ~MacMetalDisplayView();
	
	pthread_mutex_t* GetMutexProcessPtr() const;
	
	virtual void Init();
	virtual bool GetViewNeedsFlush();
	virtual void SetAllowViewFlushes(bool allowFlushes);
	
	virtual void CopyHUDFont(const FT_Face &fontFace, const size_t glyphSize, const size_t glyphTileSize, GlyphInfo *glyphInfo);
	
	// NDS screen filters
	virtual void SetPixelScaler(const VideoFilterTypeID filterID);
	virtual void SetOutputFilter(const OutputFilterTypeID filterID);
	virtual void SetFiltersPreferGPU(const bool preferGPU);
	
	// Client view interface
	virtual void ProcessDisplays();
	virtual void UpdateView();
	virtual void FlushView();
};

#pragma mark -
void SetupHQnxLUTs_Metal(id<MTLDevice> &device, id<MTLTexture> &texLQ2xLUT, id<MTLTexture> &texHQ2xLUT, id<MTLTexture> &texHQ3xLUT, id<MTLTexture> &texHQ4xLUT);
void DeleteHQnxLUTs_Metal(id<MTLTexture> &texLQ2xLUT, id<MTLTexture> &texHQ2xLUT, id<MTLTexture> &texHQ3xLUT, id<MTLTexture> &texHQ4xLUT);

#endif // _MAC_METALDISPLAYVIEW_H
