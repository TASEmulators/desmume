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

#include <string>
#include "types.h"

#ifndef DESMUME_NAME
#define DESMUME_NAME "DeSmuME"
#endif // !DESMUME_NAME

extern const u8 DESMUME_VERSION_MAJOR;
extern const u8 DESMUME_VERSION_MINOR;
extern const u8 DESMUME_VERSION_BUILD;

u32 EMU_DESMUME_VERSION_NUMERIC();
const char* EMU_DESMUME_VERSION_STRING();
const char* EMU_DESMUME_SUBVERSION_STRING();
const char* EMU_DESMUME_NAME_AND_VERSION();
const char* EMU_DESMUME_COMPILER_DETAIL();
