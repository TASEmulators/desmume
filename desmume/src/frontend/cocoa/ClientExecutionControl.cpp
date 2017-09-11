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

#include <mach/mach.h>
#include <mach/mach_time.h>

#include "NDSSystem.h"
#include "GPU.h"
#include "movie.h"
#include "rtc.h"
#include "slot2.h"

#include "audiosamplegenerator.h"
#include "ClientExecutionControl.h"


ClientExecutionControl::ClientExecutionControl()
{
	_nullSampleGenerator = new NullGenerator;
	_internalNoiseGenerator = new InternalNoiseGenerator;
	_whiteNoiseGenerator = new WhiteNoiseGenerator;
	_sineWaveGenerator = new SineWaveGenerator(250.0, MIC_SAMPLE_RATE);
	_selectedAudioFileGenerator = NULL;
	
	_newSettingsPendingOnReset = true;
	_newSettingsPendingOnExecutionLoopStart = true;
	_newSettingsPendingOnNDSExec = true;
	
	_enableAutohold = false;
	
	_touchLocXPending     = _touchLocXProcessing     = _touchLocXApplied     = 0;
	_touchLocYPending     = _touchLocYProcessing     = _touchLocYApplied     = 0;
	_touchPressurePending = _touchPressureProcessing = _touchPressureApplied = 50;
	_paddleValuePending   = _paddleValueProcessing   = _paddleValueApplied   = 0;
	_paddleAdjustPending  = _paddleAdjustProcessing  = _paddleAdjustApplied  = 0;
	
	_softwareMicSampleGeneratorPending = _softwareMicSampleGeneratorProcessing = _softwareMicSampleGeneratorApplied = _nullSampleGenerator;
	
	_needResetFramesToSkip = false;
	
	_frameTime = 0.0;
	_framesToSkip = 0;
	_prevExecBehavior = ExecutionBehavior_Pause;
	
	_settingsPending.cpuEngineID						= CPUEmulationEngineID_Interpreter;
	_settingsPending.JITMaxBlockSize					= 12;
	_settingsPending.slot1DeviceType					= NDS_SLOT1_RETAIL_AUTO;
	_settingsPending.filePathARM9BIOS					= std::string();
	_settingsPending.filePathARM7BIOS					= std::string();
	_settingsPending.filePathFirmware					= std::string();
	_settingsPending.filePathSlot1R4					= std::string();
	_settingsPending.cpuEmulationEngineName				= "Interpreter";
	_settingsPending.slot1DeviceName					= "Uninitialized";
	
	_settingsPending.enableAdvancedBusLevelTiming		= true;
	_settingsPending.enableRigorous3DRenderingTiming	= false;
	_settingsPending.enableGameSpecificHacks			= true;
	_settingsPending.enableExternalBIOS					= false;
	_settingsPending.enableBIOSInterrupts				= false;
	_settingsPending.enableBIOSPatchDelayLoop			= false;
	_settingsPending.enableExternalFirmware				= false;
	_settingsPending.enableFirmwareBoot					= false;
	_settingsPending.enableDebugConsole					= false;
	_settingsPending.enableEnsataEmulation				= false;
	
	_settingsPending.enableExecutionSpeedLimiter		= true;
	_settingsPending.executionSpeed						= SPEED_SCALAR_NORMAL;
	
	_settingsPending.enableFrameSkip					= true;
	_settingsPending.frameJumpRelativeTarget			= 60;
	_settingsPending.frameJumpTarget					= 60;
	
	_settingsPending.execBehavior						= ExecutionBehavior_Pause;
	_settingsPending.jumpBehavior						= FrameJumpBehavior_Forward;
	
	_settingsApplied = _settingsPending;
	_settingsApplied.filePathARM9BIOS					= _settingsPending.filePathARM9BIOS;
	_settingsApplied.filePathARM7BIOS					= _settingsPending.filePathARM7BIOS;
	_settingsApplied.filePathFirmware					= _settingsPending.filePathFirmware;
	_settingsApplied.filePathSlot1R4					= _settingsPending.filePathSlot1R4;
	_settingsApplied.cpuEmulationEngineName				= _settingsPending.cpuEmulationEngineName;
	_settingsApplied.slot1DeviceName					= _settingsPending.slot1DeviceName;
	
	_ndsFrameInfo.clear();
	_ndsFrameInfo.cpuEmulationEngineName				= _settingsPending.cpuEmulationEngineName;
	_ndsFrameInfo.slot1DeviceName						= _settingsPending.slot1DeviceName;
	
	_cpuEmulationEngineNameOut							= _ndsFrameInfo.cpuEmulationEngineName;
	_slot1DeviceNameOut									= _ndsFrameInfo.slot1DeviceName;
	
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
	
	pthread_mutex_init(&_mutexSettingsPendingOnReset, NULL);
	pthread_mutex_init(&_mutexSettingsPendingOnExecutionLoopStart, NULL);
	pthread_mutex_init(&_mutexSettingsPendingOnNDSExec, NULL);
	pthread_mutex_init(&_mutexInputsPending, NULL);
	pthread_mutex_init(&_mutexOutputPostNDSExec, NULL);
}

ClientExecutionControl::~ClientExecutionControl()
{
	pthread_mutex_destroy(&this->_mutexSettingsPendingOnReset);
	pthread_mutex_destroy(&this->_mutexSettingsPendingOnExecutionLoopStart);
	pthread_mutex_destroy(&this->_mutexSettingsPendingOnNDSExec);
	pthread_mutex_destroy(&this->_mutexInputsPending);
	pthread_mutex_destroy(&this->_mutexOutputPostNDSExec);
	
	delete this->_nullSampleGenerator;
	delete this->_internalNoiseGenerator;
	delete this->_whiteNoiseGenerator;
	delete this->_sineWaveGenerator;
}

CPUEmulationEngineID ClientExecutionControl::GetCPUEmulationEngineID()
{
	pthread_mutex_lock(&this->_mutexSettingsPendingOnReset);
	const CPUEmulationEngineID engineID = this->_settingsPending.cpuEngineID;
	pthread_mutex_unlock(&this->_mutexSettingsPendingOnReset);
	
	return engineID;
}

const char* ClientExecutionControl::GetCPUEmulationEngineName()
{
	pthread_mutex_lock(&this->_mutexSettingsPendingOnReset);
	this->_cpuEmulationEngineNameOut = this->_settingsPending.cpuEmulationEngineName;
	pthread_mutex_unlock(&this->_mutexSettingsPendingOnReset);
	
	return this->_cpuEmulationEngineNameOut.c_str();
}

