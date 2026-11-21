// Executes the lightweight helper-call variants (call_mem / call_mem_r7 /
// call_mem_r2) on real AArch64 hardware.
//
// a64_dump_new can only prove that the words match the hand encoder's. The
// point of these variants is a *survival* claim instead: they drop 12 of the 15
// instructions callR3 emits, and the justification is that every register they
// stop saving is dead at those call sites. An encoding dump cannot check that,
// so this probe executes the sequences: it parks sentinels in the emulated-
// register cache slots, calls a C helper that deliberately trashes every
// caller-saved register the way a real mem_op helper does (they are ordinary
// AAPCS64 C++, so they are free to), and reads the slots back.
//
// The "raw blr" cases are the negative controls: same body, no stp/ldp pair.
// They must come back with the helper's garbage. If they ever come back with
// the sentinel, the helper is not really clobbering that register and the
// matching positive case proves nothing.
//
// Build and run (Termux / any aarch64 Linux):
//     clang++ -std=c++20 -O1 -I desmume/3rdparty -DASMJIT_STATIC -DASMJIT_NO_X86 \
//         tools/jit_matrix/a64_memcall_probe.cpp obj/*.o -o a64_memcall_probe
//     ./a64_memcall_probe

#include "a64_dump_common.h"

#include "../../desmume/src/utils/arm_jit/arm64_emit.cpp"

t_bytes *g_out = NULL;

static uint64_t g_label_gen = 0;
uint64_t genlabel() { return ++g_label_gen; }

// Stands in for the real get_flags(); none of the cases below call it.
static void get_flags(int, int) {
	uint32_t nop = A64_DUMP_GET_FLAGS_MARKER;
	g_out->embed(&nop, 4);
}

// --- the helper -------------------------------------------------------------
// Mirrors what a mem_op handler looks like to the block: two 32-bit arguments
// in w0/w1, a cycle count back in w0, and no obligation whatsoever to preserve
// x2/x12/x15 (all caller-saved under AAPCS64, and the shipped handlers are
// ordinary C++ that the compiler is free to use them in).
//
// The asm block is what makes the test meaningful: without an explicit clobber
// the compiler might keep x12/x15 untouched for its own reasons and every
// positive case would pass for the wrong reason.
static uint64_t g_calls = 0;
static uint64_t g_last_args = 0;

extern "C" uint32_t jit_mem_helper(uint32_t a, uint32_t b) {
	g_calls++;
	g_last_args = ((uint64_t)b << 32) | a;
	asm volatile("movz x12, #0xBEEF, lsl #16\n\t"
	             "movz x15, #0xF00D, lsl #16\n\t"
	             "movz x2,  #0xDEAD, lsl #16"
	             ::: "x12", "x15", "x2");
	return a + b;
}

// Sentinel values written into the slots before the call, and the garbage the
// helper leaves behind.
static const uint32_t S_X12 = 0x1111;
static const uint32_t S_X15 = 0x2222;
static const uint32_t S_X2 = 0x4444;
static const uint32_t G_X12 = 0xBEEF0000u;
static const uint32_t G_X15 = 0xF00D0000u;
static const uint32_t G_X2 = 0xDEAD0000u;

typedef uint64_t (*Fn3)(uint64_t, uint64_t, uint64_t);

static int g_fail = 0;
static int g_run = 0;

static void report(const char *name, uint64_t got, uint64_t want) {
	g_run++;
	if (got != want) {
		printf("FAIL %-30s got=%016llX want=%016llX\n",
		       name, (unsigned long long)got, (unsigned long long)want);
		g_fail++;
	} else {
		printf("ok   %-30s %016llX\n", name, (unsigned long long)got);
	}
}

// Builds one block, lands it and calls it once with (a, b, c).
template <typename Emit>
static void run_case(const char *name, Emit emit, uint64_t a, uint64_t b,
                     uint64_t c, uint64_t want) {
	g_out = allocBytes();
	emit_startfunc(g_out);
	emit(g_out);
	// Real blocks close themselves with emit_endfunc(); asmjit's end_func()
	// only appends a `ret` and would leave the 64 bytes save() pushed on the
	// stack, corrupting the caller's frame.
	emit_endfunc(g_out);

	Fn3 fn = (Fn3)createFunc(g_out);
	if (!fn) {
		printf("FAIL %-30s createFunc() returned null\n", name);
		g_fail++;
		releaseBytes(g_out);
		return;
	}

	report(name, fn(a, b, c), want);
	releaseBytes(g_out);
}

// The body every case shares: sentinel into one cache slot, helper pointer into
// x3, then whichever call form is under test.
enum CallForm { kRawBlr, kCallMem, kCallMemR7, kCallMemR2 };

static void emit_call_form(t_bytes *o, CallForm form) {
	emit_ptr(o, (uintptr_t)jit_mem_helper, form == kCallMemR7 ? 7 : 3);
	switch (form) {
	case kRawBlr:    ASMJIT_ASSUME_OK(o->blr(gx(3))); break;
	case kCallMem:   call_mem(o);    break;
	case kCallMemR7: call_mem_r7(o); break;
	case kCallMemR2: call_mem_r2(o); break;
	}
}

