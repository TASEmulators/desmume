// [AsmJit]
// Complete JIT Assembler for C++ Language.
//
// [License]
// Zlib - See COPYING file in this package.

// [Guard]
#ifndef _ASMJIT_X86_X86OPERAND_H
#define _ASMJIT_X86_X86OPERAND_H

// [Dependencies - AsmJit]
#include "../core/defs.h"
#include "../core/operand.h"

#include "../x86/x86defs.h"

namespace AsmJit {

// ============================================================================
// [Forward Declarations]
// ============================================================================

struct GpReg;
struct GpVar;
struct Mem;
struct MmReg;
struct MmVar;
struct Var;
struct X87Reg;
struct X87Var;
struct XmmReg;
struct XmmVar;

struct SegmentReg;

//! @addtogroup AsmJit_X86
//! @{

// ============================================================================
// [AsmJit::MmData]
// ============================================================================

//! @brief Structure used for MMX specific data (64-bit).
//!
//! This structure can be used to load / store data from / to MMX register.
union MmData
{
  // --------------------------------------------------------------------------
  // [Methods]
  // --------------------------------------------------------------------------

  //! @brief Set all eight signed 8-bit integers.
  inline void setSB(
    int8_t x0, int8_t x1, int8_t x2, int8_t x3, int8_t x4, int8_t x5, int8_t x6, int8_t x7)
  {
    sb[0] = x0; sb[1] = x1; sb[2] = x2; sb[3] = x3; sb[4] = x4; sb[5] = x5; sb[6] = x6; sb[7] = x7;
  }

  //! @brief Set all eight unsigned 8-bit integers.
  inline void setUB(
    uint8_t x0, uint8_t x1, uint8_t x2, uint8_t x3, uint8_t x4, uint8_t x5, uint8_t x6, uint8_t x7)
  {
    ub[0] = x0; ub[1] = x1; ub[2] = x2; ub[3] = x3; ub[4] = x4; ub[5] = x5; ub[6] = x6; ub[7] = x7;
  }

  //! @brief Set all four signed 16-bit integers.
  inline void setSW(
    int16_t x0, int16_t x1, int16_t x2, int16_t x3)
  {
    sw[0] = x0; sw[1] = x1; sw[2] = x2; sw[3] = x3;
  }

  //! @brief Set all four unsigned 16-bit integers.
  inline void setUW(
    uint16_t x0, uint16_t x1, uint16_t x2, uint16_t x3)
  {
    uw[0] = x0; uw[1] = x1; uw[2] = x2; uw[3] = x3;
  }

  //! @brief Set all two signed 32-bit integers.
  inline void setSD(
    int32_t x0, int32_t x1)
  {
    sd[0] = x0; sd[1] = x1;
  }

  //! @brief Set all two unsigned 32-bit integers.
  inline void setUD(
    uint32_t x0, uint32_t x1)
  {
    ud[0] = x0; ud[1] = x1;
  }

  //! @brief Set signed 64-bit integer.
  inline void setSQ(
    int64_t x0)
  {
    sq[0] = x0;
  }

  //! @brief Set unsigned 64-bit integer.
  inline void setUQ(
    uint64_t x0)
  {
    uq[0] = x0;
  }

  //! @brief Set all two SP-FP values.
  inline void setSF(
    float x0, float x1)
  {
    sf[0] = x0; sf[1] = x1;
  }

  // --------------------------------------------------------------------------
  // [Members]
  // --------------------------------------------------------------------------

  //! @brief Array of eight signed 8-bit integers.
  int8_t sb[8];
  //! @brief Array of eight unsigned 8-bit integers.
  uint8_t ub[8];
  //! @brief Array of four signed 16-bit integers.
  int16_t sw[4];
  //! @brief Array of four unsigned 16-bit integers.
  uint16_t uw[4];
  //! @brief Array of two signed 32-bit integers.
  int32_t sd[2];
  //! @brief Array of two unsigned 32-bit integers.
  uint32_t ud[2];
  //! @brief Array of one signed 64-bit integer.
  int64_t sq[1];
  //! @brief Array of one unsigned 64-bit integer.
  uint64_t uq[1];

  //! @brief Array of two SP-FP values.
  float sf[2];
};

// ============================================================================
// [AsmJit::XmmData]
// ============================================================================

//! @brief Structure used for SSE specific data (128-bit).
//!
//! This structure can be used to load / store data from / to SSE register.
//!
//! @note Always align SSE data to 16-bytes.
union XmmData
{
  // --------------------------------------------------------------------------
  // [Methods]
  // --------------------------------------------------------------------------

  //! @brief Set all sixteen signed 8-bit integers.
  inline void setSB(
    int8_t x0, int8_t x1, int8_t x2 , int8_t x3 , int8_t x4 , int8_t x5 , int8_t x6 , int8_t x7 ,
    int8_t x8, int8_t x9, int8_t x10, int8_t x11, int8_t x12, int8_t x13, int8_t x14, int8_t x15)
  {
    sb[0] = x0; sb[1] = x1; sb[ 2] = x2 ; sb[3 ] = x3 ; sb[4 ] = x4 ; sb[5 ] = x5 ; sb[6 ] = x6 ; sb[7 ] = x7 ;
    sb[8] = x8; sb[9] = x9; sb[10] = x10; sb[11] = x11; sb[12] = x12; sb[13] = x13; sb[14] = x14; sb[15] = x15; 
  }

  //! @brief Set all sixteen unsigned 8-bit integers.
  inline void setUB(
    uint8_t x0, uint8_t x1, uint8_t x2 , uint8_t x3 , uint8_t x4 , uint8_t x5 , uint8_t x6 , uint8_t x7 ,
    uint8_t x8, uint8_t x9, uint8_t x10, uint8_t x11, uint8_t x12, uint8_t x13, uint8_t x14, uint8_t x15)
  {
    ub[0] = x0; ub[1] = x1; ub[ 2] = x2 ; ub[3 ] = x3 ; ub[4 ] = x4 ; ub[5 ] = x5 ; ub[6 ] = x6 ; ub[7 ] = x7 ;
    ub[8] = x8; ub[9] = x9; ub[10] = x10; ub[11] = x11; ub[12] = x12; ub[13] = x13; ub[14] = x14; ub[15] = x15; 
  }

  //! @brief Set all eight signed 16-bit integers.
  inline void setSW(
    int16_t x0, int16_t x1, int16_t x2, int16_t x3, int16_t x4, int16_t x5, int16_t x6, int16_t x7)
  {
    sw[0] = x0; sw[1] = x1; sw[2] = x2; sw[3] = x3; sw[4] = x4; sw[5] = x5; sw[6] = x6; sw[7] = x7;
  }

  //! @brief Set all eight unsigned 16-bit integers.
  inline void setUW(
    uint16_t x0, uint16_t x1, uint16_t x2, uint16_t x3, uint16_t x4, uint16_t x5, uint16_t x6, uint16_t x7)
  {
    uw[0] = x0; uw[1] = x1; uw[2] = x2; uw[3] = x3; uw[4] = x4; uw[5] = x5; uw[6] = x6; uw[7] = x7;
  }

  //! @brief Set all four signed 32-bit integers.
  inline void setSD(
    int32_t x0, int32_t x1, int32_t x2, int32_t x3)
  {
    sd[0] = x0; sd[1] = x1; sd[2] = x2; sd[3] = x3;
  }

  //! @brief Set all four unsigned 32-bit integers.
  inline void setUD(
    uint32_t x0, uint32_t x1, uint32_t x2, uint32_t x3)
  {
    ud[0] = x0; ud[1] = x1; ud[2] = x2; ud[3] = x3;
  }

  //! @brief Set all two signed 64-bit integers.
  inline void setSQ(
    int64_t x0, int64_t x1)
  {
    sq[0] = x0; sq[1] = x1;
  }

  //! @brief Set all two unsigned 64-bit integers.
  inline void setUQ(
    uint64_t x0, uint64_t x1)
  {
    uq[0] = x0; uq[1] = x1;
  }

  //! @brief Set all four SP-FP floats.
  inline void setSF(
    float x0, float x1, float x2, float x3)
  {
    sf[0] = x0; sf[1] = x1; sf[2] = x2; sf[3] = x3;
  }

  //! @brief Set all two DP-FP floats.
  inline void setDF(
    double x0, double x1)
  {
    df[0] = x0; df[1] = x1;
  }

  // --------------------------------------------------------------------------
  // [Members]
  // --------------------------------------------------------------------------

  //! @brief Array of sixteen signed 8-bit integers.
  int8_t sb[16];
  //! @brief Array of sixteen unsigned 8-bit integers.
  uint8_t ub[16];
  //! @brief Array of eight signed 16-bit integers.
  int16_t sw[8];
  //! @brief Array of eight unsigned 16-bit integers.
  uint16_t uw[8];
  //! @brief Array of four signed 32-bit integers.
  int32_t sd[4];
  //! @brief Array of four unsigned 32-bit integers.
  uint32_t ud[4];
  //! @brief Array of two signed 64-bit integers.
  int64_t sq[2];
  //! @brief Array of two unsigned 64-bit integers.
  uint64_t uq[2];

  //! @brief Array of four 32-bit single precision floating points.
  float sf[4];
  //! @brief Array of two 64-bit double precision floating points.
  double df[2];
};

// ============================================================================
// [AsmJit::GpReg]
// ============================================================================

//! @brief General purpose register.
//!
//! This class is for all general purpose registers (64, 32, 16 and 8-bit).
struct GpReg : public Reg
{
  // --------------------------------------------------------------------------
  // [Construction / Destruction]
  // --------------------------------------------------------------------------

