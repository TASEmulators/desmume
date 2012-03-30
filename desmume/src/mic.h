/*
	Copyright (C) 2009-2011 DeSmuME Team

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

#ifndef MIC_H
#define MIC_H

#include <iostream>
#include "emufile.h"
#include "types.h"

#ifdef WIN32
static char MicSampleName[256];
bool LoadSample(const char *name);
#endif

extern int MicDisplay;

#ifdef FAKE_MIC
void Mic_DoNoise(BOOL);
#endif

BOOL Mic_Init(void);
void Mic_Reset(void);
void Mic_DeInit(void);
u8 Mic_ReadSample(void);

void mic_savestate(EMUFILE* os);
bool mic_loadstate(EMUFILE* is, int size);

#endif // MIC_H
