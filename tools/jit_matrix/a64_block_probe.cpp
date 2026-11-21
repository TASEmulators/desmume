// Executes whole JIT blocks on real AArch64 hardware.
//
// a64_dump_new can compare the words a primitive emits, and a64_exec_probe can
// execute single primitives, but neither can run a *handler*: the handlers live
// in arm_jit_arm.cpp, which needs the emulator around it. That is why the
// migration's semantic verification has been stuck on "needs macOS with the
// full core" -- an x86 host can only compare bytes.
//
// This probe closes that gap differently: it includes arm_jit_arm.cpp directly
// and supplies the small remainder of the core the backend actually references
// (68 symbols, measured with `nm -u`). Then it compiles a one-instruction block
// through the real compile_basicblock() path, calls the resulting block through
// the C ABI, and dumps the emulated CPU state afterwards.
//
// Two things are deliberately fake, and both are fake in ways that make the
// comparison *stricter*, not looser:
//
//   * The interpreter tables (arm_instructions_set / thumb_instructions_set)
//     point at a stub that does nothing and returns 1 cycle. compile_basicblock()
//     drives the interpreter over each instruction as it emits it (that is how
//     it accumulates cycles), so the real tables would execute the instruction
//     before the block does. With the stub only the pipeline registers move, and
//     the probe restores them before calling the block -- so the block starts
//     from exactly the state the case primed.
//
//   * The MMU is main RAM only: 0x02000000..0x023FFFFF plus its mirrors, no
//     IO registers. Instruction semantics do not depend on it, and the address
//     translation is the same code path the emulator uses (_MMU_read32's inline
//     main-RAM case).
//
// Output is a per-case dump of R0..R14, CPSR and (for the second half of the
// suite) SPSR plus every banked R13/R14, plus self-checks against a C model of
// the instructions covered. The dump is meant to be diffed against a copy taken
// from the previous build -- either by pointing the include below at the old
// arm_jit_arm.cpp or by keeping the old dump around: the register-cache
// migration must not change a single bit of it, except where the batch under
// test deliberately changes behaviour.
//
// Two sections:
//   * multiply/CLZ (batch 5): single-instruction blocks, three operand sets,
//     two stores (one with the DMA flag armed) and a `mul`+`str` two-instruction
//     block that guards the deferred-write path;
//   * PSR / branch / exception (batch 6): MRS/MSR, B/BL/BX/BLX, SWI, BKPT, plus
//     three `add sp,#4 ; <switch>` blocks. Those only mean something because
//     armcpu_switchMode() below really moves the banked registers -- with a
//     no-op stub the cache would look correct no matter what.
//
// Build and run (Termux / any aarch64 host):
//     clang++ -std=c++20 -O1 -I desmume/3rdparty -I desmume/src -DASMJIT_STATIC \
//         -DASMJIT_NO_X86 -DHAVE_JIT tools/jit_matrix/a64_block_probe.cpp \
//         obj/*.o -o a64_block_probe
//     ./a64_block_probe > block_new.txt
//     ./a64_block_probe --dump N        # words of case N's block
//     ./a64_block_probe --dump-psr N    # same, for the PSR/branch table

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <vector>

#include "../../desmume/src/utils/arm_jit/arm_jit_arm.cpp"

//-----------------------------------------------------------------------------
//   The remainder of the core
//---------------------------------------------------------------------------
// Everything below is what `nm -u arm_jit_arm.o` reports as missing. Nothing
// here implements instruction semantics; the handlers do that, and they are the
// subject of the probe.

armcpu_t NDS_ARM7;
armcpu_t NDS_ARM9;
armcp15_t cp15;
MMU_struct MMU;
MMU_struct_timing MMU_timing;

u32 _MMU_MAIN_MEM_MASK   = 0x003FFFFF;
u32 _MMU_MAIN_MEM_MASK16 = 0x003FFFFE;
u32 _MMU_MAIN_MEM_MASK32 = 0x003FFFFC;

u32 mmu_dma_event_pending = 0;
volatile bool execute = true;

std::vector<u32> memReadBreakPoints;
std::vector<u32> memWriteBreakPoints;

TCommonSettings CommonSettings;

// Only touched by OP_MSR_ (mode switch) and _maskPrecalc(), neither of which the
// multiply batch uses. NB: the parameter cannot be called `cpu` -- arm_jit_arm.cpp
// defines that as a macro.
void NDS_Reschedule() {}

// A faithful copy of armcpu.cpp's banking, minus armcpu_changeCPSR() (that one
// recomputes the cp15 region permissions and needs the full core). The copy has
// to move registers through armcpu_t in *memory*, otherwise the cases below
// could not tell a flushed cache from a stale one: the whole point is that
// switchMode() reads R13/R14/SPSR out of the struct, so a dirty cache slot hands
// it the pre-instruction value.
u32 armcpu_switchMode(armcpu_t *cp, u8 mode)
{
	const u32 oldmode = cp->CPSR.bits.mode;
	u32 t0, t1;

	switch (oldmode)
	{
		case USR: case SYS:
			cp->R13_usr = cp->R[13]; cp->R14_usr = cp->R[14];
			break;
		case FIQ:
			t0 = cp->R[8];  cp->R[8]  = cp->R8_fiq;  cp->R8_fiq  = t0;
			t0 = cp->R[9];  cp->R[9]  = cp->R9_fiq;  cp->R9_fiq  = t0;
			t0 = cp->R[10]; cp->R[10] = cp->R10_fiq; cp->R10_fiq = t0;
			t0 = cp->R[11]; cp->R[11] = cp->R11_fiq; cp->R11_fiq = t0;
			t0 = cp->R[12]; cp->R[12] = cp->R12_fiq; cp->R12_fiq = t0;
			cp->R13_fiq = cp->R[13]; cp->R14_fiq = cp->R[14];
			cp->SPSR_fiq = cp->SPSR;
			break;
		case IRQ: cp->R13_irq = cp->R[13]; cp->R14_irq = cp->R[14]; cp->SPSR_irq = cp->SPSR; break;
		case SVC: cp->R13_svc = cp->R[13]; cp->R14_svc = cp->R[14]; cp->SPSR_svc = cp->SPSR; break;
		case ABT: cp->R13_abt = cp->R[13]; cp->R14_abt = cp->R[14]; cp->SPSR_abt = cp->SPSR; break;
		case UND: cp->R13_und = cp->R[13]; cp->R14_und = cp->R[14]; cp->SPSR_und = cp->SPSR; break;
		default: break;
	}

	switch (mode)
	{
		case USR: case SYS:
			cp->R[13] = cp->R13_usr; cp->R[14] = cp->R14_usr;
			break;
		case FIQ:
			t1 = cp->R[8];  cp->R[8]  = cp->R8_fiq;  cp->R8_fiq  = t1;
			t1 = cp->R[9];  cp->R[9]  = cp->R9_fiq;  cp->R9_fiq  = t1;
			t1 = cp->R[10]; cp->R[10] = cp->R10_fiq; cp->R10_fiq = t1;
			t1 = cp->R[11]; cp->R[11] = cp->R11_fiq; cp->R11_fiq = t1;
			t1 = cp->R[12]; cp->R[12] = cp->R12_fiq; cp->R12_fiq = t1;
			cp->R[13] = cp->R13_fiq; cp->R[14] = cp->R14_fiq;
			cp->SPSR = cp->SPSR_fiq;
			break;
		case IRQ: cp->R[13] = cp->R13_irq; cp->R[14] = cp->R14_irq; cp->SPSR = cp->SPSR_irq; break;
		case SVC: cp->R[13] = cp->R13_svc; cp->R[14] = cp->R14_svc; cp->SPSR = cp->SPSR_svc; break;
		case ABT: cp->R[13] = cp->R13_abt; cp->R[14] = cp->R14_abt; cp->SPSR = cp->SPSR_abt; break;
		case UND: cp->R[13] = cp->R13_und; cp->R[14] = cp->R14_und; cp->SPSR = cp->SPSR_und; break;
		default: break;
	}

	cp->CPSR.bits.mode = mode & 0x1F;
	return oldmode;
}
void armcp15_setSingleRegionAccess(armcp15_t *, u8, u32, u32) {}

// TCommonSettings declares its constructor but defines it in common.cpp, which
// is not linked here. Nothing in the multiply batch reads a default, and the
// one field the backend does read (jit_max_block_size) is set explicitly in
// main(), so an empty constructor is equivalent for this purpose.
TCommonSettings::TCommonSettings() {}

// The interpreter tables. See the header comment: a do-nothing stub is what
// keeps the compile pass from executing the instruction itself.
static int g_interp_calls = 0;
static u32 DESMUME_FASTCALL interp_stub(const u32) { g_interp_calls++; return 1; }

#define S1   interp_stub
#define S4   S1, S1, S1, S1
#define S16  S4, S4, S4, S4
#define S64  S16, S16, S16, S16
#define S256 S64, S64, S64, S64
#define S1024 S256, S256, S256, S256
#define S4096 S1024, S1024, S1024, S1024

const OpFunc arm_instructions_set[2][4096] = { S4096, S4096 };
const OpFunc thumb_instructions_set[2][1024] = { S1024, S1024 };

// Main RAM only. The inline path in MMU.h already handles 0x02xxxxxx without
// reaching these, so they only see out-of-range addresses -- return 0/hang onto
// main RAM mirrors the way an unmapped read would.
static inline bool in_main(u32 adr) { return (adr & 0x0F000000u) == 0x02000000u; }

u32 DESMUME_FASTCALL _MMU_ARM9_read32(u32 adr)
{
	if (!in_main(adr)) return 0;
	u32 v; memcpy(&v, MMU.MAIN_MEM + (adr & _MMU_MAIN_MEM_MASK32), 4); return v;
}
u16 DESMUME_FASTCALL _MMU_ARM9_read16(u32 adr)
{
	if (!in_main(adr)) return 0;
	u16 v; memcpy(&v, MMU.MAIN_MEM + (adr & _MMU_MAIN_MEM_MASK16), 2); return v;
}
u8 DESMUME_FASTCALL _MMU_ARM9_read08(u32 adr)
{
	if (!in_main(adr)) return 0;
	return MMU.MAIN_MEM[adr & _MMU_MAIN_MEM_MASK];
}
void DESMUME_FASTCALL _MMU_ARM9_write32(u32 adr, u32 val)
{
	if (!in_main(adr)) return;
	memcpy(MMU.MAIN_MEM + (adr & _MMU_MAIN_MEM_MASK32), &val, 4);
}
void DESMUME_FASTCALL _MMU_ARM9_write16(u32 adr, u16 val)
{
	if (!in_main(adr)) return;
	memcpy(MMU.MAIN_MEM + (adr & _MMU_MAIN_MEM_MASK16), &val, 2);
}
void DESMUME_FASTCALL _MMU_ARM9_write08(u32 adr, u8 val)
{
	if (!in_main(adr)) return;
	MMU.MAIN_MEM[adr & _MMU_MAIN_MEM_MASK] = val;
}

