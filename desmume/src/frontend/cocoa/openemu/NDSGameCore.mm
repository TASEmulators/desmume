/*
	Copyright (C) 2012-2022 DeSmuME team

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
#import "OESoundInterface.h"
#import "OENDSSystemResponderClient.h"

#import "../cocoa_cheat.h"
#import "../cocoa_globals.h"
#import "../cocoa_file.h"
#import "../cocoa_firmware.h"
#import "../cocoa_GPU.h"
#import "../cocoa_input.h"
#import "../ClientDisplayView.h"
#import "../ClientInputHandler.h"

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
	unfairlockDisplayMode = apple_unfairlock_create();
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
	cdsGPU = [[CocoaDSGPU alloc] init];
	[cdsGPU setRender3DThreads:0]; // Pass 0 to automatically set the number of rendering threads
	[cdsGPU setRender3DRenderingEngine:CORE3DLIST_SWRASTERIZE];
	[cdsGPU setGpuScale:1];
	[cdsGPU setGpuColorFormat:NDSColorFormat_BGR666_Rev];
	
	// Set up the DS controller
	cdsController = [[CocoaDSController alloc] init];
	
	// Set up the cheat system
	cdsCheats = [[CocoaDSCheatManager alloc] init];
	[cdsCheats setRwlockCoreExecute:&rwlockCoreExecute];
	addedCheatsDict = [[NSMutableDictionary alloc] initWithCapacity:128];
	
	// Set up the DS firmware using the internal firmware
	cdsFirmware = [[CocoaDSFirmware alloc] init];
	[cdsFirmware applySettings];
	
	// Set up the sound core
	CommonSettings.spu_advanced = true;
	CommonSettings.spuInterpolationMode = SPUInterpolation_Cosine;
	CommonSettings.SPU_sync_mode = SPU_SYNC_MODE_SYNCHRONOUS;
	CommonSettings.SPU_sync_method = SPU_SYNC_METHOD_N;
	openEmuSoundInterfaceBuffer = [self audioBufferAtIndex:0];
	
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
	[cdsCheats release];
	[cdsController release];
	[cdsGPU release];
	[cdsFirmware release];
	
	pthread_rwlock_destroy(&rwlockCoreExecute);
	apple_unfairlock_destroy(unfairlockDisplayMode);
	
	[super dealloc];
}

- (NSInteger) displayMode
{
	apple_unfairlock_lock(unfairlockDisplayMode);
	const NSInteger theMode = displayMode;
	apple_unfairlock_unlock(unfairlockDisplayMode);
	
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
	
	apple_unfairlock_lock(unfairlockDisplayMode);
	displayMode = theMode;
	displayRect = newDisplayRect;
	displayAspectRatio = newDisplayAspectRatio;
	apple_unfairlock_unlock(unfairlockDisplayMode);
}

#pragma mark - Plug-in Support

- (BOOL)supportsRewinding
{
	return NO;
}

#pragma mark - Starting

/*!
 * @method loadFileAtPath:error
 * @discussion
 * Try to load a ROM and return NO if it fails, or YES if it succeeds.
 * You can do any setup you want here.
 */
- (BOOL)loadFileAtPath:(NSString *)path error:(NSError **)error
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

#pragma mark - Stopping

/*!
 * @method stopEmulation
 * @discussion
 * Shut down the core. In non-debugging modes of core execution,
 * the process will be exit immediately after, so you don't need to
 * free any CPU or OpenGL resources.
 *
 * The OpenGL context is available in this method.
 */
//- (void)stopEmulation;
//- (void)stopEmulationWithCompletionHandler:(void(^)(void))completionHandler;

#pragma mark - Execution

/*!
 * @property frameInterval
 * @abstract The ideal *frequency* (in Hz) of -executeFrame calls when rate=1.0.
 * This property is only read at the start and cannot be changed.
 * @discussion Even though the property name and type indicate that
 * a *period* in seconds should be returned (i.e. 1/60.0 ~= 0.01667 for 60 FPS execution),
 * this method shall return a frequency in Hz instead (the inverse of that period).
 * This naming mistake must be mantained for backwards compatibility.
 */
- (NSTimeInterval)frameInterval
{
	return DS_FRAMES_PER_SECOND;
}

/*!
 * @property rate
 * @discussion
 * The rate the game is currently running at. Generally 1.0.
 * If 0, the core is paused.
 * If >1.0, the core is fast-forwarding and -executeFrame will be called more often.
 * Values <1.0 are not expected.
 *
 * There is no need to check this property if your core does all work inside -executeFrame.
 */
//@property (nonatomic, assign) float rate;

/*!
 * @method executeFrame
 * @discussion
 * Called every 1/(rate*frameInterval) seconds by -runGameLoop:.
 * The core should produce 1 frameInterval worth of audio and can output 1 frame of video.
 * If the game core option OEGameCoreOptionCanSkipFrames is set, the property shouldSkipFrame may be YES.
 * In this case the core can read from videoBuffer but must not write to it. All work done to render video can be skipped.
 */
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

/*!
 * @method resetEmulation
 * @abstract Presses the reset button on the console.
 */
