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

/* wait until either of 2 gdb stubs gives control back to the emulator.
   pass a stubs[2], one of them may be NULL.
   the primary usecase for this is to do a blocking wait until the stub returns
   control to the emulator, in order to not waste cpu cycles.
   the timeout parameter specifies the maximum time in milliseconds to wait.
   if timeout == -1L, block indefinitely, if timeout == 0 return immediately,
   effecting a poll.
   return value: response from stub | (stub number<<31),
   i.e. if stub 1 responded, the high bit is set (and so the result negative).
   returns 0 on failure or if no response was available. */
int gdbstub_wait( gdbstub_handle_t *stubs, long timeout);

/* enable or disable use of the pipe for gdbstub_wait() */
void gdbstub_wait_set_enabled(gdbstub_handle_t stub, int on);


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

