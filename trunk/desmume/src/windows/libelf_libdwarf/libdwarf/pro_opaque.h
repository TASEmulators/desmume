/*

  Copyright (C) 2000,2002,2004 Silicon Graphics, Inc.  All Rights Reserved.
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


#include <stddef.h>

/* 
    Sgidefs included to define __uint32_t, 
    a guaranteed 4-byte quantity.            
*/
#include "libdwarfdefs.h"

#define true                    1
#define false                   0

/* to identify a cie */
#define DW_CIE_ID 		~(0x0)
#define DW_CIE_VERSION		1

/*Dwarf_Word  is unsigned word usable for index, count in memory */
/*Dwarf_Sword is   signed word usable for index, count in memory */
/* The are 32 or 64 bits depending if 64 bit longs or not, which
** fits the  ILP32 and LP64 models
** These work equally well with ILP64.
*/

typedef unsigned long Dwarf_Word;
typedef long Dwarf_Sword;


typedef signed char Dwarf_Sbyte;
typedef unsigned char Dwarf_Ubyte;
typedef signed short Dwarf_Shalf;

/*
	On any change that makes libdwarf producer
	incompatible, increment this number.
	1->2->3 ...

*/
#define  PRO_VERSION_MAGIC 0xdead1


/* these 2 are fixed sizes which must not vary with the
** ILP32/LP64 model. These two stay at 32 bit.
*/
typedef __uint32_t Dwarf_ufixed;
typedef __int32_t Dwarf_sfixed;

/* 
	producer:
	This struct is used to hold information about all
	debug* sections. On creating a new section, section
	names and indices are added to this struct
	definition in pro_section.h
*/
typedef struct Dwarf_P_Section_Data_s *Dwarf_P_Section_Data;

/*
	producer:
	This struct is used to hold entries in the include directories
	part of statement prologue. Definition in pro_line.h
*/
typedef struct Dwarf_P_Inc_Dir_s *Dwarf_P_Inc_Dir;

/*
	producer:
	This struct holds file entries for the statement prologue. 
	Defined in pro_line.h
*/
typedef struct Dwarf_P_F_Entry_s *Dwarf_P_F_Entry;

/*
	producer:
	This struct holds information for each cie. Defn in pro_frame.h
*/
typedef struct Dwarf_P_Cie_s *Dwarf_P_Cie;

/*
	producer:
	Struct to hold line number information, different from 
	Dwarf_Line opaque type.
*/
typedef struct Dwarf_P_Line_s *Dwarf_P_Line;

/*
	producer:
	Struct to hold information about address ranges.
*/
typedef struct Dwarf_P_Simple_nameentry_s *Dwarf_P_Simple_nameentry;
typedef struct Dwarf_P_Simple_name_header_s *Dwarf_P_Simple_name_header;
typedef struct Dwarf_P_Arange_s *Dwarf_P_Arange;
typedef struct Dwarf_P_Per_Reloc_Sect_s *Dwarf_P_Per_Reloc_Sect;
typedef struct Dwarf_P_Per_Sect_String_Attrs_s *Dwarf_P_Per_Sect_String_Attrs;

/* Defined to get at the elf section numbers and section name
   indices in symtab for the dwarf sections
   Must match .rel.* names in _dwarf_rel_section_names
   exactly.
*/
#define         DEBUG_INFO      0
#define         DEBUG_LINE      1
#define         DEBUG_ABBREV    2
#define         DEBUG_FRAME     3
#define         DEBUG_ARANGES   4
#define         DEBUG_PUBNAMES  5
#define         DEBUG_STR       6
#define         DEBUG_FUNCNAMES 7
#define         DEBUG_TYPENAMES 8
#define         DEBUG_VARNAMES  9
#define         DEBUG_WEAKNAMES 10
#define         DEBUG_MACINFO   11
#define         DEBUG_LOC   12

    /* number of debug_* sections not including the relocations */
#define         NUM_DEBUG_SECTIONS      DEBUG_LOC + 1


