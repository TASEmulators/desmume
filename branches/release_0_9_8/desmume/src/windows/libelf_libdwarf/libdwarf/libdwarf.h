/*

  Copyright (C) 2000-2010 Silicon Graphics, Inc.  All Rights Reserved.
  Portions Copyright 2007-2010 Sun Microsystems, Inc. All rights reserved.
  Portions Copyright 2008-2010 David Anderson. All rights reserved.
  Portions Copyright 2008-2010 Arxan Technologies, Inc. All rights reserved.

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


#ifndef _LIBDWARF_H
#define _LIBDWARF_H
#ifdef __cplusplus
extern "C" {
#endif
/*
    libdwarf.h  
    $Revision: #9 $ $Date: 2008/01/17 $

    For libdwarf producers and consumers

    The interface is defined as having 8-byte signed and unsigned
    values so it can handle 64-or-32bit target on 64-or-32bit host.
    Addr is the native size: it represents pointers on
    the host machine (not the target!).

    This contains declarations for types and all producer
    and consumer functions.

    Function declarations are written on a single line each here
    so one can use grep  to each declaration in its entirety.
    The declarations are a little harder to read this way, but...

*/

struct Elf;
typedef struct Elf* dwarf_elf_handle;

/* To enable printing with printf regardless of the
   actual underlying data type, we define the DW_PR_xxx macros. */
#if (_MIPS_SZLONG == 64)
/* Special case for MIPS, so -64 (LP64) build gets simple -long-.
   Non-MIPS LP64 or ILP64 environments should probably ensure
   _MIPS_SZLONG set to 64 everywhere this header is #included.
*/
typedef int             Dwarf_Bool;         /* boolean type */
typedef unsigned long   Dwarf_Off;          /* 4 or 8 byte file offset */
typedef unsigned long   Dwarf_Unsigned;     /* 4 or 8 byte unsigned value */
typedef unsigned short  Dwarf_Half;         /* 2 byte unsigned value */
typedef unsigned char   Dwarf_Small;        /* 1 byte unsigned value */
typedef signed   long   Dwarf_Signed;       /* 4 or 8 byte signed value */
typedef unsigned long   Dwarf_Addr;         /* target memory address */
#define  DW_PR_DUx  "lx"
#define  DW_PR_DSx  "lx"
#define  DW_PR_DUu  "lu"
#define  DW_PR_DSd  "ld"

#else /* 32-bit */
/* This is for ILP32, allowing i/o of 64bit dwarf info.
   Also should be fine for LP64 and ILP64 cases.
*/
typedef int                 Dwarf_Bool;     /* boolean type */
typedef unsigned long long  Dwarf_Off;      /* 8 byte file offset */
typedef unsigned long long  Dwarf_Unsigned; /* 8 byte unsigned value*/
typedef unsigned short      Dwarf_Half;     /* 2 byte unsigned value */
typedef unsigned char       Dwarf_Small;    /* 1 byte unsigned value */
typedef signed   long long  Dwarf_Signed;   /* 8 byte signed value */
typedef unsigned long long  Dwarf_Addr;     /* target memory address */
#define  DW_PR_DUx  "llx"
#define  DW_PR_DSx  "llx"
#define  DW_PR_DUu  "llu"
#define  DW_PR_DSd  "lld"
#endif
#ifdef HAVE_NONSTANDARD_PRINTF_64_FORMAT
/* Windows does not use std C formatting, so allow it. */
#undef DW_PR_DUx
#undef DW_PR_DSx
#undef DW_PR_DUu
#undef DW_PR_DSd
#define  DW_PR_DUx  "I64x"
#define  DW_PR_DSx  "I64x"
#define  DW_PR_DUu  "I64u"
#define  DW_PR_DSd  "I64d"
#endif /* HAVE_NONSTANDARD_FORMAT */

typedef void*        Dwarf_Ptr;          /* host machine pointer */

/* Used for DW_FORM_ref_sig8. It is not a string, it
   is 8 bytes of a signature one would use to find
   a type unit. See dwarf_formsig8()
*/
typedef struct  {
    char signature[8];
} Dwarf_Sig8;

/* Contains info on an uninterpreted block of data
*/
typedef struct {
    Dwarf_Unsigned  bl_len;         /* length of block */
    Dwarf_Ptr       bl_data;        /* uninterpreted data */
    Dwarf_Small     bl_from_loclist; /*non-0 if loclist, else debug_info*/
    Dwarf_Unsigned  bl_section_offset; /* Section (not CU) offset
                                        which 'data' comes from. */
} Dwarf_Block;


/* location record
*/
typedef struct {
    Dwarf_Small     lr_atom;        /* location operation */
    Dwarf_Unsigned  lr_number;      /* operand */
    Dwarf_Unsigned  lr_number2;     /* for OP_BREGx */
    Dwarf_Unsigned  lr_offset;      /* offset in locexpr for OP_BRA etc */
} Dwarf_Loc;


/* location description
*/
typedef struct {
    Dwarf_Addr      ld_lopc;        /* beginning of active range */ 
    Dwarf_Addr      ld_hipc;        /* end of active range */
    Dwarf_Half      ld_cents;       /* count of location records */
    Dwarf_Loc*      ld_s;           /* pointer to list of same */
    Dwarf_Small     ld_from_loclist; 
                      /* non-0 if loclist, else debug_info*/

    Dwarf_Unsigned  ld_section_offset; /* Section (not CU) offset
                    where loc-expr begins*/
} Dwarf_Locdesc;

/* First appears in DWARF3.
   The dwr_addr1/addr2 data is either an offset (DW_RANGES_ENTRY)
   or an address (dwr_addr2 in DW_RANGES_ADDRESS_SELECTION) or
   both are zero (DW_RANGES_END).
*/
enum Dwarf_Ranges_Entry_Type { DW_RANGES_ENTRY, 
    DW_RANGES_ADDRESS_SELECTION,
    DW_RANGES_END };
typedef struct {
    Dwarf_Addr dwr_addr1;
    Dwarf_Addr dwr_addr2; 
    enum Dwarf_Ranges_Entry_Type  dwr_type;
} Dwarf_Ranges;

/* Frame description instructions expanded.
*/
typedef struct {
    Dwarf_Small     fp_base_op;
    Dwarf_Small     fp_extended_op;
    Dwarf_Half      fp_register;

    /* Value may be signed, depends on op. 
           Any applicable data_alignment_factor has
           not been applied, this is the  raw offset. */
    Dwarf_Unsigned  fp_offset;
    Dwarf_Off       fp_instr_offset;
} Dwarf_Frame_Op; /* DWARF2 */

typedef struct {
    Dwarf_Small     fp_base_op;
    Dwarf_Small     fp_extended_op;
    Dwarf_Half      fp_register;

    /* Value may be signed, depends on op. 
           Any applicable data_alignment_factor has
           not been applied, this is the  raw offset. */
    Dwarf_Unsigned  fp_offset_or_block_len; 
    Dwarf_Small     *fp_expr_block;

    Dwarf_Off       fp_instr_offset;
} Dwarf_Frame_Op3;  /* DWARF3 and DWARF2 compatible */

/*  ***IMPORTANT NOTE, TARGET DEPENDENCY ****
   DW_REG_TABLE_SIZE must be at least as large as
   the number of registers
   (DW_FRAME_LAST_REG_NUM) as defined in dwarf.h
   Preferably identical to DW_FRAME_LAST_REG_NUM.
   Ensure [0-DW_REG_TABLE_SIZE] does not overlap 
   DW_FRAME_UNDEFINED_VAL or DW_FRAME_SAME_VAL. 
   Also ensure DW_FRAME_REG_INITIAL_VALUE is set to what
   is appropriate to your cpu.
   For various CPUs  DW_FRAME_UNDEFINED_VAL is correct
   as the value for DW_FRAME_REG_INITIAL_VALUE.

   For consumer apps, this can be set dynamically: see
   dwarf_set_frame_rule_table_size();
 */
#ifndef DW_REG_TABLE_SIZE
#define DW_REG_TABLE_SIZE  66
#endif

/* For MIPS, DW_FRAME_SAME_VAL is the correct default value 
   for a frame register value. For other CPUS another value
   may be better, such as DW_FRAME_UNDEFINED_VAL.
   See dwarf_set_frame_rule_table_size
*/
#ifndef DW_FRAME_REG_INITIAL_VALUE
#define DW_FRAME_REG_INITIAL_VALUE DW_FRAME_SAME_VAL
#endif

/* Taken as meaning 'undefined value', this is not
   a column or register number.
   Only present at libdwarf runtime in the consumer
   interfaces. Never on disk.
   DW_FRAME_* Values present on disk are in dwarf.h
   Ensure this is > DW_REG_TABLE_SIZE (the reg table
   size is changeable at runtime with the *reg3() interfaces,
   and this value must be greater than the reg table size).
*/
#define DW_FRAME_UNDEFINED_VAL          1034

/* Taken as meaning 'same value' as caller had, not a column
   or register number.
   Only present at libdwarf runtime in the consumer
   interfaces. Never on disk.
   DW_FRAME_* Values present on disk are in dwarf.h
   Ensure this is > DW_REG_TABLE_SIZE (the reg table
   size is changeable at runtime with the *reg3() interfaces,
   and this value must be greater than the reg table size).
*/
#define DW_FRAME_SAME_VAL               1035

/* For DWARF3 consumer interfaces, make the CFA a column with no
   real table number.  This is what should have been done
   for the DWARF2 interfaces.  This actually works for
   both DWARF2 and DWARF3, but see the libdwarf documentation
   on Dwarf_Regtable3 and  dwarf_get_fde_info_for_reg3()
   and  dwarf_get_fde_info_for_all_regs3()  
   Do NOT use this with the older dwarf_get_fde_info_for_reg()
   or dwarf_get_fde_info_for_all_regs() consumer interfaces.
   Must be higher than any register count for *any* ABI
   (ensures maximum applicability with minimum effort).
   Ensure this is > DW_REG_TABLE_SIZE (the reg table
   size is changeable at runtime with the *reg3() interfaces,
   and this value must be greater than the reg table size).
   Only present at libdwarf runtime in the consumer
   interfaces. Never on disk.
*/
#define DW_FRAME_CFA_COL3               1436

/* The following are all needed to evaluate DWARF3 register rules.
*/
#define DW_EXPR_OFFSET 0  /* DWARF2 only sees this. */
#define DW_EXPR_VAL_OFFSET 1
#define DW_EXPR_EXPRESSION 2
#define DW_EXPR_VAL_EXPRESSION 3

typedef struct Dwarf_Regtable_Entry_s {
    /*  For each index i (naming a hardware register with dwarf number
            i) the following is true and defines the value of that register:

           If dw_regnum is Register DW_FRAME_UNDEFINED_VAL  
         it is not DWARF register number but
        a place holder indicating the register has no defined value.
           If dw_regnum is Register DW_FRAME_SAME_VAL 
           it  is not DWARF register number but
        a place holder indicating the register has the same
                value in the previous frame.
       DW_FRAME_UNDEFINED_VAL, DW_FRAME_SAME_VAL are
           only present at libdwarf runtime. Never on disk.
           DW_FRAME_* Values present on disk are in dwarf.h

          Otherwise: the register number is a DWARF register number
          (see ABI documents for how this translates to hardware/
           software register numbers in the machine hardware)
      and the following applies:

          if dw_value_type == DW_EXPR_OFFSET (the only case for dwarf2):
            If dw_offset_relevant is non-zero, then
                the value is stored at at the address CFA+N where 
                N is a signed offset. 
                Rule: Offset(N)
            If dw_offset_relevant is zero, then the value of the register
                is the value of (DWARF) register number dw_regnum.
                Rule: register(F)
          Other values of dw_value_type are an error.
        */
    Dwarf_Small         dw_offset_relevant;

    /* For DWARF2, always 0 */
        Dwarf_Small         dw_value_type; 

    Dwarf_Half          dw_regnum;

    /* The data type here should  the larger of Dwarf_Addr
           and Dwarf_Unsigned and Dwarf_Signed. */
    Dwarf_Addr          dw_offset;
} Dwarf_Regtable_Entry;

typedef struct Dwarf_Regtable_s {
    struct Dwarf_Regtable_Entry_s rules[DW_REG_TABLE_SIZE];
} Dwarf_Regtable;

/* opaque type. Functional interface shown later. */
struct Dwarf_Reg_value3_s;
typedef struct Dwarf_Reg_value3_s Dwarf_Reg_Value3; 

typedef struct Dwarf_Regtable_Entry3_s {
    /*  For each index i (naming a hardware register with dwarf number
        i) the following is true and defines the value of that register:

          If dw_regnum is Register DW_FRAME_UNDEFINED_VAL  
             it is not DWARF register number but
             a place holder indicating the register has no defined value.
          If dw_regnum is Register DW_FRAME_SAME_VAL 
             it  is not DWARF register number but
             a place holder indicating the register has the same
             value in the previous frame.
           DW_FRAME_UNDEFINED_VAL, DW_FRAME_SAME_VAL and
             DW_FRAME_CFA_COL3 are only present at libdwarf runtime. 
             Never on disk.
             DW_FRAME_* Values present on disk are in dwarf.h
           Because DW_FRAME_SAME_VAL and DW_FRAME_UNDEFINED_VAL 
           and DW_FRAME_CFA_COL3 are defineable at runtime 
           consider the names symbolic in this comment, not absolute.

          Otherwise: the register number is a DWARF register number
            (see ABI documents for how this translates to hardware/
             software register numbers in the machine hardware)
             and the following applies:

           In a cfa-defining entry (rt3_cfa_rule) the regnum is the
           CFA 'register number'. Which is some 'normal' register,
           not DW_FRAME_CFA_COL3, nor DW_FRAME_SAME_VAL, nor 
           DW_FRAME_UNDEFINED_VAL.

          If dw_value_type == DW_EXPR_OFFSET (the only  possible case for 
             dwarf2):
            If dw_offset_relevant is non-zero, then
               the value is stored at at the address 
               CFA+N where N is a signed offset. 
               dw_regnum is the cfa register rule which means
               one ignores dw_regnum and uses the CFA appropriately.
               So dw_offset_or_block_len is a signed value, really,
               and must be printed/evaluated as such.
               Rule: Offset(N)
            If dw_offset_relevant is zero, then the value of the register
               is the value of (DWARF) register number dw_regnum.
               Rule: register(R)
          If dw_value_type  == DW_EXPR_VAL_OFFSET
            the  value of this register is CFA +N where N is a signed offset.
            dw_regnum is the cfa register rule which means
            one ignores dw_regnum and uses the CFA appropriately.
            Rule: val_offset(N)
          If dw_value_type  == DW_EXPR_EXPRESSION
            The value of the register is the value at the address
            computed by evaluating the DWARF expression E.
            Rule: expression(E)
            The expression E byte stream is pointed to by dw_block_ptr.
            The expression length in bytes is given by
            dw_offset_or_block_len.
          If dw_value_type  == DW_EXPR_VAL_EXPRESSION
            The value of the register is the value
            computed by evaluating the DWARF expression E.
            Rule: val_expression(E)
            The expression E byte stream is pointed to by dw_block_ptr.
            The expression length in bytes is given by
            dw_offset_or_block_len.
          Other values of dw_value_type are an error.
        */
    Dwarf_Small         dw_offset_relevant;
    Dwarf_Small         dw_value_type; 
    Dwarf_Half          dw_regnum;
    Dwarf_Unsigned      dw_offset_or_block_len;
    Dwarf_Ptr           dw_block_ptr;

}Dwarf_Regtable_Entry3;

