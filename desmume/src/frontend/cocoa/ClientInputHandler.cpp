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

#include "../../movie.h"
#include "../../NDSSystem.h"
#include "../../slot2.h"

#include "audiosamplegenerator.h"
#include "ClientExecutionControl.h"

#include "ClientInputHandler.h"


ClientInputDeviceProperties ClientInputDevicePropertiesEncoder::EncodeKeyboardInput(const int32_t keyCode, bool keyPressed)
{
	ClientInputDeviceProperties inputProperty;
	strncpy(inputProperty.deviceCode, "GenericClientKeyboard", INPUT_HANDLER_STRING_LENGTH);
	strncpy(inputProperty.deviceName, "Keyboard", INPUT_HANDLER_STRING_LENGTH);
	snprintf(inputProperty.elementCode, INPUT_HANDLER_STRING_LENGTH, "%i", (int)keyCode);
	snprintf(inputProperty.elementName, INPUT_HANDLER_STRING_LENGTH, "KeyCode %i", (int)keyCode);
	
	inputProperty.isAnalog		= false;
	inputProperty.state			= (keyPressed) ? ClientInputDeviceState_On : ClientInputDeviceState_Off;
	inputProperty.intCoordX		= 0;
	inputProperty.intCoordY		= 0;
	inputProperty.floatCoordX	= 0.0f;
	inputProperty.floatCoordY	= 0.0f;
	inputProperty.scalar		= (keyPressed) ? 1.0f : 0.0f;
	inputProperty.object		= NULL;
	
	return inputProperty;
}

ClientInputDeviceProperties ClientInputDevicePropertiesEncoder::EncodeMouseInput(const int32_t buttonNumber, float touchLocX, float touchLocY, bool buttonPressed)
{
	ClientInputDeviceProperties inputProperty;
	strncpy(inputProperty.deviceCode, "GenericClientMouse", INPUT_HANDLER_STRING_LENGTH);
	strncpy(inputProperty.deviceName, "Mouse", INPUT_HANDLER_STRING_LENGTH);
	snprintf(inputProperty.elementCode, INPUT_HANDLER_STRING_LENGTH, "%i", (int)buttonNumber);
	strncpy(inputProperty.elementName, "Generic Mouse Button", INPUT_HANDLER_STRING_LENGTH);
	
	inputProperty.isAnalog		= false;
	inputProperty.state			= (buttonPressed) ? ClientInputDeviceState_On : ClientInputDeviceState_Off;
	inputProperty.intCoordX		= (int32_t)touchLocX;
	inputProperty.intCoordY		= (int32_t)touchLocY;
	inputProperty.floatCoordX	= touchLocX;
	inputProperty.floatCoordY	= touchLocY;
	inputProperty.scalar		= (buttonPressed) ? 1.0f : 0.0f;
	inputProperty.object		= NULL;
	
	return inputProperty;
}

