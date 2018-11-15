/*
	Copyright (C) 2017-2018 DeSmuME team
 
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

#include "../../armcpu.h"
#include "../../GPU.h"
#include "../../movie.h"
#include "../../NDSSystem.h"
#include "../../gdbstub.h"
#include "../../rtc.h"

#include "ClientAVCaptureObject.h"
#include "ClientExecutionControl.h"

// Need to include assert.h this way so that GDB stub will work
// with an optimized build.
#if defined(GDB_STUB) && defined(NDEBUG)
#define TEMP_NDEBUG
#undef NDEBUG
#endif

#include <assert.h>

#if defined(TEMP_NDEBUG)
#undef TEMP_NDEBUG
#define NDEBUG
#endif


ClientExecutionControl::ClientExecutionControl()
{
	_inputHandler = NULL;
	
	_newSettingsPendingOnReset = true;
	_newSettingsPendingOnExecutionLoopStart = true;
	_newSettingsPendingOnNDSExec = true;
	
	_needResetFramesToSkip = false;
	
	_frameTime = 0.0;
	_framesToSkip = 0;
	_lastSetFrameSkip = 0.0;
	_unskipStep = 0;
	_dynamicBiasStep = 0;
	_prevExecBehavior = ExecutionBehavior_Pause;
	
	_isGdbStubStarted = false;
	_enableGdbStubARM9 = false;
	_enableGdbStubARM7 = false;
	_gdbStubPortARM9 = 0;
	_gdbStubPortARM7 = 0;
	_gdbStubHandleARM9 = NULL;
	_gdbStubHandleARM7 = NULL;
	_isInDebugTrap = false;
	
	_settingsPending.cpuEngineID						= CPUEmulationEngineID_Interpreter;
	_settingsPending.JITMaxBlockSize					= 12;
	_settingsPending.slot1DeviceType					= NDS_SLOT1_RETAIL_AUTO;
	
	memset(&_settingsPending.fwConfig, 0, sizeof(FirmwareConfig));
	
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
	_settingsPending.wifiEmulationMode					= WifiEmulationLevel_Off;
	_settingsPending.wifiBridgeDeviceIndex				= 0;
	
	_settingsPending.enableExecutionSpeedLimiter		= true;
	_settingsPending.executionSpeed						= SPEED_SCALAR_NORMAL;
	
	_settingsPending.enableFrameSkip					= true;
	_settingsPending.frameJumpRelativeTarget			= 60;
	_settingsPending.frameJumpTarget					= 60;
	
	_settingsPending.execBehavior						= ExecutionBehavior_Pause;
	_settingsPending.jumpBehavior						= FrameJumpBehavior_Forward;
	
	_settingsPending.avCaptureObject					= NULL;
	
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

ClientAVCaptureObject* ClientExecutionControl::GetClientAVCaptureObject()
{
	pthread_mutex_lock(&this->_mutexSettingsPendingOnNDSExec);
	ClientAVCaptureObject *theCaptureObject = this->_settingsPending.avCaptureObject;
	pthread_mutex_unlock(&this->_mutexSettingsPendingOnNDSExec);
	
	return theCaptureObject;
}

ClientAVCaptureObject* ClientExecutionControl::GetClientAVCaptureObjectApplied()
{
	return this->_settingsApplied.avCaptureObject;
}

void ClientExecutionControl::SetClientAVCaptureObject(ClientAVCaptureObject *theCaptureObject)
{
	pthread_mutex_lock(&this->_mutexSettingsPendingOnNDSExec);
	
	if (this->_settingsPending.avCaptureObject != theCaptureObject)
	{
		this->_settingsPending.avCaptureObject = theCaptureObject;
		
		this->_needResetFramesToSkip = true;
		this->_newSettingsPendingOnNDSExec = true;
	}
	
	pthread_mutex_unlock(&this->_mutexSettingsPendingOnNDSExec);
}

ClientInputHandler* ClientExecutionControl::GetClientInputHandler()
{
	return this->_inputHandler;
}

void ClientExecutionControl::SetClientInputHandler(ClientInputHandler *inputHandler)
{
	this->_inputHandler = inputHandler;
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
		this->_settingsPending.filePathFirmware = std::string(filePath, sizeof(CommonSettings.ExtFirmwarePath));
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
	pthread_mutex_lock(&this->_mutexSettingsPendingOnReset);
	const bool enable = this->_settingsPending.enableExternalBIOS;
	pthread_mutex_unlock(&this->_mutexSettingsPendingOnReset);
	
	return enable;
}

void ClientExecutionControl::SetEnableExternalBIOS(bool enable)
{
	pthread_mutex_lock(&this->_mutexSettingsPendingOnReset);
	this->_settingsPending.enableExternalBIOS = enable;
	
	this->_newSettingsPendingOnReset = true;
	pthread_mutex_unlock(&this->_mutexSettingsPendingOnReset);
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

FirmwareConfig ClientExecutionControl::GetFirmwareConfig()
{
	pthread_mutex_lock(&this->_mutexSettingsPendingOnReset);
	const FirmwareConfig outConfig = this->_settingsPending.fwConfig;
	pthread_mutex_unlock(&this->_mutexSettingsPendingOnReset);
	
	return outConfig;
}

FirmwareConfig ClientExecutionControl::GetFirmwareConfigApplied()
{
	return this->_settingsApplied.fwConfig;
}

void ClientExecutionControl::SetFirmwareConfig(const FirmwareConfig &inConfig)
{
	pthread_mutex_lock(&this->_mutexSettingsPendingOnReset);
	this->_settingsPending.fwConfig = inConfig;
	
	this->_newSettingsPendingOnReset = true;
	pthread_mutex_unlock(&this->_mutexSettingsPendingOnReset);
}

bool ClientExecutionControl::GetEnableExternalFirmware()
{
	pthread_mutex_lock(&this->_mutexSettingsPendingOnReset);
	const bool enable = this->_settingsPending.enableExternalFirmware;
	pthread_mutex_unlock(&this->_mutexSettingsPendingOnReset);
	
	return enable;
}

void ClientExecutionControl::SetEnableExternalFirmware(bool enable)
{
	pthread_mutex_lock(&this->_mutexSettingsPendingOnReset);
	this->_settingsPending.enableExternalFirmware = enable;
	
	this->_newSettingsPendingOnReset = true;
	pthread_mutex_unlock(&this->_mutexSettingsPendingOnReset);
}

bool ClientExecutionControl::GetEnableFirmwareBoot()
{
	pthread_mutex_lock(&this->_mutexSettingsPendingOnReset);
	const bool enable = this->_settingsPending.enableFirmwareBoot;
	pthread_mutex_unlock(&this->_mutexSettingsPendingOnReset);
	
	return enable;
}

void ClientExecutionControl::SetEnableFirmwareBoot(bool enable)
{
	pthread_mutex_lock(&this->_mutexSettingsPendingOnReset);
	this->_settingsPending.enableFirmwareBoot = enable;
	
	this->_newSettingsPendingOnReset = true;
	pthread_mutex_unlock(&this->_mutexSettingsPendingOnReset);
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

int ClientExecutionControl::GetWifiEmulationMode()
{
	pthread_mutex_lock(&this->_mutexSettingsPendingOnReset);
	const int wifiEmulationMode = this->_settingsPending.wifiEmulationMode;
	pthread_mutex_unlock(&this->_mutexSettingsPendingOnReset);
	
	return wifiEmulationMode;
}

void ClientExecutionControl::SetWifiEmulationMode(int wifiEmulationMode)
{
	pthread_mutex_lock(&this->_mutexSettingsPendingOnReset);
	this->_settingsPending.wifiEmulationMode = wifiEmulationMode;
	
	this->_newSettingsPendingOnReset = true;
	pthread_mutex_unlock(&this->_mutexSettingsPendingOnReset);
}

int ClientExecutionControl::GetWifiBridgeDeviceIndex()
{
	pthread_mutex_lock(&this->_mutexSettingsPendingOnReset);
	const int wifiBridgeDeviceIndex = this->_settingsPending.wifiBridgeDeviceIndex;
	pthread_mutex_unlock(&this->_mutexSettingsPendingOnReset);
	
	return wifiBridgeDeviceIndex;
}

void ClientExecutionControl::SetWifiBridgeDeviceIndex(int wifiBridgeDeviceIndex)
{
	pthread_mutex_lock(&this->_mutexSettingsPendingOnReset);
	this->_settingsPending.wifiBridgeDeviceIndex = wifiBridgeDeviceIndex;
	
	this->_newSettingsPendingOnReset = true;
	pthread_mutex_unlock(&this->_mutexSettingsPendingOnReset);
}

uint8_t* ClientExecutionControl::GetCurrentSessionMACAddress()
{
	return (uint8_t *)FW_Mac;
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

bool ClientExecutionControl::GetEnableSpeedLimiterApplied()
{
	return this->_settingsApplied.enableExecutionSpeedLimiter;
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

double ClientExecutionControl::GetExecutionSpeedApplied()
{
	return this->_settingsApplied.executionSpeed;
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

bool ClientExecutionControl::IsGDBStubARM9Enabled()
{
	return this->_enableGdbStubARM9;
}

void ClientExecutionControl::SetGDBStubARM9Enabled(bool theState)
{
	this->_enableGdbStubARM9 = theState;
}

bool ClientExecutionControl::IsGDBStubARM7Enabled()
{
	return this->_enableGdbStubARM7;
}

void ClientExecutionControl::SetGDBStubARM7Enabled(bool theState)
{
	this->_enableGdbStubARM7 = theState;
}

uint16_t ClientExecutionControl::GetGDBStubARM9Port()
{
	return this->_gdbStubPortARM9;
}

void ClientExecutionControl::SetGDBStubARM9Port(uint16_t portNumber)
{
	this->_gdbStubPortARM9 = portNumber;
}

uint16_t ClientExecutionControl::GetGDBStubARM7Port()
{
	return this->_gdbStubPortARM7;
}

void ClientExecutionControl::SetGDBStubARM7Port(uint16_t portNumber)
{
	this->_gdbStubPortARM7 = portNumber;
}

bool ClientExecutionControl::IsGDBStubStarted()
{
	return this->_isGdbStubStarted;
}

void ClientExecutionControl::SetIsGDBStubStarted(bool theState)
{
#ifdef GDB_STUB
	if (theState)
	{
        gdbstub_mutex_init();
        
		if (this->_enableGdbStubARM9)
		{
			const uint16_t arm9Port = this->_gdbStubPortARM9;
			if(arm9Port > 0)
			{
				this->_gdbStubHandleARM9 = createStub_gdb(arm9Port, &NDS_ARM9, &arm9_direct_memory_iface);
				if (this->_gdbStubHandleARM9 == NULL)
				{
					printf("Failed to create ARM9 gdbstub on port %d\n", arm9Port);
				}
				else
				{
					activateStub_gdb(this->_gdbStubHandleARM9);
				}
			}
		}
		else
		{
			destroyStub_gdb(this->_gdbStubHandleARM9);
			this->_gdbStubHandleARM9 = NULL;
		}
		
		if (this->_enableGdbStubARM7)
		{
			const uint16_t arm7Port = this->_gdbStubPortARM7;
			if (arm7Port > 0)
			{
				this->_gdbStubHandleARM7 = createStub_gdb(arm7Port, &NDS_ARM7, &arm7_base_memory_iface);
				if (this->_gdbStubHandleARM7 == NULL)
				{
					printf("Failed to create ARM7 gdbstub on port %d\n", arm7Port);
				}
				else
				{
					activateStub_gdb(this->_gdbStubHandleARM7);
				}
			}
		}
		else
		{
			destroyStub_gdb(this->_gdbStubHandleARM7);
			this->_gdbStubHandleARM7 = NULL;
		}
	}
	else
	{
		destroyStub_gdb(this->_gdbStubHandleARM9);
		this->_gdbStubHandleARM9 = NULL;
		
		destroyStub_gdb(this->_gdbStubHandleARM7);
		this->_gdbStubHandleARM7 = NULL;

        gdbstub_mutex_destroy();
	}
#endif
	if ( (this->_gdbStubHandleARM9 == NULL) && (this->_gdbStubHandleARM7 == NULL) )
	{
		theState = false;
	}
	
	this->_isGdbStubStarted = theState;
}

bool ClientExecutionControl::IsInDebugTrap()
{
	return this->_isInDebugTrap;
}

void ClientExecutionControl::SetIsInDebugTrap(bool theState)
{
	// If we're transitioning out of the debug trap, then ignore
	// frame skipping this time.
	if (this->_isInDebugTrap && !theState)
	{
		this->ResetFramesToSkip();
	}
	
	this->_isInDebugTrap = theState;
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
		this->_settingsApplied.cpuEngineID					= this->_settingsPending.cpuEngineID;
		this->_settingsApplied.JITMaxBlockSize				= this->_settingsPending.JITMaxBlockSize;
		
		this->_settingsApplied.filePathARM9BIOS				= this->_settingsPending.filePathARM9BIOS;
		this->_settingsApplied.filePathARM7BIOS				= this->_settingsPending.filePathARM7BIOS;
		this->_settingsApplied.filePathFirmware				= this->_settingsPending.filePathFirmware;
		this->_settingsApplied.filePathSlot1R4				= this->_settingsPending.filePathSlot1R4;
		
		this->_settingsApplied.enableExternalBIOS			= this->_settingsPending.enableExternalBIOS;
		this->_settingsApplied.enableExternalFirmware		= this->_settingsPending.enableExternalFirmware;
		this->_settingsApplied.enableFirmwareBoot			= this->_settingsPending.enableFirmwareBoot;
		
		this->_settingsApplied.fwConfig						= this->_settingsPending.fwConfig;
		
		this->_settingsApplied.wifiEmulationMode			= this->_settingsPending.wifiEmulationMode;
		this->_settingsApplied.wifiBridgeDeviceIndex		= this->_settingsPending.wifiBridgeDeviceIndex;
		
		this->_settingsApplied.cpuEmulationEngineName		= this->_settingsPending.cpuEmulationEngineName;
		this->_settingsApplied.slot1DeviceName				= this->_settingsPending.slot1DeviceName;
		this->_ndsFrameInfo.cpuEmulationEngineName			= this->_settingsApplied.cpuEmulationEngineName;
		this->_ndsFrameInfo.slot1DeviceName					= this->_settingsApplied.slot1DeviceName;
		
		const bool didChangeSlot1Type = (this->_settingsApplied.slot1DeviceType != this->_settingsPending.slot1DeviceType);
		if (didChangeSlot1Type)
		{
			this->_settingsApplied.slot1DeviceType	= this->_settingsPending.slot1DeviceType;
		}
		
		this->_newSettingsPendingOnReset = false;
		pthread_mutex_unlock(&this->_mutexSettingsPendingOnReset);
		
		CommonSettings.use_jit					= (this->_settingsApplied.cpuEngineID == CPUEmulationEngineID_DynamicRecompiler);
		CommonSettings.jit_max_block_size		= this->_settingsApplied.JITMaxBlockSize;
		CommonSettings.UseExtBIOS				= this->_settingsApplied.enableExternalBIOS;
		CommonSettings.UseExtFirmware			= this->_settingsApplied.enableExternalFirmware;
		CommonSettings.UseExtFirmwareSettings	= this->_settingsApplied.enableExternalFirmware;
		CommonSettings.BootFromFirmware			= this->_settingsApplied.enableFirmwareBoot;
		CommonSettings.WifiBridgeDeviceID		= this->_settingsApplied.wifiBridgeDeviceIndex;
		CommonSettings.fwConfig					= this->_settingsApplied.fwConfig;
		
		wifiHandler->SetEmulationLevel((WifiEmulationLevel)this->_settingsApplied.wifiEmulationMode);
		wifiHandler->SetBridgeDeviceIndex(this->_settingsApplied.wifiBridgeDeviceIndex);
		
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
			memset(CommonSettings.ExtFirmwarePath, 0, sizeof(CommonSettings.ExtFirmwarePath));
		}
		else
		{
			strlcpy(CommonSettings.ExtFirmwarePath, this->_settingsApplied.filePathFirmware.c_str(), sizeof(CommonSettings.ExtFirmwarePath));
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
		this->_settingsApplied.enableBIOSInterrupts				= this->_settingsPending.enableBIOSInterrupts;
		this->_settingsApplied.enableBIOSPatchDelayLoop			= this->_settingsPending.enableBIOSPatchDelayLoop;
		this->_settingsApplied.enableDebugConsole				= this->_settingsPending.enableDebugConsole;
		this->_settingsApplied.enableEnsataEmulation			= this->_settingsPending.enableEnsataEmulation;
		
		this->_settingsApplied.enableCheats						= this->_settingsPending.enableCheats;
		
		this->_settingsApplied.enableFrameSkip					= this->_settingsPending.enableFrameSkip;
		
		this->_settingsApplied.avCaptureObject					= this->_settingsPending.avCaptureObject;
		
		const bool needResetFramesToSkip = this->_needResetFramesToSkip;
		
		this->_needResetFramesToSkip = false;
		this->_newSettingsPendingOnNDSExec = false;
		pthread_mutex_unlock(&this->_mutexSettingsPendingOnNDSExec);
		
		CommonSettings.advanced_timing			= this->_settingsApplied.enableAdvancedBusLevelTiming;
		CommonSettings.rigorous_timing			= this->_settingsApplied.enableRigorous3DRenderingTiming;
		CommonSettings.SWIFromBIOS				= this->_settingsApplied.enableBIOSInterrupts;
		CommonSettings.PatchSWI3				= this->_settingsApplied.enableBIOSPatchDelayLoop;
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
	
	this->_ndsFrameInfo.inputStatesApplied.value              = INPUT_STATES_CLEAR_VALUE;
	
	if (this->_inputHandler != NULL)
	{
		const ClientInput *appliedInput = this->_inputHandler->GetClientInputsApplied();
		
		this->_ndsFrameInfo.inputStatesApplied.A                  = (appliedInput[NDSInputID_A].isPressed)                 ? 0 : 1;
		this->_ndsFrameInfo.inputStatesApplied.B                  = (appliedInput[NDSInputID_B].isPressed)                 ? 0 : 1;
		this->_ndsFrameInfo.inputStatesApplied.Select             = (appliedInput[NDSInputID_Select].isPressed)            ? 0 : 1;
		this->_ndsFrameInfo.inputStatesApplied.Start              = (appliedInput[NDSInputID_Start].isPressed)             ? 0 : 1;
		this->_ndsFrameInfo.inputStatesApplied.Right              = (appliedInput[NDSInputID_Right].isPressed)             ? 0 : 1;
		this->_ndsFrameInfo.inputStatesApplied.Left               = (appliedInput[NDSInputID_Left].isPressed)              ? 0 : 1;
		this->_ndsFrameInfo.inputStatesApplied.Up                 = (appliedInput[NDSInputID_Up].isPressed)                ? 0 : 1;
		this->_ndsFrameInfo.inputStatesApplied.Down               = (appliedInput[NDSInputID_Down].isPressed)              ? 0 : 1;
		this->_ndsFrameInfo.inputStatesApplied.R                  = (appliedInput[NDSInputID_R].isPressed)                 ? 0 : 1;
		this->_ndsFrameInfo.inputStatesApplied.L                  = (appliedInput[NDSInputID_L].isPressed)                 ? 0 : 1;
		this->_ndsFrameInfo.inputStatesApplied.X                  = (appliedInput[NDSInputID_X].isPressed)                 ? 0 : 1;
		this->_ndsFrameInfo.inputStatesApplied.Y                  = (appliedInput[NDSInputID_Y].isPressed)                 ? 0 : 1;
		this->_ndsFrameInfo.inputStatesApplied.Debug              = (appliedInput[NDSInputID_Debug].isPressed)             ? 0 : 1;
		this->_ndsFrameInfo.inputStatesApplied.Touch              = (appliedInput[NDSInputID_Touch].isPressed)             ? 0 : 1;
		this->_ndsFrameInfo.inputStatesApplied.Lid                = (appliedInput[NDSInputID_Lid].isPressed)               ? 0 : 1;
		this->_ndsFrameInfo.inputStatesApplied.PianoC             = (appliedInput[NDSInputID_Piano_C].isPressed)           ? 0 : 1;
		this->_ndsFrameInfo.inputStatesApplied.PianoCSharp        = (appliedInput[NDSInputID_Piano_CSharp].isPressed)      ? 0 : 1;
		this->_ndsFrameInfo.inputStatesApplied.PianoD             = (appliedInput[NDSInputID_Piano_D].isPressed)           ? 0 : 1;
		this->_ndsFrameInfo.inputStatesApplied.PianoDSharp        = (appliedInput[NDSInputID_Piano_DSharp].isPressed)      ? 0 : 1;
		this->_ndsFrameInfo.inputStatesApplied.PianoE             = (appliedInput[NDSInputID_Piano_E].isPressed)           ? 0 : 1;
		this->_ndsFrameInfo.inputStatesApplied.PianoF             = (appliedInput[NDSInputID_Piano_F].isPressed)           ? 0 : 1;
		this->_ndsFrameInfo.inputStatesApplied.PianoFSharp        = (appliedInput[NDSInputID_Piano_FSharp].isPressed)      ? 0 : 1;
		this->_ndsFrameInfo.inputStatesApplied.PianoG             = (appliedInput[NDSInputID_Piano_G].isPressed)           ? 0 : 1;
		this->_ndsFrameInfo.inputStatesApplied.PianoGSharp        = (appliedInput[NDSInputID_Piano_GSharp].isPressed)      ? 0 : 1;
		this->_ndsFrameInfo.inputStatesApplied.PianoA             = (appliedInput[NDSInputID_Piano_A].isPressed)           ? 0 : 1;
		this->_ndsFrameInfo.inputStatesApplied.PianoASharp        = (appliedInput[NDSInputID_Piano_ASharp].isPressed)      ? 0 : 1;
		this->_ndsFrameInfo.inputStatesApplied.PianoB             = (appliedInput[NDSInputID_Piano_B].isPressed)           ? 0 : 1;
		this->_ndsFrameInfo.inputStatesApplied.PianoHighC         = (appliedInput[NDSInputID_Piano_HighC].isPressed)       ? 0 : 1;
		this->_ndsFrameInfo.inputStatesApplied.GuitarGripBlue     = (appliedInput[NDSInputID_GuitarGrip_Blue].isPressed)   ? 0 : 1;
		this->_ndsFrameInfo.inputStatesApplied.GuitarGripYellow   = (appliedInput[NDSInputID_GuitarGrip_Yellow].isPressed) ? 0 : 1;
		this->_ndsFrameInfo.inputStatesApplied.GuitarGripRed      = (appliedInput[NDSInputID_GuitarGrip_Red].isPressed)    ? 0 : 1;
		this->_ndsFrameInfo.inputStatesApplied.GuitarGripGreen    = (appliedInput[NDSInputID_GuitarGrip_Green].isPressed)  ? 0 : 1;
		this->_ndsFrameInfo.inputStatesApplied.Paddle             = (appliedInput[NDSInputID_Paddle].isPressed)            ? 0 : 1;
		this->_ndsFrameInfo.inputStatesApplied.Microphone         = (appliedInput[NDSInputID_Microphone].isPressed)        ? 0 : 1;
		this->_ndsFrameInfo.inputStatesApplied.Reset              = (appliedInput[NDSInputID_Reset].isPressed)             ? 0 : 1;
		this->_ndsFrameInfo.touchLocXApplied                      = this->_inputHandler->GetTouchLocXApplied();
		this->_ndsFrameInfo.touchLocYApplied                      = this->_inputHandler->GetTouchLocYApplied();
		this->_ndsFrameInfo.touchPressureApplied                  = this->_inputHandler->GetTouchPressureApplied();
		this->_ndsFrameInfo.paddleValueApplied                    = this->_inputHandler->GetPaddleValueApplied();
		this->_ndsFrameInfo.paddleAdjustApplied                   = this->_inputHandler->GetPaddleAdjustApplied();
	}
	
	pthread_mutex_unlock(&this->_mutexOutputPostNDSExec);
}

const NDSFrameInfo& ClientExecutionControl::GetNDSFrameInfo()
{
	return this->_ndsFrameInfo;
}

void ClientExecutionControl::SetFrameInfoExecutionSpeed(double executionSpeed)
{
	this->_ndsFrameInfo.executionSpeed = executionSpeed;
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
	static const double unskipCurve[21]	= {0.98, 0.95, 0.91, 0.86, 0.80, 0.73, 0.65, 0.56, 0.46, 0.35, 0.23, 0.20, 0.17, 0.14, 0.11, 0.08, 0.06, 0.04, 0.02, 0.01, 0.00};
	static const double dynamicBiasCurve[15] = {0.0, 0.2, 0.6, 1.2, 2.0, 3.0, 4.2, 5.6, 7.2, 9.0, 11.0, 13.2, 15.6, 18.2, 20.0};
	
	// Calculate the time remaining.
	const double elapsed = this->GetCurrentAbsoluteTime() - startAbsoluteTime;
	uint64_t framesToSkipInt = 0;
	
	if (elapsed > frameAbsoluteTime)
	{
		if (frameAbsoluteTime > 0)
		{
			const double framesToSkipReal = ((elapsed * FRAME_SKIP_AGGRESSIVENESS) / frameAbsoluteTime) + dynamicBiasCurve[this->_dynamicBiasStep] + FRAME_SKIP_BIAS;
			framesToSkipInt = (uint64_t)(framesToSkipReal + 0.5);
			
			const double frameSkipDiff = framesToSkipReal - this->_lastSetFrameSkip;
			if (this->_unskipStep > 0)
			{
				if (this->_dynamicBiasStep > 0)
				{
					this->_dynamicBiasStep--;
				}
			}
			else if (frameSkipDiff > 0.0)
			{
				if (this->_dynamicBiasStep < 14)
				{
					this->_dynamicBiasStep++;
				}
			}
			
			this->_unskipStep = 0;
			this->_lastSetFrameSkip = framesToSkipReal;
		}
		else
		{
			static const double frameRate100x = (double)FRAME_SKIP_AGGRESSIVENESS / CalculateFrameAbsoluteTime(1.0/100.0);
			framesToSkipInt = (uint64_t)(elapsed * frameRate100x);
		}
	}
	else
	{
		const double framesToSkipReal = this->_lastSetFrameSkip * unskipCurve[this->_unskipStep];
		framesToSkipInt = (uint64_t)(framesToSkipReal + 0.5);
		
		if (this->_unskipStep < 20)
		{
			this->_unskipStep++;
		}
		
		if (framesToSkipInt == 0)
		{
			this->_lastSetFrameSkip = 0.0;
			this->_unskipStep = 20;
		}
	}
	
	// Bound the frame skip.
	static const uint64_t kMaxFrameSkip = (uint64_t)MAX_FRAME_SKIP;
	if (framesToSkipInt > kMaxFrameSkip)
	{
		framesToSkipInt = kMaxFrameSkip;
	}
	
	return (uint8_t)framesToSkipInt;
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

void* createThread_gdb(void (*thread_function)(void *data), void *thread_data)
{
	// Create the thread using POSIX routines.
	pthread_attr_t  attr;
	pthread_t*      posixThreadID = (pthread_t*)malloc(sizeof(pthread_t));
	
	assert(!pthread_attr_init(&attr));
	assert(!pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE));
	
	int threadError = pthread_create(posixThreadID, &attr, (void* (*)(void *))thread_function, thread_data);
	
	assert(!pthread_attr_destroy(&attr));
	
	if (threadError != 0)
	{
		// Report an error.
		return NULL;
	}
	else
	{
		return posixThreadID;
	}
}

void joinThread_gdb(void *thread_handle)
{
	pthread_join(*(pthread_t *)thread_handle, NULL);
	free(thread_handle);
}