- (void)resetEmulation
{
	pthread_rwlock_wrlock(&rwlockCoreExecute);
	NDS_Reset();
	pthread_rwlock_unlock(&rwlockCoreExecute);
	execute = true;
}

/*!
 * @method beginPausedExecution
 * @abstract Run the thread without appearing to execute the game.
 * @discussion OpenEmu may ask the core to save the game, etc. even though it is paused.
 * Some cores need to run their -executeFrame to process the save message (e.g. Mupen).
 * Call this method from inside the save method to handle this case without disturbing the UI.
 */
//- (void)beginPausedExecution;
//- (void)endPausedExecution;

#pragma mark - Video

/*!
 * @method getVideoBufferWithHint:
 * @param hint If possible, use 'hint' as the video buffer for this frame.
 * @discussion
 * Called before each -executeFrame call. The method should return
 * a video buffer containing 'bufferSize' packed pixels, and -executeFrame
 * should draw into this buffer. If 'hint' is set, using that as the video
 * buffer may be faster. Besides that, returning the same buffer each time
 * may be faster.
 */
- (const void *)getVideoBufferWithHint:(void *)hint
{
	return GPU->GetDisplayInfo().masterCustomBuffer;
}

/*!
 * @method tryToResizeVideoTo:
 * @discussion
 * If the core can natively draw at any resolution, change the resolution
 * to 'size' and return YES. Otherwise, return NO. If YES, the next call to
 * -executeFrame will have a newly sized framebuffer.
 * It is assumed that only 3D cores can do this.
 */
//- (BOOL)tryToResizeVideoTo:(OEIntSize)size;

/*!
 * @property gameCoreRendering
 * @discussion
 * What kind of 3D API the core requires, or none.
 * Defaults to 2D.
 */
- (OEGameCoreRendering)gameCoreRendering
{
	return OEGameCoreRendering2DVideo;
}

/*!
 * @property hasAlternateRenderingThread
 * @abstract If the core starts another thread to do 3D operations on.
 * @discussion
 * 3D -
 * OE will provide one extra GL context for this thread to avoid corruption
 * of the main context. More than one rendering thread is not supported.
 */
- (BOOL)hasAlternateRenderingThread
{
	return NO;
}

/*!
 * @property needsDoubleBufferedFBO
 * @abstract If the game flickers when rendering directly to IOSurface.
 * @discussion
 * 3D -
 * Some cores' OpenGL renderers accidentally cause the IOSurface to update early,
 * either by calling glFlush() or through GL driver bugs. This implements a workaround.
 * Used by Mupen64Plus.
 */
- (BOOL)needsDoubleBufferedFBO
{
	return NO;
}

/*!
 * @property bufferSize
 * @discussion
 * 2D -
 * The size in pixels to allocate the framebuffer at.
 * Cores should output at their largest native size, including overdraw, without aspect ratio correction.
 * 3D -
 * The initial size to allocate the framebuffer at.
 * The user may decide to resize it later, but OE will try to request new sizes at the same aspect ratio as bufferSize.
 */
- (OEIntSize)bufferSize
{
	return OEIntSizeMake(GPU_FRAMEBUFFER_NATIVE_WIDTH, GPU_FRAMEBUFFER_NATIVE_HEIGHT * 2);
}

/*!
 * @property screenRect
 * @discussion
 * The rect inside the framebuffer showing the currently displayed picture,
 * not including overdraw, but without aspect ratio correction.
 * Aspect ratio correction is not used for 3D.
 */
- (OEIntRect)screenRect
{
	apple_unfairlock_lock(unfairlockDisplayMode);
	const OEIntRect theRect = displayRect;
	apple_unfairlock_unlock(unfairlockDisplayMode);
	
	return theRect;
}

/*!
 * @property aspectSize
 * @discussion
 * The size at the display aspect ratio (DAR) of the picture.
 * The actual pixel values are not used; only the ratio is used.
 * Aspect ratio correction is not used for 3D.
 */
- (OEIntSize)aspectSize
{
	apple_unfairlock_lock(unfairlockDisplayMode);
	const OEIntSize theAspectRatio = displayAspectRatio;
	apple_unfairlock_unlock(unfairlockDisplayMode);
	
	return theAspectRatio;
}

/*!
 * @property internalPixelFormat
 * @discussion
 * The 'internalFormat' parameter to glTexImage2D, used to create the framebuffer.
 * Defaults to GL_RGB (sometimes GL_SRGB8). You probably do not need to override this.
 * Ignored for 3D cores.
 */
- (GLenum)internalPixelFormat
{
	return GL_RGBA;
}

/*!
 * @property pixelFormat
 * @discussion
 * The 'type' parameter to glTexImage2D, used to create the framebuffer.
 * GL_BGRA is preferred, but avoid doing any conversions inside the core.
 * Ignored for 3D cores.
 */
- (GLenum)pixelType
{
	return GL_UNSIGNED_INT_8_8_8_8_REV;
}

/*!
 * @property pixelFormat
 * @discussion
 * The 'format' parameter to glTexImage2D, used to create the framebuffer.
 * GL_UNSIGNED_SHORT_1_5_5_5_REV or GL_UNSIGNED_INT_8_8_8_8_REV are preferred, but
 * avoid doing any conversions inside the core.
 * Ignored for 3D cores.
 */