/* For the DWARF3 version, moved the DW_FRAME_CFA_COL
   out of the array and into its own struct.  
   Having it part of the array is not very easy to work
   with from a portability point of view: changing
   the number for every architecture is a pain (if one fails
   to set it correctly a register rule gets clobbered when
   setting CFA).  With MIPS it just happened to be easy to use 
   DW_FRAME_CFA_COL (it was wrong conceptually but it was easy...).

   rt3_rules and rt3_reg_table_size must be filled in before 
   calling libdwarf.  Filled in with a pointer to an array 
   (pointer and array  set up by the calling application) 
   of rt3_reg_table_size Dwarf_Regtable_Entry3_s structs.   
   libdwarf does not allocate or deallocate space for the
   rules, you must do so.   libdwarf will initialize the
   contents rules array, you do not need to do so (though
   if you choose to initialize the array somehow that is ok:
   libdwarf will overwrite your initializations with its own).

*/
typedef struct Dwarf_Regtable3_s {
    struct Dwarf_Regtable_Entry3_s   rt3_cfa_rule;

    Dwarf_Half                       rt3_reg_table_size;
    struct Dwarf_Regtable_Entry3_s * rt3_rules;
} Dwarf_Regtable3;


/* Use for DW_EPXR_STANDARD., DW_EXPR_VAL_OFFSET. 
   Returns DW_DLV_OK if the value is available.
   If DW_DLV_OK returns the regnum and offset thru the pointers
   (which the consumer must use appropriately). 
*/
int dwarf_frame_get_reg_register(struct Dwarf_Regtable_Entry3_s *reg_in,
    Dwarf_Small *offset_relevant,
    Dwarf_Half *regnum_out,
    Dwarf_Signed *offset_out);

/* Use for DW_EXPR_EXPRESSION, DW_EXPR_VAL_EXPRESSION.
   Returns DW_DLV_OK if the value is available.
   The caller must pass in the address of a valid
   Dwarf_Block (the caller need not initialize it).
*/
int dwarf_frame_get_reg_expression(struct Dwarf_Regtable_Entry3_s *reg_in,
    Dwarf_Block *block_out);


/* For DW_DLC_SYMBOLIC_RELOCATIONS output to caller 
   v2, adding drd_length: some relocations are 4 and
   some 8 bytes (pointers are 8, section offsets 4) in
   some dwarf environments. (MIPS relocations are all one
   size in any given ABI.) Changing drd_type to an unsigned char
   to keep struct size down.
*/
enum Dwarf_Rel_Type {
        dwarf_drt_none,        /* Should not get to caller */
        dwarf_drt_data_reloc,  /* Simple normal relocation. */
        dwarf_drt_segment_rel, /* Special reloc, exceptions. */
        /* dwarf_drt_first_of_length_pair  and drt_second 
           are for for the  .word end - begin case. */
        dwarf_drt_first_of_length_pair,
        dwarf_drt_second_of_length_pair
};

typedef struct Dwarf_P_Marker_s * Dwarf_P_Marker;
struct Dwarf_P_Marker_s {
    Dwarf_Unsigned ma_marker;
    Dwarf_Unsigned ma_offset;
};

typedef struct Dwarf_Relocation_Data_s  * Dwarf_Relocation_Data;
struct Dwarf_Relocation_Data_s {
    unsigned char drd_type;   /* Cast to/from Dwarf_Rel_Type
                               to keep size small in struct. */
    unsigned char drd_length; /* Length in bytes of data being 
                               relocated. 4 for 32bit data,
                               8 for 64bit data. */
    Dwarf_Unsigned       drd_offset; /* Where the data to reloc is. */
    Dwarf_Unsigned       drd_symbol_index;
};

typedef struct Dwarf_P_String_Attr_s  * Dwarf_P_String_Attr;
struct Dwarf_P_String_Attr_s {
    Dwarf_Unsigned        sa_offset;  /* Offset of string attribute data */
    Dwarf_Unsigned        sa_nbytes;  
};
    

/* Opaque types for Consumer Library. */
typedef struct Dwarf_Debug_s*      Dwarf_Debug;
typedef struct Dwarf_Die_s*        Dwarf_Die;
typedef struct Dwarf_Line_s*       Dwarf_Line;
typedef struct Dwarf_Global_s*     Dwarf_Global;
typedef struct Dwarf_Func_s*       Dwarf_Func;
typedef struct Dwarf_Type_s*       Dwarf_Type;
typedef struct Dwarf_Var_s*        Dwarf_Var;
typedef struct Dwarf_Weak_s*       Dwarf_Weak;
typedef struct Dwarf_Error_s*      Dwarf_Error;
typedef struct Dwarf_Attribute_s*  Dwarf_Attribute;
typedef struct Dwarf_Abbrev_s*       Dwarf_Abbrev;
typedef struct Dwarf_Fde_s*         Dwarf_Fde;
typedef struct Dwarf_Cie_s*         Dwarf_Cie;
typedef struct Dwarf_Arange_s*       Dwarf_Arange;

/* Opaque types for Producer Library. */
typedef struct Dwarf_P_Debug_s*           Dwarf_P_Debug;
typedef struct Dwarf_P_Die_s*           Dwarf_P_Die;
typedef struct Dwarf_P_Attribute_s*    Dwarf_P_Attribute;
typedef struct Dwarf_P_Fde_s*        Dwarf_P_Fde;
typedef struct Dwarf_P_Expr_s*        Dwarf_P_Expr;
typedef Dwarf_Unsigned                Dwarf_Tag;


/* error handler function
*/
typedef void  (*Dwarf_Handler)(Dwarf_Error /*error*/, Dwarf_Ptr /*errarg*/); 


/* Begin libdwarf Object File Interface declarations.

As of February 2008 there are multiple dwarf_reader object access
initialization methods available:
The traditional dwarf_elf_init() and dwarf_init()  and dwarf_finish() 
    which assume libelf and POSIX file access. 
An object-file and library agnostic dwarf_object_init() and dwarf_object_finish()
    which allow the coder to provide object access routines
    abstracting away the elf interface.  So there is no dependence in the
    reader code on the object format and no dependence on libelf.
    See the code in dwarf_elf_access.c  and dwarf_original_elf_init.c 
    to see an example of initializing the structures mentioned below.

Projects using dwarf_elf_init() or dwarf_init() can ignore
the Dwarf_Obj_Access* structures entirely as all these details
are completed for you.

*/

typedef struct Dwarf_Obj_Access_Interface_s   Dwarf_Obj_Access_Interface;
typedef struct Dwarf_Obj_Access_Methods_s     Dwarf_Obj_Access_Methods;
typedef struct Dwarf_Obj_Access_Section_s     Dwarf_Obj_Access_Section;


/* Used in the get_section interface function
   in Dwarf_Obj_Access_Section_s.  Since libdwarf
   depends on standard DWARF section names an object
   format that has no such names (but has some
   method of setting up 'sections equivalents')
   must arrange to return standard DWARF section
   names in the 'name' field.  libdwarf does
   not free the strings in 'name'. */
struct Dwarf_Obj_Access_Section_s {
    Dwarf_Addr     addr;
    Dwarf_Unsigned size;
    const char*    name;
    /* Set link to zero if it is meaningless.  If non-zero
       it should be a link to a rela section or from symtab
       to strtab.  In Elf it is sh_link. */
    Dwarf_Unsigned link;
};

/* Returned by the get_endianness function in 
   Dwarf_Obj_Access_Methods_s. */
typedef enum {
    DW_OBJECT_MSB,
    DW_OBJECT_LSB
} Dwarf_Endianness;

/* The functions we need to access object data from libdwarf are declared here.

   In these function pointer declarations
   'void *obj' is intended to be a pointer (the object field in  
   Dwarf_Obj_Access_Interface_s)
   that hides the library-specific and object-specific data that makes
   it possible to handle multiple object formats and multiple libraries. 
   It's not required that one handles multiple such in a single libdwarf
   archive/shared-library (but not ruled out either).
   See  dwarf_elf_object_access_internals_t and dwarf_elf_access.c
   for an example. 

*/
struct Dwarf_Obj_Access_Methods_s {
    /**
     * get_section_info
     *
     * Get address, size, and name info about a section.
     * 
     * Parameters
     * section_index - Zero-based index.
     * return_section - Pointer to a structure in which section info 
     *   will be placed.   Caller must provide a valid pointer to a
     *   structure area.  The structure's contents will be overwritten
     *   by the call to get_section_info.
     * error - A pointer to an integer in which an error code may be stored.
     *
     * Return
     * DW_DLV_OK - Everything ok.
     * DW_DLV_ERROR - Error occurred. Use 'error' to determine the 
     *    libdwarf defined error.
     * DW_DLV_NO_ENTRY - No such section.
     */
    int    (*get_section_info)(void* obj, Dwarf_Half section_index, 
        Dwarf_Obj_Access_Section* return_section, int* error);
    /**
     * get_byte_order
     *
     * Get whether the object file represented by this interface is big-endian 
     * (DW_OBJECT_MSB) or little endian (DW_OBJECT_LSB).
     *
     * Parameters
     * obj - Equivalent to 'this' in OO languages.
     *
     * Return
     * Endianness of object. Cannot fail.
     */
    Dwarf_Endianness  (*get_byte_order)(void* obj);
    /**
     * get_length_size
     *
     * Get the size of a length field in the underlying object file. 
     * libdwarf currently supports * 4 and 8 byte sizes, but may 
     * support larger in the future.
     * Perhaps the return type should be an enumeration?
     *
     * Parameters
     * obj - Equivalent to 'this' in OO languages.
     *
     * Return
     * Size of length. Cannot fail.
     */
    Dwarf_Small   (*get_length_size)(void* obj);
    /**
     * get_pointer_size
     *
     * Get the size of a pointer field in the underlying object file. 
     * libdwarf currently supports  4 and 8 byte sizes.
     * Perhaps the return type should be an enumeration?

     * Return
     * Size of pointer. Cannot fail.
     */
    Dwarf_Small   (*get_pointer_size)(void* obj);
    /**
     * get_section_count
     *
     * Get the number of sections in the object file.
     *
     * Parameters
     *
     * Return
     * Number of sections
     */
    Dwarf_Unsigned  (*get_section_count)(void* obj);
    /**
     * load_section
     *
     * Get a pointer to an array of bytes that represent the section.
     *
     * Parameters
     * section_index - Zero-based index.
     * return_data - The address of a pointer to which the section data block 
     *   will be assigned.
     * error - Pointer to an integer for returning libdwarf-defined 
     *   error numbers.
     *
     * Return
     * DW_DLV_OK - No error.
     * DW_DLV_ERROR - Error. Use 'error' to indicate a libdwarf-defined 
     *    error number.
     * DW_DLV_NO_ENTRY - No such section.
     */
    int    (*load_section)(void* obj, Dwarf_Half section_index, 
        Dwarf_Small** return_data, int* error);

   /**
    * relocate_a_section
    * If relocations are not supported leave this pointer NULL.
    *
    * Get a pointer to an array of bytes that represent the section.
    *
    * Parameters
    * section_index - Zero-based index of the section to be relocated.
    * error - Pointer to an integer for returning libdwarf-defined 
    *   error numbers.
    *
    * Return
    * DW_DLV_OK - No error.
    * DW_DLV_ERROR - Error. Use 'error' to indicate a libdwarf-defined 
    *    error number.
    * DW_DLV_NO_ENTRY - No such section.
    */
    int    (*relocate_a_section)(void* obj, Dwarf_Half section_index,
         Dwarf_Debug dbg,
         int* error);

};



/* These structures are allocated and deallocated by your code
   when you are using the libdwarf Object File Interface  
   [dwarf_object_init() and dwarf_object_finish()] directly.
   dwarf_object_finish() does not free
   struct Dwarf_Obj_Access_Interface_s or its content. 
   (libdwarf does record a pointer to this struct: you must
   ensure that pointer remains valid for as long as
   a libdwarf instance is open (meaning
   after dwarf_init() and before dwarf_finish()).

   If you are reading Elf objects and libelf use dwarf_init()
   or dwarf_elf_init() which take care of these details.
*/
struct Dwarf_Obj_Access_Interface_s {
    /* object is a void* as it hides the data the object access routines
       need (which varies by library in use and object format). 
    */
    void* object;
    const Dwarf_Obj_Access_Methods * methods;
};
  
/* End libdwarf Object File Interface */

/* 
    Dwarf_dealloc() alloc_type arguments.
    Argument points to:
*/
#define DW_DLA_STRING          0x01     /* char* */
#define DW_DLA_LOC             0x02     /* Dwarf_Loc */
#define DW_DLA_LOCDESC         0x03     /* Dwarf_Locdesc */
#define DW_DLA_ELLIST          0x04     /* Dwarf_Ellist (not used)*/
#define DW_DLA_BOUNDS          0x05     /* Dwarf_Bounds (not used) */
#define DW_DLA_BLOCK           0x06     /* Dwarf_Block */
#define DW_DLA_DEBUG           0x07     /* Dwarf_Debug */
#define DW_DLA_DIE             0x08     /* Dwarf_Die */
#define DW_DLA_LINE            0x09     /* Dwarf_Line */
#define DW_DLA_ATTR            0x0a     /* Dwarf_Attribute */
#define DW_DLA_TYPE            0x0b     /* Dwarf_Type  (not used) */
#define DW_DLA_SUBSCR          0x0c     /* Dwarf_Subscr (not used) */
#define DW_DLA_GLOBAL          0x0d     /* Dwarf_Global */
#define DW_DLA_ERROR           0x0e     /* Dwarf_Error */
#define DW_DLA_LIST            0x0f     /* a list */
#define DW_DLA_LINEBUF         0x10     /* Dwarf_Line* (not used) */
#define DW_DLA_ARANGE          0x11     /* Dwarf_Arange */
#define DW_DLA_ABBREV          0x12      /* Dwarf_Abbrev */
#define DW_DLA_FRAME_OP        0x13      /* Dwarf_Frame_Op */
#define DW_DLA_CIE             0x14     /* Dwarf_Cie */
#define DW_DLA_FDE             0x15     /* Dwarf_Fde */
#define DW_DLA_LOC_BLOCK       0x16     /* Dwarf_Loc Block (not used) */
#define DW_DLA_FRAME_BLOCK     0x17     /* Dwarf_Frame Block (not used) */
#define DW_DLA_FUNC            0x18     /* Dwarf_Func */
#define DW_DLA_TYPENAME        0x19     /* Dwarf_Type */
#define DW_DLA_VAR             0x1a     /* Dwarf_Var */
#define DW_DLA_WEAK            0x1b     /* Dwarf_Weak */
#define DW_DLA_ADDR            0x1c     /* Dwarf_Addr sized entries */
#define DW_DLA_RANGES          0x1d     /* Dwarf_Ranges */

/* The augmenter string for CIE */
#define DW_CIE_AUGMENTER_STRING_V0              "z"

/* dwarf_init() access arguments
*/
#define DW_DLC_READ        0        /* read only access */
#define DW_DLC_WRITE       1        /* write only access */
#define DW_DLC_RDWR        2        /* read/write access NOT SUPPORTED*/

/* pro_init() access flag modifiers
   If HAVE_DWARF2_99_EXTENSION is defined at libdwarf build time
   and DW_DLC_OFFSET_SIZE_64  is passed in pro_init() flags then the DWARF3 
   64 bit offset extension is used to generate 64 bit offsets.
*/
#define DW_DLC_SIZE_64     0x40000000 /* 32-bit address-size target */
#define DW_DLC_SIZE_32     0x20000000 /* 64-bit address-size target */
#define DW_DLC_OFFSET_SIZE_64 0x10000000 /* 64-bit offset-size DWARF */

