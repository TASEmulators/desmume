#ifndef DWARF_UTIL_H
#define DWARF_UTIL_H
/*

  Copyright (C) 2000,2003,2004 Silicon Graphics, Inc.  All Rights Reserved.
  Portions Copyright (C) 2007-2010 David Anderson. All Rights Reserved.

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
/* The address of the Free Software Foundation is
   Free Software Foundation, Inc., 51 Franklin St, Fifth Floor, 
   Boston, MA 02110-1301, USA.
   SGI has moved from the Crittenden Lane address.
*/




/*
    Decodes unsigned leb128 encoded numbers.
    Make sure ptr is a pointer to a 1-byte type.  
    In 2003 and earlier this was a hand-inlined
    version of _dwarf_decode_u_leb128() which did
    not work correctly if Dwarf_Word was 64 bits.
*/
#define DECODE_LEB128_UWORD(ptr, value) \
    do { \
       Dwarf_Word uleblen; \
	value = _dwarf_decode_u_leb128(ptr,&uleblen); \
        ptr += uleblen; \
    } while (0)

/*
    Decodes signed leb128 encoded numbers.
    Make sure ptr is a pointer to a 1-byte type.
    In 2003 and earlier this was a hand-inlined
    version of _dwarf_decode_s_leb128() which did
    not work correctly if Dwarf_Word was 64 bits.

*/
#define DECODE_LEB128_SWORD(ptr, value) \
    do { \
       Dwarf_Word sleblen; \
	value = _dwarf_decode_s_leb128(ptr,&sleblen); \
        ptr += sleblen; \
    } while(0)


/*
    Skips leb128_encoded numbers that are guaranteed 
    to be no more than 4 bytes long.  Same for both
    signed and unsigned numbers.
*/
#define SKIP_LEB128_WORD(ptr) \
    do{ if ((*(ptr++) & 0x80) != 0) { \
        if ((*(ptr++) & 0x80) != 0) { \
            if ((*(ptr++) & 0x80) != 0) { \
	        if ((*(ptr++) & 0x80) != 0) { \
	        } \
	    } \
        } \
    } } while (0)


#define CHECK_DIE(die, error_ret_value) \
do {if (die == NULL) { \
	_dwarf_error(NULL, error, DW_DLE_DIE_NULL); \
	return(error_ret_value); \
    } \
    if (die->di_cu_context == NULL) { \
	_dwarf_error(NULL, error, DW_DLE_DIE_NO_CU_CONTEXT); \
	return(error_ret_value); \
    } \
    if (die->di_cu_context->cc_dbg == NULL) { \
	_dwarf_error(NULL, error, DW_DLE_DBG_NULL); \
	return(error_ret_value); \
    }  \
} while (0)


/* 
   Reads 'source' for 'length' bytes from unaligned addr.

   Avoids any constant-in-conditional warnings and
   avoids a test in the generated code (for non-const cases,
	which are in the majority.)
   Uses a temp to avoid the test.
   The decl here should avoid any problem of size in the temp.
   This code is ENDIAN DEPENDENT
   The memcpy args are the endian issue.
*/
typedef Dwarf_Unsigned BIGGEST_UINT;

#ifdef WORDS_BIGENDIAN
#define READ_UNALIGNED(dbg,dest,desttype, source, length) \
    do { \
      BIGGEST_UINT _ltmp = 0;  \
      dbg->de_copy_word( (((char *)(&_ltmp)) + sizeof(_ltmp) - length), \
			source, length) ; \
      dest = (desttype)_ltmp;  \
    } while (0)


/*
    This macro sign-extends a variable depending on the length.
    It fills the bytes between the size of the destination and
    the length with appropriate padding.
    This code is ENDIAN DEPENDENT but dependent only
    on host endianness, not object file endianness.
    The memcpy args are the issue.
*/
#define SIGN_EXTEND(dest, length) \
    do {if (*(Dwarf_Sbyte *)((char *)&dest + sizeof(dest) - length) < 0) {\
	memcpy((char *)&dest, "\xff\xff\xff\xff\xff\xff\xff\xff", \
	    sizeof(dest) - length);  \
        } \
     } while (0)
#else /* LITTLE ENDIAN */

#define READ_UNALIGNED(dbg,dest,desttype, source, length) \
    do  { \
      BIGGEST_UINT _ltmp = 0;  \
      dbg->de_copy_word( (char *)(&_ltmp) , \
                        source, length) ; \
      dest = (desttype)_ltmp;  \
     } while (0)


/*
    This macro sign-extends a variable depending on the length.
    It fills the bytes between the size of the destination and
    the length with appropriate padding.
    This code is ENDIAN DEPENDENT but dependent only
    on host endianness, not object file endianness.
    The memcpy args are the issue.
*/
#define SIGN_EXTEND(dest, length) \
    do {if (*(Dwarf_Sbyte *)((char *)&dest + (length-1)) < 0) {\
        memcpy((char *)&dest+length,    \
                "\xff\xff\xff\xff\xff\xff\xff\xff", \
            sizeof(dest) - length); \
        }  \
    } while (0)

#endif /* ! LITTLE_ENDIAN */



