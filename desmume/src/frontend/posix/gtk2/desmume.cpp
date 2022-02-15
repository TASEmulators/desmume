/* desmume.c - this file is part of DeSmuME
 *
 * Copyright (C) 2006-2015 DeSmuME Team
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

#include "../NDSSystem.h"
#include "../SPU.h"
#include "../shared/sndsdl.h"
#include "../movie.h"

volatile bool execute = false;
BOOL click = FALSE;

void desmume_init( int disable_sound)
{
	NDS_Init();
	
	if ( !disable_sound) {
		SPU_ChangeSoundCore(SNDCORE_SDL, 735 * 4);
	}
	execute = false;
}

void desmume_free( void)
{
	execute = false;
	NDS_DeInit();
}

void desmume_pause( void)
{
	execute = false;
	SPU_Pause(1);
}
void desmume_resume( void)
{
	execute = true;
	SPU_Pause(0);
}
void desmume_toggle( void)
{
	execute ^= true;
}
bool desmume_running( void)
{
	return execute;
}

void desmume_cycle( void)
{
  FCEUMOV_AddInputState();
  NDS_exec<false>();
  SPU_Emulate_user();
}
 
