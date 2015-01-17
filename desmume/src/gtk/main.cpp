/* main.cpp - this file is part of DeSmuME
 *
 * Copyright (C) 2006-2015 DeSmuME Team
 * Copyright (C) 2007 Pascal Giard (evilynux)
 *
 * This file is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * This file is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; see the file COPYING.  If not, write to
 * the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#ifndef GTK_UI
#define GTK_UI
#endif

#include "version.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>
#include <SDL.h>

#include "types.h"
#include "firmware.h"
#include "NDSSystem.h"
#include "driver.h"
#include "GPU.h"
#include "SPU.h"
#include "sndsdl.h"
#include "ctrlssdl.h"
#include "MMU.h"
#include "render3D.h"
#include "desmume.h"
#include "debug.h"
#include "rasterize.h"
#include "saves.h"
#include "mic.h"
#include "movie.h"
#include "dTool.h"
#include "desmume_config.h"
#include "cheatsGTK.h"
#include "GPU_osd.h"

#include "avout_x264.h"
#include "avout_flac.h"

#include "commandline.h"

#include "slot2.h"

#include "utils/xstring.h"

#include "filter/videofilter.h"

#ifdef GDB_STUB
#include "armcpu.h"
#include "gdbstub.h"
#endif

#if defined(HAVE_LIBOSMESA) || defined(HAVE_GL_GLX)
#include <GL/gl.h>
#include <GL/glu.h>
#include "OGLRender.h"
#include "OGLRender_3_2.h"
#include "osmesa_3Demu.h"
#include "glx_3Demu.h"
#endif

#include "config.h"

#include "DeSmuME.xpm"

#undef GPOINTER_TO_INT
#define GPOINTER_TO_INT(p) ((gint)  (glong) (p))

#undef GPOINTER_TO_UINT
#define GPOINTER_TO_UINT(p) ((guint)  (glong) (p))

#define EMULOOP_PRIO (G_PRIORITY_HIGH_IDLE + 20 + 1)

#define GAP_SIZE 64

static int draw_count;
extern int _scanline_filter_a, _scanline_filter_b, _scanline_filter_c, _scanline_filter_d;
VideoFilter* video;

desmume::config::Config config;

enum {
    MAIN_BG_0 = 0,
    MAIN_BG_1,
    MAIN_BG_2,
    MAIN_BG_3,
    MAIN_OBJ,
    SUB_BG_0,
    SUB_BG_1,
    SUB_BG_2,
    SUB_BG_3,
    SUB_OBJ
};

gboolean EmuLoop(gpointer data);

static AVOutX264 avout_x264;
static AVOutFlac avout_flac;
static void RecordAV_x264();
static void RecordAV_flac();
static void RecordAV_stop();
static void RedrawScreen();

static void DoQuit();
static void RecordMovieDialog();
static void PlayMovieDialog();
static void StopMovie();
static void OpenNdsDialog();
static void SaveStateDialog();
static void LoadStateDialog();
void Launch();
void Pause();
static void ResetSaveStateTimes();
static void LoadSaveStateInfo();
static void Printscreen();
static void Reset();
static void Edit_Controls();
static void Edit_Joystick_Controls();
static void MenuSave(GtkMenuItem *item, gpointer slot);
static void MenuLoad(GtkMenuItem *item, gpointer slot);
static void About();
static void ToggleMenuVisible(GtkToggleAction *action);
static void ToggleStatusbarVisible(GtkToggleAction *action);
static void ToggleToolbarVisible(GtkToggleAction *action);
static void ToggleFullscreen (GtkToggleAction *action);
static void ToggleAudio (GtkToggleAction *action);
#ifdef FAKE_MIC
static void ToggleMicNoise (GtkToggleAction *action);
#endif
static void ToggleFpsLimiter (GtkToggleAction *action);
static void ToggleAutoFrameskip (GtkToggleAction *action);
static void ToggleSwapScreens(GtkToggleAction *action);
static void ToggleGap (GtkToggleAction *action);
static void SetRotation(GtkAction *action, GtkRadioAction *current);
static void SetWinSize(GtkAction *action, GtkRadioAction *current);
static void SetOrientation(GtkAction *action, GtkRadioAction *current);
static void ToggleLayerVisibility(GtkToggleAction* action, gpointer data);
static void ToggleHudDisplay(GtkToggleAction* action, gpointer data);
#ifdef DESMUME_GTK_FIRMWARE_BROKEN
static void SelectFirmwareFile();
#endif

static const char *ui_description =
"<ui>"
"  <menubar name='MainMenu'>"
"    <menu action='FileMenu'>"
"      <menuitem action='open'/>"
"      <menu action='RecentMenu'/>"
"      <separator/>"
"      <menuitem action='run'/>"
"      <menuitem action='pause'/>"
"      <menuitem action='reset'/>"
"      <separator/>"
"      <menuitem action='savestateto'/>"
"      <menuitem action='loadstatefrom'/>"
"      <menu action='SavestateMenu'>"
"        <menuitem action='savestate1'/>"
"        <menuitem action='savestate2'/>"
"        <menuitem action='savestate3'/>"
"        <menuitem action='savestate4'/>"
"        <menuitem action='savestate5'/>"
"        <menuitem action='savestate6'/>"
"        <menuitem action='savestate7'/>"
"        <menuitem action='savestate8'/>"
"        <menuitem action='savestate9'/>"
"        <menuitem action='savestate10'/>"
"      </menu>"
"      <menu action='LoadstateMenu'>"
"        <menuitem action='loadstate1'/>"
"        <menuitem action='loadstate2'/>"
"        <menuitem action='loadstate3'/>"
"        <menuitem action='loadstate4'/>"
"        <menuitem action='loadstate5'/>"
"        <menuitem action='loadstate6'/>"
"        <menuitem action='loadstate7'/>"
"        <menuitem action='loadstate8'/>"
"        <menuitem action='loadstate9'/>"
"        <menuitem action='loadstate10'/>"
"      </menu>"
#ifdef GTK_DESMUME_FIRMWARE_BROKEN
"      <menuitem action='loadfirmware'/>"
#endif
"      <separator/>"
"      <menuitem action='recordmovie'/>"
"      <menuitem action='playmovie'/>"
"      <menuitem action='stopmovie'/>"
"      <separator/>"
"      <menu action='RecordAVMenu'>"
"        <menuitem action='record_x264'/>"
"        <menuitem action='record_flac'/>"
"        <menuitem action='record_stop'/>"
"      </menu>"
"      <menuitem action='printscreen'/>"
"      <separator/>"
"      <menuitem action='quit'/>"
"    </menu>"
"    <menu action='ViewMenu'>"
"      <menu action='OrientationMenu'>"
"        <menuitem action='orient_vertical'/>"
"        <menuitem action='orient_horizontal'/>"
"        <menuitem action='orient_single'/>"
"        <separator/>"
"        <menuitem action='orient_swapscreens'/>"
"      </menu>"
"      <menu action='RotationMenu'>"
"        <menuitem action='rotate_0'/>"
"        <menuitem action='rotate_90'/>"
"        <menuitem action='rotate_180'/>"
"        <menuitem action='rotate_270'/>"
"      </menu>"
"      <menu action='WinsizeMenu'>"
"        <menuitem action='winsize_half'/>"
"        <menuitem action='winsize_1'/>"
"        <menuitem action='winsize_1half'/>"
"        <menuitem action='winsize_2'/>"
"        <menuitem action='winsize_2half'/>"
"        <menuitem action='winsize_3'/>"
"        <menuitem action='winsize_4'/>"
"        <menuitem action='winsize_5'/>"
"        <menuitem action='winsize_scale'/>"
"        <separator/>"
"        <menuitem action='fullscreen'/>"
"      </menu>"
"      <menuitem action='gap'/>"
"      <menu action='PriInterpolationMenu'>"
"        <menuitem action='pri_interp_none'/>"
"        <menuitem action='pri_interp_lq2x'/>"
"        <menuitem action='pri_interp_lq2xs'/>"
"        <menuitem action='pri_interp_hq2x'/>"
"        <menuitem action='pri_interp_hq2xs'/>"
"        <menuitem action='pri_interp_hq4x'/>"
"        <menuitem action='pri_interp_hq4xs'/>"
"        <menuitem action='pri_interp_2xsai'/>"
"        <menuitem action='pri_interp_super2xsai'/>"
"        <menuitem action='pri_interp_supereagle'/>"
"        <menuitem action='pri_interp_scanline'/>"
"        <menuitem action='pri_interp_bilinear'/>"
"        <menuitem action='pri_interp_nearest2x'/>"
"        <menuitem action='pri_interp_nearest_1point5x'/>"
"        <menuitem action='pri_interp_nearestplus_1point5x'/>"
"        <menuitem action='pri_interp_epx'/>"
"        <menuitem action='pri_interp_epxplus'/>"
"        <menuitem action='pri_interp_epx_1point5x'/>"
"        <menuitem action='pri_interp_epxplus_1point5x'/>"
"        <menuitem action='pri_interp_2xbrz'/>"
"        <menuitem action='pri_interp_3xbrz'/>"
"        <menuitem action='pri_interp_4xbrz'/>"
"        <menuitem action='pri_interp_5xbrz'/>"
"      </menu>"
"      <menu action='InterpolationMenu'>"
"        <menuitem action='interp_fast'/>"
"        <menuitem action='interp_nearest'/>"
"        <menuitem action='interp_good'/>"
"        <menuitem action='interp_bilinear'/>"
"        <menuitem action='interp_best'/>"
"      </menu>"
"      <menu action='HudMenu'>"
#ifdef HAVE_LIBAGG
"        <menuitem action='hud_fps'/>"
"        <menuitem action='hud_input'/>"
"        <menuitem action='hud_graphicalinput'/>"
"        <menuitem action='hud_framecounter'/>"
"        <menuitem action='hud_lagcounter'/>"
"        <menuitem action='hud_rtc'/>"
"        <menuitem action='hud_mic'/>"
"        <separator/>"
"        <menuitem action='hud_editor'/>"
#else
"        <menuitem action='hud_notsupported'/>"
#endif
"      </menu>"
"      <separator/>"
"      <menuitem action='view_menu'/>"
"      <menuitem action='view_toolbar'/>"
"      <menuitem action='view_statusbar'/>"
"    </menu>"
"    <menu action='ConfigMenu'>"
"      <menuitem action='enableaudio'/>"
#ifdef FAKE_MIC
"      <menuitem action='micnoise'/>"
#endif
"      <menu action='SPUModeMenu'>"
"        <menuitem action='SPUModeDualASync'/>"
"        <menuitem action='SPUModeSyncN'/>"
"        <menuitem action='SPUModeSyncZ'/>"
#ifdef HAVE_LIBSOUNDTOUCH
"        <menuitem action='SPUModeSyncP'/>"
#endif
"      </menu>"
"      <menu action='SPUInterpolationMenu'>"
"        <menuitem action='SPUInterpolationNone'/>"
"        <menuitem action='SPUInterpolationLinear'/>"
"        <menuitem action='SPUInterpolationCosine'/>"
"      </menu>"
"      <menu action='FrameskipMenu'>"
"        <menuitem action='enablefpslimiter'/>"
"        <separator/>"
"        <menuitem action='frameskipA'/>"
"        <separator/>"
"        <menuitem action='frameskip0'/>"
"        <menuitem action='frameskip1'/>"
"        <menuitem action='frameskip2'/>"
"        <menuitem action='frameskip3'/>"
"        <menuitem action='frameskip4'/>"
"        <menuitem action='frameskip5'/>"
"        <menuitem action='frameskip6'/>"
"        <menuitem action='frameskip7'/>"
"        <menuitem action='frameskip8'/>"
"        <menuitem action='frameskip9'/>"
"      </menu>"
"      <menu action='CheatMenu'>"
"        <menuitem action='cheatsearch'/>"
"        <menuitem action='cheatlist'/>"
"      </menu>"
"      <menu action='ConfigSaveMenu'>"
"        <menuitem action='save_t0'/>"
"        <menuitem action='save_t1'/>"
"        <menuitem action='save_t2'/>"
"        <menuitem action='save_t3'/>"
"        <menuitem action='save_t4'/>"
"        <menuitem action='save_t5'/>"
"        <menuitem action='save_t6'/>"
"      </menu>"
"      <menuitem action='editctrls'/>"
"      <menuitem action='editjoyctrls'/>"
"    </menu>"
"    <menu action='ToolsMenu'>"
"      <menuitem action='ioregs'/>"
"      <separator/>"
"      <menu action='LayersMenu'>"
"        <menuitem action='layermainbg0'/>"
"        <menuitem action='layermainbg1'/>"
"        <menuitem action='layermainbg2'/>"
"        <menuitem action='layermainbg3'/>"
"        <menuitem action='layermainobj'/>"
"        <menuitem action='layersubbg0'/>"
"        <menuitem action='layersubbg1'/>"
"        <menuitem action='layersubbg2'/>"
"        <menuitem action='layersubbg3'/>"
"        <menuitem action='layersubobj'/>"
"      </menu>"
"    </menu>"
"    <menu action='HelpMenu'>"
"      <menuitem action='about'/>"
"    </menu>"
"  </menubar>"
"  <toolbar name='ToolBar'>"
"    <toolitem action='open'/>"
"    <toolitem action='run'/>"
"    <toolitem action='pause'/>"
"    <toolitem action='quit'/>"
"  </toolbar>"
"</ui>";
  
static const GtkActionEntry action_entries[] = {
    { "FileMenu", NULL, "_File" },
      { "open",          "gtk-open",    "_Open",         "<Ctrl>o",  NULL,   OpenNdsDialog },
      { "RecentMenu", NULL, "Open _recent" },
      { "run",        "gtk-media-play",   "_Run",          "Pause",  NULL,   Launch },
      { "pause",      "gtk-media-pause",  "_Pause",        "Pause",  NULL,   Pause },
      { "reset",      "gtk-refresh",      "Re_set",        NULL,       NULL,   Reset },
      { "savestateto",    NULL,         "Save state _to ...",         NULL,  NULL,   SaveStateDialog },
      { "loadstatefrom",  NULL,         "Load state _from ...",         NULL,  NULL,   LoadStateDialog },
      { "recordmovie",  NULL,         "Record movie _to ...",         NULL,  NULL,   RecordMovieDialog },
      { "playmovie",  NULL,         "Play movie _from ...",         NULL,  NULL,   PlayMovieDialog },
      { "stopmovie",  NULL,         "Stop movie", NULL,  NULL,   StopMovie },
      { "RecordAVMenu", NULL, "Record _video/audio" },
        { "record_x264", "gtk-media-record", "Record lossless H._264 (video only)...", NULL, NULL, RecordAV_x264 },
        { "record_flac", "gtk-media-record", "Record _flac (audio only)...", NULL, NULL, RecordAV_flac },
        { "record_stop", "gtk-media-stop", "_Stop recording", NULL, NULL, RecordAV_stop },
      { "SavestateMenu", NULL, "_Save state" },
      { "LoadstateMenu", NULL, "_Load state" },
#ifdef DESMUME_GTK_FIRMWARE_BROKEN
      { "loadfirmware","gtk-open",        "Load Firm_ware file", "<Ctrl>l",  NULL, SelectFirmwareFile },
#endif
      { "printscreen","gtk-media-record", "Take a _screenshot",    "<Ctrl>s",  NULL,   Printscreen },
      { "quit",       "gtk-quit",         "_Quit",         "<Ctrl>q",  NULL,   DoQuit },

    { "ViewMenu", NULL, "_View" },
      { "RotationMenu", NULL, "_Rotation" },
      { "OrientationMenu", NULL, "LCDs _Layout" },
      { "WinsizeMenu", NULL, "_Window Size" },
      { "PriInterpolationMenu", NULL, "Video _Filter" },
      { "InterpolationMenu", NULL, "S_econdary Video Filter" },
      { "HudMenu", NULL, "_HUD" },
#ifndef HAVE_LIBAGG
      { "hud_notsupported", NULL, "HUD support not compiled" },
#endif

    { "ConfigMenu", NULL, "_Config" },
      { "SPUModeMenu", NULL, "Audio _Synchronization" },
      { "SPUInterpolationMenu", NULL, "Audio _Interpolation" },
      { "FrameskipMenu", NULL, "_Frameskip" },
      { "CheatMenu", NULL, "_Cheat" },
        { "cheatsearch",     NULL,      "_Search",        NULL,       NULL,   CheatSearch },
        { "cheatlist",       NULL,      "_List",        NULL,       NULL,   CheatList },
      { "ConfigSaveMenu", NULL, "_Saves" },
      { "editctrls",  NULL,        "_Edit controls",NULL,    NULL,   Edit_Controls },
      { "editjoyctrls",  NULL,     "Edit _Joystick controls",NULL,       NULL,   Edit_Joystick_Controls },

    { "ToolsMenu", NULL, "_Tools" },
      { "LayersMenu", NULL, "View _Layers" },

    { "HelpMenu", NULL, "_Help" },
      { "about",      "gtk-about",        "_About",        NULL,       NULL,   About }
};

static const GtkToggleActionEntry toggle_entries[] = {
    { "enableaudio", NULL, "_Enable audio", NULL, NULL, G_CALLBACK(ToggleAudio), TRUE},
#ifdef FAKE_MIC
    { "micnoise", NULL, "Fake mic _noise", NULL, NULL, G_CALLBACK(ToggleMicNoise), FALSE},
#endif
    { "enablefpslimiter", NULL, "_Limit to 60 fps", NULL, NULL, G_CALLBACK(ToggleFpsLimiter), TRUE},
    { "frameskipA", NULL, "_Auto-minimize skipping", NULL, NULL, G_CALLBACK(ToggleAutoFrameskip), TRUE},
    { "gap", NULL, "Screen _Gap", NULL, NULL, G_CALLBACK(ToggleGap), FALSE},
    { "view_menu", NULL, "Show _menu", NULL, NULL, G_CALLBACK(ToggleMenuVisible), TRUE},
    { "view_toolbar", NULL, "Show _toolbar", NULL, NULL, G_CALLBACK(ToggleToolbarVisible), TRUE},
    { "view_statusbar", NULL, "Show _statusbar", NULL, NULL, G_CALLBACK(ToggleStatusbarVisible), TRUE},
    { "orient_swapscreens", NULL, "S_wap screens", "space", NULL, G_CALLBACK(ToggleSwapScreens), FALSE},
    { "fullscreen", NULL, "_Fullscreen", "F11", NULL, G_CALLBACK(ToggleFullscreen), FALSE},
};

static const GtkRadioActionEntry pri_interpolation_entries[] = {
    { "pri_interp_none", NULL, VideoFilterAttributesList[VideoFilterTypeID_None].typeString, NULL, NULL, VideoFilterTypeID_None},
    { "pri_interp_lq2x", NULL, VideoFilterAttributesList[VideoFilterTypeID_LQ2X].typeString, NULL, NULL, VideoFilterTypeID_LQ2X},
    { "pri_interp_lq2xs", NULL, VideoFilterAttributesList[VideoFilterTypeID_LQ2XS].typeString, NULL, NULL, VideoFilterTypeID_LQ2XS},
    { "pri_interp_hq2x", NULL, VideoFilterAttributesList[VideoFilterTypeID_HQ2X].typeString, NULL, NULL, VideoFilterTypeID_HQ2X},
    { "pri_interp_hq2xs", NULL, VideoFilterAttributesList[VideoFilterTypeID_HQ2XS].typeString, NULL, NULL, VideoFilterTypeID_HQ2XS},
    { "pri_interp_hq4x", NULL, VideoFilterAttributesList[VideoFilterTypeID_HQ4X].typeString, NULL, NULL, VideoFilterTypeID_HQ4X},
    { "pri_interp_hq4xs", NULL, VideoFilterAttributesList[VideoFilterTypeID_HQ4XS].typeString, NULL, NULL, VideoFilterTypeID_HQ4XS},
    { "pri_interp_2xsai", NULL, VideoFilterAttributesList[VideoFilterTypeID_2xSaI].typeString, NULL, NULL, VideoFilterTypeID_2xSaI},
    { "pri_interp_super2xsai", NULL, VideoFilterAttributesList[VideoFilterTypeID_Super2xSaI].typeString, NULL, NULL, VideoFilterTypeID_Super2xSaI},
    { "pri_interp_supereagle", NULL, VideoFilterAttributesList[VideoFilterTypeID_SuperEagle].typeString, NULL, NULL, VideoFilterTypeID_SuperEagle},
    { "pri_interp_scanline", NULL, VideoFilterAttributesList[VideoFilterTypeID_Scanline].typeString, NULL, NULL, VideoFilterTypeID_Scanline},
    { "pri_interp_bilinear", NULL, VideoFilterAttributesList[VideoFilterTypeID_Bilinear].typeString, NULL, NULL, VideoFilterTypeID_Bilinear},
    { "pri_interp_nearest2x", NULL, VideoFilterAttributesList[VideoFilterTypeID_Nearest2X].typeString, NULL, NULL, VideoFilterTypeID_Nearest2X},
    { "pri_interp_nearest_1point5x", NULL, VideoFilterAttributesList[VideoFilterTypeID_Nearest1_5X].typeString, NULL, NULL, VideoFilterTypeID_Nearest1_5X},
    { "pri_interp_nearestplus_1point5x", NULL, VideoFilterAttributesList[VideoFilterTypeID_NearestPlus1_5X].typeString, NULL, NULL, VideoFilterTypeID_NearestPlus1_5X},
    { "pri_interp_epx", NULL, VideoFilterAttributesList[VideoFilterTypeID_EPX].typeString, NULL, NULL, VideoFilterTypeID_EPX},
    { "pri_interp_epxplus", NULL, VideoFilterAttributesList[VideoFilterTypeID_EPXPlus].typeString, NULL, NULL, VideoFilterTypeID_EPXPlus},
    { "pri_interp_epx_1point5x", NULL, VideoFilterAttributesList[VideoFilterTypeID_EPX1_5X].typeString, NULL, NULL, VideoFilterTypeID_EPX1_5X},
    { "pri_interp_epxplus_1point5x", NULL, VideoFilterAttributesList[VideoFilterTypeID_EPXPlus1_5X].typeString, NULL, NULL, VideoFilterTypeID_EPXPlus1_5X},
    { "pri_interp_2xbrz", NULL, VideoFilterAttributesList[VideoFilterTypeID_2xBRZ].typeString, NULL, NULL, VideoFilterTypeID_2xBRZ},
    { "pri_interp_3xbrz", NULL, VideoFilterAttributesList[VideoFilterTypeID_3xBRZ].typeString, NULL, NULL, VideoFilterTypeID_3xBRZ},
    { "pri_interp_4xbrz", NULL, VideoFilterAttributesList[VideoFilterTypeID_4xBRZ].typeString, NULL, NULL, VideoFilterTypeID_4xBRZ},
    { "pri_interp_5xbrz", NULL, VideoFilterAttributesList[VideoFilterTypeID_5xBRZ].typeString, NULL, NULL, VideoFilterTypeID_5xBRZ},
};

static const GtkRadioActionEntry interpolation_entries[] = {
    { "interp_fast", NULL, "_Fast", NULL, NULL, CAIRO_FILTER_FAST },
    { "interp_nearest", NULL, "_Nearest-neighbor", NULL, NULL, CAIRO_FILTER_NEAREST },
    { "interp_good", NULL, "_Good", NULL, NULL, CAIRO_FILTER_GOOD },
    { "interp_bilinear", NULL, "_Bilinear", NULL, NULL, CAIRO_FILTER_BILINEAR },
    { "interp_best", NULL, "B_est", NULL, NULL, CAIRO_FILTER_BEST },
};

static const GtkRadioActionEntry rotation_entries[] = {
    { "rotate_0",   "gtk-orientation-portrait",          "_0",  NULL, NULL, 0 },
    { "rotate_90",  "gtk-orientation-landscape",         "_90", NULL, NULL, 90 },
    { "rotate_180", "gtk-orientation-reverse-portrait",  "_180",NULL, NULL, 180 },
    { "rotate_270", "gtk-orientation-reverse-landscape", "_270",NULL, NULL, 270 },
};

enum winsize_enum {
	WINSIZE_SCALE = 0,
	WINSIZE_HALF = 1,
	WINSIZE_1 = 2,
	WINSIZE_1HALF = 3,
	WINSIZE_2 = 4,
	WINSIZE_2HALF = 5,
	WINSIZE_3 = 6,
	WINSIZE_4 = 8,
	WINSIZE_5 = 10,
};

static winsize_enum winsize_current;

static const GtkRadioActionEntry winsize_entries[] = {
	{ "winsize_half", NULL, "0_.5x", NULL, NULL, WINSIZE_HALF },
	{ "winsize_1", NULL, "_1x", NULL, NULL, WINSIZE_1 },
	{ "winsize_1half", NULL, "1.5x", NULL, NULL, WINSIZE_1HALF },
	{ "winsize_2", NULL, "_2x", NULL, NULL, WINSIZE_2 },
	{ "winsize_2half", NULL, "2.5x", NULL, NULL, WINSIZE_2HALF },
	{ "winsize_3", NULL, "_3x", NULL, NULL, WINSIZE_3 },
	{ "winsize_4", NULL, "_4x", NULL, NULL, WINSIZE_4 },
	{ "winsize_5", NULL, "_5x", NULL, NULL, WINSIZE_5 },
	{ "winsize_scale", NULL, "_Scale to window", NULL, NULL, WINSIZE_SCALE },
};

/* When adding modes here remember to add the relevent entry to screen_size */
enum orientation_enum {
    ORIENT_VERTICAL = 0,
    ORIENT_HORIZONTAL = 1,
    ORIENT_SINGLE = 2,
    ORIENT_N
};

