/*  Copyright (C) 2006 yopyop
    yopyop156@ifrance.com
    yopyop156.ifrance.com

    This file is part of DeSmuME

    DeSmuME is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    DeSmuME is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with DeSmuME; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

#ifndef ARM_CPU
#define ARM_CPU

#include "types.h"
#include "bits.h"
#include "MMU.h"

#ifdef __cplusplus
extern "C" {
#endif

#define ARMCPU_ARM7 1
#define ARMCPU_ARM9 0

#define CODE(i)     (((i)>>25)&0X7)
#define OPCODE(i)   (((i)>>21)&0xF)
#define SIGNEBIT(i) BIT_N(i,20)

#define INSTRUCTION_INDEX(i) ((((i)>>16)&0xFF0)|(((i)>>4)&0xF))

#define ROR(i, j)   ((((u32)(i))>>(j)) | (((u32)(i))<<(32-(j))))

#define UNSIGNED_OVERFLOW(a,b,c) ((BIT31(a)&BIT31(b)) | \
								  ((BIT31(a)|BIT31(b))&BIT31(~c)))

#define UNSIGNED_UNDERFLOW(a,b,c) ((BIT31(~a)&BIT31(b)) | \
								  ((BIT31(~a)|BIT31(b))&BIT31(c)))

#define SIGNED_OVERFLOW(a,b,c) ((BIT31(a)&BIT31(b)&BIT31(~c))|\
								(BIT31(~a)&BIT31(~(b))&BIT31(c)))

#define SIGNED_UNDERFLOW(a,b,c) ((BIT31(a)&BIT31(~(b))&BIT31(~c))|\
								(BIT31(~a)&BIT31(b)&BIT31(c)))

#define EQ	0x0
#define NE	0x1
#define CS	0x2
#define CC	0x3
#define MI	0x4
#define PL	0x5
#define VS	0x6
#define VC	0x7
#define HI	0x8
#define LS	0x9
#define GE	0xA
#define LT	0xB
#define GT	0xC
#define LE	0xD
#define AL	0xE

/*
#define TEST_COND(cond, CPSR)	(((cond)==AL) ||\
					 (((cond)==EQ) && ( CPSR.bits.Z))||\
					 (((cond)==NE) && (!CPSR.bits.Z))||\
					 (((cond)==CS) && ( CPSR.bits.C))||\
					 (((cond)==CC) && (!CPSR.bits.C))||\
					 (((cond)==MI) && ( CPSR.bits.N))||\
					 (((cond)==PL) && (!CPSR.bits.N))||\
					 (((cond)==VS) && ( CPSR.bits.V))||\
					 (((cond)==VC) && (!CPSR.bits.V))||\
					 (((cond)==HI) && (CPSR.bits.C) && (!CPSR.bits.Z))||\
					 (((cond)==LS) && ((CPSR.bits.Z) || (!CPSR.bits.C)))||\
					 (((cond)==GE) && (CPSR.bits.N==CPSR.bits.V))||\
					 (((cond)==LT) && (CPSR.bits.N!=CPSR.bits.V))||\
					 (((cond)==GT) && (CPSR.bits.Z==0) && (CPSR.bits.N==CPSR.bits.V))||\
					 (((cond)==LE) && ((CPSR.bits.Z) || (CPSR.bits.N!=CPSR.bits.V))))
*/

extern const unsigned char arm_cond_table[16*16];

#define TEST_COND(cond, inst, CPSR)   ((arm_cond_table[((CPSR.val >> 24) & 0xf0)+(cond)] >> (inst)) & 1)


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

#ifdef WORDS_BIGENDIAN
typedef union
{
	struct
	{
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
	} bits;
        u32 val;
} Status_Reg;
#else
typedef union
{
	struct
	{
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
	} bits;
        u32 val;
} Status_Reg;
#endif

/**
 * The control interface to a CPU
 */
struct armcpu_ctrl_iface {
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

typedef struct armcpu_t
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

	armcp_t *coproc[16];

        u32 intVector;
        u8 LDTBit;  //1 : ARMv5 style 0 : non ARMv5
	BOOL waitIRQ;
	BOOL wIRQ;
	BOOL wirq;

        u32 (* *swi_tab)(struct armcpu_t * cpu);

#ifdef GDB_STUB
  /** there is a pending irq for the cpu */
  int irq_flag;

  /** the post executed function (if installed) */
  void (*post_ex_fn)( void *, u32 adr, int thumb);

  /** data for the post executed function */
  void *post_ex_fn_data;


  /** flag indicating if the processor is stalled */
  int stalled;

  /** the memory interface */
  struct armcpu_memory_iface *mem_if;

  /** the ctrl interface */
  struct armcpu_ctrl_iface ctrl_iface;
#endif
} armcpu_t;

#ifdef GDB_STUB
int armcpu_new( armcpu_t *armcpu, u32 id, struct armcpu_memory_iface *mem_if,
                struct armcpu_ctrl_iface **ctrl_iface_ret);
#else
int armcpu_new( armcpu_t *armcpu, u32 id);
#endif
void armcpu_init(armcpu_t *armcpu, u32 adr);
u32 armcpu_switchMode(armcpu_t *armcpu, u8 mode);
static u32 armcpu_prefetch(armcpu_t *armcpu);
u32 armcpu_exec(armcpu_t *armcpu);
BOOL armcpu_irqExeption(armcpu_t *armcpu);
//BOOL armcpu_prefetchExeption(armcpu_t *armcpu);
BOOL
armcpu_flagIrq( armcpu_t *armcpu);

extern armcpu_t NDS_ARM7;
extern armcpu_t NDS_ARM9;

static INLINE void NDS_makeARM9Int(u32 num)
{
        /* flag the interrupt request source */
        MMU.reg_IF[0] |= (1<<num);

        /* generate the interrupt if enabled */
	if ((MMU.reg_IE[0] & (1 << num)) && MMU.reg_IME[0])
	{
		NDS_ARM9.wIRQ = TRUE;
		NDS_ARM9.waitIRQ = FALSE;
	}
}

static INLINE void NDS_makeARM7Int(u32 num)
{
        /* flag the interrupt request source */
	MMU.reg_IF[1] |= (1<<num);

        /* generate the interrupt if enabled */
	if ((MMU.reg_IE[1] & (1 << num)) && MMU.reg_IME[1])
	{
		NDS_ARM7.wIRQ = TRUE;
		NDS_ARM7.waitIRQ = FALSE;
	}
}

static INLINE void NDS_makeInt(u8 proc_ID,u32 num)
{
	switch (proc_ID)
	{
		case 0:
			NDS_makeARM9Int(num) ;
			break ;
		case 1:
			NDS_makeARM7Int(num) ;
			break ;
	}
}


#ifdef __cplusplus
}
#endif

#endif
