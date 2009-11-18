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

#import <Cocoa/Cocoa.h>

#import "nds_control.h"

@class VideoOutputView;
@class InputHandler;
//this is used internally by VideoOutputWindow

// Backup media type array length
#define MAX_SAVE_TYPE 7


//This interface is to create and manaage the window
//that displays DS video output and takes keyboard/mouse input
//do not instanciate more than one of these
@interface VideoOutputWindow : NintendoDS
{
	@private
	NSWindow *window;
	NSWindowController *controller;
	VideoOutputView *video_output_view;
	NSTextField *status_view;
	NSString *status_bar_text;
	InputHandler *input;

	bool no_smaller_than_ds;
	bool keep_proportions;
}

//initialization
- (id)init;
- (void)dealloc;

//overloaded from NintendoDS class
- (BOOL)loadROM:(NSString*)filename;
- (void)execute;
- (void)pause;
- (void)reset;
- (void)setFrameSkip:(int)frameskip;
- (void)setSaveType:(int)savetype;
- (void)closeROM;

//save features overloaded from nds class
- (BOOL)saveState:(NSString*)file;
- (BOOL)loadState:(NSString*)file;
- (BOOL)saveStateToSlot:(int)slot; //0 to MAX_SLOTS-1, anything else is ignored
- (BOOL)loadStateFromSlot:(int)slot;

//save functions for the program menu to callback to
- (BOOL)saveStateAs;
- (BOOL)loadStateFrom;

//toggles between allowing tiny sizes and only being as small as DS
- (void)toggleMinSize;

//this method will contrain the size as well
//this is screen size not window size
- (void)resizeScreen:(NSSize)size;

//like resizeScreen but does a size in relation to DS pixels
- (void)resizeScreen1x;
- (void)resizeScreen2x;
- (void)resizeScreen3x;
- (void)resizeScreen4x;

//converts window coords to DS coords (returns -1, -1 if invalid)
- (NSPoint)windowPointToDSCoords:(NSPoint)location;

//
- (void)toggleConstrainProportions;
- (void)toggleStatusBar;

//rotation
- (void)setRotation:(float)rotation;
- (void)setRotation0;
- (void)setRotation90;
- (void)setRotation180;
- (void)setRotation270;
- (float)rotation;

//layers
- (void)toggleTopBackground0;
- (void)toggleTopBackground1;
- (void)toggleTopBackground2;
- (void)toggleTopBackground3;
- (void)toggleSubBackground0;
- (void)toggleSubBackground1;
- (void)toggleSubBackground2;
- (void)toggleSubBackground3;

//screenshots
- (void)saveScreenshot;

//delegate
- (void)windowDidBecomeMain:(NSNotification*)notification;

@end
