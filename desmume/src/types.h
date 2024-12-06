/*
	Copyright (C) 2005 Guillaume Duhamel
	Copyright (C) 2008-2023 DeSmuME team

	This file is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 2 of the License, or
	(at your option) any later version.

	This file is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with the this software.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef TYPES_HPP
#define TYPES_HPP

#include <retro_miscellaneous.h>
#include <retro_inline.h>
#include <math/fxp.h>

#ifdef __APPLE__
	#include <AvailabilityMacros.h>
#endif

//analyze microsoft compilers
#ifdef _MSC_VER
	#define HOST_WINDOWS
#endif //_MSC_VER

// Determine CPU architecture for platforms that don't use the autoconf script
#if defined(HOST_WINDOWS) || defined(DESMUME_COCOA)
	#if defined(__x86_64__) || defined(__LP64) || defined(__IA64__) || defined(_M_X64) || defined(_WIN64) || defined(__aarch64__) || defined(_M_ARM64) || defined(__ppc64__)
		#define HOST_64
	#else
		#define HOST_32
	#endif
#endif

//enforce a constraint: gdb stub requires developer
#if defined(GDB_STUB) && !defined(DEVELOPER)
	#define DEVELOPER
#endif

#ifdef DEVELOPER
	#define IF_DEVELOPER(X) X
#else
	#define IF_DEVELOPER(X)
#endif

#ifdef __GNUC__
// Our AltiVec code assumes that its being run on a big-endian system. While
// the ppcle and ppc64le architectures do exist, our AltiVec code does not
// support little-endian right now.
	#if defined(__ALTIVEC__) && defined(MSB_FIRST) && (MSB_FIRST > 0)
		#define ENABLE_ALTIVEC
	#endif

// For now, we'll be starting off with only using NEON-A64 for easier testing
// and development. If the development for A64 goes well and if an A32 backport
// is discovered to be feasible, then we may explore backporting the NEON code
// to A32 at a later date.
	#if (defined(__ARM_NEON__) || defined(__ARM_NEON)) && (defined(__aarch64__) || defined(_M_ARM64))
		#define ENABLE_NEON_A64
	#endif

	#ifdef __SSE__
		#define ENABLE_SSE
	#endif

	#ifdef __SSE2__
		#define ENABLE_SSE2
	#endif

	#ifdef __SSE3__
		#define ENABLE_SSE3
	#endif

	#ifdef __SSSE3__
		#define ENABLE_SSSE3
	#endif

	#ifdef __SSE4_1__
		#define ENABLE_SSE4_1
	#endif

	#ifdef __SSE4_2__
		#define ENABLE_SSE4_2
	#endif

	#ifdef __AVX__
		#define ENABLE_AVX
	#endif

	#ifdef __AVX2__
		#define ENABLE_AVX2
	#endif

	// AVX-512 is special because it has multiple tiers of support.
	//
	// For our case, Tier-0 will be the baseline AVX-512 tier that includes the basic Foundation and
	// Conflict Detection extensions, which should be supported on all AVX-512 CPUs. Higher tiers
	// include more extensions, where each higher tier also assumes support for all lower tiers.
	//
	// For typical use cases in DeSmuME, the most practical AVX-512 tier will be Tier-1.
	#if defined(__AVX512F__) && defined(__AVX512CD__)
		#define ENABLE_AVX512_0
	#endif

	#if defined(ENABLE_AVX512_0) && defined(__AVX512BW__) && defined(__AVX512DQ__) && !defined(FORCE_AVX512_0)
		#define ENABLE_AVX512_1
	#endif

	#if defined(ENABLE_AVX512_1) && defined(__AVX512IFMA__) && defined(__AVX512VBMI__)
		#define ENABLE_AVX512_2
	#endif

	#if defined(ENABLE_AVX512_2) && defined(__AVX512VNNI__) && defined(__AVX512VBMI2__) && defined(__AVX512BITALG__)
		#define ENABLE_AVX512_3
	#endif
#endif

#if defined(_MSC_VER) || defined(__MINGW32__)
	#include <compat/msvc.h>

#else
	#define WINAPI
#endif

#if !defined(MAX_PATH)
	#if defined(HOST_WINDOWS)
		#define MAX_PATH 260
	#elif defined(__GNUC__)
		#include <limits.h>
		#if !defined(PATH_MAX)
			#define MAX_PATH 1024
		#else
			#define MAX_PATH PATH_MAX
		#endif
	#else
		#define MAX_PATH 1024
	#endif
#endif

//------------alignment macros-------------
//dont apply these to types without further testing. it only works portably here on declarations of variables
//cant we find a pattern other people use more successfully?
#if _MSC_VER >= 9999 // Was 1900. The way we use DS_ALIGN doesn't jive with how alignas() wants itself to be used, so just use __declspec(align(X)) for now to avoid problems.
	#define DS_ALIGN(X) alignas(X)
#elif defined(_MSC_VER) || defined(__INTEL_COMPILER)
	#define DS_ALIGN(X) __declspec(align(X))
#elif defined(__GNUC__)
	#define DS_ALIGN(X) __attribute__ ((aligned (X)))
#else
	#define DS_ALIGN(X)
#endif

#ifdef HOST_64
	#define CACHE_ALIGN_SIZE 64
#else
	#define CACHE_ALIGN_SIZE 32
#endif

#if defined(__sparc_v9__) || defined(__sparcv9)
	#define PAGE_ALIGN_SIZE 8192 // UltraSPARC architecture uses 8 KB pages.
#else
	#define PAGE_ALIGN_SIZE 4096 // Most architectures use 4 KB pages.
#endif

//use this for example when you want a byte value to be better-aligned
#define CACHE_ALIGN DS_ALIGN(CACHE_ALIGN_SIZE)
#define PAGE_ALIGN DS_ALIGN(PAGE_ALIGN_SIZE)
#define FAST_ALIGN DS_ALIGN(4)
//---------------------------------------------

#if defined (__i386__) && !defined(__clang__)
	#define DESMUME_FASTCALL __attribute__((regparm(3)))
	#define ASMJIT_CALL_CONV kX86FuncConvGccRegParm3
#elif defined(_MSC_VER) || defined(__INTEL_COMPILER)
	#define DESMUME_FASTCALL
	#define ASMJIT_CALL_CONV kX86FuncConvDefault
#else
	#define DESMUME_FASTCALL
	#define ASMJIT_CALL_CONV kX86FuncConvDefault
#endif

#ifdef _MSC_VER
	#define _CDECL_ __cdecl
#else
	#define _CDECL_
#endif

#ifndef FORCEINLINE
	#if defined(_MSC_VER) || defined(__INTEL_COMPILER)
		#define FORCEINLINE __forceinline
		#define MSC_FORCEINLINE __forceinline
	#else
		#define FORCEINLINE inline __attribute__((always_inline))
		#define MSC_FORCEINLINE
	#endif
#endif

#ifndef NOINLINE
	#ifdef __GNUC__
		#define NOINLINE __attribute__((noinline))
	#else
		#define NOINLINE
	#endif
#endif

#ifndef LOOPVECTORIZE_DISABLE
	#if defined(_MSC_VER)
		#if _MSC_VER >= 1700
			#define LOOPVECTORIZE_DISABLE loop(no_vector)
		#else
			#define LOOPVECTORIZE_DISABLE 
		#endif
	#elif defined(__clang__)
		#define LOOPVECTORIZE_DISABLE clang loop vectorize(disable)
	#else
		#define LOOPVECTORIZE_DISABLE
	#endif
#endif

#if defined(__LP64__)
typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned int u32;
typedef unsigned long long u64;

typedef signed char s8;
typedef signed short s16;
typedef signed int s32;
typedef signed long long s64;
#else
typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned int u32;
#if defined(_MSC_VER) || defined(__INTEL_COMPILER)
typedef unsigned __int64 u64;
#else
typedef unsigned long long u64;
#endif

typedef signed char s8;
typedef signed short s16;
typedef signed int s32;
#if defined(_MSC_VER) || defined(__INTEL_COMPILER)
typedef __int64 s64;
#else
typedef signed long long s64;
#endif
#endif

typedef u8  uint8;
typedef u16 uint16;

#ifndef OBJ_C
typedef u32 uint32;
#else
#define uint32 u32 //uint32 is defined in Leopard somewhere, avoid conflicts
#endif

#ifdef ENABLE_ALTIVEC
	#ifndef __APPLE_ALTIVEC__
		#include <altivec.h>
	#endif
typedef __vector unsigned char v128u8;
typedef __vector signed char v128s8;
typedef __vector unsigned short v128u16;
typedef __vector signed short v128s16;
typedef __vector unsigned int v128u32;
typedef __vector signed int v128s32;
typedef __vector float v128f32;

#define AVAILABLE_TYPE_v128u8
#define AVAILABLE_TYPE_v128s8
#define AVAILABLE_TYPE_v128u16
#define AVAILABLE_TYPE_v128s16
#define AVAILABLE_TYPE_v128u32
#define AVAILABLE_TYPE_v128s32
#define AVAILABLE_TYPE_v128f32
#endif

#ifdef ENABLE_NEON_A64
#include <arm_neon.h>
typedef uint8x16_t v128u8;
typedef int8x16_t v128s8;
typedef uint16x8_t v128u16;
typedef int16x8_t v128s16;
typedef uint32x4_t v128u32;
typedef int32x4_t v128s32;
typedef float32x4_t v128f32;

#define AVAILABLE_TYPE_v128u8
#define AVAILABLE_TYPE_v128s8
#define AVAILABLE_TYPE_v128u16
#define AVAILABLE_TYPE_v128s16
#define AVAILABLE_TYPE_v128u32
#define AVAILABLE_TYPE_v128s32
#define AVAILABLE_TYPE_v128f32
#endif

#ifdef ENABLE_SSE
#include <xmmintrin.h>
typedef __m128 v128f32;
#define AVAILABLE_TYPE_v128f32
#endif

#ifdef ENABLE_SSE2
#include <emmintrin.h>
typedef __m128i v128u8;
typedef __m128i v128s8;
typedef __m128i v128u16;
typedef __m128i v128s16;
typedef __m128i v128u32;
typedef __m128i v128s32;

#define AVAILABLE_TYPE_v128u8
#define AVAILABLE_TYPE_v128s8
#define AVAILABLE_TYPE_v128u16
#define AVAILABLE_TYPE_v128s16
#define AVAILABLE_TYPE_v128u32
#define AVAILABLE_TYPE_v128s32
#endif

#if defined(ENABLE_AVX) || defined(ENABLE_AVX2) || defined(ENABLE_AVX512_0)

#include <immintrin.h>
typedef __m256  v256f32;
#define AVAILABLE_TYPE_v256f32

#if defined(ENABLE_AVX2) || defined(ENABLE_AVX512_0)
typedef __m256i v256u8;
typedef __m256i v256s8;
typedef __m256i v256u16;
typedef __m256i v256s16;
typedef __m256i v256u32;
typedef __m256i v256s32;

#define AVAILABLE_TYPE_v256u8
#define AVAILABLE_TYPE_v256s8
#define AVAILABLE_TYPE_v256u16
#define AVAILABLE_TYPE_v256s16
#define AVAILABLE_TYPE_v256u32
#define AVAILABLE_TYPE_v256s32
#endif // defined(ENABLE_AVX2) || defined(ENABLE_AVX512_0)

#if defined(ENABLE_AVX512_0)
typedef __m512i v512u8;
typedef __m512i v512s8;
typedef __m512i v512u16;
typedef __m512i v512s16;
typedef __m512i v512u32;
typedef __m512i v512s32;
typedef __m512  v512f32;

#define AVAILABLE_TYPE_v512u8
#define AVAILABLE_TYPE_v512s8
#define AVAILABLE_TYPE_v512u16
#define AVAILABLE_TYPE_v512s16
#define AVAILABLE_TYPE_v512u32
#define AVAILABLE_TYPE_v512s32
#define AVAILABLE_TYPE_v512f32
#endif // defined(ENABLE_AVX512_0)

#endif // defined(ENABLE_AVX) || defined(ENABLE_AVX2) || defined(ENABLE_AVX512_0)

/*---------- GPU3D fixed-points types -----------*/

