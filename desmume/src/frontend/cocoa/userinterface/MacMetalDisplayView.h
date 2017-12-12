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

#include <mach/task.h>
#include <mach/semaphore.h>
#include <mach/sync_policy.h>

#import "DisplayViewCALayer.h"
#import "../cocoa_GPU.h"
#import "../cocoa_util.h"
#include "../ClientDisplayView.h"

#ifdef BOOL
#undef BOOL
#endif

#define METAL_BUFFER_COUNT 3

class MacMetalFetchObject;
class MacMetalDisplayPresenter;
class MacMetalDisplayView;

struct MetalProcessedFrameInfo
{
	uint8_t processIndex;
    id<MTLTexture> tex[2];
};
typedef struct MetalProcessedFrameInfo MetalProcessedFrameInfo;

struct MetalRenderFrameInfo
{
	uint8_t renderIndex;
	bool needEncodeViewport;
	MTLViewport newViewport;
	bool willDrawHUD;
	bool willDrawHUDInput;
	size_t hudStringLength;
	size_t hudTouchLineLength;
	size_t hudTotalLength;
};
typedef struct MetalRenderFrameInfo MetalRenderFrameInfo;

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
	id<MTLComputePipelineState> _fetch666Pipeline;
	id<MTLComputePipelineState> _fetch888Pipeline;
	id<MTLComputePipelineState> _fetch555ConvertOnlyPipeline;
	id<MTLComputePipelineState> _fetch666ConvertOnlyPipeline;
	id<MTLComputePipelineState> deposterizePipeline;
	id<MTLRenderPipelineState> hudPipeline;
	id<MTLRenderPipelineState> hudRGBAPipeline;
	
	id<MTLSamplerState> samplerHUDBox;
	id<MTLSamplerState> samplerHUDText;
	
	id<MTLBuffer> hudIndexBuffer;
	id<MTLBuffer> _bufDisplayFetchNative[2][2];
	id<MTLBuffer> _bufDisplayFetchCustom[2][2];
	
	id<MTLBuffer> _bufMasterBrightMode[2][2];
	id<MTLBuffer> _bufMasterBrightIntensity[2][2];
	
	size_t _fetchPixelBytes;
	size_t _nativeLineSize;
	size_t _nativeBufferSize;
	size_t _customLineSize;
	size_t _customBufferSize;
	
	id<MTLTexture> _texDisplayFetchNative[2][2];
	id<MTLTexture> _texDisplayFetchCustom[2][2];
	id<MTLTexture> _texDisplayPostprocessNative[2][2];
	id<MTLTexture> _texDisplayPostprocessCustom[2][2];
	
	id<MTLTexture> texFetchMain;
	id<MTLTexture> texFetchTouch;
	
	id<MTLTexture> texLQ2xLUT;
	id<MTLTexture> texHQ2xLUT;
	id<MTLTexture> texHQ3xLUT;
	id<MTLTexture> texHQ4xLUT;
	id<MTLTexture> texCurrentHQnxLUT;
	
	MTLResourceOptions preferredResourceStorageMode;
	
	MTLSize _fetchThreadsPerGroup;
	MTLSize _fetchThreadGroupsPerGridNative;
	MTLSize _fetchThreadGroupsPerGridCustom;
	MTLSize deposterizeThreadsPerGroup;
	MTLSize deposterizeThreadGroupsPerGrid;
	
	BOOL _isSharedBufferTextureSupported;
}

@property (readonly, nonatomic) id<MTLDevice> device;
@property (readonly, nonatomic) id<MTLCommandQueue> commandQueue;
@property (readonly, nonatomic) id<MTLLibrary> defaultLibrary;

@property (readonly, nonatomic) id<MTLComputePipelineState> deposterizePipeline;
@property (readonly, nonatomic) id<MTLRenderPipelineState> hudPipeline;
@property (readonly, nonatomic) id<MTLRenderPipelineState> hudRGBAPipeline;
@property (readonly, nonatomic) id<MTLSamplerState> samplerHUDBox;
@property (readonly, nonatomic) id<MTLSamplerState> samplerHUDText;

@property (readonly, nonatomic) id<MTLBuffer> hudIndexBuffer;

@property (retain) id<MTLTexture> texFetchMain;
@property (retain) id<MTLTexture> texFetchTouch;

@property (readonly, nonatomic) id<MTLTexture> texLQ2xLUT;
@property (readonly, nonatomic) id<MTLTexture> texHQ2xLUT;
@property (readonly, nonatomic) id<MTLTexture> texHQ3xLUT;
@property (readonly, nonatomic) id<MTLTexture> texHQ4xLUT;
@property (retain) id<MTLTexture> texCurrentHQnxLUT;

