/*
	Copyright (C) 2012 DeSmuME team

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

#import "OESoundInterface.h"

#import "cocoa_globals.h"


OERingBuffer *openEmuSoundInterfaceBuffer = nil;
static pthread_mutex_t *mutexAudioSampleReadWrite = NULL;
pthread_mutex_t *mutexAudioEmulateCore = NULL;

// Sound interface to the SPU
SoundInterface_struct SNDOpenEmu = {
	SNDCORE_OPENEMU,
	"OpenEmu Sound Interface",
	SNDOpenEmuInit,
	SNDOpenEmuDeInit,
	SNDOpenEmuUpdateAudio,
	SNDOpenEmuGetAudioSpace,
	SNDOpenEmuMuteAudio,
	SNDOpenEmuUnMuteAudio,
	SNDOpenEmuSetVolume,
	SNDOpenEmuClearBuffer,
	SNDOpenEmuFetchSamples,
	SNDOpenEmuPostProcessSamples
};

SoundInterface_struct *SNDCoreList[] = {
	&SNDDummy,
	&SNDOpenEmu,
	NULL
};

int SNDOpenEmuInit(int buffer_size)
{
	[openEmuSoundInterfaceBuffer setLength:buffer_size];
	
	if (mutexAudioSampleReadWrite == NULL)
	{
		mutexAudioSampleReadWrite = (pthread_mutex_t *)malloc(sizeof(pthread_mutex_t));
		pthread_mutex_init(mutexAudioSampleReadWrite, NULL);
	}
	
	return 0;
}

void SNDOpenEmuDeInit()
{
	if (mutexAudioSampleReadWrite != NULL)
	{
		pthread_mutex_destroy(mutexAudioSampleReadWrite);
		free(mutexAudioSampleReadWrite);
		mutexAudioSampleReadWrite = NULL;
	}
}

int SNDOpenEmuReset()
{
	// Do nothing. The OpenEmu frontend will take care of this.
	return 0;
}

void SNDOpenEmuUpdateAudio(s16 *buffer, u32 num_samples)
{
	[openEmuSoundInterfaceBuffer write:buffer maxLength:num_samples * SPU_SAMPLE_SIZE];
}

u32 SNDOpenEmuGetAudioSpace()
{	
	return (u32)[openEmuSoundInterfaceBuffer usedBytes] / SPU_SAMPLE_SIZE;
}

void SNDOpenEmuMuteAudio()
{
	// Do nothing. The OpenEmu frontend will take care of this.
}

void SNDOpenEmuUnMuteAudio()
{
	// Do nothing. The OpenEmu frontend will take care of this.
}

void SNDOpenEmuSetVolume(int volume)
{
	// Do nothing. The OpenEmu frontend will take care of this.
}

void SNDOpenEmuClearBuffer()
{
	// Do nothing. The OpenEmu frontend will take care of this.
}

void SNDOpenEmuFetchSamples(s16 *sampleBuffer, size_t sampleCount, ESynchMode synchMode, ISynchronizingAudioBuffer *theSynchronizer)
{
	if (mutexAudioSampleReadWrite == NULL)
	{
		return;
	}
	
	pthread_mutex_lock(mutexAudioSampleReadWrite);
	SPU_DefaultFetchSamples(sampleBuffer, sampleCount, synchMode, theSynchronizer);
	pthread_mutex_unlock(mutexAudioSampleReadWrite);
}

size_t SNDOpenEmuPostProcessSamples(s16 *postProcessBuffer, size_t requestedSampleCount, ESynchMode synchMode, ISynchronizingAudioBuffer *theSynchronizer)
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
