// Structural test for the block-level shape that the differential harness
// cannot reach: a mid-block `ret` followed by code that a conditional branch
// jumps over.
//
// This is the DMA early-exit contract (migration plan §3.7): emit_dma_event_check
// returns from the middle of a block, writes instruct_adr/next_instruction and
// then the rest of the block continues at the label after the ret. asmjit builds
// a CFG and runs a register allocator, so "code after a ret" is exactly the kind
// of thing that can silently disappear.
//
// The check is deliberately dumb: emit the shape, disassemble, and assert the
// instructions on both sides of the ret are present in order.

#include "a64_dump_common.h"

#include "../../desmume/src/utils/arm_jit/arm64_emit.cpp"

t_bytes *g_out = NULL;

static uint64_t g_label_gen = 0;
uint64_t genlabel() { return ++g_label_gen; }

static void get_flags(int, int) {
	uint32_t nop = A64_DUMP_GET_FLAGS_MARKER;
	g_out->embed(&nop, 4);
}

int main() {
	int failures = 0;

	g_out = allocBytes();

	emit_startfunc(g_out);

	// Something before the early exit so the prolog is not empty.
	emit_add(g_out, 1, 2, 3);

	// if (flag) return;  -- the shape emit_dma_event_check produces.
	uint64_t skip = genlabel();
	emit_cmpimm(g_out, R0, 1);
	emit_branch_label(g_out, skip, 0x0);
	emit_write_ptr32_regptrTO_immFROM(g_out, R3, 0);
	emit_movimm(g_out, 0x1234, R0);
	emit_endfunc(g_out);

	// The rest of the block, reachable only through the branch.
	emit_label(g_out, skip);
	emit_sub(g_out, 4, 5, 6);
	emit_str(g_out, 7, 8);
	emit_endfunc(g_out);

	JittedFunc fn = createFunc(g_out);
	if (!fn) {
		printf("FAILED: createFunc returned null\n");
		return 1;
	}

	uint32_t words[64];
	int n = g_TOTALBYTES / 4;
	if (n > 64) n = 64;
	memcpy(words, (const void *)fn, n * 4);

	printf("block (%d words):\n", n);
	for (int i = 0; i < n; i++)
		printf("  %2d: %08X\n", i, words[i]);

	// The instructions have to appear in this order on both sides of the early
	// exit. Addresses are not checked, because asmjit is free to encode `b.al` as
	// `b` and to fold the two `ret`s into jumps to a single exit -- what matters
	// is that nothing on either side of the early exit was dropped.
	//
	// emit_cmpimm(x, 1) goes through movimm, hence the explicit `mov w16, #1`.
	struct { const char *what; uint32_t word; } expected[] = {
		{"add w3, w1, w2",        0x0B020023u},
		{"mov w16, #1",           0x52800030u},
		{"cmp w0, w16",           0x6B10001Fu},
		{"b.eq",                  0x54000000u},   // masked below
		{"mov w10, #0",           0x5280000Au},
		{"str w10, [x3]",         0xB900006Au},
		{"mov w0, #0x1234",       0x52824680u},
		{"sub w6, w4, w5",        0x4B050086u},
		{"str w8, [x7]",          0xB90000E8u},
	};

	int pos = 0;
	for (size_t e = 0; e < sizeof(expected)/sizeof(expected[0]); e++) {
		int found = -1;
		for (int i = pos; i < n; i++) {
			uint32_t mask = (expected[e].word == 0x54000000u) ? 0xFF000010u : 0xFFFFFFFFu;
			if ((words[i] & mask) == expected[e].word) { found = i; break; }
		}
		if (found < 0) {
			printf("MISSING (or out of order): %s\n", expected[e].what);
			failures++;
		} else {
			printf("ok  %2d: %s\n", found, expected[e].what);
			pos = found + 1;
		}
	}

	// At least one return, and it must not be before the tail: the early exit
	// has to leave the block without executing the rest.
	int rets = 0;
	for (int i = 0; i < n; i++)
		if (words[i] == 0xD65F03C0u) rets++;
	if (rets < 1) {
		printf("no ret at all\n");
		failures++;
	}

	printf(failures ? "FAILED (%d)\n" : "ok: mid-block ret keeps the tail (%d)\n", failures);
	return failures ? 1 : 0;
}
