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

#ifndef _CLIENT_EXECUTION_CONTROL_H_
#define _CLIENT_EXECUTION_CONTROL_H_

#include <pthread.h>
#include <string>

#define SPEED_SCALAR_QUARTER						0.25	// Speed scalar for quarter execution speed.
#define SPEED_SCALAR_HALF							0.5		// Speed scalar for half execution speed.
#define SPEED_SCALAR_THREE_QUARTER					0.75	// Speed scalar for three quarters execution speed.
#define SPEED_SCALAR_NORMAL							1.0		// Speed scalar for normal execution speed.
#define SPEED_SCALAR_DOUBLE							2.0		// Speed scalar for double execution speed.
#define SPEED_SCALAR_MIN							0.005	// Lower limit for the speed multiplier.

#define DS_FRAMES_PER_SECOND						59.8261	// Number of DS frames per second.
#define DS_SECONDS_PER_FRAME						(1.0 / DS_FRAMES_PER_SECOND) // The length of time in seconds that, ideally, a frame should be processed within.

#define FRAME_SKIP_AGGRESSIVENESS					9.0		// Must be a value between 0.0 (inclusive) and positive infinity.
															// This value acts as a scalar multiple of the frame skip.
#define FRAME_SKIP_BIAS								0.1		// May be any real number. This value acts as a vector addition to the frame skip.
#define MAX_FRAME_SKIP								(DS_FRAMES_PER_SECOND / 2.98)

enum ExecutionBehavior
{
	ExecutionBehavior_Pause = 0,
	ExecutionBehavior_Run,
	ExecutionBehavior_FrameAdvance,
	ExecutionBehavior_FrameJump
};

enum FrameJumpBehavior
{
	FrameJumpBehavior_Forward		= 0,
	FrameJumpBehavior_ToFrame		= 1,
	FrameJumpBehavior_NextMarker	= 2
};

enum CPUEmulationEngineID
{
	CPUEmulationEngineID_Interpreter			= 0,
	CPUEmulationEngineID_DynamicRecompiler		= 1
};

typedef struct ClientExecutionControlSettings
{
	CPUEmulationEngineID cpuEngineID;
	uint8_t JITMaxBlockSize;
	std::string filePathARM9BIOS;
	std::string filePathARM7BIOS;
	std::string filePathFirmware;
	
	bool enableAdvancedBusLevelTiming;
	bool enableRigorous3DRenderingTiming;
	bool enableGameSpecificHacks;
	bool enableExternalBIOS;
	bool enableBIOSInterrupts;
	bool enableBIOSPatchDelayLoop;
	bool enableExternalFirmware;
	bool enableFirmwareBoot;
	bool enableDebugConsole;
	bool enableEnsataEmulation;
	
	bool enableCheats;
	
	bool enableExecutionSpeedLimiter;
	double executionSpeed;
	
	bool enableFrameSkip;
	uint64_t frameJumpTarget;
	
	ExecutionBehavior execBehavior;
	FrameJumpBehavior jumpBehavior;
	
} ClientExecutionControlSettings;

class ClientExecutionControl
{
private:
	ClientExecutionControlSettings _settingsPending;
	ClientExecutionControlSettings _settingsApplied;
	
	bool _newSettingsPendingOnReset;
	bool _newSettingsPendingOnExecutionLoopStart;
	bool _newSettingsPendingOnNDSExec;
	
	bool _needResetFramesToSkip;
	
	double _frameTime;
	uint8_t _framesToSkip;
	ExecutionBehavior _prevExecBehavior;
	
	pthread_mutex_t _mutexSettingsPendingOnReset;
	pthread_mutex_t _mutexSettingsPendingOnExecutionLoopStart;
	pthread_mutex_t _mutexSettingsPendingOnNDSExec;
	
public:
	ClientExecutionControl();
	~ClientExecutionControl();
	
	CPUEmulationEngineID GetCPUEmulationEngineID();
	void SetCPUEmulationEngineID(CPUEmulationEngineID engineID);
	
	uint8_t GetJITMaxBlockSize();
	void SetJITMaxBlockSize(uint8_t blockSize);
	
	const char* GetARM9ImagePath();
	void SetARM9ImagePath(const char *filePath);
	
	const char* GetARM7ImagePath();
	void SetARM7ImagePath(const char *filePath);
	
	const char* GetFirmwareImagePath();
	void SetFirmwareImagePath(const char *filePath);
	
	bool GetEnableAdvancedBusLevelTiming();
	void SetEnableAdvancedBusLevelTiming(bool enable);
	
	bool GetEnableRigorous3DRenderingTiming();
	void SetEnableRigorous3DRenderingTiming(bool enable);
	
	bool GetEnableGameSpecificHacks();
	void SetEnableGameSpecificHacks(bool enable);
	
	bool GetEnableExternalBIOS();
	void SetEnableExternalBIOS(bool enable);
	
	bool GetEnableBIOSInterrupts();
	void SetEnableBIOSInterrupts(bool enable);
	
	bool GetEnableBIOSPatchDelayLoop();
	void SetEnableBIOSPatchDelayLoop(bool enable);
	
	bool GetEnableExternalFirmware();
	void SetEnableExternalFirmware(bool enable);
	
	bool GetEnableFirmwareBoot();
	void SetEnableFirmwareBoot(bool enable);
	
	bool GetEnableDebugConsole();
	void SetEnableDebugConsole(bool enable);
	
	bool GetEnableEnsataEmulation();
	void SetEnableEnsataEmulation(bool enable);
	
	bool GetEnableCheats();
	void SetEnableCheats(bool enable);
	
	bool GetEnableSpeedLimiter();
	void SetEnableSpeedLimiter(bool enable);
	
	double GetExecutionSpeed();
	void SetExecutionSpeed(double speedScalar);
	
	bool GetEnableFrameSkip();
	bool GetEnableFrameSkipApplied();
	void SetEnableFrameSkip(bool enable);
	
	uint8_t GetFramesToSkip();
	void SetFramesToSkip(uint8_t numFrames);
	void ResetFramesToSkip();
	
	uint64_t GetFrameJumpTarget();
	uint64_t GetFrameJumpTargetApplied();
	void SetFrameJumpTarget(uint64_t newJumpTarget);
	
	ExecutionBehavior GetPreviousExecutionBehavior();
	ExecutionBehavior GetExecutionBehavior();
	ExecutionBehavior GetExecutionBehaviorApplied();
	void SetExecutionBehavior(ExecutionBehavior newBehavior);
	
	FrameJumpBehavior GetFrameJumpBehavior();
	void SetFrameJumpBehavior(FrameJumpBehavior newBehavior);
	
	void ApplySettingsOnReset();
	void ApplySettingsOnExecutionLoopStart();
	void ApplySettingsOnNDSExec();
	
	double GetFrameTime();
	uint8_t CalculateFrameSkip(double startAbsoluteTime, double frameAbsoluteTime);
	
	virtual double GetCurrentAbsoluteTime();
	virtual double CalculateFrameAbsoluteTime(double frameTimeScalar);
	virtual void WaitUntilAbsoluteTime(double deadlineAbsoluteTime);
};

#endif // _CLIENT_EXECUTION_CONTROL_H_
