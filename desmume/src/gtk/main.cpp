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
#ifdef GTKGLEXT_AVAILABLE
#include <GL/gl.h>
#include <GL/glu.h>
#include <gtk/gtkgl.h>
#endif

#include <gtk/gtk.h>

#include "globals.h"
#include "desmume.h"
#include "../debug.h"

#ifdef GDB_STUB
#include "../gdbstub.h"
#endif

#ifdef GTKGLEXT_AVAILABLE
#include "../OGLRender.h"
#include "gdk_3Demu.h"
#endif

#include "DeSmuME.xpm"

#define EMULOOP_PRIO (G_PRIORITY_HIGH_IDLE + 20)

#define ENABLE_MEMORY_PROFILING 1

#ifdef ENABLE_MEMORY_PROFILING
#include <gdk/gdkkeysyms.h>
#endif

static const char *bad_glob_cflash_disk_image_file;


#define FPS_LIMITER_FRAME_PERIOD 8
static SDL_sem *fps_limiter_semaphore;
static int gtk_fps_limiter_disabled = 0;


/************************ CONFIG FILE *****************************/

// extern char FirmwareFile[256];
// int LoadFirmware(const char *filename);

static void Open_Select(GtkWidget* widget, gpointer data);
static void Launch();
static void Pause();
static void Printscreen();
static void Reset();

static const GtkActionEntry action_entries[] = {
	{ "open",	"gtk-open",		"Open",		"<Ctrl>o",	NULL,	G_CALLBACK(Open_Select) },
	{ "run",	"gtk-media-play",	"Run",		"<Ctrl>r",	NULL,	G_CALLBACK(Launch) },
	{ "pause",	"gtk-media-pause",	"Pause",	"<Ctrl>p",	NULL,	G_CALLBACK(Pause) },
	{ "quit",	"gtk-quit",		"Quit",		"<Ctrl>q",	NULL,	G_CALLBACK(gtk_main_quit) },
	{ "printscreen",NULL,			"Printscreen",	NULL,		NULL,	G_CALLBACK(Printscreen) },
	{ "reset",	"gtk-refresh",		"Reset",	NULL,		NULL,	G_CALLBACK(Reset) }
};

GtkActionGroup * action_group;

SoundInterface_struct *SNDCoreList[] = {
&SNDDummy,
&SNDFile,
&SNDSDL,
NULL
};

GPU3DInterface *core3DList[] = {
  &gpu3DNull
#ifdef GTKGLEXT_AVAILABLE
  ,
  &gpu3Dgl
#endif
};


struct configured_features {
  int opengl;
  int soft_colour;

  int disable_sound;
  int disable_3d;
  int disable_limiter;

  int arm9_gdb_port;
  int arm7_gdb_port;

  int firmware_language;

  const char *nds_file;
  const char *cflash_disk_image_file;
};

static void
init_configured_features( struct configured_features *config)
{
  config->arm9_gdb_port = 0;
  config->arm7_gdb_port = 0;

  config->disable_sound = 0;

  config->opengl = 0;
  config->soft_colour = 0;

  config->disable_3d = 0;

  config->disable_limiter = 0;

  config->nds_file = NULL;

  config->cflash_disk_image_file = NULL;

  /* use the default language */
  config->firmware_language = -1;
}

