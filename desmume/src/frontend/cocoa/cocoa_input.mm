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

#include "ClientInputHandler.h"


@implementation CocoaDSController

@synthesize delegate;
@dynamic inputHandler;
@dynamic autohold;
@dynamic paddleAdjust;
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
@synthesize CAInputDevice;
@dynamic softwareMicSampleGenerator;
@dynamic selectedAudioFileGenerator;
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
		
	delegate = nil;
	inputHandler = new ClientInputHandler;
	_availableMicSamples = 0;
	
	micLevel = 0.0f;
	_micLevelTotal = 0.0f;
	_micLevelsRead = 0.0f;
	
	CAInputDevice = new CoreAudioInput;
	CAInputDevice->SetCallbackHardwareStateChanged(&CAHardwareStateChangedCallback, self, NULL);
	CAInputDevice->SetCallbackHardwareGainChanged(&CAHardwareGainChangedCallback, self, NULL);
	
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

- (ClientInputHandler *) inputHandler
{
	return inputHandler;
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

- (void) setSoftwareMicState:(BOOL)theState mode:(NSInteger)micMode
{
	inputHandler->SetClientSoftwareMicState((theState) ? true : false, (MicrophoneMode)micMode);
}

- (BOOL) softwareMicState
{
	return (inputHandler->GetClientSoftwareMicState()) ? YES : NO;
}

- (AudioGenerator *) softwareMicSampleGenerator
{
	return inputHandler->GetClientSoftwareMicSampleGenerator();
}

- (void) setSelectedAudioFileGenerator:(AudioSampleBlockGenerator *)audioGenerator
{
	inputHandler->SetClientSelectedAudioFileGenerator(audioGenerator);
}

- (AudioSampleBlockGenerator *) selectedAudioFileGenerator
{
	return inputHandler->GetClientSelectedAudioFileGenerator();
}

- (void) setAutohold:(BOOL)theState
{
	inputHandler->SetEnableAutohold((theState) ? true: false);
}

- (BOOL) autohold
{
	return (inputHandler->GetEnableAutohold()) ? YES : NO;
}

- (void) setPaddleAdjust:(NSInteger)paddleAdjust
{
	inputHandler->SetClientPaddleAdjust((int16_t)paddleAdjust);
}

- (NSInteger) paddleAdjust
{
	return (NSInteger)inputHandler->GetClientPaddleAdjust();
}

- (void) setControllerState:(BOOL)theState controlID:(const NSUInteger)controlID
{
	inputHandler->SetClientInputStateUsingID((NDSInputID)controlID,
											 (theState) ? true : false,
											 false,
											 0,
											 0);
}

- (void) setControllerState:(BOOL)theState controlID:(const NSUInteger)controlID turbo:(const BOOL)isTurboEnabled turboPattern:(uint32_t)turboPattern turboPatternLength:(uint32_t)turboPatternLength
{
	inputHandler->SetClientInputStateUsingID((NDSInputID)controlID,
											 (theState) ? true : false,
											 (isTurboEnabled) ? true : false,
											 turboPattern,
											 turboPatternLength);
}

- (void) setTouchState:(BOOL)theState location:(const NSPoint)theLocation
{
	inputHandler->SetClientTouchState((theState) ? true : false,
									  (uint8_t)(theLocation.x + 0.3f),
									  (uint8_t)(theLocation.y + 0.3f),
									  (uint8_t)[self stylusPressure]);
}

- (void) setSineWaveGeneratorFrequency:(const double)freq
{
	inputHandler->SetSineWaveFrequency(freq);
}

- (void) clearAutohold
{
	inputHandler->ClearAutohold();
}

- (void) reset
{
	[self setAutohold:NO];
	[self setMicLevel:0.0f];
	[self clearMicLevelMeasure];
	
	_availableMicSamples = 0;
}

- (void) clearMicLevelMeasure
{
	_micLevelTotal = 0.0f;
	_micLevelsRead = 0.0f;
}

- (void) updateMicLevel
{
	float avgMicLevel = (_micLevelsRead != 0) ? _micLevelTotal / _micLevelsRead : 0.0f;
	
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
	
	if (!inputHandler->GetClientSoftwareMicStateApplied() && (caInput != NULL))
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