static const GtkRadioActionEntry orientation_entries[] = {
    { "orient_vertical",   NULL, "_Vertical",   "<Ctrl>1", NULL, ORIENT_VERTICAL },
    { "orient_horizontal", NULL, "_Horizontal", "<Ctrl>2", NULL, ORIENT_HORIZONTAL },
    { "orient_single",     NULL, "_Single screen", "<Ctrl>0", NULL, ORIENT_SINGLE },
};

struct screen_size_t {
   gint width;
   gint height;
};

const struct screen_size_t screen_size[ORIENT_N] = {
    {256, 384},
    {512, 192},
    {256, 192}
};

enum spumode_enum {
    SPUMODE_DUALASYNC = 0,
    SPUMODE_SYNCN = 1,
    SPUMODE_SYNCZ = 2,
    SPUMODE_SYNCP = 3
};

static const GtkRadioActionEntry spumode_entries[] = {
    { "SPUModeSyncN", NULL, "Synchronous (N)", NULL, NULL, SPUMODE_SYNCN},
    { "SPUModeSyncZ", NULL, "Synchronous (Z)", NULL, NULL, SPUMODE_SYNCZ},
#ifdef HAVE_LIBSOUNDTOUCH
    { "SPUModeSyncP", NULL, "Synchronous (P)", NULL, NULL, SPUMODE_SYNCP},
#endif
    { "SPUModeDualASync", NULL, "Dual Asynchronous", NULL, NULL, SPUMODE_DUALASYNC}
};

static const GtkRadioActionEntry spuinterpolation_entries[] = {
    { "SPUInterpolationNone", NULL, "_None", NULL, NULL, SPUInterpolation_None },
    { "SPUInterpolationLinear", NULL, "_Linear", NULL, NULL, SPUInterpolation_Linear },
    { "SPUInterpolationCosine", NULL, "_Cosine", NULL, NULL, SPUInterpolation_Cosine }
};

enum frameskip_enum {
    FRAMESKIP_0 = 0,
    FRAMESKIP_1 = 1,
    FRAMESKIP_2 = 2,
    FRAMESKIP_3 = 3,
    FRAMESKIP_4 = 4,
    FRAMESKIP_5 = 5,
    FRAMESKIP_6 = 6,
    FRAMESKIP_7 = 7,
    FRAMESKIP_8 = 8,
    FRAMESKIP_9 = 9,
};

static const GtkRadioActionEntry frameskip_entries[] = {
    { "frameskip0", NULL, "_0 (never skip)", NULL, NULL, FRAMESKIP_0},
    { "frameskip1", NULL, "_1", NULL, NULL, FRAMESKIP_1},
    { "frameskip2", NULL, "_2", NULL, NULL, FRAMESKIP_2},
    { "frameskip3", NULL, "_3", NULL, NULL, FRAMESKIP_3},
    { "frameskip4", NULL, "_4", NULL, NULL, FRAMESKIP_4},
    { "frameskip5", NULL, "_5", NULL, NULL, FRAMESKIP_5},
    { "frameskip6", NULL, "_6", NULL, NULL, FRAMESKIP_6},
    { "frameskip7", NULL, "_7", NULL, NULL, FRAMESKIP_7},
    { "frameskip8", NULL, "_8", NULL, NULL, FRAMESKIP_8},
    { "frameskip9", NULL, "_9", NULL, NULL, FRAMESKIP_9},
};

static const GtkRadioActionEntry savet_entries[] = {
    { "save_t0", NULL, "_0 Autodetect",     NULL, NULL, 0},
    { "save_t1", NULL, "_1 EEPROM 4kbit",   NULL, NULL, 1},
    { "save_t2", NULL, "_2 EEPROM 64kbit",  NULL, NULL, 2},
    { "save_t3", NULL, "_3 EEPROM 512kbit", NULL, NULL, 3},
    { "save_t4", NULL, "_4 FRAM 256kbit",   NULL, NULL, 4},
    { "save_t5", NULL, "_5 FLASH 2mbit",    NULL, NULL, 5},
    { "save_t6", NULL, "_6 FLASH 4mbit",    NULL, NULL, 6}
};

SoundInterface_struct *SNDCoreList[] = {
&SNDDummy,
&SNDSDL,
NULL
};

GPU3DInterface *core3DList[] = {
  &gpu3DNull,
  &gpu3DRasterize,
#if defined(HAVE_LIBOSMESA) || defined(HAVE_GL_GLX)
  &gpu3Dgl,
#endif
};

static const u16 gtk_kb_cfg[NB_KEYS] = {
    GDK_x,         // A
    GDK_z,         // B
    GDK_Shift_R,   // select
    GDK_Return,    // start
    GDK_Right,     // Right
    GDK_Left,      // Left
    GDK_Up,        // Up
    GDK_Down,      // Down       
    GDK_w,         // R
    GDK_q,         // L
    GDK_s,         // X
    GDK_a,         // Y
    GDK_p,         // DEBUG
    GDK_o,         // BOOST
    GDK_BackSpace, // Lid
};

GKeyFile *keyfile;

struct modify_key_ctx {
    gint mk_key_chosen;
    GtkWidget *label;
    u8 key_id;
};

static u16 keys_latch = 0;
static u16 gdk_shift_pressed = 0;
u16 Keypad_Temp[NB_KEYS];

class configured_features : public CommandLine
{
public:
  int engine_3d;
  int savetype;

  int firmware_language;

  int timeout;
};

static void
init_configured_features( class configured_features *config)
{
  config->engine_3d = 1;

  config->savetype = 0;

  config->timeout = 0;

  /* use the default language */
  config->firmware_language = -1;
}

