// [AsmJit]
// Complete JIT Assembler for C++ Language.
//
// [License]
// Zlib - See COPYING file in this package.

// [Guard]
#ifndef _ASMJIT_X86_X86COMPILERITEM_H
#define _ASMJIT_X86_X86COMPILERITEM_H

// [Dependencies - AsmJit]
#include "../x86/x86assembler.h"
#include "../x86/x86compiler.h"

// [Api-Begin]
#include "../core/apibegin.h"

namespace AsmJit {

//! @addtogroup AsmJit_X86
//! @{

// ============================================================================
// [AsmJit::X86CompilerAlign]
// ============================================================================

//! @brief Compiler align item.
struct X86CompilerAlign : public CompilerAlign
{
  ASMJIT_NO_COPY(X86CompilerAlign)

  // --------------------------------------------------------------------------
  // [Construction / Destruction]
  // --------------------------------------------------------------------------

  //! @brief Create a new @ref CompilerAlign instance.
  ASMJIT_API X86CompilerAlign(X86Compiler* x86Compiler, uint32_t size = 0);
  //! @brief Destroy the @ref CompilerAlign instance.
  ASMJIT_API virtual ~X86CompilerAlign();

  // --------------------------------------------------------------------------
  // [Accessors]
  // --------------------------------------------------------------------------

  //! @brief Get compiler as @ref X86Compiler.
  inline X86Compiler* getCompiler() const
  { return reinterpret_cast<X86Compiler*>(_compiler); }

  // --------------------------------------------------------------------------
  // [Interface]
  // --------------------------------------------------------------------------

  ASMJIT_API virtual void emit(Assembler& a);
};

// ============================================================================
// [AsmJit::X86CompilerHint]
// ============================================================================

//! @brief @ref X86Compiler variable hint item.
struct X86CompilerHint : public CompilerHint
{
  ASMJIT_NO_COPY(X86CompilerHint)

  // --------------------------------------------------------------------------
  // [Construction / Destruction]
  // --------------------------------------------------------------------------

  //! @brief Create a new @ref X86CompilerHint instance.
  ASMJIT_API X86CompilerHint(X86Compiler* compiler, X86CompilerVar* var, uint32_t hintId, uint32_t hintValue);
  //! @brief Destroy the @ref X86CompilerHint instance.
  ASMJIT_API virtual ~X86CompilerHint();

  // --------------------------------------------------------------------------
  // [Accessors]
  // --------------------------------------------------------------------------

  //! @brief Get variable as @ref X86CompilerVar.
  inline X86CompilerVar* getVar() const
  { return reinterpret_cast<X86CompilerVar*>(_var); }

  // --------------------------------------------------------------------------
  // [Interface]
  // --------------------------------------------------------------------------

  ASMJIT_API virtual void prepare(CompilerContext& cc);
  ASMJIT_API virtual CompilerItem* translate(CompilerContext& cc);

  // --------------------------------------------------------------------------
  // [Misc]
  // --------------------------------------------------------------------------

  ASMJIT_API virtual int getMaxSize() const;
};

// ============================================================================
// [AsmJit::X86CompilerTarget]
// ============================================================================

//! @brief X86Compiler target item.
struct X86CompilerTarget : public CompilerTarget
{
  ASMJIT_NO_COPY(X86CompilerTarget)

  // --------------------------------------------------------------------------
  // [Construction / Destruction]
  // --------------------------------------------------------------------------

  //! @brief Create a new @ref X86CompilerTarget instance.
  ASMJIT_API X86CompilerTarget(X86Compiler* x86Compiler, const Label& target);
  //! @brief Destroy the @ref X86CompilerTarget instance.
  ASMJIT_API virtual ~X86CompilerTarget();

  // --------------------------------------------------------------------------
  // [Accessors]
  // --------------------------------------------------------------------------

  //! @brief Get compiler as @ref X86Compiler.
  inline X86Compiler* getCompiler() const
  { return reinterpret_cast<X86Compiler*>(_compiler); }

  //! @brief Get state as @ref X86CompilerState.
  inline X86CompilerState* getState() const
  { return reinterpret_cast<X86CompilerState*>(_state); }

  // --------------------------------------------------------------------------
  // [Interface]
  // --------------------------------------------------------------------------

  ASMJIT_API virtual void prepare(CompilerContext& cc);
  ASMJIT_API virtual CompilerItem* translate(CompilerContext& cc);
  ASMJIT_API virtual void emit(Assembler& a);
};

// ============================================================================
// [AsmJit::X86CompilerInst]
// ============================================================================

//! @brief @ref X86Compiler instruction item.
struct X86CompilerInst : public CompilerInst
{
  ASMJIT_NO_COPY(X86CompilerInst)

  // --------------------------------------------------------------------------
  // [Construction / Destruction]
  // --------------------------------------------------------------------------

