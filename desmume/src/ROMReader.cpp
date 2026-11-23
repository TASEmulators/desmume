/*
	Copyright 2007 Guillaume Duhamel
	Copyright 2007-2017 DeSmuME team

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

#include "ROMReader.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "utils/xstring.h"
#include "utils/ziparchive.h"

#ifdef WIN32
#define stat(...) _stat(__VA_ARGS__)
#define S_IFMT _S_IFMT
#define S_IFREG _S_IFREG
#endif

ROMReader_struct * ROMReaderInit(char ** filename)
{
	const size_t length = strlen(*filename);
#ifdef HAVE_LIBZ
	if (length >= 3 && !strcasecmp(".gz", *filename + (length - 3)))
	{
		(*filename)[length - 3] = '\0';
		return &GZIPROMReader;
	}
#endif
	if (length >= 4 && !strcasecmp(".zip", *filename + (length - 4)))
	{
		(*filename)[length - 4] = '\0';
		return &ZIPROMReader;
	}
	return &STDROMReader;
}

void * STDROMReaderInit(const char * filename);
void STDROMReaderDeInit(void *);
u32 STDROMReaderSize(void *);
int STDROMReaderSeek(void *, int, int);
int STDROMReaderRead(void *, void *, u32);
int STDROMReaderWrite(void *, void *, u32);

ROMReader_struct STDROMReader =
{
	ROMREADER_STD,
	"Standard ROM Reader",
	STDROMReaderInit,
	STDROMReaderDeInit,
	STDROMReaderSize,
	STDROMReaderSeek,
	STDROMReaderRead,
	STDROMReaderWrite
};

struct STDROMReaderData
{
	FILE* file;
	long pos;
};

void* STDROMReaderInit(const char* filename)
{
#if !defined(_MSC_VER) && !defined(__MINGW32__)
	struct stat sb;
	if (stat(filename, &sb) == -1)
		return 0;

 	if ((sb.st_mode & S_IFMT) != S_IFREG)
		return 0;
#endif
	
#ifdef WIN32
	FILE* inf = _wfopen(mbstowcs((std::string)filename).c_str(),L"rb");
#else
	FILE* inf = fopen(filename, "rb");
#endif
	
	if(!inf) return NULL;

	STDROMReaderData* ret = new STDROMReaderData();
	ret->file = inf;
	ret->pos = 0;
	return (void*)ret;
}


void STDROMReaderDeInit(void * file)
{
	if (!file) return ;
	fclose(((STDROMReaderData*)file)->file);
	delete ((STDROMReaderData*)file);
}

u32 STDROMReaderSize(void * file)
{
	u32 size;

	if (!file) return 0;

	FILE* inf = ((STDROMReaderData*)file)->file;

	fseek(inf, 0, SEEK_END);
	size = ftell(inf);
	fseek(inf, ((STDROMReaderData*)file)->pos, SEEK_SET);

	return size;
}

int STDROMReaderSeek(void * file, int offset, int whence)
{
	//not normal fseek return value meanings. awesome.
	if (!file) return 0;
	if(whence == SEEK_SET && offset == ((STDROMReaderData*)file)->pos)
		return 1;
	fseek(((STDROMReaderData*)file)->file, offset, whence);
	((STDROMReaderData*)file)->pos = ftell(((STDROMReaderData*)file)->file);
	return 1;
}

int STDROMReaderRead(void * file, void * buffer, u32 size)
{
	if (!file) return 0;
	int read = fread(buffer, 1, size, ((STDROMReaderData*)file)->file);
	((STDROMReaderData*)file)->pos += read;
	return read;
}

int STDROMReaderWrite(void *, void *, u32)
{
	//not supported, for now
	return 0;
}

#ifdef HAVE_LIBZ
void * GZIPROMReaderInit(const char * filename);
void GZIPROMReaderDeInit(void *);
u32 GZIPROMReaderSize(void *);
int GZIPROMReaderSeek(void *, int, int);
int GZIPROMReaderRead(void *, void *, u32);
int GZIPROMReaderWrite(void *, void *, u32);

ROMReader_struct GZIPROMReader =
{
	ROMREADER_GZIP,
	"Gzip ROM Reader",
	GZIPROMReaderInit,
	GZIPROMReaderDeInit,
	GZIPROMReaderSize,
	GZIPROMReaderSeek,
	GZIPROMReaderRead,
	GZIPROMReaderWrite
};

void * GZIPROMReaderInit(const char * filename)
{
	return (void*)gzopen(filename, "rb");
}

void GZIPROMReaderDeInit(void * file)
{
	gzclose((gzFile)file);
}

u32 GZIPROMReaderSize(void * file)
{
	char useless[1024];
	u32 size = 0;

	/* FIXME this function should first save the current
	 * position and restore it after size calculation */
	gzrewind((gzFile)file);
	while (gzeof ((gzFile)file) == 0)
		size += gzread((gzFile)file, useless, 1024);
	gzrewind((gzFile)file);

	return size;
}

int GZIPROMReaderSeek(void * file, int offset, int whence)
{
	return gzseek((gzFile)file, offset, whence);
}

int GZIPROMReaderRead(void * file, void * buffer, u32 size)
{
	return gzread((gzFile)file, buffer, size);
}

int GZIPROMReaderWrite(void *, void *, u32)
{
	//not supported, ever
	return 0;
}
#endif

namespace
{
	struct ZIPROMReaderData
	{
		unsigned char* data;
		u32 size;
		u32 pos;
	};

	bool ZipNameEndsWith(const char* name, const char* suffix)
	{
		const size_t nameLength = strlen(name);
		const size_t suffixLength = strlen(suffix);
		if (nameLength < suffixLength) return false;
		for (size_t i = 0; i < suffixLength; i++)
		{
			if (tolower((unsigned char)name[nameLength - suffixLength + i]) !=
			    tolower((unsigned char)suffix[i])) return false;
		}
		return true;
	}
}

