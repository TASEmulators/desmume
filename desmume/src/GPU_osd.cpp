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

#include "GPU_osd.h"
#include "mem.h"
#include <string.h> //mem funcs
#include <stdarg.h> //va_start, etc

extern u8 GPU_screen[4*256*192];

#include "font_eng.inc"

OSDCLASS::OSDCLASS(u8 core)
{
	int i;

	memset(screen, 0, sizeof(screen));
	memset(name,0,7);
	//memset(line, 0, sizeof(line));
	memset(timer, 0, sizeof(timer));
	memset(color, 0, sizeof(color));

	old_msg = new char[512];
	memset(old_msg, 0, 512);
	
	current_color = 0x8F;
	mode=core;
	offset=0;
	startline=0;
	lastline=0;

	needUpdate = false;

	if (core==0) 
		memcpy(name,"Core A",6);
	else
	if (core==1)
		memcpy(name,"Core B",6);
	else
	{
		memcpy(name,"Main",6);
		mode=255;
	}
	

	LOG("OSD_Init (%s)\n",name);
}

OSDCLASS::~OSDCLASS()
{
	LOG("OSD_Deinit (%s)\n",name);

	delete[] old_msg;
}

void OSDCLASS::setOffset(u16 ofs)
{
	offset=ofs;
}

void OSDCLASS::clear()
{
	memset(screen, 0, sizeof(screen));
	memset(line, 0, sizeof(line));
	memset(timer, 0, sizeof(timer));
	needUpdate=false;
}

void OSDCLASS::setColor(u16 col)
{
	current_color = col;
}

void INLINE OSDCLASS::printChar(u16 x, u16 y, u8 c)
{
	int i, j;
	int ofs=c*OSD_FONT_HEIGHT;
	unsigned char	bits[9]={256, 128, 64, 32, 16, 8, 4, 2, 1};
	u8	*dst=screen;
	dst+=(y*256)+x;

	for (i = 0; i < OSD_FONT_HEIGHT; i++)
	{
		for (j = 0; j < OSD_FONT_WIDTH; j++)
			if (font_eng[ofs] & bits[j]) dst[j]=1;
			else dst[j]=0;
		dst+=256;
		ofs++;
	}
}

void OSDCLASS::update() // don't optimized
{
	if (!needUpdate) return;	// don't update if buffer empty (speed up)
	u8	*dst=GPU_screen;
	if (mode!=255)
		dst+=offset*512;

	for (int i=0; i<256*192; i++)
	{
		if (screen[i])
		{
			T2WriteWord(dst,(i << 1), current_color );
		}
	}
}

void OSDCLASS::addLine(const char *fmt, ...)
{

}

void OSDCLASS::addFixed(u16 x, u16 y, const char *fmt, ...)
{
	va_list list;
	char msg[512];

//	memset(msg,0,512);

	va_start(list,fmt);
#if defined(_MSC_VER) || defined(__INTEL_COMPILER)
		_vsnprintf(msg,511,fmt,list);
#else
		vsnprintf(msg,511,fmt,list);
#endif

	va_end(list);

	int len=strlen(msg);
	if (strcmp(msg, old_msg) == 0) return;

	for (int i=0; i<len; i++)
	{
		printChar(x, y, msg[i]);
		x+=OSD_FONT_WIDTH+2;
		old_msg[i]=msg[i];
	}
	old_msg[511]=0;
	needUpdate = true;
}
