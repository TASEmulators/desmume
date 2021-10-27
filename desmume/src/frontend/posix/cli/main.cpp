/* main.c - this file is part of DeSmuME
 *
 * Copyright (C) 2006-2021 DeSmuME Team
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
#include <X11/Xlib.h>
#include <SDL.h>
#include <SDL_thread.h>
#include <stdlib.h>
#include <string.h>
#include <glib.h>

#ifndef VERSION
#define VERSION "Unknown version"
#endif

#ifndef CLI_UI
#define CLI_UI
#endif

#include "../NDSSystem.h"
#include "../driver.h"
#include "../GPU.h"
#include "../SPU.h"
#include "../shared/sndsdl.h"
#include "../shared/ctrlssdl.h"
#include "../render3D.h"
#include "../rasterize.h"
#include "../saves.h"
#include "../frontend/modules/osd/agg/agg_osd.h"
#include "../shared/desmume_config.h"
#include "../commandline.h"
#include "../slot2.h"
#include "../utils/xstring.h"

#ifdef GDB_STUB
#include "../armcpu.h"
#include "../gdbstub.h"
class CliDriver : public BaseDriver
{
private:
	gdbstub_handle_t __stubs[2];
public:
	virtual void EMU_DebugIdleEnter() {
		SPU_Pause(1);
	}
	virtual void EMU_DebugIdleUpdate() {
		gdbstub_wait(__stubs, -1L);
	}
	virtual void EMU_DebugIdleWakeUp() {
		SPU_Pause(0);
	}
	virtual void setStubs(gdbstub_handle_t stubs[2]) {
		this->__stubs[0] = stubs[0];
		this->__stubs[1] = stubs[1];
	}
};
#else
class CliDriver : public BaseDriver {};
#endif

volatile bool execute = false;

static float nds_screen_size_ratio = 1.0f;

#define DISPLAY_FPS

#ifdef DISPLAY_FPS
#define NUM_FRAMES_TO_TIME 60
#endif

#define FPS_LIMITER_FPS 60

static SDL_Window * window;
static SDL_Renderer * renderer;
static SDL_Texture *screen[2];

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
const u32 cli_kb_cfg[NB_KEYS] =
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
    SDLK_o,         // BOOST
    SDLK_BACKSPACE, // Lid
  };

class configured_features : public CommandLine
{
public:
  int auto_pause;

  int engine_3d;
  int savetype;

  int firmware_language;
};

static void
init_config( class configured_features *config) {

  config->auto_pause = 0;

  config->engine_3d = 1;
  config->savetype = 0;

  /* use the default language */
  config->firmware_language = -1;
}


