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
    Largest register value that can be coded into
    the opcode since there are only 6 bits in the
    register field.
*/
#define MAX_6_BIT_VALUE		0x3f

/*
	This struct holds debug_frame instructions
*/
typedef struct Dwarf_P_Frame_Pgm_s *Dwarf_P_Frame_Pgm;

struct Dwarf_P_Frame_Pgm_s {
    Dwarf_Ubyte dfp_opcode;	/* opcode - includes reg # */
    char *dfp_args;		/* operands */
    int dfp_nbytes;		/* number of bytes in args */
#if 0
    Dwarf_Unsigned dfp_sym_index;	/* 0 unless reloc needed */
#endif
    Dwarf_P_Frame_Pgm dfp_next;
};


/*
	This struct has cie related information. Used to gather data 
	from user program, and later to transform to disk form
*/
struct Dwarf_P_Cie_s {
    Dwarf_Ubyte cie_version;
    char *cie_aug;		/* augmentation */
    Dwarf_Ubyte cie_code_align;	/* alignment of code */
    Dwarf_Sbyte cie_data_align;
    Dwarf_Ubyte cie_ret_reg;	/* return register # */
    char *cie_inst;		/* initial instruction */
    long cie_inst_bytes;
    /* no of init_inst */
    Dwarf_P_Cie cie_next;
};


/* producer fields */
struct Dwarf_P_Fde_s {
    Dwarf_Unsigned fde_unused1;

    /* function/subr die for this fde */
    Dwarf_P_Die fde_die;

    /* index to asso. cie */
    Dwarf_Word fde_cie;

    /* Address of first location of the code this frame applies to If
       fde_end_symbol non-zero, this represents the offset from the
       symbol indicated by fde_r_symidx */
    Dwarf_Addr fde_initloc;

    /* Relocation symbol for address of the code this frame applies to. 
     */
    Dwarf_Unsigned fde_r_symidx;

    /* Bytes of instr for this fde, if known */
    Dwarf_Unsigned fde_addr_range;

    /* linked list of instructions we will put in fde. */
    Dwarf_P_Frame_Pgm fde_inst;

    /* number of instructions in fde */
    long fde_n_inst;

    /* number of bytes of inst in fde */
    long fde_n_bytes;

    /* offset into exception table for this function. */
    Dwarf_Signed fde_offset_into_exception_tables;

    /* The symbol for the exception table elf section. */
    Dwarf_Unsigned fde_exception_table_symbol;

    /* pointer to last inst */
    Dwarf_P_Frame_Pgm fde_last_inst;

    Dwarf_P_Fde fde_next;

    /* The symbol and offset of the end symbol. When fde_end_symbol is
       non-zero we must represent the */
    Dwarf_Addr fde_end_symbol_offset;
    Dwarf_Unsigned fde_end_symbol;

    int fde_uwordb_size;
    Dwarf_P_Debug fde_dbg;

    /* If fde_block is non-null, then it is the set of instructions.
       so we should use it rather than fde_inst. */
    Dwarf_Unsigned fde_inst_block_size;
    void *fde_block;
};
