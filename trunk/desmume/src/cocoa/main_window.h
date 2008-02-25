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

#ifndef MAIN_WINDOW_H
#define MAIN_WINDOW_H

#import <Cocoa/Cocoa.h>

@class VideoOutputView;
//this is used internally by VideoOutputWindow


//This interface is to create and manaage the window
//that displays DS video output and takes keyboard/mouse input
//do not instanciate more than one of these
@interface VideoOutputWindow : NSWindow
{
	@private
	NSWindowController *controller;
	VideoOutputView *video_output_view;
}

//initialization
- (id)init;
- (void)dealloc;

//call this function to read the screen from the emulator and update the window
//this is the only method involving cocoa that should be called from the run() function
- (void)updateScreen;

//
- (void)clearScreenWhite; //for when a rom is loaded but stopped
- (void)clearScreenBlack; //for when the emulator is not loaded

//resets the min size internalls (when rotation changes), shouldn't be used from outside
- (void)resetMinSize:(bool)resize;

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

//
- (void)toggleAllowsResize;
- (void)toggleConstrainProportions;

//rotation
- (void)setRotation0;
- (void)setRotation90;
- (void)setRotation180;
- (void)setRotation270;

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
- (void)screenShotToFile;
- (void)screenShotToWindow;
- (const unsigned char *)getBuffer;//pause before calling

//keyboard input
- (void)keyDown:(NSEvent*)event;
- (void)keyUp:(NSEvent*)event;

//mouse input
- (void)mouseDown:(NSEvent*)event;
- (void)mouseDragged:(NSEvent*)event;
- (void)mouseUp:(NSEvent*)event;

@end

#endif //MAIN_WINDOW_H
