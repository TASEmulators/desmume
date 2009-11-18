/* desmume.h - this file is part of DeSmuME
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

#ifndef __DESMUME_H__
#define __DESMUME_H__

extern void desmume_init( struct armcpu_memory_iface *arm9_mem_if,
                          struct armcpu_ctrl_iface **arm9_ctrl_iface,
                          struct armcpu_memory_iface *arm7_mem_if,
                          struct armcpu_ctrl_iface **arm7_ctrl_iface,
                          int disable_sound);
extern void desmume_free( void);

extern void desmume_pause( void);
extern void desmume_resume( void);
extern void desmume_toggle( void);
extern bool desmume_running( void);

extern void desmume_cycle( void);

#endif /*__DESMUME_H__*/

