// [AsmJit]
// Complete JIT Assembler for C++ Language.
//
// [License]
// Zlib - See COPYING file in this package.

// [Guard]
#ifndef _ASMJIT_CORE_INTUTIL_H
#define _ASMJIT_CORE_INTUTIL_H

// [Dependencies - AsmJit]
#include "../core/assert.h"

// [Api-Begin]
#include "../core/apibegin.h"

namespace AsmJit {

//! @addtogroup AsmJit_Core
//! @{

// ============================================================================
// [AsmJit::I32FPUnion]
// ============================================================================

//! @internal
//!
//! @brief used to cast from float to 32-bit integer and vica versa.
union I32FPUnion
{
  //! @brief 32-bit signed integer value.
  int32_t i;
  //! @brief 32-bit SP-FP value.
  float f;
};

// ============================================================================
// [AsmJit::I64FPUnion]
// ============================================================================

//! @internal
//!
//! @brief used to cast from double to 64-bit integer and vica versa.
union I64FPUnion
{
  //! @brief 64-bit signed integer value.
  int64_t i;
  //! @brief 64-bit DP-FP value.
  double f;
};

// ============================================================================
// [AsmJit::IntUtil]
// ============================================================================

namespace IntUtil
{
  // --------------------------------------------------------------------------
  // [Min/Max]
  // --------------------------------------------------------------------------

  // NOTE: Because some environments declare min() and max() as macros, we
  // decided to use different name so we never collide.

  template<typename T>
  static inline T _min(const T& a, const T& b) { return a < b ? a : b; }
  template<typename T>
  static inline T _max(const T& a, const T& b) { return a > b ? a : b; }

  // --------------------------------------------------------------------------
  // [Limits]
  // --------------------------------------------------------------------------

  template<typename T>
  static inline T maxValue() { return ~T(0); }

  // --------------------------------------------------------------------------
  // [IsInt / IsUInt]
  // --------------------------------------------------------------------------

  //! @brief Returns @c true if a given integer @a x is signed 8-bit integer
  static inline bool isInt8(intptr_t x) { return x >= -128 && x <= 127; }
  //! @brief Returns @c true if a given integer @a x is unsigned 8-bit integer
  static inline bool isUInt8(intptr_t x) { return x >= 0 && x <= 255; }

  //! @brief Returns @c true if a given integer @a x is signed 16-bit integer
  static inline bool isInt16(intptr_t x) { return x >= -32768 && x <= 32767; }
  //! @brief Returns @c true if a given integer @a x is unsigned 16-bit integer
  static inline bool isUInt16(intptr_t x) { return x >= 0 && x <= 65535; }

  //! @brief Returns @c true if a given integer @a x is signed 16-bit integer
  static inline bool isInt32(intptr_t x)
  {
#if defined(ASMJIT_X86)
    return true;
#else
    return x >= ASMJIT_INT64_C(-2147483648) && x <= ASMJIT_INT64_C(2147483647);
#endif
  }
  //! @brief Returns @c true if a given integer @a x is unsigned 16-bit integer
  static inline bool isUInt32(intptr_t x)
  {
#if defined(ASMJIT_X86)
    return x >= 0;
#else
    return x >= 0 && x <= ASMJIT_INT64_C(4294967295);
#endif
  }

  // --------------------------------------------------------------------------
  // [Masking]
  // --------------------------------------------------------------------------

  static inline uint32_t maskFromIndex(uint32_t x)
  {
    ASMJIT_ASSERT(x < 32);
    return (1U << x);
  }

  static inline uint32_t maskUpToIndex(uint32_t x)
  {
    if (x >= 32)
      return 0xFFFFFFFF;
    else
      return (1U << x) - 1;
  }

  // --------------------------------------------------------------------------
  // [Bits]
  // --------------------------------------------------------------------------

  // From http://graphics.stanford.edu/~seander/bithacks.html .
  static inline uint32_t bitCount(uint32_t x)
  {
    x = x - ((x >> 1) & 0x55555555U);
    x = (x & 0x33333333U) + ((x >> 2) & 0x33333333U);
    return (((x + (x >> 4)) & 0x0F0F0F0FU) * 0x01010101U) >> 24;
  }

  static inline uint32_t findFirstBit(uint32_t mask)
  {
    for (uint32_t i = 0; i < sizeof(uint32_t) * 8; i++, mask >>= 1)
    {
      if (mask & 0x1)
        return i;
    }

    // kInvalidValue.
    return 0xFFFFFFFF;
  }

  // --------------------------------------------------------------------------
  // [Alignment]
  // --------------------------------------------------------------------------

  template<typename T>
  static inline bool isAligned(T base, T alignment)
  {
    return (base % alignment) == 0;
  }

  //! @brief Align @a base to @a alignment.
  template<typename T>
  static inline T align(T base, T alignment)
  {
    return (base + (alignment - 1)) & ~(alignment - 1);
  }

  //! @brief Get delta required to align @a base to @a alignment.
  template<typename T>
  static inline T delta(T base, T alignment)
  {
    return align(base, alignment) - base;
  }

  // --------------------------------------------------------------------------
  // [Round]
  // --------------------------------------------------------------------------

  template<typename T>
  static inline T roundUp(T base, T alignment)
  {
    T over = base % alignment;
    return base + (over > 0 ? alignment - over : 0);
  }

  template<typename T>
  static inline T roundUpToPowerOf2(T base)
  {
    // Implementation is from "Hacker's Delight" by Henry S. Warren, Jr.,
    // figure 3-3, page 48, where the function is called clp2.
    base -= 1;

    // I'm trying to make this portable and MSVC strikes me the warning C4293:
    //   "Shift count negative or too big, undefined behavior"
    // Fixing...
#if defined(_MSC_VER)
# pragma warning(push)
# pragma warning(disable: 4293)
#endif // _MSC_VER

    base = base | (base >> 1);
    base = base | (base >> 2);
    base = base | (base >> 4);

    if (sizeof(T) >= 2) base = base | (base >>  8);
    if (sizeof(T) >= 4) base = base | (base >> 16);
    if (sizeof(T) >= 8) base = base | (base >> 32);

#if defined(_MSC_VER)
# pragma warning(pop)
#endif // _MSC_VER

    return base + 1;
  }

  // --------------------------------------------------------------------------
  // [Cast]
  // --------------------------------------------------------------------------

  //! @brief Binary cast from 32-bit integer to SP-FP value (@c float).
  static inline float int32AsFloat(int32_t i)
  {
    I32FPUnion u;
    u.i = i;
    return u.f;
  }

  //! @brief Binary cast SP-FP value (@c float) to 32-bit integer.
  static inline int32_t floatAsInt32(float f)
  {
    I32FPUnion u;
    u.f = f;
    return u.i;
  }

  //! @brief Binary cast from 64-bit integer to DP-FP value (@c double).
  static inline double int64AsDouble(int64_t i)
  {
    I64FPUnion u;
    u.i = i;
    return u.f;
  }

  //! @brief Binary cast from DP-FP value (@c double) to 64-bit integer.
  static inline int64_t doubleAsInt64(double f)
  {
    I64FPUnion u;
    u.f = f;
    return u.i;
  }
};

//! @}

} // AsmJit namespace

// [Api-End]
#include "../core/apiend.h"

// [Guard]
#endif // _ASMJIT_CORE_INTUTIL_H
