#include "SDL.h"
#include <stdlib.h>
#include <string.h>
#include <libgen.h>

#ifndef CLI_UI
#define CLI_UI
#endif

#include "../MMU.h"
#include "../NDSSystem.h"
#include "../cflash.h"
#include "../debug.h"
#include "../sndsdl.h"
#include "../ctrlssdl.h"

#define DESMUME_NB_KEYS		13
#define DESMUME_KEYMASK_(k)	(1 << k)

volatile BOOL execute = FALSE;

SDL_Surface * surface;

SoundInterface_struct *SNDCoreList[] = {
  &SNDDummy,
  &SNDFile,
  &SNDSDL,
  NULL
};

const u16 Default_Joypad_Config[DESMUME_NB_KEYS] =
  { 1,  // A
    0,  // B
    5,  // select
    8,  // start
    20, // Right -- Start cheating abit...
    21, // Left
    22, // Up
    23, // Down  -- End of cheating.
    7,  // R
    6,  // L
    3,  // Y
    4,  // X
    -1  // DEBUG
  };

int Draw() {
  SDL_Surface *rawImage;
	
  rawImage = SDL_CreateRGBSurfaceFrom((void*)&GPU_screen, 256, 384, 16, 512, 0x001F, 0x03E0, 0x7C00, 0);
  if(rawImage == NULL) return 1;
	
  SDL_BlitSurface(rawImage, 0, surface, 0);
  SDL_UpdateRect(surface, 0, 0, 0, 0);
  
  SDL_FreeSurface(rawImage);
  return 1;
}

int main(int argc, char ** argv) {
  static unsigned short keypad = 0;
  u32 last_cycle;

#ifdef DEBUG
  LogStart();
#endif
  NDS_Init();
  SPU_ChangeSoundCore(SNDCORE_SDL, 735 * 4);

  if (argc < 2) {
    fprintf(stderr, "usage: %s filename\n", argv[0]);
    return 1;
  }

  if (NDS_LoadROM(argv[1], MC_TYPE_AUTODETECT, 1) < 0) {
    fprintf(stderr, "error while loading %s\n", argv[1]);
    return 2;
  }

  /*      // This has to get fixed yet
          strcpy(szRomPath, dirname(argv[1]));
          cflash_close();
          cflash_init();
  */
  
  execute = TRUE;

  if(SDL_Init(SDL_INIT_VIDEO) == -1)
    {
      fprintf(stderr, "Error trying to initialize SDL: %s\n",
              SDL_GetError());
      return 1;
    }
  SDL_WM_SetCaption("Desmume SDL", NULL);

  /* Initialize joysticks */
  if(!init_joy(Default_Joypad_Config)) return 1;

  surface = SDL_SetVideoMode(256, 384, 32, SDL_SWSURFACE);

  while(!sdl_quit) {
    /* Look for queued events and update keypad status */
    keypad = process_ctrls_events(keypad);
    /* Update mouse position and click */
    if(mouse_down) NDS_setTouchPos(mouse_pos.x, mouse_pos.y);
    if(mouse_click)
      { 
        NDS_releasTouch();
        mouse_click = FALSE;
      }

    /* Update keypad */
    ((unsigned short *)ARM9Mem.ARM9_REG)[0x130>>1] = ~(keypad & 0x3FF);
    ((unsigned short *)MMU.ARM7_REG)[0x130>>1] = ~((keypad >> 10) & 0x3);

/*     NDS_exec(1120380, FALSE); */
    last_cycle = NDS_exec((560190 << 1) - last_cycle, FALSE);
    SPU_Emulate();
    Draw();
  }

  /* Unload joystick */
  uninit_joy();

  SDL_Quit();
  NDS_DeInit();
#ifdef DEBUG
  LogStop();
#endif

  return 0;
}