void ClientExecutionControl::SetCPUEmulationEngineByID(CPUEmulationEngineID engineID)
{
#if !defined(__i386__) && !defined(__x86_64__)
	engineID = CPUEmulationEngineID_Interpreter;
#endif
	
	pthread_mutex_lock(&this->_mutexSettingsPendingOnReset);
	this->_settingsPending.cpuEngineID = engineID;
	
	switch (engineID)
	{
		case CPUEmulationEngineID_Interpreter:
			this->_settingsPending.cpuEmulationEngineName = "Interpreter";
			break;
			
		case CPUEmulationEngineID_DynamicRecompiler:
			this->_settingsPending.cpuEmulationEngineName = "Dynamic Recompiler";
			break;
			
		default:
			this->_settingsPending.cpuEmulationEngineName = "Unknown Engine";
			break;
	}
	
	this->_newSettingsPendingOnReset = true;
	pthread_mutex_unlock(&this->_mutexSettingsPendingOnReset);
}

uint8_t ClientExecutionControl::GetJITMaxBlockSize()
{
	pthread_mutex_lock(&this->_mutexSettingsPendingOnReset);
	const uint8_t blockSize = this->_settingsPending.JITMaxBlockSize;
	pthread_mutex_unlock(&this->_mutexSettingsPendingOnReset);
	
	return blockSize;
}

void ClientExecutionControl::SetJITMaxBlockSize(uint8_t blockSize)
{
	if (blockSize == 0)
	{
		blockSize = 1;
	}
	else if (blockSize > 100)
	{
		blockSize = 100;
	}
	
	pthread_mutex_lock(&this->_mutexSettingsPendingOnReset);
	this->_settingsPending.JITMaxBlockSize = blockSize;
	
	this->_newSettingsPendingOnReset = true;
	pthread_mutex_unlock(&this->_mutexSettingsPendingOnReset);
}

NDS_SLOT1_TYPE ClientExecutionControl::GetSlot1DeviceType()
{
	pthread_mutex_lock(&this->_mutexSettingsPendingOnReset);
	const NDS_SLOT1_TYPE type = this->_settingsPending.slot1DeviceType;
	pthread_mutex_unlock(&this->_mutexSettingsPendingOnReset);
	
	return type;
}

const char* ClientExecutionControl::GetSlot1DeviceName()
{
	pthread_mutex_lock(&this->_mutexSettingsPendingOnReset);
	this->_slot1DeviceNameOut = this->_settingsPending.slot1DeviceName;
	pthread_mutex_unlock(&this->_mutexSettingsPendingOnReset);
	
	return this->_slot1DeviceNameOut.c_str();
}

void ClientExecutionControl::SetSlot1DeviceByType(NDS_SLOT1_TYPE type)
{
	pthread_mutex_lock(&this->_mutexSettingsPendingOnReset);
	this->_settingsPending.slot1DeviceType = type;
	
	if ( (type < 0) || type >= (NDS_SLOT1_COUNT) )
	{
		this->_settingsPending.slot1DeviceName = "Uninitialized";
	}
	else
	{
		this->_settingsPending.slot1DeviceName = slot1_List[type]->info()->name();
	}
	
	this->_newSettingsPendingOnReset = true;
	pthread_mutex_unlock(&this->_mutexSettingsPendingOnReset);
}

const char* ClientExecutionControl::GetARM9ImagePath()
{
	pthread_mutex_lock(&this->_mutexSettingsPendingOnReset);
	const char *filePath = this->_settingsPending.filePathARM9BIOS.c_str();
	pthread_mutex_unlock(&this->_mutexSettingsPendingOnReset);
	
	return filePath;
}

void ClientExecutionControl::SetARM9ImagePath(const char *filePath)
{
	pthread_mutex_lock(&this->_mutexSettingsPendingOnReset);
	
	if (filePath == NULL)
	{
		this->_settingsPending.filePathARM9BIOS.clear();
	}
	else
	{
		this->_settingsPending.filePathARM9BIOS = std::string(filePath, sizeof(CommonSettings.ARM9BIOS));
	}
	
	this->_newSettingsPendingOnReset = true;
	pthread_mutex_unlock(&this->_mutexSettingsPendingOnReset);
}

const char* ClientExecutionControl::GetARM7ImagePath()
{
	pthread_mutex_lock(&this->_mutexSettingsPendingOnReset);
	const char *filePath = this->_settingsPending.filePathARM7BIOS.c_str();
	pthread_mutex_unlock(&this->_mutexSettingsPendingOnReset);
	
	return filePath;
}

void ClientExecutionControl::SetARM7ImagePath(const char *filePath)
{
	pthread_mutex_lock(&this->_mutexSettingsPendingOnReset);
	
	if (filePath == NULL)
	{
		this->_settingsPending.filePathARM7BIOS.clear();
	}
	else
	{
		this->_settingsPending.filePathARM7BIOS = std::string(filePath, sizeof(CommonSettings.ARM7BIOS));
	}
	
	this->_newSettingsPendingOnReset = true;
	pthread_mutex_unlock(&this->_mutexSettingsPendingOnReset);
}

const char* ClientExecutionControl::GetFirmwareImagePath()
{
	pthread_mutex_lock(&this->_mutexSettingsPendingOnReset);
	const char *filePath = this->_settingsPending.filePathFirmware.c_str();
	pthread_mutex_unlock(&this->_mutexSettingsPendingOnReset);
	
	return filePath;
}

void ClientExecutionControl::SetFirmwareImagePath(const char *filePath)
{
	pthread_mutex_lock(&this->_mutexSettingsPendingOnReset);
	
	if (filePath == NULL)
	{
		this->_settingsPending.filePathFirmware.clear();
	}
	else
	{
		this->_settingsPending.filePathFirmware = std::string(filePath, sizeof(CommonSettings.Firmware));
	}
	
	this->_newSettingsPendingOnReset = true;
	pthread_mutex_unlock(&this->_mutexSettingsPendingOnReset);
}

const char* ClientExecutionControl::GetSlot1R4Path()
{
	pthread_mutex_lock(&this->_mutexSettingsPendingOnReset);
	const char *filePath = this->_settingsPending.filePathSlot1R4.c_str();
	pthread_mutex_unlock(&this->_mutexSettingsPendingOnReset);
	
	return filePath;
}

void ClientExecutionControl::SetSlot1R4Path(const char *filePath)
{
	pthread_mutex_lock(&this->_mutexSettingsPendingOnReset);
	
	if (filePath == NULL)
	{
		this->_settingsPending.filePathSlot1R4.clear();
	}
	else
	{
		this->_settingsPending.filePathSlot1R4 = std::string(filePath);
	}
	
	this->_newSettingsPendingOnReset = true;
	pthread_mutex_unlock(&this->_mutexSettingsPendingOnReset);
}

bool ClientExecutionControl::GetEnableAdvancedBusLevelTiming()
{
	pthread_mutex_lock(&this->_mutexSettingsPendingOnNDSExec);
	const bool enable = this->_settingsPending.enableAdvancedBusLevelTiming;
	pthread_mutex_unlock(&this->_mutexSettingsPendingOnNDSExec);
	
	return enable;
}

void ClientExecutionControl::SetEnableAdvancedBusLevelTiming(bool enable)
{
	pthread_mutex_lock(&this->_mutexSettingsPendingOnNDSExec);
	this->_settingsPending.enableAdvancedBusLevelTiming = enable;
	
	this->_newSettingsPendingOnNDSExec = true;
	pthread_mutex_unlock(&this->_mutexSettingsPendingOnNDSExec);
}

