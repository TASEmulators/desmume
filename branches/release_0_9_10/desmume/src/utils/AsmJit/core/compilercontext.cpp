// [AsmJit]
// Complete JIT Assembler for C++ Language.
//
// [License]
// Zlib - See COPYING file in this package.

#define ASMJIT_EXPORTS

// [Dependencies - AsmJit]
#include "../core/compilercontext.h"

// [Api-Begin]
#include "../core/apibegin.h"

namespace AsmJit {

// ============================================================================
// [AsmJit::CompilerContext - Construction / Destruction]
// ============================================================================

CompilerContext::CompilerContext(Compiler* compiler) :
  _zoneMemory(8192 - sizeof(ZoneChunk) - 32),
  _compiler(compiler),
  _func(NULL),
  _start(NULL),
  _stop(NULL),
  _extraBlock(NULL),
  _state(NULL),
  _active(NULL),
  _currentOffset(0),
  _isUnreachable(0)
{
}

CompilerContext::~CompilerContext()
{
}

} // AsmJit namespace

// [Api-End]
#include "../core/apiend.h"
