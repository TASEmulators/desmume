/*  Copyright (C) 2006 yopyop
	Copyright (C) 2006 Mic
    Copyright (C) 2009 CrazyMax 
	Copyright (C) 2009 DeSmuME team

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
    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301  USA
*/

#include "../addons.h"
#include <string>
#include <string.h>
#include "debug.h"
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>

#include <fcntl.h>
#include <sys/stat.h>
#ifdef WIN32
#include <io.h>
#define OPEN_MODE _O_RDWR | _O_BINARY

#define OPEN_FN _open
#define CLOSE_FN _close
#define LSEEK_FN _lseek
#define WRITE_FN _write
#define READ_FN _read
#else
#include <unistd.h>
#define OPEN_MODE O_RDWR

#define OPEN_FN open
#define CLOSE_FN close
#define LSEEK_FN lseek
#define WRITE_FN write
#define READ_FN read
#endif

#include "types.h"
#include "../fat.h"
#include "../fs.h"
#include "MMU.h"
#include "NDSSystem.h"
#include "../path.h"

typedef struct {
	int level,parent,filesInDir;
} FILE_INFO;

// Set up addresses for GBAMP
#define CF_REG_DATA 0x9000000
#define CF_REG_ERR 0x9020000
#define CF_REG_SEC 0x9040000
#define CF_REG_LBA1 0x9060000
#define CF_REG_LBA2 0x9080000
#define CF_REG_LBA3 0x90A0000
#define CF_REG_LBA4 0x90C0000
#define CF_REG_CMD 0x90E0000
#define CF_REG_STS 0x98C0000

// CF Card commands
#define CF_CMD_LBA 0xE0
#define CF_CMD_READ 0x20
#define CF_CMD_WRITE 0x30

static int disk_image = -1;
static off_t file_size;

static u16	cf_reg_sts, 
			cf_reg_lba1,
			cf_reg_lba2,
			cf_reg_lba3,
			cf_reg_lba4,
			cf_reg_cmd;
static off_t currLBA;

static const int lfnPos[13] = {1,3,5,7,9,14,16,18,20,22,24,28,30};

#define SECPERFAT	128
#define SECPERCLUS	16
#define MAXFILES	32768
#define SECRESV		2
#define NUMSECTORS	0x80000
#define NUMCLUSTERS	(NUMSECTORS/SECPERCLUS)
#define BYTESPERCLUS (512*SECPERCLUS)
#define DIRENTSPERCLUS ((BYTESPERCLUS)/32)

BOOT_RECORD MBR;
static DIR_ENT *files,*dirEntries,**dirEntryPtr;
static FILE_INFO *fileLink,*dirEntryLink;
static u32 filesysFAT,filesysRootDir,filesysData;
static u16 FAT16[SECPERFAT*256];
static u16 numExtraEntries[SECPERFAT*256];
static DIR_ENT *extraDirEntries[SECPERFAT*256];
static int numFiles,maxLevel,numRootFiles;
static int *dirEntriesInCluster, clusterNum, firstDirEntCluster,
  lastDirEntCluster, lastFileDataCluster;
static int activeDirEnt=-1;
static u32 bufferStart;
static u32 fileStartLBA,fileEndLBA;
static u16 freadBuffer[256];
static FILE * hFile;
static char fpath[255+1];
static BOOL cflashDeviceEnabled = FALSE;

static std::string sFlashPath;

// ===========================
BOOL	inited;

static int lfn_checksum( void)
{
	int i;
	u8 chk = 0;

	for (i=0; i < (NAME_LEN + EXT_LEN); i++) 
	{
		chk = ((chk & 1) ? 0x80 : 0) + (chk >> 1) + 
			(i < NAME_LEN ? files[numFiles].name[i] : files[numFiles].ext[i - NAME_LEN]);
	}
	return chk;
}

