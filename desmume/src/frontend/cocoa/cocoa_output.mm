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

#import "cocoa_output.h"
#import "cocoa_globals.h"
#import "cocoa_videofilter.h"
#import "cocoa_util.h"
#import "cocoa_core.h"
#include "sndOSX.h"

#include "ClientAVCaptureObject.h"

#include "../../NDSSystem.h"
#include "../../common.h"
#include "../../GPU.h"
#include "../../gfx3d.h"
#include "../../SPU.h"
#include "../../movie.h"
#include "../../metaspu/metaspu.h"
#include "../../rtc.h"

#import <Cocoa/Cocoa.h>

#ifdef BOOL
#undef BOOL
#endif


@implementation CocoaDSOutput

@synthesize property;
@synthesize rwlockProducer;

- (id)init
{
	self = [super init];
	if (self == nil)
	{
		return self;
	}
		
	property = [[NSMutableDictionary alloc] init];
	[property setValue:[NSDate date] forKey:@"outputTime"];
	
	rwlockProducer = NULL;
	
	_threadMessageID = MESSAGE_NONE;
	_pthread = NULL;
	
	_idleState = NO;
	spinlockIdle = OS_SPINLOCK_INIT;
	
	return self;
}

- (void)dealloc
{
	[self exitThread];
	
	[property release];
	
	[super dealloc];
}

- (void) setIdle:(BOOL)theState
{
	OSSpinLockLock(&spinlockIdle);
	_idleState = theState;
	OSSpinLockUnlock(&spinlockIdle);
}

- (BOOL) idle
{
	OSSpinLockLock(&spinlockIdle);
	const BOOL theState = _idleState;
	OSSpinLockUnlock(&spinlockIdle);
	
	return theState;
}

- (void) createThread
{
	pthread_mutex_init(&_mutexMessageLoop, NULL);
	pthread_cond_init(&_condSignalMessage, NULL);
	
	pthread_attr_t threadAttr;
	pthread_attr_init(&threadAttr);
	pthread_attr_setschedpolicy(&threadAttr, SCHED_RR);
	
	struct sched_param sp;
	memset(&sp, 0, sizeof(struct sched_param));
	sp.sched_priority = 45;
	pthread_attr_setschedparam(&threadAttr, &sp);
	
	pthread_create(&_pthread, &threadAttr, &RunOutputThread, self);
	pthread_attr_destroy(&threadAttr);
}

- (void) exitThread
{
	pthread_mutex_lock(&_mutexMessageLoop);
	
	if (_pthread != NULL)
	{
		_threadMessageID = MESSAGE_NONE;
		pthread_mutex_unlock(&_mutexMessageLoop);
		
		pthread_cancel(_pthread);
		pthread_join(_pthread, NULL);
		_pthread = NULL;
		
		pthread_cond_destroy(&_condSignalMessage);
		pthread_mutex_destroy(&_mutexMessageLoop);
	}
	else
	{
		pthread_mutex_unlock(&_mutexMessageLoop);
	}
}

- (void) runMessageLoop
{
	pthread_mutex_lock(&_mutexMessageLoop);
	
	do
	{
		while (_threadMessageID == MESSAGE_NONE)
		{
			pthread_cond_wait(&_condSignalMessage, &_mutexMessageLoop);
		}
		
		[self handleSignalMessageID:_threadMessageID];
		_threadMessageID = MESSAGE_NONE;
		
	} while(true);
}

- (void) signalMessage:(int32_t)messageID
{
	pthread_mutex_lock(&_mutexMessageLoop);
	
	_threadMessageID = messageID;
	pthread_cond_signal(&_condSignalMessage);
	
	pthread_mutex_unlock(&_mutexMessageLoop);
}

- (void) handleSignalMessageID:(int32_t)messageID
{
	switch (messageID)
	{
		case MESSAGE_EMU_FRAME_PROCESSED:
			[self handleEmuFrameProcessed];
			break;
			
		default:
			break;
	}
}

- (void) handleEmuFrameProcessed
{
	// The base method does nothing.
}

- (void) doCoreEmuFrame
{
	[self signalMessage:MESSAGE_EMU_FRAME_PROCESSED];
}

@end

@implementation CocoaDSSpeaker

@synthesize bufferSize;

- (id)init
{
	return [self initWithVolume:MAX_VOLUME];
}

- (id) initWithVolume:(CGFloat)vol
{
	self = [super init];
	if (self == nil)
	{
		return self;
	}
	
	spinlockAudioOutputEngine = OS_SPINLOCK_INIT;
	spinlockVolume = OS_SPINLOCK_INIT;
	spinlockSpuAdvancedLogic = OS_SPINLOCK_INIT;
	spinlockSpuInterpolationMode = OS_SPINLOCK_INIT;
	spinlockSpuSyncMode = OS_SPINLOCK_INIT;
	spinlockSpuSyncMethod = OS_SPINLOCK_INIT;
	
	_idleState = YES;
	bufferSize = 0;
	
	// Set up properties.
	[property setValue:[NSNumber numberWithFloat:(float)vol] forKey:@"volume"];
	[property setValue:[NSNumber numberWithBool:NO] forKey:@"mute"];
	[property setValue:[NSNumber numberWithInteger:0] forKey:@"filter"];
	[property setValue:[NSNumber numberWithInteger:SNDCORE_DUMMY] forKey:@"audioOutputEngine"];
	[property setValue:[NSNumber numberWithBool:NO] forKey:@"spuAdvancedLogic"];
	[property setValue:[NSNumber numberWithInteger:SPUInterpolation_None] forKey:@"spuInterpolationMode"];
	[property setValue:[NSNumber numberWithInteger:SPU_SYNC_MODE_DUAL_SYNC_ASYNC] forKey:@"spuSyncMode"];
	[property setValue:[NSNumber numberWithInteger:SPU_SYNC_METHOD_N] forKey:@"spuSyncMethod"];
	
	return self;
}

