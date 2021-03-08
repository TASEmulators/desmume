/*
	Copyright (C) 2006 yopyop
	Copyright (C) 2006-2007 shash
	Copyright (C) 2008-2020 DeSmuME team

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

#include <SDL.h>
#include "interface.h"

#include <stdio.h>
#include "../../NDSSystem.h"
#include "../../GPU.h"
#include "../../SPU.h"
#include "../../MMU.h"
#include "../../rasterize.h"
#include "../../saves.h"
#include "../../movie.h"
#include "../../mc.h"
#include "../../firmware.h"
#include "../../armcpu.h"
#include "../posix/shared/sndsdl.h"
#include "../posix/shared/ctrlssdl.h"
#include <locale>
#include <codecvt>
#include <string>

#define SCREENS_PIXEL_SIZE 98304
volatile bool execute = false;
TieredRegion hooked_regions [HOOK_COUNT];
std::map<unsigned int, memory_cb_fnc> hooks[HOOK_COUNT];


SoundInterface_struct *SNDCoreList[] = {
        &SNDDummy,
        &SNDDummy,
        &SNDSDL,
        NULL
};

GPU3DInterface *core3DList[] = {
        &gpu3DNull,
        &gpu3DRasterize,
        NULL
};

std::wstring s2ws(const std::string& str)
{
    std::wstring_convert<std::codecvt_utf8_utf16<wchar_t> > converter;
    return converter.from_bytes(str);
}

EXPORTED int desmume_init()
{
    NDS_Init();
    // TODO: Option to disable audio
    SPU_ChangeSoundCore(SNDCORE_SDL, 735 * 4);
    SPU_SetSynchMode(0, 0);
    SPU_SetVolume(100);
    SNDSDLSetAudioVolume(100);
    // TODO: Option to configure 3d
    GPU->Change3DRendererByID(RENDERID_SOFTRASTERIZER);
    // TODO: Without SDL init?
    if(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) == -1) {
        fprintf(stderr, "Error trying to initialize SDL: %s\n",
                SDL_GetError());
        return -1;
    }
    execute = false;
    return 0;
}

EXPORTED void desmume_free()
{
    execute = false;
    NDS_DeInit();
    SDL_Quit();
}

EXPORTED void desmume_set_language(u8 lang)
{
    CommonSettings.fwConfig.language = lang;
}

EXPORTED int desmume_open(const char *filename)
{
    int i;
    clear_savestates();
    i = NDS_LoadROM(filename);
    return i;
}

EXPORTED void desmume_set_savetype(int type) {
    backup_setManualBackupType(type);
}


EXPORTED void desmume_pause()
{
    execute = false;
    SPU_Pause(1);
}

EXPORTED void desmume_resume()
{
    execute = true;
    SPU_Pause(0);
}

EXPORTED void desmume_reset()
{
    NDS_Reset();
    desmume_resume();
}

EXPORTED BOOL desmume_running()
{
    return execute;
}

EXPORTED void desmume_skip_next_frame()
{
    NDS_SkipNextFrame();
}

EXPORTED void desmume_cycle(BOOL with_joystick)
{
    u16 keypad;
    /* Joystick events */
    if (with_joystick) {
        /* Retrieve old value: can use joysticks w/ another device (from our side) */
        keypad = get_keypad();
        /* Process joystick events if any */
        process_joystick_events(&keypad);
        /* Update keypad value */
        update_keypad(keypad);
    }

    NDS_beginProcessingInput();
    {
        FCEUMOV_AddInputState();
    }
    NDS_endProcessingInput();

    NDS_exec<false>();
    SPU_Emulate_user();
}

EXPORTED int desmume_sdl_get_ticks()
{
    return SDL_GetTicks();
}

