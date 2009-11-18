/*  driver.h

    Copyright (C) 2009 DeSmuME team

    This file is part of DeSmuME

    DeSmuME is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    DeSmuME is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with DeSmuME; if not, write to the Free Software
    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301  USA
*/


#ifndef _DRIVER_H_
#define _DRIVER_H_

#include "types.h"
#include "debug.h"
#include <stdio.h>

//each platform needs to implement this, although it doesnt need to implement any functions
class BaseDriver {
public:
	virtual bool WIFI_Host_InitSystem() { return FALSE; }
	virtual void WIFI_Host_ShutdownSystem() {}

	virtual void AVI_SoundUpdate(void* soundData, int soundLen) {}
	virtual bool AVI_IsRecording() { return FALSE; }
	virtual bool WAV_IsRecording() { return FALSE; }

	virtual void USR_InfoMessage(const char *message) { LOG("%s\n", message); }
	virtual void USR_RefreshScreen() {}
	virtual void USR_SetDisplayPostpone(int milliseconds, bool drawNextFrame) {} // -1 == indefinitely, 0 == don't pospone, 500 == don't draw for 0.5 seconds

	enum eStepMainLoopResult
	{
		ESTEP_NOT_IMPLEMENTED = -1,
		ESTEP_CALL_AGAIN = 0,
		ESTEP_DONE = 1,
	};
	virtual eStepMainLoopResult EMU_StepMainLoop(bool allowSleep, bool allowPause, int frameSkip, bool disableUser, bool disableCore) { return ESTEP_NOT_IMPLEMENTED; } // -1 frameSkip == useCurrentDefault
	virtual void EMU_PauseEmulation(bool pause) {}
	virtual bool EMU_IsEmulationPaused() { return false; }
	virtual bool EMU_IsFastForwarding() { return false; }
	virtual bool EMU_HasEmulationStarted() { return true; }
	virtual bool EMU_IsAtFrameBoundary() { return true; }

	enum eDebug_IOReg
	{
		EDEBUG_IOREG_DMA
	};

	virtual void DEBUG_UpdateIORegView(eDebug_IOReg category) { }
};
extern BaseDriver* driver;

#endif //_DRIVER_H_
