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
void macOS_driver::QueueScript(int uid, const char *filename)
{
	pthread_mutex_lock(&__mutexQueue);
	__queuedScriptUid = uid;
	__queuedScriptFile = filename ? filename : "";
	__hasQueuedScript = true;
	pthread_mutex_unlock(&__mutexQueue);
}

bool macOS_driver::GetQueuedScript(int &uid, std::string &filename)
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

BaseDriver::eStepMainLoopResult macOS_driver::EMU_StepMainLoop(bool allowSleep, bool allowPause, int frameSkip, bool disableUser, bool disableCore)
{
	if (this->__execControl == NULL)
	{
		return ESTEP_DONE;
	}

	// We are already on the core thread, and the core thread already holds the write lock
	// on rwlockCoreExecute and the mutex lock on mutexThreadExecute.
	// Therefore, we must NOT lock them again here.

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

	if (allowSleep)
	{
		usleep(16666);
	}
	else
	{
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
