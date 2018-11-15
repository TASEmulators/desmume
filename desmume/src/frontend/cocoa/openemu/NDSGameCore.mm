/*
	Copyright (C) 2012-2017 DeSmuME team

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

#import "NDSGameCore.h"
#import "cocoa_cheat.h"
#import "cocoa_globals.h"
#import "cocoa_file.h"
#import "cocoa_firmware.h"
#import "cocoa_GPU.h"
#import "cocoa_input.h"
#import "ClientDisplayView.h"
#import "ClientInputHandler.h"
#import "OESoundInterface.h"
#import "OENDSSystemResponderClient.h"

#include <OpenGL/gl.h>
#include "../../NDSSystem.h"
#include "../../GPU.h"
#undef BOOL


volatile bool execute = true;

@implementation NDSGameCore

@synthesize cdsController;
@synthesize cdsGPU;
@synthesize cdsFirmware;
@synthesize cdsCheats;
@dynamic displayMode;

- (id)init
{
	self = [super init];
	if(self == nil)
	{
		return self;
	}
	
	// Set up threading locks
	spinlockDisplayMode = OS_SPINLOCK_INIT;
	pthread_rwlock_init(&rwlockCoreExecute, NULL);
	
	// Set up input handling
	touchLocation.x = 0;
	touchLocation.y = 0;
	
	inputID[OENDSButtonUp]			= NDSInputID_Up;
	inputID[OENDSButtonDown]		= NDSInputID_Down;
	inputID[OENDSButtonLeft]		= NDSInputID_Left;
	inputID[OENDSButtonRight]		= NDSInputID_Right;
	inputID[OENDSButtonA]			= NDSInputID_A;
	inputID[OENDSButtonB]			= NDSInputID_B;
	inputID[OENDSButtonX]			= NDSInputID_X;
	inputID[OENDSButtonY]			= NDSInputID_Y;
	inputID[OENDSButtonL]			= NDSInputID_L;
	inputID[OENDSButtonR]			= NDSInputID_R;
	inputID[OENDSButtonStart]		= NDSInputID_Start;
	inputID[OENDSButtonSelect]		= NDSInputID_Select;
	inputID[OENDSButtonMicrophone]	= NDSInputID_Microphone;
	inputID[OENDSButtonLid]			= NDSInputID_Lid;
	inputID[OENDSButtonDebug]		= NDSInputID_Debug;
	
	// Set up the emulation core
	CommonSettings.advanced_timing = true;
	CommonSettings.jit_max_block_size = 12;
	CommonSettings.use_jit = true;
	NDS_Init();
	
	// Set up the DS GPU
	cdsGPU = [[[[CocoaDSGPU alloc] init] retain] autorelease];
	[cdsGPU setRender3DThreads:0]; // Pass 0 to automatically set the number of rendering threads
	[cdsGPU setRender3DRenderingEngine:CORE3DLIST_SWRASTERIZE];
	[cdsGPU setGpuScale:1];
	[cdsGPU setGpuColorFormat:NDSColorFormat_BGR666_Rev];
	
	// Set up the DS controller
	cdsController = [[[[CocoaDSController alloc] init] retain] autorelease];
	
	// Set up the cheat system
	cdsCheats = [[[[CocoaDSCheatManager alloc] init] retain] autorelease];
	[cdsCheats setRwlockCoreExecute:&rwlockCoreExecute];
	addedCheatsDict = [[NSMutableDictionary alloc] initWithCapacity:128];
	
	// Set up the DS firmware using the internal firmware
	cdsFirmware = [[[[CocoaDSFirmware alloc] init] retain] autorelease];
	[cdsFirmware applySettings];
	
	// Set up the sound core
	CommonSettings.spu_advanced = true;
	CommonSettings.spuInterpolationMode = SPUInterpolation_Cosine;
	CommonSettings.SPU_sync_mode = SPU_SYNC_MODE_SYNCHRONOUS;
	CommonSettings.SPU_sync_method = SPU_SYNC_METHOD_N;
	openEmuSoundInterfaceBuffer = [self ringBufferAtIndex:0];
	
	NSInteger result = SPU_ChangeSoundCore(SNDCORE_OPENEMU, (int)SPU_BUFFER_BYTES);
	if (result == -1)
	{
		SPU_ChangeSoundCore(SNDCORE_DUMMY, 0);
	}
	
	SPU_SetSynchMode(CommonSettings.SPU_sync_mode, CommonSettings.SPU_sync_method);
	SPU_SetVolume(100);
    
	// Set up the DS display
	displayMode = ClientDisplayMode_Dual;
	displayRect = OEIntRectMake(0, 0, GPU_FRAMEBUFFER_NATIVE_WIDTH, GPU_FRAMEBUFFER_NATIVE_HEIGHT * 2);
	displayAspectRatio = OEIntSizeMake(2, 3);
	
	return self;
}

- (void)dealloc
{
	SPU_ChangeSoundCore(SNDCORE_DUMMY, 0);
	NDS_DeInit();
	
	[addedCheatsDict release];
	[self setCdsCheats:nil];
	[self setCdsController:nil];
	[self setCdsGPU:nil];
	[self setCdsFirmware:nil];
	
	pthread_rwlock_destroy(&rwlockCoreExecute);
	
	[super dealloc];
}

- (NSInteger) displayMode
{
	OSSpinLockLock(&spinlockDisplayMode);
	const NSInteger theMode = displayMode;
	OSSpinLockUnlock(&spinlockDisplayMode);
	
	return theMode;
}

- (void) setDisplayMode:(NSInteger)theMode
{
	OEIntRect newDisplayRect;
	OEIntSize newDisplayAspectRatio;
	
	switch (theMode)
	{
		case ClientDisplayMode_Main:
			newDisplayRect = OEIntRectMake(0, 0, GPU_FRAMEBUFFER_NATIVE_WIDTH, GPU_FRAMEBUFFER_NATIVE_HEIGHT);
			newDisplayAspectRatio = OEIntSizeMake(4, 3);
			break;
			
		case ClientDisplayMode_Touch:
			newDisplayRect = OEIntRectMake(0, GPU_FRAMEBUFFER_NATIVE_HEIGHT + 1, GPU_FRAMEBUFFER_NATIVE_WIDTH, GPU_FRAMEBUFFER_NATIVE_HEIGHT);
			newDisplayAspectRatio = OEIntSizeMake(4, 3);
			break;
			
		case ClientDisplayMode_Dual:
			newDisplayRect = OEIntRectMake(0, 0, GPU_FRAMEBUFFER_NATIVE_WIDTH, GPU_FRAMEBUFFER_NATIVE_HEIGHT * 2);
			newDisplayAspectRatio = OEIntSizeMake(2, 3);
			break;
			
		default:
			return;
			break;
	}
	
	OSSpinLockLock(&spinlockDisplayMode);
	displayMode = theMode;
	displayRect = newDisplayRect;
	displayAspectRatio = newDisplayAspectRatio;
	OSSpinLockUnlock(&spinlockDisplayMode);
}

#pragma mark - Plug-in Support

- (BOOL)supportsRewinding
{
	return NO;
}

#pragma mark - Starting

- (BOOL)loadFileAtPath:(NSString*)path
{
	BOOL isRomLoaded = NO;
	NSString *openEmuDataPath = [self batterySavesDirectoryPath];
	NSURL *openEmuDataURL = [NSURL fileURLWithPath:openEmuDataPath];
	
	[CocoaDSFile addURLToURLDictionary:openEmuDataURL groupKey:@PATH_OPEN_EMU fileKind:@"ROM"];
	[CocoaDSFile addURLToURLDictionary:openEmuDataURL groupKey:@PATH_OPEN_EMU fileKind:@"ROM Save"];
	[CocoaDSFile addURLToURLDictionary:openEmuDataURL groupKey:@PATH_OPEN_EMU fileKind:@"Save State"];
	[CocoaDSFile addURLToURLDictionary:openEmuDataURL groupKey:@PATH_OPEN_EMU fileKind:@"Screenshot"];
	[CocoaDSFile addURLToURLDictionary:openEmuDataURL groupKey:@PATH_OPEN_EMU fileKind:@"Video"];
	[CocoaDSFile addURLToURLDictionary:openEmuDataURL groupKey:@PATH_OPEN_EMU fileKind:@"Cheat"];
	[CocoaDSFile addURLToURLDictionary:openEmuDataURL groupKey:@PATH_OPEN_EMU fileKind:@"Sound Sample"];
	[CocoaDSFile addURLToURLDictionary:openEmuDataURL groupKey:@PATH_OPEN_EMU fileKind:@"Firmware Configuration"];
	[CocoaDSFile addURLToURLDictionary:openEmuDataURL groupKey:@PATH_OPEN_EMU fileKind:@"Lua Script"];
	
	[CocoaDSFile setupAllFilePathsWithURLDictionary:@PATH_OPEN_EMU];
	
	// Ensure that the OpenEmu data directory exists before loading the ROM.
	NSFileManager *fileManager = [[NSFileManager alloc] init];
	[fileManager createDirectoryAtPath:openEmuDataPath withIntermediateDirectories:YES attributes:nil error:NULL];
	[fileManager release];
	
	isRomLoaded = [CocoaDSFile loadRom:[NSURL fileURLWithPath:path]];
	
	[CocoaDSCheatManager setMasterCheatList:cdsCheats];
	
	return isRomLoaded;
}

#pragma mark - Execution

- (NSTimeInterval)frameInterval
{
	return DS_FRAMES_PER_SECOND;
}

- (void)executeFrame
{
	ClientInputHandler *inputHandler = [cdsController inputHandler];
	inputHandler->ProcessInputs();
	inputHandler->ApplyInputs();
	
	pthread_rwlock_wrlock(&rwlockCoreExecute);
	NDS_exec<false>();
	pthread_rwlock_unlock(&rwlockCoreExecute);
	
	SPU_Emulate_user();
}

- (void)resetEmulation
{
	pthread_rwlock_wrlock(&rwlockCoreExecute);
	NDS_Reset();
	pthread_rwlock_unlock(&rwlockCoreExecute);
	execute = true;
}

- (void)executeFrameSkippingFrame:(BOOL)skip
{
	if (skip)
	{
		NDS_SkipNextFrame();
	}
	
	[self executeFrame];
}

#pragma mark - Video

- (const void *)getVideoBufferWithHint:(void *)hint
{
	return GPU->GetDisplayInfo().masterNativeBuffer;
}

- (OEGameCoreRendering)gameCoreRendering
{
	return OEGameCoreRendering2DVideo;
}

- (BOOL)hasAlternateRenderingThread
{
	return NO;
}

- (BOOL)needsDoubleBufferedFBO
{
	return NO;
}

- (OEIntSize)bufferSize
{
	return OEIntSizeMake(GPU_FRAMEBUFFER_NATIVE_WIDTH, GPU_FRAMEBUFFER_NATIVE_HEIGHT * 2);
}

- (OEIntRect)screenRect
{
	OSSpinLockLock(&spinlockDisplayMode);
	const OEIntRect theRect = displayRect;
	OSSpinLockUnlock(&spinlockDisplayMode);
	
	return theRect;
}

- (OEIntSize)aspectSize
{
	OSSpinLockLock(&spinlockDisplayMode);
	const OEIntSize theAspectRatio = displayAspectRatio;
	OSSpinLockUnlock(&spinlockDisplayMode);
	
	return theAspectRatio;
}

- (GLenum)pixelType
{
	return GL_UNSIGNED_INT_8_8_8_8_REV;
}

- (GLenum)pixelFormat
{
	return GL_RGBA;
}

- (GLenum)internalPixelFormat
{
	return GL_RGBA;
}

#pragma mark - Audio

- (NSUInteger)audioBufferCount
{
	return 1;
}

- (NSUInteger)channelCount
{
	return SPU_NUMBER_CHANNELS;
}

- (NSUInteger)audioBitDepth
{
	return SPU_SAMPLE_RESOLUTION;
}

- (double)audioSampleRate
{
	return SPU_SAMPLE_RATE;
}

- (NSUInteger)channelCountForBuffer:(NSUInteger)buffer
{
	return [self channelCount];
}

- (NSUInteger)audioBufferSizeForBuffer:(NSUInteger)buffer
{
	return (NSUInteger)SPU_BUFFER_BYTES;
}

- (double)audioSampleRateForBuffer:(NSUInteger)buffer
{
	return [self audioSampleRate];
}

#pragma mark - Save States

- (void)saveStateToFileAtPath:(NSString *)fileName completionHandler:(void(^)(BOOL success, NSError *error))block
{
	BOOL fileSuccess = [CocoaDSFile saveState:[NSURL fileURLWithPath:fileName]];
	
	if (block != nil)
	{
		block(fileSuccess, nil);
	}
}

- (void)loadStateFromFileAtPath:(NSString *)fileName completionHandler:(void(^)(BOOL success, NSError *error))block
{	
	BOOL fileSuccess = [CocoaDSFile loadState:[NSURL fileURLWithPath:fileName]];
	
	if (block != nil)
	{
		block(fileSuccess, nil);
	}
}

#pragma mark - Optional

- (NSTrackingAreaOptions)mouseTrackingOptions
{
	return 0;
}

#pragma mark - Cheats - Optional

- (void)setCheat:(NSString *)code setType:(NSString *)type setEnabled:(BOOL)enabled
{
	// This method can be used for both adding a new cheat or setting the enable
	// state on an existing cheat, so be sure to account for both cases.
	
	// First check if the cheat exists.
	CocoaDSCheatItem *cheatItem = (CocoaDSCheatItem *)[addedCheatsDict objectForKey:code];
	
	if (cheatItem == nil)
	{
		// If the cheat doesn't already exist, then create a new one and add it.
		cheatItem = [[[CocoaDSCheatItem alloc] init] autorelease];
		[cheatItem setCheatType:CHEAT_TYPE_ACTION_REPLAY]; // Default to Action Replay for now
		[cheatItem setFreezeType:0];
		[cheatItem setDescription:@""]; // OpenEmu takes care of this
		[cheatItem setCode:code];
		[cheatItem setMemAddress:0x00000000]; // UNUSED
		[cheatItem setBytes:1]; // UNUSED
		[cheatItem setValue:0]; // UNUSED
		
		[cheatItem setEnabled:enabled];
		[[self cdsCheats] add:cheatItem];
		
		// OpenEmu doesn't currently save cheats per game, so assume that the
		// cheat list is short and that code strings are unique. This allows
		// us to get away with simply saving the cheat code string and hashing
		// for it later.
		[addedCheatsDict setObject:cheatItem forKey:code];
	}
	else
	{
		// If the cheat does exist, then just set its enable state.
		[cheatItem setEnabled:enabled];
		[[self cdsCheats] update:cheatItem];
	}
}

#pragma mark - Internal

- (void)startEmulation
{
	[cdsController startHardwareMicDevice];
	[cdsController setHardwareMicMute:NO];
	[cdsController setHardwareMicPause:NO];
	
	[super startEmulation];
}

- (void)setPauseEmulation:(BOOL)pauseEmulation
{
	[cdsController setHardwareMicPause:pauseEmulation];
	
	[super setPauseEmulation:pauseEmulation];
}

#pragma mark - Input

- (oneway void)didPushNDSButton:(OENDSButton)button forPlayer:(NSUInteger)player
{
	switch (inputID[button])
	{
		case NDSInputID_Microphone:
			[cdsController setSoftwareMicState:YES mode:MicrophoneMode_InternalNoise];
			break;
			
		case NDSInputID_Paddle:
			// Do nothing for now. OpenEmu doesn't currently support the Taito Paddle Controller.
			//[cdsController setPaddleAdjust:0];
			break;
			
		default:
			[cdsController setControllerState:YES controlID:inputID[button]];
			break;
	}
}

- (oneway void)didReleaseNDSButton:(OENDSButton)button forPlayer:(NSUInteger)player
{
	switch (inputID[button])
	{
		case NDSInputID_Microphone:
			[cdsController setSoftwareMicState:NO mode:MicrophoneMode_InternalNoise];
			break;
			
		case NDSInputID_Paddle:
			[cdsController setPaddleAdjust:0];
			break;
			
		default:
			[cdsController setControllerState:NO controlID:inputID[button]];
			break;
	}
}

- (oneway void)didTouchScreenPoint:(OEIntPoint)aPoint
{
	BOOL isTouchPressed = NO;
	NSInteger dispMode = [self displayMode];
	
	switch (dispMode)
	{
		case ClientDisplayMode_Main:
			isTouchPressed = NO; // Reject touch input if showing only the main screen.
			break;
			
		case ClientDisplayMode_Touch:
			isTouchPressed = YES;
			break;
			
		case ClientDisplayMode_Dual:
			isTouchPressed = YES;
			aPoint.y -= GPU_FRAMEBUFFER_NATIVE_HEIGHT; // Normalize the y-coordinate to the DS.
			break;
			
		default:
			return;
			break;
	}
	
	// Constrain the touch point to the DS dimensions.
	if (aPoint.x < 0)
	{
		aPoint.x = 0;
	}
	else if (aPoint.x > (GPU_FRAMEBUFFER_NATIVE_WIDTH - 1))
	{
		aPoint.x = (GPU_FRAMEBUFFER_NATIVE_WIDTH - 1);
	}
	
	if (aPoint.y < 0)
	{
		aPoint.y = 0;
	}
	else if (aPoint.y > (GPU_FRAMEBUFFER_NATIVE_HEIGHT - 1))
	{
		aPoint.y = (GPU_FRAMEBUFFER_NATIVE_HEIGHT - 1);
	}
	
	touchLocation = NSMakePoint(aPoint.x, aPoint.y);
	[cdsController setTouchState:isTouchPressed location:touchLocation];
}

- (oneway void)didReleaseTouch
{
	[cdsController setTouchState:NO location:touchLocation];
}

- (void)changeDisplayMode
{
	switch (displayMode)
	{
		case ClientDisplayMode_Main:
			[self setDisplayMode:ClientDisplayMode_Touch];
			break;
			
		case ClientDisplayMode_Touch:
			[self setDisplayMode:ClientDisplayMode_Dual];
			break;
			
		case ClientDisplayMode_Dual:
			[self setDisplayMode:ClientDisplayMode_Main];
			break;
			
		default:
			return;
			break;
	}
}

@end
