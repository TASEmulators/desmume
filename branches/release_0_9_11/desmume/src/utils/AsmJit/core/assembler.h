// [AsmJit]
// Complete JIT Assembler for C++ Language.
//
// [License]
// Zlib - See COPYING file in this package.

// [Guard]
#ifndef _ASMJIT_CORE_ASSEMBLER_H
#define _ASMJIT_CORE_ASSEMBLER_H

// [Dependencies - AsmJit]
#include "../core/buffer.h"
#include "../core/context.h"
#include "../core/defs.h"
#include "../core/logger.h"
#include "../core/podvector.h"
#include "../core/zonememory.h"

// [Api-Begin]
#include "../core/apibegin.h"

namespace AsmJit {

//! @addtogroup AsmJit_Core
//! @{

// ============================================================================
// [AsmJit::Assembler]
// ============================================================================

//! @brief Base class for @ref Assembler.
//!
//! This class implements core setialization API only. The platform specific
//! methods and intrinsics is implemented by derived classes.
//!
//! @sa @c Assembler.
struct Assembler
{
  // --------------------------------------------------------------------------
  // [Construction / Destruction]
  // --------------------------------------------------------------------------

  //! @brief Creates Assembler instance.
  ASMJIT_API Assembler(Context* context);
  //! @brief Destroys Assembler instance
  ASMJIT_API virtual ~Assembler();

  // --------------------------------------------------------------------------
  // [LabelLink]
  // --------------------------------------------------------------------------

  //! @brief Data structure used to link linked-labels.
  struct LabelLink
  {
    //! @brief Previous link.
    LabelLink* prev;
    //! @brief Offset.
    sysint_t offset;
    //! @brief Inlined displacement.
    sysint_t displacement;
    //! @brief RelocId if link must be absolute when relocated.
    sysint_t relocId;
  };

  // --------------------------------------------------------------------------
  // [LabelData]
  // --------------------------------------------------------------------------

  //! @brief Label data.
  struct LabelData
  {
    //! @brief Label offset.
    sysint_t offset;
    //! @brief Label links chain.
    LabelLink* links;
  };

  // --------------------------------------------------------------------------
  // [RelocData]
  // --------------------------------------------------------------------------

  // X86 architecture uses 32-bit absolute addressing model by memory operands,
  // but 64-bit mode uses relative addressing model (RIP + displacement). In
  // code we are always using relative addressing model for referencing labels
  // and embedded data. In 32-bit mode we must patch all references to absolute
  // address before we can call generated function. We are patching only memory
  // operands.

  //! @brief Code relocation data (relative vs absolute addresses).
  struct RelocData
  {
    //! @brief Type of relocation.
    uint32_t type;
    //! @brief Size of relocation (4 or 8 bytes).
    uint32_t size;
    //! @brief Offset from code begin address.
    sysint_t offset;

    //! @brief Relative displacement or absolute address.
    union
    {
      //! @brief Relative displacement from code begin address (not to @c offset).
      sysint_t destination;
      //! @brief Absolute address where to jump;
      void* address;
    };
  };

  // --------------------------------------------------------------------------
  // [Context]
  // --------------------------------------------------------------------------

  //! @brief Get code generator.
  inline Context* getContext() const
  { return _context; }

  // --------------------------------------------------------------------------
  // [Memory Management]
  // --------------------------------------------------------------------------

  //! @brief Get zone memory manager.
  inline ZoneMemory* getZoneMemory() const
  { return const_cast<ZoneMemory*>(&_zoneMemory); }

  // --------------------------------------------------------------------------
  // [Logging]
  // --------------------------------------------------------------------------

  //! @brief Get logger.
  inline Logger* getLogger() const
  { return _logger; }

  //! @brief Set logger to @a logger.
  ASMJIT_API virtual void setLogger(Logger* logger);

  // --------------------------------------------------------------------------
  // [Error Handling]
  // --------------------------------------------------------------------------

  //! @brief Get error code.
  inline uint32_t getError() const
  { return _error; }

  //! @brief Set error code.
  //!
  //! This method is virtual, because higher classes can use it to catch all
  //! errors.
  ASMJIT_API virtual void setError(uint32_t error);

