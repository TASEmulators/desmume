/* main.c - this file is part of DeSmuME
 *
 * Copyright (C) 2006,2007 DeSmuME Team
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
#include <SDL.h>

#include "types.h"
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

#ifdef GDB_STUB
#include "gdbstub.h"
#endif

#if defined(HAVE_LIBOSMESA)
#include <GL/gl.h>
#include <GL/glu.h>
#include "OGLRender.h"
#ifdef HAVE_LIBOSMESA
#include "osmesa_3Demu.h"
#endif
#endif

#include "DeSmuME.xpm"

#define EMULOOP_PRIO (G_PRIORITY_HIGH_IDLE + 20)

#include <gdk/gdkkeysyms.h>

static int backupmemorytype=MC_TYPE_AUTODETECT;
static u32 backupmemorysize=1;

static const char *bad_glob_cflash_disk_image_file;

#define SCREENS_PIXEL_SIZE 98304

#define FPS_LIMITER_FRAME_PERIOD 8
static SDL_sem *fps_limiter_semaphore;
static int gtk_fps_limiter_disabled;

const u16 gtk_kb_cfg[NB_KEYS] =
  {
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

/************************ CONFIG FILE *****************************/

// extern char FirmwareFile[256];
// int LoadFirmware(const char *filename);

static void Open_Select();
static void Launch();
static void Pause();
static void Printscreen();
static void Reset();
static void Edit_Controls();
static void MenuSave(GtkMenuItem *item, gpointer slot);
static void MenuLoad(GtkMenuItem *item, gpointer slot);
static void About();//GtkWidget* widget, gpointer data);
static void desmume_gtk_disable_audio (GtkToggleAction *action);
static void Modify_Layer(GtkToggleAction* action, gpointer data);

static const char *ui_description =
"<ui>"
"  <menubar name='MainMenu'>"
"    <menu action='FileMenu'>"
"      <menuitem action='open'/>"
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
"      <menuitem action='printscreen'/>"
"      <menuitem action='quit'/>"
"    </menu>"
"    <menu action='EmulationMenu'>"
"      <menuitem action='run'/>"
"      <menuitem action='pause'/>"
"      <menuitem action='reset'/>"
"      <menuitem action='enableaudio'/>"
"      <menu action='FrameskipMenu'>"
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
"      <menuitem action='editctrls'/>"
"    </menu>"
"    <menu action='ToolsMenu'>"
"      <menuitem action='ioregtool'/>"
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
    { "EmulationMenu", NULL, "_Emulation" },
    { "ConfigMenu", NULL, "_Config" },
    { "ToolsMenu", NULL, "_Tools" },
    { "HelpMenu", NULL, "_Help" },

    { "SavestateMenu", NULL, "Sa_ve state" },
    { "LoadstateMenu", NULL, "_Load state" },

    { "FrameskipMenu", NULL, "_Frameskip" },
    { "LayersMenu", NULL, "_Layers" },

    { "ConfigSaveMenu", NULL, "_Saves" },

    { "open",       "gtk-open",         "_Open",         "<Ctrl>o",  NULL,   Open_Select },
    { "printscreen","gtk-media-record", "Take a _screenshot",    "<Ctrl>s",  NULL,   Printscreen },
    { "quit",       "gtk-quit",         "_Quit",         "<Ctrl>q",  NULL,   gtk_main_quit },

    { "run",        "gtk-media-play",   "_Run",          "<Ctrl>r",  NULL,   Launch },
    { "pause",      "gtk-media-pause",  "_Pause",        "<Ctrl>p",  NULL,   Pause },
    { "reset",      "gtk-refresh",      "Re_set",        NULL,       NULL,   Reset },

    { "editctrls",  NULL,               "_Edit controls",NULL,       NULL,   Edit_Controls },
    { "about",      "gtk-about",        "_About",        NULL,       NULL,   About }
};

static const GtkToggleActionEntry toggle_entries[] = {
    { "enableaudio", NULL, "_Enable audio", NULL, NULL, G_CALLBACK(desmume_gtk_disable_audio), TRUE}//,
};

