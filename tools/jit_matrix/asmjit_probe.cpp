// S2 skeleton probe -- answers, by compiling and running, the questions the
// migration plan currently only asserts on paper:
//
//   1. Does a CodeHolder + x86::Compiler + manual landing path actually work on
//      the vendored asmjit (677a10b), and does the result execute?
//   2. Is the landing order flatten() -> resolve_cross_section_fixups() ->
//      relocate_to_base() -> copy_flattened_data() the one asmjit itself uses?
//      (See core/jit_runtime.cpp, JitRuntime::_add.)
//   3. Does an unbound label survive finalize() and get rejected at commit time
//      with kUnresolvedFixups, instead of producing garbage code?
//   4. Does a64::Compiler build a function at all on an x86 host (it cannot run
//      here, but it must finalize and produce the expected encoding)?
//
// Build: pwsh -File tools/jit_matrix/build_asmjit.ps1
// Run:   build_core/asmjit_probe.exe
//
// This file is a probe, not production code. It is not part of any build.

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include <asmjit/core.h>
#include <asmjit/x86.h>
#include <asmjit/a64.h>

#include <stdio.h>
#include <stdint.h>
#include <string.h>

using namespace asmjit;

static int g_pass = 0;
static int g_fail = 0;

static void check(bool ok, const char* what) {
  if (ok) { g_pass++; printf("  PASS  %s\n", what); }
  else    { g_fail++; printf("  FAIL  %s\n", what); }
}

static void check_err(Error e, Error expect, const char* what) {
  if (e == expect) {
    g_pass++;
    printf("  PASS  %s\n", what);
  } else {
    g_fail++;
    printf("  FAIL  %s (got %s, expected %s)\n", what,
           stringify_error(e), stringify_error(expect));
  }
}

// ---------------------------------------------------------------------------
// Scratchpad: stands in for arm_jit.cpp's static 32MB buffer. Executable
// because the probe has to call what it emits; the real JIT allocates its
// scratchpad the same way (VirtualAlloc with PAGE_EXECUTE_READWRITE).
// ---------------------------------------------------------------------------
static uint8_t* g_scratch      = nullptr;
static size_t   g_scratch_size = 4u << 20;
static size_t   g_scratch_used = 0;

static bool scratch_init() {
  g_scratch = static_cast<uint8_t*>(
      VirtualAlloc(nullptr, g_scratch_size, MEM_COMMIT | MEM_RESERVE,
                   PAGE_EXECUTE_READWRITE));
  return g_scratch != nullptr;
}

static void scratch_reset() { g_scratch_used = 0; }

// ---------------------------------------------------------------------------
// Landing path. Mirrors JitRuntime::_add() step for step, except that the
// destination is a bump pointer into our own buffer instead of JitAllocator
// spans. Anything missing here is missing from the migration plan as well.
// ---------------------------------------------------------------------------
static Error commit(CodeHolder& code, void** out) {
  *out = nullptr;

  ASMJIT_PROPAGATE(code.flatten());
  ASMJIT_PROPAGATE(code.resolve_cross_section_fixups());

  // A label that was referenced but never bound is not an error until here.
  if (code.has_unresolved_fixups())
    return Error::kUnresolvedFixups;

  size_t estimated_size = code.code_size();
  if (estimated_size == 0)
    return Error::kNoCodeGenerated;

  if (g_scratch_used + estimated_size > g_scratch_size)
    return Error::kOutOfMemory;

  uint64_t base = uint64_t(g_scratch + g_scratch_used);

  CodeHolder::RelocationSummary summary;
  ASMJIT_PROPAGATE(code.relocate_to_base(base, &summary));

  // Relocation can shrink the code when a relocation no longer needs an entry
  // in an address table. asmjit asserts this equals code.code_size().
  size_t final_size = estimated_size - summary.code_size_reduction;

  ASMJIT_PROPAGATE(code.copy_flattened_data(g_scratch + g_scratch_used, final_size));

  *out = g_scratch + g_scratch_used;
  g_scratch_used += final_size;
  return Error::kOk;
}

