// [AsmJit]
// Complete JIT Assembler for C++ Language.
//
// [License]
// Zlib - See COPYING file in this package.

// [Guard]
#ifndef _ASMJIT_CORE_STRINGUTIL_H
#define _ASMJIT_CORE_STRINGUTIL_H

// [Dependencies - AsmJit]
#include "../core/defs.h"

namespace AsmJit {

//! @addtogroup AsmJit_Core
//! @{

// ============================================================================
// [AsmJit::StringUtil]
// ============================================================================

//! @brief String utilities.
struct StringUtil
{
  ASMJIT_API static char* copy(char* dst, const char* src, size_t len = kInvalidSize);
  ASMJIT_API static char* fill(char* dst, const int c, size_t len);
  ASMJIT_API static char* hex(char* dst, const uint8_t* src, size_t len);

  ASMJIT_API static char* utoa(char* dst, uintptr_t i, size_t base = 10);
  ASMJIT_API static char* itoa(char* dst, intptr_t i, size_t base = 10);

  static inline void memset32(uint32_t* p, uint32_t c, size_t len)
  {
    for (size_t i = 0; i < len; i++) p[i] = c;
  }
};

//! @}

} // AsmJit namespace

#endif // _ASMJIT_CORE_STRINGUTIL_H