static const GtkRadioActionEntry frameskip_entries[] = {
    { "frameskip0", NULL, "_0", NULL, NULL, 0},
    { "frameskip1", NULL, "_1", NULL, NULL, 1},
    { "frameskip2", NULL, "_2", NULL, NULL, 2},
    { "frameskip3", NULL, "_3", NULL, NULL, 3},
    { "frameskip4", NULL, "_4", NULL, NULL, 4},
    { "frameskip5", NULL, "_5", NULL, NULL, 5},
    { "frameskip6", NULL, "_6", NULL, NULL, 6},
    { "frameskip7", NULL, "_7", NULL, NULL, 7},
    { "frameskip8", NULL, "_8", NULL, NULL, 8},
    { "frameskip9", NULL, "_9", NULL, NULL, 9},
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

GtkActionGroup * action_group;

SoundInterface_struct *SNDCoreList[] = {
&SNDDummy,
&SNDFile,
&SNDSDL,
NULL
};

GPU3DInterface *core3DList[] = {
  &gpu3DNull,
  &gpu3DRasterize
#if defined(HAVE_LIBOSMESA)
  ,
  &gpu3Dgl
#endif
};

static u16 Cur_Keypad = 0;
static u16 gdk_shift_pressed = 0;

struct configured_features {
  int load_slot;
  int opengl_2d;
  int soft_colour;

  int disable_sound;
  int engine_3d;
  int disable_limiter;
  int savetype;

  int arm9_gdb_port;
  int arm7_gdb_port;

  int firmware_language;

  const char *nds_file;
  const char *cflash_disk_image_file;
};

static void
init_configured_features( struct configured_features *config)
{
  config->load_slot = 0;

  config->arm9_gdb_port = 0;
  config->arm7_gdb_port = 0;

  config->disable_sound = 0;

  config->opengl_2d = 0;
  config->soft_colour = 0;

  config->engine_3d = 1;

  config->disable_limiter = 0;

  config->nds_file = NULL;
  config->savetype = 0;

  config->cflash_disk_image_file = NULL;

  /* use the default language */
  config->firmware_language = -1;
}

static int
fill_configured_features( struct configured_features *config,
                          int argc, char ** argv)
{
  GOptionEntry options[] = {
    { "load-slot", 0, 0, G_OPTION_ARG_INT, &config->load_slot, "Loads savegame from slot NUM", "NUM"},
    { "disable-sound", 0, 0, G_OPTION_ARG_NONE, &config->disable_sound, "Disables the sound emulation", NULL},
    { "disable-limiter", 0, 0, G_OPTION_ARG_NONE, &config->disable_limiter, "Disables the 60fps limiter", NULL},
    { "3d-engine", 0, 0, G_OPTION_ARG_INT, &config->engine_3d, "Select 3d rendering engine. Available engines:\n"
        "\t\t\t\t  0 = 3d disabled\n"
        "\t\t\t\t  1 = internal rasterizer (default)\n"
#ifdef HAVE_LIBOSMESA
        "\t\t\t\t  2 = osmesa opengl\n"
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
#ifdef GDB_STUB
    { "arm9gdb", 0, 0, G_OPTION_ARG_INT, &config->arm9_gdb_port, "Enable the ARM9 GDB stub on the given port", "PORT_NUM"},
    { "arm7gdb", 0, 0, G_OPTION_ARG_INT, &config->arm7_gdb_port, "Enable the ARM7 GDB stub on the given port", "PORT_NUM"},
#endif
    { "cflash", 0, 0, G_OPTION_ARG_FILENAME, &config->cflash_disk_image_file, "Enable disk image GBAMP compact flash emulation", "PATH_TO_DISK_IMAGE"},
    { NULL }
  };
  GOptionContext *ctx;
  GError *error = NULL;

  ctx = g_option_context_new ("");
  g_option_context_add_main_entries (ctx, options, "options");
  g_option_context_add_group (ctx, gtk_get_option_group (TRUE));
  g_option_context_parse (ctx, &argc, &argv, &error);
  g_option_context_free (ctx);

  if (error) {
    g_printerr("Error parsing command line arguments: %s\n", error->message);
    g_error_free (error);
    return 0;
  }

  if (argc == 2)
    config->nds_file = argv[1];
  if (argc > 2)
    goto error;

  if (config->savetype < 0 || config->savetype > 6) {
    g_printerr("Accepted savetypes are from 0 to 6.\n");
    goto error;
  }

  if (config->firmware_language < -1 || config->firmware_language > 5) {
    g_printerr("Firmware language must be set to a value from 0 to 5.\n");
    goto error;
  }

  if (config->load_slot < 0 || config->load_slot > 10) {
    g_printerr("I only know how to load from slots 1-10, 0 means 'do not load savegame' and is default\n");
    goto error;
  }

  if (config->engine_3d != 0 && config->engine_3d != 1
#if defined(HAVE_LIBOSMESA)
           && config->engine_3d != 2
#endif
          ) {
    g_printerr("Currently available ENGINES: 0, 1"
#if defined(HAVE_LIBOSMESA)
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
    g_printerr("USAGE: %s [options] [nds-file]\n", argv[0]);
    g_printerr("USAGE: %s --help    - for help\n", argv[0]);
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


u16 Keypad_Temp[NB_KEYS];

static int Write_ConfigFile(const gchar *config_file)
{
    int i;
    GKeyFile * keyfile;
    gchar *contents;
    gboolean ret;

    keyfile = g_key_file_new();

    for(i = 0; i < NB_KEYS; i++) {
        g_key_file_set_integer(keyfile, "KEYS", key_names[i], keyboard_cfg[i]);
        g_key_file_set_integer(keyfile, "JOYKEYS", key_names[i], joypad_cfg[i]);
    }

//  if(FirmwareFile[0]) {
//      ini_add_section(ini, "FIRMWARE");
//      ini_add_value(ini, "FIRMWARE", "FILE", FirmwareFile);
//  }

    contents = g_key_file_to_data(keyfile, 0, 0);   
    ret = g_file_set_contents(config_file, contents, -1, NULL);
    if (!ret)
        g_printerr("Failed to write to %s\n", config_file);
    g_free (contents);

    g_key_file_free(keyfile);

    return 0;
}

static int Read_ConfigFile(const gchar *config_file)
{
    int i, tmp;
    GKeyFile * keyfile = g_key_file_new();
    GError * error = NULL;

    load_default_config(gtk_kb_cfg);

    g_key_file_load_from_file(keyfile, config_file, G_KEY_FILE_NONE, 0);

    /* Load keyboard keys */
    for(i = 0; i < NB_KEYS; i++) {
        tmp = g_key_file_get_integer(keyfile, "KEYS", key_names[i], &error);
        if (error != NULL) {
                  g_error_free(error);
                  error = NULL;
        } else {
                  keyboard_cfg[i] = tmp;
        }
    }

    /* Load joystick keys */
    for(i = 0; i < NB_KEYS; i++) {
        tmp = g_key_file_get_integer(keyfile, "JOYKEYS", key_names[i], &error);
        if (error != NULL) {
                  g_error_free(error);
                  error = NULL;
        } else {
                  joypad_cfg[i] = tmp;
        }
    }

    g_key_file_free(keyfile);

    return 0;
}

/************************ GTK *******************************/

uint Frameskip = 0;

static GtkWidget *pWindow;
static GtkWidget *pStatusBar;
static GtkWidget *pDrawingArea;

/** The target for the expose event */
static GtkWidget *nds_screen_widget;

float nds_screen_size_ratio = 1.0f;

static BOOL regMainLoop = FALSE;

static inline void pStatusBar_Change (const char *message)
{
    gint pStatusBar_Ctx;

    pStatusBar_Ctx = gtk_statusbar_get_context_id(GTK_STATUSBAR(pStatusBar), "Global");
    gtk_statusbar_pop(GTK_STATUSBAR(pStatusBar), pStatusBar_Ctx);
    gtk_statusbar_push(GTK_STATUSBAR(pStatusBar), pStatusBar_Ctx, message);
}

gboolean EmuLoop(gpointer data);

static void About()//GtkWidget* widget, gpointer data)
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

static int Open(const char *filename, const char *cflash_disk_image)
{
    return NDS_LoadROM( filename, backupmemorytype, backupmemorysize, cflash_disk_image );
}

static void Launch()
{
    desmume_resume();

    if(!regMainLoop) {
        g_idle_add_full(EMULOOP_PRIO, &EmuLoop, pWindow, NULL);
        regMainLoop = TRUE;
    }

    pStatusBar_Change("Running ...");

    gtk_action_set_sensitive(gtk_action_group_get_action(action_group, "pause"), TRUE);
    gtk_action_set_sensitive(gtk_action_group_get_action(action_group, "run"), FALSE);
    gtk_action_set_sensitive(gtk_action_group_get_action(action_group, "reset"), TRUE);
    gtk_action_set_sensitive(gtk_action_group_get_action(action_group, "printscreen"), TRUE);
}

static void Pause()
{
    desmume_pause();
    pStatusBar_Change("Paused");

    gtk_action_set_sensitive(gtk_action_group_get_action(action_group, "pause"), FALSE);
    gtk_action_set_sensitive(gtk_action_group_get_action(action_group, "run"), TRUE);
}

/* Choose a file then load it */
static void Open_Select()
{
    GtkFileFilter *pFilter_nds, *pFilter_dsgba, *pFilter_any;
    GtkWidget *pFileSelection;
    GtkWidget *pParent;
    gchar *sPath;

    if (desmume_running())
        Pause();

    pParent = GTK_WIDGET(pWindow);

    pFilter_nds = gtk_file_filter_new();
    gtk_file_filter_add_pattern(pFilter_nds, "*.nds");
#ifdef HAVE_LIBZ
    gtk_file_filter_add_pattern(pFilter_nds, "*.nds.gz");
#endif
#ifdef HAVE_LIBZZIP
    gtk_file_filter_add_pattern(pFilter_nds, "*.nds.zip");
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

    /* Showing the window */
    switch(gtk_dialog_run(GTK_DIALOG(pFileSelection))) {
    case GTK_RESPONSE_OK:
        sPath = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(pFileSelection));
        if(Open((const char*)sPath, bad_glob_cflash_disk_image_file) < 0) {
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

        //Launch(NULL, pWindow);

        g_free(sPath);
        break;
    default:
        break;
    }
    gtk_widget_destroy(pFileSelection);
}

#if 0 /* not used */
static void Close()
{
}
#endif

static void Reset()
{
    NDS_Reset();
    desmume_resume();

    pStatusBar_Change("Running ...");
}


/////////////////////////////// DRAWING SCREEN //////////////////////////////////

static inline void gpu_screen_to_rgb(u8 *rgb, int size)
{
    for (int i = 0; i < size; i++) {
        rgb[(i*3)+0] = ((*((u16 *)&GPU_screen[(i<<1)]) >> 0) & 0x1f) << 3;
        rgb[(i*3)+1] = ((*((u16 *)&GPU_screen[(i<<1)]) >> 5) & 0x1f) << 3;
        rgb[(i*3)+2] = ((*((u16 *)&GPU_screen[(i<<1)]) >> 10) & 0x1f) << 3;
    }
}

/* Drawing callback */
static int gtkFloatExposeEvent (GtkWidget *widget, GdkEventExpose *event, gpointer data)
{
    guchar *rgb;
    rgb = (guchar *) malloc(SCREENS_PIXEL_SIZE*3);
    if (!rgb)
        return 0;

    gpu_screen_to_rgb(rgb, SCREENS_PIXEL_SIZE);
    gdk_draw_rgb_image (widget->window,
                widget->style->fg_gc[widget->state], 0, 0,
                256, 192*2,
                GDK_RGB_DITHER_NONE,
                rgb, 256*3);

    free(rgb);
    return 1;
}

/////////////////////////////// KEYS AND STYLUS UPDATE ///////////////////////////////////////

static gboolean Stylus_Move(GtkWidget *w, GdkEventMotion *e, gpointer data)
{
    GdkModifierType state;
    gint x,y;
    s32 EmuX, EmuY;
    const int *opengl = (const int *)data;

    if(click) {
        int scaled_x, scaled_y;
        if(e->is_hint)
            gdk_window_get_pointer(w->window, &x, &y, &state);
        else {
            x= (gint)e->x;
            y= (gint)e->y;
            state=(GdkModifierType)e->state;
        }

        scaled_x = int(x * nds_screen_size_ratio);
        scaled_y = int(y * nds_screen_size_ratio);

        LOG("X=%d, Y=%d, S&1=%d\n", x,y,state&GDK_BUTTON1_MASK);

        if ( !(*opengl)) {
            scaled_y -= 192;
        }
        if(scaled_y >= 0 && (state & GDK_BUTTON1_MASK)) {
            EmuX = CLAMP(scaled_x, 0, 255);
            EmuY = CLAMP(scaled_y, 0, 192);
            NDS_setTouchPos(EmuX, EmuY);
        }
    }

    return TRUE;
}

static gboolean Stylus_Press(GtkWidget * w, GdkEventButton * e,
                             gpointer data)
{
    GdkModifierType state;
    gint x, y;
    s32 EmuX, EmuY;
    const int *opengl = (const int *) data;

    if (desmume_running()) {
        if (e->button == 1) {
            int scaled_x, scaled_y;
            click = TRUE;

            gdk_window_get_pointer(w->window, &x, &y, &state);

            scaled_x = int(x * nds_screen_size_ratio);
            scaled_y = int(y * nds_screen_size_ratio);

            if (!(*opengl)) {
                scaled_y -= 192;
            }
            if (scaled_y >= 0 && (state & GDK_BUTTON1_MASK)) {
                EmuX = CLAMP(scaled_x, 0, 255);
                EmuY = CLAMP(scaled_y, 0, 192);
                NDS_setTouchPos(EmuX, EmuY);
            }
        }
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
      ADD_KEY( Cur_Keypad, Key );
      if(desmume_running()) update_keypad(Cur_Keypad);
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
  RM_KEY( Cur_Keypad, Key );
  if(desmume_running()) update_keypad(Cur_Keypad);
  return 1;

}

/////////////////////////////// CONTROLS EDIT //////////////////////////////////////

struct modify_key_ctx {
    gint mk_key_chosen;
    GtkWidget *label;
};

static void Modify_Key_Press(GtkWidget *w, GdkEventKey *e, struct modify_key_ctx *ctx)
{
    gchar *YouPressed;

    ctx->mk_key_chosen = e->keyval;
    YouPressed = g_strdup_printf("You pressed : %s\nClick OK to keep this key.", gdk_keyval_name(e->keyval));
    gtk_label_set(GTK_LABEL(ctx->label), YouPressed);
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

    g_signal_connect(G_OBJECT(mkDialog), "key_press_event", G_CALLBACK(Modify_Key_Press), &ctx);

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
    case GTK_RESPONSE_CANCEL:
    case GTK_RESPONSE_NONE:
        break;
    }
    gtk_widget_destroy(ecDialog);

}

///////////////////////////////// SCREEN SCALING //////////////////////////////

#ifdef BROKEN_SCREENSIZE
#define MAX_SCREENCOEFF 4 

static void Modify_ScreenCoeff(GtkWidget* widget, gpointer data)
{
    guint Size = GPOINTER_TO_UINT(data);
    static int ScreenCoeff_Size;

    gtk_drawing_area_size(GTK_DRAWING_AREA(pDrawingArea), 256 * Size, 384 * Size);
    gtk_widget_set_usize (pDrawingArea, 256 * Size, 384 * Size);

    ScreenCoeff_Size = Size;
}
#endif

/////////////////////////////// LAYER HIDING /////////////////////////////////

static void Modify_Layer(GtkToggleAction* action, gpointer data)
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
        if(active == TRUE) {
            if (!MainScreen.gpu->dispBG[Layer])
                GPU_addBack(MainScreen.gpu, Layer);
        } else {
            if (MainScreen.gpu->dispBG[Layer])
                 GPU_remove(MainScreen.gpu, Layer);
        }
        break;
    case MAIN_OBJ:
        if(active == TRUE) {
            if (!MainScreen.gpu->dispOBJ)
                GPU_addBack(MainScreen.gpu, Layer);
        } else { 
            if (MainScreen.gpu->dispOBJ)
                GPU_remove(MainScreen.gpu, Layer);
        }
        break;
    case SUB_BG_0:
    case SUB_BG_1:
    case SUB_BG_2:
    case SUB_BG_3:
        if(active == TRUE) {
            if (!SubScreen.gpu->dispBG[Layer-SUB_BG_0])
                GPU_addBack(SubScreen.gpu, Layer-SUB_BG_0);
        } else { 
            if (SubScreen.gpu->dispBG[Layer-SUB_BG_0])
                GPU_remove(SubScreen.gpu, Layer-SUB_BG_0);
        }
        break;
    case SUB_OBJ:
        if(active == TRUE) {
            if (!SubScreen.gpu->dispOBJ)
                GPU_addBack(SubScreen.gpu, Layer-SUB_BG_0);
        } else { 
            if (SubScreen.gpu->dispOBJ)
                GPU_remove(SubScreen.gpu, Layer-SUB_BG_0);
        }
        break;
    default:
        break;
    }
}

/////////////////////////////// PRINTSCREEN /////////////////////////////////

static void Printscreen()
{
    GdkPixbuf *screenshot;
    gchar *filename;
    GError *error = NULL;
    u8 *rgb;
    static int seq = 0;

    rgb = (u8 *) malloc(SCREENS_PIXEL_SIZE*3);
    if (!rgb)
        return;

    gpu_screen_to_rgb(rgb, SCREENS_PIXEL_SIZE);
    screenshot = gdk_pixbuf_new_from_data(rgb,
                          GDK_COLORSPACE_RGB,
                          FALSE,
                          8,
                          256,
                          192*2,
                          256*3, 
                          NULL,
                          NULL);

    filename = g_strdup_printf("./desmume-screenshot-%d.png", seq);
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
}

/////////////////////////////// DS CONFIGURATION //////////////////////////////////

#if 0

char FirmwareFile[256];

int LoadFirmware(const char *filename)
{
    int i;
    u32 size;
    FILE *f;

    strncpy(FirmwareFile, filename, ARRAY_SIZE(FirmwareFile));

    f = fopen(filename, "rb");
    if(!f) return -1;

    fseek(f, 0, SEEK_END);
    size = ftell(f);
    fseek(f, 0, SEEK_SET);

    if(size > MMU.spi7.fw.size) { fclose(f); return -1; }       /* this must be a small file*/

    i = fread(MMU.spi7.fw.data, size, 1, f);

    fclose(f);

    return i;
}

int SelectFirmwareFile_Load(GtkWidget *w, gpointer data)
{
    GtkFileFilter *pFilter_nds, *pFilter_bin, *pFilter_any;
    GtkWidget *pFileSelection;
    GtkWidget *pParent;
    gchar *sPath;

    BOOL oldState = desmume_running();
    Pause();

    pParent = GTK_WIDGET(data);

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

    if(FirmwareFile[0]) gtk_file_chooser_select_uri(GTK_FILE_CHOOSER(pFileSelection), FirmwareFile);

    switch(gtk_dialog_run(GTK_DIALOG(pFileSelection))) {
    case GTK_RESPONSE_OK:
        sPath = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(pFileSelection));
        if(LoadFirmware((const char*)sPath) < 0) {
            GtkWidget *pDialog = gtk_message_dialog_new(GTK_WINDOW(pFileSelection), GTK_DIALOG_MODAL, GTK_MESSAGE_ERROR, GTK_BUTTONS_OK, "Unable to load :\n%s", sPath);
            gtk_dialog_run(GTK_DIALOG(pDialog));
            gtk_widget_destroy(pDialog);
        } else {
            GtkWidget *pDialog = gtk_message_dialog_new(GTK_WINDOW(pFileSelection), GTK_DIALOG_MODAL, GTK_MESSAGE_INFO, GTK_BUTTONS_OK, "Selected firmware :\n%s", sPath);
            gtk_dialog_run(GTK_DIALOG(pDialog));
            gtk_widget_destroy(pDialog);
        }

        g_free(sPath);
        break;
    default:
        break;
    }
    gtk_widget_destroy(pFileSelection);

    if(oldState) Launch();

}

int SelectFirmwareFile_Load(GtkWidget *w, gpointer data)
{
    GtkFileFilter *pFilter_nds, *pFilter_bin, *pFilter_any;
    GtkWidget *pFileSelection;
    GtkWidget *pParent;
    gchar *sPath;

    BOOL oldState = desmume_running();
    Pause();

    pParent = GTK_WIDGET(data);

    pFilter_nds = gtk_file_filter_new();
    gtk_file_filter_add_pattern(pFilter_nds, "*.nds");
    gtk_file_filter_set_name(pFilter_nds, "Nds binary (.nds)");

    pFilter_bin = gtk_file_filter_new();
    gtk_file_filter_add_pattern(pFilter_bin, "*.bin");
    gtk_file_filter_set_name(pFilter_bin, "Binary file (.bin)");

    pFilter_any = gtk_file_filter_new();
    gtk_file_filter_add_pattern(pFilter_any, "*");
    gtk_file_filter_set_name(pFilter_any, "All files");

    pFileSelection = gtk_file_chooser_dialog_new("Save firmware...",
            GTK_WINDOW(pParent),
            GTK_FILE_CHOOSER_ACTION_OPEN,
            GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
            GTK_STOCK_SAVE, GTK_RESPONSE_OK,
            NULL);
    gtk_window_set_modal(GTK_WINDOW(pFileSelection), TRUE);

    gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(pFileSelection), pFilter_nds);
    gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(pFileSelection), pFilter_bin);
    gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(pFileSelection), pFilter_any);

    if(FirmwareFile[0]) gtk_file_chooser_select_uri(GTK_FILE_CHOOSER(pFileSelection), FirmwareFile);

    switch(gtk_dialog_run(GTK_DIALOG(pFileSelection))) {
    case GTK_RESPONSE_OK:
        sPath = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(pFileSelection));
        if(LoadFirmware((const char*)sPath) < 0) {
            GtkWidget *pDialog = gtk_message_dialog_new(GTK_WINDOW(pFileSelection), GTK_DIALOG_MODAL, GTK_MESSAGE_ERROR, GTK_BUTTONS_OK, "Unable to load :\n%s", sPath);
            gtk_dialog_run(GTK_DIALOG(pDialog));
            gtk_widget_destroy(pDialog);
        } else {
            GtkWidget *pDialog = gtk_message_dialog_new(GTK_WINDOW(pFileSelection), GTK_DIALOG_MODAL, GTK_MESSAGE_INFO, GTK_BUTTONS_OK, "Selected firmware :\n%s", sPath);
            gtk_dialog_run(GTK_DIALOG(pDialog));
            gtk_widget_destroy(pDialog);
        }

        g_free(sPath);
        break;
    default:
        break;
    }
    gtk_widget_destroy(pFileSelection);

    if(oldState) Launch();

}

