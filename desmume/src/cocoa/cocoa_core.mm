/*
	Copyright (C) 2011 Roger Manuel
	Copyright (C) 2011-2013 DeSmuME team

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
#import "cocoa_GPU.h"
#import "cocoa_globals.h"
#import "cocoa_output.h"
#import "cocoa_rom.h"
#import "cocoa_util.h"

#include <mach/mach.h>
#include <mach/mach_time.h>

#include "../NDSSystem.h"
#include "../slot1.h"
#include "../slot2.h"
#undef BOOL


//accessed from other files
volatile bool execute = true;

@implementation CocoaDSCore

@dynamic cdsController;
@synthesize cdsFirmware;
@synthesize cdsGPU;
@synthesize cdsOutputList;

@dynamic masterExecute;
@dynamic isFrameSkipEnabled;
@dynamic coreState;
@dynamic isSpeedLimitEnabled;
@dynamic isCheatingEnabled;
@dynamic speedScalar;

@dynamic emulationFlags;
@synthesize emuFlagAdvancedBusLevelTiming;
@synthesize emuFlagRigorousTiming;
@synthesize emuFlagUseExternalBios;
@synthesize emuFlagEmulateBiosInterrupts;
@synthesize emuFlagPatchDelayLoop;
@synthesize emuFlagUseExternalFirmware;
@synthesize emuFlagFirmwareBoot;
@synthesize emuFlagDebugConsole;
@synthesize emuFlagEmulateEnsata;
@dynamic cpuEmulationEngine;
@dynamic maxJITBlockSize;
@synthesize slot1DeviceType;
@synthesize slot1StatusText;

@dynamic arm9ImageURL;
@dynamic arm7ImageURL;
@dynamic firmwareImageURL;
@synthesize slot1R4URL;

@dynamic mutexCoreExecute;

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
	cdsGPU = [[[[CocoaDSGPU alloc] init] autorelease] retain];
	cdsOutputList = [[NSMutableArray alloc] initWithCapacity:32];
	
	emulationFlags = EMULATION_ADVANCED_BUS_LEVEL_TIMING_MASK;
	emuFlagAdvancedBusLevelTiming = YES;
	emuFlagRigorousTiming = NO;
	emuFlagUseExternalBios = NO;
	emuFlagEmulateBiosInterrupts = NO;
	emuFlagPatchDelayLoop = NO;
	emuFlagUseExternalFirmware = NO;
	emuFlagFirmwareBoot = NO;
	emuFlagDebugConsole = NO;
	emuFlagEmulateEnsata = NO;
	
	slot1DeviceType = NDS_SLOT1_RETAIL_AUTO;
	slot1StatusText = NSSTRING_STATUS_EMULATION_NOT_RUNNING;
	
	spinlockMasterExecute = OS_SPINLOCK_INIT;
	spinlockCdsController = OS_SPINLOCK_INIT;
	spinlockExecutionChange = OS_SPINLOCK_INIT;
	spinlockCheatEnableFlag = OS_SPINLOCK_INIT;
	spinlockEmulationFlags = OS_SPINLOCK_INIT;
	spinlockCPUEmulationEngine = OS_SPINLOCK_INIT;
	
	isSpeedLimitEnabled = YES;
	speedScalar = SPEED_SCALAR_NORMAL;
	prevCoreState = CORESTATE_PAUSE;
	
	slot1R4URL = nil;
	_slot1R4Path = "";
	
	threadParam.cdsCore = self;
	threadParam.cdsController = cdsController;
	threadParam.state = CORESTATE_PAUSE;
	threadParam.isFrameSkipEnabled = true;
	threadParam.frameCount = 0;
	threadParam.framesToSkip = 0;
	
	uint64_t timeBudgetNanoseconds = (uint64_t)(DS_SECONDS_PER_FRAME * 1000000000.0 / speedScalar);
	AbsoluteTime timeBudgetAbsTime = NanosecondsToAbsolute(*(Nanoseconds *)&timeBudgetNanoseconds);
	threadParam.timeBudgetMachAbsTime = *(uint64_t *)&timeBudgetAbsTime;
	
	threadParam.exitThread = false;
	pthread_mutex_init(&threadParam.mutexCoreExecute, NULL);
	pthread_mutex_init(&threadParam.mutexOutputList, NULL);
	pthread_mutex_init(&threadParam.mutexThreadExecute, NULL);
	pthread_cond_init(&threadParam.condThreadExecute, NULL);
	pthread_create(&coreThread, NULL, &RunCoreThread, &threadParam);
	
	[cdsGPU setMutexProducer:self.mutexCoreExecute];
	
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
	
	self.cdsController = nil;
	self.cdsFirmware = nil;
	self.cdsGPU = nil;
	
	[self removeAllOutputs];
	[cdsOutputList release];
	
	pthread_mutex_destroy(&threadParam.mutexOutputList);
	pthread_mutex_destroy(&threadParam.mutexCoreExecute);
	
	[super dealloc];
}

+ (BOOL) startupCore
{
	NSInteger result = -1;
	
	if (isCoreStarted)
	{
		return isCoreStarted;
	}
	
	slot2_Change(NDS_SLOT2_NONE);
	
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
	execute = theState ? true : false;
	OSSpinLockUnlock(&spinlockMasterExecute);
}

- (BOOL) masterExecute
{
	OSSpinLockLock(&spinlockMasterExecute);
	const BOOL theState = execute ? YES : NO;
	OSSpinLockUnlock(&spinlockMasterExecute);
	
	return theState;
}

- (void) setCdsController:(CocoaDSController *)theController
{
	OSSpinLockLock(&spinlockCdsController);
	
	if (theController == cdsController)
	{
		OSSpinLockUnlock(&spinlockCdsController);
		return;
	}
	
	if (theController != nil)
	{
		[theController retain];
	}
	
	pthread_mutex_lock(&threadParam.mutexThreadExecute);
	[cdsController release];
	cdsController = theController;
	threadParam.cdsController = theController;
	pthread_mutex_unlock(&threadParam.mutexThreadExecute);
	
	OSSpinLockUnlock(&spinlockCdsController);
}

- (CocoaDSController *) cdsController
{
	OSSpinLockLock(&spinlockCdsController);
	CocoaDSController *theController = cdsController;
	OSSpinLockUnlock(&spinlockCdsController);
	
	return theController;
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
		threadParam.framesToSkip = 0;
	}
	
	pthread_mutex_unlock(&threadParam.mutexThreadExecute);
}

- (BOOL) isFrameSkipEnabled
{
	pthread_mutex_lock(&threadParam.mutexThreadExecute);
	const BOOL theState = threadParam.isFrameSkipEnabled ? YES : NO;
	pthread_mutex_unlock(&threadParam.mutexThreadExecute);
	
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
	const CGFloat scalar = speedScalar;
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
	const BOOL enabled = isSpeedLimitEnabled;
	OSSpinLockUnlock(&spinlockExecutionChange);
	
	return enabled;
}

- (void) setIsCheatingEnabled:(BOOL)theState
{
	OSSpinLockLock(&spinlockCheatEnableFlag);
	CommonSettings.cheatsDisable = theState ? false : true;
	OSSpinLockUnlock(&spinlockCheatEnableFlag);
}

- (BOOL) isCheatingEnabled
{
	OSSpinLockLock(&spinlockCheatEnableFlag);
	BOOL theState = CommonSettings.cheatsDisable ? NO : YES;
	OSSpinLockUnlock(&spinlockCheatEnableFlag);
	
	return theState;
}

- (void) setEmulationFlags:(NSUInteger)theFlags
{
	OSSpinLockLock(&spinlockEmulationFlags);
	emulationFlags = theFlags;
	OSSpinLockUnlock(&spinlockEmulationFlags);
	
	pthread_mutex_lock(&threadParam.mutexCoreExecute);
	
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
	
	if (theFlags & EMULATION_RIGOROUS_TIMING_MASK)
	{
		self.emuFlagRigorousTiming = YES;
		CommonSettings.rigorous_timing = true;
	}
	else
	{
		self.emuFlagRigorousTiming = NO;
		CommonSettings.rigorous_timing = false;
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
	
	pthread_mutex_unlock(&threadParam.mutexCoreExecute);
}

- (NSUInteger) emulationFlags
{
	OSSpinLockLock(&spinlockEmulationFlags);
	const NSUInteger theFlags = emulationFlags;
	OSSpinLockUnlock(&spinlockEmulationFlags);
	
	return theFlags;
}

- (void) setCpuEmulationEngine:(NSInteger)engineID
{
	OSSpinLockLock(&spinlockCPUEmulationEngine);
#if defined(__i386__) || defined(__x86_64__)
	cpuEmulationEngine = engineID;
#else
	cpuEmulationEngine = CPU_EMULATION_ENGINE_INTERPRETER;
#endif
	OSSpinLockUnlock(&spinlockCPUEmulationEngine);
}

- (NSInteger) cpuEmulationEngine
{
	OSSpinLockLock(&spinlockCPUEmulationEngine);
	const NSInteger engineID = cpuEmulationEngine;
	OSSpinLockUnlock(&spinlockCPUEmulationEngine);
	
	return engineID;
}

- (void) setMaxJITBlockSize:(NSInteger)blockSize
{
	pthread_mutex_lock(&threadParam.mutexCoreExecute);
	CommonSettings.jit_max_block_size = (blockSize > 0) ? blockSize : 1;
	pthread_mutex_unlock(&threadParam.mutexCoreExecute);
}

- (NSInteger) maxJITBlockSize
{
	pthread_mutex_lock(&threadParam.mutexCoreExecute);
	const NSInteger blockSize = CommonSettings.jit_max_block_size;
	pthread_mutex_unlock(&threadParam.mutexCoreExecute);
	
	return blockSize;
}

- (void) setCoreState:(NSInteger)coreState
{
	pthread_mutex_lock(&threadParam.mutexThreadExecute);
	
	if (threadParam.state == CORESTATE_PAUSE)
	{
		prevCoreState = CORESTATE_PAUSE;
	}
	else
	{
		prevCoreState = CORESTATE_EXECUTE;
	}
	
	threadParam.state = coreState;
	pthread_cond_signal(&threadParam.condThreadExecute);
	pthread_mutex_unlock(&threadParam.mutexThreadExecute);
}

- (NSInteger) coreState
{
	pthread_mutex_lock(&threadParam.mutexThreadExecute);
	const NSInteger theState = threadParam.state;
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

- (pthread_mutex_t *) mutexCoreExecute
{
	return &threadParam.mutexCoreExecute;
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

- (void) slot1Eject
{
	pthread_mutex_lock(&threadParam.mutexCoreExecute);
	NDS_TriggerCardEjectIRQ();
	pthread_mutex_unlock(&threadParam.mutexCoreExecute);
	
	[self setSlot1StatusText:NSSTRING_STATUS_SLOT1_NO_DEVICE];
}

- (void) changeRomSaveType:(NSInteger)saveTypeID
{
	pthread_mutex_lock(&threadParam.mutexCoreExecute);
	[CocoaDSRom changeRomSaveType:saveTypeID];
	pthread_mutex_unlock(&threadParam.mutexCoreExecute);
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
		
		pthread_mutex_lock(&threadParam.mutexThreadExecute);
		uint64_t timeBudgetNanoseconds = (uint64_t)(DS_SECONDS_PER_FRAME * 1000000000.0 / theSpeed);
		AbsoluteTime timeBudgetAbsTime = NanosecondsToAbsolute(*(Nanoseconds *)&timeBudgetNanoseconds);
		threadParam.timeBudgetMachAbsTime = *(uint64_t *)&timeBudgetAbsTime;
		pthread_mutex_unlock(&threadParam.mutexThreadExecute);
	}
	else
	{
		pthread_mutex_lock(&threadParam.mutexThreadExecute);
		threadParam.timeBudgetMachAbsTime = 0;
		pthread_mutex_unlock(&threadParam.mutexThreadExecute);
	}
}

/********************************************************************************************
	applyDynaRec

	Sets the use_jit variable for CommonSettings.

	Takes:
		Nothing.

	Returns:
		Nothing.

	Details:
		In the UI, we call setCpuEmulationEngine to set whether we should use the
		interpreter or the dynamic recompiler. However, the emulator cannot handle
		changing the engine while the emulation is running. Therefore, we use this
		method to set the engine at a later time, using the last cpuEmulationEngine
		value from the user.
 ********************************************************************************************/