- (void) setIdle:(BOOL)theState
{
	if (theState)
	{
		SNDOSXPauseAudio();
	}
	else
	{
		SNDOSXUnpauseAudio();
	}
	
	[super setIdle:theState];
}

- (void) setVolume:(float)vol
{
	if (vol < 0.0f)
	{
		vol = 0.0f;
	}
	else if (vol > MAX_VOLUME)
	{
		vol = MAX_VOLUME;
	}
	
	OSSpinLockLock(&spinlockVolume);
	[property setValue:[NSNumber numberWithFloat:vol] forKey:@"volume"];
	OSSpinLockUnlock(&spinlockVolume);
	
	SPU_SetVolume((int)vol);
}

- (float) volume
{
	OSSpinLockLock(&spinlockVolume);
	float vol = [(NSNumber *)[property valueForKey:@"volume"] floatValue];
	OSSpinLockUnlock(&spinlockVolume);
	
	return vol;
}

- (void) setAudioOutputEngine:(NSInteger)methodID
{
	OSSpinLockLock(&spinlockAudioOutputEngine);
	[property setValue:[NSNumber numberWithInteger:methodID] forKey:@"audioOutputEngine"];
	OSSpinLockUnlock(&spinlockAudioOutputEngine);
	
	pthread_rwlock_wrlock(self.rwlockProducer);
	
	NSInteger result = -1;
	
	if (methodID != SNDCORE_DUMMY)
	{
		result = SPU_ChangeSoundCore(methodID, (int)SPU_BUFFER_BYTES);
	}
	
	if(result == -1)
	{
		SPU_ChangeSoundCore(SNDCORE_DUMMY, 0);
	}
	
	pthread_rwlock_unlock(self.rwlockProducer);
	
	// Force the volume back to it's original setting.
	[self setVolume:[self volume]];
}

- (NSInteger) audioOutputEngine
{
	OSSpinLockLock(&spinlockAudioOutputEngine);
	NSInteger methodID = [(NSNumber *)[property valueForKey:@"audioOutputEngine"] integerValue];
	OSSpinLockUnlock(&spinlockAudioOutputEngine);
	
	return methodID;
}

- (void) setSpuAdvancedLogic:(BOOL)state
{
	OSSpinLockLock(&spinlockSpuAdvancedLogic);
	[property setValue:[NSNumber numberWithBool:state] forKey:@"spuAdvancedLogic"];
	OSSpinLockUnlock(&spinlockSpuAdvancedLogic);
	
	pthread_rwlock_wrlock(self.rwlockProducer);
	CommonSettings.spu_advanced = state;
	pthread_rwlock_unlock(self.rwlockProducer);
}

- (BOOL) spuAdvancedLogic
{
	OSSpinLockLock(&spinlockSpuAdvancedLogic);
	BOOL state = [(NSNumber *)[property valueForKey:@"spuAdvancedLogic"] boolValue];
	OSSpinLockUnlock(&spinlockSpuAdvancedLogic);
	
	return state;
}

- (void) setSpuInterpolationMode:(NSInteger)modeID
{
	OSSpinLockLock(&spinlockSpuInterpolationMode);
	[property setValue:[NSNumber numberWithInteger:modeID] forKey:@"spuInterpolationMode"];
	OSSpinLockUnlock(&spinlockSpuInterpolationMode);
	
	pthread_rwlock_wrlock(self.rwlockProducer);
	CommonSettings.spuInterpolationMode = (SPUInterpolationMode)modeID;
	pthread_rwlock_unlock(self.rwlockProducer);
}

- (NSInteger) spuInterpolationMode
{
	OSSpinLockLock(&spinlockSpuInterpolationMode);
	NSInteger modeID = [(NSNumber *)[property valueForKey:@"spuInterpolationMode"] integerValue];
	OSSpinLockUnlock(&spinlockSpuInterpolationMode);
	
	return modeID;
}

- (void) setSpuSyncMode:(NSInteger)modeID
{
	OSSpinLockLock(&spinlockSpuSyncMode);
	[property setValue:[NSNumber numberWithInteger:modeID] forKey:@"spuSyncMode"];
	OSSpinLockUnlock(&spinlockSpuSyncMode);
	
	pthread_rwlock_wrlock(self.rwlockProducer);
	CommonSettings.SPU_sync_mode = (int)modeID;
	SPU_SetSynchMode(CommonSettings.SPU_sync_mode, CommonSettings.SPU_sync_method);
	pthread_rwlock_unlock(self.rwlockProducer);
}

- (NSInteger) spuSyncMode
{
	OSSpinLockLock(&spinlockSpuSyncMode);
	NSInteger modeID = [(NSNumber *)[property valueForKey:@"spuSyncMode"] integerValue];
	OSSpinLockUnlock(&spinlockSpuSyncMode);
	
	return modeID;
}

