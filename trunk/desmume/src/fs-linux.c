#include "fs.h"

#include <sys/types.h>
#include <dirent.h>
#include <stdio.h>
#include <string.h>

void * FsReadFirst(const char * path, FsEntry * entry) {
	DIR * dir;
	struct dirent * e;

	printf("reading %s\n", path);
	dir = opendir(path);
	if (!dir)
		return NULL;

	e = readdir(dir);
	if (!e)
		return NULL;

	strcpy(entry->cFileName, e->d_name);
	strncpy(entry->cAlternateFileName, e->d_name, 12);
	entry->flags = 0;

	return dir;
}

int FsReadNext(void * search, FsEntry * entry) {
	struct dirent * e;

	e = readdir(search);
	if (!e)
		return 0;

	strcpy(entry->cFileName, e->d_name);
	strncpy(entry->cAlternateFileName, e->d_name, 12);
	entry->flags = 0;

	return 1;
}

void FsClose(void * search) {
	DIR * dir = search;

	closedir(dir);
}

int FsError(void) {
	return 0;
}
