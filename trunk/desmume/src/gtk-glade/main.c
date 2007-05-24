/* main.c - this file is part of DeSmuME
 *
 * Copyright (C) 2007 Damien Nozay (damdoum)
 * Copyright (C) 2007 Pascal Giard (evilynux)
 * Author: damdoum at users.sourceforge.net
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

#include "callbacks.h"
#include "callbacks_IO.h"
#include "dTools/callbacks_dtools.h"
#include "globals.h"

#ifdef GTKGLEXT_AVAILABLE
#include <gtk/gtkgl.h>
#include "../opengl_collector_3Demu.h"
#include "gdk_3Demu.h"
#endif

/*
 * The frame limiter semaphore
 */
SDL_sem *glade_fps_limiter_semaphore;
int glade_fps_limiter_disabled = 0;


GtkWidget * pWindow;
GtkWidget * pDrawingArea, * pDrawingArea2;
GladeXML  * xml, * xml_tools;

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
  &gpu3D_opengl_collector
#endif
};


/*
 *
 * Command line handling
 *
 */
struct configured_features {
  int software_colour_convert;
  int disable_3d;
  int disable_limiter;
  const char *nds_file;
};

static void
init_configured_features( struct configured_features *config) {
  config->software_colour_convert = 0;

  config->disable_3d = 0;

  config->disable_limiter = 0;

  config->nds_file = NULL;
}

static int
fill_configured_features( struct configured_features *config,
                          int argc, char ** argv) {
  int good_args = 1;
  int print_usage = 0;
  int i;

  for ( i = 1; i < argc && good_args; i++) {
    if ( strcmp( argv[i], "--help") == 0) {
      g_print( "USAGE: %s [OPTIONS] [nds-file]\n", argv[0]);
      g_print( "OPTIONS:\n");
#ifdef GTKGLEXT_AVAILABLE
      g_print( "   --soft-convert      Use software colour conversion during OpenGL\n");
      g_print( "                       screen rendering. May produce better or worse\n");
      g_print( "                       frame rates depending on hardware.\n");
      g_print( "\n");
      g_print( "   --disable-3d        Disables the 3D emulation\n");
      g_print( "\n");
#endif
      g_print( "   --disable-limiter   Disables the 60 fps limiter\n");
      g_print( "\n");
      g_print( "   --help              Display this message\n");
      good_args = 0;
    }
#ifdef GTKGLEXT_AVAILABLE
    else if ( strcmp( argv[i], "--soft-convert") == 0) {
      config->software_colour_convert = 1;
    }
    else if ( strcmp( argv[i], "--disable-3d") == 0) {
      config->disable_3d = 1;
    }
#endif
    else if ( strcmp( argv[i], "--disable-limiter") == 0) {
      config->disable_limiter = 1;
    }
    else {
      if ( config->nds_file == NULL) {
        config->nds_file = argv[i];
      }
      else {
        g_print( "NDS file (\"%s\") already set\n", config->nds_file);
        good_args = 0;
      }
    }
  }

  if ( good_args) {
    /*
     * check if the configured features are consistant
     */
  }

  if ( print_usage) {
    g_print( "USAGE: %s [options] [nds-file]\n", argv[0]);
    g_print( "USAGE: %s --help    - for help\n", argv[0]);
  }

  return good_args;
}



/* ***** ***** TOOLS ***** ***** */

GList * tools_to_update = NULL;

// register tool
void register_Tool(VoidFunPtr fun) {
	tools_to_update = g_list_append(tools_to_update, fun);
}
void unregister_Tool(VoidFunPtr fun) {
	if (tools_to_update == NULL) return;
	tools_to_update = g_list_remove(tools_to_update, fun);
}

void notify_Tool (VoidFunPtr fun, gpointer func_data) {
	fun();
}

void notify_Tools() {
	g_list_foreach(tools_to_update, (GFunc)notify_Tool, NULL);
}

/* Return the glade directory. 
   Note: See configure.ac for the value of GLADEUI_UNINSTALLED_DIR. */