  //! @brief Create a new @ref X86CompilerInst instance.
  ASMJIT_API X86CompilerInst(X86Compiler* x86Compiler, uint32_t code, Operand* opData, uint32_t opCount);
  //! @brief Destroy the @ref X86CompilerInst instance.
  ASMJIT_API virtual ~X86CompilerInst();

  // --------------------------------------------------------------------------
  // [Accessors]
  // --------------------------------------------------------------------------

  //! @brief Get compiler as @ref X86Compiler.
  inline X86Compiler* getCompiler() const
  { return reinterpret_cast<X86Compiler*>(_compiler); }

  //! @brief Get whether the instruction is special.
  inline bool isSpecial() const
  { return (_instFlags & kX86CompilerInstFlagIsSpecial) != 0; }

  //! @brief Get whether the instruction is FPU.
  inline bool isFpu() const
  { return (_instFlags & kX86CompilerInstFlagIsFpu) != 0; }

  //! @brief Get whether the instruction is used with GpbLo register.
  inline bool isGpbLoUsed() const
  { return (_instFlags & kX86CompilerInstFlagIsGpbLoUsed) != 0; }

  //! @brief Get whether the instruction is used with GpbHi register.
  inline bool isGpbHiUsed() const
  { return (_instFlags & kX86CompilerInstFlagIsGpbHiUsed) != 0; }

  //! @brief Get memory operand.
  inline Mem* getMemOp()
  { return _memOp; }

  //! @brief Set memory operand.
  inline void setMemOp(Mem* memOp)
  { _memOp = memOp; }

  //! @brief Get operands array (3 operands total).
  inline VarAllocRecord* getVars()
  { return _vars; }

  //! @brief Get operands array (3 operands total).
  inline const VarAllocRecord* getVars() const
  { return _vars; }

  // --------------------------------------------------------------------------
  // [Interface]
  // --------------------------------------------------------------------------

  ASMJIT_API virtual void prepare(CompilerContext& cc);
  ASMJIT_API virtual CompilerItem* translate(CompilerContext& cc);
  ASMJIT_API virtual void emit(Assembler& a);

  // --------------------------------------------------------------------------
  // [Misc]
  // --------------------------------------------------------------------------

  ASMJIT_API virtual int getMaxSize() const;
  ASMJIT_API virtual bool _tryUnuseVar(CompilerVar* v);

  // --------------------------------------------------------------------------
  // [Members]
  // --------------------------------------------------------------------------

  //! @brief Memory operand or NULL.
  Mem* _memOp;
  //! @brief Variables (extracted from operands).
  VarAllocRecord* _vars;
};

// ============================================================================
// [AsmJit::X86CompilerJmpInst]
// ============================================================================

//! @brief @ref X86Compiler "jmp" instruction item.
struct X86CompilerJmpInst : public X86CompilerInst
{
  ASMJIT_NO_COPY(X86CompilerJmpInst)

  // --------------------------------------------------------------------------
  // [Construction / Destruction]
  // --------------------------------------------------------------------------

  ASMJIT_API X86CompilerJmpInst(X86Compiler* x86Compiler, uint32_t code, Operand* opData, uint32_t opCount);
  ASMJIT_API virtual ~X86CompilerJmpInst();

  // --------------------------------------------------------------------------
  // [Accessors]
  // --------------------------------------------------------------------------

  inline X86CompilerJmpInst* getJumpNext() const
  { return _jumpNext; }
  
  inline bool isTaken() const
  { return (_instFlags & kX86CompilerInstFlagIsTaken) != 0; }

  // --------------------------------------------------------------------------
  // [Interface]
  // --------------------------------------------------------------------------

  ASMJIT_API virtual void prepare(CompilerContext& cc);
  ASMJIT_API virtual CompilerItem* translate(CompilerContext& cc);
  ASMJIT_API virtual void emit(Assembler& a);

  // --------------------------------------------------------------------------
  // [DoJump]
  // --------------------------------------------------------------------------

  ASMJIT_API void doJump(CompilerContext& cc);

  // --------------------------------------------------------------------------
  // [Jump]
  // --------------------------------------------------------------------------

  ASMJIT_API virtual CompilerTarget* getJumpTarget() const;

  // --------------------------------------------------------------------------
  // [Members]
  // --------------------------------------------------------------------------

  //! @brief Jump target.
  X86CompilerTarget* _jumpTarget;
  //! @brief Next jump to the same target in a single linked list.
  X86CompilerJmpInst *_jumpNext;
  //! @brief State associated with the jump.
  X86CompilerState* _state;
};

//! @}

} // AsmJit namespace

// [Api-End]
#include "../core/apiend.h"

// [Guard]
#endif // _ASMJIT_X86_X86COMPILERITEM_H
