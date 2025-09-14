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

#ifndef _MAC_COREAUDIO_OUTPUT_H_
#define _MAC_COREAUDIO_OUTPUT_H_

#include <CoreServices/CoreServices.h>
#include <AudioUnit/AudioUnit.h>

#include "utilities.h"

#include "ClientAudioOutput.h"

#define SNDCORE_MAC_COREAUDIO 58325 //hopefully this is unique number


class RingBuffer;

class MacCoreAudioOutputEngine : public ClientAudioOutputEngine
{
protected:
	AudioUnit _au;
	RingBuffer *_buffer;
	apple_unfairlock_t _unfairlockAU;
	float _coreAudioVolume;
	
public:
	MacCoreAudioOutputEngine();
	~MacCoreAudioOutputEngine();
	
	// ClientAudioOutputEngine methods
	virtual void Start();
	virtual void Stop();
	virtual void SetMute(bool theState);
	virtual void SetPause(bool theState);
	virtual void SetVolume(int volume);
	
	virtual size_t GetAvailableSamples() const;
	virtual void WriteToBuffer(const void *inSamples, size_t numberSampleFrames);
	virtual void ClearBuffer();
};

OSStatus CoreAudioOutputRenderCallback(void *inRefCon,
									   AudioUnitRenderActionFlags *ioActionFlags,
									   const AudioTimeStamp *inTimeStamp,
									   UInt32 inBusNumber,
									   UInt32 inNumberFrames,
									   AudioBufferList *ioData);

#endif // _MAC_COREAUDIO_OUTPUT_H_
