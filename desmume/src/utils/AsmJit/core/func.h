// [AsmJit]
// Complete JIT Assembler for C++ Language.
//
// [License]
// Zlib - See COPYING file in this package.

// [Guard]
#ifndef _ASMJIT_CORE_FUNC_H
#define _ASMJIT_CORE_FUNC_H

// [Dependencies - AsmJit]
#include "../core/assert.h"
#include "../core/defs.h"

// [Api-Begin]
#include "../core/apibegin.h"

namespace AsmJit {

// ============================================================================
// [Forward Declaration]
// ============================================================================

template<typename T>
struct TypeId;

// ============================================================================
// [AsmJit::TypeId]
// ============================================================================

#if defined(ASMJIT_HAS_PARTIAL_TEMPLATE_SPECIALIZATION)

#define ASMJIT_DECLARE_TYPE_CORE(_PtrId_) \
  template<typename T> \
  struct TypeId \
  { \
    enum \
    { \
      Id = static_cast<int>(::AsmJit::kVarTypeInvalid) \
    }; \
  }; \
  \
  template<typename T> \
  struct TypeId<T*> { enum { Id = _PtrId_ }; }

#else

// Code without partial template specialization is a bit complex. We need to
// determine whether the size of the return value of this function is equal
// to sizeof(char) or sizeof(void*). Any sizeof() can be used to distinguish
// between these two, but these are commonly used in other libraries as well.
template<typename T>
char TypeId_NoPtiHelper(T*(*)());
// And specialization.
void* TypeId_NoPtiHelper(...);

#define ASMJIT_DECLARE_TYPE_CORE(_PtrId_) \
  template<typename T> \
  struct TypeId \
  { \
    enum \
    { \
      Id = (sizeof( ::AsmJit::TypeId_NoPtiHelper((T(*)())0) ) == sizeof(char) \
        ? static_cast<int>(_PtrId_) \
        : static_cast<int>(::AsmJit::kVarTypeInvalid)) \
    }; \
  }

#endif // ASMJIT_HAS_PARTIAL_TEMPLATE_SPECIALIZATION

//! @brief Declare C/C++ type-id mapped to @c AsmJit::kX86VarType.
#define ASMJIT_DECLARE_TYPE_ID(_T_, _Id_) \
  template<> \
  struct TypeId<_T_> { enum { Id = _Id_ }; }

// ============================================================================
// [AsmJit::FuncArg]
// ============================================================================

//! @brief Function argument translated from @ref FuncPrototype.
struct FuncArg
{
  // --------------------------------------------------------------------------
  // [Accessors]
  // --------------------------------------------------------------------------

  inline uint32_t getVarType() const
  { return _varType; }

  inline uint32_t getRegIndex() const
  { return _regIndex; }

  inline bool hasRegIndex() const
  { return _regIndex != kRegIndexInvalid; }

  inline int32_t getStackOffset() const
  { return static_cast<int32_t>(_stackOffset); }

  inline bool hasStackOffset() const
  { return _stackOffset != kFuncStackInvalid; }

  //! @brief Get whether the argument is assigned, for private use only.
  inline bool isAssigned() const
  { return (_regIndex != kRegIndexInvalid) | (_stackOffset != kFuncStackInvalid); }

  // --------------------------------------------------------------------------
  // [Reset]
  // --------------------------------------------------------------------------

  //! @brief Reset the function argument to "unassigned state".
  inline void reset()
  { _packed = 0xFFFFFFFF; }

  // --------------------------------------------------------------------------
  // [Members]
  // --------------------------------------------------------------------------

  union
  {
    struct
    {
      //! @brief Variable type, see @c kVarType.
      uint8_t _varType;
      //! @brief Register index is argument is passed through register.
      uint8_t _regIndex;
      //! @brief Stack offset if argument is passed through stack.
      int16_t _stackOffset;
    };

    //! @brief All members packed into single 32-bit integer.
    uint32_t _packed;
  };
};

// ============================================================================
// [AsmJit::FuncPrototype]
// ============================================================================

//! @brief Function prototype.
//!
//! Function prototype contains information about function return type, count
//! of arguments and their types. Function definition is low level structure
//! which doesn't contain platform or calling convention specific information.
struct FuncPrototype
{
  // --------------------------------------------------------------------------
  // [Accessors]
  // --------------------------------------------------------------------------

  //! @brief Get function return value.
  inline uint32_t getReturnType() const
  { return _returnType; }

  //! @brief Get count of function arguments.
  inline uint32_t getArgumentsCount() const
  { return _argumentsCount; }

  //! @brief Get argument at index @a id.
  inline uint32_t getArgument(uint32_t id) const
  {
    ASMJIT_ASSERT(id < _argumentsCount);
    return _arguments[id];
  }

