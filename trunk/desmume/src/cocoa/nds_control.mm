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

#import "nds_control.h"
#import "preferences.h"
#import "screen_state.h"
#import "main_window.h"

#ifdef DESMUME_COCOA
#import "sndOSX.h"
#endif

#ifdef HAVE_OPENGL
#import <OpenGL/OpenGL.h>
#import <OpenGL/gl.h>
#endif

//DeSmuME general includes
#include "../NDSSystem.h"
#include "../saves.h"
#include "../render3D.h"
#include "../GPU.h"
#include "../OGLRender.h"
#include "../rasterize.h"
#undef BOOL

//this bool controls whether we will use a timer to constantly check for screen updates
//or if we can use inter-thread messaging introduced in leopard to update the screen (more efficient)
bool timer_based;

//ds screens are 59.8 frames per sec, so 1/59.8 seconds per frame
//times one million for microseconds per frame
#define DS_SECONDS_PER_FRAME (1.0 / 59.8)
#define DS_MICROSECONDS_PER_FRAME (1.0 / 59.8) * 1000000.0

//accessed from other files
volatile bool execute = true;

GPU3DInterface *core3DList[] = {
&gpu3DNull,
&gpu3DRasterize,
#ifdef HAVE_OPENGL
//&gpu3Dgl,
#endif
NULL
};

enum
{
	CORE3DLIST_NULL = 0,
	CORE3DLIST_RASTERIZE,
	CORE3DLIST_OPENGL
};

SoundInterface_struct *SNDCoreList[] = {
&SNDDummy,
#ifdef DESMUME_COCOA
&SNDOSX,
#endif
NULL
};

struct NDS_fw_config_data macDS_firmware;

bool opengl_init()
{
	return true;
}

@implementation CocoaDSStateBuffer

- (id) init
{
	frame_skip = -1; //default to auto frame skip
	speed_limit = 100; //default to max speed = normal speed
	
	return self;
}

@end

