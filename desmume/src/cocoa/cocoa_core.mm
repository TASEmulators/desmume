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

#import "cocoa_core.h"
#import "cocoa_input.h"
#import "cocoa_firmware.h"
#import "cocoa_globals.h"
#import "cocoa_output.h"
#import "cocoa_rom.h"
#import "cocoa_util.h"

#include "../NDSSystem.h"
#undef BOOL


//accessed from other files
volatile bool execute = true;

@implementation CocoaDSCore

@synthesize cdsController;
@synthesize cdsFirmware;
@synthesize cdsOutputList;

@dynamic masterExecute;
@dynamic isFrameSkipEnabled;
@dynamic coreState;
@dynamic isSpeedLimitEnabled;
@dynamic isCheatingEnabled;
@dynamic speedScalar;

@dynamic emulationFlags;
@synthesize emuFlagAdvancedBusLevelTiming;
@synthesize emuFlagUseExternalBios;
@synthesize emuFlagEmulateBiosInterrupts;
@synthesize emuFlagPatchDelayLoop;
@synthesize emuFlagUseExternalFirmware;
@synthesize emuFlagFirmwareBoot;
@synthesize emuFlagDebugConsole;
@synthesize emuFlagEmulateEnsata;

@dynamic arm9ImageURL;
@dynamic arm7ImageURL;
@dynamic firmwareImageURL;

@synthesize mutexCoreExecute;

static BOOL isCoreStarted = NO;

- (id)init
{
	self = [super init];
	if(self == nil)
	{
		return self;
	}
	
	cdsController = nil;
	cdsFirmware = nil;
	cdsOutputList = [[NSMutableArray alloc] initWithCapacity:32];
	
	emulationFlags = EMULATION_ADVANCED_BUS_LEVEL_TIMING_MASK;
	emuFlagAdvancedBusLevelTiming = YES;
	emuFlagUseExternalBios = NO;
	emuFlagEmulateBiosInterrupts = NO;
	emuFlagPatchDelayLoop = NO;
	emuFlagUseExternalFirmware = NO;
	emuFlagFirmwareBoot = NO;
	emuFlagDebugConsole = NO;
	emuFlagEmulateEnsata = NO;
	
	mutexCoreExecute = (pthread_mutex_t *)malloc(sizeof(pthread_mutex_t));
	pthread_mutex_init(mutexCoreExecute, NULL);
	spinlockMasterExecute = OS_SPINLOCK_INIT;
	spinlockExecutionChange = OS_SPINLOCK_INIT;
	spinlockCheatEnableFlag = OS_SPINLOCK_INIT;
	spinlockEmulationFlags = OS_SPINLOCK_INIT;
	
	isSpeedLimitEnabled = YES;
	speedScalar = SPEED_SCALAR_NORMAL;
	prevCoreState = CORESTATE_PAUSE;
	
	threadParam.cdsCore = self;
	threadParam.state = CORESTATE_PAUSE;
	threadParam.isFrameSkipEnabled = true;
	threadParam.frameCount = 0;
	threadParam.framesToSkip = 0;
	threadParam.calcTimeBudget = (NSTimeInterval)(DS_SECONDS_PER_FRAME / speedScalar);
	threadParam.exitThread = false;
	threadParam.mutexCoreExecute = mutexCoreExecute;
	pthread_mutex_init(&threadParam.mutexThreadExecute, NULL);
	pthread_cond_init(&threadParam.condThreadExecute, NULL);
	pthread_create(&coreThread, NULL, &RunCoreThread, &threadParam);
	
	return self;
}

- (void)dealloc
{
	[self setCoreState:CORESTATE_PAUSE];
	
	// Exit the thread.
	if (self.thread != nil)
	{
		self.threadExit = YES;
		
		// Wait until the thread has shut down.
		while (self.thread != nil)
		{
			[NSThread sleepUntilDate:[NSDate dateWithTimeIntervalSinceNow:0.01]];
		}
	}
	
	pthread_mutex_lock(&threadParam.mutexThreadExecute);
	threadParam.exitThread = true;
	pthread_cond_signal(&threadParam.condThreadExecute);
	pthread_mutex_unlock(&threadParam.mutexThreadExecute);
	
	pthread_join(coreThread, NULL);
	coreThread = nil;
	pthread_mutex_destroy(&threadParam.mutexThreadExecute);
	pthread_cond_destroy(&threadParam.condThreadExecute);
	
	pthread_mutex_destroy(self.mutexCoreExecute);
	free(self.mutexCoreExecute);
	mutexCoreExecute = nil;
	
	[super dealloc];
}

