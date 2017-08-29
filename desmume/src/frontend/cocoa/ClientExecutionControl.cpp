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

#include "NDSSystem.h"

#include "ClientExecutionControl.h"


ClientExecutionControl::ClientExecutionControl()
{
	_settingsPending.cpuEngineID = CPUEmulationEngineID_Interpreter;
	_settingsPending.JITMaxBlockSize = 12;
	
	_settingsPending.enableAdvancedBusLevelTiming = true;
	_settingsPending.enableRigorous3DRenderingTiming = false;
	_settingsPending.enableExternalBIOS = false;
	_settingsPending.enableBIOSInterrupts = false;
	_settingsPending.enableBIOSPatchDelayLoop = false;
	_settingsPending.enableExternalFirmware = false;
	_settingsPending.enableFirmwareBoot = false;
	_settingsPending.enableDebugConsole = false;
	_settingsPending.enableEnsataEmulation = false;
	
	_settingsPending.executionSpeed = 1.0f;
	
	_settingsPending.enableFrameSkip = true;
	_settingsPending.framesToSkip = 0;
	_settingsPending.frameJumpTarget = 0;
	
	_settingsPending.execBehavior = ExecutionBehavior_Pause;
	_settingsPending.jumpBehavior = FrameJumpBehavior_Forward;
	
	pthread_mutex_init(&_mutexSettingsPendingOnReset, NULL);
	pthread_mutex_init(&_mutexSettingsApplyOnReset, NULL);
	pthread_mutex_init(&_mutexSettingsPendingOnExecutionLoopStart, NULL);
	pthread_mutex_init(&_mutexSettingsApplyOnExecutionLoopStart, NULL);
	pthread_mutex_init(&_mutexSettingsPendingOnNDSExec, NULL);
	pthread_mutex_init(&_mutexSettingsApplyOnNDSExec, NULL);
}

ClientExecutionControl::~ClientExecutionControl()
{
	pthread_mutex_destroy(&this->_mutexSettingsPendingOnReset);
	pthread_mutex_destroy(&this->_mutexSettingsApplyOnReset);
	pthread_mutex_destroy(&this->_mutexSettingsPendingOnExecutionLoopStart);
	pthread_mutex_destroy(&this->_mutexSettingsApplyOnExecutionLoopStart);
	pthread_mutex_destroy(&this->_mutexSettingsPendingOnNDSExec);
	pthread_mutex_destroy(&this->_mutexSettingsApplyOnNDSExec);
}

CPUEmulationEngineID ClientExecutionControl::GetCPUEmulationEngineID()
{
	pthread_mutex_lock(&this->_mutexSettingsPendingOnReset);
	const CPUEmulationEngineID engineID = this->_settingsPending.cpuEngineID;
	pthread_mutex_unlock(&this->_mutexSettingsPendingOnReset);
	
	return engineID;
}

void ClientExecutionControl::SetCPUEmulationEngineID(CPUEmulationEngineID engineID)
{
	pthread_mutex_lock(&this->_mutexSettingsPendingOnReset);
	this->_settingsPending.cpuEngineID = engineID;
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
	pthread_mutex_unlock(&this->_mutexSettingsPendingOnNDSExec);
}

bool ClientExecutionControl::GetEnableSpeedLimiter()
{
	pthread_mutex_lock(&this->_mutexSettingsPendingOnExecutionLoopStart);
	const bool enable = this->_settingsPending.enableSpeedLimiter;
	pthread_mutex_unlock(&this->_mutexSettingsPendingOnExecutionLoopStart);
	
	return enable;
}

void ClientExecutionControl::SetEnableSpeedLimiter(bool enable)
{
	pthread_mutex_lock(&this->_mutexSettingsPendingOnExecutionLoopStart);
	this->_settingsPending.enableSpeedLimiter = enable;
	pthread_mutex_unlock(&this->_mutexSettingsPendingOnExecutionLoopStart);
}

float ClientExecutionControl::GetExecutionSpeed()
{
	pthread_mutex_lock(&this->_mutexSettingsPendingOnExecutionLoopStart);
	const float speedScalar = this->_settingsPending.executionSpeed;
	pthread_mutex_unlock(&this->_mutexSettingsPendingOnExecutionLoopStart);
	
	return speedScalar;
}

void ClientExecutionControl::SetExecutionSpeed(float speedScalar)
{
	pthread_mutex_lock(&this->_mutexSettingsPendingOnExecutionLoopStart);
	this->_settingsPending.executionSpeed = speedScalar;
	pthread_mutex_unlock(&this->_mutexSettingsPendingOnExecutionLoopStart);
}

bool ClientExecutionControl::GetEnableFrameSkip()
{
	pthread_mutex_lock(&this->_mutexSettingsPendingOnExecutionLoopStart);
	const bool enable = this->_settingsPending.enableFrameSkip;
	pthread_mutex_unlock(&this->_mutexSettingsPendingOnExecutionLoopStart);
	
	return enable;
}

void ClientExecutionControl::SetEnableFrameSkip(bool enable)
{
	pthread_mutex_lock(&this->_mutexSettingsPendingOnExecutionLoopStart);
	
	if (this->_settingsPending.enableFrameSkip != enable)
	{
		this->_settingsPending.enableFrameSkip = enable;
		this->_settingsPending.framesToSkip = 0;
	}
	
	pthread_mutex_unlock(&this->_mutexSettingsPendingOnExecutionLoopStart);
}

