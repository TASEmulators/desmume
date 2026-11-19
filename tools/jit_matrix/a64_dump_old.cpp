// Dumps the words produced by the hand-written AArch64 encoder for every
// primitive in a64_corpus.inc. The output is frozen as a64_golden.txt and the
// asmjit rewrite (a64_dump_new.cpp) has to reproduce it byte for byte.
//
// This tool only builds against the pre-migration tree, because it includes the
// real emit_core.cpp through arm64_emit.cpp. To regenerate the golden file:
//
//     git worktree add /tmp/a64-old 49e5fe6c
//     clang++ -std=c++17 -o a64_dump_old.exe tools/jit_matrix/a64_dump_old.cpp \
//             -DA64_EMIT_SRC='"/tmp/a64-old/desmume/src/utils/arm_jit/arm64_emit.cpp"'
//     a64_dump_old.exe > tools/jit_matrix/a64_golden.txt
//
// After the migration the tree no longer contains the old encoder, so the
// golden file is the reference, not the tool.

#include "a64_dump_common.h"

// Override to point at a worktree holding the pre-migration encoder. The default
// path does not exist on purpose: after the migration this tree holds the asmjit
// encoder, and building against it here would silently label new output as "old".
#ifndef A64_EMIT_SRC
#define A64_EMIT_SRC "../a64-old/desmume/src/utils/arm_jit/arm64_emit.cpp"
#endif

#include A64_EMIT_SRC

t_bytes *g_out = NULL;

static uint64_t g_label_gen = 0;
uint64_t genlabel() { return ++g_label_gen; }

// The real get_flags() lives in arm_jit_arm.cpp and emits a store through the
// CPU pointer macro chain, which is far outside this migration. A nop keeps the
// position in the stream visible while staying identical on both sides.
static void get_flags(int, int) {
	output_w32(g_out, A64_DUMP_GET_FLAGS_MARKER);
}

#define A64_CASE(name, ...)                                                    \
	do {                                                                        \
		g_out = allocBytes();                                                    \
		__VA_ARGS__;                                                             \
		JittedFunc fn_ = createFunc(g_out);                                      \
		printf("[%03d] %-42s", ++n_case, name);                                  \
		for (int i_ = 0; i_ < g_TOTALBYTES; i_ += 4) {                           \
			uint32_t w_;                                                           \
			memcpy(&w_, (const uint8_t *)fn_ + i_, 4);                             \
			printf(" %08X", w_);                                                   \
		}                                                                         \
		printf("\n");                                                            \
		releaseBytes(g_out);                                                     \
	} while (0)

int main() {
	int n_case = -1;
	printf("# a64 primitive encodings, one line per corpus case\n");

#include "a64_corpus.inc"

	return 0;
}
