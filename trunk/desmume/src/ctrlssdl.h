/* joysdl.h - this file is part of DeSmuME
 *
 * Copyright (C) 2007 Pascal Giard
 *
 * Author: Pascal Giard <evilynux@gmail.com>
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

#ifndef CTRLSSDL_H
#define CTRLSSDL_H

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <SDL/SDL.h>

#include "types.h"

#define NB_KEYS		14

#define ADD_KEY(keypad,key) ( (keypad) |= (key) )
#define RM_KEY(keypad,key) ( (keypad) &= ~(key) )
#define KEYMASK_(k)	(1 << k)

u16 joypadCfg[NB_KEYS];
u16 nbr_joy;

#ifndef GTK_UI
struct mouse_status
{
  signed long x;
  signed long y;
  BOOL click;
  BOOL down;
};

struct mouse_status mouse;

BOOL sdl_quit;

void set_mouse_coord(signed long x,signed long y);
#endif // !GTK_UI

BOOL init_joy(u16 joyCfg[]);
void uninit_joy();
u16 get_set_joy_key(int index);
u16 process_ctrls_events(u16 oldkeypad);

#endif /* CTRLSSDL_H */