ClientInputHandler::ClientInputHandler()
{
	_execControl = NULL;
	
	_nullSampleGenerator = new NullGenerator;
	_internalNoiseGenerator = new InternalNoiseGenerator;
	_whiteNoiseGenerator = new WhiteNoiseGenerator;
	_sineWaveGenerator = new SineWaveGenerator(250.0, MIC_SAMPLE_RATE);
	_selectedAudioFileGenerator = NULL; // Note that this value can be NULL.
	_hardwareMicSampleGenerator = _nullSampleGenerator;
	
	_avgMicLevel = 0.0f;
	_avgMicLevelTotal = 0.0f;
	_avgMicLevelsRead = 0.0f;
	_isHardwareMicMuted = true;
	_isHardwareMicPaused = true;
	
	_enableAutohold = false;
	
	_touchLocXPending     = _touchLocXProcessing     = _touchLocXApplied     = 0;
	_touchLocYPending     = _touchLocYProcessing     = _touchLocYApplied     = 0;
	_touchPressurePending = _touchPressureProcessing = _touchPressureApplied = 50;
	_paddleValuePending   = _paddleValueProcessing   = _paddleValueApplied   = 0;
	_paddleAdjustPending  = _paddleAdjustProcessing  = _paddleAdjustApplied  = 0;
	
	_softwareMicSampleGeneratorPending = _nullSampleGenerator;
	_softwareMicSampleGeneratorProcessing = _nullSampleGenerator;
	_softwareMicSampleGeneratorApplied = _nullSampleGenerator;
	
	memset(_clientInputPending, 0, sizeof(_clientInputPending));
	memset(_clientInputProcessing, 0, sizeof(_clientInputProcessing));
	memset(_clientInputApplied, 0, sizeof(_clientInputApplied));
	
	_userInputMap[NDSInputID_Debug]     = 0;
	_userInputMap[NDSInputID_R]         = 1;
	_userInputMap[NDSInputID_L]         = 2;
	_userInputMap[NDSInputID_X]         = 3;
	_userInputMap[NDSInputID_Y]         = 4;
	_userInputMap[NDSInputID_A]         = 5;
	_userInputMap[NDSInputID_B]         = 6;
	_userInputMap[NDSInputID_Start]     = 7;
	_userInputMap[NDSInputID_Select]    = 8;
	_userInputMap[NDSInputID_Up]        = 9;
	_userInputMap[NDSInputID_Down]      = 10;
	_userInputMap[NDSInputID_Left]      = 11;
	_userInputMap[NDSInputID_Right]     = 12;
	_userInputMap[NDSInputID_Lid]       = 13;
	
	_inputStateBitMap[NDSInputID_A]                 = NDSInputStateBit_A;
	_inputStateBitMap[NDSInputID_B]                 = NDSInputStateBit_B;
	_inputStateBitMap[NDSInputID_Select]            = NDSInputStateBit_Select;
	_inputStateBitMap[NDSInputID_Start]             = NDSInputStateBit_Start;
	_inputStateBitMap[NDSInputID_Right]             = NDSInputStateBit_Right;
	_inputStateBitMap[NDSInputID_Left]              = NDSInputStateBit_Left;
	_inputStateBitMap[NDSInputID_Up]                = NDSInputStateBit_Up;
	_inputStateBitMap[NDSInputID_Down]              = NDSInputStateBit_Down;
	_inputStateBitMap[NDSInputID_R]                 = NDSInputStateBit_R;
	_inputStateBitMap[NDSInputID_L]                 = NDSInputStateBit_L;
	_inputStateBitMap[NDSInputID_X]                 = NDSInputStateBit_X;
	_inputStateBitMap[NDSInputID_Y]                 = NDSInputStateBit_Y;
	_inputStateBitMap[NDSInputID_Debug]             = NDSInputStateBit_Debug;
	_inputStateBitMap[NDSInputID_Touch]             = NDSInputStateBit_Touch;
	_inputStateBitMap[NDSInputID_Lid]               = NDSInputStateBit_Lid;
	_inputStateBitMap[NDSInputID_Piano_C]           = NDSInputStateBit_Piano_C;
	_inputStateBitMap[NDSInputID_Piano_CSharp]      = NDSInputStateBit_Piano_CSharp;
	_inputStateBitMap[NDSInputID_Piano_D]           = NDSInputStateBit_Piano_D;
	_inputStateBitMap[NDSInputID_Piano_DSharp]      = NDSInputStateBit_Piano_DSharp;
	_inputStateBitMap[NDSInputID_Piano_E]           = NDSInputStateBit_Piano_E;
	_inputStateBitMap[NDSInputID_Piano_F]           = NDSInputStateBit_Piano_F;
	_inputStateBitMap[NDSInputID_Piano_FSharp]      = NDSInputStateBit_Piano_FSharp;
	_inputStateBitMap[NDSInputID_Piano_G]           = NDSInputStateBit_Piano_G;
	_inputStateBitMap[NDSInputID_Piano_GSharp]      = NDSInputStateBit_Piano_GSharp;
	_inputStateBitMap[NDSInputID_Piano_A]           = NDSInputStateBit_Piano_A;
	_inputStateBitMap[NDSInputID_Piano_ASharp]      = NDSInputStateBit_Piano_ASharp;
	_inputStateBitMap[NDSInputID_Piano_B]           = NDSInputStateBit_Piano_B;
	_inputStateBitMap[NDSInputID_Piano_HighC]       = NDSInputStateBit_Piano_HighC;
	_inputStateBitMap[NDSInputID_GuitarGrip_Green]  = NDSInputStateBit_GuitarGrip_Green;
	_inputStateBitMap[NDSInputID_GuitarGrip_Red]    = NDSInputStateBit_GuitarGrip_Red;
	_inputStateBitMap[NDSInputID_GuitarGrip_Yellow] = NDSInputStateBit_GuitarGrip_Yellow;
	_inputStateBitMap[NDSInputID_GuitarGrip_Blue]   = NDSInputStateBit_GuitarGrip_Blue;
	_inputStateBitMap[NDSInputID_Paddle]            = NDSInputStateBit_Paddle;
	_inputStateBitMap[NDSInputID_Microphone]        = NDSInputStateBit_Microphone;
	_inputStateBitMap[NDSInputID_Reset]             = NDSInputStateBit_Reset;
	
	pthread_mutex_init(&_mutexInputsPending, NULL);
}