/* dwarf_pro_init() access flag modifiers
*/
#define DW_DLC_ISA_MIPS             0x00000000 /* MIPS target */
#define DW_DLC_ISA_IA64             0x01000000 /* IA64 target */
#define DW_DLC_STREAM_RELOCATIONS   0x02000000 /* Old style binary relocs */

    /* Usable with assembly output because it is up to the producer to
       deal with locations in whatever manner the producer code wishes. 
       Possibly emitting text an assembler will recognize. */
#define DW_DLC_SYMBOLIC_RELOCATIONS 0x04000000 

#define DW_DLC_TARGET_BIGENDIAN     0x08000000 /* Big    endian target */
#define DW_DLC_TARGET_LITTLEENDIAN  0x00100000 /* Little endian target */

#if 0
  /*
   The libdwarf producer interfaces jumble these two semantics together in
   confusing ways.  We *should* have flags like these...
   But changing the code means a lot of diffs.  So for now,
   we leave things as they are 
  */
  #define DW_DLC_SUN_OFFSET32        0x00010000 /* use 32-bit sec offsets */
  #define DW_DLC_SUN_OFFSET64        0x00020000 /* use 64-bit sec offsets */
  #define DW_DLC_SUN_POINTER32        0x00040000 /* use 4 for address_size */
  #define DW_DLC_SUN_POINTER64        0x00080000 /* use 8 for address_size */
#endif

/* dwarf_pcline() slide arguments
*/
#define DW_DLS_BACKWARD   -1       /* slide backward to find line */
#define DW_DLS_NOSLIDE     0       /* match exactly without sliding */ 
#define DW_DLS_FORWARD     1       /* slide forward to find line */

/* libdwarf error numbers
*/
#define DW_DLE_NE          0     /* no error */ 
#define DW_DLE_VMM         1     /* dwarf format/library version mismatch */
#define DW_DLE_MAP         2     /* memory map failure */
#define DW_DLE_LEE         3     /* libelf error */
#define DW_DLE_NDS         4     /* no debug section */
#define DW_DLE_NLS         5     /* no line section */
#define DW_DLE_ID          6     /* invalid descriptor for query */
#define DW_DLE_IOF         7     /* I/O failure */
#define DW_DLE_MAF         8     /* memory allocation failure */
#define DW_DLE_IA          9     /* invalid argument */ 
#define DW_DLE_MDE         10     /* mangled debugging entry */
#define DW_DLE_MLE         11     /* mangled line number entry */
#define DW_DLE_FNO         12     /* file not open */
#define DW_DLE_FNR         13     /* file not a regular file */
#define DW_DLE_FWA         14     /* file open with wrong access */
#define DW_DLE_NOB         15     /* not an object file */
#define DW_DLE_MOF         16     /* mangled object file header */
#define DW_DLE_EOLL        17     /* end of location list entries */
#define DW_DLE_NOLL        18     /* no location list section */
#define DW_DLE_BADOFF      19     /* Invalid offset */
#define DW_DLE_EOS         20     /* end of section  */
#define DW_DLE_ATRUNC      21     /* abbreviations section appears truncated*/
#define DW_DLE_BADBITC     22     /* Address size passed to dwarf bad*/
                    /* It is not an allowed size (64 or 32) */
    /* Error codes defined by the current Libdwarf Implementation. */
#define DW_DLE_DBG_ALLOC                        23
#define DW_DLE_FSTAT_ERROR                      24
#define DW_DLE_FSTAT_MODE_ERROR                 25
#define DW_DLE_INIT_ACCESS_WRONG                26
#define DW_DLE_ELF_BEGIN_ERROR                  27
#define DW_DLE_ELF_GETEHDR_ERROR                28
#define DW_DLE_ELF_GETSHDR_ERROR                29
#define DW_DLE_ELF_STRPTR_ERROR                 30
#define DW_DLE_DEBUG_INFO_DUPLICATE             31
#define DW_DLE_DEBUG_INFO_NULL                  32
#define DW_DLE_DEBUG_ABBREV_DUPLICATE           33
#define DW_DLE_DEBUG_ABBREV_NULL                34
#define DW_DLE_DEBUG_ARANGES_DUPLICATE          35
#define DW_DLE_DEBUG_ARANGES_NULL               36
#define DW_DLE_DEBUG_LINE_DUPLICATE             37
#define DW_DLE_DEBUG_LINE_NULL                  38
#define DW_DLE_DEBUG_LOC_DUPLICATE              39
#define DW_DLE_DEBUG_LOC_NULL                   40
#define DW_DLE_DEBUG_MACINFO_DUPLICATE          41
#define DW_DLE_DEBUG_MACINFO_NULL               42
#define DW_DLE_DEBUG_PUBNAMES_DUPLICATE         43
#define DW_DLE_DEBUG_PUBNAMES_NULL              44
#define DW_DLE_DEBUG_STR_DUPLICATE              45
#define DW_DLE_DEBUG_STR_NULL                   46
#define DW_DLE_CU_LENGTH_ERROR                  47
#define DW_DLE_VERSION_STAMP_ERROR              48
#define DW_DLE_ABBREV_OFFSET_ERROR              49
#define DW_DLE_ADDRESS_SIZE_ERROR               50
#define DW_DLE_DEBUG_INFO_PTR_NULL              51
#define DW_DLE_DIE_NULL                         52
#define DW_DLE_STRING_OFFSET_BAD                53
#define DW_DLE_DEBUG_LINE_LENGTH_BAD            54
#define DW_DLE_LINE_PROLOG_LENGTH_BAD           55
#define DW_DLE_LINE_NUM_OPERANDS_BAD            56
#define DW_DLE_LINE_SET_ADDR_ERROR              57 /* No longer used. */
#define DW_DLE_LINE_EXT_OPCODE_BAD              58
#define DW_DLE_DWARF_LINE_NULL                  59
#define DW_DLE_INCL_DIR_NUM_BAD                 60
#define DW_DLE_LINE_FILE_NUM_BAD                61
#define DW_DLE_ALLOC_FAIL                       62
#define DW_DLE_NO_CALLBACK_FUNC                 63
#define DW_DLE_SECT_ALLOC                       64
#define DW_DLE_FILE_ENTRY_ALLOC                 65
#define DW_DLE_LINE_ALLOC                       66
#define DW_DLE_FPGM_ALLOC                       67
#define DW_DLE_INCDIR_ALLOC                     68
#define DW_DLE_STRING_ALLOC                     69
#define DW_DLE_CHUNK_ALLOC                      70
#define DW_DLE_BYTEOFF_ERR                      71
#define DW_DLE_CIE_ALLOC                        72
#define DW_DLE_FDE_ALLOC                        73
#define DW_DLE_REGNO_OVFL                       74
#define DW_DLE_CIE_OFFS_ALLOC                   75
#define DW_DLE_WRONG_ADDRESS                    76
#define DW_DLE_EXTRA_NEIGHBORS                  77
#define    DW_DLE_WRONG_TAG                     78
#define DW_DLE_DIE_ALLOC                        79
#define DW_DLE_PARENT_EXISTS                    80
#define DW_DLE_DBG_NULL                         81
#define DW_DLE_DEBUGLINE_ERROR                  82
#define DW_DLE_DEBUGFRAME_ERROR                 83
#define DW_DLE_DEBUGINFO_ERROR                  84
#define DW_DLE_ATTR_ALLOC                       85
#define DW_DLE_ABBREV_ALLOC                     86
#define DW_DLE_OFFSET_UFLW                      87
#define DW_DLE_ELF_SECT_ERR                     88
#define DW_DLE_DEBUG_FRAME_LENGTH_BAD           89
#define DW_DLE_FRAME_VERSION_BAD                90
#define DW_DLE_CIE_RET_ADDR_REG_ERROR           91
#define DW_DLE_FDE_NULL                         92
#define DW_DLE_FDE_DBG_NULL                     93
#define DW_DLE_CIE_NULL                         94
#define DW_DLE_CIE_DBG_NULL                     95
#define DW_DLE_FRAME_TABLE_COL_BAD              96
#define DW_DLE_PC_NOT_IN_FDE_RANGE              97
#define DW_DLE_CIE_INSTR_EXEC_ERROR             98
#define DW_DLE_FRAME_INSTR_EXEC_ERROR           99
#define DW_DLE_FDE_PTR_NULL                    100
#define DW_DLE_RET_OP_LIST_NULL                101
#define DW_DLE_LINE_CONTEXT_NULL               102
#define DW_DLE_DBG_NO_CU_CONTEXT               103
#define DW_DLE_DIE_NO_CU_CONTEXT               104
#define DW_DLE_FIRST_DIE_NOT_CU                105
#define DW_DLE_NEXT_DIE_PTR_NULL               106
#define DW_DLE_DEBUG_FRAME_DUPLICATE           107
#define DW_DLE_DEBUG_FRAME_NULL                108
#define DW_DLE_ABBREV_DECODE_ERROR             109
#define DW_DLE_DWARF_ABBREV_NULL               110
#define DW_DLE_ATTR_NULL                       111
#define DW_DLE_DIE_BAD                         112
#define DW_DLE_DIE_ABBREV_BAD                  113
#define DW_DLE_ATTR_FORM_BAD                   114
#define DW_DLE_ATTR_NO_CU_CONTEXT              115
#define DW_DLE_ATTR_FORM_SIZE_BAD              116
#define DW_DLE_ATTR_DBG_NULL                   117
#define DW_DLE_BAD_REF_FORM                    118
#define DW_DLE_ATTR_FORM_OFFSET_BAD            119
#define DW_DLE_LINE_OFFSET_BAD                 120
#define DW_DLE_DEBUG_STR_OFFSET_BAD            121
#define DW_DLE_STRING_PTR_NULL                 122
#define DW_DLE_PUBNAMES_VERSION_ERROR          123
#define DW_DLE_PUBNAMES_LENGTH_BAD             124
#define DW_DLE_GLOBAL_NULL                     125
#define DW_DLE_GLOBAL_CONTEXT_NULL             126
#define DW_DLE_DIR_INDEX_BAD                   127
#define DW_DLE_LOC_EXPR_BAD                    128
#define DW_DLE_DIE_LOC_EXPR_BAD                129
#define DW_DLE_ADDR_ALLOC                      130
#define DW_DLE_OFFSET_BAD                      131
#define DW_DLE_MAKE_CU_CONTEXT_FAIL            132
#define DW_DLE_REL_ALLOC                       133
#define DW_DLE_ARANGE_OFFSET_BAD               134
#define DW_DLE_SEGMENT_SIZE_BAD                135
#define DW_DLE_ARANGE_LENGTH_BAD               136
#define DW_DLE_ARANGE_DECODE_ERROR             137
#define DW_DLE_ARANGES_NULL                    138
#define DW_DLE_ARANGE_NULL                     139
#define DW_DLE_NO_FILE_NAME                    140
#define DW_DLE_NO_COMP_DIR                     141
#define DW_DLE_CU_ADDRESS_SIZE_BAD             142
#define DW_DLE_INPUT_ATTR_BAD                  143
#define DW_DLE_EXPR_NULL                       144
#define DW_DLE_BAD_EXPR_OPCODE                 145
#define DW_DLE_EXPR_LENGTH_BAD                 146
#define DW_DLE_MULTIPLE_RELOC_IN_EXPR          147
#define DW_DLE_ELF_GETIDENT_ERROR              148
#define DW_DLE_NO_AT_MIPS_FDE                  149
#define DW_DLE_NO_CIE_FOR_FDE                  150
#define DW_DLE_DIE_ABBREV_LIST_NULL            151
#define DW_DLE_DEBUG_FUNCNAMES_DUPLICATE       152
#define DW_DLE_DEBUG_FUNCNAMES_NULL            153
#define DW_DLE_DEBUG_FUNCNAMES_VERSION_ERROR   154
#define DW_DLE_DEBUG_FUNCNAMES_LENGTH_BAD      155
#define DW_DLE_FUNC_NULL                       156
#define DW_DLE_FUNC_CONTEXT_NULL               157
#define DW_DLE_DEBUG_TYPENAMES_DUPLICATE       158
#define DW_DLE_DEBUG_TYPENAMES_NULL            159
#define DW_DLE_DEBUG_TYPENAMES_VERSION_ERROR   160
#define DW_DLE_DEBUG_TYPENAMES_LENGTH_BAD      161
#define DW_DLE_TYPE_NULL                       162
#define DW_DLE_TYPE_CONTEXT_NULL               163
#define DW_DLE_DEBUG_VARNAMES_DUPLICATE        164
#define DW_DLE_DEBUG_VARNAMES_NULL             165
#define DW_DLE_DEBUG_VARNAMES_VERSION_ERROR    166
#define DW_DLE_DEBUG_VARNAMES_LENGTH_BAD       167
#define DW_DLE_VAR_NULL                        168
#define DW_DLE_VAR_CONTEXT_NULL                169
#define DW_DLE_DEBUG_WEAKNAMES_DUPLICATE       170
#define DW_DLE_DEBUG_WEAKNAMES_NULL            171
#define DW_DLE_DEBUG_WEAKNAMES_VERSION_ERROR   172
#define DW_DLE_DEBUG_WEAKNAMES_LENGTH_BAD      173
#define DW_DLE_WEAK_NULL                       174
#define DW_DLE_WEAK_CONTEXT_NULL               175
#define DW_DLE_LOCDESC_COUNT_WRONG             176
#define DW_DLE_MACINFO_STRING_NULL             177
#define DW_DLE_MACINFO_STRING_EMPTY            178
#define DW_DLE_MACINFO_INTERNAL_ERROR_SPACE    179
#define DW_DLE_MACINFO_MALLOC_FAIL             180
#define DW_DLE_DEBUGMACINFO_ERROR              181
#define DW_DLE_DEBUG_MACRO_LENGTH_BAD          182
#define DW_DLE_DEBUG_MACRO_MAX_BAD             183
#define DW_DLE_DEBUG_MACRO_INTERNAL_ERR        184
#define DW_DLE_DEBUG_MACRO_MALLOC_SPACE        185
#define DW_DLE_DEBUG_MACRO_INCONSISTENT        186
#define DW_DLE_DF_NO_CIE_AUGMENTATION          187
#define DW_DLE_DF_REG_NUM_TOO_HIGH             188 
#define DW_DLE_DF_MAKE_INSTR_NO_INIT           189 
#define DW_DLE_DF_NEW_LOC_LESS_OLD_LOC         190
#define DW_DLE_DF_POP_EMPTY_STACK              191
#define DW_DLE_DF_ALLOC_FAIL                   192
#define DW_DLE_DF_FRAME_DECODING_ERROR         193
#define DW_DLE_DEBUG_LOC_SECTION_SHORT         194
#define DW_DLE_FRAME_AUGMENTATION_UNKNOWN      195
#define DW_DLE_PUBTYPE_CONTEXT                 196 /* Unused. */
#define DW_DLE_DEBUG_PUBTYPES_LENGTH_BAD       197
#define DW_DLE_DEBUG_PUBTYPES_VERSION_ERROR    198
#define DW_DLE_DEBUG_PUBTYPES_DUPLICATE        199
#define DW_DLE_FRAME_CIE_DECODE_ERROR          200
#define DW_DLE_FRAME_REGISTER_UNREPRESENTABLE  201
#define DW_DLE_FRAME_REGISTER_COUNT_MISMATCH   202
#define DW_DLE_LINK_LOOP                       203
#define DW_DLE_STRP_OFFSET_BAD                 204
#define DW_DLE_DEBUG_RANGES_DUPLICATE          205
#define DW_DLE_DEBUG_RANGES_OFFSET_BAD         206
#define DW_DLE_DEBUG_RANGES_MISSING_END        207
#define DW_DLE_DEBUG_RANGES_OUT_OF_MEM         208
#define DW_DLE_DEBUG_SYMTAB_ERR                209
#define DW_DLE_DEBUG_STRTAB_ERR                210
#define DW_DLE_RELOC_MISMATCH_INDEX            211
#define DW_DLE_RELOC_MISMATCH_RELOC_INDEX      212
#define DW_DLE_RELOC_MISMATCH_STRTAB_INDEX     213
#define DW_DLE_RELOC_SECTION_MISMATCH          214
#define DW_DLE_RELOC_SECTION_MISSING_INDEX     215
#define DW_DLE_RELOC_SECTION_LENGTH_ODD        216
#define DW_DLE_RELOC_SECTION_PTR_NULL          217
#define DW_DLE_RELOC_SECTION_MALLOC_FAIL       218
#define DW_DLE_NO_ELF64_SUPPORT                219
#define DW_DLE_MISSING_ELF64_SUPPORT           220
#define DW_DLE_ORPHAN_FDE                      221
#define DW_DLE_DUPLICATE_INST_BLOCK            222
#define DW_DLE_BAD_REF_SIG8_FORM               223
#define DW_DLE_ATTR_EXPRLOC_FORM_BAD           224
#define DW_DLE_FORM_SEC_OFFSET_LENGTH_BAD      225
#define DW_DLE_NOT_REF_FORM                    226
#define DW_DLE_DEBUG_FRAME_LENGTH_NOT_MULTIPLE 227



    /* DW_DLE_LAST MUST EQUAL LAST ERROR NUMBER */
