/*
	Copyright (C) 2011 Roger Manuel
	Copyright (C) 2011-2017 DeSmuME team

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

#include "../../NDSSystem.h"
#include "../../common.h"
#include "../../GPU.h"
#include "../../gfx3d.h"
#include "../../SPU.h"
#include "../../movie.h"
#include "../../metaspu/metaspu.h"
#include "../../rtc.h"

#import <Cocoa/Cocoa.h>

#undef BOOL


@implementation CocoaDSOutput

@synthesize property;
@synthesize mutexConsume;
@synthesize rwlockProducer;

- (id)init
{
	self = [super initWithAutoreleaseInterval:0.1];
	if (self == nil)
	{
		return self;
	}
		
	property = [[NSMutableDictionary alloc] init];
	[property setValue:[NSDate date] forKey:@"outputTime"];
	
	mutexConsume = (pthread_mutex_t *)malloc(sizeof(pthread_mutex_t));
	pthread_mutex_init(mutexConsume, NULL);
	
	return self;
}

- (void)dealloc
{
	// Force the thread to exit now so that we can safely release other resources.
	[self forceThreadExit];
	
	[property release];
	
	pthread_mutex_destroy(mutexConsume);
	free(mutexConsume);
	mutexConsume = NULL;
	
	[super dealloc];
}

- (void) doCoreEmuFrame
{
	[CocoaDSUtil messageSendOneWay:self.receivePort msgID:MESSAGE_EMU_FRAME_PROCESSED];
}

- (void)handlePortMessage:(NSPortMessage *)portMessage
{
	NSInteger message = (NSInteger)[portMessage msgid];
	
	switch (message)
	{
		case MESSAGE_EMU_FRAME_PROCESSED:
			[self handleEmuFrameProcessed];
			break;
			
		default:
			[super handlePortMessage:portMessage];
			break;
	}
}

- (void) handleEmuFrameProcessed
{
	// The base method does nothing.
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

- (void)dealloc
{
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
	
	rwlockAudioEmulateCore = self.rwlockProducer;
	
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

- (void)handlePortMessage:(NSPortMessage*)portMessage
{
	NSInteger message = (NSInteger)[portMessage msgid];
	NSArray *messageComponents = [portMessage components];
	
	switch (message)
	{
		case MESSAGE_SET_AUDIO_PROCESS_METHOD:
			[self handleSetAudioOutputEngine:[messageComponents objectAtIndex:0]];
			break;
			
		case MESSAGE_SET_SPU_ADVANCED_LOGIC:
			[self handleSetSpuAdvancedLogic:[messageComponents objectAtIndex:0]];
			break;

		case MESSAGE_SET_SPU_SYNC_MODE:
			[self handleSetSpuSyncMode:[messageComponents objectAtIndex:0]];
			break;
		
		case MESSAGE_SET_SPU_SYNC_METHOD:
			[self handleSetSpuSyncMethod:[messageComponents objectAtIndex:0]];
			break;
			
		case MESSAGE_SET_SPU_INTERPOLATION_MODE:
			[self handleSetSpuInterpolationMode:[messageComponents objectAtIndex:0]];
			break;
			
		case MESSAGE_SET_VOLUME:
			[self handleSetVolume:[messageComponents objectAtIndex:0]];
			break;
			
		default:
			[super handlePortMessage:portMessage];
			break;
	}
}

- (void) handleEmuFrameProcessed
{
	SPU_Emulate_user();
	
	[super handleEmuFrameProcessed];
}

- (void) handleSetVolume:(NSData *)volumeData
{
	const float vol = *(float *)[volumeData bytes];
	[self setVolume:vol];
}

- (void) handleSetAudioOutputEngine:(NSData *)methodIdData
{
	const NSInteger methodID = *(NSInteger *)[methodIdData bytes];
	[self setAudioOutputEngine:methodID];
}

- (void) handleSetSpuAdvancedLogic:(NSData *)stateData
{
	const BOOL theState = *(BOOL *)[stateData bytes];
	[self setSpuAdvancedLogic:theState];
}

- (void) handleSetSpuSyncMode:(NSData *)modeIdData
{
	const NSInteger modeID = *(NSInteger *)[modeIdData bytes];
	[self setSpuSyncMode:modeID];
}

- (void) handleSetSpuSyncMethod:(NSData *)methodIdData
{
	const NSInteger methodID = *(NSInteger *)[methodIdData bytes];
	[self setSpuSyncMethod:methodID];
}

- (void) handleSetSpuInterpolationMode:(NSData *)modeIdData
{
	const NSInteger modeID = *(NSInteger *)[modeIdData bytes];
	[self setSpuInterpolationMode:modeID];
}

@end

@implementation CocoaDSDisplay

@dynamic clientDisplayView;
@dynamic displaySize;

- (id)init
{
	self = [super init];
	if (self == nil)
	{
		return self;
	}
	
	spinlockReceivedFrameIndex = OS_SPINLOCK_INIT;
	spinlockCPULoadAverage = OS_SPINLOCK_INIT;
	spinlockViewProperties = OS_SPINLOCK_INIT;
	
	_cdv = NULL;
	
	_receivedFrameIndex = 0;
	_currentReceivedFrameIndex = 0;
	_receivedFrameCount = 0;
	
	_cpuLoadAvgARM9 = 0;
	_cpuLoadAvgARM7 = 0;
		
	return self;
}

- (void)dealloc
{
	[super dealloc];
}

- (void) setClientDisplayView:(ClientDisplay3DView *)clientDisplayView
{
	_cdv = clientDisplayView;
}

- (ClientDisplay3DView *) clientDisplayView
{
	return _cdv;
}

- (void) commitViewProperties:(const ClientDisplayViewProperties &)viewProps
{
	OSSpinLockLock(&spinlockViewProperties);
	_intermediateViewProps = viewProps;
	OSSpinLockUnlock(&spinlockViewProperties);
	
	[self handleChangeViewProperties];
}

- (NSSize) displaySize
{
	pthread_rwlock_rdlock(self.rwlockProducer);
	NSSize size = NSMakeSize((CGFloat)GPU->GetCustomFramebufferWidth(), (_cdv->GetMode() == ClientDisplayMode_Dual) ? (CGFloat)(GPU->GetCustomFramebufferHeight() * 2): (CGFloat)GPU->GetCustomFramebufferHeight());
	pthread_rwlock_unlock(self.rwlockProducer);
	
	return size;
}

- (void)handlePortMessage:(NSPortMessage *)portMessage
{
	NSInteger message = (NSInteger)[portMessage msgid];
	NSArray *messageComponents = [portMessage components];
	
	switch (message)
	{
		case MESSAGE_RECEIVE_GPU_FRAME:
			[self handleReceiveGPUFrame];
			break;
			
		case MESSAGE_CHANGE_VIEW_PROPERTIES:
			[self handleChangeViewProperties];
			break;
			
		case MESSAGE_REQUEST_SCREENSHOT:
			[self handleRequestScreenshot:[messageComponents objectAtIndex:0] fileTypeData:[messageComponents objectAtIndex:1]];
			break;
			
		case MESSAGE_COPY_TO_PASTEBOARD:
			[self handleCopyToPasteboard];
			break;
		
		default:
			[super handlePortMessage:portMessage];
			break;
	}
}

- (void) handleReceiveGPUFrame
{
	OSSpinLockLock(&spinlockReceivedFrameIndex);
	_receivedFrameIndex++;
	OSSpinLockUnlock(&spinlockReceivedFrameIndex);
}

- (void) handleChangeViewProperties
{
	OSSpinLockLock(&spinlockViewProperties);
	_cdv->CommitViewProperties(_intermediateViewProps);
	OSSpinLockUnlock(&spinlockViewProperties);
	
	_cdv->SetupViewProperties();
}

- (void) handleRequestScreenshot:(NSData *)fileURLStringData fileTypeData:(NSData *)fileTypeData
{
	NSString *fileURLString = [[NSString alloc] initWithData:fileURLStringData encoding:NSUTF8StringEncoding];
	NSURL *fileURL = [NSURL URLWithString:fileURLString];
	NSBitmapImageFileType fileType = *(NSBitmapImageFileType *)[fileTypeData bytes];	
	
	NSDictionary *userInfo = [[NSDictionary alloc] initWithObjectsAndKeys:
							  fileURL, @"fileURL", 
							  [NSNumber numberWithInteger:(NSInteger)fileType], @"fileType",
							  [self image], @"screenshotImage",
							  nil];
	
	[[NSNotificationCenter defaultCenter] postNotificationOnMainThreadName:@"org.desmume.DeSmuME.requestScreenshotDidFinish" object:self userInfo:userInfo];
	[userInfo release];
	
	[fileURLString release];
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

- (void) takeFrameCount
{
	OSSpinLockLock(&spinlockReceivedFrameIndex);
	_receivedFrameCount = _receivedFrameIndex - _currentReceivedFrameIndex;
	_currentReceivedFrameIndex = _receivedFrameIndex;
	OSSpinLockUnlock(&spinlockReceivedFrameIndex);
}

- (void) setCPULoadAvgARM9:(uint32_t)loadAvgARM9 ARM7:(uint32_t)loadAvgARM7
{
	OSSpinLockLock(&spinlockCPULoadAverage);
	_cpuLoadAvgARM9 = loadAvgARM9;
	_cpuLoadAvgARM7 = loadAvgARM7;
	OSSpinLockUnlock(&spinlockCPULoadAverage);
}

- (NSImage *) image
{
	NSImage *newImage = [[NSImage alloc] initWithSize:[self displaySize]];
	if (newImage == nil)
	{
		return newImage;
	}
	 
	// Render the frame in an NSBitmapImageRep
	NSBitmapImageRep *newImageRep = [self bitmapImageRep];
	if (newImageRep == nil)
	{
		[newImage release];
		newImage = nil;
		return newImage;
	}
	
	// Attach the rendered frame to the NSImageRep
	[newImage addRepresentation:newImageRep];
	
	return [newImage autorelease];
}

- (NSBitmapImageRep *) bitmapImageRep
{
	const NDSDisplayInfo &dispInfo = GPU->GetDisplayInfo();
	NSUInteger w = (NSUInteger)dispInfo.customWidth;
	NSUInteger h = (_cdv->GetMode() == ClientDisplayMode_Dual) ? (NSUInteger)(dispInfo.customHeight * 2) : (NSUInteger)dispInfo.customHeight;
	
	NSBitmapImageRep *imageRep = [[NSBitmapImageRep alloc] initWithBitmapDataPlanes:NULL
																		 pixelsWide:w
																		 pixelsHigh:h
																	  bitsPerSample:8
																	samplesPerPixel:4
																		   hasAlpha:YES
																		   isPlanar:NO
																	 colorSpaceName:NSCalibratedRGBColorSpace
																		bytesPerRow:w * 4
																	   bitsPerPixel:32];
	
	if(imageRep == nil)
	{
		return imageRep;
	}
	
	void *displayBuffer = dispInfo.masterCustomBuffer;
	uint32_t *bitmapData = (uint32_t *)[imageRep bitmapData];
	
	pthread_rwlock_rdlock(self.rwlockProducer);
	
	GPU->GetEngineMain()->ResolveToCustomFramebuffer();
	GPU->GetEngineSub()->ResolveToCustomFramebuffer();
	
	if (dispInfo.pixelBytes == 2)
	{
		ColorspaceConvertBuffer555To8888Opaque<false, true>((u16 *)displayBuffer, bitmapData, (w * h));
	}
	else if (dispInfo.pixelBytes == 4)
	{
		memcpy(bitmapData, displayBuffer, w * h * sizeof(uint32_t));
	}
	
	pthread_rwlock_unlock(self.rwlockProducer);
	
#ifdef MSB_FIRST
	uint32_t *bitmapDataEnd = bitmapData + (w * h);
	while (bitmapData < bitmapDataEnd)
	{
		*bitmapData++ = CFSwapInt32LittleToHost(*bitmapData);
	}
#endif
	
	return [imageRep autorelease];
}

@end

@implementation CocoaDSDisplayVideo

@dynamic canFilterOnGPU;
@dynamic isHUDVisible;
@dynamic isHUDVideoFPSVisible;
@dynamic isHUDRender3DFPSVisible;
@dynamic isHUDFrameIndexVisible;
@dynamic isHUDLagFrameCountVisible;
@dynamic isHUDCPULoadAverageVisible;
@dynamic isHUDRealTimeClockVisible;
@dynamic useVerticalSync;
@dynamic videoFiltersPreferGPU;
@dynamic sourceDeposterize;
@dynamic outputFilter;
@dynamic pixelScaler;

- (id)init
{
	self = [super init];
	if (self == nil)
	{
		return self;
	}
	
	spinlockIsHUDVisible = OS_SPINLOCK_INIT;
	spinlockUseVerticalSync = OS_SPINLOCK_INIT;
	spinlockVideoFiltersPreferGPU = OS_SPINLOCK_INIT;
	spinlockOutputFilter = OS_SPINLOCK_INIT;
	spinlockSourceDeposterize = OS_SPINLOCK_INIT;
	spinlockPixelScaler = OS_SPINLOCK_INIT;
		
	return self;
}

- (void)dealloc
{
	[super dealloc];
}

- (BOOL) canFilterOnGPU
{
	return (_cdv->CanFilterOnGPU()) ? YES : NO;
}

- (void) setIsHUDVisible:(BOOL)theState
{
	OSSpinLockLock(&spinlockIsHUDVisible);
	_cdv->SetHUDVisibility((theState) ? true : false);
	OSSpinLockUnlock(&spinlockIsHUDVisible);
}

- (BOOL) isHUDVisible
{
	OSSpinLockLock(&spinlockIsHUDVisible);
	const BOOL theState = (_cdv->GetHUDVisibility()) ? YES : NO;
	OSSpinLockUnlock(&spinlockIsHUDVisible);
	
	return theState;
}

- (void) setIsHUDVideoFPSVisible:(BOOL)theState
{
	OSSpinLockLock(&spinlockIsHUDVisible);
	_cdv->SetHUDShowVideoFPS((theState) ? true : false);
	OSSpinLockUnlock(&spinlockIsHUDVisible);
}

- (BOOL) isHUDVideoFPSVisible
{
	OSSpinLockLock(&spinlockIsHUDVisible);
	const BOOL theState = (_cdv->GetHUDShowVideoFPS()) ? YES : NO;
	OSSpinLockUnlock(&spinlockIsHUDVisible);
	
	return theState;
}

- (void) setIsHUDRender3DFPSVisible:(BOOL)theState
{
	OSSpinLockLock(&spinlockIsHUDVisible);
	_cdv->SetHUDShowRender3DFPS((theState) ? true : false);
	OSSpinLockUnlock(&spinlockIsHUDVisible);
}

- (BOOL) isHUDRender3DFPSVisible
{
	OSSpinLockLock(&spinlockIsHUDVisible);
	const BOOL theState = (_cdv->GetHUDShowRender3DFPS()) ? YES : NO;
	OSSpinLockUnlock(&spinlockIsHUDVisible);
	
	return theState;
}

- (void) setIsHUDFrameIndexVisible:(BOOL)theState
{
	OSSpinLockLock(&spinlockIsHUDVisible);
	_cdv->SetHUDShowFrameIndex((theState) ? true : false);
	OSSpinLockUnlock(&spinlockIsHUDVisible);
}

- (BOOL) isHUDFrameIndexVisible
{
	OSSpinLockLock(&spinlockIsHUDVisible);
	const BOOL theState = (_cdv->GetHUDShowFrameIndex()) ? YES : NO;
	OSSpinLockUnlock(&spinlockIsHUDVisible);
	
	return theState;
}

- (void) setIsHUDLagFrameCountVisible:(BOOL)theState
{
	OSSpinLockLock(&spinlockIsHUDVisible);
	_cdv->SetHUDShowLagFrameCount((theState) ? true : false);
	OSSpinLockUnlock(&spinlockIsHUDVisible);
}

- (BOOL) isHUDLagFrameCountVisible
{
	OSSpinLockLock(&spinlockIsHUDVisible);
	const BOOL theState = (_cdv->GetHUDShowLagFrameCount()) ? YES : NO;
	OSSpinLockUnlock(&spinlockIsHUDVisible);
	
	return theState;
}

- (void) setIsHUDCPULoadAverageVisible:(BOOL)theState
{
	OSSpinLockLock(&spinlockIsHUDVisible);
	_cdv->SetHUDShowCPULoadAverage((theState) ? true : false);
	OSSpinLockUnlock(&spinlockIsHUDVisible);
}

- (BOOL) isHUDCPULoadAverageVisible
{
	OSSpinLockLock(&spinlockIsHUDVisible);
	const BOOL theState = (_cdv->GetHUDShowCPULoadAverage()) ? YES : NO;
	OSSpinLockUnlock(&spinlockIsHUDVisible);
	
	return theState;
}

- (void) setIsHUDRealTimeClockVisible:(BOOL)theState
{
	OSSpinLockLock(&spinlockIsHUDVisible);
	_cdv->SetHUDShowRTC((theState) ? true : false);
	OSSpinLockUnlock(&spinlockIsHUDVisible);
}

- (BOOL) isHUDRealTimeClockVisible
{
	OSSpinLockLock(&spinlockIsHUDVisible);
	const BOOL theState = (_cdv->GetHUDShowRTC()) ? YES : NO;
	OSSpinLockUnlock(&spinlockIsHUDVisible);
	
	return theState;
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
	_cdv->SetFiltersPreferGPU((theState) ? true : false);
	OSSpinLockUnlock(&spinlockVideoFiltersPreferGPU);
}

- (BOOL) videoFiltersPreferGPU
{
	OSSpinLockLock(&spinlockVideoFiltersPreferGPU);
	const BOOL theState = (_cdv->GetFiltersPreferGPU()) ? YES : NO;
	OSSpinLockUnlock(&spinlockVideoFiltersPreferGPU);
	
	return theState;
}

- (void) setSourceDeposterize:(BOOL)theState
{
	OSSpinLockLock(&spinlockSourceDeposterize);
	_cdv->SetSourceDeposterize((theState) ? true : false);
	OSSpinLockUnlock(&spinlockSourceDeposterize);
}

- (BOOL) sourceDeposterize
{
	OSSpinLockLock(&spinlockSourceDeposterize);
	const BOOL theState = (_cdv->GetSourceDeposterize()) ? YES : NO;
	OSSpinLockUnlock(&spinlockSourceDeposterize);
	
	return theState;
}

- (void) setOutputFilter:(NSInteger)filterID
{
	OSSpinLockLock(&spinlockOutputFilter);
	_cdv->SetOutputFilter((OutputFilterTypeID)filterID);
	OSSpinLockUnlock(&spinlockOutputFilter);
}

- (NSInteger) outputFilter
{
	OSSpinLockLock(&spinlockOutputFilter);
	const NSInteger filterID = _cdv->GetOutputFilter();
	OSSpinLockUnlock(&spinlockOutputFilter);
	
	return filterID;
}

- (void) setPixelScaler:(NSInteger)filterID
{
	OSSpinLockLock(&spinlockPixelScaler);
	_cdv->SetPixelScaler((VideoFilterTypeID)filterID);
	OSSpinLockUnlock(&spinlockPixelScaler);
}

- (NSInteger) pixelScaler
{
	OSSpinLockLock(&spinlockPixelScaler);
	const NSInteger filterID = _cdv->GetPixelScaler();
	OSSpinLockUnlock(&spinlockPixelScaler);
	
	return filterID;
}

- (void)handlePortMessage:(NSPortMessage *)portMessage
{
	NSInteger message = (NSInteger)[portMessage msgid];
	
	switch (message)
	{
		case MESSAGE_RELOAD_REPROCESS_REDRAW:
			[self handleReloadReprocessRedraw];
			break;
			
		case MESSAGE_REPROCESS_AND_REDRAW:
			[self handleReprocessRedraw];
			break;
			
		case MESSAGE_REDRAW_VIEW:
			[self handleRedraw];
			break;
			
		default:
			[super handlePortMessage:portMessage];
			break;
	}
}

- (void) handleEmuFrameProcessed
{
	[super handleEmuFrameProcessed];
	
	NDSFrameInfo frameInfo;
	frameInfo.render3DFPS = GPU->GetFPSRender3D();
	frameInfo.frameIndex = currFrameCounter;
	frameInfo.lagFrameCount = TotalLagFrames;
	
	OSSpinLockLock(&spinlockReceivedFrameIndex);
	frameInfo.videoFPS = _receivedFrameCount;
	OSSpinLockUnlock(&spinlockReceivedFrameIndex);
	
	OSSpinLockLock(&spinlockCPULoadAverage);
	frameInfo.cpuLoadAvgARM9 = _cpuLoadAvgARM9;
	frameInfo.cpuLoadAvgARM7 = _cpuLoadAvgARM7;
	OSSpinLockUnlock(&spinlockCPULoadAverage);
	
	rtcGetTimeAsString(frameInfo.rtcString);
	
	_cdv->HandleEmulatorFrameEndEvent(frameInfo);
}

- (void) handleReceiveGPUFrame
{
	[super handleReceiveGPUFrame];
	
	_cdv->LoadDisplays();
	_cdv->ProcessDisplays();
}

- (void) handleReloadReprocessRedraw
{
	GPUClientFetchObject &fetchObjMutable = (GPUClientFetchObject &)_cdv->GetFetchObject();
	const u8 bufferIndex = fetchObjMutable.GetLastFetchIndex();
	
	fetchObjMutable.FetchFromBufferIndex(bufferIndex);
	_cdv->LoadDisplays();
	_cdv->ProcessDisplays();
	
	[self handleEmuFrameProcessed];
}

- (void) handleReprocessRedraw
{	
	_cdv->ProcessDisplays();
	_cdv->UpdateView();
}

- (void) handleRedraw
{
	_cdv->UpdateView();
}

- (void) setScaleFactor:(float)theScaleFactor
{
	OSSpinLockLock(&spinlockIsHUDVisible);
	_cdv->SetScaleFactor(theScaleFactor);
	OSSpinLockUnlock(&spinlockIsHUDVisible);
}

@end
