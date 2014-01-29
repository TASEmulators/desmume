/*
	Copyright (C) 2011 Roger Manuel
	Copyright (C) 2012-2014 DeSmuME Team

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
#include <libkern/OSAtomic.h>
#include "mic_ext.h"
#undef BOOL

enum
{
	DSControllerState_Right = 0,
	DSControllerState_Left,
	DSControllerState_Down,
	DSControllerState_Up,
	DSControllerState_Select,
	DSControllerState_Start,
	DSControllerState_B,
	DSControllerState_A,
	DSControllerState_Y,
	DSControllerState_X,
	DSControllerState_L,
	DSControllerState_R,
	DSControllerState_Debug,
	DSControllerState_Lid,
	
	DSControllerState_Touch,
	DSControllerState_Microphone,
	
	DSControllerState_GuitarGrip_Green,
	DSControllerState_GuitarGrip_Red,
	DSControllerState_GuitarGrip_Yellow,
	DSControllerState_GuitarGrip_Blue,
	DSControllerState_Piano_C,
	DSControllerState_Piano_CSharp,
	DSControllerState_Piano_D,
	DSControllerState_Piano_DSharp,
	DSControllerState_Piano_E,
	DSControllerState_Piano_F,
	DSControllerState_Piano_FSharp,
	DSControllerState_Piano_G,
	DSControllerState_Piano_GSharp,
	DSControllerState_Piano_A,
	DSControllerState_Piano_ASharp,
	DSControllerState_Piano_B,
	DSControllerState_Piano_HighC,
	DSControllerState_Paddle,
	
	DSControllerState_StatesCount
};

@interface CocoaDSController : NSObject
{
	NSInteger micMode;
	NSPoint touchLocation;
	bool controllerState[DSControllerState_StatesCount];
	AudioSampleBlockGenerator *selectedAudioFileGenerator;
	NSInteger paddleAdjust;
	
	OSSpinLock spinlockControllerState;
}

@property (assign) NSInteger micMode;
@property (assign) AudioSampleBlockGenerator *selectedAudioFileGenerator;
@property (assign) NSInteger paddleAdjust;

- (void) setControllerState:(BOOL)theState controlID:(const NSUInteger)controlID;
- (void) setTouchState:(BOOL)theState location:(const NSPoint)theLocation;
- (void) setMicrophoneState:(BOOL)theState inputMode:(const NSInteger)inputMode;
- (void) setSineWaveGeneratorFrequency:(const double)freq;
- (void) flush;
- (void) flushEmpty;

@end
