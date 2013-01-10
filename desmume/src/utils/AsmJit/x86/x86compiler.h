// [AsmJit]
// Complete JIT Assembler for C++ Language.
//
// [License]
// Zlib - See COPYING file in this package.

// [Guard]
#ifndef _ASMJIT_X86_X86COMPILER_H
#define _ASMJIT_X86_X86COMPILER_H

// [Dependencies - AsmJit]
#include "../core/build.h"
#include "../core/compiler.h"
#include "../core/compilercontext.h"
#include "../core/compilerfunc.h"
#include "../core/compileritem.h"

#include "../x86/x86assembler.h"
#include "../x86/x86defs.h"
#include "../x86/x86func.h"
#include "../x86/x86util.h"

// [Api-Begin]
#include "../core/apibegin.h"

//! @internal
//!
//! @brief Mark methods not supported by @ref Compiler. These methods are
//! usually used only in function prologs/epilogs or to manage stack.
#define ASMJIT_NOT_SUPPORTED_BY_COMPILER 0

namespace AsmJit {

//! @addtogroup AsmJit_X86
//! @{

// ============================================================================
// [Forward Declarations]
// ============================================================================

struct X86Compiler;
struct X86CompilerAlign;
struct X86CompilerContext;
struct X86CompilerFuncCall;
struct X86CompilerFuncDecl;
struct X86CompilerFuncEnd;
struct X86CompilerInst;
struct X86CompilerJmpInst;
struct X86CompilerState;
struct X86CompilerTarget;
struct X86CompilerVar;

// ============================================================================
// [AsmJit::X86CompilerVar]
// ============================================================================

//! @brief @ref X86Compiler variable.
struct X86CompilerVar : public CompilerVar
{
  // --------------------------------------------------------------------------
  // [AsVar]
  // --------------------------------------------------------------------------

  inline GpVar asGpVar() const
  {
    GpVar var;
    var._var.id = _id;
    var._var.size = _size;
    var._var.regCode = x86VarInfo[_type].getCode();
    var._var.varType = _type;
    return var;
  }

  inline MmVar asMmVar() const
  {
    MmVar var;
    var._var.id = _id;
    var._var.size = _size;
    var._var.regCode = x86VarInfo[_type].getCode();
    var._var.varType = _type;
    return var;
  }

  inline XmmVar asXmmVar() const
  {
    XmmVar var;
    var._var.id = _id;
    var._var.size = _size;
    var._var.regCode = x86VarInfo[_type].getCode();
    var._var.varType = _type;
    return var;
  }

  // --------------------------------------------------------------------------
  // [Members - Scope]
  // --------------------------------------------------------------------------

  //! @brief The first item where the variable is accessed.
  //! @note If this member is @c NULL then variable isn't used.
  CompilerItem* firstItem;
  //! @brief The last item where the variable is accessed.
  CompilerItem* lastItem;

  //! @brief Scope (NULL if variable is global).
  X86CompilerFuncDecl* funcScope;
  //! @brief The first call which is after the @c firstItem.
  X86CompilerFuncCall* funcCall;

  // --------------------------------------------------------------------------
  // [Members - Home]
  // --------------------------------------------------------------------------

  //! @brief Home register index or @c kRegIndexInvalid (used by register allocator).
  uint32_t homeRegisterIndex;
  //! @brief Preferred registers mask.
  uint32_t prefRegisterMask;

  //! @brief Home memory address offset.
  int32_t homeMemoryOffset;
  //! @brief Used by @c CompilerContext, do not touch (initially NULL).
  void* homeMemoryData;

  // --------------------------------------------------------------------------
  // [Members - Actual]
  // --------------------------------------------------------------------------

  //! @brief Actual register index (connected with actual @c X86CompilerState).
  uint32_t regIndex;
  //! @brief Actual working offset. This member is set before register allocator
  //! is called. If workOffset is same as CompilerContext::_currentOffset then
  //! this variable is probably used in next instruction and can't be spilled.
  uint32_t workOffset;

  //! @brief Next active variable in circular double-linked list.
  X86CompilerVar* nextActive;
  //! @brief Previous active variable in circular double-linked list.
  X86CompilerVar* prevActive;

  // --------------------------------------------------------------------------
  // [Members - Flags]
  // --------------------------------------------------------------------------

  //! @brief Variable state (connected with actual @c X86CompilerState).
  uint8_t state;
  //! @brief Whether variable was changed (connected with actual @c X86CompilerState).
  uint8_t changed;
  //! @brief Save on unuse (at end of the variable scope).
  uint8_t saveOnUnuse;

  // --------------------------------------------------------------------------
  // [Members - Statistics]
  // --------------------------------------------------------------------------

  //! @brief Register read access statistics.
  uint32_t regReadCount;
  //! @brief Register write access statistics.
  uint32_t regWriteCount;
  //! @brief Register read/write access statistics (related to a single instruction).
  uint32_t regRwCount;

  //! @brief Register GpbLo access statistics.
  uint32_t regGpbLoCount;
  //! @brief Register GpbHi access statistics.
  uint32_t regGpbHiCount;

  //! @brief Memory read statistics.
  uint32_t memReadCount;
  //! @brief Memory write statistics.
  uint32_t memWriteCount;
  //! @brief Memory read+write statistics.
  uint32_t memRwCount;

  // --------------------------------------------------------------------------
  // [Members - Temporary]
  // --------------------------------------------------------------------------

  //! @brief Temporary data that can be used in prepare/translate stage.
  //!
  //! Initial value is NULL and it's expected that after use it's set back to
  //! NULL.
  //!
  //! The temporary data is designed to be used by algorithms that need to
  //! set some state into variables, do something and then clean-up. See
  //! state switch and function call for details.
  union
  {
    void* tPtr;
    intptr_t tInt;
  };
};

// ============================================================================
// [AsmJit::X86CompilerState]
// ============================================================================

//! @brief @ref X86Compiler state.
struct X86CompilerState : CompilerState
{
  enum
  {
    //! @brief Base for Gp registers.
    kStateRegGpBase = 0,
    //! @brief Base for Mm registers.
    kStateRegMmBase = 16,
    //! @brief Base for Xmm registers.
    kStateRegXmmBase = 24,

    //! @brief Count of all registers in @ref X86CompilerState.
    kStateRegCount = 16 + 8 + 16
  };

  // --------------------------------------------------------------------------
  // [Clear]
  // --------------------------------------------------------------------------

  inline void clear()
  { memset(this, 0, sizeof(*this)); }

  // --------------------------------------------------------------------------
  // [Members]
  // --------------------------------------------------------------------------

  union
  {
    //! @brief All allocated variables in one array.
    X86CompilerVar* regs[kStateRegCount];

    struct
    {
      //! @brief Allocated GP registers.
      X86CompilerVar* gp[16];
      //! @brief Allocated MM registers.
      X86CompilerVar* mm[8];
      //! @brief Allocated XMM registers.
      X86CompilerVar* xmm[16];
    };
  };

  //! @brief Used GP registers bit-mask.
  uint32_t usedGP;
  //! @brief Used MM registers bit-mask.
  uint32_t usedMM;
  //! @brief Used XMM registers bit-mask.
  uint32_t usedXMM;

  //! @brief Changed GP registers bit-mask.
  uint32_t changedGP;
  //! @brief Changed MM registers bit-mask.
  uint32_t changedMM;
  //! @brief Changed XMM registers bit-mask.
  uint32_t changedXMM;

  //! @brief Count of variables in @c memVarsData.
  uint32_t memVarsCount;
  //! @brief Variables stored in memory (@c kVarStateMem).
  //!
  //! When saving / restoring state it's important to keep registers which are
  //! still in memory. Register is always unused when it is going out-of-scope.
  //! All variables which are not here are unused (@c kVarStateUnused).
  X86CompilerVar* memVarsData[1];
};

// ============================================================================
// [AsmJit::VarMemBlock]
// ============================================================================

struct VarMemBlock
{
  int32_t offset;
  uint32_t size;

  VarMemBlock* nextUsed;
  VarMemBlock* nextFree;
};

// ============================================================================
// [AsmJit::VarAllocRecord]
// ============================================================================

//! @brief Variable alloc record (for each instruction that uses variables).
//!
//! Variable record contains pointer to variable data and register allocation
//! flags. These flags are important to determine the best alloc instruction.
struct VarAllocRecord
{
  //! @brief Variable data (the structure owned by @c Compiler).
  X86CompilerVar* vdata;
  //! @brief Variable alloc flags, see @c kVarAllocFlags.
  uint32_t vflags;
  //! @brief Register mask (default is 0).
  uint32_t regMask;
};

// ============================================================================
// [AsmJit::VarCallRecord]
// ============================================================================

//! @brief Variable call-fn record (for each callable that uses variables).
//!
//! This record contains variables that are used to call a function (using 
//! @c X86CompilerFuncCall item). Each variable contains the registers where
//! it must be and registers where the value will be returned.
struct VarCallRecord
{
  //! @brief Variable data (the structure owned by @c Compiler).
  X86CompilerVar* vdata;
  uint32_t flags;

  uint8_t inCount;
  uint8_t inDone;

  uint8_t outCount;
  uint8_t outDone;

  enum FLAGS
  {
    kFlagInGp = 0x0001,
    kFlagInMm = 0x0002,
    kFlagInXmm = 0x0004,
    kFlagInStack = 0x0008,

    kFlagOutEax = 0x0010,
    kFlagOutEdx = 0x0020,
    kFlagOutSt0 = 0x0040,
    kFlagOutSt1 = 0x0080,
    kFlagOutMm0 = 0x0100,
    kFlagOutXmm0 = 0x0400,
    kFlagOutXmm1 = 0x0800,

    kFlagInMemPtr = 0x1000,
    kFlagCallReg = 0x2000,
    kFlagCallMem = 0x4000,
    kFlagUnuseAfterUse = 0x8000
  };
};

// ============================================================================
// [AsmJit::VarHintRecord]
// ============================================================================

struct VarHintRecord
{
  X86CompilerVar* vdata;
  uint32_t hint;
};

// ============================================================================
// [AsmJit::ForwardJumpData]
// ============================================================================

struct ForwardJumpData
{
  X86CompilerJmpInst* inst;
  X86CompilerState* state;
  ForwardJumpData* next;
};

// ============================================================================
// [AsmJit::CompilerUtil]
// ============================================================================

//! @brief Static class that contains utility methods.
struct CompilerUtil
{
  ASMJIT_API static bool isStack16ByteAligned();
};

// ============================================================================
// [AsmJit::X86Compiler]
// ============================================================================

//! @brief Compiler - high level code generation.
//!
//! This class is used to store instruction stream and allows to modify
//! it on the fly. It uses different concept than @c AsmJit::Assembler class
//! and in fact @c AsmJit::Assembler is only used as a backend. Compiler never
//! emits machine code and each instruction you use is stored to instruction
//! array instead. This allows to modify instruction stream later and for
//! example to reorder instructions to make better performance.
//!
//! Using @c AsmJit::Compiler moves code generation to higher level. Higher
//! level constructs allows to write more abstract and extensible code that
//! is not possible with pure @c AsmJit::Assembler class. Because
//! @c AsmJit::Compiler needs to create many objects and lifetime of these
//! objects is small (same as @c AsmJit::Compiler lifetime itself) it uses
//! very fast memory management model. This model allows to create object
//! instances in nearly zero time (compared to @c malloc() or @c new()
//! operators) so overhead by creating machine code by @c AsmJit::Compiler
//! is minimized.
//!
//! @section AsmJit_Compiler_TheStory The Story
//!
//! Before telling you how Compiler works I'd like to write a story. I'd like
//! to cover reasons why this class was created and why I'm recommending to use 
//! it. When I released the first version of AsmJit (0.1) it was a toy. The
//! first function I wrote was function which is still available as testjit and
//! which simply returns 1024. The reason why function works for both 32-bit/
//! 64-bit mode and for Windows/Unix specific calling conventions is luck, no
//! arguments usage and no registers usage except returning value in EAX/RAX.
//!
//! Then I started a project called BlitJit which was targetted to generating
//! JIT code for computer graphics. After writing some lines I decided that I
//! can't join pieces of code together without abstraction, should be
//! pixels source pointer in ESI/RSI or EDI/RDI or it's completelly 
//! irrellevant? What about destination pointer and SSE2 register for reading
//! input pixels? The simple answer might be "just pick some one and use it".
//!
//! Another reason for abstraction is function calling-conventions. It's really
//! not easy to write assembler code for 32-bit and 64-bit platform supporting
//! three calling conventions (32-bit is similar between Windows and Unix, but
//! 64-bit calling conventions are different).
//!
//! At this time I realized that I can't write code which uses named registers,
//! I need to abstract it. In most cases you don't need specific register, you
//! need to emit instruction that does something with 'virtual' register(s),
//! memory, immediate or label.
//!
//! The first version of AsmJit with Compiler was 0.5 (or 0.6?, can't remember).
//! There was support for 32-bit and 64-bit mode, function calling conventions,
//! but when emitting instructions the developer needed to decide which 
//! registers are changed, which are only read or completely overwritten. This
//! model helped a lot when generating code, especially when joining more
//! code-sections together, but there was also small possibility for mistakes.
//! Simply the first version of Compiler was great improvement over low-level 
//! Assembler class, but the API design wasn't perfect.
//!
//! The second version of Compiler, completelly rewritten and based on 
//! different goals, is part of AsmJit starting at version 1.0. This version
//! was designed after the first one and it contains serious improvements over
//! the old one. The first improvement is that you just use instructions with 
//! virtual registers - called variables. When using compiler there is no way
//! to use native registers, there are variables instead. AsmJit is smarter 
//! than before and it knows which register is needed only for read (r), 
//! read/write (w) or overwrite (x). Supported are also instructions which 
//! are using some registers in implicit way (these registers are not part of
//! instruction definition in string form). For example to use CPUID instruction 
//! you must give it four variables which will be automatically allocated to
//! input/output registers (EAX, EBX, ECX, EDX).
//! 
//! Another improvement is algorithm used by a register allocator. In first
//! version the registers were allocated when creating instruction stream. In
//! new version registers are allocated after calling @c Compiler::make(). This
//! means that register allocator has information about scope of all variables
//! and their usage statistics. The algorithm to allocate registers is very
//! simple and it's always called as a 'linear scan register allocator'. When
//! you get out of registers the all possible variables are scored and the worst
//! is spilled. Of course algorithm ignores the variables used for current
//! instruction.
//!
//! In addition, because registers are allocated after the code stream is
//! generated, the state switches between jumps are handled by Compiler too.
//! You don't need to worry about jumps, compiler always do this dirty work 
//! for you.
//!
//! The nearly last thing I'd like to present is calling other functions from 
//! the generated code. AsmJit uses a @c FunctionPrototype class to hold
//! the function parameters, their position in stack (or register index) and
//! function return value. This class is used internally, but it can be
//! used to create your own function calling-convention. All standard function
//! calling conventions are implemented.
//!
//! Please enjoy the new version of Compiler, it was created for writing a
//! low-level code using high-level API, leaving developer to concentrate to
//! real problems and not to solving a register puzzle.
//!
//! @section AsmJit_Compiler_CodeGeneration Code Generation
//!
//! First that is needed to know about compiler is that compiler never emits
//! machine code. It's used as a middleware between @c AsmJit::Assembler and
//! your code. There is also convenience method @c make() that allows to
//! generate machine code directly without creating @c AsmJit::Assembler
//! instance.
//!
//! Comparison of generating machine code through @c Assembler and directly
//! by @c Compiler:
//!
//! @code
//! // Assembler instance is low level code generation class that emits
//! // machine code.
//! X86Assembler a;
//!
//! // Compiler instance is high level code generation class that stores all
//! // instructions in internal representation.
//! X86Compiler c;
//!
//! // ... put your code here ...
//!
//! // Final step - generate code. AsmJit::Compiler::serialize() will serialize
//! // all instructions into Assembler and this ensures generating real machine
//! // code.
//! c.serialize(a);
//!
//! // Your function
//! void* fn = a.make();
//! @endcode
//!
//! Example how to generate machine code using only @c Compiler (preferred):
//!
//! @code
//! // Compiler instance is enough.
//! X86Compiler c;
//!
//! // ... put your code here ...
//!
//! // Your function
//! void* fn = c.make();
//! @endcode
//!
//! You can see that there is @c AsmJit::Compiler::serialize() function that
//! emits instructions into @c AsmJit::Assembler(). This layered architecture
//! means that each class is used for something different and there is no code
//! duplication. For convenience there is also @c AsmJit::Compiler::make()
//! method that can create your function using @c AsmJit::Assembler, but
//! internally (this is preffered bahavior when using @c AsmJit::Compiler).
//!
//! The @c make() method allocates memory using @c Context instance passed
//! into the @c X86Compiler constructor. If code generator is used to create JIT
//! function then virtual memory allocated by @c MemoryManager is used. To get
//! global memory manager use @c MemoryManager::getGlobal().
//!
//! @code
//! // Compiler instance is enough.
//! X86Compiler c;
//!
//! // ... put your code using Compiler instance ...
//!
//! // Your function
//! void* fn = c.make();
//!
//! // Free it if you don't want it anymore
//! // (using global memory manager instance)
//! MemoryManager::getGlobal()->free(fn);
//! @endcode
//!
//! @section AsmJit_Compiler_Functions Functions
//!
//! To build functions with @c Compiler, see @c AsmJit::Compiler::newFunc()
//! method.
//!
//! @section AsmJit_Compiler_Variables Variables
//!
//! Compiler is able to manage variables and function arguments. Internally
//! there is no difference between function argument and variable declared
//! inside. To get function argument you use @c getGpArg() method and to declare
//! variable use @c newGpVar(), @c newMmVar() and @c newXmmVar() methods. The @c newXXX()
//! methods accept also parameter describing the variable type. For example
//! the @c newGpVar() method always creates variable which size matches the target
//! architecture size (for 32-bit target the 32-bit variable is created, for
//! 64-bit target the variable size is 64-bit). To override this behavior the
//! variable type must be specified.
//!
//! @code
//! // Compiler and function declaration - void f(int*);
//! X86Compiler c;
//! c.newFunc(kX86FuncConvDefault, BuildFunction1<int*>());
//!
//! // Get argument variable (it's pointer).
//! GpVar a1(c.getGpArg(0));
//!
//! // Create your variables.
//! GpVar x1(c.newGpVar(kX86VarTypeGpd));
//! GpVar x2(c.newGpVar(kX86VarTypeGpd));
//!
//! // Init your variables.
//! c.mov(x1, 1);
//! c.mov(x2, 2);
//!
//! // ... your code ...
//! c.add(x1, x2);
//! // ... your code ...
//!
//! // Store result to a given pointer in first argument
//! c.mov(dword_ptr(a1), x1);
//!
//! // End of function body.
//! c.endFunc();
//!
//! // Make the function.
//! typedef void (*MyFn)(int*);
//! MyFn fn = asmjit_cast<MyFn>(c.make());
//! @endcode
//!
//! This code snipped needs to be explained. You can see that there are more 
//! variable types that can be used by @c Compiler. Most useful variables can
//! be allocated using general purpose registers (@c GpVar), MMX registers 
//! (@c MmVar) or SSE registers (@c XmmVar).
//!
//! X86/X64 variable types:
//! 
//! - @c kX86VarTypeGpd - 32-bit general purpose register (EAX, EBX, ...).
//! - @c kX86VarTypeGpq - 64-bit general purpose register (RAX, RBX, ...).
//! - @c kX86VarTypeGpz - 32-bit or 64-bit general purpose register, depends
//!   to target architecture. Mapped to @c kX86VarTypeGpd or @c kX86VarTypeGpq.
//!
//! - @c kX86VarTypeX87 - 80-bit floating point stack register st(0 to 7).
//! - @c kX86VarTypeX87SS - 32-bit floating point stack register st(0 to 7).
//! - @c kX86VarTypeX87SD - 64-bit floating point stack register st(0 to 7).
//!
//! - @c VARIALBE_TYPE_MM - 64-bit MMX register.
//!
//! - @c kX86VarTypeXmm - 128-bit SSE register.
//! - @c kX86VarTypeXmmSS - 128-bit SSE register which contains 
//!   scalar 32-bit single precision floating point.
//! - @c kX86VarTypeXmmSD - 128-bit SSE register which contains
//!   scalar 64-bit double precision floating point.
//! - @c kX86VarTypeXmmPS - 128-bit SSE register which contains
//!   4 packed 32-bit single precision floating points.
//! - @c kX86VarTypeXmmPD - 128-bit SSE register which contains
//!   2 packed 64-bit double precision floating points.
//!
//! Unified variable types:
//!
//! - @c kX86VarTypeInt32 - 32-bit general purpose register.
//! - @c kX86VarTypeInt64 - 64-bit general purpose register.
//! - @c kX86VarTypeIntPtr - 32-bit or 64-bit general purpose register / pointer.
//!
//! - @c kX86VarTypeFloat - 32-bit single precision floating point.
//! - @c kX86VarTypeDouble - 64-bit double precision floating point.
//!
//! Variable states:
//!
//! - @c kVarStateUnused - State that is assigned to newly created
//!   variables or to not used variables (dereferenced to zero).
//! - @c kVarStateReg - State that means that variable is currently
//!   allocated in register.
//! - @c kVarStateMem - State that means that variable is currently
//!   only in memory location.
//!
//! When you create new variable, initial state is always @c kVarStateUnused,
//! allocating it to register or spilling to memory changes this state to
//! @c kVarStateReg or @c kVarStateMem, respectively.
//! During variable lifetime it's usual that its state is changed multiple
//! times. To generate better code, you can control allocating and spilling
//! by using up to four types of methods that allows it (see next list).
//!
//! Explicit variable allocating / spilling methods:
//!
//! - @c Compiler::alloc() - Explicit method to alloc variable into
//!      register. You can use this before loops or code blocks.
//!
//! - @c Compiler::spill() - Explicit method to spill variable. If variable
//!      is in register and you call this method, it's moved to its home memory
//!      location. If variable is not in register no operation is performed.
//!
//! - @c Compiler::unuse() - Unuse variable (you can use this to end the
//!      variable scope or sub-scope).
//!
//! Please see AsmJit tutorials (testcompiler.cpp and testvariables.cpp) for
//! more complete examples.
//!
//! @section AsmJit_Compiler_MemoryManagement Memory Management
//!
//! @c Compiler Memory management follows these rules:
//! - Everything created by @c Compiler is always freed by @c Compiler.
//! - To get decent performance, compiler always uses larger memory buffer
//!   for objects to allocate and when compiler instance is destroyed, this
//!   buffer is freed. Destructors of active objects are called when
//!   destroying compiler instance. Destructors of abadonded compiler
//!   objects are called immediately after abadonding them.
//! - This type of memory management is called 'zone memory management'.
//!
//! This means that you can't use any @c Compiler object after destructing it,
//! it also means that each object like @c Label, @c Var and others are created
//! and managed by @c Compiler itself. These objects contain ID which is used
//! internally by Compiler to store additional information about these objects.
//!
//! @section AsmJit_Compiler_StateManagement Control-Flow and State Management.
//!
//! The @c Compiler automatically manages state of the variables when using
//! control flow instructions like jumps, conditional jumps and calls. There
//! is minimal heuristics for choosing the method how state is saved or restored.
//!
//! Generally the state can be changed only when using jump or conditional jump
//! instruction. When using non-conditional jump then state change is embedded
//! into the instruction stream before the jump. When using conditional jump
//! the @c Compiler decides whether to restore state before the jump or whether
//! to use another block where state is restored. The last case is that no-code
//! have to be emitted and there is no state change (this is of course ideal).
//!
//! Choosing whether to embed 'restore-state' section before conditional jump
//! is quite simple. If jump is likely to be 'taken' then code is embedded, if
//! jump is unlikely to be taken then the small code section for state-switch
//! will be generated instead.
//!
//! Next example is the situation where the extended code block is used to
//! do state-change:
//!
//! @code
//! X86Compiler c;
//!
//! c.newFunc(kX86FuncConvDefault, FuncBuilder0<Void>());
//! c.getFunc()->setHint(kFuncHintNaked, true);
//!
//! // Labels.
//! Label L0 = c.newLabel();
//!
//! // Variables.
//! GpVar var0 = c.newGpVar();
//! GpVar var1 = c.newGpVar();
//!
//! // Cleanup. After these two lines, the var0 and var1 will be always stored
//! // in registers. Our example is very small, but in larger code the var0 can
//! // be spilled by xor(var1, var1).
//! c.xor_(var0, var0);
//! c.xor_(var1, var1);
//! c.cmp(var0, var1);
//! // State:
//! //   var0 - register.
//! //   var1 - register.
//!
//! // We manually spill these variables.
//! c.spill(var0);
//! c.spill(var1);
//! // State:
//! //   var0 - memory.
//! //   var1 - memory.
//!
//! // Conditional jump to L0. It will be always taken, but compiler thinks that
//! // it is unlikely taken so it will embed state change code somewhere.
//! c.je(L0);
//!
//! // Do something. The variables var0 and var1 will be allocated again.
//! c.add(var0, 1);
//! c.add(var1, 2);
//! // State:
//! //   var0 - register.
//! //   var1 - register.
//!
//! // Bind label here, the state is not changed.
//! c.bind(L0);
//! // State:
//! //   var0 - register.
//! //   var1 - register.
//!
//! // We need to use var0 and var1, because if compiler detects that variables
//! // are out of scope then it optimizes the state-change.
//! c.sub(var0, var1);
//! // State:
//! //   var0 - register.
//! //   var1 - register.
//!
//! c.endFunc();
//! @endcode
//!
//! The output:
//!
//! @verbatim
//! xor eax, eax                    ; xor var_0, var_0
//! xor ecx, ecx                    ; xor var_1, var_1
//! cmp eax, ecx                    ; cmp var_0, var_1
//! mov [esp - 24], eax             ; spill var_0
//! mov [esp - 28], ecx             ; spill var_1
//! je L0_Switch
//! mov eax, [esp - 24]             ; alloc var_0
//! add eax, 1                      ; add var_0, 1
//! mov ecx, [esp - 28]             ; alloc var_1
//! add ecx, 2                      ; add var_1, 2
//! L0:
//! sub eax, ecx                    ; sub var_0, var_1
//! ret
//!
//! ; state-switch begin
//! L0_Switch0:
//! mov eax, [esp - 24]             ; alloc var_0
//! mov ecx, [esp - 28]             ; alloc var_1
//! jmp short L0
//! ; state-switch end
//! @endverbatim
//!
//! You can see that the state-switch section was generated (see L0_Switch0).
//! The compiler is unable to restore state immediately when emitting the
//! forward jump (the code is generated from first to last instruction and
//! the target state is simply not known at this time).
//!
//! To tell @c Compiler that you want to embed state-switch code before jump
//! it's needed to create backward jump (where also processor expects that it
//! will be taken). To demonstrate the possibility to embed state-switch before
//! jump we use slightly modified code:
//!
//! @code
//! X86Compiler c;
//! 
//! c.newFunc(kX86FuncConvDefault, FuncBuilder0<Void>());
//! c.getFunc()->setHint(kFuncHintNaked, true);
//! 
//! // Labels.
//! Label L0 = c.newLabel();
//! 
//! // Variables.
//! GpVar var0 = c.newGpVar();
//! GpVar var1 = c.newGpVar();
//! 
//! // Cleanup. After these two lines, the var0 and var1 will be always stored
//! // in registers. Our example is very small, but in larger code the var0 can
//! // be spilled by xor(var1, var1).
//! c.xor_(var0, var0);
//! c.xor_(var1, var1);
//! // State:
//! //   var0 - register.
//! //   var1 - register.
//! 
//! // We manually spill these variables.
//! c.spill(var0);
//! c.spill(var1);
//! // State:
//! //   var0 - memory.
//! //   var1 - memory.
//! 
//! // Bind our label here.
//! c.bind(L0);
//! 
//! // Do something, the variables will be allocated again.
//! c.add(var0, 1);
//! c.add(var1, 2);
//! // State:
//! //   var0 - register.
//! //   var1 - register.
//! 
//! // Backward conditional jump to L0. The default behavior is that it is taken
//! // so state-change code will be embedded here.
//! c.je(L0);
//! 
//! c.endFunc();
//! @endcode
//!
//! The output:
//!
//! @verbatim
//! xor ecx, ecx                    ; xor var_0, var_0
//! xor edx, edx                    ; xor var_1, var_1
//! mov [esp - 24], ecx             ; spill var_0
//! mov [esp - 28], edx             ; spill var_1
//! L.2:
//! mov ecx, [esp - 24]             ; alloc var_0
//! add ecx, 1                      ; add var_0, 1
//! mov edx, [esp - 28]             ; alloc var_1
//! add edx, 2                      ; add var_1, 2
//!
//! ; state-switch begin
//! mov [esp - 24], ecx             ; spill var_0
//! mov [esp - 28], edx             ; spill var_1
//! ; state-switch end
//!
//! je short L.2
//! ret
//! @endverbatim
//!
//! Please notice where the state-switch section is located. The @c Compiler 
//! decided that jump is likely to be taken so the state change is embedded
//! before the conditional jump. To change this behavior into the previous
//! case it's needed to add a hint (@c kCondHintLikely or @c kCondHintUnlikely).
//!
//! Replacing the <code>c.je(L0)</code> by <code>c.je(L0, kCondHintUnlikely)
//! will generate code like this:
//!
//! @verbatim
//! xor ecx, ecx                    ; xor var_0, var_0
//! xor edx, edx                    ; xor var_1, var_1
//! mov [esp - 24], ecx             ; spill var_0
//! mov [esp - 28], edx             ; spill var_1
//! L0:
//! mov ecx, [esp - 24]             ; alloc var_0
//! add ecx, 1                      ; add var_0, a
//! mov edx, [esp - 28]             ; alloc var_1
//! add edx, 2                      ; add var_1, 2
//! je L0_Switch, 2
//! ret
//!
//! ; state-switch begin
//! L0_Switch:
//! mov [esp - 24], ecx             ; spill var_0
//! mov [esp - 28], edx             ; spill var_1
//! jmp short L0
//! ; state-switch end
//! @endverbatim
//!
//! This section provided information about how state-change works. The 
//! behavior is deterministic and it can be overridden.
//!
//! @section AsmJit_Compiler_AdvancedCodeGeneration Advanced Code Generation
//!
//! This section describes advanced method of code generation available to
//! @c Compiler (but also to @c Assembler). When emitting code to instruction
//! stream the methods like @c mov(), @c add(), @c sub() can be called directly
//! (advantage is static-type control performed also by C++ compiler) or 
//! indirectly using @c emit() method. The @c emit() method needs only 
//! instruction code and operands.
//!
//! Example of code generating by standard type-safe API:
//!
//! @code
//! X86Compiler c;
//! GpVar var0 = c.newGpVar();
//! GpVar var1 = c.newGpVar();
//!
//! ...
//!
//! c.mov(var0, imm(0));
//! c.add(var0, var1);
//! c.sub(var0, var1);
//! @endcode
//!
//! The code above can be rewritten as:
//!
//! @code
//! X86Compiler c;
//! GpVar var0 = c.newGpVar();
//! GpVar var1 = c.newGpVar();
//!
//! ...
//!
//! c.emit(kX86InstMov, var0, imm(0));
//! c.emit(kX86InstAdd, var0, var1);
//! c.emit(kX86InstSub, var0, var1);
//! @endcode
//!
//! The advantage of first snippet is very friendly API and type-safe control
//! that is controlled by the C++ compiler. The advantage of second snippet is
//! availability to replace or generate instruction code in different places.
//! See the next example how the @c emit() method can be used to generate
//! abstract code.
//!
//! Use case:
//!
//! @code
//! bool emitArithmetic(Compiler& c, XmmVar& var0, XmmVar& var1, const char* op)
//! {
//!   uint code = kInstNone;
//!
//!   if (strcmp(op, "ADD") == 0)
//!     code = kX86InstAddSS;
//!   else if (strcmp(op, "SUBTRACT") == 0)
//!     code = kX86InstSubSS;
//!   else if (strcmp(op, "MULTIPLY") == 0)
//!     code = kX86InstMulSS;
//!   else if (strcmp(op, "DIVIDE") == 0)
//!     code = kX86InstDivSS;
//!   else
//!     // Invalid parameter?
//!     return false;
//!
//!   c.emit(code, var0, var1);
//! }
//! @endcode
//!
//! Other use cases are waiting for you! Be sure that instruction you are 
//! emitting is correct and encodable, because if not, Assembler will set
//! error code to @c kErrorUnknownInstruction.
//!
//! @section AsmJit_Compiler_CompilerDetails Compiler Details
//!
//! This section is here for people interested in the compiling process. There
//! are few steps that must be done for each compiled function (or your code).
//!
//! When your @c Compiler instance is ready, you can create function and add
//! compiler-items using intrinsics or higher level methods implemented by the
//! @c AsmJit::Compiler. When you are done (all instructions serialized) you
//! should call @c AsmJit::Compiler::make() method which will analyze your code,
//! allocate registers and memory for local variables and serialize all items
//! to @c AsmJit::Assembler instance. Next steps shows what's done internally
//! before code is serialized into @c AsmJit::Assembler
//!   (implemented in @c AsmJit::Compiler::serialize() method).
//!
//! 1. Compiler try to match function and end-function items (these items
//!    define function body and blocks).
//!
//! 2. For all items inside the function-body the virtual functions
//!    are called in this order:
//!    - CompilerItem::prepare()
//!    - CompilerItem::translate()
//!    - CompilerItem::emit()
//!    - CompilerItem::post()
//!
//!    There is some extra work when emitting function prolog / epilog and
//!    register allocator.
//!
//! 3. Emit jump tables data.
//!
//! When everything here ends, @c AsmJit::Assembler contains binary stream
//! that needs only relocation to be callable by C/C++ code.
//!
//! @section AsmJit_Compiler_Differences Summary of Differences between @c Assembler and @c Compiler
//!
//! - Instructions are not translated to machine code immediately, they are
//!   stored as emmitables, see @c AsmJit::CompilerItem.
//! - Contains function builder and ability to call other functions.
//! - Contains register allocator and variable management.
//! - Contains a lot of helper methods to simplify the code generation not
//!   available/possible in @c AsmJit::Assembler.
//! - Ability to pre-process or post-process the code which is being generated.
struct X86Compiler : public Compiler
{
  // Special X86 instructions:
  // - cpuid,
  // - cbw, cwd, cwde, cdq, cdqe, cqo
  // - cmpxchg
  // - cmpxchg8b, cmpxchg16b,
  // - daa, das,
  // - imul, mul, idiv, div,
  // - mov_ptr
  // - lahf, sahf
  // - maskmovq, maskmovdqu
  // - enter, leave
  // - ret
  // - monitor, mwait
  // - pop, popad, popfd, popfq,
  // - push, pushad, pushfd, pushfq
  // - rcl, rcr, rol, ror, sal, sar, shl, shr
  // - shld, shrd
  // - rdtsc. rdtscp
  // - lodsb, lodsd, lodsq, lodsw
  // - movsb, movsd, movsq, movsw
  // - stosb, stosd, stosq, stosw
  // - cmpsb, cmpsd, cmpsq, cmpsw
  // - scasb, scasd, scasq, scasw
  //
  // Special X87 instructions:
  // - fisttp

  // --------------------------------------------------------------------------
  // [Construction / Destruction]
  // --------------------------------------------------------------------------

  //! @brief Create a @ref X86Compiler instance.
  ASMJIT_API X86Compiler(Context* context = JitContext::getGlobal());
  //! @brief Destroy the @ref X86Compiler instance.
  ASMJIT_API ~X86Compiler();

  // --------------------------------------------------------------------------
  // [Accessors]
  // --------------------------------------------------------------------------

  //! @brief Get current function as @ref X86CompilerFuncDecl.
  //!
  //! This method can be called within @c newFunc() and @c endFunc()
  //! block to get current function you are working with. It's recommended
  //! to store @c AsmJit::Function pointer returned by @c newFunc<> method,
  //! because this allows you in future implement function sections outside of
  //! function itself (yeah, this is possible!).
  inline X86CompilerFuncDecl* getFunc() const
  { return reinterpret_cast<X86CompilerFuncDecl*>(_func); }

  // --------------------------------------------------------------------------
  // [Function Builder]
  // --------------------------------------------------------------------------