ClientInputHandler::~ClientInputHandler()
{
	pthread_mutex_destroy(&this->_mutexInputsPending);
	
	delete this->_nullSampleGenerator;
	delete this->_internalNoiseGenerator;
	delete this->_whiteNoiseGenerator;
	delete this->_sineWaveGenerator;
}

ClientExecutionControl* ClientInputHandler::GetClientExecutionController()
{
	return this->_execControl;
}

void ClientInputHandler::SetClientExecutionController(ClientExecutionControl *execControl)
{
	this->_execControl = execControl;
}

bool ClientInputHandler::GetEnableAutohold()
{
	pthread_mutex_lock(&this->_mutexInputsPending);
	const bool enable = this->_enableAutohold;
	pthread_mutex_unlock(&this->_mutexInputsPending);
	
	return enable;
}

void ClientInputHandler::SetEnableAutohold(bool enable)
{
	pthread_mutex_lock(&this->_mutexInputsPending);
	this->_enableAutohold = enable;
	pthread_mutex_unlock(&this->_mutexInputsPending);
}

void ClientInputHandler::ClearAutohold()
{
	pthread_mutex_lock(&this->_mutexInputsPending);
	
	for (size_t i = 0; i < NDSInputID_InputCount; i++)
	{
		this->_clientInputPending[i].autohold = false;
	}
	
	pthread_mutex_unlock(&this->_mutexInputsPending);
}

void ClientInputHandler::SetClientInputStateUsingID(NDSInputID inputID, bool pressedState)
{
	this->SetClientInputStateUsingID(inputID, pressedState, false, 0, 0);
}

void ClientInputHandler::SetClientInputStateUsingID(NDSInputID inputID, bool pressedState, bool isTurboEnabled, uint32_t turboPattern, uint32_t turboPatternLength)
{
	if (inputID >= NDSInputID_InputCount)
	{
		return;
	}
	
	pthread_mutex_lock(&this->_mutexInputsPending);
	
	if (this->_enableAutohold && pressedState)
	{
		this->_clientInputPending[inputID].autohold = true;
	}
	
	this->_clientInputPending[inputID].isPressed = pressedState;
	this->_clientInputPending[inputID].turbo = isTurboEnabled;
	this->_clientInputPending[inputID].turboPattern = turboPattern;
	this->_clientInputPending[inputID].turboPatternLength = (turboPatternLength > 32) ? 32 : turboPatternLength;
	
	if (this->_execControl != NULL)
	{
		NDSFrameInfo &frameInfoMutable = (NDSFrameInfo &)this->_execControl->GetNDSFrameInfo();
		const uint64_t bitValue = LOCAL_TO_LE_64(1ULL << this->_inputStateBitMap[inputID]);
		
		frameInfoMutable.inputStatesPending.value = (this->_clientInputPending[inputID].isPressed) ? frameInfoMutable.inputStatesPending.value & ~bitValue : frameInfoMutable.inputStatesPending.value | bitValue;
	}
	
	pthread_mutex_unlock(&this->_mutexInputsPending);
}

