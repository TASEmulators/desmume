#include "fs.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <stdio.h>
#include <string.h>

void * FsReadFirst(const char * path, FsEntry * entry) {
	DIR * dir;
	struct dirent * e;
	struct stat s;

	/* hack: reading a directory gives relative file names
	 * and there's no way to know that directory from
	 * DIR, so we're changing working directory... */ 
	chdir(path);

	dir = opendir(path);
	if (!dir)
		return NULL;

	e = readdir(dir);
	if (!e)
		return NULL;

	strcpy(entry->cFileName, e->d_name);
	strncpy(entry->cAlternateFileName, e->d_name, 12);
	entry->flags = 0;

	stat(e->d_name, &s);
	if (s.st_mode & S_IFDIR) {
		entry->flags = FS_IS_DIR;
	}

	return dir;
}

int FsReadNext(void * search, FsEntry * entry) {
	struct dirent * e;
	struct stat s;

	e = readdir(search);
	if (!e)
		return 0;

	strcpy(entry->cFileName, e->d_name);
	strncpy(entry->cAlternateFileName, e->d_name, 12);
	entry->flags = 0;

	stat(e->d_name, &s);
	if (S_ISDIR(s.st_mode)) {
		entry->flags = FS_IS_DIR;
	}

	return 1;
}

void FsClose(void * search) {
	DIR * dir = search;

	closedir(dir);
}

int FsError(void) {
	return 0;
}
