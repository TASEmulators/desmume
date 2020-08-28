/*
	Copyright (C) 2009-2018 DeSmuME team

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

#ifndef _DRIVER_H_
#define _DRIVER_H_

#include <stdio.h>
#include "types.h"
#include "debug.h"


class VIEW3D_Driver
{
public:
	virtual void Launch() {}
	virtual void NewFrame() {}
	virtual bool IsRunning() { return false; }
};

//each platform needs to implement this, although it doesnt need to implement any functions
class BaseDriver {
public:
	BaseDriver();
	~BaseDriver();
	
	virtual void AVI_SoundUpdate(void* soundData, int soundLen) {}
	virtual bool AVI_IsRecording() { return FALSE; }
	virtual bool WAV_IsRecording() { return FALSE; }

	virtual void USR_InfoMessage(const char *message) { LOG("%s\n", message); }
	virtual void USR_RefreshScreen() {}
	virtual void USR_SetDisplayPostpone(int milliseconds, bool drawNextFrame) {} // -1 == indefinitely, 0 == don't pospone, 500 == don't draw for 0.5 seconds

	enum eStepMainLoopResult
	{
		ESTEP_NOT_IMPLEMENTED = -1,
		ESTEP_CALL_AGAIN      = 0,
		ESTEP_DONE            = 1
	};
	virtual eStepMainLoopResult EMU_StepMainLoop(bool allowSleep, bool allowPause, int frameSkip, bool disableUser, bool disableCore) { return ESTEP_NOT_IMPLEMENTED; } // -1 frameSkip == useCurrentDefault
	virtual void EMU_PauseEmulation(bool pause) {}
	virtual bool EMU_IsEmulationPaused() { return false; }
	virtual bool EMU_IsFastForwarding() { return false; }
	virtual bool EMU_HasEmulationStarted() { return true; }
	virtual bool EMU_IsAtFrameBoundary() { return true; }
	
	virtual void EMU_DebugIdleEnter() {}
	virtual void EMU_DebugIdleUpdate() {}
	virtual void EMU_DebugIdleWakeUp() {}

	enum eDebug_IOReg
	{
		EDEBUG_IOREG_DMA
	};

	virtual void DEBUG_UpdateIORegView(eDebug_IOReg category) { }

	VIEW3D_Driver* view3d;
	void VIEW3D_Shutdown();
	void VIEW3D_Init();

	virtual void AddLine(const char *fmt, ...);
	virtual void SetLineColor(u8 r, u8 b, u8 g);
};
extern BaseDriver* driver;

#endif //_DRIVER_H_
