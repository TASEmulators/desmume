/*
	Copyright (C) 2008-2017 DeSmuME team

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

#ifndef _READWRITE_H_
#define _READWRITE_H_

#include <iostream>
#include <cstdio>
#include <vector>

#include "types.h"

size_t read_32LE(u32 &u32valueOut, std::istream *is);
size_t read_16LE(u16 &u16valueOut, std::istream *is);

#endif
