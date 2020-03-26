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

#include <SDL.h>
#include <../../types.h>

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

extern "C" {
// callback for memory hooks: if it returns false, remove it.
typedef BOOL (*memory_cb_fnc)(void);

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
EXPORTED void desmume_cycle(void);

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

EXPORTED int desmume_sram_load(const char *file_name);
EXPORTED int desmume_sram_save(const char *file_name);

EXPORTED void desmume_savestate_clear();
EXPORTED BOOL desmume_savestate_load(const char *file_name);
EXPORTED BOOL desmume_savestate_save(const char *file_name);
EXPORTED void desmume_savestate_scan();
EXPORTED void desmume_savestate_slot_load(int index);
EXPORTED void desmume_savestate_slot_save(int index);
EXPORTED BOOL desmume_savestate_slot_exists(int index);
EXPORTED char* desmume_savestate_slot_date(int index);

EXPORTED void desmume_savestate_load_slot(int num);
EXPORTED void desmume_savestate_save_slot(int num);

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
EXPORTED unsigned char *desmume_memory_read_byterange(int address, int length);

EXPORTED void desmume_memory_write_byte(int address, unsigned char value);
EXPORTED void desmume_memory_write_byte_signed(int address, signed char value);
EXPORTED void desmume_memory_write_short(int address, unsigned short value);
EXPORTED void desmume_memory_write_short_signed(int address, signed short value);
EXPORTED void desmume_memory_write_long(int address, unsigned long value);
EXPORTED void desmume_memory_write_long_signed(int address, signed long value);
EXPORTED void desmume_memory_write_byterange(int address, const unsigned char *bytes);

EXPORTED long desmume_memory_read_register(char* register_name);
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
EXPORTED char *desmume_movie_get_mode();
EXPORTED int desmume_movie_get_length();
EXPORTED char *desmume_movie_get_name();
EXPORTED int desmume_movie_get_rerecord_count();
EXPORTED void desmume_movie_set_rerecord_count(int count);
EXPORTED BOOL desmume_movie_get_rerecord_counting();
EXPORTED void desmume_movie_set_rerecord_counting(BOOL state);
EXPORTED BOOL desmume_movie_get_readonly();
EXPORTED void desmume_movie_set_readonly(BOOL state);
EXPORTED void desmume_movie_play(/* ??? */);
EXPORTED void desmume_movie_replay();
EXPORTED void desmume_movie_stop();

EXPORTED void desmume_gui_pixel(int x, int y, int color);
EXPORTED void desmume_gui_line(int x1, int y1, int x2, int y2, int color);
EXPORTED void desmume_gui_box(int x1, int y1, int x2, int y2, int fillcolor, int outlinecolor);
EXPORTED void desmume_gui_box(int x1, int y1, int x2, int y2, int fillcolor, int outlinecolor);
EXPORTED void desmume_gui_text(int x, int y, char *str, int textcolor, BOOL has_backcolor, int backcolor);
EXPORTED void desmume_gui_gdoverlay(int dx, int dy, char* str, int alphamul);
EXPORTED void desmume_gui_opacity(int alpha);

};
#endif //DESMUME_INTERFACE_H
