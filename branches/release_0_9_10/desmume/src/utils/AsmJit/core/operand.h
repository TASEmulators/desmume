// [AsmJit]
// Complete JIT Assembler for C++ Language.
//
// [License]
// Zlib - See COPYING file in this package.

// [Guard]
#ifndef _ASMJIT_CORE_OPERAND_H
#define _ASMJIT_CORE_OPERAND_H

// [Dependencies - AsmJit]
#include "../core/defs.h"

namespace AsmJit {

//! @addtogroup AsmJit_Core
//! @{

// ============================================================================
// [AsmJit::_OperandData]
// ============================================================================

//! @internal
//! 
//! @brief Base operand data.
struct _OpBase
{
  //! @brief Type of operand, see @c kOperandType.
  uint8_t op;
  //! @brief Size of operand (register, address, immediate, or variable).
  uint8_t size;
  //! @brief Not used.
  uint8_t reserved[2];

  //! @brief Operand ID (private variable for @c Assembler and @c Compiler classes).
  //!
  //! @note Uninitialized operand has always zero id.
  uint32_t id;
};

//! @internal
//! 
//! @brief Label operand data.
struct _OpLabel
{
  //! @brief Type of operand, see @c kOperandType (in this case @c kOperandLabel).
  uint8_t op;
  //! @brief Size of label, currently not used.
  uint8_t size;
  //! @brief Not used.
  uint8_t reserved[2];

  //! @brief Operand ID.
  uint32_t id;
};

//! @internal
//! 
//! @brief Register operand data.
struct _OpReg
{
  //! @brief Type of operand, see @c kOperandType (in this case @c kOperandReg).
  uint8_t op;
  //! @brief Size of register.
  uint8_t size;
  //! @brief Not used.
  uint8_t reserved[2];

  //! @brief Operand id.
  uint32_t id;
  //! @brief Register/Variable code, see @c REG.
  uint32_t code;
};

//! @internal
//! 
//! @brief Variable operand data.
struct _OpVar
{
  //! @brief Type of operand, see @c kOperandType (in this case @c kOperandVar).
  uint8_t op;
  //! @brief Size of variable (0 if don't known).
  uint8_t size;
  //! @brief Not used.
  uint8_t reserved[2];

  //! @brief Operand ID.
  uint32_t id;

  //! @brief Type (and later also code) of register, see @c kX86RegType, @c kX86RegCode.
  //!
  //! @note Register code and variable code are two different things. In most
  //! cases regCode is very related to varType, but general purpose registers 
  //! are divided to 64-bit, 32-bit, 16-bit and 8-bit entities so the regCode
  //! can be used to access these, varType remains unchanged from the 
  //! initialization state. Variable type describes mainly variable type and
  //! home memory size.
  uint32_t regCode;

  //! @brief Type of variable. See @c kX86VarType enum.
  uint32_t varType;
};

//! @internal
//!
//! @brief Memory operand data.
struct _OpMem
{
  //! @brief Type of operand, see @c kOperandType (in this case @c kOperandMem).
  uint8_t op;
  //! @brief Size of pointer.
  uint8_t size;

  //! @brief Memory operand type, see @c kOperandMemType.
  uint8_t type;
  //! @brief Segment override prefix, see @c kX86Seg.
  uint8_t segment : 4;
  //! @brief Emit MOV/LEA instruction using 16-bit/32-bit form of base/index
  //! registers.
  uint8_t sizePrefix : 1;
  //! @brief Index register shift/scale (0 to 3 inclusive, see @c kScale).
  uint8_t shift : 3;

  //! @brief Operand ID.
  uint32_t id;

  //! @brief Base register index, variable or label id.
  uint32_t base;
  //! @brief Index register index or variable id.
  uint32_t index;

  //! @brief Target (for 32-bit, absolute address).
  void* target;
  //! @brief Displacement.
  sysint_t displacement;
};

//! @internal
//! 
//! @brief Immediate operand data.
struct _OpImm
{
  //! @brief Type of operand, see @c kOperandType (in this case @c kOperandImm)..
  uint8_t op;
  //! @brief Size of immediate (or 0 to autodetect).
  uint8_t size;
  //! @brief @c true if immediate is unsigned.
  uint8_t isUnsigned;
  //! @brief Not used.
  uint8_t reserved;

