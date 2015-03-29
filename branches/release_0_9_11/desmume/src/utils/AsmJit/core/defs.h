// [AsmJit]
// Complete JIT Assembler for C++ Language.
//
// [License]
// Zlib - See COPYING file in this package.

// [Guard]
#ifndef _ASMJIT_CORE_DEFS_H
#define _ASMJIT_CORE_DEFS_H

// [Dependencies - AsmJit]
#include "../core/build.h"

// [Api-Begin]
#include "../core/apibegin.h"

namespace AsmJit {

//! @addtogroup AsmJit_Core
//! @{

// ============================================================================
// [AsmJit::Global]
// ============================================================================

enum
{
  //! @brief Invalid operand identifier.
  kInvalidValue = 0xFFFFFFFFU,
  //! @brief Minimum reserved bytes in @ref Buffer.
  kBufferGrow = 32U
};

static const size_t kInvalidSize = (size_t)-1;

// ============================================================================
// [AsmJit::kStringBuilderOpType]
// ============================================================================

//! @brief String builder operation.
enum kStringBuilderOpType
{
  //! @brief Replace the current content by a given content.
  kStringBuilderOpSet = 0,
  //! @brief Append a given content to the current content.
  kStringBuilderOpAppend = 1
};

// ============================================================================
// [AsmJit::kStringBuilderNumType]
// ============================================================================

enum kStringBuilderNumFlags
{
  kStringBuilderNumShowSign = (1U << 0),
  kStringBuilderNumShowSpace = (1U << 1),
  kStringBuilderNumAlternate = (1U << 2),
  kStringBuilderNumSigned = (1U << 31)
};

// ============================================================================
// [AsmJit::kLoggerOption]
// ============================================================================

enum kLoggerFlag
{
  //! @brief Whether logger is enabled or disabled.
  //!
  //! Default @c true.
  kLoggerIsEnabled = 0x00000001,

  //! @brief Whether logger is enabled and can be used.
  //!
  //! This value can be set by inherited classes to inform @c Logger that
  //! assigned stream (or something that can log output) is invalid. If
  //! @c _used is false it means that there is no logging output and AsmJit
  //! shouldn't use this logger (because all messages will be lost).
  //!
  //! This is designed only to optimize cases that logger exists, but its
  //! configured not to output messages. The API inside Logging and AsmJit
  //! should only check this value when needed. The API outside AsmJit should
  //! check only whether logging is @c _enabled.
  //!
  //! Default @c true.
  kLoggerIsUsed = 0x00000002,

  //! @brief Whether to output instructions also in binary form.
  kLoggerOutputBinary = 0x00000010,
  //! @brief Whether to output immediates as hexadecimal numbers.
  kLoggerOutputHexImmediate = 0x00000020,
  //! @brief Whether to output displacements as hexadecimal numbers.
  kLoggerOutputHexDisplacement = 0x00000040
};

// ============================================================================
// [AsmJit::kCpu]
// ============================================================================

//! @brief Cpu vendor IDs.
//!
//! Cpu vendor IDs are specific for AsmJit library. Vendor ID is not directly
//! read from cpuid result, instead it's based on CPU vendor string.
enum kCpu
{
  //! @brief Unknown CPU vendor.
  kCpuUnknown = 0,

  //! @brief Intel CPU vendor.
  kCpuIntel = 1,
  //! @brief AMD CPU vendor.
  kCpuAmd = 2,
  //! @brief National Semiconductor CPU vendor (applies also to Cyrix processors).
  kCpuNSM = 3,
  //! @brief Transmeta CPU vendor.
  kCpuTransmeta = 4,
  //! @brief VIA CPU vendor.
  kCpuVia = 5
};

// ============================================================================
// [AsmJit::kMemAllocType]
// ============================================================================

//! @brief Types of allocation used by @c AsmJit::MemoryManager::alloc() method.
enum kMemAllocType
{
  //! @brief Allocate memory that can be freed by @c AsmJit::MemoryManager::free()
  //! method.
  kMemAllocFreeable = 0,
  //! @brief Allocate permanent memory that will be never freed.
  kMemAllocPermanent = 1
};

// ============================================================================
// [AsmJit::kOperandType]
// ============================================================================

//! @brief Operand types that can be encoded in @c Op operand.
enum kOperandType
{
  //! @brief Operand is none, used only internally (not initialized Operand).
  //!
  //! This operand is not valid.
  kOperandNone = 0x00,
  //! @brief Operand is label.
  kOperandLabel = 0x01,
  //! @brief Operand is register.
  kOperandReg = 0x02,
  //! @brief Operand is variable.
  kOperandVar = 0x04,
  //! @brief Operand is memory.
  kOperandMem = 0x08,
  //! @brief Operand is immediate.
  kOperandImm = 0x10
};

// ============================================================================
// [AsmJit::kOperandMemType]
// ============================================================================

//! @brief Type of memory operand.
enum kOperandMemType
{
  //! @brief Operand is combination of register(s) and displacement (native).
  kOperandMemNative = 0,
  //! @brief Operand is label.
  kOperandMemLabel = 1,
  //! @brief Operand is absolute memory location (supported mainly in 32-bit mode)
  kOperandMemAbsolute = 2,
};

// ============================================================================
// [AsmJit::kOperandId]
// ============================================================================

//! @brief Operand ID masks used to determine the operand type.
enum kOperandId
{
  //! @brief Operand id type mask (part used for operand type).
  kOperandIdTypeMask = 0xC0000000,
  //! @brief Label operand mark id.
  kOperandIdTypeLabel = 0x40000000,
  //! @brief Variable operand mark id.
  kOperandIdTypeVar = 0x80000000,

