/*
	Copyright (C) 2026 DeSmuME Team

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

#ifndef __MACLUASCRIPTCONSOLE_H__
#define __MACLUASCRIPTCONSOLE_H__

#ifdef HAVE_LUA



#ifdef __cplusplus
extern "C" {
#endif

void lua_script_open_console(void);
void lua_script_close_all(void);

// Back buffer: the Lua engine draws into this on the emulation core thread.
uint32_t* lua_script_get_graphics_buffer(void);
// Clears the back buffer at the start of a frame, before GUI draws are flushed.
void lua_script_clear_graphics_buffer(void);
// Publishes the completed back buffer to the front buffer for the renderer.
// Called once per frame after the frame's GUI draw calls have been flushed.
void lua_script_present_graphics_buffer(void);

// Front buffer: a stable snapshot of the last completed frame for the render
// thread to upload. lock returns the buffer with the overlay mutex held; the
// caller must call unlock once the upload is finished.
uint32_t* lua_script_lock_overlay_buffer(void);
void lua_script_unlock_overlay_buffer(void);

#ifdef __cplusplus
}
#endif

#endif /* HAVE_LUA */

#endif /* __MACLUASCRIPTCONSOLE_H__ */