static int
fill_configured_features( struct configured_features *config,
                          int argc, char ** argv)
{
  GOptionEntry options[] = {
#ifdef GTKGLEXT_AVAILABLE
    { "opengl-2d", 0, 0, G_OPTION_ARG_NONE, &config->opengl, "Enables using OpenGL for screen rendering", NULL},
    { "soft-convert", 0, 0, G_OPTION_ARG_NONE, &config->soft_colour, "Use software colour conversion during OpenGL screen rendering."
									    " May produce better or worse frame rates depending on hardware", NULL},
    { "disable-3d", 0, 0, G_OPTION_ARG_NONE, &config->disable_3d, "Disables the 3D emulation", NULL},
#endif
    { "disable-sound", 0, 0, G_OPTION_ARG_NONE, &config->disable_sound, "Disables the sound emulation", NULL},
    { "disable-limiter", 0, 0, G_OPTION_ARG_NONE, &config->disable_limiter, "Disables the 60fps limiter", NULL},
    { "fwlang", 0, 0, G_OPTION_ARG_INT, &config->firmware_language, "Set the language in the firmware, LANG as follows:\n"
								    "  \t\t\t\t  0 = Japanese\n"
								    "  \t\t\t\t  1 = English\n"
								    "  \t\t\t\t  2 = French\n"
								    "  \t\t\t\t  3 = German\n"
								    "  \t\t\t\t  4 = Italian\n"
								    "  \t\t\t\t  5 = Spanish\n"
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
    fprintf (stderr, "Error parsing command line arguments: %s\n", error->message);
    g_error_free (error);
    return 0;
  }

  if (argc == 2)
    config->nds_file = argv[1];
  if (argc > 2)
    goto error;

  if (config->firmware_language < -1 || config->firmware_language > 5) {
    fprintf(stderr, "Firmware language must be set to a value from 0 to 5.\n");
    goto error;
  }

#ifdef GDB_STUB
  if (config->arm9_gdb_port < 1 || config->arm9_gdb_port > 65535) {
    fprintf(stderr, "ARM9 GDB stub port must be in the range 1 to 65535\n");
    goto error;
  }

  if (config->arm7_gdb_port < 1 || config->arm7_gdb_port > 65535) {
    fprintf(stderr, "ARM7 GDB stub port must be in the range 1 to 65535\n");
    goto error;
  }
#endif

  return 1;

error:
    fprintf(stderr, "USAGE: %s [options] [nds-file]\n", argv[0]);
    fprintf(stderr, "USAGE: %s --help    - for help\n", argv[0]);
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

// 	if(FirmwareFile[0]) {
// 		ini_add_section(ini, "FIRMWARE");
// 		ini_add_value(ini, "FIRMWARE", "FILE", FirmwareFile);
// 	}

	contents = g_key_file_to_data(keyfile, 0, 0);	
	ret = g_file_set_contents(config_file, contents, -1, NULL);
	if (!ret)
		fprintf(stderr, "Failed to write to %s\n", config_file);
	g_free (contents);

	g_key_file_free(keyfile);

	return 0;
}

static int Read_ConfigFile(const gchar *config_file)
{
	int i, tmp;
	GKeyFile * keyfile = g_key_file_new();
	GError * error = NULL;

	load_default_config();

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

//const gint StaCounter[20] = {0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19};

static GtkWidget *pWindow;
static GtkWidget *pStatusBar;
static GtkWidget *pToolbar;
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

static gint pStatusBar_Ctx;
#define pStatusBar_Change(t) gtk_statusbar_pop(GTK_STATUSBAR(pStatusBar), pStatusBar_Ctx); gtk_statusbar_push(GTK_STATUSBAR(pStatusBar), pStatusBar_Ctx, t)

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
        return NDS_LoadROM( filename, MC_TYPE_AUTODETECT, 1,
                             cflash_disk_image);
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
	gchar *sChemin;

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
	/* On limite les actions a cette fenetre */
	gtk_window_set_modal(GTK_WINDOW(pFileSelection), TRUE);

	gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(pFileSelection), pFilter_nds);
	gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(pFileSelection), pFilter_dsgba);
	gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(pFileSelection), pFilter_any);

	/* Affichage fenetre */
	switch(gtk_dialog_run(GTK_DIALOG(pFileSelection))) {
	case GTK_RESPONSE_OK:
		/* Recuperation du chemin */
		sChemin = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(pFileSelection));
		if(Open((const char*)sChemin, bad_glob_cflash_disk_image_file) < 0) {
			GtkWidget *pDialog = gtk_message_dialog_new(GTK_WINDOW(pFileSelection),
					GTK_DIALOG_MODAL,
					GTK_MESSAGE_ERROR,
					GTK_BUTTONS_OK,
					"Unable to load :\n%s", sChemin);
			gtk_dialog_run(GTK_DIALOG(pDialog));
			gtk_widget_destroy(pDialog);
		} else {
			gtk_action_set_sensitive(gtk_action_group_get_action(action_group, "run"), TRUE);
			gtk_action_set_sensitive(gtk_action_group_get_action(action_group, "reset"), TRUE);
		}

		//Launch(NULL, pWindow);

		g_free(sChemin);
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

int ScreenCoeff_Size;


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

  //printf("Doing GL init\n");

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
    fprintf( stderr, "Failed to init GL: %s\n", errString);
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
    fprintf( stderr, "Failed to init GL: %s\n", errString);
  }

  gdk_gl_drawable_gl_end (gldrawable);
  /*** OpenGL END ***/
}
#endif


/////////////////////////////// DRAWING SCREEN //////////////////////////////////

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
    int i;
    u8 converted[256 * 384 * 3];

    for ( i = 0; i < (256 * 384); i++) {
      converted[(i * 3) + 0] = ((*((u16 *)&GPU_screen[(i<<1)]) >> 0) & 0x1f) << 3;
      converted[(i * 3) + 1] = ((*((u16 *)&GPU_screen[(i<<1)]) >> 5) & 0x1f) << 3;
      converted[(i * 3) + 2] = ((*((u16 *)&GPU_screen[(i<<1)]) >> 10) & 0x1f) << 3;
    }

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
    fprintf( stderr, "GL subimage failed: %s\n", errString);
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
    fprintf( stderr, "GL draw failed: %s\n", errString);
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

  //g_print("Sub Expose\n");

  /*** OpenGL BEGIN ***/
  if (!gdk_gl_drawable_gl_begin (gldrawable, glcontext)) {
    g_print("begin failed\n");
    return FALSE;
  }
  //g_print("begin\n");

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
    fprintf( stderr, "sub GL draw failed: %s\n", errString);
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

  //g_print("wdith %d, height %d\n", event->width, event->height);

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

    /*
    printf("%d,%d\n", width, height);
    printf("l %lf, r %lf, t %lf, b %lf, other dimen %lf\n",
           left, right, top, bottom, other_dimen);
    */

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
    fprintf( stderr, "GL resie failed: %s\n", errString);
  }

  gdk_gl_drawable_gl_end (gldrawable);
  /*** OpenGL END ***/

  return TRUE;
}

#endif



/* Drawing callback */
static int gtkFloatExposeEvent (GtkWidget *widget, GdkEventExpose *event, gpointer data)
{
	SDL_PixelFormat screenPixFormat;
	SDL_Surface *rawImage, *screenImage;

	rawImage = SDL_CreateRGBSurfaceFrom((void*)&GPU_screen, 256, 192*2, 16, 512, 0x001F, 0x03E0, 0x7C00, 0);
	if(rawImage == NULL) return 1;

	memcpy(&screenPixFormat, rawImage->format, sizeof(SDL_PixelFormat));

	screenPixFormat.BitsPerPixel = 24;
	screenPixFormat.BytesPerPixel = 3;
	screenPixFormat.Rshift = 0;
	screenPixFormat.Gshift = 8;
	screenPixFormat.Bshift = 16;
	screenPixFormat.Rmask = 0x0000FF;
	screenPixFormat.Gmask = 0x00FF00;
	screenPixFormat.Bmask = 0xFF0000;

	screenImage = SDL_ConvertSurface(rawImage, &screenPixFormat, 0);

	gdk_draw_rgb_image	(widget->window,
							  widget->style->fg_gc[widget->state],0,0,screenImage->w, screenImage->h,
							  GDK_RGB_DITHER_NONE,(guchar*)screenImage->pixels,screenImage->pitch);
	SDL_FreeSurface(screenImage);
	SDL_FreeSurface(rawImage);

	return 1;
}