  //! @brief Create a new function.
  //!
  //! @param cconv Calling convention to use (see @c kX86FuncConv enum)
  //! @param params Function arguments prototype.
  //!
  //! This method is usually used as a first step when generating functions
  //! by @c Compiler. First parameter @a cconv specifies function calling
  //! convention to use. Second parameter @a params specifies function
  //! arguments. To create function arguments are used templates
  //! @c BuildFunction0<>, @c BuildFunction1<...>, @c BuildFunction2<...>,
  //! etc...
  //!
  //! Templates with BuildFunction prefix are used to generate argument IDs
  //! based on real C++ types. See next example how to generate function with
  //! two 32-bit integer arguments.
  //!
  //! @code
  //! // Building function using AsmJit::Compiler example.
  //!
  //! // Compiler instance
  //! X86Compiler c;
  //!
  //! // Begin of function (also emits function @c Prolog)
  //! c.newFunc(
  //!   // Default calling convention (32-bit cdecl or 64-bit for host OS)
  //!   kX86FuncConvDefault,
  //!   // Using function builder to generate arguments list
  //!   BuildFunction2<int, int>());
  //!
  //! // End of function (also emits function @c Epilog)
  //! c.endFunc();
  //! @endcode
  //!
  //! You can see that building functions is really easy. Previous code snipped
  //! will generate code for function with two 32-bit integer arguments. You
  //! can access arguments by @c AsmJit::Function::argument() method. Arguments
  //! are indexed from 0 (like everything in C).
  //!
  //! @code
  //! // Accessing function arguments through AsmJit::Function example.
  //!
  //! // Compiler instance
  //! X86Compiler c;
  //!
  //! // Begin of function (also emits function @c Prolog)
  //! c.newFunc(
  //!   // Default calling convention (32-bit cdecl or 64-bit for host OS)
  //!   kX86FuncConvDefault,
  //!   // Using function builder to generate arguments list
  //!   BuildFunction2<int, int>());
  //!
  //! // Arguments are like other variables, you need to reference them by
  //! // variable operands:
  //! GpVar a0 = c.getGpArg(0);
  //! GpVar a1 = c.getGpArg(1);
  //!
  //! // Use them.
  //! c.add(a0, a1);
  //!
  //! // End of function (emits function epilog and return)
  //! c.endFunc();
  //! @endcode
  //!
  //! Arguments are like variables. How to manipulate with variables is
  //! documented in @c AsmJit::Compiler, variables section.
  //!
  //! @note To get current function use @c currentFunction() method or save
  //! pointer to @c AsmJit::Function returned by @c AsmJit::Compiler::newFunc<>
  //! method. Recommended is to save the pointer.
  //!
  //! @sa @c BuildFunction0, @c BuildFunction1, @c BuildFunction2, ...
  inline X86CompilerFuncDecl* newFunc(uint32_t convention, const FuncPrototype& func)
  { return newFunc_(convention, func.getReturnType(), func.getArguments(), func.getArgumentsCount()); }

  //! @brief Create a new function (low level version).
  //!
  //! @param cconv Function calling convention (see @c AsmJit::kX86FuncConv).
  //! @param args Function arguments (see @c AsmJit::kX86VarType).
  //! @param count Arguments count.
  //!
  //! This method is internally called from @c newFunc() method and
  //! contains arguments thats used internally by @c AsmJit::Compiler.
  //!
  //! @note To get current function use @c currentFunction() method.
  ASMJIT_API X86CompilerFuncDecl* newFunc_(uint32_t convenion, uint32_t returnType, const uint32_t* arguments, uint32_t argumentsCount);

  //! @brief End of current function scope and all variables.
  ASMJIT_API X86CompilerFuncDecl* endFunc();

  // --------------------------------------------------------------------------
  // [Emit]
  // --------------------------------------------------------------------------

  //! @brief Emit instruction with no operand.
  ASMJIT_API void _emitInstruction(uint32_t code);

  //! @brief Emit instruction with one operand.
  ASMJIT_API void _emitInstruction(uint32_t code, const Operand* o0);

  //! @brief Emit instruction with two operands.
  ASMJIT_API void _emitInstruction(uint32_t code, const Operand* o0, const Operand* o1);

  //! @brief Emit instruction with three operands.
  ASMJIT_API void _emitInstruction(uint32_t code, const Operand* o0, const Operand* o1, const Operand* o2);

  //! @brief Emit instruction with four operands (Special instructions).
  ASMJIT_API void _emitInstruction(uint32_t code, const Operand* o0, const Operand* o1, const Operand* o2, const Operand* o3);

  //! @brief Emit instruction with five operands (Special instructions).
  ASMJIT_API void _emitInstruction(uint32_t code, const Operand* o0, const Operand* o1, const Operand* o2, const Operand* o3, const Operand* o4);

  //! @brief Private method for emitting jcc.
  ASMJIT_API void _emitJcc(uint32_t code, const Label* label, uint32_t hint);

  //! @brief Private method for emitting function call.
  ASMJIT_API X86CompilerFuncCall* _emitCall(const Operand* o0);

  //! @brief Private method for returning a value from the function.
  ASMJIT_API void _emitReturn(const Operand* first, const Operand* second);

  // --------------------------------------------------------------------------
  // [Align]
  // --------------------------------------------------------------------------

  //! @brief Align target buffer to @a m bytes.
  //!
  //! Typical usage of this is to align labels at start of the inner loops.
  //!
  //! Inserts @c nop() instructions or CPU optimized NOPs.
  ASMJIT_API void align(uint32_t m);

  // --------------------------------------------------------------------------
  // [Label]
  // --------------------------------------------------------------------------

  //! @brief Create and return new label.
  ASMJIT_API Label newLabel();

  //! @brief Bind label to the current offset.
  //!
  //! @note Label can be bound only once!
  ASMJIT_API void bind(const Label& label);

  // --------------------------------------------------------------------------
  // [Variables]
  // --------------------------------------------------------------------------

  //! @brief Get compiler variable at @a id.
  inline X86CompilerVar* _getVar(uint32_t id) const
  {
    ASMJIT_ASSERT(id != kInvalidValue);
    return reinterpret_cast<X86CompilerVar*>(_vars[id & kOperandIdValueMask]);
  }

  //! @internal
  //!
  //! @brief Create a new variable data.
  ASMJIT_API X86CompilerVar* _newVar(const char* name, uint32_t type, uint32_t size);

  //! @brief Create a new general-purpose variable.
  ASMJIT_API GpVar newGpVar(uint32_t varType = kX86VarTypeGpz, const char* name = NULL);
  //! @brief Get argument as general-purpose variable.
  ASMJIT_API GpVar getGpArg(uint32_t argIndex);

  //! @brief Create a new MM variable.
  ASMJIT_API MmVar newMmVar(uint32_t varType = kX86VarTypeMm, const char* name = NULL);
  //! @brief Get argument as MM variable.
  ASMJIT_API MmVar getMmArg(uint32_t argIndex);

  //! @brief Create a new XMM variable.
  ASMJIT_API XmmVar newXmmVar(uint32_t varType = kX86VarTypeXmm, const char* name = NULL);
  //! @brief Get argument as XMM variable.
  ASMJIT_API XmmVar getXmmArg(uint32_t argIndex);

  //! @internal
  //!
  //! @brief Serialize variable hint.
  ASMJIT_API void _vhint(Var& var, uint32_t hintId, uint32_t hintValue);

  //! @brief Alloc variable @a var.
  ASMJIT_API void alloc(Var& var);
  //! @brief Alloc variable @a var using @a regIndex as a register index.
  ASMJIT_API void alloc(Var& var, uint32_t regIndex);
  //! @brief Alloc variable @a var using @a reg as a demanded register.
  ASMJIT_API void alloc(Var& var, const Reg& reg);
  //! @brief Spill variable @a var.
  ASMJIT_API void spill(Var& var);
  //! @brief Save variable @a var if modified.
  ASMJIT_API void save(Var& var);
  //! @brief Unuse variable @a var.
  ASMJIT_API void unuse(Var& var);

  //! @brief Get memory home of variable @a var.
  ASMJIT_API void getMemoryHome(Var& var, GpVar* home, int* displacement = NULL);

  //! @brief Set memory home of variable @a var.
  //!
  //! Default memory home location is on stack (ESP/RSP), but when needed the
  //! bebahior can be changed by this method.
  //!
  //! It is an error to chaining memory home locations. For example the given 
  //! code is invalid:
  //!
  //! @code
  //! X86Compiler c;
  //!
  //! ...
  //! GpVar v0 = c.newGpVar();
  //! GpVar v1 = c.newGpVar();
  //! GpVar v2 = c.newGpVar();
  //! GpVar v3 = c.newGpVar();
  //!
  //! c.setMemoryHome(v1, v0, 0); // Allowed, [v0] is memory home for v1.
  //! c.setMemoryHome(v2, v0, 4); // Allowed, [v0+4] is memory home for v2.
  //! c.setMemoryHome(v3, v2);    // CHAINING, NOT ALLOWED!
  //! @endcode
  ASMJIT_API void setMemoryHome(Var& var, const GpVar& home, int displacement = 0);

  //! @brief Get priority of variable @a var.
  ASMJIT_API uint32_t getPriority(Var& var) const;
  //! @brief Set priority of variable @a var to @a priority.
  ASMJIT_API void setPriority(Var& var, uint32_t priority);

  //! @brief Get save-on-unuse @a var property.
  ASMJIT_API bool getSaveOnUnuse(Var& var) const;
  //! @brief Set save-on-unuse @a var property to @a value.
  ASMJIT_API void setSaveOnUnuse(Var& var, bool value);

  //! @brief Rename variable @a var to @a name.
  //!
  //! @note Only new name will appear in the logger.
  ASMJIT_API void rename(Var& var, const char* name);

  // --------------------------------------------------------------------------
  // [State]
  // --------------------------------------------------------------------------

  //! @internal
  //!
  //! @brief Create a new @ref X86CompilerState.
  ASMJIT_API X86CompilerState* _newState(uint32_t memVarsCount);

  // --------------------------------------------------------------------------
  // [Make]
  // --------------------------------------------------------------------------

  //! @brief Make is convenience method to make currently serialized code and
  //! return pointer to generated function.
  //!
  //! What you need is only to cast this pointer to your function type and call
  //! it. Note that if there was an error and calling @c getError() method doesn't
  //! return @c kErrorOk (zero) then this function always returns @c NULL and
  //! error value remains the same.
  ASMJIT_API virtual void* make();

  //! @brief Method that will emit everything to @c Assembler instance @a a.
  ASMJIT_API virtual void serialize(Assembler& a);

  // --------------------------------------------------------------------------
  // [Data]
  // --------------------------------------------------------------------------

  //! @brief Get target from label @a id.
  inline X86CompilerTarget* _getTarget(uint32_t id)
  {
    ASMJIT_ASSERT((id & kOperandIdTypeMask) == kOperandIdTypeLabel);
    return reinterpret_cast<X86CompilerTarget*>(_targets[id & kOperandIdValueMask]);
  }

  // --------------------------------------------------------------------------
  // [Embed]
  // --------------------------------------------------------------------------

  //! @brief Add 8-bit integer data to the instuction stream.
  inline void db(uint8_t  x) { embed(&x, 1); }
  //! @brief Add 16-bit integer data to the instuction stream.
  inline void dw(uint16_t x) { embed(&x, 2); }
  //! @brief Add 32-bit integer data to the instuction stream.
  inline void dd(uint32_t x) { embed(&x, 4); }
  //! @brief Add 64-bit integer data to the instuction stream.
  inline void dq(uint64_t x) { embed(&x, 8); }

  //! @brief Add 8-bit integer data to the instuction stream.
  inline void dint8(int8_t x) { embed(&x, sizeof(int8_t)); }
  //! @brief Add 8-bit integer data to the instuction stream.
  inline void duint8(uint8_t x) { embed(&x, sizeof(uint8_t)); }

  //! @brief Add 16-bit integer data to the instuction stream.
  inline void dint16(int16_t x) { embed(&x, sizeof(int16_t)); }
  //! @brief Add 16-bit integer data to the instuction stream.
  inline void duint16(uint16_t x) { embed(&x, sizeof(uint16_t)); }

  //! @brief Add 32-bit integer data to the instuction stream.
  inline void dint32(int32_t x) { embed(&x, sizeof(int32_t)); }
  //! @brief Add 32-bit integer data to the instuction stream.
  inline void duint32(uint32_t x) { embed(&x, sizeof(uint32_t)); }

  //! @brief Add 64-bit integer data to the instuction stream.
  inline void dint64(int64_t x) { embed(&x, sizeof(int64_t)); }
  //! @brief Add 64-bit integer data to the instuction stream.
  inline void duint64(uint64_t x) { embed(&x, sizeof(uint64_t)); }

  //! @brief Add system-integer data to the instuction stream.
  inline void dintptr(intptr_t x) { embed(&x, sizeof(intptr_t)); }
  //! @brief Add system-integer data to the instuction stream.
  inline void duintptr(uintptr_t x) { embed(&x, sizeof(uintptr_t)); }

  //! @brief Add float data to the instuction stream.
  inline void dfloat(float x) { embed(&x, sizeof(float)); }
  //! @brief Add double data to the instuction stream.
  inline void ddouble(double x) { embed(&x, sizeof(double)); }

  //! @brief Add pointer data to the instuction stream.
  inline void dptr(void* x) { embed(&x, sizeof(void*)); }

  //! @brief Add MM data to the instuction stream.
  inline void dmm(const MmData& x) { embed(&x, sizeof(MmData)); }
  //! @brief Add XMM data to the instuction stream.
  inline void dxmm(const XmmData& x) { embed(&x, sizeof(XmmData)); }

  //! @brief Add data to the instuction stream.
  inline void data(const void* data, size_t size) { embed(data, size); }

  //! @brief Add data in a given structure instance to the instuction stream.
  template<typename T>
  inline void dstruct(const T& x) { embed(&x, sizeof(T)); }

  // --------------------------------------------------------------------------
  // [Custom Instructions]
  // --------------------------------------------------------------------------

  // These emitters are used by custom compiler code (register alloc / spill,
  // prolog / epilog generator, ...).

  inline void emit(uint32_t code)
  { _emitInstruction(code); }

  inline void emit(uint32_t code, const Operand& o0)
  { _emitInstruction(code, &o0); }

  inline void emit(uint32_t code, const Operand& o0, const Operand& o1)
  { _emitInstruction(code, &o0, &o1); }

  inline void emit(uint32_t code, const Operand& o0, const Operand& o1, const Operand& o2)
  { _emitInstruction(code, &o0, &o1, &o2); }

  // --------------------------------------------------------------------------
  // [X86 Instructions]
  // --------------------------------------------------------------------------

  //! @brief Add with Carry.
  inline void adc(const GpVar& dst, const GpVar& src)
  { _emitInstruction(kX86InstAdc, &dst, &src); }

  //! @brief Add with Carry.
  inline void adc(const GpVar& dst, const Mem& src)
  { _emitInstruction(kX86InstAdc, &dst, &src); }

  //! @brief Add with Carry.
  inline void adc(const GpVar& dst, const Imm& src)
  { _emitInstruction(kX86InstAdc, &dst, &src); }

  //! @brief Add with Carry.
  inline void adc(const Mem& dst, const GpVar& src)
  { _emitInstruction(kX86InstAdc, &dst, &src); }

  //! @brief Add with Carry.
  inline void adc(const Mem& dst, const Imm& src)
  { _emitInstruction(kX86InstAdc, &dst, &src); }

  //! @brief Add.
  inline void add(const GpVar& dst, const GpVar& src)
  { _emitInstruction(kX86InstAdd, &dst, &src); }

  //! @brief Add.
  inline void add(const GpVar& dst, const Mem& src)
  { _emitInstruction(kX86InstAdd, &dst, &src); }

  //! @brief Add.
  inline void add(const GpVar& dst, const Imm& src)
  { _emitInstruction(kX86InstAdd, &dst, &src); }

  //! @brief Add.
  inline void add(const Mem& dst, const GpVar& src)
  { _emitInstruction(kX86InstAdd, &dst, &src); }

  //! @brief Add.
  inline void add(const Mem& dst, const Imm& src)
  { _emitInstruction(kX86InstAdd, &dst, &src); }

  //! @brief Logical And.
  inline void and_(const GpVar& dst, const GpVar& src)
  { _emitInstruction(kX86InstAnd, &dst, &src); }

  //! @brief Logical And.
  inline void and_(const GpVar& dst, const Mem& src)
  { _emitInstruction(kX86InstAnd, &dst, &src); }

  //! @brief Logical And.
  inline void and_(const GpVar& dst, const Imm& src)
  { _emitInstruction(kX86InstAnd, &dst, &src); }

  //! @brief Logical And.
  inline void and_(const Mem& dst, const GpVar& src)
  { _emitInstruction(kX86InstAnd, &dst, &src); }

  //! @brief Logical And.
  inline void and_(const Mem& dst, const Imm& src)
  { _emitInstruction(kX86InstAnd, &dst, &src); }

  //! @brief Bit Scan Forward.
  inline void bsf(const GpVar& dst, const GpVar& src)
  {
    ASMJIT_ASSERT(!dst.isGpb());
    _emitInstruction(kX86InstBsf, &dst, &src);
  }

  //! @brief Bit Scan Forward.
  inline void bsf(const GpVar& dst, const Mem& src)
  {
    ASMJIT_ASSERT(!dst.isGpb());
    _emitInstruction(kX86InstBsf, &dst, &src);
  }

  //! @brief Bit Scan Reverse.
  inline void bsr(const GpVar& dst, const GpVar& src)
  {
    ASMJIT_ASSERT(!dst.isGpb());
    _emitInstruction(kX86InstBsr, &dst, &src);
  }

  //! @brief Bit Scan Reverse.
  inline void bsr(const GpVar& dst, const Mem& src)
  {
    ASMJIT_ASSERT(!dst.isGpb());
    _emitInstruction(kX86InstBsr, &dst, &src);
  }

  //! @brief Byte swap (32-bit or 64-bit registers only) (i486).
  inline void bswap(const GpVar& dst)
  {
    // ASMJIT_ASSERT(dst.getRegType() == kX86RegGPD || dst.getRegType() == kX86RegGPQ);
    _emitInstruction(kX86InstBSwap, &dst);
  }

  //! @brief Bit test.
  inline void bt(const GpVar& dst, const GpVar& src)
  { _emitInstruction(kX86InstBt, &dst, &src); }

  //! @brief Bit test.
  inline void bt(const GpVar& dst, const Imm& src)
  { _emitInstruction(kX86InstBt, &dst, &src); }

  //! @brief Bit test.
  inline void bt(const Mem& dst, const GpVar& src)
  { _emitInstruction(kX86InstBt, &dst, &src); }

  //! @brief Bit test.
  inline void bt(const Mem& dst, const Imm& src)
  { _emitInstruction(kX86InstBt, &dst, &src); }

  //! @brief Bit test and complement.
  inline void btc(const GpVar& dst, const GpVar& src)
  { _emitInstruction(kX86InstBtc, &dst, &src); }

  //! @brief Bit test and complement.
  inline void btc(const GpVar& dst, const Imm& src)
  { _emitInstruction(kX86InstBtc, &dst, &src); }

  //! @brief Bit test and complement.
  inline void btc(const Mem& dst, const GpVar& src)
  { _emitInstruction(kX86InstBtc, &dst, &src); }

  //! @brief Bit test and complement.
  inline void btc(const Mem& dst, const Imm& src)
  { _emitInstruction(kX86InstBtc, &dst, &src); }

  //! @brief Bit test and reset.
  inline void btr(const GpVar& dst, const GpVar& src)
  { _emitInstruction(kX86InstBtr, &dst, &src); }

  //! @brief Bit test and reset.
  inline void btr(const GpVar& dst, const Imm& src)
  { _emitInstruction(kX86InstBtr, &dst, &src); }

  //! @brief Bit test and reset.
  inline void btr(const Mem& dst, const GpVar& src)
  { _emitInstruction(kX86InstBtr, &dst, &src); }

  //! @brief Bit test and reset.
  inline void btr(const Mem& dst, const Imm& src)
  { _emitInstruction(kX86InstBtr, &dst, &src); }

  //! @brief Bit test and set.
  inline void bts(const GpVar& dst, const GpVar& src)
  { _emitInstruction(kX86InstBts, &dst, &src); }

  //! @brief Bit test and set.
  inline void bts(const GpVar& dst, const Imm& src)
  { _emitInstruction(kX86InstBts, &dst, &src); }

  //! @brief Bit test and set.
  inline void bts(const Mem& dst, const GpVar& src)
  { _emitInstruction(kX86InstBts, &dst, &src); }

  //! @brief Bit test and set.
  inline void bts(const Mem& dst, const Imm& src)
  { _emitInstruction(kX86InstBts, &dst, &src); }

  //! @brief Call Procedure.
  inline X86CompilerFuncCall* call(const GpVar& dst)
  { return _emitCall(&dst); }

  //! @brief Call Procedure.
  inline X86CompilerFuncCall* call(const Mem& dst)
  { return _emitCall(&dst); }

  //! @brief Call Procedure.
  inline X86CompilerFuncCall* call(const Imm& dst)
  { return _emitCall(&dst); }

  //! @brief Call Procedure.
  //! @overload
  inline X86CompilerFuncCall* call(void* dst)
  {
    Imm imm((sysint_t)dst);
    return _emitCall(&imm);
  }

  //! @brief Call Procedure.
  inline X86CompilerFuncCall* call(const Label& label)
  { return _emitCall(&label); }

  //! @brief Convert Byte to Word (Sign Extend).
  inline void cbw(const GpVar& dst)
  { _emitInstruction(kX86InstCbw, &dst); }

  //! @brief Convert Word to DWord (Sign Extend).
  inline void cwd(const GpVar& dst, const GpVar& src)
  { _emitInstruction(kX86InstCwd, &dst, &src); }

  //! @brief Convert Word to DWord (Sign Extend).
  inline void cwde(const GpVar& dst)
  { _emitInstruction(kX86InstCwde, &dst); }

  //! @brief Convert Word to DWord (Sign Extend).
  inline void cdq(const GpVar& dst, const GpVar& src)
  { _emitInstruction(kX86InstCdq, &dst, &src); }

#if defined(ASMJIT_X64)
  //! @brief Convert DWord to QWord (Sign Extend).
  inline void cdqe(const GpVar& dst)
  { _emitInstruction(kX86InstCdqe, &dst); }
#endif // ASMJIT_X64

#if defined(ASMJIT_X64)
  //! @brief Convert QWord to DQWord (Sign Extend).
  inline void cqo(const GpVar& dst, const GpVar& src)
  { _emitInstruction(kX86InstCqo, &dst, &src); }
#endif // ASMJIT_X64

  //! @brief Clear Carry flag
  //!
  //! This instruction clears the CF flag in the EFLAGS register.
  inline void clc()
  { _emitInstruction(kX86InstClc); }

  //! @brief Clear Direction flag
  //!
  //! This instruction clears the DF flag in the EFLAGS register.
  inline void cld()
  { _emitInstruction(kX86InstCld); }

  //! @brief Complement Carry Flag.
  //!
  //! This instruction complements the CF flag in the EFLAGS register.
  //! (CF = NOT CF)
  inline void cmc()
  { _emitInstruction(kX86InstCmc); }

  //! @brief Conditional Move.
  inline void cmov(kX86Cond cc, const GpVar& dst, const GpVar& src)
  { _emitInstruction(X86Util::getCMovccInstFromCond(cc), &dst, &src); }

  //! @brief Conditional Move.
  inline void cmov(kX86Cond cc, const GpVar& dst, const Mem& src)
  { _emitInstruction(X86Util::getCMovccInstFromCond(cc), &dst, &src); }

  //! @brief Conditional Move.
  inline void cmova  (const GpVar& dst, const GpVar& src) { _emitInstruction(kX86InstCMovA  , &dst, &src); }
  //! @brief Conditional Move.
  inline void cmova  (const GpVar& dst, const Mem& src)   { _emitInstruction(kX86InstCMovA  , &dst, &src); }
  //! @brief Conditional Move.
  inline void cmovae (const GpVar& dst, const GpVar& src) { _emitInstruction(kX86InstCMovAE , &dst, &src); }
  //! @brief Conditional Move.
  inline void cmovae (const GpVar& dst, const Mem& src)   { _emitInstruction(kX86InstCMovAE , &dst, &src); }
  //! @brief Conditional Move.
  inline void cmovb  (const GpVar& dst, const GpVar& src) { _emitInstruction(kX86InstCMovB  , &dst, &src); }
  //! @brief Conditional Move.
  inline void cmovb  (const GpVar& dst, const Mem& src)   { _emitInstruction(kX86InstCMovB  , &dst, &src); }
  //! @brief Conditional Move.
  inline void cmovbe (const GpVar& dst, const GpVar& src) { _emitInstruction(kX86InstCMovBE , &dst, &src); }
  //! @brief Conditional Move.
  inline void cmovbe (const GpVar& dst, const Mem& src)   { _emitInstruction(kX86InstCMovBE , &dst, &src); }
  //! @brief Conditional Move.
  inline void cmovc  (const GpVar& dst, const GpVar& src) { _emitInstruction(kX86InstCMovC  , &dst, &src); }
  //! @brief Conditional Move.
  inline void cmovc  (const GpVar& dst, const Mem& src)   { _emitInstruction(kX86InstCMovC  , &dst, &src); }
  //! @brief Conditional Move.
  inline void cmove  (const GpVar& dst, const GpVar& src) { _emitInstruction(kX86InstCMovE  , &dst, &src); }
  //! @brief Conditional Move.
  inline void cmove  (const GpVar& dst, const Mem& src)   { _emitInstruction(kX86InstCMovE  , &dst, &src); }
  //! @brief Conditional Move.
  inline void cmovg  (const GpVar& dst, const GpVar& src) { _emitInstruction(kX86InstCMovG  , &dst, &src); }
  //! @brief Conditional Move.
  inline void cmovg  (const GpVar& dst, const Mem& src)   { _emitInstruction(kX86InstCMovG  , &dst, &src); }
  //! @brief Conditional Move.
  inline void cmovge (const GpVar& dst, const GpVar& src) { _emitInstruction(kX86InstCMovGE , &dst, &src); }
  //! @brief Conditional Move.
  inline void cmovge (const GpVar& dst, const Mem& src)   { _emitInstruction(kX86InstCMovGE , &dst, &src); }
  //! @brief Conditional Move.
  inline void cmovl  (const GpVar& dst, const GpVar& src) { _emitInstruction(kX86InstCMovL  , &dst, &src); }
  //! @brief Conditional Move.
  inline void cmovl  (const GpVar& dst, const Mem& src)   { _emitInstruction(kX86InstCMovL  , &dst, &src); }
  //! @brief Conditional Move.
  inline void cmovle (const GpVar& dst, const GpVar& src) { _emitInstruction(kX86InstCMovLE , &dst, &src); }
  //! @brief Conditional Move.
  inline void cmovle (const GpVar& dst, const Mem& src)   { _emitInstruction(kX86InstCMovLE , &dst, &src); }
  //! @brief Conditional Move.
  inline void cmovna (const GpVar& dst, const GpVar& src) { _emitInstruction(kX86InstCMovNA , &dst, &src); }
  //! @brief Conditional Move.
  inline void cmovna (const GpVar& dst, const Mem& src)   { _emitInstruction(kX86InstCMovNA , &dst, &src); }
  //! @brief Conditional Move.
  inline void cmovnae(const GpVar& dst, const GpVar& src) { _emitInstruction(kX86InstCMovNAE, &dst, &src); }
  //! @brief Conditional Move.
  inline void cmovnae(const GpVar& dst, const Mem& src)   { _emitInstruction(kX86InstCMovNAE, &dst, &src); }
  //! @brief Conditional Move.
  inline void cmovnb (const GpVar& dst, const GpVar& src) { _emitInstruction(kX86InstCMovNB , &dst, &src); }
  //! @brief Conditional Move.
  inline void cmovnb (const GpVar& dst, const Mem& src)   { _emitInstruction(kX86InstCMovNB , &dst, &src); }
  //! @brief Conditional Move.
  inline void cmovnbe(const GpVar& dst, const GpVar& src) { _emitInstruction(kX86InstCMovNBE, &dst, &src); }
  //! @brief Conditional Move.
  inline void cmovnbe(const GpVar& dst, const Mem& src)   { _emitInstruction(kX86InstCMovNBE, &dst, &src); }
  //! @brief Conditional Move.
  inline void cmovnc (const GpVar& dst, const GpVar& src) { _emitInstruction(kX86InstCMovNC , &dst, &src); }
  //! @brief Conditional Move.
  inline void cmovnc (const GpVar& dst, const Mem& src)   { _emitInstruction(kX86InstCMovNC , &dst, &src); }
  //! @brief Conditional Move.
  inline void cmovne (const GpVar& dst, const GpVar& src) { _emitInstruction(kX86InstCMovNE , &dst, &src); }
  //! @brief Conditional Move.
  inline void cmovne (const GpVar& dst, const Mem& src)   { _emitInstruction(kX86InstCMovNE , &dst, &src); }
  //! @brief Conditional Move.
  inline void cmovng (const GpVar& dst, const GpVar& src) { _emitInstruction(kX86InstCMovNG , &dst, &src); }
  //! @brief Conditional Move.
  inline void cmovng (const GpVar& dst, const Mem& src)   { _emitInstruction(kX86InstCMovNG , &dst, &src); }
  //! @brief Conditional Move.
  inline void cmovnge(const GpVar& dst, const GpVar& src) { _emitInstruction(kX86InstCMovNGE, &dst, &src); }
  //! @brief Conditional Move.
  inline void cmovnge(const GpVar& dst, const Mem& src)   { _emitInstruction(kX86InstCMovNGE, &dst, &src); }
  //! @brief Conditional Move.
  inline void cmovnl (const GpVar& dst, const GpVar& src) { _emitInstruction(kX86InstCMovNL , &dst, &src); }
  //! @brief Conditional Move.
  inline void cmovnl (const GpVar& dst, const Mem& src)   { _emitInstruction(kX86InstCMovNL , &dst, &src); }
  //! @brief Conditional Move.
  inline void cmovnle(const GpVar& dst, const GpVar& src) { _emitInstruction(kX86InstCMovNLE, &dst, &src); }
  //! @brief Conditional Move.
  inline void cmovnle(const GpVar& dst, const Mem& src)   { _emitInstruction(kX86InstCMovNLE, &dst, &src); }
  //! @brief Conditional Move.
  inline void cmovno (const GpVar& dst, const GpVar& src) { _emitInstruction(kX86InstCMovNO , &dst, &src); }
  //! @brief Conditional Move.
  inline void cmovno (const GpVar& dst, const Mem& src)   { _emitInstruction(kX86InstCMovNO , &dst, &src); }
  //! @brief Conditional Move.
  inline void cmovnp (const GpVar& dst, const GpVar& src) { _emitInstruction(kX86InstCMovNP , &dst, &src); }
  //! @brief Conditional Move.
  inline void cmovnp (const GpVar& dst, const Mem& src)   { _emitInstruction(kX86InstCMovNP , &dst, &src); }
  //! @brief Conditional Move.
  inline void cmovns (const GpVar& dst, const GpVar& src) { _emitInstruction(kX86InstCMovNS , &dst, &src); }
  //! @brief Conditional Move.
  inline void cmovns (const GpVar& dst, const Mem& src)   { _emitInstruction(kX86InstCMovNS , &dst, &src); }
  //! @brief Conditional Move.
  inline void cmovnz (const GpVar& dst, const GpVar& src) { _emitInstruction(kX86InstCMovNZ , &dst, &src); }
  //! @brief Conditional Move.
  inline void cmovnz (const GpVar& dst, const Mem& src)   { _emitInstruction(kX86InstCMovNZ , &dst, &src); }
  //! @brief Conditional Move.
  inline void cmovo  (const GpVar& dst, const GpVar& src) { _emitInstruction(kX86InstCMovO  , &dst, &src); }
  //! @brief Conditional Move.
  inline void cmovo  (const GpVar& dst, const Mem& src)   { _emitInstruction(kX86InstCMovO  , &dst, &src); }
  //! @brief Conditional Move.
  inline void cmovp  (const GpVar& dst, const GpVar& src) { _emitInstruction(kX86InstCMovP  , &dst, &src); }
  //! @brief Conditional Move.
  inline void cmovp  (const GpVar& dst, const Mem& src)   { _emitInstruction(kX86InstCMovP  , &dst, &src); }
  //! @brief Conditional Move.
  inline void cmovpe (const GpVar& dst, const GpVar& src) { _emitInstruction(kX86InstCMovPE , &dst, &src); }
  //! @brief Conditional Move.
  inline void cmovpe (const GpVar& dst, const Mem& src)   { _emitInstruction(kX86InstCMovPE , &dst, &src); }
  //! @brief Conditional Move.
  inline void cmovpo (const GpVar& dst, const GpVar& src) { _emitInstruction(kX86InstCMovPO , &dst, &src); }
  //! @brief Conditional Move.
  inline void cmovpo (const GpVar& dst, const Mem& src)   { _emitInstruction(kX86InstCMovPO , &dst, &src); }
  //! @brief Conditional Move.
  inline void cmovs  (const GpVar& dst, const GpVar& src) { _emitInstruction(kX86InstCMovS  , &dst, &src); }
  //! @brief Conditional Move.
  inline void cmovs  (const GpVar& dst, const Mem& src)   { _emitInstruction(kX86InstCMovS  , &dst, &src); }
  //! @brief Conditional Move.
  inline void cmovz  (const GpVar& dst, const GpVar& src) { _emitInstruction(kX86InstCMovZ  , &dst, &src); }
  //! @brief Conditional Move.
  inline void cmovz  (const GpVar& dst, const Mem& src)   { _emitInstruction(kX86InstCMovZ  , &dst, &src); }

  //! @brief Compare Two Operands.
  inline void cmp(const GpVar& dst, const GpVar& src)
  { _emitInstruction(kX86InstCmp, &dst, &src); }

  //! @brief Compare Two Operands.
  inline void cmp(const GpVar& dst, const Mem& src)
  { _emitInstruction(kX86InstCmp, &dst, &src); }

  //! @brief Compare Two Operands.
  inline void cmp(const GpVar& dst, const Imm& src)
  { _emitInstruction(kX86InstCmp, &dst, &src); }

  //! @brief Compare Two Operands.
  inline void cmp(const Mem& dst, const GpVar& src)
  { _emitInstruction(kX86InstCmp, &dst, &src); }

  //! @brief Compare Two Operands.
  inline void cmp(const Mem& dst, const Imm& src)
  { _emitInstruction(kX86InstCmp, &dst, &src); }

  //! @brief Compare and Exchange (i486).
  inline void cmpxchg(const GpVar cmp_1_eax, const GpVar& cmp_2, const GpVar& src)
  {
    ASMJIT_ASSERT(cmp_1_eax.getId() != src.getId());
    _emitInstruction(kX86InstCmpXCHG, &cmp_1_eax, &cmp_2, &src);
  }

  //! @brief Compare and Exchange (i486).
  inline void cmpxchg(const GpVar cmp_1_eax, const Mem& cmp_2, const GpVar& src)
  {
    ASMJIT_ASSERT(cmp_1_eax.getId() != src.getId());
    _emitInstruction(kX86InstCmpXCHG, &cmp_1_eax, &cmp_2, &src);
  }

  //! @brief Compares the 64-bit value in EDX:EAX with the memory operand (Pentium).
  //!
  //! If the values are equal, then this instruction stores the 64-bit value
  //! in ECX:EBX into the memory operand and sets the zero flag. Otherwise,
  //! this instruction copies the 64-bit memory operand into the EDX:EAX
  //! registers and clears the zero flag.
  inline void cmpxchg8b(
    const GpVar& cmp_edx, const GpVar& cmp_eax,
    const GpVar& cmp_ecx, const GpVar& cmp_ebx,
    const Mem& dst)
  {
    ASMJIT_ASSERT(cmp_edx.getId() != cmp_eax.getId() &&
                  cmp_eax.getId() != cmp_ecx.getId() &&
                  cmp_ecx.getId() != cmp_ebx.getId());

    _emitInstruction(kX86InstCmpXCHG8B, &cmp_edx, &cmp_eax, &cmp_ecx, &cmp_ebx, &dst);
  }

#if defined(ASMJIT_X64)
  //! @brief Compares the 128-bit value in RDX:RAX with the memory operand (X64).
  //!
  //! If the values are equal, then this instruction stores the 128-bit value
  //! in RCX:RBX into the memory operand and sets the zero flag. Otherwise,
  //! this instruction copies the 128-bit memory operand into the RDX:RAX
  //! registers and clears the zero flag.
  inline void cmpxchg16b(
    const GpVar& cmp_edx, const GpVar& cmp_eax,
    const GpVar& cmp_ecx, const GpVar& cmp_ebx,
    const Mem& dst)
  {
    ASMJIT_ASSERT(cmp_edx.getId() != cmp_eax.getId() &&
                  cmp_eax.getId() != cmp_ecx.getId() &&
                  cmp_ecx.getId() != cmp_ebx.getId());

    _emitInstruction(kX86InstCmpXCHG16B, &cmp_edx, &cmp_eax, &cmp_ecx, &cmp_ebx, &dst);
  }
#endif // ASMJIT_X64

  //! @brief CPU Identification (i486).
  inline void cpuid(
    const GpVar& inout_eax,
    const GpVar& out_ebx,
    const GpVar& out_ecx,
    const GpVar& out_edx)
  {
    // Destination variables must be different.
    ASMJIT_ASSERT(inout_eax.getId() != out_ebx.getId() &&
                  out_ebx.getId() != out_ecx.getId() &&
                  out_ecx.getId() != out_edx.getId());

    _emitInstruction(kX86InstCpuId, &inout_eax, &out_ebx, &out_ecx, &out_edx);
  }

#if defined(ASMJIT_X86)
  inline void daa(const GpVar& dst)
  { _emitInstruction(kX86InstDaa, &dst); }
#endif // ASMJIT_X86

#if defined(ASMJIT_X86)
  inline void das(const GpVar& dst)
  { _emitInstruction(kX86InstDas, &dst); }
#endif // ASMJIT_X86

