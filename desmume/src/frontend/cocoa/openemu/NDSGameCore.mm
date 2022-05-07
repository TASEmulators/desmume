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
#include "../OGLDisplayOutput_3_2.h"

#import "OEDisplayView.h"

#include "../../NDSSystem.h"
#include "../../GPU.h"
#include "../../render3D.h"
#undef BOOL

#define DISPLAYMODE_STATEBIT_CHECK(_STATEBITS_, _ID_) ((((_STATEBITS_) >> ((uint64_t)_ID_)) & 1ULL) != 0ULL)
#define DISPLAYMODE_STATEBITGROUP_CLEAR(_STATEBITS_, _BITMASK_) _STATEBITS_ = (((_STATEBITS_) | (_BITMASK_)) ^ (_BITMASK_))


static const OEMenuItemDesc kDisplayModeItem[NDSDisplayOptionID_Count] = {
	{ @NDSDISPLAYMODE_NAMEKEY_MODE_DUALSCREEN, @NDSDISPLAYMODE_PREFKEY_DISPLAYMODE, NDSDISPLAYMODE_GROUPBITMASK_DISPLAYMODE, nil },
	{ @NDSDISPLAYMODE_NAMEKEY_MODE_MAIN,       @NDSDISPLAYMODE_PREFKEY_DISPLAYMODE, NDSDISPLAYMODE_GROUPBITMASK_DISPLAYMODE, nil },
	{ @NDSDISPLAYMODE_NAMEKEY_MODE_TOUCH,      @NDSDISPLAYMODE_PREFKEY_DISPLAYMODE, NDSDISPLAYMODE_GROUPBITMASK_DISPLAYMODE, nil },
	
	{ @NDSDISPLAYMODE_NAMEKEY_ROTATION_0,   @NDSDISPLAYMODE_PREFKEY_ROTATION, NDSDISPLAYMODE_GROUPBITMASK_ROTATION, nil },
	{ @NDSDISPLAYMODE_NAMEKEY_ROTATION_90,  @NDSDISPLAYMODE_PREFKEY_ROTATION, NDSDISPLAYMODE_GROUPBITMASK_ROTATION, nil },
	{ @NDSDISPLAYMODE_NAMEKEY_ROTATION_180, @NDSDISPLAYMODE_PREFKEY_ROTATION, NDSDISPLAYMODE_GROUPBITMASK_ROTATION, nil },
	{ @NDSDISPLAYMODE_NAMEKEY_ROTATION_270, @NDSDISPLAYMODE_PREFKEY_ROTATION, NDSDISPLAYMODE_GROUPBITMASK_ROTATION, nil },
	
	{ @NDSDISPLAYMODE_NAMEKEY_LAYOUT_VERTICAL,     @NDSDISPLAYMODE_PREFKEY_LAYOUT, NDSDISPLAYMODE_GROUPBITMASK_LAYOUT, nil },
	{ @NDSDISPLAYMODE_NAMEKEY_LAYOUT_HORIZONTAL,   @NDSDISPLAYMODE_PREFKEY_LAYOUT, NDSDISPLAYMODE_GROUPBITMASK_LAYOUT, nil },
	{ @NDSDISPLAYMODE_NAMEKEY_LAYOUT_HYBRID_2_1,   @NDSDISPLAYMODE_PREFKEY_LAYOUT, NDSDISPLAYMODE_GROUPBITMASK_LAYOUT, nil },
	{ @NDSDISPLAYMODE_NAMEKEY_LAYOUT_HYBRID_16_9,  @NDSDISPLAYMODE_PREFKEY_LAYOUT, NDSDISPLAYMODE_GROUPBITMASK_LAYOUT, nil },
	{ @NDSDISPLAYMODE_NAMEKEY_LAYOUT_HYBRID_16_10, @NDSDISPLAYMODE_PREFKEY_LAYOUT, NDSDISPLAYMODE_GROUPBITMASK_LAYOUT, nil },
	
	{ @NDSDISPLAYMODE_NAMEKEY_DISPLAYORDER_MAIN,  @NDSDISPLAYMODE_PREFKEY_ORDER, NDSDISPLAYMODE_GROUPBITMASK_ORDER, nil },
	{ @NDSDISPLAYMODE_NAMEKEY_DISPLAYORDER_TOUCH, @NDSDISPLAYMODE_PREFKEY_ORDER, NDSDISPLAYMODE_GROUPBITMASK_ORDER, nil },
	
	{ @NDSDISPLAYMODE_NAMEKEY_SEPARATION_0,   @NDSDISPLAYMODE_PREFKEY_SEPARATION, NDSDISPLAYMODE_GROUPBITMASK_SEPARATION, nil },
	{ @NDSDISPLAYMODE_NAMEKEY_SEPARATION_50,  @NDSDISPLAYMODE_PREFKEY_SEPARATION, NDSDISPLAYMODE_GROUPBITMASK_SEPARATION, nil },
	{ @NDSDISPLAYMODE_NAMEKEY_SEPARATION_100, @NDSDISPLAYMODE_PREFKEY_SEPARATION, NDSDISPLAYMODE_GROUPBITMASK_SEPARATION, nil },
	{ @NDSDISPLAYMODE_NAMEKEY_SEPARATION_150, @NDSDISPLAYMODE_PREFKEY_SEPARATION, NDSDISPLAYMODE_GROUPBITMASK_SEPARATION, nil },
	{ @NDSDISPLAYMODE_NAMEKEY_SEPARATION_200, @NDSDISPLAYMODE_PREFKEY_SEPARATION, NDSDISPLAYMODE_GROUPBITMASK_SEPARATION, nil },
	
	{ @NDSDISPLAYMODE_NAMEKEY_VIDEOSOURCEMAIN_NONE,      @NDSDISPLAYMODE_PREFKEY_VIDEOSOURCE_MAIN, NDSDISPLAYMODE_GROUPBITMASK_VIDEOSOURCEMAIN, nil },
	{ @NDSDISPLAYMODE_NAMEKEY_VIDEOSOURCEMAIN_NDS,       @NDSDISPLAYMODE_PREFKEY_VIDEOSOURCE_MAIN, NDSDISPLAYMODE_GROUPBITMASK_VIDEOSOURCEMAIN, nil },
	{ @NDSDISPLAYMODE_NAMEKEY_VIDEOSOURCEMAIN_FORCEMAIN, @NDSDISPLAYMODE_PREFKEY_VIDEOSOURCE_MAIN, NDSDISPLAYMODE_GROUPBITMASK_VIDEOSOURCEMAIN, nil },
	{ @NDSDISPLAYMODE_NAMEKEY_VIDEOSOURCEMAIN_FORCESUB,  @NDSDISPLAYMODE_PREFKEY_VIDEOSOURCE_MAIN, NDSDISPLAYMODE_GROUPBITMASK_VIDEOSOURCEMAIN, nil },
	
	{ @NDSDISPLAYMODE_NAMEKEY_VIDEOSOURCETOUCH_NONE,      @NDSDISPLAYMODE_PREFKEY_VIDEOSOURCE_TOUCH, NDSDISPLAYMODE_GROUPBITMASK_VIDEOSOURCETOUCH, nil },
	{ @NDSDISPLAYMODE_NAMEKEY_VIDEOSOURCETOUCH_NDS,       @NDSDISPLAYMODE_PREFKEY_VIDEOSOURCE_TOUCH, NDSDISPLAYMODE_GROUPBITMASK_VIDEOSOURCETOUCH, nil },
	{ @NDSDISPLAYMODE_NAMEKEY_VIDEOSOURCETOUCH_FORCEMAIN, @NDSDISPLAYMODE_PREFKEY_VIDEOSOURCE_TOUCH, NDSDISPLAYMODE_GROUPBITMASK_VIDEOSOURCETOUCH, nil },
	{ @NDSDISPLAYMODE_NAMEKEY_VIDEOSOURCETOUCH_FORCESUB,  @NDSDISPLAYMODE_PREFKEY_VIDEOSOURCE_TOUCH, NDSDISPLAYMODE_GROUPBITMASK_VIDEOSOURCETOUCH, nil },
	
	{ @NDSDISPLAYMODE_NAMEKEY_HUD_ENABLE,          @NDSDISPLAYMODE_PREFKEY_HUD_ENABLE, 0, nil },
	{ @NDSDISPLAYMODE_NAMEKEY_HUD_EXECUTIONSPEED,  @NDSDISPLAYMODE_PREFKEY_HUD_EXECUTIONSPEED, 0, nil },
	{ @NDSDISPLAYMODE_NAMEKEY_HUD_VIDEOFPS,        @NDSDISPLAYMODE_PREFKEY_HUD_VIDEOFPS, 0, nil },
	{ @NDSDISPLAYMODE_NAMEKEY_HUD_3DRENDERERFPS,   @NDSDISPLAYMODE_PREFKEY_HUD_3DRENDERERFPS, 0, nil },
	{ @NDSDISPLAYMODE_NAMEKEY_HUD_FRAMEINDEX,      @NDSDISPLAYMODE_PREFKEY_HUD_FRAMEINDEX, 0, nil },
	{ @NDSDISPLAYMODE_NAMEKEY_HUD_LAGFRAMECOUNTER, @NDSDISPLAYMODE_PREFKEY_HUD_LAGFRAMECOUNTER, 0, nil },
	{ @NDSDISPLAYMODE_NAMEKEY_HUD_CPULOADAVERAGE,  @NDSDISPLAYMODE_PREFKEY_HUD_CPULOADAVERAGE, 0, nil },
	{ @NDSDISPLAYMODE_NAMEKEY_HUD_REALTIMECLOCK,   @NDSDISPLAYMODE_PREFKEY_HUD_REALTIMECLOCK, 0, nil },
	{ @NDSDISPLAYMODE_NAMEKEY_HUD_INPUT,           @NDSDISPLAYMODE_PREFKEY_HUD_INPUT, 0, nil },
	
	{ @NDSDISPLAYMODE_NAMEKEY_GPU_ENGINE_SOFTWARE, @NDSDISPLAYMODE_PREFKEY_GPU_ENGINE, NDSDISPLAYMODE_GROUPBITMASK_GPUENGINE, nil },
	{ @NDSDISPLAYMODE_NAMEKEY_GPU_ENGINE_OPENGL,   @NDSDISPLAYMODE_PREFKEY_GPU_ENGINE, NDSDISPLAYMODE_GROUPBITMASK_GPUENGINE, nil },
	
	{ @NDSDISPLAYMODE_NAMEKEY_GPU_SOFTWARE_FRAGMENTSAMPLINGHACK, @NDSDISPLAYMODE_PREFKEY_GPU_SOFTWARE_FRAGMENTSAMPLINGHACK, 0, nil },
	{ @NDSDISPLAYMODE_NAMEKEY_GPU_OPENGL_SMOOTHTEXTURES,         @NDSDISPLAYMODE_PREFKEY_GPU_OPENGL_SMOOTHTEXTURES, 0, nil },
	
	{ @NDSDISPLAYMODE_NAMEKEY_3D_RENDERSCALING_1X, @NDSDISPLAYMODE_PREFKEY_3D_RENDERSCALING, NDSDISPLAYMODE_GROUPBITMASK_RENDERSCALING, nil },
	{ @NDSDISPLAYMODE_NAMEKEY_3D_RENDERSCALING_2X, @NDSDISPLAYMODE_PREFKEY_3D_RENDERSCALING, NDSDISPLAYMODE_GROUPBITMASK_RENDERSCALING, nil },
	{ @NDSDISPLAYMODE_NAMEKEY_3D_RENDERSCALING_3X, @NDSDISPLAYMODE_PREFKEY_3D_RENDERSCALING, NDSDISPLAYMODE_GROUPBITMASK_RENDERSCALING, nil },
	{ @NDSDISPLAYMODE_NAMEKEY_3D_RENDERSCALING_4X, @NDSDISPLAYMODE_PREFKEY_3D_RENDERSCALING, NDSDISPLAYMODE_GROUPBITMASK_RENDERSCALING, nil },
	{ @NDSDISPLAYMODE_NAMEKEY_3D_RENDERSCALING_5X, @NDSDISPLAYMODE_PREFKEY_3D_RENDERSCALING, NDSDISPLAYMODE_GROUPBITMASK_RENDERSCALING, nil },
	{ @NDSDISPLAYMODE_NAMEKEY_3D_RENDERSCALING_6X, @NDSDISPLAYMODE_PREFKEY_3D_RENDERSCALING, NDSDISPLAYMODE_GROUPBITMASK_RENDERSCALING, nil },
	{ @NDSDISPLAYMODE_NAMEKEY_3D_RENDERSCALING_7X, @NDSDISPLAYMODE_PREFKEY_3D_RENDERSCALING, NDSDISPLAYMODE_GROUPBITMASK_RENDERSCALING, nil },
	{ @NDSDISPLAYMODE_NAMEKEY_3D_RENDERSCALING_8X, @NDSDISPLAYMODE_PREFKEY_3D_RENDERSCALING, NDSDISPLAYMODE_GROUPBITMASK_RENDERSCALING, nil },
	
	{ @NDSDISPLAYMODE_NAMEKEY_3D_TEXTURESCALING_1X, @NDSDISPLAYMODE_PREFKEY_3D_TEXTURESCALING, NDSDISPLAYMODE_GROUPBITMASK_TEXTURESCALING, nil },
	{ @NDSDISPLAYMODE_NAMEKEY_3D_TEXTURESCALING_2X, @NDSDISPLAYMODE_PREFKEY_3D_TEXTURESCALING, NDSDISPLAYMODE_GROUPBITMASK_TEXTURESCALING, nil },
	{ @NDSDISPLAYMODE_NAMEKEY_3D_TEXTURESCALING_4X, @NDSDISPLAYMODE_PREFKEY_3D_TEXTURESCALING, NDSDISPLAYMODE_GROUPBITMASK_TEXTURESCALING, nil }
};

