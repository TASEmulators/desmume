/*  Copyright (C) 2007 Jeff Bland

    This file is part of DeSmuME

    DeSmuME is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    DeSmuME is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with DeSmuME; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

#ifndef GLOBALS_H_INCLUDED
#define GLOBALS_H_INCLUDED

#import <Cocoa/cocoa.h>

//fixme
#define desmume_TRUE TRUE

//Nintendo DS constants---------------------------------------------------------------------------

//This just improves legibility throughut the code
//by showing you names instead of numbers

#define DS_SCREEN_WIDTH 256
#define DS_SCREEN_HEIGHT 192
#define DS_SCREEN_HEIGHT_COMBINED (192*2) /*height of the two screens*/
#define DS_SCREEN_X_RATIO (256.0 / (192.0 * 2.0))
#define DS_SCREEN_Y_RATIO ((192.0 * 2.0) / 256.0)

//Port Specific constants ---------------------------------------------------------------------

#define ROTATION_0   0
#define ROTATION_90  1
#define ROTATION_180 2
#define ROTATION_270 3

//Cocoa Util------------------------------------------------------------------------------------
//These are just useful little functions to help with stuff
//defined in cocoa_util.m

//c string to NSString
#define NSSTRc(x) ([[NSString alloc] initWithCString:(x) encoding:NSASCIIStringEncoding])//dialogs

void messageDialogBlank();
void messageDialog(NSString *title, NSString *text);
BOOL messageDialogYN(NSString *title, NSString *text);
NSString* openDialog(NSArray *file_types);

//Menus -------------------------------------------------------------------------------------------
//These are all the menu items that need checkmarks
//or their enabled status managed

//global menu (defined in main.m)
extern NSMenu *menu;

//view (defined/managed in main_window.m)
extern NSMenuItem *resize1x;
extern NSMenuItem *resize2x;
extern NSMenuItem *resize3x;
extern NSMenuItem *resize4x;

extern NSMenuItem *allows_resize_item;
extern NSMenuItem *constrain_item;
extern NSMenuItem *min_size_item;

extern NSMenuItem *rotation0_item;
extern NSMenuItem *rotation90_item;
extern NSMenuItem *rotation180_item;
extern NSMenuItem *rotation270_item;

extern NSMenuItem *topBG0_item;
extern NSMenuItem *topBG1_item;
extern NSMenuItem *topBG2_item;
extern NSMenuItem *topBG3_item;
extern NSMenuItem *subBG0_item;
extern NSMenuItem *subBG1_item;
extern NSMenuItem *subBG2_item;
extern NSMenuItem *subBG3_item;

//execution control (defined/managed in nds_control.m)
extern NSMenuItem *close_rom_item;

extern NSMenuItem *execute_item;
extern NSMenuItem *pause_item;
extern NSMenuItem *reset_item;

extern NSMenuItem *save_state_as_item;
extern NSMenuItem *load_state_from_item;

//sound (defined/managed in nds_control.m)
extern NSMenuItem *volume_item[10];
extern NSMenuItem *mute_item;

#define SAVE_SLOTS 10 //this should never be more than NB_SAVES in saves.h
extern NSMenuItem *saveSlot_item[SAVE_SLOTS];
extern NSMenuItem *loadSlot_item[SAVE_SLOTS];

#define MAX_FRAME_SKIP 10 //this is actually 1 more than the highest frame skip
extern NSMenuItem *frame_skip_auto_item;
extern NSMenuItem *frame_skip_item[SAVE_SLOTS];

//extern NSMenuItem *clear_all_saves_item; waiting for more functionality from saves.h

extern NSMenuItem *rom_info_item;

//Cocoa Port Stuff ---------------------------------------------------------------------------------

//Enable this to multithread the program
//#define MULTITHREADED

//prototype for our NintendoDS class (defined in nds_control.h)
//
@class NintendoDS;

//Here's our global DS object
//which controls things like pausing and whatnot
extern NintendoDS *NDS;

@class VideoOutputWindow;
extern VideoOutputWindow *main_window;

void setAppDefaults(); //this is defined in preferences.m and should be called at app launch

void clearEvents(bool wait);

//--------------------------------------------------------------------------------------------------

extern volatile int /*desmume_BOOL*/ execute;

extern volatile BOOL finished;
extern volatile BOOL paused;

//--------------------------------------------------------------------------------------------------

#ifndef __OBJC2__
typedef int NSInteger;
typedef unsigned NSUInteger;
#endif

#endif // GLOBALS_H_INCLUDED
