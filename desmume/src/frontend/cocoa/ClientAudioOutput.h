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

#include "../../SPU.h"


class ClientAudioOutput : public ClientEmulationOutput
{
protected:
	float _volume;
	int _engineID;
	bool _spuAdvancedLogic;
	SPUInterpolationMode _interpolationModeID;
	ESynchMode _syncModeID;
	ESynchMethod _syncMethodID;
	bool _isMuted;
	int _filterID;
	
	std::string _engineString;
	std::string _spuInterpolationModeString;
	std::string _spuSyncMethodString;
	
	pthread_mutex_t _mutexEngine;
	pthread_mutex_t _mutexVolume;
	pthread_mutex_t _mutexSpuAdvancedLogic;
	pthread_mutex_t _mutexSpuInterpolationMode;
	pthread_mutex_t _mutexSpuSyncMethod;
	
public:
	ClientAudioOutput();
	~ClientAudioOutput();
	
	void SetEngineByID(int engineID);
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
	
	void SetFilterByID(int filterID);
	int GetFilterByID();
	
	const char* GetEngineString();
	const char* GetSPUInterpolationModeString();
	const char* GetSPUSyncMethodString();
};

#endif // _CLIENT_AUDIO_OUTPUT_H_