#endif

/////////////////////////////// FRAMESKIP /////////////////////////////////

#define MAX_FRAMESKIP 10

static void Modify_Frameskip(GtkAction *action, GtkRadioAction *current)
{
    Frameskip = gtk_radio_action_get_current_value(current) ;
}

/////////////////////////////// TOOLS MANAGEMENT ///////////////////////////////

#include "dTool.h"

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

/////////////////////////////// MAIN EMULATOR LOOP ///////////////////////////////

static inline void _updateDTools()
{
    if (dTools_running == NULL)
        return;

    for(int i = 0; i < dTools_list_size; i++) {
        if(dTools_running[i]) { dTools_list[i]->update(); }
    }
}

gboolean EmuLoop(gpointer data)
{
    static Uint32 fps, fps_SecStart, fps_FrameCount;
    unsigned int i;
    gchar *Title;

    if(desmume_running()) { /* If desmume is currently running */
        static int limiter_frame_counter = 0;
        fps_FrameCount += Frameskip + 1;
        if(!fps_SecStart) fps_SecStart = SDL_GetTicks();
        if(SDL_GetTicks() - fps_SecStart >= 1000) {
            fps_SecStart = SDL_GetTicks();
            fps = fps_FrameCount;
            fps_FrameCount = 0;

            Title = g_strdup_printf("Desmume - %dfps", fps);
            gtk_window_set_title(GTK_WINDOW(pWindow), Title);
            g_free(Title);
        }

        desmume_cycle();    /* Emule ! */
        NDS_SkipFrame(true);
        for(i = 0; i < Frameskip; i++) {
            desmume_cycle();
        }
        NDS_SkipFrame(false);

        _updateDTools();
        gtk_widget_queue_draw( nds_screen_widget);

                if ( !gtk_fps_limiter_disabled) {
                  limiter_frame_counter += 1;
                  if ( limiter_frame_counter >= FPS_LIMITER_FRAME_PERIOD) {
                    limiter_frame_counter = 0;

                    /* wait for the timer to expire */
                    SDL_SemWait( fps_limiter_semaphore);
                  }
        }


        return TRUE;
    }

    regMainLoop = FALSE;
    return FALSE;
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

        snprintf(label, 60, "_%d", i % 10);

        snprintf(name, 60, "savestate%d", i);
        act = gtk_action_new(name, label, NULL, NULL);
        g_signal_connect(G_OBJECT(act), "activate", G_CALLBACK(MenuSave), GUINT_TO_POINTER(i));
        gtk_action_group_add_action(ag, GTK_ACTION(act));

        snprintf(name, 60, "loadstate%d", i);
        act = gtk_action_new(name, label, NULL, NULL);
        g_signal_connect(G_OBJECT(act), "activate", G_CALLBACK(MenuLoad), GUINT_TO_POINTER(i));
        gtk_action_group_add_action(ag, GTK_ACTION(act));
    }
}

