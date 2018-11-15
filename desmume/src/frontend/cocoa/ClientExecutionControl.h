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

#ifndef _CLIENT_EXECUTION_CONTROL_H_
#define _CLIENT_EXECUTION_CONTROL_H_

#include <pthread.h>
#include <map>
#include <string>

#include "ClientInputHandler.h"

#include "../../slot1.h"

#ifdef BOOL
#undef BOOL
#endif

#define SPEED_SCALAR_QUARTER						0.25		// Speed scalar for quarter execution speed.
#define SPEED_SCALAR_HALF							0.5			// Speed scalar for half execution speed.
#define SPEED_SCALAR_THREE_QUARTER					0.75		// Speed scalar for three quarters execution speed.
#define SPEED_SCALAR_NORMAL							1.0			// Speed scalar for normal execution speed.
#define SPEED_SCALAR_DOUBLE							2.0			// Speed scalar for double execution speed.
#define SPEED_SCALAR_MIN							0.005		// Lower limit for the speed multiplier.

#define DS_FRAMES_PER_SECOND						59.8261		// Number of DS frames per second.
#define DS_SECONDS_PER_FRAME						(1.0 / DS_FRAMES_PER_SECOND) // The length of time in seconds that, ideally, a frame should be processed within.

#define FRAME_SKIP_AGGRESSIVENESS					1.0			// Must be a value between 0.0 (inclusive) and positive infinity.
																// This value acts as a scalar multiple of the frame skip.
#define FRAME_SKIP_BIAS								0.9			// May be any real number. This value acts as a vector addition to the frame skip.
#define MAX_FRAME_SKIP								(DS_FRAMES_PER_SECOND / 2.98)

#define EXECUTION_WAIT_BIAS_MIN						0.70
#define EXECUTION_WAIT_BIAS_MAX						1.10

class ClientAVCaptureObject;

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

struct ClientExecutionControlSettings
{
	CPUEmulationEngineID cpuEngineID;
	uint8_t JITMaxBlockSize;
	NDS_SLOT1_TYPE slot1DeviceType;
	std::string filePathARM9BIOS;
	std::string filePathARM7BIOS;
	std::string filePathFirmware;
	std::string filePathSlot1R4;
	
	FirmwareConfig fwConfig;
	
	std::string cpuEmulationEngineName;
	std::string slot1DeviceName;
	
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
	
	int wifiEmulationMode;
	int wifiBridgeDeviceIndex;
	
	bool enableCheats;
	
	bool enableExecutionSpeedLimiter;
	double executionSpeed;
	
	bool enableFrameSkip;
	uint64_t frameJumpRelativeTarget;
	uint64_t frameJumpTarget;
	
	ExecutionBehavior execBehavior;
	FrameJumpBehavior jumpBehavior;
	
	ClientAVCaptureObject *avCaptureObject;
};

struct NDSFrameInfo
{
	std::string cpuEmulationEngineName;
	std::string slot1DeviceName;
	std::string rtcString;
	
	uint64_t frameIndex;
	uint32_t render3DFPS;
	uint32_t lagFrameCount;
	uint32_t cpuLoadAvgARM9;
	uint32_t cpuLoadAvgARM7;
	double executionSpeed;
	
	NDSInputState inputStatesPending;
	uint8_t touchLocXPending;
	uint8_t touchLocYPending;
	uint8_t touchPressurePending;
	int16_t paddleValuePending;
	int16_t paddleAdjustPending;
	
	NDSInputState inputStatesApplied;
	uint8_t touchLocXApplied;
	uint8_t touchLocYApplied;
	uint8_t touchPressureApplied;
	int16_t paddleValueApplied;
	int16_t paddleAdjustApplied;
	
	void clear()
	{
		this->cpuEmulationEngineName			= std::string();
		this->slot1DeviceName					= std::string();
		this->rtcString							= std::string();
		
		this->frameIndex						= 0;
		this->render3DFPS						= 0;
		this->lagFrameCount						= 0;
		this->cpuLoadAvgARM9					= 0;
		this->cpuLoadAvgARM7					= 0;
		this->executionSpeed					= 0.0;
		
		this->inputStatesPending.value			= INPUT_STATES_CLEAR_VALUE;
		this->touchLocXPending					= 0;
		this->touchLocYPending					= 0;
		this->touchPressurePending				= 50;
		this->paddleValuePending				= 0;
		this->paddleAdjustPending				= 0;
		
		this->inputStatesApplied.value			= INPUT_STATES_CLEAR_VALUE;
		this->touchLocXApplied					= 0;
		this->touchLocYApplied					= 0;
		this->touchPressureApplied				= 50;
		this->paddleValueApplied				= 0;
		this->paddleAdjustApplied				= 0;
	}
	