- (void) setSpuSyncMethod:(NSInteger)methodID
{
	OSSpinLockLock(&spinlockSpuSyncMethod);
	[property setValue:[NSNumber numberWithInteger:methodID] forKey:@"spuSyncMethod"];
	OSSpinLockUnlock(&spinlockSpuSyncMethod);
	
	pthread_rwlock_wrlock(self.rwlockProducer);
	CommonSettings.SPU_sync_method = (int)methodID;
	SPU_SetSynchMode(CommonSettings.SPU_sync_mode, CommonSettings.SPU_sync_method);
	pthread_rwlock_unlock(self.rwlockProducer);
}

- (NSInteger) spuSyncMethod
{
	OSSpinLockLock(&spinlockSpuSyncMethod);
	NSInteger methodID = [(NSNumber *)[property valueForKey:@"spuSyncMethod"] integerValue];
	OSSpinLockUnlock(&spinlockSpuSyncMethod);
	
	return methodID;
}

- (BOOL) mute
{
	return [[property objectForKey:@"mute"] boolValue];
}

- (void) setMute:(BOOL)mute
{
	[property setValue:[NSNumber numberWithBool:mute] forKey:@"mute"];
	
	if (mute)
	{
		SPU_SetVolume(0);
	}
	else
	{
		SPU_SetVolume((int)[self volume]);
	}
}

- (NSInteger) filter
{
	return [[property objectForKey:@"filter"] integerValue];
}

- (void) setFilter:(NSInteger)filter
{
	[property setValue:[NSNumber numberWithInteger:filter] forKey:@"filter"];
}

- (NSString *) audioOutputEngineString
{
	NSString *theString = @"Uninitialized";
	
	pthread_rwlock_rdlock(self.rwlockProducer);
	
	SoundInterface_struct *soundCore = SPU_SoundCore();
	if(soundCore == NULL)
	{
		pthread_rwlock_unlock(self.rwlockProducer);
		return theString;
	}
	
	const char *theName = soundCore->Name;
	theString = [NSString stringWithCString:theName encoding:NSUTF8StringEncoding];
	
	pthread_rwlock_unlock(self.rwlockProducer);
	
	return theString;
}

- (NSString *) spuInterpolationModeString
{
	NSString *theString = @"Unknown";
	NSInteger theMode = [self spuInterpolationMode];
	
	switch (theMode)
	{
		case SPUInterpolation_None:
			theString = @"None";
			break;
			
		case SPUInterpolation_Linear:
			theString = @"Linear";
			break;
			
		case SPUInterpolation_Cosine:
			theString = @"Cosine";
			break;
			
		default:
			break;
	}
	
	return theString;
}

- (NSString *) spuSyncMethodString
{
	NSString *theString = @"Unknown";
	NSInteger theMode = [self spuSyncMode];
	NSInteger theMethod = [self spuSyncMethod];
	
	if (theMode == ESynchMode_DualSynchAsynch)
	{
		theString = @"Dual SPU Sync/Async";
	}
	else
	{
		switch (theMethod)
		{
			case ESynchMethod_N:
				theString = @"\"N\" Sync Method";
				break;
				
			case ESynchMethod_Z:
				theString = @"\"Z\" Sync Method";
				break;
				
			case ESynchMethod_P:
				theString = @"\"P\" Sync Method";
				break;
				
			default:
				break;
		}
	}
	
	return theString;
}

@end

@implementation CocoaDSDisplay

- (id)init
{
	self = [super init];
	if (self == nil)
	{
		return self;
	}
	
	spinlockReceivedFrameIndex = OS_SPINLOCK_INIT;
	spinlockNDSFrameInfo = OS_SPINLOCK_INIT;
	
	_ndsFrameInfo.clear();
	
	_receivedFrameIndex = 0;
	_currentReceivedFrameIndex = 0;
	_receivedFrameCount = 0;
	
	[self createThread];
	
	return self;
}

- (void) handleSignalMessageID:(int32_t)messageID
{
	switch (messageID)
	{
		case MESSAGE_RECEIVE_GPU_FRAME:
			[self handleReceiveGPUFrame];
			break;
			
		default:
			[super handleSignalMessageID:messageID];
			break;
	}
}

- (void) handleReceiveGPUFrame
{
	OSSpinLockLock(&spinlockReceivedFrameIndex);
	_receivedFrameIndex++;
	OSSpinLockUnlock(&spinlockReceivedFrameIndex);
}

- (void) takeFrameCount
{
	OSSpinLockLock(&spinlockReceivedFrameIndex);
	_receivedFrameCount = _receivedFrameIndex - _currentReceivedFrameIndex;
	_currentReceivedFrameIndex = _receivedFrameIndex;
	OSSpinLockUnlock(&spinlockReceivedFrameIndex);
}

- (void) setNDSFrameInfo:(const NDSFrameInfo &)ndsFrameInfo
{
	OSSpinLockLock(&spinlockNDSFrameInfo);
	_ndsFrameInfo.copyFrom(ndsFrameInfo);
	OSSpinLockUnlock(&spinlockNDSFrameInfo);
}

@end

@implementation CocoaDSVideoCapture

@synthesize _cdp;
@synthesize avCaptureObject;

- (id)init
{
	self = [super init];
	if (self == nil)
	{
		return self;
	}
	
	_cdp = NULL;
	avCaptureObject = NULL;
	_videoCaptureBuffer = NULL;
	
	pthread_mutex_init(&_mutexCaptureBuffer, NULL);
	
	return self;
}

- (void)dealloc
{
	pthread_mutex_lock(&_mutexCaptureBuffer);
	free_aligned(_videoCaptureBuffer);
	pthread_mutex_unlock(&_mutexCaptureBuffer);
	
	pthread_mutex_destroy(&_mutexCaptureBuffer);
	
	[super dealloc];
}