@implementation NintendoDS
- (id)init
{
	//
	self = [super init];
	if(self == nil)return nil;

	display_object = nil;
	error_object = nil;
	frame_skip = -1; //default to auto frame skip
	speed_limit = 100; //default to max speed = normal speed
	calcTimeBudget = (NSTimeInterval)(DS_SECONDS_PER_FRAME / ((float)speed_limit / 100.0));
	gui_thread = [NSThread currentThread];
	current_file = nil;
	flash_file = nil;
	execution_lock = [[NSLock alloc] init];
	sound_lock = [[NSLock alloc] init];
	current_screen = nil;
  
#ifdef GDB_STUB
  arm9_gdb_port = 0;
  arm7_gdb_port = 0;
  arm9_gdb_stub = NULL;
  arm7_gdb_stub = NULL;
  struct armcpu_memory_iface *arm9_memio = &arm9_base_memory_iface;
  struct armcpu_memory_iface *arm7_memio = &arm7_base_memory_iface;
  struct armcpu_ctrl_iface *arm9_ctrl_iface;
  struct armcpu_ctrl_iface *arm7_ctrl_iface;
#endif

	//Set the flash file if it's in the prefs/cmd line.  It will be nil if it isn't.
	NSUserDefaults *defaults = [NSUserDefaults standardUserDefaults];
	flash_file = [[defaults stringForKey:PREF_FLASH_FILE] retain];
	if ([flash_file length] > 0) {
		//NSLog(@"Using flash file: \"%@\"\n", flash_file);
	} else {
		[flash_file release];
		flash_file = nil;
		//NSLog(@"No flash file given\n");
	}
  
#ifdef GDB_STUB
  //create GDB stubs if required
  arm9_gdb_port = [defaults integerForKey:PREF_ARM9_GDB_PORT];
  arm7_gdb_port = [defaults integerForKey:PREF_ARM7_GDB_PORT];
  
  if(arm9_gdb_port > 0 && arm9_gdb_port < 65536)
  {
    NSLog(@"Using ARM9 GDB port %d", arm9_gdb_port);
    arm9_gdb_stub = createStub_gdb(arm9_gdb_port,
                                   &arm9_memio,
                                   &arm9_direct_memory_iface);
    if ( arm9_gdb_stub == NULL)
    {
      NSLog(@"Failed to create ARM9 gdbstub on port %d\n",arm9_gdb_port);
      exit(1);
    }
  }
  
  if (arm7_gdb_port > 0 && arm7_gdb_port < 65536)
  {
    NSLog(@"Using ARM7 GDB port %d", arm7_gdb_port);
    arm7_gdb_stub = createStub_gdb(arm7_gdb_port,
                                   &arm7_memio,
                                   &arm7_base_memory_iface);
    
    if ( arm7_gdb_stub == NULL) {
      NSLog(@"Failed to create ARM7 gdbstub on port %d\n",arm7_gdb_port);
      exit(1);
    }
  }
#endif

	//check if we can send messages on other threads, which we will use for video update
	//this is for compatibility for tiger and earlier
	timer_based = ([NSObject instancesRespondToSelector:@selector(performSelector:onThread:withObject:waitUntilDone:)]==NO)?true:false;

	//Firmware setup
#ifdef GDB_STUB
  NDS_Init(arm9_memio, &arm9_ctrl_iface,
           arm7_memio, &arm7_ctrl_iface);
#else
  NDS_Init();
#endif

	//use default firmware
	NDS_FillDefaultFirmwareConfigData(&macDS_firmware);
	NDS_CreateDummyFirmware(&macDS_firmware);

  /*
   * Activate the GDB stubs
   * This has to come after the NDS_Init where the cpus are set up.
   */
#ifdef GDB_STUB
  if(arm9_gdb_port > 0 && arm9_gdb_port < 65536)
  {  
    activateStub_gdb( arm9_gdb_stub, arm9_ctrl_iface);
  }
  if(arm7_gdb_port > 0 && arm7_gdb_port < 65536)
  {
    activateStub_gdb( arm7_gdb_stub, arm7_ctrl_iface);
  }
#endif
  
  
	//3D Init
#ifdef HAVE_OPENGL
	NSOpenGLContext *prev_context = [NSOpenGLContext currentContext];
	[prev_context retain];

	bool gl_ready = false;

	NSOpenGLPixelFormatAttribute attrs[] =
	{
		NSOpenGLPFAColorSize, (NSOpenGLPixelFormatAttribute)24,
		NSOpenGLPFAAlphaSize, (NSOpenGLPixelFormatAttribute)8,
		NSOpenGLPFADepthSize, (NSOpenGLPixelFormatAttribute)24,
		NSOpenGLPFAStencilSize, (NSOpenGLPixelFormatAttribute)8,
		NSOpenGLPFAOffScreen,
		(NSOpenGLPixelFormatAttribute)0
	};

	if((pixel_format = [[NSOpenGLPixelFormat alloc] initWithAttributes:attrs]) == nil)
	{
		messageDialog(NSLocalizedString(@"Error", nil), @"Couldn't create OpenGL pixel format for 3D rendering");
		context = nil;

	} else if((context = [[NSOpenGLContext alloc] initWithFormat:pixel_format shareContext:nil]) == nil)
	{
		[pixel_format release];
		pixel_format = nil;
		messageDialog(NSLocalizedString(@"Error", nil), @"Couldn't create OpenGL context for 3D rendering");
	} else
	{
		[context makeCurrentContext];

		//check extensions
		BOOL supports_pixel_buffers = NO;
		const char *extension_list = (const char*)glGetString(GL_EXTENSIONS);
		if(extension_list)
		{
			NSArray *extensions = [[NSString stringWithCString:extension_list encoding:NSASCIIStringEncoding] componentsSeparatedByString:@" "];
			supports_pixel_buffers = [extensions containsObject:@"GL_APPLE_pixel_buffer"];
		}

		//attempt to use a pixel-buffer for hopefully hardware accelerated offscreen drawing
		if(supports_pixel_buffers == YES)
		{
			NSOpenGLPixelBuffer *pixel_buffer = [[NSOpenGLPixelBuffer alloc]
			initWithTextureTarget:GL_TEXTURE_2D
			textureInternalFormat:GL_RGBA
			textureMaxMipMapLevel:0
			pixelsWide:DS_SCREEN_WIDTH
			pixelsHigh:DS_SCREEN_HEIGHT*2];

			if(pixel_buffer == nil)
			{
				GLenum error = glGetError();
				messageDialog(NSLocalizedString(@"Error", nil),
				[NSString stringWithFormat:@"Error setting up rgba pixel buffer for 3D rendering (glerror: %d)", error]);
			} else
			{
				[context setPixelBuffer:pixel_buffer cubeMapFace:0 mipMapLevel:0 currentVirtualScreen:0];
				[pixel_buffer release];
				gl_ready = true;
			}
		}

		//if pixel buffers didn't work out, try simple offscreen renderings (probably software accelerated)
		if(!gl_ready)
		{
			[context setOffScreen:(void*)&gpu_buff width:DS_SCREEN_WIDTH height:DS_SCREEN_HEIGHT rowbytes:DS_SCREEN_WIDTH*5];
			gl_ready = true;
		}
	}

	if(context)
	{
		[context makeCurrentContext];

		//oglrender_init = &opengl_init;
		NDS_3D_SetDriver(CORE3DLIST_RASTERIZE);
		if(!gpu3D->NDS_3D_Init())
			messageDialog(NSLocalizedString(@"Error", nil), @"Unable to initialize OpenGL components");
	}

	if(prev_context != nil) //make sure the old context is restored, and make sure our new context is not set in this thread (since the other thread will need it)
	{
		[prev_context makeCurrentContext];
		[prev_context release];
	} else
		[NSOpenGLContext clearCurrentContext];
#endif

	//Sound Init
	muted = false;
	volume = 100;
#ifdef DESMUME_COCOA
	if(SPU_ChangeSoundCore(SNDCORE_OSX, 735 * 4) != 0)
		messageDialog(NSLocalizedString(@"Error", nil), @"Unable to initialize sound core");
	else
		SPU_SetVolume(volume);
#endif

	//Breakoff a new thread that will execute the ds stuff
	finish = false;
	finished = false;
	run = false;
	paused = false;
	[NSThread detachNewThreadSelector:@selector(run:) toTarget:self withObject:context];

	//Start a timer to update the screen
	if(timer_based)
	{
		video_update_lock = [[NSLock alloc] init];
		[NSTimer scheduledTimerWithTimeInterval:DS_SECONDS_PER_FRAME target:self selector:@selector(videoUpdateTimerHelper) userInfo:nil repeats:YES];
	}
	
	dsStateBuffer = [[CocoaDSStateBuffer alloc] init];
	dsController = [[CocoaDSController alloc] init];

	return self;
}

