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
#include <sstream>
#include <stdio.h>
#include <time.h>
#include "debug.h"

#include "aggdraw.h"
#include "movie.h"
#include "NDSSystem.h"
#include "mic.h"
#include "saves.h"
#include "glib.h"

bool HudEditorMode = false;
OSDCLASS	*osd = NULL;
HudStruct Hud;

//contains a timer to be used for well-timed hud components
static s64 hudTimer;

static void SetHudDummy (HudCoordinates *hud)
{
	hud->x=666;
	hud->y=666;
}

static bool IsHudDummy (HudCoordinates *hud)
{
	return (hud->x == 666 && hud->y == 666);
}

void EditHud(s32 x, s32 y, HudStruct *hudstruct) {

	u32 i = 0;

	while (!IsHudDummy(&hudstruct->hud(i))) {
		HudCoordinates &hud = hudstruct->hud(i);

		//reset
		if(!hud.clicked) {
			hud.storedx=0;
			hud.storedy=0;
		}

		if((x >= hud.x && x <= hud.x + hud.xsize) && 
			(y >= hud.y && y <= hud.y + hud.ysize) && !hud.clicked ) {

				hud.clicked=1;
				hud.storedx = x - hud.x;
				hud.storedy = y - hud.y;
		}

		if(hud.clicked) {
			hud.x = x - hud.storedx;
			hud.y = y - hud.storedy;
		}

		//sanity checks
		if(hud.x < 0)  hud.x = 0;
		if(hud.y < 0)  hud.y = 0;
		if(hud.x > 245)hud.x = 245; //margins
		if(hud.y > 384-16)hud.y = 384-16;

		if(hud.clicked)
			break;//prevent items from grouping together

		i++;
	}
}

void HudClickRelease(HudStruct *hudstruct) {

	u32 i = 0;

	while (!IsHudDummy(&hudstruct->hud(i))) {
		HudCoordinates &hud = hudstruct->hud(i);
		hud.clicked=0;
		i++;
	}
}

void HudStruct::reset()
{
	FpsDisplay.x=0;
	FpsDisplay.y=5;
	FpsDisplay.xsize=120;
	FpsDisplay.ysize=10;

	FrameCounter.x=0;
	FrameCounter.y=25;
	FrameCounter.xsize=60;
	FrameCounter.ysize=10;

	InputDisplay.x=0;
	InputDisplay.y=45;
	InputDisplay.xsize=120;
	InputDisplay.ysize=10;

	GraphicalInputDisplay.x=8;
	GraphicalInputDisplay.y=328;
	GraphicalInputDisplay.xsize=100;
	GraphicalInputDisplay.ysize=40;

	LagFrameCounter.x=0;
	LagFrameCounter.y=65;
	LagFrameCounter.xsize=30;
	LagFrameCounter.ysize=10;
	
	Microphone.x=0;
	Microphone.y=85;
	Microphone.xsize=20;
	Microphone.ysize=10;

	SavestateSlots.x = 8;
	SavestateSlots.y = 160;
	SavestateSlots.xsize = 240;
	SavestateSlots.ysize = 24;

	SetHudDummy(&Dummy);
}

void joyFill(int n) {

	if(nds.pad & (1 << n))
		aggDraw.hud->fillColor(0,0,0,255);
	else
		aggDraw.hud->fillColor(255,255,255,255);
}

void joyEllipse(double ex, double ey, int xc, int yc, int x, int y, double ratio, double rad, int button) {

	joyFill(button);
	aggDraw.hud->ellipse(x+((xc*ex)*ratio), y+((yc*ey)*ratio), rad*ratio, rad*ratio);
}

void gradientFill(double x1,double y1,double x2,double y2,AggColor c1,AggColor c2, int n) {

	if(nds.pad & (1 << n))
		aggDraw.hud->fillLinearGradient(x1,y1,x2,y2,c1,c2);
	else
		aggDraw.hud->fillColor(255,255,255,255);
}