  //! @brief Decrement by 1.
  //! @note This instruction can be slower than sub(dst, 1)
  inline void dec(const GpVar& dst)
  { _emitInstruction(kX86InstDec, &dst); }

  //! @brief Decrement by 1.
  //! @note This instruction can be slower than sub(dst, 1)
  inline void dec(const Mem& dst)
  { _emitInstruction(kX86InstDec, &dst); }

  //! @brief Unsigned divide.
  //!
  //! This instruction divides (unsigned) the value in the AL, AX, or EAX
  //! register by the source operand and stores the result in the AX,
  //! DX:AX, or EDX:EAX registers.
  inline void div(const GpVar& dst_rem, const GpVar& dst_quot, const GpVar& src)
  {
    // Destination variables must be different.
    ASMJIT_ASSERT(dst_rem.getId() != dst_quot.getId());
    _emitInstruction(kX86InstDiv, &dst_rem, &dst_quot, &src);
  }

  //! @brief Unsigned divide.
  //! @overload
  inline void div(const GpVar& dst_rem, const GpVar& dst_quot, const Mem& src)
  {
    // Destination variables must be different.
    ASMJIT_ASSERT(dst_rem.getId() != dst_quot.getId());
    _emitInstruction(kX86InstDiv, &dst_rem, &dst_quot, &src);
  }

#if ASMJIT_NOT_SUPPORTED_BY_COMPILER
  //! @brief Make Stack Frame for Procedure Parameters.
  inline void enter(const Imm& imm16, const Imm& imm8)
  { _emitInstruction(kX86InstEnter, &imm16, &imm8); }
#endif // ASMJIT_NOT_SUPPORTED_BY_COMPILER

  //! @brief Signed divide.
  //!
  //! This instruction divides (signed) the value in the AL, AX, or EAX
  //! register by the source operand and stores the result in the AX,
  //! DX:AX, or EDX:EAX registers.
  inline void idiv(const GpVar& dst_rem, const GpVar& dst_quot, const GpVar& src)
  {
    // Destination variables must be different.
    ASMJIT_ASSERT(dst_rem.getId() != dst_quot.getId());
    _emitInstruction(kX86InstIDiv, &dst_rem, &dst_quot, &src);
  }

  //! @brief Signed divide.
  //! @overload
  inline void idiv(const GpVar& dst_rem, const GpVar& dst_quot, const Mem& src)
  {
    // Destination variables must be different.
    ASMJIT_ASSERT(dst_rem.getId() != dst_quot.getId());
    _emitInstruction(kX86InstIDiv, &dst_rem, &dst_quot, &src);
  }

  //! @brief Signed multiply.
  //!
  //! [dst_lo:dst_hi] = dst_hi * src.
  inline void imul(const GpVar& dst_hi, const GpVar& dst_lo, const GpVar& src)
  {
    // Destination variables must be different.
    ASMJIT_ASSERT(dst_hi.getId() != dst_lo.getId());
    _emitInstruction(kX86InstIMul, &dst_hi, &dst_lo, &src);
  }

  //! @overload
  inline void imul(const GpVar& dst_hi, const GpVar& dst_lo, const Mem& src)
  {
    // Destination variables must be different.
    ASMJIT_ASSERT(dst_hi.getId() != dst_lo.getId());
    _emitInstruction(kX86InstIMul, &dst_hi, &dst_lo, &src);
  }

  //! @brief Signed multiply.
  //!
  //! Destination operand (the first operand) is multiplied by the source
  //! operand (second operand). The destination operand is a general-purpose
  //! register and the source operand is an immediate value, a general-purpose
  //! register, or a memory location. The product is then stored in the
  //! destination operand location.
  inline void imul(const GpVar& dst, const GpVar& src)
  { _emitInstruction(kX86InstIMul, &dst, &src); }

  //! @brief Signed multiply.
  //! @overload
  inline void imul(const GpVar& dst, const Mem& src)
  { _emitInstruction(kX86InstIMul, &dst, &src); }

  //! @brief Signed multiply.
  //! @overload
  inline void imul(const GpVar& dst, const Imm& src)
  { _emitInstruction(kX86InstIMul, &dst, &src); }

  //! @brief Signed multiply.
  //!
  //! source operand (which can be a general-purpose register or a memory
  //! location) is multiplied by the second source operand (an immediate
  //! value). The product is then stored in the destination operand
  //! (a general-purpose register).
  inline void imul(const GpVar& dst, const GpVar& src, const Imm& imm)
  { _emitInstruction(kX86InstIMul, &dst, &src, &imm); }

  //! @overload
  inline void imul(const GpVar& dst, const Mem& src, const Imm& imm)
  { _emitInstruction(kX86InstIMul, &dst, &src, &imm); }

  //! @brief Increment by 1.
  //! @note This instruction can be slower than add(dst, 1)
  inline void inc(const GpVar& dst)
  { _emitInstruction(kX86InstInc, &dst); }

  //! @brief Increment by 1.
  //! @note This instruction can be slower than add(dst, 1)
  inline void inc(const Mem& dst)
  { _emitInstruction(kX86InstInc, &dst); }

  //! @brief Interrupt 3 - trap to debugger.
  inline void int3()
  { _emitInstruction(kX86InstInt3); }

  //! @brief Jump to label @a label if condition @a cc is met.
  //!
  //! This instruction checks the state of one or more of the status flags in
  //! the EFLAGS register (CF, OF, PF, SF, and ZF) and, if the flags are in the
  //! specified state (condition), performs a jump to the target instruction
  //! specified by the destination operand. A condition code (cc) is associated
  //! with each instruction to indicate the condition being tested for. If the
  //! condition is not satisfied, the jump is not performed and execution
  //! continues with the instruction following the Jcc instruction.
  inline void j(kX86Cond cc, const Label& label, uint32_t hint = kCondHintNone)
  { _emitJcc(X86Util::getJccInstFromCond(cc), &label, hint); }

  //! @brief Jump to label @a label if condition is met.
  inline void ja  (const Label& label, uint32_t hint = kCondHintNone) { _emitJcc(kX86InstJA  , &label, hint); }
  //! @brief Jump to label @a label if condition is met.
  inline void jae (const Label& label, uint32_t hint = kCondHintNone) { _emitJcc(kX86InstJAE , &label, hint); }
  //! @brief Jump to label @a label if condition is met.
  inline void jb  (const Label& label, uint32_t hint = kCondHintNone) { _emitJcc(kX86InstJB  , &label, hint); }
  //! @brief Jump to label @a label if condition is met.
  inline void jbe (const Label& label, uint32_t hint = kCondHintNone) { _emitJcc(kX86InstJBE , &label, hint); }
  //! @brief Jump to label @a label if condition is met.
  inline void jc  (const Label& label, uint32_t hint = kCondHintNone) { _emitJcc(kX86InstJC  , &label, hint); }
  //! @brief Jump to label @a label if condition is met.
  inline void je  (const Label& label, uint32_t hint = kCondHintNone) { _emitJcc(kX86InstJE  , &label, hint); }
  //! @brief Jump to label @a label if condition is met.
  inline void jg  (const Label& label, uint32_t hint = kCondHintNone) { _emitJcc(kX86InstJG  , &label, hint); }
  //! @brief Jump to label @a label if condition is met.
  inline void jge (const Label& label, uint32_t hint = kCondHintNone) { _emitJcc(kX86InstJGE , &label, hint); }
  //! @brief Jump to label @a label if condition is met.
  inline void jl  (const Label& label, uint32_t hint = kCondHintNone) { _emitJcc(kX86InstJL  , &label, hint); }
  //! @brief Jump to label @a label if condition is met.
  inline void jle (const Label& label, uint32_t hint = kCondHintNone) { _emitJcc(kX86InstJLE , &label, hint); }
  //! @brief Jump to label @a label if condition is met.
  inline void jna (const Label& label, uint32_t hint = kCondHintNone) { _emitJcc(kX86InstJNA , &label, hint); }
  //! @brief Jump to label @a label if condition is met.
  inline void jnae(const Label& label, uint32_t hint = kCondHintNone) { _emitJcc(kX86InstJNAE, &label, hint); }
  //! @brief Jump to label @a label if condition is met.
  inline void jnb (const Label& label, uint32_t hint = kCondHintNone) { _emitJcc(kX86InstJNB , &label, hint); }
  //! @brief Jump to label @a label if condition is met.
  inline void jnbe(const Label& label, uint32_t hint = kCondHintNone) { _emitJcc(kX86InstJNBE, &label, hint); }
  //! @brief Jump to label @a label if condition is met.
  inline void jnc (const Label& label, uint32_t hint = kCondHintNone) { _emitJcc(kX86InstJNC , &label, hint); }
  //! @brief Jump to label @a label if condition is met.
  inline void jne (const Label& label, uint32_t hint = kCondHintNone) { _emitJcc(kX86InstJNE , &label, hint); }
  //! @brief Jump to label @a label if condition is met.
  inline void jng (const Label& label, uint32_t hint = kCondHintNone) { _emitJcc(kX86InstJNG , &label, hint); }
  //! @brief Jump to label @a label if condition is met.
  inline void jnge(const Label& label, uint32_t hint = kCondHintNone) { _emitJcc(kX86InstJNGE, &label, hint); }
  //! @brief Jump to label @a label if condition is met.
  inline void jnl (const Label& label, uint32_t hint = kCondHintNone) { _emitJcc(kX86InstJNL , &label, hint); }
  //! @brief Jump to label @a label if condition is met.
  inline void jnle(const Label& label, uint32_t hint = kCondHintNone) { _emitJcc(kX86InstJNLE, &label, hint); }
  //! @brief Jump to label @a label if condition is met.
  inline void jno (const Label& label, uint32_t hint = kCondHintNone) { _emitJcc(kX86InstJNO , &label, hint); }
  //! @brief Jump to label @a label if condition is met.
  inline void jnp (const Label& label, uint32_t hint = kCondHintNone) { _emitJcc(kX86InstJNP , &label, hint); }
  //! @brief Jump to label @a label if condition is met.
  inline void jns (const Label& label, uint32_t hint = kCondHintNone) { _emitJcc(kX86InstJNS , &label, hint); }
  //! @brief Jump to label @a label if condition is met.
  inline void jnz (const Label& label, uint32_t hint = kCondHintNone) { _emitJcc(kX86InstJNZ , &label, hint); }
  //! @brief Jump to label @a label if condition is met.
  inline void jo  (const Label& label, uint32_t hint = kCondHintNone) { _emitJcc(kX86InstJO  , &label, hint); }
  //! @brief Jump to label @a label if condition is met.
  inline void jp  (const Label& label, uint32_t hint = kCondHintNone) { _emitJcc(kX86InstJP  , &label, hint); }
  //! @brief Jump to label @a label if condition is met.
  inline void jpe (const Label& label, uint32_t hint = kCondHintNone) { _emitJcc(kX86InstJPE , &label, hint); }
  //! @brief Jump to label @a label if condition is met.
  inline void jpo (const Label& label, uint32_t hint = kCondHintNone) { _emitJcc(kX86InstJPO , &label, hint); }
  //! @brief Jump to label @a label if condition is met.
  inline void js  (const Label& label, uint32_t hint = kCondHintNone) { _emitJcc(kX86InstJS  , &label, hint); }
  //! @brief Jump to label @a label if condition is met.
  inline void jz  (const Label& label, uint32_t hint = kCondHintNone) { _emitJcc(kX86InstJZ  , &label, hint); }

  //! @brief Jump.
  //! @overload
  inline void jmp(const GpVar& dst)
  { _emitInstruction(kX86InstJmp, &dst); }

  //! @brief Jump.
  //! @overload
  inline void jmp(const Mem& dst)
  { _emitInstruction(kX86InstJmp, &dst); }

  //! @brief Jump.
  //! @overload
  inline void jmp(const Imm& dst)
  { _emitInstruction(kX86InstJmp, &dst); }

  //! @brief Jump.
  //! @overload
  inline void jmp(void* dst)
  {
    Imm imm((sysint_t)dst);
    _emitInstruction(kX86InstJmp, &imm);
  }

  //! @brief Jump.
  //!
  //! This instruction transfers program control to a different point
  //! in the instruction stream without recording return information.
  //! The destination (target) operand specifies the label of the
  //! instruction being jumped to.
  inline void jmp(const Label& label)
  { _emitInstruction(kX86InstJmp, &label); }

  //! @brief Load Effective Address
  //!
  //! This instruction computes the effective address of the second
  //! operand (the source operand) and stores it in the first operand
  //! (destination operand). The source operand is a memory address
  //! (offset part) specified with one of the processors addressing modes.
  //! The destination operand is a general-purpose register.
  inline void lea(const GpVar& dst, const Mem& src)
  { _emitInstruction(kX86InstLea, &dst, &src); }

#if ASMJIT_NOT_SUPPORTED_BY_COMPILER
  //! @brief High Level Procedure Exit.
  inline void leave()
  { _emitInstruction(kX86InstLeave); }
#endif // ASMJIT_NOT_SUPPORTED_BY_COMPILER

  //! @brief Move.
  //!
  //! This instruction copies the second operand (source operand) to the first
  //! operand (destination operand). The source operand can be an immediate
  //! value, general-purpose register, segment register, or memory location.
  //! The destination register can be a general-purpose register, segment
  //! register, or memory location. Both operands must be the same size, which
  //! can be a byte, a word, or a DWORD.
  //!
  //! @note To move MMX or SSE registers to/from GP registers or memory, use
  //! corresponding functions: @c movd(), @c movq(), etc. Passing MMX or SSE
  //! registers to @c mov() is illegal.
  inline void mov(const GpVar& dst, const GpVar& src)
  { _emitInstruction(kX86InstMov, &dst, &src); }

  //! @brief Move.
  //! @overload
  inline void mov(const GpVar& dst, const Mem& src)
  { _emitInstruction(kX86InstMov, &dst, &src); }

  //! @brief Move.
  //! @overload
  inline void mov(const GpVar& dst, const Imm& src)
  { _emitInstruction(kX86InstMov, &dst, &src); }

  //! @brief Move.
  //! @overload
  inline void mov(const Mem& dst, const GpVar& src)
  { _emitInstruction(kX86InstMov, &dst, &src); }

  //! @brief Move.
  //! @overload
  inline void mov(const Mem& dst, const Imm& src)
  { _emitInstruction(kX86InstMov, &dst, &src); }

  //! @brief Move from segment register.
  //! @overload.
  inline void mov(const GpVar& dst, const SegmentReg& src)
  { _emitInstruction(kX86InstMov, &dst, &src); }
  
  //! @brief Move from segment register.
  //! @overload.
  inline void mov(const Mem& dst, const SegmentReg& src)
  { _emitInstruction(kX86InstMov, &dst, &src); }

  //! @brief Move to segment register.
  //! @overload.
  inline void mov(const SegmentReg& dst, const GpVar& src)
  { _emitInstruction(kX86InstMov, &dst, &src); }

  //! @brief Move to segment register.
  //! @overload.
  inline void mov(const SegmentReg& dst, const Mem& src)
  { _emitInstruction(kX86InstMov, &dst, &src); }

  //! @brief Move byte, word, dword or qword from absolute address @a src to
  //! AL, AX, EAX or RAX register.
  inline void mov_ptr(const GpVar& dst, void* src)
  {
    Imm imm((sysint_t)src);
    _emitInstruction(kX86InstMovPtr, &dst, &imm);
  }

  //! @brief Move byte, word, dword or qword from AL, AX, EAX or RAX register
  //! to absolute address @a dst.
  inline void mov_ptr(void* dst, const GpVar& src)
  {
    Imm imm((sysint_t)dst);
    _emitInstruction(kX86InstMovPtr, &imm, &src);
  }

  //! @brief Move with Sign-Extension.
  //!
  //! This instruction copies the contents of the source operand (register
  //! or memory location) to the destination operand (register) and sign
  //! extends the value to 16, 32 or 64-bits.
  //!
  //! @sa movsxd().
  void movsx(const GpVar& dst, const GpVar& src)
  { _emitInstruction(kX86InstMovSX, &dst, &src); }

  //! @brief Move with Sign-Extension.
  //! @overload
  void movsx(const GpVar& dst, const Mem& src)
  { _emitInstruction(kX86InstMovSX, &dst, &src); }

#if defined(ASMJIT_X64)
  //! @brief Move DWord to QWord with sign-extension.
  inline void movsxd(const GpVar& dst, const GpVar& src)
  { _emitInstruction(kX86InstMovSXD, &dst, &src); }

  //! @brief Move DWord to QWord with sign-extension.
  //! @overload
  inline void movsxd(const GpVar& dst, const Mem& src)
  { _emitInstruction(kX86InstMovSXD, &dst, &src); }
#endif // ASMJIT_X64

  //! @brief Move with Zero-Extend.
  //!
  //! This instruction copies the contents of the source operand (register
  //! or memory location) to the destination operand (register) and zero
  //! extends the value to 16 or 32-bits. The size of the converted value
  //! depends on the operand-size attribute.
  inline void movzx(const GpVar& dst, const GpVar& src)
  { _emitInstruction(kX86InstMovZX, &dst, &src); }

  //! @brief Move with Zero-Extend.
  //! @brief Overload
  inline void movzx(const GpVar& dst, const Mem& src)
  { _emitInstruction(kX86InstMovZX, &dst, &src); }

  //! @brief Unsigned multiply.
  //!
  //! Source operand (in a general-purpose register or memory location)
  //! is multiplied by the value in the AL, AX, or EAX register (depending
  //! on the operand size) and the product is stored in the AX, DX:AX, or
  //! EDX:EAX registers, respectively.
  inline void mul(const GpVar& dst_hi, const GpVar& dst_lo, const GpVar& src)
  {
    // Destination variables must be different.
    ASMJIT_ASSERT(dst_hi.getId() != dst_lo.getId());
    _emitInstruction(kX86InstMul, &dst_hi, &dst_lo, &src);
  }

  //! @brief Unsigned multiply.
  //! @overload
  inline void mul(const GpVar& dst_hi, const GpVar& dst_lo, const Mem& src)
  {
    // Destination variables must be different.
    ASMJIT_ASSERT(dst_hi.getId() != dst_lo.getId());
    _emitInstruction(kX86InstMul, &dst_hi, &dst_lo, &src);
  }

  //! @brief Two's Complement Negation.
  inline void neg(const GpVar& dst)
  { _emitInstruction(kX86InstNeg, &dst); }

  //! @brief Two's Complement Negation.
  inline void neg(const Mem& dst)
  { _emitInstruction(kX86InstNeg, &dst); }

  //! @brief No Operation.
  //!
  //! This instruction performs no operation. This instruction is a one-byte
  //! instruction that takes up space in the instruction stream but does not
  //! affect the machine context, except the EIP register. The NOP instruction
  //! is an alias mnemonic for the XCHG (E)AX, (E)AX instruction.
  inline void nop()
  { _emitInstruction(kX86InstNop); }

  //! @brief One's Complement Negation.
  inline void not_(const GpVar& dst)
  { _emitInstruction(kX86InstNot, &dst); }

  //! @brief One's Complement Negation.
  inline void not_(const Mem& dst)
  { _emitInstruction(kX86InstNot, &dst); }

  //! @brief Logical Inclusive OR.
  inline void or_(const GpVar& dst, const GpVar& src)
  { _emitInstruction(kX86InstOr, &dst, &src); }

  //! @brief Logical Inclusive OR.
  inline void or_(const GpVar& dst, const Mem& src)
  { _emitInstruction(kX86InstOr, &dst, &src); }

  //! @brief Logical Inclusive OR.
  inline void or_(const GpVar& dst, const Imm& src)
  { _emitInstruction(kX86InstOr, &dst, &src); }

  //! @brief Logical Inclusive OR.
  inline void or_(const Mem& dst, const GpVar& src)
  { _emitInstruction(kX86InstOr, &dst, &src); }

  //! @brief Logical Inclusive OR.
  inline void or_(const Mem& dst, const Imm& src)
  { _emitInstruction(kX86InstOr, &dst, &src); }

  //! @brief Pop a Value from the Stack.
  //!
  //! This instruction loads the value from the top of the stack to the location
  //! specified with the destination operand and then increments the stack pointer.
  //! The destination operand can be a general purpose register, memory location,
  //! or segment register.
  inline void pop(const GpVar& dst)
  { _emitInstruction(kX86InstPop, &dst); }

  inline void pop(const Mem& dst)
  {
    ASMJIT_ASSERT(dst.getSize() == 2 || dst.getSize() == sizeof(sysint_t));
    _emitInstruction(kX86InstPop, &dst);
  }

#if defined(ASMJIT_X86)
  //! @brief Pop All General-Purpose Registers.
  //!
  //! Pop EDI, ESI, EBP, EBX, EDX, ECX, and EAX.
  inline void popad()
  { _emitInstruction(kX86InstPopAD); }
#endif // ASMJIT_X86

  //! @brief Pop Stack into EFLAGS Register (32-bit or 64-bit).
  inline void popf()
  {
#if defined(ASMJIT_X86)
    popfd();
#else
    popfq();
#endif
  }

#if defined(ASMJIT_X86)
  //! @brief Pop Stack into EFLAGS Register (32-bit).
  inline void popfd()
  { _emitInstruction(kX86InstPopFD); }
#else
  //! @brief Pop Stack into EFLAGS Register (64-bit).
  inline void popfq()
  { _emitInstruction(kX86InstPopFQ); }
#endif

  //! @brief Push WORD/DWORD/QWORD Onto the Stack.
  //!
  //! @note 32-bit architecture pushed DWORD while 64-bit
  //! pushes QWORD. 64-bit mode not provides instruction to
  //! push 32-bit register/memory.
  inline void push(const GpVar& src)
  { _emitInstruction(kX86InstPush, &src); }

  //! @brief Push WORD/DWORD/QWORD Onto the Stack.
  inline void push(const Mem& src)
  {
    ASMJIT_ASSERT(src.getSize() == 2 || src.getSize() == sizeof(sysint_t));
    _emitInstruction(kX86InstPush, &src);
  }

  //! @brief Push WORD/DWORD/QWORD Onto the Stack.
  inline void push(const Imm& src)
  { _emitInstruction(kX86InstPush, &src); }

#if defined(ASMJIT_X86)
  //! @brief Push All General-Purpose Registers.
  //!
  //! Push EAX, ECX, EDX, EBX, original ESP, EBP, ESI, and EDI.
  inline void pushad()
  { _emitInstruction(kX86InstPushAD); }
#endif // ASMJIT_X86

  //! @brief Push EFLAGS Register (32-bit or 64-bit) onto the Stack.
  inline void pushf()
  {
#if defined(ASMJIT_X86)
    pushfd();
#else
    pushfq();
#endif
  }

#if defined(ASMJIT_X86)
  //! @brief Push EFLAGS Register (32-bit) onto the Stack.
  inline void pushfd()
  { _emitInstruction(kX86InstPushFD); }
#else
  //! @brief Push EFLAGS Register (64-bit) onto the Stack.
  inline void pushfq()
  { _emitInstruction(kX86InstPushFQ); }
#endif // ASMJIT_X86

  //! @brief Rotate Bits Left.
  //! @note @a src register can be only @c cl.
  inline void rcl(const GpVar& dst, const GpVar& src)
  { _emitInstruction(kX86InstRcl, &dst, &src); }

  //! @brief Rotate Bits Left.
  inline void rcl(const GpVar& dst, const Imm& src)
  { _emitInstruction(kX86InstRcl, &dst, &src); }

  //! @brief Rotate Bits Left.
  //! @note @a src register can be only @c cl.
  inline void rcl(const Mem& dst, const GpVar& src)
  { _emitInstruction(kX86InstRcl, &dst, &src); }

  //! @brief Rotate Bits Left.
  inline void rcl(const Mem& dst, const Imm& src)
  { _emitInstruction(kX86InstRcl, &dst, &src); }

  //! @brief Rotate Bits Right.
  //! @note @a src register can be only @c cl.
  inline void rcr(const GpVar& dst, const GpVar& src)
  { _emitInstruction(kX86InstRcr, &dst, &src); }

  //! @brief Rotate Bits Right.
  inline void rcr(const GpVar& dst, const Imm& src)
  { _emitInstruction(kX86InstRcr, &dst, &src); }

  //! @brief Rotate Bits Right.
  //! @note @a src register can be only @c cl.
  inline void rcr(const Mem& dst, const GpVar& src)
  { _emitInstruction(kX86InstRcr, &dst, &src); }

  //! @brief Rotate Bits Right.
  inline void rcr(const Mem& dst, const Imm& src)
  { _emitInstruction(kX86InstRcr, &dst, &src); }

  //! @brief Read Time-Stamp Counter (Pentium).
  inline void rdtsc(const GpVar& dst_edx, const GpVar& dst_eax)
  {
    // Destination registers must be different.
    ASMJIT_ASSERT(dst_edx.getId() != dst_eax.getId());
    _emitInstruction(kX86InstRdtsc, &dst_edx, &dst_eax);
  }

  //! @brief Read Time-Stamp Counter and Processor ID (New).
  inline void rdtscp(const GpVar& dst_edx, const GpVar& dst_eax, const GpVar& dst_ecx)
  {
    // Destination registers must be different.
    ASMJIT_ASSERT(dst_edx.getId() != dst_eax.getId() && dst_eax.getId() != dst_ecx.getId());
    _emitInstruction(kX86InstRdtscP, &dst_edx, &dst_eax, &dst_ecx);
  }

  //! @brief Load ECX/RCX BYTEs from DS:[ESI/RSI] to AL.
  inline void rep_lodsb(const GpVar& dst_val, const GpVar& src_addr, const GpVar& cnt_ecx)
  {
    // All registers must be unique, they will be reallocated to dst=EAX,RAX, src=DS:ESI/RSI, cnt=ECX/RCX.
    ASMJIT_ASSERT(dst_val.getId() != src_addr.getId() && src_addr.getId() != cnt_ecx.getId());
    _emitInstruction(kX86InstRepLodSB, &dst_val, &src_addr, &cnt_ecx);
  }

  //! @brief Load ECX/RCX DWORDs from DS:[ESI/RSI] to EAX.
  inline void rep_lodsd(const GpVar& dst_val, const GpVar& src_addr, const GpVar& cnt_ecx)
  {
    // All registers must be unique, they will be reallocated to dst=EAX,RAX, src=DS:ESI/RSI, cnt=ECX/RCX.
    ASMJIT_ASSERT(dst_val.getId() != src_addr.getId() && src_addr.getId() != cnt_ecx.getId());
    _emitInstruction(kX86InstRepLodSD, &dst_val, &src_addr, &cnt_ecx);
  }

#if defined(ASMJIT_X64)
  //! @brief Load ECX/RCX QWORDs from DS:[ESI/RSI] to RAX.
  inline void rep_lodsq(const GpVar& dst_val, const GpVar& src_addr, const GpVar& cnt_ecx)
  {
    // All registers must be unique, they will be reallocated to dst=EAX,RAX, src=DS:ESI/RSI, cnt=ECX/RCX.
    ASMJIT_ASSERT(dst_val.getId() != src_addr.getId() && src_addr.getId() != cnt_ecx.getId());
    _emitInstruction(kX86InstRepLodSQ, &dst_val, &src_addr, &cnt_ecx);
  }
#endif // ASMJIT_X64

  //! @brief Load ECX/RCX WORDs from DS:[ESI/RSI] to AX.
  inline void rep_lodsw(const GpVar& dst_val, const GpVar& src_addr, const GpVar& cnt_ecx)
  {
    // All registers must be unique, they will be reallocated to dst=EAX,RAX, src=DS:ESI/RSI, cnt=ECX/RCX.
    ASMJIT_ASSERT(dst_val.getId() != src_addr.getId() && src_addr.getId() != cnt_ecx.getId());
    _emitInstruction(kX86InstRepLodSW, &dst_val, &src_addr, &cnt_ecx);
  }

  //! @brief Move ECX/RCX BYTEs from DS:[ESI/RSI] to ES:[EDI/RDI].
  inline void rep_movsb(const GpVar& dst_addr, const GpVar& src_addr, const GpVar& cnt_ecx)
  {
    // All registers must be unique, they will be reallocated to dst=ES:EDI,RDI, src=DS:ESI/RSI, cnt=ECX/RCX.
    ASMJIT_ASSERT(dst_addr.getId() != src_addr.getId() && src_addr.getId() != cnt_ecx.getId());
    _emitInstruction(kX86InstRepMovSB, &dst_addr, &src_addr, &cnt_ecx);
  }

  //! @brief Move ECX/RCX DWORDs from DS:[ESI/RSI] to ES:[EDI/RDI].
  inline void rep_movsd(const GpVar& dst_addr, const GpVar& src_addr, const GpVar& cnt_ecx)
  {
    // All registers must be unique, they will be reallocated to dst=ES:EDI,RDI, src=DS:ESI/RSI, cnt=ECX/RCX.
    ASMJIT_ASSERT(dst_addr.getId() != src_addr.getId() && src_addr.getId() != cnt_ecx.getId());
    _emitInstruction(kX86InstRepMovSD, &dst_addr, &src_addr, &cnt_ecx);
  }

#if defined(ASMJIT_X64)
  //! @brief Move ECX/RCX QWORDs from DS:[ESI/RSI] to ES:[EDI/RDI].
  inline void rep_movsq(const GpVar& dst_addr, const GpVar& src_addr, const GpVar& cnt_ecx)
  {
    // All registers must be unique, they will be reallocated to dst=ES:EDI,RDI, src=DS:ESI/RSI, cnt=ECX/RCX.
    ASMJIT_ASSERT(dst_addr.getId() != src_addr.getId() && src_addr.getId() != cnt_ecx.getId());
    _emitInstruction(kX86InstRepMovSQ, &dst_addr, &src_addr, &cnt_ecx);
  }
#endif // ASMJIT_X64

  //! @brief Move ECX/RCX WORDs from DS:[ESI/RSI] to ES:[EDI/RDI].
  inline void rep_movsw(const GpVar& dst_addr, const GpVar& src_addr, const GpVar& cnt_ecx)
  {
    // All registers must be unique, they will be reallocated to dst=ES:EDI,RDI, src=DS:ESI/RSI, cnt=ECX/RCX.
    ASMJIT_ASSERT(dst_addr.getId() != src_addr.getId() && src_addr.getId() != cnt_ecx.getId());
    _emitInstruction(kX86InstRepMovSW, &dst_addr, &src_addr, &cnt_ecx);
  }

  //! @brief Fill ECX/RCX BYTEs at ES:[EDI/RDI] with AL.
  inline void rep_stosb(const GpVar& dst_addr, const GpVar& src_val, const GpVar& cnt_ecx)
  {
    // All registers must be unique, they will be reallocated to dst=ES:EDI,RDI, src=EAX/RAX, cnt=ECX/RCX.
    ASMJIT_ASSERT(dst_addr.getId() != src_val.getId() && src_val.getId() != cnt_ecx.getId());
    _emitInstruction(kX86InstRepStoSB, &dst_addr, &src_val, &cnt_ecx);
  }

  //! @brief Fill ECX/RCX DWORDs at ES:[EDI/RDI] with EAX.
  inline void rep_stosd(const GpVar& dst_addr, const GpVar& src_val, const GpVar& cnt_ecx)
  {
    // All registers must be unique, they will be reallocated to dst=ES:EDI,RDI, src=EAX/RAX, cnt=ECX/RCX.
    ASMJIT_ASSERT(dst_addr.getId() != src_val.getId() && src_val.getId() != cnt_ecx.getId());
    _emitInstruction(kX86InstRepStoSD, &dst_addr, &src_val, &cnt_ecx);
  }

#if defined(ASMJIT_X64)
  //! @brief Fill ECX/RCX QWORDs at ES:[EDI/RDI] with RAX.
  inline void rep_stosq(const GpVar& dst_addr, const GpVar& src_val, const GpVar& cnt_ecx)
  {
    // All registers must be unique, they will be reallocated to dst=ES:EDI,RDI, src=EAX/RAX, cnt=ECX/RCX.
    ASMJIT_ASSERT(dst_addr.getId() != src_val.getId() && src_val.getId() != cnt_ecx.getId());
    _emitInstruction(kX86InstRepStoSQ, &dst_addr, &src_val, &cnt_ecx);
  }
#endif // ASMJIT_X64

  //! @brief Fill ECX/RCX WORDs at ES:[EDI/RDI] with AX.
  inline void rep_stosw(const GpVar& dst_addr, const GpVar& src_val, const GpVar& cnt_ecx)
  {
    // All registers must be unique, they will be reallocated to dst=ES:EDI,RDI, src=EAX/RAX, cnt=ECX/RCX.
    ASMJIT_ASSERT(dst_addr.getId() != src_val.getId() && src_val.getId() != cnt_ecx.getId());
    _emitInstruction(kX86InstRepStoSW, &dst_addr, &src_val, &cnt_ecx);
  }

  //! @brief Repeated find nonmatching BYTEs in ES:[EDI/RDI] and DS:[ESI/RDI].
  inline void repe_cmpsb(const GpVar& cmp1_addr, const GpVar& cmp2_addr, const GpVar& cnt_ecx)
  {
    // All registers must be unique, they will be reallocated to cmp1=ES:EDI,RDI, cmp2=ES:[EDI/RDI], cnt=ECX/RCX.
    ASMJIT_ASSERT(cmp1_addr.getId() != cmp2_addr.getId() && cmp2_addr.getId() != cnt_ecx.getId());
    _emitInstruction(kX86InstRepECmpSB, &cmp1_addr, &cmp2_addr, &cnt_ecx);
  }

  //! @brief Repeated find nonmatching DWORDs in ES:[EDI/RDI] and DS:[ESI/RDI].
  inline void repe_cmpsd(const GpVar& cmp1_addr, const GpVar& cmp2_addr, const GpVar& cnt_ecx)
  {
    // All registers must be unique, they will be reallocated to cmp1=ES:EDI,RDI, cmp2=ES:[EDI/RDI], cnt=ECX/RCX.
    ASMJIT_ASSERT(cmp1_addr.getId() != cmp2_addr.getId() && cmp2_addr.getId() != cnt_ecx.getId());
    _emitInstruction(kX86InstRepECmpSD, &cmp1_addr, &cmp2_addr, &cnt_ecx);
  }

#if defined(ASMJIT_X64)
  //! @brief Repeated find nonmatching QWORDs in ES:[EDI/RDI] and DS:[ESI/RDI].
  inline void repe_cmpsq(const GpVar& cmp1_addr, const GpVar& cmp2_addr, const GpVar& cnt_ecx)
  {
    // All registers must be unique, they will be reallocated to cmp1=ES:EDI,RDI, cmp2=ES:[EDI/RDI], cnt=ECX/RCX.
    ASMJIT_ASSERT(cmp1_addr.getId() != cmp2_addr.getId() && cmp2_addr.getId() != cnt_ecx.getId());
    _emitInstruction(kX86InstRepECmpSQ, &cmp1_addr, &cmp2_addr, &cnt_ecx);
  }
#endif // ASMJIT_X64

  //! @brief Repeated find nonmatching WORDs in ES:[EDI/RDI] and DS:[ESI/RDI].
  inline void repe_cmpsw(const GpVar& cmp1_addr, const GpVar& cmp2_addr, const GpVar& cnt_ecx)
  {
    // All registers must be unique, they will be reallocated to cmp1=ES:EDI,RDI, cmp2=ES:[EDI/RDI], cnt=ECX/RCX.
    ASMJIT_ASSERT(cmp1_addr.getId() != cmp2_addr.getId() && cmp2_addr.getId() != cnt_ecx.getId());
    _emitInstruction(kX86InstRepECmpSW, &cmp1_addr, &cmp2_addr, &cnt_ecx);
  }

  //! @brief Find non-AL BYTE starting at ES:[EDI/RDI].
  inline void repe_scasb(const GpVar& cmp1_addr, const GpVar& cmp2_val, const GpVar& cnt_ecx)
  {
    // All registers must be unique, they will be reallocated to cmp1=ES:EDI,RDI, src=AL, cnt=ECX/RCX.
    ASMJIT_ASSERT(cmp1_addr.getId() != cmp2_val.getId() && cmp2_val.getId() != cnt_ecx.getId());
    _emitInstruction(kX86InstRepEScaSB, &cmp1_addr, &cmp2_val, &cnt_ecx);
  }

  //! @brief Find non-EAX DWORD starting at ES:[EDI/RDI].
  inline void repe_scasd(const GpVar& cmp1_addr, const GpVar& cmp2_val, const GpVar& cnt_ecx)
  {
    // All registers must be unique, they will be reallocated to cmp1=ES:EDI,RDI, src=EAX, cnt=ECX/RCX.
    ASMJIT_ASSERT(cmp1_addr.getId() != cmp2_val.getId() && cmp2_val.getId() != cnt_ecx.getId());
    _emitInstruction(kX86InstRepEScaSD, &cmp1_addr, &cmp2_val, &cnt_ecx);
  }

#if defined(ASMJIT_X64)
  //! @brief Find non-RAX QWORD starting at ES:[EDI/RDI].
  inline void repe_scasq(const GpVar& cmp1_addr, const GpVar& cmp2_val, const GpVar& cnt_ecx)
  {
    // All registers must be unique, they will be reallocated to cmp1=ES:EDI,RDI, src=RAX, cnt=ECX/RCX.
    ASMJIT_ASSERT(cmp1_addr.getId() != cmp2_val.getId() && cmp2_val.getId() != cnt_ecx.getId());
    _emitInstruction(kX86InstRepEScaSQ, &cmp1_addr, &cmp2_val, &cnt_ecx);
  }
#endif // ASMJIT_X64

