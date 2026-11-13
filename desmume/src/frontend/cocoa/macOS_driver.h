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

#ifndef _MACOS_DRIVER_H_
#define _MACOS_DRIVER_H_

#include <pthread.h>

#include "../../driver.h"

#ifdef HAVE_LUA
#include <string>
#endif

class ClientExecutionControl;

class ClientDisplayViewOutputManager;

class macOS_driver : public BaseDriver
{
private:
	pthread_mutex_t *__mutexThreadExecute;
	pthread_rwlock_t *__rwlockCoreExecute;
	ClientExecutionControl *__execControl;
	ClientDisplayViewOutputManager *__displayOutputManager;
	
#ifdef HAVE_LUA
	pthread_mutex_t __mutexQueue;
	std::string __queuedScriptFile;
	int __queuedScriptUid;
	bool __hasQueuedScript;
#endif
	
public:
	macOS_driver();
	virtual ~macOS_driver();
	
	pthread_mutex_t* GetCoreThreadMutexLock();
	void SetCoreThreadMutexLock(pthread_mutex_t *theMutex);
	pthread_rwlock_t* GetCoreExecuteRWLock();
	void SetCoreExecuteRWLock(pthread_rwlock_t *theRwLock);
	void SetExecutionControl(ClientExecutionControl *execControl);
	void SetDisplayOutputManager(ClientDisplayViewOutputManager *displayOutputManager);
	
#ifdef HAVE_LUA
	void QueueScript(int uid, const char *filename);
	bool GetQueuedScript(int &uid, std::string &filename);
#endif
	
	virtual void AVI_SoundUpdate(void *soundData, int soundLen);
	virtual bool AVI_IsRecording();
	virtual bool WAV_IsRecording();
	
	virtual eStepMainLoopResult EMU_StepMainLoop(bool allowSleep, bool allowPause, int frameSkip, bool disableUser, bool disableCore);
	virtual void EMU_DebugIdleEnter();
	virtual void EMU_DebugIdleUpdate();
	virtual void EMU_DebugIdleWakeUp();
};


#endif // _MACOS_DRIVER_H_
