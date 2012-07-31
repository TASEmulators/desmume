/*	Copyright (C) 2006 yopyop
	Copyright (C) 2011 Loren Merritt
	Copyright (C) 2012 DeSmuME team

	This file is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.

	This file is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with the this software.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "types.h"
#ifdef HAVE_JIT
#if !defined(__x86_64__) && !defined(__LP64) && !defined(__IA64__) && !defined(_M_X64) && !defined(_WIN64) && !defined(_M_IX86) && !defined(__INTEL__) && !defined(__i386__)
#error "ERROR: JIT compiler - unsupported target platform"
#endif
#ifdef _WINDOWS
// **** Windows port
#else
#include <sys/mman.h>
#include <errno.h>
#include <unistd.h>
#define HAVE_STATIC_CODE_BUFFER
#endif
#include "instructions.h"
#include "instruction_attributes.h"
#include "Disassembler.h"
#include "MMU.h"
#include "MMU_timing.h"
#include "utils/AsmJit/AsmJit.h"
#include "arm_jit.h"
#include "bios.h"

#define MAX_JIT_BLOCK_SIZE 100
#define LOG_JIT_LEVEL 0

using namespace AsmJit;

#if (LOG_JIT_LEVEL > 0)
#define LOG_JIT 1
#define JIT_COMMENT(...) c.comment(__VA_ARGS__)
#else
#define LOG_JIT 0
#define JIT_COMMENT(...)
#endif

#ifdef MAPPED_JIT_FUNCS
CACHE_ALIGN JIT_struct JIT;

uintptr_t *JIT_struct::JIT_MEM[2][0x4000] = {{0}};

static uintptr_t *JIT_MEM[2][32] = {
	//arm9
	{
		/* 0X*/	DUP2(JIT.ARM9_ITCM),
		/* 1X*/	DUP2(JIT.ARM9_ITCM), // mirror
		/* 2X*/	DUP2(JIT.MAIN_MEM),
		/* 3X*/	DUP2(JIT.SWIRAM),
		/* 4X*/	DUP2(NULL),
		/* 5X*/	DUP2(NULL),
		/* 6X*/		 NULL, 
					 JIT.ARM9_LCDC,	// Plain ARM9-CPU Access (LCDC mode) (max 656KB)
		/* 7X*/	DUP2(NULL),
		/* 8X*/	DUP2(NULL),
		/* 9X*/	DUP2(NULL),
		/* AX*/	DUP2(NULL),
		/* BX*/	DUP2(NULL),
		/* CX*/	DUP2(NULL),
		/* DX*/	DUP2(NULL),
		/* EX*/	DUP2(NULL),
		/* FX*/	DUP2(JIT.ARM9_BIOS)
	},
	//arm7
	{
		/* 0X*/	DUP2(JIT.ARM7_BIOS),
		/* 1X*/	DUP2(NULL),
		/* 2X*/	DUP2(JIT.MAIN_MEM),
		/* 3X*/	     JIT.SWIRAM,
		             JIT.ARM7_ERAM,
		/* 4X*/	     NULL,
		             JIT.ARM7_WIRAM,
		/* 5X*/	DUP2(NULL),
		/* 6X*/		 JIT.ARM7_WRAM,		// VRAM allocated as Work RAM to ARM7 (max. 256K)
					 NULL,
		/* 7X*/	DUP2(NULL),
		/* 8X*/	DUP2(NULL),
		/* 9X*/	DUP2(NULL),
		/* AX*/	DUP2(NULL),
		/* BX*/	DUP2(NULL),
		/* CX*/	DUP2(NULL),
		/* DX*/	DUP2(NULL),
		/* EX*/	DUP2(NULL),
		/* FX*/	DUP2(NULL)
		}
};

static u32 JIT_MASK[2][32] = {
	//arm9
	{
		/* 0X*/	DUP2(0x00007FFF),
		/* 1X*/	DUP2(0x00007FFF),
		/* 2X*/	DUP2(0x003FFFFF), // FIXME _MMU_MAIN_MEM_MASK
		/* 3X*/	DUP2(0x00007FFF),
		/* 4X*/	DUP2(0x00000000),
		/* 5X*/	DUP2(0x00000000),
		/* 6X*/		 0x00000000,
					 0x000FFFFF,
		/* 7X*/	DUP2(0x00000000),
		/* 8X*/	DUP2(0x00000000),
		/* 9X*/	DUP2(0x00000000),
		/* AX*/	DUP2(0x00000000),
		/* BX*/	DUP2(0x00000000),
		/* CX*/	DUP2(0x00000000),
		/* DX*/	DUP2(0x00000000),
		/* EX*/	DUP2(0x00000000),
		/* FX*/	DUP2(0x00007FFF)
	},
	//arm7
	{
		/* 0X*/	DUP2(0x00003FFF),
		/* 1X*/	DUP2(0x00000000),
		/* 2X*/	DUP2(0x003FFFFF),
		/* 3X*/	     0x00007FFF,
		             0x0000FFFF,
		/* 4X*/	     0x00000000,
		             0x0000FFFF,
		/* 5X*/	DUP2(0x00000000),
		/* 6X*/		 0x0003FFFF,
					 0x00000000,
		/* 7X*/	DUP2(0x00000000),
		/* 8X*/	DUP2(0x00000000),
		/* 9X*/	DUP2(0x00000000),
		/* AX*/	DUP2(0x00000000),
		/* BX*/	DUP2(0x00000000),
		/* CX*/	DUP2(0x00000000),
		/* DX*/	DUP2(0x00000000),
		/* EX*/	DUP2(0x00000000),
		/* FX*/	DUP2(0x00000000)
		}
};

static void init_jit_mem()
{
	static bool inited = false;
	if(inited)
		return;
	inited = true;
	for(int proc=0; proc<2; proc++)
		for(int i=0; i<0x4000; i++)
			JIT.JIT_MEM[proc][i] = JIT_MEM[proc][i>>9] + (((i<<14) & JIT_MASK[proc][i>>9]) >> 1);
}

#else
DS_ALIGN(4096) uintptr_t compiled_funcs[1<<26] = {0};
#endif

static u8 recompile_counts[(1<<26)/16];

#ifdef HAVE_STATIC_CODE_BUFFER
// On x86_64, allocate jitted code from a static buffer to ensure that it's within 2GB of .text
// Allows call instructions to use pcrel offsets, as opposed to slower indirect calls.
// Reduces memory needed for function pointers.
// FIXME win64 needs this too, x86_32 doesn't

DS_ALIGN(4096) static u8 scratchpad[1<<25];
static u8 *scratchptr;

struct ASMJIT_API StaticCodeGenerator : public CodeGenerator
{
	StaticCodeGenerator()
	{
		scratchptr = scratchpad;
		int align = (uintptr_t)scratchpad & (sysconf(_SC_PAGESIZE) - 1);
		int err = mprotect(scratchpad-align, sizeof(scratchpad)+align, PROT_READ|PROT_WRITE|PROT_EXEC);
		if(err)
		{
			fprintf(stderr, "mprotect failed: %s\n", strerror(errno));
			abort();
		}
	}

	uint32_t generate(void** dest, Assembler* assembler)
	{
		uintptr_t size = assembler->getCodeSize();
		if(size == 0)
		{
			*dest = NULL;
			return ERROR_NO_FUNCTION;
		}
		if(size > (uintptr_t)(scratchpad+sizeof(scratchpad)-scratchptr))
		{
			fprintf(stderr, "Out of memory for asmjit. Clearing code cache.\n");
			arm_jit_reset(1);
			// If arm_jit_reset didn't involve recompiling op_cmp, we could keep the current function.
			*dest = NULL;
			return ERROR_NONE;
		}
		void *p = scratchptr;
		size = assembler->relocCode(p);
		scratchptr += size;
		*dest = p;
		return ERROR_NONE;
	}
};

static StaticCodeGenerator codegen;
static Compiler c(&codegen);
#else
static Compiler c;
#endif

static void emit_branch(int cond, Label to);
static void _armlog(u8 proc, u32 addr, u32 opcode);

static FileLogger logger(stderr);

static int PROCNUM;
static int *PROCNUM_ptr = &PROCNUM;
static int bb_opcodesize;
static int bb_adr;
static bool bb_thumb;
static GPVar bb_cpu;
static GPVar bb_cycles;
static GPVar total_cycles;

static void *op_cmp[2][2];

#define cpu (&ARMPROC)
#define bb_next_instruction (bb_adr+bb_opcodesize)
#define bb_r15				(bb_adr+2*bb_opcodesize)

#define cpu_ptr(x)			dword_ptr(bb_cpu, offsetof(armcpu_t,x))
#define cpu_ptr_byte(x, y)	byte_ptr(bb_cpu, offsetof(armcpu_t,x)+y)
#define flags_ptr			cpu_ptr_byte(CPSR.val, 3)
#define reg_ptr(x)			dword_ptr(bb_cpu, offsetof(armcpu_t,R)+(4*(x)))
#define reg_pos_ptr(x)		dword_ptr(bb_cpu, offsetof(armcpu_t,R)+(4*REG_POS(i,(x))))
#define reg_pos_ptrL(x)		word_ptr( bb_cpu, offsetof(armcpu_t,R)+(4*REG_POS(i,(x))))
#define reg_pos_ptrH(x)		word_ptr( bb_cpu, offsetof(armcpu_t,R)+(4*REG_POS(i,(x)))+2)
#define reg_pos_ptrB(x)		byte_ptr( bb_cpu, offsetof(armcpu_t,R)+(4*REG_POS(i,(x))))
#define reg_pos_thumb(x)	dword_ptr(bb_cpu, offsetof(armcpu_t,R)+(4*((i>>x)&0x7)))
#define cp15_ptr(x)			dword_ptr(bb_cp15, offsetof(armcp15_t,x))
#define mmu_ptr(x)			dword_ptr(bb_mmu, offsetof(MMU_struct,x))
#define mmu_ptr_byte(x)		byte_ptr(bb_mmu, offsetof(MMU_struct,x))
#define _REG_NUM(i, n)		((i>>n)&0x7)

#ifndef ASMJIT_X64
#define r64 r32
#endif

// sequencer.reschedule = true;
#define changeCPSR { \
			ECall* ctxCPSR = c.call((void*)NDS_Reschedule); \
			ctxCPSR->setPrototype(ASMJIT_CALL_CONV, FunctionBuilder0<void>()); \
}

//-----------------------------------------------------------------------------
//   Shifting macros
//-----------------------------------------------------------------------------
#define SET_NZC { \
		JIT_COMMENT("SET_NZC"); \
		GPVar x = c.newGP(VARIABLE_TYPE_GPD); \
		GPVar y = c.newGP(VARIABLE_TYPE_GPD); \
		c.sets(x.r8Lo()); \
		c.setz(y.r8Lo()); \
		c.lea(x, ptr(y.r64(), x.r64(), TIMES_2)); \
		c.lea(x, ptr(rcf.r64(), x.r64(), TIMES_2)); \
		c.unuse(rcf); \
		c.movzx(y, flags_ptr); \
		c.shl(x, 5); \
		c.and_(y, 0x1F); \
		c.or_(x, y); \
		c.mov(flags_ptr, x.r8Lo()); \
		JIT_COMMENT("end SET_NZC"); }

#define SET_NZ { \
		JIT_COMMENT("SET_NZ"); \
		GPVar x = c.newGP(VARIABLE_TYPE_GPD); \
		GPVar y = c.newGP(VARIABLE_TYPE_GPD); \
		c.sets(x.r8Lo()); \
		c.setz(y.r8Lo()); \
		c.lea(x, ptr(y.r64(), x.r64(), TIMES_2)); \
		c.movzx(y, flags_ptr); \
		c.shl(x, 6); \
		c.and_(y, 0x3F); \
		c.or_(x, y); \
		c.mov(flags_ptr, x.r8Lo()); \
		c.unuse(x); \
		c.unuse(y); \
		JIT_COMMENT("end SET_NZ"); }

#define SET_NZ_W { \
		JIT_COMMENT("SET_NZ_W"); \
		GPVar x = c.newGP(VARIABLE_TYPE_GPD); \
		GPVar y = c.newGP(VARIABLE_TYPE_GPD); \
		c.cmp(hi, lhs); \
		c.setz(y.r8Lo()); \
		c.bt(hi, 31); \
		c.setc(x.r8Lo()); \
		c.lea(x, ptr(y.r64(), x.r64(), TIMES_2)); \
		c.movzx(y, flags_ptr); \
		c.shl(x, 6); \
		c.and_(y, 0x3F); \
		c.or_(x, y); \
		c.mov(flags_ptr, x.r8Lo()); \
		c.unuse(x); \
		c.unuse(y); \
		JIT_COMMENT("end SET_NZ_W"); }

#define SET_Q { \
		JIT_COMMENT("SET_Q"); \
		Label __skipQ = c.newLabel(); \
		c.jno(__skipQ); \
		c.or_(flags_ptr, (1<<3)); \
		c.bind(__skipQ); \
		JIT_COMMENT("end SET_Q"); }

#define S_DST_R15 { \
	JIT_COMMENT("S_DST_R15"); \
	GPVar SPSR = c.newGP(VARIABLE_TYPE_GPD); \
	GPVar tmp = c.newGP(VARIABLE_TYPE_GPD); \
	c.mov(SPSR, cpu_ptr(SPSR.val)); \
	c.mov(tmp, SPSR); \
	c.and_(tmp, 0x1F); \
	ECall* ctx = c.call((void*)armcpu_switchMode); \
	ctx->setPrototype(ASMJIT_CALL_CONV, FunctionBuilder2<Void, void*, u8>()); \
	ctx->setArgument(0, bb_cpu); \
	ctx->setArgument(1, tmp); \
	c.mov(cpu_ptr(CPSR.val), SPSR); \
	c.and_(SPSR, (1<<5)); \
	c.shr(SPSR, 5); \
	c.lea(tmp, ptr_abs((void*)0xFFFFFFFC, SPSR.r64(), TIMES_2)); \
	c.and_(tmp, reg_ptr(15)); \
	c.mov(cpu_ptr(next_instruction), tmp); \
	c.unuse(tmp); \
	JIT_COMMENT("end S_DST_R15"); \
}

// ============================================================================================= IMM
#define LSL_IMM \
	JIT_COMMENT("LSL_IMM"); \
	bool rhs_is_imm = false; \
	u32 imm = ((i>>7)&0x1F); \
    GPVar rhs = c.newGP(VARIABLE_TYPE_GPD); \
	c.mov(rhs, reg_pos_ptr(0)); \
	if(imm) c.shl(rhs, imm); \
	u32 rhs_first = cpu->R[REG_POS(i,0)] << imm;

#define S_LSL_IMM \
	JIT_COMMENT("S_LSL_IMM"); \
	bool rhs_is_imm = false; \
	GPVar rcf = c.newGP(VARIABLE_TYPE_GPD); \
	GPVar rhs = c.newGP(VARIABLE_TYPE_GPD); \
	u32 imm = ((i>>7)&0x1F); \
	c.mov(rhs, reg_pos_ptr(0)); \
	if (imm == 0) \
		c.bt(flags_ptr, 5); \
	else \
		c.shl(rhs, imm); \
	c.setc(rcf.r8Lo());

#define LSR_IMM \
	JIT_COMMENT("LSR_IMM"); \
	bool rhs_is_imm = false; \
	u32 imm = ((i>>7)&0x1F); \
	GPVar rhs = c.newGP(VARIABLE_TYPE_GPD); \
	if(imm) \
	{ \
		c.mov(rhs, reg_pos_ptr(0)); \
		c.shr(rhs, imm); \
	} \
	else \
		c.mov(rhs, 0); \
	u32 rhs_first = imm ? cpu->R[REG_POS(i,0)] >> imm : 0;

#define S_LSR_IMM \
	JIT_COMMENT("S_LSR_IMM"); \
	bool rhs_is_imm = false; \
	GPVar rcf = c.newGP(VARIABLE_TYPE_GPD); \
	GPVar rhs = c.newGP(VARIABLE_TYPE_GPD); \
	u32 imm = ((i>>7)&0x1F); \
	if (!imm) \
	{ \
		c.bt(reg_pos_ptr(0), 31); \
		c.mov(rhs, 0); \
	} \
	else \
	{ \
		c.mov(rhs, reg_pos_ptr(0)); \
		c.shr(rhs, imm); \
	} \
	c.setc(rcf.r8Lo());

#define ASR_IMM \
	JIT_COMMENT("ASR_IMM"); \
	bool rhs_is_imm = false; \
	u32 imm = ((i>>7)&0x1F); \
	GPVar rhs = c.newGP(VARIABLE_TYPE_GPD); \
	c.mov(rhs, reg_pos_ptr(0)); \
	if(!imm) imm = 31; \
	c.sar(rhs, imm); \
	u32 rhs_first = (s32)cpu->R[REG_POS(i,0)] >> imm;

#define S_ASR_IMM \
	JIT_COMMENT("S_ASR_IMM"); \
	bool rhs_is_imm = false; \
	GPVar rcf = c.newGP(VARIABLE_TYPE_GPD); \
	GPVar rhs = c.newGP(VARIABLE_TYPE_GPD); \
	u32 imm = ((i>>7)&0x1F); \
	c.mov(rhs, reg_pos_ptr(0)); \
	if (!imm) \
	{ \
		c.sar(rhs, 31); \
		c.setnz(rcf.r8Lo()); \
	} \
	else \
	{ \
		c.sar(rhs, imm); \
		c.setc(rcf.r8Lo()); \
	}

#define ROR_IMM \
	JIT_COMMENT("ROR_IMM"); \
	bool rhs_is_imm = false; \
	u32 imm = ((i>>7)&0x1F); \
	GPVar rhs = c.newGP(VARIABLE_TYPE_GPD); \
	c.mov(rhs, reg_pos_ptr(0)); \
	if (!imm) \
	{ \
		c.bt(flags_ptr, 5); \
		c.rcr(rhs, 1); \
	} \
	else \
		c.ror(rhs, imm); \
	u32 rhs_first = ROR(cpu->R[REG_POS(i,0)], imm);	/* TODO */

#define S_ROR_IMM \
	JIT_COMMENT("S_ROR_IMM"); \
	bool rhs_is_imm = false; \
	GPVar rcf = c.newGP(VARIABLE_TYPE_GPD); \
	GPVar rhs = c.newGP(VARIABLE_TYPE_GPD); \
	u32 imm = ((i>>7)&0x1F); \
	c.mov(rhs, reg_pos_ptr(0)); \
	if (!imm) \
	{ \
		c.bt(flags_ptr, 5); \
		c.rcr(rhs, 1); \
	} \
	else \
		c.ror(rhs, imm); \
	c.setc(rcf.r8Lo());

#define REG_OFF \
	JIT_COMMENT("REG_OFF"); \
	bool rhs_is_imm = false; \
	Mem rhs = reg_pos_ptr(0); \
	u32 rhs_first = cpu->R[REG_POS(i,0)];

#define IMM_VAL \
	JIT_COMMENT("IMM_VAL"); \
	bool rhs_is_imm = true; \
	u32 rhs = ROR((i&0xFF), (i>>7)&0x1E); \
	u32 rhs_first = rhs;

#define S_IMM_VAL \
	JIT_COMMENT("S_IMM_VAL"); \
	bool rhs_is_imm = true; \
	GPVar rcf = c.newGP(VARIABLE_TYPE_GPD); \
	u32 rhs = ROR((i&0xFF), (i>>7)&0x1E); \
	if ((i>>8)&0xF) c.mov(rcf, BIT31(rhs)); \
	else \
	{ \
		c.bt(flags_ptr, 5); \
		c.setc(rcf.r8Lo()); \
	} \
	u32 rhs_first = rhs;

#define IMM_VALUE \
	JIT_COMMENT("IMM_VALUE"); \
	bool rhs_is_imm = true; \
	u32 rhs = ROR((i&0xFF), (i>>7)&0x1E); \
	u32 rhs_first = rhs;

#define IMM_OFF \
	JIT_COMMENT("IMM_OFF"); \
	bool rhs_is_imm = true; \
	u32 rhs = ((i>>4)&0xF0)+(i&0xF); \
	u32 rhs_first = rhs;

#define IMM_OFF_12 \
	JIT_COMMENT("IMM_OFF_12"); \
	bool rhs_is_imm = true; \
	u32 rhs = i&0xFFF; \
	u32 rhs_first = rhs;

