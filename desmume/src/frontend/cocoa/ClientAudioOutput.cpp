/*
	Copyright (C) 2025-2026 DeSmuME team

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


// Global sound playback manager
static ClientAudioOutputEngine *__SharedAudioInterface = NULL;

int SPUInitCallback(int buffer_size)
{
	__SharedAudioInterface->Start();
	
	return 0;
}

void SPUDeInitCallback()
{
	__SharedAudioInterface->Stop();
}

void SPUUpdateAudioCallback(s16 *buffer, u32 num_samples)
{
	__SharedAudioInterface->WriteToBuffer(buffer, (size_t)num_samples);
}

u32 SPUGetAudioSpaceCallback()
{
	return (u32)__SharedAudioInterface->GetAvailableSamples();
}

void SPUMuteAudioCallback()
{
	__SharedAudioInterface->SetMute(true);
}

void SPUUnMuteAudioCallback()
{
	__SharedAudioInterface->SetMute(false);
}

void SPUSetVolumeCallback(int volume)
{
	__SharedAudioInterface->SetVolume(volume);
}

void SPUClearBufferCallback()
{
	__SharedAudioInterface->ClearBuffer();
}

void SPUFetchSamplesCallback(s16 *sampleBuffer, size_t sampleCount, ESynchMode synchMode, ISynchronizingAudioBuffer *theSynchronizer)
{
	__SharedAudioInterface->HandleFetchSamples(sampleBuffer, sampleCount, synchMode, theSynchronizer);
}

size_t SPUPostProcessSamples(s16 *postProcessBuffer, size_t requestedSampleCount, ESynchMode synchMode, ISynchronizingAudioBuffer *theSynchronizer)
{
	return __SharedAudioInterface->HandlePostProcessSamples(postProcessBuffer, requestedSampleCount, synchMode, theSynchronizer);
}

SoundInterface_struct *SNDCoreList[] = {
	&SNDDummy, // Index 0 must always be the dummy engine, since this is our fallback in case something goes wrong.
	NULL,      // Index 1 is reserved for the client audio output. It is dynamically assigned whenever the audio output engine is changed.
	NULL       // The last entry must always be NULL, representing the end of the list.
};

ClientAudioOutputEngine::ClientAudioOutputEngine()
{
	_engineID = SNDDummy.id;
	_engineString = SNDDummy.Name;
	_bufferSizeForSPUInit = 0;
	_isPaused = true;
}

ClientAudioOutputEngine::~ClientAudioOutputEngine()
{
	// Do nothing. This is implementation dependent.
}

int ClientAudioOutputEngine::GetEngineID() const
{
	return this->_engineID;
}

const char* ClientAudioOutputEngine::GetEngineString() const
{
	return this->_engineString.c_str();
}

int ClientAudioOutputEngine::GetBufferSizeForSPUInit() const
{
	return this->_bufferSizeForSPUInit;
}

bool ClientAudioOutputEngine::IsPaused() const
{
	return this->_isPaused;
}

bool ClientAudioOutputEngine::WillDeleteOnEngineChange() const
{
	return false;
}
   
void ClientAudioOutputEngine::Start()
{
	// Do nothing. This is implementation dependent.
}

void ClientAudioOutputEngine::Stop()
{
	// Do nothing. This is implementation dependent.
}

void ClientAudioOutputEngine::SetMute(bool theState)
{
	// Do nothing. This is implementation dependent.
}

void ClientAudioOutputEngine::SetPause(bool theState)
{
	this->_isPaused = theState;
}

void ClientAudioOutputEngine::SetVolume(int volume)
{
	// Do nothing. This is implementation dependent.
}

size_t ClientAudioOutputEngine::GetAvailableSamples() const
{
	// Do nothing. This is implementation dependent.
	return 0;
}

void ClientAudioOutputEngine::WriteToBuffer(const void *inSamples, size_t numberSampleFrames)
{
	// Do nothing. This is implementation dependent.
}

void ClientAudioOutputEngine::ClearBuffer()
{
	// Do nothing. This is implementation dependent.
}

void ClientAudioOutputEngine::HandleFetchSamples(s16 *sampleBuffer, size_t sampleCount, ESynchMode synchMode, ISynchronizingAudioBuffer *theSynchronizer)
{
	SPU_DefaultFetchSamples(sampleBuffer, sampleCount, synchMode, theSynchronizer);
}

size_t ClientAudioOutputEngine::HandlePostProcessSamples(s16 *postProcessBuffer, size_t requestedSampleCount, ESynchMode synchMode, ISynchronizingAudioBuffer *theSynchronizer)
{
	return SPU_DefaultPostProcessSamples(postProcessBuffer, requestedSampleCount, synchMode, theSynchronizer);
}

ClientAudioOutput::ClientAudioOutput()
{
	_platformId = PlatformTypeID_Any;
	_typeId = OutputTypeID_Generic | OutputTypeID_Audio;
	
	_dummyAudioEngine = new DummyAudioOutputEngine;
	if (__SharedAudioInterface == NULL)
	{
		__SharedAudioInterface = _dummyAudioEngine;
	}
	
	_audioEnginePending = _dummyAudioEngine;
	_audioEngineApplied = _audioEnginePending;
	_spuCallbackStruct.id                 = _audioEngineApplied->GetEngineID();
	_spuCallbackStruct.Name               = _audioEngineApplied->GetEngineString();
	_spuCallbackStruct.Init               = &SPUInitCallback;
	_spuCallbackStruct.DeInit             = &SPUDeInitCallback;
	_spuCallbackStruct.UpdateAudio        = &SPUUpdateAudioCallback;
	_spuCallbackStruct.GetAudioSpace      = &SPUGetAudioSpaceCallback;
	_spuCallbackStruct.MuteAudio          = &SPUMuteAudioCallback;
	_spuCallbackStruct.UnMuteAudio        = &SPUUnMuteAudioCallback;
	_spuCallbackStruct.SetVolume          = &SPUSetVolumeCallback;
	_spuCallbackStruct.ClearBuffer        = &SPUClearBufferCallback;
	_spuCallbackStruct.FetchSamples       = &SPUFetchSamplesCallback;
	_spuCallbackStruct.PostProcessSamples = &SPUPostProcessSamples;
	
	if (SNDCoreList[1] == NULL)
	{
		SNDCoreList[1] = &_spuCallbackStruct;
	}
	
	_volume = MAX_VOLUME;
	_spuAdvancedLogic = true;
	_interpolationModeID = SPUInterpolation_Cosine;
	_spuInterpolationModeString = "Cosine";
	_syncModeID = ESynchMode_Synchronous;
	_syncMethodID = ESynchMethod_N;
	_spuSyncMethodString = "\"N\" Sync Method";
	_isMuted = false;
	
	_mutexEngine = slock_new();
	_mutexVolume = slock_new();
	_mutexSpuAdvancedLogic = slock_new();
	_mutexSpuInterpolationMode = slock_new();
	_mutexSpuSyncMethod = slock_new();
	
	_needUpdateAudioEngine = true;
	_needUpdateVolume = true;
	_needUpdateSPUAdvancedLogic = true;
	_needUpdateSPUInterpolationMode = true;
	_needUpdateSPUSyncMethod = true;
}

ClientAudioOutput::~ClientAudioOutput()
{
	delete this->_dummyAudioEngine;
	this->_dummyAudioEngine = NULL;
	
	slock_free(this->_mutexEngine);
	slock_free(this->_mutexVolume);
	slock_free(this->_mutexSpuAdvancedLogic);
	slock_free(this->_mutexSpuInterpolationMode);
	slock_free(this->_mutexSpuSyncMethod);
}

void ClientAudioOutput::SetEngine(ClientAudioOutputEngine *theEngine)
{
	if (theEngine == NULL)
	{
		theEngine = this->_dummyAudioEngine;
	}
	
	slock_lock(this->_mutexEngine);
	this->_audioEnginePending = theEngine;
	this->_needUpdateAudioEngine = true;
	slock_unlock(this->_mutexEngine);
	
	slock_lock(this->_mutexVolume);
	this->_needUpdateVolume = true;
	slock_unlock(this->_mutexVolume);
}

ClientAudioOutputEngine* ClientAudioOutput::GetEngine()
{
	slock_lock(this->_mutexEngine);
	ClientAudioOutputEngine *theEngine = this->_audioEnginePending;
	slock_unlock(this->_mutexEngine);
	
	return theEngine;
}

int ClientAudioOutput::GetEngineByID()
{
	return this->_audioEnginePending->GetEngineID();
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
	
	slock_lock(this->_mutexVolume);
	this->_volume = vol;
	this->_needUpdateVolume = true;
	slock_unlock(this->_mutexVolume);
}

float ClientAudioOutput::GetVolume()
{
	slock_lock(this->_mutexVolume);
	const float vol = this->_volume;
	slock_unlock(this->_mutexVolume);
	
	return vol;
}

void ClientAudioOutput::SetSPUAdvancedLogic(bool theState)
{
	slock_lock(this->_mutexSpuAdvancedLogic);
	this->_spuAdvancedLogic = theState;
	this->_needUpdateSPUAdvancedLogic = true;
	slock_unlock(this->_mutexSpuAdvancedLogic);
}

bool ClientAudioOutput::GetSPUAdvancedLogic()
{
	slock_lock(this->_mutexSpuAdvancedLogic);
	const bool spuAdvancedLogic = this->_spuAdvancedLogic;
	slock_unlock(this->_mutexSpuAdvancedLogic);
	
	return spuAdvancedLogic;
}

void ClientAudioOutput::SetSPUInterpolationModeByID(SPUInterpolationMode interpModeID)
{
	slock_lock(this->_mutexSpuInterpolationMode);
	this->_interpolationModeID = interpModeID;
	this->_needUpdateSPUInterpolationMode = true;
	slock_unlock(this->_mutexSpuInterpolationMode);
	
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
	slock_lock(this->_mutexSpuInterpolationMode);
	const SPUInterpolationMode interpModeID = this->_interpolationModeID;
	slock_unlock(this->_mutexSpuInterpolationMode);
	
	return interpModeID;
}

void ClientAudioOutput::SetSPUSyncModeByID(ESynchMode syncModeID)
{
	slock_lock(this->_mutexSpuSyncMethod);
	this->_syncModeID = syncModeID;
	this->_needUpdateSPUSyncMethod = true;
	slock_unlock(this->_mutexSpuSyncMethod);
	
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
	slock_lock(this->_mutexSpuSyncMethod);
	const ESynchMode syncModeID = this->_syncModeID;
	slock_unlock(this->_mutexSpuSyncMethod);
	
	return syncModeID;
}

void ClientAudioOutput::SetSPUSyncMethodByID(ESynchMethod syncMethodID)
{
	slock_lock(this->_mutexSpuSyncMethod);
	this->_syncMethodID = syncMethodID;
	this->_needUpdateSPUSyncMethod = true;
	slock_unlock(this->_mutexSpuSyncMethod);
	
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
	slock_lock(this->_mutexSpuSyncMethod);
	const ESynchMethod syncMethodID = this->_syncMethodID;
	slock_unlock(this->_mutexSpuSyncMethod);
	
	return syncMethodID;
}

void ClientAudioOutput::SetMute(bool theState)
{
	slock_lock(this->_mutexVolume);
	this->_isMuted = theState;
	this->_needUpdateVolume = true;
	slock_unlock(this->_mutexVolume);
}

bool ClientAudioOutput::IsMuted()
{
	slock_lock(this->_mutexVolume);
	const bool isMuted = this->_isMuted;
	slock_unlock(this->_mutexVolume);
	
	return isMuted;
}

const char* ClientAudioOutput::GetEngineString()
{
	return this->_audioEnginePending->GetEngineString();
}

const char* ClientAudioOutput::GetSPUInterpolationModeString()
{
	return this->_spuInterpolationModeString.c_str();
}

const char* ClientAudioOutput::GetSPUSyncMethodString()
{
	return this->_spuSyncMethodString.c_str();
}

void ClientAudioOutput::SetIdle(bool theState)
{
	this->_audioEngineApplied->SetPause(theState);
	ClientEmulationOutput::SetIdle(theState);
}

void ClientAudioOutput::NDSExecConfigure()
{
	slock_lock(this->_mutexEngine);
	if (this->_needUpdateAudioEngine)
	{
		this->_needUpdateAudioEngine = false;
		
		const ClientAudioOutputEngine *oldEngine = this->_audioEngineApplied;
		const bool wasOldEnginePaused = oldEngine->IsPaused();
		
		this->_audioEngineApplied = this->_audioEnginePending;
		slock_unlock(this->_mutexEngine);
		
		this->_spuCallbackStruct.id   = this->_audioEngineApplied->GetEngineID();
		this->_spuCallbackStruct.Name = this->_audioEngineApplied->GetEngineString();
		__SharedAudioInterface = this->_audioEngineApplied;
		
		int result = SPU_ChangeSoundCore(this->_audioEngineApplied->GetEngineID(), this->_audioEngineApplied->GetBufferSizeForSPUInit());
		if (result == -1)
		{
			SPU_ChangeSoundCore(this->_dummyAudioEngine->GetEngineID(), this->_dummyAudioEngine->GetBufferSizeForSPUInit());
		}
		
		this->_audioEngineApplied->SetPause(wasOldEnginePaused);
		
		if ( oldEngine->WillDeleteOnEngineChange() )
		{
			delete oldEngine;
		}
	}
	else
	{
		slock_unlock(this->_mutexEngine);
	}
	
	slock_lock(this->_mutexVolume);
	if (this->_needUpdateVolume)
	{
		this->_needUpdateVolume = false;
		const float vol = this->_volume;
		const bool isMuted = this->_isMuted;
		slock_unlock(this->_mutexVolume);
		
		if (isMuted)
		{
			SPU_SetVolume(0);
		}
		else
		{
			SPU_SetVolume((int)(vol + 0.5f));
		}
	}
	else
	{
		slock_unlock(this->_mutexVolume);
	}
	
	slock_lock(this->_mutexSpuAdvancedLogic);
	if (this->_needUpdateSPUAdvancedLogic)
	{
		this->_needUpdateSPUAdvancedLogic = false;
		CommonSettings.spu_advanced = this->_spuAdvancedLogic;
		slock_unlock(this->_mutexSpuAdvancedLogic);
	}
	else
	{
		slock_unlock(this->_mutexSpuAdvancedLogic);
	}
	
	slock_lock(this->_mutexSpuInterpolationMode);
	if (this->_needUpdateSPUInterpolationMode)
	{
		this->_needUpdateSPUInterpolationMode = false;
		CommonSettings.spuInterpolationMode = this->_interpolationModeID;
		slock_unlock(this->_mutexSpuInterpolationMode);
	}
	else
	{
		slock_unlock(this->_mutexSpuInterpolationMode);
	}
	
	slock_lock(this->_mutexSpuSyncMethod);
	if (this->_needUpdateSPUSyncMethod)
	{
		this->_needUpdateSPUSyncMethod = false;
		CommonSettings.SPU_sync_mode = this->_syncModeID;
		CommonSettings.SPU_sync_method = this->_syncMethodID;
		slock_unlock(this->_mutexSpuSyncMethod);
		
		SPU_SetSynchMode(CommonSettings.SPU_sync_mode, CommonSettings.SPU_sync_method);
	}
	else
	{
		slock_unlock(this->_mutexSpuSyncMethod);
	}
}
