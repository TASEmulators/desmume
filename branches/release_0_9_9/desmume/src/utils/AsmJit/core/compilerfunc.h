// [AsmJit]
// Complete JIT Assembler for C++ Language.
//
// [License]
// Zlib - See COPYING file in this package.

// [Guard]
#ifndef _ASMJIT_CORE_COMPILERFUNC_H
#define _ASMJIT_CORE_COMPILERFUNC_H

// [Dependencies - AsmJit]
#include "../core/compiler.h"
#include "../core/compileritem.h"

// [Api-Begin]
#include "../core/apibegin.h"

namespace AsmJit {

// ============================================================================
// [AsmJit::CompilerFuncDecl]
// ============================================================================

//! @brief Compiler function declaration item.
//!
//! Functions are base blocks for generating assembler output. Each generated
//! assembler stream needs standard entry and leave sequences thats compatible
//! with the operating system conventions (ABI).
//!
//! Function class can be used to generate function prolog) and epilog sequences
//! that are compatible with the demanded calling convention and to allocate and
//! manage variables that can be allocated/spilled during compilation time.
//!
//! @note To create a function use @c Compiler::newFunc() method, do not 
//! create any form of compiler function items using new operator.
//!
//! @sa @ref CompilerState, @ref CompilerVar.
struct CompilerFuncDecl : public CompilerItem
{
  // --------------------------------------------------------------------------
  // [Construction / Destruction]
  // --------------------------------------------------------------------------

  //! @brief Create a new @c CompilerFuncDecl instance.
  //!
  //! @note Always use @c AsmJit::Compiler::newFunc() to create @c Function
  //! instance.
  ASMJIT_API CompilerFuncDecl(Compiler* compiler);
  //! @brief Destroy the @c CompilerFuncDecl instance.
  ASMJIT_API virtual ~CompilerFuncDecl();

  // --------------------------------------------------------------------------
  // [Accessors]
  // --------------------------------------------------------------------------

  //! @brief Get function entry label.
  //!
  //! Entry label can be used to call this function from another code that's
  //! being generated.
  inline const Label& getEntryLabel() const
  { return _entryLabel; }

  //! @brief Get function exit label.
  //!
  //! Use exit label to jump to function epilog.
  inline const Label& getExitLabel() const
  { return _exitLabel; }

  //! @brief Get function entry target.
  inline CompilerTarget* getEntryTarget() const
  { return _entryTarget; }

  //! @brief Get function exit target.
  inline CompilerTarget* getExitTarget() const
  { return _exitTarget; }

  //! @brief Get function end item.
  inline CompilerFuncEnd* getEnd() const
  { return _end; }

  //! @brief Get function declaration.
  inline FuncDecl* getDecl() const
  { return _decl; }

  //! @brief Get function arguments as variables.
  inline CompilerVar** getVars() const
  { return _vars; }

  //! @brief Get function argument at @a index.
  inline CompilerVar* getVar(uint32_t index) const
  {
    ASMJIT_ASSERT(index < _decl->getArgumentsCount());
    return _vars[index];
  }

  //! @brief Get function hints.
  inline uint32_t getFuncHints() const
  { return _funcHints; }

  //! @brief Get function flags.
  inline uint32_t getFuncFlags() const
  { return _funcFlags; }

  //! @brief Get whether the _funcFlags has @a flag
  inline bool hasFuncFlag(uint32_t flag) const
  { return (_funcFlags & flag) != 0; }

  //! @brief Set function @a flag.
  inline void setFuncFlag(uint32_t flag)
  { _funcFlags |= flag; }

  //! @brief Clear function @a flag.
  inline void clearFuncFlag(uint32_t flag)
  { _funcFlags &= ~flag; }
  
  //! @brief Get whether the function is also a caller.
  inline bool isCaller() const
  { return hasFuncFlag(kFuncFlagIsCaller); }

  //! @brief Get whether the function is finished.
  inline bool isFinished() const
  { return hasFuncFlag(kFuncFlagIsFinished); }

  //! @brief Get whether the function is naked.
  inline bool isNaked() const
  { return hasFuncFlag(kFuncFlagIsNaked); }

  //! @brief Get stack size needed to call other functions.
  inline int32_t getFuncCallStackSize() const
  { return _funcCallStackSize; }

  // --------------------------------------------------------------------------
  // [Hints]
  // --------------------------------------------------------------------------

  //! @brief Set function hint.
  ASMJIT_API virtual void setHint(uint32_t hint, uint32_t value);
  //! @brief Get function hint.
  ASMJIT_API virtual uint32_t getHint(uint32_t hint) const;

  // --------------------------------------------------------------------------
  // [Prototype]
  // --------------------------------------------------------------------------

  virtual void setPrototype(
    uint32_t convention, 
    uint32_t returnType,
    const uint32_t* arguments, 
    uint32_t argumentsCount) = 0;

  // --------------------------------------------------------------------------
  // [Members]
  // --------------------------------------------------------------------------

  //! @brief Function entry label.
  Label _entryLabel;
  //! @brief Function exit label.
  Label _exitLabel;
  
  //! @brief Function entry target.
  CompilerTarget* _entryTarget;
  //! @brief Function exit target.
  CompilerTarget* _exitTarget;

  //! @brief Function end item.
  CompilerFuncEnd* _end;

  //! @brief Function declaration.
  FuncDecl* _decl;
  //! @brief Function arguments as compiler variables.
  CompilerVar** _vars;

  //! @brief Function hints;
  uint32_t _funcHints;
  //! @brief Function flags.
  uint32_t _funcFlags;