static void changesavetype(GtkAction *action, GtkRadioAction *current)
{
    mmu_select_savetype( gtk_radio_action_get_current_value(current), 
            &backupmemorytype, &backupmemorysize);
}

#ifdef BROKEN_SCREENSIZE
static void desmume_gtk_menu_emulation_display_size (GtkWidget *pMenu, gboolean opengl)
{
    GtkWidget *pMenuItem, *pSubmenu;
    GtkWidget *mSize_Radio[MAX_SCREENCOEFF];
    gchar *buf;
    guint i;

    /* FIXME: this does not work and there's a lot of code that assume the default screen size, drawing function included */
    if (!opengl) {
        pSubmenu = gtk_menu_new();
        pMenuItem = gtk_menu_item_new_with_label("Display size");
        gtk_menu_item_set_submenu(GTK_MENU_ITEM(pMenuItem), pSubmenu);
        gtk_menu_shell_append(GTK_MENU_SHELL(pMenu), pMenuItem);

        for(i = 1; i < MAX_SCREENCOEFF; i++) {
            buf = g_strdup_printf("x%d", i);
            if (i>1)
                mSize_Radio[i] = gtk_radio_menu_item_new_with_label_from_widget(GTK_RADIO_MENU_ITEM(mSize_Radio[i-1]), buf);
            else
                mSize_Radio[i] = gtk_radio_menu_item_new_with_label(NULL, buf);
            g_free(buf);
            g_signal_connect(G_OBJECT(mSize_Radio[i]), "activate", G_CALLBACK(Modify_ScreenCoeff), GUINT_TO_POINTER(i));
            gtk_menu_shell_append(GTK_MENU_SHELL(pSubmenu), mSize_Radio[i]);
        }
    }
}
#endif

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
        g_signal_connect(G_OBJECT(act), "activate", G_CALLBACK(Modify_Layer), GUINT_TO_POINTER(i));
        gtk_action_group_add_action(ag, GTK_ACTION(act));
    }
}

