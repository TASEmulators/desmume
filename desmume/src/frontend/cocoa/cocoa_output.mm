/*
	Copyright (C) 2011 Roger Manuel
	Copyright (C) 2011-2022 DeSmuME team

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
	_unfairlockIdle = apple_unfairlock_create();
	
	return self;
}

- (void)dealloc
{
	[self exitThread];
	
	[property release];
	apple_unfairlock_destroy(_unfairlockIdle);
	
	[super dealloc];
}

- (void) setIdle:(BOOL)theState
{
	apple_unfairlock_lock(_unfairlockIdle);
	_idleState = theState;
	apple_unfairlock_unlock(_unfairlockIdle);
}

- (BOOL) idle
{
	apple_unfairlock_lock(_unfairlockIdle);
	const BOOL theState = _idleState;
	apple_unfairlock_unlock(_unfairlockIdle);
	
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
	
	_unfairlockAudioOutputEngine = apple_unfairlock_create();
	_unfairlockVolume = apple_unfairlock_create();
	_unfairlockSpuAdvancedLogic = apple_unfairlock_create();
	_unfairlockSpuInterpolationMode = apple_unfairlock_create();
	_unfairlockSpuSyncMode = apple_unfairlock_create();
	_unfairlockSpuSyncMethod = apple_unfairlock_create();
	
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

- (void)dealloc
{
	apple_unfairlock_destroy(_unfairlockAudioOutputEngine);
	apple_unfairlock_destroy(_unfairlockVolume);
	apple_unfairlock_destroy(_unfairlockSpuAdvancedLogic);
	apple_unfairlock_destroy(_unfairlockSpuInterpolationMode);
	apple_unfairlock_destroy(_unfairlockSpuSyncMode);
	apple_unfairlock_destroy(_unfairlockSpuSyncMethod);
	
	[super dealloc];
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
	
	apple_unfairlock_lock(_unfairlockVolume);
	[property setValue:[NSNumber numberWithFloat:vol] forKey:@"volume"];
	apple_unfairlock_unlock(_unfairlockVolume);
	
	SPU_SetVolume((int)vol);
}

- (float) volume
{
	apple_unfairlock_lock(_unfairlockVolume);
	float vol = [(NSNumber *)[property valueForKey:@"volume"] floatValue];
	apple_unfairlock_unlock(_unfairlockVolume);
	
	return vol;
}

- (void) setAudioOutputEngine:(NSInteger)methodID
{
	apple_unfairlock_lock(_unfairlockAudioOutputEngine);
	[property setValue:[NSNumber numberWithInteger:methodID] forKey:@"audioOutputEngine"];
	apple_unfairlock_unlock(_unfairlockAudioOutputEngine);
	
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
	apple_unfairlock_lock(_unfairlockAudioOutputEngine);
	NSInteger methodID = [(NSNumber *)[property valueForKey:@"audioOutputEngine"] integerValue];
	apple_unfairlock_unlock(_unfairlockAudioOutputEngine);
	
	return methodID;
}

- (void) setSpuAdvancedLogic:(BOOL)state
{
	apple_unfairlock_lock(_unfairlockSpuAdvancedLogic);
	[property setValue:[NSNumber numberWithBool:state] forKey:@"spuAdvancedLogic"];
	apple_unfairlock_unlock(_unfairlockSpuAdvancedLogic);
	
	pthread_rwlock_wrlock(self.rwlockProducer);
	CommonSettings.spu_advanced = state;
	pthread_rwlock_unlock(self.rwlockProducer);
}

- (BOOL) spuAdvancedLogic
{
	apple_unfairlock_lock(_unfairlockSpuAdvancedLogic);
	BOOL state = [(NSNumber *)[property valueForKey:@"spuAdvancedLogic"] boolValue];
	apple_unfairlock_unlock(_unfairlockSpuAdvancedLogic);
	
	return state;
}

- (void) setSpuInterpolationMode:(NSInteger)modeID
{
	apple_unfairlock_lock(_unfairlockSpuInterpolationMode);
	[property setValue:[NSNumber numberWithInteger:modeID] forKey:@"spuInterpolationMode"];
	apple_unfairlock_unlock(_unfairlockSpuInterpolationMode);
	
	pthread_rwlock_wrlock(self.rwlockProducer);
	CommonSettings.spuInterpolationMode = (SPUInterpolationMode)modeID;
	pthread_rwlock_unlock(self.rwlockProducer);
}

- (NSInteger) spuInterpolationMode
{
	apple_unfairlock_lock(_unfairlockSpuInterpolationMode);
	NSInteger modeID = [(NSNumber *)[property valueForKey:@"spuInterpolationMode"] integerValue];
	apple_unfairlock_unlock(_unfairlockSpuInterpolationMode);
	
	return modeID;
}

- (void) setSpuSyncMode:(NSInteger)modeID
{
	apple_unfairlock_lock(_unfairlockSpuSyncMode);
	[property setValue:[NSNumber numberWithInteger:modeID] forKey:@"spuSyncMode"];
	apple_unfairlock_unlock(_unfairlockSpuSyncMode);
	
	pthread_rwlock_wrlock(self.rwlockProducer);
	CommonSettings.SPU_sync_mode = (int)modeID;
	SPU_SetSynchMode(CommonSettings.SPU_sync_mode, CommonSettings.SPU_sync_method);
	pthread_rwlock_unlock(self.rwlockProducer);
}

- (NSInteger) spuSyncMode
{
	apple_unfairlock_lock(_unfairlockSpuSyncMode);
	NSInteger modeID = [(NSNumber *)[property valueForKey:@"spuSyncMode"] integerValue];
	apple_unfairlock_unlock(_unfairlockSpuSyncMode);
	
	return modeID;
}

- (void) setSpuSyncMethod:(NSInteger)methodID
{
	apple_unfairlock_lock(_unfairlockSpuSyncMethod);
	[property setValue:[NSNumber numberWithInteger:methodID] forKey:@"spuSyncMethod"];
	apple_unfairlock_unlock(_unfairlockSpuSyncMethod);
	
	pthread_rwlock_wrlock(self.rwlockProducer);
	CommonSettings.SPU_sync_method = (int)methodID;
	SPU_SetSynchMode(CommonSettings.SPU_sync_mode, CommonSettings.SPU_sync_method);
	pthread_rwlock_unlock(self.rwlockProducer);
}

- (NSInteger) spuSyncMethod
{
	apple_unfairlock_lock(_unfairlockSpuSyncMethod);
	NSInteger methodID = [(NSNumber *)[property valueForKey:@"spuSyncMethod"] integerValue];
	apple_unfairlock_unlock(_unfairlockSpuSyncMethod);
	
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
	
	_unfairlockReceivedFrameIndex = apple_unfairlock_create();
	_unfairlockNDSFrameInfo = apple_unfairlock_create();
	
	_ndsFrameInfo.clear();
	
	_receivedFrameIndex = 0;
	_currentReceivedFrameIndex = 0;
	_receivedFrameCount = 0;
	
	[self createThread];
	
	return self;
}

- (void)dealloc
{
	apple_unfairlock_destroy(_unfairlockReceivedFrameIndex);
	apple_unfairlock_destroy(_unfairlockNDSFrameInfo);
	
	[super dealloc];
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
	apple_unfairlock_lock(_unfairlockReceivedFrameIndex);
	_receivedFrameIndex++;
	apple_unfairlock_unlock(_unfairlockReceivedFrameIndex);
}

- (void) takeFrameCount
{
	apple_unfairlock_lock(_unfairlockReceivedFrameIndex);
	_receivedFrameCount = _receivedFrameIndex - _currentReceivedFrameIndex;
	_currentReceivedFrameIndex = _receivedFrameIndex;
	apple_unfairlock_unlock(_unfairlockReceivedFrameIndex);
}

- (void) setNDSFrameInfo:(const NDSFrameInfo &)ndsFrameInfo
{
	apple_unfairlock_lock(_unfairlockNDSFrameInfo);
	_ndsFrameInfo.copyFrom(ndsFrameInfo);
	apple_unfairlock_unlock(_unfairlockNDSFrameInfo);
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
	
	_unfairlockViewProperties = apple_unfairlock_create();
	_unfairlockIsHUDVisible = apple_unfairlock_create();
	_unfairlockUseVerticalSync = apple_unfairlock_create();
	_unfairlockVideoFiltersPreferGPU = apple_unfairlock_create();
	_unfairlockOutputFilter = apple_unfairlock_create();
	_unfairlockSourceDeposterize = apple_unfairlock_create();
	_unfairlockPixelScaler = apple_unfairlock_create();
	_unfairlockDisplayVideoSource = apple_unfairlock_create();
	_unfairlockDisplayID = apple_unfairlock_create();
		
	return self;
}

- (void)dealloc
{
	apple_unfairlock_destroy(_unfairlockViewProperties);
	apple_unfairlock_destroy(_unfairlockIsHUDVisible);
	apple_unfairlock_destroy(_unfairlockUseVerticalSync);
	apple_unfairlock_destroy(_unfairlockVideoFiltersPreferGPU);
	apple_unfairlock_destroy(_unfairlockOutputFilter);
	apple_unfairlock_destroy(_unfairlockSourceDeposterize);
	apple_unfairlock_destroy(_unfairlockPixelScaler);
	apple_unfairlock_destroy(_unfairlockDisplayVideoSource);
	apple_unfairlock_destroy(_unfairlockDisplayID);
	
	[super dealloc];
}

- (void) commitPresenterProperties:(const ClientDisplayPresenterProperties &)viewProps
{
	apple_unfairlock_lock(_unfairlockViewProperties);
	_intermediateViewProps = viewProps;
	apple_unfairlock_unlock(_unfairlockViewProperties);
	
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
	apple_unfairlock_lock(_unfairlockIsHUDVisible);
	_cdv->Get3DPresenter()->SetHUDVisibility((theState) ? true : false);
	apple_unfairlock_unlock(_unfairlockIsHUDVisible);
	
	_cdv->SetViewNeedsFlush();
}

- (BOOL) isHUDVisible
{
	apple_unfairlock_lock(_unfairlockIsHUDVisible);
	const BOOL theState = (_cdv->Get3DPresenter()->GetHUDVisibility()) ? YES : NO;
	apple_unfairlock_unlock(_unfairlockIsHUDVisible);
	
	return theState;
}

- (void) setIsHUDExecutionSpeedVisible:(BOOL)theState
{
	apple_unfairlock_lock(_unfairlockIsHUDVisible);
	_cdv->Get3DPresenter()->SetHUDShowExecutionSpeed((theState) ? true : false);
	apple_unfairlock_unlock(_unfairlockIsHUDVisible);
	
	_cdv->SetViewNeedsFlush();
}

- (BOOL) isHUDExecutionSpeedVisible
{
	apple_unfairlock_lock(_unfairlockIsHUDVisible);
	const BOOL theState = (_cdv->Get3DPresenter()->GetHUDShowExecutionSpeed()) ? YES : NO;
	apple_unfairlock_unlock(_unfairlockIsHUDVisible);
	
	return theState;
}

- (void) setIsHUDVideoFPSVisible:(BOOL)theState
{
	apple_unfairlock_lock(_unfairlockIsHUDVisible);
	_cdv->Get3DPresenter()->SetHUDShowVideoFPS((theState) ? true : false);
	apple_unfairlock_unlock(_unfairlockIsHUDVisible);
	
	_cdv->SetViewNeedsFlush();
}

- (BOOL) isHUDVideoFPSVisible
{
	apple_unfairlock_lock(_unfairlockIsHUDVisible);
	const BOOL theState = (_cdv->Get3DPresenter()->GetHUDShowVideoFPS()) ? YES : NO;
	apple_unfairlock_unlock(_unfairlockIsHUDVisible);
	
	return theState;
}

- (void) setIsHUDRender3DFPSVisible:(BOOL)theState
{
	apple_unfairlock_lock(_unfairlockIsHUDVisible);
	_cdv->Get3DPresenter()->SetHUDShowRender3DFPS((theState) ? true : false);
	apple_unfairlock_unlock(_unfairlockIsHUDVisible);
	
	_cdv->SetViewNeedsFlush();
}

- (BOOL) isHUDRender3DFPSVisible
{
	apple_unfairlock_lock(_unfairlockIsHUDVisible);
	const BOOL theState = (_cdv->Get3DPresenter()->GetHUDShowRender3DFPS()) ? YES : NO;
	apple_unfairlock_unlock(_unfairlockIsHUDVisible);
	
	return theState;
}

- (void) setIsHUDFrameIndexVisible:(BOOL)theState
{
	apple_unfairlock_lock(_unfairlockIsHUDVisible);
	_cdv->Get3DPresenter()->SetHUDShowFrameIndex((theState) ? true : false);
	apple_unfairlock_unlock(_unfairlockIsHUDVisible);
	
	_cdv->SetViewNeedsFlush();
}

- (BOOL) isHUDFrameIndexVisible
{
	apple_unfairlock_lock(_unfairlockIsHUDVisible);
	const BOOL theState = (_cdv->Get3DPresenter()->GetHUDShowFrameIndex()) ? YES : NO;
	apple_unfairlock_unlock(_unfairlockIsHUDVisible);
	
	return theState;
}

- (void) setIsHUDLagFrameCountVisible:(BOOL)theState
{
	apple_unfairlock_lock(_unfairlockIsHUDVisible);
	_cdv->Get3DPresenter()->SetHUDShowLagFrameCount((theState) ? true : false);
	apple_unfairlock_unlock(_unfairlockIsHUDVisible);
	
	_cdv->SetViewNeedsFlush();
}

- (BOOL) isHUDLagFrameCountVisible
{
	apple_unfairlock_lock(_unfairlockIsHUDVisible);
	const BOOL theState = (_cdv->Get3DPresenter()->GetHUDShowLagFrameCount()) ? YES : NO;
	apple_unfairlock_unlock(_unfairlockIsHUDVisible);
	
	return theState;
}

- (void) setIsHUDCPULoadAverageVisible:(BOOL)theState
{
	apple_unfairlock_lock(_unfairlockIsHUDVisible);
	_cdv->Get3DPresenter()->SetHUDShowCPULoadAverage((theState) ? true : false);
	apple_unfairlock_unlock(_unfairlockIsHUDVisible);
	
	_cdv->SetViewNeedsFlush();
}

- (BOOL) isHUDCPULoadAverageVisible
{
	apple_unfairlock_lock(_unfairlockIsHUDVisible);
	const BOOL theState = (_cdv->Get3DPresenter()->GetHUDShowCPULoadAverage()) ? YES : NO;
	apple_unfairlock_unlock(_unfairlockIsHUDVisible);
	
	return theState;
}

- (void) setIsHUDRealTimeClockVisible:(BOOL)theState
{
	apple_unfairlock_lock(_unfairlockIsHUDVisible);
	_cdv->Get3DPresenter()->SetHUDShowRTC((theState) ? true : false);
	apple_unfairlock_unlock(_unfairlockIsHUDVisible);
	
	_cdv->SetViewNeedsFlush();
}

- (BOOL) isHUDRealTimeClockVisible
{
	apple_unfairlock_lock(_unfairlockIsHUDVisible);
	const BOOL theState = (_cdv->Get3DPresenter()->GetHUDShowRTC()) ? YES : NO;
	apple_unfairlock_unlock(_unfairlockIsHUDVisible);
	
	return theState;
}

- (void) setIsHUDInputVisible:(BOOL)theState
{
	apple_unfairlock_lock(_unfairlockIsHUDVisible);
	_cdv->Get3DPresenter()->SetHUDShowInput((theState) ? true : false);
	apple_unfairlock_unlock(_unfairlockIsHUDVisible);
	
	_cdv->SetViewNeedsFlush();
}

- (BOOL) isHUDInputVisible
{
	apple_unfairlock_lock(_unfairlockIsHUDVisible);
	const BOOL theState = (_cdv->Get3DPresenter()->GetHUDShowInput()) ? YES : NO;
	apple_unfairlock_unlock(_unfairlockIsHUDVisible);
	
	return theState;
}

- (void) setHudColorExecutionSpeed:(uint32_t)theColor
{
	apple_unfairlock_lock(_unfairlockIsHUDVisible);
	_cdv->Get3DPresenter()->SetHUDColorExecutionSpeed(theColor);
	apple_unfairlock_unlock(_unfairlockIsHUDVisible);
	
	_cdv->SetViewNeedsFlush();
}

- (uint32_t) hudColorExecutionSpeed
{
	apple_unfairlock_lock(_unfairlockIsHUDVisible);
	const uint32_t color32 = _cdv->Get3DPresenter()->GetHUDColorExecutionSpeed();
	apple_unfairlock_unlock(_unfairlockIsHUDVisible);
	
	return color32;
}

- (void) setHudColorVideoFPS:(uint32_t)theColor
{
	apple_unfairlock_lock(_unfairlockIsHUDVisible);
	_cdv->Get3DPresenter()->SetHUDColorVideoFPS(theColor);
	apple_unfairlock_unlock(_unfairlockIsHUDVisible);
	
	_cdv->SetViewNeedsFlush();
}

- (uint32_t) hudColorVideoFPS
{
	apple_unfairlock_lock(_unfairlockIsHUDVisible);
	const uint32_t color32 = _cdv->Get3DPresenter()->GetHUDColorVideoFPS();
	apple_unfairlock_unlock(_unfairlockIsHUDVisible);
	
	return color32;
}

- (void) setHudColorRender3DFPS:(uint32_t)theColor
{
	apple_unfairlock_lock(_unfairlockIsHUDVisible);
	_cdv->Get3DPresenter()->SetHUDColorRender3DFPS(theColor);
	apple_unfairlock_unlock(_unfairlockIsHUDVisible);
	
	_cdv->SetViewNeedsFlush();
}

- (uint32_t) hudColorRender3DFPS
{
	apple_unfairlock_lock(_unfairlockIsHUDVisible);
	const uint32_t color32 = _cdv->Get3DPresenter()->GetHUDColorRender3DFPS();
	apple_unfairlock_unlock(_unfairlockIsHUDVisible);
	
	return color32;
}

- (void) setHudColorFrameIndex:(uint32_t)theColor
{
	apple_unfairlock_lock(_unfairlockIsHUDVisible);
	_cdv->Get3DPresenter()->SetHUDColorFrameIndex(theColor);
	apple_unfairlock_unlock(_unfairlockIsHUDVisible);
	
	_cdv->SetViewNeedsFlush();
}

- (uint32_t) hudColorFrameIndex
{
	apple_unfairlock_lock(_unfairlockIsHUDVisible);
	const uint32_t color32 = _cdv->Get3DPresenter()->GetHUDColorFrameIndex();
	apple_unfairlock_unlock(_unfairlockIsHUDVisible);
	
	return color32;
}

- (void) setHudColorLagFrameCount:(uint32_t)theColor
{
	apple_unfairlock_lock(_unfairlockIsHUDVisible);
	_cdv->Get3DPresenter()->SetHUDColorLagFrameCount(theColor);
	apple_unfairlock_unlock(_unfairlockIsHUDVisible);
	
	_cdv->SetViewNeedsFlush();
}

- (uint32_t) hudColorLagFrameCount
{
	apple_unfairlock_lock(_unfairlockIsHUDVisible);
	const uint32_t color32 = _cdv->Get3DPresenter()->GetHUDColorLagFrameCount();
	apple_unfairlock_unlock(_unfairlockIsHUDVisible);
	
	return color32;
}

- (void) setHudColorCPULoadAverage:(uint32_t)theColor
{
	apple_unfairlock_lock(_unfairlockIsHUDVisible);
	_cdv->Get3DPresenter()->SetHUDColorCPULoadAverage(theColor);
	apple_unfairlock_unlock(_unfairlockIsHUDVisible);
	
	_cdv->SetViewNeedsFlush();
}

- (uint32_t) hudColorCPULoadAverage
{
	apple_unfairlock_lock(_unfairlockIsHUDVisible);
	const uint32_t color32 = _cdv->Get3DPresenter()->GetHUDColorCPULoadAverage();
	apple_unfairlock_unlock(_unfairlockIsHUDVisible);
	
	return color32;
}

- (void) setHudColorRTC:(uint32_t)theColor
{
	apple_unfairlock_lock(_unfairlockIsHUDVisible);
	_cdv->Get3DPresenter()->SetHUDColorRTC(theColor);
	apple_unfairlock_unlock(_unfairlockIsHUDVisible);
	
	_cdv->SetViewNeedsFlush();
}

- (uint32_t) hudColorRTC
{
	apple_unfairlock_lock(_unfairlockIsHUDVisible);
	const uint32_t color32 = _cdv->Get3DPresenter()->GetHUDColorRTC();
	apple_unfairlock_unlock(_unfairlockIsHUDVisible);
	
	return color32;
}

- (void) setHudColorInputPendingAndApplied:(uint32_t)theColor
{
	apple_unfairlock_lock(_unfairlockIsHUDVisible);
	_cdv->Get3DPresenter()->SetHUDColorInputPendingAndApplied(theColor);
	apple_unfairlock_unlock(_unfairlockIsHUDVisible);
	
	_cdv->SetViewNeedsFlush();
}

- (uint32_t) hudColorInputPendingAndApplied
{
	apple_unfairlock_lock(_unfairlockIsHUDVisible);
	const uint32_t color32 = _cdv->Get3DPresenter()->GetHUDColorInputPendingAndApplied();
	apple_unfairlock_unlock(_unfairlockIsHUDVisible);
	
	return color32;
}

- (void) setHudColorInputAppliedOnly:(uint32_t)theColor
{
	apple_unfairlock_lock(_unfairlockIsHUDVisible);
	_cdv->Get3DPresenter()->SetHUDColorInputAppliedOnly(theColor);
	apple_unfairlock_unlock(_unfairlockIsHUDVisible);
	
	_cdv->SetViewNeedsFlush();
}

- (uint32_t) hudColorInputAppliedOnly
{
	apple_unfairlock_lock(_unfairlockIsHUDVisible);
	const uint32_t color32 = _cdv->Get3DPresenter()->GetHUDColorInputAppliedOnly();
	apple_unfairlock_unlock(_unfairlockIsHUDVisible);
	
	return color32;
}

- (void) setHudColorInputPendingOnly:(uint32_t)theColor
{
	apple_unfairlock_lock(_unfairlockIsHUDVisible);
	_cdv->Get3DPresenter()->SetHUDColorInputPendingOnly(theColor);
	apple_unfairlock_unlock(_unfairlockIsHUDVisible);
	
	_cdv->SetViewNeedsFlush();
}

- (uint32_t) hudColorInputPendingOnly
{
	apple_unfairlock_lock(_unfairlockIsHUDVisible);
	const uint32_t color32 = _cdv->Get3DPresenter()->GetHUDColorInputPendingOnly();
	apple_unfairlock_unlock(_unfairlockIsHUDVisible);
	
	return color32;
}

- (void) setDisplayMainVideoSource:(NSInteger)displaySourceID
{
	apple_unfairlock_lock(_unfairlockDisplayVideoSource);
	_cdv->Get3DPresenter()->SetDisplayVideoSource(NDSDisplayID_Main, (ClientDisplaySource)displaySourceID);
	apple_unfairlock_unlock(_unfairlockDisplayVideoSource);
}

- (NSInteger) displayMainVideoSource
{
	apple_unfairlock_lock(_unfairlockDisplayVideoSource);
	const NSInteger displayVideoSource = _cdv->Get3DPresenter()->GetDisplayVideoSource(NDSDisplayID_Main);
	apple_unfairlock_unlock(_unfairlockDisplayVideoSource);
	
	return displayVideoSource;
}

- (void) setDisplayTouchVideoSource:(NSInteger)displaySourceID
{
	apple_unfairlock_lock(_unfairlockDisplayVideoSource);
	_cdv->Get3DPresenter()->SetDisplayVideoSource(NDSDisplayID_Touch, (ClientDisplaySource)displaySourceID);
	apple_unfairlock_unlock(_unfairlockDisplayVideoSource);
}

- (NSInteger) displayTouchVideoSource
{
	apple_unfairlock_lock(_unfairlockDisplayVideoSource);
	const NSInteger displayVideoSource = _cdv->Get3DPresenter()->GetDisplayVideoSource(NDSDisplayID_Touch);
	apple_unfairlock_unlock(_unfairlockDisplayVideoSource);
	
	return displayVideoSource;
}

- (void) setCurrentDisplayID:(uint32_t)theDisplayID
{
	apple_unfairlock_lock(_unfairlockDisplayID);
	_cdv->SetDisplayViewID((int64_t)theDisplayID);
	apple_unfairlock_unlock(_unfairlockDisplayID);
}

- (uint32_t) currentDisplayID
{
	apple_unfairlock_lock(_unfairlockDisplayID);
	const uint32_t displayID = (uint32_t)_cdv->GetDisplayViewID();
	apple_unfairlock_unlock(_unfairlockDisplayID);
	
	return displayID;
}

- (void) setUseVerticalSync:(BOOL)theState
{
	apple_unfairlock_lock(_unfairlockUseVerticalSync);
	_cdv->SetUseVerticalSync((theState) ? true : false);
	apple_unfairlock_unlock(_unfairlockUseVerticalSync);
}

- (BOOL) useVerticalSync
{
	apple_unfairlock_lock(_unfairlockUseVerticalSync);
	const BOOL theState = (_cdv->GetUseVerticalSync()) ? YES : NO;
	apple_unfairlock_unlock(_unfairlockUseVerticalSync);
	
	return theState;
}

- (void) setVideoFiltersPreferGPU:(BOOL)theState
{
	apple_unfairlock_lock(_unfairlockVideoFiltersPreferGPU);
	_cdv->Get3DPresenter()->SetFiltersPreferGPU((theState) ? true : false);
	apple_unfairlock_unlock(_unfairlockVideoFiltersPreferGPU);
}

- (BOOL) videoFiltersPreferGPU
{
	apple_unfairlock_lock(_unfairlockVideoFiltersPreferGPU);
	const BOOL theState = (_cdv->Get3DPresenter()->GetFiltersPreferGPU()) ? YES : NO;
	apple_unfairlock_unlock(_unfairlockVideoFiltersPreferGPU);
	
	return theState;
}

- (void) setSourceDeposterize:(BOOL)theState
{
	apple_unfairlock_lock(_unfairlockSourceDeposterize);
	_cdv->Get3DPresenter()->SetSourceDeposterize((theState) ? true : false);
	apple_unfairlock_unlock(_unfairlockSourceDeposterize);
}

- (BOOL) sourceDeposterize
{
	apple_unfairlock_lock(_unfairlockSourceDeposterize);
	const BOOL theState = (_cdv->Get3DPresenter()->GetSourceDeposterize()) ? YES : NO;
	apple_unfairlock_unlock(_unfairlockSourceDeposterize);
	
	return theState;
}

- (void) setOutputFilter:(NSInteger)filterID
{
	apple_unfairlock_lock(_unfairlockOutputFilter);
	_cdv->Get3DPresenter()->SetOutputFilter((OutputFilterTypeID)filterID);
	apple_unfairlock_unlock(_unfairlockOutputFilter);
}

- (NSInteger) outputFilter
{
	apple_unfairlock_lock(_unfairlockOutputFilter);
	const NSInteger filterID = _cdv->Get3DPresenter()->GetOutputFilter();
	apple_unfairlock_unlock(_unfairlockOutputFilter);
	
	return filterID;
}

- (void) setPixelScaler:(NSInteger)filterID
{
	apple_unfairlock_lock(_unfairlockPixelScaler);
	_cdv->Get3DPresenter()->SetPixelScaler((VideoFilterTypeID)filterID);
	apple_unfairlock_unlock(_unfairlockPixelScaler);
}

- (NSInteger) pixelScaler
{
	apple_unfairlock_lock(_unfairlockPixelScaler);
	const NSInteger filterID = _cdv->Get3DPresenter()->GetPixelScaler();
	apple_unfairlock_unlock(_unfairlockPixelScaler);
	
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
	apple_unfairlock_lock(_unfairlockViewProperties);
	_cdv->Get3DPresenter()->CommitPresenterProperties(_intermediateViewProps);
	apple_unfairlock_unlock(_unfairlockViewProperties);
	
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
	[pboard declareTypes:[NSArray arrayWithObjects:PASTEBOARDTYPE_TIFF, nil] owner:self];
	[pboard setData:[screenshot TIFFRepresentationUsingCompression:NSTIFFCompressionLZW factor:1.0f] forType:PASTEBOARDTYPE_TIFF];
}

- (void) setScaleFactor:(float)theScaleFactor
{
	apple_unfairlock_lock(_unfairlockIsHUDVisible);
	_cdv->Get3DPresenter()->SetScaleFactor(theScaleFactor);
	apple_unfairlock_unlock(_unfairlockIsHUDVisible);
}

- (void) hudUpdate
{
	apple_unfairlock_lock(_unfairlockReceivedFrameIndex);
	ClientFrameInfo clientFrameInfo;
	clientFrameInfo.videoFPS = _receivedFrameCount;
	apple_unfairlock_unlock(_unfairlockReceivedFrameIndex);
	
	apple_unfairlock_lock(_unfairlockNDSFrameInfo);
	_cdv->Get3DPresenter()->SetHUDInfo(clientFrameInfo, _ndsFrameInfo);
	apple_unfairlock_unlock(_unfairlockNDSFrameInfo);
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
#if defined(MAC_OS_X_VERSION_10_6) && (MAC_OS_X_VERSION_MAX_ALLOWED >= MAC_OS_X_VERSION_10_6)
	if (kCFCoreFoundationVersionNumber >= kCFCoreFoundationVersionNumber10_6)
	{
		pthread_setname_np("EmulationOutput");
	}
#endif
	
	CocoaDSOutput *cdsDisplayOutput = (CocoaDSOutput *)arg;
	[cdsDisplayOutput runMessageLoop];
	
	return NULL;
}
