/*
	Copyright (C) 2009-2015 DeSmuME team

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
#ifdef HOST_WINDOWS
	virtual bool WIFI_SocketsAvailable() { return true; }
	virtual bool WIFI_PCapAvailable() { return false; }

	virtual void WIFI_GetUniqueMAC(u8* mac) {}

	virtual bool WIFI_WFCWarning() { return false; }

	virtual int PCAP_findalldevs(pcap_if_t** alldevs, char* errbuf) { return -1; }
	virtual void PCAP_freealldevs(pcap_if_t* alldevs) {}
	virtual pcap_t* PCAP_open(const char* source, int snaplen, int flags, int readtimeout, char* errbuf) { return NULL; }
	virtual void PCAP_close(pcap_t* dev) {}
	virtual int PCAP_setnonblock(pcap_t* dev, int nonblock, char* errbuf) { return -1; }
	virtual int PCAP_sendpacket(pcap_t* dev, const u_char* data, int len) { return -1; }
	virtual int PCAP_dispatch(pcap_t* dev, int num, pcap_handler callback, u_char* userdata) { return -1; }
#else
	virtual bool WIFI_SocketsAvailable() { return true; }
	virtual bool WIFI_PCapAvailable() { return true; }

	virtual void WIFI_GetUniqueMAC(u8* mac) {}

	virtual bool WIFI_WFCWarning() { return false; }

	virtual int PCAP_findalldevs(pcap_if_t** alldevs, char* errbuf) {
		return pcap_findalldevs(alldevs, errbuf); }

	virtual void PCAP_freealldevs(pcap_if_t* alldevs) {
		pcap_freealldevs(alldevs); }

	virtual pcap_t* PCAP_open(const char* source, int snaplen, int flags, int readtimeout, char* errbuf) {
		return pcap_open_live(source, snaplen, flags, readtimeout, errbuf); }

	virtual void PCAP_close(pcap_t* dev) {
		pcap_close(dev); }

	virtual int PCAP_setnonblock(pcap_t* dev, int nonblock, char* errbuf) {
		return pcap_setnonblock(dev, nonblock, errbuf); }

	virtual int PCAP_sendpacket(pcap_t* dev, const u_char* data, int len) {
		return pcap_sendpacket(dev, data, len); }

	virtual int PCAP_dispatch(pcap_t* dev, int num, pcap_handler callback, u_char* userdata) {
		return pcap_dispatch(dev, num, callback, userdata); }
#endif
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
		ESTEP_CALL_AGAIN      = 0,
		ESTEP_DONE            = 1
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

	virtual void AddLine(const char *fmt, ...);
	virtual void SetLineColor(u8 r, u8 b, u8 g);
};
extern BaseDriver* driver;

#endif //_DRIVER_H_
