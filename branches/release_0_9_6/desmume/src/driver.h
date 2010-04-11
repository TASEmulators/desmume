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

#ifdef EXPERIMENTAL_WIFI_COMM
#include <pcap.h>
#endif

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

#ifdef EXPERIMENTAL_WIFI_COMM
	virtual bool WIFI_SocketsAvailable() { return true; }
	virtual bool WIFI_PCapAvailable() { return false; }

	virtual int PCAP_findalldevs(pcap_if_t** alldevs, char* errbuf) { return -1; }
	virtual void PCAP_freealldevs(pcap_if_t* alldevs) {}
	virtual pcap_t* PCAP_open(const char* source, int snaplen, int flags, int readtimeout, char* errbuf) { return NULL; }
	virtual void PCAP_close(pcap_t* dev) {}
	virtual int PCAP_setnonblock(pcap_t* dev, int nonblock, char* errbuf) { return -1; }
	virtual int PCAP_sendpacket(pcap_t* dev, const u_char* data, int len) { return -1; }
	virtual int PCAP_dispatch(pcap_t* dev, int num, pcap_handler callback, u_char* userdata) { return -1; }
#endif

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
	virtual void EMU_DebugIdleUpdate() {}

	enum eDebug_IOReg
	{
		EDEBUG_IOREG_DMA
	};

	virtual void DEBUG_UpdateIORegView(eDebug_IOReg category) { }

	VIEW3D_Driver* view3d;
	void VIEW3D_Shutdown();
	void VIEW3D_Init();
};
extern BaseDriver* driver;

#endif //_DRIVER_H_