void ClientInputHandler::SetClientTouchState(bool pressedState, uint8_t touchLocX, uint8_t touchLocY, uint8_t touchPressure)
{
	pthread_mutex_lock(&this->_mutexInputsPending);
	
	this->_clientInputPending[NDSInputID_Touch].isPressed = pressedState;
	this->_touchLocXPending = touchLocX;
	this->_touchLocYPending = touchLocY;
	this->_touchPressurePending = touchPressure;
	
	if (this->_execControl != NULL)
	{
		NDSFrameInfo &frameInfoMutable = (NDSFrameInfo &)this->_execControl->GetNDSFrameInfo();
		frameInfoMutable.inputStatesPending.Touch = (this->_clientInputPending[NDSInputID_Touch].isPressed) ? 0 : 1;
		frameInfoMutable.touchLocXPending         = this->_touchLocXPending;
		frameInfoMutable.touchLocYPending         = this->_touchLocYPending;
		frameInfoMutable.touchPressurePending     = this->_touchPressurePending;
	}
	
	pthread_mutex_unlock(&this->_mutexInputsPending);
}

double ClientInputHandler::GetSineWaveFrequency()
{
	return this->_sineWaveGenerator->getFrequency();
}

void ClientInputHandler::SetSineWaveFrequency(double freq)
{
	this->_sineWaveGenerator->setFrequency(freq);
}

float ClientInputHandler::GetAverageMicLevel()
{
	return this->_avgMicLevel;
}

void ClientInputHandler::AddSampleToAverageMicLevel(uint8_t sampleValue)
{
	this->_avgMicLevelTotal += (float)( (MIC_NULL_SAMPLE_VALUE > sampleValue) ? MIC_NULL_SAMPLE_VALUE - sampleValue : sampleValue - MIC_NULL_SAMPLE_VALUE );
	this->_avgMicLevelsRead += 1.0f;
	this->_avgMicLevel = this->_avgMicLevelTotal / this->_avgMicLevelsRead;
}

void ClientInputHandler::ClearAverageMicLevel()
{
	this->_avgMicLevelTotal = 0.0f;
	this->_avgMicLevelsRead = 0.0f;
	this->_avgMicLevel = 0.0f;
}

bool ClientInputHandler::IsMicrophoneIdle()
{
	return (this->_avgMicLevel < MIC_NULL_LEVEL_THRESHOLD);
}

bool ClientInputHandler::IsMicrophoneClipping()
{
	return (this->_avgMicLevel >= MIC_CLIP_LEVEL_THRESHOLD);
}

AudioGenerator* ClientInputHandler::GetClientSoftwareMicSampleGenerator()
{
	pthread_mutex_lock(&this->_mutexInputsPending);
	AudioGenerator *softwareMicSampleGenerator = this->_softwareMicSampleGeneratorPending;
	pthread_mutex_unlock(&this->_mutexInputsPending);
	
	return softwareMicSampleGenerator;
}

AudioGenerator* ClientInputHandler::GetClientSoftwareMicSampleGeneratorApplied()
{
	return this->_softwareMicSampleGeneratorApplied;
}

AudioSampleBlockGenerator* ClientInputHandler::GetClientSelectedAudioFileGenerator()
{
	pthread_mutex_lock(&this->_mutexInputsPending);
	AudioSampleBlockGenerator *selectedAudioFileGenerator = this->_selectedAudioFileGenerator;
	pthread_mutex_unlock(&this->_mutexInputsPending);
	
	return selectedAudioFileGenerator;
}

void ClientInputHandler::SetClientSelectedAudioFileGenerator(AudioSampleBlockGenerator *selectedAudioFileGenerator)
{
	pthread_mutex_lock(&this->_mutexInputsPending);
	this->_selectedAudioFileGenerator = selectedAudioFileGenerator;
	pthread_mutex_unlock(&this->_mutexInputsPending);
}

void ClientInputHandler::SetClientHardwareMicSampleGeneratorApplied(AudioGenerator *hwGenerator)
{
	if (hwGenerator == NULL)
	{
		this->_hardwareMicSampleGenerator = this->_nullSampleGenerator;
	}
	else
	{
		this->_hardwareMicSampleGenerator = hwGenerator;
	}
}

AudioGenerator* ClientInputHandler::GetClientHardwareMicSampleGeneratorApplied()
{
	return this->_hardwareMicSampleGenerator;
}