- (void)setVideoUpdateCallback:(SEL)callback withObject:(id)object
{
	//release object we were previously using
	[display_object release];

	//get and retain the new one
	display_object = object;
	[display_object retain];

	//set the selector
	display_func = callback;
}

- (void)setErrorCallback:(SEL)callback withObject:(id)object;
{
	[error_object release];

	error_object = object;
	[error_object retain];

	error_func = callback;
}

- (void)dealloc
{
	//end the other thread
	finish = true;
	while(!finished){}
	
	[dsStateBuffer release];
	[dsController release];
	
	[display_object release];
	[error_object release];
	[context release];
	[pixel_format release];
	[current_file release];
	[flash_file release];
	[sound_lock release];
	[execution_lock release];

	NDS_DeInit();

#ifdef GDB_STUB
  if(arm9_gdb_port > 0 && arm9_gdb_port < 65536)
  {
    destroyStub_gdb(arm9_gdb_stub);
  }
  if(arm7_gdb_port > 0 && arm7_gdb_port < 65536)
  {
    destroyStub_gdb(arm7_gdb_stub);
  }
#endif
  
	[super dealloc];
}

- (CocoaDSController*) getDSController
{
	return dsController;
}

- (void)setPlayerName:(NSString*)player_name
{
	//first we convert to UTF-16 which the DS uses to store the nickname
	NSData *string_chars = [player_name dataUsingEncoding:NSUnicodeStringEncoding];

	//copy the bytes
	macDS_firmware.nickname_len = MIN([string_chars length],MAX_FW_NICKNAME_LENGTH);
	[string_chars getBytes:macDS_firmware.nickname length:macDS_firmware.nickname_len];
	macDS_firmware.nickname[macDS_firmware.nickname_len / 2] = 0;

	//set the firmware
	//NDS_CreateDummyFirmware(&macDS_firmware);
}

