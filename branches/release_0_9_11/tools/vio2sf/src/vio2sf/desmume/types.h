/*  Copyright (C) 2005 Guillaume Duhamel

    This file is part of DeSmuME

    DeSmuME is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    DeSmuME is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with DeSmuME; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

#ifndef TYPES_HPP
#define TYPES_HPP

#ifndef FASTCALL
#ifdef __MINGW32__
#define FASTCALL __attribute__((fastcall))
#elif defined (__i386__)
#define FASTCALL __attribute__((regparm(3)))
#else
#define FASTCALL
#endif
#endif

#ifndef INLINE
#ifdef _MSC_VER
#define INLINE _inline
#else
#define INLINE inline
#endif
#endif

#ifdef DESMUME_COCOA
#ifdef __BIG_ENDIAN__
#define WORDS_BIGENDIAN
#endif
#endif

#if defined(__LP64__)
typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned int u32;
typedef unsigned long u64;
typedef unsigned long pointer;

typedef signed char s8;
typedef signed short s16;
typedef signed int s32;
typedef signed long s64;
#else
typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned long u32;
#ifdef _MSC_VER
typedef unsigned __int64 u64;
#else
typedef unsigned long long u64;
#endif
typedef unsigned long pointer;

typedef signed char s8;
typedef signed short s16;
typedef signed long s32;
#ifdef _MSC_VER
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

/*---------- GPU3D fixed-points types -----------*/

typedef s32 f32;
#define inttof32(n)          ((n) << 12)
#define f32toint(n)          ((n) >> 12)
#define floattof32(n)        ((int32)((n) * (1 << 12)))
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

#ifdef __GNUC__
#define PACKED __attribute__((packed))
#endif

#ifdef WORDS_BIGENDIAN
# define LOCAL_BE
#else
# define LOCAL_LE
#endif

/* little endian (ds' endianess) to local endianess convert macros */
#ifdef LOCAL_BE	/* local arch is big endian */
# define LE_TO_LOCAL_16(x) ((((x)&0xff)<<8)|(((x)>>8)&0xff))
# define LE_TO_LOCAL_32(x) ((((x)&0xff)<<24)|(((x)&0xff00)<<8)|(((x)>>8)&0xff00)|(((x)>>24)&0xff))
# define LOCAL_TO_LE_16(x) ((((x)&0xff)<<8)|(((x)>>8)&0xff))
# define LOCAL_TO_LE_32(x) ((((x)&0xff)<<24)|(((x)&0xff00)<<8)|(((x)>>8)&0xff00)|(((x)>>24)&0xff))
#else		/* local arch is little endian */
# define LE_TO_LOCAL_16(x) (x)
# define LE_TO_LOCAL_32(x) (x)
# define LOCAL_TO_LE_16(x) (x)
# define LOCAL_TO_LE_32(x) (x)
#endif

/* kilobytes and megabytes macro */
#define MB(x) ((x)*1024*1024)
#define KB(x) ((x)*1024)

#define CPU_STR(c) ((c==ARM9)?"ARM9":"ARM7")
typedef enum
{
	ARM9 = 0,
	ARM7 = 1
} cpu_id_t;

#define __PACKED __attribute__((__packed__))

#endif
