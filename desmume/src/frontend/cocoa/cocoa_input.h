/*
	Copyright (C) 2011 Roger Manuel
	Copyright (C) 2012-2017 DeSmuME Team

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
#include <vector>

#include "ClientExecutionControl.h"

@class CocoaDSController;
class CoreAudioInput;
struct CoreAudioInputDeviceInfo;
class AudioGenerator;
class AudioSampleBlockGenerator;

typedef struct
{
	bool isPressed;
	bool turbo;
	bool autohold;
	uint32_t turboPattern;
	uint8_t turboPatternStep;
} ClientInput;

@protocol CocoaDSControllerDelegate <NSObject>

@optional
- (void) doMicLevelUpdateFromController:(CocoaDSController *)cdsController;
- (void) doMicHardwareStateChangedFromController:(CocoaDSController *)cdsController
									   isEnabled:(BOOL)isHardwareEnabled
										isLocked:(BOOL)isHardwareLocked;

- (void) doMicHardwareGainChangedFromController:(CocoaDSController *)cdsController gain:(float)gainValue;

@end

@interface CocoaDSController : NSObject
{
	id <CocoaDSControllerDelegate> delegate;
	
	ClientInput clientInput[NDSInputID_InputCount];
	BOOL autohold;
	BOOL _isAutoholdCleared;
	
	NSPoint touchLocation;
	NSInteger paddleAdjust;
	NSInteger stylusPressure;
	
	float micLevel;
	float _micLevelTotal;
	float _micLevelsRead;
	
	BOOL hardwareMicMute;
	BOOL _useHardwareMic;
	size_t _availableMicSamples;
	NSInteger micMode;
	
	AudioSampleBlockGenerator *selectedAudioFileGenerator;
	CoreAudioInput *CAInputDevice;
	AudioGenerator *softwareMicSampleGenerator;
	
	NSString *hardwareMicInfoString;
	NSString *hardwareMicNameString;
	NSString *hardwareMicManufacturerString;
	NSString *hardwareMicSampleRateString;
	
	OSSpinLock spinlockControllerState;
}

@property (retain) id <CocoaDSControllerDelegate> delegate;
@property (assign) BOOL autohold;
@property (assign) NSInteger paddleAdjust;
@property (assign) NSInteger stylusPressure;
@property (readonly) BOOL isHardwareMicAvailable;
@property (readonly) BOOL isHardwareMicIdle;
@property (readonly) BOOL isHardwareMicInClip;
@property (assign) float micLevel;
@property (assign) BOOL hardwareMicEnabled;
@property (readonly) BOOL hardwareMicLocked;
@property (assign) float hardwareMicGain;
@property (assign) BOOL hardwareMicMute;
@property (assign) BOOL hardwareMicPause;
@property (assign) BOOL softwareMicState;
@property (assign) NSInteger softwareMicMode;
@property (assign) NSInteger micMode;
@property (readonly) CoreAudioInput *CAInputDevice;
@property (readonly) AudioGenerator *softwareMicSampleGenerator;
@property (assign) AudioSampleBlockGenerator *selectedAudioFileGenerator;
@property (retain) NSString *hardwareMicInfoString;
@property (retain) NSString *hardwareMicNameString;
@property (retain) NSString *hardwareMicManufacturerString;
@property (retain) NSString *hardwareMicSampleRateString;

- (void) setControllerState:(BOOL)theState controlID:(const NSUInteger)controlID;
- (void) setControllerState:(BOOL)theState controlID:(const NSUInteger)controlID turbo:(const BOOL)isTurboEnabled turboPattern:(uint32_t)turboPattern;
- (void) setTouchState:(BOOL)theState location:(const NSPoint)theLocation;
- (void) setSineWaveGeneratorFrequency:(const double)freq;
- (void) clearAutohold;
- (void) flush;
- (void) flushEmpty;
- (void) reset;

- (void) clearMicLevelMeasure;
- (void) updateMicLevel;
- (uint8_t) handleMicSampleRead:(CoreAudioInput *)caInput softwareMic:(AudioGenerator *)sampleGenerator;
- (void) handleMicHardwareStateChanged:(CoreAudioInputDeviceInfo *)deviceInfo
							 isEnabled:(BOOL)isHardwareEnabled
							  isLocked:(BOOL)isHardwareLocked;
- (void) handleMicHardwareGainChanged:(float)gainValue;

@end

#ifdef __cplusplus
extern "C"
{
#endif

void CAResetCallback(void *inParam1, void *inParam2);
uint8_t CASampleReadCallback(void *inParam1, void *inParam2);
void CAHardwareStateChangedCallback(CoreAudioInputDeviceInfo *deviceInfo,
									const bool isHardwareEnabled,
									const bool isHardwareLocked,
									void *inParam1,
									void *inParam2);
void CAHardwareGainChangedCallback(float normalizedGain, void *inParam1, void *inParam2);

#ifdef __cplusplus
}
#endif
