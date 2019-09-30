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
DLL void ResetFrameCounters()
{
	currFrameCounter = 0;
}

DLL bool LoadROM(u8* file, int fileSize)
{
	return (NDS_LoadROM(file, fileSize) > 0);
}

DLL void SetInput(u16 buttons, u8 touchX, u8 touchY)
{
	NDS_setPad(buttons & 0x0002, buttons & 0x0001, buttons & 0x0008, buttons & 0x0004,
		buttons & 0x0800, buttons & 0x0400, buttons & 0x0020, buttons & 0x0010,
		buttons & 0x0080, buttons & 0x0040, buttons & 0x0100, buttons & 0x0200,
		buttons & 0x1000, buttons & 0x2000);
	if (buttons & 0x4000)
		NDS_setTouchPos(touchX, touchY);
	else
		NDS_releaseTouch();
}
DLL void FrameAdvance()
{
	NDS_beginProcessingInput();
	NDS_endProcessingInput();

	NDS_exec<false>();
	SPU_Emulate_user();
}

u16* VideoBuffer16bit()
{
	return (u16*)GPU->GetDisplayInfo().masterNativeBuffer;
}
DLL void VideoBuffer32bit(s32* dst)
{
	u16* v = VideoBuffer16bit();
	ColorspaceConvertBuffer555To8888Opaque<true, false>(v, (u32*)dst, 256 * 192 * 2);
}
