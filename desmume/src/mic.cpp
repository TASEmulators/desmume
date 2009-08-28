/* mic.cpp - this file is part of DeSmuME
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

#ifndef WIN32

#include "types.h"
#include "mic.h"
#include "readwrite.h"
#include "emufile.h"

static BOOL silence = TRUE;

BOOL Mic_Init()
{
	return TRUE;
}

void Mic_Reset()
{
}

void Mic_DeInit()
{
}

u8 Mic_ReadSample()
{
	if (silence)
		return 0;

	return 150;
}

void Mic_DoNoise(BOOL noise)
{
	silence = !noise;
}

void mic_savestate(EMUFILE* os)
{
	write32le(-1,os);
}

bool mic_loadstate(EMUFILE* is, int size)
{
	is->fseek(size, SEEK_CUR);
	return TRUE;
}

#endif
