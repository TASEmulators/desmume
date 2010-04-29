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





/* relocation section names */
extern char *_dwarf_rel_section_names[];

/* section names */
extern char *_dwarf_sectnames[];

/* struct to hold relocation entries. Its mantained as a linked
   list of relocation structs, and will then be written at as a 
   whole into the relocation section. Whether its 32 bit or
   64 bit will be obtained from Dwarf_Debug pointer.
*/





/*
	struct stores a chunk of data pertaining to a section 
*/
struct Dwarf_P_Section_Data_s {
    int ds_elf_sect_no;		/* elf section number */
    char *ds_data;		/* data contained in section */
    unsigned long ds_nbytes;	/* bytes of data used so far */
    unsigned long ds_orig_alloc;	/* bytes allocated originally */
    Dwarf_P_Section_Data ds_next;	/* next on the list */
};

/* Used to allow a dummy initial struct (which we
   drop before it gets used
   This must not match any legitimate 'section' number.
*/
#define MAGIC_SECT_NO -3

/* Size of chunk of data allocated in one alloc 
   Not clear if this is the best size.
   Used to be just 4096 for user data, the section data struct
   was a separate malloc.
*/
#define 	CHUNK_SIZE (4096 - sizeof (struct Dwarf_P_Section_Data_s))

/*
	chunk alloc routine - 
	if chunk->ds_data is nil, it will alloc CHUNK_SIZE bytes, 
	and return pointer to the beginning. If chunk is not nil, 
	it will see if there's enoungh space for nbytes in current 
	chunk, if not, add new chunk to linked list, and return 
	a char * pointer to it. Return null if unsuccessful.
*/
Dwarf_Small *_dwarf_pro_buffer(Dwarf_P_Debug dbg, int sectno,
			       unsigned long nbytes);

#define GET_CHUNK(dbg,sectno,ptr,nbytes,error) \
	{ \
	    (ptr) = _dwarf_pro_buffer((dbg),(sectno),(nbytes)); \
	    if ((ptr) == NULL) { \
		DWARF_P_DBG_ERROR(dbg,DW_DLE_CHUNK_ALLOC,-1); \
	    } \
	}



int
  _dwarf_transform_arange_to_disk(Dwarf_P_Debug dbg,
				  Dwarf_Error * error);

/* These are for creating ELF section type codes.
*/
#if defined(linux) || defined(__BEOS__) || !defined(SHT_MIPS_DWARF)
/* Intel's SoftSdv accepts only this */
#define SECTION_TYPE            SHT_PROGBITS
#else
#define SECTION_TYPE            SHT_MIPS_DWARF
#endif
