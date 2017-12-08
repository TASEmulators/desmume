/*
	Copyright (C) 2012-2017 DeSmuME team

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
#include <pthread.h>


OERingBuffer *openEmuSoundInterfaceBuffer = nil;

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
	NULL,
	NULL
};

SoundInterface_struct *SNDCoreList[] = {
	&SNDDummy,
	&SNDOpenEmu,
	NULL
};

int SNDOpenEmuInit(int buffer_size)
{
	[openEmuSoundInterfaceBuffer setLength:buffer_size * 4 / SPU_SAMPLE_SIZE];
	
	return 0;
}

void SNDOpenEmuDeInit()
{
	// Do nothing.
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
	// TODO: Use the newer-named method, [OERingBuffer freeBytes], when a newer version of OpenEmu.app released.
	// But for now, use the older-named method, [OERingBuffer usedBytes].
	
	//return (u32)[openEmuSoundInterfaceBuffer freeBytes] / SPU_SAMPLE_SIZE;
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