/*
   READ_AREA LENGTH reads the length (the older way
   of pure 32 or 64 bit
   or the new proposed dwarfv2.1 64bit-extension way)

   It reads the bits from where rw_src_data_p  points to 
   and updates the rw_src_data_p to point past what was just read.

   It updates w_length_size (to the size of an offset, either 4 or 8)
   and w_exten_size (set 0 unless this frame has the DWARF3,4 64bit
   extension, in which case w_exten_size is set to 4).

   r_dbg is just the current dbg pointer.
   w_target is the output length field.
   r_targtype is the output type. Always Dwarf_Unsigned so far.
  
*/
/* This one handles the v2.1 64bit extension  
   and 32bit (and   MIPS fixed 64  bit via the
	dwarf_init-set r_dbg->de_length_size)..
   It does not recognize any but the one distingushed value
   (the only one with defined meaning).
   It assumes that no CU will have a length
	0xffffffxx  (32bit length)
	or
	0xffffffxx xxxxxxxx (64bit length)
   which makes possible auto-detection of the extension.

   This depends on knowing that only a non-zero length
   is legitimate (AFAICT), and for IRIX non-standard -64 
   dwarf that the first 32 bits of the 64bit offset will be
   zero (because the compiler could not handle a truly large 
   value as of Jan 2003 and because no app has that much debug 
   info anyway, at least not in the IRIX case).

   At present not testing for '64bit elf' here as that
   does not seem necessary (none of the 64bit length seems 
   appropriate unless it's  ident[EI_CLASS] == ELFCLASS64).
*/
#   define    READ_AREA_LENGTH(r_dbg,w_target,r_targtype,         \
	rw_src_data_p,w_length_size,w_exten_size)                 \
do {    READ_UNALIGNED(r_dbg,w_target,r_targtype,                 \
                rw_src_data_p, ORIGINAL_DWARF_OFFSET_SIZE);       \
    if(w_target == DISTINGUISHED_VALUE) {                         \
	     /* dwarf3 64bit extension */                         \
             w_length_size  = DISTINGUISHED_VALUE_OFFSET_SIZE;    \
             rw_src_data_p += ORIGINAL_DWARF_OFFSET_SIZE;         \
             w_exten_size   = ORIGINAL_DWARF_OFFSET_SIZE;         \
             READ_UNALIGNED(r_dbg,w_target,r_targtype,            \
                  rw_src_data_p, DISTINGUISHED_VALUE_OFFSET_SIZE);\
             rw_src_data_p += DISTINGUISHED_VALUE_OFFSET_SIZE;    \
    } else {                                                      \
	if(w_target == 0 && r_dbg->de_big_endian_object) {        \
	     /* IRIX 64 bit, big endian.  This test */            \
	     /* is not a truly precise test, a precise test */    \
             /* would check if the target was IRIX.  */           \
             READ_UNALIGNED(r_dbg,w_target,r_targtype,            \
                rw_src_data_p, DISTINGUISHED_VALUE_OFFSET_SIZE);  \
	     w_length_size  = DISTINGUISHED_VALUE_OFFSET_SIZE;    \
	     rw_src_data_p += DISTINGUISHED_VALUE_OFFSET_SIZE;    \
	     w_exten_size = 0;                                    \
	} else {                                                  \
	     /* standard 32 bit dwarf2/dwarf3 */                  \
	     w_exten_size   = 0;                                  \
             w_length_size  = ORIGINAL_DWARF_OFFSET_SIZE;         \
             rw_src_data_p += w_length_size;                      \
	}                                                         \
    } } while(0)

Dwarf_Unsigned
_dwarf_decode_u_leb128(Dwarf_Small * leb128,
		       Dwarf_Word * leb128_length);

Dwarf_Signed
_dwarf_decode_s_leb128(Dwarf_Small * leb128,
		       Dwarf_Word * leb128_length);

Dwarf_Unsigned
_dwarf_get_size_of_val(Dwarf_Debug dbg,
    Dwarf_Unsigned form,
    Dwarf_Half address_size,
    Dwarf_Small * val_ptr, 
    int v_length_size);

struct Dwarf_Hash_Table_Entry_s;
/* This single struct is the base for the hash table.
   The intent is that once the total_abbrev_count across
   all the entries is greater than  10*current_table_entry_count
   one should build a new Dwarf_Hash_Table_Base_s, rehash
   all the existing entries, and delete the old table and entries. 
   (10 is a heuristic, nothing magic about it, but once the
   count gets to 30 or 40 times current_table_entry_count
   things really slow down a lot. One (500MB) application had
   127000 abbreviations in one compilation unit)
   The incoming 'code' is an abbrev number and those simply
   increase linearly so the hashing is perfect always.
*/
struct Dwarf_Hash_Table_s {
      unsigned long       tb_table_entry_count;
      unsigned long       tb_total_abbrev_count;
      /* Each table entry is a list of abbreviations. */
      struct  Dwarf_Hash_Table_Entry_s *tb_entries;
};

/*
    This struct is used to build a hash table for the
    abbreviation codes for a compile-unit.  
*/
struct Dwarf_Hash_Table_Entry_s {
    Dwarf_Abbrev_List at_head;
};



Dwarf_Abbrev_List
_dwarf_get_abbrev_for_code(Dwarf_CU_Context cu_context,
			   Dwarf_Unsigned code);


/* return 1 if string ends before 'endptr' else
** return 0 meaning string is not properly terminated.
** Presumption is the 'endptr' pts to end of some dwarf section data.
*/
int _dwarf_string_valid(void *startptr, void *endptr);

Dwarf_Unsigned _dwarf_length_of_cu_header(Dwarf_Debug,
					  Dwarf_Unsigned offset);
Dwarf_Unsigned _dwarf_length_of_cu_header_simple(Dwarf_Debug);

int  _dwarf_load_debug_info(Dwarf_Debug dbg, Dwarf_Error *error);
void _dwarf_free_abbrev_hash_table_contents(Dwarf_Debug dbg,
    struct Dwarf_Hash_Table_s* hash_table);
int _dwarf_get_address_size(Dwarf_Debug dbg, Dwarf_Die die);

#endif /* DWARF_UTIL_H */