  // --------------------------------------------------------------------------
  // [Properties]
  // --------------------------------------------------------------------------

  //! @brief Get assembler property.
  ASMJIT_API virtual uint32_t getProperty(uint32_t propertyId) const;

  //! @brief Set assembler property.
  ASMJIT_API virtual void setProperty(uint32_t propertyId, uint32_t value);

  // --------------------------------------------------------------------------
  // [Capacity]
  // --------------------------------------------------------------------------

  //! @brief Get capacity of internal code buffer.
  inline size_t getCapacity() const
  { return _buffer.getCapacity(); }

  // --------------------------------------------------------------------------
  // [Offset]
  // --------------------------------------------------------------------------

  //! @brief Return current offset in buffer.
  inline size_t getOffset() const
  { return _buffer.getOffset(); }

  //! @brief Set offset to @a o and returns previous offset.
  //!
  //! This method can be used to truncate code (previous offset is not
  //! recorded) or to overwrite instruction stream at position @a o.
  //!
  //! @return Previous offset value that can be uset to set offset back later.
  inline size_t toOffset(size_t o)
  { return _buffer.toOffset(o); }

  // --------------------------------------------------------------------------
  // [GetCode / GetCodeSize]
  // --------------------------------------------------------------------------

  //! @brief Return start of assembler code buffer.
  //!
  //! Note that buffer address can change if you emit instruction or something
  //! else. Use this pointer only when you finished or make sure you do not
  //! use returned pointer after emitting.
  inline uint8_t* getCode() const
  { return _buffer.getData(); }

  //! @brief Return current offset in buffer (same as <code>getOffset() + getTramplineSize()</code>).
  inline size_t getCodeSize() const
  { return _buffer.getOffset() + getTrampolineSize(); }

  // --------------------------------------------------------------------------
  // [TakeCode]
  // --------------------------------------------------------------------------

  //! @brief Take internal code buffer and NULL all pointers (you take the ownership).
  ASMJIT_API uint8_t* takeCode();

  // --------------------------------------------------------------------------
  // [Clear / Reset]
  // --------------------------------------------------------------------------

  //! @brief Clear everything, but not deallocate buffers.
  ASMJIT_API void clear();

  //! @brief Reset everything (means also to free all buffers).
  ASMJIT_API void reset();

  //! @brief Called by clear() and reset() to clear all data related to derived
  //! class implementation.
  ASMJIT_API virtual void _purge();

  // --------------------------------------------------------------------------
  // [EnsureSpace]
  // --------------------------------------------------------------------------

  //! @brief Ensure space for next instruction.
  //!
  //! Note that this method can return false. It's rare and probably you never
  //! get this, but in some situations it's still possible.
  inline bool ensureSpace()
  { return _buffer.ensureSpace(); }

  // --------------------------------------------------------------------------
  // [GetTrampolineSize]
  // --------------------------------------------------------------------------

  //! @brief Get size of all possible trampolines needed to successfuly generate
  //! relative jumps to absolute addresses. This value is only non-zero if jmp
  //! of call instructions were used with immediate operand (this means jump or
  //! call absolute address directly).
  //!
  //! Currently only _emitJmpOrCallReloc() method can increase trampoline size
  //! value.
  inline size_t getTrampolineSize() const
  { return _trampolineSize; }

  // --------------------------------------------------------------------------
  // [Buffer - Getters]
  // --------------------------------------------------------------------------

  //! @brief Get byte at position @a pos.
  inline uint8_t getByteAt(size_t pos) const
  { return _buffer.getByteAt(pos); }
  
  //! @brief Get word at position @a pos.
  inline uint16_t getWordAt(size_t pos) const
  { return _buffer.getWordAt(pos); }
  
  //! @brief Get dword at position @a pos.
  inline uint32_t getDWordAt(size_t pos) const
  { return _buffer.getDWordAt(pos); }
  
  //! @brief Get qword at position @a pos.
  inline uint64_t getQWordAt(size_t pos) const
  { return _buffer.getQWordAt(pos); }

  //! @brief Get int32_t at position @a pos.
  inline int32_t getInt32At(size_t pos) const
  { return (int32_t)_buffer.getDWordAt(pos); }

