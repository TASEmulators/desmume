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

#if defined(__APPLE__)
	#include <mach-o/dyld.h>
#endif

#include "armcpu.h"
#include "MMU.h"
#include "NDSSystem.h"
#include "arm_jit.h"
#include "instructions.h"

// Where the probe instructions are planted. ARM9 main RAM starts at 0x02000000
// and is directly addressable with the MMU off, which is the state after
// NDS_Init().
static const u32 kCodeBase = 0x02000000;

// Top of ARM9 main RAM (4 MB at 0x02000000), used as the initial stack pointer.
static const u32 kStackTop = 0x02400000;

// Distinct operand sets. The first five are the boundary values that ARM
// semantics turn on; the last three are patterned to catch byte/halfword bugs.
static const u32 kOperands[8] = {
	0x00000000u, 0x00000001u, 0x7FFFFFFFu, 0x80000000u,
	0xFFFFFFFFu, 0x0000FFFFu, 0xFF000000u, 0xDEADBEEFu,
};

// Set by --verbose. When a comparison is about to crash, the last line printed
// names the exact engine call, which is the only way to tell "the JIT produced
// bad code" from "the harness primed an illegal state".
static bool gTrace = false;

// Counts comparisons where the first call into a compiled block produced a
// different architectural result than a second call on identical state. Not a
// failure by itself: the verdict uses the first call, and a difference means the
// compile path and the steady-state block disagree -- a bug either way, but a
// different one from "the block computes the wrong answer".
static int gJitUnstable = 0;

static const char *mnemonic(bool thumb, u32 opcode);

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

	// R13 must point at real memory: the JIT writes to the stack on some paths
	// (measured as an access violation when SP was left at 0), and an emulated
	// instruction may legitimately push. Keep it inside ARM9 main RAM.
	NDS_ARM9.R[13] = kStackTop;

	// R15 must be primed to the value the pipeline would have left there, not to
	// the address of the instruction. armcpu_prefetch() establishes
	// R15 = instruct_adr + 2*opcodesize before the handler runs, and handlers
	// that branch rely on it: priming R15 = codeAdr made `B #0` compute a target
	// of codeAdr instead of codeAdr+8, which looks exactly like a JIT bug.
	const u32 insnSize = (cpsr & 0x20u) ? 2u : 4u;
	NDS_ARM9.R[15] = codeAdr + 2 * insnSize;
	NDS_ARM9.instruct_adr = codeAdr;
	// Next_instruction is the *following* instruction: the interpreter prefetches
	// ahead by one slot, and priming it to codeAdr makes the post-execute
	// prefetch land back on the instruction just executed.
	NDS_ARM9.next_instruction = codeAdr + insnSize;
	NDS_ARM9.instruction = opcode;
}

// What is compared, and what deliberately is not.
//
// R0-R14, CPSR, cycles and instruct_adr are the contract both engines keep, so
// they are always compared.
//
// R15 and next_instruction are only partly maintained by the JIT, by design:
//   * sync_r15() writes next_instruction only for a block-final instruction that
//     does not redirect PC; for a branch end-of-block it writes instruct_adr and
//     leaves next_instruction alone (arm_jit.cpp:3995, 4010).
//   * R15 is written only when the instruction reads PC, as a compile-time
//     constant (bb_r15, arm_jit.cpp:4005). A compiled block never otherwise
//     maintains R[15]; between blocks it holds whatever was there before.
// Neither is read back by the emulator: armcpu_exec<P,true>() resumes from
// instruct_adr. So comparing them unconditionally reports every branch as a
// failure and, for everything else, only echoes the primed value back.
//
// next_instruction is not compared. The two engines use it with a one-slot
// difference that is internal bookkeeping rather than architectural state:
// after one instruction the interpreter has already prefetched the following
// one (next_instruction = instruct_adr + size), while a compiled block leaves
// both pointing at the next instruction (arm_jit.cpp:4003 copies
// bb_next_instruction into next_instruction, then the block tail copies it into
// instruct_adr). instruct_adr is the resume point for both, so it is compared
// and it subsumes the other.
//
// R15 is never compared; the interpreter's own invariant is asserted instead,
// which is what would catch the harness priming an illegal state. The invariant
// only applies when the instruction fell through: SWI/BX/BLX/POP{pc} re-enter
// the pipeline at a vector or branch target and set R15 themselves.
static bool sameSnapshot(const CpuSnapshot *a, const CpuSnapshot *b,
	u32 codeAdr, u32 insnSize, char *why, size_t whyLen)
{
	for (int i = 0; i < 15; i++)
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
	if (a->instruct_adr != b->instruct_adr)
	{
		snprintf(why, whyLen, "instruct_adr: interp=%08X jit=%08X", a->instruct_adr, b->instruct_adr);
		return false;
	}

	// Harness self-check: on a fall-through the interpreter must hold
	// R15 = next_instruction + opcodesize. If that does not hold, either the state
	// was primed illegally or the interpreter's pipeline model changed; either way
	// the comparison above is not meaningful.
	const bool fellThrough = (a->instruct_adr == codeAdr + insnSize);
	if (fellThrough && a->r[15] != a->next_instruction + insnSize)
	{
		snprintf(why, whyLen, "interpreter invariant broken: R15=%08X next=%08X size=%u",
			a->r[15], a->next_instruction, insnSize);
		return false;
	}

	return true;
}

