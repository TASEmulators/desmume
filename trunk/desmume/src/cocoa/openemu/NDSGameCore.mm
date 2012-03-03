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

#import "NDSGameCore.h"
#import "cocoa_globals.h"
#import "cocoa_file.h"
#import "cocoa_firmware.h"
#import "cocoa_core.h"
#import "cocoa_mic.h"
#import "cocoa_output.h"
#include "../sndOSX.h"
//#include "OpenEmuSoundInterface.h"

#include <OpenGL/gl.h>
#include "../../path.h"
#include "../../NDSSystem.h"
#include "../../render3D.h"
#undef BOOL

@implementation NDSGameCore

@synthesize firmware;
@synthesize microphone;

- (id)init
{
	self = [super init];
    if(self == nil)
    {
		return self;
	}
	
	input = (bool *)calloc(sizeof(bool), OENDSButtonCount);
	
	microphone = [[CocoaDSMic alloc] init];
	microphone.mode = MICMODE_INTERNAL_NOISE;
	
	CommonSettings.advanced_timing = true;
	[CocoaDSCore startupCore];
	
	firmware = [[CocoaDSFirmware alloc] init];
	[firmware update];
	
	NSUInteger numberCores = [[NSProcessInfo processInfo] activeProcessorCount];
	if (numberCores >= 4)
	{
		numberCores = 4;
	}
	else if (numberCores >= 2)
	{
		numberCores = 2;
	}
	else
	{
		numberCores = 1;
	}
	
	CommonSettings.num_cores = numberCores;
	NDS_3D_ChangeCore(CORE3DLIST_SWRASTERIZE);
	
	CommonSettings.spu_advanced = true;
	CommonSettings.spuInterpolationMode = SPUInterpolation_Cosine;
	
	// Set up the sound core
	NSInteger result = SPU_ChangeSoundCore(SNDCORE_OSX, (int)SPU_BUFFER_BYTES);
	//NSInteger result = SPU_ChangeSoundCore(SNDCORE_OPENEMU, (int)SPU_BUFFER_BYTES);
	if(result == -1)
	{
		SPU_ChangeSoundCore(SNDCORE_DUMMY, 0);
	}
	
	SPU_SetSynchMode(SPU_SYNC_MODE_SYNCHRONOUS, SPU_SYNC_METHOD_P);
	SPU_SetVolume(100);
    
	return self;
}

- (void)dealloc
{
	self.microphone = nil;
	free(input);
	
	SPU_ChangeSoundCore(SNDCORE_DUMMY, 0);
	NDS_3D_ChangeCore(CORE3DLIST_SWRASTERIZE);
	[CocoaDSCore shutdownCore];
	
	self.firmware = nil;
	
	[super dealloc];
}

#pragma mark -

#pragma mark Execution

- (void)resetEmulation
{
	NDS_Reset();
	execute = true;
}

- (void)executeFrameSkippingFrame:(BOOL)skip
{
	if (skip)
	{
		NDS_SkipNextFrame();
	}
	
	[self executeFrame];
}

- (void)executeFrame
{
	[self updateNDSController];
	
	NDS_beginProcessingInput();
	NDS_endProcessingInput();
	
	NDS_exec<false>();
	SPU_Emulate_user();
}

- (BOOL)loadFileAtPath:(NSString*)path
{		
	NSURL *openEmuDataURL = [NSURL fileURLWithPath:[self batterySavesDirectoryPath]];
	
	[CocoaDSFile addURLToURLDictionary:openEmuDataURL groupKey:@PATH_OPEN_EMU fileKind:@"ROM"];
	[CocoaDSFile addURLToURLDictionary:openEmuDataURL groupKey:@PATH_OPEN_EMU fileKind:@"ROM Save"];
	[CocoaDSFile addURLToURLDictionary:openEmuDataURL groupKey:@PATH_OPEN_EMU fileKind:@"Save State"];
	[CocoaDSFile addURLToURLDictionary:openEmuDataURL groupKey:@PATH_OPEN_EMU fileKind:@"Screenshot"];
	[CocoaDSFile addURLToURLDictionary:openEmuDataURL groupKey:@PATH_OPEN_EMU fileKind:@"Video"];
	[CocoaDSFile addURLToURLDictionary:openEmuDataURL groupKey:@PATH_OPEN_EMU fileKind:@"Cheat"];
	[CocoaDSFile addURLToURLDictionary:openEmuDataURL groupKey:@PATH_OPEN_EMU fileKind:@"Sound Sample"];
	[CocoaDSFile addURLToURLDictionary:openEmuDataURL groupKey:@PATH_OPEN_EMU fileKind:@"Firmware Configuration"];
	[CocoaDSFile addURLToURLDictionary:openEmuDataURL groupKey:@PATH_OPEN_EMU fileKind:@"Lua Script"];
	
	[CocoaDSFile setupAllFilePathsWithURLDictionary:@PATH_OPEN_EMU];
	
	return [CocoaDSFile loadRom:[NSURL fileURLWithPath:path]];
}

#pragma mark Video