bool ClientInputHandler::GetClientSoftwareMicState()
{
	pthread_mutex_lock(&this->_mutexInputsPending);
	const bool isPressed = this->_clientInputPending[NDSInputID_Microphone].isPressed;
	pthread_mutex_unlock(&this->_mutexInputsPending);
	
	return isPressed;
}

bool ClientInputHandler::GetClientSoftwareMicStateApplied()
{
	return this->_clientInputApplied[NDSInputID_Microphone].isPressed;
}

void ClientInputHandler::SetClientSoftwareMicState(bool pressedState, MicrophoneMode micMode)
{
	pthread_mutex_lock(&this->_mutexInputsPending);
	
	this->_clientInputPending[NDSInputID_Microphone].isPressed = pressedState;
	
	if (this->_execControl != NULL)
	{
		NDSFrameInfo &frameInfoMutable = (NDSFrameInfo &)this->_execControl->GetNDSFrameInfo();
		frameInfoMutable.inputStatesPending.Microphone = (this->_clientInputPending[NDSInputID_Microphone].isPressed) ? 0 : 1;
	}
	
	if (pressedState)
	{
		switch (micMode)
		{
			case MicrophoneMode_InternalNoise:
				this->_softwareMicSampleGeneratorPending = this->_internalNoiseGenerator;
				break;
				
			case MicrophoneMode_WhiteNoise:
				this->_softwareMicSampleGeneratorPending = this->_whiteNoiseGenerator;
				break;
				
			case MicrophoneMode_SineWave:
				this->_softwareMicSampleGeneratorPending = this->_sineWaveGenerator;
				break;
				
			case MicrophoneMode_AudioFile:
				if (this->_selectedAudioFileGenerator != NULL)
				{
					this->_softwareMicSampleGeneratorPending = this->_selectedAudioFileGenerator;
				}
				break;
				
			default:
				this->_softwareMicSampleGeneratorPending = this->_nullSampleGenerator;
				break;
		}
	}
	else
	{
		this->_softwareMicSampleGeneratorPending = this->_nullSampleGenerator;
	}
	
	pthread_mutex_unlock(&this->_mutexInputsPending);
}

int16_t ClientInputHandler::GetClientPaddleAdjust()
{
	pthread_mutex_lock(&this->_mutexInputsPending);
	const int16_t paddleAdjust = this->_paddleAdjustPending;
	pthread_mutex_unlock(&this->_mutexInputsPending);
	
	return paddleAdjust;
}

void ClientInputHandler::SetClientPaddleAdjust(int16_t paddleAdjust)
{
	pthread_mutex_lock(&this->_mutexInputsPending);
	
	this->_paddleAdjustPending = paddleAdjust;
	this->_paddleValuePending = this->_paddleValueApplied + paddleAdjust;
	
	if (this->_execControl != NULL)
	{
		NDSFrameInfo &frameInfoMutable = (NDSFrameInfo &)this->_execControl->GetNDSFrameInfo();
		frameInfoMutable.paddleValuePending  = this->_paddleValuePending;
		frameInfoMutable.paddleAdjustPending = this->_paddleAdjustPending;
	}
	
	pthread_mutex_unlock(&this->_mutexInputsPending);
}

const ClientInput* ClientInputHandler::GetClientInputsApplied()
{
	return this->_clientInputApplied;
}

uint8_t ClientInputHandler::GetTouchLocXApplied()
{
	return this->_touchLocXApplied;
}

uint8_t ClientInputHandler::GetTouchLocYApplied()
{
	return this->_touchLocYApplied;
}

uint8_t ClientInputHandler::GetTouchPressureApplied()
{
	return this->_touchPressureApplied;
}

int16_t ClientInputHandler::GetPaddleValueApplied()
{
	return this->_paddleValueApplied;
}

int16_t ClientInputHandler:: GetPaddleAdjustApplied()
{
	return this->_paddleAdjustApplied;
}