- (void) applyDynaRec
{
	const NSInteger engineID = [self cpuEmulationEngine];
	
	pthread_mutex_lock(&threadParam.mutexThreadExecute);
	CommonSettings.use_jit = (engineID == CPU_EMULATION_ENGINE_DYNAMIC_RECOMPILER);
	pthread_mutex_unlock(&threadParam.mutexThreadExecute);
}

- (BOOL) applySlot1Device
{
	const NSInteger deviceTypeID = [self slot1DeviceType];
	NSString *r4Path = [[self slot1R4URL] path];
	
	pthread_mutex_lock(&threadParam.mutexThreadExecute);
	
	_slot1R4Path = (r4Path != nil) ? std::string([r4Path cStringUsingEncoding:NSUTF8StringEncoding]) : "";
	slot1_SetFatDir(_slot1R4Path);
	
	BOOL result = slot1_Change((NDS_SLOT1_TYPE)deviceTypeID);
	
	pthread_mutex_unlock(&threadParam.mutexThreadExecute);
	
	switch (deviceTypeID)
	{
		case NDS_SLOT1_NONE:
			[self setSlot1StatusText:NSSTRING_STATUS_SLOT1_NO_DEVICE];
			break;
			
		case NDS_SLOT1_RETAIL_AUTO:
			[self setSlot1StatusText:NSSTRING_STATUS_SLOT1_RETAIL_INSERTED];
			break;
			
		case NDS_SLOT1_RETAIL_NAND:
			[self setSlot1StatusText:NSSTRING_STATUS_SLOT1_RETAIL_NAND_INSERTED];
			break;
			
		case NDS_SLOT1_R4:
			[self setSlot1StatusText:NSSTRING_STATUS_SLOT1_R4_INSERTED];
			break;
			
		case NDS_SLOT1_RETAIL_MCROM:
			[self setSlot1StatusText:NSSTRING_STATUS_SLOT1_STANDARD_INSERTED];
			break;
			
		default:
			[self setSlot1StatusText:NSSTRING_STATUS_SLOT1_UNKNOWN_STATE];
			break;
	}
	
	return result;
}