static void desmume_gtk_disable_audio (GtkToggleAction *action)
{
    if (gtk_toggle_action_get_active(action) == TRUE) {
        SPU_ChangeSoundCore(SNDCORE_SDL, 735 * 4);
    } else {
        SPU_ChangeSoundCore(0, 0);
    }
}

#if 0
static void desmume_gtk_menu_config (GtkWidget *pMenuBar, int act_savetype)
{
    GtkWidget *pSubMenu;
    pSubmenu = gtk_menu_new();
    pMenuItem = gtk_menu_item_new_with_label("Firmware");
    gtk_menu_item_set_submenu(GTK_MENU_ITEM(pMenuItem), pSubmenu);
    gtk_menu_shell_append(GTK_MENU_SHELL(pMenu), pMenuItem);

    pMenuItem = gtk_menu_item_new_with_label("Select...");
    g_signal_connect(G_OBJECT(pMenuItem), "activate", G_CALLBACK(SelectFirmwareFile), (gpointer)0);
    gtk_menu_shell_append(GTK_MENU_SHELL(pSubmenu), pMenuItem);
}
#endif

static void desmume_gtk_menu_tools (GtkActionGroup *ag)
{
    gint i;
    for(i = 0; i < dTools_list_size; i++) {
        GtkAction *act;
        //FIXME: remove hardcoded 'ioregtool' from here and in ui_description
        act = gtk_action_new("ioregtool", dTools_list[i]->name, NULL, NULL);
        g_signal_connect(G_OBJECT(act), "activate", G_CALLBACK(Start_dTool), GINT_TO_POINTER(i));
        gtk_action_group_add_action(ag, GTK_ACTION(act));
    }
}

