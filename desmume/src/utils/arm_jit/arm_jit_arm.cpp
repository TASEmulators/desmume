/*
	Copyright (C) 2022 Patrick Herlihy - https://byte4byte.com
	Copyright (C) 2022 DeSmuME team

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

#include "types.h"

#ifdef HAVE_JIT

#define PTR_STORE_REG 11

#include <map>
#include <vector>

#include "utils/bits.h"
#include "armcpu.h"
#include "instructions.h"
#include "instruction_attributes.h"
#include "MMU.h"
#include "MMU_timing.h"
#include "arm_jit.h"
#include "bios.h"

#include <stdio.h>
static const char *disassemble(u32 opcode);

#define ADDRESS 		0
#define VALUE 			1
#define VALUE_NO_CACHE	2

#ifdef __aarch64__
#include "arm64_emit.cpp"
#else
#include "arm_emit.cpp"
#endif
t_bytes *g_out = NULL;

#ifdef __APPLE__
#include <sys/mman.h>
#include <pthread.h>
static int g_jitWrite = 0;
void jit_exec() {
  if (g_jitWrite) {
    (void)pthread_jit_write_protect_np(1);
    g_jitWrite = 0;
  }
}

void jit_write() {
  if (! g_jitWrite) {
    (void)pthread_jit_write_protect_np(0);
    g_jitWrite = 1;
  }
}
#else
#define jit_write(...)
#define jit_exec(...)
#endif

#define tmp_reg R2
#define tmp2_reg R3
#define x_reg R8
#define y_reg R9
#define imm_reg R4
#define rcf_reg R5
#define rhs_reg R6
#define lhs_reg R7

#define rcf rcf_reg
#define rhs rhs_reg
#define lhs lhs_reg
#define x x_reg
#define y y_reg
#define tmp tmp_reg
#define tmp2 tmp2_reg

#define LOG_JIT_LEVEL 0
#define PROFILER_JIT_LEVEL 0

#if (PROFILER_JIT_LEVEL > 0)
#include <algorithm>
#endif

#if (LOG_JIT_LEVEL > 0)
#define LOG_JIT 1
#define JIT_COMMENT(...) c.comment(__VA_ARGS__)
#define printJIT(buf, val) { \
}
#else
#define LOG_JIT 0
#define JIT_COMMENT(...)
#define printJIT(buf, val)
#endif

#define CACHED_PTR(exp) PTR_STORE_REG

u32 saveBlockSizeJIT = 0;

static volatile unsigned int label_gen_num=0;
unsigned int genlabel() {
	return ++label_gen_num;
}

#include <sys/stat.h>

#define GpVar uintptr_t

static volatile int PROCNUM;
static volatile int *PROCNUM_ptr = &PROCNUM;
static int bb_opcodesize;
static int bb_adr;
static bool bb_thumb;
static GpVar bb_cpu;
static GpVar bb_cycles;
static GpVar bb_total_cycles;
static u32 bb_constant_cycles;

#define cpu (&ARMPROC)

static void save();
static void restore();

static int inline cpu_get_ptr(void *start, int o, int r, int v) {
	emit_ptr(g_out, (uintptr_t)((uintptr_t)(&ARMPROC)+o), v == VALUE ? PTR_STORE_REG : r);
	if (v) emit_ldr(g_out, v == VALUE ? PTR_STORE_REG : r, r);
	return r;
}

static int inline cpu_get_ptrB(void *start, int o, int r, int v) {
	emit_ptr(g_out, (uintptr_t)((uintptr_t)(&ARMPROC)+o), v == VALUE ? PTR_STORE_REG : r);
	if (v) emit_ldrb(g_out, v == VALUE ? PTR_STORE_REG : r, r);
	return r;
}


static int inline cpu_get_ptrW(void *start, int o, int r, int v) {
	emit_ptr(g_out, (uintptr_t)((uintptr_t)(&ARMPROC)+o), v == VALUE ? PTR_STORE_REG : r);
	if (v) emit_ldrw(g_out, v == VALUE ? PTR_STORE_REG : r, r);
	return r;
}

static int inline cpu_get_ptrS(void *start, int o, int r, int v) {
	emit_ptr(g_out, (uintptr_t)((uintptr_t)(&ARMPROC)+o), v == VALUE ? PTR_STORE_REG : r);
	if (v) emit_ldrs(g_out, v == VALUE ? PTR_STORE_REG : r, r);
	return r;
}

static int inline cpu_get_ptrSB(void *start, int o, int r, int v) {
	emit_ptr(g_out, (uintptr_t)((uintptr_t)(&ARMPROC)+o), v == VALUE ? PTR_STORE_REG : r);
	if (v) emit_ldrsb(g_out, v == VALUE ? PTR_STORE_REG : r, r);
	return r;
}


static int inline cpu_get_ptrSW(void *start, int o, int r, int v) {
	emit_ptr(g_out, (uintptr_t)((uintptr_t)(&ARMPROC)+o), v == VALUE ? PTR_STORE_REG : r);
	if (v) emit_ldrsw(g_out, v == VALUE ? PTR_STORE_REG : r, r);
	return r;
}


static int inline get_ptr(void *start, int o, int r, int v) {
	emit_read_ptr64_reg(g_out, (uintptr_t)start, r);
	emit_addimm64(g_out, r, o, r);
	if (v) emit_ldr(g_out, r, r);
	return r;
}

static int inline get_ptrB(void *start, int o, int r, int v) {
	emit_read_ptr64_reg(g_out, (uintptr_t)start, r);
	emit_addimm64(g_out, r, o, r);
	if (v) emit_ldrb(g_out, r, r);
	return r;
}


static int inline get_ptrW(void *start, int o, int r, int v) {
	emit_read_ptr64_reg(g_out, (uintptr_t)start, r);
	emit_addimm64(g_out, r, o, r);
	if (v) emit_ldrw(g_out, r, r);
	return r;
}

static int inline get_ptrS(void *start, int o, int r, int v) {
	emit_read_ptr64_reg(g_out, (uintptr_t)start, r);
	emit_addimm64(g_out, r, o, r);
	if (v) emit_ldrs(g_out, r, r);
	return r;
}

static int inline get_ptrSB(void *start, int o, int r, int v) {
	emit_read_ptr64_reg(g_out, (uintptr_t)start, r);
	emit_addimm64(g_out, r, o, r);
	if (v) emit_ldrsb(g_out, r, r);
	return r;
}


static int inline get_ptrSW(void *start, int o, int r, int v) {
	emit_read_ptr64_reg(g_out, (uintptr_t)start, r);
	emit_addimm64(g_out, r, o, r);
	if (v) emit_ldrsw(g_out, r, r);
	return r;
}

#define cpu_dword_ptr(z, o, r, v) cpu_get_ptr(&z, o, r, v)
#define cpu_byte_ptr(z, o, r, v) cpu_get_ptrB(&z, o, r, v)
#define cpu_word_ptr(z, o, r, v) cpu_get_ptrW(&z, o, r, v)
#define cpu_dwords_ptr(z, o, r, v) cpu_get_ptrS(&z, o, r, v)
#define cpu_bytes_ptr(z, o, r, v) cpu_get_ptrSB(&z, o, r, v)
#define cpu_words_ptr(z, o, r, v) cpu_get_ptrSW(&z, o, r, v)

#define dword_ptr(z, o, r, v) get_ptr(&z, o, r, v)
#define byte_ptr(z, o, r, v) get_ptrB(&z, o, r, v)
#define word_ptr(z, o, r, v) get_ptrW(&z, o, r, v)
#define dwords_ptr(z, o, r, v) get_ptrS(&z, o, r, v)
#define bytes_ptr(z, o, r, v) get_ptrSB(&z, o, r, v)
#define words_ptr(z, o, r, v) get_ptrSW(&z, o, r, v)

#define bb_next_instruction (bb_adr + bb_opcodesize)
#define bb_r15				(bb_adr + 2 * bb_opcodesize)

#define cpu_ptr(x, reg, v)			cpu_dword_ptr(bb_cpu, offsetof(armcpu_t, x), reg, v)
#define cpu_ptr_byte(x, y, reg, v)	cpu_byte_ptr(bb_cpu, offsetof(armcpu_t, x) + y, reg, v)
#define flags_ptr(reg, v)			cpu_ptr_byte(CPSR.val, 3, reg, v)
#define reg_ptr(x, reg, v)			cpu_dword_ptr(bb_cpu, offsetof(armcpu_t, R) + 4*(x), reg, v)
#define reg_pos_ptr(x, reg, v)		cpu_dword_ptr(bb_cpu, offsetof(armcpu_t, R) + 4*REG_POS(i,(x)), reg, v)
#define reg_pos_ptrL(x, reg, v)		cpu_word_ptr( bb_cpu, offsetof(armcpu_t, R) + 4*REG_POS(i,(x)), reg, v)
#define reg_pos_ptrH(x, reg, v)		cpu_word_ptr( bb_cpu, offsetof(armcpu_t, R) + 4*REG_POS(i,(x)) + 2, reg, v)
#define reg_pos_ptrB(x, reg, v)		cpu_byte_ptr( bb_cpu, offsetof(armcpu_t, R) + 4*REG_POS(i,(x)), reg, v)
#define reg_pos_ptrSL(x, reg, v)		cpu_words_ptr( bb_cpu, offsetof(armcpu_t, R) + 4*REG_POS(i,(x)), reg, v)
#define reg_pos_ptrSH(x, reg, v)		cpu_words_ptr( bb_cpu, offsetof(armcpu_t, R) + 4*REG_POS(i,(x)) + 2, reg, v)
#define reg_pos_ptrSB(x, reg, v)		cpu_bytes_ptr( bb_cpu, offsetof(armcpu_t, R) + 4*REG_POS(i,(x)), reg, v)
#define reg_pos_thumb(x, reg, v)	cpu_dword_ptr(bb_cpu, offsetof(armcpu_t, R) + 4*((i>>(x))&0x7), reg, v)
#define reg_pos_thumbB(x, reg, v)	cpu_byte_ptr(bb_cpu, offsetof(armcpu_t, R) + 4*((i>>(x))&0x7), reg, v)
#define cp15_ptr(x, reg, v)			dword_ptr(bb_cp15, offsetof(armcp15_t, x), reg, v)
#define cp15_ptr_off(x, y, reg, v)	dword_ptr(bb_cp15, offsetof(armcp15_t, x) + y, reg, v)
#define mmu_ptr(x, reg, v)			dword_ptr(bb_mmu, offsetof(MMU_struct, x), reg, v)
#define mmu_ptr_byte(x, reg, v)		byte_ptr(bb_mmu, offsetof(MMU_struct, x), reg, v)
#define _REG_NUM(i, n)		((i>>(n))&0x7)

#define special_page  (1==1 /*g_i == 80 ||  g_i == 81 || g_i == 82*/)
#define print_page  (1==1)

static inline void get_flags(int reg, int v) {
  	flags_ptr(reg, v);
}


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

static void emit_branch(int cond, int to);
static void _armlog(u8 proc, u32 addr, u32 opcode);

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
#ifdef DEBUG_JIT
		_armlog(PROCNUM, adr, opcode);
		callExe2(opcode>>6, opcode, thumb);
#endif
		cycles = thumb_instructions_set[PROCNUM][opcode>>6](opcode);
	}
	else
	{
		cpu->next_instruction = adr + 4;
		cpu->R[15] = adr + 8;
		u32 opcode = _MMU_read32<PROCNUM, MMU_AT_CODE>(adr);
#ifdef DEBUG_JIT
		_armlog(PROCNUM, adr, opcode);
#endif
		if(CONDITION(opcode) == 0xE || TEST_COND(CONDITION(opcode), CODE(opcode), cpu->CPSR)) {
#ifdef DEBUG_JIT
			callExe2(INSTRUCTION_INDEX(opcode), opcode, thumb);
#endif
			cycles = arm_instructions_set[PROCNUM][INSTRUCTION_INDEX(opcode)](opcode);
		}
		else
			cycles = 1;
	}
	cpu->instruct_adr = cpu->next_instruction;
	return cycles;
}

static const ArmOpCompiled op_decode[2][2] = { OP_DECODE<0,0>, OP_DECODE<0,1>, OP_DECODE<1,0>, OP_DECODE<1,1> };

#define changeCPSR { \
			emit_ptr(g_out, (uintptr_t)NDS_Reschedule, R3);\
			callR3(g_out); \
}

#if (PROFILER_JIT_LEVEL > 0)
struct PROFILER_COUNTER_INFO
{
	u64	count;
	char name[64];
};

struct JIT_PROFILER
{
	JIT_PROFILER()
	{
		memset(&arm_count[0], 0, sizeof(arm_count));
		memset(&thumb_count[0], 0, sizeof(thumb_count));
	}

	u64 arm_count[4096];
	u64 thumb_count[1024];
} profiler_counter[2];

static GpVar bb_profiler;

#define profiler_counter_arm(opcode)   qword_ptr(bb_profiler, offsetof(JIT_PROFILER, arm_count) + (INSTRUCTION_INDEX(opcode)*sizeof(u64)))
#define profiler_counter_thumb(opcode) qword_ptr(bb_profiler, offsetof(JIT_PROFILER, thumb_count) + ((opcode>>6)*sizeof(u64)))

#if (PROFILER_JIT_LEVEL > 1)
struct PROFILER_ENTRY
{
	u32 addr;
	u32	cycles;
} profiler_entry[2][1<<26];

static GpVar bb_profiler_entry;
#endif

#endif

#define S_DST_R15 { \
	JIT_COMMENT("S_DST_R15"); \
	cpu_ptr(next_instruction, R3, VALUE);\
	cpu_ptr(SPSR.val, R9, VALUE);\
	emit_mov(g_out, R9, R10); \
	emit_andimm(g_out, R10, 0x1F, R10); \
	emit_ptr(g_out, (uintptr_t)&ARMPROC, 0); \
	emit_mov(g_out, R10, R1); \
	emit_ptr(g_out, (uintptr_t)armcpu_switchMode, R3); \
	callR3(g_out); \
	emit_write_ptr32_regptrTO_regFROM(g_out, cpu_ptr(CPSR.val, R1, ADDRESS), R9); \
	emit_andimm(g_out, R9, (1<<5), R9); \
	emit_lsr(g_out, R9, 5); \
	emit_lea_ptr_reg(g_out, 0xFFFFFFFC,R9, 2, R10); \
	emit_and(g_out, R10, reg_ptr(15, R1, VALUE), R10); \
	emit_write_ptr32_regptrTO_regFROM(g_out, cpu_ptr(next_instruction, R1, ADDRESS), R10); \
}

// ============================================================================================= IMM
#define LSL_IMM \
	bool rhs_is_imm = false; \
	static unsigned int rhs_var;\
	u32 imm2 = ((i>>7)&0x1F); \
	reg_pos_ptr(0, rhs_reg, VALUE); \
	if(imm2) emit_lsl(g_out, rhs_reg, imm2); \
	u32 rhs_first = cpu->R[REG_POS(i,0)] << imm2;\
	emit_movimm(g_out, imm2, imm_reg);

#define S_LSL_IMM \
	bool rhs_is_imm = false; \
	u8 cf_change = 0; \
	static unsigned int rhs_var;\
	u32 imm2 = ((i>>7)&0x1F); \
	reg_pos_ptr(0, rhs_reg, VALUE); \
	if (imm2)  \
	{ \
		cf_change = 1; \
		emit_lsls(g_out, rhs_reg, imm2); \
	} \
	emit_movimm(g_out, imm2, imm_reg);

#define LSR_IMM \
	bool rhs_is_imm = false; \
	static unsigned int rhs_var;\
	u32 imm2 = ((i>>7)&0x1F); \
	if(imm2) \
	{ \
		reg_pos_ptr(0, rhs_reg, VALUE); \
		emit_lsr(g_out, rhs, imm2); \
	} \
	else \
		emit_movimm(g_out, 0, rhs_reg); \
	u32 rhs_first = imm2 ? cpu->R[REG_POS(i,0)] >> imm2 : 0; \
	emit_movimm(g_out, imm2, imm_reg);

#define S_LSR_IMM \
	bool rhs_is_imm = false; \
	u8 cf_change = 1; \
	static unsigned int rhs_var; \
	u32 imm2 = ((i>>7)&0x1F); \
	reg_pos_ptr(0, rhs_reg, VALUE); \
	if (!imm2) \
	{ \
		emit_testimm(g_out, rhs_reg,  (1 << 31)); \
		emit_setnz(g_out, rcf); \
		emit_xor(g_out, rhs_reg, rhs_reg, rhs_reg); \
	} \
	else \
	{ \
		emit_lsrs(g_out, rhs_reg, imm2); \
	} \
	emit_movimm(g_out, imm2, imm_reg);

#define ASR_IMM \
	bool rhs_is_imm = false; \
	static unsigned int rhs_var;\
	u32 imm2 = ((i>>7)&0x1F); \
	reg_pos_ptr(0, rhs_reg, VALUE); \
	if(!imm2) imm2 = 31; \
	emit_asr(g_out, rhs_reg, imm2); \
	u32 rhs_first = (s32)cpu->R[REG_POS(i,0)] >> imm2; \
	emit_movimm(g_out, imm2, imm_reg);

#define S_ASR_IMM \
	bool rhs_is_imm = false; \
	u8 cf_change = 1; \
	static unsigned int rhs_var; \
	u32 imm2 = ((i>>7)&0x1F); \
	reg_pos_ptr(0, rhs_reg, VALUE); \
	if (!imm2) imm2 = 31; \
	emit_asrs(g_out, rhs_reg, imm2); \
	emit_movimm(g_out, imm2, imm_reg); \

#define ROR_IMM \
	bool rhs_is_imm = false; \
	u32 imm2 = ((i>>7)&0x1F); \
	static unsigned int rhs_var; \
	reg_pos_ptr(0, rhs_reg, VALUE); \
	if (!imm2) \
	{ \
		GET_CARRY(0) \
		emit_rrx(g_out, rhs_reg, 1); \
	} \
	else \
		emit_ror(g_out, rhs_reg, imm2); \
	u32 rhs_first = imm2?ROR(cpu->R[REG_POS(i,0)], imm2) : ((u32)cpu->CPSR.bits.C<<31)|(cpu->R[REG_POS(i,0)]>>1); \
	emit_movimm(g_out, imm2, imm_reg);

#define S_ROR_IMM \
	bool rhs_is_imm = false; \
	u8 cf_change = 1; \
	static unsigned int rhs_var; \
	u32 imm2 = ((i>>7)&0x1F); \
	reg_pos_ptr(0, rhs_reg, VALUE); \
	if (!imm2) \
	{ \
		GET_CARRY(0) \
		emit_rrxs(g_out, rhs_reg, 1); \
	} \
	else \
		emit_rors(g_out, rhs_reg, imm2); \
	emit_movimm(g_out, imm2, imm_reg); \

#define REG_OFF \
	bool rhs_is_imm = false; \
	static unsigned int rhs_var; \
	reg_pos_ptr(0, rhs_reg, VALUE); \
	u32 rhs_first = cpu->R[REG_POS(i,0)];

#define IMM_VAL \
	JIT_COMMENT("IMM_VAL"); \
	bool rhs_is_imm = true; \
	u32 rhs_var = ROR((i&0xFF), (i>>7)&0x1E); \
	u32 rhs_first = rhs_var; \
	emit_movimm(g_out, rhs_var, rhs_reg);

#define S_IMM_VAL \
	bool rhs_is_imm = true; \
	u8 cf_change = 0; \
	static unsigned int rhs_var; \
	rhs_var = ROR((i&0xFF), (i>>7)&0x1E); \
	if ((i>>8)&0xF) \
	{ \
		cf_change = 1; \
		emit_movimm(g_out, BIT31(rhs_var), rcf); \
	} \
	u32 rhs_first = rhs_var; \
	emit_movimm(g_out, rhs_var, rhs_reg);