// Returns the value of one slot after the call.
static void case_slot(const char *name, CallForm form, int slot,
                      uint32_t sentinel, uint64_t want) {
	run_case(name, [form, slot, sentinel](t_bytes *o) {
		emit_movimm(o, sentinel, slot);
		emit_call_form(o, form);
		emit_mov(o, slot, 0);
	}, 0, 0, 0, want);
}

int main() {
	printf("AArch64 helper-call register survival probe\n");
	printf("helper: jit_mem_helper, clobbers x12/x15/x2\n\n");

	// --- negative controls: the helper really does trash these registers ----
	case_slot("raw blr -> x12",   kRawBlr, 12, S_X12, G_X12);
	case_slot("raw blr -> x15",   kRawBlr, 15, S_X15, G_X15);
	case_slot("raw blr -> x2",    kRawBlr, 2,  S_X2,  G_X2);

	// --- call_mem: the two volatile cache slots must survive ---------------
	case_slot("call_mem x12",     kCallMem, 12, S_X12, S_X12);
	case_slot("call_mem x15",     kCallMem, 15, S_X15, S_X15);
	// x21 is the third cache slot and is AAPCS64 callee-saved, which is the
	// whole reason call_mem does not push it. Check the ABI actually delivers.
	case_slot("call_mem x21 (callee-saved)", kCallMem, 21, 0x3333, 0x3333);

	// The cycle count has to come back in x0: it is the only value the LDR/STR
	// handlers read after the call.
	run_case("call_mem returns cycles", [](t_bytes *o) {
		emit_call_form(o, kCallMem);
	}, 0x1234, 0x111, 0, 0x1345);

	// --- call_mem_r7: same, helper pointer in x7 ---------------------------
	case_slot("call_mem_r7 x12",  kCallMemR7, 12, S_X12, S_X12);
	case_slot("call_mem_r7 x15",  kCallMemR7, 15, S_X15, S_X15);

	// --- call_mem_r2: additionally keeps the LDRD/STRD address in x2 -------
	case_slot("call_mem_r2 x2",   kCallMemR2, 2, S_X2, S_X2);
	case_slot("call_mem_r2 x12",  kCallMemR2, 12, S_X12, S_X12);
	case_slot("call_mem_r2 x15",  kCallMemR2, 15, S_X15, S_X15);

	// All three slots at once: the spill and the reload must not alias each
	// other's stack slot (the variant pushes x2+x12 and x15 in two steps).
	run_case("call_mem_r2 all slots", [](t_bytes *o) {
		emit_movimm(o, S_X12, 12);
		emit_movimm(o, S_X15, 15);
		emit_movimm(o, S_X2, 2);
		emit_call_form(o, kCallMemR2);
		emit_xor(o, 12, 15, 0);
		emit_xor(o, 0, 2, 0);
	}, 0, 0, 0, (uint64_t)(S_X12 ^ S_X15 ^ S_X2));

	run_case("call_mem_r2 returns cycles", [](t_bytes *o) {
		emit_call_form(o, kCallMemR2);
	}, 0x1234, 0x111, 0, 0x1345);

	// --- stack balance -----------------------------------------------------
	// call_mem_r2 pushes in two steps and pops in two steps of a *different*
	// shape (stp/str out, ldr/ldp in). An off-by-16 there still leaves the
	// function returning something plausible, so it has to be caught by
	// running it enough times to walk off the stack: 1e6 * 16B = 16MB, well
	// past the default limit, and a leak would also smash the canary below.
	g_out = allocBytes();
	emit_startfunc(g_out);
	emit_movimm(g_out, S_X12, 12);
	emit_movimm(g_out, S_X15, 15);
	emit_movimm(g_out, S_X2, 2);
	emit_call_form(g_out, kCallMemR2);
	emit_xor(g_out, 12, 15, 0);
	emit_xor(g_out, 0, 2, 0);
	emit_endfunc(g_out);
	{
		Fn3 fn = (Fn3)createFunc(g_out);
		if (!fn) {
			printf("FAIL %-30s createFunc() returned null\n", "stack balance");
			g_fail++;
		} else {
			volatile uint64_t canary[16];
			for (int i = 0; i < 16; i++)
				canary[i] = 0x5A5A000000000000ull + (uint64_t)i;

			uint64_t bad = 0;
			const uint64_t want = (uint64_t)(S_X12 ^ S_X15 ^ S_X2);
			for (uint64_t i = 0; i < 1000000ull; i++) {
				if (fn(0, 0, 0) != want) { bad = i + 1; break; }
			}
			for (int i = 0; i < 16; i++) {
				if (canary[i] != 0x5A5A000000000000ull + (uint64_t)i) {
					bad = 0xFFFFFFFFull;
					break;
				}
			}
			report("stack balance (1e6 calls)", bad, 0);
		}
	}
	releaseBytes(g_out);

	// The helper has to have been entered once per case above, or the whole run
	// compared stale values.
	printf("\nhelper calls: %llu, last args (a,b) = %08X/%08X\n",
	       (unsigned long long)g_calls,
	       (uint32_t)(g_last_args & 0xFFFFFFFFu),
	       (uint32_t)(g_last_args >> 32));

	printf("\n%d/%d cases ran, %d failed\n", g_run, g_run + g_fail, g_fail);
	return g_fail ? 1 : 0;
}
