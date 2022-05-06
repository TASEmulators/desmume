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

#import <Cocoa/Cocoa.h>
#import <OpenEmuBase/OEGameCore.h>
#import "OENDSSystemResponderClient.h"
#import "../cocoa_input.h"
#include "../utilities.h"
#include <pthread.h>

#include "../ClientExecutionControl.h"
#include "OEDisplayView.h"

#define SILENCE_DEPRECATION_OPENEMU(expression)  SILENCE_DEPRECATION(expression)

class ClientInputHandler;
class OE_OGLDisplayPresenter;

@class CocoaDSCheatManager;
@class CocoaDSGPU;
@class CocoaDSFirmware;
	
// NDS Display Mode Keys for OpenEmu Display Mode Options
//
// Things to be aware of in OpenEmu's API:
// 1. All options are searched with a single OEGameCoreDisplayModeNameKey, and therefore must be unique.
// 2. Because all options are searched with a single OEGameCoreDisplayModeNameKey, the search list best
//    takes the form of a 1-dimensional list (no nested stuff).
// 2. OEGameCoreDisplayModeNameKey is also used for the presented name in the "Display Modes" GUI menu.
//    This implies that all presented menu item names are forced to be unique.
// 3. The actual generated menu uses an NSArray of NSDictionary objects, in which the position of the
//    menu items is dependent on the order of the NSDictionary objects in the NSArray.
#define NDSDISPLAYMODE_NAMEKEY_MODE_DUALSCREEN             "Dual Screen"
#define NDSDISPLAYMODE_NAMEKEY_MODE_MAIN                   "Main"
#define NDSDISPLAYMODE_NAMEKEY_MODE_TOUCH                  "Touch"
	
#define NDSDISPLAYMODE_NAMEKEY_LAYOUT_VERTICAL             "Vertical"
#define NDSDISPLAYMODE_NAMEKEY_LAYOUT_HORIZONTAL           "Horizontal"
#define NDSDISPLAYMODE_NAMEKEY_LAYOUT_HYBRID_2_1           "Hybrid (2:1)"
#define NDSDISPLAYMODE_NAMEKEY_LAYOUT_HYBRID_16_9          "Hybrid (16:9)"
#define NDSDISPLAYMODE_NAMEKEY_LAYOUT_HYBRID_16_10         "Hybrid (16:10)"
	
#define NDSDISPLAYMODE_NAMEKEY_DISPLAYORDER_MAIN           "Main First"
#define NDSDISPLAYMODE_NAMEKEY_DISPLAYORDER_TOUCH          "Touch First"
	
#define NDSDISPLAYMODE_NAMEKEY_ROTATION_0                  "0°"
#define NDSDISPLAYMODE_NAMEKEY_ROTATION_90                 "90°"
#define NDSDISPLAYMODE_NAMEKEY_ROTATION_180                "180°"
#define NDSDISPLAYMODE_NAMEKEY_ROTATION_270                "270°"
	
#define NDSDISPLAYMODE_NAMEKEY_SEPARATION_0                "0%"
#define NDSDISPLAYMODE_NAMEKEY_SEPARATION_50               "50%"
#define NDSDISPLAYMODE_NAMEKEY_SEPARATION_100              "100%"
#define NDSDISPLAYMODE_NAMEKEY_SEPARATION_150              "150%"
#define NDSDISPLAYMODE_NAMEKEY_SEPARATION_200              "200%"
	
#define NDSDISPLAYMODE_NAMEKEY_VIDEOSOURCEMAIN_NONE        "Main Display - None"
#define NDSDISPLAYMODE_NAMEKEY_VIDEOSOURCEMAIN_NDS         "Main Display - Let NDS Decide"
#define NDSDISPLAYMODE_NAMEKEY_VIDEOSOURCEMAIN_FORCEMAIN   "Main Display - Force Main Engine"
#define NDSDISPLAYMODE_NAMEKEY_VIDEOSOURCEMAIN_FORCESUB    "Main Display - Force Sub Engine"
	
