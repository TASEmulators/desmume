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
#import <OpenGL/gl.h>

//DeSmuME Cocoa includes
#import "globals.h"
#import "main_window.h"
#import "nds_control.h"

//DeSmuME general includes
#define OBJ_C
#include "../MMU.h"
#include "../GPU.h"
#undef BOOL

//
BOOL keep_proportions = YES;

//Title bar size (calculated by VideoOuputWindow init)
//useful for window rect calculations
unsigned short title_bar_height;

//How much padding to put around the video output
#define WINDOW_BORDER_PADDING 5

//data is copied here from the DS screen to flip it
volatile u8 correction_buffer[DS_SCREEN_WIDTH * DS_SCREEN_HEIGHT_COMBINED * 2 /*bits per pixel*/];
volatile u8 temp_buffer[DS_SCREEN_WIDTH * DS_SCREEN_HEIGHT_COMBINED * 2 /*bits per pixel*/];

//this is th opengl view where we will display the video output
@class VideoOutputView;
VideoOutputView *video_output_view;

//fast access to size of openglview (not size of window)
volatile unsigned short x_size = DS_SCREEN_WIDTH;
volatile unsigned short y_size = DS_SCREEN_HEIGHT_COMBINED;

//screen menu items
NSMenuItem *resize1x;
NSMenuItem *resize2x;
NSMenuItem *resize3x;
NSMenuItem *resize4x;
NSMenuItem *allows_resize_item;
NSMenuItem *constrain_item;
NSMenuItem *min_size_item;
NSMenuItem *rotation0_item;
NSMenuItem *rotation90_item;
NSMenuItem *rotation180_item;
NSMenuItem *rotation270_item;
NSMenuItem *topBG0_item;
NSMenuItem *topBG1_item;
NSMenuItem *topBG2_item;
NSMenuItem *topBG3_item;
NSMenuItem *subBG0_item;
NSMenuItem *subBG1_item;
NSMenuItem *subBG2_item;
NSMenuItem *subBG3_item;

//Default key config (based on the windows version)
unsigned short ds_up     = 126; //up arrow
unsigned short ds_down   = 125; //down arrow
unsigned short ds_left   = 123; //left arrow
unsigned short ds_right  = 124; //right arrrow
unsigned short ds_a      = 9  ; //v
unsigned short ds_b      = 11 ; //b
unsigned short ds_x      = 5  ; //g
unsigned short ds_y      = 4  ; //h
unsigned short ds_l      = 8  ; //c
unsigned short ds_r      = 45 ; //n
unsigned short ds_select = 49 ; //space bar
unsigned short ds_start  = 36 ; //enter
unsigned short ds_debug  = 999;

//
unsigned short save_slot_1  = 122; //F1
unsigned short save_slot_2  = 120; //F2
unsigned short save_slot_3  =  99; //F3
unsigned short save_slot_4  = 118; //F4
unsigned short save_slot_5  =  96; //F5
unsigned short save_slot_6  =  97; //F6
unsigned short save_slot_7  =  98; //F7
unsigned short save_slot_8  = 100; //F8
unsigned short save_slot_9  = 101; //F9
unsigned short save_slot_10 = 109; //F10
//unsigned short save_slot_11 = 103; //F11
//unsigned short save_slot_12 = 111; //F12

NSOpenGLContext *gpu_context;

//rotation
#define ROTATION_0   0
#define ROTATION_90  1
#define ROTATION_180 2
#define ROTATION_270 3
u8 rotation = ROTATION_0;

//extern unsigned char GPU_screen3D[256*256*4];

u8 gpu_buff[256 * 256 * 4];
//#define gpu_buff GPU_screen3D

//min sizes (window size, not ds screen size)
NSSize min_size;

//Window delegate class ---------------------------------------------------------

@interface WindowDelegate : NSObject {}
- (void)windowDidResize:(NSNotification *)aNotification;
- (BOOL)windowShouldClose:(id)sender;
- (NSSize)windowWillResize:(NSWindow *)sender toSize:(NSSize)proposedFrameSize;
@end

@interface VideoOutputView : NSView
{
	@private
	NSOpenGLContext* context;
	NSOpenGLPixelFormat* format;
}
- (id)initWithFrame:(NSRect)frame;
- (void)draw;
- (void)drawRect:(NSRect)bounds;
- (void)setFrame:(NSRect)rect;
- (BOOL) isOpaque;
@end

@implementation VideoOutputView

