/*
byteswap.h - functions and macros for byte swapping.
Copyright (C) 1995 - 2001 Michael Riepe

This library is free software; you can redistribute it and/or
modify it under the terms of the GNU Library General Public
License as published by the Free Software Foundation; either
version 2 of the License, or (at your option) any later version.

This library is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
Library General Public License for more details.

You should have received a copy of the GNU Library General Public
License along with this library; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA
*/

/* @(#) $Id: byteswap.h,v 1.7 2008/05/23 08:15:34 michael Exp $ */

#ifndef _BYTESWAP_H
#define _BYTESWAP_H

#define lu(from,i,s)	(((__libelf_u32_t)((unsigned char*)(from))[i])<<(s))
#define li(from,i,s)	(((__libelf_i32_t)((  signed char*)(from))[i])<<(s))

#define __load_u16L(from)	((__libelf_u32_t) \
    (lu(from,1,8) | lu(from,0,0)))
#define __load_u16M(from)	((__libelf_u32_t) \
    (lu(from,0,8) | lu(from,1,0)))
#define __load_i16L(from)	((__libelf_i32_t) \
    (li(from,1,8) | lu(from,0,0)))
#define __load_i16M(from)	((__libelf_i32_t) \
    (li(from,0,8) | lu(from,1,0)))

#define __load_u32L(from)	((__libelf_u32_t) \
    (lu(from,3,24) | lu(from,2,16) | lu(from,1,8) | lu(from,0,0)))
#define __load_u32M(from)	((__libelf_u32_t) \
    (lu(from,0,24) | lu(from,1,16) | lu(from,2,8) | lu(from,3,0)))
#define __load_i32L(from)	((__libelf_i32_t) \
    (li(from,3,24) | lu(from,2,16) | lu(from,1,8) | lu(from,0,0)))
#define __load_i32M(from)	((__libelf_i32_t) \
    (li(from,0,24) | lu(from,1,16) | lu(from,2,8) | lu(from,3,0)))

#define su(to,i,v,s)	(((char*)(to))[i]=((__libelf_u32_t)(v)>>(s)))
#define si(to,i,v,s)	(((char*)(to))[i]=((__libelf_i32_t)(v)>>(s)))

#define __store_u16L(to,v)	\
    (su(to,1,v,8), su(to,0,v,0))
#define __store_u16M(to,v)	\
    (su(to,0,v,8), su(to,1,v,0))
#define __store_i16L(to,v)	\
    (si(to,1,v,8), si(to,0,v,0))
#define __store_i16M(to,v)	\
    (si(to,0,v,8), si(to,1,v,0))

#define __store_u32L(to,v)	\
    (su(to,3,v,24), su(to,2,v,16), su(to,1,v,8), su(to,0,v,0))
#define __store_u32M(to,v)	\
    (su(to,0,v,24), su(to,1,v,16), su(to,2,v,8), su(to,3,v,0))
#define __store_i32L(to,v)	\
    (si(to,3,v,24), si(to,2,v,16), si(to,1,v,8), si(to,0,v,0))
#define __store_i32M(to,v)	\
    (si(to,0,v,24), si(to,1,v,16), si(to,2,v,8), si(to,3,v,0))

#if __LIBELF64

/*
 * conversion functions from swap64.c
 */
extern __libelf_u64_t _elf_load_u64L(const unsigned char *from);
extern __libelf_u64_t _elf_load_u64M(const unsigned char *from);
extern __libelf_i64_t _elf_load_i64L(const unsigned char *from);
extern __libelf_i64_t _elf_load_i64M(const unsigned char *from);
extern void _elf_store_u64L(unsigned char *to, __libelf_u64_t v);
extern void _elf_store_u64M(unsigned char *to, __libelf_u64_t v);
extern void _elf_store_i64L(unsigned char *to, __libelf_u64_t v);
extern void _elf_store_i64M(unsigned char *to, __libelf_u64_t v);

/*
 * aliases for existing conversion code
 */
#define __load_u64L	_elf_load_u64L
#define __load_u64M	_elf_load_u64M
#define __load_i64L	_elf_load_i64L
#define __load_i64M	_elf_load_i64M
#define __store_u64L	_elf_store_u64L
#define __store_u64M	_elf_store_u64M
#define __store_i64L	_elf_store_i64L
#define __store_i64M	_elf_store_i64M

#endif /* __LIBELF64 */

#endif /* _BYTESWAP_H */