  //! @brief Find non-AX WORD starting at ES:[EDI/RDI].
  inline void repe_scasw(const GpVar& cmp1_addr, const GpVar& cmp2_val, const GpVar& cnt_ecx)
  {
    // All registers must be unique, they will be reallocated to cmp1=ES:EDI,RDI, src=AX, cnt=ECX/RCX.
    ASMJIT_ASSERT(cmp1_addr.getId() != cmp2_val.getId() && cmp2_val.getId() != cnt_ecx.getId());
    _emitInstruction(kX86InstRepEScaSW, &cmp1_addr, &cmp2_val, &cnt_ecx);
  }

  //! @brief Find matching BYTEs in [RDI] and [RSI].
  inline void repne_cmpsb(const GpVar& cmp1_addr, const GpVar& cmp2_addr, const GpVar& cnt_ecx)
  {
    // All registers must be unique, they will be reallocated to cmp1=ES:EDI,RDI, cmp2=ES:[EDI/RDI], cnt=ECX/RCX.
    ASMJIT_ASSERT(cmp1_addr.getId() != cmp2_addr.getId() && cmp2_addr.getId() != cnt_ecx.getId());
    _emitInstruction(kX86InstRepNECmpSB, &cmp1_addr, &cmp2_addr, &cnt_ecx);
  }

  //! @brief Find matching DWORDs in [RDI] and [RSI].
  inline void repne_cmpsd(const GpVar& cmp1_addr, const GpVar& cmp2_addr, const GpVar& cnt_ecx)
  {
    // All registers must be unique, they will be reallocated to cmp1=ES:EDI,RDI, cmp2=ES:[EDI/RDI], cnt=ECX/RCX.
    ASMJIT_ASSERT(cmp1_addr.getId() != cmp2_addr.getId() && cmp2_addr.getId() != cnt_ecx.getId());
    _emitInstruction(kX86InstRepNECmpSD, &cmp1_addr, &cmp2_addr, &cnt_ecx);
  }

#if defined(ASMJIT_X64)
  //! @brief Find matching QWORDs in [RDI] and [RSI].
  inline void repne_cmpsq(const GpVar& cmp1_addr, const GpVar& cmp2_addr, const GpVar& cnt_ecx)
  {
    // All registers must be unique, they will be reallocated to cmp1=ES:EDI,RDI, cmp2=ES:[EDI/RDI], cnt=ECX/RCX.
    ASMJIT_ASSERT(cmp1_addr.getId() != cmp2_addr.getId() && cmp2_addr.getId() != cnt_ecx.getId());
    _emitInstruction(kX86InstRepNECmpSQ, &cmp1_addr, &cmp2_addr, &cnt_ecx);
  }
#endif // ASMJIT_X64

  //! @brief Find matching WORDs in [RDI] and [RSI].
  inline void repne_cmpsw(const GpVar& cmp1_addr, const GpVar& cmp2_addr, const GpVar& cnt_ecx)
  {
    // All registers must be unique, they will be reallocated to cmp1=ES:EDI,RDI, cmp2=ES:[EDI/RDI], cnt=ECX/RCX.
    ASMJIT_ASSERT(cmp1_addr.getId() != cmp2_addr.getId() && cmp2_addr.getId() != cnt_ecx.getId());
    _emitInstruction(kX86InstRepNECmpSW, &cmp1_addr, &cmp2_addr, &cnt_ecx);
  }

  //! @brief Find AL, starting at ES:[EDI/RDI].
  inline void repne_scasb(const GpVar& cmp1_addr, const GpVar& cmp2_val, const GpVar& cnt_ecx)
  {
    // All registers must be unique, they will be reallocated to cmp1=ES:EDI,RDI, src=AL, cnt=ECX/RCX.
    ASMJIT_ASSERT(cmp1_addr.getId() != cmp2_val.getId() && cmp2_val.getId() != cnt_ecx.getId());
    _emitInstruction(kX86InstRepNEScaSB, &cmp1_addr, &cmp2_val, &cnt_ecx);
  }

  //! @brief Find EAX, starting at ES:[EDI/RDI].
  inline void repne_scasd(const GpVar& cmp1_addr, const GpVar& cmp2_val, const GpVar& cnt_ecx)
  {
    // All registers must be unique, they will be reallocated to cmp1=ES:EDI,RDI, src=EAX, cnt=ECX/RCX.
    ASMJIT_ASSERT(cmp1_addr.getId() != cmp2_val.getId() && cmp2_val.getId() != cnt_ecx.getId());
    _emitInstruction(kX86InstRepNEScaSD, &cmp1_addr, &cmp2_val, &cnt_ecx);
  }

#if defined(ASMJIT_X64)
  //! @brief Find RAX, starting at ES:[EDI/RDI].
  inline void repne_scasq(const GpVar& cmp1_addr, const GpVar& cmp2_val, const GpVar& cnt_ecx)
  {
    // All registers must be unique, they will be reallocated to cmp1=ES:EDI,RDI, src=RAX, cnt=ECX/RCX.
    ASMJIT_ASSERT(cmp1_addr.getId() != cmp2_val.getId() && cmp2_val.getId() != cnt_ecx.getId());
    _emitInstruction(kX86InstRepNEScaSQ, &cmp1_addr, &cmp2_val, &cnt_ecx);
  }
#endif // ASMJIT_X64

  //! @brief Find AX, starting at ES:[EDI/RDI].
  inline void repne_scasw(const GpVar& cmp1_addr, const GpVar& cmp2_val, const GpVar& cnt_ecx)
  {
    // All registers must be unique, they will be reallocated to cmp1=ES:EDI,RDI, src=AX, cnt=ECX/RCX.
    ASMJIT_ASSERT(cmp1_addr.getId() != cmp2_val.getId() && cmp2_val.getId() != cnt_ecx.getId());
    _emitInstruction(kX86InstRepNEScaSW, &cmp1_addr, &cmp2_val, &cnt_ecx);
  }

  //! @brief Return from Procedure.
  inline void ret()
  { _emitReturn(NULL, NULL); }

  //! @brief Return from Procedure.
  inline void ret(const GpVar& first)
  { _emitReturn(&first, NULL); }

  //! @brief Return from Procedure.
  inline void ret(const GpVar& first, const GpVar& second)
  { _emitReturn(&first, &second); }

  //! @brief Return from Procedure.
  inline void ret(const XmmVar& first)
  { _emitReturn(&first, NULL); }

  //! @brief Return from Procedure.
  inline void ret(const XmmVar& first, const XmmVar& second)
  { _emitReturn(&first, &second); }

  //! @brief Rotate Bits Left.
  //! @note @a src register can be only @c cl.
  inline void rol(const GpVar& dst, const GpVar& src)
  { _emitInstruction(kX86InstRol, &dst, &src); }

  //! @brief Rotate Bits Left.
  inline void rol(const GpVar& dst, const Imm& src)
  { _emitInstruction(kX86InstRol, &dst, &src); }

  //! @brief Rotate Bits Left.
  //! @note @a src register can be only @c cl.
  inline void rol(const Mem& dst, const GpVar& src)
  { _emitInstruction(kX86InstRol, &dst, &src); }

  //! @brief Rotate Bits Left.
  inline void rol(const Mem& dst, const Imm& src)
  { _emitInstruction(kX86InstRol, &dst, &src); }

  //! @brief Rotate Bits Right.
  //! @note @a src register can be only @c cl.
  inline void ror(const GpVar& dst, const GpVar& src)
  { _emitInstruction(kX86InstRor, &dst, &src); }

  //! @brief Rotate Bits Right.
  inline void ror(const GpVar& dst, const Imm& src)
  { _emitInstruction(kX86InstRor, &dst, &src); }

  //! @brief Rotate Bits Right.
  //! @note @a src register can be only @c cl.
  inline void ror(const Mem& dst, const GpVar& src)
  { _emitInstruction(kX86InstRor, &dst, &src); }

  //! @brief Rotate Bits Right.
  inline void ror(const Mem& dst, const Imm& src)
  { _emitInstruction(kX86InstRor, &dst, &src); }

#if defined(ASMJIT_X86)
  //! @brief Store @a var (allocated to AH/AX/EAX/RAX) into Flags.
  inline void sahf(const GpVar& var)
  { _emitInstruction(kX86InstSahf, &var); }
#endif // ASMJIT_X86

  //! @brief Integer subtraction with borrow.
  inline void sbb(const GpVar& dst, const GpVar& src)
  { _emitInstruction(kX86InstSbb, &dst, &src); }

  //! @brief Integer subtraction with borrow.
  inline void sbb(const GpVar& dst, const Mem& src)
  { _emitInstruction(kX86InstSbb, &dst, &src); }

  //! @brief Integer subtraction with borrow.
  inline void sbb(const GpVar& dst, const Imm& src)
  { _emitInstruction(kX86InstSbb, &dst, &src); }

  //! @brief Integer subtraction with borrow.
  inline void sbb(const Mem& dst, const GpVar& src)
  { _emitInstruction(kX86InstSbb, &dst, &src); }

  //! @brief Integer subtraction with borrow.
  inline void sbb(const Mem& dst, const Imm& src)
  { _emitInstruction(kX86InstSbb, &dst, &src); }

  //! @brief Shift Bits Left.
  //! @note @a src register can be only @c cl.
  inline void sal(const GpVar& dst, const GpVar& src)
  { _emitInstruction(kX86InstSal, &dst, &src); }

  //! @brief Shift Bits Left.
  inline void sal(const GpVar& dst, const Imm& src)
  { _emitInstruction(kX86InstSal, &dst, &src); }

  //! @brief Shift Bits Left.
  //! @note @a src register can be only @c cl.
  inline void sal(const Mem& dst, const GpVar& src)
  { _emitInstruction(kX86InstSal, &dst, &src); }

  //! @brief Shift Bits Left.
  inline void sal(const Mem& dst, const Imm& src)
  { _emitInstruction(kX86InstSal, &dst, &src); }

  //! @brief Shift Bits Right.
  //! @note @a src register can be only @c cl.
  inline void sar(const GpVar& dst, const GpVar& src)
  { _emitInstruction(kX86InstSar, &dst, &src); }

  //! @brief Shift Bits Right.
  inline void sar(const GpVar& dst, const Imm& src)
  { _emitInstruction(kX86InstSar, &dst, &src); }

  //! @brief Shift Bits Right.
  //! @note @a src register can be only @c cl.
  inline void sar(const Mem& dst, const GpVar& src)
  { _emitInstruction(kX86InstSar, &dst, &src); }

  //! @brief Shift Bits Right.
  inline void sar(const Mem& dst, const Imm& src)
  { _emitInstruction(kX86InstSar, &dst, &src); }

  //! @brief Set Byte on Condition.
  inline void set(kX86Cond cc, const GpVar& dst)
  {
    ASMJIT_ASSERT(dst.getSize() == 1);
    _emitInstruction(X86Util::getSetccInstFromCond(cc), &dst);
  }

  //! @brief Set Byte on Condition.
  inline void set(kX86Cond cc, const Mem& dst)
  {
    ASMJIT_ASSERT(dst.getSize() <= 1);
    _emitInstruction(X86Util::getSetccInstFromCond(cc), &dst);
  }

  //! @brief Set Byte on Condition.
  inline void seta  (const GpVar& dst) { ASMJIT_ASSERT(dst.getSize() == 1); _emitInstruction(kX86InstSetA  , &dst); }
  //! @brief Set Byte on Condition.
  inline void seta  (const Mem& dst)   { ASMJIT_ASSERT(dst.getSize() <= 1); _emitInstruction(kX86InstSetA  , &dst); }
  //! @brief Set Byte on Condition.
  inline void setae (const GpVar& dst) { ASMJIT_ASSERT(dst.getSize() == 1); _emitInstruction(kX86InstSetAE , &dst); }
  //! @brief Set Byte on Condition.
  inline void setae (const Mem& dst)   { ASMJIT_ASSERT(dst.getSize() <= 1); _emitInstruction(kX86InstSetAE , &dst); }
  //! @brief Set Byte on Condition.
  inline void setb  (const GpVar& dst) { ASMJIT_ASSERT(dst.getSize() == 1); _emitInstruction(kX86InstSetB  , &dst); }
  //! @brief Set Byte on Condition.
  inline void setb  (const Mem& dst)   { ASMJIT_ASSERT(dst.getSize() <= 1); _emitInstruction(kX86InstSetB  , &dst); }
  //! @brief Set Byte on Condition.
  inline void setbe (const GpVar& dst) { ASMJIT_ASSERT(dst.getSize() == 1); _emitInstruction(kX86InstSetBE , &dst); }
  //! @brief Set Byte on Condition.
  inline void setbe (const Mem& dst)   { ASMJIT_ASSERT(dst.getSize() <= 1); _emitInstruction(kX86InstSetBE , &dst); }
  //! @brief Set Byte on Condition.
  inline void setc  (const GpVar& dst) { ASMJIT_ASSERT(dst.getSize() == 1); _emitInstruction(kX86InstSetC  , &dst); }
  //! @brief Set Byte on Condition.
  inline void setc  (const Mem& dst)   { ASMJIT_ASSERT(dst.getSize() <= 1); _emitInstruction(kX86InstSetC  , &dst); }
  //! @brief Set Byte on Condition.
  inline void sete  (const GpVar& dst) { ASMJIT_ASSERT(dst.getSize() == 1); _emitInstruction(kX86InstSetE  , &dst); }
  //! @brief Set Byte on Condition.
  inline void sete  (const Mem& dst)   { ASMJIT_ASSERT(dst.getSize() <= 1); _emitInstruction(kX86InstSetE  , &dst); }
  //! @brief Set Byte on Condition.
  inline void setg  (const GpVar& dst) { ASMJIT_ASSERT(dst.getSize() == 1); _emitInstruction(kX86InstSetG  , &dst); }
  //! @brief Set Byte on Condition.
  inline void setg  (const Mem& dst)   { ASMJIT_ASSERT(dst.getSize() <= 1); _emitInstruction(kX86InstSetG  , &dst); }
  //! @brief Set Byte on Condition.
  inline void setge (const GpVar& dst) { ASMJIT_ASSERT(dst.getSize() == 1); _emitInstruction(kX86InstSetGE , &dst); }
  //! @brief Set Byte on Condition.
  inline void setge (const Mem& dst)   { ASMJIT_ASSERT(dst.getSize() <= 1); _emitInstruction(kX86InstSetGE , &dst); }
  //! @brief Set Byte on Condition.
  inline void setl  (const GpVar& dst) { ASMJIT_ASSERT(dst.getSize() == 1); _emitInstruction(kX86InstSetL  , &dst); }
  //! @brief Set Byte on Condition.
  inline void setl  (const Mem& dst)   { ASMJIT_ASSERT(dst.getSize() <= 1); _emitInstruction(kX86InstSetL  , &dst); }
  //! @brief Set Byte on Condition.
  inline void setle (const GpVar& dst) { ASMJIT_ASSERT(dst.getSize() == 1); _emitInstruction(kX86InstSetLE , &dst); }
  //! @brief Set Byte on Condition.
  inline void setle (const Mem& dst)   { ASMJIT_ASSERT(dst.getSize() <= 1); _emitInstruction(kX86InstSetLE , &dst); }
  //! @brief Set Byte on Condition.
  inline void setna (const GpVar& dst) { ASMJIT_ASSERT(dst.getSize() == 1); _emitInstruction(kX86InstSetNA , &dst); }
  //! @brief Set Byte on Condition.
  inline void setna (const Mem& dst)   { ASMJIT_ASSERT(dst.getSize() <= 1); _emitInstruction(kX86InstSetNA , &dst); }
  //! @brief Set Byte on Condition.
  inline void setnae(const GpVar& dst) { ASMJIT_ASSERT(dst.getSize() == 1); _emitInstruction(kX86InstSetNAE, &dst); }
  //! @brief Set Byte on Condition.
  inline void setnae(const Mem& dst)   { ASMJIT_ASSERT(dst.getSize() <= 1); _emitInstruction(kX86InstSetNAE, &dst); }
  //! @brief Set Byte on Condition.
  inline void setnb (const GpVar& dst) { ASMJIT_ASSERT(dst.getSize() == 1); _emitInstruction(kX86InstSetNB , &dst); }
  //! @brief Set Byte on Condition.
  inline void setnb (const Mem& dst)   { ASMJIT_ASSERT(dst.getSize() <= 1); _emitInstruction(kX86InstSetNB , &dst); }
  //! @brief Set Byte on Condition.
  inline void setnbe(const GpVar& dst) { ASMJIT_ASSERT(dst.getSize() == 1); _emitInstruction(kX86InstSetNBE, &dst); }
  //! @brief Set Byte on Condition.
  inline void setnbe(const Mem& dst)   { ASMJIT_ASSERT(dst.getSize() <= 1); _emitInstruction(kX86InstSetNBE, &dst); }
  //! @brief Set Byte on Condition.
  inline void setnc (const GpVar& dst) { ASMJIT_ASSERT(dst.getSize() == 1); _emitInstruction(kX86InstSetNC , &dst); }
  //! @brief Set Byte on Condition.
  inline void setnc (const Mem& dst)   { ASMJIT_ASSERT(dst.getSize() <= 1); _emitInstruction(kX86InstSetNC , &dst); }
  //! @brief Set Byte on Condition.
  inline void setne (const GpVar& dst) { ASMJIT_ASSERT(dst.getSize() == 1); _emitInstruction(kX86InstSetNE , &dst); }
  //! @brief Set Byte on Condition.
  inline void setne (const Mem& dst)   { ASMJIT_ASSERT(dst.getSize() <= 1); _emitInstruction(kX86InstSetNE , &dst); }
  //! @brief Set Byte on Condition.
  inline void setng (const GpVar& dst) { ASMJIT_ASSERT(dst.getSize() == 1); _emitInstruction(kX86InstSetNG , &dst); }
  //! @brief Set Byte on Condition.
  inline void setng (const Mem& dst)   { ASMJIT_ASSERT(dst.getSize() <= 1); _emitInstruction(kX86InstSetNG , &dst); }
  //! @brief Set Byte on Condition.
  inline void setnge(const GpVar& dst) { ASMJIT_ASSERT(dst.getSize() == 1); _emitInstruction(kX86InstSetNGE, &dst); }
  //! @brief Set Byte on Condition.
  inline void setnge(const Mem& dst)   { ASMJIT_ASSERT(dst.getSize() <= 1); _emitInstruction(kX86InstSetNGE, &dst); }
  //! @brief Set Byte on Condition.
  inline void setnl (const GpVar& dst) { ASMJIT_ASSERT(dst.getSize() == 1); _emitInstruction(kX86InstSetNL , &dst); }
  //! @brief Set Byte on Condition.
  inline void setnl (const Mem& dst)   { ASMJIT_ASSERT(dst.getSize() <= 1); _emitInstruction(kX86InstSetNL , &dst); }
  //! @brief Set Byte on Condition.
  inline void setnle(const GpVar& dst) { ASMJIT_ASSERT(dst.getSize() == 1); _emitInstruction(kX86InstSetNLE, &dst); }
  //! @brief Set Byte on Condition.
  inline void setnle(const Mem& dst)   { ASMJIT_ASSERT(dst.getSize() <= 1); _emitInstruction(kX86InstSetNLE, &dst); }
  //! @brief Set Byte on Condition.
  inline void setno (const GpVar& dst) { ASMJIT_ASSERT(dst.getSize() == 1); _emitInstruction(kX86InstSetNO , &dst); }
  //! @brief Set Byte on Condition.
  inline void setno (const Mem& dst)   { ASMJIT_ASSERT(dst.getSize() <= 1); _emitInstruction(kX86InstSetNO , &dst); }
  //! @brief Set Byte on Condition.
  inline void setnp (const GpVar& dst) { ASMJIT_ASSERT(dst.getSize() == 1); _emitInstruction(kX86InstSetNP , &dst); }
  //! @brief Set Byte on Condition.
  inline void setnp (const Mem& dst)   { ASMJIT_ASSERT(dst.getSize() <= 1); _emitInstruction(kX86InstSetNP , &dst); }
  //! @brief Set Byte on Condition.
  inline void setns (const GpVar& dst) { ASMJIT_ASSERT(dst.getSize() == 1); _emitInstruction(kX86InstSetNS , &dst); }
  //! @brief Set Byte on Condition.
  inline void setns (const Mem& dst)   { ASMJIT_ASSERT(dst.getSize() <= 1); _emitInstruction(kX86InstSetNS , &dst); }
  //! @brief Set Byte on Condition.
  inline void setnz (const GpVar& dst) { ASMJIT_ASSERT(dst.getSize() == 1); _emitInstruction(kX86InstSetNZ , &dst); }
  //! @brief Set Byte on Condition.
  inline void setnz (const Mem& dst)   { ASMJIT_ASSERT(dst.getSize() <= 1); _emitInstruction(kX86InstSetNZ , &dst); }
  //! @brief Set Byte on Condition.
  inline void seto  (const GpVar& dst) { ASMJIT_ASSERT(dst.getSize() == 1); _emitInstruction(kX86InstSetO  , &dst); }
  //! @brief Set Byte on Condition.
  inline void seto  (const Mem& dst)   { ASMJIT_ASSERT(dst.getSize() <= 1); _emitInstruction(kX86InstSetO  , &dst); }
  //! @brief Set Byte on Condition.
  inline void setp  (const GpVar& dst) { ASMJIT_ASSERT(dst.getSize() == 1); _emitInstruction(kX86InstSetP  , &dst); }
  //! @brief Set Byte on Condition.
  inline void setp  (const Mem& dst)   { ASMJIT_ASSERT(dst.getSize() <= 1); _emitInstruction(kX86InstSetP  , &dst); }
  //! @brief Set Byte on Condition.
  inline void setpe (const GpVar& dst) { ASMJIT_ASSERT(dst.getSize() == 1); _emitInstruction(kX86InstSetPE , &dst); }
  //! @brief Set Byte on Condition.
  inline void setpe (const Mem& dst)   { ASMJIT_ASSERT(dst.getSize() <= 1); _emitInstruction(kX86InstSetPE , &dst); }
  //! @brief Set Byte on Condition.
  inline void setpo (const GpVar& dst) { ASMJIT_ASSERT(dst.getSize() == 1); _emitInstruction(kX86InstSetPO , &dst); }
  //! @brief Set Byte on Condition.
  inline void setpo (const Mem& dst)   { ASMJIT_ASSERT(dst.getSize() <= 1); _emitInstruction(kX86InstSetPO , &dst); }
  //! @brief Set Byte on Condition.
  inline void sets  (const GpVar& dst) { ASMJIT_ASSERT(dst.getSize() == 1); _emitInstruction(kX86InstSetS  , &dst); }
  //! @brief Set Byte on Condition.
  inline void sets  (const Mem& dst)   { ASMJIT_ASSERT(dst.getSize() <= 1); _emitInstruction(kX86InstSetS  , &dst); }
  //! @brief Set Byte on Condition.
  inline void setz  (const GpVar& dst) { ASMJIT_ASSERT(dst.getSize() == 1); _emitInstruction(kX86InstSetZ  , &dst); }
  //! @brief Set Byte on Condition.
  inline void setz  (const Mem& dst)   { ASMJIT_ASSERT(dst.getSize() <= 1); _emitInstruction(kX86InstSetZ  , &dst); }

  //! @brief Shift Bits Left.
  //! @note @a src register can be only @c cl.
  inline void shl(const GpVar& dst, const GpVar& src)
  { _emitInstruction(kX86InstShl, &dst, &src); }

  //! @brief Shift Bits Left.
  inline void shl(const GpVar& dst, const Imm& src)
  { _emitInstruction(kX86InstShl, &dst, &src); }

  //! @brief Shift Bits Left.
  //! @note @a src register can be only @c cl.
  inline void shl(const Mem& dst, const GpVar& src)
  { _emitInstruction(kX86InstShl, &dst, &src); }

  //! @brief Shift Bits Left.
  inline void shl(const Mem& dst, const Imm& src)
  { _emitInstruction(kX86InstShl, &dst, &src); }

  //! @brief Shift Bits Right.
  //! @note @a src register can be only @c cl.
  inline void shr(const GpVar& dst, const GpVar& src)
  { _emitInstruction(kX86InstShr, &dst, &src); }

  //! @brief Shift Bits Right.
  inline void shr(const GpVar& dst, const Imm& src)
  { _emitInstruction(kX86InstShr, &dst, &src); }

  //! @brief Shift Bits Right.
  //! @note @a src register can be only @c cl.
  inline void shr(const Mem& dst, const GpVar& src)
  { _emitInstruction(kX86InstShr, &dst, &src); }

  //! @brief Shift Bits Right.
  inline void shr(const Mem& dst, const Imm& src)
  { _emitInstruction(kX86InstShr, &dst, &src); }

  //! @brief Double Precision Shift Left.
  //! @note src2 register can be only @c cl register.
  inline void shld(const GpVar& dst, const GpVar& src1, const GpVar& src2)
  { _emitInstruction(kX86InstShld, &dst, &src1, &src2); }

  //! @brief Double Precision Shift Left.
  inline void shld(const GpVar& dst, const GpVar& src1, const Imm& src2)
  { _emitInstruction(kX86InstShld, &dst, &src1, &src2); }

  //! @brief Double Precision Shift Left.
  //! @note src2 register can be only @c cl register.
  inline void shld(const Mem& dst, const GpVar& src1, const GpVar& src2)
  { _emitInstruction(kX86InstShld, &dst, &src1, &src2); }

  //! @brief Double Precision Shift Left.
  inline void shld(const Mem& dst, const GpVar& src1, const Imm& src2)
  { _emitInstruction(kX86InstShld, &dst, &src1, &src2); }

  //! @brief Double Precision Shift Right.
  //! @note src2 register can be only @c cl register.
  inline void shrd(const GpVar& dst, const GpVar& src1, const GpVar& src2)
  { _emitInstruction(kX86InstShrd, &dst, &src1, &src2); }

  //! @brief Double Precision Shift Right.
  inline void shrd(const GpVar& dst, const GpVar& src1, const Imm& src2)
  { _emitInstruction(kX86InstShrd, &dst, &src1, &src2); }

  //! @brief Double Precision Shift Right.
  //! @note src2 register can be only @c cl register.
  inline void shrd(const Mem& dst, const GpVar& src1, const GpVar& src2)
  { _emitInstruction(kX86InstShrd, &dst, &src1, &src2); }

  //! @brief Double Precision Shift Right.
  inline void shrd(const Mem& dst, const GpVar& src1, const Imm& src2)
  { _emitInstruction(kX86InstShrd, &dst, &src1, &src2); }

  //! @brief Set Carry Flag to 1.
  inline void stc()
  { _emitInstruction(kX86InstStc); }

  //! @brief Set Direction Flag to 1.
  inline void std()
  { _emitInstruction(kX86InstStd); }

  //! @brief Subtract.
  inline void sub(const GpVar& dst, const GpVar& src)
  { _emitInstruction(kX86InstSub, &dst, &src); }

  //! @brief Subtract.
  inline void sub(const GpVar& dst, const Mem& src)
  { _emitInstruction(kX86InstSub, &dst, &src); }

  //! @brief Subtract.
  inline void sub(const GpVar& dst, const Imm& src)
  { _emitInstruction(kX86InstSub, &dst, &src); }

  //! @brief Subtract.
  inline void sub(const Mem& dst, const GpVar& src)
  { _emitInstruction(kX86InstSub, &dst, &src); }

  //! @brief Subtract.
  inline void sub(const Mem& dst, const Imm& src)
  { _emitInstruction(kX86InstSub, &dst, &src); }

  //! @brief Logical Compare.
  inline void test(const GpVar& op1, const GpVar& op2)
  { _emitInstruction(kX86InstTest, &op1, &op2); }

  //! @brief Logical Compare.
  inline void test(const GpVar& op1, const Imm& op2)
  { _emitInstruction(kX86InstTest, &op1, &op2); }

  //! @brief Logical Compare.
  inline void test(const Mem& op1, const GpVar& op2)
  { _emitInstruction(kX86InstTest, &op1, &op2); }

  //! @brief Logical Compare.
  inline void test(const Mem& op1, const Imm& op2)
  { _emitInstruction(kX86InstTest, &op1, &op2); }

  //! @brief Undefined instruction - Raise invalid opcode exception.
  inline void ud2()
  { _emitInstruction(kX86InstUd2); }

  //! @brief Exchange and Add.
  inline void xadd(const GpVar& dst, const GpVar& src)
  { _emitInstruction(kX86InstXadd, &dst, &src); }

  //! @brief Exchange and Add.
  inline void xadd(const Mem& dst, const GpVar& src)
  { _emitInstruction(kX86InstXadd, &dst, &src); }

  //! @brief Exchange Register/Memory with Register.
  inline void xchg(const GpVar& dst, const GpVar& src)
  { _emitInstruction(kX86InstXchg, &dst, &src); }

  //! @brief Exchange Register/Memory with Register.
  inline void xchg(const Mem& dst, const GpVar& src)
  { _emitInstruction(kX86InstXchg, &dst, &src); }

  //! @brief Exchange Register/Memory with Register.
  inline void xchg(const GpVar& dst, const Mem& src)
  { _emitInstruction(kX86InstXchg, &src, &dst); }

  //! @brief Exchange Register/Memory with Register.
  inline void xor_(const GpVar& dst, const GpVar& src)
  { _emitInstruction(kX86InstXor, &dst, &src); }

  //! @brief Exchange Register/Memory with Register.
  inline void xor_(const GpVar& dst, const Mem& src)
  { _emitInstruction(kX86InstXor, &dst, &src); }

  //! @brief Exchange Register/Memory with Register.
  inline void xor_(const GpVar& dst, const Imm& src)
  { _emitInstruction(kX86InstXor, &dst, &src); }

  //! @brief Exchange Register/Memory with Register.
  inline void xor_(const Mem& dst, const GpVar& src)
  { _emitInstruction(kX86InstXor, &dst, &src); }

  //! @brief Exchange Register/Memory with Register.
  inline void xor_(const Mem& dst, const Imm& src)
  { _emitInstruction(kX86InstXor, &dst, &src); }

  // --------------------------------------------------------------------------
  // [MMX]
  // --------------------------------------------------------------------------

  //! @brief Empty MMX state.
  inline void emms()
  { _emitInstruction(kX86InstEmms); }

  //! @brief Move DWord (MMX).
  inline void movd(const Mem& dst, const MmVar& src)
  { _emitInstruction(kX86InstMovD, &dst, &src); }

  //! @brief Move DWord (MMX).
  inline void movd(const GpVar& dst, const MmVar& src)
  { _emitInstruction(kX86InstMovD, &dst, &src); }

  //! @brief Move DWord (MMX).
  inline void movd(const MmVar& dst, const Mem& src)
  { _emitInstruction(kX86InstMovD, &dst, &src); }

  //! @brief Move DWord (MMX).
  inline void movd(const MmVar& dst, const GpVar& src)
  { _emitInstruction(kX86InstMovD, &dst, &src); }

  //! @brief Move QWord (MMX).
  inline void movq(const MmVar& dst, const MmVar& src)
  { _emitInstruction(kX86InstMovQ, &dst, &src); }

  //! @brief Move QWord (MMX).
  inline void movq(const Mem& dst, const MmVar& src)
  { _emitInstruction(kX86InstMovQ, &dst, &src); }

#if defined(ASMJIT_X64)
  //! @brief Move QWord (MMX).
  inline void movq(const GpVar& dst, const MmVar& src)
  { _emitInstruction(kX86InstMovQ, &dst, &src); }
#endif

  //! @brief Move QWord (MMX).
  inline void movq(const MmVar& dst, const Mem& src)
  { _emitInstruction(kX86InstMovQ, &dst, &src); }

#if defined(ASMJIT_X64)
  //! @brief Move QWord (MMX).
  inline void movq(const MmVar& dst, const GpVar& src)
  { _emitInstruction(kX86InstMovQ, &dst, &src); }
#endif

  //! @brief Pack with Signed Saturation (MMX).
  inline void packsswb(const MmVar& dst, const MmVar& src)
  { _emitInstruction(kX86InstPackSSWB, &dst, &src); }

  //! @brief Pack with Signed Saturation (MMX).
  inline void packsswb(const MmVar& dst, const Mem& src)
  { _emitInstruction(kX86InstPackSSWB, &dst, &src); }

  //! @brief Pack with Signed Saturation (MMX).
  inline void packssdw(const MmVar& dst, const MmVar& src)
  { _emitInstruction(kX86InstPackSSDW, &dst, &src); }

  //! @brief Pack with Signed Saturation (MMX).
  inline void packssdw(const MmVar& dst, const Mem& src)
  { _emitInstruction(kX86InstPackSSDW, &dst, &src); }

  //! @brief Pack with Unsigned Saturation (MMX).
  inline void packuswb(const MmVar& dst, const MmVar& src)
  { _emitInstruction(kX86InstPackUSWB, &dst, &src); }

  //! @brief Pack with Unsigned Saturation (MMX).
  inline void packuswb(const MmVar& dst, const Mem& src)
  { _emitInstruction(kX86InstPackUSWB, &dst, &src); }

  //! @brief Packed BYTE Add (MMX).
  inline void paddb(const MmVar& dst, const MmVar& src)
  { _emitInstruction(kX86InstPAddB, &dst, &src); }

  //! @brief Packed BYTE Add (MMX).
  inline void paddb(const MmVar& dst, const Mem& src)
  { _emitInstruction(kX86InstPAddB, &dst, &src); }

  //! @brief Packed WORD Add (MMX).
  inline void paddw(const MmVar& dst, const MmVar& src)
  { _emitInstruction(kX86InstPAddW, &dst, &src); }

  //! @brief Packed WORD Add (MMX).
  inline void paddw(const MmVar& dst, const Mem& src)
  { _emitInstruction(kX86InstPAddW, &dst, &src); }

  //! @brief Packed DWORD Add (MMX).
  inline void paddd(const MmVar& dst, const MmVar& src)
  { _emitInstruction(kX86InstPAddD, &dst, &src); }

  //! @brief Packed DWORD Add (MMX).
  inline void paddd(const MmVar& dst, const Mem& src)
  { _emitInstruction(kX86InstPAddD, &dst, &src); }

  //! @brief Packed Add with Saturation (MMX).
  inline void paddsb(const MmVar& dst, const MmVar& src)
  { _emitInstruction(kX86InstPAddSB, &dst, &src); }

  //! @brief Packed Add with Saturation (MMX).
  inline void paddsb(const MmVar& dst, const Mem& src)
  { _emitInstruction(kX86InstPAddSB, &dst, &src); }

  //! @brief Packed Add with Saturation (MMX).
  inline void paddsw(const MmVar& dst, const MmVar& src)
  { _emitInstruction(kX86InstPAddSW, &dst, &src); }

  //! @brief Packed Add with Saturation (MMX).
  inline void paddsw(const MmVar& dst, const Mem& src)
  { _emitInstruction(kX86InstPAddSW, &dst, &src); }

  //! @brief Packed Add Unsigned with Saturation (MMX).
  inline void paddusb(const MmVar& dst, const MmVar& src)
  { _emitInstruction(kX86InstPAddUSB, &dst, &src); }

  //! @brief Packed Add Unsigned with Saturation (MMX).
  inline void paddusb(const MmVar& dst, const Mem& src)
  { _emitInstruction(kX86InstPAddUSB, &dst, &src); }

  //! @brief Packed Add Unsigned with Saturation (MMX).
  inline void paddusw(const MmVar& dst, const MmVar& src)
  { _emitInstruction(kX86InstPAddUSW, &dst, &src); }

  //! @brief Packed Add Unsigned with Saturation (MMX).
  inline void paddusw(const MmVar& dst, const Mem& src)
  { _emitInstruction(kX86InstPAddUSW, &dst, &src); }

  //! @brief Logical AND (MMX).
  inline void pand(const MmVar& dst, const MmVar& src)
  { _emitInstruction(kX86InstPAnd, &dst, &src); }

  //! @brief Logical AND (MMX).
  inline void pand(const MmVar& dst, const Mem& src)
  { _emitInstruction(kX86InstPAnd, &dst, &src); }

  //! @brief Logical AND Not (MMX).
  inline void pandn(const MmVar& dst, const MmVar& src)
  { _emitInstruction(kX86InstPAndN, &dst, &src); }

  //! @brief Logical AND Not (MMX).
  inline void pandn(const MmVar& dst, const Mem& src)
  { _emitInstruction(kX86InstPAndN, &dst, &src); }

  //! @brief Packed Compare for Equal (BYTES) (MMX).
  inline void pcmpeqb(const MmVar& dst, const MmVar& src)
  { _emitInstruction(kX86InstPCmpEqB, &dst, &src); }

  //! @brief Packed Compare for Equal (BYTES) (MMX).
  inline void pcmpeqb(const MmVar& dst, const Mem& src)
  { _emitInstruction(kX86InstPCmpEqB, &dst, &src); }

