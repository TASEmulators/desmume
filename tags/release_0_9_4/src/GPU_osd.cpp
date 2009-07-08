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
#include "GPU.h"
#include "mem.h"
#include <string.h> //mem funcs
#include <stdarg.h> //va_start, etc
#include <time.h>
#include "debug.h"

#include "softrender.h"

#include "softrender_v3sysfont.h"
#include "softrender_desmumefont.h"

using namespace softrender;

image screenshell;

OSDCLASS::OSDCLASS(u8 core)
{
	memset(screen, 0, sizeof(screen));
	memset(name,0,7);

	mode=core;
	offset=0;

	lastLineText=0;
	lineText_x = 5;
	lineText_y = 120;
	lineText_color = render51.MakeColor(255, 255, 255);
	for (int i=0; i < OSD_MAX_LINES+1; i++)
	{
		lineText[i] = new char[1024];
		memset(lineText[i], 0, 1024);
		lineTimer[i] = 0;
		lineColor[i] = lineText_color;
	}

	rotAngle = 0;

	needUpdate = false;

	if (core==0) 
		strcpy(name,"Core A");
	else
	if (core==1)
		strcpy(name,"Core B");
	else
	{
		strcpy(name,"Main");
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

	border(false);

	LOG("OSD_Init (%s)\n",name);
}

OSDCLASS::~OSDCLASS()
{
	LOG("OSD_Deinit (%s)\n",name);

	for (int i=0; i < OSD_MAX_LINES+1; i++)
	{
		if (lineText[i]) 
			delete [] lineText[i];
		lineText[i] = NULL;
	}
}

void OSDCLASS::setOffset(u16 ofs)
{
	offset=ofs;
}

void OSDCLASS::setRotate(u16 angle)
{
	rotAngle = angle;

	switch(rotAngle)
	{
	case 0:
	case 180:
		{
			screenshell.width = 256;
			screenshell.height = 384;
			screenshell.pitch = 256;
			screenshell.cx1 = 0;
			screenshell.cx2 = 255;
			screenshell.cy1 = 0;
			screenshell.cy2 = 383;
		}
		break;
	case 90:
	case 270:
		{
			screenshell.width = 384;
			screenshell.height = 256;
			screenshell.pitch = 384;
			screenshell.cx1 = 0;
			screenshell.cx2 = 383;
			screenshell.cy1 = 0;
			screenshell.cy2 = 255;
		}
		break;
	}
}

void OSDCLASS::clear()
{
	memset(screen, 0, sizeof(screen));
	needUpdate=false;
}

bool OSDCLASS::checkTimers()
{
	if (lastLineText == 0) return false;

	time_t	tmp_time = time(NULL);

	for (int i=0; i < lastLineText; i++)
	{
		if (tmp_time > (lineTimer[i] + OSD_TIMER_SECS) )
		{
			if (i < lastLineText)
			{
				for (int j=i; j < lastLineText; j++)
				{
					strcpy(lineText[j], lineText[j+1]);
					lineTimer[j] = lineTimer[j+1];
					lineColor[j] = lineColor[j+1];
				}
			}
			lineTimer[lastLineText] = 0;
			lastLineText--;
			if (lastLineText == 0) return false;
		}
	}
	return true;
}

void OSDCLASS::update()
{
	if ( (!needUpdate) && (!lastLineText) ) return;	// don't update if buffer empty (speed up)
	if (lastLineText)
	{
		if (checkTimers())
		{
			for (int i=0; i < lastLineText; i++)
			{
				render51.PrintString<DesmumeFont>(1,lineText_x,lineText_y+(i*16),lineColor[i],lineText[i],&screenshell);
			}
		}
		else
		{
			if (!needUpdate) return;
		}
	}

	u16	*dst = (u16*)GPU_screen;

	if (mode!=255)
		dst+=offset*512;

	for (int i=0; i<256*192*2; i++)
	{
		if(screen[i]&0x8000)
			T2WriteWord((u8*)dst,(i << 1), screen[i] );
	}
}

void OSDCLASS::setListCoord(u16 x, u16 y)
{
	lineText_x = x;
	lineText_y = y;
}

void OSDCLASS::setLineColor(u8 r=255, u8 b=255, u8 g=255)
{
	lineText_color = render51.MakeColor(r, g, b);
}

void OSDCLASS::addLine(const char *fmt, ...)
{
	va_list list;

	if (lastLineText > OSD_MAX_LINES) lastLineText = OSD_MAX_LINES;
	if (lastLineText == OSD_MAX_LINES)	// full
	{
		lastLineText--;
		for (int j=0; j < lastLineText; j++)
		{
			strcpy(lineText[j], lineText[j+1]);
			lineTimer[j] = lineTimer[j+1];
			lineColor[j] = lineColor[j+1];
		}
	}

	va_start(list,fmt);
#if defined(_MSC_VER) || defined(__INTEL_COMPILER)
		_vsnprintf(lineText[lastLineText],1023,fmt,list);
#else
		vsnprintf(lineText[lastLineText],1023,fmt,list);
#endif
	va_end(list);
	lineColor[lastLineText] = lineText_color;
	lineTimer[lastLineText] = time(NULL);
	needUpdate = true;

	lastLineText++;
}

void OSDCLASS::addFixed(u16 x, u16 y, const char *fmt, ...)
{
	va_list list;
	char msg[1024];

	va_start(list,fmt);
#if defined(_MSC_VER) || defined(__INTEL_COMPILER)
		_vsnprintf(msg,1023,fmt,list);
#else
		vsnprintf(msg,1023,fmt,list);
#endif
	va_end(list);

	render51.PrintString<DesmumeFont>(1,x,y,render51.MakeColor(255,255,255),msg,&screenshell);

	needUpdate = true;
}

void OSDCLASS::border(bool enabled)
{
	render51.setTextBoxBorder(enabled);
}
