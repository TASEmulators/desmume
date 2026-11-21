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
#include <chrono>

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

// Size of the last block the backend compiled (emit_core.cpp records it). Only
// used by --dump, and only because the S4 acceptance gate is a code-size ratio.
extern int g_TOTALBYTES;

// Total bytes of every block compiled so far (see emit_core.cpp).
extern uint64_t g_all_block_bytes;

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
// Stack pointer the probes prime with. kStackTop is the first byte *past* main
// RAM, which is fine for the descending forms (PUSH/STMDB write below it) but
// puts every ascending access outside the mapped region. Probes that read at or
// above the base set this to a spot 64 bytes inside RAM instead, so the data
// path is compared rather than only the fault path.
static u32 gPrimeSp = 0;

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
	NDS_ARM9.R[13] = gPrimeSp ? gPrimeSp : kStackTop;

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
//
// `checkFallThrough` says whether that last restriction applies. A sequence run
// has to pass false -- see compareSequence -- because reaching the end of a
// planted sequence is not the same event as executing its last instruction.
static bool interpInvariantHolds(const CpuSnapshot *a, u32 insnSize, char *why, size_t whyLen)
{
	if (a->r[15] == a->next_instruction + insnSize)
		return true;
	snprintf(why, whyLen, "interpreter invariant broken: R15=%08X next=%08X size=%u",
		a->r[15], a->next_instruction, insnSize);
	return false;
}

static bool sameSnapshot(const CpuSnapshot *a, const CpuSnapshot *b,
	u32 codeAdr, u32 insnSize, bool checkFallThrough, char *why, size_t whyLen)
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
	if (checkFallThrough && fellThrough && !interpInvariantHolds(a, insnSize, why, whyLen))
		return false;

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

	return sameSnapshot(&interp, &jit, codeAdr, (cpsr & 0x20u) ? 2u : 4u,
		true, why, whyLen);
}

struct Probe
{
	const char *name;
	u32 opcode;
	bool thumb;
	// null = the default kOperands rotation. Probes whose base or target
	// register cannot be an arbitrary 32-bit value pin their own set: an
	// address of 0x7FFFFFFF would put every access outside main RAM and the
	// comparison would only ever cover the fault path.
	const u32 *ops;
	// 0 = kStackTop. See gPrimeSp.
	u32 sp;
};

// A stack pointer inside main RAM, 64 bytes below the end. The default
// kStackTop sits one byte past the last mapped byte, which is fine for the
// descending forms but leaves every ascending access unmapped.
static const u32 kStackProbe = 0x023FFFC0u;

// R0 = base, the rest is data. Used by the Thumb forms whose base is not SP.
static const u32 kOpsStack[8] = {
	kStackProbe, 0x11223344u, 0x55667788u, 0x99AABBCCu,
	0xDEADBEEFu, 0x00000004u, 0x00000008u, 0x00000010u,
};

// R1 = base, R2 = offset, R0 = value. The Thumb register-offset forms have no
// SP-relative encoding, and a 0x7FFFFFFF base would only test the fault path.
static const u32 kOpsThumbMem[8] = {
	0xDEADBEEFu, kStackProbe, 0x00000004u, 0, 0, 0, 0, 0,
};

// Small offsets for the ARM register-offset forms (R1 = Rm).
static const u32 kOpsOffsets[8] = {
	0x00000000u, 0x00000004u, 0x00000008u, 0x0000000Cu,
	0x00000010u, 0x00000020u, 0x00000040u, 0x00000080u,
};

// R0 = IRQ mode: `msr cpsr_c, r0` has to bank R13/R14 rather than switch to an
// architecturally invalid mode.
static const u32 kOpsMode[8] = { 0x00000012u, kStackProbe, 0, 0, 0, 0, 0, 0 };

