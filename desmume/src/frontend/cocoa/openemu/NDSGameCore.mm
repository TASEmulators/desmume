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

#define DISPLAYMODE_STATEBIT_CHECK(_STATEBITS_, _ID_) ((((_STATEBITS_) >> (_ID_)) & 1) != 0)
#define DISPLAYMODE_STATEBITGROUP_CLEAR(_STATEBITS_, _BITMASK_) _STATEBITS_ = (((_STATEBITS_) | (_BITMASK_)) ^ (_BITMASK_))


static const NDSDisplayMenuItem kDisplayModeItem[NDSDisplayOptionID_Count] = {
	{ @NDSDISPLAYMODE_NAMEKEY_MODE_DUALSCREEN, @NDSDISPLAYMODE_PREFKEY_DISPLAYMODE },
	{ @NDSDISPLAYMODE_NAMEKEY_MODE_MAIN,       @NDSDISPLAYMODE_PREFKEY_DISPLAYMODE },
	{ @NDSDISPLAYMODE_NAMEKEY_MODE_TOUCH,      @NDSDISPLAYMODE_PREFKEY_DISPLAYMODE },
	
	{ @NDSDISPLAYMODE_NAMEKEY_ROTATION_0,   @NDSDISPLAYMODE_PREFKEY_ROTATION },
	{ @NDSDISPLAYMODE_NAMEKEY_ROTATION_90,  @NDSDISPLAYMODE_PREFKEY_ROTATION },
	{ @NDSDISPLAYMODE_NAMEKEY_ROTATION_180, @NDSDISPLAYMODE_PREFKEY_ROTATION },
	{ @NDSDISPLAYMODE_NAMEKEY_ROTATION_270, @NDSDISPLAYMODE_PREFKEY_ROTATION },
	
	{ @NDSDISPLAYMODE_NAMEKEY_LAYOUT_VERTICAL,     @NDSDISPLAYMODE_PREFKEY_LAYOUT },
	{ @NDSDISPLAYMODE_NAMEKEY_LAYOUT_HORIZONTAL,   @NDSDISPLAYMODE_PREFKEY_LAYOUT },
	{ @NDSDISPLAYMODE_NAMEKEY_LAYOUT_HYBRID_2_1,   @NDSDISPLAYMODE_PREFKEY_LAYOUT },
	{ @NDSDISPLAYMODE_NAMEKEY_LAYOUT_HYBRID_16_9,  @NDSDISPLAYMODE_PREFKEY_LAYOUT },
	{ @NDSDISPLAYMODE_NAMEKEY_LAYOUT_HYBRID_16_10, @NDSDISPLAYMODE_PREFKEY_LAYOUT },
	
	{ @NDSDISPLAYMODE_NAMEKEY_DISPLAYORDER_MAIN,  @NDSDISPLAYMODE_PREFKEY_ORDER },
	{ @NDSDISPLAYMODE_NAMEKEY_DISPLAYORDER_TOUCH, @NDSDISPLAYMODE_PREFKEY_ORDER },
	
	{ @NDSDISPLAYMODE_NAMEKEY_SEPARATION_0,   @NDSDISPLAYMODE_PREFKEY_SEPARATION },
	{ @NDSDISPLAYMODE_NAMEKEY_SEPARATION_50,  @NDSDISPLAYMODE_PREFKEY_SEPARATION },
	{ @NDSDISPLAYMODE_NAMEKEY_SEPARATION_100, @NDSDISPLAYMODE_PREFKEY_SEPARATION },
	{ @NDSDISPLAYMODE_NAMEKEY_SEPARATION_150, @NDSDISPLAYMODE_PREFKEY_SEPARATION },
	{ @NDSDISPLAYMODE_NAMEKEY_SEPARATION_200, @NDSDISPLAYMODE_PREFKEY_SEPARATION },
	
	{ @NDSDISPLAYMODE_NAMEKEY_VIDEOSOURCEMAIN_NONE,       @NDSDISPLAYMODE_PREFKEY_VIDEOSOURCE_MAIN },
	{ @NDSDISPLAYMODE_NAMEKEY_VIDEOSOURCEMAIN_NDS,        @NDSDISPLAYMODE_PREFKEY_VIDEOSOURCE_MAIN },
	{ @NDSDISPLAYMODE_NAMEKEY_VIDEOSOURCEMAIN_FORCEMAIN,  @NDSDISPLAYMODE_PREFKEY_VIDEOSOURCE_MAIN },
	{ @NDSDISPLAYMODE_NAMEKEY_VIDEOSOURCEMAIN_FORCETOUCH, @NDSDISPLAYMODE_PREFKEY_VIDEOSOURCE_MAIN },
	
	{ @NDSDISPLAYMODE_NAMEKEY_VIDEOSOURCETOUCH_NONE,       @NDSDISPLAYMODE_PREFKEY_VIDEOSOURCE_TOUCH },
	{ @NDSDISPLAYMODE_NAMEKEY_VIDEOSOURCETOUCH_NDS,        @NDSDISPLAYMODE_PREFKEY_VIDEOSOURCE_TOUCH },
	{ @NDSDISPLAYMODE_NAMEKEY_VIDEOSOURCETOUCH_FORCEMAIN,  @NDSDISPLAYMODE_PREFKEY_VIDEOSOURCE_TOUCH },
	{ @NDSDISPLAYMODE_NAMEKEY_VIDEOSOURCETOUCH_FORCETOUCH, @NDSDISPLAYMODE_PREFKEY_VIDEOSOURCE_TOUCH }
};