static void traceStep(const char *what, u32 codeAdr, u32 cpsr)
{
	if (!gTrace)
		return;
	printf("      step %-16s adr=%08X cpsr=%08X R13=%08X\n", what, codeAdr, cpsr, NDS_ARM9.R[13]);
	fflush(stdout);
}

// A sweep that dies on the first faulting slot is worth very little: an access
// violation can come from an uninitialised front-end pointer, from a handler
// path the emulator never takes with a real game, or from a genuine bug -- and
// you cannot tell which without seeing all of them. So the two engine calls are
// wrapped in a structured-exception boundary and recorded per slot.
//
// This does not make a fault harmless: the machine state afterwards is
// undefined, so the comparison is reported as a failure and the next vector
// re-primes from scratch.
#ifdef _MSC_VER
#define HARNESS_HAVE_SEH 1
static unsigned long gFaultCode = 0;
static uintptr_t gFaultAddr = 0;
static uintptr_t gFaultAccess = 0;
static uintptr_t gFaultOp = 0;

#define HARNESS_EXCEPT_FILTER \
	((gFaultCode = GetExceptionCode()), \
	 (gFaultAddr = (uintptr_t)GetExceptionInformation()->ExceptionRecord->ExceptionAddress), \
	 (gFaultOp = (GetExceptionInformation()->ExceptionRecord->NumberParameters > 0) \
	     ? GetExceptionInformation()->ExceptionRecord->ExceptionInformation[0] : 0), \
	 (gFaultAccess = (GetExceptionInformation()->ExceptionRecord->NumberParameters > 1) \
	     ? GetExceptionInformation()->ExceptionRecord->ExceptionInformation[1] : 0), \
	 EXCEPTION_EXECUTE_HANDLER)
#elif defined(__APPLE__) || defined(__linux__)
// Same idea on a POSIX host, where there is no SEH: a signal handler records the
// fault and siglongjmp()s back to the caller. This is what makes the arm64 side
// measurable at all -- a hand-written encoder that emits a broken instruction
// faults on the first slot that reaches it, and without this the whole sweep
// would die there instead of reporting the slot.
//
// Longjmping out of a signal handler is only safe because the only state that
// survives is the CPU snapshot, which is re-primed from scratch for every
// vector. The handler stays armed only across the engine call itself.
#define HARNESS_HAVE_POSIX_GUARD 1
#include <signal.h>
#include <setjmp.h>

static unsigned long gFaultCode = 0;
static uintptr_t gFaultAddr = 0;
static uintptr_t gFaultAccess = 0;
static unsigned long gFaultOp = 0;
static sigjmp_buf gFaultJump;
static volatile sig_atomic_t gFaultArmed = 0;

static void harnessFaultHandler(int sig, siginfo_t *info, void *ctx)
{
	(void)ctx;
	if (!gFaultArmed)
	{
		// Not inside a guarded engine call: this is a real crash, so restore the
		// default action instead of letting the faulting instruction re-execute.
		signal(sig, SIG_DFL);
		raise(sig);
		return;
	}
	gFaultArmed = 0;
	gFaultCode = (unsigned long)sig;
	gFaultAddr = (uintptr_t)info->si_addr;
	gFaultAccess = (uintptr_t)info->si_addr;
	gFaultOp = 0;
	siglongjmp(gFaultJump, 1);
}

