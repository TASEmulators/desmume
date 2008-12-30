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
#include "types.h"

#define OSD_MAX_LINES	10

class OSDCLASS
{
private:
	u16		screen[256*192*2];
	u64		offset;
	u8		mode;

	u8		startline;
	u8		lastline;

	u8		*line[OSD_MAX_LINES];
	u8		timer[OSD_MAX_LINES];
	u8		color[OSD_MAX_LINES];

	char	*old_msg;

	u16		current_color;

	bool	needUpdate;

	void printChar(u16 x, u16 y, u8 c);
public:
	char	name[7];		// for debuging
	OSDCLASS(u8 core);
	~OSDCLASS();

	void	setOffset(u16 ofs);
	void	update();
	void	clear();
	void	setColor(u16 col);
	void	addLine(const char *fmt, ...);
	void	addFixed(u16 x, u16 y, const char *fmt, ...);
};

extern OSDCLASS	*osd;
extern OSDCLASS	*osdA;
extern OSDCLASS	*osdB;

#endif
