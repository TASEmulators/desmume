/* desmume.c - this file is part of DeSmuME
 *
 * Copyright (C) 2006,2007 DeSmuME Team
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

volatile BOOL execute = FALSE;
BOOL click = FALSE;
BOOL fini = FALSE;
unsigned long glock = 0;

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

void desmume_pause()
{
	execute = FALSE;
}
void desmume_resume()
{
	execute = TRUE;
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
	process_joy_events();
	desmume_last_cycle = NDS_exec((560190 << 1) - desmume_last_cycle, FALSE);
        SPU_Emulate();
}

void desmume_keypad(u16 k)
{
  printf("Update keypad...%x\n", k);
	unsigned short k_ext = (k >> 10) & 0x3;
	unsigned short k_pad = k & 0x3FF;
	((unsigned short *)ARM9Mem.ARM9_REG)[0x130>>1] = k_pad;
	((unsigned short *)MMU.ARM7_REG)[0x130>>1] = k_pad;
	MMU.ARM7_REG[0x136] = (k_ext & 0x3) | (MMU.ARM7_REG[0x136] & ~0x3);
}
 
