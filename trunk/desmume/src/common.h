/*
	Copyright (C) 2008-2010 DeSmuME team

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

//TODO - dismantle this file

#ifndef _COMMON_H_
#define _COMMON_H_

#include <stdio.h>
#include <string.h>
#include "types.h"
#include <string>

#if defined(WIN32)

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

template<typename T>
T reverseBits(T x)
{
	T h = 0;
	T i = 0;

	for (i = 0; i < sizeof(T)*8; i++)
	{
		h = (h << 1) + (x & 1); 
		x >>= 1; 
	}

	return h;
}

template<typename T>
char *intToBin(T val)
{
	char buf[256] = {0};
	for (int i = sizeof(T)*8, t = 0;  i > 0; --i, t++)
	{
		buf[i-1] = (val & (1<<t))?'1':'0';
	}
	return strdup(buf);
}

extern char *trim(char *s, int len=-1);
extern char *removeSpecialChars(char *s);

// ===============================================================================
// Message dialogs
// ===============================================================================
#define CALL_CONVENTION
typedef struct
{
	void (CALL_CONVENTION*  info)	(const char *fmt, ...);
	bool (CALL_CONVENTION*  confirm)(const char *fmt, ...);
	void (CALL_CONVENTION*  error)	(const char *fmt, ...);
	void (CALL_CONVENTION*  warn)	(const char *fmt, ...);
} msgBoxInterface;

extern msgBoxInterface *msgbox;

// ===============================================================================
// Maker codes
// ===============================================================================

struct MAKER
{
	u16 code;
	const char* name;
};

std::string getDeveloperNameByID(u16 id);


extern int NDS_WritePNG(const char *fname, u8 *data);
extern int NDS_WriteBMP(const char *filename, u8 *data);
extern int NDS_WriteBMP_32bppBuffer(int width, int height, const void* buf, const char *filename);

#endif