  //! @brief Operand ID.
  uint32_t id;
  //! @brief Immediate value.
  sysint_t value;
};

//! @internal
//!
//! @brief Binary operand data.
struct _OpBin
{
  //! @brief First four 32-bit integers.
  uint32_t u32[4];
  //! @brief Second two 32-bit or 64-bit integers.
  uintptr_t uptr[2];
};

// ============================================================================
// [AsmJit::Operand]
// ============================================================================

//! @brief Operand can contain register, memory location, immediate, or label.
struct Operand
{
  // --------------------------------------------------------------------------
  // [Construction / Destruction]
  // --------------------------------------------------------------------------

  //! @brief Create an uninitialized operand.
  inline Operand()
  {
    memset(this, 0, sizeof(Operand));
    _base.id = kInvalidValue;
  }

  //! @brief Create a reference to @a other operand.
  inline Operand(const Operand& other)
  {
    _init(other);
  }

#if !defined(ASMJIT_NODOC)
  inline Operand(const _DontInitialize&) {}
#endif // ASMJIT_NODOC

  // --------------------------------------------------------------------------
  // [Init & Copy]
  // --------------------------------------------------------------------------

  //! @internal
  //!
  //! @brief Initialize operand to @a other (used by constructors).
  inline void _init(const Operand& other)
  { memcpy(this, &other, sizeof(Operand)); }

  //! @internal
  //!
  //! @brief Initialize operand to @a other (used by assign operators).
  inline void _copy(const Operand& other)
  { memcpy(this, &other, sizeof(Operand)); }

  // --------------------------------------------------------------------------
  // [Data]
  // --------------------------------------------------------------------------

  template<typename T>
  inline T& getData() { return reinterpret_cast<T&>(_base); }

  template<typename T>
  inline const T& getData() const { return reinterpret_cast<const T&>(_base); }

  // --------------------------------------------------------------------------
  // [Type]
  // --------------------------------------------------------------------------

  //! @brief Get type of the operand, see @c kOperandType.
  inline uint32_t getType() const
  { return _base.op; }

  //! @brief Get whether the operand is none (@c kOperandNone).
  inline bool isNone() const
  { return (_base.op == kOperandNone); }

  //! @brief Get whether the operand is any (general purpose, mmx or sse) register (@c kOperandReg).
  inline bool isReg() const
  { return (_base.op == kOperandReg); }

  //! @brief Get whether the operand is memory address (@c kOperandMem).
  inline bool isMem() const
  { return (_base.op == kOperandMem); }

  //! @brief Get whether the operand is immediate (@c kOperandImm).
  inline bool isImm() const
  { return (_base.op == kOperandImm); }

  //! @brief Get whether the operand is label (@c kOperandLabel).
  inline bool isLabel() const
  { return (_base.op == kOperandLabel); }

  //! @brief Get whether the operand is variable (@c kOperandVar).
  inline bool isVar() const
  { return (_base.op == kOperandVar); }

  //! @brief Get whether the operand is variable or memory.
  inline bool isVarMem() const
  { return ((_base.op & (kOperandVar | kOperandMem)) != 0); }

  //! @brief Get whether the operand is register and type of register is @a regType.
  inline bool isRegType(uint32_t regType) const
  { return (_base.op == kOperandReg) & ((_reg.code & kRegTypeMask) == regType); }

  //! @brief Get whether the operand is register and code of register is @a regCode.
  inline bool isRegCode(uint32_t regCode) const
  { return (_base.op == kOperandReg) & (_reg.code == regCode); }

  //! @brief Get whether the operand is register and index of register is @a regIndex.
  inline bool isRegIndex(uint32_t regIndex) const
  { return (_base.op == kOperandReg) & ((_reg.code & kRegIndexMask) == (regIndex & kRegIndexMask)); }

  //! @brief Get whether the operand is any register or memory.
  inline bool isRegMem() const
  { return ((_base.op & (kOperandReg | kOperandMem)) != 0); }

  //! @brief Get whether the operand is register of @a regType type or memory.
  inline bool isRegTypeMem(uint32_t regType) const
  { return ((_base.op == kOperandReg) & ((_reg.code & kRegTypeMask) == regType)) | (_base.op == kOperandMem); }

  // --------------------------------------------------------------------------
  // [Size]
  // --------------------------------------------------------------------------

  //! @brief Get size of the operand in bytes.
  inline uint32_t getSize() const
  { return _base.size; }

  // --------------------------------------------------------------------------
  // [Id]
  // --------------------------------------------------------------------------

