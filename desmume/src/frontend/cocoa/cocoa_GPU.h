/*
	Copyright (C) 2013-2018 DeSmuME team

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

#import <Foundation/Foundation.h>
#import <CoreVideo/CoreVideo.h>
#include <pthread.h>
#include <libkern/OSAtomic.h>
#include <mach/task.h>
#include <mach/semaphore.h>
#include <mach/sync_policy.h>
#include <map>
#include <vector>

#import "cocoa_util.h"
#include "../../GPU.h"

// This symbol only exists in the kernel headers, but not in the user headers.
// Manually define the symbol here, since we will be Mach semaphores in the user-space.
#ifndef SYNC_POLICY_PREPOST
#define SYNC_POLICY_PREPOST 0x4
#endif

#ifdef BOOL
#undef BOOL
#endif

#if defined(PORT_VERSION_OS_X_APP)
	#define ENABLE_SHARED_FETCH_OBJECT
#endif

#if defined(ENABLE_SHARED_FETCH_OBJECT) && !defined(METAL_DISABLE_FOR_BUILD_TARGET) && defined(MAC_OS_X_VERSION_10_11) && (MAC_OS_X_VERSION_MAX_ALLOWED >= MAC_OS_X_VERSION_10_11)
	#define ENABLE_APPLE_METAL
#endif

#define VIDEO_FLUSH_TIME_LIMIT_OFFSET	8	// The amount of time, in seconds, to wait for a flush to occur on a given CVDisplayLink before stopping it.

enum ClientDisplayBufferState
{
	ClientDisplayBufferState_Idle			= 0,	// The buffer has already been read and is currently idle. It is a candidate for a read or write operation.
	ClientDisplayBufferState_Writing		= 1,	// The buffer is currently being written. It cannot be accessed.
	ClientDisplayBufferState_Ready			= 2,	// The buffer was just written to, but has not been read yet. It is a candidate for a read or write operation.
	ClientDisplayBufferState_PendingRead	= 3,	// The buffer has been marked that it will be read. It must not be accessed.
	ClientDisplayBufferState_Reading		= 4		// The buffer is currently being read. It cannot be accessed.
};

class GPUEventHandlerOSX;
class ClientDisplay3DView;

#ifdef ENABLE_SHARED_FETCH_OBJECT

typedef std::map<CGDirectDisplayID, CVDisplayLinkRef> DisplayLinksActiveMap;
typedef std::map<CGDirectDisplayID, int64_t> DisplayLinkFlushTimeLimitMap;

@interface MacClientSharedObject : NSObject
{
	GPUClientFetchObject *GPUFetchObject;
	task_t _taskEmulationLoop;
	
	OSSpinLock _spinlockFramebufferStates[MAX_FRAMEBUFFER_PAGES];
	semaphore_t _semFramebuffer[MAX_FRAMEBUFFER_PAGES];
	volatile ClientDisplayBufferState _framebufferState[MAX_FRAMEBUFFER_PAGES];
	
	pthread_rwlock_t *_rwlockOutputList;
	pthread_mutex_t _mutexDisplayLinkLists;
	NSMutableArray *_cdsOutputList;
	volatile int32_t numberViewsUsingDirectToCPUFiltering;
	
	DisplayLinksActiveMap _displayLinksActiveList;
	DisplayLinkFlushTimeLimitMap _displayLinkFlushTimeList;
	
	OSSpinLock spinlockFetchSignal;
	uint32_t _threadMessageID;
	uint8_t _fetchIndex;
	pthread_t _threadFetch;
	pthread_cond_t _condSignalFetch;
	pthread_mutex_t _mutexFetchExecute;
}

@property (assign, nonatomic) GPUClientFetchObject *GPUFetchObject;
@property (readonly, nonatomic) volatile int32_t numberViewsUsingDirectToCPUFiltering;

- (void) semaphoreFramebufferCreate;
- (void) semaphoreFramebufferDestroy;
- (u8) selectBufferIndex:(const u8)currentIndex pageCount:(size_t)pageCount;
- (semaphore_t) semaphoreFramebufferPageAtIndex:(const u8)bufferIndex;
- (ClientDisplayBufferState) framebufferStateAtIndex:(uint8_t)index;
- (void) setFramebufferState:(ClientDisplayBufferState)bufferState index:(uint8_t)index;

- (void) setOutputList:(NSMutableArray *)theOutputList rwlock:(pthread_rwlock_t *)theRWLock;
- (void) incrementViewsUsingDirectToCPUFiltering;
- (void) decrementViewsUsingDirectToCPUFiltering;
- (void) pushVideoDataToAllDisplayViews;

- (void) flushAllDisplaysOnDisplayLink:(CVDisplayLinkRef)displayLink timeStampNow:(const CVTimeStamp *)timeStampNow timeStampOutput:(const CVTimeStamp *)timeStampOutput;
- (void) flushMultipleViews:(const std::vector<ClientDisplay3DView *> &)cdvFlushList timeStampNow:(const CVTimeStamp *)timeStampNow timeStampOutput:(const CVTimeStamp *)timeStampOutput;

- (void) displayLinkStartUsingID:(CGDirectDisplayID)displayID;
- (void) displayLinkListUpdate;

- (void) fetchSynchronousAtIndex:(uint8_t)index;
- (void) signalFetchAtIndex:(uint8_t)index message:(int32_t)messageID;
- (void) runFetchLoop;

@end

#endif

@interface CocoaDSGPU : NSObject
{
	UInt32 gpuStateFlags;
	uint8_t _gpuScale;
	NSUInteger openglDeviceMaxMultisamples;
	NSString *render3DMultisampleSizeString;
	BOOL isCPUCoreCountAuto;
	BOOL _needRestoreRender3DLock;
	
	OSSpinLock spinlockGpuState;
	GPUEventHandlerOSX *gpuEvent;
	
	GPUClientFetchObject *fetchObject;
}

@property (assign) UInt32 gpuStateFlags;
@property (assign) NSSize gpuDimensions;
@property (assign) NSUInteger gpuScale;
@property (assign) NSUInteger gpuColorFormat;

@property (readonly) NSUInteger openglDeviceMaxMultisamples;

@property (assign) BOOL layerMainGPU;
@property (assign) BOOL layerMainBG0;
@property (assign) BOOL layerMainBG1;
@property (assign) BOOL layerMainBG2;
@property (assign) BOOL layerMainBG3;
@property (assign) BOOL layerMainOBJ;
@property (assign) BOOL layerSubGPU;
@property (assign) BOOL layerSubBG0;
@property (assign) BOOL layerSubBG1;
@property (assign) BOOL layerSubBG2;
@property (assign) BOOL layerSubBG3;
@property (assign) BOOL layerSubOBJ;

@property (assign) NSInteger render3DRenderingEngine;
@property (assign) BOOL render3DHighPrecisionColorInterpolation;
@property (assign) BOOL render3DEdgeMarking;
@property (assign) BOOL render3DFog;
@property (assign) BOOL render3DTextures;
@property (assign) NSUInteger render3DThreads;
@property (assign) BOOL render3DLineHack;
@property (assign) NSUInteger render3DMultisampleSize;
@property (retain) NSString *render3DMultisampleSizeString;
@property (assign) BOOL render3DTextureDeposterize;
@property (assign) BOOL render3DTextureSmoothing;
@property (assign) NSUInteger render3DTextureScalingFactor;
@property (assign) BOOL render3DFragmentSamplingHack;
@property (assign) BOOL openGLEmulateShadowPolygon;
@property (assign) BOOL openGLEmulateSpecialZeroAlphaBlending;
@property (assign) BOOL openGLEmulateNDSDepthCalculation;
@property (assign) BOOL openGLEmulateDepthLEqualPolygonFacing;

#ifdef ENABLE_SHARED_FETCH_OBJECT
@property (readonly, nonatomic) GPUClientFetchObject *fetchObject;
@property (readonly, nonatomic) MacClientSharedObject *sharedData;

- (void) setOutputList:(NSMutableArray *)theOutputList rwlock:(pthread_rwlock_t *)theRWLock;
#endif

- (BOOL) gpuStateByBit:(const UInt32)stateBit;
- (NSString *) render3DRenderingEngineString;
- (void) clearWithColor:(const uint16_t)colorBGRA5551;
- (void) respondToPauseState:(BOOL)isPaused;

@end

#ifdef __cplusplus
extern "C"
{
#endif

#ifdef ENABLE_SHARED_FETCH_OBJECT

static void* RunFetchThread(void *arg);

CVReturn MacDisplayLinkCallback(CVDisplayLinkRef displayLink,
								const CVTimeStamp *inNow,
								const CVTimeStamp *inOutputTime,
								CVOptionFlags flagsIn,
								CVOptionFlags *flagsOut,
								void *displayLinkContext);
#endif

bool OSXOpenGLRendererInit();
bool OSXOpenGLRendererBegin();
void OSXOpenGLRendererEnd();
bool OSXOpenGLRendererFramebufferDidResize(const bool isFBOSupported, size_t w, size_t h);

bool CreateOpenGLRenderer();
void DestroyOpenGLRenderer();
void RequestOpenGLRenderer_3_2(bool request_3_2);
void SetOpenGLRendererFunctions(bool (*initFunction)(),
								bool (*beginOGLFunction)(),
								void (*endOGLFunction)(),
								bool (*resizeOGLFunction)(const bool isFBOSupported, size_t w, size_t h));

#ifdef __cplusplus
}
#endif