  //! @brief Stack size needed to call other functions.
  int32_t _funcCallStackSize;
};

// ============================================================================
// [AsmJit::CompilerFuncEnd]
// ============================================================================

//! @brief Compiler function end item.
//!
//! This item does nothing; it's only used by @ref Compiler to mark  specific 
//! location in the code. The @c CompilerFuncEnd is similar to @c CompilerMark,
//! except that it overrides @c translate() to return @c NULL.
struct CompilerFuncEnd : public CompilerItem
{
  ASMJIT_NO_COPY(CompilerFuncEnd)

  // --------------------------------------------------------------------------
  // [Construction / Destruction]
  // --------------------------------------------------------------------------

  //! @brief Create a new @ref CompilerMark instance.
  ASMJIT_API CompilerFuncEnd(Compiler* compiler, CompilerFuncDecl* func);
  //! @brief Destroy the @ref CompilerMark instance.
  ASMJIT_API virtual ~CompilerFuncEnd();

  // --------------------------------------------------------------------------
  // [Accessors]
  // --------------------------------------------------------------------------
  
  //! @brief Get related function.
  inline CompilerFuncDecl* getFunc() const
  { return _func; }

  // --------------------------------------------------------------------------
  // [Interface]
  // --------------------------------------------------------------------------

  ASMJIT_API virtual CompilerItem* translate(CompilerContext& cc);

  // --------------------------------------------------------------------------
  // [Members]
  // --------------------------------------------------------------------------

  //! @brief Related function.
  CompilerFuncDecl* _func;
};

// ============================================================================
// [AsmJit::CompilerFuncRet]
// ============================================================================

//! @brief Compiler return from function item.
struct CompilerFuncRet : public CompilerItem
{
  ASMJIT_NO_COPY(CompilerFuncRet)

  // --------------------------------------------------------------------------
  // [Construction / Destruction]
  // --------------------------------------------------------------------------

  //! @brief Create a new @ref CompilerFuncRet instance.
  ASMJIT_API CompilerFuncRet(Compiler* compiler, CompilerFuncDecl* func,
    const Operand* first, const Operand* second);
  //! @brief Destroy the @ref CompilerFuncRet instance.
  ASMJIT_API virtual ~CompilerFuncRet();

  // --------------------------------------------------------------------------
  // [Accessors]
  // --------------------------------------------------------------------------

  //! @Brief Get the related function.
  inline CompilerFuncDecl* getFunc() const
  { return _func; }

  //! @brief Get the first return operand.
  inline Operand& getFirst()
  { return _ret[0]; }
  
  //! @overload
  inline const Operand& getFirst() const
  { return _ret[0]; }

  //! @brief Get the second return operand.
  inline Operand& getSecond()
  { return _ret[1]; }
 
  //! @overload
  inline const Operand& getSecond() const
  { return _ret[1]; }

  // --------------------------------------------------------------------------
  // [Misc]
  // --------------------------------------------------------------------------

  //! @brief Get whether jump to epilog has to be emitted.
  ASMJIT_API bool mustEmitJump() const;

  // --------------------------------------------------------------------------
  // [Members]
  // --------------------------------------------------------------------------

  //! @brief Related function.
  CompilerFuncDecl* _func;
  //! @brief Return operand(s).
  Operand _ret[2];
};

// ============================================================================
// [AsmJit::CompilerFuncCall]
// ============================================================================

//! @brief Compiler function call item.
struct CompilerFuncCall : public CompilerItem
{
  ASMJIT_NO_COPY(CompilerFuncCall)

  // --------------------------------------------------------------------------
  // [Construction / Destruction]
  // --------------------------------------------------------------------------

  //! @brief Create a new @ref CompilerFuncCall instance.
  ASMJIT_API CompilerFuncCall(Compiler* compiler, CompilerFuncDecl* caller, const Operand* target);
  //! @brief Destroy the @ref CompilerFuncCall instance.
  ASMJIT_API virtual ~CompilerFuncCall();

  // --------------------------------------------------------------------------
  // [Accessors]
  // --------------------------------------------------------------------------

  //! @brief Get caller.
  inline CompilerFuncDecl* getCaller() const
  { return _caller; }

  //! @brief Get function declaration.
  inline FuncDecl* getDecl() const
  { return _decl; }

  //! @brief Get target operand.
  inline Operand& getTarget()
  { return _target; }

  //! @overload
  inline const Operand& getTarget() const 
  { return _target; }

  // --------------------------------------------------------------------------
  // [Prototype]
  // --------------------------------------------------------------------------

  virtual void setPrototype(uint32_t convention, uint32_t returnType, const uint32_t* arguments, uint32_t argumentsCount) = 0;

  //! @brief Set function prototype.
  inline void setPrototype(uint32_t convention, const FuncPrototype& func)
  { setPrototype(convention, func.getReturnType(), func.getArguments(), func.getArgumentsCount()); }

  // --------------------------------------------------------------------------
  // [Members]
  // --------------------------------------------------------------------------

  //! @brief Caller (the function which does the call).
  CompilerFuncDecl* _caller;
  //! @brief Function declaration.
  FuncDecl* _decl;

  //! @brief Operand (address of function, register, label, ...).
  Operand _target;
  //! @brief Return operands.
  Operand _ret[2];
  //! @brief Arguments operands.
  Operand* _args;
};

} // AsmJit namespace

// [Api-End]
#include "../core/apiend.h"

// [Guard]
#endif // _ASMJIT_CORE_COMPILERFUNC_H
