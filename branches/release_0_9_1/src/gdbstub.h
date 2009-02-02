/* $Id: gdbstub.h,v 1.1 2007-06-07 09:43:25 masscat Exp $
 */
/*  Copyright (C) 2006 Ben Jaques
 * masscat@btinternet.com
 *
 * This file is part of DeSmuME
 *
 * DeSmuME is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * DeSmuME is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with DeSmuME; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#ifndef _GDBSTUB_H_
#define _GDBSTUB_H_ 1

#include "types.h"

typedef void *gdbstub_handle_t;

/*
 * The function interface
 */

gdbstub_handle_t
createStub_gdb( u16 port,
                struct armcpu_memory_iface **cpu_memio,
                struct armcpu_memory_iface *direct_memio);

void
destroyStub_gdb( gdbstub_handle_t stub);

void
activateStub_gdb( gdbstub_handle_t stub,
                  struct armcpu_ctrl_iface *cpu_ctrl);

  /*
   * An implementation of the following functions is required
   * for the GDB stub to function.
   */
void *
createThread_gdb( void (WINAPI *thread_function)( void *data),
                  void *thread_data);

void
joinThread_gdb( void *thread_handle);

#endif /* End of _GDBSTUB_H_ */