bool ClientExecutionControl::GetEnableRigorous3DRenderingTiming()
{
	pthread_mutex_lock(&this->_mutexSettingsPendingOnNDSExec);
	const bool enable = this->_settingsPending.enableRigorous3DRenderingTiming;
	pthread_mutex_unlock(&this->_mutexSettingsPendingOnNDSExec);
	
	return enable;
}

void ClientExecutionControl::SetEnableRigorous3DRenderingTiming(bool enable)
{
	pthread_mutex_lock(&this->_mutexSettingsPendingOnNDSExec);
	this->_settingsPending.enableRigorous3DRenderingTiming = enable;
	
	this->_newSettingsPendingOnNDSExec = true;
	pthread_mutex_unlock(&this->_mutexSettingsPendingOnNDSExec);
}

bool ClientExecutionControl::GetEnableGameSpecificHacks()
{
	pthread_mutex_lock(&this->_mutexSettingsPendingOnNDSExec);
	const bool enable = this->_settingsPending.enableGameSpecificHacks;
	pthread_mutex_unlock(&this->_mutexSettingsPendingOnNDSExec);
	
	return enable;
}

void ClientExecutionControl::SetEnableGameSpecificHacks(bool enable)
{
	pthread_mutex_lock(&this->_mutexSettingsPendingOnNDSExec);
	this->_settingsPending.enableGameSpecificHacks = enable;
	
	this->_newSettingsPendingOnNDSExec = true;
	pthread_mutex_unlock(&this->_mutexSettingsPendingOnNDSExec);
}

bool ClientExecutionControl::GetEnableExternalBIOS()
{
	pthread_mutex_lock(&this->_mutexSettingsPendingOnNDSExec);
	const bool enable = this->_settingsPending.enableExternalBIOS;
	pthread_mutex_unlock(&this->_mutexSettingsPendingOnNDSExec);
	
	return enable;
}

void ClientExecutionControl::SetEnableExternalBIOS(bool enable)
{
	pthread_mutex_lock(&this->_mutexSettingsPendingOnNDSExec);
	this->_settingsPending.enableExternalBIOS = enable;
	
	this->_newSettingsPendingOnNDSExec = true;
	pthread_mutex_unlock(&this->_mutexSettingsPendingOnNDSExec);
}

bool ClientExecutionControl::GetEnableBIOSInterrupts()
{
	pthread_mutex_lock(&this->_mutexSettingsPendingOnNDSExec);
	const bool enable = this->_settingsPending.enableBIOSInterrupts;
	pthread_mutex_unlock(&this->_mutexSettingsPendingOnNDSExec);
	
	return enable;
}

void ClientExecutionControl::SetEnableBIOSInterrupts(bool enable)
{
	pthread_mutex_lock(&this->_mutexSettingsPendingOnNDSExec);
	this->_settingsPending.enableBIOSInterrupts = enable;
	
	this->_newSettingsPendingOnNDSExec = true;
	pthread_mutex_unlock(&this->_mutexSettingsPendingOnNDSExec);
}

bool ClientExecutionControl::GetEnableBIOSPatchDelayLoop()
{
	pthread_mutex_lock(&this->_mutexSettingsPendingOnNDSExec);
	const bool enable = this->_settingsPending.enableBIOSPatchDelayLoop;
	pthread_mutex_unlock(&this->_mutexSettingsPendingOnNDSExec);
	
	return enable;
}

void ClientExecutionControl::SetEnableBIOSPatchDelayLoop(bool enable)
{
	pthread_mutex_lock(&this->_mutexSettingsPendingOnNDSExec);
	this->_settingsPending.enableBIOSPatchDelayLoop = enable;
	
	this->_newSettingsPendingOnNDSExec = true;
	pthread_mutex_unlock(&this->_mutexSettingsPendingOnNDSExec);
}

bool ClientExecutionControl::GetEnableExternalFirmware()
{
	pthread_mutex_lock(&this->_mutexSettingsPendingOnNDSExec);
	const bool enable = this->_settingsPending.enableExternalFirmware;
	pthread_mutex_unlock(&this->_mutexSettingsPendingOnNDSExec);
	
	return enable;
}

void ClientExecutionControl::SetEnableExternalFirmware(bool enable)
{
	pthread_mutex_lock(&this->_mutexSettingsPendingOnNDSExec);
	this->_settingsPending.enableExternalFirmware = enable;
	
	this->_newSettingsPendingOnNDSExec = true;
	pthread_mutex_unlock(&this->_mutexSettingsPendingOnNDSExec);
}

bool ClientExecutionControl::GetEnableFirmwareBoot()
{
	pthread_mutex_lock(&this->_mutexSettingsPendingOnNDSExec);
	const bool enable = this->_settingsPending.enableFirmwareBoot;
	pthread_mutex_unlock(&this->_mutexSettingsPendingOnNDSExec);
	
	return enable;
}

void ClientExecutionControl::SetEnableFirmwareBoot(bool enable)
{
	pthread_mutex_lock(&this->_mutexSettingsPendingOnNDSExec);
	this->_settingsPending.enableFirmwareBoot = enable;
	
	this->_newSettingsPendingOnNDSExec = true;
	pthread_mutex_unlock(&this->_mutexSettingsPendingOnNDSExec);
}

bool ClientExecutionControl::GetEnableDebugConsole()
{
	pthread_mutex_lock(&this->_mutexSettingsPendingOnNDSExec);
	const bool enable = this->_settingsPending.enableDebugConsole;
	pthread_mutex_unlock(&this->_mutexSettingsPendingOnNDSExec);
	
	return enable;
}

void ClientExecutionControl::SetEnableDebugConsole(bool enable)
{
	pthread_mutex_lock(&this->_mutexSettingsPendingOnNDSExec);
	this->_settingsPending.enableDebugConsole = enable;
	
	this->_newSettingsPendingOnNDSExec = true;
	pthread_mutex_unlock(&this->_mutexSettingsPendingOnNDSExec);
}

bool ClientExecutionControl::GetEnableEnsataEmulation()
{
	pthread_mutex_lock(&this->_mutexSettingsPendingOnNDSExec);
	const bool enable = this->_settingsPending.enableEnsataEmulation;
	pthread_mutex_unlock(&this->_mutexSettingsPendingOnNDSExec);
	
	return enable;
}

void ClientExecutionControl::SetEnableEnsataEmulation(bool enable)
{
	pthread_mutex_lock(&this->_mutexSettingsPendingOnNDSExec);
	this->_settingsPending.enableEnsataEmulation = enable;
	
	this->_newSettingsPendingOnNDSExec = true;
	pthread_mutex_unlock(&this->_mutexSettingsPendingOnNDSExec);
}

bool ClientExecutionControl::GetEnableCheats()
{
	pthread_mutex_lock(&this->_mutexSettingsPendingOnNDSExec);
	const bool enable = this->_settingsPending.enableCheats;
	pthread_mutex_unlock(&this->_mutexSettingsPendingOnNDSExec);
	
	return enable;
}