u32 DESMUME_FASTCALL _MMU_ARM7_read32(u32 adr) { return _MMU_ARM9_read32(adr); }
u16 DESMUME_FASTCALL _MMU_ARM7_read16(u32 adr) { return _MMU_ARM9_read16(adr); }
u8  DESMUME_FASTCALL _MMU_ARM7_read08(u32 adr) { return _MMU_ARM9_read08(adr); }
void DESMUME_FASTCALL _MMU_ARM7_write32(u32 adr, u32 val) { _MMU_ARM9_write32(adr, val); }
void DESMUME_FASTCALL _MMU_ARM7_write16(u32 adr, u16 val) { _MMU_ARM9_write16(adr, val); }
void DESMUME_FASTCALL _MMU_ARM7_write08(u32 adr, u8 val) { _MMU_ARM9_write08(adr, val); }

//-----------------------------------------------------------------------------
//   Harness
//---------------------------------------------------------------------------

static const u32 kCodeBase = 0x02000000;
// Distinct slot per case: arm_jit_compile() keys the compiled-block cache on the
// address, so reusing one address would run the first case's block for all of
// them. 0x100 apart keeps every case inside main RAM and 4-byte aligned.
static const u32 kCaseStride = 0x100;
static const u32 kCpsrInit = 0x00000013u;   // supervisor, ARM, F/I masked, AL
static const u32 kCpsrThumb = 0x00000033u;  // same, Thumb
static const u32 kCpsrThumbZ = 0x40000033u; // Thumb with Z set, for `beq`

static int g_pass = 0, g_fail = 0;

// Set by the Thumb runner: PC is four bytes ahead there instead of eight, and
// next_instruction two instead of four. compile_basicblock() reads CPSR.T, so
// the caller's cpsr is what actually picks the decoder -- this only fixes the
// pipeline values the handlers see.
static bool g_thumb_pipe = false;

static void prime_cpu(u32 adr, u32 opcode, const u32 *ops, u32 cpsr)
{
	memset(NDS_ARM9.R, 0, sizeof(NDS_ARM9.R));
	for (int i = 0; i < 16; i++)
		NDS_ARM9.R[i] = ops[i];
	NDS_ARM9.CPSR.val = cpsr;
	// The pipeline value, not the address: handlers that read PC rely on
	// R15 = instruct_adr + 2*insnsize.
	const u32 insn = g_thumb_pipe ? 2 : 4;
	NDS_ARM9.R[15] = adr + 2 * insn;
	NDS_ARM9.instruct_adr = adr;
	NDS_ARM9.next_instruction = adr + insn;
	NDS_ARM9.instruction = opcode;
	NDS_ARM9.R[13] = 0x023FF000;            // inside main RAM, unused here
}

struct Result
{
	u32 r[16];
	u32 cpsr;
	u32 cycles;
};

// Compiles a single-instruction block at `adr` and executes it.
static bool run_one(u32 adr, u32 opcode, const u32 *ops, u32 cpsr, Result *out)
{
	*(u32 *)(MMU.MAIN_MEM + (adr & _MMU_MAIN_MEM_MASK32)) = opcode;

	prime_cpu(adr, opcode, ops, cpsr);
	arm_jit_compile<0>();                    // emits the block (and traces it)

	// The trace pass advanced the pipeline through the stub interpreter;
	// put the primed state back so the block runs from the case's inputs.
	prime_cpu(adr, opcode, ops, cpsr);

	ArmOpCompiled f = (ArmOpCompiled)JIT_COMPILED_FUNC(adr, 0);
	if (f == (ArmOpCompiled)interp_stub) {
		printf("FAIL %08X: block did not compile (fell back to the interpreter)\n", opcode);
		return false;
	}

	out->cycles = f();
	memcpy(out->r, NDS_ARM9.R, sizeof(out->r));
	out->cpsr = NDS_ARM9.CPSR.val;
	return true;
}

// Same, for blocks of up to two instructions (a mode switch is only interesting
// when a preceding instruction left a deferred write behind).
static bool run_block(u32 adr, const u32 *words, int n, const u32 *ops, u32 cpsr, Result *out)
{
	for (int k = 0; k < n; k++)
		*(u32 *)(MMU.MAIN_MEM + ((adr + 4 * k) & _MMU_MAIN_MEM_MASK32)) = words[k];

	CommonSettings.jit_max_block_size = n;
	prime_cpu(adr, words[0], ops, cpsr);
	arm_jit_compile<0>();
	prime_cpu(adr, words[0], ops, cpsr);
	ArmOpCompiled f = (ArmOpCompiled)JIT_COMPILED_FUNC(adr, 0);
	CommonSettings.jit_max_block_size = 1;

	if (f == (ArmOpCompiled)interp_stub) {
		printf("FAIL %08X: block did not compile (fell back to the interpreter)\n", words[0]);
		return false;
	}
	out->cycles = f();
	memcpy(out->r, NDS_ARM9.R, sizeof(out->r));
	out->cpsr = NDS_ARM9.CPSR.val;
	return true;
}

// Thumb: 16-bit opcodes, and the block size is in instructions either way.
static bool run_thumb(u32 adr, const u16 *words, int n, const u32 *ops, u32 cpsr, Result *out)
{
	// _MMU_MAIN_MEM_MASK32 strips the low two bits (it is meant for aligned word
	// access), so the offset has to be computed once and stepped by two bytes --
	// masking adr and adr+2 separately sends both to the same byte.
	const u32 base = adr & _MMU_MAIN_MEM_MASK32;
	for (int k = 0; k < n; k++)
		*(u16 *)(MMU.MAIN_MEM + base + 2 * k) = words[k];

	CommonSettings.jit_max_block_size = n;
	g_thumb_pipe = true;
	prime_cpu(adr, words[0], ops, cpsr);
	if (getenv("PROBE_TRACE"))
		printf("TRACE adr=%08X ia=%08X w0=%04X w1=%04X max=%d n=%d words=%04X,%04X\n", adr,
		       NDS_ARM9.instruct_adr,
		       *(u16 *)(MMU.MAIN_MEM + base),
		       *(u16 *)(MMU.MAIN_MEM + base + 2),
		       (int)CommonSettings.jit_max_block_size, n, words[0], words[1]);
	arm_jit_compile<0>();
	prime_cpu(adr, words[0], ops, cpsr);
	ArmOpCompiled f = (ArmOpCompiled)JIT_COMPILED_FUNC(adr, 0);
	CommonSettings.jit_max_block_size = 1;
	g_thumb_pipe = false;

	if (f == (ArmOpCompiled)interp_stub) {
		printf("FAIL %04X: block did not compile (fell back to the interpreter)\n", words[0]);
		return false;
	}
	out->cycles = f();
	memcpy(out->r, NDS_ARM9.R, sizeof(out->r));
	out->cpsr = NDS_ARM9.CPSR.val;
	return true;
}

// Distinct sentinel in every banked register, so a case that banks the wrong
// value shows *which* bank got it. SPSR is pre-loaded because `mrs r0,spsr` and
// the exception entries both read it.
static void reset_banks()
{
	NDS_ARM9.SPSR.val  = 0xA5A50013u;
	NDS_ARM9.R13_usr   = 0x11111000u; NDS_ARM9.R14_usr = 0x11111111u;
	NDS_ARM9.R13_svc   = 0x22222000u; NDS_ARM9.R14_svc = 0x22222222u;
	NDS_ARM9.R13_abt   = 0x33333000u; NDS_ARM9.R14_abt = 0x33333333u;
	NDS_ARM9.R13_und   = 0x44444000u; NDS_ARM9.R14_und = 0x44444444u;
	NDS_ARM9.R13_irq   = 0x55555000u; NDS_ARM9.R14_irq = 0x55555555u;
	NDS_ARM9.R13_fiq   = 0x66666000u; NDS_ARM9.R14_fiq = 0x66666666u;
	NDS_ARM9.R8_fiq = 0x66666008u; NDS_ARM9.R9_fiq  = 0x66666009u;
	NDS_ARM9.R10_fiq = 0x6666600Au; NDS_ARM9.R11_fiq = 0x6666600Bu;
	NDS_ARM9.R12_fiq = 0x6666600Cu;
	NDS_ARM9.SPSR_svc.val = 0x77777000u; NDS_ARM9.SPSR_abt.val = 0x77777001u;
	NDS_ARM9.SPSR_und.val = 0x77777002u; NDS_ARM9.SPSR_irq.val = 0x77777003u;
	NDS_ARM9.SPSR_fiq.val = 0x77777004u;
}

//-----------------------------------------------------------------------------
//   Cases
//---------------------------------------------------------------------------

// ARM data-processing encodings, written as fields rather than magic constants so
// a wrong bit shows up as a wrong field. Verified against capstone.
#define MUL_OP(rd, rm, rs, s) \
	(0xE0000000u | ((u32)(s) << 20) | ((u32)(rd) << 16) | ((u32)(rs) << 8) | 0x90u | (u32)(rm))
#define MLA_OP(rd, rn, rm, rs, s) \
	(0xE0000000u | ((u32)(s) << 20) | 0x00200000u | ((u32)(rd) << 16) | ((u32)(rn) << 12) | \
	 ((u32)(rs) << 8) | 0x90u | (u32)(rm))
// op21: UMULL 4, UMLAL 5, SMULL 6, SMLAL 7.
#define MULL_OP(op21, hi, lo, rm, rs, s) \
	(0xE0000000u | ((u32)(s) << 20) | ((u32)(op21) << 21) | ((u32)(hi) << 16) | \
	 ((u32)(lo) << 12) | ((u32)(rs) << 8) | 0x90u | (u32)(rm))
// Signed halfword / word multiplies (ARMv5TE DSP extensions): bits 27..20 name
// the operation, bit 5 = x (top half of Rm), bit 6 = y (top half of Rs) --
// SMUL<x><y>, so 0xA1 ("x set, y clear") is smultb and 0xC1 is smulbt. Checked
// against capstone, which is also what the case names below say.
#define DSP_OP(op27_20, rd, rn, rm, rs, x, y) \
	(0xE0000000u | ((u32)(op27_20) << 20) | ((u32)(rd) << 16) | ((u32)(rn) << 12) | \
	 ((u32)(rs) << 8) | ((u32)(x) << 5) | ((u32)(y) << 6) | 0x80u | (u32)(rm))
#define CLZ_OP(rd, rm) (0xE16F0F10u | ((u32)(rd) << 16) | (u32)(rm))
// Saturating arithmetic. The field layout matches the data-processing group --
// bits 19-16 = Rn, 15-12 = Rd, 3-0 = Rm -- but the *syntax* is
// `op{cond} {Rd}, Rm, Rn`, so bits 3-0 hold the first source operand. That is
// the whole trap with this family: `QDADD r0, r1, r2` doubles R2, not R1.
// Bits 23-20 pick the operation (0 = QADD, 2 = QSUB, 4 = QDADD, 6 = QDSUB);
// bit 4 is always set. Checked against capstone.
#define QADD_OP(rn, rd, rm)  (0xE1000050u | ((u32)(rn) << 16) | ((u32)(rd) << 12) | (u32)(rm))
#define QSUB_OP(rn, rd, rm)  (0xE1200050u | ((u32)(rn) << 16) | ((u32)(rd) << 12) | (u32)(rm))
#define QDADD_OP(rn, rd, rm) (0xE1400050u | ((u32)(rn) << 16) | ((u32)(rd) << 12) | (u32)(rm))
#define QDSUB_OP(rn, rd, rm) (0xE1600050u | ((u32)(rn) << 16) | ((u32)(rd) << 12) | (u32)(rm))
// SMULW/SMLAW do not use the x/y halfword selectors: bit 5 is always set for
// SMULW and clear for SMLAW, and bit 6 picks Rs's top half.
#define SMULW_OP(rd, rm, rs, y) \
	(0xE12000A0u | ((u32)(y) << 6) | ((u32)(rd) << 16) | ((u32)(rs) << 8) | (u32)(rm))