  //! @brief Packed Compare for Equal (WORDS) (MMX).
  inline void pcmpeqw(const MmVar& dst, const MmVar& src)
  { _emitInstruction(kX86InstPCmpEqW, &dst, &src); }

  //! @brief Packed Compare for Equal (WORDS) (MMX).
  inline void pcmpeqw(const MmVar& dst, const Mem& src)
  { _emitInstruction(kX86InstPCmpEqW, &dst, &src); }

  //! @brief Packed Compare for Equal (DWORDS) (MMX).
  inline void pcmpeqd(const MmVar& dst, const MmVar& src)
  { _emitInstruction(kX86InstPCmpEqD, &dst, &src); }

  //! @brief Packed Compare for Equal (DWORDS) (MMX).
  inline void pcmpeqd(const MmVar& dst, const Mem& src)
  { _emitInstruction(kX86InstPCmpEqD, &dst, &src); }

  //! @brief Packed Compare for Greater Than (BYTES) (MMX).
  inline void pcmpgtb(const MmVar& dst, const MmVar& src)
  { _emitInstruction(kX86InstPCmpGtB, &dst, &src); }

  //! @brief Packed Compare for Greater Than (BYTES) (MMX).
  inline void pcmpgtb(const MmVar& dst, const Mem& src)
  { _emitInstruction(kX86InstPCmpGtB, &dst, &src); }

  //! @brief Packed Compare for Greater Than (WORDS) (MMX).
  inline void pcmpgtw(const MmVar& dst, const MmVar& src)
  { _emitInstruction(kX86InstPCmpGtW, &dst, &src); }

  //! @brief Packed Compare for Greater Than (WORDS) (MMX).
  inline void pcmpgtw(const MmVar& dst, const Mem& src)
  { _emitInstruction(kX86InstPCmpGtW, &dst, &src); }

  //! @brief Packed Compare for Greater Than (DWORDS) (MMX).
  inline void pcmpgtd(const MmVar& dst, const MmVar& src)
  { _emitInstruction(kX86InstPCmpGtD, &dst, &src); }

  //! @brief Packed Compare for Greater Than (DWORDS) (MMX).
  inline void pcmpgtd(const MmVar& dst, const Mem& src)
  { _emitInstruction(kX86InstPCmpGtD, &dst, &src); }

  //! @brief Packed Multiply High (MMX).
  inline void pmulhw(const MmVar& dst, const MmVar& src)
  { _emitInstruction(kX86InstPMulHW, &dst, &src); }

  //! @brief Packed Multiply High (MMX).
  inline void pmulhw(const MmVar& dst, const Mem& src)
  { _emitInstruction(kX86InstPMulHW, &dst, &src); }

  //! @brief Packed Multiply Low (MMX).
  inline void pmullw(const MmVar& dst, const MmVar& src)
  { _emitInstruction(kX86InstPMulLW, &dst, &src); }

  //! @brief Packed Multiply Low (MMX).
  inline void pmullw(const MmVar& dst, const Mem& src)
  { _emitInstruction(kX86InstPMulLW, &dst, &src); }

  //! @brief Bitwise Logical OR (MMX).
  inline void por(const MmVar& dst, const MmVar& src)
  { _emitInstruction(kX86InstPOr, &dst, &src); }

  //! @brief Bitwise Logical OR (MMX).
  inline void por(const MmVar& dst, const Mem& src)
  { _emitInstruction(kX86InstPOr, &dst, &src); }

  //! @brief Packed Multiply and Add (MMX).
  inline void pmaddwd(const MmVar& dst, const MmVar& src)
  { _emitInstruction(kX86InstPMAddWD, &dst, &src); }

  //! @brief Packed Multiply and Add (MMX).
  inline void pmaddwd(const MmVar& dst, const Mem& src)
  { _emitInstruction(kX86InstPMAddWD, &dst, &src); }

  //! @brief Packed Shift Left Logical (MMX).
  inline void pslld(const MmVar& dst, const MmVar& src)
  { _emitInstruction(kX86InstPSllD, &dst, &src); }

  //! @brief Packed Shift Left Logical (MMX).
  inline void pslld(const MmVar& dst, const Mem& src)
  { _emitInstruction(kX86InstPSllD, &dst, &src); }

  //! @brief Packed Shift Left Logical (MMX).
  inline void pslld(const MmVar& dst, const Imm& src)
  { _emitInstruction(kX86InstPSllD, &dst, &src); }

  //! @brief Packed Shift Left Logical (MMX).
  inline void psllq(const MmVar& dst, const MmVar& src)
  { _emitInstruction(kX86InstPSllQ, &dst, &src); }

  //! @brief Packed Shift Left Logical (MMX).
  inline void psllq(const MmVar& dst, const Mem& src)
  { _emitInstruction(kX86InstPSllQ, &dst, &src); }

  //! @brief Packed Shift Left Logical (MMX).
  inline void psllq(const MmVar& dst, const Imm& src)
  { _emitInstruction(kX86InstPSllQ, &dst, &src); }

  //! @brief Packed Shift Left Logical (MMX).
  inline void psllw(const MmVar& dst, const MmVar& src)
  { _emitInstruction(kX86InstPSllW, &dst, &src); }

  //! @brief Packed Shift Left Logical (MMX).
  inline void psllw(const MmVar& dst, const Mem& src)
  { _emitInstruction(kX86InstPSllW, &dst, &src); }

  //! @brief Packed Shift Left Logical (MMX).
  inline void psllw(const MmVar& dst, const Imm& src)
  { _emitInstruction(kX86InstPSllW, &dst, &src); }

  //! @brief Packed Shift Right Arithmetic (MMX).
  inline void psrad(const MmVar& dst, const MmVar& src)
  { _emitInstruction(kX86InstPSraD, &dst, &src); }

  //! @brief Packed Shift Right Arithmetic (MMX).
  inline void psrad(const MmVar& dst, const Mem& src)
  { _emitInstruction(kX86InstPSraD, &dst, &src);}

  //! @brief Packed Shift Right Arithmetic (MMX).
  inline void psrad(const MmVar& dst, const Imm& src)
  { _emitInstruction(kX86InstPSraD, &dst, &src); }

  //! @brief Packed Shift Right Arithmetic (MMX).
  inline void psraw(const MmVar& dst, const MmVar& src)
  { _emitInstruction(kX86InstPSraW, &dst, &src); }

  //! @brief Packed Shift Right Arithmetic (MMX).
  inline void psraw(const MmVar& dst, const Mem& src)
  { _emitInstruction(kX86InstPSraW, &dst, &src); }

  //! @brief Packed Shift Right Arithmetic (MMX).
  inline void psraw(const MmVar& dst, const Imm& src)
  { _emitInstruction(kX86InstPSraW, &dst, &src); }

  //! @brief Packed Shift Right Logical (MMX).
  inline void psrld(const MmVar& dst, const MmVar& src)
  { _emitInstruction(kX86InstPSrlD, &dst, &src); }

  //! @brief Packed Shift Right Logical (MMX).
  inline void psrld(const MmVar& dst, const Mem& src)
  { _emitInstruction(kX86InstPSrlD, &dst, &src); }

  //! @brief Packed Shift Right Logical (MMX).
  inline void psrld(const MmVar& dst, const Imm& src)
  { _emitInstruction(kX86InstPSrlD, &dst, &src); }

  //! @brief Packed Shift Right Logical (MMX).
  inline void psrlq(const MmVar& dst, const MmVar& src)
  { _emitInstruction(kX86InstPSrlQ, &dst, &src); }

  //! @brief Packed Shift Right Logical (MMX).
  inline void psrlq(const MmVar& dst, const Mem& src)
  { _emitInstruction(kX86InstPSrlQ, &dst, &src); }

  //! @brief Packed Shift Right Logical (MMX).
  inline void psrlq(const MmVar& dst, const Imm& src)
  { _emitInstruction(kX86InstPSrlQ, &dst, &src); }

  //! @brief Packed Shift Right Logical (MMX).
  inline void psrlw(const MmVar& dst, const MmVar& src)
  { _emitInstruction(kX86InstPSrlW, &dst, &src); }

  //! @brief Packed Shift Right Logical (MMX).
  inline void psrlw(const MmVar& dst, const Mem& src)
  { _emitInstruction(kX86InstPSrlW, &dst, &src); }

  //! @brief Packed Shift Right Logical (MMX).
  inline void psrlw(const MmVar& dst, const Imm& src)
  { _emitInstruction(kX86InstPSrlW, &dst, &src); }

  //! @brief Packed Subtract (MMX).
  inline void psubb(const MmVar& dst, const MmVar& src)
  { _emitInstruction(kX86InstPSubB, &dst, &src); }

  //! @brief Packed Subtract (MMX).
  inline void psubb(const MmVar& dst, const Mem& src)
  { _emitInstruction(kX86InstPSubB, &dst, &src); }

  //! @brief Packed Subtract (MMX).
  inline void psubw(const MmVar& dst, const MmVar& src)
  { _emitInstruction(kX86InstPSubW, &dst, &src); }

  //! @brief Packed Subtract (MMX).
  inline void psubw(const MmVar& dst, const Mem& src)
  { _emitInstruction(kX86InstPSubW, &dst, &src); }

  //! @brief Packed Subtract (MMX).
  inline void psubd(const MmVar& dst, const MmVar& src)
  { _emitInstruction(kX86InstPSubD, &dst, &src); }

  //! @brief Packed Subtract (MMX).
  inline void psubd(const MmVar& dst, const Mem& src)
  { _emitInstruction(kX86InstPSubD, &dst, &src); }

  //! @brief Packed Subtract with Saturation (MMX).
  inline void psubsb(const MmVar& dst, const MmVar& src)
  { _emitInstruction(kX86InstPSubSB, &dst, &src); }

  //! @brief Packed Subtract with Saturation (MMX).
  inline void psubsb(const MmVar& dst, const Mem& src)
  { _emitInstruction(kX86InstPSubSB, &dst, &src); }

  //! @brief Packed Subtract with Saturation (MMX).
  inline void psubsw(const MmVar& dst, const MmVar& src)
  { _emitInstruction(kX86InstPSubSW, &dst, &src); }

  //! @brief Packed Subtract with Saturation (MMX).
  inline void psubsw(const MmVar& dst, const Mem& src)
  { _emitInstruction(kX86InstPSubSW, &dst, &src); }

  //! @brief Packed Subtract with Unsigned Saturation (MMX).
  inline void psubusb(const MmVar& dst, const MmVar& src)
  { _emitInstruction(kX86InstPSubUSB, &dst, &src); }

  //! @brief Packed Subtract with Unsigned Saturation (MMX).
  inline void psubusb(const MmVar& dst, const Mem& src)
  { _emitInstruction(kX86InstPSubUSB, &dst, &src); }

  //! @brief Packed Subtract with Unsigned Saturation (MMX).
  inline void psubusw(const MmVar& dst, const MmVar& src)
  { _emitInstruction(kX86InstPSubUSW, &dst, &src); }

  //! @brief Packed Subtract with Unsigned Saturation (MMX).
  inline void psubusw(const MmVar& dst, const Mem& src)
  { _emitInstruction(kX86InstPSubUSW, &dst, &src); }

  //! @brief Unpack High Packed Data (MMX).
  inline void punpckhbw(const MmVar& dst, const MmVar& src)
  { _emitInstruction(kX86InstPunpckHBW, &dst, &src); }

  //! @brief Unpack High Packed Data (MMX).
  inline void punpckhbw(const MmVar& dst, const Mem& src)
  { _emitInstruction(kX86InstPunpckHBW, &dst, &src); }

  //! @brief Unpack High Packed Data (MMX).
  inline void punpckhwd(const MmVar& dst, const MmVar& src)
  { _emitInstruction(kX86InstPunpckHWD, &dst, &src); }

  //! @brief Unpack High Packed Data (MMX).
  inline void punpckhwd(const MmVar& dst, const Mem& src)
  { _emitInstruction(kX86InstPunpckHWD, &dst, &src); }

  //! @brief Unpack High Packed Data (MMX).
  inline void punpckhdq(const MmVar& dst, const MmVar& src)
  { _emitInstruction(kX86InstPunpckHDQ, &dst, &src); }

  //! @brief Unpack High Packed Data (MMX).
  inline void punpckhdq(const MmVar& dst, const Mem& src)
  { _emitInstruction(kX86InstPunpckHDQ, &dst, &src); }

  //! @brief Unpack High Packed Data (MMX).
  inline void punpcklbw(const MmVar& dst, const MmVar& src)
  { _emitInstruction(kX86InstPunpckLBW, &dst, &src); }

  //! @brief Unpack High Packed Data (MMX).
  inline void punpcklbw(const MmVar& dst, const Mem& src)
  { _emitInstruction(kX86InstPunpckLBW, &dst, &src); }

  //! @brief Unpack High Packed Data (MMX).
  inline void punpcklwd(const MmVar& dst, const MmVar& src)
  { _emitInstruction(kX86InstPunpckLWD, &dst, &src); }

  //! @brief Unpack High Packed Data (MMX).
  inline void punpcklwd(const MmVar& dst, const Mem& src)
  { _emitInstruction(kX86InstPunpckLWD, &dst, &src); }

  //! @brief Unpack High Packed Data (MMX).
  inline void punpckldq(const MmVar& dst, const MmVar& src)
  { _emitInstruction(kX86InstPunpckLDQ, &dst, &src); }

  //! @brief Unpack High Packed Data (MMX).
  inline void punpckldq(const MmVar& dst, const Mem& src)
  { _emitInstruction(kX86InstPunpckLDQ, &dst, &src); }

  //! @brief Bitwise Exclusive OR (MMX).
  inline void pxor(const MmVar& dst, const MmVar& src)
  { _emitInstruction(kX86InstPXor, &dst, &src); }

  //! @brief Bitwise Exclusive OR (MMX).
  inline void pxor(const MmVar& dst, const Mem& src)
  { _emitInstruction(kX86InstPXor, &dst, &src); }

  // --------------------------------------------------------------------------
  // [3dNow]
  // --------------------------------------------------------------------------

  //! @brief Faster EMMS (3dNow!).
  //!
  //! @note Use only for early AMD processors where is only 3dNow! or SSE. If
  //! CPU contains SSE2, it's better to use @c emms() ( @c femms() is mapped
  //! to @c emms() ).
  inline void femms()
  { _emitInstruction(kX86InstFEmms); }

  //! @brief Packed SP-FP to Integer Convert (3dNow!).
  inline void pf2id(const MmVar& dst, const MmVar& src)
  { _emitInstruction(kX86InstPF2ID, &dst, &src); }

  //! @brief Packed SP-FP to Integer Convert (3dNow!).
  inline void pf2id(const MmVar& dst, const Mem& src)
  { _emitInstruction(kX86InstPF2ID, &dst, &src); }

  //! @brief  Packed SP-FP to Integer Word Convert (3dNow!).
  inline void pf2iw(const MmVar& dst, const MmVar& src)
  { _emitInstruction(kX86InstPF2IW, &dst, &src); }

  //! @brief  Packed SP-FP to Integer Word Convert (3dNow!).
  inline void pf2iw(const MmVar& dst, const Mem& src)
  { _emitInstruction(kX86InstPF2IW, &dst, &src); }

  //! @brief Packed SP-FP Accumulate (3dNow!).
  inline void pfacc(const MmVar& dst, const MmVar& src)
  { _emitInstruction(kX86InstPFAcc, &dst, &src); }

  //! @brief Packed SP-FP Accumulate (3dNow!).
  inline void pfacc(const MmVar& dst, const Mem& src)
  { _emitInstruction(kX86InstPFAcc, &dst, &src); }

  //! @brief Packed SP-FP Addition (3dNow!).
  inline void pfadd(const MmVar& dst, const MmVar& src)
  { _emitInstruction(kX86InstPFAdd, &dst, &src); }

  //! @brief Packed SP-FP Addition (3dNow!).
  inline void pfadd(const MmVar& dst, const Mem& src)
  { _emitInstruction(kX86InstPFAdd, &dst, &src); }

  //! @brief Packed SP-FP Compare - dst == src (3dNow!).
  inline void pfcmpeq(const MmVar& dst, const MmVar& src)
  { _emitInstruction(kX86InstPFCmpEQ, &dst, &src); }

  //! @brief Packed SP-FP Compare - dst == src (3dNow!).
  inline void pfcmpeq(const MmVar& dst, const Mem& src)
  { _emitInstruction(kX86InstPFCmpEQ, &dst, &src); }

  //! @brief Packed SP-FP Compare - dst >= src (3dNow!).
  inline void pfcmpge(const MmVar& dst, const MmVar& src)
  { _emitInstruction(kX86InstPFCmpGE, &dst, &src); }

  //! @brief Packed SP-FP Compare - dst >= src (3dNow!).
  inline void pfcmpge(const MmVar& dst, const Mem& src)
  { _emitInstruction(kX86InstPFCmpGE, &dst, &src); }

  //! @brief Packed SP-FP Compare - dst > src (3dNow!).
  inline void pfcmpgt(const MmVar& dst, const MmVar& src)
  { _emitInstruction(kX86InstPFCmpGT, &dst, &src); }

  //! @brief Packed SP-FP Compare - dst > src (3dNow!).
  inline void pfcmpgt(const MmVar& dst, const Mem& src)
  { _emitInstruction(kX86InstPFCmpGT, &dst, &src); }

  //! @brief Packed SP-FP Maximum (3dNow!).
  inline void pfmax(const MmVar& dst, const MmVar& src)
  { _emitInstruction(kX86InstPFMax, &dst, &src); }

  //! @brief Packed SP-FP Maximum (3dNow!).
  inline void pfmax(const MmVar& dst, const Mem& src)
  { _emitInstruction(kX86InstPFMax, &dst, &src); }

  //! @brief Packed SP-FP Minimum (3dNow!).
  inline void pfmin(const MmVar& dst, const MmVar& src)
  { _emitInstruction(kX86InstPFMin, &dst, &src); }

  //! @brief Packed SP-FP Minimum (3dNow!).
  inline void pfmin(const MmVar& dst, const Mem& src)
  { _emitInstruction(kX86InstPFMin, &dst, &src); }

  //! @brief Packed SP-FP Multiply (3dNow!).
  inline void pfmul(const MmVar& dst, const MmVar& src)
  { _emitInstruction(kX86InstPFMul, &dst, &src); }

  //! @brief Packed SP-FP Multiply (3dNow!).
  inline void pfmul(const MmVar& dst, const Mem& src)
  { _emitInstruction(kX86InstPFMul, &dst, &src); }

  //! @brief Packed SP-FP Negative Accumulate (3dNow!).
  inline void pfnacc(const MmVar& dst, const MmVar& src)
  { _emitInstruction(kX86InstPFNAcc, &dst, &src); }

  //! @brief Packed SP-FP Negative Accumulate (3dNow!).
  inline void pfnacc(const MmVar& dst, const Mem& src)
  { _emitInstruction(kX86InstPFNAcc, &dst, &src); }

  //! @brief Packed SP-FP Mixed Accumulate (3dNow!).
  inline void pfpnacc(const MmVar& dst, const MmVar& src)
  { _emitInstruction(kX86InstPFPNAcc, &dst, &src); }

  //! @brief Packed SP-FP Mixed Accumulate (3dNow!).
  inline void pfpnacc(const MmVar& dst, const Mem& src)
  { _emitInstruction(kX86InstPFPNAcc, &dst, &src); }

  //! @brief Packed SP-FP Reciprocal Approximation (3dNow!).
  inline void pfrcp(const MmVar& dst, const MmVar& src)
  { _emitInstruction(kX86InstPFRcp, &dst, &src); }

  //! @brief Packed SP-FP Reciprocal Approximation (3dNow!).
  inline void pfrcp(const MmVar& dst, const Mem& src)
  { _emitInstruction(kX86InstPFRcp, &dst, &src); }

  //! @brief Packed SP-FP Reciprocal, First Iteration Step (3dNow!).
  inline void pfrcpit1(const MmVar& dst, const MmVar& src)
  { _emitInstruction(kX86InstPFRcpIt1, &dst, &src); }

  //! @brief Packed SP-FP Reciprocal, First Iteration Step (3dNow!).
  inline void pfrcpit1(const MmVar& dst, const Mem& src)
  { _emitInstruction(kX86InstPFRcpIt1, &dst, &src); }

  //! @brief Packed SP-FP Reciprocal, Second Iteration Step (3dNow!).
  inline void pfrcpit2(const MmVar& dst, const MmVar& src)
  { _emitInstruction(kX86InstPFRcpIt2, &dst, &src); }

  //! @brief Packed SP-FP Reciprocal, Second Iteration Step (3dNow!).
  inline void pfrcpit2(const MmVar& dst, const Mem& src)
  { _emitInstruction(kX86InstPFRcpIt2, &dst, &src); }

  //! @brief Packed SP-FP Reciprocal Square Root, First Iteration Step (3dNow!).
  inline void pfrsqit1(const MmVar& dst, const MmVar& src)
  { _emitInstruction(kX86InstPFRSqIt1, &dst, &src); }

  //! @brief Packed SP-FP Reciprocal Square Root, First Iteration Step (3dNow!).
  inline void pfrsqit1(const MmVar& dst, const Mem& src)
  { _emitInstruction(kX86InstPFRSqIt1, &dst, &src); }

  //! @brief Packed SP-FP Reciprocal Square Root Approximation (3dNow!).
  inline void pfrsqrt(const MmVar& dst, const MmVar& src)
  { _emitInstruction(kX86InstPFRSqrt, &dst, &src); }

  //! @brief Packed SP-FP Reciprocal Square Root Approximation (3dNow!).
  inline void pfrsqrt(const MmVar& dst, const Mem& src)
  { _emitInstruction(kX86InstPFRSqrt, &dst, &src); }

  //! @brief Packed SP-FP Subtract (3dNow!).
  inline void pfsub(const MmVar& dst, const MmVar& src)
  { _emitInstruction(kX86InstPFSub, &dst, &src); }

  //! @brief Packed SP-FP Subtract (3dNow!).
  inline void pfsub(const MmVar& dst, const Mem& src)
  { _emitInstruction(kX86InstPFSub, &dst, &src); }

  //! @brief Packed SP-FP Reverse Subtract (3dNow!).
  inline void pfsubr(const MmVar& dst, const MmVar& src)
  { _emitInstruction(kX86InstPFSubR, &dst, &src); }

  //! @brief Packed SP-FP Reverse Subtract (3dNow!).
  inline void pfsubr(const MmVar& dst, const Mem& src)
  { _emitInstruction(kX86InstPFSubR, &dst, &src); }

  //! @brief Packed DWords to SP-FP (3dNow!).
  inline void pi2fd(const MmVar& dst, const MmVar& src)
  { _emitInstruction(kX86InstPI2FD, &dst, &src); }

  //! @brief Packed DWords to SP-FP (3dNow!).
  inline void pi2fd(const MmVar& dst, const Mem& src)
  { _emitInstruction(kX86InstPI2FD, &dst, &src); }

  //! @brief Packed Words to SP-FP (3dNow!).
  inline void pi2fw(const MmVar& dst, const MmVar& src)
  { _emitInstruction(kX86InstPI2FW, &dst, &src); }

  //! @brief Packed Words to SP-FP (3dNow!).
  inline void pi2fw(const MmVar& dst, const Mem& src)
  { _emitInstruction(kX86InstPI2FW, &dst, &src); }

  //! @brief Packed swap DWord (3dNow!)
  inline void pswapd(const MmVar& dst, const MmVar& src)
  { _emitInstruction(kX86InstPSwapD, &dst, &src); }

  //! @brief Packed swap DWord (3dNow!)
  inline void pswapd(const MmVar& dst, const Mem& src)
  { _emitInstruction(kX86InstPSwapD, &dst, &src); }

  // --------------------------------------------------------------------------
  // [SSE]
  // --------------------------------------------------------------------------

  //! @brief Packed SP-FP Add (SSE).
  inline void addps(const XmmVar& dst, const XmmVar& src)
  { _emitInstruction(kX86InstAddPS, &dst, &src); }
  //! @brief Packed SP-FP Add (SSE).
  inline void addps(const XmmVar& dst, const Mem& src)
  { _emitInstruction(kX86InstAddPS, &dst, &src); }

  //! @brief Scalar SP-FP Add (SSE).
  inline void addss(const XmmVar& dst, const XmmVar& src)
  { _emitInstruction(kX86InstAddSS, &dst, &src); }
  //! @brief Scalar SP-FP Add (SSE).
  inline void addss(const XmmVar& dst, const Mem& src)
  { _emitInstruction(kX86InstAddSS, &dst, &src); }

  //! @brief Bit-wise Logical And Not For SP-FP (SSE).
  inline void andnps(const XmmVar& dst, const XmmVar& src)
  { _emitInstruction(kX86InstAndnPS, &dst, &src); }
  //! @brief Bit-wise Logical And Not For SP-FP (SSE).
  inline void andnps(const XmmVar& dst, const Mem& src)
  { _emitInstruction(kX86InstAndnPS, &dst, &src); }

  //! @brief Bit-wise Logical And For SP-FP (SSE).
  inline void andps(const XmmVar& dst, const XmmVar& src)
  { _emitInstruction(kX86InstAndPS, &dst, &src); }
  //! @brief Bit-wise Logical And For SP-FP (SSE).
  inline void andps(const XmmVar& dst, const Mem& src)
  { _emitInstruction(kX86InstAndPS, &dst, &src); }

  //! @brief Packed SP-FP Compare (SSE).
  inline void cmpps(const XmmVar& dst, const XmmVar& src, const Imm& imm8)
  { _emitInstruction(kX86InstCmpPS, &dst, &src, &imm8); }
  //! @brief Packed SP-FP Compare (SSE).
  inline void cmpps(const XmmVar& dst, const Mem& src, const Imm& imm8)
  { _emitInstruction(kX86InstCmpPS, &dst, &src, &imm8); }

  //! @brief Compare Scalar SP-FP Values (SSE).
  inline void cmpss(const XmmVar& dst, const XmmVar& src, const Imm& imm8)
  { _emitInstruction(kX86InstCmpSS, &dst, &src, &imm8); }
  //! @brief Compare Scalar SP-FP Values (SSE).
  inline void cmpss(const XmmVar& dst, const Mem& src, const Imm& imm8)
  { _emitInstruction(kX86InstCmpSS, &dst, &src, &imm8); }

  //! @brief Scalar Ordered SP-FP Compare and Set EFLAGS (SSE).
  inline void comiss(const XmmVar& dst, const XmmVar& src)
  { _emitInstruction(kX86InstComISS, &dst, &src); }
  //! @brief Scalar Ordered SP-FP Compare and Set EFLAGS (SSE).
  inline void comiss(const XmmVar& dst, const Mem& src)
  { _emitInstruction(kX86InstComISS, &dst, &src); }

  //! @brief Packed Signed INT32 to Packed SP-FP Conversion (SSE).
  inline void cvtpi2ps(const XmmVar& dst, const MmVar& src)
  { _emitInstruction(kX86InstCvtPI2PS, &dst, &src); }
  //! @brief Packed Signed INT32 to Packed SP-FP Conversion (SSE).
  inline void cvtpi2ps(const XmmVar& dst, const Mem& src)
  { _emitInstruction(kX86InstCvtPI2PS, &dst, &src); }

  //! @brief Packed SP-FP to Packed INT32 Conversion (SSE).
  inline void cvtps2pi(const MmVar& dst, const XmmVar& src)
  { _emitInstruction(kX86InstCvtPS2PI, &dst, &src); }
  //! @brief Packed SP-FP to Packed INT32 Conversion (SSE).
  inline void cvtps2pi(const MmVar& dst, const Mem& src)
  { _emitInstruction(kX86InstCvtPS2PI, &dst, &src); }

  //! @brief Scalar Signed INT32 to SP-FP Conversion (SSE).
  inline void cvtsi2ss(const XmmVar& dst, const GpVar& src)
  { _emitInstruction(kX86InstCvtSI2SS, &dst, &src); }
  //! @brief Scalar Signed INT32 to SP-FP Conversion (SSE).
  inline void cvtsi2ss(const XmmVar& dst, const Mem& src)
  { _emitInstruction(kX86InstCvtSI2SS, &dst, &src); }

  //! @brief Scalar SP-FP to Signed INT32 Conversion (SSE).
  inline void cvtss2si(const GpVar& dst, const XmmVar& src)
  { _emitInstruction(kX86InstCvtSS2SI, &dst, &src); }
  //! @brief Scalar SP-FP to Signed INT32 Conversion (SSE).
  inline void cvtss2si(const GpVar& dst, const Mem& src)
  { _emitInstruction(kX86InstCvtSS2SI, &dst, &src); }

  //! @brief Packed SP-FP to Packed INT32 Conversion (truncate) (SSE).
  inline void cvttps2pi(const MmVar& dst, const XmmVar& src)
  { _emitInstruction(kX86InstCvttPS2PI, &dst, &src); }
  //! @brief Packed SP-FP to Packed INT32 Conversion (truncate) (SSE).
  inline void cvttps2pi(const MmVar& dst, const Mem& src)
  { _emitInstruction(kX86InstCvttPS2PI, &dst, &src); }

  //! @brief Scalar SP-FP to Signed INT32 Conversion (truncate) (SSE).
  inline void cvttss2si(const GpVar& dst, const XmmVar& src)
  { _emitInstruction(kX86InstCvttSS2SI, &dst, &src); }
  //! @brief Scalar SP-FP to Signed INT32 Conversion (truncate) (SSE).
  inline void cvttss2si(const GpVar& dst, const Mem& src)
  { _emitInstruction(kX86InstCvttSS2SI, &dst, &src); }

  //! @brief Packed SP-FP Divide (SSE).
  inline void divps(const XmmVar& dst, const XmmVar& src)
  { _emitInstruction(kX86InstDivPS, &dst, &src); }
  //! @brief Packed SP-FP Divide (SSE).
  inline void divps(const XmmVar& dst, const Mem& src)
  { _emitInstruction(kX86InstDivPS, &dst, &src); }

  //! @brief Scalar SP-FP Divide (SSE).
  inline void divss(const XmmVar& dst, const XmmVar& src)
  { _emitInstruction(kX86InstDivSS, &dst, &src); }
  //! @brief Scalar SP-FP Divide (SSE).
  inline void divss(const XmmVar& dst, const Mem& src)
  { _emitInstruction(kX86InstDivSS, &dst, &src); }

  //! @brief Load Streaming SIMD Extension Control/Status (SSE).
  inline void ldmxcsr(const Mem& src)
  { _emitInstruction(kX86InstLdMXCSR, &src); }

  //! @brief Byte Mask Write (SSE).
  //!
  //! @note The default memory location is specified by DS:EDI.
  inline void maskmovq(const GpVar& dst_ptr, const MmVar& data, const MmVar& mask)
  { _emitInstruction(kX86InstMaskMovQ, &dst_ptr, &data, &mask); }

  //! @brief Packed SP-FP Maximum (SSE).
  inline void maxps(const XmmVar& dst, const XmmVar& src)
  { _emitInstruction(kX86InstMaxPS, &dst, &src); }
  //! @brief Packed SP-FP Maximum (SSE).
  inline void maxps(const XmmVar& dst, const Mem& src)
  { _emitInstruction(kX86InstMaxPS, &dst, &src); }

  //! @brief Scalar SP-FP Maximum (SSE).
  inline void maxss(const XmmVar& dst, const XmmVar& src)
  { _emitInstruction(kX86InstMaxSS, &dst, &src); }
  //! @brief Scalar SP-FP Maximum (SSE).
  inline void maxss(const XmmVar& dst, const Mem& src)
  { _emitInstruction(kX86InstMaxSS, &dst, &src); }

  //! @brief Packed SP-FP Minimum (SSE).
  inline void minps(const XmmVar& dst, const XmmVar& src)
  { _emitInstruction(kX86InstMinPS, &dst, &src); }
  //! @brief Packed SP-FP Minimum (SSE).
  inline void minps(const XmmVar& dst, const Mem& src)
  { _emitInstruction(kX86InstMinPS, &dst, &src); }

  //! @brief Scalar SP-FP Minimum (SSE).
  inline void minss(const XmmVar& dst, const XmmVar& src)
  { _emitInstruction(kX86InstMinSS, &dst, &src); }
  //! @brief Scalar SP-FP Minimum (SSE).
  inline void minss(const XmmVar& dst, const Mem& src)
  { _emitInstruction(kX86InstMinSS, &dst, &src); }

  //! @brief Move Aligned Packed SP-FP Values (SSE).
  inline void movaps(const XmmVar& dst, const XmmVar& src)
  { _emitInstruction(kX86InstMovAPS, &dst, &src); }
  //! @brief Move Aligned Packed SP-FP Values (SSE).
  inline void movaps(const XmmVar& dst, const Mem& src)
  { _emitInstruction(kX86InstMovAPS, &dst, &src); }

  //! @brief Move Aligned Packed SP-FP Values (SSE).
  inline void movaps(const Mem& dst, const XmmVar& src)
  { _emitInstruction(kX86InstMovAPS, &dst, &src); }

  //! @brief Move DWord.
  inline void movd(const Mem& dst, const XmmVar& src)
  { _emitInstruction(kX86InstMovD, &dst, &src); }
  //! @brief Move DWord.
  inline void movd(const GpVar& dst, const XmmVar& src)
  { _emitInstruction(kX86InstMovD, &dst, &src); }
  //! @brief Move DWord.
  inline void movd(const XmmVar& dst, const Mem& src)
  { _emitInstruction(kX86InstMovD, &dst, &src); }
  //! @brief Move DWord.
  inline void movd(const XmmVar& dst, const GpVar& src)
  { _emitInstruction(kX86InstMovD, &dst, &src); }

  //! @brief Move QWord (SSE).
  inline void movq(const XmmVar& dst, const XmmVar& src)
  { _emitInstruction(kX86InstMovQ, &dst, &src); }
  //! @brief Move QWord (SSE).
  inline void movq(const Mem& dst, const XmmVar& src)
  { _emitInstruction(kX86InstMovQ, &dst, &src); }
#if defined(ASMJIT_X64)
  //! @brief Move QWord (SSE).
  inline void movq(const GpVar& dst, const XmmVar& src)
  { _emitInstruction(kX86InstMovQ, &dst, &src); }
#endif // ASMJIT_X64
  //! @brief Move QWord (SSE).
  inline void movq(const XmmVar& dst, const Mem& src)
  { _emitInstruction(kX86InstMovQ, &dst, &src); }
#if defined(ASMJIT_X64)
  //! @brief Move QWord (SSE).
  inline void movq(const XmmVar& dst, const GpVar& src)
  { _emitInstruction(kX86InstMovQ, &dst, &src); }
#endif // ASMJIT_X64

  //! @brief Move 64 Bits Non Temporal (SSE).
  inline void movntq(const Mem& dst, const MmVar& src)
  { _emitInstruction(kX86InstMovNTQ, &dst, &src); }

  //! @brief High to Low Packed SP-FP (SSE).
  inline void movhlps(const XmmVar& dst, const XmmVar& src)
  { _emitInstruction(kX86InstMovHLPS, &dst, &src); }

  //! @brief Move High Packed SP-FP (SSE).
  inline void movhps(const XmmVar& dst, const Mem& src)
  { _emitInstruction(kX86InstMovHPS, &dst, &src); }

  //! @brief Move High Packed SP-FP (SSE).
  inline void movhps(const Mem& dst, const XmmVar& src)
  { _emitInstruction(kX86InstMovHPS, &dst, &src); }

  //! @brief Move Low to High Packed SP-FP (SSE).
  inline void movlhps(const XmmVar& dst, const XmmVar& src)
  { _emitInstruction(kX86InstMovLHPS, &dst, &src); }

  //! @brief Move Low Packed SP-FP (SSE).
  inline void movlps(const XmmVar& dst, const Mem& src)
  { _emitInstruction(kX86InstMovLPS, &dst, &src); }

  //! @brief Move Low Packed SP-FP (SSE).
  inline void movlps(const Mem& dst, const XmmVar& src)
  { _emitInstruction(kX86InstMovLPS, &dst, &src); }

  //! @brief Move Aligned Four Packed SP-FP Non Temporal (SSE).
  inline void movntps(const Mem& dst, const XmmVar& src)
  { _emitInstruction(kX86InstMovNTPS, &dst, &src); }

  //! @brief Move Scalar SP-FP (SSE).
  inline void movss(const XmmVar& dst, const XmmVar& src)
  { _emitInstruction(kX86InstMovSS, &dst, &src); }

  //! @brief Move Scalar SP-FP (SSE).
  inline void movss(const XmmVar& dst, const Mem& src)
  { _emitInstruction(kX86InstMovSS, &dst, &src); }

  //! @brief Move Scalar SP-FP (SSE).
  inline void movss(const Mem& dst, const XmmVar& src)
  { _emitInstruction(kX86InstMovSS, &dst, &src); }

  //! @brief Move Unaligned Packed SP-FP Values (SSE).
  inline void movups(const XmmVar& dst, const XmmVar& src)
  { _emitInstruction(kX86InstMovUPS, &dst, &src); }
  //! @brief Move Unaligned Packed SP-FP Values (SSE).
  inline void movups(const XmmVar& dst, const Mem& src)
  { _emitInstruction(kX86InstMovUPS, &dst, &src); }

  //! @brief Move Unaligned Packed SP-FP Values (SSE).
  inline void movups(const Mem& dst, const XmmVar& src)
  { _emitInstruction(kX86InstMovUPS, &dst, &src); }