static int
fill_configured_features( class configured_features *config,
                          int argc, char ** argv)
{
  GOptionEntry options[] = {
    { "3d-engine", 0, 0, G_OPTION_ARG_INT, &config->engine_3d, "Select 3d rendering engine. Available engines:\n"
        "\t\t\t\t  0 = 3d disabled\n"
        "\t\t\t\t  1 = internal rasterizer (default)\n"
#if defined(HAVE_LIBOSMESA) || defined(HAVE_GL_GLX)
        "\t\t\t\t  2 = opengl\n"
#endif
        ,"ENGINE"},
    { "save-type", 0, 0, G_OPTION_ARG_INT, &config->savetype, "Select savetype from the following:\n"
    "\t\t\t\t  0 = Autodetect (default)\n"
    "\t\t\t\t  1 = EEPROM 4kbit\n"
    "\t\t\t\t  2 = EEPROM 64kbit\n"
    "\t\t\t\t  3 = EEPROM 512kbit\n"
    "\t\t\t\t  4 = FRAM 256kbit\n"
    "\t\t\t\t  5 = FLASH 2mbit\n"
    "\t\t\t\t  6 = FLASH 4mbit\n",
    "SAVETYPE"},
    { "fwlang", 0, 0, G_OPTION_ARG_INT, &config->firmware_language, "Set the language in the firmware, LANG as follows:\n"
                                    "\t\t\t\t  0 = Japanese\n"
                                    "\t\t\t\t  1 = English\n"
                                    "\t\t\t\t  2 = French\n"
                                    "\t\t\t\t  3 = German\n"
                                    "\t\t\t\t  4 = Italian\n"
                                    "\t\t\t\t  5 = Spanish\n",
                                    "LANG"},
    { "timeout", 0, 0, G_OPTION_ARG_INT, &config->timeout, "Quit DeSmuME after the specified seconds for testing purpose.", "SECONDS"},
    { NULL }
  };

  config->loadCommonOptions();
  g_option_context_add_main_entries (config->ctx, options, "options");
  g_option_context_add_group (config->ctx, gtk_get_option_group (TRUE));
  config->parse(argc,argv);

  if(!config->validate())
    goto error;

  if (config->savetype < 0 || config->savetype > 6) {
    g_printerr("Accepted savetypes are from 0 to 6.\n");
    return false;
  }

  if (config->firmware_language < -1 || config->firmware_language > 5) {
    g_printerr("Firmware language must be set to a value from 0 to 5.\n");
    goto error;
  }

  if (config->engine_3d != 0 && config->engine_3d != 1
#if defined(HAVE_LIBOSMESA) || defined(HAVE_GL_GLX)
           && config->engine_3d != 2
#endif
          ) {
    g_printerr("Currently available ENGINES: 0, 1"
#if defined(HAVE_LIBOSMESA) || defined(HAVE_GL_GLX)
            ", 2"
#endif
            "\n");
    goto error;
  }

#ifdef GDB_STUB
  if (config->arm9_gdb_port != 0 && (config->arm9_gdb_port < 1 || config->arm9_gdb_port > 65535)) {
    g_printerr("ARM9 GDB stub port must be in the range 1 to 65535\n");
    goto error;
  }

  if (config->arm7_gdb_port != 0 && (config->arm7_gdb_port < 1 || config->arm7_gdb_port > 65535)) {
    g_printerr("ARM7 GDB stub port must be in the range 1 to 65535\n");
    goto error;
  }
#endif

  return 1;

error:
  config->errorHelp(argv[0]);

  return 0;
}


/*
 * The thread handling functions needed by the GDB stub code.
 */
#ifdef GDB_STUB
void *
createThread_gdb( void (*thread_function)( void *data),
                  void *thread_data)
{
  GThread *new_thread = g_thread_create( (GThreadFunc)thread_function,
                                         thread_data,
                                         TRUE,
                                         NULL);

  return new_thread;
}

void
joinThread_gdb( void *thread_handle) {
  g_thread_join( (GThread *)thread_handle);
}
#endif

/************************ GTK *******************************/

uint SPUMode = SPUMODE_DUALASYNC;
uint Frameskip = 0;
uint autoFrameskipMax = 0;
bool autoframeskip = true;
cairo_filter_t Interpolation = CAIRO_FILTER_NEAREST;

static GtkWidget *pWindow;
static GtkWidget *pStatusBar;
static GtkWidget *pDrawingArea;
static GtkActionGroup * action_group;
static GtkUIManager *ui_manager;

struct nds_screen_t {
    guint gap_size;
    gint rotation_angle;
    orientation_enum orientation;
    cairo_matrix_t touch_matrix;
    cairo_matrix_t topscreen_matrix;
    gboolean swap;
};

struct nds_screen_t nds_screen;

static BOOL regMainLoop = FALSE;

static inline void UpdateStatusBar (const char *message)
{
    gint pStatusBar_Ctx;

    pStatusBar_Ctx = gtk_statusbar_get_context_id(GTK_STATUSBAR(pStatusBar), "Global");
    gtk_statusbar_pop(GTK_STATUSBAR(pStatusBar), pStatusBar_Ctx);
    gtk_statusbar_push(GTK_STATUSBAR(pStatusBar), pStatusBar_Ctx, message);
}

static void About()
{
    GdkPixbuf * pixbuf = gdk_pixbuf_new_from_xpm_data(DeSmuME_xpm);

    static const gchar *authors[] = {
    	"yopyop (original author)",
    	"DeSmuME team",
    	NULL
    };

    gtk_show_about_dialog(GTK_WINDOW(pWindow),
            "program-name", "DeSmuME",
            "version", EMU_DESMUME_VERSION_STRING() + 1, // skip space
            "website", "http://desmume.org",
            "logo", pixbuf,
            "comments", "Nintendo DS emulator based on work by Yopyop",
            "authors", authors,
            NULL);

    g_object_unref(pixbuf);
}

static void ToggleMenuVisible(GtkToggleAction *action)
{
    GtkWidget *pMenuBar = gtk_ui_manager_get_widget (ui_manager, "/MainMenu");

    config.view_menu = gtk_toggle_action_get_active(action);
    if (config.view_menu)
      gtk_widget_show(pMenuBar);
    else
      gtk_widget_hide(pMenuBar);
}

static void ToggleToolbarVisible(GtkToggleAction *action)
{
    GtkWidget *pToolBar = gtk_ui_manager_get_widget (ui_manager, "/ToolBar");

    config.view_toolbar = gtk_toggle_action_get_active(action);
    if (config.view_toolbar)
      gtk_widget_show(pToolBar);
    else
      gtk_widget_hide(pToolBar);
}

static void ToggleStatusbarVisible(GtkToggleAction *action)
{
    config.view_statusbar = gtk_toggle_action_get_active(action);
    if (config.view_statusbar)
      gtk_widget_show(pStatusBar);
    else
      gtk_widget_hide(pStatusBar);
}

static void ToggleFullscreen(GtkToggleAction *action)
{
  GtkWidget *pMenuBar = gtk_ui_manager_get_widget (ui_manager, "/MainMenu");
  GtkWidget *pToolBar = gtk_ui_manager_get_widget (ui_manager, "/ToolBar");
  config.window_fullscreen = gtk_toggle_action_get_active(action);
  if (config.window_fullscreen)
  {
    gtk_widget_hide(pMenuBar);
    gtk_widget_hide(pToolBar);
    gtk_widget_hide(pStatusBar);
    gtk_action_set_sensitive(gtk_action_group_get_action(action_group, "view_menu"), FALSE);
    gtk_action_set_sensitive(gtk_action_group_get_action(action_group, "view_toolbar"), FALSE);
    gtk_action_set_sensitive(gtk_action_group_get_action(action_group, "view_statusbar"), FALSE);
    gtk_window_fullscreen(GTK_WINDOW(pWindow));
  }
  else
  {
    if (config.view_menu) {
      gtk_widget_show(pMenuBar);
    }
    if (config.view_toolbar) {
      gtk_widget_show(pToolBar);
    }
    if (config.view_statusbar) {
      gtk_widget_show(pStatusBar);
    }
    gtk_action_set_sensitive(gtk_action_group_get_action(action_group, "view_menu"), TRUE);
    gtk_action_set_sensitive(gtk_action_group_get_action(action_group, "view_toolbar"), TRUE);
    gtk_action_set_sensitive(gtk_action_group_get_action(action_group, "view_statusbar"), TRUE);
    gtk_window_unfullscreen(GTK_WINDOW(pWindow));
  }
}


static int Open(const char *filename)
{
    int res;
    ResetSaveStateTimes();
    res = NDS_LoadROM( filename );
    if(res > 0) {
        LoadSaveStateInfo();
        gtk_action_set_sensitive(gtk_action_group_get_action(action_group, "cheatlist"), TRUE);
    }
    return res;
}

void Launch()
{
    GtkWidget *pause;
    desmume_resume();

    if(!regMainLoop) {
        g_idle_add_full(EMULOOP_PRIO, &EmuLoop, pWindow, NULL);
        regMainLoop = TRUE;
    }

    UpdateStatusBar("Running ...");

    gtk_action_set_sensitive(gtk_action_group_get_action(action_group, "pause"), TRUE);
    gtk_action_set_sensitive(gtk_action_group_get_action(action_group, "run"), FALSE);
    gtk_action_set_sensitive(gtk_action_group_get_action(action_group, "reset"), TRUE);
    gtk_action_set_sensitive(gtk_action_group_get_action(action_group, "printscreen"), TRUE);

    pause = gtk_bin_get_child(GTK_BIN(gtk_ui_manager_get_widget(ui_manager, "/ToolBar/pause")));
    gtk_widget_grab_focus(pause);
}

void Pause()
{
    GtkWidget *run;
    desmume_pause();
    UpdateStatusBar("Paused");

    gtk_action_set_sensitive(gtk_action_group_get_action(action_group, "pause"), FALSE);
    gtk_action_set_sensitive(gtk_action_group_get_action(action_group, "run"), TRUE);

    run = gtk_bin_get_child(GTK_BIN(gtk_ui_manager_get_widget(ui_manager, "/ToolBar/run")));
    gtk_widget_grab_focus(run);
}

static void LoadStateDialog()
{
    GtkFileFilter *pFilter_ds, *pFilter_any;
    GtkWidget *pFileSelection;
    GtkWidget *pParent;
    gchar *sPath;

    if (desmume_running())
        Pause();

    pParent = GTK_WIDGET(pWindow);

    pFilter_ds = gtk_file_filter_new();
    gtk_file_filter_add_pattern(pFilter_ds, "*.ds*");
    gtk_file_filter_set_name(pFilter_ds, "DeSmuME binary (.ds*)");

    pFilter_any = gtk_file_filter_new();
    gtk_file_filter_add_pattern(pFilter_any, "*");
    gtk_file_filter_set_name(pFilter_any, "All files");

    /* Creating the selection window */
    pFileSelection = gtk_file_chooser_dialog_new("Load State From ...",
            GTK_WINDOW(pParent),
            GTK_FILE_CHOOSER_ACTION_OPEN,
            GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
            GTK_STOCK_OPEN, GTK_RESPONSE_OK,
            NULL);

    /* Only the dialog window is accepting events: */
    gtk_window_set_modal(GTK_WINDOW(pFileSelection), TRUE);

    gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(pFileSelection), pFilter_ds);
    gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(pFileSelection), pFilter_any);

    /* Showing the window */
    switch(gtk_dialog_run(GTK_DIALOG(pFileSelection))) {
    case GTK_RESPONSE_OK:
        sPath = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(pFileSelection));

        if(savestate_load(sPath) == false ) {
            GtkWidget *pDialog = gtk_message_dialog_new(GTK_WINDOW(pFileSelection),
                    GTK_DIALOG_MODAL,
                    GTK_MESSAGE_ERROR,
                    GTK_BUTTONS_OK,
                    "Unable to load :\n%s", sPath);
            gtk_dialog_run(GTK_DIALOG(pDialog));
            gtk_widget_destroy(pDialog);
        } else {
            gtk_action_set_sensitive(gtk_action_group_get_action(action_group, "run"), TRUE);
        }

        g_free(sPath);
        break;
    default:
        break;
    }
    gtk_widget_destroy(pFileSelection);
}

static void RecordMovieDialog()
{
 GtkFileFilter *pFilter_dsm, *pFilter_any;
    GtkWidget *pFileSelection;
    GtkWidget *pParent;
    gchar *sPath;

    if (desmume_running())
        Pause();

    pParent = GTK_WIDGET(pWindow);

    pFilter_dsm = gtk_file_filter_new();
    gtk_file_filter_add_pattern(pFilter_dsm, "*.dsm*");
    gtk_file_filter_set_name(pFilter_dsm, "DeSmuME movie file (.dsm*)");

    pFilter_any = gtk_file_filter_new();
    gtk_file_filter_add_pattern(pFilter_any, "*");
    gtk_file_filter_set_name(pFilter_any, "All files");

    /* Creating the selection window */
    pFileSelection = gtk_file_chooser_dialog_new("Save Movie To ...",
            GTK_WINDOW(pParent),
            GTK_FILE_CHOOSER_ACTION_SAVE,
            GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
            GTK_STOCK_SAVE, GTK_RESPONSE_OK,
            NULL);
    gtk_file_chooser_set_do_overwrite_confirmation (GTK_FILE_CHOOSER (pFileSelection), TRUE);

    /* Only the dialog window is accepting events: */
    gtk_window_set_modal(GTK_WINDOW(pFileSelection), TRUE);

    gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(pFileSelection), pFilter_dsm);
    gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(pFileSelection), pFilter_any);

    /* Showing the window */
    switch(gtk_dialog_run(GTK_DIALOG(pFileSelection))) {
    case GTK_RESPONSE_OK:
        sPath = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(pFileSelection));
        FCEUI_SaveMovie(sPath,L"",0,"", FCEUI_MovieGetRTCDefault());
        g_free(sPath);
        break;
    default:
        break;
    }
    gtk_widget_destroy(pFileSelection);
}

static void StopMovie()
{
	FCEUI_StopMovie();
}

static void PlayMovieDialog()
{
   GtkFileFilter *pFilter_dsm, *pFilter_any;
    GtkWidget *pFileSelection;
    GtkWidget *pParent;
    gchar *sPath;

    if (desmume_running())
        Pause();

    pParent = GTK_WIDGET(pWindow);

    pFilter_dsm = gtk_file_filter_new();
    gtk_file_filter_add_pattern(pFilter_dsm, "*.dsm*");
    gtk_file_filter_set_name(pFilter_dsm, "DeSmuME movie file (.dsm*)");

    pFilter_any = gtk_file_filter_new();
    gtk_file_filter_add_pattern(pFilter_any, "*");
    gtk_file_filter_set_name(pFilter_any, "All files");

    /* Creating the selection window */
    pFileSelection = gtk_file_chooser_dialog_new("Play movie from...",
            GTK_WINDOW(pParent),
            GTK_FILE_CHOOSER_ACTION_OPEN,
            GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
            GTK_STOCK_OPEN, GTK_RESPONSE_OK,
            NULL);
    gtk_file_chooser_set_do_overwrite_confirmation (GTK_FILE_CHOOSER (pFileSelection), TRUE);

    /* Only the dialog window is accepting events: */
    gtk_window_set_modal(GTK_WINDOW(pFileSelection), TRUE);

    gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(pFileSelection), pFilter_dsm);
    gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(pFileSelection), pFilter_any);

    /* Showing the window */
    switch(gtk_dialog_run(GTK_DIALOG(pFileSelection))) {
    case GTK_RESPONSE_OK:
        sPath = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(pFileSelection));

		FCEUI_LoadMovie(sPath,true,false,-1);

        g_free(sPath);
        break;
    default:
        break;
    }
    gtk_widget_destroy(pFileSelection);
}

static void SaveStateDialog()
{
    GtkFileFilter *pFilter_ds, *pFilter_any;
    GtkWidget *pFileSelection;
    GtkWidget *pParent;
    gchar *sPath;

    if (desmume_running())
        Pause();

    pParent = GTK_WIDGET(pWindow);

    pFilter_ds = gtk_file_filter_new();
    gtk_file_filter_add_pattern(pFilter_ds, "*.ds*");
    gtk_file_filter_set_name(pFilter_ds, "DeSmuME binary (.ds*)");

    pFilter_any = gtk_file_filter_new();
    gtk_file_filter_add_pattern(pFilter_any, "*");
    gtk_file_filter_set_name(pFilter_any, "All files");

    /* Creating the selection window */
    pFileSelection = gtk_file_chooser_dialog_new("Save State To ...",
            GTK_WINDOW(pParent),
            GTK_FILE_CHOOSER_ACTION_SAVE,
            GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
            GTK_STOCK_SAVE, GTK_RESPONSE_OK,
            NULL);
    gtk_file_chooser_set_do_overwrite_confirmation (GTK_FILE_CHOOSER (pFileSelection), TRUE);

    /* Only the dialog window is accepting events: */
    gtk_window_set_modal(GTK_WINDOW(pFileSelection), TRUE);

    gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(pFileSelection), pFilter_ds);
    gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(pFileSelection), pFilter_any);

    /* Showing the window */
    switch(gtk_dialog_run(GTK_DIALOG(pFileSelection))) {
    case GTK_RESPONSE_OK:
        sPath = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(pFileSelection));

        if(savestate_save(sPath) == false ) {
            GtkWidget *pDialog = gtk_message_dialog_new(GTK_WINDOW(pFileSelection),
                    GTK_DIALOG_MODAL,
                    GTK_MESSAGE_ERROR,
                    GTK_BUTTONS_OK,
                    "Unable to save :\n%s", sPath);
            gtk_dialog_run(GTK_DIALOG(pDialog));
            gtk_widget_destroy(pDialog);
        } else {
            gtk_action_set_sensitive(gtk_action_group_get_action(action_group, "run"), TRUE);
        }

        g_free(sPath);
        break;
    default:
        break;
    }
    gtk_widget_destroy(pFileSelection);
}

