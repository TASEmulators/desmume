/*
Copyright (C) 2018 DeSmuME team

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

#ifndef _DDRAW_H_
#define _DDRAW_H_

#include "types.h"
#include "directx/ddraw.h"

extern const char *DDerrors[];

struct DDRAW
{
	DDRAW();

	u32	create(HWND hwnd);
	bool release();
	bool createSurfaces(HWND hwnd);
	bool createBackSurface(int width, int height);
	bool lock();
	bool unlock();
	bool blt(LPRECT dst, LPRECT src);
	bool OK();

	LPDIRECTDRAW7			handle;
	struct
	{
		LPDIRECTDRAWSURFACE7	primary;
		LPDIRECTDRAWSURFACE7	back;
	} surface;

	DDSURFACEDESC2			surfDesc;
	DDSURFACEDESC2			surfDescBack;
	LPDIRECTDRAWCLIPPER		clip;

	bool systemMemory = false;
	bool vSync = false;
};

#endif