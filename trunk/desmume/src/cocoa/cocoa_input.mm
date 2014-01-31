/*
	Copyright (C) 2011 Roger Manuel
	Copyright (C) 2012-2014 DeSmuME team

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

#import "cocoa_input.h"
#import "cocoa_globals.h"

#include "../NDSSystem.h"
#include "../slot2.h"
#undef BOOL


@implementation CocoaDSController

@dynamic autohold;
@synthesize micMode;
@synthesize selectedAudioFileGenerator;
@synthesize paddleAdjust;

- (id)init
{
	self = [super init];
	if (self == nil)
	{
		return self;
	}
	
	for (size_t i = 0; i < DSControllerState_StatesCount; i++)
	{
		ndsInput[i].state = false;
		ndsInput[i].turbo = false;
		ndsInput[i].turboPattern = false;
		ndsInput[i].autohold = false;
	}
	
	spinlockControllerState = OS_SPINLOCK_INIT;	
	autohold = NO;
	isAutoholdCleared = YES;
	
	micMode = MICMODE_NONE;
	selectedAudioFileGenerator = NULL;
	touchLocation = NSMakePoint(0.0f, 0.0f);
	paddleAdjust = 0;
	
	return self;
}

- (void)dealloc
{
	[super dealloc];
}

- (void) setAutohold:(BOOL)theState
{
	OSSpinLockLock(&spinlockControllerState);
	
	autohold = theState;
	
	if (autohold && isAutoholdCleared)
	{
		memset(ndsInput, 0, sizeof(ndsInput));
		isAutoholdCleared = NO;
	}
	
	if (!autohold)
	{
		for (size_t i = 0; i < DSControllerState_StatesCount; i++)
		{
			ndsInput[i].state = ndsInput[i].autohold;
		}
	}
	
	OSSpinLockUnlock(&spinlockControllerState);
}

- (BOOL) autohold
{
	OSSpinLockLock(&spinlockControllerState);
	const BOOL theState = autohold;
	OSSpinLockUnlock(&spinlockControllerState);
	return theState;
}

- (void) setControllerState:(BOOL)theState controlID:(const NSUInteger)controlID
{
	[self setControllerState:theState controlID:controlID turbo:NO];
}

- (void) setControllerState:(BOOL)theState controlID:(const NSUInteger)controlID turbo:(const BOOL)isTurboEnabled
{
	if (controlID >= DSControllerState_StatesCount)
	{
		return;
	}
	
	OSSpinLockLock(&spinlockControllerState);
	
	if (autohold)
	{
		if (theState)
		{
			ndsInput[controlID].turbo = (isTurboEnabled) ? true : false;
			ndsInput[controlID].turboPattern = (ndsInput[controlID].turbo) ? 0x5555 : 0;
			ndsInput[controlID].autohold = true;
		}
	}
	else
	{
		ndsInput[controlID].state = (theState || ndsInput[controlID].autohold);
		ndsInput[controlID].turbo = (isTurboEnabled && ndsInput[controlID].state);
		ndsInput[controlID].turboPattern = (ndsInput[controlID].turbo) ? 0x5555 : 0;
	}
	
	OSSpinLockUnlock(&spinlockControllerState);
}

- (void) setTouchState:(BOOL)theState location:(const NSPoint)theLocation
{
	OSSpinLockLock(&spinlockControllerState);
	ndsInput[DSControllerState_Touch].state = (theState) ? true : false;
	touchLocation = theLocation;
	OSSpinLockUnlock(&spinlockControllerState);
}

- (void) setMicrophoneState:(BOOL)theState inputMode:(const NSInteger)inputMode
{
	OSSpinLockLock(&spinlockControllerState);
	ndsInput[DSControllerState_Microphone].state = (theState) ? true : false;
	micMode = inputMode;
	OSSpinLockUnlock(&spinlockControllerState);
}

- (void) setSineWaveGeneratorFrequency:(const double)freq
{
	sineWaveGenerator.setFrequency(freq);
}

- (void) clearAutohold
{
	[self setAutohold:NO];
	OSSpinLockLock(&spinlockControllerState);
	
	if (!isAutoholdCleared)
	{
		memset(ndsInput, 0, sizeof(ndsInput));
		isAutoholdCleared = YES;
	}
	
	OSSpinLockUnlock(&spinlockControllerState);
}

- (void) flush
{
	OSSpinLockLock(&spinlockControllerState);
	
	const NSPoint theLocation = touchLocation;
	const NSInteger theMicMode = micMode;
	static bool flushedStates[DSControllerState_StatesCount] = {0};
	
	if (!autohold)
	{
		for (size_t i = 0; i < DSControllerState_StatesCount; i++)
		{
			flushedStates[i] = (ndsInput[i].state || ndsInput[i].autohold);
			
			if (ndsInput[i].turbo)
			{
				const bool turboState = ndsInput[i].turboPattern & 0x0001;
				flushedStates[i] = (flushedStates[i] && turboState);
				ndsInput[i].turboPattern >>= 1;
				ndsInput[i].turboPattern |= (turboState) ? 0x8000 : 0x0000;
			}
			else
			{
				flushedStates[i] = ndsInput[i].state;
			}
		}
	}
	
	OSSpinLockUnlock(&spinlockControllerState);
	
	const bool isTouchDown = flushedStates[DSControllerState_Touch];
	const bool isMicPressed = flushedStates[DSControllerState_Microphone];
	
	// Setup the DS pad.
	NDS_setPad(flushedStates[DSControllerState_Right],
			   flushedStates[DSControllerState_Left],
			   flushedStates[DSControllerState_Down],
			   flushedStates[DSControllerState_Up],
			   flushedStates[DSControllerState_Select],
			   flushedStates[DSControllerState_Start],
			   flushedStates[DSControllerState_B],
			   flushedStates[DSControllerState_A],
			   flushedStates[DSControllerState_Y],
			   flushedStates[DSControllerState_X],
			   flushedStates[DSControllerState_L],
			   flushedStates[DSControllerState_R],
			   flushedStates[DSControllerState_Debug],
			   flushedStates[DSControllerState_Lid]);
	
	// Setup the DS touch pad.
	if (isTouchDown)
	{
		NDS_setTouchPos((u16)theLocation.x, (u16)theLocation.y);
	}
	else
	{
		NDS_releaseTouch();
	}
	
	// Setup the inputs from SLOT-2 devices.
	const NDS_SLOT2_TYPE slot2DeviceType = slot2_GetSelectedType();
	switch (slot2DeviceType)
	{
		case NDS_SLOT2_GUITARGRIP:
			guitarGrip_setKey(flushedStates[DSControllerState_GuitarGrip_Green],
							  flushedStates[DSControllerState_GuitarGrip_Red],
							  flushedStates[DSControllerState_GuitarGrip_Yellow],
							  flushedStates[DSControllerState_GuitarGrip_Blue]);
			break;
			
		case NDS_SLOT2_EASYPIANO:
			piano_setKey(flushedStates[DSControllerState_Piano_C],
						 flushedStates[DSControllerState_Piano_CSharp],
						 flushedStates[DSControllerState_Piano_D],
						 flushedStates[DSControllerState_Piano_DSharp],
						 flushedStates[DSControllerState_Piano_E],
						 flushedStates[DSControllerState_Piano_F],
						 flushedStates[DSControllerState_Piano_FSharp],
						 flushedStates[DSControllerState_Piano_G],
						 flushedStates[DSControllerState_Piano_GSharp],
						 flushedStates[DSControllerState_Piano_A],
						 flushedStates[DSControllerState_Piano_ASharp],
						 flushedStates[DSControllerState_Piano_B],
						 flushedStates[DSControllerState_Piano_HighC]);
			break;
			
		case NDS_SLOT2_PADDLE:
		{
			const u16 newPaddleValue = Paddle_GetValue() + (u16)[self paddleAdjust];
			Paddle_SetValue(newPaddleValue);
			break;
		}
			
		default:
			break;
	}
	
	// Setup the DS mic.
	AudioGenerator *selectedGenerator = &nullSampleGenerator;
	switch (theMicMode)
	{
		case MICMODE_INTERNAL_NOISE:
			selectedGenerator = &internalNoiseGenerator;
			break;
			
		case MICMODE_WHITE_NOISE:
			selectedGenerator = &whiteNoiseGenerator;
			break;
			
		case MICMODE_SINE_WAVE:
			selectedGenerator = &sineWaveGenerator;
			break;
			
		case MICMODE_AUDIO_FILE:
			if (selectedAudioFileGenerator != NULL)
			{
				selectedGenerator = selectedAudioFileGenerator;
			}
			break;
			
		default:
			break;
	}
	
	NDS_setMic(isMicPressed);
	if (!isMicPressed)
	{
		selectedGenerator = &nullSampleGenerator;
		
		internalNoiseGenerator.setSamplePosition(0);
		sineWaveGenerator.setCyclePosition(0.0);
		
		if (selectedAudioFileGenerator != NULL)
		{
			selectedAudioFileGenerator->setSamplePosition(0);
		}
	}
	
	static const bool useBufferedSource = false;
	Mic_SetUseBufferedSource(useBufferedSource);
	if (useBufferedSource)
	{
		static u8 generatedSampleBuffer[(size_t)(MIC_MAX_BUFFER_SAMPLES + 0.5)] = {0};
		static const size_t requestedSamples = MIC_MAX_BUFFER_SAMPLES;
		
		const size_t availableSamples = micInputBuffer.getAvailableElements();
		if (availableSamples < requestedSamples)
		{
			micInputBuffer.drop(requestedSamples - availableSamples);
		}
		
		selectedGenerator->generateSampleBlock(requestedSamples, generatedSampleBuffer);
		micInputBuffer.write(generatedSampleBuffer, requestedSamples);
	}
	else
	{
		Mic_SetSelectedDirectSampleGenerator(selectedGenerator);
	}
}

- (void) flushEmpty
{
	// Setup the DS pad.
	NDS_setPad(false,
			   false,
			   false,
			   false,
			   false,
			   false,
			   false,
			   false,
			   false,
			   false,
			   false,
			   false,
			   false,
			   false);
	
	// Setup the DS touch pad.
	NDS_releaseTouch();
	
	// Setup the inputs from SLOT-2 devices.
	const NDS_SLOT2_TYPE slot2DeviceType = slot2_GetSelectedType();
	switch (slot2DeviceType)
	{
		case NDS_SLOT2_GUITARGRIP:
			guitarGrip_setKey(false,
							  false,
							  false,
							  false);
			break;
			
		case NDS_SLOT2_EASYPIANO:
			piano_setKey(false,
						 false,
						 false,
						 false,
						 false,
						 false,
						 false,
						 false,
						 false,
						 false,
						 false,
						 false,
						 false);
			break;
			
		case NDS_SLOT2_PADDLE:
			// Do nothing.
			break;
			
		default:
			break;
	}
	
	// Setup the DS mic.
	AudioGenerator *selectedGenerator = &nullSampleGenerator;
	
	NDS_setMic(false);
	
	static const bool useBufferedSource = false;
	Mic_SetUseBufferedSource(useBufferedSource);
	if (useBufferedSource)
	{
		static u8 generatedSampleBuffer[(size_t)(MIC_MAX_BUFFER_SAMPLES + 0.5)] = {0};
		static const size_t requestedSamples = MIC_MAX_BUFFER_SAMPLES;
		
		const size_t availableSamples = micInputBuffer.getAvailableElements();
		if (availableSamples < requestedSamples)
		{
			micInputBuffer.drop(requestedSamples - availableSamples);
		}
		
		selectedGenerator->generateSampleBlock(requestedSamples, generatedSampleBuffer);
		micInputBuffer.write(generatedSampleBuffer, requestedSamples);
	}
	else
	{
		Mic_SetSelectedDirectSampleGenerator(selectedGenerator);
	}
}

@end
