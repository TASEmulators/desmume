// [AsmJit]
// Complete JIT Assembler for C++ Language.
//
// [License]
// Zlib - See COPYING file in this package.

// [Guard]
#ifndef _ASMJIT_X86_X86DEFS_H
#define _ASMJIT_X86_X86DEFS_H

// [Dependencies - AsmJit]
#include "../core/assert.h"
#include "../core/defs.h"

// [Api-Begin]
#include "../core/apibegin.h"

namespace AsmJit {

//! @addtogroup AsmJit_X86
//! @{

// ============================================================================
// [AsmJit::kX86Feature]
// ============================================================================

//! @brief X86 CPU features.
enum kX86Feature
{
  //! @brief Cpu has RDTSC instruction.
  kX86FeatureRdtsc = 1U << 0,
  //! @brief Cpu has RDTSCP instruction.
  kX86FeatureRdtscP = 1U << 1,
  //! @brief Cpu has CMOV instruction (conditional move)
  kX86FeatureCMov = 1U << 2,
  //! @brief Cpu has CMPXCHG8B instruction
  kX86FeatureCmpXchg8B = 1U << 3,
  //! @brief Cpu has CMPXCHG16B instruction (64-bit processors)
  kX86FeatureCmpXchg16B = 1U << 4,
  //! @brief Cpu has CLFUSH instruction
  kX86FeatureClFlush = 1U << 5,
  //! @brief Cpu has PREFETCH instruction
  kX86FeaturePrefetch = 1U << 6,
  //! @brief Cpu supports LAHF and SAHF instrictions.
  kX86FeatureLahfSahf = 1U << 7,
  //! @brief Cpu supports FXSAVE and FXRSTOR instructions.
  kX86FeatureFXSR = 1U << 8,
  //! @brief Cpu supports FXSAVE and FXRSTOR instruction optimizations (FFXSR).
  kX86FeatureFFXSR = 1U << 9,
  //! @brief Cpu has MMX.
  kX86FeatureMmx = 1U << 10,
  //! @brief Cpu has extended MMX.
  kX86FeatureMmxExt = 1U << 11,
  //! @brief Cpu has 3dNow!
  kX86Feature3dNow = 1U << 12,
  //! @brief Cpu has enchanced 3dNow!
  kX86Feature3dNowExt = 1U << 13,
  //! @brief Cpu has SSE.
  kX86FeatureSse = 1U << 14,
  //! @brief Cpu has SSE2.
  kX86FeatureSse2 = 1U << 15,
  //! @brief Cpu has SSE3.
  kX86FeatureSse3 = 1U << 16,
  //! @brief Cpu has Supplemental SSE3 (SSSE3).
  kX86FeatureSsse3 = 1U << 17,
  //! @brief Cpu has SSE4.A.
  kX86FeatureSse4A = 1U << 18,
  //! @brief Cpu has SSE4.1.
  kX86FeatureSse41 = 1U << 19,
  //! @brief Cpu has SSE4.2.
  kX86FeatureSse42 = 1U << 20,
  //! @brief Cpu has AVX.
  kX86FeatureAvx = 1U << 22,
  //! @brief Cpu has Misaligned SSE (MSSE).
  kX86FeatureMSse = 1U << 23,
  //! @brief Cpu supports MONITOR and MWAIT instructions.
  kX86FeatureMonitorMWait = 1U << 24,
  //! @brief Cpu supports MOVBE instruction.
  kX86FeatureMovBE = 1U << 25,
  //! @brief Cpu supports POPCNT instruction.
  kX86FeaturePopCnt = 1U << 26,
  //! @brief Cpu supports LZCNT instruction.
  kX86FeatureLzCnt = 1U << 27,
  //! @brief Cpu supports PCLMULDQ set of instructions.
  kX86FeaturePclMulDQ  = 1U << 28,
  //! @brief Cpu supports multithreading.
  kX86FeatureMultiThreading = 1U << 29,
  //! @brief Cpu supports execute disable bit (execute protection).
  kX86FeatureExecuteDisableBit = 1U << 30,
  //! @brief 64-bit CPU.
  kX86Feature64Bit = 1U << 31
};

// ============================================================================
// [AsmJit::kX86Bug]
// ============================================================================

//! @brief X86 CPU bugs.
enum kX86Bug
{
  //! @brief Whether the processor contains bug seen in some 
  //! AMD-Opteron processors.
  kX86BugAmdLockMB = 1U << 0
};

// ============================================================================
// [AsmJit::kX86Property]
// ============================================================================

//! @brief @ref X86Assembler and @ref X86Compiler properties.
enum kX86Property
{
  //! @brief Optimize align for current processor.
  //!
  //! Default: @c true.
  kX86PropertyOptimizedAlign = 0,

  //! @brief Emit hints added to jcc() instructions.
  //!
  //! Default: @c true.
  kX86PropertyJumpHints = 1
};

// ============================================================================
// [AsmJit::kX86Seg]
// ============================================================================

//! @brief X86 segment codes.
enum kX86Seg
{
  // DO NOT MODIFY INDEX CODES - They are used by _emitSegmentPrefix() and
  // by logger in the following order:

  //! @brief ES segment.
  kX86SegEs = 0,
  //! @brief CS segment.
  kX86SegCs = 1,
  //! @brief SS segment.
  kX86SegSs = 2,
  //! @brief DS segment.
  kX86SegDs = 3,
  //! @brief FS segment.
  kX86SegFs = 4,
  //! @brief GS segment.
  kX86SegGs = 5,
  //! @brief Count of segments.
  kX86SegCount = 6,

  //! @brief No segment override prefix.
  kX86SegNone = 0xF
};

// ============================================================================
// [AsmJit::kX86RegNum]
// ============================================================================

//! @brief X86 registers count.
//!
//! Count of general purpose registers and XMM registers depends on current
//! mode. If application is compiled for 32-bit platform then this number is 8,
//! 64-bit platforms have 8 extra general purpose and XMM registers (16 total).
enum kX86RegNum
{
  //! @var kX86RegNumBase
  //!
  //! Count of general purpose registers and XMM registers depends on current
  //! bit-mode. If application is compiled for 32-bit platform then this number
  //! is 8, 64-bit platforms have 8 extra general purpose and XMM registers (16
  //! total).
#if defined(ASMJIT_X86)
  kX86RegNumBase = 8,
#else
  kX86RegNumBase = 16,
#endif // ASMJIT

  //! @brief Count of general purpose registers.
  //!
  //! 8 in 32-bit mode and 16 in 64-bit mode.
  kX86RegNumGp = kX86RegNumBase,

  //! @brief Count of FPU stack registers (always 8).
  kX86RegNumX87 = 8,
  //! @brief Count of MM registers (always 8).
  kX86RegNumMm = 8,

  //! @brief Count of XMM registers.
  //!
  //! 8 in 32-bit mode and 16 in 64-bit mode.
  kX86RegNumXmm = kX86RegNumBase,
  //! @brief Count of YMM registers.
  //!
  //! 8 in 32-bit mode and 16 in 64-bit mode.
  kX86RegNumYmm = kX86RegNumBase,

  //! @brief Count of segment registers, including no segment (AsmJit specific).
  //!
  //! @note There are 6 segment registers, but AsmJit uses 0 as no segment, and
  //! 1...6 as segment registers, this means that there are 7 segment registers
  //! in AsmJit API, but only 6 can be used through @c Assembler or @c Compiler
  //! API.
  kX86RegNumSeg = 7
};

//! @brief X86 register types.
enum kX86RegType
{
  // First byte contains register type (mask 0xFF00), Second byte contains
  // register index code.

  // --------------------------------------------------------------------------
  // [GP Register Types]
  // --------------------------------------------------------------------------

  //! @brief 8-bit general purpose register type (LO).
  kX86RegTypeGpbLo = 0x0100,
  //! @brief 8-bit general purpose register type (HI, only AH, BH, CH, DH).
  kX86RegTypeGpbHi = 0x0200,
  //! @brief 16-bit general purpose register type.
  kX86RegTypeGpw = 0x1000,
  //! @brief 32-bit general purpose register type.
  kX86RegTypeGpd = 0x2000,
  //! @brief 64-bit general purpose register type.
  kX86RegTypeGpq = 0x3000,

  //! @var kX86RegTypeGpz
  //! @brief 32-bit or 64-bit general purpose register type.
#if defined(ASMJIT_X86)
  kX86RegTypeGpz = kX86RegTypeGpd,
#else
  kX86RegTypeGpz = kX86RegTypeGpq,
#endif

  //! @brief X87 (FPU) register type.
  kX86RegTypeX87 = 0x5000,
  //! @brief 64-bit MM register type.
  kX86RegTypeMm = 0x6000,

  //! @brief 128-bit XMM register type.
  kX86RegTypeXmm = 0x7000,
  //! @brief 256-bit YMM register type.
  kX86RegTypeYmm = 0x8000,

  //! @brief 16-bit segment register type.
  kX86RegTypeSeg = 0xD000
};

// ============================================================================
// [AsmJit::kX86RegIndex]
// ============================================================================

//! @brief X86 register indices.
//!
//! These codes are real, don't miss with @c REG enum! and don't use these
//! values if you are not writing AsmJit code.
enum kX86RegIndex
{
  //! @brief ID for AX/EAX/RAX registers.
  kX86RegIndexEax = 0,
  //! @brief ID for CX/ECX/RCX registers.
  kX86RegIndexEcx = 1,
  //! @brief ID for DX/EDX/RDX registers.
  kX86RegIndexEdx = 2,
  //! @brief ID for BX/EBX/RBX registers.
  kX86RegIndexEbx = 3,
  //! @brief ID for SP/ESP/RSP registers.
  kX86RegIndexEsp = 4,
  //! @brief ID for BP/EBP/RBP registers.
  kX86RegIndexEbp = 5,
  //! @brief ID for SI/ESI/RSI registers.
  kX86RegIndexEsi = 6,
  //! @brief ID for DI/EDI/RDI registers.
  kX86RegIndexEdi = 7,

  //! @brief ID for AX/EAX/RAX registers.
  kX86RegIndexRax = 0,
  //! @brief ID for CX/ECX/RCX registers.
  kX86RegIndexRcx = 1,
  //! @brief ID for DX/EDX/RDX registers.
  kX86RegIndexRdx = 2,
  //! @brief ID for BX/EBX/RBX registers.
  kX86RegIndexRbx = 3,
  //! @brief ID for SP/ESP/RSP registers.
  kX86RegIndexRsp = 4,
  //! @brief ID for BP/EBP/RBP registers.
  kX86RegIndexRbp = 5,
  //! @brief ID for SI/ESI/RSI registers.
  kX86RegIndexRsi = 6,
  //! @brief ID for DI/EDI/RDI registers.
  kX86RegIndexRdi = 7,