- (BOOL)loadROM:(NSString*)filename
{
	//pause if not already paused
	BOOL was_paused = [self paused];
	[self pause];

	//get the flash file
	const char *flash;
	if (flash_file != nil)
		flash = [flash_file UTF8String];
	else
		flash = NULL;

	//load the rom
	if(!NDS_LoadROM([filename cStringUsingEncoding:NSUTF8StringEncoding], flash) > 0)
	{
		//if it didn't work give an error and dont unpause
		messageDialog(NSLocalizedString(@"Error", nil), @"Could not open file");

		//continue playing if load didn't work
		if(was_paused == NO)[self execute];

		return NO;
	}

	//clear screen data
	if(current_screen != nil)
	{
		[current_screen release];
		current_screen = nil;
	}

	//set local vars
	current_file = filename;
	[current_file retain];

	//this is incase emulation stopped from the
	//emulation core somehow
	execute = true;

	return YES;
}

- (BOOL)ROMLoaded
{
	return (current_file==nil)?NO:YES;
}

- (void)closeROM
{
	[self pause];

	if(current_screen != nil)
	{
		[current_screen release];
		current_screen = nil;
	}

	NDS_FreeROM();

	[current_file release];
	current_file = nil;
}

- (NSImage*)ROMIcon
{
	NDS_header *header = NDS_getROMHeader();
	if(!header)return nil;

	if(header->IconOff == 0)return nil;

	NSImage *result = [[NSImage alloc] initWithSize:NSMakeSize(32, 32)];
	if(result == nil)return nil;

	NSBitmapImageRep *image = [[NSBitmapImageRep alloc] initWithBitmapDataPlanes:NULL
	pixelsWide:32
	pixelsHigh:32
	bitsPerSample:8
	samplesPerPixel:4
	hasAlpha:YES
	isPlanar:NO
	colorSpaceName:NSCalibratedRGBColorSpace
	bytesPerRow:32 * 4
	bitsPerPixel:32];

	[image setAlpha:YES];

	if(image == nil)
	{
		[result release];
		return nil;
	}

	//load the palette
	//the pallete contains 16 entries, 2 bytes each
	//the first entry represents alpha so the value is ignored

	u8 palette[16*4]; //16 entries at 32 bit (we will convert)
	int x;

	for(x = 0; x < 16; x++)
	{
		u16 temp = T1ReadWord(MMU.CART_ROM, header->IconOff + 0x220 + x*2);
		palette[x*4+0] = (temp & 0x001F) << 3;       //r
		palette[x*4+1] = (temp & 0x03E0) >> 5 << 3;  //g
		palette[x*4+2] = (temp & 0x7C00) >> 10 << 3; //b
		palette[x*4+3] = x==0?0:255; //alpha: color 0 is always transparent
	}

	//load the image
	//the image is 32x32 pixels, each 4bit (correspoding to the pallete)
	//it's stored just before the pallete

	u8 *bitmap_data = [image bitmapData];

	int y, inner_y, inner_x, offset = 0;
	for(y = 0; y < 4; y++) //the image is split into 16 squares (4 on each axis)
	for(x = 0; x < 4; x++)
	for(inner_y = 0; inner_y < 8; inner_y++) //each square is 8x8
	for(inner_x = 0; inner_x < 8; inner_x+=2) //increment by 2 since each byte is two 4bit colors
	{
		//grab the color indicies of the next 2 pixels
		u8 color = T1ReadByte(MMU.CART_ROM, header->IconOff + 0x20 + offset++);

		//set the first pixel color
		*(bitmap_data+( (y*8+inner_y)*32*4+(x*8+inner_x+1)*4+0 )) = palette[(color>>4) * 4 + 0]; //r
		*(bitmap_data+( (y*8+inner_y)*32*4+(x*8+inner_x+1)*4+1 )) = palette[(color>>4) * 4 + 1]; //g
		*(bitmap_data+( (y*8+inner_y)*32*4+(x*8+inner_x+1)*4+2 )) = palette[(color>>4) * 4 + 2]; //b
		*(bitmap_data+( (y*8+inner_y)*32*4+(x*8+inner_x+1)*4+3 )) = palette[(color>>4) * 4 + 3]; //a

		//set the next pixel color
		*(bitmap_data+( (y*8+inner_y)*32*4+(x*8+inner_x+0)*4+0 )) = palette[(color&0x0F) * 4 + 0]; //r
		*(bitmap_data+( (y*8+inner_y)*32*4+(x*8+inner_x+0)*4+1 )) = palette[(color&0x0F) * 4 + 1]; //g
		*(bitmap_data+( (y*8+inner_y)*32*4+(x*8+inner_x+0)*4+2 )) = palette[(color&0x0F) * 4 + 2]; //b
		*(bitmap_data+( (y*8+inner_y)*32*4+(x*8+inner_x+0)*4+3 )) = palette[(color&0x0F) * 4 + 3]; //a
	}

	[result addRepresentation:image];
	[image release];
	[result autorelease];
	return result;
}

