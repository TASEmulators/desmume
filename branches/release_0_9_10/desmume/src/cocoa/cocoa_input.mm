/*
	Copyright (C) 2011 Roger Manuel
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

#import "cocoa_input.h"
#import "cocoa_globals.h"

#include "../NDSSystem.h"
#undef BOOL


@implementation CocoaDSController

@synthesize micMode;
@synthesize selectedAudioFileGenerator;

- (id)init
{
	self = [super init];
	if (self == nil)
	{
		return self;
	}
	
	for (size_t i = 0; i < DSControllerState_StatesCount; i++)
	{
		controllerState[i] = false;
	}
	
	spinlockControllerState = OS_SPINLOCK_INIT;	
	micMode = MICMODE_NONE;
	selectedAudioFileGenerator = NULL;
	touchLocation = NSMakePoint(0.0f, 0.0f);
	
	return self;
}

- (void)dealloc
{
	[super dealloc];
}

- (void) setControllerState:(BOOL)theState controlID:(const NSUInteger)controlID 
{
	if (controlID >= DSControllerState_StatesCount)
	{
		return;
	}
	
	OSSpinLockLock(&spinlockControllerState);
	controllerState[controlID] = (theState) ? true : false;
	OSSpinLockUnlock(&spinlockControllerState);
}

- (void) setTouchState:(BOOL)theState location:(const NSPoint)theLocation
{
	OSSpinLockLock(&spinlockControllerState);
	controllerState[DSControllerState_Touch] = (theState) ? true : false;
	touchLocation = theLocation;
	OSSpinLockUnlock(&spinlockControllerState);
}

- (void) setMicrophoneState:(BOOL)theState inputMode:(const NSInteger)inputMode
{
	OSSpinLockLock(&spinlockControllerState);
	controllerState[DSControllerState_Microphone] = (theState) ? true : false;
	micMode = inputMode;
	OSSpinLockUnlock(&spinlockControllerState);
}

- (void) setSineWaveGeneratorFrequency:(const double)freq
{
	sineWaveGenerator.setFrequency(freq);
}

- (void) flush
{
	OSSpinLockLock(&spinlockControllerState);
	
	const NSPoint theLocation = touchLocation;
	const NSInteger theMicMode = micMode;
	static bool flushedStates[DSControllerState_StatesCount] = {0};
	
	for (size_t i = 0; i < DSControllerState_StatesCount; i++)
	{
		flushedStates[i] = controllerState[i];
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

@end
