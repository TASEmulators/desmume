/*  Copyright (C) 2006 Guillaume Duhamel

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

#ifndef FS_H
#define FS_H

#include "types.h"

#define FS_IS_DIR 1

#define FS_ERR_UNKNOWN -1
#define FS_ERR_NO_MORE_FILES 1

extern const char FS_SEPARATOR;

typedef struct {
	char cFileName[256];
	char cAlternateFileName[14];
	u32 flags;
        u32 fileSize;
} FsEntry;

void * FsReadFirst(const char * path, FsEntry * entry);
int FsReadNext(void * search, FsEntry * entry);
void FsClose(void * search);
int FsError(void);

#endif
