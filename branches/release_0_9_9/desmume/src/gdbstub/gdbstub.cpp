/*
	Copyright (C) 2008-2010 DeSmuME team

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

#include <errno.h>
//#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <fcntl.h>

#include "types.h"
#include "NDSSystem.h"

#ifdef WIN32
#include <winsock2.h>
#else
#include <unistd.h>
#include <sys/socket.h>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#endif

#include "armcpu.h"

#define uint32_t u32
#define uint16_t u16
#define uint8_t u8

#include "gdbstub.h"
#include "gdbstub_internal.h"

#ifdef __GNUC__
#define UNUSED_PARM( parm) parm __attribute__((unused))
#else
#define UNUSED_PARM( parm) parm
#endif

#if 1
#define DEBUG_LOG( fmt, ...) fprintf(stdout, fmt, ##__VA_ARGS__)
#else
#define DEBUG_LOG( fmt, ...)
#endif

#if 0
#define LOG_ERROR( fmt, ...) fprintf(stderr, fmt, ##__VA_ARGS__)
#else
#define LOG_ERROR( fmt, ...)
#endif

#define LITTLE_ENDIAN_TO_UINT32_T( v) (\
  ((v)[3] << 24) | \
  ((v)[2] << 16) | \
  ((v)[1] <<  8) | \
  ((v)[0] <<  0))

/*
 * The state of the packet reader
 */
#define IDLE_READ_STATE 0
#define MID_PACKET_READ_STATE 1
#define FIRST_CHECKSUM_READ_STATE 2
#define SECOND_CHECKSUM_READ_STATE 3

/*
 * The gdb stub thread control message types.
 */
enum stub_message_type {
  /** The quit message type */
  QUIT_STUB_MESSAGE = 0,
  /** The emulated CPU has stopped */
  CPU_STOPPED_STUB_MESSAGE = 1
};


/*
 * The GDB signal values
 */
enum target_signal
  {
    /* Used some places (e.g. stop_signal) to record the concept that
       there is no signal.  */
    TARGET_SIGNAL_0 = 0,
    TARGET_SIGNAL_FIRST = 0,
    TARGET_SIGNAL_HUP = 1,
    TARGET_SIGNAL_INT = 2,
    TARGET_SIGNAL_QUIT = 3,
    TARGET_SIGNAL_ILL = 4,
    TARGET_SIGNAL_TRAP = 5,
    TARGET_SIGNAL_ABRT = 6,
    TARGET_SIGNAL_EMT = 7,
    TARGET_SIGNAL_FPE = 8,
    TARGET_SIGNAL_KILL = 9,
    TARGET_SIGNAL_BUS = 10,
    TARGET_SIGNAL_SEGV = 11,
    TARGET_SIGNAL_SYS = 12,
    TARGET_SIGNAL_PIPE = 13,
    TARGET_SIGNAL_ALRM = 14,
    TARGET_SIGNAL_TERM = 15,
    TARGET_SIGNAL_URG = 16,
    TARGET_SIGNAL_STOP = 17,
    TARGET_SIGNAL_TSTP = 18,
    TARGET_SIGNAL_CONT = 19,
    TARGET_SIGNAL_CHLD = 20,
    TARGET_SIGNAL_TTIN = 21,
    TARGET_SIGNAL_TTOU = 22,
    TARGET_SIGNAL_IO = 23,
    TARGET_SIGNAL_XCPU = 24,
    TARGET_SIGNAL_XFSZ = 25,
    TARGET_SIGNAL_VTALRM = 26,
    TARGET_SIGNAL_PROF = 27,
    TARGET_SIGNAL_WINCH = 28,
    TARGET_SIGNAL_LOST = 29,
    TARGET_SIGNAL_USR1 = 30,
    TARGET_SIGNAL_USR2 = 31,
    TARGET_SIGNAL_PWR = 32,
    /* Similar to SIGIO.  Perhaps they should have the same number.  */
    TARGET_SIGNAL_POLL = 33,
    TARGET_SIGNAL_WIND = 34,
    TARGET_SIGNAL_PHONE = 35,
    TARGET_SIGNAL_WAITING = 36,
    TARGET_SIGNAL_LWP = 37,
    TARGET_SIGNAL_DANGER = 38,
    TARGET_SIGNAL_GRANT = 39,
    TARGET_SIGNAL_RETRACT = 40,
    TARGET_SIGNAL_MSG = 41,
    TARGET_SIGNAL_SOUND = 42,
    TARGET_SIGNAL_SAK = 43,
    TARGET_SIGNAL_PRIO = 44,
  };





static void
causeQuit_gdb( struct gdb_stub_state *stub) {
  uint8_t command = QUIT_STUB_MESSAGE;

#ifdef WIN32
  send( stub->ctl_pipe[1], (char*)&command, 1, 0);
#else
  write( stub->ctl_pipe[1], &command, 1);
#endif
}

static void
indicateCPUStop_gdb( struct gdb_stub_state *stub) {
  uint8_t command = CPU_STOPPED_STUB_MESSAGE;

#ifdef WIN32
  send( stub->ctl_pipe[1], (char*)&command, 1, 0);
#else
  write( stub->ctl_pipe[1], &command, 1);
#endif
}




/*
 *
 *
 *
 */
static void
break_execution( void *data, UNUSED_PARM(uint32_t addr), UNUSED_PARM(int thunmb)) {
  struct gdb_stub_state *stub = (struct gdb_stub_state *)data;

  /* stall the processor */
  stub->cpu_ctrl->stall( stub->cpu_ctrl->data);
  NDS_debug_break();

  /* remove the post execution function */
  stub->cpu_ctrl->remove_post_ex_fn( stub->cpu_ctrl->data);

  /* indicate the halt */
  stub->stop_type = STOP_HOST_BREAK;
  indicateCPUStop_gdb( stub);
}