#ifdef INCLUDE_OPENGL_2D
EXPORTED void desmume_draw_opengl(GLuint *texture)
{
#ifdef HAVE_LIBAGG
    //TODO : osd->update();
    //TODO : DrawHUD();
#endif
    const NDSDisplayInfo &displayInfo = GPU->GetDisplayInfo();

    /* Clear The Screen And The Depth Buffer */
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    /* Move Into The Screen 5 Units */
    glLoadIdentity();

    /* Draw the main screen as a textured quad */
    glBindTexture(GL_TEXTURE_2D, texture[NDSDisplayID_Main]);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, GPU_FRAMEBUFFER_NATIVE_WIDTH, GPU_FRAMEBUFFER_NATIVE_HEIGHT,
                  GL_RGBA,
                  GL_UNSIGNED_SHORT_1_5_5_5_REV,
                  displayInfo.renderedBuffer[NDSDisplayID_Main]);

    GLfloat backlightIntensity = displayInfo.backlightIntensity[NDSDisplayID_Main];

    glBegin(GL_QUADS);
        glTexCoord2f(0.00f, 0.00f); glVertex2f(  0.0f,   0.0f); glColor4f(backlightIntensity, backlightIntensity, backlightIntensity, 1.0f);
        glTexCoord2f(1.00f, 0.00f); glVertex2f(256.0f,   0.0f); glColor4f(backlightIntensity, backlightIntensity, backlightIntensity, 1.0f);
        glTexCoord2f(1.00f, 0.75f); glVertex2f(256.0f, 192.0f); glColor4f(backlightIntensity, backlightIntensity, backlightIntensity, 1.0f);
        glTexCoord2f(0.00f, 0.75f); glVertex2f(  0.0f, 192.0f); glColor4f(backlightIntensity, backlightIntensity, backlightIntensity, 1.0f);
    glEnd();

    /* Draw the touch screen as a textured quad */
    glBindTexture(GL_TEXTURE_2D, texture[NDSDisplayID_Touch]);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, GPU_FRAMEBUFFER_NATIVE_WIDTH, GPU_FRAMEBUFFER_NATIVE_HEIGHT,
                  GL_RGBA,
                  GL_UNSIGNED_SHORT_1_5_5_5_REV,
                  displayInfo.renderedBuffer[NDSDisplayID_Touch]);

    backlightIntensity = displayInfo.backlightIntensity[NDSDisplayID_Touch];

    glBegin(GL_QUADS);
        glTexCoord2f(0.00f, 0.00f); glVertex2f(  0.0f, 192.0f); glColor4f(backlightIntensity, backlightIntensity, backlightIntensity, 1.0f);
        glTexCoord2f(1.00f, 0.00f); glVertex2f(256.0f, 192.0f); glColor4f(backlightIntensity, backlightIntensity, backlightIntensity, 1.0f);
        glTexCoord2f(1.00f, 0.75f); glVertex2f(256.0f, 384.0f); glColor4f(backlightIntensity, backlightIntensity, backlightIntensity, 1.0f);
        glTexCoord2f(0.00f, 0.75f); glVertex2f(  0.0f, 384.0f); glColor4f(backlightIntensity, backlightIntensity, backlightIntensity, 1.0f);
    glEnd();
#ifdef HAVE_LIBAGG
    //TODO : osd->clear();
#endif
}
EXPORTED extern BOOL desmume_has_opengl()
{
    return true;
}
#else
EXPORTED extern BOOL desmume_has_opengl()
{
    return false;
}
#endif

// SDL drawing is in draw_sdl_window.cpp

EXPORTED u16 *desmume_draw_raw()
{
    const NDSDisplayInfo &displayInfo = GPU->GetDisplayInfo();
    const size_t pixCount = GPU_FRAMEBUFFER_NATIVE_WIDTH * GPU_FRAMEBUFFER_NATIVE_HEIGHT;
    ColorspaceApplyIntensityToBuffer16<false, false>((u16 *)displayInfo.masterNativeBuffer, pixCount, displayInfo.backlightIntensity[NDSDisplayID_Main]);
    ColorspaceApplyIntensityToBuffer16<false, false>((u16 *)displayInfo.masterNativeBuffer + pixCount, pixCount, displayInfo.backlightIntensity[NDSDisplayID_Touch]);

    return (u16*) displayInfo.masterNativeBuffer;
}

EXPORTED void desmume_draw_raw_as_rgbx(u8 *buffer)
{
    u16 *gpuFramebuffer = desmume_draw_raw();

    for (int i = 0; i < SCREENS_PIXEL_SIZE; i++) {
        buffer[(i * 4) + 2] = ((gpuFramebuffer[i] >> 0) & 0x1f) << 3;
        buffer[(i * 4) + 1] = ((gpuFramebuffer[i] >> 5) & 0x1f) << 3;
        buffer[(i * 4) + 0] = ((gpuFramebuffer[i] >> 10) & 0x1f) << 3;
    }
}

EXPORTED void desmume_savestate_clear()
{
    clear_savestates();
}

EXPORTED BOOL desmume_savestate_load(const char *file_name)
{
    return savestate_load(file_name);
}

EXPORTED BOOL desmume_savestate_save(const char *file_name)
{
    return savestate_save(file_name);
}

EXPORTED void desmume_savestate_scan()
{
    scan_savestates();
}

EXPORTED void desmume_savestate_slot_load(int index)
{
    loadstate_slot(index);
}

