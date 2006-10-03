#ifndef _SRAM_H
#define _SRAM_H

#ifdef __cplusplus
extern "C" {
#endif

#include "types.h"

	#define SRAM_ADDRESS	0x0A000000
	#define SRAM_SIZE		0x10000

        u8 sram_read (u32 address);
        void sram_write (u32 address, u8 value);
        int sram_load (const char *file_name);
        int sram_save (const char *file_name);
	
        int savestate_load (const char *file_name);
        int savestate_save (const char *file_name);

#ifdef __cplusplus
}
#endif

#endif
