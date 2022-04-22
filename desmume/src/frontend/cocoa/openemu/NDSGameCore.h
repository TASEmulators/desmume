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

@class CocoaDSCheatManager;
@class CocoaDSController;
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
#define NDSDISPLAYMODE_NAMEKEY_VIDEOSOURCEMAIN_FORCETOUCH  "Main Display - Force Touch Engine"
	
#define NDSDISPLAYMODE_NAMEKEY_VIDEOSOURCETOUCH_NONE       "Touch Display - None"
#define NDSDISPLAYMODE_NAMEKEY_VIDEOSOURCETOUCH_NDS        "Touch Display - Let NDS Decide"
#define NDSDISPLAYMODE_NAMEKEY_VIDEOSOURCETOUCH_FORCEMAIN  "Touch Display - Force Main Engine"
#define NDSDISPLAYMODE_NAMEKEY_VIDEOSOURCETOUCH_FORCETOUCH "Touch Display - Force Touch Engine"
	
#define NDSDISPLAYMODE_PREFKEY_DISPLAYMODE                 "displayMode"
#define NDSDISPLAYMODE_PREFKEY_LAYOUT                      "layout"
#define NDSDISPLAYMODE_PREFKEY_ORDER                       "dualOrder"
#define NDSDISPLAYMODE_PREFKEY_ROTATION                    "rotation"
#define NDSDISPLAYMODE_PREFKEY_SEPARATION                  "gap"
#define NDSDISPLAYMODE_PREFKEY_VIDEOSOURCE_MAIN            "videosource_main"
#define NDSDISPLAYMODE_PREFKEY_VIDEOSOURCE_TOUCH           "videosource_touch"

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
	NDSDisplayOptionID_VideoSourceMain_ForceTouch,
	
	NDSDisplayOptionID_VideoSourceTouch_None,
	NDSDisplayOptionID_VideoSourceTouch_NDS,
	NDSDisplayOptionID_VideoSourceTouch_ForceMain,
	NDSDisplayOptionID_VideoSourceTouch_ForceTouch,
	
	NDSDisplayOptionID_Count
};

// Used to track the bits that each option group uses, useful for clearing the
// entire group of bits prior to setting a single bit for that particular group.
enum NDSDisplayOptionStateBitmask
{
	NDSDisplayOptionStateBitmask_DisplayMode      = (1 << NDSDisplayOptionID_Mode_DualScreen) | (1 << NDSDisplayOptionID_Mode_Main) | (1 << NDSDisplayOptionID_Mode_Touch),
	NDSDisplayOptionStateBitmask_Rotation         = (1 << NDSDisplayOptionID_Rotation_0) | (1 << NDSDisplayOptionID_Rotation_90) | (1 << NDSDisplayOptionID_Rotation_180) | (1 << NDSDisplayOptionID_Rotation_270),
	NDSDisplayOptionStateBitmask_Layout           = (1 << NDSDisplayOptionID_Layout_Vertical) | (1 << NDSDisplayOptionID_Layout_Horizontal) | (1 << NDSDisplayOptionID_Layout_Hybrid_2_1) | (1 << NDSDisplayOptionID_Layout_Hybrid_16_9) | (1 << NDSDisplayOptionID_Layout_Hybrid_16_10),
	NDSDisplayOptionStateBitmask_Order            = (1 << NDSDisplayOptionID_Order_MainFirst) | (1 << NDSDisplayOptionID_Order_TouchFirst),
	NDSDisplayOptionStateBitmask_Separation       = (1 << NDSDisplayOptionID_Separation_0) | (1 << NDSDisplayOptionID_Separation_50) | (1 << NDSDisplayOptionID_Separation_100) | (1 << NDSDisplayOptionID_Separation_150) | (1 << NDSDisplayOptionID_Separation_200),
	NDSDisplayOptionStateBitmask_VideoSourceMain  = (1 << NDSDisplayOptionID_VideoSourceMain_None) | (1 << NDSDisplayOptionID_VideoSourceMain_NDS) | (1 << NDSDisplayOptionID_VideoSourceMain_ForceMain) | (1 << NDSDisplayOptionID_VideoSourceMain_ForceTouch),
	NDSDisplayOptionStateBitmask_VideoSourceTouch = (1 << NDSDisplayOptionID_VideoSourceTouch_None) | (1 << NDSDisplayOptionID_VideoSourceTouch_NDS) | (1 << NDSDisplayOptionID_VideoSourceTouch_ForceMain) | (1 << NDSDisplayOptionID_VideoSourceTouch_ForceTouch)
};

// Describes the data associations for a single display mode menu item, used for generating
// the NSDictionary items that are part of the display mode menu descriptor. This struct
// represents the immutable portions of the data. The mutable parts, such as the menu item
// states, are handled elsewhere.
struct NDSDisplayMenuItem
{
	NSString *nameKey;
	NSString *prefKey;
};
typedef NDSDisplayMenuItem NDSDisplayMenuItem;

@interface NDSGameCore : OEGameCore
{
	apple_unfairlock_t unfairlockDisplayMode;
	pthread_rwlock_t rwlockCoreExecute;
	
	NSPoint touchLocation;
	NSMutableDictionary *addedCheatsDict;
	CocoaDSCheatManager *cdsCheats;
	CocoaDSController *cdsController;
	CocoaDSGPU *cdsGPU;
	CocoaDSFirmware *cdsFirmware;
	
	NSUInteger inputID[OENDSButtonCount]; // Key = OpenEmu's input ID, Value = DeSmuME's input ID
	OEIntRect _displayRect;
	OEIntSize _displayAspectRatio;
	
	// Records the display mode menu states on a per-bit basis. This only works because OpenEmu
	// tracks only binary states for the menu items. Currently, there are 27 different states
	// to keep track of, which leaves us another 37 states for future use.
	uint64_t ndsDisplayMode;
	
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
@property (retain) CocoaDSController *cdsController;
@property (retain) CocoaDSGPU *cdsGPU;
@property (retain) CocoaDSFirmware *cdsFirmware;
@property (assign) uint64_t ndsDisplayMode;

- (NSDictionary<NSString *, id> *) generateDisplayModeItemByID:(NDSDisplayOptionID)optionID states:(const uint64_t)states;

@end
