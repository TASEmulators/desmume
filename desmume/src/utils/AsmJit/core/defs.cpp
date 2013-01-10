// [AsmJit]
// Complete JIT Assembler for C++ Language.
//
// [License]
// Zlib - See COPYING file in this package.

#define ASMJIT_EXPORTS

// [Dependencies - AsmJit]
#include "../core/defs.h"

// [Api-Begin]
#include "../core/apibegin.h"

namespace AsmJit {

// ============================================================================
// [AsmJit::getErrorString]
// ============================================================================

const char* getErrorString(uint32_t error)
{
  static const char* errorMessage[] = {
    "No error",

    "No heap memory",
    "No virtual memory",

    "Unknown instruction",
    "Illegal instruction",
    "Illegal addressing",
    "Illegal short jump",

    "No function defined",
    "Incomplete function",

    "Not enough registers",
    "Registers overlap",

    "Incompatible argument",
    "Incompatible return value",

    "Unknown error"
  };

  // Saturate error code to be able to use errorMessage[].
  if (error > kErrorCount)
    error = kErrorCount;

  return errorMessage[error];
}

} // AsmJit

// [Api-End]
#include "../core/apiend.h"