static void
step_instruction_watch( void *data, uint32_t addr, UNUSED_PARM(int thunmb)) {
  struct gdb_stub_state *stub = (struct gdb_stub_state *)data;

  DEBUG_LOG("Step watch: waiting for %08x at %08x\n", stub->step_instr_address,
      addr);

  if ( addr == stub->step_instr_address) {
    DEBUG_LOG("Step hit -> %08x\n", stub->cpu_ctrl->read_reg( stub->cpu_ctrl->data, 15));
    /* stall the processor */
    stub->cpu_ctrl->stall( stub->cpu_ctrl->data);

    /* remove the post execution function */
    stub->cpu_ctrl->remove_post_ex_fn( stub->cpu_ctrl->data);

    /* indicate the halt */
    stub->stop_type = STOP_STEP_BREAK;
    indicateCPUStop_gdb( stub);

	NDS_debug_break();
  }
}





/************************************************************************/
/* BUFMAX defines the maximum number of characters in inbound/outbound buffers*/
/* at least NUMREGBYTES*2 are needed for register packets */
#define BUFMAX 2048



static const char hexchars[]="0123456789abcdef";

/*
 * Convert ch from a hex digit to an int
 */
static int
hex (unsigned char ch) {
  if (ch >= 'a' && ch <= 'f')
    return ch-'a'+10;
  if (ch >= '0' && ch <= '9')
    return ch-'0';
  if (ch >= 'A' && ch <= 'F')
    return ch-'A'+10;
  return -1;
}

static const unsigned char *
hex2mem ( const unsigned char *buf, uint8_t *mem, int count) {
  int i;
  unsigned char ch;

  for (i=0; i<count; i++) {
    ch = hex(*buf++) << 4;
    ch |= hex(*buf++);

    *mem = ch;
    mem += 1;
  }

  return buf;
}

/*
 * While we find nice hex chars, build an int.
 * Return number of chars processed.
 */
static int
hexToInt( const uint8_t **ptr, uint32_t *intValue)
{
  int numChars = 0;
  int hexValue;

  *intValue = 0;

  while (**ptr) {
    hexValue = hex(**ptr);
    if (hexValue < 0)
      break;

    *intValue = (*intValue << 4) | hexValue;
    numChars ++;

    (*ptr)++;
  }

  return (numChars);
}

/* Convert the memory pointed to by mem into hex, placing result in buf.
 * Return a pointer to the last char put in buf (null), in case of mem fault,
 * return 0.
 * If MAY_FAULT is non-zero, then we will handle memory faults by returning
 * a 0, else treat a fault like any other fault in the stub.
 */

static uint8_t *
mem2hex ( struct armcpu_memory_iface *memio, uint32_t mem_addr,
          uint8_t *buf, int count)
{
  uint8_t ch;

  //set_mem_fault_trap(may_fault);

  while (count-- > 0)
    {
      ch = memio->read8( memio->data, mem_addr++);
      *buf++ = hexchars[ch >> 4];
      *buf++ = hexchars[ch & 0xf];
    }

  *buf = 0;

  return buf;
}



static enum read_res_gdb
readPacket_gdb( SOCKET_TYPE sock, struct packet_reader_gdb *packet) {
  uint8_t cur_byte;
  enum read_res_gdb read_res = READ_NOT_FINISHED;
  int sock_res;

  /* update the state */

  while ( (sock_res = recv( sock, (char*)&cur_byte, 1, 0)) == 1) {
    switch ( packet->state) {
    case IDLE_READ_STATE:
      /* wait for the '$' start of packet character
       */
      if ( cur_byte == '$') {
	//DEBUG_LOG( "Found packet\n");
	packet->state = MID_PACKET_READ_STATE;
	packet->pos_index = 0;
	packet->checksum = 0;
      }
      /* Is this the break command */
      else if ( cur_byte == 3) {
	//DEBUG_LOG( "GDB has sent a break\n");
	packet->buffer[0] = cur_byte;
	packet->buffer[1] = 0;
	packet->pos_index = 1;
	return READ_BREAK;
      }
      break;

    case MID_PACKET_READ_STATE:
      if ( cur_byte == '#') {
	//DEBUG_LOG( "\nAbout to get checksum\n");
	packet->buffer[packet->pos_index] = '\0';
	packet->state = FIRST_CHECKSUM_READ_STATE;
      }
      else if ( packet->pos_index >= BUFMAX - 1) {
	//DEBUG_LOG( "read buffer exceeded\n");
	packet->state = IDLE_READ_STATE;
      }
      else {
	//DEBUG_LOG( "%c", cur_byte);
	packet->checksum = packet->checksum + cur_byte;
	packet->buffer[packet->pos_index] = cur_byte;
	packet->pos_index = packet->pos_index + 1;
      }
      break;

    case FIRST_CHECKSUM_READ_STATE:
      packet->read_checksum = hex( cur_byte) << 4;
      packet->state = SECOND_CHECKSUM_READ_STATE;
      break;

    case SECOND_CHECKSUM_READ_STATE: {
      packet->read_checksum += hex( cur_byte);
      packet->state = IDLE_READ_STATE;
      return READ_COMPLETE;
      break;
    }
    }
  }

  if ( sock_res == 0) {
    read_res = READ_SOCKET_ERROR;
  }
  else if ( sock_res == -1) {
    if ( errno != EAGAIN) {
      read_res = READ_SOCKET_ERROR;
    }
  }

  return read_res;
}


struct hidden_debug_out_packet {
  unsigned char *start_ptr;
};
struct debug_out_packet {
  unsigned char *const start_ptr;
};

static struct hidden_debug_out_packet the_out_packet;
static unsigned char hidden_buffer[BUFMAX];


static struct debug_out_packet *
getOutPacket( void) {
  the_out_packet.start_ptr = &hidden_buffer[1];
  return (struct debug_out_packet *)&the_out_packet;
}

#define CHECKSUM_PART_SIZE 3
/**
 * send the packet in buffer.
 */