static void harnessInstallFaultHandlers(void)
{
	struct sigaction sa;
	memset(&sa, 0, sizeof(sa));
	sa.sa_sigaction = harnessFaultHandler;
	sa.sa_flags = SA_SIGINFO | SA_NODEFER;
	sigemptyset(&sa.sa_mask);
	sigaction(SIGSEGV, &sa, NULL);
	sigaction(SIGBUS, &sa, NULL);
	sigaction(SIGILL, &sa, NULL);
	sigaction(SIGFPE, &sa, NULL);
}

static const char *harnessSignalName(unsigned long sig)
{
	switch ((int)sig)
	{
		case SIGSEGV: return "SIGSEGV";
		case SIGBUS:  return "SIGBUS";
		case SIGILL:  return "SIGILL";
		case SIGFPE:  return "SIGFPE";
		default:      return "signal";
	}
}
#endif

static u32 execInterp(void)
{
#if defined(HARNESS_HAVE_SEH)
	u32 cycles = 0;
	gFaultCode = 0;
	gFaultAddr = 0;
	gFaultAccess = 0;
	gFaultOp = 0;
	__try { cycles = armcpu_exec<0, false>(); }
	__except (HARNESS_EXCEPT_FILTER) { }
	return cycles;
#elif defined(HARNESS_HAVE_POSIX_GUARD)
	u32 cycles = 0;
	gFaultCode = 0;
	if (sigsetjmp(gFaultJump, 1) == 0)
	{
		gFaultArmed = 1;
		cycles = armcpu_exec<0, false>();
		gFaultArmed = 0;
	}
	return cycles;
#else
	return armcpu_exec<0, false>();
#endif
}

static u32 execJit(void)
{
#if defined(HARNESS_HAVE_SEH)
	u32 cycles = 0;
	gFaultCode = 0;
	gFaultAddr = 0;
	gFaultAccess = 0;
	gFaultOp = 0;
	__try { cycles = armcpu_exec<0, true>(); }
	__except (HARNESS_EXCEPT_FILTER) { }
	return cycles;
#elif defined(HARNESS_HAVE_POSIX_GUARD)
	u32 cycles = 0;
	gFaultCode = 0;
	if (sigsetjmp(gFaultJump, 1) == 0)
	{
		gFaultArmed = 1;
		cycles = armcpu_exec<0, true>();
		gFaultArmed = 0;
	}
	return cycles;
#else
	return armcpu_exec<0, true>();
#endif
}

static bool faulted(char *why, size_t whyLen, const char *engine)
{
#if defined(HARNESS_HAVE_SEH)
	if (gFaultCode != 0)
	{
		snprintf(why, whyLen, "%s: exception %08lX at %p op=%lu access=%p",
			engine, gFaultCode, (void *)gFaultAddr, (unsigned long)gFaultOp, (void *)gFaultAccess);
		return true;
	}
#elif defined(HARNESS_HAVE_POSIX_GUARD)
	if (gFaultCode != 0)
	{
		snprintf(why, whyLen, "%s: %s at %p",
			engine, harnessSignalName(gFaultCode), (void *)gFaultAddr);
		return true;
	}
#endif
	(void)why; (void)whyLen; (void)engine;
	return false;
}

