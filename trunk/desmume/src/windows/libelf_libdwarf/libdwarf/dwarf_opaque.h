/*

  Copyright (C) 2000-2005 Silicon Graphics, Inc.  All Rights Reserved.
  Portions Copyright (C) 2007-2010 David Anderson. All Rights Reserved.
  Portions Copyright (C) 2008-2010 Arxan Technologies, Inc. All Rights Reserved.

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
/* The versions applicable by section are:
                       DWARF2 DWARF3 DWARF4
 .debug_abbrev           -      -      -
 .debug_aranges          2      2      2
 .debug_frame            1      3      4
 .debug_info             2      3      4
 .debug_line             2      3      4
 .debug_loc              -      -      -
 .debug_macinfo          -      -      -
 .debug_pubtypes         x      2      2
 .debug_pubnames         2      2      2
 .debug_ranges           x      -      -
 .debug_str              -      -      -
 .debug_types            x      x      4
*/

#include <stddef.h>


struct Dwarf_Die_s {
    Dwarf_Byte_Ptr di_debug_info_ptr;
    Dwarf_Abbrev_List di_abbrev_list;
    Dwarf_CU_Context di_cu_context;
    int  di_abbrev_code;
};

struct Dwarf_Attribute_s {
    Dwarf_Half ar_attribute;	/* Attribute Value. */
    Dwarf_Half ar_attribute_form;	/* Attribute Form. */
    Dwarf_Half ar_attribute_form_direct;
	        /* Identical to ar_attribute_form except that if
		the original form uleb was DW_FORM_indirect,
		ar_attribute_form_direct contains DW_FORM_indirect
		but ar_attribute_form contains the true form. */

    Dwarf_CU_Context ar_cu_context;
    Dwarf_Small *ar_debug_info_ptr;
    Dwarf_Attribute ar_next;
};

/*
    This structure provides the context for a compilation unit.  
    Thus, it contains the Dwarf_Debug, cc_dbg, that this cu
    belongs to.  It contains the information in the compilation 
    unit header, cc_length, cc_version_stamp, cc_abbrev_offset,
    and cc_address_size, in the .debug_info section for that cu.  
    In addition, it contains the count, cc_count_cu, of the cu 
    number of that cu in the list of cu's in the .debug_info.  
    The count starts at 1, ie cc_count_cu is 1 for the first cu, 
    2 for the second and so on.  This struct also contains a 
    pointer, cc_abbrev_table, to a list of pairs of abbrev code 
    and a pointer to the start of that abbrev 
    in the .debug_abbrev section.

    Each die will also contain a pointer to such a struct to 
    record the context for that die.  

    Notice that a pointer to the CU DIE itself is 
    Dwarf_Off off2 = cu_context->cc_debug_info_offset;
    cu_die_info_ptr = dbg->de_debug_info.dss_data +
            off2 + _dwarf_length_of_cu_header(dbg, off2);
    
    **Updated by dwarf_next_cu_header in dwarf_die_deliv.c
*/
struct Dwarf_CU_Context_s {
    Dwarf_Debug cc_dbg;
    /* The sum of cc_length, cc_length_size, and cc_extension_size
       is the total length of the CU including its header. */
    Dwarf_Word cc_length;
    /* cc_length_size is the size in bytes of an offset.
       4 for 32bit dwarf, 8 for 64bit dwarf (whether MIPS/IRIX
       64bit dwarf or standard 64bit dwarf using the extension
       mechanism). */
    Dwarf_Small cc_length_size;
    /* cc_extension_size is zero unless this is standard
       DWARF3 and later 64bit dwarf using the extension mechanism.
       If it is the DWARF3 and later 64bit dwarf cc_extension
       size is 4. So for 32bit dwarf and MIPS/IRIX 64bit dwarf
       cc_extension_size is zero.  */
    Dwarf_Small cc_extension_size;
    Dwarf_Half cc_version_stamp;
    Dwarf_Sword cc_abbrev_offset;
    Dwarf_Small cc_address_size;
    /* cc_debug_info_offset is the offset in the section
       of the CU header of this CU.  Dwarf_Word
       should be large enough. */
    Dwarf_Word cc_debug_info_offset;
    Dwarf_Byte_Ptr cc_last_abbrev_ptr;
    Dwarf_Hash_Table cc_abbrev_hash_table;
    Dwarf_CU_Context cc_next;
    /*unsigned char cc_offset_length; */
};

