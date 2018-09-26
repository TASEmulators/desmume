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
