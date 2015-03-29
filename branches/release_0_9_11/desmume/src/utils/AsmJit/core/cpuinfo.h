// [AsmJit]
// Complete JIT Assembler for C++ Language.
//
// [License]
// Zlib - See COPYING file in this package.

// [Guard]
#ifndef _ASMJIT_CORE_CPUINFO_H
#define _ASMJIT_CORE_CPUINFO_H

// [Dependencies - AsmJit]
#include "../core/build.h"

// [Api-Begin]
#include "../core/apibegin.h"

namespace AsmJit {

//! @addtogroup AsmJit_Core
//! @{

// ============================================================================
// [AsmJit::CpuInfo]
// ============================================================================

//! @brief Informations about host cpu.
struct CpuInfo
{
  // --------------------------------------------------------------------------
  // [Construction / Destruction]
  // --------------------------------------------------------------------------

  inline CpuInfo(uint32_t size = sizeof(CpuInfo)) :
    _size(size)
  {
  }

  // --------------------------------------------------------------------------
  // [Accessors]
  // --------------------------------------------------------------------------

  //! @brief Get CPU vendor string.
  inline const char* getVendorString() const { return _vendorString; }
  //! @brief Get CPU brand string.
  inline const char* getBrandString() const { return _brandString; }

  //! @brief Get CPU vendor ID.
  inline uint32_t getVendorId() const { return _vendorId; }
  //! @brief Get CPU family ID.
  inline uint32_t getFamily() const { return _family; }
  //! @brief Get CPU model ID.
  inline uint32_t getModel() const { return _model; }
  //! @brief Get CPU stepping.
  inline uint32_t getStepping() const { return _stepping; }
  //! @brief Get CPU count.
  inline uint32_t getNumberOfProcessors() const { return _numberOfProcessors; }
  //! @brief Get CPU features.
  inline uint32_t getFeatures() const { return _features; }
  //! @brief Get CPU bugs.
  inline uint32_t getBugs() const { return _bugs; }

  //! @brief Get whether CPU has feature @a feature.
  inline bool hasFeature(uint32_t feature) { return (_features & feature) != 0; }
  //! @brief Get whether CPU has bug @a bug.
  inline bool hasBug(uint32_t bug) { return (_bugs & bug) != 0; }

  // --------------------------------------------------------------------------
  // [Statics]
  // --------------------------------------------------------------------------

  //! @brief Detect number of processors.
  ASMJIT_API static uint32_t detectNumberOfProcessors();

  //! @brief Get global instance of @ref CpuInfo.
  ASMJIT_API static const CpuInfo* getGlobal();

  // --------------------------------------------------------------------------
  // [Members]
  // --------------------------------------------------------------------------

  //! @brief Size of CpuInfo structure (in bytes).
  uint32_t _size;

  //! @brief Cpu short vendor string.
  char _vendorString[16];
  //! @brief Cpu long vendor string (brand).
  char _brandString[64];

  //! @brief Cpu vendor id (see @c AsmJit::CpuInfo::VendorId enum).
  uint32_t _vendorId;
  //! @brief Cpu family ID.
  uint32_t _family;
  //! @brief Cpu model ID.
  uint32_t _model;
  //! @brief Cpu stepping.
  uint32_t _stepping;
  //! @brief Number of processors or cores.
  uint32_t _numberOfProcessors;
  //! @brief Cpu features bitfield, see @c AsmJit::CpuInfo::Feature enum).
  uint32_t _features;
  //! @brief Cpu bugs bitfield, see @c AsmJit::CpuInfo::Bug enum).
  uint32_t _bugs;
};

//! @}

} // AsmJit namespace

// [Api-End]
#include "../core/apiend.h"

// [Guard]
#endif // _ASMJIT_CORE_CPUINFO_H