- (NSString*)ROMFile
{
	return current_file;
}

- (NSString*)ROMTitle
{
	return [[NSString alloc] initWithCString:(NDS_getROMHeader()->gameTile) encoding:NSUTF8StringEncoding];
}

- (NSInteger)ROMMaker
{
	return NDS_getROMHeader()->makerCode;
}

- (NSInteger)ROMSize
{
	return NDS_getROMHeader()->cardSize;
}

- (NSInteger)ROMARM9Size
{
	return NDS_getROMHeader()->ARM9binSize;
}

- (NSInteger)ROMARM7Size
{
	return NDS_getROMHeader()->ARM7binSize;
}

- (NSInteger)ROMDataSize
{
	return NDS_getROMHeader()->ARM7binSize + NDS_getROMHeader()->ARM7src;
}

- (NSString*)flashFile
{
	return flash_file;
}

- (void)setFlashFile:(NSString*)filename
{
	if (flash_file)
		[flash_file release];
	flash_file = [filename retain];
}

- (BOOL)executing
{
	return run;
}

- (void)execute
{
	run = TRUE;
}

- (BOOL)paused
{
	return !run;
}

- (void)pause
{
	run = false;

	//wait for the other thread to stop execution
	while (!paused) {}
}

- (void)reset
{
	//note that the execution_lock method would probably be a little better

	//but the NDS_Reset() function sets execution to false for some reason
	//we treat execution == false as an emulation error
	//pausing allows the other thread to not think theres an emulation error

	//[execution_lock lock];
	bool old_run = run;
	if(old_run)
	{
	   run = false;
		while(!paused){}
	}

	NDS_Reset();

	//[execution_lock unlock];
	run = old_run;

	//if there was a previous emulation error, clear it, since we reset
	execute = true;
}

- (void)setFrameSkip:(int)frameskip
{
	dsStateBuffer->frame_skip = frameskip;
	
	if(frameskip < 0)
	{
		dsStateBuffer->frame_skip = -1;
	}
	
	doesConfigNeedUpdate = true;
}

- (int)frameSkip
{
	return frame_skip;
}