// Run one instruction through both engines on identical state and compare.
//
// The interpreter is run once and discarded first: its first execution of an
// address pays a one-off cost that has nothing to do with instruction semantics
// (measured 52 cycles, 1 on the second run of the same state).
//
// The JIT is called twice and the second result is the one compared. The first
// call at an address only compiles the block -- compile_basicblock() executes
// the instruction through op_decode[] while emitting and returns that cycle
// count -- so taking the first call would compare the interpreter with itself
// and hide every defect in the emitted code.
static bool compareOne(u32 codeAdr, const u32 *ops, u32 opcode, u32 cpsr,
	char *why, size_t whyLen, u32 *jitCycles)
{
	traceStep("prime-interp-warm", codeAdr, cpsr);
	primeState(codeAdr, ops, opcode, cpsr);
	execInterp();
	if (faulted(why, whyLen, "interpreter (warm-up)"))
		return false;
	traceStep("ret-interp-warm", codeAdr, cpsr);

	CpuSnapshot interp;
	traceStep("prime+interp-run", codeAdr, cpsr);
	primeState(codeAdr, ops, opcode, cpsr);
	u32 c1 = execInterp();
	if (faulted(why, whyLen, "interpreter"))
		return false;
	takeSnapshot(&interp, c1);

	traceStep("prime+interp-again", codeAdr, cpsr);
	primeState(codeAdr, ops, opcode, cpsr);
	u32 c1b = execInterp();
	if (faulted(why, whyLen, "interpreter (repeat)"))
		return false;
	if (c1b != c1)
	{
		snprintf(why, whyLen, "interpreter not repeatable: %u then %u", c1, c1b);
		return false;
	}

	CpuSnapshot jit;
	traceStep("prime+jit-compile", codeAdr, cpsr);
	primeState(codeAdr, ops, opcode, cpsr);
	u32 c2c = execJit();
	if (faulted(why, whyLen, "jit (compile)"))
		return false;
	(void)c2c;

	// The first call into an uncompiled address is NOT the JIT running the
	// instruction: compile_basicblock() emits the block and then executes the
	// instruction through op_decode[] at compile time, returning that cycle
	// count (arm_jit.cpp:4252). Only the second call jumps into the emitted
	// code. Comparing against the first call would therefore test the
	// interpreter against itself and hide every JIT defect; the first call is
	// still run, because it is what builds the block.
	traceStep("prime+jit-run", codeAdr, cpsr);
	primeState(codeAdr, ops, opcode, cpsr);
	u32 c2 = execJit();
	if (faulted(why, whyLen, "jit"))
		return false;
	takeSnapshot(&jit, c2);
	*jitCycles = c2;

	// A compiled block must produce the same architectural result on every call.
	// This is a diagnostic, not a failure: the verdict below uses the call above.
	CpuSnapshot jit2;
	primeState(codeAdr, ops, opcode, cpsr);
	u32 c2b = execJit();
	takeSnapshot(&jit2, c2b);
	if (memcmp(&jit, &jit2, sizeof(jit)) != 0 || c2 != c2b)
	{
		if (gJitUnstable < 8)
		{
			printf("      note  jit not repeatable: %s slot/addr=%08X\n", "-", codeAdr);
			printf("            first : cpsr=%08X R0=%08X cyc=%u\n", jit.cpsr, jit.r[0], c2);
			printf("            second: cpsr=%08X R0=%08X cyc=%u\n", jit2.cpsr, jit2.r[0], c2b);
			fflush(stdout);
		}
		gJitUnstable++;
	}

	if (gTrace)
	{
		printf("      interp pc=%08X nxt=%08X R15=%08X | jit pc=%08X nxt=%08X R15=%08X\n",
			interp.instruct_adr, interp.next_instruction, interp.r[15],
			jit.instruct_adr, jit.next_instruction, jit.r[15]);
		printf("      interp R0-R3=%08X %08X %08X %08X cpsr=%08X\n",
			interp.r[0], interp.r[1], interp.r[2], interp.r[3], interp.cpsr);
		printf("      jit    R0-R3=%08X %08X %08X %08X cpsr=%08X\n",
			jit.r[0], jit.r[1], jit.r[2], jit.r[3], jit.cpsr);
		fflush(stdout);
	}

	return sameSnapshot(&interp, &jit, codeAdr, (cpsr & 0x20u) ? 2u : 4u, why, whyLen);
}

struct Probe
{
	const char *name;
	u32 opcode;
	bool thumb;
};

// Targeted probes, on top of the slot sweep. The sweep's opcodes all have zero
// low bits, so a register-shift instruction always ends up with Rs == Rd == r0;
// these pairs deliberately separate the two so an operand-ordering bug cannot
// hide behind it.
static const Probe kProbes[] = {
	{ "ARM   MOV  r0,r1",        0xE1A00001u, false },
	{ "ARM   ADD  r0,r1,r2",     0xE0810002u, false },
	{ "ARM   SUB  r0,r1,r2",     0xE0410002u, false },
	{ "ARM   AND  r0,r1,r2",     0xE0010002u, false },
	{ "ARM   ORR  r0,r1,r2",     0xE1810002u, false },
	{ "ARM   EOR  r0,r1,r2",     0xE0210002u, false },
	{ "ARM   CMP  r1,r2",        0xE1510002u, false },
	{ "ARM   MOV  r0,r1,LSL#4",  0xE1A00201u, false },
	{ "ARM   ADD  r0,r1,r2,ASR2",0xE0810142u, false },
	{ "ARM   MUL  r0,r1,r2",     0xE0000291u, false },
	// Thumb format 4, register shifts: (Rs == Rd) then (Rs != Rd).
	{ "THUMB LSL  r0,r0",        0x4080u, true },
	{ "THUMB LSL  r0,r1",        0x4088u, true },
	{ "THUMB LSR  r0,r0",        0x40C0u, true },
	{ "THUMB LSR  r0,r1",        0x40C8u, true },
	{ "THUMB ASR  r0,r0",        0x4100u, true },
	{ "THUMB ASR  r0,r1",        0x4108u, true },
	{ "THUMB ROR  r0,r0",        0x41C0u, true },
	{ "THUMB ROR  r0,r1",        0x41C8u, true },
};