- (void) handleReceiveGPUFrame
{
	[super handleReceiveGPUFrame];
	
	if ( (_cdp == NULL) || (avCaptureObject == NULL) )
	{
		return;
	}
	
	const ClientDisplayPresenterProperties &cdpProperty = _cdp->GetPresenterProperties();
	
	_cdp->LoadDisplays();
	_cdp->ProcessDisplays();
	_cdp->UpdateLayout();
	
	pthread_mutex_lock(&_mutexCaptureBuffer);
	
	if (_videoCaptureBuffer == NULL)
	{
		_videoCaptureBuffer = (uint32_t *)malloc_alignedPage(cdpProperty.clientWidth * cdpProperty.clientHeight * sizeof(uint32_t));
	}
	
	_cdp->CopyFrameToBuffer(_videoCaptureBuffer);
	avCaptureObject->CaptureVideoFrame(_videoCaptureBuffer, (size_t)cdpProperty.clientWidth, (size_t)cdpProperty.clientHeight, NDSColorFormat_BGR888_Rev);
	avCaptureObject->StreamWriteStart();
	
	pthread_mutex_unlock(&_mutexCaptureBuffer);
}

@end

@implementation CocoaDSDisplayVideo

@synthesize _cdv;
@dynamic canFilterOnGPU;
@dynamic willFilterOnGPU;
@dynamic isHUDVisible;
@dynamic isHUDExecutionSpeedVisible;
@dynamic isHUDVideoFPSVisible;
@dynamic isHUDRender3DFPSVisible;
@dynamic isHUDFrameIndexVisible;
@dynamic isHUDLagFrameCountVisible;
@dynamic isHUDCPULoadAverageVisible;
@dynamic isHUDRealTimeClockVisible;
@dynamic isHUDInputVisible;
@dynamic hudColorExecutionSpeed;
@dynamic hudColorVideoFPS;
@dynamic hudColorRender3DFPS;
@dynamic hudColorFrameIndex;
@dynamic hudColorLagFrameCount;
@dynamic hudColorCPULoadAverage;
@dynamic hudColorRTC;
@dynamic hudColorInputPendingAndApplied;
@dynamic hudColorInputAppliedOnly;
@dynamic hudColorInputPendingOnly;
@dynamic currentDisplayID;
@dynamic useVerticalSync;
@dynamic videoFiltersPreferGPU;
@dynamic sourceDeposterize;
@dynamic outputFilter;
@dynamic pixelScaler;
@dynamic displayMainVideoSource;
@dynamic displayTouchVideoSource;

- (id)init
{
	self = [super init];
	if (self == nil)
	{
		return self;
	}
	
	_cdv = NULL;
	
	spinlockViewProperties = OS_SPINLOCK_INIT;
	spinlockIsHUDVisible = OS_SPINLOCK_INIT;
	spinlockUseVerticalSync = OS_SPINLOCK_INIT;
	spinlockVideoFiltersPreferGPU = OS_SPINLOCK_INIT;
	spinlockOutputFilter = OS_SPINLOCK_INIT;
	spinlockSourceDeposterize = OS_SPINLOCK_INIT;
	spinlockPixelScaler = OS_SPINLOCK_INIT;
	spinlockDisplayVideoSource = OS_SPINLOCK_INIT;
	spinlockDisplayID = OS_SPINLOCK_INIT;
		
	return self;
}

- (void) commitPresenterProperties:(const ClientDisplayPresenterProperties &)viewProps
{
	OSSpinLockLock(&spinlockViewProperties);
	_intermediateViewProps = viewProps;
	OSSpinLockUnlock(&spinlockViewProperties);
	
	[self handleChangeViewProperties];
}

- (BOOL) canFilterOnGPU
{
	return (_cdv->Get3DPresenter()->CanFilterOnGPU()) ? YES : NO;
}

- (BOOL) willFilterOnGPU
{
	return (_cdv->Get3DPresenter()->WillFilterOnGPU()) ? YES : NO;
}

- (void) setIsHUDVisible:(BOOL)theState
{
	OSSpinLockLock(&spinlockIsHUDVisible);
	_cdv->Get3DPresenter()->SetHUDVisibility((theState) ? true : false);
	OSSpinLockUnlock(&spinlockIsHUDVisible);
	
	_cdv->SetViewNeedsFlush();
}

- (BOOL) isHUDVisible
{
	OSSpinLockLock(&spinlockIsHUDVisible);
	const BOOL theState = (_cdv->Get3DPresenter()->GetHUDVisibility()) ? YES : NO;
	OSSpinLockUnlock(&spinlockIsHUDVisible);
	
	return theState;
}

- (void) setIsHUDExecutionSpeedVisible:(BOOL)theState
{
	OSSpinLockLock(&spinlockIsHUDVisible);
	_cdv->Get3DPresenter()->SetHUDShowExecutionSpeed((theState) ? true : false);
	OSSpinLockUnlock(&spinlockIsHUDVisible);
	
	_cdv->SetViewNeedsFlush();
}

- (BOOL) isHUDExecutionSpeedVisible
{
	OSSpinLockLock(&spinlockIsHUDVisible);
	const BOOL theState = (_cdv->Get3DPresenter()->GetHUDShowExecutionSpeed()) ? YES : NO;
	OSSpinLockUnlock(&spinlockIsHUDVisible);
	
	return theState;
}