- (void) restoreCoreState
{
	[self setCoreState:prevCoreState];
}

- (void) reset
{
	[self setCoreState:CORESTATE_PAUSE];
	[self applyDynaRec];
	[self applySlot1Device];
	
	pthread_mutex_lock(&threadParam.mutexThreadExecute);
	NDS_Reset();
	pthread_mutex_unlock(&threadParam.mutexThreadExecute);
	
	[self restoreCoreState];
	self.masterExecute = YES;
}

- (void) addOutput:(CocoaDSOutput *)theOutput
{
	pthread_mutex_lock(&threadParam.mutexOutputList);
	theOutput.mutexProducer = self.mutexCoreExecute;
	[self.cdsOutputList addObject:theOutput];
	pthread_mutex_unlock(&threadParam.mutexOutputList);
}

- (void) removeOutput:(CocoaDSOutput *)theOutput
{
	pthread_mutex_lock(&threadParam.mutexOutputList);
	[self.cdsOutputList removeObject:theOutput];
	pthread_mutex_unlock(&threadParam.mutexOutputList);
}

- (void) removeAllOutputs
{
	pthread_mutex_lock(&threadParam.mutexOutputList);
	[self.cdsOutputList removeAllObjects];
	pthread_mutex_unlock(&threadParam.mutexOutputList);
}