  //! @brief Operand id value mask (part used for IDs).
  kOperandIdValueMask = 0x3FFFFFFF
};

// ============================================================================
// [AsmJit::kRegType / kRegIndex]
// ============================================================================

enum
{
  //! @brief Mask for register type.
  kRegTypeMask = 0xFF00,

  //! @brief Mask for register code (index).
  kRegIndexMask = 0xFF,
  //! @brief Invalid register index.
  kRegIndexInvalid = 0xFF
};

// ============================================================================
// [AsmJit::kCondHint]
// ============================================================================

//! @brief Condition hint.
enum kCondHint
{
  //! @brief No hint.
  kCondHintNone = 0x00,
  //! @brief Condition is likely to be taken.
  kCondHintLikely = 0x01,
  //! @brief Condition is unlikely to be taken.
  kCondHintUnlikely = 0x02
};

// ============================================================================
// [AsmJit::kFuncAnonymous]
// ============================================================================

enum
{
  //! @brief Maximum allowed arguments per function declaration / call.
  kFuncArgsMax = 32,
  //! @brief Invalid stack offset in function or function parameter. 
  kFuncStackInvalid = -1
};

// ============================================================================
// [AsmJit::kFuncConv]
// ============================================================================

enum kFuncConv
{
  //! @brief Calling convention is invalid (can't be used).
  kFuncConvNone = 0
};

// ============================================================================
// [AsmJit::kFuncHint]
// ============================================================================

//! @brief Function hints.
enum kFuncHint
{
  //! @brief Make naked function (without using ebp/erp in prolog / epilog).
  kFuncHintNaked = 0
};

// ============================================================================
// [AsmJit::kFuncFlags]
// ============================================================================

//! @brief Function flags.
enum kFuncFlags
{
  //! @brief Whether another function is called from this function.
  //!
  //! If another function is called from this function, it's needed to prepare
  //! stack for it. If this member is true then it's likely that true will be
  //! also @c _isEspAdjusted one.
  kFuncFlagIsCaller = (1U << 0),

  //! @brief Whether the function is finished using @c Compiler::endFunc().
  kFuncFlagIsFinished = (1U << 1),

  //! @brief Whether the function is using naked (minimal) prolog / epilog.
  kFuncFlagIsNaked = (1U << 2)
};

// ============================================================================
// [AsmJit::kFuncArgsDirection]
// ============================================================================

//! @brief Function arguments direction.
enum kFuncArgsDirection
{
  //! @brief Arguments are passed left to right.
  //!
  //! This arguments direction is unusual to C programming, it's used by pascal
  //! compilers and in some calling conventions by Borland compiler).
  kFuncArgsLTR = 0,
  //! @brief Arguments are passed right ro left
  //!
  //! This is default argument direction in C programming.
  kFuncArgsRTL = 1
};

// ============================================================================
// [AsmJit::kInstCode]
// ============================================================================

enum kInstCode
{
  //! @brief No instruction.
  kInstNone = 0
};

// ============================================================================
// [AsmJit::kVarAllocFlags]
// ============================================================================

//! @brief Variable alloc mode.
enum kVarAllocFlags
{
  //! @brief Allocating variable to read only.
  //!
  //! Read only variables are used to optimize variable spilling. If variable
  //! is some time ago deallocated and it's not marked as changed (so it was
  //! all the life time read only) then spill is simply NOP (no mov instruction
  //! is generated to move it to it's home memory location).
  kVarAllocRead = 0x01,
  //! @brief Allocating variable to write only (overwrite).
  //!
  //! Overwriting means that if variable is in memory, there is no generated
  //! instruction to move variable from memory to register, because that
  //! register will be overwritten by next instruction. This is used as a
  //! simple optimization to improve generated code by @c Compiler.
  kVarAllocWrite = 0x02,
  //! @brief Allocating variable to read / write.
  //!
  //! Variable allocated for read / write is marked as changed. This means that
  //! if variable must be later spilled into memory, mov (or similar)
  //! instruction will be generated.
  kVarAllocReadWrite = 0x03,

