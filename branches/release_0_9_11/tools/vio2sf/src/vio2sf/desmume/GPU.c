/*  Copyright (C) 2006 yopyop
    yopyop156@ifrance.com
    yopyop156.ifrance.com

    Copyright (C) 2006-2007 Theo Berkau
    Copyright (C) 2007 shash

    This file is part of DeSmuME

    DeSmuME is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    DeSmuME is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with DeSmuME; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

// CONTENTS
//	INITIALIZATION
//	ENABLING / DISABLING LAYERS
//	PARAMETERS OF BACKGROUNDS
//	PARAMETERS OF ROTOSCALE
//	PARAMETERS OF EFFECTS
//	PARAMETERS OF WINDOWS
//	ROUTINES FOR INSIDE / OUTSIDE WINDOW CHECKS
//	PIXEL RENDERING
//	BACKGROUND RENDERING -TEXT-
//	BACKGROUND RENDERING -ROTOSCALE-
//	BACKGROUND RENDERING -HELPER FUNCTIONS-
//	SPRITE RENDERING -HELPER FUNCTIONS-
//	SPRITE RENDERING
//	SCREEN FUNCTIONS
//	GRAPHICS CORE
//	GPU_ligne

#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include "MMU.h"
#include "GPU.h"

ARM9_struct ARM9Mem;

NDS_Screen MainScreen;
NDS_Screen SubScreen;

//#define DEBUG_TRI

/*****************************************************************************/
//			INITIALIZATION
/*****************************************************************************/

GPU * GPU_Init(u8 l)
{
     GPU * g;

     if ((g = (GPU *) malloc(sizeof(GPU))) == NULL)
        return NULL;

     GPU_Reset(g, l);
     return g;
}

void GPU_Reset(GPU *g, u8 l)
{
   memset(g, 0, sizeof(GPU));
}

void GPU_DeInit(GPU * gpu)
{
	if (gpu) free(gpu);
}


int Screen_Init(int coreid) {
   MainScreen.gpu = GPU_Init(0);
   SubScreen.gpu = GPU_Init(1);

   return 0;
}

void Screen_Reset(void) {
   GPU_Reset(MainScreen.gpu, 0);
   GPU_Reset(SubScreen.gpu, 1);
}

void Screen_DeInit(void) {
	GPU_DeInit(MainScreen.gpu);
	GPU_DeInit(SubScreen.gpu);

}
