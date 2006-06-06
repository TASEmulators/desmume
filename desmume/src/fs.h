#ifndef FS_H
#define FS_H

#include "types.h"

#ifdef __cplusplus
extern "C" {
#endif

#define FS_IS_DIR 1

#define FS_ERR_NO_MORE_FILES 1

typedef struct {
	char cFileName[256];
	char cAlternateFileName[14];
	u32 flags;
} FsEntry;

void * FsReadFirst(const char * path, FsEntry * entry);
int FsReadNext(void * search, FsEntry * entry);
void FsClose(void * search);
int FsError(void);

#ifdef __cplusplus
}
#endif

#endif