static const uint64_t kDisplayModeStatesDefault = (1LLU << NDSDisplayOptionID_Mode_DualScreen) |
                                                  (1LLU << NDSDisplayOptionID_Rotation_0) |
                                                  (1LLU << NDSDisplayOptionID_Layout_Vertical) |
                                                  (1LLU << NDSDisplayOptionID_Order_MainFirst) |
                                                  (1LLU << NDSDisplayOptionID_Separation_0) |
                                                  (1LLU << NDSDisplayOptionID_VideoSourceMain_NDS) |
                                                  (1LLU << NDSDisplayOptionID_VideoSourceTouch_NDS) |
                                                  (1LLU << NDSDisplayOptionID_HUD_ExecutionSpeed) |
                                                  (1LLU << NDSDisplayOptionID_HUD_VideoFPS) |
                                                  (1LLU << NDSDisplayOptionID_GPU_Engine_Software) |
                                                  (1LLU << NDSDisplayOptionID_3D_RenderScaling_1x) |
                                                  (1LLU << NDSDisplayOptionID_3D_TextureScaling_1x);

volatile bool execute = true;

@implementation NDSGameCore

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
	
	_fpsTimer = nil;
	_willUseOpenGL3Context = true;
	_videoContext = NULL;
	_cdp = NULL;
	cdsGPU = NULL;
	
	// Set up threading locks
	_unfairlockReceivedFrameIndex = apple_unfairlock_create();
	_unfairlockNDSFrameInfo = apple_unfairlock_create();
	
	unfairlockDisplayMode = apple_unfairlock_create();
	
	// Set up input handling
	_lastTouchLocation.x = 0;
	_lastTouchLocation.y = 0;
	_isInitialTouchPress = false;
	_isTouchPressInMajorDisplay = false;
	
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
	
	memset(&_clientFrameInfo, 0, sizeof(_clientFrameInfo));
	_ndsFrameInfo.clear();
	
	_isTimerAtSecond =  NO;
	_receivedFrameIndex = 0;
	_currentReceivedFrameIndex = 0;
	_receivedFrameCount = 0;
	
	_executionSpeedAverageFramesCollected = 0.0;
	
	// Set up the emulation core
	CommonSettings.advanced_timing = true;
	CommonSettings.jit_max_block_size = 12;
#ifdef HAVE_JIT
	CommonSettings.use_jit = true;
#else
	CommonSettings.use_jit = false;
