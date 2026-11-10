/*
	Copyright (C) 2018 DeSmuME team
 
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

#include <unistd.h>

#include "macOS_driver.h"
#include "ClientAVCaptureObject.h"
#include "ClientExecutionControl.h"
#include "ClientVideoOutput.h"
#include "NDSSystem.h"
#include "SPU.h"
#ifdef HAVE_LUA
#include "lua-engine.h"
#include "MacLuaScriptConsole.h"
#include "MacLuaFrontend.h"
#endif
#ifdef DESMUME_COCOA_METAL_OSD
#include "userinterface/MacMetalOSDDraw.h"
#endif


macOS_driver::macOS_driver()
	: __mutexThreadExecute(NULL)
	, __rwlockCoreExecute(NULL)
	, __execControl(NULL)
	, __displayOutputManager(NULL)
{
#ifdef HAVE_LUA
	pthread_mutex_init(&__mutexQueue, NULL);
	__queuedScriptUid = 0;
	__hasQueuedScript = false;
#endif
}

macOS_driver::~macOS_driver()
{
#ifdef HAVE_LUA
	pthread_mutex_destroy(&__mutexQueue);
#endif
}

#ifdef HAVE_LUA
void macOS_driver::QueueScript(intptr_t uid, const char *filename)
{
	pthread_mutex_lock(&__mutexQueue);
	__queuedScriptUid = uid;
	__queuedScriptFile = filename ? filename : "";
	__hasQueuedScript = true;
	pthread_mutex_unlock(&__mutexQueue);
}

bool macOS_driver::GetQueuedScript(intptr_t &uid, std::string &filename)
{
	bool result = false;
	pthread_mutex_lock(&__mutexQueue);
	if (__hasQueuedScript)
	{
		uid = __queuedScriptUid;
		filename = __queuedScriptFile;
		__hasQueuedScript = false;
		result = true;
	}
	pthread_mutex_unlock(&__mutexQueue);
	return result;
}
#endif

pthread_mutex_t* macOS_driver::GetCoreThreadMutexLock()
{
	return this->__mutexThreadExecute;
}

void macOS_driver::SetCoreThreadMutexLock(pthread_mutex_t *theMutex)
{
	this->__mutexThreadExecute = theMutex;
}

pthread_rwlock_t* macOS_driver::GetCoreExecuteRWLock()
{
	return this->__rwlockCoreExecute;
}

void macOS_driver::SetCoreExecuteRWLock(pthread_rwlock_t *theRwLock)
{
	this->__rwlockCoreExecute = theRwLock;
}

void macOS_driver::SetExecutionControl(ClientExecutionControl *execControl)
{
	this->__execControl = execControl;
}

void macOS_driver::AVI_SoundUpdate(void *soundData, int soundLen)
{
	ClientAVCaptureObject *avCaptureObject = this->__execControl->GetClientAVCaptureObjectApplied();
	
	if ((avCaptureObject == NULL) || !avCaptureObject->IsCapturingAudio())
	{
		return;
	}
	
	avCaptureObject->CaptureAudioFrames(soundData, soundLen);
}

bool macOS_driver::AVI_IsRecording()
{
	ClientAVCaptureObject *avCaptureObject = this->__execControl->GetClientAVCaptureObjectApplied();
	return ((avCaptureObject != NULL) && avCaptureObject->IsCapturingVideo());
}

bool macOS_driver::WAV_IsRecording()
{
	ClientAVCaptureObject *avCaptureObject = this->__execControl->GetClientAVCaptureObjectApplied();
	return ((avCaptureObject != NULL) && avCaptureObject->IsCapturingAudio());
}

void macOS_driver::EMU_DebugIdleEnter()
{
	this->__execControl->SetIsInDebugTrap(true);
	pthread_rwlock_unlock(this->__rwlockCoreExecute);
	pthread_mutex_unlock(this->__mutexThreadExecute);
}

void macOS_driver::EMU_DebugIdleUpdate()
{
	usleep(50);
}

void macOS_driver::EMU_DebugIdleWakeUp()
{
	pthread_mutex_lock(this->__mutexThreadExecute);
	pthread_rwlock_wrlock(this->__rwlockCoreExecute);
	this->__execControl->SetIsInDebugTrap(false);
}

void macOS_driver::SetDisplayOutputManager(ClientDisplayViewOutputManager *displayOutputManager)
{
	this->__displayOutputManager = displayOutputManager;
}

extern "C" void macOS_PumpEvents();

void macOS_driver::USR_InfoMessage(const char *message)
{
	// Windows shows these through OSDCLASS (osd->addLine).  Cocoa has no
	// OSDCLASS, so hand the text to the Lua frontend, which keeps it around
	// long enough for the frame loop to paint it into the Metal OSD.
	if (message == NULL)
		return;

	LOG("%s\n", message);
#ifdef HAVE_LUA
	MacLua_ShowOsdMessage(message);
#endif
}

// Frame pacing for Lua-driven frames (emu.frameadvance()).  Only the core
// thread reaches here, so these need no lock.  See the comment in
// EMU_StepMainLoop() for why this is an accumulating absolute deadline.
static double sLuaFrameDeadline = 0.0;
static double sLuaFrameDuration = 0.0;

BaseDriver::eStepMainLoopResult macOS_driver::EMU_StepMainLoop(bool allowSleep, bool allowPause, int frameSkip, bool disableUser, bool disableCore)
{
	if (this->__execControl == NULL)
	{
		return ESTEP_DONE;
	}

	// We are already on the core thread, and the core thread already holds the write lock
	// on rwlockCoreExecute and the mutex lock on mutexThreadExecute.
	// Therefore, we must NOT lock them again here.

	// allowSleep means "apply the speed limiter to this frame" (see
	// StepEmulationAtSpeed() and Windows' StepRunLoop_Throttle()).  The outer
	// RunCoreThread loop in cocoa_core.mm paces itself with
	// WaitUntilAbsoluteTime(startTime + frameTime * executionWaitBias), but it
	// is blocked in RunLuaScriptFile() while a script drives frames, so this
	// step loop is the only throttle that runs.
	//
	// The old implementation slept a fixed 16666 us AFTER doing the frame's
	// work, making the period (emulation work + 16.666 ms); a long usleep also
	// overshoots by ~3 ms on macOS, so even an empty script dropped from 60 to
	// ~45 FPS.  GetFrameTime() is in mach_absolute_time units, honours the
	// speed setting, and is 0.0 when the speed limiter is disabled.
	const double frameStartTime = this->__execControl->GetCurrentAbsoluteTime();
	double frameDuration = allowSleep ? this->__execControl->GetFrameTime() : 0.0;
	if (allowSleep && frameDuration <= 0.0 && this->__execControl->GetEnableSpeedLimiterApplied())
	{
		// Settings are normally applied by the outer loop, which is blocked
		// while a script runs.  Don't fall back to an unthrottled loop if the
		// cached frame time happens to be missing.
		frameDuration = this->__execControl->CalculateFrameAbsoluteTime(1.0);
	}

	if (!disableCore)
	{
		// Apply the user's controller/touch input for this frame. The normal core
		// loop does this every frame; without it, a Lua script driving execution
		// via emu.frameadvance() would leave the game unable to receive input.
		ClientInputHandler *inputHandler = this->__execControl->GetClientInputHandler();
		if (inputHandler != NULL)
		{
			inputHandler->ProcessInputs();
			inputHandler->ApplyInputs();
		}

#ifdef DESMUME_COCOA_METAL_OSD
		MacMetalOSD_ClearBack();
#endif
#ifdef HAVE_LUA
		lua_script_clear_graphics_buffer();
		NDS_beginProcessingInput();
		CallRegisteredLuaFunctions(LUACALL_BEFOREEMULATION);
		NDS_endProcessingInput();
#endif

		NDS_exec<false>();
		SPU_Emulate_user();
		this->__execControl->FetchOutputPostNDSExec();

#ifdef HAVE_LUA
		CallRegisteredLuaFunctions(LUACALL_AFTEREMULATION);
		// Flush GUI draw calls deferred from the script body (gui.drawbox, etc.),
		// then publish the completed overlay frame to the renderer.
		CallRegisteredLuaFunctions(LUACALL_AFTEREMULATIONGUI);
		lua_script_present_graphics_buffer();
#endif
#ifdef DESMUME_COCOA_METAL_OSD
		MacMetalOSD_PresentBack();
#endif
	}

	if (!disableUser && this->__displayOutputManager != NULL)
	{
		this->__displayOutputManager->SetNDSFrameInfoToAll(this->__execControl->GetNDSFrameInfo());
	}

	// Yield the locks so that the display views (which run on separate threads
	// and need the read lock to access framebuffer data) and the UI thread can run.
	if (this->__rwlockCoreExecute != NULL)
	{
		pthread_rwlock_unlock(this->__rwlockCoreExecute);
	}
	if (this->__mutexThreadExecute != NULL)
	{
		pthread_mutex_unlock(this->__mutexThreadExecute);
	}

	if (frameDuration > 0.0 && !disableCore)
	{
		// Keep an ideal absolute schedule instead of waiting "one frame from
		// now": if the timer wakes us late, the next deadline is unchanged and
		// the following wait is automatically shorter.  That compensates for
		// the timer's systematic overshoot without the 64-frame feedback loop
		// the outer core thread uses.
		const double now = this->__execControl->GetCurrentAbsoluteTime();
		if (sLuaFrameDeadline == 0.0 ||
			sLuaFrameDuration != frameDuration ||
			sLuaFrameDeadline < now - frameDuration ||
			sLuaFrameDeadline > now + frameDuration)
		{
			// First frame, a speed change, or a long stall (script paused,
			// debugger, UI event): re-anchor on this frame's start.
			sLuaFrameDeadline = frameStartTime + frameDuration;
			sLuaFrameDuration = frameDuration;
		}
		else
		{
			sLuaFrameDeadline += frameDuration;
		}

		if (now < sLuaFrameDeadline)
		{
			this->__execControl->WaitUntilAbsoluteTime(sLuaFrameDeadline);
		}
	}
	else
	{
		// Fast-forward / emu.wait(): don't throttle, but still yield so the
		// display views can take the read lock we just released.
		sLuaFrameDeadline = 0.0;
		sLuaFrameDuration = 0.0;
		usleep(1);
	}

	// Re-acquire the locks before returning control to the Lua engine.
	if (this->__rwlockCoreExecute != NULL)
	{
		pthread_rwlock_wrlock(this->__rwlockCoreExecute);
	}
	if (this->__mutexThreadExecute != NULL)
	{
		pthread_mutex_lock(this->__mutexThreadExecute);
	}

	return ESTEP_DONE;
}
