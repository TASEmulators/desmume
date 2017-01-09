/*
	Copyright (C) 2011 Roger Manuel
	Copyright (C) 2011-2016 DeSmuME team

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
#undef BOOL

@class NSImage;
@class NSBitmapImageRep;

struct NDSFrameInfo;

@interface CocoaDSOutput : CocoaDSThread
{
	NSMutableDictionary *property;
	
	pthread_mutex_t *mutexConsume;
	pthread_rwlock_t *rwlockProducer;
}

@property (readonly) NSMutableDictionary *property;
@property (assign) pthread_rwlock_t *rwlockProducer;
@property (readonly) pthread_mutex_t *mutexConsume;

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
- (NSString *) audioOutputEngineString;
- (NSString *) spuInterpolationModeString;
- (NSString *) spuSyncMethodString;
- (void) handleSetVolume:(NSData *)volumeData;
- (void) handleSetAudioOutputEngine:(NSData *)methodIdData;
- (void) handleSetSpuAdvancedLogic:(NSData *)stateData;
- (void) handleSetSpuSyncMode:(NSData *)modeIdData;
- (void) handleSetSpuSyncMethod:(NSData *)methodIdData;
- (void) handleSetSpuInterpolationMode:(NSData *)modeIdData;

@end

@class CocoaVideoFilter;

@interface CocoaDSDisplay : CocoaDSOutput
{
	ClientDisplay3DView *_cdv;
	ClientDisplayViewProperties _intermediateViewProps;
	NSSize displaySize;
	
	uint32_t _receivedFrameIndex;
	uint32_t _currentReceivedFrameIndex;
	uint32_t _receivedFrameCount;
	
	uint32_t _cpuLoadAvgARM9;
	uint32_t _cpuLoadAvgARM7;
	
	OSSpinLock spinlockReceivedFrameIndex;
	OSSpinLock spinlockCPULoadAverage;
	OSSpinLock spinlockViewProperties;
}

@property (assign, nonatomic) ClientDisplay3DView *clientDisplayView;
@property (readonly) NSSize displaySize;

- (void) commitViewProperties:(const ClientDisplayViewProperties &)viewProps;

- (void) doReceiveGPUFrame;
- (void) handleReceiveGPUFrame;
- (void) handleChangeViewProperties;
- (void) handleRequestScreenshot:(NSData *)fileURLStringData fileTypeData:(NSData *)fileTypeData;
- (void) handleCopyToPasteboard;

- (void) finishFrame;
- (void) takeFrameCount;
- (void) setCPULoadAvgARM9:(uint32_t)loadAvgARM9 ARM7:(uint32_t)loadAvgARM7;
- (NSImage *) image;
- (NSBitmapImageRep *) bitmapImageRep;

@end

@interface CocoaDSDisplayVideo : CocoaDSDisplay
{
	void *_videoBuffer;
	void *_nativeBuffer[2];
	void *_customBuffer[2];
	
	OSSpinLock spinlockIsHUDVisible;
	OSSpinLock spinlockUseVerticalSync;
	OSSpinLock spinlockVideoFiltersPreferGPU;
	OSSpinLock spinlockOutputFilter;
	OSSpinLock spinlockSourceDeposterize;
	OSSpinLock spinlockPixelScaler;
}

@property (readonly) BOOL canFilterOnGPU;
@property (assign) BOOL isHUDVisible;
@property (assign) BOOL isHUDVideoFPSVisible;
@property (assign) BOOL isHUDRender3DFPSVisible;
@property (assign) BOOL isHUDFrameIndexVisible;
@property (assign) BOOL isHUDLagFrameCountVisible;
@property (assign) BOOL isHUDCPULoadAverageVisible;
@property (assign) BOOL isHUDRealTimeClockVisible;
@property (assign) BOOL useVerticalSync;
@property (assign) BOOL videoFiltersPreferGPU;
@property (assign) BOOL sourceDeposterize;
@property (assign) NSInteger outputFilter;
@property (assign) NSInteger pixelScaler;

- (void) handleReceiveGPUFrame;
- (void) handleRedrawView;
- (void) handleReloadAndRedraw;
- (void) handleReprocessAndRedraw;

- (void) resetVideoBuffers;
- (void) setScaleFactor:(float)theScaleFactor;

@end
