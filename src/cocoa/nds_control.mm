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

#ifdef DESMUME_COCOA
#import "sndOSX.h"
#endif

#ifdef HAVE_OPENGL
#import <OpenGL/OpenGL.h>
#import <OpenGL/gl.h>
#endif

//DeSmuME general includes
#define OBJ_C
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
volatile desmume_BOOL execute = true;

GPU3DInterface *core3DList[] = {
&gpu3DNull,
&gpu3DRasterize,
#ifdef HAVE_OPENGL
&gpu3Dgl,
#endif
NULL
};

SoundInterface_struct *SNDCoreList[] = {
&SNDDummy,
#ifdef DESMUME_COCOA
&SNDOSX,
#endif
NULL
};

struct NDS_fw_config_data firmware;

bool opengl_init()
{
	return true;
}

@implementation NintendoDS
- (id)init;
{
	//
	self = [super init];
	if(self == nil)return nil;

	display_object = nil;
	error_object = nil;
	frame_skip = -1; //default to auto frame skip
	speed_limit = 100; //default to max speed = normal speed
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
	NDS_FillDefaultFirmwareConfigData(&firmware);
	NDS_CreateDummyFirmware(&firmware);

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

		oglrender_init = &opengl_init;
		NDS_3D_SetDriver(1);
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
		[NSTimer scheduledTimerWithTimeInterval:1.0f/60.0f target:self selector:@selector(videoUpdateTimerHelper) userInfo:nil repeats:YES];
	}

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

- (void)setPlayerName:(NSString*)player_name
{
	//first we convert to UTF-16 which the DS uses to store the nickname
	NSData *string_chars = [player_name dataUsingEncoding:NSUnicodeStringEncoding];

	//copy the bytes
	firmware.nickname_len = MIN([string_chars length],MAX_FW_NICKNAME_LENGTH);
	[string_chars getBytes:firmware.nickname length:firmware.nickname_len];
	firmware.nickname[firmware.nickname_len / 2] = 0;

	//set the firmware
	//NDS_CreateDummyFirmware(&firmware);
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
	if(frameskip < 0)frame_skip = -1;
	else frame_skip = frameskip;
}

- (int)frameSkip
{
	return frame_skip;
}

