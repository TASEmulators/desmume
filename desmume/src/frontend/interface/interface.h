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

#ifndef DESMUME_INTERFACE_H
#define DESMUME_INTERFACE_H

#include "../../types.h"
#include "../../movie.h"

#ifdef HAVE_GL_GL_H
#define INCLUDE_OPENGL_2D
#endif

#ifdef INCLUDE_OPENGL_2D
#include <GL/gl.h>
#include <GL/glext.h>
#endif

// Define EXPORTED for any platform
#ifdef _WIN32
# ifdef WIN_EXPORT
#   define EXPORTED  __declspec( dllexport )
# else
#   define EXPORTED  __declspec( dllimport )
# endif
#else
# define EXPORTED
#endif

enum MemHookType
{
    HOOK_WRITE,
    HOOK_READ,
    HOOK_EXEC,

    HOOK_COUNT
};

extern "C" {
// callback for memory hooks (get's two values: address and size of operation that triggered the hook)
typedef BOOL (*memory_cb_fnc)(unsigned int, int);

struct SimpleDate {
    int year;
    int month;
    int day;
    int hour;
    int minute;
    int second;
    int millisecond;
};

EXPORTED int desmume_init(void);
EXPORTED void desmume_free(void);

// 0 = Japanese, 1 = English, 2 = French, 3 = German, 4 = Italian, 5 = Spanish
EXPORTED void desmume_set_language(u8 language);
EXPORTED int desmume_open(const char *filename);
EXPORTED void desmume_set_savetype(int type);
EXPORTED void desmume_pause(void);
EXPORTED void desmume_resume(void);
EXPORTED void desmume_reset(void);
EXPORTED BOOL desmume_running(void);
EXPORTED void desmume_skip_next_frame(void);
EXPORTED void desmume_cycle(BOOL with_joystick);

EXPORTED int desmume_sdl_get_ticks();

// Drawing is either supported manually via a custom OpenGL texture...
#ifdef INCLUDE_OPENGL_2D
EXPORTED void desmume_draw_opengl(GLuint *texture);
#endif
EXPORTED BOOL desmume_has_opengl();
// ... or via an SDL window created by Desmume.
EXPORTED int desmume_draw_window_init(BOOL auto_pause, BOOL use_opengl_if_possible);
EXPORTED void desmume_draw_window_input();
EXPORTED void desmume_draw_window_frame();
EXPORTED BOOL desmume_draw_window_has_quit();
EXPORTED void desmume_draw_window_free();
// ... or have fun working with the display buffer directly
EXPORTED u16 *desmume_draw_raw();
EXPORTED void desmume_draw_raw_as_rgbx(u8 *buffer);

EXPORTED void desmume_savestate_clear();
EXPORTED BOOL desmume_savestate_load(const char *file_name);
EXPORTED BOOL desmume_savestate_save(const char *file_name);
EXPORTED void desmume_savestate_scan();
EXPORTED void desmume_savestate_slot_load(int index);
EXPORTED void desmume_savestate_slot_save(int index);
EXPORTED BOOL desmume_savestate_slot_exists(int index);
EXPORTED char* desmume_savestate_slot_date(int index);

EXPORTED BOOL desmume_gpu_get_layer_main_enable_state(int layer_index);
EXPORTED BOOL desmume_gpu_get_layer_sub_enable_state(int layer_index);
EXPORTED void desmume_gpu_set_layer_main_enable_state(int layer_index, BOOL the_state);
EXPORTED void desmume_gpu_set_layer_sub_enable_state(int layer_index, BOOL the_state);

EXPORTED int desmume_volume_get();
EXPORTED void desmume_volume_set(int volume);

EXPORTED unsigned char desmume_memory_read_byte(int address);
EXPORTED signed char desmume_memory_read_byte_signed(int address);
EXPORTED unsigned short desmume_memory_read_short(int address);
EXPORTED signed short desmume_memory_read_short_signed(int address);
EXPORTED unsigned long desmume_memory_read_long(int address);
EXPORTED signed long desmume_memory_read_long_signed(int address);
//EXPORTED unsigned char *desmume_memory_read_byterange(int address, int length);

EXPORTED void desmume_memory_write_byte(int address, unsigned char value);
EXPORTED void desmume_memory_write_short(int address, unsigned short value);
EXPORTED void desmume_memory_write_long(int address, unsigned long value);
//EXPORTED void desmume_memory_write_byterange(int address, int length, const unsigned char *bytes);

EXPORTED int desmume_memory_read_register(char* register_name);
EXPORTED void desmume_memory_write_register(char* register_name, long value);

EXPORTED void desmume_memory_register_write(int address, int size, memory_cb_fnc cb);
EXPORTED void desmume_memory_register_read(int address, int size, memory_cb_fnc cb);
EXPORTED void desmume_memory_register_exec(int address, int size, memory_cb_fnc cb);

// Buffer must have the size 98304*3
EXPORTED void desmume_screenshot(char *screenshot_buffer);

// For automatic input processing with the SDL window, see desmume_draw_sdl_input.
EXPORTED BOOL desmume_input_joy_init(void);
EXPORTED void desmume_input_joy_uninit(void);
EXPORTED u16 desmume_input_joy_number_connected(void);
EXPORTED u16 desmume_input_joy_get_key(int index);
EXPORTED u16 desmume_input_joy_get_set_key(int index);
EXPORTED void desmume_input_joy_set_key(int index, int joystick_key_index);

EXPORTED void desmume_input_keypad_update(u16 keys);
EXPORTED u16 desmume_input_keypad_get(void);

EXPORTED void desmume_input_set_touch_pos(u16 x, u16 y);
EXPORTED void desmume_input_release_touch();

EXPORTED BOOL desmume_movie_is_active();
EXPORTED BOOL desmume_movie_is_recording();
EXPORTED BOOL desmume_movie_is_playing();
EXPORTED BOOL desmume_movie_is_finished();
EXPORTED int desmume_movie_get_length();
EXPORTED char *desmume_movie_get_name();
EXPORTED int desmume_movie_get_rerecord_count();
EXPORTED void desmume_movie_set_rerecord_count(int count);
EXPORTED BOOL desmume_movie_get_readonly();
EXPORTED void desmume_movie_set_readonly(BOOL state);
// Returns NULL on success, error message otherwise.
EXPORTED const char *desmume_movie_play(const char *file_name);
EXPORTED void desmume_movie_record_simple(const char *save_file_name, const char *author_name);
EXPORTED void desmume_movie_record(const char *save_file_name, const char *author_name, START_FROM start_from, const char* sram_file_name);
EXPORTED void desmume_movie_record_from_date(const char *save_file_name, const char *author_name, START_FROM start_from, const char* sram_file_name, SimpleDate date);
EXPORTED void desmume_movie_replay();
EXPORTED void desmume_movie_stop();

};

