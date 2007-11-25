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

#ifndef SCREENSHOT_H
#define SCREENSHOT_H

#import <Cocoa/Cocoa.h>

#define OBJ_C
#include "../types.h"
#undef BOOL

@interface Screenshot : NSObject
{
	NSWindow *window;
	NSImage *image;
	NSImageView *image_view;
	NSButton *save_button;
	NSButton *update_button;
	NSBitmapImageRep *image_rep;
	NSView *format_selection;
	NSPopUpButton *format_button;
}

- (id)initWithBuffer:(const u8*)buffer rotation:(u8)rotation saveOnly:(BOOL)save_only;
@end

#endif
