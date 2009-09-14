/*  Copyright (C) 2006 yopyop
    yopyop156@ifrance.com
    yopyop156.ifrance.com

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

#ifndef IO_REG_H
#define IO_REG_H

#include "../common.h"

LRESULT CALLBACK IORegView_Proc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);

class CIORegView : public CToolWindow
{
public:
	CIORegView();
	~CIORegView();

	void ChangeCPU(int cpu);
	void ChangeReg(int reg);

	void UpdateScrollbar();

	int CPU;
	int Reg;

	HFONT hFont;

	int rebarHeight;
	int vsbWidth;

	HWND hScrollbar;
	HWND hRebar;
	HWND hCPUCombo;
	HWND hRegCombo;

	int lineheight;
	int numlines;
	int maxlines;

	int yoff;
};

#endif 