#define NDSDISPLAYMODE_NAMEKEY_VIDEOSOURCETOUCH_NONE       "Touch Display - None"
#define NDSDISPLAYMODE_NAMEKEY_VIDEOSOURCETOUCH_NDS        "Touch Display - Let NDS Decide"
#define NDSDISPLAYMODE_NAMEKEY_VIDEOSOURCETOUCH_FORCEMAIN  "Touch Display - Force Main Engine"
#define NDSDISPLAYMODE_NAMEKEY_VIDEOSOURCETOUCH_FORCESUB   "Touch Display - Force Sub Engine"

#define NDSDISPLAYMODE_NAMEKEY_HUD_ENABLE                  "Enable HUD"
#define NDSDISPLAYMODE_NAMEKEY_HUD_EXECUTIONSPEED          "Execution Speed"
#define NDSDISPLAYMODE_NAMEKEY_HUD_VIDEOFPS                "Video FPS"
#define NDSDISPLAYMODE_NAMEKEY_HUD_3DRENDERERFPS           "3D Renderer FPS"
#define NDSDISPLAYMODE_NAMEKEY_HUD_FRAMEINDEX              "Frame Index"
#define NDSDISPLAYMODE_NAMEKEY_HUD_LAGFRAMECOUNTER         "Lag Frame Counter"
#define NDSDISPLAYMODE_NAMEKEY_HUD_CPULOADAVERAGE          "CPU Load Average"
#define NDSDISPLAYMODE_NAMEKEY_HUD_REALTIMECLOCK           "Real-Time Clock"
#define NDSDISPLAYMODE_NAMEKEY_HUD_INPUT                   "Input"

#define NDSDISPLAYMODE_NAMEKEY_3D_RENDERSCALING_1X         "1x Native Resolution"
#define NDSDISPLAYMODE_NAMEKEY_3D_RENDERSCALING_2X         "2x Resolution"
#define NDSDISPLAYMODE_NAMEKEY_3D_RENDERSCALING_3X         "3x Resolution"
#define NDSDISPLAYMODE_NAMEKEY_3D_RENDERSCALING_4X         "4x Resolution"

#define NDSDISPLAYMODE_NAMEKEY_3D_TEXTURESCALING_1X        "1x Native Texture"
#define NDSDISPLAYMODE_NAMEKEY_3D_TEXTURESCALING_2X        "2x Texture Upscaling"
#define NDSDISPLAYMODE_NAMEKEY_3D_TEXTURESCALING_4X        "4x Texture Upscaling"
	
#define NDSDISPLAYMODE_PREFKEY_DISPLAYMODE                 "displayMode"
#define NDSDISPLAYMODE_PREFKEY_LAYOUT                      "layout"
#define NDSDISPLAYMODE_PREFKEY_ORDER                       "dualOrder"
#define NDSDISPLAYMODE_PREFKEY_ROTATION                    "rotation"
#define NDSDISPLAYMODE_PREFKEY_SEPARATION                  "gap"
#define NDSDISPLAYMODE_PREFKEY_VIDEOSOURCE_MAIN            "videosource_main"
#define NDSDISPLAYMODE_PREFKEY_VIDEOSOURCE_TOUCH           "videosource_touch"
#define NDSDISPLAYMODE_PREFKEY_HUD_ENABLE                  "hud_enable"
#define NDSDISPLAYMODE_PREFKEY_HUD_EXECUTIONSPEED          "hud_executionspeed"
#define NDSDISPLAYMODE_PREFKEY_HUD_VIDEOFPS                "hud_videofps"
#define NDSDISPLAYMODE_PREFKEY_HUD_3DRENDERERFPS           "hud_3drendererfps"
#define NDSDISPLAYMODE_PREFKEY_HUD_FRAMEINDEX              "hud_frameindex"
#define NDSDISPLAYMODE_PREFKEY_HUD_LAGFRAMECOUNTER         "hud_lagframecounter"
#define NDSDISPLAYMODE_PREFKEY_HUD_CPULOADAVERAGE          "hud_cpuloadaverage"
#define NDSDISPLAYMODE_PREFKEY_HUD_REALTIMECLOCK           "hud_realtimeclock"
#define NDSDISPLAYMODE_PREFKEY_HUD_INPUT                   "hud_input"
#define NDSDISPLAYMODE_PREFKEY_3D_RENDERSCALING            "3D_renderscaling"
#define NDSDISPLAYMODE_PREFKEY_3D_TEXTURESCALING           "3D_texturescaling"