+ (BOOL) startupCore
{
	NSInteger result = -1;
	
	if (isCoreStarted)
	{
		return isCoreStarted;
	}
	
	result = NDS_Init();
	if (result == -1)
	{
		isCoreStarted = NO;
		return isCoreStarted;
	}
	
	isCoreStarted = YES;
	
	return isCoreStarted;
}

+ (void) shutdownCore
{
	if (isCoreStarted)
	{
		NDS_DeInit();
		isCoreStarted = NO;
	}
}

+ (BOOL) isCoreStarted
{
	return isCoreStarted;
}

- (void) setMasterExecute:(BOOL)theState
{
	OSSpinLockLock(&spinlockMasterExecute);
	
	if (theState)
	{
		execute = true;
	}
	else
	{
		execute = false;
	}
	
	OSSpinLockUnlock(&spinlockMasterExecute);
}

- (BOOL) masterExecute
{
	BOOL theState = NO;
	
	OSSpinLockLock(&spinlockMasterExecute);
	
	if (execute)
	{
		theState = YES;
	}
		
	OSSpinLockUnlock(&spinlockMasterExecute);
	
	return theState;
}

- (void) setIsFrameSkipEnabled:(BOOL)theState
{
	pthread_mutex_lock(&threadParam.mutexThreadExecute);
	
	if (theState)
	{
		threadParam.isFrameSkipEnabled = true;
	}
	else
	{
		threadParam.isFrameSkipEnabled = false;
	}
	
	pthread_mutex_unlock(&threadParam.mutexThreadExecute);
}

- (BOOL) isFrameSkipEnabled
{
	BOOL theState = NO;
	
	pthread_mutex_lock(&threadParam.mutexThreadExecute);
	bool cState = threadParam.isFrameSkipEnabled;
	pthread_mutex_unlock(&threadParam.mutexThreadExecute);
	
	if (cState)
	{
		theState = YES;
	}
	
	return theState;
}

- (void) setSpeedScalar:(CGFloat)scalar
{
	OSSpinLockLock(&spinlockExecutionChange);
	speedScalar = scalar;
	OSSpinLockUnlock(&spinlockExecutionChange);
	[self changeExecutionSpeed];
}

- (CGFloat) speedScalar
{
	OSSpinLockLock(&spinlockExecutionChange);
	CGFloat scalar = speedScalar;
	OSSpinLockUnlock(&spinlockExecutionChange);
	
	return scalar;
}

- (void) setIsSpeedLimitEnabled:(BOOL)theState
{
	OSSpinLockLock(&spinlockExecutionChange);
	isSpeedLimitEnabled = theState;
	OSSpinLockUnlock(&spinlockExecutionChange);
	[self changeExecutionSpeed];
}

- (BOOL) isSpeedLimitEnabled
{
	OSSpinLockLock(&spinlockExecutionChange);
	BOOL enabled = isSpeedLimitEnabled;
	OSSpinLockUnlock(&spinlockExecutionChange);
	
	return enabled;
}

- (void) setIsCheatingEnabled:(BOOL)theState
{
	OSSpinLockLock(&spinlockCheatEnableFlag);
	
	if (theState)
	{
		CommonSettings.cheatsDisable = false;
	}
	else
	{
		CommonSettings.cheatsDisable = true;
	}
	
	OSSpinLockUnlock(&spinlockCheatEnableFlag);
}

- (BOOL) isCheatingEnabled
{
	BOOL theState = YES;
	
	OSSpinLockLock(&spinlockCheatEnableFlag);
	
	if (CommonSettings.cheatsDisable)
	{
		theState = NO;
	}
	
	OSSpinLockUnlock(&spinlockCheatEnableFlag);
	
	return theState;
}

