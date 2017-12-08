/*
	Copyright (C) 2007 Jeff Bland
	Copyright (C) 2007-2014 DeSmuME team

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
	NULL,
	NULL
};

SoundInterface_struct *SNDCoreList[] = {
	&SNDDummy,
	&SNDOSX,
	NULL
};

int SNDOSXInit(int buffer_size)
{
	CoreAudioOutput *oldcoreAudioPlaybackManager = coreAudioPlaybackManager;
	coreAudioPlaybackManager = new CoreAudioOutput(buffer_size * 4 / SPU_SAMPLE_SIZE, SPU_SAMPLE_SIZE);
	delete oldcoreAudioPlaybackManager;
	
	coreAudioPlaybackManager->start();
	coreAudioPlaybackManager->pause();
	
	return 0;
}

void SNDOSXDeInit()
{
	delete coreAudioPlaybackManager;
	coreAudioPlaybackManager = NULL;
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

void SNDOSXPauseAudio()
{
	if (coreAudioPlaybackManager == NULL)
	{
		return;
	}
	
	coreAudioPlaybackManager->pause();
}

void SNDOSXUnpauseAudio()
{
	if (coreAudioPlaybackManager == NULL)
	{
		return;
	}
	
	coreAudioPlaybackManager->unpause();
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
