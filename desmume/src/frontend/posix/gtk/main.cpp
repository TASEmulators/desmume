 /*
	Copyright (C) 2007 Pascal Giard (evilynux)
	Copyright (C) 2006-2019 DeSmuME team
 
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

#ifndef GTK_UI
#define GTK_UI
#endif

#include "version.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <gtk/gtk.h>
#include <gio/gapplication.h>
#include <gdk/gdkkeysyms.h>
#include <SDL.h>
#include <X11/Xlib.h>
#include <vector>

#include "types.h"
#include "firmware.h"
#include "NDSSystem.h"
#include "driver.h"
#include "GPU.h"
#include "SPU.h"
#include "../shared/sndsdl.h"
#include "../shared/ctrlssdl.h"
#include "MMU.h"
#include "render3D.h"
#include "desmume.h"
#include "debug.h"
#include "rasterize.h"
#include "saves.h"
#include "mic.h"
#include "movie.h"
#include "dTool.h"
#include "../shared/desmume_config.h"
#include "cheatsGTK.h"
#include "frontend/modules/osd/agg/agg_osd.h"

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

#define HAVE_OPENGL

#ifdef HAVE_OPENGL
	#include <GL/gl.h>
	#include "OGLRender.h"
	#include "OGLRender_3_2.h"
#endif

#if defined(HAVE_LIBOSMESA)
	#include "osmesa_3Demu.h"
#else
	#include "sdl_3Demu.h"
#endif

#include "config.h"

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

#ifdef GDB_STUB
gdbstub_handle_t arm9_gdb_stub = NULL;
gdbstub_handle_t arm7_gdb_stub = NULL;
#endif

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
static void RecordAV_x264(GSimpleAction *action, GVariant *parameter, gpointer user_data);
static void RecordAV_flac(GSimpleAction *action, GVariant *parameter, gpointer user_data);
static void RecordAV_stop(GSimpleAction *action, GVariant *parameter, gpointer user_data);
static void RedrawScreen();

static void DoQuit(GSimpleAction *action, GVariant *parameter, gpointer user_data);
static void RecordMovieDialog(GSimpleAction *action, GVariant *parameter, gpointer user_data);
static void PlayMovieDialog(GSimpleAction *action, GVariant *parameter, gpointer user_data);
static void StopMovie(GSimpleAction *action, GVariant *parameter, gpointer user_data);
static void OpenNdsDialog(GSimpleAction *action, GVariant *parameter, gpointer user_data);
static void OpenRecent(GSimpleAction *action, GVariant *parameter, gpointer user_data);
static void SaveStateDialog(GSimpleAction *action, GVariant *parameter, gpointer user_data);
static void LoadStateDialog(GSimpleAction *action, GVariant *parameter, gpointer user_data);
static void ExportBackupMemoryDialog(GSimpleAction *action, GVariant *parameter, gpointer user_data);
static void ImportBackupMemoryDialog(GSimpleAction *action, GVariant *parameter, gpointer user_data);
void Launch(GSimpleAction *action, GVariant *parameter, gpointer user_data);
void Pause(GSimpleAction *action, GVariant *parameter, gpointer user_data);
static void ResetSaveStateTimes();
static void LoadSaveStateInfo();
static void Printscreen(GSimpleAction *action, GVariant *parameter, gpointer user_data);
static void Reset(GSimpleAction *action, GVariant *parameter, gpointer user_data);
static void SetAudioVolume(GSimpleAction *action, GVariant *parameter, gpointer user_data);
static void SetFirmwareLanguage(GSimpleAction *action, GVariant *parameter, gpointer user_data);
static void Edit_Controls(GSimpleAction *action, GVariant *parameter, gpointer user_data);
static void Edit_Joystick_Controls(GSimpleAction *action, GVariant *parameter, gpointer user_data);
static void MenuSave(GSimpleAction *action, GVariant *parameter, gpointer user_data);
static void changesavetype(GSimpleAction *action, GVariant *parameter, gpointer user_data);
static void MenuLoad(GSimpleAction *action, GVariant *parameter, gpointer user_data);
static void About(GSimpleAction *action, GVariant *parameter, gpointer user_data);
static void ToggleMenuVisible(GSimpleAction *action, GVariant *parameter, gpointer user_data);
static void ToggleStatusbarVisible(GSimpleAction *action, GVariant *parameter, gpointer user_data);
static void ToggleToolbarVisible(GSimpleAction *action, GVariant *parameter, gpointer user_data);
static void ToggleFullscreen(GSimpleAction *action, GVariant *parameter, gpointer user_data);
static void ToggleAudio(GSimpleAction *action, GVariant *parameter, gpointer user_data);
static void Modify_SPUMode(GSimpleAction *action, GVariant *parameter, gpointer user_data);
static void Modify_SPUInterpolation(GSimpleAction *action, GVariant *parameter, gpointer user_data);
#ifdef FAKE_MIC
static void ToggleMicNoise(GSimpleAction *action, GVariant *parameter, gpointer user_data);
#endif
static void ToggleFpsLimiter(GSimpleAction *action, GVariant *parameter, gpointer user_data);
static void ToggleAutoFrameskip(GSimpleAction *action, GVariant *parameter, gpointer user_data);
static void Modify_Frameskip(GSimpleAction *action, GVariant *parameter, gpointer user_data);
static void ToggleSwapScreens(GSimpleAction *action, GVariant *parameter, gpointer user_data);
static void ToggleGap(GSimpleAction *action, GVariant *parameter, gpointer user_data);
static void SetRotation(GSimpleAction *action, GVariant *parameter, gpointer user_data);
static void SetWinsize(GSimpleAction *action, GVariant *parameter, gpointer user_data);
static void Modify_PriInterpolation(GSimpleAction *action, GVariant *parameter, gpointer user_data);
static void Modify_Interpolation(GSimpleAction *action, GVariant *parameter, gpointer user_data);
static void SetOrientation(GSimpleAction *action, GVariant *parameter, gpointer user_data);
static void ToggleLayerVisibility(GSimpleAction *action, GVariant *parameter, gpointer user_data);
#ifdef HAVE_LIBAGG
static void HudFps(GSimpleAction *action, GVariant *parameter, gpointer user_data);
static void HudInput(GSimpleAction *action, GVariant *parameter, gpointer user_data);
static void HudGraphicalInput(GSimpleAction *action, GVariant *parameter, gpointer user_data);
static void HudFrameCounter(GSimpleAction *action, GVariant *parameter, gpointer user_data);
static void HudLagCounter(GSimpleAction *action, GVariant *parameter, gpointer user_data);
static void HudRtc(GSimpleAction *action, GVariant *parameter, gpointer user_data);
static void HudMic(GSimpleAction *action, GVariant *parameter, gpointer user_data);
static void HudEditor(GSimpleAction *action, GVariant *parameter, gpointer user_data);
#endif
#ifdef DESMUME_GTK_FIRMWARE_BROKEN
static void SelectFirmwareFile(GSimpleAction *action, GVariant *parameter, gpointer user_data);
#endif
#ifdef HAVE_JIT
static void EmulationSettingsDialog(GSimpleAction *action, GVariant *parameter, gpointer user_data);
static void ToggleJIT();
static void JITMaxBlockSizeChanged(GtkAdjustment* adj,void *);
#endif
static void GraphicsSettingsDialog(GSimpleAction *action, GVariant *parameter, gpointer user_data);

static const GActionEntry app_entries[] = {
    // File
    { "open",          OpenNdsDialog },
    { "recent",        OpenRecent, "s" },
    { "run",           Launch },
    { "pause",         Pause },
    { "reset",         Reset },
    { "savestateto",   SaveStateDialog },
    { "loadstatefrom", LoadStateDialog },
    { "savestate",     MenuSave, "u" },
    { "loadstate",     MenuLoad, "u" },
    { "importbackup",  ImportBackupMemoryDialog },
    { "exportbackup",  ExportBackupMemoryDialog },
    { "recordmovie",   RecordMovieDialog },
    { "playmovie",     PlayMovieDialog },
    { "stopmovie",     StopMovie },
    { "record_x264",   RecordAV_x264 },
    { "record_flac",   RecordAV_flac },
    { "record_stop",   RecordAV_stop },
#ifdef DESMUME_GTK_FIRMWARE_BROKEN
    { "loadfirmware",  SelectFirmwareFile },
#endif
    { "printscreen",   Printscreen },
    { "quit",          DoQuit },

    // View
    { "orient",         SetOrientation,         "s",  "\"vertical\"" },
    { "swapscreens",    ToggleSwapScreens,      NULL, "true" },
    { "rotate",         SetRotation,            "s",  "\"0\"" },
    { "winsize",        SetWinsize,             "s",  "\"scale\"" },
    { "fullscreen",     ToggleFullscreen,       NULL, "false" },
    { "gap",            ToggleGap,              NULL, "false" },
    { "pri_interpolation", Modify_PriInterpolation, "s", "\"none\"" },
    { "interpolation",  Modify_Interpolation,   "s",  "\"nearest\"" },
    { "view_menu",      ToggleMenuVisible,      NULL, "true" },
    { "view_toolbar",   ToggleToolbarVisible,   NULL, "true" },
    { "view_statusbar", ToggleStatusbarVisible, NULL, "true" },
#ifdef HAVE_LIBAGG
    { "hud_fps",        HudFps,                 NULL, "false" },
    { "hud_input",      HudInput,               NULL, "false" },
    { "hud_graphicalinput", HudGraphicalInput,  NULL, "false" },
    { "hud_framecounter", HudFrameCounter,      NULL, "false" },
    { "hud_lagcounter", HudLagCounter,          NULL, "false" },
    { "hud_rtc",        HudRtc,                 NULL, "false" },
    { "hud_mic",        HudMic,                 NULL, "false" },
    { "hud_editor",     HudEditor,              NULL, "false" },
#endif

    // Config
    { "enablefpslimiter",  ToggleFpsLimiter,    NULL, "true" },
    { "frameskipA",        ToggleAutoFrameskip, NULL, "true" },
    { "frameskip",         Modify_Frameskip,    "s",  "\"0\"" },
    { "enableaudio",       ToggleAudio,         NULL, "true" },
#ifdef FAKE_MIC
    { "micnoise",          ToggleMicNoise,      NULL, "false" },
#endif
#ifdef HAVE_JIT
    { "emulationsettings",   EmulationSettingsDialog },
#endif
    { "graphics_settings",   GraphicsSettingsDialog },
    { "spu_mode",            Modify_SPUMode,          "s", "\"dual-async\"" },
    { "spu_interpolation",   Modify_SPUInterpolation, "s", "\"linear\"" },
    { "cheatsearch",         CheatSearch },
    { "cheatlist",           CheatList },
    { "savetype",            changesavetype,          "s", "\"autodetect\"" },
    { "setaudiovolume",      SetAudioVolume },
    { "setfirmwarelanguage", SetFirmwareLanguage },
    { "editctrls",           Edit_Controls },
    { "editjoyctrls",        Edit_Joystick_Controls },

    // Tools
    // Populated in desmume_gtk_menu_tools().

    // Help
    { "about",         About },
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

/* When adding modes here remember to add the relevent entry to screen_size */
enum orientation_enum {
    ORIENT_VERTICAL = 0,
    ORIENT_HORIZONTAL = 1,
    ORIENT_SINGLE = 2,
    ORIENT_N
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

SoundInterface_struct *SNDCoreList[] = {
&SNDDummy,
&SNDSDL,
NULL
};

GPU3DInterface *core3DList[] = {
  &gpu3DNull,
  &gpu3DRasterize,
#ifdef HAVE_OPENGL
  &gpu3Dgl,
#endif
};

int multisampleSizes[] = {0, 2, 4, 8, 16, 32};

static const u32 gtk_kb_cfg[NB_KEYS] = {
    GDK_KEY_x,         // A
    GDK_KEY_z,         // B
    GDK_KEY_Shift_R,   // select
    GDK_KEY_Return,    // start
    GDK_KEY_Right,     // Right
    GDK_KEY_Left,      // Left
    GDK_KEY_Up,        // Up
    GDK_KEY_Down,      // Down
    GDK_KEY_w,         // R
    GDK_KEY_q,         // L
    GDK_KEY_s,         // X
    GDK_KEY_a,         // Y
    GDK_KEY_p,         // DEBUG
    GDK_KEY_o,         // BOOST
    GDK_KEY_BackSpace, // Lid
};

GKeyFile *keyfile;

struct modify_key_ctx {
    gint mk_key_chosen;
    GtkWidget *label;
    u8 key_id;
};

static u16 keys_latch = 0;
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
init_configured_features( class configured_features *config	)
{
  if(config->render3d == COMMANDLINE_RENDER3D_GL || config->render3d == COMMANDLINE_RENDER3D_OLDGL || config->render3d == COMMANDLINE_RENDER3D_AUTOGL)
    config->engine_3d = 2;
  else if (config->render3d == COMMANDLINE_RENDER3D_DEFAULT)
	  // Setting it to -1 so that common_gtk_main() will ignore it and load it from file or reconfigure it.
    config->engine_3d = -1;
  else
	  config->engine_3d = 1;

  config->savetype = 0;

  config->timeout = 0;

  /* use the default language */
  config->firmware_language = -1; 

  /* If specified by --lang option the lang will change to choosed one */
  config->firmware_language = config->language;
}

static int
fill_configured_features( class configured_features *config,
                          char ** argv)
{
  GOptionEntry options[] = {
    { "3d-render", 0, 0, G_OPTION_ARG_INT, &config->engine_3d, "Select 3D rendering engine. Available engines:\n"
        "\t\t\t\t  0 = 3d disabled\n"
        "\t\t\t\t  1 = internal rasterizer (default)\n"
#ifdef HAVE_OPENGL
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

  //g_option_context_add_main_entries (config->ctx, options, "options");
  //g_option_context_add_group (config->ctx, gtk_get_option_group (TRUE));

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

  // Check if the commandLine argument was actually passed
	if (config->engine_3d != -1) {
		if (config->engine_3d != 0 && config->engine_3d != 1
#ifdef HAVE_OPENGL
				&& config->engine_3d != 2
#endif
						) {
			g_printerr("Currently available ENGINES: 0, 1"
#ifdef HAVE_OPENGL
							", 2"
#endif
					"\n");
			goto error;
		}
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
  GThread *new_thread = g_thread_new(NULL,
                                     (GThreadFunc)thread_function,
                                     thread_data);

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

static GtkApplication *pApp;
static GtkWidget *pWindow;
static GtkWidget *pToolBar;
static GtkWidget *pStatusBar;
static GtkWidget *pDrawingArea;

struct nds_screen_t {
    guint gap_size;
    gint rotation_angle;
    orientation_enum orientation;
    cairo_matrix_t touch_matrix;
    cairo_matrix_t topscreen_matrix;
    gboolean swap;
};

struct nds_screen_t nds_screen;

static guint regMainLoop = 0;

static inline void UpdateStatusBar (const char *message)
{
    gint pStatusBar_Ctx;

    pStatusBar_Ctx = gtk_statusbar_get_context_id(GTK_STATUSBAR(pStatusBar), "Global");
    gtk_statusbar_pop(GTK_STATUSBAR(pStatusBar), pStatusBar_Ctx);
    gtk_statusbar_push(GTK_STATUSBAR(pStatusBar), pStatusBar_Ctx, message);
}

static void About(GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
    static const gchar *authors[] = {
    	"yopyop (original author)",
    	"DeSmuME team",
    	NULL
    };

    gtk_show_about_dialog(GTK_WINDOW(pWindow),
            "program-name", "DeSmuME",
            "version", EMU_DESMUME_VERSION_STRING() + 1, // skip space
            "website", "http://desmume.org",
            "logo-icon-name", "org.desmume.DeSmuME",
            "comments", "Nintendo DS emulator based on work by Yopyop",
            "authors", authors,
            NULL);
}

static void ToggleMenuVisible(GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
    GVariant *variant = g_action_get_state(G_ACTION(action));
    gboolean value = !g_variant_get_boolean(variant);
    config.view_menu = value;
    gtk_application_window_set_show_menubar(GTK_APPLICATION_WINDOW(pWindow), config.view_menu);
    g_simple_action_set_state(action, g_variant_new_boolean(value));
}

static void ToggleToolbarVisible(GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
    GVariant *variant = g_action_get_state(G_ACTION(action));
    gboolean value = !g_variant_get_boolean(variant);
    config.view_toolbar = value;
    if (config.view_toolbar)
      gtk_widget_show(pToolBar);
    else
      gtk_widget_hide(pToolBar);
    g_simple_action_set_state(action, g_variant_new_boolean(value));
}

static void ToggleStatusbarVisible(GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
    GVariant *variant = g_action_get_state(G_ACTION(action));
    gboolean value = !g_variant_get_boolean(variant);
    config.view_statusbar = value;
    if (config.view_statusbar)
      gtk_widget_show(pStatusBar);
    else
      gtk_widget_hide(pStatusBar);
    g_simple_action_set_state(action, g_variant_new_boolean(value));
}

static void ToggleFullscreen(GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
  GVariant *variant = g_action_get_state(G_ACTION(action));
  gboolean fullscreen = !g_variant_get_boolean(variant);
  config.window_fullscreen = fullscreen;
  printf("ToggleFullscreen -> %d\n", fullscreen);
  g_simple_action_set_state(action, g_variant_new_boolean(fullscreen));
  if (config.window_fullscreen)
  {
    gtk_application_window_set_show_menubar(GTK_APPLICATION_WINDOW(pWindow), FALSE);
    gtk_widget_hide(pToolBar);
    gtk_widget_hide(pStatusBar);
    g_simple_action_set_enabled(G_SIMPLE_ACTION(g_action_map_lookup_action(G_ACTION_MAP(pApp), "view_menu")), FALSE);
    g_simple_action_set_enabled(G_SIMPLE_ACTION(g_action_map_lookup_action(G_ACTION_MAP(pApp), "view_toolbar")), FALSE);
    g_simple_action_set_enabled(G_SIMPLE_ACTION(g_action_map_lookup_action(G_ACTION_MAP(pApp), "view_statusbar")), FALSE);
    gtk_window_fullscreen(GTK_WINDOW(pWindow));
  }
  else
  {
    if (config.view_menu) {
      gtk_application_window_set_show_menubar(GTK_APPLICATION_WINDOW(pWindow), TRUE);
    }
    if (config.view_toolbar) {
      gtk_widget_show(pToolBar);
    }
    if (config.view_statusbar) {
      gtk_widget_show(pStatusBar);
    }
    g_simple_action_set_enabled(G_SIMPLE_ACTION(g_action_map_lookup_action(G_ACTION_MAP(pApp), "view_menu")), TRUE);
    g_simple_action_set_enabled(G_SIMPLE_ACTION(g_action_map_lookup_action(G_ACTION_MAP(pApp), "view_toolbar")), TRUE);
    g_simple_action_set_enabled(G_SIMPLE_ACTION(g_action_map_lookup_action(G_ACTION_MAP(pApp), "view_statusbar")), TRUE);
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
        g_simple_action_set_enabled(G_SIMPLE_ACTION(g_action_map_lookup_action(G_ACTION_MAP(pApp), "cheatlist")), TRUE);
    }
    return res;
}

void Launch(GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
    GtkWidget *pause;
    desmume_resume();

    if(!regMainLoop) {
        regMainLoop = g_idle_add_full(EMULOOP_PRIO, &EmuLoop, pWindow, NULL);
    }

    UpdateStatusBar("Running ...");

    g_simple_action_set_enabled(G_SIMPLE_ACTION(g_action_map_lookup_action(G_ACTION_MAP(pApp), "pause")), TRUE);
    g_simple_action_set_enabled(G_SIMPLE_ACTION(g_action_map_lookup_action(G_ACTION_MAP(pApp), "run")), FALSE);
    g_simple_action_set_enabled(G_SIMPLE_ACTION(g_action_map_lookup_action(G_ACTION_MAP(pApp), "reset")), TRUE);
    g_simple_action_set_enabled(G_SIMPLE_ACTION(g_action_map_lookup_action(G_ACTION_MAP(pApp), "printscreen")), TRUE);
    g_simple_action_set_enabled(G_SIMPLE_ACTION(g_action_map_lookup_action(G_ACTION_MAP(pApp), "exportbackup")), TRUE);
    g_simple_action_set_enabled(G_SIMPLE_ACTION(g_action_map_lookup_action(G_ACTION_MAP(pApp), "importbackup")), TRUE);

    //pause = gtk_bin_get_child(GTK_BIN(gtk_ui_manager_get_widget(ui_manager, "/ToolBar/pause")));
    //gtk_widget_grab_focus(pause);
}


void Pause(GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
    GtkWidget *run;
    desmume_pause();
    UpdateStatusBar("Paused");

    g_simple_action_set_enabled(G_SIMPLE_ACTION(g_action_map_lookup_action(G_ACTION_MAP(pApp), "pause")), FALSE);
    g_simple_action_set_enabled(G_SIMPLE_ACTION(g_action_map_lookup_action(G_ACTION_MAP(pApp), "run")), TRUE);

    //run = gtk_bin_get_child(GTK_BIN(gtk_ui_manager_get_widget(ui_manager, "/ToolBar/run")));
    //gtk_widget_grab_focus(run);
}

static void LoadStateDialog(GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
    GtkFileFilter *pFilter_ds, *pFilter_any;
    GtkFileChooserNative *pFileSelection;

    if (desmume_running())
        Pause(NULL, NULL, NULL);

    pFilter_ds = gtk_file_filter_new();
    gtk_file_filter_add_pattern(pFilter_ds, "*.ds*");
    gtk_file_filter_set_name(pFilter_ds, "DeSmuME binary (.ds*)");

    pFilter_any = gtk_file_filter_new();
    gtk_file_filter_add_pattern(pFilter_any, "*");
    gtk_file_filter_set_name(pFilter_any, "All files");

    /* Creating the selection window */
    pFileSelection = gtk_file_chooser_native_new("Load State From ...",
            GTK_WINDOW(pWindow),
            GTK_FILE_CHOOSER_ACTION_OPEN,
            "_Open", "_Cancel");

    gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(pFileSelection), pFilter_ds);
    gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(pFileSelection), pFilter_any);

    /* Showing the window */
    int response = gtk_native_dialog_run(GTK_NATIVE_DIALOG(pFileSelection));
    if (response == GTK_RESPONSE_ACCEPT) {
        GFile *file = gtk_file_chooser_get_file(GTK_FILE_CHOOSER(pFileSelection));
        gchar *sPath = g_file_get_path(file);

        if(savestate_load(sPath) == false ) {
            GtkWidget *pDialog = gtk_message_dialog_new(GTK_WINDOW(pWindow),
                    GTK_DIALOG_MODAL,
                    GTK_MESSAGE_ERROR,
                    GTK_BUTTONS_OK,
                    "Unable to load :\n%s", sPath);
            gtk_dialog_run(GTK_DIALOG(pDialog));
            gtk_widget_destroy(pDialog);
        } else {
            g_simple_action_set_enabled(G_SIMPLE_ACTION(g_action_map_lookup_action(G_ACTION_MAP(pApp), "run")), TRUE);
        }

        g_free(sPath);
    }
    g_object_unref(pFileSelection);
}

static void RecordMovieDialog(GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
 GtkFileFilter *pFilter_dsm, *pFilter_any;
    GtkFileChooserNative *pFileSelection;

    if (desmume_running())
        Pause(NULL, NULL, NULL);

    pFilter_dsm = gtk_file_filter_new();
    gtk_file_filter_add_pattern(pFilter_dsm, "*.dsm*");
    gtk_file_filter_set_name(pFilter_dsm, "DeSmuME movie file (.dsm*)");

    pFilter_any = gtk_file_filter_new();
    gtk_file_filter_add_pattern(pFilter_any, "*");
    gtk_file_filter_set_name(pFilter_any, "All files");

    /* Creating the selection window */
    pFileSelection = gtk_file_chooser_native_new("Save Movie To ...",
            GTK_WINDOW(pWindow),
            GTK_FILE_CHOOSER_ACTION_SAVE,
            "_Save", "_Cancel");
    gtk_file_chooser_set_do_overwrite_confirmation (GTK_FILE_CHOOSER (pFileSelection), TRUE);

    gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(pFileSelection), pFilter_dsm);
    gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(pFileSelection), pFilter_any);

    /* Showing the window */
    int response = gtk_native_dialog_run(GTK_NATIVE_DIALOG(pFileSelection));
    if (response == GTK_RESPONSE_ACCEPT) {
        GFile *file = gtk_file_chooser_get_file(GTK_FILE_CHOOSER(pFileSelection));
        gchar *sPath = g_file_get_path(file);

        FCEUI_SaveMovie(sPath,L"",START_BLANK,"", FCEUI_MovieGetRTCDefault());
        g_free(sPath);
    }
    g_object_unref(pFileSelection);
}

static void StopMovie(GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
	FCEUI_StopMovie();
}

static void PlayMovieDialog(GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
   GtkFileFilter *pFilter_dsm, *pFilter_any;
    GtkFileChooserNative *pFileSelection;

    if (desmume_running())
        Pause(NULL, NULL, NULL);

    pFilter_dsm = gtk_file_filter_new();
    gtk_file_filter_add_pattern(pFilter_dsm, "*.dsm*");
    gtk_file_filter_set_name(pFilter_dsm, "DeSmuME movie file (.dsm*)");

    pFilter_any = gtk_file_filter_new();
    gtk_file_filter_add_pattern(pFilter_any, "*");
    gtk_file_filter_set_name(pFilter_any, "All files");

    /* Creating the selection window */
    pFileSelection = gtk_file_chooser_native_new("Play movie from...",
            GTK_WINDOW(pWindow),
            GTK_FILE_CHOOSER_ACTION_OPEN,
            "_Open", "_Cancel");
    gtk_file_chooser_set_do_overwrite_confirmation (GTK_FILE_CHOOSER (pFileSelection), TRUE);

    gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(pFileSelection), pFilter_dsm);
    gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(pFileSelection), pFilter_any);

    /* Showing the window */
    int response = gtk_native_dialog_run(GTK_NATIVE_DIALOG(pFileSelection));
    if (response == GTK_RESPONSE_ACCEPT) {
        GFile *file = gtk_file_chooser_get_file(GTK_FILE_CHOOSER(pFileSelection));
        gchar *sPath = g_file_get_path(file);

		FCEUI_LoadMovie(sPath,true,false,-1);

        g_free(sPath);
    }
    g_object_unref(pFileSelection);
}

