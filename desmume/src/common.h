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
#include <string>

extern const u8	logo_data[156];

#if defined(WIN32) && !defined(_XBOX)

	#include <winsock2.h>
	#include <windows.h>



	#define CLASSNAME "DeSmuME"

	extern HINSTANCE hAppInst;

	extern bool romloaded;

	extern char IniName[MAX_PATH];
	extern void GetINIPath();
	extern void WritePrivateProfileInt(char* appname, char* keyname, int val, char* file);

	bool GetPrivateProfileBool(const char* appname, const char* keyname, bool defval, const char* filename);
	void WritePrivateProfileBool(char* appname, char* keyname, bool val, char* file);

#else		// non Windows

#define sscanf_s sscanf

#endif

extern u8	reverseBitsInByte(u8 x);
extern char *trim(char *s);
extern char *removeSpecialChars(char *s);

#endif