typedef s32 f32;
#define inttof32(n)          ((n) << 12)
#define f32toint(n)          ((n) >> 12)
#define floattof32(n)        ((s32)((n) * (1 << 12)))
#define f32tofloat(n)        (((float)(n)) / (float)(1<<12))

typedef s16 t16;
#define f32tot16(n)          ((t16)(n >> 8))
#define inttot16(n)          ((n) << 4)
#define t16toint(n)          ((n) >> 4)
#define floattot16(n)        ((t16)((n) * (1 << 4)))
#define t16ofloat(n)         (((float)(n)) / (float)(1<<4))

typedef s16 v16;
#define inttov16(n)          ((n) << 12)
#define f32tov16(n)          (n)
#define floattov16(n)        ((v16)((n) * (1 << 12)))
#define v16toint(n)          ((n) >> 12)
#define v16tofloat(n)        (((float)(n)) / (float)(1<<12))

typedef s16 v10;
#define inttov10(n)          ((n) << 9)
#define f32tov10(n)          ((v10)(n >> 3))
#define v10toint(n)          ((n) >> 9)
#define floattov10(n)        ((v10)((n) * (1 << 9)))
#define v10tofloat(n)        (((float)(n)) / (float)(1<<9))

union Vector2s16
{
	s16 vec[2];
	s16 coord[2];
	struct { s16 s, t; };
	struct { s16 u, v; };
	struct { s16 x, y; };
	