static void ImportBackupMemoryDialog(GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
    GtkFileFilter *pFilter_raw, *pFilter_ar, *pFilter_any;
    GtkFileChooserNative *pFileSelection;

    if (desmume_running())
        Pause(NULL, NULL, NULL);

    pFilter_raw = gtk_file_filter_new();
    gtk_file_filter_add_pattern(pFilter_raw, "*.sav");
    gtk_file_filter_set_name(pFilter_raw, "Raw/No$GBA Save format (*.sav)");

    pFilter_ar = gtk_file_filter_new();
    gtk_file_filter_add_pattern(pFilter_ar, "*.duc");
    gtk_file_filter_add_pattern(pFilter_ar, "*.dss");
    gtk_file_filter_set_name(pFilter_ar, "Action Replay DS Save (*.duc,*.dss)");

    pFilter_any = gtk_file_filter_new();
    gtk_file_filter_add_pattern(pFilter_any, "*.sav");
    gtk_file_filter_add_pattern(pFilter_any, "*.duc");
    gtk_file_filter_add_pattern(pFilter_any, "*.dss");
    gtk_file_filter_set_name(pFilter_any, "All supported types");

    /* Creating the selection window */
    pFileSelection = gtk_file_chooser_native_new("Import Backup Memory From ...",
            GTK_WINDOW(pWindow),
            GTK_FILE_CHOOSER_ACTION_OPEN,
            "_Open", "_Cancel");

    gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(pFileSelection), pFilter_raw);
    gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(pFileSelection), pFilter_ar);
    gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(pFileSelection), pFilter_any);

    /* Showing the window */
    int response = gtk_native_dialog_run(GTK_NATIVE_DIALOG(pFileSelection));
    if (response == GTK_RESPONSE_ACCEPT) {
        GFile *file = gtk_file_chooser_get_file(GTK_FILE_CHOOSER(pFileSelection));
        gchar *sPath = g_file_get_path(file);

        if(MMU_new.backupDevice.importData(sPath) == false ) {
            GtkWidget *pDialog = gtk_message_dialog_new(GTK_WINDOW(pWindow),
                    GTK_DIALOG_MODAL,
                    GTK_MESSAGE_ERROR,
                    GTK_BUTTONS_OK,
                    "Unable to import:\n%s", sPath);
            gtk_dialog_run(GTK_DIALOG(pDialog));
            gtk_widget_destroy(pDialog);
        }

        g_free(sPath);
    }
    g_object_unref(pFileSelection);
    Launch(NULL, NULL, NULL);
}

