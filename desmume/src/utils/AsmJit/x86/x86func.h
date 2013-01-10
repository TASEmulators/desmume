// [AsmJit]
// Complete JIT Assembler for C++ Language.
//
// [License]
// Zlib - See COPYING file in this package.

// [Guard]
#ifndef _ASMJIT_X86_X86FUNC_H
#define _ASMJIT_X86_X86FUNC_H

// [Dependencies - AsmJit]
#include "../core/defs.h"
#include "../core/func.h"

#include "../x86/x86defs.h"

// [Api-Begin]
#include "../core/apibegin.h"

namespace AsmJit {

//! @addtogroup AsmJit_X86
//! @{

// ============================================================================
// [AsmJit::TypeId]
// ============================================================================

ASMJIT_DECLARE_TYPE_CORE(kX86VarTypeIntPtr);
ASMJIT_DECLARE_TYPE_ID(void, kVarTypeInvalid);
ASMJIT_DECLARE_TYPE_ID(Void, kVarTypeInvalid);

ASMJIT_DECLARE_TYPE_ID(int8_t, kX86VarTypeGpd);
ASMJIT_DECLARE_TYPE_ID(uint8_t, kX86VarTypeGpd);

ASMJIT_DECLARE_TYPE_ID(int16_t, kX86VarTypeGpd);
ASMJIT_DECLARE_TYPE_ID(uint16_t, kX86VarTypeGpd);

ASMJIT_DECLARE_TYPE_ID(int32_t, kX86VarTypeGpd);
ASMJIT_DECLARE_TYPE_ID(uint32_t, kX86VarTypeGpd);

#if defined(ASMJIT_X64)
ASMJIT_DECLARE_TYPE_ID(int64_t, kX86VarTypeGpq);
ASMJIT_DECLARE_TYPE_ID(uint64_t, kX86VarTypeGpq);
#endif // ASMJIT_X64

ASMJIT_DECLARE_TYPE_ID(float, kX86VarTypeFloat);
ASMJIT_DECLARE_TYPE_ID(double, kX86VarTypeDouble);

// ============================================================================
// [AsmJit::X86FuncDecl]
// ============================================================================

//! @brief X86 function, including calling convention, arguments and their
//! register indices or stack positions.
struct X86FuncDecl : public FuncDecl
{
  // --------------------------------------------------------------------------
  // [Construction / Destruction]
  // --------------------------------------------------------------------------

  //! @brief Create a new @ref FunctionX86 instance.
  inline X86FuncDecl()
  { reset(); }

  // --------------------------------------------------------------------------
  // [Accessors - Core]
  // --------------------------------------------------------------------------

  //! @brief Get stack size needed for function arguments passed on the stack.
  inline uint32_t getArgumentsStackSize() const
  { return _argumentsStackSize; }

  //! @brief Get bit-mask of GP registers used to pass function arguments.
  inline uint32_t getGpArgumentsMask() const
  { return _gpArgumentsMask; }

  //! @brief Get bit-mask of MM registers used to pass function arguments.
  inline uint32_t getMmArgumentsMask() const
  { return _mmArgumentsMask; }

  //! @brief Get bit-mask of XMM registers used to pass function arguments.
  inline uint32_t getXmmArgumentsMask() const
  { return _xmmArgumentsMask; }

  // --------------------------------------------------------------------------
  // [Accessors - Convention]
  // --------------------------------------------------------------------------

  //! @brief Get function calling convention, see @c kX86FuncConv.
  inline uint32_t getConvention() const
  { return _convention; }

  //! @brief Get whether the callee pops the stack.
  inline uint32_t getCalleePopsStack() const
  { return _calleePopsStack; }

  //! @brief Get direction of arguments passed on the stack.
  //!
  //! Direction should be always @c kFuncArgsRTL.
  //!
  //! @note This is related to used calling convention, it's not affected by
  //! number of function arguments or their types.
  inline uint32_t getArgumentsDirection() const
  { return _argumentsDirection; }

  //! @brief Get registers used to pass first integer parameters by current
  //! calling convention.
  //!
  //! @note This is related to used calling convention, it's not affected by
  //! number of function arguments or their types.
  inline const uint8_t* getGpList() const
  { return _gpList; }

  //! @brief Get registers used to pass first SP-FP or DP-FPparameters by
  //! current calling convention.
  //!
  //! @note This is related to used calling convention, it's not affected by
  //! number of function arguments or their types.
  inline const uint8_t* getXmmList() const
  { return _xmmList; }

  //! @brief Get bit-mask of GP registers which might be used for arguments.
  inline uint32_t getGpListMask() const
  { return _gpListMask; }

  //! @brief Get bit-mask of MM registers which might be used for arguments.
  inline uint32_t getMmListMask() const
  { return _mmListMask; }