	void copyFrom(const NDSFrameInfo &fromObject)
	{
		this->cpuEmulationEngineName			= fromObject.cpuEmulationEngineName;
		this->slot1DeviceName					= fromObject.slot1DeviceName;
		this->rtcString							= fromObject.rtcString;
		
		this->frameIndex						= fromObject.frameIndex;
		this->render3DFPS						= fromObject.render3DFPS;
		this->lagFrameCount						= fromObject.lagFrameCount;
		this->cpuLoadAvgARM9					= fromObject.cpuLoadAvgARM9;
		this->cpuLoadAvgARM7					= fromObject.cpuLoadAvgARM7;
		this->executionSpeed					= fromObject.executionSpeed;
		
		this->inputStatesPending				= fromObject.inputStatesPending;
		this->touchLocXPending					= fromObject.touchLocXPending;
		this->touchLocYPending					= fromObject.touchLocYPending;
		this->touchPressurePending				= fromObject.touchPressurePending;
		this->paddleValuePending				= fromObject.paddleValuePending;
		this->paddleAdjustPending				= fromObject.paddleAdjustPending;
		
		this->inputStatesApplied				= fromObject.inputStatesApplied;
		this->touchLocXApplied					= fromObject.touchLocXApplied;
		this->touchLocYApplied					= fromObject.touchLocYApplied;
		this->touchPressureApplied				= fromObject.touchPressureApplied;
		this->paddleValueApplied				= fromObject.paddleValueApplied;
		this->paddleAdjustApplied				= fromObject.paddleAdjustApplied;
	}
};

typedef void *gdbstub_handle_t;

class ClientExecutionControl
{
protected:
	ClientInputHandler *_inputHandler;
	
	ClientExecutionControlSettings _settingsPending;
	ClientExecutionControlSettings _settingsApplied;
	
	NDSFrameInfo _ndsFrameInfo;
	
	bool _newSettingsPendingOnReset;
	bool _newSettingsPendingOnExecutionLoopStart;
	bool _newSettingsPendingOnNDSExec;
	
	bool _needResetFramesToSkip;
	
	double _frameTime;
	uint8_t _framesToSkip;
	double _lastSetFrameSkip;
	size_t _unskipStep;
	size_t _dynamicBiasStep;
	ExecutionBehavior _prevExecBehavior;
	
	bool _isGdbStubStarted;
	bool _enableGdbStubARM9;
	bool _enableGdbStubARM7;
	uint16_t _gdbStubPortARM9;
	uint16_t _gdbStubPortARM7;
	volatile gdbstub_handle_t _gdbStubHandleARM9;
	volatile gdbstub_handle_t _gdbStubHandleARM7;
	bool _isInDebugTrap;
	
	std::string _cpuEmulationEngineNameOut;
	std::string _slot1DeviceNameOut;
	std::string _rtcStringOut;
	
	pthread_mutex_t _mutexSettingsPendingOnReset;
	pthread_mutex_t _mutexSettingsPendingOnExecutionLoopStart;
	pthread_mutex_t _mutexSettingsPendingOnNDSExec;
	pthread_mutex_t _mutexOutputPostNDSExec;
	
public:
	ClientExecutionControl();
	~ClientExecutionControl();
	
	ClientAVCaptureObject* GetClientAVCaptureObject();
	ClientAVCaptureObject* GetClientAVCaptureObjectApplied();
	void SetClientAVCaptureObject(ClientAVCaptureObject *theCaptureObject);
	
	ClientInputHandler* GetClientInputHandler();
	void SetClientInputHandler(ClientInputHandler *inputHandler);
	
	CPUEmulationEngineID GetCPUEmulationEngineID();
	const char* GetCPUEmulationEngineName();
	void SetCPUEmulationEngineByID(CPUEmulationEngineID engineID);
	
	uint8_t GetJITMaxBlockSize();
	void SetJITMaxBlockSize(uint8_t blockSize);
	