static void ExportBackupMemoryDialog(GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
    GtkFileFilter *pFilter_raw, *pFilter_ar, *pFilter_any;
    GtkFileChooserNative *pFileSelection;

    if (desmume_running())
        Pause(NULL, NULL, NULL);

    pFilter_raw = gtk_file_filter_new();
    gtk_file_filter_add_pattern(pFilter_raw, "*.sav");
    gtk_file_filter_set_name(pFilter_raw, "Raw/No$GBA Save format (*.sav)");

    pFilter_any = gtk_file_filter_new();
    gtk_file_filter_add_pattern(pFilter_any, "*.sav");
    gtk_file_filter_set_name(pFilter_any, "All supported types");

    /* Creating the selection window */
    pFileSelection = gtk_file_chooser_native_new("Export Backup Memory To ...",
            GTK_WINDOW(pWindow),
            GTK_FILE_CHOOSER_ACTION_SAVE,
            "_Save", "_Cancel");
    gtk_file_chooser_set_do_overwrite_confirmation (GTK_FILE_CHOOSER (pFileSelection), TRUE);

    gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(pFileSelection), pFilter_raw);
    gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(pFileSelection), pFilter_any);

    /* Showing the window */
    int response = gtk_native_dialog_run(GTK_NATIVE_DIALOG(pFileSelection));
    if (response == GTK_RESPONSE_ACCEPT) {
        GFile *file = gtk_file_chooser_get_file(GTK_FILE_CHOOSER(pFileSelection));
        gchar *sPath = g_file_get_path(file);

        if(! g_str_has_suffix(sPath, ".sav"))
        {
            sPath = g_strjoin(NULL, sPath, ".sav", NULL);
        }

        if(MMU_new.backupDevice.exportData(sPath) == false ) {
            GtkWidget *pDialog = gtk_message_dialog_new(GTK_WINDOW(pWindow),
                    GTK_DIALOG_MODAL,
                    GTK_MESSAGE_ERROR,
                    GTK_BUTTONS_OK,
                    "Unable to export:\n%s", sPath);
            gtk_dialog_run(GTK_DIALOG(pDialog));
            gtk_widget_destroy(pDialog);
        }

        g_free(sPath);
    }
    g_object_unref(pFileSelection);
    Launch(NULL, NULL, NULL);
}

static void SaveStateDialog(GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
    GtkFileFilter *pFilter_ds, *pFilter_any;
    GtkFileChooserNative *pFileSelection;

    if (desmume_running())
        Pause(NULL, NULL, NULL);

    pFilter_ds = gtk_file_filter_new();
    gtk_file_filter_add_pattern(pFilter_ds, "*.ds*");
    gtk_file_filter_set_name(pFilter_ds, "DeSmuME binary (.ds*)");

    pFilter_any = gtk_file_filter_new();
    gtk_file_filter_add_pattern(pFilter_any, "*");
    gtk_file_filter_set_name(pFilter_any, "All files");

    /* Creating the selection window */
    pFileSelection = gtk_file_chooser_native_new("Save State To ...",
            GTK_WINDOW(pWindow),
            GTK_FILE_CHOOSER_ACTION_SAVE,
            "_Save", "_Cancel");
    gtk_file_chooser_set_do_overwrite_confirmation (GTK_FILE_CHOOSER (pFileSelection), TRUE);

    gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(pFileSelection), pFilter_ds);
    gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(pFileSelection), pFilter_any);

    /* Showing the window */
    int response = gtk_native_dialog_run(GTK_NATIVE_DIALOG(pFileSelection));
    if (response == GTK_RESPONSE_ACCEPT) {
        GFile *file = gtk_file_chooser_get_file(GTK_FILE_CHOOSER(pFileSelection));
        gchar *sPath = g_file_get_path(file);

        if(savestate_save(sPath) == false ) {
            GtkWidget *pDialog = gtk_message_dialog_new(GTK_WINDOW(pWindow),
                    GTK_DIALOG_MODAL,
                    GTK_MESSAGE_ERROR,
                    GTK_BUTTONS_OK,
                    "Unable to save :\n%s", sPath);
            gtk_dialog_run(GTK_DIALOG(pDialog));
            gtk_widget_destroy(pDialog);
        } else {
            g_simple_action_set_enabled(G_SIMPLE_ACTION(g_action_map_lookup_action(G_ACTION_MAP(pApp), "run")), TRUE);
        }

        g_free(sPath);
    }
    g_object_unref(pFileSelection);
}

static void RecordAV_x264(GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
    GtkFileFilter *pFilter_mkv, *pFilter_mp4, *pFilter_any;
    GtkFileChooserNative *pFileSelection;

    if (desmume_running())
        Pause(NULL, NULL, NULL);

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
    pFileSelection = gtk_file_chooser_native_new("Save video...",
            GTK_WINDOW(pWindow),
            GTK_FILE_CHOOSER_ACTION_SAVE,
            "_Save", "_Cancel");
    gtk_file_chooser_set_do_overwrite_confirmation (GTK_FILE_CHOOSER (pFileSelection), TRUE);

    gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(pFileSelection), pFilter_mkv);
    gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(pFileSelection), pFilter_mp4);
    gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(pFileSelection), pFilter_any);

    /* Showing the window */
    int response = gtk_native_dialog_run(GTK_NATIVE_DIALOG(pFileSelection));
    if (response == GTK_RESPONSE_ACCEPT) {
        GFile *file = gtk_file_chooser_get_file(GTK_FILE_CHOOSER(pFileSelection));
        gchar *sPath = g_file_get_path(file);

        if(avout_x264.begin(sPath)) {
            g_simple_action_set_enabled(G_SIMPLE_ACTION(g_action_map_lookup_action(G_ACTION_MAP(pApp), "record_x264")), FALSE);
        } else {
            GtkWidget *pDialog = gtk_message_dialog_new(GTK_WINDOW(pWindow),
                    GTK_DIALOG_MODAL,
                    GTK_MESSAGE_ERROR,
                    GTK_BUTTONS_OK,
                    "Unable to record video to:\n%s", sPath);
            gtk_dialog_run(GTK_DIALOG(pDialog));
            gtk_widget_destroy(pDialog);
        }

        g_free(sPath);
    }
    g_object_unref(pFileSelection);
}

static void RecordAV_flac(GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
    GtkFileFilter *pFilter_flac, *pFilter_any;
    GtkFileChooserNative *pFileSelection;

    if (desmume_running())
        Pause(NULL, NULL, NULL);

    pFilter_flac = gtk_file_filter_new();
    gtk_file_filter_add_pattern(pFilter_flac, "*.flac");
    gtk_file_filter_set_name(pFilter_flac, "FLAC file (.flac)");

    pFilter_any = gtk_file_filter_new();
    gtk_file_filter_add_pattern(pFilter_any, "*");
    gtk_file_filter_set_name(pFilter_any, "All files");

    /* Creating the selection window */
    pFileSelection = gtk_file_chooser_native_new("Save audio...",
            GTK_WINDOW(pWindow),
            GTK_FILE_CHOOSER_ACTION_SAVE,
            "_Save", "_Cancel");
    gtk_file_chooser_set_do_overwrite_confirmation (GTK_FILE_CHOOSER (pFileSelection), TRUE);

    gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(pFileSelection), pFilter_flac);
    gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(pFileSelection), pFilter_any);

    /* Showing the window */
    int response = gtk_native_dialog_run(GTK_NATIVE_DIALOG(pFileSelection));
    if (response == GTK_RESPONSE_ACCEPT) {
        GFile *file = gtk_file_chooser_get_file(GTK_FILE_CHOOSER(pFileSelection));
        gchar *sPath = g_file_get_path(file);

        if(avout_flac.begin(sPath)) {
            g_simple_action_set_enabled(G_SIMPLE_ACTION(g_action_map_lookup_action(G_ACTION_MAP(pApp), "record_flac")), FALSE);
        } else {
            GtkWidget *pDialog = gtk_message_dialog_new(GTK_WINDOW(pWindow),
                    GTK_DIALOG_MODAL,
                    GTK_MESSAGE_ERROR,
                    GTK_BUTTONS_OK,
                    "Unable to record audio to:\n%s", sPath);
            gtk_dialog_run(GTK_DIALOG(pDialog));
            gtk_widget_destroy(pDialog);
        }

        g_free(sPath);
    }
    g_object_unref(pFileSelection);
}

static void RecordAV_stop(GSimpleAction *action, GVariant *parameter, gpointer user_data) {
	avout_x264.end();
	avout_flac.end();
    g_simple_action_set_enabled(G_SIMPLE_ACTION(g_action_map_lookup_action(G_ACTION_MAP(pApp), "record_x264")), TRUE);
    g_simple_action_set_enabled(G_SIMPLE_ACTION(g_action_map_lookup_action(G_ACTION_MAP(pApp), "record_flac")), TRUE);
}

static void OpenNdsDialog(GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
    GtkFileFilter *pFilter_nds, *pFilter_dsgba, *pFilter_any;
    GtkFileChooserNative *pFileSelection;

    if (desmume_running())
        Pause(NULL, NULL, NULL);

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
    pFileSelection = gtk_file_chooser_native_new("Open...",
            GTK_WINDOW(pWindow),
            GTK_FILE_CHOOSER_ACTION_OPEN,
            "_Open", "_Cancel");

    gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(pFileSelection), pFilter_nds);
    gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(pFileSelection), pFilter_dsgba);
    gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(pFileSelection), pFilter_any);

    gtk_file_chooser_set_current_folder(GTK_FILE_CHOOSER(pFileSelection), g_get_home_dir());

    /* Showing the window */
    int response = gtk_native_dialog_run(GTK_NATIVE_DIALOG(pFileSelection));
    if (response == GTK_RESPONSE_ACCEPT) {
        GFile *file = gtk_file_chooser_get_file(GTK_FILE_CHOOSER(pFileSelection));
        gchar *sPath = g_file_get_path(file);

        if(Open((const char*)sPath) < 0) {
            GtkWidget *pDialog = gtk_message_dialog_new(GTK_WINDOW(pWindow),
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
            g_free(recentData.app_exec);
            g_simple_action_set_enabled(G_SIMPLE_ACTION(g_action_map_lookup_action(G_ACTION_MAP(pApp), "run")), TRUE);
            Launch(NULL, NULL, NULL);
        }

        g_free(sPath);
    }
    g_object_unref(pFileSelection);
}

