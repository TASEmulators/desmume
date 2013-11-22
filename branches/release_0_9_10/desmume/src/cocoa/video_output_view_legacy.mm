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

#import "video_output_view_legacy.h"
#import "cocoa_globals.h"
#import "cocoa_util.h"
#import "nds_control_legacy.h"
#import "input_legacy.h"
#import "cocoa_input_legacy.h"
#import "preferences_legacy.h"
#import "screen_state_legacy.h"
#include "../GPU.h"
#include "../NDSSystem.h"

#undef BOOL

#define HORIZONTAL(angle) ((angle) == -90 || (angle) == -270)
#define VERTICAL(angle) ((angle) == 0 || (angle) == -180)

#ifdef HAVE_OPENGL
#import <OpenGL/gl.h>
#endif

#ifdef HAVE_OPENGL
//screenstate extended to hold rotated copies
@interface ScreenState(extended)
- (void)rotateTo90;
- (void)rotateTo0;
@end
#endif

@implementation VideoOutputView

- (id)initWithFrame:(NSRect)frame
{
	//Initialize the view------------------------------------------------------------------

	self = [super initWithFrame:frame];

	if(self==nil)
	{
		[CocoaDSUtil quickDialogUsingTitle:NSLocalizedString(@"Error", nil) message:NSLocalizedString(@"Couldn't create a view for video output", nil)];
		return nil;
	}

	screen_buffer = nil;
	viewScale = 1.0;
	viewRotation = 0.0;
	displayMode = DS_DISPLAY_TYPE_COMBO;
	gpuStateFlags =	GPUSTATE_MAIN_GPU_MASK |
	GPUSTATE_MAIN_BG0_MASK |
	GPUSTATE_MAIN_BG1_MASK |
	GPUSTATE_MAIN_BG2_MASK |
	GPUSTATE_MAIN_BG3_MASK |
	GPUSTATE_MAIN_OBJ_MASK |
	GPUSTATE_SUB_GPU_MASK |
	GPUSTATE_SUB_BG0_MASK |
	GPUSTATE_SUB_BG1_MASK |
	GPUSTATE_SUB_BG2_MASK |
	GPUSTATE_SUB_BG3_MASK |
	GPUSTATE_SUB_OBJ_MASK;

	//Initialize image view if for displaying the screen ----------------------------------------
#ifndef HAVE_OPENGL
	[self setImageFrameStyle: NSImageFrameNone];
	[self setImageScaling:NSScaleToFit];
	[self setEditable:NO];
	[self setEnabled:NO];

	//Initialize the OpenGL context for displaying the screen -----------------------------------
#else
	//Create the pixel format for our video output view
	NSOpenGLPixelFormatAttribute attrs[] =
	{
		//NSOpenGLPFAFullScreen,
		NSOpenGLPFAWindow, //need a renderer that can draw to a window
		//NSOpenGLPFARendererID, some_number, //this picks a particular renderer, for testing
		(NSOpenGLPixelFormatAttribute)0
	};

	NSOpenGLPixelFormat* pixel_format = [[NSOpenGLPixelFormat alloc] initWithAttributes:attrs];
	if(pixel_format == nil)
	{
		[CocoaDSUtil quickDialogUsingTitle:NSLocalizedString(@"Error", nil) message:NSLocalizedString(@"Couldn't create OpenGL pixel format for video output", nil)];
		context = nil;
		[self release];
		return nil;
	} else
	{

		context = [[NSOpenGLContext alloc] initWithFormat:pixel_format shareContext:nil];
		[pixel_format release];
		if(context == nil)
		{
			[CocoaDSUtil quickDialogUsingTitle:NSLocalizedString(@"Error", nil) message:NSLocalizedString(@"Couldn't create OpenGL context for video output", nil)];
			[self release];
			return nil;
		}
	}

	//init gl for drawing
	[self setFrame:frame];
	[self setBoundsRotation:0];
#endif

	//init screen buffer
	[self setScreenState:[ScreenState blackScreenState]];
	
	cdsController = nil;
	
	return self;
}

- (void)dealloc
{
#ifdef HAVE_OPENGL
	[context release];
#endif
	[screen_buffer release];

	[super dealloc];
}