- (void) setIsHUDVideoFPSVisible:(BOOL)theState
{
	OSSpinLockLock(&spinlockIsHUDVisible);
	_cdv->Get3DPresenter()->SetHUDShowVideoFPS((theState) ? true : false);
	OSSpinLockUnlock(&spinlockIsHUDVisible);
	
	_cdv->SetViewNeedsFlush();
}

- (BOOL) isHUDVideoFPSVisible
{
	OSSpinLockLock(&spinlockIsHUDVisible);
	const BOOL theState = (_cdv->Get3DPresenter()->GetHUDShowVideoFPS()) ? YES : NO;
	OSSpinLockUnlock(&spinlockIsHUDVisible);
	
	return theState;
}

- (void) setIsHUDRender3DFPSVisible:(BOOL)theState
{
	OSSpinLockLock(&spinlockIsHUDVisible);
	_cdv->Get3DPresenter()->SetHUDShowRender3DFPS((theState) ? true : false);
	OSSpinLockUnlock(&spinlockIsHUDVisible);
	
	_cdv->SetViewNeedsFlush();
}

- (BOOL) isHUDRender3DFPSVisible
{
	OSSpinLockLock(&spinlockIsHUDVisible);
	const BOOL theState = (_cdv->Get3DPresenter()->GetHUDShowRender3DFPS()) ? YES : NO;
	OSSpinLockUnlock(&spinlockIsHUDVisible);
	
	return theState;
}

- (void) setIsHUDFrameIndexVisible:(BOOL)theState
{
	OSSpinLockLock(&spinlockIsHUDVisible);
	_cdv->Get3DPresenter()->SetHUDShowFrameIndex((theState) ? true : false);
	OSSpinLockUnlock(&spinlockIsHUDVisible);
	
	_cdv->SetViewNeedsFlush();
}

- (BOOL) isHUDFrameIndexVisible
{
	OSSpinLockLock(&spinlockIsHUDVisible);
	const BOOL theState = (_cdv->Get3DPresenter()->GetHUDShowFrameIndex()) ? YES : NO;
	OSSpinLockUnlock(&spinlockIsHUDVisible);
	
	return theState;
}

- (void) setIsHUDLagFrameCountVisible:(BOOL)theState
{
	OSSpinLockLock(&spinlockIsHUDVisible);
	_cdv->Get3DPresenter()->SetHUDShowLagFrameCount((theState) ? true : false);
	OSSpinLockUnlock(&spinlockIsHUDVisible);
	
	_cdv->SetViewNeedsFlush();
}

- (BOOL) isHUDLagFrameCountVisible
{
	OSSpinLockLock(&spinlockIsHUDVisible);
	const BOOL theState = (_cdv->Get3DPresenter()->GetHUDShowLagFrameCount()) ? YES : NO;
	OSSpinLockUnlock(&spinlockIsHUDVisible);
	
	return theState;
}

- (void) setIsHUDCPULoadAverageVisible:(BOOL)theState
{
	OSSpinLockLock(&spinlockIsHUDVisible);
	_cdv->Get3DPresenter()->SetHUDShowCPULoadAverage((theState) ? true : false);
	OSSpinLockUnlock(&spinlockIsHUDVisible);
	
	_cdv->SetViewNeedsFlush();
}

- (BOOL) isHUDCPULoadAverageVisible
{
	OSSpinLockLock(&spinlockIsHUDVisible);
	const BOOL theState = (_cdv->Get3DPresenter()->GetHUDShowCPULoadAverage()) ? YES : NO;
	OSSpinLockUnlock(&spinlockIsHUDVisible);
	
	return theState;
}

- (void) setIsHUDRealTimeClockVisible:(BOOL)theState
{
	OSSpinLockLock(&spinlockIsHUDVisible);
	_cdv->Get3DPresenter()->SetHUDShowRTC((theState) ? true : false);
	OSSpinLockUnlock(&spinlockIsHUDVisible);
	
	_cdv->SetViewNeedsFlush();
}

- (BOOL) isHUDRealTimeClockVisible
{
	OSSpinLockLock(&spinlockIsHUDVisible);
	const BOOL theState = (_cdv->Get3DPresenter()->GetHUDShowRTC()) ? YES : NO;
	OSSpinLockUnlock(&spinlockIsHUDVisible);
	
	return theState;
}

- (void) setIsHUDInputVisible:(BOOL)theState
{
	OSSpinLockLock(&spinlockIsHUDVisible);
	_cdv->Get3DPresenter()->SetHUDShowInput((theState) ? true : false);
	OSSpinLockUnlock(&spinlockIsHUDVisible);
	
	_cdv->SetViewNeedsFlush();
}

- (BOOL) isHUDInputVisible
{
	OSSpinLockLock(&spinlockIsHUDVisible);
	const BOOL theState = (_cdv->Get3DPresenter()->GetHUDShowInput()) ? YES : NO;
	OSSpinLockUnlock(&spinlockIsHUDVisible);
	
	return theState;
}

- (void) setHudColorExecutionSpeed:(uint32_t)theColor
{
	OSSpinLockLock(&spinlockIsHUDVisible);
	_cdv->Get3DPresenter()->SetHUDColorExecutionSpeed(theColor);
	OSSpinLockUnlock(&spinlockIsHUDVisible);
	
	_cdv->SetViewNeedsFlush();
}

