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

//TODO - move this into ndssystem where it belongs probably

#include "common.h"

#ifndef _MSC_VER
#include <stdint.h>
#endif

#include <string.h>
#include <string>
#include <stdarg.h>
#include <zlib.h>
#include <stdlib.h>
#include <map>

static std::map<void *, void *> _alignedPtrList; // Key: Aligned pointer / Value: Original pointer

// ===============================================================================
// Message dialogs
// ===============================================================================
#define MSG_PRINT { \
	va_list args; \
	va_start (args, fmt); \
	vprintf (fmt, args); \
	va_end (args); \
}
void msgFakeInfo(const char *fmt, ...)
{
	MSG_PRINT;
}

bool msgFakeConfirm(const char *fmt, ...)
{
	MSG_PRINT;
	return true;
}

void msgFakeError(const char *fmt, ...)
{
	MSG_PRINT;
}

void msgFakeWarn(const char *fmt, ...)
{
	MSG_PRINT;
}

msgBoxInterface msgBoxFake = {
	msgFakeInfo,
	msgFakeConfirm,
	msgFakeError,
	msgFakeWarn,
};

msgBoxInterface *msgbox = &msgBoxFake;

void* malloc_aligned(size_t length, size_t alignment)
{
	const uintptr_t ptrOffset = alignment; // This value must be a power of 2, or this function will fail.
	const uintptr_t ptrOffsetMask = ~(ptrOffset - 1);
	
	void *originalPtr = malloc(length + ptrOffset);
	if (originalPtr == NULL)
	{
		return originalPtr;
	}
	
	void *alignedPtr = (void *)(((uintptr_t)originalPtr + ptrOffset) & ptrOffsetMask);
	_alignedPtrList[alignedPtr] = originalPtr;
	
	return alignedPtr;
}

void* malloc_aligned16(size_t length)
{
	return malloc_aligned(length, 16);
}

void* malloc_aligned32(size_t length)
{
	return malloc_aligned(length, 32);
}

void* malloc_aligned64(size_t length)
{
	return malloc_aligned(length, 64);
}

void* malloc_alignedCacheLine(size_t length)
{
#if defined(HOST_32)
	return malloc_aligned32(length);
#elif defined(HOST_64)
	return malloc_aligned64(length);
#else
	return malloc_aligned16(length);
#endif
}

void* malloc_alignedPage(size_t length)
{
	// WARNING!
	//
	// This may fail for SPARC users, which have a page size
	// of 8KB instead of the more typical 4KB.
	return malloc_aligned(length, 4096);
}

void free_aligned(void *ptr)
{
	if (ptr == NULL)
	{
		return;
	}
	
	// If the input pointer is aligned through malloc_aligned(),
	// then retrieve the original pointer first. Otherwise, this
	// function behaves like the usual free().
	void *originalPtr = ptr;
	if (_alignedPtrList.find(ptr) != _alignedPtrList.end())
	{
		originalPtr = _alignedPtrList[ptr];
		_alignedPtrList.erase(ptr);
	}
	
	free(originalPtr);
}