  //! @brief ID for r8 register (additional register introduced by 64-bit architecture).
  kX86RegIndexR8 = 8,
  //! @brief ID for R9 register (additional register introduced by 64-bit architecture).
  kX86RegIndexR9 = 9,
  //! @brief ID for R10 register (additional register introduced by 64-bit architecture).
  kX86RegIndexR10 = 10,
  //! @brief ID for R11 register (additional register introduced by 64-bit architecture).
  kX86RegIndexR11 = 11,
  //! @brief ID for R12 register (additional register introduced by 64-bit architecture).
  kX86RegIndexR12 = 12,
  //! @brief ID for R13 register (additional register introduced by 64-bit architecture).
  kX86RegIndexR13 = 13,
  //! @brief ID for R14 register (additional register introduced by 64-bit architecture).
  kX86RegIndexR14 = 14,
  //! @brief ID for R15 register (additional register introduced by 64-bit architecture).
  kX86RegIndexR15 = 15,

  //! @brief ID for mm0 register.
  kX86RegIndexMm0 = 0,
  //! @brief ID for mm1 register.
  kX86RegIndexMm1 = 1,
  //! @brief ID for mm2 register.
  kX86RegIndexMm2 = 2,
  //! @brief ID for mm3 register.
  kX86RegIndexMm3 = 3,
  //! @brief ID for mm4 register.
  kX86RegIndexMm4 = 4,
  //! @brief ID for mm5 register.
  kX86RegIndexMm5 = 5,
  //! @brief ID for mm6 register.
  kX86RegIndexMm6 = 6,
  //! @brief ID for mm7 register.
  kX86RegIndexMm7 = 7,

  //! @brief ID for xmm0 register.
  kX86RegIndexXmm0 = 0,
  //! @brief ID for xmm1 register.
  kX86RegIndexXmm1 = 1,
  //! @brief ID for xmm2 register.
  kX86RegIndexXmm2 = 2,
  //! @brief ID for xmm3 register.
  kX86RegIndexXmm3 = 3,
  //! @brief ID for xmm4 register.
  kX86RegIndexXmm4 = 4,
  //! @brief ID for xmm5 register.
  kX86RegIndexXmm5 = 5,
  //! @brief ID for xmm6 register.
  kX86RegIndexXmm6 = 6,
  //! @brief ID for xmm7 register.
  kX86RegIndexXmm7 = 7,

  //! @brief ID for xmm8 register (additional register introduced by 64-bit architecture).
  kX86RegIndexXmm8 = 8,
  //! @brief ID for xmm9 register (additional register introduced by 64-bit architecture).
  kX86RegIndexXmm9 = 9,
  //! @brief ID for xmm10 register (additional register introduced by 64-bit architecture).
  kX86RegIndexXmm10 = 10,
  //! @brief ID for xmm11 register (additional register introduced by 64-bit architecture).
  kX86RegIndexXmm11 = 11,
  //! @brief ID for xmm12 register (additional register introduced by 64-bit architecture).
  kX86RegIndexXmm12 = 12,
  //! @brief ID for xmm13 register (additional register introduced by 64-bit architecture).
  kX86RegIndexXmm13 = 13,
  //! @brief ID for xmm14 register (additional register introduced by 64-bit architecture).
  kX86RegIndexXmm14 = 14,
  //! @brief ID for xmm15 register (additional register introduced by 64-bit architecture).
  kX86RegIndexXmm15 = 15,

  //! @brief ID for ES segment register.
  kX86RegIndexEs = 0,
  //! @brief ID for CS segment register.
  kX86RegIndexCs = 1,
  //! @brief ID for SS segment register.
  kX86RegIndexSs = 2,
  //! @brief ID for DS segment register.
  kX86RegIndexDs = 3,
  //! @brief ID for FS segment register.
  kX86RegIndexFs = 4,
  //! @brief ID for GS segment register.
  kX86RegIndexGs = 5
};

// ============================================================================
// [AsmJit::kX86RegCode]
// ============================================================================

//! @brief X86 pseudo (not real X86) register codes used for generating opcodes.
//!
//! From this register code can be generated real x86 register ID, type of
//! register and size of register.
enum kX86RegCode
{
  // --------------------------------------------------------------------------
  // [8-bit Registers]
  // --------------------------------------------------------------------------

  kX86RegAl = kX86RegTypeGpbLo,
  kX86RegCl,
  kX86RegDl,
  kX86RegBl,
#if defined(ASMJIT_X64)
  kX86RegSpl,
  kX86RegBpl,
  kX86RegSil,
  kX86RegDil,
#endif // ASMJIT_X64

#if defined(ASMJIT_X64)
  kX86RegR8b,
  kX86RegR9b,
  kX86RegR10b,
  kX86RegR11b,
  kX86RegR12b,
  kX86RegR13b,
  kX86RegR14b,
  kX86RegR15b,
#endif // ASMJIT_X64

  kX86RegAh = kX86RegTypeGpbHi,
  kX86RegCh,
  kX86RegDh,
  kX86RegBh,

  // --------------------------------------------------------------------------
  // [16-bit Registers]
  // --------------------------------------------------------------------------

  kX86RegAx = kX86RegTypeGpw,
  kX86RegCx,
  kX86RegDx,
  kX86RegBx,
  kX86RegSp,
  kX86RegBp,
  kX86RegSi,
  kX86RegDi,
#if defined(ASMJIT_X64)
  kX86RegR8w,
  kX86RegR9w,
  kX86RegR10w,
  kX86RegR11w,
  kX86RegR12w,
  kX86RegR13w,
  kX86RegR14w,
  kX86RegR15w,
#endif // ASMJIT_X64

  // --------------------------------------------------------------------------
  // [32-bit Registers]
  // --------------------------------------------------------------------------

  kX86RegEax = kX86RegTypeGpd,
  kX86RegEcx,
  kX86RegEdx,
  kX86RegEbx,
  kX86RegEsp,
  kX86RegEbp,
  kX86RegEsi,
  kX86RegEdi,
#if defined(ASMJIT_X64)
  kX86RegR8d,
  kX86RegR9d,
  kX86RegR10d,
  kX86RegR11d,
  kX86RegR12d,
  kX86RegR13d,
  kX86RegR14d,
  kX86RegR15d,
#endif // ASMJIT_X64

  // --------------------------------------------------------------------------
  // [64-bit Registers]
  // --------------------------------------------------------------------------

#if defined(ASMJIT_X64)
  kX86RegRax = kX86RegTypeGpq,
  kX86RegRcx,
  kX86RegRdx,
  kX86RegRbx,
  kX86RegRsp,
  kX86RegRbp,
  kX86RegRsi,
  kX86RegRdi,
  kX86RegR8,
  kX86RegR9,
  kX86RegR10,
  kX86RegR11,
  kX86RegR12,
  kX86RegR13,
  kX86RegR14,
  kX86RegR15,
#endif // ASMJIT_X64

  // --------------------------------------------------------------------------
  // [MM Registers]
  // --------------------------------------------------------------------------

  kX86RegMm0 = kX86RegTypeMm,
  kX86RegMm1,
  kX86RegMm2,
  kX86RegMm3,
  kX86RegMm4,
  kX86RegMm5,
  kX86RegMm6,
  kX86RegMm7,

  // --------------------------------------------------------------------------
  // [XMM Registers]
  // --------------------------------------------------------------------------

  kX86RegXmm0 = kX86RegTypeXmm,
  kX86RegXmm1,
  kX86RegXmm2,
  kX86RegXmm3,
  kX86RegXmm4,
  kX86RegXmm5,
  kX86RegXmm6,
  kX86RegXmm7,
#if defined(ASMJIT_X64)
  kX86RegXmm8,
  kX86RegXmm9,
  kX86RegXmm10,
  kX86RegXmm11,
  kX86RegXmm12,
  kX86RegXmm13,
  kX86RegXmm14,
  kX86RegXmm15,
#endif // ASMJIT_X64

  // --------------------------------------------------------------------------
  // [Native registers (depends on 32-bit or 64-bit mode)]
  // --------------------------------------------------------------------------

  kX86RegZax = kX86RegTypeGpz,
  kX86RegZcx,
  kX86RegZdx,
  kX86RegZbx,
  kX86RegZsp,
  kX86RegZbp,
  kX86RegZsi,
  kX86RegZdi,

  // --------------------------------------------------------------------------
  // [Segment registers]
  // --------------------------------------------------------------------------

  //! @brief ES segment register.
  kX86RegEs = kX86RegTypeSeg,
  //! @brief CS segment register.
  kX86RegCs,
  //! @brief SS segment register.
  kX86RegSs,
  //! @brief DS segment register.
  kX86RegDs,
  //! @brief FS segment register.
  kX86RegFs,
  //! @brief GS segment register.
  kX86RegGs
};

// ============================================================================
// [AsmJit::kX86Cond]
// ============================================================================

//! @brief X86 Condition codes.
enum kX86Cond
{
  // Condition codes from processor manuals.
  kX86CondA               = 0x07,
  kX86CondAE              = 0x03,
  kX86CondB               = 0x02,
  kX86CondBE              = 0x06,
  kX86CondC               = 0x02,
  kX86CondE               = 0x04,
  kX86CondG               = 0x0F,
  kX86CondGE              = 0x0D,
  kX86CondL               = 0x0C,
  kX86CondLE              = 0x0E,
  kX86CondNA              = 0x06,
  kX86CondNAE             = 0x02,
  kX86CondNB              = 0x03,
  kX86CondNBE             = 0x07,
  kX86CondNC              = 0x03,
  kX86CondNE              = 0x05,
  kX86CondNG              = 0x0E,
  kX86CondNGE             = 0x0C,
  kX86CondNL              = 0x0D,
  kX86CondNLE             = 0x0F,
  kX86CondNO              = 0x01,
  kX86CondNP              = 0x0B,
  kX86CondNS              = 0x09,
  kX86CondNZ              = 0x05,
  kX86CondO               = 0x00,
  kX86CondP               = 0x0A,
  kX86CondPE              = 0x0A,
  kX86CondPO              = 0x0B,
  kX86CondS               = 0x08,
  kX86CondZ               = 0x04,

  // Simplified condition codes.
  kX86CondOverflow        = 0x00,
  kX86CondNotOverflow     = 0x01,
  kX86CondBelow           = 0x02,
  kX86CondAboveEqual      = 0x03,
  kX86CondEqual           = 0x04,
  kX86CondNotEqual        = 0x05,
  kX86CondBelowEqual      = 0x06,
  kX86CondAbove           = 0x07,
  kX86CondSign            = 0x08,
  kX86CondNotSign         = 0x09,
  kX86CondParityEven      = 0x0A,
  kX86CondParityOdd       = 0x0B,
  kX86CondLess            = 0x0C,
  kX86CondGreaterEqual    = 0x0D,
  kX86CondLessEqual       = 0x0E,
  kX86CondGreater         = 0x0F,

  // Aliases.
  kX86CondZero            = 0x04,
  kX86CondNotZero         = 0x05,
  kX86CondNegative        = 0x08,
  kX86CondPositive        = 0x09,

  // X87 floating point only.
  kX86CondFpuUnordered    = 0x10,
  kX86CondFpuNotUnordered = 0x11,