// CPSR variants. For ARM the condition field is part of the dispatch index
// (opcode >> 20 includes bits 31..28), so sweeping slots already covers every
// condition *encoding*; these two make the flags differ, forcing each condition
// to be taken and not taken.
//
// Both are supervisor mode (0x13) with IRQ/FIQ masked (0xD0). The upper byte is
// where N/Z/C/V live: 0x00 = all clear, 0xF0 = all set.
//
// Note where the flags do NOT live: bits 7..4 are I, F, T and the top mode bit.
// A variant written as 0x000000F3 (the obvious-looking mistake) sets T, which
// puts the CPU in Thumb state; the ARM sweep then dispatches through
// thumb_instructions_set with index opcode>>6 == 0x4000 and dies on an access
// violation. kFlagVariants is checked for this at start-up.
static const u32 kFlagVariants[2] = { 0x000000D3u, 0xF00000D3u };

static bool compareOne(u32 codeAdr, const u32 *ops, u32 opcode, u32 cpsr,
	char *why, size_t whyLen, u32 *jitCycles);

// Probes share compareOne() with the sweep: one comparison path means a probe
// cannot accidentally be a weaker test than a sweep line.
static int runProbes(void)
{
	const int probeCount = (int)(sizeof(kProbes) / sizeof(kProbes[0]));
	const int vectors = 8;
	int failures = 0;
	u32 codeAdr = kCodeBase;

	for (int p = 0; p < probeCount; p++)
	{
		for (int v = 0; v < vectors; v++)
		{
			const u32 ops[8] = {
				kOperands[v], kOperands[(v + 1) & 7], kOperands[(v + 2) & 7], kOperands[(v + 3) & 7],
				kOperands[(v + 4) & 7], kOperands[(v + 5) & 7], kOperands[(v + 6) & 7], kOperands[(v + 7) & 7],
			};

			if (kProbes[p].thumb)
				_MMU_write16<0>(codeAdr, (u16)kProbes[p].opcode);
			else
				_MMU_write32<0>(codeAdr, kProbes[p].opcode);

			// Both flag variants: a flag-setting instruction can agree with all
			// flags clear and disagree with them set (the ROR r0,r0 case only
			// shows up with N/Z/C/V set).
			for (int f = 0; f < 2; f++)
			{
				const u32 cpsr = kFlagVariants[f] | (kProbes[p].thumb ? 0x20u : 0u);

				char why[128] = {0};
				u32 jitCycles = 0;
				if (!compareOne(codeAdr, ops, kProbes[p].opcode, cpsr, why, sizeof(why), &jitCycles))
				{
					printf("FAIL  %-24s vec%d f=%d  %s\n", kProbes[p].name, v, f, why);
					fflush(stdout);
					failures++;
				}
			}

			// One unique address per (probe, vector) so the JIT never reuses a
			// block compiled for a different opcode.
			codeAdr += 4;
		}
	}

	printf("\n%d probe(s) x %d vectors x 2 flags: %d mismatch(es)\n", probeCount, vectors, failures);
	return failures;
}
// The emulator's own mnemonic tables, so a report line names the same
// instruction the worklist in documentation/jit-opcode-matrix.md does.
static const char *mnemonic(bool thumb, u32 opcode)
{
	return thumb ? thumb_instruction_names[opcode >> 6]
	             : arm_instruction_names[opcode >> 20];
}

// Mismatch classification. "2127 mismatches" is not actionable on its own: two
// thousand cycle differences caused by one encoder decision and three register
// differences are very different problems. The buckets are derived from the
// message sameSnapshot()/compareOne() produced, so they cannot drift from it.
struct FailureBuckets
{
	int exceptions;
	int cycles;
	int cyclesGarbage;
	int registers;
	int cpsr;
	int pc;
	int invariant;
	int repeatable;

