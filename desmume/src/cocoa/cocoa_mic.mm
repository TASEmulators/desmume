/*
	Copyright (C) 2011 Roger Manuel
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

#import "cocoa_mic.h"
#import "cocoa_globals.h"

#include "../NDSSystem.h"
#undef BOOL

static CocoaDSMic *masterMic = nil;

@implementation CocoaDSMic

@synthesize buffer;
@synthesize readPosition;
@synthesize writePosition;
@synthesize fillCount;
@synthesize needsActivate;
@synthesize mode;

- (id)init
{
	self = [super init];
	if (self == nil)
	{
		return self;
	}
	
	UInt8 *newBuffer = (UInt8 *)malloc(MIC_BUFFER_SIZE);
	if (newBuffer == nil)
	{
		[self release];
		return nil;
	}
	
	buffer = newBuffer;
	[self clear];
	
	internalSamplePosition = 0;
	needsActivate = YES;
	mode = MICMODE_NONE;
	
	[CocoaDSMic setMasterMic:self];
	
	return self;
}

- (void)dealloc
{
	if ([CocoaDSMic masterMic] == self)
	{
		[CocoaDSMic setMasterMic:nil];
	}
	
	free(buffer);
	buffer = NULL;
	
	[super dealloc];
}

- (void) mode:(NSInteger)theMode
{
	switch (theMode)
	{
		case MICMODE_NONE:
			self.needsActivate = YES;
			break;
			
		case MICMODE_INTERNAL_NOISE:
			self.needsActivate = YES;
			break;
			
		case MICMODE_SOUND_FILE:
			self.needsActivate = YES;
			break;
			
		case MICMODE_WHITE_NOISE:
			self.needsActivate = YES;
			break;
			
		case MICMODE_PHYSICAL:
			self.needsActivate = NO;
			break;
			
		default:
			break;
	}
}

- (void) clear
{
	memset(buffer, MIC_NULL_SAMPLE_VALUE, MIC_BUFFER_SIZE);
	readPosition = buffer;
	writePosition = buffer;
	fillCount = 0;
}

- (NSUInteger) fillCountRemaining
{
	return (MIC_MAX_BUFFER_SAMPLES - self.fillCount);
}

- (BOOL) isFull
{
	return (self.fillCount >= MIC_MAX_BUFFER_SAMPLES);
}

- (BOOL) isEmpty
{
	return (self.fillCount == 0);
}

- (UInt8) read
{
	UInt8 theSample = MIC_NULL_SAMPLE_VALUE;
	
	bool isMicActive = NDS_getFinalUserInput().mic.micButtonPressed;
	if ([self isEmpty] || (self.needsActivate && !isMicActive))
	{
		return theSample;
	}
	
	theSample = *readPosition;
	readPosition++;
	fillCount--;
	
	// Move the pointer back to start if we reach the end of the memory block.
	if (readPosition >= (buffer + MIC_BUFFER_SIZE))
	{
		readPosition = buffer;
	}
	
	return theSample;
}

- (void) write:(UInt8)theSample
{
	if ([self isFull])
	{
		return;
	}
	
	*writePosition = theSample;
	writePosition++;
	fillCount++;
	
	// Move the pointer back to start if we reach the end of the memory block.
	if (writePosition >= (buffer + MIC_BUFFER_SIZE))
	{
		writePosition = buffer;
	}
}

- (UInt8) generateInternalNoiseSample
{
	internalSamplePosition++;
	if (internalSamplePosition >= NUM_INTERNAL_NOISE_SAMPLES)
	{
		internalSamplePosition = 0;
	}
	
	return noiseSample[internalSamplePosition];
}

- (UInt8) generateWhiteNoiseSample
{
	return (UInt8)(rand() & 0xFF);
}

- (void) fillWithNullSamples
{
	while (self.fillCount < MIC_MAX_BUFFER_SAMPLES)
	{
		[self write:MIC_NULL_SAMPLE_VALUE];
	}
}

- (void) fillWithInternalNoise
{
	while (self.fillCount < MIC_MAX_BUFFER_SAMPLES)
	{
		[self write:[self generateInternalNoiseSample]];
	}
}

- (void) fillWithWhiteNoise
{
	while (self.fillCount < MIC_MAX_BUFFER_SAMPLES)
	{
		[self write:[self generateWhiteNoiseSample]];
	}
}

+ (CocoaDSMic *) masterMic
{
	return masterMic;
}

+ (void) setMasterMic:(CocoaDSMic *)theMic
{
	masterMic = theMic;
}

@end

BOOL Mic_Init()
{
	// Do nothing. The CocoaDSMic object will take care of this. 
	return TRUE;
}

void Mic_Reset()
{
	// Do nothing.
}

void Mic_DeInit()
{
	// Do nothing. The CocoaDSMic object will take care of this. 
}

u8 Mic_ReadSample()
{
	return [masterMic read];
}

void mic_savestate(EMUFILE* os)
{
	write32le(-1, os);
}

bool mic_loadstate(EMUFILE* is, int size)
{
	is->fseek(size, SEEK_CUR);
	return true;
}