	u32 value;
};
typedef union Vector2s16 Vector2s16;

union Vector3s16
{
	s16 vec[3];
	s16 coord[3];
	struct { s16 x, y, z; };
};
typedef union Vector3s16 Vector3s16;

union Vector4s16
{
	s16 vec[4];
	s16 coord[4];
	struct { s16 x, y, z, w; };
	
	struct
	{
		Vector3s16 vec3;
		s16 :16;
	};
	
	u64 value;
};
typedef union Vector4s16 Vector4s16;

union Vector2s32
{
	s32 vec[2];
	s32 coord[2];
	struct { s32 s, t; };
	struct { s32 u, v; };
	struct { s32 x, y; };
	
	u64 value;
};
typedef union Vector2s32 Vector2s32;

union Vector3s32
{
	s32 vec[3];
	s32 coord[3];
	struct { s32 x, y, z; };
};
typedef union Vector3s32 Vector3s32;

union Vector4s32
{
	s32 vec[4];
	s32 coord[4];
	struct { s32 x, y, z, w; };
	
	struct
	{
		Vector3s32 vec3;
		s32 :32;
	};
};
typedef union Vector4s32 Vector4s32;

union Vector2s64
{
	s64 vec[2];
	s64 coord[2];
	struct { s64 s, t; };
	struct { s64 u, v; };
	struct { s64 x, y; };
};
typedef union Vector2s64 Vector2s64;