// These IDs are used to maintain the data associations for all of
// the display mode menu items.
enum NDSDisplayOptionID
{
	NDSDisplayOptionID_Mode_DualScreen = 0,
	NDSDisplayOptionID_Mode_Main,
	NDSDisplayOptionID_Mode_Touch,
	
	NDSDisplayOptionID_Rotation_0,
	NDSDisplayOptionID_Rotation_90,
	NDSDisplayOptionID_Rotation_180,
	NDSDisplayOptionID_Rotation_270,
	
	NDSDisplayOptionID_Layout_Vertical,
	NDSDisplayOptionID_Layout_Horizontal,
	NDSDisplayOptionID_Layout_Hybrid_2_1,
	NDSDisplayOptionID_Layout_Hybrid_16_9,
	NDSDisplayOptionID_Layout_Hybrid_16_10,
	
	NDSDisplayOptionID_Order_MainFirst,
	NDSDisplayOptionID_Order_TouchFirst,
	
	NDSDisplayOptionID_Separation_0,
	NDSDisplayOptionID_Separation_50,
	NDSDisplayOptionID_Separation_100,
	NDSDisplayOptionID_Separation_150,
	NDSDisplayOptionID_Separation_200,
	
	NDSDisplayOptionID_VideoSourceMain_None,
	NDSDisplayOptionID_VideoSourceMain_NDS,
	NDSDisplayOptionID_VideoSourceMain_ForceMain,
	NDSDisplayOptionID_VideoSourceMain_ForceSub,
	
	NDSDisplayOptionID_VideoSourceTouch_None,
	NDSDisplayOptionID_VideoSourceTouch_NDS,
	NDSDisplayOptionID_VideoSourceTouch_ForceMain,
	NDSDisplayOptionID_VideoSourceTouch_ForceSub,
	
	NDSDisplayOptionID_HUD_Enable,
	NDSDisplayOptionID_HUD_ExecutionSpeed,
	NDSDisplayOptionID_HUD_VideoFPS,
	NDSDisplayOptionID_HUD_3DRendererFPS,
	NDSDisplayOptionID_HUD_FrameIndex,
	NDSDisplayOptionID_HUD_LagFrameCounter,
	NDSDisplayOptionID_HUD_CPULoadAverage,
	NDSDisplayOptionID_HUD_RealTimeClock,
	NDSDisplayOptionID_HUD_Input,
	
	NDSDisplayOptionID_3D_RenderScaling_1x,
	NDSDisplayOptionID_3D_RenderScaling_2x,
	NDSDisplayOptionID_3D_RenderScaling_3x,
	NDSDisplayOptionID_3D_RenderScaling_4x,
	
	NDSDisplayOptionID_3D_TextureScaling_1x,
	NDSDisplayOptionID_3D_TextureScaling_2x,
	NDSDisplayOptionID_3D_TextureScaling_4x,
	
	NDSDisplayOptionID_Count
};

// Bitmasks that define the groupings for certain display mode options.
#define NDSDISPLAYMODE_GROUPBITMASK_DISPLAYMODE      ((1ULL << NDSDisplayOptionID_Mode_DualScreen) | \
                                                      (1ULL << NDSDisplayOptionID_Mode_Main) | \
                                                      (1ULL << NDSDisplayOptionID_Mode_Touch))

#define NDSDISPLAYMODE_GROUPBITMASK_ROTATION         ((1ULL << NDSDisplayOptionID_Rotation_0) | \
                                                      (1ULL << NDSDisplayOptionID_Rotation_90) | \
                                                      (1ULL << NDSDisplayOptionID_Rotation_180) | \
                                                      (1ULL << NDSDisplayOptionID_Rotation_270))

