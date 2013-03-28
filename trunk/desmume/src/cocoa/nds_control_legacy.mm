/*
	Copyright (C) 2007 Jeff Bland
	Copyright (C) 2011 Roger Manuel
	Copyright (C) 2013 DeSmuME team

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

#import "nds_control_legacy.h"
#import "cocoa_globals.h"
#import "cocoa_file.h"
#import "cocoa_util.h"
#import "cocoa_input_legacy.h"
#import "preferences_legacy.h"
#import "screen_state_legacy.h"
#include "sndOSX.h"

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

//accessed from other files
volatile bool execute = true;

GPU3DInterface *core3DList[] = {
&gpu3DNull,
&gpu3DRasterize,
&gpu3Dgl,
NULL
};

struct NDS_fw_config_data macDS_firmware;

bool OSXOpenGLRendererInit()
{
	return true;
}

@implementation NintendoDS
- (id)init
{
	//
	self = [super init];
	if(self == nil)return nil;

	display_object = nil;
	error_object = nil;
	frame_skip = -1; //default to auto frame skip
	speedScalar = SPEED_SCALAR_NORMAL;
	calcTimeBudget = (NSTimeInterval)(DS_SECONDS_PER_FRAME / speedScalar);
	isSpeedLimitEnabled = YES;
	gui_thread = [NSThread currentThread];
	loadedRomURL = nil;
	execution_lock = [[NSLock alloc] init];
	sound_lock = [[NSLock alloc] init];
	current_screen = nil;
	
	mutexCoreExecute = (pthread_mutex_t *)malloc(sizeof(pthread_mutex_t));
	pthread_mutex_init(mutexCoreExecute, NULL);
	mutexAudioEmulateCore = mutexCoreExecute;
  
#ifdef GDB_STUB
  arm9_gdb_port = 0;
  arm7_gdb_port = 0;
  arm9_gdb_stub = NULL;
  arm7_gdb_stub = NULL;
  struct armcpu_memory_iface *arm9_memio = &arm9_base_memory_iface;
  struct armcpu_memory_iface *arm7_memio = &arm7_base_memory_iface;
  struct armcpu_ctrl_iface *arm9_ctrl_iface;
  struct armcpu_ctrl_iface *arm7_ctrl_iface;
	
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

#ifdef HAVE_JIT
	CommonSettings.use_jit = false;
#endif
	
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
		NSOpenGLPFAAccelerated,
		(NSOpenGLPixelFormatAttribute)0
	};

	if((pixel_format = [[NSOpenGLPixelFormat alloc] initWithAttributes:attrs]) == nil)
	{
		[CocoaDSUtil quickDialogUsingTitle:NSLocalizedString(@"Error", nil) message:NSLocalizedString(@"Couldn't create OpenGL pixel format for 3D rendering", nil)];
		context = nil;

	} else if((context = [[NSOpenGLContext alloc] initWithFormat:pixel_format shareContext:nil]) == nil)
	{
		[pixel_format release];
		pixel_format = nil;
		[CocoaDSUtil quickDialogUsingTitle:NSLocalizedString(@"Error", nil) message:NSLocalizedString(@"Couldn't create OpenGL context for 3D rendering", nil)];
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
				NSString *errorStr = [NSString stringWithFormat:@"Error setting up rgba pixel buffer for 3D rendering (glerror: %d)", error];
				[CocoaDSUtil quickDialogUsingTitle:NSLocalizedString(@"Error", nil) message:NSLocalizedString(errorStr, nil)];
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
		
		oglrender_init = &OSXOpenGLRendererInit;
		NDS_3D_SetDriver(CORE3DLIST_OPENGL);
		//NDS_3D_SetDriver(CORE3DLIST_SWRASTERIZE);
		
		if(!gpu3D->NDS_3D_Init())
			[CocoaDSUtil quickDialogUsingTitle:NSLocalizedString(@"Error", nil) message:NSLocalizedString(@"Unable to initialize OpenGL components", nil)];
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
	volume = 100.0;
#ifdef DESMUME_COCOA
	if(SPU_ChangeSoundCore(SNDCORE_OSX, 735 * 4) != 0)
		[CocoaDSUtil quickDialogUsingTitle:NSLocalizedString(@"Error", nil) message:NSLocalizedString(@"Unable to initialize sound core", nil)];
	else
		SPU_SetVolume((int)volume);
#endif

	//Breakoff a new thread that will execute the ds stuff
	finish = false;
	finished = false;
	run = false;
	paused = false;
	prevCoreState = CORESTATE_PAUSE;
	[NSThread detachNewThreadSelector:@selector(run:) toTarget:self withObject:context];

	//Start a timer to update the screen
	if(timer_based)
	{
		video_update_lock = [[NSLock alloc] init];
		[NSTimer scheduledTimerWithTimeInterval:DS_SECONDS_PER_FRAME target:self selector:@selector(videoUpdateTimerHelper) userInfo:nil repeats:YES];
	}
	
	cdsController = nil;

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
	
	[cdsController release];
	
	[display_object release];
	[error_object release];
	[context release];
	[pixel_format release];
	[loadedRomURL release];
	[sound_lock release];
	[execution_lock release];
	
	pthread_mutex_destroy(mutexCoreExecute);
	free(mutexCoreExecute);

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

- (void) setMasterExecute:(BOOL)theState
{
	//OSSpinLockLock(&spinlockMasterExecute);
	
	if (theState)
	{
		execute = true;
	}
	else
	{
		execute = false;
	}
	
	//OSSpinLockUnlock(&spinlockMasterExecute);
}

- (BOOL) masterExecute
{
	BOOL theState = NO;
	
	//OSSpinLockLock(&spinlockMasterExecute);
	
	if (execute)
	{
		theState = YES;
	}
	
	//OSSpinLockUnlock(&spinlockMasterExecute);
	
	return theState;
}

- (void) setCdsController:(CocoaDSController *)theController
{
	cdsController = theController;
}

- (CocoaDSController*) cdsController
{
	return cdsController;
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

- (BOOL) loadRom:(NSURL *)romURL
{
	BOOL result = NO;
	
	//pause if not already paused
	BOOL was_paused = [self paused];
	[self pause];

	//load the rom
	result = [CocoaDSFile loadRom:romURL];
	if(!result)
	{
		//if it didn't work give an error and dont unpause
		[CocoaDSUtil quickDialogUsingTitle:NSLocalizedString(@"Error", nil) message:NSLocalizedString(@"Could not open file", nil)];

		//continue playing if load didn't work
		if(was_paused == NO)[self execute];

		return result;
	}
/*
	//clear screen data
	if(current_screen != nil)
	{
		[current_screen release];
		current_screen = nil;
	}
*/
	// Retain a copy of the URL of the currently loaded ROM, since we'll be
	// using it later.
	loadedRomURL = romURL;
	[loadedRomURL retain];

	//this is incase emulation stopped from the
	//emulation core somehow
	execute = true;
	
	result = YES;
	
	return result;
}