#define SMLAW_OP(rd, rn, rm, rs, y) \
	(0xE1200080u | ((u32)(y) << 6) | ((u32)(rd) << 16) | ((u32)(rn) << 12) | \
	 ((u32)(rs) << 8) | (u32)(rm))

static const u32 kSetA[16] = {
	0x11111111u, 0x7FFFFFFFu, 0x00000002u, 0x12345678u,
	0x00000000u, 0x00000000u, 0xCAFEBABEu, 0x00000000u,
};
static const u32 kSetB[16] = {
	0x99999999u, 0x80000000u, 0xFFFFFFFFu, 0xFFFFFFFFu,
	0xFFFFFFFFu, 0x00000001u, 0x0BADF00Du, 0x00000000u,
};
static const u32 kSetC[16] = {
	0x00000000u, 0x00010001u, 0x00020002u, 0xFFFFFFFEu,
	0x7FFFFFFFu, 0x7FFFFFFFu, 0x0000FFFFu, 0x00000000u,
};
// Operand pairs for the saturating forms: Rn/Rm sit in R1/R2, and these three
// sets make the *signed* overflow actually happen (or not) where the case name
// says so.
static const u32 kQSat[16] = {0, 0x7FFFFFFFu, 0x00000002u};   // +ve + +ve overflows
static const u32 kQNeg[16] = {0, 0x80000000u, 0x00000001u};   // 1 - INT_MIN overflows
static const u32 kQDbl[16] = {0, 0x80000000u, 0x00000002u};   // doubling overflows

struct Case
{
	const char *name;
	u32 opcode;
	const u32 *ops;
	u32 cpsr;
	// Self-check on R[dst]. Only the cases whose arithmetic is short enough to
	// state without a helper are checked here; everything else is covered by
	// diffing the full dump against the pre-migration golden copy, which is the
	// actual acceptance criterion for a register-plumbing change.
	int dst;
	u32 want;
};

#define C(nm, op, set)            { nm, op, set, kCpsrInit, -1, 0 }
#define CF(nm, op, set, cs)       { nm, op, set, cs, -1, 0 }
#define CR(nm, op, set, d, w)     { nm, op, set, kCpsrInit, d, w }

static const u32 kCpsrFlags = 0x30000013u;   // supervisor, ARM, C and V set

static const Case kCases[] = {
	// --- 32-bit MUL / MLA, with and without flags -------------------------
	CR("mul r0, r1, r2",       MUL_OP(0, 1, 2, 0), kSetA, 0, 0xFFFFFFFEu),
	CR("mul r0, r1, r2 (neg)", MUL_OP(0, 1, 2, 0), kSetB, 0, 0x80000000u),
	CR("muls r0, r1, r2",      MUL_OP(0, 1, 2, 1), kSetA, 0, 0xFFFFFFFEu),
	CF("muls r0, r1, r2, C+V", MUL_OP(0, 1, 2, 1), kSetA, kCpsrFlags),
	CR("mla r0, r1, r2, r3",   MLA_OP(0, 3, 1, 2, 0), kSetA, 0, 0x12345676u),
	CR("mlas r0, r1, r2, r3",  MLA_OP(0, 3, 1, 2, 1), kSetA, 0, 0x12345676u),
	C ("mla r0, r1, r2, r3 (b)", MLA_OP(0, 3, 1, 2, 0), kSetB),

	// --- 64-bit results ----------------------------------------------------
	CR("umull r4, r5, r1, r2", MULL_OP(4, 5, 4, 1, 2, 0), kSetA, 4, 0xFFFFFFFEu),
	CR("umull r4, r5 (hi)",    MULL_OP(4, 5, 4, 1, 2, 0), kSetA, 5, 0x00000000u),
	CR("umull r4, r5 (b)",     MULL_OP(4, 5, 4, 1, 2, 0), kSetB, 4, 0x80000000u),
	CR("umull r4, r5 (b, hi)", MULL_OP(4, 5, 4, 1, 2, 0), kSetB, 5, 0x7FFFFFFFu),
	C ("umlal r4, r5, r1, r2", MULL_OP(5, 5, 4, 1, 2, 0), kSetC),
	CR("smull r4, r5 (b, hi)", MULL_OP(6, 5, 4, 1, 2, 0), kSetB, 5, 0x00000000u),
	CR("smull r4, r5 (b)",     MULL_OP(6, 5, 4, 1, 2, 0), kSetB, 4, 0x80000000u),
	CR("smlal r4, r5 (b)",     MULL_OP(7, 5, 4, 1, 2, 0), kSetB, 4, 0x7FFFFFFFu),
	C ("smlal (b, hi)",        MULL_OP(7, 5, 4, 1, 2, 0), kSetB),
	CR("umulls r4, r5",        MULL_OP(4, 5, 4, 1, 2, 1), kSetA, 4, 0xFFFFFFFEu),
	C ("smlals r4, r5 (b)",    MULL_OP(7, 5, 4, 1, 2, 1), kSetB),

	// --- signed halfword multiply / multiply-accumulate --------------------
	CR("smulbb r0, r1, r2",    DSP_OP(0x16, 0, 0, 1, 2, 0, 0), kSetA, 0, 0xFFFFFFFEu),
	CR("smultb r0, r1, r2",    DSP_OP(0x16, 0, 0, 1, 2, 1, 0), kSetA, 0, 0x0000FFFEu),
	CR("smulbt r0, r1, r2",    DSP_OP(0x16, 0, 0, 1, 2, 0, 1), kSetA, 0, 0x00000000u),
	C ("smultt r0, r1, r2",    DSP_OP(0x16, 0, 0, 1, 2, 1, 1), kSetA),
	CR("smlabb r0, r1, r2, r3", DSP_OP(0x10, 0, 3, 1, 2, 0, 0), kSetA, 0, 0x12345676u),
	C ("smlatb r0, r1, r2, r3", DSP_OP(0x10, 0, 3, 1, 2, 1, 0), kSetA),
	C ("smlabt r0, r1, r2, r3", DSP_OP(0x10, 0, 3, 1, 2, 0, 1), kSetA),
	C ("smlatt r0, r1, r2, r3", DSP_OP(0x10, 0, 3, 1, 2, 1, 1), kSetA),
	C ("smlalbb r4, r5, r1, r2", DSP_OP(0x14, 5, 4, 1, 2, 0, 0), kSetC),
	C ("smlaltt r4, r5, r1, r2", DSP_OP(0x14, 5, 4, 1, 2, 1, 1), kSetB),
	C ("smulwb r0, r1, r2",    SMULW_OP(0, 1, 2, 0), kSetA),
	C ("smulwt r0, r1, r2",    SMULW_OP(0, 1, 2, 1), kSetB),
	C ("smlawb r0, r1, r2, r3", SMLAW_OP(0, 3, 1, 2, 0), kSetC),
	C ("smlawt r0, r1, r2, r3", SMLAW_OP(0, 3, 1, 2, 1), kSetC),

	// --- CLZ (same batch: no flags, one source) ---------------------------
	CR("clz r0, r1",           CLZ_OP(0, 1), kSetA, 0, 1),
	CR("clz r0, r1 (b)",       CLZ_OP(0, 1), kSetB, 0, 0),
	CR("clz r0, r1 (c)",       CLZ_OP(0, 1), kSetC, 0, 15),

	// --- saturation and coprocessor (group E) ----------------------------
	// QADD/QSUB/QDADD/QDSUB used to return 0, i.e. the whole block fell back to
	// the interpreter; these are the first executions of the new inline code.
	// Names match the encoding: Rn = R1 (second operand), Rm = R2 (first), so
	// the disassembly reads `op r0, r2, r1`. run_qsat_model() below sweeps the
	// whole operand matrix against the ARM ARM pseudocode.
	CR("qadd r0,r2,r1 (sat)",  QADD_OP(1, 0, 2),  kQSat, 0, 0x7FFFFFFFu),
	CR("qadd r0,r2,r1",        QADD_OP(1, 0, 2),  kSetC, 0, 0x00030003u),
	CR("qsub r0,r2,r1 (sat)",  QSUB_OP(1, 0, 2),  kQNeg, 0, 0x7FFFFFFFu),
	CR("qdadd r0,r2,r1 (sat)", QDADD_OP(1, 0, 2), kQDbl, 0, 0x80000002u),
	CR("qdsub r0,r2,r1 (sat)", QDSUB_OP(1, 0, 2), kQDbl, 0, 0x7FFFFFFFu),
	C ("mrc p15,0,r0,c0,c0,0", 0xEE100F10u, kSetA),
	C ("mcr p15,0,r0,c1,c0,0", 0xEE010F10u, kSetA),
};

static const size_t kCaseCount = sizeof(kCases) / sizeof(kCases[0]);

// --- reference model for the saturating arithmetic --------------------------
// ARM DDI 0403E, "QADD/QSUB/QDADD/QDSUB, Operation" (the A32 and T32 encodings
// share this pseudocode; Rm is the first operand of `op{cond} {Rd}, Rm, Rn`,
// i.e. bits 3-0, and Rn is the second, bits 19-16):
//
//   QADD : (R[d], sat)  = SignedSatQ(SInt(R[m]) + SInt(R[n]), 32)
//   QSUB : (R[d], sat)  = SignedSatQ(SInt(R[m]) - SInt(R[n]), 32)
//   QDADD: (doubled,s1) = SignedSatQ(2 * SInt(R[n]), 32)
//          (R[d], s2)   = SignedSatQ(SInt(R[m]) + SInt(doubled), 32)
//   QDSUB: (doubled,s1) = SignedSatQ(2 * SInt(R[n]), 32)
//          (R[d], s2)   = SignedSatQ(SInt(R[m]) - SInt(doubled), 32)
//
// Note which register doubles: Rn, the *second* source operand. Reading the
// syntax as if it were SUB's `Rd, Rn, Rm` gets QDADD/QDSUB backwards.
// arm_instructions.cpp has this shape too (doubles bits 19-16, combines with
// bits 3-0), so this model and the interpreter should agree bit for bit --
// which is what run_qsat_model() checks, instead of only checking that the
// migration did not move the values.
static u32 satsq32(s64 v, bool &sat)
{
	if (v > 0x7FFFFFFFLL)  { sat = true; return 0x7FFFFFFFu; }
	if (v < -2147483648LL) { sat = true; return 0x80000000u; }
	sat = false;
	return (u32)(s32)v;
}