  //! @brief Get function arguments' IDs.
  inline const uint32_t* getArguments() const
  { return _arguments; }

  //! @brief Set function definition - return type and arguments.
  inline void _setPrototype(uint32_t returnType, const uint32_t* arguments, uint32_t argumentsCount)
  {
    _returnType = returnType;
    _arguments = arguments;
    _argumentsCount = argumentsCount;
  }

  // --------------------------------------------------------------------------
  // [Members]
  // --------------------------------------------------------------------------

  uint32_t _returnType;
  uint32_t _argumentsCount;
  const uint32_t* _arguments;
};

// ============================================================================
// [AsmJit::FuncDecl]
// ============================================================================

//! @brief Function declaration.
struct FuncDecl
{
  // --------------------------------------------------------------------------
  // [Accessors]
  // --------------------------------------------------------------------------

  //! @brief Get function return value or @ref kInvalidValue if it's void.
  inline uint32_t getReturnType() const
  { return _returnType; }

  //! @brief Get count of function arguments.
  inline uint32_t getArgumentsCount() const
  { return _argumentsCount; }

  //! @brief Get function arguments array.
  inline FuncArg* getArguments()
  { return _arguments; }

  //! @brief Get function arguments array (const).
  inline const FuncArg* getArguments() const
  { return _arguments; }

  //! @brief Get function argument at index @a index.
  inline FuncArg& getArgument(size_t index)
  {
    ASMJIT_ASSERT(index < static_cast<size_t>(_argumentsCount));
    return _arguments[index];
  }

  //! @brief Get function argument at index @a index.
  inline const FuncArg& getArgument(size_t index) const
  {
    ASMJIT_ASSERT(index < static_cast<size_t>(_argumentsCount));
    return _arguments[index];
  }

  // --------------------------------------------------------------------------
  // [Members]
  // --------------------------------------------------------------------------

  //! @brief Function return type.
  uint8_t _returnType;
  //! @brief Count of arguments (in @c _argumentsList).
  uint8_t _argumentsCount;
  //! @brief Reserved for future use (alignment).
  uint8_t _reserved0[2];

  //! @brief Function arguments array.
  FuncArg _arguments[kFuncArgsMax];
};

// ============================================================================
// [AsmJit::FuncBuilderX]
// ============================================================================

//! @brief Custom function builder for up to 32 function arguments.
struct FuncBuilderX : public FuncPrototype
{
  // --------------------------------------------------------------------------
  // [Construction / Destruction]
  // --------------------------------------------------------------------------

  inline FuncBuilderX()
  { _setPrototype(kVarTypeInvalid, _argumentsData, 0); }

  // --------------------------------------------------------------------------
  // [Accessors]
  // --------------------------------------------------------------------------

  template<typename T>
  inline void setReturnTypeT()
  { setReturnTypeRaw(TypeId<ASMJIT_TYPE_TO_TYPE(T)>::Id); }

  template<typename T>
  inline void setArgumentT(uint32_t id)
  { setArgumentRaw(id, TypeId<ASMJIT_TYPE_TO_TYPE(T)>::Id); }

  template<typename T>
  inline void addArgumentT()
  { addArgumentRaw(TypeId<ASMJIT_TYPE_TO_TYPE(T)>::Id); }

  inline void setReturnTypeRaw(uint32_t returnType)
  { _returnType = returnType; }

  inline void setArgumentRaw(uint32_t id, uint32_t type)
  {
    ASMJIT_ASSERT(id < _argumentsCount);
    _argumentsData[id] = type;
  }

  inline void addArgumentRaw(uint32_t type)
  {
    ASMJIT_ASSERT(_argumentsCount < kFuncArgsMax);
    _argumentsData[_argumentsCount++] = type;
  }

  // --------------------------------------------------------------------------
  // [Members]
  // --------------------------------------------------------------------------