static void OpenRecent(GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
    GtkRecentManager *recent_manager = gtk_recent_manager_get_default();
    gchar *romname;
    int ret;

    if (desmume_running())
        Pause(NULL, NULL, NULL);

    const char *uri = g_variant_get_string(parameter, NULL);
    romname = g_filename_from_uri(uri, NULL, NULL);
    ret = Open(romname);
    if (ret > 0) {
        g_simple_action_set_enabled(G_SIMPLE_ACTION(g_action_map_lookup_action(G_ACTION_MAP(pApp), "run")), TRUE);
        Launch(NULL, NULL, NULL);
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

    g_free(romname);
}

static void Reset(GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
    bool shouldBeRunning = desmume_running();
    Pause(NULL, NULL, NULL);
    NDS_Reset();
    RedrawScreen();
    if (shouldBeRunning) {
        Launch(NULL, NULL, NULL);
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

static void ToggleGap(GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
    GVariant *variant = g_action_get_state(G_ACTION(action));
    gboolean value = !g_variant_get_boolean(variant);
    config.view_gap = value;
    nds_screen.gap_size = config.view_gap ? GAP_SIZE : 0;
    UpdateDrawingAreaAspect();
    g_simple_action_set_state(action, g_variant_new_boolean(value));
}

static void SetRotation(GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
    const char *string = g_variant_get_string(parameter, NULL);
    nds_screen.rotation_angle = g_ascii_strtoll(string, NULL, 10);
    config.view_rot = nds_screen.rotation_angle;
    UpdateDrawingAreaAspect();
    g_simple_action_set_state(action, parameter);
}

static void SetWinsize(GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
    const char *string = g_variant_get_string(parameter, NULL);
    winsize_enum winsize;
    if (strcmp(string, "scale") == 0)
        winsize = WINSIZE_SCALE;
    else if (strcmp(string, "0.5") == 0)
        winsize = WINSIZE_HALF;
    else if (strcmp(string, "1") == 0)
        winsize = WINSIZE_1;
    else if (strcmp(string, "1.5") == 0)
        winsize = WINSIZE_1HALF;
    else if (strcmp(string, "2") == 0)
        winsize = WINSIZE_2;
    else if (strcmp(string, "2.5") == 0)
        winsize = WINSIZE_2HALF;
    else if (strcmp(string, "3") == 0)
        winsize = WINSIZE_3;
    else if (strcmp(string, "4") == 0)
        winsize = WINSIZE_4;
    else if (strcmp(string, "5") == 0)
        winsize = WINSIZE_5;
    winsize_current = winsize;
    config.window_scale = winsize_current;
    if (config.window_fullscreen) {
        g_simple_action_set_state(G_SIMPLE_ACTION(g_action_map_lookup_action(G_ACTION_MAP(pApp), "fullscreen")), FALSE);
    }
    g_simple_action_set_enabled(G_SIMPLE_ACTION(g_action_map_lookup_action(G_ACTION_MAP(pApp), "fullscreen")), winsize_current == WINSIZE_SCALE);
    UpdateDrawingAreaAspect();
    g_simple_action_set_state(action, parameter);
}

static void SetOrientation(GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
    const char *string = g_variant_get_string(parameter, NULL);
	orientation_enum orient=ORIENT_VERTICAL;
	if(strcmp(string, "vertical") == 0)
		orient = ORIENT_VERTICAL;
	else if(strcmp(string, "horizontal") == 0)
		orient = ORIENT_HORIZONTAL;
	else if(strcmp(string, "single") == 0)
		orient = ORIENT_SINGLE;
    nds_screen.orientation = orient;
#ifdef HAVE_LIBAGG
    osd->singleScreen = nds_screen.orientation == ORIENT_SINGLE;
#endif
    config.view_orient = nds_screen.orientation;
    UpdateDrawingAreaAspect();
    g_simple_action_set_state(action, parameter);
}

static void ToggleSwapScreens(GSimpleAction *action, GVariant *parameter, gpointer user_data) {
    GVariant *variant = g_action_get_state(G_ACTION(action));
    gboolean value = !g_variant_get_boolean(variant);
    nds_screen.swap = value;
#ifdef HAVE_LIBAGG
    osd->swapScreens = nds_screen.swap;
#endif
    config.view_swap = nds_screen.swap;
    RedrawScreen();
    g_simple_action_set_state(action, g_variant_new_boolean(value));
}

static int ConfigureDrawingArea(GtkWidget *widget, GdkEventConfigure *event, gpointer data)
{
    return TRUE;
}

static inline void gpu_screen_to_rgb(u32* dst)
{
    ColorspaceConvertBuffer555To8888Opaque<false, false>((const uint16_t *)GPU->GetDisplayInfo().masterNativeBuffer, dst, GPU_FRAMEBUFFER_NATIVE_WIDTH * GPU_FRAMEBUFFER_NATIVE_HEIGHT * 2);
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

	GdkDrawingContext *context = gdk_window_begin_draw_frame(window, gdk_window_get_clip_region(window));
	cairo_t* cr = gdk_drawing_context_get_cairo_context(context);

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

	gdk_window_end_draw_frame(window, context);
	draw_count++;

	return TRUE;
}

static void RedrawScreen() {
	ColorspaceConvertBuffer555To8888Opaque<true, false>((const uint16_t *)GPU->GetDisplayInfo().masterNativeBuffer, (uint32_t *)video->GetSrcBufferPtr(), GPU_FRAMEBUFFER_NATIVE_WIDTH * GPU_FRAMEBUFFER_NATIVE_HEIGHT * 2);
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
            gdk_window_get_device_position(gtk_widget_get_window(w), e->device, &x, &y, &state);
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

#if 0
    if (e->button == 3) {
        GtkWidget * pMenu = gtk_menu_item_get_submenu ( GTK_MENU_ITEM(
                    gtk_ui_manager_get_widget (ui_manager, "/MainMenu/ViewMenu")));
        gtk_menu_popup(GTK_MENU(pMenu), NULL, NULL, NULL, NULL, 3, e->time);
    }
#endif

    if (e->button == 1) {
        gdk_window_get_device_position(gtk_widget_get_window(w), e->device, &x, &y, &state);

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
       Pause(NULL, NULL, NULL);
       loadstate_slot(num);
       Launch(NULL, NULL, NULL);
   }
   else
       loadstate_slot(num);
   RedrawScreen();
}

static void savegame(int num){
   if (desmume_running())
   {   
       Pause(NULL, NULL, NULL);
       savestate_slot(num);
       Launch(NULL, NULL, NULL);
   }
   else
       savestate_slot(num);
   LoadSaveStateInfo();
   RedrawScreen();
}

static void MenuLoad(GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
    loadgame(g_variant_get_uint32(parameter));
}

static void MenuSave(GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
    savegame(g_variant_get_uint32(parameter));
}

static gint Key_Press(GtkWidget *w, GdkEventKey *e, gpointer data)
{
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
  u16 Key = lookup_key(e->keyval);
  RM_KEY( keys_latch, Key );
  return 1;

}

/////////////////////////////// SET AUDIO VOLUME //////////////////////////////////////

static void CallbackSetAudioVolume(GtkWidget *scale, gpointer data)
{
	SNDSDLSetAudioVolume(gtk_range_get_value(GTK_RANGE(scale)));
	config.audio_volume = SNDSDLGetAudioVolume();
}

static void SetAudioVolume(GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
	GtkWidget *dialog = NULL;
	GtkWidget *scale = NULL;
	int audio_volume = SNDSDLGetAudioVolume();
	dialog = gtk_dialog_new_with_buttons("Set audio volume", GTK_WINDOW(pWindow), GTK_DIALOG_MODAL, "_OK", GTK_RESPONSE_OK, "_Cancel", GTK_RESPONSE_CANCEL, NULL);
	scale = gtk_scale_new_with_range(GTK_ORIENTATION_HORIZONTAL, 0, SDL_MIX_MAXVOLUME, 1);
	gtk_range_set_value(GTK_RANGE(scale), SNDSDLGetAudioVolume());
	g_signal_connect(G_OBJECT(scale), "value-changed", G_CALLBACK(CallbackSetAudioVolume), NULL);
	gtk_box_pack_start(GTK_BOX(gtk_dialog_get_content_area(GTK_DIALOG(dialog))), scale, TRUE, FALSE, 0);
	gtk_widget_show_all(gtk_dialog_get_content_area(GTK_DIALOG(dialog)));
	switch(gtk_dialog_run(GTK_DIALOG(dialog)))
	{
		case GTK_RESPONSE_OK:
			break;
		case GTK_RESPONSE_CANCEL:
		case GTK_RESPONSE_NONE:
			SNDSDLSetAudioVolume(audio_volume);
			config.audio_volume = SNDSDLGetAudioVolume();
			break;
	}
	gtk_widget_destroy(dialog);
}

/////////////////////////////// SET FIRMWARE LANGUAGE //////////////////////////////////////

static void CallbackSetFirmwareLanguage(GtkWidget *check_button, gpointer data)
{
	gtk_widget_set_sensitive(GTK_WIDGET(data), gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(check_button)));
}

static void SetFirmwareLanguage(GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
	GtkWidget *dialog = NULL;
	GtkWidget *combo_box_text = NULL;
	GtkWidget *check_button = NULL;
	const char *languages[6] = {"Japanese", "English", "French", "German", "Italian", "Spanish"};
	gchar *text = NULL;
	dialog = gtk_dialog_new_with_buttons("Set firmware language", GTK_WINDOW(pWindow), GTK_DIALOG_MODAL, "_OK", GTK_RESPONSE_OK, "_Cancel", GTK_RESPONSE_CANCEL, NULL);
	combo_box_text = gtk_combo_box_text_new();
	for(int index = 0; index < 6; index++)
		gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(combo_box_text), languages[index]);
	gtk_combo_box_set_active(GTK_COMBO_BOX(combo_box_text), config.firmware_language);
	gtk_widget_set_sensitive(combo_box_text, config.command_line_overriding_firmware_language);
	check_button = gtk_check_button_new_with_mnemonic("_Enable command line overriding");
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(check_button), config.command_line_overriding_firmware_language);
	g_signal_connect(G_OBJECT(check_button), "toggled", G_CALLBACK(CallbackSetFirmwareLanguage), combo_box_text);
	gtk_box_pack_start(GTK_BOX(gtk_dialog_get_content_area(GTK_DIALOG(dialog))), check_button, TRUE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(gtk_dialog_get_content_area(GTK_DIALOG(dialog))), combo_box_text, TRUE, FALSE, 0);
	gtk_widget_show_all(gtk_dialog_get_content_area(GTK_DIALOG(dialog)));
	switch(gtk_dialog_run(GTK_DIALOG(dialog)))
	{
		case GTK_RESPONSE_OK:
			text = gtk_combo_box_text_get_active_text(GTK_COMBO_BOX_TEXT(combo_box_text));
			for(int index = 0; index < 6; index++)
				if(strcmp(text, languages[index]) == 0)
				{
					CommonSettings.fwConfig.language = index;
					config.firmware_language = index;
				}
			config.command_line_overriding_firmware_language = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(check_button));
			break;
		case GTK_RESPONSE_CANCEL:
		case GTK_RESPONSE_NONE:
			break;
	}
	gtk_widget_destroy(dialog);
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
        "_OK",GTK_RESPONSE_OK,
        "_Cancel",GTK_RESPONSE_CANCEL,
        NULL);

    ctx.label = gtk_label_new(Title);
    g_free(Title);
    gtk_box_pack_start(GTK_BOX(gtk_dialog_get_content_area(GTK_DIALOG(mkDialog))), ctx.label, TRUE, FALSE, 0);

    g_signal_connect(G_OBJECT(mkDialog), "key_press_event", G_CALLBACK(AcceptNewInputKey), &ctx);

    gtk_widget_show_all(gtk_dialog_get_content_area(GTK_DIALOG(mkDialog)));

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