- (BOOL) isRomLoaded
{
	return (loadedRomURL==nil)?NO:YES;
}

- (void)closeROM
{
	[self pause];
/*
	if(current_screen != nil)
	{
		[current_screen release];
		current_screen = nil;
	}
*/
	NDS_FreeROM();

	[loadedRomURL release];
	loadedRomURL = nil;
}

- (NSImage *) romIcon
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

- (NSString *) romFileName
{
	return [[loadedRomURL path] lastPathComponent];
}

- (NSString *) romTitle
{
	return [[NSString alloc] initWithCString:(NDS_getROMHeader()->gameTile) encoding:NSUTF8StringEncoding];
}

- (NSInteger) romMaker
{
	return NDS_getROMHeader()->makerCode;
}

- (NSInteger) romSize
{
	return NDS_getROMHeader()->cardSize;
}

- (NSInteger) romArm9Size
{
	return NDS_getROMHeader()->ARM9binSize;
}

- (NSInteger) romArm7Size
{
	return NDS_getROMHeader()->ARM7binSize;
}

- (NSInteger) romDataSize
{
	return NDS_getROMHeader()->ARM7binSize + NDS_getROMHeader()->ARM7src;
}

- (NSMutableDictionary *) romInfoBindings
{
	if (![self isRomLoaded])
	{
		return [NintendoDS romNotLoadedBindings];
	}
	
	return [NSMutableDictionary dictionaryWithObjectsAndKeys:
			[self romFileName], @"romFileName",
			[self romTitle], @"romTitle",
			[NSString stringWithFormat:@"%04X", [self romMaker]], @"makerCode",
			[NSString stringWithFormat:NSSTRING_STATUS_SIZE_BYTES, [self romSize]], @"romSize",
			[NSString stringWithFormat:NSSTRING_STATUS_SIZE_BYTES, [self romArm9Size]], @"arm9BinarySize",
			[NSString stringWithFormat:NSSTRING_STATUS_SIZE_BYTES, [self romArm7Size]], @"arm7BinarySize",
			[NSString stringWithFormat:NSSTRING_STATUS_SIZE_BYTES, [self romDataSize]], @"dataSize",
			[self romIcon], @"iconImage",
			nil];
}

+ (NSMutableDictionary *) romNotLoadedBindings
{
	NSImage *iconImage = [[[NSImage alloc] initWithContentsOfFile:[[NSBundle mainBundle] pathForResource:@"AppIcon_DeSmuME" ofType:@"icns"]] autorelease];
	
	return [NSMutableDictionary dictionaryWithObjectsAndKeys:
			NSSTRING_STATUS_NO_ROM_LOADED, @"romFileName",
			NSSTRING_STATUS_NO_ROM_LOADED, @"romTitle",
			NSSTRING_STATUS_NO_ROM_LOADED, @"makerCode",
			[NSString stringWithFormat:NSSTRING_STATUS_SIZE_BYTES, 0], @"romSize",
			[NSString stringWithFormat:NSSTRING_STATUS_SIZE_BYTES, 0], @"arm9BinarySize",
			[NSString stringWithFormat:NSSTRING_STATUS_SIZE_BYTES, 0], @"arm7BinarySize",
			[NSString stringWithFormat:NSSTRING_STATUS_SIZE_BYTES, 0], @"dataSize",
			iconImage, @"iconImage",
			nil];
}

