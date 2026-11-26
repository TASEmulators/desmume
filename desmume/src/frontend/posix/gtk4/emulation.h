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
 *
 * `frame` is a GBytes whose data is the opaque BGRA8888 frame; its lifetime is
 * managed by the reference count. The sink must keep a reference for as long
 * as it needs the pixels (GTK4's GdkMemoryTexture does exactly that) and
 * unref it when done. This is what lets the widget build textures with zero
 * copies instead of memcpy-ing the frame a second time. */
typedef void (*DesmumeFrameSink)(GBytes *frame, int width, int height,
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

/* ---- ROM information / battery backup ------------------------------- */

/* One label/value pair of the ROM Info dialog.  Both strings belong to the
 * row and are released by desmume_emu_rom_info_free(). */
typedef struct
{
	char *label;
	char *value;
} DesmumeRomInfoRow;

/* Describe the loaded ROM (title, game code, maker, cartridge and loaded
 * size, CRC32, save type, CPU and 3D engine).  Returns a NULL-terminated
 * array, or NULL when no ROM is loaded. */
DesmumeRomInfoRow *desmume_emu_rom_info(void);
void desmume_emu_rom_info_free(DesmumeRomInfoRow *rows);

/* Import a raw/Action Replay battery save into the loaded ROM's backup
 * device, or write the current battery contents out.  Pause the emulation
 * first: both replace the whole backup device contents.  On failure
 * *error_message receives an owned string when requested. */
gboolean desmume_emu_import_backup(const char *path, char **error_message);
gboolean desmume_emu_export_backup(const char *path, char **error_message);

/* Manual save type, in mc.h order: 0 = autodetect, 1..6 = EEPROM 4/64/512
 * kbit, FRAM 256 kbit, FLASH 2/4 Mbit.  The backup device is re-detected and
 * the existing save file is left untouched. */
void desmume_emu_set_save_type(int type);
int desmume_emu_get_save_type(void);

/* Parse an ADVANsCEne XML database into the binary .ddb the core reads next
 * to the executable, so later ROM loads can pick the save type from it.
 * Returns TRUE and stores the record count in *record_count on success. */
gboolean desmume_emu_import_advanscene(const char *xml_path,
                                       guint *record_count,
                                       char **error_message);

/* ---- GPU layer visibility -------------------------------------------- */

/* Layers 0..4 are the main engine's BG 0..3 and OBJ, 5..9 the sub engine's
 * BG 0..3 and OBJ.  The state is per session and follows the emulation
 * rather than the config file, like the GTK3 View > Layers menu.  Returns
 * FALSE when no core is running yet, so the caller can leave the menu check
 * mark alone. */
gboolean desmume_emu_set_layer_enabled(int layer, gboolean enabled);
gboolean desmume_emu_get_layer_enabled(int layer);

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

/* Globally stop the core from applying the cheat list without removing any
 * entry (the "Disable cheats" menu switch).  The list itself is untouched. */
void desmume_emu_set_cheats_disabled(gboolean disabled);
gboolean desmume_emu_get_cheats_disabled(void);

/* ---- P7a: WebGPU off-screen GPU filters ----------------------------- */

/* Configure the optional GPU upscaler filter that runs between the core frame
 * and the display.  `upscaler` is a WGPUPresentUpscaler value (0 = off);
 * `scale` is the linear upscale factor applied to the two screens (>= 1).
 * Both are mirrored here as plain ints/floats so the GTK layer never needs the
 * WebGPU headers. Calling with upscaler 0 disables the path entirely and
 * releases the WebGPU device. Returns TRUE when the filter was accepted; a
 * build without WebGPU simply returns FALSE and the plain frame path is used. */
gboolean desmume_emu_set_gpu_filter(int upscaler, float scale);

/* Current GPU filter settings (0 / 1.0 when the path is off). */
int desmume_emu_get_gpu_filter(void);
float desmume_emu_get_gpu_scale(void);

/* Whether a GPU filter is actually running (device created and filter not
 * off). Useful for the menu tick / status readout. */
gboolean desmume_emu_gpu_filter_active(void);

/* Diagnostic: run one synthetic frame through the active off-screen filter
 * and report whether the GPU render + readback completed.  Does not touch the
 * display sink. Returns TRUE only when a filter is active and the whole path
 * (render, map, copy) succeeded; used by automated smoke tests. */
gboolean desmume_emu_self_test_gpu_filter(void);

/* Screen presentation backend (the GTK4 counterpart of GTK3's
 * "Screen presentation" combo).  GTK4 presents through a GdkTexture, so the
 * WebGPU path is the off-screen filter above: choosing a backend pins the
 * WGPUInstanceBackend the filter device is created on.  A build without
 * WebGPU reports zero entries, in which case only the software path exists.
 * Index 0..count-1; mask() is the WGPUInstanceBackend bit to store in the
 * config, label() a short backend name ("Vulkan", "OpenGL", ...). */
int desmume_emu_presentation_backend_count(void);
const char *desmume_emu_presentation_backend_label(int index);
guint32 desmume_emu_presentation_backend_mask(int index);

/* Select the instance backend for the off-screen filter, or 0 to let
 * wgpu-native choose.  Changing it while a filter is active rebuilds the
 * device on the new backend.  Call before desmume_emu_set_gpu_filter() at
 * startup, or any time afterwards to switch live. */
void desmume_emu_set_presentation_backend(guint32 backend_mask);
guint32 desmume_emu_get_presentation_backend(void);

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

/* Mirror the GTK config's Audio section into the core.  Call before
 * desmume_emu_init(); the settings are applied when the core starts.  The
 * individual setters also work at runtime and update a live sound core. */
void desmume_emu_set_audio_config(gboolean enabled, int volume,
                                  int sync_mode, int interpolation);
void desmume_emu_set_audio_enabled(gboolean enabled);
void desmume_emu_set_audio_volume(int volume);
void desmume_emu_set_audio_sync(int sync_mode);
void desmume_emu_set_audio_interpolation(int interpolation);

/* ---- Emulation / graphics config -------------------------------------- */

/* Configure the ARM dynamic recompiler.  Call before desmume_emu_init(); the
 * setting is applied when the core starts and may also be changed live.  On
 * builds without HAVE_JIT the request is remembered but has no effect. */
void desmume_emu_set_jit_config(gboolean enabled, int block_size);

/* The 3D renderer list compiled into this frontend.  Index 0..count-1, with
 * desmume_emu_renderer_id() returning the RendererID to store in the config. */
int desmume_emu_renderer_count(void);
const char *desmume_emu_renderer_name(int index);
int desmume_emu_renderer_id(int index);

/* RendererID currently in use, or the one that will be selected at init. */
int desmume_emu_get_renderer(void);

/* Mirror the Graphics tab of the config dialog into the core.  Call before
 * desmume_emu_init(); the values are applied at init and can be changed live.
 * `texture_upscale` is 1, 2 or 4 and `multisample_size` is one of
 * 0/2/4/8/16/32. */
void desmume_emu_set_graphics_config(int renderer, int texture_upscale,
                                     gboolean deposterize, gboolean smoothing,
                                     gboolean high_color_interpolation,
                                     int multisample_size);

/* ---- Emulation / BIOS / firmware (read while the core resets) ---------- */

/* Values the core only reads while it boots: bus timing, game hacks, the
 * debug console and Ensata emulation.  Changing any of them after a ROM has
 * been loaded needs a reset (see desmume_emu_reset()). */
void desmume_emu_set_cpu_options(gboolean advanced_timing, gboolean game_hacks,
                                 gboolean debug_console, gboolean ensata);

/* External ARM9/ARM7 BIOS images.  Paths are copied into the core's fixed
 * buffers; an empty path restores the default file name. */
void desmume_emu_set_bios_config(gboolean use_external,
                                 const char *arm9_path, const char *arm7_path,
                                 gboolean swi_from_bios, gboolean patch_swi3);

/* External firmware image ("Boot from firmware" needs both an external BIOS
 * and an external firmware image, matching the Windows dialog). */
void desmume_emu_set_firmware_image_config(gboolean use_external,
                                           const char *path,
                                           gboolean boot_from_firmware,
                                           gboolean use_external_settings);

/* The user name/message/birthday/colour/MAC written into the internal
 * firmware image.  UTF-8 in, converted to the firmware's UTF-16 units here so
 * the GTK layer never deals with the encoding.  `mac_address` accepts the
 * usual 00:09:BF:... spelling or 12 bare hex digits. */
void desmume_emu_set_firmware_user_config(const char *nickname,
                                          const char *message,
                                          int favorite_color,
                                          int birth_month, int birth_day,
                                          int language, const char *mac_address);

/* ---- 3D renderer options ---------------------------------------------- */

/* The switches the graphics tab shares with GTK3/Windows.  The accuracy
 * options are baked into the OpenGL shaders, so changing one rebuilds the
 * active renderer; the software and WebGPU renderers read them per frame.
 * `color_depth` is 15, 18 or 24 bits. */
void desmume_emu_set_gfx3d_config(gboolean edge_mark, gboolean fog,
                                  gboolean texture, gboolean line_hack,
                                  gboolean txt_hack, gboolean shadow_polygon,
                                  gboolean zero_alpha_blending,
                                  gboolean nds_depth_calculation,
                                  gboolean depth_lequal_facing,
                                  int color_depth);

/* ---- Slot 1 / Slot 2 addons ------------------------------------------- */

/* Device lists for the addon combo boxes.  Names are owned by the core and
 * live for the whole process; ids are what the config stores.  The lists are
 * populated on first use (slot1_Init()/slot2_Init()). */
int desmume_emu_slot1_count(void);
const char *desmume_emu_slot1_name(int index);
const char *desmume_emu_slot1_description(int index);
int desmume_emu_slot1_id(int index);
/* Which of the optional rows apply to the device at `index`: the R4 needs a
 * FAT folder, the retail debug device a filesystem folder. */
gboolean desmume_emu_slot1_has_fat(int index);
gboolean desmume_emu_slot1_has_debug(int index);
char *desmume_emu_slot1_default_debug_path(void);

int desmume_emu_slot2_count(void);
const char *desmume_emu_slot2_name(int index);
const char *desmume_emu_slot2_description(int index);
int desmume_emu_slot2_id(int index);
/* The CompactFlash device takes an image/folder plus a mode; the GBA
 * cartridge device takes a ROM file. */
gboolean desmume_emu_slot2_has_cflash(int index);
gboolean desmume_emu_slot2_has_gba_rom(int index);

/* `id` is the addon id from the list above (0xFE = auto).  The FAT folder is
 * only used by the R4 device, the debug folder only by the retail debug
 * device; both are ignored otherwise.  Returns FALSE when the core refused
 * the device, in which case the previous one stays connected. */
gboolean desmume_emu_set_slot1_config(int id, const char *fat_dir,
                                      gboolean fat_same_as_rom,
                                      const char *debug_dir);
gboolean desmume_emu_set_slot2_config(int id, int cflash_mode,
                                      const char *cflash_path,
                                      const char *gba_rom_path);

/* ---- Wi-Fi / microphone ----------------------------------------------- */

/* Number of bridge adapters libpcap reported.  A build without Wi-Fi support,
 * or a machine without libpcap, reports 0; *error_message then explains why
 * (owned string) so the dialog can show the same hint GTK3 does. */
int desmume_emu_wifi_bridge_count(char **error_message);
const char *desmume_emu_wifi_bridge_name(int index);

/* Turn Wi-Fi emulation on/off and pick the bridge adapter.  A build without
 * EXPERIMENTAL_WIFI_COMM keeps the core off no matter what is stored. */
void desmume_emu_set_wifi_config(gboolean enabled, gboolean compatibility_mode,
                                 int bridge_index);

/* `mode` is a MicMode value (0 = internal noise, 1 = samples, 2 = random,
 * 3 = physical device).  Returns FALSE when the sample file cannot be used;
 * the previous source stays active in that case. */
gboolean desmume_emu_set_mic_config(int mode, const char *sample_file);


/* Keyvals use the GTK3 default keymap.  Touch coordinates are in NDS logical
 * space (0..255 / 0..191), exactly what DesmumeDisplay's signals emit. */
void desmume_emu_set_key(guint keyval, gboolean pressed);
void desmume_emu_touch(double x, double y);
void desmume_emu_release_touch(void);

G_END_DECLS

#endif /* DESMUME_GTK4_EMULATION_H_ */
