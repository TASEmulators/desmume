/* main.c - this file is part of DeSmuME
 *
 * Copyright (C) 2007-2015 DeSmuME Team
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
#include "keyval_names.h"
#include "rasterize.h"
#include "desmume.h"
#include "firmware.h"
#include "../shared/desmume_config.h"

#ifdef GDB_STUB
#include "../armcpu.h"
#include "../gdbstub.h"
#endif

#ifdef GTKGLEXT_AVAILABLE
#include <gtk/gtkgl.h>
#include "../OGLRender.h"
#include "gdk_3Demu.h"
#endif

int glade_fps_limiter_disabled = 0;

GtkWidget * pWindow;
GtkWidget * pDrawingArea, * pDrawingArea2;
GladeXML  * xml, * xml_tools;

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
    GDK_o,         // BOOST
    GDK_BackSpace, // Lid
  };

SoundInterface_struct *SNDCoreList[] = {
&SNDDummy,
&SNDDummy,
&SNDSDL,
NULL
};

GPU3DInterface *core3DList[] = {
&gpu3DNull,
&gpu3DRasterize
#ifdef GTKGLEXT_AVAILABLE
  ,
  &gpu3Dgl
#endif
};

/*
 *
 * Command line handling
 *
 */
struct configured_features {
  int load_slot;
  int opengl_2d;
  int engine_3d;
  int disable_limiter;
  int savetype;

  u16 arm9_gdb_port;
  u16 arm7_gdb_port;

  int firmware_language;

  const char *nds_file;
};

static void
init_configured_features( struct configured_features *config) {
  config->load_slot = 0;
  config->arm9_gdb_port = 0;
  config->arm7_gdb_port = 0;

  config->opengl_2d = 0;
  config->engine_3d = 1;

  config->disable_limiter = 0;
  config->savetype = 0;

  config->nds_file = NULL;

  /* use the default language */
  config->firmware_language = -1;
}