  //! @brief Create non-initialized general purpose register.
  inline GpReg() : Reg(kInvalidValue, 0) {}
  //! @brief Create a reference to @a other general purpose register.
  inline GpReg(const GpReg& other) : Reg(other) {}

#if !defined(ASMJIT_NODOC)
  inline GpReg(const _DontInitialize& dontInitialize) : Reg(dontInitialize) {}
  inline GpReg(const _Initialize&, uint32_t code) : Reg(code, static_cast<uint32_t>(1U << ((code & kRegTypeMask) >> 12))) {}
#endif // ASMJIT_NODOC

  // --------------------------------------------------------------------------
  // [Reg Specific]
  // --------------------------------------------------------------------------

  //! @brief Set register code to @a code.
  inline GpReg& setCode(uint32_t code)
  {
    _reg.code = code;
    return *this;
  }

  //! @brief Set register size to @a size.
  inline GpReg& setSize(uint32_t size)
  {
    _reg.size = static_cast<uint8_t>(size);
    return *this;
  }

  // --------------------------------------------------------------------------
  // [GpReg Specific]
  // --------------------------------------------------------------------------

  //! @brief Get whether the general purpose register is BYTE (8-bit) type.
  inline bool isGpb() const { return (_reg.code & kRegTypeMask) <= kX86RegTypeGpbHi; }
  //! @brief Get whether the general purpose register is LO-BYTE (8-bit) type.
  inline bool isGpbLo() const { return (_reg.code & kRegTypeMask) == kX86RegTypeGpbLo; }
  //! @brief Get whether the general purpose register is HI-BYTE (8-bit) type.
  inline bool isGpbHi() const { return (_reg.code & kRegTypeMask) == kX86RegTypeGpbHi; }

  //! @brief Get whether the general purpose register is WORD (16-bit) type.
  inline bool isGpw() const { return (_reg.code & kRegTypeMask) == kX86RegTypeGpw; }
  //! @brief Get whether the general purpose register is DWORD (32-bit) type.
  //!
  //! This is default type for 32-bit platforms.
  inline bool isGpd() const { return (_reg.code & kRegTypeMask) == kX86RegTypeGpd; }
  //! @brief Get whether the general purpose register is QWORD (64-bit) type.
  //!
  //! This is default type for 64-bit platforms.
  inline bool isGpq() const { return (_reg.code & kRegTypeMask) == kX86RegTypeGpq; }

  // --------------------------------------------------------------------------
  // [Operator Overload]
  // --------------------------------------------------------------------------

#if !defined(ASMJIT_NODOC)
  inline GpReg& operator=(const GpReg& other) { _copy(other); return *this; }
  inline bool operator==(const GpReg& other) const { return getRegCode() == other.getRegCode(); }
  inline bool operator!=(const GpReg& other) const { return getRegCode() != other.getRegCode(); }
#endif // ASMJIT_NODOC
};

// ============================================================================
// [AsmJit::X87Reg]
// ============================================================================

//! @brief 80-bit x87 floating point register.
//!
//! To create instance of x87 register, use @c st() function.
struct X87Reg : public Reg
{
  // --------------------------------------------------------------------------
  // [Construction / Destruction]
  // --------------------------------------------------------------------------

  //! @brief Create non-initialized x87 register.
  inline X87Reg() : Reg(kInvalidValue, 10) {}
  //! @brief Create a reference to @a other x87 register.
  inline X87Reg(const X87Reg& other) : Reg(other) {}

#if !defined(ASMJIT_NODOC)
  inline X87Reg(const _DontInitialize& dontInitialize) : Reg(dontInitialize) {}
  inline X87Reg(const _Initialize&, uint32_t code) : Reg(code | kX86RegTypeX87, 10) {}
#endif // ASMJIT_NODOC

  // --------------------------------------------------------------------------
  // [Reg Specific]
  // --------------------------------------------------------------------------

  //! @brief Set register code to @a code.
  inline X87Reg& setCode(uint32_t code)
  {
    _reg.code = code;
    return *this;
  }

  //! @brief Set register size to @a size.
  inline X87Reg& setSize(uint32_t size)
  {
    _reg.size = static_cast<uint8_t>(size);
    return *this;
  }

  // --------------------------------------------------------------------------
  // [Operator Overload]
  // --------------------------------------------------------------------------

#if !defined(ASMJIT_NODOC)
  inline X87Reg& operator=(const X87Reg& other) { _copy(other); return *this; }
  inline bool operator==(const X87Reg& other) const { return getRegCode() == other.getRegCode(); }
  inline bool operator!=(const X87Reg& other) const { return getRegCode() != other.getRegCode(); }
#endif // ASMJIT_NODOC
};

// ============================================================================
// [AsmJit::MmReg]
// ============================================================================

//! @brief 64-bit MMX register.
struct MmReg : public Reg
{
  // --------------------------------------------------------------------------
  // [Construction / Destruction]
  // --------------------------------------------------------------------------

  //! @brief Create non-initialized MM register.
  inline MmReg() : Reg(kInvalidValue, 8) {}
  //! @brief Create a reference to @a other MM register.
  inline MmReg(const MmReg& other) : Reg(other) {}

#if !defined(ASMJIT_NODOC)
  inline MmReg(const _DontInitialize& dontInitialize) : Reg(dontInitialize) {}
  inline MmReg(const _Initialize&, uint32_t code) : Reg(code, 8) {}
#endif // ASMJIT_NODOC

  // --------------------------------------------------------------------------
  // [Reg Specific]
  // --------------------------------------------------------------------------

  //! @brief Set register code to @a code.
  inline MmReg& setCode(uint32_t code)
  {
    _reg.code = code;
    return *this;
  }

  //! @brief Set register size to @a size.
  inline MmReg& setSize(uint32_t size)
  {
    _reg.size = static_cast<uint8_t>(size);
    return *this;
  }

  // --------------------------------------------------------------------------
  // [Operator Overload]
  // --------------------------------------------------------------------------

#if !defined(ASMJIT_NODOC)
  inline MmReg& operator=(const MmReg& other) { _copy(other); return *this; }
  inline bool operator==(const MmReg& other) const { return getRegCode() == other.getRegCode(); }
  inline bool operator!=(const MmReg& other) const { return getRegCode() != other.getRegCode(); }
#endif // ASMJIT_NODOC
};

// ============================================================================
// [AsmJit::XmmReg]
// ============================================================================

//! @brief 128-bit SSE register.
struct XmmReg : public Reg
{
  // --------------------------------------------------------------------------
  // [Construction / Destruction]
  // --------------------------------------------------------------------------

  //! @brief Create non-initialized XMM register.
  inline XmmReg() : Reg(kInvalidValue, 16) {}
  //! @brief Create a reference to @a other XMM register.
  inline XmmReg(const _Initialize&, uint32_t code) : Reg(code, 16) {}

#if !defined(ASMJIT_NODOC)
  inline XmmReg(const _DontInitialize& dontInitialize) : Reg(dontInitialize) {}
  inline XmmReg(const XmmReg& other) : Reg(other) {}
#endif // ASMJIT_NODOC

  // --------------------------------------------------------------------------
  // [Reg Specific]
  // --------------------------------------------------------------------------

  //! @brief Set register code to @a code.
  inline XmmReg& setCode(uint32_t code)
  {
    _reg.code = code;
    return *this;
  }

  //! @brief Set register size to @a size.
  inline XmmReg& setSize(uint32_t size)
  {
    _reg.size = static_cast<uint8_t>(size);
    return *this;
  }

  // --------------------------------------------------------------------------
  // [Operator Overload]
  // --------------------------------------------------------------------------

#if !defined(ASMJIT_NODOC)
  inline XmmReg& operator=(const XmmReg& other) { _copy(other); return *this; }
  inline bool operator==(const XmmReg& other) const { return getRegCode() == other.getRegCode(); }
  inline bool operator!=(const XmmReg& other) const { return getRegCode() != other.getRegCode(); }
#endif // ASMJIT_NODOC
};

// ============================================================================
// [AsmJit::SegmentReg]
// ============================================================================

//! @brief Segment register.
struct SegmentReg : public Reg
{
  // --------------------------------------------------------------------------
  // [Construction / Destruction]
  // --------------------------------------------------------------------------

  //! @brief Create non-initialized segment register.
  inline SegmentReg() : Reg(kInvalidValue, 2) {}
  //! @brief Create a reference to @a other segment register.
  inline SegmentReg(const _Initialize&, uint32_t code) : Reg(code, 2) {}

#if !defined(ASMJIT_NODOC)
  inline SegmentReg(const _DontInitialize& dontInitialize) : Reg(dontInitialize) {}
  inline SegmentReg(const SegmentReg& other) : Reg(other) {}
#endif // ASMJIT_NODOC

  // --------------------------------------------------------------------------
  // [Reg Specific]
  // --------------------------------------------------------------------------

  //! @brief Set register code to @a code.
  inline SegmentReg& setCode(uint32_t code)
  {
    _reg.code = code;
    return *this;
  }

  //! @brief Set register size to @a size.
  inline SegmentReg& setSize(uint32_t size)
  {
    _reg.size = static_cast<uint8_t>(size);
    return *this;
  }

