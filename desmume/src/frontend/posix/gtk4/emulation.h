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

/* Run exactly one frame even while paused (the "Frame advance" action). */
gboolean desmume_emu_step_frame_advance(void);

/* Frame pacing / speed controls (P3d).  These mirror the GTK3 front end's
 * config.fpslimiter / autoframeskip / frameskip and the KEY_BOOST latch:
 *   - fps limiter on: the 60 Hz source is the pace; when the emulation falls
 *     behind, auto frameskip runs extra unrendered core frames to catch up.
 *   - fps limiter off or fast forward: run several frames per tick with no
 *     pacing, so the emulation speeds up.
 * config.frameskip doubles as the fixed skip count and the auto-skip cap,
 * exactly like GTK3. */
void desmume_emu_set_fps_limiter(gboolean enabled);
gboolean desmume_emu_get_fps_limiter(void);
void desmume_emu_set_auto_frameskip(gboolean enabled);
gboolean desmume_emu_get_auto_frameskip(void);
void desmume_emu_set_frameskip(int value);
int desmume_emu_get_frameskip(void);
void desmume_emu_set_fast_forward(gboolean enabled);
gboolean desmume_emu_get_fast_forward(void);

/* Emulated frames per second over a rolling one-second window, including
 * frames that were skipped (matches GTK3's title-bar FPS counter). */
guint desmume_emu_get_fps(void);

/* Read a raw IO register for the read-only IO-registers tool.  `cpu` is 0 for
 * ARM9 and 1 for ARM7; values are returned even while paused.
 */
typedef enum
{
	DESMUME_IOREG_IME = 0,
	DESMUME_IOREG_IE,
	DESMUME_IOREG_IF,
	DESMUME_IOREG_IPCFIFOCNT,
	DESMUME_IOREG_POWCNT,
	DESMUME_IOREG_SPICNT
} DesmumeIOReg;

guint32 desmume_emu_read_ioreg(int cpu, DesmumeIOReg reg);

/* Add an internal ("Add Cheat") constant-write entry from RAM Search.  The
 * address must be inside main RAM and a ROM must be loaded.  Returns FALSE
 * otherwise; *error_message, when set, receives an owned UTF-8 string. */
gboolean desmume_emu_add_cheat(guint32 address, int sizeInBytes, guint32 value,
                               const char *description, char **error_message);

/* ---- movie playback / recording (P4b) -------------------------------- */

/* Start-from modes mirror START_FROM in movie.h; kept as an enum here so the
 * GTK layer never has to include the core header. */
typedef enum
{
	DESMUME_MOVIE_START_BLANK = 0,
	DESMUME_MOVIE_START_SRAM = 1,
	DESMUME_MOVIE_START_SAVESTATE = 2
} DesmumeMovieStartFrom;

/* All functions return TRUE on success; on failure *error_message receives an
 * owned UTF-8 string when requested.  A movie must only be started with a ROM
 * loaded; StopMovie is always safe. */
gboolean desmume_emu_load_movie(const char *path, gboolean read_only,
                                int pause_frame, char **error_message);
gboolean desmume_emu_save_movie(const char *path, const char *author,
                                DesmumeMovieStartFrom start_from,
                                const char *sram_path,
                                int year, int month, int day,
                                int hour, int minute, int second,
                                const char *description, char **error_message);
void desmume_emu_stop_movie(void);
gboolean desmume_emu_movie_active(void);

/* Base name of the loaded ROM path without its extension, or "" when no ROM
 * is loaded.  Owned string; used to seed movie file names. */
char *desmume_emu_rom_base_name(void);

/* Keyvals use the GTK3 default keymap.  Touch coordinates are in NDS logical
 * space (0..255 / 0..191), exactly what DesmumeDisplay's signals emit. */
void desmume_emu_set_key(guint keyval, gboolean pressed);
void desmume_emu_touch(double x, double y);
void desmume_emu_release_touch(void);

G_END_DECLS

#endif /* DESMUME_GTK4_EMULATION_H_ */