- (GLenum)pixelFormat
{
	return GL_RGBA;
}

/*!
 * @property bytesPerRow
 * @discussion
 * If the core outputs pixels with custom padding, and that padding cannot be expressed
 * as overscan with bufferSize, you can implement this to return the distance between
 * the first two rows in bytes.
 * Ignored for 3D cores.
 */
//@property(readonly) NSInteger   bytesPerRow;

/*!
 * @property shouldSkipFrame
 * @abstract See -executeFrame.
 */
//@property(assign) BOOL shouldSkipFrame;

#pragma mark - Audio

//- (void)getAudioBuffer:(void *)buffer frameCount:(NSUInteger)frameCount bufferIndex:(NSUInteger)index;

/**
 * Returns the OEAudioBuffer associated to the specified audio track.
 * @discussion A concrete game core can override this method to customize
 *      its audio buffering system. OpenEmu never calls the -write:maxLength: method
 *      of a buffer returned by this method.
 * @param index The audio track index.
 * @returns The audio buffer from which to read audio samples.
 */
//- (id<OEAudioBuffer>)audioBufferAtIndex:(NSUInteger)index;

/*!
 * @property audioBufferCount
 * @discussion
 * Defaults to 1. Return a value other than 1 if the core can export
 * multiple audio tracks. There is currently not much need for this.
 */
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

- (double)audioSampleRateForBuffer:(NSUInteger)buffer
{
	return [self audioSampleRate];
}

/*!
 * @method audioBufferSizeForBuffer:
 * Returns the number of audio frames that are enquequed by the game core into
 * the ring buffer every video frame.
 * @param buffer The index of the buffer.
 * @note If this method is not overridden by a concrete game core, it
 * returns a very conservative frame count.
 */
- (NSUInteger)audioBufferSizeForBuffer:(NSUInteger)buffer
{
	return (NSUInteger)SPU_BUFFER_BYTES;
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

//- (void)setRandomByte;

#pragma mark - Save state - Optional

//- (NSData *)serializeStateWithError:(NSError **)outError;
//- (BOOL)deserializeState:(NSData *)state withError:(NSError **)outError;

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

#pragma mark - Display Mode - Optional

/** An array describing the available display mode options and the
 *  appearance of the menu used to select them.
 *  @discussion Each NSDictionary in the array corresponds to an item
 *    in the Display Modes menu.
 *    Each item can represent one of these things, depending on the keys
 *    contained in the dictionary:
 *     - A label
 *     - A separator
 *     - A binary (toggleable) option
 *     - An option mutually exclusive with other options
 *     - A nested group of options (which appears as a submenu)
 *    See OEGameCoreController.h for a detailed discussion of the keys contained
 *    in each item dictionary. */
//@property(readonly) NSArray<NSDictionary<NSString *, id> *> *displayModes;

/** Change display mode.
 *  @param displayMode The name of the display mode to enable or disable, as
 *    specified in its OEGameCoreDisplayModeNameKey key. */
//- (void)changeDisplayWithMode:(NSString *)displayMode;

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

#pragma mark - Internal

// There should be no need to override these methods.
/*!
 * @method runGameLoop:
 * @discussion
 * Cores may implement this if they wish to control their entire event loop.
 * This is not recommended.
 */
//- (void)runGameLoop:(id)anArgument;

/*!
 * @method startEmulation
 * @discussion
 * A method called on OEGameCore after -setupEmulation and
 * before -executeFrame. You may implement it for organizational
 * purposes but it is not necessary.
 *
 * The OpenGL context is available in this method.
 */
- (void)startEmulation
{
	[cdsController startHardwareMicDevice];
	[cdsController setHardwareMicMute:NO];
	[cdsController setHardwareMicPause:NO];
	
	[super startEmulation];
}

/*!
 * @method setupEmulation
 * @discussion
 * Try to setup emulation as much as possible before the UI appears.
 * Audio/video properties don't need to be valid before this method, but
 * do need to be valid after.
 *
 * It's not necessary to implement this, all setup can be done in loadFileAtPath
 * or in the first executeFrame. But you're more likely to run into OE bugs that way.
 */
//- (void)setupEmulation;

//- (void)didStopEmulation;
//- (void)runStartUpFrameWithCompletionHandler:(void(^)(void))handler;

//- (void)stopEmulationWithCompletionHandler:(void(^)(void))completionHandler;

/*!
 * @property pauseEmulation
 * @discussion Pauses the emulator "nicely".
 * When set to YES, pauses emulation. When set to NO,
 * resets the rate to whatever it previously was.
 * The FPS limiter will stop, causing your rendering thread to pause.
 * You should probably not override this.
 */
- (void)setPauseEmulation:(BOOL)pauseEmulation
{
	[cdsController setHardwareMicPause:pauseEmulation];
	
	[super setPauseEmulation:pauseEmulation];
}

/// When didExecute is called, will be the next wakeup time.
//@property (nonatomic, readonly) NSTimeInterval nextFrameTime;

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

@end
