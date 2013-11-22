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
#include <SDL.h>
#include <SDL_thread.h>
#include <stdlib.h>
#include <string.h>
#include <glib.h>

#ifndef VERSION
#define VERSION "Unknown version"
#endif

/*
 * FIXME: Not sure how to detect OpenGL in a platform portable way.
 */
#ifdef HAVE_GL_GL_H
#define INCLUDE_OPENGL_2D
#endif

#ifdef INCLUDE_OPENGL_2D
#include <GL/gl.h>
#include <GL/glu.h>
#endif

#ifndef CLI_UI
#define CLI_UI
#endif

#include "MMU.h"
#include "NDSSystem.h"
#include "debug.h"
#include "sndsdl.h"
#include "ctrlssdl.h"
#include "render3D.h"
#include "rasterize.h"
#include "saves.h"
#include "firmware.h"
#include "GPU_osd.h"
#include "desmume_config.h"
#include "commandline.h"
#include "slot2.h"
#ifdef GDB_STUB
#include "gdbstub.h"
#endif

volatile bool execute = false;

static float nds_screen_size_ratio = 1.0f;

#define DISPLAY_FPS

#ifdef DISPLAY_FPS
#define NUM_FRAMES_TO_TIME 60
#endif

#define FPS_LIMITER_FPS 60

static SDL_Surface * surface;

/* Flags to pass to SDL_SetVideoMode */
static int sdl_videoFlags;

SoundInterface_struct *SNDCoreList[] = {
  &SNDDummy,
  &SNDDummy,
  &SNDSDL,
  NULL
};

GPU3DInterface *core3DList[] = {
&gpu3DNull,
&gpu3DRasterize,
NULL
};

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


/* Our keyboard config is different because of the directional keys */
/* Please note that m is used for fake microphone */
const u16 cli_kb_cfg[NB_KEYS] =
  { 
    SDLK_x,         // A
    SDLK_z,         // B
    SDLK_RSHIFT,    // select
    SDLK_RETURN,    // start
    SDLK_RIGHT,     // Right
    SDLK_LEFT,      // Left
    SDLK_UP,        // Up
    SDLK_DOWN,      // Down
    SDLK_w,         // R
    SDLK_q,         // L
    SDLK_s,         // X
    SDLK_a,         // Y
    SDLK_p,         // DEBUG
    SDLK_o          // BOOST
  };

class configured_features : public CommandLine
{
public:
  int auto_pause;
  int frameskip;

  int engine_3d;
  int savetype;
  
#ifdef INCLUDE_OPENGL_2D
  int opengl_2d;
  int soft_colour_convert;
#endif

  int firmware_language;
};

static void
init_config( class configured_features *config) {

  config->auto_pause = 0;
  config->frameskip = 0;

  config->engine_3d = 1;
  config->savetype = 0;

#ifdef INCLUDE_OPENGL_2D
  config->opengl_2d = 0;
  config->soft_colour_convert = 0;
#endif

  /* use the default language */
  config->firmware_language = -1;
}