  //! @brief Variable can be allocated in register.
  kVarAllocRegister = 0x04,
  //! @brief Variable can be allocated only to a special register.
  kVarAllocSpecial = 0x08,

  //! @brief Variable can be allocated in memory.
  kVarAllocMem = 0x10,

  //! @brief Unuse the variable after use.
  kVarAllocUnuseAfterUse = 0x20
};

// ============================================================================
// [AsmJit::kVarHint]
// ============================================================================

//! @brief Variable hint (used by @ref Compiler).
//!
//! @sa @ref Compiler.
enum kVarHint
{
  //! @brief Alloc variable.
  kVarHintAlloc = 0,
  //! @brief Spill variable.
  kVarHintSpill = 1,
  //! @brief Save variable if modified.
  kVarHintSave = 2,
  //! @brief Save variable if modified and mark it as unused.
  kVarHintSaveAndUnuse = 3,
  //! @brief Mark variable as unused.
  kVarHintUnuse = 4
};

// ============================================================================
// [AsmJit::kVarPolicy]
// ============================================================================

//! @brief Variable allocation method.
//!
//! Variable allocation method is used by compiler and it means if compiler
//! should first allocate preserved registers or not. Preserved registers are
//! registers that must be saved / restored by generated function.
//!
//! This option is for people who are calling C/C++ functions from JIT code so
//! Compiler can recude generating push/pop sequences before and after call,
//! respectively.
enum kVarPolicy
{
  //! @brief Allocate preserved registers first.
  kVarPolicyPreservedFirst = 0,
  //! @brief Allocate preserved registers last (default).
  kVarPolicyPreservedLast = 1
};

// ============================================================================
// [AsmJit::kVarState]
// ============================================================================

//! @brief State of variable.
//!
//! @note State of variable is used only during make process and it's not
//! visible to the developer.
enum kVarState
{
  //! @brief Variable is currently not used.
  kVarStateUnused = 0,
  //! @brief Variable is in register.
  //!
  //! Variable is currently allocated in register.
  kVarStateReg = 1,
  //! @brief Variable is in memory location or spilled.
  //!
  //! Variable was spilled from register to memory or variable is used for
  //! memory only storage.
  kVarStateMem = 2
};

// ============================================================================
// [AsmJit::kVarType]
// ============================================================================

enum kVarType
{
  //! @brief Invalid variable type.
  kVarTypeInvalid = 0xFF
};

// ============================================================================
// [AsmJit::kScale]
// ============================================================================

//! @brief Scale which can be used for addressing (it the target instruction
//! supports it).
//!
//! See @c Op and addressing methods like @c byte_ptr(), @c word_ptr(),
//! @c dword_ptr(), etc...
enum kScale
{
  //! @brief No scale.
  kScaleNone = 0,
  //! @brief Scale 2 times (same as shifting to left by 1).
  kScale2Times = 1,
  //! @brief Scale 4 times (same as shifting to left by 2).
  kScale4Times = 2,
  //! @brief Scale 8 times (same as shifting to left by 3).
  kScale8Times = 3
};

// ============================================================================
// [AsmJit::kSize]
// ============================================================================

//! @brief Size of registers and pointers.
enum kSize
{
  //! @brief 1 byte size.
  kSizeByte   = 1,
  //! @brief 2 bytes size.
  kSizeWord   = 2,
  //! @brief 4 bytes size.
  kSizeDWord  = 4,
  //! @brief 8 bytes size.
  kSizeQWord  = 8,
  //! @brief 10 bytes size.
  kSizeTWord  = 10,
  //! @brief 16 bytes size.
  kSizeDQWord = 16
};

// ============================================================================
// [AsmJit::kRelocMode]
// ============================================================================

enum kRelocMode
{
  kRelocAbsToAbs = 0,
  kRelocRelToAbs = 1,
  kRelocAbsToRel = 2,
  kRelocTrampoline = 3
};

// ============================================================================
// [AsmJit::kCompilerItem]
// ============================================================================

//! @brief Type of @ref CompilerItem.
//!
//! Each @c CompilerItem contains information about its type. Compiler can 
//! optimize instruction stream by analyzing items and each type is hint
//! for it. The most used/serialized items are instructions
//! (@c kCompilerItemInst).
enum kCompilerItem
{
  //! @brief Invalid item (can't be used).
  kCompilerItemNone = 0,
  //! @brief Item is mark, see @ref CompilerMark.
  kCompilerItemMark,
  //! @brief Item is comment, see @ref CompilerComment.
  kCompilerItemComment,
  //! @brief Item is embedded data, see @ref CompilerEmbed.
  kCompilerItemEmbed,
  //! @brief Item is .align directive, see @ref CompilerAlign.
  kCompilerItemAlign,
  //! @brief Item is variable hint (alloc, spill, use, unuse), see @ref CompilerHint.
  kCompilerItemHint,
  //! @brief Item is instruction, see @ref CompilerInst.
  kCompilerItemInst,
  //! @brief Item is target, see @ref CompilerTarget.
  kCompilerItemTarget,
  //! @brief Item is function call, see @ref CompilerFuncCall.
  kCompilerItemFuncCall,
  //! @brief Item is function declaration, see @ref CompilerFuncDecl.
  kCompilerItemFuncDecl,
  //! @brief Item is an end of the function, see @ref CompilerFuncEnd.
  kCompilerItemFuncEnd,
  //! @brief Item is function return, see @ref CompilerFuncRet.
  kCompilerItemFuncRet
};

// ============================================================================
// [AsmJit::kError]
// ============================================================================

//! @brief Error codes.
enum kError
{
  //! @brief No error (success).
  //!
  //! This is default state and state you want.
  kErrorOk = 0,

