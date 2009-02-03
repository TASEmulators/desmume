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

#ifdef WIN32

	#define _WINSOCKAPI_
	#include <windows.h>

	#define CLASSNAME "DeSmuME"

	extern HINSTANCE hAppInst;

	extern BOOL romloaded;

	extern char IniName[MAX_PATH];
	extern void GetINIPath();
	extern void WritePrivateProfileInt(char* appname, char* keyname, int val, char* file);

	#define EXPERIMENTAL_GBASLOT 1
	// this is experimental and only tested for windows port 
	// (I can`t test on another ports) 
	// 
	// About GBA game in slot (only for use together with NDS + GBA): 
	// in real BIOS 9 at offset 0x0020 placed compressed logo 
	// for comparing with logo in header GBA cartridge. 
	// so, GBA game in slot work now.
	// Later need make loading this table in BIOS memory (from gba_header_data_0x04)
	//
	// in windows ports:
	// i added in menu "Emulation" submenu "GBA slot"
#endif

extern u8 reverseBitsInByte(u8 x);
	
#endif

