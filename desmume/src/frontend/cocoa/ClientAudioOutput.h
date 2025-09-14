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

#ifndef _CLIENT_AUDIO_OUTPUT_H_
#define _CLIENT_AUDIO_OUTPUT_H_

#include "ClientEmulationOutput.h"

#include "rthreads.h"
#include "../../SPU.h"


class ClientAudioOutputEngine
{
protected:
	int _engineID;
	std::string _engineString;
	int _bufferSizeForSPUInit;
	bool _isPaused;
	
public:
	ClientAudioOutputEngine();
	virtual ~ClientAudioOutputEngine();
	
	int GetEngineID() const;
	const char* GetEngineString() const;
	int GetBufferSizeForSPUInit() const;
	bool IsPaused() const;
	
	virtual void Start();
	virtual void Stop();
	virtual void SetMute(bool theState);
	virtual void SetPause(bool theState);
	virtual void SetVolume(int volume);
	
	virtual size_t GetAvailableSamples() const;
	virtual void WriteToBuffer(const void *inSamples, size_t numberSampleFrames);
	virtual void ClearBuffer();
	
	virtual void HandleFetchSamples(s16 *sampleBuffer, size_t sampleCount, ESynchMode synchMode, ISynchronizingAudioBuffer *theSynchronizer);
	virtual size_t HandlePostProcessSamples(s16 *postProcessBuffer, size_t requestedSampleCount, ESynchMode synchMode, ISynchronizingAudioBuffer *theSynchronizer);
	
};

class DummyAudioOutputEngine : public ClientAudioOutputEngine
{
public:
	DummyAudioOutputEngine() {};
};

class ClientAudioOutput : public ClientEmulationOutput
{
protected:
	DummyAudioOutputEngine *_dummyAudioEngine;
	ClientAudioOutputEngine *_audioEngine;
	SoundInterface_struct _spuCallbackStruct;
	
	float _volume;
	bool _spuAdvancedLogic;
	SPUInterpolationMode _interpolationModeID;
	ESynchMode _syncModeID;
	ESynchMethod _syncMethodID;
	bool _isMuted;
	
	std::string _spuInterpolationModeString;
	std::string _spuSyncMethodString;
	
	slock_t *_mutexEngine;
	slock_t *_mutexVolume;
	slock_t *_mutexSpuAdvancedLogic;
	slock_t *_mutexSpuInterpolationMode;
	slock_t *_mutexSpuSyncMethod;
	
public:
	ClientAudioOutput();
	~ClientAudioOutput();
	
	void SetEngine(ClientAudioOutputEngine *theEngine);
	ClientAudioOutputEngine* GetEngine();
	int GetEngineByID();
	
	void SetVolume(float vol);
	float GetVolume();
	
	void SetSPUAdvancedLogic(bool theState);
	bool GetSPUAdvancedLogic();
	
	void SetSPUInterpolationModeByID(SPUInterpolationMode interpModeID);
	SPUInterpolationMode GetSPUInterpolationModeByID();
	
	void SetSPUSyncModeByID(ESynchMode syncModeID);
	ESynchMode GetSPUSyncModeByID();
	
	void SetSPUSyncMethodByID(ESynchMethod syncMethodID);
	ESynchMethod GetSPUSyncMethodByID();
	
	void SetMute(bool theState);
	bool IsMuted();
	
	const char* GetEngineString();
	const char* GetSPUInterpolationModeString();
	const char* GetSPUSyncMethodString();
	
	// ClientEmulationOutput methods
	virtual void SetIdle(bool theState);
};

#endif // _CLIENT_AUDIO_OUTPUT_H_
