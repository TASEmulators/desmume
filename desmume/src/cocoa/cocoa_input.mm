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
#import "cocoa_mic.h"

#include "../NDSSystem.h"
#undef BOOL


@implementation CocoaDSController

@synthesize cdsMic;

- (id)init
{
	return [self initWithMic:nil];
}

- (id) initWithMic:(CocoaDSMic *)theMic
{
	self = [super init];
	if (self == nil)
	{
		return self;
	}
	
	for (unsigned int i = 0; i < DSControllerState_StatesCount; i++)
	{
		controllerState[i] = false;
	}
	
	spinlockControllerState = OS_SPINLOCK_INIT;
	
	cdsMic = [theMic retain];
	touchLocation = NSMakePoint(0.0f, 0.0f);
	
	return self;
}

- (void)dealloc
{
	self.cdsMic = nil;
	
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
	self.cdsMic.mode = inputMode;
	OSSpinLockUnlock(&spinlockControllerState);
}

- (void) flush
{
	OSSpinLockLock(&spinlockControllerState);
	
	NSPoint theLocation = touchLocation;
	NSInteger micMode = cdsMic.mode;
	bool flushedStates[DSControllerState_StatesCount] = {0};
	
	for (unsigned int i = 0; i < DSControllerState_StatesCount; i++)
	{
		flushedStates[i] = controllerState[i];
	}
	
	OSSpinLockUnlock(&spinlockControllerState);
	
	bool isTouchDown = flushedStates[DSControllerState_Touch];
	bool isMicPressed = flushedStates[DSControllerState_Microphone];
	
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
	NDS_setMic(isMicPressed);
	
	if (isMicPressed)
	{
		if (micMode == MICMODE_NONE)
		{
			[cdsMic fillWithNullSamples];
		}
		else if (micMode == MICMODE_INTERNAL_NOISE)
		{
			[cdsMic fillWithInternalNoise];
		}
		else if (micMode == MICMODE_WHITE_NOISE)
		{
			[cdsMic fillWithWhiteNoise];
		}
		else if (micMode == MICMODE_SOUND_FILE)
		{
			// TODO: Need to implement. Does nothing for now.
		}
	}
}

@end
