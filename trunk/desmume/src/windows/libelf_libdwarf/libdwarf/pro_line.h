/*

  Copyright (C) 2000,2004 Silicon Graphics, Inc.  All Rights Reserved.
  Portions Copyright 2007-2010 Sun Microsystems, Inc. All rights reserved.

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



#define VERSION				2
#ifdef __i386
#define MIN_INST_LENGTH			1
#else
#define MIN_INST_LENGTH			4
#endif
#define DEFAULT_IS_STMT			false
			/* line base and range are temporarily defines.
			   They need to be calculated later */
#define LINE_BASE			-1
#define LINE_RANGE			4

#define OPCODE_BASE			10
#define MAX_OPCODE			255


/*
	This struct is used to hold entries in the include directories
	part of statement prologue.
*/
struct Dwarf_P_Inc_Dir_s {
    char *did_name;		/* name of directory */
    Dwarf_P_Inc_Dir did_next;
};


/*
	This struct holds file entries for the statement prologue. 
	Defined in pro_line.h
*/
struct Dwarf_P_F_Entry_s {
    char *dfe_name;
    char *dfe_args;		/* has dir index, time of modification,
				   length in bytes. Encodes as leb128 */
    int dfe_nbytes;		/* number of bytes in args */
    Dwarf_P_F_Entry dfe_next;
};


/*
	Struct holding line number information for each of the producer 
	line entries 
*/
struct Dwarf_P_Line_s {
    /* code address */
    Dwarf_Addr dpl_address;

    /* file index, index into file entry */
    Dwarf_Word dpl_file;

    /* line number */
    Dwarf_Word dpl_line;

    /* column number */
    Dwarf_Word dpl_column;

    /* whether its a beginning of a stmt */
    Dwarf_Ubyte dpl_is_stmt;

    /* whether its a beginning of basic blk */
    Dwarf_Ubyte dpl_basic_block;

    /* used to store opcodes set_address, and end_seq */
    Dwarf_Ubyte dpl_opc;

    /* 
       Used only for relocations.  Has index of symbol relative to
       which relocation has to be done (the S part in S + A) */
    Dwarf_Unsigned dpl_r_symidx;

    Dwarf_P_Line dpl_next;
};

/* 
	to initialize state machine registers, definition in 
	pro_line.c
*/
void _dwarf_pro_reg_init(Dwarf_P_Line);