  // --------------------------------------------------------------------------
  // [Operator Overload]
  // --------------------------------------------------------------------------

#if !defined(ASMJIT_NODOC)
  inline SegmentReg& operator=(const SegmentReg& other) { _copy(other); return *this; }
  inline bool operator==(const SegmentReg& other) const { return getRegCode() == other.getRegCode(); }
  inline bool operator!=(const SegmentReg& other) const { return getRegCode() != other.getRegCode(); }
#endif // ASMJIT_NODOC
};

// ============================================================================
// [AsmJit::Registers - no_reg]
// ============================================================================

//! @brief No register, can be used only in @c Mem operand.
ASMJIT_VAR const GpReg no_reg;

// ============================================================================
// [AsmJit::Registers - 8-bit]
// ============================================================================

//! @brief 8-bit General purpose register.
ASMJIT_VAR const GpReg al;
//! @brief 8-bit General purpose register.
ASMJIT_VAR const GpReg cl;
//! @brief 8-bit General purpose register.
ASMJIT_VAR const GpReg dl;
//! @brief 8-bit General purpose register.
ASMJIT_VAR const GpReg bl;

#if defined(ASMJIT_X64)
//! @brief 8-bit General purpose register (64-bit mode only).
ASMJIT_VAR const GpReg spl;
//! @brief 8-bit General purpose register (64-bit mode only).
ASMJIT_VAR const GpReg bpl;
//! @brief 8-bit General purpose register (64-bit mode only).
ASMJIT_VAR const GpReg sil;
//! @brief 8-bit General purpose register (64-bit mode only).
ASMJIT_VAR const GpReg dil;

//! @brief 8-bit General purpose register (64-bit mode only).
ASMJIT_VAR const GpReg r8b;
//! @brief 8-bit General purpose register (64-bit mode only).
ASMJIT_VAR const GpReg r9b;
//! @brief 8-bit General purpose register (64-bit mode only).
ASMJIT_VAR const GpReg r10b;
//! @brief 8-bit General purpose register (64-bit mode only).
ASMJIT_VAR const GpReg r11b;
//! @brief 8-bit General purpose register (64-bit mode only).
ASMJIT_VAR const GpReg r12b;
//! @brief 8-bit General purpose register (64-bit mode only).
ASMJIT_VAR const GpReg r13b;
//! @brief 8-bit General purpose register (64-bit mode only).
ASMJIT_VAR const GpReg r14b;
//! @brief 8-bit General purpose register (64-bit mode only).
ASMJIT_VAR const GpReg r15b;
#endif // ASMJIT_X64

//! @brief 8-bit General purpose register.
ASMJIT_VAR const GpReg ah;
//! @brief 8-bit General purpose register.
ASMJIT_VAR const GpReg ch;
//! @brief 8-bit General purpose register.
ASMJIT_VAR const GpReg dh;
//! @brief 8-bit General purpose register.
ASMJIT_VAR const GpReg bh;

// ============================================================================
// [AsmJit::Registers - 16-bit]
// ============================================================================

//! @brief 16-bit General purpose register.
ASMJIT_VAR const GpReg ax;
//! @brief 16-bit General purpose register.
ASMJIT_VAR const GpReg cx;
//! @brief 16-bit General purpose register.
ASMJIT_VAR const GpReg dx;
//! @brief 16-bit General purpose register.
ASMJIT_VAR const GpReg bx;
//! @brief 16-bit General purpose register.
ASMJIT_VAR const GpReg sp;
//! @brief 16-bit General purpose register.
ASMJIT_VAR const GpReg bp;
//! @brief 16-bit General purpose register.
ASMJIT_VAR const GpReg si;
//! @brief 16-bit General purpose register.
ASMJIT_VAR const GpReg di;

#if defined(ASMJIT_X64)
//! @brief 16-bit General purpose register (64-bit mode only).
ASMJIT_VAR const GpReg r8w;
//! @brief 16-bit General purpose register (64-bit mode only).
ASMJIT_VAR const GpReg r9w;
//! @brief 16-bit General purpose register (64-bit mode only).
ASMJIT_VAR const GpReg r10w;
//! @brief 16-bit General purpose register (64-bit mode only).
ASMJIT_VAR const GpReg r11w;
//! @brief 16-bit General purpose register (64-bit mode only).
ASMJIT_VAR const GpReg r12w;
//! @brief 16-bit General purpose register (64-bit mode only).
ASMJIT_VAR const GpReg r13w;
//! @brief 16-bit General purpose register (64-bit mode only).
ASMJIT_VAR const GpReg r14w;
//! @brief 16-bit General purpose register (64-bit mode only).
ASMJIT_VAR const GpReg r15w;
#endif // ASMJIT_X64

// ============================================================================
// [AsmJit::Registers - 32-bit]
// ============================================================================

//! @brief 32-bit General purpose register.
ASMJIT_VAR const GpReg eax;
//! @brief 32-bit General purpose register.
ASMJIT_VAR const GpReg ecx;
//! @brief 32-bit General purpose register.
ASMJIT_VAR const GpReg edx;
//! @brief 32-bit General purpose register.
ASMJIT_VAR const GpReg ebx;
//! @brief 32-bit General purpose register.
ASMJIT_VAR const GpReg esp;
//! @brief 32-bit General purpose register.
ASMJIT_VAR const GpReg ebp;
//! @brief 32-bit General purpose register.
ASMJIT_VAR const GpReg esi;
//! @brief 32-bit General purpose register.
ASMJIT_VAR const GpReg edi;

#if defined(ASMJIT_X64)
//! @brief 32-bit General purpose register.
ASMJIT_VAR const GpReg r8d;
//! @brief 32-bit General purpose register.
ASMJIT_VAR const GpReg r9d;
//! @brief 32-bit General purpose register.
ASMJIT_VAR const GpReg r10d;
//! @brief 32-bit General purpose register.
ASMJIT_VAR const GpReg r11d;
//! @brief 32-bit General purpose register.
ASMJIT_VAR const GpReg r12d;
//! @brief 32-bit General purpose register.
ASMJIT_VAR const GpReg r13d;
//! @brief 32-bit General purpose register.
ASMJIT_VAR const GpReg r14d;
//! @brief 32-bit General purpose register.
ASMJIT_VAR const GpReg r15d;
#endif // ASMJIT_X64

// ============================================================================
// [AsmJit::Registers - 64-bit]
// ============================================================================

#if defined(ASMJIT_X64)
//! @brief 64-bit General purpose register (64-bit mode only).
ASMJIT_VAR const GpReg rax;
//! @brief 64-bit General purpose register (64-bit mode only).
ASMJIT_VAR const GpReg rcx;
//! @brief 64-bit General purpose register (64-bit mode only).
ASMJIT_VAR const GpReg rdx;
//! @brief 64-bit General purpose register (64-bit mode only).
ASMJIT_VAR const GpReg rbx;
//! @brief 64-bit General purpose register (64-bit mode only).
ASMJIT_VAR const GpReg rsp;
//! @brief 64-bit General purpose register (64-bit mode only).
ASMJIT_VAR const GpReg rbp;
//! @brief 64-bit General purpose register (64-bit mode only).
ASMJIT_VAR const GpReg rsi;
//! @brief 64-bit General purpose register (64-bit mode only).
ASMJIT_VAR const GpReg rdi;

//! @brief 64-bit General purpose register (64-bit mode only).
ASMJIT_VAR const GpReg r8;
//! @brief 64-bit General purpose register (64-bit mode only).
ASMJIT_VAR const GpReg r9;
//! @brief 64-bit General purpose register (64-bit mode only).
ASMJIT_VAR const GpReg r10;
//! @brief 64-bit General purpose register (64-bit mode only).
ASMJIT_VAR const GpReg r11;
//! @brief 64-bit General purpose register (64-bit mode only).
ASMJIT_VAR const GpReg r12;
//! @brief 64-bit General purpose register (64-bit mode only).
ASMJIT_VAR const GpReg r13;
//! @brief 64-bit General purpose register (64-bit mode only).
ASMJIT_VAR const GpReg r14;
//! @brief 64-bit General purpose register (64-bit mode only).
ASMJIT_VAR const GpReg r15;
#endif // ASMJIT_X64

// ============================================================================
// [AsmJit::Registers - Native (AsmJit extension)]
// ============================================================================

//! @brief 32-bit or 64-bit General purpose register.
ASMJIT_VAR const GpReg zax;
//! @brief 32-bit or 64-bit General purpose register.
ASMJIT_VAR const GpReg zcx;
//! @brief 32-bit or 64-bit General purpose register.
ASMJIT_VAR const GpReg zdx;
//! @brief 32-bit or 64-bit General purpose register.
ASMJIT_VAR const GpReg zbx;
//! @brief 32-bit or 64-bit General purpose register.
ASMJIT_VAR const GpReg zsp;
//! @brief 32-bit or 64-bit General purpose register.
ASMJIT_VAR const GpReg zbp;
//! @brief 32-bit or 64-bit General purpose register.
ASMJIT_VAR const GpReg zsi;
//! @brief 32-bit or 64-bit General purpose register.
ASMJIT_VAR const GpReg zdi;

// ============================================================================
// [AsmJit::Registers - MM]
// ============================================================================

//! @brief 64-bit MM register.
ASMJIT_VAR const MmReg mm0;
//! @brief 64-bit MM register.
ASMJIT_VAR const MmReg mm1;
//! @brief 64-bit MM register.
ASMJIT_VAR const MmReg mm2;
//! @brief 64-bit MM register.
ASMJIT_VAR const MmReg mm3;
//! @brief 64-bit MM register.
ASMJIT_VAR const MmReg mm4;
//! @brief 64-bit MM register.
ASMJIT_VAR const MmReg mm5;
//! @brief 64-bit MM register.
ASMJIT_VAR const MmReg mm6;
//! @brief 64-bit MM register.
ASMJIT_VAR const MmReg mm7;

// ============================================================================
// [AsmJit::Registers - XMM]
// ============================================================================

//! @brief 128-bit XMM register.
ASMJIT_VAR const XmmReg xmm0;
//! @brief 128-bit XMM register.
ASMJIT_VAR const XmmReg xmm1;
//! @brief 128-bit XMM register.
ASMJIT_VAR const XmmReg xmm2;
//! @brief 128-bit XMM register.
ASMJIT_VAR const XmmReg xmm3;
//! @brief 128-bit XMM register.
ASMJIT_VAR const XmmReg xmm4;
//! @brief 128-bit XMM register.
ASMJIT_VAR const XmmReg xmm5;
//! @brief 128-bit XMM register.
ASMJIT_VAR const XmmReg xmm6;
//! @brief 128-bit XMM register.
ASMJIT_VAR const XmmReg xmm7;

#if defined(ASMJIT_X64)
//! @brief 128-bit XMM register (64-bit mode only).
ASMJIT_VAR const XmmReg xmm8;
//! @brief 128-bit XMM register (64-bit mode only).
ASMJIT_VAR const XmmReg xmm9;
//! @brief 128-bit XMM register (64-bit mode only).
ASMJIT_VAR const XmmReg xmm10;
//! @brief 128-bit XMM register (64-bit mode only).
ASMJIT_VAR const XmmReg xmm11;
//! @brief 128-bit XMM register (64-bit mode only).
ASMJIT_VAR const XmmReg xmm12;
//! @brief 128-bit XMM register (64-bit mode only).
ASMJIT_VAR const XmmReg xmm13;
//! @brief 128-bit XMM register (64-bit mode only).
ASMJIT_VAR const XmmReg xmm14;
//! @brief 128-bit XMM register (64-bit mode only).
ASMJIT_VAR const XmmReg xmm15;
#endif // ASMJIT_X64

// ============================================================================
// [AsmJit::Registers - Segment]
// ============================================================================

//! @brief CS segment register.
ASMJIT_VAR const SegmentReg cs;
//! @brief SS segment register.
ASMJIT_VAR const SegmentReg ss;
//! @brief DS segment register.
ASMJIT_VAR const SegmentReg ds;
//! @brief ES segment register.
ASMJIT_VAR const SegmentReg es;
//! @brief FS segment register.
ASMJIT_VAR const SegmentReg fs;
//! @brief GS segment register.
ASMJIT_VAR const SegmentReg gs;

// ============================================================================
// [AsmJit::Registers - Register From Index]
// ============================================================================

//! @brief Get general purpose register of byte size.
static inline GpReg gpb_lo(uint32_t index)
{ return GpReg(_Initialize(), static_cast<uint32_t>(index | kX86RegTypeGpbLo)); }

//! @brief Get general purpose register of byte size.
static inline GpReg gpb_hi(uint32_t index)
{ return GpReg(_Initialize(), static_cast<uint32_t>(index | kX86RegTypeGpbHi)); }

//! @brief Get general purpose register of word size.
static inline GpReg gpw(uint32_t index)
{ return GpReg(_Initialize(), static_cast<uint32_t>(index | kX86RegTypeGpw)); }

//! @brief Get general purpose register of dword size.
static inline GpReg gpd(uint32_t index)
{ return GpReg(_Initialize(), static_cast<uint32_t>(index | kX86RegTypeGpd)); }

#if defined(ASMJIT_X64)
//! @brief Get general purpose register of qword size (64-bit only).
static inline GpReg gpq(uint32_t index)
{ return GpReg(_Initialize(), static_cast<uint32_t>(index | kX86RegTypeGpq)); }
#endif

//! @brief Get general purpose dword/qword register (depending to architecture).
static inline GpReg gpz(uint32_t index)
{ return GpReg(_Initialize(), static_cast<uint32_t>(index | kX86RegTypeGpz)); }

//! @brief Get MMX (MM) register .
static inline MmReg mm(uint32_t index)
{ return MmReg(_Initialize(), static_cast<uint32_t>(index | kX86RegTypeMm)); }

//! @brief Get SSE (XMM) register.
static inline XmmReg xmm(uint32_t index)
{ return XmmReg(_Initialize(), static_cast<uint32_t>(index | kX86RegTypeXmm)); }

//! @brief Get x87 register with index @a i.
static inline X87Reg st(uint32_t i)
{
  ASMJIT_ASSERT(i < 8);
  return X87Reg(_Initialize(), static_cast<uint32_t>(i));
}

// ============================================================================
// [AsmJit::Mem]
// ============================================================================

//! @brief Memory operand.
struct Mem : public Operand
{
  // --------------------------------------------------------------------------
  // [Construction / Destruction]
  // --------------------------------------------------------------------------

