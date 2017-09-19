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

#import "cocoa_core.h"
#import "cocoa_input.h"
#import "cocoa_firmware.h"
#import "cocoa_GPU.h"
#import "cocoa_globals.h"
#import "cocoa_output.h"
#import "cocoa_rom.h"
#import "cocoa_util.h"

#include "ClientExecutionControl.h"
#include "ClientInputHandler.h"

#include "../../movie.h"
#include "../../NDSSystem.h"
#include "../../armcpu.h"
#include "../../driver.h"
#include "../../gdbstub.h"
#include "../../slot1.h"
#include "../../slot2.h"
#include "../../SPU.h"
#undef BOOL

// Need to include assert.h this way so that GDB stub will work
// with an optimized build.
#if defined(GDB_STUB) && defined(NDEBUG)
	#define TEMP_NDEBUG
	#undef NDEBUG
#endif

#include <assert.h>

#if defined(TEMP_NDEBUG)
	#undef TEMP_NDEBUG
	#define NDEBUG
#endif

class OSXDriver : public BaseDriver
{
private:
	pthread_mutex_t *mutexThreadExecute;
	pthread_rwlock_t *rwlockCoreExecute;
	CocoaDSCore *cdsCore;
	
public:
	pthread_mutex_t* GetCoreThreadMutexLock();
	void SetCoreThreadMutexLock(pthread_mutex_t *theMutex);
	pthread_rwlock_t* GetCoreExecuteRWLock();
	void SetCoreExecuteRWLock(pthread_rwlock_t *theRwLock);
	void SetCore(CocoaDSCore *theCore);
	
	virtual void EMU_DebugIdleEnter();
	virtual void EMU_DebugIdleUpdate();
	virtual void EMU_DebugIdleWakeUp();
};


//accessed from other files
volatile bool execute = true;

@implementation CocoaDSCore

@synthesize execControl;

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

@dynamic frameJumpBehavior;
@dynamic frameJumpNumberFramesForward;
@dynamic frameJumpToFrameIndex;

@dynamic isGdbStubStarted;
@dynamic isInDebugTrap;
@synthesize enableGdbStubARM9;
@synthesize enableGdbStubARM7;
@synthesize gdbStubPortARM9;
@synthesize gdbStubPortARM7;

@dynamic emuFlagAdvancedBusLevelTiming;
@dynamic emuFlagRigorousTiming;
@dynamic emuFlagUseGameSpecificHacks;
@dynamic emuFlagUseExternalBios;
@dynamic emuFlagEmulateBiosInterrupts;
@dynamic emuFlagPatchDelayLoop;
@dynamic emuFlagUseExternalFirmware;
@dynamic emuFlagFirmwareBoot;
@dynamic emuFlagDebugConsole;
@dynamic emuFlagEmulateEnsata;
@dynamic cpuEmulationEngine;
@dynamic maxJITBlockSize;
@dynamic slot1DeviceType;
@synthesize slot1StatusText;
@synthesize frameStatus;
@synthesize executionSpeedStatus;
@synthesize errorStatus;

@dynamic arm9ImageURL;
@dynamic arm7ImageURL;
@dynamic firmwareImageURL;
@dynamic slot1R4URL;

@dynamic rwlockCoreExecute;

- (id)init
{
	self = [super init];
	if(self == nil)
	{
		return self;
	}
	
	isGdbStubStarted = NO;
	isInDebugTrap = NO;
	enableGdbStubARM9 = NO;
	enableGdbStubARM7 = NO;
	gdbStubPortARM9 = 0;
	gdbStubPortARM7 = 0;
	gdbStubHandleARM9 = NULL;
	gdbStubHandleARM7 = NULL;
	
	int initResult = NDS_Init();
	if (initResult == -1)
	{
		[self release];
		self = nil;
		return self;
	}
	
	execControl = new ClientExecutionControl;
	
	_fpsTimer = nil;
	_isTimerAtSecond = NO;
	
	cdsFirmware = nil;
	cdsGPU = [[[[CocoaDSGPU alloc] init] autorelease] retain];
	cdsController = [[[[CocoaDSController alloc] init] autorelease] retain];
	cdsOutputList = [[[[NSMutableArray alloc] initWithCapacity:32] autorelease] retain];
	
	ClientInputHandler *inputHandler = [cdsController inputHandler];
	inputHandler->SetClientExecutionController(execControl);
	execControl->SetClientInputHandler(inputHandler);
	
	slot1StatusText = NSSTRING_STATUS_EMULATION_NOT_RUNNING;
	
	spinlockMasterExecute = OS_SPINLOCK_INIT;
	spinlockCdsController = OS_SPINLOCK_INIT;
	
	threadParam.cdsCore = self;
	
	pthread_mutex_init(&threadParam.mutexOutputList, NULL);
	pthread_mutex_init(&threadParam.mutexThreadExecute, NULL);
	pthread_cond_init(&threadParam.condThreadExecute, NULL);
	pthread_rwlock_init(&threadParam.rwlockCoreExecute, NULL);
	pthread_create(&coreThread, NULL, &RunCoreThread, &threadParam);
	
	// The core emulation thread needs max priority since it is the sole
	// producer thread for all output threads. Note that this is not being
	// done for performance -- this is being done for timing accuracy. The
	// core emulation thread is responsible for determining the emulator's
	// timing. If one output thread interferes with timing, then it ends up
	// affecting the whole emulator.
	//
	// Though it may be tempting to make this a real-time thread, it's best
	// to keep this a normal thread. The core emulation thread can use up a
	// lot of CPU time under certain conditions, which may interfere with
	// other threads. (Example: Video tearing on display windows, even with
	// V-sync enabled.)
	struct sched_param sp;
	int thePolicy = 0;
	memset(&sp, 0, sizeof(struct sched_param));
	pthread_getschedparam(coreThread, &thePolicy, &sp);
	sp.sched_priority = sched_get_priority_max(thePolicy);
	pthread_setschedparam(coreThread, thePolicy, &sp);
	
	[cdsGPU setOutputList:cdsOutputList mutexPtr:&threadParam.mutexOutputList];
	
	OSXDriver *newDriver = new OSXDriver;
	newDriver->SetCoreThreadMutexLock(&threadParam.mutexThreadExecute);
	newDriver->SetCoreExecuteRWLock(self.rwlockCoreExecute);
	newDriver->SetCore(self);
	driver = newDriver;
	
	frameStatus = @"---";
	executionSpeedStatus = @"1.00x";
	errorStatus = @"";
	
	return self;
}

