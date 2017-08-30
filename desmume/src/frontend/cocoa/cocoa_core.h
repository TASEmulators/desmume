/*
	Copyright (C) 2011 Roger Manuel
	Copyright (C) 2011-2017 DeSmuME team

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
#include <libkern/OSAtomic.h>
#include <string>
#import "cocoa_util.h"
#include "ClientExecutionControl.h"


@class CocoaDSCore;
@class CocoaDSController;
@class CocoaDSFirmware;
@class CocoaDSGPU;
@class CocoaDSOutput;

typedef void *gdbstub_handle_t;

typedef struct
{
	CocoaDSCore *cdsCore;
	pthread_mutex_t mutexOutputList;
	pthread_mutex_t mutexThreadExecute;
	pthread_cond_t condThreadExecute;
	pthread_rwlock_t rwlockCoreExecute;
} CoreThreadParam;

@interface CocoaDSCore : NSObject
{
	ClientExecutionControl *execControl;
	
	CocoaDSController *cdsController;
	CocoaDSFirmware *cdsFirmware;
	CocoaDSGPU *cdsGPU;
	NSMutableArray *cdsOutputList;
	
	pthread_t coreThread;
	CoreThreadParam threadParam;
	
	NSTimer *_fpsTimer;
	BOOL _isTimerAtSecond;
	
	BOOL isGdbStubStarted;
	BOOL isInDebugTrap;
	BOOL enableGdbStubARM9;
	BOOL enableGdbStubARM7;
	NSUInteger gdbStubPortARM9;
	NSUInteger gdbStubPortARM7;
	volatile gdbstub_handle_t gdbStubHandleARM9;
	volatile gdbstub_handle_t gdbStubHandleARM7;
	
	NSString *slot1StatusText;
	NSString *frameStatus;
	NSString *executionSpeedStatus;
	
	OSSpinLock spinlockCdsController;
	OSSpinLock spinlockMasterExecute;
}

@property (readonly, nonatomic) ClientExecutionControl *execControl;

@property (retain) CocoaDSController *cdsController;
@property (retain) CocoaDSFirmware *cdsFirmware;
@property (retain) CocoaDSGPU *cdsGPU;
@property (retain) NSMutableArray *cdsOutputList;

@property (assign) BOOL masterExecute;
@property (assign) BOOL isFrameSkipEnabled;
@property (assign) NSInteger coreState;

@property (assign) BOOL isSpeedLimitEnabled;
@property (assign) BOOL isCheatingEnabled;
@property (assign) CGFloat speedScalar;

@property (assign) BOOL isGdbStubStarted;
@property (assign) BOOL isInDebugTrap;
@property (assign) BOOL enableGdbStubARM9;
@property (assign) BOOL enableGdbStubARM7;
@property (assign) NSUInteger gdbStubPortARM9;
@property (assign) NSUInteger gdbStubPortARM7;

@property (assign) BOOL emuFlagAdvancedBusLevelTiming;
@property (assign) BOOL emuFlagRigorousTiming;
@property (assign) BOOL emuFlagUseGameSpecificHacks;
@property (assign) BOOL emuFlagUseExternalBios;
@property (assign) BOOL emuFlagEmulateBiosInterrupts;
@property (assign) BOOL emuFlagPatchDelayLoop;
@property (assign) BOOL emuFlagUseExternalFirmware;
@property (assign) BOOL emuFlagFirmwareBoot;
@property (assign) BOOL emuFlagDebugConsole;
@property (assign) BOOL emuFlagEmulateEnsata;
@property (assign) NSInteger cpuEmulationEngine;
@property (assign) NSInteger maxJITBlockSize;
@property (assign) NSInteger slot1DeviceType;
@property (assign) NSString *slot1StatusText;
@property (assign) NSString *frameStatus;
@property (assign) NSString *executionSpeedStatus;

@property (copy) NSURL *arm9ImageURL;
@property (copy) NSURL *arm7ImageURL;
@property (copy) NSURL *firmwareImageURL;
@property (copy) NSURL *slot1R4URL;

@property (readonly) pthread_rwlock_t *rwlockCoreExecute;

- (BOOL) isSlot1Ejected;
- (void) slot1Eject;

- (void) changeRomSaveType:(NSInteger)saveTypeID;
- (void) updateExecutionSpeedStatus;
- (void) updateSlot1DeviceStatus;

- (void) restoreCoreState;
- (void) reset;
- (void) getTimedEmulatorStatistics:(NSTimer *)timer;
- (NSUInteger) frameNumber;
- (void) frameJumpTo:(NSUInteger)targetFrameNum;
- (void) frameJump:(NSUInteger)relativeFrameNum;

- (void) addOutput:(CocoaDSOutput *)theOutput;
- (void) removeOutput:(CocoaDSOutput *)theOutput;
- (void) removeAllOutputs;

- (NSString *) cpuEmulationEngineString;
- (NSString *) slot1DeviceTypeString;
- (NSString *) slot2DeviceTypeString;

- (BOOL) startReplayRecording:(NSURL *)fileURL sramURL:(NSURL *)sramURL;
- (void) stopReplay;

@end

static void* RunCoreThread(void *arg);
