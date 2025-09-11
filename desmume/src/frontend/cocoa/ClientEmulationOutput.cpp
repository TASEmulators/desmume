/*
	Copyright (C) 2025 DeSmuME team

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

#include "ClientEmulationOutput.h"
#include "cocoa_globals.h"

#ifdef __APPLE__
#include <AvailabilityMacros.h>
#include <CoreFoundation/CoreFoundation.h>
#include <pthread.h>
#endif

#include "../../NDSSystem.h"

ClientEmulationOutput::ClientEmulationOutput()
{
	_clientObject = NULL;
	_isIdle = true;
	_platformId = 0;
	_typeId = 0;
	_rwlockProducer = NULL;
}

ClientEmulationOutput::~ClientEmulationOutput()
{
	// Do nothing.
}

void* ClientEmulationOutput::GetClientObject()
{
	return this->_clientObject;
}

void ClientEmulationOutput::SetClientObject(void *theObject)
{
	this->_clientObject = theObject;
}

void ClientEmulationOutput::SetIdle(bool theState)
{
	this->_isIdle = theState;
}

bool ClientEmulationOutput::IsIdle()
{
	return this->_isIdle;
}

void ClientEmulationOutput::SetPlatformId(int32_t platformId)
{
	this->_platformId = platformId;
}

int32_t ClientEmulationOutput::GetPlatformId()
{
	return this->_platformId;
}

void ClientEmulationOutput::SetTypeId(int32_t typeId)
{
	this->_typeId = typeId;
}

int32_t ClientEmulationOutput::GetTypeId()
{
	return this->_typeId;
}

pthread_rwlock_t* ClientEmulationOutput::GetProducerRWLock()
{
	return this->_rwlockProducer;
}

void ClientEmulationOutput::SetProducerRWLock(pthread_rwlock_t *rwlockProducer)
{
	this->_rwlockProducer = rwlockProducer;
}

void ClientEmulationOutput::Dispatch(int32_t messageID)
{
	this->_HandleDispatch(messageID);
}

void ClientEmulationOutput::_HandleDispatch(int32_t messageID)
{
	switch (messageID)
	{
		case MESSAGE_EMU_FRAME_PROCESSED:
			this->HandleEmuFrameProcessed();
			break;
			
		default:
			break;
	}
}

void ClientEmulationOutput::HandleEmuFrameProcessed()
{
	// Do nothing. This is method is implementation dependent.
}

static void* RunOutputThread(void *arg)
{
	ClientEmulationThreadedOutput *output = (ClientEmulationThreadedOutput *)arg;
	
#if defined(MAC_OS_X_VERSION_10_6) && (MAC_OS_X_VERSION_MAX_ALLOWED >= MAC_OS_X_VERSION_10_6)
	if ( !output->IsPThreadNameFlagSet() && (kCFCoreFoundationVersionNumber >= kCFCoreFoundationVersionNumber10_6) )
	{
		pthread_setname_np(output->GetThreadName());
		output->SetFlagPThreadName(true);
	}
#endif
	
	output->RunLoop();
	
	return NULL;
}

ClientEmulationThreadedOutput::ClientEmulationThreadedOutput()
{
	_threadName = "EmulationOutputNew";
	_isPThreadNameSet = false;
	_threadMessageID = MESSAGE_NONE;
	_pthread = NULL;
}

ClientEmulationThreadedOutput::~ClientEmulationThreadedOutput()
{
	this->ExitThread();
}

const char* ClientEmulationThreadedOutput::GetThreadName() const
{
	return this->_threadName.c_str();
}

void ClientEmulationThreadedOutput::SetThreadName(const char *theString)
{
	this->_threadName = theString;
	this->SetFlagPThreadName(false);
}

void ClientEmulationThreadedOutput::SetFlagPThreadName(bool isFlagSet)
{
	this->_isPThreadNameSet = isFlagSet;
}

bool ClientEmulationThreadedOutput::IsPThreadNameFlagSet() const
{
	return this->_isPThreadNameSet;
}

void ClientEmulationThreadedOutput::CreateThread()
{
	pthread_mutex_init(&this->_mutexMessageLoop, NULL);
	pthread_cond_init(&this->_condSignalMessage, NULL);
	
	if (CommonSettings.num_cores > 1)
	{
		pthread_attr_t threadAttr;
		pthread_attr_init(&threadAttr);
		pthread_attr_setschedpolicy(&threadAttr, SCHED_RR);
		
		struct sched_param sp;
		memset(&sp, 0, sizeof(struct sched_param));
		sp.sched_priority = 45;
		pthread_attr_setschedparam(&threadAttr, &sp);
		
		pthread_create(&this->_pthread, &threadAttr, &RunOutputThread, this);
		pthread_attr_destroy(&threadAttr);
	}
	else
	{
		pthread_create(&_pthread, NULL, &RunOutputThread, this);
	}
}

void ClientEmulationThreadedOutput::ExitThread()
{
	pthread_mutex_lock(&this->_mutexMessageLoop);
	
	if (this->_pthread != NULL)
	{
		this->_threadMessageID = MESSAGE_NONE;
		pthread_mutex_unlock(&this->_mutexMessageLoop);
		
		pthread_cancel(this->_pthread);
		pthread_join(this->_pthread, NULL);
		this->_pthread = NULL;
		
		pthread_cond_destroy(&this->_condSignalMessage);
		pthread_mutex_destroy(&this->_mutexMessageLoop);
	}
	else
	{
		pthread_mutex_unlock(&this->_mutexMessageLoop);
	}
}

void ClientEmulationThreadedOutput::RunLoop()
{
	pthread_mutex_lock(&this->_mutexMessageLoop);
	
	do
	{
		while (this->_threadMessageID == MESSAGE_NONE)
		{
			pthread_cond_wait(&this->_condSignalMessage, &this->_mutexMessageLoop);
		}
		
		this->_HandleDispatch(this->_threadMessageID);
		this->_threadMessageID = MESSAGE_NONE;
	} while(true);
}

void ClientEmulationThreadedOutput::Dispatch(int32_t messageID)
{
	pthread_mutex_lock(&this->_mutexMessageLoop);
	
	this->_threadMessageID = messageID;
	pthread_cond_signal(&this->_condSignalMessage);
	
	pthread_mutex_unlock(&this->_mutexMessageLoop);
}

ClientEmulationOutputManager::ClientEmulationOutputManager()
{
	_runList.clear();
	_pendingRegisterList.clear();
	_pendingUnregisterList.clear();
	
	pthread_mutex_init(&_pendingRegisterListMutex, NULL);
	pthread_mutex_init(&_pendingUnregisterListMutex, NULL);
}

ClientEmulationOutputManager::~ClientEmulationOutputManager()
{
	pthread_mutex_destroy(&this->_pendingRegisterListMutex);
	pthread_mutex_destroy(&this->_pendingUnregisterListMutex);
}

void ClientEmulationOutputManager::SetIdleStatePlatformAndType(int32_t platformId, int32_t typeId, bool idleState)
{
	// Add all pending outputs to be registered.
	pthread_mutex_lock(&this->_pendingRegisterListMutex);
	this->_RegisterPending();
	pthread_mutex_unlock(&this->_pendingRegisterListMutex);
	
	// Remove all pending outputs to be unregistered.
	pthread_mutex_lock(&this->_pendingUnregisterListMutex);
	this->_UnregisterPending();
	
	// Finally, run the outputs.
	for (size_t i = 0; i < this->_runList.size(); i++)
	{
		ClientEmulationOutput *runningOutput = this->_runList[i];
		
		if ( ((runningOutput->GetPlatformId() & platformId) != 0) && ((runningOutput->GetTypeId() & typeId) != 0) )
		{
			runningOutput->SetIdle(idleState);
		}
	}
	
	pthread_mutex_unlock(&this->_pendingUnregisterListMutex);
}

void ClientEmulationOutputManager::SetIdleStateAll(bool idleState)
{
	this->SetIdleStatePlatformAndType(PlatformTypeID_Any, OutputTypeID_Any, idleState);
}

void ClientEmulationOutputManager::Register(ClientEmulationOutput *theOutput, pthread_rwlock_t *rwlockProducer)
{
	if (theOutput == NULL)
	{
		return;
	}
	
	pthread_mutex_lock(&this->_pendingRegisterListMutex);
	theOutput->SetProducerRWLock(rwlockProducer);
	this->_pendingRegisterList.push_back(theOutput);
	pthread_mutex_unlock(&this->_pendingRegisterListMutex);
}

void ClientEmulationOutputManager::Unregister(ClientEmulationOutput *theOutput)
{
	if (theOutput == NULL)
	{
		return;
	}
	
	pthread_mutex_lock(&this->_pendingUnregisterListMutex);
	this->_pendingUnregisterList.push_back(theOutput);
	pthread_mutex_unlock(&this->_pendingUnregisterListMutex);
}

void ClientEmulationOutputManager::UnregisterAtIndex(size_t index)
{
	pthread_mutex_lock(&this->_pendingUnregisterListMutex);
	
	if (index >= this->_runList.size())
	{
		pthread_mutex_unlock(&this->_pendingUnregisterListMutex);
		return;
	}
	
	this->_pendingUnregisterList.push_back( this->_runList[index] );
	pthread_mutex_unlock(&this->_pendingUnregisterListMutex);
}

void ClientEmulationOutputManager::UnregisterAll()
{
	pthread_mutex_lock(&this->_pendingUnregisterListMutex);
	this->_pendingUnregisterList.clear();
	this->_runList.clear();
	pthread_mutex_unlock(&this->_pendingUnregisterListMutex);
}

void ClientEmulationOutputManager::_RegisterPending()
{
	ClientEmulationOutput *existingOutput = NULL;
	
	const size_t registerSize = this->_pendingRegisterList.size();
	if (registerSize > 0)
	{
		bool isExistingFound = false;
		
		for (size_t i = 0; i < registerSize; i++)
		{
			isExistingFound = false;
			ClientEmulationOutput *pendingOutput = this->_pendingRegisterList[i];
			
			for (size_t j = 0; j < this->_runList.size(); j++)
			{
				existingOutput = this->_runList[j];
				if (pendingOutput == existingOutput)
				{
					isExistingFound = true;
					break;
				}
			}
			
			if (!isExistingFound)
			{
				this->_runList.push_back(pendingOutput);
			}
		}
		
		this->_pendingRegisterList.clear();
	}
}

void ClientEmulationOutputManager::_UnregisterPending()
{
	ClientEmulationOutput *existingOutput = NULL;
	
	const size_t unregisterSize = this->_pendingUnregisterList.size();
	if ( (unregisterSize > 0) && (this->_runList.size() > 0) )
	{
		for (size_t i = 0; i < unregisterSize; i++)
		{
			ClientEmulationOutput *pendingOutput = this->_pendingUnregisterList[i];
			
			for (size_t j = this->_runList.size() - 1; j < this->_runList.size(); j--)
			{
				existingOutput = this->_runList[j];
				if (pendingOutput == existingOutput)
				{
					this->_runList.erase(this->_runList.begin() + j);
				}
			}
		}
		
		this->_pendingUnregisterList.clear();
	}
}

void ClientEmulationOutputManager::DispatchAllOfPlatformAndType(int32_t platformId, int32_t typeId, int32_t messageID)
{
	// Add all pending outputs to be registered.
	pthread_mutex_lock(&this->_pendingRegisterListMutex);
	this->_RegisterPending();
	pthread_mutex_unlock(&this->_pendingRegisterListMutex);
	
	// Remove all pending outputs to be unregistered.
	pthread_mutex_lock(&this->_pendingUnregisterListMutex);
	this->_UnregisterPending();
	
	// Finally, run the outputs.
	for (size_t i = 0; i < this->_runList.size(); i++)
	{
		ClientEmulationOutput *runningOutput = this->_runList[i];
		if ( !runningOutput->IsIdle() && ((runningOutput->GetPlatformId() & platformId) != 0) && ((runningOutput->GetTypeId() & typeId) != 0) )
		{
			runningOutput->Dispatch(messageID);
		}
	}
	
	pthread_mutex_unlock(&this->_pendingUnregisterListMutex);
}

void ClientEmulationOutputManager::DispatchAll(int32_t messageID)
{
	this->DispatchAllOfPlatformAndType(PlatformTypeID_Any, OutputTypeID_Any, messageID);
}
