/*
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

#include "coreaudiosound.h"

#include <CoreAudio/CoreAudio.h>
#include "ClientInputHandler.h"
#include "utilities.h"

#import "cocoa_globals.h"

//#define FORCE_AUDIOCOMPONENT_10_5

CoreAudioInput::CoreAudioInput()
{
	_spinlockAUHAL = (OSSpinLock *)malloc(sizeof(OSSpinLock));
	*_spinlockAUHAL = OS_SPINLOCK_INIT;
	
	_hwStateChangedCallbackFunc = &CoreAudioInputDefaultHardwareStateChangedCallback;
	_hwStateChangedCallbackParam1 = NULL;
	_hwStateChangedCallbackParam2 = NULL;
	
	_hwGainChangedCallbackFunc = &CoreAudioInputDefaultHardwareGainChangedCallback;
	_hwGainChangedCallbackParam1 = NULL;
	_hwGainChangedCallbackParam2 = NULL;
	
	_inputGainNormalized = 0.0f;
	_inputElement = 0;
	
	_hwDeviceInfo.objectID = kAudioObjectUnknown;
	_hwDeviceInfo.name = CFSTR("Unknown Device");
	_hwDeviceInfo.manufacturer = CFSTR("Unknown Manufacturer");
	_hwDeviceInfo.deviceUID = CFSTR("");
	_hwDeviceInfo.modelUID = CFSTR("");
	_hwDeviceInfo.sampleRate = 0.0;
	_isPaused = true;
	_isHardwareEnabled = false;
	_isHardwareLocked = true;
	_captureFrames = 0;
	
	_auHALInputDevice = NULL;
	_auGraph = NULL;
	_auFormatConverterUnit = NULL;
	_auOutputUnit = NULL;
	_auFormatConverterNode = 0;
	_auOutputNode = 0;
	memset(&_timeStamp, 0, sizeof(AudioTimeStamp));
	
#if !defined(FORCE_AUDIOCOMPONENT_10_5) && defined(MAC_OS_X_VERSION_10_6) && MAC_OS_X_VERSION_MAX_ALLOWED >= MAC_OS_X_VERSION_10_6
	AudioComponentDescription halInputDeviceDesc;
	AudioComponentDescription formatConverterDesc;
	AudioComponentDescription outputDesc;
#else
	ComponentDescription halInputDeviceDesc;
	ComponentDescription formatConverterDesc;
	ComponentDescription outputDesc;
#endif
	halInputDeviceDesc.componentType = kAudioUnitType_Output;
	halInputDeviceDesc.componentSubType = kAudioUnitSubType_HALOutput;
	halInputDeviceDesc.componentManufacturer = kAudioUnitManufacturer_Apple;
	halInputDeviceDesc.componentFlags = 0;
	halInputDeviceDesc.componentFlagsMask = 0;
	
	formatConverterDesc.componentType = kAudioUnitType_FormatConverter;
	formatConverterDesc.componentSubType = kAudioUnitSubType_AUConverter;
	formatConverterDesc.componentManufacturer = kAudioUnitManufacturer_Apple;
	formatConverterDesc.componentFlags = 0;
	formatConverterDesc.componentFlagsMask = 0;
	
	outputDesc.componentType = kAudioUnitType_Output;
	outputDesc.componentSubType = kAudioUnitSubType_GenericOutput;
	outputDesc.componentManufacturer = kAudioUnitManufacturer_Apple;
	outputDesc.componentFlags = 0;
	outputDesc.componentFlagsMask = 0;
	
	CreateAudioUnitInstance(&_auHALInputDevice, &halInputDeviceDesc);
	
	NewAUGraph(&_auGraph);
	AUGraphOpen(_auGraph);
#if defined(MAC_OS_X_VERSION_10_6) && MAC_OS_X_VERSION_MAX_ALLOWED >= MAC_OS_X_VERSION_10_6
	AUGraphAddNode(_auGraph, (AudioComponentDescription *)&formatConverterDesc, &_auFormatConverterNode);
	AUGraphAddNode(_auGraph, (AudioComponentDescription *)&outputDesc, &_auOutputNode);
#else
	AUGraphAddNode(_auGraph, (ComponentDescription *)&formatConverterDesc, &_auFormatConverterNode);
	AUGraphAddNode(_auGraph, (ComponentDescription *)&outputDesc, &_auOutputNode);
#endif
	AUGraphConnectNodeInput(_auGraph, _auFormatConverterNode, 0, _auOutputNode, 0);
	
	AUGraphNodeInfo(_auGraph, _auFormatConverterNode, NULL, &_auFormatConverterUnit);
	AUGraphNodeInfo(_auGraph, _auOutputNode, NULL, &_auOutputUnit);
	
	static const UInt32 disableFlag = 0;
	static const UInt32 enableFlag = 1;
	static const AudioUnitScope inputBus = 1;
	static const AudioUnitScope outputBus = 0;
	UInt32 propertySize = 0;
	
	AudioUnitSetProperty(_auHALInputDevice,
						 kAudioOutputUnitProperty_EnableIO,
						 kAudioUnitScope_Input,
						 inputBus,
						 &enableFlag,
						 sizeof(enableFlag) );
	
	AudioUnitSetProperty(_auHALInputDevice,
						 kAudioOutputUnitProperty_EnableIO,
						 kAudioUnitScope_Output,
						 outputBus,
						 &disableFlag,
						 sizeof(disableFlag) );
	
	AudioStreamBasicDescription outputFormat;
	propertySize = sizeof(AudioStreamBasicDescription);
	outputFormat.mSampleRate = MIC_SAMPLE_RATE;
	outputFormat.mFormatID =  kAudioFormatLinearPCM;
	outputFormat.mFormatFlags = kAudioFormatFlagIsPacked;
	outputFormat.mBytesPerPacket = MIC_SAMPLE_SIZE;
	outputFormat.mFramesPerPacket = 1;
	outputFormat.mBytesPerFrame = MIC_SAMPLE_SIZE;
	outputFormat.mChannelsPerFrame = MIC_NUMBER_CHANNELS;
	outputFormat.mBitsPerChannel = MIC_SAMPLE_RESOLUTION;
	
	AudioStreamBasicDescription deviceOutputFormat;
	AudioUnitGetProperty(_auOutputUnit,
						 kAudioUnitProperty_StreamFormat,
						 kAudioUnitScope_Output,
						 0,
						 &deviceOutputFormat,
						 &propertySize);
	
	AudioUnitSetProperty(_auFormatConverterUnit,
						 kAudioUnitProperty_StreamFormat,
						 kAudioUnitScope_Input,
						 0,
						 &outputFormat,
						 propertySize);
	
	AudioUnitSetProperty(_auFormatConverterUnit,
						 kAudioUnitProperty_StreamFormat,
						 kAudioUnitScope_Output,
						 0,
						 &outputFormat,
						 propertySize);
	
	AudioUnitSetProperty(_auOutputUnit,
						 kAudioUnitProperty_StreamFormat,
						 kAudioUnitScope_Input,
						 0,
						 &outputFormat,
						 propertySize);
	
	AudioUnitSetProperty(_auOutputUnit,
						 kAudioUnitProperty_StreamFormat,
						 kAudioUnitScope_Output,
						 0,
						 &outputFormat,
						 propertySize);
	
	static const UInt32 bestQuality = kAudioUnitSampleRateConverterComplexity_Mastering;
	AudioUnitSetProperty(_auFormatConverterUnit,
						 kAudioUnitProperty_SampleRateConverterComplexity,
						 kAudioUnitScope_Global,
						 0,
						 &bestQuality,
						 sizeof(bestQuality));
	
	// Set up the capture buffers.
	const size_t audioBufferListSize = offsetof(AudioBufferList, mBuffers[0]) + sizeof(AudioBuffer);
	
	_captureBufferList = (AudioBufferList *)malloc(audioBufferListSize);
	_captureBufferList->mNumberBuffers = 0;
	_captureBufferList->mBuffers[0].mNumberChannels = 0;
	_captureBufferList->mBuffers[0].mDataByteSize = 0;
	_captureBufferList->mBuffers[0].mData = NULL;
	
	_convertBufferList = (AudioBufferList *)malloc(audioBufferListSize);
	_convertBufferList->mNumberBuffers = 1;
	_convertBufferList->mBuffers[0].mNumberChannels = 1;
	_convertBufferList->mBuffers[0].mDataByteSize = MIC_CAPTURE_FRAMES * sizeof(uint8_t);
	_convertBufferList->mBuffers[0].mData = malloc(_convertBufferList->mBuffers[0].mDataByteSize);
	
	_samplesCaptured = new RingBuffer(MIC_CAPTURE_FRAMES * 3, sizeof(uint8_t));
	_samplesConverted = new RingBuffer(MIC_CAPTURE_FRAMES * 3, sizeof(uint8_t));
	
	// Set the AUHAL callback.
	AURenderCallbackStruct inputCaptureCallback;
	inputCaptureCallback.inputProc = &CoreAudioInputCaptureCallback;
	inputCaptureCallback.inputProcRefCon = this;
	
	AudioUnitSetProperty(_auHALInputDevice,
						 kAudioOutputUnitProperty_SetInputCallback,
						 kAudioUnitScope_Global,
						 0,
						 &inputCaptureCallback,
						 sizeof(inputCaptureCallback) );
	
	AudioUnitAddPropertyListener(this->_auHALInputDevice,
								 kAudioDevicePropertyVolumeScalar,
								 &CoreAudioInputAUHALChanged,
								 this);
	
	AudioUnitAddPropertyListener(this->_auHALInputDevice,
								 kAudioHardwarePropertyDefaultInputDevice,
								 &CoreAudioInputAUHALChanged,
								 this);
	
	AudioUnitAddPropertyListener(this->_auHALInputDevice,
								 kAudioDevicePropertyHogMode,
								 &CoreAudioInputAUHALChanged,
								 this);
	
	AudioUnitAddPropertyListener(this->_auHALInputDevice,
								 kAudioDevicePropertyJackIsConnected,
								 &CoreAudioInputAUHALChanged,
								 this);
	
	AudioObjectPropertyAddress defaultDeviceProperty;
	defaultDeviceProperty.mSelector = kAudioHardwarePropertyDefaultInputDevice;
	defaultDeviceProperty.mScope = kAudioObjectPropertyScopeGlobal;
	defaultDeviceProperty.mElement = kAudioObjectPropertyElementMaster;
	
	AudioObjectAddPropertyListener(kAudioObjectSystemObject,
										   &defaultDeviceProperty,
										   &CoreAudioInputDeviceChanged,
										   this);
	
	// Set up the AUGraph callbacks
	AURenderCallbackStruct inputReceiveCallback;
	inputReceiveCallback.inputProc = &CoreAudioInputReceiveCallback;
	inputReceiveCallback.inputProcRefCon = this->_samplesCaptured;
	
	AudioUnitSetProperty(_auFormatConverterUnit,
						 kAudioUnitProperty_SetRenderCallback,
						 kAudioUnitScope_Global,
						 0,
						 &inputReceiveCallback,
						 sizeof(inputReceiveCallback) );
	
	AUGraphAddRenderNotify(_auGraph, &CoreAudioInputConvertCallback, this->_samplesConverted);
}

CoreAudioInput::~CoreAudioInput()
{
	OSSpinLockLock(_spinlockAUHAL);
	DestroyAudioUnitInstance(&_auHALInputDevice);
	OSSpinLockUnlock(_spinlockAUHAL);
	
	AUGraphClose(_auGraph);
	AUGraphUninitialize(_auGraph);
	AUGraphRemoveNode(_auGraph, _auFormatConverterNode);
	AUGraphRemoveNode(_auGraph, _auOutputNode);
	DisposeAUGraph(_auGraph);
	
	free(_captureBufferList->mBuffers[0].mData);
	_captureBufferList->mBuffers[0].mData = NULL;
	free(_captureBufferList);
	_captureBufferList = NULL;
	
	free(_convertBufferList->mBuffers[0].mData);
	_convertBufferList->mBuffers[0].mData = NULL;
	free(_convertBufferList);
	_convertBufferList = NULL;
	
	delete _samplesCaptured;
	_samplesCaptured = NULL;
	
	delete _samplesConverted;
	_samplesConverted = NULL;
	
	free(_spinlockAUHAL);
	_spinlockAUHAL = NULL;
	
	CFRelease(this->_hwDeviceInfo.name);
	CFRelease(this->_hwDeviceInfo.manufacturer);
	CFRelease(this->_hwDeviceInfo.deviceUID);
	CFRelease(this->_hwDeviceInfo.modelUID);
}

OSStatus CoreAudioInput::InitInputAUHAL(UInt32 deviceID)
{
	OSStatus error = noErr;
	static const AudioUnitScope inputBus = 1;
	UInt32 propertySize = 0;
	
	this->_hwDeviceInfo.objectID = kAudioObjectUnknown;
	if (deviceID == kAudioObjectUnknown)
	{
		error = kAudioHardwareUnspecifiedError;
		return error;
	}
	
	// Get some information about the device before attempting to attach it to the AUHAL.
	AudioObjectPropertyAddress deviceProperty;
	UInt32 dataSize = 0;
	deviceProperty.mScope = kAudioObjectPropertyScopeGlobal;
	deviceProperty.mElement = kAudioObjectPropertyElementMaster;
	
	deviceProperty.mSelector = kAudioObjectPropertyName;
	error = AudioObjectGetPropertyDataSize(deviceID, &deviceProperty, 0, NULL, &dataSize);
	if (error == noErr)
	{
		CFRelease(this->_hwDeviceInfo.name);
		AudioObjectGetPropertyData(deviceID, &deviceProperty, 0, NULL, &dataSize, &this->_hwDeviceInfo.name);
	}
	
	deviceProperty.mSelector = kAudioObjectPropertyManufacturer;
	error = AudioObjectGetPropertyDataSize(deviceID, &deviceProperty, 0, NULL, &dataSize);
	if (error == noErr)
	{
		CFRelease(this->_hwDeviceInfo.manufacturer);
		AudioObjectGetPropertyData(deviceID, &deviceProperty, 0, NULL, &dataSize, &this->_hwDeviceInfo.manufacturer);
	}
	
	deviceProperty.mSelector = kAudioDevicePropertyDeviceUID;
	error = AudioObjectGetPropertyDataSize(deviceID, &deviceProperty, 0, NULL, &dataSize);
	if (error == noErr)
	{
		CFRelease(this->_hwDeviceInfo.deviceUID);
		AudioObjectGetPropertyData(deviceID, &deviceProperty, 0, NULL, &dataSize, &this->_hwDeviceInfo.deviceUID);
	}
	
	deviceProperty.mSelector = kAudioDevicePropertyModelUID;
	error = AudioObjectGetPropertyDataSize(deviceID, &deviceProperty, 0, NULL, &dataSize);
	if (error == noErr)
	{
		CFRelease(this->_hwDeviceInfo.modelUID);
		AudioObjectGetPropertyData(deviceID, &deviceProperty, 0, NULL, &dataSize, &this->_hwDeviceInfo.modelUID);
	}
	
	deviceProperty.mSelector = kAudioDevicePropertyNominalSampleRate;
	deviceProperty.mScope = kAudioDevicePropertyScopeInput;
	error = AudioObjectGetPropertyDataSize(deviceID, &deviceProperty, 0, NULL, &dataSize);
	if (error == noErr)
	{
		AudioObjectGetPropertyData(deviceID, &deviceProperty, 0, NULL, &dataSize, &this->_hwDeviceInfo.sampleRate);
	}
	
	// Before attaching the HAL input device, stop everything first.
	AUGraphStop(this->_auGraph);
	AUGraphUninitialize(this->_auGraph);
	AudioOutputUnitStop(this->_auHALInputDevice);
	AudioUnitUninitialize(this->_auHALInputDevice);
	
	// Attach the device to the AUHAL.
	//
	// From here on out, any error related to the AUHAL will be treated
	// as a failure to initialize and cause this method to exit.
	error = AudioUnitSetProperty(this->_auHALInputDevice,
								 kAudioOutputUnitProperty_CurrentDevice,
								 kAudioUnitScope_Global,
								 0,
								 &deviceID,
								 sizeof(deviceID) );
	if (error != noErr)
	{
		return error;
	}
	
	// Get the AUHAL's audio format and set that as on the input scope
	// of the format converter unit.
	AudioStreamBasicDescription inputFormat;
	propertySize = sizeof(AudioStreamBasicDescription);
	error = AudioUnitGetProperty(this->_auHALInputDevice,
								 kAudioUnitProperty_StreamFormat,
								 kAudioUnitScope_Input,
								 inputBus,
								 &inputFormat,
								 &propertySize);
	if (error != noErr)
	{
		return error;
	}
	
	// Interleaved audio is the only real requirement for the audio input.
	// All the other ASBD fields can be passed as-is.
	inputFormat.mFormatFlags &= ~kAudioFormatFlagIsNonInterleaved;
	
	error = AudioUnitSetProperty(this->_auHALInputDevice,
								 kAudioUnitProperty_StreamFormat,
								 kAudioUnitScope_Output,
								 inputBus,
								 &inputFormat,
								 propertySize);
	if (error != noErr)
	{
		return error;
	}
	
	error = AudioUnitSetProperty(this->_auFormatConverterUnit,
								 kAudioUnitProperty_StreamFormat,
								 kAudioUnitScope_Input,
								 0,
								 &inputFormat,
								 propertySize);
	if (error != noErr)
	{
		return error;
	}
	
	// Set up the capture buffers.
	AudioValueRange bufferRange;
	propertySize = sizeof(AudioValueRange);
	error = AudioUnitGetProperty(this->_auHALInputDevice,
								 kAudioDevicePropertyBufferFrameSizeRange,
								 kAudioUnitScope_Input,
								 inputBus,
								 &bufferRange,
								 &propertySize);
	if (error != noErr)
	{
		return error;
	}
	
	const UInt32 captureFramesScaled = (MIC_CAPTURE_FRAMES * ((double)inputFormat.mSampleRate / (double)MIC_SAMPLE_RATE)) + 0.5;
	this->_captureFrames = (captureFramesScaled < bufferRange.mMinimum) ? bufferRange.mMinimum : ((captureFramesScaled > bufferRange.mMaximum) ? bufferRange.mMaximum : captureFramesScaled);
	
	propertySize = sizeof(UInt32);
	error = AudioUnitSetProperty(this->_auHALInputDevice,
								 kAudioDevicePropertyBufferFrameSize,
								 kAudioUnitScope_Input,
								 inputBus,
								 &this->_captureFrames,
								 propertySize);
	if (error != noErr)
	{
		return error;
	}
	
	free(this->_captureBufferList->mBuffers[0].mData);
	this->_samplesCaptured->resize(this->_captureFrames * 3, inputFormat.mBytesPerFrame);
	this->_captureBufferList->mNumberBuffers = 1;
	this->_captureBufferList->mBuffers[0].mNumberChannels = inputFormat.mChannelsPerFrame;
	this->_captureBufferList->mBuffers[0].mDataByteSize = this->_captureFrames * inputFormat.mBytesPerFrame;
	this->_captureBufferList->mBuffers[0].mData = malloc(this->_captureBufferList->mBuffers[0].mDataByteSize);
	
	// Now that the AUHAL is set up, attempt to initialize the AUHAL.
	error = AudioUnitInitialize(this->_auHALInputDevice);
	if (error != noErr)
	{
		return error;
	}
	
	this->_hwDeviceInfo.objectID = deviceID;
	
	return error;
}

void CoreAudioInput::Start()
{
	OSStatus error = noErr;
	char errorString[5] = {0};
	
	// Get the default input device ID.
	AudioObjectID defaultInputDeviceID = kAudioObjectUnknown;
	UInt32 propertySize = sizeof(defaultInputDeviceID);
	AudioObjectPropertyAddress defaultDeviceProperty;
	defaultDeviceProperty.mSelector = kAudioHardwarePropertyDefaultInputDevice;
	defaultDeviceProperty.mScope = kAudioObjectPropertyScopeGlobal;
	defaultDeviceProperty.mElement = kAudioObjectPropertyElementMaster;
	
	AudioObjectGetPropertyData(kAudioObjectSystemObject,
							   &defaultDeviceProperty,
							   0,
							   NULL,
							   &propertySize,
							   &defaultInputDeviceID);
	
	// Set the default input device to the audio unit.
	OSSpinLockLock(this->_spinlockAUHAL);
	
	error = this->InitInputAUHAL(defaultInputDeviceID);
	if (error == noErr)
	{
		Float32 theGain = 0.0f;
		UInt32 gainPropSize = sizeof(theGain);
		
		// Try and get the gain properties on the AUHAL.
		for (AudioUnitElement elementNumber = 0; elementNumber < 3; elementNumber++)
		{
			error = AudioUnitGetProperty(this->_auHALInputDevice,
										 kAudioDevicePropertyVolumeScalar,
										 kAudioUnitScope_Input,
										 elementNumber,
										 &theGain,
										 &gainPropSize);
			
			if (error == noErr)
			{
				this->_inputElement = elementNumber;
				this->UpdateHardwareGain(theGain);
				break;
			}
		}
		
		this->_isHardwareEnabled = true;
		this->UpdateHardwareLock();
	}
	else
	{
		*(OSStatus *)errorString = CFSwapInt32BigToHost(error);
		printf("CoreAudioInput - AUHAL init error: %s\n", errorString);
		this->_isHardwareEnabled = false;
	}
	
	if (this->IsHardwareEnabled() && !this->IsHardwareLocked() && !this->GetPauseState())
	{
		AudioOutputUnitStart(this->_auHALInputDevice);
	}
	
	OSSpinLockUnlock(this->_spinlockAUHAL);
	
	AUGraphInitialize(_auGraph);
	if (!this->GetPauseState())
	{
		AUGraphStart(this->_auGraph);
	}
	
	this->_samplesCaptured->clear();
	this->_samplesConverted->clear();
	this->_hwStateChangedCallbackFunc(&this->_hwDeviceInfo,
									  this->IsHardwareEnabled(),
									  this->IsHardwareLocked(),
									  this->_hwStateChangedCallbackParam1,
									  this->_hwStateChangedCallbackParam2);
}

void CoreAudioInput::Stop()
{
	OSSpinLockLock(this->_spinlockAUHAL);
	AudioOutputUnitStop(this->_auHALInputDevice);
	AudioUnitUninitialize(this->_auHALInputDevice);
	OSSpinLockUnlock(this->_spinlockAUHAL);
	
	AUGraphStop(this->_auGraph);
	AUGraphUninitialize(this->_auGraph);
	
	this->_isHardwareEnabled = false;
	this->_samplesCaptured->clear();
	this->_samplesConverted->clear();
}

size_t CoreAudioInput::Pull()
{
	AudioUnitRenderActionFlags ioActionFlags = 0;
	
	AudioUnitRender(this->_auOutputUnit,
					&ioActionFlags,
					&this->_timeStamp,
					0,
					MIC_CAPTURE_FRAMES,
					this->_convertBufferList);
	
	return MIC_CAPTURE_FRAMES;
}

bool CoreAudioInput::IsHardwareEnabled() const
{
	return this->_isHardwareEnabled;
}

bool CoreAudioInput::IsHardwareLocked() const
{
	return this->_isHardwareLocked;
}

bool CoreAudioInput::GetPauseState() const
{
	return this->_isPaused;
}

void CoreAudioInput::SetPauseState(bool pauseState)
{
	if (pauseState && !this->GetPauseState())
	{
		OSSpinLockLock(this->_spinlockAUHAL);
		AudioOutputUnitStop(this->_auHALInputDevice);
		OSSpinLockUnlock(this->_spinlockAUHAL);
		AUGraphStop(this->_auGraph);
	}
	else if (!pauseState && this->GetPauseState() && !this->IsHardwareLocked())
	{
		OSSpinLockLock(this->_spinlockAUHAL);
		AudioOutputUnitStart(this->_auHALInputDevice);
		OSSpinLockUnlock(this->_spinlockAUHAL);
		AUGraphStart(this->_auGraph);
	}
	
	this->_isPaused = (this->IsHardwareLocked()) ? true : pauseState;
}

float CoreAudioInput::GetNormalizedGain() const
{
	return this->_inputGainNormalized;
}

void CoreAudioInput::SetGainAsNormalized(float normalizedGain)
{
	Float32 gainValue = normalizedGain;
	UInt32 gainPropSize = sizeof(gainValue);
	
	OSSpinLockLock(this->_spinlockAUHAL);
	AudioUnitSetProperty(this->_auHALInputDevice,
						 kAudioDevicePropertyVolumeScalar,
						 kAudioUnitScope_Input,
						 this->_inputElement,
						 &gainValue,
						 gainPropSize);
	OSSpinLockUnlock(this->_spinlockAUHAL);
}

void CoreAudioInput::UpdateHardwareGain(float normalizedGain)
{
	this->_inputGainNormalized = normalizedGain;
	this->_hwGainChangedCallbackFunc(this->_inputGainNormalized, this->_hwGainChangedCallbackParam1, this->_hwGainChangedCallbackParam2);
}

void CoreAudioInput::UpdateHardwareLock()
{
	OSStatus error = noErr;
	bool hardwareLocked = false;
	
	if (this->IsHardwareEnabled())
	{
		// Check if another application has exclusive access to the hardware.
		pid_t hogMode = 0;
		UInt32 propertySize = sizeof(hogMode);
		error = AudioUnitGetProperty(this->_auHALInputDevice,
									 kAudioDevicePropertyHogMode,
									 kAudioUnitScope_Input,
									 1,
									 &hogMode,
									 &propertySize);
		if (error == noErr)
		{
			if (hogMode != -1)
			{
				hardwareLocked = true;
			}
		}
		else
		{
			// If this property is not supported, then always assume that
			// the hardware device is shared.
			hogMode = -1;
		}
		
		// Check if the hardware device is plugged in.
		UInt32 isJackConnected = 0;
		propertySize = sizeof(isJackConnected);
		error = AudioUnitGetProperty(this->_auHALInputDevice,
									 kAudioDevicePropertyJackIsConnected,
									 kAudioUnitScope_Input,
									 1,
									 &hogMode,
									 &propertySize);
		if (error == noErr)
		{
			// If the kAudioDevicePropertyJackIsConnected property is supported,
			// then lock the hardware if the jack isn't connected.
			//
			// If the kAudioDevicePropertyJackIsConnected property is not supported,
			// then always assume that the hardware device is always plugged in.
			if (isJackConnected == 0)
			{
				hardwareLocked = true;
			}
		}
	}
	else
	{
		hardwareLocked = true;
	}
	
	this->_isHardwareLocked = hardwareLocked;
	if (this->_isHardwareLocked && !this->GetPauseState())
	{
		this->SetPauseState(true);
	}
	
	this->_hwStateChangedCallbackFunc(&this->_hwDeviceInfo,
									  this->IsHardwareEnabled(),
									  this->IsHardwareLocked(),
									  this->_hwStateChangedCallbackParam1,
									  this->_hwStateChangedCallbackParam2);
}

size_t CoreAudioInput::resetSamples()
{
	size_t samplesToDrop = this->_samplesConverted->getUsedElements();
	this->_samplesConverted->clear();
	
	return samplesToDrop;
}

uint8_t CoreAudioInput::generateSample()
{
	uint8_t theSample = MIC_NULL_SAMPLE_VALUE;
	
	if (this->_isPaused)
	{
		return theSample;
	}
	else
	{
		if (this->_samplesConverted->getUsedElements() == 0)
		{
			this->Pull();
		}
		
		this->_samplesConverted->read(&theSample, 1);
		theSample >>= 1; // Samples from CoreAudio are 8-bit, so we need to convert the sample to 7-bit
	}
	
	return theSample;
}

void CoreAudioInput::SetCallbackHardwareStateChanged(CoreAudioInputHardwareStateChangedCallback callbackFunc, void *inParam1, void *inParam2)
{
	this->_hwStateChangedCallbackFunc = callbackFunc;
	this->_hwStateChangedCallbackParam1 = inParam1;
	this->_hwStateChangedCallbackParam2 = inParam2;
}

void CoreAudioInput::SetCallbackHardwareGainChanged(CoreAudioInputHardwareGainChangedCallback callbackFunc, void *inParam1, void *inParam2)
{
	this->_hwGainChangedCallbackFunc = callbackFunc;
	this->_hwGainChangedCallbackParam1 = inParam1;
	this->_hwGainChangedCallbackParam2 = inParam2;
}

OSStatus CoreAudioInputCaptureCallback(void *inRefCon,
									   AudioUnitRenderActionFlags *ioActionFlags,
									   const AudioTimeStamp *inTimeStamp,
									   UInt32 inBusNumber,
									   UInt32 inNumberFrames,
									   AudioBufferList *ioData)
{
	OSStatus error = noErr;
	CoreAudioInput *caInput = (CoreAudioInput *)inRefCon;
	caInput->_timeStamp = *inTimeStamp;
	
	error = AudioUnitRender(caInput->_auHALInputDevice,
							ioActionFlags,
							inTimeStamp,
							inBusNumber,
							inNumberFrames,
							caInput->_captureBufferList);
	
	if (error != noErr)
	{
		return error;
	}
	
	caInput->_samplesCaptured->write(caInput->_captureBufferList->mBuffers[0].mData, inNumberFrames);
	
	return error;
}

OSStatus CoreAudioInputReceiveCallback(void *inRefCon,
									   AudioUnitRenderActionFlags *ioActionFlags,
									   const AudioTimeStamp *inTimeStamp,
									   UInt32 inBusNumber,
									   UInt32 inNumberFrames,
									   AudioBufferList *ioData)
{
	OSStatus error = noErr;
	RingBuffer *__restrict__ samplesCaptured = (RingBuffer *)inRefCon;
	uint8_t *__restrict__ receiveBuffer = (uint8_t *)ioData->mBuffers[0].mData;
	const size_t framesRead = samplesCaptured->read(receiveBuffer, inNumberFrames);
	
	// Pad any remaining samples.
	if (framesRead < inNumberFrames)
	{
		const size_t frameSize = samplesCaptured->getElementSize();
		memset(receiveBuffer + (framesRead * frameSize), MIC_NULL_SAMPLE_VALUE, (inNumberFrames - framesRead) * frameSize);
	}
	
	return error;
}

OSStatus CoreAudioInputConvertCallback(void *inRefCon,
									   AudioUnitRenderActionFlags *ioActionFlags,
									   const AudioTimeStamp *inTimeStamp,
									   UInt32 inBusNumber,
									   UInt32 inNumberFrames,
									   AudioBufferList *ioData)
{
	OSStatus error = noErr;
	
	if (*ioActionFlags & kAudioUnitRenderAction_PostRender)
	{
		RingBuffer *samplesConverted = (RingBuffer *)inRefCon;
		samplesConverted->write(ioData->mBuffers[0].mData, inNumberFrames);
	}
	
	return error;
}

OSStatus CoreAudioInputDeviceChanged(AudioObjectID inObjectID,
									 UInt32 inNumberAddresses,
									 const AudioObjectPropertyAddress inAddresses[],
									 void *inClientData)
{
	OSStatus error = noErr;
	CoreAudioInput *caInput = (CoreAudioInput *)inClientData;
	
	caInput->Start();
	if (!caInput->IsHardwareEnabled())
	{
		error = kAudioHardwareNotRunningError;
	}
	
	return error;
}

void CoreAudioInputAUHALChanged(void *inRefCon,
								AudioUnit inUnit,
								AudioUnitPropertyID inID,
								AudioUnitScope inScope,
								AudioUnitElement inElement)
{
	OSStatus error = noErr;
	CoreAudioInput *caInput = (CoreAudioInput *)inRefCon;
	
	switch (inID)
	{
		case kAudioDevicePropertyVolumeScalar:
		{
			Float32 gainValue = 0.0f;
			UInt32 propertySize = sizeof(gainValue);
			error = AudioUnitGetProperty(inUnit,
										 inID,
										 kAudioUnitScope_Input,
										 inElement,
										 &gainValue,
										 &propertySize);
			if (error == noErr)
			{
				caInput->UpdateHardwareGain(gainValue);
			}
			break;
		}
		
		case kAudioDevicePropertyHogMode:
		case kAudioDevicePropertyJackIsConnected:
			caInput->UpdateHardwareLock();
			break;
		
		default:
			break;
	}
}

void CoreAudioInputDefaultHardwareStateChangedCallback(CoreAudioInputDeviceInfo *deviceInfo,
													   const bool isHardwareEnabled,
													   const bool isHardwareLocked,
													   void *inParam1,
													   void *inParam2)
{
	// Do nothing.
}

void CoreAudioInputDefaultHardwareGainChangedCallback(float normalizedGain, void *inParam1, void *inParam2)
{
	// Do nothing.
}

#pragma mark -

CoreAudioOutput::CoreAudioOutput(size_t bufferSamples, size_t sampleSize)
{
	OSStatus error = noErr;
	
	_spinlockAU = (OSSpinLock *)malloc(sizeof(OSSpinLock));
	*_spinlockAU = OS_SPINLOCK_INIT;
	
	_buffer = new RingBuffer(bufferSamples, sampleSize);
	_volume = 1.0f;
	
	// Create a new audio unit
#if !defined(FORCE_AUDIOCOMPONENT_10_5) && defined(MAC_OS_X_VERSION_10_6) && MAC_OS_X_VERSION_MAX_ALLOWED >= MAC_OS_X_VERSION_10_6
	if (IsOSXVersionSupported(10, 6, 0))
	{
		AudioComponentDescription audioDesc;
		audioDesc.componentType = kAudioUnitType_Output;
		audioDesc.componentSubType = kAudioUnitSubType_DefaultOutput;
		audioDesc.componentManufacturer = kAudioUnitManufacturer_Apple;
		audioDesc.componentFlags = 0;
		audioDesc.componentFlagsMask = 0;
		
		CreateAudioUnitInstance(&_au, &audioDesc);
	}
	else
#endif
	{
		ComponentDescription audioDesc;
		audioDesc.componentType = kAudioUnitType_Output;
		audioDesc.componentSubType = kAudioUnitSubType_DefaultOutput;
		audioDesc.componentManufacturer = kAudioUnitManufacturer_Apple;
		audioDesc.componentFlags = 0;
		audioDesc.componentFlagsMask = 0;
		
		CreateAudioUnitInstance(&_au, &audioDesc);
	}
	
	// Set the render callback
	AURenderCallbackStruct callback;
	callback.inputProc = &CoreAudioOutputRenderCallback;
	callback.inputProcRefCon = _buffer;
	
	error = AudioUnitSetProperty(_au,
								 kAudioUnitProperty_SetRenderCallback,
								 kAudioUnitScope_Input,
								 0,
								 &callback,
								 sizeof(callback) );
	
	if(error != noErr)
	{
		return;
	}
	
	// Set up the audio unit for audio streaming
	AudioStreamBasicDescription outputFormat;
	outputFormat.mSampleRate = SPU_SAMPLE_RATE;
	outputFormat.mFormatID = kAudioFormatLinearPCM;
	outputFormat.mFormatFlags = kAudioFormatFlagIsSignedInteger | kAudioFormatFlagsNativeEndian | kLinearPCMFormatFlagIsPacked;
	outputFormat.mBytesPerPacket = SPU_SAMPLE_SIZE;
	outputFormat.mFramesPerPacket = 1;
	outputFormat.mBytesPerFrame = SPU_SAMPLE_SIZE;
	outputFormat.mChannelsPerFrame = SPU_NUMBER_CHANNELS;
	outputFormat.mBitsPerChannel = SPU_SAMPLE_RESOLUTION;
	
	error = AudioUnitSetProperty(_au,
								 kAudioUnitProperty_StreamFormat,
								 kAudioUnitScope_Input,
								 0,
								 &outputFormat,
								 sizeof(outputFormat) );
	
	if(error != noErr)
	{
		return;
	}
	
	// Initialize our new audio unit
	error = AudioUnitInitialize(_au);
	if(error != noErr)
	{
		return;
	}
}

CoreAudioOutput::~CoreAudioOutput()
{
	OSSpinLockLock(_spinlockAU);
	DestroyAudioUnitInstance(&_au);
	OSSpinLockUnlock(_spinlockAU);
	
	delete _buffer;
	_buffer = NULL;
	
	free(_spinlockAU);
	_spinlockAU = NULL;
}

void CoreAudioOutput::start()
{
	this->clearBuffer();
	
	OSSpinLockLock(this->_spinlockAU);
	AudioUnitReset(this->_au, kAudioUnitScope_Global, 0);
	AudioOutputUnitStart(this->_au);
	OSSpinLockUnlock(this->_spinlockAU);
}

void CoreAudioOutput::pause()
{
	OSSpinLockLock(this->_spinlockAU);
	AudioOutputUnitStop(this->_au);
	OSSpinLockUnlock(this->_spinlockAU);
}

void CoreAudioOutput::unpause()
{
	OSSpinLockLock(this->_spinlockAU);
	AudioOutputUnitStart(this->_au);
	OSSpinLockUnlock(this->_spinlockAU);
}

void CoreAudioOutput::stop()
{
	OSSpinLockLock(this->_spinlockAU);
	AudioOutputUnitStop(this->_au);
	OSSpinLockUnlock(this->_spinlockAU);
	
	this->clearBuffer();
}

void CoreAudioOutput::writeToBuffer(const void *buffer, size_t numberSampleFrames)
{
	size_t availableSampleFrames = this->_buffer->getAvailableElements();
	if (availableSampleFrames < numberSampleFrames)
	{
		this->_buffer->drop(numberSampleFrames - availableSampleFrames);
	}
	
	this->_buffer->write(buffer, numberSampleFrames);
}

void CoreAudioOutput::clearBuffer()
{
	this->_buffer->clear();
}

void CoreAudioOutput::mute()
{
	OSSpinLockLock(this->_spinlockAU);
	AudioUnitSetParameter(this->_au, kHALOutputParam_Volume, kAudioUnitScope_Global, 0, 0.0f, 0);
	OSSpinLockUnlock(this->_spinlockAU);
}

void CoreAudioOutput::unmute()
{
	OSSpinLockLock(this->_spinlockAU);
	AudioUnitSetParameter(this->_au, kHALOutputParam_Volume, kAudioUnitScope_Global, 0, this->_volume, 0);
	OSSpinLockUnlock(this->_spinlockAU);
}

size_t CoreAudioOutput::getAvailableSamples() const
{
	return this->_buffer->getAvailableElements();
}

float CoreAudioOutput::getVolume() const
{
	return this->_volume;
}

void CoreAudioOutput::setVolume(float vol)
{
	this->_volume = vol;
	
	OSSpinLockLock(this->_spinlockAU);
	AudioUnitSetParameter(this->_au, kHALOutputParam_Volume, kAudioUnitScope_Global, 0, vol, 0);
	OSSpinLockUnlock(this->_spinlockAU);
}

OSStatus CoreAudioOutputRenderCallback(void *inRefCon,
									   AudioUnitRenderActionFlags *ioActionFlags,
									   const AudioTimeStamp *inTimeStamp,
									   UInt32 inBusNumber,
									   UInt32 inNumberFrames,
									   AudioBufferList *ioData)
{
	RingBuffer *__restrict__ audioBuffer = (RingBuffer *)inRefCon;
	UInt8 *__restrict__ playbackBuffer = (UInt8 *)ioData->mBuffers[0].mData;
	const size_t framesRead = audioBuffer->read(playbackBuffer, inNumberFrames);
	
	// Pad any remaining samples.
	if (framesRead < inNumberFrames)
	{
		const size_t frameSize = audioBuffer->getElementSize();
		memset(playbackBuffer + (framesRead * frameSize), 0, (inNumberFrames - framesRead) * frameSize);
	}
	
	return noErr;
}

#pragma mark -

bool CreateAudioUnitInstance(AudioUnit *au, ComponentDescription *auDescription)
{
	bool result = false;
	if (au == NULL || auDescription == NULL)
	{
		return result;
	}
	
	Component theComponent = FindNextComponent(NULL, auDescription);
	if (theComponent == NULL)
	{
		return result;
	}
	
	OSErr error = OpenAComponent(theComponent, au);
	if (error != noErr)
	{
		return result;
	}
	
	result = true;
	return result;
}

#if !defined(FORCE_AUDIOCOMPONENT_10_5) && defined(MAC_OS_X_VERSION_10_6) && MAC_OS_X_VERSION_MAX_ALLOWED >= MAC_OS_X_VERSION_10_6
bool CreateAudioUnitInstance(AudioUnit *au, AudioComponentDescription *auDescription)
{
	bool result = false;
	if (au == NULL || auDescription == NULL || !IsOSXVersionSupported(10, 6, 0))
	{
		return result;
	}
	
	AudioComponent theComponent = AudioComponentFindNext(NULL, auDescription);
	if (theComponent == NULL)
	{
		return result;
	}
	
	OSStatus error = AudioComponentInstanceNew(theComponent, au);
	if (error != noErr)
	{
		return result;
	}
	
	result = true;
	return result;
}
#endif

void DestroyAudioUnitInstance(AudioUnit *au)
{
	if (au == NULL)
	{
		return;
	}
	
	AudioOutputUnitStop(*au);
	AudioUnitUninitialize(*au);
#if !defined(FORCE_AUDIOCOMPONENT_10_5) && defined(MAC_OS_X_VERSION_10_6) && MAC_OS_X_VERSION_MAX_ALLOWED >= MAC_OS_X_VERSION_10_6
	if (IsOSXVersionSupported(10, 6, 0))
	{
		AudioComponentInstanceDispose(*au);
	}
	else
#endif
	{
		CloseComponent(*au);
	}
}