void ClientExecutionControl::SetEnableCheats(bool enable)
{
	pthread_mutex_lock(&this->_mutexSettingsPendingOnNDSExec);
	this->_settingsPending.enableCheats = enable;
	
	this->_newSettingsPendingOnNDSExec = true;
	pthread_mutex_unlock(&this->_mutexSettingsPendingOnNDSExec);
}

bool ClientExecutionControl::GetEnableSpeedLimiter()
{
	pthread_mutex_lock(&this->_mutexSettingsPendingOnExecutionLoopStart);
	const bool enable = this->_settingsPending.enableExecutionSpeedLimiter;
	pthread_mutex_unlock(&this->_mutexSettingsPendingOnExecutionLoopStart);
	
	return enable;
}

void ClientExecutionControl::SetEnableSpeedLimiter(bool enable)
{
	pthread_mutex_lock(&this->_mutexSettingsPendingOnExecutionLoopStart);
	this->_settingsPending.enableExecutionSpeedLimiter = enable;
	
	this->_newSettingsPendingOnExecutionLoopStart = true;
	pthread_mutex_unlock(&this->_mutexSettingsPendingOnExecutionLoopStart);
}

double ClientExecutionControl::GetExecutionSpeed()
{
	pthread_mutex_lock(&this->_mutexSettingsPendingOnExecutionLoopStart);
	const double speedScalar = this->_settingsPending.executionSpeed;
	pthread_mutex_unlock(&this->_mutexSettingsPendingOnExecutionLoopStart);
	
	return speedScalar;
}

void ClientExecutionControl::SetExecutionSpeed(double speedScalar)
{
	pthread_mutex_lock(&this->_mutexSettingsPendingOnExecutionLoopStart);
	this->_settingsPending.executionSpeed = speedScalar;
	
	this->_newSettingsPendingOnExecutionLoopStart = true;
	pthread_mutex_unlock(&this->_mutexSettingsPendingOnExecutionLoopStart);
}

bool ClientExecutionControl::GetEnableFrameSkip()
{
	pthread_mutex_lock(&this->_mutexSettingsPendingOnExecutionLoopStart);
	const bool enable = this->_settingsPending.enableFrameSkip;
	pthread_mutex_unlock(&this->_mutexSettingsPendingOnExecutionLoopStart);
	
	return enable;
}

bool ClientExecutionControl::GetEnableFrameSkipApplied()
{
	return this->_settingsApplied.enableFrameSkip;
}

void ClientExecutionControl::SetEnableFrameSkip(bool enable)
{
	pthread_mutex_lock(&this->_mutexSettingsPendingOnNDSExec);
	
	if (this->_settingsPending.enableFrameSkip != enable)
	{
		this->_settingsPending.enableFrameSkip = enable;
		
		this->_needResetFramesToSkip = true;
		this->_newSettingsPendingOnNDSExec = true;
	}
	
	pthread_mutex_unlock(&this->_mutexSettingsPendingOnNDSExec);
}

uint8_t ClientExecutionControl::GetFramesToSkip()
{
	return this->_framesToSkip;
}

void ClientExecutionControl::SetFramesToSkip(uint8_t numFrames)
{
	this->_framesToSkip = numFrames;
}

void ClientExecutionControl::ResetFramesToSkip()
{
	pthread_mutex_lock(&this->_mutexSettingsPendingOnNDSExec);
	this->_needResetFramesToSkip = true;
	
	this->_newSettingsPendingOnNDSExec = true;
	pthread_mutex_unlock(&this->_mutexSettingsPendingOnNDSExec);
}

uint64_t ClientExecutionControl::GetFrameJumpRelativeTarget()
{
	pthread_mutex_lock(&this->_mutexSettingsPendingOnExecutionLoopStart);
	const uint64_t jumpTarget = this->_settingsPending.frameJumpRelativeTarget;
	pthread_mutex_unlock(&this->_mutexSettingsPendingOnExecutionLoopStart);
	
	return jumpTarget;
}

void ClientExecutionControl::SetFrameJumpRelativeTarget(uint64_t newRelativeTarget)
{
	pthread_mutex_lock(&this->_mutexSettingsPendingOnExecutionLoopStart);
	this->_settingsPending.frameJumpRelativeTarget = newRelativeTarget;
	
	this->_newSettingsPendingOnExecutionLoopStart = true;
	pthread_mutex_unlock(&this->_mutexSettingsPendingOnExecutionLoopStart);
}

uint64_t ClientExecutionControl::GetFrameJumpTarget()
{
	pthread_mutex_lock(&this->_mutexSettingsPendingOnExecutionLoopStart);
	const uint64_t jumpTarget = this->_settingsPending.frameJumpTarget;
	pthread_mutex_unlock(&this->_mutexSettingsPendingOnExecutionLoopStart);
	
	return jumpTarget;
}

uint64_t ClientExecutionControl::GetFrameJumpTargetApplied()
{
	return this->_settingsApplied.frameJumpTarget;
}

void ClientExecutionControl::SetFrameJumpTarget(uint64_t newJumpTarget)
{
	pthread_mutex_lock(&this->_mutexSettingsPendingOnExecutionLoopStart);
	this->_settingsPending.frameJumpTarget = newJumpTarget;
	
	this->_newSettingsPendingOnExecutionLoopStart = true;
	pthread_mutex_unlock(&this->_mutexSettingsPendingOnExecutionLoopStart);
}

ExecutionBehavior ClientExecutionControl::GetPreviousExecutionBehavior()
{
	pthread_mutex_lock(&this->_mutexSettingsPendingOnExecutionLoopStart);
	const ExecutionBehavior behavior = this->_prevExecBehavior;
	pthread_mutex_unlock(&this->_mutexSettingsPendingOnExecutionLoopStart);
	
	return behavior;
}

ExecutionBehavior ClientExecutionControl::GetExecutionBehavior()
{
	pthread_mutex_lock(&this->_mutexSettingsPendingOnExecutionLoopStart);
	const ExecutionBehavior behavior = this->_settingsPending.execBehavior;
	pthread_mutex_unlock(&this->_mutexSettingsPendingOnExecutionLoopStart);
	
	return behavior;
}

ExecutionBehavior ClientExecutionControl::GetExecutionBehaviorApplied()
{
	return this->_settingsApplied.execBehavior;
}

void ClientExecutionControl::SetExecutionBehavior(ExecutionBehavior newBehavior)
{
	pthread_mutex_lock(&this->_mutexSettingsPendingOnExecutionLoopStart);
	this->_settingsPending.execBehavior = newBehavior;
	
	this->_newSettingsPendingOnExecutionLoopStart = true;
	pthread_mutex_unlock(&this->_mutexSettingsPendingOnExecutionLoopStart);
}

