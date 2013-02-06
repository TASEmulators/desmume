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

#include "coreaudiosound.h"
#include "cocoa_globals.h"

CoreAudioSound::CoreAudioSound(size_t bufferSamples, size_t sampleSize)
{
	OSStatus error = noErr;
	
	_spinlockAU = (OSSpinLock *)malloc(sizeof(OSSpinLock));
	*_spinlockAU = OS_SPINLOCK_INIT;
	
	_buffer = new RingBuffer(bufferSamples, sampleSize);
	_volume = 1.0f;
	
	// Create a new audio unit
	ComponentDescription desc;
	desc.componentType = kAudioUnitType_Output;
	desc.componentSubType = kAudioUnitSubType_DefaultOutput;
	desc.componentManufacturer = kAudioUnitManufacturer_Apple;
	desc.componentFlags = 0;
	desc.componentFlagsMask = 0;
	
	Component comp = FindNextComponent(NULL, &desc);
	if (comp == NULL)
	{
		return;
	}
	
	error = OpenAComponent(comp, &_au);
	if (error != noErr || comp == NULL)
	{
		return;
	}
	
	// Set the render callback
	AURenderCallbackStruct callback;
	callback.inputProc = &RenderCallback;
	callback.inputProcRefCon = _buffer;
	
	error = AudioUnitSetProperty(_au,
								 kAudioUnitProperty_SetRenderCallback,
								 kAudioUnitScope_Input,
								 0,
								 &callback,
								 sizeof(callback) );
	
	if(error != noErr)
	{
		return;
	}
	
	// Set up the audio unit for audio streaming
	AudioStreamBasicDescription audio_format;
	audio_format.mSampleRate = SPU_SAMPLE_RATE;
	audio_format.mFormatID = kAudioFormatLinearPCM;
	audio_format.mFormatFlags = kAudioFormatFlagIsSignedInteger | kAudioFormatFlagsNativeEndian | kLinearPCMFormatFlagIsPacked;
	audio_format.mBytesPerPacket = SPU_SAMPLE_SIZE;
	audio_format.mFramesPerPacket = 1;
	audio_format.mBytesPerFrame = SPU_SAMPLE_SIZE;
	audio_format.mChannelsPerFrame = SPU_NUMBER_CHANNELS;
	audio_format.mBitsPerChannel = SPU_SAMPLE_RESOLUTION;
	
	error = AudioUnitSetProperty(_au,
								 kAudioUnitProperty_StreamFormat,
								 kAudioUnitScope_Input,
								 0,
								 &audio_format,
								 sizeof(audio_format) );
	
	if(error != noErr)
	{
		return;
	}
	
	// Initialize our new audio unit
	error = AudioUnitInitialize(_au);
	if(error != noErr)
	{
		return;
	}
}

CoreAudioSound::~CoreAudioSound()
{
	OSSpinLockLock(_spinlockAU);
	
	if(_au != NULL)
	{
		AudioOutputUnitStop(_au);
		AudioUnitUninitialize(_au);
		_au = NULL;
	}
	
	OSSpinLockUnlock(_spinlockAU);
	
	delete _buffer;
	_buffer = NULL;
	
	free(_spinlockAU);
	_spinlockAU = NULL;
}

RingBuffer* CoreAudioSound::getBuffer()
{
	return this->_buffer;
}

void CoreAudioSound::start()
{
	this->clearBuffer();
	
	OSSpinLockLock(this->_spinlockAU);
	AudioUnitReset(this->_au, kAudioUnitScope_Global, 0);
	AudioOutputUnitStart(this->_au);
	OSSpinLockUnlock(this->_spinlockAU);
}

void CoreAudioSound::stop()
{
	OSSpinLockLock(this->_spinlockAU);
	AudioOutputUnitStop(this->_au);
	OSSpinLockUnlock(this->_spinlockAU);
	
	this->clearBuffer();
}

void CoreAudioSound::writeToBuffer(const void *buffer, size_t numberBytes)
{
	this->getBuffer()->write(buffer, numberBytes);
}

void CoreAudioSound::clearBuffer()
{
	this->_buffer->clear();
}

void CoreAudioSound::mute()
{
	OSSpinLockLock(this->_spinlockAU);
	AudioUnitSetParameter(this->_au, kHALOutputParam_Volume, kAudioUnitScope_Global, 0, 0.0f, 0);
	OSSpinLockUnlock(this->_spinlockAU);
}

void CoreAudioSound::unmute()
{
	OSSpinLockLock(this->_spinlockAU);
	AudioUnitSetParameter(this->_au, kHALOutputParam_Volume, kAudioUnitScope_Global, 0, this->_volume, 0);
	OSSpinLockUnlock(this->_spinlockAU);
}

size_t CoreAudioSound::getAvailableSamples()
{
	return this->_buffer->getAvailableElements();
}

float CoreAudioSound::getVolume()
{
	return this->_volume;
}

void CoreAudioSound::setVolume(float vol)
{
	this->_volume = vol;
	
	OSSpinLockLock(this->_spinlockAU);
	AudioUnitSetParameter(this->_au, kHALOutputParam_Volume, kAudioUnitScope_Global, 0, vol, 0);
	OSSpinLockUnlock(this->_spinlockAU);
}

OSStatus RenderCallback(void *inRefCon,
						AudioUnitRenderActionFlags *ioActionFlags,
						const AudioTimeStamp *inTimeStamp,
						UInt32 inBusNumber,
						UInt32 inNumberFrames,
						AudioBufferList *ioData)
{
	RingBuffer *__restrict__ audioBuffer = (RingBuffer *)inRefCon;
	UInt8 *__restrict__ playbackBuffer = (UInt8 *)ioData->mBuffers[0].mData;
	const size_t totalReadSize = inNumberFrames * audioBuffer->getElementSize();
	const size_t bytesRead = audioBuffer->read(playbackBuffer, totalReadSize);
	
	// Pad any remaining samples.
	if (bytesRead < totalReadSize)
	{
		memset(playbackBuffer + bytesRead, 0, totalReadSize - bytesRead);
	}
	
	// Copy to other channels.
	for (UInt32 channel = 1; channel < ioData->mNumberBuffers; channel++)
	{
		memcpy(ioData->mBuffers[channel].mData, playbackBuffer, ioData->mBuffers[0].mDataByteSize);
	}
	
	return noErr;
}
