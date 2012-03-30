/*
	Copyright (C) 2010 DeSmumE team

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

#include "types.h"
#include "GPU_osd.h"

OSDCLASS        *osd;

OSDCLASS::OSDCLASS(u8 core) {}
OSDCLASS::~OSDCLASS() {}
void OSDCLASS::update() {}
void OSDCLASS::clear() {}
void OSDCLASS::setLineColor(u8 r, u8 b, u8 g) {}
void OSDCLASS::addLine(const char *fmt, ...) {}

void DrawHUD() {}
