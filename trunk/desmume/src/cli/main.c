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

volatile BOOL execute = FALSE;

SDL_Surface * surface;

SoundInterface_struct *SNDCoreList[] = {
  &SNDDummy,
  &SNDFile,
  &SNDSDL,
  NULL
};

/* Our keyboard config is different because of the directional keys */
const u16 cli_kb_cfg[NB_KEYS] =
  { SDLK_c,         // A
    SDLK_x,         // B
    SDLK_BACKSPACE, // select
    SDLK_RETURN,    // start
    SDLK_RIGHT,     // Right
    SDLK_LEFT,      // Left
    SDLK_UP,        // Up
    SDLK_DOWN,      // Down
    SDLK_e,         // R
    SDLK_w,         // L
    SDLK_d,         // X
    SDLK_s,         // Y
    SDLK_p,         // DEBUG
    SDLK_o          // BOOST
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
  if(!init_joy()) return 1;
  /* Load our own keyboard configuration */
  set_kb_keys(cli_kb_cfg);

  surface = SDL_SetVideoMode(256, 384, 32, SDL_SWSURFACE);

  while(!sdl_quit) {
    /* Look for queued events and update keypad status */
    keypad = process_ctrls_events(keypad);
    /* Update mouse position and click */
    if(mouse.down) NDS_setTouchPos(mouse.x, mouse.y);
    if(mouse.click)
      { 
        NDS_releasTouch();
        mouse.click = FALSE;
      }

    update_keypad(keypad);     /* Update keypad */
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