/* Consolidates section-specific data in one place.
   Section is an Elf specific term, intended as a general
   term (for non-Elf objects some code must synthesize the 
   values somehow).
   Makes adding more section-data much simpler. */
struct Dwarf_Section_s {
    Dwarf_Small *  dss_data;
    Dwarf_Unsigned dss_size;
    Dwarf_Word     dss_index;
    /* dss_addr is the 'section address' which is only
       non-zero for a GNU eh section.
       Purpose: to handle DW_EH_PE_pcrel encoding. Leaving
       it zero is fine for non-elf.  */
    Dwarf_Addr     dss_addr;
    Dwarf_Small    dss_data_was_malloc;

    /* For non-elf, leaving the following fields zero
       will mean they are ignored. */
    /* dss_link should be zero unless a section has a link
       to another (sh_link).  Used to access relocation data for
       a section (and for symtab section, access its strtab). */
    Dwarf_Word     dss_link;
    /* The following is used when reading .rela sections
       (such sections appear in some .o files). */
    Dwarf_Half     dss_reloc_index; /* Zero means ignore the reloc fields. */
    Dwarf_Small *  dss_reloc_data;
    Dwarf_Unsigned dss_reloc_size;
    Dwarf_Addr     dss_reloc_addr;
    /* dss_reloc_symtab is the sh_link of a .rela to its .symtab, leave
       it 0 if non-meaningful. */
    Dwarf_Addr     dss_reloc_symtab;
    /* dss_reloc_link should be zero unless a reloc section has a link
       to another (sh_link).  Used to access the symtab for relocations
       a section. */
    Dwarf_Word     dss_reloc_link;
    /* Pointer to the elf symtab, used for elf .rela. Leave it 0
       if not relevant. */
    struct Dwarf_Section_s *dss_symtab;
};

/* Overview: if next_to_use== first, no error slots are used.
   If next_to_use+1 (mod maxcount) == first the slots are all used
*/
struct Dwarf_Harmless_s {
  unsigned dh_maxcount;
  unsigned dh_next_to_use;
  unsigned dh_first;
  unsigned dh_errs_count;
  char **  dh_errors;
};

struct Dwarf_Debug_s {
    /* All file access methods and support data 
       are hidden in this structure. 
       We get a pointer, callers control the lifetime of the
       structure and contents. */
    struct Dwarf_Obj_Access_Interface_s *de_obj_file;

    Dwarf_Handler de_errhand;
    Dwarf_Ptr de_errarg;

    /* 
       Context for the compilation_unit just read by a call to
       dwarf_next_cu_header. **Updated by dwarf_next_cu_header in
       dwarf_die_deliv.c */
    Dwarf_CU_Context de_cu_context;

    /* 
       Points to linked list of CU Contexts for the CU's already read.
       These are only CU's read by dwarf_next_cu_header(). */
    Dwarf_CU_Context de_cu_context_list;

    /* 
       Points to the last CU Context added to the list by
       dwarf_next_cu_header(). */
    Dwarf_CU_Context de_cu_context_list_end;

    /* 
       This is the list of CU contexts read for dwarf_offdie().  These
       may read ahead of dwarf_next_cu_header(). */
    Dwarf_CU_Context de_offdie_cu_context;
    Dwarf_CU_Context de_offdie_cu_context_end;

    /* Offset of last byte of last CU read. */
    Dwarf_Word de_info_last_offset;

    /* 
       Number of bytes in the length, and offset field in various
       .debug_* sections.  It's not very meaningful, and is
       only used in one 'approximate' calculation.  */
    Dwarf_Small de_length_size;

    /* number of bytes in a pointer of the target in various .debug_
       sections. 4 in 32bit, 8 in MIPS 64, ia64. */
    Dwarf_Small de_pointer_size;

    /* set at creation of a Dwarf_Debug to say if form_string should be 
       checked for valid length at every call. 0 means do the check.
       non-zero means do not do the check. */
    Dwarf_Small de_assume_string_in_bounds;

    /* 
       Dwarf_Alloc_Hdr_s structs used to manage chunks that are
       malloc'ed for each allocation type for structs. */
    struct Dwarf_Alloc_Hdr_s de_alloc_hdr[ALLOC_AREA_REAL_TABLE_MAX];
#ifdef DWARF_SIMPLE_MALLOC
    struct simple_malloc_record_s *  de_simple_malloc_base;
#endif
    