static int
fill_configured_features( struct configured_features *config,
                          int argc, char ** argv) {
  int good_args = 1;
  int print_usage = 0;
  int i;

  for ( i = 1; i < argc && good_args; i++) {
    if ( strcmp( argv[i], "--help") == 0) {
      g_print( _("USAGE: %s [OPTIONS] [nds-file]\n"), argv[0]);
      g_print( _("OPTIONS:\n"));
      g_print( _("\
   --load-slot=NUM     Load game saved under NUM position.\n\n"));
      g_print( _("\
   --3d-engine=ENGINE  Selects 3D rendering engine\n\
                         0 = disabled\n\
                         1 = internal desmume rasterizer (default)\n"));
#ifdef GTKGLEXT_AVAILABLE
      g_print( _("\
                         2 = gtkglext off-screen 3d opengl\n\n"));
#endif
      g_print( _("\
   --disable-limiter   Disables the 60 fps limiter\n\n"));
      g_print( _("\
   --save-type=TYPE    Selects savetype:\n\
                         0 = Autodetect (default)\n\
                         1 = EEPROM 4kbit\n\
                         2 = EEPROM 64kbit\n\
                         3 = EEPROM 512kbit\n\
                         4 = FRAM 256kbit\n\
                         5 = FLASH 2mbit\n\
                         6 = FLASH 4mbit\n\
   \n"));
      g_print( _("\
   --fwlang=LANG       Set the language in the firmware, LANG as follows:\n\
                         0 = Japanese\n\
                         1 = English\n\
                         2 = French\n\
                         3 = German\n\
                         4 = Italian\n\
                         5 = Spanish\n\n"));
#ifdef GDB_STUB
      g_print( _("\
   --arm9gdb=PORT_NUM  Enable the ARM9 GDB stub on the given port\n\
   --arm7gdb=PORT_NUM  Enable the ARM7 GDB stub on the given port\n\n"));
#endif
      g_print( _("\
   --help              Display this message\n"));
      //g_print("   --sticky            Enable sticky keys and stylus\n");
      good_args = 0;
    }
    else if ( strncmp( argv[i], "--load-slot=", 12) == 0) {
      char *end_char;
      int slot = strtoul( &argv[i][12], &end_char, 10);

      if ( slot >= 0 && slot <= 10) {
        config->load_slot = slot;
      }
      else {
        g_printerr( _("I only know how to load from slots 1-10.\n"));
        good_args = 0;
      }
    }
#ifdef GTKGLEXT_AVAILABLE
    else if ( strcmp( argv[i], "--opengl-2d") == 0) {
        // FIXME: to be implemented
      config->opengl_2d = 1;
    }
#define MAX3DEMU 2
#else
#define MAX3DEMU 1
#endif
    else if ( strncmp( argv[i], "--3d-engine=", 12) == 0) {
      char *end_char;
      int engine = strtoul( &argv[i][12], &end_char, 10);

      if ( engine >= 0 && engine <= MAX3DEMU) {
        config->engine_3d = engine;
      }
      else {
        g_printerr( _("Supported 3d engines: 0, 1, and on some machines 2; use --help option for details\n"));
        good_args = 0;
      }
    }
    else if ( strncmp( argv[i], "--save-type=", 12) == 0) {
      char *end_char;
      int type = strtoul( &argv[i][12], &end_char, 10);

      if ( type >= 0 && type <= 6) {
        config->savetype = type;
      }
      else {
        g_printerr( _("select savetype from 0 to 6; use --help option for details\n"));
        good_args = 0;
      }
    }
    else if ( strncmp( argv[i], "--fwlang=", 9) == 0) {
      char *end_char;
      int lang = strtoul( &argv[i][9], &end_char, 10);

      if ( lang >= 0 && lang <= 5) {
        config->firmware_language = lang;
      }
      else {
        g_printerr( _("Firmware language must be set to a value from 0 to 5.\n"));
        good_args = 0;
      }
    }
#ifdef GDB_STUB
    else if ( strncmp( argv[i], "--arm9gdb=", 10) == 0) {
      char *end_char;
      unsigned long port_num = strtoul( &argv[i][10], &end_char, 10);

      if ( port_num > 0 && port_num < 65536) {
        config->arm9_gdb_port = port_num;
      }
      else {
        g_print( _("ARM9 GDB stub port must be in the range 1 to 65535\n"));
        good_args = 0;
      }
    }
    else if ( strncmp( argv[i], "--arm7gdb=", 10) == 0) {
      char *end_char;
      unsigned long port_num = strtoul( &argv[i][10], &end_char, 10);

      if ( port_num > 0 && port_num < 65536) {
        config->arm7_gdb_port = port_num;
      }
      else {
        g_print( _("ARM7 GDB stub port must be in the range 1 to 65535\n"));
        good_args = 0;
      }
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
        g_print( _("NDS file (\"%s\") already set\n"), config->nds_file);
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
    g_print( _("USAGE: %s [options] [nds-file]\n"), argv[0]);
    g_print( _("USAGE: %s --help    - for help\n"), argv[0]);
  }

  return good_args;
}



/* ***** ***** TOOLS ***** ***** */

GList * tools_to_update = NULL;

// register tool
void register_Tool(VoidFunPtr fun) {
	tools_to_update = g_list_append(tools_to_update, (void *) fun);
}
void unregister_Tool(VoidFunPtr fun) {
	if (tools_to_update == NULL) return;
	tools_to_update = g_list_remove(tools_to_update, (void *) fun);
}

static void notify_Tool (VoidFunPtr fun, gpointer func_data) {
	fun();
}

void notify_Tools() {
	g_list_foreach(tools_to_update, (GFunc)notify_Tool, NULL);
}

/* Return the glade directory. 
   Note: See configure.ac for the value of GLADEUI_UNINSTALLED_DIR. */
gchar * get_ui_file (const char *filename)
{
	gchar *path;

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


/*
 * The thread handling functions needed by the GDB stub code.
 */
#ifdef GDB_STUB
void *
createThread_gdb( void (*thread_function)( void *data),
                  void *thread_data) {
  GThread *new_thread = g_thread_create( (GThreadFunc)thread_function,
                                         thread_data,
                                         TRUE,
                                         NULL);

  return new_thread;
}

void
joinThread_gdb( void *thread_handle) {
  g_thread_join((GThread *) thread_handle);
}
#endif


/* ***** ***** MAIN ***** ***** */

static int
common_gtk_glade_main( struct configured_features *my_config) {
        /* the firmware settings */
        FirmwareConfig fw_config;
	gchar *uifile;
	GKeyFile *keyfile;

        /* default the firmware settings, they may get changed later */
        NDS_GetDefaultFirmwareConfig(fw_config);

        /* use any language set on the command line */
        if ( my_config->firmware_language != -1) {
          fw_config.language = my_config->firmware_language;
        }
        desmume_savetype(my_config->savetype);

#ifdef GTKGLEXT_AVAILABLE
// check if you have GTHREAD when running configure script
	//g_thread_init(NULL);
	//register_gl_fun(my_gl_Begin,my_gl_End);
#endif
	init_keyvals();

	if(SDL_Init( SDL_INIT_TIMER | SDL_INIT_VIDEO) == -1)
          {
            fprintf(stderr, _("Error trying to initialize SDL: %s\n"),
                    SDL_GetError());
            return 1;
          }

	desmume_init();

        /* Create the dummy firmware */
        NDS_InitFirmwareWithConfig(fw_config);

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

        /* Initialize joysticks */
        if(!init_joy()) return 1;

	keyfile = desmume_config_read_file(gtk_kb_cfg);

	/* load the interface */
	uifile        = get_ui_file("DeSmuMe.glade");
	xml           = glade_xml_new(uifile, NULL, NULL);
	g_free (uifile);
	uifile        = get_ui_file("DeSmuMe_Dtools.glade");
	xml_tools     = glade_xml_new(uifile, NULL, NULL);
	g_free (uifile);
	pWindow       = glade_xml_get_widget(xml, "wMainW");
	pDrawingArea  = glade_xml_get_widget(xml, "wDraw_Main");
	pDrawingArea2 = glade_xml_get_widget(xml, "wDraw_Sub");

    {
      char wdgName[40];
      snprintf(wdgName, 39, "savetype%d", my_config->savetype+1);
      GtkWidget * wdgt = glade_xml_get_widget(xml, wdgName);
      gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM (wdgt), TRUE);
    }

	/* connect the signals in the interface */
	glade_xml_signal_autoconnect_StringObject(xml);
	glade_xml_signal_autoconnect_StringObject(xml_tools);

	init_GL_capabilities();

	/* check command line file */
	if( my_config->nds_file) {
		if(desmume_open( my_config->nds_file) >= 0)	{
            loadstate_slot( my_config->load_slot);
			desmume_resume();
			enable_rom_features();
		} else {
			GtkWidget *pDialog = gtk_message_dialog_new(GTK_WINDOW(pWindow),
					GTK_DIALOG_MODAL,
					GTK_MESSAGE_INFO,
					GTK_BUTTONS_OK,
					_("Unable to load :\n%s"), my_config->nds_file);
			gtk_dialog_run(GTK_DIALOG(pDialog));
			gtk_widget_destroy(pDialog);
		}
	}

        gtk_widget_show(pDrawingArea);
        gtk_widget_show(pDrawingArea2);

        {
          int engine = my_config->engine_3d;

#ifdef GTKGLEXT_AVAILABLE
          if ( my_config->engine_3d==2 )
            /* setup the gdk 3D emulation */
              if(!init_opengl_gdk_3Demu(GDK_DRAWABLE(pWindow->window))){
                  fprintf( stderr, _("Failed to initialise openGL 3D emulation; "
                                     "removing 3D support\n"));
                  engine = 0;
              }
#endif
          if (!GPU->Change3DRendererByID(engine)) {
              GPU->Change3DRendererByID(RENDERID_SOFTRASTERIZER);
              fprintf(stderr, _("3D renderer initialization failed!\nFalling back to 3D core: %s\n"), core3DList[RENDERID_SOFTRASTERIZER]->name);
          }
        }

//	on_menu_tileview_activate(NULL,NULL);

        /* setup the frame limiter and indicate if it is disabled */
        glade_fps_limiter_disabled = my_config->disable_limiter;

	/* start event loop */
	gtk_main();
	desmume_free();

#ifdef GDB_STUB
    destroyStub_gdb( arm9_gdb_stub);
	arm9_gdb_stub = NULL;
	
    destroyStub_gdb( arm7_gdb_stub);
	arm7_gdb_stub = NULL;

    gdbstub_mutex_destroy();
#endif

        /* Unload joystick */
        uninit_joy();

	SDL_Quit();
	desmume_config_update_keys(keyfile);
	desmume_config_update_joykeys(keyfile);
	desmume_config_dispose(keyfile);
	return EXIT_SUCCESS;
}

int main(int argc, char *argv[]) {
  struct configured_features my_config;

  // Localization
  bindtextdomain (GETTEXT_PACKAGE, LOCALEDIR);
  bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
  textdomain (GETTEXT_PACKAGE);

  init_configured_features( &my_config);

  /* X11 multi-threading support */
  if(!XInitThreads())
    {
      fprintf(stderr, "Warning: X11 not thread-safe\n");
    }

#if !g_thread_supported()
  g_thread_init( NULL);
#endif

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