- (void) setEmulationFlags:(NSUInteger)theFlags
{
	OSSpinLockLock(&spinlockEmulationFlags);
	emulationFlags = theFlags;
	OSSpinLockUnlock(&spinlockEmulationFlags);
	
	pthread_mutex_lock(self.mutexCoreExecute);
	
	if (theFlags & EMULATION_ADVANCED_BUS_LEVEL_TIMING_MASK)
	{
		self.emuFlagAdvancedBusLevelTiming = YES;
		CommonSettings.advanced_timing = true;
	}
	else
	{
		self.emuFlagAdvancedBusLevelTiming = NO;
		CommonSettings.advanced_timing = false;
	}
	
	if (theFlags & EMULATION_ENSATA_MASK)
	{
		self.emuFlagEmulateEnsata = YES;
		CommonSettings.EnsataEmulation = true;
	}
	else
	{
		self.emuFlagEmulateEnsata = NO;
		CommonSettings.EnsataEmulation = false;
	}
	
	if (theFlags & EMULATION_USE_EXTERNAL_BIOS_MASK)
	{
		self.emuFlagUseExternalBios = YES;
		CommonSettings.UseExtBIOS = true;
	}
	else
	{
		self.emuFlagUseExternalBios = NO;
		CommonSettings.UseExtBIOS = false;
	}
	
	if (theFlags & EMULATION_BIOS_SWI_MASK)
	{
		self.emuFlagEmulateBiosInterrupts = YES;
		CommonSettings.SWIFromBIOS = true;
	}
	else
	{
		self.emuFlagEmulateBiosInterrupts = NO;
		CommonSettings.SWIFromBIOS = false;
	}
	
	if (theFlags & EMULATION_PATCH_DELAY_LOOP_MASK)
	{
		self.emuFlagPatchDelayLoop = YES;
		CommonSettings.PatchSWI3 = true;
	}
	else
	{
		self.emuFlagPatchDelayLoop = NO;
		CommonSettings.PatchSWI3 = false;
	}
	
	if (theFlags & EMULATION_USE_EXTERNAL_FIRMWARE_MASK)
	{
		self.emuFlagUseExternalFirmware = YES;
		CommonSettings.UseExtFirmware = true;
	}
	else
	{
		self.emuFlagUseExternalFirmware = NO;
		CommonSettings.UseExtFirmware = false;
	}
	
	if (theFlags & EMULATION_BOOT_FROM_FIRMWARE_MASK)
	{
		self.emuFlagFirmwareBoot = YES;
		CommonSettings.BootFromFirmware = true;
	}
	else
	{
		self.emuFlagFirmwareBoot = NO;
		CommonSettings.BootFromFirmware = false;
	}
	
	if (theFlags & EMULATION_DEBUG_CONSOLE_MASK)
	{
		self.emuFlagDebugConsole = YES;
		CommonSettings.DebugConsole = true;
	}
	else
	{
		self.emuFlagDebugConsole = NO;
		CommonSettings.DebugConsole = false;
	}
	
	pthread_mutex_unlock(self.mutexCoreExecute);
}

- (NSUInteger) emulationFlags
{
	OSSpinLockLock(&spinlockEmulationFlags);
	NSUInteger theFlags = emulationFlags;
	OSSpinLockUnlock(&spinlockEmulationFlags);
	
	return theFlags;
}

- (void) setCoreState:(NSInteger)coreState
{
	if (threadParam.state == CORESTATE_PAUSE)
	{
		prevCoreState = CORESTATE_PAUSE;
	}
	else
	{
		prevCoreState = CORESTATE_EXECUTE;
	}
	
	pthread_mutex_lock(&threadParam.mutexThreadExecute);
	threadParam.state = coreState;
	pthread_cond_signal(&threadParam.condThreadExecute);
	pthread_mutex_unlock(&threadParam.mutexThreadExecute);
}

- (NSInteger) coreState
{
	pthread_mutex_lock(&threadParam.mutexThreadExecute);
	NSInteger theState = threadParam.state;
	pthread_mutex_unlock(&threadParam.mutexThreadExecute);
	
	return theState;
}

- (void) setArm9ImageURL:(NSURL *)fileURL
{
	if (fileURL != nil)
	{
		strlcpy(CommonSettings.ARM9BIOS, [[fileURL path] cStringUsingEncoding:NSUTF8StringEncoding], sizeof(CommonSettings.ARM9BIOS));
	}
	else
	{
		memset(CommonSettings.ARM9BIOS, 0, sizeof(CommonSettings.ARM9BIOS));
	}
}