  //! @brief Packed SP-FP Multiply (SSE).
  inline void mulps(const XmmVar& dst, const XmmVar& src)
  { _emitInstruction(kX86InstMulPS, &dst, &src); }
  //! @brief Packed SP-FP Multiply (SSE).
  inline void mulps(const XmmVar& dst, const Mem& src)
  { _emitInstruction(kX86InstMulPS, &dst, &src); }

  //! @brief Scalar SP-FP Multiply (SSE).
  inline void mulss(const XmmVar& dst, const XmmVar& src)
  { _emitInstruction(kX86InstMulSS, &dst, &src); }
  //! @brief Scalar SP-FP Multiply (SSE).
  inline void mulss(const XmmVar& dst, const Mem& src)
  { _emitInstruction(kX86InstMulSS, &dst, &src); }

  //! @brief Bit-wise Logical OR for SP-FP Data (SSE).
  inline void orps(const XmmVar& dst, const XmmVar& src)
  { _emitInstruction(kX86InstOrPS, &dst, &src); }
  //! @brief Bit-wise Logical OR for SP-FP Data (SSE).
  inline void orps(const XmmVar& dst, const Mem& src)
  { _emitInstruction(kX86InstOrPS, &dst, &src); }

  //! @brief Packed Average (SSE).
  inline void pavgb(const MmVar& dst, const MmVar& src)
  { _emitInstruction(kX86InstPAvgB, &dst, &src); }
  //! @brief Packed Average (SSE).
  inline void pavgb(const MmVar& dst, const Mem& src)
  { _emitInstruction(kX86InstPAvgB, &dst, &src); }

  //! @brief Packed Average (SSE).
  inline void pavgw(const MmVar& dst, const MmVar& src)
  { _emitInstruction(kX86InstPAvgW, &dst, &src); }
  //! @brief Packed Average (SSE).
  inline void pavgw(const MmVar& dst, const Mem& src)
  { _emitInstruction(kX86InstPAvgW, &dst, &src); }

  //! @brief Extract Word (SSE).
  inline void pextrw(const GpVar& dst, const MmVar& src, const Imm& imm8)
  { _emitInstruction(kX86InstPExtrW, &dst, &src, &imm8); }

  //! @brief Insert Word (SSE).
  inline void pinsrw(const MmVar& dst, const GpVar& src, const Imm& imm8)
  { _emitInstruction(kX86InstPInsRW, &dst, &src, &imm8); }
  //! @brief Insert Word (SSE).
  inline void pinsrw(const MmVar& dst, const Mem& src, const Imm& imm8)
  { _emitInstruction(kX86InstPInsRW, &dst, &src, &imm8); }

  //! @brief Packed Signed Integer Word Maximum (SSE).
  inline void pmaxsw(const MmVar& dst, const MmVar& src)
  { _emitInstruction(kX86InstPMaxSW, &dst, &src); }
  //! @brief Packed Signed Integer Word Maximum (SSE).
  inline void pmaxsw(const MmVar& dst, const Mem& src)
  { _emitInstruction(kX86InstPMaxSW, &dst, &src); }

  //! @brief Packed Unsigned Integer Byte Maximum (SSE).
  inline void pmaxub(const MmVar& dst, const MmVar& src)
  { _emitInstruction(kX86InstPMaxUB, &dst, &src); }
  //! @brief Packed Unsigned Integer Byte Maximum (SSE).
  inline void pmaxub(const MmVar& dst, const Mem& src)
  { _emitInstruction(kX86InstPMaxUB, &dst, &src); }

  //! @brief Packed Signed Integer Word Minimum (SSE).
  inline void pminsw(const MmVar& dst, const MmVar& src)
  { _emitInstruction(kX86InstPMinSW, &dst, &src); }
  //! @brief Packed Signed Integer Word Minimum (SSE).
  inline void pminsw(const MmVar& dst, const Mem& src)
  { _emitInstruction(kX86InstPMinSW, &dst, &src); }

  //! @brief Packed Unsigned Integer Byte Minimum (SSE).
  inline void pminub(const MmVar& dst, const MmVar& src)
  { _emitInstruction(kX86InstPMinUB, &dst, &src); }
  //! @brief Packed Unsigned Integer Byte Minimum (SSE).
  inline void pminub(const MmVar& dst, const Mem& src)
  { _emitInstruction(kX86InstPMinUB, &dst, &src); }

  //! @brief Move Byte Mask To Integer (SSE).
  inline void pmovmskb(const GpVar& dst, const MmVar& src)
  { _emitInstruction(kX86InstPMovMskB, &dst, &src); }

  //! @brief Packed Multiply High Unsigned (SSE).
  inline void pmulhuw(const MmVar& dst, const MmVar& src)
  { _emitInstruction(kX86InstPMulHUW, &dst, &src); }
  //! @brief Packed Multiply High Unsigned (SSE).
  inline void pmulhuw(const MmVar& dst, const Mem& src)
  { _emitInstruction(kX86InstPMulHUW, &dst, &src); }

  //! @brief Packed Sum of Absolute Differences (SSE).
  inline void psadbw(const MmVar& dst, const MmVar& src)
  { _emitInstruction(kX86InstPSADBW, &dst, &src); }
  //! @brief Packed Sum of Absolute Differences (SSE).
  inline void psadbw(const MmVar& dst, const Mem& src)
  { _emitInstruction(kX86InstPSADBW, &dst, &src); }

  //! @brief Packed Shuffle word (SSE).
  inline void pshufw(const MmVar& dst, const MmVar& src, const Imm& imm8)
  { _emitInstruction(kX86InstPShufW, &dst, &src, &imm8); }
  //! @brief Packed Shuffle word (SSE).
  inline void pshufw(const MmVar& dst, const Mem& src, const Imm& imm8)
  { _emitInstruction(kX86InstPShufW, &dst, &src, &imm8); }

  //! @brief Packed SP-FP Reciprocal (SSE).
  inline void rcpps(const XmmVar& dst, const XmmVar& src)
  { _emitInstruction(kX86InstRcpPS, &dst, &src); }
  //! @brief Packed SP-FP Reciprocal (SSE).
  inline void rcpps(const XmmVar& dst, const Mem& src)
  { _emitInstruction(kX86InstRcpPS, &dst, &src); }

  //! @brief Scalar SP-FP Reciprocal (SSE).
  inline void rcpss(const XmmVar& dst, const XmmVar& src)
  { _emitInstruction(kX86InstRcpSS, &dst, &src); }
  //! @brief Scalar SP-FP Reciprocal (SSE).
  inline void rcpss(const XmmVar& dst, const Mem& src)
  { _emitInstruction(kX86InstRcpSS, &dst, &src); }

  //! @brief Prefetch (SSE).
  inline void prefetch(const Mem& mem, const Imm& hint)
  { _emitInstruction(kX86InstPrefetch, &mem, &hint); }

  //! @brief Compute Sum of Absolute Differences (SSE).
  inline void psadbw(const XmmVar& dst, const XmmVar& src)
  { _emitInstruction(kX86InstPSADBW, &dst, &src); }
  //! @brief Compute Sum of Absolute Differences (SSE).
  inline void psadbw(const XmmVar& dst, const Mem& src)
  { _emitInstruction(kX86InstPSADBW, &dst, &src); }

  //! @brief Packed SP-FP Square Root Reciprocal (SSE).
  inline void rsqrtps(const XmmVar& dst, const XmmVar& src)
  { _emitInstruction(kX86InstSqrtPS, &dst, &src); }
  //! @brief Packed SP-FP Square Root Reciprocal (SSE).
  inline void rsqrtps(const XmmVar& dst, const Mem& src)
  { _emitInstruction(kX86InstSqrtPS, &dst, &src); }

  //! @brief Scalar SP-FP Square Root Reciprocal (SSE).
  inline void rsqrtss(const XmmVar& dst, const XmmVar& src)
  { _emitInstruction(kX86InstSqrtSS, &dst, &src); }
  //! @brief Scalar SP-FP Square Root Reciprocal (SSE).
  inline void rsqrtss(const XmmVar& dst, const Mem& src)
  { _emitInstruction(kX86InstSqrtSS, &dst, &src); }

  //! @brief Store fence (SSE).
  inline void sfence()
  { _emitInstruction(kX86InstSFence); }

  //! @brief Shuffle SP-FP (SSE).
  inline void shufps(const XmmVar& dst, const XmmVar& src, const Imm& imm8)
  { _emitInstruction(kX86InstShufPS, &dst, &src, &imm8); }
  //! @brief Shuffle SP-FP (SSE).
  inline void shufps(const XmmVar& dst, const Mem& src, const Imm& imm8)
  { _emitInstruction(kX86InstShufPS, &dst, &src, &imm8); }

  //! @brief Packed SP-FP Square Root (SSE).
  inline void sqrtps(const XmmVar& dst, const XmmVar& src)
  { _emitInstruction(kX86InstSqrtPS, &dst, &src); }
  //! @brief Packed SP-FP Square Root (SSE).
  inline void sqrtps(const XmmVar& dst, const Mem& src)
  { _emitInstruction(kX86InstSqrtPS, &dst, &src); }

  //! @brief Scalar SP-FP Square Root (SSE).
  inline void sqrtss(const XmmVar& dst, const XmmVar& src)
  { _emitInstruction(kX86InstSqrtSS, &dst, &src); }
  //! @brief Scalar SP-FP Square Root (SSE).
  inline void sqrtss(const XmmVar& dst, const Mem& src)
  { _emitInstruction(kX86InstSqrtSS, &dst, &src); }

  //! @brief Store Streaming SIMD Extension Control/Status (SSE).
  inline void stmxcsr(const Mem& dst)
  { _emitInstruction(kX86InstStMXCSR, &dst); }

  //! @brief Packed SP-FP Subtract (SSE).
  inline void subps(const XmmVar& dst, const XmmVar& src)
  { _emitInstruction(kX86InstSubPS, &dst, &src); }
  //! @brief Packed SP-FP Subtract (SSE).
  inline void subps(const XmmVar& dst, const Mem& src)
  { _emitInstruction(kX86InstSubPS, &dst, &src); }

  //! @brief Scalar SP-FP Subtract (SSE).
  inline void subss(const XmmVar& dst, const XmmVar& src)
  { _emitInstruction(kX86InstSubSS, &dst, &src); }
  //! @brief Scalar SP-FP Subtract (SSE).
  inline void subss(const XmmVar& dst, const Mem& src)
  { _emitInstruction(kX86InstSubSS, &dst, &src); }

  //! @brief Unordered Scalar SP-FP compare and set EFLAGS (SSE).
  inline void ucomiss(const XmmVar& dst, const XmmVar& src)
  { _emitInstruction(kX86InstUComISS, &dst, &src); }
  //! @brief Unordered Scalar SP-FP compare and set EFLAGS (SSE).
  inline void ucomiss(const XmmVar& dst, const Mem& src)
  { _emitInstruction(kX86InstUComISS, &dst, &src); }

  //! @brief Unpack High Packed SP-FP Data (SSE).
  inline void unpckhps(const XmmVar& dst, const XmmVar& src)
  { _emitInstruction(kX86InstUnpckHPS, &dst, &src); }
  //! @brief Unpack High Packed SP-FP Data (SSE).
  inline void unpckhps(const XmmVar& dst, const Mem& src)
  { _emitInstruction(kX86InstUnpckHPS, &dst, &src); }

  //! @brief Unpack Low Packed SP-FP Data (SSE).
  inline void unpcklps(const XmmVar& dst, const XmmVar& src)
  { _emitInstruction(kX86InstUnpckLPS, &dst, &src); }
  //! @brief Unpack Low Packed SP-FP Data (SSE).
  inline void unpcklps(const XmmVar& dst, const Mem& src)
  { _emitInstruction(kX86InstUnpckLPS, &dst, &src); }

  //! @brief Bit-wise Logical Xor for SP-FP Data (SSE).
  inline void xorps(const XmmVar& dst, const XmmVar& src)
  { _emitInstruction(kX86InstXorPS, &dst, &src); }
  //! @brief Bit-wise Logical Xor for SP-FP Data (SSE).
  inline void xorps(const XmmVar& dst, const Mem& src)
  { _emitInstruction(kX86InstXorPS, &dst, &src); }

  // --------------------------------------------------------------------------
  // [SSE2]
  // --------------------------------------------------------------------------

  //! @brief Packed DP-FP Add (SSE2).
  inline void addpd(const XmmVar& dst, const XmmVar& src)
  { _emitInstruction(kX86InstAddPD, &dst, &src); }
  //! @brief Packed DP-FP Add (SSE2).
  inline void addpd(const XmmVar& dst, const Mem& src)
  { _emitInstruction(kX86InstAddPD, &dst, &src); }

  //! @brief Scalar DP-FP Add (SSE2).
  inline void addsd(const XmmVar& dst, const XmmVar& src)
  { _emitInstruction(kX86InstAddSD, &dst, &src); }
  //! @brief Scalar DP-FP Add (SSE2).
  inline void addsd(const XmmVar& dst, const Mem& src)
  { _emitInstruction(kX86InstAddSD, &dst, &src); }

  //! @brief Bit-wise Logical And Not For DP-FP (SSE2).
  inline void andnpd(const XmmVar& dst, const XmmVar& src)
  { _emitInstruction(kX86InstAndnPD, &dst, &src); }
  //! @brief Bit-wise Logical And Not For DP-FP (SSE2).
  inline void andnpd(const XmmVar& dst, const Mem& src)
  { _emitInstruction(kX86InstAndnPD, &dst, &src); }

  //! @brief Bit-wise Logical And For DP-FP (SSE2).
  inline void andpd(const XmmVar& dst, const XmmVar& src)
  { _emitInstruction(kX86InstAndPD, &dst, &src); }
  //! @brief Bit-wise Logical And For DP-FP (SSE2).
  inline void andpd(const XmmVar& dst, const Mem& src)
  { _emitInstruction(kX86InstAndPD, &dst, &src); }

  //! @brief Flush Cache Line (SSE2).
  inline void clflush(const Mem& mem)
  { _emitInstruction(kX86InstClFlush, &mem); }

  //! @brief Packed DP-FP Compare (SSE2).
  inline void cmppd(const XmmVar& dst, const XmmVar& src, const Imm& imm8)
  { _emitInstruction(kX86InstCmpPD, &dst, &src, &imm8); }
  //! @brief Packed DP-FP Compare (SSE2).
  inline void cmppd(const XmmVar& dst, const Mem& src, const Imm& imm8)
  { _emitInstruction(kX86InstCmpPD, &dst, &src, &imm8); }

  //! @brief Compare Scalar SP-FP Values (SSE2).
  inline void cmpsd(const XmmVar& dst, const XmmVar& src, const Imm& imm8)
  { _emitInstruction(kX86InstCmpSD, &dst, &src, &imm8); }
  //! @brief Compare Scalar SP-FP Values (SSE2).
  inline void cmpsd(const XmmVar& dst, const Mem& src, const Imm& imm8)
  { _emitInstruction(kX86InstCmpSD, &dst, &src, &imm8); }

  //! @brief Scalar Ordered DP-FP Compare and Set EFLAGS (SSE2).
  inline void comisd(const XmmVar& dst, const XmmVar& src)
  { _emitInstruction(kX86InstComISD, &dst, &src); }
  //! @brief Scalar Ordered DP-FP Compare and Set EFLAGS (SSE2).
  inline void comisd(const XmmVar& dst, const Mem& src)
  { _emitInstruction(kX86InstComISD, &dst, &src); }

  //! @brief Convert Packed Dword Integers to Packed DP-FP Values (SSE2).
  inline void cvtdq2pd(const XmmVar& dst, const XmmVar& src)
  { _emitInstruction(kX86InstCvtDQ2PD, &dst, &src); }
  //! @brief Convert Packed Dword Integers to Packed DP-FP Values (SSE2).
  inline void cvtdq2pd(const XmmVar& dst, const Mem& src)
  { _emitInstruction(kX86InstCvtDQ2PD, &dst, &src); }

  //! @brief Convert Packed Dword Integers to Packed SP-FP Values (SSE2).
  inline void cvtdq2ps(const XmmVar& dst, const XmmVar& src)
  { _emitInstruction(kX86InstCvtDQ2PS, &dst, &src); }
  //! @brief Convert Packed Dword Integers to Packed SP-FP Values (SSE2).
  inline void cvtdq2ps(const XmmVar& dst, const Mem& src)
  { _emitInstruction(kX86InstCvtDQ2PS, &dst, &src); }

  //! @brief Convert Packed DP-FP Values to Packed Dword Integers (SSE2).
  inline void cvtpd2dq(const XmmVar& dst, const XmmVar& src)
  { _emitInstruction(kX86InstCvtPD2DQ, &dst, &src); }
  //! @brief Convert Packed DP-FP Values to Packed Dword Integers (SSE2).
  inline void cvtpd2dq(const XmmVar& dst, const Mem& src)
  { _emitInstruction(kX86InstCvtPD2DQ, &dst, &src); }

  //! @brief Convert Packed DP-FP Values to Packed Dword Integers (SSE2).
  inline void cvtpd2pi(const MmVar& dst, const XmmVar& src)
  { _emitInstruction(kX86InstCvtPD2PI, &dst, &src); }
  //! @brief Convert Packed DP-FP Values to Packed Dword Integers (SSE2).
  inline void cvtpd2pi(const MmVar& dst, const Mem& src)
  { _emitInstruction(kX86InstCvtPD2PI, &dst, &src); }

  //! @brief Convert Packed DP-FP Values to Packed SP-FP Values (SSE2).
  inline void cvtpd2ps(const XmmVar& dst, const XmmVar& src)
  { _emitInstruction(kX86InstCvtPD2PS, &dst, &src); }
  //! @brief Convert Packed DP-FP Values to Packed SP-FP Values (SSE2).
  inline void cvtpd2ps(const XmmVar& dst, const Mem& src)
  { _emitInstruction(kX86InstCvtPD2PS, &dst, &src); }

  //! @brief Convert Packed Dword Integers to Packed DP-FP Values (SSE2).
  inline void cvtpi2pd(const XmmVar& dst, const MmVar& src)
  { _emitInstruction(kX86InstCvtPI2PD, &dst, &src); }
  //! @brief Convert Packed Dword Integers to Packed DP-FP Values (SSE2).
  inline void cvtpi2pd(const XmmVar& dst, const Mem& src)
  { _emitInstruction(kX86InstCvtPI2PD, &dst, &src); }

  //! @brief Convert Packed SP-FP Values to Packed Dword Integers (SSE2).
  inline void cvtps2dq(const XmmVar& dst, const XmmVar& src)
  { _emitInstruction(kX86InstCvtPS2DQ, &dst, &src); }
  //! @brief Convert Packed SP-FP Values to Packed Dword Integers (SSE2).
  inline void cvtps2dq(const XmmVar& dst, const Mem& src)
  { _emitInstruction(kX86InstCvtPS2DQ, &dst, &src); }

  //! @brief Convert Packed SP-FP Values to Packed DP-FP Values (SSE2).
  inline void cvtps2pd(const XmmVar& dst, const XmmVar& src)
  { _emitInstruction(kX86InstCvtPS2PD, &dst, &src); }
  //! @brief Convert Packed SP-FP Values to Packed DP-FP Values (SSE2).
  inline void cvtps2pd(const XmmVar& dst, const Mem& src)
  { _emitInstruction(kX86InstCvtPS2PD, &dst, &src); }

  //! @brief Convert Scalar DP-FP Value to Dword Integer (SSE2).
  inline void cvtsd2si(const GpVar& dst, const XmmVar& src)
  { _emitInstruction(kX86InstCvtSD2SI, &dst, &src); }
  //! @brief Convert Scalar DP-FP Value to Dword Integer (SSE2).
  inline void cvtsd2si(const GpVar& dst, const Mem& src)
  { _emitInstruction(kX86InstCvtSD2SI, &dst, &src); }

  //! @brief Convert Scalar DP-FP Value to Scalar SP-FP Value (SSE2).
  inline void cvtsd2ss(const XmmVar& dst, const XmmVar& src)
  { _emitInstruction(kX86InstCvtSD2SS, &dst, &src); }
  //! @brief Convert Scalar DP-FP Value to Scalar SP-FP Value (SSE2).
  inline void cvtsd2ss(const XmmVar& dst, const Mem& src)
  { _emitInstruction(kX86InstCvtSD2SS, &dst, &src); }

  //! @brief Convert Dword Integer to Scalar DP-FP Value (SSE2).
  inline void cvtsi2sd(const XmmVar& dst, const GpVar& src)
  { _emitInstruction(kX86InstCvtSI2SD, &dst, &src); }
  //! @brief Convert Dword Integer to Scalar DP-FP Value (SSE2).
  inline void cvtsi2sd(const XmmVar& dst, const Mem& src)
  { _emitInstruction(kX86InstCvtSI2SD, &dst, &src); }

  //! @brief Convert Scalar SP-FP Value to Scalar DP-FP Value (SSE2).
  inline void cvtss2sd(const XmmVar& dst, const XmmVar& src)
  { _emitInstruction(kX86InstCvtSS2SD, &dst, &src); }
  //! @brief Convert Scalar SP-FP Value to Scalar DP-FP Value (SSE2).
  inline void cvtss2sd(const XmmVar& dst, const Mem& src)
  { _emitInstruction(kX86InstCvtSS2SD, &dst, &src); }

  //! @brief Convert with Truncation Packed DP-FP Values to Packed Dword Integers (SSE2).
  inline void cvttpd2pi(const MmVar& dst, const XmmVar& src)
  { _emitInstruction(kX86InstCvttPD2PI, &dst, &src); }
  //! @brief Convert with Truncation Packed DP-FP Values to Packed Dword Integers (SSE2).
  inline void cvttpd2pi(const MmVar& dst, const Mem& src)
  { _emitInstruction(kX86InstCvttPD2PI, &dst, &src); }

  //! @brief Convert with Truncation Packed DP-FP Values to Packed Dword Integers (SSE2).
  inline void cvttpd2dq(const XmmVar& dst, const XmmVar& src)
  { _emitInstruction(kX86InstCvttPD2DQ, &dst, &src); }
  //! @brief Convert with Truncation Packed DP-FP Values to Packed Dword Integers (SSE2).
  inline void cvttpd2dq(const XmmVar& dst, const Mem& src)
  { _emitInstruction(kX86InstCvttPD2DQ, &dst, &src); }

  //! @brief Convert with Truncation Packed SP-FP Values to Packed Dword Integers (SSE2).
  inline void cvttps2dq(const XmmVar& dst, const XmmVar& src)
  { _emitInstruction(kX86InstCvttPS2DQ, &dst, &src); }
  //! @brief Convert with Truncation Packed SP-FP Values to Packed Dword Integers (SSE2).
  inline void cvttps2dq(const XmmVar& dst, const Mem& src)
  { _emitInstruction(kX86InstCvttPS2DQ, &dst, &src); }

  //! @brief Convert with Truncation Scalar DP-FP Value to Signed Dword Integer (SSE2).
  inline void cvttsd2si(const GpVar& dst, const XmmVar& src)
  { _emitInstruction(kX86InstCvttSD2SI, &dst, &src); }
  //! @brief Convert with Truncation Scalar DP-FP Value to Signed Dword Integer (SSE2).
  inline void cvttsd2si(const GpVar& dst, const Mem& src)
  { _emitInstruction(kX86InstCvttSD2SI, &dst, &src); }

  //! @brief Packed DP-FP Divide (SSE2).
  inline void divpd(const XmmVar& dst, const XmmVar& src)
  { _emitInstruction(kX86InstDivPD, &dst, &src); }
  //! @brief Packed DP-FP Divide (SSE2).
  inline void divpd(const XmmVar& dst, const Mem& src)
  { _emitInstruction(kX86InstDivPD, &dst, &src); }

  //! @brief Scalar DP-FP Divide (SSE2).
  inline void divsd(const XmmVar& dst, const XmmVar& src)
  { _emitInstruction(kX86InstDivSD, &dst, &src); }
  //! @brief Scalar DP-FP Divide (SSE2).
  inline void divsd(const XmmVar& dst, const Mem& src)
  { _emitInstruction(kX86InstDivSD, &dst, &src); }

  //! @brief Load Fence (SSE2).
  inline void lfence()
  { _emitInstruction(kX86InstLFence); }

  //! @brief Store Selected Bytes of Double Quadword (SSE2).
  //!
  //! @note Target is DS:EDI.
  inline void maskmovdqu(const GpVar& dst_ptr, const XmmVar& src, const XmmVar& mask)
  { _emitInstruction(kX86InstMaskMovDQU, &dst_ptr, &src, &mask); }

  //! @brief Return Maximum Packed Double-Precision FP Values (SSE2).
  inline void maxpd(const XmmVar& dst, const XmmVar& src)
  { _emitInstruction(kX86InstMaxPD, &dst, &src); }
  //! @brief Return Maximum Packed Double-Precision FP Values (SSE2).
  inline void maxpd(const XmmVar& dst, const Mem& src)
  { _emitInstruction(kX86InstMaxPD, &dst, &src); }

  //! @brief Return Maximum Scalar Double-Precision FP Value (SSE2).
  inline void maxsd(const XmmVar& dst, const XmmVar& src)
  { _emitInstruction(kX86InstMaxSD, &dst, &src); }
  //! @brief Return Maximum Scalar Double-Precision FP Value (SSE2).
  inline void maxsd(const XmmVar& dst, const Mem& src)
  { _emitInstruction(kX86InstMaxSD, &dst, &src); }

  //! @brief Memory Fence (SSE2).
  inline void mfence()
  { _emitInstruction(kX86InstMFence); }

  //! @brief Return Minimum Packed DP-FP Values (SSE2).
  inline void minpd(const XmmVar& dst, const XmmVar& src)
  { _emitInstruction(kX86InstMinPD, &dst, &src); }
  //! @brief Return Minimum Packed DP-FP Values (SSE2).
  inline void minpd(const XmmVar& dst, const Mem& src)
  { _emitInstruction(kX86InstMinPD, &dst, &src); }

  //! @brief Return Minimum Scalar DP-FP Value (SSE2).
  inline void minsd(const XmmVar& dst, const XmmVar& src)
  { _emitInstruction(kX86InstMinSD, &dst, &src); }
  //! @brief Return Minimum Scalar DP-FP Value (SSE2).
  inline void minsd(const XmmVar& dst, const Mem& src)
  { _emitInstruction(kX86InstMinSD, &dst, &src); }

  //! @brief Move Aligned DQWord (SSE2).
  inline void movdqa(const XmmVar& dst, const XmmVar& src)
  { _emitInstruction(kX86InstMovDQA, &dst, &src); }
  //! @brief Move Aligned DQWord (SSE2).
  inline void movdqa(const XmmVar& dst, const Mem& src)
  { _emitInstruction(kX86InstMovDQA, &dst, &src); }

  //! @brief Move Aligned DQWord (SSE2).
  inline void movdqa(const Mem& dst, const XmmVar& src)
  { _emitInstruction(kX86InstMovDQA, &dst, &src); }

  //! @brief Move Unaligned Double Quadword (SSE2).
  inline void movdqu(const XmmVar& dst, const XmmVar& src)
  { _emitInstruction(kX86InstMovDQU, &dst, &src); }
  //! @brief Move Unaligned Double Quadword (SSE2).
  inline void movdqu(const XmmVar& dst, const Mem& src)
  { _emitInstruction(kX86InstMovDQU, &dst, &src); }

  //! @brief Move Unaligned Double Quadword (SSE2).
  inline void movdqu(const Mem& dst, const XmmVar& src)
  { _emitInstruction(kX86InstMovDQU, &dst, &src); }

  //! @brief Extract Packed SP-FP Sign Mask (SSE2).
  inline void movmskps(const GpVar& dst, const XmmVar& src)
  { _emitInstruction(kX86InstMovMskPS, &dst, &src); }

  //! @brief Extract Packed DP-FP Sign Mask (SSE2).
  inline void movmskpd(const GpVar& dst, const XmmVar& src)
  { _emitInstruction(kX86InstMovMskPD, &dst, &src); }

  //! @brief Move Scalar Double-Precision FP Value (SSE2).
  inline void movsd(const XmmVar& dst, const XmmVar& src)
  { _emitInstruction(kX86InstMovSD, &dst, &src); }
  //! @brief Move Scalar Double-Precision FP Value (SSE2).
  inline void movsd(const XmmVar& dst, const Mem& src)
  { _emitInstruction(kX86InstMovSD, &dst, &src); }

  //! @brief Move Scalar Double-Precision FP Value (SSE2).
  inline void movsd(const Mem& dst, const XmmVar& src)
  { _emitInstruction(kX86InstMovSD, &dst, &src); }

  //! @brief Move Aligned Packed Double-Precision FP Values (SSE2).
  inline void movapd(const XmmVar& dst, const XmmVar& src)
  { _emitInstruction(kX86InstMovAPD, &dst, &src); }

  //! @brief Move Aligned Packed Double-Precision FP Values (SSE2).
  inline void movapd(const XmmVar& dst, const Mem& src)
  { _emitInstruction(kX86InstMovAPD, &dst, &src); }

  //! @brief Move Aligned Packed Double-Precision FP Values (SSE2).
  inline void movapd(const Mem& dst, const XmmVar& src)
  { _emitInstruction(kX86InstMovAPD, &dst, &src); }

  //! @brief Move Quadword from XMM to MMX Technology Register (SSE2).
  inline void movdq2q(const MmVar& dst, const XmmVar& src)
  { _emitInstruction(kX86InstMovDQ2Q, &dst, &src); }

  //! @brief Move Quadword from MMX Technology to XMM Register (SSE2).
  inline void movq2dq(const XmmVar& dst, const MmVar& src)
  { _emitInstruction(kX86InstMovQ2DQ, &dst, &src); }

  //! @brief Move High Packed Double-Precision FP Value (SSE2).
  inline void movhpd(const XmmVar& dst, const Mem& src)
  { _emitInstruction(kX86InstMovHPD, &dst, &src); }

  //! @brief Move High Packed Double-Precision FP Value (SSE2).
  inline void movhpd(const Mem& dst, const XmmVar& src)
  { _emitInstruction(kX86InstMovHPD, &dst, &src); }

  //! @brief Move Low Packed Double-Precision FP Value (SSE2).
  inline void movlpd(const XmmVar& dst, const Mem& src)
  { _emitInstruction(kX86InstMovLPD, &dst, &src); }

  //! @brief Move Low Packed Double-Precision FP Value (SSE2).
  inline void movlpd(const Mem& dst, const XmmVar& src)
  { _emitInstruction(kX86InstMovLPD, &dst, &src); }

  //! @brief Store Double Quadword Using Non-Temporal Hint (SSE2).
  inline void movntdq(const Mem& dst, const XmmVar& src)
  { _emitInstruction(kX86InstMovNTDQ, &dst, &src); }

  //! @brief Store Store DWORD Using Non-Temporal Hint (SSE2).
  inline void movnti(const Mem& dst, const GpVar& src)
  { _emitInstruction(kX86InstMovNTI, &dst, &src); }

  //! @brief Store Packed Double-Precision FP Values Using Non-Temporal Hint (SSE2).
  inline void movntpd(const Mem& dst, const XmmVar& src)
  { _emitInstruction(kX86InstMovNTPD, &dst, &src); }

  //! @brief Move Unaligned Packed Double-Precision FP Values (SSE2).
  inline void movupd(const XmmVar& dst, const XmmVar& src)
  { _emitInstruction(kX86InstMovUPD, &dst, &src); }

  //! @brief Move Unaligned Packed Double-Precision FP Values (SSE2).
  inline void movupd(const XmmVar& dst, const Mem& src)
  { _emitInstruction(kX86InstMovUPD, &dst, &src); }

  //! @brief Move Unaligned Packed Double-Precision FP Values (SSE2).
  inline void movupd(const Mem& dst, const XmmVar& src)
  { _emitInstruction(kX86InstMovUPD, &dst, &src); }

  //! @brief Packed DP-FP Multiply (SSE2).
  inline void mulpd(const XmmVar& dst, const XmmVar& src)
  { _emitInstruction(kX86InstMulPD, &dst, &src); }
  //! @brief Packed DP-FP Multiply (SSE2).
  inline void mulpd(const XmmVar& dst, const Mem& src)
  { _emitInstruction(kX86InstMulPD, &dst, &src); }

  //! @brief Scalar DP-FP Multiply (SSE2).
  inline void mulsd(const XmmVar& dst, const XmmVar& src)
  { _emitInstruction(kX86InstMulSD, &dst, &src); }
  //! @brief Scalar DP-FP Multiply (SSE2).
  inline void mulsd(const XmmVar& dst, const Mem& src)
  { _emitInstruction(kX86InstMulSD, &dst, &src); }

  //! @brief Bit-wise Logical OR for DP-FP Data (SSE2).
  inline void orpd(const XmmVar& dst, const XmmVar& src)
  { _emitInstruction(kX86InstOrPD, &dst, &src); }
  //! @brief Bit-wise Logical OR for DP-FP Data (SSE2).
  inline void orpd(const XmmVar& dst, const Mem& src)
  { _emitInstruction(kX86InstOrPD, &dst, &src); }

  //! @brief Pack with Signed Saturation (SSE2).
  inline void packsswb(const XmmVar& dst, const XmmVar& src)
  { _emitInstruction(kX86InstPackSSWB, &dst, &src); }
  //! @brief Pack with Signed Saturation (SSE2).
  inline void packsswb(const XmmVar& dst, const Mem& src)
  { _emitInstruction(kX86InstPackSSWB, &dst, &src); }

  //! @brief Pack with Signed Saturation (SSE2).
  inline void packssdw(const XmmVar& dst, const XmmVar& src)
  { _emitInstruction(kX86InstPackSSDW, &dst, &src); }
  //! @brief Pack with Signed Saturation (SSE2).
  inline void packssdw(const XmmVar& dst, const Mem& src)
  { _emitInstruction(kX86InstPackSSDW, &dst, &src); }

  //! @brief Pack with Unsigned Saturation (SSE2).
  inline void packuswb(const XmmVar& dst, const XmmVar& src)
  { _emitInstruction(kX86InstPackUSWB, &dst, &src); }
  //! @brief Pack with Unsigned Saturation (SSE2).
  inline void packuswb(const XmmVar& dst, const Mem& src)
  { _emitInstruction(kX86InstPackUSWB, &dst, &src); }

  //! @brief Packed BYTE Add (SSE2).
  inline void paddb(const XmmVar& dst, const XmmVar& src)
  { _emitInstruction(kX86InstPAddB, &dst, &src); }
  //! @brief Packed BYTE Add (SSE2).
  inline void paddb(const XmmVar& dst, const Mem& src)
  { _emitInstruction(kX86InstPAddB, &dst, &src); }

  //! @brief Packed WORD Add (SSE2).
  inline void paddw(const XmmVar& dst, const XmmVar& src)
  { _emitInstruction(kX86InstPAddW, &dst, &src); }
  //! @brief Packed WORD Add (SSE2).
  inline void paddw(const XmmVar& dst, const Mem& src)
  { _emitInstruction(kX86InstPAddW, &dst, &src); }

  //! @brief Packed DWORD Add (SSE2).
  inline void paddd(const XmmVar& dst, const XmmVar& src)
  { _emitInstruction(kX86InstPAddD, &dst, &src); }
  //! @brief Packed DWORD Add (SSE2).
  inline void paddd(const XmmVar& dst, const Mem& src)
  { _emitInstruction(kX86InstPAddD, &dst, &src); }

  //! @brief Packed QWORD Add (SSE2).
  inline void paddq(const MmVar& dst, const MmVar& src)
  { _emitInstruction(kX86InstPAddQ, &dst, &src); }
  //! @brief Packed QWORD Add (SSE2).
  inline void paddq(const MmVar& dst, const Mem& src)
  { _emitInstruction(kX86InstPAddQ, &dst, &src); }

  //! @brief Packed QWORD Add (SSE2).
  inline void paddq(const XmmVar& dst, const XmmVar& src)
  { _emitInstruction(kX86InstPAddQ, &dst, &src); }
  //! @brief Packed QWORD Add (SSE2).
  inline void paddq(const XmmVar& dst, const Mem& src)
  { _emitInstruction(kX86InstPAddQ, &dst, &src); }

  //! @brief Packed Add with Saturation (SSE2).
  inline void paddsb(const XmmVar& dst, const XmmVar& src)
  { _emitInstruction(kX86InstPAddSB, &dst, &src); }
  //! @brief Packed Add with Saturation (SSE2).
  inline void paddsb(const XmmVar& dst, const Mem& src)
  { _emitInstruction(kX86InstPAddSB, &dst, &src); }

  //! @brief Packed Add with Saturation (SSE2).
  inline void paddsw(const XmmVar& dst, const XmmVar& src)
  { _emitInstruction(kX86InstPAddSW, &dst, &src); }
  //! @brief Packed Add with Saturation (SSE2).
  inline void paddsw(const XmmVar& dst, const Mem& src)
  { _emitInstruction(kX86InstPAddSW, &dst, &src); }

