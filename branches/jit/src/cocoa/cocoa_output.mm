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

#import "cocoa_output.h"
#import "cocoa_globals.h"
#import "cocoa_videofilter.h"
#import "cocoa_util.h"

#include <OpenGL/OpenGL.h>

#include "../NDSSystem.h"
#include "../GPU.h"
#include "../OGLRender.h"
#include "../rasterize.h"
#include "../SPU.h"
#include "../metaspu/metaspu.h"

#undef BOOL

GPU3DInterface *core3DList[] = {
	&gpu3DNull,
	&gpu3DRasterize,
	//&gpu3Dgl,
	NULL
};

@implementation CocoaDSOutput

@synthesize isStateChanged;
@synthesize frameCount;
@synthesize frameData;
@synthesize property;
@synthesize mutexProducer;
@synthesize mutexConsume;

- (id)init
{
	self = [super init];
	if (self == nil)
	{
		return self;
	}
	
	isStateChanged = NO;
	frameCount = 0;
	frameData = nil;
	
	property = [[NSMutableDictionary alloc] init];
	[property setValue:[NSDate date] forKey:@"outputTime"];
	
	mutexConsume = (pthread_mutex_t *)malloc(sizeof(pthread_mutex_t));
	pthread_mutex_init(mutexConsume, NULL);
	
	return self;
}

- (void)dealloc
{
	// Exit the thread.
	if (self.thread != nil)
	{
		self.threadExit = YES;
		
		// Wait until the thread has shut down.
		while (self.thread != nil)
		{
			[NSThread sleepUntilDate:[NSDate dateWithTimeIntervalSinceNow:0.01]];
		}
	}
	
	self.frameData = nil;
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
	NSArray *messageComponents = [portMessage components];
	
	switch (message)
	{
		case MESSAGE_EMU_FRAME_PROCESSED:
			[self handleEmuFrameProcessed:[messageComponents objectAtIndex:0]];
			break;
			
		default:
			[super handlePortMessage:portMessage];
			break;
	}
}