static void RecordAV_x264()
{
    GtkFileFilter *pFilter_mkv, *pFilter_mp4, *pFilter_any;
    GtkWidget *pFileSelection;
    GtkWidget *pParent;
    gchar *sPath;

    if (desmume_running())
        Pause();

    pParent = GTK_WIDGET(pWindow);

    pFilter_mkv = gtk_file_filter_new();
    gtk_file_filter_add_pattern(pFilter_mkv, "*.mkv");
    gtk_file_filter_set_name(pFilter_mkv, "Matroska (.mkv)");

    pFilter_mp4 = gtk_file_filter_new();
    gtk_file_filter_add_pattern(pFilter_mp4, "*.mp4");
    gtk_file_filter_set_name(pFilter_mp4, "MP4 (.mp4)");

    pFilter_any = gtk_file_filter_new();
    gtk_file_filter_add_pattern(pFilter_any, "*");
    gtk_file_filter_set_name(pFilter_any, "All files");

    /* Creating the selection window */
    pFileSelection = gtk_file_chooser_dialog_new("Save video...",
            GTK_WINDOW(pParent),
            GTK_FILE_CHOOSER_ACTION_SAVE,
            GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
            GTK_STOCK_SAVE, GTK_RESPONSE_OK,
            NULL);
    gtk_file_chooser_set_do_overwrite_confirmation (GTK_FILE_CHOOSER (pFileSelection), TRUE);

    /* Only the dialog window is accepting events: */
    gtk_window_set_modal(GTK_WINDOW(pFileSelection), TRUE);

    gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(pFileSelection), pFilter_mkv);
    gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(pFileSelection), pFilter_mp4);
    gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(pFileSelection), pFilter_any);

    /* Showing the window */
    switch(gtk_dialog_run(GTK_DIALOG(pFileSelection))) {
    case GTK_RESPONSE_OK:
        sPath = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(pFileSelection));

        if(avout_x264.begin(sPath)) {
            gtk_action_set_sensitive(gtk_action_group_get_action(action_group, "record_x264"), FALSE);
        } else {
            GtkWidget *pDialog = gtk_message_dialog_new(GTK_WINDOW(pFileSelection),
                    GTK_DIALOG_MODAL,
                    GTK_MESSAGE_ERROR,
                    GTK_BUTTONS_OK,
                    "Unable to record video to:\n%s", sPath);
            gtk_dialog_run(GTK_DIALOG(pDialog));
            gtk_widget_destroy(pDialog);
        }

        g_free(sPath);
        break;
    default:
        break;
    }
    gtk_widget_destroy(pFileSelection);
}

static void RecordAV_flac()
{
    GtkFileFilter *pFilter_flac, *pFilter_any;
    GtkWidget *pFileSelection;
    GtkWidget *pParent;
    gchar *sPath;

    if (desmume_running())
        Pause();

    pParent = GTK_WIDGET(pWindow);

    pFilter_flac = gtk_file_filter_new();
    gtk_file_filter_add_pattern(pFilter_flac, "*.flac");
    gtk_file_filter_set_name(pFilter_flac, "FLAC file (.flac)");

    pFilter_any = gtk_file_filter_new();
    gtk_file_filter_add_pattern(pFilter_any, "*");
    gtk_file_filter_set_name(pFilter_any, "All files");

    /* Creating the selection window */
    pFileSelection = gtk_file_chooser_dialog_new("Save audio...",
            GTK_WINDOW(pParent),
            GTK_FILE_CHOOSER_ACTION_SAVE,
            GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
            GTK_STOCK_SAVE, GTK_RESPONSE_OK,
            NULL);
    gtk_file_chooser_set_do_overwrite_confirmation (GTK_FILE_CHOOSER (pFileSelection), TRUE);

    /* Only the dialog window is accepting events: */
    gtk_window_set_modal(GTK_WINDOW(pFileSelection), TRUE);

    gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(pFileSelection), pFilter_flac);
    gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(pFileSelection), pFilter_any);

    /* Showing the window */
    switch(gtk_dialog_run(GTK_DIALOG(pFileSelection))) {
    case GTK_RESPONSE_OK:
        sPath = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(pFileSelection));

        if(avout_flac.begin(sPath)) {
            gtk_action_set_sensitive(gtk_action_group_get_action(action_group, "record_flac"), FALSE);
        } else {
            GtkWidget *pDialog = gtk_message_dialog_new(GTK_WINDOW(pFileSelection),
                    GTK_DIALOG_MODAL,
                    GTK_MESSAGE_ERROR,
                    GTK_BUTTONS_OK,
                    "Unable to record audio to:\n%s", sPath);
            gtk_dialog_run(GTK_DIALOG(pDialog));
            gtk_widget_destroy(pDialog);
        }

        g_free(sPath);
        break;
    default:
        break;
    }
    gtk_widget_destroy(pFileSelection);
}

static void RecordAV_stop() {
	avout_x264.end();
	avout_flac.end();
	gtk_action_set_sensitive(gtk_action_group_get_action(action_group, "record_x264"), TRUE);
	gtk_action_set_sensitive(gtk_action_group_get_action(action_group, "record_flac"), TRUE);
}

static void OpenNdsDialog()
{
    GtkFileFilter *pFilter_nds, *pFilter_dsgba, *pFilter_any;
    GtkWidget *pFileSelection;
    GtkWidget *pParent;
    gchar *sPath;

    if (desmume_running())
        Pause();

    pParent = GTK_WIDGET(pWindow);

    pFilter_nds = gtk_file_filter_new();
    gtk_file_filter_add_pattern(pFilter_nds, "*.[nN][dD][sS]");
#ifdef HAVE_LIBZ
    gtk_file_filter_add_pattern(pFilter_nds, "*.[nN][dD][sS].gz");
#endif
#ifdef HAVE_LIBZZIP
    gtk_file_filter_add_pattern(pFilter_nds, "*.[nN][dD][sS].zip");
#endif
    gtk_file_filter_set_name(pFilter_nds, "Nds binary (.nds)");

    pFilter_dsgba = gtk_file_filter_new();
    gtk_file_filter_add_pattern(pFilter_dsgba, "*.ds.gba");
    gtk_file_filter_set_name(pFilter_dsgba, "Nds binary with loader (.ds.gba)");

    pFilter_any = gtk_file_filter_new();
    gtk_file_filter_add_pattern(pFilter_any, "*");
    gtk_file_filter_set_name(pFilter_any, "All files");

    /* Creating the selection window */
    pFileSelection = gtk_file_chooser_dialog_new("Open...",
            GTK_WINDOW(pParent),
            GTK_FILE_CHOOSER_ACTION_OPEN,
            GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
            GTK_STOCK_OPEN, GTK_RESPONSE_OK,
            NULL);
    /* Only the dialog window is accepting events: */
    gtk_window_set_modal(GTK_WINDOW(pFileSelection), TRUE);

    gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(pFileSelection), pFilter_nds);
    gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(pFileSelection), pFilter_dsgba);
    gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(pFileSelection), pFilter_any);

    gtk_file_chooser_set_current_folder(GTK_FILE_CHOOSER(pFileSelection), g_get_home_dir());

    /* Showing the window */
    switch(gtk_dialog_run(GTK_DIALOG(pFileSelection))) {
    case GTK_RESPONSE_OK:
        sPath = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(pFileSelection));
        if(Open((const char*)sPath) < 0) {
            GtkWidget *pDialog = gtk_message_dialog_new(GTK_WINDOW(pFileSelection),
                    GTK_DIALOG_MODAL,
                    GTK_MESSAGE_ERROR,
                    GTK_BUTTONS_OK,
                    "Unable to load :\n%s", sPath);
            gtk_dialog_run(GTK_DIALOG(pDialog));
            gtk_widget_destroy(pDialog);
        } else {
            GtkRecentData recentData;
            gchar *uri;
            memset(&recentData, 0, sizeof(GtkRecentData));
            recentData.mime_type = g_strdup("application/x-nintendo-ds-rom");
            recentData.app_name = (gchar *) g_get_application_name ();
            recentData.app_exec = g_strjoin (" ", g_get_prgname (), "%f", NULL);

            GtkRecentManager *manager;
            manager = gtk_recent_manager_get_default ();
            uri = g_filename_to_uri (sPath, NULL, NULL);
            gtk_recent_manager_add_full (manager, uri, &recentData);

            g_free(uri);
            g_free(recentData.app_name);
            g_free(recentData.app_exec);
            gtk_action_set_sensitive(gtk_action_group_get_action(action_group, "run"), TRUE);
            Launch();
        }

        g_free(sPath);
        break;
    default:
        break;
    }
    gtk_widget_destroy(pFileSelection);
}

static void OpenRecent(GtkRecentChooser *chooser, gpointer user_data)
{
    GtkRecentManager *recent_manager = gtk_recent_manager_get_default();
    gchar *uri, *romname;
    int ret;

    if (desmume_running())
        Pause();

    uri = gtk_recent_chooser_get_current_uri(chooser);
    romname = g_filename_from_uri(uri, NULL, NULL);
    ret = Open(romname);
    if (ret > 0) {
        gtk_action_set_sensitive(gtk_action_group_get_action(action_group, "run"), TRUE);
        Launch();
    } else {
        gtk_recent_manager_remove_item(recent_manager, uri, NULL);
        GtkWidget *pDialog = gtk_message_dialog_new(GTK_WINDOW(pWindow),
                GTK_DIALOG_MODAL,
                GTK_MESSAGE_ERROR,
                GTK_BUTTONS_OK,
                "Unable to load :\n%s", uri);
        gtk_dialog_run(GTK_DIALOG(pDialog));
        gtk_widget_destroy(pDialog);
    }

    g_free(uri);
    g_free(romname);
}

static void Reset()
{
    bool shouldBeRunning = desmume_running();
    Pause();
    NDS_Reset();
    // Clear the NDS screen
    memset(GPU_screen, 0xFF, sizeof(GPU_screen));
    RedrawScreen();
    if (shouldBeRunning) {
        Launch();
    }
}


/////////////////////////////// DRAWING SCREEN //////////////////////////////////
static void UpdateDrawingAreaAspect()
{
    gint H, W;

    if (nds_screen.rotation_angle == 0 || nds_screen.rotation_angle == 180) {
        W = screen_size[nds_screen.orientation].width;
        H = screen_size[nds_screen.orientation].height;
    } else {
        W = screen_size[nds_screen.orientation].height;
        H = screen_size[nds_screen.orientation].width;
    }

    if (nds_screen.orientation != ORIENT_SINGLE) {
        if (nds_screen.orientation == ORIENT_VERTICAL) {
            if ((nds_screen.rotation_angle == 0 || nds_screen.rotation_angle == 180)) {
                H += nds_screen.gap_size;
            } else {
                W += nds_screen.gap_size;
            }
        }
    }

	if (winsize_current == WINSIZE_SCALE) {
		gtk_window_set_resizable(GTK_WINDOW(pWindow), TRUE);
		gtk_widget_set_size_request(GTK_WIDGET(pDrawingArea), W, H);
	} else {
		gtk_window_unmaximize(GTK_WINDOW(pWindow));
		gtk_window_set_resizable(GTK_WINDOW(pWindow), FALSE);
		gtk_widget_set_size_request(GTK_WIDGET(pDrawingArea), W * winsize_current / 2, H * winsize_current / 2);
	}
}

static void ToggleGap(GtkToggleAction* action)
{
    config.view_gap = gtk_toggle_action_get_active(action);
    nds_screen.gap_size = config.view_gap ? GAP_SIZE : 0;
    UpdateDrawingAreaAspect();
}

static void SetRotation(GtkAction *action, GtkRadioAction *current)
{
    nds_screen.rotation_angle = gtk_radio_action_get_current_value(current);
    config.view_rot = nds_screen.rotation_angle;
    UpdateDrawingAreaAspect();
}

static void SetWinsize(GtkAction *action, GtkRadioAction *current)
{
	winsize_current = (winsize_enum) gtk_radio_action_get_current_value(current);
	config.window_scale = winsize_current;
	if (config.window_fullscreen) {
		gtk_toggle_action_set_active((GtkToggleAction*)gtk_action_group_get_action(action_group, "fullscreen"), FALSE);
	}
	gtk_action_set_sensitive(gtk_action_group_get_action(action_group, "fullscreen"), winsize_current == WINSIZE_SCALE);
	UpdateDrawingAreaAspect();
}

static void SetOrientation(GtkAction *action, GtkRadioAction *current)
{
    nds_screen.orientation = (orientation_enum)gtk_radio_action_get_current_value(current);
#ifdef HAVE_LIBAGG
    osd->singleScreen = nds_screen.orientation == ORIENT_SINGLE;
#endif
    config.view_orient = nds_screen.orientation;
    UpdateDrawingAreaAspect();
}

static void ToggleSwapScreens(GtkToggleAction *action) {
    nds_screen.swap = gtk_toggle_action_get_active(action);
#ifdef HAVE_LIBAGG
    osd->swapScreens = nds_screen.swap;
#endif
    config.view_swap = nds_screen.swap;
    RedrawScreen();
}

static int ConfigureDrawingArea(GtkWidget *widget, GdkEventConfigure *event, gpointer data)
{
    return TRUE;
}

// Adapted from Cocoa port
static const uint8_t bits5to8[] = {
	0x00, 0x08, 0x10, 0x19, 0x21, 0x29, 0x31, 0x3A,
	0x42, 0x4A, 0x52, 0x5A, 0x63, 0x6B, 0x73, 0x7B,
	0x84, 0x8C, 0x94, 0x9C, 0xA5, 0xAD, 0xB5, 0xBD,
	0xC5, 0xCE, 0xD6, 0xDE, 0xE6, 0xEF, 0xF7, 0xFF
};

static inline uint32_t RGB555ToRGBA8888(const uint16_t color16)
{
	return	(bits5to8[((color16 >> 0) & 0x001F)] << 0) |
			(bits5to8[((color16 >> 5) & 0x001F)] << 8) |
			(bits5to8[((color16 >> 10) & 0x001F)] << 16) |
			0xFF000000;
}

static inline uint32_t RGB555ToBGRA8888(const uint16_t color16)
{
	return	(bits5to8[((color16 >> 10) & 0x001F)] << 0) |
			(bits5to8[((color16 >> 5) & 0x001F)] << 8) |
			(bits5to8[((color16 >> 0) & 0x001F)] << 16) |
			0xFF000000;
}

// Adapted from Cocoa port
static inline void RGB555ToRGBA8888Buffer(const uint16_t *__restrict__ srcBuffer, uint32_t *__restrict__ destBuffer, size_t pixelCount)
{
	const uint32_t *__restrict__ destBufferEnd = destBuffer + pixelCount;
	
	while (destBuffer < destBufferEnd)
	{
		*destBuffer++ = RGB555ToRGBA8888(*srcBuffer++);
	}
}

static inline void RGB555ToBGRA8888Buffer(const uint16_t *__restrict__ srcBuffer, uint32_t *__restrict__ destBuffer, size_t pixelCount)
{
	const uint32_t *__restrict__ destBufferEnd = destBuffer + pixelCount;
	
	while (destBuffer < destBufferEnd)
	{
		*destBuffer++ = RGB555ToBGRA8888(*srcBuffer++);
	}
}

static inline void gpu_screen_to_rgb(u32* dst)
{
    RGB555ToRGBA8888Buffer((u16*)GPU_screen, dst, 256 * 384);
}

static inline void drawScreen(cairo_t* cr, u32* buf, gint w, gint h) {
	cairo_surface_t* surf = cairo_image_surface_create_for_data((u8*)buf, CAIRO_FORMAT_RGB24, w, h, w * 4);
	cairo_set_source_surface(cr, surf, 0, 0);
	cairo_pattern_set_filter(cairo_get_source(cr), Interpolation);
	cairo_pattern_set_extend(cairo_get_source(cr), CAIRO_EXTEND_PAD);
	cairo_rectangle(cr, 0, 0, w, h);
	cairo_fill(cr);
	cairo_surface_destroy(surf);
}

static inline void drawTopScreen(cairo_t* cr, u32* buf, gint w, gint h, gint gap, gint rotation_angle, bool swap, orientation_enum orientation) {
	if (orientation == ORIENT_SINGLE && swap) {
		return;
	}
	cairo_save(cr);
	switch (orientation) {
	case ORIENT_VERTICAL:
		if (swap) {
			cairo_translate(cr, 0, h + gap);
		}
		break;
	case ORIENT_HORIZONTAL:
		if (swap) {
			cairo_translate(cr, w, 0);
		}
		break;
	}
	// Used for HUD editor mode
	cairo_get_matrix(cr, &nds_screen.topscreen_matrix);
	drawScreen(cr, buf, w, h);
	cairo_restore(cr);
}

