/* Cheats converter

	Copyright 2009 DeSmuME team

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

#ifndef _COMMON_H__
#define _COMMON_H__

#ifdef _MSC_VER
#pragma warning(disable: 4995)
#pragma warning(disable: 4996)
#endif

#ifdef _WIN32
#include <windows.h>
#endif

#include <ctype.h>
#include <string.h>


typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned int u32;
typedef unsigned __int64 u64;
typedef signed char s8;
typedef signed short s16;
typedef signed int s32;
typedef __int64 s64;

static char *trim(char *s)
{
	char *ptr = NULL;
	if (!s) return NULL;
	if (!*s) return s;
	
	for (ptr = s + strlen(s) - 1; (ptr >= s) && isspace(*ptr); ptr--);
	ptr[1] = '\0';
	return s;
}

#ifdef _WIN32
char __forceinline *error() 
{ 
	LPVOID lpMsgBuf = NULL;

    FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
       NULL, GetLastError(), MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPTSTR)&lpMsgBuf, 512, NULL);

	return strdup((char*)lpMsgBuf);
}
#endif

#endif