- (void) runThread:(id)object
{
	[CocoaDSCore startupCore];
	[super runThread:object];
	[CocoaDSCore shutdownCore];
}

- (NSString *) cpuEmulationEngineString
{
	NSString *theString = @"Uninitialized";
	
	switch ([self cpuEmulationEngine])
	{
		case CPU_EMULATION_ENGINE_INTERPRETER:
			theString = @"Interpreter";
			break;
			
		case CPU_EMULATION_ENGINE_DYNAMIC_RECOMPILER:
			theString = @"Dynamic Recompiler";
			break;
			
		default:
			break;
	}
	
	return theString;
}

- (NSString *) slot1DeviceTypeString
{
	NSString *theString = @"Uninitialized";
	
	pthread_mutex_lock(&threadParam.mutexThreadExecute);
	
	if(slot1_device == NULL)
	{
		pthread_mutex_unlock(&threadParam.mutexThreadExecute);
		return theString;
	}
	
	const Slot1Info *info = slot1_device->info();
	theString = [NSString stringWithCString:info->name() encoding:NSUTF8StringEncoding];
	
	pthread_mutex_unlock(&threadParam.mutexThreadExecute);
	
	return theString;
}

- (NSString *) slot2DeviceTypeString
{
	NSString *theString = @"Uninitialized";
	
	pthread_mutex_lock(&threadParam.mutexThreadExecute);
	
	if(slot2_device == NULL)
	{
		pthread_mutex_unlock(&threadParam.mutexThreadExecute);
		return theString;
	}
	
	const Slot2Info *info = slot2_device->info();
	theString = [NSString stringWithCString:info->name() encoding:NSUTF8StringEncoding];
	
	pthread_mutex_unlock(&threadParam.mutexThreadExecute);
	
	return theString;
}

@end