gchar * get_ui_file (const char *filename)
{
	char *path;

	/* looking in uninstalled (aka building) dir first */
	path = g_build_filename (GLADEUI_UNINSTALLED_DIR, filename, NULL);
	if (g_file_test (path, G_FILE_TEST_IS_REGULAR)) return path;
	g_free (path);
	
	/* looking in installed dir */
	path = g_build_filename (DATADIR, filename, NULL);
	if (g_file_test (path, G_FILE_TEST_IS_REGULAR)) return path;
	g_free (path);
	
	/* not found */
	return NULL;
}


/* ***** ***** CONFIG FILE ***** ***** */
char * CONFIG_FILE;

int Read_ConfigFile()
{
	int i, tmp;
	GKeyFile * keyfile = g_key_file_new();
	GError * error = NULL;
	
	load_default_config();
	
	g_key_file_load_from_file(keyfile, CONFIG_FILE, G_KEY_FILE_NONE, 0);

	const char *c;

	/* Load keypad keys */
	for(i = 0; i < NB_KEYS; i++)
	{
		tmp = g_key_file_get_integer(keyfile, "KEYS", key_names[i], &error);
		if (error != NULL) {
                  g_error_free(error);
                  error = NULL;
		} else {
                  keyboard_cfg[i] = tmp;
		}
	}
		
	/* Load joypad keys */
	for(i = 0; i < NB_KEYS; i++)
	{
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

int Write_ConfigFile()
{
	int i;
	GKeyFile * keyfile;
	
	keyfile = g_key_file_new();
	
	for(i = 0; i < NB_KEYS; i++)
	{
		g_key_file_set_integer(keyfile, "KEYS", key_names[i], keyboard_cfg[i]);
		g_key_file_set_integer(keyfile, "JOYKEYS", key_names[i], joypad_cfg[i]);
	}
	
	g_file_set_contents(CONFIG_FILE, g_key_file_to_data(keyfile, 0, 0), -1, 0);
	g_key_file_free(keyfile);
	
	return 0;
}

/* ******** Savestate menu items handling ******** */

void set_menuitem_label(GtkWidget * w, char * text )
{
  GtkLabel * child;

  if ( GTK_BIN(w)->child )
    {
      child = (GtkLabel*)GTK_BIN(w)->child;
      gtk_label_set_text(child, text);
    }
}

void clear_savestate_menu(char * cb_name, u8 num)
{
  GtkWidget * w;
  char cb[40];
  char text[40];

  sprintf( cb, "%s%d", cb_name, num);
  sprintf( text, "State %d (empty)", num);
  w = glade_xml_get_widget(xml, cb);
  set_menuitem_label( w, text );
}

void update_savestate_menu(char * cb_name, u8 num)
{
  GtkWidget * w;
  char cb[40];

  sprintf( cb, "%s%d", cb_name, num);
  w = glade_xml_get_widget(xml, cb);
  set_menuitem_label( w, savestates[num-1].date );
}

void update_savestates_menu()
{
  char cb[15];
  u8 i;
  GtkWidget * w;

  for( i = 1; i <= NB_STATES; i++ )
    {
      if( savestates[i-1].exists == TRUE )
        {
          update_savestate_menu("loadstate", i);
          update_savestate_menu("savestate", i);
        }
      else
        {
          clear_savestate_menu("loadstate", i);
          clear_savestate_menu("savestate", i);
        }
    }
}

void update_savestate(u8 num)
{
  desmume_pause();
  savestate_slot(num);
  update_savestate_menu("savestate", num);
  update_savestate_menu("loadstate", num);
  desmume_resume();
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
static Uint32
glade_fps_limiter_fn( Uint32 interval, void *param) {
  SDL_sem *sdl_semaphore = (SDL_sem *)param;

  /* signal the semaphore if it is getting low */
  if ( SDL_SemValue( sdl_semaphore) < 4) {
    SDL_SemPost( sdl_semaphore);
  }

  return interval;
}

/* ***** ***** MAIN ***** ***** */

static int
common_gtk_glade_main( struct configured_features *my_config) {
	SDL_TimerID limiter_timer;


#ifdef GTKGLEXT_AVAILABLE
// check if you have GTHREAD when running configure script
	//g_thread_init(NULL);
	register_gl_fun(my_gl_Begin,my_gl_End);
#endif

#ifdef DEBUG
        LogStart();
#endif
	init_keyvals();

	if(SDL_Init( SDL_INIT_TIMER | SDL_INIT_VIDEO) == -1)
          {
            fprintf(stderr, "Error trying to initialize SDL: %s\n",
                    SDL_GetError());
            return 1;
          }


	desmume_init();
        /* Initialize joysticks */
        if(!init_joy()) return 1;

	CONFIG_FILE = g_build_filename(g_get_home_dir(), ".desmume.ini", NULL);
	Read_ConfigFile();

	/* load the interface */
	xml           = glade_xml_new(get_ui_file("DeSmuMe.glade"), NULL, NULL);
	xml_tools     = glade_xml_new(get_ui_file("DeSmuMe_Dtools.glade"), NULL, NULL);
	pWindow       = glade_xml_get_widget(xml, "wMainW");
	pDrawingArea  = glade_xml_get_widget(xml, "wDraw_Main");
	pDrawingArea2 = glade_xml_get_widget(xml, "wDraw_Sub");

	/* connect the signals in the interface */
	glade_xml_signal_autoconnect_StringObject(xml);
	glade_xml_signal_autoconnect_StringObject(xml_tools);

	init_GL_capabilities( my_config->software_colour_convert);

	/* check command line file */
	if( my_config->nds_file) {
		if(desmume_open( my_config->nds_file) >= 0)	{
			desmume_resume();
			enable_rom_features();
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

        gtk_widget_show(pDrawingArea);
        gtk_widget_show(pDrawingArea2);

        {
          int use_null_3d = my_config->disable_3d;

#ifdef GTKGLEXT_AVAILABLE
          if ( !use_null_3d) {
            /* setup the gdk 3D emulation */
            if ( init_opengl_gdk_3Demu()) {
              NDS_3D_SetDriver(1);

              if (!gpu3D->NDS_3D_Init()) {
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

//	on_menu_tileview_activate(NULL,NULL);

        /* setup the frame limiter and indicate if it is disabled */
        glade_fps_limiter_disabled = my_config->disable_limiter;

        if ( !glade_fps_limiter_disabled) {
          /* create the semaphore used for fps limiting */
          glade_fps_limiter_semaphore = SDL_CreateSemaphore( 1);

          /* start a SDL timer for every FPS_LIMITER_FRAME_PERIOD
           * frames to keep us at 60 fps */
          limiter_timer = SDL_AddTimer( 16 * FPS_LIMITER_FRAME_PERIOD,
                                        glade_fps_limiter_fn,
                                        glade_fps_limiter_semaphore);
          if ( limiter_timer == NULL) {
            fprintf( stderr, "Error trying to start FPS limiter timer: %s\n",
                     SDL_GetError());
            SDL_DestroySemaphore( glade_fps_limiter_semaphore);
            glade_fps_limiter_disabled = 1;
          }
        }

	/* start event loop */
	gtk_main();
	desmume_free();

        if ( !glade_fps_limiter_disabled) {
          /* tidy up the FPS limiter timer and semaphore */
          SDL_RemoveTimer( limiter_timer);
          SDL_DestroySemaphore( glade_fps_limiter_semaphore);
        }

#ifdef DEBUG
        LogStop();
#endif
        /* Unload joystick */
        uninit_joy();

	SDL_Quit();
	Write_ConfigFile();
	return EXIT_SUCCESS;
}

int main(int argc, char *argv[]) {
  struct configured_features my_config;

  init_configured_features( &my_config);
        
  gtk_init(&argc, &argv);

#ifdef GTKGLEXT_AVAILABLE
  gtk_gl_init( &argc, &argv);
#endif

  if ( !fill_configured_features( &my_config, argc, argv)) {
    exit(0);
  }

  return common_gtk_glade_main( &my_config);
}


#ifdef WIN32
int WinMain ( HINSTANCE hThisInstance, HINSTANCE hPrevInstance,
              LPSTR lpszArgument, int nFunsterStil)
{
  int argc = 0;
  char *argv[] = NULL;

  /*
   * FIXME:
   * Emulate the argc and argv main parameters. Could do this using
   * CommandLineToArgvW and then convert the wide chars to thin chars.
   * Or parse the wide chars directly and call common_gtk_glade_main with a
   * filled configuration structure.
   */
  main( argc, argv);
}
#endif

