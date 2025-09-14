/*
	Copyright (C) 2012-2025 DeSmuME team

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

#ifndef _COREAUDIOSOUND_
#define _COREAUDIOSOUND_

#include <CoreServices/CoreServices.h>
#include <AudioUnit/AudioUnit.h>
#include <AudioToolbox/AudioToolbox.h>

#include "utilities.h"
#include "ringbuffer.h"
#include "audiosamplegenerator.h"

struct CoreAudioInputDeviceInfo
{
	AudioObjectID objectID;
	CFStringRef name;
	CFStringRef manufacturer;
	CFStringRef deviceUID;
	CFStringRef modelUID;
	Float64 sampleRate;
};
typedef CoreAudioInputDeviceInfo CoreAudioInputDeviceInfo;

typedef void (*CoreAudioInputHardwareStateChangedCallback)(CoreAudioInputDeviceInfo *deviceInfo,
														   const bool isHardwareEnabled,
														   const bool isHardwareLocked,
														   const bool isHardwareAuthorized,
														   void *inParam1,
														   void *inParam2);

typedef void (*CoreAudioInputHardwareGainChangedCallback)(float normalizedGain, void *inParam1, void *inParam2);

class CoreAudioInput : public AudioGenerator
{
private:
	apple_unfairlock_t _unfairlockAUHAL;
	
	CoreAudioInputHardwareStateChangedCallback _hwStateChangedCallbackFunc;
	void *_hwStateChangedCallbackParam1;
	void *_hwStateChangedCallbackParam2;
	
	CoreAudioInputHardwareGainChangedCallback _hwGainChangedCallbackFunc;
	void *_hwGainChangedCallbackParam1;
	void *_hwGainChangedCallbackParam2;
	
	AUGraph _auGraph;
	AUNode _auFormatConverterNode;
	AUNode _auOutputNode;
	AudioUnit _auFormatConverterUnit;
	AudioUnit _auOutputUnit;
	AudioBufferList *_convertBufferList;
	UInt32 _captureFrames;
	
	RingBuffer *_samplesConverted;
	
	float _inputGainNormalized;
	AudioUnitElement _inputElement;
	
	bool _isPaused;
	CoreAudioInputDeviceInfo _hwDeviceInfo;
	bool _isHardwareEnabled;
	bool _isHardwareLocked;
	bool _isHardwareAuthorized;
	
	OSStatus InitInputAUHAL(UInt32 deviceID);
	
public:
	AudioUnit _auHALInputDevice;
	AudioTimeStamp _timeStamp;
	AudioBufferList *_captureBufferList;
	RingBuffer *_samplesCaptured;
	
	CoreAudioInput();
	virtual ~CoreAudioInput();
	
	void Start();
	void Stop();
	size_t Pull();
	
	bool IsHardwareEnabled() const;
	bool IsHardwareLocked() const;
	bool IsHardwareAuthorized() const;
	void SetHardwareAuthorized(bool isAuthorized);
	bool GetPauseState() const;
	void SetPauseState(bool pauseState);
	float GetNormalizedGain() const;
	void SetGainAsNormalized(float normalizedGain);
	
	void UpdateHardwareGain(float normalizedGain);
	void UpdateHardwareLock();
	void SetCallbackHardwareStateChanged(CoreAudioInputHardwareStateChangedCallback callbackFunc, void *inParam1, void *inParam2);
	void SetCallbackHardwareGainChanged(CoreAudioInputHardwareGainChangedCallback callbackFunc, void *inParam1, void *inParam2);
	
	virtual size_t resetSamples();
	virtual uint8_t generateSample();
};

OSStatus CoreAudioInputCaptureCallback(void *inRefCon,
									   AudioUnitRenderActionFlags *ioActionFlags,
									   const AudioTimeStamp *inTimeStamp,
									   UInt32 inBusNumber,
									   UInt32 inNumberFrames,
									   AudioBufferList *ioData);

OSStatus CoreAudioInputReceiveCallback(void *inRefCon,
									   AudioUnitRenderActionFlags *ioActionFlags,
									   const AudioTimeStamp *inTimeStamp,
									   UInt32 inBusNumber,
									   UInt32 inNumberFrames,
									   AudioBufferList *ioData);

OSStatus CoreAudioInputConvertCallback(void *inRefCon,
									   AudioUnitRenderActionFlags *ioActionFlags,
									   const AudioTimeStamp *inTimeStamp,
									   UInt32 inBusNumber,
									   UInt32 inNumberFrames,
									   AudioBufferList *ioData);

OSStatus CoreAudioInputDeviceChanged(AudioObjectID inObjectID,
									 UInt32 inNumberAddresses,
									 const AudioObjectPropertyAddress inAddresses[],
									 void *inClientData);

void CoreAudioInputAUHALChanged(void *inRefCon,
								AudioUnit inUnit,
								AudioUnitPropertyID inID,
								AudioUnitScope inScope,
								AudioUnitElement inElement);

void CoreAudioInputDefaultHardwareStateChangedCallback(CoreAudioInputDeviceInfo *deviceInfo,
													   const bool isHardwareEnabled,
													   const bool isHardwareLocked,
													   const bool isHardwareAuthorized,
													   void *inParam1,
													   void *inParam2);

void CoreAudioInputDefaultHardwareGainChangedCallback(float normalizedGain, void *inParam1, void *inParam2);

bool CreateAudioUnitInstance(AudioUnit *au, ComponentDescription *auDescription);
#if defined(MAC_OS_X_VERSION_10_6) && MAC_OS_X_VERSION_MAX_ALLOWED >= MAC_OS_X_VERSION_10_6
bool CreateAudioUnitInstance(AudioUnit *au, AudioComponentDescription *auDescription);
#endif
void DestroyAudioUnitInstance(AudioUnit *au);

#endif