void drawPad(int x, int y, double ratio) {

	int xc = 41;
	int yc = 20;

	aggDraw.hud->lineColor(128,128,128,255);

	aggDraw.hud->fillLinearGradient(x, y, x+(xc*ratio), y+(yc*ratio), agg::rgba8(222,222,222,128), agg::rgba8(255,255,255,255));

	if(nds.pad & (1 << 2))
		aggDraw.hud->fillLinearGradient(x, y, x+(xc*ratio), y+(yc*ratio), agg::rgba8(0,0,0,128), agg::rgba8(255,255,255,255));

	if(nds.pad & (1 << 1))
		aggDraw.hud->fillLinearGradient(x+(xc*ratio), y+(yc*ratio), x, y, agg::rgba8(0,0,0,128), agg::rgba8(255,255,255,255));

	aggDraw.hud->roundedRect (x, y, x+(xc*ratio), y+(yc*ratio), 1);

	aggDraw.hud->fillLinearGradient(x+(xc*.25*ratio), y+(yc*.1*ratio), x+(xc*.75*ratio), y+(yc*.85*ratio), agg::rgba8(128,128,128,128), agg::rgba8(255,255,255,255));

	aggDraw.hud->roundedRect (x+(xc*.25*ratio), y+(yc*.1*ratio), x+(xc*.75*ratio),y+(yc*.85*ratio), 1);
	
	joyEllipse(.89,.45,xc,yc,x,y,ratio,1,6);//B
	joyEllipse(.89,.22,xc,yc,x,y,ratio,1,3);//X
	joyEllipse(.83,.34,xc,yc,x,y,ratio,1,4);//Y
	joyEllipse(.95,.34,xc,yc,x,y,ratio,1,5);//A
	joyEllipse(.82,.72,xc,yc,x,y,ratio,.5,7);//Start
	joyEllipse(.82,.85,xc,yc,x,y,ratio,.5,8);//Select

	aggDraw.hud->noLine();
	aggDraw.hud->fillColor(255,255,255,200);

	//left
	gradientFill(x+(xc*.04*ratio), y+(yc*.33*ratio), x+(xc*.17*ratio), y+(yc*.43*ratio), agg::rgba8(0,0,0,255), agg::rgba8(255,255,255,255),11);

	//right
	if(nds.pad & (1 << 12))
		aggDraw.hud->fillLinearGradient(x+(xc*.17*ratio), y+(yc*.43*ratio), x+(xc*.04*ratio), y+(yc*.33*ratio), agg::rgba8(0,0,0,255), agg::rgba8(255,255,255,255));
		
	aggDraw.hud->roundedRect (x+(xc*.04*ratio), y+(yc*.33*ratio), x+(xc*.17*ratio), y+(yc*.43*ratio), 1);

	//down
	gradientFill(x+(xc*.13*ratio), y+(yc*.52*ratio), x+(xc*.08*ratio), y+(yc*.23*ratio), agg::rgba8(0,0,0,255), agg::rgba8(255,255,255,255),10);

	//up
	if(nds.pad & (1<< 9))
		aggDraw.hud->fillLinearGradient(x+(xc*.08*ratio), y+(yc*.23*ratio), x+(xc*.13*ratio), y+(yc*.52*ratio), agg::rgba8(0,0,0,255), agg::rgba8(255,255,255,255));

	aggDraw.hud->roundedRect (x+(xc*.08*ratio), y+(yc*.23*ratio), x+(xc*.13*ratio), y+(yc*.52*ratio), 1);
}


struct TouchInfo{
	u16 X;
	u16 Y;
};
static int touchalpha[8]= {31, 63, 95, 127, 159, 191, 223, 255};
static TouchInfo temptouch;
bool touchshadow = true;
static std::vector<TouchInfo> touch (8);