// Add a DIR_ENT for the files
static void add_file(char *fname, FsEntry * entry, int fileLevel)
{
	int i,j,k,n;
	u8 chk;

	if (numFiles < MAXFILES-1)
	{
		if (strcmp(fname,"..") != 0)
		{
			for (i=strlen(fname)-1; i>=0; i--)
				if (fname[i]=='.') break;
			if ((i==0)&&(strcmp(fname,".")==0)) i = 1;
			if (i<0) i = strlen(fname);
			for (j=0; j<i; j++)
				files[numFiles].name[j] = fname[j];
			for (; j<NAME_LEN; j++)
				files[numFiles].name[j] = 0x20;
			for (j=0; j<EXT_LEN; j++) {
				if ((size_t)(j+i+1)>=strlen(fname)) break;
			files[numFiles].ext[j] = fname[j+i+1];
		}
		for (; j<3; j++)
			files[numFiles].ext[j] = 0x20;

		fileLink[fileLevel].filesInDir += 1;

		// See if LFN entries need to be added
		if (strlen(entry->cAlternateFileName)>0) 
		{
			char *p = NULL;

			chk = lfn_checksum();
			k = (strlen(entry->cFileName)/13) + (((strlen(entry->cFileName)%13)!=0)?1:0);
			numFiles += k;

			fileLink[fileLevel].filesInDir += k;

			n = 0;
			j = 13;
			for (i=0; (size_t)i<strlen(entry->cFileName); i++)
			{
				if (j == 13)
				{
					n++;
					p = (char*)&files[numFiles-n].name[0];
					fileLink[numFiles-n].parent = fileLevel;	
					p[0] = n;
					((DIR_ENT*)p)->attrib = ATTRIB_LFN;
					p[0xD] = chk;
					j = 0;
				}
				*(p + lfnPos[j]) = entry->cFileName[i];
				*(p + lfnPos[j]+1) = 0;
				j++;
			}
			for (; j<13; j++) 
			{
				*(p + lfnPos[j]) = entry->cFileName[i];
				*(p + lfnPos[j]+1) = 0;
			}
			if (p != NULL)
				p[0] |= 0x40;	// END
			for (i=strlen(fname)-1; i>=0; i--)
				if (fname[i]=='.') break;
			if ((i==0)&&(strcmp(fname,".")==0)) i = 1;
			if (i<0) i = strlen(fname);
			for (j=0; j<i; j++)
				files[numFiles].name[j] = fname[j];
			for (; j<NAME_LEN; j++)
				files[numFiles].name[j] = 0x20;
			for (j=0; j<EXT_LEN; j++) 
			{
				if ((size_t)(j+i+1)>=strlen(fname)) break;
				files[numFiles].ext[j] = fname[j+i+1];
			}
			for (; j<3; j++)
			files[numFiles].ext[j] = 0x20;
		}

		files[numFiles].fileSize = entry->fileSize;

		if (entry->flags & FS_IS_DIR)
		{
			if (strcmp(fname,".")==0)
				fileLink[numFiles].level = maxLevel;
			else
			fileLink[numFiles].level = maxLevel+1;
			files[numFiles].attrib = ATTRIB_DIR;
		}
		else
			files[numFiles].attrib = 0;

		fileLink[numFiles].parent = fileLevel;	

		numFiles++;
	}
	else
		if (fileLevel > 0) 
		{
			fileLink[fileLevel].filesInDir += 1;
			strncpy((char*)&files[numFiles].name[0],"..      ",NAME_LEN);
			strncpy((char*)&files[numFiles].ext[0],"   ",EXT_LEN);
			fileLink[numFiles].parent = fileLevel;	
			files[numFiles].attrib = 0x10;
			numFiles++;
		}
	}
}