- (id)initWithFrame:(NSRect)frame
{
	//Initialize the view------------------------------------------------------------------

	self = [super initWithFrame:frame];

	if(self==nil)
	{
		messageDialog(@"Error", @"Could not init frame for OpenGL display");
		return nil;
	}

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
		messageDialog(@"Error", @"Couldn't create OpenGL pixel format for video output");
		return self;
	}

	context = [[NSOpenGLContext alloc] initWithFormat:pixel_format shareContext:nil];
	if(context == nil)
	{
		messageDialog(@"Error", @"Couldn't create OpenGL context for video output");
		return self;
	}

	//Initialize the OpenGL context for 3D rendering ---------------------------------------

	//Create the pixel format for our gpu
	NSOpenGLPixelFormatAttribute attrs2[] =
	{
		//NSOpenGLPFAAccelerated,
		//NSOpenGLPFANoRecovery,
		//NSOpenGLPFADoubleBuffer,
		NSOpenGLPFAColorSize, 24,
		NSOpenGLPFAAlphaSize, 8,
		NSOpenGLPFADepthSize, 24,
		//NSOpenGLPFAStencilSize, 8,
		NSOpenGLPFAOffScreen,
		0
	};

	NSOpenGLPixelFormat* pixel_format2 = [[NSOpenGLPixelFormat alloc] initWithAttributes:attrs2];
	if(pixel_format2 == nil)
	{
		messageDialog(@"Error", @"Couldn't create OpenGL pixel format for GPU");
		return self;
	}

	gpu_context = [[NSOpenGLContext alloc] initWithFormat:pixel_format2 shareContext:nil];
	if(gpu_context == nil)
	{
		messageDialog(@"Error", @"Couldn't create OpenGL context for GPU");
		return self;
	}

	//offscreen rendering
	[gpu_context setOffScreen:(void*)&gpu_buff width:DS_SCREEN_WIDTH height:DS_SCREEN_HEIGHT rowbytes:DS_SCREEN_WIDTH * 4];

	//this will init opengl for drawing
	[self setFrame:frame];

	//
	return self;
}

- (void) draw
{
	NSRect blar;
	[self drawRect:blar];
}

- (void)drawRect:(NSRect)bounds
{
	[context setView:self]; //for some reason this doesnt work when called from the init function

	[context makeCurrentContext];

	if(rotation == ROTATION_0 || rotation == ROTATION_180)
	{

		//here we send our corrected video buffer off to OpenGL where it gets pretty much
		//directly copied to the frame buffer (and converted to the devices color format)
		glDrawPixels(DS_SCREEN_WIDTH, DS_SCREEN_HEIGHT_COMBINED, GL_RGBA, GL_UNSIGNED_SHORT_1_5_5_5_REV, (GLvoid*)&correction_buffer);

	} else
	{
		glDrawPixels(DS_SCREEN_HEIGHT_COMBINED, DS_SCREEN_WIDTH, GL_RGBA, GL_UNSIGNED_SHORT_1_5_5_5_REV, (GLvoid*)&correction_buffer);
	}

	glFlush();

	[gpu_context makeCurrentContext];
}

- (void)setFrame:(NSRect)rect
{
	BOOL was_paused = paused;
	[NDS pause];

	[super setFrame:rect];

	[context update];

#ifndef MULTITHREADED
	[context makeCurrentContext];
#endif

	//set the viewport (so the raster pos will be correct)
	glViewport(0, 0, rect.size.width, rect.size.height);

	if(rotation == ROTATION_0)
	{

		//the raster pos controls where the bitmap where will be placed
		glRasterPos2f(-1, 1);

		//set the pixel zoom so our bitmap streches to fit our new opengl size
		glPixelZoom(((float)rect.size.width) / ((float)DS_SCREEN_WIDTH), -((float)rect.size.height) / ((float)DS_SCREEN_HEIGHT_COMBINED));

	} else if (rotation == ROTATION_90)
	{

		//the raster pos controls where the bitmap where will be placed
		glRasterPos2f(1, 1);

		//set the pixel zoom so our bitmap streches to fit our new opengl size
		glPixelZoom(-((float)rect.size.width) / ((float)DS_SCREEN_HEIGHT_COMBINED), -((float)rect.size.height) / ((float)DS_SCREEN_WIDTH));

	} else if (rotation == ROTATION_180)
	{

		//set the viewport (so the raster pos will be correct)
		glViewport(0, 0, rect.size.width, rect.size.height);

		//the raster pos controls where the bitmap where will be placed
		glRasterPos2f(1, -1);

		//set the pixel zoom so our bitmap streches to fit our new opengl size
		glPixelZoom(-((float)rect.size.width) / ((float)DS_SCREEN_WIDTH), ((float)rect.size.height) / ((float)DS_SCREEN_HEIGHT_COMBINED));

	} else if (rotation == ROTATION_270)
	{
		//the raster pos controls where the bitmap where will be placed
		glRasterPos2f(-1, -1);

		//set the pixel zoom so our bitmap streches to fit our new opengl size
		glPixelZoom(((float)rect.size.width) / ((float)DS_SCREEN_HEIGHT_COMBINED), ((float)rect.size.height) / ((float)DS_SCREEN_WIDTH));

	}

#ifndef MULTITHREADED
	[gpu_context makeCurrentContext];
#endif

	if(!was_paused)[NDS execute];
}

- (BOOL)isOpaque
{
	return YES;
}
@end

///////////////////////////////////////////////////////////////////////////////////////
//Video output window class ----------------------------------------------------------
///////////////////////////////////////////////////////////////////////////////////////

@implementation VideoOutputWindow

