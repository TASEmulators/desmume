/* main.cpp - this file is part of DeSmuME
 *
 * Copyright (C) 2006-2009 DeSmuME Team
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

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>
#include <SDL.h>

#include "types.h"
#include "firmware.h"
#include "armcpu.h"
#include "NDSSystem.h"
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

#include "commandline.h"

#include "slot2.h"

#include "filter/videofilter.h"

#ifdef GDB_STUB
#include "gdbstub.h"
#endif

#if defined(HAVE_LIBOSMESA) || defined(HAVE_GL_GLX)
#include <GL/gl.h>
#include <GL/glu.h>
#include "OGLRender.h"
#include "osmesa_3Demu.h"
#include "glx_3Demu.h"
#endif

#include "DeSmuME.xpm"

#undef GPOINTER_TO_INT
#define GPOINTER_TO_INT(p) ((gint)  (glong) (p))

#undef GPOINTER_TO_UINT
#define GPOINTER_TO_UINT(p) ((guint)  (glong) (p))

#define EMULOOP_PRIO (G_PRIORITY_HIGH_IDLE + 20)

#define SCREENS_PIXEL_SIZE (256*192*2)
#define SCREEN_BYTES_PER_PIXEL 3
#define GAP_SIZE 50

#define FPS_LIMITER_FRAME_PERIOD 8
static SDL_sem *fps_limiter_semaphore;
static int gtk_fps_limiter_disabled;
extern int _scanline_filter_a, _scanline_filter_b, _scanline_filter_c, _scanline_filter_d;
VideoFilter video(256, 384, VideoFilterTypeID_HQ2XS, 4);

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

static void DoQuit();
static void RecordMovieDialog();
static void PlayMovieDialog();
static void StopMovie();
static void OpenNdsDialog();
static void SaveStateDialog();
static void LoadStateDialog();
void Launch();
void Pause();
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
static void ToggleSwapScreens(GtkToggleAction *action);
static void ToggleGap (GtkToggleAction *action);
static void SetRotation(GtkAction *action, GtkRadioAction *current);
static void SetOrientation(GtkAction *action, GtkRadioAction *current);
static void ToggleLayerVisibility(GtkToggleAction* action, gpointer data);
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
"      <menuitem action='printscreen'/>"
"      <separator/>"
"      <menuitem action='quit'/>"
"    </menu>"
"    <menu action='EmulationMenu'>"
"      <menuitem action='run'/>"
"      <menuitem action='pause'/>"
"      <menuitem action='reset'/>"
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
"      <menu action='FrameskipMenu'>"
"        <menuitem action='frameskipA'/>"
"        <menuitem action='frameskip0'/>"
"        <menuitem action='frameskip1'/>"
"        <menuitem action='frameskip2'/>"
"        <menuitem action='frameskip3'/>"
"      </menu>"
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
"      <menu action='CheatMenu'>"
"        <menuitem action='cheatsearch'/>"
"        <menuitem action='cheatlist'/>"
"      </menu>"
"    </menu>"
"    <menu action='ConfigMenu'>"
"      <menu action='ConfigSaveMenu'>"
"        <menuitem action='save_t0'/>"
"        <menuitem action='save_t1'/>"
"        <menuitem action='save_t2'/>"
"        <menuitem action='save_t3'/>"
"        <menuitem action='save_t4'/>"
"        <menuitem action='save_t5'/>"
"        <menuitem action='save_t6'/>"
"      </menu>"
"      <menu action='RotationMenu'>"
"        <menuitem action='rotate_0'/>"
"        <menuitem action='rotate_90'/>"
"        <menuitem action='rotate_180'/>"
"        <menuitem action='rotate_270'/>"
"      </menu>"
"      <menu action='OrientationMenu'>"
"        <menuitem action='orient_vertical'/>"
"        <menuitem action='orient_horizontal'/>"
"        <menuitem action='orient_single'/>"
"        <separator/>"
"        <menuitem action='orient_swapscreens'/>"
"      </menu>"
"      <menu action='PriInterpolationMenu'>"
"        <menuitem action='pri_interp_none'/>"
"        <menuitem action='pri_interp_nearest2x'/>"
"        <menuitem action='pri_interp_lq2x'/>"
"        <menuitem action='pri_interp_lq2xs'/>"
"        <menuitem action='pri_interp_hq2x'/>"
"        <menuitem action='pri_interp_hq4x'/>"
"        <menuitem action='pri_interp_hq2xs'/>"
"        <menuitem action='pri_interp_2xsai'/>"
"        <menuitem action='pri_interp_super2xsai'/>"
"        <menuitem action='pri_interp_supereagle'/>"
"        <menuitem action='pri_interp_scanline'/>"
"        <menuitem action='pri_interp_bilinear'/>"
"        <menuitem action='pri_interp_epx'/>"
"        <menuitem action='pri_interp_epxplus'/>"
"        <menuitem action='pri_interp_epx_1point5x'/>"
"        <menuitem action='pri_interp_epxplus_1point5x'/>"
"        <menuitem action='pri_interp_nearest_1point5x'/>"
"        <menuitem action='pri_interp_nearestplus_1point5x'/>"
"      </menu>"
"      <menu action='InterpolationMenu'>"
"        <menuitem action='interp_nearest'/>"
"        <menuitem action='interp_tiles'/>"
"        <menuitem action='interp_bilinear'/>"
"        <menuitem action='interp_hyper'/>"
"      </menu>"
"      <menuitem action='gap'/>"
"      <menuitem action='editctrls'/>"
"      <menuitem action='editjoyctrls'/>"
"      <menu action='ViewMenu'>"
"        <menuitem action='view_menu'/>"
"        <menuitem action='view_toolbar'/>"
"        <menuitem action='view_statusbar'/>"
"        <menuitem action='fullscreen'/>"
"      </menu>"
"    </menu>"
"    <menu action='ToolsMenu'>"
"      <menuitem action='ioregs'/>"
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
      { "savestateto",    NULL,         "Save state _to ...",         NULL,  NULL,   SaveStateDialog },
      { "loadstatefrom",  NULL,         "Load state _from ...",         NULL,  NULL,   LoadStateDialog },
      { "recordmovie",  NULL,         "Record movie _to ...",         NULL,  NULL,   RecordMovieDialog },
      { "playmovie",  NULL,         "Play movie _from ...",         NULL,  NULL,   PlayMovieDialog },
      { "stopmovie",  NULL,         "Stop movie", NULL,  NULL,   StopMovie },
      { "SavestateMenu", NULL, "_Save state" },
      { "LoadstateMenu", NULL, "_Load state" },
#ifdef DESMUME_GTK_FIRMWARE_BROKEN
      { "loadfirmware","gtk-open",        "Load Firm_ware file", "<Ctrl>l",  NULL, SelectFirmwareFile },
#endif
      { "printscreen","gtk-media-record", "Take a _screenshot",    "<Ctrl>s",  NULL,   Printscreen },
      { "quit",       "gtk-quit",         "_Quit",         "<Ctrl>q",  NULL,   DoQuit },

    { "EmulationMenu", NULL, "_Emulation" },
      { "run",        "gtk-media-play",   "_Run",          "<Ctrl>r",  NULL,   Launch },
      { "pause",      "gtk-media-pause",  "_Pause",        "<Ctrl>p",  NULL,   Pause },
      { "reset",      "gtk-refresh",      "Re_set",        NULL,       NULL,   Reset },
      { "SPUModeMenu", NULL, "_SPU Mode" },
      { "FrameskipMenu", NULL, "_Frameskip" },
      { "LayersMenu", NULL, "_Layers" },
      { "CheatMenu", NULL, "_Cheat" },
        { "cheatsearch",     NULL,      "_Search",        NULL,       NULL,   CheatSearch },
        { "cheatlist",       NULL,      "_List",        NULL,       NULL,   CheatList },

    { "ConfigMenu", NULL, "_Config" },
      { "ConfigSaveMenu", NULL, "_Saves" },
      { "editctrls",  NULL,        "_Edit controls",NULL,    NULL,   Edit_Controls },
      { "editjoyctrls",  NULL,     "Edit _Joystick controls",NULL,       NULL,   Edit_Joystick_Controls },
      { "RotationMenu", NULL, "_Rotation" },
      { "OrientationMenu", NULL, "_Orientation" },
      { "PriInterpolationMenu", NULL, "Primary _Interpolation" },
      { "InterpolationMenu", NULL, "Secondary _Interpolation" },
      { "ViewMenu", NULL, "_View" },

    { "ToolsMenu", NULL, "_Tools" },

    { "HelpMenu", NULL, "_Help" },
      { "about",      "gtk-about",        "_About",        NULL,       NULL,   About }
};

static const GtkToggleActionEntry toggle_entries[] = {
    { "enableaudio", NULL, "_Enable audio", NULL, NULL, G_CALLBACK(ToggleAudio), TRUE},
#ifdef FAKE_MIC
    { "micnoise", NULL, "Fake mic _noise", NULL, NULL, G_CALLBACK(ToggleMicNoise), FALSE},
#endif
    { "gap", NULL, "_Gap", NULL, NULL, G_CALLBACK(ToggleGap), FALSE},
    { "view_menu", NULL, "View _menu", NULL, NULL, G_CALLBACK(ToggleMenuVisible), TRUE},
    { "view_toolbar", NULL, "View _toolbar", NULL, NULL, G_CALLBACK(ToggleToolbarVisible), TRUE},
    { "view_statusbar", NULL, "View _statusbar", NULL, NULL, G_CALLBACK(ToggleStatusbarVisible), TRUE},
    { "orient_swapscreens", NULL, "S_wap screens", "space", NULL, G_CALLBACK(ToggleSwapScreens), FALSE},
    { "fullscreen", NULL, "Fullscreen", "<Ctrl>f", NULL, G_CALLBACK(ToggleFullscreen), FALSE},
};

static const GtkRadioActionEntry pri_interpolation_entries[] = {
    { "pri_interp_none", NULL, VideoFilterAttributesList[VideoFilterTypeID_None].typeString, NULL, NULL, VideoFilterTypeID_None},
    { "pri_interp_nearest2x", NULL, VideoFilterAttributesList[VideoFilterTypeID_Nearest2X].typeString, NULL, NULL, VideoFilterTypeID_Nearest2X},
    { "pri_interp_lq2x", NULL, VideoFilterAttributesList[VideoFilterTypeID_LQ2X].typeString, NULL, NULL, VideoFilterTypeID_LQ2X},
    { "pri_interp_lq2xs", NULL, VideoFilterAttributesList[VideoFilterTypeID_LQ2XS].typeString, NULL, NULL, VideoFilterTypeID_LQ2XS},
    { "pri_interp_hq2x", NULL, VideoFilterAttributesList[VideoFilterTypeID_HQ2X].typeString, NULL, NULL, VideoFilterTypeID_HQ2X},
    { "pri_interp_hq4x", NULL, VideoFilterAttributesList[VideoFilterTypeID_HQ4X].typeString, NULL, NULL, VideoFilterTypeID_HQ4X},
    { "pri_interp_hq2xs", NULL, VideoFilterAttributesList[VideoFilterTypeID_HQ2XS].typeString, NULL, NULL, VideoFilterTypeID_HQ2XS},
    { "pri_interp_2xsai", NULL, VideoFilterAttributesList[VideoFilterTypeID_2xSaI].typeString, NULL, NULL, VideoFilterTypeID_2xSaI},
    { "pri_interp_super2xsai", NULL, VideoFilterAttributesList[VideoFilterTypeID_Super2xSaI].typeString, NULL, NULL, VideoFilterTypeID_Super2xSaI},
    { "pri_interp_supereagle", NULL, VideoFilterAttributesList[VideoFilterTypeID_SuperEagle].typeString, NULL, NULL, VideoFilterTypeID_SuperEagle},
    { "pri_interp_scanline", NULL, VideoFilterAttributesList[VideoFilterTypeID_Scanline].typeString, NULL, NULL, VideoFilterTypeID_Scanline},
    { "pri_interp_bilinear", NULL, VideoFilterAttributesList[VideoFilterTypeID_Bilinear].typeString, NULL, NULL, VideoFilterTypeID_Bilinear},
    { "pri_interp_epx", NULL, VideoFilterAttributesList[VideoFilterTypeID_EPX].typeString, NULL, NULL, VideoFilterTypeID_EPX},
    { "pri_interp_epxplus", NULL, VideoFilterAttributesList[VideoFilterTypeID_EPXPlus].typeString, NULL, NULL, VideoFilterTypeID_EPXPlus},
    { "pri_interp_epx_1point5x", NULL, VideoFilterAttributesList[VideoFilterTypeID_EPX1_5X].typeString, NULL, NULL, VideoFilterTypeID_EPX1_5X},
    { "pri_interp_epxplus_1point5x", NULL, VideoFilterAttributesList[VideoFilterTypeID_EPXPlus1_5X].typeString, NULL, NULL, VideoFilterTypeID_EPXPlus1_5X},
    { "pri_interp_nearest_1point5x", NULL, VideoFilterAttributesList[VideoFilterTypeID_Nearest1_5X].typeString, NULL, NULL, VideoFilterTypeID_Nearest1_5X},
    { "pri_interp_nearestplus_1point5x", NULL, VideoFilterAttributesList[VideoFilterTypeID_NearestPlus1_5X].typeString, NULL, NULL, VideoFilterTypeID_NearestPlus1_5X},
};

static const GtkRadioActionEntry interpolation_entries[] = {
    { "interp_nearest", NULL, "_Nearest", NULL, NULL, GDK_INTERP_NEAREST},
    { "interp_tiles", NULL, "_Tiles", NULL, NULL, GDK_INTERP_TILES},
    { "interp_bilinear", NULL, "_Bilinear", NULL, NULL, GDK_INTERP_BILINEAR},
    { "interp_hyper", NULL, "_Hyper", NULL, NULL, GDK_INTERP_HYPER},
};

static const GtkRadioActionEntry rotation_entries[] = {
    { "rotate_0",   "gtk-orientation-portrait",          "_0",  NULL, NULL, 0 },
    { "rotate_90",  "gtk-orientation-landscape",         "_90", NULL, NULL, 90 },
    { "rotate_180", "gtk-orientation-reverse-portrait",  "_180",NULL, NULL, 180 },
    { "rotate_270", "gtk-orientation-reverse-landscape", "_270",NULL, NULL, 270 },
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

enum frameskip_enum {
    FRAMESKIP_0 = 0,
    FRAMESKIP_1 = 1,
    FRAMESKIP_2 = 2,
    FRAMESKIP_3 = 3,
    FRAMESKIP_AUTO
};

static const GtkRadioActionEntry frameskip_entries[] = {
    { "frameskipA", NULL, "_Auto", NULL, NULL, FRAMESKIP_AUTO},
    { "frameskip0", NULL, "_0", NULL, NULL, FRAMESKIP_0},
    { "frameskip1", NULL, "_1", NULL, NULL, FRAMESKIP_1},
    { "frameskip2", NULL, "_2", NULL, NULL, FRAMESKIP_2},
    { "frameskip3", NULL, "_3", NULL, NULL, FRAMESKIP_3},
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
  &gpu3DRasterize
#if defined(HAVE_LIBOSMESA) || defined(HAVE_GL_GLX)
  ,
  &gpu3Dgl
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
    GDK_o          // BOOST
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
    { "timeout", 0, 0, G_OPTION_ARG_INT, &config->timeout, "Quit desmume after the specified seconds for testing purpose.", "SECONDS"},
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
bool autoframeskip = false;
GdkInterpType Interpolation = GDK_INTERP_NEAREST;

static GtkWidget *pWindow;
static GtkWidget *pStatusBar;
static GtkWidget *pDrawingArea;
static GtkActionGroup * action_group;
static GtkUIManager *ui_manager;

struct nds_screen_t {
    guint gap_size;
    gint rotation_angle;
    guint orientation;
    gint touch_x;
    gint touch_y;
    gint touch_width;
    gint touch_height;
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

    gtk_show_about_dialog(GTK_WINDOW(pWindow),
            "name", "DeSmuME",
            "version", VERSION,
            "website", "http://desmume.org",
            "logo", pixbuf,
            "comments", "Nintendo DS emulator based on work by Yopyop",
            NULL);

    g_object_unref(pixbuf);
}

static void ToggleMenuVisible(GtkToggleAction *action)
{
    GtkWidget *pMenuBar = gtk_ui_manager_get_widget (ui_manager, "/MainMenu");

    if (gtk_toggle_action_get_active(action) == TRUE)
      gtk_widget_show(pMenuBar);
    else
      gtk_widget_hide(pMenuBar);
}

static void ToggleToolbarVisible(GtkToggleAction *action)
{
    GtkWidget *pToolBar = gtk_ui_manager_get_widget (ui_manager, "/ToolBar");

    if (gtk_toggle_action_get_active(action) == TRUE)
      gtk_widget_show(pToolBar);
    else
      gtk_widget_hide(pToolBar);
}

static void ToggleStatusbarVisible(GtkToggleAction *action)
{
    if (gtk_toggle_action_get_active(action) == TRUE)
      gtk_widget_show(pStatusBar);
    else
      gtk_widget_hide(pStatusBar);
}

static void ToggleFullscreen(GtkToggleAction *action)
{
  GtkWidget *pMenuBar = gtk_ui_manager_get_widget (ui_manager, "/MainMenu");
  GtkWidget *pToolBar = gtk_ui_manager_get_widget (ui_manager, "/ToolBar");
  if (gtk_toggle_action_get_active(action) == TRUE)
  {
    gtk_widget_hide(pMenuBar);
    gtk_widget_hide(pToolBar);
    gtk_widget_hide(pStatusBar);
    gtk_toggle_action_set_active((GtkToggleAction*)gtk_action_group_get_action(action_group, "view_menu"), FALSE);
    gtk_toggle_action_set_active((GtkToggleAction*)gtk_action_group_get_action(action_group, "view_toolbar"), FALSE);
    gtk_toggle_action_set_active((GtkToggleAction*)gtk_action_group_get_action(action_group, "view_statusbar"), FALSE);
    gtk_window_fullscreen(GTK_WINDOW(pWindow));
  }
  else
  {
    gtk_widget_show(pMenuBar);
    gtk_widget_show(pToolBar);
    gtk_widget_show(pStatusBar);
    gtk_toggle_action_set_active((GtkToggleAction*)gtk_action_group_get_action(action_group, "view_menu"), TRUE);
    gtk_toggle_action_set_active((GtkToggleAction*)gtk_action_group_get_action(action_group, "view_toolbar"), TRUE);
    gtk_toggle_action_set_active((GtkToggleAction*)gtk_action_group_get_action(action_group, "view_statusbar"), TRUE);
    gtk_window_unfullscreen(GTK_WINDOW(pWindow));
  }
}


static int Open(const char *filename)
{
    int res;
    res = NDS_LoadROM( filename );
    if(res > 0)
        gtk_action_set_sensitive(gtk_action_group_get_action(action_group, "cheatlist"), TRUE);
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
    NDS_Reset();
    Launch();
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
        if ((nds_screen.rotation_angle == 0 || nds_screen.rotation_angle == 180) ^
        (nds_screen.orientation == ORIENT_HORIZONTAL)) {
            H += nds_screen.gap_size;
        } else {
            W += nds_screen.gap_size;
        }
    }

    video.SetSourceSize(W, H);
    gtk_widget_set_size_request(GTK_WIDGET(pDrawingArea), W, H);
}

static void ToggleGap(GtkToggleAction* action)
{
    nds_screen.gap_size = gtk_toggle_action_get_active(action) ? GAP_SIZE : 0;
    UpdateDrawingAreaAspect();
}

static void SetRotation(GtkAction *action, GtkRadioAction *current)
{
    nds_screen.rotation_angle = gtk_radio_action_get_current_value(current);
    UpdateDrawingAreaAspect();
}

static void SetOrientation(GtkAction *action, GtkRadioAction *current)
{
    nds_screen.orientation = gtk_radio_action_get_current_value(current);
    UpdateDrawingAreaAspect();
}

static void ToggleSwapScreens(GtkToggleAction *action) {
    nds_screen.swap = gtk_toggle_action_get_active(action);
}

static int ConfigureDrawingArea(GtkWidget *widget, GdkEventConfigure *event, gpointer data)
{
    return TRUE;
}


static inline void gpu_screen_to_rgb(guchar * rgb, int size, int pixelsize)
{
    gint rot = nds_screen.rotation_angle;
    gint height, width;
    u16 gpu_pixel;
    u32 offset;

    width = screen_size[nds_screen.orientation].width;
    height = screen_size[nds_screen.orientation].height;

    for (gint i = 0; i < width; i++) {
        for (gint j = 0; j < height; j++) {
            gint row = j, col = i;

            if (i >= 256) {
                col = i - 256;
                row = j + 192;
            }
            if (nds_screen.swap)
                 row = (row + 192) % 384;

            gpu_pixel = *((u16 *) & GPU_screen[(col + row * 256) << 1]);

            if (rot == 0 || rot == 180)
                offset = i * pixelsize + j * pixelsize * width;
            else
                offset = j * pixelsize + (width - i - 1) * pixelsize * height;

            if (rot == 90 || rot == 180)
                offset = size - offset - pixelsize;
              
            *(rgb + offset + 0) = ((gpu_pixel >> 0) & 0x1f) << 3;
            *(rgb + offset + 1) = ((gpu_pixel >> 5) & 0x1f) << 3;
            *(rgb + offset + 2) = ((gpu_pixel >> 10) & 0x1f) << 3;
        }
    }
}

/* Drawing callback */
static gboolean ExposeDrawingArea (GtkWidget *widget, GdkEventExpose *event, gpointer data)
{
    GdkPixbuf *resizedPixbuf, *drawPixbuf;
    cairo_t *cr;
    GdkWindow *window;

    gfloat vratio, hratio, nscreen_ratio;
    gint daW, daH, imgW, imgH, screenW, screenH, gapW, gapH;
    gint primaryOffsetX, primaryOffsetY, secondaryOffsetX, secondaryOffsetY;
    const gboolean gap_vertical = ((nds_screen.orientation == ORIENT_VERTICAL) ^ (nds_screen.rotation_angle == 90 || nds_screen.rotation_angle == 270));
  
    window = gtk_widget_get_window(GTK_WIDGET(pDrawingArea));
#if GTK_CHECK_VERSION(2,24,0)
    daW = gdk_window_get_width(window);
    daH = gdk_window_get_height(window);
#else
    gdk_drawable_get_size(window, &daW, &daH);
#endif

    if (nds_screen.rotation_angle == 0 || nds_screen.rotation_angle == 180) {
        imgW = screen_size[nds_screen.orientation].width;
        imgH = screen_size[nds_screen.orientation].height;
    } else {
        imgH = screen_size[nds_screen.orientation].width;
        imgW = screen_size[nds_screen.orientation].height;
    }

    if (nds_screen.orientation == ORIENT_SINGLE) {
        gapH = 0;
        gapW = 0;
    } else if (gap_vertical) {
        gapH = nds_screen.gap_size;
        gapW = 0;
    } else {
        gapH = 0;
        gapW = nds_screen.gap_size;
    }

    hratio = (float)(daW - gapW) / (float)imgW;
    vratio = (float)(daH - gapH) / (float)imgH;
    hratio = MIN(hratio, vratio);
    vratio = hratio;

    primaryOffsetX = (daW-(int)(hratio*(float)imgW)-gapW)/2;
    primaryOffsetY = (daH-(int)(vratio*(float)imgH)-gapH)/2;

    nscreen_ratio = nds_screen.orientation == ORIENT_SINGLE ? 1 : 0.5;
    if (gap_vertical) {
        screenW = (int)(hratio*(float)imgW);
        screenH = (int)(vratio*(float)imgH*nscreen_ratio);
        secondaryOffsetX = primaryOffsetX;
        secondaryOffsetY = primaryOffsetY + screenH + gapH;
    } else {
        screenW = (int)(hratio*(float)imgW*nscreen_ratio);
        screenH = (int)(vratio*(float)imgH);
        secondaryOffsetX = primaryOffsetX + screenW + gapW;
        secondaryOffsetY = primaryOffsetY;
    }

    if ((nds_screen.swap) ^
            ((nds_screen.orientation == ORIENT_VERTICAL && (nds_screen.rotation_angle == 90 || nds_screen.rotation_angle == 180)) ||
            (nds_screen.orientation == ORIENT_HORIZONTAL && (nds_screen.rotation_angle == 180 || nds_screen.rotation_angle == 270)))) {
        nds_screen.touch_x = primaryOffsetX;
        nds_screen.touch_y = primaryOffsetY;
    } else if (nds_screen.orientation != ORIENT_SINGLE) {
        nds_screen.touch_x = secondaryOffsetX;
        nds_screen.touch_y = secondaryOffsetY;
    } else {
        nds_screen.touch_x = -1;
        nds_screen.touch_y = -1;
    }
    nds_screen.touch_width = screenW;
    nds_screen.touch_height = screenH;

    osd->update();
    DrawHUD();

    gpu_screen_to_rgb((u8*)video.GetSrcBufferPtr(), imgW*imgH*4, 4);
    
    u32* fbuf = video.RunFilter();
    int dstW = video.GetDstWidth();
    int dstH = video.GetDstHeight();
    //Convert to 24-bit
    guchar dsurf24[dstW*dstH*SCREEN_BYTES_PER_PIXEL];
    gint k=0;
    for (gint i=0; i<dstW*dstH; i++)
    {
      *(u32*) &(dsurf24[k]) = fbuf[i];
      k+=3;
    }
    
    drawPixbuf = gdk_pixbuf_new_from_data(dsurf24, GDK_COLORSPACE_RGB, 
            FALSE, 8, dstW, dstH, dstW*SCREEN_BYTES_PER_PIXEL, NULL, NULL);

    resizedPixbuf = gdk_pixbuf_scale_simple(drawPixbuf, hratio*imgW, vratio*imgH,
                Interpolation);
    g_object_unref(drawPixbuf);
    drawPixbuf = resizedPixbuf;

    cr = gdk_cairo_create(widget->window);
    gdk_cairo_set_source_pixbuf(cr, drawPixbuf, 0, 0);

    if (nds_screen.orientation != ORIENT_SINGLE) {
        gdk_cairo_set_source_pixbuf(cr, drawPixbuf, primaryOffsetX, primaryOffsetY);
    }
    
    g_object_unref(drawPixbuf); //drawPixbuf was never unref'd, so its ref count stayed above 0 and it was never freed

    cairo_paint(cr);
    cairo_destroy(cr);

    return TRUE;
}