	NDS_SLOT1_TYPE GetSlot1DeviceType();
	const char* GetSlot1DeviceName();
	void SetSlot1DeviceByType(NDS_SLOT1_TYPE type);
	
	const char* GetARM9ImagePath();
	void SetARM9ImagePath(const char *filePath);
	
	const char* GetARM7ImagePath();
	void SetARM7ImagePath(const char *filePath);
	
	const char* GetFirmwareImagePath();
	void SetFirmwareImagePath(const char *filePath);
	
	const char* GetSlot1R4Path();
	void SetSlot1R4Path(const char *filePath);
	
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
	
	FirmwareConfig GetFirmwareConfig();
	FirmwareConfig GetFirmwareConfigApplied();
	void SetFirmwareConfig(const FirmwareConfig &inConfig);
	
	bool GetEnableExternalFirmware();
	void SetEnableExternalFirmware(bool enable);
	
	bool GetEnableFirmwareBoot();
	void SetEnableFirmwareBoot(bool enable);
	
	bool GetEnableDebugConsole();
	void SetEnableDebugConsole(bool enable);
	
	bool GetEnableEnsataEmulation();
	void SetEnableEnsataEmulation(bool enable);
	
	int GetWifiEmulationMode();
	void SetWifiEmulationMode(int wifiEmulationMode);
	
	int GetWifiBridgeDeviceIndex();
	void SetWifiBridgeDeviceIndex(int wifiBridgeDeviceIndex);
	
	uint8_t* GetCurrentSessionMACAddress();
	
	bool GetEnableCheats();
	void SetEnableCheats(bool enable);
	
	bool GetEnableSpeedLimiter();
	bool GetEnableSpeedLimiterApplied();
	void SetEnableSpeedLimiter(bool enable);
	
	double GetExecutionSpeed();
	double GetExecutionSpeedApplied();
	void SetExecutionSpeed(double speedScalar);
	
	bool GetEnableFrameSkip();
	bool GetEnableFrameSkipApplied();
	void SetEnableFrameSkip(bool enable);
	
	uint8_t GetFramesToSkip();
	void SetFramesToSkip(uint8_t numFrames);
	void ResetFramesToSkip();
	
	uint64_t GetFrameJumpRelativeTarget();
	void SetFrameJumpRelativeTarget(uint64_t newRelativeTarget);
	
	uint64_t GetFrameJumpTarget();
	uint64_t GetFrameJumpTargetApplied();
	void SetFrameJumpTarget(uint64_t newJumpTarget);
	
	bool IsGDBStubARM9Enabled();
	void SetGDBStubARM9Enabled(bool theState);
	bool IsGDBStubARM7Enabled();
	void SetGDBStubARM7Enabled(bool theState);
	
	uint16_t GetGDBStubARM9Port();
	void SetGDBStubARM9Port(uint16_t portNumber);
	uint16_t GetGDBStubARM7Port();
	void SetGDBStubARM7Port(uint16_t portNumber);
	
	bool IsGDBStubStarted();
	void SetIsGDBStubStarted(bool theState);
	bool IsInDebugTrap();
	void SetIsInDebugTrap(bool theState);
	
	ExecutionBehavior GetPreviousExecutionBehavior();
	ExecutionBehavior GetExecutionBehavior();
	ExecutionBehavior GetExecutionBehaviorApplied();
	void SetExecutionBehavior(ExecutionBehavior newBehavior);
	
	FrameJumpBehavior GetFrameJumpBehavior();
	void SetFrameJumpBehavior(FrameJumpBehavior newBehavior);
	
	void ApplySettingsOnReset();
	void ApplySettingsOnExecutionLoopStart();
	void ApplySettingsOnNDSExec();
	
	void FetchOutputPostNDSExec();
	const NDSFrameInfo& GetNDSFrameInfo();
	void SetFrameInfoExecutionSpeed(double executionSpeed);
	uint64_t GetFrameIndex();
	
	double GetFrameTime();
	uint8_t CalculateFrameSkip(double startAbsoluteTime, double frameAbsoluteTime);
	
	virtual double GetCurrentAbsoluteTime();
	virtual double CalculateFrameAbsoluteTime(double frameTimeScalar);
	virtual void WaitUntilAbsoluteTime(double deadlineAbsoluteTime);
};

#endif // _CLIENT_EXECUTION_CONTROL_H_
