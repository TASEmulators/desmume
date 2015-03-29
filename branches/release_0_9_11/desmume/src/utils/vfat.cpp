/*
	Copyright (C) 2006 yopyop
	Copyright (C) 2006 Mic
	Copyright (C) 2010-2015 DeSmuME team

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

#include <string>
#include <string.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <stack>

#include "../types.h"
#include "../debug.h"
#include "../emufile.h"
#include "../fs.h"

#include "emufat.h"
#include "vfat.h"
#include "libfat/libfat_public_api.h"


enum EListCallbackArg {
	EListCallbackArg_Item, EListCallbackArg_Pop
};

typedef void (*ListCallback)(FsEntry* fs, EListCallbackArg);

// List all files and subdirectories recursively
static void list_files(const char *filepath, ListCallback list_callback)
{
	char DirSpec[255+1], SubDir[255+1];
	FsEntry entry;
	void * hFind;
	char *fname;
	u32 dwError;

	strncpy(DirSpec, filepath, ARRAY_SIZE(DirSpec));
	DirSpec[255] = 0 ; // hard limit the string here

	hFind = FsReadFirst(DirSpec, &entry);
	if (hFind == NULL) return;

	do {
		fname = (strlen(entry.cAlternateFileName)>0) ? entry.cAlternateFileName : entry.cFileName;
		list_callback(&entry,EListCallbackArg_Item);
		printf("cflash added %s\n",entry.cFileName);

		if ((entry.flags & FS_IS_DIR) && (strcmp(fname, ".")) && (strcmp(fname, ".."))) 
		{
			if (strlen(fname)+strlen(filepath)+2 < 256) 
			{
				sprintf(SubDir, "%s%c%s", filepath, FS_SEPARATOR, fname);
				list_files(SubDir, list_callback);
				list_callback(&entry, EListCallbackArg_Pop);
			}
		}
	} while (FsReadNext(hFind, &entry) != 0);

	dwError = FsError();
	FsClose(hFind);
	if (dwError != FS_ERR_NO_MORE_FILES) return;
}

static u64 dataSectors = 0;
void count_ListCallback(FsEntry* fs, EListCallbackArg arg)
{
	if(arg == EListCallbackArg_Pop) return;
	u32 sectors = 1;
	if(fs->flags & FS_IS_DIR)
	{
	}
	else
		sectors += (fs->fileSize+511)/512 + 1;
	dataSectors += sectors; 
}

static std::string currPath;
static std::stack<std::string> pathStack;
static std::stack<std::string> virtPathStack;
static std::string currVirtPath;
void build_ListCallback(FsEntry* fs, EListCallbackArg arg)
{
	char* fname = (strlen(fs->cAlternateFileName)>0) ? fs->cAlternateFileName : fs->cFileName;

	//we use cFileName always because it is a LFN and we are making sure that we always make a fat32 image
	fname = fs->cFileName;

	if(arg == EListCallbackArg_Pop) 
	{
		currPath = pathStack.top();
		pathStack.pop();
		currVirtPath = virtPathStack.top();
		virtPathStack.pop();
		return;
	}
	
	if(fs->flags & FS_IS_DIR)
	{
		if(!strcmp(fname,".")) return;
		if(!strcmp(fname,"..")) return;

		pathStack.push(currPath);
		virtPathStack.push(currVirtPath);

		currVirtPath = currVirtPath + "/" + fname;
		bool ok = LIBFAT::MkDir(currVirtPath.c_str());

		if(!ok)
			printf("ERROR adding dir %s via libfat\n",currVirtPath.c_str());

		currPath = currPath + std::string(1,FS_SEPARATOR) + fname;
		return;
	}
	else
	{
		std::string path = currPath + std::string(1,FS_SEPARATOR) + fname;

		FILE* inf = fopen(path.c_str(),"rb");
		if(inf)
		{
			fseek(inf,0,SEEK_END);
			long len = ftell(inf);
			fseek(inf,0,SEEK_SET);
			u8 *buf = new u8[len];
			fread(buf,1,len,inf);
			fclose(inf);

			std::string path = currVirtPath + "/" + fname;
			printf("adding path %s for libfat\n",path.c_str());
			bool ok = LIBFAT::WriteFile(path.c_str(),buf,len);
			if(!ok) 
				printf("ERROR adding file to fat\n");
			delete[] buf;
		} else printf("ERROR opening file for fat\n");
	}
		
}



bool VFAT::build(const char* path, int extra_MB)
{
	dataSectors = 0;
	currVirtPath = "";
	currPath = path;
	list_files(path, count_ListCallback);

	dataSectors += 8; //a few for reserved sectors, etc.

	dataSectors += extra_MB*1024*1024/512; //add extra write space
	//dataSectors += 16*1024*1024/512; //add 16MB worth of write space. this is probably enough for anyone, but maybe it should be configurable.
	//we could always suggest to users to add a big file to their directory to overwrite (that would cause the image to get padded)

	//this seems to be the minimum size that will turn into a solid fat32
	if(dataSectors<36*1024*1024/512)
		dataSectors = 36*1024*1024/512;

	if(dataSectors>=(0x80000000>>9))
	{
		printf("error allocating memory for fat (%d KBytes)\n",(dataSectors*512)/1024);
		printf("total fat sizes > 2GB are never going to work\n");
	}
	
	delete file;
	try 
	{
		file = new EMUFILE_MEMORY(dataSectors*512);
	}
	catch(std::bad_alloc)
	{
		printf("error allocating memory for fat (%d KBytes)\n",(dataSectors*512)/1024);
		printf("(out of memory)\n");
		return false;
	}

	//debug..
	//file = new EMUFILE_FILE("c:\\temp.ima","rb+");
	
	//format the disk
	{
		EmuFat fat(file);
		EmuFatVolume vol;
		u8 ok = vol.init(&fat);
		vol.formatNew(dataSectors);

		//ensure we are working in memory, just in case we were testing with a disk file.
		//libfat will need to go straight to memory (for now; we could easily change it to work with the disk)
		file = file->memwrap();
	}
	EMUFILE_MEMORY* memf = (EMUFILE_MEMORY*)file;

	//setup libfat and write all the files through it
	LIBFAT::Init(memf->buf(),memf->size());
	list_files(path, build_ListCallback);
	LIBFAT::Shutdown();

	return true;
}

VFAT::VFAT()
	: file(NULL)
{
}

VFAT::~VFAT()
{
	delete file;
}

EMUFILE* VFAT::detach()
{
	EMUFILE* ret = file;
	file = NULL;
	return ret;
}