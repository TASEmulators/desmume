// [AsmJit]
// Complete JIT Assembler for C++ Language.
//
// [License]
// Zlib - See COPYING file in this package.

#define ASMJIT_EXPORTS

// [Dependencies - AsmJit]
#include "../x86/x86defs.h"
#include "../x86/x86operand.h"

// [Api-Begin]
#include "../core/apibegin.h"

namespace AsmJit {

// ============================================================================
// [AsmJit::Registers - no_reg]
// ============================================================================

const GpReg no_reg(_Initialize(), kInvalidValue);

// ============================================================================
// [AsmJit::Registers - 8-bit]
// ============================================================================

const GpReg al(_Initialize(), kX86RegAl);
const GpReg cl(_Initialize(), kX86RegCl);
const GpReg dl(_Initialize(), kX86RegDl);
const GpReg bl(_Initialize(), kX86RegBl);

#if defined(ASMJIT_X64)
const GpReg spl(_Initialize(), kX86RegSpl);
const GpReg bpl(_Initialize(), kX86RegBpl);
const GpReg sil(_Initialize(), kX86RegSil);
const GpReg dil(_Initialize(), kX86RegDil);

const GpReg r8b(_Initialize(), kX86RegR8b);
const GpReg r9b(_Initialize(), kX86RegR9b);
const GpReg r10b(_Initialize(), kX86RegR10b);
const GpReg r11b(_Initialize(), kX86RegR11b);
const GpReg r12b(_Initialize(), kX86RegR12b);
const GpReg r13b(_Initialize(), kX86RegR13b);
const GpReg r14b(_Initialize(), kX86RegR14b);
const GpReg r15b(_Initialize(), kX86RegR15b);
#endif // ASMJIT_X64

const GpReg ah(_Initialize(), kX86RegAh);
const GpReg ch(_Initialize(), kX86RegCh);
const GpReg dh(_Initialize(), kX86RegDh);
const GpReg bh(_Initialize(), kX86RegBh);

// ============================================================================
// [AsmJit::Registers - 16-bit]
// ============================================================================

const GpReg ax(_Initialize(), kX86RegAx);
const GpReg cx(_Initialize(), kX86RegCx);
const GpReg dx(_Initialize(), kX86RegDx);
const GpReg bx(_Initialize(), kX86RegBx);
const GpReg sp(_Initialize(), kX86RegSp);
const GpReg bp(_Initialize(), kX86RegBp);
const GpReg si(_Initialize(), kX86RegSi);
const GpReg di(_Initialize(), kX86RegDi);

#if defined(ASMJIT_X64)
const GpReg r8w(_Initialize(), kX86RegR8w);
const GpReg r9w(_Initialize(), kX86RegR9w);
const GpReg r10w(_Initialize(), kX86RegR10w);
const GpReg r11w(_Initialize(), kX86RegR11w);
const GpReg r12w(_Initialize(), kX86RegR12w);
const GpReg r13w(_Initialize(), kX86RegR13w);
const GpReg r14w(_Initialize(), kX86RegR14w);
const GpReg r15w(_Initialize(), kX86RegR15w);
#endif // ASMJIT_X64

// ============================================================================
// [AsmJit::Registers - 32-bit]
// ============================================================================

const GpReg eax(_Initialize(), kX86RegEax);
const GpReg ecx(_Initialize(), kX86RegEcx);
const GpReg edx(_Initialize(), kX86RegEdx);
const GpReg ebx(_Initialize(), kX86RegEbx);
const GpReg esp(_Initialize(), kX86RegEsp);
const GpReg ebp(_Initialize(), kX86RegEbp);
const GpReg esi(_Initialize(), kX86RegEsi);
const GpReg edi(_Initialize(), kX86RegEdi);

#if defined(ASMJIT_X64)
const GpReg r8d(_Initialize(), kX86RegR8d);
const GpReg r9d(_Initialize(), kX86RegR9d);
const GpReg r10d(_Initialize(), kX86RegR10d);
const GpReg r11d(_Initialize(), kX86RegR11d);
const GpReg r12d(_Initialize(), kX86RegR12d);
const GpReg r13d(_Initialize(), kX86RegR13d);
const GpReg r14d(_Initialize(), kX86RegR14d);
const GpReg r15d(_Initialize(), kX86RegR15d);
#endif // ASMJIT_X64

// ============================================================================
// [AsmJit::Registers - 64-bit]
// ============================================================================

#if defined(ASMJIT_X64)
const GpReg rax(_Initialize(), kX86RegRax);
const GpReg rcx(_Initialize(), kX86RegRcx);
const GpReg rdx(_Initialize(), kX86RegRdx);
const GpReg rbx(_Initialize(), kX86RegRbx);
const GpReg rsp(_Initialize(), kX86RegRsp);
const GpReg rbp(_Initialize(), kX86RegRbp);
const GpReg rsi(_Initialize(), kX86RegRsi);
const GpReg rdi(_Initialize(), kX86RegRdi);

const GpReg r8(_Initialize(), kX86RegR8);
const GpReg r9(_Initialize(), kX86RegR9);
const GpReg r10(_Initialize(), kX86RegR10);
const GpReg r11(_Initialize(), kX86RegR11);
const GpReg r12(_Initialize(), kX86RegR12);
const GpReg r13(_Initialize(), kX86RegR13);
const GpReg r14(_Initialize(), kX86RegR14);
const GpReg r15(_Initialize(), kX86RegR15);
#endif // ASMJIT_X64

// ============================================================================
// [AsmJit::Registers - Native (AsmJit extension)]
// ============================================================================

const GpReg zax(_Initialize(), kX86RegZax);
const GpReg zcx(_Initialize(), kX86RegZcx);
const GpReg zdx(_Initialize(), kX86RegZdx);
const GpReg zbx(_Initialize(), kX86RegZbx);
const GpReg zsp(_Initialize(), kX86RegZsp);
const GpReg zbp(_Initialize(), kX86RegZbp);
const GpReg zsi(_Initialize(), kX86RegZsi);
const GpReg zdi(_Initialize(), kX86RegZdi);

// ============================================================================
// [AsmJit::Registers - MM]
// ============================================================================

const MmReg mm0(_Initialize(), kX86RegMm0);
const MmReg mm1(_Initialize(), kX86RegMm1);
const MmReg mm2(_Initialize(), kX86RegMm2);
const MmReg mm3(_Initialize(), kX86RegMm3);
const MmReg mm4(_Initialize(), kX86RegMm4);
const MmReg mm5(_Initialize(), kX86RegMm5);
const MmReg mm6(_Initialize(), kX86RegMm6);
const MmReg mm7(_Initialize(), kX86RegMm7);

// ============================================================================
// [AsmJit::Registers - XMM]
// ============================================================================

const XmmReg xmm0(_Initialize(), kX86RegXmm0);
const XmmReg xmm1(_Initialize(), kX86RegXmm1);
const XmmReg xmm2(_Initialize(), kX86RegXmm2);
const XmmReg xmm3(_Initialize(), kX86RegXmm3);
const XmmReg xmm4(_Initialize(), kX86RegXmm4);
const XmmReg xmm5(_Initialize(), kX86RegXmm5);
const XmmReg xmm6(_Initialize(), kX86RegXmm6);
const XmmReg xmm7(_Initialize(), kX86RegXmm7);

#if defined(ASMJIT_X64)
const XmmReg xmm8(_Initialize(), kX86RegXmm8);
const XmmReg xmm9(_Initialize(), kX86RegXmm9);
const XmmReg xmm10(_Initialize(), kX86RegXmm10);
const XmmReg xmm11(_Initialize(), kX86RegXmm11);
const XmmReg xmm12(_Initialize(), kX86RegXmm12);
const XmmReg xmm13(_Initialize(), kX86RegXmm13);
const XmmReg xmm14(_Initialize(), kX86RegXmm14);
const XmmReg xmm15(_Initialize(), kX86RegXmm15);
#endif // ASMJIT_X64

// ============================================================================
// [AsmJit::Registers - Segment]
// ============================================================================

const SegmentReg cs(_Initialize(), kX86RegCs);
const SegmentReg ss(_Initialize(), kX86RegSs);
const SegmentReg ds(_Initialize(), kX86RegDs);
const SegmentReg es(_Initialize(), kX86RegEs);
const SegmentReg fs(_Initialize(), kX86RegFs);
const SegmentReg gs(_Initialize(), kX86RegGs);

// ============================================================================
// [AsmJit::Var]
// ============================================================================

Mem _BaseVarMem(const Var& var, uint32_t size)
{
  Mem m; //(_DontInitialize());

  m._mem.op = kOperandMem;
  m._mem.size = static_cast<uint8_t>(size == kInvalidValue ? var.getSize() : size);
  m._mem.type = kOperandMemNative;
  m._mem.segment = kX86SegNone;
  m._mem.sizePrefix = 0;
  m._mem.shift = 0;

  m._mem.id = var.getId();
  m._mem.base = kInvalidValue;
  m._mem.index = kInvalidValue;

  m._mem.target = NULL;
  m._mem.displacement = 0;

  return m;
}


Mem _BaseVarMem(const Var& var, uint32_t size, sysint_t disp)
{
  Mem m; //(_DontInitialize());

  m._mem.op = kOperandMem;
  m._mem.size = static_cast<uint8_t>(size == kInvalidValue ? var.getSize() : size);
  m._mem.type = kOperandMemNative;
  m._mem.segment = kX86SegNone;
  m._mem.sizePrefix = 0;
  m._mem.shift = 0;

  m._mem.id = var.getId();

  m._mem.base = kInvalidValue;
  m._mem.index = kInvalidValue;

  m._mem.target = NULL;
  m._mem.displacement = disp;

  return m;
}

Mem _BaseVarMem(const Var& var, uint32_t size, const GpVar& index, uint32_t shift, sysint_t disp)
{
  Mem m; //(_DontInitialize());

  m._mem.op = kOperandMem;
  m._mem.size = static_cast<uint8_t>(size == kInvalidValue ? var.getSize() : size);
  m._mem.type = kOperandMemNative;
  m._mem.segment = kX86SegNone;
  m._mem.sizePrefix = 0;
  m._mem.shift = shift;

  m._mem.id = var.getId();

  m._mem.base = kInvalidValue;
  m._mem.index = index.getId();

  m._mem.target = NULL;
  m._mem.displacement = disp;

  return m;
}

// ============================================================================
// [AsmJit::Mem - ptr[]]
// ============================================================================

Mem ptr(const Label& label, sysint_t disp, uint32_t size)
{
  return Mem(label, disp, size);
}

Mem ptr(const Label& label, const GpReg& index, uint32_t shift, sysint_t disp, uint32_t size)
{
  Mem m(label, disp, size);

  m._mem.index = index.getRegIndex();
  m._mem.shift = shift;

  return m;
}

Mem ptr(const Label& label, const GpVar& index, uint32_t shift, sysint_t disp, uint32_t size)
{
  Mem m(label, disp, size);

  m._mem.index = index.getId();
  m._mem.shift = shift;

  return m;
}

// ============================================================================
// [AsmJit::Mem - ptr[] - Absolute Addressing]
// ============================================================================

ASMJIT_API Mem ptr_abs(void* target, sysint_t disp, uint32_t size)
{
  Mem m;

  m._mem.size = size;
  m._mem.type = kOperandMemAbsolute;
  m._mem.segment = kX86SegNone;

  m._mem.target = target;
  m._mem.displacement = disp;

  return m;
}

ASMJIT_API Mem ptr_abs(void* target, const GpReg& index, uint32_t shift, sysint_t disp, uint32_t size)
{
  Mem m;// (_DontInitialize());

  m._mem.op = kOperandMem;
  m._mem.size = size;
  m._mem.type = kOperandMemAbsolute;
  m._mem.segment = kX86SegNone;

#if defined(ASMJIT_X86)
  m._mem.sizePrefix = index.getSize() != 4;
#else
  m._mem.sizePrefix = index.getSize() != 8;
#endif

  m._mem.shift = shift;

  m._mem.id = kInvalidValue;
  m._mem.base = kInvalidValue;
  m._mem.index = index.getRegIndex();

  m._mem.target = target;
  m._mem.displacement = disp;

  return m;
}

ASMJIT_API Mem ptr_abs(void* target, const GpVar& index, uint32_t shift, sysint_t disp, uint32_t size)
{
  Mem m;// (_DontInitialize());

  m._mem.op = kOperandMem;
  m._mem.size = size;
  m._mem.type = kOperandMemAbsolute;
  m._mem.segment = kX86SegNone;

#if defined(ASMJIT_X86)
  m._mem.sizePrefix = index.getSize() != 4;
#else
  m._mem.sizePrefix = index.getSize() != 8;
#endif

  m._mem.shift = shift;

  m._mem.id = kInvalidValue;
  m._mem.base = kInvalidValue;
  m._mem.index = index.getId();

  m._mem.target = target;
  m._mem.displacement = disp;

  return m;
}

// ============================================================================
// [AsmJit::Mem - ptr[base + displacement]]
// ============================================================================

Mem ptr(const GpReg& base, sysint_t disp, uint32_t size)
{
  return Mem(base, disp, size);
}

Mem ptr(const GpReg& base, const GpReg& index, uint32_t shift, sysint_t disp, uint32_t size)
{
  return Mem(base, index, shift, disp, size);
}

Mem ptr(const GpVar& base, sysint_t disp, uint32_t size)
{
  return Mem(base, disp, size);
}

Mem ptr(const GpVar& base, const GpVar& index, uint32_t shift, sysint_t disp, uint32_t size)
{
  return Mem(base, index, shift, disp, size);
}

} // AsmJit namespace

// [Api-End]
#include "../core/apiend.h"