  //! @brief Get int64_t at position @a pos.
  inline int64_t getInt64At(size_t pos) const
  { return (int64_t)_buffer.getQWordAt(pos); }

  //! @brief Get intptr_t at position @a pos.
  inline intptr_t getIntPtrTAt(size_t pos) const
  { return _buffer.getIntPtrTAt(pos); }

  //! @brief Get uintptr_t at position @a pos.
  inline uintptr_t getUIntPtrTAt(size_t pos) const
  { return _buffer.getUIntPtrTAt(pos); }

  //! @brief Get uintptr_t at position @a pos.
  inline size_t getSizeTAt(size_t pos) const
  { return _buffer.getSizeTAt(pos); }

  // --------------------------------------------------------------------------
  // [Buffer - Setters]
  // --------------------------------------------------------------------------

  //! @brief Set byte at position @a pos.
  inline void setByteAt(size_t pos, uint8_t x)
  { _buffer.setByteAt(pos, x); }
  
  //! @brief Set word at position @a pos.
  inline void setWordAt(size_t pos, uint16_t x)
  { _buffer.setWordAt(pos, x); }
  
  //! @brief Set dword at position @a pos.
  inline void setDWordAt(size_t pos, uint32_t x)
  { _buffer.setDWordAt(pos, x); }
  
  //! @brief Set qword at position @a pos.
  inline void setQWordAt(size_t pos, uint64_t x)
  { _buffer.setQWordAt(pos, x); }
  
  //! @brief Set int32_t at position @a pos.
  inline void setInt32At(size_t pos, int32_t x)
  { _buffer.setDWordAt(pos, (uint32_t)x); }

  //! @brief Set int64_t at position @a pos.
  inline void setInt64At(size_t pos, int64_t x)
  { _buffer.setQWordAt(pos, (uint64_t)x); }

  //! @brief Set intptr_t at position @a pos.
  inline void setIntPtrTAt(size_t pos, intptr_t x)
  { _buffer.setIntPtrTAt(pos, x); }

  //! @brief Set uintptr_t at position @a pos.
  inline void setUInt64At(size_t pos, uintptr_t x)
  { _buffer.setUIntPtrTAt(pos, x); }

  //! @brief Set size_t at position @a pos.
  inline void setSizeTAt(size_t pos, size_t x)
  { _buffer.setSizeTAt(pos, x); }

  // --------------------------------------------------------------------------
  // [CanEmit]
  // --------------------------------------------------------------------------

  //! @brief Get whether the instruction can be emitted.
  //!
  //! This function behaves like @c ensureSpace(), but it also checks if
  //! assembler is in error state and in that case it returns @c false.
  //! Assembler internally always uses this function before new instruction is
  //! emitted.
  //!
  //! It's implemented like:
  //!   <code>return ensureSpace() && !getError();</code>
  inline bool canEmit()
  {
    // If there is an error, we can't emit another instruction until last error
    // is cleared by calling @c setError(kErrorOk). If something caused the
    // error while generating code it's probably fatal in all cases. You can't
    // use generated code anymore, because you are not sure about the status.
    if (_error)
      return false;

    // The ensureSpace() method returns true on success and false on failure. We
    // are catching return value and setting error code here.
    if (ensureSpace())
      return true;

    // If we are here, there is memory allocation error. Note that this is HEAP
    // allocation error, virtual allocation error can be caused only by
    // AsmJit::VirtualMemory class!
    setError(kErrorNoHeapMemory);
    return false;
  }

  // --------------------------------------------------------------------------
  // [Emit]
  //
  // These functions are not protected against buffer overrun. Each place of
  // code which calls these functions ensures that there is some space using
  // canEmit() method. Emitters are internally protected in AsmJit::Buffer,
  // but only in debug builds.
  // --------------------------------------------------------------------------

  //! @brief Emit Byte to internal buffer.
  inline void _emitByte(uint8_t x)
  { _buffer.emitByte(x); }

  //! @brief Emit word (2 bytes) to internal buffer.
  inline void _emitWord(uint16_t x)
  { _buffer.emitWord(x); }

  //! @brief Emit dword (4 bytes) to internal buffer.
  inline void _emitDWord(uint32_t x)
  { _buffer.emitDWord(x); }

