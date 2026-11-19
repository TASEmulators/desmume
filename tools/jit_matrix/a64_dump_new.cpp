// Dumps the words produced by the current (asmjit) AArch64 encoder for the same
// corpus as a64_dump_old.cpp. Diff its output against a64_golden.txt:
//
//     build_core\a64_dump_new.exe > build_core\a64_new.txt
//     diff tools\jit_matrix\a64_golden.txt build_core\a64_new.txt
//
// Any difference is either a translation mistake or an encoding asmjit picks
// differently; both have to be looked at, because the whole point of S2 is that
// the *behaviour* of every primitive stays identical.

#include "a64_dump_common.h"

#include "../../desmume/src/utils/arm_jit/arm64_emit.cpp"

t_bytes *g_out = NULL;

static uint64_t g_label_gen = 0;
uint64_t genlabel() { return ++g_label_gen; }

// See a64_dump_old.cpp: the real get_flags() lives in arm_jit_arm.cpp.
static void get_flags(int, int) {
	uint32_t nop = A64_DUMP_GET_FLAGS_MARKER;
	g_out->embed(&nop, 4);
}

#define A64_CASE(name, ...)                                                    \
	do {                                                                        \
		g_out = allocBytes();                                                    \
		__VA_ARGS__;                                                             \
		JittedFunc fn_ = createFunc(g_out);                                      \
		printf("[%03d] %-42s", ++n_case, name);                                  \
		if (! fn_) {                                                             \
			printf(" <FAILED>\n");                                                 \
			releaseBytes(g_out);                                                   \
			continue;                                                              \
		}                                                                        \
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