#endif
	pthread_rwlock_init(&rwlockCoreExecute, NULL);
	NDS_Init();
	
	// Set up the DS controller
	_inputHandler = new MacInputHandler;
	
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
	
	if ([self respondsToSelector:@selector(audioBufferAtIndex:)])
	{
		openEmuSoundInterfaceBuffer = [self audioBufferAtIndex:0];
	}
	else
	{
		SILENCE_DEPRECATION_OPENEMU( openEmuSoundInterfaceBuffer = [self ringBufferAtIndex:0]; )
	}
	
	NSInteger result = SPU_ChangeSoundCore(SNDCORE_OPENEMU, (int)SPU_BUFFER_BYTES);
	if (result == -1)
	{
		SPU_ChangeSoundCore(SNDCORE_DUMMY, 0);
	}
	
	SPU_SetSynchMode(CommonSettings.SPU_sync_mode, CommonSettings.SPU_sync_method);
	SPU_SetVolume(100);
    
	// Set up the DS display
	_displayModeStatesPending = kDisplayModeStatesDefault;
	_displayModeStatesApplied = kDisplayModeStatesDefault;
	_OEViewSize.width  = GPU_FRAMEBUFFER_NATIVE_WIDTH;
	_OEViewSize.height = GPU_FRAMEBUFFER_NATIVE_HEIGHT * 2;
	_displayBufferSize = OEIntSizeMake(_OEViewSize.width, _OEViewSize.height);
	_displayRect = OEIntRectMake(0, 0, _OEViewSize.width, _OEViewSize.height);
	_displayAspectRatio = OEIntSizeMake(_OEViewSize.width, _OEViewSize.height);
	_canRespondToViewResize = NO;
	_selectedRenderScaling = 1;
	_selectedTextureScaling = 1;
	
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
								
								[NSNumber numberWithInteger:NDSDisplayOptionID_VideoSourceMain_None],      kDisplayModeItem[NDSDisplayOptionID_VideoSourceMain_None].nameKey,
								[NSNumber numberWithInteger:NDSDisplayOptionID_VideoSourceMain_NDS],       kDisplayModeItem[NDSDisplayOptionID_VideoSourceMain_NDS].nameKey,
								[NSNumber numberWithInteger:NDSDisplayOptionID_VideoSourceMain_ForceMain], kDisplayModeItem[NDSDisplayOptionID_VideoSourceMain_ForceMain].nameKey,
								[NSNumber numberWithInteger:NDSDisplayOptionID_VideoSourceMain_ForceSub],  kDisplayModeItem[NDSDisplayOptionID_VideoSourceMain_ForceSub].nameKey,
								
								[NSNumber numberWithInteger:NDSDisplayOptionID_VideoSourceTouch_None],      kDisplayModeItem[NDSDisplayOptionID_VideoSourceTouch_None].nameKey,
								[NSNumber numberWithInteger:NDSDisplayOptionID_VideoSourceTouch_NDS],       kDisplayModeItem[NDSDisplayOptionID_VideoSourceTouch_NDS].nameKey,
								[NSNumber numberWithInteger:NDSDisplayOptionID_VideoSourceTouch_ForceMain], kDisplayModeItem[NDSDisplayOptionID_VideoSourceTouch_ForceMain].nameKey,
								[NSNumber numberWithInteger:NDSDisplayOptionID_VideoSourceTouch_ForceSub],  kDisplayModeItem[NDSDisplayOptionID_VideoSourceTouch_ForceSub].nameKey,
								
								[NSNumber numberWithInteger:NDSDisplayOptionID_HUD_Enable],          kDisplayModeItem[NDSDisplayOptionID_HUD_Enable].nameKey,
								[NSNumber numberWithInteger:NDSDisplayOptionID_HUD_ExecutionSpeed],  kDisplayModeItem[NDSDisplayOptionID_HUD_ExecutionSpeed].nameKey,
								[NSNumber numberWithInteger:NDSDisplayOptionID_HUD_VideoFPS],        kDisplayModeItem[NDSDisplayOptionID_HUD_VideoFPS].nameKey,
								[NSNumber numberWithInteger:NDSDisplayOptionID_HUD_3DRendererFPS],   kDisplayModeItem[NDSDisplayOptionID_HUD_3DRendererFPS].nameKey,
								[NSNumber numberWithInteger:NDSDisplayOptionID_HUD_FrameIndex],      kDisplayModeItem[NDSDisplayOptionID_HUD_FrameIndex].nameKey,
								[NSNumber numberWithInteger:NDSDisplayOptionID_HUD_LagFrameCounter], kDisplayModeItem[NDSDisplayOptionID_HUD_LagFrameCounter].nameKey,
								[NSNumber numberWithInteger:NDSDisplayOptionID_HUD_CPULoadAverage],  kDisplayModeItem[NDSDisplayOptionID_HUD_CPULoadAverage].nameKey,
								[NSNumber numberWithInteger:NDSDisplayOptionID_HUD_RealTimeClock],   kDisplayModeItem[NDSDisplayOptionID_HUD_RealTimeClock].nameKey,
								[NSNumber numberWithInteger:NDSDisplayOptionID_HUD_Input],           kDisplayModeItem[NDSDisplayOptionID_HUD_Input].nameKey,
								
								[NSNumber numberWithInteger:NDSDisplayOptionID_GPU_Engine_Software], kDisplayModeItem[NDSDisplayOptionID_GPU_Engine_Software].nameKey,
								[NSNumber numberWithInteger:NDSDisplayOptionID_GPU_Engine_OpenGL],   kDisplayModeItem[NDSDisplayOptionID_GPU_Engine_OpenGL].nameKey,
								
								[NSNumber numberWithInteger:NDSDisplayOptionID_GPU_Software_FragmentSamplingHack], kDisplayModeItem[NDSDisplayOptionID_GPU_Software_FragmentSamplingHack].nameKey,
								[NSNumber numberWithInteger:NDSDisplayOptionID_GPU_OpenGL_SmoothTextures],         kDisplayModeItem[NDSDisplayOptionID_GPU_OpenGL_SmoothTextures].nameKey,
								
								[NSNumber numberWithInteger:NDSDisplayOptionID_3D_RenderScaling_1x], kDisplayModeItem[NDSDisplayOptionID_3D_RenderScaling_1x].nameKey,
								[NSNumber numberWithInteger:NDSDisplayOptionID_3D_RenderScaling_2x], kDisplayModeItem[NDSDisplayOptionID_3D_RenderScaling_2x].nameKey,
								[NSNumber numberWithInteger:NDSDisplayOptionID_3D_RenderScaling_3x], kDisplayModeItem[NDSDisplayOptionID_3D_RenderScaling_3x].nameKey,
								[NSNumber numberWithInteger:NDSDisplayOptionID_3D_RenderScaling_4x], kDisplayModeItem[NDSDisplayOptionID_3D_RenderScaling_4x].nameKey,
								[NSNumber numberWithInteger:NDSDisplayOptionID_3D_RenderScaling_5x], kDisplayModeItem[NDSDisplayOptionID_3D_RenderScaling_5x].nameKey,
								[NSNumber numberWithInteger:NDSDisplayOptionID_3D_RenderScaling_6x], kDisplayModeItem[NDSDisplayOptionID_3D_RenderScaling_6x].nameKey,
								[NSNumber numberWithInteger:NDSDisplayOptionID_3D_RenderScaling_7x], kDisplayModeItem[NDSDisplayOptionID_3D_RenderScaling_7x].nameKey,
								[NSNumber numberWithInteger:NDSDisplayOptionID_3D_RenderScaling_8x], kDisplayModeItem[NDSDisplayOptionID_3D_RenderScaling_8x].nameKey,
								
								[NSNumber numberWithInteger:NDSDisplayOptionID_3D_TextureScaling_1x], kDisplayModeItem[NDSDisplayOptionID_3D_TextureScaling_1x].nameKey,
								[NSNumber numberWithInteger:NDSDisplayOptionID_3D_TextureScaling_2x], kDisplayModeItem[NDSDisplayOptionID_3D_TextureScaling_2x].nameKey,
								[NSNumber numberWithInteger:NDSDisplayOptionID_3D_TextureScaling_4x], kDisplayModeItem[NDSDisplayOptionID_3D_TextureScaling_4x].nameKey,
								nil];
	
	return self;
}

- (void)dealloc
{
	[_fpsTimer invalidate];
	_fpsTimer = nil;
	
	[addedCheatsDict release];
	[cdsCheats release];
	[cdsFirmware release];
	
	[_displayModeIDFromString release];
	
	CGLReleaseContext(_videoContext);
	_videoContext = NULL;
	
	delete _cdp;
	delete _inputHandler;
	
	SPU_ChangeSoundCore(SNDCORE_DUMMY, 0);
	NDS_DeInit();
	
	// Release cdsGPU after NDS_DeInit() to avoid potential crashes.
	[cdsGPU release];
	
	pthread_rwlock_destroy(&rwlockCoreExecute);
	apple_unfairlock_destroy(unfairlockDisplayMode);
	
	apple_unfairlock_destroy(_unfairlockReceivedFrameIndex);
	apple_unfairlock_destroy(_unfairlockNDSFrameInfo);
	
	[super dealloc];
}

- (void) initVideoWithCurrentOpenEmuContext
{
	CGLContextObj oldContext = _videoContext;
	OE_OGLClientFetchObject *fetchObj = NULL;
	
	// Set up the NDS GPU
	if (cdsGPU == nil)
	{
		cdsGPU = [[CocoaDSGPU alloc] init];
		[cdsGPU setRender3DThreads:0]; // Pass 0 to automatically set the number of rendering threads
		[cdsGPU setRender3DRenderingEngine:CORE3DLIST_SWRASTERIZE];
		[cdsGPU setRender3DFragmentSamplingHack:NO];
		[cdsGPU setRender3DTextureSmoothing:NO];
		[cdsGPU setRender3DTextureScalingFactor:1];
		[cdsGPU setGpuScale:1];
		[cdsGPU setGpuColorFormat:NDSColorFormat_BGR666_Rev];
		
		fetchObj = (OE_OGLClientFetchObject *)[cdsGPU fetchObject];
	}
	else
	{
		fetchObj = (OE_OGLClientFetchObject *)[cdsGPU fetchObject];
		fetchObj->Init();
		fetchObj->SetFetchBuffers( GPU->GetDisplayInfo() );
	}
	
	_videoContext = fetchObj->GetContext();
	CGLRetainContext(_videoContext);
	
	// Set up the presenter
	if (_cdp == NULL)
	{
		NSString *gameCoreFontPath = [[NSBundle bundleForClass:[self class]] pathForResource:@"SourceSansPro-Bold" ofType:@"otf"];
		const char *hudFontPath = [[NSFileManager defaultManager] fileSystemRepresentationWithPath:gameCoreFontPath];
		
		_cdp = new OE_OGLDisplayPresenter(fetchObj);
		_cdp->Init();
		_cdp->SetHUDFontPath(hudFontPath);
		_cdp->SetHUDRenderMipmapped(false); // Mipmapped HUD rendering doesn't work on OpenEmu!
		
		// OpenEmu doesn't provide us with the backing scaling factor, which is used to
		// adapt ClientDisplayPresenter to HiDPI/Retina displays. But if OpenEmu ever
		// does provide this information, we can give ClientDisplayPresenter this hint.
		// For now, just assume that the backing scale is 1.0.
		double scaleFactor = 1.0;
		if (scaleFactor != 1.0)
		{
			_cdp->SetScaleFactor(scaleFactor);
		}
		else
		{
			_cdp->LoadHUDFont();
		}
		
		_cdp->SetOutputFilter(OutputFilterTypeID_NearestNeighbor);
		_cdp->SetPixelScaler(VideoFilterTypeID_None);
		_cdp->SetFiltersPreferGPU(true);
		_cdp->SetSourceDeposterize(false);
		_cdp->SetHUDColorInputAppliedOnly(0xFFFFFFFF);
		_cdp->SetHUDColorInputPendingOnly(0xFFFFFFFF);
		_cdp->SetHUDColorInputPendingAndApplied(0xFFFFFFFF);
	}
	else
	{
		_cdp->Init();
		_cdp->LoadHUDFont();
	}
	
	if (oldContext != NULL)
	{
		CGLReleaseContext(oldContext);
	}
}

- (void) newFPSTimer
{
	if (_fpsTimer != nil)
	{
		[_fpsTimer invalidate];
	}
	
	_isTimerAtSecond = NO;
	_fpsTimer = [NSTimer timerWithTimeInterval:0.5
										target:self
									  selector:@selector(getTimedEmulatorStatistics:)
									  userInfo:nil
									   repeats:YES];
	
	[[NSRunLoop currentRunLoop] addTimer:_fpsTimer forMode:NSRunLoopCommonModes];
}

- (void) getTimedEmulatorStatistics:(NSTimer *)timer
{
	// The timer should fire every 0.5 seconds, so only take the frame
	// count every other instance the timer fires.
	_isTimerAtSecond = !_isTimerAtSecond;
	
	if (_isTimerAtSecond)
	{
		apple_unfairlock_lock(_unfairlockReceivedFrameIndex);
		_receivedFrameCount = _receivedFrameIndex - _currentReceivedFrameIndex;
		_currentReceivedFrameIndex = _receivedFrameIndex;
		apple_unfairlock_unlock(_unfairlockReceivedFrameIndex);
	}
}

