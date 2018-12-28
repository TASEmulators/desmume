/*
	Copyright (C) 2017-2018 DeSmuME team

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

#define METAL_FETCH_BUFFER_COUNT	3
#define RENDER_BUFFER_COUNT			12

class MacMetalFetchObject;
class MacMetalDisplayPresenter;
class MacMetalDisplayView;

struct MetalTexturePair
{
	uint8_t bufferIndex;
	size_t fetchSequenceNumber;
	
	union
	{
		id<MTLTexture> tex[2];
		
		struct
		{
			id<MTLTexture> main;
			id<MTLTexture> touch;
		};
	};
};
typedef struct MetalTexturePair MetalTexturePair;

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
	id<MTLCommandQueue> _fetchCommandQueue;
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
	id<MTLBuffer> _bufDisplayFetchNative[2][METAL_FETCH_BUFFER_COUNT];
	id<MTLBuffer> _bufDisplayFetchCustom[2][METAL_FETCH_BUFFER_COUNT];
	
	id<MTLBuffer> _bufMasterBrightMode[2][METAL_FETCH_BUFFER_COUNT];
	id<MTLBuffer> _bufMasterBrightIntensity[2][METAL_FETCH_BUFFER_COUNT];
	
	size_t _fetchPixelBytes;
	size_t _nativeLineSize;
	size_t _nativeBufferSize;
	size_t _customLineSize;
	size_t _customBufferSize;
	
	id<MTLTexture> _texDisplayFetchNative[2][METAL_FETCH_BUFFER_COUNT];
	id<MTLTexture> _texDisplayFetchCustom[2][METAL_FETCH_BUFFER_COUNT];
	id<MTLTexture> _texDisplayPostprocessNative[2][METAL_FETCH_BUFFER_COUNT];
	id<MTLTexture> _texDisplayPostprocessCustom[2][METAL_FETCH_BUFFER_COUNT];
	
	MetalTexturePair texPairFetch;
	id<MTLBlitCommandEncoder> bceFetch;
	
	id<MTLTexture> texLQ2xLUT;
	id<MTLTexture> texHQ2xLUT;
	id<MTLTexture> texHQ3xLUT;
	id<MTLTexture> texHQ4xLUT;
	id<MTLTexture> texCurrentHQnxLUT;
	
	MTLSize _fetchThreadsPerGroupNative;
	MTLSize _fetchThreadGroupsPerGridNative;
	MTLSize _fetchThreadsPerGroupCustom;
	MTLSize _fetchThreadGroupsPerGridCustom;
	MTLSize deposterizeThreadsPerGroup;
	MTLSize deposterizeThreadGroupsPerGrid;
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

@property (assign) MetalTexturePair texPairFetch;
@property (assign) id<MTLBlitCommandEncoder> bceFetch;

@property (readonly, nonatomic) id<MTLTexture> texLQ2xLUT;
@property (readonly, nonatomic) id<MTLTexture> texHQ2xLUT;
@property (readonly, nonatomic) id<MTLTexture> texHQ3xLUT;
@property (readonly, nonatomic) id<MTLTexture> texHQ4xLUT;
@property (retain) id<MTLTexture> texCurrentHQnxLUT;

@property (readonly, nonatomic) MTLSize deposterizeThreadsPerGroup;
@property (readonly, nonatomic) MTLSize deposterizeThreadGroupsPerGrid;

- (void) setFetchBuffersWithDisplayInfo:(const NDSDisplayInfo &)dispInfo;
- (MetalTexturePair) setFetchTextureBindingsAtIndex:(const uint8_t)index commandBuffer:(id<MTLCommandBuffer>)cb;
- (void) fetchFromBufferIndex:(const u8)index;
- (void) fetchNativeDisplayByID:(const NDSDisplayID)displayID bufferIndex:(const u8)bufferIndex blitCommandEncoder:(id<MTLBlitCommandEncoder>)bce;
- (void) fetchCustomDisplayByID:(const NDSDisplayID)displayID bufferIndex:(const u8)bufferIndex blitCommandEncoder:(id<MTLBlitCommandEncoder>)bce;

- (void) flushMultipleViews:(const std::vector<ClientDisplay3DView *> &)cdvFlushList timeStampNow:(const CVTimeStamp *)timeStampNow timeStampOutput:(const CVTimeStamp *)timeStampOutput;

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
	
	id<MTLBuffer> _hudVtxPositionBuffer[RENDER_BUFFER_COUNT];
	id<MTLBuffer> _hudVtxColorBuffer[RENDER_BUFFER_COUNT];
	id<MTLBuffer> _hudTexCoordBuffer[RENDER_BUFFER_COUNT];
	id<MTLBuffer> _bufCPUFilterSrcMain;
	id<MTLBuffer> _bufCPUFilterSrcTouch;
	id<MTLBuffer> bufCPUFilterDstMain;
	id<MTLBuffer> bufCPUFilterDstTouch;
	
	float _vtxPositionBuffer[32];
	float _texCoordBuffer[32];
	DisplayViewShaderProperties _cdvPropertiesBuffer;
	
	id<MTLTexture> _texDisplaySrcDeposterize[2][2]; // [NDSDisplayID][stage]
	id<MTLTexture> _texDisplayPixelScaler[2];
	id<MTLTexture> texHUDCharMap;
	
	MTLSize _pixelScalerThreadsPerGroup;
	MTLSize _pixelScalerThreadGroupsPerGrid;
	
	BOOL needsProcessFrameWait;
	BOOL needsViewportUpdate;
	BOOL needsRotationScaleUpdate;
	BOOL needsScreenVerticesUpdate;
	BOOL needsHUDVerticesUpdate;
	
	OSSpinLock _spinlockRenderBufferStates[RENDER_BUFFER_COUNT];
	dispatch_semaphore_t _semRenderBuffers[RENDER_BUFFER_COUNT];
	volatile ClientDisplayBufferState _renderBufferState[RENDER_BUFFER_COUNT];
	
	MetalTexturePair texPairProcess;
	MetalRenderFrameInfo renderFrameInfo;
}

@property (readonly, nonatomic) ClientDisplay3DPresenter *cdp;
@property (assign, nonatomic) MetalDisplayViewSharedData *sharedData;
@property (readonly, nonatomic) MTLRenderPassColorAttachmentDescriptor *colorAttachment0Desc;
@property (retain) id<MTLComputePipelineState> pixelScalePipeline;
@property (retain) id<MTLRenderPipelineState> outputRGBAPipeline;
@property (retain) id<MTLRenderPipelineState> outputDrawablePipeline;
@property (assign) MTLPixelFormat drawableFormat;
@property (retain) id<MTLBuffer> bufCPUFilterDstMain;
@property (retain) id<MTLBuffer> bufCPUFilterDstTouch;
@property (retain) id<MTLTexture> texHUDCharMap;
@property (assign) BOOL needsProcessFrameWait;
@property (assign) BOOL needsViewportUpdate;
@property (assign) BOOL needsRotationScaleUpdate;
@property (assign) BOOL needsScreenVerticesUpdate;
@property (assign) BOOL needsHUDVerticesUpdate;
@property (assign) MetalTexturePair texPairProcess;
@property (assign) MetalRenderFrameInfo renderFrameInfo;
@property (assign, nonatomic) VideoFilterTypeID pixelScaler;
@property (assign, nonatomic) OutputFilterTypeID outputFilter;

- (id) initWithDisplayPresenter:(MacMetalDisplayPresenter *)thePresenter;
- (id<MTLCommandBuffer>) newCommandBuffer;
- (void) setup;
- (void) resizeCPUPixelScalerUsingFilterID:(const VideoFilterTypeID)filterID;
- (void) copyHUDFontUsingFace:(const FT_Face &)fontFace size:(const size_t)glyphSize tileSize:(const size_t)glyphTileSize info:(GlyphInfo *)glyphInfo;
- (void) processDisplays;
- (void) updateRenderBuffers;
- (void) renderForCommandBuffer:(id<MTLCommandBuffer>)cb
			outputPipelineState:(id<MTLRenderPipelineState>)outputPipelineState
			   hudPipelineState:(id<MTLRenderPipelineState>)hudPipelineState
					texDisplays:(MetalTexturePair)texDisplay
						   mrfi:(MetalRenderFrameInfo)mrfi
						doYFlip:(BOOL)willFlip;
- (void) renderStartAtIndex:(uint8_t)index;
- (void) renderFinishAtIndex:(uint8_t)index;
- (ClientDisplayBufferState) renderBufferStateAtIndex:(uint8_t)index;
- (void) setRenderBufferState:(ClientDisplayBufferState)bufferState index:(uint8_t)index;
- (void) renderToBuffer:(uint32_t *)dstBuffer;

@end

@interface DisplayViewMetalLayer : CAMetalLayer<DisplayViewCALayer>
{
	MacDisplayLayeredView *_cdv;
	MacMetalDisplayPresenterObject *presenterObject;
	dispatch_semaphore_t _semDrawable;
	id<CAMetalDrawable> _currentDrawable;
	id<CAMetalDrawable> layerDrawable0;
	id<CAMetalDrawable> layerDrawable1;
	id<CAMetalDrawable> layerDrawable2;
	
	MetalTexturePair _displayTexturePair;
}

@property (readonly, nonatomic) MacMetalDisplayPresenterObject *presenterObject;
@property (retain) id<CAMetalDrawable> layerDrawable0;
@property (retain) id<CAMetalDrawable> layerDrawable1;
@property (retain) id<CAMetalDrawable> layerDrawable2;

- (id) initWithDisplayPresenterObject:(MacMetalDisplayPresenterObject *)thePresenterObject;
- (void) setupLayer;
- (void) renderToDrawableUsingCommandBuffer:(id<MTLCommandBuffer>)cb;
- (void) presentDrawableWithCommandBuffer:(id<MTLCommandBuffer>)cb outputTime:(uint64_t)outputTime;
- (void) renderAndPresentDrawableImmediate;

@end

#pragma mark -

class MacMetalFetchObject : public GPUClientFetchObject
{
protected:
	bool _useDirectToCPUFilterPipeline;
	uint32_t *_srcNativeCloneMaster;
	uint32_t *_srcNativeClone[2][METAL_FETCH_BUFFER_COUNT];
	pthread_rwlock_t _srcCloneRWLock[2][METAL_FETCH_BUFFER_COUNT];
	
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
	dispatch_semaphore_t _semCPUFilter[2];
	
	virtual void _UpdateNormalSize();
	virtual void _UpdateOrder();
	virtual void _UpdateRotation();
	virtual void _UpdateClientSize();
	virtual void _UpdateViewScale();
	virtual void _LoadNativeDisplayByID(const NDSDisplayID displayID);
	virtual void _ResizeCPUPixelScaler(const VideoFilterTypeID filterID);
	
public:
	MacMetalDisplayPresenter();
	MacMetalDisplayPresenter(MacClientSharedObject *sharedObject);
	virtual ~MacMetalDisplayPresenter();
	
	MacMetalDisplayPresenterObject* GetPresenterObject() const;
	pthread_mutex_t* GetMutexProcessPtr();
	dispatch_semaphore_t GetCPUFilterSemaphore(const NDSDisplayID displayID);
	
	virtual void Init();
	virtual void SetSharedData(MacClientSharedObject *sharedObject);
	virtual void CopyHUDFont(const FT_Face &fontFace, const size_t glyphSize, const size_t glyphTileSize, GlyphInfo *glyphInfo);
	
	// NDS screen filters
	virtual void SetPixelScaler(const VideoFilterTypeID filterID);
	virtual void SetOutputFilter(const OutputFilterTypeID filterID);
	virtual void SetFiltersPreferGPU(const bool preferGPU);
	
	// Client view interface
	virtual void ProcessDisplays();
	virtual void UpdateLayout();
	
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
	virtual void FlushView(void *userData);
	virtual void FinalizeFlush(void *userData, uint64_t outputTime);
	virtual void FlushAndFinalizeImmediate();
};

#pragma mark -
void SetupHQnxLUTs_Metal(id<MTLDevice> &device, id<MTLCommandQueue> &commandQueue, id<MTLTexture> &texLQ2xLUT, id<MTLTexture> &texHQ2xLUT, id<MTLTexture> &texHQ3xLUT, id<MTLTexture> &texHQ4xLUT);
void DeleteHQnxLUTs_Metal(id<MTLTexture> &texLQ2xLUT, id<MTLTexture> &texHQ2xLUT, id<MTLTexture> &texHQ3xLUT, id<MTLTexture> &texHQ4xLUT);

#endif // _MAC_METALDISPLAYVIEW_H
