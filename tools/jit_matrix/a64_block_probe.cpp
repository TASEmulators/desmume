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
// Output is a per-case dump of R0..R14, CPSR and cycles, plus a handful of
// self-checks against a C model of the multiply instructions. The dump is meant
// to be diffed against a golden copy taken from the previous build: the
// register-cache migration must not change a single bit of it.
//
// Build and run (Termux / any aarch64 host):
//     clang++ -std=c++20 -O1 -I desmume/3rdparty -I desmume/src -DASMJIT_STATIC \
//         -DASMJIT_NO_X86 -DHAVE_JIT tools/jit_matrix/a64_block_probe.cpp \
//         obj/*.o -o a64_block_probe
//     ./a64_block_probe > block_new.txt

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
u32 armcpu_switchMode(armcpu_t *cp, u8 mode)
{
	cp->CPSR.bits.mode = mode & 0x1F;
	return 0;
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

static int g_pass = 0, g_fail = 0;

static void prime_cpu(u32 adr, u32 opcode, const u32 *ops, u32 cpsr)
{
	memset(NDS_ARM9.R, 0, sizeof(NDS_ARM9.R));
	for (int i = 0; i < 16; i++)
		NDS_ARM9.R[i] = ops[i];
	NDS_ARM9.CPSR.val = cpsr;
	// The pipeline value, not the address: handlers that read PC rely on
	// R15 = instruct_adr + 2*insnsize.
	NDS_ARM9.R[15] = adr + 8;
	NDS_ARM9.instruct_adr = adr;
	NDS_ARM9.next_instruction = adr + 4;
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
};

static const size_t kCaseCount = sizeof(kCases) / sizeof(kCases[0]);

int main(int argc, char **argv)
{
	// --dump N prints the words of case N's block instead of running the suite.
	// The encoding layer is where a register-plumbing change is easiest to see.
	const int dump_case = (argc > 2 && strcmp(argv[1], "--dump") == 0) ? atoi(argv[2]) : -1;
	memset(&MMU, 0, sizeof(MMU));
	// A main-RAM mirror is compared against DTCMRegion with the low 14 bits
	// masked off; park it far away so no case address can alias into it.
	MMU.DTCMRegion = 0x0F000000;
	memset(&cp15, 0, sizeof(cp15));
	memset(&NDS_ARM7, 0, sizeof(NDS_ARM7));
	memset(&NDS_ARM9, 0, sizeof(NDS_ARM9));

	CommonSettings.jit_max_block_size = 1;   // one instruction per block
	arm_jit_reset(true, true);

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

	printf("\n%d checks passed, %d failed\n", g_pass, g_fail);
	return g_fail ? 1 : 0;
}