// List all files and subdirectories recursively
static void list_files(const char *filepath)
{
	char DirSpec[255+1], SubDir[255+1];
	FsEntry entry;
	void * hFind;
	char *fname;
	u32 dwError;
	int fileLevel;

	maxLevel++;
	fileLevel = maxLevel;

	strncpy(DirSpec, filepath, ARRAY_SIZE(DirSpec));
	DirSpec[255] = 0 ; // hard limit the string here

	hFind = FsReadFirst(DirSpec, &entry);
	if (hFind == NULL) return;

	fname = (strlen(entry.cAlternateFileName)>0) ? entry.cAlternateFileName : entry.cFileName;
	add_file(fname, &entry, fileLevel);

	while (FsReadNext(hFind, &entry) != 0)
	{
		fname = (strlen(entry.cAlternateFileName)>0) ? entry.cAlternateFileName : entry.cFileName;
		add_file(fname, &entry, fileLevel);
		CFLASHLOG("cflash added %s\n",fname);

		if (numFiles==MAXFILES-1) break;

		if ((entry.flags & FS_IS_DIR) && (strcmp(fname, ".")) && (strcmp(fname, ".."))) 
		{
			if (strlen(fname)+strlen(filepath)+2 < 256) 
			{
				sprintf(SubDir, "%s%c%s", filepath, FS_SEPARATOR, fname);
				list_files(SubDir);
			}
		}
	}

	dwError = FsError();
	FsClose(hFind);
	if (dwError != FS_ERR_NO_MORE_FILES) return;

	if (numFiles < MAXFILES)
	{
		fileLink[numFiles].parent = fileLevel;
		files[numFiles++].name[0] = 0;
	}
}

