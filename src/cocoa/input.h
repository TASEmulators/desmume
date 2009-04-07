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

/* The main window class instanciates an input object,
 and places it after the window in the responder chain, so
 any events not handled by a the window get sent here.
 */

@class VideoOutputWindow;

@interface InputHandler : NSResponder
{
@private
	VideoOutputWindow *my_ds;
}
//preferences
+ (NSView*)createPreferencesView:(float)width;
+ (NSDictionary*)appDefaults;

//creation/deletion
- (id)initWithWindow:(VideoOutputWindow*)nds;
- (void)dealloc;

//keyboard input
- (void)keyDown:(NSEvent*)event;
- (void)keyUp:(NSEvent*)event;

//mouse input
- (void)mouseDown:(NSEvent*)event;
- (void)mouseDragged:(NSEvent*)event;
- (void)mouseUp:(NSEvent*)event;
@end
