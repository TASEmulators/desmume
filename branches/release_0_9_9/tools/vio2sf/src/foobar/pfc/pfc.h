#ifndef ___PFC_H___
#define ___PFC_H___

#if !defined(_WINDOWS) && (defined(WIN32) || defined(_WIN32) || defined(WIN64) || defined(_WIN64) || defined(_WIN32_WCE))
#define _WINDOWS
#endif


#define PFC_DLL_EXPORT

#ifdef _WINDOWS

#ifndef STRICT
#define STRICT
#endif

#ifndef _SYS_GUID_OPERATOR_EQ_
#define _NO_SYS_GUID_OPERATOR_EQ_	//fix retarded warning with operator== on GUID returning int
#endif

#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x500
#endif

#include <windows.h>

#ifndef _SYS_GUID_OPERATOR_EQ_
__inline bool __InlineIsEqualGUID(REFGUID rguid1, REFGUID rguid2)
{
   return (
      ((unsigned long *) &rguid1)[0] == ((unsigned long *) &rguid2)[0] &&
      ((unsigned long *) &rguid1)[1] == ((unsigned long *) &rguid2)[1] &&
      ((unsigned long *) &rguid1)[2] == ((unsigned long *) &rguid2)[2] &&
      ((unsigned long *) &rguid1)[3] == ((unsigned long *) &rguid2)[3]);
}

inline bool operator==(REFGUID guidOne, REFGUID guidOther) {return __InlineIsEqualGUID(guidOne,guidOther);}
inline bool operator!=(REFGUID guidOne, REFGUID guidOther) {return !__InlineIsEqualGUID(guidOne,guidOther);}
#endif

#include <tchar.h>

#elif defined(__GNUC__) && (defined __unix__ || defined __POSIX__)
#include <stdint.h>
#include <memory.h>
typedef struct {
        uint32_t Data1;
        uint16_t Data2;
        uint16_t Data3;
        uint8_t  Data4[ 8 ];
    } GUID; //same as win32 GUID

inline bool operator==(const GUID & p_item1,const GUID & p_item2) {
	return memcmp(&p_item1,&p_item2,sizeof(GUID)) == 0;
}

inline bool operator!=(const GUID & p_item1,const GUID & p_item2) {
	return memcmp(&p_item1,&p_item2,sizeof(GUID)) != 0;
}

#else

#error Only win32 or unix target supported.

#endif



#define PFC_MEMORY_SPACE_LIMIT ((t_uint64)1<<(sizeof(void*)*8-1))

#define PFC_ALLOCA_LIMIT (4096)

#define INDEX_INVALID ((unsigned)(-1))


#include <exception>
#include <stdexcept>
#include <new>

#include <malloc.h>

#include <stdio.h>

#include <assert.h>

#include <math.h>
#include <float.h>

#define _PFC_WIDESTRING(_String) L ## _String
#define PFC_WIDESTRING(_String) _PFC_WIDESTRING(_String)

#ifndef _DEBUG
#define PFC_ASSERT(_Expression)     ((void)0)
#define PFC_ASSERT_SUCCESS(_Expression) (void)( (_Expression), 0)
#define PFC_ASSERT_NO_EXCEPTION(_Expression) { _Expression; }
#else

#ifdef _WIN32
namespace pfc { void myassert(const wchar_t * _Message, const wchar_t *_File, unsigned _Line); }
#define PFC_ASSERT(_Expression) (void)( (!!(_Expression)) || (pfc::myassert(PFC_WIDESTRING(#_Expression), PFC_WIDESTRING(__FILE__), __LINE__), 0) )
#define PFC_ASSERT_SUCCESS(_Expression) PFC_ASSERT(_Expression)
#else
#define PFC_ASSERT(_Expression) assert(_Expression)
#define PFC_ASSERT_SUCCESS(_Expression) (void)( (_Expression), 0) //FIXME
#endif

#define PFC_ASSERT_NO_EXCEPTION(_Expression) { try { _Expression; } catch(...) { PFC_ASSERT(!"Should not get here - unexpected exception"); } }
#endif

#ifdef _MSC_VER

#ifdef _DEBUG
#define NOVTABLE
#else
#define NOVTABLE _declspec(novtable)
#endif

#ifdef _DEBUG
#define ASSUME(X) PFC_ASSERT(X)
#else
#define ASSUME(X) __assume(X)
#endif

#define PFC_DEPRECATE(X) __declspec(deprecated(X))
#else

#define NOVTABLE
#define ASSUME(X) assert(X)
#define PFC_DEPRECATE(X)

#endif

#include "int_types.h"
#include "traits.h"
#include "bit_array.h"
#include "primitives.h"
#include "alloc.h"
#include "array.h"
#include "bit_array_impl.h"
#include "binary_search.h"
#include "bsearch_inline.h"
#include "bsearch.h"
#include "sort.h"
#include "order_helper.h"
#include "list.h"
#include "ptr_list.h"
#include "string.h"
#include "string_list.h"
#include "ref_counter.h"
#include "iterators.h"
#include "avltree.h"
#include "map.h"
#include "profiler.h"
#include "guid.h"
#include "byte_order_helper.h"
#include "other.h"
#include "chain_list_v2.h"
#include "rcptr.h"
#include "com_ptr_t.h"
#include "string_conv.h"
#include "stringNew.h"
#include "pathUtils.h"
#include "instance_tracker.h"
#include "threads.h"

#endif //___PFC_H___
