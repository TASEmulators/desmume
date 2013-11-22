// [AsmJit]
// Complete JIT Assembler for C++ Language.
//
// [License]
// Zlib - See COPYING file in this package.

// [Guard]
#ifndef _ASMJIT_CORE_LOCK_H
#define _ASMJIT_CORE_LOCK_H

// [Dependencies - AsmJit]
#include "../core/build.h"

// [Dependencies - Windows]
#if defined(ASMJIT_WINDOWS)
# include <windows.h>
#endif // ASMJIT_WINDOWS

// [Dependencies - Posix]
#if defined(ASMJIT_POSIX)
# include <pthread.h>
#endif // ASMJIT_POSIX

// [Api-Begin]
#include "../core/apibegin.h"

namespace AsmJit {

//! @addtogroup AsmJit_Core
//! @{

// ============================================================================
// [AsmJit::Lock]
// ============================================================================

//! @brief Lock - used in thread-safe code for locking.
struct Lock
{
  ASMJIT_NO_COPY(Lock)

  // --------------------------------------------------------------------------
  // [Windows]
  // --------------------------------------------------------------------------

#if defined(ASMJIT_WINDOWS)
  typedef CRITICAL_SECTION Handle;

  //! @brief Create a new @ref Lock instance.
  inline Lock() { InitializeCriticalSection(&_handle); }
  //! @brief Destroy the @ref Lock instance.
  inline ~Lock() { DeleteCriticalSection(&_handle); }

  //! @brief Lock.
  inline void lock() { EnterCriticalSection(&_handle); }
  //! @brief Unlock.
  inline void unlock() { LeaveCriticalSection(&_handle); }

#endif // ASMJIT_WINDOWS

  // --------------------------------------------------------------------------
  // [Posix]
  // --------------------------------------------------------------------------

#if defined(ASMJIT_POSIX)
  typedef pthread_mutex_t Handle;

  //! @brief Create a new @ref Lock instance.
  inline Lock() { pthread_mutex_init(&_handle, NULL); }
  //! @brief Destroy the @ref Lock instance.
  inline ~Lock() { pthread_mutex_destroy(&_handle); }

  //! @brief Lock.
  inline void lock() { pthread_mutex_lock(&_handle); }
  //! @brief Unlock.
  inline void unlock() { pthread_mutex_unlock(&_handle); }
#endif // ASMJIT_POSIX

  // --------------------------------------------------------------------------
  // [Accessors]
  // --------------------------------------------------------------------------

  //! @brief Get handle.
  inline Handle& getHandle() { return _handle; }
  //! @overload
  inline const Handle& getHandle() const { return _handle; }

  // --------------------------------------------------------------------------
  // [Members]
  // --------------------------------------------------------------------------

  //! @brief Handle.
  Handle _handle;
};

// ============================================================================
// [AsmJit::AutoLock]
// ============================================================================

//! @brief Scope auto locker.
struct AutoLock
{
  ASMJIT_NO_COPY(AutoLock)

  // --------------------------------------------------------------------------
  // [Construction / Destruction]
  // --------------------------------------------------------------------------

  //! @brief Locks @a target.
  inline AutoLock(Lock& target) : _target(target)
  {
    _target.lock();
  }

  //! @brief Unlocks target.
  inline ~AutoLock()
  {
    _target.unlock();
  }

  // --------------------------------------------------------------------------
  // [Members]
  // --------------------------------------------------------------------------

  //! @brief Pointer to target (lock).
  Lock& _target;
};

//! @}

} // AsmJit namespace

// [Api-End]
#include "../core/apiend.h"

// [Guard]
#endif // _ASMJIT_CORE_LOCK_H