union Vector3s64
{
	s64 vec[3];
	s64 coord[3];
	struct { s64 x, y, z; };
};
typedef union Vector3s64 Vector3s64;

union Vector4s64
{
	s64 vec[4];
	s64 coord[4];
	struct { s64 x, y, z, w; };
	
	struct
	{
		Vector3s64 vec3;
		s64 :64;
	};
};
typedef union Vector4s64 Vector4s64;

union Vector2f32
{
	float vec[2];
	float coord[2];
	struct { float s, t; };
	struct { float u, v; };
	struct { float x, y; };
};
typedef union Vector2f32 Vector2f32;

union Vector3f32
{
	float vec[3];
	float coord[3];
	struct { float x, y, z; };
};
typedef union Vector3f32 Vector3f32;

union Vector4f32
{
	float vec[4];
	float coord[4];
	struct { float x, y, z, w; };
	
	struct
	{
		Vector3f32 vec3;
		float ignore;
	};
};
typedef union Vector4f32 Vector4f32;

union Color4u8
{
	u8 component[4];
	struct { u8 r, g, b, a; };
	
	u32 value;
};
typedef union Color4u8 Color4u8;

union Color3s32
{
	s32 component[3];
	struct { s32 r, g, b; };
};
typedef union Color3s32 Color3s32;