/////////////////////////////// KEYS AND STYLUS UPDATE ///////////////////////////////////////

static gboolean rotoscaled_touchpos(gint x, gint y, gboolean start)
{
    u16 EmuX, EmuY;
    gint X, Y;

    if (nds_screen.touch_x == -1 || nds_screen.touch_y == -1) {
        return FALSE;
    }

    if (nds_screen.rotation_angle == 0 || nds_screen.rotation_angle == 180) {
        X = (x - nds_screen.touch_x) * 256 / nds_screen.touch_width;
        Y = (y - nds_screen.touch_y) * 192 / nds_screen.touch_height;
    } else {
        X = (y - nds_screen.touch_y) * 256 / nds_screen.touch_height;
        Y = (x - nds_screen.touch_x) * 192 / nds_screen.touch_width;
    }

    if (nds_screen.rotation_angle == 180 || nds_screen.rotation_angle == 270) {
        X = 255 - X;
    }

    if (nds_screen.rotation_angle == 90 || nds_screen.rotation_angle == 180) {
        Y = 191 - Y;
    }

    LOG("X=%d, Y=%d\n",x,y);

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

        if(state & GDK_BUTTON1_MASK)
            rotoscaled_touchpos(x, y, FALSE);
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
                    gtk_ui_manager_get_widget (ui_manager, "/MainMenu/ConfigMenu/ViewMenu")));
        gtk_menu_popup(GTK_MENU(pMenu), NULL, NULL, NULL, NULL, 3, e->time);
    }

    if( !desmume_running()) 
        return TRUE;

    if (e->button == 1) {
        gdk_window_get_pointer(w->window, &x, &y, &state);

        if(state & GDK_BUTTON1_MASK)
            if (rotoscaled_touchpos(x, y, TRUE))
                click = TRUE;
    }

    return TRUE;
}
static gboolean Stylus_Release(GtkWidget *w, GdkEventButton *e, gpointer data)
{
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
          loadgame(e->keyval - GDK_F1 + 1);
      else
          savegame(e->keyval - GDK_F1 + 1);
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
    u8 *rgb;
    static int seq = 0;
    gint H, W;

    rgb = (u8 *) malloc(SCREENS_PIXEL_SIZE*SCREEN_BYTES_PER_PIXEL);
    if (!rgb)
        return;

    if (nds_screen.rotation_angle == 0 || nds_screen.rotation_angle == 180) {
        W = screen_size[nds_screen.orientation].width;
        H = screen_size[nds_screen.orientation].height;
    } else {
        W = screen_size[nds_screen.orientation].height;
        H = screen_size[nds_screen.orientation].width;
    }

    gpu_screen_to_rgb(rgb, W*H*SCREEN_BYTES_PER_PIXEL, 3);
    screenshot = gdk_pixbuf_new_from_data(rgb,
                          GDK_COLORSPACE_RGB,
                          FALSE,
                          8,
                          W,
                          H,
                          W*SCREEN_BYTES_PER_PIXEL,
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

    free(rgb);
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
    video.ChangeFilterByID((VideoFilterTypeID)filter);
}

static void Modify_Interpolation(GtkAction *action, GtkRadioAction *current)
{
    Interpolation = (GdkInterpType)gtk_radio_action_get_current_value(current);
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
}

static void Modify_Frameskip(GtkAction *action, GtkRadioAction *current)
{
    Frameskip = gtk_radio_action_get_current_value(current) ;
    autoframeskip = Frameskip == FRAMESKIP_AUTO;
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

class GtkDriver : public UnixDriver
{
public:
    virtual void EMU_DebugIdleUpdate()
    {
        usleep(1000);
        _updateDTools();
        while (gtk_events_pending())
            gtk_main_iteration();
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
    static int limiter_frame_counter;
    unsigned int i;
    gchar Title[20];

    if (!desmume_running()) {
      regMainLoop = FALSE;
      return FALSE;
    }

    /* If desmume is currently running */
    if (!fps_SecStart)
      fps_SecStart = SDL_GetTicks();

    /* don't count autoframeskip as real frameskip  */
    if (Frameskip == FRAMESKIP_AUTO)
      Frameskip = 0;

    fps_FrameCount += Frameskip + 1;
    next_fps_SecStart = SDL_GetTicks();
    if ((next_fps_SecStart - fps_SecStart) >= 1000) {
        fps_SecStart = next_fps_SecStart;

        snprintf(Title, sizeof(Title), "Desmume - %dfps", fps_FrameCount);
        gtk_window_set_title(GTK_WINDOW(pWindow), Title);

        float emu_ratio = fps_FrameCount / 60.0;
        if (autoframeskip) {
            if (emu_ratio < 1 && (fps_FrameCount - skipped_frames) > skipped_frames)
                Frameskip = MIN(Frameskip+1, FRAMESKIP_AUTO-1);
            if (emu_ratio > 1 || skipped_frames >= fps_FrameCount-Frameskip)
                Frameskip = Frameskip ? Frameskip-1 : 0;
        }
        LOG("auto: %d fps: %u skipped: %u emu_ratio: %f Frameskip: %u\n", autoframeskip, fps_FrameCount, skipped_frames, emu_ratio, Frameskip);
        fps_FrameCount = 0;
        skipped_frames = 0;
    }

    /* Merge the joystick keys with the keyboard ones */
    process_joystick_events(&keys_latch);
    /* Update! */
    update_keypad(keys_latch);

    desmume_cycle();    /* Emule ! */
    for (i = 0; i < Frameskip; i++) {
        NDS_SkipNextFrame();
        desmume_cycle();
        skipped_frames++;
    }

    _updateDTools();
    gtk_widget_queue_draw( pDrawingArea );
    osd->clear();

    if (!gtk_fps_limiter_disabled) {
        limiter_frame_counter += 1;
        if (limiter_frame_counter >= FPS_LIMITER_FRAME_PERIOD) {
            limiter_frame_counter = 0;
            /* wait for the timer to expire */
            SDL_SemWait( fps_limiter_semaphore);
        }
    }

    return TRUE;
}


/** 
 * A SDL timer callback function. Signals the supplied SDL semaphore
 * if its value is small.
 * 
 * @param interval The interval since the last call (in ms)
 * @param param The pointer to the semaphore.
 * 
 * @return The interval to the next call (required by SDL)
 */
static Uint32 fps_limiter_fn(Uint32 interval, void *param)
{
  SDL_sem *sdl_semaphore = (SDL_sem *)param;

  /* signal the semaphore if it is getting low */
  if ( SDL_SemValue( sdl_semaphore) < 4) {
    SDL_SemPost( sdl_semaphore);
  }

  return interval;
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

static void desmume_gtk_menu_file_saveload_slot (GtkActionGroup *ag)
{
    for(guint i = 1; i <= 10; i++){
        GtkAction *act;
        char label[64], name[64];

        snprintf(label, 60, "_%d", i);

        snprintf(name, 60, "savestate%d", i);
        act = gtk_action_new(name, label, NULL, NULL);
        g_signal_connect(G_OBJECT(act), "activate", G_CALLBACK(MenuSave), GUINT_TO_POINTER(i));
        gtk_action_group_add_action_with_accel(ag, GTK_ACTION(act), NULL);

        snprintf(name, 60, "loadstate%d", i);
        act = gtk_action_new(name, label, NULL, NULL);
        g_signal_connect(G_OBJECT(act), "activate", G_CALLBACK(MenuLoad), GUINT_TO_POINTER(i));
        gtk_action_group_add_action_with_accel(ag, GTK_ACTION(act), NULL);
    }
}

static void changesavetype(GtkAction *action, GtkRadioAction *current)
{
    backup_setManualBackupType( gtk_radio_action_get_current_value(current));
}

static void desmume_gtk_menu_emulation_layers (GtkActionGroup *ag)
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

static void ToggleAudio (GtkToggleAction *action)
{
    if (gtk_toggle_action_get_active(action) == TRUE) {
        SPU_ChangeSoundCore(SNDCORE_SDL, 735 * 4);
        osd->addLine("Audio enabled");
    } else {
        SPU_ChangeSoundCore(0, 0);
        osd->addLine("Audio disabled");
    }
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
}
#endif

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
    driver = new GtkDriver();

    SDL_TimerID limiter_timer = NULL;

    GtkAccelGroup * accel_group;
    GtkWidget *pVBox;
    GtkWidget *pMenuBar;
    GtkWidget *pToolBar;

#ifdef GDB_STUB
    gdbstub_handle_t arm9_gdb_stub;
    gdbstub_handle_t arm7_gdb_stub;
#endif
    struct armcpu_memory_iface *arm9_memio = &arm9_base_memory_iface;
    struct armcpu_memory_iface *arm7_memio = &arm7_base_memory_iface;
    struct armcpu_ctrl_iface *arm9_ctrl_iface;
    struct armcpu_ctrl_iface *arm7_ctrl_iface;

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
        strncpy(GBAgameName, my_config->gbaslot_rom.c_str(), MAX_PATH);
        // Check if the file exists and can be opened
        FILE * test = fopen(GBAgameName, "rb");
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

#ifdef GDB_STUB
    if ( my_config->arm9_gdb_port != 0) {
		arm9_gdb_stub = createStub_gdb( my_config->arm9_gdb_port,
                                          &arm9_memio,
                                          &arm9_base_memory_iface);

        if ( arm9_gdb_stub == NULL) {
            g_printerr("Failed to create ARM9 gdbstub on port %d\n",
                     my_config->arm9_gdb_port);
            exit( -1);
        }
    }
    if ( my_config->arm7_gdb_port != 0) {
         arm7_gdb_stub = createStub_gdb( my_config->arm7_gdb_port,
                                          &arm7_memio,
                                          &arm7_base_memory_iface);

        if ( arm7_gdb_stub == NULL) {
            g_printerr("Failed to create ARM7 gdbstub on port %d\n",
                     my_config->arm7_gdb_port);
            exit( -1);
        }
    }
#endif

    /* FIXME: SDL_INIT_VIDEO is needed for joystick support to work!?
     * Perhaps it needs a "window" to catch events...? */
    if(SDL_Init(SDL_INIT_TIMER|SDL_INIT_VIDEO) == -1) {
        g_printerr("Error trying to initialize SDL: %s\n",
                    SDL_GetError());
        return 1;
    }
    desmume_init( arm9_memio, &arm9_ctrl_iface,
                      arm7_memio, &arm7_ctrl_iface,
                      my_config->disable_sound);

    /* Init the hud / osd stuff */
#ifdef HAVE_LIBAGG
    Desmume_InitOnce();
    Hud.reset();
    aggDraw.hud->attach(GPU_screen, 256, 384, 512);
#endif

    /*
     * Activate the GDB stubs
     * This has to come after the NDS_Init (called in desmume_init)
     * where the cpus are set up.
     */
#ifdef GDB_STUB
    if ( my_config->arm9_gdb_port != 0) {
        activateStub_gdb( arm9_gdb_stub, arm9_ctrl_iface);
    }
    if ( my_config->arm7_gdb_port != 0) {
        activateStub_gdb( arm7_gdb_stub, arm7_ctrl_iface);
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

    /* Create the window */
    pWindow = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(pWindow), "Desmume");
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
    if (my_config->disable_sound) {
        GtkAction *action = gtk_action_group_get_action(action_group, "enableaudio");
        if (action)
            gtk_toggle_action_set_active((GtkToggleAction *)action, FALSE);
    }
    desmume_gtk_menu_emulation_layers(action_group);
    desmume_gtk_menu_file_saveload_slot(action_group);
    desmume_gtk_menu_tools(action_group);
    gtk_action_group_add_radio_actions(action_group, savet_entries, G_N_ELEMENTS(savet_entries), 
            my_config->savetype, G_CALLBACK(changesavetype), NULL);
    gtk_action_group_add_radio_actions(action_group, interpolation_entries, G_N_ELEMENTS(interpolation_entries), 
            GDK_INTERP_NEAREST, G_CALLBACK(Modify_Interpolation), NULL);
    gtk_action_group_add_radio_actions(action_group, pri_interpolation_entries, G_N_ELEMENTS(pri_interpolation_entries), 
            VideoFilterTypeID_HQ2XS, G_CALLBACK(Modify_PriInterpolation), NULL);
    gtk_action_group_add_radio_actions(action_group, spumode_entries, G_N_ELEMENTS(spumode_entries),
            0, G_CALLBACK(Modify_SPUMode), NULL);
    gtk_action_group_add_radio_actions(action_group, frameskip_entries, G_N_ELEMENTS(frameskip_entries), 
            0, G_CALLBACK(Modify_Frameskip), NULL);
    gtk_action_group_add_radio_actions(action_group, rotation_entries, G_N_ELEMENTS(rotation_entries), 
            0, G_CALLBACK(SetRotation), NULL);
    gtk_action_group_add_radio_actions(action_group, orientation_entries, G_N_ELEMENTS(orientation_entries), 
            0, G_CALLBACK(SetOrientation), NULL);
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

    gtk_widget_set_size_request(GTK_WIDGET(pDrawingArea), 256, 384);

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
    UpdateStatusBar("Desmume");
    gtk_box_pack_end(GTK_BOX(pVBox), pStatusBar, FALSE, FALSE, 0);

    gtk_widget_show_all(pWindow);

    gtk_fps_limiter_disabled = my_config->disable_limiter;
    if ( !gtk_fps_limiter_disabled) {
        /* create the semaphore used for fps limiting */
        fps_limiter_semaphore = SDL_CreateSemaphore( 1);

        /* start a SDL timer for every FPS_LIMITER_FRAME_PERIOD frames to keep us at 60 fps */
        limiter_timer = SDL_AddTimer( 16 * FPS_LIMITER_FRAME_PERIOD, fps_limiter_fn, fps_limiter_semaphore);
        if ( limiter_timer == NULL) {
            g_printerr("Error trying to start FPS limiter timer: %s\n",
                     SDL_GetError());
            return 1;
        }
    }

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
    video.SetFilterParameteri(VF_PARAM_SCANLINE_A, _scanline_filter_a);
    video.SetFilterParameteri(VF_PARAM_SCANLINE_B, _scanline_filter_b);
    video.SetFilterParameteri(VF_PARAM_SCANLINE_C, _scanline_filter_c);
    video.SetFilterParameteri(VF_PARAM_SCANLINE_D, _scanline_filter_d);

    /* Main loop */
    gtk_main();

    desmume_free();

#if defined(HAVE_LIBOSMESA)
    deinit_osmesa_3Demu();
#elif defined(HAVE_GL_GLX)
    deinit_glx_3Demu();
#endif

    if ( !gtk_fps_limiter_disabled) {
        /* tidy up the FPS limiter timer and semaphore */
        SDL_RemoveTimer( limiter_timer);
        SDL_DestroySemaphore( fps_limiter_semaphore);
    }

    /* Unload joystick */
    uninit_joy();

    SDL_Quit();

    desmume_config_dispose(keyfile);

#ifdef GDB_STUB
    if ( my_config->arm9_gdb_port != 0) {
        destroyStub_gdb( arm9_gdb_stub);
    }
    if ( my_config->arm7_gdb_port != 0) {
        destroyStub_gdb( arm7_gdb_stub);
    }
#endif

    return EXIT_SUCCESS;
}


int main (int argc, char *argv[])
{
  configured_features my_config;

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

