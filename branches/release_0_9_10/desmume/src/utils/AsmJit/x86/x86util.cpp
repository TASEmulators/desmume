// [AsmJit]
// Complete JIT Assembler for C++ Language.
//
// [License]
// Zlib - See COPYING file in this package.

#define ASMJIT_EXPORTS

// [Dependencies - AsmJit]
#include "../x86/x86defs.h"

// [Api-Begin]
#include "../core/apibegin.h"

namespace AsmJit {

// ============================================================================
// [AsmJit::_x86UtilJccFromCond]
// ============================================================================

const uint32_t _x86UtilJccFromCond[20] =
{
  kX86InstJO,
  kX86InstJNO,
  kX86InstJB,
  kX86InstJAE,
  kX86InstJE,
  kX86InstJNE,
  kX86InstJBE,
  kX86InstJA,
  kX86InstJS,
  kX86InstJNS,
  kX86InstJPE,
  kX86InstJPO,
  kX86InstJL,
  kX86InstJGE,
  kX86InstJLE,
  kX86InstJG,

  kInstNone,
  kInstNone,
  kInstNone,
  kInstNone
};

// ============================================================================
// [AsmJit::_x86UtilMovccFromCond]
// ============================================================================

const uint32_t _x86UtilMovccFromCond[20] =
{
  kX86InstCMovO,
  kX86InstCMovNO,
  kX86InstCMovB,
  kX86InstCMovAE,
  kX86InstCMovE,
  kX86InstCMovNE,
  kX86InstCMovBE,
  kX86InstCMovA,
  kX86InstCMovS,
  kX86InstCMovNS,
  kX86InstCMovPE,
  kX86InstCMovPO,
  kX86InstCMovL,
  kX86InstCMovGE,
  kX86InstCMovLE,
  kX86InstCMovG,

  kInstNone,
  kInstNone,
  kInstNone,
  kInstNone
};

// ============================================================================
// [AsmJit::_x86UtilSetccFromCond]
// ============================================================================

const uint32_t _x86UtilSetccFromCond[20] =
{
  kX86InstSetO,
  kX86InstSetNO,
  kX86InstSetB,
  kX86InstSetAE,
  kX86InstSetE,
  kX86InstSetNE,
  kX86InstSetBE,
  kX86InstSetA,
  kX86InstSetS,
  kX86InstSetNS,
  kX86InstSetPE,
  kX86InstSetPO,
  kX86InstSetL,
  kX86InstSetGE,
  kX86InstSetLE,
  kX86InstSetG,

  kInstNone,
  kInstNone,
  kInstNone,
  kInstNone
};

// ============================================================================
// [AsmJit::_x86UtilReversedCond]
// ============================================================================

const uint32_t _x86UtilReversedCond[20] =
{
  /* x86CondO  -> */ kX86CondO,
  /* x86CondNO -> */ kX86CondNO,
  /* x86CondB  -> */ kX86CondA,
  /* x86CondAE -> */ kX86CondBE,
  /* x86CondE  -> */ kX86CondE,
  /* x86CondNE -> */ kX86CondNE,
  /* x86CondBE -> */ kX86CondAE,
  /* x86CondA  -> */ kX86CondB,
  /* x86CondS  -> */ kX86CondS,
  /* x86CondNS -> */ kX86CondNS,
  /* x86CondPE -> */ kX86CondPE,
  /* x86CondPO -> */ kX86CondPO,

  /* x86CondL  -> */ kX86CondG,
  /* x86CondGE -> */ kX86CondLE,

  /* x86CondLE -> */ kX86CondGE,
  /* x86CondG  -> */ kX86CondL,

  /* kX86CondFpuUnordered    -> */ kX86CondFpuUnordered,
  /* kX86CondFpuNotUnordered -> */ kX86CondFpuNotUnordered,

  0x12,
  0x13
};

} // AsmJit namespace

#include "../core/apiend.h"
