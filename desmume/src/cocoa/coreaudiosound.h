/*
	Copyright (C) 2012-2013 DeSmuME team

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

#ifndef _COREAUDIOSOUND_
#define _COREAUDIOSOUND_

#include <CoreServices/CoreServices.h>
#include <AudioUnit/AudioUnit.h>
#include <AudioToolbox/AudioToolbox.h>
#include <libkern/OSAtomic.h>
#include "ringbuffer.h"


class CoreAudioOutput
{
private:
	AudioUnit _au;
	RingBuffer *_buffer;
	OSSpinLock *_spinlockAU;
	float _volume;
	
public:
	CoreAudioOutput(size_t bufferSamples, size_t sampleSize);
	~CoreAudioOutput();
	
	void start();
	void stop();
	void writeToBuffer(const void *buffer, size_t numberSampleFrames);
	void clearBuffer();
	size_t getAvailableSamples() const;
	void mute();
	void unmute();
	float getVolume() const;
	void setVolume(float vol);
};

OSStatus CoreAudioOutputRenderCallback(void *inRefCon,
									   AudioUnitRenderActionFlags *ioActionFlags,
									   const AudioTimeStamp *inTimeStamp,
									   UInt32 inBusNumber,
									   UInt32 inNumberFrames,
									   AudioBufferList *ioData);

#endif