static int
fill_config( class configured_features *config,
             int argc, char ** argv) {
  GOptionEntry options[] = {
    { "auto-pause", 0, 0, G_OPTION_ARG_NONE, &config->auto_pause, "Pause emulation if focus is lost", NULL},
    { "frameskip", 0, 0, G_OPTION_ARG_INT, &config->frameskip, "Set frameskip", "FRAMESKIP"},
    { "3d-engine", 0, 0, G_OPTION_ARG_INT, &config->engine_3d, "Select 3d rendering engine. Available engines:\n"
        "\t\t\t\t\t\t  0 = 3d disabled\n"
        "\t\t\t\t\t\t  1 = internal rasterizer (default)\n"
        ,"ENGINE"},
    { "save-type", 0, 0, G_OPTION_ARG_INT, &config->savetype, "Select savetype from the following:\n"
    "\t\t\t\t\t\t  0 = Autodetect (default)\n"
    "\t\t\t\t\t\t  1 = EEPROM 4kbit\n"
    "\t\t\t\t\t\t  2 = EEPROM 64kbit\n"
    "\t\t\t\t\t\t  3 = EEPROM 512kbit\n"
    "\t\t\t\t\t\t  4 = FRAM 256kbit\n"
    "\t\t\t\t\t\t  5 = FLASH 2mbit\n"
    "\t\t\t\t\t\t  6 = FLASH 4mbit\n",
    "SAVETYPE"},
#ifdef INCLUDE_OPENGL_2D
    { "opengl-2d", 0, 0, G_OPTION_ARG_NONE, &config->opengl_2d, "Enables using OpenGL for screen rendering", NULL},
    { "soft-convert", 0, 0, G_OPTION_ARG_NONE, &config->soft_colour_convert, "Use software colour conversion during OpenGL screen rendering. May produce better or worse frame rates depending on hardware.",
     NULL},
#endif
    { "fwlang", 0, 0, G_OPTION_ARG_INT, &config->firmware_language, "Set the language in the firmware, LANG as follows:\n"
    "\t\t\t\t\t\t  0 = Japanese\n"
    "\t\t\t\t\t\t  1 = English\n"
    "\t\t\t\t\t\t  2 = French\n"
    "\t\t\t\t\t\t  3 = German\n"
    "\t\t\t\t\t\t  4 = Italian\n"
    "\t\t\t\t\t\t  5 = Spanish\n",
    "LANG"},
    { NULL }
  };

  config->loadCommonOptions();
  g_option_context_add_main_entries (config->ctx, options, "options");
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

  if (config->engine_3d != 0 && config->engine_3d != 1) {
    g_printerr("Currently available engines: 0, 1.\n");
    goto error;
  }

  if (config->frameskip < 0) {
    g_printerr("Frameskip must be >= 0.\n");
    goto error;
  }

  if (config->nds_file == "") {
    g_printerr("Need to specify file to load.\n");
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
                  void *thread_data) {
  SDL_Thread *new_thread = SDL_CreateThread( (int (*)(void *data))thread_function,
                                             thread_data);

  return new_thread;
}

void
joinThread_gdb( void *thread_handle) {
  int ignore;
  SDL_WaitThread( (SDL_Thread*)thread_handle, &ignore);
}
#endif

#ifdef INCLUDE_OPENGL_2D
/* initialization openGL function */
static int
initGL( GLuint *screen_texture) {
  GLenum errCode;
  u16 blank_texture[256 * 512];

  memset(blank_texture, 0x001f, sizeof(blank_texture));

  /* Enable Texture Mapping */
  glEnable( GL_TEXTURE_2D );

  /* Set the background black */
  glClearColor( 0.0f, 0.0f, 0.0f, 0.5f );

  /* Depth buffer setup */
  glClearDepth( 1.0f );

  /* Enables Depth Testing */
  glEnable( GL_DEPTH_TEST );

  /* The Type Of Depth Test To Do */
  glDepthFunc( GL_LEQUAL );

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

    return 0;
  }

  return 1;
}

static void
resizeWindow( u16 width, u16 height, GLuint *screen_texture) {

  int comp_width = 3 * width;
  int comp_height = 2 * height;
  GLenum errCode;

  surface = SDL_SetVideoMode(width, height, 32, sdl_videoFlags);
  initGL(screen_texture);

#ifdef HAVE_LIBAGG
  Hud.reset();
#endif

  if ( comp_width > comp_height) {
    width = 2*height/3;
  }
  height = 3*width/2;
  nds_screen_size_ratio = 256.0 / (double)width;

  /* Setup our viewport. */
  glViewport( 0, 0, ( GLint )width, ( GLint )height );

  /*
   * change to the projection matrix and set
   * our viewing volume.
   */
  glMatrixMode( GL_PROJECTION );
  glLoadIdentity( );

  gluOrtho2D( 0.0, 256.0, 384.0, 0.0);

  /* Make sure we're chaning the model view and not the projection */
  glMatrixMode( GL_MODELVIEW );

  /* Reset The View */
  glLoadIdentity( );

  if ((errCode = glGetError()) != GL_NO_ERROR) {
    const GLubyte *errString;

    errString = gluErrorString(errCode);
    fprintf( stderr, "GL resize failed: %s\n", errString);
  }
}


