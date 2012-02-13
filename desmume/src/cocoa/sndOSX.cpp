/*
	Copyright (C) 2007 Jeff Bland
	Copyright (C) 2007-2012 DeSmuME team

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

#include <CoreServices/CoreServices.h>
#include <AudioUnit/AudioUnit.h>
#include <AudioToolbox/AudioToolbox.h>
#include <pthread.h>

#include "cocoa_globals.h"


//globals
static AudioUnit output_unit = NULL; //pointer to our audio device
static UInt8 *sound_data = NULL; //buffer where we hold data between getting it from the emulator and sending it to the device
static size_t sound_buffer_size = 0; //size in bytes of sound_data
static size_t sound_offset = SPU_STEREO_SAMPLE_SIZE; //position in the buffer that we have copied to from the emu
static size_t sound_position = 0; //position in the buffer that we have played to
static float current_volume_scalar = 1.0f; //for volume/muting

//file output
static bool file_open = false;
//static ExtAudioFileRef outfile;

static pthread_mutex_t *mutexSoundData = NULL;
static pthread_mutex_t *mutexAudioUnit = NULL;

//////////////////////////////////////////////////////////////////////////////

//This is the callback where we will stick the sound data we've gotten from
//the emulator into a core audio buffer to be processed and sent to the sound driver

OSStatus soundMixer(void *inRefCon,
					AudioUnitRenderActionFlags *ioActionFlags,
					const AudioTimeStamp *inTimeStamp,
					UInt32 inBusNumber,
					UInt32 inNumberFrames,
					AudioBufferList *ioData)
{
//printf("SOUND CALLBACK %u off%u  pos%u\n", inNumberFrames * 4, sound_offset, sound_position);
	UInt8 *__restrict__ outputData = (UInt8 *)ioData->mBuffers[0].mData;
	const size_t copySize = inNumberFrames * SPU_STEREO_SAMPLE_SIZE;
	size_t hiBufferAvailable = 0;
	size_t loBufferAvailable = 0;
	
	pthread_mutex_lock(mutexSoundData);
	
	if(sound_data != NULL)
	{
		const UInt8 *__restrict__ inputData = sound_data;
		const size_t inputDataReadPos = sound_position;
		const size_t inputDataWritePos = sound_offset;
		const size_t inputDataSize = sound_buffer_size;
		
		// Determine buffer availability
		if (inputDataReadPos < inputDataWritePos)
		{
			hiBufferAvailable = inputDataWritePos - inputDataReadPos - 1;
		}
		else if (inputDataReadPos > inputDataWritePos)
		{
			hiBufferAvailable = inputDataSize - inputDataReadPos - 1;
			loBufferAvailable = inputDataWritePos;
		}
		
		// Copy sound data from buffer
		if (copySize <= hiBufferAvailable)
		{
			memcpy(outputData, inputData + inputDataReadPos + 1, copySize);
			sound_position += copySize;
		}
		else
		{
			memcpy(outputData, inputData + inputDataReadPos + 1, hiBufferAvailable);
			
			if (copySize - hiBufferAvailable <= loBufferAvailable)
			{
				memcpy(outputData + hiBufferAvailable, inputData, copySize - hiBufferAvailable);
				sound_position = copySize - hiBufferAvailable - 1;
			}
			else
			{
				memcpy(outputData + hiBufferAvailable, inputData, loBufferAvailable);
				if (inputDataWritePos == 0)
				{
					sound_position = inputDataSize - 1;
				}
				else
				{
					sound_position = inputDataWritePos - 1;
				}
				
				// Pad any remaining samples with null samples
				const size_t totalAvailable = hiBufferAvailable + loBufferAvailable;
				if (copySize > totalAvailable)
				{
					memset(outputData + totalAvailable, 0, copySize - totalAvailable);
				}
			}
		}
	}
	else
	{
		memset(outputData, 0, copySize);
	}
	
	pthread_mutex_unlock(mutexSoundData);
	
	//copy to other channels
	for (UInt32 channel = 1; channel < ioData->mNumberBuffers; channel++)
	{
		memcpy(ioData->mBuffers[channel].mData, outputData, ioData->mBuffers[0].mDataByteSize);
	}
	
	//record to file
	if(file_open)
	{
		//ExtAudioFileWrite(outfile, inNumberFrames, ioData);
	}
	
	return noErr;
}

//////////////////////////////////////////////////////////////////////////////

void SNDOSXStartup()
{
	OSStatus error = noErr;
	
	if (mutexSoundData == NULL)
	{
		mutexSoundData = (pthread_mutex_t *)malloc(sizeof(pthread_mutex_t));
		pthread_mutex_init(mutexSoundData, NULL);
	}
	
	if (mutexAudioUnit == NULL)
	{
		mutexAudioUnit = (pthread_mutex_t *)malloc(sizeof(pthread_mutex_t));
		pthread_mutex_init(mutexAudioUnit, NULL);
	}
	
	//Setup the sound buffer -------------------------------------------
	pthread_mutex_lock(mutexSoundData);
	
	sound_data = (UInt8 *)calloc((SPU_SAMPLE_RATE / DS_FRAMES_PER_SECOND) + 1, SPU_STEREO_SAMPLE_SIZE);
	if(sound_data == NULL)
	{
		pthread_mutex_unlock(mutexSoundData);
		return;
	}
	
	sound_position = 0;
	sound_offset = SPU_STEREO_SAMPLE_SIZE;
	sound_buffer_size = ((SPU_SAMPLE_RATE / DS_FRAMES_PER_SECOND) + 1) * SPU_STEREO_SAMPLE_SIZE;
	
	pthread_mutex_unlock(mutexSoundData);
	
	//grab the default audio unit -------------------------
	
	pthread_mutex_lock(mutexAudioUnit);
	
	current_volume_scalar = 1.0f;
	
	ComponentDescription desc;
	desc.componentType = kAudioUnitType_Output;
	desc.componentSubType = kAudioUnitSubType_DefaultOutput;
	desc.componentManufacturer = kAudioUnitManufacturer_Apple;
	desc.componentFlags = 0;
	desc.componentFlagsMask = 0;
	
	Component comp = FindNextComponent(NULL, &desc);
	if (comp == NULL)
	{
		pthread_mutex_unlock(mutexAudioUnit);
		return;
	}
	
	error = OpenAComponent(comp, &output_unit);
	if (comp == NULL)
	{
		pthread_mutex_unlock(mutexAudioUnit);
		return;
	}
	
	//then setup the callback where we will send the audio -------
	
	AURenderCallbackStruct callback;
	callback.inputProc = soundMixer;
	callback.inputProcRefCon = NULL;
	
	error = AudioUnitSetProperty(output_unit,
								 kAudioUnitProperty_SetRenderCallback,
								 kAudioUnitScope_Input,
								 0,
								 &callback,
								 sizeof(callback) );
	
	if(error != noErr)
	{
		pthread_mutex_unlock(mutexAudioUnit);
		return;
	}
	
	//now begin running the audio unit-- ----------------------
	
	AudioStreamBasicDescription audio_format;
	audio_format.mSampleRate = SPU_SAMPLE_RATE;
	audio_format.mFormatID = kAudioFormatLinearPCM;
	audio_format.mFormatFlags = kAudioFormatFlagIsSignedInteger
	| kAudioFormatFlagsNativeEndian
	| kLinearPCMFormatFlagIsPacked;
	audio_format.mBytesPerPacket = 4;
	audio_format.mFramesPerPacket = 1;
	audio_format.mBytesPerFrame = 4;
	audio_format.mChannelsPerFrame = 2;
	audio_format.mBitsPerChannel = SPU_SAMPLE_RESOLUTION;
	
	error = AudioUnitSetProperty(output_unit,
								 kAudioUnitProperty_StreamFormat,
								 kAudioUnitScope_Input,
								 0,
								 &audio_format,
								 sizeof(audio_format) );
	
	if(error != noErr)
	{
		pthread_mutex_unlock(mutexAudioUnit);
		return;
	}
	
    // Initialize unit
	error = AudioUnitInitialize(output_unit);
	if(error != noErr)
	{
		pthread_mutex_unlock(mutexAudioUnit);
		return;
	}
	
	pthread_mutex_unlock(mutexAudioUnit);
	
	//we call the CFRunLoopRunInMode to service any notifications that the audio
	//system has to deal with
	//CFRunLoopRunInMode(kCFRunLoopDefaultMode, 2, false);
	//verify_noerr (AudioOutputUnitStop (output_unit));
}

void SNDOSXShutdown()
{
	pthread_mutex_lock(mutexAudioUnit);
	
	//closes the audio unit (errors are ignored here)
	if(output_unit != NULL)
	{
		AudioOutputUnitStop(output_unit);
		AudioUnitUninitialize(output_unit);
		output_unit = NULL;
	}
	
	pthread_mutex_unlock(mutexAudioUnit);
	
	pthread_mutex_lock(mutexSoundData);
	
	if(sound_data != NULL)
	{
		free(sound_data);
		sound_data = NULL;
		sound_position = 0;
		sound_offset = SPU_STEREO_SAMPLE_SIZE;
		sound_buffer_size = 0;
	}
	
	pthread_mutex_unlock(mutexSoundData);
	
	if (mutexSoundData != NULL)
	{
		pthread_mutex_destroy(mutexSoundData);
		free(mutexSoundData);
		mutexSoundData = NULL;
	}
	
	if (mutexAudioUnit != NULL)
	{
		pthread_mutex_destroy(mutexAudioUnit);
		free(mutexAudioUnit);
		mutexAudioUnit = NULL;
	}
}

int SNDOSXInit(int buffer_size)
{
	OSStatus error = noErr;
	const size_t singleSampleSize = SPU_STEREO_SAMPLE_SIZE;
	UInt8 *newSoundData = NULL;
	
	//Setup the sound buffer -------------------------------------------
	pthread_mutex_lock(mutexSoundData);
	
	//add one more since sound position can never catch up to
	//sound_offset - because if they were the same it would signify
	//that the buffer is empty
	
	sound_buffer_size = buffer_size + singleSampleSize;
	
	newSoundData = (UInt8 *)realloc(sound_data, sound_buffer_size);
	if(newSoundData == NULL)
	{
		free(sound_data);
		pthread_mutex_unlock(mutexSoundData);
		return -1;
	}
	
	memset(newSoundData, 0, sound_buffer_size);
	sound_data = newSoundData;
	sound_position = 0;
	sound_offset = singleSampleSize;
	
	pthread_mutex_unlock(mutexSoundData);
	
	//------------------------------------------------------------------
	
	//Start the rendering
	//The DefaultOutputUnit will do any format conversions to the format of the default device
	pthread_mutex_lock(mutexAudioUnit);
	
	if (output_unit != NULL)
	{
		AudioUnitReset(output_unit, kAudioUnitScope_Global, 0);
		
		error = AudioOutputUnitStart(output_unit);
		if(error != noErr)
		{
			pthread_mutex_unlock(mutexAudioUnit);
			return -1;
		}
	}
	
	pthread_mutex_unlock(mutexAudioUnit);
	
	return 0;
}

//////////////////////////////////////////////////////////////////////////////

void SNDOSXDeInit()
{
	pthread_mutex_lock(mutexAudioUnit);
	
	if(output_unit != NULL)
	{
		AudioOutputUnitStop(output_unit);
	}
	
	pthread_mutex_unlock(mutexAudioUnit);
	
	SNDOSXClearBuffer();
	SNDOSXCloseFile(); //end recording to file if needed
}

//////////////////////////////////////////////////////////////////////////////

int SNDOSXReset()
{
	SNDOSXClearBuffer();

	return 0;
}

void SNDOSXUpdateAudio(s16 *buffer, u32 num_samples)
{
	const size_t singleSampleSize = SPU_STEREO_SAMPLE_SIZE;
	size_t copySize = num_samples * singleSampleSize;
	size_t hiBufferAvailable = 0; // Buffer space ahead of offset
	size_t loBufferAvailable = 0; // Buffer space before read position
	
	pthread_mutex_lock(mutexSoundData);
	
	if(sound_data == NULL)
	{
		pthread_mutex_unlock(mutexSoundData);
		return;
	}
	
	UInt8 *__restrict__ inputData = sound_data;
	const size_t inputDataReadPos = sound_position;
	const size_t inputDataWritePos = sound_offset;
	const size_t inputDataSize = sound_buffer_size;
	
	if (inputDataWritePos >= inputDataReadPos)
	{
		hiBufferAvailable = inputDataSize - inputDataWritePos;
		loBufferAvailable = inputDataReadPos;
		
		// Subtract a sample's worth of bytes
		if (loBufferAvailable > 0)
		{
			loBufferAvailable -= 1;
		}
		else
		{
			hiBufferAvailable -= 1;
		}
	}
	else // (inputDataWritePos < inputDataReadPos)
	{
		hiBufferAvailable = inputDataReadPos - inputDataWritePos - 1;
	}
	
	if (copySize > hiBufferAvailable + loBufferAvailable)
	{
		//this shouldn't happen as the emulator core generally asks how much
		//space is available before sending stuff, but just in case
		int bytesShort = copySize - hiBufferAvailable - loBufferAvailable;
		
		printf("SNDOSXUpdateAudio() ERROR: Not enough space in buffer -- %i bytes short.\n", bytesShort);
		
		copySize = hiBufferAvailable + loBufferAvailable;
		copySize -= copySize % singleSampleSize;
	}
	
	if (copySize <= hiBufferAvailable)
	{
		memcpy(inputData + inputDataWritePos, buffer, copySize);
	}
	else
	{
		memcpy(inputData + inputDataWritePos, buffer, hiBufferAvailable);
		memcpy(inputData, ((UInt8 *)buffer) + hiBufferAvailable, copySize - hiBufferAvailable);
	}
	
	// Advance the offset
	sound_offset += copySize;
	if (sound_offset >= inputDataSize)
	{
		sound_offset -= inputDataSize;
	}
	
	pthread_mutex_unlock(mutexSoundData);
}

//////////////////////////////////////////////////////////////////////////////

u32 SNDOSXGetAudioSpace()
{
	const size_t singleSampleSize = SPU_STEREO_SAMPLE_SIZE;
	size_t free_space = 0;
	
	pthread_mutex_lock(mutexSoundData);
	
	if(sound_data == NULL)
	{
		pthread_mutex_unlock(mutexSoundData);
		return 0;
	}

	if(sound_offset >= sound_position)
	{
		free_space = (sound_buffer_size - sound_offset) + sound_position;
	}
	else // (sound_offset < sound_position)
	{
		free_space = sound_position - sound_offset;
	}
	
	pthread_mutex_unlock(mutexSoundData);
	
	if (free_space >= singleSampleSize)
	{
		free_space -= singleSampleSize;
	}
	else
	{
		free_space = 0;
	}
	
	return (u32)(free_space / singleSampleSize);
}

//////////////////////////////////////////////////////////////////////////////

void SNDOSXMuteAudio()
{
	OSStatus error = noErr;
	
	pthread_mutex_lock(mutexAudioUnit);
	
	if(output_unit == NULL)
	{
		pthread_mutex_unlock(mutexAudioUnit);
		return;
	}
	
	error = AudioUnitSetParameter(output_unit, kHALOutputParam_Volume, kAudioUnitScope_Global, 0, 0, 0);
	
	pthread_mutex_unlock(mutexAudioUnit);

	if(error != noErr)
	{
		printf("SNDOSXMuteAudio() ERROR: Could not set Audio Unit parameter -- Volume=0.0%%\n");
	}
}

//////////////////////////////////////////////////////////////////////////////

void SNDOSXUnMuteAudio()
{
	OSStatus error = noErr;
	float volumeScalar = 0.0f;

	pthread_mutex_lock(mutexAudioUnit);
	
	if(output_unit == NULL)
	{
		pthread_mutex_unlock(mutexAudioUnit);
		return;
	}
	
	volumeScalar = current_volume_scalar;
	error = AudioUnitSetParameter(output_unit, kHALOutputParam_Volume, kAudioUnitScope_Global, 0, volumeScalar, 0);
	
	pthread_mutex_unlock(mutexAudioUnit);

	if(error != noErr)
	{
		printf("SNDOSXUnMuteAudio() ERROR: Could not set Audio Unit parameter -- Volume=%1.1f%%\n", volumeScalar * 100.0f);
	}
}

//////////////////////////////////////////////////////////////////////////////

void SNDOSXSetVolume(int volume)
{
	OSStatus error = noErr;
	float newVolumeScalar = (float)volume / 100.0f;
	
	if(volume > 100)
	{
		newVolumeScalar = 1.0f;
	}
	else if(volume < 0)
	{
		newVolumeScalar = 0.0f;
	}
		
	pthread_mutex_lock(mutexAudioUnit);
	
	if(output_unit == NULL)
	{
		pthread_mutex_unlock(mutexAudioUnit);
		return;
	}
	
	current_volume_scalar = newVolumeScalar;
	error = AudioUnitSetParameter(output_unit, kHALOutputParam_Volume, kAudioUnitScope_Global, 0, newVolumeScalar, 0);
	
	pthread_mutex_unlock(mutexAudioUnit);

	if(error != noErr)
	{
		printf("SNDOSXSetVolume() ERROR: Could not set Audio Unit parameter -- Volume=%1.1f%%\n", newVolumeScalar * 100.0f);
	}
}

void SNDOSXClearBuffer()
{
	pthread_mutex_lock(mutexSoundData);
	
	if(sound_data != NULL)
	{
		memset(sound_data, 0, sound_buffer_size);
		sound_position = 0;
		sound_offset = SPU_STEREO_SAMPLE_SIZE;
	}
	
	pthread_mutex_unlock(mutexSoundData);
	
	pthread_mutex_lock(mutexAudioUnit);
	
	if(output_unit != NULL)
	{
		AudioUnitReset(output_unit, kAudioUnitScope_Global, 0);
	}
	
	pthread_mutex_unlock(mutexAudioUnit);
}

//////////////////////////////////////////////////////////////////////////////

SoundInterface_struct SNDOSX = {
	SNDCORE_OSX,
	"Core Audio Sound Interface",
	SNDOSXInit,
	SNDOSXDeInit,
	SNDOSXUpdateAudio,
	SNDOSXGetAudioSpace,
	SNDOSXMuteAudio,
	SNDOSXUnMuteAudio,
	SNDOSXSetVolume,
	SNDOSXClearBuffer
};

//////////////////////////////////////////////////////////////////////////////
//Sound recording
//////////////////////////////////////////////////////////////////////////////

bool SNDOSXOpenFile(void *fname)
{
	/*
	if(sound_data == NULL)return false;

	SNDOSXCloseFile();

	if(!fname)return false;

	NSString *filename = (NSString*)fname;
	FSRef ref;

	if(FSPathMakeRef((const UInt8*)[[filename stringByDeletingLastPathComponent] fileSystemRepresentation], &ref, NULL) != noErr)
	{
		SNDOSXStopRecording();
		return false;
	}

	AudioStreamBasicDescription audio_format;
	audio_format.mSampleRate = SPU_SAMPLE_RATE;
	audio_format.mFormatID = kAudioFormatLinearPCM;
	audio_format.mFormatFlags = kAudioFormatFlagIsSignedInteger
								| kAudioFormatFlagsNativeEndian
								| kLinearPCMFormatFlagIsPacked;
	audio_format.mBytesPerPacket = 4;
	audio_format.mFramesPerPacket = 1;
	audio_format.mBytesPerFrame = 4;
	audio_format.mChannelsPerFrame = 2;
	audio_format.mBitsPerChannel = SPU_SAMPLE_RESOLUTION;
	 
	if(ExtAudioFileCreateNew(&ref, (CFStringRef)[[filename pathComponents] lastObject], kAudioFileWAVEType, &audio_format, NULL, &outfile) != noErr)
		return false;

	file_open = true;

	return true;
	 */
	return false;
}

void SNDOSXStartRecording()
{
	pthread_mutex_lock(mutexSoundData);
	
	if(sound_data == NULL)
	{
		pthread_mutex_unlock(mutexSoundData);
		return;
	}
	
	pthread_mutex_unlock(mutexSoundData);
}

void SNDOSXStopRecording()
{
	pthread_mutex_lock(mutexSoundData);
	
	if(sound_data == NULL)
	{
		pthread_mutex_unlock(mutexSoundData);
		return;
	}
	
	pthread_mutex_unlock(mutexSoundData);
}

void SNDOSXCloseFile()
{
	pthread_mutex_lock(mutexSoundData);
	
	if(sound_data == NULL)
	{
		pthread_mutex_unlock(mutexSoundData);
		return;
	}

	if(file_open)
	{
		file_open = false;

		//if it's rendering sound, wait until it's not
		//so we dont close the file while writing to it
		
		// Do something here, just not implemented yet...

		//ExtAudioFileDispose(outfile);
	}
	
	pthread_mutex_unlock(mutexSoundData);
}

///////////////////////////////////////////////////////////////////////////
