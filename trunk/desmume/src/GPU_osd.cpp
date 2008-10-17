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

#include "softrender.h"

#include "softrender_v3sysfont.h"
#include "softrender_desmumefont.h"

using namespace softrender;

extern u8 GPU_screen[4*256*192];
image screenshell;

OSDCLASS::OSDCLASS(u8 core)
{
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

	screenshell.shell = true;
	screenshell.data = screen;
	screenshell.bpp = 15;
	screenshell.width = 256;
	screenshell.height = 384;
	screenshell.pitch = 256;
	screenshell.cx1 = 0;
	screenshell.cx2 = 256-1;
	screenshell.cy1 = 0;
	screenshell.cy2 = 384-1;

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

void OSDCLASS::update() // don't optimized
{
	if (!needUpdate) return;	// don't update if buffer empty (speed up)
	u16	*dst=(u16*)GPU_screen;
	if (mode!=255)
		dst+=offset*512;

	for (int i=0; i<256*192; i++)
	{
		if(screen[i]&0x8000)
			T2WriteWord((u8*)dst,(i << 1), screen[i] );
	}
}

void OSDCLASS::addLine(const char *fmt, ...)
{

}

void OSDCLASS::addFixed(u16 x, u16 y, const char *fmt, ...)
{
	va_list list;
	char msg[1024];

//	memset(msg,0,1024);

	va_start(list,fmt);
#if defined(_MSC_VER) || defined(__INTEL_COMPILER)
		_vsnprintf(msg,1023,fmt,list);
#else
		vsnprintf(msg,1023,fmt,list);
#endif

	va_end(list);

	if (strcmp(msg, old_msg) == 0) return;

	render51.PrintString<DesmumeFont>(1,0,0,render51.MakeColor(128,0,0),msg,&screenshell);

	needUpdate = true;
}
