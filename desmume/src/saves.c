#ifdef HAVE_LIBZ
#include <zlib.h>
#endif
#include <stdio.h>
#include "saves.h"
#include "MMU.hpp"

u8 sram_read (u32 address) {
	address = address - SRAM_ADDRESS;
	
	if ( address > SRAM_SIZE )
		return 0;

	return MMU.CART_RAM[address];

}

void sram_write (u32 address, u8 value) {

	address = address - SRAM_ADDRESS;

	if ( address < SRAM_SIZE )
		MMU.CART_RAM[address] = value;
	
}

int sram_load (const char *file_name) {
	
	FILE *file;

	file = fopen ( file_name, "rb" );
	if( file == NULL )
		return 0;

	fread ( MMU.CART_RAM, SRAM_SIZE, 1, file );

	fclose ( file );

	return 1;

}

int sram_save (const char *file_name) {

	FILE *file;

	file = fopen ( file_name, "wb" );
	if( file == NULL )
		return 0;

	fwrite ( MMU.CART_RAM, SRAM_SIZE, 1, file );

	fclose ( file );

	return 1;

}

int savestate_load (const char *file_name)    {
#ifdef HAVE_LIBZ
	
	gzFile file;

	file = gzopen( file_name, "rb" );
	if( file == NULL )
		return 0;

        gzread ( file, ARM9Mem.ARM9_ITCM, 0x8000 );
        gzread ( file, ARM9Mem.ARM9_DTCM, 0x4000 );
	////fread ( MMU::ARM9_WRAM, 0x1000000, 1, file );
    gzread ( file, ARM9Mem.MAIN_MEM, 0x400000 );
    gzread ( file, ARM9Mem.ARM9_REG, 0x10000 );
    gzread ( file, ARM9Mem.ARM9_VMEM, 0x800 );
    gzread ( file, ARM9Mem.ARM9_OAM, 0x800 );
    
    gzread ( file, ARM9Mem.ARM9_ABG, 0x80000 );
    gzread ( file, ARM9Mem.ARM9_BBG, 0x20000 );
    gzread ( file, ARM9Mem.ARM9_AOBJ, 0x40000 );
    gzread ( file, ARM9Mem.ARM9_BOBJ, 0x20000 );
    gzread ( file, ARM9Mem.ARM9_LCD, 0xA4000 );
    
    //ARM7 memory
    //gzread ( file, MMU::ARM7_ERAM, 0x10000 );
    //gzread ( file, MMU::ARM7_REG, 0x10000 );
    //gzread ( file, MMU::ARM7_WIRAM, 0x10000 );

	gzclose ( file );

	return 1;
#endif
}

int savestate_save (const char *file_name)    {
#ifdef HAVE_LIBZ
	gzFile file;

	file = gzopen( file_name, "wb" );
	if( file == NULL )
		return 0;

        gzwrite ( file, ARM9Mem.ARM9_ITCM, 0x8000 );
        gzwrite ( file ,ARM9Mem.ARM9_DTCM, 0x4000 );
	////fwrite ( MMU::ARM9_WRAM, 0x1000000, 1, file );
    gzwrite ( file, ARM9Mem.MAIN_MEM, 0x400000 );
    gzwrite ( file, ARM9Mem.ARM9_REG, 0x10000 );
    gzwrite ( file, ARM9Mem.ARM9_VMEM, 0x800 );
    gzwrite ( file, ARM9Mem.ARM9_OAM, 0x800 );
    
    gzwrite ( file, ARM9Mem.ARM9_ABG, 0x80000 );
    gzwrite ( file, ARM9Mem.ARM9_BBG, 0x20000 );
    gzwrite ( file, ARM9Mem.ARM9_AOBJ, 0x40000 );
    gzwrite ( file, ARM9Mem.ARM9_BOBJ, 0x20000 );
    gzwrite ( file, ARM9Mem.ARM9_LCD, 0xA4000 );
    
    //ARM7 memory
    //gzwrite ( file, MMU::ARM7_ERAM, 0x10000 );
    //gzwrite ( file, MMU::ARM7_REG, 0x10000 );
    //gzwrite ( file, MMU::ARM7_WIRAM, 0x10000 );

	gzclose ( file );

	return 1;
#endif
}

