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

#include "ClientAudioOutput.h"

#include "ClientExecutionControl.h"
#include "cocoa_globals.h"

#include "../../NDSSystem.h"

ClientAudioOutput::ClientAudioOutput()
{
	_platformId = PlatformTypeID_Any;
	_typeId = OutputTypeID_Generic | OutputTypeID_Audio;
	
	_volume = MAX_VOLUME;
	_engineID = 0;
	_spuAdvancedLogic = true;
	_interpolationModeID = SPUInterpolation_Cosine;
	_syncModeID = ESynchMode_Synchronous;
	_syncMethodID = ESynchMethod_N;
	_isMuted = false;
	_filterID = 0;
	
	_engineString = "None";
	_spuInterpolationModeString = "Cosine";
	_spuSyncMethodString = "N";
	
	pthread_mutex_init(&_mutexEngine, NULL);
	pthread_mutex_init(&_mutexVolume, NULL);
	pthread_mutex_init(&_mutexSpuAdvancedLogic, NULL);
	pthread_mutex_init(&_mutexSpuInterpolationMode, NULL);
	pthread_mutex_init(&_mutexSpuSyncMethod, NULL);
}

ClientAudioOutput::~ClientAudioOutput()
{
	pthread_mutex_destroy(&this->_mutexEngine);
	pthread_mutex_destroy(&this->_mutexVolume);
	pthread_mutex_destroy(&this->_mutexSpuAdvancedLogic);
	pthread_mutex_destroy(&this->_mutexSpuInterpolationMode);
	pthread_mutex_destroy(&this->_mutexSpuSyncMethod);
}

void ClientAudioOutput::SetEngineByID(int engineID)
{
	pthread_mutex_lock(&this->_mutexEngine);
	this->_engineID = engineID;
	pthread_mutex_unlock(&this->_mutexEngine);
	
	int result = -1;
	
	pthread_rwlock_wrlock(this->_rwlockProducer);
	
	if (engineID != SNDCORE_DUMMY)
	{
		result = SPU_ChangeSoundCore(engineID, (int)SPU_BUFFER_BYTES);
	}
	
	if (result == -1)
	{
		SPU_ChangeSoundCore(SNDCORE_DUMMY, 0);
	}
	
	SoundInterface_struct *soundCore = SPU_SoundCore();
	if (soundCore != NULL)
	{
		this->_engineString = soundCore->Name;
	}
	
	pthread_rwlock_unlock(this->_rwlockProducer);
	
	pthread_mutex_lock(&this->_mutexVolume);
	
	if (!this->_isMuted)
	{
		SPU_SetVolume((int)(this->_volume + 0.5f));
	}
	
	pthread_mutex_unlock(&this->_mutexVolume);
}

int ClientAudioOutput::GetEngineByID()
{
	return this->_engineID;
}

void ClientAudioOutput::SetVolume(float vol)
{
	if (vol < 0.0f)
	{
		vol = 0.0f;
	}
	else if (vol > MAX_VOLUME)
	{
		vol = MAX_VOLUME;
	}
	
	pthread_mutex_lock(&this->_mutexVolume);
	
	this->_volume = vol;
	
	if (!this->_isMuted)
	{
		SPU_SetVolume((int)(vol + 0.5f));
	}
	
	pthread_mutex_unlock(&this->_mutexVolume);
}

float ClientAudioOutput::GetVolume()
{
	pthread_mutex_lock(&this->_mutexVolume);
	const float vol = this->_volume;
	pthread_mutex_unlock(&this->_mutexVolume);
	
	return vol;
}

void ClientAudioOutput::SetSPUAdvancedLogic(bool theState)
{
	pthread_mutex_lock(&this->_mutexSpuAdvancedLogic);
	this->_spuAdvancedLogic = theState;
	pthread_mutex_unlock(&this->_mutexSpuAdvancedLogic);
	
	pthread_rwlock_wrlock(this->_rwlockProducer);
	CommonSettings.spu_advanced = theState;
	pthread_rwlock_unlock(this->_rwlockProducer);
}

bool ClientAudioOutput::GetSPUAdvancedLogic()
{
	return this->_spuAdvancedLogic;
}