static void Draw()
{
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

                scaled_x = x * nds_screen_size_ratio;
                scaled_y = y * nds_screen_size_ratio;

	//	fprintf(stderr,"X=%d, Y=%d, S&1=%d\n", x,y,state&GDK_BUTTON1_MASK);

                if ( !(*opengl)) {
                  scaled_y -= 192;
                }
		if(scaled_y >= 0 && (state & GDK_BUTTON1_MASK)) {
                  EmuX = scaled_x;
                  EmuY = scaled_y;
                  if(EmuX<0) EmuX = 0; else if(EmuX>255) EmuX = 255;
                  if(EmuY<0) EmuY = 0; else if(EmuY>192) EmuY = 192;
                  NDS_setTouchPos(EmuX, EmuY);
		}
	}

	return TRUE;
}
static gboolean Stylus_Press(GtkWidget *w, GdkEventButton *e, gpointer data)
{
	GdkModifierType state;
	gint x,y;
	s32 EmuX, EmuY;
        const int *opengl = (const int *)data;

	if(desmume_running()) {
		if(e->button == 1) {
                  int scaled_x, scaled_y;
			click = TRUE;

			gdk_window_get_pointer(w->window, &x, &y, &state);

                        scaled_x = x * nds_screen_size_ratio;
                        scaled_y = y * nds_screen_size_ratio;

                        if ( !(*opengl)) {
                          scaled_y -= 192;
                        }
                        if(scaled_y >= 0 && (state & GDK_BUTTON1_MASK)) {
                            EmuX = scaled_x;
                            EmuY = scaled_y;
                            if(EmuX<0) EmuX = 0; else if(EmuX>255) EmuX = 255;
                            if(EmuY<0) EmuY = 0; else if(EmuY>192) EmuY = 192;
                            NDS_setTouchPos(EmuX, EmuY);
			}
		}
	}

	return TRUE;
}
static gboolean Stylus_Release(GtkWidget *w, GdkEventButton *e, gpointer data)
{
	if(click) NDS_releasTouch();
	click = FALSE;
	return TRUE;
}

static u16 Cur_Keypad = 0;

static gint Key_Press(GtkWidget *w, GdkEventKey *e)
{
  u16 Key = lookup_key(e->keyval);
  ADD_KEY( Cur_Keypad, Key );
  if(desmume_running()) update_keypad(Cur_Keypad);

#ifdef ENABLE_MEMORY_PROFILING
  if ( e->keyval == GDK_Tab) {
    print_memory_profiling();
  }
#endif
  return 1;
}

static gint Key_Release(GtkWidget *w, GdkEventKey *e)
{
  u16 Key = lookup_key(e->keyval);
  RM_KEY( Cur_Keypad, Key );
  if(desmume_running()) update_keypad(Cur_Keypad);
  return 1;
}

/////////////////////////////// CONTROLS EDIT //////////////////////////////////////

GtkWidget *mkLabel;
gint Modify_Key_Chosen = 0;

static void Modify_Key_Press(GtkWidget *w, GdkEventKey *e)
{
	gchar *YouPressed;

	Modify_Key_Chosen = e->keyval;
	YouPressed = g_strdup_printf("You pressed : %s\nClick OK to keep this key.", gdk_keyval_name(e->keyval));
	gtk_label_set(GTK_LABEL(mkLabel), YouPressed);
	g_free(YouPressed);
}