static inline void drawBottomScreen(cairo_t* cr, u32* buf, gint w, gint h, gint gap, gint rotation_angle, bool swap, orientation_enum orientation) {
	if (orientation == ORIENT_SINGLE && !swap) {
		return;
	}
	cairo_save(cr);
	switch (orientation) {
	case ORIENT_VERTICAL:
		if (!swap) {
			cairo_translate(cr, 0, h + gap);
		}
		break;
	case ORIENT_HORIZONTAL:
		if (!swap) {
			cairo_translate(cr, w, 0);
		}
		break;
	}
	// Store the matrix for converting touchscreen coordinates
	cairo_get_matrix(cr, &nds_screen.touch_matrix);
	drawScreen(cr, buf, w, h);
	cairo_restore(cr);
}

/* Drawing callback */
static gboolean ExposeDrawingArea (GtkWidget *widget, GdkEventExpose *event, gpointer data)
{
	GdkWindow* window = gtk_widget_get_window(widget);
	gint daW, daH;
#if GTK_CHECK_VERSION(2,24,0)
	daW = gdk_window_get_width(window);
	daH = gdk_window_get_height(window);
#else
	gdk_drawable_get_size(window, &daW, &daH);
#endif
	u32* fbuf = video->GetDstBufferPtr();
	gint dstW = video->GetDstWidth();
	gint dstH = video->GetDstHeight();

	gint dstScale = dstW * 2 / 256; // Actual scale * 2 to handle 1.5x filters
	
	gint gap = nds_screen.orientation == ORIENT_VERTICAL ? nds_screen.gap_size * dstScale / 2 : 0;
	gint imgW, imgH;
	if (nds_screen.rotation_angle == 0 || nds_screen.rotation_angle == 180) {
		imgW = screen_size[nds_screen.orientation].width * dstScale / 2;
		imgH = screen_size[nds_screen.orientation].height * dstScale / 2 + gap;
	} else {
		imgH = screen_size[nds_screen.orientation].width * dstScale / 2;
		imgW = screen_size[nds_screen.orientation].height * dstScale / 2 + gap;
	}

	// Calculate scale to fit display area to window
	gfloat hratio = (gfloat)daW / (gfloat)imgW;
	gfloat vratio = (gfloat)daH / (gfloat)imgH;
	hratio = MIN(hratio, vratio);
	vratio = hratio;

	cairo_t* cr = gdk_cairo_create(window);

	// Scale to window size at center of area
	cairo_translate(cr, daW / 2, daH / 2);
	cairo_scale(cr, hratio, vratio);
	// Rotate area
	cairo_rotate(cr, M_PI / 180 * nds_screen.rotation_angle);
	// Translate area to top-left corner
	if (nds_screen.rotation_angle == 0 || nds_screen.rotation_angle == 180) {
		cairo_translate(cr, -imgW / 2, -imgH / 2);
	} else {
		cairo_translate(cr, -imgH / 2, -imgW / 2);
	}
	// Draw both screens
	drawTopScreen(cr, fbuf, dstW, dstH / 2, gap, nds_screen.rotation_angle, nds_screen.swap, nds_screen.orientation);
	drawBottomScreen(cr, fbuf + dstW * dstH / 2, dstW, dstH / 2, gap, nds_screen.rotation_angle, nds_screen.swap, nds_screen.orientation);
	// Draw gap
	cairo_set_source_rgb(cr, 0.3, 0.3, 0.3);
	cairo_rectangle(cr, 0, dstH / 2, dstW, gap);
	cairo_fill(cr);
	// Complete the touch transformation matrix
	cairo_matrix_scale(&nds_screen.topscreen_matrix, (double)dstScale / 2, (double)dstScale / 2);
	cairo_matrix_invert(&nds_screen.topscreen_matrix);
	cairo_matrix_scale(&nds_screen.touch_matrix, (double)dstScale / 2, (double)dstScale / 2);
	cairo_matrix_invert(&nds_screen.touch_matrix);

	cairo_destroy(cr);
	draw_count++;

	return TRUE;
}

static void RedrawScreen() {
	RGB555ToBGRA8888Buffer((u16*)GPU_screen, video->GetSrcBufferPtr(), 256 * 384);
#ifdef HAVE_LIBAGG
	aggDraw.hud->attach((u8*)video->GetSrcBufferPtr(), 256, 384, 1024);
	osd->update();
	DrawHUD();
	osd->clear();
#endif
	video->RunFilter();
	gtk_widget_queue_draw(pDrawingArea);
}

/////////////////////////////// KEYS AND STYLUS UPDATE ///////////////////////////////////////

#ifdef HAVE_LIBAGG
static gboolean rotoscaled_hudedit(gint x, gint y, gboolean start)
{
	double devX, devY;
	gint X, Y, topX = -1, topY = -1, botX = -1, botY = -1;
	static gint startScreen = 0;

	if (nds_screen.orientation != ORIENT_SINGLE || !nds_screen.swap) {
		devX = x;
		devY = y;
		cairo_matrix_transform_point(&nds_screen.topscreen_matrix, &devX, &devY);
		topX = devX;
		topY = devY;
	}

	if (nds_screen.orientation != ORIENT_SINGLE || nds_screen.swap) {
		devX = x;
		devY = y;
		cairo_matrix_transform_point(&nds_screen.touch_matrix, &devX, &devY);
		botX = devX;
		botY = devY;
	}

	if (topX >= 0 && topY >= 0 && topX < 256 && topY < 192) {
		X = topX;
		Y = topY + (nds_screen.swap ? 192 : 0);
		startScreen = 0;
	} else if (botX >= 0 && botY >= 0 && botX < 256 && botY < 192) {
		X = botX;
		Y = botY + (nds_screen.swap ? 0 : 192);
		startScreen = 1;
	} else if (!start) {
		if (startScreen == 0) {
			X = CLAMP(topX, 0, 255);
			Y = CLAMP(topY, 0, 191) + (nds_screen.swap ? 192 : 0);
		} else {
			X = CLAMP(botX, 0, 255);
			Y = CLAMP(botY, 0, 191) + (nds_screen.swap ? 0 : 192);
		}
	} else {
		LOG("TopX=%d, TopY=%d, BotX=%d, BotY=%d\n", topX, topY, botX, botY);
		return FALSE;
	}

	LOG("TopX=%d, TopY=%d, BotX=%d, BotY=%d, X=%d, Y=%d\n", topX, topY, botX, botY, X, Y);
	EditHud(X, Y, &Hud);
	RedrawScreen();
	return TRUE;
}
#endif

static gboolean rotoscaled_touchpos(gint x, gint y, gboolean start)
{
    double devX, devY;
    u16 EmuX, EmuY;
    gint X, Y;

    if (nds_screen.orientation == ORIENT_SINGLE && !nds_screen.swap) {
        return FALSE;
    }

    devX = x;
    devY = y;
    cairo_matrix_transform_point(&nds_screen.touch_matrix, &devX, &devY);
    X = devX;
    Y = devY;

    LOG("X=%d, Y=%d\n", X, Y);

    if (!start || (X >= 0 && Y >= 0 && X < 256 && Y < 192)) {
        EmuX = CLAMP(X, 0, 255);
        EmuY = CLAMP(Y, 0, 191);
        NDS_setTouchPos(EmuX, EmuY);
        return TRUE;
    }

    return FALSE;
}

static gboolean Stylus_Move(GtkWidget *w, GdkEventMotion *e, gpointer data)
{
    GdkModifierType state;
    gint x,y;

    if(click) {
        if(e->is_hint)
            gdk_window_get_pointer(w->window, &x, &y, &state);
        else {
            x= (gint)e->x;
            y= (gint)e->y;
            state=(GdkModifierType)e->state;
        }

        if(state & GDK_BUTTON1_MASK) {
#ifdef HAVE_LIBAGG
            if (HudEditorMode) {
                rotoscaled_hudedit(x, y, FALSE);
            } else {
#else
            {
#endif
                rotoscaled_touchpos(x, y, FALSE);
            }
        }
    }

    return TRUE;
}

static gboolean Stylus_Press(GtkWidget * w, GdkEventButton * e,
                             gpointer data)
{
    GdkModifierType state;
    gint x, y;

    if (e->button == 3) {
        GtkWidget * pMenu = gtk_menu_item_get_submenu ( GTK_MENU_ITEM(
                    gtk_ui_manager_get_widget (ui_manager, "/MainMenu/ViewMenu")));
        gtk_menu_popup(GTK_MENU(pMenu), NULL, NULL, NULL, NULL, 3, e->time);
    }

    if (e->button == 1) {
        gdk_window_get_pointer(w->window, &x, &y, &state);

        if(state & GDK_BUTTON1_MASK) {
#ifdef HAVE_LIBAGG
            if (HudEditorMode) {
                click = rotoscaled_hudedit(x, y, TRUE);
            } else
#endif
            if (desmume_running()) {
                click = rotoscaled_touchpos(x, y, TRUE);
            }
        }
    }

    return TRUE;
}
static gboolean Stylus_Release(GtkWidget *w, GdkEventButton *e, gpointer data)
{
#ifdef HAVE_LIBAGG
    HudClickRelease(&Hud);
#endif
    if(click) NDS_releaseTouch();
    click = FALSE;
    return TRUE;
}

static void loadgame(int num){
   if (desmume_running())
   {   
       Pause();
       loadstate_slot(num);
       Launch();
   }
   else
       loadstate_slot(num);
   RedrawScreen();
}

static void savegame(int num){
   if (desmume_running())
   {   
       Pause();
       savestate_slot(num);
       Launch();
   }
   else
       savestate_slot(num);
   LoadSaveStateInfo();
   RedrawScreen();
}

static void MenuLoad(GtkMenuItem *item, gpointer slot)
{
    loadgame(GPOINTER_TO_INT(slot));
}

static void MenuSave(GtkMenuItem *item, gpointer slot)
{
    savegame(GPOINTER_TO_INT(slot));
}

static gint Key_Press(GtkWidget *w, GdkEventKey *e, gpointer data)
{
  if (e->keyval == GDK_Shift_L){
      gdk_shift_pressed |= 1;
      return 1;
  }
  if (e->keyval == GDK_Shift_R){
      gdk_shift_pressed |= 2;
      return 1;
  }
  if( e->keyval >= GDK_F1 && e->keyval <= GDK_F10 ){
      if(!gdk_shift_pressed)
          loadgame((e->keyval - GDK_F1 + 1) % 10);
      else
          savegame((e->keyval - GDK_F1 + 1) % 10);
      return 1;
  }
  guint mask;
  mask = gtk_accelerator_get_default_mod_mask ();
  if( (e->state & mask) == 0){
    u16 Key = lookup_key(e->keyval);
    if(Key){
      ADD_KEY( keys_latch, Key );
      return 1;
    }
  }

#ifdef PROFILE_MEMORY_ACCESS
  if ( e->keyval == GDK_Tab) {
    print_memory_profiling();
    return 1;
  }
#endif
  return 0;
}

static gint Key_Release(GtkWidget *w, GdkEventKey *e, gpointer data)
{
  if (e->keyval == GDK_Shift_L){
      gdk_shift_pressed &= ~1;
      return 1;
  }
  if (e->keyval == GDK_Shift_R){
      gdk_shift_pressed &= ~2;
      return 1;
  }
  u16 Key = lookup_key(e->keyval);
  RM_KEY( keys_latch, Key );
  return 1;

}

/////////////////////////////// CONTROLS EDIT //////////////////////////////////////

static void AcceptNewInputKey(GtkWidget *w, GdkEventKey *e, struct modify_key_ctx *ctx)
{
    gchar *YouPressed;

    ctx->mk_key_chosen = e->keyval;
    YouPressed = g_strdup_printf("You pressed : %s\nClick OK to keep this key.", gdk_keyval_name(e->keyval));
    gtk_label_set_text(GTK_LABEL(ctx->label), YouPressed);
    g_free(YouPressed);
}

static void Modify_Key(GtkWidget* widget, gpointer data)
{
    struct modify_key_ctx ctx;
    GtkWidget *mkDialog;
    gchar *Key_Label;
    gchar *Title;
    gint Key;

    Key = GPOINTER_TO_INT(data);
    ctx.mk_key_chosen = 0;
    Title = g_strdup_printf("Press \"%s\" key ...\n", key_names[Key]);
    mkDialog = gtk_dialog_new_with_buttons(Title,
        GTK_WINDOW(pWindow),
        GTK_DIALOG_MODAL,
        GTK_STOCK_OK,GTK_RESPONSE_OK,
        GTK_STOCK_CANCEL,GTK_RESPONSE_CANCEL,
        NULL);

    ctx.label = gtk_label_new(Title);
    g_free(Title);
    gtk_box_pack_start(GTK_BOX(GTK_DIALOG(mkDialog)->vbox), ctx.label, TRUE, FALSE, 0);

    g_signal_connect(G_OBJECT(mkDialog), "key_press_event", G_CALLBACK(AcceptNewInputKey), &ctx);

    gtk_widget_show_all(GTK_DIALOG(mkDialog)->vbox);

    switch(gtk_dialog_run(GTK_DIALOG(mkDialog))) {
    case GTK_RESPONSE_OK:
        Keypad_Temp[Key] = ctx.mk_key_chosen;
        Key_Label = g_strdup_printf("%s (%s)", key_names[Key], gdk_keyval_name(Keypad_Temp[Key]));
        gtk_button_set_label(GTK_BUTTON(widget), Key_Label);
        g_free(Key_Label);
        break;
    case GTK_RESPONSE_CANCEL:
    case GTK_RESPONSE_NONE:
        ctx.mk_key_chosen = 0;
        break;
    }

    gtk_widget_destroy(mkDialog);

}

static void Edit_Controls()
{
    GtkWidget *ecDialog;
    GtkWidget *ecKey;
    gchar *Key_Label;
    int i;

    memcpy(&Keypad_Temp, &keyboard_cfg, sizeof(keyboard_cfg));

    ecDialog = gtk_dialog_new_with_buttons("Edit controls",
                        GTK_WINDOW(pWindow),
                        GTK_DIALOG_MODAL,
                        GTK_STOCK_OK,GTK_RESPONSE_OK,
                        GTK_STOCK_CANCEL,GTK_RESPONSE_CANCEL,
                        NULL);

    for(i = 0; i < NB_KEYS; i++) {
        Key_Label = g_strdup_printf("%s (%s)", key_names[i], gdk_keyval_name(Keypad_Temp[i]));
        ecKey = gtk_button_new_with_label(Key_Label);
        g_free(Key_Label);
        g_signal_connect(G_OBJECT(ecKey), "clicked", G_CALLBACK(Modify_Key), GINT_TO_POINTER(i));
        gtk_box_pack_start(GTK_BOX(GTK_DIALOG(ecDialog)->vbox), ecKey,TRUE, FALSE, 0);
    }

    gtk_widget_show_all(GTK_DIALOG(ecDialog)->vbox);

    switch (gtk_dialog_run(GTK_DIALOG(ecDialog))) {
    case GTK_RESPONSE_OK:
        memcpy(&keyboard_cfg, &Keypad_Temp, sizeof(keyboard_cfg));
        desmume_config_update_keys(keyfile);
        break;
    case GTK_RESPONSE_CANCEL:
    case GTK_RESPONSE_NONE:
        break;
    }
    gtk_widget_destroy(ecDialog);

}

static void AcceptNewJoyKey(GtkWidget *w, GdkEventFocus *e, struct modify_key_ctx *ctx)
{
    gchar *YouPressed;

    ctx->mk_key_chosen = get_joy_key(ctx->key_id);

    YouPressed = g_strdup_printf("You pressed : %d\nClick OK to keep this key.", ctx->mk_key_chosen);
    gtk_label_set_text(GTK_LABEL(ctx->label), YouPressed);
    g_free(YouPressed);
}

static void Modify_JoyKey(GtkWidget* widget, gpointer data)
{
    struct modify_key_ctx ctx;
    GtkWidget *mkDialog;
    gchar *Key_Label;
    gchar *Title;
    gint Key;

    Key = GPOINTER_TO_INT(data);
    /* Joypad keys start at 1 */
    ctx.key_id = Key+1;
    ctx.mk_key_chosen = 0;
    Title = g_strdup_printf("Press \"%s\" key ...\n", key_names[Key]);
    mkDialog = gtk_dialog_new_with_buttons(Title,
        GTK_WINDOW(pWindow),
        GTK_DIALOG_MODAL,
        GTK_STOCK_OK,GTK_RESPONSE_OK,
        GTK_STOCK_CANCEL,GTK_RESPONSE_CANCEL,
        NULL);

    ctx.label = gtk_label_new(Title);
    g_free(Title);
    gtk_box_pack_start(GTK_BOX(GTK_DIALOG(mkDialog)->vbox), ctx.label, TRUE, FALSE, 0);
    gtk_widget_show_all(GTK_DIALOG(mkDialog)->vbox);

    g_signal_connect(G_OBJECT(mkDialog), "focus_in_event", G_CALLBACK(AcceptNewJoyKey), &ctx);
   
    switch(gtk_dialog_run(GTK_DIALOG(mkDialog))) {
    case GTK_RESPONSE_OK:
        Keypad_Temp[Key] = ctx.mk_key_chosen;
        Key_Label = g_strdup_printf("%s (%d)", key_names[Key], Keypad_Temp[Key]);
        gtk_button_set_label(GTK_BUTTON(widget), Key_Label);
        g_free(Key_Label);
        break;
    case GTK_RESPONSE_CANCEL:
    case GTK_RESPONSE_NONE:
        ctx.mk_key_chosen = 0;
        break;
    }

    gtk_widget_destroy(mkDialog);

}

static void Edit_Joystick_Controls()
{
    GtkWidget *ecDialog;
    GtkWidget *ecKey;
    gchar *Key_Label;
    int i;

    memcpy(&Keypad_Temp, &joypad_cfg, sizeof(joypad_cfg));

    ecDialog = gtk_dialog_new_with_buttons("Edit controls",
                        GTK_WINDOW(pWindow),
                        GTK_DIALOG_MODAL,
                        GTK_STOCK_OK,GTK_RESPONSE_OK,
                        GTK_STOCK_CANCEL,GTK_RESPONSE_CANCEL,
                        NULL);

    for(i = 0; i < NB_KEYS; i++) {
        Key_Label = g_strdup_printf("%s (%d)", key_names[i], Keypad_Temp[i]);
        ecKey = gtk_button_new_with_label(Key_Label);
        g_free(Key_Label);
        g_signal_connect(G_OBJECT(ecKey), "clicked", G_CALLBACK(Modify_JoyKey), GINT_TO_POINTER(i));
        gtk_box_pack_start(GTK_BOX(GTK_DIALOG(ecDialog)->vbox), ecKey,TRUE, FALSE, 0);
    }

    gtk_widget_show_all(GTK_DIALOG(ecDialog)->vbox);

    switch (gtk_dialog_run(GTK_DIALOG(ecDialog))) {
    case GTK_RESPONSE_OK:
        memcpy(&joypad_cfg, &Keypad_Temp, sizeof(keyboard_cfg));
        desmume_config_update_joykeys(keyfile);
        break;
    case GTK_RESPONSE_CANCEL:
    case GTK_RESPONSE_NONE:
        break;
    }
    gtk_widget_destroy(ecDialog);

}

static void ToggleLayerVisibility(GtkToggleAction* action, gpointer data)
{
    guint Layer = GPOINTER_TO_UINT(data);
    gboolean active;

    // FIXME: make it work after resume
    if (!desmume_running())
        return;

    active = gtk_toggle_action_get_active(action);

    switch (Layer) {
    case MAIN_BG_0:
    case MAIN_BG_1:
    case MAIN_BG_2:
    case MAIN_BG_3:
    case MAIN_OBJ:
        if(active == TRUE) {
            GPU_addBack(MainScreen.gpu, Layer);
        } else {
            GPU_remove(MainScreen.gpu, Layer);
        }
        break;
    case SUB_BG_0:
    case SUB_BG_1:
    case SUB_BG_2:
    case SUB_BG_3:
    case SUB_OBJ:
        if(active == TRUE) {
            GPU_addBack(SubScreen.gpu, Layer-SUB_BG_0);
        } else { 
            GPU_remove(SubScreen.gpu, Layer-SUB_BG_0);
        }
        break;
    default:
        break;
    }
}

static void Printscreen()
{
    GdkPixbuf *screenshot;
    gchar *filename, *filen;
    GError *error = NULL;
    u8 rgb[256 * 384 * 4];
    static int seq = 0;
    gint H, W;

    //rgb = (u8 *) malloc(SCREENS_PIXEL_SIZE*SCREEN_BYTES_PER_PIXEL);
    //if (!rgb)
    //    return;

    if (nds_screen.rotation_angle == 0 || nds_screen.rotation_angle == 180) {
        W = screen_size[nds_screen.orientation].width;
        H = screen_size[nds_screen.orientation].height;
    } else {
        W = screen_size[nds_screen.orientation].height;
        H = screen_size[nds_screen.orientation].width;
    }

    gpu_screen_to_rgb((u32*)rgb);
    screenshot = gdk_pixbuf_new_from_data(rgb,
                          GDK_COLORSPACE_RGB,
                          TRUE,
                          8,
                          W,
                          H,
                          W * 4,
                          NULL,
                          NULL);

    filen = g_strdup_printf("desmume-screenshot-%d.png", seq);
    filename = g_build_filename(g_get_user_special_dir(G_USER_DIRECTORY_PICTURES), filen, NULL);

    gdk_pixbuf_save(screenshot, filename, "png", &error, NULL);
    if (error) {
        g_error_free (error);
        g_printerr("Failed to save %s", filename);
    } else {
        seq++;
    }

    //free(rgb);
    g_object_unref(screenshot);
    g_free(filename);
    g_free(filen);
}

#ifdef DESMUME_GTK_FIRMWARE_BROKEN
static void SelectFirmwareFile()
{
    GtkFileFilter *pFilter_nds, *pFilter_bin, *pFilter_any;
    GtkWidget *pFileSelection;
    GtkWidget *pParent;
    gchar *sPath;

    BOOL oldState = desmume_running();
    Pause();

    pParent = GTK_WIDGET(pWindow);

    pFilter_nds = gtk_file_filter_new();
    gtk_file_filter_add_pattern(pFilter_nds, "*.nds");
    gtk_file_filter_set_name(pFilter_nds, "Nds binary (.nds)");

    pFilter_bin = gtk_file_filter_new();
    gtk_file_filter_add_pattern(pFilter_bin, "*.bin");
    gtk_file_filter_set_name(pFilter_bin, "Binary file (.bin)");

    pFilter_any = gtk_file_filter_new();
    gtk_file_filter_add_pattern(pFilter_any, "*");
    gtk_file_filter_set_name(pFilter_any, "All files");

    pFileSelection = gtk_file_chooser_dialog_new("Load firmware...",
            GTK_WINDOW(pParent),
            GTK_FILE_CHOOSER_ACTION_OPEN,
            GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
            GTK_STOCK_OPEN, GTK_RESPONSE_OK,
            NULL);
    gtk_window_set_modal(GTK_WINDOW(pFileSelection), TRUE);

    gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(pFileSelection), pFilter_nds);
    gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(pFileSelection), pFilter_bin);
    gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(pFileSelection), pFilter_any);

    switch(gtk_dialog_run(GTK_DIALOG(pFileSelection))) {
    case GTK_RESPONSE_OK:
        sPath = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(pFileSelection));
        CommonSettings.UseExtFirmware = true;
        strncpy(CommonSettings.Firmware, (const char*)sPath, g_utf8_strlen(sPath, -1));
        g_free(sPath);
        break;
    default:
        break;
    }
    gtk_widget_destroy(pFileSelection);

    if(oldState) Launch();
}
#endif