union Color4s32
{
	s32 component[4];
	struct { s32 r, g, b, a; };
	
	struct
	{
		Color3s32 color3;
		s32 alpha;
	};
};
typedef union Color4s32 Color4s32;

union Color3s64
{
	s64 component[3];
	struct { s64 r, g, b; };
};
typedef union Color3s64 Color3s64;

union Color4s64
{
	s64 component[4];
	struct { s64 r, g, b, a; };
	
	struct
	{
		Color3s64 color3;
		s64 alpha;
	};
};
typedef union Color4s64 Color4s64;

union Color3f32
{
	float component[3];
	struct { float r, g, b; };
};
typedef union Color3f32 Color3f32;

union Color4f32
{
	float component[4];
	struct { float r, g, b, a; };
	
	struct
	{
		Color3f32 color3;
		float alpha;
	};
};
typedef union Color4f32 Color4f32;

/*----------------------*/

#ifndef OBJ_C
typedef int BOOL;
#else
//apple also defines BOOL
typedef int desmume_BOOL;
#define BOOL desmume_BOOL
#endif

#ifndef TRUE
#define TRUE 1
#endif

#ifndef FALSE
#define FALSE 0
#endif

// Atomic functions
#if defined(HOST_WINDOWS)
#include <winnt.h>

//#define atomic_add_32(V,M)					InterlockedAddNoFence((volatile LONG *)(V),(LONG)(M))			// Requires Windows 8
inline s32 atomic_add_32(volatile s32 *V, s32 M)						{ return (s32)(InterlockedExchangeAdd((volatile LONG *)V, (LONG)M) + M); }
inline s32 atomic_add_barrier32(volatile s32 *V, s32 M)					{ return (s32)(InterlockedExchangeAdd((volatile LONG *)V, (LONG)M) + M); }

//#define atomic_inc_32(V)						InterlockedIncrementNoFence((volatile LONG *)(V))				// Requires Windows 8
#define atomic_inc_32(V)						_InterlockedIncrement((volatile LONG *)(V))
#define atomic_inc_barrier32(V)					_InterlockedIncrement((volatile LONG *)(V))
//#define atomic_dec_32(V)						InterlockedDecrementNoFence((volatile LONG *)(V))				// Requires Windows 8
#define atomic_dec_32(V)						_InterlockedDecrement((volatile LONG *)(V))
#define atomic_dec_barrier32(V)					_InterlockedDecrement((volatile LONG *)(V))

//#define atomic_or_32(V,M)						InterlockedOrNoFence((volatile LONG *)(V),(LONG)(M))			// Requires Windows 8
#define atomic_or_32(V,M)						_InterlockedOr((volatile LONG *)(V),(LONG)(M))
#define atomic_or_barrier32(V,M)				_InterlockedOr((volatile LONG *)(V),(LONG)(M))
//#define atomic_and_32(V,M)					InterlockedAndNoFence((volatile LONG *)(V),(LONG)(M))			// Requires Windows 8
#define atomic_and_32(V,M)						_InterlockedAnd((volatile LONG *)(V),(LONG)(M))
#define atomic_and_barrier32(V,M)				_InterlockedAnd((volatile LONG *)(V),(LONG)(M))
//#define atomic_xor_32(V,M)					InterlockedXorNoFence((volatile LONG *)(V),(LONG)(M))			// Requires Windows 8
#define atomic_xor_32(V,M)						_InterlockedXor((volatile LONG *)(V),(LONG)(M))
#define atomic_xor_barrier32(V,M)				_InterlockedXor((volatile LONG *)(V),(LONG)(M))

inline bool atomic_test_and_set_32(volatile s32 *V, s32 M)				{ return (_interlockedbittestandset((volatile LONG *)V, (LONG)M)) ? true : false; }
inline bool atomic_test_and_set_barrier32(volatile s32 *V, s32 M)		{ return (_interlockedbittestandset((volatile LONG *)V, (LONG)M)) ? true : false; }
inline bool atomic_test_and_clear_32(volatile s32 *V, s32 M)			{ return (_interlockedbittestandreset((volatile LONG *)V, (LONG)M)) ? true : false; }
inline bool atomic_test_and_clear_barrier32(volatile s32 *V, s32 M)		{ return (_interlockedbittestandreset((volatile LONG *)V, (LONG)M)) ? true : false; }

