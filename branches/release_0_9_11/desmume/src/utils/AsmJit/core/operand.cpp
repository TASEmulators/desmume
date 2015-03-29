// [AsmJit]
// Complete JIT Assembler for C++ Language.
//
// [License]
// Zlib - See COPYING file in this package.

#define ASMJIT_EXPORTS

// [Dependencies - AsmJit]
#include "../core/operand.h"

// [Api-Begin]
#include "../core/apibegin.h"

namespace AsmJit {

// ============================================================================
// [AsmJit::Operand]
// ============================================================================

const Operand noOperand;

// ============================================================================
// [AsmJit::Imm]
// ============================================================================

//! @brief Create signed immediate value operand.
Imm imm(sysint_t i)
{ 
  return Imm(i, false);
}

//! @brief Create unsigned immediate value operand.
Imm uimm(sysuint_t i)
{
  return Imm((sysint_t)i, true);
}

} // AsmJit namespace

// [Api-End]
#include "../core/apiend.h"