static u32 qop_ref(bool sub, bool dbl, u32 rm, u32 rn, bool &q)
{
	bool sat1 = false, sat2 = false;
	s64 n = (s32)rn;
	if (dbl)
		n = (s32)satsq32(2 * n, sat1);
	const s64 m = (s32)rm;
	const u32 r = satsq32(sub ? (m - n) : (m + n), sat2);
	if (sat1 || sat2)
		q = true;                       // CPSR.Q is sticky
	return r;
}

// 4 instructions x 64 operand pairs x 3 destination choices (Rd distinct, or
// aliased to either source -- the cache has to read both operands before it
// renames the slot).
static void run_qsat_model()
{
	static const u32 vals[8] = {
		0x00000000u, 0x00000001u, 0xFFFFFFFFu, 0x7FFFFFFFu,
		0x80000000u, 0x40000000u, 0xC0000000u, 0x12345678u,
	};
	static const struct { const char *nm; u32 base; bool sub, dbl; } ins[4] = {
		{ "qadd",  0xE1000050u, false, false },
		{ "qsub",  0xE1200050u, true,  false },
		{ "qdadd", 0xE1400050u, false, true  },
		{ "qdsub", 0xE1600050u, true,  true  },
	};

	int pass = 0, fail = 0, n = 0;
	CommonSettings.jit_max_block_size = 1;

	for (int t = 0; t < 4; t++)
		for (int rd = 0; rd < 3; rd++)          // 0 = fresh, 1 = Rd==Rn, 2 = Rd==Rm
			for (int i = 0; i < 8; i++)
				for (int j = 0; j < 8; j++) {
					u32 ops[16] = {0};
					ops[1] = vals[j];               // Rn, the doubled one
					ops[2] = vals[i];               // Rm
					const u32 op = ins[t].base | (1u << 16) | ((u32)rd << 12) | 2u;
					const u32 adr = kCodeBase + 0x50000u + (u32)n * 4u;
					n++;

					bool q = false;
					const u32 want = qop_ref(ins[t].sub, ins[t].dbl, vals[i], vals[j], q);

					Result res;
					if (!run_one(adr, op, ops, kCpsrInit, &res)) { fail++; continue; }
					const bool gotq = (res.cpsr & (1u << 27)) != 0;
					if (res.r[rd] != want || gotq != q) {
						printf("  FAIL %-5s r%d,r2,r1 (Rm=%08X Rn=%08X) R%d=%08X want=%08X Q=%d want=%d\n",
						       ins[t].nm, rd, vals[i], vals[j], rd, res.r[rd], want,
						       (int)gotq, (int)q);
						fail++;
					} else {
						pass++;
					}
				}

	printf("\nsaturating arithmetic: %d model checks passed, %d failed (%d blocks, vs ARM ARM pseudocode)\n",
	       pass, fail, n);
	g_pass += pass;
	g_fail += fail;
}

// --- marginal code size -------------------------------------------------
// `--marginal` compiles the same block twice, once with one extra guest
// instruction, and reports the byte difference. Prologue, epilogue and cycle
// accounting are identical in both compiles, so the delta is that
// instruction's marginal host cost. It is the arm64 counterpart of the x86
// harness's `--block --slot` measurement, and it runs on the device.
struct MarginalCase
{
	const char *name;
	bool thumb;
	u32 prefix[4];
	int nprefix;
	u32 target;
	u32 ops[16];
};

static int compile_size(u32 adr, const u32 *words, int n, const u32 *ops, bool thumb)
{
	const u32 base = adr & _MMU_MAIN_MEM_MASK32;
	for (int k = 0; k < n; k++)
	{
		if (thumb)
			*(u16 *)(MMU.MAIN_MEM + base + 2 * k) = (u16)words[k];
		else
			*(u32 *)(MMU.MAIN_MEM + base + 4 * k) = words[k];
	}

	CommonSettings.jit_max_block_size = n;
	g_thumb_pipe = thumb;
	prime_cpu(adr, words[0], ops, thumb ? kCpsrThumb : kCpsrInit);
	arm_jit_compile<0>();
	CommonSettings.jit_max_block_size = 1;
	g_thumb_pipe = false;

	if ((ArmOpCompiled)JIT_COMPILED_FUNC(adr, 0) == (ArmOpCompiled)interp_stub)
		return -1;
	return g_TOTALBYTES;
}

static int run_marginal()
{
	// Both compiles end with this cheap, non-branch instruction, so the
	// per-block "last instruction" handling cancels instead of being charged
	// to the target.
	const u32 kTailArm = 0xE1A00000u;   // mov r0,r0
	const u32 kTailThumb = 0x1C00u;     // adds r0,r0,#0

	static const MarginalCase list[] = {
		// ALU: the S forms pay for the flag export, the plain ones do not. The
		// pair is what makes "Thumb is more expensive" measurable instead of
		// anecdotal -- `adds` is the only form Thumb has.
		{ "arm  add r0,#1",                    false, {0}, 0, 0xE2800001u, {0} },
		{ "arm  adds r0,#1",                   false, {0}, 0, 0xE2900001u, {0} },
		{ "arm  cmp r0,#0",                    false, {0}, 0, 0xE3500000u, {0} },
		{ "arm  ldr r0,[r1,#4]",               false, {0}, 0, 0xE5910004u, {0, 0x02003000u} },
		{ "arm  ldr r0,[r1,r2]  R2 cold",      false, {0}, 0, 0xE7910002u, {0, 0x02003000u, 4} },
		{ "arm  ldr r0,[r1,r2]  R2 dirty",     false, {0xE2822004u}, 1, 0xE7910002u, {0, 0x02003000u} },
		{ "arm  ldr r0,[r1,r2,lsl #2] dirty",  false, {0xE2822001u}, 1, 0xE7910102u, {0, 0x02003000u, 0} },
		// Stores carry the DMA early-exit check, whose cold path is emitted
		// inline in every block that stores. Its size is part of this number
		// even though only three instructions of it ever execute.
		{ "arm  str r0,[r1,r2]  R2 dirty",     false, {0xE2822004u}, 1, 0xE7810002u, {0xDEADBEEFu, 0x02003000u} },
		{ "arm  add r0,r0,r2,lsl #2  dirty",   false, {0xE2822004u}, 1, 0xE0800102u, {0x11111111u} },
		{ "arm  ldrd r0,[r1,r2] R2 dirty",     false, {0xE2822004u}, 1, 0xE18100D2u, {0, 0x02003000u} },
		{ "arm  swp r0,r2,[r1]  R1 dirty",     false, {0xE2811004u}, 1, 0xE1010092u, {0, 0x02003000u, 0xCAFEBABEu} },
		{ "thumb adds r0,#1",                  true,  {0}, 0, 0x3001u, {0} },
		{ "thumb cmp r0,#0",                   true,  {0}, 0, 0x2800u, {0} },
		{ "thumb ldr r0,[r1,#4]",              true,  {0}, 0, 0x6848u, {0, 0x02003000u} },
		{ "thumb ldr r0,[r1,r2]  R2 dirty",    true,  {0x3200u}, 1, 0x5888u, {0, 0x02003000u, 4} },
	};

	u32 adr = kCodeBase + 0x60000u;
	int failed = 0;
	printf("# marginal host cost (block with target minus block without it)\n");
	for (size_t i = 0; i < sizeof(list) / sizeof(list[0]); i++)
	{
		const MarginalCase &c = list[i];
		u32 base_words[8] = {0}, full_words[8] = {0};
		int n = 0;
		for (int k = 0; k < c.nprefix; k++)
			base_words[n++] = c.prefix[k];
		memcpy(full_words, base_words, sizeof(u32) * n);
		full_words[n] = c.target;
		full_words[n + 1] = c.thumb ? kTailThumb : kTailArm;
		base_words[n] = c.thumb ? kTailThumb : kTailArm;

		const int base_size = compile_size(adr, base_words, n + 1, c.ops, c.thumb);
		adr += 0x100;
		const int full_size = compile_size(adr, full_words, n + 2, c.ops, c.thumb);
		adr += 0x100;

		if (base_size < 0 || full_size < 0)
		{
			printf("%-38s FAILED TO COMPILE\n", c.name);
			failed++;
			continue;
		}

		const int delta = full_size - base_size;
		printf("%-38s %4d bytes  %5.1f host insns   (%d -> %d)\n",
		       c.name, delta, delta / 4.0, base_size, full_size);
	}

	arm_jit_reset(true, true);
	return failed ? 1 : 0;
}