- (OEIntRect)screenRect
{
    return (OEIntRect){{}, [self bufferSize]};
}

- (OEIntSize)bufferSize
{
	return OESizeMake(256, 384);
}

- (const void *)videoBuffer
{
    return GPU_screen;
}

- (GLenum)pixelFormat
{
	return GL_RGBA;
}

- (GLenum)pixelType
{
	return GL_UNSIGNED_SHORT_1_5_5_5_REV;
}

- (GLenum)internalPixelFormat
{
	return GL_RGB5_A1;
}

#pragma mark Audio

- (NSUInteger)soundBufferCount
{
	return 1;
}

- (void)getAudioBuffer:(void *)buffer frameCount:(NSUInteger)frameCount bufferIndex:(NSUInteger)index
{
	// TODO: Better integrate the emulator audio with OpenEmu's audio system.
	//
	// Right now, we're using the Cocoa port's audio handling code, which works great.
	// But it's a good idea to not use it for the purposes of proving good code modularity.
	
	//if (openEmuSoundInterfaceBuffer != NULL)
	//{
	//	openEmuSoundInterfaceBuffer->read(buffer, frameCount * SPU_BUFFER_BYTES);
	//}
}

- (NSUInteger)channelCount
{
	//return SPU_NUMBER_CHANNELS;
	return 0;
}

- (NSUInteger)frameSampleCount
{
	//static NSUInteger count = (NSUInteger)(SPU_SAMPLE_RATE / DS_FRAMES_PER_SECOND) + 1;
	static NSUInteger count = 0;
	
	return count;
}

- (NSUInteger)soundBufferSize
{
	//return SPU_BUFFER_BYTES;
	return 0;
}

- (NSUInteger)frameSampleRate
{
	//return SPU_SAMPLE_RATE;
	return 0;
}

- (NSUInteger)channelCountForBuffer:(NSUInteger)buffer
{
    return [self channelCount];
}

- (NSUInteger)frameSampleCountForBuffer:(NSUInteger)buffer
{
    return [self frameSampleCount];
}

- (NSUInteger)soundBufferSizeForBuffer:(NSUInteger)buffer
{
    return [self soundBufferSize];
}

- (NSUInteger)frameSampleRateForBuffer:(NSUInteger)buffer
{
    return [self frameSampleRate];
}


#pragma mark Input

- (void)didPushNDSButton:(OENDSButton)button forPlayer:(NSUInteger)player
{
	input[button] = true;
}

- (void)didReleaseNDSButton:(OENDSButton)button forPlayer:(NSUInteger)player
{
	input[button] = false;
}

- (void) updateNDSController
{
	// Setup the DS pad.
	NDS_setPad(input[OENDSButtonRight],
			   input[OENDSButtonLeft],
			   input[OENDSButtonDown],
			   input[OENDSButtonUp],
			   input[OENDSButtonSelect],
			   input[OENDSButtonStart],
			   input[OENDSButtonB],
			   input[OENDSButtonA],
			   input[OENDSButtonY],
			   input[OENDSButtonX],
			   input[OENDSButtonL],
			   input[OENDSButtonR],
			   input[OENDSButtonDebug],
			   input[OENDSButtonLid]);
	
	// TODO: Add touch pad support in OpenEmu.
	//
	// As of March 3, 2012, reading coordinates from a view is not exposed in OpenEmu.
	// When this functionality is exposed, then the DS touch pad will be supported.
	/*
	// Setup the DS touch pad.
	if ([self isInputPressed:@"Touch"])
	{
		NSPoint touchLocation = [self inputLocation:@"Touch"];
		NDS_setTouchPos((u16)touchLocation.x, (u16)touchLocation.y);
	}
	else
	{
		NDS_releaseTouch();
	}
	*/
	// Setup the DS mic.
	NDS_setMic(input[OENDSButtonMicrophone]);
	
	if (input[OENDSButtonMicrophone])
	{
		if (self.microphone.mode == MICMODE_NONE)
		{
			[self.microphone fillWithNullSamples];
		}
		else if (self.microphone.mode == MICMODE_INTERNAL_NOISE)
		{
			[self.microphone fillWithInternalNoise];
		}
		else if (self.microphone.mode == MICMODE_WHITE_NOISE)
		{
			[self.microphone fillWithWhiteNoise];
		}
		else if (self.microphone.mode == MICMODE_SOUND_FILE)
		{
			// TODO: Need to implement. Does nothing for now.
		}
	}
}

- (NSTrackingAreaOptions)mouseTrackingOptions
{
    return 0;
}

- (void)settingWasSet:(id)aValue forKey:(NSString *)keyName
{
    DLog(@"keyName = %@", keyName);
    //[self doesNotImplementSelector:_cmd];
}

#pragma mark Save State

- (BOOL)saveStateToFileAtPath:(NSString *)fileName
{
    return [CocoaDSFile saveState:[NSURL fileURLWithPath:fileName]];
}

- (BOOL)loadStateFromFileAtPath:(NSString *)fileName
{
    return [CocoaDSFile loadState:[NSURL fileURLWithPath:fileName]];
}


@end