- (uint64_t) ndsDisplayMode
{
	apple_unfairlock_lock(unfairlockDisplayMode);
	const uint64_t displayModeStates = _displayModeStatesApplied;
	apple_unfairlock_unlock(unfairlockDisplayMode);
	
	return displayModeStates;
}

void UpdateDisplayPropertiesFromStates(uint64_t displayModeStates, ClientDisplayPresenterProperties &outProps)
{
	     if ( DISPLAYMODE_STATEBIT_CHECK(displayModeStates, NDSDisplayOptionID_Mode_DualScreen) ) outProps.mode = ClientDisplayMode_Dual;
	else if ( DISPLAYMODE_STATEBIT_CHECK(displayModeStates, NDSDisplayOptionID_Mode_Main) )       outProps.mode = ClientDisplayMode_Main;
	else if ( DISPLAYMODE_STATEBIT_CHECK(displayModeStates, NDSDisplayOptionID_Mode_Touch) )      outProps.mode = ClientDisplayMode_Touch;
	
	     if ( DISPLAYMODE_STATEBIT_CHECK(displayModeStates, NDSDisplayOptionID_Layout_Vertical) )     outProps.layout = ClientDisplayLayout_Vertical;
	else if ( DISPLAYMODE_STATEBIT_CHECK(displayModeStates, NDSDisplayOptionID_Layout_Horizontal) )   outProps.layout = ClientDisplayLayout_Horizontal;
	else if ( DISPLAYMODE_STATEBIT_CHECK(displayModeStates, NDSDisplayOptionID_Layout_Hybrid_2_1) )   outProps.layout = ClientDisplayLayout_Hybrid_2_1;
	else if ( DISPLAYMODE_STATEBIT_CHECK(displayModeStates, NDSDisplayOptionID_Layout_Hybrid_16_9) )  outProps.layout = ClientDisplayLayout_Hybrid_16_9;
	else if ( DISPLAYMODE_STATEBIT_CHECK(displayModeStates, NDSDisplayOptionID_Layout_Hybrid_16_10) ) outProps.layout = ClientDisplayLayout_Hybrid_16_10;
	
	     if ( DISPLAYMODE_STATEBIT_CHECK(displayModeStates, NDSDisplayOptionID_Order_MainFirst) )  outProps.order = ClientDisplayOrder_MainFirst;
	else if ( DISPLAYMODE_STATEBIT_CHECK(displayModeStates, NDSDisplayOptionID_Order_TouchFirst) ) outProps.order = ClientDisplayOrder_TouchFirst;
	
	     if ( DISPLAYMODE_STATEBIT_CHECK(displayModeStates, NDSDisplayOptionID_Separation_0) )   outProps.gapScale = 0.0;
	else if ( DISPLAYMODE_STATEBIT_CHECK(displayModeStates, NDSDisplayOptionID_Separation_50) )  outProps.gapScale = 0.5;
	else if ( DISPLAYMODE_STATEBIT_CHECK(displayModeStates, NDSDisplayOptionID_Separation_100) ) outProps.gapScale = 1.0;
	else if ( DISPLAYMODE_STATEBIT_CHECK(displayModeStates, NDSDisplayOptionID_Separation_150) ) outProps.gapScale = 1.5;
	else if ( DISPLAYMODE_STATEBIT_CHECK(displayModeStates, NDSDisplayOptionID_Separation_200) ) outProps.gapScale = 2.0;
	
	     if ( DISPLAYMODE_STATEBIT_CHECK(displayModeStates, NDSDisplayOptionID_Rotation_0) )   outProps.rotation = 0.0;
	else if ( DISPLAYMODE_STATEBIT_CHECK(displayModeStates, NDSDisplayOptionID_Rotation_90) )  outProps.rotation = 90.0;
	else if ( DISPLAYMODE_STATEBIT_CHECK(displayModeStates, NDSDisplayOptionID_Rotation_180) ) outProps.rotation = 180.0;
	else if ( DISPLAYMODE_STATEBIT_CHECK(displayModeStates, NDSDisplayOptionID_Rotation_270) ) outProps.rotation = 270.0;
}

- (void) setNdsDisplayMode:(uint64_t)displayModeStates
{
	ClientDisplayPresenterProperties newProps;
	UpdateDisplayPropertiesFromStates(displayModeStates, newProps);
	
	ClientDisplayPresenter::CalculateNormalSize(newProps.mode, newProps.layout, newProps.gapScale, newProps.normalWidth, newProps.normalHeight);
	double transformNormalWidth  = newProps.normalWidth;
	double transformNormalHeight = newProps.normalHeight;
	ClientDisplayPresenter::ConvertNormalToTransformedBounds(1.0, newProps.rotation, transformNormalWidth, transformNormalHeight);
	
	     if ( DISPLAYMODE_STATEBIT_CHECK(displayModeStates, NDSDisplayOptionID_3D_RenderScaling_1x) ) _selectedRenderScaling = 1;
	else if ( DISPLAYMODE_STATEBIT_CHECK(displayModeStates, NDSDisplayOptionID_3D_RenderScaling_2x) ) _selectedRenderScaling = 2;
	else if ( DISPLAYMODE_STATEBIT_CHECK(displayModeStates, NDSDisplayOptionID_3D_RenderScaling_3x) ) _selectedRenderScaling = 3;
	else if ( DISPLAYMODE_STATEBIT_CHECK(displayModeStates, NDSDisplayOptionID_3D_RenderScaling_4x) ) _selectedRenderScaling = 4;
	else if ( DISPLAYMODE_STATEBIT_CHECK(displayModeStates, NDSDisplayOptionID_3D_RenderScaling_5x) ) _selectedRenderScaling = 5;
	else if ( DISPLAYMODE_STATEBIT_CHECK(displayModeStates, NDSDisplayOptionID_3D_RenderScaling_6x) ) _selectedRenderScaling = 6;
	else if ( DISPLAYMODE_STATEBIT_CHECK(displayModeStates, NDSDisplayOptionID_3D_RenderScaling_7x) ) _selectedRenderScaling = 7;
	else if ( DISPLAYMODE_STATEBIT_CHECK(displayModeStates, NDSDisplayOptionID_3D_RenderScaling_8x) ) _selectedRenderScaling = 8;
	
	     if ( DISPLAYMODE_STATEBIT_CHECK(displayModeStates, NDSDisplayOptionID_3D_TextureScaling_1x) ) _selectedTextureScaling = 1;
	else if ( DISPLAYMODE_STATEBIT_CHECK(displayModeStates, NDSDisplayOptionID_3D_TextureScaling_2x) ) _selectedTextureScaling = 2;
	else if ( DISPLAYMODE_STATEBIT_CHECK(displayModeStates, NDSDisplayOptionID_3D_TextureScaling_4x) ) _selectedTextureScaling = 4;
	
	// If we're on an older version of OpenEmu that doesn't send us view resizing events via
	// [OEGameCore tryToResizeVideoTo:], then we need to guess an appropriate draw size based
	// on the transformed normal size of the presenter. If we don't do this, then OpenEmu will
	// draw at 1x scaling, which starts looking uglier as the view size is increased.
	if (!_canRespondToViewResize)
	{
		int legacyVersionScale = 1;
		_OEViewSize.width  = (int)(transformNormalWidth  + 0.0005);
		_OEViewSize.height = (int)(transformNormalHeight + 0.0005);
		
		if ( DISPLAYMODE_STATEBIT_CHECK(displayModeStates, NDSDisplayOptionID_Mode_DualScreen) )
		{
			// Let's increase the size of the hybrid display modes so that the minor displays
			// don't look so lo-res.
			if ( DISPLAYMODE_STATEBIT_CHECK(displayModeStates, NDSDisplayOptionID_Layout_Hybrid_2_1) )
			{
				legacyVersionScale = 2;
			}
			else if ( DISPLAYMODE_STATEBIT_CHECK(displayModeStates, NDSDisplayOptionID_Layout_Hybrid_16_9) ||
					  DISPLAYMODE_STATEBIT_CHECK(displayModeStates, NDSDisplayOptionID_Layout_Hybrid_16_10) )
			{
				legacyVersionScale = 4;
			}
		}
		
		// Also give hi-res 3D rendering a shot at increasing the scaling.
		if (legacyVersionScale < _selectedRenderScaling)
		{
			legacyVersionScale = _selectedRenderScaling;
		}
		
		// Multiply the normal size by our fixed scaling value.
		_OEViewSize.width  *= legacyVersionScale;
		_OEViewSize.height *= legacyVersionScale;
	}
	
	apple_unfairlock_lock(unfairlockDisplayMode);
	
	// Actual display rendering will be changed in [OEGameCore executeFrame] (see notes
	// in that method for why this is so), and so we need to push this change by setting
	// _displayModeStatesPending to a non-zero value.
	_displayModeStatesPending = displayModeStates;
	
	// The display buffer size represents the size of an IOSurface, which is fixed by
	// OpenEmu. However, ClientDisplayPresenter can draw into any sized framebuffer
	// while also assuming that it is drawing into the maximum available space that the
	// view can provide. Therefore, we need to manually set the display buffer size to
	// whatever size the OpenEmu view is.
	_displayBufferSize = _OEViewSize;
	
	// The display aspect ratio represents the actual drawable area within the view. Due
	// to this, the aspect ratio is implicitly defined by its drawable area. Since we
	// want to use the entire view as drawable area, this means that we must set the
	// "aspect ratio" to the view size.
	_displayAspectRatio = _OEViewSize;
	
	// The display rect represents the emulator's video output at a 1x scaling. Despite
	// the description given by OpenEmu's SDK, the "display rect" is what actually sets
	// the aspect ratio, but only for the emulator's video output. When the user changes
	// the view size, the Scale menu changes itself relative to the display size.
	//
	// And unlike what the OpenEmu SDK suggests, the "display rect" has absolutely nothing
	// to do with the drawable area, since we can draw things, such as the HUD, outside the
	// "display rect". Of course, drawing the HUD relative to the view rather than relative
	// to the emulator's video output is the intended behavior, but the OpenEmu SDK notes
	// don't make this easy to figure out!
	if (_canRespondToViewResize)
	{
		// For newer OpenEmu versions, the display rect size is 1x normal scaled.
		_displayRect = OEIntRectMake(0, 0, (int)(transformNormalWidth + 0.0005), (int)(transformNormalHeight + 0.0005));
	}
	else
	{
		// For older OpenEmu versions, the display rect size includes a fixed prescaling.
		// We'll need to keep this in mind when we calculate touch coordinates.
		_displayRect = OEIntRectMake(0, 0, _OEViewSize.width, _OEViewSize.height);
	}
	
	apple_unfairlock_unlock(unfairlockDisplayMode);
}

