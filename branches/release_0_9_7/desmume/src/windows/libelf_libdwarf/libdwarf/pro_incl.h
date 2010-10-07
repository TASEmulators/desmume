/*

  Copyright (C) 2000,2002,2004 Silicon Graphics, Inc.  All Rights Reserved.
  Portions Copyright 2002-2010 Sun Microsystems, Inc. All rights reserved.
  Portions Copyright 2008-2010 David Anderson. All rights reserved.

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


#ifdef HAVE_ELF_H
#include <elf.h>
#elif defined(HAVE_LIBELF_H) 
/* On one platform without elf.h this gets Elf32_Rel 
   type defined (a required type). */
#include <libelf.h>
#elif defined(HAVE_LIBELF_LIBELF_H)
#include <libelf/libelf.h>
#endif

#if defined(sun)
#include <sys/elf_SPARC.h>
#include <sys/elf_386.h>
#endif

/* The target address is given: the place in the source integer
   is to be determined.
*/
#ifdef WORDS_BIGENDIAN
#define WRITE_UNALIGNED(dbg,dest,source, srclength,len_out) \
    { \
      dbg->de_copy_word(dest, \
                        ((char *)source) +srclength-len_out,  \
			len_out) ; \
    }


#else /* LITTLE ENDIAN */

#define WRITE_UNALIGNED(dbg,dest,source, srclength,len_out) \
    { \
      dbg->de_copy_word( (dest) , \
                        ((char *)source) ,  \
			len_out) ; \
    }
#endif


#if defined(sparc) && defined(sun)
#define REL32 Elf32_Rela
#define REL64 Elf64_Rela
#define REL_SEC_PREFIX ".rela"
#else
#define REL32 Elf32_Rel
#define REL64 Elf64_Rel
#define REL_SEC_PREFIX ".rel"
#endif

#include "dwarf.h"
#include "libdwarf.h"

#include "pro_opaque.h"
#include "pro_error.h"
#include "pro_util.h"
#include "pro_encode_nm.h"
#include "pro_alloc.h"