- (void)setSpeedLimit:(int)speedLimit
{
	if(speedLimit < 0 || speedLimit > 1000)
	{
		return;
	}

	dsStateBuffer->speed_limit = speedLimit;
	
	doesConfigNeedUpdate = true;
}

- (int)speedLimit
{
	return speed_limit;
}

- (void)setSaveType:(int)savetype
{
	if(savetype < 0 || savetype > 6) savetype = 0;
	
	// Set the savetype
	backup_setManualBackupType(savetype);
}

- (int)saveType
{
	return CommonSettings.manualBackupType;
}

- (BOOL)saveState:(NSString*)file
{
	[execution_lock lock];
	
	BOOL result = NO;
	if(savestate_save([file cStringUsingEncoding:NSUTF8StringEncoding]))
		result = YES;
	
	[execution_lock unlock];
	
	return result;
}

- (BOOL)loadState:(NSString*)file
{
	[execution_lock lock];
	
	//Set the GPU context (if it exists) incase the core needs to load anything into opengl during state load
	NSOpenGLContext *prev_context = [NSOpenGLContext currentContext];
	[prev_context retain];
	[context makeCurrentContext];
	
	BOOL result = NO;
	if(savestate_load([file cStringUsingEncoding:NSUTF8StringEncoding]))
		result = YES;
	
	[execution_lock unlock];
	
	if(prev_context != nil)
	{
		[prev_context makeCurrentContext];
		[prev_context release];
	} else
		[NSOpenGLContext clearCurrentContext];
	
	return result;
}

- (BOOL) isSubScreenLayerDisplayed:(int)i
{
	GPU *theGPU = SubScreen.gpu;
	BOOL isLayerDisplayed = NO;
	
	// Check bounds on the layer index.
	if(i < 0 || i > 4)
	{
		return isLayerDisplayed;
	}
	
	// Check if theGPU exists.
	if(theGPU == nil)
	{
		return isLayerDisplayed;
	}
	
	// CommonSettings.dispLayers is returned as a bool, so we convert
	// to BOOL here.
	if (CommonSettings.dispLayers[theGPU->core][i])
	{
		isLayerDisplayed = YES;
	}
	
	return isLayerDisplayed;
}

- (BOOL) isMainScreenLayerDisplayed:(int)i
{
	GPU *theGPU = MainScreen.gpu;
	BOOL isLayerDisplayed = NO;
	
	// Check bounds on the layer index.
	if(i < 0 || i > 4)
	{
		return isLayerDisplayed;
	}
	
	// Check if theGPU exists.
	if(theGPU == nil)
	{
		return isLayerDisplayed;
	}
	
	// CommonSettings.dispLayers is returned as a bool, so we convert
	// to BOOL here.
	if (CommonSettings.dispLayers[theGPU->core][i])
	{
		isLayerDisplayed = YES;
	}
	
	return isLayerDisplayed;
}

- (void) toggleSubScreenLayer:(int)i
{
	GPU *theGPU = SubScreen.gpu;
	BOOL isLayerDisplayed;
	
	// Check bounds on the layer index.
	if(i < 0 || i > 4)
	{
		return;
	}
	
	// Check if theGPU exists.
	if(theGPU == nil)
	{
		return;	
	}
	
	isLayerDisplayed = [self isSubScreenLayerDisplayed:i];
	if(isLayerDisplayed == YES)
	{
		GPU_remove(theGPU, i);
	}
	else
	{
		GPU_addBack(theGPU, i);
	}
}

- (void) toggleMainScreenLayer:(int)i
{
	GPU *theGPU = MainScreen.gpu;
	BOOL isLayerDisplayed;
	
	// Check bounds on the layer index.
	if(i < 0 || i > 4)
	{
		return;
	}
	
	// Check if theGPU exists.
	if(theGPU == nil)
	{
		return;	
	}
	
	isLayerDisplayed = [self isMainScreenLayerDisplayed:i];
	if(isLayerDisplayed == YES)
	{
		GPU_remove(theGPU, i);
	}
	else
	{
		GPU_addBack(theGPU, i);
	}
}

