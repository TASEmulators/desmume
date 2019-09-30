/*
	Copyright (C) 2006 Theo Berkau
	Copyright (C) 2006-2019 DeSmuME team

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

#define DLL extern "C" __declspec(dllexport)

// TOOD: The "main" files (and others) should probably be excluded from this project and the relevant code copied/refactored.
#include "main.h"

// emulator core
#include "NDSSystem.h"
#include "SPU.h"
#include "slot1.h"
#include "slot2.h"
#include "movie.h"

DLL void Init_NDS()
{
	InitializeCriticalSection(&win_execute_sync);
	CommonSettings.num_cores = 1; // is this necessary?

	Desmume_InitOnce();
	NDS_Init();

	paused = FALSE; // is this necessary?
	execute = TRUE;
	SPU_Pause(0);

	CommonSettings.use_jit = true;
	CommonSettings.jit_max_block_size = 100;

	slot1_Init();
	slot2_Init();
	slot2_Change(NDS_SLOT2_NONE);

	cur3DCore = GPU3D_SWRAST;
	GPU->Change3DRendererByID(GPU3D_SWRAST);

	// sound
	EnterCriticalSection(&win_execute_sync);
	int spu_ret = SPU_ChangeSoundCore(SNDCORE_DUMMY, DESMUME_SAMPLE_RATE * 8 / 60);
	LeaveCriticalSection(&win_execute_sync);
	SPU_SetVolume(100);
}
DLL void DeInit_NDS()
{
	NDS_DeInit();
}

DLL int GetFrameCount()
{
	return currFrameCounter;
}

DLL bool LoadROM(u8* file, int fileSize)
{
	return (NDS_LoadROM(file, fileSize) > 0);
}
