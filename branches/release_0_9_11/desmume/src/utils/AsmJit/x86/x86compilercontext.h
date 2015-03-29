// [AsmJit]
// Complete JIT Assembler for C++ Language.
//
// [License]
// Zlib - See COPYING file in this package.

// [Guard]
#ifndef _ASMJIT_X86_X86COMPILERCONTEXT_H
#define _ASMJIT_X86_X86COMPILERCONTEXT_H

// [Dependencies - AsmJit]
#include "../core/intutil.h"
#include "../core/podvector.h"

#include "../x86/x86assembler.h"
#include "../x86/x86compiler.h"
#include "../x86/x86compilerfunc.h"
#include "../x86/x86compileritem.h"

// [Api-Begin]
#include "../core/apibegin.h"

namespace AsmJit {

//! @addtogroup AsmJit_X86
//! @{

// ============================================================================
// [AsmJit::X86CompilerContext]
// ============================================================================

//! @internal
//!
//! @brief Compiler context is used by @ref X86Compiler.
//!
//! X86Compiler context is used during compilation and normally developer doesn't
//! need access to it. The context is user per function (it's reset after each
//! function is generated).
struct X86CompilerContext : public CompilerContext
{
  // --------------------------------------------------------------------------
  // [Construction / Destruction]
  // --------------------------------------------------------------------------

  //! @brief Create a new @ref X86CompilerContext instance.
  ASMJIT_API X86CompilerContext(X86Compiler* x86Compiler);
  //! @brief Destroy the @ref X86CompilerContext instance.
  ASMJIT_API ~X86CompilerContext();

  // --------------------------------------------------------------------------
  // [Accessor]
  // --------------------------------------------------------------------------

  //! @brief Get compiler as @ref X86Compiler.
  inline X86Compiler* getCompiler() const
  { return reinterpret_cast<X86Compiler*>(_compiler); }

  //! @brief Get function as @ref X86CompilerFuncDecl.
  inline X86CompilerFuncDecl* getFunc() const
  { return reinterpret_cast<X86CompilerFuncDecl*>(_func); }

  // --------------------------------------------------------------------------
  // [Clear]
  // --------------------------------------------------------------------------

  //! @brief Clear context, preparing it for next function generation.
  ASMJIT_API void _clear();

  // --------------------------------------------------------------------------
  // [Register Allocator]
  // --------------------------------------------------------------------------

  //! @brief Allocate variable
  //!
  //! Calls @c allocGpVar, @c allocMmVar or @c allocXmmVar methods.
  ASMJIT_API void allocVar(X86CompilerVar* cv, uint32_t regMask, uint32_t vflags);

  //! @brief Save variable.
  //!
  //! Calls @c saveGpVar, @c saveMmVar or @c saveXmmVar methods.
  ASMJIT_API void saveVar(X86CompilerVar* cv);

  //! @brief Spill variable.
  //!
  //! Calls @c spillGpVar, @c spillMmVar or @c spillXmmVar methods.
  ASMJIT_API void spillVar(X86CompilerVar* cv);

  //! @brief Unuse variable (didn't spill, just forget about it).
  ASMJIT_API void unuseVar(X86CompilerVar* cv, uint32_t toState);

  //! @brief Helper method that is called for each variable per item.
  inline void _unuseVarOnEndOfScope(CompilerItem* item, X86CompilerVar* cv)
  {
    if (cv->lastItem == item)
      unuseVar(cv, kVarStateUnused);
  }
  //! @overload
  inline void _unuseVarOnEndOfScope(CompilerItem* item, VarAllocRecord* rec)
  {
    X86CompilerVar* cv = rec->vdata;
    if (cv->lastItem == item || (rec->vflags & kVarAllocUnuseAfterUse))
      unuseVar(cv, kVarStateUnused);
  }
  //! @overload
  inline void _unuseVarOnEndOfScope(CompilerItem* item, VarCallRecord* rec)
  {
    X86CompilerVar* v = rec->vdata;
    if (v->lastItem == item || (rec->flags & VarCallRecord::kFlagUnuseAfterUse))
      unuseVar(v, kVarStateUnused);
  }

  //! @brief Allocate variable (GP).
  ASMJIT_API void allocGpVar(X86CompilerVar* cv, uint32_t regMask, uint32_t vflags);
  //! @brief Save variable (GP).
  ASMJIT_API void saveGpVar(X86CompilerVar* cv);
  //! @brief Spill variable (GP).
  ASMJIT_API void spillGpVar(X86CompilerVar* cv);