  inline Mem() :
    Operand(_DontInitialize())
  {
    _mem.op = kOperandMem;
    _mem.size = 0;
    _mem.type = kOperandMemNative;
    _mem.segment = kX86SegNone;
    _mem.sizePrefix = 0;
    _mem.shift = 0;

    _mem.id = kInvalidValue;
    _mem.base = kInvalidValue;
    _mem.index = kInvalidValue;

    _mem.target = NULL;
    _mem.displacement = 0;
  }

  inline Mem(const Label& label, sysint_t displacement, uint32_t size = 0) :
    Operand(_DontInitialize())
  {
    _mem.op = kOperandMem;
    _mem.size = (uint8_t)size;
    _mem.type = kOperandMemLabel;
    _mem.segment = kX86SegNone;
    _mem.sizePrefix = 0;
    _mem.shift = 0;

    _mem.id = kInvalidValue;
    _mem.base = reinterpret_cast<const Operand&>(label)._base.id;
    _mem.index = kInvalidValue;

    _mem.target = NULL;
    _mem.displacement = displacement;
  }

  inline Mem(const GpReg& base, sysint_t displacement, uint32_t size = 0) :
    Operand(_DontInitialize())
  {
    _mem.op = kOperandMem;
    _mem.size = (uint8_t)size;
    _mem.type = kOperandMemNative;
    _mem.segment = kX86SegNone;

#if defined(ASMJIT_X86)
    _mem.sizePrefix = base.getSize() != 4;
#else
    _mem.sizePrefix = base.getSize() != 8;
#endif

    _mem.shift = 0;

    _mem.id = kInvalidValue;
    _mem.base = base.getRegCode() & kRegIndexMask;
    _mem.index = kInvalidValue;

    _mem.target = NULL;
    _mem.displacement = displacement;
  }

  inline Mem(const GpVar& base, sysint_t displacement, uint32_t size = 0) :
    Operand(_DontInitialize())
  {
    _mem.op = kOperandMem;
    _mem.size = (uint8_t)size;
    _mem.type = kOperandMemNative;
    _mem.segment = kX86SegNone;

#if defined(ASMJIT_X86)
    _mem.sizePrefix = (reinterpret_cast<const Operand&>(base)._var.size) != 4;
#else
    _mem.sizePrefix = (reinterpret_cast<const Operand&>(base)._var.size) != 8;
#endif

    _mem.shift = 0;

    _mem.id = kInvalidValue;
    _mem.base = reinterpret_cast<const Operand&>(base).getId();
    _mem.index = kInvalidValue;

    _mem.target = NULL;
    _mem.displacement = displacement;
  }

  inline Mem(const GpReg& base, const GpReg& index, uint32_t shift, sysint_t displacement, uint32_t size = 0) :
    Operand(_DontInitialize())
  {
    ASMJIT_ASSERT(shift <= 3);

    _mem.op = kOperandMem;
    _mem.size = (uint8_t)size;
    _mem.type = kOperandMemNative;
    _mem.segment = kX86SegNone;

#if defined(ASMJIT_X86)
    _mem.sizePrefix = (base.getSize() | index.getSize()) != 4;
#else
    _mem.sizePrefix = (base.getSize() | index.getSize()) != 8;
#endif

    _mem.shift = (uint8_t)shift;

    _mem.id = kInvalidValue;
    _mem.base = base.getRegIndex();
    _mem.index = index.getRegIndex();

    _mem.target = NULL;
    _mem.displacement = displacement;
  }

  inline Mem(const GpVar& base, const GpVar& index, uint32_t shift, sysint_t displacement, uint32_t size = 0) :
    Operand(_DontInitialize())
  {
    ASMJIT_ASSERT(shift <= 3);

    _mem.op = kOperandMem;
    _mem.size = (uint8_t)size;
    _mem.type = kOperandMemNative;
    _mem.segment = kX86SegNone;

#if defined(ASMJIT_X86)
    _mem.sizePrefix = (reinterpret_cast<const Operand&>(base )._var.size | 
                       reinterpret_cast<const Operand&>(index)._var.size ) != 4;
#else
    _mem.sizePrefix = (reinterpret_cast<const Operand&>(base )._var.size | 
                       reinterpret_cast<const Operand&>(index)._var.size ) != 8;
#endif

    _mem.shift = (uint8_t)shift;

    _mem.id = kInvalidValue;
    _mem.base = reinterpret_cast<const Operand&>(base).getId();
    _mem.index = reinterpret_cast<const Operand&>(index).getId();

    _mem.target = NULL;
    _mem.displacement = displacement;
  }

  inline Mem(const Mem& other) :
    Operand(other)
  {
  }

  inline Mem(const _DontInitialize& dontInitialize) :
    Operand(dontInitialize)
  {
  }

  // --------------------------------------------------------------------------
  // [Mem Specific]
  // --------------------------------------------------------------------------

  //! @brief Get type of memory operand, see @c kOperandMemType.
  inline uint32_t getMemType() const
  { return _mem.type; }

  //! @brief Get memory operand segment, see @c kX86Seg.
  inline uint32_t getSegment() const
  { return _mem.segment; }

  //! @brief Set memory operand segment, see @c kX86Seg.
  inline Mem& setSegment(uint32_t seg)
  {
    _mem.segment = static_cast<uint8_t>(seg);
    return *this;
  }

  //! @brief Set memory operand segment, see @c kX86Seg.
  inline Mem& setSegment(const SegmentReg& seg)
  {
    _mem.segment = static_cast<uint8_t>(seg.getRegIndex());
    return *this;
  }

  //! @brief Get whether the memory operand has segment override prefix.
  inline bool hasSegment() const
  { return _mem.segment >= kX86SegCount; }

  //! @brief Get whether the memory operand has base register.
  inline bool hasBase() const
  { return _mem.base != kInvalidValue; }

  //! @brief Get whether the memory operand has index.
  inline bool hasIndex() const
  { return _mem.index != kInvalidValue; }

  //! @brief Get whether the memory operand has shift used.
  inline bool hasShift() const
  { return _mem.shift != 0; }

