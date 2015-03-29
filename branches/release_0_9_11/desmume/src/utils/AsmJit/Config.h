// [AsmJit]
// Complete JIT Assembler for C++ Language.
//
// [License]
// Zlib - See COPYING file in this package.

// [Guard]
#ifndef _ASMJIT_CONFIG_H
#define _ASMJIT_CONFIG_H

// This file is designed to be modifyable. Platform specific changes should
// be applied to this file so it's guaranteed that never versions of AsmJit
// library will never overwrite generated config files.
//
// So modify this file by your build system or by hand.

// ============================================================================
// [AsmJit - OS]
// ============================================================================

// Provides definitions about your operating system. It's detected by default,
// so override it if you have problems with automatic detection.
//
// #define ASMJIT_WINDOWS
// #define ASMJIT_POSIX

// ============================================================================
// [AsmJit - Architecture]
// ============================================================================

// Provides definitions about your cpu architecture. It's detected by default,
// so override it if you have problems with automatic detection.

// #define ASMJIT_X86
// #define ASMJIT_X64

// ============================================================================
// [AsmJit - API]
// ============================================================================

// If you are embedding AsmJit library into your project (statically), undef
// ASMJIT_API macro.
#define ASMJIT_API

// ============================================================================
// [AsmJit - Memory Management]
// ============================================================================

// #define ASMJIT_MALLOC ::malloc
// #define ASMJIT_REALLOC ::realloc
// #define ASMJIT_FREE ::free

// ============================================================================
// [AsmJit - Debug]
// ============================================================================

// Turn debug on/off (to bypass autodetection)
// #define ASMJIT_DEBUG
// #define ASMJIT_NO_DEBUG

// Setup custom assertion code.
// #define ASMJIT_ASSERT(exp) do { if (!(exp)) ::AsmJit::assertionFailure(__FILE__, __LINE__, #exp); } while(0)

// [Guard]
#endif // _ASMJIT_CONFIG_H