static void Modify_Key(GtkWidget* widget, gpointer data)
{
	gint Key = GPOINTER_TO_INT(data);
	GtkWidget *mkDialog;
	gchar *Key_Label;
	gchar *Title;

	Title = g_strdup_printf("Press \"%s\" key ...\n", key_names[Key]);
	mkDialog = gtk_dialog_new_with_buttons(Title,
		GTK_WINDOW(pWindow),
		GTK_DIALOG_MODAL,
		GTK_STOCK_OK,GTK_RESPONSE_OK,
		GTK_STOCK_CANCEL,GTK_RESPONSE_CANCEL,
		NULL);

	g_signal_connect(G_OBJECT(mkDialog), "key_press_event", G_CALLBACK(Modify_Key_Press), NULL);

	mkLabel = gtk_label_new(Title);
	g_free(Title);
	gtk_box_pack_start(GTK_BOX(GTK_DIALOG(mkDialog)->vbox), mkLabel,TRUE, FALSE, 0);	

	gtk_widget_show_all(GTK_DIALOG(mkDialog)->vbox);

	switch(gtk_dialog_run(GTK_DIALOG(mkDialog))) {
	case GTK_RESPONSE_OK:
		Keypad_Temp[Key] = Modify_Key_Chosen;
		Key_Label = g_strdup_printf("%s (%d)", key_names[Key], Keypad_Temp[Key]);
		gtk_button_set_label(GTK_BUTTON(widget), Key_Label);
		g_free(Key_Label);
		break;
	case GTK_RESPONSE_CANCEL:
	case GTK_RESPONSE_NONE:
		Modify_Key_Chosen = 0;
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

#define MAX_SCREENCOEFF 4 

static void Modify_ScreenCoeff(GtkWidget* widget, gpointer data)
{
	int Size = GPOINTER_TO_INT(data);

	gtk_drawing_area_size(GTK_DRAWING_AREA(pDrawingArea), 256 * Size, 384 * Size);
	gtk_widget_set_usize (pDrawingArea, 256 * Size, 384 * Size);

	ScreenCoeff_Size = Size;

}

/////////////////////////////// LAYER HIDING /////////////////////////////////

static void Modify_Layer(GtkWidget* widget, gpointer data)
{
	int i;
	gchar *Layer = (gchar*)data;

	if(!desmume_running()) {
		return;
	}

	if(memcmp(Layer, "Sub", 3) == 0) {
		if(memcmp(Layer, "Sub BG", 6) == 0) {
			i = atoi(Layer + strlen("Sub BG "));
			if(gtk_check_menu_item_get_active(GTK_CHECK_MENU_ITEM(widget)) == TRUE) {
				if(!SubScreen.gpu->dispBG[i]) GPU_addBack(SubScreen.gpu, i);
			} else { 
				if(SubScreen.gpu->dispBG[i]) GPU_remove(SubScreen.gpu, i);
			}
		} else {
			/* TODO: Disable sprites */
		}
	} else {
		if(memcmp(Layer, "Main BG", 7) == 0) {
			i = atoi(Layer + strlen("Main BG "));
			if(gtk_check_menu_item_get_active(GTK_CHECK_MENU_ITEM(widget)) == TRUE) {
				if(!MainScreen.gpu->dispBG[i]) GPU_addBack(MainScreen.gpu, i);
			} else { 
				if(MainScreen.gpu->dispBG[i]) GPU_remove(MainScreen.gpu, i);
			}
		} else {
			/* TODO: Disable sprites */
		}
	}
	//fprintf(stderr,"Changed %s to %d\n",Layer,gtk_check_menu_item_get_active(GTK_CHECK_MENU_ITEM(widget)));
}

const char *Layers_Menu[10] =
{
	"Main BG 0",
	"Main BG 1",
	"Main BG 2",
	"Main BG 3",
	"Main OBJ",
	"SUB BG 0",
	"SUB BG 1",
	"SUB BG 2",
	"SUB BG 3",
	"SUB OBJ",
};

/////////////////////////////// PRINTSCREEN /////////////////////////////////
// TODO: improve (let choose filename, and use png)

typedef struct
{
    u32 header_size;
    s32 width;
    s32 height;
    u16 r1;
    u16 depth;
    u32 r2;
    u32 size;
    s32 r3,r4;
    u32 r5,r6;
}BmpImageHeader;

typedef struct
{
    u16 type;
    u32 size;
    u16 r1, r2;
    u32 data_offset;
}BmpFileHeader;


static int WriteBMP(const char *filename,u16 *bmp)
{
    BmpFileHeader  fileheader;
    BmpImageHeader imageheader;
    fileheader.size = 14;
    fileheader.type = 0x4D42;
    fileheader.r1 = 0;
    fileheader.r2=0;
    fileheader.data_offset = 14+40;

    imageheader.header_size = 40;
    imageheader.r1 = 1;
    imageheader.depth = 24;
    imageheader.r2 = 0;
    imageheader.size = 256 * 192*2 * 3;
    imageheader.r3 = 0;
    imageheader.r4 = 0;
    imageheader.r5 = 0;
    imageheader.r6 = 0;
    imageheader.width = 256;
    imageheader.height = 192*2;

    FILE *fichier = fopen(filename,"wb");
    if (!fichier)
        return 0;
    //fwrite(&fileheader, 1, 14, fichier);

    //fwrite(&imageheader, 1, 40, fichier);
    fwrite( &fileheader.type,  sizeof(fileheader.type),  1, fichier);
    fwrite( &fileheader.size,  sizeof(fileheader.size),  1, fichier);
    fwrite( &fileheader.r1,  sizeof(fileheader.r1),  1, fichier);
    fwrite( &fileheader.r2,  sizeof(fileheader.r2),  1, fichier);
    fwrite( &fileheader.data_offset,  sizeof(fileheader.data_offset),  1, fichier);
    fwrite( &imageheader.header_size, sizeof(imageheader.header_size), 1, fichier);
    fwrite( &imageheader.width, sizeof(imageheader.width), 1, fichier);
    fwrite( &imageheader.height, sizeof(imageheader.height), 1, fichier);
    fwrite( &imageheader.r1, sizeof(imageheader.r1), 1, fichier);
    fwrite( &imageheader.depth, sizeof(imageheader.depth), 1, fichier);
    fwrite( &imageheader.r2, sizeof(imageheader.r2), 1, fichier);
    fwrite( &imageheader.size, sizeof(imageheader.size), 1, fichier);
    fwrite( &imageheader.r3, sizeof(imageheader.r3), 1, fichier);
    fwrite( &imageheader.r4, sizeof(imageheader.r4), 1, fichier);
    fwrite( &imageheader.r5, sizeof(imageheader.r5), 1, fichier);
    fwrite( &imageheader.r6, sizeof(imageheader.r6), 1, fichier);
    for(int j=0; j<192*2; j++) {
      for(int i=0; i<256; i++) {
        u8 r,g,b;
        u16 pixel = bmp[i+(192*2-j)*256];
        r = pixel>>10;
        pixel-=r<<10;
        g = pixel>>5;
        pixel-=g<<5;
        b = pixel;
        r*=255/31;
        g*=255/31;
        b*=255/31;
        fwrite(&r, 1, sizeof(char), fichier); 
        fwrite(&g, 1, sizeof(char), fichier); 
        fwrite(&b, 1, sizeof(char), fichier); 
      }
    }
    fclose(fichier);
    return 1;
}

static void Printscreen()
{
	WriteBMP("./test.bmp",(u16 *)GPU_screen);
}

/////////////////////////////// DS CONFIGURATION //////////////////////////////////

#if 0

char FirmwareFile[256];

int LoadFirmware(const char *filename)
{
	int i;
	u32 size;
	FILE *f;

	strncpy(FirmwareFile, filename, 256);

	f = fopen(filename, "rb");
	if(!f) return -1;

	fseek(f, 0, SEEK_END);
	size = ftell(f);
	fseek(f, 0, SEEK_SET);

	if(size > MMU.spi7.fw.size) { fclose(f); return -1; }		/* this must be a small file*/

	i = fread(MMU.spi7.fw.data, size, 1, f);

	fclose(f);

	return i;
}

int SelectFirmwareFile_Load(GtkWidget *w, gpointer data)
{
	GtkFileFilter *pFilter_nds, *pFilter_bin, *pFilter_any;
	GtkWidget *pFileSelection;
	GtkWidget *pParent;
	gchar *sChemin;

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

	/* Creation de la fenetre de selection */
	pFileSelection = gtk_file_chooser_dialog_new("Load firmware...",
			GTK_WINDOW(pParent),
			GTK_FILE_CHOOSER_ACTION_OPEN,
			GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
			GTK_STOCK_OPEN, GTK_RESPONSE_OK,
			NULL);
	/* On limite les actions a cette fenetre */
	gtk_window_set_modal(GTK_WINDOW(pFileSelection), TRUE);

	gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(pFileSelection), pFilter_nds);
	gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(pFileSelection), pFilter_bin);
	gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(pFileSelection), pFilter_any);

	if(FirmwareFile[0]) gtk_file_chooser_select_uri(GTK_FILE_CHOOSER(pFileSelection), FirmwareFile);

	/* Affichage fenetre */
	switch(gtk_dialog_run(GTK_DIALOG(pFileSelection))) {
	case GTK_RESPONSE_OK:
		/* Recuperation du chemin */
		sChemin = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(pFileSelection));
		if(LoadFirmware((const char*)sChemin) < 0) {
			GtkWidget *pDialog = gtk_message_dialog_new(GTK_WINDOW(pFileSelection), GTK_DIALOG_MODAL, GTK_MESSAGE_ERROR, GTK_BUTTONS_OK, "Unable to load :\n%s", sChemin);
			gtk_dialog_run(GTK_DIALOG(pDialog));
			gtk_widget_destroy(pDialog);
		} else {
			GtkWidget *pDialog = gtk_message_dialog_new(GTK_WINDOW(pFileSelection), GTK_DIALOG_MODAL, GTK_MESSAGE_INFO, GTK_BUTTONS_OK, "Selected firmware :\n%s", sChemin);
			gtk_dialog_run(GTK_DIALOG(pDialog));
			gtk_widget_destroy(pDialog);
		}

		g_free(sChemin);
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
	gchar *sChemin;

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

	/* Creation de la fenetre de selection */
	pFileSelection = gtk_file_chooser_dialog_new("Save firmware...",
			GTK_WINDOW(pParent),
			GTK_FILE_CHOOSER_ACTION_OPEN,
			GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
			GTK_STOCK_SAVE, GTK_RESPONSE_OK,
			NULL);
	/* On limite les actions a cette fenetre */
	gtk_window_set_modal(GTK_WINDOW(pFileSelection), TRUE);

	gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(pFileSelection), pFilter_nds);
	gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(pFileSelection), pFilter_bin);
	gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(pFileSelection), pFilter_any);

	if(FirmwareFile[0]) gtk_file_chooser_select_uri(GTK_FILE_CHOOSER(pFileSelection), FirmwareFile);

	/* Affichage fenetre */
	switch(gtk_dialog_run(GTK_DIALOG(pFileSelection))) {
	case GTK_RESPONSE_OK:
		/* Recuperation du chemin */
		sChemin = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(pFileSelection));
		if(LoadFirmware((const char*)sChemin) < 0) {
			GtkWidget *pDialog = gtk_message_dialog_new(GTK_WINDOW(pFileSelection), GTK_DIALOG_MODAL, GTK_MESSAGE_ERROR, GTK_BUTTONS_OK, "Unable to load :\n%s", sChemin);
			gtk_dialog_run(GTK_DIALOG(pDialog));
			gtk_widget_destroy(pDialog);
		} else {
			GtkWidget *pDialog = gtk_message_dialog_new(GTK_WINDOW(pFileSelection), GTK_DIALOG_MODAL, GTK_MESSAGE_INFO, GTK_BUTTONS_OK, "Selected firmware :\n%s", sChemin);
			gtk_dialog_run(GTK_DIALOG(pDialog));
			gtk_widget_destroy(pDialog);
		}

		g_free(sChemin);
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

