#include "fs.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <stdio.h>
#include <string.h>

typedef struct {
	DIR * dir;
	char * path;
} FsLinuxDir;

const char FS_SEPARATOR = '/';

void * FsReadFirst(const char * path, FsEntry * entry) {
	FsLinuxDir * dir;
	struct dirent * e;
	struct stat s;
	char buffer[1024];
	DIR * tmp;

	tmp = opendir(path);
	if (!tmp)
		return NULL;

	e = readdir(tmp);
	if (!e)
		return NULL;

	dir = malloc(sizeof(FsLinuxDir));
	dir->dir = tmp;

	strcpy(entry->cFileName, e->d_name);
	// there's no 8.3 file names support on linux :)
	strcpy(entry->cAlternateFileName, "");
	entry->flags = 0;


	/* hack: reading a directory gives relative file names
	 * and there's no way to know that directory from
	 * DIR, so we're changing working directory... */ 
	chdir(path);
	getcwd(buffer, 1024);
	dir->path = strdup(buffer);

	stat(e->d_name, &s);
	if (S_ISDIR(s.st_mode)) {
		entry->flags = FS_IS_DIR;
	}

	return dir;
}

int FsReadNext(void * search, FsEntry * entry) {
	FsLinuxDir * dir = search;
	struct dirent * e;
	struct stat s;

	e = readdir(dir->dir);
	if (!e)
		return 0;

	strcpy(entry->cFileName, e->d_name);
	// there's no 8.3 file names support on linux :)
	strcpy(entry->cAlternateFileName, "");
	entry->flags = 0;

	chdir(dir->path);

	stat(e->d_name, &s);
	if (S_ISDIR(s.st_mode)) {
		entry->flags = FS_IS_DIR;
	}

	return 1;
}

void FsClose(void * search) {
	DIR * dir = ((FsLinuxDir *) search)->dir;

	closedir(dir);
	free(search);
}

int FsError(void) {
	return 0;
}
