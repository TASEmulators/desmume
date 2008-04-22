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

#import "video_output_view.h"
#import "nds_control.h" //for screenstate

#import <OpenGL/gl.h>

@implementation VideoOutputView

- (id)initWithFrame:(NSRect)frame withDS:(NintendoDS*)ds
{
	//Initialize the view------------------------------------------------------------------

	self = [super initWithFrame:frame];

	if(self==nil)
	{
		messageDialog(NSLocalizedString(@"Error", nil), @"Could not init frame for OpenGL display");
		return nil;
	}

	//Init the screen buffer -------------------------------------------------------------

	screen_buffer = [[ScreenState alloc] init];

	if(screen_buffer == nil)
	{
		messageDialog(NSLocalizedString(@"Error", nil), @"Couldn't initialize screen view data");

		//we dont return in this case because we will hopefully get new screen buffers
		//constantly as emulation is going on, so this just means that there will be no display before emulation starts
	}

	//fill with black
	else [screen_buffer fillWithBlack];

	//DS Related ------------------------------------------------------------------------------

	//currenty we dont actually need access to the DS object
	//DS = ds;
	//[DS retain];

	//Initialize the OpenGL context for displaying the screen -----------------------------------

	//Create the pixel format for our video output view
	NSOpenGLPixelFormatAttribute attrs[] =
	{
		NSOpenGLPFAAccelerated,
		NSOpenGLPFANoRecovery,
		NSOpenGLPFAColorSize, 16,
		NSOpenGLPFAAlphaSize, 0,
		NSOpenGLPFADepthSize, 0,
		NSOpenGLPFAWindow,
		0
	};

	NSOpenGLPixelFormat* pixel_format = [[NSOpenGLPixelFormat alloc] initWithAttributes:attrs];
	if(pixel_format == nil)
	{
		context = nil;
		messageDialog(NSLocalizedString(@"Error", nil), @"Couldn't create OpenGL pixel format for video output");
	} else
	{

		context = [[NSOpenGLContext alloc] initWithFormat:pixel_format shareContext:nil];
		if(context == nil)
		{
			messageDialog(NSLocalizedString(@"Error", nil), @"Couldn't create OpenGL context for video output");
			return self;
		}
	}

	//this will init opengl for drawing
	[self setFrame:frame];

	return self;
}

- (void)dealloc
{
	//[DS release];
	[context release];
	[format release];
	[screen_buffer release];

	[super dealloc];
}

- (void)setRotation:(enum ScreenRotation)rot
{
	//note that we use an optimization -
	//rotation 180 is stored as rotation 0
	//and rotation 270 us stored as rotation 90
	//and using opengl we flip it (which is hopefully faster than rotating it manually)

	if((rot == ROTATION_180) || (rot == ROTATION_0))
		[screen_buffer setRotation:ROTATION_0];
	else if((rot == ROTATION_270) || (rot == ROTATION_90))
		[screen_buffer setRotation:ROTATION_90];
	else return; //invalid rotation value passed to this function

	rotation = rot;
}

- (enum ScreenRotation)rotation
{
	return rotation;
}

- (void)drawRect:(NSRect)bounds
{
	if(screen_buffer == nil)return; //simply dont draw anything if we dont have a screen data object allocated
	if(context == nil)return; //

	[context makeCurrentContext];

	if(rotation == ROTATION_0 || rotation == ROTATION_180)
	{
		//here we send our corrected video buffer off to OpenGL where it gets pretty much
		//directly copied to the frame buffer (and converted to the devices color format)
		glDrawPixels(DS_SCREEN_WIDTH, DS_SCREEN_HEIGHT*2, GL_RGBA, GL_UNSIGNED_SHORT_1_5_5_5_REV, (const GLvoid*)[screen_buffer colorData]);
	} else
	{
		glDrawPixels(DS_SCREEN_HEIGHT*2, DS_SCREEN_WIDTH, GL_RGBA, GL_UNSIGNED_SHORT_1_5_5_5_REV, (const GLvoid*)[screen_buffer colorData]);
	}

	glFlush();
}

- (void)setFrame:(NSRect)rect
{
	if(context == nil)return;

	[super setFrame:rect];

	[context update];

	[context makeCurrentContext];

	//set the viewport (so the raster pos will be correct)
	glViewport(0, 0, rect.size.width, rect.size.height);

	if(rotation == ROTATION_0)
	{

		//the raster pos controls where the bitmap where will be placed
		glRasterPos2f(-1, 1);

		//set the pixel zoom so our bitmap streches to fit our new opengl size
		glPixelZoom(((float)rect.size.width) / ((float)DS_SCREEN_WIDTH), -((float)rect.size.height) / ((float)DS_SCREEN_HEIGHT*2));

	} else if (rotation == ROTATION_90)
	{

		//the raster pos controls where the bitmap where will be placed
		glRasterPos2f(-1, 1);

		//set the pixel zoom so our bitmap streches to fit our new opengl size
		glPixelZoom(((float)rect.size.width) / ((float)DS_SCREEN_HEIGHT*2), -((float)rect.size.height) / ((float)DS_SCREEN_WIDTH));

	} else if (rotation == ROTATION_180)
	{

		//the raster pos controls where the bitmap where will be placed
		glRasterPos2f(1, -1);

		//set the pixel zoom so our bitmap streches to fit our new opengl size
		glPixelZoom(-((float)rect.size.width) / ((float)DS_SCREEN_WIDTH), ((float)rect.size.height) / ((float)DS_SCREEN_HEIGHT*2));

	} else if (rotation == ROTATION_270)
	{
		//the raster pos controls where the bitmap where will be placed
		glRasterPos2f(1, -1);

		//set the pixel zoom so our bitmap streches to fit our new opengl size
		glPixelZoom(-((float)rect.size.width) / ((float)DS_SCREEN_HEIGHT*2), ((float)rect.size.height) / ((float)DS_SCREEN_WIDTH));

	}

}

- (BOOL)isOpaque
{
	if(screen_buffer)
		return YES;

	//if there is no screen buffer, then we can't draw anything
	//so this view is completely transparent
	return NO;
}

- (void)updateScreen:(ScreenState*)screen
{
	if(screen == nil)
	{
		messageDialog(NSLocalizedString(@"Error", nil), @"Recieved invalid screen update");
		return;
	}

	[screen_buffer release]; //get rid of old screen data
	screen_buffer = screen;
	[screen_buffer retain]; //retain the new screendata since we will need it if we have to redraw before we recieve another update

	//if the screen needs to be rotated
	if(rotation == ROTATION_90 || rotation == ROTATION_270)
		[screen_buffer setRotation:ROTATION_90]; //

	//then video output view draws from that buffer
	[self display];
}

- (void)clearScreenWhite
{
	[screen_buffer fillWithWhite];
	[self display];
}

- (void)clearScreenBlack
{
	[screen_buffer fillWithBlack];
	[self display];
}

- (void)viewDidMoveToWindow
{
	//the setView message doesnt work if the view
	//isn't in a window, which is the case in the init func
	//so we use this callback to bind the context to the window

	if([self window] != nil)
		[context setView:self];
	else
		[context clearDrawable];
}

- (const ScreenState*)screenState
{
	ScreenState *result;

	if(rotation == ROTATION_180)
	{
		result = [[ScreenState alloc] initWithScreenState:screen_buffer];
		[result setRotation:ROTATION_180];
	} else if(rotation == ROTATION_270)
	{
		result = [[ScreenState alloc] initWithScreenState:screen_buffer];
		[result setRotation:ROTATION_270];
	} else result = screen_buffer;

	return result;
}

@end

