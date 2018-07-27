/*
	Copyright (C) 2011 Roger Manuel
	Copyright (C) 2011-2018 DeSmuME team

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
#include <pthread.h>
#include <libkern/OSAtomic.h>

#import "cocoa_util.h"
#include "ClientDisplayView.h"
#include "ClientExecutionControl.h"

#ifdef BOOL
#undef BOOL
#endif


class ClientAVCaptureObject;
@class NSImage;
@class NSBitmapImageRep;

@interface CocoaDSOutput : NSObject
{
	uint32_t _threadMessageID;
	pthread_mutex_t _mutexMessageLoop;
	pthread_cond_t _condSignalMessage;
	pthread_t _pthread;
	
	NSMutableDictionary *property;
	
	pthread_rwlock_t *rwlockProducer;
	
	BOOL _idleState;
	OSSpinLock spinlockIdle;
}

@property (readonly) NSMutableDictionary *property;
@property (assign) pthread_rwlock_t *rwlockProducer;
@property (assign) BOOL idle;

- (void) createThread;
- (void) exitThread;

- (void) runMessageLoop;
- (void) signalMessage:(int32_t)messageID;
- (void) handleSignalMessageID:(int32_t)messageID;
- (void) doCoreEmuFrame;
- (void) handleEmuFrameProcessed;

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

- (id) initWithVolume:(CGFloat)vol;

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
- (NSString *) audioOutputEngineString;
- (NSString *) spuInterpolationModeString;
- (NSString *) spuSyncMethodString;

@end

@class CocoaVideoFilter;

@interface CocoaDSDisplay : CocoaDSOutput
{
	uint32_t _receivedFrameIndex;
	uint32_t _currentReceivedFrameIndex;
	uint32_t _receivedFrameCount;
	
	NDSFrameInfo _ndsFrameInfo;
	
	OSSpinLock spinlockReceivedFrameIndex;
	OSSpinLock spinlockNDSFrameInfo;
}

- (void) handleReceiveGPUFrame;

- (void) takeFrameCount;
- (void) setNDSFrameInfo:(const NDSFrameInfo &)ndsFrameInfo;

@end

@interface CocoaDSVideoCapture : CocoaDSDisplay
{
	ClientDisplay3DPresenter *_cdp;
	ClientAVCaptureObject *avCaptureObject;
	uint32_t *_videoCaptureBuffer;
	
	pthread_mutex_t _mutexCaptureBuffer;
}

@property (assign, nonatomic, getter=clientDisplay3DPresenter, setter=setClientDisplay3DPresenter:) ClientDisplay3DPresenter *_cdp;
@property (assign, nonatomic) ClientAVCaptureObject *avCaptureObject;

- (void) handleReceiveGPUFrame;

@end

@interface CocoaDSDisplayVideo : CocoaDSDisplay
{
	ClientDisplay3DView *_cdv;
	ClientDisplayPresenterProperties _intermediateViewProps;
	
	OSSpinLock spinlockViewProperties;
	OSSpinLock spinlockIsHUDVisible;
	OSSpinLock spinlockUseVerticalSync;
	OSSpinLock spinlockVideoFiltersPreferGPU;
	OSSpinLock spinlockOutputFilter;
	OSSpinLock spinlockSourceDeposterize;
	OSSpinLock spinlockPixelScaler;
	OSSpinLock spinlockDisplayVideoSource;
	OSSpinLock spinlockDisplayID;
}

@property (assign, nonatomic, getter=clientDisplay3DView, setter=setClientDisplay3DView:) ClientDisplay3DView *_cdv;
@property (readonly, nonatomic) BOOL canFilterOnGPU;
@property (readonly, nonatomic) BOOL willFilterOnGPU;
@property (assign) BOOL isHUDVisible;
@property (assign) BOOL isHUDExecutionSpeedVisible;
@property (assign) BOOL isHUDVideoFPSVisible;
@property (assign) BOOL isHUDRender3DFPSVisible;
@property (assign) BOOL isHUDFrameIndexVisible;
@property (assign) BOOL isHUDLagFrameCountVisible;
@property (assign) BOOL isHUDCPULoadAverageVisible;
@property (assign) BOOL isHUDRealTimeClockVisible;
@property (assign) BOOL isHUDInputVisible;
@property (assign) uint32_t hudColorExecutionSpeed;
@property (assign) uint32_t hudColorVideoFPS;
@property (assign) uint32_t hudColorRender3DFPS;
@property (assign) uint32_t hudColorFrameIndex;
@property (assign) uint32_t hudColorLagFrameCount;
@property (assign) uint32_t hudColorCPULoadAverage;
@property (assign) uint32_t hudColorRTC;
@property (assign) uint32_t hudColorInputPendingAndApplied;
@property (assign) uint32_t hudColorInputAppliedOnly;
@property (assign) uint32_t hudColorInputPendingOnly;
@property (assign) NSInteger displayMainVideoSource;
@property (assign) NSInteger displayTouchVideoSource;
@property (assign) uint32_t currentDisplayID;
@property (assign) BOOL useVerticalSync;
@property (assign) BOOL videoFiltersPreferGPU;
@property (assign) BOOL sourceDeposterize;
@property (assign) NSInteger outputFilter;
@property (assign) NSInteger pixelScaler;

- (void) commitPresenterProperties:(const ClientDisplayPresenterProperties &)viewProps;

- (void) handleChangeViewProperties;
- (void) handleReceiveGPUFrame;
- (void) handleReloadReprocessRedraw;
- (void) handleReprocessRedraw;
- (void) handleRedraw;
- (void) handleCopyToPasteboard;

- (void) setScaleFactor:(float)theScaleFactor;
- (void) hudUpdate;
- (NSImage *) image;

@end

#ifdef __cplusplus
extern "C"
{
#endif

static void* RunOutputThread(void *arg);

#ifdef __cplusplus
}
#endif