- (BOOL)hasSound
{
	SoundInterface_struct *core = SPU_SoundCore();
	if(!core)return NO;
	return core != &SNDDummy;
}

- (void)setVolume:(int)new_volume
{
	if(new_volume < 0)new_volume = 0;
	if(new_volume > 100)new_volume = 100;
	if(volume == new_volume)return;

	volume = new_volume;
	[sound_lock lock];
	SPU_SetVolume(volume);
	[sound_lock unlock];
}

- (int)volume
{
	if([self hasSound])
		return volume;
	return -1;
}

- (void)enableMute
{
	[sound_lock lock];
	SPU_SetVolume(0);
	[sound_lock unlock];
	muted = true;
}

- (void)disableMute
{
	[sound_lock lock];
	SPU_SetVolume(volume);
	[sound_lock unlock];
	muted = false;
}

- (void)toggleMute
{
	if(muted)
		[self disableMute];
	else
		[self enableMute];
}

- (BOOL)muted
{
	return muted?YES:NO;
}

//----------------------------
//Here's the run function which continuously executes in a separate thread
//it's controlled by the run and finish varaiables of the instance
//and its status can be determined with the paused and finished variables

- (void)videoUpdateHelper:(ScreenState*)screen_data
{
	//we check if the emulation is running before we update the screen
	//this is because the emulation could have been paused or ended
	//with a video update call still queued on this thread

	//this also means it may skip a frame or more upon resuming emulation

	//the ideal thing would be to find a way to have the pause function
	//check what video updates are queued on this run loop and perform them

	if(run)
		[display_object performSelector:display_func withObject:screen_data];
}

- (void)videoUpdateTimerHelper
{
	if(!run)return;

	[video_update_lock lock];
	ScreenState *screen = current_screen;
	[screen retain];
	current_screen = nil;
	[video_update_lock unlock];

	if(screen != nil)
	{
		[display_object performSelector:display_func withObject:screen];
		[screen release];
	}

}

- (void)run:(NSOpenGLContext*)gl_context
{
	NSAutoreleasePool *autorelease = [[NSAutoreleasePool alloc] init];
	
	NSDate *loopStartDate;
	NSDate *emulation_start_date;
	NSDate *frame_start_date;
	
	NSTimeInterval timeBudget;
	NSTimeInterval timePad;
	
#ifdef HAVE_OPENGL
	[gl_context retain];
	[gl_context makeCurrentContext];
	CGLLockContext((CGLContextObj)[gl_context CGLContextObj]);
#endif
	
	//program main loop
	while(!finish)
	{
		if(!run)
		{
			paused = true;
		}
		
		//run the emulator
		while(run && execute) //run controls when the emulator runs, execute prevents it from continuing execution if there are errors
		{
		/*
			Get the start time for the loop. This will be needed when it comes
			time to determine the total time spent, and then calculating what
			timePad should be.
		 */
			loopStartDate = [NSDate date];
			
		/*
			Some controls may affect how the loop runs.
		 
			Instead of checking and modifying the NDS config every time through
			the loop, only change the config on an as-needed basis.
		 */
			if(doesConfigNeedUpdate == true)
			{
				[self updateConfig];
				doesConfigNeedUpdate = false;
			}
			
			// Force paused state.
			paused = false;
			
		/*
			Set up our time budget, which is equal to calcTimeBudget, which
			is the relationship between DS_SECONDS_PER_FRAME and speed_limit.
			timeBudget represents how much time we have to spend doing the
			various functions of making a new frame.
		 
			The major parts we spend our time on is:
				- Emulation
				- Drawing the frame
				- Pad time
		 
			The priorities for spending time are the same as the order listed
			above.
		 
			timePad represents any excess time that can be released back
			to the OS.
		 */
			timeBudget = calcTimeBudget;
			timePad = timeBudget;
			
		/*
			Set up the inputs for the emulator.
			
			The time taken up by this step should be insignificant, so we
			won't bother calculating this in the time budget.
		 */
			[dsController setupAllDSInputs];
			NDS_beginProcessingInput();
		/*
			Shouldn't need to do any special processing steps in between.
			We'll just jump directly to ending the input processing.
		 */
			NDS_endProcessingInput();
			
			emulation_start_date = [NSDate date];
			[self emulateDS];
			
		/*
			Subtract the emulation time from our time budget.
			
			For some reason, [NSDate timeIntervalSinceNow] returns a
			negative interval if the receiver is an earlier date than now.
			So to subtract from timeBudget, we add the interval.
		 
			Go figure.
		 */
			timeBudget += [emulation_start_date timeIntervalSinceNow];
			
		/*
			If we have time left in our time budget, draw the frame.
			
			But if we don't have time left in our time budget, then we need
			to make a decision on whether to simply drop the frame, or just
			draw the frame.
		 
			Currently, the decision is to just draw everything because
			frame drawing time is very negligible. Dropping a whole bunch of
			frames will NOT yield any significant speed increase.
		 */
			if(timeBudget > 0)
			{
				frame_start_date = [NSDate date];
				[self drawFrame];
				
				// Subtract the drawing time from our time budget.
				timeBudget += [frame_start_date timeIntervalSinceNow];
			}
			else
			{
				// Don't even bother calculating timeBudget. We've already
				// gone over at this point.
				[self drawFrame];
			}
			
			// If there is any time left in the loop, go ahead and pad it.
			timePad += [loopStartDate timeIntervalSinceNow];
			if(timePad > 0)
			{
				[self padTime:timePad];
			}
		}
		
		//when emulation is paused, return CPU time to the OS
		[NSThread sleepUntilDate:[NSDate dateWithTimeIntervalSinceNow:.1]];
	}
	
#ifdef HAVE_OPENGL
	CGLUnlockContext((CGLContextObj)[gl_context CGLContextObj]);
	[gl_context release];
#endif
	
	[autorelease release];
	
	paused = true;
	finished = true;
}

