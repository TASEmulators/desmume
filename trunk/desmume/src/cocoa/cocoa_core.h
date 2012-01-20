/*
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
#include <libkern/OSAtomic.h>
#import "cocoa_util.h"


@class CocoaDSRom;
@class CocoaDSController;
@class CocoaDSFirmware;
@class CocoaDSOutput;

typedef struct
{
	void *cdsCore;
	int state;
	bool isFrameSkipEnabled;
	unsigned int frameCount;
	unsigned int framesToSkip;
	NSTimeInterval calcTimeBudget;
	bool exitThread;
	pthread_mutex_t *mutexCoreExecute;
	pthread_mutex_t mutexThreadExecute;
	pthread_cond_t condThreadExecute;
} CoreThreadParam;

@interface CocoaDSCore : CocoaDSThread
{
	CocoaDSController *cdsController;
	CocoaDSFirmware *cdsFirmware;
	NSMutableArray *cdsOutputList;
	
	pthread_t coreThread;
	CoreThreadParam threadParam;
	
	NSInteger prevCoreState;
	BOOL isSpeedLimitEnabled;
	CGFloat speedScalar;
	
	NSUInteger emulationFlags;
	BOOL emuFlagAdvancedBusLevelTiming;
	BOOL emuFlagUseExternalBios;
	BOOL emuFlagEmulateBiosInterrupts;
	BOOL emuFlagPatchDelayLoop;
	BOOL emuFlagUseExternalFirmware;
	BOOL emuFlagFirmwareBoot;
	BOOL emuFlagDebugConsole;
	BOOL emuFlagEmulateEnsata;
	
	pthread_mutex_t *mutexCoreExecute;
	OSSpinLock spinlockMasterExecute;
	OSSpinLock spinlockExecutionChange;
	OSSpinLock spinlockCheatEnableFlag;
	OSSpinLock spinlockEmulationFlags;
}

@property (assign) CocoaDSController *cdsController;
@property (assign) CocoaDSFirmware *cdsFirmware;
@property (assign) NSMutableArray *cdsOutputList;

@property (assign) BOOL masterExecute;
@property (assign) BOOL isFrameSkipEnabled;
@property (assign) NSInteger coreState;

@property (assign) BOOL isSpeedLimitEnabled;
@property (assign) BOOL isCheatingEnabled;
@property (assign) CGFloat speedScalar;

@property (assign) NSUInteger emulationFlags;
@property (assign) BOOL emuFlagAdvancedBusLevelTiming;
@property (assign) BOOL emuFlagUseExternalBios;
@property (assign) BOOL emuFlagEmulateBiosInterrupts;
@property (assign) BOOL emuFlagPatchDelayLoop;
@property (assign) BOOL emuFlagUseExternalFirmware;
@property (assign) BOOL emuFlagFirmwareBoot;
@property (assign) BOOL emuFlagDebugConsole;
@property (assign) BOOL emuFlagEmulateEnsata;

@property (assign) NSURL *arm9ImageURL;
@property (assign) NSURL *arm7ImageURL;
@property (assign) NSURL *firmwareImageURL;

@property (readonly) pthread_mutex_t *mutexCoreExecute;

+ (BOOL) startupCore;
+ (void) shutdownCore;
+ (BOOL) isCoreStarted;

- (BOOL) ejectCardFlag;
- (void) setEjectCardFlag;
- (void) toggleEjectCard;

- (void) changeRomSaveType:(NSInteger)saveTypeID;
- (void) changeExecutionSpeed;

- (void) restoreCoreState;
- (void) reset;

- (void) addOutput:(CocoaDSOutput *)theOutput;
- (void) removeOutput:(CocoaDSOutput *)theOutput;
- (void) removeAllOutputs;

@end

void* RunCoreThread(void *arg);
void CoreFrameSkip(NSTimeInterval timeBudget, NSTimeInterval timeRemaining, unsigned int *outFramesToSkip);