- (void)dealloc
{
	[self setCoreState:ExecutionBehavior_Pause];
	
	[self removeAllOutputs];
	
	[self setCdsController:nil];
	[self setCdsFirmware:nil];
	[self setCdsGPU:nil];
	[self setCdsOutputList:nil];
	[self setErrorStatus:nil];
	
	pthread_cancel(coreThread);
	pthread_join(coreThread, NULL);
	coreThread = NULL;
	
	pthread_mutex_destroy(&threadParam.mutexThreadExecute);
	pthread_cond_destroy(&threadParam.condThreadExecute);
	pthread_mutex_destroy(&threadParam.mutexOutputList);
	pthread_rwlock_destroy(&threadParam.rwlockCoreExecute);
	
	[self setIsGdbStubStarted:NO];
	
	delete execControl;
	NDS_DeInit();
	
	[super dealloc];
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
		emu_halt(EMUHALT_REASON_UNKNOWN, NDSErrorTag_BothCPUs);
	}
	
	OSSpinLockUnlock(&spinlockMasterExecute);
}

- (BOOL) masterExecute
{
	OSSpinLockLock(&spinlockMasterExecute);
	const BOOL theState = (execute) ? YES : NO;
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

- (void) setIsFrameSkipEnabled:(BOOL)enable
{
	execControl->SetEnableFrameSkip((enable) ? true : false);
}

- (BOOL) isFrameSkipEnabled
{
	const bool enable = execControl->GetEnableFrameSkip();
	return (enable) ? YES : NO;
}

- (void) setSpeedScalar:(CGFloat)scalar
{
	execControl->SetExecutionSpeed((float)scalar);
	[self updateExecutionSpeedStatus];
}

- (CGFloat) speedScalar
{
	const CGFloat scalar = (CGFloat)execControl->GetExecutionSpeed();
	return scalar;
}

- (void) setFrameJumpBehavior:(NSInteger)theBehavior
{
	execControl->SetFrameJumpBehavior((FrameJumpBehavior)theBehavior);
}

- (NSInteger) frameJumpBehavior
{
	const NSInteger theBehavior = (NSInteger)execControl->GetFrameJumpBehavior();
	return theBehavior;
}

- (void) setFrameJumpNumberFramesForward:(NSUInteger)numberFrames
{
	execControl->SetFrameJumpRelativeTarget((uint64_t)numberFrames);	
	[self setFrameJumpToFrameIndex:[self frameNumber] + numberFrames];
}

- (NSUInteger) frameJumpNumberFramesForward
{
	const NSUInteger numberFrames = execControl->GetFrameJumpRelativeTarget();
	return numberFrames;
}

- (void) setFrameJumpToFrameIndex:(NSUInteger)frameIndex
{
	execControl->SetFrameJumpTarget((uint64_t)frameIndex);
}

- (NSUInteger) frameJumpToFrameIndex
{
	const NSUInteger frameIndex = execControl->GetFrameJumpTarget();
	return frameIndex;
}

- (void) setIsSpeedLimitEnabled:(BOOL)enable
{
	execControl->SetEnableSpeedLimiter((enable) ? true : false);
	[self updateExecutionSpeedStatus];
}

- (BOOL) isSpeedLimitEnabled
{
	const bool enable = execControl->GetEnableSpeedLimiter();
	return (enable) ? YES : NO;
}

- (void) setIsCheatingEnabled:(BOOL)enable
{
	execControl->SetEnableCheats((enable) ? true : false);
}

- (BOOL) isCheatingEnabled
{
	const bool enable = execControl->GetEnableCheats();
	return (enable) ? YES : NO;
}

- (void) setIsGdbStubStarted:(BOOL)theState
{
#ifdef GDB_STUB
	if (theState)
	{
        gdbstub_mutex_init();
        
		if ([self enableGdbStubARM9])
		{
			const uint16_t arm9Port = (uint16_t)[self gdbStubPortARM9];
			if(arm9Port > 0)
			{
				gdbStubHandleARM9 = createStub_gdb(arm9Port, &NDS_ARM9, &arm9_direct_memory_iface);
				if (gdbStubHandleARM9 == NULL)
				{
					NSLog(@"Failed to create ARM9 gdbstub on port %d\n", arm9Port);
				}
				else
				{
					activateStub_gdb(gdbStubHandleARM9);
				}
			}
		}
		else
		{
			destroyStub_gdb(gdbStubHandleARM9);
			gdbStubHandleARM9 = NULL;
		}
		
		if ([self enableGdbStubARM7])
		{
			const uint16_t arm7Port = (uint16_t)[self gdbStubPortARM7];
			if (arm7Port > 0)
			{
				gdbStubHandleARM7 = createStub_gdb(arm7Port, &NDS_ARM7, &arm7_base_memory_iface);
				if (gdbStubHandleARM7 == NULL)
				{
					NSLog(@"Failed to create ARM7 gdbstub on port %d\n", arm7Port);
				}
				else
				{
					activateStub_gdb(gdbStubHandleARM7);
				}
			}
		}
		else
		{
			destroyStub_gdb(gdbStubHandleARM7);
			gdbStubHandleARM7 = NULL;
		}
	}
	else
	{
		destroyStub_gdb(gdbStubHandleARM9);
		gdbStubHandleARM9 = NULL;
		
		destroyStub_gdb(gdbStubHandleARM7);
		gdbStubHandleARM7 = NULL;

        gdbstub_mutex_destroy();
	}
#endif
	if (gdbStubHandleARM9 == NULL && gdbStubHandleARM7 == NULL)
	{
		theState = NO;
	}
	
	isGdbStubStarted = theState;
}

- (BOOL) isGdbStubStarted
{
	return isGdbStubStarted;
}

- (void) setIsInDebugTrap:(BOOL)theState
{
	// If we're transitioning out of the debug trap, then ignore
	// frame skipping this time.
	if (isInDebugTrap && !theState)
	{
		execControl->ResetFramesToSkip();
	}
	
	isInDebugTrap = theState;
}

- (BOOL) isInDebugTrap
{
	return isInDebugTrap;
}

- (void) setEmuFlagAdvancedBusLevelTiming:(BOOL)enable
{
	execControl->SetEnableAdvancedBusLevelTiming((enable) ? true : false);
}

- (BOOL) emuFlagAdvancedBusLevelTiming
{
	const bool enable = execControl->GetEnableAdvancedBusLevelTiming();
	return (enable) ? YES : NO;
}

- (void) setEmuFlagRigorousTiming:(BOOL)enable
{
	execControl->SetEnableRigorous3DRenderingTiming((enable) ? true : false);
}

- (BOOL) emuFlagRigorousTiming
{
	const bool enable = execControl->GetEnableRigorous3DRenderingTiming();
	return (enable) ? YES : NO;
}

- (void) setEmuFlagUseGameSpecificHacks:(BOOL)enable
{
	execControl->SetEnableGameSpecificHacks((enable) ? true : false);
}

- (BOOL) emuFlagUseGameSpecificHacks
{
	const bool enable = execControl->GetEnableGameSpecificHacks();
	return (enable) ? YES : NO;
}

- (void) setEmuFlagUseExternalBios:(BOOL)enable
{
	execControl->SetEnableExternalBIOS((enable) ? true : false);
}

- (BOOL) emuFlagUseExternalBios
{
	const bool enable = execControl->GetEnableExternalBIOS();
	return (enable) ? YES : NO;
}

- (void) setEmuFlagEmulateBiosInterrupts:(BOOL)enable
{
	execControl->SetEnableBIOSInterrupts((enable) ? true : false);
}

- (BOOL) emuFlagEmulateBiosInterrupts
{
	const bool enable = execControl->GetEnableBIOSInterrupts();
	return (enable) ? YES : NO;
}

- (void) setEmuFlagPatchDelayLoop:(BOOL)enable
{
	execControl->SetEnableBIOSPatchDelayLoop((enable) ? true : false);
}

- (BOOL) emuFlagPatchDelayLoop
{
	const bool enable = execControl->GetEnableBIOSPatchDelayLoop();
	return (enable) ? YES : NO;
}

- (void) setEmuFlagUseExternalFirmware:(BOOL)enable
{
	execControl->SetEnableExternalFirmware((enable) ? true : false);
}

- (BOOL) emuFlagUseExternalFirmware
{
	const bool enable = execControl->GetEnableExternalFirmware();
	return (enable) ? YES : NO;
}

- (void) setEmuFlagFirmwareBoot:(BOOL)enable
{
	execControl->SetEnableFirmwareBoot((enable) ? true : false);
}

- (BOOL) emuFlagFirmwareBoot
{
	const bool enable = execControl->GetEnableFirmwareBoot();
	return (enable) ? YES : NO;
}

- (void) setEmuFlagDebugConsole:(BOOL)enable
{
	execControl->SetEnableDebugConsole((enable) ? true : false);
}

- (BOOL) emuFlagDebugConsole
{
	const bool enable = execControl->GetEnableDebugConsole();
	return (enable) ? YES : NO;
}

- (void) setEmuFlagEmulateEnsata:(BOOL)enable
{
	execControl->SetEnableEnsataEmulation((enable) ? true : false);
}

- (BOOL) emuFlagEmulateEnsata
{
	const bool enable = execControl->GetEnableEnsataEmulation();
	return (enable) ? YES : NO;
}

- (void) setCpuEmulationEngine:(NSInteger)engineID
{
	execControl->SetCPUEmulationEngineByID((CPUEmulationEngineID)engineID);
}

- (NSInteger) cpuEmulationEngine
{
	return (NSInteger)execControl->GetCPUEmulationEngineID();
}

- (void) setMaxJITBlockSize:(NSInteger)blockSize
{
	execControl->SetJITMaxBlockSize((uint8_t)blockSize);
}

- (NSInteger) maxJITBlockSize
{
	return (NSInteger)execControl->GetJITMaxBlockSize();
}

- (void) setSlot1DeviceType:(NSInteger)theType
{
	execControl->SetSlot1DeviceByType((NDS_SLOT1_TYPE)theType);
}

- (NSInteger) slot1DeviceType
{
	return (NSInteger)execControl->GetSlot1DeviceType();
}

- (void) setCoreState:(NSInteger)coreState
{
	if (coreState == ExecutionBehavior_FrameJump)
	{
		uint64_t frameIndex = [self frameNumber];
		
		switch ([self frameJumpBehavior])
		{
			case FrameJumpBehavior_Forward:
				[self setFrameJumpToFrameIndex:[self frameJumpNumberFramesForward] + frameIndex];
				break;
				
			case FrameJumpBehavior_NextMarker:
				// TODO: Support frame jumping to replay markers.
				break;
				
			case FrameJumpBehavior_ToFrame:
			default:
				break;
		}
		
		uint64_t jumpTarget = [self frameJumpToFrameIndex];
		
		if (frameIndex >= jumpTarget)
		{
			return;
		}
	}
	
	pthread_mutex_lock(&threadParam.mutexThreadExecute);
	
	execControl->SetExecutionBehavior((ExecutionBehavior)coreState);
	
	pthread_mutex_lock(&threadParam.mutexOutputList);
	
	switch ((ExecutionBehavior)coreState)
	{
		case ExecutionBehavior_Pause:
		{
			for (CocoaDSOutput *cdsOutput in cdsOutputList)
			{
				[cdsOutput setIdle:YES];
			}
			
			[self setFrameStatus:[NSString stringWithFormat:@"%llu", (unsigned long long)[self frameNumber]]];
			[_fpsTimer invalidate];
			_fpsTimer = nil;
			break;
		}
			
		case ExecutionBehavior_FrameAdvance:
		{
			for (CocoaDSOutput *cdsOutput in cdsOutputList)
			{
				[cdsOutput setIdle:NO];
			}
			
			[self setFrameStatus:[NSString stringWithFormat:@"%llu", (unsigned long long)[self frameNumber]]];
			[_fpsTimer invalidate];
			_fpsTimer = nil;
			break;
		}
			
		case ExecutionBehavior_Run:
		{
			for (CocoaDSOutput *cdsOutput in cdsOutputList)
			{
				[cdsOutput setIdle:NO];
			}
			
			[self setFrameStatus:@"Executing..."];
			
			if (_fpsTimer == nil)
			{
				_isTimerAtSecond = NO;
				_fpsTimer = [[NSTimer alloc] initWithFireDate:[NSDate date]
													 interval:0.5
													   target:self
													 selector:@selector(getTimedEmulatorStatistics:)
													 userInfo:nil
													  repeats:YES];
				
				[[NSRunLoop currentRunLoop] addTimer:_fpsTimer forMode:NSRunLoopCommonModes];
			}
			break;
		}
			
		case ExecutionBehavior_FrameJump:
		{
			for (CocoaDSOutput *cdsOutput in cdsOutputList)
			{
				if (![cdsOutput isKindOfClass:[CocoaDSDisplay class]])
				{
					[cdsOutput setIdle:YES];
				}
			}
			
			[self setFrameStatus:[NSString stringWithFormat:@"Jumping to frame %llu.", (unsigned long long)execControl->GetFrameJumpTarget()]];
			[_fpsTimer invalidate];
			_fpsTimer = nil;
			break;
		}
			
		default:
			break;
	}
	
	pthread_mutex_unlock(&threadParam.mutexOutputList);
	
	pthread_cond_signal(&threadParam.condThreadExecute);
	pthread_mutex_unlock(&threadParam.mutexThreadExecute);
	
	[[self cdsGPU] respondToPauseState:(coreState == ExecutionBehavior_Pause)];
	[[self cdsController] setHardwareMicPause:(coreState != ExecutionBehavior_Run)];
}

- (NSInteger) coreState
{
	const NSInteger behavior = (NSInteger)execControl->GetExecutionBehavior();
	return behavior;
}

- (void) setArm9ImageURL:(NSURL *)fileURL
{
	const char *filePath = (fileURL != NULL) ? [[fileURL path] cStringUsingEncoding:NSUTF8StringEncoding] : NULL;
	execControl->SetARM9ImagePath(filePath);
}

- (NSURL *) arm9ImageURL
{
	const char *filePath = execControl->GetARM9ImagePath();
	return [NSURL fileURLWithPath:[NSString stringWithCString:filePath encoding:NSUTF8StringEncoding]];
}

- (void) setArm7ImageURL:(NSURL *)fileURL
{
	const char *filePath = (fileURL != NULL) ? [[fileURL path] cStringUsingEncoding:NSUTF8StringEncoding] : NULL;
	execControl->SetARM7ImagePath(filePath);
}

- (NSURL *) arm7ImageURL
{
	const char *filePath = execControl->GetARM7ImagePath();
	return [NSURL fileURLWithPath:[NSString stringWithCString:filePath encoding:NSUTF8StringEncoding]];
}

- (void) setFirmwareImageURL:(NSURL *)fileURL
{
	const char *filePath = (fileURL != NULL) ? [[fileURL path] cStringUsingEncoding:NSUTF8StringEncoding] : NULL;
	execControl->SetFirmwareImagePath(filePath);
}

- (NSURL *) firmwareImageURL
{
	const char *filePath = execControl->GetFirmwareImagePath();
	return [NSURL fileURLWithPath:[NSString stringWithCString:filePath encoding:NSUTF8StringEncoding]];
}

- (void) setSlot1R4URL:(NSURL *)fileURL
{
	const char *filePath = (fileURL != NULL) ? [[fileURL path] cStringUsingEncoding:NSUTF8StringEncoding] : NULL;
	execControl->SetSlot1R4Path(filePath);
}

- (NSURL *) slot1R4URL
{
	const char *filePath = execControl->GetSlot1R4Path();
	return [NSURL fileURLWithPath:[NSString stringWithCString:filePath encoding:NSUTF8StringEncoding]];
}

- (pthread_rwlock_t *) rwlockCoreExecute
{
	return &threadParam.rwlockCoreExecute;
}

- (BOOL) isSlot1Ejected
{
	const BOOL isEjected = (nds.cardEjected) ? YES : NO;
	return isEjected;
}

- (void) slot1Eject
{
	pthread_rwlock_wrlock(&threadParam.rwlockCoreExecute);
	NDS_TriggerCardEjectIRQ();
	pthread_rwlock_unlock(&threadParam.rwlockCoreExecute);
	
	[self setSlot1StatusText:NSSTRING_STATUS_SLOT1_NO_DEVICE];
}

- (void) changeRomSaveType:(NSInteger)saveTypeID
{
	pthread_rwlock_wrlock(&threadParam.rwlockCoreExecute);
	[CocoaDSRom changeRomSaveType:saveTypeID];
	pthread_rwlock_unlock(&threadParam.rwlockCoreExecute);
}

- (void) updateExecutionSpeedStatus
{
	if ([self isSpeedLimitEnabled])
	{
		[self setExecutionSpeedStatus:[NSString stringWithFormat:@"%1.2fx", [self speedScalar]]];
	}
	else
	{
		[self setExecutionSpeedStatus:@"Unlimited"];
	}
}

- (void) updateSlot1DeviceStatus
{
	const NDS_SLOT1_TYPE deviceTypeID = execControl->GetSlot1DeviceType();
	
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
}

- (void) restoreCoreState
{
	[self setCoreState:(NSInteger)execControl->GetPreviousExecutionBehavior()];
}

- (void) reset
{
	[self setCoreState:ExecutionBehavior_Pause];
	
	pthread_mutex_lock(&threadParam.mutexThreadExecute);
	execControl->ApplySettingsOnReset();
	NDS_Reset();
	pthread_mutex_unlock(&threadParam.mutexThreadExecute);
	
	[self updateSlot1DeviceStatus];
	[self setMasterExecute:YES];
	[self restoreCoreState];
	[[self cdsController] reset];
	[[self cdsController] updateMicLevel];
}

- (void) getTimedEmulatorStatistics:(NSTimer *)timer
{
	// The timer should fire every 0.5 seconds, so only take the frame
	// count every other instance the timer fires.
	_isTimerAtSecond = !_isTimerAtSecond;
	
	for (CocoaDSOutput *cdsOutput in cdsOutputList)
	{
		if ([cdsOutput isKindOfClass:[CocoaDSDisplay class]])
		{
			if (_isTimerAtSecond)
			{
				[(CocoaDSDisplay *)cdsOutput takeFrameCount];
			}
		}
	}
}

- (NSUInteger) frameNumber
{
	return (NSUInteger)execControl->GetFrameIndex();
}

- (void) addOutput:(CocoaDSOutput *)theOutput
{
	pthread_mutex_lock(&threadParam.mutexOutputList);
	
	if ([theOutput isKindOfClass:[CocoaDSDisplay class]])
	{
		[theOutput setRwlockProducer:[[self cdsGPU] gpuFrameRWLock]];
	}
	else
	{
		[theOutput setRwlockProducer:[self rwlockCoreExecute]];
	}
	
	[[self cdsOutputList] addObject:theOutput];
	pthread_mutex_unlock(&threadParam.mutexOutputList);
}

- (void) removeOutput:(CocoaDSOutput *)theOutput
{
	pthread_mutex_lock(&threadParam.mutexOutputList);
	[[self cdsOutputList] removeObject:theOutput];
	pthread_mutex_unlock(&threadParam.mutexOutputList);
}

- (void) removeAllOutputs
{
	pthread_mutex_lock(&threadParam.mutexOutputList);
	[[self cdsOutputList] removeAllObjects];
	pthread_mutex_unlock(&threadParam.mutexOutputList);
}

- (NSString *) cpuEmulationEngineString
{
	return [NSString stringWithCString:execControl->GetCPUEmulationEngineName() encoding:NSUTF8StringEncoding];
}

- (NSString *) slot1DeviceTypeString
{
	return [NSString stringWithCString:execControl->GetSlot1DeviceName() encoding:NSUTF8StringEncoding];
}

- (NSString *) slot2DeviceTypeString
{
	NSString *theString = @"Uninitialized";
	
	pthread_mutex_lock(&threadParam.mutexThreadExecute);
	
	if (slot2_device == NULL)
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

- (void) postNDSError:(const NDSError &)ndsError
{
	NSString *newErrorString = nil;
	
	switch (ndsError.code)
	{
		case NDSError_NoError:
			// Do nothing if there is no error.
			return;
			
		case NDSError_SystemPoweredOff:
			newErrorString = @"The system powered off using the ARM7 SPI device.";
			break;
			
		case NDSError_JITUnmappedAddressException:
		{
			if (ndsError.tag == NDSErrorTag_ARM9)
			{
				newErrorString = [NSString stringWithFormat:
@"JIT UNMAPPED ADDRESS EXCEPTION - ARM9:\n\
\tARM9 Program Counter:     0x%08X\n\
\tARM9 Instruction:         0x%08X\n\
\tARM9 Instruction Address: 0x%08X",
							  ndsError.programCounterARM9, ndsError.instructionARM9, ndsError.instructionAddrARM9];
			}
			else if (ndsError.tag == NDSErrorTag_ARM7)
			{
				newErrorString = [NSString stringWithFormat:
@"JIT UNMAPPED ADDRESS EXCEPTION - ARM7:\n\
\tARM7 Program Counter:     0x%08X\n\
\tARM7 Instruction:         0x%08X\n\
\tARM7 Instruction Address: 0x%08X",
							  ndsError.programCounterARM7, ndsError.instructionARM7, ndsError.instructionAddrARM7];
			}
			else
			{
				newErrorString = [NSString stringWithFormat:
@"JIT UNMAPPED ADDRESS EXCEPTION - UNKNOWN CPU:\n\
\tARM9 Program Counter:     0x%08X\n\
\tARM9 Instruction:         0x%08X\n\
\tARM9 Instruction Address: 0x%08X\n\
\tARM7 Program Counter:     0x%08X\n\
\tARM7 Instruction:         0x%08X\n\
\tARM7 Instruction Address: 0x%08X",
							  ndsError.programCounterARM9, ndsError.instructionARM9, ndsError.instructionAddrARM9,
							  ndsError.programCounterARM7, ndsError.instructionARM7, ndsError.instructionAddrARM7];
			}
			break;
		}
			
		case NDSError_ARMUndefinedInstructionException:
		{
			if (ndsError.tag == NDSErrorTag_ARM9)
			{
				newErrorString = [NSString stringWithFormat:
@"ARM9 UNDEFINED INSTRUCTION EXCEPTION:\n\
\tARM9 Program Counter:     0x%08X\n\
\tARM9 Instruction:         0x%08X\n\
\tARM9 Instruction Address: 0x%08X",
							  ndsError.programCounterARM9, ndsError.instructionARM9, ndsError.instructionAddrARM9];
			}
			else if (ndsError.tag == NDSErrorTag_ARM7)
			{
				newErrorString = [NSString stringWithFormat:
@"ARM7 UNDEFINED INSTRUCTION EXCEPTION:\n\
\tARM7 Program Counter:     0x%08X\n\
\tARM7 Instruction:         0x%08X\n\
\tARM7 Instruction Address: 0x%08X",
							  ndsError.programCounterARM7, ndsError.instructionARM7, ndsError.instructionAddrARM7];
			}
			else
			{
				newErrorString = [NSString stringWithFormat:
@"UNKNOWN ARM CPU UNDEFINED INSTRUCTION EXCEPTION:\n\
\tARM9 Program Counter:     0x%08X\n\
\tARM9 Instruction:         0x%08X\n\
\tARM9 Instruction Address: 0x%08X\n\
\tARM7 Program Counter:     0x%08X\n\
\tARM7 Instruction:         0x%08X\n\
\tARM7 Instruction Address: 0x%08X",
							  ndsError.programCounterARM9, ndsError.instructionARM9, ndsError.instructionAddrARM9,
							  ndsError.programCounterARM7, ndsError.instructionARM7, ndsError.instructionAddrARM7];
			}
			break;
		}
			
		case NDSError_UnknownError:
		default:
			newErrorString = [NSString stringWithFormat:
@"UNKNOWN ERROR:\n\
\tARM9 Program Counter:     0x%08X\n\
\tARM9 Instruction:         0x%08X\n\
\tARM9 Instruction Address: 0x%08X\n\
\tARM7 Program Counter:     0x%08X\n\
\tARM7 Instruction:         0x%08X\n\
\tARM7 Instruction Address: 0x%08X",
							  ndsError.programCounterARM9, ndsError.instructionARM9, ndsError.instructionAddrARM9,
							  ndsError.programCounterARM7, ndsError.instructionARM7, ndsError.instructionAddrARM7];
			break;
	}
	
	[self setErrorStatus:newErrorString];
	[[NSNotificationCenter defaultCenter] postNotificationOnMainThreadName:@"org.desmume.DeSmuME.handleNDSError" object:self userInfo:nil];
}

@end

static void* RunCoreThread(void *arg)
{
	CoreThreadParam *param = (CoreThreadParam *)arg;
	CocoaDSCore *cdsCore = (CocoaDSCore *)param->cdsCore;
	ClientExecutionControl *execControl = [cdsCore execControl];
	ClientInputHandler *inputHandler = execControl->GetClientInputHandler();
	NSMutableArray *cdsOutputList = [cdsCore cdsOutputList];
	const NDSFrameInfo &ndsFrameInfo = execControl->GetNDSFrameInfo();
	
	double startTime = 0;
	double frameTime = 0; // The amount of time that is expected for the frame to run.
	
	ExecutionBehavior behavior = ExecutionBehavior_Pause;
	uint64_t frameJumpTarget = 0;
	
	do
	{
		startTime = execControl->GetCurrentAbsoluteTime();
		
		pthread_mutex_lock(&param->mutexThreadExecute);
		execControl->ApplySettingsOnExecutionLoopStart();
		behavior = execControl->GetExecutionBehaviorApplied();
		
		while (!(behavior != ExecutionBehavior_Pause && execute))
		{
			pthread_cond_wait(&param->condThreadExecute, &param->mutexThreadExecute);
			
			startTime = execControl->GetCurrentAbsoluteTime();
			execControl->ApplySettingsOnExecutionLoopStart();
			behavior = execControl->GetExecutionBehaviorApplied();
		}
		
		frameTime = execControl->GetFrameTime();
		frameJumpTarget = execControl->GetFrameJumpTargetApplied();
		
		inputHandler->ProcessInputs();
		inputHandler->ApplyInputs();
		execControl->ApplySettingsOnNDSExec();
		
		// Execute the frame and increment the frame counter.
		pthread_rwlock_wrlock(&param->rwlockCoreExecute);
		NDS_exec<false>();
		SPU_Emulate_user();
		execControl->FetchOutputPostNDSExec();
		pthread_rwlock_unlock(&param->rwlockCoreExecute);
		
		// Check if an internal execution error occurred that halted the emulation.
		if (!execute)
		{
			NDSError ndsError = NDS_GetLastError();
			pthread_mutex_unlock(&param->mutexThreadExecute);
			
			[cdsCore postNDSError:ndsError];
			continue;
		}
		
		// Make sure that the mic level is updated at least once every 8 frames, regardless
		// of whether the NDS actually reads the mic or not.
		if ((ndsFrameInfo.frameIndex & 0x07) == 0x07)
		{
			CocoaDSController *cdsController = [cdsCore cdsController];
			[cdsController updateMicLevel];
			[cdsController clearMicLevelMeasure];
		}
		
		const uint8_t framesToSkip = execControl->GetFramesToSkip();
		
		pthread_mutex_lock(&param->mutexOutputList);
		
		switch (behavior)
		{
			case ExecutionBehavior_Run:
			case ExecutionBehavior_FrameAdvance:
			case ExecutionBehavior_FrameJump:
			{
				for (CocoaDSOutput *cdsOutput in cdsOutputList)
				{
					if ([cdsOutput isKindOfClass:[CocoaDSDisplay class]])
					{
						[(CocoaDSDisplay *)cdsOutput setNDSFrameInfo:ndsFrameInfo];
					}
					
					if ( ![cdsOutput isKindOfClass:[CocoaDSSpeaker class]] && (![cdsOutput isKindOfClass:[CocoaDSDisplay class]] || (framesToSkip == 0)) )
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
		
		switch (behavior)
		{
			case ExecutionBehavior_Run:
			{
				if (execControl->GetEnableFrameSkipApplied())
				{
					if (framesToSkip > 0)
					{
						NDS_SkipNextFrame();
						execControl->SetFramesToSkip(framesToSkip - 1);
					}
					else
					{
						execControl->SetFramesToSkip( execControl->CalculateFrameSkip(startTime, frameTime) );
					}
				}
				break;
			}
			
			case ExecutionBehavior_FrameJump:
			{
				if (framesToSkip > 0)
				{
					NDS_SkipNextFrame();
					execControl->SetFramesToSkip(framesToSkip - 1);
				}
				else
				{
					execControl->SetFramesToSkip( (uint8_t)((DS_FRAMES_PER_SECOND * 1.0) + 0.85) );
				}
				break;
			}
				
			default:
				break;
		}
		
		pthread_mutex_unlock(&param->mutexThreadExecute);
		
		// If we're doing a frame advance, switch back to pause state immediately
		// after we're done with the frame.
		if (behavior == ExecutionBehavior_FrameAdvance)
		{
			[cdsCore setCoreState:ExecutionBehavior_Pause];
		}
		else if (behavior == ExecutionBehavior_FrameJump)
		{
			if (ndsFrameInfo.frameIndex == (frameJumpTarget - 1))
			{
				execControl->ResetFramesToSkip();
			}
			else if (ndsFrameInfo.frameIndex >= frameJumpTarget)
			{
				[cdsCore restoreCoreState];
			}
		}
		else
		{
			// If there is any time left in the loop, go ahead and pad it.
			execControl->WaitUntilAbsoluteTime(startTime + frameTime);
		}
		
	} while(true);
	
	return NULL;
}

#pragma mark - OSXDriver

pthread_mutex_t* OSXDriver::GetCoreThreadMutexLock()
{
	return this->mutexThreadExecute;
}

void OSXDriver::SetCoreThreadMutexLock(pthread_mutex_t *theMutex)
{
	this->mutexThreadExecute = theMutex;
}

pthread_rwlock_t* OSXDriver::GetCoreExecuteRWLock()
{
	return this->rwlockCoreExecute;
}

void OSXDriver::SetCoreExecuteRWLock(pthread_rwlock_t *theRwLock)
{
	this->rwlockCoreExecute = theRwLock;
}

void OSXDriver::SetCore(CocoaDSCore *theCore)
{
	this->cdsCore = theCore;
}

void OSXDriver::EMU_DebugIdleEnter()
{
	[this->cdsCore setIsInDebugTrap:YES];
	pthread_rwlock_unlock(this->rwlockCoreExecute);
	pthread_mutex_unlock(this->mutexThreadExecute);
}

void OSXDriver::EMU_DebugIdleUpdate()
{
	usleep(50);
}

void OSXDriver::EMU_DebugIdleWakeUp()
{
	pthread_mutex_lock(this->mutexThreadExecute);
	pthread_rwlock_wrlock(this->rwlockCoreExecute);
	[this->cdsCore setIsInDebugTrap:NO];
}

#pragma mark - GDB Stub implementation

void* createThread_gdb(void (*thread_function)(void *data), void *thread_data)
{
	// Create the thread using POSIX routines.
	pthread_attr_t  attr;
	pthread_t*      posixThreadID = (pthread_t*)malloc(sizeof(pthread_t));
	
	assert(!pthread_attr_init(&attr));
	assert(!pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE));
	
	int threadError = pthread_create(posixThreadID, &attr, (void* (*)(void *))thread_function, thread_data);
	
	assert(!pthread_attr_destroy(&attr));
	
	if (threadError != 0)
	{
		// Report an error.
		return NULL;
	}
	else
	{
		return posixThreadID;
	}
}

void joinThread_gdb(void *thread_handle)
{
	pthread_join(*(pthread_t *)thread_handle, NULL);
	free(thread_handle);
}