  //! @brief Return operand Id (Operand Id's are used internally by 
  //! @c Assembler and @c Compiler classes).
  //!
  //! @note There is no way how to change or remove operand id. If you don't
  //! need the operand just assign different operand to this one.
  inline uint32_t getId() const
  { return _base.id; }

  // --------------------------------------------------------------------------
  // [Members]
  // --------------------------------------------------------------------------

  union
  {
    //! @brief Base operand data.
    _OpBase _base;
    //! @brief Label operand data.
    _OpLabel _lbl;
    //! @brief Register operand data.
    _OpReg _reg;
    //! @brief Variable operand data.
    _OpVar _var;
    //! @brief Memory operand data.
    _OpMem _mem;
    //! @brief Immediate operand data.
    _OpImm _imm;
    //! @brief Binary data.
    _OpBin _bin;
  };
};

ASMJIT_VAR const Operand noOperand;

// ============================================================================
// [AsmJit::Reg]
// ============================================================================

//! @brief Base class for all register operands.
struct Reg : public Operand
{
  // --------------------------------------------------------------------------
  // [Construction / Destruction]
  // --------------------------------------------------------------------------

  //! @brief Create a new base register.
  inline Reg(uint32_t code, uint32_t size) : Operand(_DontInitialize())
  {
    _reg.op = kOperandReg;
    _reg.size = (uint8_t)size;
    _reg.id = kInvalidValue;
    _reg.code = code;
  }

  //! @brief Create a new reference to @a other.
  inline Reg(const Reg& other) : Operand(other) {}

#if !defined(ASMJIT_NODOC)
  inline Reg(const _DontInitialize& dontInitialize) : Operand(dontInitialize) {}
#endif // ASMJIT_NODOC

  // --------------------------------------------------------------------------
  // [Reg Specific]
  // --------------------------------------------------------------------------

  //! @brief Get register code, see @c REG.
  inline uint32_t getRegCode() const
  { return (uint32_t)(_reg.code); }

  //! @brief Get register type, see @c REG.
  inline uint32_t getRegType() const
  { return (uint32_t)(_reg.code & kRegTypeMask); }

  //! @brief Get register index (value from 0 to 7/15).
  inline uint32_t getRegIndex() const
  { return (uint32_t)(_reg.code & kRegIndexMask); }

  //! @brief Get whether register code is equal to @a code.
  inline bool isRegCode(uint32_t code) const
  { return _reg.code == code; }

  //! @brief Get whether register code is equal to @a type.
  inline bool isRegType(uint32_t type) const
  { return (uint32_t)(_reg.code & kRegTypeMask) == type; }

  //! @brief Get whether register index is equal to @a index.
  inline bool isRegIndex(uint32_t index) const
  { return (uint32_t)(_reg.code & kRegIndexMask) == index; }

  //! @brief Set register code to @a code.
  inline Reg& setCode(uint32_t code)
  {
    _reg.code = code;
    return *this;
  }

  //! @brief Set register size to @a size.
  inline Reg& setSize(uint32_t size)
  {
    _reg.size = static_cast<uint8_t>(size);
    return *this;
  }

  // --------------------------------------------------------------------------
  // [Operator Overload]
  // --------------------------------------------------------------------------

  inline Reg& operator=(const Reg& other)
  { _copy(other); return *this; }

  inline bool operator==(const Reg& other) const
  { return getRegCode() == other.getRegCode(); }

  inline bool operator!=(const Reg& other) const
  { return getRegCode() != other.getRegCode(); }
};

// ============================================================================
// [AsmJit::Imm]
// ============================================================================

//! @brief Immediate operand.
//!
//! Immediate operand is part of instruction (it's inlined after it).
//!
//! To create immediate operand, use @c imm() and @c uimm() constructors
//! or constructors provided by @c Immediate class itself.
struct Imm : public Operand
{
  // --------------------------------------------------------------------------
  // [Construction / Destruction]
  // --------------------------------------------------------------------------

  //! @brief Create a new immediate value (initial value is 0).
  Imm() : Operand(_DontInitialize())
  {
    _imm.op = kOperandImm;
    _imm.size = 0;
    _imm.isUnsigned = false;
    _imm.reserved = 0;

    _imm.id = kInvalidValue;
    _imm.value = 0;
  }