int main(int argc, char **argv)
{
	// --dump N prints the words of case N's block instead of running the suite.
	// The encoding layer is where a register-plumbing change is easiest to see.
	const int dump_case = (argc > 2 && strcmp(argv[1], "--dump") == 0) ? atoi(argv[2]) : -1;
	// Same for the psr/branch table below: --dump-psr N prints the words of case
	// N's block, which is how the order of the flush and the switchMode call gets
	// checked without reading the emitter.
	const int dump_psr = (argc > 2 && strcmp(argv[1], "--dump-psr") == 0) ? atoi(argv[2]) : -1;
	const int dump_mem = (argc > 2 && strcmp(argv[1], "--dump-mem") == 0) ? atoi(argv[2]) : -1;
	const int dump_t = (argc > 2 && strcmp(argv[1], "--dump-t") == 0) ? atoi(argv[2]) : -1;
	memset(&MMU, 0, sizeof(MMU));
	// A main-RAM mirror is compared against DTCMRegion with the low 14 bits
	// masked off; park it far away so no case address can alias into it.
	MMU.DTCMRegion = 0x0F000000;
	memset(&cp15, 0, sizeof(cp15));
	memset(&NDS_ARM7, 0, sizeof(NDS_ARM7));
	memset(&NDS_ARM9, 0, sizeof(NDS_ARM9));

	CommonSettings.jit_max_block_size = 1;   // one instruction per block
	arm_jit_reset(true, true);

	// `--marginal` is a measurement, not a test: it compiles two blocks per
	// entry and prints the byte delta, then exits.
	if (argc > 1 && strcmp(argv[1], "--marginal") == 0)
		return run_marginal();

		printf("# a64 block execution: %u single-instruction blocks\n", (unsigned)kCaseCount);

	if (dump_case >= 0) {
		if ((size_t)dump_case >= kCaseCount) {
			printf("no such case\n");
			return 2;
		}
		const Case &c = kCases[dump_case];
		Result res;
		if (!run_one(kCodeBase, c.opcode, c.ops, c.cpsr, &res))
			return 1;
		ArmOpCompiled f = (ArmOpCompiled)JIT_COMPILED_FUNC(kCodeBase, 0);
		printf("%s %08X  %d bytes\n", c.name, c.opcode, g_TOTALBYTES);
		for (int i = 0; i < g_TOTALBYTES; i += 4) {
			u32 w;
			memcpy(&w, (const u8 *)f + i, 4);
			printf(" %08X", w);
		}
		printf("\n");
		return 0;
	}

	for (size_t i = 0; i < kCaseCount; i++) {
		const Case &c = kCases[i];
		const u32 adr = kCodeBase + (u32)i * kCaseStride;
		Result res;
		if (!run_one(adr, c.opcode, c.ops, c.cpsr, &res)) {
			g_fail++;
			continue;
		}

		// Full architectural state, so a golden diff catches anything the
		// migration touches -- including flags the case did not ask about.
		printf("%-26s %08X cpsr=%08X cyc=%2u", c.name, c.opcode, res.cpsr, res.cycles);
		for (int r = 0; r < 15; r++)
			printf(" %08X", res.r[r]);
		printf("\n");

		if (c.dst >= 0) {
			if (res.r[c.dst] == c.want) {
				g_pass++;
			} else {
				printf("  FAIL %-24s R%d=%08X want=%08X\n", c.name, c.dst,
				       res.r[c.dst], c.want);
				g_fail++;
			}
		}
	}

	// Runs before the summary so its checks are counted with the rest.
	run_qsat_model();

	printf("\n%d self-checks passed, %d failed, %d trace stubs called\n",
	       g_pass, g_fail, g_interp_calls);

	// --- stores ------------------------------------------------------------
	// The store instructions are the only ones that run emit_dma_event_check,
	// and that check has two paths: fall through, or leave the block so the
	// transfer can run first. Both are exercised here because the pending flag
	// is under the probe's control -- and because the deferred write-back of a
	// migrated handler has to survive both (the check writes back inside one arm
	// of a branch).
	printf("\n");
	{
		struct StoreCase { const char *name; u32 opcode; u32 val; u32 adr; int dma; };
		static const StoreCase stores[] = {
			{ "str r0, [r1]", 0xE5810000u, 0xDEADBEEFu, 0x02001000u, 0 },
			{ "str r0, [r1], DMA armed", 0xE5810000u, 0xFEEDFACEu, 0x02001000u, 1 },
		};
		for (size_t s = 0; s < sizeof(stores) / sizeof(stores[0]); s++) {
			const StoreCase &c = stores[s];
			u32 ops[16] = {0};
			ops[0] = c.val;
			ops[1] = c.adr;
			const u32 adr = kCodeBase + (u32)(kCaseCount + s) * kCaseStride;

			*(u32 *)(MMU.MAIN_MEM + (c.adr & _MMU_MAIN_MEM_MASK32)) = 0;
			mmu_dma_event_pending = (u32)c.dma;

			Result res;
			bool ok = run_one(adr, c.opcode, ops, kCpsrInit, &res);
			mmu_dma_event_pending = 0;
			if (!ok) { g_fail++; continue; }

			u32 mem;
			memcpy(&mem, MMU.MAIN_MEM + (c.adr & _MMU_MAIN_MEM_MASK32), 4);

			// Falling through resumes at the next instruction; the armed path
			// has to report the same resume point through instruct_adr.
			const u32 resumed = NDS_ARM9.instruct_adr;
			printf("%-26s %08X cpsr=%08X cyc=%2u mem=%08X resume=%08X\n",
			       c.name, c.opcode, res.cpsr, res.cycles, mem, resumed);

			if (mem != c.val) {
				printf("  FAIL %-24s mem=%08X want=%08X\n", c.name, mem, c.val);
				g_fail++;
			} else if (c.dma && resumed != adr + 4) {
				printf("  FAIL %-24s resume=%08X want=%08X\n", c.name, resumed,
				       adr + 4);
				g_fail++;
			} else {
				g_pass++;
			}
		}
	}

	printf("\n%d checks passed, %d failed\n", g_pass, g_fail);

	// --- two-instruction block: deferred write followed by a store ----------
	// This is the shape that exposed the write-back bug in emit_dma_event_check.
	// `mul` leaves its result in a deregistered slot, `str` then runs the DMA
	// check, and that check used to flush() inside one arm of its branch -- which
	// cleared the compile-time dirty bits, so the block tail emitted no store at
	// all and R0 came out unmodified. The store deliberately does not touch R0,
	// otherwise the store handler's own eviction would hide the problem.
	printf("\n");
	{
		u32 ops[16] = {0};
		ops[1] = 0x7FFFFFFFu;      // Rm
		ops[2] = 0x00000002u;      // Rs
		ops[3] = 0xCAFEF00Du;      // value the store writes
		ops[4] = 0x02002000u;      // store address

		const u32 adr = kCodeBase + (u32)(kCaseCount + 8) * kCaseStride;
		*(u32 *)(MMU.MAIN_MEM + (adr & _MMU_MAIN_MEM_MASK32) + 0) = MUL_OP(0, 1, 2, 0);
		*(u32 *)(MMU.MAIN_MEM + (adr & _MMU_MAIN_MEM_MASK32) + 4) = 0xE5843000u; // str r3, [r4]
		*(u32 *)(MMU.MAIN_MEM + (ops[4] & _MMU_MAIN_MEM_MASK32)) = 0;

		CommonSettings.jit_max_block_size = 2;
		prime_cpu(adr, 0, ops, kCpsrInit);
		arm_jit_compile<0>();
		prime_cpu(adr, 0, ops, kCpsrInit);
		ArmOpCompiled f = (ArmOpCompiled)JIT_COMPILED_FUNC(adr, 0);
		CommonSettings.jit_max_block_size = 1;

		if (f == (ArmOpCompiled)interp_stub) {
			printf("FAIL mul+str: block did not compile\n");
			g_fail++;
		} else {
			const u32 cycles = f();
			u32 mem;
			memcpy(&mem, MMU.MAIN_MEM + (ops[4] & _MMU_MAIN_MEM_MASK32), 4);
			printf("%-26s cpsr=%08X cyc=%2u R0=%08X mem=%08X\n", "mul r0 + str r3,[r4]",
			       NDS_ARM9.CPSR.val, cycles, NDS_ARM9.R[0], mem);

			if (NDS_ARM9.R[0] != 0xFFFFFFFEu) {
				printf("  FAIL mul+str: R0=%08X want=FFFFFFFE (deferred write lost)\n",
				       NDS_ARM9.R[0]);
				g_fail++;
			} else if (mem != 0xCAFEF00Du) {
				printf("  FAIL mul+str: mem=%08X want=CAFEF00D\n", mem);
				g_fail++;
			} else {
				g_pass++;
			}
		}
	}

	// --- PSR / branch / exception ------------------------------------------
	// Batch 6. Three things are being checked here that the multiply batch
	// could not reach:
	//   * MRS/MRS write a general register, so they exercise rc_store() with a
	//     source that is not an emulated register;
	//   * MSR/SWI/BKPT call armcpu_switchMode(), which banks R13/R14/SPSR by
	//     reading armcpu_t -- a deferred write that has not landed yet gives the
	//     old bank the wrong value and the new bank the old one;
	//   * B/BL/BX/BLX report the branch target through instruct_adr.
	printf("\n# psr / branch / exception\n");

	static const u32 kOpsNone[16]  = {0};
	static const u32 kOpsMode[16]  = {0x00000012u};             // R0 = IRQ
	static const u32 kOpsFlags[16] = {0xF0000000u};             // R0 = top byte
	static const u32 kOpsBx[16]    = {0x02000301u};             // -> 0x02000300, Thumb
	static const u32 kOpsBlx[16]   = {0, 0, 0, 0x02000305u};    // R3 -> 0x02000304
	static const u32 kOpsAdd1[16]  = {0x02000200u};

	enum Chk { CHK_NONE, CHK_R0, CHK_R13, CHK_R14, CHK_R13_SVC, CHK_R13_ABT,
	           CHK_CPSR, CHK_SPSR, CHK_IA };

	struct PsrCase
	{
		const char *name;
		u32 w[2];
		int n;
		const u32 *ops;
		int chk;
		u32 want;
	};

	// Encodings, checked against capstone: MRS/MSR, B/BL (+8 from the pipeline
	// value), BX/BLX, SWI, BKPT and the two data-processing instructions the
	// two-instruction cases use to leave a dirty cache entry behind.
	static const PsrCase psr[] = {
		{ "mrs r0, cpsr",            {0xE10F0000u}, 1, kOpsNone,  CHK_R0,       0x00000013u },
		{ "mrs r0, spsr",            {0xE14F0000u}, 1, kOpsNone,  CHK_R0,       0xA5A50013u },
		{ "msr cpsr_f, r0",          {0xE128F000u}, 1, kOpsFlags, CHK_CPSR,     0xF0000013u },
		{ "msr spsr_f, r0",          {0xE168F000u}, 1, kOpsFlags, CHK_SPSR,     0xF0A50013u },
		{ "msr cpsr_c, r0 (-> irq)", {0xE121F000u}, 1, kOpsMode,  CHK_R13,      0x55555000u },
		{ "add sp,#4 ; msr cpsr_c",  {0xE28DD004u, 0xE121F000u}, 2, kOpsMode,  CHK_R13_SVC, 0x023FF004u },
		{ "b #+8",                   {0xEA000002u}, 1, kOpsNone,  CHK_IA,       0 },
		{ "bl #+8",                  {0xEB000002u}, 1, kOpsNone,  CHK_R14,      0 },
		{ "bx r0",                   {0xE12FFF10u}, 1, kOpsBx,    CHK_IA,       0x02000300u },
		{ "blx r3",                  {0xE12FFF33u}, 1, kOpsBlx,   CHK_R14,      0 },
		{ "add r0,#1 ; bx r0",       {0xE2800001u, 0xE12FFF10u}, 2, kOpsAdd1,  CHK_IA,       0x02000200u },
		{ "swi #0x123456",           {0xEF123456u}, 1, kOpsNone,  CHK_IA,       0x00000008u },
		{ "add sp,#4 ; swi #0",      {0xE28DD004u, 0xEF000000u}, 2, kOpsNone,  CHK_R13_SVC, 0x023FF004u },
		{ "bkpt #0",                 {0xE1200070u}, 1, kOpsNone,  CHK_IA,       0x0000000Cu },
		{ "add sp,#4 ; bkpt #0",     {0xE28DD004u, 0xE1200070u}, 2, kOpsNone,  CHK_R13_SVC, 0x023FF004u },
	};
	const size_t psr_count = sizeof(psr) / sizeof(psr[0]);

	for (size_t i = 0; i < psr_count; i++) {
		const PsrCase &c = psr[i];
		const u32 adr = kCodeBase + 0x20000u + (u32)i * kCaseStride;

		reset_banks();
		memset(NDS_ARM9.R, 0, sizeof(NDS_ARM9.R));
		NDS_ARM9.CPSR.val = 0;               // prime_cpu below sets the real one
		NDS_ARM9.SPSR.val = 0xA5A50013u;

		Result res;
		if (!run_block(adr, c.w, c.n, c.ops, kCpsrInit, &res)) {
			g_fail++;
			continue;
		}

		const u32 ia = NDS_ARM9.instruct_adr;
		const u32 ni = NDS_ARM9.next_instruction;

		if (dump_psr == (int)i) {
			ArmOpCompiled pf = (ArmOpCompiled)JIT_COMPILED_FUNC(adr, 0);
			printf("DUMP %s %d bytes:\n", c.name, g_TOTALBYTES);
			for (int k = 0; k < g_TOTALBYTES; k += 4) {
				u32 word;
				memcpy(&word, (const u8 *)pf + k, 4);
				printf(" %08X", word);
				if ((k / 4) % 8 == 7) printf("\n");
			}
			printf("\n");
		}

		printf("%-26s cyc=%2u cpsr=%08X spsr=%08X ia=%08X ni=%08X\n",
		       c.name, res.cycles, res.cpsr, NDS_ARM9.SPSR.val, ia, ni);
		printf("   r:");
		for (int r = 0; r < 15; r++)
			printf(" %08X", res.r[r]);
		printf("\n   bank: svc=%08X,%08X abt=%08X,%08X irq=%08X,%08X usr=%08X,%08X fiq=%08X,%08X sfc=%08X sabt=%08X sirq=%08X\n",
		       NDS_ARM9.R13_svc, NDS_ARM9.R14_svc, NDS_ARM9.R13_abt, NDS_ARM9.R14_abt,
		       NDS_ARM9.R13_irq, NDS_ARM9.R14_irq, NDS_ARM9.R13_usr, NDS_ARM9.R14_usr,
		       NDS_ARM9.R13_fiq, NDS_ARM9.R14_fiq, NDS_ARM9.SPSR_svc.val,
		       NDS_ARM9.SPSR_abt.val, NDS_ARM9.SPSR_irq.val);

		u32 got = 0;
		switch (c.chk) {
			case CHK_R0:       got = res.r[0]; break;
			case CHK_R13:      got = res.r[13]; break;
			case CHK_R14:      got = res.r[14]; break;
			case CHK_R13_SVC:  got = NDS_ARM9.R13_svc; break;
			case CHK_R13_ABT:  got = NDS_ARM9.R13_abt; break;
			case CHK_CPSR:     got = res.cpsr; break;
			case CHK_SPSR:     got = NDS_ARM9.SPSR.val; break;
			case CHK_IA:       got = ia; break;
			default: break;
		}
		// B/BL/BL's expected value depends on the block address, which the
		// table cannot spell out; compute it here instead.
		u32 want = c.want;
		if (c.chk == CHK_IA && want == 0) want = adr + 16;          // b/bl #+8
		if (c.chk == CHK_R14 && want == 0) want = adr + 4;          // bl/blx link

		if (c.chk != CHK_NONE) {
			if (got == want) {
				g_pass++;
			} else {
				printf("  FAIL %-22s got=%08X want=%08X\n", c.name, got, want);
				g_fail++;
			}
		}
	}

	// --- memory access ------------------------------------------------------
	// Group C. The single-instruction cases pin the addressing modes; the
	// two-instruction ones are the point -- a preceding ALU instruction leaves
	// the base or the value in a cache slot, and every helper here works on
	// armcpu_t (or on a register-number list) rather than on the cache.
	printf("\n# memory access\n");
	{
		struct MemCase
		{
			const char *name;
			u32 w[2];
			int n;
			u32 ops[8];      // R0..R7
			u32 mem[2];      // primed at kDataAdr (+0, +4)
			int reg_chk;     // -1 = none
			u32 reg_want;
			int mem_chk;     // -1 = none, index into the primed pair
			u32 mem_want;
		};

		const u32 kDataAdr = 0x02003000u;
		// R1 = kDataAdr unless a case says otherwise; R0 is the store value.
		#define B kDataAdr
		#define D 0xDEADBEEFu

		static const MemCase m[] = {
			{ "ldr r0,[r1,#4]",       {0xE5910004u}, 1, {0,B},      {0x11223344u,0x55667788u}, 0, 0x55667788u, -1, 0 },
			// PC-relative: the base is R15, which never enters the cache, and
			// the value at pc-8 is the instruction itself.
			{ "ldr r0,[pc,#-8]",      {0xE51F0008u}, 1, {0,0},      {0,0},                      0, 0xE51F0008u, -1, 0 },
			{ "ldr r0,[r1,#-4]",      {0xE5110004u}, 1, {0,0x02003004u}, {0x11223344u,0x55667788u}, 0, 0x11223344u, -1, 0 },
			{ "ldr r0,[r1,r2]",       {0xE7910002u}, 1, {0,B,4},    {0x11223344u,0x55667788u}, 0, 0x55667788u, -1, 0 },
			{ "ldr r0,[r1,#4]!",      {0xE5B10004u}, 1, {0,B},      {0x11223344u,0x55667788u}, 1, 0x02003004u, -1, 0 },
			{ "ldr r0,[r1],#4",       {0xE4910004u}, 1, {0,B},      {0x11223344u,0x55667788u}, 1, 0x02003004u, -1, 0 },
			{ "str r0,[r1,#4]",       {0xE5810004u}, 1, {D,B},      {0,0},                      -1, 0,           1, D },
			{ "str r0,[r1],#4",       {0xE4810004u}, 1, {D,B},      {0,0},                      1, 0x02003004u,   0, D },
			{ "str r0,[r1,#4]!",      {0xE5A10004u}, 1, {D,B},      {0,0},                      1, 0x02003004u,   1, D },
			{ "ldrb r2,[r1,#1]",      {0xE5D12001u}, 1, {0,B},      {0x0000AB00u,0},            2, 0x000000ABu,  -1, 0 },
			{ "ldrsb r2,[r1,#1]",     {0xE1D120D1u}, 1, {0,B},      {0x00008000u,0},            2, 0xFFFFFF80u,  -1, 0 },
			{ "ldrh r2,[r1,#2]",      {0xE1D120B2u}, 1, {0,B},      {0xBEEF0000u,0},            2, 0x0000BEEFu,  -1, 0 },
			{ "ldrsh r2,[r1,#2]",     {0xE1D120F2u}, 1, {0,B},      {0x80000000u,0},            2, 0xFFFF8000u,  -1, 0 },
			{ "strh r0,[r1,#2]",      {0xE1C100B2u}, 1, {D,B},      {0xAAAAAAAAu,0},            -1, 0,           0, 0xBEEFAAAAu },
			{ "strb r0,[r1,#1]",      {0xE5C10001u}, 1, {D,B},      {0xAAAAAAAAu,0},            -1, 0,           0, 0xAAAAEFAAu },
			{ "strd r2,[r1]",         {0xE1C120F0u}, 1, {0,B,0xAABBCCDDu,0x11223344u}, {0,0},      -1, 0,           0, 0xAABBCCDDu },
			{ "strd r2,[r1] (hi)",    {0xE1C120F0u}, 1, {0,B,0xAABBCCDDu,0x11223344u}, {0,0},      -1, 0,           1, 0x11223344u },
			{ "ldrd r2,[r1]",         {0xE1C120D0u}, 1, {0,B},      {0x11223344u,0x55667788u},  2, 0x11223344u,  -1, 0 },
			{ "ldrd r2,[r1] (hi)",    {0xE1C120D0u}, 1, {0,B},      {0x11223344u,0x55667788u},  3, 0x55667788u,  -1, 0 },
			{ "swp r0,r2,[r1]",       {0xE1010092u}, 1, {0,B,0xCAFEBABEu}, {0x12345678u,0},    0, 0x12345678u,   0, 0xCAFEBABEu },
			{ "swpb r0,r2,[r1]",      {0xE1410092u}, 1, {0,B,0xCAFEBABEu}, {0x00000078u,0},    0, 0x00000078u,   0, 0x000000BEu },
			{ "stmia r0,{r1,r2,r3}",  {0xE880000Eu}, 1, {kDataAdr,1,2,3}, {0,0},                  -1, 0,           0, 1 },
			{ "stmia r0 (hi)",        {0xE880000Eu}, 1, {kDataAdr,1,2,3}, {0,0},                  -1, 0,           1, 2 },
			{ "ldmia r0,{r1,r2,r3}",  {0xE890000Eu}, 1, {kDataAdr}, {0x11u,0x22u},              2, 0x22u,         -1, 0 },
			{ "stmdb r0!,{r1,r2}",    {0xE9200006u}, 1, {0x02003008u,1,2}, {0,0},               0, 0x02003000u,   0, 1 },
			{ "ldmia r0!,{r1,r2}",    {0xE8B00006u}, 1, {kDataAdr}, {0x11u,0x22u},              0, 0x02003008u,   -1, 0 },
			// The other three dir/before combinations, so all four code paths
			// through op_ldm_stm/stm are executed at least once.
			{ "ldmdb r0,{r1,r2}",     {0xE9100006u}, 1, {0x02003008u}, {0x11u,0x22u},          2, 0x22u,         -1, 0 },
			{ "stmib r0!,{r1,r2}",    {0xE9A00006u}, 1, {kDataAdr,1,2}, {0,0},                   -1, 0,           1, 1 },
			// The DA forms are pinned but not asserted: their effective
			// addresses follow desmume's own convention rather than the ARM
			// ARM's, and settling that needs the interpreter as the oracle --
			// which this probe cannot link. Both builds agree, which is what
			// this batch has to show.
			{ "ldmda r0,{r1,r2}",     {0xE8100006u}, 1, {kDataAdr}, {0x11u,0x22u},              -1, 0,           -1, 0 },
			{ "stmda r0,{r1,r2}",     {0xE8000006u}, 1, {0x02003008u,1,2}, {0,0},               -1, 0,           -1, 0 },

			// --- two instructions: the producer leaves a slot dirty ----------
			{ "add r1,#4 ; ldr r0,[r1]",  {0xE2811004u,0xE5910000u}, 2, {0,B}, {0x11223344u,0x55667788u}, 0, 0x55667788u, -1, 0 },
			{ "add r1,#4 ; str r0,[r1]",  {0xE2811004u,0xE5810000u}, 2, {D,B}, {0,0},                     -1, 0,           1, D },
			{ "add r0,#1 ; str r0,[r1]",  {0xE2800001u,0xE5810000u}, 2, {0x11111110u,B}, {0,0},            -1, 0,           0, 0x11111111u },
			// The one that was wrong before this batch: STM reads cpu->R[r4]
			// straight out of armcpu_t, so a dirty slot stored the stale value.
			{ "add r4,#1 ; stmia {r4}",   {0xE2844001u,0xE8800010u}, 2, {kDataAdr,0,0,0,0x40000000u}, {0,0}, -1, 0,  0, 0x40000001u },
			{ "add r4,#1 ; stmdb r0!",    {0xE2844001u,0xE9200010u}, 2, {kDataAdr + 4,0,0,0,0x40000000u}, {0,0}, -1, 0, 0, 0x40000001u },
			{ "add r1,#4 ; ldmia {r2}",   {0xE2811004u,0xE8B10004u}, 2, {0,B}, {0x11223344u,0x55667788u}, 2, 0x55667788u, -1, 0 },
			// Register-offset forms where Rm is the result of the previous
			// instruction. Before the cache-aware Rm change these evicted the
			// dirty slot to memory and loaded it straight back; the values
			// below are the same either way, which is the point.
			{ "add r2,#4 ; ldr [r1,r2]",  {0xE2822004u,0xE7910002u}, 2, {0,B,0}, {0x11223344u,0x55667788u}, 0, 0x55667788u, -1, 0 },
			{ "add r2,#1 ; ldr [r1,r2,lsl#2]", {0xE2822001u,0xE7910102u}, 2, {0,B,0}, {0x11223344u,0x55667788u}, 0, 0x55667788u, -1, 0 },
			{ "add r2,#4 ; str [r1,r2]",  {0xE2822004u,0xE7810002u}, 2, {D,B,0}, {0,0},                      -1, 0,           1, D },
			{ "add r2,#4 ; add r0,r0,r2,lsl#2", {0xE2822004u,0xE0800102u}, 2, {0x11111111u,B,0}, {0,0},        0, 0x11111121u,  -1, 0 },
			// LDRD/STRD and SWP took the same treatment later: Rm (and SWP's
			// base) are plain reads, so they come out of a slot instead of being
			// evicted and reloaded. The pair/base write-backs still go through
			// armcpu_t, which is why Rd stays on the memory path.
			{ "add r2,#4 ; ldrd r0,[r1,r2]",  {0xE2822004u,0xE18100D2u}, 2, {0,B,0}, {0x11223344u,0x55667788u}, 0, 0x55667788u, -1, 0 },
			{ "add r2,#4 ; strd r0,[r1,r2]",  {0xE2822004u,0xE18100F2u}, 2, {D,B,0}, {0,0},                      -1, 0,           1, D },
			{ "add r4,#4 ; ldrd r2,[r1,r4]!", {0xE2844004u,0xE1A120D4u}, 2, {0,B,0}, {0x11223344u,0x55667788u}, 2, 0x55667788u, -1, 0 },
			{ "add r1,#4 ; swp r0,r2,[r1]",   {0xE2811004u,0xE1010092u}, 2, {0,B,0xCAFEBABEu}, {0x11223344u,0x55667788u}, 0, 0x55667788u, 1, 0xCAFEBABEu },
			// Base in the list: the write-back value is the one ARM leaves
			// unspecified. Printed, not asserted -- it needs the interpreter as
			// the oracle, which this probe cannot link (see the doc note).
			{ "ldmia r0!,{r0,r1}",        {0xE8B00003u}, 1, {kDataAdr}, {0x11u,0x22u},              -1, 0,           -1, 0 },
		};
		#undef B
		#undef D

		for (size_t i = 0; i < sizeof(m) / sizeof(m[0]); i++) {
			const MemCase &c = m[i];
			const u32 adr = kCodeBase + 0x30000u + (u32)i * kCaseStride;
			u32 ops[16] = {0};
			memcpy(ops, c.ops, sizeof(c.ops));

			memset(MMU.MAIN_MEM + (kDataAdr & _MMU_MAIN_MEM_MASK), 0, 0x40);
			for (int k = 0; k < 2; k++)
				memcpy(MMU.MAIN_MEM + ((kDataAdr + 4 * k) & _MMU_MAIN_MEM_MASK32), &c.mem[k], 4);

			Result res;
			if (!run_block(adr, c.w, c.n, ops, kCpsrInit, &res)) {
				g_fail++;
				continue;
			}

			u32 got[2];
			memcpy(&got[0], MMU.MAIN_MEM + (kDataAdr & _MMU_MAIN_MEM_MASK32), 4);
			memcpy(&got[1], MMU.MAIN_MEM + ((kDataAdr + 4) & _MMU_MAIN_MEM_MASK32), 4);

			printf("%-26s cyc=%2u cpsr=%08X R0=%08X R1=%08X R2=%08X R3=%08X mem=%08X,%08X\n",
			       c.name, res.cycles, res.cpsr, res.r[0], res.r[1], res.r[2], res.r[3],
			       got[0], got[1]);

			if ((int)i == dump_mem) {
				ArmOpCompiled pf = (ArmOpCompiled)JIT_COMPILED_FUNC(adr, 0);
				printf("DUMP %s %d bytes:\n", c.name, g_TOTALBYTES);
				for (int k = 0; k < g_TOTALBYTES; k += 4) {
					u32 word;
					memcpy(&word, (const u8 *)pf + k, 4);
					printf(" %08X", word);
					if ((k / 4) % 8 == 7) printf("\n");
				}
				printf("\n");
			}

			if (c.reg_chk >= 0) {
				if (res.r[c.reg_chk] == c.reg_want) g_pass++;
				else { printf("  FAIL %-22s R%d=%08X want=%08X\n", c.name, c.reg_chk,
				              res.r[c.reg_chk], c.reg_want); g_fail++; }
			}
			if (c.mem_chk >= 0) {
				if (got[c.mem_chk] == c.mem_want) g_pass++;
				else { printf("  FAIL %-22s mem[%d]=%08X want=%08X\n", c.name, c.mem_chk,
				              got[c.mem_chk], c.mem_want); g_fail++; }
			}
		}
	}

	// --- Thumb --------------------------------------------------------------
	// Group E. The Thumb handlers are the same cache migration as the ARM ones,
	// but every operand sits in a 3-bit field and the "special" forms carry the
	// fourth bit outside it, so the field extraction is where a mistake would
	// hide. Single-instruction blocks have no deferred write to lose, so they
	// must come out byte-identical to the pre-migration build; the handful of
	// two-instruction blocks at the end are the ones that can actually differ.
	printf("\n# thumb\n");
	{
		// Thumb encodings as fields, verified against capstone.
		#define T_LSL(imm,rm,rd)    (0x0000u | ((u32)(imm)<<6) | ((u32)(rm)<<3) | (u32)(rd))
		#define T_LSR(imm,rm,rd)    (0x0800u | ((u32)(imm)<<6) | ((u32)(rm)<<3) | (u32)(rd))
		#define T_ASR(imm,rm,rd)    (0x1000u | ((u32)(imm)<<6) | ((u32)(rm)<<3) | (u32)(rd))
		#define T_ROP(base,rm,rd)   ((base) | ((u32)(rm)<<3) | (u32)(rd))
		// Format 3 arithmetic is `bits 8-6 = Rm, bits 5-3 = Rn, bits 2-0 = Rd`
		// (sub is the one where the order shows: it is Rn - Rm).
		#define T_ADD_R(rm,rn,rd)   (0x1800u | ((u32)(rm)<<6) | ((u32)(rn)<<3) | (u32)(rd))
		#define T_SUB_R(rm,rn,rd)   (0x1A00u | ((u32)(rm)<<6) | ((u32)(rn)<<3) | (u32)(rd))
		#define T_ADD_I3(imm,rn,rd) (0x1C00u | ((u32)(imm)<<6) | ((u32)(rn)<<3) | (u32)(rd))
		#define T_SUB_I3(imm,rn,rd) (0x1E00u | ((u32)(imm)<<6) | ((u32)(rn)<<3) | (u32)(rd))
		#define T_IMM8(base,rd,imm) ((base) | ((u32)(rd)<<8) | (u32)(imm))
		#define T_SPE(base,h1,h2,rm,rd) ((base) | ((u32)(h1)<<7) | ((u32)(h2)<<6) | ((u32)(rm)<<3) | (u32)(rd))
		#define T_MEM_R(base,rm,rn,rd) ((base) | ((u32)(rm)<<6) | ((u32)(rn)<<3) | (u32)(rd))
		#define T_MEM_I(base,imm,rn,rd) ((base) | ((u32)(imm)<<6) | ((u32)(rn)<<3) | (u32)(rd))
		#define T_SPREL(base,rd,imm) ((base) | ((u32)(rd)<<8) | (u32)(imm))
		#define T_LDM(base,rn,lst)  ((base) | ((u32)(rn)<<8) | (u32)(lst))

		static const u32 tA[16]   = {0x11111111u, 0x7FFFFFFFu, 0x00000002u, 0x12345678u, 0,0,0xCAFEBABEu, 0};
		static const u32 tB[16]   = {0x99999999u, 0x80000000u, 0xFFFFFFFFu, 0xFFFFFFFFu, 0,0,0x0BADF00Du, 0};
		static const u32 tC[16]   = {0x00000000u, 0x00010001u, 0x00020002u, 0xFFFFFFFEu, 0,0,0x0000FFFFu, 0};
		static const u32 tMem[16] = {0xDEADBEEFu, 0x02003000u, 0x00000004u, 0};
		// Same window, but Rm = 5 so LDRSB reaches the 0x80 byte at +5.
		static const u32 tMemB[16] = {0xDEADBEEFu, 0x02003000u, 0x00000005u, 0};
		static const u32 tLdm[16] = {0x02003000u, 0x00000011u, 0x00000022u, 0x00000033u, 0};
		static const u32 tBx[16]  = {0, 0x02000301u};

		const u32 kStackTop = 0x023FF000u;

		struct ThumbCase
		{
			const char *name;
			u16 w[2];
			int n;
			const u32 *ops;
			u32 cpsr;
			int chk;        // -1 none, 0..15 register, 16 instruct_adr
			u32 want;
		};

		#define TA(nm, op, set)              { nm, {(u16)(op)}, 1, set, kCpsrThumb, -1, 0 }
		#define TX(nm, op, set, d, w)        { nm, {(u16)(op)}, 1, set, kCpsrThumb, d, w }
		#define TF(nm, op, set, cs)          { nm, {(u16)(op)}, 1, set, cs, -1, 0 }
		#define T2(nm, a, b, set)            { nm, {(u16)(a), (u16)(b)}, 2, set, kCpsrThumb, -1, 0 }
		#define T2X(nm, a, b, set, d, w)     { nm, {(u16)(a), (u16)(b)}, 2, set, kCpsrThumb, d, w }

		static const ThumbCase tc[] = {
			// --- shifts -----------------------------------------------------
			TX("lsl r0,r1,#4",     T_LSL(4,1,0), tA, 0, 0xFFFFFFF0u),
			TX("lsl r0,r0,#4",     T_LSL(4,0,0), tA, 0, 0x11111110u),
			TX("lsr r0,r1,#4",     T_LSR(4,1,0), tA, 0, 0x07FFFFFFu),
			TA("lsr r0,r1,#0",     T_LSR(0,1,0), tA),
			TX("asr r0,r2,#4",     T_ASR(4,2,0), tA, 0, 0x00000000u),
			TA("asr r0,r1,#0",     T_ASR(0,1,0), tA),
			TX("lsls r0,r2",       T_ROP(0x4080,2,0), tA, 0, 0x44444444u),
			TX("lsrs r0,r2",       T_ROP(0x40C0,2,0), tA, 0, 0x04444444u),
			TX("asrs r0,r2",       T_ROP(0x4100,2,0), tA, 0, 0x04444444u),
			TX("rors r0,r2",       T_ROP(0x41C0,2,0), tA, 0, 0x44444444u),

			// --- logical ----------------------------------------------------
			TX("ands r0,r2",       T_ROP(0x4000,2,0), tA, 0, 0x00000000u),
			TX("eors r0,r2",       T_ROP(0x4040,2,0), tA, 0, 0x11111113u),
			TX("orrs r0,r2",       T_ROP(0x4300,2,0), tA, 0, 0x11111113u),
			TX("bics r0,r2",       T_ROP(0x4380,2,0), tA, 0, 0x11111111u),
			TX("mvns r0,r2",       T_ROP(0x43C0,2,0), tA, 0, 0xFFFFFFFDu),
			TA("tst r0,r2",        T_ROP(0x4200,2,0), tA),
			TA("adcs r0,r2",       T_ROP(0x4140,2,0), tA),
			TA("sbcs r0,r2",       T_ROP(0x4180,2,0), tA),
			TX("muls r0,r2",       T_ROP(0x4340,2,0), tA, 0, 0x22222222u),
			TX("rsbs r0,r2,#0",    T_ROP(0x4240,2,0), tA, 0, 0xFFFFFFFEu),
			TA("cmp r0,r2",        T_ROP(0x4280,2,0), tA),
			TA("cmn r0,r2",        T_ROP(0x42C0,2,0), tA),

			// --- add/sub ----------------------------------------------------
			TX("adds r0,r1,r2",    T_ADD_R(2,1,0), tA, 0, 0x80000001u),
			TX("adds r0,r0,r2",    T_ADD_R(2,0,0), tA, 0, 0x11111113u),
			TX("adds r0,r1,r0",    T_ADD_R(0,1,0), tA, 0, 0x91111110u),
			TX("subs r0,r1,r2",    T_SUB_R(2,1,0), tA, 0, 0x7FFFFFFDu),
			TX("subs r0,r0,r2",    T_SUB_R(2,0,0), tA, 0, 0x1111110Fu),
			TX("adds r0,r1,#3",    T_ADD_I3(3,1,0), tA, 0, 0x80000002u),
			TX("adds r0,r1,#0",    T_ADD_I3(0,1,0), tA, 0, 0x7FFFFFFFu),
			TX("subs r0,r1,#3",    T_SUB_I3(3,1,0), tA, 0, 0x7FFFFFFCu),
			TX("movs r0,#0x5a",    T_IMM8(0x2000,0,0x5A), tA, 0, 0x0000005Au),
			TA("cmp r1,#0x5a",     T_IMM8(0x2800,1,0x5A), tA),
			TX("adds r1,#0x5a",    T_IMM8(0x3000,1,0x5A), tA, 1, 0x80000059u),
			TX("subs r1,#0x5a",    T_IMM8(0x3800,1,0x5A), tA, 1, 0x7FFFFFA5u),

			// --- high registers / PC / SP -----------------------------------
			TX("add r8,r0",        T_SPE(0x4400,1,0,0,0), tA, 8, 0x11111111u),
			TX("add r8,r1",        T_SPE(0x4400,1,0,1,0), tA, 8, 0x7FFFFFFFu),
			TA("cmp r0,r1",        T_SPE(0x4500,0,0,1,0), tA),
			TX("mov r8,r1",        T_SPE(0x4600,1,0,1,0), tA, 8, 0x7FFFFFFFu),
			TX("mov r0,r8",        T_SPE(0x4600,0,1,0,0), tA, 0, 0x00000000u),
			TA("adr r0,#0x10",     T_SPREL(0xA000,0,0x10), tA),
			TX("add r0,sp,#0x10",  T_SPREL(0xA800,0,0x10), tA, 0, kStackTop + 0x40u),
			TX("add sp,#0x20",     0xB000u | 8, tA, 13, kStackTop + 0x20u),
			TX("sub sp,#0x20",     0xB080u | 8, tA, 13, kStackTop - 0x20u),

			// --- loads / stores ---------------------------------------------
			// Data window: +0 = 11223344, +4 = 80008000, +8 = 0000AB00,
			// +12 = 00008000. Halfword and byte loads use offsets 1/2/4 so the
			// sign-extension cases are worth asserting.
			TX("str r0,[r1,#4]",   T_MEM_I(0x6000,1,1,0), tMem, -1, 0),
			TX("ldr r0,[r1,#4]",   T_MEM_I(0x6800,1,1,0), tMem, 0, 0x80008000u),
			TX("strb r0,[r1,#1]",  T_MEM_I(0x7000,1,1,0), tMem, -1, 0),
			TX("ldrb r0,[r1,#1]",  T_MEM_I(0x7800,1,1,0), tMem, 0, 0x00000033u),
			TX("strh r0,[r1,#2]",  T_MEM_I(0x8000,1,1,0), tMem, -1, 0),
			TX("ldrh r0,[r1,#2]",  T_MEM_I(0x8800,1,1,0), tMem, 0, 0x00001122u),
			TX("str r0,[r1,r2]",   T_MEM_R(0x5000,2,1,0), tMem, -1, 0),
			TX("strh r0,[r1,r2]",  T_MEM_R(0x5200,2,1,0), tMem, -1, 0),
			TX("strb r0,[r1,r2]",  T_MEM_R(0x5400,2,1,0), tMem, -1, 0),
			TX("ldr r0,[r1,r2]",   T_MEM_R(0x5800,2,1,0), tMem, 0, 0x80008000u),
			TX("ldrh r0,[r1,r2]",  T_MEM_R(0x5A00,2,1,0), tMem, 0, 0x00008000u),
			TX("ldrb r0,[r1,r2]",  T_MEM_R(0x5C00,2,1,0), tMem, 0, 0x00000000u),
			TX("ldrsb r0,[r1,r2]", T_MEM_R(0x5600,2,1,0), tMemB, 0, 0xFFFFFF80u),
			TX("ldrsh r0,[r1,r2]", T_MEM_R(0x5E00,2,1,0), tMem, 0, 0xFFFF8000u),
			TX("str r0,[sp,#8]",   T_SPREL(0x9000,0,2), tMem, -1, 0),
			TX("ldr r0,[sp,#8]",   T_SPREL(0x9800,0,2), tMem, 0, 0xB2B2B2B2u),
			TA("ldr r0,[pc,#0x20]", T_SPREL(0x4800,0,8), tA),

			// --- block transfers --------------------------------------------
			TA("push {r0,r1}",     0xB403u, tA),
			TA("push {r0,lr}",     0xB501u, tA),
			TX("pop {r0,r1}",      0xBC03u, tA, 0, 0xB0B0B0B0u),
			TA("pop {pc}",         0xBD01u, tA),
			TA("stm r0!,{r1,r2,r3}", T_LDM(0xC000,0,0x0E), tLdm),
			TX("ldm r0!,{r1,r2,r3}", T_LDM(0xC800,0,0x0E), tLdm, 1, 0x11223344u),

			// --- branch ------------------------------------------------------
			TA("bx r1",            0x4708u, tBx),
			TA("blx r1",           0x4788u, tBx),
			TA("b #8",             0xE002u, tA),
			TF("beq #8 (Z set)",   0xD002u, tA, kCpsrThumbZ),
			TA("svc #0x12",        0xDF12u, tA),
			T2("bl #8",            0xF001u, 0xF810u, tA),
			T2("blx #8",           0xF001u, 0xE810u, tA),

			// --- two instructions: a deferred write has to survive ----------
			T2("mov r0,#5 ; str [r1]",  0x2005u, 0x6008u, tMem),
			T2("adds r0,#1 ; str [r1]", 0x3001u, 0x6008u, tMem),
			T2("adds r0,#1 ; push {r0}", 0x3001u, 0xB401u, tA),
			T2("sub sp,#8 ; push {r0}", 0xB082u, 0xB401u, tA),
			T2("adds r1,#4 ; ldr r0,[r1]", 0x3104u, 0x6808u, tMem),
			// Same dirty-Rm case as the ARM table above: R2 is written by the
			// first instruction and read as the LDR/STR offset by the second.
			T2X("adds r2,#0 ; ldr r0,[r1,r2]", 0x3200u, 0x5888u, tMem, 0, 0x80008000u),
			T2("adds r2,#0 ; str r0,[r1,r2]",  0x3200u, 0x5088u, tMem),
		};

		// Stack window: PUSH writes below SP, POP and the SP-relative forms read
		// from SP up. Six words cover both, and the sentinels make "which slot
		// got the value" readable.
		const u32 kStackWin = 0x023FEFF8u;
		for (size_t i = 0; i < sizeof(tc) / sizeof(tc[0]); i++) {
			const ThumbCase &c = tc[i];
			const u32 adr = kCodeBase + 0x40000u + (u32)i * kCaseStride;

			// Data window at kDataAdr, stack window around SP.
			for (u32 k = 0; k < 0x40; k += 4)
				*(u32 *)(MMU.MAIN_MEM + ((0x02003000u + k) & _MMU_MAIN_MEM_MASK32)) = 0;
			*(u32 *)(MMU.MAIN_MEM + ((0x02003000u + 0) & _MMU_MAIN_MEM_MASK32)) = 0x11223344u;
			*(u32 *)(MMU.MAIN_MEM + ((0x02003000u + 4) & _MMU_MAIN_MEM_MASK32)) = 0x80008000u;
			*(u32 *)(MMU.MAIN_MEM + ((0x02003000u + 8) & _MMU_MAIN_MEM_MASK32)) = 0x0000AB00u;
			*(u32 *)(MMU.MAIN_MEM + ((0x02003000u + 12) & _MMU_MAIN_MEM_MASK32)) = 0x00008000u;

			const u32 stk[6] = {0xA1A1A1A1u, 0xA2A2A2A2u, 0xB0B0B0B0u,
			                    0xB1B1B1B1u, 0xB2B2B2B2u, 0xB3B3B3B3u};
			for (int k = 0; k < 6; k++)
				*(u32 *)(MMU.MAIN_MEM + ((kStackWin + 4 * k) & _MMU_MAIN_MEM_MASK32)) = stk[k];

			Result res;
			if (!run_thumb(adr, c.w, c.n, c.ops, c.cpsr, &res)) {
				g_fail++;
				continue;
			}

			const u32 ia = NDS_ARM9.instruct_adr;
			const u32 ni = NDS_ARM9.next_instruction;

			if ((int)i == dump_t) {
				ArmOpCompiled pf = (ArmOpCompiled)JIT_COMPILED_FUNC(adr, 0);
				printf("DUMP %s %d bytes:\n", c.name, g_TOTALBYTES);
				for (int k = 0; k < g_TOTALBYTES; k += 4) {
					u32 word;
					memcpy(&word, (const u8 *)pf + k, 4);
					printf(" %08X", word);
					if ((k / 4) % 8 == 7) printf("\n");
				}
				printf("\n");
			}

			printf("%-24s %04X,%04X cyc=%2u cpsr=%08X", c.name, c.w[0], c.w[1],
			       res.cycles, res.cpsr);
			for (int r = 0; r < 15; r++)
				printf(" %08X", res.r[r]);
			printf(" ia=%08X ni=%08X d=", ia, ni);
			for (int k = 0; k < 4; k++) {
				u32 v;
				memcpy(&v, MMU.MAIN_MEM + ((0x02003000u + 4 * k) & _MMU_MAIN_MEM_MASK32), 4);
				printf("%08X,", v);
			}
			printf(" s=");
			for (int k = 0; k < 6; k++) {
				u32 v;
				memcpy(&v, MMU.MAIN_MEM + ((kStackWin + 4 * k) & _MMU_MAIN_MEM_MASK32), 4);
				printf("%08X,", v);
			}
			printf("\n");

			if (c.chk >= 0) {
				const u32 got = (c.chk == 16) ? ia : res.r[c.chk];
				if (got == c.want) {
					g_pass++;
				} else {
					printf("  FAIL %-22s got=%08X want=%08X\n", c.name, got, c.want);
					g_fail++;
				}
			}
		}

		#undef TA
		#undef TX
		#undef TF
		#undef T2
	}

	printf("\n%d checks passed, %d failed\n", g_pass, g_fail);
	return g_fail ? 1 : 0;
}