#define IMM_OFF \
	JIT_COMMENT("IMM_OFF"); \
	bool rhs_is_imm = true; \
	u32 rhs_var = ((i>>4)&0xF0)+(i&0xF); \
	u32 rhs_first = rhs_var; \
	emit_movimm(g_out, rhs_var, rhs_reg);

#define IMM_OFF_12 \
	JIT_COMMENT("IMM_OFF_12"); \
	bool rhs_is_imm = true; \
	u32 rhs_var = (i & 0xFFF); \
	u32 rhs_first = rhs_var; \
	emit_movimm(g_out, rhs_var, rhs_reg);

// ============================================================================================= REG
#define LSX_REG(name, x86inst, sign) \
	bool rhs_is_imm = false; \
	static unsigned int rhs_var; \
	reg_pos_ptrB(8, imm_reg, VALUE); \
	reg_pos_ptr(0, rhs_reg, VALUE); \
	x86inst(g_out, rhs_reg, imm_reg);

#define S_LSX_REG(name, x86inst, sign) \
	bool rhs_is_imm = false; \
	u8 cf_change = 1; \
	GET_CARRY(0); \
	reg_pos_ptr(8, imm_reg, VALUE); \
	reg_pos_ptr(0, rhs_reg, VALUE); \
	x86inst(g_out, rhs_reg, imm_reg); \

#define LSL_REG LSX_REG(LSL_REG, emit_lsl_reg, 0)
#define LSR_REG LSX_REG(LSR_REG, emit_lsr_reg, 0)
#define ASR_REG LSX_REG(ASR_REG, emit_asr_reg, 1)
#define S_LSL_REG S_LSX_REG(S_LSL_REG, emit_lsls_reg, 0)
#define S_LSR_REG S_LSX_REG(S_LSR_REG, emit_lsrs_reg, 0)
#define S_ASR_REG S_LSX_REG(S_ASR_REG, emit_asrs_reg, 1)

#define ROR_REG \
	bool rhs_is_imm = false; \
	static unsigned int rhs_var; \
	reg_pos_ptr(0, rhs_reg, VALUE); \
	reg_pos_ptrB(8, imm_reg, VALUE); \
	emit_ror_reg(g_out, rhs_reg, imm_reg);

#define S_ROR_REG \
	bool rhs_is_imm = false; \
	bool cf_change = 1; \
	static unsigned int rhs_var; \
	GET_CARRY(0); \
	reg_pos_ptr(8, imm_reg, VALUE); \
	reg_pos_ptr(0, rhs_reg, VALUE); \
	emit_rors_reg(g_out, rhs_reg, imm_reg); \

//==================================================================== common funcs
static void emit_MMU_aluMemCycles(int alu_cycles, void *mem_cycles, int population)
{
	if(PROCNUM==ARMCPU_ARM9)
	{
		if(population < alu_cycles)
		{
			emit_movimm(g_out, alu_cycles, x_reg);
			emit_read_ptr32_reg(g_out, (uintptr_t)mem_cycles, R2);
			emit_cmp(g_out, R2, x_reg);
			emit_str_ptr_cond(g_out, (uintptr_t)mem_cycles, x_reg, 0xb);
		}
	}
	else
		emit_inc_ptr_imm(g_out, (uintptr_t)mem_cycles, alu_cycles);
}

//-----------------------------------------------------------------------------
//   OPs
//-----------------------------------------------------------------------------
#define OP_ARITHMETIC(arg, x86inst, symmetric, flags) \
	arg; \
	if(REG_POS(i,12) == REG_POS(i,16)) \
	{ \
		reg_pos_ptr(12, R1, VALUE);\
		x86inst(g_out, R1, rhs, R1); \
		emit_write_ptr32_regptrTO_regFROM(g_out,CACHED_PTR(reg_pos_ptr(12, R2, ADDRESS)), R1); \
	} \
	else if(symmetric && !rhs_is_imm) \
	{ \
		x86inst(g_out, rhs_reg, reg_pos_ptr(16, R1, VALUE), rhs_reg); \
		emit_write_ptr32_regptrTO_regFROM(g_out, reg_pos_ptr(12, R1, ADDRESS), rhs_reg); \
	} \
	else \
	{ \
		reg_pos_ptr(16, lhs, VALUE); \
		x86inst(g_out, lhs, rhs, lhs); \
		emit_write_ptr32_regptrTO_regFROM(g_out, reg_pos_ptr(12, R1, ADDRESS), lhs_reg); \
	} \
	if(flags) \
	{ \
		if(REG_POS(i,12)==15) \
		{ \
			S_DST_R15; \
			emit_inc_ptr_imm(g_out, (uintptr_t)&bb_total_cycles, 2); \
			return 1; \
		} \
		SET_NZCV(!symmetric); \
	} \
	else \
	{ \
		if(REG_POS(i,12)==15) \
		{ \
			reg_ptr(15, tmp_reg, VALUE); \
			emit_write_ptr32_regptrTO_regFROM(g_out, cpu_ptr(next_instruction, R1, ADDRESS), tmp_reg); \
			emit_inc_ptr_imm(g_out, (uintptr_t)&bb_total_cycles, 2); \
		} \
	} \
	return 1;

#define OP_ARITHMETIC_R(arg, x86inst, flags) \
	arg; \
	emit_mov(g_out, rhs, lhs); \
	x86inst(g_out, lhs, reg_pos_ptr(16, R1, VALUE), lhs); \
	emit_write_ptr32_regptrTO_regFROM(g_out, reg_pos_ptr(12, R1, ADDRESS), lhs); \
	if(flags) \
	{ \
		if(REG_POS(i,12)==15) \
		{ \
			S_DST_R15; \
			emit_inc_ptr_imm(g_out, (uintptr_t)&bb_total_cycles, 2); \
			return 1; \
		} \
		SET_NZCV(1); \
	} \
	else \
	{ \
		if(REG_POS(i,12)==15) \
		{ \
			emit_write_ptr32_regptrTO_regFROM(g_out, cpu_ptr(next_instruction, R1, ADDRESS), lhs); \
			emit_inc_ptr_imm(g_out, (uintptr_t)&bb_total_cycles, 2); \
		} \
	} \
	return 1;

#define OP_ARITHMETIC_S(arg, x86inst, symmetric) \
	arg; \
	if(REG_POS(i,12) == REG_POS(i,16)) \
	{ \
		reg_pos_ptr(12, R1, VALUE); \
		x86inst(g_out, R1, rhs, R1); \
		emit_write_ptr32_regptrTO_regFROM(g_out, CACHED_PTR(reg_pos_ptr(12, R2, ADDRESS)), R1); \
	} \
	else if(symmetric && !rhs_is_imm) \
	{ \
		x86inst(g_out, rhs_reg, reg_pos_ptr(16, R1, VALUE), rhs_reg); \
		emit_write_ptr32_regptrTO_regFROM(g_out, reg_pos_ptr(12, R1, ADDRESS), rhs_reg); \
	} \
	else \
	{ \
		reg_pos_ptr(16, lhs, VALUE); \
		x86inst(g_out, lhs, rhs, lhs); \
		emit_write_ptr32_regptrTO_regFROM(g_out, reg_pos_ptr(12, R1, ADDRESS), lhs); \
	} \
	if(REG_POS(i,12)==15) \
	{ \
		S_DST_R15; \
		emit_inc_ptr_imm(g_out, (uintptr_t)&bb_total_cycles, 2); \
		return 1; \
	} \
	SET_NZC; \
	return 1;

static int OP_AND_LSL_IMM(const u32 i) { OP_ARITHMETIC(LSL_IMM, emit_and, 1, 0); }
static int OP_AND_LSL_REG(const u32 i) { OP_ARITHMETIC(LSL_REG, emit_and, 1, 0); }
static int OP_AND_LSR_IMM(const u32 i) { OP_ARITHMETIC(LSR_IMM, emit_and, 1, 0); }
static int OP_AND_LSR_REG(const u32 i) { OP_ARITHMETIC(LSR_REG, emit_and, 1, 0); }
static int OP_AND_ASR_IMM(const u32 i) { OP_ARITHMETIC(ASR_IMM, emit_and, 1, 0); }
static int OP_AND_ASR_REG(const u32 i) { OP_ARITHMETIC(ASR_REG, emit_and, 1, 0); }
static int OP_AND_ROR_IMM(const u32 i) { OP_ARITHMETIC(ROR_IMM, emit_and, 1, 0); }
static int OP_AND_ROR_REG(const u32 i) { OP_ARITHMETIC(ROR_REG, emit_and, 1, 0); }
static int OP_AND_IMM_VAL(const u32 i) { OP_ARITHMETIC(IMM_VAL, emit_and, 1, 0); }

static int OP_EOR_LSL_IMM(const u32 i) { OP_ARITHMETIC(LSL_IMM, emit_xor, 1, 0); }
static int OP_EOR_LSL_REG(const u32 i) { OP_ARITHMETIC(LSL_REG, emit_xor, 1, 0); }
static int OP_EOR_LSR_IMM(const u32 i) { OP_ARITHMETIC(LSR_IMM, emit_xor, 1, 0); }
static int OP_EOR_LSR_REG(const u32 i) { OP_ARITHMETIC(LSR_REG, emit_xor, 1, 0); }
static int OP_EOR_ASR_IMM(const u32 i) { OP_ARITHMETIC(ASR_IMM, emit_xor, 1, 0); }
static int OP_EOR_ASR_REG(const u32 i) { OP_ARITHMETIC(ASR_REG, emit_xor, 1, 0); }
static int OP_EOR_ROR_IMM(const u32 i) { OP_ARITHMETIC(ROR_IMM, emit_xor, 1, 0); }
static int OP_EOR_ROR_REG(const u32 i) { OP_ARITHMETIC(ROR_REG, emit_xor, 1, 0); }
static int OP_EOR_IMM_VAL(const u32 i) { OP_ARITHMETIC(IMM_VAL, emit_xor, 1, 0); }

static int OP_ORR_LSL_IMM(const u32 i) { OP_ARITHMETIC(LSL_IMM, emit_or, 1, 0); }
static int OP_ORR_LSL_REG(const u32 i) { OP_ARITHMETIC(LSL_REG, emit_or, 1, 0); }
static int OP_ORR_LSR_IMM(const u32 i) { OP_ARITHMETIC(LSR_IMM, emit_or, 1, 0); }
static int OP_ORR_LSR_REG(const u32 i) { OP_ARITHMETIC(LSR_REG, emit_or, 1, 0); }
static int OP_ORR_ASR_IMM(const u32 i) { OP_ARITHMETIC(ASR_IMM, emit_or, 1, 0); }
static int OP_ORR_ASR_REG(const u32 i) { OP_ARITHMETIC(ASR_REG, emit_or, 1, 0); }
static int OP_ORR_ROR_IMM(const u32 i) { OP_ARITHMETIC(ROR_IMM, emit_or, 1, 0); }
static int OP_ORR_ROR_REG(const u32 i) { OP_ARITHMETIC(ROR_REG, emit_or, 1, 0); }
static int OP_ORR_IMM_VAL(const u32 i) { OP_ARITHMETIC(IMM_VAL, emit_or, 1, 0); }

static int OP_ADD_LSL_IMM(const u32 i) { OP_ARITHMETIC(LSL_IMM, emit_add, 1, 0); }
static int OP_ADD_LSL_REG(const u32 i) { OP_ARITHMETIC(LSL_REG, emit_add, 1, 0); }
static int OP_ADD_LSR_IMM(const u32 i) { OP_ARITHMETIC(LSR_IMM, emit_add, 1, 0); }
static int OP_ADD_LSR_REG(const u32 i) { OP_ARITHMETIC(LSR_REG, emit_add, 1, 0); }
static int OP_ADD_ASR_IMM(const u32 i) { OP_ARITHMETIC(ASR_IMM, emit_add, 1, 0); }
static int OP_ADD_ASR_REG(const u32 i) { OP_ARITHMETIC(ASR_REG, emit_add, 1, 0); }
static int OP_ADD_ROR_IMM(const u32 i) { OP_ARITHMETIC(ROR_IMM, emit_add, 1, 0); }
static int OP_ADD_ROR_REG(const u32 i) { OP_ARITHMETIC(ROR_REG, emit_add, 1, 0); }
static int OP_ADD_IMM_VAL(const u32 i) { OP_ARITHMETIC(IMM_VAL, emit_add, 1, 0); }

static int OP_SUB_LSL_IMM(const u32 i) { OP_ARITHMETIC(LSL_IMM, emit_sub, 0, 0); }
static int OP_SUB_LSL_REG(const u32 i) { OP_ARITHMETIC(LSL_REG, emit_sub, 0, 0); }
static int OP_SUB_LSR_IMM(const u32 i) { OP_ARITHMETIC(LSR_IMM, emit_sub, 0, 0); }
static int OP_SUB_LSR_REG(const u32 i) { OP_ARITHMETIC(LSR_REG, emit_sub, 0, 0); }
static int OP_SUB_ASR_IMM(const u32 i) { OP_ARITHMETIC(ASR_IMM, emit_sub, 0, 0); }
static int OP_SUB_ASR_REG(const u32 i) { OP_ARITHMETIC(ASR_REG, emit_sub, 0, 0); }
static int OP_SUB_ROR_IMM(const u32 i) { OP_ARITHMETIC(ROR_IMM, emit_sub, 0, 0); }
static int OP_SUB_ROR_REG(const u32 i) { OP_ARITHMETIC(ROR_REG, emit_sub, 0, 0); }
static int OP_SUB_IMM_VAL(const u32 i) { OP_ARITHMETIC(IMM_VAL, emit_sub, 0, 0); }

static int OP_RSB_LSL_IMM(const u32 i) { OP_ARITHMETIC_R(LSL_IMM, emit_sub, 0); }
static int OP_RSB_LSL_REG(const u32 i) { OP_ARITHMETIC_R(LSL_REG, emit_sub, 0); }
static int OP_RSB_LSR_IMM(const u32 i) { OP_ARITHMETIC_R(LSR_IMM, emit_sub, 0); }
static int OP_RSB_LSR_REG(const u32 i) { OP_ARITHMETIC_R(LSR_REG, emit_sub, 0); }
static int OP_RSB_ASR_IMM(const u32 i) { OP_ARITHMETIC_R(ASR_IMM, emit_sub, 0); }
static int OP_RSB_ASR_REG(const u32 i) { OP_ARITHMETIC_R(ASR_REG, emit_sub, 0); }
static int OP_RSB_ROR_IMM(const u32 i) { OP_ARITHMETIC_R(ROR_IMM, emit_sub, 0); }
static int OP_RSB_ROR_REG(const u32 i) { OP_ARITHMETIC_R(ROR_REG, emit_sub, 0); }
static int OP_RSB_IMM_VAL(const u32 i) { OP_ARITHMETIC_R(IMM_VAL, emit_sub, 0); }

// ================================ S instructions
static int OP_AND_S_LSL_IMM(const u32 i) { OP_ARITHMETIC_S(S_LSL_IMM, emit_ands, 1); }
static int OP_AND_S_LSL_REG(const u32 i) { OP_ARITHMETIC_S(S_LSL_REG, emit_ands, 1); }
static int OP_AND_S_LSR_IMM(const u32 i) { OP_ARITHMETIC_S(S_LSR_IMM, emit_ands, 1); }
static int OP_AND_S_LSR_REG(const u32 i) { OP_ARITHMETIC_S(S_LSR_REG, emit_ands, 1); }
static int OP_AND_S_ASR_IMM(const u32 i) { OP_ARITHMETIC_S(S_ASR_IMM, emit_ands, 1); }
static int OP_AND_S_ASR_REG(const u32 i) { OP_ARITHMETIC_S(S_ASR_REG, emit_ands, 1); }
static int OP_AND_S_ROR_IMM(const u32 i) { OP_ARITHMETIC_S(S_ROR_IMM, emit_ands, 1); }
static int OP_AND_S_ROR_REG(const u32 i) { OP_ARITHMETIC_S(S_ROR_REG, emit_ands, 1); }
static int OP_AND_S_IMM_VAL(const u32 i) { OP_ARITHMETIC_S(S_IMM_VAL, emit_ands, 1); }

static int OP_EOR_S_LSL_IMM(const u32 i) { OP_ARITHMETIC_S(S_LSL_IMM, emit_xors, 1); }
static int OP_EOR_S_LSL_REG(const u32 i) { OP_ARITHMETIC_S(S_LSL_REG, emit_xors, 1); }
static int OP_EOR_S_LSR_IMM(const u32 i) { OP_ARITHMETIC_S(S_LSR_IMM, emit_xors, 1); }
static int OP_EOR_S_LSR_REG(const u32 i) { OP_ARITHMETIC_S(S_LSR_REG, emit_xors, 1); }
static int OP_EOR_S_ASR_IMM(const u32 i) { OP_ARITHMETIC_S(S_ASR_IMM, emit_xors, 1); }
static int OP_EOR_S_ASR_REG(const u32 i) { OP_ARITHMETIC_S(S_ASR_REG, emit_xors, 1); }
static int OP_EOR_S_ROR_IMM(const u32 i) { OP_ARITHMETIC_S(S_ROR_IMM, emit_xors, 1); }
static int OP_EOR_S_ROR_REG(const u32 i) { OP_ARITHMETIC_S(S_ROR_REG, emit_xors, 1); }
static int OP_EOR_S_IMM_VAL(const u32 i) { OP_ARITHMETIC_S(S_IMM_VAL, emit_xors, 1); }

static int OP_ORR_S_LSL_IMM(const u32 i) { OP_ARITHMETIC_S(S_LSL_IMM, emit_ors, 1); }
static int OP_ORR_S_LSL_REG(const u32 i) { OP_ARITHMETIC_S(S_LSL_REG, emit_ors, 1); }
static int OP_ORR_S_LSR_IMM(const u32 i) { OP_ARITHMETIC_S(S_LSR_IMM, emit_ors, 1); }
static int OP_ORR_S_LSR_REG(const u32 i) { OP_ARITHMETIC_S(S_LSR_REG, emit_ors, 1); }
static int OP_ORR_S_ASR_IMM(const u32 i) { OP_ARITHMETIC_S(S_ASR_IMM, emit_ors, 1); }
static int OP_ORR_S_ASR_REG(const u32 i) { OP_ARITHMETIC_S(S_ASR_REG, emit_ors, 1); }
static int OP_ORR_S_ROR_IMM(const u32 i) { OP_ARITHMETIC_S(S_ROR_IMM, emit_ors, 1); }
static int OP_ORR_S_ROR_REG(const u32 i) { OP_ARITHMETIC_S(S_ROR_REG, emit_ors, 1); }
static int OP_ORR_S_IMM_VAL(const u32 i) { OP_ARITHMETIC_S(S_IMM_VAL, emit_ors, 1); }

static int OP_ADD_S_LSL_IMM(const u32 i) { OP_ARITHMETIC(LSL_IMM, emit_adds, 1, 1); }
static int OP_ADD_S_LSL_REG(const u32 i) { OP_ARITHMETIC(LSL_REG, emit_adds, 1, 1); }
static int OP_ADD_S_LSR_IMM(const u32 i) { OP_ARITHMETIC(LSR_IMM, emit_adds, 1, 1); }
static int OP_ADD_S_LSR_REG(const u32 i) { OP_ARITHMETIC(LSR_REG, emit_adds, 1, 1); }
static int OP_ADD_S_ASR_IMM(const u32 i) { OP_ARITHMETIC(ASR_IMM, emit_adds, 1, 1); }
static int OP_ADD_S_ASR_REG(const u32 i) { OP_ARITHMETIC(ASR_REG, emit_adds, 1, 1); }
static int OP_ADD_S_ROR_IMM(const u32 i) { OP_ARITHMETIC(ROR_IMM, emit_adds, 1, 1); }
static int OP_ADD_S_ROR_REG(const u32 i) { OP_ARITHMETIC(ROR_REG, emit_adds, 1, 1); }
static int OP_ADD_S_IMM_VAL(const u32 i) { OP_ARITHMETIC(IMM_VAL, emit_adds, 1, 1); }