  //! @brief No condition code.
  kX86CondNone            = 0x12
};

// ============================================================================
// [AsmJit::kX86CondPrefix]
// ============================================================================

//! @brief X86 condition hint prefix code, see @ref kCondHint.
enum kX86CondPrefix
{
  //! @brief Condition is likely to be taken.
  kX86CondPrefixLikely = 0x3E,
  //! @brief Condition is unlikely to be taken.
  kX86CondPrefixUnlikely = 0x2E
};

// ============================================================================
// [AsmJit::kX86PrefetchHint]
// ============================================================================

//! @brief X86 Prefetch hints.
enum kX86PrefetchHint
{
  //! @brief Prefetch using NT hint.
  kX86PrefetchNta = 0,
  //! @brief Prefetch to L0 cache.
  kX86PrefetchT0 = 1,
  //! @brief Prefetch to L1 cache.
  kX86PrefetchT1 = 2,
  //! @brief Prefetch to L2 cache.
  kX86PrefetchT2 = 3
};

// ============================================================================
// [AsmJit::kX86FPSW]
// ============================================================================

//! @brief X86 FPU status-word.
enum kX86FPSW
{
  kX86FPSW_Invalid        = 0x0001,
  kX86FPSW_Denormalized   = 0x0002,
  kX86FPSW_DivByZero      = 0x0004,
  kX86FPSW_Overflow       = 0x0008,
  kX86FPSW_Underflow      = 0x0010,
  kX86FPSW_Precision      = 0x0020,
  kX86FPSW_StackFault     = 0x0040,
  kX86FPSW_Interrupt      = 0x0080,
  kX86FPSW_C0             = 0x0100,
  kX86FPSW_C1             = 0x0200,
  kX86FPSW_C2             = 0x0400,
  kX86FPSW_Top            = 0x3800,
  kX86FPSW_C3             = 0x4000,
  kX86FPSW_Busy           = 0x8000
};

// ============================================================================
// [AsmJit::kX86FPCW]
// ============================================================================

//! @brief X86 FPU control-word.
enum kX86FPCW
{
  // --------------------------------------------------------------------------
  // [Exception-Mask]
  // --------------------------------------------------------------------------

  kX86FPCW_EM_Mask        = 0x003F, // Bits 0-5.

  kX86FPCW_EM_Invalid     = 0x0001,
  kX86FPCW_EM_Denormal    = 0x0002,
  kX86FPCW_EM_DivByZero   = 0x0004,
  kX86FPCW_EM_Overflow    = 0x0008,
  kX86FPCW_EM_Underflow   = 0x0010,
  kX86FPCW_EM_Inexact     = 0x0020,

  // --------------------------------------------------------------------------
  // [Precision-Control]
  // --------------------------------------------------------------------------

  kX86FPCW_PC_Mask        = 0x0300, // Bits 8-9.

  kX86FPCW_PC_Float       = 0x0000,
  kX86FPCW_PC_Reserved    = 0x0100,
  kX86FPCW_PC_Double      = 0x0200,
  kX86FPCW_PC_Extended    = 0x0300,

  // --------------------------------------------------------------------------
  // [Rounding-Control]
  // --------------------------------------------------------------------------

  kX86FPCW_RC_Mask        = 0x0C00, // Bits 10-11.

  kX86FPCW_RC_Nearest     = 0x0000,
  kX86FPCW_RC_Down        = 0x0400,
  kX86FPCW_RC_Up          = 0x0800,
  kX86FPCW_RC_Truncate    = 0x0C00,

  // --------------------------------------------------------------------------
  // [Infinity-Control]
  // --------------------------------------------------------------------------

  kX86FPCW_IC_Mask        = 0x1000, // Bit 12.

  kX86FPCW_IC_Projective  = 0x0000,
  kX86FPCW_IC_Affine      = 0x1000
};

// ============================================================================
// [AsmJit::kX86EmitOption]
// ============================================================================

//! @brief Emit options, mainly for internal purposes.
enum kX86EmitOption
{
  //! @brief Force REX prefix to be emitted.
  //!
  //! This option should be used carefully, because there are unencodable
  //! combinations. If you want to access ah, bh, ch or dh registers then you
  //! can't emit REX prefix and it will cause an illegal instruction error.
  kX86EmitOptionRex = 0x1,

  //! @brief Tell @c Assembler or @c Compiler to emit and validate lock prefix.
  //!
  //! If this option is used and instruction doesn't support LOCK prefix then
  //! invalid instruction error is generated.
  kX86EmitOptionLock = 0x2,

  //! @brief Emit short/near jump or conditional jump instead of far one to
  //! some bytes.
  //!
  //! @note This option could be dangerous in case that the short jump is not
  //! possible (displacement can't fit into signed 8-bit integer). AsmJit can
  //! automatically generate back short jumps, but always generates long forward
  //! jumps, because the information about the code size between the instruction
  //! and target is not known.
  kX86EmitOptionShortJump = 0x4,

  //! @brief Emit full immediate instead of BYTE in all cases.
  //!
  //! @note AsmJit is able to emit both forms of immediate value. In case that
  //! the instruction supports short form and immediate can fit into a signed 
  //! 8-bit integer short for is preferred, but if for any reason the full form
  //! is required it can be overridden by using this option.
  kX86EmitOptionFullImmediate = 0x8
};

// ============================================================================
// [AsmJit::kX86InstCode]
// ============================================================================

//! @brief X86 instruction codes.
//!
//! Note that these instruction codes are AsmJit specific. Each instruction is
//! unique ID into AsmJit instruction table. Instruction codes are used together
//! with AsmJit::Assembler and you can also use instruction codes to serialize
//! instructions by @ref Assembler::_emitInstruction() or
//! @ref Compiler::_emitInstruction()
enum kX86InstCode
{
  kX86InstAdc = 1,         // X86/X64
  kX86InstAdd,             // X86/X64
  kX86InstAddPD,           // SSE2
  kX86InstAddPS,           // SSE
  kX86InstAddSD,           // SSE2
  kX86InstAddSS,           // SSE
  kX86InstAddSubPD,        // SSE3
  kX86InstAddSubPS,        // SSE3
  kX86InstAmdPrefetch,     // 3dNow!
  kX86InstAmdPrefetchW,    // 3dNow!
  kX86InstAnd,             // X86/X64
  kX86InstAndnPD,          // SSE2
  kX86InstAndnPS,          // SSE
  kX86InstAndPD,           // SSE2
  kX86InstAndPS,           // SSE
  kX86InstBlendPD,         // SSE4.1
  kX86InstBlendPS,         // SSE4.1
  kX86InstBlendVPD,        // SSE4.1
  kX86InstBlendVPS,        // SSE4.1
  kX86InstBsf,             // X86/X64
  kX86InstBsr,             // X86/X64
  kX86InstBSwap,           // X86/X64 (i486)
  kX86InstBt,              // X86/X64
  kX86InstBtc,             // X86/X64
  kX86InstBtr,             // X86/X64
  kX86InstBts,             // X86/X64
  kX86InstCall,            // X86/X64
  kX86InstCbw,             // X86/X64
  kX86InstCdq,             // X86/X64
  kX86InstCdqe,            // X64 only
  kX86InstClc,             // X86/X64
  kX86InstCld,             // X86/X64
  kX86InstClFlush,         // SSE2
  kX86InstCmc,             // X86/X64

  kX86InstCMov,            // Begin (cmovcc) (i586)
  kX86InstCMovA=kX86InstCMov,//X86/X64 (cmovcc) (i586)
  kX86InstCMovAE,          // X86/X64 (cmovcc) (i586)
  kX86InstCMovB,           // X86/X64 (cmovcc) (i586)
  kX86InstCMovBE,          // X86/X64 (cmovcc) (i586)
  kX86InstCMovC,           // X86/X64 (cmovcc) (i586)
  kX86InstCMovE,           // X86/X64 (cmovcc) (i586)
  kX86InstCMovG,           // X86/X64 (cmovcc) (i586)
  kX86InstCMovGE,          // X86/X64 (cmovcc) (i586)
  kX86InstCMovL,           // X86/X64 (cmovcc) (i586)
  kX86InstCMovLE,          // X86/X64 (cmovcc) (i586)
  kX86InstCMovNA,          // X86/X64 (cmovcc) (i586)
  kX86InstCMovNAE,         // X86/X64 (cmovcc) (i586)
  kX86InstCMovNB,          // X86/X64 (cmovcc) (i586)
  kX86InstCMovNBE,         // X86/X64 (cmovcc) (i586)
  kX86InstCMovNC,          // X86/X64 (cmovcc) (i586)
  kX86InstCMovNE,          // X86/X64 (cmovcc) (i586)
  kX86InstCMovNG,          // X86/X64 (cmovcc) (i586)
  kX86InstCMovNGE,         // X86/X64 (cmovcc) (i586)
  kX86InstCMovNL,          // X86/X64 (cmovcc) (i586)
  kX86InstCMovNLE,         // X86/X64 (cmovcc) (i586)
  kX86InstCMovNO,          // X86/X64 (cmovcc) (i586)
  kX86InstCMovNP,          // X86/X64 (cmovcc) (i586)
  kX86InstCMovNS,          // X86/X64 (cmovcc) (i586)
  kX86InstCMovNZ,          // X86/X64 (cmovcc) (i586)
  kX86InstCMovO,           // X86/X64 (cmovcc) (i586)
  kX86InstCMovP,           // X86/X64 (cmovcc) (i586)
  kX86InstCMovPE,          // X86/X64 (cmovcc) (i586)
  kX86InstCMovPO,          // X86/X64 (cmovcc) (i586)
  kX86InstCMovS,           // X86/X64 (cmovcc) (i586)
  kX86InstCMovZ,           // X86/X64 (cmovcc) (i586)