@property (readonly, nonatomic) MTLResourceOptions preferredResourceStorageMode;

@property (readonly, nonatomic) MTLSize deposterizeThreadsPerGroup;
@property (readonly, nonatomic) MTLSize deposterizeThreadGroupsPerGrid;

- (void) setFetchBuffersWithDisplayInfo:(const NDSDisplayInfo &)dispInfo;
- (void) setFetchTextureBindingsAtIndex:(const u8)index commandBuffer:(id<MTLCommandBuffer>)cb;
- (void) fetchFromBufferIndex:(const u8)index;
- (void) fetchNativeDisplayByID:(const NDSDisplayID)displayID bufferIndex:(const u8)bufferIndex;
- (void) fetchCustomDisplayByID:(const NDSDisplayID)displayID bufferIndex:(const u8)bufferIndex;

@end

@interface MacMetalDisplayPresenterObject : NSObject
{
	ClientDisplay3DPresenter *cdp;
	MetalDisplayViewSharedData *sharedData;
	
	MTLRenderPassDescriptor *_outputRenderPassDesc;
	MTLRenderPassColorAttachmentDescriptor *colorAttachment0Desc;
	id<MTLComputePipelineState> pixelScalePipeline;
	id<MTLRenderPipelineState> outputRGBAPipeline;
	id<MTLRenderPipelineState> outputDrawablePipeline;
	MTLPixelFormat drawableFormat;
	
	id<MTLBuffer> _hudVtxPositionBuffer[METAL_BUFFER_COUNT];
	id<MTLBuffer> _hudVtxColorBuffer[METAL_BUFFER_COUNT];
	id<MTLBuffer> _hudTexCoordBuffer[METAL_BUFFER_COUNT];
	id<MTLBuffer> _bufCPUFilterSrcMain;
	id<MTLBuffer> _bufCPUFilterSrcTouch;
	
	float _vtxPositionBuffer[32];
	float _texCoordBuffer[32];
	DisplayViewShaderProperties _cdvPropertiesBuffer;
	
	id<MTLTexture> _texDisplaySrcDeposterize[METAL_BUFFER_COUNT][2][2]; // [_processIndex][NDSDisplayID][stage]
	id<MTLTexture> texDisplayPixelScaleMain;
	id<MTLTexture> texDisplayPixelScaleTouch;
	id<MTLTexture> texHUDCharMap;
	
	MTLSize _pixelScalerThreadsPerGroup;
	MTLSize _pixelScalerThreadGroupsPerGrid;
	
	BOOL needsViewportUpdate;
	BOOL needsRotationScaleUpdate;
	BOOL needsScreenVerticesUpdate;
	BOOL needsHUDVerticesUpdate;
	
	dispatch_semaphore_t _semProcessBuffers;
	dispatch_semaphore_t _semRenderBuffers;
	OSSpinLock _spinlockProcessedFrameInfo;
	
	uint8_t _processIndex;
	MetalProcessedFrameInfo _mpfi;
	MetalRenderFrameInfo _mrfi;
}

@property (readonly, nonatomic) ClientDisplay3DPresenter *cdp;
@property (assign, nonatomic) MetalDisplayViewSharedData *sharedData;
@property (readonly, nonatomic) MTLRenderPassColorAttachmentDescriptor *colorAttachment0Desc;
@property (retain) id<MTLComputePipelineState> pixelScalePipeline;
@property (retain) id<MTLRenderPipelineState> outputRGBAPipeline;
@property (retain) id<MTLRenderPipelineState> outputDrawablePipeline;
@property (assign) MTLPixelFormat drawableFormat;
@property (retain) id<MTLTexture> texDisplayPixelScaleMain;
@property (retain) id<MTLTexture> texDisplayPixelScaleTouch;
@property (retain) id<MTLTexture> texHUDCharMap;
@property (assign) BOOL needsViewportUpdate;
@property (assign) BOOL needsRotationScaleUpdate;
@property (assign) BOOL needsScreenVerticesUpdate;
@property (assign) BOOL needsHUDVerticesUpdate;
@property (assign, nonatomic) VideoFilterTypeID pixelScaler;
@property (assign, nonatomic) OutputFilterTypeID outputFilter;