#define DW_DLE_LAST        227
#define DW_DLE_LO_USER     0x10000

   /* Taken as meaning 'undefined value', this is not
      a column or register number.
      Only present at libdwarf runtime. Never on disk.
      DW_FRAME_* Values present on disk are in dwarf.h
   */
#define DW_FRAME_UNDEFINED_VAL          1034

   /* Taken as meaning 'same value' as caller had, not a column
      or register number
      Only present at libdwarf runtime. Never on disk.
      DW_FRAME_* Values present on disk are in dwarf.h
   */
#define DW_FRAME_SAME_VAL               1035



/* error return values  
*/
#define DW_DLV_BADADDR     (~(Dwarf_Addr)0)   
    /* for functions returning target address */

#define DW_DLV_NOCOUNT     ((Dwarf_Signed)-1) 
    /* for functions returning count */

#define DW_DLV_BADOFFSET   (~(Dwarf_Off)0)    
    /* for functions returning offset */

/* standard return values for functions */
#define DW_DLV_NO_ENTRY -1
#define DW_DLV_OK        0
#define DW_DLV_ERROR     1

/* Special values for offset_into_exception_table field of dwarf fde's. */
/* The following value indicates that there is no Exception table offset
   associated with a dwarf frame. */
#define DW_DLX_NO_EH_OFFSET         (-1LL)
/* The following value indicates that the producer was unable to analyse the
   source file to generate Exception tables for this function. */
#define DW_DLX_EH_OFFSET_UNAVAILABLE  (-2LL)


/*===========================================================================*/
/*  Dwarf consumer interface initialization and termination operations */

/* Initialization based on Unix open fd (using libelf internally). */
int dwarf_init(int    /*fd*/, 
    Dwarf_Unsigned    /*access*/, 
    Dwarf_Handler     /*errhand*/, 
    Dwarf_Ptr         /*errarg*/, 
    Dwarf_Debug*      /*dbg*/,
    Dwarf_Error*      /*error*/);

/* Initialization based on libelf/sgi-fastlibelf open pointer. */
int dwarf_elf_init(dwarf_elf_handle /*elf*/,
    Dwarf_Unsigned    /*access*/, 
    Dwarf_Handler     /*errhand*/, 
    Dwarf_Ptr         /*errarg*/, 
    Dwarf_Debug*      /*dbg*/,
    Dwarf_Error*      /*error*/);

/* Undocumented function for memory allocator. */
void dwarf_print_memory_stats(Dwarf_Debug  /*dbg*/);

int dwarf_get_elf(Dwarf_Debug /*dbg*/,
    dwarf_elf_handle* /*return_elfptr*/,
    Dwarf_Error*      /*error*/);

int dwarf_finish(Dwarf_Debug /*dbg*/, Dwarf_Error* /*error*/);


int dwarf_object_init(Dwarf_Obj_Access_Interface* /* obj */,
    Dwarf_Handler /* errhand */,
    Dwarf_Ptr     /* errarg */,
    Dwarf_Debug*  /* dbg */,
    Dwarf_Error*  /* error */);

int dwarf_object_finish(Dwarf_Debug /* dbg */,
    Dwarf_Error* /* error */);
  
/* die traversal operations */
int dwarf_next_cu_header_b(Dwarf_Debug /*dbg*/, 
    Dwarf_Unsigned* /*cu_header_length*/, 
    Dwarf_Half*     /*version_stamp*/, 
    Dwarf_Off*      /*abbrev_offset*/, 
    Dwarf_Half*     /*address_size*/, 
    Dwarf_Half*     /*length_size*/, 
    Dwarf_Half*     /*extension_size*/, 
    Dwarf_Unsigned* /*next_cu_header_offset*/,
    Dwarf_Error*    /*error*/);
/* The following is now obsolete, though supported. November 2009. */
int dwarf_next_cu_header(Dwarf_Debug /*dbg*/, 
    Dwarf_Unsigned* /*cu_header_length*/, 
    Dwarf_Half*     /*version_stamp*/, 
    Dwarf_Off*      /*abbrev_offset*/, 
    Dwarf_Half*     /*address_size*/, 
    Dwarf_Unsigned* /*next_cu_header_offset*/,
    Dwarf_Error*    /*error*/);

int dwarf_siblingof(Dwarf_Debug /*dbg*/, 
    Dwarf_Die        /*die*/, 
    Dwarf_Die*       /*return_siblingdie*/,
    Dwarf_Error*     /*error*/);

int dwarf_child(Dwarf_Die /*die*/, 
    Dwarf_Die*       /*return_childdie*/,
    Dwarf_Error*     /*error*/);

/* Finding die given global (not CU-relative) offset */
int dwarf_offdie(Dwarf_Debug /*dbg*/, 
    Dwarf_Off        /*offset*/, 
    Dwarf_Die*       /*return_die*/,
    Dwarf_Error*     /*error*/);

/* Higher level functions (Unimplemented) */
int dwarf_pcfile(Dwarf_Debug /*dbg*/, 
    Dwarf_Addr       /*pc*/, 
    Dwarf_Die*       /*return_die*/,
    Dwarf_Error*     /*error*/);

/* Unimplemented */
int dwarf_pcsubr(Dwarf_Debug /*dbg*/, 
    Dwarf_Addr       /*pc*/, 
    Dwarf_Die*       /*return_die*/,
    Dwarf_Error*     /*error*/);

/* Unimplemented */
int dwarf_pcscope(Dwarf_Debug /*dbg*/, 
    Dwarf_Addr       /*pc*/, 
    Dwarf_Die*       /*return_die*/,
    Dwarf_Error*     /*error*/);

/* operations on DIEs */
int dwarf_tag(Dwarf_Die /*die*/, 
    Dwarf_Half*      /*return_tag*/,
    Dwarf_Error*     /*error*/);

/* utility? */
/* dwarf_dieoffset returns the global debug_info
   section offset, not the CU relative offset. */
int dwarf_dieoffset(Dwarf_Die /*die*/, 
    Dwarf_Off*       /*return_offset*/,
    Dwarf_Error*     /*error*/);

/* dwarf_CU_dieoffset_given_die returns
   the global debug_info section offset of the CU die
   that is the CU containing the given_die
   (the passed in DIE can be any DIE). 
   This information makes it possible for a consumer to
   find and print CU context information for any die. 
   See also dwarf_get_cu_die_offset_given_cu_header_offset(). */
int dwarf_CU_dieoffset_given_die(Dwarf_Die /*given_die*/, 
    Dwarf_Off*       /*return_offset*/,
    Dwarf_Error*     /*error*/);

/* dwarf_die_CU_offset returns the CU relative offset
   not the global debug_info section offset, given
   any DIE in the CU.  See also dwarf_CU_dieoffset_given_die().
   */
int dwarf_die_CU_offset(Dwarf_Die /*die*/,
    Dwarf_Off*       /*return_offset*/,
    Dwarf_Error*     /*error*/);

int dwarf_die_CU_offset_range(Dwarf_Die /*die*/,
    Dwarf_Off*       /*return_CU_header_offset*/,
    Dwarf_Off*       /*return_CU_length_bytes*/,
    Dwarf_Error*     /*error*/);

int dwarf_attr (Dwarf_Die /*die*/, 
    Dwarf_Half        /*attr*/, 
    Dwarf_Attribute * /*returned_attr*/,
    Dwarf_Error*      /*error*/);

int dwarf_diename(Dwarf_Die /*die*/, 
    char   **        /*diename*/,
    Dwarf_Error*     /*error*/);

/* Returns the  abbrev code of the die. Cannot fail. */
int dwarf_die_abbrev_code(Dwarf_Die /*die */);


/* convenience functions, alternative to using dwarf_attrlist() */
int dwarf_hasattr(Dwarf_Die /*die*/, 
    Dwarf_Half       /*attr*/, 
    Dwarf_Bool *     /*returned_bool*/,
    Dwarf_Error*     /*error*/);

/* dwarf_loclist_n preferred over dwarf_loclist */
int dwarf_loclist_n(Dwarf_Attribute /*attr*/,  
    Dwarf_Locdesc*** /*llbuf*/, 
    Dwarf_Signed *   /*locCount*/,
    Dwarf_Error*     /*error*/);

int dwarf_loclist(Dwarf_Attribute /*attr*/,  /* inflexible! */
    Dwarf_Locdesc**  /*llbuf*/, 
    Dwarf_Signed *   /*locCount*/,
    Dwarf_Error*     /*error*/);

/* Extracts a dwarf expression from an expression byte stream.
   Useful to get expressions from DW_CFA_def_cfa_expression
   DW_CFA_expression DW_CFA_val_expression expression bytes.
   27 April 2009: dwarf_loclist_from_expr() interface with
   no addr_size is obsolete but supported, 
   use dwarf_loclist_from_expr_a() instead.  
*/
int dwarf_loclist_from_expr(Dwarf_Debug dbg,
    Dwarf_Ptr expression_in,
    Dwarf_Unsigned expression_length,
    Dwarf_Locdesc ** llbuf,
    Dwarf_Signed * listlen, Dwarf_Error * error);

/* dwarf_loclist_from_expr_a() new 27 Apr 2009: added addr_size argument. */
int dwarf_loclist_from_expr_a(Dwarf_Debug dbg,
    Dwarf_Ptr expression_in,
    Dwarf_Unsigned expression_length,
    Dwarf_Half addr_size,
    Dwarf_Locdesc ** llbuf,
    Dwarf_Signed * listlen, Dwarf_Error * error);

/* Unimplemented */
int dwarf_stringlen(Dwarf_Die /*die*/, 
    Dwarf_Locdesc ** /*returned_locdesc*/,
    Dwarf_Error*     /*error*/);

/* Unimplemented */
int dwarf_subscrcnt(Dwarf_Die /*die*/, 
    Dwarf_Signed *   /*returned_count*/,
    Dwarf_Error*     /*error*/);

/* Unimplemented */
int dwarf_nthsubscr(Dwarf_Die /*die*/, 
    Dwarf_Unsigned   /*ssndx*/, 
    Dwarf_Die *      /*returned_die*/,
    Dwarf_Error*     /*error*/);

int dwarf_lowpc(Dwarf_Die /*die*/, 
    Dwarf_Addr  *    /*returned_addr*/,
    Dwarf_Error*     /*error*/);

int dwarf_highpc(Dwarf_Die /*die*/, 
    Dwarf_Addr  *    /*returned_addr*/,
    Dwarf_Error*     /*error*/);

int dwarf_bytesize(Dwarf_Die /*die*/, 
    Dwarf_Unsigned * /*returned_size*/,
    Dwarf_Error*     /*error*/);

/* Unimplemented */
int dwarf_isbitfield(Dwarf_Die /*die*/, 
    Dwarf_Bool  *    /*returned_bool*/,
    Dwarf_Error*     /*error*/);

int dwarf_bitsize(Dwarf_Die /*die*/, 
    Dwarf_Unsigned * /*returned_size*/,
    Dwarf_Error*     /*error*/);

int dwarf_bitoffset(Dwarf_Die /*die*/, 
    Dwarf_Unsigned * /*returned_offset*/,
    Dwarf_Error*     /*error*/);

int dwarf_srclang(Dwarf_Die /*die*/, 
    Dwarf_Unsigned * /*returned_lang*/,
    Dwarf_Error*     /*error*/);

int dwarf_arrayorder(Dwarf_Die /*die*/, 
    Dwarf_Unsigned * /*returned_order*/,
    Dwarf_Error*     /*error*/);

/* end of convenience function list */

/* this is the main interface to attributes of a DIE */
int dwarf_attrlist(Dwarf_Die /*die*/, 
    Dwarf_Attribute** /*attrbuf*/, 
    Dwarf_Signed   * /*attrcount*/,
    Dwarf_Error*     /*error*/);

/* query operations for attributes */
int dwarf_hasform(Dwarf_Attribute /*attr*/, 
    Dwarf_Half       /*form*/, 
    Dwarf_Bool *     /*returned_bool*/,
    Dwarf_Error*     /*error*/);

int dwarf_whatform(Dwarf_Attribute /*attr*/, 
    Dwarf_Half *     /*returned_form*/,
    Dwarf_Error*     /*error*/);

int dwarf_whatform_direct(Dwarf_Attribute /*attr*/, 
    Dwarf_Half *     /*returned_form*/,
    Dwarf_Error*     /*error*/);

int dwarf_whatattr(Dwarf_Attribute /*attr*/, 
    Dwarf_Half *     /*returned_attr_num*/,
    Dwarf_Error*     /*error*/);

/* 
    The following are concerned with the Primary Interface: getting
    the actual data values. One function per 'kind' of FORM.
*/
/*  dwarf_formref returns, thru return_offset, a CU-relative offset
    and does not allow DW_FORM_ref_addr*/
int dwarf_formref(Dwarf_Attribute /*attr*/, 
    Dwarf_Off*       /*return_offset*/,
    Dwarf_Error*     /*error*/);
/*  dwarf_global_formref returns, thru return_offset, 
    a debug_info-relative offset and does allow all reference forms*/
int dwarf_global_formref(Dwarf_Attribute /*attr*/, 
    Dwarf_Off*       /*return_offset*/,
    Dwarf_Error*     /*error*/);

/*  dwarf_formsig8 returns in the caller-provided 8 byte area
    the 8 bytes of a DW_FORM_ref_sig8.  Not a string.  */
int dwarf_formsig8(Dwarf_Attribute /*attr*/,
    Dwarf_Sig8 * /*returned sig bytes*/,
    Dwarf_Error*     /*error*/);

int dwarf_formaddr(Dwarf_Attribute /*attr*/, 
    Dwarf_Addr   *   /*returned_addr*/,
    Dwarf_Error*     /*error*/);

int dwarf_formflag(Dwarf_Attribute /*attr*/,
    Dwarf_Bool *     /*returned_bool*/,
    Dwarf_Error*     /*error*/);

int dwarf_formudata(Dwarf_Attribute /*attr*/, 
    Dwarf_Unsigned  * /*returned_val*/,
    Dwarf_Error*     /*error*/);

