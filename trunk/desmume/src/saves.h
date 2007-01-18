/*  Copyright (C) 2006 Normmatt

    This file is part of DeSmuME

    DeSmuME is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    DeSmuME is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with DeSmuME; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

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


	void savestate_slot(int num);
	void loadstate_slot(int num);

#ifdef __cplusplus
}
#endif

#endif