FrameJumpBehavior ClientExecutionControl::GetFrameJumpBehavior()
{
	pthread_mutex_lock(&this->_mutexSettingsPendingOnExecutionLoopStart);
	const FrameJumpBehavior jumpBehavior = this->_settingsPending.jumpBehavior;
	pthread_mutex_unlock(&this->_mutexSettingsPendingOnExecutionLoopStart);
	
	return jumpBehavior;
}

void ClientExecutionControl::SetFrameJumpBehavior(FrameJumpBehavior newBehavior)
{
	pthread_mutex_lock(&this->_mutexSettingsPendingOnExecutionLoopStart);
	this->_settingsPending.jumpBehavior = newBehavior;
	
	this->_newSettingsPendingOnExecutionLoopStart = true;
	pthread_mutex_unlock(&this->_mutexSettingsPendingOnExecutionLoopStart);
}

void ClientExecutionControl::ApplySettingsOnReset()
{
	pthread_mutex_lock(&this->_mutexSettingsPendingOnReset);
	
	if (this->_newSettingsPendingOnReset)
	{
		this->_settingsApplied.cpuEngineID				= this->_settingsPending.cpuEngineID;
		this->_settingsApplied.JITMaxBlockSize			= this->_settingsPending.JITMaxBlockSize;
		
		this->_settingsApplied.filePathARM9BIOS			= this->_settingsPending.filePathARM9BIOS;
		this->_settingsApplied.filePathARM7BIOS			= this->_settingsPending.filePathARM7BIOS;
		this->_settingsApplied.filePathFirmware			= this->_settingsPending.filePathFirmware;
		this->_settingsApplied.filePathSlot1R4			= this->_settingsPending.filePathSlot1R4;
		
		this->_settingsApplied.cpuEmulationEngineName	= this->_settingsPending.cpuEmulationEngineName;
		this->_settingsApplied.slot1DeviceName			= this->_settingsPending.slot1DeviceName;
		this->_ndsFrameInfo.cpuEmulationEngineName		= this->_settingsApplied.cpuEmulationEngineName;
		this->_ndsFrameInfo.slot1DeviceName				= this->_settingsApplied.slot1DeviceName;
		
		const bool didChangeSlot1Type = (this->_settingsApplied.slot1DeviceType != this->_settingsPending.slot1DeviceType);
		if (didChangeSlot1Type)
		{
			this->_settingsApplied.slot1DeviceType	= this->_settingsPending.slot1DeviceType;
		}
		
		this->_newSettingsPendingOnReset = false;
		pthread_mutex_unlock(&this->_mutexSettingsPendingOnReset);
		
		CommonSettings.use_jit				= (this->_settingsApplied.cpuEngineID == CPUEmulationEngineID_DynamicRecompiler);
		CommonSettings.jit_max_block_size	= this->_settingsApplied.JITMaxBlockSize;
		
		if (this->_settingsApplied.filePathARM9BIOS.length() == 0)
		{
			memset(CommonSettings.ARM9BIOS, 0, sizeof(CommonSettings.ARM9BIOS));
		}
		else
		{
			strlcpy(CommonSettings.ARM9BIOS, this->_settingsApplied.filePathARM9BIOS.c_str(), sizeof(CommonSettings.ARM9BIOS));
		}
		
		if (this->_settingsApplied.filePathARM7BIOS.length() == 0)
		{
			memset(CommonSettings.ARM7BIOS, 0, sizeof(CommonSettings.ARM7BIOS));
		}
		else
		{
			strlcpy(CommonSettings.ARM7BIOS, this->_settingsApplied.filePathARM7BIOS.c_str(), sizeof(CommonSettings.ARM7BIOS));
		}
		
		if (this->_settingsApplied.filePathFirmware.length() == 0)
		{
			memset(CommonSettings.Firmware, 0, sizeof(CommonSettings.Firmware));
		}
		else
		{
			strlcpy(CommonSettings.Firmware, this->_settingsApplied.filePathFirmware.c_str(), sizeof(CommonSettings.Firmware));
		}
		
		if (this->_settingsApplied.filePathSlot1R4.length() > 0)
		{
			slot1_SetFatDir(this->_settingsApplied.filePathSlot1R4);
		}
		
		if (didChangeSlot1Type)
		{
			slot1_Change(this->_settingsApplied.slot1DeviceType);
		}
	}
	else
	{
		pthread_mutex_unlock(&this->_mutexSettingsPendingOnReset);
	}
}

void ClientExecutionControl::ApplySettingsOnExecutionLoopStart()
{
	pthread_mutex_lock(&this->_mutexSettingsPendingOnExecutionLoopStart);
	
	if (this->_newSettingsPendingOnExecutionLoopStart)
	{
		const double speedScalar = (this->_settingsPending.executionSpeed > SPEED_SCALAR_MIN) ? this->_settingsPending.executionSpeed : SPEED_SCALAR_MIN;
		this->_settingsApplied.enableExecutionSpeedLimiter	= this->_settingsPending.enableExecutionSpeedLimiter;
		this->_settingsApplied.executionSpeed				= speedScalar;
		
		this->_settingsApplied.jumpBehavior					= this->_settingsPending.jumpBehavior;
		this->_settingsApplied.frameJumpRelativeTarget		= this->_settingsPending.frameJumpRelativeTarget;
		
		switch (this->_settingsApplied.jumpBehavior)
		{
			case FrameJumpBehavior_Forward:
				this->_settingsApplied.frameJumpTarget		= this->_ndsFrameInfo.frameIndex + this->_settingsApplied.frameJumpRelativeTarget;
				break;
				
			case FrameJumpBehavior_NextMarker:
				// TODO: Support frame markers in replays.
				break;
				
			case FrameJumpBehavior_ToFrame:
			default:
				this->_settingsApplied.frameJumpTarget		= this->_settingsPending.frameJumpTarget;
				break;
		}
		
		const bool needBehaviorChange = (this->_settingsApplied.execBehavior != this->_settingsPending.execBehavior);
		if (needBehaviorChange)
		{
			if ( (this->_settingsApplied.execBehavior == ExecutionBehavior_Run) || (this->_settingsApplied.execBehavior == ExecutionBehavior_Pause) )
			{
				this->_prevExecBehavior = this->_settingsApplied.execBehavior;
			}
			
			this->_settingsApplied.execBehavior = this->_settingsPending.execBehavior;
		}
		
		this->_newSettingsPendingOnExecutionLoopStart = false;
		pthread_mutex_unlock(&this->_mutexSettingsPendingOnExecutionLoopStart);
		
		if (this->_settingsApplied.enableExecutionSpeedLimiter)
		{
			this->_frameTime = this->CalculateFrameAbsoluteTime(1.0/speedScalar);
		}
		else
		{
			this->_frameTime = 0.0;
		}
		
		if (needBehaviorChange)
		{
			this->ResetFramesToSkip();
		}
	}
	else
	{
		pthread_mutex_unlock(&this->_mutexSettingsPendingOnExecutionLoopStart);
	}
}