int dwarf_formsdata(Dwarf_Attribute     /*attr*/, 
    Dwarf_Signed  *  /*returned_val*/,
    Dwarf_Error*     /*error*/);

int dwarf_formblock(Dwarf_Attribute /*attr*/, 
    Dwarf_Block    ** /*returned_block*/,
    Dwarf_Error*     /*error*/);

int dwarf_formstring(Dwarf_Attribute /*attr*/, 
    char   **        /*returned_string*/,
    Dwarf_Error*     /*error*/);

int dwarf_formexprloc(Dwarf_Attribute /*attr*/,
    Dwarf_Unsigned * /*return_exprlen*/,
    Dwarf_Ptr  * /*block_ptr*/,
    Dwarf_Error * /*error*/);


/* end attribute query operations. */

/* line number operations */
/* dwarf_srclines  is the normal interface */
int dwarf_srclines(Dwarf_Die /*die*/, 
    Dwarf_Line**     /*linebuf*/, 
    Dwarf_Signed *   /*linecount*/,
    Dwarf_Error*     /*error*/);

/* dwarf_srclines_dealloc, created July 2005, is the new
   method for deallocating what dwarf_srclines returns.
   More complete free than using dwarf_dealloc directly. */
void dwarf_srclines_dealloc(Dwarf_Debug /*dbg*/, 
    Dwarf_Line*       /*linebuf*/,
    Dwarf_Signed      /*count */);


int dwarf_srcfiles(Dwarf_Die /*die*/, 
    char***          /*srcfiles*/, 
    Dwarf_Signed *   /*filecount*/,
    Dwarf_Error*     /*error*/);

/* Unimplemented. */
int dwarf_dieline(Dwarf_Die /*die*/, 
    Dwarf_Line  *    /*returned_line*/,
    Dwarf_Error *    /*error*/);

int dwarf_linebeginstatement(Dwarf_Line /*line*/, 
    Dwarf_Bool  *    /*returned_bool*/,
    Dwarf_Error*     /*error*/);

int dwarf_lineendsequence(Dwarf_Line /*line*/,
    Dwarf_Bool  *    /*returned_bool*/,
    Dwarf_Error*     /*error*/);

int dwarf_lineno(Dwarf_Line /*line*/, 
    Dwarf_Unsigned * /*returned_lineno*/,
    Dwarf_Error*     /*error*/);

int dwarf_line_srcfileno(Dwarf_Line /*line*/,
    Dwarf_Unsigned * /*ret_fileno*/, 
    Dwarf_Error *    /*error*/);

int dwarf_lineaddr(Dwarf_Line /*line*/, 
    Dwarf_Addr *     /*returned_addr*/,
    Dwarf_Error*     /*error*/);

int dwarf_lineoff(Dwarf_Line /*line*/, 
    Dwarf_Signed  *  /*returned_lineoffset*/,
    Dwarf_Error*     /*error*/);

int dwarf_linesrc(Dwarf_Line /*line*/, 
    char   **        /*returned_name*/,
    Dwarf_Error*     /*error*/);

int dwarf_lineblock(Dwarf_Line /*line*/, 
    Dwarf_Bool  *    /*returned_bool*/,
    Dwarf_Error*     /*error*/);

/* tertiary interface to line info */
/* Unimplemented */
int dwarf_pclines(Dwarf_Debug /*dbg*/, 
    Dwarf_Addr       /*pc*/, 
    Dwarf_Line**     /*linebuf*/, 
    Dwarf_Signed *   /*linecount*/,
    Dwarf_Signed     /*slide*/, 
    Dwarf_Error*     /*error*/);
/* end line number operations */

/* global name space operations (.debug_pubnames access) */
int dwarf_get_globals(Dwarf_Debug /*dbg*/, 
    Dwarf_Global**   /*globals*/, 
    Dwarf_Signed *   /*number_of_globals*/,
    Dwarf_Error*     /*error*/);
void dwarf_globals_dealloc(Dwarf_Debug /*dbg*/,
    Dwarf_Global*    /*globals*/,
    Dwarf_Signed     /*number_of_globals*/);

int dwarf_globname(Dwarf_Global /*glob*/, 
    char   **        /*returned_name*/,
    Dwarf_Error*     /*error*/);

int dwarf_global_die_offset(Dwarf_Global /*global*/, 
    Dwarf_Off*       /*return_offset*/,
    Dwarf_Error *    /*error*/);

/* This returns the CU die global offset if one knows the
   CU header global offset.
   See also dwarf_CU_dieoffset_given_die(). */ 
int dwarf_get_cu_die_offset_given_cu_header_offset(
    Dwarf_Debug      /*dbg*/,
    Dwarf_Off        /*in_cu_header_offset*/,
    Dwarf_Off *  /*out_cu_die_offset*/, 
    Dwarf_Error *    /*err*/);
#ifdef __sgi /* pragma is sgi MIPS only */
#pragma optional dwarf_get_cu_die_offset_given_cu_header_offset
#endif

int dwarf_global_cu_offset(Dwarf_Global /*global*/, 
    Dwarf_Off*       /*return_offset*/,
    Dwarf_Error*     /*error*/);

int dwarf_global_name_offsets(Dwarf_Global /*global*/, 
    char   **        /*returned_name*/,
    Dwarf_Off*       /*die_offset*/, 
    Dwarf_Off*       /*cu_offset*/, 
    Dwarf_Error*     /*error*/);

/* Static function name operations.  */
int dwarf_get_funcs(Dwarf_Debug    /*dbg*/,
    Dwarf_Func**     /*funcs*/,
    Dwarf_Signed *   /*number_of_funcs*/,
    Dwarf_Error*     /*error*/);
void dwarf_funcs_dealloc(Dwarf_Debug /*dbg*/,
    Dwarf_Func*      /*funcs*/,
    Dwarf_Signed     /*number_of_funcs*/);

int dwarf_funcname(Dwarf_Func /*func*/,
    char   **        /*returned_name*/,
    Dwarf_Error*     /*error*/);

int dwarf_func_die_offset(Dwarf_Func /*func*/,
    Dwarf_Off*       /*return_offset*/,
    Dwarf_Error*     /*error*/);

int dwarf_func_cu_offset(Dwarf_Func /*func*/,
    Dwarf_Off*       /*return_offset*/,
    Dwarf_Error*     /*error*/);

int dwarf_func_name_offsets(Dwarf_Func /*func*/,
    char   **        /*returned_name*/,
    Dwarf_Off*       /*die_offset*/,
    Dwarf_Off*       /*cu_offset*/,
    Dwarf_Error*     /*error*/);

/* User-defined type name operations, SGI IRIX .debug_typenames section.
   Same content as DWARF3 .debug_pubtypes, but defined years before
   .debug_pubtypes was defined.   SGI IRIX only. */
int dwarf_get_types(Dwarf_Debug    /*dbg*/,
    Dwarf_Type**     /*types*/,
    Dwarf_Signed *   /*number_of_types*/,
    Dwarf_Error*     /*error*/);
void dwarf_types_dealloc(Dwarf_Debug /*dbg*/,
    Dwarf_Type*      /*types*/,
    Dwarf_Signed     /*number_of_types*/);


int dwarf_typename(Dwarf_Type /*type*/,
    char   **        /*returned_name*/,
    Dwarf_Error*     /*error*/);

int dwarf_type_die_offset(Dwarf_Type /*type*/,
    Dwarf_Off*       /*return_offset*/,
    Dwarf_Error*     /*error*/);

int dwarf_type_cu_offset(Dwarf_Type /*type*/,
    Dwarf_Off*       /*return_offset*/,
    Dwarf_Error*     /*error*/);

int dwarf_type_name_offsets(Dwarf_Type    /*type*/,
    char   **        /*returned_name*/,
    Dwarf_Off*       /*die_offset*/,
    Dwarf_Off*       /*cu_offset*/,
    Dwarf_Error*     /*error*/);

/* User-defined type name operations, DWARF3  .debug_pubtypes section. 
*/
int dwarf_get_pubtypes(Dwarf_Debug    /*dbg*/,
    Dwarf_Type**     /*types*/,
    Dwarf_Signed *   /*number_of_types*/,
    Dwarf_Error*     /*error*/);
void dwarf_pubtypes_dealloc(Dwarf_Debug /*dbg*/,
    Dwarf_Type*      /*pubtypes*/,
    Dwarf_Signed     /*number_of_pubtypes*/);


int dwarf_pubtypename(Dwarf_Type /*type*/,
    char   **        /*returned_name*/,
    Dwarf_Error*     /*error*/);

int dwarf_pubtype_die_offset(Dwarf_Type /*type*/,
    Dwarf_Off*       /*return_offset*/,
    Dwarf_Error*     /*error*/);

int dwarf_pubtype_cu_offset(Dwarf_Type /*type*/,
    Dwarf_Off*       /*return_offset*/,
    Dwarf_Error*     /*error*/);

int dwarf_pubtype_name_offsets(Dwarf_Type    /*type*/,
    char   **        /*returned_name*/,
    Dwarf_Off*       /*die_offset*/,
    Dwarf_Off*       /*cu_offset*/,
    Dwarf_Error*     /*error*/);

/* File-scope static variable name operations.  */
int dwarf_get_vars(Dwarf_Debug    /*dbg*/,
    Dwarf_Var**      /*vars*/,
    Dwarf_Signed *   /*number_of_vars*/,
    Dwarf_Error*     /*error*/);
void dwarf_vars_dealloc(Dwarf_Debug /*dbg*/,
    Dwarf_Var*       /*vars*/,
    Dwarf_Signed     /*number_of_vars*/);


int dwarf_varname(Dwarf_Var /*var*/,
    char   **        /*returned_name*/,
    Dwarf_Error*     /*error*/);

int dwarf_var_die_offset(Dwarf_Var /*var*/,
    Dwarf_Off*       /*return_offset*/,
    Dwarf_Error*     /*error*/);

int dwarf_var_cu_offset(Dwarf_Var /*var*/,
    Dwarf_Off*       /*return_offset*/,
    Dwarf_Error*     /*error*/);

int dwarf_var_name_offsets(Dwarf_Var /*var*/,
    char   **        /*returned_name*/,
    Dwarf_Off*       /*die_offset*/,
    Dwarf_Off*       /*cu_offset*/,
    Dwarf_Error*     /*error*/);

/* weak name operations.  */
int dwarf_get_weaks(Dwarf_Debug    /*dbg*/,
    Dwarf_Weak**     /*weaks*/,
    Dwarf_Signed *   /*number_of_weaks*/,
    Dwarf_Error*     /*error*/);
void dwarf_weaks_dealloc(Dwarf_Debug /*dbg*/,
    Dwarf_Weak*      /*weaks*/,
    Dwarf_Signed     /*number_of_weaks*/);


int dwarf_weakname(Dwarf_Weak /*weak*/,
    char   **        /*returned_name*/,
    Dwarf_Error*     /*error*/);

int dwarf_weak_die_offset(Dwarf_Weak /*weak*/,
    Dwarf_Off*       /*return_offset*/,
    Dwarf_Error*     /*error*/);

int dwarf_weak_cu_offset(Dwarf_Weak /*weak*/,
    Dwarf_Off*       /*return_offset*/,
    Dwarf_Error*     /*error*/);

int dwarf_weak_name_offsets(Dwarf_Weak    /*weak*/,
    char   **        /*returned_name*/,
    Dwarf_Off*       /*die_offset*/,
    Dwarf_Off*       /*cu_offset*/,
    Dwarf_Error*     /*error*/);

/* location list section operation.  (.debug_loc access) */
int dwarf_get_loclist_entry(Dwarf_Debug /*dbg*/, 
    Dwarf_Unsigned   /*offset*/, 
    Dwarf_Addr*      /*hipc*/, 
    Dwarf_Addr*      /*lopc*/, 
    Dwarf_Ptr*       /*data*/, 
    Dwarf_Unsigned*  /*entry_len*/, 
    Dwarf_Unsigned*  /*next_entry*/, 
    Dwarf_Error*     /*error*/);

/* abbreviation section operations */
int dwarf_get_abbrev(Dwarf_Debug /*dbg*/, 
    Dwarf_Unsigned   /*offset*/, 
    Dwarf_Abbrev  *  /*returned_abbrev*/,
    Dwarf_Unsigned*  /*length*/, 
    Dwarf_Unsigned*  /*attr_count*/, 
    Dwarf_Error*     /*error*/);

int dwarf_get_abbrev_tag(Dwarf_Abbrev /*abbrev*/, 
    Dwarf_Half*      /*return_tag_number*/,
    Dwarf_Error*     /*error*/);
int dwarf_get_abbrev_code(Dwarf_Abbrev /*abbrev*/, 
    Dwarf_Unsigned*  /*return_code_number*/,
    Dwarf_Error*     /*error*/);

int dwarf_get_abbrev_children_flag(Dwarf_Abbrev /*abbrev*/, 
    Dwarf_Signed*    /*return_flag*/,
    Dwarf_Error*     /*error*/);

int dwarf_get_abbrev_entry(Dwarf_Abbrev /*abbrev*/, 
    Dwarf_Signed     /*index*/, 
    Dwarf_Half  *    /*returned_attr_num*/,
    Dwarf_Signed*    /*form*/, 
    Dwarf_Off*       /*offset*/, 
    Dwarf_Error*     /*error*/);

/* consumer string section operation */
int dwarf_get_str(Dwarf_Debug /*dbg*/, 
    Dwarf_Off        /*offset*/, 
    char**           /*string*/, 
    Dwarf_Signed *   /*strlen_of_string*/,
    Dwarf_Error*     /*error*/);

/* Consumer op on  gnu .eh_frame info */
int dwarf_get_fde_list_eh(
    Dwarf_Debug      /*dbg*/,
    Dwarf_Cie**      /*cie_data*/,
    Dwarf_Signed*    /*cie_element_count*/,
    Dwarf_Fde**      /*fde_data*/,
    Dwarf_Signed*    /*fde_element_count*/,
    Dwarf_Error*     /*error*/);


/* consumer operations on frame info: .debug_frame */
int dwarf_get_fde_list(Dwarf_Debug /*dbg*/, 
    Dwarf_Cie**      /*cie_data*/, 
    Dwarf_Signed*    /*cie_element_count*/, 
    Dwarf_Fde**      /*fde_data*/, 
    Dwarf_Signed*    /*fde_element_count*/, 
    Dwarf_Error*     /*error*/);

/* Release storage gotten by dwarf_get_fde_list_eh() or
   dwarf_get_fde_list() */
void dwarf_fde_cie_list_dealloc(Dwarf_Debug dbg,
    Dwarf_Cie *cie_data,
    Dwarf_Signed cie_element_count,
    Dwarf_Fde *fde_data,
    Dwarf_Signed fde_element_count);



int dwarf_get_fde_range(Dwarf_Fde /*fde*/, 
    Dwarf_Addr*      /*low_pc*/, 
    Dwarf_Unsigned*  /*func_length*/, 
    Dwarf_Ptr*       /*fde_bytes*/, 
    Dwarf_Unsigned*  /*fde_byte_length*/, 
    Dwarf_Off*       /*cie_offset*/, 
    Dwarf_Signed*    /*cie_index*/, 
    Dwarf_Off*       /*fde_offset*/, 
    Dwarf_Error*     /*error*/);

/*  Useful for IRIX only:  see dwarf_get_cie_augmentation_data()
       dwarf_get_fde_augmentation_data() for GNU .eh_frame. */
int dwarf_get_fde_exception_info(Dwarf_Fde /*fde*/,
    Dwarf_Signed*    /* offset_into_exception_tables */,
    Dwarf_Error*     /*error*/);


int dwarf_get_cie_of_fde(Dwarf_Fde /*fde*/,
    Dwarf_Cie *      /*cie_returned*/,
    Dwarf_Error*     /*error*/);

