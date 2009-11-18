 /* Copyright (C) 2008-2009 DeSmuME team
 *
 * This file is part of DeSmuME
 *
 * DeSmuME is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * DeSmuME is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with DeSmuME; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301  USA
*/

#ifndef _READWRITE_H_
#define _READWRITE_H_

#include "types.h"
#include "emufile.h"
#include <iostream>
#include <cstdio>
#include <vector>

//well. just for the sake of consistency
int write8le(u8 b, EMUFILE *fp);
int write16le(u16 b, EMUFILE* os);
int write32le(u32 b, EMUFILE* os);
int write64le(u64 b, EMUFILE* os);

int read8le(u8 *Bufo, EMUFILE*is);
int read16le(u16 *Bufo, EMUFILE*is);
inline int read16le(s16 *Bufo, EMUFILE*is) { return read16le((u16*)Bufo,is); }
int read32le(u32 *Bufo, EMUFILE*is);
inline int read32le(s32 *Bufo, EMUFILE*is) { return read32le((u32*)Bufo,is); }
int read64le(u64 *Bufo, EMUFILE*is);
int read16le(u16 *Bufo, std::istream *is);


template<typename T>
int readle(T *Bufo, EMUFILE*is)
{
	CTASSERT(sizeof(T)==1||sizeof(T)==2||sizeof(T)==4||sizeof(T)==8);
	switch(sizeof(T)) {
		case 1: return read8le((u8*)Bufo,is);
		case 2: return read16le((u16*)Bufo,is);
		case 4: return read32le((u32*)Bufo,is);
		case 8: return read64le((u64*)Bufo,is);
		default:
			return 0;
	}
}



int readbool(bool *b, EMUFILE* is);
void writebool(bool b, EMUFILE* os);

int readbuffer(std::vector<u8> &vec, EMUFILE* is);
int writebuffer(std::vector<u8>& vec, EMUFILE* os);

#endif
