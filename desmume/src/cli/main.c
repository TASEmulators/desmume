#include "SDL.h"
#include <stdlib.h>
#include <string.h>
#include <libgen.h>

#include "../MMU.h"
#include "../NDSSystem.h"
#include "../cflash.h"
#include "../debug.h"
#include "../sndsdl.h"

BOOL execute = FALSE;

SDL_Surface * surface;

SoundInterface_struct *SNDCoreList[] = {
&SNDDummy,
&SNDFile,
&SNDSDL,
NULL
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
	BOOL click;
	unsigned short keypad;
	SDL_Event event;
	int end = 0;

#ifdef DEBUG
	LogStart();
#endif
	NDS_Init();

	if (argc < 2) {
		fprintf(stderr, "usage: %s filename\n", argv[0]);
		return 1;
	}

        if (NDS_LoadROM(argv[1]) < 0) {
		fprintf(stderr, "error while loading %s\n", argv[1]);
		return 2;
	}

/*      // This has to get fixed yet
	strcpy(szRomPath, dirname(argv[1]));
	cflash_close();
	cflash_init();
*/

	execute = TRUE;

	SDL_Init(SDL_INIT_VIDEO);
	SDL_WM_SetCaption("Desmume SDL", NULL);

	surface = SDL_SetVideoMode(256, 384, 32, SDL_SWSURFACE);

	while(!end) {
		
		

			
		keypad=0x0000;
			
		Uint8 *keys;
		keys = SDL_GetKeyState(NULL);
		if(keys[SDLK_UP])	keypad |= 0x40;
		if(keys[SDLK_DOWN])	keypad |= 0x80;
		if(keys[SDLK_RIGHT])	keypad |= 0x10;
		if(keys[SDLK_LEFT])	keypad |= 0x20;
		if(keys[SDLK_SPACE])	keypad |= 0x1;
		if(keys['b'])	keypad |= 0x2;
		if(keys[SDLK_BACKSPACE])	keypad |= 0x4;
		if(keys[SDLK_RETURN])	keypad |= 0x8;
		if(keys['0'])	keypad |= 0x200;
		if(keys['.'])	keypad |= 0x100;
			
		((unsigned short *)ARM9Mem.ARM9_REG)[0x130>>1] = ~keypad;
		((unsigned short *)MMU.ARM7_REG)[0x130>>1] = ~keypad;
			
		
		if (SDL_PollEvent(&event)) {
			switch(event.type) {
				case SDL_QUIT:
					end = 1;
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
						
			}
		}
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