void ClientExecutionControl::ApplySettingsOnNDSExec()
{
	pthread_mutex_lock(&this->_mutexSettingsPendingOnNDSExec);
	
	if (this->_newSettingsPendingOnNDSExec)
	{
		this->_settingsApplied.enableAdvancedBusLevelTiming		= this->_settingsPending.enableAdvancedBusLevelTiming;
		this->_settingsApplied.enableRigorous3DRenderingTiming	= this->_settingsPending.enableRigorous3DRenderingTiming;
		this->_settingsApplied.enableGameSpecificHacks			= this->_settingsPending.enableGameSpecificHacks;
		this->_settingsApplied.enableExternalBIOS				= this->_settingsPending.enableExternalBIOS;
		this->_settingsApplied.enableBIOSInterrupts				= this->_settingsPending.enableBIOSInterrupts;
		this->_settingsApplied.enableBIOSPatchDelayLoop			= this->_settingsPending.enableBIOSPatchDelayLoop;
		this->_settingsApplied.enableExternalFirmware			= this->_settingsPending.enableExternalFirmware;
		this->_settingsApplied.enableFirmwareBoot				= this->_settingsPending.enableFirmwareBoot;
		this->_settingsApplied.enableDebugConsole				= this->_settingsPending.enableDebugConsole;
		this->_settingsApplied.enableEnsataEmulation			= this->_settingsPending.enableEnsataEmulation;
		
		this->_settingsApplied.enableCheats						= this->_settingsPending.enableCheats;
		
		this->_settingsApplied.enableFrameSkip					= this->_settingsPending.enableFrameSkip;
		
		const bool needResetFramesToSkip = this->_needResetFramesToSkip;
		
		this->_needResetFramesToSkip = false;
		this->_newSettingsPendingOnNDSExec = false;
		pthread_mutex_unlock(&this->_mutexSettingsPendingOnNDSExec);
		
		CommonSettings.advanced_timing			= this->_settingsApplied.enableAdvancedBusLevelTiming;
		CommonSettings.rigorous_timing			= this->_settingsApplied.enableRigorous3DRenderingTiming;
		CommonSettings.UseExtBIOS				= this->_settingsApplied.enableExternalBIOS;
		CommonSettings.SWIFromBIOS				= this->_settingsApplied.enableBIOSInterrupts;
		CommonSettings.PatchSWI3				= this->_settingsApplied.enableBIOSPatchDelayLoop;
		CommonSettings.UseExtFirmware			= this->_settingsApplied.enableExternalFirmware;
		CommonSettings.UseExtFirmwareSettings	= this->_settingsApplied.enableExternalFirmware;
		CommonSettings.BootFromFirmware			= this->_settingsApplied.enableFirmwareBoot;
		CommonSettings.DebugConsole				= this->_settingsApplied.enableDebugConsole;
		CommonSettings.EnsataEmulation			= this->_settingsApplied.enableEnsataEmulation;
		
		CommonSettings.cheatsDisable			= !this->_settingsApplied.enableCheats;
		
		CommonSettings.gamehacks.en				= this->_settingsApplied.enableGameSpecificHacks;
		CommonSettings.gamehacks.apply();
		
		if (needResetFramesToSkip)
		{
			this->_framesToSkip = 0;
			NDS_OmitFrameSkip(2);
		}
	}
	else
	{
		pthread_mutex_unlock(&this->_mutexSettingsPendingOnNDSExec);
	}
}

bool ClientExecutionControl::GetEnableAutohold()
{
	pthread_mutex_lock(&this->_mutexInputsPending);
	const bool enable = this->_enableAutohold;
	pthread_mutex_unlock(&this->_mutexInputsPending);
	
	return enable;
}

void ClientExecutionControl::SetEnableAutohold(bool enable)
{
	pthread_mutex_lock(&this->_mutexInputsPending);
	this->_enableAutohold = enable;
	pthread_mutex_unlock(&this->_mutexInputsPending);
}

void ClientExecutionControl::ClearAutohold()
{
	pthread_mutex_lock(&this->_mutexInputsPending);
	
	for (size_t i = 0; i < NDSInputID_InputCount; i++)
	{
		this->_clientInputPending[i].autohold = false;
	}
	
	pthread_mutex_unlock(&this->_mutexInputsPending);
}

void ClientExecutionControl::SetClientInputStateUsingID(NDSInputID inputID, bool pressedState)
{
	this->SetClientInputStateUsingID(inputID, pressedState, false, 0, 0);
}

void ClientExecutionControl::SetClientInputStateUsingID(NDSInputID inputID, bool pressedState, bool isTurboEnabled, uint32_t turboPattern, uint32_t turboPatternLength)
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
	
	const uint64_t bitValue = (1 << _inputStateBitMap[inputID]);
	this->_ndsFrameInfo.inputStatesPending.value = (this->_clientInputPending[inputID].isPressed) ? this->_ndsFrameInfo.inputStatesPending.value & ~bitValue : this->_ndsFrameInfo.inputStatesPending.value | bitValue;
	
	pthread_mutex_unlock(&this->_mutexInputsPending);
}

void ClientExecutionControl::SetClientTouchState(bool pressedState, uint8_t touchLocX, uint8_t touchLocY, uint8_t touchPressure)
{
	pthread_mutex_lock(&this->_mutexInputsPending);
	
	this->_clientInputPending[NDSInputID_Touch].isPressed = pressedState;
	this->_touchLocXPending = touchLocX;
	this->_touchLocYPending = touchLocY;
	this->_touchPressurePending = touchPressure;
	
	this->_ndsFrameInfo.inputStatesPending.Touch = (this->_clientInputPending[NDSInputID_Touch].isPressed) ? 0 : 1;
	this->_ndsFrameInfo.touchLocXPending         = this->_touchLocXPending;
	this->_ndsFrameInfo.touchLocYPending         = this->_touchLocYPending;
	this->_ndsFrameInfo.touchPressurePending     = this->_touchPressurePending;
	
	pthread_mutex_unlock(&this->_mutexInputsPending);
}

double ClientExecutionControl::GetSineWaveFrequency()
{
	return this->_sineWaveGenerator->getFrequency();
}

void ClientExecutionControl::SetSineWaveFrequency(double freq)
{
	this->_sineWaveGenerator->setFrequency(freq);
}

AudioGenerator* ClientExecutionControl::GetClientSoftwareMicSampleGenerator()
{
	pthread_mutex_lock(&this->_mutexInputsPending);
	AudioGenerator *softwareMicSampleGenerator = this->_softwareMicSampleGeneratorPending;
	pthread_mutex_unlock(&this->_mutexInputsPending);
	
	return softwareMicSampleGenerator;
}

AudioGenerator* ClientExecutionControl::GetClientSoftwareMicSampleGeneratorApplied()
{
	return this->_softwareMicSampleGeneratorApplied;
}

AudioSampleBlockGenerator* ClientExecutionControl::GetClientSelectedAudioFileGenerator()
{
	pthread_mutex_lock(&this->_mutexInputsPending);
	AudioSampleBlockGenerator *selectedAudioFileGenerator = this->_selectedAudioFileGenerator;
	pthread_mutex_unlock(&this->_mutexInputsPending);
	
	return selectedAudioFileGenerator;
}

