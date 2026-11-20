/*
	Copyright (C) 2022 Patrick Herlihy - https://byte4byte.com
	Copyright (C) 2022 DeSmuME team

	This file is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 2 of the License, or
	(at your option) any later version.

	This file is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with the this software.  If not, see <http://www.gnu.org/licenses/>.
*/

// ---------------------------------------------------------------------------
// AArch64 emit core.
//
// Replaces the hand-written encoder: the old version kept the block in a linked
// list of `t_bytes` chunks, resolved `MOD_JUMP` fixups into that buffer and
// memcpy'd the result into the 40MB block. asmjit now owns the encoding, the
// label bookkeeping and the fixups; landing a finished block into executable
// memory is delegated to asmjit's JitRuntime, which also handles the
// instruction-cache flush and the platform-specific W^X requirements.
//
// Included from arm64_emit.cpp, which supplies `t_bytes` (a64::Compiler) and
// the register macros. Not a translation unit of its own.
// ---------------------------------------------------------------------------

#define CHUNK_SIZE 6048*4 // only used by the size assertions in the dump tool

// An instruction that asmjit refused to encode would silently drop out of the
// block, which is the worst possible failure mode: the block still runs, it
// just computes the wrong thing on some inputs. Record it and make createFunc()
// fail, so the caller falls back to the interpreter instead.
static int g_emit_error = 0;

#define ASMJIT_ASSUME_OK(expr)                                                 \
	do {                                                                        \
		asmjit::Error _e = (expr);                                               \
		if (ASMJIT_UNLIKELY(_e != asmjit::Error::kOk)) {                         \
			fprintf(stderr, "asmjit a64: %s @ %s:%d\n",                            \
			        asmjit::stringify_error(_e), __FILE__, __LINE__);              \
			g_emit_error = 1;                                                      \
			ASMJIT_ASSERT(false);                                                  \
		}                                                                         \
	} while (0)

// ARM condition code (the raw 4-bit field) to asmjit's CondCode. The numeric
// values are *not* interchangeable: ARM 0x0..0xD happen to land on CondCode
// 0x2..0xF, but ARM's AL (0xE) is CondCode kAL (0x0), so a cast would turn an
// unconditional branch into a `gt`.
static inline asmjit::arm::CondCode a64_cond(int c) {
	switch (c) {
		case 0x0: return asmjit::arm::CondCode::kEQ;
		case 0x1: return asmjit::arm::CondCode::kNE;
		case 0x2: return asmjit::arm::CondCode::kCS;
		case 0x3: return asmjit::arm::CondCode::kCC;
		case 0x4: return asmjit::arm::CondCode::kMI;
		case 0x5: return asmjit::arm::CondCode::kPL;
		case 0x6: return asmjit::arm::CondCode::kVS;
		case 0x7: return asmjit::arm::CondCode::kVC;
		case 0x8: return asmjit::arm::CondCode::kHI;
		case 0x9: return asmjit::arm::CondCode::kLS;
		case 0xA: return asmjit::arm::CondCode::kGE;
		case 0xB: return asmjit::arm::CondCode::kLT;
		case 0xC: return asmjit::arm::CondCode::kGT;
		case 0xD: return asmjit::arm::CondCode::kLE;
		case 0xE: return asmjit::arm::CondCode::kAL;
		default:
			// 0xF is the "never" encoding, which the code generator never asks for.
			ASMJIT_ASSERT(false);
			return asmjit::arm::CondCode::kAL;
	}
}

// One CodeHolder and one Compiler for the whole backend: compile_basicblock()
// emits a single block at a time, so there is nothing to nest.
static asmjit::CodeHolder g_code;
static t_bytes *g_cc = NULL;
static asmjit::FuncNode *g_fn = NULL;
static std::map<uint64_t, asmjit::Label> g_labels;

// Size of the block createFunc() produced last. Only the offline dump tools
// read it; the backend itself does not need it.
int g_TOTALBYTES = 0;

// Sum of every block createFunc() has landed since the last reset. The S4
// acceptance gate is a code-size ratio, and a single block's size is a poor
// proxy: it is dominated by the prologue, and a test block is not the same
// length as a real one. The total over a full 5120-slot sweep is comparable
// between builds without any of those caveats.
uint64_t g_all_block_bytes = 0;

