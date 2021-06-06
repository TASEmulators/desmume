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
#ifdef HAVE_LIBZZIP
#include <zzip/zzip.h>
#endif

#include "utils/xstring.h"

#ifdef WIN32
#define stat(...) _stat(__VA_ARGS__)
#define S_IFMT _S_IFMT
#define S_IFREG _S_IFREG
#endif

ROMReader_struct * ROMReaderInit(char ** filename)
{
#ifdef xHAVE_LIBZ
	if(!strcasecmp(".gz", *filename + (strlen(*filename) - 3)))
	{
		(*filename)[strlen(*filename) - 3] = '\0';
		return &GZIPROMReader;
	}
#endif
#ifdef HAVE_LIBZZIP
	if (!strcasecmp(".zip", *filename + (strlen(*filename) - 4)))
	{
		(*filename)[strlen(*filename) - 4] = '\0';
		return &ZIPROMReader;
	}
#endif
	return &STDROMReader;
}

extern u8* romBuffer;
extern int romLen;

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
	long pos;
};

void* STDROMReaderInit(const char* filename)
{

	STDROMReaderData* ret = new STDROMReaderData();
	ret->pos = 0;
	return (void*)ret;
}


void STDROMReaderDeInit(void * file)
{
	delete ((STDROMReaderData*)file);
}

u32 STDROMReaderSize(void * file)
{

	return romLen;
}

int STDROMReaderSeek(void * file, int offset, int whence)
{
	STDROMReaderData* data = (STDROMReaderData*)file;

	if (whence == SEEK_SET) {
		data->pos = offset;
	}
	if (whence == SEEK_CUR) {
		data->pos += offset;
	}
	if (whence == SEEK_END) {
		data->pos = romLen + offset;
	}
	return 1;
}

int STDROMReaderRead(void * file, void * buffer, u32 size)
{
	int pos = ((STDROMReaderData*)file)->pos;
	int read = size;
	if (pos + read > romLen)  {
		read = romLen - pos;
	}
	memcpy(buffer,romBuffer + pos, read);
	((STDROMReaderData*)file)->pos += read;
	return read;
}

int STDROMReaderWrite(void *, void *, u32)
{
	//not supported, for now
	return 0;
}

#ifdef xHAVE_LIBZ
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

#ifdef HAVE_LIBZZIP
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
	ZZIP_DIR * dir = zzip_opendir(filename);
	ZZIP_DIRENT * dirent = zzip_readdir(dir);
	if (dir != NULL)
	{
		char *tmp1;
		char tmp2[1024];

		memset(tmp2,0,sizeof(tmp2));
		tmp1 = strndup(filename, strlen(filename) - 4);
		sprintf(tmp2, "%s/%s", tmp1, dirent->d_name);
		free(tmp1);
		return zzip_fopen(tmp2, "rb");
	}
	return NULL;
}

void ZIPROMReaderDeInit(void * file)
{
	zzip_close((ZZIP_FILE*)file);
}

u32 ZIPROMReaderSize(void * file)
{
	u32 size;

	zzip_seek((ZZIP_FILE*)file, 0, SEEK_END);
	size = zzip_tell((ZZIP_FILE*)file);
	zzip_seek((ZZIP_FILE*)file, 0, SEEK_SET);

	return size;
}

int ZIPROMReaderSeek(void * file, int offset, int whence)
{
	return zzip_seek((ZZIP_FILE*)file, offset, whence);
}

int ZIPROMReaderRead(void * file, void * buffer, u32 size)
{
#ifdef ZZIP_OLD_READ
	return zzip_read((ZZIP_FILE*)file, (char *) buffer, size);
#else
	return zzip_read((ZZIP_FILE*)file, buffer, size);
#endif
}

int ZIPROMReaderWrite(void *, void *, u32)
{
	//not supported ever
	return 0;
}
#endif

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

	if(todo==1) *(u8*)buffer = ((u8*)mem.buf)[mem.pos];
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
