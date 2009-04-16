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

#if defined(GTKGLEXT_AVAILABLE) || defined(HAVE_LIBOSMESA)
#include <GL/gl.h>
#include <GL/glu.h>
#include "OGLRender.h"
#ifdef GTKGLEXT_AVAILABLE
#include <gtk/gtkgl.h>
#include "gdk_3Demu.h"
#endif
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

const char * save_type_names[] = {
    "Autodetect",
    "EEPROM 4kbit",
    "EEPROM 64kbit",
    "EEPROM 512kbit",
    "FRAM 256kbit",
    "FLASH 2mbit",
    "FLASH 4mbit",
    NULL
};

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

static void Open_Select(GtkWidget* widget, gpointer data);
static void Launch();
static void Pause();
static void Printscreen();
static void Reset();

static const GtkActionEntry action_entries[] = {
    { "open",       "gtk-open",         "Open",         "<Ctrl>o",  NULL,   G_CALLBACK(Open_Select) },
    { "run",        "gtk-media-play",   "Run",          "<Ctrl>r",  NULL,   G_CALLBACK(Launch) },
    { "pause",      "gtk-media-pause",  "Pause",        "<Ctrl>p",  NULL,   G_CALLBACK(Pause) },
    { "quit",       "gtk-quit",         "Quit",         "<Ctrl>q",  NULL,   G_CALLBACK(gtk_main_quit) },
    { "printscreen","gtk-media-record", "Take a screenshot",    "<Ctrl>s",  NULL,   G_CALLBACK(Printscreen) },
    { "reset",      "gtk-refresh",      "Reset",        NULL,       NULL,   G_CALLBACK(Reset) }
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
#if defined(GTKGLEXT_AVAILABLE) || defined(HAVE_LIBOSMESA)
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
// GTKGLEXT and LIBOSMESA are currently exclusive, so, no conflict below
#ifdef GTKGLEXT_AVAILABLE
        "\t\t\t\t  2 = gtkglext off-screen opengl\n"
#endif
#ifdef HAVE_LIBOSMESA
        "\t\t\t\t  2 = osmesa opengl\n"
#endif
        ,"ENGINE"},
#if defined(GTKGLEXT_AVAILABLE)
    { "opengl-2d", 0, 0, G_OPTION_ARG_NONE, &config->opengl_2d, "Enables using OpenGL for screen rendering", NULL},
    { "soft-convert", 0, 0, G_OPTION_ARG_NONE, &config->soft_colour, 
        "Use software colour conversion during OpenGL screen rendering.\n"
            "\t\t\t\t  May produce better or worse frame rates depending on hardware", NULL},