- (id)init
{

	//
	NSRect rect;

	//Create the window rect (content rect - title bar not included in size)
	rect.size.width = DS_SCREEN_WIDTH + WINDOW_BORDER_PADDING * 2;
	rect.size.height = DS_SCREEN_HEIGHT_COMBINED + WINDOW_BORDER_PADDING;
	rect.origin.x = 200;
	rect.origin.y = 200;

	//allocate the window
	self = [super initWithContentRect:rect styleMask:

	NSTitledWindowMask|NSResizableWindowMask|/*NSClosableWindowMask|*/NSMiniaturizableWindowMask|NSTexturedBackgroundWindowMask

	backing:NSBackingStoreBuffered defer:NO screen:nil];

	[self setTitle:@"DeSmuME Emulator"];

	//set minimum window size (to the starting size, pixel-for-pixel to DS)
	//this re-gets the window rect to include the title bar size
	NSSize tempsize;
	tempsize.width = 1;
	tempsize.height = 1;
	[self setMinSize:tempsize];
	min_size = rect.size;

	NSSize temp;
	temp.width = WINDOW_BORDER_PADDING * 2 + 2;
	temp.height = WINDOW_BORDER_PADDING + 2;
	[self setContentMinSize:temp];

	//set title_bar_height
	title_bar_height = [super frame].size.height - rect.size.height;

	//Set the window delegate
	[self setDelegate:[[WindowDelegate alloc] init]];

	//Create the image view where we will display video output
	rect.origin.x = WINDOW_BORDER_PADDING;
	rect.origin.y = WINDOW_BORDER_PADDING;
	rect.size.width = DS_SCREEN_WIDTH;
	rect.size.height = DS_SCREEN_HEIGHT_COMBINED;
	if((video_output_view = [[VideoOutputView alloc] initWithFrame:rect]) == nil)
		messageDialog(@"Error", @"Couldn't create OpenGL view to display screens");
	else
	{
		[[self contentView] addSubview:video_output_view];

	}

	//Show the window
	[[[NSWindowController alloc] initWithWindow:self] showWindow:nil];

	return self;
}

- (void)updateScreen
{
	//here we copy gpu_screen to our own buffer

	if(rotation == ROTATION_0 || rotation == ROTATION_180)
		memcpy(&correction_buffer, &GPU_screen, DS_SCREEN_WIDTH * DS_SCREEN_HEIGHT_COMBINED * 2);
	else
	{
		//here is where we turn the screen sideways

		int x, y;
		for(x = 0; x< DS_SCREEN_WIDTH; x++)
			for(y = 0; y < DS_SCREEN_HEIGHT_COMBINED; y++)
			{
				correction_buffer[(x * DS_SCREEN_HEIGHT_COMBINED + y) * 2] = GPU_screen[(y * DS_SCREEN_WIDTH + x) * 2];
				correction_buffer[(x * DS_SCREEN_HEIGHT_COMBINED + y) * 2 + 1] = GPU_screen[(y * DS_SCREEN_WIDTH + x) * 2 + 1];
			}

	}

	//then video output view draws from that buffer
	[video_output_view draw];
}

- (void)clearScreen
{
	//fill our buffer with pure white
	memset(&correction_buffer, 255, DS_SCREEN_WIDTH * DS_SCREEN_HEIGHT_COMBINED * 2);

	[video_output_view draw];
}

- (void)resetMinSize:(bool)resize
{
	if(rotation == ROTATION_0 || rotation == ROTATION_180)
	{
		min_size.width = DS_SCREEN_WIDTH + WINDOW_BORDER_PADDING * 2;
		min_size.height = DS_SCREEN_HEIGHT_COMBINED + WINDOW_BORDER_PADDING + title_bar_height;

		//resize if too small
		if(resize)
		{
			NSSize temp = [video_output_view frame].size;
			if(temp.width < DS_SCREEN_WIDTH || temp.height < DS_SCREEN_HEIGHT_COMBINED)
				[self resizeScreen:NSMakeSize(DS_SCREEN_WIDTH, DS_SCREEN_HEIGHT_COMBINED)]; //this could be per axis?
		}

	} else
	{
		min_size.width = DS_SCREEN_HEIGHT_COMBINED + WINDOW_BORDER_PADDING * 2;
		min_size.height = DS_SCREEN_WIDTH + WINDOW_BORDER_PADDING + title_bar_height;

		//resize if too small
		if(resize)
		{
			NSSize temp = [video_output_view frame].size;
			if(temp.width < DS_SCREEN_HEIGHT_COMBINED || temp.height < DS_SCREEN_WIDTH)
				[self resizeScreen:NSMakeSize(DS_SCREEN_HEIGHT_COMBINED, DS_SCREEN_WIDTH)];
		}
	}
}

- (void)toggleMinSize
{
	static bool state = false;

	if([min_size_item state] == NSOnState)
	{
		[min_size_item setState:NSOffState];
		min_size.width = 0;
		min_size.height = 0;
	} else
	{
		[min_size_item setState:NSOnState];
		[self resetMinSize:true];
	}
}

- (void)resizeScreen:(NSSize)size
{
	//if(x_size == size.width && y_size == size.height)return;

	BOOL was_paused = paused;
	[NDS pause];

	//add the window borders
	size.width += WINDOW_BORDER_PADDING * 2;
	size.height += WINDOW_BORDER_PADDING + title_bar_height;

	//constrain
	size = [[self delegate] windowWillResize:self toSize:size];

	//set the position (keeping the center of the window the same)
	NSRect current_window_rect = [super frame];
	current_window_rect.origin.x -= ((size.width + WINDOW_BORDER_PADDING * 2) - current_window_rect.size.width) / 2;
	current_window_rect.origin.y -= ((size.height + WINDOW_BORDER_PADDING + title_bar_height) - current_window_rect.size.height) / 2;

	//and change the size
	current_window_rect.size.width = size.width;
	current_window_rect.size.height = size.height;

	//send it off to cocoa
	[self setFrame:current_window_rect display:YES];

	if(!was_paused)[NDS execute];
}