- (void)setSpeedLimit:(int)speedLimit
{
	if(speedLimit < 0)return;
	if(speedLimit > 1000)return;

	speed_limit = speedLimit;
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


- (void)touch:(NSPoint)point
{
	NDS_setTouchPos((unsigned short)point.x, (unsigned short)point.y);
}

- (void)releaseTouch
{
	NDS_releaseTouch();
}

- (void)pressStart
{
#ifndef __BIG_ENDIAN__
	((u16 *)ARM9Mem.ARM9_REG)[0x130>>1] &= 0xFFF7;
	((u16 *)MMU.ARM7_REG)[0x130>>1] &= 0xFFF7;
#else
	((u16 *)ARM9Mem.ARM9_REG)[0x130>>1] &= 0xF7FF;
	((u16 *)MMU.ARM7_REG)[0x130>>1] &= 0xF7FF;
#endif
}

- (void)liftStart
{
#ifndef __BIG_ENDIAN__
	((u16 *)ARM9Mem.ARM9_REG)[0x130>>1] |= 0x0008;
	((u16 *)MMU.ARM7_REG)[0x130>>1] |= 0x0008;
#else
	((u16 *)ARM9Mem.ARM9_REG)[0x130>>1] |= 0x0800;
	((u16 *)MMU.ARM7_REG)[0x130>>1] |= 0x0800;
#endif
}

- (BOOL)start
{
#ifndef __BIG_ENDIAN__
	if((((u16 *)MMU.ARM7_REG)[0x130>>1] & 0x0008) == 0)
#else
	if((((u16 *)MMU.ARM7_REG)[0x130>>1] & 0x0800) == 0)
#endif
		return YES;
	return NO;
}

- (void)pressSelect
{
#ifndef __BIG_ENDIAN__
	((u16 *)ARM9Mem.ARM9_REG)[0x130>>1] &= 0xFFFB;
	((u16 *)MMU.ARM7_REG)[0x130>>1] &= 0xFFFB;
#else
	((u16 *)ARM9Mem.ARM9_REG)[0x130>>1] &= 0xFBFF;
	((u16 *)MMU.ARM7_REG)[0x130>>1] &= 0xFBFF;
#endif
}

- (void)liftSelect
{
#ifndef __BIG_ENDIAN__
	((u16 *)ARM9Mem.ARM9_REG)[0x130>>1] |= 0x0004;
	((u16 *)MMU.ARM7_REG)[0x130>>1] |= 0x0004;
#else
	((u16 *)ARM9Mem.ARM9_REG)[0x130>>1] |= 0x0400;
	((u16 *)MMU.ARM7_REG)[0x130>>1] |= 0x0400;
#endif
}

- (BOOL)select
{
#ifndef __BIG_ENDIAN__
	if((((u16 *)MMU.ARM7_REG)[0x130>>1] & 0x0004) == 0)
#else
	if((((u16 *)MMU.ARM7_REG)[0x130>>1] & 0x0400) == 0)
#endif
		return YES;
	return NO;
}

- (void)pressLeft
{
#ifndef __BIG_ENDIAN__
	((u16 *)ARM9Mem.ARM9_REG)[0x130>>1] &= 0xFFDF;
	((u16 *)MMU.ARM7_REG)[0x130>>1] &= 0xFFDF;
#else
	((u16 *)ARM9Mem.ARM9_REG)[0x130>>1] &= 0xDFFF;
	((u16 *)MMU.ARM7_REG)[0x130>>1] &= 0xDFFF;
#endif
}

- (void)liftLeft
{
#ifndef __BIG_ENDIAN__
	((u16 *)ARM9Mem.ARM9_REG)[0x130>>1] |= 0x0020;
	((u16 *)MMU.ARM7_REG)[0x130>>1] |= 0x0020;
#else
	((u16 *)ARM9Mem.ARM9_REG)[0x130>>1] |= 0x2000;
	((u16 *)MMU.ARM7_REG)[0x130>>1] |= 0x2000;
#endif
}

- (BOOL)left
{
#ifndef __BIG_ENDIAN__
	if((((u16 *)ARM9Mem.ARM9_REG)[0x130>>1] & 0x0020) == 0)
#else
	if((((u16 *)ARM9Mem.ARM9_REG)[0x130>>1] & 0x2000) == 0)
#endif
		return YES;
	return NO;
}

- (void)pressRight
{
#ifndef __BIG_ENDIAN__
	((u16 *)ARM9Mem.ARM9_REG)[0x130>>1] &= 0xFFEF;
	((u16 *)MMU.ARM7_REG)[0x130>>1] &= 0xFFEF;
#else
	((u16 *)ARM9Mem.ARM9_REG)[0x130>>1] &= 0xEFFF;
	((u16 *)MMU.ARM7_REG)[0x130>>1] &= 0xEFFF;
#endif
}

- (void)liftRight
{
#ifndef __BIG_ENDIAN__
	((u16 *)ARM9Mem.ARM9_REG)[0x130>>1] |= 0x0010;
	((u16 *)MMU.ARM7_REG)[0x130>>1] |= 0x0010;
#else
	((u16 *)ARM9Mem.ARM9_REG)[0x130>>1] |= 0x1000;
	((u16 *)MMU.ARM7_REG)[0x130>>1] |= 0x1000;
#endif
}

- (BOOL)right
{
#ifndef __BIG_ENDIAN__
	if((((u16*)ARM9Mem.ARM9_REG)[0x130>>1] & 0x0010) == 0)
#else
	if((((u16*)ARM9Mem.ARM9_REG)[0x130>>1] & 0x1000) == 0)
#endif
		return YES;
	return NO;
}

- (void)pressUp
{
#ifndef __BIG_ENDIAN__
	((u16 *)ARM9Mem.ARM9_REG)[0x130>>1] &= 0xFFBF;
	((u16 *)MMU.ARM7_REG)[0x130>>1] &= 0xFFBF;
#else
	((u16 *)ARM9Mem.ARM9_REG)[0x130>>1] &= 0xBFFF;
	((u16 *)MMU.ARM7_REG)[0x130>>1] &= 0xBFFF;
#endif
}

- (void)liftUp
{
#ifndef __BIG_ENDIAN__
	((u16 *)ARM9Mem.ARM9_REG)[0x130>>1] |= 0x0040;
	((u16 *)MMU.ARM7_REG)[0x130>>1] |= 0x0040;
#else
	((u16 *)ARM9Mem.ARM9_REG)[0x130>>1] |= 0x4000;
	((u16 *)MMU.ARM7_REG)[0x130>>1] |= 0x4000;
#endif
}

- (BOOL)up
{
#ifndef __BIG_ENDIAN__
	if((((u16 *)ARM9Mem.ARM9_REG)[0x130>>1] & 0x0040) == 0)
#else
	if((((u16 *)ARM9Mem.ARM9_REG)[0x130>>1] & 0x4000) == 0)
#endif
		return YES;
	return NO;
}

- (void)pressDown
{
#ifndef __BIG_ENDIAN__
	((u16 *)ARM9Mem.ARM9_REG)[0x130>>1] &= 0xFF7F;
	((u16 *)MMU.ARM7_REG)[0x130>>1] &= 0xFF7F;
#else
	((u16 *)ARM9Mem.ARM9_REG)[0x130>>1] &= 0x7FFF;
	((u16 *)MMU.ARM7_REG)[0x130>>1] &= 0x7FFF;
#endif
}

- (void)liftDown
{
#ifndef __BIG_ENDIAN__
	((u16 *)ARM9Mem.ARM9_REG)[0x130>>1] |= 0x0080;
	((u16 *)MMU.ARM7_REG)[0x130>>1] |= 0x0080;
#else
	((u16 *)ARM9Mem.ARM9_REG)[0x130>>1] |= 0x8000;
	((u16 *)MMU.ARM7_REG)[0x130>>1] |= 0x8000;
#endif
}

- (BOOL)down
{
#ifndef __BIG_ENDIAN__
	if((((u16 *)ARM9Mem.ARM9_REG)[0x130>>1] & 0x0080) == 0)
#else
	if((((u16 *)ARM9Mem.ARM9_REG)[0x130>>1] & 0x8000) == 0)
#endif
		return YES;
	return NO;
}

- (void)pressA
{
#ifndef __BIG_ENDIAN__
	((u16 *)ARM9Mem.ARM9_REG)[0x130>>1] &= 0xFFFE;
	((u16 *)MMU.ARM7_REG)[0x130>>1] &= 0xFFFE;
#else
	((u16 *)ARM9Mem.ARM9_REG)[0x130>>1] &= 0xFEFF;
	((u16 *)MMU.ARM7_REG)[0x130>>1] &= 0xFEFF;
#endif
}

- (void)liftA
{
#ifndef __BIG_ENDIAN__
	((u16 *)ARM9Mem.ARM9_REG)[0x130>>1] |= 0x0001;
	((u16 *)MMU.ARM7_REG)[0x130>>1] |= 0x0001;
#else
	((u16 *)ARM9Mem.ARM9_REG)[0x130>>1] |= 0x0100;
	((u16 *)MMU.ARM7_REG)[0x130>>1] |= 0x0100;
#endif
}

- (BOOL)A
{
#ifndef __BIG_ENDIAN__
	if((((u16 *)ARM9Mem.ARM9_REG)[0x130>>1] & 0x0001) == 0)
#else
	if((((u16 *)ARM9Mem.ARM9_REG)[0x130>>1] & 0x0100) == 0)
#endif
		return YES;
	return NO;
}

- (void)pressB
{
#ifndef __BIG_ENDIAN__
	((u16 *)ARM9Mem.ARM9_REG)[0x130>>1] &= 0xFFFD;
	((u16 *)MMU.ARM7_REG)[0x130>>1] &= 0xFFFD;
#else
	((u16 *)ARM9Mem.ARM9_REG)[0x130>>1] &= 0xFDFF;
	((u16 *)MMU.ARM7_REG)[0x130>>1] &= 0xFDFF;
#endif
}

- (void)liftB
{
#ifndef __BIG_ENDIAN__
	((u16 *)ARM9Mem.ARM9_REG)[0x130>>1] |= 0x0002;
	((u16 *)MMU.ARM7_REG)[0x130>>1] |= 0x0002;
#else
	((u16 *)ARM9Mem.ARM9_REG)[0x130>>1] |= 0x0200;
	((u16 *)MMU.ARM7_REG)[0x130>>1] |= 0x0200;
#endif
}

- (BOOL)B
{
#ifndef __BIG_ENDIAN__
	if((((u16 *)ARM9Mem.ARM9_REG)[0x130>>1] & 0x0002) == 0)
#else
	if((((u16 *)ARM9Mem.ARM9_REG)[0x130>>1] & 0x0200) == 0)
#endif
		return YES;
	return NO;
}

- (void)pressX
{
#ifndef __BIG_ENDIAN__
	((u16 *)MMU.ARM7_REG)[0x136>>1] &= 0xFFFE;
#else
	((u16 *)MMU.ARM7_REG)[0x136>>1] &= 0xFEFF;
#endif
}

- (void)liftX
{
#ifndef __BIG_ENDIAN__
	((u16 *)MMU.ARM7_REG)[0x136>>1] |= 0x0001;
#else
	((u16 *)MMU.ARM7_REG)[0x136>>1] |= 0x0100;
#endif
}

- (BOOL)X
{
#ifndef __BIG_ENDIAN__
	if((((u16 *)MMU.ARM7_REG)[0x136>>1] & 0x0001) == 0)
	if((((u16 *)MMU.ARM7_REG)[0x136>>1] & 0x0001) == 0)
#else
	if((((u16 *)MMU.ARM7_REG)[0x136>>1] & 0x0100) == 0)
	if((((u16 *)MMU.ARM7_REG)[0x136>>1] & 0x0100) == 0)
#endif
		return YES;
	return NO;
}

- (void)pressY
{
#ifndef __BIG_ENDIAN__
	((u16 *)MMU.ARM7_REG)[0x136>>1] &= 0xFFFD;
#else
	((u16 *)MMU.ARM7_REG)[0x136>>1] &= 0xFDFF;
#endif
}

- (void)liftY
{
#ifndef __BIG_ENDIAN__
	((u16 *)MMU.ARM7_REG)[0x136>>1] |= 0x0002;
#else
	((u16 *)MMU.ARM7_REG)[0x136>>1] |= 0x0200;
#endif
}

- (BOOL)Y
{
#ifndef __BIG_ENDIAN__
	if((((u16 *)MMU.ARM7_REG)[0x136>>1] & 0x0002) == 0)
	if((((u16 *)MMU.ARM7_REG)[0x136>>1] & 0x0002) == 0)
#else
	if((((u16 *)MMU.ARM7_REG)[0x136>>1] & 0x0200) == 0)
	if((((u16 *)MMU.ARM7_REG)[0x136>>1] & 0x0200) == 0)
#endif
		return YES;
	return NO;
}

- (void)pressL
{
#ifndef __BIG_ENDIAN__
	((u16 *)ARM9Mem.ARM9_REG)[0x130>>1] &= 0xFDFF;
	((u16 *)MMU.ARM7_REG)[0x130>>1] &= 0xFDFF;
#else
	((u16 *)ARM9Mem.ARM9_REG)[0x130>>1] &= 0xFFFD;
	((u16 *)MMU.ARM7_REG)[0x130>>1] &= 0xFFFD;
#endif
}

- (void)liftL
{
#ifndef __BIG_ENDIAN__
	((u16 *)ARM9Mem.ARM9_REG)[0x130>>1] |= 0x0200;
	((u16 *)MMU.ARM7_REG)[0x130>>1] |= 0x0200;
#else
	((u16 *)ARM9Mem.ARM9_REG)[0x130>>1] |= 0x0002;
	((u16 *)MMU.ARM7_REG)[0x130>>1] |= 0x0002;
#endif
}

- (BOOL)L
{
#ifndef __BIG_ENDIAN__
	if((((u16 *)ARM9Mem.ARM9_REG)[0x130>>1] & 0x0200) == 0)
#else
	if((((u16 *)ARM9Mem.ARM9_REG)[0x130>>1] & 0x0002) == 0)
#endif
		return YES;
	return NO;
}

- (void)pressR
{
#ifndef __BIG_ENDIAN__
	((u16 *)ARM9Mem.ARM9_REG)[0x130>>1] &= 0xFEFF;
	((u16 *)MMU.ARM7_REG)[0x130>>1] &= 0xFEFF;
#else
	((u16 *)ARM9Mem.ARM9_REG)[0x130>>1] &= 0xFFFE;
	((u16 *)MMU.ARM7_REG)[0x130>>1] &= 0xFFFE;
#endif
}

- (void)liftR
{
#ifndef __BIG_ENDIAN__
	((u16 *)ARM9Mem.ARM9_REG)[0x130>>1] |= 0x0100;
	((u16 *)MMU.ARM7_REG)[0x130>>1] |= 0x0100;
#else
	((u16 *)ARM9Mem.ARM9_REG)[0x130>>1] |= 0x0001;
	((u16 *)MMU.ARM7_REG)[0x130>>1] |= 0x0001;
#endif
}

- (BOOL)R
{
#ifndef __BIG_ENDIAN__
	if((((u16 *)MMU.ARM7_REG)[0x130>>1] & 0x0100) == 0)
#else
	if((((u16 *)MMU.ARM7_REG)[0x130>>1] & 0x0001) == 0)
#endif
		return YES;
	return NO;
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

- (BOOL)saveStateToSlot:(int)slot
{
	if(slot >= MAX_SLOTS)return NO;
	if(slot < 0)return NO;

	[execution_lock lock];

	BOOL result = NO;

	savestate_slot(slot + 1);//no execption handling?
	result = YES;

	[execution_lock unlock];

	return result;
}

- (BOOL)loadStateFromSlot:(int)slot
{
	if(slot >= MAX_SLOTS)return NO;
	if(slot < 0)return NO;

	BOOL result = NO;
	[execution_lock lock];

	//Set the GPU context (if it exists) incase the core needs to load anything into opengl during state load
	NSOpenGLContext *prev_context = [NSOpenGLContext currentContext];
	[prev_context retain];
	[context makeCurrentContext];

	loadstate_slot(slot + 1); //no exection handling?
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

- (BOOL)saveStateExists:(int)slot
{
	scan_savestates();
	if(savestates[slot].exists)
		return YES;
	return NO;
}

- (void)toggleTopBackground0
{
	if(SubScreen.gpu->dispBG[0])
		GPU_remove(SubScreen.gpu, 0);
	else
		GPU_addBack(SubScreen.gpu, 0);
}

- (BOOL)showingTopBackground0
{
	return SubScreen.gpu->dispBG[0];
}

- (void)toggleTopBackground1
{
	if(SubScreen.gpu->dispBG[1])
		GPU_remove(SubScreen.gpu, 1);
	else
		GPU_addBack(SubScreen.gpu, 1);
}

- (BOOL)showingTopBackground1
{
	return SubScreen.gpu->dispBG[1];
}

- (void)toggleTopBackground2
{
	if(SubScreen.gpu->dispBG[2])
		GPU_remove(SubScreen.gpu, 2);
	else
		GPU_addBack(SubScreen.gpu, 2);
}

- (BOOL)showingTopBackground2
{
	return SubScreen.gpu->dispBG[2];
}

- (void)toggleTopBackground3
{
	if(SubScreen.gpu->dispBG[3])
		GPU_remove(SubScreen.gpu, 3);
	else
		GPU_addBack(SubScreen.gpu, 3);
}

- (BOOL)showingTopBackground3
{
	return SubScreen.gpu->dispBG[3];
}

- (void)toggleSubBackground0
{
	if(MainScreen.gpu->dispBG[0])
		GPU_remove(MainScreen.gpu, 0);
	else
		GPU_addBack(MainScreen.gpu, 0);
}

- (BOOL)showingSubBackground0
{
	return MainScreen.gpu->dispBG[0];
}

- (void)toggleSubBackground1
{
	if(MainScreen.gpu->dispBG[1])
		GPU_remove(MainScreen.gpu, 1);
	else
		GPU_addBack(MainScreen.gpu, 1);
}

- (BOOL)showingSubBackground1
{
	return MainScreen.gpu->dispBG[1];
}

- (void)toggleSubBackground2
{
	if(MainScreen.gpu->dispBG[2])
		GPU_remove(MainScreen.gpu, 2);
	else
		GPU_addBack(MainScreen.gpu, 2);
}

- (BOOL)showingSubBackground2
{
	return MainScreen.gpu->dispBG[2];
}

- (void)toggleSubBackground3
{
	if(MainScreen.gpu->dispBG[3])
		GPU_remove(MainScreen.gpu, 3);
	else
		GPU_addBack(MainScreen.gpu, 3);
}

- (BOOL)showingSubBackground3
{
	return MainScreen.gpu->dispBG[3];
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

#ifdef HAVE_OPENGL
	[gl_context retain];
	[gl_context makeCurrentContext];
	CGLLockContext((CGLContextObj)[gl_context CGLContextObj]);
#endif

	NSDate *frame_start_date, *frame_end_date, *ideal_frame_end_date;

	int frames_to_skip = 0;

	//program main loop
	while(!finish)
	{
		if(!run)paused = true;

		//run the emulator
		while(run && execute) //run controls when the emulator runs, execute prevents it from continuing execution if there are errors
		{

			paused = false;

			int speed_limit_this_frame = speed_limit; //dont let speed limit change midframe
			if(speed_limit_this_frame)ideal_frame_end_date = [NSDate dateWithTimeIntervalSinceNow: DS_SECONDS_PER_FRAME / ((float)speed_limit_this_frame / 100.0)];

			frame_start_date = [NSDate dateWithTimeIntervalSinceNow:0];

			[execution_lock lock];

			NDS_exec<false>();

			[sound_lock lock];
			int x;
			for(x = 0; x <= frames_to_skip; x++)
			{
				SPU_Emulate_user();
				SPU_Emulate_core();
			}
			[sound_lock unlock];

			[execution_lock unlock];

			frame_end_date = [NSDate dateWithTimeIntervalSinceNow:0];

			//speed limit
			if(speed_limit_this_frame)
				[NSThread sleepUntilDate:ideal_frame_end_date];

			if(frames_to_skip > 0)
				frames_to_skip--;

			else
			{
				if(frame_skip < 0)
				{ //automatic

					//i don't know if theres a standard strategy, but here we calculate how much
					//longer the frame took than it should have, then set it to skip that many frames.
					frames_to_skip = [frame_end_date timeIntervalSinceDate:frame_start_date] / (float)DS_SECONDS_PER_FRAME;
					if(frames_to_skip > 10)frames_to_skip = 10;

				} else
				{

					frames_to_skip = frame_skip;

				}

				//update the screen
				ScreenState *new_screen_data = [[ScreenState alloc] initWithColorData:GPU_screen];

				if(timer_based)
				{ //for tiger compatibility
					[video_update_lock lock];
					[current_screen release];
					current_screen = new_screen_data;
					[video_update_lock unlock];
				} else
				{ //for leopard and later

					//this will generate a warning when compiling on tiger or earlier, but it should
					//be ok since the purpose of the if statement is to check if this will work
					[self performSelector:@selector(videoUpdateHelper:) onThread:gui_thread withObject:new_screen_data waitUntilDone:NO];
					[new_screen_data release]; //performSelector will auto retain the screen data while the other thread displays
				}
			}

			//execute is set to false sometimes by the emulation core
			//when there is an error, if this happens notify the other thread that emulation has stopped.
			if(!execute)
				if(!timer_based) //wont display an error on tiger or earlier
					[error_object performSelector:error_func onThread:gui_thread withObject:nil waitUntilDone:YES];

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

@end
