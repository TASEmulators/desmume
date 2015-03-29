/*  Copyright (C) 2006 thoduv
    Copyright (C) 2006-2007 Theo Berkau

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

#include <stdlib.h>
#include "debug.h"
#include "types.h"
#include "mc.h"

#define FW_CMD_READ             0x3
#define FW_CMD_WRITEDISABLE     0x4
#define FW_CMD_READSTATUS       0x5
#define FW_CMD_WRITEENABLE      0x6
#define FW_CMD_PAGEWRITE        0xA

#define BM_CMD_AUTODETECT       0xFF
#define BM_CMD_WRITESTATUS      0x1
#define BM_CMD_WRITELOW         0x2
#define BM_CMD_READLOW          0x3
#define BM_CMD_WRITEDISABLE     0x4
#define BM_CMD_READSTATUS       0x5
#define BM_CMD_WRITEENABLE      0x6
#define BM_CMD_WRITEHIGH        0xA
#define BM_CMD_READHIGH         0xB

/* FLASH*/
#define COMM_PAGE_WRITE		0x0A
#define COMM_PAGE_ERASE		0xDB
#define COMM_SECTOR_ERASE	0xD8
#define COMM_CHIP_ERASE		0xC7
#define CARDFLASH_READ_BYTES_FAST	0x0B    /* Not used*/
#define CARDFLASH_DEEP_POWDOWN		0xB9    /* Not used*/
#define CARDFLASH_WAKEUP			0xAB    /* Not used*/

void mc_init(memory_chip_t *mc, int type)
{
        mc->com = 0;
        mc->addr = 0;
        mc->addr_shift = 0;
        mc->data = NULL;
        mc->size = 0;
        mc->write_enable = FALSE;
        mc->writeable_buffer = FALSE;
        mc->type = type;
        mc->autodetectsize = 0;
                               
        switch(mc->type)
        {
           case MC_TYPE_EEPROM1:
              mc->addr_size = 1;
              break;
           case MC_TYPE_EEPROM2:
           case MC_TYPE_FRAM:
              mc->addr_size = 2;
              break;
           case MC_TYPE_FLASH:
              mc->addr_size = 3;
              break;
           default: break;
        }
}

u8 *mc_alloc(memory_chip_t *mc, u32 size)
{
	u8 *buffer;
	buffer = malloc(size);

	mc->data = buffer;
	if(!buffer) { return NULL; }
	mc->size = size;
	mc->writeable_buffer = TRUE;

	return buffer;
}

void mc_free(memory_chip_t *mc)
{
	if(mc->data)
	{
		free(mc->data);
		mc->data = 0;
	}
	mc_init(mc, 0);
}

void mc_realloc(memory_chip_t *mc, int type, u32 size)
{
	mc_free(mc);
	mc_init(mc, type);
	mc_alloc(mc, size);
}


void mc_reset_com(memory_chip_t *mc)
{
}
u8 fw_transfer(memory_chip_t *mc, u8 data)
{
	return 0;
}
u8 bm_transfer(memory_chip_t *mc, u8 data)
{
	return 0;
}