- (uint32_t) hudColorExecutionSpeed
{
	OSSpinLockLock(&spinlockIsHUDVisible);
	const uint32_t color32 = _cdv->Get3DPresenter()->GetHUDColorExecutionSpeed();
	OSSpinLockUnlock(&spinlockIsHUDVisible);
	
	return color32;
}

- (void) setHudColorVideoFPS:(uint32_t)theColor
{
	OSSpinLockLock(&spinlockIsHUDVisible);
	_cdv->Get3DPresenter()->SetHUDColorVideoFPS(theColor);
	OSSpinLockUnlock(&spinlockIsHUDVisible);
	
	_cdv->SetViewNeedsFlush();
}

- (uint32_t) hudColorVideoFPS
{
	OSSpinLockLock(&spinlockIsHUDVisible);
	const uint32_t color32 = _cdv->Get3DPresenter()->GetHUDColorVideoFPS();
	OSSpinLockUnlock(&spinlockIsHUDVisible);
	
	return color32;
}

- (void) setHudColorRender3DFPS:(uint32_t)theColor
{
	OSSpinLockLock(&spinlockIsHUDVisible);
	_cdv->Get3DPresenter()->SetHUDColorRender3DFPS(theColor);
	OSSpinLockUnlock(&spinlockIsHUDVisible);
	
	_cdv->SetViewNeedsFlush();
}

- (uint32_t) hudColorRender3DFPS
{
	OSSpinLockLock(&spinlockIsHUDVisible);
	const uint32_t color32 = _cdv->Get3DPresenter()->GetHUDColorRender3DFPS();
	OSSpinLockUnlock(&spinlockIsHUDVisible);
	
	return color32;
}

- (void) setHudColorFrameIndex:(uint32_t)theColor
{
	OSSpinLockLock(&spinlockIsHUDVisible);
	_cdv->Get3DPresenter()->SetHUDColorFrameIndex(theColor);
	OSSpinLockUnlock(&spinlockIsHUDVisible);
	
	_cdv->SetViewNeedsFlush();
}

- (uint32_t) hudColorFrameIndex
{
	OSSpinLockLock(&spinlockIsHUDVisible);
	const uint32_t color32 = _cdv->Get3DPresenter()->GetHUDColorFrameIndex();
	OSSpinLockUnlock(&spinlockIsHUDVisible);
	
	return color32;
}

- (void) setHudColorLagFrameCount:(uint32_t)theColor
{
	OSSpinLockLock(&spinlockIsHUDVisible);
	_cdv->Get3DPresenter()->SetHUDColorLagFrameCount(theColor);
	OSSpinLockUnlock(&spinlockIsHUDVisible);
	
	_cdv->SetViewNeedsFlush();
}

- (uint32_t) hudColorLagFrameCount
{
	OSSpinLockLock(&spinlockIsHUDVisible);
	const uint32_t color32 = _cdv->Get3DPresenter()->GetHUDColorLagFrameCount();
	OSSpinLockUnlock(&spinlockIsHUDVisible);
	
	return color32;
}

- (void) setHudColorCPULoadAverage:(uint32_t)theColor
{
	OSSpinLockLock(&spinlockIsHUDVisible);
	_cdv->Get3DPresenter()->SetHUDColorCPULoadAverage(theColor);
	OSSpinLockUnlock(&spinlockIsHUDVisible);
	
	_cdv->SetViewNeedsFlush();
}

- (uint32_t) hudColorCPULoadAverage
{
	OSSpinLockLock(&spinlockIsHUDVisible);
	const uint32_t color32 = _cdv->Get3DPresenter()->GetHUDColorCPULoadAverage();
	OSSpinLockUnlock(&spinlockIsHUDVisible);
	
	return color32;
}

- (void) setHudColorRTC:(uint32_t)theColor
{
	OSSpinLockLock(&spinlockIsHUDVisible);
	_cdv->Get3DPresenter()->SetHUDColorRTC(theColor);
	OSSpinLockUnlock(&spinlockIsHUDVisible);
	
	_cdv->SetViewNeedsFlush();
}

- (uint32_t) hudColorRTC
{
	OSSpinLockLock(&spinlockIsHUDVisible);
	const uint32_t color32 = _cdv->Get3DPresenter()->GetHUDColorRTC();
	OSSpinLockUnlock(&spinlockIsHUDVisible);
	
	return color32;
}

- (void) setHudColorInputPendingAndApplied:(uint32_t)theColor
{
	OSSpinLockLock(&spinlockIsHUDVisible);
	_cdv->Get3DPresenter()->SetHUDColorInputPendingAndApplied(theColor);
	OSSpinLockUnlock(&spinlockIsHUDVisible);
	
	_cdv->SetViewNeedsFlush();
}

- (uint32_t) hudColorInputPendingAndApplied
{
	OSSpinLockLock(&spinlockIsHUDVisible);
	const uint32_t color32 = _cdv->Get3DPresenter()->GetHUDColorInputPendingAndApplied();
	OSSpinLockUnlock(&spinlockIsHUDVisible);
	
	return color32;
}

- (void) setHudColorInputAppliedOnly:(uint32_t)theColor
{
	OSSpinLockLock(&spinlockIsHUDVisible);
	_cdv->Get3DPresenter()->SetHUDColorInputAppliedOnly(theColor);
	OSSpinLockUnlock(&spinlockIsHUDVisible);
	
	_cdv->SetViewNeedsFlush();
}