// Set up the MBR, FAT and DIR_ENTs
static BOOL cflash_build_fat()
{
	int i,j,k,l,
	clust,numClusters,
	clusterNum2,rootCluster;
	int fileLevel;

	numFiles  = 0;
	fileLevel = -1;
	maxLevel  = -1;

	files = (DIR_ENT *) malloc(MAXFILES*sizeof(DIR_ENT));
	if (files == NULL) return FALSE;
	fileLink = (FILE_INFO *) malloc(MAXFILES*sizeof(FILE_INFO));
	if (fileLink == NULL)
	{
		free(files);
		return FALSE;
	}

	for (i=0; i<MAXFILES; i++)
	{
		files[i].attrib = 0;
		files[i].name[0] = FILE_FREE;
		files[i].fileSize = 0;

		fileLink[i].filesInDir = 0;

		extraDirEntries[i] = NULL;
		numExtraEntries[i] = 0;
	}

	list_files(sFlashPath.c_str());

	k            = 0;
	clusterNum   = rootCluster = (SECRESV + SECPERFAT)/SECPERCLUS;
	numClusters  = 0;
	clust        = 0;
	numRootFiles = 0;

	// Allocate memory to hold information about the files 
	dirEntries = (DIR_ENT *) malloc(numFiles*sizeof(DIR_ENT));
	if (dirEntries==NULL) return FALSE;

	dirEntryLink = (FILE_INFO *) malloc(numFiles*sizeof(FILE_INFO));
	if (dirEntryLink==NULL)
	{
		free(dirEntries);
		return FALSE;
	}
	dirEntriesInCluster = (int *) malloc(NUMCLUSTERS*sizeof(int));
	if (dirEntriesInCluster==NULL)
	{
		free(dirEntries);
		free(dirEntryLink);
		return FALSE;
	}
	dirEntryPtr = (DIR_ENT **) malloc(NUMCLUSTERS*sizeof(DIR_ENT*));
	if (dirEntryPtr==NULL)
	{
		free(dirEntries);
		free(dirEntryLink);
		free(dirEntriesInCluster);
		return FALSE;
	}

	memset(dirEntriesInCluster, 0, NUMCLUSTERS*sizeof(int));
	memset(dirEntryPtr, NULL, NUMCLUSTERS*sizeof(DIR_ENT*));

	// Change the hierarchical layout to a flat one 
	for (i=0; i<=maxLevel; i++)
	{
		clusterNum2 = clusterNum;
		for (j=0; j<numFiles; j++)
		{
			if (fileLink[j].parent == i)
			{
				if (dirEntryPtr[clusterNum] == NULL)
					dirEntryPtr[clusterNum] = &dirEntries[k];
				dirEntryLink[k] = fileLink[j];
				dirEntries[k++] = files[j];
				if ((files[j].attrib & ATTRIB_LFN)==0)
				{
					if (files[j].attrib & ATTRIB_DIR)
					{
						if (strncmp((char*)&files[j].name[0],".       ",NAME_LEN)==0)
							dirEntries[k-1].startCluster = dirEntryLink[k-1].level; 
						else
							if (strncmp((char*)&files[j].name[0],"..      ",NAME_LEN)==0)
							{
								dirEntries[k-1].startCluster = dirEntryLink[k-1].parent; 
							}
							else
							{
								clust++;
								dirEntries[k-1].startCluster = clust;
								l = (fileLink[fileLink[j].level].filesInDir)>>8;
								clust += l;
								numClusters += l;
							}
					}
					else
						dirEntries[k-1].startCluster = clusterNum;
				}
				if (i==0) numRootFiles++;
				dirEntriesInCluster[clusterNum]++;
				if (dirEntriesInCluster[clusterNum]==256)
					clusterNum++;
			}
		}
		clusterNum = clusterNum2 + ((fileLink[i].filesInDir)>>8) + 1;
		numClusters++;
	}

	// Free the file indexing buffer
	free(files);
	free(fileLink);

	// Set up the MBR
	MBR.bytesPerSector = 512;
	MBR.numFATs = 1;
	// replaced strcpy with strncpy. It doesnt matter here, as the strings are constant
	// but we should extingish all unrestricted strcpy,strcat from the project
	strncpy((char*)&MBR.OEMName[0],"DESMUM",8);	
	strncpy((char*)&MBR.fat16.fileSysType[0],"FAT16  ",8);
	MBR.reservedSectors = SECRESV;
	MBR.numSectors = 524288;
	MBR.numSectorsSmall = 0;
	MBR.sectorsPerCluster = SECPERCLUS;
	MBR.sectorsPerFAT = SECPERFAT;
	MBR.rootEntries = 512;
	MBR.fat16.signature = 0xAA55;
	MBR.mediaDesc = 1;

	filesysFAT = 0 + MBR.reservedSectors;
	filesysRootDir = filesysFAT + (MBR.numFATs * MBR.sectorsPerFAT);
	filesysData = filesysRootDir + ((MBR.rootEntries * sizeof(DIR_ENT)) / 512);

	// Set up the cluster values for all subdirectories
	clust = filesysData / SECPERCLUS;
	firstDirEntCluster = clust;
	for (i=1; i<numFiles; i++)
	{
		if (((dirEntries[i].attrib & ATTRIB_DIR)!=0) &&	((dirEntries[i].attrib & ATTRIB_LFN)==0))
		{
			if (dirEntries[i].startCluster > rootCluster)
				dirEntries[i].startCluster += clust-rootCluster;
		}
	}
	lastDirEntCluster = clust+numClusters-1;

	// Set up the cluster values for all files
	clust += numClusters; //clusterNum;
	for (i=0; i<numFiles; i++)
	{
		if (((dirEntries[i].attrib & ATTRIB_DIR)==0) &&	((dirEntries[i].attrib & ATTRIB_LFN)==0))
		{
			dirEntries[i].startCluster = clust;
			clust += (dirEntries[i].fileSize/(512*SECPERCLUS));
			clust++;
		}
	}
	lastFileDataCluster = clust-1;

	// Set up the FAT16
	memset(FAT16,0,SECPERFAT*256*sizeof(u16));
	FAT16[0] = 0xFF01;
	FAT16[1] = 0xFFFF;
	for (i=2; i<=lastDirEntCluster; i++)
	FAT16[i] = 0xFFFF;
	k = 2;
	for (i=0; i<numFiles; i++)
	{
		if (((dirEntries[i].attrib & ATTRIB_LFN)==0) &&	(dirEntries[i].name[0] != FILE_FREE))
		{
			j = 0;
			l = dirEntries[i].fileSize - (512*SECPERCLUS);
			while (l > 0)
			{
				if (dirEntries[i].startCluster+j < MAXFILES)
					FAT16[dirEntries[i].startCluster+j] = dirEntries[i].startCluster+j+1;
				j++;
				l -= (512*16);
			} 
			if ((dirEntries[i].attrib & ATTRIB_DIR)==0)
			{
				if (dirEntries[i].startCluster+j < MAXFILES)
				FAT16[dirEntries[i].startCluster+j] = 0xFFFF;
			}
			k = dirEntries[i].startCluster+j;
		}
	}

	for (i=(filesysData/SECPERCLUS); i<NUMCLUSTERS; i++)
	{
		if (dirEntriesInCluster[i]==256)
			FAT16[i] = i+1;
	}

	return TRUE;
}

