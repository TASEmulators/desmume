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

#include <unistd.h>
#include <stddef.h>
#include <stdint.h>

#include "arm_gen.h"
#include "reg_manager.h"
using namespace arm_gen;

#include "instructions.h"
#include "instruction_attributes.h"
#include "MMU.h"
#include "MMU_timing.h"
#include "arm_jit.h"
#include "bios.h"
#include "armcpu.h"

u32 saveBlockSizeJIT = 0;

#ifdef MAPPED_JIT_FUNCS
CACHE_ALIGN JIT_struct JIT;

uintptr_t *JIT_struct::JIT_MEM[2][0x4000] = {{0}};

static uintptr_t *JIT_MEM[2][32] = {
   //arm9
   {
      /* 0X*/  DUP2(JIT.ARM9_ITCM),
      /* 1X*/  DUP2(JIT.ARM9_ITCM), // mirror
      /* 2X*/  DUP2(JIT.MAIN_MEM),
      /* 3X*/  DUP2(JIT.SWIRAM),
      /* 4X*/  DUP2(NULL),
      /* 5X*/  DUP2(NULL),
      /* 6X*/      NULL,
                JIT.ARM9_LCDC,   // Plain ARM9-CPU Access (LCDC mode) (max 656KB)
      /* 7X*/  DUP2(NULL),
      /* 8X*/  DUP2(NULL),
      /* 9X*/  DUP2(NULL),
      /* AX*/  DUP2(NULL),
      /* BX*/  DUP2(NULL),
      /* CX*/  DUP2(NULL),
      /* DX*/  DUP2(NULL),
      /* EX*/  DUP2(NULL),
      /* FX*/  DUP2(JIT.ARM9_BIOS)
   },
   //arm7
   {
      /* 0X*/  DUP2(JIT.ARM7_BIOS),
      /* 1X*/  DUP2(NULL),
      /* 2X*/  DUP2(JIT.MAIN_MEM),
      /* 3X*/       JIT.SWIRAM,
                   JIT.ARM7_ERAM,
      /* 4X*/       NULL,
                   JIT.ARM7_WIRAM,
      /* 5X*/  DUP2(NULL),
      /* 6X*/      JIT.ARM7_WRAM,      // VRAM allocated as Work RAM to ARM7 (max. 256K)
                NULL,
      /* 7X*/  DUP2(NULL),
      /* 8X*/  DUP2(NULL),
      /* 9X*/  DUP2(NULL),
      /* AX*/  DUP2(NULL),
      /* BX*/  DUP2(NULL),
      /* CX*/  DUP2(NULL),
      /* DX*/  DUP2(NULL),
      /* EX*/  DUP2(NULL),
      /* FX*/  DUP2(NULL)
      }
};