static void Modify_PriInterpolation(GtkAction *action, GtkRadioAction *current)
{
    uint filter = gtk_radio_action_get_current_value(current) ;
    video->ChangeFilterByID((VideoFilterTypeID)filter);
    config.view_filter = filter;
    RedrawScreen();
}

static void Modify_Interpolation(GtkAction *action, GtkRadioAction *current)
{
    Interpolation = (cairo_filter_t)gtk_radio_action_get_current_value(current);
    config.view_cairoFilter = Interpolation;
    RedrawScreen();
}

static void Modify_SPUMode(GtkAction *action, GtkRadioAction *current)
{
    const uint mode = gtk_radio_action_get_current_value(current);

    switch (mode) {
    case SPUMODE_SYNCN:
    case SPUMODE_SYNCZ:
#ifdef HAVE_LIBSOUNDTOUCH
    case SPUMODE_SYNCP:
#endif
        SPUMode = mode;
        SPU_SetSynchMode(1, mode-1);
        break;

    case SPUMODE_DUALASYNC:
    default:
        SPUMode = SPUMODE_DUALASYNC;
        SPU_SetSynchMode(0, 0);
        break;
    }
    config.audio_sync = SPUMode;
}

static void Modify_SPUInterpolation(GtkAction *action, GtkRadioAction *current)
{
    CommonSettings.spuInterpolationMode = (SPUInterpolationMode)gtk_radio_action_get_current_value(current);
}

static void Modify_Frameskip(GtkAction *action, GtkRadioAction *current)
{
    autoFrameskipMax = gtk_radio_action_get_current_value(current) ;
    config.frameskip = autoFrameskipMax;
    if (!autoframeskip) {
        Frameskip = autoFrameskipMax;
    }
}

/////////////////////////////// TOOLS MANAGEMENT ///////////////////////////////

extern const dTool_t *dTools_list[];
extern const int dTools_list_size;

BOOL *dTools_running;

static void Start_dTool(GtkWidget *widget, gpointer data)
{
    int tool = GPOINTER_TO_INT(data);

    if(dTools_running == NULL || dTools_running[tool])
        return;

    dTools_list[tool]->open(tool);
    dTools_running[tool] = TRUE;
}

void dTool_CloseCallback(int tool)
{
    if (dTools_running == NULL)
        return;

    dTools_running[tool] = FALSE;
}


static inline void _updateDTools()
{
    if (dTools_running == NULL)
        return;

    for(int i = 0; i < dTools_list_size; i++) {
        if(dTools_running[i]) { dTools_list[i]->update(); }
    }
}

/////////////////////////////// MAIN EMULATOR LOOP ///////////////////////////////

class GtkDriver : public BaseDriver
{
public:
    virtual void EMU_DebugIdleUpdate()
    {
        usleep(1000);
        _updateDTools();
        while (gtk_events_pending())
            gtk_main_iteration();
    }

	// HUD uses this to show pause state
	virtual bool EMU_IsEmulationPaused() { return !desmume_running(); }

	virtual bool AVI_IsRecording()
	{
		return avout_x264.isRecording() || avout_flac.isRecording();
	}

	virtual void AVI_SoundUpdate(void* soundData, int soundLen) { 
		avout_flac.updateAudio(soundData, soundLen);
	}
};

static void DoQuit()
{
    emu_halt();
    gtk_main_quit();
}


gboolean EmuLoop(gpointer data)
{
    static Uint32 fps_SecStart, next_fps_SecStart, fps_FrameCount, skipped_frames;
    static int last_tick;
    static unsigned next_frame_time;
    static int frame_mod3;
    unsigned int i;
    gchar Title[50];

    if (!desmume_running()) {
      // Set the next frame time to 0 so that it will recount
      next_frame_time = 0;
      frame_mod3 = 0;
      gtk_window_set_title(GTK_WINDOW(pWindow), "DeSmuME - Paused");
      fps_SecStart = 0;
      regMainLoop = FALSE;
      RedrawScreen();
      return FALSE;
    }

    fps_FrameCount += Frameskip + 1;
    next_fps_SecStart = SDL_GetTicks();

    if (fps_SecStart == 0) {
    	fps_SecStart = next_fps_SecStart;
        fps_FrameCount = 0;
        gtk_window_set_title(GTK_WINDOW(pWindow), "DeSmuME - Running");
    }

    bool oneSecond = false;
    if ((next_fps_SecStart - fps_SecStart) >= 1000) {
        oneSecond = true;
        fps_SecStart = next_fps_SecStart;

        float emu_ratio = fps_FrameCount / 60.0;
        LOG("auto: %d fps: %u skipped: %u emu_ratio: %f Frameskip: %u\n", autoframeskip, fps_FrameCount, skipped_frames, emu_ratio, Frameskip);

        snprintf(Title, sizeof(Title), "DeSmuME - %dfps, %d skipped, draw: %dfps", fps_FrameCount, skipped_frames, draw_count);
        gtk_window_set_title(GTK_WINDOW(pWindow), Title);

#ifdef HAVE_LIBAGG
        Hud.fps = fps_FrameCount;
#endif

        fps_FrameCount = 0;
        skipped_frames = 0;
        draw_count = 0;
    }

	// HUD display things (copied from Windows main.cpp)
#ifdef HAVE_LIBAGG
	gfx3d.frameCtrRaw++;
	if(gfx3d.frameCtrRaw == 60) {
		Hud.fps3d = gfx3d.frameCtr;
		gfx3d.frameCtrRaw = 0;
		gfx3d.frameCtr = 0;
	}
	if(nds.idleFrameCounter==0 || oneSecond) 
	{
		//calculate a 16 frame arm9 load average
		for(int cpu=0;cpu<2;cpu++)
		{
			int load = 0;
			//printf("%d: ",cpu);
			for(int i=0;i<16;i++)
			{
				//blend together a few frames to keep low-framerate games from having a jittering load average
				//(they will tend to work 100% for a frame and then sleep for a while)
				//4 frames should handle even the slowest of games
				s32 sample = 
					nds.runCycleCollector[cpu][(i+0+nds.idleFrameCounter)&15]
				+	nds.runCycleCollector[cpu][(i+1+nds.idleFrameCounter)&15]
				+	nds.runCycleCollector[cpu][(i+2+nds.idleFrameCounter)&15]
				+	nds.runCycleCollector[cpu][(i+3+nds.idleFrameCounter)&15];
				sample /= 4;
				load = load/8 + sample*7/8;
			}
			//printf("\n");
			load = std::min(100,std::max(0,(int)(load*100/1120380)));
			Hud.cpuload[cpu] = load;
		}
	}
	Hud.cpuloopIterationCount = nds.cpuloopIterationCount;
#endif

    /* Merge the joystick keys with the keyboard ones */
    process_joystick_events(&keys_latch);
    /* Update! */
    update_keypad(keys_latch);

    desmume_cycle();    /* Emule ! */

    _updateDTools();
	avout_x264.updateVideo((u16*)GPU_screen);
	RedrawScreen();

    if (!config.fpslimiter || keys_latch & KEYMASK_(KEY_BOOST - 1)) {
        if (autoframeskip) {
            Frameskip = 0;
        } else {
            for (i = 0; i < Frameskip; i++) {
                NDS_SkipNextFrame();
#ifdef HAVE_LIBAGG
                gfx3d.frameCtrRaw++;
                if(gfx3d.frameCtrRaw == 60) {
                    Hud.fps3d = gfx3d.frameCtr;
                    gfx3d.frameCtrRaw = 0;
                    gfx3d.frameCtr = 0;
                }
#endif
                desmume_cycle();
                skipped_frames++;
            }
        }
        next_frame_time = SDL_GetTicks() + 16;
        frame_mod3 = 0;
    } else {
        if (!autoframeskip) {
            for (i = 0; i < Frameskip; i++) {
                NDS_SkipNextFrame();
#ifdef HAVE_LIBAGG
                gfx3d.frameCtrRaw++;
                if(gfx3d.frameCtrRaw == 60) {
                    Hud.fps3d = gfx3d.frameCtr;
                    gfx3d.frameCtrRaw = 0;
                    gfx3d.frameCtr = 0;
                }
#endif
                desmume_cycle();
                skipped_frames++;
                // Update next frame time
                frame_mod3 = (frame_mod3 + 1) % 3;
                next_frame_time += (frame_mod3 == 0) ? 16 : 17;
            }
        }
        unsigned this_tick = SDL_GetTicks();
        if (this_tick < next_frame_time) {
            unsigned timeleft = next_frame_time - this_tick;
            usleep((timeleft - 1) * 1000);
            while (SDL_GetTicks() < next_frame_time);
            this_tick = SDL_GetTicks();
        }
        if (autoframeskip) {
            // Determine the auto frameskip value, maximum 4
            for (Frameskip = 0; this_tick > next_frame_time && Frameskip < autoFrameskipMax; Frameskip++, this_tick = SDL_GetTicks()) {
                // Aggressively skip frames to avoid delay
                NDS_SkipNextFrame();
#ifdef HAVE_LIBAGG
                gfx3d.frameCtrRaw++;
                if(gfx3d.frameCtrRaw == 60) {
                    Hud.fps3d = gfx3d.frameCtr;
                    gfx3d.frameCtrRaw = 0;
                    gfx3d.frameCtr = 0;
                }
#endif
                desmume_cycle();
                skipped_frames++;
                // Update next frame time
                frame_mod3 = (frame_mod3 + 1) % 3;
                next_frame_time += (frame_mod3 == 0) ? 16 : 17;
            }
            if (Frameskip > autoFrameskipMax) {
                Frameskip = autoFrameskipMax;
            }
            //while (SDL_GetTicks() < next_frame_time);
            //this_tick = SDL_GetTicks();
        }
        if (this_tick > next_frame_time && this_tick - next_frame_time >= 17) {
            // If we fall too much behind, don't try to catch up.
            next_frame_time = this_tick;
        }
        // We want to achieve a total 17 + 17 + 16 = 50 ms for every 3 frames.
        frame_mod3 = (frame_mod3 + 1) % 3;
        next_frame_time += (frame_mod3 == 0) ? 16 : 17;
    }

    return TRUE;
}