  //! @brief Get bit-mask of XMM registers which might be used for arguments.
  inline uint32_t getXmmListMask() const
  { return _xmmListMask; }

  //! @brief Get bit-mask of general purpose registers that's preserved
  //! (non-volatile).
  //!
  //! @note This is related to used calling convention, it's not affected by
  //! number of function arguments or their types.
  inline uint32_t getGpPreservedMask() const
  { return _gpPreservedMask; }

  //! @brief Get bit-mask of MM registers that's preserved (non-volatile).
  //!
  //! @note No standardized calling function is not preserving MM registers.
  //! This member is here for extension writers who need for some reason custom
  //! calling convention that can be called through code generated by AsmJit
  //! (or other runtime code generator).
  inline uint32_t getMmPreservedMask() const
  { return _mmPreservedMask; }

  //! @brief Get bit-mask of XMM registers that's preserved (non-volatile).
  //!
  //! @note This is related to used calling convention, it's not affected by
  //! number of function arguments or their types.
  inline uint32_t getXmmPreservedMask() const
  { return _xmmPreservedMask; }

  // --------------------------------------------------------------------------
  // [Methods]
  // --------------------------------------------------------------------------

  //! @brief Find argument ID by the register code.
  ASMJIT_API uint32_t findArgumentByRegCode(uint32_t regCode) const;

  // --------------------------------------------------------------------------
  // [SetPrototype]
  // --------------------------------------------------------------------------

  //! @brief Set function prototype.
  //!
  //! This will set function calling convention and setup arguments variables.
  //!
  //! @note This function will allocate variables, it can be called only once.
  ASMJIT_API void setPrototype(uint32_t convention, uint32_t returnType, const uint32_t* arguments, uint32_t argumentsCount);

  // --------------------------------------------------------------------------
  // [Reset]
  // --------------------------------------------------------------------------

  ASMJIT_API void reset();

  // --------------------------------------------------------------------------
  // [Members - Core]
  // --------------------------------------------------------------------------

  //! @brief Count of bytes consumed by arguments on the stack.
  uint16_t _argumentsStackSize;
  //! @brief Bitmask for GP registers used as passed function arguments.
  uint16_t _gpArgumentsMask;
  //! @brief Bitmask for MM registers used as passed function arguments.
  uint16_t _mmArgumentsMask;
  //! @brief Bitmask for XMM registers used as passed function arguments.
  uint16_t _xmmArgumentsMask;

  // --------------------------------------------------------------------------
  // [Membes - Convention]
  //
  // This section doesn't depend on function arguments or return type. It 
  // depends only on function calling convention and it's filled according to
  // that value.
  // --------------------------------------------------------------------------

  //! @brief Calling convention.
  uint8_t _convention;
  //! @brief Whether a callee pops stack.
  uint8_t _calleePopsStack;
  //! @brief Direction for arguments passed on the stack, see @c kFuncArgsDirection.
  uint8_t _argumentsDirection;
  //! @brief Reserved for future use #1 (alignment).
  uint8_t _reserved1;

  //! @brief List of register IDs used for GP arguments (order is important).
  //!
  //! @note All registers in _gpList are also specified in @ref _gpListMask.
  //! Unused fields are filled by @ref kRegIndexInvalid.
  uint8_t _gpList[16];
  //! @brief List of register IDs used for XMM arguments (order is important).
  //!
  //! @note All registers in _gpList are also specified in @ref _xmmListMask.
  //! Unused fields are filled by @ref kRegIndexInvalid.
  uint8_t _xmmList[16];

  //! @brief Bitmask for GP registers which might be used by arguments.
  //!
  //! @note All registers in _gpListMask are also specified in @ref _gpList.
  uint16_t _gpListMask;
  //! @brief Bitmask for MM registers which might be used by arguments.
  uint16_t _mmListMask;
  //! @brief Bitmask for XMM registers which might be used by arguments.
  //!
  //! @note All registers in _xmmListMask are also specified in @ref _xmmList.
  uint16_t _xmmListMask;

  //! @brief Bitmask for GP registers preserved across the function call.
  //!
  //! @note Preserved register mask is complement to @ref _gpListMask.
  uint16_t _gpPreservedMask;
  //! @brief Bitmask for MM registers preserved across the function call.
  //!
  //! @note Preserved register mask is complement to @ref _mmListMask.
  uint16_t _mmPreservedMask;
  //! @brief Bitmask for XMM registers preserved across the function call.
  //!
  //! @note Preserved register mask is complement to @ref _xmmListMask.
  uint16_t _xmmPreservedMask;
};

//! @}

} // AsmJit namespace

// [Api-End]
#include "../core/apiend.h"

// [Guard]
#endif // _ASMJIT_X86_X86FUNC_H