#endif
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

  if (config->firmware_language < -1 || config->firmware_language > 5) {
    g_printerr("Firmware language must be set to a value from 0 to 5.\n");
    goto error;
  }

  if (config->load_slot < 0 || config->load_slot > 10) {
    g_printerr("I only know how to load from slots 1-10, 0 means 'do not load savegame' and is default\n");
    goto error;
  }

  if (config->engine_3d != 0 && config->engine_3d != 1
#if defined(GTKGLEXT_AVAILABLE) || defined(HAVE_LIBOSMESA)
           && config->engine_3d != 2
#endif
          ) {
    g_printerr("Currently available ENGINES: 0, 1"
#if defined(GTKGLEXT_AVAILABLE) || defined(HAVE_LIBOSMESA)
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

#ifdef GTKGLEXT_AVAILABLE
static GtkWidget *top_screen_widget;
static GtkWidget *bottom_screen_widget;

GLuint screen_texture[1];
#endif

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

static void About(GtkWidget* widget, gpointer data)
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
static void Open_Select(GtkWidget* widget, gpointer data)
{
    GtkFileFilter *pFilter_nds, *pFilter_dsgba, *pFilter_any;
    GtkWidget *pFileSelection;
    GtkWidget *pParent;
    gchar *sPath;

    if (desmume_running())
        Pause();

    pParent = GTK_WIDGET(data);

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

#ifdef GTKGLEXT_AVAILABLE
static void
gtk_init_main_gl_area(GtkWidget *widget,
                      gpointer   data)
{
  GLenum errCode;
  GdkGLContext *glcontext;
  GdkGLDrawable *gldrawable;
  glcontext = gtk_widget_get_gl_context (widget);
  gldrawable = gtk_widget_get_gl_drawable (widget);
  uint16_t blank_texture[256 * 512];

  /*** OpenGL BEGIN ***/
  if (!gdk_gl_drawable_gl_begin (gldrawable, glcontext))
    return;

  LOG("Doing GL init\n");

  memset(blank_texture, 0x001f, sizeof(blank_texture));

  /* Enable Texture Mapping */
  glEnable( GL_TEXTURE_2D );

  /* Set the background black */
  glClearColor( 0.0f, 0.0f, 0.0f, 0.0f );

  /* Create The Texture */
  glGenTextures( 1, &screen_texture[0]);

  glBindTexture( GL_TEXTURE_2D, screen_texture[0]);

  /* Generate The Texture */
  glTexImage2D( GL_TEXTURE_2D, 0, GL_RGB, 256, 512,
                0, GL_RGBA,
                GL_UNSIGNED_SHORT_1_5_5_5_REV,
                blank_texture);

  /* Linear Filtering */
  glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR );
  glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR );

  if ((errCode = glGetError()) != GL_NO_ERROR) {
    const GLubyte *errString;

    errString = gluErrorString(errCode);
    g_printerr("Failed to init GL: %s\n", errString);
  }

  gdk_gl_drawable_gl_end (gldrawable);
  /*** OpenGL END ***/
}

static void
gtk_init_sub_gl_area(GtkWidget *widget,
                      gpointer   data)
{
  GLenum errCode;
  GdkGLContext *glcontext;
  GdkGLDrawable *gldrawable;
  glcontext = gtk_widget_get_gl_context (widget);
  gldrawable = gtk_widget_get_gl_drawable (widget);


  /*** OpenGL BEGIN ***/
  if (!gdk_gl_drawable_gl_begin (gldrawable, glcontext))
    return;

  /* Enable Texture Mapping */
  glEnable( GL_TEXTURE_2D );

  /* Set the background black */
  glClearColor( 0.0f, 0.0f, 0.0f, 0.0f );

  glBindTexture( GL_TEXTURE_2D, screen_texture[0]);

  if ((errCode = glGetError()) != GL_NO_ERROR) {
    const GLubyte *errString;

    errString = gluErrorString(errCode);
    g_printerr("Failed to init GL: %s\n", errString);
  }

  gdk_gl_drawable_gl_end (gldrawable);
  /*** OpenGL END ***/
}
#endif


/////////////////////////////// DRAWING SCREEN //////////////////////////////////

static inline void gpu_screen_to_rgb(u8 *rgb, int size)
{
    for (int i = 0; i < size; i++) {
        rgb[(i*3)+0] = ((*((u16 *)&GPU_screen[(i<<1)]) >> 0) & 0x1f) << 3;
        rgb[(i*3)+1] = ((*((u16 *)&GPU_screen[(i<<1)]) >> 5) & 0x1f) << 3;
        rgb[(i*3)+2] = ((*((u16 *)&GPU_screen[(i<<1)]) >> 10) & 0x1f) << 3;
    }
}

#ifdef GTKGLEXT_AVAILABLE
static int
top_screen_expose_fn( GtkWidget *widget, GdkEventExpose *event, gpointer data)
{
  GdkGLContext *glcontext = gtk_widget_get_gl_context (widget);
  GdkGLDrawable *gldrawable = gtk_widget_get_gl_drawable (widget);
  int software_convert = *((int *)data);

  /*** OpenGL BEGIN ***/
  if (!gdk_gl_drawable_gl_begin (gldrawable, glcontext))
    return FALSE;

  GLenum errCode;

  /* Clear The Screen And The Depth Buffer */
  glClear( GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT );

  /* Move Into The Screen 5 Units */
  //glLoadIdentity( );

  /* Select screen Texture */
  glBindTexture( GL_TEXTURE_2D, screen_texture[0]);

  if ( software_convert) {
    u8 converted[256 * 384 * 3];

    gpu_screen_to_rgb(converted, 256 * 384);
    glTexSubImage2D( GL_TEXTURE_2D, 0, 0, 0, 256, 384,
                     GL_RGB,
                     GL_UNSIGNED_BYTE,
                     converted);
  } else {
    glTexSubImage2D( GL_TEXTURE_2D, 0, 0, 0, 256, 384,
                     GL_RGBA,
                     GL_UNSIGNED_SHORT_1_5_5_5_REV,
                     &GPU_screen);
  }


  if ((errCode = glGetError()) != GL_NO_ERROR) {
    const GLubyte *errString;

    errString = gluErrorString(errCode);
    g_printerr("GL subimage failed: %s\n", errString);
  }


  glBegin( GL_QUADS);

  /* Top screen */
  glTexCoord2f( 0.0f, 0.0f ); glVertex3f( 0.0f,  0.0f, 0.0f );
  glTexCoord2f( 1.0f, 0.0f ); glVertex3f( 256.0f,  0.0f,  0.0f );
  glTexCoord2f( 1.0f, 0.375f ); glVertex3f( 256.0f,  192.0f,  0.0f );
  glTexCoord2f( 0.0f, 0.375f ); glVertex3f(  0.0f,  192.0f, 0.0f );
  glEnd( );

  if ((errCode = glGetError()) != GL_NO_ERROR) {
    const GLubyte *errString;

    errString = gluErrorString(errCode);
    g_printerr("GL draw failed: %s\n", errString);
  }

  if (gdk_gl_drawable_is_double_buffered (gldrawable))
    gdk_gl_drawable_swap_buffers (gldrawable);
  else
    glFlush ();


  gdk_gl_drawable_gl_end (gldrawable);
  /*** OpenGL END ***/

  gtk_widget_queue_draw( bottom_screen_widget);

  return TRUE;
}

static int
bottom_screen_expose_fn(GtkWidget *widget, GdkEventExpose *event, gpointer data)
{
  GdkGLContext *glcontext = gtk_widget_get_gl_context (widget);
  GdkGLDrawable *gldrawable = gtk_widget_get_gl_drawable (widget);

  LOG("Sub Expose\n");

  /*** OpenGL BEGIN ***/
  if (!gdk_gl_drawable_gl_begin (gldrawable, glcontext)) {
    g_printerr("begin failed\n");
    return FALSE;
  }
  LOG("begin\n");

  GLenum errCode;

  /* Clear The Screen */
  glClear( GL_COLOR_BUFFER_BIT);

  //glBindTexture( GL_TEXTURE_2D, screen_texture[0]);

  glBegin( GL_QUADS);

  /* Bottom screen */
  glTexCoord2f( 0.0f, 0.375f ); glVertex2f( 0.0f,  0.0f);
  glTexCoord2f( 1.0f, 0.375f ); glVertex2f( 256.0f,  0.0f);
  glTexCoord2f( 1.0f, 0.75f ); glVertex2f( 256.0f,  192.0f);
  glTexCoord2f( 0.0f, 0.75f ); glVertex2f(  0.0f,  192.0f);
  glEnd( );

  if (gdk_gl_drawable_is_double_buffered (gldrawable))
    gdk_gl_drawable_swap_buffers (gldrawable);
  else
    glFlush ();


  if ((errCode = glGetError()) != GL_NO_ERROR) {
    const GLubyte *errString;

    errString = gluErrorString(errCode);
    g_printerr("sub GL draw failed: %s\n", errString);
  }

  gdk_gl_drawable_gl_end (gldrawable);
  /*** OpenGL END ***/

  return 1;
}

static gboolean
common_configure_fn( GtkWidget *widget,
                     GdkEventConfigure *event )
{
  if ( gtk_widget_is_gl_capable( widget) == FALSE)
    return TRUE;

  GdkGLContext *glcontext = gtk_widget_get_gl_context (widget);
  GdkGLDrawable *gldrawable = gtk_widget_get_gl_drawable (widget);

  int comp_width = 3 * event->width;
  int comp_height = 4 * event->height;
  int use_width = 1;
  GLenum errCode;

  /* Height / width ration */
  GLfloat ratio;

  LOG("width %d, height %d\n", event->width, event->height);

  /*** OpenGL BEGIN ***/
  if (!gdk_gl_drawable_gl_begin (gldrawable, glcontext))
    return FALSE;

  if ( comp_width > comp_height) {
    use_width = 0;
  }

  /* Protect against a divide by zero */
  if ( event->height == 0 )
    event->height = 1;
  if ( event->width == 0)
    event->width = 1;

  ratio = ( GLfloat )event->width / ( GLfloat )event->height;

  /* Setup our viewport. */
  glViewport( 0, 0, ( GLint )event->width, ( GLint )event->height );

  /*
   * change to the projection matrix and set
   * our viewing volume.
   */
  glMatrixMode( GL_PROJECTION );
  glLoadIdentity( );

  {
    double left;
    double right;
    double bottom;
    double top;
    double other_dimen;

    if ( use_width) {
      left = 0.0;
      right = 256.0;

      nds_screen_size_ratio = 256.0 / (double)event->width;

      other_dimen = (double)event->width * 3.0 / 4.0;

      top = 0.0;
      bottom = 192.0 * ((double)event->height / other_dimen);
    } else {
      top = 0.0;
      bottom = 192.0;

      nds_screen_size_ratio = 192.0 / (double)event->height;

      other_dimen = (double)event->height * 4.0 / 3.0;

      left = 0.0;
      right = 256.0 * ((double)event->width / other_dimen);
    }

    LOG("%d,%d\n", event->width, event->height);
    LOG("l %lf, r %lf, t %lf, b %lf, other dimen %lf\n",
           left, right, top, bottom, other_dimen);

    /* get the area (0,0) to (256,384) into the middle of the viewport */
    gluOrtho2D( left, right, bottom, top);
  }

  /* Make sure we're chaning the model view and not the projection */
  glMatrixMode( GL_MODELVIEW );

  /* Reset The View */
  glLoadIdentity( );

  if ((errCode = glGetError()) != GL_NO_ERROR) {
    const GLubyte *errString;

    errString = gluErrorString(errCode);
    g_printerr("GL resie failed: %s\n", errString);
  }

  gdk_gl_drawable_gl_end (gldrawable);
  /*** OpenGL END ***/

  return TRUE;
}

#endif



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
  // FIXME: this is a hack to allow accels to work together with keypad emulation
  // should be fixed by somebody who knows how to make key_press_event trigger AFTER all GtkAccels
  guint mask;
  mask = GDK_SHIFT_MASK | GDK_CONTROL_MASK | GDK_MOD1_MASK | GDK_MOD5_MASK; // shift,ctrl, both alts
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

static void Edit_Controls(GtkWidget* widget, gpointer data)
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

static void Modify_Layer(GtkWidget* widget, gpointer data)
{
    guint Layer = GPOINTER_TO_UINT(data);
    gboolean active;

    // FIXME: make it work after resume
    if (!desmume_running())
        return;

    active = gtk_check_menu_item_get_active(GTK_CHECK_MENU_ITEM(widget));

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

static void Modify_Frameskip(GtkWidget *widget, gpointer data)
{
    Frameskip = GPOINTER_TO_INT(data);
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

    if(desmume_running()) { /* Si on est en train d'executer le programme ... */
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

static void dui_set_accel_group(gpointer action, gpointer group) {
        gtk_action_set_accel_group((GtkAction *)action, (GtkAccelGroup *)group);
}

/////////////////////////////// MAIN ///////////////////////////////

static void desmume_gtk_menu_file_saveload_slot (GtkWidget *pMenu)
{
    GtkWidget *pMenuItemS, *pMenuItemL, *pSubmenuS, *pSubmenuL, *item;
    GSList * list;

    pSubmenuS = gtk_menu_new();
    pMenuItemS = gtk_menu_item_new_with_label("Save State");
    gtk_menu_item_set_submenu(GTK_MENU_ITEM(pMenuItemS), pSubmenuS);
    gtk_menu_shell_append(GTK_MENU_SHELL(pMenu), pMenuItemS);

    pSubmenuL = gtk_menu_new();
    pMenuItemL = gtk_menu_item_new_with_label("Load State");
    gtk_menu_item_set_submenu(GTK_MENU_ITEM(pMenuItemL), pSubmenuL);
    gtk_menu_shell_append(GTK_MENU_SHELL(pMenu), pMenuItemL);

    for(int i = 1; i <= 10; i++){
        char label[64];
        snprintf(label, 60, "SSlot %d", i);
        item = gtk_menu_item_new_with_label(label);
        gtk_menu_shell_append(GTK_MENU_SHELL(pSubmenuS), item);
        g_signal_connect(G_OBJECT(item), "activate", G_CALLBACK(MenuSave), GINT_TO_POINTER(i));

        snprintf(label, 60, "LSlot %d", i);
        item = gtk_menu_item_new_with_label(label);
        gtk_menu_shell_append(GTK_MENU_SHELL(pSubmenuL), item);
        g_signal_connect(G_OBJECT(item), "activate", G_CALLBACK(MenuLoad), GINT_TO_POINTER(i));
    }
}

static void desmume_gtk_menu_file (GtkWidget *pMenuBar)
{
    GtkWidget *pMenu, *pMenuItem;

    pMenu = gtk_menu_new();
    gtk_container_add(GTK_CONTAINER(pMenu), gtk_action_create_menu_item(gtk_action_group_get_action(action_group, "open")));
    desmume_gtk_menu_file_saveload_slot(pMenu);
    gtk_container_add(GTK_CONTAINER(pMenu), gtk_action_create_menu_item(gtk_action_group_get_action(action_group, "printscreen")));
    gtk_container_add(GTK_CONTAINER(pMenu), gtk_action_create_menu_item(gtk_action_group_get_action(action_group, "quit")));

    pMenuItem = gtk_menu_item_new_with_label("File");
    gtk_menu_item_set_submenu(GTK_MENU_ITEM(pMenuItem), pMenu);
    gtk_menu_shell_append(GTK_MENU_SHELL(pMenuBar), pMenuItem);
}

static void changesavetype(GtkCheckMenuItem *checkmenuitem, gpointer type)
{
    if (gtk_check_menu_item_get_active(checkmenuitem))
        mmu_select_savetype(GPOINTER_TO_INT(type), &backupmemorytype, &backupmemorysize);
}

static void desmume_gtk_menu_emulation_saves (GtkWidget *pMenu, int act_savetype)
{
    GtkWidget *pMenuItem, *pSubmenu, *item;
    GSList * list;

    pSubmenu = gtk_menu_new();
    pMenuItem = gtk_menu_item_new_with_label("Saves");
    gtk_menu_item_set_submenu(GTK_MENU_ITEM(pMenuItem), pSubmenu);
    gtk_menu_shell_append(GTK_MENU_SHELL(pMenu), pMenuItem);

    list = NULL;
    for (gint i = 0; save_type_names[i] != NULL; i++)
    {
        item = gtk_radio_menu_item_new_with_label(list, save_type_names[i]);
        g_signal_connect(item, "toggled", G_CALLBACK(changesavetype), GINT_TO_POINTER(i));
        list = gtk_radio_menu_item_get_group(GTK_RADIO_MENU_ITEM(item));
        gtk_menu_shell_append(GTK_MENU_SHELL(pSubmenu), item);
        if( i == act_savetype )
            gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(item), TRUE);
    }
}

static void desmume_gtk_menu_emulation_frameskip (GtkWidget *pMenu)
{
    GtkWidget *mFrameskip_Radio[MAX_FRAMESKIP];
    GtkWidget *pMenuItem, *pSubmenu;
    gchar *buf;
    guint i;

    pSubmenu = gtk_menu_new();
    pMenuItem = gtk_menu_item_new_with_label("Frameskip");
    gtk_menu_item_set_submenu(GTK_MENU_ITEM(pMenuItem), pSubmenu);
    gtk_menu_shell_append(GTK_MENU_SHELL(pMenu), pMenuItem);

    for(i = 0; i < MAX_FRAMESKIP; i++) {
        buf = g_strdup_printf("%d", i);
        if (i>0)
            mFrameskip_Radio[i] = gtk_radio_menu_item_new_with_label_from_widget(GTK_RADIO_MENU_ITEM(mFrameskip_Radio[i-1]), buf);
        else
            mFrameskip_Radio[i] = gtk_radio_menu_item_new_with_label(NULL, buf);
        g_free(buf);
        g_signal_connect(G_OBJECT(mFrameskip_Radio[i]), "activate", G_CALLBACK(Modify_Frameskip), GINT_TO_POINTER(i));
        gtk_menu_shell_append(GTK_MENU_SHELL(pSubmenu), mFrameskip_Radio[i]);
    }
    gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(mFrameskip_Radio[0]), TRUE);
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

static void desmume_gtk_menu_emulation_layers (GtkWidget *pMenu, gboolean opengl)
{
    GtkWidget *pMenuItem, *pSubmenu;
    GtkWidget *mLayers_Radio[10];
    const char *Layers_Menu[10] = {
        "Main BG 0",
        "Main BG 1",
        "Main BG 2",
        "Main BG 3",
        "Main OBJ",
        "SUB BG 0",
        "SUB BG 1",
        "SUB BG 2",
        "SUB BG 3",
        "SUB OBJ"
    };
    guint i;

    pSubmenu = gtk_menu_new();
    pMenuItem = gtk_menu_item_new_with_label("Layers");
    gtk_menu_item_set_submenu(GTK_MENU_ITEM(pMenuItem), pSubmenu);
    gtk_menu_shell_append(GTK_MENU_SHELL(pMenu), pMenuItem);

    for(i = 0; i < 10; i++) {
        mLayers_Radio[i] = gtk_check_menu_item_new_with_label(Layers_Menu[i]);
        g_signal_connect(G_OBJECT(mLayers_Radio[i]), "activate", G_CALLBACK(Modify_Layer), GUINT_TO_POINTER(i));
        gtk_menu_shell_append(GTK_MENU_SHELL(pSubmenu), mLayers_Radio[i]);
        gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(mLayers_Radio[i]), TRUE);
    }
}

static void desmume_gtk_disable_audio (GtkWidget *widget, gpointer data)
{
    if (gtk_check_menu_item_get_active(GTK_CHECK_MENU_ITEM(widget)) == TRUE) {
        SPU_ChangeSoundCore(SNDCORE_SDL, 735 * 4);
    } else {
        SPU_ChangeSoundCore(0, 0);
    }
}

static void desmume_gtk_menu_emulation_disable_audio (GtkWidget *pMenu)
{
    GtkWidget *pMenuItem;

    pMenuItem = gtk_check_menu_item_new_with_label("Enable Audio");
    g_signal_connect(G_OBJECT(pMenuItem), "toggled", G_CALLBACK(desmume_gtk_disable_audio), NULL);
    gtk_menu_shell_append(GTK_MENU_SHELL(pMenu), pMenuItem);
    gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(pMenuItem), TRUE);
}

static void desmume_gtk_menu_emulation (GtkWidget *pMenuBar, gboolean opengl)
{
    GtkWidget *pMenu, *pMenuItem;

    pMenu = gtk_menu_new();
    pMenuItem = gtk_menu_item_new_with_label("Emulation");
    gtk_menu_item_set_submenu(GTK_MENU_ITEM(pMenuItem), pMenu);
    gtk_menu_shell_append(GTK_MENU_SHELL(pMenuBar), pMenuItem);

    gtk_container_add(GTK_CONTAINER(pMenu), gtk_action_create_menu_item(gtk_action_group_get_action(action_group, "run")));
    gtk_container_add(GTK_CONTAINER(pMenu), gtk_action_create_menu_item(gtk_action_group_get_action(action_group, "pause")));
    gtk_container_add(GTK_CONTAINER(pMenu), gtk_action_create_menu_item(gtk_action_group_get_action(action_group, "reset")));

    desmume_gtk_menu_emulation_disable_audio(pMenu);
    desmume_gtk_menu_emulation_frameskip(pMenu);
    desmume_gtk_menu_emulation_layers(pMenu, opengl);
#ifdef BROKEN_SCREENSIZE
    desmume_gtk_menu_emulation_display_size(pMenu, opengl);
#endif
}

static void desmume_gtk_menu_config (GtkWidget *pMenuBar, int act_savetype)
{
    GtkWidget *pMenu, *pMenuItem;
    
    pMenu = gtk_menu_new();
    pMenuItem = gtk_menu_item_new_with_label("Config");
    gtk_menu_item_set_submenu(GTK_MENU_ITEM(pMenuItem), pMenu);
    gtk_menu_shell_append(GTK_MENU_SHELL(pMenuBar), pMenuItem);

    desmume_gtk_menu_emulation_saves(pMenu, act_savetype);
    pMenuItem = gtk_menu_item_new_with_label("Edit controls");
    g_signal_connect(G_OBJECT(pMenuItem), "activate", G_CALLBACK(Edit_Controls), (GtkWidget*) pWindow);
    gtk_menu_shell_append(GTK_MENU_SHELL(pMenu), pMenuItem);

#if 0
    GtkWidget *pSubMenu;
    pSubmenu = gtk_menu_new();
    pMenuItem = gtk_menu_item_new_with_label("Firmware");
    gtk_menu_item_set_submenu(GTK_MENU_ITEM(pMenuItem), pSubmenu);
    gtk_menu_shell_append(GTK_MENU_SHELL(pMenu), pMenuItem);

    pMenuItem = gtk_menu_item_new_with_label("Select...");
    g_signal_connect(G_OBJECT(pMenuItem), "activate", G_CALLBACK(SelectFirmwareFile), (gpointer)0);
    gtk_menu_shell_append(GTK_MENU_SHELL(pSubmenu), pMenuItem);
#endif
}

static void desmume_gtk_menu_tools (GtkWidget *pMenuBar)
{
    GtkWidget *pMenu, *pMenuItem;
    gint i;

    pMenu = gtk_menu_new();
    for(i = 0; i < dTools_list_size; i++) {
        pMenuItem = gtk_menu_item_new_with_label(dTools_list[i]->name);
        g_signal_connect(G_OBJECT(pMenuItem), "activate", G_CALLBACK(Start_dTool), GINT_TO_POINTER(i));
        gtk_menu_shell_append(GTK_MENU_SHELL(pMenu), pMenuItem);
    }

    pMenuItem = gtk_menu_item_new_with_label("Tools");
    gtk_menu_item_set_submenu(GTK_MENU_ITEM(pMenuItem), pMenu);
    gtk_menu_shell_append(GTK_MENU_SHELL(pMenuBar), pMenuItem);
}

static void desmume_gtk_menu_help (GtkWidget *pMenuBar)
{
    GtkWidget *pMenu, *pMenuItem;

    pMenu = gtk_menu_new();
    pMenuItem = gtk_image_menu_item_new_from_stock(GTK_STOCK_ABOUT,NULL);
    g_signal_connect(G_OBJECT(pMenuItem), "activate", G_CALLBACK(About), (GtkWidget*) pWindow);
    gtk_menu_shell_append(GTK_MENU_SHELL(pMenu), pMenuItem);

    pMenuItem = gtk_menu_item_new_with_label("Help");
    gtk_menu_item_set_submenu(GTK_MENU_ITEM(pMenuItem), pMenu);
    gtk_menu_shell_append(GTK_MENU_SHELL(pMenuBar), pMenuItem);
}

static void desmume_gtk_toolbar (GtkWidget *pVBox)
{
    GtkWidget *pToolbar;
    
    pToolbar = gtk_toolbar_new();
    gtk_box_pack_start(GTK_BOX(pVBox), pToolbar, FALSE, FALSE, 0);

    gtk_toolbar_insert(GTK_TOOLBAR(pToolbar), GTK_TOOL_ITEM(gtk_action_create_tool_item(gtk_action_group_get_action(action_group, "open"))), -1);
    gtk_toolbar_insert(GTK_TOOLBAR(pToolbar), GTK_TOOL_ITEM(gtk_action_create_tool_item(gtk_action_group_get_action(action_group, "run"))), -1);
    gtk_toolbar_insert(GTK_TOOLBAR(pToolbar), GTK_TOOL_ITEM(gtk_action_create_tool_item(gtk_action_group_get_action(action_group, "pause"))), -1);
    gtk_toolbar_insert(GTK_TOOLBAR(pToolbar), GTK_TOOL_ITEM(gtk_action_create_tool_item(gtk_action_group_get_action(action_group, "quit"))), -1);
}

static int
common_gtk_main( struct configured_features *my_config)
{
    SDL_TimerID limiter_timer = NULL;
    gchar *config_file;

    GtkAccelGroup * accel_group;
    GtkWidget *pVBox;
    GtkWidget *pMenuBar;
    gint pStatusBar_Ctx;

#ifdef GTKGLEXT_AVAILABLE
        GdkGLConfig *glconfig;
        GdkGLContext *glcontext;
#endif
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

#ifdef GTKGLEXT_AVAILABLE
        /* Try double-buffered visual */
        glconfig = gdk_gl_config_new_by_mode ((GdkGLConfigMode)(GDK_GL_MODE_RGB |
                                              GDK_GL_MODE_DEPTH |
                                              GDK_GL_MODE_DOUBLE));
        if (glconfig == NULL) {
            g_printerr ("*** Cannot find the double-buffered visual.\n");
            g_printerr ("*** Trying single-buffered visual.\n");

            /* Try single-buffered visual */
            glconfig = gdk_gl_config_new_by_mode ((GdkGLConfigMode)(GDK_GL_MODE_RGB |
                                                  GDK_GL_MODE_DEPTH));
            if (glconfig == NULL) {
              g_printerr ("*** No appropriate OpenGL-capable visual found.\n");
              exit (1);
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

    accel_group = gtk_accel_group_new();
    action_group = gtk_action_group_new("dui");
    gtk_action_group_add_actions(action_group, action_entries, sizeof(action_entries) / sizeof(GtkActionEntry), pWindow);
    {
        GList * list = gtk_action_group_list_actions(action_group);
        g_list_foreach(list, dui_set_accel_group, accel_group);
    }
    gtk_window_add_accel_group(GTK_WINDOW(pWindow), accel_group);
    gtk_action_set_sensitive(gtk_action_group_get_action(action_group, "pause"), FALSE);
    gtk_action_set_sensitive(gtk_action_group_get_action(action_group, "run"), FALSE);
    gtk_action_set_sensitive(gtk_action_group_get_action(action_group, "reset"), FALSE);
    gtk_action_set_sensitive(gtk_action_group_get_action(action_group, "printscreen"), FALSE);

    /* Menu and Toolbar */
    pMenuBar = gtk_menu_bar_new();

    desmume_gtk_menu_file(pMenuBar);
    desmume_gtk_menu_emulation(pMenuBar, my_config->opengl_2d);
    desmume_gtk_menu_config(pMenuBar, my_config->savetype);
    desmume_gtk_menu_tools(pMenuBar);
    desmume_gtk_menu_help(pMenuBar);

    gtk_box_pack_start(GTK_BOX(pVBox), pMenuBar, FALSE, FALSE, 0);

    desmume_gtk_toolbar(pVBox);

    /* Creating the place for showing DS screens */
#ifdef GTKGLEXT_AVAILABLE
        if ( my_config->opengl_2d) {
          /*
           * Create the top screen render area
           */
          top_screen_widget = gtk_drawing_area_new();
          gtk_drawing_area_size(GTK_DRAWING_AREA(top_screen_widget), 256, 192);
          gtk_widget_set_gl_capability ( top_screen_widget,
                                         glconfig,
                                         NULL,
                                         TRUE,
                                         GDK_GL_RGBA_TYPE);

          g_signal_connect_after (G_OBJECT (top_screen_widget), "realize",
                                  G_CALLBACK (gtk_init_main_gl_area),
                                  NULL);
          gtk_widget_set_events( top_screen_widget, GDK_EXPOSURE_MASK);
          g_signal_connect( G_OBJECT(top_screen_widget), "expose_event",
                            G_CALLBACK(top_screen_expose_fn),
                            &my_config->soft_colour) ;
          g_signal_connect( G_OBJECT(top_screen_widget), "configure_event",
                            G_CALLBACK(common_configure_fn), NULL ) ;

          gtk_box_pack_start(GTK_BOX(pVBox), top_screen_widget, TRUE, TRUE, 0);

          /* realise the topscreen so we can get the openGL context */
          gtk_widget_realize ( top_screen_widget);
          glcontext = gtk_widget_get_gl_context( top_screen_widget);

          LOG("Window is direct? %d\n",
            gdk_gl_context_is_direct( glcontext));

          /*
           *create the bottom screen drawing area.
           */
          bottom_screen_widget = gtk_drawing_area_new();
          gtk_drawing_area_size(GTK_DRAWING_AREA(bottom_screen_widget), 256, 192);
          gtk_widget_set_gl_capability ( bottom_screen_widget,
                                         glconfig,
                                         glcontext,
                                         TRUE,
                                         GDK_GL_RGBA_TYPE);

          g_signal_connect_after (G_OBJECT (bottom_screen_widget), "realize",
                                  G_CALLBACK (gtk_init_sub_gl_area),
                                  NULL);
          gtk_widget_set_events( bottom_screen_widget,
                                 GDK_EXPOSURE_MASK | GDK_LEAVE_NOTIFY_MASK |
                                 GDK_BUTTON_PRESS_MASK | GDK_BUTTON_RELEASE_MASK |
                                 GDK_POINTER_MOTION_MASK | GDK_KEY_PRESS_MASK );
          g_signal_connect( G_OBJECT(bottom_screen_widget), "expose_event",
                            G_CALLBACK(bottom_screen_expose_fn), NULL ) ;
          g_signal_connect( G_OBJECT(bottom_screen_widget), "configure_event",
                            G_CALLBACK(common_configure_fn), NULL ) ;
          g_signal_connect(G_OBJECT(bottom_screen_widget), "button_press_event",
                           G_CALLBACK(Stylus_Press), &my_config->opengl_2d);
          g_signal_connect(G_OBJECT(bottom_screen_widget), "button_release_event",
                           G_CALLBACK(Stylus_Release), NULL);
          g_signal_connect(G_OBJECT(bottom_screen_widget), "motion_notify_event",
                           G_CALLBACK(Stylus_Move), &my_config->opengl_2d);

          gtk_box_pack_start(GTK_BOX(pVBox), bottom_screen_widget, TRUE, TRUE, 0);

          /* each frame expose the top screen */
          nds_screen_widget = top_screen_widget;
        } else {
#else
    {
#endif
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

            g_signal_connect( G_OBJECT(pDrawingArea), "expose_event",
                              G_CALLBACK(gtkFloatExposeEvent), NULL ) ;

            gtk_box_pack_start(GTK_BOX(pVBox), pDrawingArea, FALSE, FALSE, 0);

            nds_screen_widget = pDrawingArea;
        }

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
    /* setup the gdk 3D emulation; GTKGLEXT and LIBOSMESA are exclusive currently */
#if defined(GTKGLEXT_AVAILABLE) || defined(HAVE_LIBOSMESA)
    if(my_config->engine_3d == 2){
#if defined(GTKGLEXT_AVAILABLE) 
        core = init_opengl_gdk_3Demu(GDK_DRAWABLE(pWindow->window)) ? 2 : GPU3D_NULL;
#else
        core = init_osmesa_3Demu() ? 2 : GPU3D_NULL;
#endif
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

#ifdef GTKGLEXT_AVAILABLE
  gtk_gl_init( &argc, &argv);
#endif

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