  kX86InstCmp,             // X86/X64
  kX86InstCmpPD,           // SSE2
  kX86InstCmpPS,           // SSE
  kX86InstCmpSD,           // SSE2
  kX86InstCmpSS,           // SSE
  kX86InstCmpXCHG,         // X86/X64 (i486)
  kX86InstCmpXCHG16B,      // X64 only
  kX86InstCmpXCHG8B,       // X86/X64 (i586)
  kX86InstComISD,          // SSE2
  kX86InstComISS,          // SSE
  kX86InstCpuId,           // X86/X64 (i486)
  kX86InstCqo,             // X64 only
  kX86InstCrc32,           // SSE4.2
  kX86InstCvtDQ2PD,        // SSE2
  kX86InstCvtDQ2PS,        // SSE2
  kX86InstCvtPD2DQ,        // SSE2
  kX86InstCvtPD2PI,        // SSE2
  kX86InstCvtPD2PS,        // SSE2
  kX86InstCvtPI2PD,        // SSE2
  kX86InstCvtPI2PS,        // SSE
  kX86InstCvtPS2DQ,        // SSE2
  kX86InstCvtPS2PD,        // SSE2
  kX86InstCvtPS2PI,        // SSE
  kX86InstCvtSD2SI,        // SSE2
  kX86InstCvtSD2SS,        // SSE2
  kX86InstCvtSI2SD,        // SSE2
  kX86InstCvtSI2SS,        // SSE
  kX86InstCvtSS2SD,        // SSE2
  kX86InstCvtSS2SI,        // SSE
  kX86InstCvttPD2DQ,       // SSE2
  kX86InstCvttPD2PI,       // SSE2
  kX86InstCvttPS2DQ,       // SSE2
  kX86InstCvttPS2PI,       // SSE
  kX86InstCvttSD2SI,       // SSE2
  kX86InstCvttSS2SI,       // SSE
  kX86InstCwd,             // X86/X64
  kX86InstCwde,            // X86/X64
  kX86InstDaa,             // X86 only
  kX86InstDas,             // X86 only
  kX86InstDec,             // X86/X64
  kX86InstDiv,             // X86/X64
  kX86InstDivPD,           // SSE2
  kX86InstDivPS,           // SSE
  kX86InstDivSD,           // SSE2
  kX86InstDivSS,           // SSE
  kX86InstDpPD,            // SSE4.1
  kX86InstDpPS,            // SSE4.1
  kX86InstEmms,            // MMX
  kX86InstEnter,           // X86/X64
  kX86InstExtractPS,       // SSE4.1
  kX86InstF2XM1,           // X87
  kX86InstFAbs,            // X87
  kX86InstFAdd,            // X87
  kX86InstFAddP,           // X87
  kX86InstFBLd,            // X87
  kX86InstFBStP,           // X87
  kX86InstFCHS,            // X87
  kX86InstFClex,           // X87
  kX86InstFCMovB,          // X87
  kX86InstFCMovBE,         // X87
  kX86InstFCMovE,          // X87
  kX86InstFCMovNB,         // X87
  kX86InstFCMovNBE,        // X87
  kX86InstFCMovNE,         // X87
  kX86InstFCMovNU,         // X87
  kX86InstFCMovU,          // X87
  kX86InstFCom,            // X87
  kX86InstFComI,           // X87
  kX86InstFComIP,          // X87
  kX86InstFComP,           // X87
  kX86InstFComPP,          // X87
  kX86InstFCos,            // X87
  kX86InstFDecStP,         // X87
  kX86InstFDiv,            // X87
  kX86InstFDivP,           // X87
  kX86InstFDivR,           // X87
  kX86InstFDivRP,          // X87
  kX86InstFEmms,           // 3dNow!
  kX86InstFFree,           // X87
  kX86InstFIAdd,           // X87
  kX86InstFICom,           // X87
  kX86InstFIComP,          // X87
  kX86InstFIDiv,           // X87
  kX86InstFIDivR,          // X87
  kX86InstFILd,            // X87
  kX86InstFIMul,           // X87
  kX86InstFIncStP,         // X87
  kX86InstFInit,           // X87
  kX86InstFISt,            // X87
  kX86InstFIStP,           // X87
  kX86InstFISttP,          // SSE3
  kX86InstFISub,           // X87
  kX86InstFISubR,          // X87
  kX86InstFLd,             // X87
  kX86InstFLd1,            // X87
  kX86InstFLdCw,           // X87
  kX86InstFLdEnv,          // X87
  kX86InstFLdL2E,          // X87
  kX86InstFLdL2T,          // X87
  kX86InstFLdLg2,          // X87
  kX86InstFLdLn2,          // X87
  kX86InstFLdPi,           // X87
  kX86InstFLdZ,            // X87
  kX86InstFMul,            // X87
  kX86InstFMulP,           // X87
  kX86InstFNClex,          // X87
  kX86InstFNInit,          // X87
  kX86InstFNop,            // X87
  kX86InstFNSave,          // X87
  kX86InstFNStCw,          // X87
  kX86InstFNStEnv,         // X87
  kX86InstFNStSw,          // X87
  kX86InstFPAtan,          // X87
  kX86InstFPRem,           // X87
  kX86InstFPRem1,          // X87
  kX86InstFPTan,           // X87
  kX86InstFRndInt,         // X87
  kX86InstFRstor,          // X87
  kX86InstFSave,           // X87
  kX86InstFScale,          // X87
  kX86InstFSin,            // X87
  kX86InstFSinCos,         // X87
  kX86InstFSqrt,           // X87
  kX86InstFSt,             // X87
  kX86InstFStCw,           // X87
  kX86InstFStEnv,          // X87
  kX86InstFStP,            // X87
  kX86InstFStSw,           // X87
  kX86InstFSub,            // X87
  kX86InstFSubP,           // X87
  kX86InstFSubR,           // X87
  kX86InstFSubRP,          // X87
  kX86InstFTst,            // X87
  kX86InstFUCom,           // X87
  kX86InstFUComI,          // X87
  kX86InstFUComIP,         // X87
  kX86InstFUComP,          // X87
  kX86InstFUComPP,         // X87
  kX86InstFWait,           // X87
  kX86InstFXam,            // X87
  kX86InstFXch,            // X87
  kX86InstFXRstor,         // X87
  kX86InstFXSave,          // X87
  kX86InstFXtract,         // X87
  kX86InstFYL2X,           // X87
  kX86InstFYL2XP1,         // X87
  kX86InstHAddPD,          // SSE3
  kX86InstHAddPS,          // SSE3
  kX86InstHSubPD,          // SSE3
  kX86InstHSubPS,          // SSE3
  kX86InstIDiv,            // X86/X64
  kX86InstIMul,            // X86/X64
  kX86InstInc,             // X86/X64
  kX86InstInt3,            // X86/X64
  kX86InstJ,               // Begin (jcc)
  kX86InstJA = kX86InstJ,  // X86/X64 (jcc)
  kX86InstJAE,             // X86/X64 (jcc)
  kX86InstJB,              // X86/X64 (jcc)
  kX86InstJBE,             // X86/X64 (jcc)
  kX86InstJC,              // X86/X64 (jcc)
  kX86InstJE,              // X86/X64 (jcc)
  kX86InstJG,              // X86/X64 (jcc)
  kX86InstJGE,             // X86/X64 (jcc)
  kX86InstJL,              // X86/X64 (jcc)
  kX86InstJLE,             // X86/X64 (jcc)
  kX86InstJNA,             // X86/X64 (jcc)
  kX86InstJNAE,            // X86/X64 (jcc)
  kX86InstJNB,             // X86/X64 (jcc)
  kX86InstJNBE,            // X86/X64 (jcc)
  kX86InstJNC,             // X86/X64 (jcc)
  kX86InstJNE,             // X86/X64 (jcc)
  kX86InstJNG,             // X86/X64 (jcc)
  kX86InstJNGE,            // X86/X64 (jcc)
  kX86InstJNL,             // X86/X64 (jcc)
  kX86InstJNLE,            // X86/X64 (jcc)
  kX86InstJNO,             // X86/X64 (jcc)
  kX86InstJNP,             // X86/X64 (jcc)
  kX86InstJNS,             // X86/X64 (jcc)
  kX86InstJNZ,             // X86/X64 (jcc)
  kX86InstJO,              // X86/X64 (jcc)
  kX86InstJP,              // X86/X64 (jcc)
  kX86InstJPE,             // X86/X64 (jcc)
  kX86InstJPO,             // X86/X64 (jcc)
  kX86InstJS,              // X86/X64 (jcc)
  kX86InstJZ,              // X86/X64 (jcc)
  kX86InstJmp,             // X86/X64 (jmp)
  kX86InstLdDQU,           // SSE3
  kX86InstLdMXCSR,         // SSE
  kX86InstLahf,            // X86/X64 (CPUID NEEDED)
  kX86InstLea,             // X86/X64
  kX86InstLeave,           // X86/X64
  kX86InstLFence,          // SSE2
  kX86InstMaskMovDQU,      // SSE2
  kX86InstMaskMovQ,        // MMX-Ext
  kX86InstMaxPD,           // SSE2
  kX86InstMaxPS,           // SSE
  kX86InstMaxSD,           // SSE2
  kX86InstMaxSS,           // SSE
  kX86InstMFence,          // SSE2
  kX86InstMinPD,           // SSE2
  kX86InstMinPS,           // SSE
  kX86InstMinSD,           // SSE2
  kX86InstMinSS,           // SSE
  kX86InstMonitor,         // SSE3
  kX86InstMov,             // X86/X64
  kX86InstMovAPD,          // SSE2
  kX86InstMovAPS,          // SSE
  kX86InstMovBE,           // SSE3 - Intel-Atom
  kX86InstMovD,            // MMX/SSE2
  kX86InstMovDDup,         // SSE3
  kX86InstMovDQ2Q,         // SSE2
  kX86InstMovDQA,          // SSE2
  kX86InstMovDQU,          // SSE2
  kX86InstMovHLPS,         // SSE
  kX86InstMovHPD,          // SSE2
  kX86InstMovHPS,          // SSE
  kX86InstMovLHPS,         // SSE
  kX86InstMovLPD,          // SSE2
  kX86InstMovLPS,          // SSE
  kX86InstMovMskPD,        // SSE2
  kX86InstMovMskPS,        // SSE2
  kX86InstMovNTDQ,         // SSE2
  kX86InstMovNTDQA,        // SSE4.1
  kX86InstMovNTI,          // SSE2
  kX86InstMovNTPD,         // SSE2
  kX86InstMovNTPS,         // SSE
  kX86InstMovNTQ,          // MMX-Ext
  kX86InstMovQ,            // MMX/SSE/SSE2
  kX86InstMovQ2DQ,         // SSE2
  kX86InstMovSD,           // SSE2
  kX86InstMovSHDup,        // SSE3
  kX86InstMovSLDup,        // SSE3
  kX86InstMovSS,           // SSE
  kX86InstMovSX,           // X86/X64
  kX86InstMovSXD,          // X86/X64
  kX86InstMovUPD,          // SSE2
  kX86InstMovUPS,          // SSE
  kX86InstMovZX,           // X86/X64
  kX86InstMovPtr,          // X86/X64
  kX86InstMPSADBW,         // SSE4.1
  kX86InstMul,             // X86/X64
  kX86InstMulPD,           // SSE2
  kX86InstMulPS,           // SSE
  kX86InstMulSD,           // SSE2
  kX86InstMulSS,           // SSE
  kX86InstMWait,           // SSE3
  kX86InstNeg,             // X86/X64
  kX86InstNop,             // X86/X64
  kX86InstNot,             // X86/X64
  kX86InstOr,              // X86/X64
  kX86InstOrPD,            // SSE2
  kX86InstOrPS,            // SSE
  kX86InstPAbsB,           // SSSE3
  kX86InstPAbsD,           // SSSE3
  kX86InstPAbsW,           // SSSE3
  kX86InstPackSSDW,        // MMX/SSE2
  kX86InstPackSSWB,        // MMX/SSE2
  kX86InstPackUSDW,        // SSE4.1
  kX86InstPackUSWB,        // MMX/SSE2
  kX86InstPAddB,           // MMX/SSE2
  kX86InstPAddD,           // MMX/SSE2
  kX86InstPAddQ,           // SSE2
  kX86InstPAddSB,          // MMX/SSE2
  kX86InstPAddSW,          // MMX/SSE2
  kX86InstPAddUSB,         // MMX/SSE2
  kX86InstPAddUSW,         // MMX/SSE2
  kX86InstPAddW,           // MMX/SSE2
  kX86InstPAlignR,         // SSSE3
  kX86InstPAnd,            // MMX/SSE2
  kX86InstPAndN,           // MMX/SSE2
  kX86InstPause,           // SSE2.
  kX86InstPAvgB,           // MMX-Ext
  kX86InstPAvgW,           // MMX-Ext
  kX86InstPBlendVB,        // SSE4.1
  kX86InstPBlendW,         // SSE4.1
  kX86InstPCmpEqB,         // MMX/SSE2
  kX86InstPCmpEqD,         // MMX/SSE2
  kX86InstPCmpEqQ,         // SSE4.1
  kX86InstPCmpEqW,         // MMX/SSE2    
  kX86InstPCmpEStrI,       // SSE4.2
  kX86InstPCmpEStrM,       // SSE4.2
  kX86InstPCmpGtB,         // MMX/SSE2
  kX86InstPCmpGtD,         // MMX/SSE2
  kX86InstPCmpGtQ,         // SSE4.2
  kX86InstPCmpGtW,         // MMX/SSE2
  kX86InstPCmpIStrI,       // SSE4.2
  kX86InstPCmpIStrM,       // SSE4.2
  kX86InstPExtrB,          // SSE4.1
  kX86InstPExtrD,          // SSE4.1        
  kX86InstPExtrQ,          // SSE4.1
  kX86InstPExtrW,          // MMX-Ext/SSE2
  kX86InstPF2ID,           // 3dNow!
  kX86InstPF2IW,           // Enhanced 3dNow!
  kX86InstPFAcc,           // 3dNow!
  kX86InstPFAdd,           // 3dNow!
  kX86InstPFCmpEQ,         // 3dNow!
  kX86InstPFCmpGE,         // 3dNow!
  kX86InstPFCmpGT,         // 3dNow!
  kX86InstPFMax,           // 3dNow!
  kX86InstPFMin,           // 3dNow!
  kX86InstPFMul,           // 3dNow!
  kX86InstPFNAcc,          // Enhanced 3dNow!
  kX86InstPFPNAcc,         // Enhanced 3dNow!
  kX86InstPFRcp,           // 3dNow!
  kX86InstPFRcpIt1,        // 3dNow!
  kX86InstPFRcpIt2,        // 3dNow!
  kX86InstPFRSqIt1,        // 3dNow!
  kX86InstPFRSqrt,         // 3dNow!
  kX86InstPFSub,           // 3dNow!
  kX86InstPFSubR,          // 3dNow!
  kX86InstPHAddD,          // SSSE3
  kX86InstPHAddSW,         // SSSE3
  kX86InstPHAddW,          // SSSE3
  kX86InstPHMinPOSUW,      // SSE4.1
  kX86InstPHSubD,          // SSSE3
  kX86InstPHSubSW,         // SSSE3
  kX86InstPHSubW,          // SSSE3
  kX86InstPI2FD,           // 3dNow!
  kX86InstPI2FW,           // Enhanced 3dNow!
  kX86InstPInsRB,          // SSE4.1
  kX86InstPInsRD,          // SSE4.1
  kX86InstPInsRQ,          // SSE4.1
  kX86InstPInsRW,          // MMX-Ext
  kX86InstPMAddUBSW,       // SSSE3
  kX86InstPMAddWD,         // MMX/SSE2
  kX86InstPMaxSB,          // SSE4.1
  kX86InstPMaxSD,          // SSE4.1
  kX86InstPMaxSW,          // MMX-Ext
  kX86InstPMaxUB,          // MMX-Ext
  kX86InstPMaxUD,          // SSE4.1
  kX86InstPMaxUW,          // SSE4.1
  kX86InstPMinSB,          // SSE4.1
  kX86InstPMinSD,          // SSE4.1
  kX86InstPMinSW,          // MMX-Ext
  kX86InstPMinUB,          // MMX-Ext
  kX86InstPMinUD,          // SSE4.1
  kX86InstPMinUW,          // SSE4.1
  kX86InstPMovMskB,        // MMX-Ext
  kX86InstPMovSXBD,        // SSE4.1
  kX86InstPMovSXBQ,        // SSE4.1
  kX86InstPMovSXBW,        // SSE4.1
  kX86InstPMovSXDQ,        // SSE4.1
  kX86InstPMovSXWD,        // SSE4.1
  kX86InstPMovSXWQ,        // SSE4.1
  kX86InstPMovZXBD,        // SSE4.1
  kX86InstPMovZXBQ,        // SSE4.1
  kX86InstPMovZXBW,        // SSE4.1
  kX86InstPMovZXDQ,        // SSE4.1
  kX86InstPMovZXWD,        // SSE4.1
  kX86InstPMovZXWQ,        // SSE4.1
  kX86InstPMulDQ,          // SSE4.1
  kX86InstPMulHRSW,        // SSSE3
  kX86InstPMulHUW,         // MMX-Ext
  kX86InstPMulHW,          // MMX/SSE2
  kX86InstPMulLD,          // SSE4.1
  kX86InstPMulLW,          // MMX/SSE2
  kX86InstPMulUDQ,         // SSE2
  kX86InstPop,             // X86/X64
  kX86InstPopAD,           // X86 only
  kX86InstPopCnt,          // SSE4.2
  kX86InstPopFD,           // X86 only
  kX86InstPopFQ,           // X64 only
  kX86InstPOr,             // MMX/SSE2
  kX86InstPrefetch,        // MMX-Ext
  kX86InstPSADBW,          // MMX-Ext
  kX86InstPShufB,          // SSSE3
  kX86InstPShufD,          // SSE2
  kX86InstPShufW,          // MMX-Ext
  kX86InstPShufHW,         // SSE2
  kX86InstPShufLW,         // SSE2
  kX86InstPSignB,          // SSSE3
  kX86InstPSignD,          // SSSE3
  kX86InstPSignW,          // SSSE3
  kX86InstPSllD,           // MMX/SSE2
  kX86InstPSllDQ,          // SSE2
  kX86InstPSllQ,           // MMX/SSE2
  kX86InstPSllW,           // MMX/SSE2
  kX86InstPSraD,           // MMX/SSE2
  kX86InstPSraW,           // MMX/SSE2
  kX86InstPSrlD,           // MMX/SSE2
  kX86InstPSrlDQ,          // SSE2
  kX86InstPSrlQ,           // MMX/SSE2
  kX86InstPSrlW,           // MMX/SSE2
  kX86InstPSubB,           // MMX/SSE2
  kX86InstPSubD,           // MMX/SSE2
  kX86InstPSubQ,           // SSE2
  kX86InstPSubSB,          // MMX/SSE2
  kX86InstPSubSW,          // MMX/SSE2
  kX86InstPSubUSB,         // MMX/SSE2
  kX86InstPSubUSW,         // MMX/SSE2
  kX86InstPSubW,           // MMX/SSE2
  kX86InstPSwapD,          // Enhanced 3dNow!
  kX86InstPTest,           // SSE4.1
  kX86InstPunpckHBW,       // MMX/SSE2
  kX86InstPunpckHDQ,       // MMX/SSE2
  kX86InstPunpckHQDQ,      // SSE2
  kX86InstPunpckHWD,       // MMX/SSE2
  kX86InstPunpckLBW,       // MMX/SSE2
  kX86InstPunpckLDQ,       // MMX/SSE2
  kX86InstPunpckLQDQ,      // SSE2
  kX86InstPunpckLWD,       // MMX/SSE2
  kX86InstPush,            // X86/X64
  kX86InstPushAD,          // X86 only
  kX86InstPushFD,          // X86 only
  kX86InstPushFQ,          // X64 only
  kX86InstPXor,            // MMX/SSE2
  kX86InstRcl,             // X86/X64
  kX86InstRcpPS,           // SSE
  kX86InstRcpSS,           // SSE
  kX86InstRcr,             // X86/X64
  kX86InstRdtsc,           // X86/X64
  kX86InstRdtscP,          // X86/X64
  kX86InstRepLodSB,        // X86/X64 (REP)
  kX86InstRepLodSD,        // X86/X64 (REP)
  kX86InstRepLodSQ,        // X64 only (REP)
  kX86InstRepLodSW,        // X86/X64 (REP)
  kX86InstRepMovSB,        // X86/X64 (REP)
  kX86InstRepMovSD,        // X86/X64 (REP)
  kX86InstRepMovSQ,        // X64 only (REP)
  kX86InstRepMovSW,        // X86/X64 (REP)
  kX86InstRepStoSB,        // X86/X64 (REP)
  kX86InstRepStoSD,        // X86/X64 (REP)
  kX86InstRepStoSQ,        // X64 only (REP)
  kX86InstRepStoSW,        // X86/X64 (REP)
  kX86InstRepECmpSB,       // X86/X64 (REP)
  kX86InstRepECmpSD,       // X86/X64 (REP)
  kX86InstRepECmpSQ,       // X64 only (REP)
  kX86InstRepECmpSW,       // X86/X64 (REP)
  kX86InstRepEScaSB,       // X86/X64 (REP)
  kX86InstRepEScaSD,       // X86/X64 (REP)
  kX86InstRepEScaSQ,       // X64 only (REP)
  kX86InstRepEScaSW,       // X86/X64 (REP)
  kX86InstRepNECmpSB,      // X86/X64 (REP)
  kX86InstRepNECmpSD,      // X86/X64 (REP)
  kX86InstRepNECmpSQ,      // X64 only (REP)
  kX86InstRepNECmpSW,      // X86/X64 (REP)
  kX86InstRepNEScaSB,      // X86/X64 (REP)
  kX86InstRepNEScaSD,      // X86/X64 (REP)
  kX86InstRepNEScaSQ,      // X64 only (REP)
  kX86InstRepNEScaSW,      // X86/X64 (REP)
  kX86InstRet,             // X86/X64
  kX86InstRol,             // X86/X64
  kX86InstRor,             // X86/X64
  kX86InstRoundPD,         // SSE4.1
  kX86InstRoundPS,         // SSE4.1
  kX86InstRoundSD,         // SSE4.1
  kX86InstRoundSS,         // SSE4.1
  kX86InstRSqrtPS,         // SSE
  kX86InstRSqrtSS,         // SSE
  kX86InstSahf,            // X86/X64 (CPUID NEEDED)
  kX86InstSal,             // X86/X64
  kX86InstSar,             // X86/X64
  kX86InstSbb,             // X86/X64
  kX86InstSet,             // Begin (setcc)
  kX86InstSetA=kX86InstSet,// X86/X64 (setcc)
  kX86InstSetAE,           // X86/X64 (setcc)
  kX86InstSetB,            // X86/X64 (setcc)
  kX86InstSetBE,           // X86/X64 (setcc)
  kX86InstSetC,            // X86/X64 (setcc)
  kX86InstSetE,            // X86/X64 (setcc)
  kX86InstSetG,            // X86/X64 (setcc)
  kX86InstSetGE,           // X86/X64 (setcc)
  kX86InstSetL,            // X86/X64 (setcc)
  kX86InstSetLE,           // X86/X64 (setcc)
  kX86InstSetNA,           // X86/X64 (setcc)
  kX86InstSetNAE,          // X86/X64 (setcc)
  kX86InstSetNB,           // X86/X64 (setcc)
  kX86InstSetNBE,          // X86/X64 (setcc)
  kX86InstSetNC,           // X86/X64 (setcc)
  kX86InstSetNE,           // X86/X64 (setcc)
  kX86InstSetNG,           // X86/X64 (setcc)
  kX86InstSetNGE,          // X86/X64 (setcc)
  kX86InstSetNL,           // X86/X64 (setcc)
  kX86InstSetNLE,          // X86/X64 (setcc)
  kX86InstSetNO,           // X86/X64 (setcc)
  kX86InstSetNP,           // X86/X64 (setcc)
  kX86InstSetNS,           // X86/X64 (setcc)
  kX86InstSetNZ,           // X86/X64 (setcc)
  kX86InstSetO,            // X86/X64 (setcc)
  kX86InstSetP,            // X86/X64 (setcc)
  kX86InstSetPE,           // X86/X64 (setcc)
  kX86InstSetPO,           // X86/X64 (setcc)
  kX86InstSetS,            // X86/X64 (setcc)
  kX86InstSetZ,            // X86/X64 (setcc)
  kX86InstSFence,          // MMX-Ext/SSE
  kX86InstShl,             // X86/X64
  kX86InstShld,            // X86/X64
  kX86InstShr,             // X86/X64
  kX86InstShrd,            // X86/X64
  kX86InstShufPD,          // SSE2
  kX86InstShufPS,          // SSE
  kX86InstSqrtPD,          // SSE2
  kX86InstSqrtPS,          // SSE
  kX86InstSqrtSD,          // SSE2
  kX86InstSqrtSS,          // SSE
  kX86InstStc,             // X86/X64
  kX86InstStd,             // X86/X64
  kX86InstStMXCSR,         // SSE
  kX86InstSub,             // X86/X64
  kX86InstSubPD,           // SSE2
  kX86InstSubPS,           // SSE
  kX86InstSubSD,           // SSE2
  kX86InstSubSS,           // SSE
  kX86InstTest,            // X86/X64
  kX86InstUComISD,         // SSE2
  kX86InstUComISS,         // SSE
  kX86InstUd2,             // X86/X64
  kX86InstUnpckHPD,        // SSE2
  kX86InstUnpckHPS,        // SSE
  kX86InstUnpckLPD,        // SSE2
  kX86InstUnpckLPS,        // SSE
  kX86InstXadd,            // X86/X64 (i486)
  kX86InstXchg,            // X86/X64 (i386)
  kX86InstXor,             // X86/X64
  kX86InstXorPD,           // SSE2
  kX86InstXorPS,           // SSE