#define NDSDISPLAYMODE_GROUPBITMASK_LAYOUT           ((1ULL << NDSDisplayOptionID_Layout_Vertical) | \
                                                      (1ULL << NDSDisplayOptionID_Layout_Horizontal) | \
                                                      (1ULL << NDSDisplayOptionID_Layout_Hybrid_2_1) | \
                                                      (1ULL << NDSDisplayOptionID_Layout_Hybrid_16_9) | \
                                                      (1ULL << NDSDisplayOptionID_Layout_Hybrid_16_10))

#define NDSDISPLAYMODE_GROUPBITMASK_ORDER            ((1ULL << NDSDisplayOptionID_Order_MainFirst) | \
                                                      (1ULL << NDSDisplayOptionID_Order_TouchFirst))

#define NDSDISPLAYMODE_GROUPBITMASK_SEPARATION       ((1ULL << NDSDisplayOptionID_Separation_0) | \
                                                      (1ULL << NDSDisplayOptionID_Separation_50) | \
                                                      (1ULL << NDSDisplayOptionID_Separation_100) | \
                                                      (1ULL << NDSDisplayOptionID_Separation_150) | \
                                                      (1ULL << NDSDisplayOptionID_Separation_200))

#define NDSDISPLAYMODE_GROUPBITMASK_VIDEOSOURCEMAIN  ((1ULL << NDSDisplayOptionID_VideoSourceMain_None) | \
                                                      (1ULL << NDSDisplayOptionID_VideoSourceMain_NDS) | \
                                                      (1ULL << NDSDisplayOptionID_VideoSourceMain_ForceMain) | \
                                                      (1ULL << NDSDisplayOptionID_VideoSourceMain_ForceSub))

#define NDSDISPLAYMODE_GROUPBITMASK_VIDEOSOURCETOUCH ((1ULL << NDSDisplayOptionID_VideoSourceTouch_None) | \
                                                      (1ULL << NDSDisplayOptionID_VideoSourceTouch_NDS) | \
                                                      (1ULL << NDSDisplayOptionID_VideoSourceTouch_ForceMain) | \
                                                      (1ULL << NDSDisplayOptionID_VideoSourceTouch_ForceSub))

#define NDSDISPLAYMODE_GROUPBITMASK_RENDERSCALING    ((1ULL << NDSDisplayOptionID_3D_RenderScaling_1x) | \
                                                      (1ULL << NDSDisplayOptionID_3D_RenderScaling_2x) | \
                                                      (1ULL << NDSDisplayOptionID_3D_RenderScaling_3x) | \
                                                      (1ULL << NDSDisplayOptionID_3D_RenderScaling_4x))

#define NDSDISPLAYMODE_GROUPBITMASK_TEXTURESCALING   ((1ULL << NDSDisplayOptionID_3D_TextureScaling_1x) | \
                                                      (1ULL << NDSDisplayOptionID_3D_TextureScaling_2x) | \
                                                      (1ULL << NDSDisplayOptionID_3D_TextureScaling_4x))

// Describes the data associations for a single display mode menu item, used for generating
// the NSDictionary items that are part of the display mode menu descriptor. This struct
// represents the immutable portions of the data. The mutable parts, such as the menu item
// states, are handled elsewhere.
struct OEMenuItemDesc
{
	NSString *nameKey; // The displayed name of the menu item. Must conform to OEGameCoreDisplayModeNameKey.
	NSString *prefKey; // The user defaults key of the menu item. Must conform to OEGameCoreDisplayModePrefKeyNameKey.
	
	uint64_t groupBitmask; // The associated state bits of all menu items that belong in the same group.
	                       // Assigning zero or non-zero controls the behavior of this menu item.
	                       //   Zero     - Toggle type. This item can be toggled and/or selected all by itself.
	                       //   Non-zero - Group type. This item cannot be toggled, and selection is mutually exclusive
	                       //              with its associated group. The group is defined by using these bits.
	
	
	// Note: Alternate menu item names are unsupported in OpenEmu. This feature is currently unimplemented.
	//
	// OpenEmu ties the presented menu item state, denoted by a check mark, with writing out that state
	// to user defaults. In other words, using alternate names for presentation in lieu of a check mark
	// will disrupt OpenEmu's user defaults system. And so to retain a consistent user defaults behavior,
	// we cannot use alternate menu item names in OpenEmu.
	//
	// This item exists here just in case OpenEmu ever decides to decouple presentation with state. If
	// they do, then we'll have another presentation option here.
	NSString *altNameKey; // The alternate displayed name of the menu item. Must conform to OEGameCoreDisplayModeNameKey.
	                      // Only used for toggle-type items and ignored for group-type items.
	                      //   nil value - Do not use altNameKey when toggle, but use a check mark to denote state.
	                      //   Assigned value - Switches between nameKey and altNameKey to denote state.
};
typedef struct OEMenuItemDesc OEMenuItemDesc;

