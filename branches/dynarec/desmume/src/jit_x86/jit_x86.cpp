/*  Copyright (C) 2007-2009 DeSmuME team

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

#include "../types.h"
#include "../armcpu.h"
#include "../NDSSystem.h"
#include "jit_x86.h"

// Number chosen due to space restriction in THUMB undefined opcodes.
// If you want to increase it, THUMB JitBlock instructions will need
// to occupy two THUMB opcodes.
#define MAX_BLOCKS 1024

#define MAKE_OP_ARM_JITBLOCK(num) (0xF6000010 | (((num) & 0x3FF) << 8))
#define MAKE_OP_THUMB_JITBLOCK(num) (0xB800 | ((num) & 0x3FF))

namespace JIT_x86
{

CCodeCache::CCodeCache()
{
	cache[0].index = 0;
	memset(cache[0].blocks, 0, sizeof(cache[0].blocks));
	cache[1].index = 0;
	memset(cache[1].blocks, 0, sizeof(cache[1].blocks));
}

CCodeCache::~CCodeCache() 
{
}


CCodeBlock* CCodeCache::FindCodeBlock(u32 cpu, u32 address, u32 thumb)
{
	//printf("look for code block: %08X %08X %08X\n", cpu, address, thumb);
	if (thumb == 0)
	{
		u32 first = MMU_read32(cpu, address);
		if ((first & 0xFFFC00FF) != 0xF6000010)
			return NULL;

		CCodeBlock* block = cache[cpu].blocks[(first >> 8) & 0x3FF];
	//	if (block->cpu != cpu)
	//		return NULL;

		return block;
	}
	else
	{
		u16 first = MMU_read16(cpu, address);
		if ((first & 0xFC00) != 0xB800)
			return NULL;

		CCodeBlock* block = cache[cpu].blocks[first & 0x3FF];
	//	if (block->cpu != cpu)
	//		return NULL;

		return block;
	}
#if 0
	u32 page = Address >> 12;

	if (cache[CPU][page] == NULL)
	{
		printf("JIT error: trying to find a code block into a non-code region (ARM%s %08X).\n"
			   "The program currently running must have crashed. Stopping execution.\n",
			   (CPU ? "7":"9"), Address);

		emu_halt();
		return NULL;
	}

	/*CCodeBlock* block =  cache[CPU][page][(Address & 0x00000FFF) >> 1];

	if (block == NULL)
		return NULL;

	if (block->address != Address)
		return NULL;

	if (block->mode != Mode)
		return NULL;

	return block;*/
	return cache[CPU][page][(Address & 0x00000FFF) >> 1];
#endif
}

void CCodeCache::AddCodeBlock(CCodeBlock* block)
{
	u32 cpu = block->cpu;
	u32 addr = block->address;
	//if (addr == 0x0208D064) printf("add code block %i %i\n", cache[cpu].index, cpu);

	if (block->thumb == 0)
	{
		block->origFirstInstruction = MMU_read32(cpu, addr);
		MMU_write32(cpu, addr, 0xF6000010 | (cache[cpu].index << 8));
	}
	else
	{
		block->origFirstInstruction = (u32)MMU_read16(cpu, addr);
		MMU_write16(cpu, addr, 0xB800 | cache[cpu].index);
	}

//	if (cache[cpu].index == 960)
	//	printf("add block 960! (%i)\n", cpu);

	if (cache[cpu].blocks[cache[cpu].index] != NULL)
	{
		RemoveCodeBlock(cache[cpu].blocks[cache[cpu].index], cache[cpu].index);
	}

	cache[cpu].blocks[cache[cpu].index] = block;
	cache[cpu].index ++;
	if (cache[cpu].index == 1024)
	{
		cache[cpu].index = 0;
		//RemoveCodeBlock(cache[cpu].blocks[0]);
	}
#if 0
	u32 page = Block->address >> 12;
	u32 offset = (Block->address & 0x00000FFF) >> 1;

	for (int i = 0; i < /*Block->num_ARM_instructions*/1; i++)
	{
	//	if (cache[Block->cpu][page][offset] != NULL)
	//		continue;

		cache[Block->cpu][page][offset] = Block;
		offset += (Block->mode ? 1:2);
	}
#endif
}

void CCodeCache::RemoveCodeBlock(CCodeBlock* block, int num)
{
	if (block == NULL)
		return;

	if (block->thumb == 0)
	{
		int _num = (MMU_read32(block->cpu, block->address) >> 8) & 0x3FF;
		//if (_num == 960) printf("remove block %i (%i %08X %08X)\n", _num, block->cpu, block->address, MMU_read32(block->cpu, block->address));
		//if (_num != num) printf("ALERTE ROUGE DANS LE DYNAREC!!! (%i != %i)\n", num, _num);
		if (_num == num)MMU_write32(block->cpu, block->address, block->origFirstInstruction);
		if (_num == num)cache[block->cpu].blocks[_num] = NULL;
	}
	else
	{
		int _num = MMU_read16(block->cpu, block->address) & 0x3FF;
		//if (_num == 960) printf("remove block %i\n", _num);
		if (_num == num)MMU_write16(block->cpu, block->address, (u16)block->origFirstInstruction);
		if (_num == num)cache[block->cpu].blocks[_num] = NULL;
	}

	delete block;
}


