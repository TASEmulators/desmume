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

#include "OpenEmuSoundInterface.h"

#include "cocoa_globals.h"

RingBuffer *openEmuSoundInterfaceBuffer = NULL;

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
	SNDOpenEmuClearBuffer
};

SoundInterface_struct *SNDCoreList[] = {
	&SNDDummy,
	&SNDOpenEmu,
	NULL
};

int SNDOpenEmuInit(int buffer_size)
{
	if (openEmuSoundInterfaceBuffer != NULL)
	{
		RingBuffer *oldBuffer = openEmuSoundInterfaceBuffer;
		openEmuSoundInterfaceBuffer = new RingBuffer(buffer_size / SPU_SAMPLE_SIZE, SPU_SAMPLE_SIZE);
		delete oldBuffer;
	}
	else
	{
		openEmuSoundInterfaceBuffer = new RingBuffer(buffer_size / SPU_SAMPLE_SIZE, SPU_SAMPLE_SIZE);
	}
	
	return 0;
}

void SNDOpenEmuDeInit()
{
	delete openEmuSoundInterfaceBuffer;
	openEmuSoundInterfaceBuffer = NULL;
}

int SNDOpenEmuReset()
{
	SNDOpenEmuClearBuffer();
	
	return 0;
}

void SNDOpenEmuUpdateAudio(s16 *buffer, u32 num_samples)
{
	if (openEmuSoundInterfaceBuffer != NULL)
	{
		openEmuSoundInterfaceBuffer->write(buffer, openEmuSoundInterfaceBuffer->getElementSize() * (size_t)num_samples);
	}
}

u32 SNDOpenEmuGetAudioSpace()
{
	u32 availableSamples = 0;
	
	if (openEmuSoundInterfaceBuffer != NULL)
	{
		availableSamples = (u32)openEmuSoundInterfaceBuffer->getAvailableElements();
	}
	
	return availableSamples;
}

void SNDOpenEmuMuteAudio()
{
	
}

void SNDOpenEmuUnMuteAudio()
{
	
}

void SNDOpenEmuSetVolume(int volume)
{
	
}

void SNDOpenEmuClearBuffer()
{
	if (openEmuSoundInterfaceBuffer != NULL)
	{
		openEmuSoundInterfaceBuffer->clear();
	}
}
