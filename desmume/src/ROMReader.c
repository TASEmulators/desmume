#include "ROMReader.h"

#include <stdio.h>

ROMReader_struct * ROMReaderInit(const char ** filename)
{
	if(!strcasecmp(".gz", *filename + (strlen(*filename) - 3)))
	{
		*filename -= 3;
		return &GZIPROMReader;
	}
	else
	{
		return &STDROMReader;
	}
}

void * STDROMReaderInit(const char * filename);
void STDROMReaderDeInit(void *);
u32 STDROMReaderSize(void *);
int STDROMReaderSeek(void *, int, int);
int STDROMReaderRead(void *, void *, u32);

ROMReader_struct STDROMReader =
{
	ROMREADER_STD,
	"Standard ROM Reader",
	STDROMReaderInit,
	STDROMReaderDeInit,
	STDROMReaderSize,
	STDROMReaderSeek,
	STDROMReaderRead
};

void * STDROMReaderInit(const char * filename)
{
	return (void *) fopen(filename, "rb");
}

void STDROMReaderDeInit(void * file)
{
	fclose(file);
}

u32 STDROMReaderSize(void * file)
{
	u32 size;

	fseek(file, 0, SEEK_END);
	size = ftell(file);
	fseek(file, 0, SEEK_SET);

	return size;
}

int STDROMReaderSeek(void * file, int offset, int whence)
{
	return fseek(file, offset, whence);
}

int STDROMReaderRead(void * file, void * buffer, u32 size)
{
	return fread(buffer, 1, size, file);
}

#ifdef HAVE_LIBZ
void * GZIPROMReaderInit(const char * filename);
void GZIPROMReaderDeInit(void *);
u32 GZIPROMReaderSize(void *);
int GZIPROMReaderSeek(void *, int, int);
int GZIPROMReaderRead(void *, void *, u32);

ROMReader_struct GZIPROMReader =
{
	ROMREADER_GZIP,
	"Gzip ROM Reader",
	GZIPROMReaderInit,
	GZIPROMReaderDeInit,
	GZIPROMReaderSize,
	GZIPROMReaderSeek,
	GZIPROMReaderRead
};

void * GZIPROMReaderInit(const char * filename)
{
	return gzopen(filename, "rb");
}

void GZIPROMReaderDeInit(void * file)
{
	gzclose(file);
}

u32 GZIPROMReaderSize(void * file)
{
	char useless[1024];
	u32 size;

	/* FIXME this function should first save the current
	 * position and restore it after size calculation */
	gzrewind(file);
	while (gzeof (file) == 0)
		size += gzread(file, useless, 1024);
	gzrewind(file);

	return size;
}

int GZIPROMReaderSeek(void * file, int offset, int whence)
{
	return gzseek(file, offset, whence);
}

int GZIPROMReaderRead(void * file, void * buffer, u32 size)
{
	return gzread(file, buffer, size);
}
#endif
