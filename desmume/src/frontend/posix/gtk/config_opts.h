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

/* Config */
OPT(fpslimiter, bool, true, Config, FpsLimiter)
OPT(autoframeskip, bool, true, Config, AudoFrameskip)
OPT(frameskip, int, 0, Config, Frameskip)
OPT(use_jit,bool,false,Config,JIT_Enabled)
OPT(jit_max_block_size,int,100,Config,JITBlockSize)
OPT(core3D, int, 1, Config, Core3D)
OPT(textureDeposterize, bool, false, Config, 3DTextureDeposterization)
OPT(textureSmoothing, bool, false, Config, 3DTextureSmoothing)
OPT(textureUpscale, int, 1, Config, 3DTextureUpscaling)
OPT(highColorInterpolation, bool, true, Config, HighResolutionColorInterpolation)
OPT(multisampling, bool, false, Config, OpenGLMultisampling)
OPT(multisamplingSize, int, 0, Config, OpenGLMultisamplingSize)
OPT(command_line_overriding_firmware_language, bool, false, Config, CommandLineOverridingFirmwareLanguage)
OPT(firmware_language, int, 1, Config, FirmwareLanguage)
OPT(savetype, int, 0, Config, SaveType)

/* Audio */
OPT(audio_enabled, bool, true, Audio, Enabled)
OPT(audio_sync, int, 0, Audio, Synchronization)
OPT(audio_interpolation, int, 1, Audio, Interpolation)
OPT(audio_volume, int, 128, Audio, Volume)