- (void)resizeScreen1x
{

	if(rotation == ROTATION_0 || rotation == ROTATION_180)
		[self resizeScreen:NSMakeSize(DS_SCREEN_WIDTH, DS_SCREEN_HEIGHT_COMBINED)];
	else
		[self resizeScreen:NSMakeSize(DS_SCREEN_HEIGHT_COMBINED, DS_SCREEN_WIDTH)];
}

- (void)resizeScreen2x
{

	if(rotation == ROTATION_0 || rotation == ROTATION_180)
		[self resizeScreen:NSMakeSize(DS_SCREEN_WIDTH * 2, DS_SCREEN_HEIGHT_COMBINED * 2)];
	else
		[self resizeScreen:NSMakeSize(DS_SCREEN_HEIGHT_COMBINED * 2, DS_SCREEN_WIDTH * 2)];
}

- (void)resizeScreen3x
{
	if(rotation == ROTATION_0 || rotation == ROTATION_180)
		[self resizeScreen:NSMakeSize(DS_SCREEN_WIDTH * 3, DS_SCREEN_HEIGHT_COMBINED * 3)];
	else
		[self resizeScreen:NSMakeSize(DS_SCREEN_HEIGHT_COMBINED * 3, DS_SCREEN_WIDTH * 3)];
}

- (void)resizeScreen4x
{
	if(rotation == ROTATION_0 || rotation == ROTATION_180)
		[self resizeScreen:NSMakeSize(DS_SCREEN_WIDTH * 4, DS_SCREEN_HEIGHT_COMBINED * 4)];
	else
		[self resizeScreen:NSMakeSize(DS_SCREEN_HEIGHT_COMBINED * 4, DS_SCREEN_WIDTH * 4)];
}

- (void)toggleAllowsResize
{
	//get the resize control state
	BOOL resizes = [self showsResizeIndicator];

	if(resizes == YES)
	{
		//
		resizes = NO;

		//don't allow resize
		//hack!!! fixme
		NSSize temp;
		temp.width = 9000;
		temp.height = 9000;
		[self setResizeIncrements:temp];

		//don't show resize control
		[self setShowsResizeIndicator:NO];

		//unset the check
		[allows_resize_item setState:NSOffState];

	} else{
		resizes = YES;

		//allow resize
		NSSize temp;
		temp.width = 1;
		temp.height = 1;
		[self setResizeIncrements:temp];

		//show resize control
		[self setShowsResizeIndicator:YES];

		//set the check
		[allows_resize_item setState:NSOnState];
	}
}

- (void)toggleConstrainProportions;
{
	//change the state
	keep_proportions = !keep_proportions;

	//constrain
	NSSize size;
	size.width = x_size;
	size.height = y_size;
	if(keep_proportions)
		[self resizeScreen:size];

	//set the menu checks
	if(keep_proportions)
		[constrain_item setState:NSOnState];
	else
		[constrain_item setState:NSOffState];
}

- (void)setRotation0
{
	if(rotation == ROTATION_0)return;

	if(rotation == ROTATION_180)
	{
		rotation = ROTATION_0;

		[video_output_view setFrame:[video_output_view frame]];
		[self updateScreen];

	} else
	{
		//set the global rotation state
		rotation = ROTATION_0;

		//fix the buffer
		memcpy(&temp_buffer, &correction_buffer, DS_SCREEN_WIDTH * DS_SCREEN_HEIGHT_COMBINED * 2);
		int x, y;
		for(x = 0; x< DS_SCREEN_WIDTH; x++)
			for(y = 0; y < DS_SCREEN_HEIGHT_COMBINED; y++)
			{
				correction_buffer[(y * DS_SCREEN_WIDTH + x) * 2] = temp_buffer[(x * DS_SCREEN_HEIGHT_COMBINED + y) * 2];
				correction_buffer[(y * DS_SCREEN_WIDTH + x) * 2 + 1] = temp_buffer[(x * DS_SCREEN_HEIGHT_COMBINED + y) * 2 + 1];
			}

		//fix the min size
		if([min_size_item state] == NSOnState)
			[self resetMinSize:false];

		//resize the window/view
		NSSize video_size = [video_output_view frame].size;
		float temp = video_size.width;
		video_size.width = video_size.height;
		video_size.height = temp;
		[self resizeScreen:video_size];
	}

	//set the checkmarks
	[rotation0_item   setState:NSOnState ];
	[rotation90_item  setState:NSOffState];
	[rotation180_item setState:NSOffState];
	[rotation270_item setState:NSOffState];
}

- (void)setRotation90
{
	if(rotation == ROTATION_90)return;

	if(rotation == ROTATION_270)
	{

		rotation = ROTATION_90;

		[video_output_view setFrame:[video_output_view frame]];
		[self updateScreen];

	} else
	{
		//set the global rotation state
		rotation = ROTATION_90;

		//fix the buffer
		memcpy(&temp_buffer, &correction_buffer, DS_SCREEN_WIDTH * DS_SCREEN_HEIGHT_COMBINED * 2);
		int x, y;
		for(x = 0; x< DS_SCREEN_WIDTH; x++)
			for(y = 0; y < DS_SCREEN_HEIGHT_COMBINED; y++)
			{
				correction_buffer[(x * DS_SCREEN_HEIGHT_COMBINED + y) * 2] = temp_buffer[(y * DS_SCREEN_WIDTH + x) * 2];
				correction_buffer[(x * DS_SCREEN_HEIGHT_COMBINED + y) * 2 + 1] = temp_buffer[(y * DS_SCREEN_WIDTH + x) * 2 + 1];
			}

		//fix min size
		if([min_size_item state] == NSOnState)
			[self resetMinSize:false];

		//resize the window/screen
		NSSize video_size = [video_output_view frame].size;
		float temp = video_size.width;
		video_size.width = video_size.height;
		video_size.height = temp;
		[self resizeScreen:video_size];
	}

	//set the checkmarks
	[rotation0_item   setState:NSOffState];
	[rotation90_item  setState:NSOnState ];
	[rotation180_item setState:NSOffState];
	[rotation270_item setState:NSOffState];
}