// ---------------------------------------------------------------------------
// x86 cases
// ---------------------------------------------------------------------------
static void x86_ret_zero() {
  printf("\n[x86] function that only returns 0\n");

  CodeHolder code;
  Error e = code.init(Environment::host());
  check_err(e, Error::kOk, "CodeHolder::init(Environment::host())");

  x86::Compiler cc(&code);
  cc.add_func(FuncSignature::build<int32_t>());
  cc.mov(x86::eax, 0);
  cc.ret();
  cc.end_func();

  check_err(cc.finalize(), Error::kOk, "x86::Compiler::finalize()");

  void* fn = nullptr;
  Error ce = commit(code, &fn);
  check_err(ce, Error::kOk, "commit() lands the block");
  if (ce != Error::kOk) return;

  typedef int32_t (*Fn)();
  Fn f = reinterpret_cast<Fn>(fn);
  int32_t v = f();
  check(v == 0, "executed block returns 0");
  printf("        (code bytes: %zu)\n", code.code_size());
}

static void x86_mov_42() {
  printf("\n[x86] mov eax, 42; ret\n");

  CodeHolder code;
  code.init(Environment::host());

  x86::Compiler cc(&code);
  cc.add_func(FuncSignature::build<int32_t>());
  cc.mov(x86::eax, 42);
  cc.ret();
  cc.end_func();
  check_err(cc.finalize(), Error::kOk, "finalize()");

  void* fn = nullptr;
  check_err(commit(code, &fn), Error::kOk, "commit()");
  if (!fn) return;

  typedef int32_t (*Fn)();
  check(reinterpret_cast<Fn>(fn)() == 42, "executed block returns 42");
}

static void x86_with_args() {
  printf("\n[x86] f(a, b) = a * 3 + b  (argument marshalling)\n");

  CodeHolder code;
  code.init(Environment::host());

  x86::Compiler cc(&code);
  FuncNode* fn_node = cc.add_func(FuncSignature::build<int32_t, int32_t, int32_t>());

  x86::Gp a = cc.new_gp32("a");
  x86::Gp b = cc.new_gp32("b");
  fn_node->set_arg(0, a);   // note: FuncNode::set_arg, not Compiler::set_arg
  fn_node->set_arg(1, b);

  cc.lea(a, ptr(a.r64(), a.r64(), 1));  // a = a + a*2  (ptr's 3rd arg is a shift)
  cc.add(a, b);
  cc.mov(x86::eax, a);                  // SysV/Win64 return value goes in eax
  cc.ret();
  cc.end_func();
  check_err(cc.finalize(), Error::kOk, "finalize() with two args");

  void* fp = nullptr;
  check_err(commit(code, &fp), Error::kOk, "commit()");
  if (!fp) return;

  typedef int32_t (*Fn)(int32_t, int32_t);
  Fn f = reinterpret_cast<Fn>(fp);
  int32_t got = f(7, 5);
  check(got == 26, "f(7,5) == 26");
  if (got != 26) printf("        (got %d)\n", got);
  (void)fn_node;
}

static void x86_unresolved_label() {
  printf("\n[x86] error injection: jump to a label that is never bound\n");

  CodeHolder code;
  code.init(Environment::host());

  x86::Compiler cc(&code);
  cc.add_func(FuncSignature::build<int32_t>());
  Label never = cc.new_label();
  cc.jmp(never);
  cc.end_func();

  Error fe = cc.finalize();
  printf("        finalize() -> %s\n", stringify_error(fe));

  void* fn = nullptr;
  Error ce = commit(code, &fn);
  printf("        commit()   -> %s\n", stringify_error(ce));

  // Measured: finalize() already refuses the block with InvalidState, so nothing
  // reaches commit() and code_size() is 0 -> NoCodeGenerated. The
  // has_unresolved_fixups() check inside commit() is therefore a safety net for
  // paths that bypass the Compiler's own validation, not the primary detector.
  // Either way the contract that matters is: no code, no pointer, no crash.
  check(fe != Error::kOk, "finalize() reports the unbound label");
  check(ce != Error::kOk, "commit() refuses the block as well");
  check(fn == nullptr, "rejected block leaves *dst == nullptr");
}

