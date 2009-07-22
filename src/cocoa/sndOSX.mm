/*  Copyright 2007 Jeff Bland

    This file is part of DeSmuME

    DeSmuME is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    DeSmuME is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with DeSmuME; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

//external includes

#include "stdlib.h"
#include "stdio.h"

#include <Cocoa/Cocoa.h>

#include <CoreServices/CoreServices.h>
#include <AudioUnit/AudioUnit.h>
#include <AudioToolbox/AudioToolbox.h>

//desmume includes
#define OBJ_C
#include "sndOSX.h"
#undef BOOL

//globals
AudioUnit output_unit = NULL; //pointer to our audio device
UInt16 *sound_data = NULL; //buffer where we hold data between getting it from the emulator and sending it to the device
u32 sound_buffer_size; //size in bytes of sound_data
volatile u32 sound_offset; //position in the buffer that we have copied to from the emu
volatile u32 sound_position; //position in the buffer that we have played to

volatile bool mix_sound = false; //if false, our sound callback will do nothing
volatile bool in_mix = false; //should probably use a real mutex instead but this seems to work

float current_volume_scalar = 100; //for volume/muting

//file output
volatile bool file_open = false;
ExtAudioFileRef outfile;

//defines
#define SAMPLE_SIZE (sizeof(s16) * 2) //16 bit samples, stereo sound

//////////////////////////////////////////////////////////////////////////////

//This is the callback where we will stick the sound data we've gotten from
//the emulator into a core audio buffer to be processed and sent to the sound driver

OSStatus soundMixer(
	void *inRefCon,
	AudioUnitRenderActionFlags *ioActionFlags,
	const AudioTimeStamp *inTimeStamp,
	UInt32 	inBusNumber,
	UInt32 	inNumberFrames,
	AudioBufferList *ioData)
{
//printf("SOUND CALLBACK %u off%u  pos%u\n", inNumberFrames * 4, sound_offset, sound_position);

	if(!mix_sound)
		return noErr;

	in_mix = true;

	UInt32 bytes_to_copy = inNumberFrames * 4;
	UInt8 *data_output = (UInt8*)ioData->mBuffers[0].mData;
	UInt8 *data_input = (UInt8*)sound_data;

	if(sound_position == sound_offset)
	{//buffer empty

		memset(data_output, 0, bytes_to_copy);

	} else
	{ //buffer is not empty

		int x;
		for(x = 0; x < bytes_to_copy; x++)
		{
			sound_position++;

			if(sound_position >= sound_buffer_size)sound_position = 0;

			if(sound_position == sound_offset)
			{
				sound_position --;
				memset(&data_output[x], 0, bytes_to_copy - x);
				break;
			}

			data_output[x] = data_input[sound_position];

		}
	}

	//copy to other channels
	UInt32 channel;
	for (channel = 1; channel < ioData->mNumberBuffers; channel++)
	memcpy(ioData->mBuffers[channel].mData, ioData->mBuffers[0].mData, ioData->mBuffers[0].mDataByteSize);

	//record to file
	if(file_open)
		;//ExtAudioFileWrite(outfile, inNumberFrames, ioData);

	in_mix = false;
	return noErr;
}

//////////////////////////////////////////////////////////////////////////////

int SNDOSXInit(int buffer_size)
{
	OSStatus err = noErr;

	//Setup the sound buffer -------------------------------------------

	//add one more since sound position can never catch up to
	//sound_offset - because if they were the same it would signify
	//that the buffer is emptys
	buffer_size += 1;

	if((sound_data = (UInt16*)malloc(buffer_size)) == NULL)
		return -1;

	sound_buffer_size = buffer_size;

	SNDOSXReset();

	//grab the default audio unit -------------------------

	ComponentDescription desc;
	desc.componentType = kAudioUnitType_Output;
	desc.componentSubType = kAudioUnitSubType_DefaultOutput;
	desc.componentManufacturer = kAudioUnitManufacturer_Apple;
	desc.componentFlags = 0;
	desc.componentFlagsMask = 0;

	Component comp = FindNextComponent(NULL, &desc);
	if (comp == NULL)
		return -1;

	err = OpenAComponent(comp, &output_unit);
	if (comp == NULL)
		return -1;

	//then setup the callback where we will send the audio -------

	AURenderCallbackStruct callback;
	callback.inputProc = soundMixer;
	callback.inputProcRefCon = NULL;

	err = AudioUnitSetProperty(
		output_unit,
		kAudioUnitProperty_SetRenderCallback,
		kAudioUnitScope_Input,
		0,
		&callback,
		sizeof(callback)
	);

	if(err != noErr)
		return -1;

	//now begin running the audio unit-- ----------------------

	AudioStreamBasicDescription audio_format;
	audio_format.mSampleRate = 44100;
	audio_format.mFormatID = kAudioFormatLinearPCM;
	audio_format.mFormatFlags = kAudioFormatFlagIsSignedInteger
								| kAudioFormatFlagsNativeEndian
								| kLinearPCMFormatFlagIsPacked;
	audio_format.mBytesPerPacket = 4;
	audio_format.mFramesPerPacket = 1;
	audio_format.mBytesPerFrame = 4;
	audio_format.mChannelsPerFrame = 2;
	audio_format.mBitsPerChannel = 16;

	err = AudioUnitSetProperty(
		output_unit,
		kAudioUnitProperty_StreamFormat,
		kAudioUnitScope_Input,
		0,
		&audio_format,
		sizeof(audio_format)
	);

	if(err != noErr){return -1;}

    // Initialize unit
	err = AudioUnitInitialize(output_unit);
	if(err != noErr)
		return -1;

	//Start the rendering
	//The DefaultOutputUnit will do any format conversions to the format of the default device
	err = AudioOutputUnitStart(output_unit);
	if(err != noErr)
		return -1;

	//
	current_volume_scalar = 100;

	//we call the CFRunLoopRunInMode to service any notifications that the audio
	//system has to deal with
	//CFRunLoopRunInMode(kCFRunLoopDefaultMode, 2, false);
	//verify_noerr (AudioOutputUnitStop (output_unit));

	mix_sound = true;

	//------------------------------------------------------------------

	return 0;
}

//////////////////////////////////////////////////////////////////////////////

void SNDOSXDeInit()
{
	if(output_unit != NULL)
	{
		//wait for mix to end
		mix_sound = false;
		while(in_mix);

		//closes the audio unit (errors are ignored here)
		AudioOutputUnitStop(output_unit);
		AudioUnitUninitialize(output_unit);

		output_unit = NULL;
	}

	SNDOSXCloseFile(); //end recording to file if needed

	if(sound_data != NULL)
	{
		free(sound_data);
		sound_data = NULL;
	}
}

//////////////////////////////////////////////////////////////////////////////

int SNDOSXReset()
{
	if(sound_data == NULL)return 0;

	memset(sound_data, 0, sound_buffer_size);

	sound_offset = 0;
	sound_position = 0;

	return 0;
}

//////////////////////////////////////////////////////////////////////////////

void SNDOSXUpdateAudio(s16 *buffer, u32 num_samples)
{
//printf("NEW SOUND DATA %u  ", num_samples * SAMPLE_SIZE);

	if(sound_data == NULL)return;

	//get the avilable bufferspace ------------------------------------------------

	//in these calculatations:
	//1 is subtracted from sound_buffer_size since if the size is 2, we can only go to index 1
	//1 is subtracted from sound_position since sound_offset should never catch up to it
	//since them being equal means that the buffer is empty, rather than full

	u32 bytes_to_copy = num_samples * SAMPLE_SIZE;

	int sound_pos = sound_position; //incase the sound render thread changes it

	u32 copy1size, copy2size;

	if(sound_offset >= sound_pos)
	{
		if(sound_pos > 0)
		{
			copy1size = sound_buffer_size - sound_offset;
			copy2size = (sound_pos-1);
		} else
		{
			copy1size = sound_buffer_size - sound_offset - 1;
			copy2size = 0;
		}
	} else if(sound_offset < sound_pos)
	{
		copy1size = sound_pos - sound_offset - 1;
		copy2size = 0;
	}

//printf("copy1:%u copy2:%u both:%u\n",copy1size, copy2size, copy2size+copy1size);
	//truncate the copy sizes to however much we've been been given to fill --------

	if(copy1size + copy2size < bytes_to_copy)
		//this shouldn't happen as the emulator core generally asks how much
		//space is available before sending stuff, but just incase
		printf("NDOSXUpdateAudio() error: not enough space in buffer");
	else
	{
		u32 diff = copy1size + copy2size - bytes_to_copy;

		if(diff <= copy2size)
		{
			copy2size -= diff;
		} else
		{
			diff -= copy2size;
			copy2size = 0;
			copy1size -= diff;
		}

	}

	//copy the data -------------------------------------------------------------

	memcpy((((u8 *)sound_data)+sound_offset), buffer, copy1size);

	if(copy2size)
		memcpy(sound_data, ((u8 *)buffer)+copy1size, copy2size);

	//change our sound_offset
	sound_offset += copy1size + copy2size;
	while(sound_offset >= sound_buffer_size)sound_offset -= sound_buffer_size;

//printf("cop1%u cop2%u   off%u   size%u \n", copy1size, copy2size, sound_offset, sound_buffer_size);
}

//////////////////////////////////////////////////////////////////////////////

u32 SNDOSXGetAudioSpace()
{
	if(sound_data == NULL)return 0;

	int sound_pos = sound_position; //incase the sound render thread changes it

	u32 free_space;

	if(sound_offset > sound_pos)
		free_space = sound_buffer_size + sound_pos - 1 - sound_offset;
	else if(sound_offset < sound_pos)
		free_space = sound_pos - sound_offset - 1;
	else
		free_space = sound_buffer_size-1; //sound_offset == sound_data, meaning the buffer is empty

	return (free_space / SAMPLE_SIZE);
}

//////////////////////////////////////////////////////////////////////////////

void SNDOSXMuteAudio()
{
	if(sound_data == NULL)return;

	OSStatus err = AudioUnitSetParameter(output_unit, kHALOutputParam_Volume, kAudioUnitScope_Global, 0, 0, 0);

	if(err != noErr)
		printf("SNDOSXMuteAudio error\n");
}

//////////////////////////////////////////////////////////////////////////////

void SNDOSXUnMuteAudio()
{
	if(sound_data == NULL)return;

	OSStatus err = AudioUnitSetParameter(output_unit, kHALOutputParam_Volume, kAudioUnitScope_Global, 0, current_volume_scalar, 0);

	if(err != noErr)
		printf("SNDOSXUnMuteAudio %f error\n",current_volume_scalar);
}

//////////////////////////////////////////////////////////////////////////////

void SNDOSXSetVolume(int volume)
{
	if(sound_data == NULL)return;

	if(volume > 100)volume = 100;
	if(volume < 0)volume = 0;

	current_volume_scalar = volume;
	current_volume_scalar /= 100.0;

	OSStatus err = AudioUnitSetParameter(output_unit, kHALOutputParam_Volume, kAudioUnitScope_Global, 0, current_volume_scalar, 0);

	if(err != noErr)
		printf("SNDOSXSetVolume %f error\n",current_volume_scalar);
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
	SNDOSXSetVolume
};

//////////////////////////////////////////////////////////////////////////////
//Sound recording
//////////////////////////////////////////////////////////////////////////////

bool SNDOSXOpenFile(void *fname)
{
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
	audio_format.mSampleRate = 44100;
	audio_format.mFormatID = kAudioFormatLinearPCM;
	audio_format.mFormatFlags = kAudioFormatFlagIsSignedInteger
								| kAudioFormatFlagsNativeEndian
								| kLinearPCMFormatFlagIsPacked;
	audio_format.mBytesPerPacket = 4;
	audio_format.mFramesPerPacket = 1;
	audio_format.mBytesPerFrame = 4;
	audio_format.mChannelsPerFrame = 2;
	audio_format.mBitsPerChannel = 16;
/*
	if(ExtAudioFileCreateNew(&ref, (CFStringRef)[[filename pathComponents] lastObject], kAudioFileWAVEType, &audio_format, NULL, &outfile) != noErr)
		return false;

	file_open = true;

	return true;
*/
return false;
}

void SNDOSXStartRecording()
{
	if(sound_data == NULL)return;
}

void SNDOSXStopRecording()
{
	if(sound_data == NULL)return;
}

void SNDOSXCloseFile()
{
	if(sound_data == NULL)return;

	if(file_open)
	{
		file_open = false;

		//if it's rendering sound, wait until it's not
		//so we dont close the file while writing to it
		while(in_mix);

		//ExtAudioFileDispose(outfile);
	}
}

///////////////////////////////////////////////////////////////////////////
