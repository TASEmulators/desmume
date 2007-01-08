/* desmume.h - this file is part of DeSmuME
 *
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

#ifndef __DESMUME_H__
#define __DESMUME_H__


#define DESMUME_NB_KEYS		14

#define DESMUME_KEYMASK_(k)	(1 << k)

#define DESMUME_KEY_NONE		0
#define DESMUME_KEY_A			1
#define DESMUME_KEY_B			2
#define DESMUME_KEY_Select		3
#define DESMUME_KEY_Start		4
#define DESMUME_KEY_Right		5
#define DESMUME_KEY_Left		6
#define DESMUME_KEY_Up			7
#define DESMUME_KEY_Down		8
#define DESMUME_KEY_R			9
#define DESMUME_KEY_L			10

#define DESMUME_KEY_Y			11
#define DESMUME_KEY_X			12
#define DESMUME_KEY_DEBUG		13
#define DESMUME_KEY_BOOST		14

#include "joysdl.h"

extern void desmume_init();
extern void desmume_free();

extern int desmume_open(const char *filename);
extern void desmume_pause();
extern void desmume_resume();
extern void desmume_reset();
extern void desmume_toggle();
extern BOOL desmume_running();

extern void desmume_cycle();

extern void desmume_keypad(u16 k); 

#endif /*__DESMUME_H__*/