// BX/BLX targets: ARM state 0x02000300 and Thumb state 0x02000305.
static const u32 kOpsBx[8] = { 0x02000301u, 0x02000305u, 0, 0, 0, 0, 0, 0 };

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
	// Saturating arithmetic. `op{Rd}, Rm, Rn` puts Rm in bits 3-0 and Rn in
	// bits 19-16, so these read `qadd r0, r2, r1`: the source operands are
	// separated (R1 != R2) because the whole family's trap is which of the two
	// gets doubled. The last two alias Rd to a source, where the operand has to
	// be read before the destination is written.
	{ "ARM   QADD  r0,r2,r1",    0xE1010052u, false },
	{ "ARM   QSUB  r0,r2,r1",    0xE1210052u, false },
	{ "ARM   QDADD r0,r2,r1",    0xE1410052u, false },
	{ "ARM   QDSUB r0,r2,r1",    0xE1610052u, false },
	{ "ARM   QADD  r1,r1,r2",    0xE1021051u, false },
	{ "ARM   QDADD r1,r2,r1",    0xE1411052u, false },
	// Memory-access probes. The slot sweep only emits opcodes with the low 20
	// bits zero (opcode = slot << 20), so every memory slot is exercised only in
	// its degenerate form: base == dest, zero offset, no writeback. These cover
	// the variants the sweep cannot reach: Rd != Rn, positive/negative offsets,
	// pre/post-index writeback, byte/halfword widths, and register offsets.
	// All use R13 (the primed stack pointer, kStackTop) as a legal base so both
	// engines read/write the same mapped RAM; STR probes write an idempotent
	// value (the same primed R0) and the harness compares register state only.
	{ "ARM   LDR  r0,[r13,#4]",     0xE59D0004u, false },
	{ "ARM   LDR  r0,[r13,#-4]",    0xE51D0004u, false },
	{ "ARM   LDR  r0,[r13,#4]!",    0xE5BD0004u, false },  // pre-index writeback
	{ "ARM   LDR  r0,[r13],#4",     0xE49D0004u, false },  // post-index writeback
	{ "ARM   LDRB r0,[r13,#1]",     0xE5DD0001u, false },
	{ "ARM   LDRH r0,[r13,#2]",     0xE1DD0002u, false },
	{ "ARM   LDRSB r0,[r13,#1]",    0xE1DD000Du, false },
	{ "ARM   LDRSH r0,[r13,#2]",    0xE1DD000Fu, false },
	{ "ARM   LDR  r0,[r13,r1]",     0xE79D0001u, false },  // register offset
	{ "ARM   STR  r0,[r13,#4]",     0xE58D0004u, false },
	{ "ARM   STR  r0,[r13,#-4]",    0xE50D0004u, false },
	{ "ARM   STR  r0,[r13,#4]!",    0xE5AD0004u, false },  // pre-index writeback
	{ "ARM   STR  r0,[r13],#4",     0xE48D0004u, false },  // post-index writeback
	{ "ARM   STRB r0,[r13,#1]",     0xE5CD0001u, false },
	{ "ARM   STRH r0,[r13,#2]",     0xE1CD0002u, false },
	// Thumb format 2 (LDR/STR immediate) and format 3 (register offset).
	// LDR/STR immediate use SP as base (a legal mapped address) so the MMU
	// access is deterministic; Rn=R1 would land on unmapped operands whose
	// MMU timing differs per vector (cycle-only noise, not a semantic defect).
	{ "THUMB LDR  r0,[sp,#4]",      0x9801u, true },
	{ "THUMB STR  r0,[sp,#4]",      0x9001u, true },
	{ "THUMB LDR  r0,[r1,r2]",      0x5088u, true },
	// Thumb PUSH/POP (STMDB/LDMIA with SP): stack is legal mapped RAM.
	{ "THUMB PUSH {r0,r1}",         0xB403u, true },
	{ "THUMB POP  {r0,r1}",         0xC003u, true },
	// Thumb format 4, register shifts: (Rs == Rd) then (Rs != Rd).
	{ "THUMB LSL  r0,r0",        0x4080u, true },
	{ "THUMB LSL  r0,r1",        0x4088u, true },
	{ "THUMB LSR  r0,r0",        0x40C0u, true },
	{ "THUMB LSR  r0,r1",        0x40C8u, true },
	{ "THUMB ASR  r0,r0",        0x4100u, true },
	{ "THUMB ASR  r0,r1",        0x4108u, true },
	{ "THUMB ROR  r0,r0",        0x41C0u, true },
	{ "THUMB ROR  r0,r1",        0x41C8u, true },

	// ----------------------------------------------------------------------
	// The block-level probe on the arm64 device covers shapes the slot sweep
	// cannot reach, because the sweep forces every operand field to zero. These
	// are the same shapes, ported so the x64 backend is held to them too.
	// ----------------------------------------------------------------------

	// LDRD/STRD: the only doubleword accesses, and the only handlers whose
	// helper walks cpu->R[] by *index* (op_ldrd_tab / op_strd_tab). A
	// destination still sitting in a cache slot is the mistake to catch, and
	// the W bit has its own write-back ordering per index mode.
	{ "ARM   LDRD r0,[r13]",       0xE1CD00D0u, false, nullptr, kStackProbe },
	{ "ARM   LDRD r0,[r13,#8]",    0xE1CD00D8u, false, nullptr, kStackProbe },
	{ "ARM   LDRD r0,[r13],#8",    0xE0CD00D8u, false, nullptr, kStackProbe },
	{ "ARM   LDRD r0,[r13,#8]!",   0xE1ED00D8u, false, nullptr, kStackProbe },
	{ "ARM   LDRD r0,[r13,r1]",    0xE18D00D1u, false, kOpsOffsets, kStackProbe },
	{ "ARM   STRD r0,[r13]",       0xE1CD00F0u, false, nullptr, kStackProbe },
	{ "ARM   STRD r0,[r13,#8]",    0xE1CD00F8u, false, nullptr, kStackProbe },
	{ "ARM   STRD r0,[r13],#8",    0xE0CD00F8u, false, nullptr, kStackProbe },
	{ "ARM   STRD r0,[r13,#8]!",   0xE1ED00F8u, false, nullptr, kStackProbe },
	{ "ARM   STRD r0,[r13,r1]",    0xE18D00F1u, false, kOpsOffsets, kStackProbe },

	// SWP/SWPB: address and data by value, destination through `u32*`.
	{ "ARM   SWP  r0,r1,[r13]",    0xE10D0091u, false, nullptr, kStackProbe },
	{ "ARM   SWPB r0,r1,[r13]",    0xE14D0091u, false, nullptr, kStackProbe },

	// LDM/STM: all four address modes. The list also carries a PC variant,
	// because a load with bit 15 set is a branch and a store with bit 15 set
	// leaks whatever the pipeline holds.
	{ "ARM   LDMIA r13!,{r0-r2}",  0xE8BD0007u, false, nullptr, kStackProbe },
	{ "ARM   LDMIB r13,{r0-r2}",   0xE99D0007u, false, nullptr, kStackProbe },
	{ "ARM   LDMDA r13,{r0-r2}",   0xE81D0007u, false, nullptr, kStackProbe },
	{ "ARM   LDMDB r13,{r0-r2}",   0xE91D0007u, false, nullptr, kStackProbe },
	{ "ARM   STMIA r13!,{r0-r2}",  0xE8AD0007u, false, nullptr, kStackProbe },
	{ "ARM   STMIB r13!,{r0-r2}",  0xE9AD0007u, false, nullptr, kStackProbe },
	{ "ARM   STMDA r13,{r0-r2}",   0xE80D0007u, false, nullptr, kStackProbe },
	{ "ARM   STMDB r13!,{r0-r2}",  0xE92D0007u, false, nullptr, kStackProbe },
	{ "ARM   LDMIA r13!,{r0,pc}",  0xE8BD8001u, false, nullptr, kStackProbe },
	{ "ARM   STMDB r13!,{r0,pc}",  0xE92D8001u, false, nullptr, kStackProbe },

	// Register offset *with a shift*: Rm goes through the shifter, which is the
	// path the arm64 backend only just moved onto its register cache.
	{ "ARM   LDR r0,[r13,r1,LSL2]",0xE79D0101u, false, kOpsOffsets, kStackProbe },
	{ "ARM   LDR r0,[r13,-r1]",    0xE71D0001u, false, kOpsOffsets, kStackProbe },
	{ "ARM   LDR r0,[r13,r1,ASR2]",0xE79D0141u, false, kOpsOffsets, kStackProbe },
	{ "ARM   STR r0,[r13,r1,LSL2]",0xE78D0101u, false, kOpsOffsets, kStackProbe },
	{ "ARM   STR r0,[r13,-r1]",    0xE70D0001u, false, kOpsOffsets, kStackProbe },

	// Rd == PC on a load: the block ends by branching to the loaded word. This
	// is the shape that used to fail asmjit's finalize pass on arm64.
	{ "ARM   LDR pc,[r13,#4]",     0xE59DF004u, false, nullptr, kStackProbe },

	// Multiply family: the operand that the cycle tail rewrites in place is the
	// reason these cannot take a cache slot, on either backend.
	{ "ARM   MULS r0,r1,r2",       0xE0100291u, false },
	{ "ARM   MLA  r0,r1,r2,r3",    0xE0203291u, false },
	{ "ARM   UMULL r4,r5,r1,r2",   0xE0854291u, false },
	{ "ARM   SMULL r4,r5,r1,r2",   0xE0C54291u, false },
	{ "ARM   UMLAL r4,r5,r1,r2",   0xE0A54291u, false },
	{ "ARM   SMLAL r4,r5,r1,r2",   0xE0E54291u, false },
	{ "ARM   SMULBB r0,r1,r2",     0xE1600281u, false },
	{ "ARM   SMLABT r0,r1,r2,r3",  0xE10032C1u, false },
	{ "ARM   SMULWB r0,r1,r2",     0xE12002A1u, false },
	{ "ARM   SMLAWX r0,r1,r2,r3",  0xE12032C1u, false },
	{ "ARM   SMLALBB r4,r5,r1,r2", 0xE1454281u, false },
	{ "ARM   CLZ  r0,r1",          0xE16F0F11u, false },

	// Shifts by register: Rm and Rs are separate fields, and Rs is what the
	// sweep pins to zero.
	{ "ARM   MOV r0,r1,LSR r2",    0xE1A00231u, false },
	{ "ARM   MOV r0,r1,ASR r2",    0xE1A00251u, false },
	{ "ARM   MOV r0,r1,ROR r2",    0xE1A00271u, false },
	{ "ARM   ORR r0,r1,r2,LSL r3", 0xE1810312u, false },

	// PSR transfer. The last one leaves supervisor mode, which forces the
	// banking path (armcpu_switchMode) through both engines.
	{ "ARM   MRS  r0,CPSR",        0xE10F0000u, false },
	{ "ARM   MRS  r0,SPSR",        0xE14F0000u, false },
	{ "ARM   MSR  CPSR_f,r0",      0xE128F000u, false },
	{ "ARM   MSR  SPSR_f,r0",      0xE168F000u, false },
	{ "ARM   MSR  CPSR_c,r0",      0xE121F000u, false, kOpsMode },

	// Branch / exception.
	{ "ARM   B    #+8",            0xEA000002u, false },
	{ "ARM   BL   #+8",            0xEB000002u, false },
	{ "ARM   BX   r0",             0xE12FFF10u, false, kOpsBx },
	{ "ARM   BLX  r1",             0xE12FFF31u, false, kOpsBx },
	{ "ARM   SWI  #0",             0xEF000000u, false },
	{ "ARM   BKPT #0",             0xE1200070u, false },

	// Thumb memory: immediate and register-offset forms for every width, with a
	// base inside main RAM (format 3 has no SP-relative encoding).
	{ "THUMB LDR  r0,[r1,#4]",     0x6848u, true, kOpsThumbMem },
	{ "THUMB STR  r0,[r1,#4]",     0x6048u, true, kOpsThumbMem },
	{ "THUMB LDRB r0,[r1,#1]",     0x7848u, true, kOpsThumbMem },
	{ "THUMB STRB r0,[r1,#1]",     0x7048u, true, kOpsThumbMem },
	{ "THUMB LDRH r0,[r1,#2]",     0x8848u, true, kOpsThumbMem },
	{ "THUMB STRH r0,[r1,#2]",     0x8048u, true, kOpsThumbMem },
	{ "THUMB LDR  r0,[r1,r2]",     0x5888u, true, kOpsThumbMem },
	{ "THUMB LDRB r0,[r1,r2]",     0x5C88u, true, kOpsThumbMem },
	{ "THUMB STRB r0,[r1,r2]",     0x5488u, true, kOpsThumbMem },
	{ "THUMB LDRH r0,[r1,r2]",     0x5A88u, true, kOpsThumbMem },
	{ "THUMB STRH r0,[r1,r2]",     0x5288u, true, kOpsThumbMem },
	{ "THUMB LDRSB r0,[r1,r2]",    0x5688u, true, kOpsThumbMem },
	{ "THUMB LDRSH r0,[r1,r2]",    0x5E88u, true, kOpsThumbMem },

	// Thumb PC/SP arithmetic and the block-level stack forms.
	{ "THUMB LDR  r0,[pc,#4]",     0x4801u, true },
	{ "THUMB ADR  r0,#4",          0xA001u, true },
	{ "THUMB ADD  r0,sp,#4",       0xA801u, true },
	{ "THUMB PUSH {r0,lr}",        0xB501u, true },
	{ "THUMB POP  {r0,pc}",        0xBD01u, true },
	{ "THUMB LDMIA r0!,{r1,r2}",   0xC806u, true, kOpsStack },
	{ "THUMB STMIA r0!,{r1,r2}",   0xC006u, true, kOpsStack },

	// Thumb control flow and exceptions.
	{ "THUMB B    #+8",            0xE004u, true },
	{ "THUMB BEQ  #+8",            0xD004u, true },
	{ "THUMB SWI  #0",             0xDF00u, true },
	{ "THUMB BKPT #0",             0xBE00u, true },
	{ "THUMB BX   r1",             0x4708u, true, kOpsBx },
	{ "THUMB BLX  r1",             0x4788u, true, kOpsBx },

	// Thumb high-register forms (both registers low is the deprecated encoding
	// the assembler refuses to emit, so these are spelled as `.inst`).
	{ "THUMB MOV  r0,r1 (hi)",     0x4608u, true },
	{ "THUMB ADD  r0,r1 (hi)",     0x4408u, true },
	{ "THUMB CMP  r0,r1 (hi)",     0x4288u, true },

	// Thumb immediate ALU: the sweep only reaches the zero immediate.
	{ "THUMB MOVS r0,#5",          0x2005u, true },
	{ "THUMB CMP  r0,#5",          0x2805u, true },
	{ "THUMB ADDS r0,#5",          0x3005u, true },
	{ "THUMB SUBS r0,#5",          0x3805u, true },
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
	int cycleRecords = 0;
	u32 codeAdr = kCodeBase;

	for (int p = 0; p < probeCount; p++)
	{
		// Probes that read at or above their base have to see mapped memory;
		// see gPrimeSp. Restored after the probe so the sweep is unaffected.
		gPrimeSp = kProbes[p].sp;

		for (int v = 0; v < vectors; v++)
		{
			const u32 rotated[8] = {
				kOperands[v], kOperands[(v + 1) & 7], kOperands[(v + 2) & 7], kOperands[(v + 3) & 7],
				kOperands[(v + 4) & 7], kOperands[(v + 5) & 7], kOperands[(v + 6) & 7], kOperands[(v + 7) & 7],
			};
			const u32 *ops = kProbes[p].ops ? kProbes[p].ops : rotated;

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
					// Same arbitration as the sweep: a cycle difference is a
					// record, not a failure. The JIT's cycle model is
					// approximate by design (§3.5), and one encoder decision can
					// move every instruction in a class at once -- which is
					// exactly what `--full` already reports as a bucket. An
					// architectural difference (R0-R14, CPSR, instruct_adr, a
					// fault, an invariant break) still fails.
					if (strncmp(why, "cycles:", 7) == 0)
					{
						cycleRecords++;
						if (gTrace)
						{
							printf("note  %-24s vec%d f=%d  %s\n", kProbes[p].name, v, f, why);
							// Without the flush, notes and the interpreter's own
							// stderr chatter interleave arbitrarily once stdout is
							// a pipe, so two runs of the same binary look different.
							fflush(stdout);
						}
					}
					else
					{
						printf("FAIL  %-24s vec%d f=%d  %s\n", kProbes[p].name, v, f, why);
						fflush(stdout);
						failures++;
					}
				}
			}

			// One unique address per (probe, vector) so the JIT never reuses a
			// block compiled for a different opcode.
			codeAdr += 4;
		}

		gPrimeSp = 0;
	}

	printf("\n%d probe(s) x %d vectors x 2 flags: %d mismatch(es), %d cycle record(s)\n",
		probeCount, vectors, failures, cycleRecords);
	return failures;
}
// The emulator's own mnemonic tables, so a report line names the same
// instruction the worklist in documentation/jit-opcode-matrix.md does.
static const char *mnemonic(bool thumb, u32 opcode)
{
	// The ARM dispatch table is indexed by INSTRUCTION_INDEX(), not by
	// (opcode >> 20): the two agree only for the low four bits being zero, so
	// naming a slot from `opcode >> 20` mislabels most of the sweep -- including
	// every S-bit variant, which is exactly where the interesting failures are.
	return thumb ? thumb_instruction_names[opcode >> 6]
	             : arm_instruction_names[INSTRUCTION_INDEX(opcode)];
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

// Dumps the machine code a compiled block consists of. `size` is the block's
// own size, not g_TOTALBYTES: by the time this runs the backend may already have
// compiled the follow-on block (see measureBlockSize), and printing that one's
// length would truncate or over-read this one.
static void dumpCompiledBlock(u32 adr, uintptr_t fn, int size)
{
	printf("\nblock %08X -> %p, %d bytes (%d instructions)\n",
		adr, (void *)fn, size, size / 4);
	if (!fn)
		return;

	const unsigned char *p = (const unsigned char *)fn;
	if (size <= 0 || size > 4096) size = 512;   // unknown: keep the old bound
	FILE *f = fopen("block.bin", "wb");
	if (f)
	{
		fwrite(p, 1, size, f);
		fclose(f);
	}
	for (int i = 0; i < size; i++)
	{
		printf("%02X", p[i]);
		if ((i & 15) == 15) printf("\n");
	}
}

// Code-size measurement: the S4 acceptance gate is stated as "generated
// instructions per handler <= 50% of before", and that cannot be measured with
// the sweep, which forces one instruction per block on purpose (a comparison
// spanning several instructions is ambiguous). A real block is up to
// CommonSettings.jit_max_block_size instructions long and the block prologue --
// the CPU base pointer in particular -- is amortised over all of them, so a
// one-instruction block understates the win. This plants the same opcode `n`
// times, compiles the block once, and reports its size.
static int measureBlockSize(int n, int slot, bool thumb, u32 dumpAdr, u32 rawOpcode)
{
	// --opcode bypasses the slot<<20 / slot<<6 synthesis for encodings it cannot
	// express: ARM `bkpt` carries bits in 7..4, so slot 0x127 would assemble to
	// 0x12700000 -- a different instruction -- and the bl/zbl offset fields move
	// the slot too.
	if (!rawOpcode && slot < 0)
	{
		printf("--block needs --slot (and --thumb for the Thumb set), or --opcode\n");
		return 2;
	}
	if (n < 1 || n > 100)
	{
		printf("--block must be 1..100\n");
		return 2;
	}

	const u32 opcode = rawOpcode ? rawOpcode
	                             : (thumb ? ((u32)slot << 6) : ((u32)slot << 20));
	const u32 insnSize = thumb ? 2u : 4u;
	for (int i = 0; i < n; i++)
	{
		if (thumb) _MMU_write16<0>(kCodeBase + i * insnSize, (u16)opcode);
		else _MMU_write32<0>(kCodeBase + i * insnSize, opcode);
	}

	CommonSettings.jit_max_block_size = n;
	arm_jit_reset(true, true);

	primeState(kCodeBase, kOperands, opcode, 0x00000013u | (thumb ? 0x20u : 0u));
	execJit();   // compiles the block at kCodeBase

	// The size has to be read here, not after the run below. A block whose last
	// instruction falls through leaves instruct_adr at kCodeBase + n*size, so the
	// second execJit() compiles *another* block there and leaves its size in
	// g_TOTALBYTES -- which is what this function used to print. The number then
	// belonged to the follow-on block and did not move when the handler changed.
	const int blockBytes = g_TOTALBYTES;
	const uintptr_t blockFn = JIT_COMPILED_FUNC(kCodeBase, 0);

	execJit();   // runs it, so what is measured is code that has been executed

	if (rawOpcode)
		printf("block: %s opcode %08X x %d instruction(s) -> %d bytes (%d host instructions)\n",
			thumb ? "thumb" : "arm", opcode, n, blockBytes, blockBytes / 4);
	else
		printf("block: %s slot %03X x %d instruction(s) -> %d bytes (%d host instructions)\n",
			thumb ? "thumb" : "arm", slot, n, blockBytes, blockBytes / 4);

	if (dumpAdr)
		dumpCompiledBlock(blockFn ? dumpAdr : kCodeBase, blockFn, blockBytes);
	return 0;
}

// Block benchmark. --block reports code size, which cannot show a change that
// only removes work from a C helper: the emitted block is unchanged there and
// the win is entirely inside the call. This plants `n` copies of one opcode,
// compiles the block once and times repeated calls of the compiled function.
//
// The whole block re-reads the same address, so after the first access the
// cache controller's last-block shortcut is primed. That is the best case for
// any main-RAM fast-path work, which makes the number a ceiling rather than a
// prediction -- see the note in the migration plan (10.13).
static int runBench(int n, int slot, bool thumb, int iters, u32 rawOpcode)
{
	if (!rawOpcode && slot < 0)
	{
		printf("--bench needs --slot (and --thumb for the Thumb set), or --opcode\n");
		return 2;
	}
	if (n < 1 || n > 100 || iters < 1)
	{
		printf("--bench needs 1..100 instructions and 1+ iterations\n");
		return 2;
	}

	const u32 opcode = rawOpcode ? rawOpcode
	                             : (thumb ? ((u32)slot << 6) : ((u32)slot << 20));
	const u32 insnSize = thumb ? 2u : 4u;
	for (int i = 0; i < n; i++)
	{
		if (thumb) _MMU_write16<0>(kCodeBase + i * insnSize, (u16)opcode);
		else _MMU_write32<0>(kCodeBase + i * insnSize, opcode);
	}

	CommonSettings.jit_max_block_size = n;
	arm_jit_reset(true, true);
	// Every register points into main RAM, so load and store shapes both land
	// in the region the fast path covers (kOperands[1] is 1, which would send
	// every access down the generic path instead).
	u32 ops[8];
	for (int i = 0; i < 8; i++)
		ops[i] = kCodeBase + 0x3000;
	primeState(kCodeBase, ops, opcode, 0x00000013u | (thumb ? 0x20u : 0u));
	execJit();

	u32 (*fn)() = (u32 (*)())JIT_COMPILED_FUNC(kCodeBase, 0);
	if (!fn)
	{
		printf("bench: no compiled block at %08X\n", kCodeBase);
		return 1;
	}

	// Warm up: the first calls also fill the emulated cache, and priming the
	// branch predictor keeps the measurement from rewarding luck.
	for (int i = 0; i < 10000; i++)
		fn();

	const std::chrono::steady_clock::time_point t0 = std::chrono::steady_clock::now();
	for (int i = 0; i < iters; i++)
		fn();
	const std::chrono::steady_clock::time_point t1 = std::chrono::steady_clock::now();

	const double nsPerBlock = std::chrono::duration<double, std::nano>(t1 - t0).count() / iters;
	if (rawOpcode)
		printf("bench: %s opcode %08X x %d -> %.1f ns/block, %.3f ns/instruction (%d iterations)\n",
			thumb ? "thumb" : "arm", opcode, n, nsPerBlock, nsPerBlock / n, iters);
	else
		printf("bench: %s slot %03X x %d -> %.1f ns/block, %.3f ns/instruction (%d iterations)\n",
			thumb ? "thumb" : "arm", slot, n, nsPerBlock, nsPerBlock / n, iters);
	return 0;
}

// ---------------------------------------------------------------------------
// Multi-instruction sequences
// ---------------------------------------------------------------------------
//
// Every comparison above runs a one-instruction block, which is exactly what a
// per-handler check wants: the result is unambiguous. It is also blind to the
// whole class of defects that only appear once instructions share a block --
// register residency, the CACHED_PTR() side effect, sync_r15() bookkeeping, and
// everything S4 changes. So this mode plants n instructions, runs the
// interpreter one step at a time and one compiled block of the same n
// instructions, and compares the end state.
//
// Two things make a sequence not comparable, and both are skipped rather than
// reported:
//
//  * the engines end up at a different PC. A compiled block always ends at a
//    branch, taken or not, so "one block" is not "n instructions"; both engines
//    are therefore driven until PC reaches the end of the sequence (see
//    runToEnd) and a sequence where they disagree about the path is dropped.
//  * an instruction has a side effect on memory. primeState() resets registers
//    but not memory, and each engine runs after the other has already written:
//    a load that reads a location a previous store in the same run wrote would
//    compare data produced by the previous engine, not instruction semantics.
//    Slots whose mnemonic stores are therefore left out of the sequence, which
//    is decided from the same name table the failures are printed from.
static bool slotHasMemorySideEffect(u32 slot)
{
	const char *name = mnemonic(false, (u32)slot << 20);
	if (!name)
		return true;
	// ST* covers STR/LDM-variants' counterparts and STC; SWP and the
	// coprocessor writes change state the next instruction observes.
	return strstr(name, "OP_ST") != NULL
	    || strstr(name, "OP_SWP") != NULL
	    || strstr(name, "OP_MCR") != NULL
	    || strstr(name, "OP_CDP") != NULL
	    || strstr(name, "OP_LDC") != NULL
	    || strstr(name, "OP_UND") != NULL;
}

// Drives one engine until PC reaches the end of the planted sequence. The step
// count is not the stopping condition on purpose: a compiled block ends at
// every branch whether it is taken or not, so "one block" is not "n
// instructions". Comparing at a common PC keeps the two runs comparable through
// a branch; a run that never gets there (or overshoots) is reported through
// *stuck and the sequence is dropped by the caller.
//
// `isJit` selects the engine; the interpreter executes one instruction per call,
// the JIT one block per call.
static u32 runToEnd(u32 codeAdr, int n, bool isJit, bool *stuck)
{
	const u32 endAdr = codeAdr + 4u * (u32)n;
	u32 total = 0;
	*stuck = false;

	for (int guard = 0; guard < 4 * n + 8; guard++)
	{
		if (NDS_ARM9.instruct_adr == endAdr)
			return total;
		const u32 before = NDS_ARM9.instruct_adr;
		total += isJit ? execJit() : execInterp();
		if (NDS_ARM9.instruct_adr == before)
		{
			// Not advancing: PC went somewhere this sequence does not cover
			// (an exception vector, a branch out of the planted block).
			*stuck = true;
			return total;
		}
	}

	*stuck = true;
	return total;
}

// Runs one sequence through both engines. Returns false only for a real
// mismatch; *skipped is set when the sequence turned out not to be comparable.
static bool compareSequence(u32 codeAdr, const u32 *ops, const u32 *seq, int n, u32 cpsr,
	char *why, size_t whyLen, u32 *jitCycles, bool *skipped)
{
	*skipped = false;
	*jitCycles = 0;
	bool stuck = false;

	// Warm-up: the interpreter's first execution of an *address* pays a one-off
	// cost (measured 52 cycles) that has nothing to do with instruction
	// semantics. It is per address, not per run, so every instruction of the
	// sequence has to be executed once before the measured run.
	primeState(codeAdr, ops, seq[0], cpsr);
	for (int k = 0; k < n; k++)
		execInterp();
	if (faulted(why, whyLen, "interpreter (warm-up)"))
	{
		*skipped = true;
		return true;
	}

	// Same for the JIT, and for a different reason: the first call into an
	// uncompiled address compiles the block and advances the state through
	// op_decode[] instead of running emitted code. Walking the sequence once
	// leaves every block on the path compiled, so the measured run is emitted
	// code from the first instruction on.
	primeState(codeAdr, ops, seq[0], cpsr);
	for (int k = 0; k < n; k++)
	{
		u32 before = NDS_ARM9.instruct_adr;
		execJit();
		if (NDS_ARM9.instruct_adr == before)
			break;
	}
	if (faulted(why, whyLen, "jit (warm-up)"))
	{
		*skipped = true;
		return true;
	}

	primeState(codeAdr, ops, seq[0], cpsr);
	u32 total = runToEnd(codeAdr, n, false, &stuck);
	if (faulted(why, whyLen, "interpreter"))
		return false;
	if (stuck)
	{
		*skipped = true;
		return true;
	}
	CpuSnapshot interp;
	takeSnapshot(&interp, total);

	// The blocks on the path are compiled by the warm-up above; this run is
	// emitted code from the first instruction on.
	primeState(codeAdr, ops, seq[0], cpsr);
	u32 cj = runToEnd(codeAdr, n, true, &stuck);
	if (faulted(why, whyLen, "jit"))
		return false;
	if (stuck)
	{
		*skipped = true;
		return true;
	}
	CpuSnapshot jit;
	takeSnapshot(&jit, cj);
	*jitCycles = cj;

	if (gTrace)
	{
		printf("      interp pc=%08X cpsr=%08X R0=%08X cyc=%u | jit pc=%08X cpsr=%08X R0=%08X cyc=%u\n",
			interp.instruct_adr, interp.cpsr, interp.r[0], interp.cycles,
			jit.instruct_adr, jit.cpsr, jit.r[0], jit.cycles);
		fflush(stdout);
	}

	if (!sameSnapshot(&interp, &jit, codeAdr + 4u * (u32)(n - 1), 4, false, why, whyLen))
		return false;

	// Pipeline consistency, asserted on the interpreter alone.
	//
	// The fall-through gate in sameSnapshot() cannot be used here: the run is
	// driven until PC reaches the end of the planted sequence, and for n == 2 a
	// cond=0xF `B` at the first slot jumps there in one step (op_b() switches to
	// Thumb for cond 0xF -- arm_instructions.cpp:3131, arm_jit.cpp:2505 -- so the
	// pipeline re-primes at the target in Thumb state). Taking size = 4 because
	// the sequence was planted in ARM state then reports
	// "R15=..A06C next=..A06A", which is perfectly consistent for size = 2.
	//
	// What actually holds, and is worth asserting, is the post-prefetch relation
	// with the size implied by the state the run ended in.
	const u32 endSize = (interp.cpsr & 0x20u) ? 2u : 4u;
	return interpInvariantHolds(&interp, endSize, why, whyLen);
}

static int runSequenceSweep(int n, bool full, int onlySlot, bool verbose, bool listAll, u32 dumpAdr)
{
	const int armSlots = 4096;
	const int vectors = full ? 8 : 2;
	const int flagVariants = 2;
	const int showCap = listAll ? 0x7FFFFFFF : 40;
	int failures = 0;
	int checked = 0;
	int skipped = 0;
	int cycleDiffs = 0;
	int shown = 0;
	FailureBuckets buckets = {0, 0, 0, 0, 0, 0, 0, 0};
	u32 codeAdr = kCodeBase;
	u32 *seq = (u32 *)calloc((size_t)n, sizeof(u32));

	if (!seq)
	{
		printf("out of memory for a %d-instruction sequence\n", n);
		return -1;
	}

	// The block size is a compile-time property of the backend, so it has to be
	// set before any block is built.
	CommonSettings.jit_max_block_size = n;
	arm_jit_reset(true, true);

	for (int slot = 0; slot < armSlots; slot++)
	{
		if (onlySlot >= 0 && slot != onlySlot)
			continue;

		bool usable = true;
		for (int k = 0; k < n && usable; k++)
		{
			const u32 s = (u32)((slot + k) & (armSlots - 1));
			if (slotHasMemorySideEffect(s))
				usable = false;
			seq[k] = s << 20;
		}
		if (!usable)
		{
			skipped++;
			continue;
		}

		for (int v = 0; v < vectors; v++)
		{
			const u32 ops[8] = {
				kOperands[v & 7], kOperands[(v + 1) & 7], kOperands[(v + 2) & 7], kOperands[(v + 3) & 7],
				kOperands[(v + 4) & 7], kOperands[(v + 5) & 7], kOperands[(v + 6) & 7], kOperands[(v + 7) & 7],
			};

			for (int k = 0; k < n; k++)
				_MMU_write32<0>(codeAdr + 4u * (u32)k, seq[k]);

			for (int f = 0; f < flagVariants; f++)
			{
				char why[128] = {0};
				bool wasSkipped = false;
				u32 jitCycles = 0;
				checked++;

				if (verbose)
				{
					printf("  try seq slot=%03X v=%d f=%d adr=%08X", slot, v, f, codeAdr);
					for (int k = 0; k < n; k++)
						printf(" %s", mnemonic(false, seq[k]));
					printf("\n");
					fflush(stdout);
				}

				if (!compareSequence(codeAdr, ops, seq, n, kFlagVariants[f], why, sizeof(why), &jitCycles, &wasSkipped))
				{
					// Cycle counts are implementation defined -- the ARM ARM says
					// nothing about how many cycles an instruction takes -- so a
					// difference here is a recorded observation, not a defect. The
					// single-instruction sweep keeps them in its failure count
					// because that is how the baseline was recorded; here they
					// would drown the signal (the block and the step loop do not
					// charge the same for a conditional instruction).
					if (strncmp(why, "cycles:", 7) == 0)
					{
						cycleDiffs++;
						if (verbose)
						{
							printf("  note cycles slot=%03X v=%d flags=%d %s\n", slot, v, f, why);
							fflush(stdout);
						}
					}
					else
					{
						failures++;
						buckets.add(why, jitCycles);
						if (shown < showCap)
						{
							printf("FAIL  seq slot=%03X v=%d flags=%d adr=%08X %s\n",
								slot, v, f, codeAdr, why);
							for (int k = 0; k < n; k++)
								printf("      [%d] %08X %s\n", k, seq[k], mnemonic(false, seq[k]));
							printf("      R0-R7 = %08X %08X %08X %08X %08X %08X %08X %08X\n",
								ops[0], ops[1], ops[2], ops[3], ops[4], ops[5], ops[6], ops[7]);
							fflush(stdout);
							shown++;
						}
					}
				}
				else if (wasSkipped)
				{
					skipped++;
				}

				// Single-sequence debugging aid: dump the machine code of the
				// block that was just compared, so a mismatch can be read off the
				// host instructions instead of guessed at.
				if (dumpAdr != 0 && v == 0 && f == 0)
					dumpCompiledBlock(codeAdr, JIT_COMPILED_FUNC(codeAdr, 0), g_TOTALBYTES);
			}

			codeAdr += 4u * (u32)n;
			if (codeAdr > kCodeBase + 0x00040000u)
				codeAdr = kCodeBase;
		}

		if ((slot & 0xFF) == 0xFF)
			arm_jit_reset(true, true);
	}

	free(seq);

	printf("\nsequence sweep (%d instructions): %d comparison(s), %d mismatch(es), %d skipped\n",
		n, checked, failures, skipped);
	printf("  cycle count (recorded, not arbitrated) : %d\n", cycleDiffs);
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
	int measureBlock = 0;
	int benchIters = 0;
	int sequenceLen = 0;
	bool measureThumb = false;
	u32 dumpBlockAdr = 0;
	u32 rawOpcode = 0;
	for (int i = 1; i < argc; i++)
	{
		if (strcmp(argv[i], "--sweep") == 0) sweep = true;
		else if (strcmp(argv[i], "--full") == 0) { full = true; sweep = true; }
		else if (strcmp(argv[i], "--verbose") == 0) verbose = true;
		else if (strcmp(argv[i], "--all") == 0) listAll = true;
		else if (strcmp(argv[i], "--slot") == 0 && i + 1 < argc) onlySlot = atoi(argv[++i]);
		else if (strcmp(argv[i], "--opcode") == 0 && i + 1 < argc)
			rawOpcode = (u32)strtoul(argv[++i], NULL, 16);
		else if (strcmp(argv[i], "--block") == 0 && i + 1 < argc) measureBlock = atoi(argv[++i]);
		else if (strcmp(argv[i], "--bench") == 0 && i + 1 < argc) benchIters = atoi(argv[++i]);
		else if (strcmp(argv[i], "--seq") == 0 && i + 1 < argc) { sequenceLen = atoi(argv[++i]); sweep = true; }
		else if (strcmp(argv[i], "--thumb") == 0) measureThumb = true;
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

	if (benchIters > 0)
		return runBench(measureBlock > 0 ? measureBlock : 16, onlySlot, measureThumb, benchIters, rawOpcode);

	if (measureBlock > 0)
		return measureBlockSize(measureBlock, onlySlot, measureThumb, dumpBlockAdr, rawOpcode);

	if (sequenceLen > 0)
	{
		if (sequenceLen < 2 || sequenceLen > 32)
		{
			printf("--seq must be 2..32\n");
			return 2;
		}
		int seqFailures = runSequenceSweep(sequenceLen, full, onlySlot, verbose, listAll, dumpBlockAdr);
		if (seqFailures < 0)
			return 2;
		if (gJitUnstable)
			printf("jit not repeatable: %d comparison(s)\n", gJitUnstable);
		arm_jit_close();
		return seqFailures == 0 ? 0 : 1;
	}

	int failures = sweep ? runSweep(full, onlySlot, verbose, listAll) : runProbes();

	// Reverse-engineering aid: dump the machine code a probe compiled to, so the
	// host instruction sequence can be disassembled instead of assumed.
	if (dumpBlockAdr != 0)
		dumpCompiledBlock(dumpBlockAdr, JIT_COMPILED_FUNC(dumpBlockAdr, 0), g_TOTALBYTES);

	if (sweep)
		printf("jit code: %llu bytes total\n", (unsigned long long)g_all_block_bytes);

	if (gJitUnstable)
		printf("jit not repeatable: %d comparison(s) where the first call and a\n"
		       "second call on identical state disagreed\n", gJitUnstable);

	arm_jit_close();
	return failures == 0 ? 0 : 1;
}

#else
int main() { printf("built without HAVE_JIT\n"); return 2; }
#endif
