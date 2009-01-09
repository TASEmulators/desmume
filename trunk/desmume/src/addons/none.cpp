/*  Copyright (C) 2006 yopyop
    yopyop156@ifrance.com
    yopyop156.ifrance.com

	Copyright (C) 2009 DeSmuME team

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

#include "../addons.h"
#include <string.h>

BOOL None_init(void) { return (TRUE); };
void None_reset(void) {};
void None_close(void) {};
void None_config(void) {};
void None_write08(u32 adr, u8 val) {};
void None_write16(u32 adr, u16 val) {};
void None_write32(u32 adr, u32 val) {};
u8   None_read08(u32 adr){ return (0); };
u16  None_read16(u32 adr){ return (0); };
u32  None_read32(u32 adr){ return (0); };
void None_info(char *info) { strcpy(info, "Nothing in GBA slot"); };

ADDONINTERFACE addonNone = {
				"NONE",
				None_init,
				None_reset,
				None_close,
				None_config,
				None_write08,
				None_write16,
				None_write32,
				None_read08,
				None_read16,
				None_read32,
				None_info};