static int
putpacket ( SOCKET_TYPE sock, struct debug_out_packet *out_packet, uint32_t size) {
  unsigned char checksum = 0;
  uint32_t count;
  unsigned char *ch_ptr = (unsigned char *)&out_packet->start_ptr[-1];
  uint8_t reply_ch;

  //DEBUG_LOG_START( "Putting packet size %d ", size);
  /* add the '$' to the start of the packet */
  *ch_ptr++ = '$';

  /* calculate and add the checksum to the packet */
  for ( count = 0; count < size; count += 1) {
    checksum += ch_ptr[count];
    //DEBUG_LOG_PART("%c", ch_ptr[count]);
  }
  //DEBUG_LOG_PART("\n");

  ch_ptr[count++] = '#';
  ch_ptr[count++] = hexchars[checksum >> 4];
  ch_ptr[count++] = hexchars[checksum & 0xf];
  ch_ptr[count] = '\0';
  //DEBUG_LOG("packet %s\n", &out_packet->start_ptr[-1]);

  /* add one for the '$' character */
  count += 1;

  do {
    int reply_found = 0;

    send( sock, (char*)&out_packet->start_ptr[-1], count, 0);

    do {
      int read_res = recv( sock, (char*)&reply_ch, 1, 0);

      if ( read_res == 0) {
	return -1;
      }
      else if ( read_res == -1) {
	if ( errno != EAGAIN) {
	  return -1;
	}
      }
      else {
	reply_found = 1;
      }
    } while ( !reply_found);
  } while ( reply_ch != '+');

  /*  $<packet info>#<checksum>. */
  return count - 4;
}



static uint32_t
make_stop_packet( uint8_t *ptr, enum stop_type type, uint32_t stop_address) {
  uint32_t stop_size = 0;
  int watch_index = 0;
  const char watch_chars[] = { 'a', 'r' };

  switch (type) {
  case STOP_UNKNOWN:
  case STOP_HOST_BREAK:
    ptr[0] = 'S';
    ptr[1] = hexchars[TARGET_SIGNAL_INT >> 4];
    ptr[2] = hexchars[TARGET_SIGNAL_INT & 0xf];
    stop_size = 3;
    break;

  case STOP_STEP_BREAK:
  case STOP_BREAKPOINT:
    ptr[0] = 'S';
    ptr[1] = hexchars[TARGET_SIGNAL_TRAP >> 4];
    ptr[2] = hexchars[TARGET_SIGNAL_TRAP & 0xf];
    stop_size = 3;
    break;

  case STOP_WATCHPOINT:
    watch_index = 1;
  case STOP_RWATCHPOINT:
    watch_index += 1;
  case STOP_AWATCHPOINT:
    {
      int i;
      int out_index = 0;
      ptr[out_index++] = 'T';
      ptr[out_index++] = hexchars[TARGET_SIGNAL_ABRT >> 4];
      ptr[out_index++] = hexchars[TARGET_SIGNAL_ABRT & 0xf];

      if ( watch_index < 2) {
        ptr[out_index++] = watch_chars[watch_index];
      }
      ptr[out_index++] = 'w';
      ptr[out_index++] = 'a';
      ptr[out_index++] = 't';
      ptr[out_index++] = 'c';
      ptr[out_index++] = 'h';
      ptr[out_index++] = ':';

      for ( i = 0; i < 8; i++) {
        ptr[out_index++] = hexchars[(stop_address >> (i * 4)) & 0xf];
      }
      ptr[out_index++] = ';';

      stop_size = out_index;
    }
    break;
  }

  return stop_size;
}

/**
 * Returns -1 if there is a socket error.
 */