EXPORTED void desmume_savestate_slot_save(int index)
{
    savestate_slot(index);
}

EXPORTED BOOL desmume_savestate_slot_exists(int index)
{
    return savestates[index].exists;
}

EXPORTED char* desmume_savestate_slot_date(int index)
{
    return savestates[index].date;
}

EXPORTED BOOL desmume_gpu_get_layer_main_enable_state(int layer_index)
{
    return GPU->GetEngineMain()->GetLayerEnableState(layer_index);
}

EXPORTED BOOL desmume_gpu_get_layer_sub_enable_state(int layer_index)
{
    return GPU->GetEngineSub()->GetLayerEnableState(layer_index);
}

EXPORTED void desmume_gpu_set_layer_main_enable_state(int layer_index, BOOL the_state)
{
    GPU->GetEngineMain()->SetLayerEnableState(layer_index, the_state);
}

EXPORTED void desmume_gpu_set_layer_sub_enable_state(int layer_index, BOOL the_state)
{
    GPU->GetEngineSub()->SetLayerEnableState(layer_index, the_state);
}

EXPORTED int desmume_volume_get()
{
    return SNDSDLGetAudioVolume();
}
EXPORTED void desmume_volume_set(int volume)
{
    SNDSDLSetAudioVolume(volume);
}

EXPORTED unsigned char desmume_memory_read_byte(int address)
{
    return (unsigned char)(_MMU_read08<ARMCPU_ARM9>(address) & 0xFF);
}

EXPORTED signed char desmume_memory_read_byte_signed(int address)
{
    return (signed char)(_MMU_read08<ARMCPU_ARM9>(address) & 0xFF);
}

EXPORTED unsigned short desmume_memory_read_short(int address)
{
    return (unsigned short)(_MMU_read16<ARMCPU_ARM9>(address) & 0xFFFF);
}

EXPORTED signed short desmume_memory_read_short_signed(int address)
{
    return (signed short)(_MMU_read16<ARMCPU_ARM9>(address) & 0xFFFF);
}

EXPORTED unsigned long desmume_memory_read_long(int address)
{
    return (unsigned long)(_MMU_read32<ARMCPU_ARM9>(address));
}

EXPORTED signed long desmume_memory_read_long_signed(int address)
{
    return (signed long)(_MMU_read32<ARMCPU_ARM9>(address));
}

EXPORTED void desmume_memory_write_byte(int address, unsigned char value)
{
    _MMU_write08<ARMCPU_ARM9>(address, value);
}

EXPORTED void desmume_memory_write_short(int address, unsigned short value)
{
    _MMU_write16<ARMCPU_ARM9>(address, value);
}

EXPORTED void desmume_memory_write_long(int address, unsigned long value)
{
    _MMU_write32<ARMCPU_ARM9>(address, value);
}

struct registerPointerMap
{
    const char* registerName;
    unsigned int* pointer;
    int dataSize;
};

#define RPM_ENTRY(name,var) {name, (unsigned int*)&var, sizeof(var)},

registerPointerMap arm9PointerMap [] = {
	RPM_ENTRY("r0", NDS_ARM9.R[0])
	RPM_ENTRY("r1", NDS_ARM9.R[1])
	RPM_ENTRY("r2", NDS_ARM9.R[2])
	RPM_ENTRY("r3", NDS_ARM9.R[3])
	RPM_ENTRY("r4", NDS_ARM9.R[4])
	RPM_ENTRY("r5", NDS_ARM9.R[5])
	RPM_ENTRY("r6", NDS_ARM9.R[6])
	RPM_ENTRY("r7", NDS_ARM9.R[7])
	RPM_ENTRY("r8", NDS_ARM9.R[8])
	RPM_ENTRY("r9", NDS_ARM9.R[9])
	RPM_ENTRY("r10", NDS_ARM9.R[10])
	RPM_ENTRY("r11", NDS_ARM9.R[11])
	RPM_ENTRY("r12", NDS_ARM9.R[12])
	RPM_ENTRY("r13", NDS_ARM9.R[13])
	RPM_ENTRY("r14", NDS_ARM9.R[14])
	RPM_ENTRY("r15", NDS_ARM9.R[15])
	RPM_ENTRY("cpsr", NDS_ARM9.CPSR.val)
	RPM_ENTRY("spsr", NDS_ARM9.SPSR.val)
	{}
};
registerPointerMap arm7PointerMap [] = {
	RPM_ENTRY("r0", NDS_ARM7.R[0])
	RPM_ENTRY("r1", NDS_ARM7.R[1])
	RPM_ENTRY("r2", NDS_ARM7.R[2])
	RPM_ENTRY("r3", NDS_ARM7.R[3])
	RPM_ENTRY("r4", NDS_ARM7.R[4])
	RPM_ENTRY("r5", NDS_ARM7.R[5])
	RPM_ENTRY("r6", NDS_ARM7.R[6])
	RPM_ENTRY("r7", NDS_ARM7.R[7])
	RPM_ENTRY("r8", NDS_ARM7.R[8])
	RPM_ENTRY("r9", NDS_ARM7.R[9])
	RPM_ENTRY("r10", NDS_ARM7.R[10])
	RPM_ENTRY("r11", NDS_ARM7.R[11])
	RPM_ENTRY("r12", NDS_ARM7.R[12])
	RPM_ENTRY("r13", NDS_ARM7.R[13])
	RPM_ENTRY("r14", NDS_ARM7.R[14])
	RPM_ENTRY("r15", NDS_ARM7.R[15])
	RPM_ENTRY("cpsr", NDS_ARM7.CPSR.val)
	RPM_ENTRY("spsr", NDS_ARM7.SPSR.val)
	{}
};