- (void) emulateDS
{
	[execution_lock lock];
	
	NDS_exec<false>();
	
	[sound_lock lock];
	
	SPU_Emulate_user();
	
	[sound_lock unlock];
	
	[execution_lock unlock];
}

- (void) drawFrame
{
	ScreenState *new_screen_data = [[ScreenState alloc] initWithColorData:GPU_screen];
	
	if(timer_based)
	{ //for tiger compatibility
		[video_update_lock lock];
		[current_screen release];
		current_screen = new_screen_data;
		[video_update_lock unlock];
	}
	else
	{ //for leopard and later
		
		//this will generate a warning when compiling on tiger or earlier, but it should
		//be ok since the purpose of the if statement is to check if this will work
		[self performSelector:@selector(videoUpdateHelper:) onThread:gui_thread withObject:new_screen_data waitUntilDone:NO];
		[new_screen_data release]; //performSelector will auto retain the screen data while the other thread displays
	}
}

- (void) padTime:(NSTimeInterval)timePad
{
#if (MAC_OS_X_VERSION_MAX_ALLOWED <= MAC_OS_X_VERSION_10_4) // Code for Mac OS X 10.4 and earlier
	
	[NSThread sleepUntilDate:[NSDate dateWithTimeIntervalSinceNow:timePad]];
	
#else // Code for Mac OS X 10.5 and later
	
	[NSThread sleepForTimeInterval:timePad];
	
#endif
}

- (void) updateConfig
{
	// Update the Nintendo DS config
	frame_skip = dsStateBuffer->frame_skip;
	speed_limit = dsStateBuffer->speed_limit;
	
	if(speed_limit <= 0)
	{
		calcTimeBudget = 0;
	}
	else if(speed_limit > 0 && speed_limit < 1000)
	{
		calcTimeBudget = (NSTimeInterval)(DS_SECONDS_PER_FRAME / ((float)speed_limit / 100.0));
	}
	else
	{
		calcTimeBudget = (NSTimeInterval)(DS_SECONDS_PER_FRAME / ((float)1000.0 / 100.0));
	}
}

@end
