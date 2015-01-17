/*
	Copyright (C) 2006 Ben Jaques
	Copyright (C) 2008-2015 DeSmuME team

	This file is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 2 of the License, or
	(at your option) any later version.

	This file is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with the this software.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef _GDBSTUB_H_
#define _GDBSTUB_H_ 1

#include "types.h"

typedef void *gdbstub_handle_t;
struct armcpu_t;
struct armcpu_memory_iface;

void gdbstub_mutex_init();
void gdbstub_mutex_destroy();
void gdbstub_mutex_lock();
void gdbstub_mutex_unlock();

/*
 * The function interface
 */

gdbstub_handle_t
createStub_gdb( u16 port,
                armcpu_t *theCPU,
                const armcpu_memory_iface *direct_memio);

void
destroyStub_gdb( gdbstub_handle_t stub);

void
activateStub_gdb( gdbstub_handle_t stub);

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

