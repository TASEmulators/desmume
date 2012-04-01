#ifndef ___PFC_H___
#define ___PFC_H___

#ifdef WIN32
#ifndef STRICT
#define STRICT
#endif
#include <windows.h>
#endif

#ifndef _MSC_VER
typedef long long __int64;
#endif

#ifndef QWORD
typedef unsigned __int64 QWORD;
#endif

#ifndef DWORD
typedef unsigned long DWORD;
#endif

#ifndef WORD
typedef unsigned short WORD;
#endif

#ifndef BYTE
typedef unsigned char BYTE;
#endif

#define PFC_ALLOCA_LIMIT (4096)

#define INDEX_INVALID ((unsigned)(-1))

#include <malloc.h>

#include <tchar.h>
#include <stdio.h>

#include <assert.h>

#include <math.h>
#include <float.h>

#ifdef _MSC_VER

#define NOVTABLE _declspec(novtable)

#ifdef _DEBUG
#define ASSUME(X) assert(X)
#else
#define ASSUME(X) __assume(X)
#endif

#else

#define NOVTABLE

#define ASSUME(X) assert(X)

#endif


#define tabsize(x) (sizeof(x)/sizeof(*x))

#include "bit_array.h"
//#include "critsec.h"
#include "mem_block.h"
#include "list.h"
#include "ptr_list.h"
#include "string.h"
#include "profiler.h"
#include "cfg_var.h"
#include "cfg_memblock.h"
#include "guid.h"
#include "byte_order_helper.h"
#include "other.h"
#include "chainlist.h"
#include "array.h"
#endif //___PFC_H___