  //! @brief Create a new signed immediate value, assigning the value to @a i.
  Imm(sysint_t i) : Operand(_DontInitialize())
  {
    _imm.op = kOperandImm;
    _imm.size = 0;
    _imm.isUnsigned = false;
    _imm.reserved = 0;

    _imm.id = kInvalidValue;
    _imm.value = i;
  }

  //! @brief Create a new signed or unsigned immediate value, assigning the value to @a i.
  Imm(sysint_t i, bool isUnsigned) : Operand(_DontInitialize())
  {
    _imm.op = kOperandImm;
    _imm.size = 0;
    _imm.isUnsigned = isUnsigned;
    _imm.reserved = 0;

    _imm.id = kInvalidValue;
    _imm.value = i;
  }

  //! @brief Create a new immediate value from @a other.
  inline Imm(const Imm& other) : Operand(other)
  {
  }

  // --------------------------------------------------------------------------
  // [Immediate Specific]
  // --------------------------------------------------------------------------

  //! @brief Get whether an immediate is unsigned value.
  inline bool isUnsigned() const
  { return _imm.isUnsigned != 0; }

  //! @brief Get signed immediate value.
  inline sysint_t getValue() const
  { return _imm.value; }

  //! @brief Get unsigned immediate value.
  inline sysuint_t getUValue() const
  { return static_cast<sysuint_t>(_imm.value); }

  //! @brief Set immediate value as signed type to @a val.
  inline Imm& setValue(sysint_t val, bool isUnsigned = false)
  {
    _imm.value = val;
    _imm.isUnsigned = isUnsigned;
    return *this;
  }

  //! @brief Set immediate value as unsigned type to @a val.
  inline Imm& setUValue(sysuint_t val)
  {
    _imm.value = (sysint_t)val;
    _imm.isUnsigned = true;
    return *this;
  }

  // --------------------------------------------------------------------------
  // [Operator Overload]
  // --------------------------------------------------------------------------

  //! @brief Assign a signed value @a val to the immediate operand.
  inline Imm& operator=(sysint_t val)
  { setValue(val); return *this; }

  //! @brief Assign @a other to the immediate operand.
  inline Imm& operator=(const Imm& other)
  { _copy(other); return *this; }
};

//! @brief Create signed immediate value operand.
ASMJIT_API Imm imm(sysint_t i);

//! @brief Create unsigned immediate value operand.
ASMJIT_API Imm uimm(sysuint_t i);

// ============================================================================
// [AsmJit::Label]
// ============================================================================

//! @brief Label (jump target or data location).
//!
//! Label represents locations typically used as jump targets, but may be also
//! used as position where are stored constants or static variables. If you 
//! want to use @c Label you need first to associate it with @c Assembler or
//! @c Compiler instance. To create new label use @c Assembler::newLabel() or
//! @c Compiler::newLabel().
//!
//! Example of using labels:
//!
//! @code
//! // Create Assembler or Compiler instance.
//! X86Assembler a;
//! 
//! // Create Label instance.
//! Label L_1(a);
//!
//! // ... your code ...
//!
//! // Using label, see @c AsmJit::Assembler or @c AsmJit::Compiler.
//! a.jump(L_1);
//!
//! // ... your code ...
//!
//! // Bind label to current position, see @c AsmJit::Assembler::bind() or
//! // @c AsmJit::Compiler::bind().
//! a.bind(L_1);
//! @endcode
struct Label : public Operand
{
  // --------------------------------------------------------------------------
  // [Construction / Destruction]
  // --------------------------------------------------------------------------

  //! @brief Create new, unassociated label.
  inline Label() : Operand(_DontInitialize())
  {
    _lbl.op = kOperandLabel;
    _lbl.size = 0;
    _lbl.id = kInvalidValue;
  }

  //! @brief Create reference to another label.
  inline Label(const Label& other) : Operand(other) {}

  //! @brief Destroy the label.
  inline ~Label() {}

  // --------------------------------------------------------------------------
  // [Operator Overload]
  // --------------------------------------------------------------------------

#if !defined(ASMJIT_NODOC)
  inline Label& operator=(const Label& other)
  { _copy(other); return *this; }

  inline bool operator==(const Label& other) const { return _base.id == other._base.id; }
  inline bool operator!=(const Label& other) const { return _base.id != other._base.id; }
#endif // ASMJIT_NODOC
};

//! @}

} // AsmJit namespace

// [Guard]
#endif // _ASMJIT_CORE_OPERAND_H