int dwarf_get_cie_info(Dwarf_Cie /*cie*/, 
    Dwarf_Unsigned * /*bytes_in_cie*/,
    Dwarf_Small*     /*version*/, 
    char        **   /*augmenter*/,
    Dwarf_Unsigned*  /*code_alignment_factor*/, 
    Dwarf_Signed*    /*data_alignment_factor*/, 
    Dwarf_Half*      /*return_address_register_rule*/, 
    Dwarf_Ptr*       /*initial_instructions*/, 
    Dwarf_Unsigned*  /*initial_instructions_length*/, 
    Dwarf_Error*     /*error*/);

/* dwarf_get_cie_index new September 2009. */
int dwarf_get_cie_index(
    Dwarf_Cie /*cie*/,
    Dwarf_Signed* /*index*/, 
    Dwarf_Error* /*error*/ );


int dwarf_get_fde_instr_bytes(Dwarf_Fde /*fde*/, 
    Dwarf_Ptr *      /*outinstrs*/, Dwarf_Unsigned * /*outlen*/, 
    Dwarf_Error *    /*error*/);

int dwarf_get_fde_info_for_all_regs(Dwarf_Fde /*fde*/, 
    Dwarf_Addr       /*pc_requested*/,
    Dwarf_Regtable*  /*reg_table*/,
    Dwarf_Addr*      /*row_pc*/,
    Dwarf_Error*     /*error*/);

int dwarf_get_fde_info_for_all_regs3(Dwarf_Fde /*fde*/, 
    Dwarf_Addr       /*pc_requested*/,
    Dwarf_Regtable3* /*reg_table*/,
    Dwarf_Addr*      /*row_pc*/,
    Dwarf_Error*     /*error*/);

/* In this older interface DW_FRAME_CFA_COL is a meaningful
    column (which does not work well with DWARF3 or
    non-MIPS architectures). */
int dwarf_get_fde_info_for_reg(Dwarf_Fde /*fde*/, 
    Dwarf_Half       /*table_column*/, 
    Dwarf_Addr       /*pc_requested*/, 
    Dwarf_Signed*    /*offset_relevant*/,
    Dwarf_Signed*    /*register*/,  
    Dwarf_Signed*    /*offset*/, 
    Dwarf_Addr*      /*row_pc*/, 
    Dwarf_Error*     /*error*/);

/* See discussion of dw_value_type, libdwarf.h. 
   Use of DW_FRAME_CFA_COL is not meaningful in this interface.
   See dwarf_get_fde_info_for_cfa_reg3().
*/
/* dwarf_get_fde_info_for_reg3 is useful on a single column, but
   it is inefficient to iterate across all table_columns using this
   function.  Instead call dwarf_get_fde_info_for_all_regs3() and index
   into the table it fills in. */
int dwarf_get_fde_info_for_reg3(Dwarf_Fde /*fde*/, 
    Dwarf_Half       /*table_column*/, 
    Dwarf_Addr       /*pc_requested*/, 
    Dwarf_Small  *   /*value_type*/, 
    Dwarf_Signed *   /*offset_relevant*/,
    Dwarf_Signed*    /*register*/,  
    Dwarf_Signed*    /*offset_or_block_len*/, 
    Dwarf_Ptr   *    /*block_ptr */,
    Dwarf_Addr*      /*row_pc_out*/, 
    Dwarf_Error*     /*error*/);

/* Use this to get the cfa. */
int dwarf_get_fde_info_for_cfa_reg3(Dwarf_Fde /*fde*/, 
    Dwarf_Addr       /*pc_requested*/, 
    Dwarf_Small  *   /*value_type*/, 
    Dwarf_Signed *   /*offset_relevant*/,
    Dwarf_Signed*    /*register*/,  
    Dwarf_Signed*    /*offset_or_block_len*/, 
    Dwarf_Ptr   *    /*block_ptr */,
    Dwarf_Addr*      /*row_pc_out*/, 
    Dwarf_Error*     /*error*/);

int dwarf_get_fde_for_die(Dwarf_Debug /*dbg*/, 
    Dwarf_Die        /*subr_die */, 
    Dwarf_Fde  *     /*returned_fde*/,
    Dwarf_Error*     /*error*/);

int dwarf_get_fde_n(Dwarf_Fde* /*fde_data*/, 
    Dwarf_Unsigned   /*fde_index*/, 
    Dwarf_Fde  *     /*returned_fde*/,
    Dwarf_Error*     /*error*/);

int dwarf_get_fde_at_pc(Dwarf_Fde* /*fde_data*/, 
    Dwarf_Addr       /*pc_of_interest*/, 
    Dwarf_Fde  *     /*returned_fde*/,
    Dwarf_Addr*      /*lopc*/, 
    Dwarf_Addr*      /*hipc*/, 
    Dwarf_Error*     /*error*/);

/* GNU .eh_frame augmentation information, raw form, see
   Linux Standard Base Core Specification version 3.0 . */
int dwarf_get_cie_augmentation_data(Dwarf_Cie /* cie*/,
    Dwarf_Small **   /* augdata */,
    Dwarf_Unsigned * /* augdata_len */,
    Dwarf_Error*     /*error*/);
/* GNU .eh_frame augmentation information, raw form, see
   Linux Standard Base Core Specification version 3.0 . */
int dwarf_get_fde_augmentation_data(Dwarf_Fde /* fde*/,
    Dwarf_Small **   /* augdata */,
    Dwarf_Unsigned * /* augdata_len */,
    Dwarf_Error*     /*error*/);

int dwarf_expand_frame_instructions(Dwarf_Cie /*cie*/, 
    Dwarf_Ptr        /*instruction*/, 
    Dwarf_Unsigned   /*i_length*/, 
    Dwarf_Frame_Op** /*returned_op_list*/, 
    Dwarf_Signed*    /*op_count*/,
    Dwarf_Error*     /*error*/);

/* Operations on .debug_aranges. */
int dwarf_get_aranges(Dwarf_Debug /*dbg*/, 
    Dwarf_Arange**   /*aranges*/, 
    Dwarf_Signed *   /*arange_count*/,
    Dwarf_Error*     /*error*/);



int dwarf_get_arange(
    Dwarf_Arange*    /*aranges*/, 
    Dwarf_Unsigned   /*arange_count*/, 
    Dwarf_Addr       /*address*/, 
    Dwarf_Arange *   /*returned_arange*/,
    Dwarf_Error*     /*error*/);

int dwarf_get_cu_die_offset(
    Dwarf_Arange     /*arange*/, 
    Dwarf_Off*       /*return_offset*/,
    Dwarf_Error*     /*error*/);

int dwarf_get_arange_cu_header_offset(
    Dwarf_Arange     /*arange*/, 
    Dwarf_Off*       /*return_cu_header_offset*/,
    Dwarf_Error*     /*error*/);
#ifdef __sgi /* pragma is sgi MIPS only */
#pragma optional dwarf_get_arange_cu_header_offset
#endif

/* DWARF2,3 interface. No longer really adequate (it was never
   right for segmented address spaces, please switch
   to using dwarf_get_arange_info_b instead.  
   There is no effective difference between these
   functions  if the address space
   of the target is not segmented.  */
int dwarf_get_arange_info(
    Dwarf_Arange     /*arange*/, 
    Dwarf_Addr*      /*start*/, 
    Dwarf_Unsigned*  /*length*/, 
    Dwarf_Off*       /*cu_die_offset*/, 
    Dwarf_Error*     /*error*/ );

/* New for DWARF4, entries may have segment information. 
   *segment is only meaningful if *segment_entry_size is non-zero. */
int dwarf_get_arange_info_b(
    Dwarf_Arange     /*arange*/, 
    Dwarf_Unsigned*  /*segment*/, 
    Dwarf_Unsigned*  /*segment_entry_size*/, 
    Dwarf_Addr    *  /*start*/, 
    Dwarf_Unsigned*  /*length*/, 
    Dwarf_Off     *  /*cu_die_offset*/, 
    Dwarf_Error   *  /*error*/ );


/* consumer .debug_macinfo information interface.
*/
struct Dwarf_Macro_Details_s {
    Dwarf_Off    dmd_offset; /* offset, in the section,
                              of this macro info */
    Dwarf_Small  dmd_type;   /* the type, DW_MACINFO_define etc*/
    Dwarf_Signed dmd_lineno; /* the source line number where
                              applicable and vend_def # if
                              vendor_extension op
                             */

    Dwarf_Signed dmd_fileindex;/* the source file index:
                              applies to define undef start_file
                               */
    char *       dmd_macro;  /* macro name (with value for defineop)
                              string from vendor ext
                             */
};

/* dwarf_print_lines is for use by dwarfdump: it prints
   line info to stdout.
   The _dwarf name is obsolete. Use dwarf_ instead.
   Added extra argnument 2/2009 for better checking.
*/
int _dwarf_print_lines(Dwarf_Die /*cu_die*/,Dwarf_Error * /*error*/);
int dwarf_print_lines(Dwarf_Die /*cu_die*/,Dwarf_Error * /*error*/,
   int * /*error_count_out */);

/* dwarf_check_lineheader lets dwarfdump get detailed messages
   about some compiler errors we detect.
   We return the count of detected errors throught the
   pointer.
*/
void dwarf_check_lineheader(Dwarf_Die /*cu_die*/,int *errcount_out);

/* dwarf_ld_sort_lines helps SGI IRIX ld 
   rearrange lines in .debug_line in a .o created with a text
   section per function.  
        -OPT:procedure_reorder=ON
   where ld-cord (cord(1)ing by ld, 
   not by cord(1)) may have changed the function order.
   The _dwarf name is obsolete. Use dwarf_ instead.
*/
int _dwarf_ld_sort_lines(
    void *         /*orig_buffer*/,
    unsigned long  /* buffer_len*/,
    int            /*is_64_bit*/,
    int *          /*any_change*/,
    int *          /*err_code*/);
int dwarf_ld_sort_lines(
    void *         /*orig_buffer*/,
    unsigned long  /*buffer_len*/,
    int            /*is_64_bit*/,
    int *          /*any_change*/,
    int *          /*err_code*/);

/* Used by dwarfdump -v to print fde offsets from debugging
   info.
   The _dwarf name is obsolete. Use dwarf_ instead.
*/
int _dwarf_fde_section_offset(Dwarf_Debug dbg,
    Dwarf_Fde         /*in_fde*/,
    Dwarf_Off *       /*fde_off*/, 
    Dwarf_Off *       /*cie_off*/,
    Dwarf_Error *     /*err*/);
int dwarf_fde_section_offset(Dwarf_Debug dbg,
    Dwarf_Fde         /*in_fde*/,
    Dwarf_Off *       /*fde_off*/, 
    Dwarf_Off *       /*cie_off*/,
    Dwarf_Error *     /*err*/);

/* Used by dwarfdump -v to print cie offsets from debugging
   info.
   The _dwarf name is obsolete. Use dwarf_ instead.
*/
int dwarf_cie_section_offset(Dwarf_Debug /*dbg*/,
    Dwarf_Cie     /*in_cie*/,
    Dwarf_Off *   /*cie_off */,
    Dwarf_Error * /*err*/);
int _dwarf_cie_section_offset(Dwarf_Debug /*dbg*/,
    Dwarf_Cie     /*in_cie*/,
    Dwarf_Off *   /*cie_off*/,
    Dwarf_Error * /*err*/);

typedef struct Dwarf_Macro_Details_s Dwarf_Macro_Details;

int dwarf_get_macro(Dwarf_Debug /*dbg*/,
    char *        /*requested_macro_name*/,
    Dwarf_Addr    /*pc_of_request*/,
    char **       /*returned_macro_value*/,
    Dwarf_Error * /*error*/);

int dwarf_get_all_defined_macros(Dwarf_Debug /*dbg*/,
    Dwarf_Addr     /*pc_of_request*/,
    Dwarf_Signed * /*returned_count*/,
    char ***       /*returned_pointers_to_macros*/,
    Dwarf_Error *  /*error*/);

char *dwarf_find_macro_value_start(char * /*macro_string*/);

int dwarf_get_macro_details(Dwarf_Debug /*dbg*/,
    Dwarf_Off            /*macro_offset*/,
    Dwarf_Unsigned       /*maximum_count*/,
    Dwarf_Signed         * /*entry_count*/,
    Dwarf_Macro_Details ** /*details*/,
    Dwarf_Error *        /*err*/);


int dwarf_get_address_size(Dwarf_Debug /*dbg*/,
    Dwarf_Half  *    /*addr_size*/,
    Dwarf_Error *    /*error*/);

/* The dwarf specification separates FORMs into
different classes.  To do the seperation properly
requires 4 pieces of data as of DWARF4 (thus the
function arguments listed here). 
The DWARF4 specification class definition suffices to
describe all DWARF versions.
See section 7.5.4, Attribute Encodings.
A return of DW_FORM_CLASS_UNKNOWN means we could not properly figure
out what form-class it is.

    DW_FORM_CLASS_FRAMEPTR is MIPS/IRIX only, and refers
    to the DW_AT_MIPS_fde attribute (a reference to the
    .debug_frame section).
*/
enum Dwarf_Form_Class {
    DW_FORM_CLASS_UNKNOWN,   DW_FORM_CLASS_ADDRESS,
    DW_FORM_CLASS_BLOCK,     DW_FORM_CLASS_CONSTANT, 
    DW_FORM_CLASS_EXPRLOC,   DW_FORM_CLASS_FLAG, 
    DW_FORM_CLASS_LINEPTR,   DW_FORM_CLASS_LOCLISTPTR, 
    DW_FORM_CLASS_MACPTR,    DW_FORM_CLASS_RANGELISTPTR, 
    DW_FORM_CLASS_REFERENCE, DW_FORM_CLASS_STRING,
    DW_FORM_CLASS_FRAMEPTR
};

enum Dwarf_Form_Class dwarf_get_form_class(
    Dwarf_Half /* dwversion */,
    Dwarf_Half /* attrnum */, 
    Dwarf_Half /*offset_size */, 
    Dwarf_Half /*form*/);

/* utility operations */
Dwarf_Unsigned dwarf_errno(Dwarf_Error     /*error*/);

char* dwarf_errmsg(Dwarf_Error    /*error*/);

/* stringcheck zero is default and means do all
** string length validity checks.
** Call with parameter value 1 to turn off many such checks (and
** increase performance).
** Call with zero for safest running.
** Actual value saved and returned is only 8 bits! Upper bits
** ignored by libdwarf (and zero on return).
** Returns previous value.
*/
int dwarf_set_stringcheck(int /*stringcheck*/);

/* 'apply' defaults to 1 and means do all
 * 'rela' relocations on reading in a dwarf object section with
 * such relocations.
 * Call with parameter value 0 to turn off application of 
 * such relocations.
 * Since the static linker leaves 'bogus' data in object sections
 * with a 'rela' relocation section such data cannot be read
 * sensibly without processing the relocations.  Such relocations
 * do not exist in executables and shared objects (.so), the
 * relocations only exist in plain .o relocatable object files.
 * Actual value saved and returned is only 8 bits! Upper bits
 * ignored by libdwarf (and zero on return).
 * Returns previous value.
 * */
int dwarf_set_reloc_application(int /*apply*/);


/* Unimplemented */
Dwarf_Handler dwarf_seterrhand(Dwarf_Debug /*dbg*/, Dwarf_Handler /*errhand*/);

/* Unimplemented */
Dwarf_Ptr dwarf_seterrarg(Dwarf_Debug /*dbg*/, Dwarf_Ptr /*errarg*/);

