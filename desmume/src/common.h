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

//TODO - dismantle this file

#ifndef _COMMON_H_
#define _COMMON_H_

#include <stdio.h>
#include <string.h>
#include <string>
#include "types.h"

// ===============================================================================
// Message dialogs
// ===============================================================================
#define CALL_CONVENTION
typedef struct
{
	void (CALL_CONVENTION*  info)	(const char *fmt, ...);
	bool (CALL_CONVENTION*  confirm)(const char *fmt, ...);
	void (CALL_CONVENTION*  error)	(const char *fmt, ...);
	void (CALL_CONVENTION*  warn)	(const char *fmt, ...);
} msgBoxInterface;

extern msgBoxInterface *msgbox;

// ===============================================================================
// Maker codes
// ===============================================================================
//

void* malloc_aligned(size_t length, size_t alignment);
void* malloc_aligned16(size_t length);
void* malloc_aligned32(size_t length);
void* malloc_aligned64(size_t length);
void* malloc_alignedCacheLine(size_t length);
void* malloc_alignedPage(size_t length);
void free_aligned(void *ptr);

#endif