static int
processPacket_gdb( SOCKET_TYPE sock, const uint8_t *packet,
		   struct gdb_stub_state *stub) {
  //  uint8_t remcomOutBuffer[BUFMAX_GDB];
  struct debug_out_packet *out_packet = getOutPacket();
  uint8_t *out_ptr = out_packet->start_ptr;
  int send_reply = 1;
  uint32_t send_size = 0;

  DEBUG_LOG("Processing packet %c\n", packet[0]);

  switch( packet[0]) {
  case 3:
    /* The break command */
    //stub->running_state = gdb_stub_state::STOPPED_GDB_STATE;
    //*ptr++ = 'S';
    //*ptr++ = hexchars[0x2 >> 4];
    //*ptr++ = hexchars[0x2 & 0xf];
    //*ptr++ = 0;
    send_reply = 0;
    break;

  case '?':
    send_size = make_stop_packet( out_ptr, stub->stop_type, stub->stop_address);
    /**ptr++ = 'S';
    *ptr++ = hexchars[stub->stop_reason >> 4];
    *ptr++ = hexchars[stub->stop_reason & 0xf];
    send_size = 3;*/
    break;

  case 'c':
	stub->emu_stub_state = gdb_stub_state::RUNNING_EMU_GDB_STATE;
    stub->ctl_stub_state = gdb_stub_state::START_RUN_GDB_STATE;
    stub->main_stop_flag = 0;
    send_reply = 0;
    /* remove the cpu stall */
    stub->cpu_ctrl->unstall( stub->cpu_ctrl->data);
	NDS_debug_continue();
    break;

  case 's': {
    uint32_t instr_addr = stub->cpu_ctrl->read_reg( stub->cpu_ctrl->data, 15);
    /* Determine where the next instruction will take the CPU.
     * Execute the instruction using a copy of the CPU with a zero memory interface.
     */
    DEBUG_LOG( "Stepping instruction at %08x\n", instr_addr);

    /* install the post execution function */
    stub->step_instr_address = instr_addr;
    stub->cpu_ctrl->install_post_ex_fn( stub->cpu_ctrl->data,
                                        step_instruction_watch,
                                        stub);

    stub->emu_stub_state = gdb_stub_state::RUNNING_EMU_GDB_STATE;
    stub->ctl_stub_state = gdb_stub_state::START_RUN_GDB_STATE;
    stub->main_stop_flag = 0;
    send_reply = 0;

    /* remove the cpu stall */
    stub->cpu_ctrl->unstall( stub->cpu_ctrl->data);
	//NDS_debug_step();
	NDS_debug_continue();
    break;
  }

    /*
     * Register set
     */
  case 'P': {
    uint32_t reg;
    uint32_t value;
    const uint8_t *rx_ptr = &packet[1];

    DEBUG_LOG("Processing packet %s\n", packet);
    if ( hexToInt( &rx_ptr, &reg)) {
      if ( *rx_ptr++ == '=') {
        uint8_t tmp_mem[4];

        rx_ptr = hex2mem( rx_ptr, tmp_mem, 4);
        value = LITTLE_ENDIAN_TO_UINT32_T( tmp_mem);
        DEBUG_LOG("Setting reg %d to %08x\n", reg, value);
        if ( reg < 16)
          stub->cpu_ctrl->set_reg( stub->cpu_ctrl->data, reg, value);
        if ( reg == 25) {
          stub->cpu_ctrl->set_reg( stub->cpu_ctrl->data, 16, value);
        }

        strcpy( (char *)out_ptr, "OK");
        send_size = 2;
      }
    }
    break;
  }

  case 'm': {
    uint32_t addr = 0;
    uint32_t length = 0;
    int error01 = 1;
    const uint8_t *rx_ptr = &packet[1];

    if ( hexToInt( &rx_ptr, &addr)) {
      if ( *rx_ptr++ == ',') {
        if ( hexToInt( &rx_ptr, &length)) {
          //DEBUG_LOG("mem read from %08x (%d)\n", addr, length);
          if ( !mem2hex( stub->direct_memio, addr, out_ptr, length)) {
            strcpy ( (char *)out_ptr, "E03");
            send_size = 3;
          }
          else {
            send_size = length * 2;
          }
          error01 = 0;
        }
      }
    }
    if ( error01)
      strcpy( (char *)out_ptr,"E01");
    break;
  }

    /* MAA..AA,LLLL: Write LLLL bytes at address AA.AA return OK */
  case 'M': {
    /* Try to read '%x,%x:'.  */
    const uint8_t *rx_ptr = &packet[1];
    uint32_t addr = 0;
    uint32_t length = 0;
    int error01 = 1;
    DEBUG_LOG("Memory write %s\n", rx_ptr);
    if ( hexToInt(&rx_ptr, &addr)) {
      if ( *rx_ptr++ == ',') {
        if ( hexToInt(&rx_ptr, &length)) {
          if ( *rx_ptr++ == ':') {
            uint8_t write_byte;
            unsigned int i;
            DEBUG_LOG("Memory write of %d bytes to %08x\n",
                      length, addr);

            for ( i = 0; i < length; i++) {
              rx_ptr = hex2mem( rx_ptr, &write_byte, 1);

              stub->direct_memio->write8( stub->direct_memio->data,
                                          addr++, write_byte);
            }

            strcpy( (char *)out_ptr, "OK");
            error01 = 0;
          }
        }
        else {
          DEBUG_LOG("Failed to find length (addr %08x)\n", addr);
        }
      }
      else {
        DEBUG_LOG("expected ',' got '%c' (addr = %08x)\n", *rx_ptr, addr);
      }
    }
    else {
      DEBUG_LOG("Failed to find addr\n");
    }

    if ( error01) {
      strcpy( (char *)out_ptr, "E02");
    }
    break;
  }

  case 'Z':
  case 'z': {
    const uint8_t *rx_ptr = &packet[2];
    int remove_flag = 0;

    if ( packet[0] == 'z')
      remove_flag = 1;

    DEBUG_LOG( "%c%c packet %s (remove? %d)\n", packet[0], packet[1],
               rx_ptr, remove_flag);

    switch ( packet[1]) {

      /* all instruction breakpoints are treated the same */
    case '0':
    case '1':
    case '2':
    case '3':
    case '4':
      {
	uint32_t addr = 0;
	uint32_t length = 0;
	int error01 = 1;
        struct breakpoint_gdb **bpoint_list;

        switch ( packet[1]) {
        case '0':
        case '1':
          bpoint_list = &stub->instr_breakpoints;
          break;

        case '2':
          bpoint_list = &stub->write_breakpoints;
          break;

        case '3':
          bpoint_list = &stub->read_breakpoints;
          break;

        case '4':
          bpoint_list = &stub->access_breakpoints;
          break;
        }

        if ( *rx_ptr++ == ',') {
          DEBUG_LOG("%s\n", rx_ptr);

          if ( hexToInt( &rx_ptr, &addr)) {
            if ( *rx_ptr++ == ',') {
              DEBUG_LOG("addr %08x %s\n", addr, rx_ptr);

              if ( hexToInt( &rx_ptr, &length)) {
                if ( remove_flag) {
                  int removed = 0;
                  struct breakpoint_gdb *last_bpoint = NULL;
                  struct breakpoint_gdb *bpoint = *bpoint_list;

                  while ( bpoint != NULL && !removed) {
                    if ( bpoint->addr == addr) {
                      DEBUG_LOG("Breakpoint(%c) at %08x removed\n", packet[1], addr);
                      removed = 1;

                      if ( last_bpoint == NULL) {
                        *bpoint_list = bpoint->next;
                      }
                      else {
                        last_bpoint->next = bpoint->next;
                      }
                      bpoint->next = stub->free_breakpoints;
                      stub->free_breakpoints = bpoint;
                    }
                    last_bpoint = bpoint;
                    bpoint = bpoint->next;
                  }

                  strcpy( (char *)out_ptr, "OK");
                  send_size = 2;
                  error01 = 0;
                }
                else {
                  /* get a breakpoint descriptor from the free pool and add it to
                   * the current stub instruction breakpoint list */
                  struct breakpoint_gdb *bpoint = stub->free_breakpoints;

                  if ( bpoint != NULL) {
                    DEBUG_LOG( "Breakpoint(%c) added at %08x length %d\n",
                               packet[1],  addr, length);
                    stub->free_breakpoints = bpoint->next;

                    bpoint->addr = addr;
                    bpoint->size = length;

                    bpoint->next = *bpoint_list;
                    *bpoint_list = bpoint;

                    strcpy( (char *)out_ptr, "OK");
                    send_size = 2;
                    error01 = 0;
                  }
		}
	      }
	    }
	  }
	}

	if ( error01) {
	  strcpy( (char *)out_ptr, "E01");
	  send_size = 3;
	}
      }
      break;

    default:
      break;
    }
    break;
  }

    /*
     * Set the register values
     */
  case 'G':
    {
      int i;
      const uint8_t *rx_ptr = &packet[1];
      uint32_t reg_values[16];
      uint32_t cpsr;
      uint8_t tmp_mem[4];
      DEBUG_LOG("'G' command %s\n", rx_ptr);

      /* general purpose regs 0 to 15 */
      for ( i = 0; i < 16; i++) {
	rx_ptr = hex2mem( rx_ptr, tmp_mem, 4);

        reg_values[i] = LITTLE_ENDIAN_TO_UINT32_T( tmp_mem);
        DEBUG_LOG("Setting reg %d to %08x\n", i, reg_values[i]);
      }

      /* skip the floaing point registers and floating point status register */
      rx_ptr += 8 * (96 / 8 * 2);
      rx_ptr += 8;

      /* the CPSR register is last */
      rx_ptr = hex2mem( rx_ptr, tmp_mem, 4);
      cpsr = LITTLE_ENDIAN_TO_UINT32_T( tmp_mem);
      DEBUG_LOG("Setting cpsr to %08x\n", cpsr);

      strcpy( (char *)out_ptr, "OK");
      send_size = 2;
      break;
    }

  case 'g':		/* return the value of the CPU registers */
    {
      int i;
      int out_index = 0;
      uint32_t pc_value = stub->cpu_ctrl->read_reg( stub->cpu_ctrl->data, 15);
      uint32_t cpsr_value = stub->cpu_ctrl->read_reg( stub->cpu_ctrl->data, 16);

      DEBUG_LOG("'g' command PC = %08x\n", pc_value);

      /* general purpose regs 0 to 14 */
      for ( i = 0; i < 15; i++) {
        uint32_t reg = stub->cpu_ctrl->read_reg( stub->cpu_ctrl->data, i);
	out_ptr[out_index++] = hexchars[(reg >> 4) & 0xf];
	out_ptr[out_index++] = hexchars[(reg >> 0) & 0xf];
	out_ptr[out_index++] = hexchars[(reg >> 12) & 0xf];
	out_ptr[out_index++] = hexchars[(reg >> 8) & 0xf];
	out_ptr[out_index++] = hexchars[(reg >> 20) & 0xf];
	out_ptr[out_index++] = hexchars[(reg >> 16) & 0xf];
	out_ptr[out_index++] = hexchars[(reg >> 28) & 0xf];
	out_ptr[out_index++] = hexchars[(reg >> 24) & 0xf];
      }

      out_ptr[out_index++] = hexchars[(pc_value >> 4) & 0xf];
      out_ptr[out_index++] = hexchars[(pc_value >> 0) & 0xf];
      out_ptr[out_index++] = hexchars[(pc_value >> 12) & 0xf];
      out_ptr[out_index++] = hexchars[(pc_value >> 8) & 0xf];
      out_ptr[out_index++] = hexchars[(pc_value >> 20) & 0xf];
      out_ptr[out_index++] = hexchars[(pc_value >> 16) & 0xf];
      out_ptr[out_index++] = hexchars[(pc_value >> 28) & 0xf];
      out_ptr[out_index++] = hexchars[(pc_value >> 24) & 0xf];

      /* floating point registers (8 96bit) */
      for ( i = 0; i < 8; i++) {
	int j;
	for ( j = 0; j < (96/4); j++) {
	  out_ptr[out_index++] = '0';
	}
      }

      /* floating point status register? */
      for ( i = 0; i < 1; i++) {
	int j;
	for ( j = 0; j < 8; j++) {
	  out_ptr[out_index++] = '0';
	}
      }

      /* The CPSR */
      for ( i = 0; i < 1; i++) {
	out_ptr[out_index++] = hexchars[(cpsr_value >> 4) & 0xf];
	out_ptr[out_index++] = hexchars[(cpsr_value >> 0) & 0xf];
	out_ptr[out_index++] = hexchars[(cpsr_value >> 12) & 0xf];
	out_ptr[out_index++] = hexchars[(cpsr_value >> 8) & 0xf];
	out_ptr[out_index++] = hexchars[(cpsr_value >> 20) & 0xf];
	out_ptr[out_index++] = hexchars[(cpsr_value >> 16) & 0xf];
	out_ptr[out_index++] = hexchars[(cpsr_value >> 28) & 0xf];
	out_ptr[out_index++] = hexchars[(cpsr_value >> 24) & 0xf];
      }
      send_size = out_index;
    }
    break;
  }

  if ( send_reply) {
    return putpacket( sock, out_packet, send_size);
  }

  return 0;
}