- (void)setRotation180
{
	if(rotation == ROTATION_180)return;

	if(rotation == ROTATION_0)
	{
		rotation = ROTATION_180;

		[video_output_view setFrame:[video_output_view frame]];
		[self updateScreen];

	} else
	{
		//set the global rotation state
		rotation = ROTATION_180;

		//fix the buffer
		memcpy(&temp_buffer, &correction_buffer, DS_SCREEN_WIDTH * DS_SCREEN_HEIGHT_COMBINED * 2);
		int x, y;
		for(x = 0; x< DS_SCREEN_WIDTH; x++)
			for(y = 0; y < DS_SCREEN_HEIGHT_COMBINED; y++)
			{
				correction_buffer[(y * DS_SCREEN_WIDTH + x) * 2] = temp_buffer[(x * DS_SCREEN_HEIGHT_COMBINED + y) * 2];
				correction_buffer[(y * DS_SCREEN_WIDTH + x) * 2 + 1] = temp_buffer[(x * DS_SCREEN_HEIGHT_COMBINED + y) * 2 + 1];
			}

		//fix the min size
		if([min_size_item state] == NSOnState)
			[self resetMinSize:false];

		//resize the window/view
		NSSize video_size = [video_output_view frame].size;
		float temp = video_size.width;
		video_size.width = video_size.height;
		video_size.height = temp;
		[self resizeScreen:video_size];

	}

	//set the checkmarks
	[rotation0_item   setState:NSOffState];
	[rotation90_item  setState:NSOffState];
	[rotation180_item setState:NSOnState ];
	[rotation270_item setState:NSOffState];
}

- (void)setRotation270
{
	if(rotation == ROTATION_270)return;

	if(rotation == ROTATION_90)
	{
		rotation = ROTATION_270;

		[video_output_view setFrame:[video_output_view frame]];
		[self updateScreen];

	} else
	{

		//set the global rotation state
		rotation = ROTATION_270;

		//fix the buffer
		memcpy(&temp_buffer, &correction_buffer, DS_SCREEN_WIDTH * DS_SCREEN_HEIGHT_COMBINED * 2);
		int x, y;
		for(x = 0; x< DS_SCREEN_WIDTH; x++)
			for(y = 0; y < DS_SCREEN_HEIGHT_COMBINED; y++)
			{
				correction_buffer[(x * DS_SCREEN_HEIGHT_COMBINED + y) * 2] = temp_buffer[(y * DS_SCREEN_WIDTH + x) * 2];
				correction_buffer[(x * DS_SCREEN_HEIGHT_COMBINED + y) * 2 + 1] = temp_buffer[(y * DS_SCREEN_WIDTH + x) * 2 + 1];
			}

		//fix the min size
		if([min_size_item state] == NSOnState)
			[self resetMinSize:false];

		//resize the window/screen
		NSSize video_size = [video_output_view frame].size;
		float temp = video_size.width;
		video_size.width = video_size.height;
		video_size.height = temp;
		[self resizeScreen:video_size];
	}

	//set the checkmarks
	[rotation0_item   setState:NSOffState];
	[rotation90_item  setState:NSOffState];
	[rotation180_item setState:NSOffState];
	[rotation270_item setState:NSOnState];
}

- (void)toggleTopBackground0
{
	if(SubScreen.gpu->dispBG[0])
	{
		GPU_remove(SubScreen.gpu, 0);
		[topBG0_item setState:NSOffState];
	}
	else
	{
		GPU_addBack(SubScreen.gpu, 0);
		[topBG0_item setState:NSOnState];
	}
}

- (void)toggleTopBackground1
{
	if(SubScreen.gpu->dispBG[1])
	{
		GPU_remove(SubScreen.gpu, 1);
		[topBG1_item setState:NSOffState];
	}
	else
	{
		GPU_addBack(SubScreen.gpu, 1);
		[topBG1_item setState:NSOnState];
	}
}

- (void)toggleTopBackground2
{
	if(SubScreen.gpu->dispBG[2])
	{
		GPU_remove(SubScreen.gpu, 2);
		[topBG2_item setState:NSOffState];
	}
	else
	{
		GPU_addBack(SubScreen.gpu, 2);
		[topBG2_item setState:NSOnState];
	}
}

- (void)toggleTopBackground3
{
	if(SubScreen.gpu->dispBG[3])
	{
		GPU_remove(SubScreen.gpu, 3);
		[topBG3_item setState:NSOffState];
	}
	else
	{
		GPU_addBack(SubScreen.gpu, 3);
		[topBG3_item setState:NSOnState];
	}
}

- (void)toggleSubBackground0
{
	if(MainScreen.gpu->dispBG[0])
	{
		GPU_remove(MainScreen.gpu, 0);
		[subBG0_item setState:NSOffState];
	}
	else
	{
		GPU_addBack(MainScreen.gpu, 0);
		[subBG0_item setState:NSOnState];
	}
}

