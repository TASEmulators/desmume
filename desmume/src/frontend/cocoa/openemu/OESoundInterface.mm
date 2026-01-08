/*
	Copyright (C) 2012-2026 DeSmuME team

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

#import <OpenEmuBase/OERingBuffer.h>
#import "../cocoa_globals.h"
#import "NDSGameCore.h"


OEAudioOutputEngine::OEAudioOutputEngine()
{
	__InstanceInit(NULL, false);
}

OEAudioOutputEngine::OEAudioOutputEngine(OERingBuffer *oeBuffer, bool useOldAPI)
{
	__InstanceInit(oeBuffer, useOldAPI);
}

void OEAudioOutputEngine::__InstanceInit(OERingBuffer *oeBuffer, bool useOldAPI)
{
	_engineID = SNDCORE_OPENEMU;
	_engineString = "OpenEmu Sound Interface";
	_bufferSizeForSPUInit = SPU_BUFFER_BYTES;
	_isPaused = false;
	
	[oeBuffer setLength:_bufferSizeForSPUInit * 4 / SPU_SAMPLE_SIZE];
	_buffer = oeBuffer;
	_willUseOldAPI = useOldAPI;
}

// ClientAudioOutputEngine methods

size_t OEAudioOutputEngine::GetAvailableSamples() const
{
	if (this->_willUseOldAPI)
	{
		SILENCE_DEPRECATION_OPENEMU( return (size_t)[this->_buffer usedBytes] / SPU_SAMPLE_SIZE; )
	}
	
	return (size_t)[this->_buffer freeBytes] / SPU_SAMPLE_SIZE;
}

void OEAudioOutputEngine::WriteToBuffer(const void *inSamples, size_t numberSampleFrames)
{
	[this->_buffer write:inSamples maxLength:numberSampleFrames * SPU_SAMPLE_SIZE];
}
