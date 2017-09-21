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


@implementation CocoaDSController

@synthesize delegate;
@dynamic inputHandler;
@dynamic autohold;
@dynamic paddleAdjust;
@synthesize stylusPressure;
@dynamic isHardwareMicAvailable;
@dynamic isHardwareMicIdle;
@dynamic isHardwareMicInClip;
@dynamic micLevel;
@dynamic hardwareMicGain;
@dynamic hardwareMicMute;
@dynamic hardwareMicPause;
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
		
	hardwareMicInfoString = @"No hardware input detected.";
	hardwareMicNameString = @"No hardware input detected.";
	hardwareMicManufacturerString = @"No hardware input detected.";
	hardwareMicSampleRateString = @"No hardware input detected.";
	
	inputHandler = new MacInputHandler;
	((MacInputHandler *)inputHandler)->SetCocoaController(self);
	
	return self;
}

- (void)dealloc
{
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
	return (inputHandler->IsHardwareMicAvailable()) ? YES : NO;
}

- (BOOL) isHardwareMicIdle
{
	return (inputHandler->IsMicrophoneIdle()) ? YES : NO;
}

- (BOOL) isHardwareMicInClip
{
	return (inputHandler->IsMicrophoneClipping()) ? YES : NO;
}

- (void) setMicLevel:(float)micLevelValue
{
	// This method doesn't set the mic level, since the mic level is always an internally
	// calculated value. What this method actually does is trigger updates for any
	// KVO-compliant controls that are associated with this property.
	
	if ( (delegate != nil) && [delegate respondsToSelector:@selector(doMicLevelUpdateFromController:)] )
	{
		NSAutoreleasePool *tempPool = [[NSAutoreleasePool alloc] init];
		[[self delegate] doMicLevelUpdateFromController:self];
		[tempPool release];
	}
}

- (float) micLevel
{
	return inputHandler->GetAverageMicLevel();
}

- (void) setHardwareMicGain:(float)micGain
{
	inputHandler->SetHardwareMicGainAsNormalized(micGain);
}

- (float) hardwareMicGain
{
	return inputHandler->GetHardwareMicNormalizedGain();
}

- (void) setHardwareMicMute:(BOOL)isMicMuted
{
	inputHandler->SetHardwareMicMute((isMicMuted) ? true : false);
}

- (BOOL) hardwareMicMute
{
	return (inputHandler->GetHardwareMicMute()) ? YES : NO;
}

- (void) setHardwareMicPause:(BOOL)isMicPaused
{
	inputHandler->SetHardwareMicPause((isMicPaused) ? true : false);
}

- (BOOL) hardwareMicPause
{
	return (inputHandler->GetHardwareMicPause()) ? YES : NO;
}

- (void) setSoftwareMicState:(BOOL)theState mode:(NSInteger)micMode
{
	inputHandler->SetClientSoftwareMicState((theState) ? true : false, (MicrophoneMode)micMode);
}

- (BOOL) softwareMicState
{
	return (inputHandler->GetClientSoftwareMicState()) ? YES : NO;
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
	
	inputHandler->ResetHardwareMic();
}

- (void) startHardwareMicDevice
{
	((MacInputHandler *)inputHandler)->StartHardwareMicDevice();
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
	
	inputHandler->ClearAverageMicLevel();
	inputHandler->ReportAverageMicLevel();
	
	if ( (delegate != nil) && [delegate respondsToSelector:@selector(doMicHardwareStateChangedFromController:isEnabled:isLocked:)] )
	{
		[[self delegate] doMicHardwareStateChangedFromController:self
													   isEnabled:isHardwareEnabled
														isLocked:isHardwareLocked];
	}
	
	[tempPool release];
}

- (void) handleMicHardwareGainChanged:(float)gainValue
{
	if ( (delegate != nil) && [delegate respondsToSelector:@selector(doMicHardwareGainChangedFromController:gain:)] )
	{
		NSAutoreleasePool *tempPool = [[NSAutoreleasePool alloc] init];
		[[self delegate] doMicHardwareGainChangedFromController:self gain:gainValue];
		[tempPool release];
	}
}

@end

MacInputHandler::MacInputHandler()
{
	_cdsController = nil;
	_CAInputDevice = new CoreAudioInput;
	_hardwareMicSampleGenerator = _CAInputDevice;
	_isHardwareMicMuted = false;
	
	_CAInputDevice->SetCallbackHardwareStateChanged(&CAHardwareStateChangedCallback, _cdsController, NULL);
	_CAInputDevice->SetCallbackHardwareGainChanged(&CAHardwareGainChangedCallback, _cdsController, NULL);
	
	Mic_SetResetCallback(&CAResetCallback, _CAInputDevice, NULL);
	Mic_SetSampleReadCallback(&CASampleReadCallback, this, NULL);
}

MacInputHandler::~MacInputHandler()
{
	delete this->_CAInputDevice;
}

CocoaDSController* MacInputHandler::GetCocoaController()
{
	return this->_cdsController;
}

void MacInputHandler::SetCocoaController(CocoaDSController *theController)
{
	this->_cdsController = theController;
	this->_CAInputDevice->SetCallbackHardwareStateChanged(&CAHardwareStateChangedCallback, theController, NULL);
	this->_CAInputDevice->SetCallbackHardwareGainChanged(&CAHardwareGainChangedCallback, theController, NULL);
}

void MacInputHandler::StartHardwareMicDevice()
{
	this->_CAInputDevice->Start();
}

bool MacInputHandler::IsHardwareMicAvailable()
{
	return ( this->_CAInputDevice->IsHardwareEnabled() && !this->_CAInputDevice->IsHardwareLocked() );
}

void MacInputHandler::ReportAverageMicLevel()
{
	[this->_cdsController setMicLevel:this->GetAverageMicLevel()];
}

void MacInputHandler::SetHardwareMicMute(bool muteState)
{
	const bool needSetMuteState = (this->_isHardwareMicMuted != muteState);
	
	if (needSetMuteState)
	{
		if (muteState)
		{
			this->_hardwareMicSampleGenerator->resetSamples();
			this->ClearAverageMicLevel();
		}
		
		this->_isHardwareMicMuted = muteState;
		this->_CAInputDevice->SetPauseState(this->_isHardwareMicPaused || muteState);
	}
}

bool MacInputHandler::GetHardwareMicPause()
{
	return this->_CAInputDevice->GetPauseState();
}

void MacInputHandler::SetHardwareMicPause(bool pauseState)
{
	const bool needSetPauseState = (this->_isHardwareMicPaused != pauseState);
	
	if (needSetPauseState)
	{
		this->_isHardwareMicPaused = pauseState;
		this->_CAInputDevice->SetPauseState(pauseState || this->_isHardwareMicMuted);
	}
}

float MacInputHandler::GetHardwareMicNormalizedGain()
{
	return this->_CAInputDevice->GetNormalizedGain();
}

void MacInputHandler::SetHardwareMicGainAsNormalized(float normalizedGain)
{
	this->_CAInputDevice->SetGainAsNormalized(normalizedGain);
}

void CAResetCallback(void *inParam1, void *inParam2)
{
	CoreAudioInput *caInputDevice = (CoreAudioInput *)inParam1;
	caInputDevice->Start();
}

uint8_t CASampleReadCallback(void *inParam1, void *inParam2)
{
	ClientInputHandler *inputHandler = (ClientInputHandler *)inParam1;
	return inputHandler->HandleMicSampleRead();
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