void ClientInputHandler::ProcessInputs()
{
	// Before we begin input processing, we need to send all pending inputs to the core code.
	pthread_mutex_lock(&this->_mutexInputsPending);
	
	NDS_setPad(this->_clientInputPending[NDSInputID_Right].isPressed,
			   this->_clientInputPending[NDSInputID_Left].isPressed,
			   this->_clientInputPending[NDSInputID_Down].isPressed,
			   this->_clientInputPending[NDSInputID_Up].isPressed,
			   this->_clientInputPending[NDSInputID_Select].isPressed,
			   this->_clientInputPending[NDSInputID_Start].isPressed,
			   this->_clientInputPending[NDSInputID_B].isPressed,
			   this->_clientInputPending[NDSInputID_A].isPressed,
			   this->_clientInputPending[NDSInputID_Y].isPressed,
			   this->_clientInputPending[NDSInputID_X].isPressed,
			   this->_clientInputPending[NDSInputID_L].isPressed,
			   this->_clientInputPending[NDSInputID_R].isPressed,
			   this->_clientInputPending[NDSInputID_Debug].isPressed,
			   this->_clientInputPending[NDSInputID_Lid].isPressed);
	
	if (this->_clientInputPending[NDSInputID_Touch].isPressed)
	{
		NDS_setTouchPos((u16)this->_touchLocXPending, (u16)this->_touchLocYPending);
	}
	else
	{
		NDS_releaseTouch();
	}
	
	NDS_setMic(this->_clientInputPending[NDSInputID_Microphone].isPressed);
	
	// Copy all pending inputs for further processing.
	for (size_t i = 0; i < NDSInputID_InputCount; i++)
	{
		this->_clientInputProcessing[i].isPressed = this->_clientInputPending[i].isPressed;
		this->_clientInputProcessing[i].autohold = this->_clientInputPending[i].autohold;
		this->_clientInputProcessing[i].turbo = this->_clientInputPending[i].turbo;
		
		if ( (this->_clientInputProcessing[i].turboPattern != this->_clientInputPending[i].turboPattern) ||
			 (this->_clientInputProcessing[i].turboPatternLength != this->_clientInputPending[i].turboPatternLength) )
		{
			this->_clientInputProcessing[i].turboPatternStep = 0;
			this->_clientInputProcessing[i].turboPattern = this->_clientInputPending[i].turboPattern;
			this->_clientInputProcessing[i].turboPatternLength = this->_clientInputPending[i].turboPatternLength;
		}
	}
	
	this->_touchLocXProcessing = this->_touchLocXPending;
	this->_touchLocYProcessing = this->_touchLocYPending;
	this->_touchPressureProcessing = this->_touchPressurePending;
	this->_paddleAdjustProcessing = this->_paddleAdjustPending;
	this->_paddleValueProcessing = this->_paddleValuePending;
	this->_softwareMicSampleGeneratorProcessing = this->_softwareMicSampleGeneratorPending;
	
	pthread_mutex_unlock(&this->_mutexInputsPending);
	
	// Begin processing the input based on current parameters.
	NDS_beginProcessingInput();
	UserInput &processedInput = NDS_getProcessingUserInput();
	
	if (movieMode == MOVIEMODE_PLAY)
	{
		FCEUMOV_HandlePlayback();
		
		this->_clientInputProcessing[NDSInputID_A].isPressed          = processedInput.buttons.A;
		this->_clientInputProcessing[NDSInputID_B].isPressed          = processedInput.buttons.B;
		this->_clientInputProcessing[NDSInputID_Select].isPressed     = processedInput.buttons.T;
		this->_clientInputProcessing[NDSInputID_Start].isPressed      = processedInput.buttons.S;
		this->_clientInputProcessing[NDSInputID_Right].isPressed      = processedInput.buttons.R;
		this->_clientInputProcessing[NDSInputID_Left].isPressed       = processedInput.buttons.L;
		this->_clientInputProcessing[NDSInputID_Up].isPressed         = processedInput.buttons.U;
		this->_clientInputProcessing[NDSInputID_Down].isPressed       = processedInput.buttons.D;
		this->_clientInputProcessing[NDSInputID_R].isPressed          = processedInput.buttons.E;
		this->_clientInputProcessing[NDSInputID_L].isPressed          = processedInput.buttons.W;
		this->_clientInputProcessing[NDSInputID_X].isPressed          = processedInput.buttons.X;
		this->_clientInputProcessing[NDSInputID_Y].isPressed          = processedInput.buttons.Y;
		this->_clientInputProcessing[NDSInputID_Debug].isPressed      = processedInput.buttons.G;
		this->_clientInputProcessing[NDSInputID_Touch].isPressed      = processedInput.touch.isTouch;
		this->_clientInputProcessing[NDSInputID_Lid].isPressed        = processedInput.buttons.F;
		this->_clientInputProcessing[NDSInputID_Microphone].isPressed = (processedInput.mic.micButtonPressed != 0);
		
		this->_touchLocXProcessing = processedInput.touch.touchX >> 4;
		this->_touchLocYProcessing = processedInput.touch.touchY >> 4;
	}
	else
	{
		if ( (this->_execControl == NULL) || ((this->_execControl != NULL) && (this->_execControl->GetExecutionBehaviorApplied() != ExecutionBehavior_FrameJump)) )
		{
			size_t i = 0;
			
			// First process the inputs that exist in the core code's UserInput struct. The core code will
			// use this struct for its own purposes later.
			for (; i <= NDSInputID_Lid; i++)
			{
				bool &pressedState = (i != NDSInputID_Touch) ? processedInput.buttons.array[this->_userInputMap[(NDSInputID)i]] : processedInput.touch.isTouch;
				pressedState = (this->_clientInputProcessing[i].isPressed || this->_clientInputProcessing[i].autohold);
				this->_clientInputProcessing[i].isPressed = pressedState;
				
				if (this->_clientInputProcessing[i].turbo)
				{
					const bool turboPressedState = ( ((this->_clientInputProcessing[i].turboPattern >> this->_clientInputProcessing[i].turboPatternStep) & 0x00000001) == 1 );
					pressedState = (pressedState && turboPressedState);
					this->_clientInputProcessing[i].isPressed = pressedState;
					
					this->_clientInputProcessing[i].turboPatternStep++;
					if (this->_clientInputProcessing[i].turboPatternStep >= this->_clientInputProcessing[i].turboPatternLength)
					{
						this->_clientInputProcessing[i].turboPatternStep = 0;
					}
				}
			}
			
			// Process the remaining inputs.
			for (i = NDSInputID_Piano_C; i < NDSInputID_InputCount; i++)
			{
				bool &pressedState = this->_clientInputProcessing[i].isPressed;
				pressedState = (this->_clientInputProcessing[i].isPressed || this->_clientInputProcessing[i].autohold);
				
				if (this->_clientInputProcessing[i].turbo)
				{
					const bool turboPressedState = ( ((this->_clientInputProcessing[i].turboPattern >> this->_clientInputProcessing[i].turboPatternStep) & 0x00000001) == 1 );
					pressedState = (pressedState && turboPressedState);
					
					this->_clientInputProcessing[i].turboPatternStep++;
					if (this->_clientInputProcessing[i].turboPatternStep >= this->_clientInputProcessing[i].turboPatternLength)
					{
						this->_clientInputProcessing[i].turboPatternStep = 0;
					}
				}
			}
		}
		else
		{
			memset(&processedInput, 0, sizeof(UserInput));
			memset(this->_clientInputProcessing, 0, sizeof(this->_clientInputProcessing));
			
			this->_softwareMicSampleGeneratorProcessing = this->_nullSampleGenerator;
		}
	}
}