  //! @brief Get memory operand base register or @c kInvalidValue.
  inline uint32_t getBase() const
  { return _mem.base; }

  //! @brief Get memory operand index register or @c kInvalidValue.
  inline uint32_t getIndex() const
  { return _mem.index; }

  //! @brief Get memory operand index scale (0, 1, 2 or 3).
  inline uint32_t getShift() const
  { return _mem.shift; }

  //! @brief Get whether to use size-override prefix.
  //!
  //! @note This is useful only for MOV and LEA type of instructions.
  inline bool getSizePrefix() const
  { return _mem.sizePrefix; }
  
  //! @brief Set whether to use size-override prefix.
  inline Mem& setSizePrefix(bool b)
  {
    _mem.sizePrefix = b;
    return *this;
  }

  //! @brief Get absolute target address.
  //!
  //! @note You should always check if operand contains address by @c getMemType().
  inline void* getTarget() const
  { return _mem.target; }

  //! @brief Set absolute target address.
  inline Mem& setTarget(void* target)
  {
    _mem.target = target;
    return *this;
  }

  //! @brief Set memory operand size.
  inline Mem& setSize(uint32_t size)
  {
    _mem.size = size;
    return *this;
  }

  //! @brief Get memory operand relative displacement.
  inline sysint_t getDisplacement() const
  { return _mem.displacement; }

  //! @brief Set memory operand relative displacement.
  inline Mem& setDisplacement(sysint_t displacement)
  {
    _mem.displacement = displacement;
    return *this;
  }

  //! @brief Adjust memory operand relative displacement by @a displacement.
  inline Mem& adjust(sysint_t displacement)
  {
    _mem.displacement += displacement;
    return *this;
  }

  //! @brief Get new memory operand adjusted by @a displacement.
  inline Mem adjusted(sysint_t displacement) const
  {
    Mem result(*this);
    result.adjust(displacement);
    return result;
  }

  // --------------------------------------------------------------------------
  // [Operator Overload]
  // --------------------------------------------------------------------------

#if !defined(ASMJIT_NODOC)
  inline Mem& operator=(const Mem& other)
  {
    _copy(other);
    return *this;
  }

  inline bool operator==(const Mem& other) const
  {
    return _bin.u32[0] == other._bin.u32[0] &&
           _bin.u32[1] == other._bin.u32[1] &&
           _bin.u32[2] == other._bin.u32[2] &&
           _bin.u32[3] == other._bin.u32[3] &&
           _bin.uptr[0] == other._bin.uptr[0] &&
           _bin.uptr[1] == other._bin.uptr[1];
  }

  inline bool operator!=(const Mem& other) const
  {
    return !(*this == other);
  }
#endif // ASMJIT_NODOC
};

// ============================================================================
// [AsmJit::Var]
// ============================================================================

ASMJIT_API Mem _BaseVarMem(const Var& var, uint32_t size);
ASMJIT_API Mem _BaseVarMem(const Var& var, uint32_t size, sysint_t disp);
ASMJIT_API Mem _BaseVarMem(const Var& var, uint32_t size, const GpVar& index, uint32_t shift, sysint_t disp);

//! @brief Base class for all variables.
struct Var : public Operand
{
  // --------------------------------------------------------------------------
  // [Construction / Destruction]
  // --------------------------------------------------------------------------

#if !defined(ASMJIT_NODOC)
  inline Var(const _DontInitialize& dontInitialize) :
    Operand(dontInitialize)
  {
  }
#endif // ASMJIT_NODOC

  inline Var() :
    Operand(_DontInitialize())
  {
    _var.op = kOperandVar;
    _var.size = 0;
    _var.regCode = kInvalidValue;
    _var.varType = kInvalidValue;
    _var.id = kInvalidValue;
  }

  inline Var(const Var& other) :
    Operand(other)
  {
  }

  // --------------------------------------------------------------------------
  // [Type]
  // --------------------------------------------------------------------------

  inline uint32_t getVarType() const
  { return _var.varType; }

  inline bool isGpVar() const
  { return _var.varType <= kX86VarTypeGpq; }

  inline bool isX87Var() const
  { return _var.varType >= kX86VarTypeX87 && _var.varType <= kX86VarTypeX87SD; }

  inline bool isMmVar() const
  { return _var.varType == kX86VarTypeMm; }

  inline bool isXmmVar() const
  { return _var.varType >= kX86VarTypeXmm && _var.varType <= kX86VarTypeXmmPD; }

  // --------------------------------------------------------------------------
  // [Memory Cast]
  // --------------------------------------------------------------------------

  //! @brief Cast this variable to memory operand.
  //!
  //! @note Size of operand depends on native variable type, you can use other
  //! variants if you want specific one.
  inline Mem m() const
  { return _BaseVarMem(*this, kInvalidValue); }

  //! @overload.
  inline Mem m(sysint_t disp) const
  { return _BaseVarMem(*this, kInvalidValue, disp); }

  //! @overload.
  inline Mem m(const GpVar& index, uint32_t shift = 0, sysint_t disp = 0) const
  { return _BaseVarMem(*this, kInvalidValue, index, shift, disp); }

  //! @brief Cast this variable to 8-bit memory operand.
  inline Mem m8() const
  { return _BaseVarMem(*this, 1); }

  //! @overload.
  inline Mem m8(sysint_t disp) const
  { return _BaseVarMem(*this, 1, disp); }

  //! @overload.
  inline Mem m8(const GpVar& index, uint32_t shift = 0, sysint_t disp = 0) const
  { return _BaseVarMem(*this, 1, index, shift, disp); }

  //! @brief Cast this variable to 16-bit memory operand.
  inline Mem m16() const
  { return _BaseVarMem(*this, 2); }

  //! @overload.
  inline Mem m16(sysint_t disp) const
  { return _BaseVarMem(*this, 2, disp); }

  //! @overload.
  inline Mem m16(const GpVar& index, uint32_t shift = 0, sysint_t disp = 0) const
  { return _BaseVarMem(*this, 2, index, shift, disp); }

  //! @brief Cast this variable to 32-bit memory operand.
  inline Mem m32() const
  { return _BaseVarMem(*this, 4); }

  //! @overload.
  inline Mem m32(sysint_t disp) const
  { return _BaseVarMem(*this, 4, disp); }

  //! @overload.
  inline Mem m32(const GpVar& index, uint32_t shift = 0, sysint_t disp = 0) const
  { return _BaseVarMem(*this, 4, index, shift, disp); }

  //! @brief Cast this variable to 64-bit memory operand.
  inline Mem m64() const
  { return _BaseVarMem(*this, 8); }

  //! @overload.
  inline Mem m64(sysint_t disp) const
  { return _BaseVarMem(*this, 8, disp); }

  //! @overload.
  inline Mem m64(const GpVar& index, uint32_t shift = 0, sysint_t disp = 0) const
  { return _BaseVarMem(*this, 8, index, shift, disp); }

  //! @brief Cast this variable to 80-bit memory operand (long double).
  inline Mem m80() const
  { return _BaseVarMem(*this, 10); }

  //! @overload.
  inline Mem m80(sysint_t disp) const
  { return _BaseVarMem(*this, 10, disp); }

  //! @overload.
  inline Mem m80(const GpVar& index, uint32_t shift = 0, sysint_t disp = 0) const
  { return _BaseVarMem(*this, 10, index, shift, disp); }

  //! @brief Cast this variable to 128-bit memory operand.
  inline Mem m128() const
  { return _BaseVarMem(*this, 16); }

  //! @overload.
  inline Mem m128(sysint_t disp) const
  { return _BaseVarMem(*this, 16, disp); }

  //! @overload.
  inline Mem m128(const GpVar& index, uint32_t shift = 0, sysint_t disp = 0) const
  { return _BaseVarMem(*this, 16, index, shift, disp); }

  // --------------------------------------------------------------------------
  // [Operator Overload]
  // --------------------------------------------------------------------------

#if !defined(ASMJIT_NODOC)
  inline Var& operator=(const Var& other)
  { _copy(other); return *this; }

  inline bool operator==(const Var& other) const { return _base.id == other._base.id && _var.regCode == other._var.regCode; }
  inline bool operator!=(const Var& other) const { return _base.id != other._base.id || _var.regCode != other._var.regCode; }
#endif // ASMJIT_NODOC

  // --------------------------------------------------------------------------
  // [Private]
  // --------------------------------------------------------------------------

protected:
  inline Var(const Var& other, uint32_t regCode, uint32_t size) :
    Operand(_DontInitialize())
  {
    _var.op = kOperandVar;
    _var.size = (uint8_t)size;
    _var.id = other._base.id;
    _var.regCode = regCode;
    _var.varType = other._var.varType;
  }
};

// ============================================================================
// [AsmJit::X87Var]
// ============================================================================

//! @brief X87 Variable operand.
struct X87Var : public Var
{
  // --------------------------------------------------------------------------
  // [Construction / Destruction]
  // --------------------------------------------------------------------------

  inline X87Var(const _DontInitialize& dontInitialize) :
    Var(dontInitialize)
  {
  }

  inline X87Var() :
    Var(_DontInitialize())
  {
    _var.op = kOperandVar;
    _var.size = 12;
    _var.id = kInvalidValue;

    _var.regCode = kX86RegTypeX87;
    _var.varType = kX86VarTypeX87;
  }

  inline X87Var(const X87Var& other) :
    Var(other) {}

  // --------------------------------------------------------------------------
  // [Operator Overload]
  // --------------------------------------------------------------------------

#if !defined(ASMJIT_NODOC)
  inline X87Var& operator=(const X87Var& other)
  { _copy(other); return *this; }

