/*
	Copyright (C) 2011 Roger Manuel
	Copyright (C) 2011-2015 DeSmuME team

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


@class CocoaDSController;
@class CocoaDSFirmware;
@class CocoaDSGPU;
@class CocoaDSOutput;

typedef void *gdbstub_handle_t;

typedef struct
{
	void *cdsCore;
	void *cdsController;
	int state;
	bool isFrameSkipEnabled;
	size_t frameCount;
	NSUInteger frameJumpTarget;
	int framesToSkip;
	uint64_t timeBudgetMachAbsTime;
	bool exitThread;
	pthread_mutex_t mutexOutputList;
	pthread_mutex_t mutexThreadExecute;
	pthread_cond_t condThreadExecute;
	pthread_rwlock_t rwlockCoreExecute;
} CoreThreadParam;

@interface CocoaDSCore : NSObject
{
	CocoaDSController *cdsController;
	CocoaDSFirmware *cdsFirmware;
	CocoaDSGPU *cdsGPU;
	NSMutableArray *cdsOutputList;
	
	pthread_t coreThread;
	CoreThreadParam threadParam;
	
	NSInteger prevCoreState;
	BOOL isSpeedLimitEnabled;
	CGFloat speedScalar;
	std::string _slot1R4Path;
	
	BOOL isGdbStubStarted;
	BOOL isInDebugTrap;
	BOOL enableGdbStubARM9;
	BOOL enableGdbStubARM7;
	NSUInteger gdbStubPortARM9;
	NSUInteger gdbStubPortARM7;
	volatile gdbstub_handle_t gdbStubHandleARM9;
	volatile gdbstub_handle_t gdbStubHandleARM7;
	
	NSUInteger emulationFlags;
	BOOL emuFlagAdvancedBusLevelTiming;
	BOOL emuFlagRigorousTiming;
	BOOL emuFlagUseExternalBios;
	BOOL emuFlagEmulateBiosInterrupts;
	BOOL emuFlagPatchDelayLoop;
	BOOL emuFlagUseExternalFirmware;
	BOOL emuFlagFirmwareBoot;
	BOOL emuFlagDebugConsole;
	BOOL emuFlagEmulateEnsata;
	NSInteger cpuEmulationEngine;
	NSInteger slot1DeviceType;
	NSString *slot1StatusText;
	NSString *frameStatus;
	NSString *executionSpeedStatus;
	
	NSURL *slot1R4URL;
	
	OSSpinLock spinlockCdsController;
	OSSpinLock spinlockMasterExecute;
	OSSpinLock spinlockExecutionChange;
	OSSpinLock spinlockCheatEnableFlag;
	OSSpinLock spinlockEmulationFlags;
	OSSpinLock spinlockCPUEmulationEngine;
}

@property (retain) CocoaDSController *cdsController;
@property (retain) CocoaDSFirmware *cdsFirmware;
@property (retain) CocoaDSGPU *cdsGPU;
@property (assign) NSMutableArray *cdsOutputList;

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

@property (assign) NSUInteger emulationFlags;
@property (assign) BOOL emuFlagAdvancedBusLevelTiming;
@property (assign) BOOL emuFlagRigorousTiming;
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
@property (retain) NSURL *slot1R4URL;

@property (readonly) pthread_rwlock_t *rwlockCoreExecute;

- (BOOL) ejectCardFlag;
- (void) setEjectCardFlag;
- (void) slot1Eject;

- (void) changeRomSaveType:(NSInteger)saveTypeID;
- (void) changeExecutionSpeed;
- (void) applyDynaRec;
- (BOOL) applySlot1Device;

- (void) restoreCoreState;
- (void) reset;
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
static int CalculateFrameSkip(uint64_t timeBudgetMachAbsTime, uint64_t frameStartMachAbsTime);
uint64_t GetFrameAbsoluteTime(const double frameTimeScalar);
