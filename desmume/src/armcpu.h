/*
	Copyright (C) 2006 yopyop
	Copyright (C) 2006-2021 DeSmuME team

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

#ifndef DESMUME_ARMCPU_H
#define DESMUME_ARMCPU_H

//half of this stuff is arm cpu internals
//half is public API
//really a mess

#include "types.h"
#include "MMU.h"

#define CONDITION(i) ((i)>>28)
#define REG_POS(i,n) (((i)>>n)&0xF)
#define CODE(i) (((i)>>25)&0x7)
#define OPCODE(i) (((i)>>21)&0xF)
#define SIGNEBIT(i) (((i)>>20)&1)

#define EXCEPTION_RESET 0x00
#define EXCEPTION_UNDEFINED_INSTRUCTION 0x04
#define EXCEPTION_SWI 0x08
#define EXCEPTION_PREFETCH_ABORT 0x0C
#define EXCEPTION_DATA_ABORT 0x10
#define EXCEPTION_RESERVED_0x14 0x14
#define EXCEPTION_IRQ 0x18
#define EXCEPTION_FAST_IRQ 0x1C


#define CPU_FREEZE_NONE 0x00
#define CPU_FREEZE_WAIT_IRQ 0x01 //waiting for some IRQ to happen, any IRQ
#define CPU_FREEZE_IE_IF 0x02 //waiting for IE&IF to signal something (probably edge triggered on IRQ too)
#define CPU_FREEZE_IRQ_IE_IF (CPU_FREEZE_WAIT_IRQ|CPU_FREEZE_IE_IF)
#define CPU_FREEZE_OVERCLOCK_HACK 0x04

#define INSTRUCTION_INDEX(i) ((((i)>>16)&0xFF0)|(((i)>>4)&0xF))

inline u32 ROR(u32 i, u32 j)   { return ((((u32)(i))>>(j)) | (((u32)(i))<<(32-(j)))); }

template<typename T>
inline T UNSIGNED_OVERFLOW(T a,T b,T c) { return BIT31(((a)&(b)) | (((a)|(b))&(~c))); }

template<typename T>
inline T UNSIGNED_UNDERFLOW(T a,T b,T c) { return BIT31(((~a)&(b)) | (((~a)|(b))&(c))); }

template<typename T>
inline T SIGNED_OVERFLOW(T a,T b,T c) { return BIT31(((a)&(b)&(~c)) | ((~a)&(~(b))&(c))); }

template<typename T>
inline T SIGNED_UNDERFLOW(T a,T b,T c) { return BIT31(((a)&(~(b))&(~c)) | ((~a)&(b)&(c))); }

#if !defined(bswap32)
#define bswap32(val) \
			( (val << 24) & 0xFF000000) | \
			( (val <<  8) & 0x00FF0000) | \
			( (val >>  8) & 0x0000FF00) | \
			( (val >> 24) & 0x000000FF) 
#endif

#if !defined(bswap64)
#define bswap64(x) \
			( (x << 56) & 0xff00000000000000ULL ) | \
			( (x << 40) & 0x00ff000000000000ULL ) | \
			( (x << 24) & 0x0000ff0000000000ULL ) | \
			( (x <<  8) & 0x000000ff00000000ULL ) | \
			( (x >>  8) & 0x00000000ff000000ULL ) | \
			( (x >> 24) & 0x0000000000ff0000ULL ) | \
			( (x >> 40) & 0x000000000000ff00ULL ) | \
			( (x >> 56) & 0x00000000000000ffULL ) 
#endif

// ============================= CPRS flags funcs
inline bool CarryFrom(s32 left, s32 right)
{
  u32 res  = (0xFFFFFFFFU - (u32)left);

  return ((u32)right > res);
}

inline bool BorrowFrom(s32 left, s32 right)
{
  return ((u32)right > (u32)left);
}

inline bool OverflowFromADD(s32 alu_out, s32 left, s32 right)
{
    return ((left >= 0 && right >= 0) || (left < 0 && right < 0))
			&& ((left < 0 && alu_out >= 0) || (left >= 0 && alu_out < 0));
}

inline bool OverflowFromSUB(s32 alu_out, s32 left, s32 right)
{
    return ((left < 0 && right >= 0) || (left >= 0 && right < 0))
			&& ((left < 0 && alu_out >= 0) || (left >= 0 && alu_out < 0));
}

//zero 15-feb-2009 - these werent getting used and they were getting in my way
//#define EQ	0x0
//#define NE	0x1
//#define CS	0x2
//#define CC	0x3
//#define MI	0x4
//#define PL	0x5
//#define VS	0x6
//#define VC	0x7
//#define HI	0x8
//#define LS	0x9
//#define GE	0xA
//#define LT	0xB
//#define GT	0xC
//#define LE	0xD
//#define AL	0xE

static const u8 arm_cond_table[16*16] = {
    // N=0, Z=0, C=0, V=0
    0x00,0xFF,0x00,0xFF,0x00,0xFF,0x00,0xFF,	// 0x00
    0x00,0xFF,0xFF,0x00,0xFF,0x00,0xFF,0x20,	// 0x00
    // N=0, Z=0, C=0, V=1
    0x00,0xFF,0x00,0xFF,0x00,0xFF,0xFF,0x00,	// 0x10
    0x00,0xFF,0x00,0xFF,0x00,0xFF,0xFF,0x20,
    // N=0, Z=0, C=1, V=0
    0x00,0xFF,0xFF,0x00,0x00,0xFF,0x00,0xFF,	// 0x20
    0xFF,0x00,0xFF,0x00,0xFF,0x00,0xFF,0x20,
    // N=0, Z=0, C=1, V=1
    0x00,0xFF,0xFF,0x00,0x00,0xFF,0xFF,0x00,	// 0x30
    0xFF,0x00,0x00,0xFF,0x00,0xFF,0xFF,0x20,
    // N=0, Z=1, C=0, V=0
    0xFF,0x00,0x00,0xFF,0x00,0xFF,0x00,0xFF,	// 0x40
    0x00,0xFF,0xFF,0x00,0x00,0xFF,0xFF,0x20,
    // N=0, Z=1, C=0, V=1
    0xFF,0x00,0x00,0xFF,0x00,0xFF,0xFF,0x00,	// 0x50
    0x00,0xFF,0x00,0xFF,0x00,0xFF,0xFF,0x20,
    // N=0, Z=1, C=1, V=0
    0xFF,0x00,0xFF,0x00,0x00,0xFF,0x00,0xFF,	// 0x60
    0x00,0xFF,0xFF,0x00,0x00,0xFF,0xFF,0x20,
    // N=0, Z=1, C=1, V=1
    0xFF,0x00,0xFF,0x00,0x00,0xFF,0xFF,0x00,	// 0x70
    0x00,0xFF,0x00,0xFF,0x00,0xFF,0xFF,0x20,
    // N=1, Z=0, C=0, V=0
    0x00,0xFF,0x00,0xFF,0xFF,0x00,0x00,0xFF,	// 0x80
    0x00,0xFF,0x00,0xFF,0x00,0xFF,0xFF,0x20,
    // N=1, Z=0, C=0, V=1
    0x00,0xFF,0x00,0xFF,0xFF,0x00,0xFF,0x00,	// 0x90
    0x00,0xFF,0xFF,0x00,0xFF,0x00,0xFF,0x20,
    // N=1, Z=0, C=1, V=0
    0x00,0xFF,0xFF,0x00,0xFF,0x00,0x00,0xFF,	// 0xA0
    0xFF,0x00,0x00,0xFF,0x00,0xFF,0xFF,0x20,
    // N=1, Z=0, C=1, V=1
    0x00,0xFF,0xFF,0x00,0xFF,0x00,0xFF,0x00,	// 0xB0
    0xFF,0x00,0xFF,0x00,0xFF,0x00,0xFF,0x20,
    // N=1, Z=1, C=0, V=0
    0xFF,0x00,0x00,0xFF,0xFF,0x00,0x00,0xFF,	// 0xC0
    0x00,0xFF,0x00,0xFF,0x00,0xFF,0xFF,0x20,
    // N=1, Z=1, C=0, V=1
    0xFF,0x00,0x00,0xFF,0xFF,0x00,0xFF,0x00,	// 0xD0
    0x00,0xFF,0xFF,0x00,0x00,0xFF,0xFF,0x20,
    // N=1, Z=1, C=1, V=0
    0xFF,0x00,0xFF,0x00,0xFF,0x00,0x00,0xFF,	// 0xE0
    0x00,0xFF,0x00,0xFF,0x00,0xFF,0xFF,0x20,
    // N=1, Z=1, C=1, V=1
    0xFF,0x00,0xFF,0x00,0xFF,0x00,0xFF,0x00,	// 0xF0
    0x00,0xFF,0xFF,0x00,0x00,0xFF,0xFF,0x20
};

#define TEST_COND(cond, inst, CPSR)   ((arm_cond_table[((CPSR.val >> 24) & 0xf0)|(cond)]) & (1 << (inst)))


enum Mode
{
	USR = 0x10,
	FIQ = 0x11,
	IRQ = 0x12,
	SVC = 0x13,
	ABT = 0x17,
	UND = 0x1B,
	SYS = 0x1F
};

typedef union
{
	struct
	{
#ifdef MSB_FIRST
		u32 N : 1,
		Z : 1,
		C : 1,
		V : 1,
		Q : 1,
		RAZ : 19,
		I : 1,
		F : 1,
		T : 1,
      mode : 5;
#else
      u32 mode : 5,
		T : 1,
		F : 1,
		I : 1,
		RAZ : 19,
		Q : 1,
		V : 1,
		C : 1,
		Z : 1,
		N : 1;
#endif
	} bits;
   u32 val;
} Status_Reg;

/**
 * The control interface to a CPU
 */