  _kX86InstCount,

  _kX86InstJBegin = kX86InstJ,
  _kX86InstJEnd = kX86InstJmp
};

// ============================================================================
// [AsmJit::kX86InstGroup]
// ============================================================================

//! @brief X86 instruction groups.
//!
//! This should be only used by assembler, because it's @c AsmJit::Assembler
//! specific grouping. Each group represents one 'case' in the Assembler's 
//! main emit method.
enum kX86InstGroup
{
  // Group categories.
  kX86InstGroupNone,
  kX86InstGroupEmit,

  kX86InstGroupArith,
  kX86InstGroupBSwap,
  kX86InstGroupBTest,
  kX86InstGroupCall,
  kX86InstGroupCrc32,
  kX86InstGroupEnter,
  kX86InstGroupIMul,
  kX86InstGroupIncDec,
  kX86InstGroupJcc,
  kX86InstGroupJmp,
  kX86InstGroupLea,
  kX86InstGroupMem,
  kX86InstGroupMov,
  kX86InstGroupMovPtr,
  kX86InstGroupMovSxMovZx,
  kX86InstGroupMovSxD,
  kX86InstGroupPush,
  kX86InstGroupPop,
  kX86InstGroupRegRm,
  kX86InstGroupRm,
  kX86InstGroupRmByte,
  kX86InstGroupRmReg,
  kX86InstGroupRep,
  kX86InstGroupRet,
  kX86InstGroupRot,
  kX86InstGroupShldShrd,
  kX86InstGroupTest,
  kX86InstGroupXchg,