- (NSURL *) loadedRomURL
{
	return loadedRomURL;
}

- (void) setCoreState:(NSInteger)coreState
{
	if ([self paused])
	{
		prevCoreState = CORESTATE_PAUSE;
	}
	else
	{
		prevCoreState = CORESTATE_EXECUTE;
	}
	
	switch (coreState)
	{
		case CORESTATE_EXECUTE:
			[self execute];
			break;
			
		case CORESTATE_PAUSE:
			[self pause];
			break;
			
		default:
			break;
	}
}

- (void) restoreCoreState
{
	switch (prevCoreState)
	{
		case CORESTATE_EXECUTE:
			[self execute];
			break;
			
		case CORESTATE_PAUSE:
			[self pause];
			break;
			
		default:
			break;
	}
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

	pthread_mutex_lock(mutexCoreExecute);
	NDS_Reset();
	pthread_mutex_unlock(mutexCoreExecute);

	//[execution_lock unlock];
	run = old_run;

	//if there was a previous emulation error, clear it, since we reset
	execute = true;
}

- (void) setSpeedScalar:(CGFloat)scalar
{
	speedScalar = scalar;
	[self updateConfig];
}

- (CGFloat) speedScalar
{
	return speedScalar;
}

- (void) setIsSpeedLimitEnabled:(BOOL)theState
{
	isSpeedLimitEnabled = theState;
	[self updateConfig];
}

- (BOOL) isSpeedLimitEnabled
{
	return isSpeedLimitEnabled;
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

- (void) setVolume:(float)vol
{
	if (vol < 0.0f)
	{
		vol = 0.0f;
	}
	else if (vol > MAX_VOLUME)
	{
		vol = MAX_VOLUME;
	}

	volume = vol;
	
	[sound_lock lock];
	SPU_SetVolume((int)vol);
	[sound_lock unlock];
}

- (float) volume
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
	SPU_SetVolume((int)volume);
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

- (void) copyToPasteboard
{
	if (current_screen == nil)
	{
		return;
	}
	
	NSImage *screenshot = [[current_screen image] autorelease];
	if (screenshot == nil)
	{
		return;
	}
	
	NSPasteboard *pboard = [NSPasteboard generalPasteboard];
	[pboard declareTypes:[NSArray arrayWithObjects:NSTIFFPboardType, nil] owner:self];
	[pboard setData:[screenshot TIFFRepresentationUsingCompression:NSTIFFCompressionLZW factor:1.0f] forType:NSTIFFPboardType];
}

- (NSBitmapImageRep *) bitmapImageRep
{
	NSBitmapImageRep *currentScreenImageRep = nil;
	
	if (current_screen == nil)
	{
		return currentScreenImageRep;
	}
	
	currentScreenImageRep = [current_screen imageRep];
	
	return currentScreenImageRep;
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
	if(!run || current_screen == nil)
	{
		return;
	}

	[display_object performSelector:display_func withObject:current_screen];
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
			[cdsController setupAllDSInputs];
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
	
	pthread_mutex_lock(mutexCoreExecute);
	NDS_exec<false>();
	pthread_mutex_unlock(mutexCoreExecute);
	
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
		ScreenState *oldScreenData = current_screen;
		current_screen = new_screen_data;
		[oldScreenData release];
		[video_update_lock unlock];
	}
	else
	{ //for leopard and later
		ScreenState *oldScreenData = current_screen;
		current_screen = new_screen_data;
		
		//this will generate a warning when compiling on tiger or earlier, but it should
		//be ok since the purpose of the if statement is to check if this will work
		[self performSelector:@selector(videoUpdateHelper:) onThread:gui_thread withObject:current_screen waitUntilDone:NO];
		[oldScreenData release]; //performSelector will auto retain the screen data while the other thread displays
	}
}

- (void) padTime:(NSTimeInterval)timePad
{
#if MAC_OS_X_VERSION_MIN_REQUIRED <= MAC_OS_X_VERSION_10_4 // Code for Mac OS X 10.4 and earlier
	
	[NSThread sleepUntilDate:[NSDate dateWithTimeIntervalSinceNow:timePad]];
	
#else // Code for Mac OS X 10.5 and later
	
	[NSThread sleepForTimeInterval:timePad];
	
#endif
}

- (void) updateConfig
{
	CGFloat newTimeBudget;
	
	// Update speed limit
	if(!isSpeedLimitEnabled)
	{
		newTimeBudget = 0.0;
	}
	else
	{
		CGFloat theSpeed = speedScalar;
		if(theSpeed <= SPEED_SCALAR_MIN)
		{
			theSpeed = SPEED_SCALAR_MIN;
		}
		
		newTimeBudget = (NSTimeInterval)(DS_SECONDS_PER_FRAME / theSpeed);
	}
	
	calcTimeBudget = newTimeBudget;
}

@end
