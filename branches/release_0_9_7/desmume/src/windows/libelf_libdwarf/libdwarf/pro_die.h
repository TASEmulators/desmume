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
	This struct holds the abbreviation table, before they are written 
	on disk. Holds a linked list of abbreviations, each consisting of
	a bitmap for attributes and a bitmap for forms
*/
typedef struct Dwarf_P_Abbrev_s *Dwarf_P_Abbrev;

struct Dwarf_P_Abbrev_s {
    Dwarf_Unsigned abb_idx;	/* index of abbreviation */
    Dwarf_Tag abb_tag;		/* tag of die */
    Dwarf_Ubyte abb_children;	/* if children are present */
    Dwarf_ufixed *abb_attrs;	/* holds names of attrs */
    Dwarf_ufixed *abb_forms;	/* forms of attributes */
    int abb_n_attr;		/* num of attrs = # of forms */
    Dwarf_P_Abbrev abb_next;
};

/* used in pro_section.c */

int _dwarf_pro_add_AT_fde(Dwarf_P_Debug dbg, Dwarf_P_Die die,
			  Dwarf_Unsigned offset, Dwarf_Error * error);

int _dwarf_pro_add_AT_stmt_list(Dwarf_P_Debug dbg,
				Dwarf_P_Die first_die,
				Dwarf_Error * error);

int _dwarf_pro_add_AT_macro_info(Dwarf_P_Debug dbg,
				 Dwarf_P_Die first_die,
				 Dwarf_Unsigned offset,
				 Dwarf_Error * error);