// ---------------------------------------------------------------------------
// arm64: cannot run on this host, but must assemble.
// ---------------------------------------------------------------------------
static void a64_compile() {
  printf("\n[a64] compile-only: mov w0, #42; ret\n");

  CodeHolder code;
  Error e = code.init(Environment(Arch::kAArch64));
  check_err(e, Error::kOk, "CodeHolder::init(Environment(AArch64))");
  if (e != Error::kOk) return;

  a64::Compiler cc(&code);
  cc.add_func(FuncSignature::build<int32_t>());
  cc.mov(a64::w0, Imm(42));
  cc.ret(a64::x30);
  cc.end_func();

  check_err(cc.finalize(), Error::kOk, "a64::Compiler::finalize()");

  void* fn = nullptr;
  Error ce = commit(code, &fn);
  check_err(ce, Error::kOk, "commit() lands the arm64 block");
  if (ce != Error::kOk) return;

  size_t size = code.code_size();
  printf("        code bytes: %zu\n", size);
  check(size > 0, "arm64 block is non-empty");

  // 0xD65F03C0 = RET X30, little endian. If it is the last instruction the
  // whole path (compiler -> encoder -> flatten -> relocate -> copy) is sane.
  if (size >= 4) {
    uint32_t last;
    memcpy(&last, static_cast<uint8_t*>(fn) + size - 4, 4);
    printf("        last insn: 0x%08X\n", last);
    check(last == 0xD65F03C0u, "block ends with RET X30");
  }
}

// ---------------------------------------------------------------------------
// Does a CodeHolder survive reinit() and produce a second working function?
// The real JIT resets the generator per block, so this matters.
// ---------------------------------------------------------------------------
static void x86_reuse() {
  printf("\n[x86] reuse: two blocks from one CodeHolder via reinit()\n");

  CodeHolder code;
  code.init(Environment::host());

  void* f1 = nullptr;
  void* f2 = nullptr;

  {
    x86::Compiler cc(&code);
    cc.add_func(FuncSignature::build<int32_t>());
    cc.mov(x86::eax, 11);
    cc.ret();
    cc.end_func();
    if (cc.finalize() != Error::kOk) { check(false, "first finalize()"); return; }
    if (commit(code, &f1) != Error::kOk) { check(false, "first commit()"); return; }
  }

  code.reinit();

  {
    x86::Compiler cc(&code);
    cc.add_func(FuncSignature::build<int32_t>());
    cc.mov(x86::eax, 22);
    cc.ret();
    cc.end_func();
    if (cc.finalize() != Error::kOk) { check(false, "second finalize()"); return; }
    if (commit(code, &f2) != Error::kOk) { check(false, "second commit()"); return; }
  }

  typedef int32_t (*Fn)();
  check(reinterpret_cast<Fn>(f1)() == 11, "first block still returns 11");
  check(reinterpret_cast<Fn>(f2)() == 22, "second block returns 22");
  check(f1 != f2, "second block landed at a different address");
}

int main() {
  printf("asmjit skeleton probe -- vendored master 677a10b\n");

  if (!scratch_init()) {
    printf("VirtualAlloc failed\n");
    return 2;
  }
  printf("scratchpad: %zu KB executable\n", g_scratch_size / 1024);

  x86_ret_zero();
  scratch_reset();
  x86_mov_42();
  scratch_reset();
  x86_with_args();
  scratch_reset();
  x86_unresolved_label();
  scratch_reset();
  a64_compile();
  scratch_reset();
  x86_reuse();

  printf("\n%d passed, %d failed\n", g_pass, g_fail);
  return g_fail ? 1 : 0;
}
