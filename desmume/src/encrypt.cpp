/*
	Copyright (C) 2009-2013 DeSmuME team

	This file is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 2 of the License, or
	(at your option) any later version.

	This file is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with the this software.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "encrypt.h"
#include "MMU.h"

//================================================================================== KEY1
#define bswap32(val) (((val & 0x000000FF) << 24) | ((val & 0x0000FF00) << 8) | ((val & 0x00FF0000) >> 8) | ((val & 0xFF000000) >> 24))
#define DWNUM(i) ((i) >> 2)

void _KEY1::init(u32 idcode, u8 level, u8 modulo)
{
	memcpy(keyBuf, keyBufPtr, 0x1048);
	keyCode[0] = idcode;
	keyCode[1] = idcode >> 1;
	keyCode[2] = idcode << 1;
	if (level >= 1)				// first apply (always)
		applyKeycode(modulo);
	if (level >= 2)				// second apply (optional)
		applyKeycode(modulo);
	keyCode[1] <<= 1;
	keyCode[2] >>= 1;
	if (level >= 3)				// third apply (optional)
		applyKeycode(modulo);

	printf("keycode1: %08X%08X%08X\n", keyCode[2], keyCode[1], keyCode[0]);
}

void _KEY1::applyKeycode(u8 modulo)
{
	encrypt(&keyCode[1]);
	encrypt(&keyCode[0]);
	
	u32 scratch[2] = {0};

	for (u32 i = 0; i <= 0x44; i += 4)			// xor with reversed byte-order (bswap)
		keyBuf[DWNUM(i)] ^= bswap32(keyCode[DWNUM(i % modulo)]);

	for (u32 i = 0; i <= 0x1040; i += 8)
	{
		encrypt(scratch);						// encrypt S (64bit) by keybuf
		keyBuf[DWNUM(i)] = scratch[1];			// write S to keybuf (first upper 32bit)
		keyBuf[DWNUM(i+4)] = scratch[0];		// write S to keybuf (then lower 32bit)
	}
}

void _KEY1::decrypt(u32 *ptr)
{
	u32 y = ptr[0];
	u32 x = ptr[1];

	for (u32 i = 0x11; i >= 0x02; i--)
	{
		u32 z = keyBuf[i] ^ x;
		x = keyBuf[DWNUM(0x048 + (((z >> 24) & 0xFF) << 2))];
		x = keyBuf[DWNUM(0x448 + (((z >> 16) & 0xFF) << 2))] + x;
		x = keyBuf[DWNUM(0x848 + (((z >>  8) & 0xFF) << 2))] ^ x;
		x = keyBuf[DWNUM(0xC48 + (((z >>  0) & 0xFF) << 2))] + x;
		x = y ^ x;
		y = z;
	}
	ptr[0] = x ^ keyBuf[DWNUM(0x04)];
	ptr[1] = y ^ keyBuf[DWNUM(0x00)];
}

void _KEY1::encrypt(u32 *ptr)
{
	u32 y = ptr[0];
	u32 x = ptr[1];

	for (u32 i = 0x00; i <= 0x0F; i++)
	{
		u32 z = keyBuf[i] ^ x;
		x = keyBuf[DWNUM(0x048 + (((z >> 24) & 0xFF) << 2))];
		x = keyBuf[DWNUM(0x448 + (((z >> 16) & 0xFF) << 2))] + x;
		x = keyBuf[DWNUM(0x848 + (((z >>  8) & 0xFF) << 2))] ^ x;
		x = keyBuf[DWNUM(0xC48 + (((z >>  0) & 0xFF) << 2))] + x;
		x = y ^ x;
		y = z;
	}

	ptr[0] = x ^ keyBuf[DWNUM(0x40)];
	ptr[1] = y ^ keyBuf[DWNUM(0x44)];
}
#undef DWNUM
#undef bswap32

//================================================================================== KEY2
void _KEY2::applySeed(u8 PROCNUM)
{
	u64 tmp = (MMU_read8(PROCNUM, REG_ENCSEED0H) & 0xFF);
	seed0 = MMU_read32(PROCNUM, REG_ENCSEED0L) | (tmp << 32);
	tmp = (MMU_read8(PROCNUM, REG_ENCSEED1H) & 0xFF);
	seed1 = MMU_read32(PROCNUM, REG_ENCSEED1L) | (tmp << 32);
	printf("ARM%c: set KEY2 seed0 to %010llX\n", PROCNUM?'7':'9', seed0);
	printf("ARM%c: set KEY2 seed1 to %010llX\n", PROCNUM?'7':'9', seed1);
}
