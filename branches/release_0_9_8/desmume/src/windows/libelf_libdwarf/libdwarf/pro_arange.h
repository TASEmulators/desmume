/*

  Copyright (C) 2000,2004 Silicon Graphics, Inc.  All Rights Reserved.

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


/*
	If ag_end_symbol_index is zero, 
	ag_length must be known and non-zero.


	Deals with length being known costant or fr
	assembler output, not known.

*/

struct Dwarf_P_Arange_s {
    Dwarf_Addr ag_begin_address;	/* known address or for
					   symbolic assem output,
					   offset of symbol */
    Dwarf_Addr ag_length;	/* zero or address or offset */
    Dwarf_Unsigned ag_symbol_index;

    Dwarf_P_Arange ag_next;

    Dwarf_Unsigned ag_end_symbol_index;	/* zero or index/id of end
					   symbol */
    Dwarf_Addr ag_end_symbol_offset;	/* known address or for
					   symbolic assem output,
					   offset of end symbol */

};
