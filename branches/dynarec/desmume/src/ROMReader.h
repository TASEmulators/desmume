/*  Copyright 2007 Guillaume Duhamel

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

#ifdef HAVE_LIBZ
#include <zlib.h>
#endif
#include <string.h>

#include "types.h"

#define ROMREADER_DEFAULT -1
#define ROMREADER_STD	0
#define ROMREADER_GZIP	1
#define ROMREADER_ZIP	2

typedef struct
{
	int id;
	const char * Name;
	void * (*Init)(const char * filename);
	void (*DeInit)(void * file);
	u32 (*Size)(void * file);
	int (*Seek)(void * file, int offset, int whence);
	int (*Read)(void * file, void * buffer, u32 size);
} ROMReader_struct;

extern ROMReader_struct STDROMReader;
#ifdef HAVE_LIBZ
extern ROMReader_struct GZIPROMReader;
#endif
#ifdef HAVE_LIBZZIP
extern ROMReader_struct ZIPROMReader;
#endif

ROMReader_struct * ROMReaderInit(char ** filename);