  // Group for x87 FP instructions in format mem or st(i), st(i) (fadd, fsub, fdiv, ...)
  kX86InstGroupX87StM,
  // Group for x87 FP instructions in format st(i), st(i)
  kX86InstGroupX87StI,
  // Group for fld/fst/fstp instruction, internally uses @ref kX86InstGroupX87StM group.
  kX86InstGroupX87FldFst,
  // Group for x87 FP instructions that uses Word, DWord, QWord or TWord memory pointer.
  kX86InstGroupX87Mem,
  // Group for x87 FSTSW/FNSTSW instructions
  kX86InstGroupX87Status,

  // Group for movbe instruction
  kX86InstGroupMovBE,

  // Group for MMX/SSE instructions in format (X)MM|Reg|Mem <- (X)MM|Reg|Mem,
  // 0x66 prefix must be set manually in opcodes.
  // - Primary opcode is used for instructions in (X)MM <- (X)MM/Mem format,
  // - Secondary opcode is used for instructions in (X)MM/Mem <- (X)MM format.
  kX86InstGroupMmuMov,
  kX86InstGroupMmuMovD,
  kX86InstGroupMmuMovQ,

  // Group for pextrd, pextrq and pextrw instructions (it's special instruction
  // not similar to others)
  kX86InstGroupMmuExtract,
  // Group for prefetch instruction
  kX86InstGroupMmuPrefetch,

  // Group for MMX/SSE instructions in format (X)MM|Reg <- (X)MM|Reg|Mem|Imm,
  // 0x66 prefix is added for MMX instructions that used by SSE2 registers.
  // - Primary opcode is used for instructions in (X)MM|Reg <- (X)MM|Reg|Mem format,
  // - Secondary opcode is iused for instructions in (X)MM|Reg <- Imm format.
  kX86InstGroupMmuRmI,
  kX86InstGroupMmuRmImm8,
  // Group for 3dNow instructions
  kX86InstGroupMmuRm3dNow
};

// ============================================================================
// [AsmJit::kX86InstFlags]
// ============================================================================

//! @brief X86 instruction type flags.
enum kX86InstFlags
{
  //! @brief No flags.
  kX86InstFlagNone = 0x00,

  //! @brief Instruction is jump, conditional jump, call or ret.
  kX86InstFlagJump = 0x01,

  //! @brief Instruction will overwrite first operand - o[0].
  kX86InstFlagMov = 0x02,

  //! @brief Instruction is X87 FPU.
  kX86InstFlagFpu = 0x04,

  //! @brief Instruction can be prepended using LOCK prefix
  //! (usable for multithreaded applications).
  kX86InstFlagLockable = 0x08,

  //! @brief Instruction is special, this is for @c Compiler.
  kX86InstFlagSpecial = 0x10,

  //! @brief Instruction always performs memory access.
  //!
  //! This flag is always combined with @c kX86InstFlagSpecial and signalizes
  //! that there is an implicit address which is accessed (usually EDI/RDI or
  //! ESI/EDI).
  kX86InstFlagSpecialMem = 0x20
};

// ============================================================================
// [AsmJit::kX86InstOp]
// ============================================================================

//! @brief X86 instruction operand flags.
enum kX86InstOp
{
  // X86, MM, XMM
  kX86InstOpGb          = 0x0001,
  kX86InstOpGw          = 0x0002,
  kX86InstOpGd          = 0x0004,
  kX86InstOpGq          = 0x0008,
  kX86InstOpMm          = 0x0010,
  kX86InstOpXmm         = 0x0020,
  kX86InstOpMem         = 0x0040,
  kX86InstOpImm         = 0x0080,

  kX86InstOpGbMem       = kX86InstOpGb     | kX86InstOpMem,
  kX86InstOpGwMem       = kX86InstOpGw     | kX86InstOpMem,
  kX86InstOpGdMem       = kX86InstOpGd     | kX86InstOpMem,
  kX86InstOpGqMem       = kX86InstOpGq     | kX86InstOpMem,

  kX86InstOpGqdwb       = kX86InstOpGq     | kX86InstOpGd | kX86InstOpGw | kX86InstOpGb,
  kX86InstOpGqdw        = kX86InstOpGq     | kX86InstOpGd | kX86InstOpGw,
  kX86InstOpGqd         = kX86InstOpGq     | kX86InstOpGd,
  kX86InstOpGwb         = kX86InstOpGw     | kX86InstOpGb,

  kX86InstOpGqdwbMem    = kX86InstOpGqdwb  | kX86InstOpMem,
  kX86InstOpGqdwMem     = kX86InstOpGqdw   | kX86InstOpMem,
  kX86InstOpGqdMem      = kX86InstOpGqd    | kX86InstOpMem,
  kX86InstOpGwbMem      = kX86InstOpGwb    | kX86InstOpMem,

  // MMX/XMM.
  kX86InstOpMmMem       = kX86InstOpMm     | kX86InstOpMem,
  kX86InstOpXmmMem      = kX86InstOpXmm    | kX86InstOpMem,
  kX86InstOpMmXmm       = kX86InstOpMm     | kX86InstOpXmm,
  kX86InstOpMmXmmMem    = kX86InstOpMmXmm  | kX86InstOpMem,

  // X87.
  kX86InstOpStM2        = kX86InstOpMem    | 0x0100,
  kX86InstOpStM4        = kX86InstOpMem    | 0x0200,
  kX86InstOpStM8        = kX86InstOpMem    | 0x0400,
  kX86InstOpStM10       = kX86InstOpMem    | 0x0800,

  kX86InstOpStM2_4      = kX86InstOpStM2   | kX86InstOpStM4,
  kX86InstOpStM2_4_8    = kX86InstOpStM2_4 | kX86InstOpStM8,
  kX86InstOpStM4_8      = kX86InstOpStM4   | kX86InstOpStM8,
  kX86InstOpStM4_8_10   = kX86InstOpStM4_8 | kX86InstOpStM10,

  // Don't emit REX prefix.
  kX86InstOpNoRex       = 0x2000
};

// ============================================================================
// [AsmJit::x86InstName]
// ============================================================================

//! @internal
//! 
//! @brief X86 instruction names.
ASMJIT_VAR const char x86InstName[];

// ============================================================================
// [AsmJit::X86InstInfo]
// ============================================================================

//! @brief X86 instruction information.
struct X86InstInfo
{
  // --------------------------------------------------------------------------
  // [Accessors]
  // --------------------------------------------------------------------------

  //! @brief Get instruction code, see @ref kX86InstCode.
  inline uint32_t getCode() const
  { return _code; }

  //! @brief Get instruction name string (null terminated string).
  inline const char* getName() const
  { return x86InstName + static_cast<uint32_t>(_nameIndex); }

  //! @brief Get instruction name index (index to @ref x86InstName array).
  inline uint32_t getNameIndex() const
  { return _nameIndex; }

  //! @brief Get instruction group, see @ref kX86InstGroup.
  inline uint32_t getGroup() const
  { return _group; }

  //! @brief Get instruction flags, see @ref kX86InstFlags.
  inline uint32_t getFlags() const
  { return _group; }

  //! @brief Get whether the instruction is conditional or standard jump.
  inline bool isJump() const
  { return (_flags & kX86InstFlagJump) != 0; }

