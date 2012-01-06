/*
	Copyright (C) 2007 Jeff Bland
	Copyright (C) 2011 Roger Manuel
	Copyright (C) 2012 DeSmuME team

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

//This class uses OpenGL for drawing for speed
//if opengl is not available it uses NSImage

@class ScreenState;

@interface VideoOutputView :
#ifdef HAVE_OPENGL
NSView
#else
NSImageView
#endif
{
#ifdef HAVE_OPENGL
	NSOpenGLContext* context;
#endif
	ScreenState *screen_buffer;
}
//init
- (id)initWithFrame:(NSRect)frame;

//image to display
- (void)setScreenState:(ScreenState*)screen;
- (const ScreenState*)screenState;

//size in pixels of screen display (disreguarding rotation of the view)
- (float)screenHeight;
- (float)screenWidth;
@end