- (NSURL *) arm9ImageURL
{
	return [NSURL fileURLWithPath:[NSString stringWithCString:CommonSettings.ARM9BIOS encoding:NSUTF8StringEncoding]];
}

- (void) setArm7ImageURL:(NSURL *)fileURL
{
	if (fileURL != nil)
	{
		strlcpy(CommonSettings.ARM7BIOS, [[fileURL path] cStringUsingEncoding:NSUTF8StringEncoding], sizeof(CommonSettings.ARM7BIOS));
	}
	else
	{
		memset(CommonSettings.ARM7BIOS, 0, sizeof(CommonSettings.ARM7BIOS));
	}
}

- (NSURL *) arm7ImageURL
{
	return [NSURL fileURLWithPath:[NSString stringWithCString:CommonSettings.ARM7BIOS encoding:NSUTF8StringEncoding]];
}

- (void) setFirmwareImageURL:(NSURL *)fileURL
{
	if (fileURL != nil)
	{
		strlcpy(CommonSettings.Firmware, [[fileURL path] cStringUsingEncoding:NSUTF8StringEncoding], sizeof(CommonSettings.Firmware));
	}
	else
	{
		memset(CommonSettings.Firmware, 0, sizeof(CommonSettings.Firmware));
	}
}

- (NSURL *) firmwareImageURL
{
	return [NSURL fileURLWithPath:[NSString stringWithCString:CommonSettings.Firmware encoding:NSUTF8StringEncoding]];
}

- (void) setEjectCardFlag
{
	if (nds.cardEjected)
	{
		self.emulationFlags = self.emulationFlags | EMULATION_CARD_EJECT_MASK;
		return;
	}
	
	self.emulationFlags = self.emulationFlags & ~EMULATION_CARD_EJECT_MASK;
}

- (BOOL) ejectCardFlag
{
	[self setEjectCardFlag];
	if (nds.cardEjected)
	{
		return YES;
	}
	
	return NO;
}

- (void) toggleEjectCard
{
	NDS_ToggleCardEject();
	[self setEjectCardFlag];
}

- (void) changeRomSaveType:(NSInteger)saveTypeID
{
	pthread_mutex_lock(self.mutexCoreExecute);
	[CocoaDSRom changeRomSaveType:saveTypeID];
	pthread_mutex_unlock(self.mutexCoreExecute);
}

- (void) changeExecutionSpeed
{
	if (self.isSpeedLimitEnabled)
	{
		CGFloat theSpeed = self.speedScalar;
		if(theSpeed <= SPEED_SCALAR_MIN)
		{
			theSpeed = SPEED_SCALAR_MIN;
		}
		
		pthread_mutex_unlock(&threadParam.mutexThreadExecute);
		threadParam.calcTimeBudget = (NSTimeInterval)(DS_SECONDS_PER_FRAME / theSpeed);
		pthread_mutex_unlock(&threadParam.mutexThreadExecute);
	}
	else
	{
		pthread_mutex_unlock(&threadParam.mutexThreadExecute);
		threadParam.calcTimeBudget = 0.0;
		pthread_mutex_unlock(&threadParam.mutexThreadExecute);
	}
}

- (void) restoreCoreState
{
	[self setCoreState:prevCoreState];
}

- (void) reset
{
	[self setCoreState:CORESTATE_PAUSE];
	
	pthread_mutex_lock(&threadParam.mutexThreadExecute);
	NDS_Reset();
	pthread_mutex_unlock(&threadParam.mutexThreadExecute);
	
	[self restoreCoreState];
	self.masterExecute = YES;
}

- (void) addOutput:(CocoaDSOutput *)theOutput
{
	[self.cdsOutputList addObject:theOutput];
}

- (void) removeOutput:(CocoaDSOutput *)theOutput
{
	[self.cdsOutputList removeObject:theOutput];
}

- (void) removeAllOutputs
{
	[self.cdsOutputList removeAllObjects];
}

- (void) runThread:(id)object
{
	[CocoaDSCore startupCore];
	[super runThread:object];
	[CocoaDSCore shutdownCore];
}

@end