- (void)setScreenState:(ScreenState*)screen
{
	if(screen_buffer == screen)return;

	if(screen == nil)
	{
		[CocoaDSUtil quickDialogUsingTitle:NSLocalizedString(@"Error", nil) message:NSLocalizedString(@"Recieved invalid screen update", nil)];
		return;
	}

	[screen_buffer release]; //get rid of old screen data
	screen_buffer = screen;
	[screen_buffer retain]; //retain the new screendata since we will need it if we have to redraw before we recieve another update

	//rotate the screen
#ifdef HAVE_OPENGL
	if(HORIZONTAL([self boundsRotation]))[screen_buffer rotateTo90];
#endif

	//redraw
#ifdef HAVE_OPENGL
	[self display];
#else
	[self setImage:[screen_buffer image]];
#endif
}

- (const ScreenState*)screenState
{
#ifdef HAVE_OPENGL
	ScreenState *temp = [[ScreenState alloc] initWithScreenState:screen_buffer];
	if(HORIZONTAL([self boundsRotation]))[temp rotateTo0];
	return temp;
#else
	return screen_buffer;
#endif
}

- (float)screenWidth
{
	return DS_SCREEN_WIDTH;
}

- (float)screenHeight
{
	return DS_SCREEN_HEIGHT*2;
}

- (NSSize) normalSize
{
	return [ScreenState size];
}

- (void) setScale:(double)scalar
{
	viewScale = scalar;
}

- (double) scale
{
	return viewScale;
}

- (void) setRotation:(double)angleDegrees
{
	viewRotation = angleDegrees;
	[self setBoundsRotation:-angleDegrees];
}

- (double) rotation
{
	return viewRotation;
}

- (void) setDisplayMode:(NSInteger)theMode
{
	// Do nothing. This is a stub function only.
}

- (NSInteger) displayMode
{
	return displayMode;
}

- (void) setGpuStateFlags:(UInt32)flags
{
	gpuStateFlags = flags;
	
	if (flags & GPUSTATE_MAIN_GPU_MASK)
	{
		SetGPUDisplayState(DS_GPU_TYPE_MAIN, true);
	}
	else
	{
		SetGPUDisplayState(DS_GPU_TYPE_MAIN, false);
	}
	
	if (flags & GPUSTATE_MAIN_BG0_MASK)
	{
		SetGPULayerState(DS_GPU_TYPE_MAIN, 0, true);
	}
	else
	{
		SetGPULayerState(DS_GPU_TYPE_MAIN, 0, false);
	}
	
	if (flags & GPUSTATE_MAIN_BG1_MASK)
	{
		SetGPULayerState(DS_GPU_TYPE_MAIN, 1, true);
	}
	else
	{
		SetGPULayerState(DS_GPU_TYPE_MAIN, 1, false);
	}
	
	if (flags & GPUSTATE_MAIN_BG2_MASK)
	{
		SetGPULayerState(DS_GPU_TYPE_MAIN, 2, true);
	}
	else
	{
		SetGPULayerState(DS_GPU_TYPE_MAIN, 2, false);
	}
	
	if (flags & GPUSTATE_MAIN_BG3_MASK)
	{
		SetGPULayerState(DS_GPU_TYPE_MAIN, 3, true);
	}
	else
	{
		SetGPULayerState(DS_GPU_TYPE_MAIN, 3, false);
	}
	
	if (flags & GPUSTATE_MAIN_OBJ_MASK)
	{
		SetGPULayerState(DS_GPU_TYPE_MAIN, 4, true);
	}
	else
	{
		SetGPULayerState(DS_GPU_TYPE_MAIN, 4, false);
	}
	
	if (flags & GPUSTATE_SUB_GPU_MASK)
	{
		SetGPUDisplayState(DS_GPU_TYPE_SUB, true);
	}
	else
	{
		SetGPUDisplayState(DS_GPU_TYPE_SUB, false);
	}
	
	if (flags & GPUSTATE_SUB_BG0_MASK)
	{
		SetGPULayerState(DS_GPU_TYPE_SUB, 0, true);
	}
	else
	{
		SetGPULayerState(DS_GPU_TYPE_SUB, 0, false);
	}
	
	if (flags & GPUSTATE_SUB_BG1_MASK)
	{
		SetGPULayerState(DS_GPU_TYPE_SUB, 1, true);
	}
	else
	{
		SetGPULayerState(DS_GPU_TYPE_SUB, 1, false);
	}
	
	if (flags & GPUSTATE_SUB_BG2_MASK)
	{
		SetGPULayerState(DS_GPU_TYPE_SUB, 2, true);
	}
	else
	{
		SetGPULayerState(DS_GPU_TYPE_SUB, 2, false);
	}
	
	if (flags & GPUSTATE_SUB_BG3_MASK)
	{
		SetGPULayerState(DS_GPU_TYPE_SUB, 3, true);
	}
	else
	{
		SetGPULayerState(DS_GPU_TYPE_SUB, 3, false);
	}
	
	if (flags & GPUSTATE_SUB_OBJ_MASK)
	{
		SetGPULayerState(DS_GPU_TYPE_SUB, 4, true);
	}
	else
	{
		SetGPULayerState(DS_GPU_TYPE_SUB, 4, false);
	}
}