void ClientInputHandler::ApplyInputs()
{
	NDS_endProcessingInput();
	
	this->_touchLocXApplied = this->_touchLocXProcessing;
	this->_touchLocYApplied = this->_touchLocYProcessing;
	this->_touchPressureApplied = this->_touchPressureProcessing;
	this->_paddleAdjustApplied = this->_paddleAdjustProcessing;
	this->_paddleValueApplied = this->_paddleValueProcessing;
	
	memcpy(this->_clientInputApplied, this->_clientInputProcessing, sizeof(this->_clientInputProcessing));
	CommonSettings.StylusPressure = (int)this->_touchPressureApplied;
	this->_softwareMicSampleGeneratorApplied = this->_softwareMicSampleGeneratorProcessing;
	
	if (!this->_clientInputApplied[NDSInputID_Microphone].isPressed)
	{
		this->_internalNoiseGenerator->setSamplePosition(0);
		this->_sineWaveGenerator->setCyclePosition(0.0);
		
		if (this->_selectedAudioFileGenerator != NULL)
		{
			this->_selectedAudioFileGenerator->setSamplePosition(0);
		}
	}
	
	// Setup the inputs from SLOT-2 devices.
	const NDS_SLOT2_TYPE slot2DeviceType = slot2_GetSelectedType();
	switch (slot2DeviceType)
	{
		case NDS_SLOT2_GUITARGRIP:
			guitarGrip_setKey(this->_clientInputApplied[NDSInputID_GuitarGrip_Green].isPressed,
							  this->_clientInputApplied[NDSInputID_GuitarGrip_Red].isPressed,
							  this->_clientInputApplied[NDSInputID_GuitarGrip_Yellow].isPressed,
							  this->_clientInputApplied[NDSInputID_GuitarGrip_Blue].isPressed);
			break;
			
		case NDS_SLOT2_EASYPIANO:
			piano_setKey(this->_clientInputApplied[NDSInputID_Piano_C].isPressed,
						 this->_clientInputApplied[NDSInputID_Piano_CSharp].isPressed,
						 this->_clientInputApplied[NDSInputID_Piano_D].isPressed,
						 this->_clientInputApplied[NDSInputID_Piano_DSharp].isPressed,
						 this->_clientInputApplied[NDSInputID_Piano_E].isPressed,
						 this->_clientInputApplied[NDSInputID_Piano_F].isPressed,
						 this->_clientInputApplied[NDSInputID_Piano_FSharp].isPressed,
						 this->_clientInputApplied[NDSInputID_Piano_G].isPressed,
						 this->_clientInputApplied[NDSInputID_Piano_GSharp].isPressed,
						 this->_clientInputApplied[NDSInputID_Piano_A].isPressed,
						 this->_clientInputApplied[NDSInputID_Piano_ASharp].isPressed,
						 this->_clientInputApplied[NDSInputID_Piano_B].isPressed,
						 this->_clientInputApplied[NDSInputID_Piano_HighC].isPressed);
			break;
			
		case NDS_SLOT2_PADDLE:
			Paddle_SetValue((uint16_t)this->_paddleValueApplied);
			break;
			
		default:
			break;
	}
	
	if (movieMode == MOVIEMODE_RECORD)
	{
		FCEUMOV_HandleRecording();
	}
}

