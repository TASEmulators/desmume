// [AsmJit]
// Complete JIT Assembler for C++ Language.
//
// [License]
// Zlib - See COPYING file in this package.

#define ASMJIT_EXPORTS

// [Dependencies - AsmJit]
#include "../core/assembler.h"
#include "../core/compiler.h"
#include "../core/compilercontext.h"
#include "../core/compilerfunc.h"
#include "../core/compileritem.h"
#include "../core/intutil.h"
#include "../core/logger.h"

// [Dependencies - C]
#include <stdarg.h>

// [Api-Begin]
#include "../core/apibegin.h"

namespace AsmJit {

// ============================================================================
// [AsmJit::CompilerItem - Construction / Destruction]
// ============================================================================

CompilerItem::CompilerItem(Compiler* compiler, uint32_t type) :
  _compiler(compiler),
  _prev(NULL),
  _next(NULL),
  _comment(NULL),
  _type(static_cast<uint8_t>(type)),
  _isTranslated(false),
  _isUnreachable(false),
  _reserved(0),
  _offset(kInvalidValue)
{
}

CompilerItem::~CompilerItem()
{
}

// ============================================================================
// [AsmJit::CompilerItem - Interface]
// ============================================================================

void CompilerItem::prepare(CompilerContext& cc)
{
  _offset = cc._currentOffset;
}

CompilerItem* CompilerItem::translate(CompilerContext& cc)
{
  return translated();
}

void CompilerItem::emit(Assembler& a) {}
void CompilerItem::post(Assembler& a) {}

// ============================================================================
// [AsmJit::CompilerItem - Misc]
// ============================================================================

int CompilerItem::getMaxSize() const
{
  // Default maximum size is -1 which means that it's not known.
  return -1;
}

bool CompilerItem::_tryUnuseVar(CompilerVar* v)
{
  return false;
}

// ============================================================================
// [AsmJit::CompilerItem - Comment]
// ============================================================================

void CompilerItem::setComment(const char* str)
{
  _comment = _compiler->getZoneMemory().sdup(str);
}

void CompilerItem::formatComment(const char* fmt, ...)
{
  // The capacity should be large enough.
  char buf[128];

  va_list ap;
  va_start(ap, fmt);
  vsnprintf(buf, ASMJIT_ARRAY_SIZE(buf), fmt, ap);
  va_end(ap);

  // I don't know if vsnprintf can produce non-null terminated string, in case
  // it can, we terminate it here.
  buf[ASMJIT_ARRAY_SIZE(buf) - 1] = '\0';

  setComment(buf);
}

// ============================================================================
// [AsmJit::CompilerMark - Construction / Destruction]
// ============================================================================

CompilerMark::CompilerMark(Compiler* compiler) :
  CompilerItem(compiler, kCompilerItemMark)
{
}

CompilerMark::~CompilerMark()
{
}

// ============================================================================
// [AsmJit::CompilerMark - Misc]
// ============================================================================

int CompilerMark::getMaxSize() const
{
  return 0;
}

// ============================================================================
// [AsmJit::CompilerComment - Construction / Destruction]
// ============================================================================

CompilerComment::CompilerComment(Compiler* compiler, const char* str) :
  CompilerItem(compiler, kCompilerItemComment)
{
  if (str != NULL)
    setComment(str);
}

CompilerComment::~CompilerComment()
{
}

// ============================================================================
// [AsmJit::CompilerComment - Interface]
// ============================================================================

void CompilerComment::emit(Assembler& a)
{
  Logger* logger = a.getLogger();
  if (logger == NULL || !logger->isUsed())
    return;

  logger->logString(logger->getInstructionPrefix());
  logger->logString(getComment());
}

// ============================================================================
// [AsmJit::CompilerComment - Misc]
// ============================================================================

int CompilerComment::getMaxSize() const
{
  return 0;
}

// ============================================================================
// [AsmJit::CompilerEmbed - Construction / Destruction]
// ============================================================================

CompilerEmbed::CompilerEmbed(Compiler* compiler, const void* data, size_t length) :
  CompilerItem(compiler, kCompilerItemEmbed)
{
  _length = length;
  memcpy(_data, data, length);
}

CompilerEmbed::~CompilerEmbed()
{
}

// ============================================================================
// [AsmJit::CompilerEmbed - Interface]
// ============================================================================

void CompilerEmbed::emit(Assembler& a)
{
  a.embed(_data, _length);
}

// ============================================================================
// [AsmJit::CompilerEmbed - Misc]
// ============================================================================

int CompilerEmbed::getMaxSize() const
{
  return (int)_length;;
}

// ============================================================================
// [AsmJit::CompilerAlign - Construction / Destruction]
// ============================================================================

CompilerAlign::CompilerAlign(Compiler* compiler, uint32_t size) :
  CompilerItem(compiler, kCompilerItemAlign), _size(size)
{
}

CompilerAlign::~CompilerAlign()
{
}

// ============================================================================
// [AsmJit::CompilerAlign - Misc]
// ============================================================================

int CompilerAlign::getMaxSize() const
{
  if (_size == 0)
    return 0;
  else
    return static_cast<int>(_size - 1);
}

// ============================================================================
// [AsmJit::CompilerHint - Construction / Destruction]
// ============================================================================

CompilerHint::CompilerHint(Compiler* compiler, CompilerVar* var, uint32_t hintId, uint32_t hintValue) :
  CompilerItem(compiler, kCompilerItemHint),
  _var(var),
  _hintId(hintId),
  _hintValue(hintValue)
{
  ASMJIT_ASSERT(var != NULL);
}

CompilerHint::~CompilerHint()
{
}

// ============================================================================
// [AsmJit::CompilerTarget - Construction / Destruction]
// ============================================================================

CompilerTarget::CompilerTarget(Compiler* compiler, const Label& label) :
  CompilerItem(compiler, kCompilerItemTarget),
  _label(label),
  _from(NULL),
  _state(NULL),
  _jumpsCount(0)
{
}

CompilerTarget::~CompilerTarget()
{
}

// ============================================================================
// [AsmJit::CompilerTarget - Misc]
// ============================================================================

int CompilerTarget::getMaxSize() const
{
  return 0;
}

// ============================================================================
// [AsmJit::CompilerInst - Construction / Destruction]
// ============================================================================

CompilerInst::CompilerInst(Compiler* compiler, uint32_t code, Operand* opData, uint32_t opCount) :
  CompilerItem(compiler, kCompilerItemInst),
  _code(code),
  _emitOptions(static_cast<uint8_t>(compiler->_emitOptions)),
  _instFlags(0),
  _operandsCount(static_cast<uint8_t>(opCount)),
  _variablesCount(0),
  _operands(opData)
{
  // Each created instruction takes emit options and clears it.
  compiler->_emitOptions = 0;
}

CompilerInst::~CompilerInst()
{
}

// ============================================================================
// [AsmJit::CompilerInst - GetJumpTarget]
// ============================================================================

CompilerTarget* CompilerInst::getJumpTarget() const
{
  return NULL;
}

} // AsmJit namespace

// [Api-Begin]
#include "../core/apibegin.h"