  //! @brief Packed Add Unsigned with Saturation (SSE2).
  inline void paddusb(const XmmVar& dst, const XmmVar& src)
  { _emitInstruction(kX86InstPAddUSB, &dst, &src); }
  //! @brief Packed Add Unsigned with Saturation (SSE2).
  inline void paddusb(const XmmVar& dst, const Mem& src)
  { _emitInstruction(kX86InstPAddUSB, &dst, &src); }

  //! @brief Packed Add Unsigned with Saturation (SSE2).
  inline void paddusw(const XmmVar& dst, const XmmVar& src)
  { _emitInstruction(kX86InstPAddUSW, &dst, &src); }
  //! @brief Packed Add Unsigned with Saturation (SSE2).
  inline void paddusw(const XmmVar& dst, const Mem& src)
  { _emitInstruction(kX86InstPAddUSW, &dst, &src); }

  //! @brief Logical AND (SSE2).
  inline void pand(const XmmVar& dst, const XmmVar& src)
  { _emitInstruction(kX86InstPAnd, &dst, &src); }
  //! @brief Logical AND (SSE2).
  inline void pand(const XmmVar& dst, const Mem& src)
  { _emitInstruction(kX86InstPAnd, &dst, &src); }

  //! @brief Logical AND Not (SSE2).
  inline void pandn(const XmmVar& dst, const XmmVar& src)
  { _emitInstruction(kX86InstPAndN, &dst, &src); }
  //! @brief Logical AND Not (SSE2).
  inline void pandn(const XmmVar& dst, const Mem& src)
  { _emitInstruction(kX86InstPAndN, &dst, &src); }

  //! @brief Spin Loop Hint (SSE2).
  inline void pause()
  { _emitInstruction(kX86InstPause); }

  //! @brief Packed Average (SSE2).
  inline void pavgb(const XmmVar& dst, const XmmVar& src)
  { _emitInstruction(kX86InstPAvgB, &dst, &src); }
  //! @brief Packed Average (SSE2).
  inline void pavgb(const XmmVar& dst, const Mem& src)
  { _emitInstruction(kX86InstPAvgB, &dst, &src); }

  //! @brief Packed Average (SSE2).
  inline void pavgw(const XmmVar& dst, const XmmVar& src)
  { _emitInstruction(kX86InstPAvgW, &dst, &src); }
  //! @brief Packed Average (SSE2).
  inline void pavgw(const XmmVar& dst, const Mem& src)
  { _emitInstruction(kX86InstPAvgW, &dst, &src); }

  //! @brief Packed Compare for Equal (BYTES) (SSE2).
  inline void pcmpeqb(const XmmVar& dst, const XmmVar& src)
  { _emitInstruction(kX86InstPCmpEqB, &dst, &src); }
  //! @brief Packed Compare for Equal (BYTES) (SSE2).
  inline void pcmpeqb(const XmmVar& dst, const Mem& src)
  { _emitInstruction(kX86InstPCmpEqB, &dst, &src); }

  //! @brief Packed Compare for Equal (WORDS) (SSE2).
  inline void pcmpeqw(const XmmVar& dst, const XmmVar& src)
  { _emitInstruction(kX86InstPCmpEqW, &dst, &src); }
  //! @brief Packed Compare for Equal (WORDS) (SSE2).
  inline void pcmpeqw(const XmmVar& dst, const Mem& src)
  { _emitInstruction(kX86InstPCmpEqW, &dst, &src); }

  //! @brief Packed Compare for Equal (DWORDS) (SSE2).
  inline void pcmpeqd(const XmmVar& dst, const XmmVar& src)
  { _emitInstruction(kX86InstPCmpEqD, &dst, &src); }
  //! @brief Packed Compare for Equal (DWORDS) (SSE2).
  inline void pcmpeqd(const XmmVar& dst, const Mem& src)
  { _emitInstruction(kX86InstPCmpEqD, &dst, &src); }

  //! @brief Packed Compare for Greater Than (BYTES) (SSE2).
  inline void pcmpgtb(const XmmVar& dst, const XmmVar& src)
  { _emitInstruction(kX86InstPCmpGtB, &dst, &src); }
  //! @brief Packed Compare for Greater Than (BYTES) (SSE2).
  inline void pcmpgtb(const XmmVar& dst, const Mem& src)
  { _emitInstruction(kX86InstPCmpGtB, &dst, &src); }

  //! @brief Packed Compare for Greater Than (WORDS) (SSE2).
  inline void pcmpgtw(const XmmVar& dst, const XmmVar& src)
  { _emitInstruction(kX86InstPCmpGtW, &dst, &src); }
  //! @brief Packed Compare for Greater Than (WORDS) (SSE2).
  inline void pcmpgtw(const XmmVar& dst, const Mem& src)
  { _emitInstruction(kX86InstPCmpGtW, &dst, &src); }

  //! @brief Packed Compare for Greater Than (DWORDS) (SSE2).
  inline void pcmpgtd(const XmmVar& dst, const XmmVar& src)
  { _emitInstruction(kX86InstPCmpGtD, &dst, &src); }
  //! @brief Packed Compare for Greater Than (DWORDS) (SSE2).
  inline void pcmpgtd(const XmmVar& dst, const Mem& src)
  { _emitInstruction(kX86InstPCmpGtD, &dst, &src); }

  //! @brief Extract Word (SSE2).
  inline void pextrw(const GpVar& dst, const XmmVar& src, const Imm& imm8)
  { _emitInstruction(kX86InstPExtrW, &dst, &src, &imm8); }
  //! @brief Extract Word (SSE2).
  inline void pextrw(const Mem& dst, const XmmVar& src, const Imm& imm8)
  { _emitInstruction(kX86InstPExtrW, &dst, &src, &imm8); }

  //! @brief Packed Signed Integer Word Maximum (SSE2).
  inline void pmaxsw(const XmmVar& dst, const XmmVar& src)
  { _emitInstruction(kX86InstPMaxSW, &dst, &src); }
  //! @brief Packed Signed Integer Word Maximum (SSE2).
  inline void pmaxsw(const XmmVar& dst, const Mem& src)
  { _emitInstruction(kX86InstPMaxSW, &dst, &src); }

  //! @brief Packed Unsigned Integer Byte Maximum (SSE2).
  inline void pmaxub(const XmmVar& dst, const XmmVar& src)
  { _emitInstruction(kX86InstPMaxUB, &dst, &src); }
  //! @brief Packed Unsigned Integer Byte Maximum (SSE2).
  inline void pmaxub(const XmmVar& dst, const Mem& src)
  { _emitInstruction(kX86InstPMaxUB, &dst, &src); }

  //! @brief Packed Signed Integer Word Minimum (SSE2).
  inline void pminsw(const XmmVar& dst, const XmmVar& src)
  { _emitInstruction(kX86InstPMinSW, &dst, &src); }
  //! @brief Packed Signed Integer Word Minimum (SSE2).
  inline void pminsw(const XmmVar& dst, const Mem& src)
  { _emitInstruction(kX86InstPMinSW, &dst, &src); }

  //! @brief Packed Unsigned Integer Byte Minimum (SSE2).
  inline void pminub(const XmmVar& dst, const XmmVar& src)
  { _emitInstruction(kX86InstPMinUB, &dst, &src); }
  //! @brief Packed Unsigned Integer Byte Minimum (SSE2).
  inline void pminub(const XmmVar& dst, const Mem& src)
  { _emitInstruction(kX86InstPMinUB, &dst, &src); }

  //! @brief Move Byte Mask (SSE2).
  inline void pmovmskb(const GpVar& dst, const XmmVar& src)
  { _emitInstruction(kX86InstPMovMskB, &dst, &src); }

  //! @brief Packed Multiply High (SSE2).
  inline void pmulhw(const XmmVar& dst, const XmmVar& src)
  { _emitInstruction(kX86InstPMulHW, &dst, &src); }
  //! @brief Packed Multiply High (SSE2).
  inline void pmulhw(const XmmVar& dst, const Mem& src)
  { _emitInstruction(kX86InstPMulHW, &dst, &src); }

  //! @brief Packed Multiply High Unsigned (SSE2).
  inline void pmulhuw(const XmmVar& dst, const XmmVar& src)
  { _emitInstruction(kX86InstPMulHUW, &dst, &src); }
  //! @brief Packed Multiply High Unsigned (SSE2).
  inline void pmulhuw(const XmmVar& dst, const Mem& src)
  { _emitInstruction(kX86InstPMulHUW, &dst, &src); }

  //! @brief Packed Multiply Low (SSE2).
  inline void pmullw(const XmmVar& dst, const XmmVar& src)
  { _emitInstruction(kX86InstPMulLW, &dst, &src); }
  //! @brief Packed Multiply Low (SSE2).
  inline void pmullw(const XmmVar& dst, const Mem& src)
  { _emitInstruction(kX86InstPMulLW, &dst, &src); }

  //! @brief Packed Multiply to QWORD (SSE2).
  inline void pmuludq(const MmVar& dst, const MmVar& src)
  { _emitInstruction(kX86InstPMulUDQ, &dst, &src); }
  //! @brief Packed Multiply to QWORD (SSE2).
  inline void pmuludq(const MmVar& dst, const Mem& src)
  { _emitInstruction(kX86InstPMulUDQ, &dst, &src); }

  //! @brief Packed Multiply to QWORD (SSE2).
  inline void pmuludq(const XmmVar& dst, const XmmVar& src)
  { _emitInstruction(kX86InstPMulUDQ, &dst, &src); }
  //! @brief Packed Multiply to QWORD (SSE2).
  inline void pmuludq(const XmmVar& dst, const Mem& src)
  { _emitInstruction(kX86InstPMulUDQ, &dst, &src); }

  //! @brief Bitwise Logical OR (SSE2).
  inline void por(const XmmVar& dst, const XmmVar& src)
  { _emitInstruction(kX86InstPOr, &dst, &src); }
  //! @brief Bitwise Logical OR (SSE2).
  inline void por(const XmmVar& dst, const Mem& src)
  { _emitInstruction(kX86InstPOr, &dst, &src); }

  //! @brief Packed Shift Left Logical (SSE2).
  inline void pslld(const XmmVar& dst, const XmmVar& src)
  { _emitInstruction(kX86InstPSllD, &dst, &src); }
  //! @brief Packed Shift Left Logical (SSE2).
  inline void pslld(const XmmVar& dst, const Mem& src)
  { _emitInstruction(kX86InstPSllD, &dst, &src); }
  //! @brief Packed Shift Left Logical (SSE2).
  inline void pslld(const XmmVar& dst, const Imm& src)
  { _emitInstruction(kX86InstPSllD, &dst, &src); }

  //! @brief Packed Shift Left Logical (SSE2).
  inline void psllq(const XmmVar& dst, const XmmVar& src)
  { _emitInstruction(kX86InstPSllQ, &dst, &src); }
  //! @brief Packed Shift Left Logical (SSE2).
  inline void psllq(const XmmVar& dst, const Mem& src)
  { _emitInstruction(kX86InstPSllQ, &dst, &src); }
  //! @brief Packed Shift Left Logical (SSE2).
  inline void psllq(const XmmVar& dst, const Imm& src)
  { _emitInstruction(kX86InstPSllQ, &dst, &src); }

  //! @brief Packed Shift Left Logical (SSE2).
  inline void psllw(const XmmVar& dst, const XmmVar& src)
  { _emitInstruction(kX86InstPSllW, &dst, &src); }
  //! @brief Packed Shift Left Logical (SSE2).
  inline void psllw(const XmmVar& dst, const Mem& src)
  { _emitInstruction(kX86InstPSllW, &dst, &src); }
  //! @brief Packed Shift Left Logical (SSE2).
  inline void psllw(const XmmVar& dst, const Imm& src)
  { _emitInstruction(kX86InstPSllW, &dst, &src); }

  //! @brief Packed Shift Left Logical (SSE2).
  inline void pslldq(const XmmVar& dst, const Imm& src)
  { _emitInstruction(kX86InstPSllDQ, &dst, &src); }

  //! @brief Packed Shift Right Arithmetic (SSE2).
  inline void psrad(const XmmVar& dst, const XmmVar& src)
  { _emitInstruction(kX86InstPSraD, &dst, &src); }
  //! @brief Packed Shift Right Arithmetic (SSE2).
  inline void psrad(const XmmVar& dst, const Mem& src)
  { _emitInstruction(kX86InstPSraD, &dst, &src); }
  //! @brief Packed Shift Right Arithmetic (SSE2).
  inline void psrad(const XmmVar& dst, const Imm& src)
  { _emitInstruction(kX86InstPSraD, &dst, &src); }

  //! @brief Packed Shift Right Arithmetic (SSE2).
  inline void psraw(const XmmVar& dst, const XmmVar& src)
  { _emitInstruction(kX86InstPSraW, &dst, &src); }
  //! @brief Packed Shift Right Arithmetic (SSE2).
  inline void psraw(const XmmVar& dst, const Mem& src)
  { _emitInstruction(kX86InstPSraW, &dst, &src); }
  //! @brief Packed Shift Right Arithmetic (SSE2).
  inline void psraw(const XmmVar& dst, const Imm& src)
  { _emitInstruction(kX86InstPSraW, &dst, &src); }

  //! @brief Packed Subtract (SSE2).
  inline void psubb(const XmmVar& dst, const XmmVar& src)
  { _emitInstruction(kX86InstPSubB, &dst, &src); }
  //! @brief Packed Subtract (SSE2).
  inline void psubb(const XmmVar& dst, const Mem& src)
  { _emitInstruction(kX86InstPSubB, &dst, &src); }

  //! @brief Packed Subtract (SSE2).
  inline void psubw(const XmmVar& dst, const XmmVar& src)
  { _emitInstruction(kX86InstPSubW, &dst, &src); }
  //! @brief Packed Subtract (SSE2).
  inline void psubw(const XmmVar& dst, const Mem& src)
  { _emitInstruction(kX86InstPSubW, &dst, &src); }

  //! @brief Packed Subtract (SSE2).
  inline void psubd(const XmmVar& dst, const XmmVar& src)
  { _emitInstruction(kX86InstPSubD, &dst, &src); }
  //! @brief Packed Subtract (SSE2).
  inline void psubd(const XmmVar& dst, const Mem& src)
  { _emitInstruction(kX86InstPSubD, &dst, &src); }

  //! @brief Packed Subtract (SSE2).
  inline void psubq(const MmVar& dst, const MmVar& src)
  { _emitInstruction(kX86InstPSubQ, &dst, &src); }
  //! @brief Packed Subtract (SSE2).
  inline void psubq(const MmVar& dst, const Mem& src)
  { _emitInstruction(kX86InstPSubQ, &dst, &src); }

  //! @brief Packed Subtract (SSE2).
  inline void psubq(const XmmVar& dst, const XmmVar& src)
  { _emitInstruction(kX86InstPSubQ, &dst, &src); }
  //! @brief Packed Subtract (SSE2).
  inline void psubq(const XmmVar& dst, const Mem& src)
  { _emitInstruction(kX86InstPSubQ, &dst, &src); }

  //! @brief Packed Multiply and Add (SSE2).
  inline void pmaddwd(const XmmVar& dst, const XmmVar& src)
  { _emitInstruction(kX86InstPMAddWD, &dst, &src); }
  //! @brief Packed Multiply and Add (SSE2).
  inline void pmaddwd(const XmmVar& dst, const Mem& src)
  { _emitInstruction(kX86InstPMAddWD, &dst, &src); }

  //! @brief Shuffle Packed DWORDs (SSE2).
  inline void pshufd(const XmmVar& dst, const XmmVar& src, const Imm& imm8)
  { _emitInstruction(kX86InstPShufD, &dst, &src, &imm8); }
  //! @brief Shuffle Packed DWORDs (SSE2).
  inline void pshufd(const XmmVar& dst, const Mem& src, const Imm& imm8)
  { _emitInstruction(kX86InstPShufD, &dst, &src, &imm8); }

  //! @brief Shuffle Packed High Words (SSE2).
  inline void pshufhw(const XmmVar& dst, const XmmVar& src, const Imm& imm8)
  { _emitInstruction(kX86InstPShufHW, &dst, &src, &imm8); }
  //! @brief Shuffle Packed High Words (SSE2).
  inline void pshufhw(const XmmVar& dst, const Mem& src, const Imm& imm8)
  { _emitInstruction(kX86InstPShufHW, &dst, &src, &imm8); }

  //! @brief Shuffle Packed Low Words (SSE2).
  inline void pshuflw(const XmmVar& dst, const XmmVar& src, const Imm& imm8)
  { _emitInstruction(kX86InstPShufLW, &dst, &src, &imm8); }
  //! @brief Shuffle Packed Low Words (SSE2).
  inline void pshuflw(const XmmVar& dst, const Mem& src, const Imm& imm8)
  { _emitInstruction(kX86InstPShufLW, &dst, &src, &imm8); }

  //! @brief Packed Shift Right Logical (SSE2).
  inline void psrld(const XmmVar& dst, const XmmVar& src)
  { _emitInstruction(kX86InstPSrlD, &dst, &src); }
  //! @brief Packed Shift Right Logical (SSE2).
  inline void psrld(const XmmVar& dst, const Mem& src)
  { _emitInstruction(kX86InstPSrlD, &dst, &src); }
  //! @brief Packed Shift Right Logical (SSE2).
  inline void psrld(const XmmVar& dst, const Imm& src)
  { _emitInstruction(kX86InstPSrlD, &dst, &src); }

  //! @brief Packed Shift Right Logical (SSE2).
  inline void psrlq(const XmmVar& dst, const XmmVar& src)
  { _emitInstruction(kX86InstPSrlQ, &dst, &src); }
  //! @brief Packed Shift Right Logical (SSE2).
  inline void psrlq(const XmmVar& dst, const Mem& src)
  { _emitInstruction(kX86InstPSrlQ, &dst, &src); }
  //! @brief Packed Shift Right Logical (SSE2).
  inline void psrlq(const XmmVar& dst, const Imm& src)
  { _emitInstruction(kX86InstPSrlQ, &dst, &src); }

  //! @brief DQWord Shift Right Logical (MMX).
  inline void psrldq(const XmmVar& dst, const Imm& src)
  { _emitInstruction(kX86InstPSrlDQ, &dst, &src); }

  //! @brief Packed Shift Right Logical (SSE2).
  inline void psrlw(const XmmVar& dst, const XmmVar& src)
  { _emitInstruction(kX86InstPSrlW, &dst, &src); }
  //! @brief Packed Shift Right Logical (SSE2).
  inline void psrlw(const XmmVar& dst, const Mem& src)
  { _emitInstruction(kX86InstPSrlW, &dst, &src); }
  //! @brief Packed Shift Right Logical (SSE2).
  inline void psrlw(const XmmVar& dst, const Imm& src)
  { _emitInstruction(kX86InstPSrlW, &dst, &src); }

  //! @brief Packed Subtract with Saturation (SSE2).
  inline void psubsb(const XmmVar& dst, const XmmVar& src)
  { _emitInstruction(kX86InstPSubSB, &dst, &src); }
  //! @brief Packed Subtract with Saturation (SSE2).
  inline void psubsb(const XmmVar& dst, const Mem& src)
  { _emitInstruction(kX86InstPSubSB, &dst, &src); }

  //! @brief Packed Subtract with Saturation (SSE2).
  inline void psubsw(const XmmVar& dst, const XmmVar& src)
  { _emitInstruction(kX86InstPSubSW, &dst, &src); }
  //! @brief Packed Subtract with Saturation (SSE2).
  inline void psubsw(const XmmVar& dst, const Mem& src)
  { _emitInstruction(kX86InstPSubSW, &dst, &src); }

  //! @brief Packed Subtract with Unsigned Saturation (SSE2).
  inline void psubusb(const XmmVar& dst, const XmmVar& src)
  { _emitInstruction(kX86InstPSubUSB, &dst, &src); }
  //! @brief Packed Subtract with Unsigned Saturation (SSE2).
  inline void psubusb(const XmmVar& dst, const Mem& src)
  { _emitInstruction(kX86InstPSubUSB, &dst, &src); }

  //! @brief Packed Subtract with Unsigned Saturation (SSE2).
  inline void psubusw(const XmmVar& dst, const XmmVar& src)
  { _emitInstruction(kX86InstPSubUSW, &dst, &src); }
  //! @brief Packed Subtract with Unsigned Saturation (SSE2).
  inline void psubusw(const XmmVar& dst, const Mem& src)
  { _emitInstruction(kX86InstPSubUSW, &dst, &src); }

  //! @brief Unpack High Data (SSE2).
  inline void punpckhbw(const XmmVar& dst, const XmmVar& src)
  { _emitInstruction(kX86InstPunpckHBW, &dst, &src); }
  //! @brief Unpack High Data (SSE2).
  inline void punpckhbw(const XmmVar& dst, const Mem& src)
  { _emitInstruction(kX86InstPunpckHBW, &dst, &src); }

  //! @brief Unpack High Data (SSE2).
  inline void punpckhwd(const XmmVar& dst, const XmmVar& src)
  { _emitInstruction(kX86InstPunpckHWD, &dst, &src); }
  //! @brief Unpack High Data (SSE2).
  inline void punpckhwd(const XmmVar& dst, const Mem& src)
  { _emitInstruction(kX86InstPunpckHWD, &dst, &src); }

  //! @brief Unpack High Data (SSE2).
  inline void punpckhdq(const XmmVar& dst, const XmmVar& src)
  { _emitInstruction(kX86InstPunpckHDQ, &dst, &src); }
  //! @brief Unpack High Data (SSE2).
  inline void punpckhdq(const XmmVar& dst, const Mem& src)
  { _emitInstruction(kX86InstPunpckHDQ, &dst, &src); }

  //! @brief Unpack High Data (SSE2).
  inline void punpckhqdq(const XmmVar& dst, const XmmVar& src)
  { _emitInstruction(kX86InstPunpckHQDQ, &dst, &src); }
  //! @brief Unpack High Data (SSE2).
  inline void punpckhqdq(const XmmVar& dst, const Mem& src)
  { _emitInstruction(kX86InstPunpckHQDQ, &dst, &src); }

  //! @brief Unpack Low Data (SSE2).
  inline void punpcklbw(const XmmVar& dst, const XmmVar& src)
  { _emitInstruction(kX86InstPunpckLBW, &dst, &src); }
  //! @brief Unpack Low Data (SSE2).
  inline void punpcklbw(const XmmVar& dst, const Mem& src)
  { _emitInstruction(kX86InstPunpckLBW, &dst, &src); }

  //! @brief Unpack Low Data (SSE2).
  inline void punpcklwd(const XmmVar& dst, const XmmVar& src)
  { _emitInstruction(kX86InstPunpckLWD, &dst, &src); }
  //! @brief Unpack Low Data (SSE2).
  inline void punpcklwd(const XmmVar& dst, const Mem& src)
  { _emitInstruction(kX86InstPunpckLWD, &dst, &src); }

  //! @brief Unpack Low Data (SSE2).
  inline void punpckldq(const XmmVar& dst, const XmmVar& src)
  { _emitInstruction(kX86InstPunpckLDQ, &dst, &src); }
  //! @brief Unpack Low Data (SSE2).
  inline void punpckldq(const XmmVar& dst, const Mem& src)
  { _emitInstruction(kX86InstPunpckLDQ, &dst, &src); }

  //! @brief Unpack Low Data (SSE2).
  inline void punpcklqdq(const XmmVar& dst, const XmmVar& src)
  { _emitInstruction(kX86InstPunpckLQDQ, &dst, &src); }
  //! @brief Unpack Low Data (SSE2).
  inline void punpcklqdq(const XmmVar& dst, const Mem& src)
  { _emitInstruction(kX86InstPunpckLQDQ, &dst, &src); }

  //! @brief Bitwise Exclusive OR (SSE2).
  inline void pxor(const XmmVar& dst, const XmmVar& src)
  { _emitInstruction(kX86InstPXor, &dst, &src); }
  //! @brief Bitwise Exclusive OR (SSE2).
  inline void pxor(const XmmVar& dst, const Mem& src)
  { _emitInstruction(kX86InstPXor, &dst, &src); }

  //! @brief Shuffle DP-FP (SSE2).
  inline void shufpd(const XmmVar& dst, const XmmVar& src, const Imm& imm8)
  { _emitInstruction(kX86InstShufPD, &dst, &src, &imm8); }
  //! @brief Shuffle DP-FP (SSE2).
  inline void shufpd(const XmmVar& dst, const Mem& src, const Imm& imm8)
  { _emitInstruction(kX86InstShufPD, &dst, &src, &imm8); }

  //! @brief Compute Square Roots of Packed DP-FP Values (SSE2).
  inline void sqrtpd(const XmmVar& dst, const XmmVar& src)
  { _emitInstruction(kX86InstSqrtPD, &dst, &src); }
  //! @brief Compute Square Roots of Packed DP-FP Values (SSE2).
  inline void sqrtpd(const XmmVar& dst, const Mem& src)
  { _emitInstruction(kX86InstSqrtPD, &dst, &src); }

  //! @brief Compute Square Root of Scalar DP-FP Value (SSE2).
  inline void sqrtsd(const XmmVar& dst, const XmmVar& src)
  { _emitInstruction(kX86InstSqrtSD, &dst, &src); }
  //! @brief Compute Square Root of Scalar DP-FP Value (SSE2).
  inline void sqrtsd(const XmmVar& dst, const Mem& src)
  { _emitInstruction(kX86InstSqrtSD, &dst, &src); }

  //! @brief Packed DP-FP Subtract (SSE2).
  inline void subpd(const XmmVar& dst, const XmmVar& src)
  { _emitInstruction(kX86InstSubPD, &dst, &src); }
  //! @brief Packed DP-FP Subtract (SSE2).
  inline void subpd(const XmmVar& dst, const Mem& src)
  { _emitInstruction(kX86InstSubPD, &dst, &src); }

  //! @brief Scalar DP-FP Subtract (SSE2).
  inline void subsd(const XmmVar& dst, const XmmVar& src)
  { _emitInstruction(kX86InstSubSD, &dst, &src); }
  //! @brief Scalar DP-FP Subtract (SSE2).
  inline void subsd(const XmmVar& dst, const Mem& src)
  { _emitInstruction(kX86InstSubSD, &dst, &src); }

  //! @brief Scalar Unordered DP-FP Compare and Set EFLAGS (SSE2).
  inline void ucomisd(const XmmVar& dst, const XmmVar& src)
  { _emitInstruction(kX86InstUComISD, &dst, &src); }
  //! @brief Scalar Unordered DP-FP Compare and Set EFLAGS (SSE2).
  inline void ucomisd(const XmmVar& dst, const Mem& src)
  { _emitInstruction(kX86InstUComISD, &dst, &src); }

  //! @brief Unpack and Interleave High Packed Double-Precision FP Values (SSE2).
  inline void unpckhpd(const XmmVar& dst, const XmmVar& src)
  { _emitInstruction(kX86InstUnpckHPD, &dst, &src); }
  //! @brief Unpack and Interleave High Packed Double-Precision FP Values (SSE2).
  inline void unpckhpd(const XmmVar& dst, const Mem& src)
  { _emitInstruction(kX86InstUnpckHPD, &dst, &src); }

  //! @brief Unpack and Interleave Low Packed Double-Precision FP Values (SSE2).
  inline void unpcklpd(const XmmVar& dst, const XmmVar& src)
  { _emitInstruction(kX86InstUnpckLPD, &dst, &src); }
  //! @brief Unpack and Interleave Low Packed Double-Precision FP Values (SSE2).
  inline void unpcklpd(const XmmVar& dst, const Mem& src)
  { _emitInstruction(kX86InstUnpckLPD, &dst, &src); }

  //! @brief Bit-wise Logical OR for DP-FP Data (SSE2).
  inline void xorpd(const XmmVar& dst, const XmmVar& src)
  { _emitInstruction(kX86InstXorPD, &dst, &src); }
  //! @brief Bit-wise Logical OR for DP-FP Data (SSE2).
  inline void xorpd(const XmmVar& dst, const Mem& src)
  { _emitInstruction(kX86InstXorPD, &dst, &src); }

  // --------------------------------------------------------------------------
  // [SSE3]
  // --------------------------------------------------------------------------

  //! @brief Packed DP-FP Add/Subtract (SSE3).
  inline void addsubpd(const XmmVar& dst, const XmmVar& src)
  { _emitInstruction(kX86InstAddSubPD, &dst, &src); }
  //! @brief Packed DP-FP Add/Subtract (SSE3).
  inline void addsubpd(const XmmVar& dst, const Mem& src)
  { _emitInstruction(kX86InstAddSubPD, &dst, &src); }

  //! @brief Packed SP-FP Add/Subtract (SSE3).
  inline void addsubps(const XmmVar& dst, const XmmVar& src)
  { _emitInstruction(kX86InstAddSubPS, &dst, &src); }
  //! @brief Packed SP-FP Add/Subtract (SSE3).
  inline void addsubps(const XmmVar& dst, const Mem& src)
  { _emitInstruction(kX86InstAddSubPS, &dst, &src); }

#if ASMJIT_NOT_SUPPORTED_BY_COMPILER
  // TODO: NOT IMPLEMENTED BY THE COMPILER.
  //! @brief Store Integer with Truncation (SSE3).
  inline void fisttp(const Mem& dst)
  { _emitInstruction(kX86InstFISttP, &dst); }
#endif // ASMJIT_NOT_SUPPORTED_BY_COMPILER

  //! @brief Packed DP-FP Horizontal Add (SSE3).
  inline void haddpd(const XmmVar& dst, const XmmVar& src)
  { _emitInstruction(kX86InstHAddPD, &dst, &src); }
  //! @brief Packed DP-FP Horizontal Add (SSE3).
  inline void haddpd(const XmmVar& dst, const Mem& src)
  { _emitInstruction(kX86InstHAddPD, &dst, &src); }

  //! @brief Packed SP-FP Horizontal Add (SSE3).
  inline void haddps(const XmmVar& dst, const XmmVar& src)
  { _emitInstruction(kX86InstHAddPS, &dst, &src); }
  //! @brief Packed SP-FP Horizontal Add (SSE3).
  inline void haddps(const XmmVar& dst, const Mem& src)
  { _emitInstruction(kX86InstHAddPS, &dst, &src); }

  //! @brief Packed DP-FP Horizontal Subtract (SSE3).
  inline void hsubpd(const XmmVar& dst, const XmmVar& src)
  { _emitInstruction(kX86InstHSubPD, &dst, &src); }
  //! @brief Packed DP-FP Horizontal Subtract (SSE3).
  inline void hsubpd(const XmmVar& dst, const Mem& src)
  { _emitInstruction(kX86InstHSubPD, &dst, &src); }

  //! @brief Packed SP-FP Horizontal Subtract (SSE3).
  inline void hsubps(const XmmVar& dst, const XmmVar& src)
  { _emitInstruction(kX86InstHSubPS, &dst, &src); }
  //! @brief Packed SP-FP Horizontal Subtract (SSE3).
  inline void hsubps(const XmmVar& dst, const Mem& src)
  { _emitInstruction(kX86InstHSubPS, &dst, &src); }

  //! @brief Load Unaligned Integer 128 Bits (SSE3).
  inline void lddqu(const XmmVar& dst, const Mem& src)
  { _emitInstruction(kX86InstLdDQU, &dst, &src); }

#if ASMJIT_NOT_SUPPORTED_BY_COMPILER
  //! @brief Set Up Monitor Address (SSE3).
  inline void monitor()
  { _emitInstruction(kX86InstMonitor); }
#endif // ASMJIT_NOT_SUPPORTED_BY_COMPILER

  //! @brief Move One DP-FP and Duplicate (SSE3).
  inline void movddup(const XmmVar& dst, const XmmVar& src)
  { _emitInstruction(kX86InstMovDDup, &dst, &src); }
  //! @brief Move One DP-FP and Duplicate (SSE3).
  inline void movddup(const XmmVar& dst, const Mem& src)
  { _emitInstruction(kX86InstMovDDup, &dst, &src); }

  //! @brief Move Packed SP-FP High and Duplicate (SSE3).
  inline void movshdup(const XmmVar& dst, const XmmVar& src)
  { _emitInstruction(kX86InstMovSHDup, &dst, &src); }
  //! @brief Move Packed SP-FP High and Duplicate (SSE3).
  inline void movshdup(const XmmVar& dst, const Mem& src)
  { _emitInstruction(kX86InstMovSHDup, &dst, &src); }

  //! @brief Move Packed SP-FP Low and Duplicate (SSE3).
  inline void movsldup(const XmmVar& dst, const XmmVar& src)
  { _emitInstruction(kX86InstMovSLDup, &dst, &src); }
  //! @brief Move Packed SP-FP Low and Duplicate (SSE3).
  inline void movsldup(const XmmVar& dst, const Mem& src)
  { _emitInstruction(kX86InstMovSLDup, &dst, &src); }

#if ASMJIT_NOT_SUPPORTED_BY_COMPILER
  //! @brief Monitor Wait (SSE3).
  inline void mwait()
  { _emitInstruction(kX86InstMWait); }
#endif // ASMJIT_NOT_SUPPORTED_BY_COMPILER

  // --------------------------------------------------------------------------
  // [SSSE3]
  // --------------------------------------------------------------------------

  //! @brief Packed SIGN (SSSE3).
  inline void psignb(const MmVar& dst, const MmVar& src)
  { _emitInstruction(kX86InstPSignB, &dst, &src); }
  //! @brief Packed SIGN (SSSE3).
  inline void psignb(const MmVar& dst, const Mem& src)
  { _emitInstruction(kX86InstPSignB, &dst, &src); }

  //! @brief Packed SIGN (SSSE3).
  inline void psignb(const XmmVar& dst, const XmmVar& src)
  { _emitInstruction(kX86InstPSignB, &dst, &src); }
  //! @brief Packed SIGN (SSSE3).
  inline void psignb(const XmmVar& dst, const Mem& src)
  { _emitInstruction(kX86InstPSignB, &dst, &src); }

  //! @brief Packed SIGN (SSSE3).
  inline void psignw(const MmVar& dst, const MmVar& src)
  { _emitInstruction(kX86InstPSignW, &dst, &src); }
  //! @brief Packed SIGN (SSSE3).
  inline void psignw(const MmVar& dst, const Mem& src)
  { _emitInstruction(kX86InstPSignW, &dst, &src); }

  //! @brief Packed SIGN (SSSE3).
  inline void psignw(const XmmVar& dst, const XmmVar& src)
  { _emitInstruction(kX86InstPSignW, &dst, &src); }
  //! @brief Packed SIGN (SSSE3).
  inline void psignw(const XmmVar& dst, const Mem& src)
  { _emitInstruction(kX86InstPSignW, &dst, &src); }

  //! @brief Packed SIGN (SSSE3).
  inline void psignd(const MmVar& dst, const MmVar& src)
  { _emitInstruction(kX86InstPSignD, &dst, &src); }
  //! @brief Packed SIGN (SSSE3).
  inline void psignd(const MmVar& dst, const Mem& src)
  { _emitInstruction(kX86InstPSignD, &dst, &src); }

  //! @brief Packed SIGN (SSSE3).
  inline void psignd(const XmmVar& dst, const XmmVar& src)
  { _emitInstruction(kX86InstPSignD, &dst, &src); }
  //! @brief Packed SIGN (SSSE3).
  inline void psignd(const XmmVar& dst, const Mem& src)
  { _emitInstruction(kX86InstPSignD, &dst, &src); }

  //! @brief Packed Horizontal Add (SSSE3).
  inline void phaddw(const MmVar& dst, const MmVar& src)
  { _emitInstruction(kX86InstPHAddW, &dst, &src); }
  //! @brief Packed Horizontal Add (SSSE3).
  inline void phaddw(const MmVar& dst, const Mem& src)
  { _emitInstruction(kX86InstPHAddW, &dst, &src); }

  //! @brief Packed Horizontal Add (SSSE3).
  inline void phaddw(const XmmVar& dst, const XmmVar& src)
  { _emitInstruction(kX86InstPHAddW, &dst, &src); }
  //! @brief Packed Horizontal Add (SSSE3).
  inline void phaddw(const XmmVar& dst, const Mem& src)
  { _emitInstruction(kX86InstPHAddW, &dst, &src); }

  //! @brief Packed Horizontal Add (SSSE3).
  inline void phaddd(const MmVar& dst, const MmVar& src)
  { _emitInstruction(kX86InstPHAddD, &dst, &src); }
  //! @brief Packed Horizontal Add (SSSE3).
  inline void phaddd(const MmVar& dst, const Mem& src)
  { _emitInstruction(kX86InstPHAddD, &dst, &src); }

  //! @brief Packed Horizontal Add (SSSE3).
  inline void phaddd(const XmmVar& dst, const XmmVar& src)
  { _emitInstruction(kX86InstPHAddD, &dst, &src); }
  //! @brief Packed Horizontal Add (SSSE3).
  inline void phaddd(const XmmVar& dst, const Mem& src)
  { _emitInstruction(kX86InstPHAddD, &dst, &src); }

  //! @brief Packed Horizontal Add and Saturate (SSSE3).
  inline void phaddsw(const MmVar& dst, const MmVar& src)
  { _emitInstruction(kX86InstPHAddSW, &dst, &src); }
  //! @brief Packed Horizontal Add and Saturate (SSSE3).
  inline void phaddsw(const MmVar& dst, const Mem& src)
  { _emitInstruction(kX86InstPHAddSW, &dst, &src); }

