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

#include "aggdraw.h"
#include "movie.h"
#include "NDSSystem.h"

OSDCLASS	*osd = NULL;

OSDCLASS::OSDCLASS(u8 core)
{
	memset(name,0,7);

	mode=core;
	offset=0;

	lastLineText=0;
	lineText_x = 5;
	lineText_y = 120;
	lineText_color = AggColor(255, 255, 255);
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

	//border(false);

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
}

void OSDCLASS::clear()
{
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
				aggDraw.hud->lineColor(lineColor[i]);
				aggDraw.hud->renderTextDropshadowed(lineText_x,lineText_y+(i*16),lineText[i]);
			}
		}
		else
		{
			if (!needUpdate) return;
		}
	}
}

void OSDCLASS::setListCoord(u16 x, u16 y)
{
	lineText_x = x;
	lineText_y = y;
}

void OSDCLASS::setLineColor(u8 r=255, u8 b=255, u8 g=255)
{
	lineText_color = AggColor(r,g,b);
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
	vsnprintf(msg,1023,fmt,list);
	va_end(list);

	aggDraw.hud->lineColor(255,255,255);
	aggDraw.hud->renderTextDropshadowed(x,y,msg);

	needUpdate = true;
}

void OSDCLASS::border(bool enabled)
{
	//render51.setTextBoxBorder(enabled);
}

struct TouchInfo{
	u16 X;
	u16 Y;
};
static int touchalpha[8]= {31, 63, 95, 127, 159, 191, 223, 255};
static TouchInfo temptouch;
bool touchshadow = true;
static std::vector<TouchInfo> touch (8);

void OSDCLASS::TouchDisplay(){

	if(!ShowInputDisplay) return;

	aggDraw.hud->lineWidth(1.0);

	temptouch.X = nds.touchX >> 4;
	temptouch.Y = nds.touchY >> 4;
	touch.push_back(temptouch);

	if(touch.size() > 8) touch.erase(touch.begin());

	if(touchshadow) {
		for (int i = 0; i < 8; i++) {
			temptouch = touch[i];
			if(temptouch.X != 0 || temptouch.Y != 0) {
				aggDraw.hud->lineColor(0, 255, 0, touchalpha[i]);
				aggDraw.hud->line(temptouch.X - 256, temptouch.Y + 192, temptouch.X + 256, temptouch.Y + 192); //horiz
				aggDraw.hud->line(temptouch.X, temptouch.Y - 256, temptouch.X, temptouch.Y + 384); //vert
				aggDraw.hud->fillColor(0, 0, 0, touchalpha[i]);
				aggDraw.hud->rectangle(temptouch.X-1, temptouch.Y-1 + 192, temptouch.X+1, temptouch.Y+1 + 192);
			}
		}
	}
	else
		if(nds.isTouch) {
			aggDraw.hud->line(temptouch.X - 256, temptouch.Y + 192, temptouch.X + 256, temptouch.Y + 192); //horiz
			aggDraw.hud->line(temptouch.X, temptouch.Y - 256, temptouch.X, temptouch.Y + 384); //vert
		}
}