static BOOL cflash_init() 
{
	if (inited) return FALSE;
	BOOL init_good = FALSE;

	CFLASHLOG("CFlash_Mode: %d\n",CFlash_Mode);

	if (CFlash_Mode == ADDON_CFLASH_MODE_RomPath)
	{
		sFlashPath = path.pathToRoms;
		INFO("Using CFlash directory of rom: %s\n", sFlashPath.c_str());
	}
	else if(CFlash_Mode == ADDON_CFLASH_MODE_Path)
	{
		sFlashPath = CFlash_Path;
		INFO("Using CFlash directory: %s\n", sFlashPath.c_str());
	}

	if(CFlash_IsUsingPath())
	{
		cflashDeviceEnabled = FALSE;
		currLBA = 0;

		if (activeDirEnt != -1)
			fclose(hFile);
		activeDirEnt = -1;
		fileStartLBA = fileEndLBA = 0xFFFFFFFF;
		if (!cflash_build_fat()) {
			CFLASHLOG("FAILED cflash_build_fat\n");
			return FALSE;
		}
		cf_reg_sts = 0x58;	// READY

		cflashDeviceEnabled = TRUE;
		init_good = TRUE;
	}
	else
	{
		sFlashPath = CFlash_Path;
		INFO("Using CFlash disk image file %s\n", sFlashPath.c_str());
		disk_image = OPEN_FN( sFlashPath.c_str(), OPEN_MODE);

		if ( disk_image != -1)
		{
			file_size = LSEEK_FN( disk_image, 0, SEEK_END);
			if (0 && file_size == -1)
			{
				CFLASHLOG( "Error when seeking to end of disk image" );
			}
			else
			{
				LSEEK_FN( disk_image, 0, SEEK_SET);
				CFLASHLOG( "Disk image size = %ld (%ld sectors)\n", file_size, file_size / 512);
				init_good = TRUE;
			}
		}
		else
			// TODO: create image if not exist
			CFLASHLOG("Failed to open file %s: \"%s\"\n", sFlashPath.c_str(), strerror( errno));
	}

	// READY
	cf_reg_sts = 0x58;

	currLBA = 0;
	cf_reg_lba1 = cf_reg_lba2 =
	cf_reg_lba3 = cf_reg_lba4 = 0;

	inited = TRUE;
	return init_good;
}

// Convert a space-padded 8.3 filename into an asciiz string
static void fatstring_to_asciiz(int dirent,char *out,DIR_ENT *d)
{
	int i,j;
	DIR_ENT *pd;

	if (d == NULL)
		pd = &dirEntries[dirent];
	else
		pd = d;

	for (i=0; i<NAME_LEN; i++)
	{
		if (pd->name[i] == ' ') break;
		out[i] = pd->name[i];
	}
	if ((pd->attrib & 0x10)==0)
	{
		out[i++] = '.';
		for (j=0; j<EXT_LEN; j++)
		{
			if (pd->ext[j] == ' ') break;
			out[i++] = pd->ext[j];
		}
	}
	out[i] = '\0';
}

// Resolve the path of a files by working backwards through the directory entries
static void resolve_path(int dirent)
{
	int i;
	char dirname[128];

	while (dirEntryLink[dirent].parent > 0)
	{
		for (i=0; i<dirent; i++)
		{
			if ((dirEntryLink[dirent].parent==dirEntryLink[i].level) &&	((dirEntries[i].attrib&ATTRIB_DIR)!=0))
			{
				fatstring_to_asciiz(i,dirname,NULL);
				strncat(fpath,dirname,ARRAY_SIZE(fpath)-strlen(fpath));
				strncat(fpath,DIR_SEP,ARRAY_SIZE(fpath)-strlen(fpath));
				dirent = i;
				break;
			}
		}
	}
}