  //! @brief Allocate variable (MM).
  ASMJIT_API void allocMmVar(X86CompilerVar* cv, uint32_t regMask, uint32_t vflags);
  //! @brief Save variable (MM).
  ASMJIT_API void saveMmVar(X86CompilerVar* cv);
  //! @brief Spill variable (MM).
  ASMJIT_API void spillMmVar(X86CompilerVar* cv);

  //! @brief Allocate variable (XMM).
  ASMJIT_API void allocXmmVar(X86CompilerVar* cv, uint32_t regMask, uint32_t vflags);
  //! @brief Save variable (XMM).
  ASMJIT_API void saveXmmVar(X86CompilerVar* cv);
  //! @brief Spill variable (XMM).
  ASMJIT_API void spillXmmVar(X86CompilerVar* cv);

  //! @brief Emit load variable instruction(s).
  ASMJIT_API void emitLoadVar(X86CompilerVar* cv, uint32_t regIndex);
  //! @brief Emit save variable instruction(s).
  ASMJIT_API void emitSaveVar(X86CompilerVar* cv, uint32_t regIndex);

  //! @brief Emit move variable instruction(s).
  ASMJIT_API void emitMoveVar(X86CompilerVar* cv, uint32_t regIndex, uint32_t vflags);
  //! @brief Emit exchange variable instruction(s).
  ASMJIT_API void emitExchangeVar(X86CompilerVar* cv, uint32_t regIndex, uint32_t vflags, X86CompilerVar* other);

  //! @brief Called each time a variable is alloceted.
  ASMJIT_API void _postAlloc(X86CompilerVar* cv, uint32_t vflags);
  //! @brief Marks variable home memory as used (must be called at least once
  //! for each variable that uses function local memory - stack).
  ASMJIT_API void _markMemoryUsed(X86CompilerVar* cv);

  ASMJIT_API Mem _getVarMem(X86CompilerVar* cv);

  ASMJIT_API X86CompilerVar* _getSpillCandidateGP();
  ASMJIT_API X86CompilerVar* _getSpillCandidateMM();
  ASMJIT_API X86CompilerVar* _getSpillCandidateXMM();
  ASMJIT_API X86CompilerVar* _getSpillCandidateGeneric(X86CompilerVar** varArray, uint32_t count);

  inline bool _isActive(X86CompilerVar* cv)
  { return cv->nextActive != NULL; }
  
  ASMJIT_API void _addActive(X86CompilerVar* cv);
  ASMJIT_API void _freeActive(X86CompilerVar* cv);
  ASMJIT_API void _freeAllActive();

  ASMJIT_API void _allocatedVariable(X86CompilerVar* cv);

  inline void _allocatedGpRegister(uint32_t index)
  {
    _x86State.usedGP |= IntUtil::maskFromIndex(index);
    _modifiedGpRegisters |= IntUtil::maskFromIndex(index);
  }
  
  inline void _allocatedMmRegister(uint32_t index)
  {
    _x86State.usedMM |= IntUtil::maskFromIndex(index);
    _modifiedMmRegisters |= IntUtil::maskFromIndex(index);
  }
  
  inline void _allocatedXmmRegister(uint32_t index)
  {
    _x86State.usedXMM |= IntUtil::maskFromIndex(index);
    _modifiedXmmRegisters |= IntUtil::maskFromIndex(index);
  }

  inline void _freedGpRegister(uint32_t index)
  { _x86State.usedGP &= ~IntUtil::maskFromIndex(index); }

  inline void _freedMmRegister(uint32_t index)
  { _x86State.usedMM &= ~IntUtil::maskFromIndex(index); }

  inline void _freedXmmRegister(uint32_t index)
  { _x86State.usedXMM &= ~IntUtil::maskFromIndex(index); }

  inline void _markGpRegisterModified(uint32_t index)
  { _modifiedGpRegisters |= IntUtil::maskFromIndex(index); }

  inline void _markMmRegisterModified(uint32_t index)
  { _modifiedMmRegisters |= IntUtil::maskFromIndex(index); }

  inline void _markXmmRegisterModified(uint32_t index)
  { _modifiedXmmRegisters |= IntUtil::maskFromIndex(index); }