static int
common_gtk_main( struct configured_features *my_config)
{
    SDL_TimerID limiter_timer = NULL;
    gchar *config_file;

    GtkAccelGroup * accel_group;
    GtkWidget *pVBox;
    GtkWidget *pMenuBar;
    GtkWidget *pToolBar;
    gint pStatusBar_Ctx;

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

        bad_glob_cflash_disk_image_file = my_config->cflash_disk_image_file;

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
           Perhaps it needs a "window" to catch events...? */
    if(SDL_Init(SDL_INIT_TIMER|SDL_INIT_VIDEO) == -1) {
            g_printerr("Error trying to initialize SDL: %s\n",
                    SDL_GetError());
            return 1;
        }
    desmume_init( arm9_memio, &arm9_ctrl_iface,
                      arm7_memio, &arm7_ctrl_iface,
                      my_config->disable_sound);

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

    config_file = g_build_filename(g_get_home_dir(), ".desmume.ini", NULL);
    Read_ConfigFile(config_file);

    /* Create the window */
    pWindow = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(pWindow), "Desmume");

    gtk_window_set_resizable(GTK_WINDOW (pWindow), my_config->opengl_2d ? TRUE : FALSE);

    gtk_window_set_icon(GTK_WINDOW (pWindow), gdk_pixbuf_new_from_xpm_data(DeSmuME_xpm));

    g_signal_connect(G_OBJECT(pWindow), "destroy", G_CALLBACK(gtk_main_quit), NULL);
    g_signal_connect(G_OBJECT(pWindow), "key_press_event", G_CALLBACK(Key_Press), NULL);
    g_signal_connect(G_OBJECT(pWindow), "key_release_event", G_CALLBACK(Key_Release), NULL);

    /* Create the GtkVBox */
    pVBox = gtk_vbox_new(FALSE, 0);
    gtk_container_add(GTK_CONTAINER(pWindow), pVBox);

    GtkUIManager *ui_manager;
    ui_manager = gtk_ui_manager_new ();
    accel_group = gtk_accel_group_new();
    action_group = gtk_action_group_new("dui");

    gtk_action_group_add_actions(action_group, action_entries, G_N_ELEMENTS(action_entries), NULL);
    gtk_action_group_add_toggle_actions(action_group, toggle_entries, G_N_ELEMENTS(toggle_entries), NULL);
    desmume_gtk_menu_emulation_layers(action_group);
    desmume_gtk_menu_file_saveload_slot(action_group);
    desmume_gtk_menu_tools(action_group);
    gtk_action_group_add_radio_actions(action_group, savet_entries, G_N_ELEMENTS(savet_entries), 
            my_config->savetype, G_CALLBACK(changesavetype), NULL);
    gtk_action_group_add_radio_actions(action_group, frameskip_entries, G_N_ELEMENTS(frameskip_entries), 
            0, G_CALLBACK(Modify_Frameskip), NULL);
    {
        GList * list = gtk_action_group_list_actions(action_group);
        g_list_foreach(list, dui_set_accel_group, accel_group);
    }
    gtk_window_add_accel_group(GTK_WINDOW(pWindow), accel_group);
    gtk_action_set_sensitive(gtk_action_group_get_action(action_group, "pause"), FALSE);
    gtk_action_set_sensitive(gtk_action_group_get_action(action_group, "run"), FALSE);
    gtk_action_set_sensitive(gtk_action_group_get_action(action_group, "reset"), FALSE);
    gtk_action_set_sensitive(gtk_action_group_get_action(action_group, "printscreen"), FALSE);

    gtk_ui_manager_insert_action_group (ui_manager, action_group, 0);
    
    accel_group = gtk_ui_manager_get_accel_group (ui_manager);
    gtk_window_add_accel_group (GTK_WINDOW (pWindow), accel_group);

    desmume_try_adding_ui(ui_manager, ui_description);

    pMenuBar = gtk_ui_manager_get_widget (ui_manager, "/MainMenu");
    gtk_box_pack_start (GTK_BOX (pVBox), pMenuBar, FALSE, FALSE, 0);
    pToolBar = gtk_ui_manager_get_widget (ui_manager, "/ToolBar");
    gtk_box_pack_start (GTK_BOX(pVBox), pToolBar, FALSE, FALSE, 0);

    /* Creating the place for showing DS screens */
    pDrawingArea= gtk_drawing_area_new();

    gtk_drawing_area_size(GTK_DRAWING_AREA(pDrawingArea), 256, 384);
    gtk_widget_set_usize (pDrawingArea, 256, 384);

    gtk_widget_set_events(pDrawingArea,
                          GDK_EXPOSURE_MASK | GDK_LEAVE_NOTIFY_MASK |
                          GDK_BUTTON_PRESS_MASK | GDK_BUTTON_RELEASE_MASK |
                          GDK_POINTER_MOTION_MASK | GDK_KEY_PRESS_MASK );

    g_signal_connect(G_OBJECT(pDrawingArea), "button_press_event",
                     G_CALLBACK(Stylus_Press), &my_config->opengl_2d);
    g_signal_connect(G_OBJECT(pDrawingArea), "button_release_event",
                     G_CALLBACK(Stylus_Release), NULL);
    g_signal_connect(G_OBJECT(pDrawingArea), "motion_notify_event",
                     G_CALLBACK(Stylus_Move), &my_config->opengl_2d);

    g_signal_connect(G_OBJECT(pDrawingArea), "expose_event",
                     G_CALLBACK(gtkFloatExposeEvent), NULL ) ;

    gtk_box_pack_start(GTK_BOX(pVBox), pDrawingArea, FALSE, FALSE, 0);

    nds_screen_widget = pDrawingArea;

    /* Status bar */
    pStatusBar = gtk_statusbar_new();
    pStatusBar_Ctx = gtk_statusbar_get_context_id(GTK_STATUSBAR(pStatusBar), "Global");
    pStatusBar_Change("Desmume");
    gtk_box_pack_end(GTK_BOX(pVBox), pStatusBar, FALSE, FALSE, 0);

    gtk_widget_show_all(pWindow);

    //LoadFirmware("fw.bin");

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

    /*
     * Set the 3D emulation to use
     */
    unsigned core = my_config->engine_3d;
    /* setup the gdk 3D emulation; */