/**
 * create the listening socket
 */
static SOCKET_TYPE
createSocket ( int port) {
  SOCKET_TYPE sock;
  struct sockaddr_in bind_addr;

  memset (&bind_addr, 0, sizeof (bind_addr));
  bind_addr.sin_family = AF_INET;
  bind_addr.sin_port = htons( port);
  bind_addr.sin_addr.s_addr = htonl (INADDR_ANY);

  /* create the socket, bind the address */

  sock = socket (PF_INET, SOCK_STREAM, 0);

#ifdef WIN32
  if ( sock != INVALID_SOCKET)
#else
  if (sock != -1)
#endif
    {
      if (bind (sock, (struct sockaddr *) &bind_addr,
                sizeof (bind_addr)) == -1) {
        LOG_ERROR("Bind failed \"%s\" port %d\n", strerror( errno), port);
#ifdef WIN32
        closesocket( sock);
#else
        close (sock);
#endif
        sock = -1;
      }
      else if (listen (sock, 5) == -1) {
        LOG_ERROR("Listen failed \"%s\"\n", strerror( errno));
#ifdef WIN32
        closesocket( sock);
#else
        close (sock);
#endif
        sock = -1;
      }
    }
  else {
#ifdef WIN32
       char message[30];
       int error = WSAGetLastError();
       sprintf( message, "Error creating socket %d\n", error);
       LOG_ERROR("Error creating socket %d\n", error);
#endif
  }

  return sock;
}




/*
 * Handle GDB stub connections.
 */


/**
 */
INLINE static int
check_breaks_gdb( struct gdb_stub_state *gdb_state,
                  struct breakpoint_gdb *bpoint_list,
                  uint32_t addr,
                  UNUSED_PARM(uint32_t size),
                  enum stop_type stop_type) {
  int found_break = 0;

  if ( gdb_state->active) {
    struct breakpoint_gdb *bpoint = bpoint_list;

    while ( bpoint != NULL && !found_break) {
      if ( addr == bpoint->addr) {
        DEBUG_LOG("Breakpoint hit at %08x\n", addr);

        /* stall the processor */
        gdb_state->cpu_ctrl->stall( gdb_state->cpu_ctrl->data);
		NDS_debug_break();


        /* indicate the break to the GDB stub thread */
        gdb_state->stop_type = stop_type;
        gdb_state->stop_address = addr;
        indicateCPUStop_gdb( gdb_state);
      }
      bpoint = bpoint->next;
    }
  }

  return found_break;
}