- (uint32_t) hudColorInputAppliedOnly
{
	OSSpinLockLock(&spinlockIsHUDVisible);
	const uint32_t color32 = _cdv->Get3DPresenter()->GetHUDColorInputAppliedOnly();
	OSSpinLockUnlock(&spinlockIsHUDVisible);
	
	return color32;
}

- (void) setHudColorInputPendingOnly:(uint32_t)theColor
{
	OSSpinLockLock(&spinlockIsHUDVisible);
	_cdv->Get3DPresenter()->SetHUDColorInputPendingOnly(theColor);
	OSSpinLockUnlock(&spinlockIsHUDVisible);
	
	_cdv->SetViewNeedsFlush();
}

- (uint32_t) hudColorInputPendingOnly
{
	OSSpinLockLock(&spinlockIsHUDVisible);
	const uint32_t color32 = _cdv->Get3DPresenter()->GetHUDColorInputPendingOnly();
	OSSpinLockUnlock(&spinlockIsHUDVisible);
	
	return color32;
}

- (void) setDisplayMainVideoSource:(NSInteger)displaySourceID
{
	OSSpinLockLock(&spinlockDisplayVideoSource);
	_cdv->Get3DPresenter()->SetDisplayVideoSource(NDSDisplayID_Main, (ClientDisplaySource)displaySourceID);
	OSSpinLockUnlock(&spinlockDisplayVideoSource);
}

- (NSInteger) displayMainVideoSource
{
	OSSpinLockLock(&spinlockDisplayVideoSource);
	const NSInteger displayVideoSource = _cdv->Get3DPresenter()->GetDisplayVideoSource(NDSDisplayID_Main);
	OSSpinLockUnlock(&spinlockDisplayVideoSource);
	
	return displayVideoSource;
}

- (void) setDisplayTouchVideoSource:(NSInteger)displaySourceID
{
	OSSpinLockLock(&spinlockDisplayVideoSource);
	_cdv->Get3DPresenter()->SetDisplayVideoSource(NDSDisplayID_Touch, (ClientDisplaySource)displaySourceID);
	OSSpinLockUnlock(&spinlockDisplayVideoSource);
}

- (NSInteger) displayTouchVideoSource
{
	OSSpinLockLock(&spinlockDisplayVideoSource);
	const NSInteger displayVideoSource = _cdv->Get3DPresenter()->GetDisplayVideoSource(NDSDisplayID_Touch);
	OSSpinLockUnlock(&spinlockDisplayVideoSource);
	
	return displayVideoSource;
}

- (void) setCurrentDisplayID:(uint32_t)theDisplayID
{
	OSSpinLockLock(&spinlockDisplayID);
	_cdv->SetDisplayViewID((int64_t)theDisplayID);
	OSSpinLockUnlock(&spinlockDisplayID);
}

- (uint32_t) currentDisplayID
{
	OSSpinLockLock(&spinlockDisplayID);
	const uint32_t displayID = (uint32_t)_cdv->GetDisplayViewID();
	OSSpinLockUnlock(&spinlockDisplayID);
	
	return displayID;
}

- (void) setUseVerticalSync:(BOOL)theState
{
	OSSpinLockLock(&spinlockUseVerticalSync);
	_cdv->SetUseVerticalSync((theState) ? true : false);
	OSSpinLockUnlock(&spinlockUseVerticalSync);
}

- (BOOL) useVerticalSync
{
	OSSpinLockLock(&spinlockUseVerticalSync);
	const BOOL theState = (_cdv->GetUseVerticalSync()) ? YES : NO;
	OSSpinLockUnlock(&spinlockUseVerticalSync);
	
	return theState;
}

- (void) setVideoFiltersPreferGPU:(BOOL)theState
{
	OSSpinLockLock(&spinlockVideoFiltersPreferGPU);
	_cdv->Get3DPresenter()->SetFiltersPreferGPU((theState) ? true : false);
	OSSpinLockUnlock(&spinlockVideoFiltersPreferGPU);
}

- (BOOL) videoFiltersPreferGPU
{
	OSSpinLockLock(&spinlockVideoFiltersPreferGPU);
	const BOOL theState = (_cdv->Get3DPresenter()->GetFiltersPreferGPU()) ? YES : NO;
	OSSpinLockUnlock(&spinlockVideoFiltersPreferGPU);
	
	return theState;
}

- (void) setSourceDeposterize:(BOOL)theState
{
	OSSpinLockLock(&spinlockSourceDeposterize);
	_cdv->Get3DPresenter()->SetSourceDeposterize((theState) ? true : false);
	OSSpinLockUnlock(&spinlockSourceDeposterize);
}

- (BOOL) sourceDeposterize
{
	OSSpinLockLock(&spinlockSourceDeposterize);
	const BOOL theState = (_cdv->Get3DPresenter()->GetSourceDeposterize()) ? YES : NO;
	OSSpinLockUnlock(&spinlockSourceDeposterize);
	
	return theState;
}

- (void) setOutputFilter:(NSInteger)filterID
{
	OSSpinLockLock(&spinlockOutputFilter);
	_cdv->Get3DPresenter()->SetOutputFilter((OutputFilterTypeID)filterID);
	OSSpinLockUnlock(&spinlockOutputFilter);
}

- (NSInteger) outputFilter
{
	OSSpinLockLock(&spinlockOutputFilter);
	const NSInteger filterID = _cdv->Get3DPresenter()->GetOutputFilter();
	OSSpinLockUnlock(&spinlockOutputFilter);
	
	return filterID;
}

