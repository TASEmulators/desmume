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
#include <pthread.h>


#if MAC_OS_X_VERSION_MIN_REQUIRED <= MAC_OS_X_VERSION_10_4
	#include "macosx_10_4_compat.h"
#endif

#ifdef GDB_STUB
#define OBJ_C
#include "../gdbstub.h"
#endif

#define MAX_SLOTS 10

@class CocoaDSController;
@class ScreenState;

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
	pthread_mutex_t *mutexCoreExecute;
	
	ScreenState * volatile current_screen;
	NSLock *video_update_lock;

	volatile bool finish; //set to true to make the other thread finish
	volatile bool finished; //set to true by the other thread after it finishes
	volatile bool run; //set to control execution in other thread
	volatile bool paused; //sey by other thread to let us know if its executing
	
	bool muted;
	float volume;

	volatile int frame_skip;
	volatile int speed_limit;
	volatile int save_type;
	CGFloat speedScalar;
	BOOL isSpeedLimitEnabled;
	NSTimeInterval calcTimeBudget;
	CocoaDSController *cdsController;
  
#ifdef GDB_STUB
  NSInteger arm9_gdb_port;
  NSInteger arm7_gdb_port;
  gdbstub_handle_t arm9_gdb_stub;
  gdbstub_handle_t arm7_gdb_stub;
#endif

	unsigned char gpu_buff[256 * 256 * 5]; //this is where the 3D rendering of the NDS is stored
	
	@public
	NSURL *loadedRomURL;
	NSInteger prevCoreState;
}

//Instanciating, setup, and deconstruction
- (id)init;
- (void)setVideoUpdateCallback:(SEL)callback withObject:(id)object; //this callback should take one ScreenState(below) parameter
- (void)setErrorCallback:(SEL)callback withObject:(id)object;
- (void)dealloc;

// Data accessors
- (void) setMasterExecute:(BOOL)theState;
- (BOOL) masterExecute;
- (void) setCdsController:(CocoaDSController *)theController;
- (CocoaDSController*) cdsController;

//Firmware control
- (void)setPlayerName:(NSString*)player_name;

//ROM control
- (BOOL) loadRom:(NSURL *)romURL;
- (BOOL)isRomLoaded;
- (void)closeROM;

//ROM Info
- (NSImage *)romIcon;
- (NSString *) romFileName;
- (NSString *)romTitle;
- (NSInteger)romMaker;
- (NSInteger)romSize;
- (NSInteger)romArm9Size;
- (NSInteger)romArm7Size;
- (NSInteger)romDataSize;
- (NSURL *) loadedRomURL;
- (NSMutableDictionary *) romInfoBindings;
+ (NSMutableDictionary *) romNotLoadedBindings;

//execution control
- (void) setCoreState:(NSInteger)coreState;
- (void) restoreCoreState;
- (BOOL)executing;
- (void)execute;
- (BOOL)paused;
- (void)pause;
- (void)reset;
- (void) setSpeedScalar:(CGFloat)scalar;
- (CGFloat) speedScalar;
- (void) setIsSpeedLimitEnabled:(BOOL)theState;
- (BOOL) isSpeedLimitEnabled;
- (void)setSaveType:(int)savetype; // see save_types in src/mmu.h
- (int)saveType; // default is 0, which is autodetect
- (void) updateConfig;
- (void) emulateDS;
- (void) drawFrame;
- (void) padTime:(NSTimeInterval)timePad;

- (BOOL) isSubScreenLayerDisplayed:(int)i;
- (BOOL) isMainScreenLayerDisplayed:(int)i;
- (void) toggleMainScreenLayer:(int)i;
- (void) toggleSubScreenLayer:(int)i;

//Sound
- (BOOL)hasSound;
- (void) setVolume:(float)vol; //clamped: 0 to 100
- (float) volume;
- (void)enableMute;
- (void)disableMute;
- (void)toggleMute;
- (BOOL)muted;

- (void) copyToPasteboard;
- (NSBitmapImageRep *) bitmapImageRep;

@end

bool OSXOpenGLRendererInit();
