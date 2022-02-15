/*
	Copyright (C) 2009-2021 DeSmuME team

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

#ifndef _ENCRYPT_H_
#define _ENCRYPT_H_
#include "types.h"
#include <string.h>

struct _KEY1
{
	_KEY1(const u8 *inKeyBufPtr)
	{
		memset(keyBuf, 0, sizeof(keyBuf));
		memset(keyCode, 0, sizeof(keyCode));
		keyBufPtr = inKeyBufPtr;
	}

	u32 keyBuf[0x412];
	u32 keyCode[3];
	const u8 *keyBufPtr;

	void init(u32 idcode, u8 level, u8 modulo);
	void applyKeycode(u8 modulo);
	void decrypt(u32 *ptr);
	void encrypt(u32 *ptr);
};

struct _KEY2
{
private:
	u64 seed0;
	u64 seed1;
	u64 x;
	u64 y;

	u64 bitsReverse39(u64 key);

public:
	_KEY2()
	{
		seed0 = 0x58C56DE0E8ULL;
		seed1 = 0x5C879B9B05ULL;
	}
	
	void applySeed(u8 PROCNUM);
	u8 apply(u8 data);
};

#endif