  inline bool operator==(const X87Var& other) const { return _base.id == other._base.id; }
  inline bool operator!=(const X87Var& other) const { return _base.id != other._base.id; }
#endif // ASMJIT_NODOC
};

// ============================================================================
// [AsmJit::GpVar]
// ============================================================================

//! @brief GP variable operand.
struct GpVar : public Var
{
  // --------------------------------------------------------------------------
  // [Construction / Destruction]
  // --------------------------------------------------------------------------

  //! @brief Create new uninitialized @c GpVar instance (internal constructor).
  inline GpVar(const _DontInitialize& dontInitialize) :
    Var(dontInitialize)
  {
  }

  //! @brief Create new uninitialized @c GpVar instance.
  inline GpVar() :
    Var(_DontInitialize())
  {
    _var.op = kOperandVar;
    _var.size = sizeof(sysint_t);
    _var.id = kInvalidValue;

    _var.regCode = kX86RegTypeGpz;
    _var.varType = kX86VarTypeGpz;
  }

  //! @brief Create new @c GpVar instance using @a other.
  //!
  //! Note this will not create a different variable, use @c Compiler::newGpVar()
  //! if you want to do so. This is only copy-constructor that allows to store
  //! the same variable in different places.
  inline GpVar(const GpVar& other) :
    Var(other) {}

  // --------------------------------------------------------------------------
  // [GpVar Specific]
  // --------------------------------------------------------------------------

  //! @brief Get whether this variable is general purpose BYTE register.
  inline bool isGpb() const { return (_var.regCode & kRegTypeMask) <= kX86RegTypeGpbHi; }
  //! @brief Get whether this variable is general purpose BYTE.LO register.
  inline bool isGpbLo() const { return (_var.regCode & kRegTypeMask) == kX86RegTypeGpbLo; }
  //! @brief Get whether this variable is general purpose BYTE.HI register.
  inline bool isGpbHi() const { return (_var.regCode & kRegTypeMask) == kX86RegTypeGpbHi; }

  //! @brief Get whether this variable is general purpose WORD register.
  inline bool isGpw() const { return (_var.regCode & kRegTypeMask) == kX86RegTypeGpw; }
  //! @brief Get whether this variable is general purpose DWORD register.
  inline bool isGpd() const { return (_var.regCode & kRegTypeMask) == kX86RegTypeGpd; }
  //! @brief Get whether this variable is general purpose QWORD (only 64-bit) register.
  inline bool isGpq() const { return (_var.regCode & kRegTypeMask) == kX86RegTypeGpq; }

  // --------------------------------------------------------------------------
  // [GpVar Cast]
  // --------------------------------------------------------------------------

  //! @brief Cast this variable to 8-bit (LO) part of variable
  inline GpVar r8() const { return GpVar(*this, kX86RegTypeGpbLo, 1); }
  //! @brief Cast this variable to 8-bit (LO) part of variable
  inline GpVar r8Lo() const { return GpVar(*this, kX86RegTypeGpbLo, 1); }
  //! @brief Cast this variable to 8-bit (HI) part of variable
  inline GpVar r8Hi() const { return GpVar(*this, kX86RegTypeGpbHi, 1); }

  //! @brief Cast this variable to 16-bit part of variable
  inline GpVar r16() const { return GpVar(*this, kX86RegTypeGpw, 2); }
  //! @brief Cast this variable to 32-bit part of variable
  inline GpVar r32() const { return GpVar(*this, kX86RegTypeGpd, 4); }
#if defined(ASMJIT_X64)
  //! @brief Cast this variable to 64-bit part of variable
  inline GpVar r64() const { return GpVar(*this, kX86RegTypeGpq, 8); }
#endif // ASMJIT_X64

  // --------------------------------------------------------------------------
  // [Operator Overload]
  // --------------------------------------------------------------------------

#if !defined(ASMJIT_NODOC)
  inline GpVar& operator=(const GpVar& other)
  { _copy(other); return *this; }

  inline bool operator==(const GpVar& other) const { return _base.id == other._base.id && _var.regCode == other._var.regCode; }
  inline bool operator!=(const GpVar& other) const { return _base.id != other._base.id || _var.regCode != other._var.regCode; }
#endif // ASMJIT_NODOC

  // --------------------------------------------------------------------------
  // [Private]
  // --------------------------------------------------------------------------

protected:
  inline GpVar(const GpVar& other, uint32_t regCode, uint32_t size) :
    Var(other, regCode, size)
  {
  }
};

// ============================================================================
// [AsmJit::MmVar]
// ============================================================================

//! @brief MM variable operand.
struct MmVar : public Var
{
  // --------------------------------------------------------------------------
  // [Construction / Destruction]
  // --------------------------------------------------------------------------

  //! @brief Create new uninitialized @c MmVar instance (internal constructor).
  inline MmVar(const _DontInitialize& dontInitialize) :
    Var(dontInitialize)
  {
  }

  //! @brief Create new uninitialized @c MmVar instance.
  inline MmVar() :
    Var(_DontInitialize())
  {
    _var.op = kOperandVar;
    _var.size = 8;
    _var.id = kInvalidValue;

    _var.regCode = kX86RegTypeMm;
    _var.varType = kX86VarTypeMm;
  }

  //! @brief Create new @c MmVar instance using @a other.
  //!
  //! Note this will not create a different variable, use @c Compiler::newMmVar()
  //! if you want to do so. This is only copy-constructor that allows to store
  //! the same variable in different places.
  inline MmVar(const MmVar& other) :
    Var(other) {}

  // --------------------------------------------------------------------------
  // [MmVar Cast]
  // --------------------------------------------------------------------------

  // --------------------------------------------------------------------------
  // [Operator Overload]
  // --------------------------------------------------------------------------

#if !defined(ASMJIT_NODOC)
  inline MmVar& operator=(const MmVar& other)
  { _copy(other); return *this; }

  inline bool operator==(const MmVar& other) const { return _base.id == other._base.id; }
  inline bool operator!=(const MmVar& other) const { return _base.id != other._base.id; }
#endif // ASMJIT_NODOC
};

// ============================================================================
// [AsmJit::XmmVar]
// ============================================================================

//! @brief XMM Variable operand.
struct XmmVar : public Var
{
  // --------------------------------------------------------------------------
  // [Construction / Destruction]
  // --------------------------------------------------------------------------

  inline XmmVar(const _DontInitialize& dontInitialize) :
    Var(dontInitialize)
  {
  }

  inline XmmVar() :
    Var(_DontInitialize())
  {
    _var.op = kOperandVar;
    _var.size = 16;
    _var.id = kInvalidValue;

    _var.regCode = kX86RegTypeXmm;
    _var.varType = kX86VarTypeXmm;
  }

  inline XmmVar(const XmmVar& other) :
    Var(other) {}

  // --------------------------------------------------------------------------
  // [XmmVar Access]
  // --------------------------------------------------------------------------

  // --------------------------------------------------------------------------
  // [Operator Overload]
  // --------------------------------------------------------------------------

#if !defined(ASMJIT_NODOC)
  inline XmmVar& operator=(const XmmVar& other)
  { _copy(other); return *this; }

