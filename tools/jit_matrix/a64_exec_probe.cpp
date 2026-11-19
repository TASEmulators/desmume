// Executes the AArch64 primitives on real hardware.
//
// a64_dump_new only compares encodings: it can prove that asmjit emits the same
// instruction stream as the hand encoder, but not that the stream does the right
// thing, nor that the landing path (flatten -> relocate -> copy -> icache flush)
// produces callable code. On an aarch64 host both can be checked directly, so
// this probe builds one-block functions out of the same emit_* primitives the
// backend uses, calls them through the C ABI and compares the result.
//
// Build and run (Termux / any aarch64 Linux):
//     clang++ -std=c++20 -O1 -I desmume/3rdparty -DASMJIT_STATIC -DASMJIT_NO_X86 \
//         tools/jit_matrix/a64_exec_probe.cpp obj/*.o -o a64_exec_probe
//     ./a64_exec_probe

#include "a64_dump_common.h"

#include "../../desmume/src/utils/arm_jit/arm64_emit.cpp"

t_bytes *g_out = NULL;

static uint64_t g_label_gen = 0;
uint64_t genlabel() { return ++g_label_gen; }

// Stands in for the real get_flags(), which lives in arm_jit_arm.cpp and stores
// NZCV into the emulated CPU. None of the cases below call it.
static void get_flags(int, int) {
	uint32_t nop = A64_DUMP_GET_FLAGS_MARKER;
	g_out->embed(&nop, 4);
}

typedef uint64_t (*Fn3)(uint64_t, uint64_t, uint64_t);

static int g_fail = 0;
static int g_run = 0;

// w0/w1/w2 carry the arguments, w0 carries the result: the same convention the
// generated blocks use before they copy anything back into armcpu_t.
template <typename Emit>
static void run_case(const char *name, Emit emit, uint64_t a, uint64_t b, uint64_t c,
                     uint64_t want) {
	g_out = allocBytes();
	emit_startfunc(g_out);
	emit(g_out);
	// Real blocks close themselves with emit_endfunc(); asmjit's end_func()
	// only appends a `ret` and would leave the 64 bytes save() pushed on the
	// stack, which corrupts the caller's frame.
	emit_endfunc(g_out);

	Fn3 fn = (Fn3)createFunc(g_out);
	if (!fn) {
		printf("FAIL %-22s createFunc() returned null\n", name);
		g_fail++;
		releaseBytes(g_out);
		return;
	}

	const uint64_t got = fn(a, b, c);
	g_run++;

	if (got != want) {
		printf("FAIL %-22s got=%016llX want=%016llX\n",
		       name, (unsigned long long)got, (unsigned long long)want);
		g_fail++;
	} else {
		printf("ok   %-22s %016llX\n", name, (unsigned long long)got);
	}

	releaseBytes(g_out);
}