struct Dwarf_P_Die_s {
    Dwarf_Unsigned di_offset;	/* offset in debug info */
    char *di_abbrev;		/* abbreviation */
    Dwarf_Word di_abbrev_nbytes;	/* # of bytes in abbrev */
    Dwarf_Tag di_tag;
    Dwarf_P_Die di_parent;	/* parent of current die */
    Dwarf_P_Die di_child;	/* first child */
    /* The last child field makes linking up children an O(1) operation,
       See pro_die.c. */
    Dwarf_P_Die di_last_child;	
    Dwarf_P_Die di_left;	/* left sibling */
    Dwarf_P_Die di_right;	/* right sibling */
    Dwarf_P_Attribute di_attrs;	/* list of attributes */
    Dwarf_P_Attribute di_last_attr;	/* last attribute */
    int di_n_attr;		/* number of attributes */
    Dwarf_P_Debug di_dbg;	/* For memory management */
    Dwarf_Unsigned di_marker;   /* used to attach symbols to dies */
};


/* producer fields */
struct Dwarf_P_Attribute_s {
    Dwarf_Half ar_attribute;	/* Attribute Value. */
    Dwarf_Half ar_attribute_form;	/* Attribute Form. */
    Dwarf_P_Die ar_ref_die;	/* die pointer if form ref */
    char *ar_data;		/* data, format given by form */
    Dwarf_Unsigned ar_nbytes;	/* no. of bytes of data */
    Dwarf_Unsigned ar_rel_symidx;	/* when attribute has a
					   relocatable value, holds
					   index of symbol in SYMTAB */
    Dwarf_Ubyte ar_rel_type;	/* relocation type */
    Dwarf_Word ar_rel_offset;	/* Offset of relocation within block */
    char ar_reloc_len;		/* Number of bytes that relocation
				   applies to. 4 or 8. Unused and may
				   be 0 if if ar_rel_type is
				   R_MIPS_NONE */
    Dwarf_P_Attribute ar_next;
};

/* A block of .debug_macinfo data: this forms a series of blocks.
** Each macinfo input is compressed immediately and put into
** the current block if room, else a newblock allocated.
** The space allocation is such that the block and the macinfo
** data are one malloc block: free with a pointer to this and the
** mb_data is freed automatically.
** Like the struct hack, but legal ANSI C.
*/
struct dw_macinfo_block_s {
    struct dw_macinfo_block_s *mb_next;
    unsigned long mb_avail_len;
    unsigned long mb_used_len;
    unsigned long mb_macinfo_data_space_len;
    char *mb_data;		/* original malloc ptr. */
};

/* dwarf_sn_kind is for the array of similarly-treated
   name -> cu ties
*/
enum dwarf_sn_kind { dwarf_snk_pubname, dwarf_snk_funcname,
    dwarf_snk_weakname, dwarf_snk_typename,
    dwarf_snk_varname,
    dwarf_snk_entrycount	/* this one must be last */
};



/* The calls to add a varname etc use a list of
   these as the list.
*/
struct Dwarf_P_Simple_nameentry_s {
    Dwarf_P_Die sne_die;
    char *sne_name;
    int sne_name_len;
    Dwarf_P_Simple_nameentry sne_next;
};

/* An array of these, each of which heads a list
   of Dwarf_P_Simple_nameentry
*/
struct Dwarf_P_Simple_name_header_s {
    Dwarf_P_Simple_nameentry sn_head;
    Dwarf_P_Simple_nameentry sn_tail;
    Dwarf_Signed sn_count;

    /* length that will be generated, not counting fixed header or
       trailer */
    Dwarf_Signed sn_net_len;
};
typedef int (*_dwarf_pro_reloc_name_func_ptr) (Dwarf_P_Debug dbg, 
    int sec_index, 
    Dwarf_Unsigned offset,/* r_offset */
    Dwarf_Unsigned symidx,
    enum Dwarf_Rel_Type type,
    int reltarget_length);

typedef int (*_dwarf_pro_reloc_length_func_ptr) (Dwarf_P_Debug dbg, 
    int sec_index, Dwarf_Unsigned offset,/* r_offset */
    Dwarf_Unsigned start_symidx,
    Dwarf_Unsigned end_symidx,
    enum Dwarf_Rel_Type type,
    int reltarget_length);
typedef int (*_dwarf_pro_transform_relocs_func_ptr) (Dwarf_P_Debug dbg,
						     Dwarf_Signed *
						     new_sec_count);

