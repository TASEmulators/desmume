/*
	Copyright (C) 2006 yopyop
	Copyright (C) 2006-2011 DeSmuME team

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

#include "GPU_osd.h"
#include "GPU.h"
#include "mem.h"
#include <string.h> //mem funcs
#include <stdarg.h> //va_start, etc
#include <sstream>
#include <stdio.h>
#include <time.h>
#include <glib.h>
#include "debug.h"

#include "aggdraw.h"
#include "movie.h"
#include "rtc.h"
#include "NDSSystem.h"
#include "mic.h"
#include "saves.h"

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

template<typename T>
static T calcY(T y) // alters a GUI element y coordinate as necessary to obey swapScreens and singleScreen settings
{
	if(osd->singleScreen)
	{
		if(y >= 192)
			y -= 192;
		if(osd->swapScreens)
			y += 192;
	}
	else if(osd->swapScreens)
	{
		if(y >= 192)
			y -= 192;
		else
			y += 192;
	}
	return y;
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
			(calcY(y) >= calcY(hud.y) && calcY(y) <= calcY(hud.y) + hud.ysize) && !hudstruct->clicked ) {

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
		{
			hudstruct->clicked = true;
			break;//prevent items from grouping together
		}

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

	hudstruct->clicked = false;
}

void HudStruct::reset()
{
	FpsDisplay.x=0;
	FpsDisplay.y=5;
	FpsDisplay.xsize=166;
	FpsDisplay.ysize=10;

	FrameCounter.x=0;
	FrameCounter.y=25;
	FrameCounter.xsize=60;
	FrameCounter.ysize=10;

	InputDisplay.x=0;
	InputDisplay.y=45;
	InputDisplay.xsize=220;
	InputDisplay.ysize=10;

	GraphicalInputDisplay.x=8;
	GraphicalInputDisplay.y=328;
	GraphicalInputDisplay.xsize=102;
	GraphicalInputDisplay.ysize=50;

	LagFrameCounter.x=0;
	LagFrameCounter.y=65;
	LagFrameCounter.xsize=30;
	LagFrameCounter.ysize=10;
	
	Microphone.x=0;
	Microphone.y=85;
	Microphone.xsize=20;
	Microphone.ysize=10;

	RTCDisplay.x=0;
	RTCDisplay.y=105;
	RTCDisplay.xsize=220;
	RTCDisplay.ysize=10;

	SavestateSlots.x = 8;
	SavestateSlots.y = 160;
	SavestateSlots.xsize = 240;
	SavestateSlots.ysize = 24;

	SetHudDummy(&Dummy);
	clicked = false;
}

static void joyFill(int n) {

	bool pressedForGame = NDS_getFinalUserInput().buttons.array[n];
	bool physicallyPressed = NDS_getRawUserInput().buttons.array[n];
	if(pressedForGame && physicallyPressed)
		aggDraw.hud->fillColor(0,0,0,255);
	else if(pressedForGame)
		aggDraw.hud->fillColor(255,0,0,255);
	else if(physicallyPressed)
		aggDraw.hud->fillColor(0,255,0,255);
	else
		aggDraw.hud->fillColor(255,255,255,255);
}

static void joyEllipse(double ex, double ey, int xc, int yc, int x, int y, double ratio, double rad, int button) {

	joyFill(button);
	aggDraw.hud->lineWidth(rad);
	aggDraw.hud->ellipse(x+((xc*ex)*ratio), y+((yc*ey)*ratio), rad*ratio, rad*ratio);
}

static void joyRoundedRect(double x1, double y1, int x2, int y2, int alpha1, int alpha2, int button)
{
	bool pressedForGame = NDS_getFinalUserInput().buttons.array[button];
	bool physicallyPressed = NDS_getRawUserInput().buttons.array[button];
	if(pressedForGame && physicallyPressed)
		aggDraw.hud->fillLinearGradient(x1,y1,x2,y2,agg::rgba8(0,0,0,alpha1), agg::rgba8(0,0,0,alpha2));
	else if(pressedForGame)
		aggDraw.hud->fillLinearGradient(x1,y1,x2,y2,agg::rgba8(255,0,0,alpha1), agg::rgba8(255,0,0,alpha2));
	else if(physicallyPressed)
		aggDraw.hud->fillLinearGradient(x1,y1,x2,y2,agg::rgba8(0,255,0,alpha1), agg::rgba8(0,255,0,alpha2));
	else
		return; //aggDraw.hud->fillLinearGradient(x1,y1,x2,y2,agg::rgba8(255,255,255,alpha1), agg::rgba8(255,255,255,alpha2));

	aggDraw.hud->roundedRect(x1,y1,x2,y2,1);
}


static void drawPad(double x, double y, double ratio) {

	// you might notice black/red/green colors used to show what buttons are pressed.
	// the logic is roughly:
	//    RED == PAST    (the button was held last frame)
	//  GREEN == FUTURE  (the button is physically held now)
	//  BLACK == PRESENT (the button was held last frame and is still physically held now)

	// aligning to odd half-pixel boundaries prevents agg2d from blurring thin straight lines
	x = floor(x) + 0.5;
	y = floor(calcY(y)) + 0.5;
	double xc = 41 - 0.5;
	double yc = 20 - 0.5;

	aggDraw.hud->lineColor(128,128,128,255);

	aggDraw.hud->fillLinearGradient(x, y, x+(xc*ratio), y+(yc*ratio), agg::rgba8(222,222,222,128), agg::rgba8(255,255,255,255));

	aggDraw.hud->roundedRect (x, y, floor(x+(xc*ratio))+0.5, floor(y+(yc*ratio))+0.5, 1);

	double screenLeft = x+(xc*.25*ratio);
	double screenTop = y+(yc*.1*ratio);
	double screenRight = x+(xc*.745*ratio);
	double screenBottom = y+(yc*.845*ratio);
	aggDraw.hud->fillLinearGradient(screenLeft, screenTop, screenRight, screenBottom, agg::rgba8(128,128,128,128), agg::rgba8(255,255,255,255));
	aggDraw.hud->roundedRect (screenLeft, screenTop, screenRight, screenBottom, 1);


	joyEllipse(.89,.45,xc,yc,x,y,ratio,1,6);//B
	joyEllipse(.89,.22,xc,yc,x,y,ratio,1,3);//X
	joyEllipse(.83,.34,xc,yc,x,y,ratio,1,4);//Y
	joyEllipse(.95,.34,xc,yc,x,y,ratio,1,5);//A
	joyEllipse(.82,.716,xc,yc,x,y,ratio,.5,7);//Start
	joyEllipse(.82,.842,xc,yc,x,y,ratio,.5,8);//Select


	double dpadPoints [][2] = {
		{.04,.33}, // top-left corner of left button
		{.08,.33},
		{.08,.24}, // top-left corner of up button
		{.13,.24}, // top-right corner of up button
		{.13,.33},
		{.17,.33}, // top-right corner of right button
		{.17,.43}, // bottom-right corner of right button
		{.13,.43},
		{.13,.516}, // bottom-right corner of down button
		{.08,.516}, // bottom-left corner of down button
		{.08,.43},
		{.04,.43}, // bottom-left corner of left button
	};
	static const int numdpadPoints = sizeof(dpadPoints)/sizeof(dpadPoints[0]);
	for(int i = 0; i < numdpadPoints; i++)
	{
		dpadPoints[i][0] = x+(xc*(dpadPoints[i][0]+.01)*ratio);
		dpadPoints[i][1] = y+(yc*(dpadPoints[i][1]+.00)*ratio);
	}

	// dpad outline
	aggDraw.hud->fillColor(255,255,255,200);
	aggDraw.hud->polygon((double*)dpadPoints, numdpadPoints);

	aggDraw.hud->noLine();

	// left
	joyRoundedRect(dpadPoints[0][0], dpadPoints[0][1], dpadPoints[7][0], dpadPoints[7][1], 255, 0, 11);
	
	// right
	joyRoundedRect(dpadPoints[1][0], dpadPoints[1][1], dpadPoints[6][0], dpadPoints[6][1], 0, 255, 12);

	// up
	joyRoundedRect(dpadPoints[2][0], dpadPoints[2][1], dpadPoints[7][0], dpadPoints[7][1], 255, 0, 9);
	
	// right
	joyRoundedRect(dpadPoints[1][0], dpadPoints[1][1], dpadPoints[8][0], dpadPoints[8][1], 0, 255, 10);

	// left shoulder
	joyRoundedRect(x+(xc*.00*ratio), y+(yc*.00*ratio), x+(xc*.15*ratio), y+(yc*.07*ratio), 255, 200, 2);

	// right shoulder
	joyRoundedRect(x+(xc*.85*ratio), y+(yc*.00*ratio), x+(xc*1.0*ratio), y+(yc*.07*ratio), 200, 255, 1);

	// lid...
	joyRoundedRect(x+(xc*.4*ratio), y+(yc*.96*ratio), x+(xc*0.6*ratio), y+(yc*1.0*ratio), 200, 200, 13);

	// touch pad
	{
		BOOL gameTouchOn = nds.isTouch;
		double gameTouchX = screenLeft+1 + (nds.scr_touchX * 0.0625) * (screenRight - screenLeft - 2) / 256.0;
		double gameTouchY = screenTop+1 + (nds.scr_touchY * 0.0625) * (screenBottom - screenTop - 2) / 192.0;
		bool physicalTouchOn = NDS_getRawUserInput().touch.isTouch;
		double physicalTouchX = screenLeft+1 + (NDS_getRawUserInput().touch.touchX * 0.0625) * (screenRight - screenLeft - 2) / 256.0;
		double physicalTouchY = screenTop+1 + (NDS_getRawUserInput().touch.touchY * 0.0625) * (screenBottom - screenTop - 2) / 192.0;
		if(gameTouchOn && physicalTouchOn && gameTouchX == physicalTouchX && gameTouchY == physicalTouchY)
		{
			aggDraw.hud->fillColor(0,0,0,255);
			aggDraw.hud->ellipse(gameTouchX, gameTouchY, ratio*0.37, ratio*0.37);
		}
		else
		{
			if(physicalTouchOn)
			{
				aggDraw.hud->fillColor(0,0,0,128);
				aggDraw.hud->ellipse(physicalTouchX, physicalTouchY, ratio*0.5, ratio*0.5);
				aggDraw.hud->fillColor(0,255,0,255);
				aggDraw.hud->ellipse(physicalTouchX, physicalTouchY, ratio*0.37, ratio*0.37);
			}
			if(gameTouchOn)
			{
				aggDraw.hud->fillColor(255,0,0,255);
				aggDraw.hud->ellipse(gameTouchX, gameTouchY, ratio*0.37, ratio*0.37);
			}
		}
	}
}


struct TouchInfo{
	u16 X;
	u16 Y;
};
static int touchalpha[8]= {31, 63, 95, 127, 159, 191, 223, 255};
static TouchInfo temptouch;
static const bool touchshadow = false;//true; // sorry, it's cool but also distracting and looks cleaner with it off. maybe if it drew line segments between touch points instead of isolated crosses...
static std::vector<TouchInfo> touch (8);

static void TextualInputDisplay() {

	// drawing the whole string at once looks ugly
	// (because of variable width font and the "shadow" appearing over blank space)
	// and can't give us the color-coded effects we want anyway (see drawPad for info)

	const UserButtons& gameButtons = NDS_getFinalUserInput().buttons;
	const UserButtons& physicalButtons = NDS_getRawUserInput().buttons;

	double x = Hud.InputDisplay.x;

	// from order FRLDUTSBAYXWEG where G is 0
	static const char* buttonChars = "<^>vABXYLRSsgf";
	static const int buttonIndex [14] = {11,9,12,10,5,6,3,4,2,1,7,8,0,13};
	for(int i = 0; i < 14; i++, x+=11.0)
	{
		bool pressedForGame = gameButtons.array[buttonIndex[i]];
		bool physicallyPressed = physicalButtons.array[buttonIndex[i]];
		if(pressedForGame && physicallyPressed)
			aggDraw.hud->lineColor(255,255,255,255);
		else if(pressedForGame)
			aggDraw.hud->lineColor(255,48,48,255);
		else if(physicallyPressed)
			aggDraw.hud->lineColor(0,192,0,255);
		else
			continue;

		// cast from char to std::string is a bit awkward
		std::string str(buttonChars+i, 2);
		str[1] = '\0';

		aggDraw.hud->renderTextDropshadowed(x, calcY(Hud.InputDisplay.y), str);
	}

	// touch pad
	{
		char str [32];
		BOOL gameTouchOn = nds.isTouch;
		int gameTouchX = nds.adc_touchX >> 4;
		int gameTouchY = nds.adc_touchY >> 4;
		bool physicalTouchOn = NDS_getRawUserInput().touch.isTouch;
		int physicalTouchX = NDS_getRawUserInput().touch.touchX >> 4;
		int physicalTouchY = NDS_getRawUserInput().touch.touchY >> 4;
		if(gameTouchOn && physicalTouchOn && gameTouchX == physicalTouchX && gameTouchY == physicalTouchY)
		{
			sprintf(str, "%d,%d", gameTouchX, gameTouchY);
			aggDraw.hud->lineColor(255,255,255,255);
			aggDraw.hud->renderTextDropshadowed(x, calcY(Hud.InputDisplay.y), str);
		}
		else
		{
			if(gameTouchOn)
			{
				sprintf(str, "%d,%d", gameTouchX, gameTouchY);
				aggDraw.hud->lineColor(255,48,48,255);
				aggDraw.hud->renderTextDropshadowed(x, calcY(Hud.InputDisplay.y)-(physicalTouchOn?8:0), str);
			}
			if(physicalTouchOn)
			{
				sprintf(str, "%d,%d", physicalTouchX, physicalTouchY);
				aggDraw.hud->lineColor(0,192,0,255);
				aggDraw.hud->renderTextDropshadowed(x, calcY(Hud.InputDisplay.y)+(gameTouchOn?8:0), str);
			}
		}
	}
}

static void TouchDisplay() {
	// note: calcY should not be used in this function.
	aggDraw.hud->lineWidth(1.0);

	temptouch.X = NDS_getRawUserInput().touch.touchX >> 4;
	temptouch.Y = NDS_getRawUserInput().touch.touchY >> 4;

	if(touchshadow) {

		touch.push_back(temptouch);
		if(touch.size() > 8) touch.erase(touch.begin());

		for (int i = 0; i < 8; i++) {
			temptouch = touch[i];
			if(temptouch.X != 0 || temptouch.Y != 0) {
				aggDraw.hud->lineColor(0, 255, 0, touchalpha[i]);
				aggDraw.hud->line(temptouch.X - 256, temptouch.Y + 192, temptouch.X + 256, temptouch.Y + 192); //horiz
				aggDraw.hud->line(temptouch.X, temptouch.Y - 256, temptouch.X, temptouch.Y + 384); //vert
				aggDraw.hud->fillColor(0, 0, 0, touchalpha[i]);
				aggDraw.hud->rectangle(temptouch.X-1, temptouch.Y + 192-1, temptouch.X+1, temptouch.Y + 192+1);
			}
		}
	}
	else
		if(NDS_getRawUserInput().touch.isTouch) {
			aggDraw.hud->lineColor(0, 255, 0, 128);
			aggDraw.hud->line(temptouch.X - 256, temptouch.Y + 192, temptouch.X + 256, temptouch.Y + 192); //horiz
			aggDraw.hud->line(temptouch.X, temptouch.Y - 256, temptouch.X, temptouch.Y + 384); //vert
		}

	if(nds.isTouch)
	{
		temptouch.X = nds.scr_touchX / 16;
		temptouch.Y = nds.scr_touchY / 16;
		aggDraw.hud->lineColor(255, 0, 0, 128);
		aggDraw.hud->line(temptouch.X - 256, temptouch.Y + 192, temptouch.X + 256, temptouch.Y + 192); //horiz
		aggDraw.hud->line(temptouch.X, temptouch.Y - 256, temptouch.X, temptouch.Y + 384); //vert
	}

}

static int previousslot = 0;
static char number[10];
static s64 slotTimer=0;

static void DrawStateSlots(){

	const int yloc = calcY(Hud.SavestateSlots.y); //160
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

static void DrawEditableElementIndicators()
{
	u32 i = 0;
	while (!IsHudDummy(&Hud.hud(i))) {
		HudCoordinates &hud = Hud.hud(i);
		aggDraw.hud->fillColor(0,0,0,0);
		aggDraw.hud->lineColor(0,0,0,64);
		aggDraw.hud->lineWidth(2.0);
		aggDraw.hud->rectangle(hud.x,calcY(hud.y),hud.x+hud.xsize+1.0,calcY(hud.y)+hud.ysize+1.0);
		aggDraw.hud->lineColor(255,hud.clicked?127:255,0,255);
		aggDraw.hud->lineWidth(1.0);
		aggDraw.hud->rectangle(hud.x-0.5,calcY(hud.y)-0.5,hud.x+hud.xsize+0.5,calcY(hud.y)+hud.ysize+0.5);
		i++;
	}
}


void DrawHUD()
{
	GTimeVal time;
	g_get_current_time(&time);
	hudTimer = ((s64)time.tv_sec * 1000) + ((s64)time.tv_usec/1000);

	if (HudEditorMode)
	{
		DrawEditableElementIndicators();
	}
	
	if (CommonSettings.hud.ShowInputDisplay) 
	{
		TextualInputDisplay();
		TouchDisplay();
	}

	if (CommonSettings.hud.FpsDisplay) 
	{
		osd->addFixed(Hud.FpsDisplay.x, Hud.FpsDisplay.y, "Fps:%02d/%02d (%02d%%/%02d%%)%s", Hud.fps, Hud.fps3d, Hud.cpuload[0], Hud.cpuload[1], driver->EMU_IsEmulationPaused() ? " (paused)" : "");
	}

	if (CommonSettings.hud.FrameCounterDisplay) 
	{
		if(movieMode == MOVIEMODE_RECORD) 
			osd->addFixed(Hud.FrameCounter.x, Hud.FrameCounter.y, "%d",currFrameCounter);
		else if (movieMode == MOVIEMODE_PLAY)
			osd->addFixed(Hud.FrameCounter.x, Hud.FrameCounter.y, "%d/%d",currFrameCounter,currMovieData.records.size());
		else if (movieMode == MOVIEMODE_FINISHED)
			osd->addFixed(Hud.FrameCounter.x, Hud.FrameCounter.y, "%d/%d (finished)",currFrameCounter,currMovieData.records.size());
		else
			osd->addFixed(Hud.FrameCounter.x, Hud.FrameCounter.y, "%d (no movie)",currFrameCounter);
	}

	if (CommonSettings.hud.ShowLagFrameCounter) 
	{
		osd->addFixed(Hud.LagFrameCounter.x, Hud.LagFrameCounter.y, "%d",TotalLagFrames);
	}

	if (CommonSettings.hud.ShowGraphicalInputDisplay)
	{
		drawPad(Hud.GraphicalInputDisplay.x, Hud.GraphicalInputDisplay.y, 2.5);
	}

	#if defined(WIN32)
	if (CommonSettings.hud.ShowMicrophone) 
	{
		osd->addFixed(Hud.Microphone.x, Hud.Microphone.y, "%03d [%07d]",MicDisplay, Hud.cpuloopIterationCount);
	}
	#endif

	if (CommonSettings.hud.ShowRTC) 
	{
		DateTime tm = rtcGetTime();
		static const char *wday[] = { "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat" };
		osd->addFixed(Hud.RTCDisplay.x, Hud.RTCDisplay.y, "%04d-%03s-%02d %s %02d:%02d:%02d", tm.get_Year(), DateTime::GetNameOfMonth(tm.get_Month()), tm.get_Day(), wday[tm.get_DayOfWeek()%7], tm.get_Hour(), tm.get_Minute(), tm.get_Second());
	}

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

	singleScreen = false;
	swapScreens = false;

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

void OSDCLASS::setLineColor(u8 r=255, u8 g=255, u8 b=255)
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
	aggDraw.hud->renderTextDropshadowed(x,calcY(y),msg);

	needUpdate = true;
}

void OSDCLASS::border(bool enabled)
{
	//render51.setTextBoxBorder(enabled);
}