#elif defined(DESMUME_COCOA) && ( !defined(__clang__) || (__clang_major__ < 9) || !defined(MAC_OS_X_VERSION_10_7) || (MAC_OS_X_VERSION_MIN_REQUIRED < MAC_OS_X_VERSION_10_7) )
#include <libkern/OSAtomic.h>

#define atomic_add_32(V,M)						OSAtomicAdd32((M),(V))
#define atomic_add_barrier32(V,M)				OSAtomicAdd32Barrier((M),(V))

#define atomic_inc_32(V)						OSAtomicIncrement32((V))
#define atomic_inc_barrier32(V)					OSAtomicIncrement32Barrier((V))
#define atomic_dec_32(V)						OSAtomicDecrement32((V))
#define atomic_dec_barrier32(V)					OSAtomicDecrement32Barrier((V))

#define atomic_or_32(V,M)						OSAtomicOr32((M),(volatile uint32_t *)(V))
#define atomic_or_barrier32(V,M)				OSAtomicOr32Barrier((M),(volatile uint32_t *)(V))
#define atomic_and_32(V,M)						OSAtomicAnd32((M),(volatile uint32_t *)(V))
#define atomic_and_barrier32(V,M)				OSAtomicAnd32Barrier((M),(volatile uint32_t *)(V))
#define atomic_xor_32(V,M)						OSAtomicXor32((M),(volatile uint32_t *)(V))
#define atomic_xor_barrier32(V,M)				OSAtomicXor32Barrier((M),(volatile uint32_t *)(V))

#define atomic_test_and_set_32(V,M)				OSAtomicTestAndSet((M),(V))
#define atomic_test_and_set_barrier32(V,M)		OSAtomicTestAndSetBarrier((M),(V))
#define atomic_test_and_clear_32(V,M)			OSAtomicTestAndClear((M),(V))
#define atomic_test_and_clear_barrier32(V,M)	OSAtomicTestAndClearBarrier((M),(V))

#else // Just use C++11 std::atomic
#include <atomic>

inline s32 atomic_add_32(volatile s32 *V, s32 M)			{ return std::atomic_fetch_add_explicit<s32>((volatile std::atomic<s32> *)V, M, std::memory_order::memory_order_relaxed) + M; }
inline s32 atomic_add_barrier32(volatile s32 *V, s32 M)		{ return std::atomic_fetch_add_explicit<s32>((volatile std::atomic<s32> *)V, M, std::memory_order::memory_order_seq_cst) + M; }

inline s32 atomic_inc_32(volatile s32 *V)					{ return atomic_add_32(V, 1); }
inline s32 atomic_inc_barrier32(volatile s32 *V)			{ return atomic_add_barrier32(V, 1); }
inline s32 atomic_dec_32(volatile s32 *V)					{ return atomic_add_32(V, -1); }
inline s32 atomic_dec_barrier32(volatile s32 *V)			{ return atomic_add_barrier32(V, -1); }

inline s32 atomic_or_32(volatile s32 *V, s32 M)				{ return std::atomic_fetch_or_explicit<s32>((volatile std::atomic<s32> *)V, M, std::memory_order::memory_order_relaxed) | M; }
inline s32 atomic_or_barrier32(volatile s32 *V, s32 M)		{ return std::atomic_fetch_or_explicit<s32>((volatile std::atomic<s32> *)V, M, std::memory_order::memory_order_seq_cst) | M; }
inline s32 atomic_and_32(volatile s32 *V, s32 M)			{ return std::atomic_fetch_and_explicit<s32>((volatile std::atomic<s32> *)V, M, std::memory_order::memory_order_relaxed) & M; }
inline s32 atomic_and_barrier32(volatile s32 *V, s32 M)		{ return std::atomic_fetch_and_explicit<s32>((volatile std::atomic<s32> *)V, M, std::memory_order::memory_order_seq_cst) & M; }
inline s32 atomic_xor_32(volatile s32 *V, s32 M)			{ return std::atomic_fetch_xor_explicit<s32>((volatile std::atomic<s32> *)V, M, std::memory_order::memory_order_relaxed) ^ M; }
inline s32 atomic_xor_barrier32(volatile s32 *V, s32 M)		{ return std::atomic_fetch_xor_explicit<s32>((volatile std::atomic<s32> *)V, M, std::memory_order::memory_order_seq_cst) ^ M; }