static void Edit_Controls(GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
    GtkWidget *ecDialog;
    GtkWidget *ecKey;
    gchar *Key_Label;
    int i;

    memcpy(&Keypad_Temp, &keyboard_cfg, sizeof(keyboard_cfg));

    ecDialog = gtk_dialog_new_with_buttons("Edit controls",
                        GTK_WINDOW(pWindow),
                        GTK_DIALOG_MODAL,
                        "_OK",GTK_RESPONSE_OK,
                        "_Cancel",GTK_RESPONSE_CANCEL,
                        NULL);

    for(i = 0; i < NB_KEYS; i++) {
        Key_Label = g_strdup_printf("%s (%s)", key_names[i], gdk_keyval_name(Keypad_Temp[i]));
        ecKey = gtk_button_new_with_label(Key_Label);
        g_free(Key_Label);
        g_signal_connect(G_OBJECT(ecKey), "clicked", G_CALLBACK(Modify_Key), GINT_TO_POINTER(i));
        gtk_box_pack_start(GTK_BOX(gtk_dialog_get_content_area(GTK_DIALOG(ecDialog))), ecKey,TRUE, FALSE, 0);
    }

    gtk_widget_show_all(gtk_dialog_get_content_area(GTK_DIALOG(ecDialog)));

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
        "_OK",GTK_RESPONSE_OK,
        "_Cancel",GTK_RESPONSE_CANCEL,
        NULL);

    ctx.label = gtk_label_new(Title);
    g_free(Title);
    gtk_box_pack_start(GTK_BOX(gtk_dialog_get_content_area(GTK_DIALOG(mkDialog))), ctx.label, TRUE, FALSE, 0);
    gtk_widget_show_all(gtk_dialog_get_content_area(GTK_DIALOG(mkDialog)));

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

#ifdef HAVE_JIT

static void EmulationSettingsDialog(GSimpleAction *action, GVariant *parameter, gpointer user_data) {
    GtkDialog *dialog;
    GtkCheckButton *use_dynarec;
    GtkAdjustment *block_size;

    GtkBuilder *builder = gtk_builder_new_from_resource("/org/desmume/DeSmuME/emulation_settings.ui");
    dialog = GTK_DIALOG(gtk_builder_get_object(builder, "dialog"));
    use_dynarec = GTK_CHECK_BUTTON(gtk_builder_get_object(builder, "use-dynarec"));
    block_size = GTK_ADJUSTMENT(gtk_builder_get_object(builder, "block-size"));
    g_object_unref(builder);

    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(use_dynarec), config.use_jit);
    g_signal_connect(G_OBJECT(use_dynarec), "clicked", G_CALLBACK(ToggleJIT), GINT_TO_POINTER(0));

    gtk_adjustment_set_value(block_size, config.jit_max_block_size);
    g_signal_connect(G_OBJECT(block_size), "value_changed", G_CALLBACK(JITMaxBlockSizeChanged), NULL);

    bool prev_use_jit = config.use_jit;
    int prev_jit_max_block_size = config.jit_max_block_size;

    switch (gtk_dialog_run(dialog)) {
    case GTK_RESPONSE_OK:
        CommonSettings.jit_max_block_size = config.jit_max_block_size;
        arm_jit_sync();
        arm_jit_reset(CommonSettings.use_jit = config.use_jit);
        break;
    case GTK_RESPONSE_CANCEL:
    case GTK_RESPONSE_NONE:
        config.use_jit = prev_use_jit;
        config.jit_max_block_size = prev_jit_max_block_size;
        break;
    }
    gtk_widget_destroy(GTK_WIDGET(dialog));

}

static void JITMaxBlockSizeChanged(GtkAdjustment* adj, gpointer data) {
    config.jit_max_block_size = (int)gtk_adjustment_get_value(adj);
}

static void ToggleJIT() {
    config.use_jit = !config.use_jit;
}

#endif

static void Edit_Joystick_Controls(GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
    GtkWidget *ecDialog;
    GtkWidget *ecKey;
    gchar *Key_Label;
    int i;

    memcpy(&Keypad_Temp, &joypad_cfg, sizeof(joypad_cfg));

    ecDialog = gtk_dialog_new_with_buttons("Edit controls",
                        GTK_WINDOW(pWindow),
                        GTK_DIALOG_MODAL,
                        "_OK",GTK_RESPONSE_OK,
                        "_Cancel",GTK_RESPONSE_CANCEL,
                        NULL);

    for(i = 0; i < NB_KEYS; i++) {
        Key_Label = g_strdup_printf("%s (%d)", key_names[i], Keypad_Temp[i]);
        ecKey = gtk_button_new_with_label(Key_Label);
        g_free(Key_Label);
        g_signal_connect(G_OBJECT(ecKey), "clicked", G_CALLBACK(Modify_JoyKey), GINT_TO_POINTER(i));
        gtk_box_pack_start(GTK_BOX(gtk_dialog_get_content_area(GTK_DIALOG(ecDialog))), ecKey,TRUE, FALSE, 0);
    }

    gtk_widget_show_all(gtk_dialog_get_content_area(GTK_DIALOG(ecDialog)));

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


static void GraphicsSettingsDialog(GSimpleAction *action, GVariant *parameter, gpointer user_data) {
	GtkDialog *dialog;
	GtkGrid *wGrid;
	GtkComboBox *coreCombo, *wScale, *wMultisample;
	GtkToggleButton *wPosterize, *wSmoothing, *wHCInterpolate;

	GtkBuilder *builder = gtk_builder_new_from_resource("/org/desmume/DeSmuME/graphics.ui");
	dialog = GTK_DIALOG(gtk_builder_get_object(builder, "dialog"));
	wGrid = GTK_GRID(gtk_builder_get_object(builder, "graphics_grid"));
	coreCombo = GTK_COMBO_BOX(gtk_builder_get_object(builder, "core_combo"));
	wScale = GTK_COMBO_BOX(gtk_builder_get_object(builder, "scale"));
	wMultisample = GTK_COMBO_BOX(gtk_builder_get_object(builder, "multisample"));
	wPosterize = GTK_TOGGLE_BUTTON(gtk_builder_get_object(builder, "posterize"));
	wSmoothing = GTK_TOGGLE_BUTTON(gtk_builder_get_object(builder, "smoothing"));
	wHCInterpolate = GTK_TOGGLE_BUTTON(gtk_builder_get_object(builder, "hc_interpolate"));
	g_object_unref(builder);

#ifndef HAVE_OPENGL
	gtk_combo_box_text_remove(GTK_COMBO_BOX_TEXT(coreCombo), 2);
	gtk_grid_remove_row(wGrid, 4);
#endif
	gtk_combo_box_set_active(coreCombo, cur3DCore);

	// The shift it work for scale up to 4. For scaling more than 4, a mapping function is required
	gtk_combo_box_set_active(wScale, CommonSettings.GFX3D_Renderer_TextureScalingFactor >> 1);

	// 3D Texture Deposterization
	gtk_toggle_button_set_active(wPosterize, CommonSettings.GFX3D_Renderer_TextureDeposterize);

	// 3D Texture Smoothing
	gtk_toggle_button_set_active(wSmoothing, CommonSettings.GFX3D_Renderer_TextureSmoothing);

#ifdef HAVE_OPENGL
	// OpenGL Multisample
	int currentMultisample = CommonSettings.GFX3D_Renderer_MultisampleSize;
	int currentActive = 0;
	// find smallest option that is larger than current value, i.e. round up to power of 2
	while (multisampleSizes[currentActive] < currentMultisample && currentActive < 5) { currentActive++; }
	gtk_combo_box_set_active(wMultisample, currentActive);
#endif

	// SoftRasterizer High Color Interpolation
	gtk_toggle_button_set_active(wHCInterpolate, CommonSettings.GFX3D_HighResolutionInterpolateColor);

	gtk_widget_show_all(gtk_dialog_get_content_area(dialog));

    switch (gtk_dialog_run(dialog)) {
    case GTK_RESPONSE_OK:
    // Start: OK Response block
    {
    	int sel3DCore = gtk_combo_box_get_active(coreCombo);

    	// Change only if needed
		if (sel3DCore != cur3DCore)
		{
			if (sel3DCore == 2)
			{
#if !defined(HAVE_OPENGL)
				sel3DCore = RENDERID_SOFTRASTERIZER;
#elif defined(HAVE_LIBOSMESA)
				if (!is_osmesa_initialized())
				{
					init_osmesa_3Demu();
				}
#else
				if (!is_sdl_initialized())
				{
					init_sdl_3Demu();
				}
#endif
			}
			
			if (GPU->Change3DRendererByID(sel3DCore))
			{
				config.core3D = sel3DCore;
			}
			else
			{
				GPU->Change3DRendererByID(RENDERID_SOFTRASTERIZER);
				g_printerr("3D renderer initialization failed!\nFalling back to 3D core: %s\n", core3DList[RENDERID_SOFTRASTERIZER]->name);
				config.core3D = RENDERID_SOFTRASTERIZER;
			}
		}

		size_t scale = 1;

		switch (gtk_combo_box_get_active(wScale)){
		case 1:
			scale = 2;
			break;
		case 2:
			scale = 4;
			break;
		default:
			break;
		}
		CommonSettings.GFX3D_Renderer_TextureDeposterize = config.textureDeposterize = gtk_toggle_button_get_active(wPosterize);
		CommonSettings.GFX3D_Renderer_TextureSmoothing = config.textureSmoothing = gtk_toggle_button_get_active(wSmoothing);
		CommonSettings.GFX3D_Renderer_TextureScalingFactor = config.textureUpscale = scale;
		CommonSettings.GFX3D_HighResolutionInterpolateColor = config.highColorInterpolation = gtk_toggle_button_get_active(wHCInterpolate);
#ifdef HAVE_OPENGL
		int selectedMultisample = gtk_combo_box_get_active(wMultisample);
		config.multisamplingSize = multisampleSizes[selectedMultisample];
		config.multisampling = selectedMultisample != 0;
		CommonSettings.GFX3D_Renderer_MultisampleSize = multisampleSizes[selectedMultisample];
#endif
    }
    // End: OK Response Block
        break;
    case GTK_RESPONSE_CANCEL:
    case GTK_RESPONSE_NONE:
        break;
    }

	gtk_widget_destroy(GTK_WIDGET(dialog));

}

static void ToggleLayerVisibility(GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
    guint Layer = GPOINTER_TO_UINT(user_data);

    // FIXME: make it work after resume
    if (!desmume_running())
        return;

    GVariant *variant = g_action_get_state(G_ACTION(action));
    gboolean active = !g_variant_get_boolean(variant);
    g_simple_action_set_state(action, g_variant_new_boolean(active));

    switch (Layer) {
    case MAIN_BG_0:
    case MAIN_BG_1:
    case MAIN_BG_2:
    case MAIN_BG_3:
    case MAIN_OBJ:
        GPU->GetEngineMain()->SetLayerEnableState(Layer, (active == TRUE) ? true : false);
        break;
    case SUB_BG_0:
    case SUB_BG_1:
    case SUB_BG_2:
    case SUB_BG_3:
    case SUB_OBJ:
        GPU->GetEngineSub()->SetLayerEnableState(Layer-SUB_BG_0, (active == TRUE) ? true : false);
        break;
    default:
        break;
    }
}

static void Printscreen(GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
    GdkPixbuf *screenshot;
    const gchar *dir;
    gchar *filename = NULL, *filen = NULL;
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

    dir = g_get_user_special_dir(G_USER_DIRECTORY_PICTURES);
    if (dir == NULL) {
        dir = g_get_user_special_dir(G_USER_DIRECTORY_DESKTOP);
    }
    if (dir == NULL) {
        dir = g_get_home_dir();
    }

    do {
        g_free(filen);
        g_free(filename);
        filen = g_strdup_printf("desmume-screenshot-%d.png", seq++);
        filename = g_build_filename(dir, filen, NULL);
    }
    while (g_file_test(filename, G_FILE_TEST_EXISTS));

    gdk_pixbuf_save(screenshot, filename, "png", &error, NULL);
    if (error) {
        g_error_free (error);
        g_printerr("Failed to save %s", filename);
        seq--;
    }

    //free(rgb);
    g_object_unref(screenshot);
    g_free(filename);
    g_free(filen);
}

#ifdef DESMUME_GTK_FIRMWARE_BROKEN
static void SelectFirmwareFile(GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
    GtkFileFilter *pFilter_nds, *pFilter_bin, *pFilter_any;
    GtkFileChooserNative *pFileSelection;

    BOOL oldState = desmume_running();
    Pause(NULL, NULL, NULL);

    pFilter_nds = gtk_file_filter_new();
    gtk_file_filter_add_pattern(pFilter_nds, "*.nds");
    gtk_file_filter_set_name(pFilter_nds, "Nds binary (.nds)");

    pFilter_bin = gtk_file_filter_new();
    gtk_file_filter_add_pattern(pFilter_bin, "*.bin");
    gtk_file_filter_set_name(pFilter_bin, "Binary file (.bin)");

    pFilter_any = gtk_file_filter_new();
    gtk_file_filter_add_pattern(pFilter_any, "*");
    gtk_file_filter_set_name(pFilter_any, "All files");

    pFileSelection = gtk_file_chooser_native_new("Load firmware...",
            GTK_WINDOW(pWindow),
            GTK_FILE_CHOOSER_ACTION_OPEN,
            "_Open", "_Cancel");

    gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(pFileSelection), pFilter_nds);
    gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(pFileSelection), pFilter_bin);
    gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(pFileSelection), pFilter_any);

    int response = gtk_native_dialog_run(GTK_NATIVE_DIALOG(pFileSelection));
    if (response == GTK_RESPONSE_ACCEPT) {
        GFile *file = gtk_file_chooser_get_file(GTK_FILE_CHOOSER(pFileSelection));
        gchar *sPath = g_file_get_path(file);

        CommonSettings.UseExtFirmware = true;
        strncpy(CommonSettings.ExtFirmwarePath, (const char*)sPath, g_utf8_strlen(sPath, -1));
        g_free(sPath);
    }
    g_object_unref(pFileSelection);

    if(oldState) Launch(NULL, NULL, NULL);
}
#endif