void* RunCoreThread(void *arg)
{
	CoreThreadParam *param = (CoreThreadParam *)arg;
	CocoaDSCore *cdsCore = (CocoaDSCore *)param->cdsCore;
	NSMutableArray *cdsOutputList = [cdsCore cdsOutputList];
	NSDate *loopStartDate = nil;
	
	do
	{
		loopStartDate = [[NSDate alloc] init];
		pthread_mutex_lock(&param->mutexThreadExecute);
		
		while (!(param->state == CORESTATE_EXECUTE && execute && !param->exitThread))
		{
			[loopStartDate release];
			pthread_cond_wait(&param->condThreadExecute, &param->mutexThreadExecute);
			loopStartDate = [[NSDate alloc] init];
		}
		
		if (param->exitThread)
		{
			[loopStartDate release];
			break;
		}
		
		// Get the user's input, execute a single emulation frame, and generate
		// the frame output.
		[cdsCore.cdsController update];
		
		NDS_beginProcessingInput();
		// Shouldn't need to do any special processing steps in between.
		// We'll just jump directly to ending the input processing.
		NDS_endProcessingInput();
		
		for(CocoaDSOutput *cdsOutput in cdsOutputList)
		{
			pthread_mutex_lock(cdsOutput.mutexOutputFrame);
		}
		
		// Execute the frame and increment the frame counter.
		pthread_mutex_lock(param->mutexCoreExecute);
		NDS_exec<false>();
		pthread_mutex_unlock(param->mutexCoreExecute);
		
		if (param->framesToSkip == 0 || !param->isFrameSkipEnabled)
		{
			param->frameCount++;
		}
		
		for(CocoaDSOutput *cdsOutput in cdsOutputList)
		{
			if (param->isFrameSkipEnabled && param->framesToSkip > 0 && [cdsOutput isMemberOfClass:[CocoaDSDisplay class]])
			{
				pthread_mutex_unlock(cdsOutput.mutexOutputFrame);
				continue;
			}
			
			[cdsOutput doCoreEmuFrame];
			pthread_mutex_unlock(cdsOutput.mutexOutputFrame);
		}
		
		// Determine the number of frames to skip based on how much time "debt"
		// we owe on timeBudget.
		if (param->isFrameSkipEnabled)
		{
			CoreFrameSkip(param->calcTimeBudget, param->calcTimeBudget + [loopStartDate timeIntervalSinceNow], &param->framesToSkip);
		}
		
		// If there is any time left in the loop, go ahead and pad it.
		NSTimeInterval timePad = param->calcTimeBudget + [loopStartDate timeIntervalSinceNow];
		
		pthread_mutex_unlock(&param->mutexThreadExecute);
		
		if(timePad > 0.0)
		{
#if (MAC_OS_X_VERSION_MAX_ALLOWED <= MAC_OS_X_VERSION_10_4) // Code for Mac OS X 10.4 and earlier
			[NSThread sleepUntilDate:[NSDate dateWithTimeIntervalSinceNow:timePad]];
#else // Code for Mac OS X 10.5 and later
			[NSThread sleepForTimeInterval:timePad];
#endif
		}
		
		[loopStartDate release];
		
	} while (!param->exitThread);
	
	return nil;
}

void CoreFrameSkip(NSTimeInterval timeBudget, NSTimeInterval timeRemaining, unsigned int *outFramesToSkip)
{
	if (*outFramesToSkip > 0)
	{
		NDS_SkipNextFrame();
		(*outFramesToSkip)--;
	}
	else
	{
		if (timeRemaining <= 0.0)
		{
			if (timeBudget > 0.0)
			{
				*outFramesToSkip = (unsigned int)( ((-timeRemaining * FRAME_SKIP_AGGRESSIVENESS)/timeBudget) + FRAME_SKIP_BIAS );
			}
			else
			{
				*outFramesToSkip = (unsigned int)( ((-timeRemaining * FRAME_SKIP_AGGRESSIVENESS * 100.0)/DS_SECONDS_PER_FRAME) + FRAME_SKIP_BIAS );
			}
			
			if (*outFramesToSkip > (unsigned int)MAX_FRAME_SKIP)
			{
				*outFramesToSkip = (unsigned int)MAX_FRAME_SKIP;
			}
		}
	}
}