uint8_t ClientExecutionControl::GetFramesToSkip()
{
	pthread_mutex_lock(&this->_mutexSettingsPendingOnExecutionLoopStart);
	const uint8_t numFramesToSkip = this->_settingsPending.framesToSkip;
	pthread_mutex_unlock(&this->_mutexSettingsPendingOnExecutionLoopStart);
	
	return numFramesToSkip;
}

uint64_t ClientExecutionControl::GetFrameJumpTarget()
{
	return this->_settingsPending.frameJumpTarget;
}

void ClientExecutionControl::SetFrameJumpTarget(uint64_t newJumpTarget)
{
	this->_settingsPending.frameJumpTarget = newJumpTarget;
}

ExecutionBehavior ClientExecutionControl::GetExecutionBehavior()
{
	return this->_settingsPending.execBehavior;
}

void ClientExecutionControl::SetExecutionBehavior(ExecutionBehavior newBehavior)
{
	this->_settingsPending.execBehavior = newBehavior;
}

FrameJumpBehavior ClientExecutionControl::GetFrameJumpBehavior()
{
	return this->_settingsPending.jumpBehavior;
}

void ClientExecutionControl::SetFrameJumpBehavior(FrameJumpBehavior newBehavior)
{
	this->_settingsPending.jumpBehavior = newBehavior;
}

void ClientExecutionControl::FlushSettingsOnReset()
{
	pthread_mutex_lock(&this->_mutexSettingsPendingOnReset);
	
	CommonSettings.use_jit = (this->_settingsPending.cpuEngineID == CPUEmulationEngineID_DynamicRecompiler);
	CommonSettings.jit_max_block_size = this->_settingsPending.JITMaxBlockSize;
	
	pthread_mutex_unlock(&this->_mutexSettingsPendingOnReset);
}

void ClientExecutionControl::FlushSettingsOnExecutionLoopStart()
{
	pthread_mutex_lock(&this->_mutexSettingsApplyOnExecutionLoopStart);
	
	pthread_mutex_lock(&this->_mutexSettingsPendingOnExecutionLoopStart);
	
	const bool didFrameSkipSettingChange = (this->_settingsPending.enableFrameSkip != this->_settingsApplied.enableFrameSkip);
	if (didFrameSkipSettingChange)
	{
		this->_settingsApplied.enableFrameSkip	= this->_settingsPending.enableFrameSkip;
		this->_settingsApplied.framesToSkip		= this->_settingsPending.framesToSkip;
	}
	
	const float speedScalar = (this->_settingsPending.executionSpeed > SPEED_SCALAR_MIN) ? this->_settingsPending.executionSpeed : SPEED_SCALAR_MIN;
	const bool didExecutionSpeedChange = (speedScalar != this->_settingsApplied.executionSpeed) || (this->_settingsPending.enableSpeedLimiter != this->_settingsApplied.enableSpeedLimiter);
	if (didExecutionSpeedChange)
	{
		this->_settingsApplied.enableSpeedLimiter	= this->_settingsPending.enableSpeedLimiter;
		this->_settingsApplied.executionSpeed		= speedScalar;
	}
	
	pthread_mutex_unlock(&this->_mutexSettingsPendingOnExecutionLoopStart);
	
	if (didFrameSkipSettingChange)
	{
		NDS_OmitFrameSkip(2);
	}
	
	if (didExecutionSpeedChange)
	{
		if (this->_settingsApplied.enableSpeedLimiter)
		{
			this->_settingsApplied.timeBudget = this->GetFrameAbsoluteTime(1.0f/speedScalar);
		}
		else
		{
			this->_settingsApplied.timeBudget = 0;
		}
	}
	
	pthread_mutex_unlock(&this->_mutexSettingsApplyOnExecutionLoopStart);
}

void ClientExecutionControl::FlushSettingsOnNDSExec()
{
	pthread_mutex_lock(&this->_mutexSettingsApplyOnNDSExec);
	
	pthread_mutex_lock(&this->_mutexSettingsPendingOnNDSExec);
	
	this->_settingsApplied.enableAdvancedBusLevelTiming		= this->_settingsPending.enableAdvancedBusLevelTiming;
	this->_settingsApplied.enableRigorous3DRenderingTiming	= this->_settingsPending.enableRigorous3DRenderingTiming;
	this->_settingsApplied.enableExternalBIOS				= this->_settingsPending.enableExternalBIOS;
	this->_settingsApplied.enableBIOSInterrupts				= this->_settingsPending.enableBIOSInterrupts;
	this->_settingsApplied.enableBIOSPatchDelayLoop			= this->_settingsPending.enableBIOSPatchDelayLoop;
	this->_settingsApplied.enableExternalFirmware			= this->_settingsPending.enableExternalFirmware;
	this->_settingsApplied.enableFirmwareBoot				= this->_settingsPending.enableFirmwareBoot;
	this->_settingsApplied.enableDebugConsole				= this->_settingsPending.enableDebugConsole;
	this->_settingsApplied.enableEnsataEmulation			= this->_settingsPending.enableEnsataEmulation;
	
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
	
	pthread_mutex_unlock(&this->_mutexSettingsApplyOnNDSExec);
}

uint64_t ClientExecutionControl::GetFrameAbsoluteTime(double frameTimeScalar)
{
	// Do nothing. This is implementation dependent;
	return 1.0;
}