// Read from a file using a 512 byte buffer
static u16 fread_buffered(int dirent,u32 cluster,u32 offset)
{
	char fname[2*NAME_LEN+EXT_LEN];
	size_t elems_read = 0;

	offset += cluster*512*SECPERCLUS;

	if (dirent == activeDirEnt)
	{
		if ((offset < bufferStart) || (offset >= bufferStart + 512)) 
		{
			if (!hFile) 
			{
				CFLASHLOG("fread_buffered with hFile null with offset %lu and bufferStart %lu\n",
					offset, bufferStart);
				return 0;
			}
			fseek(hFile, offset, SEEK_SET);
			elems_read += fread(&freadBuffer, 1, 512, hFile);
			bufferStart = offset;
		}

		return freadBuffer[(offset-bufferStart)>>1];
	}
	if (activeDirEnt != -1)
		fclose(hFile);

	strncpy(fpath,sFlashPath.c_str(),ARRAY_SIZE(fpath));
	strncat(fpath,DIR_SEP,ARRAY_SIZE(fpath)-strlen(fpath));

	resolve_path(dirent);

	fatstring_to_asciiz(dirent,fname,NULL);
	strncat(fpath,fname,ARRAY_SIZE(fpath)-strlen(fpath));

	CFLASHLOG("CFLASH Opening %s\n",fpath);
	hFile = fopen(fpath, "rb");
	if (!hFile) return 0;
	bufferStart = offset;
	fseek(hFile, offset, SEEK_SET);
	elems_read += fread(&freadBuffer, 1, 512, hFile);

	bufferStart = offset;
	activeDirEnt = dirent;
	fileStartLBA = (dirEntries[dirent].startCluster*512*SECPERCLUS);
	fileEndLBA = fileStartLBA + dirEntries[dirent].fileSize;

	return freadBuffer[(offset-bufferStart)>>1];
}