static void Modify_PriInterpolation(GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
    std::string string = g_variant_get_string(parameter, NULL);
    VideoFilterTypeID filter;
    if (string == "none")
        filter = VideoFilterTypeID_None;
    else if (string == "lq2x")
        filter = VideoFilterTypeID_LQ2X;
    else if (string == "lq2xs")
        filter = VideoFilterTypeID_LQ2XS;
    else if (string == "hq2x")
        filter = VideoFilterTypeID_HQ2X;
    else if (string == "hq2xs")
        filter = VideoFilterTypeID_HQ2XS;
    else if (string == "hq3x")
        filter = VideoFilterTypeID_HQ3X;
    else if (string == "hq3xs")
        filter = VideoFilterTypeID_HQ3XS;
    else if (string == "hq4x")
        filter = VideoFilterTypeID_HQ4X;
    else if (string == "hq4xs")
        filter = VideoFilterTypeID_HQ4XS;
    else if (string == "2xsai")
        filter = VideoFilterTypeID_2xSaI;
    else if (string == "super2xsai")
        filter = VideoFilterTypeID_Super2xSaI;
    else if (string == "supereagle")
        filter = VideoFilterTypeID_SuperEagle;
    else if (string == "scanline")
        filter = VideoFilterTypeID_Scanline;
    else if (string == "bilinear")
        filter = VideoFilterTypeID_Bilinear;
    else if (string == "nearest2x")
        filter = VideoFilterTypeID_Nearest2X;
    else if (string == "nearest_1point5x")
        filter = VideoFilterTypeID_Nearest1_5X;
    else if (string == "nearestplus_1point5x")
        filter = VideoFilterTypeID_NearestPlus1_5X;
    else if (string == "epx")
        filter = VideoFilterTypeID_EPX;
    else if (string == "epxplus")
        filter = VideoFilterTypeID_EPXPlus;
    else if (string == "epx_1point5x")
        filter = VideoFilterTypeID_EPX1_5X;
    else if (string == "epxplus_1point5x")
        filter = VideoFilterTypeID_EPXPlus1_5X;
    else if (string == "2xbrz")
        filter = VideoFilterTypeID_2xBRZ;
    else if (string == "3xbrz")
        filter = VideoFilterTypeID_3xBRZ;
    else if (string == "4xbrz")
        filter = VideoFilterTypeID_4xBRZ;
    else if (string == "5xbrz")
        filter = VideoFilterTypeID_5xBRZ;
    video->ChangeFilterByID(filter);
    config.view_filter = filter;
    RedrawScreen();
    g_simple_action_set_state(action, parameter);
}

static void Modify_Interpolation(GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
    std::string string = g_variant_get_string(parameter, NULL);
    cairo_filter_t filter;
    if (string == "fast")
        filter = CAIRO_FILTER_FAST;
    else if (string == "good")
        filter = CAIRO_FILTER_GOOD;
    else if (string == "best")
        filter = CAIRO_FILTER_BEST;
    else if (string == "nearest")
        filter = CAIRO_FILTER_NEAREST;
    else if (string == "bilinear")
        filter = CAIRO_FILTER_BILINEAR;
    else if (string == "gaussian")
        filter = CAIRO_FILTER_GAUSSIAN;
    Interpolation = filter;
    config.view_cairoFilter = Interpolation;
    RedrawScreen();
    g_simple_action_set_state(action, parameter);
}

static void Modify_SPUMode(GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
    const char *string = g_variant_get_string(parameter, NULL);
    spumode_enum mode;
    if (strcmp(string, "dual-async") == 0)
        mode = SPUMODE_DUALASYNC;
    else if (strcmp(string, "sync-n") == 0)
        mode = SPUMODE_SYNCN;
    else if (strcmp(string, "sync-z") == 0)
        mode = SPUMODE_SYNCZ;
#ifdef HAVE_LIBSOUNDTOUCH
    else if (strcmp(string, "sync-p") == 0)
        mode = SPUMODE_SYNCP;
#endif

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
    g_simple_action_set_state(action, parameter);
}

static void Modify_SPUInterpolation(GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
    const char *string = g_variant_get_string(parameter, NULL);
    SPUInterpolationMode mode;
    if (strcmp(string, "none") == 0)
        mode = SPUInterpolation_None;
    else if (strcmp(string, "linear") == 0)
        mode = SPUInterpolation_Linear;
    else if (strcmp(string, "cosine") == 0)
        mode = SPUInterpolation_Cosine;
    CommonSettings.spuInterpolationMode = mode;
    config.audio_interpolation = CommonSettings.spuInterpolationMode;
    g_simple_action_set_state(action, parameter);
}

static void Modify_Frameskip(GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
    const char *string = g_variant_get_string(parameter, NULL);
    autoFrameskipMax = g_ascii_strtoll(string, NULL, 10);
    config.frameskip = autoFrameskipMax;
    if (!autoframeskip) {
        Frameskip = autoFrameskipMax;
    }
}

/////////////////////////////// TOOLS MANAGEMENT ///////////////////////////////

extern const dTool_t *dTools_list[];
extern const int dTools_list_size;

BOOL *dTools_running;

