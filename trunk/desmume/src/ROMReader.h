#include "types.h"

#define ROMREADER_DEFAULT -1
#define ROMREADER_STD 0
#define ROMREADER_GZIP 1

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

ROMReader_struct * ROMReaderInit(const char ** filename);