static asmjit::Label a64_label(t_bytes *out, uint64_t id) {
	std::map<uint64_t, asmjit::Label>::iterator it = g_labels.find(id);
	if (it != g_labels.end())
		return it->second;

	asmjit::Label label = out->new_label();
	g_labels.insert(std::make_pair(id, label));
	return label;
}

static void emit_label(t_bytes *bytes, uint64_t id) {
	ASMJIT_ASSUME_OK(bytes->bind(a64_label(bytes, id)));
}

static void emit_branch_label(t_bytes *out, uint64_t id, int cond) {
	asmjit::Label label = a64_label(out, id);

	// cond 0xE is what the call sites use for "always". `b.al` and `b` are the
	// same instruction with different encodings; the conditional form is kept so
	// the block stays byte-identical to the hand-written encoder's output, which
	// is the only thing that can be checked without an AArch64 machine.
	if (cond == 0xE)
		ASMJIT_ASSUME_OK(out->b_al(label));
	else
		ASMJIT_ASSUME_OK(out->b(a64_cond(cond), label));
}

// asmjit owns executable memory through JitRuntime/JitAllocator. It handles
// the platform details that used to live here by hand -- VirtualAlloc on
// Windows, mmap(MAP_JIT) on macOS, W^X dual mappings under hardened runtime,
// address-range probing, and the instruction-cache flush after a write -- and
// it pools blocks instead of mmap()ing 40MB at a time. The allocator is reset
// as a whole from arm_jit_reset(), matching the old bump-allocator lifetime.
static asmjit::JitRuntime g_jit_runtime;

static void freeFuncs() {
	// Hard reset: release every block so all previously returned function
	// pointers become invalid. A soft reset keeps one block and reuses its
	// addresses, which stale compiled_funcs[] entries would silently execute.
	g_jit_runtime.reset(asmjit::ResetPolicy::kHard);
}
t_bytes *allocBytes() {
	if (! g_cc) {
		asmjit::Error err = g_code.init(asmjit::Environment(asmjit::Arch::kAArch64));
		if (err != asmjit::Error::kOk) {
			fprintf(stderr, "asmjit a64 init failed: %s\n", asmjit::stringify_error(err));
			exit(1);
		}
		g_cc = new t_bytes(&g_code);
	}
	else {
		g_code.reinit();
	}

	g_labels.clear();
	g_emit_error = 0;

	g_fn = g_cc->add_func(asmjit::FuncSignature::build<uint32_t>());
	if (! g_fn) {
		fprintf(stderr, "asmjit a64: add_func failed\n");
		exit(1);
	}

	return g_cc;
}

void releaseBytes(t_bytes *bytes) {
	// Nothing to release here: the CodeHolder is reused by allocBytes(), and
	// the executable memory is released in bulk by freeFuncs().
	(void)bytes;
}

JittedFunc createFunc(t_bytes *bytes) {
	if (g_emit_error) {
		fprintf(stderr, "a64: block emission failed, falling back to the interpreter\n");
		return NULL;
	}

	asmjit::Error err = bytes->end_func();
	if (err != asmjit::Error::kOk) {
		fprintf(stderr, "a64: end_func failed: %s\n", asmjit::stringify_error(err));
		return NULL;
	}

	// An unbound label fails right here, not at commit time, so the error has to
	// be caught before anything is written into the code block.
	err = bytes->finalize();
	if (err != asmjit::Error::kOk) {
		fprintf(stderr, "a64: finalize failed: %s\n", asmjit::stringify_error(err));
		return NULL;
	}

	// JitRuntime::add() does the whole landing sequence: flatten, resolve
	// fixups, relocate to the final address, copy, and flush the instruction
	// cache. Using it instead of a hand-written copy keeps the W^X handling in
	// one place -- asmjit picks MAP_JIT or a dual mapping as the host requires.
	JittedFunc fn = NULL;
	asmjit::Error err2 = g_jit_runtime.add(&fn, &g_code);
	if (err2 != asmjit::Error::kOk) {
		fprintf(stderr, "a64: JitRuntime::add failed: %s\n", asmjit::stringify_error(err2));
		return NULL;
	}

	g_TOTALBYTES = (int)g_code.code_size();
	g_all_block_bytes += g_TOTALBYTES;


	return (JittedFunc)fn;
}