Uint32 fps, fps_SecStart, fps_FrameCount;

gboolean EmuLoop(gpointer data)
{
	unsigned int i;
	gchar *Title;

	if(desmume_running()) {	/* Si on est en train d'executer le programme ... */
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

		desmume_cycle();	/* Emule ! */
		for(i = 0; i < Frameskip; i++) desmume_cycle();	/* cycles supplémentaires pour le frameskip */

		Draw();

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

static int
common_gtk_main( struct configured_features *my_config)
{
	gchar * config_file;
	int i;
	SDL_TimerID limiter_timer;

	GtkWidget *pVBox;
	GtkWidget *pMenuBar;
	GtkWidget *pMenu;
	GtkWidget *pMenuItem;
	GtkAccelGroup * accel_group;
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

#ifdef DEBUG
        LogStart();
#endif

#ifdef GDB_STUB
        if ( my_config->arm9_gdb_port != 0) {
          arm9_gdb_stub = createStub_gdb( my_config->arm9_gdb_port,
                                          &arm9_memio,
                                          &arm9_base_memory_iface);

          if ( arm9_gdb_stub == NULL) {
            fprintf( stderr, "Failed to create ARM9 gdbstub on port %d\n",
                     my_config->arm9_gdb_port);
            exit( -1);
          }
        }
        if ( my_config->arm7_gdb_port != 0) {
          arm7_gdb_stub = createStub_gdb( my_config->arm7_gdb_port,
                                          &arm7_memio,
                                          &arm7_base_memory_iface);

          if ( arm7_gdb_stub == NULL) {
            fprintf( stderr, "Failed to create ARM7 gdbstub on port %d\n",
                     my_config->arm7_gdb_port);
            exit( -1);
          }
        }
#endif

#ifdef GTKGLEXT_AVAILABLE
        /* Try double-buffered visual */
        glconfig = gdk_gl_config_new_by_mode ((GdkGLConfigMode)(GDK_GL_MODE_RGB    |
                                              GDK_GL_MODE_DEPTH  |
                                              GDK_GL_MODE_DOUBLE));
        if (glconfig == NULL) {
            g_print ("*** Cannot find the double-buffered visual.\n");
            g_print ("*** Trying single-buffered visual.\n");

            /* Try single-buffered visual */
            glconfig = gdk_gl_config_new_by_mode ((GdkGLConfigMode)(GDK_GL_MODE_RGB   |
                                                  GDK_GL_MODE_DEPTH));
            if (glconfig == NULL) {
              g_print ("*** No appropriate OpenGL-capable visual found.\n");
              exit (1);
            }
        }
#endif
        /* FIXME: SDL_INIT_VIDEO is needed for joystick support to work!?
           Perhaps it needs a "window" to catch events...? */
	if(SDL_Init(SDL_INIT_TIMER|SDL_INIT_VIDEO) == -1) {
            fprintf(stderr, "Error trying to initialize SDL: %s\n",
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

	/* Creation de la fenetre */
	pWindow = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	gtk_window_set_title(GTK_WINDOW(pWindow), "Desmume");

        if ( my_config->opengl) {
          gtk_window_set_resizable(GTK_WINDOW (pWindow), TRUE);
        } else {
          gtk_window_set_resizable(GTK_WINDOW (pWindow), FALSE);
        }

	gtk_window_set_icon(GTK_WINDOW (pWindow), gdk_pixbuf_new_from_xpm_data(DeSmuME_xpm));

	g_signal_connect(G_OBJECT(pWindow), "destroy", G_CALLBACK(gtk_main_quit), NULL);
	g_signal_connect(G_OBJECT(pWindow), "key_press_event", G_CALLBACK(Key_Press), NULL);
	g_signal_connect(G_OBJECT(pWindow), "key_release_event", G_CALLBACK(Key_Release), NULL);

	/* Creation de la GtkVBox */
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

	/**** Creation du menu ****/

	pMenuBar = gtk_menu_bar_new();

	/** Menu "Fichier" **/

	pMenu = gtk_menu_new();

	gtk_container_add(GTK_CONTAINER(pMenu), gtk_action_create_menu_item(gtk_action_group_get_action(action_group, "open")));
	gtk_container_add(GTK_CONTAINER(pMenu), gtk_action_create_menu_item(gtk_action_group_get_action(action_group, "printscreen")));
	gtk_container_add(GTK_CONTAINER(pMenu), gtk_action_create_menu_item(gtk_action_group_get_action(action_group, "quit")));

	pMenuItem = gtk_menu_item_new_with_label("File");
	gtk_menu_item_set_submenu(GTK_MENU_ITEM(pMenuItem), pMenu);
	gtk_menu_shell_append(GTK_MENU_SHELL(pMenuBar), pMenuItem);

	/** Menu "Emulation" **/
	GtkWidget *mEmulation;
	GtkWidget *mFrameskip;
	GtkWidget *mFrameskip_Radio[MAX_FRAMESKIP];
	GtkWidget *mGraphics;
	GtkWidget *mSize;
	GtkWidget *mSize_Radio[MAX_SCREENCOEFF];
	GtkWidget *mLayers;
	GtkWidget *mLayers_Radio[10];
	gchar *buf;

	mEmulation = gtk_menu_new();
	pMenuItem = gtk_menu_item_new_with_label("Emulation");
	gtk_menu_item_set_submenu(GTK_MENU_ITEM(pMenuItem), mEmulation);
	gtk_menu_shell_append(GTK_MENU_SHELL(pMenuBar), pMenuItem);

	gtk_container_add(GTK_CONTAINER(mEmulation), gtk_action_create_menu_item(gtk_action_group_get_action(action_group, "run")));

	gtk_container_add(GTK_CONTAINER(mEmulation), gtk_action_create_menu_item(gtk_action_group_get_action(action_group, "pause")));

	gtk_container_add(GTK_CONTAINER(mEmulation), gtk_action_create_menu_item(gtk_action_group_get_action(action_group, "reset")));

	mFrameskip = gtk_menu_new();
	pMenuItem = gtk_menu_item_new_with_label("Frameskip");
	gtk_menu_item_set_submenu(GTK_MENU_ITEM(pMenuItem), mFrameskip);
	gtk_menu_shell_append(GTK_MENU_SHELL(mEmulation), pMenuItem);

	for(i = 0; i < MAX_FRAMESKIP; i++) {
		buf = g_strdup_printf("%d", i);
		if (i>0)
			mFrameskip_Radio[i] = gtk_radio_menu_item_new_with_label_from_widget(GTK_RADIO_MENU_ITEM(mFrameskip_Radio[i-1]), buf);
		else
			mFrameskip_Radio[i] = gtk_radio_menu_item_new_with_label(NULL, buf);
		g_free(buf);
		g_signal_connect(G_OBJECT(mFrameskip_Radio[i]), "activate", G_CALLBACK(Modify_Frameskip), GINT_TO_POINTER(i));
		gtk_menu_shell_append(GTK_MENU_SHELL(mFrameskip), mFrameskip_Radio[i]);
	}
	gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(mFrameskip_Radio[0]), TRUE);

	mGraphics = gtk_menu_new();
	pMenuItem = gtk_menu_item_new_with_label("Graphics");
	gtk_menu_item_set_submenu(GTK_MENU_ITEM(pMenuItem), mGraphics);
	gtk_menu_shell_append(GTK_MENU_SHELL(mEmulation), pMenuItem);

// TODO: Un jour, peut être... ><
	if ( !my_config->opengl) {
		mSize = gtk_menu_new();
		pMenuItem = gtk_menu_item_new_with_label("Size");
		gtk_menu_item_set_submenu(GTK_MENU_ITEM(pMenuItem), mSize);
		gtk_menu_shell_append(GTK_MENU_SHELL(mGraphics), pMenuItem);

		for(i = 1; i < MAX_SCREENCOEFF; i++) {
			buf = g_strdup_printf("x%d", i);
			if (i>1)
				mSize_Radio[i] = gtk_radio_menu_item_new_with_label_from_widget(GTK_RADIO_MENU_ITEM(mSize_Radio[i-1]), buf);
			else
				mSize_Radio[i] = gtk_radio_menu_item_new_with_label(NULL, buf);
			g_free(buf);
			g_signal_connect(G_OBJECT(mSize_Radio[i]), "activate", G_CALLBACK(Modify_ScreenCoeff), GINT_TO_POINTER(i));
			gtk_menu_shell_append(GTK_MENU_SHELL(mSize), mSize_Radio[i]);
		}
	}
	gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(mFrameskip_Radio[0]), TRUE);

	mLayers = gtk_menu_new();
	pMenuItem = gtk_menu_item_new_with_label("Layers");
	gtk_menu_item_set_submenu(GTK_MENU_ITEM(pMenuItem), mLayers);
	gtk_menu_shell_append(GTK_MENU_SHELL(mGraphics), pMenuItem);

	for(i = 0; i < 10; i++) {
		mLayers_Radio[i] = gtk_check_menu_item_new_with_label(Layers_Menu[i]);
		g_signal_connect(G_OBJECT(mLayers_Radio[i]), "activate", G_CALLBACK(Modify_Layer), (void*)Layers_Menu[i]);
		gtk_menu_shell_append(GTK_MENU_SHELL(mLayers), mLayers_Radio[i]);
		gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(mLayers_Radio[i]), TRUE);
	}


	/** Menu "Options" **/
	GtkWidget *mConfig = gtk_menu_new();
	pMenuItem = gtk_menu_item_new_with_label("Config");
	gtk_menu_item_set_submenu(GTK_MENU_ITEM(pMenuItem), mConfig);
	gtk_menu_shell_append(GTK_MENU_SHELL(pMenuBar), pMenuItem);

	pMenuItem = gtk_menu_item_new_with_label("Edit controls");
	g_signal_connect(G_OBJECT(pMenuItem), "activate", G_CALLBACK(Edit_Controls), (GtkWidget*) pWindow);
	gtk_menu_shell_append(GTK_MENU_SHELL(mConfig), pMenuItem);

#if 0
	GtkWidget *mFirmware;

	mFirmware = gtk_menu_new();
	pMenuItem = gtk_menu_item_new_with_label("Firmware");
	gtk_menu_item_set_submenu(GTK_MENU_ITEM(pMenuItem), mFirmware);
	gtk_menu_shell_append(GTK_MENU_SHELL(mConfig), pMenuItem);

	pMenuItem = gtk_menu_item_new_with_label("Select...");
	g_signal_connect(G_OBJECT(pMenuItem), "activate", G_CALLBACK(SelectFirmwareFile), (gpointer)0);
	gtk_menu_shell_append(GTK_MENU_SHELL(mFirmware), pMenuItem);

	pMenuItem = gtk_menu_item_new_with_label("Config");
	gtk_menu_item_set_submenu(GTK_MENU_ITEM(pMenuItem), mConfig);
	gtk_menu_shell_append(GTK_MENU_SHELL(pMenuBar), pMenuItem);

#endif

	/** Menu "Outils" **/

	pMenu = gtk_menu_new();

	for(i = 0; i < dTools_list_size; i++) {
		pMenuItem = gtk_menu_item_new_with_label(dTools_list[i]->name);
		g_signal_connect(G_OBJECT(pMenuItem), "activate", G_CALLBACK(Start_dTool), GINT_TO_POINTER(i));
		gtk_menu_shell_append(GTK_MENU_SHELL(pMenu), pMenuItem);
	}

	pMenuItem = gtk_menu_item_new_with_label("Tools");
	gtk_menu_item_set_submenu(GTK_MENU_ITEM(pMenuItem), pMenu);
	gtk_menu_shell_append(GTK_MENU_SHELL(pMenuBar), pMenuItem);

	/** Menu "?" **/

	pMenu = gtk_menu_new();

#if ((GTK_MAJOR_VERSION >= 2) && (GTK_MINOR_VERSION >= 6))
	pMenuItem = gtk_image_menu_item_new_from_stock(GTK_STOCK_ABOUT,NULL);
#else
	pMenuItem = gtk_menu_item_new_with_label("About");
#endif
	g_signal_connect(G_OBJECT(pMenuItem), "activate", G_CALLBACK(About), (GtkWidget*) pWindow);
	gtk_menu_shell_append(GTK_MENU_SHELL(pMenu), pMenuItem);

	pMenuItem = gtk_menu_item_new_with_label("?");
	gtk_menu_item_set_submenu(GTK_MENU_ITEM(pMenuItem), pMenu);
	gtk_menu_shell_append(GTK_MENU_SHELL(pMenuBar), pMenuItem);

	/* Ajout du menu a la fenetre */
	gtk_box_pack_start(GTK_BOX(pVBox), pMenuBar, FALSE, FALSE, 0);

	/* CrÃ©ation de la Toolbar */

	pToolbar = gtk_toolbar_new();
	gtk_box_pack_start(GTK_BOX(pVBox), pToolbar, FALSE, FALSE, 0);

	gtk_toolbar_insert(GTK_TOOLBAR(pToolbar), GTK_TOOL_ITEM(gtk_action_create_tool_item(gtk_action_group_get_action(action_group, "open"))), -1);
	gtk_toolbar_insert(GTK_TOOLBAR(pToolbar), GTK_TOOL_ITEM(gtk_action_create_tool_item(gtk_action_group_get_action(action_group, "run"))), -1);
	gtk_toolbar_insert(GTK_TOOLBAR(pToolbar), GTK_TOOL_ITEM(gtk_action_create_tool_item(gtk_action_group_get_action(action_group, "pause"))), -1);
	gtk_toolbar_insert(GTK_TOOLBAR(pToolbar), GTK_TOOL_ITEM(gtk_action_create_tool_item(gtk_action_group_get_action(action_group, "quit"))), -1);

	/* CrÃ©ation de l'endroit pour l'affichage des Ã©crans */
#ifdef GTKGLEXT_AVAILABLE
        if ( my_config->opengl) {
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

          /*g_print("Window is direct? %d\n",
            gdk_gl_context_is_direct( glcontext));*/

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
                           G_CALLBACK(Stylus_Press), &my_config->opengl);
          g_signal_connect(G_OBJECT(bottom_screen_widget), "button_release_event",
                           G_CALLBACK(Stylus_Release), NULL);
          g_signal_connect(G_OBJECT(bottom_screen_widget), "motion_notify_event",
                           G_CALLBACK(Stylus_Move), &my_config->opengl);

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
                             G_CALLBACK(Stylus_Press), &my_config->opengl);
            g_signal_connect(G_OBJECT(pDrawingArea), "button_release_event",
                             G_CALLBACK(Stylus_Release), NULL);
            g_signal_connect(G_OBJECT(pDrawingArea), "motion_notify_event",
                             G_CALLBACK(Stylus_Move), &my_config->opengl);

            g_signal_connect( G_OBJECT(pDrawingArea), "realize",
                              G_CALLBACK(Draw), NULL ) ;
            g_signal_connect( G_OBJECT(pDrawingArea), "expose_event",
                              G_CALLBACK(gtkFloatExposeEvent), NULL ) ;

            gtk_box_pack_start(GTK_BOX(pVBox), pDrawingArea, FALSE, FALSE, 0);

            nds_screen_widget = pDrawingArea;
        }

	/* Création de la barre d'état */

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
            fprintf( stderr, "Error trying to start FPS limiter timer: %s\n",
                     SDL_GetError());
            return 1;
          }
	}

        /*
         * Set the 3D emulation to use
         */
        {
          int use_null_3d = my_config->disable_3d;

#ifdef GTKGLEXT_AVAILABLE
          if ( !use_null_3d) {
            /* setup the gdk 3D emulation */
            if ( init_opengl_gdk_3Demu()) {
              NDS_3D_SetDriver ( 1);

              if (!gpu3D->NDS_3D_Init ()) {
                fprintf( stderr, "Failed to initialise openGL 3D emulation; "
                         "removing 3D support\n");
                use_null_3d = 1;
              }
            }
            else {
              fprintf( stderr, "Failed to setup openGL 3D emulation; "
                       "removing 3D support\n");
              use_null_3d = 1;
            }
          }
#endif

          if ( use_null_3d) {
            NDS_3D_SetDriver ( 0);
            gpu3D->NDS_3D_Init();
          }
        }


	/* Vérifie la ligne de commandes */
	if( my_config->nds_file != NULL) {
		if(Open( my_config->nds_file, bad_glob_cflash_disk_image_file) >= 0) {
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

	/* Boucle principale */

//	gtk_idle_add(&EmuLoop, pWindow);
//	g_idle_add(&EmuLoop, pWindow);

	gtk_main();

	desmume_free();

        if ( !gtk_fps_limiter_disabled) {
          /* tidy up the FPS limiter timer and semaphore */
          SDL_RemoveTimer( limiter_timer);
          SDL_DestroySemaphore( fps_limiter_semaphore);
        }

#ifdef DEBUG
        LogStop();
#endif
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

