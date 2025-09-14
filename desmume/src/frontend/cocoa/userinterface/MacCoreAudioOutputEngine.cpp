/*
	Copyright (C) 2012-2025 DeSmuME team

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

#include "MacCoreAudioOutputEngine.h"
#include "coreaudiosound.h"

#include <CoreAudio/CoreAudio.h>
#include "ringbuffer.h"
#include "utilities.h"
#include "cocoa_globals.h"

//#define FORCE_AUDIOCOMPONENT_10_5


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
		const size_t frameSize = audioBuffer->getElementSize();
		memset(playbackBuffer + (framesRead * frameSize), 0, (inNumberFrames - framesRead) * frameSize);
	}
	
	return noErr;
}

#pragma mark -

MacCoreAudioOutputEngine::MacCoreAudioOutputEngine()
{
	_engineID = SNDCORE_MAC_COREAUDIO;
	_engineString = "macOS Core Audio Sound Interface";
	_bufferSizeForSPUInit = SPU_BUFFER_BYTES;
	
	OSStatus error = noErr;
	
	_unfairlockAU = apple_unfairlock_create();
	
	_buffer = new RingBuffer(_bufferSizeForSPUInit * 4 / SPU_SAMPLE_SIZE, SPU_SAMPLE_SIZE);
	_coreAudioVolume = 1.0f;
	
	// Create a new audio unit
#if !defined(FORCE_AUDIOCOMPONENT_10_5) && defined(MAC_OS_X_VERSION_10_6) && (MAC_OS_X_VERSION_MAX_ALLOWED >= MAC_OS_X_VERSION_10_6)
	if (IsOSXVersionSupported(10, 6, 0))
	{
		AudioComponentDescription audioDesc;
		audioDesc.componentType = kAudioUnitType_Output;
		audioDesc.componentSubType = kAudioUnitSubType_DefaultOutput;
		audioDesc.componentManufacturer = kAudioUnitManufacturer_Apple;
		audioDesc.componentFlags = 0;
		audioDesc.componentFlagsMask = 0;
		
		CreateAudioUnitInstance(&_au, &audioDesc);
	}
	else
#endif
	{
		ComponentDescription audioDesc;
		audioDesc.componentType = kAudioUnitType_Output;
		audioDesc.componentSubType = kAudioUnitSubType_DefaultOutput;
		audioDesc.componentManufacturer = kAudioUnitManufacturer_Apple;
		audioDesc.componentFlags = 0;
		audioDesc.componentFlagsMask = 0;
		
		CreateAudioUnitInstance(&_au, &audioDesc);
	}
	
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
	
	if (error != noErr)
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
	
	if (error != noErr)
	{
		return;
	}
	
	// Initialize our new audio unit
	error = AudioUnitInitialize(_au);
	if (error != noErr)
	{
		return;
	}
}

MacCoreAudioOutputEngine::~MacCoreAudioOutputEngine()
{
	apple_unfairlock_lock(this->_unfairlockAU);
	DestroyAudioUnitInstance(&this->_au);
	apple_unfairlock_unlock(this->_unfairlockAU);
	
	delete this->_buffer;
	this->_buffer = NULL;
	
	apple_unfairlock_destroy(this->_unfairlockAU);
}

void MacCoreAudioOutputEngine::Start()
{
	apple_unfairlock_lock(this->_unfairlockAU);
	AudioOutputUnitStop(this->_au);
	AudioUnitReset(this->_au, kAudioUnitScope_Global, 0);
	this->_buffer->clear();
	
	if (!this->_isPaused)
	{
		AudioOutputUnitStart(this->_au);
	}
	
	apple_unfairlock_unlock(this->_unfairlockAU);
}

void MacCoreAudioOutputEngine::Stop()
{
	apple_unfairlock_lock(this->_unfairlockAU);
	AudioOutputUnitStop(this->_au);
	AudioUnitReset(this->_au, kAudioUnitScope_Global, 0);
	apple_unfairlock_unlock(this->_unfairlockAU);
	
	this->_buffer->clear();
}

void MacCoreAudioOutputEngine::SetMute(bool theState)
{
	float vol = (theState) ? 0.0f : this->_coreAudioVolume;
	
	apple_unfairlock_lock(this->_unfairlockAU);
	AudioUnitSetParameter(this->_au, kHALOutputParam_Volume, kAudioUnitScope_Global, 0, vol, 0);
	apple_unfairlock_unlock(this->_unfairlockAU);
}

void MacCoreAudioOutputEngine::SetPause(bool theState)
{
	ClientAudioOutputEngine::SetPause(theState);
	
	apple_unfairlock_lock(this->_unfairlockAU);
	
	if (theState)
	{
		AudioOutputUnitStop(this->_au);
	}
	else
	{
		AudioOutputUnitStart(this->_au);
	}
	
	apple_unfairlock_unlock(this->_unfairlockAU);
}

void MacCoreAudioOutputEngine::SetVolume(int volume)
{
	float newVolumeScalar = (float)volume / 100.0f;
	
	if (volume > 100)
	{
		newVolumeScalar = 1.0f;
	}
	else if (volume < 0)
	{
		newVolumeScalar = 0.0f;
	}
	this->_coreAudioVolume = newVolumeScalar;
	
	apple_unfairlock_lock(this->_unfairlockAU);
	AudioUnitSetParameter(this->_au, kHALOutputParam_Volume, kAudioUnitScope_Global, 0, newVolumeScalar, 0);
	apple_unfairlock_unlock(this->_unfairlockAU);
}

size_t MacCoreAudioOutputEngine::GetAvailableSamples() const
{
	return this->_buffer->getAvailableElements();
}

void MacCoreAudioOutputEngine::WriteToBuffer(const void *inSamples, size_t numberSampleFrames)
{
	size_t availableSampleFrames = this->_buffer->getAvailableElements();
	if (availableSampleFrames < numberSampleFrames)
	{
		this->_buffer->drop(numberSampleFrames - availableSampleFrames);
	}
	
	this->_buffer->write(inSamples, numberSampleFrames);
}

void MacCoreAudioOutputEngine::ClearBuffer()
{
	this->_buffer->clear();
}
