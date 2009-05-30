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

#ifndef __GPU_OSD_
#define __GPU_OSD_

#include <stdlib.h>
#include <time.h>
#include "types.h"

#define OSD_MAX_LINES 4
#define OSD_TIMER_SECS 2

class OSDCLASS
{
private:
	u16		screen[256*192*2];
	u64		offset;
	u8		mode;

	u16		rotAngle;

	u16		lineText_x;
	u16		lineText_y;
	u32		lineText_color;
	u8		lastLineText;
	char	*lineText[OSD_MAX_LINES+1];
	time_t	lineTimer[OSD_MAX_LINES+1];
	u32		lineColor[OSD_MAX_LINES+1];

	bool	needUpdate;

	bool	checkTimers();
public:
	char	name[7];		// for debuging
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
#endif