- (UInt32) gpuStateFlags
{
	return gpuStateFlags;
}

- (void) setCdsController:(CocoaDSController *)theController
{
	cdsController = theController;
}

- (CocoaDSController*) cdsController
{
	return cdsController;
}

- (void) setViewToBlack
{
	[self setScreenState:[ScreenState blackScreenState]];
}

- (void) setViewToWhite
{
	[self setScreenState:[ScreenState whiteScreenState]];
}

- (BOOL) gpuStateByBit:(UInt32)stateBit
{
	BOOL result = NO;
	UInt32 flags = [self gpuStateFlags];
	
	if (flags & (1 << stateBit))
	{
		result = YES;
	}
	
	return result;
}

#ifdef HAVE_OPENGL
- (void)viewDidMoveToWindow
{//if the view moves to another window we need to update the drawable object

	if(!context)return;

	//withdraw from recieving updates on previously window, if any
	[[NSNotificationCenter defaultCenter] removeObserver:context];

	if([self window] != nil)
	{
		[context setView:self];

		//udpate drawable if the window changed
		[[NSNotificationCenter defaultCenter] addObserver:context selector:@selector(update) name:@"NSWindowDidResizeNotification" object:[self window]];

	} else [context clearDrawable];

}
#endif

#ifdef HAVE_OPENGL
- (void)drawRect:(NSRect)bounds
{
	if(screen_buffer == nil)return; //simply dont draw anything if we dont have a screen data object allocated

	[context makeCurrentContext];

	if([self boundsRotation] == 0 || [self boundsRotation] == -180)
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
#endif

#ifdef HAVE_OPENGL
- (void)setFrame:(NSRect)rect
{
	[super setFrame:rect];

	[context makeCurrentContext];
	[context update];

	//set the viewport (so the raster pos will be correct)
	glViewport(0, 0, rect.size.width, rect.size.height);

	float angle = [self boundsRotation];

	if(angle == 0)
	{
		glRasterPos2f(-1, 1);
		glPixelZoom(((float)rect.size.width) / ((float)DS_SCREEN_WIDTH), -((float)rect.size.height) / ((float)DS_SCREEN_HEIGHT*2));
	} else if(angle == -90)
	{
		glRasterPos2f(-1, 1);
		glPixelZoom(((float)rect.size.width) / ((float)DS_SCREEN_HEIGHT*2), -((float)rect.size.height) / ((float)DS_SCREEN_WIDTH));
	} else if (angle == -180)
	{
		glRasterPos2f(1, -1);
		glPixelZoom(-((float)rect.size.width) / ((float)DS_SCREEN_WIDTH), ((float)rect.size.height) / ((float)DS_SCREEN_HEIGHT*2));
	} else if (angle == -270)
	{
		glRasterPos2f(1, -1);
		glPixelZoom(-((float)rect.size.width) / ((float)DS_SCREEN_HEIGHT*2), ((float)rect.size.height) / ((float)DS_SCREEN_WIDTH));
	}
}
#endif

#ifdef HAVE_OPENGL
- (BOOL)isOpaque
{
	if(screen_buffer)
		return YES;

	//if there is no screen buffer, then we can't draw anything
	//so this view is completely transparent
	return NO;
}
#endif

#ifdef HAVE_OPENGL
- (void)setBoundsRotation:(CGFloat)angle
{
	int angleInt = (int)angle;
	int old_angle = (int)[self boundsRotation];

	[super setBoundsRotation:angleInt];

	[context makeCurrentContext];

	NSSize size = [self frame].size;

	if(angleInt == 0)
	{
		glRasterPos2f(-1, 1);
		glPixelZoom(((float)size.width) / ((float)DS_SCREEN_WIDTH), -((float)size.height) / ((float)DS_SCREEN_HEIGHT*2));
	} else if(angleInt == -90)
	{
		glRasterPos2f(-1, 1);
		glPixelZoom(((float)size.width) / ((float)DS_SCREEN_HEIGHT*2), -((float)size.height) / ((float)DS_SCREEN_WIDTH));
	} else if (angleInt == -180)
	{
		glRasterPos2f(1, -1);
		glPixelZoom(-((float)size.width) / ((float)DS_SCREEN_WIDTH), ((float)size.height) / ((float)DS_SCREEN_HEIGHT*2));
	} else if (angleInt == -270)
	{
		glRasterPos2f(1, -1);
		glPixelZoom(-((float)size.width) / ((float)DS_SCREEN_HEIGHT*2), ((float)size.height) / ((float)DS_SCREEN_WIDTH));
	}

	//Rotate the screen buffer
	if(HORIZONTAL(angleInt) && VERTICAL(old_angle))
		[screen_buffer rotateTo90];

	if(VERTICAL(angleInt) && HORIZONTAL(old_angle))
		[screen_buffer rotateTo0];
}
#endif

- (NSPoint) convertPointToDS:(NSPoint)touchLoc
{
	const CGFloat doubleDisplayHeight = (CGFloat)(GPU_DISPLAY_HEIGHT * 2);
	const NSInteger rotation = (NSInteger)[self boundsRotation];
	const CGFloat frameWidth = [self frame].size.width;
	const CGFloat frameHeight = [self frame].size.height;
	
	if(rotation == 0)
	{
		// Scale
		touchLoc.x *= (CGFloat)GPU_DISPLAY_WIDTH / frameWidth;
		touchLoc.y *= doubleDisplayHeight / frameHeight;
	}
	else if(rotation == -90)
	{
		// Normalize
		touchLoc.x += frameHeight;
		
		// Scale
		touchLoc.x *= doubleDisplayHeight / frameWidth;
		touchLoc.y *= (CGFloat)GPU_DISPLAY_WIDTH / frameHeight;
	}
	else if(rotation == -180)
	{
		// Normalize
		touchLoc.x += frameWidth;
		touchLoc.y += frameHeight;
		
		// Scale
		touchLoc.x *= (CGFloat)GPU_DISPLAY_WIDTH / frameWidth;
		touchLoc.y *= doubleDisplayHeight / frameHeight;
	}
	else if(rotation == -270)
	{
		// Normalize
		touchLoc.y += frameWidth;
		
		// Scale
		touchLoc.x *= doubleDisplayHeight / frameWidth;
		touchLoc.y *= (CGFloat)GPU_DISPLAY_WIDTH / frameHeight;
	}
	
	// Normalize the y-coordinate to the DS.
	touchLoc.y = GPU_DISPLAY_HEIGHT - touchLoc.y;
	
	// Constrain the touch point to the DS dimensions.
	if (touchLoc.x < 0)
	{
		touchLoc.x = 0;
	}
	else if (touchLoc.x > (GPU_DISPLAY_WIDTH - 1))
	{
		touchLoc.x = (GPU_DISPLAY_WIDTH - 1);
	}
	
	if (touchLoc.y < 0)
	{
		touchLoc.y = 0;
	}
	else if (touchLoc.y > (GPU_DISPLAY_HEIGHT - 1))
	{
		touchLoc.y = (GPU_DISPLAY_HEIGHT - 1);
	}
	
	return touchLoc;
}

- (void)mouseDown:(NSEvent*)event
{
	// Convert the clicked location from window coordinates, to view coordinates,
	// and finally to DS touchscreen coordinates.
	NSPoint touchLoc = [event locationInWindow];
	touchLoc = [self convertPoint:touchLoc fromView:nil];
	touchLoc = [self convertPointToDS:touchLoc];
		
	if(touchLoc.x >= 0 && touchLoc.y >= 0)
	{
		[cdsController touch:touchLoc];
	}
}

- (void)mouseDragged:(NSEvent*)event
{
	[self mouseDown:event];
}

- (void)mouseUp:(NSEvent*)event
{
	[cdsController releaseTouch];
}

@end

#ifdef HAVE_OPENGL
@implementation ScreenState (extended)
- (void)rotateTo90
{
	int width = [ScreenState width], height = [ScreenState height];

	unsigned char temp_buffer[width * height * DS_BPP];
	memcpy(temp_buffer, color_data, width * height * DS_BPP);

	int x, y;
	for(x = 0; x< width; x++)
		for(y = 0; y < height; y++)
		{
			color_data[(x * height + (height - y - 1)) * 2] = temp_buffer[(y * width + x) * 2];
			color_data[(x * height + (height - y - 1)) * 2 + 1] = temp_buffer[(y * width + x) * 2 + 1];
		}
}

- (void)rotateTo0
{
	int height = [ScreenState width], width = [ScreenState height];

	unsigned char temp_buffer[width * height * DS_BPP];
	memcpy(temp_buffer, color_data, width * height * DS_BPP);

	int x, y;
	for(x = 0; x< width; x++)
		for(y = 0; y < height; y++)
		{
			color_data[((width - x - 1) * height + y) * 2] = temp_buffer[(y * width + x) * 2];
			color_data[((width - x - 1) * height + y) * 2 + 1] = temp_buffer[(y * width + x) * 2 + 1];
		}
}
@end
#endif

void SetGPULayerState(int displayType, unsigned int i, bool state)
{
	GPU *theGpu = NULL;
	
	// Check bounds on the layer index.
	if(i > 4)
	{
		return;
	}
	
	switch (displayType)
	{
		case DS_GPU_TYPE_MAIN:
			theGpu = SubScreen.gpu;
			break;
			
		case DS_GPU_TYPE_SUB:
			theGpu = MainScreen.gpu;
			break;
			
		case DS_GPU_TYPE_COMBO:
			SetGPULayerState(DS_GPU_TYPE_SUB, i, state); // Recursive call
			theGpu = MainScreen.gpu;
			break;
			
		default:
			break;
	}
	
	if (theGpu != NULL)
	{
		if (state)
		{
			GPU_addBack(theGpu, i);
		}
		else
		{
			GPU_remove(theGpu, i);
		}
	}
}

bool GetGPULayerState(int displayType, unsigned int i)
{
	bool result = false;
	
	// Check bounds on the layer index.
	if(i > 4)
	{
		return result;
	}
	
	switch (displayType)
	{
		case DS_GPU_TYPE_MAIN:
			if (SubScreen.gpu != nil)
			{
				result = CommonSettings.dispLayers[SubScreen.gpu->core][i];
			}
			break;
			
		case DS_GPU_TYPE_SUB:
			if (MainScreen.gpu != nil)
			{
				result = CommonSettings.dispLayers[MainScreen.gpu->core][i];
			}
			break;
			
		case DS_GPU_TYPE_COMBO:
			if (SubScreen.gpu != nil && MainScreen.gpu != nil)
			{
				result = (CommonSettings.dispLayers[SubScreen.gpu->core][i] && CommonSettings.dispLayers[MainScreen.gpu->core][i]);
			}
			break;
			
		default:
			break;
	}
	
	return result;
}

void SetGPUDisplayState(int displayType, bool state)
{
	switch (displayType)
	{
		case DS_GPU_TYPE_MAIN:
			CommonSettings.showGpu.sub = state;
			break;
			
		case DS_GPU_TYPE_SUB:
			CommonSettings.showGpu.main = state;
			break;
			
		case DS_GPU_TYPE_COMBO:
			CommonSettings.showGpu.sub = state;
			CommonSettings.showGpu.main = state;
			break;
			
		default:
			break;
	}
}

bool GetGPUDisplayState(int displayType)
{
	bool result = false;
	
	switch (displayType)
	{
		case DS_GPU_TYPE_MAIN:
			result = CommonSettings.showGpu.sub;
			break;
			
		case DS_GPU_TYPE_SUB:
			result = CommonSettings.showGpu.main;
			break;
			
		case DS_GPU_TYPE_COMBO:
			result = (CommonSettings.showGpu.sub && CommonSettings.showGpu.main);
			break;
			
		default:
			break;
	}
	
	return result;
}
