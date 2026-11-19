/*
	Differential harness: run the same instruction through the interpreter and
	through the local JIT, then compare the resulting CPU state.

	This file is NOT part of the production build. It is compiled and linked by
	tools/jit_matrix/build_diff.ps1 against build_core/desmume_core.lib.

	Why one instruction at a time: the unit of migration is a single instruction
	(see documentation/asmjit-jit-migration-plan.md §2). Comparing whole programs
	tells you "something differs"; comparing one instruction at one slot tells you
	which slot.

	The two engines are not separate entry points here -- armcpu_exec<P, jit>()
	is the same function the emulator uses, templated on the engine. That matters:
	conditional execution and cycle accounting live in that function, not in the
	instruction handlers, so going through it is the only way to avoid a harness
	that disagrees with the emulator by construction.
*/

#include "types.h"

#ifdef HAVE_JIT

#include <stdio.h>
#include <string.h>

#include "armcpu.h"
#include "MMU.h"
#include "NDSSystem.h"
#include "arm_jit.h"

// Where the probe instructions are planted. ARM9 main RAM starts at 0x02000000
// and is directly addressable with the MMU off, which is the state after
// NDS_Init().
static const u32 kCodeBase = 0x02000000;

// Distinct operand sets. The first five are the boundary values that ARM
// semantics turn on; the last three are patterned to catch byte/halfword bugs.
static const u32 kOperands[8] = {
	0x00000000u, 0x00000001u, 0x7FFFFFFFu, 0x80000000u,
	0xFFFFFFFFu, 0x0000FFFFu, 0xFF000000u, 0xDEADBEEFu,
};

struct CpuSnapshot
{
	u32 r[16];
	u32 cpsr;
	u32 cycles;
	u32 instruction;
	u32 instruct_adr;
	u32 next_instruction;
};

static void takeSnapshot(CpuSnapshot *s, u32 cycles)
{
	memcpy(s->r, NDS_ARM9.R, sizeof(s->r));
	s->cpsr = NDS_ARM9.CPSR.val;
	s->cycles = cycles;
	s->instruction = NDS_ARM9.instruction;
	s->instruct_adr = NDS_ARM9.instruct_adr;
	s->next_instruction = NDS_ARM9.next_instruction;
}

// A clean starting point for every vector: supervisor mode, ARM state, all
// conditions true (0xE), interrupts masked, and registers filled from the
// operand table so both engines see identical inputs.
//
// `opcode` must be stored into NDS_ARM9.instruction as well as into memory: the
// interpreter executes the *prefetched* word, while the JIT re-reads memory at
// instruct_adr. Leaving the prefetch at zero makes the interpreter execute
// ANDeq r0,r0,r0 (condition false, i.e. a no-op) and every comparison then
// looks like a JIT bug.
static void primeState(u32 codeAdr, const u32 *ops, u32 opcode)
{
	memset(NDS_ARM9.R, 0, sizeof(NDS_ARM9.R));
	NDS_ARM9.R[0] = ops[0];
	NDS_ARM9.R[1] = ops[1];
	NDS_ARM9.R[2] = ops[2];
	NDS_ARM9.R[3] = ops[3];
	NDS_ARM9.R[4] = ops[4];
	NDS_ARM9.R[5] = ops[5];
	NDS_ARM9.R[6] = ops[6];
	NDS_ARM9.R[7] = ops[7];

	// CPSR: mode = supervisor (0x13), T = 0 (ARM), F/I masked, condition = AL.
	NDS_ARM9.CPSR.val = 0x000000D3;

	NDS_ARM9.R[15] = codeAdr;
	NDS_ARM9.instruct_adr = codeAdr;
	NDS_ARM9.next_instruction = codeAdr;
	NDS_ARM9.instruction = opcode;
}