inline bool atomic_test_and_set_32(volatile s32 *V, s32 M)				{ return (std::atomic_fetch_or_explicit<s32>((volatile std::atomic<s32> *)V,(0x80>>((M)&0x07)), std::memory_order::memory_order_relaxed) & (0x80>>((M)&0x07))) ? true : false; }
inline bool atomic_test_and_set_barrier32(volatile s32 *V, s32 M)		{ return (std::atomic_fetch_or_explicit<s32>((volatile std::atomic<s32> *)V,(0x80>>((M)&0x07)), std::memory_order::memory_order_seq_cst) & (0x80>>((M)&0x07))) ? true : false; }
inline bool atomic_test_and_clear_32(volatile s32 *V, s32 M)			{ return (std::atomic_fetch_and_explicit<s32>((volatile std::atomic<s32> *)V,~(s32)(0x80>>((M)&0x07)), std::memory_order::memory_order_relaxed) & (0x80>>((M)&0x07))) ? true : false; }
inline bool atomic_test_and_clear_barrier32(volatile s32 *V, s32 M)		{ return (std::atomic_fetch_and_explicit<s32>((volatile std::atomic<s32> *)V,~(s32)(0x80>>((M)&0x07)), std::memory_order::memory_order_seq_cst) & (0x80>>((M)&0x07))) ? true : false; }

#endif

// Flags used to determine how a conversion function swaps bytes for big-endian systems.
// These flags should be ignored on little-endian systems.
enum BESwapFlags
{
	BESwapNone    = 0x00, // No byte swapping for both incoming and outgoing data. All data is used as-is.
	
	BESwapIn      = 0x01, // All incoming data is byte swapped; outgoing data is used as-is.
	BESwapPre     = 0x01, // An alternate name for "BESwapIn"
	BESwapSrc     = 0x01, // An alternate name for "BESwapIn"
	
	BESwapOut     = 0x02, // All incoming data is used as-is; outgoing data is byte swapped.
	BESwapPost    = 0x02, // An alternate name for "BESwapOut"
	BESwapDst     = 0x02, // An alternate name for "BESwapOut"
	
	BESwapInOut   = 0x03, // Both incoming data and outgoing data are byte swapped.
	BESwapPrePost = 0x03, // An alternate name for "BESwapInOut"
	BESwapSrcDst  = 0x03  // An alternate name for "BESwapInOut"
};

// little endian (ds' endianess) to local endianess convert macros
#ifdef MSB_FIRST // local arch is big endian
	#define LE_TO_LOCAL_16(x) ((((x)&0xff)<<8)|(((x)>>8)&0xff))
	#define LE_TO_LOCAL_32(x) ((((x)&0xff)<<24)|(((x)&0xff00)<<8)|(((x)>>8)&0xff00)|(((x)>>24)&0xff))
	#define LE_TO_LOCAL_64(x) ((((x)&0xff)<<56)|(((x)&0xff00)<<40)|(((x)&0xff0000)<<24)|(((x)&0xff000000)<<8)|(((x)>>8)&0xff000000)|(((x)>>24)&0xff0000)|(((x)>>40)&0xff00)|(((x)>>56)&0xff))
	#define LE_TO_LOCAL_WORDS_32(x) (((x)<<16)|((x)>>16))
#else // local arch is little endian
	#define LE_TO_LOCAL_16(x) (x)
	#define LE_TO_LOCAL_32(x) (x)
	#define LE_TO_LOCAL_64(x) (x)
	#define LE_TO_LOCAL_WORDS_32(x) (x)
#endif

#define LOCAL_TO_LE_16(x) LE_TO_LOCAL_16(x)
#define LOCAL_TO_LE_32(x) LE_TO_LOCAL_32(x)
#define LOCAL_TO_LE_64(x) LE_TO_LOCAL_64(x)
#define LOCAL_WORDS_TO_LE_32(x) LE_TO_LOCAL_WORDS_32(x)

