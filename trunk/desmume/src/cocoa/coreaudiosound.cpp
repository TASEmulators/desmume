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

#include "coreaudiosound.h"
#include "cocoa_globals.h"
#include "utilities.h"


CoreAudioOutput::CoreAudioOutput(size_t bufferSamples, size_t sampleSize)
{
	OSStatus error = noErr;
	
	_spinlockAU = (OSSpinLock *)malloc(sizeof(OSSpinLock));
	*_spinlockAU = OS_SPINLOCK_INIT;
	
	_buffer = new RingBuffer(bufferSamples, sampleSize);
	_volume = 1.0f;
	
	// Create a new audio unit
#if defined(MAC_OS_X_VERSION_10_6) && MAC_OS_X_VERSION_MAX_ALLOWED >= MAC_OS_X_VERSION_10_6
	if (IsOSXVersionSupported(10, 6, 0))
	{
		AudioComponentDescription audioDesc;
		audioDesc.componentType = kAudioUnitType_Output;
		audioDesc.componentSubType = kAudioUnitSubType_DefaultOutput;
		audioDesc.componentManufacturer = kAudioUnitManufacturer_Apple;
		audioDesc.componentFlags = 0;
		audioDesc.componentFlagsMask = 0;
		
		AudioComponent audioComponent = AudioComponentFindNext(NULL, &audioDesc);
		if (audioComponent == NULL)
		{
			return;
		}
		
		error = AudioComponentInstanceNew(audioComponent, &_au);
		if (error != noErr)
		{
			return;
		}
	}
	else
	{
		ComponentDescription audioDesc;
		audioDesc.componentType = kAudioUnitType_Output;
		audioDesc.componentSubType = kAudioUnitSubType_DefaultOutput;
		audioDesc.componentManufacturer = kAudioUnitManufacturer_Apple;
		audioDesc.componentFlags = 0;
		audioDesc.componentFlagsMask = 0;
		
		Component audioComponent = FindNextComponent(NULL, &audioDesc);
		if (audioComponent == NULL)
		{
			return;
		}
		
		error = OpenAComponent(audioComponent, &_au);
		if (error != noErr)
		{
			return;
		}
	}
#else
	ComponentDescription audioDesc;
	audioDesc.componentType = kAudioUnitType_Output;
	audioDesc.componentSubType = kAudioUnitSubType_DefaultOutput;
	audioDesc.componentManufacturer = kAudioUnitManufacturer_Apple;
	audioDesc.componentFlags = 0;
	audioDesc.componentFlagsMask = 0;
	
	Component audioComponent = FindNextComponent(NULL, &audioDesc);
	if (audioComponent == NULL)
	{
		return;
	}
	
	error = OpenAComponent(audioComponent, &_au);
	if (error != noErr)
	{
		return;
	}
#endif
	
	// Set the render callback
	AURenderCallbackStruct callback;
	callback.inputProc = &CoreAudioOutputRenderCallback;
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
	AudioStreamBasicDescription outputFormat;
	outputFormat.mSampleRate = SPU_SAMPLE_RATE;
	outputFormat.mFormatID = kAudioFormatLinearPCM;
	outputFormat.mFormatFlags = kAudioFormatFlagIsSignedInteger | kAudioFormatFlagsNativeEndian | kLinearPCMFormatFlagIsPacked;
	outputFormat.mBytesPerPacket = SPU_SAMPLE_SIZE;
	outputFormat.mFramesPerPacket = 1;
	outputFormat.mBytesPerFrame = SPU_SAMPLE_SIZE;
	outputFormat.mChannelsPerFrame = SPU_NUMBER_CHANNELS;
	outputFormat.mBitsPerChannel = SPU_SAMPLE_RESOLUTION;
	
	error = AudioUnitSetProperty(_au,
								 kAudioUnitProperty_StreamFormat,
								 kAudioUnitScope_Input,
								 0,
								 &outputFormat,
								 sizeof(outputFormat) );
	
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

CoreAudioOutput::~CoreAudioOutput()
{
	OSSpinLockLock(_spinlockAU);
	
	if(_au != NULL)
	{
		AudioOutputUnitStop(_au);
		AudioUnitUninitialize(_au);
#if defined(MAC_OS_X_VERSION_10_6) && MAC_OS_X_VERSION_MAX_ALLOWED >= MAC_OS_X_VERSION_10_6
		if (IsOSXVersionSupported(10, 6, 0))
		{
			AudioComponentInstanceDispose(_au);
		}
		else
		{
			CloseComponent(_au);
		}
#else
		CloseComponent(_au);
#endif
		_au = NULL;
	}
	
	OSSpinLockUnlock(_spinlockAU);
	
	delete _buffer;
	_buffer = NULL;
	
	free(_spinlockAU);
	_spinlockAU = NULL;
}

void CoreAudioOutput::start()
{
	this->clearBuffer();
	
	OSSpinLockLock(this->_spinlockAU);
	AudioUnitReset(this->_au, kAudioUnitScope_Global, 0);
	AudioOutputUnitStart(this->_au);
	OSSpinLockUnlock(this->_spinlockAU);
}

void CoreAudioOutput::stop()
{
	OSSpinLockLock(this->_spinlockAU);
	AudioOutputUnitStop(this->_au);
	OSSpinLockUnlock(this->_spinlockAU);
	
	this->clearBuffer();
}

void CoreAudioOutput::writeToBuffer(const void *buffer, size_t numberSampleFrames)
{
	size_t availableSampleFrames = this->_buffer->getAvailableElements();
	if (availableSampleFrames < numberSampleFrames)
	{
		this->_buffer->drop(numberSampleFrames - availableSampleFrames);
	}
	
	this->_buffer->write(buffer, numberSampleFrames);
}

void CoreAudioOutput::clearBuffer()
{
	this->_buffer->clear();
}

void CoreAudioOutput::mute()
{
	OSSpinLockLock(this->_spinlockAU);
	AudioUnitSetParameter(this->_au, kHALOutputParam_Volume, kAudioUnitScope_Global, 0, 0.0f, 0);
	OSSpinLockUnlock(this->_spinlockAU);
}

void CoreAudioOutput::unmute()
{
	OSSpinLockLock(this->_spinlockAU);
	AudioUnitSetParameter(this->_au, kHALOutputParam_Volume, kAudioUnitScope_Global, 0, this->_volume, 0);
	OSSpinLockUnlock(this->_spinlockAU);
}

size_t CoreAudioOutput::getAvailableSamples() const
{
	return this->_buffer->getAvailableElements();
}

float CoreAudioOutput::getVolume() const
{
	return this->_volume;
}

void CoreAudioOutput::setVolume(float vol)
{
	this->_volume = vol;
	
	OSSpinLockLock(this->_spinlockAU);
	AudioUnitSetParameter(this->_au, kHALOutputParam_Volume, kAudioUnitScope_Global, 0, vol, 0);
	OSSpinLockUnlock(this->_spinlockAU);
}

OSStatus CoreAudioOutputRenderCallback(void *inRefCon,
									   AudioUnitRenderActionFlags *ioActionFlags,
									   const AudioTimeStamp *inTimeStamp,
									   UInt32 inBusNumber,
									   UInt32 inNumberFrames,
									   AudioBufferList *ioData)
{
	RingBuffer *__restrict__ audioBuffer = (RingBuffer *)inRefCon;
	UInt8 *__restrict__ playbackBuffer = (UInt8 *)ioData->mBuffers[0].mData;
	const size_t framesRead = audioBuffer->read(playbackBuffer, inNumberFrames);
	
	// Pad any remaining samples.
	if (framesRead < inNumberFrames)
	{
		memset(playbackBuffer + (framesRead * SPU_SAMPLE_SIZE), 0, (inNumberFrames - framesRead) * SPU_SAMPLE_SIZE);
	}
	
	// Copy to other channels.
	for (UInt32 channel = 1; channel < ioData->mNumberBuffers; channel++)
	{
		memcpy(ioData->mBuffers[channel].mData, playbackBuffer, ioData->mBuffers[0].mDataByteSize);
	}
	
	return noErr;
}