static void desmume_try_adding_ui(GtkUIManager *self, const char *ui_descr){
    GError *error;
    error = NULL;
    if (!gtk_ui_manager_add_ui_from_string (self, ui_descr, -1, &error))
      {
        g_message ("building menus failed: %s", error->message);
        g_error_free (error);
        exit (EXIT_FAILURE);
      }
}

static void dui_set_accel_group(gpointer action, gpointer group) {
        gtk_action_set_accel_group((GtkAction *)action, (GtkAccelGroup *)group);
}

// The following functions are adapted from the Windows port:
//     UpdateSaveStateMenu, ResetSaveStateTimes, LoadSaveStateInfo
static void UpdateSaveStateMenu(int pos, char* txt)
{
	char name[64];
        snprintf(name, sizeof(name), "savestate%d", (pos == 0) ? 10 : pos);
	gtk_action_set_label(gtk_action_group_get_action(action_group, name), txt);
        snprintf(name, sizeof(name), "loadstate%d", (pos == 0) ? 10 : pos);
	gtk_action_set_label(gtk_action_group_get_action(action_group, name), txt);
}

static void ResetSaveStateTimes()
{
	char ntxt[64];
	for(int i = 0; i < NB_STATES;i++)
	{
		snprintf(ntxt, sizeof(ntxt), "_%d", i);
		UpdateSaveStateMenu(i, ntxt);
	}
}

static void LoadSaveStateInfo()
{
	scan_savestates();
	char ntxt[128];
	for(int i = 0; i < NB_STATES; i++)
	{
		if(savestates[i].exists)
		{
			snprintf(ntxt, sizeof(ntxt), "_%d    %s", i, savestates[i].date);
			UpdateSaveStateMenu(i, ntxt);
		}
	}
}

static void desmume_gtk_menu_file_saveload_slot (GtkActionGroup *ag)
{
    for(guint i = 1; i <= 10; i++){
        GtkAction *act;
        char label[64], name[64], accel[64];

        snprintf(label, sizeof(label), "_%d", i % 10);

        // Note: GTK+ doesn't handle Shift correctly, so the actual action is
        // done in Key_Press. The accelerators here are simply visual cues.

        snprintf(name, sizeof(name), "savestate%d", i);
        snprintf(accel, sizeof(accel), "<Shift>F%d", i);
        act = gtk_action_new(name, label, NULL, NULL);
        g_signal_connect(G_OBJECT(act), "activate", G_CALLBACK(MenuSave), GUINT_TO_POINTER(i % 10));
        gtk_action_group_add_action_with_accel(ag, GTK_ACTION(act), accel);

        snprintf(name, sizeof(name), "loadstate%d", i);
        snprintf(accel, sizeof(accel), "F%d", i);
        act = gtk_action_new(name, label, NULL, NULL);
        g_signal_connect(G_OBJECT(act), "activate", G_CALLBACK(MenuLoad), GUINT_TO_POINTER(i % 10));
        gtk_action_group_add_action_with_accel(ag, GTK_ACTION(act), accel);
    }
}

static void changesavetype(GtkAction *action, GtkRadioAction *current)
{
    backup_setManualBackupType( gtk_radio_action_get_current_value(current));
}

static void desmume_gtk_menu_tool_layers (GtkActionGroup *ag)
{
    const char *Layers_Menu[10][2] = {
        {"layermainbg0","_0 Main BG 0"},
        {"layermainbg1","_1 Main BG 1"},
        {"layermainbg2","_2 Main BG 2"},
        {"layermainbg3","_3 Main BG 3"},
        {"layermainobj","_4 Main OBJ"},
        {"layersubbg0", "_5 SUB BG 0"},
        {"layersubbg1", "_6 SUB BG 1"},
        {"layersubbg2", "_7 SUB BG 2"},
        {"layersubbg3", "_8 SUB BG 3"},
        {"layersubobj", "_9 SUB OBJ"}
    };
    guint i;

    GtkToggleAction *act;
    for(i = 0; i< 10; i++){
        act = gtk_toggle_action_new(Layers_Menu[i][0],Layers_Menu[i][1],NULL,NULL);
        gtk_toggle_action_set_active(act, TRUE);
        g_signal_connect(G_OBJECT(act), "activate", G_CALLBACK(ToggleLayerVisibility), GUINT_TO_POINTER(i));
        gtk_action_group_add_action_with_accel(ag, GTK_ACTION(act), NULL);
    }
}

#ifdef HAVE_LIBAGG
enum hud_display_enum {
    HUD_DISPLAY_FPS,
    HUD_DISPLAY_INPUT,
    HUD_DISPLAY_GINPUT,
    HUD_DISPLAY_FCOUNTER,
    HUD_DISPLAY_LCOUNTER,
    HUD_DISPLAY_RTC,
    HUD_DISPLAY_MIC,
    HUD_DISPLAY_EDITOR,
};

static void ToggleHudDisplay(GtkToggleAction* action, gpointer data)
{
    guint hudId = GPOINTER_TO_UINT(data);
    gboolean active;

    active = gtk_toggle_action_get_active(action);

    switch (hudId) {
    case HUD_DISPLAY_FPS:
        CommonSettings.hud.FpsDisplay = active;
        config.hud_fps = active;
        break;
    case HUD_DISPLAY_INPUT:
        CommonSettings.hud.ShowInputDisplay = active;
        config.hud_input = active;
        break;
    case HUD_DISPLAY_GINPUT:
        CommonSettings.hud.ShowGraphicalInputDisplay = active;
        config.hud_graphicalInput = active;
        break;
    case HUD_DISPLAY_FCOUNTER:
        CommonSettings.hud.FrameCounterDisplay = active;
        config.hud_frameCounter = active;
        break;
    case HUD_DISPLAY_LCOUNTER:
        CommonSettings.hud.ShowLagFrameCounter = active;
        config.hud_lagCounter = active;
        break;
    case HUD_DISPLAY_RTC:
        CommonSettings.hud.ShowRTC = active;
        config.hud_rtc = active;
        break;
    case HUD_DISPLAY_MIC:
        CommonSettings.hud.ShowMicrophone = active;
        config.hud_mic = active;
        break;
    case HUD_DISPLAY_EDITOR:
        HudEditorMode = active;
        break;
    default:
        g_printerr("Unknown HUD toggle %u!", hudId);
        break;
    }
    RedrawScreen();
}

static void desmume_gtk_menu_view_hud (GtkActionGroup *ag)
{
    const struct {
        const gchar* name;
        const gchar* label;
        guint id;
        bool active;
        bool& setting;
    } hud_menu[] = {
        { "hud_fps","Display _fps", HUD_DISPLAY_FPS, config.hud_fps, CommonSettings.hud.FpsDisplay },
        { "hud_input","Display _Input", HUD_DISPLAY_INPUT, config.hud_input, CommonSettings.hud.ShowInputDisplay },
        { "hud_graphicalinput","Display _Graphical Input", HUD_DISPLAY_GINPUT, config.hud_graphicalInput, CommonSettings.hud.ShowGraphicalInputDisplay },
        { "hud_framecounter","Display Frame _Counter", HUD_DISPLAY_FCOUNTER, config.hud_frameCounter, CommonSettings.hud.FrameCounterDisplay },
        { "hud_lagcounter","Display _Lag Counter", HUD_DISPLAY_LCOUNTER, config.hud_lagCounter, CommonSettings.hud.ShowLagFrameCounter },
        { "hud_rtc","Display _RTC", HUD_DISPLAY_RTC, config.hud_rtc, CommonSettings.hud.ShowRTC },
        { "hud_mic","Display _Mic", HUD_DISPLAY_MIC, config.hud_mic, CommonSettings.hud.ShowMicrophone },
        { "hud_editor","_Editor Mode", HUD_DISPLAY_EDITOR, false, HudEditorMode },
    };
    guint i;

    GtkToggleAction *act;
    for(i = 0; i < sizeof(hud_menu) / sizeof(hud_menu[0]); i++){
        act = gtk_toggle_action_new(hud_menu[i].name, hud_menu[i].label, NULL, NULL);
        gtk_toggle_action_set_active(act, hud_menu[i].active ? TRUE : FALSE);
        hud_menu[i].setting = hud_menu[i].active;
        g_signal_connect(G_OBJECT(act), "activate", G_CALLBACK(ToggleHudDisplay), GUINT_TO_POINTER(hud_menu[i].id));
        gtk_action_group_add_action_with_accel(ag, GTK_ACTION(act), NULL);
    }
}
#endif

static void ToggleAudio (GtkToggleAction *action)
{
    config.audio_enabled = gtk_toggle_action_get_active(action);
    if (config.audio_enabled) {
        SPU_ChangeSoundCore(SNDCORE_SDL, 735 * 4);
        osd->addLine("Audio enabled");
    } else {
        SPU_ChangeSoundCore(0, 0);
        osd->addLine("Audio disabled");
    }
    RedrawScreen();
}

#ifdef FAKE_MIC
static void ToggleMicNoise (GtkToggleAction *action)
{
    BOOL doNoise = (BOOL)gtk_toggle_action_get_active(action);

    Mic_DoNoise(doNoise);
    if (doNoise)
       osd->addLine("Fake mic enabled");
    else
       osd->addLine("Fake mic disabled");
    RedrawScreen();
}
#endif

static void ToggleFpsLimiter (GtkToggleAction *action)
{
    config.fpslimiter = (BOOL)gtk_toggle_action_get_active(action);

    if (config.fpslimiter)
       osd->addLine("Fps limiter enabled");
    else
       osd->addLine("Fps limiter disabled");
    RedrawScreen();
}

static void ToggleAutoFrameskip (GtkToggleAction *action)
{
    config.autoframeskip = (BOOL)gtk_toggle_action_get_active(action);

    if (config.autoframeskip) {
        autoframeskip = true;
        Frameskip = 0;
        osd->addLine("Auto frameskip enabled");
    } else {
        autoframeskip = false;
        Frameskip = autoFrameskipMax;
        osd->addLine("Auto frameskip disabled");
    }
    RedrawScreen();
}

static void desmume_gtk_menu_tools (GtkActionGroup *ag)
{
    gint i;
    for(i = 0; i < dTools_list_size; i++) {
        GtkAction *act;
        act = gtk_action_new(dTools_list[i]->shortname, dTools_list[i]->name, NULL, NULL);
        g_signal_connect(G_OBJECT(act), "activate", G_CALLBACK(Start_dTool), GINT_TO_POINTER(i));
        gtk_action_group_add_action(ag, GTK_ACTION(act));
    }
}

static gboolean timeout_exit_cb(gpointer data)
{
    DoQuit();
    INFO("Quit after %d seconds timeout\n", GPOINTER_TO_INT(data));

    return FALSE;
}

static int
common_gtk_main( class configured_features *my_config)
{
	config.load();

    driver = new GtkDriver();

    SDL_TimerID limiter_timer = NULL;

    GtkAccelGroup * accel_group;
    GtkWidget *pVBox;
    GtkWidget *pMenuBar;
    GtkWidget *pToolBar;

    /* the firmware settings */
    struct NDS_fw_config_data fw_config;

    /* default the firmware settings, they may get changed later */
    NDS_FillDefaultFirmwareConfigData( &fw_config);

    /* use any language set on the command line */
    if ( my_config->firmware_language != -1) {
        fw_config.language = my_config->firmware_language;
    }


    //------------------addons----------
    my_config->process_addonCommands();

    int slot2_device_type = NDS_SLOT2_AUTO;

    if (my_config->is_cflash_configured)
        slot2_device_type = NDS_SLOT2_CFLASH;

    if(my_config->gbaslot_rom != "") {

        //set the GBA rom and sav paths
        GBACartridge_RomPath = my_config->gbaslot_rom.c_str();
        if(toupper(strright(GBACartridge_RomPath,4)) == ".GBA")
          GBACartridge_SRAMPath = strright(GBACartridge_RomPath,4) + ".sav";
        else
          //what to do? lets just do the same thing for now
          GBACartridge_SRAMPath = strright(GBACartridge_RomPath,4) + ".sav";

        // Check if the file exists and can be opened
        FILE * test = fopen(GBACartridge_RomPath.c_str(), "rb");
        if (test) {
            slot2_device_type = NDS_SLOT2_GBACART;
            fclose(test);
        }
    }

	switch (slot2_device_type)
	{
		case NDS_SLOT2_NONE:
			break;
		case NDS_SLOT2_AUTO:
			break;
		case NDS_SLOT2_CFLASH:
			break;
		case NDS_SLOT2_RUMBLEPAK:
			break;
		case NDS_SLOT2_GBACART:
			break;
		case NDS_SLOT2_GUITARGRIP:
			break;
		case NDS_SLOT2_EXPMEMORY:
			break;
		case NDS_SLOT2_EASYPIANO:
			break;
		case NDS_SLOT2_PADDLE:
			break;
		case NDS_SLOT2_PASSME:
			break;
		default:
			slot2_device_type = NDS_SLOT2_NONE;
			break;
	}
    
    slot2_Init();
    slot2_Change((NDS_SLOT2_TYPE)slot2_device_type);

    /* FIXME: SDL_INIT_VIDEO is needed for joystick support to work!?
     * Perhaps it needs a "window" to catch events...? */
    if(SDL_Init(SDL_INIT_TIMER|SDL_INIT_VIDEO) == -1) {
        g_printerr("Error trying to initialize SDL: %s\n",
                    SDL_GetError());
        return 1;
    }
    desmume_init( my_config->disable_sound || !config.audio_enabled);

    /* Init the hud / osd stuff */
#ifdef HAVE_LIBAGG
    Desmume_InitOnce();
    Hud.reset();
#endif

    /*
     * Activate the GDB stubs
     * This has to come after the NDS_Init (called in desmume_init)
     * where the cpus are set up.
     */
#ifdef GDB_STUB
    gdbstub_mutex_init();
    
    gdbstub_handle_t arm9_gdb_stub = NULL;
    gdbstub_handle_t arm7_gdb_stub = NULL;
    
    if ( my_config->arm9_gdb_port > 0) {
        arm9_gdb_stub = createStub_gdb( my_config->arm9_gdb_port,
                                         &NDS_ARM9,
                                         &arm9_direct_memory_iface);
        
        if ( arm9_gdb_stub == NULL) {
            g_printerr("Failed to create ARM9 gdbstub on port %d\n",
                       my_config->arm9_gdb_port);
            exit( -1);
        }
        else {
            activateStub_gdb( arm9_gdb_stub);
        }
    }
    if ( my_config->arm7_gdb_port > 0) {
        arm7_gdb_stub = createStub_gdb( my_config->arm7_gdb_port,
                                         &NDS_ARM7,
                                         &arm7_base_memory_iface);
        
        if ( arm7_gdb_stub == NULL) {
            g_printerr("Failed to create ARM7 gdbstub on port %d\n",
                       my_config->arm7_gdb_port);
            exit( -1);
        }
        else {
            activateStub_gdb( arm7_gdb_stub);
        }
    }
#endif

    /* Create the dummy firmware */
    NDS_CreateDummyFirmware( &fw_config);

    /* Initialize joysticks */
    if(!init_joy()) return 1;

    dTools_running = (BOOL*)malloc(sizeof(BOOL) * dTools_list_size);
    if (dTools_running != NULL) 
      memset(dTools_running, FALSE, sizeof(BOOL) * dTools_list_size); 

    keyfile = desmume_config_read_file(gtk_kb_cfg);

    memset(&nds_screen, 0, sizeof(nds_screen));
    nds_screen.orientation = ORIENT_VERTICAL;

    g_printerr("Using %d threads for video filter.\n", CommonSettings.num_cores);
    video = new VideoFilter(256, 384, VideoFilterTypeID_None, CommonSettings.num_cores);

    /* Create the window */
    pWindow = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(pWindow), "DeSmuME");
    gtk_window_set_resizable(GTK_WINDOW (pWindow), TRUE);
    gtk_window_set_icon(GTK_WINDOW (pWindow), gdk_pixbuf_new_from_xpm_data(DeSmuME_xpm));

    g_signal_connect(G_OBJECT(pWindow), "destroy", G_CALLBACK(DoQuit), NULL);
    g_signal_connect(G_OBJECT(pWindow), "key_press_event", G_CALLBACK(Key_Press), NULL);
    g_signal_connect(G_OBJECT(pWindow), "key_release_event", G_CALLBACK(Key_Release), NULL);

    /* Create the GtkVBox */
    pVBox = gtk_vbox_new(FALSE, 0);
    gtk_container_add(GTK_CONTAINER(pWindow), pVBox);

    ui_manager = gtk_ui_manager_new ();
    accel_group = gtk_accel_group_new();
    action_group = gtk_action_group_new("dui");

    gtk_action_group_add_actions(action_group, action_entries, G_N_ELEMENTS(action_entries), NULL);
    gtk_action_group_add_toggle_actions(action_group, toggle_entries, G_N_ELEMENTS(toggle_entries), NULL);
    /* Update audio toggle status */
    if (my_config->disable_sound || !config.audio_enabled) {
        GtkAction *action = gtk_action_group_get_action(action_group, "enableaudio");
        if (action)
            gtk_toggle_action_set_active((GtkToggleAction *)action, FALSE);
    }
    desmume_gtk_menu_tool_layers(action_group);