/*
    Each slot in a block of slots could be:
    a binary stream relocation entry (32 or 64bit relocation data)
    a SYMBOLIC relocation entry.
    During creation sometimes we create multiple chained blocks,
    but sometimes we create a single long block.
    Before returning reloc data to caller, 
    we switch to a single, long-enough,
    block.

    We make counters here Dwarf_Unsigned so that we
    get sufficient alignment. Since we use space after
    the struct (at malloc time) for user data which
    must have Dwarf_Unsigned alignment, this
    struct must have that alignment too.
*/
struct Dwarf_P_Relocation_Block_s {
    Dwarf_Unsigned rb_slots_in_block;	/* slots in block, as created */
    Dwarf_Unsigned rb_next_slot_to_use;	/* counter, start at 0. */
    struct Dwarf_P_Relocation_Block_s *rb_next;
    char *rb_where_to_add_next;	/* pointer to next slot (might be past
				   end, depending on
				   rb_next_slot_to_use) */
    char *rb_data;		/* data area */
};

/* One of these per potential relocation section 
   So one per actual dwarf section.
   Left zeroed when not used (some sections have
   no relocations).
*/
struct Dwarf_P_Per_Reloc_Sect_s {
    unsigned long pr_reloc_total_count;	/* total number of entries
        across all blocks */

    unsigned long pr_slots_per_block_to_alloc;	/* at Block alloc, this 
        is the default number of slots to use */

    int pr_sect_num_of_reloc_sect;	/* sect number returned by
        de_callback_func() or de_callback_func_b() call, this is the sect
        number of the relocation section. */

    /* singly-linked list. add at and ('last') with count of blocks */
    struct Dwarf_P_Relocation_Block_s *pr_first_block;
    struct Dwarf_P_Relocation_Block_s *pr_last_block;
    unsigned long pr_block_count;
};

#define DEFAULT_SLOTS_PER_BLOCK 3

typedef struct memory_list_s {
  struct memory_list_s *prev;
  struct memory_list_s *next;
} memory_list_t;

struct Dwarf_P_Per_Sect_String_Attrs_s {
    int sect_sa_section_number;
    unsigned sect_sa_n_alloc;
    unsigned sect_sa_n_used;
    Dwarf_P_String_Attr sect_sa_list;
};

/* Fields used by producer */
struct Dwarf_P_Debug_s {
    /* used to catch dso passing dbg to another DSO with incompatible
       version of libdwarf See PRO_VERSION_MAGIC */
    int de_version_magic_number;

    Dwarf_Handler de_errhand;
    Dwarf_Ptr de_errarg;

    /* Call back function, used to create .debug* sections. Provided
       by user. Only of these used per dbg. */
    Dwarf_Callback_Func de_callback_func;
    Dwarf_Callback_Func_b de_callback_func_b;

    /* Flags from producer_init call */
    Dwarf_Unsigned de_flags;

    /* This holds information on debug section stream output, including
       the stream data */
    Dwarf_P_Section_Data de_debug_sects;

    /* Pointer to the 'current active' section */
    Dwarf_P_Section_Data de_current_active_section;

    /* Number of debug data streams globs. */
    Dwarf_Word de_n_debug_sect;

    /* File entry information, null terminated singly-linked list */
    Dwarf_P_F_Entry de_file_entries;
    Dwarf_P_F_Entry de_last_file_entry;
    Dwarf_Unsigned de_n_file_entries;

    /* Has the directories used to search for source files */
    Dwarf_P_Inc_Dir de_inc_dirs;
    Dwarf_P_Inc_Dir de_last_inc_dir;
    Dwarf_Unsigned de_n_inc_dirs;

    /* Has all the line number info for the stmt program */
    Dwarf_P_Line de_lines;
    Dwarf_P_Line de_last_line;

    /* List of cie's for the debug unit */
    Dwarf_P_Cie de_frame_cies;
    Dwarf_P_Cie de_last_cie;
    Dwarf_Unsigned de_n_cie;

    /* Singly-linked list of fde's for the debug unit */
    Dwarf_P_Fde de_frame_fdes;
    Dwarf_P_Fde de_last_fde;
    Dwarf_Unsigned de_n_fde;

    /* First die, leads to all others */
    Dwarf_P_Die de_dies;

    /* Pointer to list of strings */
    char *de_strings;

