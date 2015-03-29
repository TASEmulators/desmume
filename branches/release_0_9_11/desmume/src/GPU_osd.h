/*
	Copyright (C) 2006 yopyop
	Copyright (C) 2008-2012 DeSmuME team

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

#ifndef __GPU_OSD_
#define __GPU_OSD_

#include "types.h"

#ifdef HAVE_LIBAGG
#include <stdlib.h>
#include <time.h>

#include "aggdraw.h"

#define OSD_MAX_LINES 4
#define OSD_TIMER_SECS 2


struct HudCoordinates{
	int x;
	int y;
	int xsize;
	int ysize;
	int storedx;
	int storedy;
	int clicked;
};

struct HudStruct
{
public:
	HudStruct()
	{
		resetTransient();
	}

	void resetTransient()
	{
		fps = 0;
		fps3d = 0;
		cpuload[0] = cpuload[1] = 0;
		cpuloopIterationCount = 0;
		clicked = false;
	}

	HudCoordinates SavestateSlots;
	HudCoordinates FpsDisplay;
	HudCoordinates FrameCounter;
	HudCoordinates InputDisplay;
	HudCoordinates GraphicalInputDisplay;
	HudCoordinates LagFrameCounter;
	HudCoordinates Microphone;
	HudCoordinates RTCDisplay;
	HudCoordinates Dummy;

	HudCoordinates &hud(int i) { return ((HudCoordinates*)this)[i]; }
	void reset();

	int fps, fps3d, cpuload[2], cpuloopIterationCount;
	bool clicked;
};

void EditHud(s32 x, s32 y, HudStruct *hudstruct);
void HudClickRelease(HudStruct *hudstruct);

void DrawHUD();

extern HudStruct Hud;
extern bool HudEditorMode;

class OSDCLASS
{
private:
	u64		offset;
	u8		mode;

	u16		rotAngle;

	u16		lineText_x;
	u16		lineText_y;
	AggColor		lineText_color;
	u8		lastLineText;
	char	*lineText[OSD_MAX_LINES+1];
	time_t	lineTimer[OSD_MAX_LINES+1];
	AggColor lineColor[OSD_MAX_LINES+1];

	bool	needUpdate;

	bool	checkTimers();

public:
	char	name[7];		// for debuging
	bool    singleScreen;
	bool    swapScreens;

	OSDCLASS(u8 core);
	~OSDCLASS();

	void	setOffset(u16 ofs);
	void	setRotate(u16 angle);
	void	update();
	void	clear();
	void	setListCoord(u16 x, u16 y);
	void	setLineColor(u8 r, u8 b, u8 g);
	void	addLine(const char *fmt, ...);
	void	addFixed(u16 x, u16 y, const char *fmt, ...);
	void	border(bool enabled);
};

extern OSDCLASS	*osd;
#else /* HAVE_LIBAGG */
void DrawHUD();

class OSDCLASS {
public:
  OSDCLASS(u8 core);
  ~OSDCLASS();
  void    update();
  void    clear();
  void    setLineColor(u8 r, u8 b, u8 g);
  void    addLine(const char *fmt, ...);
};

extern OSDCLASS        *osd;
#endif
#endif
