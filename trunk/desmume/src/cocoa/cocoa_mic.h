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

#import <Cocoa/Cocoa.h>


#define NUM_INTERNAL_NOISE_SAMPLES 32

static const UInt8 noiseSample[NUM_INTERNAL_NOISE_SAMPLES] =
{
	0xFC, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xF5, 0xFF, 0xFF, 0xFF, 0xFF, 0x8E, 0xFF, 
	0xF4, 0xE1, 0xBF, 0x9A, 0x71, 0x58, 0x5B, 0x5F, 0x62, 0xC2, 0x25, 0x05, 0x01, 0x01, 0x01, 0x01
};

@interface CocoaDSMic : NSObject
{
	UInt8 *buffer;
	UInt8 *readPosition;
	UInt8 *writePosition;
	NSUInteger fillCount;
	BOOL needsActivate;
	NSInteger mode;
	
	NSUInteger internalSamplePosition;
}

@property (readonly) UInt8 *buffer;
@property (readonly) UInt8 *readPosition;
@property (readonly) UInt8 *writePosition;
@property (readonly) NSUInteger fillCount;
@property (assign) BOOL needsActivate;
@property (assign) NSInteger mode;

- (void) clear;
- (NSUInteger) fillCountRemaining;
- (BOOL) isFull;
- (BOOL) isEmpty;
- (UInt8) read;
- (void) write:(UInt8)theSample;
- (UInt8) generateInternalNoiseSample;
- (UInt8) generateWhiteNoiseSample;
- (void) fillWithNullSamples;
- (void) fillWithInternalNoise;
- (void) fillWithWhiteNoise;
+ (CocoaDSMic *) masterMic;
+ (void) setMasterMic:(CocoaDSMic *)theMic;

@end