    /* Pointer to chain of aranges */
    Dwarf_P_Arange de_arange;
    Dwarf_P_Arange de_last_arange;
    Dwarf_Sword de_arange_count;

    /* macinfo controls. */
    /* first points to beginning of the list during creation */
    struct dw_macinfo_block_s *de_first_macinfo;

    /* current points to the current, unfilled, block */
    struct dw_macinfo_block_s *de_current_macinfo;

    /* Pointer to the first section, to support reset_section_bytes */
    Dwarf_P_Section_Data de_first_debug_sect;

    /* handles pubnames, weaknames, etc. See dwarf_sn_kind in
       pro_opaque.h */
    struct Dwarf_P_Simple_name_header_s
      de_simple_name_headers[dwarf_snk_entrycount];

    /* relocation data. not all sections will actally have relocation
       info, of course */
    struct Dwarf_P_Per_Reloc_Sect_s de_reloc_sect[NUM_DEBUG_SECTIONS];
    int de_reloc_next_to_return;	/* iterator on reloc sections
					   (SYMBOLIC output) */

    /* used in remembering sections */
    int de_elf_sects[NUM_DEBUG_SECTIONS];  /* elf sect number of
        the section itself, DEBUG_LINE for example */

    Dwarf_Unsigned de_sect_name_idx[NUM_DEBUG_SECTIONS]; /* section 
        name index or handle for the name of the symbol for
        DEBUG_LINE for example */

    int de_offset_reloc;	/* offset reloc type, R_MIPS_32 for
				   example. Specific to the ABI being
				   produced. Relocates offset size
				   field */
    int de_exc_reloc;		/* reloc type specific to exception
				   table relocs. */
    int de_ptr_reloc;		/* standard reloc type, R_MIPS_32 for
				   example. Specific to the ABI being
				   produced. relocates pointer size
				   field */

    unsigned char de_offset_size;  /* section offset. Here to
                                      avoid test of abi in macro
                                      at run time MIPS -n32 4,
                                      -64 8.  */

    unsigned char de_pointer_size;	/* size of pointer in target.
					   Here to avoid test of abi in 
					   macro at run time MIPS -n32 
					   4, -64 is 8.  */

    unsigned char de_is_64bit;	/* non-zero if is 64bit. Else 32 bit:
				   used for passing this info as a flag 
				 */
    unsigned char de_relocation_record_size;	/* reloc record size
						   varies by ABI and
						   relocation-output
						   method (stream or
						   symbolic) */

    unsigned char de_64bit_extension;	/* non-zero if creating 64 bit
					   offsets using dwarf2-99
					   extension proposal */

    int de_ar_data_attribute_form;	/* data8, data4 abi dependent */
    int de_ar_ref_attr_form;	/* ref8 ref4 , abi dependent */

    /* simple name relocations */
    _dwarf_pro_reloc_name_func_ptr de_reloc_name;

    /* relocations for a length, requiring a pair of symbols */
    _dwarf_pro_reloc_length_func_ptr de_reloc_pair;

    _dwarf_pro_transform_relocs_func_ptr de_transform_relocs_to_disk;

    /* following used for macro buffers */
    unsigned long de_compose_avail;
    unsigned long de_compose_used_len;

    unsigned char de_same_endian;
    void *(*de_copy_word) (void *, const void *, size_t);

    /* Add new fields at the END of this struct to preserve some hope
       of sensible behavior on dbg passing between DSOs linked with
       mismatched libdwarf producer versions. */

    Dwarf_P_Marker de_markers;  /* pointer to array of markers */
    unsigned de_marker_n_alloc;
    unsigned de_marker_n_used;
    int de_sect_sa_next_to_return;  /* Iterator on sring attrib sects */
    /* String attributes data of each section. */
    struct Dwarf_P_Per_Sect_String_Attrs_s de_sect_string_attr[NUM_DEBUG_SECTIONS];
};

#define CURRENT_VERSION_STAMP		2

Dwarf_Unsigned _dwarf_add_simple_name_entry(Dwarf_P_Debug dbg,
					    Dwarf_P_Die die,
					    char *entry_name,
					    enum dwarf_sn_kind
					    entrykind,
					    Dwarf_Error * error);


#define DISTINGUISHED_VALUE 0xffffffff	/* 64bit extension flag */