static int OP_SUB_S_LSL_IMM(const u32 i) { OP_ARITHMETIC(LSL_IMM, emit_subs, 0, 1); }
static int OP_SUB_S_LSL_REG(const u32 i) { OP_ARITHMETIC(LSL_REG, emit_subs, 0, 1); }
static int OP_SUB_S_LSR_IMM(const u32 i) { OP_ARITHMETIC(LSR_IMM, emit_subs, 0, 1); }
static int OP_SUB_S_LSR_REG(const u32 i) { OP_ARITHMETIC(LSR_REG, emit_subs, 0, 1); }
static int OP_SUB_S_ASR_IMM(const u32 i) { OP_ARITHMETIC(ASR_IMM, emit_subs, 0, 1); }
static int OP_SUB_S_ASR_REG(const u32 i) { OP_ARITHMETIC(ASR_REG, emit_subs, 0, 1); }
static int OP_SUB_S_ROR_IMM(const u32 i) { OP_ARITHMETIC(ROR_IMM, emit_subs, 0, 1); }
static int OP_SUB_S_ROR_REG(const u32 i) { OP_ARITHMETIC(ROR_REG, emit_subs, 0, 1); }
static int OP_SUB_S_IMM_VAL(const u32 i) { OP_ARITHMETIC(IMM_VAL, emit_subs, 0, 1); }

static int OP_RSB_S_LSL_IMM(const u32 i) { OP_ARITHMETIC_R(LSL_IMM, emit_subs, 1); }
static int OP_RSB_S_LSL_REG(const u32 i) { OP_ARITHMETIC_R(LSL_REG, emit_subs, 1); }
static int OP_RSB_S_LSR_IMM(const u32 i) { OP_ARITHMETIC_R(LSR_IMM, emit_subs, 1); }
static int OP_RSB_S_LSR_REG(const u32 i) { OP_ARITHMETIC_R(LSR_REG, emit_subs, 1); }
static int OP_RSB_S_ASR_IMM(const u32 i) { OP_ARITHMETIC_R(ASR_IMM, emit_subs, 1); }
static int OP_RSB_S_ASR_REG(const u32 i) { OP_ARITHMETIC_R(ASR_REG, emit_subs, 1); }
static int OP_RSB_S_ROR_IMM(const u32 i) { OP_ARITHMETIC_R(ROR_IMM, emit_subs, 1); }
static int OP_RSB_S_ROR_REG(const u32 i) { OP_ARITHMETIC_R(ROR_REG, emit_subs, 1); }
static int OP_RSB_S_IMM_VAL(const u32 i) { OP_ARITHMETIC_R(IMM_VAL, emit_subs, 1); }

static int OP_ADC_LSL_IMM(const u32 i) { OP_ARITHMETIC(LSL_IMM; GET_CARRY(0), emit_adcs, 1, 0); }
static int OP_ADC_LSL_REG(const u32 i) { OP_ARITHMETIC(LSL_REG; GET_CARRY(0), emit_adcs, 1, 0); }
static int OP_ADC_LSR_IMM(const u32 i) { OP_ARITHMETIC(LSR_IMM; GET_CARRY(0), emit_adcs, 1, 0); }
static int OP_ADC_LSR_REG(const u32 i) { OP_ARITHMETIC(LSR_REG; GET_CARRY(0), emit_adcs, 1, 0); }
static int OP_ADC_ASR_IMM(const u32 i) { OP_ARITHMETIC(ASR_IMM; GET_CARRY(0), emit_adcs, 1, 0); }
static int OP_ADC_ASR_REG(const u32 i) { OP_ARITHMETIC(ASR_REG; GET_CARRY(0), emit_adcs, 1, 0); }
static int OP_ADC_ROR_IMM(const u32 i) { OP_ARITHMETIC(ROR_IMM; GET_CARRY(0), emit_adcs, 1, 0); }
static int OP_ADC_ROR_REG(const u32 i) { OP_ARITHMETIC(ROR_REG; GET_CARRY(0), emit_adcs, 1, 0); }
static int OP_ADC_IMM_VAL(const u32 i) { OP_ARITHMETIC(IMM_VAL; GET_CARRY(0), emit_adcs, 1, 0); }

static int OP_ADC_S_LSL_IMM(const u32 i) { OP_ARITHMETIC(LSL_IMM; GET_CARRY(0), emit_adcs, 1, 1); }
static int OP_ADC_S_LSL_REG(const u32 i) { OP_ARITHMETIC(LSL_REG; GET_CARRY(0), emit_adcs, 1, 1); }
static int OP_ADC_S_LSR_IMM(const u32 i) { OP_ARITHMETIC(LSR_IMM; GET_CARRY(0), emit_adcs, 1, 1); }
static int OP_ADC_S_LSR_REG(const u32 i) { OP_ARITHMETIC(LSR_REG; GET_CARRY(0), emit_adcs, 1, 1); }
static int OP_ADC_S_ASR_IMM(const u32 i) { OP_ARITHMETIC(ASR_IMM; GET_CARRY(0), emit_adcs, 1, 1); }
static int OP_ADC_S_ASR_REG(const u32 i) { OP_ARITHMETIC(ASR_REG; GET_CARRY(0), emit_adcs, 1, 1); }
static int OP_ADC_S_ROR_IMM(const u32 i) { OP_ARITHMETIC(ROR_IMM; GET_CARRY(0), emit_adcs, 1, 1); }
static int OP_ADC_S_ROR_REG(const u32 i) { OP_ARITHMETIC(ROR_REG; GET_CARRY(0), emit_adcs, 1, 1); }
static int OP_ADC_S_IMM_VAL(const u32 i) { OP_ARITHMETIC(IMM_VAL; GET_CARRY(0), emit_adcs, 1, 1); }

static int OP_SBC_LSL_IMM(const u32 i) { OP_ARITHMETIC(LSL_IMM; GET_CARRY(1), emit_sbc, 0, 0); }
static int OP_SBC_LSL_REG(const u32 i) { OP_ARITHMETIC(LSL_REG; GET_CARRY(1), emit_sbc, 0, 0); }
static int OP_SBC_LSR_IMM(const u32 i) { OP_ARITHMETIC(LSR_IMM; GET_CARRY(1), emit_sbc, 0, 0); }
static int OP_SBC_LSR_REG(const u32 i) { OP_ARITHMETIC(LSR_REG; GET_CARRY(1), emit_sbc, 0, 0); }
static int OP_SBC_ASR_IMM(const u32 i) { OP_ARITHMETIC(ASR_IMM; GET_CARRY(1), emit_sbc, 0, 0); }
static int OP_SBC_ASR_REG(const u32 i) { OP_ARITHMETIC(ASR_REG; GET_CARRY(1), emit_sbc, 0, 0); }
static int OP_SBC_ROR_IMM(const u32 i) { OP_ARITHMETIC(ROR_IMM; GET_CARRY(1), emit_sbc, 0, 0); }
static int OP_SBC_ROR_REG(const u32 i) { OP_ARITHMETIC(ROR_REG; GET_CARRY(1), emit_sbc, 0, 0); }
static int OP_SBC_IMM_VAL(const u32 i) { OP_ARITHMETIC(IMM_VAL; GET_CARRY(1), emit_sbc, 0, 0); }

static int OP_SBC_S_LSL_IMM(const u32 i) { OP_ARITHMETIC(LSL_IMM; GET_CARRY(1), emit_sbcs, 0, 1); }
static int OP_SBC_S_LSL_REG(const u32 i) { OP_ARITHMETIC(LSL_REG; GET_CARRY(1), emit_sbcs, 0, 1); }
static int OP_SBC_S_LSR_IMM(const u32 i) { OP_ARITHMETIC(LSR_IMM; GET_CARRY(1), emit_sbcs, 0, 1); }
static int OP_SBC_S_LSR_REG(const u32 i) { OP_ARITHMETIC(LSR_REG; GET_CARRY(1), emit_sbcs, 0, 1); }
static int OP_SBC_S_ASR_IMM(const u32 i) { OP_ARITHMETIC(ASR_IMM; GET_CARRY(1), emit_sbcs, 0, 1); }
static int OP_SBC_S_ASR_REG(const u32 i) { OP_ARITHMETIC(ASR_REG; GET_CARRY(1), emit_sbcs, 0, 1); }
static int OP_SBC_S_ROR_IMM(const u32 i) { OP_ARITHMETIC(ROR_IMM; GET_CARRY(1), emit_sbcs, 0, 1); }
static int OP_SBC_S_ROR_REG(const u32 i) { OP_ARITHMETIC(ROR_REG; GET_CARRY(1), emit_sbcs, 0, 1); }
static int OP_SBC_S_IMM_VAL(const u32 i) { OP_ARITHMETIC(IMM_VAL; GET_CARRY(1), emit_sbcs, 0, 1); }

static int OP_RSC_LSL_IMM(const u32 i) { OP_ARITHMETIC_R(LSL_IMM; GET_CARRY(1), emit_sbc, 0); }
static int OP_RSC_LSL_REG(const u32 i) { OP_ARITHMETIC_R(LSL_REG; GET_CARRY(1), emit_sbc, 0); }
static int OP_RSC_LSR_IMM(const u32 i) { OP_ARITHMETIC_R(LSR_IMM; GET_CARRY(1), emit_sbc, 0); }
static int OP_RSC_LSR_REG(const u32 i) { OP_ARITHMETIC_R(LSR_REG; GET_CARRY(1), emit_sbc, 0); }
static int OP_RSC_ASR_IMM(const u32 i) { OP_ARITHMETIC_R(ASR_IMM; GET_CARRY(1), emit_sbc, 0); }
static int OP_RSC_ASR_REG(const u32 i) { OP_ARITHMETIC_R(ASR_REG; GET_CARRY(1), emit_sbc, 0); }
static int OP_RSC_ROR_IMM(const u32 i) { OP_ARITHMETIC_R(ROR_IMM; GET_CARRY(1), emit_sbc, 0); }
static int OP_RSC_ROR_REG(const u32 i) { OP_ARITHMETIC_R(ROR_REG; GET_CARRY(1), emit_sbc, 0); }
static int OP_RSC_IMM_VAL(const u32 i) { OP_ARITHMETIC_R(IMM_VAL; GET_CARRY(1), emit_sbc, 0); }

static int OP_RSC_S_LSL_IMM(const u32 i) { OP_ARITHMETIC_R(LSL_IMM; GET_CARRY(1), emit_sbcs, 1); }
static int OP_RSC_S_LSL_REG(const u32 i) { OP_ARITHMETIC_R(LSL_REG; GET_CARRY(1), emit_sbcs, 1); }
static int OP_RSC_S_LSR_IMM(const u32 i) { OP_ARITHMETIC_R(LSR_IMM; GET_CARRY(1), emit_sbcs, 1); }
static int OP_RSC_S_LSR_REG(const u32 i) { OP_ARITHMETIC_R(LSR_REG; GET_CARRY(1), emit_sbcs, 1); }
static int OP_RSC_S_ASR_IMM(const u32 i) { OP_ARITHMETIC_R(ASR_IMM; GET_CARRY(1), emit_sbcs, 1); }
static int OP_RSC_S_ASR_REG(const u32 i) { OP_ARITHMETIC_R(ASR_REG; GET_CARRY(1), emit_sbcs, 1); }
static int OP_RSC_S_ROR_IMM(const u32 i) { OP_ARITHMETIC_R(ROR_IMM; GET_CARRY(1), emit_sbcs, 1); }
static int OP_RSC_S_ROR_REG(const u32 i) { OP_ARITHMETIC_R(ROR_REG; GET_CARRY(1), emit_sbcs, 1); }
static int OP_RSC_S_IMM_VAL(const u32 i) { OP_ARITHMETIC_R(IMM_VAL; GET_CARRY(1), emit_sbcs, 1); }

static int OP_BIC_LSL_IMM(const u32 i) { OP_ARITHMETIC(LSL_IMM; emit_not(g_out, rhs_reg, rhs_reg), emit_and, 1, 0); }
static int OP_BIC_LSL_REG(const u32 i) { OP_ARITHMETIC(LSL_REG; emit_not(g_out, rhs_reg, rhs_reg), emit_and, 1, 0); }
static int OP_BIC_LSR_IMM(const u32 i) { OP_ARITHMETIC(LSR_IMM; emit_not(g_out, rhs_reg, rhs_reg), emit_and, 1, 0); }
static int OP_BIC_LSR_REG(const u32 i) { OP_ARITHMETIC(LSR_REG; emit_not(g_out, rhs_reg, rhs_reg), emit_and, 1, 0); }
static int OP_BIC_ASR_IMM(const u32 i) { OP_ARITHMETIC(ASR_IMM; emit_not(g_out, rhs_reg, rhs_reg), emit_and, 1, 0); }
static int OP_BIC_ASR_REG(const u32 i) { OP_ARITHMETIC(ASR_REG; emit_not(g_out, rhs_reg, rhs_reg), emit_and, 1, 0); }
static int OP_BIC_ROR_IMM(const u32 i) { OP_ARITHMETIC(ROR_IMM; emit_not(g_out, rhs_reg, rhs_reg), emit_and, 1, 0); }
static int OP_BIC_ROR_REG(const u32 i) { OP_ARITHMETIC(ROR_REG; emit_not(g_out, rhs_reg, rhs_reg), emit_and, 1, 0); }
static int OP_BIC_IMM_VAL(const u32 i) { OP_ARITHMETIC(IMM_VAL; rhs_var = ~rhs_var; emit_movimm(g_out, rhs_var, rhs_reg);,  emit_and, 1, 0); }

static int OP_BIC_S_LSL_IMM(const u32 i) { OP_ARITHMETIC_S(S_LSL_IMM; emit_not(g_out, rhs_reg, rhs_reg), emit_ands, 1); }
static int OP_BIC_S_LSL_REG(const u32 i) { OP_ARITHMETIC_S(S_LSL_REG; emit_not(g_out, rhs_reg, rhs_reg), emit_ands, 1); }
static int OP_BIC_S_LSR_IMM(const u32 i) { OP_ARITHMETIC_S(S_LSR_IMM; emit_not(g_out, rhs_reg, rhs_reg), emit_ands, 1); }
static int OP_BIC_S_LSR_REG(const u32 i) { OP_ARITHMETIC_S(S_LSR_REG; emit_not(g_out, rhs_reg, rhs_reg), emit_ands, 1); }
static int OP_BIC_S_ASR_IMM(const u32 i) { OP_ARITHMETIC_S(S_ASR_IMM; emit_not(g_out, rhs_reg, rhs_reg), emit_ands, 1); }
static int OP_BIC_S_ASR_REG(const u32 i) { OP_ARITHMETIC_S(S_ASR_REG; emit_not(g_out, rhs_reg, rhs_reg), emit_ands, 1); }
static int OP_BIC_S_ROR_IMM(const u32 i) { OP_ARITHMETIC_S(S_ROR_IMM; emit_not(g_out, rhs_reg, rhs_reg), emit_ands, 1); }
static int OP_BIC_S_ROR_REG(const u32 i) { OP_ARITHMETIC_S(S_ROR_REG; emit_not(g_out, rhs_reg, rhs_reg), emit_ands, 1); }
static int OP_BIC_S_IMM_VAL(const u32 i) { OP_ARITHMETIC_S(S_IMM_VAL; rhs_var = ~rhs_var; emit_movimm(g_out, rhs_var, rhs_reg);,  emit_ands, 1); }

//-----------------------------------------------------------------------------
//   TST
//-----------------------------------------------------------------------------
#define OP_TST_(arg) \
	arg; \
  emit_test(g_out, reg_pos_ptr(16, R1, VALUE), rhs_reg); \
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
	emit_teq(g_out, reg_pos_ptr(16, R1, VALUE), rhs_reg); \
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
#define OP_CMP(arg) \
	arg; \
	emit_cmp(g_out, reg_pos_ptr(16, R1, VALUE), rhs); \
	SET_NZCV(1); \
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
	u32 rhs_imm = *(u32*)&rhs_var; \
	int sign = rhs_is_imm && (rhs_imm != -rhs_imm); \
	if(sign) \
		emit_cmpimm(g_out, reg_pos_ptr(16, R1, VALUE), -rhs_imm); \
	else \
	{ \
		reg_pos_ptr(16, lhs, VALUE);\
		emit_adds(g_out, lhs, rhs, lhs); \
	} \
	SET_NZCV(sign); \
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
	emit_write_ptr32_regptrTO_regFROM(g_out, reg_pos_ptr(12, R1, ADDRESS), rhs);\
	if(REG_POS(i,12)==15) \
	{ \
		emit_write_ptr32_regptrTO_regFROM(g_out, cpu_ptr(next_instruction, R1, ADDRESS), rhs);\
		return 1; \
	} \
    return 1;

static int OP_MOV_LSL_IMM(const u32 i) { if (i == 0xE1A00000) { /* nop */ JIT_COMMENT("nop"); return 1; } OP_MOV(LSL_IMM); }
static int OP_MOV_LSL_REG(const u32 i) { OP_MOV(LSL_REG; if (REG_POS(i,0) == 15) emit_addimm(g_out, rhs, 4, rhs);); }
static int OP_MOV_LSR_IMM(const u32 i) { OP_MOV(LSR_IMM); }
static int OP_MOV_LSR_REG(const u32 i) { OP_MOV(LSR_REG; if (REG_POS(i,0) == 15) emit_addimm(g_out, rhs, 4, rhs);); }
static int OP_MOV_ASR_IMM(const u32 i) { OP_MOV(ASR_IMM); }
static int OP_MOV_ASR_REG(const u32 i) { OP_MOV(ASR_REG); }
static int OP_MOV_ROR_IMM(const u32 i) { OP_MOV(ROR_IMM); }
static int OP_MOV_ROR_REG(const u32 i) { OP_MOV(ROR_REG); }
static int OP_MOV_IMM_VAL(const u32 i) { OP_MOV(IMM_VAL); }

#define OP_MOV_S(arg) \
	arg; \
	emit_write_ptr32_regptrTO_regFROM(g_out, reg_pos_ptr(12, R1, ADDRESS), rhs);\
	if(REG_POS(i,12)==15) \
	{ \
		S_DST_R15; \
		emit_inc_ptr_imm(g_out, (uintptr_t)&bb_total_cycles, 2); \
		return 1; \
	} \
	if(!rhs_is_imm) \
		emit_cmpimm(g_out, rhs_reg, 0); \
	else \
		emit_cmpimm(g_out, reg_pos_ptr(12, R1, VALUE), 0); \
	SET_NZC; \
    return 1;

static int OP_MOV_S_LSL_IMM(const u32 i) { OP_MOV_S(S_LSL_IMM); }
static int OP_MOV_S_LSL_REG(const u32 i) { OP_MOV_S(S_LSL_REG; if (REG_POS(i,0) == 15) emit_addimm(g_out, rhs_reg, 4, rhs_reg);); }
static int OP_MOV_S_LSR_IMM(const u32 i) { OP_MOV_S(S_LSR_IMM); }
static int OP_MOV_S_LSR_REG(const u32 i) { OP_MOV_S(S_LSR_REG; if (REG_POS(i,0) == 15) emit_addimm(g_out, rhs_reg, 4, rhs_reg);); }
static int OP_MOV_S_ASR_IMM(const u32 i) { OP_MOV_S(S_ASR_IMM); }
static int OP_MOV_S_ASR_REG(const u32 i) { OP_MOV_S(S_ASR_REG); }
static int OP_MOV_S_ROR_IMM(const u32 i) { OP_MOV_S(S_ROR_IMM); }
static int OP_MOV_S_ROR_REG(const u32 i) { OP_MOV_S(S_ROR_REG); }
static int OP_MOV_S_IMM_VAL(const u32 i) { OP_MOV_S(S_IMM_VAL); }