static void
opengl_Draw( GLuint *texture, int software_convert) {
  GLenum errCode;

  /* Clear The Screen And The Depth Buffer */
  glClear( GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT );

  /* Move Into The Screen 5 Units */
  glLoadIdentity( );

  /* Select screen Texture */
  glBindTexture( GL_TEXTURE_2D, texture[0]);
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
  }
  else {
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


  /* Draw the screen as a textured quad */
  glBegin( GL_QUADS);
  glTexCoord2f( 0.0f, 0.0f ); glVertex3f( 0.0f,  0.0f, 0.0f );
  glTexCoord2f( 1.0f, 0.0f ); glVertex3f( 256.0f,  0.0f,  0.0f );
  glTexCoord2f( 1.0f, 0.75f ); glVertex3f( 256.0f,  384.0f,  0.0f );
  glTexCoord2f( 0.0f, 0.75f ); glVertex3f(  0.0f,  384.0f, 0.0f );
  glEnd( );

  if ((errCode = glGetError()) != GL_NO_ERROR) {
    const GLubyte *errString;

    errString = gluErrorString(errCode);
    fprintf( stderr, "GL draw failed: %s\n", errString);
  }

  /* Draw it to the screen */
  SDL_GL_SwapBuffers( );
}
#endif

/* this is a stub for resizeWindow_stub in the case of no gl headers or no opengl 2d */
#ifdef INCLUDE_OPENGL_2D
static void
resizeWindow_stub (u16 width, u16 height, GLuint *screen_texture) {
}
#else
static void
resizeWindow_stub (u16 width, u16 height, void *screen_texture) {
}
#endif

static void
Draw( void) {
  SDL_Surface *rawImage;

  rawImage = SDL_CreateRGBSurfaceFrom((void*)&GPU_screen, 256, 384, 16, 512, 0x001F, 0x03E0, 0x7C00, 0);
  if(rawImage == NULL) return;

  SDL_BlitSurface(rawImage, 0, surface, 0);

  SDL_UpdateRect(surface, 0, 0, 0, 0);

  SDL_FreeSurface(rawImage);
  return;
}

static void desmume_cycle(struct ctrls_event_config * cfg)
{
    SDL_Event event;

    cfg->nds_screen_size_ratio = nds_screen_size_ratio;

    /* Look for queued events and update keypad status */
    /* IMPORTANT: Reenable joystick events iif needed. */
    if(SDL_JoystickEventState(SDL_QUERY) == SDL_IGNORE)
      SDL_JoystickEventState(SDL_ENABLE);

    /* There's an event waiting to be processed? */
    while ( !cfg->sdl_quit &&
        (SDL_PollEvent(&event) || (!cfg->focused && SDL_WaitEvent(&event))))
      {
        process_ctrls_event( event, cfg);
    }

    /* Update mouse position and click */
    if(mouse.down) NDS_setTouchPos(mouse.x, mouse.y);
    if(mouse.click)
      { 
        NDS_releaseTouch();
        mouse.click = FALSE;
      }

    update_keypad(cfg->keypad);     /* Update keypad */
    NDS_exec<false>();
    SPU_Emulate_user();
}

