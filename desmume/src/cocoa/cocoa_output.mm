/*
	Copyright (C) 2011 Roger Manuel
	Copyright (C) 2011-2015 DeSmuME team

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
#include "sndOSX.h"

#include "../NDSSystem.h"
#include "../GPU.h"
#include "../SPU.h"
#include "../metaspu/metaspu.h"

#import <Cocoa/Cocoa.h>

#undef BOOL


@implementation CocoaDSOutput

@synthesize isStateChanged;
@synthesize frameCount;
@synthesize frameData;
@synthesize frameAttributesData;
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
	
	isStateChanged = NO;
	frameCount = 0;
	frameData = nil;
	frameAttributesData = nil;
	
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
	
	self.frameData = nil;
	self.frameAttributesData = nil;
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
			[self handleEmuFrameProcessed:[messageComponents objectAtIndex:0] attributes:[messageComponents objectAtIndex:1]];
			break;
			
		default:
			[super handlePortMessage:portMessage];
			break;
	}
}

- (void) handleEmuFrameProcessed:(NSData *)mainData attributes:(NSData *)attributesData
{
	self.frameCount++;
	self.frameData = mainData;
	self.frameAttributesData = attributesData;
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
		case MESSAGE_EMU_FRAME_PROCESSED:
			[self handleEmuFrameProcessed:nil attributes:nil];
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

- (void) handleEmuFrameProcessed:(NSData *)mainData attributes:(NSData *)attributesData
{
	SPU_Emulate_user();
	
	[super handleEmuFrameProcessed:mainData attributes:attributesData];
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

@synthesize delegate;
@dynamic displayMode;
@dynamic frameSize;


- (id)init
{
	self = [super init];
	if (self == nil)
	{
		return self;
	}
	
	spinlockDisplayType = OS_SPINLOCK_INIT;
	
	delegate = nil;
	displayMode = DS_DISPLAY_TYPE_DUAL;
	frameSize = NSMakeSize((CGFloat)GPU_DISPLAY_WIDTH, (CGFloat)GPU_DISPLAY_HEIGHT * 2);
	
	[property setValue:[NSNumber numberWithInteger:displayMode] forKey:@"displayMode"];
	[property setValue:NSSTRING_DISPLAYMODE_MAIN forKey:@"displayModeString"];
	
	return self;
}

- (void)dealloc
{
	[self setDelegate:nil];
			
	[super dealloc];
}

- (void) setDisplayMode:(NSInteger)displayModeID
{
	NSString *newDispString = nil;
	NSSize newFrameSize = NSMakeSize((CGFloat)GPU_DISPLAY_WIDTH, (CGFloat)GPU_DISPLAY_HEIGHT);
	
	switch (displayModeID)
	{
		case DS_DISPLAY_TYPE_MAIN:
			newDispString = NSSTRING_DISPLAYMODE_MAIN;
			break;
			
		case DS_DISPLAY_TYPE_TOUCH:
			newDispString = NSSTRING_DISPLAYMODE_TOUCH;
			break;
			
		case DS_DISPLAY_TYPE_DUAL:
			newDispString = NSSTRING_DISPLAYMODE_DUAL;
			newFrameSize.height *= 2;
			break;
			
		default:
			return;
			break;
	}
	
	OSSpinLockLock(&spinlockDisplayType);
	displayMode = displayModeID;
	frameSize = newFrameSize;
	[property setValue:[NSNumber numberWithInteger:displayModeID] forKey:@"displayMode"];
	[property setValue:newDispString forKey:@"displayModeString"];
	OSSpinLockUnlock(&spinlockDisplayType);
}

- (NSInteger) displayMode
{
	OSSpinLockLock(&spinlockDisplayType);
	NSInteger displayModeID = displayMode;
	OSSpinLockUnlock(&spinlockDisplayType);
	
	return displayModeID;
}

- (NSSize) frameSize
{
	OSSpinLockLock(&spinlockDisplayType);
	NSSize size = frameSize;
	OSSpinLockUnlock(&spinlockDisplayType);
	
	return size;
}

- (void) doCoreEmuFrame
{
	NSInteger displayModeID = [self displayMode];
	NSSize displayFrameSize = [self frameSize];
	
	// We will be ignoring the actual video data here since we will be pulling the video data
	// from the consumer thread instead.
	NSData *gpuData = [[NSData alloc] init];
	
	DisplaySrcPixelAttributes attr;
	attr.videoSourceID = VIDEO_SOURCE_EMULATOR;
	attr.displayModeID = (int32_t)displayModeID;
	attr.width = (uint16_t)displayFrameSize.width;
	attr.height = (uint16_t)displayFrameSize.height;
	
	NSData *attributesData = [[NSData alloc] initWithBytes:&attr length:sizeof(DisplaySrcPixelAttributes)];
	
	NSArray *messageComponents = [[NSArray alloc] initWithObjects:gpuData, attributesData, nil];
	[CocoaDSUtil messageSendOneWayWithMessageComponents:self.receivePort msgID:MESSAGE_EMU_FRAME_PROCESSED array:messageComponents];
	
	[gpuData release];
	[attributesData release];
	[messageComponents release];
}

- (void)handlePortMessage:(NSPortMessage *)portMessage
{
	NSInteger message = (NSInteger)[portMessage msgid];
	NSArray *messageComponents = [portMessage components];
	
	switch (message)
	{
		case MESSAGE_EMU_FRAME_PROCESSED:
			[self handleEmuFrameProcessed:[messageComponents objectAtIndex:0] attributes:[messageComponents objectAtIndex:1]];
			break;
			
		case MESSAGE_CHANGE_DISPLAY_TYPE:
			[self handleChangeDisplayMode:[messageComponents objectAtIndex:0]];
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

- (void) handleEmuFrameProcessed:(NSData *)mainData attributes:(NSData *)attributesData
{
	[super handleEmuFrameProcessed:mainData attributes:attributesData];
}

- (void) handleChangeDisplayMode:(NSData *)displayModeData
{
	if (delegate == nil || ![delegate respondsToSelector:@selector(doDisplayModeChanged:)])
	{
		return;
	}
	
	const NSInteger displayModeID = *(NSInteger *)[displayModeData bytes];
	[self setDisplayMode:displayModeID];
	[delegate doDisplayModeChanged:displayModeID];
}

- (void) handleSetViewToBlack
{
	[self sendVideoFrameOfRGBA5551:0x8000];
}

- (void) handleSetViewToWhite
{
	[self sendVideoFrameOfRGBA5551:0xFFFF];
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

- (NSData *) videoFrameUsingRGBA5551:(uint16_t)colorValue pixelCount:(size_t)pixCount
{
	NSData *gpuData = nil;
	const size_t bufSize = pixCount * sizeof(uint16_t);
	uint16_t *gpuBytes = (uint16_t *)malloc(bufSize);
	if (gpuBytes == NULL)
	{
		return gpuData;
	}
	
	if (pixCount % 16 == 0)
	{
		const uint16_t colorValuePattern[] = {colorValue, colorValue, colorValue, colorValue, colorValue, colorValue, colorValue, colorValue};
		memset_pattern16(gpuBytes, colorValuePattern, bufSize);
	}
	else
	{
		memset(gpuBytes, colorValue, bufSize);
	}
	
	
	gpuData = [NSData dataWithBytes:gpuBytes length:bufSize];
	
	free(gpuBytes);
	gpuBytes = nil;
	
	return gpuData;
}

- (void) sendVideoFrameOfRGBA5551:(uint16_t)colorValue
{
	const NSInteger displayModeID = [self displayMode];
	const NSSize displayFrameSize = [self frameSize];
	const size_t pixCount = (displayModeID == DS_DISPLAY_TYPE_DUAL) ? GPU_DISPLAY_WIDTH * GPU_DISPLAY_HEIGHT * 2 : GPU_DISPLAY_WIDTH * GPU_DISPLAY_HEIGHT;
	
	NSData *videoData = [self videoFrameUsingRGBA5551:colorValue pixelCount:pixCount];
	
	DisplaySrcPixelAttributes attr;
	attr.videoSourceID = VIDEO_SOURCE_INTERNAL;
	attr.displayModeID = (int32_t)displayModeID;
	attr.width = (uint16_t)displayFrameSize.width;
	attr.height = (uint16_t)displayFrameSize.height;
	
	NSData *attributesData = [NSData dataWithBytes:&attr length:sizeof(DisplaySrcPixelAttributes)];
	
	[self handleEmuFrameProcessed:videoData attributes:attributesData];
}

- (NSImage *) image
{
	NSImage *newImage = [[NSImage alloc] initWithSize:self.frameSize];
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
	
	NSSize srcSize = self.frameSize;
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
	RGB555ToRGBA8888Buffer((const uint16_t *)[self.frameData bytes], bitmapData, (w * h));
	
#ifdef __BIG_ENDIAN__
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

- (id)init
{
	self = [super init];
	if (self == nil)
	{
		return self;
	}
	
	[property setValue:[NSNumber numberWithInteger:(NSInteger)VideoFilterTypeID_None] forKey:@"videoFilterType"];
	[property setValue:[CocoaVideoFilter typeStringByID:VideoFilterTypeID_None] forKey:@"videoFilterTypeString"];
	
	return self;
}

- (void)dealloc
{
	[super dealloc];
}

- (void) runThread:(id)object
{
	NSAutoreleasePool *tempPool = [[NSAutoreleasePool alloc] init];
	[(id<CocoaDSDisplayVideoDelegate>)delegate doInitVideoOutput:self.property];
	[tempPool release];
	
	[super runThread:object];
}

- (void)handlePortMessage:(NSPortMessage *)portMessage
{
	NSInteger message = (NSInteger)[portMessage msgid];
	NSArray *messageComponents = [portMessage components];
	
	switch (message)
	{
		case MESSAGE_EMU_FRAME_PROCESSED:
			[self handleEmuFrameProcessed:[messageComponents objectAtIndex:0] attributes:[messageComponents objectAtIndex:1]];
			break;
			
		case MESSAGE_REPROCESS_AND_REDRAW:
			[self handleReprocessAndRedraw];
			break;
			
		case MESSAGE_RESIZE_VIEW:
			[self handleResizeView:[messageComponents objectAtIndex:0]];
			break;
			
		case MESSAGE_TRANSFORM_VIEW:
			[self handleTransformView:[messageComponents objectAtIndex:0]];
			break;
			
		case MESSAGE_REDRAW_VIEW:
			[self handleRedrawView];
			break;
			
		case MESSAGE_CHANGE_DISPLAY_ORIENTATION:
			[self handleChangeDisplayOrientation:[messageComponents objectAtIndex:0]];
			break;
			
		case MESSAGE_CHANGE_DISPLAY_ORDER:
			[self handleChangeDisplayOrder:[messageComponents objectAtIndex:0]];
			break;
			
		case MESSAGE_CHANGE_DISPLAY_GAP:
			[self handleChangeDisplayGap:[messageComponents objectAtIndex:0]];
			break;
			
		default:
			[super handlePortMessage:portMessage];
			break;
	}
}

- (void) handleEmuFrameProcessed:(NSData *)mainData attributes:(NSData *)attributesData
{
	if (mainData == nil || attributesData == nil)
	{
		return;
	}
	
	NSData *newVideoFrame = mainData;
	DisplaySrcPixelAttributes attr = *(DisplaySrcPixelAttributes *)[attributesData bytes];
	const NSInteger frameDisplayMode = attr.displayModeID;
	const NSInteger frameSource = attr.videoSourceID;
	const NSInteger frameWidth = attr.width;
	const NSInteger frameHeight = attr.height;
	
	if (frameSource == VIDEO_SOURCE_EMULATOR)
	{
		// Note that we simply received the attributes of the video data, not the actual
		// video data itself. So from here, we need to pull the video data from the
		// emulator and copy it to this thread.
		//
		// The video data contains the pixels from both the main and touch screens. So
		// depending on the display mode, we copy only the pixels from the respective
		// screen.
		
		pthread_rwlock_rdlock(self.rwlockProducer);
		
		switch (frameDisplayMode)
		{
			case DS_DISPLAY_TYPE_MAIN:
				newVideoFrame = [NSData dataWithBytes:GPU_screen length:GPU_SCREEN_SIZE_BYTES];
				break;
				
			case DS_DISPLAY_TYPE_TOUCH:
				newVideoFrame = [NSData dataWithBytes:(GPU_screen + GPU_SCREEN_SIZE_BYTES) length:GPU_SCREEN_SIZE_BYTES];
				break;
				
			case DS_DISPLAY_TYPE_DUAL:
				newVideoFrame = [NSData dataWithBytes:GPU_screen length:GPU_SCREEN_SIZE_BYTES*2];
				break;
				
			default:
				break;
		}
		
		pthread_rwlock_unlock(self.rwlockProducer);
	}
	
	[(id<CocoaDSDisplayVideoDelegate>)delegate doProcessVideoFrame:[newVideoFrame bytes] displayMode:frameDisplayMode width:frameWidth height:frameHeight];
	
	// If we need to use our saved frame data, make sure that we don't pull from the
	// emulation thread again.
	attr.videoSourceID = VIDEO_SOURCE_INTERNAL;
	NSData *savedAttributesData = [NSData dataWithBytes:&attr length:sizeof(DisplaySrcPixelAttributes)];
	[super handleEmuFrameProcessed:newVideoFrame attributes:savedAttributesData];
}

- (void) handleResizeView:(NSData *)rectData
{
	if (delegate == nil || ![delegate respondsToSelector:@selector(doResizeView:)])
	{
		return;
	}
	
	const NSRect resizeRect = *(NSRect *)[rectData bytes];
	[(id<CocoaDSDisplayVideoDelegate>)delegate doResizeView:resizeRect];
}

- (void) handleTransformView:(NSData *)transformData
{
	if (delegate == nil || ![delegate respondsToSelector:@selector(doTransformView:)])
	{
		return;
	}
	
	[(id<CocoaDSDisplayVideoDelegate>)delegate doTransformView:(DisplayOutputTransformData *)[transformData bytes]];
}

- (void) handleRedrawView
{
	if (delegate == nil || ![delegate respondsToSelector:@selector(doRedraw)])
	{
		return;
	}
	
	[(id<CocoaDSDisplayVideoDelegate>)delegate doRedraw];
}

- (void) handleReprocessAndRedraw
{
	[self handleEmuFrameProcessed:self.frameData attributes:self.frameAttributesData];
}

- (void) handleChangeDisplayOrientation:(NSData *)displayOrientationIdData
{
	if (delegate == nil || ![delegate respondsToSelector:@selector(doDisplayOrientationChanged:)])
	{
		return;
	}
	
	const NSInteger theOrientation = *(NSInteger *)[displayOrientationIdData bytes];
	[(id<CocoaDSDisplayVideoDelegate>)delegate doDisplayOrientationChanged:theOrientation];
}

- (void) handleChangeDisplayOrder:(NSData *)displayOrderIdData
{
	if (delegate == nil || ![delegate respondsToSelector:@selector(doDisplayOrderChanged:)])
	{
		return;
	}
	
	const NSInteger theOrder = *(NSInteger *)[displayOrderIdData bytes];
	[(id<CocoaDSDisplayVideoDelegate>)delegate doDisplayOrderChanged:theOrder];
}

- (void) handleChangeDisplayGap:(NSData *)displayGapScalarData
{
	if (delegate == nil || ![delegate respondsToSelector:@selector(doDisplayGapChanged:)])
	{
		return;
	}
	
	const float gapScalar = *(float *)[displayGapScalarData bytes];
	[(id<CocoaDSDisplayVideoDelegate>)delegate doDisplayGapChanged:gapScalar];
}

@end