// kilobytes and megabytes macro
#define MB(x) ((x)*1024*1024)
#define KB(x) ((x)*1024)

//fairly standard for loop macros
#define MACRODO1(TRICK,TODO) { const size_t X = TRICK; TODO; }
#define MACRODO2(X,TODO)   { MACRODO1((X),TODO)   MACRODO1(((X)+1),TODO) }
#define MACRODO4(X,TODO)   { MACRODO2((X),TODO)   MACRODO2(((X)+2),TODO) }
#define MACRODO8(X,TODO)   { MACRODO4((X),TODO)   MACRODO4(((X)+4),TODO) }
#define MACRODO16(X,TODO)  { MACRODO8((X),TODO)   MACRODO8(((X)+8),TODO) }
#define MACRODO32(X,TODO)  { MACRODO16((X),TODO)  MACRODO16(((X)+16),TODO) }
#define MACRODO64(X,TODO)  { MACRODO32((X),TODO)  MACRODO32(((X)+32),TODO) }
#define MACRODO128(X,TODO) { MACRODO64((X),TODO)  MACRODO64(((X)+64),TODO) }
#define MACRODO256(X,TODO) { MACRODO128((X),TODO) MACRODO128(((X)+128),TODO) }

//this one lets you loop any number of times (as long as N<256)
#define MACRODO_N(N,TODO) {\
	if((N)&0x100) MACRODO256(0,TODO); \
	if((N)&0x080) MACRODO128((N)&(0x100),TODO); \
	if((N)&0x040) MACRODO64((N)&(0x100|0x080),TODO); \
	if((N)&0x020) MACRODO32((N)&(0x100|0x080|0x040),TODO); \
	if((N)&0x010) MACRODO16((N)&(0x100|0x080|0x040|0x020),TODO); \
	if((N)&0x008) MACRODO8((N)&(0x100|0x080|0x040|0x020|0x010),TODO); \
	if((N)&0x004) MACRODO4((N)&(0x100|0x080|0x040|0x020|0x010|0x008),TODO); \
	if((N)&0x002) MACRODO2((N)&(0x100|0x080|0x040|0x020|0x010|0x008|0x004),TODO); \
	if((N)&0x001) MACRODO1((N)&(0x100|0x080|0x040|0x020|0x010|0x008|0x004|0x002),TODO); \
}

//---------------------------
//Binary constant generator macro By Tom Torfs - donated to the public domain

//turn a numeric literal into a hex constant
//(avoids problems with leading zeroes)
//8-bit constants max value 0x11111111, always fits in unsigned long
#define HEX__(n) 0x##n##LU

//8-bit conversion function 
#define B8__(x) ((x&0x0000000FLU)?1:0) \
+((x&0x000000F0LU)?2:0) \
+((x&0x00000F00LU)?4:0) \
+((x&0x0000F000LU)?8:0) \
+((x&0x000F0000LU)?16:0) \
+((x&0x00F00000LU)?32:0) \
+((x&0x0F000000LU)?64:0) \
+((x&0xF0000000LU)?128:0)

//for upto 8-bit binary constants
#define B8(d) ((unsigned char)B8__(HEX__(d)))

// for upto 16-bit binary constants, MSB first
#define B16(dmsb,dlsb) (((unsigned short)B8(dmsb)<<8) \
+ B8(dlsb))

// for upto 32-bit binary constants, MSB first */
#define B32(dmsb,db2,db3,dlsb) (((unsigned long)B8(dmsb)<<24) \
+ ((unsigned long)B8(db2)<<16) \
+ ((unsigned long)B8(db3)<<8) \
+ B8(dlsb))

//Sample usage:
//B8(01010101) = 85
//B16(10101010,01010101) = 43605
//B32(10000000,11111111,10101010,01010101) = 2164238933
//---------------------------

#ifndef CTASSERT
#define	CTASSERT(x)		typedef char __assert ## y[(x) ? 1 : -1]
#endif

template<typename T> inline void reconstruct(T* t) { 
	t->~T();
	new(t) T();
}

#endif
