#include "SDL.h"
#include <stdlib.h>
#include <string.h>
#include <libgen.h>

#include "../MMU.h"
#include "../NDSSystem.h"
#include "../cflash.h"
#include "../debug.h"
#include "../sndsdl.h"

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

const u16 Joypad_Config[DESMUME_NB_KEYS] =
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

u16 inline lookup_joykey (u16 keyval) {
  int i;
  u16 Key = 0;
  for(i = 0; i < DESMUME_NB_KEYS; i++)
    if(keyval == Joypad_Config[i]) break;
  if(i < DESMUME_NB_KEYS)	
    Key = DESMUME_KEYMASK_(i);
  printf("Lookup key %d from joypad...%x\n", keyval, Key);
  return Key;
}

int main(int argc, char ** argv) {
  BOOL click;
  static unsigned short keypad;
  SDL_Event event;
  int end = 0;
  int nbJoysticks;
  int i;

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

  if(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_JOYSTICK) == -1)
    {
      fprintf(stderr, "Error trying to initialize SDL: %s\n",
              SDL_GetError());
      return 1;
    }
  SDL_WM_SetCaption("Desmume SDL", NULL);

  surface = SDL_SetVideoMode(256, 384, 32, SDL_SWSURFACE);

  /* Initialize joysticks */
  nbJoysticks = SDL_NumJoysticks();
  printf("Nbr of joysticks: %d\n\n", nbJoysticks);

  for (i = 0; i < nbJoysticks; i++)
    {
      SDL_Joystick * joy = SDL_JoystickOpen(i);
      printf("Joystick %s\n", i, SDL_JoystickName(i));
      printf("Axes: %d\n", SDL_JoystickNumAxes(joy));
      printf("Buttons: %d\n", SDL_JoystickNumButtons(joy));
      printf("Trackballs: %d\n", SDL_JoystickNumBalls(joy));
      printf("Hats: %d\n\n", SDL_JoystickNumHats(joy));
    }

  keypad=0x0000;
  while(!end) {
    if (SDL_PollEvent(&event)) {
      switch(event.type) {
      case SDL_QUIT:
        end = 1;
        break;

      case SDL_KEYDOWN:
        switch (event.key.keysym.sym)
          {
          case SDLK_UP:	        keypad |= 0x40; break;
          case SDLK_DOWN:	keypad |= 0x80; break;
          case SDLK_RIGHT:	keypad |= 0x10; break;
          case SDLK_LEFT:	keypad |= 0x20; break;
          case SDLK_SPACE:	keypad |= 0x1; break;
          case 'b':	        keypad |= 0x2; break;
          case SDLK_BACKSPACE:	keypad |= 0x4; break;
          case SDLK_RETURN:	keypad |= 0x8; break;
          case '0':	        keypad |= 0x200; break;
          case '.':	        keypad |= 0x100; break;
          }
        break;

      case SDL_KEYUP:
        switch (event.key.keysym.sym)
          {
          case SDLK_UP:	        keypad &= ~0x40; break;
          case SDLK_DOWN:	keypad &= ~0x80; break;
          case SDLK_RIGHT:	keypad &= ~0x10; break;
          case SDLK_LEFT:	keypad &= ~0x20; break;
          case SDLK_SPACE:	keypad &= ~0x1; break;
          case 'b':	        keypad &= ~0x2; break;
          case SDLK_BACKSPACE:	keypad &= ~0x4; break;
          case SDLK_RETURN:	keypad &= ~0x8; break;
          case '0':	        keypad &= ~0x200; break;
          case '.':	        keypad &= ~0x100; break;
          }
        break;

      case SDL_MOUSEBUTTONDOWN: // Un bouton fut appuyé
        if(event.button.button==1)	click=TRUE;
						
      case SDL_MOUSEMOTION: // La souris a été déplacée sur l?écran
							
        if(!click) break;
							
        if(event.button.y>=192)
          {
            signed long x = event.button.x;
            signed long y = event.button.y - 192;
            if(x<0) x = 0; else if(x>255) x = 255;
            if(y<0) y = 0; else if(y>192) y = 192;
            NDS_setTouchPos(x, y);

          }
						
        break;

      case SDL_MOUSEBUTTONUP: // Le bouton de la souris a été relaché
        if(click)NDS_releasTouch();
        click=FALSE;
        break;

        /* Joystick axis motion */
      case SDL_JOYAXISMOTION:
        /* Horizontal */
        if (event.jaxis.axis == 0)
          if( event.jaxis.value == 0 )
            keypad &= ~(lookup_joykey( 20 ) | lookup_joykey( 21 ));
          else
            {
              if( event.jaxis.value > 0 ) keypad |= lookup_joykey( 20 );
              else keypad |= lookup_joykey( 21 );
            }
        /* Vertical */
        else if (event.jaxis.axis == 1)
          if( event.jaxis.value == 0 )
            keypad &= ~(lookup_joykey( 22 ) | lookup_joykey( 23 ));
          else
            {
              if( event.jaxis.value > 0 ) keypad |= lookup_joykey( 23 );
              else keypad |= lookup_joykey( 22 );
            }
        break;
        /* Joystick button pressed */
      case SDL_JOYBUTTONDOWN:
        keypad |= lookup_joykey(event.jbutton.button);
        break;

        /* Joystick button released */
      case SDL_JOYBUTTONUP:
        keypad &= ~lookup_joykey(event.jbutton.button);
        break;

      default:
        break;
      }
    }
    /* Update keypad */
    ((unsigned short *)ARM9Mem.ARM9_REG)[0x130>>1] = ~keypad;
    ((unsigned short *)MMU.ARM7_REG)[0x130>>1] = ~keypad;

    NDS_exec(1120380, FALSE);
    SPU_Emulate();
    Draw();
  }

  SDL_Quit();
  NDS_DeInit();
#ifdef DEBUG
  LogStop();
#endif

  return 0;
}