struct armcpu_ctrl_iface
{
  /** stall the processor */
  void (*stall)( void *instance);

  /** unstall the processor */
  void (*unstall)( void *instance);

  /** read a register value */
  u32 (*read_reg)( void *instance, u32 reg_num);

  /** set a register value */
  void (*set_reg)( void *instance, u32 reg_num, u32 value);

  /** install the post execute function */
  void (*install_post_ex_fn)( void *instance,
                              void (*fn)( void *, u32 adr, int thumb),
                              void *fn_data);

  /** remove the post execute function */
  void (*remove_post_ex_fn)( void *instance);

  /** the private data passed to all interface functions */
  void *data;
};


typedef void* armcp_t;

struct armcpu_t
{
	u32 proc_ID;
	u32 instruction; //4
	u32 instruct_adr; //8
	u32 next_instruction; //12

	u32 R[16]; //16
	Status_Reg CPSR;  //80
	Status_Reg SPSR;

	u32 R13_usr, R14_usr;
	u32 R13_svc, R14_svc;
	u32 R13_abt, R14_abt;
	u32 R13_und, R14_und;
	u32 R13_irq, R14_irq;
	u32 R8_fiq, R9_fiq, R10_fiq, R11_fiq, R12_fiq, R13_fiq, R14_fiq;
	Status_Reg SPSR_svc, SPSR_abt, SPSR_und, SPSR_irq, SPSR_fiq;