  //! @brief Get whether the instruction is MOV type.
  inline bool isMov() const
  { return (_flags & kX86InstFlagMov) != 0; }

  //! @brief Get whether the instruction is X87 FPU type.
  inline bool isFpu() const
  { return (_flags & kX86InstFlagFpu) != 0; }

  //! @brief Get whether the instruction can be prefixed by LOCK prefix.
  inline bool isLockable() const
  { return (_flags & kX86InstFlagLockable) != 0; }

  //! @brief Get whether the instruction is special type (this is used by
  //! @c Compiler to manage additional variables or functionality).
  inline bool isSpecial() const
  { return (_flags & kX86InstFlagSpecial) != 0; }

  //! @brief Get whether the instruction is special type and it performs
  //! memory access.
  inline bool isSpecialMem() const
  { return (_flags & kX86InstFlagSpecialMem) != 0; }

  // --------------------------------------------------------------------------
  // [Members]
  // --------------------------------------------------------------------------

  //! @brief Instruction code.
  uint16_t _code;
  //! @brief Instruction name index in x86InstName[] array.
  uint16_t _nameIndex;
  //! @brief Instruction group, used also by @c Compiler.
  uint8_t _group;
  //! @brief Instruction type flags.
  uint8_t _flags;

  //! @brief First and second operand flags (some groups depends on these settings, used also by @c Compiler).
  uint16_t _opFlags[2];
  //! @brief If instruction has only memory operand, this is register opcode.
  uint16_t _opCodeR;
  //! @brief Primary and secondary opcodes.
  uint32_t _opCode[2];
};

// ============================================================================
// [AsmJit::x86InstInfo]
// ============================================================================

ASMJIT_VAR const X86InstInfo x86InstInfo[];

// ============================================================================
// [AsmJit::kX86FuncConv]
// ============================================================================

//! @brief X86 function calling conventions.
//!
//! Calling convention is scheme how function arguments are passed into 
//! function and how functions returns values. In assembler programming
//! it's needed to always comply with function calling conventions, because
//! even small inconsistency can cause undefined behavior or crash.
//!
//! List of calling conventions for 32-bit x86 mode:
//! - @c kX86FuncConvCDecl - Calling convention for C runtime.
//! - @c kX86FuncConvStdCall - Calling convention for WinAPI functions.
//! - @c kX86FuncConvMsThisCall - Calling convention for C++ members under 
//!      Windows (produced by MSVC and all MSVC compatible compilers).
//! - @c kX86FuncConvMsFastCall - Fastest calling convention that can be used
//!      by MSVC compiler.
//! - @c kX86FuncConv_BORNANDFASTCALL - Borland fastcall convention.
//! - @c kX86FuncConvGccFastCall - GCC fastcall convention (2 register arguments).
//! - @c kX86FuncConvGccRegParm1 - GCC regparm(1) convention.
//! - @c kX86FuncConvGccRegParm2 - GCC regparm(2) convention.
//! - @c kX86FuncConvGccRegParm3 - GCC regparm(3) convention.
//!
//! List of calling conventions for 64-bit x86 mode (x64):
//! - @c kX86FuncConvX64W - Windows 64-bit calling convention (WIN64 ABI).
//! - @c kX86FuncConvX64U - Unix 64-bit calling convention (AMD64 ABI).
//!
//! There is also @c kX86FuncConvDefault that is defined to fit best to your 
//! compiler.
//!
//! These types are used together with @c AsmJit::Compiler::newFunc() 
//! method.
enum kX86FuncConv
{
  // --------------------------------------------------------------------------
  // [X64]
  // --------------------------------------------------------------------------

  //! @brief X64 calling convention for Windows platform (WIN64 ABI).
  //!
  //! For first four arguments are used these registers:
  //! - 1. 32/64-bit integer or floating point argument - rcx/xmm0
  //! - 2. 32/64-bit integer or floating point argument - rdx/xmm1
  //! - 3. 32/64-bit integer or floating point argument - r8/xmm2
  //! - 4. 32/64-bit integer or floating point argument - r9/xmm3
  //!
  //! Note first four arguments here means arguments at positions from 1 to 4
  //! (included). For example if second argument is not passed by register then
  //! rdx/xmm1 register is unused.
  //!
  //! All other arguments are pushed on the stack in right-to-left direction.
  //! Stack is aligned by 16 bytes. There is 32-byte shadow space on the stack
  //! that can be used to save up to four 64-bit registers (probably designed to
  //! be used to save first four arguments passed in registers).
  //!
  //! Arguments direction:
  //! - Right to Left (except for first 4 parameters that's in registers)
  //!
  //! Stack is cleaned by:
  //! - Caller.
  //!
  //! Return value:
  //! - Integer types - RAX register.
  //! - Floating points - XMM0 register.
  //!
  //! Stack is always aligned by 16 bytes.
  //!
  //! More information about this calling convention can be found on MSDN:
  //! http://msdn.microsoft.com/en-us/library/9b372w95.aspx .
  kX86FuncConvX64W = 1,

  //! @brief X64 calling convention for Unix platforms (AMD64 ABI).
  //!
  //! First six 32 or 64-bit integer arguments are passed in rdi, rsi, rdx, 
  //! rcx, r8, r9 registers. First eight floating point or XMM arguments 
  //! are passed in xmm0, xmm1, xmm2, xmm3, xmm4, xmm5, xmm6, xmm7 registers.
  //! This means that in registers can be transferred up to 14 arguments total.
  //!
  //! There is also RED ZONE below the stack pointer that can be used for 
  //! temporary storage. The red zone is the space from [rsp-128] to [rsp-8].
  //! 
  //! Arguments direction:
  //! - Right to Left (Except for arguments passed in registers).
  //!
  //! Stack is cleaned by:
  //! - Caller.
  //!
  //! Return value:
  //! - Integer types - RAX register.
  //! - Floating points - XMM0 register.
  //!
  //! Stack is always aligned by 16 bytes.
  kX86FuncConvX64U = 2,

  // --------------------------------------------------------------------------
  // [X86]
  // --------------------------------------------------------------------------

  //! @brief Cdecl calling convention (used by C runtime).
  //!
  //! Compatible across MSVC and GCC.
  //!
  //! Arguments direction:
  //! - Right to Left
  //!
  //! Stack is cleaned by:
  //! - Caller.
  kX86FuncConvCDecl = 3,

  //! @brief Stdcall calling convention (used by WinAPI).
  //!
  //! Compatible across MSVC and GCC.
  //!
  //! Arguments direction:
  //! - Right to Left
  //!
  //! Stack is cleaned by:
  //! - Callee.
  //!
  //! Return value:
  //! - Integer types - EAX:EDX registers.
  //! - Floating points - st(0) register.
  kX86FuncConvStdCall = 4,

  //! @brief MSVC specific calling convention used by MSVC/Intel compilers
  //! for struct/class methods.
  //!
  //! This is MSVC (and Intel) only calling convention used in Windows
  //! world for C++ class methods. Implicit 'this' pointer is stored in
  //! ECX register instead of storing it on the stack.
  //!
  //! Arguments direction:
  //! - Right to Left (except this pointer in ECX)
  //!
  //! Stack is cleaned by:
  //! - Callee.
  //!
  //! Return value:
  //! - Integer types - EAX:EDX registers.
  //! - Floating points - st(0) register.
  //!
  //! C++ class methods that have variable count of arguments uses different
  //! calling convention called cdecl.
  //!
  //! @note This calling convention is always used by MSVC for class methods,
  //! it's implicit and there is no way how to override it.
  kX86FuncConvMsThisCall = 5,

  //! @brief MSVC specific fastcall.
  //!
  //! Two first parameters (evaluated from left-to-right) are in ECX:EDX 
  //! registers, all others on the stack in right-to-left order.
  //!
  //! Arguments direction:
  //! - Right to Left (except to first two integer arguments in ECX:EDX)
  //!
  //! Stack is cleaned by:
  //! - Callee.
  //!
  //! Return value:
  //! - Integer types - EAX:EDX registers.
  //! - Floating points - st(0) register.
  //!
  //! @note This calling convention differs to GCC one in stack cleaning
  //! mechanism.
  kX86FuncConvMsFastCall = 6,

  //! @brief Borland specific fastcall with 2 parameters in registers.
  //!
  //! Two first parameters (evaluated from left-to-right) are in ECX:EDX 
  //! registers, all others on the stack in left-to-right order.
  //!
  //! Arguments direction:
  //! - Left to Right (except to first two integer arguments in ECX:EDX)
  //!
  //! Stack is cleaned by:
  //! - Callee.
  //!
  //! Return value:
  //! - Integer types - EAX:EDX registers.
  //! - Floating points - st(0) register.
  //!
  //! @note Arguments on the stack are in left-to-right order that differs
  //! to other fastcall conventions used in different compilers.
  kX86FuncConvBorlandFastCall = 7,

  //! @brief GCC specific fastcall convention.
  //!
  //! Two first parameters (evaluated from left-to-right) are in ECX:EDX 
  //! registers, all others on the stack in right-to-left order.
  //!
  //! Arguments direction:
  //! - Right to Left (except to first two integer arguments in ECX:EDX)
  //!
  //! Stack is cleaned by:
  //! - Callee.
  //!
  //! Return value:
  //! - Integer types - EAX:EDX registers.
  //! - Floating points - st(0) register.
  //!
  //! @note This calling convention should be compatible to
  //! @c kX86FuncConvMsFastCall.
  kX86FuncConvGccFastCall = 8,

  //! @brief GCC specific regparm(1) convention.
  //!
  //! The first parameter (evaluated from left-to-right) is in EAX register,
  //! all others on the stack in right-to-left order.
  //!
  //! Arguments direction:
  //! - Right to Left (except to first one integer argument in EAX)
  //!
  //! Stack is cleaned by:
  //! - Caller.
  //!
  //! Return value:
  //! - Integer types - EAX:EDX registers.
  //! - Floating points - st(0) register.
  kX86FuncConvGccRegParm1 = 9,

  //! @brief GCC specific regparm(2) convention.
  //!
  //! Two first parameters (evaluated from left-to-right) are in EAX:EDX 
  //! registers, all others on the stack in right-to-left order.
  //!
  //! Arguments direction:
  //! - Right to Left (except to first two integer arguments in EAX:EDX)
  //!
  //! Stack is cleaned by:
  //! - Caller.
  //!
  //! Return value:
  //! - Integer types - EAX:EDX registers.
  //! - Floating points - st(0) register.
  kX86FuncConvGccRegParm2 = 10,

  //! @brief GCC specific fastcall with 3 parameters in registers.
  //!
  //! Three first parameters (evaluated from left-to-right) are in 
  //! EAX:EDX:ECX registers, all others on the stack in right-to-left order.
  //!
  //! Arguments direction:
  //! - Right to Left (except to first three integer arguments in EAX:EDX:ECX)
  //!
  //! Stack is cleaned by:
  //! - Caller.
  //!
  //! Return value:
  //! - Integer types - EAX:EDX registers.
  //! - Floating points - st(0) register.
  kX86FuncConvGccRegParm3 = 11,

