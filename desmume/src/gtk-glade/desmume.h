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

#include "globals.h"


#define FPS_LIMITER_FRAME_PERIOD 5
extern SDL_sem *glade_fps_limiter_semaphore;
extern int glade_fps_limiter_disabled;

extern void desmume_init();
extern void desmume_free();

extern int desmume_open(const char *filename);
extern void desmume_savetype(int type);
extern void desmume_pause();
extern void desmume_resume();
extern void desmume_reset();
extern void desmume_toggle();
extern BOOL desmume_running();

extern void desmume_cycle();
#endif /*__DESMUME_H__*/