- (void)toggleSubBackground1
{
	if(MainScreen.gpu->dispBG[1])
	{
		GPU_remove(MainScreen.gpu, 1);
		[subBG1_item setState:NSOffState];
	}
	else
	{
		GPU_addBack(MainScreen.gpu, 1);
		[subBG1_item setState:NSOnState];
	}
}

- (void)toggleSubBackground2
{
	if(MainScreen.gpu->dispBG[2])
	{
		GPU_remove(MainScreen.gpu, 2);
		[subBG2_item setState:NSOffState];
	}
	else
	{
		GPU_addBack(MainScreen.gpu, 2);
		[subBG2_item setState:NSOnState];
	}
}

- (void)toggleSubBackground3
{
	if(MainScreen.gpu->dispBG[3])
	{
		GPU_remove(MainScreen.gpu, 3);
		[subBG3_item setState:NSOffState];
	}
	else
	{
		GPU_addBack(MainScreen.gpu, 3);
		[subBG3_item setState:NSOnState];
	}
}

- (void)keyDown:(NSEvent*)event
{
	if([event isARepeat])return;

	unsigned short keycode = [event keyCode];

	if(keycode == ds_a)
	{
		((u16 *)ARM9Mem.ARM9_REG)[0x130>>1] &= 0xFFFE;
		((u16 *)MMU.ARM7_REG)[0x130>>1] &= 0xFFFE;
	}

	else if(keycode == ds_b)
	{
		((u16 *)ARM9Mem.ARM9_REG)[0x130>>1] &= 0xFFFD;
		((u16 *)MMU.ARM7_REG)[0x130>>1] &= 0xFFFD;
	}

	else if(keycode == ds_select)
	{
		((u16 *)ARM9Mem.ARM9_REG)[0x130>>1] &= 0xFFFB;
		((u16 *)MMU.ARM7_REG)[0x130>>1] &= 0xFFFB;
	}

	else if(keycode == ds_start)
	{
		((u16 *)ARM9Mem.ARM9_REG)[0x130>>1] &= 0xFFF7;
		((u16 *)MMU.ARM7_REG)[0x130>>1] &= 0xFFF7;
	}

	else if(keycode == ds_right)
	{
		((u16 *)ARM9Mem.ARM9_REG)[0x130>>1] &= 0xFFEF;
		((u16 *)MMU.ARM7_REG)[0x130>>1] &= 0xFFEF;
	}

	else if(keycode == ds_left)
	{
		((u16 *)ARM9Mem.ARM9_REG)[0x130>>1] &= 0xFFDF;
		((u16 *)MMU.ARM7_REG)[0x130>>1] &= 0xFFDF;
	}

	else if(keycode == ds_up)
	{
		((u16 *)ARM9Mem.ARM9_REG)[0x130>>1] &= 0xFFBF;
		((u16 *)MMU.ARM7_REG)[0x130>>1] &= 0xFFBF;
	}

	else if(keycode == ds_down)
	{
		((u16 *)ARM9Mem.ARM9_REG)[0x130>>1] &= 0xFF7F;
		((u16 *)MMU.ARM7_REG)[0x130>>1] &= 0xFF7F;
	}

	else if(keycode == ds_r)
	{
		((u16 *)ARM9Mem.ARM9_REG)[0x130>>1] &= 0xFEFF;
		((u16 *)MMU.ARM7_REG)[0x130>>1] &= 0xFEFF;
	}

	else if(keycode == ds_l)
	{
		((u16 *)ARM9Mem.ARM9_REG)[0x130>>1] &= 0xFDFF;
		((u16 *)MMU.ARM7_REG)[0x130>>1] &= 0xFDFF;
	}

	else if(keycode == ds_x)
	{
		((u16 *)MMU.ARM7_REG)[0x136>>1] &= 0xFFFE;
	}

	else if(keycode == ds_y)
	{
		((u16 *)MMU.ARM7_REG)[0x136>>1] &= 0xFFFD;
	}

	else if(keycode == ds_debug)
	{
		((u16 *)MMU.ARM7_REG)[0x136>>1] &= 0xFFFB;
	}
}