//-----------------------------------------------------------------------------
//   MVN
//-----------------------------------------------------------------------------
static int OP_MVN_LSL_IMM(const u32 i) { OP_MOV(LSL_IMM; emit_not(g_out, rhs_reg, rhs_reg)); }
static int OP_MVN_LSL_REG(const u32 i) { OP_MOV(LSL_REG; emit_not(g_out, rhs_reg, rhs_reg)); }
static int OP_MVN_LSR_IMM(const u32 i) { OP_MOV(LSR_IMM; emit_not(g_out, rhs_reg, rhs_reg)); }
static int OP_MVN_LSR_REG(const u32 i) { OP_MOV(LSR_REG; emit_not(g_out, rhs_reg, rhs_reg)); }
static int OP_MVN_ASR_IMM(const u32 i) { OP_MOV(ASR_IMM; emit_not(g_out, rhs_reg, rhs_reg)); }
static int OP_MVN_ASR_REG(const u32 i) { OP_MOV(ASR_REG; emit_not(g_out, rhs_reg, rhs_reg)); }
static int OP_MVN_ROR_IMM(const u32 i) { OP_MOV(ROR_IMM; emit_not(g_out, rhs_reg, rhs_reg)); }
static int OP_MVN_ROR_REG(const u32 i) { OP_MOV(ROR_REG; emit_not(g_out, rhs_reg, rhs_reg)); }
static int OP_MVN_IMM_VAL(const u32 i) { OP_MOV(IMM_VAL; rhs_var = ~rhs_var; emit_movimm(g_out, rhs_var, rhs_reg););  }

static int OP_MVN_S_LSL_IMM(const u32 i) { OP_MOV_S(S_LSL_IMM; emit_not(g_out, rhs_reg, rhs_reg)); }
static int OP_MVN_S_LSL_REG(const u32 i) { OP_MOV_S(S_LSL_REG; emit_not(g_out, rhs_reg, rhs_reg)); }
static int OP_MVN_S_LSR_IMM(const u32 i) { OP_MOV_S(S_LSR_IMM; emit_not(g_out, rhs_reg, rhs_reg)); }
static int OP_MVN_S_LSR_REG(const u32 i) { OP_MOV_S(S_LSR_REG; emit_not(g_out, rhs_reg, rhs_reg)); }
static int OP_MVN_S_ASR_IMM(const u32 i) { OP_MOV_S(S_ASR_IMM; emit_not(g_out, rhs_reg, rhs_reg)); }
static int OP_MVN_S_ASR_REG(const u32 i) { OP_MOV_S(S_ASR_REG; emit_not(g_out, rhs_reg, rhs_reg)); }
static int OP_MVN_S_ROR_IMM(const u32 i) { OP_MOV_S(S_ROR_IMM; emit_not(g_out, rhs_reg, rhs_reg)); }
static int OP_MVN_S_ROR_REG(const u32 i) { OP_MOV_S(S_ROR_REG; emit_not(g_out, rhs_reg, rhs_reg)); }
static int OP_MVN_S_IMM_VAL(const u32 i) { OP_MOV_S(S_IMM_VAL; rhs_var = ~rhs_var; emit_movimm(g_out, rhs_var, rhs_reg);); }

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
static void MUL_Mxx_END(int reg, bool sign, int cycles)
{
	if(sign)
	{
		emit_mov(g_out, reg, y);
		emit_asr(g_out, reg, 31);
		emit_xor(g_out, reg, y, reg);
	}
	emit_orimm(g_out, reg, 1, reg);
	
	emit_movimm(g_out, 31, R0);
	emit_clz(g_out, reg, tmp);
	emit_sub(g_out, R0, tmp, tmp);
	emit_lsr(g_out, tmp, 3);
	emit_addimm(g_out, tmp, cycles+1, tmp);
	emit_write_ptr32_reg(g_out, (uintptr_t)&bb_cycles, tmp);
}

#define OP_MUL_(op, width, sign, accum, flags) \
	reg_pos_ptr(0, lhs, VALUE); \
	reg_pos_ptr(8, rhs, VALUE); \
	op; \
	if(width && accum) \
	{ \
		if(flags) \
		{ \
			emit_write_ptr32_regptrTO_regFROM(g_out, reg_pos_ptr(12, R3, ADDRESS), R0); \
			emit_write_ptr32_regptrTO_regFROM(g_out, reg_pos_ptr(16, R3, ADDRESS), R1); \
		} \
		else \
		{ \
			emit_write_ptr32_regptrTO_regFROM(g_out, reg_pos_ptr(12, R3, ADDRESS), R0); \
			emit_write_ptr32_regptrTO_regFROM(g_out, reg_pos_ptr(16, R3, ADDRESS), R1); \
		} \
	} \
	else if(width) \
	{ \
		emit_write_ptr32_regptrTO_regFROM(g_out, reg_pos_ptr(12, R3, ADDRESS), R0); \
		emit_write_ptr32_regptrTO_regFROM(g_out, reg_pos_ptr(16, R3, ADDRESS), R1); \
	} \
	else \
	{ \
		if (accum) { emit_write_ptr32_regptrTO_regFROM(g_out, reg_pos_ptr(16, R3, ADDRESS), hi); } \
		else { emit_write_ptr32_regptrTO_regFROM(g_out, reg_pos_ptr(16, R3, ADDRESS), lhs); } \
	} \
	if (flags) { SET_NZ(0); } \
	MUL_Mxx_END(rhs, sign, 1+width+accum); \
	return 1;\
	
#define hi R2

static int OP_MUL(const u32 i) { OP_MUL_(emit_mul(g_out, lhs,rhs), 0, 1, 0, 0); }
static int OP_MLA(const u32 i) { OP_MUL_(emit_mla(g_out, hi,reg_pos_ptr(12, R0, VALUE),lhs,rhs), 0, 1, 1, 0); }
static int OP_UMULL(const u32 i) { OP_MUL_(emit_umull(g_out, R0, R1,lhs,rhs), 1, 0, 0, 0); }
static int OP_UMLAL(const u32 i) { OP_MUL_(emit_umlal(g_out, reg_pos_ptr(12, R0, VALUE), reg_pos_ptr(16, R1, VALUE),lhs,rhs), 1, 0, 1, 0); }
static int OP_SMULL(const u32 i) { OP_MUL_(emit_smull(g_out, R0, R1,lhs,rhs), 1, 1, 0, 0); }
static int OP_SMLAL(const u32 i) { OP_MUL_(emit_smlal(g_out, reg_pos_ptr(12, R0, VALUE), reg_pos_ptr(16, R1, VALUE),lhs,rhs), 1, 1, 1, 0); }

static int OP_MUL_S(const u32 i) { OP_MUL_(emit_muls(g_out, lhs,rhs), 0, 1, 0, 1); }
static int OP_MLA_S(const u32 i) { OP_MUL_(emit_mlas(g_out, hi,reg_pos_ptr(12, R1, VALUE),lhs,rhs), 0, 1, 1, 1); }
static int OP_UMULL_S(const u32 i) { OP_MUL_(emit_umulls(g_out, R0, R1, lhs,rhs), 1, 0, 0, 1); }
static int OP_UMLAL_S(const u32 i) { OP_MUL_(emit_umlals(g_out, reg_pos_ptr(12, R0, VALUE), reg_pos_ptr(16, R1, VALUE),lhs,rhs), 1, 0, 1, 1); }
static int OP_SMULL_S(const u32 i) { OP_MUL_(emit_smulls(g_out, R0, R1,lhs,rhs), 1, 1, 0, 1); }
static int OP_SMLAL_S(const u32 i) { OP_MUL_(emit_smlals(g_out, reg_pos_ptr(12, R0, VALUE), reg_pos_ptr(16, R1, VALUE),lhs,rhs), 1, 1, 1, 1); }

#undef hi

#define OP_MULxy_(op, x, y, width, accum, flags) \
	reg_pos_ptr(0, lhs, VALUE); \
	reg_pos_ptr(8, rhs, VALUE); \
	op; \
	if(width && accum) \
	{ \
		if(flags) \
		{ \
			emit_write_ptr32_regptrTO_regFROM(g_out, reg_pos_ptr(12, R3, ADDRESS), R0); \
			emit_write_ptr32_regptrTO_regFROM(g_out, reg_pos_ptr(16, R3, ADDRESS), R1); \
		} \
		else \
		{ \
			emit_write_ptr32_regptrTO_regFROM(g_out, reg_pos_ptr(12, R3, ADDRESS), R0); \
			emit_write_ptr32_regptrTO_regFROM(g_out, reg_pos_ptr(16, R3, ADDRESS), R1); \
		} \
	} \
	else if(width) \
	{ \
		emit_write_ptr32_regptrTO_regFROM(g_out, reg_pos_ptr(12, R3, ADDRESS), R0); \
		emit_write_ptr32_regptrTO_regFROM(g_out, reg_pos_ptr(16, R3, ADDRESS), R1); \
	} \
	else \
	{ \
		if (accum) { emit_write_ptr32_regptrTO_regFROM(g_out, reg_pos_ptr(16, R3, ADDRESS), hi); } \
		else { emit_write_ptr32_regptrTO_regFROM(g_out, reg_pos_ptr(16, R3, ADDRESS), lhs); } \
	} \
	if (flags) { SET_Q; } \
	return 1;\
	
#define hi R2


//-----------------------------------------------------------------------------
//   SMUL
//-----------------------------------------------------------------------------
static int OP_SMUL_B_B(const u32 i) { OP_MULxy_(emit_smulbb(g_out, lhs, rhs), L, L, 0, 0, 0); }
static int OP_SMUL_B_T(const u32 i) { OP_MULxy_(emit_smulbt(g_out, lhs, rhs), L, H, 0, 0, 0); }
static int OP_SMUL_T_B(const u32 i) { OP_MULxy_(emit_smultb(g_out, lhs, rhs), H, L, 0, 0, 0); }
static int OP_SMUL_T_T(const u32 i) { OP_MULxy_(emit_smultt(g_out, lhs, rhs), H, H, 0, 0, 0); }

//-----------------------------------------------------------------------------
//   SMLA
//-----------------------------------------------------------------------------
static int OP_SMLA_B_B(const u32 i) { 	
	OP_MULxy_(emit_smlabb(g_out, hi,reg_pos_ptr(12, R0, VALUE),lhs,rhs), L, L, 0, 1, 1); 
}
static int OP_SMLA_B_T(const u32 i) { OP_MULxy_(emit_smlabt(g_out, hi,reg_pos_ptr(12, R0, VALUE),lhs,rhs), L, H, 0, 1, 1); }
static int OP_SMLA_T_B(const u32 i) { OP_MULxy_(emit_smlatb(g_out, hi,reg_pos_ptr(12, R0, VALUE),lhs,rhs), H, L, 0, 1, 1); }
static int OP_SMLA_T_T(const u32 i) { OP_MULxy_(emit_smlatt(g_out, hi,reg_pos_ptr(12, R0, VALUE),lhs,rhs), H, H, 0, 1, 1); }

//-----------------------------------------------------------------------------
//   SMLAL
//-----------------------------------------------------------------------------
static int OP_SMLAL_B_B(const u32 i) { OP_MULxy_(emit_smlalbb(g_out, reg_pos_ptr(12, R0, VALUE), reg_pos_ptr(16, R1, VALUE),lhs,rhs), L, L, 1, 1, 1); }
static int OP_SMLAL_B_T(const u32 i) { OP_MULxy_(emit_smlalbt(g_out, reg_pos_ptr(12, R0, VALUE), reg_pos_ptr(16, R1, VALUE),lhs,rhs), L, H, 1, 1, 1); }
static int OP_SMLAL_T_B(const u32 i) { OP_MULxy_(emit_smlaltb(g_out, reg_pos_ptr(12, R0, VALUE), reg_pos_ptr(16, R1, VALUE),lhs,rhs), H, L, 1, 1, 1); }
static int OP_SMLAL_T_T(const u32 i) { OP_MULxy_(emit_smlaltt(g_out, reg_pos_ptr(12, R0, VALUE), reg_pos_ptr(16, R1, VALUE),lhs,rhs), H, H, 1, 1, 1); }

#undef hi

//-----------------------------------------------------------------------------
//   SMULW / SMLAW
//-----------------------------------------------------------------------------

#define OP_SMxxW_(op, x, accum, flags) \
	reg_pos_ptr(8, lhs, VALUE);\
	reg_pos_ptr(0, rhs, VALUE); \
	op; \
	if (accum) { emit_write_ptr32_regptrTO_regFROM(g_out, reg_pos_ptr(16, R3, ADDRESS), hi); } \
	else { emit_write_ptr32_regptrTO_regFROM(g_out, reg_pos_ptr(16, R3, ADDRESS), lhs); } \
	if (flags) { SET_Q; } \
	return 1;

#define hi R2

static int OP_SMULW_B(const u32 i) { OP_SMxxW_(emit_smulwb(g_out, lhs, rhs), L, 0, 0); }
static int OP_SMULW_T(const u32 i) { OP_SMxxW_(emit_smulwt(g_out, lhs, rhs), H, 0, 0); }
static int OP_SMLAW_B(const u32 i) { OP_SMxxW_(emit_smlawb(g_out, hi,reg_pos_ptr(12, R0, VALUE),lhs,rhs), L, 1, 1); }
static int OP_SMLAW_T(const u32 i) { OP_SMxxW_(emit_smlawt(g_out, hi,reg_pos_ptr(12, R0, VALUE),lhs,rhs), H, 1, 1); }


#undef hi

//-----------------------------------------------------------------------------
//   MRS / MSR
//-----------------------------------------------------------------------------
static int OP_MRS_CPSR(const u32 i)
{
	cpu_ptr(CPSR, x_reg, VALUE);
	emit_write_ptr32_regptrTO_regFROM(g_out, reg_pos_ptr(12, R1, ADDRESS), x_reg);
	
	return 1;
}

static int OP_MRS_SPSR(const u32 i)
{
	cpu_ptr(SPSR, x_reg, VALUE);
	emit_write_ptr32_regptrTO_regFROM(g_out, reg_pos_ptr(12, R1, ADDRESS), x_reg);
	
	return 1;
}