int main() {
	printf("AArch64 primitive execution probe\n");

	// 32-bit ALU. The upper half of every result must be zero: the emulated
	// registers are 32-bit and the backend writes them with W-register ops.
	// The three-register forms write their *third* argument (rt); the two
	// sources come first. add/sub therefore land in w0 only when 0 is passed
	// last, unlike and/or/xor above where I wrote it that way by accident.
	run_case("add", [](t_bytes *o) { emit_add(o, 1, 2, 0); }, 0, 5, 3, 8);
	run_case("add_wrap", [](t_bytes *o) { emit_add(o, 1, 2, 0); },
	         0, 0xFFFFFFFFull, 1, 0);
	run_case("sub", [](t_bytes *o) { emit_sub(o, 1, 2, 0); }, 0, 10, 3, 7);
	run_case("and", [](t_bytes *o) { emit_and(o, 1, 2, 0); }, 0, 0xF0, 0x3C, 0x30);
	run_case("or", [](t_bytes *o) { emit_or(o, 1, 2, 0); }, 0, 0xF0, 0x0F, 0xFF);
	run_case("xor", [](t_bytes *o) { emit_xor(o, 1, 2, 0); }, 0, 0xFF, 0x0F, 0xF0);
	run_case("addimm", [](t_bytes *o) { emit_addimm(o, 1, 7, 0); }, 0, 5, 0, 12);

	// Shifts: immediate forms. emit_lsl materialises the count in SHIFT_REG,
	// which is w0, so the shifted value has to live elsewhere and be moved into
	// w0 afterwards; emit_lsr/emit_asr/emit_ror use the immediate encoding for
	// n < 32 and leave w0 alone.
	run_case("lsl #4", [](t_bytes *o) {
		emit_lsl(o, 1, 4);
		emit_mov(o, 1, 0);
	}, 0, 0x11, 0, 0x110);
	run_case("lsr #8", [](t_bytes *o) { emit_lsr(o, 0, 8); }, 0x12345678, 0, 0, 0x123456);
	run_case("asr #4", [](t_bytes *o) { emit_asr(o, 0, 4); }, 0x80000000ull, 0, 0, 0xF8000000ull);
	run_case("ror #8", [](t_bytes *o) { emit_ror(o, 0, 8); }, 0x12345678, 0, 0, 0x78123456);

	// emit_movimm picks between movz/movn/orr and a movz+movk chain; both
	// branches are worth executing, so one value per shape.
	run_case("movimm 16-bit", [](t_bytes *o) { emit_movimm(o, 0x1234, 0); }, 0, 0, 0, 0x1234);
	run_case("movimm movz+movk", [](t_bytes *o) { emit_movimm(o, 0x12345678, 0); },
	         0, 0, 0, 0x12345678);
	run_case("movimm 0xDEADBEEF", [](t_bytes *o) { emit_movimm(o, 0xDEADBEEF, 0); },
	         0, 0, 0, 0xDEADBEEF);

	// Flags: cmp sets NZCV, cset reads it back (CS = unsigned >=).
	run_case("cmp+cset (1<2)", [](t_bytes *o) {
		emit_cmp(o, 0, 1);
		emit_setc(o, 0);
	}, 1, 2, 0, 0);
	run_case("cmp+cset (2>=1)", [](t_bytes *o) {
		emit_cmp(o, 0, 1);
		emit_setc(o, 0);
	}, 2, 1, 0, 1);

	// Labels and forward branches: emit_mov_cond is the smallest helper that
	// uses them (branch over, move, branch out).
	run_case("mov_cond taken", [](t_bytes *o) {
		emit_cmp(o, 0, 1);
		emit_mov_cond(o, 2, 0, 0x0);        // EQ
	}, 7, 7, 0x99, 0x99);
	run_case("mov_cond skipped", [](t_bytes *o) {
		emit_cmp(o, 0, 1);
		emit_mov_cond(o, 2, 0, 0x0);
	}, 7, 8, 0x99, 7);

	// Multiply: emit_mul writes back into its first operand.
	run_case("mul", [](t_bytes *o) {
		emit_mul(o, 1, 2);
		emit_mov(o, 1, 0);
	}, 0, 6, 7, 42);

	// Memory: word and byte round trips through a pointer in x2.
	uint64_t slot = 0;
	run_case("str/ldr", [](t_bytes *o) {
		emit_str(o, 2, 1);
		emit_ldr(o, 2, 0);
	}, 0, 0xCAFEBABEull, (uint64_t)(uintptr_t)&slot, 0xCAFEBABEull);
	run_case("strb/ldrb", [](t_bytes *o) {
		emit_strb(o, 2, 1);
		emit_ldrb(o, 2, 0);
	}, 0, 0x1234, (uint64_t)(uintptr_t)&slot, 0x34);

	printf("\n%d/%d cases ran, %d failed\n", g_run, g_run + g_fail, g_fail);
	return g_fail ? 1 : 0;
}
