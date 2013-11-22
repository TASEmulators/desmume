// [AsmJit]
// Complete JIT Assembler for C++ Language.
//
// [License]
// Zlib - See COPYING file in this package.

// [Guard]
#ifndef _ASMJIT_CORE_ASSERT_H
#define _ASMJIT_CORE_ASSERT_H

// [Dependencies - AsmJit]
#include "../core/build.h"

// [Api-Begin]
#include "../core/apibegin.h"

namespace AsmJit {

//! @addtogroup AsmJit_Core
//! @{

// ============================================================================
// [AsmJit::Assert]
// ============================================================================

//! @brief Called in debug build on assertion failure.
//! @param file Source file name where it happened.
//! @param line Line in the source file.
//! @param exp Expression what failed.
//!
//! If you have problems with assertions simply put a breakpoint into
//! AsmJit::assertionFailure() method (AsmJit/Core/Assert.cpp file) and examine
//! call stack.
ASMJIT_API void assertionFailure(const char* file, int line, const char* exp);

// ============================================================================
// [ASMJIT_ASSERT]
// ============================================================================

#if defined(ASMJIT_DEBUG)

#if !defined(ASMJIT_ASSERT)
#define ASMJIT_ASSERT(exp) \
  do { \
    if (!(exp)) ::AsmJit::assertionFailure(__FILE__, __LINE__, #exp); \
  } while(0)
#endif

#else

#if !defined(ASMJIT_ASSERT)
#define ASMJIT_ASSERT(exp) ASMJIT_NOP()
#endif

#endif // DEBUG

//! @}

} // AsmJit namespace

// [Api-End]
#include "../core/apiend.h"

// [Guard]
#endif // _ASMJIT_CORE_ASSERT_H