- (void)keyUp:(NSEvent*)event
{
	unsigned short keycode = [event keyCode];

	if(keycode == ds_a)
	{
		((u16 *)ARM9Mem.ARM9_REG)[0x130>>1] |= 0x1;
		((u16 *)MMU.ARM7_REG)[0x130>>1] |= 0x1;
	}

	else if(keycode == ds_b)
	{
		((u16 *)ARM9Mem.ARM9_REG)[0x130>>1] |= 0x2;
		((u16 *)MMU.ARM7_REG)[0x130>>1] |= 0x2;
	}

	else if(keycode == ds_select)
	{
		((u16 *)ARM9Mem.ARM9_REG)[0x130>>1] |= 0x4;
		((u16 *)MMU.ARM7_REG)[0x130>>1] |= 0x4;
	}

	else if(keycode == ds_start)
	{
		((u16 *)ARM9Mem.ARM9_REG)[0x130>>1] |= 0x8;
		((u16 *)MMU.ARM7_REG)[0x130>>1] |= 0x8;
	}

	else if(keycode == ds_right)
	{
		((u16 *)ARM9Mem.ARM9_REG)[0x130>>1] |= 0x10;
		((u16 *)MMU.ARM7_REG)[0x130>>1] |= 0x10;
	}

	if(keycode == ds_left)
	{
		((u16 *)ARM9Mem.ARM9_REG)[0x130>>1] |= 0x20;
		((u16 *)MMU.ARM7_REG)[0x130>>1] |= 0x20;
	}

	else if(keycode == ds_up)
	{
		((u16 *)ARM9Mem.ARM9_REG)[0x130>>1] |= 0x40;
		((u16 *)MMU.ARM7_REG)[0x130>>1] |= 0x40;
	}

	else if(keycode == ds_down)
	{
		((u16 *)ARM9Mem.ARM9_REG)[0x130>>1] |= 0x80;
		((u16 *)MMU.ARM7_REG)[0x130>>1] |= 0x80;
	}

	else if(keycode == ds_r)
	{
		((u16 *)ARM9Mem.ARM9_REG)[0x130>>1] |= 0x100;
		((u16 *)MMU.ARM7_REG)[0x130>>1] |= 0x100;
	}

	else if(keycode == ds_l)
	{
		((u16 *)ARM9Mem.ARM9_REG)[0x130>>1] |= 0x200;
		((u16 *)MMU.ARM7_REG)[0x130>>1] |= 0x200;
	}

	else if(keycode == ds_x)
	{
		((u16 *)MMU.ARM7_REG)[0x136>>1] |= 0x1;
	}

	else if(keycode == ds_y)
	{
		((u16 *)MMU.ARM7_REG)[0x136>>1] |= 0x2;
	}

	else if(keycode == ds_debug)
	{
		((u16 *)MMU.ARM7_REG)[0x136>>1] |= 0x4;
	}

	else if(keycode == save_slot_1)
	{
		bool was_paused = paused;
		[NDS pause];

		if([event modifierFlags] & NSShiftKeyMask)
			savestate_slot(1);
		else
			loadstate_slot(1);

		if(!was_paused)[NDS execute];
	}

	else if(keycode == save_slot_2)
	{
		bool was_paused = paused;
		[NDS pause];

		if([event modifierFlags] & NSShiftKeyMask)
			savestate_slot(2);
		else
			loadstate_slot(2);

		if(!was_paused)[NDS execute];
	}

	else if(keycode == save_slot_3)
	{
		bool was_paused = paused;
		[NDS pause];

		if([event modifierFlags] & NSShiftKeyMask)
			savestate_slot(3);
		else
			loadstate_slot(3);

		if(!was_paused)[NDS execute];
	}

	else if(keycode == save_slot_4)
	{
		bool was_paused = paused;
		[NDS pause];

		if([event modifierFlags] & NSShiftKeyMask)
			savestate_slot(4);
		else
			loadstate_slot(4);

		if(!was_paused)[NDS execute];
	}

	else if(keycode == save_slot_5)
	{
		bool was_paused = paused;
		[NDS pause];

		if([event modifierFlags] & NSShiftKeyMask)
			savestate_slot(5);
		else
			loadstate_slot(5);

		if(!was_paused)[NDS execute];
	}

	else if(keycode == save_slot_6)
	{
		bool was_paused = paused;
		[NDS pause];

		if([event modifierFlags] & NSShiftKeyMask)
			savestate_slot(6);
		else
			loadstate_slot(6);

		if(!was_paused)[NDS execute];
	}

	else if(keycode == save_slot_7)
	{
		bool was_paused = paused;
		[NDS pause];

		if([event modifierFlags] & NSShiftKeyMask)
			savestate_slot(7);
		else
			loadstate_slot(7);

		if(!was_paused)[NDS execute];
	}

	else if(keycode == save_slot_8)
	{
		bool was_paused = paused;
		[NDS pause];

		if([event modifierFlags] & NSShiftKeyMask)
			savestate_slot(8);
		else
			loadstate_slot(8);

		if(!was_paused)[NDS execute];
	}

	else if(keycode == save_slot_9)
	{
		bool was_paused = paused;
		[NDS pause];

		if([event modifierFlags] & NSShiftKeyMask)
			savestate_slot(9);
		else
			loadstate_slot(9);

		if(!was_paused)[NDS execute];
	}

	else if(keycode == save_slot_10)
	{
		bool was_paused = paused;
		[NDS pause];

		if([event modifierFlags] & NSShiftKeyMask)
			savestate_slot(10);
		else
			loadstate_slot(10);

		if(!was_paused)[NDS execute];
	}

}

- (void)mouseDown:(NSEvent*)event
{
	NSPoint location = [event locationInWindow];

	//If the click was to the left or under the opengl view, exit
	if((location.x < WINDOW_BORDER_PADDING) || (location.y < WINDOW_BORDER_PADDING))
		return;
	location.x -= WINDOW_BORDER_PADDING;
	location.y -= WINDOW_BORDER_PADDING;

	//If the click was top or right of the view...
	if(location.x >= x_size)return;
	if(location.y >= y_size)return;

	if(rotation == ROTATION_0 || rotation == ROTATION_180)
	{
		if(rotation == ROTATION_180)
		{
			if(location.y < y_size / 2)return;
			location.x = x_size - location.x;
			location.y = y_size - location.y;
		} else
			if(location.y >= y_size / 2)return;

		//scale the coordinates
		location.x *= ((float)DS_SCREEN_WIDTH) / ((float)x_size);
		location.y *= ((float)DS_SCREEN_HEIGHT_COMBINED) / ((float)y_size);

		//invert the y
		location.y = DS_SCREEN_HEIGHT - location.y - 1;

	} else
	{

		if(rotation == ROTATION_270)
		{
			if(location.x < x_size / 2)return;
			location.x = x_size - location.x;

		} else
		{
			if(location.x >= x_size / 2)return;
			location.y = y_size - location.y;
		}

		location.x *= ((float)DS_SCREEN_HEIGHT_COMBINED) / ((float)x_size);
		location.y *= ((float)DS_SCREEN_WIDTH) / ((float)y_size);

		//invert the y
		location.x = DS_SCREEN_HEIGHT - location.x - 1;

		float temp = location.x;
		location.x = location.y;
		location.y = temp;
	}
//[self setTitle:[NSString localizedStringWithFormat:@"%u %u", (unsigned int)location.x, (unsigned int)location.y]];

	NDS_setTouchPos((unsigned short)location.x, (unsigned short)location.y);
}

