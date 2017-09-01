/*
	Copyright (C) 2011 Roger Manuel
	Copyright (C) 2012-2017 DeSmuME team

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

#include "mic_ext.h"
#include "coreaudiosound.h"
#include "audiosamplegenerator.h"
#include "../../NDSSystem.h"
#include "../../slot2.h"
#undef BOOL

NullGenerator nullSampleGenerator;
InternalNoiseGenerator internalNoiseGenerator;
WhiteNoiseGenerator whiteNoiseGenerator;
SineWaveGenerator sineWaveGenerator(250.0, MIC_SAMPLE_RATE);


@implementation CocoaDSController

@synthesize delegate;
@dynamic autohold;
@synthesize paddleAdjust;
@synthesize stylusPressure;
@dynamic isHardwareMicAvailable;
@dynamic isHardwareMicIdle;
@dynamic isHardwareMicInClip;
@synthesize micLevel;
@dynamic hardwareMicEnabled;
@dynamic hardwareMicLocked;
@dynamic hardwareMicGain;
@synthesize hardwareMicMute;
@dynamic hardwareMicPause;
@dynamic softwareMicState;
@dynamic softwareMicMode;
@synthesize micMode;
@synthesize CAInputDevice;
@synthesize softwareMicSampleGenerator;
@synthesize selectedAudioFileGenerator;
@synthesize hardwareMicInfoString;
@synthesize hardwareMicNameString;
@synthesize hardwareMicManufacturerString;
@synthesize hardwareMicSampleRateString;

- (id)init
{
	self = [super init];
	if (self == nil)
	{
		return self;
	}
	
	for (size_t i = 0; i < NDSInputID_InputCount; i++)
	{
		clientInput[i].isPressed = false;
		clientInput[i].turbo = false;
		clientInput[i].turboPattern = 0;
		clientInput[i].turboPatternStep = 0;
		clientInput[i].autohold = false;
	}
	
	delegate = nil;
	spinlockControllerState = OS_SPINLOCK_INIT;	
	autohold = NO;
	_isAutoholdCleared = YES;
	_useHardwareMic = NO;
	_availableMicSamples = 0;
	
	micLevel = 0.0f;
	_micLevelTotal = 0.0f;
	_micLevelsRead = 0.0f;
	
	micMode = MICMODE_NONE;
	selectedAudioFileGenerator = NULL;
	CAInputDevice = new CoreAudioInput;
	CAInputDevice->SetCallbackHardwareStateChanged(&CAHardwareStateChangedCallback, self, NULL);
	CAInputDevice->SetCallbackHardwareGainChanged(&CAHardwareGainChangedCallback, self, NULL);
	softwareMicSampleGenerator = &nullSampleGenerator;
	touchLocation = NSMakePoint(0.0f, 0.0f);
	paddleAdjust = 0;
	
	hardwareMicInfoString = @"No hardware input detected.";
	hardwareMicNameString = @"No hardware input detected.";
	hardwareMicManufacturerString = @"No hardware input detected.";
	hardwareMicSampleRateString = @"No hardware input detected.";
	
	Mic_SetResetCallback(&CAResetCallback, self, NULL);
	Mic_SetSampleReadCallback(&CASampleReadCallback, self, NULL);
	
	return self;
}

- (void)dealloc
{
	delete CAInputDevice;
	
	[self setDelegate:nil];
	[self setHardwareMicInfoString:nil];
	[self setHardwareMicNameString:nil];
	[self setHardwareMicManufacturerString:nil];
	[self setHardwareMicSampleRateString:nil];
	
	[super dealloc];
}

- (BOOL) isHardwareMicAvailable
{
	return ( CAInputDevice->IsHardwareEnabled() &&
			!CAInputDevice->IsHardwareLocked() &&
			!CAInputDevice->GetPauseState() ) ? YES : NO;
}

- (BOOL) isHardwareMicIdle
{
	return (micLevel < MIC_NULL_LEVEL_THRESHOLD);
}

- (BOOL) isHardwareMicInClip
{
	return (micLevel >= MIC_CLIP_LEVEL_THRESHOLD);
}

- (void) setHardwareMicEnabled:(BOOL)micEnabled
{
	if (micEnabled)
	{
		CAInputDevice->Start();
	}
	else
	{
		CAInputDevice->Stop();
	}
}

- (BOOL) hardwareMicEnabled
{
	return (CAInputDevice->IsHardwareEnabled()) ? YES : NO;
}

- (BOOL) hardwareMicLocked
{
	return (CAInputDevice->IsHardwareLocked()) ? YES : NO;
}

- (void) setHardwareMicGain:(float)micGain
{
	CAInputDevice->SetGain(micGain);
}

- (float) hardwareMicGain
{
	return CAInputDevice->GetGain();
}

- (void) setHardwareMicPause:(BOOL)isMicPaused
{
	bool pauseState = (isMicPaused || [self hardwareMicMute]) ? true : false;
	CAInputDevice->SetPauseState(pauseState);
}

- (BOOL) hardwareMicPause
{
	return (CAInputDevice->GetPauseState()) ? YES : NO;
}

- (void) setSoftwareMicState:(BOOL)theState
{
	OSSpinLockLock(&spinlockControllerState);
	clientInput[NDSInputID_Microphone].isPressed = (theState) ? true : false;
	OSSpinLockUnlock(&spinlockControllerState);
}

- (BOOL) softwareMicState
{
	OSSpinLockLock(&spinlockControllerState);
	BOOL theState = (clientInput[NDSInputID_Microphone].isPressed) ? YES : NO;
	OSSpinLockUnlock(&spinlockControllerState);
	return theState;
}

- (void) setSoftwareMicMode:(NSInteger)theMode
{
	OSSpinLockLock(&spinlockControllerState);
	micMode = theMode;
	OSSpinLockUnlock(&spinlockControllerState);
}

- (NSInteger) softwareMicMode
{
	OSSpinLockLock(&spinlockControllerState);
	NSInteger theMode = micMode;
	OSSpinLockUnlock(&spinlockControllerState);
	return theMode;
}

- (void) setAutohold:(BOOL)theState
{
	OSSpinLockLock(&spinlockControllerState);
	
	autohold = theState;
	
	if (autohold && _isAutoholdCleared)
	{
		memset(clientInput, 0, sizeof(clientInput));
		_isAutoholdCleared = NO;
	}
	
	if (!autohold)
	{
		for (size_t i = 0; i < NDSInputID_InputCount; i++)
		{
			clientInput[i].isPressed = clientInput[i].autohold;
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
	[self setControllerState:theState controlID:controlID turbo:NO turboPattern:0];
}

- (void) setControllerState:(BOOL)theState controlID:(const NSUInteger)controlID turbo:(const BOOL)isTurboEnabled turboPattern:(uint32_t)turboPattern
{
	if (controlID >= NDSInputID_InputCount)
	{
		return;
	}
	
	OSSpinLockLock(&spinlockControllerState);
	
	if (autohold)
	{
		if (theState)
		{
			clientInput[controlID].turbo = (isTurboEnabled) ? true : false;
			clientInput[controlID].turboPattern = (clientInput[controlID].turbo) ? turboPattern : 0;
			clientInput[controlID].autohold = true;
			
			if (!clientInput[controlID].turbo)
			{
				clientInput[controlID].turboPatternStep = 0;
			}
		}
	}
	else
	{
		clientInput[controlID].isPressed = (theState || clientInput[controlID].autohold);
		clientInput[controlID].turbo = (isTurboEnabled && clientInput[controlID].isPressed);
		clientInput[controlID].turboPattern = (clientInput[controlID].turbo) ? turboPattern : 0;
		
		if (!clientInput[controlID].turbo)
		{
			clientInput[controlID].turboPatternStep = 0;
		}
	}
	
	OSSpinLockUnlock(&spinlockControllerState);
}

- (void) setTouchState:(BOOL)theState location:(const NSPoint)theLocation
{
	OSSpinLockLock(&spinlockControllerState);
	clientInput[NDSInputID_Touch].isPressed = (theState) ? true : false;
	touchLocation = theLocation;
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
	
	if (!_isAutoholdCleared)
	{
		memset(clientInput, 0, sizeof(clientInput));
		_isAutoholdCleared = YES;
	}
	
	OSSpinLockUnlock(&spinlockControllerState);
}

- (void) flush
{
	OSSpinLockLock(&spinlockControllerState);
	
	const NSPoint theLocation = touchLocation;
	const NSInteger theMicMode = micMode;
	bool flushedStates[NDSInputID_InputCount] = {0};
	
	if (!autohold)
	{
		for (size_t i = 0; i < NDSInputID_InputCount; i++)
		{
			flushedStates[i] = (clientInput[i].isPressed || clientInput[i].autohold);
			
			if (clientInput[i].turbo)
			{
				const bool turboState = (clientInput[i].turboPattern >> clientInput[i].turboPatternStep) & 0x00000001;
				flushedStates[i] = (flushedStates[i] && turboState);
				
				clientInput[i].turboPatternStep++;
				if (clientInput[i].turboPatternStep >= 32)
				{
					clientInput[i].turboPatternStep = 0;
				}
			}
			else
			{
				flushedStates[i] = clientInput[i].isPressed;
			}
		}
	}
	
	OSSpinLockUnlock(&spinlockControllerState);
	
	const bool isTouchDown = flushedStates[NDSInputID_Touch];
	const bool isMicPressed = flushedStates[NDSInputID_Microphone];
	
	// Setup the DS pad.
	NDS_setPad(flushedStates[NDSInputID_Right],
			   flushedStates[NDSInputID_Left],
			   flushedStates[NDSInputID_Down],
			   flushedStates[NDSInputID_Up],
			   flushedStates[NDSInputID_Select],
			   flushedStates[NDSInputID_Start],
			   flushedStates[NDSInputID_B],
			   flushedStates[NDSInputID_A],
			   flushedStates[NDSInputID_Y],
			   flushedStates[NDSInputID_X],
			   flushedStates[NDSInputID_L],
			   flushedStates[NDSInputID_R],
			   flushedStates[NDSInputID_Debug],
			   flushedStates[NDSInputID_Lid]);
	
	// Setup the DS touch pad.
	CommonSettings.StylusPressure = (int)[self stylusPressure];
	
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
			guitarGrip_setKey(flushedStates[NDSInputID_GuitarGrip_Green],
							  flushedStates[NDSInputID_GuitarGrip_Red],
							  flushedStates[NDSInputID_GuitarGrip_Yellow],
							  flushedStates[NDSInputID_GuitarGrip_Blue]);
			break;
			
		case NDS_SLOT2_EASYPIANO:
			piano_setKey(flushedStates[NDSInputID_Piano_C],
						 flushedStates[NDSInputID_Piano_CSharp],
						 flushedStates[NDSInputID_Piano_D],
						 flushedStates[NDSInputID_Piano_DSharp],
						 flushedStates[NDSInputID_Piano_E],
						 flushedStates[NDSInputID_Piano_F],
						 flushedStates[NDSInputID_Piano_FSharp],
						 flushedStates[NDSInputID_Piano_G],
						 flushedStates[NDSInputID_Piano_GSharp],
						 flushedStates[NDSInputID_Piano_A],
						 flushedStates[NDSInputID_Piano_ASharp],
						 flushedStates[NDSInputID_Piano_B],
						 flushedStates[NDSInputID_Piano_HighC]);
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
	
	if (isMicPressed)
	{
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
	}
	else
	{
		selectedGenerator = &nullSampleGenerator;
		internalNoiseGenerator.setSamplePosition(0);
		sineWaveGenerator.setCyclePosition(0.0);
		
		if (selectedAudioFileGenerator != NULL)
		{
			selectedAudioFileGenerator->setSamplePosition(0);
		}
	}
	
	_useHardwareMic = (isMicPressed) ? NO : YES;
	softwareMicSampleGenerator = selectedGenerator;
	NDS_setMic(isMicPressed);
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
	_useHardwareMic = NO;
	softwareMicSampleGenerator = &nullSampleGenerator;
	NDS_setMic(false);
}

- (void) reset
{
	for (size_t i = 0; i < NDSInputID_InputCount; i++)
	{
		memset(clientInput, 0, sizeof(clientInput));
	}
	
	[self setAutohold:NO];
	[self setMicLevel:0.0f];
	
	_isAutoholdCleared = YES;
	_availableMicSamples = 0;
	_micLevelTotal = 0.0f;
	_micLevelsRead = 0.0f;
}

- (void) clearMicLevelMeasure
{
	_micLevelTotal = 0.0f;
	_micLevelsRead = 0.0f;
}

- (void) updateMicLevel
{
	float avgMicLevel = _micLevelTotal / _micLevelsRead;
	
	NSAutoreleasePool *tempPool = [[NSAutoreleasePool alloc] init];
	[self setMicLevel:avgMicLevel];
	
	if (delegate != nil && [delegate respondsToSelector:@selector(doMicLevelUpdateFromController:)])
	{
		[[self delegate] doMicLevelUpdateFromController:self];
	}
	
	[tempPool release];
}

- (uint8_t) handleMicSampleRead:(CoreAudioInput *)caInput softwareMic:(AudioGenerator *)sampleGenerator
{
	uint8_t theSample = MIC_NULL_SAMPLE_VALUE;
	
	if (_useHardwareMic && (caInput != NULL))
	{
		if (caInput->GetPauseState())
		{
			return theSample;
		}
		else
		{
			if (_availableMicSamples == 0)
			{
				_availableMicSamples = CAInputDevice->Pull();
			}
			
			caInput->_samplesConverted->read(&theSample, 1);
			theSample >>= 1; // Samples from CoreAudio are 8-bit, so we need to convert the sample to 7-bit
			_availableMicSamples--;
		}
	}
	else
	{
		theSample = sampleGenerator->generateSample();
	}
	
	_micLevelTotal += (float)( (MIC_NULL_SAMPLE_VALUE > theSample) ? MIC_NULL_SAMPLE_VALUE - theSample : theSample - MIC_NULL_SAMPLE_VALUE );
	_micLevelsRead += 1.0f;
	return theSample;
}

- (void) handleMicHardwareStateChanged:(CoreAudioInputDeviceInfo *)deviceInfo
							 isEnabled:(BOOL)isHardwareEnabled
							  isLocked:(BOOL)isHardwareLocked
{
	NSAutoreleasePool *tempPool = [[NSAutoreleasePool alloc] init];
	
	if (deviceInfo->objectID == kAudioObjectUnknown)
	{
		[self setHardwareMicInfoString:@"No hardware input detected."];
		[self setHardwareMicNameString:@"No hardware input detected."];
		[self setHardwareMicManufacturerString:@"No hardware input detected."];
		[self setHardwareMicSampleRateString:@"No hardware input detected."];
		
	}
	else
	{
		[self setHardwareMicInfoString:[NSString stringWithFormat:@"%@\nSample Rate: %1.1f Hz",
										(NSString *)deviceInfo->name,
										(double)deviceInfo->sampleRate]];
		[self setHardwareMicNameString:(NSString *)deviceInfo->name];
		[self setHardwareMicManufacturerString:(NSString *)deviceInfo->manufacturer];
		[self setHardwareMicSampleRateString:[NSString stringWithFormat:@"%1.1f Hz", (double)deviceInfo->sampleRate]];
	}
	
	[self clearMicLevelMeasure];
	[self setMicLevel:0.0f];
	
	if (delegate != nil && [delegate respondsToSelector:@selector(doMicHardwareStateChangedFromController:isEnabled:isLocked:)])
	{
		[[self delegate] doMicHardwareStateChangedFromController:self
													   isEnabled:isHardwareEnabled
														isLocked:isHardwareLocked];
	}
	
	[tempPool release];
}

- (void) handleMicHardwareGainChanged:(float)gainValue
{
	if (delegate != nil && [delegate respondsToSelector:@selector(doMicHardwareGainChangedFromController:gain:)])
	{
		NSAutoreleasePool *tempPool = [[NSAutoreleasePool alloc] init];
		[[self delegate] doMicHardwareGainChangedFromController:self gain:gainValue];
		[tempPool release];
	}
}

@end

void CAResetCallback(void *inParam1, void *inParam2)
{
	CocoaDSController *cdsController = (CocoaDSController *)inParam1;
	[cdsController CAInputDevice]->Start();
}

uint8_t CASampleReadCallback(void *inParam1, void *inParam2)
{
	CocoaDSController *cdsController = (CocoaDSController *)inParam1;
	return [cdsController handleMicSampleRead:[cdsController CAInputDevice] softwareMic:[cdsController softwareMicSampleGenerator]];
}

void CAHardwareStateChangedCallback(CoreAudioInputDeviceInfo *deviceInfo,
									const bool isHardwareEnabled,
									const bool isHardwareLocked,
									void *inParam1,
									void *inParam2)
{
	CocoaDSController *cdsController = (CocoaDSController *)inParam1;
	[cdsController handleMicHardwareStateChanged:(CoreAudioInputDeviceInfo *)deviceInfo
									   isEnabled:((isHardwareEnabled) ? YES : NO)
										isLocked:((isHardwareLocked) ? YES : NO)];
}

void CAHardwareGainChangedCallback(float normalizedGain, void *inParam1, void *inParam2)
{
	CocoaDSController *cdsController = (CocoaDSController *)inParam1;
	[cdsController handleMicHardwareGainChanged:normalizedGain];
}
