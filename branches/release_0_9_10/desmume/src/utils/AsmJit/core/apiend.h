// [AsmJit]
// Complete JIT Assembler for C++ Language.
//
// [License]
// Zlib - See COPYING file in this package.

// [MSVC]
#if defined(_MSC_VER)

// Pop disabled warnings by ApiBegin.h
#pragma warning(pop)

// Rename symbols back.
#undef vsnprintf
#undef snprintf

#endif // _MSC_VER

// [GNUC]
#if defined(__GNUC__)
#endif // __GNUC__