bool ClientInputHandler::IsHardwareMicAvailable()
{
	// Do nothing. This is implementation-dependent.
	return false;
}

void ClientInputHandler::ResetHardwareMic()
{
	this->ClearAverageMicLevel();
	this->ReportAverageMicLevel();
	
	this->_hardwareMicSampleGenerator->resetSamples();
}

uint8_t ClientInputHandler::HandleMicSampleRead()
{
	uint8_t theSample = MIC_NULL_SAMPLE_VALUE;
	AudioGenerator *sampleGenerator = (this->GetClientSoftwareMicStateApplied()) ? this->GetClientSoftwareMicSampleGeneratorApplied() : this->_hardwareMicSampleGenerator;
	
	theSample = sampleGenerator->generateSample();
	
	this->AddSampleToAverageMicLevel(theSample);
	return theSample;
}

void ClientInputHandler::ReportAverageMicLevel()
{
	// Do nothing. This is implementation-dependent.
	// This method mainly exists for implementations to do some stuff during the
	// emulation loop.
}

bool ClientInputHandler::GetHardwareMicMute()
{
	return this->_isHardwareMicMuted;
}

void ClientInputHandler::SetHardwareMicMute(bool muteState)
{	
	this->_isHardwareMicMuted = muteState;
	
	if (muteState)
	{
		this->_hardwareMicSampleGenerator->resetSamples();
		this->ClearAverageMicLevel();
	}
}

bool ClientInputHandler::GetHardwareMicPause()
{
	return this->_isHardwareMicPaused;
}

void ClientInputHandler::SetHardwareMicPause(bool pauseState)
{
	this->_isHardwareMicPaused = pauseState;
}

float ClientInputHandler::GetHardwareMicNormalizedGain()
{
	// Do nothing. This is implementation-dependent.
	// The default value is 0.0f, which represents 0.0% of the hardware device's pickup sensitivity.
	return 0.0f;
}

void ClientInputHandler::SetHardwareMicGainAsNormalized(float normalizedGain)
{
	// Do nothing. This is implementation-dependent.
}