	void add(const char *why, u32 jitCycles)
	{
		if (strstr(why, "exception")) exceptions++;
		else if (strstr(why, "not repeatable")) repeatable++;
		else if (strstr(why, "invariant")) invariant++;
		else if (strncmp(why, "cycles:", 7) == 0) { if (jitCycles > 4096) cyclesGarbage++; else cycles++; }
		else if (why[0] == 'R' && why[1] >= '0' && why[1] <= '9') registers++;
		else if (strncmp(why, "CPSR:", 5) == 0) cpsr++;
		else if (strncmp(why, "instruct_adr:", 13) == 0) pc++;
		else exceptions++;   // unrecognised message: do not silently drop it
	}

	void print(void) const
	{
		printf("  exceptions (engine fault) : %d\n", exceptions);
		printf("  cycle count               : %d\n", cycles);
		printf("  cycle count (implausible) : %d\n", cyclesGarbage);
		printf("  R0-R14                    : %d\n", registers);
		printf("  CPSR                      : %d\n", cpsr);
		printf("  instruct_adr              : %d\n", pc);
		printf("  harness invariant         : %d\n", invariant);
		printf("  interpreter not repeatable: %d\n", repeatable);
	}
};

// Every dispatch slot of one instruction set.
//
// The slot index is exactly what the decoder uses -- 12 bits for ARM
// (opcode >> 20) and 10 bits for Thumb (opcode16 >> 6) -- so sweeping all slots
// also covers the undefined holes in the tables without parsing the tabdef
// files.
//
// `onlySlot` >= 0 restricts the run to one slot index in each set, which is how
// a crash gets bisected down to the instruction that caused it.
static int runSweep(bool full, int onlySlot, bool verbose, bool listAll)
{
	const int armSlots = 4096;
	const int thumbSlots = 1024;
	const int vectors = full ? 8 : 1;
	const int flagVariants = 2;
	int failures = 0;
	int checked = 0;
	int shown = 0;
	const int showCap = listAll ? 0x7FFFFFFF : 40;
	FailureBuckets buckets = {0, 0, 0, 0, 0, 0, 0, 0};
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

			// The interpreter indexes thumb_instructions_set with
			// (instruction >> 6) and does not mask it, so a Thumb opcode wider than
			// 16 bits reads past the 1024-entry table instead of failing loudly.
			if (thumb && (opcode >> 6) >= (u32)thumbSlots)
			{
				printf("bad harness configuration: thumb opcode %08X out of range\n", opcode);
				return -1;
			}

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
					if (verbose)
					{
						printf("  try %s slot=%03X v=%d f=%d adr=%08X op=%08X %s\n", thumb ? "thumb" : "arm  ", slot, v, f, codeAdr, opcode, mnemonic(thumb, opcode));
						fflush(stdout);
					}
					u32 jitCycles = 0;
					if (!compareOne(codeAdr, ops, opcode, kFlagVariants[f] | (thumb ? 0x20u : 0u), why, sizeof(why), &jitCycles))
					{
						failures++;
						buckets.add(why, jitCycles);
						if (shown < showCap)
						{
							// The operand vector is printed with the failure: a flag or
							// cycle mismatch is only diagnosable if you know the inputs
							// that produced it, and re-deriving them from v/op by hand is
							// how a two-minute investigation turns into an hour.
							printf("FAIL  %s slot=%03X v=%d flags=%d op=%08X %s  %s\n",
								thumb ? "thumb" : "arm  ", slot, v, f, opcode, mnemonic(thumb, opcode), why);
							printf("      R0-R7 = %08X %08X %08X %08X %08X %08X %08X %08X\n",
								ops[0], ops[1], ops[2], ops[3], ops[4], ops[5], ops[6], ops[7]);
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
	if (failures)
		buckets.print();
	if (failures > shown)
		printf("(%d more not shown)\n", failures - shown);
	return failures;
}

// Base address the map file's RVAs are relative to. Only used to turn a fault
// address into something a debugger can name, so a NULL here costs information,
// not correctness.
static void *harnessModuleBase(void)
{
#ifdef _WIN32
	return (void *)GetModuleHandleA(NULL);
#elif defined(__APPLE__)
	// Image 0 is the executable; dyld reports its slide, not its link-time base.
	return (void *)(uintptr_t)_dyld_get_image_vmaddr_slide(0);
#else
	return NULL;
#endif
}

int main(int argc, char **argv)
{
	bool sweep = false;
	bool full = false;
	bool verbose = false;
	bool listAll = false;
	int onlySlot = -1;
	u32 dumpBlockAdr = 0;
	for (int i = 1; i < argc; i++)
	{
		if (strcmp(argv[i], "--sweep") == 0) sweep = true;
		else if (strcmp(argv[i], "--full") == 0) { full = true; sweep = true; }
		else if (strcmp(argv[i], "--verbose") == 0) verbose = true;
		else if (strcmp(argv[i], "--all") == 0) listAll = true;
		else if (strcmp(argv[i], "--slot") == 0 && i + 1 < argc) onlySlot = atoi(argv[++i]);
		else if (strcmp(argv[i], "--dump") == 0 && i + 1 < argc)
			dumpBlockAdr = (u32)strtoul(argv[++i], NULL, 16);
	}

	printf("DeSmuME JIT differential harness\n");
	// Fault addresses are absolute; without the load address the map file cannot
	// be used to name the function that faulted.
	printf("module base %p\n", harnessModuleBase());
	gTrace = verbose;

#ifdef HARNESS_HAVE_POSIX_GUARD
	harnessInstallFaultHandlers();
#endif

	// The flag variants must differ only in N/Z/C/V. T and the mode field are
	// chosen per instruction set by compareOne; a variant that also touches them
	// silently switches instruction sets and turns the sweep into an out-of-range
	// dispatch (see the comment on kFlagVariants).
	for (int f = 0; f < 2; f++)
	{
		if (kFlagVariants[f] & 0x00000020u)
		{
			printf("bad harness configuration: kFlagVariants[%d] sets the T bit\n", f);
			return 2;
		}
		if ((kFlagVariants[f] & 0x0000001Fu) != 0x00000013u)
		{
			printf("bad harness configuration: kFlagVariants[%d] is not supervisor mode\n", f);
			return 2;
		}
	}

	// NDS_Init() returns 0 on success, -1 when SPU_Init() fails. Checking for
	// "non-zero" rather than "truthy" matters: with -1 both are the same thing,
	// but a truthiness test reads as if 1 meant success, which it does not.
	const int initResult = NDS_Init();
	if (initResult != 0)
	{
		printf("NDS_Init failed (%d)\n", initResult);
		return 2;
	}

	// NDS_Init() only allocates and wires the memory interfaces; NDS_Reset() is
	// what puts the CPUs, CP15 and the coprocessor hooks into a defined state.
	// It is normally reached through NDS_LoadROM(), which the harness has no ROM
	// for -- skipping it leaves the coprocessor path uninitialised and every
	// CDP/MCR/MRC slot faults with the same access violation.
	NDS_Reset();

	// One instruction per block: the harness compares single instructions, and a
	// multi-instruction block would make a mismatch ambiguous.
	CommonSettings.jit_max_block_size = 1;
	arm_jit_reset(true, true);

	int failures = sweep ? runSweep(full, onlySlot, verbose, listAll) : runProbes();

	// Reverse-engineering aid: dump the machine code a probe compiled to, so the
	// host instruction sequence can be disassembled instead of assumed.
	if (dumpBlockAdr != 0)
	{
		const uintptr_t fn = JIT_COMPILED_FUNC(dumpBlockAdr, 0);
		printf("\nblock %08X -> %p\n", dumpBlockAdr, (void *)fn);
		if (fn)
		{
			const unsigned char *p = (const unsigned char *)fn;
			FILE *f = fopen("block.bin", "wb");
			if (f)
			{
				fwrite(p, 1, 512, f);
				fclose(f);
			}
			for (int i = 0; i < 512; i++)
			{
				printf("%02X", p[i]);
				if ((i & 15) == 15) printf("\n");
			}
		}
	}

	if (gJitUnstable)
		printf("jit not repeatable: %d comparison(s) where the first call and a\n"
		       "second call on identical state disagreed\n", gJitUnstable);

	arm_jit_close();
	return failures == 0 ? 0 : 1;
}

#else
int main() { printf("built without HAVE_JIT\n"); return 2; }
#endif
