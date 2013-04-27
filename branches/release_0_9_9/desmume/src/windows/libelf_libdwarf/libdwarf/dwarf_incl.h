/*

  Copyright (C) 2000, 2002, 2004 Silicon Graphics, Inc.  All Rights Reserved.
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



#ifndef DWARF_INCL_H
#define DWARF_INCL_H
#if (!defined(HAVE_RAW_LIBELF_OK) && defined(HAVE_LIBELF_OFF64_OK) )
/* At a certain point libelf.h requires _GNU_SOURCE.
   here we assume the criteria in configure determine that
   usefully.
*/
#define _GNU_SOURCE 1
#endif


#include "libdwarfdefs.h"
#include <string.h>

#ifdef HAVE_ELF_H
#include <elf.h>
#endif

#include <limits.h>
#include <dwarf.h>
#include <libdwarf.h>

#include "dwarf_base_types.h"
#include "dwarf_alloc.h"
#include "dwarf_opaque.h"
#include "dwarf_error.h"
#include "dwarf_util.h"
#endif /* DWARF_INCL_H */