static void Start_dTool(GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
    int tool = g_variant_get_uint32(parameter);

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

static void DoQuit(GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
    emu_halt(EMUHALT_REASON_USER_REQUESTED_HALT, NDSErrorTag_None);
    if (regMainLoop) {
        g_source_remove(regMainLoop);
        regMainLoop = 0;
    }
    gtk_window_close(GTK_WINDOW(pWindow));
}


gboolean EmuLoop(gpointer data)
{
    static Uint32 fps_SecStart, next_fps_SecStart, fps_FrameCount, skipped_frames;
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
      if (regMainLoop) {
          g_source_remove(regMainLoop);
          regMainLoop = 0;
      }
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
	Hud.fps3d = GPU->GetFPSRender3D();
	
	if(nds.idleFrameCounter==0 || oneSecond) 
	{
		u32 loadAvgARM9;
		u32 loadAvgARM7;
		NDS_GetCPULoadAverage(loadAvgARM9, loadAvgARM7);
		
		Hud.cpuload[ARMCPU_ARM9] = (int)loadAvgARM9;
		Hud.cpuload[ARMCPU_ARM7] = (int)loadAvgARM7;

	}
	Hud.cpuloopIterationCount = nds.cpuloopIterationCount;
#endif

    /* Merge the joystick keys with the keyboard ones */
    process_joystick_events(&keys_latch);
    /* Update! */
    update_keypad(keys_latch);

    desmume_cycle();    /* Emule ! */

    _updateDTools();
        avout_x264.updateVideo((const uint16_t *)GPU->GetDisplayInfo().masterNativeBuffer);
	RedrawScreen();

    if (!config.fpslimiter || keys_latch & KEYMASK_(KEY_BOOST - 1)) {
        if (autoframeskip) {
            Frameskip = 0;
        } else {
            for (i = 0; i < Frameskip; i++) {
                NDS_SkipNextFrame();
#ifdef HAVE_LIBAGG
                Hud.fps3d = GPU->GetFPSRender3D();
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
                Hud.fps3d = GPU->GetFPSRender3D();
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
                Hud.fps3d = GPU->GetFPSRender3D();
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

GMenuModel *savestates_menu;
GMenuModel *loadstates_menu;

// The following functions are adapted from the Windows port:
//     UpdateSaveStateMenu, ResetSaveStateTimes, LoadSaveStateInfo
static void UpdateSaveStateMenu(int pos, char* txt)
{
    GMenuItem *item;
    gint position = ((pos == 0) ? 10 : pos) - 1;

    item = g_menu_item_new_from_model(savestates_menu, position);
    g_menu_item_set_label(item, txt);
    g_menu_remove(G_MENU(savestates_menu), position);
    g_menu_insert_item(G_MENU(savestates_menu), position, item);

    item = g_menu_item_new_from_model(loadstates_menu, position);
    g_menu_item_set_label(item, txt);
    g_menu_remove(G_MENU(loadstates_menu), position);
    g_menu_insert_item(G_MENU(loadstates_menu), position, item);
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

static void changesavetype(GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
    const char *string = g_variant_get_string(parameter, NULL);
    int savetype;
    if (strcmp(string, "autodetect") == 0)
        savetype = 0;
    else if (strcmp(string, "eeprom-4k") == 0)
        savetype = 1;
    else if (strcmp(string, "eeprom-64k") == 0)
        savetype = 2;
    else if (strcmp(string, "eeprom-512k") == 0)
        savetype = 3;
    else if (strcmp(string, "fram-256k") == 0)
        savetype = 4;
    else if (strcmp(string, "flash-2m") == 0)
        savetype = 5;
    else if (strcmp(string, "flash-4m") == 0)
        savetype = 6;
    backup_setManualBackupType(savetype);
    config.savetype=savetype;
    g_simple_action_set_state(action, parameter);
}

static void desmume_gtk_menu_tool_layers(GtkApplication *app)
{
    const char *Layers_Menu[10][3] = {
        {"0", "layermainbg0","_0 Main BG 0"},
        {"1", "layermainbg1","_1 Main BG 1"},
        {"2", "layermainbg2","_2 Main BG 2"},
        {"3", "layermainbg3","_3 Main BG 3"},
        {"4", "layermainobj","_4 Main OBJ"},
        {"5", "layersubbg0", "_5 SUB BG 0"},
        {"6", "layersubbg1", "_6 SUB BG 1"},
        {"7", "layersubbg2", "_7 SUB BG 2"},
        {"8", "layersubbg3", "_8 SUB BG 3"},
        {"9", "layersubobj", "_9 SUB OBJ"}
    };

    std::vector<GActionEntry> entries;
    for (int i = 0; i < 10; i++) {
        GActionEntry entry = {Layers_Menu[i][1], ToggleLayerVisibility, NULL, "true", NULL};
        entries.push_back(entry);
    }
    g_action_map_add_action_entries(G_ACTION_MAP(app), entries.data(), entries.size(), NULL);
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

static void ToggleHudDisplay(hud_display_enum hudId, gboolean active)
{
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

#define HudMacro(func_name, enum_value) \
static void func_name(GSimpleAction *action, GVariant *parameter, gpointer user_data) \
{ \
    GVariant *variant = g_action_get_state(G_ACTION(action)); \
    gboolean value = !g_variant_get_boolean(variant); \
    g_simple_action_set_state(action, g_variant_new_boolean(value)); \
    ToggleHudDisplay(enum_value, value); \
}

HudMacro(HudFps, HUD_DISPLAY_FPS)
HudMacro(HudInput, HUD_DISPLAY_INPUT)
HudMacro(HudGraphicalInput, HUD_DISPLAY_GINPUT)
HudMacro(HudFrameCounter, HUD_DISPLAY_FCOUNTER)
HudMacro(HudLagCounter, HUD_DISPLAY_LCOUNTER)
HudMacro(HudRtc, HUD_DISPLAY_RTC)
HudMacro(HudMic, HUD_DISPLAY_MIC)
HudMacro(HudEditor, HUD_DISPLAY_EDITOR)

static void desmume_gtk_menu_view_hud(GtkApplication *app)
{
    const struct {
        const gchar* name;
        bool active;
        bool& setting;
    } hud_menu[] = {
        { "hud_fps", config.hud_fps, CommonSettings.hud.FpsDisplay },
        { "hud_input", config.hud_input, CommonSettings.hud.ShowInputDisplay },
        { "hud_graphicalinput", config.hud_graphicalInput, CommonSettings.hud.ShowGraphicalInputDisplay },
        { "hud_framecounter", config.hud_frameCounter, CommonSettings.hud.FrameCounterDisplay },
        { "hud_lagcounter", config.hud_lagCounter, CommonSettings.hud.ShowLagFrameCounter },
        { "hud_rtc", config.hud_rtc, CommonSettings.hud.ShowRTC },
        { "hud_mic", config.hud_mic, CommonSettings.hud.ShowMicrophone },
    };
    guint i;

    for(i = 0; i < sizeof(hud_menu) / sizeof(hud_menu[0]); i++){
        GSimpleAction *action = G_SIMPLE_ACTION(g_action_map_lookup_action(G_ACTION_MAP(app), hud_menu[i].name));
        g_simple_action_set_state(action, g_variant_new_boolean(hud_menu[i].active ? TRUE : FALSE));
        hud_menu[i].setting = hud_menu[i].active;
    }
}
#endif

static void ToggleAudio(GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
    GVariant *variant = g_action_get_state(G_ACTION(action));
    gboolean value = !g_variant_get_boolean(variant);
    config.audio_enabled = value;
    if (config.audio_enabled) {
        SPU_ChangeSoundCore(SNDCORE_SDL, 735 * 4);
        driver->AddLine("Audio enabled");
    } else {
        SPU_ChangeSoundCore(0, 0);
        driver->AddLine("Audio disabled");
    }
    RedrawScreen();
    g_simple_action_set_state(action, g_variant_new_boolean(value));
}

#ifdef FAKE_MIC
static void ToggleMicNoise(GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
    GVariant *variant = g_action_get_state(G_ACTION(action));
    gboolean doNoise = !g_variant_get_boolean(variant);

    Mic_DoNoise(doNoise);
    if (doNoise)
       driver->AddLine("Fake mic enabled");
    else
       driver->AddLine("Fake mic disabled");
    RedrawScreen();
    g_simple_action_set_state(action, g_variant_new_boolean(doNoise));
}
#endif

static void ToggleFpsLimiter(GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
    GVariant *variant = g_action_get_state(G_ACTION(action));
    gboolean value = !g_variant_get_boolean(variant);
    config.fpslimiter = value;

    if (config.fpslimiter)
       driver->AddLine("Fps limiter enabled");
    else
       driver->AddLine("Fps limiter disabled");
    RedrawScreen();
    g_simple_action_set_state(action, g_variant_new_boolean(value));
}

static void ToggleAutoFrameskip(GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
    GVariant *variant = g_action_get_state(G_ACTION(action));
    gboolean value = !g_variant_get_boolean(variant);
    config.autoframeskip = value;

    if (config.autoframeskip) {
        autoframeskip = true;
        Frameskip = 0;
        driver->AddLine("Auto frameskip enabled");
    } else {
        autoframeskip = false;
        Frameskip = autoFrameskipMax;
        driver->AddLine("Auto frameskip disabled");
    }
    RedrawScreen();
    g_simple_action_set_state(action, g_variant_new_boolean(value));
}

static void desmume_gtk_menu_tools(GtkApplication *app)
{
    std::vector<GActionEntry> entries;
    for (int i = 0; i < dTools_list_size; i++) {
        GActionEntry entry = {dTools_list[i]->shortname, Start_dTool, "u", std::to_string(i).c_str(), NULL};
        entries.push_back(entry);
    }
    g_action_map_add_action_entries(G_ACTION_MAP(app), entries.data(), entries.size(), NULL);
}

static gboolean timeout_exit_cb(gpointer data)
{
    DoQuit(NULL, NULL, NULL);
    INFO("Quit after %d seconds timeout\n", GPOINTER_TO_INT(data));

    return FALSE;
}

static void
common_gtk_main(GApplication *app, gpointer user_data)
{
    configured_features *my_config = static_cast<configured_features*>(user_data);

	config.load();

    driver = new GtkDriver();

    SDL_TimerID limiter_timer = 0;

    /* use any language set on the command line */
    if ( my_config->firmware_language != -1) {
		CommonSettings.fwConfig.language = my_config->firmware_language;
    }

    /* if the command line overriding is enabled then use the language set on the GUI */
    if(config.command_line_overriding_firmware_language)
		CommonSettings.fwConfig.language = config.firmware_language;

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
    SDL_SetHint(SDL_HINT_JOYSTICK_ALLOW_BACKGROUND_EVENTS,"1");
    if(SDL_Init(SDL_INIT_TIMER|SDL_INIT_VIDEO) == -1) {
        g_printerr("Error trying to initialize SDL: %s\n",
                    SDL_GetError());
        // TODO: return a non-zero exit status.
        g_application_quit(app);
    }
    desmume_init( my_config->disable_sound || !config.audio_enabled);

    /* Init the hud / osd stuff */
#ifdef HAVE_LIBAGG
    Desmume_InitOnce();
    Hud.reset();
    osd = new OSDCLASS(-1);
#endif

    /*
     * Activate the GDB stubs
     * This has to come after the NDS_Init (called in desmume_init)
     * where the cpus are set up.
     */
#ifdef GDB_STUB
    gdbstub_mutex_init();

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

    /* Initialize joysticks */
    if(!init_joy())
        // TODO: return a non-zero exit status.
        g_application_quit(app);

    dTools_running = (BOOL*)malloc(sizeof(BOOL) * dTools_list_size);
    if (dTools_running != NULL) 
      memset(dTools_running, FALSE, sizeof(BOOL) * dTools_list_size); 

    keyfile = desmume_config_read_file(gtk_kb_cfg);

    memset(&nds_screen, 0, sizeof(nds_screen));
    nds_screen.orientation = ORIENT_VERTICAL;

    g_printerr("Using %d threads for video filter.\n", CommonSettings.num_cores);
    video = new VideoFilter(256, 384, VideoFilterTypeID_None, CommonSettings.num_cores);

    /* Fetch the main elements from the window */
    GtkBuilder *builder = gtk_builder_new_from_resource("/org/desmume/DeSmuME/main.ui");
    pWindow = GTK_WIDGET(gtk_builder_get_object(builder, "window"));
    pToolBar = GTK_WIDGET(gtk_builder_get_object(builder, "toolbar"));
    pDrawingArea = GTK_WIDGET(gtk_builder_get_object(builder, "drawing-area"));
    pStatusBar = GTK_WIDGET(gtk_builder_get_object(builder, "status-bar"));
    g_object_unref(builder);

    gtk_application_add_window(GTK_APPLICATION(app), GTK_WINDOW(pWindow));

    g_signal_connect(G_OBJECT(pWindow), "destroy", G_CALLBACK(DoQuit), NULL);
    g_signal_connect(G_OBJECT(pWindow), "key_press_event", G_CALLBACK(Key_Press), NULL);
    g_signal_connect(G_OBJECT(pWindow), "key_release_event", G_CALLBACK(Key_Release), NULL);

    /* Update audio toggle status */
    if (my_config->disable_sound || !config.audio_enabled) {
        g_simple_action_set_state(G_SIMPLE_ACTION(g_action_map_lookup_action(G_ACTION_MAP(app), "enableaudio")), g_variant_new_boolean(FALSE));
    }
    desmume_gtk_menu_tool_layers(GTK_APPLICATION(app));
#ifdef HAVE_LIBAGG
    desmume_gtk_menu_view_hud(GTK_APPLICATION(app));
#endif
    desmume_gtk_menu_tools(GTK_APPLICATION(app));
    my_config->savetype=config.savetype;
    std::string string;
    switch (my_config->savetype) {
        case 0:
            string = "autodetect";
            break;
        case 1:
            string = "eeprom-4k";
            break;
        case 2:
            string = "eeprom-64k";
            break;
        case 3:
            string = "eeprom-512k";
            break;
        case 4:
            string = "fram-256k";
            break;
        case 5:
            string = "flash-2m";
            break;
        case 6:
            string = "flash-4m";
            break;
    }
    g_simple_action_set_state(G_SIMPLE_ACTION(g_action_map_lookup_action(G_ACTION_MAP(app), "savetype")), g_variant_new_string(string.c_str()));

    if (config.view_cairoFilter < CAIRO_FILTER_FAST || config.view_cairoFilter > CAIRO_FILTER_BILINEAR) {
    	config.view_cairoFilter = CAIRO_FILTER_NEAREST;
    }
    Interpolation = (cairo_filter_t)config.view_cairoFilter.get();
    switch (Interpolation) {
        case CAIRO_FILTER_FAST:
            string = "fast";
            break;
        case CAIRO_FILTER_NEAREST:
            string = "nearest";
            break;
        case CAIRO_FILTER_GOOD:
            string = "good";
            break;
        case CAIRO_FILTER_BILINEAR:
            string = "bilinear";
            break;
        case CAIRO_FILTER_BEST:
            string = "best";
            break;
    }
    g_simple_action_set_state(G_SIMPLE_ACTION(g_action_map_lookup_action(G_ACTION_MAP(app), "interpolation")), g_variant_new_string(string.c_str()));

    if (config.view_filter < VideoFilterTypeID_None || config.view_filter >= VideoFilterTypeIDCount) {
        config.view_filter = VideoFilterTypeID_None;
    }
    video->ChangeFilterByID((VideoFilterTypeID)config.view_filter.get());
    switch (config.view_filter) {
		case VideoFilterTypeID_None:
			string = "none";
			break;
		case VideoFilterTypeID_LQ2X:
			string = "lq2x";
			break;
		case VideoFilterTypeID_LQ2XS:
			string = "lq2xs";
			break;
		case VideoFilterTypeID_HQ2X:
			string = "hq2x";
			break;
		case VideoFilterTypeID_HQ2XS:
			string = "hq2xs";
			break;
		case VideoFilterTypeID_HQ3X:
			string = "hq3x";
			break;
		case VideoFilterTypeID_HQ3XS:
			string = "hq3xs";
			break;
		case VideoFilterTypeID_HQ4X:
			string = "hq4x";
			break;
		case VideoFilterTypeID_HQ4XS:
			string = "hq4xs";
			break;
		case VideoFilterTypeID_2xSaI:
			string = "2xsai";
			break;
		case VideoFilterTypeID_Super2xSaI:
			string = "super2xsai";
			break;
		case VideoFilterTypeID_SuperEagle:
			string = "supereagle";
			break;
		case VideoFilterTypeID_Scanline:
			string = "scanline";
			break;
		case VideoFilterTypeID_Bilinear:
			string = "bilinear";
			break;
		case VideoFilterTypeID_Nearest2X:
			string = "nearest2x";
			break;
		case VideoFilterTypeID_Nearest1_5X:
			string = "nearest_1point5x";
			break;
		case VideoFilterTypeID_NearestPlus1_5X:
			string = "nearestplus_1point5x";
			break;
		case VideoFilterTypeID_EPX:
			string = "epx";
			break;
		case VideoFilterTypeID_EPXPlus:
			string = "epxplus";
			break;
		case VideoFilterTypeID_EPX1_5X:
			string = "epx_1point5x";
			break;
		case VideoFilterTypeID_EPXPlus1_5X:
			string = "epxplus_1point5x";
			break;
		case VideoFilterTypeID_2xBRZ:
			string = "2xbrz";
			break;
		case VideoFilterTypeID_3xBRZ:
			string = "3xbrz";
			break;
		case VideoFilterTypeID_4xBRZ:
			string = "4xbrz";
			break;
		case VideoFilterTypeID_5xBRZ:
			string = "5xbrz";
			break;
    }
    g_simple_action_set_state(G_SIMPLE_ACTION(g_action_map_lookup_action(G_ACTION_MAP(app), "pri_interpolation")), g_variant_new_string(string.c_str()));

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
    switch (config.audio_sync) {
    case SPUMODE_SYNCN:
        string = "sync-n";
        break;
    case SPUMODE_SYNCZ:
        string = "sync-z";
        break;
#ifdef HAVE_LIBSOUNDTOUCH
    case SPUMODE_SYNCP:
        string = "sync-p";
        break;
#endif
    case SPUMODE_DUALASYNC:
    default:
        string = "dual-async";
        break;
    }
    g_simple_action_set_state(G_SIMPLE_ACTION(g_action_map_lookup_action(G_ACTION_MAP(app), "spu_mode")), g_variant_new_string(string.c_str()));

    CommonSettings.spuInterpolationMode = (SPUInterpolationMode)(config.audio_interpolation.get());
    switch (CommonSettings.spuInterpolationMode) {
        case SPUInterpolation_None:
            string = "none";
            break;
        case SPUInterpolation_Linear:
            string = "linear";
            break;
        case SPUInterpolation_Cosine:
            string = "cosine";
            break;
    }
    g_simple_action_set_state(G_SIMPLE_ACTION(g_action_map_lookup_action(G_ACTION_MAP(app), "spu_interpolation")), g_variant_new_string(string.c_str()));

    string = std::to_string(config.frameskip);
    g_simple_action_set_state(G_SIMPLE_ACTION(g_action_map_lookup_action(G_ACTION_MAP(app), "frameskip")), g_variant_new_string(string.c_str()));
    autoFrameskipMax = config.frameskip;
    g_simple_action_set_state(G_SIMPLE_ACTION(g_action_map_lookup_action(G_ACTION_MAP(app), "frameskipA")), g_variant_new_boolean(config.autoframeskip));
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
    string = std::to_string(nds_screen.rotation_angle);
    g_simple_action_set_state(G_SIMPLE_ACTION(g_action_map_lookup_action(G_ACTION_MAP(app), "rotate")), g_variant_new_string(string.c_str()));


    if (config.window_scale < WINSIZE_SCALE || config.window_scale > WINSIZE_5) {
    	config.window_scale = WINSIZE_SCALE;
    }
    winsize_current = (winsize_enum)config.window_scale.get();
    switch (winsize_current) {
        case WINSIZE_SCALE:
            string = "scale";
            break;
        case WINSIZE_HALF:
            string = "0.5";
            break;
        case WINSIZE_1:
            string = "1";
            break;
        case WINSIZE_1HALF:
            string = "1.5";
            break;
        case WINSIZE_2:
            string = "2";
            break;
        case WINSIZE_2HALF:
            string = "2.5";
            break;
        case WINSIZE_3:
            string = "3";
            break;
        case WINSIZE_4:
            string = "4";
            break;
        case WINSIZE_5:
            string = "5";
            break;
    }
    g_simple_action_set_state(G_SIMPLE_ACTION(g_action_map_lookup_action(G_ACTION_MAP(app), "winsize")), g_variant_new_string(string.c_str()));

    if (config.view_orient < ORIENT_VERTICAL || config.view_orient > ORIENT_SINGLE) {
        config.view_orient = ORIENT_VERTICAL;
    }
    nds_screen.orientation = (orientation_enum)config.view_orient.get();
    switch (nds_screen.orientation) {
        case ORIENT_VERTICAL:
            string = "vertical";
            break;
        case ORIENT_HORIZONTAL:
            string = "horizontal";
            break;
        case ORIENT_SINGLE:
            string = "single";
            break;
    }
    g_simple_action_set_state(G_SIMPLE_ACTION(g_action_map_lookup_action(G_ACTION_MAP(app), "orient")), g_variant_new_string(string.c_str()));

    g_simple_action_set_enabled(G_SIMPLE_ACTION(g_action_map_lookup_action(G_ACTION_MAP(app), "pause")), FALSE);
    g_simple_action_set_enabled(G_SIMPLE_ACTION(g_action_map_lookup_action(G_ACTION_MAP(app), "run")), FALSE);
    g_simple_action_set_enabled(G_SIMPLE_ACTION(g_action_map_lookup_action(G_ACTION_MAP(app), "reset")), FALSE);
    g_simple_action_set_enabled(G_SIMPLE_ACTION(g_action_map_lookup_action(G_ACTION_MAP(app), "printscreen")), FALSE);
    g_simple_action_set_enabled(G_SIMPLE_ACTION(g_action_map_lookup_action(G_ACTION_MAP(app), "cheatlist")), FALSE);
    g_simple_action_set_enabled(G_SIMPLE_ACTION(g_action_map_lookup_action(G_ACTION_MAP(app), "cheatsearch")), FALSE);
    g_simple_action_set_enabled(G_SIMPLE_ACTION(g_action_map_lookup_action(G_ACTION_MAP(app), "importbackup")), FALSE);
    g_simple_action_set_enabled(G_SIMPLE_ACTION(g_action_map_lookup_action(G_ACTION_MAP(app), "exportbackup")), FALSE);

    nds_screen.gap_size = config.view_gap ? GAP_SIZE : 0;

    nds_screen.swap = config.view_swap;
    g_simple_action_set_state(G_SIMPLE_ACTION(g_action_map_lookup_action(G_ACTION_MAP(app), "swapscreens")), g_variant_new_boolean(config.view_swap));

    builder = gtk_builder_new_from_resource("/org/desmume/DeSmuME/menu.ui");
    GMenuModel *menubar = G_MENU_MODEL(gtk_builder_get_object(builder, "menubar"));
    GMenuModel *open_recent_menu = G_MENU_MODEL(gtk_builder_get_object(builder, "open_recent"));
    savestates_menu = G_MENU_MODEL(gtk_builder_get_object(builder, "savestates"));
    loadstates_menu = G_MENU_MODEL(gtk_builder_get_object(builder, "loadstates"));
    GMenu *config_menu = G_MENU(gtk_builder_get_object(builder, "config"));

#ifdef FAKE_MIC
    GMenuItem *mic_noise = g_menu_item_new("Fake mic _noise", "app.micnoise");
    g_menu_insert_item(config_menu, 3, mic_noise);
#endif

#ifdef HAVE_JIT
    GMenuItem *emulation_settings = g_menu_item_new("Em_ulation Settings", "app.emulationsettings");
    g_menu_insert_item(config_menu, 1, emulation_settings);
#endif

#ifdef HAVE_LIBSOUNDTOUCH
    GMenu *audio_sync_menu = G_MENU(gtk_builder_get_object(builder, "audio-synchronisation"));
    GMenuItem *sync_p = g_menu_item_new("Synchronous (_P)", "app.spu_mode(\"sync-p\")");
    g_menu_append_item(audio_sync_menu, sync_p);
#endif

    GMenu *view_menu = G_MENU(gtk_builder_get_object(builder, "view"));
    GMenuModel *hud = G_MENU_MODEL(gtk_builder_get_object(builder,
#ifdef HAVE_LIBAGG
        "hud_supported"
#else
        "hud_unsupported"
#endif
    ));
    g_menu_append_submenu(view_menu, "_HUD", hud);

    gtk_application_set_menubar(GTK_APPLICATION(app), menubar);
    g_object_unref(builder);
    pApp = GTK_APPLICATION(app);

    GtkRecentManager *manager = gtk_recent_manager_get_default();
    GList *items = gtk_recent_manager_get_items(manager);
    g_list_foreach(items, [](gpointer data, gpointer user_data) {
        // TODO: Why is there no GTK_RECENT_INFO()?!
        GtkRecentInfo *info = static_cast<GtkRecentInfo*>(data);
        const char *mime = gtk_recent_info_get_mime_type(info);
        if (strcmp(mime, "application/x-nintendo-ds-rom") != 0) {
            gtk_recent_info_unref(info);
            return;
        }

        const char *uri = gtk_recent_info_get_uri(info);
        const char *name = gtk_recent_info_get_display_name(info);
        // TODO: Is that enough?  Maybe allocate instead?
        char action[1024];
        sprintf(action, "app.recent('%s')", uri);
        g_menu_append(G_MENU(user_data), name, action);

        gtk_recent_info_unref(info);
    }, open_recent_menu);
    g_list_free(items);

    /* This toggle action must not be set active before the pDrawingArea initialization to avoid a GTK warning */
    g_simple_action_set_state(G_SIMPLE_ACTION(g_action_map_lookup_action(G_ACTION_MAP(app), "gap")), g_variant_new_boolean(config.view_gap));

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
    g_signal_connect(G_OBJECT(pDrawingArea), "draw",
                     G_CALLBACK(ExposeDrawingArea), NULL ) ;
    g_signal_connect(G_OBJECT(pDrawingArea), "configure_event",
                     G_CALLBACK(ConfigureDrawingArea), NULL ) ;

    /* Status bar */
    UpdateStatusBar(EMU_DESMUME_NAME_AND_VERSION());

    gtk_widget_show_all(pWindow);

	if (winsize_current == WINSIZE_SCALE) {
		if (config.window_fullscreen) {
            gtk_application_window_set_show_menubar(GTK_APPLICATION_WINDOW(pWindow), FALSE);
			gtk_widget_hide(pToolBar);
			gtk_widget_hide(pStatusBar);
            g_simple_action_set_enabled(G_SIMPLE_ACTION(g_action_map_lookup_action(G_ACTION_MAP(app), "view_menu")), FALSE);
            g_simple_action_set_enabled(G_SIMPLE_ACTION(g_action_map_lookup_action(G_ACTION_MAP(app), "view_toolbar")), FALSE);
            g_simple_action_set_enabled(G_SIMPLE_ACTION(g_action_map_lookup_action(G_ACTION_MAP(app), "view_statusbar")), FALSE);
			gtk_window_fullscreen(GTK_WINDOW(pWindow));
		}
	} else {
		config.window_fullscreen = false;
        g_simple_action_set_enabled(G_SIMPLE_ACTION(g_action_map_lookup_action(G_ACTION_MAP(app), "fullscreen")), FALSE);
	}
	if (!config.view_menu) {
        gtk_application_window_set_show_menubar(GTK_APPLICATION_WINDOW(pWindow), FALSE);
        g_simple_action_set_state(G_SIMPLE_ACTION(g_action_map_lookup_action(G_ACTION_MAP(app), "view_menu")), g_variant_new_boolean(FALSE));
	}
	if (!config.view_toolbar) {
		gtk_widget_hide(pToolBar);
        g_simple_action_set_state(G_SIMPLE_ACTION(g_action_map_lookup_action(G_ACTION_MAP(app), "view_toolbar")), g_variant_new_boolean(FALSE));
	}
	if (!config.view_statusbar) {
		gtk_widget_hide(pStatusBar);
        g_simple_action_set_state(G_SIMPLE_ACTION(g_action_map_lookup_action(G_ACTION_MAP(app), "view_statusbar")), g_variant_new_boolean(FALSE));
	}
    UpdateDrawingAreaAspect();

    if (my_config->disable_limiter || !config.fpslimiter) {
        config.fpslimiter = false;
        g_simple_action_set_state(G_SIMPLE_ACTION(g_action_map_lookup_action(G_ACTION_MAP(app), "enablefpslimiter")), g_variant_new_boolean(FALSE));
    }

#if defined(HAVE_OPENGL) && defined(OGLRENDER_3_2_H)
    OGLLoadEntryPoints_3_2_Func = OGLLoadEntryPoints_3_2;
    OGLCreateRenderer_3_2_Func = OGLCreateRenderer_3_2;
#endif

    //Set the 3D emulation to use
    int core = my_config->engine_3d;
    // setup the gdk 3D emulation;

    // Check if commandLine argument was passed or not
    if (core == -1) {
    	// If it was not passed, then get the Renderer config from the file
    	core = config.core3D;

    	// Check if it is valid
    	if (!(core >= 0 && core <= 2)) {
    		// If it is invalid, reset it to SoftRasterizer
    		core = 1;
    	}
    	//Set this too for clarity
        my_config->engine_3d = core;
    }

	if (core == 2)
	{
#if !defined(HAVE_OPENGL)
		core = RENDERID_SOFTRASTERIZER;
#elif defined(HAVE_LIBOSMESA)
		if (!is_osmesa_initialized())
		{
			init_osmesa_3Demu();
		}
#else
		if (!is_sdl_initialized())
		{
			init_sdl_3Demu();
		}
#endif
	}

	if (!GPU->Change3DRendererByID(core)) {
		GPU->Change3DRendererByID(RENDERID_SOFTRASTERIZER);
		g_printerr("3D renderer initialization failed!\nFalling back to 3D core: %s\n", core3DList[RENDERID_SOFTRASTERIZER]->name);
		my_config->engine_3d = RENDERID_SOFTRASTERIZER;
	}

    CommonSettings.GFX3D_HighResolutionInterpolateColor = config.highColorInterpolation;
	CommonSettings.GFX3D_Renderer_MultisampleSize = (config.multisamplingSize > 0)
														? config.multisamplingSize
														: config.multisampling ? 4 : 0;
    CommonSettings.GFX3D_Renderer_TextureDeposterize = config.textureDeposterize;
    CommonSettings.GFX3D_Renderer_TextureScalingFactor = (config.textureUpscale == 1 ||
    														config.textureUpscale == 2 ||
															config.textureUpscale == 4)
															? config.textureUpscale : 1 ;
    CommonSettings.GFX3D_Renderer_TextureSmoothing = config.textureSmoothing;

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

            Launch(NULL, NULL, NULL);
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

    SNDSDLSetAudioVolume(config.audio_volume);

    /* Video filter parameters */
    video->SetFilterParameteri(VF_PARAM_SCANLINE_A, _scanline_filter_a);
    video->SetFilterParameteri(VF_PARAM_SCANLINE_B, _scanline_filter_b);
    video->SetFilterParameteri(VF_PARAM_SCANLINE_C, _scanline_filter_c);
    video->SetFilterParameteri(VF_PARAM_SCANLINE_D, _scanline_filter_d);

	RedrawScreen();
}

static void Teardown() {
    delete video;

	config.save();
	avout_x264.end();
	avout_flac.end();

    desmume_free();

#if defined(HAVE_LIBOSMESA)
	deinit_osmesa_3Demu();
#else
	deinit_sdl_3Demu();
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
}

static void
handle_open(GApplication *application,
            GFile **files,
            gint n_files,
            const gchar *hint,
            gpointer user_data)
{
    configured_features *my_config = static_cast<configured_features*>(user_data);
    my_config->nds_file = g_file_get_path(files[0]);
    common_gtk_main(application, user_data);
}

int main (int argc, char *argv[])
{
  configured_features my_config;

  // The global menu screws up the window size...
  unsetenv("UBUNTU_MENUPROXY");


  my_config.parse(argc, argv);
  init_configured_features( &my_config);

  /* X11 multi-threading support */
  if(!XInitThreads())
    {
      fprintf(stderr, "Warning: X11 not thread-safe\n");
    }

  // TODO: pass G_APPLICATION_HANDLES_COMMAND_LINE instead.
  GtkApplication *app = gtk_application_new("org.desmume.DeSmuME", G_APPLICATION_HANDLES_OPEN);
  g_signal_connect (app, "activate", G_CALLBACK(common_gtk_main), &my_config);
  g_signal_connect (app, "open", G_CALLBACK(handle_open), &my_config);
  g_action_map_add_action_entries(G_ACTION_MAP(app),
                                  app_entries, G_N_ELEMENTS(app_entries),
                                  app);

  if ( !fill_configured_features( &my_config, argv)) {
    exit(0);
  }
#ifdef HAVE_JIT
  config.load();
  CommonSettings.jit_max_block_size=config.jit_max_block_size;
  CommonSettings.use_jit=config.use_jit;
  arm_jit_sync();
  arm_jit_reset(CommonSettings.use_jit);
#endif
  int ret = g_application_run(G_APPLICATION(app), argc, argv);
  Teardown();
  return ret;
}
