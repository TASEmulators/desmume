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

#import "globals.h"

@class ScreenState;

#ifdef GDB_STUB
#define OBJ_C
#include "../gdbstub.h"
#endif

#define MAX_SLOTS 10
#define MAX_FRAME_SKIP 10

//This class is a compelte objective-c wrapper for
//the core emulation features, other objective-c code inherit
//upon or instanciate this to add interfaces for these features
//Please only instanciate once.
@interface NintendoDS : NSObject
{
	@private

	NSOpenGLContext* context; //context where the 3d gets renderered to
	NSOpenGLPixelFormat* pixel_format; //pixel format for opengl 3d renderer
	NSThread *gui_thread;
	SEL display_func; //the function id that gets called when the screen is ready to update
	id display_object; //the object that the above function is called on
	SEL error_func;
	id error_object;
	
	NSLock *execution_lock;
	NSLock *sound_lock;
	
	ScreenState * volatile current_screen;
	NSLock *video_update_lock;

	volatile bool finish; //set to true to make the other thread finish
	volatile bool finished; //set to true by the other thread after it finishes
	volatile bool run; //set to control execution in other thread
	volatile bool paused; //sey by other thread to let us know if its executing
	
	bool muted;
	int volume;

	volatile int frame_skip;
	volatile int speed_limit;
	volatile int save_type;

	NSString *current_file;
	NSString *flash_file;
  
#ifdef GDB_STUB
  NSInteger arm9_gdb_port;
  NSInteger arm7_gdb_port;
  gdbstub_handle_t arm9_gdb_stub;
  gdbstub_handle_t arm7_gdb_stub;
#endif

	unsigned char gpu_buff[256 * 256 * 5]; //this is where the 3D rendering of the NDS is stored
}

//Instanciating, setup, and deconstruction
- (id)init;
- (void)setVideoUpdateCallback:(SEL)callback withObject:(id)object; //this callback should take one ScreenState(below) parameter
- (void)setErrorCallback:(SEL)callback withObject:(id)object;
- (void)dealloc;

//Firmware control
- (void)setPlayerName:(NSString*)player_name;

//ROM control
- (BOOL)loadROM:(NSString*)filename;
- (BOOL)ROMLoaded;
- (void)closeROM;

//ROM Info
- (NSImage*)ROMIcon;
- (NSString*)ROMFile;
- (NSString*)ROMTitle;
- (NSInteger)ROMMaker;
- (NSInteger)ROMSize;
- (NSInteger)ROMARM9Size;
- (NSInteger)ROMARM7Size;
- (NSInteger)ROMDataSize;

//Flash memory
- (NSString*)flashFile;
- (void)setFlashFile:(NSString*)filename;

//execution control
- (BOOL)executing;
- (void)execute;
- (BOOL)paused;
- (void)pause;
- (void)reset;
- (void)setFrameSkip:(int)frameskip; //negative is auto, 0 is off, more than 0 is the amount of frames to skip before showing a frame
- (int)frameSkip; //defaults to -1
- (void)setSpeedLimit:(int)percent; //0 is off, 1-1000 is the pertance speed it runs at, anything else does nothing
- (int)speedLimit;
- (void)setSaveType:(int)savetype; // see save_types in src/mmu.h
- (int)saveType; // default is 0, which is autodetect

//touch screen
- (void)touch:(NSPoint)point;
- (void)releaseTouch;

//button input
- (void)pressStart;
- (void)liftStart;
- (BOOL)start;
- (void)pressSelect;
- (void)liftSelect;
- (BOOL)select;
- (void)pressLeft;
- (void)liftLeft;
- (BOOL)left;
- (void)pressRight;
- (void)liftRight;
- (BOOL)right;
- (void)pressUp;
- (void)liftUp;
- (BOOL)up;
- (void)pressDown;
- (void)liftDown;
- (BOOL)down;
- (void)pressA;
- (void)liftA;
- (BOOL)A;
- (void)pressB;
- (void)liftB;
- (BOOL)B;
- (void)pressX;
- (void)liftX;
- (BOOL)X;
- (void)pressY;
- (void)liftY;
- (BOOL)Y;
- (void)pressL;
- (void)liftL;
- (BOOL)L;
- (void)pressR;
- (void)liftR;
- (BOOL)R;

//save states
- (BOOL)saveState:(NSString*)file;
- (BOOL)loadState:(NSString*)file;
- (BOOL)saveStateToSlot:(int)slot; //0 to MAX_SLOTS-1, anything else is ignored
- (BOOL)loadStateFromSlot:(int)slot;
- (BOOL)saveStateExists:(int)slot;

//layers
- (void)toggleTopBackground0;
- (BOOL)showingTopBackground0;
- (void)toggleTopBackground1;
- (BOOL)showingTopBackground1;
- (void)toggleTopBackground2;
- (BOOL)showingTopBackground2;
- (void)toggleTopBackground3;
- (BOOL)showingTopBackground3;
- (void)toggleSubBackground0;
- (BOOL)showingSubBackground0;
- (void)toggleSubBackground1;
- (BOOL)showingSubBackground1;
- (void)toggleSubBackground2;
- (BOOL)showingSubBackground2;
- (void)toggleSubBackground3;
- (BOOL)showingSubBackground3;

//Sound
- (BOOL)hasSound;
- (void)setVolume:(int)volume; //clamped: 0 to 100
- (int)volume;
- (void)enableMute;
- (void)disableMute;
- (void)toggleMute;
- (BOOL)muted;
@end