static bool sameSnapshot(const CpuSnapshot *a, const CpuSnapshot *b, char *why, size_t whyLen)
{
	for (int i = 0; i < 16; i++)
	{
		if (a->r[i] != b->r[i])
		{
			snprintf(why, whyLen, "R%d: interp=%08X jit=%08X", i, a->r[i], b->r[i]);
			return false;
		}
	}
	if (a->cpsr != b->cpsr)
	{
		snprintf(why, whyLen, "CPSR: interp=%08X jit=%08X", a->cpsr, b->cpsr);
		return false;
	}
	if (a->cycles != b->cycles)
	{
		snprintf(why, whyLen, "cycles: interp=%u jit=%u", a->cycles, b->cycles);
		return false;
	}
	if (a->next_instruction != b->next_instruction)
	{
		snprintf(why, whyLen, "next_instruction: interp=%08X jit=%08X",
			a->next_instruction, b->next_instruction);
		return false;
	}
	return true;
}

struct Probe
{
	const char *name;
	u32 opcode;
};

static const Probe kProbes[] = {
	{ "MOV  r0,r1",           0xE1A00001u },
	{ "ADD  r0,r1,r2",        0xE0810002u },
	{ "SUB  r0,r1,r2",        0xE0410002u },
	{ "AND  r0,r1,r2",        0xE0010002u },
	{ "ORR  r0,r1,r2",        0xE1810002u },
	{ "EOR  r0,r1,r2",        0xE0210002u },
	{ "CMP  r1,r2",           0xE1510002u },
	{ "MOV  r0,r1,LSL #4",    0xE1A00201u },
	{ "ADD  r0,r1,r2,ASR #2", 0xE0810142u },
	{ "MUL  r0,r1,r2",        0xE0000291u },
};

static int runProbes(void)
{
	const int probeCount = (int)(sizeof(kProbes) / sizeof(kProbes[0]));
	int failures = 0;

	for (int p = 0; p < probeCount; p++)
	{
		for (int v = 0; v < 2; v++)
		{
			// One unique address per (probe, vector) so the JIT never reuses a
			// block compiled for a different opcode.
			const u32 codeAdr = kCodeBase + (u32)((p * 2 + v) * 4);
			const u32 ops[8] = {
				kOperands[v], kOperands[(v + 1) & 7], kOperands[(v + 2) & 7], kOperands[(v + 3) & 7],
				kOperands[(v + 4) & 7], kOperands[(v + 5) & 7], kOperands[(v + 6) & 7], kOperands[(v + 7) & 7],
			};

			_MMU_write32<0>(codeAdr, kProbes[p].opcode);

			CpuSnapshot interp;
			primeState(codeAdr, ops, kProbes[p].opcode);
			takeSnapshot(&interp, 0);
			// Re-prime: takeSnapshot captured the primed state, and the run
			// mutates it.
			primeState(codeAdr, ops, kProbes[p].opcode);
			u32 c1 = armcpu_exec<0, false>();
			takeSnapshot(&interp, c1);

			_MMU_write32<0>(codeAdr, kProbes[p].opcode);

			CpuSnapshot jit;
			primeState(codeAdr, ops, kProbes[p].opcode);
			u32 c2 = armcpu_exec<0, true>();
			takeSnapshot(&jit, c2);

			char why[128] = {0};
			if (!sameSnapshot(&interp, &jit, why, sizeof(why)))
			{
				printf("FAIL  %-22s vec%d  %s\n", kProbes[p].name, v, why);
				failures++;
			}
		}
	}

	printf("\n%d probe(s) x 2 vectors: %d mismatch(es)\n", probeCount, failures);
	return failures;
}

int main(int argc, char **argv)
{
	printf("DeSmuME JIT differential harness\n");

	// NDS_Init() returns non-zero on success (it is a BOOL-style result, not an
	// errno), so 0 means failure.
	if (!NDS_Init())
	{
		printf("NDS_Init failed\n");
		return 2;
	}

	// One instruction per block: the harness compares single instructions, and a
	// multi-instruction block would make a mismatch ambiguous.
	CommonSettings.jit_max_block_size = 1;
	arm_jit_reset(true, true);

	int failures = runProbes();

	arm_jit_close();
	return failures == 0 ? 0 : 1;
}

#else
int main() { printf("built without HAVE_JIT\n"); return 2; }
#endif