void ClientAudioOutput::SetSPUInterpolationModeByID(SPUInterpolationMode interpModeID)
{
	pthread_mutex_lock(&this->_mutexSpuInterpolationMode);
	this->_interpolationModeID = interpModeID;
	pthread_mutex_unlock(&this->_mutexSpuInterpolationMode);
	
	pthread_rwlock_wrlock(this->_rwlockProducer);
	CommonSettings.spuInterpolationMode = interpModeID;
	pthread_rwlock_unlock(this->_rwlockProducer);
	
	switch (interpModeID)
	{
		case SPUInterpolation_None:
			this->_spuInterpolationModeString = "None";
			break;
			
		case SPUInterpolation_Linear:
			this->_spuInterpolationModeString = "Linear";
			break;
			
		case SPUInterpolation_Cosine:
			this->_spuInterpolationModeString = "Cosine";
			break;
			
		case SPUInterpolation_CatmullRom:
			this->_spuInterpolationModeString = "Catmull-Rom";
			break;
			
		default:
			break;
	}
}

SPUInterpolationMode ClientAudioOutput::GetSPUInterpolationModeByID()
{
	return this->_interpolationModeID;
}

void ClientAudioOutput::SetSPUSyncModeByID(ESynchMode syncModeID)
{
	pthread_mutex_lock(&this->_mutexSpuSyncMethod);
	this->_syncModeID = syncModeID;
	pthread_mutex_unlock(&this->_mutexSpuSyncMethod);
	
	pthread_rwlock_wrlock(this->_rwlockProducer);
	CommonSettings.SPU_sync_mode = syncModeID;
	SPU_SetSynchMode(CommonSettings.SPU_sync_mode, CommonSettings.SPU_sync_method);
	pthread_rwlock_unlock(this->_rwlockProducer);
	
	if (syncModeID == ESynchMode_DualSynchAsynch)
	{
		this->_spuSyncMethodString = "Dual SPU Sync/Async";
	}
	else
	{
		switch (this->_syncMethodID)
		{
			case ESynchMethod_N:
				this->_spuSyncMethodString = "\"N\" Sync Method";
				break;
				
			case ESynchMethod_Z:
				this->_spuSyncMethodString = "\"Z\" Sync Method";
				break;
				
			case ESynchMethod_P:
				this->_spuSyncMethodString = "\"P\" Sync Method";
				break;
				
			default:
				break;
		}
	}
}

ESynchMode ClientAudioOutput::GetSPUSyncModeByID()
{
	return this->_syncModeID;
}

void ClientAudioOutput::SetSPUSyncMethodByID(ESynchMethod syncMethodID)
{
	pthread_mutex_lock(&this->_mutexSpuSyncMethod);
	this->_syncMethodID = syncMethodID;
	pthread_mutex_unlock(&this->_mutexSpuSyncMethod);
	
	pthread_rwlock_wrlock(this->_rwlockProducer);
	CommonSettings.SPU_sync_method = syncMethodID;
	SPU_SetSynchMode(CommonSettings.SPU_sync_mode, CommonSettings.SPU_sync_method);
	pthread_rwlock_unlock(this->_rwlockProducer);
	
	if (this->_syncModeID == ESynchMode_DualSynchAsynch)
	{
		this->_spuInterpolationModeString = "Dual SPU Sync/Async";
	}
	else
	{
		switch (syncMethodID)
		{
			case ESynchMethod_N:
				this->_spuSyncMethodString = "\"N\" Sync Method";
				break;
				
			case ESynchMethod_Z:
				this->_spuSyncMethodString = "\"Z\" Sync Method";
				break;
				
			case ESynchMethod_P:
				this->_spuSyncMethodString = "\"P\" Sync Method";
				break;
				
			default:
				break;
		}
	}
}

ESynchMethod ClientAudioOutput::GetSPUSyncMethodByID()
{
	return this->_syncMethodID;
}

void ClientAudioOutput::SetMute(bool theState)
{
	pthread_mutex_lock(&this->_mutexVolume);
	
	this->_isMuted = theState;
	
	if (theState)
	{
		SPU_SetVolume(0);
	}
	else
	{
		SPU_SetVolume((int)(this->_volume + 0.5f));
	}
	
	pthread_mutex_unlock(&this->_mutexVolume);
}

bool ClientAudioOutput::IsMuted()
{
	pthread_mutex_lock(&this->_mutexVolume);
	const bool isMuted = this->_isMuted;
	pthread_mutex_unlock(&this->_mutexVolume);
	
	return isMuted;
}

void ClientAudioOutput::SetFilterByID(int filterID)
{
	this->_filterID = filterID;
}

int ClientAudioOutput::GetFilterByID()
{
	return this->_filterID;
}

const char* ClientAudioOutput::GetEngineString()
{
	return this->_engineString.c_str();
}

const char* ClientAudioOutput::GetSPUInterpolationModeString()
{
	return this->_spuInterpolationModeString.c_str();
}

const char* ClientAudioOutput::GetSPUSyncMethodString()
{
	return this->_spuSyncMethodString.c_str();
}