  //! @brief Packed Horizontal Add and Saturate (SSSE3).
  inline void phaddsw(const XmmVar& dst, const XmmVar& src)
  { _emitInstruction(kX86InstPHAddSW, &dst, &src); }
  //! @brief Packed Horizontal Add and Saturate (SSSE3).
  inline void phaddsw(const XmmVar& dst, const Mem& src)
  { _emitInstruction(kX86InstPHAddSW, &dst, &src); }

  //! @brief Packed Horizontal Subtract (SSSE3).
  inline void phsubw(const MmVar& dst, const MmVar& src)
  { _emitInstruction(kX86InstPHSubW, &dst, &src); }
  //! @brief Packed Horizontal Subtract (SSSE3).
  inline void phsubw(const MmVar& dst, const Mem& src)
  { _emitInstruction(kX86InstPHSubW, &dst, &src); }

  //! @brief Packed Horizontal Subtract (SSSE3).
  inline void phsubw(const XmmVar& dst, const XmmVar& src)
  { _emitInstruction(kX86InstPHSubW, &dst, &src); }
  //! @brief Packed Horizontal Subtract (SSSE3).
  inline void phsubw(const XmmVar& dst, const Mem& src)
  { _emitInstruction(kX86InstPHSubW, &dst, &src); }

  //! @brief Packed Horizontal Subtract (SSSE3).
  inline void phsubd(const MmVar& dst, const MmVar& src)
  { _emitInstruction(kX86InstPHSubD, &dst, &src); }
  //! @brief Packed Horizontal Subtract (SSSE3).
  inline void phsubd(const MmVar& dst, const Mem& src)
  { _emitInstruction(kX86InstPHSubD, &dst, &src); }

  //! @brief Packed Horizontal Subtract (SSSE3).
  inline void phsubd(const XmmVar& dst, const XmmVar& src)
  { _emitInstruction(kX86InstPHSubD, &dst, &src); }
  //! @brief Packed Horizontal Subtract (SSSE3).
  inline void phsubd(const XmmVar& dst, const Mem& src)
  { _emitInstruction(kX86InstPHSubD, &dst, &src); }

  //! @brief Packed Horizontal Subtract and Saturate (SSSE3).
  inline void phsubsw(const MmVar& dst, const MmVar& src)
  { _emitInstruction(kX86InstPHSubSW, &dst, &src); }
  //! @brief Packed Horizontal Subtract and Saturate (SSSE3).
  inline void phsubsw(const MmVar& dst, const Mem& src)
  { _emitInstruction(kX86InstPHSubSW, &dst, &src); }

  //! @brief Packed Horizontal Subtract and Saturate (SSSE3).
  inline void phsubsw(const XmmVar& dst, const XmmVar& src)
  { _emitInstruction(kX86InstPHSubSW, &dst, &src); }
  //! @brief Packed Horizontal Subtract and Saturate (SSSE3).
  inline void phsubsw(const XmmVar& dst, const Mem& src)
  { _emitInstruction(kX86InstPHSubSW, &dst, &src); }

  //! @brief Multiply and Add Packed Signed and Unsigned Bytes (SSSE3).
  inline void pmaddubsw(const MmVar& dst, const MmVar& src)
  { _emitInstruction(kX86InstPMAddUBSW, &dst, &src); }
  //! @brief Multiply and Add Packed Signed and Unsigned Bytes (SSSE3).
  inline void pmaddubsw(const MmVar& dst, const Mem& src)
  { _emitInstruction(kX86InstPMAddUBSW, &dst, &src); }

  //! @brief Multiply and Add Packed Signed and Unsigned Bytes (SSSE3).
  inline void pmaddubsw(const XmmVar& dst, const XmmVar& src)
  { _emitInstruction(kX86InstPMAddUBSW, &dst, &src); }
  //! @brief Multiply and Add Packed Signed and Unsigned Bytes (SSSE3).
  inline void pmaddubsw(const XmmVar& dst, const Mem& src)
  { _emitInstruction(kX86InstPMAddUBSW, &dst, &src); }

  //! @brief Packed Absolute Value (SSSE3).
  inline void pabsb(const MmVar& dst, const MmVar& src)
  { _emitInstruction(kX86InstPAbsB, &dst, &src); }
  //! @brief Packed Absolute Value (SSSE3).
  inline void pabsb(const MmVar& dst, const Mem& src)
  { _emitInstruction(kX86InstPAbsB, &dst, &src); }

  //! @brief Packed Absolute Value (SSSE3).
  inline void pabsb(const XmmVar& dst, const XmmVar& src)
  { _emitInstruction(kX86InstPAbsB, &dst, &src); }
  //! @brief Packed Absolute Value (SSSE3).
  inline void pabsb(const XmmVar& dst, const Mem& src)
  { _emitInstruction(kX86InstPAbsB, &dst, &src); }

  //! @brief Packed Absolute Value (SSSE3).
  inline void pabsw(const MmVar& dst, const MmVar& src)
  { _emitInstruction(kX86InstPAbsW, &dst, &src); }
  //! @brief Packed Absolute Value (SSSE3).
  inline void pabsw(const MmVar& dst, const Mem& src)
  { _emitInstruction(kX86InstPAbsW, &dst, &src); }

  //! @brief Packed Absolute Value (SSSE3).
  inline void pabsw(const XmmVar& dst, const XmmVar& src)
  { _emitInstruction(kX86InstPAbsW, &dst, &src); }
  //! @brief Packed Absolute Value (SSSE3).
  inline void pabsw(const XmmVar& dst, const Mem& src)
  { _emitInstruction(kX86InstPAbsW, &dst, &src); }

  //! @brief Packed Absolute Value (SSSE3).
  inline void pabsd(const MmVar& dst, const MmVar& src)
  { _emitInstruction(kX86InstPAbsD, &dst, &src); }
  //! @brief Packed Absolute Value (SSSE3).
  inline void pabsd(const MmVar& dst, const Mem& src)
  { _emitInstruction(kX86InstPAbsD, &dst, &src); }

  //! @brief Packed Absolute Value (SSSE3).
  inline void pabsd(const XmmVar& dst, const XmmVar& src)
  { _emitInstruction(kX86InstPAbsD, &dst, &src); }
  //! @brief Packed Absolute Value (SSSE3).
  inline void pabsd(const XmmVar& dst, const Mem& src)
  { _emitInstruction(kX86InstPAbsD, &dst, &src); }

  //! @brief Packed Multiply High with Round and Scale (SSSE3).
  inline void pmulhrsw(const MmVar& dst, const MmVar& src)
  { _emitInstruction(kX86InstPMulHRSW, &dst, &src); }
  //! @brief Packed Multiply High with Round and Scale (SSSE3).
  inline void pmulhrsw(const MmVar& dst, const Mem& src)
  { _emitInstruction(kX86InstPMulHRSW, &dst, &src); }

  //! @brief Packed Multiply High with Round and Scale (SSSE3).
  inline void pmulhrsw(const XmmVar& dst, const XmmVar& src)
  { _emitInstruction(kX86InstPMulHRSW, &dst, &src); }
  //! @brief Packed Multiply High with Round and Scale (SSSE3).
  inline void pmulhrsw(const XmmVar& dst, const Mem& src)
  { _emitInstruction(kX86InstPMulHRSW, &dst, &src); }

  //! @brief Packed Shuffle Bytes (SSSE3).
  inline void pshufb(const MmVar& dst, const MmVar& src)
  { _emitInstruction(kX86InstPShufB, &dst, &src); }
  //! @brief Packed Shuffle Bytes (SSSE3).
  inline void pshufb(const MmVar& dst, const Mem& src)
  { _emitInstruction(kX86InstPShufB, &dst, &src); }

  //! @brief Packed Shuffle Bytes (SSSE3).
  inline void pshufb(const XmmVar& dst, const XmmVar& src)
  { _emitInstruction(kX86InstPShufB, &dst, &src); }
  //! @brief Packed Shuffle Bytes (SSSE3).
  inline void pshufb(const XmmVar& dst, const Mem& src)
  { _emitInstruction(kX86InstPShufB, &dst, &src); }

  //! @brief Packed Shuffle Bytes (SSSE3).
  inline void palignr(const MmVar& dst, const MmVar& src, const Imm& imm8)
  { _emitInstruction(kX86InstPAlignR, &dst, &src, &imm8); }
  //! @brief Packed Shuffle Bytes (SSSE3).
  inline void palignr(const MmVar& dst, const Mem& src, const Imm& imm8)
  { _emitInstruction(kX86InstPAlignR, &dst, &src, &imm8); }

  //! @brief Packed Shuffle Bytes (SSSE3).
  inline void palignr(const XmmVar& dst, const XmmVar& src, const Imm& imm8)
  { _emitInstruction(kX86InstPAlignR, &dst, &src, &imm8); }
  //! @brief Packed Shuffle Bytes (SSSE3).
  inline void palignr(const XmmVar& dst, const Mem& src, const Imm& imm8)
  { _emitInstruction(kX86InstPAlignR, &dst, &src, &imm8); }

  // --------------------------------------------------------------------------
  // [SSE4.1]
  // --------------------------------------------------------------------------

  //! @brief Blend Packed DP-FP Values (SSE4.1).
  inline void blendpd(const XmmVar& dst, const XmmVar& src, const Imm& imm8)
  { _emitInstruction(kX86InstBlendPD, &dst, &src, &imm8); }
  //! @brief Blend Packed DP-FP Values (SSE4.1).
  inline void blendpd(const XmmVar& dst, const Mem& src, const Imm& imm8)
  { _emitInstruction(kX86InstBlendPD, &dst, &src, &imm8); }

  //! @brief Blend Packed SP-FP Values (SSE4.1).
  inline void blendps(const XmmVar& dst, const XmmVar& src, const Imm& imm8)
  { _emitInstruction(kX86InstBlendPS, &dst, &src, &imm8); }
  //! @brief Blend Packed SP-FP Values (SSE4.1).
  inline void blendps(const XmmVar& dst, const Mem& src, const Imm& imm8)
  { _emitInstruction(kX86InstBlendPS, &dst, &src, &imm8); }

  //! @brief Variable Blend Packed DP-FP Values (SSE4.1).
  inline void blendvpd(const XmmVar& dst, const XmmVar& src)
  { _emitInstruction(kX86InstBlendVPD, &dst, &src); }
  //! @brief Variable Blend Packed DP-FP Values (SSE4.1).
  inline void blendvpd(const XmmVar& dst, const Mem& src)
  { _emitInstruction(kX86InstBlendVPD, &dst, &src); }

  //! @brief Variable Blend Packed SP-FP Values (SSE4.1).
  inline void blendvps(const XmmVar& dst, const XmmVar& src)
  { _emitInstruction(kX86InstBlendVPS, &dst, &src); }
  //! @brief Variable Blend Packed SP-FP Values (SSE4.1).
  inline void blendvps(const XmmVar& dst, const Mem& src)
  { _emitInstruction(kX86InstBlendVPS, &dst, &src); }

  //! @brief Dot Product of Packed DP-FP Values (SSE4.1).
  inline void dppd(const XmmVar& dst, const XmmVar& src, const Imm& imm8)
  { _emitInstruction(kX86InstDpPD, &dst, &src, &imm8); }
  //! @brief Dot Product of Packed DP-FP Values (SSE4.1).
  inline void dppd(const XmmVar& dst, const Mem& src, const Imm& imm8)
  { _emitInstruction(kX86InstDpPD, &dst, &src, &imm8); }

  //! @brief Dot Product of Packed SP-FP Values (SSE4.1).
  inline void dpps(const XmmVar& dst, const XmmVar& src, const Imm& imm8)
  { _emitInstruction(kX86InstDpPS, &dst, &src, &imm8); }
  //! @brief Dot Product of Packed SP-FP Values (SSE4.1).
  inline void dpps(const XmmVar& dst, const Mem& src, const Imm& imm8)
  { _emitInstruction(kX86InstDpPS, &dst, &src, &imm8); }

  //! @brief Extract Packed SP-FP Value (SSE4.1).
  inline void extractps(const XmmVar& dst, const XmmVar& src, const Imm& imm8)
  { _emitInstruction(kX86InstExtractPS, &dst, &src, &imm8); }
  //! @brief Extract Packed SP-FP Value (SSE4.1).
  inline void extractps(const XmmVar& dst, const Mem& src, const Imm& imm8)
  { _emitInstruction(kX86InstExtractPS, &dst, &src, &imm8); }

  //! @brief Load Double Quadword Non-Temporal Aligned Hint (SSE4.1).
  inline void movntdqa(const XmmVar& dst, const Mem& src)
  { _emitInstruction(kX86InstMovNTDQA, &dst, &src); }

  //! @brief Compute Multiple Packed Sums of Absolute Difference (SSE4.1).
  inline void mpsadbw(const XmmVar& dst, const XmmVar& src, const Imm& imm8)
  { _emitInstruction(kX86InstMPSADBW, &dst, &src, &imm8); }
  //! @brief Compute Multiple Packed Sums of Absolute Difference (SSE4.1).
  inline void mpsadbw(const XmmVar& dst, const Mem& src, const Imm& imm8)
  { _emitInstruction(kX86InstMPSADBW, &dst, &src, &imm8); }

  //! @brief Pack with Unsigned Saturation (SSE4.1).
  inline void packusdw(const XmmVar& dst, const XmmVar& src)
  { _emitInstruction(kX86InstPackUSDW, &dst, &src); }
  //! @brief Pack with Unsigned Saturation (SSE4.1).
  inline void packusdw(const XmmVar& dst, const Mem& src)
  { _emitInstruction(kX86InstPackUSDW, &dst, &src); }

  //! @brief Variable Blend Packed Bytes (SSE4.1).
  inline void pblendvb(const XmmVar& dst, const XmmVar& src)
  { _emitInstruction(kX86InstPBlendVB, &dst, &src); }
  //! @brief Variable Blend Packed Bytes (SSE4.1).
  inline void pblendvb(const XmmVar& dst, const Mem& src)
  { _emitInstruction(kX86InstPBlendVB, &dst, &src); }

  //! @brief Blend Packed Words (SSE4.1).
  inline void pblendw(const XmmVar& dst, const XmmVar& src, const Imm& imm8)
  { _emitInstruction(kX86InstPBlendW, &dst, &src, &imm8); }
  //! @brief Blend Packed Words (SSE4.1).
  inline void pblendw(const XmmVar& dst, const Mem& src, const Imm& imm8)
  { _emitInstruction(kX86InstPBlendW, &dst, &src, &imm8); }

  //! @brief Compare Packed Qword Data for Equal (SSE4.1).
  inline void pcmpeqq(const XmmVar& dst, const XmmVar& src)
  { _emitInstruction(kX86InstPCmpEqQ, &dst, &src); }
  //! @brief Compare Packed Qword Data for Equal (SSE4.1).
  inline void pcmpeqq(const XmmVar& dst, const Mem& src)
  { _emitInstruction(kX86InstPCmpEqQ, &dst, &src); }

  //! @brief Extract Byte (SSE4.1).
  inline void pextrb(const GpVar& dst, const XmmVar& src, const Imm& imm8)
  { _emitInstruction(kX86InstPExtrB, &dst, &src, &imm8); }
  //! @brief Extract Byte (SSE4.1).
  inline void pextrb(const Mem& dst, const XmmVar& src, const Imm& imm8)
  { _emitInstruction(kX86InstPExtrB, &dst, &src, &imm8); }

  //! @brief Extract Dword (SSE4.1).
  inline void pextrd(const GpVar& dst, const XmmVar& src, const Imm& imm8)
  { _emitInstruction(kX86InstPExtrD, &dst, &src, &imm8); }
  //! @brief Extract Dword (SSE4.1).
  inline void pextrd(const Mem& dst, const XmmVar& src, const Imm& imm8)
  { _emitInstruction(kX86InstPExtrD, &dst, &src, &imm8); }

  //! @brief Extract Dword (SSE4.1).
  inline void pextrq(const GpVar& dst, const XmmVar& src, const Imm& imm8)
  { _emitInstruction(kX86InstPExtrQ, &dst, &src, &imm8); }
  //! @brief Extract Dword (SSE4.1).
  inline void pextrq(const Mem& dst, const XmmVar& src, const Imm& imm8)
  { _emitInstruction(kX86InstPExtrQ, &dst, &src, &imm8); }

  //! @brief Packed Horizontal Word Minimum (SSE4.1).
  inline void phminposuw(const XmmVar& dst, const XmmVar& src)
  { _emitInstruction(kX86InstPHMinPOSUW, &dst, &src); }
  //! @brief Packed Horizontal Word Minimum (SSE4.1).
  inline void phminposuw(const XmmVar& dst, const Mem& src)
  { _emitInstruction(kX86InstPHMinPOSUW, &dst, &src); }

  //! @brief Insert Byte (SSE4.1).
  inline void pinsrb(const XmmVar& dst, const GpVar& src, const Imm& imm8)
  { _emitInstruction(kX86InstPInsRB, &dst, &src, &imm8); }
  //! @brief Insert Byte (SSE4.1).
  inline void pinsrb(const XmmVar& dst, const Mem& src, const Imm& imm8)
  { _emitInstruction(kX86InstPInsRB, &dst, &src, &imm8); }

  //! @brief Insert Dword (SSE4.1).
  inline void pinsrd(const XmmVar& dst, const GpVar& src, const Imm& imm8)
  { _emitInstruction(kX86InstPInsRD, &dst, &src, &imm8); }
  //! @brief Insert Dword (SSE4.1).
  inline void pinsrd(const XmmVar& dst, const Mem& src, const Imm& imm8)
  { _emitInstruction(kX86InstPInsRD, &dst, &src, &imm8); }

  //! @brief Insert Dword (SSE4.1).
  inline void pinsrq(const XmmVar& dst, const GpVar& src, const Imm& imm8)
  { _emitInstruction(kX86InstPInsRQ, &dst, &src, &imm8); }
  //! @brief Insert Dword (SSE4.1).
  inline void pinsrq(const XmmVar& dst, const Mem& src, const Imm& imm8)
  { _emitInstruction(kX86InstPInsRQ, &dst, &src, &imm8); }

  //! @brief Insert Word (SSE2).
  inline void pinsrw(const XmmVar& dst, const GpVar& src, const Imm& imm8)
  { _emitInstruction(kX86InstPInsRW, &dst, &src, &imm8); }
  //! @brief Insert Word (SSE2).
  inline void pinsrw(const XmmVar& dst, const Mem& src, const Imm& imm8)
  { _emitInstruction(kX86InstPInsRW, &dst, &src, &imm8); }

  //! @brief Maximum of Packed Word Integers (SSE4.1).
  inline void pmaxuw(const XmmVar& dst, const XmmVar& src)
  { _emitInstruction(kX86InstPMaxUW, &dst, &src); }
  //! @brief Maximum of Packed Word Integers (SSE4.1).
  inline void pmaxuw(const XmmVar& dst, const Mem& src)
  { _emitInstruction(kX86InstPMaxUW, &dst, &src); }

  //! @brief Maximum of Packed Signed Byte Integers (SSE4.1).
  inline void pmaxsb(const XmmVar& dst, const XmmVar& src)
  { _emitInstruction(kX86InstPMaxSB, &dst, &src); }
  //! @brief Maximum of Packed Signed Byte Integers (SSE4.1).
  inline void pmaxsb(const XmmVar& dst, const Mem& src)
  { _emitInstruction(kX86InstPMaxSB, &dst, &src); }

  //! @brief Maximum of Packed Signed Dword Integers (SSE4.1).
  inline void pmaxsd(const XmmVar& dst, const XmmVar& src)
  { _emitInstruction(kX86InstPMaxSD, &dst, &src); }
  //! @brief Maximum of Packed Signed Dword Integers (SSE4.1).
  inline void pmaxsd(const XmmVar& dst, const Mem& src)
  { _emitInstruction(kX86InstPMaxSD, &dst, &src); }

  //! @brief Maximum of Packed Unsigned Dword Integers (SSE4.1).
  inline void pmaxud(const XmmVar& dst, const XmmVar& src)
  { _emitInstruction(kX86InstPMaxUD, &dst, &src); }
  //! @brief Maximum of Packed Unsigned Dword Integers (SSE4.1).
  inline void pmaxud(const XmmVar& dst, const Mem& src)
  { _emitInstruction(kX86InstPMaxUD, &dst, &src); }

  //! @brief Minimum of Packed Signed Byte Integers (SSE4.1).
  inline void pminsb(const XmmVar& dst, const XmmVar& src)
  { _emitInstruction(kX86InstPMinSB, &dst, &src); }
  //! @brief Minimum of Packed Signed Byte Integers (SSE4.1).
  inline void pminsb(const XmmVar& dst, const Mem& src)
  { _emitInstruction(kX86InstPMinSB, &dst, &src); }

  //! @brief Minimum of Packed Word Integers (SSE4.1).
  inline void pminuw(const XmmVar& dst, const XmmVar& src)
  { _emitInstruction(kX86InstPMinUW, &dst, &src); }
  //! @brief Minimum of Packed Word Integers (SSE4.1).
  inline void pminuw(const XmmVar& dst, const Mem& src)
  { _emitInstruction(kX86InstPMinUW, &dst, &src); }

  //! @brief Minimum of Packed Dword Integers (SSE4.1).
  inline void pminud(const XmmVar& dst, const XmmVar& src)
  { _emitInstruction(kX86InstPMinUD, &dst, &src); }
  //! @brief Minimum of Packed Dword Integers (SSE4.1).
  inline void pminud(const XmmVar& dst, const Mem& src)
  { _emitInstruction(kX86InstPMinUD, &dst, &src); }

  //! @brief Minimum of Packed Dword Integers (SSE4.1).
  inline void pminsd(const XmmVar& dst, const XmmVar& src)
  { _emitInstruction(kX86InstPMinSD, &dst, &src); }
  //! @brief Minimum of Packed Dword Integers (SSE4.1).
  inline void pminsd(const XmmVar& dst, const Mem& src)
  { _emitInstruction(kX86InstPMinSD, &dst, &src); }

  //! @brief Packed Move with Sign Extend (SSE4.1).
  inline void pmovsxbw(const XmmVar& dst, const XmmVar& src)
  { _emitInstruction(kX86InstPMovSXBW, &dst, &src); }
  //! @brief Packed Move with Sign Extend (SSE4.1).
  inline void pmovsxbw(const XmmVar& dst, const Mem& src)
  { _emitInstruction(kX86InstPMovSXBW, &dst, &src); }

  //! @brief Packed Move with Sign Extend (SSE4.1).
  inline void pmovsxbd(const XmmVar& dst, const XmmVar& src)
  { _emitInstruction(kX86InstPMovSXBD, &dst, &src); }
  //! @brief Packed Move with Sign Extend (SSE4.1).
  inline void pmovsxbd(const XmmVar& dst, const Mem& src)
  { _emitInstruction(kX86InstPMovSXBD, &dst, &src); }

  //! @brief Packed Move with Sign Extend (SSE4.1).
  inline void pmovsxbq(const XmmVar& dst, const XmmVar& src)
  { _emitInstruction(kX86InstPMovSXBQ, &dst, &src); }
  //! @brief Packed Move with Sign Extend (SSE4.1).
  inline void pmovsxbq(const XmmVar& dst, const Mem& src)
  { _emitInstruction(kX86InstPMovSXBQ, &dst, &src); }

  //! @brief Packed Move with Sign Extend (SSE4.1).
  inline void pmovsxwd(const XmmVar& dst, const XmmVar& src)
  { _emitInstruction(kX86InstPMovSXWD, &dst, &src); }
  //! @brief Packed Move with Sign Extend (SSE4.1).
  inline void pmovsxwd(const XmmVar& dst, const Mem& src)
  { _emitInstruction(kX86InstPMovSXWD, &dst, &src); }

  //! @brief (SSE4.1).
  inline void pmovsxwq(const XmmVar& dst, const XmmVar& src)
  { _emitInstruction(kX86InstPMovSXWQ, &dst, &src); }
  //! @brief (SSE4.1).
  inline void pmovsxwq(const XmmVar& dst, const Mem& src)
  { _emitInstruction(kX86InstPMovSXWQ, &dst, &src); }

  //! @brief (SSE4.1).
  inline void pmovsxdq(const XmmVar& dst, const XmmVar& src)
  { _emitInstruction(kX86InstPMovSXDQ, &dst, &src); }
  //! @brief (SSE4.1).
  inline void pmovsxdq(const XmmVar& dst, const Mem& src)
  { _emitInstruction(kX86InstPMovSXDQ, &dst, &src); }

  //! @brief Packed Move with Zero Extend (SSE4.1).
  inline void pmovzxbw(const XmmVar& dst, const XmmVar& src)
  { _emitInstruction(kX86InstPMovZXBW, &dst, &src); }
  //! @brief Packed Move with Zero Extend (SSE4.1).
  inline void pmovzxbw(const XmmVar& dst, const Mem& src)
  { _emitInstruction(kX86InstPMovZXBW, &dst, &src); }

  //! @brief Packed Move with Zero Extend (SSE4.1).
  inline void pmovzxbd(const XmmVar& dst, const XmmVar& src)
  { _emitInstruction(kX86InstPMovZXBD, &dst, &src); }
  //! @brief Packed Move with Zero Extend (SSE4.1).
  inline void pmovzxbd(const XmmVar& dst, const Mem& src)
  { _emitInstruction(kX86InstPMovZXBD, &dst, &src); }

  //! @brief Packed Move with Zero Extend (SSE4.1).
  inline void pmovzxbq(const XmmVar& dst, const XmmVar& src)
  { _emitInstruction(kX86InstPMovZXBQ, &dst, &src); }
  //! @brief Packed Move with Zero Extend (SSE4.1).
  inline void pmovzxbq(const XmmVar& dst, const Mem& src)
  { _emitInstruction(kX86InstPMovZXBQ, &dst, &src); }

  //! @brief Packed Move with Zero Extend (SSE4.1).
  inline void pmovzxwd(const XmmVar& dst, const XmmVar& src)
  { _emitInstruction(kX86InstPMovZXWD, &dst, &src); }
  //! @brief Packed Move with Zero Extend (SSE4.1).
  inline void pmovzxwd(const XmmVar& dst, const Mem& src)
  { _emitInstruction(kX86InstPMovZXWD, &dst, &src); }

  //! @brief (SSE4.1).
  inline void pmovzxwq(const XmmVar& dst, const XmmVar& src)
  { _emitInstruction(kX86InstPMovZXWQ, &dst, &src); }
  //! @brief (SSE4.1).
  inline void pmovzxwq(const XmmVar& dst, const Mem& src)
  { _emitInstruction(kX86InstPMovZXWQ, &dst, &src); }

  //! @brief (SSE4.1).
  inline void pmovzxdq(const XmmVar& dst, const XmmVar& src)
  { _emitInstruction(kX86InstPMovZXDQ, &dst, &src); }
  //! @brief (SSE4.1).
  inline void pmovzxdq(const XmmVar& dst, const Mem& src)
  { _emitInstruction(kX86InstPMovZXDQ, &dst, &src); }

  //! @brief Multiply Packed Signed Dword Integers (SSE4.1).
  inline void pmuldq(const XmmVar& dst, const XmmVar& src)
  { _emitInstruction(kX86InstPMulDQ, &dst, &src); }
  //! @brief Multiply Packed Signed Dword Integers (SSE4.1).
  inline void pmuldq(const XmmVar& dst, const Mem& src)
  { _emitInstruction(kX86InstPMulDQ, &dst, &src); }

  //! @brief Multiply Packed Signed Integers and Store Low Result (SSE4.1).
  inline void pmulld(const XmmVar& dst, const XmmVar& src)
  { _emitInstruction(kX86InstPMulLD, &dst, &src); }
  //! @brief Multiply Packed Signed Integers and Store Low Result (SSE4.1).
  inline void pmulld(const XmmVar& dst, const Mem& src)
  { _emitInstruction(kX86InstPMulLD, &dst, &src); }

  //! @brief Logical Compare (SSE4.1).
  inline void ptest(const XmmVar& op1, const XmmVar& op2)
  { _emitInstruction(kX86InstPTest, &op1, &op2); }
  //! @brief Logical Compare (SSE4.1).
  inline void ptest(const XmmVar& op1, const Mem& op2)
  { _emitInstruction(kX86InstPTest, &op1, &op2); }

  //! Round Packed SP-FP Values @brief (SSE4.1).
  inline void roundps(const XmmVar& dst, const XmmVar& src, const Imm& imm8)
  { _emitInstruction(kX86InstRoundPS, &dst, &src, &imm8); }
  //! Round Packed SP-FP Values @brief (SSE4.1).
  inline void roundps(const XmmVar& dst, const Mem& src, const Imm& imm8)
  { _emitInstruction(kX86InstRoundPS, &dst, &src, &imm8); }

  //! @brief Round Scalar SP-FP Values (SSE4.1).
  inline void roundss(const XmmVar& dst, const XmmVar& src, const Imm& imm8)
  { _emitInstruction(kX86InstRoundSS, &dst, &src, &imm8); }
  //! @brief Round Scalar SP-FP Values (SSE4.1).
  inline void roundss(const XmmVar& dst, const Mem& src, const Imm& imm8)
  { _emitInstruction(kX86InstRoundSS, &dst, &src, &imm8); }

  //! @brief Round Packed DP-FP Values (SSE4.1).
  inline void roundpd(const XmmVar& dst, const XmmVar& src, const Imm& imm8)
  { _emitInstruction(kX86InstRoundPD, &dst, &src, &imm8); }
  //! @brief Round Packed DP-FP Values (SSE4.1).
  inline void roundpd(const XmmVar& dst, const Mem& src, const Imm& imm8)
  { _emitInstruction(kX86InstRoundPD, &dst, &src, &imm8); }

  //! @brief Round Scalar DP-FP Values (SSE4.1).
  inline void roundsd(const XmmVar& dst, const XmmVar& src, const Imm& imm8)
  { _emitInstruction(kX86InstRoundSD, &dst, &src, &imm8); }
  //! @brief Round Scalar DP-FP Values (SSE4.1).
  inline void roundsd(const XmmVar& dst, const Mem& src, const Imm& imm8)
  { _emitInstruction(kX86InstRoundSD, &dst, &src, &imm8); }

  // --------------------------------------------------------------------------
  // [SSE4.2]
  // --------------------------------------------------------------------------

  //! @brief Accumulate CRC32 Value (polynomial 0x11EDC6F41) (SSE4.2).
  inline void crc32(const GpVar& dst, const GpVar& src)
  { _emitInstruction(kX86InstCrc32, &dst, &src); }
  //! @brief Accumulate CRC32 Value (polynomial 0x11EDC6F41) (SSE4.2).
  inline void crc32(const GpVar& dst, const Mem& src)
  { _emitInstruction(kX86InstCrc32, &dst, &src); }

  //! @brief Packed Compare Explicit Length Strings, Return Index (SSE4.2).
  inline void pcmpestri(const XmmVar& dst, const XmmVar& src, const Imm& imm8)
  { _emitInstruction(kX86InstPCmpEStrI, &dst, &src, &imm8); }
  //! @brief Packed Compare Explicit Length Strings, Return Index (SSE4.2).
  inline void pcmpestri(const XmmVar& dst, const Mem& src, const Imm& imm8)
  { _emitInstruction(kX86InstPCmpEStrI, &dst, &src, &imm8); }

  //! @brief Packed Compare Explicit Length Strings, Return Mask (SSE4.2).
  inline void pcmpestrm(const XmmVar& dst, const XmmVar& src, const Imm& imm8)
  { _emitInstruction(kX86InstPCmpEStrM, &dst, &src, &imm8); }
  //! @brief Packed Compare Explicit Length Strings, Return Mask (SSE4.2).
  inline void pcmpestrm(const XmmVar& dst, const Mem& src, const Imm& imm8)
  { _emitInstruction(kX86InstPCmpEStrM, &dst, &src, &imm8); }

  //! @brief Packed Compare Implicit Length Strings, Return Index (SSE4.2).
  inline void pcmpistri(const XmmVar& dst, const XmmVar& src, const Imm& imm8)
  { _emitInstruction(kX86InstPCmpIStrI, &dst, &src, &imm8); }
  //! @brief Packed Compare Implicit Length Strings, Return Index (SSE4.2).
  inline void pcmpistri(const XmmVar& dst, const Mem& src, const Imm& imm8)
  { _emitInstruction(kX86InstPCmpIStrI, &dst, &src, &imm8); }

  //! @brief Packed Compare Implicit Length Strings, Return Mask (SSE4.2).
  inline void pcmpistrm(const XmmVar& dst, const XmmVar& src, const Imm& imm8)
  { _emitInstruction(kX86InstPCmpIStrM, &dst, &src, &imm8); }
  //! @brief Packed Compare Implicit Length Strings, Return Mask (SSE4.2).
  inline void pcmpistrm(const XmmVar& dst, const Mem& src, const Imm& imm8)
  { _emitInstruction(kX86InstPCmpIStrM, &dst, &src, &imm8); }

  //! @brief Compare Packed Data for Greater Than (SSE4.2).
  inline void pcmpgtq(const XmmVar& dst, const XmmVar& src)
  { _emitInstruction(kX86InstPCmpGtQ, &dst, &src); }
  //! @brief Compare Packed Data for Greater Than (SSE4.2).
  inline void pcmpgtq(const XmmVar& dst, const Mem& src)
  { _emitInstruction(kX86InstPCmpGtQ, &dst, &src); }

  //! @brief Return the Count of Number of Bits Set to 1 (SSE4.2).
  inline void popcnt(const GpVar& dst, const GpVar& src)
  { _emitInstruction(kX86InstPopCnt, &dst, &src); }
  //! @brief Return the Count of Number of Bits Set to 1 (SSE4.2).
  inline void popcnt(const GpVar& dst, const Mem& src)
  { _emitInstruction(kX86InstPopCnt, &dst, &src); }

  // --------------------------------------------------------------------------
  // [AMD only]
  // --------------------------------------------------------------------------

  //! @brief Prefetch (3dNow - Amd).
  //!
  //! Loads the entire 64-byte aligned memory sequence containing the
  //! specified memory address into the L1 data cache. The position of
  //! the specified memory address within the 64-byte cache line is
  //! irrelevant. If a cache hit occurs, or if a memory fault is detected,
  //! no bus cycle is initiated and the instruction is treated as a NOP.
  inline void amd_prefetch(const Mem& mem)
  { _emitInstruction(kX86InstAmdPrefetch, &mem); }

  //! @brief Prefetch and set cache to modified (3dNow - Amd).
  //!
  //! The PREFETCHW instruction loads the prefetched line and sets the
  //! cache-line state to Modified, in anticipation of subsequent data
  //! writes to the line. The PREFETCH instruction, by contrast, typically
  //! sets the cache-line state to Exclusive (depending on the hardware
  //! implementation).
  inline void amd_prefetchw(const Mem& mem)
  { _emitInstruction(kX86InstAmdPrefetchW, &mem); }

  // --------------------------------------------------------------------------
  // [Intel only]
  // --------------------------------------------------------------------------

  //! @brief Move Data After Swapping Bytes (SSE3 - Intel Atom).
  inline void movbe(const GpVar& dst, const Mem& src)
  {
    ASMJIT_ASSERT(!dst.isGpb());
    _emitInstruction(kX86InstMovBE, &dst, &src);
  }

  //! @brief Move Data After Swapping Bytes (SSE3 - Intel Atom).
  inline void movbe(const Mem& dst, const GpVar& src)
  {
    ASMJIT_ASSERT(!src.isGpb());
    _emitInstruction(kX86InstMovBE, &dst, &src);
  }

  // -------------------------------------------------------------------------
  // [Emit Options]
  // -------------------------------------------------------------------------

  //! @brief Assert LOCK# Signal Prefix.
  //!
  //! This instruction causes the processor's LOCK# signal to be asserted
  //! during execution of the accompanying instruction (turns the
  //! instruction into an atomic instruction). In a multiprocessor environment,
  //! the LOCK# signal insures that the processor has exclusive use of any shared
  //! memory while the signal is asserted.
  //!
  //! The LOCK prefix can be prepended only to the following instructions and
  //! to those forms of the instructions that use a memory operand: ADD, ADC,
  //! AND, BTC, BTR, BTS, CMPXCHG, DEC, INC, NEG, NOT, OR, SBB, SUB, XOR, XADD,
  //! and XCHG. An undefined opcode exception will be generated if the LOCK
  //! prefix is used with any other instruction. The XCHG instruction always
  //! asserts the LOCK# signal regardless of the presence or absence of the LOCK
  //! prefix.
  inline void lock()
  { _emitOptions |= kX86EmitOptionLock; }

  //! @brief Force REX prefix to be emitted.
  //!
  //! This option should be used carefully, because there are unencodable
  //! combinations. If you want to access ah, bh, ch or dh registers then you
  //! can't emit REX prefix and it will cause an illegal instruction error.
  //!
  //! @note REX prefix is only valid for X64/AMD64 platform.
  //!
  //! @sa @c kX86EmitOptionRex.
  inline void rex()
  { _emitOptions |= kX86EmitOptionRex; }
};

//! @}

} // AsmJit namespace

#undef ASMJIT_NOT_SUPPORTED_BY_COMPILER

// [Api-End]
#include "../core/apiend.h"

// [Guard]
#endif // _ASMJIT_X86_X86COMPILER_H
