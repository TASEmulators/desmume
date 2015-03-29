// [AsmJit]
// Complete JIT Assembler for C++ Language.
//
// [License]
// Zlib - See COPYING file in this package.

#define ASMJIT_EXPORTS

// [Dependencies - AsmJit]
#include "../core/defs.h"
#include "../core/intutil.h"
#include "../core/zonememory.h"

// [Api-Begin]
#include "../core/apibegin.h"

namespace AsmJit {

// ============================================================================
// [AsmJit::ZoneMemory]
// ============================================================================

ZoneMemory::ZoneMemory(size_t chunkSize)
{
  _chunks = NULL;
  _total = 0;
  _chunkSize = chunkSize;
}

ZoneMemory::~ZoneMemory()
{
  reset();
}

void* ZoneMemory::alloc(size_t size)
{
  ZoneChunk* cur = _chunks;

  // Align to 4 or 8 bytes.
  size = IntUtil::align<size_t>(size, sizeof(size_t));

  if (cur == NULL || cur->getRemainingBytes() < size)
  {
    size_t chSize = _chunkSize;
 
    if (chSize < size)
      chSize = size;

    cur = (ZoneChunk*)ASMJIT_MALLOC(sizeof(ZoneChunk) - sizeof(void*) + chSize);
    if (cur == NULL)
      return NULL;

    cur->prev = _chunks;
    cur->pos = 0;
    cur->size = chSize;

    _chunks = cur;
  }

  uint8_t* p = cur->data + cur->pos;
  cur->pos += size;
  _total += size;

  ASMJIT_ASSERT(cur->pos <= cur->size);
  return (void*)p;
}

char* ZoneMemory::sdup(const char* str)
{
  if (str == NULL) return NULL;

  size_t len = strlen(str);
  if (len == 0) return NULL;

  // Include NULL terminator and limit string length.
  if (++len > 256)
    len = 256;

  char* m = reinterpret_cast<char*>(alloc(IntUtil::align<size_t>(len, 16)));
  if (m == NULL)
    return NULL;

  memcpy(m, str, len);
  m[len - 1] = '\0';
  return m;
}

void ZoneMemory::clear()
{
  ZoneChunk* cur = _chunks;

  if (cur == NULL)
    return;

  cur = cur->prev;
  while (cur != NULL)
  {
    ZoneChunk* prev = cur->prev;
    ASMJIT_FREE(cur);
    cur = prev;
  }

  _chunks->pos = 0;
  _chunks->prev = NULL;
  _total = 0;
}

void ZoneMemory::reset()
{
  ZoneChunk* cur = _chunks;

  _chunks = NULL;
  _total = 0;

  while (cur != NULL)
  {
    ZoneChunk* prev = cur->prev;
    ASMJIT_FREE(cur);
    cur = prev;
  }
}

} // AsmJit namespace

// [Api-End]
#include "../core/apiend.h"