  // --------------------------------------------------------------------------
  // [Detect]
  // --------------------------------------------------------------------------

  //! @def kX86FuncConvDefault
  //! @brief Default calling convention for current platform / operating system.

  //! @def kX86FuncConvCompatFastCall
  //! @brief Compatibility for __fastcall calling convention.
  //!
  //! @note This enumeration is always set to a value which is compatible to
  //! current compilers __fastcall calling convention. In 64-bit mode the value
  //! is compatible to @ref kX86FuncConvX64W or @ref kX86FuncConvX64U.

  //! @def kX86FuncConvCompatStdCall
  //! @brief Compatibility for __stdcall calling convention.
  //!
  //! @note This enumeration is always set to a value which is compatible to
  //! current compilers __stdcall calling convention. In 64-bit mode the value
  //! is compatible to @ref kX86FuncConvX64W or @ref kX86FuncConvX64U.

  //! @def kX86FuncConvCompatCDecl
  //! @brief Default C calling convention based on current compiler's settings.

#if defined(ASMJIT_X86)

  kX86FuncConvDefault = kX86FuncConvCDecl,

# if defined(_MSC_VER)
  kX86FuncConvCompatFastCall = kX86FuncConvMsFastCall,
# elif defined(__GNUC__)
  kX86FuncConvCompatFastCall = kX86FuncConvGccFastCall,
# elif defined(__BORLANDC__)
  kX86FuncConvCompatFastCall = kX86FuncConvBorlandFastCall,
# else
#  error "AsmJit::kX86FuncConvCompatFastCall not supported."
# endif

  kX86FuncConvCompatStdCall = kX86FuncConvStdCall,
  kX86FuncConvCompatCDecl = kX86FuncConvCDecl

#else

# if defined(ASMJIT_WINDOWS)
  kX86FuncConvDefault = kX86FuncConvX64W,
# else
  kX86FuncConvDefault = kX86FuncConvX64U,
# endif

  kX86FuncConvCompatFastCall = kX86FuncConvDefault,
  kX86FuncConvCompatStdCall = kX86FuncConvDefault,
  kX86FuncConvCompatCDecl = kX86FuncConvDefault

#endif // ASMJIT_X86
};

// ============================================================================
// [AsmJit::kX86FuncHint]
// ============================================================================

//! @brief X86 function hints.
enum kX86FuncHint
{
  //! @brief Use push/pop sequences instead of mov sequences in function prolog
  //! and epilog.
  kX86FuncHintPushPop = 8,
  //! @brief Add emms instruction to the function epilog.
  kX86FuncHintEmms = 9,
  //! @brief Add sfence instruction to the function epilog.
  kX86FuncHintSFence = 10,
  //! @brief Add lfence instruction to the function epilog.
  kX86FuncHintLFence = 11,
  //! @brief Assume that stack is aligned to 16-bytes.
  kX86FuncHintAssume16ByteAlignment = 12,
  //! @brief Perform 16-byte stack alignmend by function.
  kX86FuncHintPerform16ByteAlignment = 13
};

// ============================================================================
// [AsmJit::kX86FuncFlags]
// ============================================================================

//! @brief X86 function flags.
enum kX86FuncFlags
{
  //! @brief Whether to emit prolog / epilog sequence using push & pop
  //! instructions (the default).
  kX86FuncFlagPushPop = (1U << 8),

  //! @brief Whether to emit EMMS instruction in epilog (auto-detected).
  kX86FuncFlagEmitEmms = (1U << 9),

  //! @brief Whether to emit SFence instruction in epilog (auto-detected).
  //!
  //! @note @ref kX86FuncFlagEmitSFence and @ref kX86FuncFlagEmitLFence
  //! combination will result in emitting mfence.
  kX86FuncFlagEmitSFence = (1U << 10),

  //! @brief Whether to emit LFence instruction in epilog (auto-detected).
  //!
  //! @note @ref kX86FuncFlagEmitSFence and @ref kX86FuncFlagEmitLFence
  //! combination will result in emitting mfence.
  kX86FuncFlagEmitLFence = (1U << 11),

  //! @brief Whether the function stack is aligned by 16-bytes by OS.
  //!
  //! This is always true for 64-bit mode and for linux.
  kX86FuncFlagAssume16ByteAlignment = (1U << 12),

  //! @brief Whether the function stack (for variables) is aligned manually
  //! by function to 16-bytes.
  //!
  //! This makes sense only if @ref kX86FuncFlagAssume16ByteAlignment is 
  //! false and MOVDQA instruction or other SSE/SSE2 instructions are used to
  //! work with variables stored on the stack.
  //!
  //! Value is determined automatically by these factors, expectations are:
  //!
  //!   1. There is 16-byte wide variable which address was used (alloc, spill,
  //!      op).
  //!   2. Function can't be naked.
  kX86FuncFlagPerform16ByteAlignment = (1U << 13),

  //! @brief Whether the ESP register is adjusted by the stack size needed
  //! to save registers and function variables.
  //!
  //! Esp is adjusted by 'sub' instruction in prolog and by add function in
  //! epilog (only if function is not naked).
  kX86FuncFlagIsEspAdjusted = (1U << 14)
};

// ============================================================================
// [AsmJit::kX86CompilerInst]
// ============================================================================

//! @brief Instruction flags used by @ref X86CompilerInst item.
enum kX86CompilerInstFlag
{
  //! @brief Whether the instruction is special.
  kX86CompilerInstFlagIsSpecial = (1U << 0),
  //! @brief Whether the instruction is FPU.
  kX86CompilerInstFlagIsFpu = (1U << 1),
  //! @brief Whether the one of the operands is GPB.Lo register.
  kX86CompilerInstFlagIsGpbLoUsed = (1U << 2),
  //! @brief Whether the one of the operands is GPB.Hi register.
  kX86CompilerInstFlagIsGpbHiUsed = (1U << 3),

  //! @brief Whether the jmp/jcc is likely to be taken.
  kX86CompilerInstFlagIsTaken = (1U << 7)
};

// ============================================================================
// [AsmJit::kX86VarClass]
// ============================================================================

//! @brief X86 variable class.
enum kX86VarClass
{
  //! @brief No class (used internally).
  kX86VarClassNone = 0,
  //! @brief General purpose register.
  kX86VarClassGp = 1,
  //! @brief X87 floating point.
  kX86VarClassX87 = 2,
  //! @brief MMX register.
  kX86VarClassMm = 3,
  //! @brief XMM register.
  kX86VarClassXmm = 4,

  //! @brief Count of X86 variable classes.
  kX86VarClassCount = 5
};

// ============================================================================
// [AsmJit::kX86VarFlags]
// ============================================================================

//! @brief X86 variable class.
enum kX86VarFlags
{
  //! @brief Variable contains single-precision floating-point(s).
  kX86VarFlagSP = 0x10,
  //! @brief Variable contains double-precision floating-point(s).
  kX86VarFlagDP = 0x20,
  //! @brief Variable is packed (for example float4x, double2x, ...).
  kX86VarFlagPacked = 0x40
};

// ============================================================================
// [AsmJit::kX86VarType]
// ============================================================================

//! @brief X86 variable type.
enum kX86VarType
{
  // --------------------------------------------------------------------------
  // [Platform Dependent]
  // --------------------------------------------------------------------------

  //! @brief Variable is 32-bit general purpose register.
  kX86VarTypeGpd = 0,
  //! @brief Variable is 64-bit general purpose register.
  kX86VarTypeGpq = 1,

  //! @var kX86VarTypeGpz
  //! @brief Variable is system wide general purpose register (32-bit or 64-bit).
#if defined(ASMJIT_X86)
  kX86VarTypeGpz = kX86VarTypeGpd,
#else
  kX86VarTypeGpz = kX86VarTypeGpq,
#endif

  //! @brief Variable is X87 (FPU).
  kX86VarTypeX87 = 2,
  //! @brief Variable is X87 (FPU) SP-FP number (float).
  kX86VarTypeX87SS = 3,
  //! @brief Variable is X87 (FPU) DP-FP number (double).
  kX86VarTypeX87SD = 4,

  //! @brief Variable is MM register / memory location.
  kX86VarTypeMm = 5,
  //! @brief Variable is XMM register / memory location.
  kX86VarTypeXmm = 6,

  //! @brief Variable is SSE scalar SP-FP number.
  kX86VarTypeXmmSS = 7,
  //! @brief Variable is SSE packed SP-FP number (4 floats).
  kX86VarTypeXmmPS = 8,

  //! @brief Variable is SSE2 scalar DP-FP number.
  kX86VarTypeXmmSD = 9,
  //! @brief Variable is SSE2 packed DP-FP number (2 doubles).
  kX86VarTypeXmmPD = 10,

  //! @brief Count of variable types.
  kX86VarTypeCount = 11,

  // --------------------------------------------------------------------------
  // [Platform Independent]
  // --------------------------------------------------------------------------

  //! @brief Variable is 32-bit integer.
  kX86VarTypeInt32 = kX86VarTypeGpd,
  //! @brief Variable is 64-bit integer.
  kX86VarTypeInt64 = kX86VarTypeGpq,
  //! @brief Variable is system dependent integer / pointer.
  kX86VarTypeIntPtr = kX86VarTypeGpz,

#if defined(ASMJIT_X86)
  kX86VarTypeFloat = kX86VarTypeX87SS,
  kX86VarTypeDouble = kX86VarTypeX87SD
#else
  kX86VarTypeFloat = kX86VarTypeXmmSS,
  kX86VarTypeDouble = kX86VarTypeXmmSD
#endif
};

// ============================================================================
// [AsmJit::X86VarInfo]
// ============================================================================

//! @brief X86 variable information.
struct X86VarInfo
{
  // --------------------------------------------------------------------------
  // [Accessors]
  // --------------------------------------------------------------------------

  //! @brief Get register code base, see @ref kX86RegCode.
  inline uint32_t getCode() const
  { return _code; }

  //! @brief Get register size in bytes.
  inline uint32_t getSize() const
  { return _size; }

  //! @brief Get variable class, see @ref kX86VarClass.
  inline uint32_t getClass() const
  { return _class; }

  //! @brief Get variable flags, see @ref kX86VarFlags.
  inline uint32_t getFlags() const
  { return _flags; }

  //! @brief Get variable type name.
  inline const char* getName() const
  { return _name; }

  // --------------------------------------------------------------------------
  // [Members]
  // --------------------------------------------------------------------------

  //! @brief Register code base, see @ref kX86RegCode.
  uint32_t _code;
  //! @brief Register size in bytes.
  uint16_t _size;
  //! @brief Variable class, see @ref kX86VarClass.
  uint8_t _class;
  //! @brief Variable flags, see @ref kX86VarFlags.
  uint8_t _flags;
  //! @brief Variable type name.
  char _name[8];
};

// ============================================================================
// [AsmJit::x86VarInfo]
// ============================================================================

ASMJIT_VAR const X86VarInfo x86VarInfo[];

//! @}

} // AsmJit namespace

// [Api-End]
#include "../core/apiend.h"

// [Guard]
#endif // _ASMJIT_X86_X86DEFS_H
