/* 	NDS Firmware tools

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

#if defined(_MSC_VER)
#pragma warning(disable: 4995)
#pragma warning(disable: 4996)
#endif

typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned int u32;
typedef unsigned __int64 u64;
typedef signed char s8;
typedef signed short s16;
typedef signed int s32;
typedef __int64 s64;

char __forceinline *hex2(const u32 val)
{
	char buf[30] = {0};
	sprintf(buf, "0x%02X", val);
	return strdup(buf);
}

char __forceinline *hex2w(const u32 val)
{
	char buf[30] = {0};
	sprintf(buf, "%02X", val);
	return strdup(buf);
}

char __forceinline *hex4(const u32 val)
{
	char buf[30] = {0};
	sprintf(buf, "0x%04X", val);
	return strdup(buf);
}

char __forceinline *hex4w(const u32 val)
{
	char buf[30] = {0};
	sprintf(buf, "%04X", val);
	return strdup(buf);
}

char __forceinline *hex6(const u32 val)
{
	char buf[30] = {0};
	sprintf(buf, "0x%06X", val);
	return strdup(buf);
}

char __forceinline *hex6w(const u32 val)
{
	char buf[30] = {0};
	sprintf(buf, "%06X", val);
	return strdup(buf);
}

char __forceinline *hex8(const u32 val)
{
	char buf[30] = {0};
	sprintf(buf, "0x%08X", val);
	return strdup(buf);
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

u8 __forceinline read8(u8 *mem, u32 offset)
{
	return (*(u8*)(mem + offset));
}

u16 __forceinline read16(u8 *mem, u32 offset)
{
	return (*(u16*)(mem + offset));
}

u32 __forceinline read24(u8 *mem, u32 offset)
{
	return (*(u16*)(mem + offset)) | (*(u8*)(mem + offset+2) << 16);
}

u32 __forceinline read32(u8 *mem, u32 offset)
{
	return (*(u32*)(mem + offset));
}

u64 __forceinline read64(u8 *mem, u32 offset)
{
	return (*(u64*)(mem + offset));
}

#endif
