// [AsmJit]
// Complete JIT Assembler for C++ Language.
//
// [License]
// Zlib - See COPYING file in this package.

// [Guard]
#ifndef _ASMJIT_CORE_ZONEMEMORY_H
#define _ASMJIT_CORE_ZONEMEMORY_H

// [Dependencies - AsmJit]
#include "../core/build.h"

namespace AsmJit {

//! @addtogroup AsmJit_Core
//! @{

// ============================================================================
// [AsmJit::ZoneChunk]
// ============================================================================

//! @internal
//!
//! @brief One allocated chunk of memory.
struct ZoneChunk
{
  // --------------------------------------------------------------------------
  // [Methods]
  // --------------------------------------------------------------------------

  //! @brief Get count of remaining (unused) bytes in chunk.
  inline size_t getRemainingBytes() const { return size - pos; }

  // --------------------------------------------------------------------------
  // [Members]
  // --------------------------------------------------------------------------

  //! @brief Link to previous chunk.
  ZoneChunk* prev;
  //! @brief Position in this chunk.
  size_t pos;
  //! @brief Size of this chunk (in bytes).
  size_t size;

  //! @brief Data.
  uint8_t data[sizeof(void*)];
};

// ============================================================================
// [AsmJit::ZoneMemory]
// ============================================================================

//! @brief Memory allocator designed to fast alloc memory that will be freed
//! in one step.
//!
//! @note This is hackery for performance. Concept is that objects created
//! by @c ZoneMemory are freed all at once. This means that lifetime of 
//! these objects are the same as the zone object itself.
struct ZoneMemory
{
  // --------------------------------------------------------------------------
  // [Construction / Destruction]
  // --------------------------------------------------------------------------

  //! @brief Create new instance of @c ZoneMemory.
  //! @param chunkSize Default size for one zone chunk.
  ASMJIT_API ZoneMemory(size_t chunkSize);

  //! @brief Destroy @ref ZoneMemory instance.
  ASMJIT_API ~ZoneMemory();

  // --------------------------------------------------------------------------
  // [Methods]
  // --------------------------------------------------------------------------

  //! @brief Allocate @c size bytes of memory and return pointer to it.
  //!
  //! Pointer allocated by this way will be valid until @c ZoneMemory object
  //! is destroyed. To create class by this way use placement @c new and 
  //! @c delete operators:
  //!
  //! @code
  //! // Example of allocating simple class
  //!
  //! // Your class
  //! class Object
  //! {
  //!   // members...
  //! };
  //!
  //! // Your function
  //! void f()
  //! {
  //!   // We are using AsmJit namespace
  //!   using namespace AsmJit
  //!
  //!   // Create zone object with chunk size of 65536 bytes.
  //!   ZoneMemory zone(65536);
  //!
  //!   // Create your objects using zone object allocating, for example:
  //!   Object* obj = new(zone.alloc(sizeof(YourClass))) Object();
  //! 
  //!   // ... lifetime of your objects ...
  //! 
  //!   // Destroy your objects:
  //!   obj->~Object();
  //!
  //!   // ZoneMemory destructor will free all memory allocated through it, 
  //!   // alternative is to call @c zone.reset().
  //! }
  //! @endcode
  ASMJIT_API void* alloc(size_t size);

  //! @brief Helper to duplicate string.
  ASMJIT_API char* sdup(const char* str);

  //! @brief Free all allocated memory except first block that remains for reuse.
  //!
  //! Note that this method will invalidate all instances using this memory
  //! allocated by this zone instance.
  ASMJIT_API void clear();

  //! @brief Free all allocated memory at once.
  //!
  //! Note that this method will invalidate all instances using this memory
  //! allocated by this zone instance.
  ASMJIT_API void reset();

  //! @brief Get total size of allocated objects - by @c alloc().
  inline size_t getTotal() const { return _total; }
  //! @brief Get (default) chunk size.
  inline size_t getChunkSize() const { return _chunkSize; }

  // --------------------------------------------------------------------------
  // [Members]
  // --------------------------------------------------------------------------

  //! @brief Last allocated chunk of memory.
  ZoneChunk* _chunks;
  //! @brief Total size of allocated objects - by @c alloc() method.
  size_t _total;
  //! @brief One chunk size.
  size_t _chunkSize;
};

//! @}

} // AsmJit namespace

#endif // _ASMJIT_CORE_ZONEMEMORY_H