struct cpuToRegisterMap
{
	const char* cpuName;
	registerPointerMap* rpmap;
}
cpuToRegisterMaps [] =
{
	{"arm9.", arm9PointerMap},
	{"main.", arm9PointerMap},
	{"arm7.", arm7PointerMap},
	{"sub.",  arm7PointerMap},
	{"", arm9PointerMap},
};

EXPORTED int desmume_memory_read_register(char* register_name)
{
	for(int cpu = 0; cpu < sizeof(cpuToRegisterMaps)/sizeof(*cpuToRegisterMaps); cpu++)
	{
		cpuToRegisterMap ctrm = cpuToRegisterMaps[cpu];
		int cpuNameLen = strlen(ctrm.cpuName);
		if(!strncasecmp(register_name, ctrm.cpuName, cpuNameLen))
		{
            register_name += cpuNameLen;
			for(int reg = 0; ctrm.rpmap[reg].dataSize; reg++)
			{
				registerPointerMap rpm = ctrm.rpmap[reg];
				if(!strcasecmp(register_name, rpm.registerName))
				{
					switch(rpm.dataSize)
					{ default:
					case 1: return *(unsigned char*)rpm.pointer;
					case 2: return *(unsigned short*)rpm.pointer;
					case 4: return *(unsigned long*)rpm.pointer;
					}
				}
			}
			return 0;
		}
	}
	return 0;
}

EXPORTED void desmume_memory_write_register(char* register_name, long value)
{
	for(int cpu = 0; cpu < sizeof(cpuToRegisterMaps)/sizeof(*cpuToRegisterMaps); cpu++)
	{
		cpuToRegisterMap ctrm = cpuToRegisterMaps[cpu];
		int cpuNameLen = strlen(ctrm.cpuName);
		if(!strncasecmp(register_name, ctrm.cpuName, cpuNameLen))
		{
			register_name += cpuNameLen;
			for(int reg = 0; ctrm.rpmap[reg].dataSize; reg++)
			{
				registerPointerMap rpm = ctrm.rpmap[reg];
				if(!strcasecmp(register_name, rpm.registerName))
				{
					switch(rpm.dataSize)
					{ default:
					case 1: *(unsigned char*)rpm.pointer = (unsigned char)(value & 0xFF); break;
					case 2: *(unsigned short*)rpm.pointer = (unsigned short)(value & 0xFFFF); break;
					case 4: *(unsigned long*)rpm.pointer = value; break;
					}
				}
			}
		}
	}
}

INLINE void memory_register_hook(int addr, MemHookType hook_type, int size, memory_cb_fnc cb)
{
	for(unsigned int i = addr; i != addr+size; i++)
	{
		hooks[hook_type][i] = cb;
	}
    std::vector<unsigned int> hooked_bytes;
    for(std::map<unsigned int, memory_cb_fnc>::iterator it = hooks[hook_type].begin(); it != hooks[hook_type].end(); ++it) {
        hooked_bytes.push_back(it->first);
    }
    hooked_regions[hook_type].Calculate(hooked_bytes);
}

EXPORTED void desmume_memory_register_write(int address, int size, memory_cb_fnc cb)
{
	memory_register_hook(address, HOOK_WRITE, size, cb);
}

EXPORTED void desmume_memory_register_read(int address, int size, memory_cb_fnc cb)
{
	memory_register_hook(address, HOOK_READ, size, cb);
}

EXPORTED void desmume_memory_register_exec(int address, int size, memory_cb_fnc cb)
{
	memory_register_hook(address, HOOK_EXEC, size, cb);
}