#if defined(HAVE_LIBOSMESA)
    if(my_config->engine_3d == 2){
        core = init_osmesa_3Demu() ? 2 : GPU3D_NULL;
    }
#endif
    NDS_3D_ChangeCore(core);
    if(my_config->engine_3d != 0 && gpu3D == &gpu3DNull){
            g_printerr("Failed to initialise openGL 3D emulation; "
                     "removing 3D support\n");
    }

    mmu_select_savetype(my_config->savetype, &backupmemorytype, &backupmemorysize);

    /* Command line arg */
    if( my_config->nds_file != NULL) {
        if(Open( my_config->nds_file, bad_glob_cflash_disk_image_file) >= 0) {
            if(my_config->load_slot){
              loadstate_slot(my_config->load_slot);
            }

            Launch();
        } else {
            GtkWidget *pDialog = gtk_message_dialog_new(GTK_WINDOW(pWindow),
                    GTK_DIALOG_MODAL,
                    GTK_MESSAGE_INFO,
                    GTK_BUTTONS_OK,
                    "Unable to load :\n%s", my_config->nds_file);
            gtk_dialog_run(GTK_DIALOG(pDialog));
            gtk_widget_destroy(pDialog);
        }
    }

    /* Main loop */

//  gtk_idle_add(&EmuLoop, pWindow);
//  g_idle_add(&EmuLoop, pWindow);

    gtk_main();

    desmume_free();

        if ( !gtk_fps_limiter_disabled) {
          /* tidy up the FPS limiter timer and semaphore */
          SDL_RemoveTimer( limiter_timer);
          SDL_DestroySemaphore( fps_limiter_semaphore);
        }

        /* Unload joystick */
        uninit_joy();

    SDL_Quit();

    Write_ConfigFile(config_file);
    g_free(config_file);

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


int
main (int argc, char *argv[])
{
  struct configured_features my_config;

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

