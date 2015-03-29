/*
	Copyright (C) 2006 Guillaume Duhamel
	Copyright (C) 2006-2009 DeSmuME team

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

#include "fs.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>

const char FS_SEPARATOR = '\\';

void * FsReadFirst(const char * p, FsEntry * entry) {
	WIN32_FIND_DATA FindFileData;
	HANDLE hFind;
	HANDLE * ret;
	char path[1024];
	if (strlen(p)+3 >sizeof(path)) return NULL ;

	sprintf(path, "%s\\*", p);

	hFind = FindFirstFile(path, &FindFileData);
	if (hFind == INVALID_HANDLE_VALUE)
		return NULL;

	strncpy(entry->cFileName, FindFileData.cFileName,256);
	entry->cFileName[255] = 0 ;
	strncpy(entry->cAlternateFileName, FindFileData.cAlternateFileName,14);
	entry->cAlternateFileName[13] = 0 ;
	entry->flags = 0;
	if (FindFileData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
		entry->flags = FS_IS_DIR;
                entry->fileSize = 0;
	} else {
          entry->fileSize = FindFileData.nFileSizeLow;
        }

	ret = (void**)malloc(sizeof(HANDLE));
	*ret = hFind;
	return ret;
}

int FsReadNext(void * search, FsEntry * entry) {
	WIN32_FIND_DATA FindFileData;
	HANDLE * h = (HANDLE *) search;
	int ret;

	ret = FindNextFile(*h, &FindFileData);

	strncpy(entry->cFileName, FindFileData.cFileName,256);
	entry->cFileName[255] = 0 ;
	strncpy(entry->cAlternateFileName, FindFileData.cAlternateFileName,14);
	entry->cAlternateFileName[13] = 0 ;
	entry->flags = 0;
	if (FindFileData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
		entry->flags = FS_IS_DIR;
                entry->fileSize = 0;
	} else {
          entry->fileSize = FindFileData.nFileSizeLow;
        }

	return ret;
}

void FsClose(void * search) {
	FindClose(*((HANDLE *) search));
}

int FsError(void) {
	if (GetLastError() == ERROR_NO_MORE_FILES)
		return FS_ERR_NO_MORE_FILES;

	return FS_ERR_UNKNOWN;
}
