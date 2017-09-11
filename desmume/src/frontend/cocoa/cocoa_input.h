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
	ClientExecutionControl *execControl;
	
	NSInteger stylusPressure;
	
	float micLevel;
	float _micLevelTotal;
	float _micLevelsRead;
	
	BOOL hardwareMicMute;
	size_t _availableMicSamples;
	
	CoreAudioInput *CAInputDevice;
	
	NSString *hardwareMicInfoString;
	NSString *hardwareMicNameString;
	NSString *hardwareMicManufacturerString;
	NSString *hardwareMicSampleRateString;
}

@property (retain) id <CocoaDSControllerDelegate> delegate;
@property (assign) ClientExecutionControl *execControl;
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
@property (readonly) CoreAudioInput *CAInputDevice;
@property (readonly) AudioGenerator *softwareMicSampleGenerator;
@property (assign) AudioSampleBlockGenerator *selectedAudioFileGenerator;
@property (retain) NSString *hardwareMicInfoString;
@property (retain) NSString *hardwareMicNameString;
@property (retain) NSString *hardwareMicManufacturerString;
@property (retain) NSString *hardwareMicSampleRateString;

- (void) setSoftwareMicState:(BOOL)theState mode:(NSInteger)micMode;
- (BOOL) softwareMicState;
- (void) setControllerState:(BOOL)theState controlID:(const NSUInteger)controlID;
- (void) setControllerState:(BOOL)theState controlID:(const NSUInteger)controlID turbo:(const BOOL)isTurboEnabled turboPattern:(uint32_t)turboPattern turboPatternLength:(uint32_t)turboPatternLength;
- (void) setTouchState:(BOOL)theState location:(const NSPoint)theLocation;
- (void) setSineWaveGeneratorFrequency:(const double)freq;
- (void) clearAutohold;
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
