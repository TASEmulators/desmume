/*
	Copyright (C) 2006-2017 DeSmuME team

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

#include "readwrite.h"


size_t read_32LE(u32 &u32valueOut, std::istream *is)
{
	u32 temp;
	if (is->read((char*)&temp,4).gcount() != 4)
		return 0;
	
	u32valueOut = LE_TO_LOCAL_32(temp);
	
	return 1;
}

size_t read_16LE(u16 &u16valueOut, std::istream *is)
{
	u16 temp;
	if (is->read((char*)&temp,2).gcount() != 2)
		return 0;
	
	u16valueOut = LE_TO_LOCAL_16(temp);
	
	return 1;
}
