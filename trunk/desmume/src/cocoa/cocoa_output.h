/*
	Copyright (C) 2011 Roger Manuel
	Copyright (C) 2012 DeSmuME team

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

#import <Cocoa/Cocoa.h>
#include <pthread.h>
#include <libkern/OSAtomic.h>

#import "cocoa_util.h"


@interface CocoaDSOutput : CocoaDSThread
{
	BOOL isStateChanged;
	NSUInteger frameCount;
	NSData *frameData;
	NSMutableDictionary *property;
	
	pthread_mutex_t *mutexProducer;
	pthread_mutex_t *mutexConsume;
}

@property (assign) BOOL isStateChanged;
@property (assign) NSUInteger frameCount;
@property (retain) NSData *frameData;
@property (readonly) NSMutableDictionary *property;
@property (assign) pthread_mutex_t *mutexProducer;
@property (readonly) pthread_mutex_t *mutexConsume;

- (void) doCoreEmuFrame;
- (void) handleEmuFrameProcessed:(NSData *)theData;

@end

@interface CocoaDSSpeaker : CocoaDSOutput
{
	NSUInteger bufferSize;
	
	OSSpinLock spinlockAudioOutputEngine;
	OSSpinLock spinlockVolume;
	OSSpinLock spinlockSpuAdvancedLogic;
	OSSpinLock spinlockSpuInterpolationMode;
	OSSpinLock spinlockSpuSyncMode;
	OSSpinLock spinlockSpuSyncMethod;
}

@property (assign) NSUInteger bufferSize;

- (id) init;
- (id) initWithVolume:(CGFloat)vol;
- (void) dealloc;

- (void) setVolume:(float)vol;
- (float) volume;
- (void) setAudioOutputEngine:(NSInteger)methodID;
- (NSInteger) audioOutputEngine;
- (void) setSpuAdvancedLogic:(BOOL)state;
- (BOOL) spuAdvancedLogic;
- (void) setSpuInterpolationMode:(NSInteger)modeID;
- (NSInteger) spuInterpolationMode;
- (void) setSpuSyncMode:(NSInteger)modeID;
- (NSInteger) spuSyncMode;
- (void) setSpuSyncMethod:(NSInteger)methodID;
- (NSInteger) spuSyncMethod;

- (BOOL) mute;
- (void) setMute:(BOOL)mute;
- (NSInteger) filter;
- (void) setFilter:(NSInteger)filter;
- (void) handleSetVolume:(NSData *)volumeData;
- (void) handleSetAudioOutputEngine:(NSData *)methodIdData;
- (void) handleSetSpuAdvancedLogic:(NSData *)stateData;
- (void) handleSetSpuSyncMode:(NSData *)modeIdData;
- (void) handleSetSpuSyncMethod:(NSData *)methodIdData;
- (void) handleSetSpuInterpolationMode:(NSData *)modeIdData;

@end

@class CocoaVideoFilter;

@protocol CocoaDSDisplayDelegate <NSObject>

@required
- (void) doInitVideoOutput:(NSDictionary *)properties;
- (void) doProcessVideoFrame:(const void *)videoFrameData frameSize:(NSSize)frameSize;

@property (retain) NSPort *sendPortDisplay;

@optional
- (void) doResizeView:(NSRect)rect;
- (void) doRedraw;
- (void) doDisplayTypeChanged:(NSInteger)displayTypeID;
- (void) doDisplayOrientationChanged:(NSInteger)displayOrientationID;
- (void) doDisplayOrderChanged:(NSInteger)displayOrderID;
- (void) doBilinearOutputChanged:(BOOL)useBilinear;
- (void) doVerticalSyncChanged:(BOOL)useVerticalSync;
- (void) doVideoFilterChanged:(NSInteger)videoFilterTypeID;

@property (assign) BOOL isHudEnabled;
@property (assign) BOOL isHudEditingModeEnabled;

@end


@interface CocoaDSDisplay : CocoaDSOutput
{
	UInt32 gpuStateFlags;
	id <CocoaDSDisplayDelegate> delegate;
	NSInteger displayType;
	CocoaVideoFilter *vf;
	
	pthread_mutex_t *mutexRender3D;
	OSSpinLock spinlockDelegate;
	OSSpinLock spinlockGpuState;
	OSSpinLock spinlockDisplayType;
	OSSpinLock spinlockVideoFilterType;
	OSSpinLock spinlockVfSrcBuffer;
	OSSpinLock spinlockRender3DRenderingEngine;
	OSSpinLock spinlockRender3DHighPrecisionColorInterpolation;
	OSSpinLock spinlockRender3DEdgeMarking;
	OSSpinLock spinlockRender3DFog;
	OSSpinLock spinlockRender3DTextures;
	OSSpinLock spinlockRender3DDepthComparisonThreshold;
	OSSpinLock spinlockRender3DThreads;
	OSSpinLock spinlockRender3DLineHack;
}

@property (assign) UInt32 gpuStateFlags;
@property (retain) id <CocoaDSDisplayDelegate> delegate;
@property (assign) NSInteger displayType;
@property (assign) CocoaVideoFilter *vf;
@property (readonly) pthread_mutex_t *mutexRender3D;

- (void) setRender3DRenderingEngine:(NSInteger)methodID;
- (NSInteger) render3DRenderingEngine;
- (void) setRender3DHighPrecisionColorInterpolation:(BOOL)state;
- (BOOL) render3DHighPrecisionColorInterpolation;
- (void) setRender3DEdgeMarking:(BOOL)state;
- (BOOL) render3DEdgeMarking;
- (void) setRender3DFog:(BOOL)state;
- (BOOL) render3DFog;
- (void) setRender3DTextures:(BOOL)state;
- (BOOL) render3DTextures;
- (void) setRender3DDepthComparisonThreshold:(NSUInteger)threshold;
- (NSUInteger) render3DDepthComparisonThreshold;
- (void) setRender3DThreads:(NSUInteger)numberThreads;
- (NSUInteger) render3DThreads;
- (void) setRender3DLineHack:(BOOL)state;
- (BOOL) render3DLineHack;

- (void) handleResizeView:(NSData *)rectData;
- (void) handleRedrawView;
- (void) handleChangeGpuStateFlags:(NSData *)flagsData;
- (void) handleChangeDisplayType:(NSData *)displayTypeIdData;
- (void) handleChangeDisplayOrientation:(NSData *)displayOrientationIdData;
- (void) handleChangeDisplayOrder:(NSData *)displayOrderIdData;
- (void) handleChangeBilinearOutput:(NSData *)bilinearStateData;
- (void) handleChangeVerticalSync:(NSData *)verticalSyncStateData;
- (void) handleChangeVideoFilter:(NSData *)videoFilterTypeIdData;
- (void) handleSetRender3DRenderingEngine:(NSData *)methodIdData;
- (void) handleSetRender3DHighPrecisionColorInterpolation:(NSData *)stateData;
- (void) handleSetRender3DEdgeMarking:(NSData *)stateData;
- (void) handleSetRender3DFog:(NSData *)stateData;
- (void) handleSetRender3DTextures:(NSData *)stateData;
- (void) handleSetRender3DDepthComparisonThreshold:(NSData *)thresholdData;
- (void) handleSetRender3DThreads:(NSData *)numberThreadsData;
- (void) handleSetRender3DLineHack:(NSData *)stateData;
- (void) handleSetViewToBlack;
- (void) handleSetViewToWhite;
- (void) handleRequestScreenshot:(NSData *)fileURLStringData fileTypeData:(NSData *)fileTypeData;
- (void) handleCopyToPasteboard;

- (void) fillVideoFrameWithColor:(UInt8)colorValue;

- (NSImage *) image;
- (NSBitmapImageRep *) bitmapImageRep;

- (BOOL) gpuStateByBit:(UInt32)stateBit;
- (BOOL) isGPUTypeDisplayed:(NSInteger)theGpuType;
- (void) hideGPUType:(NSInteger)theGpuType;
- (void) showGPUType:(NSInteger)theGpuType;

@end

#ifdef __cplusplus
extern "C"
{
#endif

void HandleMessageEchoResponse(NSPortMessage *portMessage);
void SetGPULayerState(int displayType, unsigned int i, bool state);
bool GetGPULayerState(int displayType, unsigned int i);
void SetGPUDisplayState(int displayType, bool state);
bool GetGPUDisplayState(int displayType);

void SetOpenGLRendererFunctions(bool (*initFunction)(),
								bool (*beginOGLFunction)(),
								void (*endOGLFunction)());

#ifdef __cplusplus
}
#endif
