/* mic.h - this file is part of DeSmuME
 *
 * Copyright (C) 2009 DeSmuME Team
 *
 * This file is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * This file is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; see the file COPYING.  If not, write to
 * the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#ifndef MIC_H
#define MIC_H
#include <iostream>
#include "emufile.h"

#ifdef WIN32
static char MicSampleName[256];
bool LoadSample(const char *name);
#endif

extern int MicDisplay;

#ifdef FAKE_MIC
void Mic_DoNoise(BOOL);
#endif

BOOL Mic_Init();
void Mic_Reset();
void Mic_DeInit();
u8 Mic_ReadSample();

void mic_savestate(EMUFILE* os);
bool mic_loadstate(EMUFILE* is, int size);

#endif