static const uint64_t kDisplayModeStatesDefault = (1 << NDSDisplayOptionID_Mode_DualScreen) |
                                                  (1 << NDSDisplayOptionID_Rotation_0) |
                                                  (1 << NDSDisplayOptionID_Layout_Vertical) |
                                                  (1 << NDSDisplayOptionID_Order_MainFirst) |
                                                  (1 << NDSDisplayOptionID_Separation_0) |
                                                  (1 << NDSDisplayOptionID_VideoSourceMain_NDS) |
                                                  (1 << NDSDisplayOptionID_VideoSourceTouch_NDS);

volatile bool execute = true;

@implementation NDSGameCore

@synthesize cdsController;
@synthesize cdsGPU;
@synthesize cdsFirmware;
@synthesize cdsCheats;
@dynamic ndsDisplayMode;

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
	ndsDisplayMode = kDisplayModeStatesDefault;
	_displayRect = OEIntRectMake(0, 0, GPU_FRAMEBUFFER_NATIVE_WIDTH, GPU_FRAMEBUFFER_NATIVE_HEIGHT * 2);
	_displayAspectRatio = OEIntSizeMake(2, 3);
	
	_displayModeIDFromString = [[NSDictionary alloc] initWithObjectsAndKeys:
								[NSNumber numberWithInteger:NDSDisplayOptionID_Mode_DualScreen], kDisplayModeItem[NDSDisplayOptionID_Mode_DualScreen].nameKey,
								[NSNumber numberWithInteger:NDSDisplayOptionID_Mode_Main],       kDisplayModeItem[NDSDisplayOptionID_Mode_Main].nameKey,
								[NSNumber numberWithInteger:NDSDisplayOptionID_Mode_Touch],      kDisplayModeItem[NDSDisplayOptionID_Mode_Touch].nameKey,
								
								[NSNumber numberWithInteger:NDSDisplayOptionID_Rotation_0],   kDisplayModeItem[NDSDisplayOptionID_Rotation_0].nameKey,
								[NSNumber numberWithInteger:NDSDisplayOptionID_Rotation_90],  kDisplayModeItem[NDSDisplayOptionID_Rotation_90].nameKey,
								[NSNumber numberWithInteger:NDSDisplayOptionID_Rotation_180], kDisplayModeItem[NDSDisplayOptionID_Rotation_180].nameKey,
								[NSNumber numberWithInteger:NDSDisplayOptionID_Rotation_270], kDisplayModeItem[NDSDisplayOptionID_Rotation_270].nameKey,
								
								[NSNumber numberWithInteger:NDSDisplayOptionID_Layout_Vertical],     kDisplayModeItem[NDSDisplayOptionID_Layout_Vertical].nameKey,
								[NSNumber numberWithInteger:NDSDisplayOptionID_Layout_Horizontal],   kDisplayModeItem[NDSDisplayOptionID_Layout_Horizontal].nameKey,
								[NSNumber numberWithInteger:NDSDisplayOptionID_Layout_Hybrid_2_1],   kDisplayModeItem[NDSDisplayOptionID_Layout_Hybrid_2_1].nameKey,
								[NSNumber numberWithInteger:NDSDisplayOptionID_Layout_Hybrid_16_9],  kDisplayModeItem[NDSDisplayOptionID_Layout_Hybrid_16_9].nameKey,
								[NSNumber numberWithInteger:NDSDisplayOptionID_Layout_Hybrid_16_10], kDisplayModeItem[NDSDisplayOptionID_Layout_Hybrid_16_10].nameKey,
								
								[NSNumber numberWithInteger:NDSDisplayOptionID_Order_MainFirst],  kDisplayModeItem[NDSDisplayOptionID_Order_MainFirst].nameKey,
								[NSNumber numberWithInteger:NDSDisplayOptionID_Order_TouchFirst], kDisplayModeItem[NDSDisplayOptionID_Order_TouchFirst].nameKey,
								
								[NSNumber numberWithInteger:NDSDisplayOptionID_Separation_0],   kDisplayModeItem[NDSDisplayOptionID_Separation_0].nameKey,
								[NSNumber numberWithInteger:NDSDisplayOptionID_Separation_50],  kDisplayModeItem[NDSDisplayOptionID_Separation_50].nameKey,
								[NSNumber numberWithInteger:NDSDisplayOptionID_Separation_100], kDisplayModeItem[NDSDisplayOptionID_Separation_100].nameKey,
								[NSNumber numberWithInteger:NDSDisplayOptionID_Separation_150], kDisplayModeItem[NDSDisplayOptionID_Separation_150].nameKey,
								[NSNumber numberWithInteger:NDSDisplayOptionID_Separation_200], kDisplayModeItem[NDSDisplayOptionID_Separation_200].nameKey,
								
								[NSNumber numberWithInteger:NDSDisplayOptionID_VideoSourceMain_None],       kDisplayModeItem[NDSDisplayOptionID_VideoSourceMain_None].nameKey,
								[NSNumber numberWithInteger:NDSDisplayOptionID_VideoSourceMain_NDS],        kDisplayModeItem[NDSDisplayOptionID_VideoSourceMain_NDS].nameKey,
								[NSNumber numberWithInteger:NDSDisplayOptionID_VideoSourceMain_ForceMain],  kDisplayModeItem[NDSDisplayOptionID_VideoSourceMain_ForceMain].nameKey,
								[NSNumber numberWithInteger:NDSDisplayOptionID_VideoSourceMain_ForceTouch], kDisplayModeItem[NDSDisplayOptionID_VideoSourceMain_ForceTouch].nameKey,
								
								[NSNumber numberWithInteger:NDSDisplayOptionID_VideoSourceTouch_None],       kDisplayModeItem[NDSDisplayOptionID_VideoSourceTouch_None].nameKey,
								[NSNumber numberWithInteger:NDSDisplayOptionID_VideoSourceTouch_NDS],        kDisplayModeItem[NDSDisplayOptionID_VideoSourceTouch_NDS].nameKey,
								[NSNumber numberWithInteger:NDSDisplayOptionID_VideoSourceTouch_ForceMain],  kDisplayModeItem[NDSDisplayOptionID_VideoSourceTouch_ForceMain].nameKey,
								[NSNumber numberWithInteger:NDSDisplayOptionID_VideoSourceTouch_ForceTouch], kDisplayModeItem[NDSDisplayOptionID_VideoSourceTouch_ForceTouch].nameKey,
								nil];
	
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
	
	[_displayModeIDFromString release];
	
	pthread_rwlock_destroy(&rwlockCoreExecute);
	apple_unfairlock_destroy(unfairlockDisplayMode);
	
	[super dealloc];
}