void dwarf_dealloc(Dwarf_Debug /*dbg*/, void* /*space*/, 
    Dwarf_Unsigned /*type*/);

/* DWARF Producer Interface */

typedef int (*Dwarf_Callback_Func)(
    char*           /*name*/, 
    int             /*size*/, 
    Dwarf_Unsigned  /*type*/,
    Dwarf_Unsigned  /*flags*/, 
    Dwarf_Unsigned  /*link*/, 
    Dwarf_Unsigned  /*info*/, 
    int*            /*sect name index*/, 
    int*            /*error*/);

Dwarf_P_Debug dwarf_producer_init(
    Dwarf_Unsigned  /*creation_flags*/, 
    Dwarf_Callback_Func    /*func*/,
    Dwarf_Handler   /*errhand*/, 
    Dwarf_Ptr       /*errarg*/, 
    Dwarf_Error*    /*error*/);

typedef int (*Dwarf_Callback_Func_b)(
    char*           /*name*/,
    int             /*size*/,
    Dwarf_Unsigned  /*type*/,
    Dwarf_Unsigned  /*flags*/,
    Dwarf_Unsigned  /*link*/,
    Dwarf_Unsigned  /*info*/,
    Dwarf_Unsigned* /*sect_name_index*/,
    int*            /*error*/);


Dwarf_P_Debug dwarf_producer_init_b(
    Dwarf_Unsigned        /*flags*/,
    Dwarf_Callback_Func_b /*func*/,
    Dwarf_Handler         /*errhand*/,
    Dwarf_Ptr             /*errarg*/,
    Dwarf_Error *         /*error*/);


Dwarf_Signed dwarf_transform_to_disk_form(Dwarf_P_Debug /*dbg*/,
    Dwarf_Error*     /*error*/);

Dwarf_Ptr dwarf_get_section_bytes(Dwarf_P_Debug /*dbg*/, 
    Dwarf_Signed     /*dwarf_section*/,
    Dwarf_Signed*    /*elf_section_index*/, 
    Dwarf_Unsigned*  /*length*/, 
    Dwarf_Error*     /*error*/);

int  dwarf_get_relocation_info_count(
    Dwarf_P_Debug    /*dbg*/,
    Dwarf_Unsigned * /*count_of_relocation_sections*/,
    int *                /*drd_buffer_version*/,
    Dwarf_Error*     /*error*/);

int dwarf_get_relocation_info(
    Dwarf_P_Debug           /*dbg*/,
    Dwarf_Signed          * /*elf_section_index*/,
    Dwarf_Signed          * /*elf_section_index_link*/,
    Dwarf_Unsigned        * /*relocation_buffer_count*/,
    Dwarf_Relocation_Data * /*reldata_buffer*/,
    Dwarf_Error*            /*error*/);

/* v1:  no drd_length field, enum explicit */
/* v2:  has the drd_length field, enum value in uchar member */
#define DWARF_DRD_BUFFER_VERSION 2

Dwarf_Signed dwarf_get_die_markers(
    Dwarf_P_Debug     /*dbg*/,
    Dwarf_P_Marker *  /*marker_list*/,
    Dwarf_Unsigned *  /*marker_count*/,
    Dwarf_Error *     /*error*/);

int dwarf_get_string_attributes_count(Dwarf_P_Debug,
    Dwarf_Unsigned *,
    int *,
    Dwarf_Error *);

int dwarf_get_string_attributes_info(Dwarf_P_Debug, 
    Dwarf_Signed *,
    Dwarf_Unsigned *,
    Dwarf_P_String_Attr *,
    Dwarf_Error *);

void dwarf_reset_section_bytes(Dwarf_P_Debug /*dbg*/);

Dwarf_Unsigned dwarf_producer_finish(Dwarf_P_Debug /*dbg*/, 
    Dwarf_Error* /*error*/);

/* Producer attribute addition functions. */
Dwarf_P_Attribute dwarf_add_AT_targ_address(Dwarf_P_Debug /*dbg*/, 
    Dwarf_P_Die     /*ownerdie*/, 
    Dwarf_Half      /*attr*/, 
    Dwarf_Unsigned  /*pc_value*/, 
    Dwarf_Signed    /*sym_index*/, 
    Dwarf_Error*    /*error*/);

Dwarf_P_Attribute dwarf_add_AT_block(Dwarf_P_Debug /*dbg*/, 
    Dwarf_P_Die     /*ownerdie*/, 
    Dwarf_Half      /*attr*/, 
    Dwarf_Small*    /*block_data*/,
    Dwarf_Unsigned  /*block_len*/,
    Dwarf_Error*    /*error*/);

Dwarf_P_Attribute dwarf_add_AT_targ_address_b(Dwarf_P_Debug /*dbg*/, 
    Dwarf_P_Die     /*ownerdie*/, 
    Dwarf_Half      /*attr*/, 
    Dwarf_Unsigned  /*pc_value*/, 
    Dwarf_Unsigned  /*sym_index*/, 
    Dwarf_Error*    /*error*/);

Dwarf_P_Attribute dwarf_add_AT_ref_address(Dwarf_P_Debug /*dbg*/, 
    Dwarf_P_Die     /*ownerdie*/, 
    Dwarf_Half      /*attr*/, 
    Dwarf_Unsigned  /*pc_value*/, 
    Dwarf_Unsigned  /*sym_index*/, 
    Dwarf_Error*    /*error*/);

Dwarf_P_Attribute dwarf_add_AT_unsigned_const(Dwarf_P_Debug /*dbg*/, 
    Dwarf_P_Die     /*ownerdie*/, 
    Dwarf_Half      /*attr*/, 
    Dwarf_Unsigned  /*value*/, 
    Dwarf_Error*    /*error*/);

Dwarf_P_Attribute dwarf_add_AT_signed_const(Dwarf_P_Debug /*dbg*/, 
    Dwarf_P_Die     /*ownerdie*/, 
    Dwarf_Half      /*attr*/, 
    Dwarf_Signed    /*value*/, 
    Dwarf_Error*    /*error*/);

Dwarf_P_Attribute dwarf_add_AT_reference(Dwarf_P_Debug /*dbg*/, 
    Dwarf_P_Die     /*ownerdie*/, 
    Dwarf_Half      /*attr*/, 
    Dwarf_P_Die     /*otherdie*/, 
    Dwarf_Error*    /*error*/);

Dwarf_P_Attribute dwarf_add_AT_dataref(
    Dwarf_P_Debug   /* dbg*/,
    Dwarf_P_Die     /*ownerdie*/, 
    Dwarf_Half      /*attr*/,
    Dwarf_Unsigned  /*pcvalue*/,
    Dwarf_Unsigned  /*sym_index*/,
    Dwarf_Error*    /*error*/);

Dwarf_P_Attribute dwarf_add_AT_const_value_string(Dwarf_P_Die /*ownerdie*/, 
    char*           /*string_value*/, 
    Dwarf_Error*    /*error*/);

Dwarf_P_Attribute dwarf_add_AT_location_expr(Dwarf_P_Debug /*dbg*/, 
    Dwarf_P_Die     /*ownerdie*/, 
    Dwarf_Half      /*attr*/, 
    Dwarf_P_Expr    /*loc_expr*/, 
    Dwarf_Error*    /*error*/);

Dwarf_P_Attribute dwarf_add_AT_string(Dwarf_P_Debug /*dbg*/, 
    Dwarf_P_Die     /*ownerdie*/, 
    Dwarf_Half      /*attr*/, 
    char*           /*string*/, 
    Dwarf_Error*     /*error*/);

Dwarf_P_Attribute dwarf_add_AT_flag(Dwarf_P_Debug /*dbg*/, 
    Dwarf_P_Die     /*ownerdie*/, 
    Dwarf_Half      /*attr*/, 
    Dwarf_Small     /*flag*/, 
    Dwarf_Error*    /*error*/);

Dwarf_P_Attribute dwarf_add_AT_producer(Dwarf_P_Die /*ownerdie*/, 
    char*           /*producer_string*/, 
    Dwarf_Error*    /*error*/);

Dwarf_P_Attribute dwarf_add_AT_const_value_signedint(Dwarf_P_Die /*ownerdie*/, 
    Dwarf_Signed    /*signed_value*/, 
    Dwarf_Error*    /*error*/);

Dwarf_P_Attribute dwarf_add_AT_const_value_unsignedint(
    Dwarf_P_Die     /*ownerdie*/, 
    Dwarf_Unsigned  /*unsigned_value*/, 
    Dwarf_Error*    /*error*/);

Dwarf_P_Attribute dwarf_add_AT_comp_dir(Dwarf_P_Die /*ownerdie*/, 
    char*           /*current_working_directory*/, 
    Dwarf_Error*    /*error*/);

Dwarf_P_Attribute dwarf_add_AT_name(Dwarf_P_Die    /*die*/,
    char*           /*name*/,
    Dwarf_Error*    /*error*/);

/* Producer line creation functions (.debug_line) */
Dwarf_Unsigned dwarf_add_directory_decl(Dwarf_P_Debug /*dbg*/, 
    char*           /*name*/, 
    Dwarf_Error*    /*error*/);

Dwarf_Unsigned dwarf_add_file_decl(Dwarf_P_Debug /*dbg*/, 
    char*           /*name*/,
    Dwarf_Unsigned  /*dir_index*/, 
    Dwarf_Unsigned  /*time_last_modified*/, 
    Dwarf_Unsigned  /*length*/, 
    Dwarf_Error*    /*error*/);

Dwarf_Unsigned dwarf_add_line_entry(Dwarf_P_Debug /*dbg*/, 
    Dwarf_Unsigned  /*file_index*/, 
    Dwarf_Addr      /*code_address*/, 
    Dwarf_Unsigned  /*lineno*/, 
    Dwarf_Signed    /*column_number*/, 
    Dwarf_Bool      /*is_source_stmt_begin*/, 
    Dwarf_Bool      /*is_basic_block_begin*/, 
    Dwarf_Error*    /*error*/);

Dwarf_Unsigned dwarf_lne_set_address(Dwarf_P_Debug /*dbg*/, 
    Dwarf_Unsigned  /*offset*/, 
    Dwarf_Unsigned  /*symbol_index*/, 
    Dwarf_Error*    /*error*/);

Dwarf_Unsigned dwarf_lne_end_sequence(Dwarf_P_Debug /*dbg*/, 
    Dwarf_Addr      /*end_address*/,
    Dwarf_Error*    /*error*/);

/* Producer .debug_frame functions */
Dwarf_Unsigned dwarf_add_frame_cie(Dwarf_P_Debug /*dbg*/, 
    char*           /*augmenter*/, 
    Dwarf_Small     /*code_alignent_factor*/, 
    Dwarf_Small     /*data_alignment_factor*/, 
    Dwarf_Small     /*return_address_reg*/, 
    Dwarf_Ptr       /*initialization_bytes*/, 
    Dwarf_Unsigned  /*init_byte_len*/, 
    Dwarf_Error*    /*error*/);

Dwarf_Unsigned dwarf_add_frame_fde( 
    Dwarf_P_Debug   /*dbg*/,
    Dwarf_P_Fde     /*fde*/, 
    Dwarf_P_Die     /*corresponding subprogram die*/,
    Dwarf_Unsigned  /*cie_to_use*/, 
    Dwarf_Unsigned  /*virt_addr_of_described_code*/, 
    Dwarf_Unsigned  /*length_of_code*/, 
    Dwarf_Unsigned  /*symbol_index*/, 
    Dwarf_Error*    /*error*/);

Dwarf_Unsigned dwarf_add_frame_fde_b(
    Dwarf_P_Debug  /*dbg*/,
    Dwarf_P_Fde    /*fde*/,
    Dwarf_P_Die    /*die*/,
    Dwarf_Unsigned /*cie*/,
    Dwarf_Addr     /*virt_addr*/,
    Dwarf_Unsigned /*code_len*/,
    Dwarf_Unsigned /*sym_idx*/,
    Dwarf_Unsigned /*sym_idx_of_end*/,
    Dwarf_Addr     /*offset_from_end_sym*/,
    Dwarf_Error*   /*error*/);

Dwarf_Unsigned dwarf_add_frame_info_b( 
    Dwarf_P_Debug dbg   /*dbg*/,
    Dwarf_P_Fde     /*fde*/,
    Dwarf_P_Die     /*die*/,
    Dwarf_Unsigned  /*cie*/,
    Dwarf_Addr      /*virt_addr*/,
    Dwarf_Unsigned  /*code_len*/,
    Dwarf_Unsigned  /*symidx*/,
    Dwarf_Unsigned  /*end_symbol */,
    Dwarf_Addr      /*offset_from_end_symbol */,
    Dwarf_Signed    /*offset_into_exception_tables*/,
    Dwarf_Unsigned  /*exception_table_symbol*/,
    Dwarf_Error*    /*error*/);

Dwarf_Unsigned dwarf_add_frame_info( 
    Dwarf_P_Debug dbg   /*dbg*/,
    Dwarf_P_Fde     /*fde*/,
    Dwarf_P_Die     /*die*/,
    Dwarf_Unsigned  /*cie*/,
    Dwarf_Addr      /*virt_addr*/,
    Dwarf_Unsigned  /*code_len*/,
    Dwarf_Unsigned  /*symidx*/,
    Dwarf_Signed    /*offset_into_exception_tables*/,
    Dwarf_Unsigned  /*exception_table_symbol*/,
    Dwarf_Error*    /*error*/);

Dwarf_P_Fde dwarf_add_fde_inst(
    Dwarf_P_Fde     /*fde*/,
    Dwarf_Small     /*op*/, 
    Dwarf_Unsigned  /*val1*/, 
    Dwarf_Unsigned  /*val2*/, 
    Dwarf_Error*    /*error*/);

/* New September 17, 2009 */
int dwarf_insert_fde_inst_bytes(
    Dwarf_P_Debug  /*dbg*/,
    Dwarf_P_Fde    /*fde*/,
    Dwarf_Unsigned /*len*/, 
    Dwarf_Ptr      /*ibytes*/,
    Dwarf_Error*   /*error*/);


Dwarf_P_Fde dwarf_new_fde(Dwarf_P_Debug    /*dbg*/, Dwarf_Error* /*error*/);

Dwarf_P_Fde dwarf_fde_cfa_offset(
    Dwarf_P_Fde     /*fde*/, 
    Dwarf_Unsigned  /*register_number*/, 
    Dwarf_Signed    /*offset*/, 
    Dwarf_Error*    /*error*/);

/* die creation & addition routines */
Dwarf_P_Die dwarf_new_die(
    Dwarf_P_Debug    /*dbg*/,
    Dwarf_Tag         /*tag*/,
    Dwarf_P_Die     /*parent*/, 
    Dwarf_P_Die     /*child*/, 
    Dwarf_P_Die     /*left */,
    Dwarf_P_Die     /*right*/,
    Dwarf_Error*    /*error*/);

Dwarf_Unsigned dwarf_add_die_to_debug(
    Dwarf_P_Debug   /*dbg*/,
    Dwarf_P_Die     /*die*/,
    Dwarf_Error*    /*error*/);

Dwarf_Unsigned dwarf_add_die_marker(
    Dwarf_P_Debug   /*dbg*/,
    Dwarf_P_Die     /*die*/,
    Dwarf_Unsigned  /*marker*/,
    Dwarf_Error *   /*error*/);

Dwarf_Unsigned dwarf_get_die_marker(
    Dwarf_P_Debug   /*dbg*/,
    Dwarf_P_Die     /*die*/,
    Dwarf_Unsigned *  /*marker*/,
    Dwarf_Error *   /*error*/);