- (void) setPixelScaler:(NSInteger)filterID
{
	OSSpinLockLock(&spinlockPixelScaler);
	_cdv->Get3DPresenter()->SetPixelScaler((VideoFilterTypeID)filterID);
	OSSpinLockUnlock(&spinlockPixelScaler);
}

- (NSInteger) pixelScaler
{
	OSSpinLockLock(&spinlockPixelScaler);
	const NSInteger filterID = _cdv->Get3DPresenter()->GetPixelScaler();
	OSSpinLockUnlock(&spinlockPixelScaler);
	
	return filterID;
}

- (void) handleSignalMessageID:(int32_t)messageID
{
	switch (messageID)
	{
		case MESSAGE_CHANGE_VIEW_PROPERTIES:
			[self handleChangeViewProperties];
			break;
			
		case MESSAGE_RELOAD_REPROCESS_REDRAW:
			[self handleReloadReprocessRedraw];
			break;
			
		case MESSAGE_REPROCESS_AND_REDRAW:
			[self handleReprocessRedraw];
			break;
			
		case MESSAGE_REDRAW_VIEW:
			[self handleRedraw];
			break;
			
		case MESSAGE_COPY_TO_PASTEBOARD:
			[self handleCopyToPasteboard];
			break;
			
		default:
			[super handleSignalMessageID:messageID];
			break;
	}
}

- (void) handleEmuFrameProcessed
{
	[super handleEmuFrameProcessed];
	[self hudUpdate];
	_cdv->SetViewNeedsFlush();
}

- (void) handleChangeViewProperties
{
	OSSpinLockLock(&spinlockViewProperties);
	_cdv->Get3DPresenter()->CommitPresenterProperties(_intermediateViewProps);
	OSSpinLockUnlock(&spinlockViewProperties);
	
	_cdv->Get3DPresenter()->SetupPresenterProperties();
	_cdv->SetViewNeedsFlush();
}

- (void) handleReceiveGPUFrame
{
	[super handleReceiveGPUFrame];
	
	_cdv->Get3DPresenter()->LoadDisplays();
	_cdv->Get3DPresenter()->ProcessDisplays();
}

- (void) handleReloadReprocessRedraw
{
	_cdv->Get3DPresenter()->LoadDisplays();
	_cdv->Get3DPresenter()->ProcessDisplays();
	
	[self handleEmuFrameProcessed];
}

- (void) handleReprocessRedraw
{	
	_cdv->Get3DPresenter()->ProcessDisplays();
	_cdv->SetViewNeedsFlush();
}

- (void) handleRedraw
{
	_cdv->SetViewNeedsFlush();
}

- (void) handleCopyToPasteboard
{
	NSImage *screenshot = [self image];
	if (screenshot == nil)
	{
		return;
	}
	
	NSPasteboard *pboard = [NSPasteboard generalPasteboard];
	[pboard declareTypes:[NSArray arrayWithObjects:NSTIFFPboardType, nil] owner:self];
	[pboard setData:[screenshot TIFFRepresentationUsingCompression:NSTIFFCompressionLZW factor:1.0f] forType:NSTIFFPboardType];
}

- (void) setScaleFactor:(float)theScaleFactor
{
	OSSpinLockLock(&spinlockIsHUDVisible);
	_cdv->Get3DPresenter()->SetScaleFactor(theScaleFactor);
	OSSpinLockUnlock(&spinlockIsHUDVisible);
}

- (void) hudUpdate
{
	OSSpinLockLock(&spinlockReceivedFrameIndex);
	ClientFrameInfo clientFrameInfo;
	clientFrameInfo.videoFPS = _receivedFrameCount;
	OSSpinLockUnlock(&spinlockReceivedFrameIndex);
	
	OSSpinLockLock(&spinlockNDSFrameInfo);
	_cdv->Get3DPresenter()->SetHUDInfo(clientFrameInfo, _ndsFrameInfo);
	OSSpinLockUnlock(&spinlockNDSFrameInfo);
}

- (NSImage *) image
{
	NSSize viewSize = NSMakeSize(_cdv->Get3DPresenter()->GetPresenterProperties().clientWidth, _cdv->Get3DPresenter()->GetPresenterProperties().clientHeight);
	NSUInteger w = viewSize.width;
	NSUInteger h = viewSize.height;
	
	NSImage *newImage = [[NSImage alloc] initWithSize:viewSize];
	if (newImage == nil)
	{
		return newImage;
	}
	
	// Render the frame in an NSBitmapImageRep
	NSBitmapImageRep *newImageRep = [[[NSBitmapImageRep alloc] initWithBitmapDataPlanes:NULL
																			 pixelsWide:w
																			 pixelsHigh:h
																		  bitsPerSample:8
																		samplesPerPixel:4
																			   hasAlpha:YES
																			   isPlanar:NO
																		 colorSpaceName:NSCalibratedRGBColorSpace
																			bytesPerRow:w * 4
																		   bitsPerPixel:32] autorelease];
	if (newImageRep == nil)
	{
		[newImage release];
		newImage = nil;
		return newImage;
	}
	
	_cdv->Get3DPresenter()->CopyFrameToBuffer((uint32_t *)[newImageRep bitmapData]);
	
	// Attach the rendered frame to the NSImageRep
	[newImage addRepresentation:newImageRep];
	
	return [newImage autorelease];
}

@end

static void* RunOutputThread(void *arg)
{
	CocoaDSOutput *cdsDisplayOutput = (CocoaDSOutput *)arg;
	[cdsDisplayOutput runMessageLoop];
	
	return NULL;
}