static unsigned int cflash_read(unsigned int address)
{
	unsigned int ret_value = 0;
	size_t elems_read = 0;
#if 0 /* used by next if 0 block */
#define BUFFERED_BLOCK_SIZE 512
	static u8 block_buffer[BUFFERED_BLOCK_SIZE];
	static s32 buffered_start_index = -1;
#endif

	switch (address)
	{
		case CF_REG_STS:
			ret_value = cf_reg_sts;
		break;

		case CF_REG_DATA:
			if (cf_reg_cmd == CF_CMD_READ)
			{
				if (!CFlash_IsUsingPath())
				{
					if ( disk_image != -1)
					{
						u8 data[2];
#if 0
						if (currLBA < buffered_start_index || currLBA >= (buffered_start_index + BUFFERED_BLOCK_SIZE))
						{
							size_t read_bytes = 0;
							LSEEK_FN( disk_image, currLBA, SEEK_SET);

							while (read_bytes < BUFFERED_BLOCK_SIZE)
							{
								size_t cur_read = READ_FN( disk_image, &block_buffer[read_bytes],
									BUFFERED_BLOCK_SIZE - read_bytes);

								if ( cur_read == -1)
								{
									CFLASHLOG( "Error during read: %s\n", strerror(errno) );
									break;
								}
								read_bytes += cur_read;
							}

							CFLASHLOG( "Read %d bytes\n", read_bytes);

							buffered_start_index = currLBA;
						}
						data[0] = block_buffer[currLBA - buffered_start_index];
						data[1] = block_buffer[currLBA + 1 - buffered_start_index];
#else
						LSEEK_FN( disk_image, currLBA, SEEK_SET);
						elems_read += READ_FN( disk_image, data, 2);
#endif
						ret_value = data[1] << 8 | data[0];
					}
					currLBA += 2;
				}
				else		// use path
				{
					unsigned char *p;
					int i;
					u32 cluster,cluster2,cluster3,fileLBA;
					cluster = (currLBA / (512 * SECPERCLUS));
					cluster2 = (((currLBA/512) - filesysData) / SECPERCLUS) + 2;

					// Reading from the MBR 
					if (currLBA < 512)
					{
						p = (unsigned char*)&MBR;
						ret_value = T1ReadWord(p, currLBA);

						// Reading the FAT 
					}
					else
						if (((u32)currLBA >= filesysFAT*512) && ((u32)currLBA < filesysRootDir*512))
						{
							p = (unsigned char*)&FAT16[0];
							ret_value = T1ReadWord(p, currLBA - filesysFAT * 512);

							// Reading directory entries 
						}
						else
							if (((u32)currLBA >= filesysRootDir*512) &&	(cluster <= (u32)lastDirEntCluster))
							{
								cluster3 = ((currLBA - (SECRESV * 512)) / (512 * SECPERCLUS));
								i = (currLBA-(((cluster3-(filesysRootDir/SECPERCLUS))*SECPERCLUS+filesysRootDir)*512)); //(currLBA - cluster*BYTESPERCLUS);
								if (i < (dirEntriesInCluster[cluster3]*32))
								{
									p = (unsigned char*)dirEntryPtr[cluster3];
									ret_value = T1ReadWord(p, i);
								}
								else
								{
									i /= 32;
									i -= dirEntriesInCluster[cluster3];
									if ((i>=0)&&(i<numExtraEntries[cluster3])) 
									{
										p = (unsigned char*)extraDirEntries[cluster3];
										ret_value = T1ReadWord(p, i * 32 + (currLBA & 0x1F));
									}
									else 
										if ((currLBA&0x1F)==0)
											ret_value = FILE_FREE;
										else
											ret_value = 0;
								}
								// Reading file data 
							}
							else
								if ((cluster2 > (u32)lastDirEntCluster) && (cluster2 <= (u32)lastFileDataCluster)) 
								{
									//else if ((cluster>lastDirEntCluster)&&(cluster<=lastFileDataCluster)) {
									fileLBA = currLBA - (filesysData-32)*512;	// 32 = # sectors used for the root entries

									// Check if the read is from the currently opened file 
									if ((fileLBA >= fileStartLBA) && (fileLBA < fileEndLBA))
									{
										cluster = (fileLBA / (512 * SECPERCLUS));
										ret_value = fread_buffered(activeDirEnt,cluster-dirEntries[activeDirEnt].startCluster,(fileLBA-fileStartLBA)&(BYTESPERCLUS-1)); 
									}
									else
									{
										for (i=0; i<numFiles; i++)
										{
											if ((fileLBA>=(u32)(dirEntries[i].startCluster*512*SECPERCLUS)) &&
												(fileLBA <(dirEntries[i].startCluster*512*SECPERCLUS)+dirEntries[i].fileSize) &&
												((dirEntries[i].attrib & (ATTRIB_DIR|ATTRIB_LFN))==0))
											{
												cluster = (fileLBA / (512 * SECPERCLUS));
												ret_value = fread_buffered(i,cluster-dirEntries[i].startCluster,fileLBA&(BYTESPERCLUS-1));
												break;
											}
										}
									}
								}
							currLBA += 2;
				}
			}
	break;

	case CF_REG_CMD:
	break;

	case CF_REG_LBA1:
		ret_value = cf_reg_lba1;
	break;
	}

	return ret_value;
}

