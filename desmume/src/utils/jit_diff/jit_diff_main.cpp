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
static void primeState(u32 codeAdr, const u32 *ops, u32 opcode, u32 cpsr)
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
	NDS_ARM9.CPSR.val = cpsr;

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

// CPSR variants. For ARM the condition field is part of the dispatch index
// (opcode >> 20 includes bits 31..28), so sweeping slots already covers every
// condition *encoding*; these two make the flags differ, forcing each condition
// to be taken and not taken. Both are supervisor mode with interrupts masked:
// 0xD3 = all flags clear, 0xF3 = N/Z/C/V set.
static const u32 kFlagVariants[2] = { 0x000000D3u, 0x000000F3u };

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

			// Warm-up. The first execution of a given address pays a one-off
			// cost that has nothing to do with instruction semantics: measured
			// 52 cycles on the first interpreter run and 1 on the second, on
			// identical state. Both engines are therefore run once and thrown
			// away before either is measured -- the JIT needs it anyway, since
			// its first run is the one that compiles the block.
			primeState(codeAdr, ops, kProbes[p].opcode, kFlagVariants[0]);
			armcpu_exec<0, false>();
			primeState(codeAdr, ops, kProbes[p].opcode, kFlagVariants[0]);
			armcpu_exec<0, true>();

			CpuSnapshot interp;
			primeState(codeAdr, ops, kProbes[p].opcode, kFlagVariants[0]);
			u32 c1 = armcpu_exec<0, false>();
			takeSnapshot(&interp, c1);

			// Guard against a regression in the warm-up assumption above: if the
			// interpreter is still not repeatable, the cycle comparison is
			// meaningless and must say so instead of failing silently.
			primeState(codeAdr, ops, kProbes[p].opcode, kFlagVariants[0]);
			u32 c1b = armcpu_exec<0, false>();
			if (c1b != c1)
			{
				printf("FAIL  %-22s vec%d  interpreter not repeatable: %u then %u\n",
					kProbes[p].name, v, c1, c1b);
				failures++;
				continue;
			}

			_MMU_write32<0>(codeAdr, kProbes[p].opcode);

			CpuSnapshot jit;
			primeState(codeAdr, ops, kProbes[p].opcode, kFlagVariants[0]);
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

// Run one instruction through both engines on identical state and compare.
//
// The interpreter is run once and discarded first: its first execution of an
// address pays a one-off cost that has nothing to do with instruction semantics
// (measured 52 cycles, 1 on the second run of the same state).
//
// The JIT is deliberately NOT pre-run. Its first call takes the compile path
// (arm_jit_compile) and leaves a different R15 than a call into an
// already-compiled block; the first call is also what the emulator does when it
// first reaches a block, so that is the behaviour worth pinning down.
static bool compareOne(u32 codeAdr, const u32 *ops, u32 opcode, u32 cpsr,
	char *why, size_t whyLen)
{
	primeState(codeAdr, ops, opcode, cpsr);
	armcpu_exec<0, false>();

	CpuSnapshot interp;
	primeState(codeAdr, ops, opcode, cpsr);
	u32 c1 = armcpu_exec<0, false>();
	takeSnapshot(&interp, c1);

	primeState(codeAdr, ops, opcode, cpsr);
	u32 c1b = armcpu_exec<0, false>();
	if (c1b != c1)
	{
		snprintf(why, whyLen, "interpreter not repeatable: %u then %u", c1, c1b);
		return false;
	}

	CpuSnapshot jit;
	primeState(codeAdr, ops, opcode, cpsr);
	u32 c2 = armcpu_exec<0, true>();
	takeSnapshot(&jit, c2);

	return sameSnapshot(&interp, &jit, why, whyLen);
}
// Every dispatch slot of one instruction set.
//
// The slot index is exactly what the decoder uses -- 12 bits for ARM
// (opcode >> 20) and 10 bits for Thumb (opcode16 >> 6) -- so sweeping all slots
// also covers the undefined holes in the tables without parsing the tabdef
// files.
//
// `onlySlot` >= 0 restricts the run to one slot index in each set, which is how
// a crash gets bisected down to the instruction that caused it.
static int runSweep(bool full, int onlySlot, bool verbose)
{
	const int armSlots = 4096;
	const int thumbSlots = 1024;
	const int vectors = full ? 8 : 1;
	const int flagVariants = 2;
	int failures = 0;
	int checked = 0;
	int shown = 0;
	u32 codeAdr = kCodeBase;

	for (int set = 0; set < 2; set++)
	{
		const bool thumb = (set == 1);
		const int slots = thumb ? thumbSlots : armSlots;

		for (int slot = 0; slot < slots; slot++)
		{
			if (onlySlot >= 0 && slot != onlySlot)
				continue;

			const u32 opcode = thumb
				? ((u32)slot << 6)
				: ((u32)slot << 20);

			if (verbose && (slot & 0x7F) == 0)
			{
				printf("  %s slot %03X/%03X\n", thumb ? "thumb" : "arm  ", slot, slots);
				fflush(stdout);
			}

			for (int v = 0; v < vectors; v++)
			{
				const u32 ops[8] = {
					kOperands[v & 7], kOperands[(v + 1) & 7], kOperands[(v + 2) & 7], kOperands[(v + 3) & 7],
					kOperands[(v + 4) & 7], kOperands[(v + 5) & 7], kOperands[(v + 6) & 7], kOperands[(v + 7) & 7],
				};

				if (thumb)
					_MMU_write16<0>(codeAdr, (u16)opcode);
				else
					_MMU_write32<0>(codeAdr, opcode);

				for (int f = 0; f < flagVariants; f++)
				{
					char why[128] = {0};
					checked++;
					if (!compareOne(codeAdr, ops, opcode, kFlagVariants[f] | (thumb ? 0x20u : 0u), why, sizeof(why)))
					{
						failures++;
						if (shown < 40)
						{
							printf("FAIL  %s slot=%03X v=%d flags=%d op=%08X  %s\n",
								thumb ? "thumb" : "arm  ", slot, v, f, opcode, why);
							fflush(stdout);
							shown++;
						}
					}
				}

				codeAdr += 4;
				if (codeAdr > kCodeBase + 0x00040000u)   // 256 KB of probe space
					codeAdr = kCodeBase;
			}

			// The JIT hands out code from a fixed scope; recycle it periodically
			// so a full sweep cannot exhaust it.
			if ((slot & 0xFF) == 0xFF)
				arm_jit_reset(true, true);
		}
	}

	printf("\n%s sweep: %d comparison(s), %d mismatch(es)\n",
		full ? "full" : "fast", checked, failures);
	if (failures > shown)
		printf("(%d more not shown)\n", failures - shown);
	return failures;
}

int main(int argc, char **argv)
{
	bool sweep = false;
	bool full = false;
	bool verbose = false;
	int onlySlot = -1;
	for (int i = 1; i < argc; i++)
	{
		if (strcmp(argv[i], "--sweep") == 0) sweep = true;
		else if (strcmp(argv[i], "--full") == 0) { full = true; sweep = true; }
		else if (strcmp(argv[i], "--verbose") == 0) verbose = true;
		else if (strcmp(argv[i], "--slot") == 0 && i + 1 < argc) onlySlot = atoi(argv[++i]);
	}

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

	int failures = sweep ? runSweep(full, onlySlot, verbose) : runProbes();

	arm_jit_close();
	return failures == 0 ? 0 : 1;
}

#else
int main() { printf("built without HAVE_JIT\n"); return 2; }
#endif