EXPORTED void desmume_screenshot(char *screenshot_buffer)
{
    u16 *gpuFramebuffer = (u16 *)GPU->GetDisplayInfo().masterNativeBuffer;
    static int seq = 0;

    for (int i = 0; i < SCREENS_PIXEL_SIZE; i++) {
        screenshot_buffer[(i * 3) + 0] = ((gpuFramebuffer[i] >> 0) & 0x1f) << 3;
        screenshot_buffer[(i * 3) + 1] = ((gpuFramebuffer[i] >> 5) & 0x1f) << 3;
        screenshot_buffer[(i * 3) + 2] = ((gpuFramebuffer[i] >> 10) & 0x1f) << 3;
    }

}

EXPORTED BOOL desmume_input_joy_init(void)
{
    return (BOOL) init_joy();
}

EXPORTED void desmume_input_joy_uninit(void)
{
    return uninit_joy();
}

EXPORTED u16 desmume_input_joy_number_connected(void)
{
    return nbr_joy;
}

EXPORTED u16 desmume_input_joy_get_key(int index)
{
    return get_joy_key(index);
}

EXPORTED u16 desmume_input_joy_get_set_key(int index)
{
    return get_set_joy_key(index);
}

EXPORTED void desmume_input_joy_set_key(int index, int joystick_key_index)
{
    joypad_cfg[index] = joystick_key_index;
}

EXPORTED void desmume_input_keypad_update(u16 keys)
{
    update_keypad(keys);
}

EXPORTED u16 desmume_input_keypad_get(void)
{
    return get_keypad();
}

EXPORTED void desmume_input_set_touch_pos(u16 x, u16 y)
{
    NDS_setTouchPos(x, y);
}

EXPORTED void desmume_input_release_touch()
{
    NDS_releaseTouch();
}

EXPORTED BOOL desmume_movie_is_active()
{
    return movieMode != MOVIEMODE_INACTIVE;
}

EXPORTED BOOL desmume_movie_is_recording()
{
    return movieMode == MOVIEMODE_RECORD;
}

EXPORTED BOOL desmume_movie_is_playing()
{
    return movieMode == MOVIEMODE_PLAY;
}

EXPORTED BOOL desmume_movie_is_finished()
{
    return movieMode == MOVIEMODE_FINISHED;
}

EXPORTED int desmume_movie_get_length()
{
    return currMovieData.records.size();
}

EXPORTED char *desmume_movie_get_name()
{
    extern char curMovieFilename[512];
    return curMovieFilename;
}

EXPORTED int desmume_movie_get_rerecord_count()
{
    return currMovieData.rerecordCount;
}

EXPORTED void desmume_movie_set_rerecord_count(int count)
{
    currMovieData.rerecordCount = count;
}

EXPORTED BOOL desmume_movie_get_readonly()
{
    return movie_readonly;
}

EXPORTED void desmume_movie_set_readonly(BOOL state)
{
    movie_readonly = state;
}

EXPORTED const char* desmume_movie_play(const char *file_name)
{
    return FCEUI_LoadMovie(file_name, true, false, 0);
}

EXPORTED void desmume_movie_record_simple(const char *save_file_name, const char *author_name)
{
    std::string s_author_name = author_name;
    FCEUI_SaveMovie(save_file_name, s2ws(s_author_name), START_BLANK, "", DateTime::get_Now());
}

EXPORTED void desmume_movie_record(const char *save_file_name, const char *author_name, START_FROM start_from, const char* sram_file_name)
{
    std::string s_author_name = author_name;
    std::string s_sram_file_name = sram_file_name;
    FCEUI_SaveMovie(save_file_name, s2ws(s_author_name), start_from, s_sram_file_name, DateTime::get_Now());
}

EXPORTED void desmume_movie_record_from_date(const char *save_file_name, const char *author_name, START_FROM start_from, const char* sram_file_name, SimpleDate date)
{
    std::string s_author_name = author_name;
    std::string s_sram_file_name = sram_file_name;
    FCEUI_SaveMovie(save_file_name, s2ws(s_author_name), start_from, s_sram_file_name,
            DateTime(date.year, date.month, date.day, date.hour, date.minute, date.second, date.millisecond));
}

EXPORTED void desmume_movie_replay()
{
    if (movieMode != MOVIEMODE_INACTIVE) {
        extern char curMovieFilename[512];
        desmume_movie_play(curMovieFilename);
    }
}
EXPORTED void desmume_movie_stop()
{
    FCEUI_StopMovie();
}
