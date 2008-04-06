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
#import <OpenGL/OpenGL.h>

//DeSmuME general includes
#define OBJ_C
#include "sndOSX.h"
#include "preferences.h"
#include "../NDSSystem.h"
#include "../saves.h"
#include "../render3D.h"
#include "../GPU.h"
#include "../Windows/OGLRender.h"
#undef BOOL

//this bool controls whether we will use a timer to constantly check for screen updates
//or if we can use inter-thread messaging introduced in leopard to update the screen (more efficient)
bool timer_based;

//ds screens are 59.8 frames per sec, so 1/59.8 seconds per frame
//times one million for microseconds per frame
#define DS_MICROSECONDS_PER_FRAME (1.0 / 59.8) * 1000000.0

//accessed from other files
volatile desmume_BOOL execute = true;

GPU3DInterface *core3DList[] = {
&gpu3DNull,
&gpu3Dgl,
NULL
};

SoundInterface_struct *SNDCoreList[] = {
&SNDDummy,
&SNDFile,
&SNDOSX,
NULL
};

static int backupmemorytype=MC_TYPE_AUTODETECT;
static u32 backupmemorysize=1;

struct NDS_fw_config_data firmware;

@implementation NintendoDS
- (id)init;
{
	//
	self = [super init];
	if(self == nil)return nil;
	
	display_object = nil;
	error_object = nil;
	frame_skip = -1; //default to auto frame skip
	gui_thread = [NSThread currentThread];
	current_file = nil;
	flash_file = nil;
	sound_lock = [[NSLock alloc] init];
	current_screen = nil;
	
	//Set the flash file if it's in the prefs/cmd line.  It will be nil if it isn't.
	NSUserDefaults *defaults = [NSUserDefaults standardUserDefaults];
	flash_file = [[defaults stringForKey:PREF_FLASH_FILE] retain];
	if ([flash_file length] > 0) {
		NSLog(@"Using flash file: \"%@\"\n", flash_file);
	} else {
		[flash_file release];
		flash_file = nil;
		NSLog(@"No flash file given\n");
	}
	
	//check if we can sen messages on other threads, which we will use for video update
	timer_based = ([NSObject instancesRespondToSelector:@selector(performSelector:onThread:withObject:waitUntilDone:)]==NO)?true:false;

	//Firmware setup
	NDS_Init();

	//use default firmware
	NDS_FillDefaultFirmwareConfigData(&firmware);
	NDS_CreateDummyFirmware(&firmware);

	//3D Init

	//Create the pixel format for our gpu
	NSOpenGLPixelFormatAttribute attrs[] =
	{
		//NSOpenGLPFAAccelerated,
		//NSOpenGLPFANoRecovery,
		//NSOpenGLPFADoubleBuffer,
		NSOpenGLPFAColorSize, 24,
		NSOpenGLPFAAlphaSize, 8,
		NSOpenGLPFADepthSize, 24,
		NSOpenGLPFAStencilSize, 8,
		NSOpenGLPFAOffScreen,
		0
	};

	pixel_format = [[NSOpenGLPixelFormat alloc] initWithAttributes:attrs];
	if(pixel_format == nil)
	{
		messageDialog(NSLocalizedString(@"Error", nil), @"Couldn't create OpenGL pixel format for GPU");
		return self;
	}

	context = [[NSOpenGLContext alloc] initWithFormat:pixel_format shareContext:nil];
	if(context == nil)
	{
		messageDialog(NSLocalizedString(@"Error", nil), @"Couldn't create OpenGL context for GPU");
		return self;
	}

	//offscreen rendering
	[context setOffScreen:(void*)&gpu_buff width:DS_SCREEN_WIDTH height:DS_SCREEN_HEIGHT rowbytes:DS_SCREEN_WIDTH * 5];
	[context makeCurrentContext];

	NDS_3D_SetDriver(GPU3D_OPENGL);
	if(!gpu3D->NDS_3D_Init())
		messageDialog(NSLocalizedString(@"Error", nil), @"Unable to initialize OpenGL components");

	//Sound Init
	if(SPU_ChangeSoundCore(SNDCORE_OSX, 735 * 4) != 0)
		messageDialog(NSLocalizedString(@"Error", nil), @"Unable to initialize sound core");
	else
		SPU_SetVolume(100);
	
	volume = 100;
	muted = false;

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

	NDS_DeInit();

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
	if(!NDS_LoadROM([filename cStringUsingEncoding:NSASCIIStringEncoding], backupmemorytype, backupmemorysize, flash) > 0)
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
	return NSSTRc(NDS_getROMHeader()->gameTile);
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
	//stop execution
	BOOL old_run = run;
	run = false;
	while (!paused)
	{

	}

	//set the execute variable incase
	//of a previous emulation error
	execute = true;

	NDS_Reset();

	//resume execution
	run = old_run;
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

- (void)touch:(NSPoint)point
{
	NDS_setTouchPos((unsigned short)point.x, (unsigned short)point.y);
}

- (void)releaseTouch
{
	NDS_releasTouch();
}

- (void)pressStart
{
	((u16 *)ARM9Mem.ARM9_REG)[0x130>>1] &= 0xFFF7;
	((u16 *)MMU.ARM7_REG)[0x130>>1] &= 0xFFF7;
}

- (void)liftStart
{
	((u16 *)ARM9Mem.ARM9_REG)[0x130>>1] |= 0x8;
	((u16 *)MMU.ARM7_REG)[0x130>>1] |= 0x8;
}

- (BOOL)start
{
	if((((u16 *)MMU.ARM7_REG)[0x130>>1] & 0x8) == 0)
		return YES;
	return NO;
}

- (void)pressSelect
{
	((u16 *)ARM9Mem.ARM9_REG)[0x130>>1] &= 0xFFFB;
	((u16 *)MMU.ARM7_REG)[0x130>>1] &= 0xFFFB;
}

- (void)liftSelect
{
	((u16 *)ARM9Mem.ARM9_REG)[0x130>>1] |= 0x4;
	((u16 *)MMU.ARM7_REG)[0x130>>1] |= 0x4;
}

- (BOOL)select
{
	if((((u16 *)MMU.ARM7_REG)[0x130>>1] & 0x4) == 0)
		return YES;
	return NO;
}

- (void)pressLeft
{
	((u16 *)ARM9Mem.ARM9_REG)[0x130>>1] &= 0xFFDF;
	((u16 *)MMU.ARM7_REG)[0x130>>1] &= 0xFFDF;
}

- (void)liftLeft
{
	((u16 *)ARM9Mem.ARM9_REG)[0x130>>1] |= 0x20;
	((u16 *)MMU.ARM7_REG)[0x130>>1] |= 0x20;
}

- (BOOL)left
{
	if((((u16 *)ARM9Mem.ARM9_REG)[0x130>>1] & 0x20) == 0)
		return YES;
	return NO;
}

- (void)pressRight
{
	((u16 *)ARM9Mem.ARM9_REG)[0x130>>1] &= 0xFFEF;
	((u16 *)MMU.ARM7_REG)[0x130>>1] &= 0xFFEF;
}

- (void)liftRight
{
	((u16 *)ARM9Mem.ARM9_REG)[0x130>>1] |= 0x10;
	((u16 *)MMU.ARM7_REG)[0x130>>1] |= 0x10;
}

- (BOOL)right
{
	if((((u16*)ARM9Mem.ARM9_REG)[0x130>>1] & 0x10) == 0)
		return YES;
	return NO;
}

- (void)pressUp
{
	((u16 *)ARM9Mem.ARM9_REG)[0x130>>1] &= 0xFFBF;
	((u16 *)MMU.ARM7_REG)[0x130>>1] &= 0xFFBF;
}

- (void)liftUp
{
	((u16 *)ARM9Mem.ARM9_REG)[0x130>>1] |= 0x40;
	((u16 *)MMU.ARM7_REG)[0x130>>1] |= 0x40;
}

- (BOOL)up
{
	if((((u16 *)ARM9Mem.ARM9_REG)[0x130>>1] & 0x40) == 0)
		return YES;
	return NO;
}

- (void)pressDown
{
	((u16 *)ARM9Mem.ARM9_REG)[0x130>>1] &= 0xFF7F;
	((u16 *)MMU.ARM7_REG)[0x130>>1] &= 0xFF7F;
}

- (void)liftDown
{
	((u16 *)ARM9Mem.ARM9_REG)[0x130>>1] |= 0x80;
	((u16 *)MMU.ARM7_REG)[0x130>>1] |= 0x80;
}

- (BOOL)down
{
	if((((u16 *)ARM9Mem.ARM9_REG)[0x130>>1] & 0x80) == 0)
		return YES;
	return NO;
}

- (void)pressA
{
	((u16 *)ARM9Mem.ARM9_REG)[0x130>>1] &= 0xFFFE;
	((u16 *)MMU.ARM7_REG)[0x130>>1] &= 0xFFFE;
}

- (void)liftA
{
	((u16 *)ARM9Mem.ARM9_REG)[0x130>>1] |= 0x1;
	((u16 *)MMU.ARM7_REG)[0x130>>1] |= 0x1;
}

- (BOOL)A
{
	if((((u16 *)ARM9Mem.ARM9_REG)[0x130>>1] & 0x1) == 0)
		return YES;
	return NO;
}

- (void)pressB
{
	((u16 *)ARM9Mem.ARM9_REG)[0x130>>1] &= 0xFFFD;
	((u16 *)MMU.ARM7_REG)[0x130>>1] &= 0xFFFD;
}

- (void)liftB
{
	((u16 *)ARM9Mem.ARM9_REG)[0x130>>1] |= 0x2;
	((u16 *)MMU.ARM7_REG)[0x130>>1] |= 0x2;
}

- (BOOL)B
{
	if((((u16 *)ARM9Mem.ARM9_REG)[0x130>>1] & 0x2) == 0)
		return YES;
	return NO;
}

- (void)pressX
{
	((u16 *)MMU.ARM7_REG)[0x136>>1] &= 0xFFFE;
}

- (void)liftX
{
	((u16 *)MMU.ARM7_REG)[0x136>>1] |= 0x1;
}

- (BOOL)X
{
	if((((u16 *)MMU.ARM7_REG)[0x136>>1] & 0x1) == 0)
		return YES;
	return NO;
}

- (void)pressY
{
	((u16 *)MMU.ARM7_REG)[0x136>>1] &= 0xFFFD;
}

- (void)liftY
{
	((u16 *)MMU.ARM7_REG)[0x136>>1] |= 0x2;
}

- (BOOL)Y
{
	if((((u16 *)MMU.ARM7_REG)[0x136>>1] & 0x2) == 0)
		return YES;
	return NO;
}

- (void)pressL
{
	((u16 *)ARM9Mem.ARM9_REG)[0x130>>1] &= 0xFDFF;
	((u16 *)MMU.ARM7_REG)[0x130>>1] &= 0xFDFF;
}

- (void)liftL
{
	((u16 *)ARM9Mem.ARM9_REG)[0x130>>1] |= 0x200;
	((u16 *)MMU.ARM7_REG)[0x130>>1] |= 0x200;
}

- (BOOL)L
{
	if((((u16 *)ARM9Mem.ARM9_REG)[0x130>>1] & 0x200) == 0)
		return YES;
	return NO;
}

- (void)pressR
{
	((u16 *)ARM9Mem.ARM9_REG)[0x130>>1] &= 0xFEFF;
	((u16 *)MMU.ARM7_REG)[0x130>>1] &= 0xFEFF;
}

- (void)liftR
{
	((u16 *)ARM9Mem.ARM9_REG)[0x130>>1] |= 0x100;
	((u16 *)MMU.ARM7_REG)[0x130>>1] |= 0x100;
}

- (BOOL)R
{
	if((((u16 *)MMU.ARM7_REG)[0x130>>1] & 0x100) == 0)
		return YES;
	return NO;
}

- (BOOL)saveState:(NSString*)file
{
	bool was_executing = !paused;
	[self pause];

	BOOL result = NO;
	if(savestate_save([file cStringUsingEncoding:NSASCIIStringEncoding]))
		result = YES;

	if(was_executing)[self execute];

	return result;
}

- (BOOL)loadState:(NSString*)file
{
	bool was_executing = !paused;
	[self pause];

	BOOL result = NO;
	if(savestate_load([file cStringUsingEncoding:NSASCIIStringEncoding]))
		result = YES;

	if(was_executing)[self execute];

	return result;
}

- (BOOL)saveStateToSlot:(int)slot
{
	if(slot >= MAX_SLOTS)return NO;
	if(slot < 0)return NO;

	bool was_executing = !paused;
	[self pause];

	BOOL result = NO;

	savestate_slot(slot + 1);//no execption handling?
	result = YES;

	if(was_executing)[self execute];

	return result;
}

- (BOOL)loadStateFromSlot:(int)slot
{
	if(slot >= MAX_SLOTS)return NO;
	if(slot < 0)return NO;

	bool was_executing = !paused;
	[self pause];

	BOOL result = NO;

	loadstate_slot(slot + 1); //no exection handling?
	result = YES;

	if(was_executing)[self execute];

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

- (void)setVolume:(int)new_volume
{
	if(new_volume < 0)new_volume = 0;
	if(new_volume > 100)new_volume = 100;
	if(volume == new_volume)return;
	volume = new_volume;
	[sound_lock lock];
	SNDOSXSetVolume(volume);
	[sound_lock unlock];
	muted = false;
}

- (int)volume
{
	return volume;
}

- (void)enableMute
{
	[sound_lock lock];
	SNDOSXMuteAudio();
	[sound_lock unlock];
	muted = true;
}

- (void)disableMute
{
	[sound_lock lock];
	SNDOSXUnMuteAudio();
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

@class NSOpenGLContext;

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

	[gl_context retain];
	[gl_context makeCurrentContext];
	CGLLockContext([gl_context CGLContextObj]);
	
	u32 cycles = 0;

	unsigned long long frame_start_time, frame_end_time;

	int frames_to_skip = 0;

	//program main loop
	while(!finish)
	{
		if(!run)paused = true;

		//run the emulator
		while(run)
		{
			paused = false;

			Microseconds((struct UnsignedWide*)&frame_start_time);

			cycles = NDS_exec((560190<<1)-cycles, FALSE);

			[sound_lock lock];
			SPU_Emulate();
			[sound_lock unlock];

			if(frames_to_skip > 0)
				frames_to_skip--;

			else
			{

				Microseconds((struct UnsignedWide*)&frame_end_time);

				if(frame_skip < 0)
				{ //automatic

					//i don't know if theres a standard strategy, but here we calculate
					//how much longer the frame took than it should have, then set it to skip
					//that many frames.
					frames_to_skip = ((double)(frame_end_time - frame_start_time)) / ((double)DS_MICROSECONDS_PER_FRAME);

				} else
				{

					frames_to_skip = frame_skip;

				}

				//update the screen
				ScreenState *new_screen_data = [[ScreenState alloc] init];
				[new_screen_data setColorData:GPU_screen];
				if(timer_based)
				{
					[video_update_lock lock];
					[current_screen release];
					current_screen = new_screen_data;
					[video_update_lock unlock];
				} else
				{
					//this will generate a warning when compiling on tiger or earlier, but it should
					//be ok since the purpose of the if statement is to check if this will work
					[self performSelector:@selector(videoUpdateHelper:) onThread:gui_thread withObject:new_screen_data waitUntilDone:NO];
					[new_screen_data release]; //performSelector will auto retain the screen data while the other thread displays
				}

			}
/* FIXME
			//execute is set to false sometimes by the emulation core
			//when there is an error, if this happens notify the other
			//thread that emulation has stopped.
			if(!execute)
			{
				run = false; //stop executing
				execute = true; //reset the var, so if someone tries to execute again, we get the error again

				[self performSelector:@selector(pause) onThread:gui_thread withObject:nil
				waitUntilDone:NO modes:[NSArray arrayWithObject:NSDefaultRunLoopMode]];

				[error_object performSelector:error_func onThread:gui_thread withObject:nil
				waitUntilDone:NO modes:[NSArray arrayWithObject:NSDefaultRunLoopMode]];
			}
*/
		}
		
		//when emulation is paused, return CPU time to the OS
		[NSThread sleepUntilDate:[NSDate dateWithTimeIntervalSinceNow:.1]];
	}

	CGLUnlockContext([gl_context CGLContextObj]);
	[gl_context release];
	[autorelease release];

	paused = true;
	finished = true;
}

@end

//////////////////////
// ScreenState
//////////////////////

@implementation ScreenState
- (id)init
{
	self = [super init];
	if(self == nil)return nil;

	rotation = ROTATION_0;

	return self;
}

- (id)initWithScreenState:(ScreenState*)state
{
	self = [super init];
	if(self == nil)return nil;

	rotation = state->rotation;
	memcpy(color_data, state->color_data, DS_SCREEN_WIDTH * DS_SCREEN_HEIGHT*2 * DS_BPP);

	return self;
}

- (NSInteger)width
{
	if(rotation==ROTATION_90 || rotation==ROTATION_270)
		return DS_SCREEN_HEIGHT*2;
	return DS_SCREEN_WIDTH;
}

- (NSInteger)height
{
	if(rotation==ROTATION_90 || rotation==ROTATION_270)
		return DS_SCREEN_WIDTH;
	return DS_SCREEN_HEIGHT*2;
}

- (NSSize)size
{
	NSSize result;

	if(rotation==ROTATION_90 || rotation==ROTATION_270)
	{
		result.width = DS_SCREEN_HEIGHT*2;
		result.height = DS_SCREEN_WIDTH;
	} else
	{
		result.width = DS_SCREEN_WIDTH;
		result.height = DS_SCREEN_HEIGHT*2;
	}

	return result;
}

- (void)fillWithWhite
{
	memset(color_data, 255, DS_SCREEN_WIDTH * DS_SCREEN_HEIGHT*2 * DS_BPP);
}

- (void)fillWithBlack
{
	memset(color_data, 0, DS_SCREEN_WIDTH * DS_SCREEN_HEIGHT*2 * DS_BPP);
}

- (NSBitmapImageRep*)imageRep
{
	NSInteger width = [self width];
	NSInteger height = [self height];

	NSBitmapImageRep *image = [[NSBitmapImageRep alloc] initWithBitmapDataPlanes:NULL
	pixelsWide:width
	pixelsHigh:height
	bitsPerSample:8
	samplesPerPixel:3
	hasAlpha:NO
	isPlanar:NO
	colorSpaceName:NSCalibratedRGBColorSpace
	bytesPerRow:width * 3
	bitsPerPixel:24];

	if(image == nil)return nil;

	u8 *bitmap_data = [image bitmapData];

	const u16 *buffer_16 = (u16*)color_data;

	int i;
	for(i = 0; i < width * height; i++)
	{ //this loop we go through pixel by pixel and convert from 16bit to 24bit for the NSImage
		*(bitmap_data++) = (*buffer_16 & 0x001F) <<  3;
		*(bitmap_data++) = (*buffer_16 & 0x03E0) >> 5 << 3;
		*(bitmap_data++) = (*buffer_16 & 0x7C00) >> 10 << 3;
		buffer_16++;
	}

	[image autorelease];

	return image;
}

- (void)setRotation:(enum ScreenRotation)rot
{
	if(rotation == rot)return;

	//here is where we turn the screen sideways
	//if you can think of a more clever way to do this than making
	//a full copy of the screen data, please implement it!

	const int width = [self width], height = [self height];

	if((rotation==ROTATION_0 && rot==ROTATION_90)
	||(rotation==ROTATION_90 && rot==ROTATION_180)
	||(rotation==ROTATION_180 && rot==ROTATION_270)
	||(rotation==ROTATION_270 && rot==ROTATION_0))
	{ //90 degree clockwise rotation
		unsigned char temp_buffer[width * height * DS_BPP];
		memcpy(temp_buffer, color_data, width * height * DS_BPP);

		int x, y;
		for(x = 0; x< width; x++)
			for(y = 0; y < height; y++)
			{
				color_data[(x * height + (height - y - 1)) * 2] = temp_buffer[(y * width + x) * 2];
				color_data[(x * height + (height - y - 1)) * 2 + 1] = temp_buffer[(y * width + x) * 2 + 1];
			}

		rotation = rot;
	}

	if((rotation==ROTATION_0 && rot==ROTATION_180)
	||(rotation==ROTATION_90 && rot==ROTATION_270)
	||(rotation==ROTATION_180 && rot==ROTATION_0)
	||(rotation==ROTATION_270 && rot==ROTATION_90))
	{ //180 degree rotation
		unsigned char temp_buffer[width * height * DS_BPP];
		memcpy(temp_buffer, color_data, width * height * DS_BPP);

		int x, y;
		for(x = 0; x < width; x++)
			for(y = 0; y < height; y++)
			{
				color_data[(x * height + y) * 2] = temp_buffer[((width - x - 1) * height + (height - y - 1)) * 2];
				color_data[(x * height + y) * 2 + 1] = temp_buffer[((width - x - 1) * height + (height - y - 1)) * 2 + 1];
			}

		rotation = rot;
	}

	if((rotation==ROTATION_0 && rot==ROTATION_270)
	||(rotation==ROTATION_90 && rot==ROTATION_0)
	||(rotation==ROTATION_180 && rot==ROTATION_90)
	||(rotation==ROTATION_270 && rot==ROTATION_180))
	{ //-90 degrees
		unsigned char temp_buffer[width * height * DS_BPP];
		memcpy(temp_buffer, color_data, width * height * DS_BPP);

		int x, y;
		for(x = 0; x< width; x++)
			for(y = 0; y < height; y++)
			{
				color_data[((width - x - 1) * height + y) * 2] = temp_buffer[(y * width + x) * 2];
				color_data[((width - x - 1) * height + y) * 2 + 1] = temp_buffer[(y * width + x) * 2 + 1];
			}

		rotation = rot;
	}

}

- (enum ScreenRotation)rotation
{
	return rotation;
}

- (void)setColorData:(const unsigned char*)data
{
	memcpy(color_data, data, DS_SCREEN_WIDTH * DS_SCREEN_HEIGHT*2 * DS_BPP);
}

- (const unsigned char*)colorData
{
	return color_data;
}
@end
