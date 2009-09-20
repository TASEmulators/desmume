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

#ifndef JIT_X86_H
#define JIT_X86_H

#define MAX_ARM_INSTRUCTIONS_PER_BLOCK 64

namespace JIT_x86
{

typedef u32(*TPrefetchFunc)();
typedef u32(__fastcall *TFallbackFunc)(u32);
typedef void(*TCompiledCode)();

class CCodeBlock
{
public:

	CCodeBlock(u32 _cpu, u32 _address, u32 _thumb)
		: cpu(_cpu)
		, address(_address)
		, thumb(_thumb)
		, num_ARM_instructions(0)
		, num_cycles(0)
	{
		//memset(code, 0xCC, sizeof(code));
	}

	u32 cpu;
	u32 address;
	u32 thumb;

	u32 num_ARM_instructions;
	u32 num_cycles;

	// Stores the first instruction of the block before it gets replaced
	// by a JitBlock opcode. Useful incases we meet a block containing
	// another smaller block.
	u32 origFirstInstruction;

	u8 code[MAX_ARM_INSTRUCTIONS_PER_BLOCK * 32 * 8];
};

//template<u32 PROCNUM>
class CCodeCache
{
public:

	CCodeCache();
	~CCodeCache();

	CCodeBlock* FindCodeBlock(u32 CPU, u32 Address, u32 Mode);
	void AddCodeBlock(CCodeBlock* block);
	void RemoveCodeBlock(CCodeBlock* block, int num);

	struct
	{
		CCodeBlock* blocks[1024];
		int index;
	} cache[2];
};

extern CCodeCache CodeCache;

extern bool CodeBlockStopped;


void BeginCodeBlock(CCodeBlock* block);
void EndCodeBlock();

void StopCodeBlock();


	namespace Emitter
	{

	//--- x86 registers ---------------------------------------------------------------

	enum Register
	{
		// 32-bit registers
		EAX = 0, ECX, EDX, EBX,
		ESP, EBP, ESI, EDI,

		// 16-bit registers
		AX = 0, CX, DX, BX,
		SP, BP, SI, DI,

		// 8-bit registers
		AL = 0, CL, DL, BL,
		AH, CH, DH, BH,
	};

	//--- Stuff for jumps ----------------------------------------------------------

	enum JumpConditionCode
	{
		// 0x00: Overflow (OF=1)
		JO = 0x00,

		// 0x01: Not overflow (OF=0)
		JNO = 0x01,

		// 0x02: Below (CF=1)
		JNAE = 0x02, JB = 0x02, JC = 0x02,

		// 0x03: Not below (CF=0)
		JAE = 0x03, JNB = 0x03, JNC = 0x03,

		// 0x04: Equal / Zero (ZF=1)
		JE = 0x04, JZ = 0x04,

		// 0x05: Not equal / Not zero (ZF=0)
		JNE = 0x05, JNZ = 0x05,

		// 0x06: Not above (CF=1 or ZF=1)
		JBE = 0x06, JNA = 0x06,

		// 0x07: Above (CF=0 and ZF=0)
		JNBE = 0x07, JA = 0x07,

		// 0x08: Sign (SF=1)
		JS = 0x08,

		// 0x09: Not sign (SF=0)
		JNS = 0x09,

		// 0x0A: Parity even (PF=1)
		JP = 0x0A, JPE = 0x0A,

		// 0x0B: Parity odd (PF=0)
		JNP = 0x0B, JPO = 0x0B,

		// 0x0C: Less (SF =/= OF)
		JL = 0x0C, JNGE = 0x0C,

		// 0x0D: Greater or equal (SF = OF)
		JNL = 0x0D, JGE = 0x0D,

		// 0x0E: Less or equal (ZF=1 or SF=/=OF)
		JLE = 0x0E, JNG = 0x0E,

		// 0x0F: Greater (ZF=0 and SF=OF)
		JNLE = 0x0F, JG = 0x0F,
	};

	typedef struct JumpSource
	{
		bool force32Bit;
		u8* ptr;
	} JumpSource;

	//---------------

	extern u8* codePtr;

	//--- Begin / End -----------------------------------------------------------------

	void Begin(u8 *ptr);
	void End();

	//--- Emitters --------------------------------------------------------------------

	void Emit8(u8 value);
	void Emit16(u16 value);
	void Emit32(u32 value);

	//--- Condition check -------------------------------------------------------------

	JumpSource EmitConditionCheck(armcpu_t* armcpu, u32 cc, u32 code);

	//--- ADD -------------------------------------------------------------------------

	void ADD(Register reg, u32 imm32);

	//--- AND -------------------------------------------------------------------------

	void AND(Register reg, u32 imm32);

	//--- CMP -------------------------------------------------------------------------

	void CMP(Register reg, u32 imm32);

	//--- MOV -------------------------------------------------------------------------

	void MOV(Register reg, u32 imm32);
	void MOV(Register reg, u32* mem32);
	void MOV(u32* mem32, u32 imm32);

	//--- PUSH / POP ------------------------------------------------------------------

	void PUSH(Register reg);
	void PUSH(u32 imm32);
	void PUSH(u32 *mem32);

	//--- TEST ------------------------------------------------------------------------

	void TEST(Register reg, u32 imm32);
	void TEST(u32* mem32, u32 imm32);

	//--- XOR -------------------------------------------------------------------------

	void XOR(Register reg, u32 imm32);

	//--- Branch ----------------------------------------------------------------------

	JumpSource JMP(bool force32Bit);
	JumpSource Jcc(JumpConditionCode cc, bool force32Bit);
	void SetJumpTarget(JumpSource js);

	void CALL(void *target);

	void RET();

	//--- Function calling ------------------------------------------------------------

	/*void CallFastcallFunc(void *func, ...)
	{
		va_list list;
		params

		va_start(
	}*/

	} // end of namespace Emitter

} // end of namespace JIT_x86

#endif
