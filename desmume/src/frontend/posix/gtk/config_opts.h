/*
	Copyright (C) 2014 DeSmuME team
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
/*
	This file contains a list of config options.

	#define OPT(name, type, default, section, key)
*/

/* View */
OPT(view_orient, int, 0, View, ScreenLayout)
OPT(view_swap, bool, false, View, SwapScreens)
OPT(view_rot, int, 0, View, Rotation)
OPT(view_gap, bool, false, View, ScreenGap)
OPT(view_filter, int, 0, View, Filter)
OPT(view_cairoFilter, int, 3, View, SecondaryFilter) /* default: nearest-neighbour */
OPT(view_menu, bool, true, View, ShowMenu)
OPT(view_toolbar, bool, true, View, ShowToolbar)
OPT(view_statusbar, bool, true, View, ShowStatusbar)

/* Window */
OPT(window_scale, int, 0, Window, Scale2x)
/*OPT(window_x, int, -1, Window, X)*/
/*OPT(window_y, int, -1, Window, Y)*/
/*OPT(window_w, int, 0, Window, Width)*/
/*OPT(window_h, int, 0, Window, Height)*/
/*OPT(window_maximized, bool, false, Window, Maximized)*/
OPT(window_fullscreen, bool, false, Window, Fullscreen)

/* HUD display */
OPT(hud_fps, bool, false, HudDisplay, Fps)
OPT(hud_frameCounter, bool, false, HudDisplay, FrameCounter)
OPT(hud_lagCounter, bool, false, HudDisplay, LagCounter)
OPT(hud_input, bool, false, HudDisplay, Input)
OPT(hud_graphicalInput, bool, false, HudDisplay, GraphicalInput)
OPT(hud_rtc, bool, false, HudDisplay, RTC)
OPT(hud_mic, bool, false, HudDisplay, Mic)
OPT(hud_layout, std::vector<int>, std::vector<int>(), HudDisplay, Layout)

/* Config */
OPT(fpslimiter, bool, true, Config, FpsLimiter)
OPT(autoframeskip, bool, true, Config, AudoFrameskip)
OPT(frameskip, int, 0, Config, Frameskip)
OPT(use_jit,bool,false,Config,JIT_Enabled)
OPT(jit_max_block_size,int,100,Config,JITBlockSize)
OPT(core3D, int, 1, Config, Core3D)
OPT(presentationWGPU, bool, true, Config, PresentationWGPU)
OPT(presentationWGPUBackend, int, 0, Config, PresentationWGPUBackend)
/* Record the OSD/Lua agg.* layer with WGPUOSDDrawTarget and sample it as a
 * second texture instead of compositing it into the CPU frame.  Only used
 * while WGPU presentation is active; set to false to get the old AGG -> CPU
 * composite path back byte for byte. */
OPT(presentationWGPUHud, bool, true, Config, PresentationWGPUHud)
OPT(gpuFilter, int, 0, Config, GPUFilter)
OPT(textureDeposterize, bool, false, Config, 3DTextureDeposterization)
OPT(textureSmoothing, bool, false, Config, 3DTextureSmoothing)
OPT(textureUpscale, int, 1, Config, 3DTextureUpscaling)
OPT(gpuScaleFactor, float, 1.0, Config, GPUScaleFactor)
OPT(highColorInterpolation, bool, true, Config, HighResolutionColorInterpolation)
OPT(multisampling, bool, false, Config, OpenGLMultisampling)
OPT(multisamplingSize, int, 0, Config, OpenGLMultisamplingSize)
OPT(firmware_language, int, 1, Config, FirmwareLanguage)
OPT(savetype, int, 0, Config, SaveType)
/* Keep Up+Down / Left+Right usable at the same time instead of letting the
 * most recently pressed direction win.  The Windows port exposes the same
 * switch in its control config dialog. */
OPT(allow_up_down, bool, false, Config, AllowUpAndDown)

/* 3D renderer options, stored with the same section and key names the Windows
 * port writes into its [3D] section.  The defaults mirror CommonSettings, so a
 * config file written before these options existed keeps the core defaults
 * instead of overriding them. */
OPT(gfx3dEdgeMark, bool, true, 3D, EnableEdgeMark)
OPT(gfx3dFog, bool, true, 3D, EnableFog)
OPT(gfx3dTexture, bool, true, 3D, EnableTexture)
OPT(gfx3dLineHack, bool, true, 3D, EnableLineHack)
OPT(gfx3dTxtHack, bool, false, 3D, EnableTXTHack)
OPT(gfx3dShadowPolygon, bool, true, 3D, EnableShadowPolygon)
OPT(gfx3dZeroAlphaBlending, bool, true, 3D, EnableSpecialZeroAlphaBlending)
OPT(gfx3dNdsDepthCalculation, bool, true, 3D, EnableNDSDepthCalculation)
OPT(gfx3dDepthLEqualFacing, bool, false, 3D, EnableDepthLEqualPolygonFacing)
/* Output color depth of the 3D renderer: 15 = BGR555, 18 = BGR666 (packed as
 * 32-bit), 24 = BGR888.  18 is the Windows port's default as well. */
OPT(gfx3dColorDepth, int, 18, 3D, GpuBpp)

/* Emulation
 *
 * The Windows port's Emulation Settings dialog keeps these in [Emulation],
 * [BIOS] and [Firmware]; the same names are used here so the values mean the
 * same thing.  Defaults mirror CommonSettings. */