- (void)mouseDragged:(NSEvent*)event
{
	[self mouseDown:event];
}

- (void)mouseUp:(NSEvent*)event
{
//	[self setTitle:@""];

	NDS_releasTouch();
}
@end

//Window Delegate -----------------------------------------------------------------------

@implementation WindowDelegate
- (BOOL)windowShouldClose:(id)sender
{
	[sender hide];
	return FALSE;
}

- (void)windowDidResize:(NSNotification *)aNotification;
{
	BOOL was_paused = paused; //we need to pause here incase the other thread wants to draw to the view while its resizing
	[NDS pause];

	//this is called after the window frame resizes
	//so we must recalculate the video output view

	NSRect rect;
	rect.size.width = x_size;
	rect.size.height = y_size;
	rect.origin.x = WINDOW_BORDER_PADDING;
	rect.origin.y = WINDOW_BORDER_PADDING;
	[video_output_view setFrame:rect];

	if(!was_paused)[NDS execute];
}

- (NSSize)windowWillResize:(NSWindow *)sender toSize:(NSSize)proposedFrameSize
{
	//constrain size
	if(proposedFrameSize.width < min_size.width)proposedFrameSize.width = min_size.width;
	if(proposedFrameSize.height < min_size.height)proposedFrameSize.height = min_size.height;

	//constrain proportions
	if(keep_proportions)
	{
		//this is a simple algorithm to constrain to the smallest axis
		unsigned short x_constrained;
		unsigned short y_constrained;

		if(rotation == ROTATION_0 || rotation == ROTATION_180)
		{
			//this is a simple algorithm to constrain to the smallest axis

			x_constrained = ((float)(proposedFrameSize.height - WINDOW_BORDER_PADDING - title_bar_height)) * DS_SCREEN_X_RATIO;
			x_constrained += WINDOW_BORDER_PADDING * 2;

			y_constrained = ((float)(proposedFrameSize.width - WINDOW_BORDER_PADDING * 2)) * DS_SCREEN_Y_RATIO;
			y_constrained += WINDOW_BORDER_PADDING + title_bar_height;

		} else
		{

			x_constrained = ((float)(proposedFrameSize.height - WINDOW_BORDER_PADDING - title_bar_height)) * DS_SCREEN_Y_RATIO;
			x_constrained += WINDOW_BORDER_PADDING * 2;

			y_constrained = ((float)(proposedFrameSize.width - WINDOW_BORDER_PADDING * 2)) * DS_SCREEN_X_RATIO;
			y_constrained += WINDOW_BORDER_PADDING + title_bar_height;

		}

		if(x_constrained < proposedFrameSize.width)proposedFrameSize.width = x_constrained;
		if(y_constrained < proposedFrameSize.height)proposedFrameSize.height = y_constrained;
	}

	//set global size variables
	x_size = proposedFrameSize.width - WINDOW_BORDER_PADDING * 2;
	y_size = proposedFrameSize.height - WINDOW_BORDER_PADDING - title_bar_height;

	//set the menu items
	if((x_size == DS_SCREEN_WIDTH) && (y_size == DS_SCREEN_HEIGHT_COMBINED))
	{
		[resize1x setState:NSOnState ];
		[resize2x setState:NSOffState];
		[resize3x setState:NSOffState];
		[resize4x setState:NSOffState];
	} else if((x_size == DS_SCREEN_WIDTH * 2) && (y_size == DS_SCREEN_HEIGHT_COMBINED * 2))
	{
		[resize1x setState:NSOffState];
		[resize2x setState:NSOnState ];
		[resize3x setState:NSOffState];
		[resize4x setState:NSOffState];
	} else if((x_size == DS_SCREEN_WIDTH * 3) && (y_size == DS_SCREEN_HEIGHT_COMBINED * 3))
	{
		[resize1x setState:NSOffState];
		[resize2x setState:NSOffState];
		[resize3x setState:NSOnState ];
		[resize4x setState:NSOffState];
	} else if((x_size == DS_SCREEN_WIDTH * 4) && (y_size == DS_SCREEN_HEIGHT_COMBINED * 4))
	{
		[resize1x setState:NSOffState];
		[resize2x setState:NSOffState];
		[resize3x setState:NSOffState];
		[resize4x setState:NSOnState ];
	} else
	{
		[resize1x setState:NSOffState];
		[resize2x setState:NSOffState];
		[resize3x setState:NSOffState];
		[resize4x setState:NSOffState];
	}

	//return the [potentially modified] frame size to cocoa to actually resize the window
	return proposedFrameSize;
}

@end
