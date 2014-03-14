/*
	Copyright (C) 2011 Roger Manuel
	Copyright (C) 2011-2014 DeSmuME team

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

#include "../movie.h"
#include "../NDSSystem.h"
#include "../slot1.h"
#include "../slot2.h"
#include "../movie.h"
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
@synthesize frameStatus;
@synthesize executionSpeedStatus;

@dynamic arm9ImageURL;
@dynamic arm7ImageURL;
@dynamic firmwareImageURL;
@synthesize slot1R4URL;

@dynamic mutexCoreExecute;

- (id)init
{
	self = [super init];
	if(self == nil)
	{
		return self;
	}
	
	int initResult = NDS_Init();
	if (initResult == -1)
	{
		[self release];
		self = nil;
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
	threadParam.frameJumpTarget = 0;
	
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
	
	frameStatus = @"---";
	executionSpeedStatus = @"1.00x";
	
	return self;
}

- (void)dealloc
{
	[self setCoreState:CORESTATE_PAUSE];
	
	[self removeAllOutputs];
	[cdsOutputList release];
	
	[self setCdsController:nil];
	[self setCdsFirmware:nil];
	[self setCdsGPU:nil];
	
	pthread_mutex_lock(&threadParam.mutexThreadExecute);
	threadParam.exitThread = true;
	pthread_cond_signal(&threadParam.condThreadExecute);
	pthread_mutex_unlock(&threadParam.mutexThreadExecute);
	
	pthread_join(coreThread, NULL);
	coreThread = NULL;
	
	pthread_mutex_destroy(&threadParam.mutexThreadExecute);
	pthread_cond_destroy(&threadParam.condThreadExecute);
	pthread_mutex_destroy(&threadParam.mutexOutputList);
	pthread_mutex_destroy(&threadParam.mutexCoreExecute);
	
	NDS_DeInit();
	
	[super dealloc];
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
	
	if (threadParam.state == CORESTATE_EXECUTE || threadParam.state == CORESTATE_PAUSE)
	{
		prevCoreState = threadParam.state;
	}
	
	threadParam.state = coreState;
	threadParam.framesToSkip = 0;
	
	switch (coreState)
	{
		case CORESTATE_PAUSE:
		{
			for(CocoaDSOutput *cdsOutput in cdsOutputList)
			{
				[cdsOutput setIdle:YES];
			}
			
			[self setFrameStatus:[NSString stringWithFormat:@"%lu", (unsigned long)[self frameNumber]]];
			break;
		}
			
		case CORESTATE_FRAMEADVANCE:
		{
			for(CocoaDSOutput *cdsOutput in cdsOutputList)
			{
				[cdsOutput setIdle:NO];
			}
			
			[self setFrameStatus:[NSString stringWithFormat:@"%lu", (unsigned long)[self frameNumber]]];
			break;
		}
			
		case CORESTATE_EXECUTE:
		{
			for(CocoaDSOutput *cdsOutput in cdsOutputList)
			{
				[cdsOutput setIdle:NO];
			}
			
			[self setFrameStatus:@"Executing..."];
			break;
		}
			
		case CORESTATE_FRAMEJUMP:
		{
			for(CocoaDSOutput *cdsOutput in cdsOutputList)
			{
				if (![cdsOutput isKindOfClass:[CocoaDSDisplay class]])
				{
					[cdsOutput setIdle:YES];
				}
			}
			
			[self setFrameStatus:[NSString stringWithFormat:@"Jumping to frame %lu.", (unsigned long)threadParam.frameJumpTarget]];
			break;
		}
			
		default:
			break;
	}
	
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
		const CGFloat theSpeed = ([self speedScalar] > SPEED_SCALAR_MIN) ? [self speedScalar] : SPEED_SCALAR_MIN;
		pthread_mutex_lock(&threadParam.mutexThreadExecute);
		threadParam.timeBudgetMachAbsTime = GetFrameAbsoluteTime(1.0/theSpeed);
		pthread_mutex_unlock(&threadParam.mutexThreadExecute);
		
		[self setExecutionSpeedStatus:[NSString stringWithFormat:@"%1.2fx", theSpeed]];
	}
	else
	{
		pthread_mutex_lock(&threadParam.mutexThreadExecute);
		threadParam.timeBudgetMachAbsTime = 0;
		pthread_mutex_unlock(&threadParam.mutexThreadExecute);
		
		[self setExecutionSpeedStatus:@"Unlimited"];
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

- (NSUInteger) frameNumber
{
	pthread_mutex_lock(&threadParam.mutexCoreExecute);
	const NSUInteger currFrameNum = currFrameCounter;
	pthread_mutex_unlock(&threadParam.mutexCoreExecute);
	
	return currFrameNum;
}

- (void) frameJumpTo:(NSUInteger)targetFrameNum
{
	pthread_mutex_lock(&threadParam.mutexThreadExecute);
	
	threadParam.frameJumpTarget = targetFrameNum;
	if (targetFrameNum <= (NSUInteger)currFrameCounter)
	{
		pthread_mutex_unlock(&threadParam.mutexThreadExecute);
		return;
	}
	
	pthread_mutex_unlock(&threadParam.mutexThreadExecute);
	
	[self setCoreState:CORESTATE_FRAMEJUMP];
}

- (void) frameJump:(NSUInteger)relativeFrameNum
{
	[self frameJumpTo:[self frameNumber] + relativeFrameNum];
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

- (BOOL) startReplayRecording:(NSURL *)fileURL sramURL:(NSURL *)sramURL
{
	if (fileURL == nil)
	{
		return NO;
	}
	
	std::string sramPath = (sramURL != nil) ? [[sramURL path] cStringUsingEncoding:NSUTF8StringEncoding] : "";
	const char *fileName = [[fileURL path] cStringUsingEncoding:NSUTF8StringEncoding];
	
	NSDate *currentDate = [NSDate date];
	NSString *currentDateStr = [currentDate descriptionWithCalendarFormat:@"%Y %m %d %H %M %S %F"
																 timeZone:nil
																   locale:[[NSUserDefaults standardUserDefaults] dictionaryRepresentation]];
	
	int dateYear = 2009;
	int dateMonth = 1;
	int dateDay = 1;
	int dateHour = 0;
	int dateMinute = 0;
	int dateSecond = 0;
	int dateMillisecond = 0;
	const char *dateCStr = [currentDateStr cStringUsingEncoding:NSUTF8StringEncoding];
	sscanf(dateCStr, "%i %i %i %i %i %i %i", &dateYear, &dateMonth, &dateDay, &dateHour, &dateMinute, &dateSecond, &dateMillisecond);
	
	DateTime rtcDate = DateTime(dateYear,
								dateMonth,
								dateDay,
								dateHour,
								dateMinute,
								dateSecond,
								dateMillisecond);
	
	FCEUI_SaveMovie(fileName, L"Test Author", 0, sramPath, rtcDate);
	
	return YES;
}

- (void) stopReplay
{
	FCEUI_StopMovie();
}

@end

static void* RunCoreThread(void *arg)
{
	CoreThreadParam *param = (CoreThreadParam *)arg;
	CocoaDSCore *cdsCore = (CocoaDSCore *)param->cdsCore;
	NSMutableArray *cdsOutputList = [cdsCore cdsOutputList];
	NSUInteger frameNum = 0;
	uint64_t startTime = 0;
	uint64_t timeBudget = 0; // Need local variable to ensure that param->timeBudgetMachAbsTime is thread-safe.
	
	do
	{
		startTime = mach_absolute_time();
		pthread_mutex_lock(&param->mutexThreadExecute);
		timeBudget = param->timeBudgetMachAbsTime;
		
		while (!(param->state != CORESTATE_PAUSE && execute && !param->exitThread))
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
		
		if (param->state != CORESTATE_FRAMEJUMP)
		{
			[(CocoaDSController *)param->cdsController flush];
		}
		else
		{
			[(CocoaDSController *)param->cdsController flushEmpty];
		}
		
		NDS_beginProcessingInput();
		FCEUMOV_HandlePlayback();
		NDS_endProcessingInput();
		FCEUMOV_HandleRecording();
		
		// Execute the frame and increment the frame counter.
		pthread_mutex_lock(&param->mutexCoreExecute);
		NDS_exec<false>();
		frameNum = currFrameCounter;
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
		
		switch (param->state)
		{
			case CORESTATE_EXECUTE:
			{
				for(CocoaDSOutput *cdsOutput in cdsOutputList)
				{
					if (param->framesToSkip > 0 && [cdsOutput isKindOfClass:[CocoaDSDisplay class]])
					{
						continue;
					}
					
					[cdsOutput doCoreEmuFrame];
				}
				break;
			}
				
			case CORESTATE_FRAMEADVANCE:
				for(CocoaDSOutput *cdsOutput in cdsOutputList)
				{
					[cdsOutput doCoreEmuFrame];
				}
				break;
				
			case CORESTATE_FRAMEJUMP:
			{
				for(CocoaDSOutput *cdsOutput in cdsOutputList)
				{
					if ([cdsOutput isKindOfClass:[CocoaDSDisplay class]] && (param->framesToSkip == 0 || frameNum >= param->frameJumpTarget))
					{
						[cdsOutput doCoreEmuFrame];
					}
				}
				break;
			}
				
			default:
				break;
		}
		
		pthread_mutex_unlock(&param->mutexOutputList);
		
		switch (param->state)
		{
			case CORESTATE_EXECUTE:
			{
				// Determine the number of frames to skip based on how much time "debt"
				// we owe on timeBudget.
				if (param->isFrameSkipEnabled)
				{
					if (param->framesToSkip > 0)
					{
						NDS_SkipNextFrame();
						param->framesToSkip--;
					}
					else
					{
						param->framesToSkip = CalculateFrameSkip(timeBudget, startTime);
					}
				}
				break;
			}
			
			case CORESTATE_FRAMEJUMP:
			{
				if (param->framesToSkip > 0)
				{
					NDS_SkipNextFrame();
					param->framesToSkip--;
				}
				else
				{
					param->framesToSkip = (int)((DS_FRAMES_PER_SECOND * 1.0) + 0.85);
				}
				break;
			}
				
			default:
				break;
		}
		
		pthread_mutex_unlock(&param->mutexThreadExecute);
		
		// If we doing a frame advance, switch back to pause state immediately
		// after we're done with the frame.
		if (param->state == CORESTATE_FRAMEADVANCE)
		{
			[cdsCore setCoreState:CORESTATE_PAUSE];
		}
		else if (param->state == CORESTATE_FRAMEJUMP)
		{
			if (frameNum >= param->frameJumpTarget)
			{
				[cdsCore restoreCoreState];
			}
		}
		else
		{
			// If there is any time left in the loop, go ahead and pad it.
			mach_wait_until(startTime + timeBudget);
		}
		
	} while (!param->exitThread);
	
	return NULL;
}

static int CalculateFrameSkip(uint64_t timeBudgetMachAbsTime, uint64_t frameStartMachAbsTime)
{
	static const double skipCurve[10]	= {0.60, 0.58, 0.55, 0.51, 0.46, 0.40, 0.30, 0.20, 0.10, 0.00};
	static const double unskipCurve[10]	= {0.75, 0.70, 0.65, 0.60, 0.50, 0.40, 0.30, 0.20, 0.10, 0.00};
	static size_t skipStep = 0;
	static size_t unskipStep = 0;
	static int lastSetFrameSkip = 0;
	
	// Calculate the time remaining.
	const uint64_t elapsed = mach_absolute_time() - frameStartMachAbsTime;
	int framesToSkip = 0;
	
	if (elapsed > timeBudgetMachAbsTime)
	{
		if (timeBudgetMachAbsTime > 0)
		{
			framesToSkip = (int)( (((double)(elapsed - timeBudgetMachAbsTime) * FRAME_SKIP_AGGRESSIVENESS) / (double)timeBudgetMachAbsTime) + FRAME_SKIP_BIAS );
			
			if (framesToSkip > lastSetFrameSkip)
			{
				framesToSkip -= (int)((double)(framesToSkip - lastSetFrameSkip) * skipCurve[skipStep]);
				if (skipStep < 9)
				{
					skipStep++;
				}
			}
			else
			{
				framesToSkip += (int)((double)(lastSetFrameSkip - framesToSkip) * skipCurve[skipStep]);
				if (skipStep > 0)
				{
					skipStep--;
				}
			}
		}
		else
		{
			static const double frameRate100x = (double)FRAME_SKIP_AGGRESSIVENESS / (double)GetFrameAbsoluteTime(1.0/100.0);
			framesToSkip = (int)((double)elapsed * frameRate100x);
		}
		
		unskipStep = 0;
	}
	else
	{
		framesToSkip = (int)((double)lastSetFrameSkip * unskipCurve[unskipStep]);
		if (unskipStep < 9)
		{
			unskipStep++;
		}
		
		skipStep = 0;
	}
	
	// Bound the frame skip.
	static const int kMaxFrameSkip = (int)MAX_FRAME_SKIP;
	if (framesToSkip > kMaxFrameSkip)
	{
		framesToSkip = kMaxFrameSkip;
	}
	
	lastSetFrameSkip = framesToSkip;
	
	return framesToSkip;
}

uint64_t GetFrameAbsoluteTime(const double frameTimeScalar)
{
	const uint64_t frameTimeNanoseconds = (uint64_t)(DS_SECONDS_PER_FRAME * 1000000000.0 * frameTimeScalar);
	const AbsoluteTime frameTimeAbsTime = NanosecondsToAbsolute(*(Nanoseconds *)&frameTimeNanoseconds);
	
	return *(uint64_t *)&frameTimeAbsTime;
}
