/*  Copyright (C) 2006 yopyop
    yopyop156@ifrance.com
    yopyop156.ifrance.com

    Copyright (C) 2006-2008 DeSmuME team

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
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

#ifndef _COMMON_H_
#define _COMMON_H_

#include <stdio.h>
#include "types.h"

extern u8	gba_header_data_0x04[156];
const char* GetRomName();	//adelikat: return the name of the Rom currently loaded
void SetRomName(const char *filename);


#ifdef WIN32

	#include <winsock2.h>
	#include <windows.h>

	#define IDT_VIEW_DISASM7						50001
	#define IDT_VIEW_DISASM9                		50002
	#define IDT_VIEW_MEM7                   		50003
	#define IDT_VIEW_MEM9                   		50004
	#define IDT_VIEW_IOREG                  		50005
	#define IDT_VIEW_PAL                    		50006
	#define IDT_VIEW_TILE                   		50007
	#define IDT_VIEW_MAP                    		50008
	#define IDT_VIEW_OAM                    		50009
	#define IDT_VIEW_MATRIX                 		50010
	#define IDT_VIEW_LIGHTS                 		50011
	#define IDM_EXEC								50112

	#define IDM_RECENT_RESERVED0                    65500
	#define IDM_RECENT_RESERVED1                    65501
	#define IDM_RECENT_RESERVED2                    65502
	#define IDM_RECENT_RESERVED3                    65503
	#define IDM_RECENT_RESERVED4                    65504
	#define IDM_RECENT_RESERVED5                    65505
	#define IDM_RECENT_RESERVED6                    65506
	#define IDM_RECENT_RESERVED7                    65507
	#define IDM_RECENT_RESERVED8                    65508
	#define IDM_RECENT_RESERVED9                    65509
	#define IDM_RECENT_RESERVED10                   65510
	#define IDM_RECENT_RESERVED11                   65511
	#define IDM_RECENT_RESERVED12                   65512
	#define IDM_RECENT_RESERVED13                   65513

	#define CLASSNAME "DeSmuME"

	extern HINSTANCE hAppInst;

	extern BOOL romloaded;

	extern char IniName[MAX_PATH];
	extern void GetINIPath();
	extern void WritePrivateProfileInt(char* appname, char* keyname, int val, char* file);

	#define EXPERIMENTAL_GBASLOT 1
#else		// non Windows

#define sscanf_s sscanf

#endif

extern u8	reverseBitsInByte(u8 x);
extern void	removeCR(char *buf);
extern u32	strlen_ws(char *buf);

#endif

