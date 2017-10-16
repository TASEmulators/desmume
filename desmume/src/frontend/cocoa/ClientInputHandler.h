/*
	Copyright (C) 2017 DeSmuME team
 
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

#ifndef _CLIENT_INPUT_HANDLER_H_
#define _CLIENT_INPUT_HANDLER_H_

#include <pthread.h>
#include <string.h>
#include <map>
#include <vector>

#define INPUT_HANDLER_STRING_LENGTH					256

#define INPUT_STATES_CLEAR_VALUE					0xFFFFFFFF00FF03FFULL

#define MIC_SAMPLE_RATE								16000.0
#define MIC_SAMPLE_RESOLUTION						8			// Bits per sample; must be a multiple of 8
#define MIC_NUMBER_CHANNELS							1			// Number of channels
#define MIC_SAMPLE_SIZE								((MIC_SAMPLE_RESOLUTION / 8) * MIC_NUMBER_CHANNELS) // Bytes per sample, multiplied by the number of channels
#define MIC_MAX_BUFFER_SAMPLES						((MIC_SAMPLE_RATE / DS_FRAMES_PER_SECOND) * MIC_SAMPLE_SIZE)
#define MIC_CAPTURE_FRAMES							192			// The number of audio frames that the NDS microphone should pull. The lower this value, the lower the latency. Ensure that this value is not too low, or else audio frames may be lost.
#define MIC_NULL_LEVEL_THRESHOLD					2.5
#define MIC_CLIP_LEVEL_THRESHOLD					39.743665431525209 // ((2.0/pi) * MIC_NULL_SAMPLE_VALUE) - 1.0
#define MIC_NULL_SAMPLE_VALUE						64

enum NDSInputID
{
	NDSInputID_A = 0,
	NDSInputID_B,
	NDSInputID_Select,
	NDSInputID_Start,
	NDSInputID_Right,
	NDSInputID_Left,
	NDSInputID_Up,
	NDSInputID_Down,
	NDSInputID_R,
	NDSInputID_L,
	
	NDSInputID_X,
	NDSInputID_Y,
	NDSInputID_Debug,
	NDSInputID_Touch,
	NDSInputID_Lid,
	
	NDSInputID_Piano_C,
	NDSInputID_Piano_CSharp,
	NDSInputID_Piano_D,
	NDSInputID_Piano_DSharp,
	NDSInputID_Piano_E,
	NDSInputID_Piano_F,
	NDSInputID_Piano_FSharp,
	NDSInputID_Piano_G,
	NDSInputID_Piano_GSharp,
	NDSInputID_Piano_A,
	NDSInputID_Piano_ASharp,
	NDSInputID_Piano_B,
	NDSInputID_Piano_HighC,
	
	NDSInputID_GuitarGrip_Green,
	NDSInputID_GuitarGrip_Red,
	NDSInputID_GuitarGrip_Yellow,
	NDSInputID_GuitarGrip_Blue,
	
	NDSInputID_Paddle,
	NDSInputID_Microphone,
	NDSInputID_Reset,
	
	NDSInputID_InputCount
};

enum NDSInputStateBit
{
	NDSInputStateBit_A						= 0,
	NDSInputStateBit_B						= 1,
	NDSInputStateBit_Select					= 2,
	NDSInputStateBit_Start					= 3,
	NDSInputStateBit_Right					= 4,
	NDSInputStateBit_Left					= 5,
	NDSInputStateBit_Up						= 6,
	NDSInputStateBit_Down					= 7,
	NDSInputStateBit_R						= 8,
	NDSInputStateBit_L						= 9,
	
	NDSInputStateBit_X						= 16,
	NDSInputStateBit_Y						= 17,
	NDSInputStateBit_Debug					= 19,
	NDSInputStateBit_Touch					= 22,
	NDSInputStateBit_Lid					= 23,
	
	NDSInputStateBit_Piano_C				= 32,
	NDSInputStateBit_Piano_CSharp			= 33,
	NDSInputStateBit_Piano_D				= 34,
	NDSInputStateBit_Piano_DSharp			= 35,
	NDSInputStateBit_Piano_E				= 36,
	NDSInputStateBit_Piano_F				= 37,
	NDSInputStateBit_Piano_FSharp			= 38,
	NDSInputStateBit_Piano_G				= 39,
	NDSInputStateBit_Piano_GSharp			= 40,
	NDSInputStateBit_Piano_A				= 41,
	NDSInputStateBit_Piano_ASharp			= 42,
	NDSInputStateBit_Piano_B				= 45,
	NDSInputStateBit_Piano_HighC			= 46,
	
	NDSInputStateBit_GuitarGrip_Green		= 51,
	NDSInputStateBit_GuitarGrip_Red			= 52,
	NDSInputStateBit_GuitarGrip_Yellow		= 53,
	NDSInputStateBit_GuitarGrip_Blue		= 54,
	
	NDSInputStateBit_Paddle					= 56,
	NDSInputStateBit_Microphone				= 57,
	NDSInputStateBit_Reset					= 58
};

enum ClientInputDeviceState
{
	ClientInputDeviceState_Off = 0,
	ClientInputDeviceState_On,
	ClientInputDeviceState_Mixed
};

enum MicrophoneMode
{
	MicrophoneMode_None = 0,
	MicrophoneMode_InternalNoise,
	MicrophoneMode_AudioFile,
	MicrophoneMode_WhiteNoise,
	MicrophoneMode_Physical,
	MicrophoneMode_SineWave
};

class ClientExecutionControl;
class AudioGenerator;
class NullGenerator;
class InternalNoiseGenerator;
class WhiteNoiseGenerator;
class SineWaveGenerator;
class AudioSampleBlockGenerator;
struct ClientCommandAttributes;

typedef union
{
	uint64_t value;
	
	struct
	{
		uint16_t gbaKeys;
		uint16_t ndsKeysExt;
		uint16_t easyPianoKeys;
		uint8_t guitarGripKeys;
		uint8_t miscKeys;
	};
	
	struct
	{
#ifndef MSB_FIRST
		uint8_t A:1;
		uint8_t B:1;
		uint8_t Select:1;
		uint8_t Start:1;
		uint8_t Right:1;
		uint8_t Left:1;
		uint8_t Up:1;
		uint8_t Down:1;
		
		uint8_t R:1;
		uint8_t L:1;
		uint8_t :6;
		
		uint8_t X:1;
		uint8_t Y:1;
		uint8_t :1;
		uint8_t Debug:1;
		uint8_t :2;
		uint8_t Touch:1;
		uint8_t Lid:1;
		
		uint8_t :8;
		
		uint8_t PianoC:1;
		uint8_t PianoCSharp:1;
		uint8_t PianoD:1;
		uint8_t PianoDSharp:1;
		uint8_t PianoE:1;
		uint8_t PianoF:1;
		uint8_t PianoFSharp:1;
		uint8_t PianoG:1;
		
		uint8_t PianoGSharp:1;
		uint8_t PianoA:1;
		uint8_t PianoASharp:1;
		uint8_t :2;
		uint8_t PianoB:1;
		uint8_t PianoHighC:1;
		uint8_t :1;
		
		uint8_t :3;
		uint8_t GuitarGripBlue:1;
		uint8_t GuitarGripYellow:1;
		uint8_t GuitarGripRed:1;
		uint8_t GuitarGripGreen:1;
		uint8_t :1;
		
		uint8_t Paddle:1;
		uint8_t Microphone:1;
		uint8_t Reset:1;
		uint8_t :5;
#else
		uint8_t Down:1;
		uint8_t Up:1;
		uint8_t Left:1;
		uint8_t Right:1;
		uint8_t Start:1;
		uint8_t Select:1;
		uint8_t B:1;
		uint8_t A:1;
		
		uint8_t :6;
		uint8_t L:1;
		uint8_t R:1;
		
		uint8_t Lid:1;
		uint8_t Touch:1;
		uint8_t :2;
		uint8_t Debug:1;
		uint8_t :1;
		uint8_t Y:1;
		uint8_t X:1;
		
		uint8_t :8;
		
		uint8_t PianoG:1;
		uint8_t PianoFSharp:1;
		uint8_t PianoF:1;
		uint8_t PianoE:1;
		uint8_t PianoDSharp:1;
		uint8_t PianoD:1;
		uint8_t PianoCSharp:1;
		uint8_t PianoC:1;
		
		uint8_t :1;
		uint8_t PianoHighC:1;
		uint8_t PianoB:1;
		uint8_t :2;
		uint8_t PianoASharp:1;
		uint8_t PianoA:1;
		uint8_t PianoGSharp:1;
		
		uint8_t :1;
		uint8_t GuitarGripGreen:1;
		uint8_t GuitarGripRed:1;
		uint8_t GuitarGripYellow:1;
		uint8_t GuitarGripBlue:1;
		uint8_t :3;
		
		uint8_t :5;
		uint8_t Reset:1;
		uint8_t Microphone:1;
		uint8_t Paddle:1;
#endif
	};
} NDSInputState; // Each bit represents the Pressed/Released state of a single input. Pressed=0, Released=1

typedef void (*ClientCommandDispatcher)(const ClientCommandAttributes &cmdAttr, void *dispatcherObject);

struct ClientInputDeviceProperties
{
	char deviceName[INPUT_HANDLER_STRING_LENGTH];
	char deviceCode[INPUT_HANDLER_STRING_LENGTH];
	char elementName[INPUT_HANDLER_STRING_LENGTH];
	char elementCode[INPUT_HANDLER_STRING_LENGTH];
	bool isAnalog;					// This is an analog input, as opposed to being a digital input.
	
	ClientInputDeviceState state;	// The input state that is sent on command dispatch
	int32_t intCoordX;				// The X-coordinate as an int for commands that require a location
	int32_t intCoordY;				// The Y-coordinate as an int for commands that require a location
	float floatCoordX;				// The X-coordinate as a float for commands that require a location
	float floatCoordY;				// The Y-coordinate as a float for commands that require a location
	float scalar;					// A scalar value for commands that require a scalar
	void *object;					// An object for commands that require an object
};
typedef struct ClientInputDeviceProperties ClientInputDeviceProperties;

struct ClientInput
{
	bool isPressed;
	bool turbo;
	bool autohold;
	uint32_t turboPattern;
	uint8_t turboPatternStep;
	uint8_t turboPatternLength;
};
typedef struct ClientInput ClientInput;

struct ClientCommandAttributes
{
	ClientCommandDispatcher dispatchFunction;		// The function to be called when this command is dispatched.
	
	char tag[INPUT_HANDLER_STRING_LENGTH];			// A string identifier for these attributes
	int32_t intValue[4];							// Context dependent int values
	float floatValue[4];							// Context dependent float values
	void *object[4];								// Context dependent objects
	
	bool useInputForIntCoord;						// The command will prefer the input device's int coordinate
	bool useInputForFloatCoord;						// The command will prefer the input device's float coordinate
	bool useInputForScalar;							// The command will prefer the input device's scalar
	bool useInputForObject;							// The command will prefer the input device's object
	
	ClientInputDeviceProperties input;				// The input device's properties
	bool allowAnalogInput;							// Flag for allowing a command to accept analog inputs
};
typedef struct ClientCommandAttributes ClientCommandAttributes;

typedef std::map<NDSInputID, size_t> NDSUserInputMap;
typedef std::map<NDSInputID, NDSInputStateBit> NDSInputStateBitMap;
typedef std::vector<ClientInputDeviceProperties> ClientInputDevicePropertiesList;

class ClientInputDevicePropertiesEncoder
{
public:
	virtual ClientInputDeviceProperties EncodeKeyboardInput(const int32_t keyCode, bool keyPressed);
	virtual ClientInputDeviceProperties EncodeMouseInput(const int32_t buttonNumber, float touchLocX, float touchLocY, bool buttonPressed);
};

class ClientInputHandler
{
protected:
	ClientExecutionControl *_execControl;
	
	NDSUserInputMap _userInputMap;
	NDSInputStateBitMap _inputStateBitMap;
	
	AudioGenerator *_softwareMicSampleGeneratorPending;
	AudioGenerator *_softwareMicSampleGeneratorProcessing;
	AudioGenerator *_softwareMicSampleGeneratorApplied;
	NullGenerator *_nullSampleGenerator;
	InternalNoiseGenerator *_internalNoiseGenerator;
	WhiteNoiseGenerator *_whiteNoiseGenerator;
	SineWaveGenerator *_sineWaveGenerator;
	AudioSampleBlockGenerator *_selectedAudioFileGenerator;
	
	AudioGenerator *_hardwareMicSampleGenerator;
	
	ClientInput _clientInputPending[NDSInputID_InputCount];
	ClientInput _clientInputProcessing[NDSInputID_InputCount];
	ClientInput _clientInputApplied[NDSInputID_InputCount];
	
	bool _enableAutohold;
	
	uint8_t _touchLocXPending;
	uint8_t _touchLocYPending;
	uint8_t _touchPressurePending;
	int16_t _paddleValuePending;
	int16_t _paddleAdjustPending;
	
	uint8_t _touchLocXProcessing;
	uint8_t _touchLocYProcessing;
	uint8_t _touchPressureProcessing;
	int16_t _paddleValueProcessing;
	int16_t _paddleAdjustProcessing;
	
	uint8_t _touchLocXApplied;
	uint8_t _touchLocYApplied;
	uint8_t _touchPressureApplied;
	int16_t _paddleValueApplied;
	int16_t _paddleAdjustApplied;
	
	float _avgMicLevel;
	float _avgMicLevelTotal;
	float _avgMicLevelsRead;
	bool _isHardwareMicMuted;
	bool _isHardwareMicPaused;
	
	pthread_mutex_t _mutexInputsPending;
	
public:
	ClientInputHandler();
	~ClientInputHandler();
	
	ClientExecutionControl* GetClientExecutionController();
	void SetClientExecutionController(ClientExecutionControl *execControl);
	
	bool GetEnableAutohold();
	void SetEnableAutohold(bool enable);
	void ClearAutohold();
	
	void SetClientInputStateUsingID(NDSInputID inputID, bool pressedState);
	void SetClientInputStateUsingID(NDSInputID inputID, bool pressedState, bool isTurboEnabled, uint32_t turboPattern, uint32_t turboPatternLength);
	void SetClientTouchState(bool pressedState, uint8_t touchLocX, uint8_t touchLocY, uint8_t touchPressure);
	
	double GetSineWaveFrequency();
	void SetSineWaveFrequency(double freq);
	
	float GetAverageMicLevel();	
	void AddSampleToAverageMicLevel(uint8_t sampleValue);
	void ClearAverageMicLevel();
	
	bool IsMicrophoneIdle();
	bool IsMicrophoneClipping();
	
	AudioGenerator* GetClientSoftwareMicSampleGenerator();
	AudioGenerator* GetClientSoftwareMicSampleGeneratorApplied();
	AudioSampleBlockGenerator* GetClientSelectedAudioFileGenerator();
	void SetClientSelectedAudioFileGenerator(AudioSampleBlockGenerator *selectedAudioFileGenerator);
	
	void SetClientHardwareMicSampleGeneratorApplied(AudioGenerator *hwGenerator);
	AudioGenerator* GetClientHardwareMicSampleGeneratorApplied();
	
	bool GetClientSoftwareMicState();
	bool GetClientSoftwareMicStateApplied();
	void SetClientSoftwareMicState(bool pressedState, MicrophoneMode micMode);
	
	int16_t GetClientPaddleAdjust();
	void SetClientPaddleAdjust(int16_t paddleAdjust);
	
	const ClientInput* GetClientInputsApplied();
	uint8_t GetTouchLocXApplied();
	uint8_t GetTouchLocYApplied();
	uint8_t GetTouchPressureApplied();
	int16_t GetPaddleValueApplied();
	int16_t GetPaddleAdjustApplied();
	
	void ProcessInputs();
	void ApplyInputs();
	
	virtual bool IsHardwareMicAvailable();
	virtual void ResetHardwareMic();
	virtual uint8_t HandleMicSampleRead();
	virtual void ReportAverageMicLevel();
	
	virtual bool GetHardwareMicMute();
	virtual void SetHardwareMicMute(bool muteState);
	
	virtual bool GetHardwareMicPause();
	virtual void SetHardwareMicPause(bool pauseState);
	
	virtual float GetHardwareMicNormalizedGain();
	virtual void SetHardwareMicGainAsNormalized(float normalizedGain);
};

#endif // _CLIENT_INPUT_HANDLER_H_