    /* 
       These fields are used to process debug_frame section.  **Updated 
       by dwarf_get_fde_list in dwarf_frame.h */
    /* 
       Points to contiguous block of pointers to Dwarf_Cie_s structs. */
    Dwarf_Cie *de_cie_data;
    /* Count of number of Dwarf_Cie_s structs. */
    Dwarf_Signed de_cie_count;
    /* Keep eh (GNU) separate!. */
    Dwarf_Cie *de_cie_data_eh;
    Dwarf_Signed de_cie_count_eh;
    /* 
       Points to contiguous block of pointers to Dwarf_Fde_s structs. */
    Dwarf_Fde *de_fde_data;
    /* Count of number of Dwarf_Fde_s structs. */
    Dwarf_Signed de_fde_count;
    /* Keep eh (GNU) separate!. */
    Dwarf_Fde *de_fde_data_eh;
    Dwarf_Signed de_fde_count_eh;

    struct Dwarf_Section_s de_debug_info; 
    struct Dwarf_Section_s de_debug_abbrev;
    struct Dwarf_Section_s de_debug_line;
    struct Dwarf_Section_s de_debug_loc;
    struct Dwarf_Section_s de_debug_aranges;
    struct Dwarf_Section_s de_debug_macinfo;
    struct Dwarf_Section_s de_debug_pubnames;
    struct Dwarf_Section_s de_debug_str;
    struct Dwarf_Section_s de_debug_frame;

    /* gnu: the g++ eh_frame section */
    struct Dwarf_Section_s de_debug_frame_eh_gnu;

    struct Dwarf_Section_s de_debug_pubtypes; /* DWARF3 .debug_pubtypes */

    struct Dwarf_Section_s de_debug_funcnames;
    struct Dwarf_Section_s de_debug_typenames; /* SGI IRIX extension essentially
			identical to DWARF3 .debug_pubtypes. */
    struct Dwarf_Section_s de_debug_varnames;
    struct Dwarf_Section_s de_debug_weaknames;
    struct Dwarf_Section_s de_debug_ranges;	

    /* For non-elf, simply leave the following two structs zeroed and
       they will be ignored. */
    struct Dwarf_Section_s de_elf_symtab;	
    struct Dwarf_Section_s de_elf_strtab;	


    void *(*de_copy_word) (void *, const void *, size_t);
    unsigned char de_same_endian;
    unsigned char de_elf_must_close; /* if non-zero, then
	it was dwarf_init (not dwarf_elf_init)
	so must elf_end() */

    /* Default is DW_FRAME_INITIAL_VALUE from header. */
    Dwarf_Half de_frame_rule_initial_value;  

    /* Default is   DW_FRAME_LAST_REG_NUM. */
    Dwarf_Half de_frame_reg_rules_entry_count; 

    Dwarf_Half de_frame_cfa_col_number; 
    Dwarf_Half de_frame_same_value_number; 
    Dwarf_Half de_frame_undefined_value_number; 

    unsigned char de_big_endian_object; /* non-zero if big-endian
		object opened. */

    struct Dwarf_Harmless_s de_harmless_errors;
};

typedef struct Dwarf_Chain_s *Dwarf_Chain;
struct Dwarf_Chain_s {
    void *ch_item;
    Dwarf_Chain ch_next;
};


#define CURRENT_VERSION_STAMP		2 /* DWARF2 */
#define CURRENT_VERSION_STAMP3		3 /* DWARF3 */
#define CURRENT_VERSION_STAMP4		4 /* DWARF4 */

    /* Size of cu header version stamp field. */
#define CU_VERSION_STAMP_SIZE   sizeof(Dwarf_Half)

    /* Size of cu header address size field. */
#define CU_ADDRESS_SIZE_SIZE	sizeof(Dwarf_Small)

void *_dwarf_memcpy_swap_bytes(void *s1, const void *s2, size_t len);

#define ORIGINAL_DWARF_OFFSET_SIZE  4
#define DISTINGUISHED_VALUE  0xffffffff
#define DISTINGUISHED_VALUE_OFFSET_SIZE 8

/*
    We don't load the sections until they are needed. This function is
    used to load the section.
 */
int _dwarf_load_section(Dwarf_Debug,
    struct Dwarf_Section_s *,
    Dwarf_Error *);