Dwarf_P_Die dwarf_die_link(
    Dwarf_P_Die     /*die*/,
    Dwarf_P_Die     /*parent*/,
    Dwarf_P_Die     /*child*/, 
    Dwarf_P_Die     /*left*/,
    Dwarf_P_Die     /*right*/, 
    Dwarf_Error*    /*error*/);

void dwarf_dealloc_compressed_block(
    Dwarf_P_Debug,
    void *
);

/* Call this passing in return value from dwarf_uncompress_integer_block()
 * to free the space the decompression allocated. */
void dwarf_dealloc_uncompressed_block(
    Dwarf_Debug,
    void *
);

void * dwarf_compress_integer_block(
    Dwarf_P_Debug,    /* dbg */
    Dwarf_Bool,       /* signed==true (or unsigned) */
    Dwarf_Small,      /* size of integer units: 8, 16, 32, 64 */
    void*,            /* data */
    Dwarf_Unsigned,   /* number of elements */
    Dwarf_Unsigned*,  /* number of bytes in output block */
    Dwarf_Error*      /* error */
);

/* Decode an array of signed leb integers (so of course the
 * array is not composed of fixed length values, but is instead
 * a sequence of sleb values).
 * Returns a DW_DLV_BADADDR on error.
 * Otherwise returns a pointer to an array of 32bit integers.
 * The signed argument must be non-zero (the decode
 * assumes sleb integers in the input data) at this time.
 * Size of integer units must be 32 (32 bits each) at this time.
 * Number of bytes in block is a byte count (not array count).
 * Returns number of units in output block (ie, number of elements
 * of the array that the return value points to) thru the argument.
 */
void * dwarf_uncompress_integer_block(
    Dwarf_Debug,      /* dbg */
    Dwarf_Bool,       /* signed==true (or unsigned) */
    Dwarf_Small,      /* size of integer units: 8, 16, 32, 64 */
    void*,            /* input data */
    Dwarf_Unsigned,   /* number of bytes in input */
    Dwarf_Unsigned*,  /* number of units in output block */
    Dwarf_Error*      /* error */
);

/* Operations to create location expressions. */
Dwarf_P_Expr dwarf_new_expr(Dwarf_P_Debug /*dbg*/, Dwarf_Error* /*error*/);

void dwarf_expr_reset(
    Dwarf_P_Expr      /*expr*/,
    Dwarf_Error*      /*error*/);

Dwarf_Unsigned dwarf_add_expr_gen(
    Dwarf_P_Expr      /*expr*/, 
    Dwarf_Small       /*opcode*/, 
    Dwarf_Unsigned    /*val1*/, 
    Dwarf_Unsigned    /*val2*/, 
    Dwarf_Error*      /*error*/);

Dwarf_Unsigned dwarf_add_expr_addr(
    Dwarf_P_Expr      /*expr*/, 
    Dwarf_Unsigned    /*addr*/, 
    Dwarf_Signed      /*sym_index*/, 
    Dwarf_Error*      /*error*/);

Dwarf_Unsigned dwarf_add_expr_addr_b(
    Dwarf_P_Expr      /*expr*/,
    Dwarf_Unsigned    /*addr*/,
    Dwarf_Unsigned    /*sym_index*/,
    Dwarf_Error*      /*error*/);

Dwarf_Unsigned dwarf_expr_current_offset(
    Dwarf_P_Expr      /*expr*/, 
    Dwarf_Error*      /*error*/);

Dwarf_Addr dwarf_expr_into_block(
    Dwarf_P_Expr      /*expr*/, 
    Dwarf_Unsigned*   /*length*/, 
    Dwarf_Error*      /*error*/);

Dwarf_Unsigned dwarf_add_arange(Dwarf_P_Debug /*dbg*/, 
    Dwarf_Addr        /*begin_address*/, 
    Dwarf_Unsigned    /*length*/, 
    Dwarf_Signed      /*symbol_index*/, 
    Dwarf_Error*      /*error*/);

Dwarf_Unsigned dwarf_add_arange_b(
    Dwarf_P_Debug  /*dbg*/,
    Dwarf_Addr     /*begin_address*/,
    Dwarf_Unsigned /*length*/,
    Dwarf_Unsigned /*symbol_index*/,
    Dwarf_Unsigned /*end_symbol_index*/,
    Dwarf_Addr     /*offset_from_end_symbol*/,
    Dwarf_Error *  /*error*/);

Dwarf_Unsigned dwarf_add_pubname(
    Dwarf_P_Debug      /*dbg*/, 
    Dwarf_P_Die        /*die*/, 
    char*              /*pubname_name*/, 
    Dwarf_Error*       /*error*/);

Dwarf_Unsigned dwarf_add_funcname(
    Dwarf_P_Debug      /*dbg*/, 
    Dwarf_P_Die        /*die*/, 
    char*              /*func_name*/, 
    Dwarf_Error*       /*error*/);

Dwarf_Unsigned dwarf_add_typename(
    Dwarf_P_Debug     /*dbg*/, 
    Dwarf_P_Die       /*die*/, 
    char*             /*type_name*/, 
    Dwarf_Error*      /*error*/);

Dwarf_Unsigned dwarf_add_varname(
    Dwarf_P_Debug     /*dbg*/, 
    Dwarf_P_Die       /*die*/, 
    char*             /*var_name*/, 
    Dwarf_Error*      /*error*/);

Dwarf_Unsigned dwarf_add_weakname(
    Dwarf_P_Debug    /*dbg*/, 
    Dwarf_P_Die      /*die*/, 
    char*            /*weak_name*/, 
    Dwarf_Error*     /*error*/);

/* .debug_macinfo producer functions
   Functions must be called in right order: the section is output
   In the order these are presented.
*/
int dwarf_def_macro(Dwarf_P_Debug /*dbg*/,
    Dwarf_Unsigned   /*line*/,
    char *           /*macname, with (arglist), no space before (*/, 
    char *           /*macvalue*/,
    Dwarf_Error*     /*error*/);

int dwarf_undef_macro(Dwarf_P_Debug /*dbg*/,
    Dwarf_Unsigned   /*line*/,
    char *           /*macname, no arglist, of course*/,
    Dwarf_Error*     /*error*/);

int dwarf_start_macro_file(Dwarf_P_Debug /*dbg*/,
    Dwarf_Unsigned   /*fileindex*/,
    Dwarf_Unsigned   /*linenumber*/,
    Dwarf_Error*     /*error*/);

int dwarf_end_macro_file(Dwarf_P_Debug /*dbg*/,
    Dwarf_Error*     /*error*/);

int dwarf_vendor_ext(Dwarf_P_Debug /*dbg*/,
    Dwarf_Unsigned   /*constant*/,
    char *           /*string*/,
    Dwarf_Error*     /*error*/);

/* end macinfo producer functions */

int dwarf_attr_offset(Dwarf_Die /*die*/,
    Dwarf_Attribute /*attr of above die*/,
    Dwarf_Off     * /*returns offset thru this ptr */,
    Dwarf_Error   * /*error*/);

/* This is a hack so clients can verify offsets.
   Added April 2005 so that debugger can detect broken offsets
   (which happened in an IRIX executable larger than 2GB
    with MIPSpro 7.3.1.3 toolchain.).
*/
int
dwarf_get_section_max_offsets(Dwarf_Debug /*dbg*/,
    Dwarf_Unsigned * /*debug_info_size*/,
    Dwarf_Unsigned * /*debug_abbrev_size*/,
    Dwarf_Unsigned * /*debug_line_size*/,
    Dwarf_Unsigned * /*debug_loc_size*/,
    Dwarf_Unsigned * /*debug_aranges_size*/,
    Dwarf_Unsigned * /*debug_macinfo_size*/,
    Dwarf_Unsigned * /*debug_pubnames_size*/,
    Dwarf_Unsigned * /*debug_str_size*/,
    Dwarf_Unsigned * /*debug_frame_size*/,
    Dwarf_Unsigned * /*debug_ranges_size*/,
    Dwarf_Unsigned * /*debug_pubtypes_size*/);

/* Multiple releases spelled 'initial' as 'inital' . 
   The 'inital' spelling should not be used. */ 
Dwarf_Half dwarf_set_frame_rule_inital_value(Dwarf_Debug /*dbg*/,
    Dwarf_Half /*value*/);
/* Additional interface with correct 'initial' spelling. */
/* It is likely you will want to call the following 5 functions
   before accessing any frame information.  All are useful
   to tailor handling of pseudo-registers needed to turn
   frame operation references into simpler forms and to
   reflect ABI specific data.  Of course altering libdwarf.h
   and dwarf.h allow the same capabilities, but such header changes
   do not let one change these values at runtime. */
Dwarf_Half dwarf_set_frame_rule_initial_value(Dwarf_Debug /*dbg*/,
    Dwarf_Half /*value*/);
Dwarf_Half dwarf_set_frame_rule_table_size(Dwarf_Debug /*dbg*/, 
    Dwarf_Half /*value*/);
Dwarf_Half dwarf_set_frame_cfa_value(Dwarf_Debug /*dbg*/, 
    Dwarf_Half /*value*/);
Dwarf_Half dwarf_set_frame_same_value(Dwarf_Debug /*dbg*/, 
    Dwarf_Half /*value*/);
Dwarf_Half dwarf_set_frame_undefined_value(Dwarf_Debug /*dbg*/, 
    Dwarf_Half /*value*/);

/* As of April 27, 2009, this version with no diepointer is
   obsolete though supported.  Use dwarf_get_ranges_a() instead. */
int dwarf_get_ranges(Dwarf_Debug /*dbg*/, 
    Dwarf_Off /*rangesoffset*/,
    Dwarf_Ranges ** /*rangesbuf*/,
    Dwarf_Signed * /*listlen*/,
    Dwarf_Unsigned * /*bytecount*/,
    Dwarf_Error * /*error*/);

/* This adds the address_size argument. New April 27, 2009 */
int dwarf_get_ranges_a(Dwarf_Debug /*dbg*/, 
    Dwarf_Off /*rangesoffset*/,
    Dwarf_Die  /* diepointer */,
    Dwarf_Ranges ** /*rangesbuf*/,
    Dwarf_Signed * /*listlen*/,
    Dwarf_Unsigned * /*bytecount*/,
    Dwarf_Error * /*error*/);

void dwarf_ranges_dealloc(Dwarf_Debug /*dbg*/, 
    Dwarf_Ranges * /*rangesbuf*/,
    Dwarf_Signed /*rangecount*/);

/* The harmless error list is a circular buffer of
   errors we note but which do not stop us from processing
   the object.  Created so dwarfdump or other tools
   can report such inconsequential errors without causing
   anything to stop early. */
#define DW_HARMLESS_ERROR_CIRCULAR_LIST_DEFAULT_SIZE 4
#define DW_HARMLESS_ERROR_MSG_STRING_SIZE   200
/* User code supplies size of array of pointers errmsg_ptrs_array
    in count and the array of pointers (the pointers themselves
    need not be initialized).
    The pointers returned in the array of pointers
    are invalidated by ANY call to libdwarf.
    Use them before making another libdwarf call!
    The array of string pointers passed in always has
    a final null pointer, so if there are N pointers the
    and M actual strings, then MIN(M,N-1) pointers are
    set to point to error strings.  The array of pointers
    to strings always terminates with a NULL pointer.
    If 'count' is passed in zero then errmsg_ptrs_array
    is not touched.

    The function returns DW_DLV_NO_ENTRY if no harmless errors 
    were noted so far.  Returns DW_DLV_OK if there are errors.
    Never returns DW_DLV_ERROR. 

    Each call empties the error list (discarding all current entries).
    If newerr_count is non-NULL the count of harmless errors
    since the last call is returned through the pointer
    (some may have been discarded or not returned, it is a circular
    list...).
    If DW_DLV_NO_ENTRY is returned none of the arguments
    here are touched or used.
    */
int dwarf_get_harmless_error_list(Dwarf_Debug /*dbg*/,
    unsigned  /*count*/, 
    const char ** /*errmsg_ptrs_array*/,
    unsigned * /*newerr_count*/);

/* Insertion is only for testing the harmless error code, it is not
    necessarily useful otherwise. */
void dwarf_insert_harmless_error(Dwarf_Debug /*dbg*/,
    char * /*newerror*/);

/* The size of the circular list of strings may be set
   and reset as needed.  If it is shortened excess
   messages are simply dropped.  It returns the previous
   size. If zero passed in the size is unchanged
   and it simply returns the current size  */
unsigned dwarf_set_harmless_error_list_size(Dwarf_Debug /*dbg*/,
    unsigned /*maxcount*/);
/* The harmless error strings (if any) are freed when the dbg
   is dwarf_finish()ed. */

/*  When the val_in is known these dwarf_get_TAG_name (etc)
    functions return the string corresponding to the val_in passed in
    through the pointer s_out and the value returned is DW_DLV_OK.   
    The strings are in static storage 
    and must not be freed.
    If DW_DLV_NO_ENTRY is returned the val_in is not known and
    *s_out is not set.  DW_DLV_ERROR is never returned.*/

extern int dwarf_get_TAG_name(unsigned int /*val_in*/, const char ** /*s_out */);
extern int dwarf_get_children_name(unsigned int /*val_in*/, const char ** /*s_out */);
extern int dwarf_get_FORM_name(unsigned int /*val_in*/, const char ** /*s_out */);
extern int dwarf_get_AT_name(unsigned int /*val_in*/, const char ** /*s_out */);
extern int dwarf_get_OP_name(unsigned int /*val_in*/, const char ** /*s_out */);
extern int dwarf_get_ATE_name(unsigned int /*val_in*/, const char ** /*s_out */);
extern int dwarf_get_DS_name(unsigned int /*val_in*/, const char ** /*s_out */);
extern int dwarf_get_END_name(unsigned int /*val_in*/, const char ** /*s_out */);
extern int dwarf_get_ATCF_name(unsigned int /*val_in*/, const char ** /*s_out */);
extern int dwarf_get_ACCESS_name(unsigned int /*val_in*/, const char ** /*s_out */);
extern int dwarf_get_VIS_name(unsigned int /*val_in*/, const char ** /*s_out */);
extern int dwarf_get_VIRTUALITY_name(unsigned int /*val_in*/, const char ** /*s_out */);
extern int dwarf_get_LANG_name(unsigned int /*val_in*/, const char ** /*s_out */);
extern int dwarf_get_ID_name(unsigned int /*val_in*/, const char ** /*s_out */);
extern int dwarf_get_CC_name(unsigned int /*val_in*/, const char ** /*s_out */);
extern int dwarf_get_INL_name(unsigned int /*val_in*/, const char ** /*s_out */);
extern int dwarf_get_ORD_name(unsigned int /*val_in*/, const char ** /*s_out */);
extern int dwarf_get_DSC_name(unsigned int /*val_in*/, const char ** /*s_out */);
extern int dwarf_get_LNS_name(unsigned int /*val_in*/, const char ** /*s_out */);
extern int dwarf_get_LNE_name(unsigned int /*val_in*/, const char ** /*s_out */);
extern int dwarf_get_MACINFO_name(unsigned int /*val_in*/, const char ** /*s_out */);
extern int dwarf_get_CFA_name(unsigned int /*val_in*/, const char ** /*s_out */);
extern int dwarf_get_EH_name(unsigned int /*val_in*/, const char ** /*s_out */);
extern int dwarf_get_FRAME_name(unsigned int /*val_in*/, const char ** /*s_out */);
extern int dwarf_get_CHILDREN_name(unsigned int /*val_in*/, const char ** /*s_out */);
extern int dwarf_get_ADDR_name(unsigned int /*val_in*/, const char ** /*s_out */);

#ifdef __cplusplus
}
#endif
#endif /* _LIBDWARF_H */