#ifdef HAVE_LIBAGG
    desmume_gtk_menu_view_hud(action_group);
#else
    gtk_action_set_sensitive(gtk_action_group_get_action(action_group, "hud_notsupported"), FALSE);
#endif
    desmume_gtk_menu_file_saveload_slot(action_group);
    desmume_gtk_menu_tools(action_group);
    gtk_action_group_add_radio_actions(action_group, savet_entries, G_N_ELEMENTS(savet_entries), 
            my_config->savetype, G_CALLBACK(changesavetype), NULL);

    if (config.view_cairoFilter < CAIRO_FILTER_FAST || config.view_cairoFilter > CAIRO_FILTER_BILINEAR) {
    	config.view_cairoFilter = CAIRO_FILTER_NEAREST;
    }
    Interpolation = (cairo_filter_t)config.view_cairoFilter.get();
    gtk_action_group_add_radio_actions(action_group, interpolation_entries, G_N_ELEMENTS(interpolation_entries), 
            Interpolation, G_CALLBACK(Modify_Interpolation), NULL);

    if (config.view_filter < VideoFilterTypeID_None || config.view_filter >= VideoFilterTypeIDCount) {
        config.view_filter = VideoFilterTypeID_None;
    }
    video->ChangeFilterByID((VideoFilterTypeID)config.view_filter.get());
    gtk_action_group_add_radio_actions(action_group, pri_interpolation_entries, G_N_ELEMENTS(pri_interpolation_entries), 
            config.view_filter, G_CALLBACK(Modify_PriInterpolation), NULL);

    switch (config.audio_sync) {
    case SPUMODE_SYNCN:
    case SPUMODE_SYNCZ:
#ifdef HAVE_LIBSOUNDTOUCH
    case SPUMODE_SYNCP:
#endif
        SPUMode = config.audio_sync;
        SPU_SetSynchMode(1, config.audio_sync-1);
        break;

    case SPUMODE_DUALASYNC:
    default:
        config.audio_sync = SPUMODE_DUALASYNC;
        SPUMode = SPUMODE_DUALASYNC;
        SPU_SetSynchMode(0, 0);
        break;
    }
    gtk_action_group_add_radio_actions(action_group, spumode_entries, G_N_ELEMENTS(spumode_entries),
            config.audio_sync, G_CALLBACK(Modify_SPUMode), NULL);

    gtk_action_group_add_radio_actions(action_group, spuinterpolation_entries, G_N_ELEMENTS(spuinterpolation_entries),
            CommonSettings.spuInterpolationMode, G_CALLBACK(Modify_SPUInterpolation), NULL);

    gtk_action_group_add_radio_actions(action_group, frameskip_entries, G_N_ELEMENTS(frameskip_entries), 
            config.frameskip, G_CALLBACK(Modify_Frameskip), NULL);
    autoFrameskipMax = config.frameskip;
    gtk_toggle_action_set_active((GtkToggleAction*)gtk_action_group_get_action(action_group, "frameskipA"), config.autoframeskip);
    if (config.autoframeskip) {
        autoframeskip = true;
        Frameskip = 0;
    } else {
        autoframeskip = false;
        Frameskip = autoFrameskipMax;
    }

    switch (config.view_rot) {
        case 0:
        case 90:
        case 180:
        case 270:
            break;
        default:
            config.view_rot = 0;
            break;
    }
    nds_screen.rotation_angle = config.view_rot;
    gtk_action_group_add_radio_actions(action_group, rotation_entries, G_N_ELEMENTS(rotation_entries), 
            nds_screen.rotation_angle, G_CALLBACK(SetRotation), NULL);


    if (config.window_scale < WINSIZE_SCALE || config.window_scale > WINSIZE_5) {
    	config.window_scale = WINSIZE_SCALE;
    }
    winsize_current = (winsize_enum)config.window_scale.get();
    gtk_action_group_add_radio_actions(action_group, winsize_entries, G_N_ELEMENTS(winsize_entries), 
            winsize_current, G_CALLBACK(SetWinsize), NULL);

    if (config.view_orient < ORIENT_VERTICAL || config.view_orient > ORIENT_SINGLE) {
        config.view_orient = ORIENT_VERTICAL;
    }
    nds_screen.orientation = (orientation_enum)config.view_orient.get();
    gtk_action_group_add_radio_actions(action_group, orientation_entries, G_N_ELEMENTS(orientation_entries), 
            nds_screen.orientation, G_CALLBACK(SetOrientation), NULL);

    {
        GList * list = gtk_action_group_list_actions(action_group);
        g_list_foreach(list, dui_set_accel_group, accel_group);
    }
    gtk_window_add_accel_group(GTK_WINDOW(pWindow), accel_group);
    gtk_action_set_sensitive(gtk_action_group_get_action(action_group, "pause"), FALSE);
    gtk_action_set_sensitive(gtk_action_group_get_action(action_group, "run"), FALSE);
    gtk_action_set_sensitive(gtk_action_group_get_action(action_group, "reset"), FALSE);
    gtk_action_set_sensitive(gtk_action_group_get_action(action_group, "printscreen"), FALSE);
    gtk_action_set_sensitive(gtk_action_group_get_action(action_group, "cheatlist"), FALSE);
    gtk_action_set_sensitive(gtk_action_group_get_action(action_group, "cheatsearch"), FALSE);

    nds_screen.gap_size = config.view_gap ? GAP_SIZE : 0;
    gtk_toggle_action_set_active((GtkToggleAction*)gtk_action_group_get_action(action_group, "gap"), config.view_gap);

    nds_screen.swap = config.view_swap;
    gtk_toggle_action_set_active((GtkToggleAction*)gtk_action_group_get_action(action_group, "orient_swapscreens"), config.view_swap);

    gtk_ui_manager_insert_action_group (ui_manager, action_group, 0);
    
    accel_group = gtk_ui_manager_get_accel_group (ui_manager);
    gtk_window_add_accel_group (GTK_WINDOW (pWindow), accel_group);

    desmume_try_adding_ui(ui_manager, ui_description);

    pMenuBar = gtk_ui_manager_get_widget (ui_manager, "/MainMenu");
    gtk_box_pack_start (GTK_BOX (pVBox), pMenuBar, FALSE, FALSE, 0);
    pToolBar = gtk_ui_manager_get_widget (ui_manager, "/ToolBar");
    gtk_box_pack_start (GTK_BOX(pVBox), pToolBar, FALSE, FALSE, 0);

    {
        GtkWidget * recentMenu = gtk_ui_manager_get_widget (ui_manager, "/MainMenu/FileMenu/RecentMenu");
        GtkWidget * recentFiles = gtk_recent_chooser_menu_new();
        GtkRecentFilter * recentFilter = gtk_recent_filter_new();
        gtk_recent_filter_add_mime_type(recentFilter, "application/x-nintendo-ds-rom");
        gtk_recent_chooser_set_filter(GTK_RECENT_CHOOSER(recentFiles), recentFilter);
        gtk_menu_item_set_submenu(GTK_MENU_ITEM(recentMenu), recentFiles);
        g_signal_connect(G_OBJECT(recentFiles), "item-activated", G_CALLBACK(OpenRecent), NULL);
    }

    /* Creating the place for showing DS screens */
    pDrawingArea = gtk_drawing_area_new();
    gtk_container_add (GTK_CONTAINER (pVBox), pDrawingArea);

    gtk_widget_set_events(pDrawingArea,
                          GDK_EXPOSURE_MASK | GDK_LEAVE_NOTIFY_MASK |
                          GDK_BUTTON_PRESS_MASK | GDK_BUTTON_RELEASE_MASK |
                          GDK_POINTER_MOTION_MASK | GDK_KEY_PRESS_MASK );

    g_signal_connect(G_OBJECT(pDrawingArea), "button_press_event",
                     G_CALLBACK(Stylus_Press), NULL);
    g_signal_connect(G_OBJECT(pDrawingArea), "button_release_event",
                     G_CALLBACK(Stylus_Release), NULL);
    g_signal_connect(G_OBJECT(pDrawingArea), "motion_notify_event",
                     G_CALLBACK(Stylus_Move), NULL);
    g_signal_connect(G_OBJECT(pDrawingArea), "expose_event",
                     G_CALLBACK(ExposeDrawingArea), NULL ) ;
    g_signal_connect(G_OBJECT(pDrawingArea), "configure_event",
                     G_CALLBACK(ConfigureDrawingArea), NULL ) ;

    /* Status bar */
    pStatusBar = gtk_statusbar_new();
    UpdateStatusBar(EMU_DESMUME_NAME_AND_VERSION());
    gtk_box_pack_end(GTK_BOX(pVBox), pStatusBar, FALSE, FALSE, 0);

    gtk_widget_show_all(pWindow);

	if (winsize_current == WINSIZE_SCALE) {
		if (config.window_fullscreen) {
			gtk_widget_hide(pMenuBar);
			gtk_widget_hide(pToolBar);
			gtk_widget_hide(pStatusBar);
			gtk_action_set_sensitive(gtk_action_group_get_action(action_group, "view_menu"), FALSE);
			gtk_action_set_sensitive(gtk_action_group_get_action(action_group, "view_toolbar"), FALSE);
			gtk_action_set_sensitive(gtk_action_group_get_action(action_group, "view_statusbar"), FALSE);
			gtk_window_fullscreen(GTK_WINDOW(pWindow));
		}
	} else {
		config.window_fullscreen = false;
		gtk_action_set_sensitive(gtk_action_group_get_action(action_group, "fullscreen"), FALSE);
	}
	if (!config.view_menu) {
		gtk_widget_hide(pMenuBar);
		gtk_toggle_action_set_active((GtkToggleAction*)gtk_action_group_get_action(action_group, "view_menu"), FALSE);
	}
	if (!config.view_toolbar) {
		gtk_widget_hide(pToolBar);
		gtk_toggle_action_set_active((GtkToggleAction*)gtk_action_group_get_action(action_group, "view_toolbar"), FALSE);
	}
	if (!config.view_statusbar) {
		gtk_widget_hide(pStatusBar);
		gtk_toggle_action_set_active((GtkToggleAction*)gtk_action_group_get_action(action_group, "view_statusbar"), FALSE);
	}
    UpdateDrawingAreaAspect();

    if (my_config->disable_limiter || !config.fpslimiter) {
        config.fpslimiter = false;
        GtkAction *action = gtk_action_group_get_action(action_group, "enablefpslimiter");
        if (action)
            gtk_toggle_action_set_active((GtkToggleAction *)action, FALSE);
    }

    OGLLoadEntryPoints_3_2_Func = OGLLoadEntryPoints_3_2;
    OGLCreateRenderer_3_2_Func = OGLCreateRenderer_3_2;

    //Set the 3D emulation to use
    unsigned core = my_config->engine_3d;
    // setup the gdk 3D emulation;
#if defined(HAVE_LIBOSMESA) || defined(HAVE_GL_GLX)
    if(my_config->engine_3d == 2)
    {
#if defined(HAVE_LIBOSMESA)
        core = init_osmesa_3Demu()
#elif defined(HAVE_GL_GLX)
        core = init_glx_3Demu()
#endif
        ? 2 : GPU3D_NULL;
    }
#endif
    NDS_3D_ChangeCore(core);
    if(my_config->engine_3d != 0 && gpu3D == &gpu3DNull) {
        g_printerr("Failed to initialise openGL 3D emulation; "
                   "removing 3D support\n");
    }

    backup_setManualBackupType(my_config->savetype);

    // Command line arg 
    if( my_config->nds_file != "") {
        if(Open( my_config->nds_file.c_str()) >= 0) {
            my_config->process_movieCommands();

            if(my_config->load_slot != -1){
				int todo = my_config->load_slot;
				if(todo == 10)
					todo = 0;
              loadstate_slot(my_config->load_slot);
            }

            Launch();
        } else {
            GtkWidget *pDialog = gtk_message_dialog_new(GTK_WINDOW(pWindow),
                    GTK_DIALOG_MODAL,
                    GTK_MESSAGE_INFO,
                    GTK_BUTTONS_OK,
                    "Unable to load :\n%s", my_config->nds_file.c_str());
            gtk_dialog_run(GTK_DIALOG(pDialog));
            gtk_widget_destroy(pDialog);
        }
    }

    if (my_config->timeout > 0) {
        g_timeout_add_seconds(my_config->timeout, timeout_exit_cb, GINT_TO_POINTER(my_config->timeout));
    }

    /* Video filter parameters */
    video->SetFilterParameteri(VF_PARAM_SCANLINE_A, _scanline_filter_a);
    video->SetFilterParameteri(VF_PARAM_SCANLINE_B, _scanline_filter_b);
    video->SetFilterParameteri(VF_PARAM_SCANLINE_C, _scanline_filter_c);
    video->SetFilterParameteri(VF_PARAM_SCANLINE_D, _scanline_filter_d);

	RedrawScreen();
    /* Main loop */
    gtk_main();

    delete video;

	config.save();
	avout_x264.end();
	avout_flac.end();

    desmume_free();

#if defined(HAVE_LIBOSMESA)
    deinit_osmesa_3Demu();
#elif defined(HAVE_GL_GLX)
    deinit_glx_3Demu();
#endif

    /* Unload joystick */
    uninit_joy();

    SDL_Quit();

    desmume_config_dispose(keyfile);

#ifdef GDB_STUB
    destroyStub_gdb( arm9_gdb_stub);
	arm9_gdb_stub = NULL;
	
    destroyStub_gdb( arm7_gdb_stub);
	arm7_gdb_stub = NULL;

    gdbstub_mutex_destroy();
#endif

    return EXIT_SUCCESS;
}


int main (int argc, char *argv[])
{
  configured_features my_config;

  // The global menu screws up the window size...
  unsetenv("UBUNTU_MENUPROXY");

  init_configured_features( &my_config);

  if (!g_thread_supported())
    g_thread_init( NULL);

  gtk_init(&argc, &argv);

  if ( !fill_configured_features( &my_config, argc, argv)) {
    exit(0);
  }

  return common_gtk_main( &my_config);
}

#ifdef WIN32
int WinMain (HINSTANCE hThisInstance, HINSTANCE hPrevInstance, LPSTR lpszArgument, int nFunsterStil)
{
  int argc = 0;
  char *argv[] = NULL;

  /*
   * FIXME:
   * Emulate the argc and argv main parameters. Could do this using
   * CommandLineToArgvW and then convert the wide chars to thin chars.
   * Or parse the wide chars directly and call common_gtk_main with a
   * filled configuration structure.
   */
  main( argc, argv);
}
#endif