- (void) applyDisplayMode:(uint64_t)appliedState
{
	if (appliedState == 0)
	{
		return;
	}
	
	if (appliedState != _displayModeStatesApplied)
	{
		ClientDisplaySource displaySource[2] = { ClientDisplaySource_DeterminedByNDS, ClientDisplaySource_DeterminedByNDS };
		
		     if ( DISPLAYMODE_STATEBIT_CHECK(appliedState, NDSDisplayOptionID_VideoSourceMain_None) )       displaySource[NDSDisplayID_Main]  = ClientDisplaySource_None;
		else if ( DISPLAYMODE_STATEBIT_CHECK(appliedState, NDSDisplayOptionID_VideoSourceMain_NDS) )        displaySource[NDSDisplayID_Main]  = ClientDisplaySource_DeterminedByNDS;
		else if ( DISPLAYMODE_STATEBIT_CHECK(appliedState, NDSDisplayOptionID_VideoSourceMain_ForceMain) )  displaySource[NDSDisplayID_Main]  = ClientDisplaySource_EngineMain;
		else if ( DISPLAYMODE_STATEBIT_CHECK(appliedState, NDSDisplayOptionID_VideoSourceMain_ForceSub) )   displaySource[NDSDisplayID_Main]  = ClientDisplaySource_EngineSub;
		
		     if ( DISPLAYMODE_STATEBIT_CHECK(appliedState, NDSDisplayOptionID_VideoSourceTouch_None) )      displaySource[NDSDisplayID_Touch] = ClientDisplaySource_None;
		else if ( DISPLAYMODE_STATEBIT_CHECK(appliedState, NDSDisplayOptionID_VideoSourceTouch_NDS) )       displaySource[NDSDisplayID_Touch] = ClientDisplaySource_DeterminedByNDS;
		else if ( DISPLAYMODE_STATEBIT_CHECK(appliedState, NDSDisplayOptionID_VideoSourceTouch_ForceMain) ) displaySource[NDSDisplayID_Touch] = ClientDisplaySource_EngineMain;
		else if ( DISPLAYMODE_STATEBIT_CHECK(appliedState, NDSDisplayOptionID_VideoSourceTouch_ForceSub) )  displaySource[NDSDisplayID_Touch] = ClientDisplaySource_EngineSub;
		
		_cdp->SetDisplayVideoSource(NDSDisplayID_Main,  displaySource[NDSDisplayID_Main]);
		_cdp->SetDisplayVideoSource(NDSDisplayID_Touch, displaySource[NDSDisplayID_Touch]);
		
		_cdp->SetHUDVisibility( DISPLAYMODE_STATEBIT_CHECK(appliedState, NDSDisplayOptionID_HUD_Enable) );
		_cdp->SetHUDShowExecutionSpeed( DISPLAYMODE_STATEBIT_CHECK(appliedState, NDSDisplayOptionID_HUD_ExecutionSpeed) );
		_cdp->SetHUDShowRender3DFPS( DISPLAYMODE_STATEBIT_CHECK(appliedState, NDSDisplayOptionID_HUD_3DRendererFPS) );
		_cdp->SetHUDShowFrameIndex( DISPLAYMODE_STATEBIT_CHECK(appliedState, NDSDisplayOptionID_HUD_FrameIndex) );
		_cdp->SetHUDShowLagFrameCount( DISPLAYMODE_STATEBIT_CHECK(appliedState, NDSDisplayOptionID_HUD_LagFrameCounter) );
		_cdp->SetHUDShowCPULoadAverage( DISPLAYMODE_STATEBIT_CHECK(appliedState, NDSDisplayOptionID_HUD_CPULoadAverage) );
		_cdp->SetHUDShowRTC( DISPLAYMODE_STATEBIT_CHECK(appliedState, NDSDisplayOptionID_HUD_RealTimeClock) );
		_cdp->SetHUDShowInput( DISPLAYMODE_STATEBIT_CHECK(appliedState, NDSDisplayOptionID_HUD_Input) );
		
		const bool isVideoFPSEnabled = DISPLAYMODE_STATEBIT_CHECK(appliedState, NDSDisplayOptionID_HUD_VideoFPS);
		if (isVideoFPSEnabled)
		{
			if (_fpsTimer == nil)
			{
				[self newFPSTimer];
			}
		}
		else
		{
			[_fpsTimer invalidate];
			_fpsTimer = nil;
		}
		_cdp->SetHUDShowVideoFPS(isVideoFPSEnabled);
		
		[cdsGPU setRender3DFragmentSamplingHack:(DISPLAYMODE_STATEBIT_CHECK(appliedState, NDSDisplayOptionID_GPU_Software_FragmentSamplingHack)) ? YES : NO];
		[cdsGPU setRender3DTextureSmoothing:(DISPLAYMODE_STATEBIT_CHECK(appliedState, NDSDisplayOptionID_GPU_OpenGL_SmoothTextures)) ? YES : NO];
		
		if ( (appliedState & NDSDISPLAYMODE_GROUPBITMASK_GPUENGINE) != (_displayModeStatesApplied & NDSDISPLAYMODE_GROUPBITMASK_GPUENGINE) )
		{
			if ( DISPLAYMODE_STATEBIT_CHECK(appliedState, NDSDisplayOptionID_GPU_Engine_Software) )
			{
				[cdsGPU setRender3DRenderingEngine:CORE3DLIST_SWRASTERIZE];
				[cdsGPU setGpuColorFormat:NDSColorFormat_BGR666_Rev];
			}
			else if ( DISPLAYMODE_STATEBIT_CHECK(appliedState, NDSDisplayOptionID_GPU_Engine_OpenGL) )
			{
				[cdsGPU setRender3DRenderingEngine:CORE3DLIST_OPENGL];
				[cdsGPU setGpuColorFormat:NDSColorFormat_BGR888_Rev];
			}
		}
		
		if ( (appliedState & NDSDISPLAYMODE_GROUPBITMASK_RENDERSCALING) != (_displayModeStatesApplied & NDSDISPLAYMODE_GROUPBITMASK_RENDERSCALING) )
		{
			[cdsGPU setGpuScale:_selectedRenderScaling];
		}
		
		if ( (appliedState & NDSDISPLAYMODE_GROUPBITMASK_TEXTURESCALING) != (_displayModeStatesApplied & NDSDISPLAYMODE_GROUPBITMASK_TEXTURESCALING) )
		{
			[cdsGPU setRender3DTextureScalingFactor:_selectedTextureScaling];
		}
		
		_displayModeStatesApplied = appliedState;
	}
	
	ClientDisplayPresenterProperties newProps;
	UpdateDisplayPropertiesFromStates(appliedState, newProps);
	newProps.clientWidth  = _OEViewSize.width;
	newProps.clientHeight = _OEViewSize.height;
	
	_cdp->CommitPresenterProperties(newProps);
	_cdp->SetupPresenterProperties();
	
	_displayModeStatesPending = 0;
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
	SILENCE_DEPRECATION_OPENEMU( return [self loadFileAtPath:path]; )
}