  inline bool operator==(const XmmVar& other) const { return _base.id == other._base.id; }
  inline bool operator!=(const XmmVar& other) const { return _base.id != other._base.id; }
#endif // ASMJIT_NODOC
};

// ============================================================================
// [AsmJit::Mem - [label + displacement]]
// ============================================================================

//! @brief Create a custom pointer operand.
ASMJIT_API Mem ptr(const Label& label, sysint_t disp = 0, uint32_t size = 0);
//! @brief Create a byte pointer operand.
static inline Mem byte_ptr(const Label& label, sysint_t disp = 0) { return ptr(label, disp, kSizeByte); }
//! @brief Create a word pointer operand.
static inline Mem word_ptr(const Label& label, sysint_t disp = 0) { return ptr(label, disp, kSizeWord); }
//! @brief Create a dword pointer operand.
static inline Mem dword_ptr(const Label& label, sysint_t disp = 0) { return ptr(label, disp, kSizeDWord); }
//! @brief Create a qword pointer operand.
static inline Mem qword_ptr(const Label& label, sysint_t disp = 0) { return ptr(label, disp, kSizeQWord); }
//! @brief Create a tword pointer operand.
static inline Mem tword_ptr(const Label& label, sysint_t disp = 0) { return ptr(label, disp, kSizeTWord); }
//! @brief Create a dqword pointer operand.
static inline Mem dqword_ptr(const Label& label, sysint_t disp = 0) { return ptr(label, disp, kSizeDQWord); }
//! @brief Create a mmword pointer operand.
static inline Mem mmword_ptr(const Label& label, sysint_t disp = 0) { return ptr(label, disp, kSizeQWord); }
//! @brief Create a xmmword pointer operand.
static inline Mem xmmword_ptr(const Label& label, sysint_t disp = 0) { return ptr(label, disp, kSizeDQWord); }
//! @brief Create an intptr_t pointer operand.
static inline Mem sysint_ptr(const Label& label, sysint_t disp = 0) { return ptr(label, disp, sizeof(sysint_t)); }

// ============================================================================
// [AsmJit::Mem - [label + index << shift + displacement]]
// ============================================================================

//! @brief Create a custom pointer operand.
ASMJIT_API Mem ptr(const Label& label, const GpReg& index, uint32_t shift, sysint_t disp = 0, uint32_t size = 0);
//! @brief Create a byte pointer operand.
static inline Mem byte_ptr(const Label& label, const GpReg& index, uint32_t shift, sysint_t disp = 0) { return ptr(label, index, shift, disp, kSizeByte); }
//! @brief Create a word pointer operand.
static inline Mem word_ptr(const Label& label, const GpReg& index, uint32_t shift, sysint_t disp = 0) { return ptr(label, index, shift, disp, kSizeWord); }
//! @brief Create a dword pointer operand.
static inline Mem dword_ptr(const Label& label, const GpReg& index, uint32_t shift, sysint_t disp = 0) { return ptr(label, index, shift, disp, kSizeDWord); }
//! @brief Create a qword pointer operand.
static inline Mem qword_ptr(const Label& label, const GpReg& index, uint32_t shift, sysint_t disp = 0) { return ptr(label, index, shift, disp, kSizeQWord); }
//! @brief Create a tword pointer operand.
static inline Mem tword_ptr(const Label& label, const GpReg& index, uint32_t shift, sysint_t disp = 0) { return ptr(label, index, shift, disp, kSizeTWord); }
//! @brief Create a dqword pointer operand.
static inline Mem dqword_ptr(const Label& label, const GpReg& index, uint32_t shift, sysint_t disp = 0) { return ptr(label, index, shift, disp, kSizeDQWord); }
//! @brief Create a mmword pointer operand.
static inline Mem mmword_ptr(const Label& label, const GpReg& index, uint32_t shift, sysint_t disp = 0) { return ptr(label, index, shift, disp, kSizeQWord); }
//! @brief Create a xmmword pointer operand.
static inline Mem xmmword_ptr(const Label& label, const GpReg& index, uint32_t shift, sysint_t disp = 0) { return ptr(label, index, shift, disp, kSizeDQWord); }
//! @brief Create an intptr_t pointer operand.
static inline Mem sysint_ptr(const Label& label, const GpReg& index, uint32_t shift, sysint_t disp = 0) { return ptr(label, index, shift, disp, sizeof(sysint_t)); }

//! @brief Create a custom pointer operand.
ASMJIT_API Mem ptr(const Label& label, const GpVar& index, uint32_t shift, sysint_t disp = 0, uint32_t size = 0);
//! @brief Create a byte pointer operand.
static inline Mem byte_ptr(const Label& label, const GpVar& index, uint32_t shift, sysint_t disp = 0) { return ptr(label, index, shift, disp, kSizeByte); }
//! @brief Create a word pointer operand.
static inline Mem word_ptr(const Label& label, const GpVar& index, uint32_t shift, sysint_t disp = 0) { return ptr(label, index, shift, disp, kSizeWord); }
//! @brief Create a dword pointer operand.
static inline Mem dword_ptr(const Label& label, const GpVar& index, uint32_t shift, sysint_t disp = 0) { return ptr(label, index, shift, disp, kSizeDWord); }
//! @brief Create a qword pointer operand.
static inline Mem qword_ptr(const Label& label, const GpVar& index, uint32_t shift, sysint_t disp = 0) { return ptr(label, index, shift, disp, kSizeQWord); }
//! @brief Create a tword pointer operand.
static inline Mem tword_ptr(const Label& label, const GpVar& index, uint32_t shift, sysint_t disp = 0) { return ptr(label, index, shift, disp, kSizeTWord); }
//! @brief Create a dqword pointer operand.
static inline Mem dqword_ptr(const Label& label, const GpVar& index, uint32_t shift, sysint_t disp = 0) { return ptr(label, index, shift, disp, kSizeDQWord); }
//! @brief Create a mmword pointer operand.
static inline Mem mmword_ptr(const Label& label, const GpVar& index, uint32_t shift, sysint_t disp = 0) { return ptr(label, index, shift, disp, kSizeQWord); }
//! @brief Create a xmmword pointer operand.
static inline Mem xmmword_ptr(const Label& label, const GpVar& index, uint32_t shift, sysint_t disp = 0) { return ptr(label, index, shift, disp, kSizeDQWord); }
//! @brief Create an intptr_t pointer operand.
static inline Mem sysint_ptr(const Label& label, const GpVar& index, uint32_t shift, sysint_t disp = 0) { return ptr(label, index, shift, disp, sizeof(sysint_t)); }

// ============================================================================
// [AsmJit::Mem - segment[target + displacement]
// ============================================================================

//! @brief Create a custom pointer operand.
ASMJIT_API Mem ptr_abs(void* target, sysint_t disp = 0, uint32_t size = 0);
//! @brief Create a byte pointer operand.
static inline Mem byte_ptr_abs(void* target, sysint_t disp = 0) { return ptr_abs(target, disp, kSizeByte); }
//! @brief Create a word pointer operand.
static inline Mem word_ptr_abs(void* target, sysint_t disp = 0) { return ptr_abs(target, disp, kSizeWord); }
//! @brief Create a dword pointer operand.
static inline Mem dword_ptr_abs(void* target, sysint_t disp = 0) { return ptr_abs(target, disp, kSizeDWord); }
//! @brief Create a qword pointer operand.
static inline Mem qword_ptr_abs(void* target, sysint_t disp = 0) { return ptr_abs(target, disp, kSizeQWord); }
//! @brief Create a tword pointer operand (used for 80-bit floating points).
static inline Mem tword_ptr_abs(void* target, sysint_t disp = 0) { return ptr_abs(target, disp, kSizeTWord); }
//! @brief Create a dqword pointer operand.
static inline Mem dqword_ptr_abs(void* target, sysint_t disp = 0) { return ptr_abs(target, disp, kSizeDQWord); }
//! @brief Create a mmword pointer operand.
static inline Mem mmword_ptr_abs(void* target, sysint_t disp = 0) { return ptr_abs(target, disp, kSizeQWord); }
//! @brief Create a xmmword pointer operand.
static inline Mem xmmword_ptr_abs(void* target, sysint_t disp = 0) { return ptr_abs(target, disp, kSizeDQWord); }
//! @brief Create an intptr_t pointer operand.
static inline Mem sysint_ptr_abs(void* target, sysint_t disp = 0) { return ptr_abs(target, disp, sizeof(sysint_t)); }

// ============================================================================
// [AsmJit::Mem - segment[target + index << shift + displacement]
// ============================================================================

//! @brief Create a custom pointer operand.
ASMJIT_API Mem ptr_abs(void* target, const GpReg& index, uint32_t shift, sysint_t disp = 0, uint32_t size = 0);
//! @brief Create a byte pointer operand.
static inline Mem byte_ptr_abs(void* target, const GpReg& index, uint32_t shift, sysint_t disp = 0) { return ptr_abs(target, index, shift, disp, kSizeByte); }
//! @brief Create a word pointer operand.
static inline Mem word_ptr_abs(void* target, const GpReg& index, uint32_t shift, sysint_t disp = 0) { return ptr_abs(target, index, shift, disp, kSizeWord); }
//! @brief Create a dword pointer operand.
static inline Mem dword_ptr_abs(void* target, const GpReg& index, uint32_t shift, sysint_t disp = 0) { return ptr_abs(target, index, shift, disp, kSizeDWord); }
//! @brief Create a qword pointer operand.
static inline Mem qword_ptr_abs(void* target, const GpReg& index, uint32_t shift, sysint_t disp = 0) { return ptr_abs(target, index, shift, disp, kSizeQWord); }
//! @brief Create a tword pointer operand.
static inline Mem tword_ptr_abs(void* target, const GpReg& index, uint32_t shift, sysint_t disp = 0) { return ptr_abs(target, index, shift, disp, kSizeTWord); }
//! @brief Create a dqword pointer operand.
static inline Mem dqword_ptr_abs(void* target, const GpReg& index, uint32_t shift, sysint_t disp = 0) { return ptr_abs(target, index, shift, disp, kSizeDQWord); }
//! @brief Create a mmword pointer operand.
static inline Mem mmword_ptr_abs(void* target, const GpReg& index, uint32_t shift, sysint_t disp = 0) { return ptr_abs(target, index, shift, disp, kSizeQWord); }
//! @brief Create a xmmword pointer operand.
static inline Mem xmmword_ptr_abs(void* target, const GpReg& index, uint32_t shift, sysint_t disp = 0) { return ptr_abs(target, index, shift, disp, kSizeDQWord); }
//! @brief Create an intptr_t pointer operand.
static inline Mem sysint_ptr_abs(void* target, const GpReg& index, uint32_t shift, sysint_t disp = 0) { return ptr_abs(target, index, shift, disp, sizeof(sysint_t)); }

//! @brief Create a custom pointer operand.
ASMJIT_API Mem ptr_abs(void* target, const GpVar& index, uint32_t shift, sysint_t disp = 0, uint32_t size = 0);
//! @brief Create a byte pointer operand.
static inline Mem byte_ptr_abs(void* target, const GpVar& index, uint32_t shift, sysint_t disp = 0) { return ptr_abs(target, index, shift, disp, kSizeByte); }
//! @brief Create a word pointer operand.
static inline Mem word_ptr_abs(void* target, const GpVar& index, uint32_t shift, sysint_t disp = 0) { return ptr_abs(target, index, shift, disp, kSizeWord); }
//! @brief Create a dword pointer operand.
static inline Mem dword_ptr_abs(void* target, const GpVar& index, uint32_t shift, sysint_t disp = 0) { return ptr_abs(target, index, shift, disp, kSizeDWord); }
//! @brief Create a qword pointer operand.
static inline Mem qword_ptr_abs(void* target, const GpVar& index, uint32_t shift, sysint_t disp = 0) { return ptr_abs(target, index, shift, disp, kSizeQWord); }
//! @brief Create a tword pointer operand.
static inline Mem tword_ptr_abs(void* target, const GpVar& index, uint32_t shift, sysint_t disp = 0) { return ptr_abs(target, index, shift, disp, kSizeTWord); }
//! @brief Create a dqword pointer operand.
static inline Mem dqword_ptr_abs(void* target, const GpVar& index, uint32_t shift, sysint_t disp = 0) { return ptr_abs(target, index, shift, disp, kSizeDQWord); }
//! @brief Create a mmword pointer operand.
static inline Mem mmword_ptr_abs(void* target, const GpVar& index, uint32_t shift, sysint_t disp = 0) { return ptr_abs(target, index, shift, disp, kSizeQWord); }
//! @brief Create a xmmword pointer operand.
static inline Mem xmmword_ptr_abs(void* target, const GpVar& index, uint32_t shift, sysint_t disp = 0) { return ptr_abs(target, index, shift, disp, kSizeDQWord); }
//! @brief Create an intptr_t pointer operand.
static inline Mem sysint_ptr_abs(void* target, const GpVar& index, uint32_t shift, sysint_t disp = 0) { return ptr_abs(target, index, shift, disp, sizeof(sysint_t)); }

// ============================================================================
// [AsmJit::Mem - ptr[base + displacement]]
// ============================================================================

//! @brief Create a custom pointer operand.
ASMJIT_API Mem ptr(const GpReg& base, sysint_t disp = 0, uint32_t size = 0);
//! @brief Create a byte pointer operand.
static inline Mem byte_ptr(const GpReg& base, sysint_t disp = 0) { return ptr(base, disp, kSizeByte); }
//! @brief Create a word pointer operand.
static inline Mem word_ptr(const GpReg& base, sysint_t disp = 0) { return ptr(base, disp, kSizeWord); }
//! @brief Create a dword pointer operand.
static inline Mem dword_ptr(const GpReg& base, sysint_t disp = 0) { return ptr(base, disp, kSizeDWord); }
//! @brief Create a qword pointer operand.
static inline Mem qword_ptr(const GpReg& base, sysint_t disp = 0) { return ptr(base, disp, kSizeQWord); }
//! @brief Create a tword pointer operand.
static inline Mem tword_ptr(const GpReg& base, sysint_t disp = 0) { return ptr(base, disp, kSizeTWord); }
//! @brief Create a dqword pointer operand.
static inline Mem dqword_ptr(const GpReg& base, sysint_t disp = 0) { return ptr(base, disp, kSizeDQWord); }
//! @brief Create a mmword pointer operand.
static inline Mem mmword_ptr(const GpReg& base, sysint_t disp = 0) { return ptr(base, disp, kSizeQWord); }
//! @brief Create a xmmword pointer operand.
static inline Mem xmmword_ptr(const GpReg& base, sysint_t disp = 0) { return ptr(base, disp, kSizeDQWord); }
//! @brief Create an intptr_t pointer operand.
static inline Mem sysint_ptr(const GpReg& base, sysint_t disp = 0) { return ptr(base, disp, sizeof(sysint_t)); }

//! @brief Create a custom pointer operand.
ASMJIT_API Mem ptr(const GpVar& base, sysint_t disp = 0, uint32_t size = 0);
//! @brief Create a byte pointer operand.
static inline Mem byte_ptr(const GpVar& base, sysint_t disp = 0) { return ptr(base, disp, kSizeByte); }
//! @brief Create a word pointer operand.
static inline Mem word_ptr(const GpVar& base, sysint_t disp = 0) { return ptr(base, disp, kSizeWord); }
//! @brief Create a dword pointer operand.
static inline Mem dword_ptr(const GpVar& base, sysint_t disp = 0) { return ptr(base, disp, kSizeDWord); }
//! @brief Create a qword pointer operand.
static inline Mem qword_ptr(const GpVar& base, sysint_t disp = 0) { return ptr(base, disp, kSizeQWord); }
//! @brief Create a tword pointer operand.
static inline Mem tword_ptr(const GpVar& base, sysint_t disp = 0) { return ptr(base, disp, kSizeTWord); }
//! @brief Create a dqword pointer operand.
static inline Mem dqword_ptr(const GpVar& base, sysint_t disp = 0) { return ptr(base, disp, kSizeDQWord); }
//! @brief Create a mmword pointer operand.
static inline Mem mmword_ptr(const GpVar& base, sysint_t disp = 0) { return ptr(base, disp, kSizeQWord); }
//! @brief Create a xmmword pointer operand.
static inline Mem xmmword_ptr(const GpVar& base, sysint_t disp = 0) { return ptr(base, disp, kSizeDQWord); }
//! @brief Create an intptr_t pointer operand.
static inline Mem sysint_ptr(const GpVar& base, sysint_t disp = 0) { return ptr(base, disp, sizeof(sysint_t)); }

// ============================================================================
// [AsmJit::Mem - ptr[base + (index << shift) + displacement]]
// ============================================================================

//! @brief Create a custom pointer operand.
ASMJIT_API Mem ptr(const GpReg& base, const GpReg& index, uint32_t shift = 0, sysint_t disp = 0, uint32_t size = 0);
//! @brief Create a byte pointer operand.
static inline Mem byte_ptr(const GpReg& base, const GpReg& index, uint32_t shift = 0, sysint_t disp = 0) { return ptr(base, index, shift, disp, kSizeByte); }
//! @brief Create a word pointer operand.
static inline Mem word_ptr(const GpReg& base, const GpReg& index, uint32_t shift = 0, sysint_t disp = 0) { return ptr(base, index, shift, disp, kSizeWord); }
//! @brief Create a dword pointer operand.
static inline Mem dword_ptr(const GpReg& base, const GpReg& index, uint32_t shift = 0, sysint_t disp = 0) { return ptr(base, index, shift, disp, kSizeDWord); }
//! @brief Create a qword pointer operand.
static inline Mem qword_ptr(const GpReg& base, const GpReg& index, uint32_t shift = 0, sysint_t disp = 0) { return ptr(base, index, shift, disp, kSizeQWord); }
//! @brief Create a tword pointer operand.
static inline Mem tword_ptr(const GpReg& base, const GpReg& index, uint32_t shift = 0, sysint_t disp = 0) { return ptr(base, index, shift, disp, kSizeTWord); }
//! @brief Create a dqword pointer operand.
static inline Mem dqword_ptr(const GpReg& base, const GpReg& index, uint32_t shift = 0, sysint_t disp = 0) { return ptr(base, index, shift, disp, kSizeDQWord); }
//! @brief Create a mmword pointer operand.
static inline Mem mmword_ptr(const GpReg& base, const GpReg& index, uint32_t shift = 0, sysint_t disp = 0) { return ptr(base, index, shift, disp, kSizeQWord); }
//! @brief Create a xmmword pointer operand.
static inline Mem xmmword_ptr(const GpReg& base, const GpReg& index, uint32_t shift = 0, sysint_t disp = 0) { return ptr(base, index, shift, disp, kSizeDQWord); }
//! @brief Create an intptr_t pointer operand.
static inline Mem sysint_ptr(const GpReg& base, const GpReg& index, uint32_t shift = 0, sysint_t disp = 0) { return ptr(base, index, shift, disp, sizeof(sysint_t)); }

//! @brief Create a custom pointer operand.
ASMJIT_API Mem ptr(const GpVar& base, const GpVar& index, uint32_t shift = 0, sysint_t disp = 0, uint32_t size = 0);
//! @brief Create a byte pointer operand.
static inline Mem byte_ptr(const GpVar& base, const GpVar& index, uint32_t shift = 0, sysint_t disp = 0) { return ptr(base, index, shift, disp, kSizeByte); }
//! @brief Create a word pointer operand.
static inline Mem word_ptr(const GpVar& base, const GpVar& index, uint32_t shift = 0, sysint_t disp = 0) { return ptr(base, index, shift, disp, kSizeWord); }
//! @brief Create a dword pointer operand.
static inline Mem dword_ptr(const GpVar& base, const GpVar& index, uint32_t shift = 0, sysint_t disp = 0) { return ptr(base, index, shift, disp, kSizeDWord); }
//! @brief Create a qword pointer operand.
static inline Mem qword_ptr(const GpVar& base, const GpVar& index, uint32_t shift = 0, sysint_t disp = 0) { return ptr(base, index, shift, disp, kSizeQWord); }
//! @brief Create a tword pointer operand.
static inline Mem tword_ptr(const GpVar& base, const GpVar& index, uint32_t shift = 0, sysint_t disp = 0) { return ptr(base, index, shift, disp, kSizeTWord); }
//! @brief Create a dqword pointer operand.
static inline Mem dqword_ptr(const GpVar& base, const GpVar& index, uint32_t shift = 0, sysint_t disp = 0) { return ptr(base, index, shift, disp, kSizeDQWord); }
//! @brief Create a mmword pointer operand.
static inline Mem mmword_ptr(const GpVar& base, const GpVar& index, uint32_t shift = 0, sysint_t disp = 0) { return ptr(base, index, shift, disp, kSizeQWord); }
//! @brief Create a xmmword pointer operand.
static inline Mem xmmword_ptr(const GpVar& base, const GpVar& index, uint32_t shift = 0, sysint_t disp = 0) { return ptr(base, index, shift, disp, kSizeDQWord); }
//! @brief Create an intptr_t pointer operand.
static inline Mem sysint_ptr(const GpVar& base, const GpVar& index, uint32_t shift = 0, sysint_t disp = 0) { return ptr(base, index, shift, disp, sizeof(sysint_t)); }

// ============================================================================
// [AsmJit::Macros]
// ============================================================================

//! @brief Create Shuffle Constant for MMX/SSE shuffle instrutions.
//! @param z First component position, number at interval [0, 3] inclusive.
//! @param x Second component position, number at interval [0, 3] inclusive.
//! @param y Third component position, number at interval [0, 3] inclusive.
//! @param w Fourth component position, number at interval [0, 3] inclusive.
//!
//! Shuffle constants can be used to make immediate value for these intrinsics:
//! - @ref X86Assembler::pshufw()
//! - @ref X86Assembler::pshufd()
//! - @ref X86Assembler::pshufhw()
//! - @ref X86Assembler::pshuflw()
//! - @ref X86Assembler::shufps()
static inline uint8_t mm_shuffle(uint8_t z, uint8_t y, uint8_t x, uint8_t w)
{ return (z << 6) | (y << 4) | (x << 2) | w; }

//! @}

} // AsmJit namespace

// [Guard]
#endif // _ASMJIT_X86_X86OPERAND_H