static void TouchDisplay() {
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

static int previousslot = 0;
static char number[10];
static s64 slotTimer=0;

static void DrawStateSlots(){

	const int yloc = Hud.SavestateSlots.y; //160
	const int xloc = Hud.SavestateSlots.x; //8

	s64 fadecounter = 512 - (hudTimer-slotTimer)/4; //change constant to alter fade speed
	if(fadecounter < 1) fadecounter = 0;
	if(fadecounter>255) fadecounter = 255;

	int alpha = (int)fadecounter;
	if(HudEditorMode)
		alpha = 255;

	if(alpha!=0)
	{
		aggDraw.hud->lineWidth(1.0);
		aggDraw.hud->lineColor(0, 0, 0, alpha);
		aggDraw.hud->fillColor(255, 255, 255, alpha);

		for ( int i = 0, xpos=0; i < 10; xpos=xpos+24) {

			int yheight=0;

			aggDraw.hud->fillLinearGradient(xloc + xpos, yloc - yheight, xloc + 22 + xpos, yloc + 20 + yheight+20, agg::rgba8(100,200,255,alpha), agg::rgba8(255,255,255,0));

			if(lastSaveState == i) {
				yheight = 5;
				aggDraw.hud->fillLinearGradient(xloc + xpos, yloc - yheight, 22 + xloc + xpos, yloc + 20 + yheight+20, agg::rgba8(100,255,255,alpha), agg::rgba8(255,255,255,0));
			}

			aggDraw.hud->rectangle(xloc + xpos , yloc - yheight, xloc + 22 + xpos , yloc + 20 + yheight);
			snprintf(number, 10, "%d", i);
			aggDraw.hud->renderText(xloc + 1 + xpos + 4, yloc+4, std::string(number));
			i++;
		}
	}

	if(lastSaveState != previousslot)
		slotTimer = hudTimer;

	previousslot = lastSaveState;
}

void DrawHUD()
{
	GTimeVal time;
	g_get_current_time(&time);
	hudTimer = ((s64)time.tv_sec * 1000) + ((s64)time.tv_usec/1000);

	
	if (CommonSettings.hud.ShowInputDisplay) 
	{
		std::stringstream ss;
		if(nds.isTouch)
			ss << (nds.touchX >> 4) << " " << (nds.touchY >> 4); 
		osd->addFixed(Hud.InputDisplay.x, Hud.InputDisplay.y, "%s",(InputDisplayString + ss.str()).c_str());
		TouchDisplay();
	}

	if (CommonSettings.hud.FpsDisplay) 
	{
		osd->addFixed(Hud.FpsDisplay.x, Hud.FpsDisplay.y, "Fps:%02d/%02d", Hud.fps, Hud.fps3d);
	}

	if (CommonSettings.hud.FrameCounterDisplay) 
	{
		if (movieMode == MOVIEMODE_PLAY)
			osd->addFixed(Hud.FrameCounter.x, Hud.FrameCounter.y, "%d/%d",currFrameCounter,currMovieData.records.size());
		else if(movieMode == MOVIEMODE_RECORD) 
			osd->addFixed(Hud.FrameCounter.x, Hud.FrameCounter.y, "%d",currFrameCounter);
		else
			osd->addFixed(Hud.FrameCounter.x, Hud.FrameCounter.y, "%d (no movie)",currFrameCounter);
	}

	if (CommonSettings.hud.ShowLagFrameCounter) 
	{
		osd->addFixed(Hud.LagFrameCounter.x, Hud.LagFrameCounter.y, "%d",TotalLagFrames);
	}

	if (CommonSettings.hud.ShowGraphicalInputDisplay)
		drawPad(Hud.GraphicalInputDisplay.x, Hud.GraphicalInputDisplay.y, 2.5);

	#ifdef WIN32
	if (CommonSettings.hud.ShowMicrophone) 
	{
		osd->addFixed(Hud.Microphone.x, Hud.Microphone.y, "%d",MicDisplay);
	}
	#endif

	DrawStateSlots();
}



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