// This older version of [OEGameCore loadFileAtPath:] exists for backwards compatibility.
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
	
	// Set the default options.
	// Have to do it now because displayModeInfo is only available now -- not at this object's init.
	NSDictionary<NSString *, id> *userDefaultsDisplayMode = nil;
	if ([self respondsToSelector:@selector(displayModeInfo)])
	{
		userDefaultsDisplayMode = [self displayModeInfo];
	}
	
	uint64_t s = _displayModeStatesPending;
	s = [self switchDisplayModeState:s nameKey:[userDefaultsDisplayMode objectForKey:@NDSDISPLAYMODE_PREFKEY_DISPLAYMODE]];
	s = [self switchDisplayModeState:s nameKey:[userDefaultsDisplayMode objectForKey:@NDSDISPLAYMODE_PREFKEY_ROTATION]];
	s = [self switchDisplayModeState:s nameKey:[userDefaultsDisplayMode objectForKey:@NDSDISPLAYMODE_PREFKEY_LAYOUT]];
	s = [self switchDisplayModeState:s nameKey:[userDefaultsDisplayMode objectForKey:@NDSDISPLAYMODE_PREFKEY_ORDER]];
	s = [self switchDisplayModeState:s nameKey:[userDefaultsDisplayMode objectForKey:@NDSDISPLAYMODE_PREFKEY_SEPARATION]];
	s = [self switchDisplayModeState:s nameKey:[userDefaultsDisplayMode objectForKey:@NDSDISPLAYMODE_PREFKEY_VIDEOSOURCE_MAIN]];
	s = [self switchDisplayModeState:s nameKey:[userDefaultsDisplayMode objectForKey:@NDSDISPLAYMODE_PREFKEY_VIDEOSOURCE_TOUCH]];
	s = [self switchDisplayModeState:s nameKey:[userDefaultsDisplayMode objectForKey:@NDSDISPLAYMODE_PREFKEY_GPU_ENGINE]];
	s = [self switchDisplayModeState:s nameKey:[userDefaultsDisplayMode objectForKey:@NDSDISPLAYMODE_PREFKEY_3D_RENDERSCALING]];
	s = [self switchDisplayModeState:s nameKey:[userDefaultsDisplayMode objectForKey:@NDSDISPLAYMODE_PREFKEY_3D_TEXTURESCALING]];
	
	s = [self setDisplayModeState:s optionID:NDSDisplayOptionID_HUD_Enable state:[userDefaultsDisplayMode objectForKey:@NDSDISPLAYMODE_PREFKEY_HUD_ENABLE]];
	s = [self setDisplayModeState:s optionID:NDSDisplayOptionID_HUD_ExecutionSpeed state:[userDefaultsDisplayMode objectForKey:@NDSDISPLAYMODE_PREFKEY_HUD_EXECUTIONSPEED]];
	s = [self setDisplayModeState:s optionID:NDSDisplayOptionID_HUD_VideoFPS state:[userDefaultsDisplayMode objectForKey:@NDSDISPLAYMODE_PREFKEY_HUD_VIDEOFPS]];
	s = [self setDisplayModeState:s optionID:NDSDisplayOptionID_HUD_3DRendererFPS state:[userDefaultsDisplayMode objectForKey:@NDSDISPLAYMODE_PREFKEY_HUD_3DRENDERERFPS]];
	s = [self setDisplayModeState:s optionID:NDSDisplayOptionID_HUD_FrameIndex state:[userDefaultsDisplayMode objectForKey:@NDSDISPLAYMODE_PREFKEY_HUD_FRAMEINDEX]];
	s = [self setDisplayModeState:s optionID:NDSDisplayOptionID_HUD_LagFrameCounter state:[userDefaultsDisplayMode objectForKey:@NDSDISPLAYMODE_PREFKEY_HUD_LAGFRAMECOUNTER]];
	s = [self setDisplayModeState:s optionID:NDSDisplayOptionID_HUD_CPULoadAverage state:[userDefaultsDisplayMode objectForKey:@NDSDISPLAYMODE_PREFKEY_HUD_CPULOADAVERAGE]];
	s = [self setDisplayModeState:s optionID:NDSDisplayOptionID_HUD_RealTimeClock state:[userDefaultsDisplayMode objectForKey:@NDSDISPLAYMODE_PREFKEY_HUD_REALTIMECLOCK]];
	s = [self setDisplayModeState:s optionID:NDSDisplayOptionID_HUD_Input state:[userDefaultsDisplayMode objectForKey:@NDSDISPLAYMODE_PREFKEY_HUD_INPUT]];
	
	s = [self setDisplayModeState:s optionID:NDSDisplayOptionID_GPU_Software_FragmentSamplingHack state:[userDefaultsDisplayMode objectForKey:@NDSDISPLAYMODE_PREFKEY_GPU_SOFTWARE_FRAGMENTSAMPLINGHACK]];
	s = [self setDisplayModeState:s optionID:NDSDisplayOptionID_GPU_OpenGL_SmoothTextures state:[userDefaultsDisplayMode objectForKey:@NDSDISPLAYMODE_PREFKEY_GPU_OPENGL_SMOOTHTEXTURES]];
	
	[self setNdsDisplayMode:s];
	
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
	// The notes for the [OEGameCore startEmulation] method suggest that the
	// OpenGL context is available in that method. That statement is false!
	// The only time it's guaranteed to be available is right here in this
	// method. Therefore, we have to lazy init this entire side of the
	// graphics stack right before we execute the frame because there is no
	// other viable way to get the current OpenGL context from OpenEmu.
	if ( (_videoContext == NULL) || (_videoContext != CGLGetCurrentContext()) )
	{
		[self initVideoWithCurrentOpenEmuContext];
	}
	
	// Applying the current display mode not only requires a working context,
	// which is only guaranteed now, and also requires the current view size,
	// which is also only guaranteed now. Due to those reasons, we can only
	// apply the display mode right now!
	if (_displayModeStatesPending != 0)
	{
		[self applyDisplayMode:_displayModeStatesPending];
	}
	
	_inputHandler->ProcessInputs();
	_inputHandler->ApplyInputs();
	
	pthread_rwlock_wrlock(&rwlockCoreExecute);
	NDS_exec<false>();
	pthread_rwlock_unlock(&rwlockCoreExecute);
	
	apple_unfairlock_lock(_unfairlockReceivedFrameIndex);
	_receivedFrameIndex++;
	apple_unfairlock_unlock(_unfairlockReceivedFrameIndex);
	
	SPU_Emulate_user();
	
	// Fetch the frame from the core emulator
	const NDSDisplayInfo &dispInfo = GPU->GetDisplayInfo();
	OE_OGLClientFetchObject &fetchObj = (OE_OGLClientFetchObject &)_cdp->GetFetchObject();
	fetchObj.SetFetchDisplayInfo(dispInfo);
	fetchObj.FetchFromBufferIndex(dispInfo.bufferIndex);
	
	// Update HUD information
	apple_unfairlock_lock(_unfairlockReceivedFrameIndex);
	_clientFrameInfo.videoFPS = _receivedFrameCount;
	_executionSpeedAverageFramesCollected += (double)_receivedFrameCount;
	apple_unfairlock_unlock(_unfairlockReceivedFrameIndex);
	
	if ((_ndsFrameInfo.frameIndex & 0x3F) == 0x3F)
	{
		_ndsFrameInfo.executionSpeed = 100.0 * _executionSpeedAverageFramesCollected / DS_FRAMES_PER_SECOND / ((double)0x3F + 1.0);
		_executionSpeedAverageFramesCollected = 0.0;
	}
	
	ClientExecutionControl::GenerateNDSFrameInfo(_inputHandler, _ndsFrameInfo);
	_cdp->SetHUDInfo(_clientFrameInfo, _ndsFrameInfo);
	
	// Render the frame to the presenter
	_cdp->LoadDisplays();
	_cdp->ProcessDisplays();
	_cdp->UpdateLayout();
	_cdp->PrerenderStateSetupOGL(); // Need this to enable OpenGL blending for the HUD.
	_cdp->RenderFrameOGL(false);
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
//- (const void *)getVideoBufferWithHint:(void *)hint;

/*!
 * @method tryToResizeVideoTo:
 * @discussion
 * If the core can natively draw at any resolution, change the resolution
 * to 'size' and return YES. Otherwise, return NO. If YES, the next call to
 * -executeFrame will have a newly sized framebuffer.
 * It is assumed that only 3D cores can do this.
 */
- (BOOL)tryToResizeVideoTo:(OEIntSize)size
{
	_canRespondToViewResize = YES;
	_OEViewSize = size;
	
	if (_displayModeStatesPending != 0)
	{
		[self setNdsDisplayMode:_displayModeStatesPending];
	}
	else
	{
		[self setNdsDisplayMode:_displayModeStatesApplied];
	}
	
	return YES;
}

/*!
 * @property gameCoreRendering
 * @discussion
 * What kind of 3D API the core requires, or none.
 * Defaults to 2D.
 */
- (OEGameCoreRendering)gameCoreRendering
{
	_willUseOpenGL3Context = true;
	return OEGameCoreRenderingOpenGL3Video;
}

- (BOOL)rendersToOpenGL
{
	// This method is retained for backwards compatibility.
	_willUseOpenGL3Context = false;
	return YES; // Equivalent to OEGameCoreRenderingOpenGL2Video
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
	apple_unfairlock_lock(unfairlockDisplayMode);
	const OEIntSize theBufferSize = _displayBufferSize;
	apple_unfairlock_unlock(unfairlockDisplayMode);
	
	return theBufferSize;
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
//- (GLenum)internalPixelFormat;

/*!
 * @property pixelType
 * @discussion
 * The 'type' parameter to glTexImage2D, used to create the framebuffer.
 * GL_BGRA is preferred, but avoid doing any conversions inside the core.
 * Ignored for 3D cores.
 */
//- (GLenum)pixelType;

/*!
 * @property pixelFormat
 * @discussion
 * The 'format' parameter to glTexImage2D, used to create the framebuffer.
 * GL_UNSIGNED_SHORT_1_5_5_5_REV or GL_UNSIGNED_INT_8_8_8_8_REV are preferred, but
 * avoid doing any conversions inside the core.
 * Ignored for 3D cores.
 */
//- (GLenum)pixelFormat;

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
	BOOL fileSuccess = [self saveStateToFileAtPath:fileName];
	
	if (block != nil)
	{
		block(fileSuccess, nil);
	}
}

- (BOOL)saveStateToFileAtPath:(NSString *)fileName
{
	// This method is retained for backwards compatibility.
	return [CocoaDSFile saveState:[NSURL fileURLWithPath:fileName]];
}

- (void)loadStateFromFileAtPath:(NSString *)fileName completionHandler:(void(^)(BOOL success, NSError *error))block
{
	BOOL fileSuccess = [self loadStateFromFileAtPath:fileName];
	
	if (block != nil)
	{
		block(fileSuccess, nil);
	}
}

