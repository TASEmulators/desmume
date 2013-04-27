/*
 * gelf.h - public header file for libelf.
 * Copyright (C) 2000 - 2006 Michael Riepe
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA
 */

/* @(#) $Id: gelf.h,v 1.16 2008/05/23 08:15:34 michael Exp $ */

#ifndef _GELF_H
#define _GELF_H

#if __LIBELF_INTERNAL__
#include <libelf.h>
#else /* __LIBELF_INTERNAL__ */
#include <libelf/libelf.h>
#endif /* __LIBELF_INTERNAL__ */

#if __LIBELF_NEED_LINK_H
#include <link.h>
#elif __LIBELF_NEED_SYS_LINK_H
#include <sys/link.h>
#endif /* __LIBELF_NEED_LINK_H */

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#ifndef __P
# if (__STDC__ + 0) || defined(__cplusplus) || defined(_WIN32)
#  define __P(args) args
# else /* __STDC__ || defined(__cplusplus) */
#  define __P(args) ()
# endif /* __STDC__ || defined(__cplusplus) */
#endif /* __P */

#if !__LIBELF64

#error "GElf is not supported on this system."

#else /* __LIBELF64 */

typedef Elf64_Addr	GElf_Addr;
typedef Elf64_Half	GElf_Half;
typedef Elf64_Off	GElf_Off;
typedef Elf64_Sword	GElf_Sword;
typedef Elf64_Word	GElf_Word;
typedef Elf64_Sxword	GElf_Sxword;
typedef Elf64_Xword	GElf_Xword;

typedef Elf64_Ehdr	GElf_Ehdr;
typedef Elf64_Phdr	GElf_Phdr;
typedef Elf64_Shdr	GElf_Shdr;
typedef Elf64_Dyn	GElf_Dyn;
typedef Elf64_Rel	GElf_Rel;
typedef Elf64_Rela	GElf_Rela;
typedef Elf64_Sym	GElf_Sym;

/*
 * Symbol versioning
 */
#if __LIBELF_SYMBOL_VERSIONS
typedef Elf64_Verdef	GElf_Verdef;
typedef Elf64_Verneed	GElf_Verneed;
typedef Elf64_Verdaux	GElf_Verdaux;
typedef Elf64_Vernaux	GElf_Vernaux;
#endif /* __LIBELF_SYMBOL_VERSIONS */

/*
 * These types aren't implemented (yet)
 *
typedef Elf64_Move    GElf_Move;
typedef Elf64_Syminfo GElf_Syminfo;
 */

/*
 * Generic macros
 */
#define GELF_ST_BIND	ELF64_ST_BIND
#define GELF_ST_TYPE	ELF64_ST_TYPE
#define GELF_ST_INFO	ELF64_ST_INFO

#define GELF_R_TYPE	ELF64_R_TYPE
#define GELF_R_SYM	ELF64_R_SYM
#define GELF_R_INFO	ELF64_R_INFO

/*
 * Function declarations
 */
extern int             gelf_getclass __P((Elf *__elf));

extern size_t             gelf_fsize __P((Elf *__elf, Elf_Type __type, size_t __count, unsigned __ver));

extern Elf_Data       *gelf_xlatetof __P((Elf *__elf, Elf_Data *__dst, const Elf_Data *__src, unsigned __encode));
extern Elf_Data       *gelf_xlatetom __P((Elf *__elf, Elf_Data *__dst, const Elf_Data *__src, unsigned __encode));

extern GElf_Ehdr       *gelf_getehdr __P((Elf *__elf, GElf_Ehdr *__dst));
extern int          gelf_update_ehdr __P((Elf *__elf, GElf_Ehdr *__src));
extern unsigned long    gelf_newehdr __P((Elf *__elf, int __elfclass));

extern GElf_Phdr       *gelf_getphdr __P((Elf *__elf, int ndx, GElf_Phdr *__dst));
extern int          gelf_update_phdr __P((Elf *__elf, int ndx, GElf_Phdr *__src));
extern unsigned long    gelf_newphdr __P((Elf *__elf, size_t __phnum));

extern GElf_Shdr       *gelf_getshdr __P((Elf_Scn *__scn, GElf_Shdr *__dst));
extern int          gelf_update_shdr __P((Elf_Scn *__scn, GElf_Shdr *__src));

extern GElf_Dyn         *gelf_getdyn __P((Elf_Data *__src, int __ndx, GElf_Dyn *__dst));
extern int           gelf_update_dyn __P((Elf_Data *__dst, int __ndx, GElf_Dyn *__src));

extern GElf_Rel         *gelf_getrel __P((Elf_Data *__src, int __ndx, GElf_Rel *__dst));
extern int           gelf_update_rel __P((Elf_Data *__dst, int __ndx, GElf_Rel *__src));

extern GElf_Rela       *gelf_getrela __P((Elf_Data *__src, int __ndx, GElf_Rela *__dst));
extern int          gelf_update_rela __P((Elf_Data *__dst, int __ndx, GElf_Rela *__src));

extern GElf_Sym         *gelf_getsym __P((Elf_Data *__src, int __ndx, GElf_Sym *__dst));
extern int           gelf_update_sym __P((Elf_Data *__dst, int __ndx, GElf_Sym *__src));

extern long            gelf_checksum __P((Elf *__elf));

/*
 * These functions aren't implemented (yet)
 *
extern GElf_Move       *gelf_getmove __P((Elf_Data *__src, int __ndx, GElf_Move *__src));
extern int          gelf_update_move __P((Elf_Data *__dst, int __ndx, GElf_Move *__src));
 *
extern GElf_Syminfo* gelf_getsyminfo __P((Elf_Data *__src, int __ndx, GElf_Syminfo *__dst));
extern int       gelf_update_syminfo __P((Elf_Data *__dst, int __ndx, GElf_Syminfo *__src));
 */

/*
 * Extensions (not available in other versions of libelf)
 */
extern size_t             gelf_msize __P((Elf *__elf, Elf_Type __type, size_t __count, unsigned __ver));

#endif /* __LIBELF64 */

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* _GELF_H */
