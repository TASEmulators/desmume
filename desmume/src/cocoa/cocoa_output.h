/*
	Copyright (C) 2011 Roger Manuel
	Copyright (C) 2013 DeSmuME team

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


typedef struct
{
	double	scale;
	double	rotation;		// Angle is in degrees
	double	translationX;
	double	translationY;
	double	translationZ;
} DisplayOutputTransformData;

typedef struct
{
	NSInteger displayModeID;
	unsigned int width;			// Measured in pixels
	unsigned int height;		// Measured in pixels
} DisplaySrcPixelAttributes;

@interface CocoaDSOutput : CocoaDSThread
{
	BOOL isStateChanged;
	NSUInteger frameCount;
	NSData *frameData;
	NSData *frameAttributesData;
	NSMutableDictionary *property;
	
	pthread_mutex_t *mutexProducer;
	pthread_mutex_t *mutexConsume;
}

@property (assign) BOOL isStateChanged;
@property (assign) NSUInteger frameCount;
@property (retain) NSData *frameData;
@property (retain) NSData *frameAttributesData;
@property (readonly) NSMutableDictionary *property;
@property (assign) pthread_mutex_t *mutexProducer;
@property (readonly) pthread_mutex_t *mutexConsume;

- (void) doCoreEmuFrame;
- (void) handleEmuFrameProcessed:(NSData *)mainData attributes:(NSData *)attributesData;

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
- (void) doDisplayModeChanged:(NSInteger)displayModeID;

@property (retain) NSPort *sendPortDisplay;
@property (assign) BOOL isHudEnabled;
@property (assign) BOOL isHudEditingModeEnabled;

@end

@protocol CocoaDSDisplayVideoDelegate <CocoaDSDisplayDelegate>

@required
- (void) doInitVideoOutput:(NSDictionary *)properties;
- (void) doProcessVideoFrame:(const void *)videoFrameData displayMode:(const NSInteger)displayModeID width:(const NSInteger)frameWidth height:(const NSInteger)frameHeight;

@optional
- (void) doResizeView:(NSRect)rect;
- (void) doTransformView:(DisplayOutputTransformData *)transformData;
- (void) doRedraw;
- (void) doDisplayOrientationChanged:(NSInteger)displayOrientationID;
- (void) doDisplayOrderChanged:(NSInteger)displayOrderID;
- (void) doBilinearOutputChanged:(BOOL)useBilinear;
- (void) doVerticalSyncChanged:(BOOL)useVerticalSync;
- (void) doVideoFilterChanged:(NSInteger)videoFilterTypeID frameSize:(NSSize)videoFilterDestSize;

@end


@interface CocoaDSDisplay : CocoaDSOutput
{
	UInt32 gpuStateFlags;
	id <CocoaDSDisplayDelegate> delegate;
	NSInteger displayMode;
	NSSize frameSize;
	
	pthread_mutex_t *mutexRender3D;
	OSSpinLock spinlockDelegate;
	OSSpinLock spinlockGpuState;
	OSSpinLock spinlockDisplayType;
	OSSpinLock spinlockRender3DRenderingEngine;
	OSSpinLock spinlockRender3DHighPrecisionColorInterpolation;
	OSSpinLock spinlockRender3DEdgeMarking;
	OSSpinLock spinlockRender3DFog;
	OSSpinLock spinlockRender3DTextures;
	OSSpinLock spinlockRender3DDepthComparisonThreshold;
	OSSpinLock spinlockRender3DThreads;
	OSSpinLock spinlockRender3DLineHack;
	OSSpinLock spinlockRender3DMultisample;
}

@property (assign) UInt32 gpuStateFlags;
@property (retain) id <CocoaDSDisplayDelegate> delegate;
@property (assign) NSInteger displayMode;
@property (readonly) NSSize frameSize;
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
- (void) setRender3DMultisample:(BOOL)state;
- (BOOL) render3DMultisample;

- (void) handleChangeGpuStateFlags:(NSData *)flagsData;
- (void) handleChangeDisplayMode:(NSData *)displayModeData;
- (void) handleSetRender3DRenderingEngine:(NSData *)methodIdData;
- (void) handleSetRender3DHighPrecisionColorInterpolation:(NSData *)stateData;
- (void) handleSetRender3DEdgeMarking:(NSData *)stateData;
- (void) handleSetRender3DFog:(NSData *)stateData;
- (void) handleSetRender3DTextures:(NSData *)stateData;
- (void) handleSetRender3DDepthComparisonThreshold:(NSData *)thresholdData;
- (void) handleSetRender3DThreads:(NSData *)numberThreadsData;
- (void) handleSetRender3DLineHack:(NSData *)stateData;
- (void) handleSetRender3DMultisample:(NSData *)stateData;
- (void) handleSetViewToBlack;
- (void) handleSetViewToWhite;
- (void) handleRequestScreenshot:(NSData *)fileURLStringData fileTypeData:(NSData *)fileTypeData;
- (void) handleCopyToPasteboard;

- (void) fillVideoFrameWithColor:(UInt16)colorValue;

- (NSImage *) image;
- (NSBitmapImageRep *) bitmapImageRep;

- (BOOL) gpuStateByBit:(UInt32)stateBit;
- (BOOL) isGPUTypeDisplayed:(NSInteger)theGpuType;
- (void) hideGPUType:(NSInteger)theGpuType;
- (void) showGPUType:(NSInteger)theGpuType;

@end

@interface CocoaDSDisplayVideo : CocoaDSDisplay
{
	CocoaVideoFilter *vf;
	id <CocoaDSDisplayVideoDelegate> videoDelegate;
	NSInteger lastDisplayMode;
		
	OSSpinLock spinlockVideoFilterType;
	OSSpinLock spinlockVFBuffers;
}

@property (retain) id <CocoaDSDisplayVideoDelegate> delegate;
@property (assign) CocoaVideoFilter *vf;

- (void) handleResizeView:(NSData *)rectData;
- (void) handleTransformView:(NSData *)transformData;
- (void) handleRedrawView;
- (void) handleChangeDisplayOrientation:(NSData *)displayOrientationIdData;
- (void) handleChangeDisplayOrder:(NSData *)displayOrderIdData;
- (void) handleChangeBilinearOutput:(NSData *)bilinearStateData;
- (void) handleChangeVerticalSync:(NSData *)verticalSyncStateData;
- (void) handleChangeVideoFilter:(NSData *)videoFilterTypeIdData;

@end

#ifdef __cplusplus
extern "C"
{
#endif

void HandleMessageEchoResponse(NSPortMessage *portMessage);
void SetGPULayerState(int gpuType, unsigned int i, bool state);
bool GetGPULayerState(int gpuType, unsigned int i);
void SetGPUDisplayState(int gpuType, bool state);
bool GetGPUDisplayState(int gpuType);

bool OSXOpenGLRendererInit();
bool OSXOpenGLRendererBegin();
void OSXOpenGLRendererEnd();

bool CreateOpenGLRenderer();
void DestroyOpenGLRenderer();
void RequestOpenGLRenderer_3_2(bool request_3_2);
void SetOpenGLRendererFunctions(bool (*initFunction)(),
								bool (*beginOGLFunction)(),
								void (*endOGLFunction)());

#ifdef __cplusplus
}
#endif