#define OP_MSR_(reg, args, sw) \
	args; \
	emit_mov(g_out, rhs, operand); \
	switch (((i>>16) & 0xF)) \
	{ \
		case 0x1:		 \
			{ \
				unsigned int __skip=genlabel(); \
				cpu_ptr(CPSR, mode, VALUE); \
				emit_andimm(g_out, mode, 0x1F, mode); \
				emit_cmpimm(g_out, mode, USR); \
				emit_branch_label(g_out, __skip, 0x0); \
				if (sw) \
				{ \
					emit_mov(g_out, rhs_reg, mode); \
					emit_andimm(g_out, mode, 0x1F, mode); \
					emit_ptr(g_out, (uintptr_t)&ARMPROC, R0); \
					emit_mov(g_out, mode, R1);\
					emit_ptr(g_out, (uintptr_t)armcpu_switchMode, R3); \
					callR3(g_out); \
				} \
				emit_writeBYTE_ptr32_regptrTO_regFROM(g_out, cpu_ptr_byte(reg, 0, R1, ADDRESS), operand); \
				changeCPSR; \
				emit_label(g_out, (int)__skip); \
			} \
			return 1; \
		case 0x2:		  \
			{ \
				unsigned int __skip=genlabel(); \
				cpu_ptr(CPSR, mode, VALUE); \
				emit_andimm(g_out, mode, 0x1F, mode); \
				emit_cmpimm(g_out, mode, USR); \
				emit_branch_label(g_out, __skip, 0x0); \
				emit_lsr(g_out, operand, 8); \
				emit_writeBYTE_ptr32_regptrTO_regFROM(g_out, cpu_ptr_byte(reg, 1, R1, ADDRESS), operand); \
				changeCPSR; \
				emit_label(g_out, (int)__skip); \
			} \
			return 1; \
		case 0x4:		  \
			{ \
				unsigned int __skip=genlabel(); \
				cpu_ptr(CPSR, mode, VALUE); \
				emit_andimm(g_out, mode, 0x1F, mode); \
				emit_cmpimm(g_out, mode, USR); \
				emit_branch_label(g_out, __skip, 0x0); \
				emit_lsr(g_out, operand, 16); \
				emit_writeBYTE_ptr32_regptrTO_regFROM(g_out, cpu_ptr_byte(reg, 2, R1, ADDRESS), operand); \
				changeCPSR; \
				emit_label(g_out, (int)__skip); \
			} \
			return 1; \
		case 0x8:		  \
			{ \
				emit_lsr(g_out, operand, 24); \
				emit_writeBYTE_ptr32_regptrTO_regFROM(g_out, cpu_ptr_byte(reg, 3, R1, ADDRESS), operand); \
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
	cpu_ptr(reg.val, xPSR_mem, ADDRESS); \
	unsigned int __USR=genlabel(); \
	unsigned int __done=genlabel(); \
	cpu_ptr(CPSR.val, mode, VALUE); \
	emit_andimm(g_out, mode, 0x1F, mode); \
	emit_cmpimm(g_out, mode, USR); \
	emit_branch_label(g_out, __USR, 0x0); \
	if (sw && BIT16(i)) \
	{ \
		  \
		emit_mov(g_out, rhs_reg, mode); \
		emit_andimm(g_out, mode, 0x1F, mode); \
		emit_ptr(g_out, (uintptr_t)&ARMPROC, R0); \
		emit_mov(g_out, mode, R1);\
		emit_ptr(g_out, (uintptr_t)armcpu_switchMode, R3); \
		callR3(g_out); \
	} \
	cpu_ptr(reg.val, xPSR, VALUE); \
	emit_andimm(g_out, operand, byte_mask, operand); \
	emit_andimm(g_out, xPSR, ~byte_mask, xPSR); \
	emit_or(g_out, xPSR, operand, xPSR); \
	emit_write_ptr32_regptrTO_regFROM(g_out, cpu_ptr(reg.val, R1, ADDRESS), xPSR); \
	emit_branch_label(g_out, __done, 0xE); \
	emit_label(g_out, (int)__USR); \
	cpu_ptr(reg.val, xPSR, VALUE); \
	emit_andimm(g_out, operand, byte_mask_USR, operand); \
	emit_andimm(g_out, xPSR, ~byte_mask_USR, xPSR); \
	emit_or(g_out, xPSR, operand, xPSR); \
	emit_write_ptr32_regptrTO_regFROM(g_out, cpu_ptr(reg.val, R1, ADDRESS), xPSR); \
	emit_label(g_out, (int)__done); \
	changeCPSR; \
	return 1;
	
#define operand R8
#define mode R9
#define xPSR R4
#define xPSR_mem R3	

static int OP_MSR_CPSR(const u32 i) { OP_MSR_(CPSR, REG_OFF, 1); }
static int OP_MSR_SPSR(const u32 i) { OP_MSR_(SPSR, REG_OFF, 0); }
static int OP_MSR_CPSR_IMM_VAL(const u32 i) { OP_MSR_(CPSR, IMM_VAL, 1); }
static int OP_MSR_SPSR_IMM_VAL(const u32 i) { OP_MSR_(SPSR, IMM_VAL, 0); }

#undef operand
#undef mode
#undef xPSR
#undef xPSR_mem

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
	u32 ret =  MMU_aluMemAccessCycles<PROCNUM,8,MMU_AD_READ>(3,adr);
	return ret;
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

static u32 add(u32 lhst, u32 rhst) { return lhst + rhst; }
static u32 sub(u32 lhst, u32 rhst) { return lhst - rhst; }

#define OP_LDR_(mem_op, arg, sign_op, writeback) \
	reg_pos_ptr(16, adr, VALUE); \
	reg_pos_ptr(12, dst, ADDRESS); \
	arg; \
	if(!rhs_is_imm || *(u32*)&rhs_var) \
	{ \
		if(writeback == 0) \
			emit_##sign_op(g_out, adr, rhs_reg, adr); \
		else if(writeback < 0) \
		{ \
			emit_##sign_op(g_out, adr, rhs_reg, adr); \
			emit_write_ptr32_regptrTO_regFROM(g_out, reg_pos_ptr(16, R1, ADDRESS), adr); \
		} \
		else if(writeback > 0) \
		{ \
			emit_mov(g_out, adr, tmp_reg); \
			emit_##sign_op(g_out, tmp_reg, rhs_reg, tmp_reg); \
			emit_write_ptr32_regptrTO_regFROM(g_out, reg_pos_ptr(16, R1, ADDRESS), tmp_reg); \
		} \
	} \
	u32 adr_first = sign_op(cpu->R[REG_POS(i,16)], rhs_first); \
	emit_mov64(g_out,adr, R0); \
	emit_mov64(g_out, dst, R1); \
	emit_ptr(g_out, (uintptr_t)mem_op##_tab[PROCNUM][classify_adr(adr_first,0)], R3); \
	callR3(g_out); \
	emit_write_ptr32_reg(g_out, (uintptr_t)&bb_cycles, R0); \
	if(REG_POS(i,12)==15) \
	{ \
		reg_ptr(15, tmp_reg, VALUE); \
		if (PROCNUM == 0) \
		{ \
			emit_mov(g_out, tmp_reg, tmp2_reg); \
			emit_andimm(g_out, tmp2_reg, 1, tmp2_reg); \
			emit_lsl(g_out, tmp2_reg, 5); \
			emit_or_ptr32_regptr_reg(g_out, cpu_ptr(CPSR, R1, ADDRESS), tmp2_reg); \
			emit_andimm(g_out, tmp_reg, 0xFFFFFFFE, tmp_reg); \
		} \
		else \
		{ \
			emit_andimm(g_out, tmp_reg, 0xFFFFFFFC, tmp_reg); \
		} \
		emit_write_ptr32_regptrTO_regFROM(g_out, cpu_ptr(next_instruction, R1, ADDRESS), tmp_reg); \
	} \
	return 1;
	
#define dst R8
#define adr R9

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

#undef dst
#undef adr

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
	reg_pos_ptr(12, data, VALUE); \
	reg_pos_ptr(16, R8, VALUE); \
	arg; \
	emit_mov(g_out, R8, adr); \
	if(!rhs_is_imm || *(u32*)&rhs_var) \
	{ \
		if(writeback == 0) \
			emit_##sign_op(g_out, adr, rhs_reg, adr); \
		else if(writeback < 0) \
		{ \
			emit_##sign_op(g_out, adr, rhs_reg, adr); \
			emit_write_ptr32_regptrTO_regFROM(g_out, reg_pos_ptr(16, R3, ADDRESS), adr); \
		} \
		else if(writeback > 0) \
		{ \
			emit_mov(g_out, adr, tmp_reg); \
			emit_##sign_op(g_out, tmp_reg, rhs_reg, tmp_reg); \
			emit_write_ptr32_regptrTO_regFROM(g_out, reg_pos_ptr(16, R3, ADDRESS), tmp_reg); \
		} \
	} \
	u32 adr_first = sign_op(cpu->R[REG_POS(i,16)], rhs_first); \
	emit_ptr(g_out, (uintptr_t)mem_op##_tab[PROCNUM][classify_adr(adr_first,1)], R3); \
	callR3(g_out); \
	emit_write_ptr32_reg(g_out, (uintptr_t)&bb_cycles, R0); \
	return 1;
	
#define data R1
#define adr R0

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

#undef data
#undef adr

//-----------------------------------------------------------------------------
//   LDRD / STRD
//-----------------------------------------------------------------------------
typedef u32 FASTCALL (*LDRD_STRD_REG)(u32);
template<int PROCNUM, u8 Rnum>
static u32 FASTCALL OP_LDRD_REG(u32 adr)
{
	cpu->R[Rnum] = READ32(cpu->mem_if->data, adr);
	
	// For even-numbered registers, we'll do a double-word load. Otherwise, we'll just do a single-word load.
	if ((Rnum & 0x01) == 0)
	{
		cpu->R[(Rnum & 0xE) + 1] = READ32(cpu->mem_if->data, adr+4);
		return (MMU_memAccessCycles<PROCNUM,32,MMU_AD_READ>(adr) + MMU_memAccessCycles<PROCNUM,32,MMU_AD_READ>(adr+4));
	}
	
	return MMU_memAccessCycles<PROCNUM,32,MMU_AD_READ>(adr);
}
template<int PROCNUM, u8 Rnum>
static u32 FASTCALL OP_STRD_REG(u32 adr)
{
	WRITE32(cpu->mem_if->data, adr, cpu->R[Rnum]);
	
	// For even-numbered registers, we'll do a double-word store. Otherwise, we'll just do a single-word store.
	if ((Rnum & 0x01) == 0)
	{
		WRITE32(cpu->mem_if->data, adr+4, cpu->R[(Rnum & 0xE) + 1]);
		return (MMU_memAccessCycles<PROCNUM,32,MMU_AD_WRITE>(adr) + MMU_memAccessCycles<PROCNUM,32,MMU_AD_WRITE>(adr+4));
	}
	
	return MMU_memAccessCycles<PROCNUM,32,MMU_AD_WRITE>(adr);
}
#define T(op, proc) op<proc,0>, op<proc,1>, op<proc,2>, op<proc,3>, op<proc,4>, op<proc,5>, op<proc,6>, op<proc,7>, op<proc,8>, op<proc,9>, op<proc,10>, op<proc,11>, op<proc,12>, op<proc,13>, op<proc,14>, op<proc,15>
static const LDRD_STRD_REG op_ldrd_tab[2][16] = { {T(OP_LDRD_REG, 0)}, {T(OP_LDRD_REG, 1)} };
static const LDRD_STRD_REG op_strd_tab[2][16] = { {T(OP_STRD_REG, 0)}, {T(OP_STRD_REG, 1)} };
#undef T

static int OP_LDRD_STRD_POST_INDEX(const u32 i) 
{
	#define addr R2
	#define Rd R4
	#define Rs R5
	
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
	
	reg_pos_ptr(16, Rd, VALUE);
	emit_mov(g_out, Rd, addr);

	// I bit - immediate or register
	if (BIT22(i))
	{
		IMM_OFF;
		BIT23(i)?emit_add_ptr32_regptrTO_regFROM(g_out, CACHED_PTR(reg_pos_ptr(16, R1, ADDRESS)), rhs):emit_sub_ptr32_regptrTO_regFROM(g_out, CACHED_PTR(reg_pos_ptr(16, R1, ADDRESS)), rhs);
	}
	else
	{
		#define idx R3
		reg_pos_ptr(0, idx, VALUE);
		BIT23(i)?emit_add_ptr32_regptrTO_regFROM(g_out, CACHED_PTR(reg_pos_ptr(16, R1, ADDRESS)), idx):emit_sub_ptr32_regptrTO_regFROM(g_out, CACHED_PTR(reg_pos_ptr(16, R1, ADDRESS)), idx);
		#undef idx
	}
	
	// Setup args
	emit_mov(g_out,addr, R0);
	// Add function
	emit_ptr(g_out, (uintptr_t)(BIT5(i) ? op_strd_tab[PROCNUM][Rd_num] : op_ldrd_tab[PROCNUM][Rd_num]), R3);
	// Call function
	callR3(g_out);
	emit_write_ptr32_reg(g_out, (uintptr_t)&bb_cycles, R0);

	emit_MMU_aluMemCycles(3, &bb_cycles, 0);	

	#undef addr
	#undef Rd
	#undef Rs
	
	return 1;
}

static int OP_LDRD_STRD_OFFSET_PRE_INDEX(const u32 i)
{
	#define addr R2
	#define Rd R4
	#define Rs R5
	
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
	
	reg_pos_ptr(16, Rd, VALUE);
	emit_mov(g_out, Rd, addr);

	// I bit - immediate or register
	if (BIT22(i))
	{
		IMM_OFF;
		BIT23(i)?emit_add(g_out, addr, rhs, addr):emit_sub(g_out, addr, rhs, addr);
	}
	else {
		reg_pos_ptr(0, R1, VALUE);
		BIT23(i)?emit_add(g_out, addr, R1, addr):emit_sub(g_out, addr, R1, addr);
	}

	if (BIT5(i))		// Store
	{
		// Setup args
		emit_mov(g_out,addr, R0);
		// Add function
		emit_ptr(g_out, (uintptr_t)op_strd_tab[PROCNUM][Rd_num], R3);
		// Call function
		callR3(g_out);
		// Return
		emit_write_ptr32_reg(g_out, (uintptr_t)&bb_cycles, R0);
		
		if (BIT21(i)) // W bit - writeback
			emit_write_ptr32_regptrTO_regFROM(g_out, reg_pos_ptr(16, R1, ADDRESS), addr);
		emit_MMU_aluMemCycles(3, &bb_cycles, 0);
	}
	else				// Load
	{
		if (BIT21(i)) // W bit - writeback
			emit_write_ptr32_regptrTO_regFROM(g_out, reg_pos_ptr(16, R1, ADDRESS), addr);
			
		// Setup args
		emit_mov(g_out,addr, R0);
		// Add function
		emit_ptr(g_out, (uintptr_t)op_ldrd_tab[PROCNUM][Rd_num], R3);
		// Call function
		callR3(g_out);
		// Return
		emit_write_ptr32_reg(g_out, (uintptr_t)&bb_cycles, R0);			

		emit_MMU_aluMemCycles(3, &bb_cycles, 0);
	}
	
	#undef addr
	#undef Rd
	#undef Rs
	
	return 1;
}

//-----------------------------------------------------------------------------
//   SWP/SWPB
//-----------------------------------------------------------------------------
template<int PROCNUM>
static u32 FASTCALL op_swp(u32 adr, u32 *Rd, u32 Rs)
{
	u32 tmpt = ROR(READ32(cpu->mem_if->data, adr), (adr & 3)<<3);
	WRITE32(cpu->mem_if->data, adr, Rs);
	*Rd = tmpt;
	return (MMU_memAccessCycles<PROCNUM,32,MMU_AD_READ>(adr) + MMU_memAccessCycles<PROCNUM,32,MMU_AD_WRITE>(adr));
}
template<int PROCNUM>
static u32 FASTCALL op_swpb(u32 adr, u32 *Rd, u32 Rs)
{
	u32 tmpt = READ8(cpu->mem_if->data, adr);
	WRITE8(cpu->mem_if->data, adr, Rs);
	*Rd = tmpt;
	return (MMU_memAccessCycles<PROCNUM,8,MMU_AD_READ>(adr) + MMU_memAccessCycles<PROCNUM,8,MMU_AD_WRITE>(adr));
}

typedef u32 FASTCALL (*OP_SWP_SWPB)(u32, u32*, u32);
static const OP_SWP_SWPB op_swp_tab[2][2] = {{ op_swp<0>, op_swp<1> }, { op_swpb<0>, op_swpb<1> }};

static int op_swp_(const u32 i, int b)
{
	#define addr R2
	#define Rd R4
	#define Rs R5
	
	reg_pos_ptr(16, addr, VALUE);
	reg_pos_ptr(12, Rd, ADDRESS);
	
	if(b)
		reg_pos_ptrB(0, Rs, VALUE);
	else
		reg_pos_ptr(0, Rs, VALUE);
	
	emit_mov(g_out, addr, R0);
	emit_mov64(g_out, Rd, R1);
	emit_mov(g_out, Rs, R2);
	
	// Add function
	//emit_movimm(g_out, (uintptr_t)op_swp_tab[b][PROCNUM], R3);
	emit_ptr(g_out, (uintptr_t)op_swp_tab[b][PROCNUM], R3);
	// Call function
	callR3(g_out);
	//emit_mov(g_out, PC, LR);
	//emit_jmpreg(g_out, R3);
	// Return
	emit_write_ptr32_reg(g_out, (uintptr_t)&bb_cycles, R0);

	emit_MMU_aluMemCycles(4, &bb_cycles, 0);
	
	#undef addr
	#undef Rd
	#undef Rs
	
	return 1;
}

static int OP_SWP(const u32 i) { return op_swp_(i, 0); }
static int OP_SWPB(const u32 i) { return op_swp_(i, 1); }

//-----------------------------------------------------------------------------
//   LDMIA / LDMIB / LDMDA / LDMDB / STMIA / STMIB / STMDA / STMDB
//-----------------------------------------------------------------------------
static u32 popregcount(u32 xt)
{
	uint32_t pop = 0;
	for(; xt; xt>>=1)
		pop += xt&1;
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
static u32 FASTCALL OP_LDM_STM(u32 adr, u32 regslo, u32 regshi, int n)
{
	// TODO use classify_adr?
	u32 cycles;
	u8 *ptr;
	u64 regs = (u64)regslo | (u64)((u64)regshi << 32);
	
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

typedef u32 FASTCALL (*LDMOpFunc)(u32,u32,u32,int);
static const LDMOpFunc op_ldm_stm_tab[2][2][2] = {{
	{ OP_LDM_STM<0,0,-1>, OP_LDM_STM<0,0,+1> },
	{ OP_LDM_STM<0,1,-1>, OP_LDM_STM<0,1,+1> },
},{
	{ OP_LDM_STM<1,0,-1>, OP_LDM_STM<1,0,+1> },
	{ OP_LDM_STM<1,1,-1>, OP_LDM_STM<1,1,+1> },
}};

static void call_ldm_stm(int reg, u32 bitmask, bool store, int dir)
{
	if(bitmask)
	{
		// Setup args
		emit_mov(g_out,reg,R0);
		emit_movimm(g_out, (uintptr_t)popregcount(bitmask), R3);
		emit_movimm(g_out, (uintptr_t)get_reg_list(bitmask, dir), R1);
		emit_movimm(g_out, (uintptr_t)(get_reg_list(bitmask, dir) >> 32), R2);
		
		// Add function
		emit_ptr(g_out, (uintptr_t)op_ldm_stm_tab[PROCNUM][store][dir>0], R7);
		
		// Call function
		callR7(g_out);
		
		emit_write_ptr32_reg(g_out, (uintptr_t)&bb_cycles, R0);

	}
	else
		bb_constant_cycles++;
}

static int op_bx(/*Mem */int srcreg, bool blx, bool test_thumb);
static int op_bx_thumb(/*Mem */ int srcreg, bool blx, bool test_thumb);

static int op_ldm_stm(u32 i, bool store, int dir, bool before, bool writeback)
{
	#define adr R4
	#define oldmode R5
	
	u32 bitmask = i & 0xFFFF;
	u32 pop = popregcount(bitmask);	
	
	reg_pos_ptr(16, adr, VALUE);
	if(before)
		emit_addimm(g_out, adr, 4*dir, adr);

	call_ldm_stm(adr, bitmask, store, dir);

	if(BIT15(i) && !store)
	{
		op_bx(reg_ptr(15, R6, VALUE), 0, PROCNUM == ARMCPU_ARM9);
	}

	if(writeback)
	{
		
		if(store || !(i & (1 << REG_POS(i,16))))
		{
			JIT_COMMENT("--- writeback");
			reg_pos_ptr(16, R3, VALUE);
			emit_addimm(g_out, R3, 4*dir*pop, R3);
			emit_write_ptr32_regptrTO_regFROM(g_out, reg_pos_ptr(16, R1, ADDRESS), R3);
		}
		else 
		{
			u32 bitlist = (~((2 << REG_POS(i,16))-1)) & 0xFFFF;
			if(i & bitlist)
			{
				JIT_COMMENT("--- writeback");
				emit_addimm(g_out, adr, 4*dir*(pop-before), adr);
				emit_write_ptr32_regptrTO_regFROM(g_out, reg_pos_ptr(16, R1, ADDRESS), adr);
			}
		}
	}

	emit_MMU_aluMemCycles(store ? 1 : 2, &bb_cycles, pop);
	
	#undef adr
	#undef oldmode
	
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
	u32 pop = popregcount(bitmask);
	bool bit15 = BIT15(i);

	u32 adr_first = cpu->R[REG_POS(i, 16)];

	#define adr R4
	#define oldmode R5
	
	reg_pos_ptr(16, adr, VALUE);

	if(before)
		emit_addimm(g_out, adr, 4*dir, adr);

	if (!bit15 || store)
	{  
		emit_ptr(g_out, (uintptr_t)&ARMPROC, R0);
		emit_movimm(g_out, SYS, R1);
		emit_ptr(g_out, (uintptr_t)armcpu_switchMode, R3);
		callR3(g_out);
		emit_mov(g_out, R0, oldmode);
	}

	call_ldm_stm(adr, bitmask, store, dir);

	if(!bit15 || store)
	{	
		emit_ptr(g_out, (uintptr_t)&ARMPROC, R0);
		emit_mov(g_out, oldmode, R1);
		emit_ptr(g_out, (uintptr_t)armcpu_switchMode, R3);
		callR3(g_out);		
	}
	else
	{
		S_DST_R15;
	}

	// FIXME
	if(writeback)
	{
		if(store || !(i & (1 << REG_POS(i,16)))) {
			emit_add_ptr32_regptrTO_immFROM(g_out, reg_pos_ptr(16, R1, ADDRESS), 4*dir*pop);
		}
		else 
		{
			u32 bitlist = (~((2 << REG_POS(i,16))-1)) & 0xFFFF;
			if(i & bitlist)
			{
				emit_addimm(g_out, adr, 4*dir*(pop-before), adr);
				emit_write_ptr32_regptrTO_regFROM(g_out, reg_pos_ptr(16, R1, ADDRESS), adr);
			}
		}
	}

	emit_MMU_aluMemCycles(store ? 1 : 2, &bb_cycles, pop);
	
	#undef adr
	#undef oldmode
	
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
		emit_or_ptr8_regptr_imm(g_out, cpu_ptr_byte(CPSR, 0, R1, ADDRESS), 1<<5);
	}
	if(bl || CONDITION(i)==0xF)
		emit_write_ptr32_regptrTO_immFROM(g_out, reg_ptr(14, R1, ADDRESS), bb_next_instruction);

	emit_write_ptr32_regptrTO_immFROM(g_out, cpu_ptr(instruct_adr, R1, ADDRESS), dst);
		
	return 1;
}

static int OP_B(const u32 i) { return op_b(i, 0); }
static int OP_BL(const u32 i) { return op_b(i, 1); }

static int op_bx(/*Mem */int srcreg, bool blx, bool test_thumb)
{
	#define thumb tmp2_reg
	#define dst tmp_reg
	#define mask R8
		
	emit_mov(g_out, srcreg, dst);

	if(test_thumb)
	{
		emit_mov(g_out, dst, thumb);
		
		emit_andimm(g_out, thumb, 1, thumb);
		emit_mov(g_out, thumb,R9);
		emit_lea_ptr_reg(g_out, 0xFFFFFFFC, R9, 2, mask);
		emit_lsl(g_out, thumb, 5);
				
		emit_or_ptr8_regptr_reg(g_out, cpu_ptr_byte(CPSR, 0, R1, ADDRESS), thumb);
				
		emit_and(g_out, dst, mask, dst);
	}
	else
		emit_andimm(g_out, dst, 0xFFFFFFFC, dst);

	if(blx)
		emit_write_ptr32_regptrTO_immFROM(g_out, reg_ptr(14, R1, ADDRESS), bb_next_instruction);
	emit_write_ptr32_regptrTO_regFROM(g_out, cpu_ptr(instruct_adr, R1, ADDRESS), dst);
	
	#undef thumb
	#undef dst
	#undef mask
	
	return 1;
}

static int OP_BX(const u32 i) { return op_bx(reg_pos_ptr(0, R1, VALUE), 0, 1); }
static int OP_BLX_REG(const u32 i) { return op_bx(reg_pos_ptr(0, R1, VALUE), 1, 1); }

//-----------------------------------------------------------------------------
//   CLZ
//-----------------------------------------------------------------------------
static int OP_CLZ(const u32 i)
{
	reg_pos_ptr(0, tmp2_reg, VALUE);
		
	emit_clz(g_out, tmp2_reg, tmp_reg);
	
	emit_write_ptr32_regptrTO_regFROM(g_out, reg_pos_ptr(12, R1, ADDRESS), tmp_reg);
	
	return 1;
}

//-----------------------------------------------------------------------------
//   MCR / MRC
//-----------------------------------------------------------------------------

// precalculate region masks/sets from cp15 register ----- JIT
static void maskPrecalc(u32 _num)
{
#define precalc(num) {  \
	u32 mask = 0, set = 0xFFFFFFFF ; /* (x & 0) == 0xFF..FF is allways false (disabled) */  \
	if (BIT_N(cp15.protectBaseSize[num],0)) /* if region is enabled */ \
	{    /* reason for this define: naming includes var */  \
		mask = CP15_MASKFROMREG(cp15.protectBaseSize[num]) ;   \
		set = CP15_SETFROMREG(cp15.protectBaseSize[num]) ; \
		if (CP15_SIZEIDENTIFIER(cp15.protectBaseSize[num])==0x1F)  \
		{   /* for the 4GB region, u32 suffers wraparound */   \
			mask = 0 ; set = 0 ;   /* (x & 0) == 0  is allways true (enabled) */  \
		} \
	}  \
	armcp15_setSingleRegionAccess(&cp15, num, mask, set) ;  \
}
	switch(_num)
	{
		case 0: precalc(0); break;
		case 1: precalc(1); break;
		case 2: precalc(2); break;
		case 3: precalc(3); break;
		case 4: precalc(4); break;
		case 5: precalc(5); break;
		case 6: precalc(6); break;
		case 7: precalc(7); break;

		case 0xFF:
			precalc(0);
			precalc(1);
			precalc(2);
			precalc(3);
			precalc(4);
			precalc(5);
			precalc(6);
			precalc(7);
		break;
	}
#undef precalc
}

#define _maskPrecalc(num) \
{ \
	emit_movimm(g_out, num, R0);\
	emit_ptr(g_out, (uintptr_t)maskPrecalc, R3); \
	callR3(g_out); \
}

static int OP_MCR(const u32 i)
{
	#define data tmp2_reg
	
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

	static uintptr_t bb_cp15;
	reg_pos_ptr(12, data, VALUE);
	emit_write_ptr(g_out, (uintptr_t)&bb_cp15, (uintptr_t)&cp15);

	bool bUnknown = false;
	switch(CRn)
	{
		case 1:
			if((opcode1==0) && (opcode2==0) && (CRm==0))
			{
				static uintptr_t bb_mmu;
				emit_write_ptr(g_out, (uintptr_t)&bb_mmu, (uintptr_t)&MMU);

				
				mmu_ptr_byte(ARM9_RW_MODE, R2, ADDRESS);
					
				emit_testimm(g_out, data, (1<<7));
				emit_setnz(g_out, R1);
				emit_strb(g_out, R2, R1);
				
				#define vec R6

				emit_movimm(g_out, 0xFFFF0000, tmp);
				emit_movimm(g_out, 0, vec);
				emit_xor(g_out, vec, vec, vec);
				emit_testimm(g_out, data, (1 << 13));
				
				emit_mov_cond(g_out, tmp, vec, 0x1);
				emit_write_ptr32_regptrTO_regFROM(g_out, cpu_ptr(intVector, R1, ADDRESS), vec);
				
				cpu_ptr_byte(LDTBit, 0, R2, ADDRESS);
				
				emit_testimm(g_out, data, (1 << 15));	
				emit_setz(g_out, R1);
				emit_strb(g_out, R2, R1);
				
				emit_andimm(g_out, data, 0x000FF085, data);
				emit_orimm(g_out, data, 0x00000078, data);

				emit_write_ptr32_regptrTO_regFROM(g_out, cp15_ptr(ctrl, R1, ADDRESS), data);
				
				#undef vec
				
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
						emit_write_ptr32_regptrTO_regFROM(g_out, cp15_ptr(DCConfig, R1, ADDRESS), data);
						break;
					case 1:
						emit_write_ptr32_regptrTO_regFROM(g_out, cp15_ptr(ICConfig, R1, ADDRESS), data);
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
				emit_write_ptr32_regptrTO_regFROM(g_out, cp15_ptr(writeBuffCtrl, R1, ADDRESS), data);
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
						emit_write_ptr32_regptrTO_regFROM(g_out, cp15_ptr(DaccessPerm, R1, ADDRESS), data);
						_maskPrecalc(0xFF);
						break;
					case 3:
						emit_write_ptr32_regptrTO_regFROM(g_out, cp15_ptr(IaccessPerm, R1, ADDRESS), data);
						_maskPrecalc(0xFF);
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
				if (CRm < 8)
				{
					emit_write_ptr32_regptrTO_regFROM(g_out, cp15_ptr_off(protectBaseSize, (CRm * sizeof(u32)), R1, ADDRESS), data);
					_maskPrecalc(CRm);
					break;
				}
			}
			bUnknown = true;
			break;
		case 7:
			if((CRm==0)&&(opcode1==0)&&((opcode2==4)))
			{
				emit_write_ptr32_regptrTO_immFROM(g_out, cpu_ptr(freeze, R1, ADDRESS), CPU_FREEZE_IRQ_IE_IF);
				break;
			}
			bUnknown = true;
			break;
		case 9:
			if(opcode1==0)
			{
				switch(CRm)
				{
					case 0:
						switch(opcode2)
						{
							case 0:
								emit_write_ptr32_regptrTO_regFROM(g_out, cp15_ptr(DcacheLock, R1, ADDRESS), data);
								break;
							case 1:
								emit_write_ptr32_regptrTO_regFROM(g_out, cp15_ptr(IcacheLock, R1, ADDRESS), data);
								break;
							default:
								bUnknown = true;
								break;
						}
					case 1:
						switch(opcode2)
						{
							case 0:
								{
									emit_andimm(g_out, data, 0x0FFFF000, data);
									static uintptr_t bb_mmu;
									emit_write_ptr(g_out, (uintptr_t)&bb_mmu, (uintptr_t)&MMU);
									emit_write_ptr32_regptrTO_regFROM(g_out, mmu_ptr(DTCMRegion, R1, ADDRESS), data);
									emit_write_ptr32_regptrTO_regFROM(g_out, cp15_ptr(DTCMRegion, R1, ADDRESS), data);
								}
								break;
							case 1:
								{
									static uintptr_t bb_mmu;
									emit_write_ptr(g_out, (uintptr_t)&bb_mmu, (uintptr_t)&MMU);
									emit_write_ptr32_regptrTO_immFROM(g_out, mmu_ptr(ITCMRegion, R1, ADDRESS), 0);
									emit_write_ptr32_regptrTO_regFROM(g_out, cp15_ptr(ITCMRegion, R1, ADDRESS), data);
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
	
	#undef data

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
	
	#define data tmp2_reg

	static uintptr_t bb_cp15;
	emit_write_ptr(g_out, (uintptr_t)&bb_cp15, (uintptr_t)&cp15);
	
	bool bUnknown = false;
	switch(CRn)
	{
		case 0:
			if((opcode1 == 0)&&(CRm==0))
			{
				switch(opcode2)
				{
					case 1:
						cp15_ptr(cacheType, data, VALUE);
						break;
					case 2:
						cp15_ptr(TCMSize, data, VALUE);
						break;
					default:		// FIXME
						cp15_ptr(IDCode, data, VALUE);
						break;
				}
				break;
			}
			bUnknown = true;
			break;
		
		case 1:
			if((opcode1==0) && (opcode2==0) && (CRm==0))
			{
				cp15_ptr(ctrl, data, VALUE);
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
						cp15_ptr(DCConfig, data, VALUE);
						break;
					case 1:
						cp15_ptr(ICConfig, data, VALUE);
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
				cp15_ptr(writeBuffCtrl, data, VALUE);
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
						cp15_ptr(DaccessPerm, data, VALUE);
						break;
					case 3:
						cp15_ptr(IaccessPerm, data, VALUE);
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
				if (CRm < 8)
				{
					cp15_ptr_off(protectBaseSize, CRm * sizeof(u32), data, VALUE);
					break;
				}
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
								cp15_ptr(DcacheLock, data, VALUE);
								break;
							case 1:
								cp15_ptr(IcacheLock, data, VALUE);
								break;
							default:
								bUnknown = true;
								break;
						}

					case 1:
						switch(opcode2)
						{
							case 0:
								cp15_ptr(DTCMRegion, data, VALUE);
								break;
							case 1:
								cp15_ptr(ITCMRegion, data, VALUE);
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
		return 1;
	}

	if (REG_POS(i, 12) == 15)
	{
		emit_andimm(g_out, data, 0xF0000000, data);
		emit_and_ptr32_regptr(g_out, cpu_ptr(CPSR, R1, ADDRESS), 0x0FFFFFFF);
		emit_or_ptr32_regptr_reg(g_out, cpu_ptr(CPSR, R1, ADDRESS), data);
	}
	else {
		//if (REG_POS(i, 12) == 1) emit_brk(g_out);
		emit_write_ptr32_regptrTO_regFROM(g_out, reg_pos_ptr(12, R1, ADDRESS), data);
	}
		
	#undef data
	
	return 1;
}

u32 op_swi(u8 swinum)
{
	if(cpu->swi_tab)
	{
		// TODO:
		return 0;

		emit_ptr(g_out, (uintptr_t)ARM_swi_tab[PROCNUM][swinum], R3);
		callR3(g_out);
		emit_write_ptr32_reg(g_out, (uintptr_t)&bb_cycles, R0);
		emit_inc_ptr_imm(g_out, (uintptr_t)&bb_cycles, 3);
		return 1;
	}
	
	cpu_ptr(CPSR.val, R4, VALUE);
	
	emit_ptr(g_out, (uintptr_t)&ARMPROC, R0);
	emit_movimm(g_out, SVC, R1);
	emit_ptr(g_out, (uintptr_t)armcpu_switchMode, R3);
	callR3(g_out);
		
	emit_mov(g_out, R4, tmp2_reg);
		
	emit_write_ptr32_regptrTO_immFROM(g_out,reg_ptr(14, R1, ADDRESS), bb_next_instruction);
		
	emit_write_ptr32_regptrTO_regFROM(g_out, cpu_ptr(SPSR.val, R1, ADDRESS), tmp2_reg);
	
	cpu_ptr(CPSR.val, tmp2_reg, VALUE);
	emit_andimm(g_out, tmp2_reg, ~(1 << 5), tmp2_reg);
	emit_orimm(g_out, tmp2_reg, (1 << 7), tmp2_reg);
	emit_write_ptr32_regptrTO_regFROM(g_out, cpu_ptr(CPSR.val, R1, ADDRESS), tmp2_reg);
	
	emit_movimm(g_out, cpu->intVector+0x08, R0);
	emit_write_ptr32_regptrTO_regFROM(g_out, cpu_ptr(next_instruction, R1, ADDRESS), R0);

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
#define OP_SHIFTS_IMM(x86inst) \
	u8 cf_change = 1; \
	const u32 rhs2 = ((i>>6) & 0x1F); \
	if (_REG_NUM(i, 0) == _REG_NUM(i, 3)) \
	{ \
		reg_pos_thumb(0, R1, VALUE);\
		x86inst(g_out, R1, rhs2);\
		emit_write_ptr32_regptrTO_regFROM(g_out, CACHED_PTR(reg_pos_thumb(0, R2, ADDRESS)), R1); \
	} \
	else \
	{ \
		reg_pos_thumb(3, R1, VALUE); \
		x86inst(g_out, R1, rhs2);\
		emit_write_ptr32_regptrTO_regFROM(g_out, reg_pos_thumb(0, R2, ADDRESS), R1); \
	} \
	SET_NZC; \
	return 1;

#define OP_SHIFTS_REG(x86inst, bit) \
	u8 cf_change = 1; \
	reg_pos_thumb(3, imm_reg, VALUE); \
	x86inst(g_out, reg_pos_thumb(0, R1, VALUE), imm_reg); \
	emit_write_ptr32_regptrTO_regFROM(g_out, reg_pos_thumb(0, R0, ADDRESS), R1); \
	SET_NZC; \
	return 1;

#define OP_LOGIC(x86inst, _conv) \
	emit_mov(g_out, reg_pos_thumb(3, R1, VALUE), rhs); \
	if (_conv==1) emit_not(g_out, rhs, rhs);\
	reg_pos_thumb(0, R1, VALUE);\
	x86inst(g_out, R1, rhs, R1);\
	emit_write_ptr32_regptrTO_regFROM(g_out, CACHED_PTR(reg_pos_thumb(0, R2, ADDRESS)), R1); \
	SET_NZ(0);\
	return 1;

//-----------------------------------------------------------------------------
//   LSL / LSR / ASR / ROR
//-----------------------------------------------------------------------------
static int OP_LSL_0(const u32 i) 
{	
	if (_REG_NUM(i, 0) == _REG_NUM(i, 3))
		emit_cmpimm(g_out, reg_pos_thumb(0, R1, VALUE), 0);
	else
	{
		//emit_read_ptr32_regptrFROM_regTO(g_out, reg_pos_thumb(3, R1, VALUE), rhs);
		emit_mov(g_out, reg_pos_thumb(3, R1, VALUE), rhs);
		emit_write_ptr32_regptrTO_regFROM(g_out, reg_pos_thumb(0, R1, ADDRESS), rhs);
		
		emit_cmpimm(g_out, rhs, 0);
	}
	SET_NZ(0);
	
	return 1;
}


static int OP_LSL(const u32 i) { OP_SHIFTS_IMM(emit_lsls); }
static int OP_LSL_REG(const u32 i) { OP_SHIFTS_REG(emit_lsls_reg, 0); }

static int OP_LSR_0(const u32 i) 
{
	emit_testimm(g_out, reg_pos_thumb(3, R1, VALUE), (1 << 31));
	emit_setnz(g_out, rcf);
	SET_NZC_SHIFTS_ZERO(1);
	emit_write_ptr32_regptrTO_immFROM(g_out, reg_pos_thumb(0, R1, ADDRESS), 0);
	
	return 1;
}

static int OP_LSR(const u32 i) { OP_SHIFTS_IMM(emit_lsrs); }
static int OP_LSR_REG(const u32 i) { OP_SHIFTS_REG(emit_lsrs_reg, 31); }

static int OP_ASR_0(const u32 i)
{
	u8 cf_change = 1;
	if (_REG_NUM(i, 0) == _REG_NUM(i, 3)) {
		emit_asrs(g_out, reg_pos_thumb(0, R1, VALUE), 31);
		emit_write_ptr32_regptrTO_regFROM(g_out, CACHED_PTR(reg_pos_thumb(0, R2, ADDRESS)), R1); // TAGCHECK
	}
	else
	{
		reg_pos_thumb(3, rhs, VALUE);
		emit_asrs(g_out, rhs, 31);
		emit_write_ptr32_regptrTO_regFROM(g_out, reg_pos_thumb(0, R1, ADDRESS), rhs);
	}
	SET_NZC;
	
	return 1;
}

static int OP_ASR(const u32 i) { OP_SHIFTS_IMM(emit_asrs); }
static int OP_ASR_REG(const u32 i) 
{
	u8 cf_change = 1;
	GET_CARRY(0);
	reg_pos_thumb(3, imm_reg, VALUE);
	emit_asrs_reg(g_out, reg_pos_thumb(0, R1, VALUE), imm_reg);
	emit_write_ptr32_regptrTO_regFROM(g_out, CACHED_PTR(reg_pos_thumb(0, R0, ADDRESS)), R1);
	SET_NZC;	
	return 1;
}

//-----------------------------------------------------------------------------
//   ROR
//-----------------------------------------------------------------------------
static int OP_ROR_REG(const u32 i)
{
	u8 cf_change = 1;
	GET_CARRY(0);
	reg_pos_thumb(3, imm_reg, VALUE);
	emit_rors_reg(g_out, reg_pos_thumb(0, R1, VALUE), imm_reg);
	emit_write_ptr32_regptrTO_regFROM(g_out, reg_pos_thumb(0, R0, ADDRESS), R1);
	SET_NZC;

	return 1;
}

//-----------------------------------------------------------------------------
//   AND / ORR / EOR / BIC
//-----------------------------------------------------------------------------
static int OP_AND(const u32 i) { OP_LOGIC(emit_ands, 0); }
static int OP_ORR(const u32 i) { OP_LOGIC(emit_ors,  0); }
static int OP_EOR(const u32 i) { OP_LOGIC(emit_xors, 0); }
static int OP_BIC(const u32 i) { OP_LOGIC(emit_ands, 1); }

//-----------------------------------------------------------------------------
//   NEG
//-----------------------------------------------------------------------------
static int OP_NEG(const u32 i)
{
	if (_REG_NUM(i, 0) == _REG_NUM(i, 3))
		emit_negs_ptr32_regptr(g_out, reg_pos_thumb(0, R1, ADDRESS));
	else
	{
		reg_pos_thumb(3, tmp, VALUE);
		emit_negs(g_out, tmp, tmp);
		emit_write_ptr32_regptrTO_regFROM(g_out, reg_pos_thumb(0, R1, ADDRESS), tmp);
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
		reg_pos_thumb(3, tmp, VALUE);
		emit_write_ptr32_regptrTO_regFROM(g_out, reg_pos_thumb(0, R1, ADDRESS), tmp);
		emit_cmpimm(g_out, tmp,0);
		SET_NZ(1);
		
		return 1;
	}
	if (_REG_NUM(i, 0) == _REG_NUM(i, 3))
	{
		emit_adds_ptr32_regptrTO_immFROM(g_out, reg_pos_thumb(0, R1, ADDRESS), imm3);
	}
	else
	{
		reg_pos_thumb(3, tmp, VALUE);
		emit_addsimm(g_out, tmp, imm3, tmp);
		emit_write_ptr32_regptrTO_regFROM(g_out, reg_pos_thumb(0, R1, ADDRESS), tmp);
	}
	SET_NZCV(0);
	
	return 1;
}
static int OP_ADD_IMM8(const u32 i) 
{
	emit_adds_ptr32_regptrTO_immFROM(g_out, reg_pos_thumb(8, R1, ADDRESS), (i & 0xFF));
	SET_NZCV(0);

	return 1; 
}
static int OP_ADD_REG(const u32 i) 
{
	if (_REG_NUM(i, 0) == _REG_NUM(i, 3))
	{
		reg_pos_thumb(6, tmp, VALUE);
		emit_adds_ptr32_regptrTO_regFROM(g_out, reg_pos_thumb(0, R1, ADDRESS), tmp);
	}
	else
		if (_REG_NUM(i, 0) == _REG_NUM(i, 6))
		{
			reg_pos_thumb(3, tmp, VALUE);
			emit_adds_ptr32_regptrTO_regFROM(g_out, reg_pos_thumb(0, R1, ADDRESS), tmp);
		}
		else
		{
			reg_pos_thumb(3, tmp, VALUE);
			reg_pos_thumb(6, tmp2, VALUE);
			emit_adds(g_out, tmp, tmp2, tmp);
			emit_write_ptr32_regptrTO_regFROM(g_out, reg_pos_thumb(0, R1, ADDRESS), tmp);
		}
			
	SET_NZCV(0);	
	
	return 1; 
}
static int OP_ADD_SPE(const u32 i)
{
	u32 Rd = _REG_NUM(i, 0) | ((i>>4)&8);
	reg_ptr(Rd, tmp, VALUE);
	reg_pos_ptr(3, tmp2, VALUE);
	emit_add(g_out, tmp, tmp2, tmp);
	emit_write_ptr32_regptrTO_regFROM(g_out, reg_ptr(Rd, R1, ADDRESS), tmp);
	
	if(Rd==15)
		emit_write_ptr32_regptrTO_regFROM(g_out, cpu_ptr(next_instruction, R1, ADDRESS), tmp);
		
	return 1;
}

static int OP_ADD_2PC(const u32 i)
{
	u32 imm = ((i&0xFF)<<2);
	emit_write_ptr32_regptrTO_immFROM(g_out, reg_pos_thumb(8, R1, ADDRESS), (bb_r15 & 0xFFFFFFFC) + imm);
	
	return 1;
}

static int OP_ADD_2SP(const u32 i)
{
	u32 imm = ((i&0xFF)<<2);
	reg_ptr(13, tmp, VALUE);
	if (imm) emit_addimm(g_out, tmp, imm, tmp);
	emit_write_ptr32_regptrTO_regFROM(g_out, reg_pos_thumb(8, R1, ADDRESS), tmp);
	
	return 1;
}

//-----------------------------------------------------------------------------
//   SUB
//-----------------------------------------------------------------------------
static int OP_SUB_IMM3(const u32 i)
{
	u32 imm3 = (i >> 6) & 0x07;

	if (_REG_NUM(i, 0) == _REG_NUM(i, 3))
	{
		emit_subs_ptr32_regptrTO_immFROM(g_out, reg_pos_thumb(0, R1, ADDRESS), imm3);
	}
	else
	{
		reg_pos_thumb(3, tmp, VALUE);
		emit_subsimm(g_out, tmp, imm3, tmp);
		emit_write_ptr32_regptrTO_regFROM(g_out, reg_pos_thumb(0, R1, ADDRESS), tmp);
	}

	SET_NZCV(1);
		
	return 1;
}
static int OP_SUB_IMM8(const u32 i)
{
	emit_subs_ptr32_regptrTO_immFROM(g_out, reg_pos_thumb(8, R1, ADDRESS), (i & 0xFF));
	SET_NZCV(1);
	
	return 1; 
}
static int OP_SUB_REG(const u32 i)
{
	if (_REG_NUM(i, 0) == _REG_NUM(i, 3))
	{
		reg_pos_thumb(6, tmp, VALUE);
		reg_pos_thumb(0, tmp2, VALUE);
		emit_subs(g_out, tmp2, tmp, tmp2);
		emit_write_ptr32_regptrTO_regFROM(g_out, CACHED_PTR(reg_pos_thumb(0, R1, ADDRESS)), tmp2);
	}
	else
	{
		reg_pos_thumb(3, tmp, VALUE);
		reg_pos_thumb(6, tmp2, VALUE);
		emit_subs(g_out, tmp, tmp2, tmp);
		emit_write_ptr32_regptrTO_regFROM(g_out, reg_pos_thumb(0, R1, ADDRESS), tmp);
	}
	
	SET_NZCV(1);
	
	return 1; 
}

//-----------------------------------------------------------------------------
//   ADC
//-----------------------------------------------------------------------------
static int OP_ADC_REG(const u32 i)
{
	
	GET_CARRY(0);
	reg_pos_thumb(3, tmp, VALUE);
	reg_pos_thumb(0, tmp2, VALUE);
	emit_adcs(g_out, tmp2, tmp, tmp2);
	emit_write_ptr32_regptrTO_regFROM(g_out, CACHED_PTR(reg_pos_thumb(0, R1, ADDRESS)), tmp2);
	SET_NZCV(0);
	
	return 1;
}

//-----------------------------------------------------------------------------
//   SBC
//-----------------------------------------------------------------------------
static int OP_SBC_REG(const u32 i)
{
	GET_CARRY(1);
	reg_pos_thumb(3, tmp, VALUE);
	reg_pos_thumb(0, tmp2, VALUE);
	emit_sbcs(g_out, tmp2, tmp, tmp2);
	emit_write_ptr32_regptrTO_regFROM(g_out, CACHED_PTR(reg_pos_thumb(0, R1, ADDRESS)), tmp2);
	SET_NZCV(1);
	
	return 1;
}

//-----------------------------------------------------------------------------
//   MOV / MVN
//-----------------------------------------------------------------------------
static int OP_MOV_IMM8(const u32 i)
{
	emit_movimms(g_out, i & 0xFF, tmp);
	emit_write_ptr32_regptrTO_regFROM(g_out, reg_pos_thumb(8, R1, ADDRESS), tmp);
	SET_NZ(0);
	
	return 1;
}

static int OP_MOV_SPE(const u32 i)
{
	u32 Rd = _REG_NUM(i, 0) | ((i>>4)&8);
	reg_pos_ptr(3, tmp, VALUE);
	emit_write_ptr32_regptrTO_regFROM(g_out, reg_ptr(Rd, R1, ADDRESS), tmp);
	if(Rd == 15)
	{
		emit_write_ptr32_regptrTO_regFROM(g_out, cpu_ptr(next_instruction, R1, ADDRESS), tmp);
		bb_constant_cycles += 2;
	}
	
	return 1;
}

static int OP_MVN(const u32 i)
{
	reg_pos_thumb(3, tmp, VALUE);
	emit_mvns(g_out, tmp, tmp2);
	emit_write_ptr32_regptrTO_regFROM(g_out, reg_pos_thumb(0, R1, ADDRESS), tmp2);
	SET_NZ(0);
	
	return 1;
}

//-----------------------------------------------------------------------------
//   MUL
//-----------------------------------------------------------------------------
static int OP_MUL_REG(const u32 i) 
{
	reg_pos_thumb(3, tmp, VALUE);
	reg_pos_thumb(0, lhs, VALUE);
	
	emit_muls(g_out, lhs, tmp);
	emit_write_ptr32_regptrTO_regFROM(g_out, CACHED_PTR(reg_pos_thumb(0, R1, ADDRESS)), lhs);
	SET_NZ(0);
	if (PROCNUM == ARMCPU_ARM7)
		emit_write_ptr32(g_out, (uintptr_t)&bb_cycles, (uintptr_t)4);
	else
		MUL_Mxx_END(lhs, 0, 1);
	
	return 1;
}

//-----------------------------------------------------------------------------
//   CMP / CMN
//-----------------------------------------------------------------------------
static int OP_CMP_IMM8(const u32 i) 
{
	reg_pos_thumb(8, tmp, VALUE);
	emit_cmpimm(g_out, tmp, (i & 0xFF));
	SET_NZCV(1);
	
	return 1; 
}

static int OP_CMP(const u32 i) 
{
	reg_pos_thumb(3, tmp2, VALUE);
	reg_pos_thumb(0, tmp, VALUE);
	emit_cmp(g_out, tmp, tmp2);
	SET_NZCV(1);
	
	return 1; 
}

static int OP_CMP_SPE(const u32 i) 
{
	u32 Rn = (i&7) | ((i>>4)&8);
	reg_pos_ptr(3, tmp2, VALUE);
	reg_ptr(Rn, tmp, VALUE);
	emit_cmp(g_out, tmp, tmp2);
	SET_NZCV(1);
	
	return 1;
}

static int OP_CMN(const u32 i) 
{
	reg_pos_thumb(0, tmp, VALUE);
	reg_pos_thumb(3, tmp2, VALUE);
	emit_adds(g_out, tmp, tmp2, tmp); // MAY NEED TO SET FLAGS HERE
	SET_NZCV(0);
	
	return 1; 
}

//-----------------------------------------------------------------------------
//   TST
//-----------------------------------------------------------------------------
static int OP_TST(const u32 i)
{
	reg_pos_thumb(3, tmp, VALUE);
	reg_pos_thumb(0, tmp2, VALUE);
	emit_test(g_out, tmp, tmp2);
	SET_NZ(0);
	
	return 1;
}

//-----------------------------------------------------------------------------
//   STR / LDR / STRB / LDRB
//-----------------------------------------------------------------------------
#define STR_THUMB(mem_op, offset) \
	u32 adr_first = cpu->R[_REG_NUM(i, 3)];\
	reg_pos_thumb(3, R0, VALUE);\
	if ((offset) != -1) \
	{ \
		if ((offset) != 0) \
		{ \
			emit_addimm(g_out, R0, (uintptr_t)(offset), R0);\
			adr_first += (u32)(offset); \
		} \
	} \
	else \
	{ \
		reg_pos_thumb(6, R1, VALUE); \
		emit_add(g_out, R0, R1, R0);\
		adr_first += cpu->R[_REG_NUM(i, 6)]; \
	} \
	reg_pos_thumb(0, R1, VALUE); \
	emit_ptr(g_out, (uintptr_t)mem_op##_tab[PROCNUM][classify_adr(adr_first,1)], R3); \
	callR3(g_out); \
	emit_write_ptr32_reg(g_out, (uintptr_t)&bb_cycles, R0);\
	return 1;

#define LDR_THUMB(mem_op, offset) \
	u32 adr_first = cpu->R[_REG_NUM(i, 3)]; \
	reg_pos_thumb(3, R0, VALUE);\
	if ((offset) != -1) \
	{ \
		if ((offset) != 0) \
		{ \
			emit_addimm(g_out, R0, (uintptr_t)(offset), R0);\
			adr_first += (u32)(offset); \
		} \
	} \
	else \
	{ \
		reg_pos_thumb(6, R1, VALUE); \
		emit_add(g_out, R0, R1, R0);\
		adr_first += cpu->R[_REG_NUM(i, 6)]; \
	} \
	reg_pos_thumb(0, R1, ADDRESS); \
	emit_ptr(g_out, (uintptr_t)mem_op##_tab[PROCNUM][classify_adr(adr_first,0)], R3); \
	callR3(g_out); \
	emit_write_ptr32_reg(g_out, (uintptr_t)&bb_cycles, R0);\
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
	#define addr R0
	#define data R1
	
	u32 imm = ((i&0xFF)<<2);
	u32 adr_first = cpu->R[13] + imm;
	
	reg_ptr(13, addr, VALUE);
	if (imm) emit_addimm(g_out, addr, imm, addr);
	reg_pos_thumb(8, data, VALUE);
	
	// Add function
	emit_ptr(g_out, (uintptr_t)STR_tab[PROCNUM][classify_adr(adr_first,1)], R3);
	
	// Call function
	callR3(g_out);
	emit_write_ptr32_reg(g_out, (uintptr_t)&bb_cycles, R0);
	
	#undef addr
	#undef data
	
	return 1;
}

static int OP_LDR_SPREL(const u32 i)
{
	#define addr R0
	#define data R1
	
	u32 imm = ((i&0xFF)<<2);
	u32 adr_first = cpu->R[13] + imm;
	
	reg_ptr(13, addr, VALUE);
	if (imm) emit_addimm(g_out, addr, imm, addr);
	reg_pos_thumb(8, data, ADDRESS);
	
	// Add function
	emit_ptr(g_out, (uintptr_t)LDR_tab[PROCNUM][classify_adr(adr_first,0)], R3);
	
	// Call function
	callR3(g_out);
	
	emit_write_ptr32_reg(g_out, (uintptr_t)&bb_cycles, R0);
	
	#undef addr
	#undef data
	
	return 1;
}

static int OP_LDR_PCREL(const u32 i)
{
	#define addr R0
	#define data R1
	
	u32 imm = ((i&0xFF)<<2);
	u32 adr_first = (bb_r15 & 0xFFFFFFFC) + imm;
	
	emit_movimm(g_out, adr_first, addr);
	reg_pos_thumb(8, data, ADDRESS);

	// Add function
	emit_ptr(g_out, (uintptr_t)LDR_tab[PROCNUM][classify_adr(adr_first,0)], R3);
	
	// Call function
	callR3(g_out);
	
	emit_write_ptr32_reg(g_out, (uintptr_t)&bb_cycles, R0);
	
	#undef addr
	#undef data
	
	
	return 1;
}

//-----------------------------------------------------------------------------
//   STMIA / LDMIA
//-----------------------------------------------------------------------------
static int op_ldm_stm_thumb(u32 i, bool store)
{
	u32 bitmask = i & 0xFF;
	u32 pop = popregcount(bitmask);

	#define adr R4

	reg_pos_thumb(8, adr, VALUE);

	call_ldm_stm(adr, bitmask, store, 1);

	// ARM_REF:	THUMB: Causes base register write-back, and is not optional
	// ARM_REF:	If the base register <Rn> is specified in <registers>, the final value of <Rn> is the loaded value
	//			(not the written-back value).
	if (store) emit_add_ptr32_regptrTO_immFROM(g_out, reg_pos_thumb(8, R1, ADDRESS), 4*pop); //c.add(reg_pos_thumb(8), 4*pop);
		
	else
	{
		if (!BIT_N(i, _REG_NUM(i, 8)))
			emit_add_ptr32_regptrTO_immFROM(g_out, reg_pos_thumb(8, R1, ADDRESS), 4*pop); //c.add(reg_pos_thumb(8), 4*pop);
	}

	emit_MMU_aluMemCycles(store ? 2 : 3, &bb_cycles, pop);
	
	#undef adr
	
	return 1;
}

static int OP_LDMIA_THUMB(const u32 i) { return op_ldm_stm_thumb(i, 0); }
static int OP_STMIA_THUMB(const u32 i) { return op_ldm_stm_thumb(i, 1); }

//-----------------------------------------------------------------------------
//   Adjust SP
//-----------------------------------------------------------------------------
static int OP_ADJUST_P_SP(const u32 i) { emit_add_ptr32_regptrTO_immFROM(g_out, reg_ptr(13, R1, ADDRESS), ((i&0x7F)<<2)); /*c.add(reg_ptr(13), ((i&0x7F)<<2));*/ return 1; }
static int OP_ADJUST_M_SP(const u32 i) { emit_sub_ptr32_regptrTO_immFROM(g_out, reg_ptr(13, R1, ADDRESS), ((i&0x7F)<<2)); /*c.sub(reg_ptr(13), ((i&0x7F)<<2));*/ return 1; }

//-----------------------------------------------------------------------------
//   PUSH / POP
//-----------------------------------------------------------------------------
static int op_push_pop(u32 i, bool store, bool pc_lr)
{
	#define adr R4
	
	u32 bitmask = (i & 0xFF);
	bitmask |= pc_lr << (store ? 14 : 15);
	u32 pop = popregcount(bitmask);
	int dir = store ? -1 : 1;
	
	reg_ptr(13, adr, VALUE);	
	
	if(store) emit_subimm(g_out, adr, 4, adr);

	call_ldm_stm(adr, bitmask, store, dir);

	if(pc_lr && !store)
		op_bx_thumb(reg_ptr(15, R1, VALUE), 0, PROCNUM == ARMCPU_ARM9);
	emit_add_ptr32_regptrTO_immFROM(g_out, reg_ptr(13, R1, ADDRESS), 4*dir*pop);

	emit_MMU_aluMemCycles(store ? (pc_lr?4:3) : (pc_lr?5:2), &bb_cycles, pop);
	
	#undef adr
	
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
	unsigned int skip=genlabel();
	u32 dst = bb_r15 + ((u32)((s8)(i&0xFF))<<1);
	
	emit_write_ptr32_regptrTO_immFROM(g_out, cpu_ptr(instruct_adr, PTR_STORE_REG, ADDRESS), bb_next_instruction);
	
	emit_mov64(g_out, PTR_STORE_REG, 12);
	emit_branch((i>>8)&0xF, skip);
	emit_write_ptr32_regptrTO_immFROM(g_out,12/*cpu_ptr(instruct_adr, R1, ADDRESS)*/, dst);
	
	emit_inc_ptr_imm(g_out, (uintptr_t)&bb_total_cycles, 2);
	emit_label(g_out, (int)skip);

	return 1;
}

static int OP_B_UNCOND(const u32 i)
{
	u32 dst = bb_r15 + (SIGNEXTEND_11(i)<<1);
	emit_write_ptr32_regptrTO_immFROM(g_out,cpu_ptr(instruct_adr, R1, ADDRESS), dst);
	
	return 1;
}

static int OP_BLX(const u32 i)
{
	#define dst R4
	
	reg_ptr(14, dst, VALUE);
	emit_addimm(g_out, dst, (i&0x7FF) << 1, dst);
	emit_andimm(g_out, dst, 0xFFFFFFFC, dst);
	emit_write_ptr32_regptrTO_regFROM(g_out, cpu_ptr(instruct_adr, R1, ADDRESS), dst);
	emit_write_ptr32_regptrTO_immFROM(g_out,reg_ptr(14, R1, ADDRESS), bb_next_instruction | 1);
	cpu_ptr_byte(CPSR, 0, tmp, VALUE);
	emit_andimm(g_out, tmp, ~(1<<5), tmp);
	emit_writeBYTE_ptr32_regptrTO_regFROM(g_out, cpu_ptr_byte(CPSR, 0, R1, ADDRESS), tmp);
	
	#undef dst

	return 1;
}

static int OP_BL_10(const u32 i)
{
	u32 dst = bb_r15 + (SIGNEXTEND_11(i)<<12);
	emit_write_ptr32_regptrTO_immFROM(g_out,reg_ptr(14, R1, ADDRESS), dst);
	return 1;
}

static int OP_BL_11(const u32 i) 
{
	#define dst R4
	
	reg_ptr(14, dst, VALUE);
	emit_addimm(g_out, dst, (i&0x7FF) << 1, dst);
	emit_write_ptr32_regptrTO_regFROM(g_out, cpu_ptr(instruct_adr, R1, ADDRESS), dst);
	emit_write_ptr32_regptrTO_immFROM(g_out, reg_ptr(14, R1, ADDRESS), bb_next_instruction | 1);
	
	#undef dst
	
	return 1;
}

static int op_bx_thumb(/*Mem*/int srcreg, bool blx, bool test_thumb)
{
	#define thumb tmp2_reg
	#define dst tmp_reg
	#define mask R8
	
	emit_mov(g_out, srcreg, dst);
	emit_mov(g_out, dst, thumb);
	emit_andimm(g_out, thumb, 1, thumb);
	
	if (blx) emit_write_ptr32_regptrTO_immFROM(g_out, reg_ptr(14, R1, ADDRESS), bb_next_instruction | 1);
	if(test_thumb)
	{
		emit_mov(g_out, thumb, R9);
		emit_lea_ptr_reg(g_out, 0xFFFFFFFC, R9, 2, mask);
		emit_and(g_out, dst, mask, dst);
	}
	else
	{
		emit_andimm(g_out, dst, 0xFFFFFFFE, dst);
	}
	
	emit_write_ptr32_regptrTO_regFROM(g_out, cpu_ptr(instruct_adr, R1, ADDRESS), dst);
	
	cpu_ptr_byte(CPSR, 0, tmp, VALUE);
	emit_andimm(g_out, tmp, ~(1<< 5), tmp);
	emit_lsl(g_out, thumb, 5);
	emit_or(g_out, tmp, thumb, tmp);	
	emit_writeBYTE_ptr32_regptrTO_regFROM(g_out, cpu_ptr_byte(CPSR, 0, R1, ADDRESS), tmp);	
	
	#undef dst
	#undef thumb
	#undef mask
	
	return 1;
}

static int op_bx_thumbR15()
{
	const u32 r15 = (bb_r15 & 0xFFFFFFFC);
	emit_write_ptr32_regptrTO_immFROM(g_out, cpu_ptr(instruct_adr, R1, ADDRESS), r15);
	emit_write_ptr32_regptrTO_immFROM(g_out, reg_ptr(15, R1, ADDRESS), r15);
	cpu_ptr(CPSR, R7, VALUE);
	emit_movimm(g_out, (u32)~(1<< 5), R8);
	emit_and(g_out, R7,R8,R7);
	emit_write_ptr32_regptrTO_regFROM(g_out, cpu_ptr(CPSR, R1, ADDRESS), R7);
	
	return 1;
}

static int OP_BX_THUMB(const u32 i) { if (REG_POS(i, 3) == 15) return op_bx_thumbR15(); return op_bx_thumb(reg_pos_ptr(3, R1, VALUE), 0, 0); }
static int OP_BLX_THUMB(const u32 i) { return op_bx_thumb(reg_pos_ptr(3, R1, VALUE), 1, 1); }

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

//#if pth // pth

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
	u32 xt = instr_attributes(opcode);
	
	if(bb_thumb)
	{
		// merge OP_BL_10+OP_BL_11
		if (xt & MERGE_NEXT) return false;
		return (xt & BRANCH_ALWAYS)
		    || ((xt & BRANCH_POS0) && ((opcode&7) | ((opcode>>4)&8)) == 15)
			|| (xt & BRANCH_SWI)
		    || (xt & JIT_BYPASS);
	}
	else
		return (xt & BRANCH_ALWAYS)
		    || ((xt & BRANCH_POS12) && REG_POS(opcode,12) == 15)
		    || ((xt & BRANCH_LDM) && BIT15(opcode))
			|| (xt & BRANCH_SWI)
		    || (xt & JIT_BYPASS);
}

static bool instr_uses_r15(u32 opcode)
{
	u32 xt = instr_attributes(opcode);
	if(bb_thumb)
		return ((xt & SRCREG_POS0) && ((opcode&7) | ((opcode>>4)&8)) == 15)
			|| ((xt & SRCREG_POS3) && REG_POS(opcode,3) == 15)
			|| (xt & JIT_BYPASS);
	else
		return ((xt & SRCREG_POS0) && REG_POS(opcode,0) == 15)
		    || ((xt & SRCREG_POS8) && REG_POS(opcode,8) == 15)
		    || ((xt & SRCREG_POS12) && REG_POS(opcode,12) == 15)
		    || ((xt & SRCREG_POS16) && REG_POS(opcode,16) == 15)
		    || ((xt & SRCREG_STM) && BIT15(opcode))
		    || (xt & JIT_BYPASS);
}

static bool instr_is_conditional(u32 opcode)
{
	if(bb_thumb) return false;
	
	return !(CONDITION(opcode) == 0xE
	         || (CONDITION(opcode) == 0xF && CODE(opcode) == 5));
}

static int instr_cycles(u32 opcode)
{
	u32 xt = instr_attributes(opcode);
	u32 ct = (xt & INSTR_CYCLES_MASK);
	if(ct == INSTR_CYCLES_VARIABLE)
	{
		if ((xt & BRANCH_SWI) && !cpu->swi_tab)
			return 3;
		
		return 0;
	}
	if(instr_is_branch(opcode) && !(instr_attributes(opcode) & (BRANCH_ALWAYS|BRANCH_LDM)))
		ct += 2;
	return ct;
}

static bool instr_does_prefetch(u32 opcode)
{
	u32 xt = instr_attributes(opcode);
	if(bb_thumb)
		return thumb_instruction_compilers[opcode>>6]
			   && (xt & BRANCH_ALWAYS);
	else
		return instr_is_branch(opcode) && arm_instruction_compilers[INSTRUCTION_INDEX(opcode)]
			   && ((xt & BRANCH_ALWAYS) || (xt & BRANCH_LDM));
}

static const char *blah = "";
static const char *disassemble(u32 opcode)
{
	return blah;
	static char str[600];
	if(bb_thumb) {
		int i = opcode>>6;
		if (i >= 0 && i <= 4095) { strcpy(str, thumb_instruction_names[opcode>>6]); return str; }
		else { strcpy(str, "bb Invalid"); return str; }
	}
	
	int idx = INSTRUCTION_INDEX(opcode);
	if (idx >= 0 && idx <= 4095) strcpy(str, arm_instruction_names[INSTRUCTION_INDEX(opcode)]);
	else { strcpy(str, "Invalid"); return str; }
	static const char *conds[16] = {"EQ","NE","CS","CC","MI","PL","VS","VC","HI","LS","GE","LT","GT","LE","AL","NV"};
	if(instr_is_conditional(opcode))
	{
		strcat(str, ".");
		if (CONDITION(opcode) >= 0 && CONDITION(opcode) <= 15) strcat(str, conds[CONDITION(opcode)]);
	}
	return str;
}

static void sync_r15(u32 opcode, bool is_last, bool force)
{
	if(instr_does_prefetch(opcode))
	{
		if(force)
		{
			emit_write_ptr32_regptrTO_immFROM(g_out, cpu_ptr(instruct_adr, R1, ADDRESS), bb_next_instruction);
		}
	}
	else
	{
		if(force || (instr_attributes(opcode) & JIT_BYPASS) || (instr_attributes(opcode) & BRANCH_SWI) || (is_last && !instr_is_branch(opcode)))
		{
			emit_write_ptr32_regptrTO_immFROM(g_out, cpu_ptr(next_instruction, R1, ADDRESS), bb_next_instruction);	
		}
		if(instr_uses_r15(opcode))
		{
			emit_write_ptr32_regptrTO_immFROM(g_out, reg_ptr(15, R1, ADDRESS), bb_r15);
		}
		if(instr_attributes(opcode) & JIT_BYPASS)
		{
			emit_write_ptr32_regptrTO_immFROM(g_out, cpu_ptr(instruct_adr, R1, ADDRESS), bb_adr);
		}
	}
}

static void emit_branch(int cond, int /*Label*/ to)
{
	static const u8 cond_bit[] = {0x40, 0x40, 0x20, 0x20, 0x80, 0x80, 0x10, 0x10};
	if(cond < 8)
	{
		emit_testimm(g_out, flags_ptr(R1, VALUE), cond_bit[cond]);
		(cond & 1)?emit_branch_label(g_out, to, 0x1):emit_branch_label(g_out, to, 0x0);
	}
	else
	{
		emit_movimm(g_out, 0, x);
		flags_ptr(x, VALUE);
		
		emit_andimm(g_out, x, 0xF0, x);
		
		emit_ptr(g_out, (uintptr_t)(arm_cond_table + cond), tmp);
		emit_add64(g_out, tmp, x, tmp);
		
		emit_movimm(g_out, 0, x);
		
		emit_ldrb(g_out, tmp, x);
		
		emit_testimm(g_out, x, 1);
		emit_branch_label(g_out, to, 0x0);
		
	}
}

static void emit_armop_call(u32 opcode)
{
	ArmOpCompiler fc = bb_thumb?	thumb_instruction_compilers[opcode>>6]:
									arm_instruction_compilers[INSTRUCTION_INDEX(opcode)];
		
	if (fc && fc(opcode)) {
		return;
	}
	
	OpFunc f = bb_thumb ? thumb_instructions_set[PROCNUM][opcode>>6]
	                     : arm_instructions_set[PROCNUM][INSTRUCTION_INDEX(opcode)];
						 	
	// Setup args
	emit_movimm(g_out, opcode, R0);
	// Add function
	emit_ptr(g_out, (uintptr_t)f, R3);
	
	// Call function
	callR3(g_out);
	
	emit_write_ptr32_reg(g_out, (uintptr_t)&bb_cycles, R0);	
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

ArmOpCompiled gf = NULL;

template<int PROCNUM>
static u32 compile_basicblock()
{
#if LOG_JIT
	bool has_variable_cycles = FALSE;
#endif
	
	u32 interpreted_cycles = 0;
	u32 start_adr = cpu->instruct_adr;
	u32 opcode = 0;
	
	bb_thumb = cpu->CPSR.bits.T;
	bb_opcodesize = bb_thumb ? 2 : 4;

	if (!JIT_MAPPED(start_adr & 0x0FFFFFFF, PROCNUM))
	{
		printf("JIT: use unmapped memory address %08X\n", start_adr);
		return 1;
	}

	jit_write();

	if (g_out) {
		releaseBytes(g_out);
	}
	g_out = allocBytes();
	
	emit_startfunc(g_out);
	
	JIT_COMMENT("CPU ptr");
	
	emit_write_ptr(g_out, (uintptr_t)&bb_cpu, (uintptr_t)&ARMPROC);

	JIT_COMMENT("reset bb_total_cycles");

	emit_write_ptr32(g_out, (uintptr_t)&bb_total_cycles, (uintptr_t)0);

#if (PROFILER_JIT_LEVEL > 0)
	JIT_COMMENT("Profiler ptr");
	emit_write_ptr(g_out, (uintptr_t)&bb_profiler, (uintptr_t)&profiler_counter[PROCNUM]);
#endif

	bb_constant_cycles = 0;
	for(u32 i=0, bEndBlock = 0; bEndBlock == 0; i++)
	{
		bb_adr = start_adr + (i * bb_opcodesize);
		if(bb_thumb)
			opcode = _MMU_read16<PROCNUM, MMU_AT_CODE>(bb_adr);
		else
			opcode = _MMU_read32<PROCNUM, MMU_AT_CODE>(bb_adr);
#if LOG_JIT
		char dasmbuf[1024] = {0};
		if(bb_thumb)
			des_thumb_instructions_set[opcode>>6](bb_adr, opcode, dasmbuf);
		else
			des_arm_instructions_set[INSTRUCTION_INDEX(opcode)](bb_adr, opcode, dasmbuf);
		fprintf(stderr, "%08X\t%s\t\t; %s \n", bb_adr, dasmbuf, disassemble(opcode));
#endif

		u32 cycles = instr_cycles(opcode);

		bEndBlock = instr_is_branch(opcode) || (i >= (CommonSettings.jit_max_block_size - 1));
		
#if LOG_JIT
		if (instr_is_conditional(opcode) && (cycles > 1) || (cycles == 0))
			has_variable_cycles = TRUE;
#endif
		bb_constant_cycles += instr_is_conditional(opcode) ? 1 : cycles;

#if (PROFILER_JIT_LEVEL > 0)
		JIT_COMMENT("*** profiler - counter");
#endif
		if (instr_is_conditional(opcode))
		{
			// 25% of conditional instructions are immediately followed by
			// another with the same condition, but merging them into a
			// single branch has negligible effect on speed.
			if(bEndBlock) sync_r15(opcode, 1, 1);
			int skip;
			skip=genlabel();
			
			emit_branch(CONDITION(opcode), skip);

			if(!bEndBlock) {
				sync_r15(opcode, 0, 0);
			}
			
			emit_armop_call(opcode);
			
			if(cycles == 0)
			{
				JIT_COMMENT("variable cycles");
				emit_inc_ptr(g_out, (uintptr_t)&bb_total_cycles, (uintptr_t)&bb_cycles);
				emit_inc_ptr_imm(g_out, (uintptr_t)&bb_total_cycles, -1);
			}
			else
				if (cycles > 1)
				{
					JIT_COMMENT("cycles (%d)", cycles);
					emit_inc_ptr_imm(g_out, (uintptr_t)&bb_total_cycles, -1);
				}
			emit_label(g_out, (int)skip);
		}
		else
		{
			sync_r15(opcode, bEndBlock, 0);
			emit_armop_call(opcode);
			
			if(cycles == 0)
			{
				JIT_COMMENT("variable cycles");
				emit_inc_ptr(g_out, (uintptr_t)&bb_total_cycles, (uintptr_t)&bb_cycles);
			}
		}

		interpreted_cycles += op_decode[PROCNUM][bb_thumb]();
	}

	if(!instr_does_prefetch(opcode))
	{
		emit_var_copy32_reg(g_out, cpu_ptr(next_instruction, R1, VALUE), cpu_ptr(instruct_adr, R2, ADDRESS));
	}

	JIT_COMMENT("total cycles (block)");

	if (bb_constant_cycles > 0)
		emit_inc_ptr_imm(g_out, (uintptr_t)&bb_total_cycles, bb_constant_cycles);

#if LOG_JIT
	fprintf(stderr, "cycles %d%s\n", bb_constant_cycles, has_variable_cycles ? " + variable" : "");
#endif
	
	emit_read_ptr32_reg(g_out, (uintptr_t)&bb_total_cycles, R0);
	
	emit_endfunc(g_out);
	
	ArmOpCompiled f;
	f = createFunc(g_out);

	jit_exec();
		
	if(! f)
	{
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

void arm_jit_reset(bool enable, bool suppress_msg)
{
#if LOG_JIT
	freopen("desmume_jit.log", "w", stderr);
#endif

	if (!suppress_msg)
		printf("CPU mode: %s\n", enable?"JIT":"Interpreter");
	saveBlockSizeJIT = CommonSettings.jit_max_block_size;

	if (enable)
	{
		printf("JIT: max block size %d instruction(s)\n", CommonSettings.jit_max_block_size);

#ifdef MAPPED_JIT_FUNCS

		//these pointers are allocated by asmjit and need freeing
#define JITFREE(x)  for(int iii=0;iii<ARRAY_SIZE(x);iii++) if(x[iii]) x[iii] = NULL;
			JITFREE(JIT.MAIN_MEM);
			JITFREE(JIT.SWIRAM);
			JITFREE(JIT.ARM9_ITCM);
			JITFREE(JIT.ARM9_LCDC);
			JITFREE(JIT.ARM9_BIOS);
			JITFREE(JIT.ARM7_BIOS);
			JITFREE(JIT.ARM7_ERAM);
			JITFREE(JIT.ARM7_WIRAM);
			JITFREE(JIT.ARM7_WRAM);
		#undef JITFREE
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
		freeFuncs();
	}

#if (PROFILER_JIT_LEVEL > 0)
	reconstruct(&profiler_counter[0]);
	reconstruct(&profiler_counter[1]);
#if (PROFILER_JIT_LEVEL > 1)
	for (u8 t = 0; t < 2; t++)
	{
		for (u32 i = 0; i < (1<<26); i++)
			memset(&profiler_entry[t][i], 0, sizeof(PROFILER_ENTRY));
	}
#endif
#endif
}

#if (PROFILER_JIT_LEVEL > 0)
static int pcmp(PROFILER_COUNTER_INFO *info1, PROFILER_COUNTER_INFO *info2)
{
	return (uintptr_t)(info2->count - info1->count);
}

#if (PROFILER_JIT_LEVEL > 1)
static int pcmp_entry(PROFILER_ENTRY *info1, PROFILER_ENTRY *info2)
{
	return (uintptr_t)(info1->cycles - info2->cycles);
}
#endif
#endif

void arm_jit_close()
{
#if (PROFILER_JIT_LEVEL > 0)
	printf("Generating profile report...");

	for (u8 proc = 0; proc < 2; proc++)
	{
		extern GameInfo gameInfo;
		u16 last[2] = {0};
		PROFILER_COUNTER_INFO *arm_info = NULL;
		PROFILER_COUNTER_INFO *thumb_info = NULL;
		
		arm_info = new PROFILER_COUNTER_INFO[4096];
		thumb_info = new PROFILER_COUNTER_INFO[1024];
		memset(arm_info, 0, sizeof(PROFILER_COUNTER_INFO) * 4096);
		memset(thumb_info, 0, sizeof(PROFILER_COUNTER_INFO) * 1024);

		// ARM
		last[0] = 0;
		for (u16 i=0; i < 4096; i++)
		{
			u16 t = 0;
			if (profiler_counter[proc].arm_count[i] == 0) continue;

			for (t = 0; t < last[0]; t++)
			{
				if (strcmp(arm_instruction_names[i], arm_info[t].name) == 0)
				{
					arm_info[t].count += profiler_counter[proc].arm_count[i];
					break;
				}
			}
			if (t == last[0])
			{
				strcpy(arm_info[last[0]++].name, arm_instruction_names[i]);
				arm_info[t].count = profiler_counter[proc].arm_count[i];
			}
		}

		// THUMB
		last[1] = 0;
		for (u16 i=0; i < 1024; i++)
		{
			u16 t = 0;
			if (profiler_counter[proc].thumb_count[i] == 0) continue;

			for (t = 0; t < last[1]; t++)
			{
				if (strcmp(thumb_instruction_names[i], thumb_info[t].name) == 0)
				{
					thumb_info[t].count += profiler_counter[proc].thumb_count[i];
					break;
				}
			}
			if (t == last[1])
			{
				strcpy(thumb_info[last[1]++].name, thumb_instruction_names[i]);
				thumb_info[t].count = profiler_counter[proc].thumb_count[i];
			}
		}

		std::qsort(arm_info, last[0], sizeof(PROFILER_COUNTER_INFO), (int (*)(const void *, const void *))pcmp);
		std::qsort(thumb_info, last[1], sizeof(PROFILER_COUNTER_INFO), (int (*)(const void *, const void *))pcmp);

		char buf[MAX_PATH] = {0};
		sprintf(buf, "desmume_jit%c_counter.profiler", proc==0?'9':'7');
		FILE *fp = fopen(buf, "w");
		if (fp)
		{
			if (!gameInfo.isHomebrew())
			{
				fprintf(fp, "Name:   %s\n", gameInfo.ROMname);
				fprintf(fp, "Serial: %s\n", gameInfo.ROMserial);
			}
			else
				fprintf(fp, "Homebrew\n");
			fprintf(fp, "CPU: ARM%c\n\n", proc==0?'9':'7');

			if (last[0])
			{
				fprintf(fp, "========================================== ARM ==========================================\n");
				for (int i=0; i < last[0]; i++)
					fprintf(fp, "%30s: %20ld\n", arm_info[i].name, arm_info[i].count);
				fprintf(fp, "\n");
			}
			
			if (last[1])
			{
				fprintf(fp, "========================================== THUMB ==========================================\n");
				for (int i=0; i < last[1]; i++)
					fprintf(fp, "%30s: %20ld\n", thumb_info[i].name, thumb_info[i].count);
				fprintf(fp, "\n");
			}

			fclose(fp);
		}

		delete [] arm_info; arm_info = NULL;
		delete [] thumb_info; thumb_info = NULL;

#if (PROFILER_JIT_LEVEL > 1)
		sprintf(buf, "desmume_jit%c_entry.profiler", proc==0?'9':'7');
		fp = fopen(buf, "w");
		if (fp)
		{
			u32 count = 0;
			PROFILER_ENTRY *tmp = NULL;

			fprintf(fp, "Entrypoints (cycles):\n");
			tmp = new PROFILER_ENTRY[1<<26];
			memset(tmp, 0, sizeof(PROFILER_ENTRY) * (1<<26));
			for (u32 i = 0; i < (1<<26); i++)
			{
				if (profiler_entry[proc][i].cycles == 0) continue;
				memcpy(&tmp[count++], &profiler_entry[proc][i], sizeof(PROFILER_ENTRY));
			}
			std::qsort(tmp, count, sizeof(PROFILER_ENTRY), (int (*)(const void *, const void *))pcmp_entry);
			if (!gameInfo.isHomebrew())
			{
				fprintf(fp, "Name:   %s\n", gameInfo.ROMname);
				fprintf(fp, "Serial: %s\n", gameInfo.ROMserial);
			}
			else
				fprintf(fp, "Homebrew\n");
			fprintf(fp, "CPU: ARM%c\n\n", proc==0?'9':'7');

			while ((count--) > 0)
				fprintf(fp, "%08X: %20ld\n", tmp[count].addr, tmp[count].cycles);

			delete [] tmp; tmp = NULL;

			fclose(fp);
		}
#endif
	}
	printf(" done.\n");
#endif
}
#endif // HAVE_JIT
