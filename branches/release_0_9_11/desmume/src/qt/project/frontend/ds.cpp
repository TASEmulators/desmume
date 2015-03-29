/*
    Copyright (C) 2014-2015 DeSmuME team
	Copyright (C) 2014 Alvin Wong

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

#include "ds.h"

#include "NDSSystem.h"
#include "firmware.h"
#include "mc.h"
#include "render3D.h"
#include "rasterize.h"
#include "SPU.h"
#include "slot2.h"
#include "GPU_osd.h"

#include "commandline.h"

//#include "filter/videofilter.h"

#include "video.h"
#include "sndqt.h"

// TODO: Handle pausing correctly
volatile bool execute = false;
// TODO: Create a subclass of BaseDriver for Qt frontend

GPU3DInterface *core3DList[] = {
	&gpu3DNull,
	&gpu3DRasterize,
#if 0
	&gpu3Dgl,
#endif
	NULL
};

SoundInterface_struct *SNDCoreList[] = {
	&SNDDummy,
	&SNDQt,
	NULL
};

namespace desmume {
namespace qt {
namespace ds {

Video *video;
Input input;

namespace {

bool romLoaded = false;
bool unpaused = true;

} /* namespace */

void init() {
	video = new Video(4);
	//video_buffer = video->GetDstBufferPtr();
	/*video->SetFilterParameteri(VF_PARAM_SCANLINE_A, _scanline_filter_a);
	video->SetFilterParameteri(VF_PARAM_SCANLINE_B, _scanline_filter_b);
	video->SetFilterParameteri(VF_PARAM_SCANLINE_C, _scanline_filter_c);
	video->SetFilterParameteri(VF_PARAM_SCANLINE_D, _scanline_filter_d);*/

	::CommonSettings.use_jit = true;
	::CommonSettings.jit_max_block_size = 100;

	struct ::NDS_fw_config_data fw_config;
	::NDS_FillDefaultFirmwareConfigData(&fw_config);

	::slot2_Init();
	::slot2_Change(NDS_SLOT2_AUTO);

	::NDS_Init();

	::SPU_ChangeSoundCore(SNDCORE_QT, 1024 * 4);
	::SPU_SetSynchMode(1, 0); // Sync N
	//::SPU_SetSynchMode(1, 2); // Sync P

	::Desmume_InitOnce();
	//Hud.reset();

	::NDS_CreateDummyFirmware(&fw_config);

	::backup_setManualBackupType(0);

	::NDS_3D_ChangeCore(1);
}

void deinit() {
	romLoaded = false;
	unpaused = false;
	::NDS_DeInit();
}


bool loadROM(const char* path) {
	bool shouldUnpause = unpaused;
	pause();
	if (::NDS_LoadROM(path) < 0) {
		return false;
	} else {
		romLoaded = true;
		if (shouldUnpause) {
			unpause();
		}
		return true;
	}
}

bool isRunning() {
	return romLoaded && unpaused;
}

void pause() {
	unpaused = false;
	::execute = false;
	::SPU_Pause(1);
}

void unpause() {
	unpaused = true;
	if (romLoaded) {
		::execute = true;
		::SPU_Pause(0);
	}
}

void execFrame() {
	if (romLoaded && unpaused) {
		::NDS_exec<false>();
		::SPU_Emulate_user();
	}
}

} /* namespace ds */
} /* namespace qt */
} /* namespace desmume */