void * ZIPROMReaderInit(const char * filename);
void ZIPROMReaderDeInit(void *);
u32 ZIPROMReaderSize(void *);
int ZIPROMReaderSeek(void *, int, int);
int ZIPROMReaderRead(void *, void *, u32);
int ZIPROMReaderWrite(void *, void *, u32);

ROMReader_struct ZIPROMReader =
{
	ROMREADER_ZIP,
	"Zip ROM Reader",
	ZIPROMReaderInit,
	ZIPROMReaderDeInit,
	ZIPROMReaderSize,
	ZIPROMReaderSeek,
	ZIPROMReaderRead,
	ZIPROMReaderWrite
};

void * ZIPROMReaderInit(const char * filename)
{
	ZipArchive archive(filename);
	if (!archive.IsValid()) return NULL;

	int chosen = -1;
	int bestScore = 0;
	const int count = archive.GetEntryCount();
	for (int i = 0; i < count; i++)
	{
		ZipEntryInfo info;
		if (!archive.GetEntryInfo(i, &info)) continue;
		if (info.uncompressedSize == 0 || ZipNameEndsWith(info.name, "/")) continue;

		int score = 1;
		if (ZipNameEndsWith(info.name, ".nds") || ZipNameEndsWith(info.name, ".srl")) score = 3;
		else if (ZipNameEndsWith(info.name, ".ds.gba") || ZipNameEndsWith(info.name, ".gba")) score = 2;

		if (score > bestScore)
		{
			bestScore = score;
			chosen = i;
			if (score == 3) break;
		}
	}
	if (chosen < 0) return NULL;

	ZipEntryInfo info;
	if (!archive.GetEntryInfo(chosen, &info) || info.uncompressedSize == 0) return NULL;

	ZIPROMReaderData* data = new ZIPROMReaderData;
	data->data = new unsigned char[info.uncompressedSize];
	data->size = info.uncompressedSize;
	data->pos = 0;

	const int extracted = archive.ExtractEntry(chosen, data->data, info.uncompressedSize);
	if (extracted <= 0)
	{
		delete[] data->data;
		delete data;
		return NULL;
	}
	data->size = (u32)extracted;
	return data;
}

void ZIPROMReaderDeInit(void * file)
{
	ZIPROMReaderData* data = (ZIPROMReaderData*)file;
	if (!data) return;
	delete[] data->data;
	delete data;
}

u32 ZIPROMReaderSize(void * file)
{
	return ((ZIPROMReaderData*)file)->size;
}

int ZIPROMReaderSeek(void * file, int offset, int whence)
{
	ZIPROMReaderData* data = (ZIPROMReaderData*)file;
	s64 base = (whence == SEEK_SET) ? 0 : (whence == SEEK_CUR) ? data->pos : data->size;
	s64 position = base + offset;
	if (position < 0) position = 0;
	if (position > (s64)data->size) position = data->size;
	data->pos = (u32)position;
	return (int)data->pos;
}

int ZIPROMReaderRead(void * file, void * buffer, u32 size)
{
	ZIPROMReaderData* data = (ZIPROMReaderData*)file;
	if (data->pos >= data->size) return 0;
	if (size > data->size - data->pos) size = data->size - data->pos;
	memcpy(buffer, data->data + data->pos, size);
	data->pos += size;
	return (int)size;
}

int ZIPROMReaderWrite(void *, void *, u32)
{
	//not supported ever
	return 0;
}

struct {
	void* buf;
	int len;
	int pos;
} mem;

void * MemROMReaderInit(const char * filename)
{
	return NULL; //dummy
}

void MemROMReaderDeInit(void *)
{
	//nothing to do
}
u32 MemROMReaderSize(void *)
{
	return (u32)mem.len;
}
int MemROMReaderSeek(void * file, int offset, int whence)
{
	switch(whence) {
	case SEEK_SET:
		mem.pos = offset;
		break;
	case SEEK_CUR:
		mem.pos += offset;
		break;
	case SEEK_END:
		mem.pos = mem.len + offset;
		break;
	}
	return mem.pos;
}

int MemROMReaderRead(void * file, void * buffer, u32 size)
{
	if(mem.pos<0) return 0;

	int todo = (int)size;
	int remain = mem.len - mem.pos;
	if(remain<todo)
		todo = remain;

	if(todo<=0)
		return 0;
	else if(todo==1) *(u8*)buffer = ((u8*)mem.buf)[mem.pos];
	else memcpy(buffer,(u8*)mem.buf + mem.pos, todo);
	mem.pos += todo;
	return todo;
}

int MemROMReaderWrite(void * file, void * buffer, u32 size)
{
	if(mem.pos<0) return 0;

	int todo = (int)size;
	int remain = mem.len - mem.pos;
	if(remain<todo)
		todo = remain;

	if (todo == 1) ((u8*)mem.buf)[mem.pos] = *(u8*)buffer;
	else memcpy((u8*)mem.buf + mem.pos,buffer, todo);
	mem.pos += todo;
	return todo;
}

static ROMReader_struct MemROMReader =
{
	ROMREADER_MEM,
	"Memory ROM Reader",
	MemROMReaderInit,
	MemROMReaderDeInit,
	MemROMReaderSize,
	MemROMReaderSeek,
	MemROMReaderRead,
	MemROMReaderWrite,
};

ROMReader_struct * MemROMReaderRead_TrueInit(void* buf, int length)
{
	mem.buf = buf;
	mem.len = length;
	mem.pos = 0;
	return &MemROMReader;
}