static void
WINAPI listenerThread_gdb( void *data) {
  struct gdb_stub_state *state = (struct gdb_stub_state *)data;
  fd_set read_sock_set;
  fd_set main_set;
  int quit = 0;

  FD_ZERO( &main_set);

  FD_SET( state->listen_fd, &main_set);
  FD_SET( state->ctl_pipe[0], &main_set);

  while (!quit) {
    int sel_res;

    read_sock_set = main_set;

    //DEBUG_LOG("Waiting on sockets\n");

    sel_res = select( FD_SETSIZE, &read_sock_set, NULL, NULL, NULL);
    //DEBUG_LOG("sockets ready %d\n", sel_res);

    if ( sel_res > 0) {

      /* Is this a control message */
      if ( FD_ISSET( state->ctl_pipe[0], &read_sock_set)) {
	uint8_t ctl_command;

        //DEBUG_LOG("Control message\n");
#ifdef WIN32
	recv( state->ctl_pipe[0], (char*)&ctl_command, 1, 0);
#else
	read( state->ctl_pipe[0], (char*)&ctl_command, 1);
#endif

	switch ( ctl_command) {

	case CPU_STOPPED_STUB_MESSAGE:
	  if ( state->active &&
		  state->ctl_stub_state != gdb_stub_state::STOPPED_GDB_STATE) {
	    struct debug_out_packet *out_packet = getOutPacket();
	    uint8_t *ptr = out_packet->start_ptr;
            uint32_t send_size;

	    /* mark the stub as stopped and send the stop packet */
			state->ctl_stub_state = gdb_stub_state::STOPPED_GDB_STATE;
	    state->main_stop_flag = 1;

            send_size = make_stop_packet( ptr, state->stop_type, state->stop_address);

	    /*ptr[0] = 'S';
	    ptr[1] = hexchars[state->stop_reason >> 4];
	    ptr[2] = hexchars[state->stop_reason & 0xf];*/

	    putpacket( state->sock_fd, out_packet, send_size);
	    DEBUG_LOG( "\nBreak from Emulation\n");
	  }
	  else {
	    LOG_ERROR( "Asked to stop and already stopped\n");
	  }
	  break;

	default:
	  /* quit out */
	  quit = 1;
	  break;
	}
      }

      else {
        //struct armcpu_t *cpu = twin_states->cpus[i];
        SOCKET_TYPE arm_listener = state->listen_fd;

        /* check for a connection request */
        if ( FD_ISSET( arm_listener, &read_sock_set)) {
          SOCKET_TYPE new_conn;
          struct sockaddr_in gdb_addr;
#ifdef WIN32
          int addr_size = sizeof( gdb_addr);
#else
          socklen_t addr_size = sizeof( gdb_addr);
#endif

          //DEBUG_LOG("listener\n");

          /* accept the connection if we do not already have one */
          new_conn = accept( arm_listener, (struct sockaddr *)&gdb_addr,
                             &addr_size);

          if ( new_conn != -1) {
            int close_sock = 1;

            //DEBUG_LOG("got new socket\n");
            if ( state->sock_fd == -1 && state->active) {
#ifdef WIN32
              BOOL nodelay_opt = 1;
#else
              int nodelay_opt = 1;
#endif
              int set_res;

              //DEBUG_LOG("new connection\n");

              close_sock = 0;
              /* make the socket NODELAY */
#ifdef WIN32
              set_res = setsockopt( new_conn, IPPROTO_TCP, TCP_NODELAY,
                                    (char*)&nodelay_opt, sizeof( nodelay_opt));
#else
              set_res = setsockopt( new_conn, IPPROTO_TCP, TCP_NODELAY,
                                    &nodelay_opt, sizeof( nodelay_opt));
#endif

              if ( set_res != 0) {
                LOG_ERROR( "Failed to set NODELAY socket option \"%s\"\n", strerror( errno));
              }

              FD_SET( new_conn, &main_set);
              state->sock_fd = new_conn;
            }

            if ( close_sock) {
              //DEBUG_LOG("closing new socket\n");
#ifdef WIN32
              closesocket( new_conn);
#else
              close( new_conn);
#endif
            }
	  }
        }

        /* handle the gdb connection */
        if ( state->sock_fd != -1 && state->active) {
          SOCKET_TYPE gdb_sock = state->sock_fd;
          if ( FD_ISSET( gdb_sock, &read_sock_set)) {
            enum read_res_gdb read_res = readPacket_gdb( gdb_sock, &state->rx_packet);

            //DEBUG_LOG("socket read %d\n", read_res);

            switch ( read_res) {
            case READ_NOT_FINISHED:
              /* do nothing here */
              break;

            case READ_SOCKET_ERROR:
              /* close the socket */
#ifdef WIN32
              closesocket( gdb_sock);
#else
              close( gdb_sock);
#endif
              state->sock_fd = -1;
              FD_CLR( gdb_sock, &main_set);
              break;

            case READ_BREAK: {
              /* break the running of the cpu */
				if ( state->ctl_stub_state != gdb_stub_state::STOPPED_GDB_STATE) {
                /* this will cause the emulation to break the execution */
                DEBUG_LOG( "Breaking execution\n");

                /* install the post execution function */
                state->cpu_ctrl->install_post_ex_fn( state->cpu_ctrl->data,
                                                     break_execution,
                                                     state);
              }
              break;
            }

            case READ_COMPLETE: {
              uint8_t reply;
              int write_res;
              int process_packet = 0;
              int close_socket = 0;
              struct packet_reader_gdb *packet = &state->rx_packet;

              if ( state->ctl_stub_state != gdb_stub_state::STOPPED_GDB_STATE) {
                /* not ready to process packet yet, send a bad reply */
                reply = '-';
              }
              else {
                /* send a reply based on the checksum and if okay process the packet */
                if ( packet->read_checksum == packet->checksum) {
                  reply = '+';
                  process_packet = 1;
                }
                else {
                  reply = '-';
                }
              }

              write_res = send( gdb_sock, (char*)&reply, 1, 0);
              
              if ( write_res != 1) {
                close_socket = 1;
              }
              else {
                if ( processPacket_gdb( gdb_sock, state->rx_packet.buffer,
                                        state) == -1) {
                  close_socket = 1;
                }
              }

              if ( close_socket) {
#ifdef WIN32
                closesocket( gdb_sock);
#else
                close( gdb_sock);
#endif
                state->sock_fd = -1;
                FD_CLR( gdb_sock, &main_set);
              }
              break;
            }
            }
	  }
	}
      }
    }
  }


  /* tidy up and leave */
  if ( state->sock_fd != -1) {
#ifdef WIN32
    closesocket( state->sock_fd);
#else
    close( state->sock_fd);
#endif
  }

  /* close the listenering sockets */
#ifdef WIN32
  closesocket( state->listen_fd);
#else
  close( state->listen_fd);
#endif

  return;
}