@interface NDSGameCore : OEGameCore
{
	apple_unfairlock_t unfairlockDisplayMode;
	pthread_rwlock_t rwlockCoreExecute;
	
	NSMutableDictionary *addedCheatsDict;
	CocoaDSCheatManager *cdsCheats;
	CocoaDSGPU *cdsGPU;
	CocoaDSFirmware *cdsFirmware;
	
	MacInputHandler *_inputHandler;
	OEIntPoint _lastTouchLocation;
	bool _isInitialTouchPress;
	bool _isTouchPressInMajorDisplay;
	
	NSUInteger inputID[OENDSButtonCount]; // Key = OpenEmu's input ID, Value = DeSmuME's input ID
	OEIntRect _displayRect;
	OEIntSize _displayAspectRatio;
	OEIntSize _displayBufferSize;
	OEIntSize _OEViewSize;
	BOOL _canRespondToViewResize;
	NSUInteger _selectedRenderScaling;
	NSUInteger _selectedTextureScaling;
	
	bool _willUseOpenGL3Context;
	CGLContextObj _videoContext;
	OE_OGLDisplayPresenter *_cdp;
	
	NSTimer *_fpsTimer;
	BOOL _isTimerAtSecond;
	apple_unfairlock_t _unfairlockReceivedFrameIndex;
	apple_unfairlock_t _unfairlockNDSFrameInfo;
	
	uint32_t _receivedFrameIndex;
	uint32_t _currentReceivedFrameIndex;
	uint32_t _receivedFrameCount;
	
	double _executionSpeedAverageFramesCollected;
	
	ClientFrameInfo _clientFrameInfo;
	NDSFrameInfo _ndsFrameInfo;
	
	// Records the display mode menu states on a per-bit basis. This only works because OpenEmu
	// tracks only binary states for the menu items. Currently, there are 27 different states
	// to keep track of, which leaves us another 37 states for future use.
	uint64_t _displayModeStatesPending;
	uint64_t _displayModeStatesApplied;
	
	// For simplicity's sake, we use NDSDisplayOptionID numbers to maintain all of the internal data
	// associations for display mode menu items. However, OpenEmu uses OEGameCoreDisplayModeNameKey
	// to request display mode changes. Therefore, we need to use an NSDictionary to translate
	// between OEGameCoreDisplayModeNameKey and NDSDisplayOptionID.
	//
	// Using NSDictionary has a hidden bonus here over simply using std::map. The
	// [NSDictionary objectForKey:] method will return nil if the passed in key is not present in
	// the NSDictionary, which is a useful feature for key validation.
	NSDictionary<NSString *, NSNumber *> *_displayModeIDFromString;
}

@property (retain) CocoaDSCheatManager *cdsCheats;
@property (retain) CocoaDSGPU *cdsGPU;
@property (retain) CocoaDSFirmware *cdsFirmware;
@property (assign) uint64_t ndsDisplayMode;

- (void) initVideoWithCurrentOpenEmuContext;

- (void) newFPSTimer;
- (void) getTimedEmulatorStatistics:(NSTimer *)timer;

- (uint64_t) switchDisplayModeState:(uint64_t)displayModeState nameKey:(NSString *)nameKey;
- (uint64_t) setDisplayModeState:(uint64_t)displayModeState optionID:(NDSDisplayOptionID)optionID state:(NSNumber *)stateObj;
- (NSDictionary<NSString *, id> *) generateDisplayModeItemByID:(NDSDisplayOptionID)optionID states:(const uint64_t)states;

@end