// TODO: Below is mostly just from lua-engine.h, might think about how to get rid of the code duplication.

#include <vector>
#include <algorithm>

// the purpose of this structure is to provide a way of
// QUICKLY determining whether a memory address range has a hook associated with it,
// with a bias toward fast rejection because the majority of addresses will not be hooked.
// (it must not use any part of Lua or perform any per-script operations,
//  otherwise it would definitely be too slow.)
// calculating the regions when a hook is added/removed may be slow,
// but this is an intentional tradeoff to obtain a high speed of checking during later execution
struct TieredRegion
{
    template<unsigned int maxGap>
    struct Region
    {
        struct Island
        {
            unsigned int start;
            unsigned int end;
            FORCEINLINE bool Contains(unsigned int address, int size) const { return address < end && address+size > start; }
        };
        std::vector<Island> islands;

        void Calculate(const std::vector<unsigned int>& bytes)
        {
            islands.clear();

            unsigned int lastEnd = ~0;

            std::vector<unsigned int>::const_iterator iter = bytes.begin();
            std::vector<unsigned int>::const_iterator end = bytes.end();
            for(; iter != end; ++iter)
            {
                unsigned int addr = *iter;
                if(addr < lastEnd || addr > lastEnd + (long long)maxGap)
                {
                    islands.push_back(Island());
                    islands.back().start = addr;
                }
                islands.back().end = addr+1;
                lastEnd = addr+1;
            }
        }

        bool Contains(unsigned int address, int size) const
        {
            typename std::vector<Island>::const_iterator iter = islands.begin();
            typename std::vector<Island>::const_iterator end = islands.end();
            for(; iter != end; ++iter)
                if(iter->Contains(address, size))
                    return true;
            return false;
        }
    };

    Region<0xFFFFFFFF> broad;
    Region<0x1000> mid;
    Region<0> narrow;

    void Calculate(std::vector<unsigned int>& bytes)
    {
        std::sort(bytes.begin(), bytes.end());

        broad.Calculate(bytes);
        mid.Calculate(bytes);
        narrow.Calculate(bytes);
    }

    TieredRegion()
    {
        std::vector<unsigned int> somevector;
        Calculate(somevector);
    }

    FORCEINLINE int NotEmpty()
    {
        return broad.islands.size();
    }

    // note: it is illegal to call this if NotEmpty() returns 0
    FORCEINLINE bool Contains(unsigned int address, int size)
    {
        return broad.islands[0].Contains(address,size) &&
               mid.Contains(address,size) &&
               narrow.Contains(address,size);
    }
};
extern TieredRegion hooked_regions [HOOK_COUNT];

extern std::map<unsigned int, memory_cb_fnc> hooks[HOOK_COUNT];

FORCEINLINE void call_registered_interface_mem_hook(unsigned int address, int size, MemHookType hook_type)
{
    // See notes for CallRegisteredLuaMemHook!
    if(hooked_regions[hook_type].NotEmpty())
    {
        if(hooked_regions[hook_type].Contains(address, size))
        {
            for(int i = address; i != address+size; i++)
            {
                memory_cb_fnc hook = hooks[hook_type][i];
                if(hook != 0)
                {
                    (*hook)(address, size);
                    break;
                }
            }
        }
    }
}

#endif //DESMUME_INTERFACE_H
