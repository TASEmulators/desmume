/*

  Copyright (C) 2000,2004 Silicon Graphics, Inc.  All Rights Reserved.
  Portions Copyright 2002-2010 Sun Microsystems, Inc. All rights reserved.

  This program is free software; you can redistribute it and/or modify it
  under the terms of version 2.1 of the GNU Lesser General Public License 
  as published by the Free Software Foundation.

  This program is distributed in the hope that it would be useful, but
  WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  

  Further, this software is distributed without any warranty that it is
  free of the rightful claim of any third person regarding infringement 
  or the like.  Any license provided herein, whether implied or 
  otherwise, applies only to this software file.  Patent licenses, if
  any, provided herein do not apply to combinations of this program with 
  other software, or any other product whatsoever.  

  You should have received a copy of the GNU Lesser General Public
  License along with this program; if not, write the Free Software
  Foundation, Inc., 51 Franklin Street - Fifth Floor, Boston MA 02110-1301,
  USA.

  Contact information:  Silicon Graphics, Inc., 1500 Crittenden Lane,
  Mountain View, CA 94043, or:

  http://www.sgi.com

  For further information regarding this notice, see:

  http://oss.sgi.com/projects/GenInfo/NoticeExplan

*/




#define IS_64BIT(dbg) ((dbg)->de_flags & DW_DLC_SIZE_64 ? 1 : 0)
#define ISA_IA64(dbg) ((dbg)->de_flags & DW_DLC_ISA_IA64 ? 1 : 0)

/* definition of sizes of types, given target machine */
#define sizeof_sbyte(dbg) sizeof(Dwarf_Sbyte)
#define sizeof_ubyte(dbg) sizeof(Dwarf_Ubyte)
#define sizeof_uhalf(dbg) sizeof(Dwarf_Half)
/* certain sizes not defined here, but set in dbg record.
   See pro_init.c
*/

/* Computes amount of padding necessary to align n to a k-boundary. */
/* Important: Assumes n, k both GREATER than zero. */
#define PADDING(n, k) ( (k)-1 - ((n)-1)%(k) )

/* The following defines are only important for users of the
** producer part of libdwarf, and such should have these
** defined correctly (as necessary) 
** by the #include <elf.h> done in pro_incl.h
** before the #include "pro_util.h".
** For others producer macros do not matter so 0 is a usable value, and
** zero values let compilation succeed on more non-MIPS architectures.
** A better approach would be welcome.
*/
/* R_MIPS* are #define so #ifndef works */
/* R_IA_64* are not necessarily #define (might be enum) so #ifndef
   is useless, we use the configure script generating 
   HAVE_R_IA_64_DIR32LSB and HAVE_R_IA64_DIR32LSB.
*/
#ifndef R_MIPS_64
#define R_MIPS_64 0
#endif
#ifndef R_MIPS_32
#define R_MIPS_32 0
#endif
#ifndef R_MIPS_SCN_DISP
#define R_MIPS_SCN_DISP 0
#endif

/* R_IA_64_DIR32LSB came before the now-standard R_IA64_DIR32LSB
   (etc) was defined.  This now deals with either form,
   preferring the new form if available.  */
#ifdef HAVE_R_IA64_DIR32LSB
#define DWARF_PRO_R_IA64_DIR32LSB R_IA64_DIR32LSB
#define DWARF_PRO_R_IA64_DIR64LSB R_IA64_DIR64LSB
#define DWARF_PRO_R_IA64_SEGREL64LSB R_IA64_SEGREL64LSB
#define DWARF_PRO_R_IA64_SEGREL32LSB R_IA64_SEGREL32LSB
#endif
#if defined(HAVE_R_IA_64_DIR32LSB) && !defined(HAVE_R_IA64_DIR32LSB)
#define DWARF_PRO_R_IA64_DIR32LSB R_IA_64_DIR32LSB
#define DWARF_PRO_R_IA64_DIR64LSB R_IA_64_DIR64LSB
#define DWARF_PRO_R_IA64_SEGREL64LSB R_IA_64_SEGREL64LSB
#define DWARF_PRO_R_IA64_SEGREL32LSB R_IA_64_SEGREL32LSB
#endif
#if !defined(HAVE_R_IA_64_DIR32LSB) && !defined(HAVE_R_IA64_DIR32LSB)
#define DWARF_PRO_R_IA64_DIR32LSB 0
#define DWARF_PRO_R_IA64_DIR64LSB 0
#define DWARF_PRO_R_IA64_SEGREL64LSB 0
#define DWARF_PRO_R_IA64_SEGREL32LSB 0
#endif

/*
 * The default "I don't know" value can't be zero.
 * Because that's the sentinel value that means "no relocation".
 * In order to use this library in 'symbolic relocation mode we
 * don't care if this value is the right relocation value,
 * only that it's non-NULL.  So at the end, we define it
 * to something sensible.
 */



#if defined(sun)
#if defined(sparc)
#define Get_REL64_isa(dbg)         (R_SPARC_UA64)
#define Get_REL32_isa(dbg)         (R_SPARC_UA32)
#define Get_REL_SEGREL_isa(dbg)    (R_SPARC_NONE) /* I don't know! */
#else  /* i386 */
#define Get_REL64_isa(dbg)         (R_386_32)   /* Any non-zero value is ok */
#define Get_REL32_isa(dbg)         (R_386_32)
#define Get_REL_SEGREL_isa(dbg)    (R_386_NONE) /* I don't know! */
#endif /* sparc || i386 */
#else  /* !sun */
#ifdef HAVE_SYS_IA64_ELF_H
#define Get_REL64_isa(dbg)         (ISA_IA64(dbg) ? \
    DWARF_PRO_R_IA64_DIR64LSB : R_MIPS_64)
#define Get_REL32_isa(dbg)         (ISA_IA64(dbg) ? \
    DWARF_PRO_R_IA64_DIR32LSB : R_MIPS_32)


/* ia64 uses 32bit dwarf offsets for sections */
#define Get_REL_SEGREL_isa(dbg)    (ISA_IA64(dbg) ? \
    DWARF_PRO_R_IA64_SEGREL32LSB : R_MIPS_SCN_DISP)
#else /* HAVE_SYS_IA64_ELF_H */

#if !defined(linux) && !defined(__BEOS__)
#define Get_REL64_isa(dbg)         (R_MIPS_64)
#define Get_REL32_isa(dbg)         (R_MIPS_32)
#define Get_REL_SEGREL_isa(dbg)    (R_MIPS_SCN_DISP)
#else
#define Get_REL64_isa(dbg) (1)
#define Get_REL32_isa(dbg) (1)   /* these are used on linux */
#define Get_REL_SEGREL_isa(dbg) (1)   /* non zero values, see comments above */
#endif

#endif /* HAVE_SYS_IA64_ELF_H */
#endif /* !sun */