// ============================================================================================= REG
#define LSX_REG(name, x86inst, sign) \
	JIT_COMMENT(#name); \
	bool rhs_is_imm = false; \
	GPVar imm = c.newGP(VARIABLE_TYPE_GPN); \
	GPVar rhs = c.newGP(VARIABLE_TYPE_GPD); \
	Label __lt32 = c.newLabel(); \
	c.movzx(imm, reg_pos_ptrB(8)); \
	c.mov(rhs, reg_pos_ptr(0)); \
	c.cmp(imm, 32); \
	c.jl(__lt32); \
	if(sign) c.mov(imm, 31); \
	else c.xor_(rhs, rhs); \
	c.bind(__lt32); \
	c.x86inst(rhs, imm);

#define S_LSX_REG(name, x86inst, sign) \
	JIT_COMMENT(#name); \
	bool rhs_is_imm = false; \
	GPVar rcf = c.newGP(VARIABLE_TYPE_GPD); \
	GPVar imm = c.newGP(VARIABLE_TYPE_GPN); \
	GPVar rhs = c.newGP(VARIABLE_TYPE_GPD); \
	Label __zero = c.newLabel(); \
	Label __lt32 = c.newLabel(); \
	Label __done = c.newLabel(); \
	c.mov(imm, reg_pos_ptr(8)); \
	c.mov(rhs, reg_pos_ptr(0)); \
	c.and_(imm, 0xFF); \
	c.jz(__zero); \
	c.cmp(imm, 32); \
	c.jl(__lt32); \
	if(!sign) \
	{ \
		Label __eq32 = c.newLabel(); \
		c.je(__eq32); \
		/* imm > 32 */ \
		c.xor_(rhs, rhs); \
		/* imm == 32 */ \
		c.bind(__eq32); \
	} \
	c.x86inst(rhs, 31); \
	c.x86inst(rhs, 1); \
	c.jmp(__done); \
	/* imm == 0 */ \
	c.bind(__zero); \
	c.bt(flags_ptr, 5); \
	c.jmp(__done); \
	/* imm < 32 */ \
	c.bind(__lt32); \
	c.x86inst(rhs, imm); \
	/* done */\
	c.bind(__done); \
	c.setc(rcf.r8Lo());

#define LSL_REG LSX_REG(LSL_REG, shl, 0)
#define LSR_REG LSX_REG(LSR_REG, shr, 0)
#define ASR_REG LSX_REG(ASR_REG, sar, 1)
#define S_LSL_REG S_LSX_REG(S_LSL_REG, shl, 0)
#define S_LSR_REG S_LSX_REG(S_LSR_REG, shr, 0)
#define S_ASR_REG S_LSX_REG(S_ASR_REG, sar, 1)

#define ROR_REG \
	JIT_COMMENT("ROR_REG"); \
	bool rhs_is_imm = false; \
	GPVar imm = c.newGP(VARIABLE_TYPE_GPN); \
	GPVar rhs = c.newGP(VARIABLE_TYPE_GPD); \
	c.mov(rhs, reg_pos_ptr(0)); \
	c.movzx(imm, reg_pos_ptrB(8)); \
	c.ror(rhs, imm); \

#define S_ROR_REG \
	JIT_COMMENT("S_ROR_REG"); \
	bool rhs_is_imm = false; \
	GPVar rcf = c.newGP(VARIABLE_TYPE_GPD); \
	GPVar imm = c.newGP(VARIABLE_TYPE_GPN); \
	GPVar rhs = c.newGP(VARIABLE_TYPE_GPD); \
	Label __zero = c.newLabel(); \
	Label __zero2 = c.newLabel(); \
	Label __done = c.newLabel(); \
	c.movzx(imm, reg_pos_ptrB(8)); \
	c.mov(rhs, reg_pos_ptr(0)); \
	c.test(imm, imm); \
	c.jz(__zero);\
	c.and_(imm, 0x1F); \
	c.jz(__zero2);\
	/* imm&0x1F != 0 */ \
	c.ror(rhs, imm); \
	c.jmp(__done); \
	/* imm&0x1F == 0 */ \
	c.bind(__zero2); \
	c.bt(rhs, 31); \
	c.jmp(__done); \
	/* imm == 0 */ \
	c.bind(__zero); \
	c.bt(flags_ptr, 5); \
	/* done */ \
	c.bind(__done); \
	c.setc(rcf.r8Lo());

//==================================================================== common funcs
static void emit_MMU_aluMemCycles(int alu_cycles, GPVar mem_cycles, int population)
{
	if(PROCNUM==ARMCPU_ARM9)
	{
		if(population < alu_cycles)
		{
			GPVar x = c.newGP(VARIABLE_TYPE_GPD);
			c.mov(x, alu_cycles);
			c.cmp(mem_cycles, alu_cycles);
			c.cmovl(mem_cycles, x);
		}
	}
	else
		c.add(mem_cycles, alu_cycles);
}

//-----------------------------------------------------------------------------
//   OPs
//-----------------------------------------------------------------------------
#define OP_ARITHMETIC(arg, x86inst, symmetric, flags) \
    arg; \
	GPVar lhs = c.newGP(VARIABLE_TYPE_GPD); \
	if(REG_POS(i,12) == REG_POS(i,16)) \
		c.x86inst(reg_pos_ptr(12), rhs); \
	else if(symmetric && !rhs_is_imm) \
	{ \
		c.x86inst(*(GPVar*)&rhs, reg_pos_ptr(16)); \
		c.mov(reg_pos_ptr(12), rhs); \
	} \
	else \
	{ \
		c.mov(lhs, reg_pos_ptr(16)); \
		c.x86inst(lhs, rhs); \
		c.mov(reg_pos_ptr(12), lhs); \
	} \
	if(flags) \
	{ \
		if(REG_POS(i,12)==15) \
		{ \
			S_DST_R15; \
			c.add(total_cycles, 2); \
			return 1; \
		} \
		c.call(op_cmp[PROCNUM][!symmetric])->setPrototype(ASMJIT_CALL_CONV, FunctionBuilder0<Void>()); \
	} \
	else \
	{ \
		if(REG_POS(i,12)==15) \
		{ \
			GPVar tmp = c.newGP(VARIABLE_TYPE_GPD); \
			c.mov(tmp, reg_ptr(15)); \
			c.mov(cpu_ptr(next_instruction), tmp); \
			c.add(total_cycles, 2); \
		} \
	} \
	return 1;

#define OP_ARITHMETIC_R(arg, x86inst, symmetric, flags) \
    arg; \
	if(symmetric && !rhs_is_imm) \
	{ \
		c.x86inst(*(GPVar*)&rhs, reg_pos_ptr(16)); \
		c.mov(reg_pos_ptr(12), rhs); \
	} \
	else \
	{ \
		GPVar lhs = c.newGP(VARIABLE_TYPE_GPD); \
		c.mov(lhs, rhs); \
		c.x86inst(lhs, reg_pos_ptr(16)); \
		c.mov(reg_pos_ptr(12), lhs); \
	} \
	if(flags) \
	{ \
		if(REG_POS(i,12)==15) \
		{ \
			S_DST_R15; \
			c.add(total_cycles, 2); \
			return 1; \
		} \
		c.call(op_cmp[PROCNUM][!symmetric])->setPrototype(ASMJIT_CALL_CONV, FunctionBuilder0<Void>()); \
	} \
	else \
	{ \
		if(REG_POS(i,12)==15) \
		{ \
			GPVar tmp = c.newGP(VARIABLE_TYPE_GPD); \
			c.mov(tmp, reg_ptr(15)); \
			c.mov(cpu_ptr(next_instruction), tmp); \
			c.add(total_cycles, 2); \
		} \
	} \
	return 1;

#define OP_ARITHMETIC_S(arg, x86inst, symmetric) \
    arg; \
	if(symmetric && !rhs_is_imm) \
	{ \
		c.x86inst(*(GPVar*)&rhs, reg_pos_ptr(16)); \
		c.mov(reg_pos_ptr(12), rhs); \
	} \
	else \
	{ \
		GPVar lhs = c.newGP(VARIABLE_TYPE_GPD); \
		c.mov(lhs, reg_pos_ptr(16)); \
		c.x86inst(lhs, rhs); \
		c.mov(reg_pos_ptr(12), lhs); \
	} \
	if(REG_POS(i,12)==15) \
	{ \
		S_DST_R15; \
		c.add(total_cycles, 2); \
		return 1; \
	} \
	SET_NZC; \
	return 1;

#define GET_CARRY(invert) { \
	c.bt(flags_ptr, 5); \
	if (invert) c.cmc(); }

static int OP_AND_LSL_IMM(const u32 i) { OP_ARITHMETIC(LSL_IMM, and_, 1, 0); }
static int OP_AND_LSL_REG(const u32 i) { OP_ARITHMETIC(LSL_REG, and_, 1, 0); }
static int OP_AND_LSR_IMM(const u32 i) { OP_ARITHMETIC(LSR_IMM, and_, 1, 0); }
static int OP_AND_LSR_REG(const u32 i) { OP_ARITHMETIC(LSR_REG, and_, 1, 0); }
static int OP_AND_ASR_IMM(const u32 i) { OP_ARITHMETIC(ASR_IMM, and_, 1, 0); }
static int OP_AND_ASR_REG(const u32 i) { OP_ARITHMETIC(ASR_REG, and_, 1, 0); }
static int OP_AND_ROR_IMM(const u32 i) { OP_ARITHMETIC(ROR_IMM, and_, 1, 0); }
static int OP_AND_ROR_REG(const u32 i) { OP_ARITHMETIC(ROR_REG, and_, 1, 0); }
static int OP_AND_IMM_VAL(const u32 i) { OP_ARITHMETIC(IMM_VAL, and_, 1, 0); }

static int OP_EOR_LSL_IMM(const u32 i) { OP_ARITHMETIC(LSL_IMM, xor_, 1, 0); }
static int OP_EOR_LSL_REG(const u32 i) { OP_ARITHMETIC(LSL_REG, xor_, 1, 0); }
static int OP_EOR_LSR_IMM(const u32 i) { OP_ARITHMETIC(LSR_IMM, xor_, 1, 0); }
static int OP_EOR_LSR_REG(const u32 i) { OP_ARITHMETIC(LSR_REG, xor_, 1, 0); }
static int OP_EOR_ASR_IMM(const u32 i) { OP_ARITHMETIC(ASR_IMM, xor_, 1, 0); }
static int OP_EOR_ASR_REG(const u32 i) { OP_ARITHMETIC(ASR_REG, xor_, 1, 0); }
static int OP_EOR_ROR_IMM(const u32 i) { OP_ARITHMETIC(ROR_IMM, xor_, 1, 0); }
static int OP_EOR_ROR_REG(const u32 i) { OP_ARITHMETIC(ROR_REG, xor_, 1, 0); }
static int OP_EOR_IMM_VAL(const u32 i) { OP_ARITHMETIC(IMM_VAL, xor_, 1, 0); }

static int OP_ORR_LSL_IMM(const u32 i) { OP_ARITHMETIC(LSL_IMM, or_, 1, 0); }
static int OP_ORR_LSL_REG(const u32 i) { OP_ARITHMETIC(LSL_REG, or_, 1, 0); }
static int OP_ORR_LSR_IMM(const u32 i) { OP_ARITHMETIC(LSR_IMM, or_, 1, 0); }
static int OP_ORR_LSR_REG(const u32 i) { OP_ARITHMETIC(LSR_REG, or_, 1, 0); }
static int OP_ORR_ASR_IMM(const u32 i) { OP_ARITHMETIC(ASR_IMM, or_, 1, 0); }
static int OP_ORR_ASR_REG(const u32 i) { OP_ARITHMETIC(ASR_REG, or_, 1, 0); }
static int OP_ORR_ROR_IMM(const u32 i) { OP_ARITHMETIC(ROR_IMM, or_, 1, 0); }
static int OP_ORR_ROR_REG(const u32 i) { OP_ARITHMETIC(ROR_REG, or_, 1, 0); }
static int OP_ORR_IMM_VAL(const u32 i) { OP_ARITHMETIC(IMM_VAL, or_, 1, 0); }

static int OP_ADD_LSL_IMM(const u32 i) { OP_ARITHMETIC(LSL_IMM, add, 1, 0); }
static int OP_ADD_LSL_REG(const u32 i) { OP_ARITHMETIC(LSL_REG, add, 1, 0); }
static int OP_ADD_LSR_IMM(const u32 i) { OP_ARITHMETIC(LSR_IMM, add, 1, 0); }
static int OP_ADD_LSR_REG(const u32 i) { OP_ARITHMETIC(LSR_REG, add, 1, 0); }
static int OP_ADD_ASR_IMM(const u32 i) { OP_ARITHMETIC(ASR_IMM, add, 1, 0); }
static int OP_ADD_ASR_REG(const u32 i) { OP_ARITHMETIC(ASR_REG, add, 1, 0); }
static int OP_ADD_ROR_IMM(const u32 i) { OP_ARITHMETIC(ROR_IMM, add, 1, 0); }
static int OP_ADD_ROR_REG(const u32 i) { OP_ARITHMETIC(ROR_REG, add, 1, 0); }
static int OP_ADD_IMM_VAL(const u32 i) { OP_ARITHMETIC(IMM_VAL, add, 1, 0); }

static int OP_SUB_LSL_IMM(const u32 i) { OP_ARITHMETIC(LSL_IMM, sub, 0, 0); }
static int OP_SUB_LSL_REG(const u32 i) { OP_ARITHMETIC(LSL_REG, sub, 0, 0); }
static int OP_SUB_LSR_IMM(const u32 i) { OP_ARITHMETIC(LSR_IMM, sub, 0, 0); }
static int OP_SUB_LSR_REG(const u32 i) { OP_ARITHMETIC(LSR_REG, sub, 0, 0); }
static int OP_SUB_ASR_IMM(const u32 i) { OP_ARITHMETIC(ASR_IMM, sub, 0, 0); }
static int OP_SUB_ASR_REG(const u32 i) { OP_ARITHMETIC(ASR_REG, sub, 0, 0); }
static int OP_SUB_ROR_IMM(const u32 i) { OP_ARITHMETIC(ROR_IMM, sub, 0, 0); }
static int OP_SUB_ROR_REG(const u32 i) { OP_ARITHMETIC(ROR_REG, sub, 0, 0); }
static int OP_SUB_IMM_VAL(const u32 i) { OP_ARITHMETIC(IMM_VAL, sub, 0, 0); }

static int OP_RSB_LSL_IMM(const u32 i) { OP_ARITHMETIC_R(LSL_IMM, sub, 0, 0); }
static int OP_RSB_LSL_REG(const u32 i) { OP_ARITHMETIC_R(LSL_REG, sub, 0, 0); }
static int OP_RSB_LSR_IMM(const u32 i) { OP_ARITHMETIC_R(LSR_IMM, sub, 0, 0); }
static int OP_RSB_LSR_REG(const u32 i) { OP_ARITHMETIC_R(LSR_REG, sub, 0, 0); }
static int OP_RSB_ASR_IMM(const u32 i) { OP_ARITHMETIC_R(ASR_IMM, sub, 0, 0); }
static int OP_RSB_ASR_REG(const u32 i) { OP_ARITHMETIC_R(ASR_REG, sub, 0, 0); }
static int OP_RSB_ROR_IMM(const u32 i) { OP_ARITHMETIC_R(ROR_IMM, sub, 0, 0); }
static int OP_RSB_ROR_REG(const u32 i) { OP_ARITHMETIC_R(ROR_REG, sub, 0, 0); }
static int OP_RSB_IMM_VAL(const u32 i) { OP_ARITHMETIC_R(IMM_VAL, sub, 0, 0); }

// ================================ S instructions
static int OP_AND_S_LSL_IMM(const u32 i) { OP_ARITHMETIC_S(S_LSL_IMM, and_, 1); }
static int OP_AND_S_LSL_REG(const u32 i) { OP_ARITHMETIC_S(S_LSL_REG, and_, 1); }
static int OP_AND_S_LSR_IMM(const u32 i) { OP_ARITHMETIC_S(S_LSR_IMM, and_, 1); }
static int OP_AND_S_LSR_REG(const u32 i) { OP_ARITHMETIC_S(S_LSR_REG, and_, 1); }
static int OP_AND_S_ASR_IMM(const u32 i) { OP_ARITHMETIC_S(S_ASR_IMM, and_, 1); }
static int OP_AND_S_ASR_REG(const u32 i) { OP_ARITHMETIC_S(S_ASR_REG, and_, 1); }
static int OP_AND_S_ROR_IMM(const u32 i) { OP_ARITHMETIC_S(S_ROR_IMM, and_, 1); }
static int OP_AND_S_ROR_REG(const u32 i) { OP_ARITHMETIC_S(S_ROR_REG, and_, 1); }
static int OP_AND_S_IMM_VAL(const u32 i) { OP_ARITHMETIC_S(S_IMM_VAL, and_, 1); }

static int OP_EOR_S_LSL_IMM(const u32 i) { OP_ARITHMETIC_S(S_LSL_IMM, xor_, 1); }
static int OP_EOR_S_LSL_REG(const u32 i) { OP_ARITHMETIC_S(S_LSL_REG, xor_, 1); }
static int OP_EOR_S_LSR_IMM(const u32 i) { OP_ARITHMETIC_S(S_LSR_IMM, xor_, 1); }
static int OP_EOR_S_LSR_REG(const u32 i) { OP_ARITHMETIC_S(S_LSR_REG, xor_, 1); }
static int OP_EOR_S_ASR_IMM(const u32 i) { OP_ARITHMETIC_S(S_ASR_IMM, xor_, 1); }
static int OP_EOR_S_ASR_REG(const u32 i) { OP_ARITHMETIC_S(S_ASR_REG, xor_, 1); }
static int OP_EOR_S_ROR_IMM(const u32 i) { OP_ARITHMETIC_S(S_ROR_IMM, xor_, 1); }
static int OP_EOR_S_ROR_REG(const u32 i) { OP_ARITHMETIC_S(S_ROR_REG, xor_, 1); }
static int OP_EOR_S_IMM_VAL(const u32 i) { OP_ARITHMETIC_S(S_IMM_VAL, xor_, 1); }

static int OP_ORR_S_LSL_IMM(const u32 i) { OP_ARITHMETIC_S(S_LSL_IMM, or_, 1); }
static int OP_ORR_S_LSL_REG(const u32 i) { OP_ARITHMETIC_S(S_LSL_REG, or_, 1); }
static int OP_ORR_S_LSR_IMM(const u32 i) { OP_ARITHMETIC_S(S_LSR_IMM, or_, 1); }
static int OP_ORR_S_LSR_REG(const u32 i) { OP_ARITHMETIC_S(S_LSR_REG, or_, 1); }
static int OP_ORR_S_ASR_IMM(const u32 i) { OP_ARITHMETIC_S(S_ASR_IMM, or_, 1); }
static int OP_ORR_S_ASR_REG(const u32 i) { OP_ARITHMETIC_S(S_ASR_REG, or_, 1); }
static int OP_ORR_S_ROR_IMM(const u32 i) { OP_ARITHMETIC_S(S_ROR_IMM, or_, 1); }
static int OP_ORR_S_ROR_REG(const u32 i) { OP_ARITHMETIC_S(S_ROR_REG, or_, 1); }
static int OP_ORR_S_IMM_VAL(const u32 i) { OP_ARITHMETIC_S(S_IMM_VAL, or_, 1); }

static int OP_ADD_S_LSL_IMM(const u32 i) { OP_ARITHMETIC(LSL_IMM, add, 1, 1); }
static int OP_ADD_S_LSL_REG(const u32 i) { OP_ARITHMETIC(LSL_REG, add, 1, 1); }
static int OP_ADD_S_LSR_IMM(const u32 i) { OP_ARITHMETIC(LSR_IMM, add, 1, 1); }
static int OP_ADD_S_LSR_REG(const u32 i) { OP_ARITHMETIC(LSR_REG, add, 1, 1); }
static int OP_ADD_S_ASR_IMM(const u32 i) { OP_ARITHMETIC(ASR_IMM, add, 1, 1); }
static int OP_ADD_S_ASR_REG(const u32 i) { OP_ARITHMETIC(ASR_REG, add, 1, 1); }
static int OP_ADD_S_ROR_IMM(const u32 i) { OP_ARITHMETIC(ROR_IMM, add, 1, 1); }
static int OP_ADD_S_ROR_REG(const u32 i) { OP_ARITHMETIC(ROR_REG, add, 1, 1); }
static int OP_ADD_S_IMM_VAL(const u32 i) { OP_ARITHMETIC(IMM_VAL, add, 1, 1); }

static int OP_SUB_S_LSL_IMM(const u32 i) { OP_ARITHMETIC(LSL_IMM, sub, 0, 1); }
static int OP_SUB_S_LSL_REG(const u32 i) { OP_ARITHMETIC(LSL_REG, sub, 0, 1); }
static int OP_SUB_S_LSR_IMM(const u32 i) { OP_ARITHMETIC(LSR_IMM, sub, 0, 1); }
static int OP_SUB_S_LSR_REG(const u32 i) { OP_ARITHMETIC(LSR_REG, sub, 0, 1); }
static int OP_SUB_S_ASR_IMM(const u32 i) { OP_ARITHMETIC(ASR_IMM, sub, 0, 1); }
static int OP_SUB_S_ASR_REG(const u32 i) { OP_ARITHMETIC(ASR_REG, sub, 0, 1); }
static int OP_SUB_S_ROR_IMM(const u32 i) { OP_ARITHMETIC(ROR_IMM, sub, 0, 1); }
static int OP_SUB_S_ROR_REG(const u32 i) { OP_ARITHMETIC(ROR_REG, sub, 0, 1); }
static int OP_SUB_S_IMM_VAL(const u32 i) { OP_ARITHMETIC(IMM_VAL, sub, 0, 1); }

static int OP_RSB_S_LSL_IMM(const u32 i) { OP_ARITHMETIC_R(LSL_IMM, sub, 0, 1); }
static int OP_RSB_S_LSL_REG(const u32 i) { OP_ARITHMETIC_R(LSL_REG, sub, 0, 1); }
static int OP_RSB_S_LSR_IMM(const u32 i) { OP_ARITHMETIC_R(LSR_IMM, sub, 0, 1); }
static int OP_RSB_S_LSR_REG(const u32 i) { OP_ARITHMETIC_R(LSR_REG, sub, 0, 1); }
static int OP_RSB_S_ASR_IMM(const u32 i) { OP_ARITHMETIC_R(ASR_IMM, sub, 0, 1); }
static int OP_RSB_S_ASR_REG(const u32 i) { OP_ARITHMETIC_R(ASR_REG, sub, 0, 1); }
static int OP_RSB_S_ROR_IMM(const u32 i) { OP_ARITHMETIC_R(ROR_IMM, sub, 0, 1); }
static int OP_RSB_S_ROR_REG(const u32 i) { OP_ARITHMETIC_R(ROR_REG, sub, 0, 1); }
static int OP_RSB_S_IMM_VAL(const u32 i) { OP_ARITHMETIC_R(IMM_VAL, sub, 0, 1); }

static int OP_ADC_LSL_IMM(const u32 i) { OP_ARITHMETIC(LSL_IMM; GET_CARRY(0), adc, 1, 0); }
static int OP_ADC_LSL_REG(const u32 i) { OP_ARITHMETIC(LSL_REG; GET_CARRY(0), adc, 1, 0); }
static int OP_ADC_LSR_IMM(const u32 i) { OP_ARITHMETIC(LSR_IMM; GET_CARRY(0), adc, 1, 0); }
static int OP_ADC_LSR_REG(const u32 i) { OP_ARITHMETIC(LSR_REG; GET_CARRY(0), adc, 1, 0); }
static int OP_ADC_ASR_IMM(const u32 i) { OP_ARITHMETIC(ASR_IMM; GET_CARRY(0), adc, 1, 0); }
static int OP_ADC_ASR_REG(const u32 i) { OP_ARITHMETIC(ASR_REG; GET_CARRY(0), adc, 1, 0); }
static int OP_ADC_ROR_IMM(const u32 i) { OP_ARITHMETIC(ROR_IMM; GET_CARRY(0), adc, 1, 0); }
static int OP_ADC_ROR_REG(const u32 i) { OP_ARITHMETIC(ROR_REG; GET_CARRY(0), adc, 1, 0); }
static int OP_ADC_IMM_VAL(const u32 i) { OP_ARITHMETIC(IMM_VAL; GET_CARRY(0), adc, 1, 0); }

static int OP_ADC_S_LSL_IMM(const u32 i) { OP_ARITHMETIC(LSL_IMM; GET_CARRY(0), adc, 1, 1); }
static int OP_ADC_S_LSL_REG(const u32 i) { OP_ARITHMETIC(LSL_REG; GET_CARRY(0), adc, 1, 1); }
static int OP_ADC_S_LSR_IMM(const u32 i) { OP_ARITHMETIC(LSR_IMM; GET_CARRY(0), adc, 1, 1); }
static int OP_ADC_S_LSR_REG(const u32 i) { OP_ARITHMETIC(LSR_REG; GET_CARRY(0), adc, 1, 1); }
static int OP_ADC_S_ASR_IMM(const u32 i) { OP_ARITHMETIC(ASR_IMM; GET_CARRY(0), adc, 1, 1); }
static int OP_ADC_S_ASR_REG(const u32 i) { OP_ARITHMETIC(ASR_REG; GET_CARRY(0), adc, 1, 1); }
static int OP_ADC_S_ROR_IMM(const u32 i) { OP_ARITHMETIC(ROR_IMM; GET_CARRY(0), adc, 1, 1); }
static int OP_ADC_S_ROR_REG(const u32 i) { OP_ARITHMETIC(ROR_REG; GET_CARRY(0), adc, 1, 1); }
static int OP_ADC_S_IMM_VAL(const u32 i) { OP_ARITHMETIC(IMM_VAL; GET_CARRY(0), adc, 1, 1); }

static int OP_SBC_LSL_IMM(const u32 i) { OP_ARITHMETIC(LSL_IMM; GET_CARRY(1), sbb, 0, 0); }
static int OP_SBC_LSL_REG(const u32 i) { OP_ARITHMETIC(LSL_REG; GET_CARRY(1), sbb, 0, 0); }
static int OP_SBC_LSR_IMM(const u32 i) { OP_ARITHMETIC(LSR_IMM; GET_CARRY(1), sbb, 0, 0); }
static int OP_SBC_LSR_REG(const u32 i) { OP_ARITHMETIC(LSR_REG; GET_CARRY(1), sbb, 0, 0); }
static int OP_SBC_ASR_IMM(const u32 i) { OP_ARITHMETIC(ASR_IMM; GET_CARRY(1), sbb, 0, 0); }
static int OP_SBC_ASR_REG(const u32 i) { OP_ARITHMETIC(ASR_REG; GET_CARRY(1), sbb, 0, 0); }
static int OP_SBC_ROR_IMM(const u32 i) { OP_ARITHMETIC(ROR_IMM; GET_CARRY(1), sbb, 0, 0); }
static int OP_SBC_ROR_REG(const u32 i) { OP_ARITHMETIC(ROR_REG; GET_CARRY(1), sbb, 0, 0); }
static int OP_SBC_IMM_VAL(const u32 i) { OP_ARITHMETIC(IMM_VAL; GET_CARRY(1), sbb, 0, 0); }

static int OP_SBC_S_LSL_IMM(const u32 i) { OP_ARITHMETIC(LSL_IMM; GET_CARRY(1), sbb, 0, 1); }
static int OP_SBC_S_LSL_REG(const u32 i) { OP_ARITHMETIC(LSL_REG; GET_CARRY(1), sbb, 0, 1); }
static int OP_SBC_S_LSR_IMM(const u32 i) { OP_ARITHMETIC(LSR_IMM; GET_CARRY(1), sbb, 0, 1); }
static int OP_SBC_S_LSR_REG(const u32 i) { OP_ARITHMETIC(LSR_REG; GET_CARRY(1), sbb, 0, 1); }
static int OP_SBC_S_ASR_IMM(const u32 i) { OP_ARITHMETIC(ASR_IMM; GET_CARRY(1), sbb, 0, 1); }
static int OP_SBC_S_ASR_REG(const u32 i) { OP_ARITHMETIC(ASR_REG; GET_CARRY(1), sbb, 0, 1); }
static int OP_SBC_S_ROR_IMM(const u32 i) { OP_ARITHMETIC(ROR_IMM; GET_CARRY(1), sbb, 0, 1); }
static int OP_SBC_S_ROR_REG(const u32 i) { OP_ARITHMETIC(ROR_REG; GET_CARRY(1), sbb, 0, 1); }
static int OP_SBC_S_IMM_VAL(const u32 i) { OP_ARITHMETIC(IMM_VAL; GET_CARRY(1), sbb, 0, 1); }

static int OP_RSC_LSL_IMM(const u32 i) { OP_ARITHMETIC_R(LSL_IMM; GET_CARRY(1), sbb, 0, 0); }
static int OP_RSC_LSL_REG(const u32 i) { OP_ARITHMETIC_R(LSL_REG; GET_CARRY(1), sbb, 0, 0); }
static int OP_RSC_LSR_IMM(const u32 i) { OP_ARITHMETIC_R(LSR_IMM; GET_CARRY(1), sbb, 0, 0); }
static int OP_RSC_LSR_REG(const u32 i) { OP_ARITHMETIC_R(LSR_REG; GET_CARRY(1), sbb, 0, 0); }
static int OP_RSC_ASR_IMM(const u32 i) { OP_ARITHMETIC_R(ASR_IMM; GET_CARRY(1), sbb, 0, 0); }
static int OP_RSC_ASR_REG(const u32 i) { OP_ARITHMETIC_R(ASR_REG; GET_CARRY(1), sbb, 0, 0); }
static int OP_RSC_ROR_IMM(const u32 i) { OP_ARITHMETIC_R(ROR_IMM; GET_CARRY(1), sbb, 0, 0); }
static int OP_RSC_ROR_REG(const u32 i) { OP_ARITHMETIC_R(ROR_REG; GET_CARRY(1), sbb, 0, 0); }
static int OP_RSC_IMM_VAL(const u32 i) { OP_ARITHMETIC_R(IMM_VAL; GET_CARRY(1), sbb, 0, 0); }

static int OP_RSC_S_LSL_IMM(const u32 i) { OP_ARITHMETIC_R(LSL_IMM; GET_CARRY(1), sbb, 0, 1); }
static int OP_RSC_S_LSL_REG(const u32 i) { OP_ARITHMETIC_R(LSL_REG; GET_CARRY(1), sbb, 0, 1); }
static int OP_RSC_S_LSR_IMM(const u32 i) { OP_ARITHMETIC_R(LSR_IMM; GET_CARRY(1), sbb, 0, 1); }
static int OP_RSC_S_LSR_REG(const u32 i) { OP_ARITHMETIC_R(LSR_REG; GET_CARRY(1), sbb, 0, 1); }
static int OP_RSC_S_ASR_IMM(const u32 i) { OP_ARITHMETIC_R(ASR_IMM; GET_CARRY(1), sbb, 0, 1); }
static int OP_RSC_S_ASR_REG(const u32 i) { OP_ARITHMETIC_R(ASR_REG; GET_CARRY(1), sbb, 0, 1); }
static int OP_RSC_S_ROR_IMM(const u32 i) { OP_ARITHMETIC_R(ROR_IMM; GET_CARRY(1), sbb, 0, 1); }
static int OP_RSC_S_ROR_REG(const u32 i) { OP_ARITHMETIC_R(ROR_REG; GET_CARRY(1), sbb, 0, 1); }
static int OP_RSC_S_IMM_VAL(const u32 i) { OP_ARITHMETIC_R(IMM_VAL; GET_CARRY(1), sbb, 0, 1); }

static int OP_BIC_LSL_IMM(const u32 i) { OP_ARITHMETIC(LSL_IMM; c.not_(rhs), and_, 1, 0); }
static int OP_BIC_LSL_REG(const u32 i) { OP_ARITHMETIC(LSL_REG; c.not_(rhs), and_, 1, 0); }
static int OP_BIC_LSR_IMM(const u32 i) { OP_ARITHMETIC(LSR_IMM; c.not_(rhs), and_, 1, 0); }
static int OP_BIC_LSR_REG(const u32 i) { OP_ARITHMETIC(LSR_REG; c.not_(rhs), and_, 1, 0); }
static int OP_BIC_ASR_IMM(const u32 i) { OP_ARITHMETIC(ASR_IMM; c.not_(rhs), and_, 1, 0); }
static int OP_BIC_ASR_REG(const u32 i) { OP_ARITHMETIC(ASR_REG; c.not_(rhs), and_, 1, 0); }
static int OP_BIC_ROR_IMM(const u32 i) { OP_ARITHMETIC(ROR_IMM; c.not_(rhs), and_, 1, 0); }
static int OP_BIC_ROR_REG(const u32 i) { OP_ARITHMETIC(ROR_REG; c.not_(rhs), and_, 1, 0); }
static int OP_BIC_IMM_VAL(const u32 i) { OP_ARITHMETIC(IMM_VAL; rhs = ~rhs,  and_, 1, 0); }

static int OP_BIC_S_LSL_IMM(const u32 i) { OP_ARITHMETIC_S(S_LSL_IMM; c.not_(rhs), and_, 1); }
static int OP_BIC_S_LSL_REG(const u32 i) { OP_ARITHMETIC_S(S_LSL_REG; c.not_(rhs), and_, 1); }
static int OP_BIC_S_LSR_IMM(const u32 i) { OP_ARITHMETIC_S(S_LSR_IMM; c.not_(rhs), and_, 1); }
static int OP_BIC_S_LSR_REG(const u32 i) { OP_ARITHMETIC_S(S_LSR_REG; c.not_(rhs), and_, 1); }
static int OP_BIC_S_ASR_IMM(const u32 i) { OP_ARITHMETIC_S(S_ASR_IMM; c.not_(rhs), and_, 1); }
static int OP_BIC_S_ASR_REG(const u32 i) { OP_ARITHMETIC_S(S_ASR_REG; c.not_(rhs), and_, 1); }
static int OP_BIC_S_ROR_IMM(const u32 i) { OP_ARITHMETIC_S(S_ROR_IMM; c.not_(rhs), and_, 1); }
static int OP_BIC_S_ROR_REG(const u32 i) { OP_ARITHMETIC_S(S_ROR_REG; c.not_(rhs), and_, 1); }
static int OP_BIC_S_IMM_VAL(const u32 i) { OP_ARITHMETIC_S(S_IMM_VAL; rhs = ~rhs,  and_, 1); }

//-----------------------------------------------------------------------------
//   TST
//-----------------------------------------------------------------------------
#define OP_TST_(arg) \
	arg; \
	c.test(reg_pos_ptr(16), rhs); \
	SET_NZC; \
	return 1;

static int OP_TST_LSL_IMM(const u32 i) { OP_TST_(S_LSL_IMM); }
static int OP_TST_LSL_REG(const u32 i) { OP_TST_(S_LSL_REG); }
static int OP_TST_LSR_IMM(const u32 i) { OP_TST_(S_LSR_IMM); }
static int OP_TST_LSR_REG(const u32 i) { OP_TST_(S_LSR_REG); }
static int OP_TST_ASR_IMM(const u32 i) { OP_TST_(S_ASR_IMM); }
static int OP_TST_ASR_REG(const u32 i) { OP_TST_(S_ASR_REG); }
static int OP_TST_ROR_IMM(const u32 i) { OP_TST_(S_ROR_IMM); }
static int OP_TST_ROR_REG(const u32 i) { OP_TST_(S_ROR_REG); }
static int OP_TST_IMM_VAL(const u32 i) { OP_TST_(S_IMM_VAL); }

//-----------------------------------------------------------------------------
//   TEQ
//-----------------------------------------------------------------------------
#define OP_TEQ_(arg) \
	arg; \
	if (!rhs_is_imm) \
		c.xor_(*(GPVar*)&rhs, reg_pos_ptr(16)); \
	else \
	{ \
		GPVar x = c.newGP(VARIABLE_TYPE_GPD); \
		c.mov(x, rhs); \
		c.xor_(x, reg_pos_ptr(16)); \
	} \
	SET_NZC; \
	return 1;

static int OP_TEQ_LSL_IMM(const u32 i) { OP_TEQ_(S_LSL_IMM); }
static int OP_TEQ_LSL_REG(const u32 i) { OP_TEQ_(S_LSL_REG); }
static int OP_TEQ_LSR_IMM(const u32 i) { OP_TEQ_(S_LSR_IMM); }
static int OP_TEQ_LSR_REG(const u32 i) { OP_TEQ_(S_LSR_REG); }
static int OP_TEQ_ASR_IMM(const u32 i) { OP_TEQ_(S_ASR_IMM); }
static int OP_TEQ_ASR_REG(const u32 i) { OP_TEQ_(S_ASR_REG); }
static int OP_TEQ_ROR_IMM(const u32 i) { OP_TEQ_(S_ROR_IMM); }
static int OP_TEQ_ROR_REG(const u32 i) { OP_TEQ_(S_ROR_REG); }
static int OP_TEQ_IMM_VAL(const u32 i) { OP_TEQ_(S_IMM_VAL); }

//-----------------------------------------------------------------------------
//   CMP
//-----------------------------------------------------------------------------
static void init_op_cmp(int PROCNUM, int sign)
{
	c.clear();
	JIT_COMMENT("init_op_cmp ARM%c, sign %d", PROCNUM?'7':'9', sign);
	// actually takes 1 input, the flags reg
	c.newFunction(ASMJIT_CALL_CONV, FunctionBuilder0<Void>());
	c.getFunction()->setHint(FUNCTION_HINT_NAKED, true);
	GPVar x = c.newGP(VARIABLE_TYPE_GPD);
	GPVar y = c.newGP(VARIABLE_TYPE_GPD);
#if defined(_M_X64) || defined(__x86_64__)
	GPVar bb_cpu = c.newGP(VARIABLE_TYPE_GPN);
	c.mov(bb_cpu, (uintptr_t)&ARMPROC);
	Mem flags = flags_ptr;
#else
	Mem flags = byte_ptr_abs((u8*)&cpu->CPSR.val+3);
#endif
	c.sets(x.r8Lo());
	c.setz(y.r8Lo());
	c.lea(x, ptr(y.r64(), x.r64(), TIMES_2));
	c.set(sign ? C_NC : C_C, y.r8Lo());
	c.lea(x, ptr(y.r64(), x.r64(), TIMES_2));
	c.seto(y.r8Lo());
	c.lea(x, ptr(y.r64(), x.r64(), TIMES_2));
	c.movzx(y, flags);
	c.shl(x, 4);
	c.and_(y, 0xF);
	c.or_(x, y);
	c.mov(flags, x.r8Lo());
	c.endFunction();
	op_cmp[PROCNUM][sign] = c.make();
}

#define OP_CMP(arg) \
	arg; \
	c.cmp(reg_pos_ptr(16), rhs); \
	ECall* ctx = c.call(op_cmp[PROCNUM][1]); \
	ctx->setPrototype(ASMJIT_CALL_CONV, FunctionBuilder0<Void>()); \
	return 1;

static int OP_CMP_LSL_IMM(const u32 i) { OP_CMP(LSL_IMM); }
static int OP_CMP_LSL_REG(const u32 i) { OP_CMP(LSL_REG); }
static int OP_CMP_LSR_IMM(const u32 i) { OP_CMP(LSR_IMM); }
static int OP_CMP_LSR_REG(const u32 i) { OP_CMP(LSR_REG); }
static int OP_CMP_ASR_IMM(const u32 i) { OP_CMP(ASR_IMM); }
static int OP_CMP_ASR_REG(const u32 i) { OP_CMP(ASR_REG); }
static int OP_CMP_ROR_IMM(const u32 i) { OP_CMP(ROR_IMM); }
static int OP_CMP_ROR_REG(const u32 i) { OP_CMP(ROR_REG); }
static int OP_CMP_IMM_VAL(const u32 i) { OP_CMP(IMM_VAL); }
#undef OP_CMP

//-----------------------------------------------------------------------------
//   CMN
//-----------------------------------------------------------------------------
#define OP_CMN(arg) \
	arg; \
	u32 rhs_imm = *(u32*)&rhs; \
	int sign = rhs_is_imm && rhs_imm != -rhs_imm; \
	if(sign) \
		c.cmp(reg_pos_ptr(16), -rhs_imm); \
	else \
	{ \
		GPVar lhs = c.newGP(VARIABLE_TYPE_GPD); \
		c.mov(lhs, reg_pos_ptr(16)); \
		c.add(lhs, rhs); \
	} \
	ECall* ctx = c.call(op_cmp[PROCNUM][sign]); \
	ctx->setPrototype(ASMJIT_CALL_CONV, FunctionBuilder0<Void>()); \
	return 1;

static int OP_CMN_LSL_IMM(const u32 i) { OP_CMN(LSL_IMM); }
static int OP_CMN_LSL_REG(const u32 i) { OP_CMN(LSL_REG); }
static int OP_CMN_LSR_IMM(const u32 i) { OP_CMN(LSR_IMM); }
static int OP_CMN_LSR_REG(const u32 i) { OP_CMN(LSR_REG); }
static int OP_CMN_ASR_IMM(const u32 i) { OP_CMN(ASR_IMM); }
static int OP_CMN_ASR_REG(const u32 i) { OP_CMN(ASR_REG); }
static int OP_CMN_ROR_IMM(const u32 i) { OP_CMN(ROR_IMM); }
static int OP_CMN_ROR_REG(const u32 i) { OP_CMN(ROR_REG); }
static int OP_CMN_IMM_VAL(const u32 i) { OP_CMN(IMM_VAL); }
#undef OP_CMN

//-----------------------------------------------------------------------------
//   MOV
//-----------------------------------------------------------------------------
#define OP_MOV(arg) \
    arg; \
	c.mov(reg_pos_ptr(12), rhs); \
	if(REG_POS(i,12)==15) \
	{ \
		c.mov(cpu_ptr(next_instruction), rhs); \
		return 1; \
	} \
    return 1;

static int OP_MOV_LSL_IMM(const u32 i) { if (i == 0xE1A00000) { /* nop */ JIT_COMMENT("nop"); return 1; } OP_MOV(LSL_IMM); }
static int OP_MOV_LSL_REG(const u32 i) { OP_MOV(LSL_REG; if (REG_POS(i,0) == 15) c.add(rhs, 4);); }
static int OP_MOV_LSR_IMM(const u32 i) { OP_MOV(LSR_IMM); }
static int OP_MOV_LSR_REG(const u32 i) { OP_MOV(LSR_REG; if (REG_POS(i,0) == 15) c.add(rhs, 4);); }
static int OP_MOV_ASR_IMM(const u32 i) { OP_MOV(ASR_IMM); }
static int OP_MOV_ASR_REG(const u32 i) { OP_MOV(ASR_REG); }
static int OP_MOV_ROR_IMM(const u32 i) { OP_MOV(ROR_IMM); }
static int OP_MOV_ROR_REG(const u32 i) { OP_MOV(ROR_REG); }
static int OP_MOV_IMM_VAL(const u32 i) { OP_MOV(IMM_VAL); }

#define OP_MOV_S(arg) \
    arg; \
	c.mov(reg_pos_ptr(12), rhs); \
	if(REG_POS(i,12)==15) \
	{ \
		S_DST_R15; \
		c.add(total_cycles, 2); \
		return 1; \
	} \
	if(!rhs_is_imm) \
		c.cmp(*(GPVar*)&rhs, 0); \
	else \
		c.cmp(reg_pos_ptr(12), 0); \
	SET_NZC; \
    return 1;

static int OP_MOV_S_LSL_IMM(const u32 i) { OP_MOV_S(S_LSL_IMM); }
static int OP_MOV_S_LSL_REG(const u32 i) { OP_MOV_S(S_LSL_REG; if (REG_POS(i,0) == 15) c.add(rhs, 4);); }
static int OP_MOV_S_LSR_IMM(const u32 i) { OP_MOV_S(S_LSR_IMM); }
static int OP_MOV_S_LSR_REG(const u32 i) { OP_MOV_S(S_LSR_REG; if (REG_POS(i,0) == 15) c.add(rhs, 4);); }
static int OP_MOV_S_ASR_IMM(const u32 i) { OP_MOV_S(S_ASR_IMM); }
static int OP_MOV_S_ASR_REG(const u32 i) { OP_MOV_S(S_ASR_REG); }
static int OP_MOV_S_ROR_IMM(const u32 i) { OP_MOV_S(S_ROR_IMM); }
static int OP_MOV_S_ROR_REG(const u32 i) { OP_MOV_S(S_ROR_REG); }
static int OP_MOV_S_IMM_VAL(const u32 i) { OP_MOV_S(S_IMM_VAL); }

//-----------------------------------------------------------------------------
//   MVN
//-----------------------------------------------------------------------------
static int OP_MVN_LSL_IMM(const u32 i) { OP_MOV(LSL_IMM; c.not_(rhs)); }
static int OP_MVN_LSL_REG(const u32 i) { OP_MOV(LSL_REG; c.not_(rhs)); }
static int OP_MVN_LSR_IMM(const u32 i) { OP_MOV(LSR_IMM; c.not_(rhs)); }
static int OP_MVN_LSR_REG(const u32 i) { OP_MOV(LSR_REG; c.not_(rhs)); }
static int OP_MVN_ASR_IMM(const u32 i) { OP_MOV(ASR_IMM; c.not_(rhs)); }
static int OP_MVN_ASR_REG(const u32 i) { OP_MOV(ASR_REG; c.not_(rhs)); }
static int OP_MVN_ROR_IMM(const u32 i) { OP_MOV(ROR_IMM; c.not_(rhs)); }
static int OP_MVN_ROR_REG(const u32 i) { OP_MOV(ROR_REG; c.not_(rhs)); }
static int OP_MVN_IMM_VAL(const u32 i) { OP_MOV(IMM_VAL; rhs = ~rhs); }

static int OP_MVN_S_LSL_IMM(const u32 i) { OP_MOV_S(S_LSL_IMM; c.not_(rhs)); }
static int OP_MVN_S_LSL_REG(const u32 i) { OP_MOV_S(S_LSL_REG; c.not_(rhs)); }
static int OP_MVN_S_LSR_IMM(const u32 i) { OP_MOV_S(S_LSR_IMM; c.not_(rhs)); }
static int OP_MVN_S_LSR_REG(const u32 i) { OP_MOV_S(S_LSR_REG; c.not_(rhs)); }
static int OP_MVN_S_ASR_IMM(const u32 i) { OP_MOV_S(S_ASR_IMM; c.not_(rhs)); }
static int OP_MVN_S_ASR_REG(const u32 i) { OP_MOV_S(S_ASR_REG; c.not_(rhs)); }
static int OP_MVN_S_ROR_IMM(const u32 i) { OP_MOV_S(S_ROR_IMM; c.not_(rhs)); }
static int OP_MVN_S_ROR_REG(const u32 i) { OP_MOV_S(S_ROR_REG; c.not_(rhs)); }
static int OP_MVN_S_IMM_VAL(const u32 i) { OP_MOV_S(S_IMM_VAL; rhs = ~rhs); }

//-----------------------------------------------------------------------------
//   QADD / QDADD / QSUB / QDSUB
//-----------------------------------------------------------------------------
// TODO
static int OP_QADD(const u32 i) { printf("JIT: unimplemented OP_QADD\n"); return 0; }
static int OP_QSUB(const u32 i) { printf("JIT: unimplemented OP_QSUB\n"); return 0; }
static int OP_QDADD(const u32 i) { printf("JIT: unimplemented OP_QDADD\n"); return 0; }
static int OP_QDSUB(const u32 i) { printf("JIT: unimplemented OP_QDSUB\n"); return 0; }

//-----------------------------------------------------------------------------
//   MUL
//-----------------------------------------------------------------------------
static void MUL_Mxx_END(GPVar x, bool sign, int cycles)
{
	if(sign)
	{
		GPVar y = c.newGP(VARIABLE_TYPE_GPD);
		c.mov(y, x);
		c.sar(x, 31);
		c.xor_(x, y);
	}
	c.or_(x, 1);
	c.bsr(bb_cycles, x);
	c.shr(bb_cycles, 3);
	c.add(bb_cycles, cycles+1);
}

#define OP_MUL_(op, width, sign, accum, flags) \
	GPVar lhs = c.newGP(VARIABLE_TYPE_GPD); \
	GPVar rhs = c.newGP(VARIABLE_TYPE_GPD); \
	GPVar hi; \
	if (width) \
	{ \
		hi = c.newGP(VARIABLE_TYPE_GPD); \
		c.xor_(hi, hi); \
	} \
	c.mov(lhs, reg_pos_ptr(0)); \
	c.mov(rhs, reg_pos_ptr(8)); \
	op; \
	if(width && accum) \
	{ \
		if(flags) \
		{ \
			c.add(lhs, reg_pos_ptr(12)); \
			c.adc(hi, reg_pos_ptr(16)); \
			c.mov(reg_pos_ptr(12), lhs); \
			c.mov(reg_pos_ptr(16), hi); \
			SET_NZ_W; \
		} \
		else \
		{ \
			c.add(reg_pos_ptr(12), lhs); \
			c.adc(reg_pos_ptr(16), hi); \
		} \
	} \
	else if(width) \
	{ \
		c.mov(reg_pos_ptr(12), lhs); \
		c.mov(reg_pos_ptr(16), hi); \
		if(flags) SET_NZ_W; \
	} \
	else \
	{ \
		if(accum) c.add(lhs, reg_pos_ptr(12)); \
		c.mov(reg_pos_ptr(16), lhs); \
		if(flags) { c.cmp(lhs, 0); SET_NZ; }\
	} \
	MUL_Mxx_END(rhs, sign, 1+width+accum); \
	return 1;

static int OP_MUL(const u32 i) { OP_MUL_(c.imul(lhs,rhs), 0, 1, 0, 0); }
static int OP_MLA(const u32 i) { OP_MUL_(c.imul(lhs,rhs), 0, 1, 1, 0); }
static int OP_UMULL(const u32 i) { OP_MUL_(c.mul(hi,lhs,rhs), 1, 0, 0, 0); }
static int OP_UMLAL(const u32 i) { OP_MUL_(c.mul(hi,lhs,rhs), 1, 0, 1, 0); }
static int OP_SMULL(const u32 i) { OP_MUL_(c.imul(hi,lhs,rhs), 1, 1, 0, 0); }
static int OP_SMLAL(const u32 i) { OP_MUL_(c.imul(hi,lhs,rhs), 1, 1, 1, 0); }

static int OP_MUL_S(const u32 i) { OP_MUL_(c.imul(lhs,rhs), 0, 1, 0, 1); }
static int OP_MLA_S(const u32 i) { OP_MUL_(c.imul(lhs,rhs), 0, 1, 1, 1); }
static int OP_UMULL_S(const u32 i) { OP_MUL_(c.mul(hi,lhs,rhs), 1, 0, 0, 1); }
static int OP_UMLAL_S(const u32 i) { OP_MUL_(c.mul(hi,lhs,rhs), 1, 0, 1, 1); }
static int OP_SMULL_S(const u32 i) { OP_MUL_(c.imul(hi,lhs,rhs), 1, 1, 0, 1); }
static int OP_SMLAL_S(const u32 i) { OP_MUL_(c.imul(hi,lhs,rhs), 1, 1, 1, 1); }

#define OP_MULxy_(op, x, y, width, accum, flags) \
	GPVar lhs = c.newGP(VARIABLE_TYPE_GPD); \
	GPVar rhs = c.newGP(VARIABLE_TYPE_GPD); \
	GPVar hi; \
	c.movsx(lhs, reg_pos_ptr##x(0)); \
	c.movsx(rhs, reg_pos_ptr##y(8)); \
	if (width) \
	{ \
		hi = c.newGP(VARIABLE_TYPE_GPD); \
	} \
	op; \
	if(width && accum) \
	{ \
		if(flags) \
		{ \
			c.add(lhs, reg_pos_ptr(12)); \
			c.adc(hi, reg_pos_ptr(16)); \
			c.mov(reg_pos_ptr(12), lhs); \
			c.mov(reg_pos_ptr(16), hi); \
			SET_Q; \
		} \
		else \
		{ \
			c.add(reg_pos_ptr(12), lhs); \
			c.adc(reg_pos_ptr(16), hi); \
		} \
	} \
	else \
	if(width) \
	{ \
		c.mov(reg_pos_ptr(12), lhs); \
		c.mov(reg_pos_ptr(16), hi); \
		if(flags) { SET_Q; }\
	} \
	else \
	{ \
		if (accum) c.add(lhs, reg_pos_ptr(12));  \
		c.mov(reg_pos_ptr(16), lhs); \
		if(flags) { SET_Q; }\
	} \
	return 1;


//-----------------------------------------------------------------------------
//   SMUL
//-----------------------------------------------------------------------------
static int OP_SMUL_B_B(const u32 i) { OP_MULxy_(c.imul(lhs, rhs), L, L, 0, 0, 0); }
static int OP_SMUL_B_T(const u32 i) { OP_MULxy_(c.imul(lhs, rhs), L, H, 0, 0, 0); }
static int OP_SMUL_T_B(const u32 i) { OP_MULxy_(c.imul(lhs, rhs), H, L, 0, 0, 0); }
static int OP_SMUL_T_T(const u32 i) { OP_MULxy_(c.imul(lhs, rhs), H, H, 0, 0, 0); }

//-----------------------------------------------------------------------------
//   SMLA
//-----------------------------------------------------------------------------
static int OP_SMLA_B_B(const u32 i) { OP_MULxy_(c.imul(lhs, rhs), L, L, 0, 1, 1); }
static int OP_SMLA_B_T(const u32 i) { OP_MULxy_(c.imul(lhs, rhs), L, H, 0, 1, 1); }
static int OP_SMLA_T_B(const u32 i) { OP_MULxy_(c.imul(lhs, rhs), H, L, 0, 1, 1); }
static int OP_SMLA_T_T(const u32 i) { OP_MULxy_(c.imul(lhs, rhs), H, H, 0, 1, 1); }

//-----------------------------------------------------------------------------
//   SMLAL
//-----------------------------------------------------------------------------
static int OP_SMLAL_B_B(const u32 i) { OP_MULxy_(c.imul(hi,lhs,rhs), L, L, 1, 1, 1); }
static int OP_SMLAL_B_T(const u32 i) { OP_MULxy_(c.imul(hi,lhs,rhs), L, H, 1, 1, 1); }
static int OP_SMLAL_T_B(const u32 i) { OP_MULxy_(c.imul(hi,lhs,rhs), H, L, 1, 1, 1); }
static int OP_SMLAL_T_T(const u32 i) { OP_MULxy_(c.imul(hi,lhs,rhs), H, H, 1, 1, 1); }

//-----------------------------------------------------------------------------
//   SMULW / SMLAW
//-----------------------------------------------------------------------------
#define OP_SMxxW_(x, accum, flags) \
	GPVar lhs = c.newGP(VARIABLE_TYPE_GPD); \
	GPVar rhs = c.newGP(VARIABLE_TYPE_GPD); \
	GPVar hi = c.newGP(VARIABLE_TYPE_GPD); \
	c.movsx(lhs, reg_pos_ptr##x(8)); \
	c.mov(rhs, reg_pos_ptr(0)); \
	c.imul(hi, lhs, rhs);  \
	c.shr(lhs, 16); \
	c.shl(hi, 16); \
	c.or_(lhs, hi); \
	if (accum) c.add(lhs, reg_pos_ptr(12)); \
	c.mov(reg_pos_ptr(16), lhs); \
	if (flags) { SET_Q; } \
	return 1;

static int OP_SMULW_B(const u32 i) { OP_SMxxW_(L, 0, 0); }
static int OP_SMULW_T(const u32 i) { OP_SMxxW_(H, 0, 0); }
static int OP_SMLAW_B(const u32 i) { OP_SMxxW_(L, 1, 1); }
static int OP_SMLAW_T(const u32 i) { OP_SMxxW_(H, 1, 1); }

//-----------------------------------------------------------------------------
//   MRS / MSR
//-----------------------------------------------------------------------------
static int OP_MRS_CPSR(const u32 i)
{
	GPVar x = c.newGP(VARIABLE_TYPE_GPD);
	c.mov(x, cpu_ptr(CPSR));
	c.mov(reg_pos_ptr(12), x);
	return 1;
}

static int OP_MRS_SPSR(const u32 i)
{
	GPVar x = c.newGP(VARIABLE_TYPE_GPD);
	c.mov(x, cpu_ptr(SPSR));
	c.mov(reg_pos_ptr(12), x);
	return 1;
}

// TODO: SPSR: if(cpu->CPSR.bits.mode == USR || cpu->CPSR.bits.mode == SYS) return 1;
#define OP_MSR_(reg, args, sw) \
	Mem xPSR_mem = cpu_ptr(reg.val); \
	GPVar xPSR = c.newGP(VARIABLE_TYPE_GPD); \
	GPVar operand = c.newGP(VARIABLE_TYPE_GPD); \
	args; \
	switch (((i>>16) & 0xF)) \
	{ \
		case 0x1:		/* bit 16 */ \
			{ \
				GPVar mode = c.newGP(VARIABLE_TYPE_GPD); \
				Label __skip = c.newLabel(); \
				c.mov(mode, cpu_ptr(CPSR)); \
				c.and_(mode, 0x1F); \
				c.cmp(mode, USR); \
				c.je(__skip); \
				if (sw) \
				{ \
					c.mov(mode, rhs); \
					c.and_(mode, 0x1F); \
					ECall* ctx = c.call((void*)armcpu_switchMode); \
					ctx->setPrototype(CALL_CONV_DEFAULT, FunctionBuilder2<void, void*, u8>()); \
					ctx->setArgument(0, bb_cpu); \
					ctx->setArgument(1, mode); \
				} \
				c.mov(operand, rhs); \
				c.mov(xPSR, xPSR_mem); \
				c.and_(operand, 0x000000FF); \
				c.and_(xPSR, 0xFFFFFF00); \
				c.or_(xPSR, operand); \
				c.mov(xPSR_mem, xPSR); \
				changeCPSR; \
				c.bind(__skip); \
			} \
			return 1; \
		case 0x2:		/* bit 17 */ \
			{ \
				GPVar mode = c.newGP(VARIABLE_TYPE_GPD); \
				Label __skip = c.newLabel(); \
				c.mov(mode, cpu_ptr(CPSR)); \
				c.and_(mode, 0x1F); \
				c.cmp(mode, USR); \
				c.je(__skip); \
				c.mov(operand, rhs); \
				c.mov(xPSR, xPSR_mem); \
				c.and_(operand, 0x0000FF00); \
				c.and_(xPSR, 0xFFFF00FF); \
				c.or_(xPSR, operand); \
				c.mov(xPSR_mem, xPSR); \
				changeCPSR; \
				c.bind(__skip); \
			} \
			return 1; \
		case 0x4:		/* bit 18 */ \
			{ \
				GPVar mode = c.newGP(VARIABLE_TYPE_GPD); \
				Label __skip = c.newLabel(); \
				c.mov(mode, cpu_ptr(CPSR)); \
				c.and_(mode, 0x1F); \
				c.cmp(mode, USR); \
				c.je(__skip); \
				c.mov(operand, rhs); \
				c.mov(xPSR, xPSR_mem); \
				c.and_(operand, 0x00FF0000); \
				c.and_(xPSR, 0xFF00FFFF); \
				c.or_(xPSR, operand); \
				c.mov(xPSR_mem, xPSR); \
				changeCPSR; \
				c.bind(__skip); \
			} \
			return 1; \
		case 0x8:		/* bit 19 */ \
			{ \
				c.mov(operand, rhs); \
				c.mov(xPSR, xPSR_mem); \
				c.and_(operand, 0xFF000000); \
				c.and_(xPSR, 0x00FFFFFF); \
				c.or_(xPSR, operand); \
				c.mov(xPSR_mem, xPSR); \
				changeCPSR; \
			} \
			return 1; \
		default: \
			break; \
	} \
\
	static u32 byte_mask =	(BIT16(i)?0x000000FF:0x00000000) | \
							(BIT17(i)?0x0000FF00:0x00000000) | \
							(BIT18(i)?0x00FF0000:0x00000000) | \
							(BIT19(i)?0xFF000000:0x00000000); \
	static u32 byte_mask_USR = (BIT19(i)?0xFF000000:0x00000000); \
\
	GPVar mode = c.newGP(VARIABLE_TYPE_GPD); \
	Label __USR = c.newLabel(); \
	Label __done = c.newLabel(); \
	c.mov(mode, cpu_ptr(CPSR.val)); \
	c.and_(mode, 0x1F); \
	c.cmp(mode, USR); \
	c.je(__USR); \
	/* mode != USR */ \
	if (sw && BIT16(i)) \
	{ \
		/* armcpu_switchMode */ \
		c.mov(mode, rhs); \
		c.and_(mode, 0x1F); \
		ECall* ctx = c.call((void*)armcpu_switchMode); \
		ctx->setPrototype(CALL_CONV_DEFAULT, FunctionBuilder2<void, void*, u8>()); \
		ctx->setArgument(0, bb_cpu); \
		ctx->setArgument(1, mode); \
	} \
	/* cpu->CPSR.val = (cpu->CPSR.val & ~byte_mask) | (operand & byte_mask); */ \
	c.mov(operand, rhs); \
	c.mov(xPSR, xPSR_mem); \
	c.and_(operand, byte_mask); \
	c.and_(xPSR, ~byte_mask); \
	c.or_(xPSR, operand); \
	c.mov(xPSR_mem, xPSR); \
	c.jmp(__done); \
	/* mode == USR */ \
	c.bind(__USR); \
	c.mov(operand, rhs); \
	c.mov(xPSR, xPSR_mem); \
	c.and_(operand, byte_mask_USR); \
	c.and_(xPSR, ~byte_mask_USR); \
	c.or_(xPSR, operand); \
	c.mov(xPSR_mem, xPSR); \
	c.bind(__done); \
	changeCPSR; \
	return 1;

static int OP_MSR_CPSR(const u32 i) { OP_MSR_(CPSR, REG_OFF, 1); }
static int OP_MSR_SPSR(const u32 i) { OP_MSR_(SPSR, REG_OFF, 0); }
static int OP_MSR_CPSR_IMM_VAL(const u32 i) { OP_MSR_(CPSR, IMM_VALUE, 1); }
static int OP_MSR_SPSR_IMM_VAL(const u32 i) { OP_MSR_(SPSR, IMM_VALUE, 0); }

//-----------------------------------------------------------------------------
//   LDR
//-----------------------------------------------------------------------------
typedef u32 (FASTCALL* OpLDR)(u32, u32*);

// 98% of all memory accesses land in the same region as the first execution of
// that instruction, so keep multiple copies with different fastpaths.
// The copies don't need to differ in any way; the point is merely to cooperate
// with x86 branch prediction.

enum {
	MEMTYPE_GENERIC = 0, // no assumptions
	MEMTYPE_MAIN = 1,
	MEMTYPE_DTCM = 2,
	MEMTYPE_ERAM = 3,
	MEMTYPE_SWIRAM = 4,
	MEMTYPE_OTHER = 5, // memory that is known to not be MAIN, DTCM, ERAM, or SWIRAM
};

static u32 classify_adr(u32 adr, bool store)
{
	if(PROCNUM==ARMCPU_ARM9 && (adr & ~0x3FFF) == MMU.DTCMRegion)
		return MEMTYPE_DTCM;
	else if((adr & 0x0F000000) == 0x02000000)
		return MEMTYPE_MAIN;
	else if(PROCNUM==ARMCPU_ARM7 && !store && (adr & 0xFF800000) == 0x03800000)
		return MEMTYPE_ERAM;
	else if(PROCNUM==ARMCPU_ARM7 && !store && (adr & 0xFF800000) == 0x03000000)
		return MEMTYPE_SWIRAM;
	else
		return MEMTYPE_GENERIC;
}

template<int PROCNUM, int memtype>
static u32 FASTCALL OP_LDR(u32 adr, u32 *dstreg)
{
	u32 data = READ32(cpu->mem_if->data, adr);
	if(adr&3)
		data = ROR(data, 8*(adr&3));
	*dstreg = data;
	return MMU_aluMemAccessCycles<PROCNUM,32,MMU_AD_READ>(3,adr);
}

template<int PROCNUM, int memtype>
static u32 FASTCALL OP_LDRH(u32 adr, u32 *dstreg)
{
	*dstreg = READ16(cpu->mem_if->data, adr);
	return MMU_aluMemAccessCycles<PROCNUM,16,MMU_AD_READ>(3,adr);
}

template<int PROCNUM, int memtype>
static u32 FASTCALL OP_LDRSH(u32 adr, u32 *dstreg)
{
	*dstreg = (s16)READ16(cpu->mem_if->data, adr);
	return MMU_aluMemAccessCycles<PROCNUM,16,MMU_AD_READ>(3,adr);
}

template<int PROCNUM, int memtype>
static u32 FASTCALL OP_LDRB(u32 adr, u32 *dstreg)
{
	*dstreg = READ8(cpu->mem_if->data, adr);
	return MMU_aluMemAccessCycles<PROCNUM,8,MMU_AD_READ>(3,adr);
}

template<int PROCNUM, int memtype>
static u32 FASTCALL OP_LDRSB(u32 adr, u32 *dstreg)
{
	*dstreg = (s8)READ8(cpu->mem_if->data, adr);
	return MMU_aluMemAccessCycles<PROCNUM,8,MMU_AD_READ>(3,adr);
}

#define T(op) op<0,0>, op<0,1>, op<0,2>, NULL, NULL, op<1,0>, op<1,1>, NULL, op<1,3>, op<1,4>
static const OpLDR LDR_tab[2][5]   = { T(OP_LDR) };
static const OpLDR LDRH_tab[2][5]  = { T(OP_LDRH) };
static const OpLDR LDRSH_tab[2][5] = { T(OP_LDRSH) };
static const OpLDR LDRB_tab[2][5]  = { T(OP_LDRB) };
static const OpLDR LDRSB_tab[2][5]  = { T(OP_LDRSB) };
#undef T

static u32 add(u32 lhs, u32 rhs) { return lhs + rhs; }
static u32 sub(u32 lhs, u32 rhs) { return lhs - rhs; }

#define OP_LDR_(mem_op, arg, sign_op, writeback) \
	GPVar adr = c.newGP(VARIABLE_TYPE_GPD); \
	GPVar dst = c.newGP(VARIABLE_TYPE_GPN); \
	c.mov(adr, reg_pos_ptr(16)); \
	c.lea(dst, reg_pos_ptr(12)); \
	arg; \
	if(!rhs_is_imm || *(u32*)&rhs) \
	{ \
		if(writeback == 0) \
			c.sign_op(adr, rhs); \
		else if(writeback < 0) \
		{ \
			c.sign_op(adr, rhs); \
			c.mov(reg_pos_ptr(16), adr); \
		} \
		else if(writeback > 0) \
		{ \
			GPVar tmp_reg = c.newGP(VARIABLE_TYPE_GPD); \
			c.mov(tmp_reg, adr); \
			c.sign_op(tmp_reg, rhs); \
			c.mov(reg_pos_ptr(16), tmp_reg); \
		} \
	} \
	u32 adr_first = sign_op(cpu->R[REG_POS(i,16)], rhs_first); \
	ECall *ctx = c.call((void*)mem_op##_tab[PROCNUM][classify_adr(adr_first,0)]); \
	ctx->setPrototype(ASMJIT_CALL_CONV, FunctionBuilder2<u32, u32, u32*>()); \
	ctx->setArgument(0, adr); \
	ctx->setArgument(1, dst); \
	ctx->setReturn(bb_cycles); \
	if(REG_POS(i,12)==15) \
	{ \
		GPVar tmp = c.newGP(VARIABLE_TYPE_GPD); \
		c.mov(tmp, reg_ptr(15)); \
		if (PROCNUM == 0) \
		{ \
			GPVar thumb = c.newGP(VARIABLE_TYPE_GPN); \
			c.movzx(thumb, reg_pos_ptrB(16)); \
			c.and_(thumb, 1); \
			c.shl(thumb, 5); \
			c.or_(cpu_ptr(CPSR), thumb.r64()); \
			c.and_(tmp, 0xFFFFFFFE); \
		} \
		else \
		{ \
			c.and_(tmp, 0xFFFFFFFC); \
		} \
		c.mov(cpu_ptr(next_instruction), tmp); \
	} \
	return 1;

// LDR
static int OP_LDR_P_IMM_OFF(const u32 i) { OP_LDR_(LDR, IMM_OFF_12, add, 0); }
static int OP_LDR_M_IMM_OFF(const u32 i) { OP_LDR_(LDR, IMM_OFF_12, sub, 0); }
static int OP_LDR_P_LSL_IMM_OFF(const u32 i) { OP_LDR_(LDR, LSL_IMM, add, 0); }
static int OP_LDR_M_LSL_IMM_OFF(const u32 i) { OP_LDR_(LDR, LSL_IMM, sub, 0); }
static int OP_LDR_P_LSR_IMM_OFF(const u32 i) { OP_LDR_(LDR, LSR_IMM, add, 0); }
static int OP_LDR_M_LSR_IMM_OFF(const u32 i) { OP_LDR_(LDR, LSR_IMM, sub, 0); }
static int OP_LDR_P_ASR_IMM_OFF(const u32 i) { OP_LDR_(LDR, ASR_IMM, add, 0); }
static int OP_LDR_M_ASR_IMM_OFF(const u32 i) { OP_LDR_(LDR, ASR_IMM, sub, 0); }
static int OP_LDR_P_ROR_IMM_OFF(const u32 i) { OP_LDR_(LDR, ROR_IMM, add, 0); }
static int OP_LDR_M_ROR_IMM_OFF(const u32 i) { OP_LDR_(LDR, ROR_IMM, sub, 0); }

static int OP_LDR_P_IMM_OFF_PREIND(const u32 i) { OP_LDR_(LDR, IMM_OFF_12, add, -1); }
static int OP_LDR_M_IMM_OFF_PREIND(const u32 i) { OP_LDR_(LDR, IMM_OFF_12, sub, -1); }
static int OP_LDR_P_LSL_IMM_OFF_PREIND(const u32 i) { OP_LDR_(LDR, LSL_IMM, add, -1); }
static int OP_LDR_M_LSL_IMM_OFF_PREIND(const u32 i) { OP_LDR_(LDR, LSL_IMM, sub, -1); }
static int OP_LDR_P_LSR_IMM_OFF_PREIND(const u32 i) { OP_LDR_(LDR, LSR_IMM, add, -1); }
static int OP_LDR_M_LSR_IMM_OFF_PREIND(const u32 i) { OP_LDR_(LDR, LSR_IMM, sub, -1); }
static int OP_LDR_P_ASR_IMM_OFF_PREIND(const u32 i) { OP_LDR_(LDR, ASR_IMM, add, -1); }
static int OP_LDR_M_ASR_IMM_OFF_PREIND(const u32 i) { OP_LDR_(LDR, ASR_IMM, sub, -1); }
static int OP_LDR_P_ROR_IMM_OFF_PREIND(const u32 i) { OP_LDR_(LDR, ROR_IMM, add, -1); }
static int OP_LDR_M_ROR_IMM_OFF_PREIND(const u32 i) { OP_LDR_(LDR, ROR_IMM, sub, -1); }
static int OP_LDR_P_IMM_OFF_POSTIND(const u32 i) { OP_LDR_(LDR, IMM_OFF_12, add, 1); }
static int OP_LDR_M_IMM_OFF_POSTIND(const u32 i) { OP_LDR_(LDR, IMM_OFF_12, sub, 1); }
static int OP_LDR_P_LSL_IMM_OFF_POSTIND(const u32 i) { OP_LDR_(LDR, LSL_IMM, add, 1); }
static int OP_LDR_M_LSL_IMM_OFF_POSTIND(const u32 i) { OP_LDR_(LDR, LSL_IMM, sub, 1); }
static int OP_LDR_P_LSR_IMM_OFF_POSTIND(const u32 i) { OP_LDR_(LDR, LSR_IMM, add, 1); }
static int OP_LDR_M_LSR_IMM_OFF_POSTIND(const u32 i) { OP_LDR_(LDR, LSR_IMM, sub, 1); }
static int OP_LDR_P_ASR_IMM_OFF_POSTIND(const u32 i) { OP_LDR_(LDR, ASR_IMM, add, 1); }
static int OP_LDR_M_ASR_IMM_OFF_POSTIND(const u32 i) { OP_LDR_(LDR, ASR_IMM, sub, 1); }
static int OP_LDR_P_ROR_IMM_OFF_POSTIND(const u32 i) { OP_LDR_(LDR, ROR_IMM, add, 1); }
static int OP_LDR_M_ROR_IMM_OFF_POSTIND(const u32 i) { OP_LDR_(LDR, ROR_IMM, sub, 1); }

// LDRH
static int OP_LDRH_P_IMM_OFF(const u32 i) { OP_LDR_(LDRH, IMM_OFF, add, 0); }
static int OP_LDRH_M_IMM_OFF(const u32 i) { OP_LDR_(LDRH, IMM_OFF, sub, 0); }
static int OP_LDRH_P_REG_OFF(const u32 i) { OP_LDR_(LDRH, REG_OFF, add, 0); }
static int OP_LDRH_M_REG_OFF(const u32 i) { OP_LDR_(LDRH, REG_OFF, sub, 0); }

static int OP_LDRH_PRE_INDE_P_IMM_OFF(const u32 i) { OP_LDR_(LDRH, IMM_OFF, add, -1); }
static int OP_LDRH_PRE_INDE_M_IMM_OFF(const u32 i) { OP_LDR_(LDRH, IMM_OFF, sub, -1); }
static int OP_LDRH_PRE_INDE_P_REG_OFF(const u32 i) { OP_LDR_(LDRH, REG_OFF, add, -1); }
static int OP_LDRH_PRE_INDE_M_REG_OFF(const u32 i) { OP_LDR_(LDRH, REG_OFF, sub, -1); }
static int OP_LDRH_POS_INDE_P_IMM_OFF(const u32 i) { OP_LDR_(LDRH, IMM_OFF, add, 1); }
static int OP_LDRH_POS_INDE_M_IMM_OFF(const u32 i) { OP_LDR_(LDRH, IMM_OFF, sub, 1); }
static int OP_LDRH_POS_INDE_P_REG_OFF(const u32 i) { OP_LDR_(LDRH, REG_OFF, add, 1); }
static int OP_LDRH_POS_INDE_M_REG_OFF(const u32 i) { OP_LDR_(LDRH, REG_OFF, sub, 1); }

// LDRSH
static int OP_LDRSH_P_IMM_OFF(const u32 i) { OP_LDR_(LDRSH, IMM_OFF, add, 0); }
static int OP_LDRSH_M_IMM_OFF(const u32 i) { OP_LDR_(LDRSH, IMM_OFF, sub, 0); }
static int OP_LDRSH_P_REG_OFF(const u32 i) { OP_LDR_(LDRSH, REG_OFF, add, 0); }
static int OP_LDRSH_M_REG_OFF(const u32 i) { OP_LDR_(LDRSH, REG_OFF, sub, 0); }

static int OP_LDRSH_PRE_INDE_P_IMM_OFF(const u32 i) { OP_LDR_(LDRSH, IMM_OFF, add, -1); }
static int OP_LDRSH_PRE_INDE_M_IMM_OFF(const u32 i) { OP_LDR_(LDRSH, IMM_OFF, sub, -1); }
static int OP_LDRSH_PRE_INDE_P_REG_OFF(const u32 i) { OP_LDR_(LDRSH, REG_OFF, add, -1); }
static int OP_LDRSH_PRE_INDE_M_REG_OFF(const u32 i) { OP_LDR_(LDRSH, REG_OFF, sub, -1); }
static int OP_LDRSH_POS_INDE_P_IMM_OFF(const u32 i) { OP_LDR_(LDRSH, IMM_OFF, add, 1); }
static int OP_LDRSH_POS_INDE_M_IMM_OFF(const u32 i) { OP_LDR_(LDRSH, IMM_OFF, sub, 1); }
static int OP_LDRSH_POS_INDE_P_REG_OFF(const u32 i) { OP_LDR_(LDRSH, REG_OFF, add, 1); }
static int OP_LDRSH_POS_INDE_M_REG_OFF(const u32 i) { OP_LDR_(LDRSH, REG_OFF, sub, 1); }

// LDRB
static int OP_LDRB_P_IMM_OFF(const u32 i) { OP_LDR_(LDRB, IMM_OFF_12, add, 0); }
static int OP_LDRB_M_IMM_OFF(const u32 i) { OP_LDR_(LDRB, IMM_OFF_12, sub, 0); }
static int OP_LDRB_P_LSL_IMM_OFF(const u32 i) { OP_LDR_(LDRB, LSL_IMM, add, 0); }
static int OP_LDRB_M_LSL_IMM_OFF(const u32 i) { OP_LDR_(LDRB, LSL_IMM, sub, 0); }
static int OP_LDRB_P_LSR_IMM_OFF(const u32 i) { OP_LDR_(LDRB, LSR_IMM, add, 0); }
static int OP_LDRB_M_LSR_IMM_OFF(const u32 i) { OP_LDR_(LDRB, LSR_IMM, sub, 0); }
static int OP_LDRB_P_ASR_IMM_OFF(const u32 i) { OP_LDR_(LDRB, ASR_IMM, add, 0); }
static int OP_LDRB_M_ASR_IMM_OFF(const u32 i) { OP_LDR_(LDRB, ASR_IMM, sub, 0); }
static int OP_LDRB_P_ROR_IMM_OFF(const u32 i) { OP_LDR_(LDRB, ROR_IMM, add, 0); }
static int OP_LDRB_M_ROR_IMM_OFF(const u32 i) { OP_LDR_(LDRB, ROR_IMM, sub, 0); }

static int OP_LDRB_P_IMM_OFF_PREIND(const u32 i) { OP_LDR_(LDRB, IMM_OFF_12, add, -1); }
static int OP_LDRB_M_IMM_OFF_PREIND(const u32 i) { OP_LDR_(LDRB, IMM_OFF_12, sub, -1); }
static int OP_LDRB_P_LSL_IMM_OFF_PREIND(const u32 i) { OP_LDR_(LDRB, LSL_IMM, add, -1); }
static int OP_LDRB_M_LSL_IMM_OFF_PREIND(const u32 i) { OP_LDR_(LDRB, LSL_IMM, sub, -1); }
static int OP_LDRB_P_LSR_IMM_OFF_PREIND(const u32 i) { OP_LDR_(LDRB, LSR_IMM, add, -1); }
static int OP_LDRB_M_LSR_IMM_OFF_PREIND(const u32 i) { OP_LDR_(LDRB, LSR_IMM, sub, -1); }
static int OP_LDRB_P_ASR_IMM_OFF_PREIND(const u32 i) { OP_LDR_(LDRB, ASR_IMM, add, -1); }
static int OP_LDRB_M_ASR_IMM_OFF_PREIND(const u32 i) { OP_LDR_(LDRB, ASR_IMM, sub, -1); }
static int OP_LDRB_P_ROR_IMM_OFF_PREIND(const u32 i) { OP_LDR_(LDRB, ROR_IMM, add, -1); }
static int OP_LDRB_M_ROR_IMM_OFF_PREIND(const u32 i) { OP_LDR_(LDRB, ROR_IMM, sub, -1); }
static int OP_LDRB_P_IMM_OFF_POSTIND(const u32 i) { OP_LDR_(LDRB, IMM_OFF_12, add, 1); }
static int OP_LDRB_M_IMM_OFF_POSTIND(const u32 i) { OP_LDR_(LDRB, IMM_OFF_12, sub, 1); }
static int OP_LDRB_P_LSL_IMM_OFF_POSTIND(const u32 i) { OP_LDR_(LDRB, LSL_IMM, add, 1); }
static int OP_LDRB_M_LSL_IMM_OFF_POSTIND(const u32 i) { OP_LDR_(LDRB, LSL_IMM, sub, 1); }
static int OP_LDRB_P_LSR_IMM_OFF_POSTIND(const u32 i) { OP_LDR_(LDRB, LSR_IMM, add, 1); }
static int OP_LDRB_M_LSR_IMM_OFF_POSTIND(const u32 i) { OP_LDR_(LDRB, LSR_IMM, sub, 1); }
static int OP_LDRB_P_ASR_IMM_OFF_POSTIND(const u32 i) { OP_LDR_(LDRB, ASR_IMM, add, 1); }
static int OP_LDRB_M_ASR_IMM_OFF_POSTIND(const u32 i) { OP_LDR_(LDRB, ASR_IMM, sub, 1); }
static int OP_LDRB_P_ROR_IMM_OFF_POSTIND(const u32 i) { OP_LDR_(LDRB, ROR_IMM, add, 1); }
static int OP_LDRB_M_ROR_IMM_OFF_POSTIND(const u32 i) { OP_LDR_(LDRB, ROR_IMM, sub, 1); }

// LDRSB
static int OP_LDRSB_P_IMM_OFF(const u32 i) { OP_LDR_(LDRSB, IMM_OFF, add, 0); }
static int OP_LDRSB_M_IMM_OFF(const u32 i) { OP_LDR_(LDRSB, IMM_OFF, sub, 0); }
static int OP_LDRSB_P_REG_OFF(const u32 i) { OP_LDR_(LDRSB, REG_OFF, add, 0); }
static int OP_LDRSB_M_REG_OFF(const u32 i) { OP_LDR_(LDRSB, REG_OFF, sub, 0); }

static int OP_LDRSB_PRE_INDE_P_IMM_OFF(const u32 i) { OP_LDR_(LDRSB, IMM_OFF, add, -1); }
static int OP_LDRSB_PRE_INDE_M_IMM_OFF(const u32 i) { OP_LDR_(LDRSB, IMM_OFF, sub, -1); }
static int OP_LDRSB_PRE_INDE_P_REG_OFF(const u32 i) { OP_LDR_(LDRSB, REG_OFF, add, -1); }
static int OP_LDRSB_PRE_INDE_M_REG_OFF(const u32 i) { OP_LDR_(LDRSB, REG_OFF, sub, -1); }
static int OP_LDRSB_POS_INDE_P_IMM_OFF(const u32 i) { OP_LDR_(LDRSB, IMM_OFF, add, 1); }
static int OP_LDRSB_POS_INDE_M_IMM_OFF(const u32 i) { OP_LDR_(LDRSB, IMM_OFF, sub, 1); }
static int OP_LDRSB_POS_INDE_P_REG_OFF(const u32 i) { OP_LDR_(LDRSB, REG_OFF, add, 1); }
static int OP_LDRSB_POS_INDE_M_REG_OFF(const u32 i) { OP_LDR_(LDRSB, REG_OFF, sub, 1); }

//-----------------------------------------------------------------------------
//   STR
//-----------------------------------------------------------------------------
template<int PROCNUM, int memtype>
static u32 FASTCALL OP_STR(u32 adr, u32 data)
{
	WRITE32(cpu->mem_if->data, adr, data);
	return MMU_aluMemAccessCycles<PROCNUM,32,MMU_AD_WRITE>(2,adr);
}

template<int PROCNUM, int memtype>
static u32 FASTCALL OP_STRH(u32 adr, u32 data)
{
	WRITE16(cpu->mem_if->data, adr, data);
	return MMU_aluMemAccessCycles<PROCNUM,16,MMU_AD_WRITE>(2,adr);
}

template<int PROCNUM, int memtype>
static u32 FASTCALL OP_STRB(u32 adr, u32 data)
{
	WRITE8(cpu->mem_if->data, adr, data);
	return MMU_aluMemAccessCycles<PROCNUM,8,MMU_AD_WRITE>(2,adr);
}

typedef u32 (FASTCALL* OpSTR)(u32, u32);
#define T(op) op<0,0>, op<0,1>, op<0,2>, op<1,0>, op<1,1>, NULL
static const OpSTR STR_tab[2][3]   = { T(OP_STR) };
static const OpSTR STRH_tab[2][3]  = { T(OP_STRH) };
static const OpSTR STRB_tab[2][3]  = { T(OP_STRB) };
#undef T

#define OP_STR_(mem_op, arg, sign_op, writeback) \
	GPVar adr = c.newGP(VARIABLE_TYPE_GPD); \
	GPVar data = c.newGP(VARIABLE_TYPE_GPD); \
	c.mov(adr, reg_pos_ptr(16)); \
	c.mov(data, reg_pos_ptr(12)); \
	arg; \
	if(!rhs_is_imm || *(u32*)&rhs) \
	{ \
		if(writeback == 0) \
			c.sign_op(adr, rhs); \
		else if(writeback < 0) \
		{ \
			c.sign_op(adr, rhs); \
			c.mov(reg_pos_ptr(16), adr); \
		} \
		else if(writeback > 0) \
		{ \
			GPVar tmp_reg = c.newGP(VARIABLE_TYPE_GPD); \
			c.mov(tmp_reg, adr); \
			c.sign_op(tmp_reg, rhs); \
			c.mov(reg_pos_ptr(16), tmp_reg); \
		} \
	} \
	u32 adr_first = sign_op(cpu->R[REG_POS(i,16)], rhs_first); \
	ECall *ctx = c.call((void*)mem_op##_tab[PROCNUM][classify_adr(adr_first,1)]); \
	ctx->setPrototype(ASMJIT_CALL_CONV, FunctionBuilder2<u32, u32, u32>()); \
	ctx->setArgument(0, adr); \
	ctx->setArgument(1, data); \
	ctx->setReturn(bb_cycles); \
	return 1;

static int OP_STR_P_IMM_OFF(const u32 i) { OP_STR_(STR, IMM_OFF_12, add, 0); }
static int OP_STR_M_IMM_OFF(const u32 i) { OP_STR_(STR, IMM_OFF_12, sub, 0); }
static int OP_STR_P_LSL_IMM_OFF(const u32 i) { OP_STR_(STR, LSL_IMM, add, 0); }
static int OP_STR_M_LSL_IMM_OFF(const u32 i) { OP_STR_(STR, LSL_IMM, sub, 0); }
static int OP_STR_P_LSR_IMM_OFF(const u32 i) { OP_STR_(STR, LSR_IMM, add, 0); }
static int OP_STR_M_LSR_IMM_OFF(const u32 i) { OP_STR_(STR, LSR_IMM, sub, 0); }
static int OP_STR_P_ASR_IMM_OFF(const u32 i) { OP_STR_(STR, ASR_IMM, add, 0); }
static int OP_STR_M_ASR_IMM_OFF(const u32 i) { OP_STR_(STR, ASR_IMM, sub, 0); }
static int OP_STR_P_ROR_IMM_OFF(const u32 i) { OP_STR_(STR, ROR_IMM, add, 0); }
static int OP_STR_M_ROR_IMM_OFF(const u32 i) { OP_STR_(STR, ROR_IMM, sub, 0); }

static int OP_STR_P_IMM_OFF_PREIND(const u32 i) { OP_STR_(STR, IMM_OFF_12, add, -1); }
static int OP_STR_M_IMM_OFF_PREIND(const u32 i) { OP_STR_(STR, IMM_OFF_12, sub, -1); }
static int OP_STR_P_LSL_IMM_OFF_PREIND(const u32 i) { OP_STR_(STR, LSL_IMM, add, -1); }
static int OP_STR_M_LSL_IMM_OFF_PREIND(const u32 i) { OP_STR_(STR, LSL_IMM, sub, -1); }
static int OP_STR_P_LSR_IMM_OFF_PREIND(const u32 i) { OP_STR_(STR, LSR_IMM, add, -1); }
static int OP_STR_M_LSR_IMM_OFF_PREIND(const u32 i) { OP_STR_(STR, LSR_IMM, sub, -1); }
static int OP_STR_P_ASR_IMM_OFF_PREIND(const u32 i) { OP_STR_(STR, ASR_IMM, add, -1); }
static int OP_STR_M_ASR_IMM_OFF_PREIND(const u32 i) { OP_STR_(STR, ASR_IMM, sub, -1); }
static int OP_STR_P_ROR_IMM_OFF_PREIND(const u32 i) { OP_STR_(STR, ROR_IMM, add, -1); }
static int OP_STR_M_ROR_IMM_OFF_PREIND(const u32 i) { OP_STR_(STR, ROR_IMM, sub, -1); }
static int OP_STR_P_IMM_OFF_POSTIND(const u32 i) { OP_STR_(STR, IMM_OFF_12, add, 1); }
static int OP_STR_M_IMM_OFF_POSTIND(const u32 i) { OP_STR_(STR, IMM_OFF_12, sub, 1); }
static int OP_STR_P_LSL_IMM_OFF_POSTIND(const u32 i) { OP_STR_(STR, LSL_IMM, add, 1); }
static int OP_STR_M_LSL_IMM_OFF_POSTIND(const u32 i) { OP_STR_(STR, LSL_IMM, sub, 1); }
static int OP_STR_P_LSR_IMM_OFF_POSTIND(const u32 i) { OP_STR_(STR, LSR_IMM, add, 1); }
static int OP_STR_M_LSR_IMM_OFF_POSTIND(const u32 i) { OP_STR_(STR, LSR_IMM, sub, 1); }
static int OP_STR_P_ASR_IMM_OFF_POSTIND(const u32 i) { OP_STR_(STR, ASR_IMM, add, 1); }
static int OP_STR_M_ASR_IMM_OFF_POSTIND(const u32 i) { OP_STR_(STR, ASR_IMM, sub, 1); }
static int OP_STR_P_ROR_IMM_OFF_POSTIND(const u32 i) { OP_STR_(STR, ROR_IMM, add, 1); }
static int OP_STR_M_ROR_IMM_OFF_POSTIND(const u32 i) { OP_STR_(STR, ROR_IMM, sub, 1); }

static int OP_STRH_P_IMM_OFF(const u32 i) { OP_STR_(STRH, IMM_OFF, add, 0); }
static int OP_STRH_M_IMM_OFF(const u32 i) { OP_STR_(STRH, IMM_OFF, sub, 0); }
static int OP_STRH_P_REG_OFF(const u32 i) { OP_STR_(STRH, REG_OFF, add, 0); }
static int OP_STRH_M_REG_OFF(const u32 i) { OP_STR_(STRH, REG_OFF, sub, 0); }

static int OP_STRH_PRE_INDE_P_IMM_OFF(const u32 i) { OP_STR_(STRH, IMM_OFF, add, -1); }
static int OP_STRH_PRE_INDE_M_IMM_OFF(const u32 i) { OP_STR_(STRH, IMM_OFF, sub, -1); }
static int OP_STRH_PRE_INDE_P_REG_OFF(const u32 i) { OP_STR_(STRH, REG_OFF, add, -1); }
static int OP_STRH_PRE_INDE_M_REG_OFF(const u32 i) { OP_STR_(STRH, REG_OFF, sub, -1); }
static int OP_STRH_POS_INDE_P_IMM_OFF(const u32 i) { OP_STR_(STRH, IMM_OFF, add, 1); }
static int OP_STRH_POS_INDE_M_IMM_OFF(const u32 i) { OP_STR_(STRH, IMM_OFF, sub, 1); }
static int OP_STRH_POS_INDE_P_REG_OFF(const u32 i) { OP_STR_(STRH, REG_OFF, add, 1); }
static int OP_STRH_POS_INDE_M_REG_OFF(const u32 i) { OP_STR_(STRH, REG_OFF, sub, 1); }

static int OP_STRB_P_IMM_OFF(const u32 i) { OP_STR_(STRB, IMM_OFF_12, add, 0); }
static int OP_STRB_M_IMM_OFF(const u32 i) { OP_STR_(STRB, IMM_OFF_12, sub, 0); }
static int OP_STRB_P_LSL_IMM_OFF(const u32 i) { OP_STR_(STRB, LSL_IMM, add, 0); }
static int OP_STRB_M_LSL_IMM_OFF(const u32 i) { OP_STR_(STRB, LSL_IMM, sub, 0); }
static int OP_STRB_P_LSR_IMM_OFF(const u32 i) { OP_STR_(STRB, LSR_IMM, add, 0); }
static int OP_STRB_M_LSR_IMM_OFF(const u32 i) { OP_STR_(STRB, LSR_IMM, sub, 0); }
static int OP_STRB_P_ASR_IMM_OFF(const u32 i) { OP_STR_(STRB, ASR_IMM, add, 0); }
static int OP_STRB_M_ASR_IMM_OFF(const u32 i) { OP_STR_(STRB, ASR_IMM, sub, 0); }
static int OP_STRB_P_ROR_IMM_OFF(const u32 i) { OP_STR_(STRB, ROR_IMM, add, 0); }
static int OP_STRB_M_ROR_IMM_OFF(const u32 i) { OP_STR_(STRB, ROR_IMM, sub, 0); }

static int OP_STRB_P_IMM_OFF_PREIND(const u32 i) { OP_STR_(STRB, IMM_OFF_12, add, -1); }
static int OP_STRB_M_IMM_OFF_PREIND(const u32 i) { OP_STR_(STRB, IMM_OFF_12, sub, -1); }
static int OP_STRB_P_LSL_IMM_OFF_PREIND(const u32 i) { OP_STR_(STRB, LSL_IMM, add, -1); }
static int OP_STRB_M_LSL_IMM_OFF_PREIND(const u32 i) { OP_STR_(STRB, LSL_IMM, sub, -1); }
static int OP_STRB_P_LSR_IMM_OFF_PREIND(const u32 i) { OP_STR_(STRB, LSR_IMM, add, -1); }
static int OP_STRB_M_LSR_IMM_OFF_PREIND(const u32 i) { OP_STR_(STRB, LSR_IMM, sub, -1); }
static int OP_STRB_P_ASR_IMM_OFF_PREIND(const u32 i) { OP_STR_(STRB, ASR_IMM, add, -1); }
static int OP_STRB_M_ASR_IMM_OFF_PREIND(const u32 i) { OP_STR_(STRB, ASR_IMM, sub, -1); }
static int OP_STRB_P_ROR_IMM_OFF_PREIND(const u32 i) { OP_STR_(STRB, ROR_IMM, add, -1); }
static int OP_STRB_M_ROR_IMM_OFF_PREIND(const u32 i) { OP_STR_(STRB, ROR_IMM, sub, -1); }
static int OP_STRB_P_IMM_OFF_POSTIND(const u32 i) { OP_STR_(STRB, IMM_OFF_12, add, 1); }
static int OP_STRB_M_IMM_OFF_POSTIND(const u32 i) { OP_STR_(STRB, IMM_OFF_12, sub, 1); }
static int OP_STRB_P_LSL_IMM_OFF_POSTIND(const u32 i) { OP_STR_(STRB, LSL_IMM, add, 1); }
static int OP_STRB_M_LSL_IMM_OFF_POSTIND(const u32 i) { OP_STR_(STRB, LSL_IMM, sub, 1); }
static int OP_STRB_P_LSR_IMM_OFF_POSTIND(const u32 i) { OP_STR_(STRB, LSR_IMM, add, 1); }
static int OP_STRB_M_LSR_IMM_OFF_POSTIND(const u32 i) { OP_STR_(STRB, LSR_IMM, sub, 1); }
static int OP_STRB_P_ASR_IMM_OFF_POSTIND(const u32 i) { OP_STR_(STRB, ASR_IMM, add, 1); }
static int OP_STRB_M_ASR_IMM_OFF_POSTIND(const u32 i) { OP_STR_(STRB, ASR_IMM, sub, 1); }
static int OP_STRB_P_ROR_IMM_OFF_POSTIND(const u32 i) { OP_STR_(STRB, ROR_IMM, add, 1); }
static int OP_STRB_M_ROR_IMM_OFF_POSTIND(const u32 i) { OP_STR_(STRB, ROR_IMM, sub, 1); }

//-----------------------------------------------------------------------------
//   LDRD / STRD
//-----------------------------------------------------------------------------
typedef u32 FASTCALL (*LDRD_STRD_REG)(u32);
template<int PROCNUM, u8 Rnum>
static u32 FASTCALL OP_LDRD_REG(u32 adr)
{
	cpu->R[Rnum] = READ32(cpu->mem_if->data, adr);
	cpu->R[Rnum+1] = READ32(cpu->mem_if->data, adr+4);
	return (MMU_memAccessCycles<PROCNUM,32,MMU_AD_READ>(adr) + MMU_memAccessCycles<PROCNUM,32,MMU_AD_READ>(adr+4));
}
template<int PROCNUM, u8 Rnum>
static u32 FASTCALL OP_STRD_REG(u32 adr)
{
	WRITE32(cpu->mem_if->data, adr, cpu->R[Rnum]);
	WRITE32(cpu->mem_if->data, adr + 4, cpu->R[Rnum + 1]);
	return (MMU_memAccessCycles<PROCNUM,32,MMU_AD_WRITE>(adr) + MMU_memAccessCycles<PROCNUM,32,MMU_AD_WRITE>(adr+4));
}
#define T(op, proc) op<proc,0>, op<proc,1>, op<proc,2>, op<proc,3>, op<proc,4>, op<proc,5>, op<proc,6>, op<proc,7>, op<proc,8>, op<proc,9>, op<proc,10>, op<proc,11>, op<proc,12>, op<proc,13>, op<proc,14>, op<proc,15>
static const LDRD_STRD_REG op_ldrd_tab[2][16] = { {T(OP_LDRD_REG, 0)}, {T(OP_LDRD_REG, 1)} };
static const LDRD_STRD_REG op_strd_tab[2][16] = { {T(OP_STRD_REG, 0)}, {T(OP_STRD_REG, 1)} };
#undef T

static int OP_LDRD_STRD_POST_INDEX(const u32 i) 
{
	u8 Rd_num = REG_POS(i, 12);
	
	if (Rd_num == 14)
	{
		printf("OP_LDRD_STRD_POST_INDEX: use R14!!!!\n");
		return 0; // TODO: exception
	}
	if (Rd_num & 0x1)
	{
		printf("OP_LDRD_STRD_POST_INDEX: ERROR!!!!\n");
		return 0; // TODO: exception
	}
	GPVar Rd = c.newGP(VARIABLE_TYPE_GPD);
	GPVar addr = c.newGP(VARIABLE_TYPE_GPD);

	c.mov(Rd, reg_pos_ptr(16));
	c.mov(addr, reg_pos_ptr(16));

	// I bit - immediate or register
	if (BIT22(i))
	{
		IMM_OFF;
		BIT23(i)?c.add(reg_pos_ptr(16), rhs):c.sub(reg_pos_ptr(16), rhs);
	}
	else
	{
		GPVar idx = c.newGP(VARIABLE_TYPE_GPD);
		c.mov(idx, reg_pos_ptr(0));
		BIT23(i)?c.add(reg_pos_ptr(16), idx):c.sub(reg_pos_ptr(16), idx);
	}

	ECall *ctx = c.call((void*)(BIT5(i) ? op_strd_tab[PROCNUM][Rd_num] : op_ldrd_tab[PROCNUM][Rd_num]));
	ctx->setPrototype(ASMJIT_CALL_CONV, FunctionBuilder1<u32, u32>());
	ctx->setArgument(0, addr);
	ctx->setReturn(bb_cycles);
	emit_MMU_aluMemCycles(3, bb_cycles, 0);
	return 1;
}

static int OP_LDRD_STRD_OFFSET_PRE_INDEX(const u32 i)
{
	u8 Rd_num = REG_POS(i, 12);
	
	if (Rd_num == 14)
	{
		printf("OP_LDRD_STRD_OFFSET_PRE_INDEX: use R14!!!!\n");
		return 0; // TODO: exception
	}
	if (Rd_num & 0x1)
	{
		printf("OP_LDRD_STRD_OFFSET_PRE_INDEX: ERROR!!!!\n");
		return 0; // TODO: exception
	}
	GPVar Rd = c.newGP(VARIABLE_TYPE_GPD);
	GPVar addr = c.newGP(VARIABLE_TYPE_GPD);

	c.mov(Rd, reg_pos_ptr(16));
	c.mov(addr, reg_pos_ptr(16));

	// I bit - immediate or register
	if (BIT22(i))
	{
		IMM_OFF;
		BIT23(i)?c.add(addr, rhs):c.sub(addr, rhs);
	}
	else
		BIT23(i)?c.add(addr, reg_pos_ptr(0)):c.sub(addr, reg_pos_ptr(0));

	if (BIT5(i))		// Store
	{
		ECall *ctx = c.call((void*)op_strd_tab[PROCNUM][Rd_num]);
		ctx->setPrototype(ASMJIT_CALL_CONV, FunctionBuilder1<u32, u32>());
		ctx->setArgument(0, addr);
		ctx->setReturn(bb_cycles);
		if (BIT21(i)) // W bit - writeback
			c.mov(reg_pos_ptr(16), addr);
		emit_MMU_aluMemCycles(3, bb_cycles, 0);
	}
	else				// Load
	{
		if (BIT21(i)) // W bit - writeback
			c.mov(reg_pos_ptr(16), addr);
		ECall *ctx = c.call((void*)op_ldrd_tab[PROCNUM][Rd_num]);
		ctx->setPrototype(ASMJIT_CALL_CONV, FunctionBuilder1<u32, u32>());
		ctx->setArgument(0, addr);
		ctx->setReturn(bb_cycles);
		emit_MMU_aluMemCycles(3, bb_cycles, 0);
	}
	return 1;
}

//-----------------------------------------------------------------------------
//   SWP/SWPB
//-----------------------------------------------------------------------------
template<int PROCNUM>
static u32 FASTCALL op_swp(u32 adr, u32 *Rd, u32 Rs)
{
	u32 tmp = ROR(READ32(cpu->mem_if->data, adr), (adr & 3)<<3);
	WRITE32(cpu->mem_if->data, adr, Rs);
	*Rd = tmp;
	return (MMU_memAccessCycles<PROCNUM,32,MMU_AD_READ>(adr) + MMU_memAccessCycles<PROCNUM,32,MMU_AD_WRITE>(adr));
}
template<int PROCNUM>
static u32 FASTCALL op_swpb(u32 adr, u32 *Rd, u32 Rs)
{
	u32 tmp = READ8(cpu->mem_if->data, adr);
	WRITE8(cpu->mem_if->data, adr, Rs);
	*Rd = tmp;
	return (MMU_memAccessCycles<PROCNUM,8,MMU_AD_READ>(adr) + MMU_memAccessCycles<PROCNUM,8,MMU_AD_WRITE>(adr));
}

typedef u32 FASTCALL (*OP_SWP_SWPB)(u32, u32*, u32);
static const OP_SWP_SWPB op_swp_tab[2][2] = {{ op_swp<0>, op_swp<1> }, { op_swpb<0>, op_swpb<1> }};

static int op_swp_(const u32 i, int b)
{
	GPVar addr = c.newGP(VARIABLE_TYPE_GPD);
	GPVar Rd = c.newGP(VARIABLE_TYPE_GPN);
	GPVar Rs = c.newGP(VARIABLE_TYPE_GPD);
	c.mov(addr, reg_pos_ptr(16));
	c.lea(Rd, reg_pos_ptr(12));
	if(b)
		c.movzx(Rs, reg_pos_ptrB(0));
	else
		c.mov(Rs, reg_pos_ptr(0));
	ECall *ctx = c.call((void*)op_swp_tab[b][PROCNUM]);
	ctx->setPrototype(ASMJIT_CALL_CONV, FunctionBuilder3<u32, u32, u32*, u32>());
	ctx->setArgument(0, addr);
	ctx->setArgument(1, Rd);
	ctx->setArgument(2, Rs);
	ctx->setReturn(bb_cycles);
	emit_MMU_aluMemCycles(4, bb_cycles, 0);
	return 1;
}

static int OP_SWP(const u32 i) { return op_swp_(i, 0); }
static int OP_SWPB(const u32 i) { return op_swp_(i, 1); }

//-----------------------------------------------------------------------------
//   LDMIA / LDMIB / LDMDA / LDMDB / STMIA / STMIB / STMDA / STMDB
//-----------------------------------------------------------------------------
static u32 popcount(u32 x)
{
	uint32_t pop = 0;
	for(; x; x>>=1)
		pop += x&1;
	return pop;
}

static u64 get_reg_list(u32 reg_mask, int dir)
{
	u64 regs = 0;
	for(int j=0; j<16; j++)
	{
		int k = dir<0 ? j : 15-j;
		if(BIT_N(reg_mask,k))
			regs = (regs << 4) | k;
	}
	return regs;
}

#ifdef ASMJIT_X64
// generic needs to spill regs and main doesn't; if it's inlined gcc isn't smart enough to keep the spills out of the common case.
#define LDM_INLINE NOINLINE
#else
// spills either way, and we might as well save codesize by not having separate functions
#define LDM_INLINE INLINE
#endif

template <int PROCNUM, bool store, int dir>
static LDM_INLINE FASTCALL u32 OP_LDM_STM_generic(u32 adr, u64 regs, int n)
{
	u32 cycles = 0;
	adr &= ~3;
	do {
		if(store) _MMU_write32<PROCNUM>(adr, cpu->R[regs&0xF]);
		else cpu->R[regs&0xF] = _MMU_read32<PROCNUM>(adr);
		cycles += MMU_memAccessCycles<PROCNUM,32,store?MMU_AD_WRITE:MMU_AD_READ>(adr);
		adr += 4*dir;
		regs >>= 4;
	} while(--n > 0);
	return cycles;
}

#ifdef ENABLE_ADVANCED_TIMING
#define ADV_CYCLES cycles += MMU_memAccessCycles<PROCNUM,32,store?MMU_AD_WRITE:MMU_AD_READ>(adr);
#else
#define ADV_CYCLES
#endif

template <int PROCNUM, bool store, int dir>
static LDM_INLINE FASTCALL u32 OP_LDM_STM_other(u32 adr, u64 regs, int n)
{
	u32 cycles = 0;
	adr &= ~3;
#ifndef ENABLE_ADVANCED_TIMING
	cycles = n * MMU_memAccessCycles<PROCNUM,32,store?MMU_AD_WRITE:MMU_AD_READ>(adr);
#endif
	do {
		if(PROCNUM==ARMCPU_ARM9)
			if(store) _MMU_ARM9_write32(adr, cpu->R[regs&0xF]);
			else cpu->R[regs&0xF] = _MMU_ARM9_read32(adr);
		else
			if(store) _MMU_ARM7_write32(adr, cpu->R[regs&0xF]);
			else cpu->R[regs&0xF] = _MMU_ARM7_read32(adr);
		ADV_CYCLES;
		adr += 4*dir;
		regs >>= 4;
	} while(--n > 0);
	return cycles;
}

template <int PROCNUM, bool store, int dir, bool null_compiled>
static FORCEINLINE FASTCALL u32 OP_LDM_STM_main(u32 adr, u64 regs, int n, u8 *ptr, u32 cycles)
{
#ifdef ENABLE_ADVANCED_TIMING
	cycles = 0;
#endif
	uintptr_t *func = (uintptr_t *)&JIT_COMPILED_FUNC(adr, PROCNUM);

#define OP(j) { \
	/* no need to zero functions in DTCM, since we can't execute from it */ \
	if(null_compiled && store) \
	{ \
		*func = 0; \
		*(func+1) = 0; \
	} \
	int Rd = ((uintptr_t)regs >> (j*4)) & 0xF; \
	if(store) *(u32*)ptr = cpu->R[Rd]; \
	else cpu->R[Rd] = *(u32*)ptr; \
	ADV_CYCLES; \
	func += 2*dir; \
	adr += 4*dir; \
	ptr += 4*dir; }

	do {
		OP(0);
		if(n == 1) break;
		OP(1);
		if(n == 2) break;
		OP(2);
		if(n == 3) break;
		OP(3);
		regs >>= 16;
		n -= 4;
	} while(n > 0);
	return cycles;
#undef OP
#undef ADV_CYCLES
}

template <int PROCNUM, bool store, int dir>
static u32 FASTCALL OP_LDM_STM(u32 adr, u64 regs, int n)
{
	// TODO use classify_adr?
	u32 cycles;
	u8 *ptr;

	if((adr ^ (adr + (dir>0 ? (n-1)*4 : -15*4))) & ~0x3FFF) // a little conservative, but we don't want to run too many comparisons
	{
		// the memory region spans a page boundary, so we can't factor the address translation out of the loop
		return OP_LDM_STM_generic<PROCNUM, store, dir>(adr, regs, n);
	}
	else if(PROCNUM==ARMCPU_ARM9 && (adr & ~0x3FFF) == MMU.DTCMRegion)
	{
		// don't special-case DTCM cycles, even though that would be both faster and more accurate,
		// because that wouldn't match the non-jitted version with !ACCOUNT_FOR_DATA_TCM_SPEED
		ptr = MMU.ARM9_DTCM + (adr & 0x3FFC);
		cycles = n * MMU_memAccessCycles<PROCNUM,32,store?MMU_AD_WRITE:MMU_AD_READ>(adr);
		if(store)
			return OP_LDM_STM_main<PROCNUM, store, dir, 0>(adr, regs, n, ptr, cycles);
	}
	else if((adr & 0x0F000000) == 0x02000000)
	{
		ptr = MMU.MAIN_MEM + (adr & _MMU_MAIN_MEM_MASK32);
		cycles = n * ((PROCNUM==ARMCPU_ARM9) ? 4 : 2);
	}
	else if(PROCNUM==ARMCPU_ARM7 && !store && (adr & 0xFF800000) == 0x03800000)
	{
		ptr = MMU.ARM7_ERAM + (adr & 0xFFFC);
		cycles = n;
	}
	else if(PROCNUM==ARMCPU_ARM7 && !store && (adr & 0xFF800000) == 0x03000000)
	{
		ptr = MMU.SWIRAM + (adr & 0x7FFC);
		cycles = n;
	}
	else
		return OP_LDM_STM_other<PROCNUM, store, dir>(adr, regs, n);

	return OP_LDM_STM_main<PROCNUM, store, dir, store>(adr, regs, n, ptr, cycles);
}

typedef u32 FASTCALL (*LDMOpFunc)(u32,u64,int);
static const LDMOpFunc op_ldm_stm_tab[2][2][2] = {{
	{ OP_LDM_STM<0,0,-1>, OP_LDM_STM<0,0,+1> },
	{ OP_LDM_STM<0,1,-1>, OP_LDM_STM<0,1,+1> },
},{
	{ OP_LDM_STM<1,0,-1>, OP_LDM_STM<1,0,+1> },
	{ OP_LDM_STM<1,1,-1>, OP_LDM_STM<1,1,+1> },
}};

static void call_ldm_stm(GPVar adr, u32 bitmask, bool store, int dir)
{
	if(bitmask)
	{
		GPVar n = c.newGP(VARIABLE_TYPE_GPD);
		c.mov(n, popcount(bitmask));
#ifdef ASMJIT_X64
		GPVar regs = c.newGP(VARIABLE_TYPE_GPN);
		c.mov(regs, get_reg_list(bitmask, dir));
		ECall *ctx = c.call((void*)op_ldm_stm_tab[PROCNUM][store][dir>0]);
		ctx->setPrototype(ASMJIT_CALL_CONV, FunctionBuilder3<u32, u32, uint64_t, int>());
		ctx->setArgument(0, adr);
		ctx->setArgument(1, regs);
		ctx->setArgument(2, n);
#else
		// same prototype, but we have to handle splitting of a u64 arg manually
		GPVar regs_lo = c.newGP(VARIABLE_TYPE_GPD);
		GPVar regs_hi = c.newGP(VARIABLE_TYPE_GPD);
		c.mov(regs_lo, (u32)get_reg_list(bitmask, dir));
		c.mov(regs_hi, get_reg_list(bitmask, dir) >> 32);
		ECall *ctx = c.call((void*)op_ldm_stm_tab[PROCNUM][store][dir>0]);
		ctx->setPrototype(ASMJIT_CALL_CONV, FunctionBuilder4<u32, u32, u32, u32, int>());
		ctx->setArgument(0, adr);
		ctx->setArgument(1, regs_lo);
		ctx->setArgument(2, regs_hi);
		ctx->setArgument(3, n);
#endif
		ctx->setReturn(bb_cycles);
	}
	else
		c.mov(bb_cycles, 1);
}

static int op_bx(Mem srcreg, bool blx, bool test_thumb);
static int op_bx_thumb(Mem srcreg, bool blx, bool test_thumb);

static int op_ldm_stm(u32 i, bool store, int dir, bool before, bool writeback)
{
	u32 bitmask = i & 0xFFFF;
	u32 pop = popcount(bitmask);

	GPVar adr = c.newGP(VARIABLE_TYPE_GPD);
	c.mov(adr, reg_pos_ptr(16));
	if(before)
		c.add(adr, 4*dir);

	call_ldm_stm(adr, bitmask, store, dir);

	if(BIT15(i) && !store)
	{
		op_bx(reg_ptr(15), 0, PROCNUM == ARMCPU_ARM9);
	}

	if(writeback)
	{
		
		if(store || !(i & (1 << REG_POS(i,16))))
		{
			JIT_COMMENT("--- writeback");
			c.add(reg_pos_ptr(16), 4*dir*pop);
		}
		else 
		{
			u32 bitlist = (~((2 << REG_POS(i,16))-1)) & 0xFFFF;
			if(i & bitlist)
			{
				JIT_COMMENT("--- writeback");
				c.add(adr, 4*dir*(pop-before));
				c.mov(reg_pos_ptr(16), adr);
			}
		}
	}

	emit_MMU_aluMemCycles(store ? 1 : 2, bb_cycles, pop);
	return 1;
}

static int OP_LDMIA(const u32 i) { return op_ldm_stm(i, 0, +1, 0, 0); }
static int OP_LDMIB(const u32 i) { return op_ldm_stm(i, 0, +1, 1, 0); }
static int OP_LDMDA(const u32 i) { return op_ldm_stm(i, 0, -1, 0, 0); }
static int OP_LDMDB(const u32 i) { return op_ldm_stm(i, 0, -1, 1, 0); }
static int OP_LDMIA_W(const u32 i) { return op_ldm_stm(i, 0, +1, 0, 1); }
static int OP_LDMIB_W(const u32 i) { return op_ldm_stm(i, 0, +1, 1, 1); }
static int OP_LDMDA_W(const u32 i) { return op_ldm_stm(i, 0, -1, 0, 1); }
static int OP_LDMDB_W(const u32 i) { return op_ldm_stm(i, 0, -1, 1, 1); }

static int OP_STMIA(const u32 i) { return op_ldm_stm(i, 1, +1, 0, 0); }
static int OP_STMIB(const u32 i) { return op_ldm_stm(i, 1, +1, 1, 0); }
static int OP_STMDA(const u32 i) { return op_ldm_stm(i, 1, -1, 0, 0); }
static int OP_STMDB(const u32 i) { return op_ldm_stm(i, 1, -1, 1, 0); }
static int OP_STMIA_W(const u32 i) { return op_ldm_stm(i, 1, +1, 0, 1); }
static int OP_STMIB_W(const u32 i) { return op_ldm_stm(i, 1, +1, 1, 1); }
static int OP_STMDA_W(const u32 i) { return op_ldm_stm(i, 1, -1, 0, 1); }
static int OP_STMDB_W(const u32 i) { return op_ldm_stm(i, 1, -1, 1, 1); }

static int op_ldm_stm2(u32 i, bool store, int dir, bool before, bool writeback)
{
	u32 bitmask = i & 0xFFFF;
	u32 pop = popcount(bitmask);
	bool bit15 = BIT15(i);

	//printf("ARM%c: %s R%d:%08X, bitmask %02X\n", PROCNUM?'7':'9', (store?"STM":"LDM"), REG_POS(i, 16), cpu->R[REG_POS(i, 16)], bitmask);
	u32 adr_first = cpu->R[REG_POS(i, 16)];

	GPVar adr = c.newGP(VARIABLE_TYPE_GPD);
	GPVar oldmode = c.newGP(VARIABLE_TYPE_GPD);

	c.mov(adr, reg_pos_ptr(16));
	if(before)
		c.add(adr, 4*dir);

	if (!bit15 || store)
	{  
		//if((cpu->CPSR.bits.mode==USR)||(cpu->CPSR.bits.mode==SYS)) { printf("ERROR1\n"); return 1; }
		//oldmode = armcpu_switchMode(cpu, SYS);
		c.mov(oldmode, SYS);
		ECall *ctx = c.call((void*)armcpu_switchMode);
		ctx->setPrototype(CALL_CONV_DEFAULT, FunctionBuilder2<u32, u8*, u8>());
		ctx->setArgument(0, bb_cpu);
		ctx->setArgument(1, oldmode);
		ctx->setReturn(oldmode);
	}

	call_ldm_stm(adr, bitmask, store, dir);

	if(!bit15 || store)
	{
		//armcpu_switchMode(cpu, oldmode);
		ECall *ctx = c.call((void*)armcpu_switchMode);
		ctx->setPrototype(CALL_CONV_DEFAULT, FunctionBuilder2<Void, u8*, u8>());
		ctx->setArgument(0, bb_cpu);
		ctx->setArgument(1, oldmode);
	}
	else
	{
		// TODO: untested
		printf("op_ldm_stm2: used R15\n");
		S_DST_R15;
	}

	// FIXME
	if(writeback)
	{
		if(store || !(i & (1 << REG_POS(i,16))))
			c.add(reg_pos_ptr(16), 4*dir*pop);
		else 
		{
			u32 bitlist = (~((2 << REG_POS(i,16))-1)) & 0xFFFF;
			if(i & bitlist)
			{
				c.add(adr, 4*dir*(pop-before));
				c.mov(reg_pos_ptr(16), adr);
			}
		}
	}

	emit_MMU_aluMemCycles(store ? 1 : 2, bb_cycles, pop);
	return 1;
}

static int OP_LDMIA2(const u32 i) { return op_ldm_stm2(i, 0, +1, 0, 0); }
static int OP_LDMIB2(const u32 i) { return op_ldm_stm2(i, 0, +1, 1, 0); }
static int OP_LDMDA2(const u32 i) { return op_ldm_stm2(i, 0, -1, 0, 0); }
static int OP_LDMDB2(const u32 i) { return op_ldm_stm2(i, 0, -1, 1, 0); }
static int OP_LDMIA2_W(const u32 i) { return op_ldm_stm2(i, 0, +1, 0, 1); }
static int OP_LDMIB2_W(const u32 i) { return op_ldm_stm2(i, 0, +1, 1, 1); }
static int OP_LDMDA2_W(const u32 i) { return op_ldm_stm2(i, 0, -1, 0, 1); }
static int OP_LDMDB2_W(const u32 i) { return op_ldm_stm2(i, 0, -1, 1, 1); }

static int OP_STMIA2(const u32 i) { return op_ldm_stm2(i, 1, +1, 0, 0); }
static int OP_STMIB2(const u32 i) { return op_ldm_stm2(i, 1, +1, 1, 0); }
static int OP_STMDA2(const u32 i) { return op_ldm_stm2(i, 1, -1, 0, 0); }
static int OP_STMDB2(const u32 i) { return op_ldm_stm2(i, 1, -1, 1, 0); }
static int OP_STMIA2_W(const u32 i) { return op_ldm_stm2(i, 1, +1, 0, 1); }
static int OP_STMIB2_W(const u32 i) { return op_ldm_stm2(i, 1, +1, 1, 1); }
static int OP_STMDA2_W(const u32 i) { return op_ldm_stm2(i, 1, -1, 0, 1); }
static int OP_STMDB2_W(const u32 i) { return op_ldm_stm2(i, 1, -1, 1, 1); }

//-----------------------------------------------------------------------------
//   Branch
//-----------------------------------------------------------------------------
#define SIGNEXTEND_11(i) (((s32)i<<21)>>21)
#define SIGNEXTEND_24(i) (((s32)i<<8)>>8)

static int op_b(u32 i, bool bl)
{
	u32 dst = bb_r15 + (SIGNEXTEND_24(i) << 2);
	if(CONDITION(i)==0xF)
	{
		if(bl)
			dst += 2;
		c.or_(cpu_ptr_byte(CPSR, 0), 1<<5);
	}
	if(bl || CONDITION(i)==0xF)
		c.mov(reg_ptr(14), bb_next_instruction);

	c.mov(cpu_ptr(instruct_adr), dst);
	return 1;
}

static int OP_B(const u32 i) { return op_b(i, 0); }
static int OP_BL(const u32 i) { return op_b(i, 1); }

static int op_bx(Mem srcreg, bool blx, bool test_thumb)
{
	GPVar dst = c.newGP(VARIABLE_TYPE_GPD);
	c.mov(dst, srcreg);

	if(test_thumb)
	{
		GPVar mask = c.newGP(VARIABLE_TYPE_GPD);
		GPVar thumb = dst;
		dst = c.newGP(VARIABLE_TYPE_GPD);
		c.mov(dst, thumb);
		c.and_(thumb, 1);
		c.lea(mask, ptr_abs((void*)0xFFFFFFFC, thumb.r64(), TIMES_2));
		c.shl(thumb, 5);
		c.or_(cpu_ptr_byte(CPSR, 0), thumb.r8Lo());
		c.and_(dst, mask);
	}
	else
		c.and_(dst, 0xFFFFFFFC);

	if(blx)
		c.mov(reg_ptr(14), bb_next_instruction);
	c.mov(cpu_ptr(instruct_adr), dst);
	return 1;
}

//TODO: exeption when Rm=PC
static int OP_BX(const u32 i) { return op_bx(reg_pos_ptr(0), 0, 1); }
static int OP_BLX_REG(const u32 i) { return op_bx(reg_pos_ptr(0), 1, 1); }

//-----------------------------------------------------------------------------
//   CLZ
//-----------------------------------------------------------------------------
static int OP_CLZ(const u32 i)
{
	GPVar res = c.newGP(VARIABLE_TYPE_GPD);
	c.mov(res, 0x3F);
	c.bsr(res, reg_pos_ptr(0));
	c.xor_(res, 0x1F);
	c.mov(reg_pos_ptr(12), res);
	
	return 1;
}

//-----------------------------------------------------------------------------
//   MCR / MRC
//-----------------------------------------------------------------------------
#define maskPrecalc \
{ \
	ECall* ctxM = c.call((void*)maskPrecalc); \
	ctxM->setPrototype(ASMJIT_CALL_CONV, FunctionBuilder0<Void>()); \
}
static int OP_MCR(const u32 i)
{
	if (PROCNUM == ARMCPU_ARM7) return 0;

	u32 cpnum = REG_POS(i, 8);
	if(cpnum != 15)
	{
		// TODO - exception?
		printf("JIT: MCR P%i, 0, R%i, C%i, C%i, %i, %i (don't allocated coprocessor)\n", 
			cpnum, REG_POS(i, 12), REG_POS(i, 16), REG_POS(i, 0), (i>>21)&0x7, (i>>5)&0x7);
		return 2;
	}
	if (REG_POS(i, 12) == 15)
	{
		printf("JIT: MCR Rd=R15\n");
		return 2;
	}

	u8 CRn =  REG_POS(i, 16);		// Cn
	u8 CRm =  REG_POS(i, 0);		// Cm
	u8 opcode1 = ((i>>21)&0x7);		// opcode1
	u8 opcode2 = ((i>>5)&0x7);		// opcode2

	GPVar bb_cp15 = c.newGP(VARIABLE_TYPE_GPN);
	GPVar data = c.newGP(VARIABLE_TYPE_GPD);
	c.mov(data, reg_pos_ptr(12));
	c.mov(bb_cp15, (uintptr_t)&cp15);

	bool bUnknown = false;
	switch(CRn)
	{
		case 1:
			if((opcode1==0) && (opcode2==0) && (CRm==0))
			{
				GPVar tmp = c.newGP(VARIABLE_TYPE_GPD);
				// On the NDS bit0,2,7,12..19 are R/W, Bit3..6 are always set, all other bits are always zero.
				//MMU.ARM9_RW_MODE = BIT7(val);
				GPVar bb_mmu = c.newGP(VARIABLE_TYPE_GPN);
				c.mov(bb_mmu, (uintptr_t)&MMU);
				Mem rwmode = mmu_ptr_byte(ARM9_RW_MODE);
				Mem ldtbit = cpu_ptr_byte(LDTBit, 0);
				c.bt(data, 7);
				c.setc(rwmode);
				//cpu->intVector = 0xFFFF0000 * (BIT13(val));
				GPVar vec = c.newGP(VARIABLE_TYPE_GPD);
				c.mov(tmp, 0xFFFF0000);
				c.xor_(vec, vec);
				c.bt(data, 13);
				c.cmovc(vec, tmp);
				c.mov(cpu_ptr(intVector), vec);
				//cpu->LDTBit = !BIT15(val); //TBit
				c.bt(data, 1);
				c.setnc(ldtbit);
				//ctrl = (val & 0x000FF085) | 0x00000078;
				c.and_(data, 0x000FF085);
				c.or_(data, 0x00000078);
				c.mov(cp15_ptr(ctrl), data);
				break;
			}
			bUnknown = true;
			break;
		case 2:
			if((opcode1==0) && (CRm==0))
			{
				switch(opcode2)
				{
					case 0:
						// DCConfig = val;
						c.mov(cp15_ptr(DCConfig), data);
						break;
					case 1:
						// ICConfig = val;
						c.mov(cp15_ptr(ICConfig), data);
						break;
					default:
						bUnknown = true;
						break;
				}
				break;
			}
			bUnknown = true;
			break;
		case 3:
			if((opcode1==0) && (opcode2==0) && (CRm==0))
			{
				//writeBuffCtrl = val;
				c.mov(cp15_ptr(writeBuffCtrl), data);
				break;
			}
			bUnknown = true;
			break;
		case 5:
			if((opcode1==0) && (CRm==0))
			{
				switch(opcode2)
				{
					case 2:
						//DaccessPerm = val;
						c.mov(cp15_ptr(DaccessPerm), data);
						maskPrecalc;
						break;
					case 3:
						//IaccessPerm = val;
						c.mov(cp15_ptr(IaccessPerm), data);
						maskPrecalc;
						break;
					default:
						bUnknown = true;
						break;
				}
			}
			bUnknown = true;
			break;
		case 6:
			if((opcode1==0) && (opcode2==0))
			{
				switch(CRm)
				{
					case 0:
						//protectBaseSize0 = val;
						c.mov(cp15_ptr(protectBaseSize0), data);
						maskPrecalc;
						break;
					case 1:
						//protectBaseSize1 = val;
						c.mov(cp15_ptr(protectBaseSize1), data);
						maskPrecalc;
						break;
					case 2:
						//protectBaseSize2 = val;
						c.mov(cp15_ptr(protectBaseSize2), data);
						maskPrecalc;
						break;
					case 3:
						//protectBaseSize3 = val;
						c.mov(cp15_ptr(protectBaseSize3), data);
						maskPrecalc;
						break;
					case 4:
						//protectBaseSize4 = val;
						c.mov(cp15_ptr(protectBaseSize4), data);
						maskPrecalc;
						break;
					case 5:
						//protectBaseSize5 = val;
						c.mov(cp15_ptr(protectBaseSize5), data);
						maskPrecalc;
						break;
					case 6:
						//protectBaseSize6 = val;
						c.mov(cp15_ptr(protectBaseSize6), data);
						maskPrecalc;
						break;
					case 7:
						//protectBaseSize7 = val;
						c.mov(cp15_ptr(protectBaseSize7), data);
						maskPrecalc;
						break;
					default:
						bUnknown = true;
						break;
				}
			}
			bUnknown = true;
			break;
		case 7:
			if((CRm==0)&&(opcode1==0)&&((opcode2==4)))
			{
				//CP15wait4IRQ;
				c.mov(cpu_ptr(waitIRQ), true);
				c.mov(cpu_ptr(halt_IE_and_IF), true);
				//IME set deliberately omitted: only SWI sets IME to 1
				break;
			}
			bUnknown = true;
			break;
		case 9:
			if((opcode1==0))
			{
				switch(CRm)
				{
					case 0:
						switch(opcode2)
						{
							case 0:
								//DcacheLock = val;
								c.mov(cp15_ptr(DcacheLock), data);
								break;
							case 1:
								//IcacheLock = val;
								c.mov(cp15_ptr(IcacheLock), data);
								break;
							default:
								bUnknown = true;
								break;
						}
					case 1:
						switch(opcode2)
						{
							case 0:
								//MMU.DTCMRegion = DTCMRegion = val & 0x0FFFF000;
								c.and_(data, 0x0FFFF000);
								c.mov(cp15_ptr(DTCMRegion), data);
								break;
							case 1:
								{
								//ITCMRegion = val;
								//ITCM base is not writeable!
								GPVar bb_mmu = c.newGP(VARIABLE_TYPE_GPN);
								c.mov(bb_mmu, (uintptr_t)&MMU);
								c.mov(mmu_ptr(ITCMRegion), 0);
								c.mov(cp15_ptr(ITCMRegion), data);
								}
								break;
							default:
								bUnknown = true;
								break;
						}
				}
				break;
			}
			bUnknown = true;
			break;
		case 13:
			bUnknown = true;
			break;
		case 15:
			bUnknown = true;
			break;
		default:
			bUnknown = true;
			break;
	}

	if (bUnknown)
	{
		//printf("Unknown MCR command: MRC P15, 0, R%i, C%i, C%i, %i, %i\n", REG_POS(i, 12), CRn, CRm, opcode1, opcode2);
		return 1;
	}

	return 1;
}
static int OP_MRC(const u32 i)
{
	if (PROCNUM == ARMCPU_ARM7) return 0;

	u32 cpnum = REG_POS(i, 8);
	if(cpnum != 15)
	{
		printf("MRC P%i, 0, R%i, C%i, C%i, %i, %i (don't allocated coprocessor)\n", cpnum, REG_POS(i, 12), REG_POS(i, 16), REG_POS(i, 0), (i>>21)&0x7, (i>>5)&0x7);
		return 2;
	}

	u8 CRn =  REG_POS(i, 16);		// Cn
	u8 CRm =  REG_POS(i, 0);		// Cm
	u8 opcode1 = ((i>>21)&0x7);		// opcode1
	u8 opcode2 = ((i>>5)&0x7);		// opcode2

	GPVar bb_cp15 = c.newGP(VARIABLE_TYPE_GPN);
	GPVar data = c.newGP(VARIABLE_TYPE_GPD);

	c.mov(bb_cp15, (uintptr_t)&cp15);
	
	bool bUnknown = false;
	switch(CRn)
	{
		case 0:
			if((opcode1 == 0)&&(CRm==0))
			{
				switch(opcode2)
				{
					case 1:
						// *R = cacheType;
						c.mov(data, cp15_ptr(cacheType));
						break;
					case 2:
						// *R = TCMSize;
						c.mov(data, cp15_ptr(TCMSize));
						break;
					default:		// FIXME
						// *R = IDCode;
						c.mov(data, cp15_ptr(IDCode));
						break;
				}
				break;
			}
			bUnknown = true;
			break;
		
		case 1:
			if((opcode1==0) && (opcode2==0) && (CRm==0))
			{
				// *R = ctrl;
				c.mov(data, cp15_ptr(ctrl));
				break;
			}
			bUnknown = true;
			break;
		
		case 2:
			if((opcode1==0) && (CRm==0))
			{
				switch(opcode2)
				{
					case 0:
						// *R = DCConfig;
						c.mov(data, cp15_ptr(DCConfig));
						break;
					case 1:
						// *R = ICConfig;
						c.mov(data, cp15_ptr(ICConfig));
						break;
					default:
						bUnknown = true;
						break;
				}
				break;
			}
			bUnknown = true;
			break;
			
		case 3:
			if((opcode1==0) && (opcode2==0) && (CRm==0))
			{
				// *R = writeBuffCtrl;
				c.mov(data, cp15_ptr(writeBuffCtrl));
				break;
			}
			bUnknown = true;
			break;
			
		case 5:
			if((opcode1==0) && (CRm==0))
			{
				switch(opcode2)
				{
					case 2:
						// *R = DaccessPerm;
						c.mov(data, cp15_ptr(DaccessPerm));
						break;
					case 3:
						// *R = IaccessPerm;
						c.mov(data, cp15_ptr(IaccessPerm));
						break;
					default:
						bUnknown = true;
						break;
				}
				break;
			}
			bUnknown = true;
			break;
			
		case 6:
			if((opcode1==0) && (opcode2==0))
			{
				switch(CRm)
				{
					case 0:
						// *R = protectBaseSize0;
						c.mov(data, cp15_ptr(protectBaseSize0));
						break;
					case 1:
						// *R = protectBaseSize1;
						c.mov(data, cp15_ptr(protectBaseSize1));
						break;
					case 2:
						// *R = protectBaseSize2;
						c.mov(data, cp15_ptr(protectBaseSize2));
						break;
					case 3:
						// *R = protectBaseSize3;
						c.mov(data, cp15_ptr(protectBaseSize3));
						break;
					case 4:
						// *R = protectBaseSize4;
						c.mov(data, cp15_ptr(protectBaseSize4));
						break;
					case 5:
						// *R = protectBaseSize5;
						c.mov(data, cp15_ptr(protectBaseSize5));
						break;
					case 6:
						// *R = protectBaseSize6;
						c.mov(data, cp15_ptr(protectBaseSize6));
						break;
					case 7:
						// *R = protectBaseSize7;
						c.mov(data, cp15_ptr(protectBaseSize7));
						break;
					default:
						bUnknown = true;
						break;
				}
				break;
			}
			bUnknown = true;
			break;
			
		case 7:
			bUnknown = true;
			break;

		case 9:
			if(opcode1 == 0)
			{
				switch(CRm)
				{
					case 0:
						switch(opcode2)
						{
							case 0:
								//*R = DcacheLock;
								c.mov(data, cp15_ptr(DcacheLock));
								break;
							case 1:
								//*R = IcacheLock;
								c.mov(data, cp15_ptr(IcacheLock));
								break;
							default:
								bUnknown = true;
								break;
						}

					case 1:
						switch(opcode2)
						{
							case 0:
								//*R = DTCMRegion;
								c.mov(data, cp15_ptr(DTCMRegion));
								break;
							case 1:
								//*R = ITCMRegion;
								c.mov(data, cp15_ptr(ITCMRegion));
								break;
							default:
								bUnknown = true;
								break;
						}
				}
				//
				break;
			}
			bUnknown = true;
			break;
			
		case 13:
			bUnknown = true;
			break;
			
		case 15:
			bUnknown = true;
			break;

		default:
			bUnknown = true;
			break;
	}

	if (bUnknown)
	{
		//printf("Unknown MRC command: MRC P15, 0, R%i, C%i, C%i, %i, %i\n", REG_POS(i, 12), CRn, CRm, opcode1, opcode2);
		return 1;
	}

	if (REG_POS(i, 12) == 15)	// set NZCV
	{
		//CPSR.bits.N = BIT31(data);
		//CPSR.bits.Z = BIT30(data);
		//CPSR.bits.C = BIT29(data);
		//CPSR.bits.V = BIT28(data);
		c.and_(data, 0xF0000000);
		c.and_(cpu_ptr(CPSR), 0x0FFFFFFF);
		c.or_(cpu_ptr(CPSR), data);
	}
	else
		c.mov(reg_pos_ptr(12), data);

	return 1;
}

u32 op_swi(u8 swinum)
{
	if(cpu->swi_tab)
	{
#if defined(_M_X64) || defined(__x86_64__)
		// TODO:
		return 0;
#else
		ECall *ctx = c.call((void*)ARM_swi_tab[PROCNUM][swinum]);
		ctx->setPrototype(ASMJIT_CALL_CONV, FunctionBuilder0<u32>());
		ctx->setReturn(bb_cycles);
		c.add(bb_cycles, 3);
		return 1;
#endif
	}

	GPVar oldCPSR = c.newGP(VARIABLE_TYPE_GPD);
	GPVar mode = c.newGP(VARIABLE_TYPE_GPD);
	Mem CPSR = cpu_ptr(CPSR.val);
	JIT_COMMENT("store CPSR to x86 stack");
	c.mov(oldCPSR, CPSR);
	JIT_COMMENT("enter SVC mode");
	c.mov(mode, imm(SVC));
	ECall* ctx = c.call((void*)armcpu_switchMode);
	ctx->setPrototype(ASMJIT_CALL_CONV, FunctionBuilder2<Void, void*, u8>());
	ctx->setArgument(0, bb_cpu);
	ctx->setArgument(1, mode);
	c.unuse(mode);
	JIT_COMMENT("store next instruction address to R14");
	c.mov(reg_ptr(14), bb_next_instruction);
	JIT_COMMENT("save old CPSR as new SPSR");
	c.mov(cpu_ptr(SPSR.val), oldCPSR);
	JIT_COMMENT("CPSR: clear T, set I");
	GPVar _cpsr = c.newGP(VARIABLE_TYPE_GPD);
	c.mov(_cpsr, CPSR);
	c.and_(_cpsr, ~(1 << 5));	/* clear T */
	c.or_(_cpsr, (1 << 7));		/* set I */
	c.mov(CPSR, _cpsr);
	c.unuse(_cpsr);
	JIT_COMMENT("set next instruction");
	c.mov(cpu_ptr(next_instruction), imm(cpu->intVector+0x08));
	
	return 1;
}

static int OP_SWI(const u32 i) { return op_swi((i >> 16) & 0x1F); }

//-----------------------------------------------------------------------------
//   BKPT
//-----------------------------------------------------------------------------
static int OP_BKPT(const u32 i) { printf("JIT: unimplemented OP_BKPT\n"); return 0; }

//-----------------------------------------------------------------------------
//   THUMB
//-----------------------------------------------------------------------------
#define SET_NZCV(sign) \
	ECall* ctx = c.call(op_cmp[PROCNUM][sign]); \
	ctx->setPrototype(ASMJIT_CALL_CONV, FunctionBuilder0<Void>());

#define SET_NZCV_ZERO_CV { \
	GPVar x = c.newGP(VARIABLE_TYPE_GPD); \
	GPVar y = c.newGP(VARIABLE_TYPE_GPD); \
	c.sets(x.r8Lo()); \
	c.setz(y.r8Lo()); \
	c.lea(x, ptr(y.r64(), x.r64(), TIMES_2)); \
	c.movzx(y, flags_ptr); \
	c.shl(x, 6); \
	c.and_(y, 0xF); \
	c.or_(x, y); \
	c.mov(flags_ptr, x.r8Lo()); }

#define SET_NZC_SHIFTS_ZERO(cf) { \
	c.and_(flags_ptr, 0x1F); \
	if(cf) \
	{ \
		c.shl(rcf, 5); \
		c.or_(rcf, (1<<6)); \
		c.or_(flags_ptr, rcf.r8Lo()); \
	} \
	else \
		c.or_(flags_ptr, (1<<6)); }

//-----------------------------------------------------------------------------
#define OP_SHIFTS_IMM(x86inst) \
	GPVar rcf = c.newGP(VARIABLE_TYPE_GPD); \
	const u32 rhs = ((i>>6) & 0x1F); \
	if (_REG_NUM(i, 0) == _REG_NUM(i, 3)) \
		c.x86inst(reg_pos_thumb(0), rhs); \
	else \
	{ \
		GPVar lhs = c.newGP(VARIABLE_TYPE_GPD); \
		c.mov(lhs, reg_pos_thumb(3)); \
		c.x86inst(lhs, rhs); \
		c.mov(reg_pos_thumb(0), lhs); \
		c.unuse(lhs); \
	} \
	c.setc(rcf.r8Lo()); \
	SET_NZC; \
	return 1;

#define OP_SHIFTS_REG(x86inst, bit) \
	GPVar imm = c.newGP(VARIABLE_TYPE_GPN); \
	GPVar rcf = c.newGP(VARIABLE_TYPE_GPD); \
	Label __eq32 = c.newLabel(); \
	Label __ls32 = c.newLabel(); \
	Label __zero = c.newLabel(); \
	Label __done = c.newLabel(); \
	\
	c.mov(imm, reg_pos_thumb(3)); \
	c.and_(imm, 0xFF); \
	c.jz(__zero); \
	c.cmp(imm, 32); \
	c.jl(__ls32); \
	c.je(__eq32); \
	/* imm > 32 */ \
	c.mov(reg_pos_thumb(0), 0); \
	SET_NZC_SHIFTS_ZERO(0); \
	c.jmp(__done); \
	/* imm == 32 */ \
	c.bind(__eq32); \
	c.bt(reg_pos_thumb(0), bit); \
	c.setc(rcf.r8Lo()); \
	c.mov(reg_pos_thumb(0), 0); \
	SET_NZC_SHIFTS_ZERO(1); \
	c.jmp(__done); \
	/* imm == 0 */ \
	c.bind(__zero); \
	c.cmp(reg_pos_thumb(0), 0); \
	SET_NZ; \
	c.jmp(__done); \
	/* imm < 32 */ \
	c.bind(__ls32); \
	c.x86inst(reg_pos_thumb(0), imm); \
	c.setc(rcf.r8Lo()); \
	SET_NZC; \
	c.bind(__done); \
	return 1;

#define OP_LOGIC(x86inst, _conv) \
	GPVar rhs = c.newGP(VARIABLE_TYPE_GPD); \
	c.mov(rhs, reg_pos_thumb(3)); \
	if (_conv==1) c.not_(rhs); \
	c.x86inst(reg_pos_thumb(0), rhs); \
	SET_NZ; \
	return 1;

//-----------------------------------------------------------------------------
//   LSL / LSR / ASR / ROR
//-----------------------------------------------------------------------------
static int OP_LSL_0(const u32 i) 
{
	if (_REG_NUM(i, 0) == _REG_NUM(i, 3))
		c.cmp(reg_pos_thumb(0), 0);
	else
	{
		GPVar rhs = c.newGP(VARIABLE_TYPE_GPD);
		c.mov(rhs, reg_pos_thumb(3));
		c.mov(reg_pos_thumb(0), rhs);
		c.cmp(rhs, 0);
	}
	SET_NZ;
	return 1;
}
static int OP_LSL(const u32 i) { OP_SHIFTS_IMM(shl); }
static int OP_LSL_REG(const u32 i) { OP_SHIFTS_REG(shl, 0); }
static int OP_LSR_0(const u32 i) 
{
	GPVar rcf = c.newGP(VARIABLE_TYPE_GPD);
	c.bt(reg_pos_thumb(3), 31);
	c.setc(rcf.r8Lo());
	SET_NZC_SHIFTS_ZERO(1);
	c.mov(reg_pos_thumb(0), 0);
	return 1;
}
static int OP_LSR(const u32 i) { OP_SHIFTS_IMM(shr); }
static int OP_LSR_REG(const u32 i) { OP_SHIFTS_REG(shr, 31); }
static int OP_ASR_0(const u32 i)
{
	GPVar rcf = c.newGP(VARIABLE_TYPE_GPD);
	GPVar rhs = c.newGP(VARIABLE_TYPE_GPD);
	c.mov(rhs, reg_pos_thumb(3));
	c.bt(rhs, 31);
	c.setc(rcf.r8Lo());
	c.sar(rhs, 31);
	c.mov(reg_pos_thumb(0), rhs);
	SET_NZC;
	return 1;
}
static int OP_ASR(const u32 i) { OP_SHIFTS_IMM(sar); }
static int OP_ASR_REG(const u32 i) 
{
	GPVar imm = c.newGP(VARIABLE_TYPE_GPN);
	GPVar rcf = c.newGP(VARIABLE_TYPE_GPD);
	GPVar rhs = c.newGP(VARIABLE_TYPE_GPD);
	Label __ls32 = c.newLabel();
	Label __zero = c.newLabel();
	Label __done = c.newLabel();

	c.mov(imm, reg_pos_thumb(3));
	c.and_(imm, 0xFF);
	c.jz(__zero);
	c.cmp(imm, 32);
	c.jl(__ls32);
	/* imm > 32 */
	c.mov(rcf, reg_pos_thumb(0));
	c.shr(rcf, 31);
	c.sar(reg_pos_thumb(0), 31);
	SET_NZC;
	c.jmp(__done);
	/* imm == 0 */
	c.bind(__zero);
	c.cmp(reg_pos_thumb(0), 0);
	SET_NZ;
	c.jmp(__done);
	/* imm < 32 */
	c.bind(__ls32);
	c.sar(reg_pos_thumb(0), imm);
	c.setc(rcf.r8Lo());
	SET_NZC;
	c.bind(__done);

	return 1;
}

//-----------------------------------------------------------------------------
//   ROR
//-----------------------------------------------------------------------------
static int OP_ROR_REG(const u32 i)
{
	GPVar imm = c.newGP(VARIABLE_TYPE_GPN);
	GPVar rcf = c.newGP(VARIABLE_TYPE_GPD);
	Label __zero = c.newLabel();
	Label __zero_1F = c.newLabel();
	Label __done = c.newLabel();

	c.mov(imm, reg_pos_thumb(3));
	c.and_(imm, 0xFF);
	c.jz(__zero);
	c.and_(imm, 0x1F);
	c.jz(__zero_1F);
	c.ror(reg_pos_thumb(0), imm);
	c.setc(rcf.r8Lo());
	SET_NZC;
	c.jmp(__done);
	/* imm & 0x1F == 0 */
	c.bind(__zero_1F);
	c.cmp(reg_pos_thumb(0), 0);
	c.sets(rcf.r8Lo());
	SET_NZC;
	c.jmp(__done);
	/* imm == 0 */
	c.bind(__zero);
	c.cmp(reg_pos_thumb(0), 0);
	SET_NZ;
	c.bind(__done);

	return 1;
}

//-----------------------------------------------------------------------------
//   AND / ORR / EOR / BIC
//-----------------------------------------------------------------------------
static int OP_AND(const u32 i) { OP_LOGIC(and_, 0); }
static int OP_ORR(const u32 i) { OP_LOGIC(or_,  0); }
static int OP_EOR(const u32 i) { OP_LOGIC(xor_, 0); }
static int OP_BIC(const u32 i) { OP_LOGIC(and_, 1); }

//-----------------------------------------------------------------------------
//   NEG
//-----------------------------------------------------------------------------
static int OP_NEG(const u32 i)
{
	if (_REG_NUM(i, 0) == _REG_NUM(i, 3))
		c.neg(reg_pos_thumb(0));
	else
	{
		GPVar tmp = c.newGP(VARIABLE_TYPE_GPD);
		c.mov(tmp, reg_pos_thumb(3));
		c.neg(tmp);
		c.mov(reg_pos_thumb(0), tmp);
	}
	SET_NZCV(1);
	return 1;
}

//-----------------------------------------------------------------------------
//   ADD
//-----------------------------------------------------------------------------
static int OP_ADD_IMM3(const u32 i) 
{
	u32 imm3 = (i >> 6) & 0x07;

	if (imm3 == 0)	// mov 2
	{
		GPVar tmp = c.newGP(VARIABLE_TYPE_GPD);
		c.mov(tmp, reg_pos_thumb(3));
		c.mov(reg_pos_thumb(0), tmp);
		c.cmp(tmp, 0);
		SET_NZCV_ZERO_CV;
		return 1;
	}
	if (_REG_NUM(i, 0) == _REG_NUM(i, 3))
	{
		c.add(reg_pos_thumb(0), imm3);
	}
	else
	{
		GPVar tmp = c.newGP(VARIABLE_TYPE_GPD);
		c.mov(tmp, reg_pos_thumb(3));
		c.add(tmp, imm3);
		c.mov(reg_pos_thumb(0), tmp);
	}
	SET_NZCV(0);
	return 1;
}
static int OP_ADD_IMM8(const u32 i) 
{
	c.add(reg_pos_thumb(8), (i & 0xFF));
	SET_NZCV(0);

	return 1; 
}
static int OP_ADD_REG(const u32 i) 
{
	//cpu->R[REG_NUM(i, 0)] = cpu->R[REG_NUM(i, 3)] + cpu->R[REG_NUM(i, 6)];
	if (_REG_NUM(i, 0) == _REG_NUM(i, 3))
	{
		GPVar tmp = c.newGP(VARIABLE_TYPE_GPD);
		c.mov(tmp, reg_pos_thumb(6));
		c.add(reg_pos_thumb(0), tmp);
	}
	else
		if (_REG_NUM(i, 0) == _REG_NUM(i, 6))
		{
			GPVar tmp = c.newGP(VARIABLE_TYPE_GPD);
			c.mov(tmp, reg_pos_thumb(3));
			c.add(reg_pos_thumb(0), tmp);
		}
		else
			{
				GPVar tmp = c.newGP(VARIABLE_TYPE_GPD);
				c.mov(tmp, reg_pos_thumb(3));
				c.add(tmp, reg_pos_thumb(6));
				c.mov(reg_pos_thumb(0), tmp);
			}
	SET_NZCV(0);
	return 1; 
}
static int OP_ADD_SPE(const u32 i)
{
	u32 Rd = _REG_NUM(i, 0) | ((i>>4)&8);
	//cpu->R[Rd] += cpu->R[REG_POS(i, 3)];
	GPVar tmp = c.newGP(VARIABLE_TYPE_GPD);
	c.mov(tmp, reg_ptr(Rd));
	c.add(tmp, reg_pos_ptr(3));
	c.mov(reg_ptr(Rd), tmp);
	
	if(Rd==15)
		c.mov(cpu_ptr(next_instruction), tmp);
		
	return 1;
}

static int OP_ADD_2PC(const u32 i)
{
	u32 imm = ((i&0xFF)<<2);
	c.mov(reg_pos_thumb(8), (bb_r15 & 0xFFFFFFFC) + imm);
	return 1;
}

static int OP_ADD_2SP(const u32 i)
{
	u32 imm = ((i&0xFF)<<2);
	//cpu->R[REG_NUM(i, 8)] = cpu->R[13] + ((i&0xFF)<<2);
	GPVar tmp = c.newGP(VARIABLE_TYPE_GPD);
	c.mov(tmp, reg_ptr(13));
	if (imm) c.add(tmp, imm);
	c.mov(reg_pos_thumb(8), tmp);
	
	return 1;
}

//-----------------------------------------------------------------------------
//   SUB
//-----------------------------------------------------------------------------
static int OP_SUB_IMM3(const u32 i)
{
	u32 imm3 = (i >> 6) & 0x07;

	// cpu->R[REG_NUM(i, 0)] = cpu->R[REG_NUM(i, 3)] - imm3;
	if (_REG_NUM(i, 0) == _REG_NUM(i, 3))
	{
		c.sub(reg_pos_thumb(0), imm3);
	}
	else
	{
		GPVar tmp = c.newGP(VARIABLE_TYPE_GPD);
		c.mov(tmp, reg_pos_thumb(3));
		c.sub(tmp, imm3);
		c.mov(reg_pos_thumb(0), tmp);
	}
	SET_NZCV(1);
	return 1;
}
static int OP_SUB_IMM8(const u32 i)
{
	//cpu->R[REG_NUM(i, 8)] -= imm8;
	c.sub(reg_pos_thumb(8), (i & 0xFF));
	SET_NZCV(1);
	return 1; 
}
static int OP_SUB_REG(const u32 i)
{
	// cpu->R[REG_NUM(i, 0)] = cpu->R[REG_NUM(i, 3)] - cpu->R[REG_NUM(i, 6)];
	if (_REG_NUM(i, 0) == _REG_NUM(i, 3))
	{
		GPVar tmp = c.newGP(VARIABLE_TYPE_GPD);
		c.mov(tmp, reg_pos_thumb(6));
		c.sub(reg_pos_thumb(0), tmp);
	}
	else
	{
		GPVar tmp = c.newGP(VARIABLE_TYPE_GPD);
		c.mov(tmp, reg_pos_thumb(3));
		c.sub(tmp, reg_pos_thumb(6));
		c.mov(reg_pos_thumb(0), tmp);
	}
	SET_NZCV(1);
	return 1; 
}

//-----------------------------------------------------------------------------
//   ADC
//-----------------------------------------------------------------------------
static int OP_ADC_REG(const u32 i)
{
	GPVar tmp = c.newGP(VARIABLE_TYPE_GPD);
	c.mov(tmp, reg_pos_thumb(3));
	GET_CARRY(0);
	c.adc(reg_pos_thumb(0), tmp);
	SET_NZCV(0);
	return 1;
}

//-----------------------------------------------------------------------------
//   SBC
//-----------------------------------------------------------------------------
static int OP_SBC_REG(const u32 i)
{
	GPVar tmp = c.newGP(VARIABLE_TYPE_GPD);
	c.mov(tmp, reg_pos_thumb(3));
	GET_CARRY(1);
	c.sbb(reg_pos_thumb(0), tmp);
	SET_NZCV(1);
	return 1;
}

//-----------------------------------------------------------------------------
//   MOV / MVN
//-----------------------------------------------------------------------------
static int OP_MOV_IMM8(const u32 i)
{
	c.mov(reg_pos_thumb(8), (i & 0xFF));
	c.cmp(reg_pos_thumb(8), 0);
	SET_NZ;
	return 1;
}

static int OP_MOV_SPE(const u32 i)
{
	u32 Rd = _REG_NUM(i, 0) | ((i>>4)&8);

	//cpu->R[Rd] = cpu->R[REG_POS(i, 3)];
	GPVar tmp = c.newGP(VARIABLE_TYPE_GPD);
	c.mov(tmp, reg_pos_ptr(3));
	c.mov(reg_ptr(Rd), tmp);
	if(Rd == 15)
	{
		c.mov(cpu_ptr(next_instruction), tmp);
		c.mov(bb_cycles, 3);
		return 1;
	}
	else
		c.mov(bb_cycles, 1); 
		
	return 1;
}

static int OP_MVN(const u32 i)
{
	GPVar tmp = c.newGP(VARIABLE_TYPE_GPD);
	c.mov(tmp, reg_pos_thumb(3));
	c.not_(tmp);
	c.mov(reg_pos_thumb(0), tmp);
	c.cmp(tmp, 0);
	SET_NZ;
	return 1;
}

//-----------------------------------------------------------------------------
//   MUL
//-----------------------------------------------------------------------------
static int OP_MUL_REG(const u32 i) 
{
	GPVar lhs = c.newGP(VARIABLE_TYPE_GPD);
	c.mov(lhs, reg_pos_thumb(0));
	c.imul(lhs, reg_pos_thumb(3));
	c.mov(reg_pos_thumb(0), lhs);
	c.cmp(lhs, 0);
	SET_NZ;
	if (PROCNUM == ARMCPU_ARM7)
		c.mov(bb_cycles, 4);
	else
		MUL_Mxx_END(lhs, 0, 1);
	return 1;
}

//-----------------------------------------------------------------------------
//   CMP / CMN
//-----------------------------------------------------------------------------
static int OP_CMP_IMM8(const u32 i) 
{
	c.cmp(reg_pos_thumb(8), (i & 0xFF));
	ECall* ctx = c.call(op_cmp[PROCNUM][1]);
	ctx->setPrototype(ASMJIT_CALL_CONV, FunctionBuilder0<Void>());
	return 1; 
};
static int OP_CMP(const u32 i) 
{
	GPVar tmp = c.newGP(VARIABLE_TYPE_GPD);
	c.mov(tmp, reg_pos_thumb(3));
	c.cmp(reg_pos_thumb(0), tmp);
	ECall* ctx = c.call(op_cmp[PROCNUM][1]);
	ctx->setPrototype(ASMJIT_CALL_CONV, FunctionBuilder0<Void>());
	return 1; 
};
static int OP_CMP_SPE(const u32 i) 
{
	u32 Rn = (i&7) | ((i>>4)&8);
	GPVar tmp = c.newGP(VARIABLE_TYPE_GPD);
	c.mov(tmp, reg_pos_ptr(3));
	c.cmp(reg_ptr(Rn), tmp);
	ECall* ctx = c.call(op_cmp[PROCNUM][1]);
	ctx->setPrototype(ASMJIT_CALL_CONV, FunctionBuilder0<Void>());
	return 1; 
};
static int OP_CMN(const u32 i) 
{
	GPVar tmp = c.newGP(VARIABLE_TYPE_GPD);
	c.mov(tmp, reg_pos_thumb(0));
	c.add(tmp, reg_pos_thumb(3));
	ECall* ctx = c.call(op_cmp[PROCNUM][0]);
	ctx->setPrototype(ASMJIT_CALL_CONV, FunctionBuilder0<Void>());
	return 1; 
};

//-----------------------------------------------------------------------------
//   TST
//-----------------------------------------------------------------------------
static int OP_TST(const u32 i)
{
	GPVar tmp = c.newGP(VARIABLE_TYPE_GPD);
	c.mov(tmp, reg_pos_thumb(3));
	c.test(reg_pos_thumb(0), tmp);
	SET_NZ;
	return 1;
}

//-----------------------------------------------------------------------------
//   STR / LDR / STRB / LDRB
//-----------------------------------------------------------------------------
#define STR_THUMB(mem_op, offset) \
	GPVar addr = c.newGP(VARIABLE_TYPE_GPD); \
	GPVar data = c.newGP(VARIABLE_TYPE_GPD); \
	u32 adr_first = cpu->R[_REG_NUM(i, 3)]; \
	 \
	c.mov(addr, reg_pos_thumb(3)); \
	if ((offset) != -1) \
	{ \
		if ((offset) != 0) \
		{ \
			c.add(addr, (u32)(offset)); \
			adr_first += (u32)(offset); \
		} \
	} \
	else \
	{ \
		c.add(addr, reg_pos_thumb(6)); \
		adr_first += cpu->R[_REG_NUM(i, 6)]; \
	} \
	c.mov(data, reg_pos_thumb(0)); \
	ECall *ctx = c.call((void*)mem_op##_tab[PROCNUM][classify_adr(adr_first,1)]); \
	ctx->setPrototype(ASMJIT_CALL_CONV, FunctionBuilder2<Void, u32, u32>()); \
	ctx->setArgument(0, addr); \
	ctx->setArgument(1, data); \
	ctx->setReturn(bb_cycles); \
	return 1;

#define LDR_THUMB(mem_op, offset) \
	GPVar addr = c.newGP(VARIABLE_TYPE_GPD); \
	GPVar data = c.newGP(VARIABLE_TYPE_GPN); \
	u32 adr_first = cpu->R[_REG_NUM(i, 3)]; \
	 \
	c.mov(addr, reg_pos_thumb(3)); \
	if ((offset) != -1) \
	{ \
		if ((offset) != 0) \
		{ \
			c.add(addr, (u32)(offset)); \
			adr_first += (u32)(offset); \
		} \
	} \
	else \
	{ \
		c.add(addr, reg_pos_thumb(6)); \
		adr_first += cpu->R[_REG_NUM(i, 6)]; \
	} \
	c.lea(data, reg_pos_thumb(0)); \
	ECall *ctx = c.call((void*)mem_op##_tab[PROCNUM][classify_adr(adr_first,0)]); \
	ctx->setPrototype(ASMJIT_CALL_CONV, FunctionBuilder2<Void, u32, u32*>()); \
	ctx->setArgument(0, addr); \
	ctx->setArgument(1, data); \
	ctx->setReturn(bb_cycles); \
	return 1;

static int OP_STRB_IMM_OFF(const u32 i) { STR_THUMB(STRB, ((i>>6)&0x1F)); }
static int OP_LDRB_IMM_OFF(const u32 i) { LDR_THUMB(LDRB, ((i>>6)&0x1F)); }
static int OP_STRB_REG_OFF(const u32 i) { STR_THUMB(STRB, -1); } 
static int OP_LDRB_REG_OFF(const u32 i) { LDR_THUMB(LDRB, -1); }
static int OP_LDRSB_REG_OFF(const u32 i) { LDR_THUMB(LDRSB, -1); }

static int OP_STRH_IMM_OFF(const u32 i) { STR_THUMB(STRH, ((i>>5)&0x3E)); }
static int OP_LDRH_IMM_OFF(const u32 i) { LDR_THUMB(LDRH, ((i>>5)&0x3E)); }
static int OP_STRH_REG_OFF(const u32 i) { STR_THUMB(STRH, -1); }
static int OP_LDRH_REG_OFF(const u32 i) { LDR_THUMB(LDRH, -1); }
static int OP_LDRSH_REG_OFF(const u32 i) { LDR_THUMB(LDRSH, -1); } 

static int OP_STR_IMM_OFF(const u32 i) { STR_THUMB(STR, ((i>>4)&0x7C)); }
static int OP_LDR_IMM_OFF(const u32 i) { LDR_THUMB(LDR, ((i>>4)&0x7C)); } // FIXME: tempValue = (tempValue>>adr) | (tempValue<<(32-adr));
static int OP_STR_REG_OFF(const u32 i) { STR_THUMB(STR, -1); }
static int OP_LDR_REG_OFF(const u32 i) { LDR_THUMB(LDR, -1); }
static int OP_STR_SPREL(const u32 i)
{
	u32 imm = ((i&0xFF)<<2);
	u32 adr_first = cpu->R[13] + imm;

	GPVar addr = c.newGP(VARIABLE_TYPE_GPD);
	c.mov(addr, reg_ptr(13));
	if (imm) c.add(addr, imm);
	GPVar data = c.newGP(VARIABLE_TYPE_GPD);
	c.mov(data, reg_pos_thumb(8));
	ECall *ctx = c.call((void*)STR_tab[PROCNUM][classify_adr(adr_first,1)]);
	ctx->setPrototype(ASMJIT_CALL_CONV, FunctionBuilder2<Void, u32, u32>());
	ctx->setArgument(0, addr);
	ctx->setArgument(1, data);
	ctx->setReturn(bb_cycles);
	return 1;
}

static int OP_LDR_SPREL(const u32 i)
{
	u32 imm = ((i&0xFF)<<2);
	u32 adr_first = cpu->R[13] + imm;
	
	GPVar addr = c.newGP(VARIABLE_TYPE_GPD);
	c.mov(addr, reg_ptr(13));
	if (imm) c.add(addr, imm);
	GPVar data = c.newGP(VARIABLE_TYPE_GPN);
	c.lea(data, reg_pos_thumb(8));
	ECall *ctx = c.call((void*)LDR_tab[PROCNUM][classify_adr(adr_first,0)]);
	ctx->setPrototype(ASMJIT_CALL_CONV, FunctionBuilder2<Void, u32, u32*>());
	ctx->setArgument(0, addr);
	ctx->setArgument(1, data);
	ctx->setReturn(bb_cycles);
	return 1;
}

static int OP_LDR_PCREL(const u32 i)
{
	u32 imm = ((i&0xFF)<<2);
	u32 adr_first = (bb_r15 & 0xFFFFFFFC) + imm;
	GPVar addr = c.newGP(VARIABLE_TYPE_GPD);
	GPVar data = c.newGP(VARIABLE_TYPE_GPN);
	c.mov(addr, adr_first);
	c.lea(data, reg_pos_thumb(8));
	ECall *ctx = c.call((void*)LDR_tab[PROCNUM][classify_adr(adr_first,0)]);
	ctx->setPrototype(ASMJIT_CALL_CONV, FunctionBuilder2<Void, u32, u32*>());
	ctx->setArgument(0, addr);
	ctx->setArgument(1, data);
	ctx->setReturn(bb_cycles);
	return 1;
}

//-----------------------------------------------------------------------------
//   STMIA / LDMIA
//-----------------------------------------------------------------------------
static int op_ldm_stm_thumb(u32 i, bool store)
{
	u32 bitmask = i & 0xFF;
	u32 pop = popcount(bitmask);

	if (BIT_N(i, _REG_NUM(i, 8)))
		printf("WARNING - %sIA with Rb in Rlist (THUMB)\n", store?"STM":"LDM");

	GPVar adr = c.newGP(VARIABLE_TYPE_GPD);
	c.mov(adr, reg_pos_thumb(8));

	call_ldm_stm(adr, bitmask, store, 1);

	// ARM_REF:	THUMB: Causes base register write-back, and is not optional
	// ARM_REF:	If the base register <Rn> is specified in <registers>, the final value of <Rn> is the loaded value
	//			(not the written-back value).
	if (store)
		c.add(reg_pos_thumb(8), 4*pop);
	else
	{
		if (!BIT_N(i, _REG_NUM(i, 8)))
			c.add(reg_pos_thumb(8), 4*pop);
	}

	emit_MMU_aluMemCycles(store ? 2 : 3, bb_cycles, pop);
	return 1;
}

static int OP_LDMIA_THUMB(const u32 i) { return op_ldm_stm_thumb(i, 0); }
static int OP_STMIA_THUMB(const u32 i) { return op_ldm_stm_thumb(i, 1); }

//-----------------------------------------------------------------------------
//   Adjust SP
//-----------------------------------------------------------------------------
static int OP_ADJUST_P_SP(const u32 i) { c.add(reg_ptr(13), ((i&0x7F)<<2)); return 1; }
static int OP_ADJUST_M_SP(const u32 i) { c.sub(reg_ptr(13), ((i&0x7F)<<2)); return 1; }

//-----------------------------------------------------------------------------
//   PUSH / POP
//-----------------------------------------------------------------------------
static int op_push_pop(u32 i, bool store, bool pc_lr)
{
	u32 bitmask = (i & 0xFF);
	bitmask |= pc_lr << (store ? 14 : 15);
	u32 pop = popcount(bitmask);
	int dir = store ? -1 : 1;

	GPVar adr = c.newGP(VARIABLE_TYPE_GPD);
	c.mov(adr, reg_ptr(13));
	if(store)
		c.sub(adr, 4);

	call_ldm_stm(adr, bitmask, store, dir);

	if(pc_lr && !store)
		op_bx_thumb(reg_ptr(15), 0, PROCNUM == ARMCPU_ARM9);
	c.add(reg_ptr(13), 4*dir*pop);

	emit_MMU_aluMemCycles(store ? (pc_lr?4:3) : (pc_lr?5:2), bb_cycles, pop);
	return 1;
}

static int OP_PUSH(const u32 i)    { return op_push_pop(i, 1, 0); }
static int OP_PUSH_LR(const u32 i) { return op_push_pop(i, 1, 1); }
static int OP_POP(const u32 i)     { return op_push_pop(i, 0, 0); }
static int OP_POP_PC(const u32 i)  { return op_push_pop(i, 0, 1); }

//-----------------------------------------------------------------------------
//   Branch
//-----------------------------------------------------------------------------
static int OP_B_COND(const u32 i)
{
	u32 dst = bb_r15 + ((u32)((s8)(i&0xFF))<<1);
	c.mov(cpu_ptr(instruct_adr), bb_next_instruction);
	c.mov(bb_cycles, 1);
	Label skip = c.newLabel();
	emit_branch((i>>8)&0xF, skip);
	c.mov(cpu_ptr(instruct_adr), dst);
	c.add(bb_cycles, 2);
	c.bind(skip);

	return 1;
}

static int OP_B_UNCOND(const u32 i)
{
	u32 dst = bb_r15 + (SIGNEXTEND_11(i)<<1);
	c.mov(cpu_ptr(instruct_adr), dst);
	return 1;
}

static int OP_BLX(const u32 i)
{
	GPVar dst = c.newGP(VARIABLE_TYPE_GPD);
	c.mov(dst, cpu_ptr(R[14]));
	c.add(dst, (i&0x7FF) << 1);
	c.and_(dst, 0xFFFFFFFC);
	c.mov(cpu_ptr(instruct_adr), dst);
	c.mov(reg_ptr(14), bb_next_instruction | 1);
	// reset T bit
	c.and_(cpu_ptr_byte(CPSR, 0), ~(1<<5));
	return 1;
}

static int OP_BL_10(const u32 i)
{
	u32 dst = bb_r15 + (SIGNEXTEND_11(i)<<12);
	c.mov(reg_ptr(14), dst);
	return 1;
}
static int OP_BL_11(const u32 i) 
{
	GPVar dst = c.newGP(VARIABLE_TYPE_GPD);
	c.mov(dst, reg_ptr(14));
	c.add(dst, (i&0x7FF) << 1);
	c.mov(cpu_ptr(instruct_adr), dst);
	c.mov(reg_ptr(14), bb_next_instruction | 1);
	return 1;
}

static int op_bx_thumb(Mem srcreg, bool blx, bool test_thumb)
{
	GPVar dst = c.newGP(VARIABLE_TYPE_GPD);
	GPVar thumb = c.newGP(VARIABLE_TYPE_GPD);
	c.mov(dst, srcreg);
	c.mov(thumb, dst);								// * cpu->CPSR.bits.T = BIT0(Rm);
	c.and_(thumb, 1);								// *
	if (blx)
		c.mov(reg_ptr(14), bb_next_instruction | 1);
	if(test_thumb)
	{
		GPVar mask = c.newGP(VARIABLE_TYPE_GPD);
		c.lea(mask, ptr_abs((void*)0xFFFFFFFC, thumb.r64(), TIMES_2));
		c.and_(dst, mask);
	}
	else
		c.and_(dst, 0xFFFFFFFE);
	c.shl(thumb, 5);								// *
	c.or_(thumb, ~(1<<5));							// *
	c.and_(cpu_ptr_byte(CPSR, 0), thumb.r8Lo());	// ******************************
	c.mov(cpu_ptr(instruct_adr), dst);
	return 1;
}

static int OP_BX_THUMB(const u32 i) { if (REG_POS(i, 3) == 15) c.mov(reg_ptr(15), bb_r15); return op_bx_thumb(reg_pos_ptr(3), 0, 0); }
static int OP_BLX_THUMB(const u32 i) { return op_bx_thumb(reg_pos_ptr(3), 1, 1); }

static int OP_SWI_THUMB(const u32 i) { return op_swi(i & 0x1F); }

//-----------------------------------------------------------------------------
//   Unimplemented; fall back to the C versions
//-----------------------------------------------------------------------------

#define OP_UND           NULL
#define OP_LDREX         NULL
#define OP_STREX         NULL
#define OP_LDC_P_IMM_OFF NULL
#define OP_LDC_M_IMM_OFF NULL
#define OP_LDC_P_PREIND  NULL
#define OP_LDC_M_PREIND  NULL
#define OP_LDC_P_POSTIND NULL
#define OP_LDC_M_POSTIND NULL
#define OP_LDC_OPTION    NULL
#define OP_STC_P_IMM_OFF NULL
#define OP_STC_M_IMM_OFF NULL
#define OP_STC_P_PREIND  NULL
#define OP_STC_M_PREIND  NULL
#define OP_STC_P_POSTIND NULL
#define OP_STC_M_POSTIND NULL
#define OP_STC_OPTION    NULL
#define OP_CDP           NULL

#define OP_UND_THUMB     NULL
#define OP_BKPT_THUMB    NULL

//-----------------------------------------------------------------------------
//   Dispatch table
//-----------------------------------------------------------------------------

typedef int (*ArmOpCompiler)(u32);
static const ArmOpCompiler arm_instruction_compilers[4096] = {
#define TABDECL(x) x
#include "instruction_tabdef.inc"
#undef TABDECL
};

static const ArmOpCompiler thumb_instruction_compilers[1024] = {
#define TABDECL(x) x
#include "thumb_tabdef.inc"
#undef TABDECL
};

//-----------------------------------------------------------------------------
//   Generic instruction wrapper
//-----------------------------------------------------------------------------

template<int PROCNUM, int thumb>
static u32 FASTCALL OP_DECODE()
{
	u32 cycles;
	u32 adr = cpu->instruct_adr;
	if(thumb)
	{
		cpu->next_instruction = adr + 2;
		cpu->R[15] = adr + 4;
		u32 opcode = _MMU_read16<PROCNUM, MMU_AT_CODE>(adr);
		_armlog(PROCNUM, adr, opcode);
		cycles = thumb_instructions_set[PROCNUM][opcode>>6](opcode);
	}
	else
	{
		cpu->next_instruction = adr + 4;
		cpu->R[15] = adr + 8;
		u32 opcode = _MMU_read32<PROCNUM, MMU_AT_CODE>(adr);
		_armlog(PROCNUM, adr, opcode);
		if(CONDITION(opcode) == 0xE || TEST_COND(CONDITION(opcode), CODE(opcode), cpu->CPSR))
			cycles = arm_instructions_set[PROCNUM][INSTRUCTION_INDEX(opcode)](opcode);
		else
			cycles = 1;
	}
	cpu->instruct_adr = cpu->next_instruction;
	return cycles;
}

static const ArmOpCompiled op_decode[2][2] = { OP_DECODE<0,0>, OP_DECODE<0,1>, OP_DECODE<1,0>, OP_DECODE<1,1> };

//-----------------------------------------------------------------------------
//   Compiler
//-----------------------------------------------------------------------------

static u32 instr_attributes(u32 opcode)
{
	return bb_thumb ? thumb_attributes[opcode>>6]
		 : instruction_attributes[INSTRUCTION_INDEX(opcode)];
}

static bool instr_is_branch(u32 opcode)
{
	u32 x = instr_attributes(opcode);
	if(bb_thumb)
		return (x & BRANCH_ALWAYS)
		    || ((x & BRANCH_POS0) && ((opcode&7) | ((opcode>>4)&8)) == 15)
			|| (x & BRANCH_SWI)
		    || (x & JIT_BYPASS);
	else
		return (x & BRANCH_ALWAYS)
		    || ((x & BRANCH_POS12) && REG_POS(opcode,12) == 15)
		    || ((x & BRANCH_LDM) && BIT15(opcode))
			|| (x & BRANCH_SWI)
		    || (x & JIT_BYPASS);
}

static bool instr_uses_r15(u32 opcode)
{
	u32 x = instr_attributes(opcode);
	if(bb_thumb)
		return ((x & SRCREG_POS0) && ((opcode&7) | ((opcode>>4)&8)) == 15)
			|| ((x & SRCREG_POS3) && REG_POS(opcode,3) == 15)
			|| (x & JIT_BYPASS);
	else
		return ((x & SRCREG_POS0) && REG_POS(opcode,0) == 15)
		    || ((x & SRCREG_POS8) && REG_POS(opcode,8) == 15)
		    || ((x & SRCREG_POS12) && REG_POS(opcode,12) == 15)
		    || ((x & SRCREG_POS16) && REG_POS(opcode,16) == 15)
		    || ((x & SRCREG_STM) && BIT15(opcode))
		    || (x & JIT_BYPASS);
}

static bool instr_is_conditional(u32 opcode)
{
	if(bb_thumb) return false;
	
	return !(CONDITION(opcode) == 0xE
	         || (CONDITION(opcode) == 0xF && CODE(opcode) == 5));
}

static int instr_cycles(u32 opcode)
{
	u32 x = instr_attributes(opcode);
	u32 c = (x & INSTR_CYCLES_MASK);
	if(c == INSTR_CYCLES_VARIABLE)
	{
		if ((x & BRANCH_SWI) && !cpu->swi_tab)
			return 3;
		
		return 0;
	}
	if(instr_is_branch(opcode) && !(instr_attributes(opcode) & (BRANCH_ALWAYS|BRANCH_LDM)))
		c += 2;
	return c;
}

static bool instr_does_prefetch(u32 opcode)
{
	u32 x = instr_attributes(opcode);
	if(bb_thumb)
		return thumb_instruction_compilers[opcode>>6]
			   && (x & BRANCH_ALWAYS);
	else
		return instr_is_branch(opcode) && arm_instruction_compilers[INSTRUCTION_INDEX(opcode)]
			   && ((x & BRANCH_ALWAYS) || (x & BRANCH_LDM));
}

static const char *disassemble(u32 opcode)
{
	if(bb_thumb)
		return thumb_instruction_names[opcode>>6];
	static char str[100];
	strcpy(str, arm_instruction_names[INSTRUCTION_INDEX(opcode)]);
	static const char *conds[16] = {"EQ","NE","CS","CC","MI","PL","VS","VC","HI","LS","GE","LT","GT","LE","AL","NV"};
	if(instr_is_conditional(opcode))
	{
		strcat(str, ".");
		strcat(str, conds[CONDITION(opcode)]);
	}
	return str;
}

static void sync_r15(u32 opcode, bool is_last, bool force)
{
	if(instr_does_prefetch(opcode))
	{
		assert(!instr_uses_r15(opcode));
		if(force)
		{
			JIT_COMMENT("sync_r15: force instruct_adr %08Xh (PREFETCH)", bb_adr);
			c.mov(cpu_ptr(instruct_adr), bb_next_instruction);
		}
	}
	else
	{
		if(force || (instr_attributes(opcode) & JIT_BYPASS) || (instr_attributes(opcode) & BRANCH_SWI) || (is_last && !instr_is_branch(opcode)))
		{
			JIT_COMMENT("sync_r15: next_instruction %08Xh - %s%s%s%s", bb_next_instruction,
				force?" FORCE":"",
				(instr_attributes(opcode) & JIT_BYPASS)?" BYPASS":"",
				(instr_attributes(opcode) & BRANCH_SWI)?" SWI":"",
				(is_last && !instr_is_branch(opcode))?" LAST":""
			);
			c.mov(cpu_ptr(next_instruction), bb_next_instruction);
		}
		if(instr_uses_r15(opcode))
		{
			JIT_COMMENT("sync_r15: R15 %08Xh (USES R15)", bb_r15);
			c.mov(reg_ptr(15), bb_r15);
		}
		if(instr_attributes(opcode) & JIT_BYPASS)
		{
			JIT_COMMENT("sync_r15: instruct_adr %08Xh (JIT_BYPASS)", bb_adr);
			c.mov(cpu_ptr(instruct_adr), bb_adr);
		}
	}
}

static void emit_branch(int cond, Label to)
{
	JIT_COMMENT("emit_branch cond %02X", cond);
	static const u8 cond_bit[] = {0x40, 0x40, 0x20, 0x20, 0x80, 0x80, 0x10, 0x10};
	if(cond < 8)
	{
		c.test(flags_ptr, cond_bit[cond]);
		(cond & 1)?c.jnz(to):c.jz(to);
	}
	else
	{
		GPVar x = c.newGP(VARIABLE_TYPE_GPN);
		c.movzx(x, flags_ptr);
		c.and_(x, 0xF0);
#if defined(_M_X64) || defined(__x86_64__)
		c.add(x, offsetof(armcpu_t,cond_table) + cond);
		c.test(byte_ptr(bb_cpu, x), 1);
#else
		c.test(byte_ptr_abs((void*)(arm_cond_table + cond), x, TIMES_1), 1);
#endif
		c.unuse(x);
		c.jz(to);
	}
}

static void emit_armop_call(u32 opcode)
{
	ArmOpCompiler fc = bb_thumb?	thumb_instruction_compilers[opcode>>6]:
									arm_instruction_compilers[INSTRUCTION_INDEX(opcode)];
	if (fc && fc(opcode)) 
		return;

	JIT_COMMENT("call interpreter");
	GPVar arg = c.newGP(VARIABLE_TYPE_GPD);
	c.mov(arg, opcode);
	OpFunc f = bb_thumb ? thumb_instructions_set[PROCNUM][opcode>>6]
	                     : arm_instructions_set[PROCNUM][INSTRUCTION_INDEX(opcode)];
	ECall* ctx = c.call((void*)f);
	ctx->setPrototype(ASMJIT_CALL_CONV, FunctionBuilder1<u32, u32>());
	ctx->setArgument(0, arg);
	ctx->setReturn(bb_cycles);
}

static void _armlog(u8 proc, u32 addr, u32 opcode)
{
#if 0
#if 0
	fprintf(stderr, "\t\t;R0:%08X R1:%08X R2:%08X R3:%08X R4:%08X R5:%08X R6:%08X R7:%08X R8:%08X R9:%08X\n\t\t;R10:%08X R11:%08X R12:%08X R13:%08X R14:%08X R15:%08X| next %08X, N:%i Z:%i C:%i V:%i\n",
		cpu->R[0],  cpu->R[1],  cpu->R[2],  cpu->R[3],  cpu->R[4],  cpu->R[5],  cpu->R[6],  cpu->R[7], 
		cpu->R[8],  cpu->R[9],  cpu->R[10],  cpu->R[11],  cpu->R[12],  cpu->R[13],  cpu->R[14],  cpu->R[15],
		cpu->next_instruction, cpu->CPSR.bits.N, cpu->CPSR.bits.Z, cpu->CPSR.bits.C, cpu->CPSR.bits.V);
#endif
	#define INDEX22(i) ((((i)>>16)&0xFF0)|(((i)>>4)&0xF))
	char dasmbuf[4096];
	if(cpu->CPSR.bits.T)
		des_thumb_instructions_set[((opcode)>>6)&1023](addr, opcode, dasmbuf);
	else
		des_arm_instructions_set[INDEX22(opcode)](addr, opcode, dasmbuf);
	#undef INDEX22
	fprintf(stderr, "%s%c %08X\t%08X \t%s\n", cpu->CPSR.bits.T?"THUMB":"ARM", proc?'7':'9', addr, opcode, dasmbuf); 
#else
	return;
#endif
}

template<int PROCNUM>
static u32 compile_basicblock()
{
	bool has_variable_cycles = 0;
	int constant_cycles = 0;
	int interpreted_cycles = 0;
	int n = 0;
	u32 start_adr = cpu->instruct_adr;
	u32 opcodes[MAX_JIT_BLOCK_SIZE];
	bb_thumb = cpu->CPSR.bits.T;
	bb_opcodesize = bb_thumb ? 2 : 4;

	if (!JIT_MAPPED(start_adr & 0x0FFFFFFF, PROCNUM))
	{
		printf("JIT: use unmapped memory address %08X\n", start_adr);
		execute = false;
		return 1;
	}

	for(n=0; n<MAX_JIT_BLOCK_SIZE;)
	{
		u32 opcode;
		if(bb_thumb)
			opcode = _MMU_read16<PROCNUM, MMU_AT_CODE>(start_adr + n*2);
		else
			opcode = _MMU_read32<PROCNUM, MMU_AT_CODE>(start_adr + n*4);
		
		opcodes[n++] = opcode;
		has_variable_cycles |= (instr_is_conditional(opcode) && instr_cycles(opcode) > 1)
		                    || instr_cycles(opcode) == 0;
		constant_cycles += instr_is_conditional(opcode) ? 1 : instr_cycles(opcode);
		if(instr_is_branch(opcode))
			break;
	}

#if LOG_JIT
	fprintf(stderr, "adr %08Xh %s%c (num %i)\n", start_adr, ARMPROC.CPSR.bits.T ? "THUMB":"ARM", PROCNUM?'7':'9', n);
	fprintf(stderr, "cycles %d%s\n", constant_cycles, has_variable_cycles ? " + variable" : "");
	for(int i=0; i<n; i++)
	{
		char dasmbuf[1024] = {0};
		u32 dasm_addr = start_adr + (i*bb_opcodesize);
		if(ARMPROC.CPSR.bits.T)
			des_thumb_instructions_set[opcodes[i]>>6](dasm_addr, opcodes[i], dasmbuf);
		else
			des_arm_instructions_set[INSTRUCTION_INDEX(opcodes[i])](dasm_addr, opcodes[i], dasmbuf);
		fprintf(stderr, "%08X\t%s\t\t; %s \n", dasm_addr, dasmbuf, disassemble(opcodes[i]));
	}
#endif

	c.clear();
	c.newFunction(ASMJIT_CALL_CONV, FunctionBuilder0<int>());
	c.getFunction()->setHint(FUNCTION_HINT_NAKED, true);
	c.getFunction()->setHint(FUNCTION_HINT_PUSH_POP_SEQUENCE, true);
	bb_cpu = c.newGP(VARIABLE_TYPE_GPN);
	JIT_COMMENT("CPU ptr");
	c.mov(bb_cpu, (uintptr_t)&ARMPROC);

	if(has_variable_cycles)
	{
		total_cycles = c.newGP(VARIABLE_TYPE_GPD);
		JIT_COMMENT("set total_cycles to %d", constant_cycles);
		c.mov(total_cycles, constant_cycles);
	}

	for(int i=0; i<n; i++)
	{
		u32 opcode = opcodes[i];
		bb_adr = start_adr + i*bb_opcodesize;
		JIT_COMMENT("%s (PC:%08X)", disassemble(opcode), bb_adr);
		
		bb_cycles = c.newGP(VARIABLE_TYPE_GPD);
		u32 cycles = instr_cycles(opcode);
		if(instr_is_conditional(opcode))
		{
			// 25% of conditional instructions are immediately followed by
			// another with the same condition, but merging them into a
			// single branch has negligible effect on speed.
			if(i == n-1) sync_r15(opcode, 1, 1);
			Label skip = c.newLabel();
			emit_branch(CONDITION(opcode), skip);
			if(i != n-1) sync_r15(opcode, 0, 0);
			emit_armop_call(opcode);
			
			if(cycles == 0)
			{
				JIT_COMMENT("cycles");
				c.lea(total_cycles, ptr(total_cycles.r64(), bb_cycles.r64(), TIMES_1, -1));
			}
			else 
				if(cycles > 1)
				{
					JIT_COMMENT("cycles (%d)", cycles);
					c.add(total_cycles, cycles - 1);
				}
			c.bind(skip);
		}
		else
		{
			sync_r15(opcode, i == (n-1), 0);
			emit_armop_call(opcode);
			if(cycles == 0)
			{
				JIT_COMMENT("cycles");
				c.add(total_cycles, bb_cycles);
			}
		}
		interpreted_cycles += op_decode[PROCNUM][bb_thumb]();
	}
	
	if(!instr_does_prefetch(opcodes[n-1]))
	{
		JIT_COMMENT("!instr_does_prefetch: copy next_instruction (%08X) to instruct_adr (%08X)", cpu->next_instruction, cpu->instruct_adr);
		GPVar x = c.newGP(VARIABLE_TYPE_GPD);
		c.mov(x, cpu_ptr(next_instruction));
		c.mov(cpu_ptr(instruct_adr), x);
		c.unuse(x);
	}

	JIT_COMMENT("total cycles (block)");
	GPVar ret = c.newGP(VARIABLE_TYPE_GPD);
	if(has_variable_cycles)
		c.mov(ret, total_cycles);
	else
		c.mov(ret, constant_cycles);
	c.ret(ret);
	c.endFunction();

	ArmOpCompiled f = (ArmOpCompiled)c.make();
	if(c.getError())
	{
		fprintf(stderr, "JIT error: %s\n", getErrorString(c.getError()));
		f = op_decode[PROCNUM][bb_thumb];
	}
#if LOG_JIT
	uintptr_t baddr = (uintptr_t)f;
	fprintf(stderr, "Block address %08lX\n\n", baddr);
	fflush(stderr);
#endif
	
	JIT_COMPILED_FUNC(start_adr, PROCNUM) = (uintptr_t)f;
	return interpreted_cycles;
}

template<int PROCNUM> u32 arm_jit_compile()
{
	*PROCNUM_ptr = PROCNUM;

	// prevent endless recompilation of self-modifying code, which would be a memleak since we only free code all at once.
	// also allows us to clear compiled_funcs[] while leaving it sparsely allocated, if the OS does memory overcommit.
	u32 adr = cpu->instruct_adr;
	u32 mask_adr = (adr & 0x07FFFFFE) >> 4;
	if(((recompile_counts[mask_adr >> 1] >> 4*(mask_adr & 1)) & 0xF) > 8)
	{
		ArmOpCompiled f = op_decode[PROCNUM][cpu->CPSR.bits.T];
		JIT_COMPILED_FUNC(adr, PROCNUM) = (uintptr_t)f;
		return f();
	}
	recompile_counts[mask_adr >> 1] += 1 << 4*(mask_adr & 1);

	return compile_basicblock<PROCNUM>();
}

template u32 arm_jit_compile<0>();
template u32 arm_jit_compile<1>();

void arm_jit_reset(bool enable)
{
#if LOG_JIT
	c.setLogger(&logger);
#ifdef _WINDOWS
	freopen("\\desmume_jit.log", "w", stderr);
#endif
#endif
#ifdef HAVE_STATIC_CODE_BUFFER
	scratchptr = scratchpad;
#endif
	printf("CPU mode: %s\n", enable?"JIT":"Interpreter");

	if (enable)
	{
#ifdef MAPPED_JIT_FUNCS
		memset(JIT.MAIN_MEM,  0, sizeof(JIT.MAIN_MEM));
		memset(JIT.SWIRAM,    0, sizeof(JIT.SWIRAM));
		memset(JIT.ARM9_ITCM, 0, sizeof(JIT.ARM9_ITCM));
		memset(JIT.ARM9_LCDC, 0, sizeof(JIT.ARM9_LCDC));
		memset(JIT.ARM9_BIOS, 0, sizeof(JIT.ARM9_BIOS));
		memset(JIT.ARM7_BIOS, 0, sizeof(JIT.ARM7_BIOS));
		memset(JIT.ARM7_ERAM, 0, sizeof(JIT.ARM7_ERAM));
		memset(JIT.ARM7_WIRAM,0, sizeof(JIT.ARM7_WIRAM));
		memset(JIT.ARM7_WRAM, 0, sizeof(JIT.ARM7_WRAM));
		memset(recompile_counts, 0, sizeof(recompile_counts));
		init_jit_mem();
#else
		for(int i=0; i<sizeof(recompile_counts)/8; i++)
			if(((u64*)recompile_counts)[i])
			{
				((u64*)recompile_counts)[i] = 0;
				memset(compiled_funcs+128*i, 0, 128*sizeof(*compiled_funcs));
			}
#endif

		init_op_cmp(0, 0);
		init_op_cmp(0, 1);
		init_op_cmp(1, 0);
		init_op_cmp(1, 1);
	}
	c.clear();
}
#endif // HAVE_JIT