  //! @brief Emit qword (8 bytes) to internal buffer.
  inline void _emitQWord(uint64_t x)
  { _buffer.emitQWord(x); }

  //! @brief Emit Int32 (4 bytes) to internal buffer.
  inline void _emitInt32(int32_t x)
  { _buffer.emitDWord((uint32_t)x); }

  //! @brief Emit Int64 (8 bytes) to internal buffer.
  inline void _emitInt64(int64_t x)
  { _buffer.emitQWord((uint64_t)x); }

  //! @brief Emit intptr_t (4 or 8 bytes) to internal buffer.
  inline void _emitIntPtrT(intptr_t x)
  { _buffer.emitIntPtrT(x); }

  //! @brief Emit uintptr_t (4 or 8 bytes) to internal buffer.
  inline void _emitUIntPtrT(uintptr_t x)
  { _buffer.emitUIntPtrT(x); }

  //! @brief Emit size_t (4 or 8 bytes) to internal buffer.
  inline void _emitSizeT(size_t x)
  { _buffer.emitSizeT(x); }

  //! @brief Embed data into instruction stream.
  ASMJIT_API void embed(const void* data, size_t len);

  // --------------------------------------------------------------------------
  // [Reloc]
  // --------------------------------------------------------------------------

  //! @brief Relocate code to a given address @a dst.
  //!
  //! @param dst Where the relocated code should me stored. The pointer can be
  //! address returned by virtual memory allocator or your own address if you
  //! want only to store the code for later reuse (or load, etc...).
  //! @param addressBase Base address used for relocation. When using JIT code
  //! generation, this will be the same as @a dst, only casted to system
  //! integer type. But when generating code for remote process then the value
  //! can be different.
  //!
  //! @retval The bytes used. Code-generator can create trampolines which are
  //! used when calling other functions inside the JIT code. However, these
  //! trampolines can be unused so the relocCode() returns the exact size needed
  //! for the function.
  //!
  //! A given buffer will be overwritten, to get number of bytes required use
  //! @c getCodeSize().
  virtual size_t relocCode(void* dst, sysuint_t addressBase) const = 0;

  //! @brief Simplifed version of @c relocCode() method designed for JIT.
  //!
  //! @overload
  inline size_t relocCode(void* dst) const
  { return relocCode(dst, (uintptr_t)dst); }

  // --------------------------------------------------------------------------
  // [Make]
  // --------------------------------------------------------------------------

  //! @brief Make is convenience method to make currently serialized code and
  //! return pointer to generated function.
  //!
  //! What you need is only to cast this pointer to your function type and call
  //! it. Note that if there was an error and calling @c getError() method not
  //! returns @c kErrorOk (zero) then this function always return @c NULL and
  //! error value remains the same.
  virtual void* make() = 0;

  // --------------------------------------------------------------------------
  // [Helpers]
  // --------------------------------------------------------------------------

  ASMJIT_API LabelLink* _newLabelLink();

  // --------------------------------------------------------------------------
  // [Members]
  // --------------------------------------------------------------------------

  //! @brief ZoneMemory management.
  ZoneMemory _zoneMemory;
  //! @brief Binary code buffer.
  Buffer _buffer;

  //! @brief Context (for example @ref JitContext).
  Context* _context;
  //! @brief Logger.
  Logger* _logger;

  //! @brief Error code.
  uint32_t _error;
  //! @brief Properties.
  uint32_t _properties;
  //! @brief Emit flags for next instruction (cleared after emit).
  uint32_t _emitOptions;
  //! @brief Size of possible trampolines.
  uint32_t _trampolineSize;

  //! @brief Inline comment that will be logged by the next instruction and
  //! set to NULL.
  const char* _inlineComment;
  //! @brief Linked list of unused links (@c LabelLink* structures)
  LabelLink* _unusedLinks;

  //! @brief Labels data.
  PodVector<LabelData> _labels;
  //! @brief Relocations data.
  PodVector<RelocData> _relocData;
};

//! @}

} // AsmJit namespace

// [Api-End]
#include "../core/apiend.h"

// [Guard]
#endif // _ASMJIT_CORE_ASSEMBLER_H