- (void) handleEmuFrameProcessed:(NSData *)theData
{
	self.frameCount++;
	self.frameData = theData;
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
	
	pthread_mutex_lock(self.mutexProducer);
	
	NSInteger result = -1;
	
	if (methodID != SNDCORE_DUMMY)
	{
		result = SPU_ChangeSoundCore(methodID, (int)SPU_BUFFER_BYTES);
	}
	
	if(result == -1)
	{
		SPU_ChangeSoundCore(SNDCORE_DUMMY, 0);
	}
	
	pthread_mutex_unlock(self.mutexProducer);
	
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
	
	pthread_mutex_lock(self.mutexProducer);
	CommonSettings.spu_advanced = state;
	pthread_mutex_unlock(self.mutexProducer);
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
	
	pthread_mutex_lock(self.mutexProducer);
	CommonSettings.spuInterpolationMode = (SPUInterpolationMode)modeID;
	pthread_mutex_unlock(self.mutexProducer);
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
	
	NSInteger methodID = [self spuSyncMethod];
	
	pthread_mutex_lock(self.mutexProducer);
	SPU_SetSynchMode(modeID, methodID);
	pthread_mutex_unlock(self.mutexProducer);
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
	
	NSInteger modeID = [self spuSyncMode];
	
	pthread_mutex_lock(self.mutexProducer);
	SPU_SetSynchMode(modeID, methodID);
	pthread_mutex_unlock(self.mutexProducer);
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

- (void)handlePortMessage:(NSPortMessage*)portMessage
{
	NSInteger message = (NSInteger)[portMessage msgid];
	NSArray *messageComponents = [portMessage components];
	
	switch (message)
	{
		case MESSAGE_EMU_FRAME_PROCESSED:
			[self handleEmuFrameProcessed:nil];
			break;
			
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

- (void) handleEmuFrameProcessed:(NSData *)theData
{
	SPU_Emulate_user();
	
	[super handleEmuFrameProcessed:theData];
}

- (void) handleSetVolume:(NSData *)volumeData
{
	const float *vol = (float *)[volumeData bytes];
	[self setVolume:*vol];
}

- (void) handleSetAudioOutputEngine:(NSData *)methodIdData
{
	const NSInteger *methodID = (NSInteger *)[methodIdData bytes];
	[self setAudioOutputEngine:*methodID];
}

- (void) handleSetSpuAdvancedLogic:(NSData *)stateData
{
	const BOOL *theState = (BOOL *)[stateData bytes];
	[self setSpuAdvancedLogic:*theState];
}

- (void) handleSetSpuSyncMode:(NSData *)modeIdData
{
	const NSInteger *modeID = (NSInteger *)[modeIdData bytes];
	[self setSpuSyncMode:*modeID];
}

- (void) handleSetSpuSyncMethod:(NSData *)methodIdData
{
	const NSInteger *methodID = (NSInteger *)[methodIdData bytes];
	[self setSpuSyncMethod:*methodID];
}

- (void) handleSetSpuInterpolationMode:(NSData *)modeIdData
{
	const NSInteger *modeID = (NSInteger *)[modeIdData bytes];
	[self setSpuInterpolationMode:*modeID];
}

@end

@implementation CocoaDSDisplay

@synthesize gpuStateFlags;
@synthesize delegate;
@synthesize displayType;
@synthesize vf;
@synthesize mutexRender3D;

- (id)init
{
	self = [super init];
	if (self == nil)
	{
		return self;
	}
	
	mutexRender3D = (pthread_mutex_t *)malloc(sizeof(pthread_mutex_t));
	pthread_mutex_init(mutexRender3D, NULL);
	
	spinlockDelegate = OS_SPINLOCK_INIT;
	spinlockGpuState = OS_SPINLOCK_INIT;
	spinlockDisplayType = OS_SPINLOCK_INIT;
	spinlockVideoFilterType = OS_SPINLOCK_INIT;
	spinlockVfSrcBuffer = OS_SPINLOCK_INIT;
	spinlockRender3DRenderingEngine = OS_SPINLOCK_INIT;
	spinlockRender3DHighPrecisionColorInterpolation = OS_SPINLOCK_INIT;
	spinlockRender3DEdgeMarking = OS_SPINLOCK_INIT;
	spinlockRender3DFog = OS_SPINLOCK_INIT;
	spinlockRender3DTextures = OS_SPINLOCK_INIT;
	spinlockRender3DDepthComparisonThreshold = OS_SPINLOCK_INIT;
	spinlockRender3DThreads = OS_SPINLOCK_INIT;
	spinlockRender3DLineHack = OS_SPINLOCK_INIT;
	
	displayType = DS_DISPLAY_TYPE_COMBO;
	vf = [[CocoaVideoFilter alloc] initWithSize:NSMakeSize(GPU_DISPLAY_WIDTH, GPU_DISPLAY_HEIGHT * 2)];
	
	gpuStateFlags =	GPUSTATE_MAIN_GPU_MASK |
	GPUSTATE_MAIN_BG0_MASK |
	GPUSTATE_MAIN_BG1_MASK |
	GPUSTATE_MAIN_BG2_MASK |
	GPUSTATE_MAIN_BG3_MASK |
	GPUSTATE_MAIN_OBJ_MASK |
	GPUSTATE_SUB_GPU_MASK |
	GPUSTATE_SUB_BG0_MASK |
	GPUSTATE_SUB_BG1_MASK |
	GPUSTATE_SUB_BG2_MASK |
	GPUSTATE_SUB_BG3_MASK |
	GPUSTATE_SUB_OBJ_MASK;
	
	frameCount = 0;
	frameData = nil;
	delegate = nil;
	
	[property setValue:[NSNumber numberWithBool:YES] forKey:@"gpuStateMainGPU"];
	[property setValue:[NSNumber numberWithBool:YES] forKey:@"gpuStateMainBG0"];
	[property setValue:[NSNumber numberWithBool:YES] forKey:@"gpuStateMainBG1"];
	[property setValue:[NSNumber numberWithBool:YES] forKey:@"gpuStateMainBG2"];
	[property setValue:[NSNumber numberWithBool:YES] forKey:@"gpuStateMainBG3"];
	[property setValue:[NSNumber numberWithBool:YES] forKey:@"gpuStateMainOBJ"];
	[property setValue:[NSNumber numberWithBool:YES] forKey:@"gpuStateSubGPU"];
	[property setValue:[NSNumber numberWithBool:YES] forKey:@"gpuStateSubBG0"];
	[property setValue:[NSNumber numberWithBool:YES] forKey:@"gpuStateSubBG1"];
	[property setValue:[NSNumber numberWithBool:YES] forKey:@"gpuStateSubBG2"];
	[property setValue:[NSNumber numberWithBool:YES] forKey:@"gpuStateSubBG3"];
	[property setValue:[NSNumber numberWithBool:YES] forKey:@"gpuStateSubOBJ"];
	[property setValue:[NSNumber numberWithInteger:displayType] forKey:@"displayMode"];
	[property setValue:NSSTRING_DISPLAYMODE_MAIN forKey:@"displayModeString"];
	[property setValue:[NSNumber numberWithInteger:(NSInteger)VideoFilterTypeID_None] forKey:@"videoFilterType"];
	[property setValue:[CocoaVideoFilter typeStringByID:VideoFilterTypeID_None] forKey:@"videoFilterTypeString"];
	[property setValue:[NSNumber numberWithInteger:CORE3DLIST_NULL] forKey:@"render3DRenderingEngine"];
	[property setValue:[NSNumber numberWithBool:YES] forKey:@"render3DHighPrecisionColorInterpolation"];
	[property setValue:[NSNumber numberWithBool:YES] forKey:@"render3DEdgeMarking"];
	[property setValue:[NSNumber numberWithBool:YES] forKey:@"render3DFog"];
	[property setValue:[NSNumber numberWithBool:YES] forKey:@"render3DTextures"];
	[property setValue:[NSNumber numberWithInteger:0] forKey:@"render3DDepthComparisonThreshold"];
	[property setValue:[NSNumber numberWithInteger:0] forKey:@"render3DThreads"];
	[property setValue:[NSNumber numberWithBool:YES] forKey:@"render3DLineHack"];
	
	return self;
}

- (void)dealloc
{
	self.delegate = nil;
	[vf release];
	
	pthread_mutex_destroy(self.mutexRender3D);
	free(self.mutexRender3D);
	mutexRender3D = nil;
			
	[super dealloc];
}

- (void) setDelegate:(id <CocoaDSDisplayDelegate>)theDelegate
{
	OSSpinLockLock(&spinlockDelegate);
	
	if (theDelegate == delegate)
	{
		OSSpinLockUnlock(&spinlockDelegate);
		return;
	}
	
	if (theDelegate != nil)
	{
		[theDelegate retain];
		[theDelegate setSendPortDisplay:self.receivePort];
	}
	
	[delegate release];
	delegate = theDelegate;
	
	OSSpinLockUnlock(&spinlockDelegate);
}

- (id <CocoaDSDisplayDelegate>) delegate
{
	OSSpinLockLock(&spinlockDelegate);
	id <CocoaDSDisplayDelegate> theDelegate = delegate;
	OSSpinLockUnlock(&spinlockDelegate);
	
	return theDelegate;
}

- (void) setGpuStateFlags:(UInt32)flags
{
	OSSpinLockLock(&spinlockGpuState);
	gpuStateFlags = flags;
	OSSpinLockUnlock(&spinlockGpuState);
	
	pthread_mutex_lock(self.mutexProducer);
	
	if (flags & GPUSTATE_MAIN_GPU_MASK)
	{
		SetGPUDisplayState(DS_GPU_TYPE_MAIN, true);
		[property setValue:[NSNumber numberWithBool:YES] forKey:@"gpuStateMainGPU"];
	}
	else
	{
		SetGPUDisplayState(DS_GPU_TYPE_MAIN, false);
		[property setValue:[NSNumber numberWithBool:NO] forKey:@"gpuStateMainGPU"];
	}
	
	if (flags & GPUSTATE_MAIN_BG0_MASK)
	{
		SetGPULayerState(DS_GPU_TYPE_MAIN, 0, true);
		[property setValue:[NSNumber numberWithBool:YES] forKey:@"gpuStateMainBG0"];
	}
	else
	{
		SetGPULayerState(DS_GPU_TYPE_MAIN, 0, false);
		[property setValue:[NSNumber numberWithBool:NO] forKey:@"gpuStateMainBG0"];
	}
	
	if (flags & GPUSTATE_MAIN_BG1_MASK)
	{
		SetGPULayerState(DS_GPU_TYPE_MAIN, 1, true);
		[property setValue:[NSNumber numberWithBool:YES] forKey:@"gpuStateMainBG1"];
	}
	else
	{
		SetGPULayerState(DS_GPU_TYPE_MAIN, 1, false);
		[property setValue:[NSNumber numberWithBool:NO] forKey:@"gpuStateMainBG1"];
	}
	
	if (flags & GPUSTATE_MAIN_BG2_MASK)
	{
		SetGPULayerState(DS_GPU_TYPE_MAIN, 2, true);
		[property setValue:[NSNumber numberWithBool:YES] forKey:@"gpuStateMainBG2"];
	}
	else
	{
		SetGPULayerState(DS_GPU_TYPE_MAIN, 2, false);
		[property setValue:[NSNumber numberWithBool:NO] forKey:@"gpuStateMainBG2"];
	}
	
	if (flags & GPUSTATE_MAIN_BG3_MASK)
	{
		SetGPULayerState(DS_GPU_TYPE_MAIN, 3, true);
		[property setValue:[NSNumber numberWithBool:YES] forKey:@"gpuStateMainBG3"];
	}
	else
	{
		SetGPULayerState(DS_GPU_TYPE_MAIN, 3, false);
		[property setValue:[NSNumber numberWithBool:NO] forKey:@"gpuStateMainBG3"];
	}
	
	if (flags & GPUSTATE_MAIN_OBJ_MASK)
	{
		SetGPULayerState(DS_GPU_TYPE_MAIN, 4, true);
		[property setValue:[NSNumber numberWithBool:YES] forKey:@"gpuStateMainOBJ"];
	}
	else
	{
		SetGPULayerState(DS_GPU_TYPE_MAIN, 4, false);
		[property setValue:[NSNumber numberWithBool:NO] forKey:@"gpuStateMainOBJ"];
	}
	
	if (flags & GPUSTATE_SUB_GPU_MASK)
	{
		SetGPUDisplayState(DS_GPU_TYPE_SUB, true);
		[property setValue:[NSNumber numberWithBool:YES] forKey:@"gpuStateSubGPU"];
	}
	else
	{
		SetGPUDisplayState(DS_GPU_TYPE_SUB, false);
		[property setValue:[NSNumber numberWithBool:NO] forKey:@"gpuStateSubGPU"];
	}
	
	if (flags & GPUSTATE_SUB_BG0_MASK)
	{
		SetGPULayerState(DS_GPU_TYPE_SUB, 0, true);
		[property setValue:[NSNumber numberWithBool:YES] forKey:@"gpuStateSubBG0"];
	}
	else
	{
		SetGPULayerState(DS_GPU_TYPE_SUB, 0, false);
		[property setValue:[NSNumber numberWithBool:NO] forKey:@"gpuStateSubBG0"];
	}
	
	if (flags & GPUSTATE_SUB_BG1_MASK)
	{
		SetGPULayerState(DS_GPU_TYPE_SUB, 1, true);
		[property setValue:[NSNumber numberWithBool:YES] forKey:@"gpuStateSubBG1"];
	}
	else
	{
		SetGPULayerState(DS_GPU_TYPE_SUB, 1, false);
		[property setValue:[NSNumber numberWithBool:NO] forKey:@"gpuStateSubBG1"];
	}
	
	if (flags & GPUSTATE_SUB_BG2_MASK)
	{
		SetGPULayerState(DS_GPU_TYPE_SUB, 2, true);
		[property setValue:[NSNumber numberWithBool:YES] forKey:@"gpuStateSubBG2"];
	}
	else
	{
		SetGPULayerState(DS_GPU_TYPE_SUB, 2, false);
		[property setValue:[NSNumber numberWithBool:NO] forKey:@"gpuStateSubBG2"];
	}
	
	if (flags & GPUSTATE_SUB_BG3_MASK)
	{
		SetGPULayerState(DS_GPU_TYPE_SUB, 3, true);
		[property setValue:[NSNumber numberWithBool:YES] forKey:@"gpuStateSubBG3"];
	}
	else
	{
		SetGPULayerState(DS_GPU_TYPE_SUB, 3, false);
		[property setValue:[NSNumber numberWithBool:NO] forKey:@"gpuStateSubBG3"];
	}
	
	if (flags & GPUSTATE_SUB_OBJ_MASK)
	{
		SetGPULayerState(DS_GPU_TYPE_SUB, 4, true);
		[property setValue:[NSNumber numberWithBool:YES] forKey:@"gpuStateSubOBJ"];
	}
	else
	{
		SetGPULayerState(DS_GPU_TYPE_SUB, 4, false);
		[property setValue:[NSNumber numberWithBool:NO] forKey:@"gpuStateSubOBJ"];
	}
	
	pthread_mutex_unlock(self.mutexProducer);
}

- (UInt32) gpuStateFlags
{
	OSSpinLockLock(&spinlockGpuState);
	UInt32 flags = gpuStateFlags;
	OSSpinLockUnlock(&spinlockGpuState);
	
	return flags;
}

- (void) setDisplayType:(NSInteger)dispType
{
	NSString *newDispString = nil;
	NSSize newSrcSize = NSMakeSize((CGFloat)GPU_DISPLAY_WIDTH, (CGFloat)GPU_DISPLAY_HEIGHT);
	
	switch (dispType)
	{
		case DS_DISPLAY_TYPE_MAIN:
			newDispString = NSSTRING_DISPLAYMODE_MAIN;
			break;
			
		case DS_DISPLAY_TYPE_TOUCH:
			newDispString = NSSTRING_DISPLAYMODE_TOUCH;
			break;
			
		case DS_DISPLAY_TYPE_COMBO:
			newDispString = NSSTRING_DISPLAYMODE_COMBO;
			newSrcSize.height *= 2;
			break;
			
		default:
			return;
			break;
	}
	
	OSSpinLockLock(&spinlockDisplayType);
	displayType = dispType;
	[property setValue:[NSNumber numberWithInteger:dispType] forKey:@"displayMode"];
	[property setValue:newDispString forKey:@"displayModeString"];
	OSSpinLockUnlock(&spinlockDisplayType);
	
	OSSpinLockLock(&spinlockVfSrcBuffer);
	[vf setSourceSize:newSrcSize];
	OSSpinLockUnlock(&spinlockVfSrcBuffer);
}

- (NSInteger) displayType
{
	OSSpinLockLock(&spinlockDisplayType);
	NSInteger dispType = displayType;
	OSSpinLockUnlock(&spinlockDisplayType);
	
	return dispType;
}

- (void) setVfType:(NSInteger)videoFilterTypeID
{
	[vf changeFilter:(VideoFilterTypeID)videoFilterTypeID];
	
	OSSpinLockLock(&spinlockVideoFilterType);
	[property setValue:[NSNumber numberWithInteger:videoFilterTypeID] forKey:@"videoFilterType"];
	[property setValue:NSLocalizedString([vf typeString], nil) forKey:@"videoFilterTypeString"];
	OSSpinLockUnlock(&spinlockVideoFilterType);
}

- (NSInteger) vfType
{
	OSSpinLockLock(&spinlockVideoFilterType);
	NSInteger theType = [(NSNumber *)[property valueForKey:@"videoFilterType"] integerValue];
	OSSpinLockUnlock(&spinlockVideoFilterType);
	
	return theType;
}

- (void) setRender3DRenderingEngine:(NSInteger)methodID
{
	OSSpinLockLock(&spinlockRender3DRenderingEngine);
	[property setValue:[NSNumber numberWithInteger:methodID] forKey:@"render3DRenderingEngine"];
	OSSpinLockUnlock(&spinlockRender3DRenderingEngine);
	
	pthread_mutex_lock(self.mutexProducer);
	NDS_3D_ChangeCore(methodID);
	pthread_mutex_unlock(self.mutexProducer);
}

- (NSInteger) render3DRenderingEngine
{
	OSSpinLockLock(&spinlockRender3DRenderingEngine);
	NSInteger methodID = [(NSNumber *)[property valueForKey:@"render3DRenderingEngine"] integerValue];
	OSSpinLockUnlock(&spinlockRender3DRenderingEngine);
	
	return methodID;
}

- (void) setRender3DHighPrecisionColorInterpolation:(BOOL)state
{
	OSSpinLockLock(&spinlockRender3DHighPrecisionColorInterpolation);
	[property setValue:[NSNumber numberWithBool:state] forKey:@"render3DHighPrecisionColorInterpolation"];
	OSSpinLockUnlock(&spinlockRender3DHighPrecisionColorInterpolation);
	
	bool cState = false;
	if (state)
	{
		cState = true;
	}
	
	pthread_mutex_lock(self.mutexProducer);
	CommonSettings.GFX3D_HighResolutionInterpolateColor = cState;
	pthread_mutex_unlock(self.mutexProducer);
}

- (BOOL) render3DHighPrecisionColorInterpolation
{
	OSSpinLockLock(&spinlockRender3DHighPrecisionColorInterpolation);
	BOOL state = [(NSNumber *)[property valueForKey:@"render3DHighPrecisionColorInterpolation"] boolValue];
	OSSpinLockUnlock(&spinlockRender3DHighPrecisionColorInterpolation);
	
	return state;
}

- (void) setRender3DEdgeMarking:(BOOL)state
{
	OSSpinLockLock(&spinlockRender3DEdgeMarking);
	[property setValue:[NSNumber numberWithBool:state] forKey:@"render3DEdgeMarking"];
	OSSpinLockUnlock(&spinlockRender3DEdgeMarking);
	
	bool cState = false;
	if (state)
	{
		cState = true;
	}
	
	pthread_mutex_lock(self.mutexProducer);
	CommonSettings.GFX3D_EdgeMark = cState;
	pthread_mutex_unlock(self.mutexProducer);
}

- (BOOL) render3DEdgeMarking
{
	OSSpinLockLock(&spinlockRender3DEdgeMarking);
	BOOL state = [(NSNumber *)[property valueForKey:@"render3DEdgeMarking"] boolValue];
	OSSpinLockUnlock(&spinlockRender3DEdgeMarking);
	
	return state;
}

- (void) setRender3DFog:(BOOL)state
{
	OSSpinLockLock(&spinlockRender3DFog);
	[property setValue:[NSNumber numberWithBool:state] forKey:@"render3DFog"];
	OSSpinLockUnlock(&spinlockRender3DFog);
	
	bool cState = false;
	if (state)
	{
		cState = true;
	}
	
	pthread_mutex_lock(self.mutexProducer);
	CommonSettings.GFX3D_Fog = cState;
	pthread_mutex_unlock(self.mutexProducer);
}

- (BOOL) render3DFog
{
	OSSpinLockLock(&spinlockRender3DFog);
	BOOL state = [(NSNumber *)[property valueForKey:@"render3DFog"] boolValue];
	OSSpinLockUnlock(&spinlockRender3DFog);
	
	return state;
}

- (void) setRender3DTextures:(BOOL)state
{
	OSSpinLockLock(&spinlockRender3DTextures);
	[property setValue:[NSNumber numberWithBool:state] forKey:@"render3DTextures"];
	OSSpinLockUnlock(&spinlockRender3DTextures);
	
	bool cState = false;
	if (state)
	{
		cState = true;
	}
	
	pthread_mutex_lock(self.mutexProducer);
	CommonSettings.GFX3D_Texture = cState;
	pthread_mutex_unlock(self.mutexProducer);
}

- (BOOL) render3DTextures
{
	OSSpinLockLock(&spinlockRender3DTextures);
	BOOL state = [(NSNumber *)[property valueForKey:@"render3DTextures"] boolValue];
	OSSpinLockUnlock(&spinlockRender3DTextures);
	
	return state;
}

- (void) setRender3DDepthComparisonThreshold:(NSUInteger)threshold
{
	OSSpinLockLock(&spinlockRender3DDepthComparisonThreshold);
	[property setValue:[NSNumber numberWithInteger:threshold] forKey:@"render3DDepthComparisonThreshold"];
	OSSpinLockUnlock(&spinlockRender3DDepthComparisonThreshold);
	
	pthread_mutex_lock(self.mutexProducer);
	CommonSettings.GFX3D_Zelda_Shadow_Depth_Hack = threshold;
	pthread_mutex_unlock(self.mutexProducer);
}

- (NSUInteger) render3DDepthComparisonThreshold
{
	OSSpinLockLock(&spinlockRender3DDepthComparisonThreshold);
	NSUInteger threshold = [(NSNumber *)[property valueForKey:@"render3DDepthComparisonThreshold"] integerValue];
	OSSpinLockUnlock(&spinlockRender3DDepthComparisonThreshold);
	
	return threshold;
}

- (void) setRender3DThreads:(NSUInteger)numberThreads
{
	OSSpinLockLock(&spinlockRender3DThreads);
	[property setValue:[NSNumber numberWithInteger:numberThreads] forKey:@"render3DThreads"];
	OSSpinLockUnlock(&spinlockRender3DThreads);
	
	NSUInteger numberCores = [[NSProcessInfo processInfo] activeProcessorCount];
	if (numberThreads == 0)
	{
		if (numberCores >= 4)
		{
			numberCores = 4;
		}
		else if (numberCores >= 2)
		{
			numberCores = 2;
		}
		else
		{
			numberCores = 1;
		}
	}
	else
	{
		numberCores = numberThreads;
	}
	
	pthread_mutex_lock(self.mutexProducer);
	CommonSettings.num_cores = numberCores;
	pthread_mutex_unlock(self.mutexProducer);
	
	if ([self render3DRenderingEngine] == CORE3DLIST_SWRASTERIZE)
	{
		pthread_mutex_lock(self.mutexProducer);
		NDS_3D_ChangeCore(CORE3DLIST_SWRASTERIZE);
		pthread_mutex_unlock(self.mutexProducer);
	}
}

- (NSUInteger) render3DThreads
{
	OSSpinLockLock(&spinlockRender3DThreads);
	NSUInteger numberThreads = [(NSNumber *)[property valueForKey:@"render3DThreads"] integerValue];
	OSSpinLockUnlock(&spinlockRender3DThreads);
	
	return numberThreads;
}

- (void) setRender3DLineHack:(BOOL)state
{
	OSSpinLockLock(&spinlockRender3DLineHack);
	[property setValue:[NSNumber numberWithBool:state] forKey:@"render3DLineHack"];
	OSSpinLockUnlock(&spinlockRender3DLineHack);
	
	bool cState = false;
	if (state)
	{
		cState = true;
	}
	
	pthread_mutex_lock(self.mutexProducer);
	CommonSettings.GFX3D_LineHack = cState;
	pthread_mutex_unlock(self.mutexProducer);
}

- (BOOL) render3DLineHack
{
	OSSpinLockLock(&spinlockRender3DLineHack);
	BOOL state = [(NSNumber *)[property valueForKey:@"render3DLineHack"] boolValue];
	OSSpinLockUnlock(&spinlockRender3DLineHack);
	
	return state;
}

- (void) runThread:(id)object
{
	NSAutoreleasePool *tempPool = [[NSAutoreleasePool alloc] init];
	[delegate doInitVideoOutput:self.property];
	[tempPool release];
	
	[super runThread:object];
}

- (void) doCoreEmuFrame
{
	NSData *gpuData = nil;
	NSInteger dispType = self.displayType;
	
	// Here, we copy the raw GPU data from the emulation core.
	// 
	// The core data contains the GPU pixels from both the main and touch screens. So
	// depending on the display type, we copy only the pixels from the respective screen.
	if (dispType == DS_DISPLAY_TYPE_MAIN)
	{
		gpuData = [[NSData alloc] initWithBytes:GPU_screen length:GPU_SCREEN_SIZE_BYTES];
	}
	else if(dispType == DS_DISPLAY_TYPE_TOUCH)
	{
		gpuData = [[NSData alloc] initWithBytes:(GPU_screen + GPU_SCREEN_SIZE_BYTES) length:GPU_SCREEN_SIZE_BYTES];
	}
	else if(dispType == DS_DISPLAY_TYPE_COMBO)
	{
		gpuData = [[NSData alloc] initWithBytes:GPU_screen length:GPU_SCREEN_SIZE_BYTES * 2];
	}
	
	[CocoaDSUtil messageSendOneWayWithData:self.receivePort msgID:MESSAGE_EMU_FRAME_PROCESSED data:gpuData];
	
	// Now that we've finished sending the GPU data, release the local copy.
	[gpuData release];
}

- (void)handlePortMessage:(NSPortMessage *)portMessage
{
	NSInteger message = (NSInteger)[portMessage msgid];
	NSArray *messageComponents = [portMessage components];
	
	switch (message)
	{
		case MESSAGE_EMU_FRAME_PROCESSED:
			[self handleEmuFrameProcessed:[messageComponents objectAtIndex:0]];
			break;
			
		case MESSAGE_RESIZE_VIEW:
			[self handleResizeView:[messageComponents objectAtIndex:0]];
			break;
			
		case MESSAGE_REDRAW_VIEW:
			[self handleRedrawView];
			break;
			
		case MESSAGE_SET_GPU_STATE_FLAGS:
			[self handleChangeGpuStateFlags:[messageComponents objectAtIndex:0]];
			break;
			
		case MESSAGE_CHANGE_DISPLAY_TYPE:
			[self handleChangeDisplayType:[messageComponents objectAtIndex:0]];
			break;
			
		case MESSAGE_CHANGE_BILINEAR_OUTPUT:
			[self handleChangeBilinearOutput:[messageComponents objectAtIndex:0]];
			break;
			
		case MESSAGE_CHANGE_VIDEO_FILTER:
			[self handleChangeVideoFilter:[messageComponents objectAtIndex:0]];
			break;
			
		case MESSAGE_SET_RENDER3D_METHOD:
			[self handleSetRender3DRenderingEngine:[messageComponents objectAtIndex:0]];
			break;
			
		case MESSAGE_SET_RENDER3D_HIGH_PRECISION_COLOR_INTERPOLATION:
			[self handleSetRender3DHighPrecisionColorInterpolation:[messageComponents objectAtIndex:0]];
			break;
			
		case MESSAGE_SET_RENDER3D_EDGE_MARKING:
			[self handleSetRender3DEdgeMarking:[messageComponents objectAtIndex:0]];
			break;
			
		case MESSAGE_SET_RENDER3D_FOG:
			[self handleSetRender3DFog:[messageComponents objectAtIndex:0]];
			break;
			
		case MESSAGE_SET_RENDER3D_TEXTURES:
			[self handleSetRender3DTextures:[messageComponents objectAtIndex:0]];
			break;
			
		case MESSAGE_SET_RENDER3D_DEPTH_COMPARISON_THRESHOLD:
			[self handleSetRender3DDepthComparisonThreshold:[messageComponents objectAtIndex:0]];
			break;
			
		case MESSAGE_SET_RENDER3D_THREADS:
			[self handleSetRender3DThreads:[messageComponents objectAtIndex:0]];
			break;
			
		case MESSAGE_SET_RENDER3D_LINE_HACK:
			[self handleSetRender3DLineHack:[messageComponents objectAtIndex:0]];
			break;
			
		case MESSAGE_SET_VIEW_TO_BLACK:
			[self handleSetViewToBlack];
			break;
			
		case MESSAGE_SET_VIEW_TO_WHITE:
			[self handleSetViewToWhite];
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

- (void) handleEmuFrameProcessed:(NSData *)theData
{
	// Tell the video output object to process the video frame with our copied GPU data.
	if ([vf typeID] == VideoFilterTypeID_None)
	{
		[delegate doProcessVideoFrame:[theData bytes] frameSize:[vf destSize]];
	}
	else
	{
		NSSize srcSize = [vf srcSize];
		
		OSSpinLockLock(&spinlockVfSrcBuffer);
		RGBA5551ToRGBA8888Buffer((const uint16_t *)[theData bytes], (uint32_t *)[vf srcBufferPtr], ((unsigned int)srcSize.width * (unsigned int)srcSize.height));
		OSSpinLockUnlock(&spinlockVfSrcBuffer);
		
		const UInt32 *vfDestBufferPtr = [vf runFilter];
		[delegate doProcessVideoFrame:vfDestBufferPtr frameSize:[vf destSize]];
	}
	
	[super handleEmuFrameProcessed:theData];
}

- (void) handleResizeView:(NSData *)rectData
{
	if (delegate == nil || ![delegate respondsToSelector:@selector(doResizeView:)])
	{
		return;
	}
	
	const NSRect *resizeRect = (NSRect *)[rectData bytes];
	[delegate doResizeView:*resizeRect];
}

- (void) handleRedrawView
{
	if (delegate == nil || ![delegate respondsToSelector:@selector(doRedraw)])
	{
		return;
	}
	
	[delegate doRedraw];
}

- (void) handleChangeGpuStateFlags:(NSData *)flagsData
{
	const NSInteger *flags = (NSInteger *)[flagsData bytes];
	self.gpuStateFlags = (UInt32)*flags;
	[self handleEmuFrameProcessed:self.frameData];
}

- (void) handleChangeDisplayType:(NSData *)displayTypeIdData
{
	if (delegate == nil || ![delegate respondsToSelector:@selector(doDisplayTypeChanged:)])
	{
		return;
	}
	
	const NSInteger *theType = (NSInteger *)[displayTypeIdData bytes];
	self.displayType = *theType;
	[delegate doDisplayTypeChanged:*theType];
}

- (void) handleChangeBilinearOutput:(NSData *)bilinearStateData
{
	if (delegate == nil || ![delegate respondsToSelector:@selector(doBilinearOutputChanged:)])
	{
		return;
	}
	
	const BOOL *theState = (BOOL *)[bilinearStateData bytes];
	[delegate doBilinearOutputChanged:*theState];
	[self handleEmuFrameProcessed:self.frameData];
}

- (void) handleChangeVideoFilter:(NSData *)videoFilterTypeIdData
{
	if (delegate == nil || ![delegate respondsToSelector:@selector(doVideoFilterChanged:)])
	{
		return;
	}
	
	const NSInteger *theType = (NSInteger *)[videoFilterTypeIdData bytes];
	[self setVfType:*theType];
	[delegate doVideoFilterChanged:*theType];
	[self handleEmuFrameProcessed:self.frameData];
}

- (void) handleSetRender3DRenderingEngine:(NSData *)methodIdData
{
	const NSInteger *methodID = (NSInteger *)[methodIdData bytes];
	[self setRender3DRenderingEngine:*methodID];
}

- (void) handleSetRender3DHighPrecisionColorInterpolation:(NSData *)stateData
{
	const BOOL *theState = (BOOL *)[stateData bytes];
	[self setRender3DHighPrecisionColorInterpolation:*theState];
}

- (void) handleSetRender3DEdgeMarking:(NSData *)stateData
{
	const BOOL *theState = (BOOL *)[stateData bytes];
	[self setRender3DEdgeMarking:*theState];
}

- (void) handleSetRender3DFog:(NSData *)stateData
{
	const BOOL *theState = (BOOL *)[stateData bytes];
	[self setRender3DFog:*theState];
}

- (void) handleSetRender3DTextures:(NSData *)stateData
{
	const BOOL *theState = (BOOL *)[stateData bytes];
	[self setRender3DTextures:*theState];
}

- (void) handleSetRender3DDepthComparisonThreshold:(NSData *)thresholdData
{
	const NSUInteger *threshold = (NSUInteger *)[thresholdData bytes];
	[self setRender3DDepthComparisonThreshold:*threshold];
}

- (void) handleSetRender3DThreads:(NSData *)numberThreadsData
{
	const NSUInteger *numberThreads = (NSUInteger *)[numberThreadsData bytes];
	[self setRender3DThreads:*numberThreads];
}

- (void) handleSetRender3DLineHack:(NSData *)stateData
{
	const BOOL *theState = (BOOL *)[stateData bytes];
	[self setRender3DLineHack:*theState];
}

- (void) handleSetViewToBlack
{
	NSSize destSize = [vf destSize];
	NSUInteger dataSize = (NSUInteger)destSize.width * (NSUInteger)destSize.height;
	
	void *texData = NULL;
	if ([vf typeID] == VideoFilterTypeID_None)
	{
		texData = calloc(dataSize, sizeof(UInt16));
		dataSize *= sizeof(UInt16);
	}
	else
	{
		texData = calloc(dataSize, sizeof(UInt32));
		dataSize *= sizeof(UInt32);
	}
	
	if (texData == NULL)
	{
		return;
	}
	
	NSData *gpuData = [[[NSData alloc] initWithBytes:texData length:dataSize] autorelease];
	if (gpuData == nil)
	{
		return;
	}
	
	[delegate doProcessVideoFrame:texData frameSize:destSize];
	
	self.frameData = gpuData;
	
	free(texData);
	texData = nil;
}

- (void) handleSetViewToWhite
{
	NSSize destSize = [vf destSize];
	NSUInteger dataSize = (NSUInteger)destSize.width * (NSUInteger)destSize.height;
	
	if ([vf typeID] == VideoFilterTypeID_None)
	{
		dataSize *= sizeof(UInt16);
	}
	else
	{
		dataSize *= sizeof(UInt32);
	}
	
	void *texData = malloc(dataSize);
	if (texData == NULL)
	{
		return;
	}
	
	memset(texData, 255, dataSize);
	
	NSData *gpuData = [[[NSData alloc] initWithBytes:texData length:dataSize] autorelease];
	if (gpuData == nil)
	{
		return;
	}
	
	[delegate doProcessVideoFrame:texData frameSize:destSize];
	
	self.frameData = gpuData;
	
	free(texData);
	texData = nil;
}

- (void) handleRequestScreenshot:(NSData *)fileURLStringData fileTypeData:(NSData *)fileTypeData
{
	NSString *fileURLString = [[NSString alloc] initWithData:fileURLStringData encoding:NSUTF8StringEncoding];
	NSURL *fileURL = [NSURL URLWithString:fileURLString];
	NSBitmapImageFileType *fileType = (NSBitmapImageFileType *)[fileTypeData bytes];	
	
	NSDictionary *userInfo = [[NSDictionary alloc] initWithObjectsAndKeys:
							  fileURL, @"fileURL", 
							  [NSNumber numberWithInteger:(NSInteger)*fileType], @"fileType",
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

- (NSImage *) image
{
	NSImage *newImage = [[NSImage alloc] initWithSize:[vf srcSize]];
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
	if (self.frameData == nil)
	{
		return nil;
	}
	
	NSSize srcSize = [vf srcSize];
	NSUInteger w = (NSUInteger)srcSize.width;
	NSUInteger h = (NSUInteger)srcSize.height;
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
	
	uint32_t *bitmapData = (uint32_t *)[imageRep bitmapData];
	RGBA5551ToRGBA8888Buffer((const uint16_t *)[self.frameData bytes], bitmapData, (w * h));
	
#ifdef __BIG_ENDIAN__
	uint32_t *bitmapDataEnd = bitmapData + (w * h);
	while (bitmapData < bitmapDataEnd)
	{
		*bitmapData++ = CFSwapInt32LittleToHost(*bitmapData);
	}
#endif
	
	return [imageRep autorelease];
}

- (BOOL) gpuStateByBit:(UInt32)stateBit
{
	BOOL result = NO;
	UInt32 flags = self.gpuStateFlags;
	
	if (flags & (1 << stateBit))
	{
		result = YES;
	}
	
	return result;
}

- (BOOL) isGPUTypeDisplayed:(NSInteger)theGpuType
{
	BOOL result = NO;
	UInt32 flags = self.gpuStateFlags;
	
	switch (theGpuType)
	{
		case DS_GPU_TYPE_MAIN:
			if (flags & GPUSTATE_MAIN_GPU_MASK)
			{
				result = YES;
			}
			break;
			
		case DS_GPU_TYPE_SUB:
			if (flags & GPUSTATE_SUB_GPU_MASK)
			{
				result = YES;
			}
			break;
			
		case DS_GPU_TYPE_COMBO:
			if (flags & (GPUSTATE_MAIN_GPU_MASK | GPUSTATE_SUB_GPU_MASK))
			{
				result = YES;
			}
			break;
			
		default:
			break;
	}
	
	return result;
}

- (void) hideGPUType:(NSInteger)theGpuType
{
	UInt32 flags = self.gpuStateFlags;
	
	switch (theGpuType)
	{
		case DS_GPU_TYPE_MAIN:
			flags &= ~GPUSTATE_MAIN_GPU_MASK;
			break;
			
		case DS_GPU_TYPE_SUB:
			flags &= ~GPUSTATE_SUB_GPU_MASK;
			break;
			
		case DS_GPU_TYPE_COMBO:
			flags &= ~GPUSTATE_MAIN_GPU_MASK;
			flags &= ~GPUSTATE_SUB_GPU_MASK;
			break;
			
		default:
			break;
	}
	
	self.gpuStateFlags = flags;
}

- (void) showGPUType:(NSInteger)theGpuType
{
	UInt32 flags = self.gpuStateFlags;
	
	switch (theGpuType)
	{
		case DS_GPU_TYPE_MAIN:
			flags |= GPUSTATE_MAIN_GPU_MASK;
			break;
			
		case DS_GPU_TYPE_SUB:
			flags |= GPUSTATE_SUB_GPU_MASK;
			break;
			
		case DS_GPU_TYPE_COMBO:
			flags |= GPUSTATE_MAIN_GPU_MASK;
			flags |= GPUSTATE_SUB_GPU_MASK;
			break;
			
		default:
			break;
	}
	
	self.gpuStateFlags = flags;
}

@end

bool opengl_init(void)
{
	return true;
}

void HandleMessageEchoResponse(NSPortMessage *portMessage)
{
	NSPortMessage *echo = [[NSPortMessage alloc] initWithSendPort:[portMessage receivePort] receivePort:[portMessage sendPort] components:nil];
	[echo setMsgid:MESSAGE_CHECK_RESPONSE_ECHO];
	NSDate *sendDate = [[NSDate alloc] init];
	[echo sendBeforeDate:sendDate];
	[echo release];
	[sendDate release];
}

void SetGPULayerState(int displayType, unsigned int i, bool state)
{
	GPU *theGpu = NULL;
	
	// Check bounds on the layer index.
	if(i > 4)
	{
		return;
	}
	
	switch (displayType)
	{
		case DS_GPU_TYPE_MAIN:
			theGpu = SubScreen.gpu;
			break;
			
		case DS_GPU_TYPE_SUB:
			theGpu = MainScreen.gpu;
			break;
			
		case DS_GPU_TYPE_COMBO:
			SetGPULayerState(DS_GPU_TYPE_SUB, i, state); // Recursive call
			theGpu = MainScreen.gpu;
			break;
			
		default:
			break;
	}
	
	if (theGpu != NULL)
	{
		if (state)
		{
			GPU_addBack(theGpu, i);
		}
		else
		{
			GPU_remove(theGpu, i);
		}
	}
}

bool GetGPULayerState(int displayType, unsigned int i)
{
	bool result = false;
	
	// Check bounds on the layer index.
	if(i > 4)
	{
		return result;
	}
	
	switch (displayType)
	{
		case DS_GPU_TYPE_MAIN:
			if (SubScreen.gpu != nil)
			{
				result = CommonSettings.dispLayers[SubScreen.gpu->core][i];
			}
			break;
			
		case DS_GPU_TYPE_SUB:
			if (MainScreen.gpu != nil)
			{
				result = CommonSettings.dispLayers[MainScreen.gpu->core][i];
			}
			break;
			
		case DS_GPU_TYPE_COMBO:
			if (SubScreen.gpu != nil && MainScreen.gpu != nil)
			{
				result = (CommonSettings.dispLayers[SubScreen.gpu->core][i] && CommonSettings.dispLayers[MainScreen.gpu->core][i]);
			}
			break;
			
		default:
			break;
	}
	
	return result;
}

void SetGPUDisplayState(int displayType, bool state)
{
	switch (displayType)
	{
		case DS_GPU_TYPE_MAIN:
			CommonSettings.showGpu.sub = state;
			break;
			
		case DS_GPU_TYPE_SUB:
			CommonSettings.showGpu.main = state;
			break;
			
		case DS_GPU_TYPE_COMBO:
			CommonSettings.showGpu.sub = state;
			CommonSettings.showGpu.main = state;
			break;
			
		default:
			break;
	}
}

bool GetGPUDisplayState(int displayType)
{
	bool result = false;
	
	switch (displayType)
	{
		case DS_GPU_TYPE_MAIN:
			result = CommonSettings.showGpu.sub;
			break;
			
		case DS_GPU_TYPE_SUB:
			result = CommonSettings.showGpu.main;
			break;
			
		case DS_GPU_TYPE_COMBO:
			result = (CommonSettings.showGpu.sub && CommonSettings.showGpu.main);
			break;
			
		default:
			break;
	}
	
	return result;
}