- (id) initWithDisplayPresenter:(MacMetalDisplayPresenter *)thePresenter;
- (MetalProcessedFrameInfo) processedFrameInfo;
- (id<MTLCommandBuffer>) newCommandBuffer;
- (void) setup;
- (void) copyHUDFontUsingFace:(const FT_Face &)fontFace size:(const size_t)glyphSize tileSize:(const size_t)glyphTileSize info:(GlyphInfo *)glyphInfo;
- (void) processDisplays;
- (void) updateRenderBuffers;
- (void) renderForCommandBuffer:(id<MTLCommandBuffer>)cb
			outputPipelineState:(id<MTLRenderPipelineState>)outputPipelineState
			   hudPipelineState:(id<MTLRenderPipelineState>)hudPipelineState
				 texDisplayMain:(id<MTLTexture>)texDisplayMain
				texDisplayTouch:(id<MTLTexture>)texDisplayTouch;
- (void) renderFinish;
- (void) renderToBuffer:(uint32_t *)dstBuffer;

@end

@interface DisplayViewMetalLayer : CAMetalLayer<DisplayViewCALayer>
{
	MacDisplayLayeredView *_cdv;
	MacMetalDisplayPresenterObject *presenterObject;
}

@property (readonly, nonatomic) MacMetalDisplayPresenterObject *presenterObject;

- (id) initWithDisplayPresenterObject:(MacMetalDisplayPresenterObject *)thePresenterObject;
- (void) setupLayer;
- (void) renderToDrawable;

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

class MacMetalDisplayPresenter : public ClientDisplay3DPresenter, public MacDisplayPresenterInterface
{
private:
	void __InstanceInit(MacClientSharedObject *sharedObject);
	
protected:
	MacMetalDisplayPresenterObject *_presenterObject;
	pthread_mutex_t _mutexProcessPtr;
	dispatch_semaphore_t _semCPUFilter[2][2];
	
	virtual void _UpdateNormalSize();
	virtual void _UpdateOrder();
	virtual void _UpdateRotation();
	virtual void _UpdateClientSize();
	virtual void _UpdateViewScale();
	virtual void _LoadNativeDisplayByID(const NDSDisplayID displayID);
	
public:
	MacMetalDisplayPresenter();
	MacMetalDisplayPresenter(MacClientSharedObject *sharedObject);
	virtual ~MacMetalDisplayPresenter();
	
	MacMetalDisplayPresenterObject* GetPresenterObject() const;
	pthread_mutex_t* GetMutexProcessPtr();
	dispatch_semaphore_t GetCPUFilterSemaphore(const NDSDisplayID displayID, const uint8_t bufferIndex);
	
	virtual void Init();
	virtual void SetSharedData(MacClientSharedObject *sharedObject);
	virtual void CopyHUDFont(const FT_Face &fontFace, const size_t glyphSize, const size_t glyphTileSize, GlyphInfo *glyphInfo);
	
	// NDS screen filters
	virtual void SetPixelScaler(const VideoFilterTypeID filterID);
	virtual void SetOutputFilter(const OutputFilterTypeID filterID);
	virtual void SetFiltersPreferGPU(const bool preferGPU);
	
	// Client view interface
	virtual void ProcessDisplays();
	
	virtual void CopyFrameToBuffer(uint32_t *dstBuffer);
};

class MacMetalDisplayView : public MacDisplayLayeredView
{
private:
	void __InstanceInit(MacClientSharedObject *sharedObject);
	
protected:
	OSSpinLock _spinlockViewNeedsFlush;
	
public:
	MacMetalDisplayView();
	MacMetalDisplayView(MacClientSharedObject *sharedObject);
	virtual ~MacMetalDisplayView();
	
	virtual void Init();
	
	virtual bool GetViewNeedsFlush();
	virtual void SetViewNeedsFlush();
	
	virtual void SetAllowViewFlushes(bool allowFlushes);
	
	// Client view interface
	virtual void FlushView();
};

#pragma mark -
void SetupHQnxLUTs_Metal(id<MTLDevice> &device, id<MTLCommandQueue> &commandQueue, id<MTLTexture> &texLQ2xLUT, id<MTLTexture> &texHQ2xLUT, id<MTLTexture> &texHQ3xLUT, id<MTLTexture> &texHQ4xLUT);
void DeleteHQnxLUTs_Metal(id<MTLTexture> &texLQ2xLUT, id<MTLTexture> &texHQ2xLUT, id<MTLTexture> &texHQ3xLUT, id<MTLTexture> &texHQ4xLUT);

#endif // _MAC_METALDISPLAYVIEW_H