CCodeCache CodeCache;

bool CodeBlockStopped;


void BeginCodeBlock(CCodeBlock* block)
{
	CodeBlockStopped = false;
	Emitter::Begin(block->code);
}

void EndCodeBlock()
{
	Emitter::RET();
	Emitter::End();
}

void StopCodeBlock()
{
	CodeBlockStopped = true;
}

	namespace Emitter
	{

	u8* codePtr;

	//--- Begin / End -----------------------------------------------------------------

	INLINE void Begin(u8 *ptr)
	{
		codePtr = ptr;
	}

	INLINE void End()
	{
		codePtr = NULL;
	}

	//--- Emitters --------------------------------------------------------------------

	void Emit8(u8 value)
	{
		*codePtr = value;
		codePtr++;
	}

	void Emit16(u16 value)
	{
		*(u16*)codePtr = value;
		codePtr += 2;
	}

	void Emit32(u32 value)
	{
		*(u32*)codePtr = value;
		codePtr += 4;
	}

	//--- Condition check -------------------------------------------------------------
	//
	// Used in ARM mode and by THUMB conditional branch opcode
	// If js.ptr is NULL then condition check passed
	// Otherwise js is the source of a conditional jump that skips code to skip

	JumpSource EmitConditionCheck(armcpu_t* armcpu, u32 cc, u32 code)
	{
		JumpSource js;

		switch (cc)
		{
		case 0xE: // always
			js.ptr = NULL;
			break;

		case 0xF: // never (except, for ARM, if (CODE(instruction) == 0x5)
			// Note: maybe we should not bother generating code and jump if
			// condition is never? But I don't think any developer is crazy 
			// enough to add instructions which never execute :D
			{
				if (code == 0x5)
					js.ptr = NULL;
				else
					js = JMP(false);
			}
			break;

		case 0x0: // EQ
			{
				TEST(&(armcpu->CPSR.val), 0x40000000);
				js = Jcc(JZ, false);
			}
			break;

		case 0x1: // NE
			{
				TEST(&(armcpu->CPSR.val), 0x40000000);
				js = Jcc(JNZ, false);
			}
			break;

		case 0x2: // CS
			{
				TEST(&(armcpu->CPSR.val), 0x20000000);
				js = Jcc(JZ, false);
			}
			break;

		case 0x3: // CC
			{
				TEST(&(armcpu->CPSR.val), 0x20000000);
				js = Jcc(JNZ, false);
			}
			break;

		case 0x4: // MI
			{
				TEST(&(armcpu->CPSR.val), 0x80000000);
				js = Jcc(JZ, false);
			}
			break;

		case 0x5: // PL
			{
				TEST(&(armcpu->CPSR.val), 0x80000000);
				js = Jcc(JNZ, false);
			}
			break;

		case 0x6: // VS
			{
				TEST(&(armcpu->CPSR.val), 0x10000000);
				js = Jcc(JZ, false);
			}
			break;

		case 0x7: // VC
			{
				TEST(&(armcpu->CPSR.val), 0x10000000);
				js = Jcc(JNZ, false);
			}
			break;

		case 0x8: // HI
			{
				MOV(EAX, &(armcpu->CPSR.val));
				AND(EAX, 0x60000000);
				CMP(EAX, 0x20000000);
				js = Jcc(JNE, false);
			}
			break;

		case 0x9: // LS
			{
				MOV(EAX, &(armcpu->CPSR.val));
				AND(EAX, 0x60000000);
				CMP(EAX, 0x20000000);
				js = Jcc(JE, false);
			}
			break;

		case 0xA: // GE
			{
				MOV(EAX, &(armcpu->CPSR.val));
				AND(EAX, 0x90000000);
				JumpSource skipJump = Jcc(JZ, false);
				CMP(EAX, 0x90000000);
				js = Jcc(JNE, false);
				SetJumpTarget(skipJump);
			}
			break;

		case 0xB: // LT
			{
				MOV(EAX, &(armcpu->CPSR.val));
				AND(EAX, 0x90000000);
				CMP(EAX, 0x10000000);
				JumpSource skipJump = Jcc(JE, false);
				CMP(EAX, 0x80000000);
				js = Jcc(JNE, false);
				SetJumpTarget(skipJump);
			}
			break;

		case 0xC: // GT
			{
				MOV(EAX, &(armcpu->CPSR.val));
				AND(EAX, 0xD0000000);
				JumpSource skipJump = Jcc(JZ, false);
				CMP(EAX, 0x90000000);
				js = Jcc(JNE, false);
				SetJumpTarget(skipJump);
			}
			break;

		case 0xD: // LE
			{
				MOV(EAX, &(armcpu->CPSR.val));
				AND(EAX, 0xD0000000);
				TEST(EAX, 0x40000000);
				JumpSource skipJump1 = Jcc(JNZ, false);
				CMP(EAX, 0x10000000);
				JumpSource skipJump2 = Jcc(JE, false);
				CMP(EAX, 0x80000000);
				js = Jcc(JNE, false);
				SetJumpTarget(skipJump1);
				SetJumpTarget(skipJump2);
			}
			break;
		}

		return js;
	}

	//--- x86 instructions ------------------------------------------------------------
	//
	// x86 instruction format:
	//  __________________________________________________________________________
	// |             |        |             |          |              |           |
	// | Instruction | Opcode | ModR/M byte | SIB byte | Displacement | Immediate |
	// | prefixes    | bytes  | (if any)    | (if any) | (if any)     | (if any)  |
	// | (if any)    |        |             |          |              |           |
	// |_____________|________|_____________|__________|______________|___________|
	//
	//---------------------------------------------------------------------------------

	//--- ADD -------------------------------------------------------------------------

	void ADD_EAX(u32 imm32)
	{
		Emit8(0x05);
		Emit32(imm32);
	}

	void ADD(Register reg, u32 imm32)
	{
		Emit8(0x81);
		Emit8(0x05 | (reg << 3));
		Emit32(imm32);
	}

	//--- AND -------------------------------------------------------------------------

	void AND(Register reg, u32 imm32)
	{
		Emit8(0x81);
		Emit8(0xE0 | reg);
		Emit32(imm32);
	}

	//--- CMP -------------------------------------------------------------------------

	void CMP(Register reg, u32 imm32)
	{
		Emit8(0x81);
		Emit8(0xF8 | reg);
		Emit32(imm32);
	}

	//--- MOV -------------------------------------------------------------------------

	void MOV(Register reg, u32 imm32)
	{
		Emit8(0xB8 | reg);
		Emit32(imm32);
	}

	void MOV(Register reg, u32* mem32)
	{
		Emit8(0x8B);
		Emit8(0x05 | (reg << 3));
		Emit32((u32)mem32);
	}

	void MOV(u32* mem32, u32 imm32)
	{
		Emit8(0xC7);
		Emit8(0x05);
		Emit32((u32)mem32);
		Emit32(imm32);
	}

	//--- PUSH / POP ------------------------------------------------------------------

	void PUSH(Register reg)
	{
		Emit8(0x50 | reg);
	}

	void PUSH(u32 imm32)
	{
		Emit8(0x68);
		Emit32(imm32);
	}

	void PUSH(u32 *mem32)
	{
		Emit8(0xFF);
		Emit8(0x35);
		Emit32((u32)mem32);
	}

	//--- TEST ------------------------------------------------------------------------

	void TEST(Register reg, u32 imm32)
	{
		Emit8(0xF7);
		Emit8(0xC0 | reg);
		Emit32(imm32);
	}

	void TEST(u32* mem32, u32 imm32)
	{
		Emit8(0xF7);
		Emit8(0x05);
		Emit32((u32)mem32);
		Emit32(imm32);
	}

	//--- XOR -------------------------------------------------------------------------

	void XOR(Register reg, u32 imm32)
	{
		Emit8(0x81);
		Emit8(0xF0 | reg);
		Emit32(imm32);
	}

	//--- Branch ----------------------------------------------------------------------

	JumpSource JMP(bool force32Bit)
	{
		JumpSource js;

		if (force32Bit)
		{
			js.ptr = codePtr + 1;
			js.force32Bit = true;
			Emit8(0xE9);
			Emit32(0);
		}
		else
		{
			js.ptr = codePtr + 1;
			js.force32Bit = false;
			Emit8(0xEB);
			Emit8(0);
		}

		return js;
	}

	JumpSource Jcc(JumpConditionCode cc, bool force32Bit)
	{
		JumpSource js;

		if (force32Bit)
		{
			js.ptr = codePtr + 2;
			js.force32Bit = true;
			Emit8(0x0F);
			Emit8(0x80 | cc);
			Emit32(0);
		}
		else
		{
			js.ptr = codePtr + 1;
			js.force32Bit = false;
			Emit8(0x70 | cc);
			Emit8(0);
		}

		return js;
	}

	void SetJumpTarget(JumpSource js)
	{
		if (js.ptr == NULL)
			return;

		if (js.force32Bit)
		{
			((s32*)js.ptr)[0] = (s32)(codePtr - (js.ptr + 4));
		}
		else
		{
			((s8*)js.ptr)[0] = (s8)(codePtr - (js.ptr + 1));
		}
	}

	void CALL(void *target)
	{
		u64 distance = (u64)target - ((u64)codePtr + 5);
		if ((distance >= 0x0000000080000000ULL) && (distance < 0xFFFFFFFF80000000ULL))
		{
			printf("JIT error: CALL out of bounds!\n");
			Emit8(0xCC); // If the call was out of bounds, emit a breakpoint instead
			return;
		}

		Emit8(0xE8);
		Emit32((u32)distance);
	}

	void RET()
	{
		Emit8(0xC3);
	}

	} // end of namespace Emitter

} // end of namespace JIT_x86