- (BOOL)loadStateFromFileAtPath:(NSString *)fileName
{
	// This method is retained for backwards compatibility.
	return [CocoaDSFile loadState:[NSURL fileURLWithPath:fileName]];
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

- (NSDictionary<NSString *, id> *) generateDisplayModeItemByID:(NDSDisplayOptionID)optionID states:(const uint64_t)states
{
	if ( (optionID < 0) || (optionID >= NDSDisplayOptionID_Count) )
	{
		return nil;
	}
	
	return [NSDictionary dictionaryWithObjectsAndKeys:
			kDisplayModeItem[optionID].nameKey, OEGameCoreDisplayModeNameKey,
			kDisplayModeItem[optionID].prefKey, OEGameCoreDisplayModePrefKeyNameKey,
			[NSNumber numberWithBool:(kDisplayModeItem[optionID].groupBitmask == 0) ? YES : NO], OEGameCoreDisplayModeAllowsToggleKey,
			[NSNumber numberWithBool:DISPLAYMODE_STATEBIT_CHECK(states, optionID) ? YES : NO], OEGameCoreDisplayModeStateKey,
			nil];
}

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

- (NSArray<NSDictionary<NSString *, id> *> *) displayModes
{
	const uint64_t s = _displayModeStatesPending;
	
	// Generate each display option submenu.
	NSArray< NSDictionary<NSString *, id> *> *displayModeMenu =
	[NSArray arrayWithObjects:
	 [self generateDisplayModeItemByID:NDSDisplayOptionID_Mode_DualScreen states:s],
	 [self generateDisplayModeItemByID:NDSDisplayOptionID_Mode_Main states:s],
	 [self generateDisplayModeItemByID:NDSDisplayOptionID_Mode_Touch states:s],
	 nil];
	
	NSArray< NSDictionary<NSString *, id> *> *displayRotationMenu =
	[NSArray arrayWithObjects:
	 [self generateDisplayModeItemByID:NDSDisplayOptionID_Rotation_0 states:s],
	 [self generateDisplayModeItemByID:NDSDisplayOptionID_Rotation_90 states:s],
	 [self generateDisplayModeItemByID:NDSDisplayOptionID_Rotation_180 states:s],
	 [self generateDisplayModeItemByID:NDSDisplayOptionID_Rotation_270 states:s],
	 nil];
	
	NSArray< NSDictionary<NSString *, id> *> *displayLayoutMenu =
	[NSArray arrayWithObjects:
	 [self generateDisplayModeItemByID:NDSDisplayOptionID_Layout_Vertical states:s],
	 [self generateDisplayModeItemByID:NDSDisplayOptionID_Layout_Horizontal states:s],
	 [self generateDisplayModeItemByID:NDSDisplayOptionID_Layout_Hybrid_2_1 states:s],
	 [self generateDisplayModeItemByID:NDSDisplayOptionID_Layout_Hybrid_16_9 states:s],
	 [self generateDisplayModeItemByID:NDSDisplayOptionID_Layout_Hybrid_16_10 states:s],
	 nil];
	
	NSArray< NSDictionary<NSString *, id> *> *displayOrderMenu =
	[NSArray arrayWithObjects:
	 [self generateDisplayModeItemByID:NDSDisplayOptionID_Order_MainFirst states:s],
	 [self generateDisplayModeItemByID:NDSDisplayOptionID_Order_TouchFirst states:s],
	 nil];
	
	NSArray< NSDictionary<NSString *, id> *> *displaySeparationMenu =
	[NSArray arrayWithObjects:
	 [self generateDisplayModeItemByID:NDSDisplayOptionID_Separation_0 states:s],
	 [self generateDisplayModeItemByID:NDSDisplayOptionID_Separation_50 states:s],
	 [self generateDisplayModeItemByID:NDSDisplayOptionID_Separation_100 states:s],
	 [self generateDisplayModeItemByID:NDSDisplayOptionID_Separation_150 states:s],
	 [self generateDisplayModeItemByID:NDSDisplayOptionID_Separation_200 states:s],
	 nil];
	 
	NSArray< NSDictionary<NSString *, id> *> *displayVideoSourceMenu =
	[NSArray arrayWithObjects:
	 [self generateDisplayModeItemByID:NDSDisplayOptionID_VideoSourceMain_None states:s],
	 [self generateDisplayModeItemByID:NDSDisplayOptionID_VideoSourceMain_NDS states:s],
	 [self generateDisplayModeItemByID:NDSDisplayOptionID_VideoSourceMain_ForceMain states:s],
	 [self generateDisplayModeItemByID:NDSDisplayOptionID_VideoSourceMain_ForceSub states:s],
	 [NSDictionary dictionaryWithObjectsAndKeys:@"---", OEGameCoreDisplayModeSeparatorItemKey, nil],
	 [self generateDisplayModeItemByID:NDSDisplayOptionID_VideoSourceTouch_None states:s],
	 [self generateDisplayModeItemByID:NDSDisplayOptionID_VideoSourceTouch_NDS states:s],
	 [self generateDisplayModeItemByID:NDSDisplayOptionID_VideoSourceTouch_ForceMain states:s],
	 [self generateDisplayModeItemByID:NDSDisplayOptionID_VideoSourceTouch_ForceSub states:s],
	 nil];
	
	NSArray< NSDictionary<NSString *, id> *> *displayHUDMenu =
	[NSArray arrayWithObjects:
	 [self generateDisplayModeItemByID:NDSDisplayOptionID_HUD_ExecutionSpeed states:s],
	 [self generateDisplayModeItemByID:NDSDisplayOptionID_HUD_VideoFPS states:s],
	 [self generateDisplayModeItemByID:NDSDisplayOptionID_HUD_3DRendererFPS states:s],
	 [self generateDisplayModeItemByID:NDSDisplayOptionID_HUD_FrameIndex states:s],
	 [self generateDisplayModeItemByID:NDSDisplayOptionID_HUD_LagFrameCounter states:s],
	 [self generateDisplayModeItemByID:NDSDisplayOptionID_HUD_CPULoadAverage states:s],
	 [self generateDisplayModeItemByID:NDSDisplayOptionID_HUD_RealTimeClock states:s],
	 [self generateDisplayModeItemByID:NDSDisplayOptionID_HUD_Input states:s],
	 nil];
	
	NSArray< NSDictionary<NSString *, id> *> *displayGPUEmulationMenu =
	[NSArray arrayWithObjects:
	 [self generateDisplayModeItemByID:NDSDisplayOptionID_GPU_Engine_Software states:s],
	 [self generateDisplayModeItemByID:NDSDisplayOptionID_GPU_Software_FragmentSamplingHack states:s],
	 [NSDictionary dictionaryWithObjectsAndKeys:@"---", OEGameCoreDisplayModeSeparatorItemKey, nil],
	 [self generateDisplayModeItemByID:NDSDisplayOptionID_GPU_Engine_OpenGL states:s],
	 [self generateDisplayModeItemByID:NDSDisplayOptionID_GPU_OpenGL_SmoothTextures states:s],
	 nil];
	
	NSArray< NSDictionary<NSString *, id> *> *display3DRenderingMenu =
	[NSArray arrayWithObjects:
	 [self generateDisplayModeItemByID:NDSDisplayOptionID_3D_RenderScaling_1x states:s],
	 [self generateDisplayModeItemByID:NDSDisplayOptionID_3D_RenderScaling_2x states:s],
	 [self generateDisplayModeItemByID:NDSDisplayOptionID_3D_RenderScaling_3x states:s],
	 [self generateDisplayModeItemByID:NDSDisplayOptionID_3D_RenderScaling_4x states:s],
	 [self generateDisplayModeItemByID:NDSDisplayOptionID_3D_RenderScaling_5x states:s],
	 [self generateDisplayModeItemByID:NDSDisplayOptionID_3D_RenderScaling_6x states:s],
	 [self generateDisplayModeItemByID:NDSDisplayOptionID_3D_RenderScaling_7x states:s],
	 [self generateDisplayModeItemByID:NDSDisplayOptionID_3D_RenderScaling_8x states:s],
	 [NSDictionary dictionaryWithObjectsAndKeys:@"---", OEGameCoreDisplayModeSeparatorItemKey, nil],
	 [self generateDisplayModeItemByID:NDSDisplayOptionID_3D_TextureScaling_1x states:s],
	 [self generateDisplayModeItemByID:NDSDisplayOptionID_3D_TextureScaling_2x states:s],
	 [self generateDisplayModeItemByID:NDSDisplayOptionID_3D_TextureScaling_4x states:s],
	 nil];
	
	// Add each submenu to the menu descriptor.
	//
	// You may wonder why we're generating the menu from scratch every time. The reason is because
	// OpenEmu wants to have a full immutable copy of the menu descriptor so that it can use it for
	// its Next/Last Display Mode switching feature. Because of this, we can't just generate a
	// single menu descriptor at init time, modify it as needed, and then send OpenEmu the pointer
	// to our menu descriptor, as OpenEmu can modify it in some unpredictable ways. We would have
	// to send a copy to keep our internal working menu descriptor from getting borked.
	//
	// Maintaining a mutable version of the menu descriptor for our internal usage is a pain, as
	// it involves a bunch of hash table lookups and calls to [NSString isEqualToString:], among
	// many other annoyances with dealing with NSMutableDictionary directly. Since we have to send
	// OpenEmu a copy of the menu descriptor anyways, it ends up being easier to just use a single
	// integer ID, NDSDisplayOptionID in our case, to keep track of all of the data associations
	// and then use that ID to generate the menu.
	
	NSArray< NSDictionary<NSString *, id> *> *newDisplayModeMenuDescription =
	[[NSArray alloc] initWithObjects:
	 [NSDictionary dictionaryWithObjectsAndKeys: @"Mode", OEGameCoreDisplayModeGroupNameKey,         displayModeMenu, OEGameCoreDisplayModeGroupItemsKey, nil],
	 [NSDictionary dictionaryWithObjectsAndKeys: @"Layout", OEGameCoreDisplayModeGroupNameKey,       displayLayoutMenu, OEGameCoreDisplayModeGroupItemsKey, nil],
	 [NSDictionary dictionaryWithObjectsAndKeys: @"Rotation", OEGameCoreDisplayModeGroupNameKey,     displayRotationMenu, OEGameCoreDisplayModeGroupItemsKey, nil],
	 [NSDictionary dictionaryWithObjectsAndKeys: @"Order", OEGameCoreDisplayModeGroupNameKey,        displayOrderMenu, OEGameCoreDisplayModeGroupItemsKey, nil],
	 [NSDictionary dictionaryWithObjectsAndKeys: @"Separation", OEGameCoreDisplayModeGroupNameKey,   displaySeparationMenu, OEGameCoreDisplayModeGroupItemsKey, nil],
	 [NSDictionary dictionaryWithObjectsAndKeys: @"Video Source", OEGameCoreDisplayModeGroupNameKey, displayVideoSourceMenu, OEGameCoreDisplayModeGroupItemsKey, nil],
	 [NSDictionary dictionaryWithObjectsAndKeys:@"---", OEGameCoreDisplayModeSeparatorItemKey, nil],
	 [self generateDisplayModeItemByID:NDSDisplayOptionID_HUD_Enable states:s],
	 [NSDictionary dictionaryWithObjectsAndKeys: @"HUD Info Visibility", OEGameCoreDisplayModeGroupNameKey, displayHUDMenu, OEGameCoreDisplayModeGroupItemsKey, nil],
	 [NSDictionary dictionaryWithObjectsAndKeys:@"---", OEGameCoreDisplayModeSeparatorItemKey, nil],
	 [NSDictionary dictionaryWithObjectsAndKeys: @"GPU Emulation", OEGameCoreDisplayModeGroupNameKey, displayGPUEmulationMenu, OEGameCoreDisplayModeGroupItemsKey, nil],
	 [NSDictionary dictionaryWithObjectsAndKeys: @"3D Rendering", OEGameCoreDisplayModeGroupNameKey, display3DRenderingMenu, OEGameCoreDisplayModeGroupItemsKey, nil],
	 nil];
	
	return newDisplayModeMenuDescription;
}

/** Change display mode.
 *  @param displayMode The name of the display mode to enable or disable, as
 *    specified in its OEGameCoreDisplayModeNameKey key. */

- (uint64_t) switchDisplayModeState:(uint64_t)displayModeState nameKey:(NSString *)nameKey
{
	// Validate the incoming nameKey string.
	if (nameKey == nil)
	{
		return displayModeState;
	}
	
	NSNumber *optionIDNumber = [_displayModeIDFromString objectForKey:nameKey];
	if (optionIDNumber == nil)
	{
		return displayModeState;
	}
	
	// Retrieve the NDSDisplayOptionID and generate its associated state bit.
	const NDSDisplayOptionID optionID = (NDSDisplayOptionID)[optionIDNumber integerValue];
	const uint64_t optionBit = (1LLU << optionID);
	
	// Manipulate our current display mode state using only nameKey.
	//
	// OpenEmu menu items are explicitly typed as toggle or group. Technically, these are
	// two separate switches that make for four possible permutations of menu item type.
	// But practically speaking, a toggle item already implies that the menu item is not
	// part of a group, while a group item already implies that the menu item will not
	// toggle. The code below is written in such a way as to reflect the above behavior.
	if (kDisplayModeItem[optionID].groupBitmask == 0)
	{
		displayModeState ^= optionBit;
	}
	else
	{
		DISPLAYMODE_STATEBITGROUP_CLEAR(displayModeState, kDisplayModeItem[optionID].groupBitmask);
		displayModeState |= optionBit;
	}
	
	return displayModeState;
}

- (uint64_t) setDisplayModeState:(uint64_t)displayModeState optionID:(NDSDisplayOptionID)optionID state:(NSNumber *)stateObj
{
	if (stateObj == nil)
	{
		return displayModeState;
	}
	
	const uint64_t optionBit = (1LLU << optionID);
	BOOL newState = [stateObj boolValue];
	
	if (newState)
	{
		displayModeState |= optionBit;
	}
	else
	{
		displayModeState &= ~optionBit;
	}
	
	return displayModeState;
}

- (void)changeDisplayWithMode:(NSString *)displayMode
{
	uint64_t oldState = [self ndsDisplayMode];
	uint64_t newState = [self switchDisplayModeState:oldState nameKey:displayMode];

	if (newState == oldState)
	{
		return;
	}
	
	[self setNdsDisplayMode:newState];
}

// This older version of [OEGameCore changeDisplayMode] exists for backwards compatibility.
// It cycles through the three NDS display modes, and that's it.
- (void)changeDisplayMode
{
	uint64_t displayModeState = [self ndsDisplayMode];
	
	if ( DISPLAYMODE_STATEBIT_CHECK(displayModeState, NDSDisplayOptionID_Mode_DualScreen) )
	{
		[self changeDisplayWithMode:kDisplayModeItem[NDSDisplayOptionID_Mode_Main].nameKey];
	}
	else if ( DISPLAYMODE_STATEBIT_CHECK(displayModeState, NDSDisplayOptionID_Mode_Main) )
	{
		[self changeDisplayWithMode:kDisplayModeItem[NDSDisplayOptionID_Mode_Touch].nameKey];
	}
	else if ( DISPLAYMODE_STATEBIT_CHECK(displayModeState, NDSDisplayOptionID_Mode_Touch) )
	{
		[self changeDisplayWithMode:kDisplayModeItem[NDSDisplayOptionID_Mode_DualScreen].nameKey];
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
	_inputHandler->StartHardwareMicDevice();
	_inputHandler->SetHardwareMicMute(false);
	_inputHandler->SetHardwareMicPause(false);
	
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
	_inputHandler->SetHardwareMicPause((pauseEmulation) ? YES : NO);
	
	if (pauseEmulation)
	{
		[_fpsTimer invalidate];
		_fpsTimer = nil;
	}
	else
	{
		if (_fpsTimer == nil)
		{
			[self newFPSTimer];
		}
	}
	
	[super setPauseEmulation:pauseEmulation];
}

/// When didExecute is called, will be the next wakeup time.
//@property (nonatomic, readonly) NSTimeInterval nextFrameTime;

#pragma mark - Input

- (oneway void)didPushNDSButton:(OENDSButton)button forPlayer:(NSUInteger)player
{
	const NDSInputID theInput = (NDSInputID)inputID[button];
	
	switch (theInput)
	{
		case NDSInputID_Microphone:
			_inputHandler->SetClientSoftwareMicState(true, MicrophoneMode_InternalNoise);
			break;
			
		case NDSInputID_Paddle:
			// Do nothing for now. OpenEmu doesn't currently support the Taito Paddle Controller.
			//_inputHandler->SetClientPaddleAdjust(0);
			break;
			
		default:
			_inputHandler->SetClientInputStateUsingID(theInput, true);
			break;
	}
	
	ClientExecutionControl::UpdateNDSFrameInfoInput(_inputHandler, _ndsFrameInfo);
	_cdp->SetHUDInfo(_clientFrameInfo, _ndsFrameInfo);
}

- (oneway void)didReleaseNDSButton:(OENDSButton)button forPlayer:(NSUInteger)player
{
	const NDSInputID theInput = (NDSInputID)inputID[button];
	
	switch (theInput)
	{
		case NDSInputID_Microphone:
			_inputHandler->SetClientSoftwareMicState(false, MicrophoneMode_InternalNoise);
			break;
			
		case NDSInputID_Paddle:
			_inputHandler->SetClientPaddleAdjust(0);
			break;
			
		default:
			_inputHandler->SetClientInputStateUsingID(theInput, false);
			break;
	}
	
	ClientExecutionControl::UpdateNDSFrameInfoInput(_inputHandler, _ndsFrameInfo);
	_cdp->SetHUDInfo(_clientFrameInfo, _ndsFrameInfo);
}

- (oneway void)didTouchScreenPoint:(OEIntPoint)aPoint
{
	uint64_t displayModeStates = [self ndsDisplayMode];
	
	// Reject touch input if showing only the main screen.
	if ( DISPLAYMODE_STATEBIT_CHECK(displayModeStates, NDSDisplayOptionID_Mode_Main) )
	{
		if (!_isInitialTouchPress)
		{
			[self didReleaseTouch];
		}
		
		return;
	}
	
	// GetNDSPoint() is intended for views where the origin point is in the bottom left,
	// translating it into an NDS point where the origin point is in the top left.
	// OpenEmu tries to be helpful by flipping the origin point to the top left for us,
	// but that's not what GetNDSPoint() wants, and so we need to flip OpenEmu's touch
	// point here.
	aPoint.y = _displayRect.size.height - aPoint.y;
	
	// The display rect should be at 1x normal scaling, but could include prescaling for
	// older OpenEmu versions, so we need to account for both cases here.
	ClientDisplayPresenterProperties propsCopy = _cdp->GetPresenterProperties();
	const double displayRectWidthScaled  = (_canRespondToViewResize) ? _displayRect.size.width  * propsCopy.viewScale : _displayRect.size.width;
	const double displayRectHeightScaled = (_canRespondToViewResize) ? _displayRect.size.height * propsCopy.viewScale : _displayRect.size.height;
	
	// We also need to center the touch point within the view, since OpenEmu has a hard time
	// (due to view behavior bugs) sizing the view to the drawable rect. More than likely,
	// there will be some kind of extraneous area around the drawable area, and so we need
	// to compensate for that here.
	aPoint.x = aPoint.x - (int)( ((((double)_OEViewSize.width  - displayRectWidthScaled)  / 2.0) / propsCopy.viewScale) + 0.0005 );
	aPoint.y = aPoint.y + (int)( ((((double)_OEViewSize.height - displayRectHeightScaled) / 2.0) / propsCopy.viewScale) + 0.0005 );
	
	uint8_t x = 0;
	uint8_t y = 0;
	ClientDisplayViewInterface::GetNDSPoint(propsCopy,
											_displayRect.size.width, _displayRect.size.height,
											aPoint.x, aPoint.y,
											_isInitialTouchPress, x, y, _isTouchPressInMajorDisplay);
	_isInitialTouchPress = false;
	
	_lastTouchLocation = OEIntPointMake(x, y);
	_inputHandler->SetClientTouchState(true, x, y, 50);
	
	ClientExecutionControl::UpdateNDSFrameInfoInput(_inputHandler, _ndsFrameInfo);
	_cdp->SetHUDInfo(_clientFrameInfo, _ndsFrameInfo);
}

- (oneway void)didReleaseTouch
{
	_inputHandler->SetClientTouchState(false, _lastTouchLocation.x, _lastTouchLocation.y, 50);
	_isInitialTouchPress = true;
	
	ClientExecutionControl::UpdateNDSFrameInfoInput(_inputHandler, _ndsFrameInfo);
	_cdp->SetHUDInfo(_clientFrameInfo, _ndsFrameInfo);
}

@end
