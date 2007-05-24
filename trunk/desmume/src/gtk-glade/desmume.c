/* desmume.c - this file is part of DeSmuME
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

#include "desmume.h"

#define EMULOOP_PRIO (G_PRIORITY_HIGH_IDLE + 20)
gboolean EmuLoop(gpointer data);
static BOOL regMainLoop = FALSE;


static BOOL noticed_3D=FALSE;
volatile BOOL execute = FALSE;
BOOL click = FALSE;
BOOL fini = FALSE;
unsigned long glock = 0;
int savetype=MC_TYPE_AUTODETECT;
u32 savesize=1;

void desmume_mem_init();

u8 *desmume_rom_data = NULL;
u32 desmume_last_cycle;

void desmume_init()
{
	NDS_Init();
        SPU_ChangeSoundCore(SNDCORE_SDL, 735 * 4);
	execute = FALSE;
}

void desmume_free()
{
	execute = FALSE;
	NDS_DeInit();
}

int desmume_open(const char *filename)
{
  int i;
  noticed_3D=attempted_3D_op=FALSE;
  clear_savestates();
  i = NDS_LoadROM(filename, savetype, savesize);
  return i;
}

void desmume_savetype(int type) {
	mmu_select_savetype(type, &savetype, &savesize);
}


void desmume_pause()
{
	execute = FALSE;
	SPU_Pause(1);
}

void desmume_resume()
{
	SPU_Pause(0);
	execute = TRUE;
	if(!regMainLoop)
		g_idle_add_full(EMULOOP_PRIO, &EmuLoop, NULL, NULL);
	regMainLoop = TRUE;
}

void desmume_reset()
{
	noticed_3D=attempted_3D_op=FALSE;
	NDS_Reset();
	desmume_resume();
}

void desmume_toggle()
{
	execute = (execute) ? FALSE : TRUE;
}
BOOL desmume_running()
{
	return execute;
}

void desmume_cycle()
{
  u16 keypad;
  /* Joystick events */
  /* Retrieve old value: can use joysticks w/ another device (from our side) */
  keypad = get_keypad();
  /* Process joystick events if any */
  process_joystick_events( &keypad);
  /* Update keypad value */
  update_keypad(keypad);

  desmume_last_cycle = NDS_exec((560190 << 1) - desmume_last_cycle, FALSE);
  SPU_Emulate();
}


Uint32 fps, fps_SecStart, fps_FrameCount;
static void Draw()
{
}

gboolean EmuLoop(gpointer data)
{
	int i;
	if (!noticed_3D && attempted_3D_op) {
		GtkWidget * dlg = glade_xml_get_widget(xml, "w3Dop");
		gtk_widget_show(dlg);
		noticed_3D=TRUE;
	}
		
	if(desmume_running())	/* Si on est en train d'executer le programme ... */
	{
	  static int limiter_frame_counter = 0;
		fps_FrameCount += Frameskip + 1;
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
		for(i = 0; i < Frameskip; i++) desmume_cycle();	/* cycles supplémentaires pour le frameskip */
		
		Draw();
		
		notify_Tools();
		gtk_widget_queue_draw(pDrawingArea);
		gtk_widget_queue_draw(pDrawingArea2);

                if ( !glade_fps_limiter_disabled) {
                  limiter_frame_counter += 1;
                  if ( limiter_frame_counter >= FPS_LIMITER_FRAME_PERIOD) {
                    limiter_frame_counter = 0;

                    /* wait for the timer to expire */
                    SDL_SemWait( glade_fps_limiter_semaphore);
                  }
                }

		return TRUE;
	}
	gtk_widget_queue_draw(pDrawingArea);
	gtk_widget_queue_draw(pDrawingArea2);
	regMainLoop = FALSE;
	return FALSE;
}