/*
 *
 * The memory interface
 *
 */
static uint32_t FASTCALL gdb_prefetch32( void *data, uint32_t adr) {
  struct gdb_stub_state *stub = (struct gdb_stub_state *)data;
  int breakpoint;

  breakpoint = check_breaks_gdb( stub, stub->instr_breakpoints, adr, 4,
                                 STOP_BREAKPOINT);

    //return stub->real_cpu_memio->prefetch32( stub->real_cpu_memio->data, adr);
  return 0;
}

static uint16_t FASTCALL gdb_prefetch16( void *data, uint32_t adr) {
  struct gdb_stub_state *stub = (struct gdb_stub_state *)data;
  int breakpoint;

  breakpoint = check_breaks_gdb( stub, stub->instr_breakpoints, adr, 2,
                                 STOP_BREAKPOINT);

    //return stub->real_cpu_memio->prefetch16( stub->real_cpu_memio->data, adr);
  return 0;
}

/** read 8 bit data value */
static uint8_t FASTCALL
gdb_read8( void *data, uint32_t adr) {
  struct gdb_stub_state *stub = (struct gdb_stub_state *)data;
  uint8_t value = 0;
  int breakpoint;

  /* pass down to the real memory interace */
  value = stub->real_cpu_memio->read8( stub->real_cpu_memio->data,
                                       adr);

  breakpoint = check_breaks_gdb( stub, stub->read_breakpoints, adr, 1,
                                 STOP_RWATCHPOINT);
  if ( !breakpoint)
    check_breaks_gdb( stub, stub->access_breakpoints, adr, 1,
                      STOP_AWATCHPOINT);

  return value;
}

/** read 16 bit data value */
static uint16_t FASTCALL
gdb_read16( void *data, uint32_t adr) {
  struct gdb_stub_state *stub = (struct gdb_stub_state *)data;
  uint16_t value;
  int breakpoint;

  /* pass down to the real memory interace */
  value = stub->real_cpu_memio->read16( stub->real_cpu_memio->data,
                                       adr);

  breakpoint = check_breaks_gdb( stub, stub->read_breakpoints, adr, 2,
                                 STOP_RWATCHPOINT);
  if ( !breakpoint)
    check_breaks_gdb( stub, stub->access_breakpoints, adr, 2,
                      STOP_AWATCHPOINT);

  return value;
}
/** read 32 bit data value */
static uint32_t FASTCALL
gdb_read32( void *data, uint32_t adr) {
  struct gdb_stub_state *stub = (struct gdb_stub_state *)data;
  uint32_t value;
  int breakpoint;

  /* pass down to the real memory interace */
  value = stub->real_cpu_memio->read32( stub->real_cpu_memio->data,
                                        adr);

  breakpoint = check_breaks_gdb( stub, stub->read_breakpoints, adr, 4,
                                 STOP_RWATCHPOINT);
  if ( !breakpoint)
    check_breaks_gdb( stub, stub->access_breakpoints, adr, 4,
                      STOP_AWATCHPOINT);

  return value;
}

/** write 8 bit data value */
static void FASTCALL
gdb_write8( void *data, uint32_t adr, uint8_t val) {
  struct gdb_stub_state *stub = (struct gdb_stub_state *)data;
  int breakpoint;

  /* pass down to the real memory interace */
  stub->real_cpu_memio->write8( stub->real_cpu_memio->data,
                                adr, val);

  breakpoint = check_breaks_gdb( stub, stub->write_breakpoints, adr, 1,
                                 STOP_WATCHPOINT);
  if ( !breakpoint)
    check_breaks_gdb( stub, stub->access_breakpoints, adr, 1,
                      STOP_AWATCHPOINT);
}

/** write 16 bit data value */
static void FASTCALL
gdb_write16( void *data, uint32_t adr, uint16_t val) {
  struct gdb_stub_state *stub = (struct gdb_stub_state *)data;
  int breakpoint;

  /* pass down to the real memory interace */
  stub->real_cpu_memio->write16( stub->real_cpu_memio->data,
                                 adr, val);

  breakpoint = check_breaks_gdb( stub, stub->write_breakpoints, adr, 2,
                                 STOP_WATCHPOINT);
  if ( !breakpoint)
    check_breaks_gdb( stub, stub->access_breakpoints, adr, 2,
                      STOP_AWATCHPOINT);
}

/** write 32 bit data value */
static void FASTCALL
gdb_write32( void *data, uint32_t adr, uint32_t val) {
  struct gdb_stub_state *stub = (struct gdb_stub_state *)data;
  int breakpoint;

  /* pass down to the real memory interace */
  stub->real_cpu_memio->write32( stub->real_cpu_memio->data,
                                 adr, val);

  breakpoint = check_breaks_gdb( stub, stub->write_breakpoints, adr, 4,
                                 STOP_WATCHPOINT);
  if ( !breakpoint)
    check_breaks_gdb( stub, stub->access_breakpoints, adr, 4,
                      STOP_AWATCHPOINT);
}







#ifdef WIN32
struct socket_creator_data {
       SOCKET_TYPE *sock;
       int port_num;
};
/**
 */
