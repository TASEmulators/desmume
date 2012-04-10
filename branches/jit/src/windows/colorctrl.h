/*
	Copyright (C) 2007 Acid Burn

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

#ifndef COLORCTRL_H
#define COLORCTRL_H

typedef struct
{
	HWND		hWnd;
	COLORREF	color;
} ColorCtrl;

void ColorCtrl_Register();
HWND ColorCtrl_Create(HWND hParent);
void ColorCtrl_SetColor(HWND hWnd, COLORREF color);

#endif // COLORCTRL_H
