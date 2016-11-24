/*
	Copyright (C) 2006 yopyop
	Copyright (C) 2006 Mic
	Copyright (C) 2010-2016 DeSmuME team

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
#include "retro_dirent.h"
#include "retro_stat.h"
#include "file/file_path.h"

#include "emufat.h"
#include "vfat.h"
#include "libfat/libfat_public_api.h"


enum EListCallbackArg {
	EListCallbackArg_Item, EListCallbackArg_Pop
};

typedef void (*ListCallback)(RDIR* rdir, EListCallbackArg);

// List all files and subdirectories recursively
//TODO: clunky architecture. we've combined the callbacks into one handler.
//we could merge the callback and list_files function, or refactor the callback into one for each enum which receives a unit of work after 
//the more detailed recursing logic (caused by libretro-common integration) is handled in the lister
static void list_files(const char *filepath, ListCallback list_callback)
{
	void * hFind;
	char *fname;
	u32 dwError;

	RDIR* rdir = retro_opendir(filepath);
	if(!rdir) return;
	if(retro_dirent_error(rdir))
	{
		retro_closedir(rdir);
		return;
	}

	for(;;)
	{
		if(!retro_readdir(rdir))
			break;

		const char* fname = retro_dirent_get_name(rdir);
		list_callback(rdir,EListCallbackArg_Item);
		printf("cflash added %s\n",fname);

		if (retro_dirent_is_dir(rdir, filepath) && (strcmp(fname, ".")) && strcmp(fname, ".."))
		{
			std::string subdir = (std::string)filepath + path_default_slash() + fname;
			list_files(subdir.c_str(), list_callback);
			list_callback(rdir, EListCallbackArg_Pop);
		}
	}

	retro_closedir(rdir);
}

enum eCallbackType
{
	eCallbackType_Count, eCallbackType_Build
};

static eCallbackType callbackType;

//for eCallbackType_Count:
static bool count_failed = false;
static u64 dataSectors = 0;

//recursing related.. really ought to be merged with list_files functionality
static std::string currPath;
static std::stack<std::string> pathStack;
static std::stack<std::string> virtPathStack;
static std::string currVirtPath;

static void DirectoryListCallback(RDIR* rdir, EListCallbackArg arg)
{
	const char* fname = retro_dirent_get_name(rdir);

	if(arg == EListCallbackArg_Pop) 
	{
		currPath = pathStack.top();
		pathStack.pop();
		currVirtPath = virtPathStack.top();
		virtPathStack.pop();
		return;
	}

	if(retro_dirent_is_dir(rdir,currVirtPath.c_str()))
	{
		if(!strcmp(fname,".")) return;
		if(!strcmp(fname,"..")) return;

		pathStack.push(currPath);
		virtPathStack.push(currVirtPath);

		currVirtPath = currVirtPath + "/" + fname;

		if(callbackType == eCallbackType_Build)
		{
			bool ok = LIBFAT::MkDir(currVirtPath.c_str());

			if(!ok)
				printf("ERROR adding dir %s via libfat\n",currVirtPath.c_str());
		}
		else
		{
			dataSectors++; //directories take one sector
		}

		currPath = currPath + path_default_slash() + fname;
		return;
	}
	else
	{
		std::string path = currPath + path_default_slash() + fname;

		if(callbackType == eCallbackType_Build)
		{
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
				printf("FAT + (%10.2f KB) %s \n",len/1024.f,path.c_str());
				bool ok = LIBFAT::WriteFile(path.c_str(),buf,len);
				if(!ok) 
					printf("ERROR adding file to fat\n");
				delete[] buf;
			} else printf("ERROR opening file for fat\n");
		}
		else
		{
			//allocate sectors for file
			int32_t fileSize = path_get_size(path.c_str());
			if(fileSize == -1) { count_failed = true; dataSectors = 0; }
			else dataSectors += (fileSize+511)/512 + 1;
		}
	}
		
}

bool VFAT::build(const char* path, int extra_MB)
{
	dataSectors = 0;
	currVirtPath = "";
	currPath = path;

	count_failed = false;
	callbackType = eCallbackType_Count;
	list_files(path, DirectoryListCallback);

	if(count_failed)
	{
		printf("FAILED enumerating files for fat\n");
		return false;
	}

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
	callbackType = eCallbackType_Build;
	list_files(path, DirectoryListCallback);
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