static int
fill_config( class configured_features *config,
             int argc, char ** argv) {
  GOptionEntry options[] = {
    { "auto-pause", 0, 0, G_OPTION_ARG_NONE, &config->auto_pause, "Pause emulation if focus is lost", NULL},
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

  //g_option_context_add_main_entries (config->ctx, options, "options");
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
createThread_gdb(void (*thread_function)(void *data),
                 void *thread_data) {
  SDL_Thread *new_thread = SDL_CreateThread((int (*)(void *data))thread_function,
                                            "gdb-stub",
                                            thread_data);

  return new_thread;
}

void
joinThread_gdb( void *thread_handle) {
  int ignore;
  SDL_WaitThread( (SDL_Thread*)thread_handle, &ignore);
}
#endif

static void
resizeWindow_stub (u16 width, u16 height, void *screen_texture) {
}

static void Draw(class configured_features *cfg) {
	const float scale = cfg->scale;
	const unsigned w = GPU_FRAMEBUFFER_NATIVE_WIDTH, h = GPU_FRAMEBUFFER_NATIVE_HEIGHT;
	const int ws = w * scale, hs = h * scale;
	const NDSDisplayInfo &displayInfo = GPU->GetDisplayInfo();
	const size_t pixCount = w * h;
	ColorspaceApplyIntensityToBuffer16<false, false>(displayInfo.nativeBuffer16[NDSDisplayID_Main],  pixCount, displayInfo.backlightIntensity[NDSDisplayID_Main]);
	ColorspaceApplyIntensityToBuffer16<false, false>(displayInfo.nativeBuffer16[NDSDisplayID_Touch], pixCount, displayInfo.backlightIntensity[NDSDisplayID_Touch]);
	const SDL_Rect destrect_v[2] = {
		{ 0, 0 , ws, hs},
		{ 0, hs, ws, hs},
	};
	const SDL_Rect destrect_h[2] = {
		{ 0, 0 , ws, hs},
		{ ws, 0, ws, hs},
	};
	unsigned i, off = 0, n = pixCount*2;
	for(i = 0; i < 2; ++i) {
		void *p = 0;
		int pitch;
		SDL_LockTexture(screen[i], NULL, &p, &pitch);
		memcpy(p, ((char*)displayInfo.masterNativeBuffer16)+off, n);
		SDL_UnlockTexture(screen[i]);
		SDL_RenderCopy(renderer, screen[i], NULL, cfg->horizontal ? &destrect_h[i] : &destrect_v[i]);
		off += n;
	}
	SDL_RenderPresent(renderer);
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
    if(mouse.down) {
	NDS_setTouchPos(mouse.x, mouse.y);
	mouse.down = 2;
    }
    if(mouse.click)
      { 
        NDS_releaseTouch();
        mouse.click = 0;
      }

    update_keypad(cfg->keypad);     /* Update keypad */
    NDS_exec<false>();
    SPU_Emulate_user();
}

#ifdef GDB_STUB
static gdbstub_handle_t setup_gdb_stub(u16 port, armcpu_t *cpu, const armcpu_memory_iface *memio, const char* desc) {
	gdbstub_handle_t stub = createStub_gdb(port, cpu, memio);
	if ( stub == NULL) {
		fprintf( stderr, "Failed to create %s gdbstub on port %d\n", desc, port);
		exit( 1);
	} else {
		activateStub_gdb(stub);
	}
	return stub;
}
#endif

int main(int argc, char ** argv) {
  class configured_features my_config;
  struct ctrls_event_config ctrls_cfg;

  int limiter_frame_counter = 0;
  int limiter_tick0 = 0;
  int error;
  unsigned i;

  GKeyFile *keyfile;

  int now;

#ifdef DISPLAY_FPS
  u32 fps_timing = 0;
  u32 fps_frame_counter = 0;
  u32 fps_previous_time = 0;
#endif

  NDS_Init();

  init_config( &my_config);

  if ( !fill_config( &my_config, argc, argv)) {
    exit(1);
  }

  /* use any language set on the command line */
  if ( my_config.firmware_language != -1) {
    CommonSettings.fwConfig.language = my_config.firmware_language;
  }

    my_config.process_addonCommands();

    int slot2_device_type = NDS_SLOT2_AUTO;

    if (my_config.is_cflash_configured)
        slot2_device_type = NDS_SLOT2_CFLASH;

    if(my_config.gbaslot_rom != "") {

        //set the GBA rom and sav paths
        GBACartridge_RomPath = my_config.gbaslot_rom.c_str();
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

  driver = new CliDriver();

#ifdef GDB_STUB
  gdbstub_mutex_init();

  /*
   * Activate the GDB stubs
   * This has to come after NDS_Init() where the CPUs are set up.
   */
  gdbstub_handle_t stubs[2] = {};
  if ( my_config.arm9_gdb_port > 0) {
    stubs[0] = setup_gdb_stub(my_config.arm9_gdb_port,
                                   &NDS_ARM9,
                                   &arm9_direct_memory_iface, "ARM9");

  }
  if ( my_config.arm7_gdb_port > 0) {
    stubs[1] = setup_gdb_stub(my_config.arm7_gdb_port,
                                   &NDS_ARM7,
                                   &arm7_base_memory_iface, "ARM7");

  }
  ((CliDriver*)driver)->setStubs(stubs);
  gdbstub_wait_set_enabled(stubs[0], 1);
  gdbstub_wait_set_enabled(stubs[1], 1);

  if(stubs[0] || stubs[1]) {
#ifdef HAVE_JIT
    if(CommonSettings.use_jit) {
      fprintf(stderr, "GDB stub enabled, turning off jit (they're incompatible)\n");
      arm_jit_sync();
      arm_jit_reset(CommonSettings.use_jit=0);
    }
#endif
  }
#endif

  if ( !my_config.disable_sound) {
    SPU_ChangeSoundCore(SNDCORE_SDL, 735 * 4);
  }

  if (!GPU->Change3DRendererByID(my_config.engine_3d)) {
    GPU->Change3DRendererByID(RENDERID_SOFTRASTERIZER);
    fprintf(stderr, "3D renderer initialization failed!\nFalling back to 3D core: %s\n", core3DList[RENDERID_SOFTRASTERIZER]->name);
  }

  backup_setManualBackupType(my_config.savetype);

  error = NDS_LoadROM( my_config.nds_file.c_str() );
  if (error < 0) {
    fprintf(stderr, "error while loading %s\n", my_config.nds_file.c_str());
    exit(-1);
  }

  execute = true;

  /* X11 multi-threading support */
  if(!XInitThreads())
    {
      fprintf(stderr, "Warning: X11 not thread-safe\n");
    }

  if(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) == -1)
    {
      fprintf(stderr, "Error trying to initialize SDL: %s\n",
              SDL_GetError());
      return 1;
    }

    nds_screen_size_ratio = my_config.scale;
    ctrls_cfg.horizontal = my_config.horizontal;
    unsigned width = 256 + my_config.horizontal*256;
    unsigned height = 192 + 192 * !my_config.horizontal;
    sdl_videoFlags |= SDL_WINDOW_OPENGL;
    window = SDL_CreateWindow( "Desmume SDL", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
	width*my_config.scale, height*my_config.scale, sdl_videoFlags );

    if ( !window ) {
      fprintf( stderr, "Window creation failed: %s\n", SDL_GetError( ) );
      exit( -1);
    }
    ctrls_cfg.window = window;

    renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);

    uint32_t desmume_pixelformat = SDL_MasksToPixelFormatEnum(16, 0x001F, 0x03E0, 0x7C00, 0);

    for(i = 0; i < 2; ++i)
      screen[i] = SDL_CreateTexture(renderer, desmume_pixelformat, SDL_TEXTUREACCESS_STREAMING,
	GPU_FRAMEBUFFER_NATIVE_WIDTH, GPU_FRAMEBUFFER_NATIVE_HEIGHT);


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
  // Now that gtk port draws to RGBA buffer directly, the other one
  // has to use ugly ways to make HUD rendering work again.
  // desmume gtk: Sorry desmume-cli :(
  T_AGG_RGB555 agg_targetScreen_cli((u8 *)GPU->GetDisplayInfo().masterNativeBuffer16, 256, 384, 512);
  aggDraw.hud = &agg_targetScreen_cli;
  aggDraw.hud->setFont("verdana18_bold");
  
  osd = new OSDCLASS(-1);
#endif

  ctrls_cfg.boost = 0;
  ctrls_cfg.sdl_quit = 0;
  ctrls_cfg.auto_pause = my_config.auto_pause;
  ctrls_cfg.focused = 1;
  ctrls_cfg.fake_mic = 0;
  ctrls_cfg.keypad = 0;
  ctrls_cfg.screen_texture = NULL;
  ctrls_cfg.resize_cb = &resizeWindow_stub;

  while(!ctrls_cfg.sdl_quit) {
    desmume_cycle(&ctrls_cfg);

#ifdef HAVE_LIBAGG
    osd->update();
    DrawHUD();
#endif

    Draw(&my_config);

#ifdef HAVE_LIBAGG
    osd->clear();
#endif

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

      snprintf( win_title, sizeof(win_title), "Desmume %.02f", fps);

      SDL_SetWindowTitle( window, win_title );
    }
#endif
  }

  /* Unload joystick */
  uninit_joy();

#ifdef GDB_STUB
  destroyStub_gdb( stubs[0]);
  destroyStub_gdb( stubs[1]);

  gdbstub_mutex_destroy();
#endif

  SDL_Quit();
  NDS_DeInit();


  return 0;
}
