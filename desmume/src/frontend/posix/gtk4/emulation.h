/*
	emulation.h - core bridge for the DeSmuME GTK4 frontend

	Copyright (C) 2026 DeSmuME team

	This file is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 2 of the License, or
	(at your option) any later version.
*/

#ifndef DESMUME_GTK4_EMULATION_H_
#define DESMUME_GTK4_EMULATION_H_

#include <gtk/gtk.h>

G_BEGIN_DECLS

/* Called once per emulated frame with a BGRA8888 buffer containing both
 * screens stacked vertically (first the main screen, then the touch screen).
 * The buffer belongs to the emulation layer and stays valid for the call. */
typedef void (*DesmumeFrameSink)(const guchar *bgra, int width, int height,
                                 gpointer user_data);

gboolean desmume_emu_init(void);
void desmume_emu_shutdown(void);

gboolean desmume_emu_load_rom(const char *path, char **error_message);
void desmume_emu_unload_rom(void);
gboolean desmume_emu_has_rom(void);
const char *desmume_emu_rom_name(void);

void desmume_emu_set_running(gboolean running);
gboolean desmume_emu_get_running(void);

/* Reset the running ROM in place (equivalent of the core's NDS_Reset()). */
void desmume_emu_reset(void);

/* Savestate slots are numbered 0..9 (the UI shows them as 1..10). */
gboolean desmume_emu_save_state(int slot);
gboolean desmume_emu_load_state(int slot);
void desmume_emu_scan_states(void);
gboolean desmume_emu_state_exists(int slot);

/* Save a PNG screenshot of the current frame (both screens) to path.
 * Returns TRUE on success; on failure *error_message holds a owned string. */
gboolean desmume_emu_screenshot(const char *path, char **error_message);

void desmume_emu_set_frame_sink(DesmumeFrameSink sink, gpointer user_data);

/* Run one frame and push it to the frame sink.  Safe to call with no ROM. */
void desmume_emu_step(void);

/* Keyvals use the GTK3 default keymap.  Touch coordinates are in NDS logical
 * space (0..255 / 0..191), exactly what DesmumeDisplay's signals emit. */
void desmume_emu_set_key(guint keyval, gboolean pressed);
void desmume_emu_touch(double x, double y);
void desmume_emu_release_touch(void);

G_END_DECLS

#endif /* DESMUME_GTK4_EMULATION_H_ */
