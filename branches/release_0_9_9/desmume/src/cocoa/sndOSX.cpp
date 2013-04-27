/*
	Copyright (C) 2007 Jeff Bland
	Copyright (C) 2007-2013 DeSmuME team

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

#include "sndOSX.h"

#include "coreaudiosound.h"
#include "cocoa_globals.h"


// Global sound playback manager
static CoreAudioOutput *coreAudioPlaybackManager = NULL;
static pthread_mutex_t *mutexAudioSampleReadWrite = NULL;
pthread_mutex_t *mutexAudioEmulateCore = NULL;

// Sound interface to the SPU
SoundInterface_struct SNDOSX = {
	SNDCORE_OSX,
	"OS X Core Audio Sound Interface",
	SNDOSXInit,
	SNDOSXDeInit,
	SNDOSXUpdateAudio,
	SNDOSXGetAudioSpace,
	SNDOSXMuteAudio,
	SNDOSXUnMuteAudio,
	SNDOSXSetVolume,
	SNDOSXClearBuffer,
	SNDOSXFetchSamples,
	SNDOSXPostProcessSamples
};

SoundInterface_struct *SNDCoreList[] = {
	&SNDDummy,
	&SNDOSX,
	NULL
};

int SNDOSXInit(int buffer_size)
{
	CoreAudioOutput *oldcoreAudioPlaybackManager = coreAudioPlaybackManager;
	coreAudioPlaybackManager = new CoreAudioOutput(buffer_size / SPU_SAMPLE_SIZE, SPU_SAMPLE_SIZE);
	delete oldcoreAudioPlaybackManager;
	
	if (mutexAudioSampleReadWrite == NULL)
	{
		mutexAudioSampleReadWrite = (pthread_mutex_t *)malloc(sizeof(pthread_mutex_t));
		pthread_mutex_init(mutexAudioSampleReadWrite, NULL);
	}
	
	coreAudioPlaybackManager->start();
	
	return 0;
}

void SNDOSXDeInit()
{
	delete coreAudioPlaybackManager;
	coreAudioPlaybackManager = NULL;
	
	if (mutexAudioSampleReadWrite != NULL)
	{
		pthread_mutex_destroy(mutexAudioSampleReadWrite);
		free(mutexAudioSampleReadWrite);
		mutexAudioSampleReadWrite = NULL;
	}
}

int SNDOSXReset()
{
	SNDOSXClearBuffer();

	return 0;
}

void SNDOSXUpdateAudio(s16 *buffer, u32 num_samples)
{
	if (coreAudioPlaybackManager == NULL)
	{
		return;
	}
	
	coreAudioPlaybackManager->writeToBuffer(buffer, num_samples);
}

u32 SNDOSXGetAudioSpace()
{
	if (coreAudioPlaybackManager == NULL)
	{
		return 0;
	}
	
	return (u32)coreAudioPlaybackManager->getAvailableSamples();
}

void SNDOSXMuteAudio()
{
	if (coreAudioPlaybackManager == NULL)
	{
		return;
	}
	
	coreAudioPlaybackManager->mute();
}

void SNDOSXUnMuteAudio()
{
	if (coreAudioPlaybackManager == NULL)
	{
		return;
	}
	
	coreAudioPlaybackManager->unmute();
}

void SNDOSXSetVolume(int volume)
{
	if (coreAudioPlaybackManager == NULL)
	{
		return;
	}
	
	float newVolumeScalar = (float)volume / 100.0f;
	
	if(volume > 100)
	{
		newVolumeScalar = 1.0f;
	}
	else if(volume < 0)
	{
		newVolumeScalar = 0.0f;
	}
	
	coreAudioPlaybackManager->setVolume(newVolumeScalar);
}

void SNDOSXClearBuffer()
{
	if (coreAudioPlaybackManager == NULL)
	{
		return;
	}
	
	coreAudioPlaybackManager->clearBuffer();
}

void SNDOSXFetchSamples(s16 *sampleBuffer, size_t sampleCount, ESynchMode synchMode, ISynchronizingAudioBuffer *theSynchronizer)
{
	if (mutexAudioSampleReadWrite == NULL)
	{
		return;
	}
	
	pthread_mutex_lock(mutexAudioSampleReadWrite);
	SPU_DefaultFetchSamples(sampleBuffer, sampleCount, synchMode, theSynchronizer);
	pthread_mutex_unlock(mutexAudioSampleReadWrite);
}

size_t SNDOSXPostProcessSamples(s16 *postProcessBuffer, size_t requestedSampleCount, ESynchMode synchMode, ISynchronizingAudioBuffer *theSynchronizer)
{
	size_t processedSampleCount = 0;
	
	switch (synchMode)
	{
		case ESynchMode_DualSynchAsynch:
			if (mutexAudioEmulateCore != NULL)
			{
				pthread_mutex_lock(mutexAudioEmulateCore);
				processedSampleCount = SPU_DefaultPostProcessSamples(postProcessBuffer, requestedSampleCount, synchMode, theSynchronizer);
				pthread_mutex_unlock(mutexAudioEmulateCore);
			}
			break;
			
		case ESynchMode_Synchronous:
			if (mutexAudioSampleReadWrite != NULL)
			{
				pthread_mutex_lock(mutexAudioSampleReadWrite);
				processedSampleCount = SPU_DefaultPostProcessSamples(postProcessBuffer, requestedSampleCount, synchMode, theSynchronizer);
				pthread_mutex_unlock(mutexAudioSampleReadWrite);
			}
			break;
			
		default:
			break;
	}
	
	return processedSampleCount;
}