	u32 intVector;
	u8 LDTBit;  //1 : ARMv5 style 0 : non ARMv5 (earlier)
	
	u32 freeze;
	//BOOL waitIRQ;
	//BOOL halt_IE_and_IF; 

	u8 intrWaitARM_state;

	BOOL BIOS_loaded;

	u32 (* *swi_tab)();

	// flag indicating if the processor is stalled (for debugging)
	int stalled;

#if defined(_M_X64) || defined(__x86_64__)
	u8 cond_table[16*16];
#endif
	
	/** there is a pending irq for the cpu */
	int irq_flag;
	
	/** the post executed function (if installed) */
	void (*post_ex_fn)( void *, u32 adr, int thumb);
	
	/** data for the post executed function */
	void *post_ex_fn_data;
	
	/** the memory interface */
	armcpu_memory_iface *mem_if;		// This is the memory interface currently in use.
	armcpu_memory_iface base_mem_if;	// This is the CPU's base memory interface.
	
	/** the ctrl interface */
	armcpu_ctrl_iface ctrl_iface;

	// debugging stuff
	u32 runToRetTmp;
	bool runToRet;
	u32 stepOverBreak;
	std::vector<u32> *breakPoints;
	bool debugStep;
};

void armcpu_SetControlInterface(armcpu_t *armcpu, const armcpu_ctrl_iface *theCtrlInterface);
armcpu_ctrl_iface* armcpu_GetControlInterface(armcpu_t *armcpu);
void armcpu_SetControlInterfaceData(armcpu_t *armcpu, void *theData);
void* armcpu_GetControlInterfaceData(const armcpu_t *armcpu);

void armcpu_SetCurrentMemoryInterface(armcpu_t *armcpu, armcpu_memory_iface *theMemInterface);
armcpu_memory_iface* armcpu_GetCurrentMemoryInterface(const armcpu_t *armcpu);
void armcpu_SetCurrentMemoryInterfaceData(armcpu_t *armcpu, void *theData);
void* armcpu_GetCurrentMemoryInterfaceData(const armcpu_t *armcpu);

void armcpu_SetBaseMemoryInterface(armcpu_t *armcpu, const armcpu_memory_iface *theMemInterface);
armcpu_memory_iface* armcpu_GetBaseMemoryInterface(armcpu_t *armcpu);
void armcpu_SetBaseMemoryInterfaceData(armcpu_t *armcpu, void *theData);
void* armcpu_GetBaseMemoryInterfaceData(const armcpu_t *armcpu);

void armcpu_ResetMemoryInterfaceToBase(armcpu_t *armcpu);

void armcpu_changeCPSR();

int armcpu_new( armcpu_t *armcpu, u32 id);
void armcpu_init(armcpu_t *armcpu, u32 adr);
u32 armcpu_switchMode(armcpu_t *armcpu, u8 mode);


BOOL armcpu_irqException(armcpu_t *armcpu);
BOOL armcpu_flagIrq( armcpu_t *armcpu);
void armcpu_exception(armcpu_t *cpu, u32 number);
u32 TRAPUNDEF(armcpu_t* cpu);
u32 armcpu_Wait4IRQ(armcpu_t *cpu);

extern armcpu_t NDS_ARM7;
extern armcpu_t NDS_ARM9;
extern const armcpu_ctrl_iface arm_default_ctrl_iface;

template<int PROCNUM> u32 armcpu_exec();
#ifdef HAVE_JIT
template<int PROCNUM, bool jit> u32 armcpu_exec();
#endif

void setIF(int PROCNUM, u32 flag);

static INLINE void NDS_makeIrq(int PROCNUM, u32 num)
{
	setIF(PROCNUM,1<<num);
}

#endif
