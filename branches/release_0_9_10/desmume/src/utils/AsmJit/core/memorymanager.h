// [AsmJit]
// Complete JIT Assembler for C++ Language.
//
// [License]
// Zlib - See COPYING file in this package.

// [Guard]
#ifndef _ASMJIT_CORE_MEMORYMANAGER_H
#define _ASMJIT_CORE_MEMORYMANAGER_H

// [Dependencies - AsmJit]
#include "../core/build.h"
#include "../core/defs.h"

// [Api-Begin]
#include "../core/apibegin.h"

// [Debug]
// #define ASMJIT_MEMORY_MANAGER_DUMP

namespace AsmJit {

//! @addtogroup AsmJit_MemoryManagement
//! @{

// ============================================================================
// [AsmJit::MemoryManager]
// ============================================================================

//! @brief Virtual memory manager interface.
//!
//! This class is pure virtual. You can get default virtual memory manager using
//! @c getGlobal() method. If you want to create more memory managers with same
//! functionality as global memory manager use @c VirtualMemoryManager class.
struct MemoryManager
{
  // --------------------------------------------------------------------------
  // [Construction / Destruction]
  // --------------------------------------------------------------------------

  //! @brief Create memory manager instance.
  ASMJIT_API MemoryManager();
  //! @brief Destroy memory manager instance, this means also to free all memory
  //! blocks.
  ASMJIT_API virtual ~MemoryManager();

  // --------------------------------------------------------------------------
  // [Interface]
  // --------------------------------------------------------------------------

  //! @brief Allocate a @a size bytes of virtual memory.
  //!
  //! Note that if you are implementing your own virtual memory manager then you
  //! can quitly ignore type of allocation. This is mainly for AsmJit to memory
  //! manager that allocated memory will be never freed.
  virtual void* alloc(size_t size, uint32_t type = kMemAllocFreeable) = 0;
  //! @brief Free previously allocated memory at a given @a address.
  virtual bool free(void* address) = 0;
  //! @brief Free some tail memory.
  virtual bool shrink(void* address, size_t used) = 0;
  //! @brief Free all allocated memory.
  virtual void freeAll() = 0;

  //! @brief Get how many bytes are currently used.
  virtual size_t getUsedBytes() = 0;
  //! @brief Get how many bytes are currently allocated.
  virtual size_t getAllocatedBytes() = 0;

  // --------------------------------------------------------------------------
  // [Statics]
  // --------------------------------------------------------------------------

  //! @brief Get global memory manager instance.
  //!
  //! Global instance is instance of @c VirtualMemoryManager class. Global memory
  //! manager is used by default by @ref Assembler::make() and @ref Compiler::make()
  //! methods.
  ASMJIT_API static MemoryManager* getGlobal();
};

// ============================================================================
// [AsmJit::VirtualMemoryManager]
// ============================================================================

//! @brief Reference implementation of memory manager that uses
//! @ref AsmJit::VirtualMemory class to allocate chunks of virtual memory
//! and bit arrays to manage it.
struct VirtualMemoryManager : public MemoryManager
{
  // --------------------------------------------------------------------------
  // [Construction / Destruction]
  // --------------------------------------------------------------------------

  //! @brief Create a @c VirtualMemoryManager instance.
  ASMJIT_API VirtualMemoryManager();

#if defined(ASMJIT_WINDOWS)
  //! @brief Create a @c VirtualMemoryManager instance for process @a hProcess.
  //!
  //! This is specialized version of constructor available only for windows and
  //! usable to alloc/free memory of different process.
  ASMJIT_API VirtualMemoryManager(HANDLE hProcess);
#endif // ASMJIT_WINDOWS

  //! @brief Destroy the @c VirtualMemoryManager instance, this means also to
  //! free all blocks.
  ASMJIT_API virtual ~VirtualMemoryManager();

  // --------------------------------------------------------------------------
  // [Interface]
  // --------------------------------------------------------------------------

  ASMJIT_API virtual void* alloc(size_t size, uint32_t type = kMemAllocFreeable);
  ASMJIT_API virtual bool free(void* address);
  ASMJIT_API virtual bool shrink(void* address, size_t used);
  ASMJIT_API virtual void freeAll();

  ASMJIT_API virtual size_t getUsedBytes();
  ASMJIT_API virtual size_t getAllocatedBytes();

  // --------------------------------------------------------------------------
  // [Virtual Memory Manager Specific]
  // --------------------------------------------------------------------------

  //! @brief Get whether to keep allocated memory after memory manager is
  //! destroyed.
  //!
  //! @sa @c setKeepVirtualMemory().
  ASMJIT_API bool getKeepVirtualMemory() const;

  //! @brief Set whether to keep allocated memory after memory manager is
  //! destroyed.
  //!
  //! This method is usable when patching code of remote process. You need to
  //! allocate process memory, store generated assembler into it and patch the
  //! method you want to redirect (into your code). This method affects only
  //! VirtualMemoryManager destructor. After destruction all internal 
  //! structures are freed, only the process virtual memory remains.
  //! 
  //! @note Memory allocated with kMemAllocPermanent is always kept.
  //!
  //! @sa @c getKeepVirtualMemory().
  ASMJIT_API void setKeepVirtualMemory(bool keepVirtualMemory);

  // --------------------------------------------------------------------------
  // [Debug]
  // --------------------------------------------------------------------------

#if defined(ASMJIT_MEMORY_MANAGER_DUMP)
  //! @brief Dump memory manager tree into file.
  //!
  //! Generated output is using DOT language (from graphviz package).
  ASMJIT_API void dump(const char* fileName);
#endif // ASMJIT_MEMORY_MANAGER_DUMP

  // --------------------------------------------------------------------------
  // [Members]
  // --------------------------------------------------------------------------

  //! @brief Pointer to private data hidden from the public API.
  void* _d;
};

//! @}

} // AsmJit namespace

// [Api-End]
#include "../core/apiend.h"

// [Guard]
#endif // _ASMJIT_CORE_MEMORYMANAGER_H
