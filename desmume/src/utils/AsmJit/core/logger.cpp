// [AsmJit]
// Complete JIT Assembler for C++ Language.
//
// [License]
// Zlib - See COPYING file in this package.

#define ASMJIT_EXPORTS

// [Dependencies - AsmJit]
#include "../core/logger.h"

// [Dependencies - C]
#include <stdarg.h>

// [Api-Begin]
#include "../core/apibegin.h"

namespace AsmJit {

// ============================================================================
// [AsmJit::Logger - Construction / Destruction]
// ============================================================================

Logger::Logger() :
  _flags(kLoggerIsEnabled | kLoggerIsUsed)
{
  memset(_instructionPrefix, 0, ASMJIT_ARRAY_SIZE(_instructionPrefix));
}

Logger::~Logger()
{
}

// ============================================================================
// [AsmJit::Logger - Logging]
// ============================================================================

void Logger::logFormat(const char* fmt, ...)
{
  char buf[1024];
  size_t len;

  va_list ap;
  va_start(ap, fmt);
  len = vsnprintf(buf, 1023, fmt, ap);
  va_end(ap);

  logString(buf, len);
}

// ============================================================================
// [AsmJit::Logger - Enabled]
// ============================================================================

void Logger::setEnabled(bool enabled)
{
  if (enabled)
    _flags |= kLoggerIsEnabled | kLoggerIsUsed;
  else
    _flags &= ~(kLoggerIsEnabled | kLoggerIsUsed);
}

// ============================================================================
// [AsmJit::Logger - LogBinary]
// ============================================================================

void Logger::setLogBinary(bool value)
{
  if (value)
    _flags |= kLoggerOutputBinary;
  else
    _flags &= ~kLoggerOutputBinary;
}

// ============================================================================
// [AsmJit::Logger - HexImmediate]
// ============================================================================

void Logger::setHexImmediate(bool value)
{
  if (value)
    _flags |= kLoggerOutputHexImmediate;
  else
    _flags &= ~kLoggerOutputHexImmediate;
}

// ============================================================================
// [AsmJit::Logger - HexDisplacement]
// ============================================================================

void Logger::setHexDisplacement(bool value)
{
  if (value)
    _flags |= kLoggerOutputHexDisplacement;
  else
    _flags &= ~kLoggerOutputHexDisplacement;
}

// ============================================================================
// [AsmJit::Logger - InstructionPrefix]
// ============================================================================

void Logger::setInstructionPrefix(const char* prefix)
{
  memset(_instructionPrefix, 0, ASMJIT_ARRAY_SIZE(_instructionPrefix));

  if (!prefix)
    return;

  size_t length = strnlen(prefix, ASMJIT_ARRAY_SIZE(_instructionPrefix) - 1);
  memcpy(_instructionPrefix, prefix, length);
}

// ============================================================================
// [AsmJit::FileLogger - Construction / Destruction]
// ============================================================================

FileLogger::FileLogger(FILE* stream)
  : _stream(NULL)
{
  setStream(stream);
}

FileLogger::~FileLogger()
{
}

// ============================================================================
// [AsmJit::FileLogger - Accessors]
// ============================================================================

//! @brief Set file stream.
void FileLogger::setStream(FILE* stream)
{
  _stream = stream;

  if (isEnabled() && _stream != NULL)
    _flags |= kLoggerIsUsed;
  else
    _flags &= ~kLoggerIsUsed;
}

// ============================================================================
// [AsmJit::FileLogger - Logging]
// ============================================================================

void FileLogger::logString(const char* buf, size_t len)
{
  if (!isUsed())
    return;

  if (len == kInvalidSize)
    len = strlen(buf);

  fwrite(buf, 1, len, _stream);
}

// ============================================================================
// [AsmJit::FileLogger - Enabled]
// ============================================================================

void FileLogger::setEnabled(bool enabled)
{
  if (enabled)
    _flags |= kLoggerIsEnabled | (_stream != NULL ? kLoggerIsUsed : 0);
  else
    _flags &= ~(kLoggerIsEnabled | kLoggerIsUsed);
}

// ============================================================================
// [AsmJit::StringLogger - Construction / Destruction]
// ============================================================================

StringLogger::StringLogger()
{
}

StringLogger::~StringLogger()
{
}

// ============================================================================
// [AsmJit::StringLogger - Logging]
// ============================================================================

void StringLogger::logString(const char* buf, size_t len)
{
  if (!isUsed())
    return;
  _stringBuilder.appendString(buf, len);
}

} // AsmJit namespace

// [Api-End]
#include "../core/apiend.h"