static void cflash_write(unsigned int address,unsigned int data)
{
	static u8 sector_data[512];
	static u32 sector_write_index = 0;

	switch (address)
	{
		case CF_REG_STS:
			cf_reg_sts = data&0xFFFF;
		break;

		case CF_REG_DATA:
			if (cf_reg_cmd == CF_CMD_WRITE)
			{
				if (!CFlash_IsUsingPath()) 
				{
					sector_data[sector_write_index] = (data >> 0) & 0xff;
					sector_data[sector_write_index + 1] = (data >> 8) & 0xff;

					sector_write_index += 2;

					if (sector_write_index == 512) 
					{
						CFLASHLOG( "Write sector to %ld\n", currLBA);
						size_t written = 0;

						if (currLBA + 512 < file_size) 
						{
							if (disk_image != -1) 
							{
								LSEEK_FN( disk_image, currLBA, SEEK_SET);
				      
								while(written < 512) 
								{
									size_t cur_write =
										WRITE_FN( disk_image, &sector_data[written], 512 - written);
									written += cur_write;

									if ( cur_write == (size_t)-1) break;
								}
							}
						}

						CFLASHLOG("Wrote %u bytes\n", written);
					
						currLBA += 512;
						sector_write_index = 0;
					}
				}
				else		// TODO: write to real partition
				{
				}
			}
		break;

		case CF_REG_CMD:
			cf_reg_cmd = data&0xFF;
			cf_reg_sts = 0x58;	// READY
		break;

		case CF_REG_LBA1:
			cf_reg_lba1 = data&0xFF;
			currLBA = (currLBA&0xFFFFFF00)| cf_reg_lba1;
		break;

		case CF_REG_LBA2:
			cf_reg_lba2 = data&0xFF;
			currLBA = (currLBA&0xFFFF00FF)|(cf_reg_lba2<<8);
		break;

		case CF_REG_LBA3:
			cf_reg_lba3 = data&0xFF;
			currLBA = (currLBA&0xFF00FFFF)|(cf_reg_lba3<<16);
		break;

		case CF_REG_LBA4:
			cf_reg_lba4 = data&0xFF;

			if ((cf_reg_lba4 & 0xf0) == CF_CMD_LBA)
			{
				currLBA = (currLBA&0x00FFFFFF)|((cf_reg_lba4&0x0F)<<24);
				currLBA *= 512;
				sector_write_index = 0;
			}
		break;
	}  
}

static void cflash_close( void) 
{
	if (!inited) return;
	if (!CFlash_IsUsingPath())
	{
		if (disk_image != -1)
		{
			CLOSE_FN(disk_image);
			disk_image = -1;
		}
	}
	else
	{
		int i;

		if (cflashDeviceEnabled) 
		{
			cflashDeviceEnabled = FALSE;

			for (i=0; i<MAXFILES; i++) 
			{
				if (extraDirEntries[i] != NULL)
					free(extraDirEntries[i]);
			}

			if (dirEntries != NULL) 
				free(dirEntries);
			if (dirEntryLink != NULL) 
				free(dirEntryLink);
			if (dirEntriesInCluster != NULL) 
				free(dirEntriesInCluster);
			if (dirEntryPtr != NULL) 
				free(dirEntryPtr);
			if (activeDirEnt != -1) 
				fclose(hFile);
		}
	}
	inited = FALSE;
}

static BOOL CFlash_init(void)
{
	return TRUE;
}

static void CFlash_reset(void)
{
	cflash_close();
	cflash_init();
}

static void CFlash_close(void)
{
	cflash_close();
}

static void CFlash_config(void)
{
}

static void CFlash_write08(u32 adr, u8 val)
{
	cflash_write(adr, val);
}

static void CFlash_write16(u32 adr, u16 val)
{
	cflash_write(adr, val);
}

static void CFlash_write32(u32 adr, u32 val)
{
	cflash_write(adr, val);
}

static u8 CFlash_read08(u32 adr)
{
	return (cflash_read(adr));
}

static u16 CFlash_read16(u32 adr)
{
	return (cflash_read(adr));
}

static u32 CFlash_read32(u32 adr)
{
	return (cflash_read(adr));
}

static void CFlash_info(char *info)
{
	strcpy(info, "Compact Flash memory in slot");
}

ADDONINTERFACE addonCFlash = {
				"Compact Flash",
				CFlash_init,
				CFlash_reset,
				CFlash_close,
				CFlash_config,
				CFlash_write08,
				CFlash_write16,
				CFlash_write32,
				CFlash_read08,
				CFlash_read16,
				CFlash_read32,
				CFlash_info};

#undef CFLASHDEBUG