static void* RunCoreThread(void *arg)
{
	CoreThreadParam *param = (CoreThreadParam *)arg;
	CocoaDSCore *cdsCore = (CocoaDSCore *)param->cdsCore;
	NSMutableArray *cdsOutputList = [cdsCore cdsOutputList];
	uint64_t startTime = 0;
	uint64_t timeBudget = 0; // Need local variable to ensure that param->timeBudgetMachAbsTime is thread-safe.
	
	do
	{
		startTime = mach_absolute_time();
		pthread_mutex_lock(&param->mutexThreadExecute);
		timeBudget = param->timeBudgetMachAbsTime;
		
		while (!(param->state == CORESTATE_EXECUTE && execute && !param->exitThread))
		{
			pthread_cond_wait(&param->condThreadExecute, &param->mutexThreadExecute);
			startTime = mach_absolute_time();
			timeBudget = param->timeBudgetMachAbsTime;
		}
		
		if (param->exitThread)
		{
			pthread_mutex_unlock(&param->mutexThreadExecute);
			break;
		}
		
		// Get the user's input, execute a single emulation frame, and generate
		// the frame output.
		[(CocoaDSController *)param->cdsController flush];
		
		NDS_beginProcessingInput();
		// Shouldn't need to do any special processing steps in between.
		// We'll just jump directly to ending the input processing.
		NDS_endProcessingInput();
		
		// Execute the frame and increment the frame counter.
		pthread_mutex_lock(&param->mutexCoreExecute);
		NDS_exec<false>();
		pthread_mutex_unlock(&param->mutexCoreExecute);
		
		// Check if an internal execution error occurred that halted the emulation.
		if (!execute)
		{
			pthread_mutex_unlock(&param->mutexThreadExecute);
			// TODO: Message the core that emulation halted.
			NSLog(@"The emulator halted during execution. Was it an internal error that caused this?");
			continue;
		}
		
		if (param->framesToSkip == 0)
		{
			param->frameCount++;
		}
		
		pthread_mutex_lock(&param->mutexOutputList);
		for(CocoaDSOutput *cdsOutput in cdsOutputList)
		{
			if (param->framesToSkip > 0 && [cdsOutput isKindOfClass:[CocoaDSDisplay class]])
			{
				continue;
			}
			
			[cdsOutput doCoreEmuFrame];
		}
		pthread_mutex_unlock(&param->mutexOutputList);
		
		// Determine the number of frames to skip based on how much time "debt"
		// we owe on timeBudget.
		if (param->isFrameSkipEnabled)
		{
			CoreFrameSkip(timeBudget, startTime, &param->framesToSkip);
		}
		
		pthread_mutex_unlock(&param->mutexThreadExecute);
		
		// If there is any time left in the loop, go ahead and pad it.
		mach_wait_until(startTime + timeBudget);
		
	} while (!param->exitThread);
	
	return nil;
}

static void CoreFrameSkip(uint64_t timeBudgetMachAbsTime, uint64_t frameStartMachAbsTime, unsigned int *outFramesToSkip)
{
	if (*outFramesToSkip > 0)
	{
		NDS_SkipNextFrame();
		(*outFramesToSkip)--;
	}
	else
	{
		// Calculate the time remaining.
		uint64_t elapsed = mach_absolute_time() - frameStartMachAbsTime;
		
		if (elapsed > timeBudgetMachAbsTime)
		{
			static unsigned int lastSetFrameSkip = 0;
			unsigned int framesToSkip = 0;
			
			if (timeBudgetMachAbsTime > 0)
			{
				framesToSkip = (unsigned int)( (((double)(elapsed - timeBudgetMachAbsTime) * FRAME_SKIP_AGGRESSIVENESS) / (double)timeBudgetMachAbsTime) + FRAME_SKIP_BIAS );
				if (framesToSkip < lastSetFrameSkip)
				{
					framesToSkip += (unsigned int)((double)(lastSetFrameSkip - framesToSkip) * FRAME_SKIP_SMOOTHNESS);
				}
				
				lastSetFrameSkip = framesToSkip;
			}
			else
			{
				framesToSkip = (unsigned int)( (((double)(elapsed - timeBudgetMachAbsTime) * FRAME_SKIP_AGGRESSIVENESS * 100.0) / DS_SECONDS_PER_FRAME) + FRAME_SKIP_BIAS );
				// Don't need to save lastSetFrameSkip here since this code path assumes that
				// the frame limiter is disabled.
			}
			
			// Bound the frame skip.
			if (framesToSkip > (unsigned int)MAX_FRAME_SKIP)
			{
				framesToSkip = (unsigned int)MAX_FRAME_SKIP;
				lastSetFrameSkip = framesToSkip;
			}
			
			*outFramesToSkip = framesToSkip;
		}
		else
		{
			*outFramesToSkip = 0;
		}
	}
}