static DWORD WINAPI
control_creator( LPVOID lpParameter)
{
 struct socket_creator_data *my_data = (struct socket_creator_data *)lpParameter;

 *my_data->sock = socket( PF_INET, SOCK_STREAM, 0);

 if ( *my_data->sock != INVALID_SOCKET) {
    int connect_res;
    struct sockaddr_in clientService; 

    clientService.sin_family = AF_INET;
    clientService.sin_addr.s_addr = inet_addr( "127.0.0.1" );
    clientService.sin_port = htons( my_data->port_num);

    connect_res = connect( *my_data->sock, (SOCKADDR*) &clientService, sizeof(clientService));

    if ( connect_res == SOCKET_ERROR) {
       LOG_ERROR( "Failed to connect to socket\n");
    }
 }

 return 0;
}

#endif



/**
 */
gdbstub_handle_t
createStub_gdb( uint16_t port,
                struct armcpu_memory_iface **cpu_memio,
                struct armcpu_memory_iface *direct_memio) {
  struct gdb_stub_state *stub;
  gdbstub_handle_t handle = NULL;
  int i;
  int res = 0;

  stub = (gdb_stub_state*)malloc( sizeof (struct gdb_stub_state));
  if ( stub == NULL) {
    return NULL;
  }

  stub->active = 0;

  /* keep the memory interfaces */
  stub->real_cpu_memio = *cpu_memio;
  stub->direct_memio = direct_memio;

  *cpu_memio = &stub->cpu_memio;

  /* fill in the memory interface */
  stub->cpu_memio.data = stub;
  stub->cpu_memio.prefetch32 = gdb_prefetch32;
  stub->cpu_memio.prefetch16 = gdb_prefetch16;

  stub->cpu_memio.read8 = gdb_read8;
  stub->cpu_memio.read16 = gdb_read16;
  stub->cpu_memio.read32 = gdb_read32;

  stub->cpu_memio.write8 = gdb_write8;
  stub->cpu_memio.write16 = gdb_write16;
  stub->cpu_memio.write32 = gdb_write32;



  /* put the breakpoint descriptors onto the free list */
  for ( i = 0; i < BREAKPOINT_POOL_SIZE - 1; i++) {
    stub->breakpoint_pool[i].next = &stub->breakpoint_pool[i+1];
  }
  stub->breakpoint_pool[i].next = NULL;
  stub->free_breakpoints = &stub->breakpoint_pool[0];
  stub->instr_breakpoints = NULL;

  stub->read_breakpoints = NULL;
  stub->write_breakpoints = NULL;
  stub->access_breakpoints = NULL;

#ifdef WIN32
  /* initialise the winsock library */
  {
    WORD wVersionRequested;
    WSADATA wsaData;
    int err;

    wVersionRequested = MAKEWORD( 2, 2 );
    
    err = WSAStartup( wVersionRequested, &wsaData );
    if ( err != 0 ) {
      return NULL;
    }
  }

  {
    struct socket_creator_data temp_data = {
      &stub->ctl_pipe[0],
      24689
    };
    SOCKET_TYPE temp_sock = createSocket ( temp_data.port_num);
    HANDLE temp_thread = INVALID_HANDLE_VALUE;
    DWORD temp_threadID;

    res = -1;

    if ( temp_sock != -1) {
      /* create a thread to connect to this socket */
      temp_thread = CreateThread( NULL, 0, control_creator, &temp_data, 0, &temp_threadID);
      if ( temp_thread != INVALID_HANDLE_VALUE) {
        struct sockaddr_in ignore_addr;
        int addr_size = sizeof( ignore_addr);

        stub->ctl_pipe[1] = accept( temp_sock, (struct sockaddr *)&ignore_addr, &addr_size);

        if ( stub->ctl_pipe[1] != INVALID_SOCKET) {
          BOOL nodelay_opt = 1;
          int set_res;

          /* make the socket NODELAY */
          set_res = setsockopt( stub->ctl_pipe[1], IPPROTO_TCP, TCP_NODELAY,
                                (char*)&nodelay_opt, sizeof( nodelay_opt));
          if ( set_res == 0) {
            closesocket( temp_sock);
            res = 0;
          }
        }
      }
    }
  }

  if ( res != 0) {
    LOG_ERROR( "Failed to create control socket\n");
  }
#else
  /* create the control pipe */
  res = pipe( stub->ctl_pipe);

  if ( res != 0) {
    LOG_ERROR( "Failed to create control pipe \"%s\"\n", strerror( errno));
  }
#endif
  else {
    stub->active = 1;
	stub->emu_stub_state = gdb_stub_state::RUNNING_EMU_GDB_STATE;
	stub->ctl_stub_state = gdb_stub_state::STOPPED_GDB_STATE;
    stub->rx_packet.state = IDLE_READ_STATE;

    stub->main_stop_flag = 1;

    stub->port_num = port;
    stub->sock_fd = -1;
    stub->listen_fd = createSocket( port);

    stub->stop_type = STOP_UNKNOWN;

    if ( stub->listen_fd == -1) {
      LOG_ERROR( "Failed to create listening socket \"%s\"\n", strerror( errno));
      res = -1;
    }
  }

  if ( res != -1) {
    /* create the listenering thread */
    stub->thread = createThread_gdb( listenerThread_gdb, stub);

    if ( stub->thread == NULL) {
      LOG_ERROR("Failed to create listener thread\n");
      free( stub);
    }
    else {
      handle = stub;

      DEBUG_LOG("Created stub on port %d\n", port);
    }
  }
  else {
    free( stub);
  }

  return handle;
}


void
destroyStub_gdb( gdbstub_handle_t instance) {
  struct gdb_stub_state *stub = (struct gdb_stub_state *)instance;

  causeQuit_gdb( stub);
  
  joinThread_gdb( stub->thread);

  //stub->cpu_ctl->unstall( stub->cpu_ctl->data);
  //stub->cpu_ctl->remove_post_ex_fn( stub->cpu_ctl->data);

  free( stub);
}

void
activateStub_gdb( gdbstub_handle_t instance,
                  struct armcpu_ctrl_iface *cpu_ctrl) {
  struct gdb_stub_state *stub = (struct gdb_stub_state *)instance;

  stub->cpu_ctrl = cpu_ctrl;

  /* stall the cpu */
  stub->cpu_ctrl->stall( stub->cpu_ctrl->data);

  stub->active = 1;
}
