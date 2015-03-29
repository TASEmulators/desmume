// [AsmJit]
// Complete JIT Assembler for C++ Language.
//
// [License]
// Zlib - See COPYING file in this package.

#define ASMJIT_EXPORTS

// [Dependencies - AsmJit]
#include "../core/assert.h"
#include "../core/stringutil.h"

// [Api-Begin]
#include "../core/apibegin.h"

namespace AsmJit {

// ============================================================================
// [AsmJit::StringUtil]
// ============================================================================

static const char letters[] = "0123456789ABCDEF";

char* StringUtil::copy(char* dst, const char* src, size_t len)
{
  if (src == NULL)
    return dst;

  if (len == kInvalidSize)
  {
    while (*src) *dst++ = *src++;
  }
  else
  {
    memcpy(dst, src, len);
    dst += len;
  }

  return dst;
}

char* StringUtil::fill(char* dst, const int c, size_t len)
{
  memset(dst, c, len);
  return dst + len;
}

char* StringUtil::hex(char* dst, const uint8_t* src, size_t len)
{
  for (size_t i = len; i; i--, dst += 2, src += 1)
  {
    dst[0] = letters[(src[0] >> 4) & 0xF];
    dst[1] = letters[(src[0]     ) & 0xF];
  }

  return dst;
}

// Not too efficient, but this is mainly for debugging:)
char* StringUtil::utoa(char* dst, uintptr_t i, size_t base)
{
  ASMJIT_ASSERT(base <= 16);

  char buf[128];
  char* p = buf + 128;

  do {
    uintptr_t b = i % base;
    *--p = letters[b];
    i /= base;
  } while (i);

  return StringUtil::copy(dst, p, (size_t)(buf + 128 - p));
}

char* StringUtil::itoa(char* dst, intptr_t i, size_t base)
{
  if (i < 0)
  {
    *dst++ = '-';
    i = -i;
  }

  return StringUtil::utoa(dst, (size_t)i, base);
}

} // AsmJit namespace

// [Api-End]
#include "../core/apiend.h"