OPT(emu_advanced_timing, bool, true, Emulation, AdvancedTiming)
OPT(emu_game_hacks, bool, true, Emulation, GameHacks)
OPT(emu_debug_console, bool, false, Emulation, DebugConsole)
OPT(emu_ensata, bool, false, Emulation, EnsataEmulation)
OPT(bios_use_external, bool, false, BIOS, UseExtBIOS)
OPT(bios_arm9_path, std::string, "biosnds9.bin", BIOS, ARM9BIOSFile)
OPT(bios_arm7_path, std::string, "biosnds7.bin", BIOS, ARM7BIOSFile)
OPT(bios_swi_from_bios, bool, false, BIOS, SWIFromBIOS)
OPT(bios_patch_swi3, bool, false, BIOS, PatchSWI3)
OPT(fw_use_external, bool, false, Firmware, UseExtFirmware)
OPT(fw_image_path, std::string, "firmware.bin", Firmware, FirmwareFile)
OPT(fw_boot_from, bool, false, Firmware, BootFromFirmware)
OPT(fw_use_ext_settings, bool, false, Firmware, UseExtFirmwareSettings)

/* Firmware
 *
 * Settings written into the internal firmware image. The section and key
 * names match the Windows desmume.ini [Firmware] section. Defaults are the
 * same as NDS_GetDefaultFirmwareConfig(), so applying the saved values is a
 * no-op until the user edits them. "Language" is not stored here: the
 * existing Config/FirmwareLanguage option is the single source of truth, and
 * the --lang command line option still overrides it at startup.
 */
OPT(fwconfig_nickname, std::string, "DeSmuME", Firmware, nickName)
OPT(fwconfig_message, std::string, "DeSmuME makes you happy!", Firmware, Message)
OPT(fwconfig_favorite_color, int, 7, Firmware, favColor)
OPT(fwconfig_birth_month, int, 6, Firmware, bMonth)
OPT(fwconfig_birth_day, int, 23, Firmware, bDay)
OPT(fwconfig_mac_address, std::string, "0009BF123456", Firmware, macAddress)

/* Wifi settings use the same section and keys as the Windows frontend. */
OPT(wifi_enabled, bool, false, Wifi, Enabled)
OPT(wifi_compatibility_mode, bool, false, Wifi, Compatibility Mode)
OPT(wifi_bridge_adapter, int, 0, Wifi, BridgeAdapter)

/* RAM Watch
 *
 * The watch list itself is a document handled by shared/ramwatch_core.cpp;
 * only the preference values live here, because persistence is a host job
 * (INI on Windows, NSUserDefaults on macOS, this key file on GTK). Defaults
 * and -1 coordinates mean "let the window manager place the window".
 */
OPT(ramwatch_autoload, bool, false, RAMWatch, AutoLoad)
OPT(ramwatch_save_window_pos, bool, false, RAMWatch, SaveWindowPosition)
OPT(ramwatch_recent_files, std::vector<std::string>, std::vector<std::string>(), RAMWatch, RecentFiles)
OPT(ramwatch_win_x, int, -1, RAMWatch, WindowX)
OPT(ramwatch_win_y, int, -1, RAMWatch, WindowY)
OPT(ramwatch_win_w, int, 640, RAMWatch, WindowWidth)
OPT(ramwatch_win_h, int, 460, RAMWatch, WindowHeight)

/* Cheats
 *
 * The cheat list itself is a document handled by src/cheatSystem.cpp; only the
 * preference values live here. "Disabled" keeps the list intact while stopping
 * the core from applying it, and "DatabasePath" remembers the R4 database the
 * Cheat Database button reads from (empty means "<config dir>/desmume/cheat.dat").
 */
OPT(cheats_disabled, bool, false, Cheats, Disabled)
OPT(cheats_database_path, std::string, std::string(), Cheats, DatabasePath)

/* Audio */
OPT(audio_enabled, bool, true, Audio, Enabled)
OPT(audio_sync, int, 0, Audio, Synchronization)
OPT(audio_interpolation, int, 1, Audio, Interpolation)
OPT(audio_volume, int, 128, Audio, Volume)
/* Sound core id from SNDCORE_*: 0 = None/dummy, 2 = SDL (the only real core
 * this frontend links).  desmume_init() always starts with SDL, so a different
 * core is selected right after it. */
OPT(audio_core, int, 2, Audio, SoundCore)

/* Recording */
OPT(record_video_encoder, std::string, "libx264", Recording, VideoEncoder)
OPT(record_audio_encoder, std::string, "aac", Recording, AudioEncoder)
OPT(record_container, std::string, "mp4", Recording, Container)
OPT(record_video_bitrate, int, 8000, Recording, VideoBitrate)
OPT(record_audio_bitrate, int, 192, Recording, AudioBitrate)
OPT(record_filtered, bool, true, Recording, FilteredOutput)
OPT(record_layout, int, 0, Recording, ScreenLayout)
OPT(record_swap, bool, false, Recording, SwapScreens)
OPT(record_gap, int, 0, Recording, ScreenGap)
OPT(record_rotation, int, 0, Recording, Rotation)

/* Movie (Play/Record dialogs). The header metadata and all decisions live in
 * shared/movie_info_core.cpp; only host preferences are kept here. */
OPT(movie_directory, std::string, std::string(), Movie, Directory)
OPT(movie_read_only, bool, true, Movie, ReadOnly)
OPT(movie_pause_enabled, bool, false, Movie, PauseEnabled)
OPT(movie_author, std::string, std::string(), Movie, Author)

