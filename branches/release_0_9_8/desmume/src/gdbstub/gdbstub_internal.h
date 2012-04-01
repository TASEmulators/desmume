/*
	Copyright (C) 2008-2009 DeSmuME team

	Originally written by Ben Jaques.

	Permission is hereby granted, free of charge, to any person obtaining a copy
	of this software and associated documentation files (the "Software"), to deal
	in the Software without restriction, including without limitation the rights
	to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
	copies of the Software, and to permit persons to whom the Software is
	furnished to do so, subject to the following conditions:

	The above copyright notice and this permission notice shall be included in
	all copies or substantial portions of the Software.

	THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
	IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
	FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
	AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
	LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
	OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
	THE SOFTWARE.
*/

#ifndef _GDBSTUB_INTERNAL_H_
#define _GDBSTUB_INTERNAL_H_ 1

#ifdef WIN32
#define SOCKET_TYPE SOCKET
#else
#define SOCKET_TYPE int
#endif


enum stop_type {
  STOP_UNKNOWN,
  STOP_HOST_BREAK,
  STOP_STEP_BREAK,
  STOP_BREAKPOINT,
  STOP_WATCHPOINT,
  STOP_RWATCHPOINT,
  STOP_AWATCHPOINT
};


/**
 * The structure describing a breakpoint.
 */
struct breakpoint_gdb {
  /** link them in a list */
  struct breakpoint_gdb *next;

  /** The address of the breakpoint */
  uint32_t addr;

  /** The size of the breakpoint */
  uint32_t size;
};


/*
 */
#define BUFMAX_GDB 2048

struct packet_reader_gdb {
  int state;

  int pos_index;

  uint8_t checksum;

  uint8_t read_checksum;

  uint8_t buffer[BUFMAX_GDB];
};

/** The maximum number of breakpoints (of all types) available to the stub */
#define BREAKPOINT_POOL_SIZE 100


struct gdb_stub_state {
  /** flag indicating if the stub is active */
  int active;

  int main_stop_flag;

  /** the listener thread */
  void *thread;

  /** the id of the cpu the is under control */
  //u32 cpu_id;

  /** the interface used to control the CPU */
  struct armcpu_ctrl_iface *cpu_ctrl;

  /** the memory interface passed to the CPU */
  struct armcpu_memory_iface cpu_memio;

  /** the direct interface to the memory system */
  struct armcpu_memory_iface *direct_memio;

  /** the CPU memory interface to the real memory system */
  struct armcpu_memory_iface *real_cpu_memio;

  /** the list of active instruction breakpoints */
  struct breakpoint_gdb *instr_breakpoints;

  /** the list of active read breakpoints */
  struct breakpoint_gdb *read_breakpoints;

  /** the list of active write breakpoints */
  struct breakpoint_gdb *write_breakpoints;

  /** the list of active access breakpoints */
  struct breakpoint_gdb *access_breakpoints;

  /** the pointer to the step break point (not NULL if set) */
  //struct breakpoint_gdb *step_breakpoint;

  uint32_t step_instr_address;

  /** the state of the stub as seen by the emulator, the emulator side can
   * set this to STOPPING_EMU_GDB_STATE to indicate that the a breakpoint has been hit,
   * and STOPPED_EMU_GDB_STATE when it has informed the gdb thread that a breakpoint has
   * been hit.
   * When handled the stub side will set it back to RUNNING_EMU_GDB_STATE.
   *
   * The emulator should only run the corresponding ARM if this is set to
   * RUNNING_EMU_GDB_STATE.
   */
  enum EMU_STUB_STATE { STOPPED_EMU_GDB_STATE, STOPPING_EMU_GDB_STATE, RUNNING_EMU_GDB_STATE} emu_stub_state;

  /** the state of the stub as set by the stub control thread */
  enum CTL_STUB_STATE { STOPPED_GDB_STATE, RUNNING_GDB_STATE,
	 STEPPING_GDB_STATE, START_RUN_GDB_STATE} ctl_stub_state;

  struct packet_reader_gdb rx_packet;

  /** the socket information */
  uint16_t port_num;
  SOCKET_TYPE sock_fd;

  /** The listening socket */
  SOCKET_TYPE listen_fd;

  /** the type of event that caused the stop */
  enum stop_type stop_type;

  /** the address of the stop */
  uint32_t stop_address;

  /** The step break point decsriptor */
  struct breakpoint_gdb step_breakpoint_descr;

  /** the breakpoint descriptor pool */
  struct breakpoint_gdb breakpoint_pool[BREAKPOINT_POOL_SIZE];

  /** the free breakpoint descriptor list */
  struct breakpoint_gdb *free_breakpoints;

  /** the control pipe (or socket) to the gdb stub */
  SOCKET_TYPE ctl_pipe[2];
};


enum read_res_gdb {
  READ_NOT_FINISHED,
  READ_SOCKET_ERROR,
  READ_COMPLETE,
  READ_BREAK
};

#endif /* End of _GDBSTUB_INTERNAL_H_ */
