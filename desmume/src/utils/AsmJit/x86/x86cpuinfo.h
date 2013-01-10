// [AsmJit]
// Complete JIT Assembler for C++ Language.
//
// [License]
// Zlib - See COPYING file in this package.

// [Guard]
#ifndef _ASMJIT_X86_X86CPUINFO_H
#define _ASMJIT_X86_X86CPUINFO_H

// [Dependencies - AsmJit]
#include "../core/cpuinfo.h"
#include "../core/defs.h"

// [Api-Begin]
#include "../core/apibegin.h"

namespace AsmJit {

//! @addtogroup AsmJit_X86
//! @{

// ============================================================================
// [AsmJit::X86CpuId]
// ============================================================================

//! @brief X86 CpuId output.
union X86CpuId
{
  //! @brief EAX/EBX/ECX/EDX output.
  uint32_t i[4];

  struct
  {
    //! @brief EAX output.
    uint32_t eax;
    //! @brief EBX output.
    uint32_t ebx;
    //! @brief ECX output.
    uint32_t ecx;
    //! @brief EDX output.
    uint32_t edx;
  };
};

// ============================================================================
// [AsmJit::X86CpuInfo]
// ============================================================================

struct X86CpuInfo : public CpuInfo
{
  // --------------------------------------------------------------------------
  // [Construction / Destruction]
  // --------------------------------------------------------------------------

  inline X86CpuInfo(uint32_t size = sizeof(X86CpuInfo)) :
    CpuInfo(size)
  {
  }

  // --------------------------------------------------------------------------
  // [Accessors]
  // --------------------------------------------------------------------------

  //! @brief Get processor type.
  inline uint32_t getProcessorType() const { return _processorType; }
  //! @brief Get brand index.
  inline uint32_t getBrandIndex() const { return _brandIndex; }
  //! @brief Get flush cache line size.
  inline uint32_t getFlushCacheLineSize() const { return _flushCacheLineSize; }
  //! @brief Get maximum logical processors count.
  inline uint32_t getMaxLogicalProcessors() const { return _maxLogicalProcessors; }
  //! @brief Get APIC physical ID.
  inline uint32_t getApicPhysicalId() const { return _apicPhysicalId; }

  // --------------------------------------------------------------------------
  // [Statics]
  // --------------------------------------------------------------------------

  //! @brief Get global instance of @ref X86CpuInfo.
  static inline const X86CpuInfo* getGlobal()
  { return static_cast<const X86CpuInfo*>(CpuInfo::getGlobal()); }

  // --------------------------------------------------------------------------
  // [Members]
  // --------------------------------------------------------------------------

  //! @brief Processor type.
  uint32_t _processorType;
  //! @brief Brand index.
  uint32_t _brandIndex;
  //! @brief Flush cache line size in bytes.
  uint32_t _flushCacheLineSize;
  //! @brief Maximum number of addressable IDs for logical processors.
  uint32_t _maxLogicalProcessors;
  //! @brief Initial APIC ID.
  uint32_t _apicPhysicalId;
};

// ============================================================================
// [AsmJit::x86CpuId]
// ============================================================================

#if defined(ASMJIT_X86) || defined(ASMJIT_X64)
//! @brief Calls CPUID instruction with eax == @a in and stores output to @a out.
//!
//! @c cpuid() function has one input parameter that is passed to cpuid through 
//! eax register and results in four output values representing result of cpuid 
//! instruction (eax, ebx, ecx and edx registers).
ASMJIT_API void x86CpuId(uint32_t in, X86CpuId* out);

// ============================================================================
// [AsmJit::x86CpuDetect]
// ============================================================================

//! @brief Detect CPU features to CpuInfo structure @a out.
//!
//! @sa @c CpuInfo.
ASMJIT_API void x86CpuDetect(X86CpuInfo* out);
#endif // ASMJIT_X86 || ASMJIT_X64

//! @}

} // AsmJit namespace

// [Api-End]
#include "../core/apiend.h"

// [Guard]
#endif // _ASMJIT_X86_X86CPUINFO_H
