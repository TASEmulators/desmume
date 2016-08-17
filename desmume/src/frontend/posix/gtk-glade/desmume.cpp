/* desmume.c - this file is part of DeSmuME
 *
 * Copyright (C) 2007-2015 DeSmuME Team
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

#include "desmume.h"

#define EMULOOP_PRIO (G_PRIORITY_HIGH_IDLE + 20)
gboolean EmuLoop(gpointer data);
static BOOL regMainLoop = FALSE;

#define TICKS_PER_FRAME 17


static BOOL noticed_3D=FALSE;
volatile bool execute = false;
BOOL click = FALSE;

void desmume_mem_init();

u8 *desmume_rom_data = NULL;
u32 desmume_last_cycle;

void desmume_init()
{
        NDS_Init();
        SPU_ChangeSoundCore(SNDCORE_SDL, 735 * 4);
	execute = false;
}

void desmume_free()
{
	execute = false;
	NDS_DeInit();
}

int desmume_open(const char *filename)
{
  int i;
  noticed_3D=FALSE;
  clear_savestates();
  i = NDS_LoadROM(filename);
  return i;
}

void desmume_savetype(int type) {
	backup_setManualBackupType(type);
}


void desmume_pause()
{
	execute = false;
	SPU_Pause(1);
}

void desmume_resume()
{
	SPU_Pause(0);
	execute = true;
	if(!regMainLoop)
		g_idle_add_full(EMULOOP_PRIO, &EmuLoop, NULL, NULL);
	regMainLoop = TRUE;
}

void desmume_reset()
{
	noticed_3D=FALSE;
	NDS_Reset();
	desmume_resume();
}

void desmume_toggle()
{
	execute ^= true;
}
/*INLINE BOOL desmume_running()
{
	return execute;
}*/

INLINE void desmume_cycle()
{
  u16 keypad;
  /* Joystick events */
  /* Retrieve old value: can use joysticks w/ another device (from our side) */
  keypad = get_keypad();
  /* Process joystick events if any */
  process_joystick_events( &keypad);
  /* Update keypad value */
  update_keypad(keypad);

  NDS_exec<false>();
  SPU_Emulate_user();
}


Uint32 fps, fps_SecStart, fps_FrameCount;
Uint32 fsFrameCount = 0;
Uint32 ticksPrevFrame = 0, ticksCurFrame = 0;
static void Draw()
{
}

gboolean EmuLoop(gpointer data)
{
	if(desmume_running())	/* Si on est en train d'executer le programme ... */
	{
		if(Frameskip != 0 && (fsFrameCount % (Frameskip+1)) != 0)
			NDS_SkipNextFrame();	
	
		fsFrameCount++;
	  
		fps_FrameCount++;
		if(!fps_SecStart) fps_SecStart = SDL_GetTicks();
		if(SDL_GetTicks() - fps_SecStart >= 1000)
		{
			fps_SecStart = SDL_GetTicks();
			fps = fps_FrameCount;
			fps_FrameCount = 0;
			
			char Title[32];
			sprintf(Title, "Desmume - %dfps", fps);
			gtk_window_set_title(GTK_WINDOW(pWindow), Title);
		}
		
		desmume_cycle();	/* Emule ! */
		//for(i = 0; i < Frameskip; i++) desmume_cycle();	/* cycles supplémentaires pour le frameskip */
		
		Draw();
		
		notify_Tools();
		gtk_widget_queue_draw(pDrawingArea);
		gtk_widget_queue_draw(pDrawingArea2);
		
		ticksCurFrame = SDL_GetTicks();
		
		if(!glade_fps_limiter_disabled)
		{
			if((ticksCurFrame - ticksPrevFrame) < TICKS_PER_FRAME)
				while((ticksCurFrame - ticksPrevFrame) < TICKS_PER_FRAME)
					ticksCurFrame = SDL_GetTicks();
		}
		
		ticksPrevFrame = SDL_GetTicks();

		return TRUE;
	}
	gtk_widget_queue_draw(pDrawingArea);
	gtk_widget_queue_draw(pDrawingArea2);
	regMainLoop = FALSE;
	return FALSE;
}