static u32 JIT_MASK[2][32] = {
   //arm9
   {
      /* 0X*/  DUP2(0x00007FFF),
      /* 1X*/  DUP2(0x00007FFF),
      /* 2X*/  DUP2(0x003FFFFF), // FIXME _MMU_MAIN_MEM_MASK
      /* 3X*/  DUP2(0x00007FFF),
      /* 4X*/  DUP2(0x00000000),
      /* 5X*/  DUP2(0x00000000),
      /* 6X*/      0x00000000,
                0x000FFFFF,
      /* 7X*/  DUP2(0x00000000),
      /* 8X*/  DUP2(0x00000000),
      /* 9X*/  DUP2(0x00000000),
      /* AX*/  DUP2(0x00000000),
      /* BX*/  DUP2(0x00000000),
      /* CX*/  DUP2(0x00000000),
      /* DX*/  DUP2(0x00000000),
      /* EX*/  DUP2(0x00000000),
      /* FX*/  DUP2(0x00007FFF)
   },
   //arm7
   {
      /* 0X*/  DUP2(0x00003FFF),
      /* 1X*/  DUP2(0x00000000),
      /* 2X*/  DUP2(0x003FFFFF),
      /* 3X*/       0x00007FFF,
                   0x0000FFFF,
      /* 4X*/       0x00000000,
                   0x0000FFFF,
      /* 5X*/  DUP2(0x00000000),
      /* 6X*/      0x0003FFFF,
                0x00000000,
      /* 7X*/  DUP2(0x00000000),
      /* 8X*/  DUP2(0x00000000),
      /* 9X*/  DUP2(0x00000000),
      /* AX*/  DUP2(0x00000000),
      /* BX*/  DUP2(0x00000000),
      /* CX*/  DUP2(0x00000000),
      /* DX*/  DUP2(0x00000000),
      /* EX*/  DUP2(0x00000000),
      /* FX*/  DUP2(0x00000000)
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

template<int PROCNUM, int thumb>
static u32 FASTCALL OP_DECODE()
{
   u32 cycles;
   u32 adr = ARMPROC.instruct_adr;
   if(thumb)
   {
      ARMPROC.next_instruction = adr + 2;
      ARMPROC.R[15] = adr + 4;
      u32 opcode = _MMU_read16<PROCNUM, MMU_AT_CODE>(adr);
      //_armlog(PROCNUM, adr, opcode);
      cycles = thumb_instructions_set[PROCNUM][opcode>>6](opcode);
   }
   else
   {
      ARMPROC.next_instruction = adr + 4;
      ARMPROC.R[15] = adr + 8;
      u32 opcode = _MMU_read32<PROCNUM, MMU_AT_CODE>(adr);
      //_armlog(PROCNUM, adr, opcode);
      if(CONDITION(opcode) == 0xE || TEST_COND(CONDITION(opcode), CODE(opcode), ARMPROC.CPSR))
         cycles = arm_instructions_set[PROCNUM][INSTRUCTION_INDEX(opcode)](opcode);
      else
         cycles = 1;
   }
   ARMPROC.instruct_adr = ARMPROC.next_instruction;
   return cycles;
}

static const ArmOpCompiled op_decode[2][2] = { OP_DECODE<0,0>, OP_DECODE<0,1>, OP_DECODE<1,0>, OP_DECODE<1,1> };


enum OP_RESULT { OPR_CONTINUE, OPR_INTERPRET, OPR_BRANCHED, OPR_RESULT_SIZE = 2147483647 };
#define OPR_RESULT(result, cycles) (OP_RESULT)((result) | ((cycles) << 16));
#define OPR_RESULT_CYCLES(result) ((result >> 16))
#define OPR_RESULT_ACTION(result) ((result & 0xFF))

typedef OP_RESULT (*ArmOpCompiler)(uint32_t pc, uint32_t opcode);

static const uint32_t INSTRUCTION_COUNT = 0xC0000;
static code_pool* block;
static register_manager* regman;
static u8 recompile_counts[(1<<26)/16];

const reg_t RCPU = 12;
const reg_t RCYC = 4;
static uint32_t block_procnum;

///////
// HELPERS
///////
static bool emu_status_dirty;

static bool bit(uint32_t value, uint32_t bit)
{
   return value & (1 << bit);
}

static uint32_t bit(uint32_t value, uint32_t first, uint32_t count)
{
   return (value >> first) & ((1 << count) - 1);
}

static uint32_t bit_write(uint32_t value, uint32_t first, uint32_t count, uint32_t insert)
{
   uint32_t result = value & ~(((1 << count) - 1) << first);
   return result | (insert << first);
}

static void load_status(reg_t scratch)
{
   block->ldr(scratch, RCPU, mem2::imm(offsetof(armcpu_t, CPSR)));
   block->set_status(scratch);
}

static void write_status(reg_t scratch)
{
   if (emu_status_dirty)
   {
      block->get_status(scratch);
      block->mov(scratch, alu2::reg_shift_imm(scratch, LSR, 24));
      block->strb(scratch, RCPU, mem2::imm(offsetof(armcpu_t, CPSR) + 3));

      emu_status_dirty = false;
   }
}

static void mark_status_dirty()
{
   emu_status_dirty = true;
}

static void call(reg_t reg)
{
   write_status(3);
   block->blx(reg);

   const unsigned PROCNUM = block_procnum;
   block->load_constant(RCPU, (uint32_t)&ARMPROC);

   load_status(3);
}

static void change_mode(bool thumb)
{
   block->ldr(0, RCPU, mem2::imm(offsetof(armcpu_t, CPSR)));

   if (!thumb)
   {
      block->bic(0, alu2::imm(1 << 5));
   }
   else
   {
      block->orr(0, alu2::imm(1 << 5));
   }
   block->str(0, RCPU, mem2::imm(offsetof(armcpu_t, CPSR)));
}

static void change_mode_reg(reg_t reg, reg_t scratch, reg_t scratch2)
{
   block->and_(scratch2, reg, alu2::imm(1));

   block->ldr(scratch, RCPU, mem2::imm(offsetof(armcpu_t, CPSR)));
   block->bic(scratch, alu2::imm(scratch2 << 5));
   block->orr(scratch, alu2::reg_shift_imm(scratch2, LSL, 5));
   block->str(scratch, RCPU, mem2::imm(offsetof(armcpu_t, CPSR)));
}

template <int PROCNUM>
static void arm_jit_prefetch(uint32_t pc, uint32_t opcode, bool thumb)
{
   const uint32_t imask = thumb ? 0xFFFFFFFE : 0xFFFFFFFC;
   const uint32_t isize = thumb ? 2 : 4;

   block->load_constant(0, pc & imask);
   block->str(0, RCPU, mem2::imm(offsetof(armcpu_t, instruct_adr)));

   block->add(0, alu2::imm(isize));
   block->str(0, RCPU, mem2::imm(offsetof(armcpu_t, next_instruction)));

   block->add(0, alu2::imm(isize));
   block->str(0, RCPU, mem2::imm(offsetof(armcpu_t, R) + 4 * 15));

   block->load_constant(0, opcode);
   block->str(0, RCPU, mem2::imm(offsetof(armcpu_t, instruction)));
}

/////////
/// ARM
/////////
static OP_RESULT ARM_OP_PATCH_DELEGATE(uint32_t pc, uint32_t opcode, int AT16, int AT12, int AT8, int AT0, bool S, uint32_t CYC)
{
   const reg_t at16 = bit(opcode, 16, 4);
   const reg_t at12 = bit(opcode, 12, 4);
   const reg_t at8  = bit(opcode, 8, 4);
   const reg_t at0  = bit(opcode, 0, 4);

   if ((AT16 && (at16 == 0xF)) || (AT12 && (at12 == 0xF)) || (AT8 && (at8 == 0xF)) || (AT0 && (at0 == 0xF)))
      return OPR_INTERPRET;

   const uint32_t weak_tag = (bit(opcode, 28, 4) == AL) ? 0x10 : 0;

   int32_t reg_list[4];
   reg_list[0] = (AT16) ? (int32_t)(at16 | ((AT16 == 2) ? weak_tag : 0)) : -1;
   reg_list[1] = (AT12) ? (int32_t)(at12 | ((AT12 == 2) ? weak_tag : 0)) : -1;
   reg_list[2] = (AT8 ) ? (int32_t)(at8  | ((AT8  == 2) ? weak_tag : 0)) : -1;
   reg_list[3] = (AT0 ) ? (int32_t)(at0  | ((AT0  == 2) ? weak_tag : 0)) : -1;
   regman->get(4, reg_list);

   opcode = AT16 ? bit_write(opcode, 16, 4, reg_list[0]) : opcode;
   opcode = AT12 ? bit_write(opcode, 12, 4, reg_list[1]) : opcode;
   opcode = AT8  ? bit_write(opcode,  8, 4, reg_list[2]) : opcode;
   opcode = AT0  ? bit_write(opcode,  0, 4, reg_list[3]) : opcode;

   block->insert_raw_instruction(opcode);
   if (S) mark_status_dirty();

   if (AT16 & 2) regman->mark_dirty(reg_list[0]);
   if (AT12 & 2) regman->mark_dirty(reg_list[1]);
   if (AT8  & 2) regman->mark_dirty(reg_list[2]);
   if (AT0  & 2) regman->mark_dirty(reg_list[3]);

   return OPR_RESULT(OPR_CONTINUE, CYC);
}

template <int AT16, int AT12, int AT8, int AT0, bool S, uint32_t CYC>
static OP_RESULT ARM_OP_PATCH(uint32_t pc, uint32_t opcode)
{
   return ARM_OP_PATCH_DELEGATE(pc, opcode, AT16, AT12, AT8, AT0, S, CYC);
}

#define ARM_ALU_OP_DEF(T, D, N, S) \
   static const ArmOpCompiler ARM_OP_##T##_LSL_IMM = ARM_OP_PATCH<N, D, 0, 1, S, 1>; \
   static const ArmOpCompiler ARM_OP_##T##_LSL_REG = ARM_OP_PATCH<N, D, 1, 1, S, 2>; \
   static const ArmOpCompiler ARM_OP_##T##_LSR_IMM = ARM_OP_PATCH<N, D, 0, 1, S, 1>; \
   static const ArmOpCompiler ARM_OP_##T##_LSR_REG = ARM_OP_PATCH<N, D, 1, 1, S, 2>; \
   static const ArmOpCompiler ARM_OP_##T##_ASR_IMM = ARM_OP_PATCH<N, D, 0, 1, S, 1>; \
   static const ArmOpCompiler ARM_OP_##T##_ASR_REG = ARM_OP_PATCH<N, D, 1, 1, S, 2>; \
   static const ArmOpCompiler ARM_OP_##T##_ROR_IMM = ARM_OP_PATCH<N, D, 0, 1, S, 1>; \
   static const ArmOpCompiler ARM_OP_##T##_ROR_REG = ARM_OP_PATCH<N, D, 1, 1, S, 2>; \
   static const ArmOpCompiler ARM_OP_##T##_IMM_VAL = ARM_OP_PATCH<N, D, 0, 0, S, 1>

ARM_ALU_OP_DEF(AND  , 2, 1, false);
ARM_ALU_OP_DEF(AND_S, 2, 1, true);
ARM_ALU_OP_DEF(EOR  , 2, 1, false);
ARM_ALU_OP_DEF(EOR_S, 2, 1, true);
ARM_ALU_OP_DEF(SUB  , 2, 1, false);
ARM_ALU_OP_DEF(SUB_S, 2, 1, true);
ARM_ALU_OP_DEF(RSB  , 2, 1, false);
ARM_ALU_OP_DEF(RSB_S, 2, 1, true);
ARM_ALU_OP_DEF(ADD  , 2, 1, false);
ARM_ALU_OP_DEF(ADD_S, 2, 1, true);
ARM_ALU_OP_DEF(ADC  , 2, 1, false);
ARM_ALU_OP_DEF(ADC_S, 2, 1, true);
ARM_ALU_OP_DEF(SBC  , 2, 1, false);
ARM_ALU_OP_DEF(SBC_S, 2, 1, true);
ARM_ALU_OP_DEF(RSC  , 2, 1, false);
ARM_ALU_OP_DEF(RSC_S, 2, 1, true);
ARM_ALU_OP_DEF(TST  , 0, 1, true);
ARM_ALU_OP_DEF(TEQ  , 0, 1, true);
ARM_ALU_OP_DEF(CMP  , 0, 1, true);
ARM_ALU_OP_DEF(CMN  , 0, 1, true);
ARM_ALU_OP_DEF(ORR  , 2, 1, false);
ARM_ALU_OP_DEF(ORR_S, 2, 1, true);
ARM_ALU_OP_DEF(MOV  , 2, 0, false);
ARM_ALU_OP_DEF(MOV_S, 2, 0, true);
ARM_ALU_OP_DEF(BIC  , 2, 1, false);
ARM_ALU_OP_DEF(BIC_S, 2, 1, true);
ARM_ALU_OP_DEF(MVN  , 2, 0, false);
ARM_ALU_OP_DEF(MVN_S, 2, 0, true);

// HACK: multiply cycles are wrong
#define ARM_OP_MUL         ARM_OP_PATCH<2, 0, 1, 1, false, 3>
#define ARM_OP_MUL_S       ARM_OP_PATCH<2, 0, 1, 1, true, 3>
#define ARM_OP_MLA         ARM_OP_PATCH<2, 1, 1, 1, false, 4>
#define ARM_OP_MLA_S       ARM_OP_PATCH<2, 1, 1, 1, true, 4>
#define ARM_OP_UMULL       ARM_OP_PATCH<2, 2, 1, 1, false, 4>
#define ARM_OP_UMULL_S     ARM_OP_PATCH<2, 2, 1, 1, true, 4>
#define ARM_OP_UMLAL       ARM_OP_PATCH<3, 3, 1, 1, false, 5>
#define ARM_OP_UMLAL_S     ARM_OP_PATCH<3, 3, 1, 1, true, 5>
#define ARM_OP_SMULL       ARM_OP_PATCH<2, 2, 1, 1, false, 4>
#define ARM_OP_SMULL_S     ARM_OP_PATCH<2, 2, 1, 1, true, 4>
#define ARM_OP_SMLAL       ARM_OP_PATCH<3, 3, 1, 1, false, 5>
#define ARM_OP_SMLAL_S     ARM_OP_PATCH<3, 3, 1, 1, true, 5>

#define ARM_OP_SMUL_B_B    ARM_OP_PATCH<2, 0, 1, 1, true, 2>
#define ARM_OP_SMUL_T_B    ARM_OP_PATCH<2, 0, 1, 1, true, 2>
#define ARM_OP_SMUL_B_T    ARM_OP_PATCH<2, 0, 1, 1, true, 2>
#define ARM_OP_SMUL_T_T    ARM_OP_PATCH<2, 0, 1, 1, true, 2>

#define ARM_OP_SMLA_B_B    ARM_OP_PATCH<2, 1, 1, 1, true, 2>
#define ARM_OP_SMLA_T_B    ARM_OP_PATCH<2, 1, 1, 1, true, 2>
#define ARM_OP_SMLA_B_T    ARM_OP_PATCH<2, 1, 1, 1, true, 2>
#define ARM_OP_SMLA_T_T    ARM_OP_PATCH<2, 1, 1, 1, true, 2>

#define ARM_OP_SMULW_B     ARM_OP_PATCH<2, 0, 1, 1, true, 2>
#define ARM_OP_SMULW_T     ARM_OP_PATCH<2, 0, 1, 1, true, 2>
#define ARM_OP_SMLAW_B     ARM_OP_PATCH<2, 1, 1, 1, true, 2>
#define ARM_OP_SMLAW_T     ARM_OP_PATCH<2, 1, 1, 1, true, 2>

#define ARM_OP_SMLAL_B_B   ARM_OP_PATCH<3, 3, 1, 1, true, 2>
#define ARM_OP_SMLAL_T_B   ARM_OP_PATCH<3, 3, 1, 1, true, 2>
#define ARM_OP_SMLAL_B_T   ARM_OP_PATCH<3, 3, 1, 1, true, 2>
#define ARM_OP_SMLAL_T_T   ARM_OP_PATCH<3, 3, 1, 1, true, 2>

#define ARM_OP_QADD        ARM_OP_PATCH<1, 2, 0, 1, true, 2>
#define ARM_OP_QSUB        ARM_OP_PATCH<1, 2, 0, 1, true, 2>
#define ARM_OP_QDADD       ARM_OP_PATCH<1, 2, 0, 1, true, 2>
#define ARM_OP_QDSUB       ARM_OP_PATCH<1, 2, 0, 1, true, 2>

#define ARM_OP_CLZ         ARM_OP_PATCH<0, 2, 0, 1, false, 2>

////////
// Need versions of these functions with exported symbol
u8  _MMU_read08_9(u32 addr) { return _MMU_read08<0>(addr); }
u8  _MMU_read08_7(u32 addr) { return _MMU_read08<1>(addr); }
u16 _MMU_read16_9(u32 addr) { return _MMU_read16<0>(addr & 0xFFFFFFFE); }
u16 _MMU_read16_7(u32 addr) { return _MMU_read16<1>(addr & 0xFFFFFFFE); }
u32 _MMU_read32_9(u32 addr) { return ::ROR(_MMU_read32<0>(addr & 0xFFFFFFFC), 8 * (addr & 3)); }
u32 _MMU_read32_7(u32 addr) { return ::ROR(_MMU_read32<1>(addr & 0xFFFFFFFC), 8 * (addr & 3)); }

void _MMU_write08_9(u32 addr, u8  val) { _MMU_write08<0>(addr, val); }
void _MMU_write08_7(u32 addr, u8  val) { _MMU_write08<1>(addr, val); }
void _MMU_write16_9(u32 addr, u16 val) { _MMU_write16<0>(addr & 0xFFFFFFFE, val); }
void _MMU_write16_7(u32 addr, u16 val) { _MMU_write16<1>(addr & 0xFFFFFFFE, val); }
void _MMU_write32_9(u32 addr, u32 val) { _MMU_write32<0>(addr & 0xFFFFFFFC, val); }
void _MMU_write32_7(u32 addr, u32 val) { _MMU_write32<1>(addr & 0xFFFFFFFC, val); }

static const uint32_t mem_funcs[12] =
{
   (uint32_t)_MMU_read08_9 , (uint32_t)_MMU_read08_7,
   (uint32_t)_MMU_write08_9, (uint32_t)_MMU_write08_7,
   (uint32_t)_MMU_read16_9,  (uint32_t)_MMU_read16_7,
   (uint32_t)_MMU_write16_9, (uint32_t)_MMU_write16_7,
   (uint32_t)_MMU_read32_9,  (uint32_t)_MMU_read32_7,
   (uint32_t)_MMU_write32_9, (uint32_t)_MMU_write32_7
};


static OP_RESULT ARM_OP_MEM(uint32_t pc, const uint32_t opcode)
{
   const AG_COND cond = (AG_COND)bit(opcode, 28, 4);
   const bool has_reg_offset = bit(opcode, 25);
   const bool has_pre_index = bit(opcode, 24);
   const bool has_up_bit = bit(opcode, 23);
   const bool has_byte_bit = bit(opcode, 22);
   const bool has_write_back = bit(opcode, 21);
   const bool has_load = bit(opcode, 20);
   const reg_t rn = bit(opcode, 16, 4);
   const reg_t rd = bit(opcode, 12, 4);
   const reg_t rm = bit(opcode, 0, 4);

   if (rn == 0xF || rd == 0xF || (has_reg_offset && (rm == 0xF)))
      return OPR_INTERPRET;

   int32_t regs[3] = { rd | (((cond == AL) && has_load) ? 0x10 : 0), rn, has_reg_offset ? (int32_t)rm : -1 };
   regman->get(3, regs);

   const reg_t dest = regs[0];
   const reg_t base = regs[1];
   const reg_t offs = has_reg_offset ? regs[2] : 3;

   // HACK: This needs to done manually here as we can't branch over the generated code
   write_status(3);

   if (cond != AL)
   {
      block->b("run", cond);
      block->b("skip");
      block->set_label("run");
   }

   // Put the indexed address in R3
   if (has_reg_offset)
   {
      const AG_ALU_SHIFT st = (AG_ALU_SHIFT)bit(opcode, 5, 2);
      const uint32_t imm = bit(opcode, 7, 5);

      if (has_up_bit) block->add(3, base, alu2::reg_shift_imm(offs, st, imm));
      else            block->sub(3, base, alu2::reg_shift_imm(offs, st, imm));
   }
   else
   {
      block->load_constant(3, opcode & 0xFFF);

      if (has_up_bit) block->add(3, base, alu2::reg(3));
      else            block->sub(3, base, alu2::reg(3));
   }

   // Load EA
   block->mov(0, alu2::reg((has_pre_index ? (reg_t)3 : base)));

   // Do Writeback
   if ((!has_pre_index) || has_write_back)
   {
      block->mov(base, alu2::reg(3));
      regman->mark_dirty(base);
   }

   // DO
   if (!has_load)
   {
      if (has_byte_bit)
      {
         block->uxtb(1, dest);
      }
      else
      {
         block->mov(1, alu2::reg(dest));
      }
   }

   uint32_t func_idx = block_procnum | (has_load ? 0 : 2) | (has_byte_bit ? 0 : 8);
   block->load_constant(2, mem_funcs[func_idx]);
   call(2);

   if (has_load)
   {
      if (has_byte_bit)
      {
         block->uxtb(dest, 0);
      }
      else
      {
         block->mov(dest, alu2::reg(0));
      }

      regman->mark_dirty(dest);
   }

   if (cond != AL)
   {
      block->set_label("skip");
      block->resolve_label("run");
      block->resolve_label("skip");
   }

   // TODO:
   return OPR_RESULT(OPR_CONTINUE, 3);
}

#define ARM_MEM_OP_DEF2(T, Q) \
   static const ArmOpCompiler ARM_OP_##T##_M_LSL_##Q = ARM_OP_MEM; \
   static const ArmOpCompiler ARM_OP_##T##_P_LSL_##Q = ARM_OP_MEM; \
   static const ArmOpCompiler ARM_OP_##T##_M_LSR_##Q = ARM_OP_MEM; \
   static const ArmOpCompiler ARM_OP_##T##_P_LSR_##Q = ARM_OP_MEM; \
   static const ArmOpCompiler ARM_OP_##T##_M_ASR_##Q = ARM_OP_MEM; \
   static const ArmOpCompiler ARM_OP_##T##_P_ASR_##Q = ARM_OP_MEM; \
   static const ArmOpCompiler ARM_OP_##T##_M_ROR_##Q = ARM_OP_MEM; \
   static const ArmOpCompiler ARM_OP_##T##_P_ROR_##Q = ARM_OP_MEM; \
   static const ArmOpCompiler ARM_OP_##T##_M_##Q = ARM_OP_MEM; \
   static const ArmOpCompiler ARM_OP_##T##_P_##Q = ARM_OP_MEM

#define ARM_MEM_OP_DEF(T) \
   ARM_MEM_OP_DEF2(T, IMM_OFF_PREIND); \
   ARM_MEM_OP_DEF2(T, IMM_OFF); \
   ARM_MEM_OP_DEF2(T, IMM_OFF_POSTIND)

ARM_MEM_OP_DEF(STR);
ARM_MEM_OP_DEF(LDR);
ARM_MEM_OP_DEF(STRB);
ARM_MEM_OP_DEF(LDRB);

//
static OP_RESULT ARM_OP_MEM_HALF(uint32_t pc, uint32_t opcode)
{
   const AG_COND cond = (AG_COND)bit(opcode, 28, 4);
   const bool has_pre_index = bit(opcode, 24);
   const bool has_up_bit = bit(opcode, 23);
   const bool has_imm_offset = bit(opcode, 22);
   const bool has_write_back = bit(opcode, 21);
   const bool has_load = bit(opcode, 20);
   const uint32_t op = bit(opcode, 5, 2);
   const reg_t rn = bit(opcode, 16, 4);
   const reg_t rd = bit(opcode, 12, 4);
   const reg_t rm = bit(opcode, 0, 4);

   if (rn == 0xF || rd == 0xF || (!has_imm_offset && (rm == 0xF)))
      return OPR_INTERPRET;

   int32_t regs[3] = { rd | (((cond == AL) && has_load) ? 0x10 : 0), rn, (!has_imm_offset) ? (int32_t)rm : -1 };
   regman->get(3, regs);

   const reg_t dest = regs[0];
   const reg_t base = regs[1];
   const reg_t offs = (!has_imm_offset) ? regs[2] : 0;

   // HACK: This needs to done manually here as we can't branch over the generated code
   write_status(3);

   if (cond != AL)
   {
      block->b("run", cond);
      block->b("skip");
      block->set_label("run");
   }

   // Put the indexed address in R3
   if (!has_imm_offset)
   {
      if (has_up_bit) block->add(3, base, alu2::reg(offs));
      else            block->sub(3, base, alu2::reg(offs));
   }
   else
   {
      block->load_constant(3, (opcode & 0xF) | ((opcode >> 4) & 0xF0));

      if (has_up_bit) block->add(3, base, alu2::reg(3));
      else            block->sub(3, base, alu2::reg(3));
   }

   // Load EA
   block->mov(0, alu2::reg((has_pre_index ? (reg_t)3 : base)));

   // Do Writeback
   if ((!has_pre_index) || has_write_back)
   {
      block->mov(base, alu2::reg(3));
      regman->mark_dirty(base);
   }

   // DO
   if (!has_load)
   {
      switch (op)
      {
         case 1: block->uxth(1, dest); break;
         case 2: block->sxtb(1, dest); break;
         case 3: block->sxth(1, dest); break;
      }
   }

   uint32_t func_idx = block_procnum | (has_load ? 0 : 2) | ((op == 2) ? 0 : 4);
   block->load_constant(2, mem_funcs[func_idx]);
   call(2);

   if (has_load)
   {
      switch (op)
      {
         case 1: block->uxth(dest, 0); break;
         case 2: block->sxtb(dest, 0); break;
         case 3: block->sxth(dest, 0); break;
      }

      regman->mark_dirty(dest);
   }

   if (cond != AL)
   {
      block->set_label("skip");
      block->resolve_label("run");
      block->resolve_label("skip");
   }

   // TODO:
   return OPR_RESULT(OPR_CONTINUE, 3);
}

#define ARM_MEM_HALF_OP_DEF2(T, P) \
   static const ArmOpCompiler ARM_OP_##T##_##P##M_REG_OFF = ARM_OP_MEM_HALF; \
   static const ArmOpCompiler ARM_OP_##T##_##P##P_REG_OFF = ARM_OP_MEM_HALF; \
   static const ArmOpCompiler ARM_OP_##T##_##P##M_IMM_OFF = ARM_OP_MEM_HALF; \
   static const ArmOpCompiler ARM_OP_##T##_##P##P_IMM_OFF = ARM_OP_MEM_HALF

#define ARM_MEM_HALF_OP_DEF(T) \
   ARM_MEM_HALF_OP_DEF2(T, POS_INDE_); \
   ARM_MEM_HALF_OP_DEF2(T, ); \
   ARM_MEM_HALF_OP_DEF2(T, PRE_INDE_)

ARM_MEM_HALF_OP_DEF(STRH);
ARM_MEM_HALF_OP_DEF(LDRH);
ARM_MEM_HALF_OP_DEF(STRSB);
ARM_MEM_HALF_OP_DEF(LDRSB);
ARM_MEM_HALF_OP_DEF(STRSH);
ARM_MEM_HALF_OP_DEF(LDRSH);

//
#define SIGNEXTEND_24(i) (((s32)i<<8)>>8)
static OP_RESULT ARM_OP_B_BL(uint32_t pc, uint32_t opcode)
{
   const AG_COND cond = (AG_COND)bit(opcode, 28, 4);
   const bool has_link = bit(opcode, 24);

   const bool unconditional = (cond == AL || cond == EGG);
   int32_t regs[1] = { (has_link || cond == EGG) ? 14 : -1 };
   regman->get(1, regs);

   uint32_t dest = (pc + 8 + (SIGNEXTEND_24(bit(opcode, 0, 24)) << 2));

   if (!unconditional)
   {
      block->load_constant(0, pc + 4);

      block->b("run", cond);
      block->b("skip");
      block->set_label("run");
   }

   if (cond == EGG)
   {
      change_mode(true);

      if (has_link)
      {
         dest += 2;
      }
   }

   if (has_link || cond == EGG)
   {
      block->load_constant(regs[0], pc + 4);
      regman->mark_dirty(regs[0]);
   }

   block->load_constant(0, dest);

   if (!unconditional)
   {
      block->set_label("skip");
      block->resolve_label("run");
      block->resolve_label("skip");
   }

   block->str(0, RCPU, mem2::imm(offsetof(armcpu_t, instruct_adr)));


   // TODO: Timing
   return OPR_RESULT(OPR_BRANCHED, 3);
}

#define ARM_OP_B  ARM_OP_B_BL
#define ARM_OP_BL ARM_OP_B_BL

////

#define ARM_OP_LDRD_STRD_POST_INDEX 0
#define ARM_OP_LDRD_STRD_OFFSET_PRE_INDEX 0
#define ARM_OP_MRS_CPSR 0
#define ARM_OP_SWP 0
#define ARM_OP_MSR_CPSR 0
#define ARM_OP_BX 0
#define ARM_OP_BLX_REG 0
#define ARM_OP_BKPT 0
#define ARM_OP_MRS_SPSR 0
#define ARM_OP_SWPB 0
#define ARM_OP_MSR_SPSR 0
#define ARM_OP_STREX 0
#define ARM_OP_LDREX 0
#define ARM_OP_MSR_CPSR_IMM_VAL 0
#define ARM_OP_MSR_SPSR_IMM_VAL 0
#define ARM_OP_STMDA 0
#define ARM_OP_LDMDA 0
#define ARM_OP_STMDA_W 0
#define ARM_OP_LDMDA_W 0
#define ARM_OP_STMDA2 0
#define ARM_OP_LDMDA2 0
#define ARM_OP_STMDA2_W 0
#define ARM_OP_LDMDA2_W 0
#define ARM_OP_STMIA 0
#define ARM_OP_LDMIA 0
#define ARM_OP_STMIA_W 0
#define ARM_OP_LDMIA_W 0
#define ARM_OP_STMIA2 0
#define ARM_OP_LDMIA2 0
#define ARM_OP_STMIA2_W 0
#define ARM_OP_LDMIA2_W 0
#define ARM_OP_STMDB 0
#define ARM_OP_LDMDB 0
#define ARM_OP_STMDB_W 0
#define ARM_OP_LDMDB_W 0
#define ARM_OP_STMDB2 0
#define ARM_OP_LDMDB2 0
#define ARM_OP_STMDB2_W 0
#define ARM_OP_LDMDB2_W 0
#define ARM_OP_STMIB 0
#define ARM_OP_LDMIB 0
#define ARM_OP_STMIB_W 0
#define ARM_OP_LDMIB_W 0
#define ARM_OP_STMIB2 0
#define ARM_OP_LDMIB2 0
#define ARM_OP_STMIB2_W 0
#define ARM_OP_LDMIB2_W 0
#define ARM_OP_STC_OPTION 0
#define ARM_OP_LDC_OPTION 0
#define ARM_OP_STC_M_POSTIND 0
#define ARM_OP_LDC_M_POSTIND 0
#define ARM_OP_STC_P_POSTIND 0
#define ARM_OP_LDC_P_POSTIND 0
#define ARM_OP_STC_M_IMM_OFF 0
#define ARM_OP_LDC_M_IMM_OFF 0
#define ARM_OP_STC_M_PREIND 0
#define ARM_OP_LDC_M_PREIND 0
#define ARM_OP_STC_P_IMM_OFF 0
#define ARM_OP_LDC_P_IMM_OFF 0
#define ARM_OP_STC_P_PREIND 0
#define ARM_OP_LDC_P_PREIND 0
#define ARM_OP_CDP 0
#define ARM_OP_MCR 0
#define ARM_OP_MRC 0
#define ARM_OP_SWI 0
#define ARM_OP_UND 0
static const ArmOpCompiler arm_instruction_compilers[4096] = {
#define TABDECL(x) ARM_##x
#include "instruction_tabdef.inc"
#undef TABDECL
};

////////
// THUMB
////////
static OP_RESULT THUMB_OP_SHIFT(uint32_t pc, uint32_t opcode)
{
   const uint32_t rd = bit(opcode, 0, 3);
   const uint32_t rs = bit(opcode, 3, 3);
   const uint32_t imm = bit(opcode, 6, 5);
   const AG_ALU_SHIFT op = (AG_ALU_SHIFT)bit(opcode, 11, 2);

   int32_t regs[2] = { rd | 0x10, rs };
   regman->get(2, regs);

   const reg_t nrd = regs[0];
   const reg_t nrs = regs[1];

   block->movs(nrd, alu2::reg_shift_imm(nrs, op, imm));
   mark_status_dirty();

   regman->mark_dirty(nrd);

   return OPR_RESULT(OPR_CONTINUE, 1);
}

static OP_RESULT THUMB_OP_ADDSUB_REGIMM(uint32_t pc, uint32_t opcode)
{
   const uint32_t rd = bit(opcode, 0, 3);
   const uint32_t rs = bit(opcode, 3, 3);
   const AG_ALU_OP op = bit(opcode, 9) ? SUBS : ADDS;
   const bool arg_type = bit(opcode, 10);
   const uint32_t arg = bit(opcode, 6, 3);

   int32_t regs[3] = { rd | 0x10, rs, (!arg_type) ? arg : -1 };
   regman->get(3, regs);

   const reg_t nrd = regs[0];
   const reg_t nrs = regs[1];

   if (arg_type) // Immediate
   {
      block->alu_op(op, nrd, nrs, alu2::imm(arg));
      mark_status_dirty();
   }
   else
   {
      block->alu_op(op, nrd, nrs, alu2::reg(regs[2]));
      mark_status_dirty();
   }

   regman->mark_dirty(nrd);

   return OPR_RESULT(OPR_CONTINUE, 1);
}

static OP_RESULT THUMB_OP_MCAS_IMM8(uint32_t pc, uint32_t opcode)
{
   const reg_t rd = bit(opcode, 8, 3);
   const uint32_t op = bit(opcode, 11, 2);
   const uint32_t imm = bit(opcode, 0, 8);

   int32_t regs[1] = { rd };
   regman->get(1, regs);
   const reg_t nrd = regs[0];

   switch (op)
   {
      case 0: block->alu_op(MOVS, nrd, nrd, alu2::imm(imm)); break;
      case 1: block->alu_op(CMP , nrd, nrd, alu2::imm(imm)); break;
      case 2: block->alu_op(ADDS, nrd, nrd, alu2::imm(imm)); break;
      case 3: block->alu_op(SUBS, nrd, nrd, alu2::imm(imm)); break;
   }

   mark_status_dirty();

   if (op != 1) // Don't keep the result of a CMP instruction
   {
      regman->mark_dirty(nrd);
   }

   return OPR_RESULT(OPR_CONTINUE, 1);
}

static OP_RESULT THUMB_OP_ALU(uint32_t pc, uint32_t opcode)
{
   const uint32_t rd = bit(opcode, 0, 3);
   const uint32_t rs = bit(opcode, 3, 3);
   const uint32_t op = bit(opcode, 6, 4);
   bool need_writeback = false;

   if (op == 13) // TODO: The MULS is interpreted for now
   {
      return OPR_INTERPRET;
   }

   int32_t regs[2] = { rd, rs };
   regman->get(2, regs);

   const reg_t nrd = regs[0];
   const reg_t nrs = regs[1];

   switch (op)
   {
      case  0: block->ands(nrd, alu2::reg(nrs)); break;
      case  1: block->eors(nrd, alu2::reg(nrs)); break;
      case  5: block->adcs(nrd, alu2::reg(nrs)); break;
      case  6: block->sbcs(nrd, alu2::reg(nrs)); break;
      case  8: block->tst (nrd, alu2::reg(nrs)); break;
      case 10: block->cmp (nrd, alu2::reg(nrs)); break;
      case 11: block->cmn (nrd, alu2::reg(nrs)); break;
      case 12: block->orrs(nrd, alu2::reg(nrs)); break;
      case 14: block->bics(nrd, alu2::reg(nrs)); break;
      case 15: block->mvns(nrd, alu2::reg(nrs)); break;

      case  2: block->movs(nrd, alu2::reg_shift_reg(nrd, LSL, nrs)); break;
      case  3: block->movs(nrd, alu2::reg_shift_reg(nrd, LSR, nrs)); break;
      case  4: block->movs(nrd, alu2::reg_shift_reg(nrd, ASR, nrs)); break;
      case  7: block->movs(nrd, alu2::reg_shift_reg(nrd, arm_gen::ROR, nrs)); break;

      case  9: block->rsbs(nrd, nrs, alu2::imm(0)); break;
   }

   mark_status_dirty();

   static const bool op_wb[16] = { 1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 0, 0, 1, 0, 1, 1 };
   if (op_wb[op])
   {
      regman->mark_dirty(nrd);
   }

   return OPR_RESULT(OPR_CONTINUE, 1);
}

static OP_RESULT THUMB_OP_SPE(uint32_t pc, uint32_t opcode)
{
   const uint32_t rd = bit(opcode, 0, 3) + (bit(opcode, 7) ? 8 : 0);
   const uint32_t rs = bit(opcode, 3, 4);
   const uint32_t op = bit(opcode, 8, 2);

   if (rd == 0xF || rs == 0xF)
   {
      return OPR_INTERPRET;
   }

   int32_t regs[2] = { rd, rs };
   regman->get(2, regs);

   const reg_t nrd = regs[0];
   const reg_t nrs = regs[1];

   switch (op)
   {
      case 0: block->add(nrd, alu2::reg(nrs)); break;
      case 1: block->cmp(nrd, alu2::reg(nrs)); break;
      case 2: block->mov(nrd, alu2::reg(nrs)); break;
   }

   if (op != 1)
   {
      regman->mark_dirty(nrd);
   }
   else
   {
      mark_status_dirty();
   }

   return OPR_RESULT(OPR_CONTINUE, 1);
}

static OP_RESULT THUMB_OP_MEMORY_DELEGATE(uint32_t pc, uint32_t opcode, bool LOAD, uint32_t SIZE, uint32_t EXTEND, bool REG_OFFSET)
{
   const uint32_t rd = bit(opcode, 0, 3);
   const uint32_t rb = bit(opcode, 3, 3);
   const uint32_t ro = bit(opcode, 6, 3);
   const uint32_t off = bit(opcode, 6, 5);

   int32_t regs[3] = { rd | (LOAD ? 0x10 : 0), rb, REG_OFFSET ? ro : -1};
   regman->get(3, regs);

   const reg_t dest = regs[0];
   const reg_t base = regs[1];

   // Calc EA

   if (REG_OFFSET)
   {
      const reg_t offset = regs[2];
      block->mov(0, alu2::reg(base));
      block->add(0, alu2::reg(offset));
   }
   else
   {
      block->add(0, base, alu2::imm(off << SIZE));
   }

   // Load access function
   block->load_constant(2, mem_funcs[(SIZE << 2) + (LOAD ? 0 : 2) + block_procnum]);

   if (!LOAD)
   {
      block->mov(1, alu2::reg(dest));
   }

   call(2);

   if (LOAD)
   {
      if (EXTEND)
      {
         if (SIZE == 0)
         {
            block->sxtb(dest, 0);
         }
         else
         {
            block->sxth(dest, 0);
         }
      }
      else
      {
         block->mov(dest, alu2::reg(0));
      }

      regman->mark_dirty(dest);
   }

   // TODO
   return OPR_RESULT(OPR_CONTINUE, 3);
}

// SIZE: 0=8, 1=16, 2=32
template <bool LOAD, uint32_t SIZE, uint32_t EXTEND, bool REG_OFFSET>
static OP_RESULT THUMB_OP_MEMORY(uint32_t pc, uint32_t opcode)
{
   return THUMB_OP_MEMORY_DELEGATE(pc, opcode, LOAD, SIZE, EXTEND, REG_OFFSET);
}

static OP_RESULT THUMB_OP_LDR_PCREL(uint32_t pc, uint32_t opcode)
{
   const uint32_t offset = bit(opcode, 0, 8);
   const reg_t rd = bit(opcode, 8, 3);

   int32_t regs[1] = { rd | 0x10 };
   regman->get(1, regs);

   const reg_t dest = regs[0];

   block->load_constant(0, ((pc + 4) & ~2) + (offset << 2));
   block->load_constant(2, mem_funcs[8 + block_procnum]);
   call(2);
   block->mov(dest, alu2::reg(0));

   regman->mark_dirty(dest);
   return OPR_RESULT(OPR_CONTINUE, 3);
}

static OP_RESULT THUMB_OP_STR_SPREL(uint32_t pc, uint32_t opcode)
{
   const uint32_t offset = bit(opcode, 0, 8);
   const reg_t rd = bit(opcode, 8, 3);

   int32_t regs[2] = { rd, 13 };
   regman->get(2, regs);

   const reg_t src = regs[0];
   const reg_t base = regs[1];

   block->add(0, base, alu2::imm_rol(offset, 2));
   block->mov(1, alu2::reg(src));
   block->load_constant(2, mem_funcs[10 + block_procnum]);
   call(2);

   return OPR_RESULT(OPR_CONTINUE, 3);
}

static OP_RESULT THUMB_OP_LDR_SPREL(uint32_t pc, uint32_t opcode)
{
   const uint32_t offset = bit(opcode, 0, 8);
   const reg_t rd = bit(opcode, 8, 3);

   int32_t regs[2] = { rd | 0x10, 13 };
   regman->get(2, regs);

   const reg_t dest = regs[0];
   const reg_t base = regs[1];

   block->add(0, base, alu2::imm_rol(offset, 2));
   block->load_constant(2, mem_funcs[8 + block_procnum]);
   call(2);
   block->mov(dest, alu2::reg(0));

   regman->mark_dirty(dest);
   return OPR_RESULT(OPR_CONTINUE, 3);
}

static OP_RESULT THUMB_OP_B_COND(uint32_t pc, uint32_t opcode)
{
   const AG_COND cond = (AG_COND)bit(opcode, 8, 4);

   block->load_constant(0, pc + 2);
   block->load_constant(0, (pc + 4) + ((u32)((s8)(opcode&0xFF))<<1), cond);
   block->str(0, RCPU, mem2::imm(offsetof(armcpu_t, instruct_adr)));

   block->add(RCYC, alu2::imm(2), cond);

   return OPR_RESULT(OPR_BRANCHED, 1);
}

static OP_RESULT THUMB_OP_B_UNCOND(uint32_t pc, uint32_t opcode)
{
   int32_t offs = (opcode & 0x7FF) | (bit(opcode, 10) ? 0xFFFFF800 : 0);
   block->load_constant(0, pc + 4 + (offs << 1));

   block->str(0, RCPU, mem2::imm(offsetof(armcpu_t, instruct_adr)));

   return OPR_RESULT(OPR_BRANCHED, 3);
}

static OP_RESULT THUMB_OP_ADJUST_SP(uint32_t pc, uint32_t opcode)
{
   const uint32_t offs = bit(opcode, 0, 7);

   int32_t regs[1] = { 13 };
   regman->get(1, regs);

   const reg_t sp = regs[0];

   if (bit(opcode, 7)) block->sub(sp, alu2::imm_rol(offs, 2));
   else                block->add(sp, alu2::imm_rol(offs, 2));

   regman->mark_dirty(sp);

   return OPR_RESULT(OPR_CONTINUE, 1);
}

static OP_RESULT THUMB_OP_ADD_2PC(uint32_t pc, uint32_t opcode)
{
   const uint32_t offset = bit(opcode, 0, 8);
   const reg_t rd = bit(opcode, 8, 3);

   int32_t regs[1] = { rd | 0x10 };
   regman->get(1, regs);

   const reg_t dest = regs[0];

   block->load_constant(dest, ((pc + 4) & 0xFFFFFFFC) + (offset << 2));
   regman->mark_dirty(dest);

   return OPR_RESULT(OPR_CONTINUE, 1);
}

static OP_RESULT THUMB_OP_ADD_2SP(uint32_t pc, uint32_t opcode)
{
   const uint32_t offset = bit(opcode, 0, 8);
   const reg_t rd = bit(opcode, 8, 3);

   int32_t regs[2] = { 13, rd | 0x10 };
   regman->get(2, regs);

   const reg_t sp = regs[0];
   const reg_t dest = regs[1];

   block->add(dest, sp, alu2::imm_rol(offset, 2));
   regman->mark_dirty(dest);

   return OPR_RESULT(OPR_CONTINUE, 1);
}

static OP_RESULT THUMB_OP_BX_BLX_THUMB(uint32_t pc, uint32_t opcode)
{
   const reg_t rm = bit(opcode, 3, 4);
   const bool link = bit(opcode, 7);

   if (rm == 15)
      return OPR_INTERPRET;

   block->load_constant(0, pc + 4);

   int32_t regs[2] = { link ? 14 : -1, (rm != 15) ? (int32_t)rm : -1 };
   regman->get(2, regs);

   if (link)
   {
      const reg_t lr = regs[0];
      block->sub(lr, 0, alu2::imm(1));
      regman->mark_dirty(lr);
   }

   reg_t target = regs[1];

   change_mode_reg(target, 2, 3);
   block->bic(0, target, alu2::imm(1));
   block->str(0, RCPU, mem2::imm(offsetof(armcpu_t, instruct_adr)));

   return OPR_RESULT(OPR_BRANCHED, 3);
}

#if 1
#define THUMB_OP_BL_LONG 0
#else
static OP_RESULT THUMB_OP_BL_LONG(uint32_t pc, uint32_t opcode)
{
   static const uint32_t op = bit(opcode, 11, 5);
   int32_t offset = bit(opcode, 0, 11);

   reg_t lr = regman->get(14, op == 0x1E);

   if (op == 0x1E)
   {
      offset |= (offset & 0x400) ? 0xFFFFF800 : 0;
      block->load_constant(lr, (pc + 4) + (offset << 12));
   }
   else
   {
      block->load_constant(0, offset << 1);

      block->add(0, lr, alu2::reg(0));
      block->str(0, RCPU, mem2::imm(offsetof(armcpu_t, instruct_adr)));

      block->load_constant(lr, pc + 3);

      if (op != 0x1F)
      {
         change_mode(false);
      }
   }

   regman->mark_dirty(lr);

   if (op == 0x1E)
   {
      return OPR_RESULT(OPR_CONTINUE, 1);
   }
   else
   {
      return OPR_RESULT(OPR_BRANCHED, (op == 0x1F) ? 3 : 4);
   }
}
#endif

#define THUMB_OP_INTERPRET       0
#define THUMB_OP_UND_THUMB       THUMB_OP_INTERPRET

#define THUMB_OP_LSL             THUMB_OP_SHIFT
#define THUMB_OP_LSL_0           THUMB_OP_SHIFT
#define THUMB_OP_LSR             THUMB_OP_SHIFT
#define THUMB_OP_LSR_0           THUMB_OP_SHIFT
#define THUMB_OP_ASR             THUMB_OP_SHIFT
#define THUMB_OP_ASR_0           THUMB_OP_SHIFT

#define THUMB_OP_ADD_REG         THUMB_OP_ADDSUB_REGIMM
#define THUMB_OP_SUB_REG         THUMB_OP_ADDSUB_REGIMM
#define THUMB_OP_ADD_IMM3        THUMB_OP_ADDSUB_REGIMM
#define THUMB_OP_SUB_IMM3        THUMB_OP_ADDSUB_REGIMM

#define THUMB_OP_MOV_IMM8        THUMB_OP_MCAS_IMM8
#define THUMB_OP_CMP_IMM8        THUMB_OP_MCAS_IMM8
#define THUMB_OP_ADD_IMM8        THUMB_OP_MCAS_IMM8
#define THUMB_OP_SUB_IMM8        THUMB_OP_MCAS_IMM8

#define THUMB_OP_AND             THUMB_OP_ALU
#define THUMB_OP_EOR             THUMB_OP_ALU
#define THUMB_OP_LSL_REG         THUMB_OP_ALU
#define THUMB_OP_LSR_REG         THUMB_OP_ALU
#define THUMB_OP_ASR_REG         THUMB_OP_ALU
#define THUMB_OP_ADC_REG         THUMB_OP_ALU
#define THUMB_OP_SBC_REG         THUMB_OP_ALU
#define THUMB_OP_ROR_REG         THUMB_OP_ALU
#define THUMB_OP_TST             THUMB_OP_ALU
#define THUMB_OP_NEG             THUMB_OP_ALU
#define THUMB_OP_CMP             THUMB_OP_ALU
#define THUMB_OP_CMN             THUMB_OP_ALU
#define THUMB_OP_ORR             THUMB_OP_ALU
#define THUMB_OP_MUL_REG         THUMB_OP_INTERPRET
#define THUMB_OP_BIC             THUMB_OP_ALU
#define THUMB_OP_MVN             THUMB_OP_ALU

#define THUMB_OP_ADD_SPE         THUMB_OP_SPE
#define THUMB_OP_CMP_SPE         THUMB_OP_SPE
#define THUMB_OP_MOV_SPE         THUMB_OP_SPE

#define THUMB_OP_ADJUST_P_SP     THUMB_OP_ADJUST_SP
#define THUMB_OP_ADJUST_M_SP     THUMB_OP_ADJUST_SP

#define THUMB_OP_LDRB_REG_OFF    THUMB_OP_MEMORY<true , 0, 0, true>
#define THUMB_OP_LDRH_REG_OFF    THUMB_OP_MEMORY<true , 1, 0, true>
#define THUMB_OP_LDR_REG_OFF     THUMB_OP_MEMORY<true , 2, 0, true>

#define THUMB_OP_STRB_REG_OFF    THUMB_OP_MEMORY<false, 0, 0, true>
#define THUMB_OP_STRH_REG_OFF    THUMB_OP_MEMORY<false, 1, 0, true>
#define THUMB_OP_STR_REG_OFF     THUMB_OP_MEMORY<false, 2, 0, true>

#define THUMB_OP_LDRB_IMM_OFF    THUMB_OP_MEMORY<true , 0, 0, false>
#define THUMB_OP_LDRH_IMM_OFF    THUMB_OP_MEMORY<true , 1, 0, false>
#define THUMB_OP_LDR_IMM_OFF     THUMB_OP_MEMORY<true , 2, 0, false>

#define THUMB_OP_STRB_IMM_OFF    THUMB_OP_MEMORY<false, 0, 0, false>
#define THUMB_OP_STRH_IMM_OFF    THUMB_OP_MEMORY<false, 1, 0, false>
#define THUMB_OP_STR_IMM_OFF     THUMB_OP_MEMORY<false, 2, 0, false>

#define THUMB_OP_LDRSB_REG_OFF   THUMB_OP_MEMORY<true , 0, 1, true>
#define THUMB_OP_LDRSH_REG_OFF   THUMB_OP_MEMORY<true , 1, 1, true>

#define THUMB_OP_BX_THUMB        THUMB_OP_BX_BLX_THUMB
#define THUMB_OP_BLX_THUMB       THUMB_OP_BX_BLX_THUMB
#define THUMB_OP_BL_10           THUMB_OP_BL_LONG
#define THUMB_OP_BL_11           THUMB_OP_BL_LONG
#define THUMB_OP_BLX             THUMB_OP_BL_LONG


// UNDEFINED OPS
#define THUMB_OP_PUSH            THUMB_OP_INTERPRET
#define THUMB_OP_PUSH_LR         THUMB_OP_INTERPRET
#define THUMB_OP_POP             THUMB_OP_INTERPRET
#define THUMB_OP_POP_PC          THUMB_OP_INTERPRET
#define THUMB_OP_BKPT_THUMB      THUMB_OP_INTERPRET
#define THUMB_OP_STMIA_THUMB     THUMB_OP_INTERPRET
#define THUMB_OP_LDMIA_THUMB     THUMB_OP_INTERPRET
#define THUMB_OP_SWI_THUMB       THUMB_OP_INTERPRET

static const ArmOpCompiler thumb_instruction_compilers[1024] = {
#define TABDECL(x) THUMB_##x
#include "thumb_tabdef.inc"
#undef TABDECL
};



// ============================================================================================= IMM

//-----------------------------------------------------------------------------
//   Compiler
//-----------------------------------------------------------------------------

static u32 instr_attributes(bool thumb, u32 opcode)
{
   return thumb ? thumb_attributes[opcode>>6]
                : instruction_attributes[INSTRUCTION_INDEX(opcode)];
}

static bool instr_is_branch(bool thumb, u32 opcode)
{
   u32 x = instr_attributes(thumb, opcode);
   if(thumb)
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



template<int PROCNUM>
static ArmOpCompiled compile_basicblock()
{
   block_procnum = PROCNUM;

   const bool thumb = ARMPROC.CPSR.bits.T == 1;
   const u32 base = ARMPROC.instruct_adr;
   const u32 isize = thumb ? 2 : 4;

   uint32_t pc = base;
   bool compiled_op = true;
   bool has_ended = false;
   uint32_t constant_cycles = 0;

   // NOTE: Expected register usage
   // R5 = Pointer to ARMPROC
   // R6 = Cycle counter

   regman->reset();
   block->push(0x4DF0);

   block->load_constant(RCPU, (uint32_t)&ARMPROC);
   block->load_constant(RCYC, 0);

   load_status(3);

   for (uint32_t i = 0; i < CommonSettings.jit_max_block_size && !has_ended; i ++, pc += isize)
   {
      uint32_t opcode = thumb ? _MMU_read16<PROCNUM, MMU_AT_CODE>(pc) : _MMU_read32<PROCNUM, MMU_AT_CODE>(pc);

      ArmOpCompiler compiler = thumb ? thumb_instruction_compilers[opcode >> 6]
                                     : arm_instruction_compilers[INSTRUCTION_INDEX(opcode)];

      int result = compiler ? compiler(pc, opcode) : OPR_INTERPRET;

      constant_cycles += OPR_RESULT_CYCLES(result);
      switch (OPR_RESULT_ACTION(result))
      {
         case OPR_INTERPRET:
         {
            if (compiled_op)
            {
               arm_jit_prefetch<PROCNUM>(pc, opcode, thumb);
               compiled_op = false;
            }

            regman->flush_all();
            regman->reset();

            block->load_constant(0, (uint32_t)&armcpu_exec<PROCNUM>);
            call(0);
            block->add(RCYC, alu2::reg(0));

            has_ended = has_ended || instr_is_branch(thumb, opcode);

            break;
         }

         case OPR_BRANCHED:
         {
            has_ended = true;
            compiled_op = false;
            break;
         }

         case OPR_CONTINUE:
         {
            compiled_op = true;
            break;
         }
      }
   }

   if (compiled_op)
   {
      block->load_constant(0, pc);
      block->str(0, RCPU, mem2::imm(offsetof(armcpu_t, instruct_adr)));
   }

   write_status(3);

   regman->flush_all();
   regman->reset();

   block->load_constant(1, constant_cycles);
   block->add(0, 1, alu2::reg(RCYC));

   block->pop(0x8DF0);

   void* fn_ptr = block->fn_pointer();
   JIT_COMPILED_FUNC(base, PROCNUM) = (uintptr_t)fn_ptr;
   return (ArmOpCompiled)fn_ptr;
}


template<int PROCNUM> u32 arm_jit_compile()
{
   u32 adr = ARMPROC.instruct_adr;
   u32 mask_adr = (adr & 0x07FFFFFE) >> 4;
   if(((recompile_counts[mask_adr >> 1] >> 4*(mask_adr & 1)) & 0xF) > 8)
   {
      ArmOpCompiled f = op_decode[PROCNUM][ARMPROC.CPSR.bits.T];
      JIT_COMPILED_FUNC(adr, PROCNUM) = (uintptr_t)f;
      return f();
   }

   recompile_counts[mask_adr >> 1] += 1 << 4*(mask_adr & 1);

   if (block->instructions_remaining() < 1000)
   {
      arm_jit_reset(true);
   }

   return compile_basicblock<PROCNUM>()();
}

template u32 arm_jit_compile<0>();
template u32 arm_jit_compile<1>();

void arm_jit_reset(bool enable, bool suppress_msg)
{
   if (!suppress_msg)
	   printf("CPU mode: %s\n", enable?"JIT":"Interpreter");

   saveBlockSizeJIT = CommonSettings.jit_max_block_size;

   if (enable)
   {
      printf("JIT: max block size %d instruction(s)\n", CommonSettings.jit_max_block_size);

#ifdef MAPPED_JIT_FUNCS

      #define JITFREE(x) memset(x,0,sizeof(x));
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

      delete block;
      block = new code_pool(INSTRUCTION_COUNT);

      delete regman;
      regman = new register_manager(block);
   }
}

void arm_jit_close()
{
   delete block;
   block = 0;

   delete regman;
   regman = 0;
}
#endif // HAVE_JIT
