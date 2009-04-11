/*  Copyright (C) 2006 Normmatt
    Copyright (C) 2007 Pascal Giard

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

#include "types.h"

#define SRAM_ADDRESS	0x0A000000
#define SRAM_SIZE	0x10000
#define NB_STATES       10

typedef struct 
{
  BOOL exists;
  char date[40];
} savestates_t;


struct SFORMAT
{
	//a string description of the element
	const char *desc;

	//the size of each element
	u32 size;

	//the number of each element
	u32 count;

	//a void* to the data or a void** to the data
	void *v;
};

extern savestates_t savestates[NB_STATES];

void clear_savestates();
void scan_savestates();
u8 sram_read (u32 address);
void sram_write (u32 address, u8 value);
int sram_load (const char *file_name);
int sram_save (const char *file_name);

bool savestate_load (const char *file_name);
bool savestate_save (const char *file_name);

void savestate_slot(int num);
void loadstate_slot(int num);

#endif
