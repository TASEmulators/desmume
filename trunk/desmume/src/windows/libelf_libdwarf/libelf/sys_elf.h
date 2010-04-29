/*
 * lib/sys_elf.h.w32 - internal configuration file for W32 port
 * Copyright (C) 2004 - 2006 Michael Riepe
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * @(#) $Id: sys_elf.h.w32,v 1.2 2006/09/07 15:55:42 michael Exp $
 */

/*
 * DO NOT USE THIS IN APPLICATIONS - #include <libelf.h> INSTEAD!
 */

/* Define to `<elf.h>' or `<sys/elf.h>' if one of them is present */
#undef __LIBELF_HEADER_ELF_H

/* Define if Elf32_Dyn is declared in <link.h> */
#undef __LIBELF_NEED_LINK_H

/* Define if Elf32_Dyn is declared in <sys/link.h> */
#undef __LIBELF_NEED_SYS_LINK_H

/* Define if you want 64-bit support (and your system supports it) */
#define __LIBELF64 1

/* Define if you want 64-bit support, and are running IRIX */
#undef __LIBELF64_IRIX

/* Define if you want 64-bit support, and are running Linux */
#undef __LIBELF64_LINUX

/* Define if you want symbol versioning (and your system supports it) */
#define __LIBELF_SYMBOL_VERSIONS 1

/* Define to a 64-bit signed integer type if one exists */
#define __libelf_i64_t __int64

/* Define to a 64-bit unsigned integer type if one exists */
#define __libelf_u64_t unsigned __int64

/* Define to a 32-bit signed integer type if one exists */
#define __libelf_i32_t int

/* Define to a 32-bit unsigned integer type if one exists */
#define __libelf_u32_t unsigned int

/* Define to a 16-bit signed integer type if one exists */
#define __libelf_i16_t short int

/* Define to a 16-bit unsigned integer type if one exists */
#define __libelf_u16_t unsigned short int

/*
 * Ok, now get the correct instance of elf.h...
 */
#ifdef __LIBELF_HEADER_ELF_H
# include __LIBELF_HEADER_ELF_H
#else /* __LIBELF_HEADER_ELF_H */
# if __LIBELF_INTERNAL__
#  include <elf_repl.h>
# else /* __LIBELF_INTERNAL__ */
#  include <libelf/elf_repl.h>
# endif /* __LIBELF_INTERNAL__ */
#endif /* __LIBELF_HEADER_ELF_H */

/*
 * On some systems, <elf.h> is severely broken.  Try to fix it.
 */
#ifdef __LIBELF_HEADER_ELF_H

# ifndef ELF32_FSZ_ADDR
#  define ELF32_FSZ_ADDR	4
#  define ELF32_FSZ_HALF	2
#  define ELF32_FSZ_OFF		4
#  define ELF32_FSZ_SWORD	4
#  define ELF32_FSZ_WORD	4
# endif /* ELF32_FSZ_ADDR */

# ifndef STN_UNDEF
#  define STN_UNDEF	0
# endif /* STN_UNDEF */

# if __LIBELF64

#  ifndef ELF64_FSZ_ADDR
#   define ELF64_FSZ_ADDR	8
#   define ELF64_FSZ_HALF	2
#   define ELF64_FSZ_OFF	8
#   define ELF64_FSZ_SWORD	4
#   define ELF64_FSZ_WORD	4
#   define ELF64_FSZ_SXWORD	8
#   define ELF64_FSZ_XWORD	8
#  endif /* ELF64_FSZ_ADDR */

#  ifndef ELF64_ST_BIND
#   define ELF64_ST_BIND(i)	((i)>>4)
#   define ELF64_ST_TYPE(i)	((i)&0xf)
#   define ELF64_ST_INFO(b,t)	(((b)<<4)+((t)&0xf))
#  endif /* ELF64_ST_BIND */

#  ifndef ELF64_R_SYM
#   define ELF64_R_SYM(i)	((Elf64_Xword)(i)>>32)
#   define ELF64_R_TYPE(i)	((i)&0xffffffffL)
#   define ELF64_R_INFO(s,t)	(((Elf64_Xword)(s)<<32)+((t)&0xffffffffL))
#  endif /* ELF64_R_SYM */

#  if __LIBELF64_LINUX
typedef __libelf_u64_t	Elf64_Addr;
typedef __libelf_u16_t	Elf64_Half;
typedef __libelf_u64_t	Elf64_Off;
typedef __libelf_i32_t	Elf64_Sword;
typedef __libelf_u32_t	Elf64_Word;
typedef __libelf_i64_t	Elf64_Sxword;
typedef __libelf_u64_t	Elf64_Xword;
#  endif /* __LIBELF64_LINUX */

# endif /* __LIBELF64 */
#endif /* __LIBELF_HEADER_ELF_H */
