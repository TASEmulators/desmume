// [AsmJit]
// Complete JIT Assembler for C++ Language.
//
// [License]
// Zlib - See COPYING file in this package.

// [Guard]
#ifndef _ASMJIT_CORE_LOGGER_H
#define _ASMJIT_CORE_LOGGER_H

// [Dependencies - AsmJit]
#include "../core/build.h"
#include "../core/defs.h"
#include "../core/stringbuilder.h"

// [Dependencies - C]
#include <stdarg.h>

// [Api-Begin]
#include "../core/apibegin.h"

namespace AsmJit {

//! @addtogroup AsmJit_Logging
//! @{

// ============================================================================
// [AsmJit::Logger]
// ============================================================================

//! @brief Abstract logging class.
//!
//! This class can be inherited and reimplemented to fit into your logging
//! subsystem. When reimplementing use @c AsmJit::Logger::log() method to
//! log into your stream.
//!
//! This class also contain @c _enabled member that can be used to enable
//! or disable logging.
struct Logger
{
  ASMJIT_NO_COPY(Logger)

  // --------------------------------------------------------------------------
  // [Construction / Destruction]
  // --------------------------------------------------------------------------

  //! @brief Create logger.
  ASMJIT_API Logger();
  //! @brief Destroy logger.
  ASMJIT_API virtual ~Logger();

  // --------------------------------------------------------------------------
  // [Logging]
  // --------------------------------------------------------------------------

  //! @brief Abstract method to log output.
  //!
  //! Default implementation that is in @c AsmJit::Logger is to do nothing.
  //! It's virtual to fit to your logging system.
  virtual void logString(const char* buf, size_t len = kInvalidSize) = 0;

  //! @brief Log formatter message (like sprintf) sending output to @c logString() method.
  ASMJIT_API virtual void logFormat(const char* fmt, ...);

  // --------------------------------------------------------------------------
  // [Flags]
  // --------------------------------------------------------------------------

  //! @brief Get logger flags (used internally by Assembler/Compiler).
  inline uint32_t getFlags() const { return _flags; }

  // --------------------------------------------------------------------------
  // [Enabled]
  // --------------------------------------------------------------------------

  //! @brief Return @c true if logging is enabled.
  inline bool isEnabled() const { return (_flags & kLoggerIsEnabled) != 0; }

  //! @brief Set logging to enabled or disabled.
  ASMJIT_API virtual void setEnabled(bool enabled);

  // --------------------------------------------------------------------------
  // [Used]
  // --------------------------------------------------------------------------

  //! @brief Get whether the logger should be used.
  inline bool isUsed() const { return (_flags & kLoggerIsUsed) != 0; }

  // --------------------------------------------------------------------------
  // [LogBinary]
  // --------------------------------------------------------------------------

  //! @brief Get whether logging of binary output is enabled.
  inline bool getLogBinary() const { return (_flags & kLoggerOutputBinary) != 0; }
  //! @brief Enable or disable binary output logging.
  ASMJIT_API void setLogBinary(bool value);

  // --------------------------------------------------------------------------
  // [HexImmediate]
  // --------------------------------------------------------------------------

  inline bool getHexImmediate() const { return (_flags & kLoggerOutputHexImmediate) != 0; }
  ASMJIT_API void setHexImmediate(bool value);

  // --------------------------------------------------------------------------
  // [HexDisplacement]
  // --------------------------------------------------------------------------

  inline bool getHexDisplacement() const { return (_flags & kLoggerOutputHexDisplacement) != 0; }
  ASMJIT_API void setHexDisplacement(bool value);

  // --------------------------------------------------------------------------
  // [InstructionPrefix]
  // --------------------------------------------------------------------------

  //! @brief Get instruction prefix.
  inline const char* getInstructionPrefix() const { return _instructionPrefix; }
  //! @brief Set instruction prefix.
  ASMJIT_API void setInstructionPrefix(const char* prefix);
  //! @brief Reset instruction prefix.
  inline void resetInstructionPrefix() { setInstructionPrefix(NULL); }

  // --------------------------------------------------------------------------
  // [Members]
  // --------------------------------------------------------------------------

  //! @brief Flags, see @ref kLoggerFlag.
  uint32_t _flags;

  //! @brief Instrictions and macro-instructions prefix.
  char _instructionPrefix[12];
};

// ============================================================================
// [AsmJit::FileLogger]
// ============================================================================

//! @brief Logger that can log to standard C @c FILE* stream.
struct FileLogger : public Logger
{
  ASMJIT_NO_COPY(FileLogger)

  // --------------------------------------------------------------------------
  // [Construction / Destruction]
  // --------------------------------------------------------------------------

  //! @brief Create a new @c FileLogger.
  //! @param stream FILE stream where logging will be sent (can be @c NULL
  //! to disable logging).
  ASMJIT_API FileLogger(FILE* stream = NULL);

  //! @brief Destroy the @ref FileLogger.
  ASMJIT_API virtual ~FileLogger();

  // --------------------------------------------------------------------------
  // [Accessors]
  // --------------------------------------------------------------------------

  //! @brief Get @c FILE* stream.
  //!
  //! @note Return value can be @c NULL.
  inline FILE* getStream() const { return _stream; }

  //! @brief Set @c FILE* stream.
  //!
  //! @param stream @c FILE stream where to log output (can be @c NULL to
  //! disable logging).
  ASMJIT_API void setStream(FILE* stream);

  // --------------------------------------------------------------------------
  // [Logging]
  // --------------------------------------------------------------------------

  ASMJIT_API virtual void logString(const char* buf, size_t len = kInvalidSize);

  // --------------------------------------------------------------------------
  // [Enabled]
  // --------------------------------------------------------------------------

  ASMJIT_API virtual void setEnabled(bool enabled);

  // --------------------------------------------------------------------------
  // [Members]
  // --------------------------------------------------------------------------

  //! @brief C file stream.
  FILE* _stream;
};

// ============================================================================
// [AsmJit::StringLogger]
// ============================================================================

//! @brief String logger.
struct StringLogger : public Logger
{
  ASMJIT_NO_COPY(StringLogger)

  // --------------------------------------------------------------------------
  // [Construction / Destruction]
  // --------------------------------------------------------------------------

  //! @brief Create new @ref StringLogger.
  ASMJIT_API StringLogger();

  //! @brief Destroy the @ref StringLogger.
  ASMJIT_API virtual ~StringLogger();

  // --------------------------------------------------------------------------
  // [Accessors]
  // --------------------------------------------------------------------------

  //! @brief Get <code>char*</code> pointer which represents the serialized
  //! string.
  //!
  //! The pointer is owned by @ref StringLogger, it can't be modified or freed.
  inline const char* getString() const { return _stringBuilder.getData(); }

  //! @brief Clear the serialized string.
  inline void clearString() { _stringBuilder.clear(); }

  // --------------------------------------------------------------------------
  // [Logging]
  // --------------------------------------------------------------------------

  ASMJIT_API virtual void logString(const char* buf, size_t len = kInvalidSize);

  // --------------------------------------------------------------------------
  // [Members]
  // --------------------------------------------------------------------------

  //! @brief Output.
  StringBuilder _stringBuilder;
};

//! @}

} // AsmJit namespace

// [Api-End]
#include "../core/apiend.h"

// [Guard]
#endif // _ASMJIT_CORE_LOGGER_H
