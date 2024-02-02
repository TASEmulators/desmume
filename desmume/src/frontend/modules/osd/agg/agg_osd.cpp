/*
	Copyright (C) 2006 yopyop
	Copyright (C) 2006-2015 DeSmuME team

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

#include "agg_osd.h"
#include "driver.h"
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
#include "rtc.h"
#include "NDSSystem.h"
#include "mic.h"
#include "saves.h"

#ifdef _MSC_VER
#include <Windows.h>
#include "winutil.h"
#else
#include <sys/time.h>
#endif

bool HudEditorMode = false;
OSDCLASS	*osd = NULL;
HudStruct Hud;

//contains a timer to be used for well-timed hud components
static s64 hudTimer;

static void SetHudDummy (HudCoordinates *hud)
{
	hud->x=-666;
	hud->y=-666;
}

static bool IsHudDummy (HudCoordinates *hud)
{
	return (hud->x == -666 && hud->y == -666);
}

static int ScreenWidth()
{
	return 256*osd->scale;
}

static int ScreenHeight()
{
	return 192*osd->scale;
}

template<typename T>
static T calcY(T y) // alters a GUI element y coordinate as necessary to obey swapScreens and singleScreen settings
{
	if(osd->singleScreen)
	{
		if(y >= ScreenHeight())
			y -= ScreenHeight();
		if(osd->swapScreens)
			y += ScreenHeight();
	}
	else if(osd->swapScreens)
	{
		if(y >= ScreenHeight())
			y -= ScreenHeight();
		else
			y += ScreenHeight();
	}
	return y;
}

static void RenderTextAutoVector(double x, double y, const std::string& str, bool shadow = true, double shadowOffset = 1.0)
{
#ifdef AGG2D_USE_VECTORFONTS
	bool render_vect = false;
	if(osd)
		if(osd->useVectorFonts)
			render_vect = true;
	if(render_vect)
	{
		if(shadow)
			aggDraw.hud->renderVectorFontTextDropshadowed(x, y, str, shadowOffset);
		else
			aggDraw.hud->renderVectorFontText(x, y, str);
	}
	else
	{
#endif
	if(shadow)
		aggDraw.hud->renderTextDropshadowed(x, y, str);
	else
		aggDraw.hud->renderText(x, y, str);
#ifdef AGG2D_USE_VECTORFONTS
	}
#endif
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
		if(hud.x > ScreenWidth()-11*osd->scale)
			hud.x = ScreenWidth()-11*osd->scale; //margins
		if(hud.y > ScreenHeight()*2-16*osd->scale)
			hud.y = ScreenHeight()*2-16*osd->scale;

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
	double sc=(osd ? osd->scale : 1.0);
	
	FpsDisplay.x=0;
	FpsDisplay.y=5*sc;
	FpsDisplay.xsize=166*sc;
	FpsDisplay.ysize=10*sc;

	FrameCounter.x=0;
	FrameCounter.y=25*sc;
	FrameCounter.xsize=60*sc;
	FrameCounter.ysize=10*sc;

	InputDisplay.x=0;
	InputDisplay.y=45*sc;
	InputDisplay.xsize=220*sc;
	InputDisplay.ysize=10*sc;

	GraphicalInputDisplay.x=8*sc;
	GraphicalInputDisplay.y=328*sc;
	GraphicalInputDisplay.xsize=102*sc;
	GraphicalInputDisplay.ysize=50*sc;

	LagFrameCounter.x=0;
	LagFrameCounter.y=65*sc;
	LagFrameCounter.xsize=30*sc;
	LagFrameCounter.ysize=10*sc;
	
	Microphone.x=0;
	Microphone.y=85*sc;
	Microphone.xsize=20*sc;
	Microphone.ysize=10*sc;

	RTCDisplay.x=0;
	RTCDisplay.y=105*sc;
	RTCDisplay.xsize=220*sc;
	RTCDisplay.ysize=10*sc;

	SavestateSlots.x = 8*sc;
	SavestateSlots.y = 160*sc;
	SavestateSlots.xsize = 240*sc;
	SavestateSlots.ysize = 24*sc;

	#ifdef _MSC_VER
	#define AGG_OSD_SETTING(which,comp) which.comp = GetPrivateProfileInt("HudEdit", #which "." #comp, which.comp, IniName);
	#include "agg_osd_settings.inc"
	#undef AGG_OSD_SETTING
	#endif

	SetHudDummy(&Dummy);
	clicked = false;
}

void HudStruct::rescale(double oldScale, double newScale)
{
	double sc=newScale/oldScale;
	
	FpsDisplay.x*=sc;
	FpsDisplay.y*=sc;
	FpsDisplay.xsize*=sc;
	FpsDisplay.ysize*=sc;

	FrameCounter.x*=sc;
	FrameCounter.y*=sc;
	FrameCounter.xsize*=sc;
	FrameCounter.ysize*=sc;

	InputDisplay.x*=sc;
	InputDisplay.y*=sc;
	InputDisplay.xsize*=sc;
	InputDisplay.ysize*=sc;

	GraphicalInputDisplay.x*=sc;
	GraphicalInputDisplay.y*=sc;
	GraphicalInputDisplay.xsize*=sc;
	GraphicalInputDisplay.ysize*=sc;

	LagFrameCounter.x*=sc;
	LagFrameCounter.y*=sc;
	LagFrameCounter.xsize*=sc;
	LagFrameCounter.ysize*=sc;
	
	Microphone.x*=sc;
	Microphone.y*=sc;
	Microphone.xsize*=sc;
	Microphone.ysize*=sc;

	RTCDisplay.x*=sc;
	RTCDisplay.y*=sc;
	RTCDisplay.xsize*=sc;
	RTCDisplay.ysize*=sc;

	SavestateSlots.x*=sc;
	SavestateSlots.y*=sc;
	SavestateSlots.xsize*=sc;
	SavestateSlots.ysize*=sc;
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
	double xc = 41*osd->scale - 0.5;
	double yc = 20*osd->scale - 0.5;

	aggDraw.hud->lineColor(128,128,128,255);

	aggDraw.hud->fillLinearGradient(x, y, x+(xc*ratio), y+(yc*ratio), agg::rgba8(222,222,222,128), agg::rgba8(255,255,255,255));

	aggDraw.hud->roundedRect (x, y, floor(x+(xc*ratio))+0.5, floor(y+(yc*ratio))+0.5, 1);

	double screenLeft = x+(xc*.25*ratio);
	double screenTop = y+(yc*.1*ratio);
	double screenRight = x+(xc*.745*ratio);
	double screenBottom = y+(yc*.845*ratio);
	aggDraw.hud->fillLinearGradient(screenLeft, screenTop, screenRight, screenBottom, agg::rgba8(128,128,128,128), agg::rgba8(255,255,255,255));
	aggDraw.hud->roundedRect (screenLeft, screenTop, screenRight, screenBottom, 1);


	joyEllipse(.89,.45,xc,yc,x,y,ratio,osd->scale,6);//B
	joyEllipse(.89,.22,xc,yc,x,y,ratio,osd->scale,3);//X
	joyEllipse(.83,.34,xc,yc,x,y,ratio,osd->scale,4);//Y
	joyEllipse(.95,.34,xc,yc,x,y,ratio,osd->scale,5);//A
	joyEllipse(.82,.716,xc,yc,x,y,ratio,osd->scale * .5,7);//Start
	joyEllipse(.82,.842,xc,yc,x,y,ratio,osd->scale * .5,8);//Select


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
		double gameTouchX = screenLeft+1 + (nds.scr_touchX * osd->scale * 0.0625) * (screenRight - screenLeft - 2) / (double)ScreenWidth();
		double gameTouchY = screenTop+1 + (nds.scr_touchY * osd->scale * 0.0625) * (screenBottom - screenTop - 2) / (double)ScreenHeight();
		bool physicalTouchOn = NDS_getRawUserInput().touch.isTouch;
		double physicalTouchX = screenLeft+1 + (NDS_getRawUserInput().touch.touchX * osd->scale * 0.0625) * (screenRight - screenLeft - 2) / (double)ScreenWidth();
		double physicalTouchY = screenTop+1 + (NDS_getRawUserInput().touch.touchY * osd->scale * 0.0625) * (screenBottom - screenTop - 2) / (double)ScreenHeight();
		if(gameTouchOn && physicalTouchOn && gameTouchX == physicalTouchX && gameTouchY == physicalTouchY)
		{
			aggDraw.hud->fillColor(0,0,0,255);
			aggDraw.hud->ellipse(gameTouchX, gameTouchY, osd->scale*ratio*0.37, osd->scale*ratio*0.37);
		}
		else
		{
			if(physicalTouchOn)
			{
				aggDraw.hud->fillColor(0,0,0,128);
				aggDraw.hud->ellipse(physicalTouchX, physicalTouchY, osd->scale*ratio*0.5, osd->scale*ratio*0.5);
				aggDraw.hud->fillColor(0,255,0,255);
				aggDraw.hud->ellipse(physicalTouchX, physicalTouchY, osd->scale*ratio*0.37, osd->scale*ratio*0.37);
			}
			if(gameTouchOn)
			{
				aggDraw.hud->fillColor(255,0,0,255);
				aggDraw.hud->ellipse(gameTouchX, gameTouchY, osd->scale*ratio*0.37, osd->scale*ratio*0.37);
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
		
		RenderTextAutoVector(x, calcY(Hud.InputDisplay.y), str, true, osd ? osd->scale : 1);
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
			RenderTextAutoVector(x, calcY(Hud.InputDisplay.y), str, true, osd ? osd->scale : 1);
		}
		else
		{
			if(gameTouchOn)
			{
				sprintf(str, "%d,%d", gameTouchX, gameTouchY);
				aggDraw.hud->lineColor(255,48,48,255);
				RenderTextAutoVector(x, calcY(Hud.InputDisplay.y)-(physicalTouchOn?8:0), str, true, osd ? osd->scale : 1);
			}
			if(physicalTouchOn)
			{
				sprintf(str, "%d,%d", physicalTouchX, physicalTouchY);
				aggDraw.hud->lineColor(0,192,0,255);
				RenderTextAutoVector(x, calcY(Hud.InputDisplay.y)+(gameTouchOn?8:0), str, true, osd ? osd->scale : 1);
			}
		}
	}
}

static void OSD_HandleTouchDisplay() {
	// note: calcY should not be used in this function.
	aggDraw.hud->lineWidth(osd->scale);

	temptouch.X = (NDS_getRawUserInput().touch.touchX >> 4) * osd->scale;
	temptouch.Y = (NDS_getRawUserInput().touch.touchY >> 4) * osd->scale;

	if(touchshadow) {

		touch.push_back(temptouch);
		if(touch.size() > 8) touch.erase(touch.begin());

		for (int i = 0; i < 8; i++) {
			temptouch = touch[i];
			if(temptouch.X != 0 || temptouch.Y != 0) {
				aggDraw.hud->lineColor(0, 255, 0, touchalpha[i]);
				aggDraw.hud->line(temptouch.X - ScreenWidth(), temptouch.Y + ScreenHeight(), temptouch.X + ScreenWidth(), temptouch.Y + ScreenHeight()); //horiz
				aggDraw.hud->line(temptouch.X, temptouch.Y - ScreenWidth(), temptouch.X, temptouch.Y + ScreenHeight()*2); //vert
				aggDraw.hud->fillColor(0, 0, 0, touchalpha[i]);
				aggDraw.hud->rectangle(temptouch.X-1, temptouch.Y + ScreenHeight()-1, temptouch.X+1, temptouch.Y + ScreenHeight()+1);
			}
		}
	}
	else
		if(NDS_getRawUserInput().touch.isTouch) {
			aggDraw.hud->lineColor(0, 255, 0, 128);
			aggDraw.hud->line(temptouch.X - ScreenWidth(), temptouch.Y + ScreenHeight(), temptouch.X + ScreenWidth(), temptouch.Y + ScreenHeight()); //horiz
			aggDraw.hud->line(temptouch.X, temptouch.Y - ScreenWidth(), temptouch.X, temptouch.Y + ScreenHeight()*2); //vert
		}

	if(nds.isTouch)
	{
		temptouch.X = nds.scr_touchX / 16 * osd->scale;
		temptouch.Y = nds.scr_touchY / 16 * osd->scale;
		aggDraw.hud->lineColor(255, 0, 0, 128);
		aggDraw.hud->line(temptouch.X - ScreenWidth(), temptouch.Y + ScreenHeight(), temptouch.X + ScreenWidth(), temptouch.Y + ScreenHeight()); //horiz
		aggDraw.hud->line(temptouch.X, temptouch.Y - ScreenWidth(), temptouch.X, temptouch.Y + ScreenHeight()*2); //vert
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
		aggDraw.hud->lineWidth(osd->scale);
		aggDraw.hud->lineColor(0, 0, 0, alpha);
		aggDraw.hud->fillColor(255, 255, 255, alpha);

		for ( int i = 0, xpos=0; i < 10; xpos=xpos+24*osd->scale) {

			int yheight=0;

			aggDraw.hud->fillLinearGradient(xloc + xpos, yloc - yheight, xloc + 22*osd->scale + xpos, yloc + 20*osd->scale + yheight+20*osd->scale, agg::rgba8(100,200,255,alpha), agg::rgba8(255,255,255,0));

			if(lastSaveState == i) {
				yheight = 5*osd->scale;
				aggDraw.hud->fillLinearGradient(xloc + xpos, yloc - yheight, 22*osd->scale + xloc + xpos, yloc + 20*osd->scale + yheight+20*osd->scale, agg::rgba8(100,255,255,alpha), agg::rgba8(255,255,255,0));
			}

			aggDraw.hud->rectangle(xloc + xpos , yloc - yheight, xloc + 22*osd->scale + xpos , yloc + 20*osd->scale + yheight);
			snprintf(number, 10, "%d", i);
			RenderTextAutoVector(xloc + osd->scale + xpos + 4*osd->scale, yloc+4*osd->scale, std::string(number), true, osd->scale);
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
		aggDraw.hud->lineWidth(2.0*osd->scale);
		aggDraw.hud->rectangle(hud.x,calcY(hud.y),hud.x+hud.xsize+1.0,calcY(hud.y)+hud.ysize+1.0);
		aggDraw.hud->lineColor(255,hud.clicked?127:255,0,255);
		aggDraw.hud->lineWidth(osd->scale);
		aggDraw.hud->rectangle(hud.x-0.5,calcY(hud.y)-0.5,hud.x+hud.xsize+0.5,calcY(hud.y)+hud.ysize+0.5);
		i++;
	}
}


void DrawHUD()
{
	#ifdef _MSC_VER
		//code taken from glib's g_get_current_time
		FILETIME ft;
		u64 time64;
		GetSystemTimeAsFileTime (&ft);
		memmove (&time64, &ft, sizeof (FILETIME));

		/* Convert from 100s of nanoseconds since 1601-01-01
		* to Unix epoch. Yes, this is Y2038 unsafe.
		*/
		time64 -= 116444736000000000LL;
		time64 /= 10;

		time_t tv_sec = time64 / 1000000;
		time_t tv_usec = time64 % 1000000;
		hudTimer = ((s64)tv_sec * 1000) + ((s64)tv_usec/1000);
	#else
		struct timeval t;
		gettimeofday (&t, NULL);
		hudTimer = ((s64)t.tv_sec * 1000) + ((s64)t.tv_usec/1000);
	#endif

	if (HudEditorMode)
	{
		DrawEditableElementIndicators();
	}
	
	if (CommonSettings.hud.ShowInputDisplay) 
	{
		TextualInputDisplay();
		OSD_HandleTouchDisplay();
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

	#if defined(WIN32_FRONTEND)
	if (CommonSettings.hud.ShowMicrophone) 
	{
		osd->addFixed(Hud.Microphone.x, Hud.Microphone.y, "%03d [%07d]",MicDisplay, Hud.cpuloopIterationCount);
	}
	#endif

	if (CommonSettings.hud.ShowRTC) 
	{
		rtcGetTimeAsString(Hud.rtcString);
		osd->addFixed(Hud.RTCDisplay.x, Hud.RTCDisplay.y, Hud.rtcString);
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
	scale = 1.0;
	needUpdate = false;
#ifdef AGG2D_USE_VECTORFONTS
	useVectorFonts = false;
#endif
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
				RenderTextAutoVector(lineText_x, lineText_y+(i*16), lineText[i], true, osd->scale);
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

void OSDCLASS::addLine(const char *fmt)
{
	//yucky copied code from the va_list addline

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

	strncpy(lineText[lastLineText],fmt,1023);
	
	lineColor[lastLineText] = lineText_color;
	lineTimer[lastLineText] = time(NULL);
	needUpdate = true;

	lastLineText++;
}

void OSDCLASS::addLine(const char *fmt, va_list args)
{
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

#if defined(_MSC_VER) || defined(__INTEL_COMPILER)
		_vsnprintf(lineText[lastLineText],1023,fmt,args);
#else
		vsnprintf(lineText[lastLineText],1023,fmt,args);
#endif
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
	RenderTextAutoVector(x, calcY(y), msg, true, osd->scale);

	needUpdate = true;
}

void OSDCLASS::border(bool enabled)
{
	//render51.setTextBoxBorder(enabled);
}

void OSDCLASS::SaveHudEditor()
{
	#ifdef _MSC_VER
	#define AGG_OSD_SETTING(which,comp) WritePrivateProfileInt("HudEdit", #which "." #comp, Hud.which.comp, IniName);
	#include "agg_osd_settings.inc"
	#undef AGG_OSD_SETTING
	#endif

	//WritePrivateProfileInt("HudEdit", "FpsDisplay" "." "x", FpsDisplay.x, IniName);
}