- (uint64_t) ndsDisplayMode
{
	apple_unfairlock_lock(unfairlockDisplayMode);
	const uint64_t displayModeStates = ndsDisplayMode;
	apple_unfairlock_unlock(unfairlockDisplayMode);
	
	return displayModeStates;
}

- (void) setNdsDisplayMode:(uint64_t)displayModeStates
{
	OEIntRect newDisplayRect;
	OEIntSize newDisplayAspectRatio;
	
	if ( DISPLAYMODE_STATEBIT_CHECK(displayModeStates, NDSDisplayOptionID_Mode_DualScreen) )
	{
		newDisplayRect = OEIntRectMake(0, 0, GPU_FRAMEBUFFER_NATIVE_WIDTH, GPU_FRAMEBUFFER_NATIVE_HEIGHT * 2);
		newDisplayAspectRatio = OEIntSizeMake(2, 3);
	}
	else if ( DISPLAYMODE_STATEBIT_CHECK(displayModeStates, NDSDisplayOptionID_Mode_Main) )
	{
		newDisplayRect = OEIntRectMake(0, 0, GPU_FRAMEBUFFER_NATIVE_WIDTH, GPU_FRAMEBUFFER_NATIVE_HEIGHT);
		newDisplayAspectRatio = OEIntSizeMake(4, 3);
	}
	else if ( DISPLAYMODE_STATEBIT_CHECK(displayModeStates, NDSDisplayOptionID_Mode_Touch) )
	{
		newDisplayRect = OEIntRectMake(0, GPU_FRAMEBUFFER_NATIVE_HEIGHT + 1, GPU_FRAMEBUFFER_NATIVE_WIDTH, GPU_FRAMEBUFFER_NATIVE_HEIGHT);
		newDisplayAspectRatio = OEIntSizeMake(4, 3);
	}
	else
	{
		return;
	}
	
	apple_unfairlock_lock(unfairlockDisplayMode);
	ndsDisplayMode = displayModeStates;
	_displayRect = newDisplayRect;
	_displayAspectRatio = newDisplayAspectRatio;
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
	
	// Set the default options.
	// Have to do it now because displayModeInfo is only available now -- not at this object's init.
	NSDictionary<NSString *, id> *userDefaultsDisplayMode = nil;
	if ([self respondsToSelector:@selector(displayModeInfo)])
	{
		userDefaultsDisplayMode = [self displayModeInfo];
	}
	
#define GENDISPLAYMODESTATE(_PREFKEY_, _DEFAULTID_) \
	( ([userDefaultsDisplayMode objectForKey:(_PREFKEY_)] == nil) || ([_displayModeIDFromString objectForKey:(NSString *)[userDefaultsDisplayMode objectForKey:(_PREFKEY_)]] == nil) ) ? \
		(1 << (_DEFAULTID_)) : \
		(1 << [(NSNumber *)[_displayModeIDFromString objectForKey:(NSString *)[userDefaultsDisplayMode objectForKey:(_PREFKEY_)]] integerValue])
	
	uint64_t newDisplayModeStates = 0;
	newDisplayModeStates |= GENDISPLAYMODESTATE( @NDSDISPLAYMODE_PREFKEY_DISPLAYMODE, NDSDisplayOptionID_Mode_DualScreen );
	newDisplayModeStates |= GENDISPLAYMODESTATE( @NDSDISPLAYMODE_PREFKEY_ROTATION, NDSDisplayOptionID_Rotation_0 );
	newDisplayModeStates |= GENDISPLAYMODESTATE( @NDSDISPLAYMODE_PREFKEY_LAYOUT, NDSDisplayOptionID_Layout_Vertical );
	newDisplayModeStates |= GENDISPLAYMODESTATE( @NDSDISPLAYMODE_PREFKEY_ORDER, NDSDisplayOptionID_Order_MainFirst );
	newDisplayModeStates |= GENDISPLAYMODESTATE( @NDSDISPLAYMODE_PREFKEY_SEPARATION, NDSDisplayOptionID_Separation_0 );
	newDisplayModeStates |= GENDISPLAYMODESTATE( @NDSDISPLAYMODE_PREFKEY_VIDEOSOURCE_MAIN, NDSDisplayOptionID_VideoSourceMain_NDS );
	newDisplayModeStates |= GENDISPLAYMODESTATE( @NDSDISPLAYMODE_PREFKEY_VIDEOSOURCE_TOUCH, NDSDisplayOptionID_VideoSourceTouch_NDS );
	
	[self setNdsDisplayMode:newDisplayModeStates];
	
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
	const OEIntRect theRect = _displayRect;
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
	const OEIntSize theAspectRatio = _displayAspectRatio;
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

- (NSDictionary<NSString *, id> *) generateDisplayModeItemByID:(NDSDisplayOptionID)optionID states:(const uint64_t)states
{
	if ( (optionID < 0) || (optionID >= NDSDisplayOptionID_Count) )
	{
		return nil;
	}
	
	return [NSDictionary dictionaryWithObjectsAndKeys:
			kDisplayModeItem[optionID].nameKey, OEGameCoreDisplayModeNameKey,
			kDisplayModeItem[optionID].prefKey, OEGameCoreDisplayModePrefKeyNameKey,
			[NSNumber numberWithBool:DISPLAYMODE_STATEBIT_CHECK(states, optionID) ? YES : NO], OEGameCoreDisplayModeStateKey,
			nil];
}

- (NSArray<NSDictionary<NSString *, id> *> *) displayModes
{
	const uint64_t currentDisplayMode = [self ndsDisplayMode];
	
	// Generate each display option submenu.
	NSArray< NSDictionary<NSString *, id> *> *displayModeMenu =
	[NSArray arrayWithObjects:
	 [self generateDisplayModeItemByID:NDSDisplayOptionID_Mode_DualScreen states:currentDisplayMode],
	 [self generateDisplayModeItemByID:NDSDisplayOptionID_Mode_Main states:currentDisplayMode],
	 [self generateDisplayModeItemByID:NDSDisplayOptionID_Mode_Touch states:currentDisplayMode],
	 nil];
	
	NSArray< NSDictionary<NSString *, id> *> *displayRotationMenu =
	[NSArray arrayWithObjects:
	 [self generateDisplayModeItemByID:NDSDisplayOptionID_Rotation_0 states:currentDisplayMode],
	 [self generateDisplayModeItemByID:NDSDisplayOptionID_Rotation_90 states:currentDisplayMode],
	 [self generateDisplayModeItemByID:NDSDisplayOptionID_Rotation_180 states:currentDisplayMode],
	 [self generateDisplayModeItemByID:NDSDisplayOptionID_Rotation_270 states:currentDisplayMode],
	 nil];
	
	NSArray< NSDictionary<NSString *, id> *> *displayLayoutMenu =
	[NSArray arrayWithObjects:
	 [self generateDisplayModeItemByID:NDSDisplayOptionID_Layout_Vertical states:currentDisplayMode],
	 [self generateDisplayModeItemByID:NDSDisplayOptionID_Layout_Horizontal states:currentDisplayMode],
	 [self generateDisplayModeItemByID:NDSDisplayOptionID_Layout_Hybrid_2_1 states:currentDisplayMode],
	 [self generateDisplayModeItemByID:NDSDisplayOptionID_Layout_Hybrid_16_9 states:currentDisplayMode],
	 [self generateDisplayModeItemByID:NDSDisplayOptionID_Layout_Hybrid_16_10 states:currentDisplayMode],
	 nil];
	
	NSArray< NSDictionary<NSString *, id> *> *displayOrderMenu =
	[NSArray arrayWithObjects:
	 [self generateDisplayModeItemByID:NDSDisplayOptionID_Order_MainFirst states:currentDisplayMode],
	 [self generateDisplayModeItemByID:NDSDisplayOptionID_Order_TouchFirst states:currentDisplayMode],
	 nil];
	
	NSArray< NSDictionary<NSString *, id> *> *displaySeparationMenu =
	[NSArray arrayWithObjects:
	 [self generateDisplayModeItemByID:NDSDisplayOptionID_Separation_0 states:currentDisplayMode],
	 [self generateDisplayModeItemByID:NDSDisplayOptionID_Separation_50 states:currentDisplayMode],
	 [self generateDisplayModeItemByID:NDSDisplayOptionID_Separation_100 states:currentDisplayMode],
	 [self generateDisplayModeItemByID:NDSDisplayOptionID_Separation_150 states:currentDisplayMode],
	 [self generateDisplayModeItemByID:NDSDisplayOptionID_Separation_200 states:currentDisplayMode],
	 nil];
	 
	NSArray< NSDictionary<NSString *, id> *> *displayVideoSourceMenu =
	[NSArray arrayWithObjects:
	 [self generateDisplayModeItemByID:NDSDisplayOptionID_VideoSourceMain_None states:currentDisplayMode],
	 [self generateDisplayModeItemByID:NDSDisplayOptionID_VideoSourceMain_NDS states:currentDisplayMode],
	 [self generateDisplayModeItemByID:NDSDisplayOptionID_VideoSourceMain_ForceMain states:currentDisplayMode],
	 [self generateDisplayModeItemByID:NDSDisplayOptionID_VideoSourceMain_ForceTouch states:currentDisplayMode],
	 [NSDictionary dictionaryWithObjectsAndKeys:@"---", OEGameCoreDisplayModeSeparatorItemKey, nil],
	 [self generateDisplayModeItemByID:NDSDisplayOptionID_VideoSourceTouch_None states:currentDisplayMode],
	 [self generateDisplayModeItemByID:NDSDisplayOptionID_VideoSourceTouch_NDS states:currentDisplayMode],
	 [self generateDisplayModeItemByID:NDSDisplayOptionID_VideoSourceTouch_ForceMain states:currentDisplayMode],
	 [self generateDisplayModeItemByID:NDSDisplayOptionID_VideoSourceTouch_ForceTouch states:currentDisplayMode],
	 nil];
	
	// Add each submenu to the menu descriptor.
	//
	// You may wonder why we're generating the menu from scratch every time. The reason is because
	// OpenEmu wants to have a full immutable copy of the menu descriptor so that it can use it for
	// its Next/Last Display Mode switching feature. Because of this, we can't just generate a
	// single menu descriptor at init time, modify it as needed, and then send OpenEmu the pointer
	// to our menu descriptor. as OpenEmu can modify it in some unpredictable ways. We would have
	// to send a copy to keep our internal working menu descriptor from getting borked.
	//
	// Maintaining a mutable version of the menu descriptor for our internal usage is a pain, as
	// it involves a bunch of hash table lookups and calls to [NSString isEqualToString:], among
	// many other annoyances with dealing with NSMutableDictionary directly. Since we have to send
	// OpenEmu a copy of the menu descriptor anyways, it ends up being easier to just use a single
	// integer ID, NDSDisplayOptionID in our case, to keep track of all of the data associations
	// and then use that ID to generate the menu.
	/*
	NSArray< NSDictionary<NSString *, id> *> *newDisplayModeMenuDescription =
	[[NSArray alloc] initWithObjects:
	 [NSDictionary dictionaryWithObjectsAndKeys: @"Mode", OEGameCoreDisplayModeGroupNameKey,         displayModeMenu, OEGameCoreDisplayModeGroupItemsKey, nil],
	 [NSDictionary dictionaryWithObjectsAndKeys: @"Rotation", OEGameCoreDisplayModeGroupNameKey,     displayRotationMenu, OEGameCoreDisplayModeGroupItemsKey, nil],
	 [NSDictionary dictionaryWithObjectsAndKeys: @"Layout", OEGameCoreDisplayModeGroupNameKey,       displayLayoutMenu, OEGameCoreDisplayModeGroupItemsKey, nil],
	 [NSDictionary dictionaryWithObjectsAndKeys: @"Order", OEGameCoreDisplayModeGroupNameKey,        displayOrderMenu, OEGameCoreDisplayModeGroupItemsKey, nil],
	 [NSDictionary dictionaryWithObjectsAndKeys: @"Separation", OEGameCoreDisplayModeGroupNameKey,   displaySeparationMenu, OEGameCoreDisplayModeGroupItemsKey, nil],
	 [NSDictionary dictionaryWithObjectsAndKeys: @"Video Source", OEGameCoreDisplayModeGroupNameKey, displayVideoSourceMenu, OEGameCoreDisplayModeGroupItemsKey, nil],
	 nil];
	*/
	
	// Still working on other display modes, so just support the original display modes for now.
	NSArray< NSDictionary<NSString *, id> *> *newDisplayModeMenuDescription =
	[[NSArray alloc] initWithObjects:
	 [self generateDisplayModeItemByID:NDSDisplayOptionID_Mode_DualScreen states:currentDisplayMode],
	 [self generateDisplayModeItemByID:NDSDisplayOptionID_Mode_Main states:currentDisplayMode],
	 [self generateDisplayModeItemByID:NDSDisplayOptionID_Mode_Touch states:currentDisplayMode],
	 nil];
	
	return newDisplayModeMenuDescription;
}

/** Change display mode.
 *  @param displayMode The name of the display mode to enable or disable, as
 *    specified in its OEGameCoreDisplayModeNameKey key. */

- (void)changeDisplayWithMode:(NSString *)displayMode
{
	NSNumber *optionIDNumber = [_displayModeIDFromString objectForKey:displayMode];
	if ( (displayMode == nil) || (optionIDNumber == nil) )
	{
		return;
	}
	
	uint64_t displayModeStates = [self ndsDisplayMode];
	
	const NDSDisplayOptionID optionID = (NDSDisplayOptionID)[optionIDNumber integerValue];
	switch (optionID)
	{
		case NDSDisplayOptionID_Mode_DualScreen:
		case NDSDisplayOptionID_Mode_Main:
		case NDSDisplayOptionID_Mode_Touch:
			DISPLAYMODE_STATEBITGROUP_CLEAR(displayModeStates, NDSDisplayOptionStateBitmask_DisplayMode);
			break;
			
		case NDSDisplayOptionID_Rotation_0:
		case NDSDisplayOptionID_Rotation_90:
		case NDSDisplayOptionID_Rotation_180:
		case NDSDisplayOptionID_Rotation_270:
			DISPLAYMODE_STATEBITGROUP_CLEAR(displayModeStates, NDSDisplayOptionStateBitmask_Rotation);
			break;
			
		case NDSDisplayOptionID_Layout_Vertical:
		case NDSDisplayOptionID_Layout_Horizontal:
		case NDSDisplayOptionID_Layout_Hybrid_2_1:
		case NDSDisplayOptionID_Layout_Hybrid_16_9:
		case NDSDisplayOptionID_Layout_Hybrid_16_10:
			DISPLAYMODE_STATEBITGROUP_CLEAR(displayModeStates, NDSDisplayOptionStateBitmask_Layout);
			break;
			
		case NDSDisplayOptionID_Order_MainFirst:
		case NDSDisplayOptionID_Order_TouchFirst:
			DISPLAYMODE_STATEBITGROUP_CLEAR(displayModeStates, NDSDisplayOptionStateBitmask_Order);
			break;
			
		case NDSDisplayOptionID_Separation_0:
		case NDSDisplayOptionID_Separation_50:
		case NDSDisplayOptionID_Separation_100:
		case NDSDisplayOptionID_Separation_150:
		case NDSDisplayOptionID_Separation_200:
			DISPLAYMODE_STATEBITGROUP_CLEAR(displayModeStates, NDSDisplayOptionStateBitmask_Separation);
			break;
			
		case NDSDisplayOptionID_VideoSourceMain_None:
		case NDSDisplayOptionID_VideoSourceMain_NDS:
		case NDSDisplayOptionID_VideoSourceMain_ForceMain:
		case NDSDisplayOptionID_VideoSourceMain_ForceTouch:
			DISPLAYMODE_STATEBITGROUP_CLEAR(displayModeStates, NDSDisplayOptionStateBitmask_VideoSourceMain);
			break;
			
		case NDSDisplayOptionID_VideoSourceTouch_None:
		case NDSDisplayOptionID_VideoSourceTouch_NDS:
		case NDSDisplayOptionID_VideoSourceTouch_ForceMain:
		case NDSDisplayOptionID_VideoSourceTouch_ForceTouch:
			DISPLAYMODE_STATEBITGROUP_CLEAR(displayModeStates, NDSDisplayOptionStateBitmask_VideoSourceTouch);
			break;
			
		default:
			break;
	}

	displayModeStates |= (1 << optionID);
	[self setNdsDisplayMode:displayModeStates];
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
	uint64_t displayModeStates = [self ndsDisplayMode];
	
	if ( DISPLAYMODE_STATEBIT_CHECK(displayModeStates, NDSDisplayOptionID_Mode_DualScreen) )
	{
		isTouchPressed = YES;
		aPoint.y -= GPU_FRAMEBUFFER_NATIVE_HEIGHT; // Normalize the y-coordinate to the DS.
	}
	else if ( DISPLAYMODE_STATEBIT_CHECK(displayModeStates, NDSDisplayOptionID_Mode_Main) )
	{
		isTouchPressed = NO; // Reject touch input if showing only the main screen.
	}
	else if ( DISPLAYMODE_STATEBIT_CHECK(displayModeStates, NDSDisplayOptionID_Mode_Touch) )
	{
		isTouchPressed = YES;
	}
	else
	{
		return;
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
