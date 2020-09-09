/*
    Copyright (C) 2010-2018 DeSmuME team

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

#ifndef WINPCAP_H
#define WINPCAP_H

#ifndef HAVE_REMOTE
	#define HAVE_REMOTE
#endif

#ifndef WPCAP
	#define WPCAP
#endif

#include <pcap.h>

typedef int (__cdecl *T_pcap_findalldevs)(pcap_if_t** alldevs, char* errbuf);
typedef void (__cdecl *T_pcap_freealldevs)(pcap_if_t* alldevs);
typedef pcap_t* (__cdecl *T_pcap_open_live)(const char* source, int snaplen, int flags, int readtimeout, char* errbuf);
typedef void (__cdecl *T_pcap_close)(pcap_t* dev);
typedef int (__cdecl *T_pcap_setnonblock)(pcap_t* dev, int nonblock, char* errbuf);
typedef int (__cdecl *T_pcap_sendpacket)(pcap_t* dev, const u_char* data, int len);
typedef int (__cdecl *T_pcap_dispatch)(pcap_t* dev, int num, pcap_handler callback, u_char* userdata);
typedef int (__cdecl *T_pcap_breakloop)(pcap_t* dev);

T_pcap_findalldevs _pcap_findalldevs = NULL;
T_pcap_freealldevs _pcap_freealldevs = NULL;
T_pcap_open_live _pcap_open_live = NULL;
T_pcap_close _pcap_close = NULL;
T_pcap_setnonblock _pcap_setnonblock = NULL;
T_pcap_sendpacket _pcap_sendpacket = NULL;
T_pcap_dispatch _pcap_dispatch = NULL;
T_pcap_breakloop _pcap_breakloop = NULL;


#define LOADSYMBOL(name) \
	_##name = (T_##name)GetProcAddress(wpcap, #name); \
	if (_##name == NULL) return;


static void LoadWinPCap(bool &outResult)
{
	bool result = false;

	HMODULE wpcap = LoadLibrary("wpcap.dll");
	if (wpcap == NULL)
	{
		outResult = result;
		return;
	}

	LOADSYMBOL(pcap_findalldevs);
	LOADSYMBOL(pcap_freealldevs);
	LOADSYMBOL(pcap_open_live);
	LOADSYMBOL(pcap_close);
	LOADSYMBOL(pcap_setnonblock);
	LOADSYMBOL(pcap_sendpacket);
	LOADSYMBOL(pcap_dispatch);
	LOADSYMBOL(pcap_breakloop);

	result = true;
	outResult = result;
}

#endif