  // TODO: Find code which uses this and improve.
  inline void _newRegisterHomeIndex(X86CompilerVar* cv, uint32_t idx)
  {
    if (cv->homeRegisterIndex == kRegIndexInvalid)
      cv->homeRegisterIndex = idx;
    cv->prefRegisterMask |= (1U << idx);
  }

  // TODO: Find code which uses this and improve.
  inline void _newRegisterHomeMask(X86CompilerVar* cv, uint32_t mask)
  {
    cv->prefRegisterMask |= mask;
  }

  // --------------------------------------------------------------------------
  // [Operand Patcher]
  // --------------------------------------------------------------------------

  ASMJIT_API void translateOperands(Operand* operands, uint32_t count);

  // --------------------------------------------------------------------------
  // [Backward Code]
  // --------------------------------------------------------------------------

  ASMJIT_API void addBackwardCode(X86CompilerJmpInst* from);

  // --------------------------------------------------------------------------
  // [Forward Jump]
  // --------------------------------------------------------------------------

  ASMJIT_API void addForwardJump(X86CompilerJmpInst* inst);

  // --------------------------------------------------------------------------
  // [State]
  // --------------------------------------------------------------------------

  ASMJIT_API X86CompilerState* _saveState();
  ASMJIT_API void _assignState(X86CompilerState* state);
  ASMJIT_API void _restoreState(X86CompilerState* state, uint32_t targetOffset = kInvalidValue);

  // --------------------------------------------------------------------------
  // [Memory Allocator]
  // --------------------------------------------------------------------------

  ASMJIT_API VarMemBlock* _allocMemBlock(uint32_t size);
  ASMJIT_API void _freeMemBlock(VarMemBlock* mem);

  ASMJIT_API void _allocMemoryOperands();
  ASMJIT_API void _patchMemoryOperands(CompilerItem* start, CompilerItem* stop);

  // --------------------------------------------------------------------------
  // [Members]
  // --------------------------------------------------------------------------

  //! @brief X86 specific compiler state (linked with @ref _state).
  X86CompilerState _x86State;

  //! @brief Forward jumps (single linked list).
  ForwardJumpData* _forwardJumps;

  //! @brief Global modified GP registers mask (per function).
  uint32_t _modifiedGpRegisters;
  //! @brief Global modified MM registers mask (per function).
  uint32_t _modifiedMmRegisters;
  //! @brief Global modified XMM registers mask (per function).
  uint32_t _modifiedXmmRegisters;

  //! @brief Whether the EBP/RBP register can be used by register allocator.
  uint32_t _allocableEBP;

  //! @brief ESP adjust constant (changed during PUSH/POP or when using
  //! stack.
  int _adjustESP;

  //! @brief Function arguments base pointer (register).
  uint32_t _argumentsBaseReg;
  //! @brief Function arguments base offset.
  int32_t _argumentsBaseOffset;
  //! @brief Function arguments displacement.
  int32_t _argumentsActualDisp;

  //! @brief Function variables base pointer (register).
  uint32_t _variablesBaseReg;
  //! @brief Function variables base offset.
  int32_t _variablesBaseOffset;
  //! @brief Function variables displacement.
  int32_t _variablesActualDisp;

  //! @brief Used memory blocks (for variables, here is each created mem block
  //! that can be also in _memFree list).
  VarMemBlock* _memUsed;
  //! @brief Free memory blocks (freed, prepared for another allocation).
  VarMemBlock* _memFree;
  //! @brief Count of 4-byte memory blocks used by the function.
  uint32_t _mem4BlocksCount;
  //! @brief Count of 8-byte memory blocks used by the function.
  uint32_t _mem8BlocksCount;
  //! @brief Count of 16-byte memory blocks used by the function.
  uint32_t _mem16BlocksCount;
  //! @brief Count of total bytes of stack memory used by the function.
  uint32_t _memBytesTotal;

  //! @brief List of items which need to be translated. These items are filled
  //! by @c addBackwardCode().
  PodVector<X86CompilerJmpInst*> _backCode;

  //! @brief Backward code position (starts at 0).
  sysuint_t _backPos;
  //! @brief Whether to emit comments.
  bool _emitComments;
};

//! @}

} // AsmJit namespace

// [Api-End]
#include "../core/apiend.h"

// [Guard]
#endif // _ASMJIT_X86_X86COMPILERCONTEXT_H
