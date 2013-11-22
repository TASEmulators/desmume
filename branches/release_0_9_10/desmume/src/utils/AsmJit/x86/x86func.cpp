// [AsmJit]
// Complete JIT Assembler for C++ Language.
//
// [License]
// Zlib - See COPYING file in this package.

#define ASMJIT_EXPORTS

// [Dependencies - AsmJit]
#include "../core/assert.h"
#include "../core/intutil.h"
#include "../core/stringutil.h"

#include "../x86/x86defs.h"
#include "../x86/x86func.h"
#include "../x86/x86util.h"

// [Api-Begin]
#include "../core/apibegin.h"

namespace AsmJit {

// ============================================================================
// [AsmJit::X86FuncDecl - Accessors]
// ============================================================================

uint32_t X86FuncDecl::findArgumentByRegCode(uint32_t regCode) const
{
  uint32_t type = regCode & kRegTypeMask;
  uint32_t idx = regCode & kRegIndexMask;

  uint32_t clazz;

  switch (type)
  {
    case kX86RegTypeGpd:
    case kX86RegTypeGpq:
      clazz = kX86VarClassGp;
      break;

    case kX86RegTypeX87:
      clazz = kX86VarClassX87;
      break;

    case kX86RegTypeMm:
      clazz = kX86VarClassMm;
      break;

    case kX86RegTypeXmm:
      clazz = kX86VarClassXmm;
      break;

    default:
      return kInvalidValue;
  }

  for (uint32_t i = 0; i < _argumentsCount; i++)
  {
    const FuncArg& arg = _arguments[i];

    if (arg.getRegIndex() == idx && (X86Util::getVarClassFromVarType(arg.getVarType()) & clazz) != 0)
      return i;
  }

  return kInvalidValue;
}

// ============================================================================
// [AsmJit::X86FuncDecl - SetPrototype - InitCallingConvention]
// ============================================================================

static void X86FuncDecl_initCallingConvention(X86FuncDecl* self, uint32_t convention)
{
  uint32_t i;

  // --------------------------------------------------------------------------
  // [Inir]
  // --------------------------------------------------------------------------

  self->_convention = convention;
  self->_calleePopsStack = false;
  self->_argumentsDirection = kFuncArgsRTL;

  for (i = 0; i < ASMJIT_ARRAY_SIZE(self->_gpList); i++)
    self->_gpList[i] = kRegIndexInvalid;

  for (i = 0; i < ASMJIT_ARRAY_SIZE(self->_xmmList); i++)
    self->_xmmList[i] = kRegIndexInvalid;

  self->_gpListMask = 0x0;
  self->_mmListMask = 0x0;
  self->_xmmListMask = 0x0;

  self->_gpPreservedMask = 0x0;
  self->_mmPreservedMask = 0x0;
  self->_xmmPreservedMask = 0x0;

  // --------------------------------------------------------------------------
  // [X86 Calling Conventions]
  // --------------------------------------------------------------------------

#if defined(ASMJIT_X86)
  self->_gpPreservedMask = static_cast<uint16_t>(
    IntUtil::maskFromIndex(kX86RegIndexEbx) |
    IntUtil::maskFromIndex(kX86RegIndexEsp) |
    IntUtil::maskFromIndex(kX86RegIndexEbp) |
    IntUtil::maskFromIndex(kX86RegIndexEsi) |
    IntUtil::maskFromIndex(kX86RegIndexEdi));
  self->_xmmPreservedMask = 0;

  switch (convention)
  {
    // ------------------------------------------------------------------------
    // [CDecl]
    // ------------------------------------------------------------------------

    case kX86FuncConvCDecl:
      break;

    // ------------------------------------------------------------------------
    // [StdCall]
    // ------------------------------------------------------------------------

    case kX86FuncConvStdCall:
      self->_calleePopsStack = true;
      break;

    // ------------------------------------------------------------------------
    // [MS-ThisCall]
    // ------------------------------------------------------------------------

    case kX86FuncConvMsThisCall:
      self->_calleePopsStack = true;

      self->_gpList[0] = kX86RegIndexEcx;

      self->_gpListMask = static_cast<uint16_t>(
        IntUtil::maskFromIndex(kX86RegIndexEcx));
      break;

    // ------------------------------------------------------------------------
    // [MS-FastCall]
    // ------------------------------------------------------------------------

    case kX86FuncConvMsFastCall:
      self->_calleePopsStack = true;

      self->_gpList[0] = kX86RegIndexEcx;
      self->_gpList[1] = kX86RegIndexEdx;

      self->_gpListMask = static_cast<uint16_t>(
        IntUtil::maskFromIndex(kX86RegIndexEcx) |
        IntUtil::maskFromIndex(kX86RegIndexEdx));
      break;

    // ------------------------------------------------------------------------
    // [Borland-FastCall]
    // ------------------------------------------------------------------------

    case kX86FuncConvBorlandFastCall:
      self->_calleePopsStack = true;
      self->_argumentsDirection = kFuncArgsLTR;

      self->_gpList[0] = kX86RegIndexEax;
      self->_gpList[1] = kX86RegIndexEdx;
      self->_gpList[2] = kX86RegIndexEcx;

      self->_gpListMask = static_cast<uint16_t>(
        IntUtil::maskFromIndex(kX86RegIndexEax) |
        IntUtil::maskFromIndex(kX86RegIndexEdx) |
        IntUtil::maskFromIndex(kX86RegIndexEcx));
      break;

    // ------------------------------------------------------------------------
    // [Gcc-FastCall]
    // ------------------------------------------------------------------------

    case kX86FuncConvGccFastCall:
      self->_calleePopsStack = true;

      self->_gpList[0] = kX86RegIndexEcx;
      self->_gpList[1] = kX86RegIndexEdx;

      self->_gpListMask = static_cast<uint16_t>(
        IntUtil::maskFromIndex(kX86RegIndexEcx) |
        IntUtil::maskFromIndex(kX86RegIndexEdx));
      break;

    // ------------------------------------------------------------------------
    // [Gcc-Regparm(1)]
    // ------------------------------------------------------------------------

    case kX86FuncConvGccRegParm1:
      self->_calleePopsStack = false;

      self->_gpList[0] = kX86RegIndexEax;
      self->_gpListMask = static_cast<uint16_t>(
        IntUtil::maskFromIndex(kX86RegIndexEax));
      break;

    // ------------------------------------------------------------------------
    // [Gcc-Regparm(2)]
    // ------------------------------------------------------------------------

    case kX86FuncConvGccRegParm2:
      self->_calleePopsStack = false;

      self->_gpList[0] = kX86RegIndexEax;
      self->_gpList[1] = kX86RegIndexEdx;

      self->_gpListMask = static_cast<uint16_t>(
        IntUtil::maskFromIndex(kX86RegIndexEax) |
        IntUtil::maskFromIndex(kX86RegIndexEdx));
      break;

    // ------------------------------------------------------------------------
    // [Gcc-Regparm(3)]
    // ------------------------------------------------------------------------

    case kX86FuncConvGccRegParm3:
      self->_calleePopsStack = false;

      self->_gpList[0] = kX86RegIndexEax;
      self->_gpList[1] = kX86RegIndexEdx;
      self->_gpList[2] = kX86RegIndexEcx;

      self->_gpListMask = static_cast<uint16_t>(
        IntUtil::maskFromIndex(kX86RegIndexEax) |
        IntUtil::maskFromIndex(kX86RegIndexEdx) |
        IntUtil::maskFromIndex(kX86RegIndexEcx));
      break;

    // ------------------------------------------------------------------------
    // [Illegal]
    // ------------------------------------------------------------------------

    default:
      // Illegal calling convention.
      ASMJIT_ASSERT(0);
  }
#endif // ASMJIT_X86

  // --------------------------------------------------------------------------
  // [X64 Calling Conventions]
  // --------------------------------------------------------------------------

#if defined(ASMJIT_X64)
  switch (convention)
  {
    // ------------------------------------------------------------------------
    // [X64-Windows]
    // ------------------------------------------------------------------------

    case kX86FuncConvX64W:
      self->_gpList[0] = kX86RegIndexRcx;
      self->_gpList[1] = kX86RegIndexRdx;
      self->_gpList[2] = kX86RegIndexR8;
      self->_gpList[3] = kX86RegIndexR9;

      self->_xmmList[0] = kX86RegIndexXmm0;
      self->_xmmList[1] = kX86RegIndexXmm1;
      self->_xmmList[2] = kX86RegIndexXmm2;
      self->_xmmList[3] = kX86RegIndexXmm3;

      self->_gpListMask = static_cast<uint16_t>(
        IntUtil::maskFromIndex(kX86RegIndexRcx  ) |
        IntUtil::maskFromIndex(kX86RegIndexRdx  ) |
        IntUtil::maskFromIndex(kX86RegIndexR8   ) |
        IntUtil::maskFromIndex(kX86RegIndexR9   ));

      self->_xmmListMask = static_cast<uint16_t>(
        IntUtil::maskFromIndex(kX86RegIndexXmm0 ) |
        IntUtil::maskFromIndex(kX86RegIndexXmm1 ) |
        IntUtil::maskFromIndex(kX86RegIndexXmm2 ) |
        IntUtil::maskFromIndex(kX86RegIndexXmm3 ));

      self->_gpPreservedMask = static_cast<uint16_t>(
        IntUtil::maskFromIndex(kX86RegIndexRbx  ) |
        IntUtil::maskFromIndex(kX86RegIndexRsp  ) |
        IntUtil::maskFromIndex(kX86RegIndexRbp  ) |
        IntUtil::maskFromIndex(kX86RegIndexRsi  ) |
        IntUtil::maskFromIndex(kX86RegIndexRdi  ) |
        IntUtil::maskFromIndex(kX86RegIndexR12  ) |
        IntUtil::maskFromIndex(kX86RegIndexR13  ) |
        IntUtil::maskFromIndex(kX86RegIndexR14  ) |
        IntUtil::maskFromIndex(kX86RegIndexR15  ));

      self->_xmmPreservedMask = static_cast<uint16_t>(
        IntUtil::maskFromIndex(kX86RegIndexXmm6 ) |
        IntUtil::maskFromIndex(kX86RegIndexXmm7 ) |
        IntUtil::maskFromIndex(kX86RegIndexXmm8 ) |
        IntUtil::maskFromIndex(kX86RegIndexXmm9 ) |
        IntUtil::maskFromIndex(kX86RegIndexXmm10) |
        IntUtil::maskFromIndex(kX86RegIndexXmm11) |
        IntUtil::maskFromIndex(kX86RegIndexXmm12) |
        IntUtil::maskFromIndex(kX86RegIndexXmm13) |
        IntUtil::maskFromIndex(kX86RegIndexXmm14) |
        IntUtil::maskFromIndex(kX86RegIndexXmm15));
      break;

    // ------------------------------------------------------------------------
    // [X64-Unix]
    // ------------------------------------------------------------------------

    case kX86FuncConvX64U:
      self->_gpList[0] = kX86RegIndexRdi;
      self->_gpList[1] = kX86RegIndexRsi;
      self->_gpList[2] = kX86RegIndexRdx;
      self->_gpList[3] = kX86RegIndexRcx;
      self->_gpList[4] = kX86RegIndexR8;
      self->_gpList[5] = kX86RegIndexR9;

      self->_xmmList[0] = kX86RegIndexXmm0;
      self->_xmmList[1] = kX86RegIndexXmm1;
      self->_xmmList[2] = kX86RegIndexXmm2;
      self->_xmmList[3] = kX86RegIndexXmm3;
      self->_xmmList[4] = kX86RegIndexXmm4;
      self->_xmmList[5] = kX86RegIndexXmm5;
      self->_xmmList[6] = kX86RegIndexXmm6;
      self->_xmmList[7] = kX86RegIndexXmm7;

      self->_gpListMask = static_cast<uint16_t>(
        IntUtil::maskFromIndex(kX86RegIndexRdi  ) |
        IntUtil::maskFromIndex(kX86RegIndexRsi  ) |
        IntUtil::maskFromIndex(kX86RegIndexRdx  ) |
        IntUtil::maskFromIndex(kX86RegIndexRcx  ) |
        IntUtil::maskFromIndex(kX86RegIndexR8   ) |
        IntUtil::maskFromIndex(kX86RegIndexR9   ));

      self->_xmmListMask = static_cast<uint16_t>(
        IntUtil::maskFromIndex(kX86RegIndexXmm0 ) |
        IntUtil::maskFromIndex(kX86RegIndexXmm1 ) |
        IntUtil::maskFromIndex(kX86RegIndexXmm2 ) |
        IntUtil::maskFromIndex(kX86RegIndexXmm3 ) |
        IntUtil::maskFromIndex(kX86RegIndexXmm4 ) |
        IntUtil::maskFromIndex(kX86RegIndexXmm5 ) |
        IntUtil::maskFromIndex(kX86RegIndexXmm6 ) |
        IntUtil::maskFromIndex(kX86RegIndexXmm7 ));

      self->_gpPreservedMask = static_cast<uint16_t>(
        IntUtil::maskFromIndex(kX86RegIndexRbx  ) |
        IntUtil::maskFromIndex(kX86RegIndexRsp  ) |
        IntUtil::maskFromIndex(kX86RegIndexRbp  ) |
        IntUtil::maskFromIndex(kX86RegIndexR12  ) |
        IntUtil::maskFromIndex(kX86RegIndexR13  ) |
        IntUtil::maskFromIndex(kX86RegIndexR14  ) |
        IntUtil::maskFromIndex(kX86RegIndexR15  ));
      break;

    // ------------------------------------------------------------------------
    // [Illegal]
    // ------------------------------------------------------------------------

    default:
      // Illegal calling convention.
      ASMJIT_ASSERT(0);
  }
#endif // ASMJIT_X64
}

// ============================================================================
// [AsmJit::X86FuncDecl - SetPrototype - InitDefinition]
// ============================================================================

static void X86FuncDecl_initDefinition(X86FuncDecl* self,
  uint32_t returnType, const uint32_t* argumentsData, uint32_t argumentsCount)
{
  ASMJIT_ASSERT(argumentsCount <= kFuncArgsMax);

  // --------------------------------------------------------------------------
  // [Init]
  // --------------------------------------------------------------------------

  int32_t i = 0;
  int32_t gpPos = 0;
  int32_t xmmPos = 0;
  int32_t stackOffset = 0;

  self->_returnType = returnType;
  self->_argumentsCount = static_cast<uint8_t>(argumentsCount);

  while (i < static_cast<int32_t>(argumentsCount))
  {
    FuncArg& arg = self->_arguments[i];

    arg._varType = static_cast<uint8_t>(argumentsData[i]);
    arg._regIndex = kRegIndexInvalid;
    arg._stackOffset = kFuncStackInvalid;

    i++;
  }

  while (i < kFuncArgsMax)
  {
    FuncArg& arg = self->_arguments[i];
    arg.reset();

    i++;
  }

  self->_argumentsStackSize = 0;
  self->_gpArgumentsMask = 0x0;
  self->_mmArgumentsMask = 0x0;
  self->_xmmArgumentsMask = 0x0;

  if (self->_argumentsCount == 0)
    return;

  // --------------------------------------------------------------------------
  // [X86 Calling Conventions (32-bit)]
  // --------------------------------------------------------------------------

#if defined(ASMJIT_X86)
  // Register arguments (Integer), always left-to-right.
  for (i = 0; i != argumentsCount; i++)
  {
    FuncArg& arg = self->_arguments[i];
    uint32_t varType = arg.getVarType();

    if (X86Util::isVarTypeInt(varType) && gpPos < 16 && self->_gpList[gpPos] != kRegIndexInvalid)
    {
      arg._regIndex = self->_gpList[gpPos++];
      self->_gpArgumentsMask |= static_cast<uint16_t>(IntUtil::maskFromIndex(arg.getRegIndex()));
    }
  }

  // Stack arguments.
  int32_t iStart = static_cast<int32_t>(argumentsCount - 1);
  int32_t iEnd   = -1;
  int32_t iStep  = -1;

  if (self->_argumentsDirection == kFuncArgsLTR)
  {
    iStart = 0;
    iEnd   = static_cast<int32_t>(argumentsCount);
    iStep  = 1;
  }

  for (i = iStart; i != iEnd; i += iStep)
  {
    FuncArg& arg = self->_arguments[i];
    uint32_t varType = arg.getVarType();

    if (arg.hasRegIndex())
      continue;

    if (X86Util::isVarTypeInt(varType))
    {
      stackOffset -= 4;
      arg._stackOffset = static_cast<int16_t>(stackOffset);
    }
    else if (X86Util::isVarTypeFloat(varType))
    {
      int32_t size = static_cast<int32_t>(x86VarInfo[varType].getSize());
      stackOffset -= size;
      arg._stackOffset = static_cast<int16_t>(stackOffset);
    }
  }
#endif // ASMJIT_X86

  // --------------------------------------------------------------------------
  // [X64 Calling Conventions (64-bit)]
  // --------------------------------------------------------------------------

#if defined(ASMJIT_X64)
  // Windows 64-bit specific.
  if (self->_convention == kX86FuncConvX64W)
  {
    int32_t max = argumentsCount < 4 ? argumentsCount : 4;

    // Register arguments (Integer / FP), always left-to-right.
    for (i = 0; i != max; i++)
    {
      FuncArg& arg = self->_arguments[i];
      uint32_t varType = arg.getVarType();

      if (X86Util::isVarTypeInt(varType))
      {
        arg._regIndex = self->_gpList[i];
        self->_gpArgumentsMask |= static_cast<uint16_t>(IntUtil::maskFromIndex(arg.getRegIndex()));
      }
      else if (X86Util::isVarTypeFloat(varType))
      {
        arg._regIndex = self->_xmmList[i];
        self->_xmmArgumentsMask |= static_cast<uint16_t>(IntUtil::maskFromIndex(arg.getRegIndex()));
      }
    }

    // Stack arguments (always right-to-left).
    for (i = argumentsCount - 1; i != -1; i--)
    {
      FuncArg& arg = self->_arguments[i];
      uint32_t varType = arg.getVarType();

      if (arg.isAssigned())
        continue;

      if (X86Util::isVarTypeInt(varType))
      {
        stackOffset -= 8; // Always 8 bytes.
        arg._stackOffset = stackOffset;
      }
      else if (X86Util::isVarTypeFloat(varType))
      {
        int32_t size = static_cast<int32_t>(x86VarInfo[varType].getSize());
        stackOffset -= size;
        arg._stackOffset = stackOffset;
      }
    }

    // 32 bytes shadow space (X64W calling convention specific).
    stackOffset -= 4 * 8;
  }
  // Linux/Unix 64-bit (AMD64 calling convention).
  else
  {
    // Register arguments (Integer), always left-to-right.
    for (i = 0; i != static_cast<int32_t>(argumentsCount); i++)
    {
      FuncArg& arg = self->_arguments[i];
      uint32_t varType = arg.getVarType();

      if (X86Util::isVarTypeInt(varType) && gpPos < 32 && self->_gpList[gpPos] != kRegIndexInvalid)
      {
        arg._regIndex = self->_gpList[gpPos++];
        self->_gpArgumentsMask |= static_cast<uint16_t>(IntUtil::maskFromIndex(arg.getRegIndex()));
      }
    }

    // Register arguments (FP), always left-to-right.
    for (i = 0; i != static_cast<int32_t>(argumentsCount); i++)
    {
      FuncArg& arg = self->_arguments[i];
      uint32_t varType = arg.getVarType();

      if (X86Util::isVarTypeFloat(varType))
      {
        arg._regIndex = self->_xmmList[xmmPos++];
        self->_xmmArgumentsMask |= static_cast<uint16_t>(IntUtil::maskFromIndex(arg.getRegIndex()));
      }
    }

    // Stack arguments.
    for (i = argumentsCount - 1; i != -1; i--)
    {
      FuncArg& arg = self->_arguments[i];
      uint32_t varType = arg.getVarType();

      if (arg.isAssigned())
        continue;

      if (X86Util::isVarTypeInt(varType))
      {
        stackOffset -= 8;
        arg._stackOffset = static_cast<int16_t>(stackOffset);
      }
      else if (X86Util::isVarTypeFloat(varType))
      {
        int32_t size = (int32_t)x86VarInfo[varType].getSize();

        stackOffset -= size;
        arg._stackOffset = static_cast<int16_t>(stackOffset);
      }
    }
  }
#endif // ASMJIT_X64

  // Modify stack offset (all function parameters will be in positive stack
  // offset that is never zero).
  for (i = 0; i < (int32_t)argumentsCount; i++)
  {
    FuncArg& arg = self->_arguments[i];
    if (!arg.hasRegIndex())
    {
      arg._stackOffset += static_cast<uint16_t>(static_cast<int32_t>(sizeof(uintptr_t)) - stackOffset);
    }
  }

  self->_argumentsStackSize = (uint32_t)(-stackOffset);
}

void X86FuncDecl::setPrototype(uint32_t convention, uint32_t returnType, const uint32_t* arguments, uint32_t argumentsCount)
{
  // Limit maximum function arguments to kFuncArgsMax.
  if (argumentsCount > kFuncArgsMax)
    argumentsCount = kFuncArgsMax;

  X86FuncDecl_initCallingConvention(this, convention);
  X86FuncDecl_initDefinition(this, returnType, arguments, argumentsCount);
}

// ============================================================================
// [AsmJit::X86FuncDecl - Reset]
// ============================================================================

void X86FuncDecl::reset()
{
  uint32_t i;

  // --------------------------------------------------------------------------
  // [Core]
  // --------------------------------------------------------------------------

  _returnType = kVarTypeInvalid;
  _argumentsCount = 0;

  _reserved0[0] = 0;
  _reserved0[1] = 0;

  for (i = 0; i < ASMJIT_ARRAY_SIZE(_arguments); i++)
    _arguments[i].reset();

  _argumentsStackSize = 0;
  _gpArgumentsMask = 0x0;
  _mmArgumentsMask = 0x0;
  _xmmArgumentsMask = 0x0;

  // --------------------------------------------------------------------------
  // [Convention]
  // --------------------------------------------------------------------------

  _convention = kFuncConvNone;
  _calleePopsStack = false;
  _argumentsDirection = kFuncArgsRTL;
  _reserved1 = 0;

  for (i = 0; i < ASMJIT_ARRAY_SIZE(_gpList); i++)
    _gpList[i] = kRegIndexInvalid;

  for (i = 0; i < ASMJIT_ARRAY_SIZE(_xmmList); i++)
    _xmmList[i] = kRegIndexInvalid;

  _gpListMask = 0x0;
  _mmListMask = 0x0;
  _xmmListMask = 0x0;

  _gpPreservedMask = 0x0;
  _mmPreservedMask = 0x0;
  _xmmPreservedMask = 0x0;
}

} // AsmJit namespace

// [Api-Begin]
#include "../core/apiend.h"