int main(int argc, char ** argv) {
  class configured_features my_config;
  struct ctrls_event_config ctrls_cfg;
#ifdef GDB_STUB
  gdbstub_handle_t arm9_gdb_stub;
  gdbstub_handle_t arm7_gdb_stub;
  struct armcpu_memory_iface *arm9_memio = &arm9_base_memory_iface;
  struct armcpu_memory_iface *arm7_memio = &arm7_base_memory_iface;
  struct armcpu_ctrl_iface *arm9_ctrl_iface;
  struct armcpu_ctrl_iface *arm7_ctrl_iface;
#endif

  int limiter_frame_counter = 0;
  int limiter_tick0 = 0;
  int error;

  GKeyFile *keyfile;

  int now;

#ifdef DISPLAY_FPS
  u32 fps_timing = 0;
  u32 fps_frame_counter = 0;
  u32 fps_previous_time = 0;
#endif

#ifdef INCLUDE_OPENGL_2D
  GLuint screen_texture[1];
#endif
  /* this holds some info about our display */
  const SDL_VideoInfo *videoInfo;

  /* the firmware settings */
  struct NDS_fw_config_data fw_config;

#ifdef GDB_STUB
  NDS_Init( arm9_memio, &arm9_ctrl_iface,
            arm7_memio, &arm7_ctrl_iface);
#else
        NDS_Init();
#endif

  /* default the firmware settings, they may get changed later */
  NDS_FillDefaultFirmwareConfigData( &fw_config);

  init_config( &my_config);

  if ( !fill_config( &my_config, argc, argv)) {
    exit(1);
  }

  /* use any language set on the command line */
  if ( my_config.firmware_language != -1) {
    fw_config.language = my_config.firmware_language;
  }

    my_config.process_addonCommands();

    int slot2_device_type = NDS_SLOT2_AUTO;

    if (my_config.is_cflash_configured)
        slot2_device_type = NDS_SLOT2_CFLASH;

    if(my_config.gbaslot_rom != "") {
        strncpy(GBAgameName, my_config.gbaslot_rom.c_str(), MAX_PATH);
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

  if ( !g_thread_supported()) {
    g_thread_init( NULL);
  }

  driver = new UnixDriver();

#ifdef GDB_STUB
  if ( my_config.arm9_gdb_port != 0) {
    arm9_gdb_stub = createStub_gdb( my_config.arm9_gdb_port,
                                    &arm9_memio,
                                    &arm9_direct_memory_iface);

    if ( arm9_gdb_stub == NULL) {
      fprintf( stderr, "Failed to create ARM9 gdbstub on port %d\n",
               my_config.arm9_gdb_port);
      exit( 1);
    }
  }
  if ( my_config.arm7_gdb_port != 0) {
    arm7_gdb_stub = createStub_gdb( my_config.arm7_gdb_port,
                                    &arm7_memio,
                                    &arm7_base_memory_iface);

    if ( arm7_gdb_stub == NULL) {
      fprintf( stderr, "Failed to create ARM7 gdbstub on port %d\n",
               my_config.arm7_gdb_port);
      exit( 1);
    }
  }
#endif

  /* Create the dummy firmware */
  NDS_CreateDummyFirmware( &fw_config);

  if ( !my_config.disable_sound) {
    SPU_ChangeSoundCore(SNDCORE_SDL, 735 * 4);
  }

  NDS_3D_ChangeCore(my_config.engine_3d);

  backup_setManualBackupType(my_config.savetype);

  error = NDS_LoadROM( my_config.nds_file.c_str() );
  if (error < 0) {
    fprintf(stderr, "error while loading %s\n", my_config.nds_file.c_str());
    exit(-1);
  }

  /*
   * Activate the GDB stubs
   * This has to come after the NDS_Init where the cpus are set up.
   */
#ifdef GDB_STUB
  if ( my_config.arm9_gdb_port != 0) {
    activateStub_gdb( arm9_gdb_stub, arm9_ctrl_iface);
  }
  if ( my_config.arm7_gdb_port != 0) {
    activateStub_gdb( arm7_gdb_stub, arm7_ctrl_iface);
  }
#endif

  execute = true;

  if(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) == -1)
    {
      fprintf(stderr, "Error trying to initialize SDL: %s\n",
              SDL_GetError());
      return 1;
    }
  SDL_WM_SetCaption("Desmume SDL", NULL);

  /* Fetch the video info */
  videoInfo = SDL_GetVideoInfo( );
  if ( !videoInfo ) {
    fprintf( stderr, "Video query failed: %s\n", SDL_GetError( ) );
    exit( -1);
  }

  /* This checks if hardware blits can be done */
  if ( videoInfo->blit_hw )
    sdl_videoFlags |= SDL_HWACCEL;

#ifdef INCLUDE_OPENGL_2D
  if ( my_config.opengl_2d) {
    /* the flags to pass to SDL_SetVideoMode */
    sdl_videoFlags  = SDL_OPENGL;          /* Enable OpenGL in SDL */
    sdl_videoFlags |= SDL_HWPALETTE;       /* Store the palette in hardware */
    sdl_videoFlags |= SDL_RESIZABLE;       /* Enable window resizing */


    /* This checks to see if surfaces can be stored in memory */
    if ( videoInfo->hw_available )
      sdl_videoFlags |= SDL_HWSURFACE;
    else
      sdl_videoFlags |= SDL_SWSURFACE;


    /* Sets up OpenGL double buffering */
    SDL_GL_SetAttribute( SDL_GL_DOUBLEBUFFER, 1 );

    surface = SDL_SetVideoMode( 256, 192 * 2, 32,
                                sdl_videoFlags );

    /* Verify there is a surface */
    if ( !surface ) {
      fprintf( stderr, "Video mode set failed: %s\n", SDL_GetError( ) );
      exit( -1);
    }


    /* initialize OpenGL */
    if ( !initGL( screen_texture)) {
      fprintf( stderr, "Failed to init GL, fall back to software render\n");

      my_config.opengl_2d = 0;
    }
  }

  if ( !my_config.opengl_2d) {
#endif
    sdl_videoFlags |= SDL_SWSURFACE;
    surface = SDL_SetVideoMode(256, 384, 32, sdl_videoFlags);

    if ( !surface ) {
      fprintf( stderr, "Video mode set failed: %s\n", SDL_GetError( ) );
      exit( -1);
    }
#ifdef INCLUDE_OPENGL_2D
  }

  /* set the initial window size */
  if ( my_config.opengl_2d) {
    resizeWindow( 256, 192*2, screen_texture);
  }
#endif

  /* Initialize joysticks */
  if(!init_joy()) return 1;
  /* Load keyboard and joystick configuration */
  keyfile = desmume_config_read_file(cli_kb_cfg);
  desmume_config_dispose(keyfile);
  /* Since gtk has a different mapping the keys stop to work with the saved configuration :| */
  load_default_config(cli_kb_cfg);

  if(my_config.load_slot != -1){
    loadstate_slot(my_config.load_slot);
  }

#ifdef HAVE_LIBAGG
  Desmume_InitOnce();
  Hud.reset();
  aggDraw.hud->attach(GPU_screen, 256, 384, 512);
#endif

  ctrls_cfg.boost = 0;
  ctrls_cfg.sdl_quit = 0;
  ctrls_cfg.auto_pause = my_config.auto_pause;
  ctrls_cfg.focused = 1;
  ctrls_cfg.fake_mic = 0;
  ctrls_cfg.keypad = 0;
#ifdef INCLUDE_OPENGL_2D
  ctrls_cfg.screen_texture = screen_texture;
#else
  ctrls_cfg.screen_texture = NULL;
#endif
  ctrls_cfg.resize_cb = &resizeWindow_stub;

  while(!ctrls_cfg.sdl_quit) {
    desmume_cycle(&ctrls_cfg);

    osd->update();
    DrawHUD();
#ifdef INCLUDE_OPENGL_2D
    if ( my_config.opengl_2d) {
      opengl_Draw( screen_texture, my_config.soft_colour_convert);
      ctrls_cfg.resize_cb = &resizeWindow;
    }
    else
#endif
      Draw();
    osd->clear();

    for ( int i = 0; i < my_config.frameskip; i++ ) {
        NDS_SkipNextFrame();
        desmume_cycle(&ctrls_cfg);
    }

#ifdef DISPLAY_FPS
    now = SDL_GetTicks();
#endif
    if ( !my_config.disable_limiter && !ctrls_cfg.boost) {
#ifndef DISPLAY_FPS
      now = SDL_GetTicks();
#endif
      int delay =  (limiter_tick0 + limiter_frame_counter*1000/FPS_LIMITER_FPS) - now;
      if (delay < -500 || delay > 100) { // reset if we fall too far behind don't want to run super fast until we catch up
        limiter_tick0 = now;
        limiter_frame_counter = 0;
      } else if (delay > 0) {
        SDL_Delay(delay);
      }
    }
    // always count frames, we'll mess up if the limiter gets turned on later otherwise
    limiter_frame_counter += 1 + my_config.frameskip;

#ifdef DISPLAY_FPS
    fps_frame_counter += 1;
    fps_timing += now - fps_previous_time;
    fps_previous_time = now;

    if ( fps_frame_counter == NUM_FRAMES_TO_TIME) {
      char win_title[20];
      float fps = NUM_FRAMES_TO_TIME * 1000.f / fps_timing;

      fps_frame_counter = 0;
      fps_timing = 0;

      snprintf( win_title, sizeof(win_title), "Desmume %f", fps);

      SDL_WM_SetCaption( win_title, NULL);
    }
#endif
  }

  /* Unload joystick */
  uninit_joy();

  SDL_Quit();
  NDS_DeInit();

#ifdef GDB_STUB
  if ( my_config.arm9_gdb_port != 0) {
    destroyStub_gdb( arm9_gdb_stub);
  }
  if ( my_config.arm7_gdb_port != 0) {
    destroyStub_gdb( arm7_gdb_stub);
  }
#endif

  return 0;
}