  uint32_t _argumentsData[kFuncArgsMax];
};

//! @brief Class used to build function without arguments.
template<typename RET>
struct FuncBuilder0 : public FuncPrototype
{
  inline FuncBuilder0()
  {
    _setPrototype(TypeId<ASMJIT_TYPE_TO_TYPE(RET)>::Id, NULL, 0);
  }
};

//! @brief Class used to build function with 1 argument.
template<typename RET, typename P0>
struct FuncBuilder1 : public FuncPrototype
{
  inline FuncBuilder1()
  {
    static const uint32_t arguments[] =
    {
      TypeId<ASMJIT_TYPE_TO_TYPE(P0)>::Id
    };

    _setPrototype(TypeId<ASMJIT_TYPE_TO_TYPE(RET)>::Id, arguments, ASMJIT_ARRAY_SIZE(arguments));
  }
};

//! @brief Class used to build function with 2 arguments.
template<typename RET, typename P0, typename P1>
struct FuncBuilder2 : public FuncPrototype
{
  inline FuncBuilder2()
  {
    static const uint32_t arguments[] =
    {
      TypeId<ASMJIT_TYPE_TO_TYPE(P0)>::Id,
      TypeId<ASMJIT_TYPE_TO_TYPE(P1)>::Id
    };

    _setPrototype(TypeId<ASMJIT_TYPE_TO_TYPE(RET)>::Id, arguments, ASMJIT_ARRAY_SIZE(arguments));
  }
};

//! @brief Class used to build function with 3 arguments.
template<typename RET, typename P0, typename P1, typename P2>
struct FuncBuilder3 : public FuncPrototype
{
  inline FuncBuilder3()
  {
    static const uint32_t arguments[] =
    {
      TypeId<ASMJIT_TYPE_TO_TYPE(P0)>::Id,
      TypeId<ASMJIT_TYPE_TO_TYPE(P1)>::Id,
      TypeId<ASMJIT_TYPE_TO_TYPE(P2)>::Id
    };

    _setPrototype(TypeId<ASMJIT_TYPE_TO_TYPE(RET)>::Id, arguments, ASMJIT_ARRAY_SIZE(arguments));
  }
};

//! @brief Class used to build function with 4 arguments.
template<typename RET, typename P0, typename P1, typename P2, typename P3>
struct FuncBuilder4 : public FuncPrototype
{
  inline FuncBuilder4()
  {
    static const uint32_t arguments[] =
    {
      TypeId<ASMJIT_TYPE_TO_TYPE(P0)>::Id,
      TypeId<ASMJIT_TYPE_TO_TYPE(P1)>::Id,
      TypeId<ASMJIT_TYPE_TO_TYPE(P2)>::Id,
      TypeId<ASMJIT_TYPE_TO_TYPE(P3)>::Id
    };

    _setPrototype(TypeId<ASMJIT_TYPE_TO_TYPE(RET)>::Id, arguments, ASMJIT_ARRAY_SIZE(arguments));
  }
};

//! @brief Class used to build function with 5 arguments.
template<typename RET, typename P0, typename P1, typename P2, typename P3, typename P4>
struct FuncBuilder5 : public FuncPrototype
{
  inline FuncBuilder5()
  {
    static const uint32_t arguments[] =
    {
      TypeId<ASMJIT_TYPE_TO_TYPE(P0)>::Id,
      TypeId<ASMJIT_TYPE_TO_TYPE(P1)>::Id,
      TypeId<ASMJIT_TYPE_TO_TYPE(P2)>::Id,
      TypeId<ASMJIT_TYPE_TO_TYPE(P3)>::Id,
      TypeId<ASMJIT_TYPE_TO_TYPE(P4)>::Id
    };

    _setPrototype(TypeId<ASMJIT_TYPE_TO_TYPE(RET)>::Id, arguments, ASMJIT_ARRAY_SIZE(arguments));
  }
};

//! @brief Class used to build function with 6 arguments.
template<typename RET, typename P0, typename P1, typename P2, typename P3, typename P4, typename P5>
struct FuncBuilder6 : public FuncPrototype
{
  inline FuncBuilder6()
  {
    static const uint32_t arguments[] =
    {
      TypeId<ASMJIT_TYPE_TO_TYPE(P0)>::Id,
      TypeId<ASMJIT_TYPE_TO_TYPE(P1)>::Id,
      TypeId<ASMJIT_TYPE_TO_TYPE(P2)>::Id,
      TypeId<ASMJIT_TYPE_TO_TYPE(P3)>::Id,
      TypeId<ASMJIT_TYPE_TO_TYPE(P4)>::Id,
      TypeId<ASMJIT_TYPE_TO_TYPE(P5)>::Id
    };

    _setPrototype(TypeId<ASMJIT_TYPE_TO_TYPE(RET)>::Id, arguments, ASMJIT_ARRAY_SIZE(arguments));
  }
};

//! @brief Class used to build function with 7 arguments.
template<typename RET, typename P0, typename P1, typename P2, typename P3, typename P4, typename P5, typename P6>
struct FuncBuilder7 : public FuncPrototype
{
  inline FuncBuilder7()
  {
    static const uint32_t arguments[] =
    {
      TypeId<ASMJIT_TYPE_TO_TYPE(P0)>::Id,
      TypeId<ASMJIT_TYPE_TO_TYPE(P1)>::Id,
      TypeId<ASMJIT_TYPE_TO_TYPE(P2)>::Id,
      TypeId<ASMJIT_TYPE_TO_TYPE(P3)>::Id,
      TypeId<ASMJIT_TYPE_TO_TYPE(P4)>::Id,
      TypeId<ASMJIT_TYPE_TO_TYPE(P5)>::Id,
      TypeId<ASMJIT_TYPE_TO_TYPE(P6)>::Id
    };

    _setPrototype(TypeId<ASMJIT_TYPE_TO_TYPE(RET)>::Id, arguments, ASMJIT_ARRAY_SIZE(arguments));
  }
};

//! @brief Class used to build function with 8 arguments.
template<typename RET, typename P0, typename P1, typename P2, typename P3, typename P4, typename P5, typename P6, typename P7>
struct FuncBuilder8 : public FuncPrototype
{
  inline FuncBuilder8()
  {
    static const uint32_t arguments[] =
    {
      TypeId<ASMJIT_TYPE_TO_TYPE(P0)>::Id,
      TypeId<ASMJIT_TYPE_TO_TYPE(P1)>::Id,
      TypeId<ASMJIT_TYPE_TO_TYPE(P2)>::Id,
      TypeId<ASMJIT_TYPE_TO_TYPE(P3)>::Id,
      TypeId<ASMJIT_TYPE_TO_TYPE(P4)>::Id,
      TypeId<ASMJIT_TYPE_TO_TYPE(P5)>::Id,
      TypeId<ASMJIT_TYPE_TO_TYPE(P6)>::Id,
      TypeId<ASMJIT_TYPE_TO_TYPE(P7)>::Id
    };

    _setPrototype(TypeId<ASMJIT_TYPE_TO_TYPE(RET)>::Id, arguments, ASMJIT_ARRAY_SIZE(arguments));
  }
};

//! @brief Class used to build function with 9 arguments.
template<typename RET, typename P0, typename P1, typename P2, typename P3, typename P4, typename P5, typename P6, typename P7, typename P8>
struct FuncBuilder9 : public FuncPrototype
{
  inline FuncBuilder9()
  {
    static const uint32_t arguments[] =
    {
      TypeId<ASMJIT_TYPE_TO_TYPE(P0)>::Id,
      TypeId<ASMJIT_TYPE_TO_TYPE(P1)>::Id,
      TypeId<ASMJIT_TYPE_TO_TYPE(P2)>::Id,
      TypeId<ASMJIT_TYPE_TO_TYPE(P3)>::Id,
      TypeId<ASMJIT_TYPE_TO_TYPE(P4)>::Id,
      TypeId<ASMJIT_TYPE_TO_TYPE(P5)>::Id,
      TypeId<ASMJIT_TYPE_TO_TYPE(P6)>::Id,
      TypeId<ASMJIT_TYPE_TO_TYPE(P7)>::Id,
      TypeId<ASMJIT_TYPE_TO_TYPE(P8)>::Id
    };

    _setPrototype(TypeId<ASMJIT_TYPE_TO_TYPE(RET)>::Id, arguments, ASMJIT_ARRAY_SIZE(arguments));
  }
};

//! @brief Class used to build function with 10 arguments.
template<typename RET, typename P0, typename P1, typename P2, typename P3, typename P4, typename P5, typename P6, typename P7, typename P8, typename P9>
struct FuncBuilder10 : public FuncPrototype
{
  inline FuncBuilder10()
  {
    static const uint32_t arguments[] =
    {
      TypeId<ASMJIT_TYPE_TO_TYPE(P0)>::Id,
      TypeId<ASMJIT_TYPE_TO_TYPE(P1)>::Id,
      TypeId<ASMJIT_TYPE_TO_TYPE(P2)>::Id,
      TypeId<ASMJIT_TYPE_TO_TYPE(P3)>::Id,
      TypeId<ASMJIT_TYPE_TO_TYPE(P4)>::Id,
      TypeId<ASMJIT_TYPE_TO_TYPE(P5)>::Id,
      TypeId<ASMJIT_TYPE_TO_TYPE(P6)>::Id,
      TypeId<ASMJIT_TYPE_TO_TYPE(P7)>::Id,
      TypeId<ASMJIT_TYPE_TO_TYPE(P8)>::Id,
      TypeId<ASMJIT_TYPE_TO_TYPE(P9)>::Id
    };

    _setPrototype(TypeId<ASMJIT_TYPE_TO_TYPE(RET)>::Id, arguments, ASMJIT_ARRAY_SIZE(arguments));
  }
};

} // AsmJit namespace 

// [Api-End]
#include "../core/apiend.h"

// [Guard]
#endif // _ASMJIT_CORE_FUNC_H
