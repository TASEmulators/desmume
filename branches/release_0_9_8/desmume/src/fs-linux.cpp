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

#include "fs.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <stdlib.h>
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
	char buffer[512+1]; /* DirSpec[256] + '/' + dirent.d_name[256] */
	DIR * tmp;

	dir = (FsLinuxDir*)malloc(sizeof(FsLinuxDir));
	if (!dir)
		return NULL;

	tmp = opendir(path);
	if (!tmp) {
		free(dir);
		return NULL;
	}
	dir->dir = tmp;

	e = readdir(tmp);
	if (!e) {
		closedir(tmp);
		free(dir);
		return NULL;
	}
	strcpy(entry->cFileName, e->d_name);
	// there's no 8.3 file names support on linux :)
	strcpy(entry->cAlternateFileName, "");
	entry->flags = 0;

	dir->path = strdup(path);
	sprintf(buffer, "%s/%s", dir->path, e->d_name);

	stat(buffer, &s);
	if (S_ISDIR(s.st_mode)) {
		entry->flags = FS_IS_DIR;
		entry->fileSize = 0;
	} else {
		entry->fileSize = s.st_size;
	}

	return dir;
}

int FsReadNext(void * search, FsEntry * entry) {
	FsLinuxDir * dir = (FsLinuxDir*)search;
	struct dirent * e;
	struct stat s;
	char buffer[1024];

	e = readdir(dir->dir);
	if (!e)
		return 0;

	strcpy(entry->cFileName, e->d_name);
	// there's no 8.3 file names support on linux :)
	strcpy(entry->cAlternateFileName, "");
	entry->flags = 0;

	sprintf(buffer, "%s/%s", dir->path, e->d_name);

	stat(buffer, &s);
	if (S_ISDIR(s.st_mode)) {
		entry->flags = FS_IS_DIR;
                entry->fileSize = 0;
	} else {
          entry->fileSize = s.st_size;
	}

	return 1;
}

void FsClose(void * search) {
	FsLinuxDir *linuxdir = (FsLinuxDir *) search; 
	DIR * dir = linuxdir->dir;

	closedir(dir);
	free(linuxdir->path);
	free(search);
}

int FsError(void) {
	return 0;
}