  //! @brief Memory allocation error (@c ASMJIT_MALLOC returned @c NULL).
  kErrorNoHeapMemory = 1,
  //! @brief Virtual memory allocation error (@c VirtualMemory returned @c NULL).
  kErrorNoVirtualMemory = 2,

  //! @brief Unknown instruction. This happens only if instruction code is
  //! out of bounds. Shouldn't happen.
  kErrorUnknownInstruction = 3,
  //! @brief Illegal instruction, usually generated by AsmJit::Assembler
  //! class when emitting instruction opcode. If this error is generated the
  //! target buffer is not affected by this invalid instruction.
  //!
  //! You can also get this error code if you are under x64 (64-bit x86) and
  //! you tried to decode instruction using AH, BH, CH or DH register with REX
  //! prefix. These registers can't be accessed if REX prefix is used and AsmJit
  //! didn't check for this situation in intrinsics (@c Compiler takes care of
  //! this and rearrange registers if needed).
  //!
  //! Examples that will raise @c kErrorIllegalInstruction error (a is
  //! @c Assembler instance):
  //!
  //! @code
  //! a.mov(dword_ptr(eax), al); // Invalid address size.
  //! a.mov(byte_ptr(r10), ah);  // Undecodable instruction (AH used with r10
  //!                            // which can be encoded only using REX prefix)
  //! @endcode
  //!
  //! @note In debug mode you get assertion failure instead of setting error
  //! code.
  kErrorIllegalInstruction = 4,
  //! @brief Illegal addressing used (unencodable).
  kErrorIllegalAddressing = 5,
  //! @brief Short jump instruction used, but displacement is out of bounds.
  kErrorIllegalShortJump = 6,

  //! @brief No function defined.
  kErrorNoFunction = 7,
  //! @brief Function generation is not finished by using @c Compiler::endFunc()
  //! or something bad happened during generation related to function. This can
  //! be missing compiler item, etc...
  kErrorIncompleteFunction = 8,

  //! @brief Compiler can't allocate registers, because all of them are used.
  //!
  //! @note AsmJit is able to spill registers so this error really shouldn't
  //! happen unless all registers have priority 0 (which means never spill).
  kErrorNoRegisters = 9,
  //! @brief Compiler can't allocate one register to multiple destinations.
  //!
  //! This error can only happen using special instructions like cmpxchg8b and
  //! others where there are more destination operands (implicit).
  kErrorOverlappedRegisters = 10,

  //! @brief Tried to call function using incompatible argument.
  kErrorIncompatibleArgumentType = 11,
  //! @brief Incompatible return value.
  kErrorIncompatibleReturnType = 12,

  //! @brief Count of error codes by AsmJit. Can grow in future.
  kErrorCount
};

// ============================================================================
// [AsmJit::API]
// ============================================================================

//! @brief Translates error code (see @c kError) into text representation.
ASMJIT_API const char* getErrorString(uint32_t error);

//! @}

} // AsmJit namespace

// [Api-End]
#include "../core/apiend.h"

// [Guard]
#endif // _ASMJIT_CORE_DEFS_H
