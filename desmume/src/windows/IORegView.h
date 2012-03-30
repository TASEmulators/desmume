/*
	Copyright (C) 2006 yopyop
	Copyright (C) 2006-2009 DeSmuME team

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

#ifndef IO_REG_H
#define IO_REG_H

#include "../common.h"
#include "CWindow.h"

LRESULT CALLBACK IORegView_Proc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);

void RefreshAllIORegViews();

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