void ClientExecutionControl::SetClientSelectedAudioFileGenerator(AudioSampleBlockGenerator *selectedAudioFileGenerator)
{
	pthread_mutex_lock(&this->_mutexInputsPending);
	this->_selectedAudioFileGenerator = selectedAudioFileGenerator;
	pthread_mutex_unlock(&this->_mutexInputsPending);
}

bool ClientExecutionControl::GetClientSoftwareMicState()
{
	pthread_mutex_lock(&this->_mutexInputsPending);
	const bool isPressed = this->_clientInputPending[NDSInputID_Microphone].isPressed;
	pthread_mutex_unlock(&this->_mutexInputsPending);
	
	return isPressed;
}

bool ClientExecutionControl::GetClientSoftwareMicStateApplied()
{
	return this->_clientInputApplied[NDSInputID_Microphone].isPressed;
}

void ClientExecutionControl::SetClientSoftwareMicState(bool pressedState, MicrophoneMode micMode)
{
	pthread_mutex_lock(&this->_mutexInputsPending);
	
	this->_clientInputPending[NDSInputID_Microphone].isPressed = pressedState;
	this->_ndsFrameInfo.inputStatesPending.Microphone = (this->_clientInputPending[NDSInputID_Microphone].isPressed) ? 0 : 1;
	
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

int16_t ClientExecutionControl::GetClientPaddleAdjust()
{
	pthread_mutex_lock(&this->_mutexInputsPending);
	const int16_t paddleAdjust = this->_paddleAdjustPending;
	pthread_mutex_unlock(&this->_mutexInputsPending);
	
	return paddleAdjust;
}

void ClientExecutionControl::SetClientPaddleAdjust(int16_t paddleAdjust)
{
	pthread_mutex_lock(&this->_mutexInputsPending);
	
	this->_paddleAdjustPending = paddleAdjust;
	this->_paddleValuePending = this->_paddleValueApplied + paddleAdjust;
	this->_ndsFrameInfo.paddleValuePending  = this->_paddleValuePending;
	this->_ndsFrameInfo.paddleAdjustPending = this->_paddleAdjustPending;
	
	pthread_mutex_unlock(&this->_mutexInputsPending);
}

void ClientExecutionControl::ProcessInputs()
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
		if (this->GetExecutionBehaviorApplied() != ExecutionBehavior_FrameJump)
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

void ClientExecutionControl::ApplyInputs()
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

void ClientExecutionControl::FetchOutputPostNDSExec()
{
	pthread_mutex_lock(&this->_mutexOutputPostNDSExec);
	
	this->_ndsFrameInfo.frameIndex		= currFrameCounter;
	this->_ndsFrameInfo.render3DFPS		= GPU->GetFPSRender3D();
	this->_ndsFrameInfo.lagFrameCount	= TotalLagFrames;
	
	if ((this->_ndsFrameInfo.frameIndex & 0xF) == 0xF)
	{
		NDS_GetCPULoadAverage(this->_ndsFrameInfo.cpuLoadAvgARM9, this->_ndsFrameInfo.cpuLoadAvgARM7);
	}
	
	char *tempBuffer = (char *)calloc(25, sizeof(char));
	rtcGetTimeAsString(tempBuffer);
	this->_ndsFrameInfo.rtcString = tempBuffer;
	free(tempBuffer);
	
	this->_ndsFrameInfo.inputStatesApplied.value              = INPUT_STATES_CLEAR_VALUE;
	this->_ndsFrameInfo.inputStatesApplied.A                  = (this->_clientInputApplied[NDSInputID_A].isPressed)                 ? 0 : 1;
	this->_ndsFrameInfo.inputStatesApplied.B                  = (this->_clientInputApplied[NDSInputID_B].isPressed)                 ? 0 : 1;
	this->_ndsFrameInfo.inputStatesApplied.Select             = (this->_clientInputApplied[NDSInputID_Select].isPressed)            ? 0 : 1;
	this->_ndsFrameInfo.inputStatesApplied.Start              = (this->_clientInputApplied[NDSInputID_Start].isPressed)             ? 0 : 1;
	this->_ndsFrameInfo.inputStatesApplied.Right              = (this->_clientInputApplied[NDSInputID_Right].isPressed)             ? 0 : 1;
	this->_ndsFrameInfo.inputStatesApplied.Left               = (this->_clientInputApplied[NDSInputID_Left].isPressed)              ? 0 : 1;
	this->_ndsFrameInfo.inputStatesApplied.Up                 = (this->_clientInputApplied[NDSInputID_Up].isPressed)                ? 0 : 1;
	this->_ndsFrameInfo.inputStatesApplied.Down               = (this->_clientInputApplied[NDSInputID_Down].isPressed)              ? 0 : 1;
	this->_ndsFrameInfo.inputStatesApplied.R                  = (this->_clientInputApplied[NDSInputID_R].isPressed)                 ? 0 : 1;
	this->_ndsFrameInfo.inputStatesApplied.L                  = (this->_clientInputApplied[NDSInputID_L].isPressed)                 ? 0 : 1;
	this->_ndsFrameInfo.inputStatesApplied.X                  = (this->_clientInputApplied[NDSInputID_X].isPressed)                 ? 0 : 1;
	this->_ndsFrameInfo.inputStatesApplied.Y                  = (this->_clientInputApplied[NDSInputID_Y].isPressed)                 ? 0 : 1;
	this->_ndsFrameInfo.inputStatesApplied.Debug              = (this->_clientInputApplied[NDSInputID_Debug].isPressed)             ? 0 : 1;
	this->_ndsFrameInfo.inputStatesApplied.Touch              = (this->_clientInputApplied[NDSInputID_Touch].isPressed)             ? 0 : 1;
	this->_ndsFrameInfo.inputStatesApplied.Lid                = (this->_clientInputApplied[NDSInputID_Lid].isPressed)               ? 0 : 1;
	this->_ndsFrameInfo.inputStatesApplied.PianoC             = (this->_clientInputApplied[NDSInputID_Piano_C].isPressed)           ? 0 : 1;
	this->_ndsFrameInfo.inputStatesApplied.PianoCSharp        = (this->_clientInputApplied[NDSInputID_Piano_CSharp].isPressed)      ? 0 : 1;
	this->_ndsFrameInfo.inputStatesApplied.PianoD             = (this->_clientInputApplied[NDSInputID_Piano_D].isPressed)           ? 0 : 1;
	this->_ndsFrameInfo.inputStatesApplied.PianoDSharp        = (this->_clientInputApplied[NDSInputID_Piano_DSharp].isPressed)      ? 0 : 1;
	this->_ndsFrameInfo.inputStatesApplied.PianoE             = (this->_clientInputApplied[NDSInputID_Piano_E].isPressed)           ? 0 : 1;
	this->_ndsFrameInfo.inputStatesApplied.PianoF             = (this->_clientInputApplied[NDSInputID_Piano_F].isPressed)           ? 0 : 1;
	this->_ndsFrameInfo.inputStatesApplied.PianoFSharp        = (this->_clientInputApplied[NDSInputID_Piano_FSharp].isPressed)      ? 0 : 1;
	this->_ndsFrameInfo.inputStatesApplied.PianoG             = (this->_clientInputApplied[NDSInputID_Piano_G].isPressed)           ? 0 : 1;
	this->_ndsFrameInfo.inputStatesApplied.PianoGSharp        = (this->_clientInputApplied[NDSInputID_Piano_GSharp].isPressed)      ? 0 : 1;
	this->_ndsFrameInfo.inputStatesApplied.PianoA             = (this->_clientInputApplied[NDSInputID_Piano_A].isPressed)           ? 0 : 1;
	this->_ndsFrameInfo.inputStatesApplied.PianoASharp        = (this->_clientInputApplied[NDSInputID_Piano_ASharp].isPressed)      ? 0 : 1;
	this->_ndsFrameInfo.inputStatesApplied.PianoB             = (this->_clientInputApplied[NDSInputID_Piano_B].isPressed)           ? 0 : 1;
	this->_ndsFrameInfo.inputStatesApplied.PianoHighC         = (this->_clientInputApplied[NDSInputID_Piano_HighC].isPressed)       ? 0 : 1;
	this->_ndsFrameInfo.inputStatesApplied.GuitarGripBlue     = (this->_clientInputApplied[NDSInputID_GuitarGrip_Blue].isPressed)   ? 0 : 1;
	this->_ndsFrameInfo.inputStatesApplied.GuitarGripYellow   = (this->_clientInputApplied[NDSInputID_GuitarGrip_Yellow].isPressed) ? 0 : 1;
	this->_ndsFrameInfo.inputStatesApplied.GuitarGripRed      = (this->_clientInputApplied[NDSInputID_GuitarGrip_Red].isPressed)    ? 0 : 1;
	this->_ndsFrameInfo.inputStatesApplied.GuitarGripGreen    = (this->_clientInputApplied[NDSInputID_GuitarGrip_Green].isPressed)  ? 0 : 1;
	this->_ndsFrameInfo.inputStatesApplied.Paddle             = (this->_clientInputApplied[NDSInputID_Paddle].isPressed)            ? 0 : 1;
	this->_ndsFrameInfo.inputStatesApplied.Microphone         = (this->_clientInputApplied[NDSInputID_Microphone].isPressed)        ? 0 : 1;
	this->_ndsFrameInfo.inputStatesApplied.Reset              = (this->_clientInputApplied[NDSInputID_Reset].isPressed)             ? 0 : 1;
	this->_ndsFrameInfo.touchLocXApplied                      = this->_touchLocXApplied;
	this->_ndsFrameInfo.touchLocYApplied                      = this->_touchLocYApplied;
	this->_ndsFrameInfo.touchPressureApplied                  = this->_touchPressureApplied;
	this->_ndsFrameInfo.paddleValueApplied                    = this->_paddleValueApplied;
	this->_ndsFrameInfo.paddleAdjustApplied                   = this->_paddleAdjustApplied;
	
	pthread_mutex_unlock(&this->_mutexOutputPostNDSExec);
}

const NDSFrameInfo& ClientExecutionControl::GetNDSFrameInfo()
{
	return this->_ndsFrameInfo;
}

uint64_t ClientExecutionControl::GetFrameIndex()
{
	pthread_mutex_lock(&this->_mutexOutputPostNDSExec);
	const uint64_t frameIndex = this->_ndsFrameInfo.frameIndex;
	pthread_mutex_unlock(&this->_mutexOutputPostNDSExec);
	
	return frameIndex;
}

double ClientExecutionControl::GetFrameTime()
{
	return this->_frameTime;
}

uint8_t ClientExecutionControl::CalculateFrameSkip(double startAbsoluteTime, double frameAbsoluteTime)
{
	static const double skipCurve[10]	= {0.60, 0.58, 0.55, 0.51, 0.46, 0.40, 0.30, 0.20, 0.10, 0.00};
	static const double unskipCurve[10]	= {0.75, 0.70, 0.65, 0.60, 0.50, 0.40, 0.30, 0.20, 0.10, 0.00};
	static size_t skipStep = 0;
	static size_t unskipStep = 0;
	static uint64_t lastSetFrameSkip = 0;
	
	// Calculate the time remaining.
	const double elapsed = this->GetCurrentAbsoluteTime() - startAbsoluteTime;
	uint64_t framesToSkip = 0;
	
	if (elapsed > frameAbsoluteTime)
	{
		if (frameAbsoluteTime > 0)
		{
			framesToSkip = (uint64_t)( (((elapsed - frameAbsoluteTime) * FRAME_SKIP_AGGRESSIVENESS) / frameAbsoluteTime) + FRAME_SKIP_BIAS );
			
			if (framesToSkip > lastSetFrameSkip)
			{
				framesToSkip -= (uint64_t)((double)(framesToSkip - lastSetFrameSkip) * skipCurve[skipStep]);
				if (skipStep < 9)
				{
					skipStep++;
				}
			}
			else
			{
				framesToSkip += (uint64_t)((double)(lastSetFrameSkip - framesToSkip) * skipCurve[skipStep]);
				if (skipStep > 0)
				{
					skipStep--;
				}
			}
		}
		else
		{
			static const double frameRate100x = (double)FRAME_SKIP_AGGRESSIVENESS / CalculateFrameAbsoluteTime(1.0/100.0);
			framesToSkip = (uint64_t)(elapsed * frameRate100x);
		}
		
		unskipStep = 0;
	}
	else
	{
		framesToSkip = (uint64_t)((double)lastSetFrameSkip * unskipCurve[unskipStep]);
		if (unskipStep < 9)
		{
			unskipStep++;
		}
		
		skipStep = 0;
	}
	
	// Bound the frame skip.
	static const uint64_t kMaxFrameSkip = (uint64_t)MAX_FRAME_SKIP;
	if (framesToSkip > kMaxFrameSkip)
	{
		framesToSkip = kMaxFrameSkip;
	}
	
	lastSetFrameSkip = framesToSkip;
	
	return (uint8_t)framesToSkip;
}

double ClientExecutionControl::GetCurrentAbsoluteTime()
{
	return (double)mach_absolute_time();
}

double ClientExecutionControl::CalculateFrameAbsoluteTime(double frameTimeScalar)
{
	mach_timebase_info_data_t timeBase;
	mach_timebase_info(&timeBase);
	
	const double frameTimeNanoseconds = DS_SECONDS_PER_FRAME * 1000000000.0 * frameTimeScalar;
	
	return (frameTimeNanoseconds * (double)timeBase.denom) / (double)timeBase.numer;
}

void ClientExecutionControl::WaitUntilAbsoluteTime(double deadlineAbsoluteTime)
{
	mach_wait_until((uint64_t)deadlineAbsoluteTime);
}
