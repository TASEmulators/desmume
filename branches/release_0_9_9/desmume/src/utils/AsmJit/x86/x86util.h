// [AsmJit]
// Complete JIT Assembler for C++ Language.
//
// [License]
// Zlib - See COPYING file in this package.

// [Guard]
#ifndef _ASMJIT_X86_X86UTIL_H
#define _ASMJIT_X86_X86UTIL_H

// [Dependencies - AsmJit]
#include "../x86/x86defs.h"
#include "../x86/x86operand.h"

// [Api-Begin]
#include "../core/apibegin.h"

namespace AsmJit {

//! @addtogroup AsmJit_X86
//! @{

// ============================================================================
// [AsmJit::X86Util]
// ============================================================================

//! @brief Map condition code to "jcc" group of instructions.
ASMJIT_VAR const uint32_t _x86UtilJccFromCond[20];
//! @brief Map condition code to "cmovcc" group of instructions.
ASMJIT_VAR const uint32_t _x86UtilMovccFromCond[20];
//! @brief Map condition code to "setcc" group of instructions.
ASMJIT_VAR const uint32_t _x86UtilSetccFromCond[20];
//! @brief Map condition code to reversed condition code.
ASMJIT_VAR const uint32_t _x86UtilReversedCond[20];

struct X86Util
{
  // --------------------------------------------------------------------------
  // [Condition Codes]
  // --------------------------------------------------------------------------

  //! @brief Get the equivalent of negated condition code.
  static inline uint32_t getNegatedCond(uint32_t cond)
  {
    return static_cast<kX86Cond>(cond ^ static_cast<uint32_t>(cond < kX86CondNone));
  }

  //! @brief Corresponds to transposing the operands of a comparison.
  static inline uint32_t getReversedCond(uint32_t cond)
  {
    ASMJIT_ASSERT(static_cast<uint32_t>(cond) < ASMJIT_ARRAY_SIZE(_x86UtilReversedCond));
    return _x86UtilReversedCond[cond];
  }

  //! @brief Translate condition code @a cc to jcc instruction code.
  //! @sa @c kX86InstCode, @c kX86InstJ.
  static inline uint32_t getJccInstFromCond(uint32_t cond)
  {
    ASMJIT_ASSERT(static_cast<uint32_t>(cond) < ASMJIT_ARRAY_SIZE(_x86UtilJccFromCond));
    return _x86UtilJccFromCond[cond];
  }

  //! @brief Translate condition code @a cc to cmovcc instruction code.
  //! @sa @c kX86InstCode, @c kX86InstCMov.
  static inline uint32_t getCMovccInstFromCond(uint32_t cond)
  {
    ASMJIT_ASSERT(static_cast<uint32_t>(cond) < ASMJIT_ARRAY_SIZE(_x86UtilMovccFromCond));
    return _x86UtilMovccFromCond[cond];
  }

  //! @brief Translate condition code @a cc to setcc instruction code.
  //! @sa @c kX86InstCode, @c kX86InstSet.
  static inline uint32_t getSetccInstFromCond(uint32_t cond)
  {
    ASMJIT_ASSERT(static_cast<uint32_t>(cond) < ASMJIT_ARRAY_SIZE(_x86UtilSetccFromCond));
    return _x86UtilSetccFromCond[cond];
  }

  // --------------------------------------------------------------------------
  // [Variables]
  // --------------------------------------------------------------------------

  static inline uint32_t getVarClassFromVarType(uint32_t varType)
  {
    ASMJIT_ASSERT(varType < kX86VarTypeCount);
    return x86VarInfo[varType].getClass();
  }

  static inline uint32_t getVarSizeFromVarType(uint32_t varType)
  {
    ASMJIT_ASSERT(varType < kX86VarTypeCount);
    return x86VarInfo[varType].getSize();
  }

  static inline uint32_t getRegCodeFromVarType(uint32_t varType, uint32_t regIndex)
  {
    ASMJIT_ASSERT(varType < kX86VarTypeCount);
    return x86VarInfo[varType].getCode() | regIndex;
  }

  static inline bool isVarTypeInt(uint32_t varType)
  {
    ASMJIT_ASSERT(varType < kX86VarTypeCount);
    return (x86VarInfo[varType].getClass() & kX86VarClassGp) != 0;
  }

  static inline bool isVarTypeFloat(uint32_t varType)
  {
    ASMJIT_ASSERT(varType < kX86VarTypeCount);
    return (x86VarInfo[varType].getFlags() & (kX86VarFlagSP | kX86VarFlagDP)) != 0;
  }
};

//! @}

} // AsmJit namespace

// [Api-End]
#include "../core/apiend.h"

// [Guard]
#endif // _ASMJIT_X86_X86UTIL_H
