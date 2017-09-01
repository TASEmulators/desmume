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

#include "ClientExecutionControl.h"


ClientExecutionControl::ClientExecutionControl()
{
	_newSettingsPendingOnReset = true;
	_newSettingsPendingOnExecutionLoopStart = true;
	_newSettingsPendingOnNDSExec = true;
	
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
	_settingsPending.frameJumpTarget					= 0;
	
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
	
	pthread_mutex_init(&_mutexSettingsPendingOnReset, NULL);
	pthread_mutex_init(&_mutexSettingsPendingOnExecutionLoopStart, NULL);
	pthread_mutex_init(&_mutexSettingsPendingOnNDSExec, NULL);
	pthread_mutex_init(&_mutexOutputPostNDSExec, NULL);
}

ClientExecutionControl::~ClientExecutionControl()
{
	pthread_mutex_destroy(&this->_mutexSettingsPendingOnReset);
	pthread_mutex_destroy(&this->_mutexSettingsPendingOnExecutionLoopStart);
	pthread_mutex_destroy(&this->_mutexSettingsPendingOnNDSExec);
	pthread_mutex_destroy(&this->_mutexOutputPostNDSExec);
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
	engineID = CPUEmulationEngine_Interpreter;
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
	return this->_settingsPending.jumpBehavior;
}

void ClientExecutionControl::SetFrameJumpBehavior(FrameJumpBehavior newBehavior)
{
	this->_settingsPending.jumpBehavior = newBehavior;
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
		
		this->_settingsApplied.frameJumpTarget				= this->_settingsPending.frameJumpTarget;
		
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
	
	const UserInput &ndsInputsPending = NDS_getRawUserInput();
	const UserInput &ndsInputsApplied = NDS_getFinalUserInput();
	
	this->_ndsFrameInfo.inputStatesPending.value			= INPUT_STATES_CLEAR_VALUE;
	this->_ndsFrameInfo.inputStatesPending.A				= (ndsInputsPending.buttons.A) ? 0 : 1;
	this->_ndsFrameInfo.inputStatesPending.B				= (ndsInputsPending.buttons.B) ? 0 : 1;
	this->_ndsFrameInfo.inputStatesPending.Select			= (ndsInputsPending.buttons.T) ? 0 : 1;
	this->_ndsFrameInfo.inputStatesPending.Start			= (ndsInputsPending.buttons.S) ? 0 : 1;
	this->_ndsFrameInfo.inputStatesPending.Right			= (ndsInputsPending.buttons.R) ? 0 : 1;
	this->_ndsFrameInfo.inputStatesPending.Left				= (ndsInputsPending.buttons.L) ? 0 : 1;
	this->_ndsFrameInfo.inputStatesPending.Up				= (ndsInputsPending.buttons.U) ? 0 : 1;
	this->_ndsFrameInfo.inputStatesPending.Down				= (ndsInputsPending.buttons.D) ? 0 : 1;
	this->_ndsFrameInfo.inputStatesPending.R				= (ndsInputsPending.buttons.E) ? 0 : 1;
	this->_ndsFrameInfo.inputStatesPending.L				= (ndsInputsPending.buttons.W) ? 0 : 1;
	this->_ndsFrameInfo.inputStatesPending.X				= (ndsInputsPending.buttons.X) ? 0 : 1;
	this->_ndsFrameInfo.inputStatesPending.Y				= (ndsInputsPending.buttons.Y) ? 0 : 1;
	this->_ndsFrameInfo.inputStatesPending.Debug			= (ndsInputsPending.buttons.G) ? 0 : 1;
	this->_ndsFrameInfo.inputStatesPending.Touch			= (ndsInputsPending.touch.isTouch) ? 0 : 1;
	this->_ndsFrameInfo.inputStatesPending.Lid				= (ndsInputsPending.buttons.F) ? 0 : 1;
	this->_ndsFrameInfo.inputStatesPending.Microphone		= (ndsInputsPending.mic.micButtonPressed != 0) ? 0 : 1;
	this->_ndsFrameInfo.touchLocXPending					= ndsInputsPending.touch.touchX >> 4;
	this->_ndsFrameInfo.touchLocYPending					= ndsInputsPending.touch.touchY >> 4;
	
	this->_ndsFrameInfo.inputStatesApplied.value			= INPUT_STATES_CLEAR_VALUE;
	this->_ndsFrameInfo.inputStatesApplied.A				= (ndsInputsApplied.buttons.A) ? 0 : 1;
	this->_ndsFrameInfo.inputStatesApplied.B				= (ndsInputsApplied.buttons.B) ? 0 : 1;
	this->_ndsFrameInfo.inputStatesApplied.Select			= (ndsInputsApplied.buttons.T) ? 0 : 1;
	this->_ndsFrameInfo.inputStatesApplied.Start			= (ndsInputsApplied.buttons.S) ? 0 : 1;
	this->_ndsFrameInfo.inputStatesApplied.Right			= (ndsInputsApplied.buttons.R) ? 0 : 1;
	this->_ndsFrameInfo.inputStatesApplied.Left				= (ndsInputsApplied.buttons.L) ? 0 : 1;
	this->_ndsFrameInfo.inputStatesApplied.Up				= (ndsInputsApplied.buttons.U) ? 0 : 1;
	this->_ndsFrameInfo.inputStatesApplied.Down				= (ndsInputsApplied.buttons.D) ? 0 : 1;
	this->_ndsFrameInfo.inputStatesApplied.R				= (ndsInputsApplied.buttons.E) ? 0 : 1;
	this->_ndsFrameInfo.inputStatesApplied.L				= (ndsInputsApplied.buttons.W) ? 0 : 1;
	this->_ndsFrameInfo.inputStatesApplied.X				= (ndsInputsApplied.buttons.X) ? 0 : 1;
	this->_ndsFrameInfo.inputStatesApplied.Y				= (ndsInputsApplied.buttons.Y) ? 0 : 1;
	this->_ndsFrameInfo.inputStatesApplied.Debug			= (ndsInputsApplied.buttons.G) ? 0 : 1;
	this->_ndsFrameInfo.inputStatesApplied.Touch			= (ndsInputsApplied.touch.isTouch) ? 0 : 1;
	this->_ndsFrameInfo.inputStatesApplied.Lid				= (ndsInputsApplied.buttons.F) ? 0 : 1;
	this->_ndsFrameInfo.inputStatesApplied.Microphone		= (ndsInputsApplied.mic.micButtonPressed != 0) ? 0 : 1;
	this->_ndsFrameInfo.touchLocXApplied					= ndsInputsApplied.touch.touchX >> 4;
	this->_ndsFrameInfo.touchLocYApplied					= ndsInputsApplied.touch.touchY >> 4;
	
	pthread_mutex_unlock(&this->_mutexOutputPostNDSExec);
}

const NDSFrameInfo& ClientExecutionControl::GetNDSFrameInfo()
{
	return this->_ndsFrameInfo;
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
