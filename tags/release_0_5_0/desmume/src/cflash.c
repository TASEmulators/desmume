/*
        CFLASH.C
	CompactFlash/FAT emulation routines for DeSmuME
	/Mic, 2006

	Logical memory layout:

	----------------------
	|        MBR         |
	----------------------
	|        FAT         |
	----------------------
	|    Root entries    |
	----------------------
	|   Subdirectories   |
	----------------------
	|     File data      |
	----------------------
	
*/

#include "fs.h"
#include "cflash.h"
#include "NDSSystem.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


#define SECPERFAT	128
#define SECPERCLUS	16
#define MAXFILES	32768
#define SECRESV		2
#define NUMSECTORS	0x80000
#define NUMCLUSTERS	(NUMSECTORS/SECPERCLUS)
#define BYTESPERCLUS (512*SECPERCLUS)
#define DIRENTSPERCLUS ((BYTESPERCLUS)/32)


u16                          cf_reg_data,cf_reg_err,cf_reg_sec,cf_reg_lba1,cf_reg_lba2,
							cf_reg_lba3,cf_reg_lba4,cf_reg_cmd,cf_reg_sts;
unsigned int				CF_REG_DATA,CF_REG_ERR,CF_REG_SEC,CF_REG_LBA1,CF_REG_LBA2,
							CF_REG_LBA3,CF_REG_LBA4,CF_REG_CMD,CF_REG_STS;

BOOT_RECORD					MBR;
DIR_ENT						*files,*dirEntries,**dirEntryPtr;
FILE_INFO					*fileLink,*dirEntryLink;
u32						currLBA;
u32						filesysFAT,filesysRootDir,filesysData;
u16							FAT16[SECPERFAT*256];
u16							numExtraEntries[SECPERFAT*256];
DIR_ENT						*extraDirEntries[SECPERFAT*256];
int							numFiles,maxLevel,dirNum,numRootFiles;
int							*dirEntriesInCluster,clusterNum,
							firstDirEntCluster,lastDirEntCluster,
							lastFileDataCluster;
char						*sRomPath;
int							activeDirEnt=-1;
u32						bufferStart;
u32						fileStartLBA,fileEndLBA;
u16                                             freadBuffer[256];
u32						dwBytesRead;
FILE *						hFile;
char						fpath[255+1];
BOOL                                            cflashDeviceEnabled = FALSE;
char                        buffer[256];
u32               dummy;


int lfn_checksum() {
	int i;
        u8 chk;

	chk = 0;
	for (i=0; i < 11; i++) {
		chk = ((chk & 1) ? 0x80 : 0) + (chk >> 1) + (i < 8 ? files[numFiles].name[i] : files[numFiles].ext[i - 8]);
	}
	return chk;
}


const int lfnPos[13] = {1,3,5,7,9,14,16,18,20,22,24,28,30};


/* Add a DIR_ENT for the files */
void add_file(char *fname, FsEntry * entry, int fileLevel) {
	int i,j,k,n;
        u8 chk;
	char *p;

	if (numFiles < MAXFILES-1) {
		if (strcmp(fname,"..") != 0) {
			for (i=strlen(fname)-1; i>=0; i--)
				if (fname[i]=='.') break;
			if ((i==0)&&(strcmp(fname,".")==0)) i = 1;
			if (i<0) i = strlen(fname);
			for (j=0; j<i; j++)
				files[numFiles].name[j] = fname[j];
			for (; j<8; j++)
				files[numFiles].name[j] = 0x20;
			for (j=0; j<3; j++) {
				if ((j+i+1)>=strlen(fname)) break;
				files[numFiles].ext[j] = fname[j+i+1];
			}
			for (; j<3; j++)
				files[numFiles].ext[j] = 0x20;

			fileLink[fileLevel].filesInDir += 1;

			// See if LFN entries need to be added
			if (strlen(entry->cAlternateFileName)>0) {
				chk = lfn_checksum();
				k = (strlen(entry->cFileName)/13) + (((strlen(entry->cFileName)%13)!=0)?1:0);
				numFiles += k;

				fileLink[fileLevel].filesInDir += k;

				n = 0;
				j = 13;
				for (i=0; i<strlen(entry->cFileName); i++) {
					if (j == 13) {
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
				for (; j<13; j++) {
					*(p + lfnPos[j]) = entry->cFileName[i];
					*(p + lfnPos[j]+1) = 0;
				}
				p[0] |= 0x40;	// END
				for (i=strlen(fname)-1; i>=0; i--)
					if (fname[i]=='.') break;
				if ((i==0)&&(strcmp(fname,".")==0)) i = 1;
				if (i<0) i = strlen(fname);
				for (j=0; j<i; j++)
					files[numFiles].name[j] = fname[j];
				for (; j<8; j++)
					files[numFiles].name[j] = 0x20;
				for (j=0; j<3; j++) {
					if ((j+i+1)>=strlen(fname)) break;
					files[numFiles].ext[j] = fname[j+i+1];
				}
				for (; j<3; j++)
					files[numFiles].ext[j] = 0x20;
			}

			//files[numFiles].fileSize = entry->nFileSizeLow;
			files[numFiles].fileSize = 0;

			if (entry->flags & FS_IS_DIR) {
				if (strcmp(fname,".")==0)
					fileLink[numFiles].level = maxLevel;
				else
					fileLink[numFiles].level = maxLevel+1;
				files[numFiles].attrib = ATTRIB_DIR;
			} else {
				files[numFiles].attrib = 0;
			}

			fileLink[numFiles].parent = fileLevel;	

			numFiles++;
		} else if (fileLevel > 0) {
			fileLink[fileLevel].filesInDir += 1;
			strncpy((char*)&files[numFiles].name[0],"..      ",8);
			strncpy((char*)&files[numFiles].ext[0],"   ",3);
			fileLink[numFiles].parent = fileLevel;	
			files[numFiles].attrib = 0x10;
			numFiles++;
		}
	}
}


/* List all files and subdirectories recursively */
void list_files(char *fpath) {
	void * hFind;
	FsEntry entry;
	char			DirSpec[255 + 1],SubDir[255+1];
	u32			dwError;
	char			*fname;
	int				i,j;
	int fileLevel;

	maxLevel++;
	fileLevel = maxLevel;

	strncpy(DirSpec, fpath, strlen(fpath)+1);

	hFind = FsReadFirst(DirSpec, &entry);

	if (hFind == NULL) {
		return;
	} else {
		fname = (strlen(entry.cAlternateFileName)>0)?entry.cAlternateFileName:entry.cFileName;
		add_file(fname, &entry, fileLevel);

		while (FsReadNext(hFind, &entry) != 0) {
			fname = (strlen(entry.cAlternateFileName)>0)?entry.cAlternateFileName:entry.cFileName;
			add_file(fname, &entry, fileLevel);

			if (numFiles==MAXFILES-1) break;

			if ((entry.flags & FS_IS_DIR) && (strcmp(fname, ".")) && (strcmp(fname, ".."))) {
				sprintf(SubDir, "%s%c%s", fpath, FS_SEPARATOR, fname);
				list_files(SubDir);
			}
		}
    
		dwError = FsError();
		FsClose(hFind);
		if (dwError != FS_ERR_NO_MORE_FILES) {
			return;
		}
	}
	if (numFiles < MAXFILES) {
		fileLink[numFiles].parent = fileLevel;
		files[numFiles++].name[0] = 0;
	}
}






/* Set up the MBR, FAT and DIR_ENTs */
BOOL cflash_build_fat() {
	int i,j,k,l,
		clust,baseCluster,numClusters,
		clusterNum2,rootCluster;
	int fileLevel;

	numFiles  = 0;
	fileLevel = -1;
	maxLevel  = -1;

	sRomPath  = szRomPath;   // From MMU.cpp
	files = (DIR_ENT *) malloc(MAXFILES*sizeof(DIR_ENT));
	fileLink = (FILE_INFO *) malloc(MAXFILES*sizeof(FILE_INFO));
	if ((files == NULL) || (fileLink == NULL))
                return FALSE;

	for (i=0; i<MAXFILES; i++) {
		files[i].attrib = 0;
		files[i].name[0] = FILE_FREE;
		files[i].fileSize = 0;

		fileLink[i].filesInDir = 0;

		extraDirEntries[i] = NULL;
		numExtraEntries[i] = 0;
	}

	list_files(sRomPath);

	k            = 0;
	clusterNum   = rootCluster = (SECRESV + SECPERFAT)/SECPERCLUS;
	numClusters  = 0;
	clust        = 0;
	numRootFiles = 0;

	// Allocate memory to hold information about the files 
	dirEntries = (DIR_ENT *) malloc(numFiles*sizeof(DIR_ENT));
	dirEntryLink = (FILE_INFO *) malloc(numFiles*sizeof(FILE_INFO));
	dirEntriesInCluster = (int *) malloc(NUMCLUSTERS*sizeof(int));
	dirEntryPtr = (DIR_ENT **) malloc(NUMCLUSTERS*sizeof(DIR_ENT*));
	if ((dirEntries==NULL) || (dirEntriesInCluster==NULL) || (dirEntryPtr==NULL))
                return FALSE;

	for (i=0; i<NUMCLUSTERS; i++) {
		dirEntriesInCluster[i] = 0;
		dirEntryPtr[i] = NULL;
	}

	// Change the hierarchical layout to a flat one 
	for (i=0; i<=maxLevel; i++) {
		clusterNum2 = clusterNum;
		for (j=0; j<numFiles; j++) {
			if (fileLink[j].parent == i) {
				if (dirEntryPtr[clusterNum] == NULL)
					dirEntryPtr[clusterNum] = &dirEntries[k];
				dirEntryLink[k] = fileLink[j];
				dirEntries[k++] = files[j];
				if ((files[j].attrib & ATTRIB_LFN)==0) {
					if (files[j].attrib & ATTRIB_DIR) {
						if (strncmp((char*)&files[j].name[0],".       ",8)==0) {
							dirEntries[k-1].startCluster = dirEntryLink[k-1].level; 
						} else if (strncmp((char*)&files[j].name[0],"..      ",8)==0) {
							dirEntries[k-1].startCluster = dirEntryLink[k-1].parent; 
						} else {
							clust++;
							dirEntries[k-1].startCluster = clust;
							l = (fileLink[fileLink[j].level].filesInDir)>>8;
							clust += l;
							numClusters += l;
						}
					} else {
						dirEntries[k-1].startCluster = clusterNum;
					}
				}
				if (i==0) numRootFiles++;
				dirEntriesInCluster[clusterNum]++;
				if (dirEntriesInCluster[clusterNum]==256) {
					clusterNum++;
				}
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
	strcpy((char*)&MBR.OEMName[0],"DESMUM");
	strcpy((char*)&MBR.fat16.fileSysType[0],"FAT16  ");
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
	for (i=1; i<numFiles; i++) {
		if (((dirEntries[i].attrib & ATTRIB_DIR)!=0) &&
			((dirEntries[i].attrib & ATTRIB_LFN)==0)) {
			if (dirEntries[i].startCluster > rootCluster)
				dirEntries[i].startCluster += clust-rootCluster;
		}
	}
	lastDirEntCluster = clust+numClusters-1;

	// Set up the cluster values for all files
	clust += numClusters; //clusterNum;
	for (i=0; i<numFiles; i++) {
		if (((dirEntries[i].attrib & ATTRIB_DIR)==0) &&
			((dirEntries[i].attrib & ATTRIB_LFN)==0)) {
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
	for (i=0; i<numFiles; i++) {
		if (((dirEntries[i].attrib & ATTRIB_LFN)==0) &&
			(dirEntries[i].name[0] != FILE_FREE)) {
			j = 0;
			l = dirEntries[i].fileSize - (512*SECPERCLUS);
			while (l > 0) {
				if (dirEntries[i].startCluster+j < MAXFILES)
					FAT16[dirEntries[i].startCluster+j] = dirEntries[i].startCluster+j+1;
				j++;
				l -= (512*16);
			} 
			if ((dirEntries[i].attrib & ATTRIB_DIR)==0) {
				if (dirEntries[i].startCluster+j < MAXFILES)
					FAT16[dirEntries[i].startCluster+j] = 0xFFFF;
			}
			k = dirEntries[i].startCluster+j;
		}
	}

	for (i=(filesysData/SECPERCLUS); i<NUMCLUSTERS; i++) {
		if (dirEntriesInCluster[i]==256)
			FAT16[i] = i+1;
	}

        return TRUE;
}




BOOL cflash_init() {
        cflashDeviceEnabled = FALSE;
	currLBA = 0;
	
	/* Set up addresses for M3CF */
	/*CF_REG_DATA = 0x8800000;
	CF_REG_ERR	= 0x8820000;
	CF_REG_SEC	= 0x8840000;
	CF_REG_LBA1	= 0x8860000;
	CF_REG_LBA2	= 0x8880000;
	CF_REG_LBA3	= 0x88A0000;
	CF_REG_LBA4	= 0x88C0000;
	CF_REG_CMD	= 0x88E0000;
	CF_REG_STS	= 0x80C0000;*/

	/* Set up addresses for GBAMP */
	CF_REG_DATA = 0x9000000;
	CF_REG_ERR	= 0x9020000;
	CF_REG_SEC	= 0x9040000;
	CF_REG_LBA1	= 0x9060000;
	CF_REG_LBA2	= 0x9080000;
	CF_REG_LBA3	= 0x90A0000;
	CF_REG_LBA4	= 0x90C0000;
	CF_REG_CMD	= 0x90E0000;
	CF_REG_STS	= 0x98C0000;
    	
	if (activeDirEnt != -1)
		fclose(hFile);
	activeDirEnt = -1;
	fileStartLBA = fileEndLBA = 0xFFFFFFFF;
	if (!cflash_build_fat())
                return FALSE;
	cf_reg_sts = 0x58;	// READY

        cflashDeviceEnabled = TRUE;
        return TRUE;
}


/* Convert a space-padded 8.3 filename into an asciiz string */
void fatstring_to_asciiz(int dirent,char *out,DIR_ENT *d) {
	int i,j;
	DIR_ENT *pd;

	if (d == NULL)
		pd = &dirEntries[dirent];
	else
		pd = d;

	for (i=0; i<8; i++) {
		if (pd->name[i] == ' ') break;
		out[i] = pd->name[i];
	}
	if ((pd->attrib & 0x10)==0) {
		out[i++] = '.';
		for (j=0; j<3; j++) {
			if (pd->ext[j] == ' ') break;
			out[i++] = pd->ext[j];
		}
	}
	out[i] = '\0';
}



/* Resolve the path of a files by working backwards through the directory entries */
void resolve_path(int dirent) {
	int i;
	char dirname[128];

	while (dirEntryLink[dirent].parent > 0) {
		for (i=0; i<dirent; i++) {
			if ((dirEntryLink[dirent].parent==dirEntryLink[i].level) &&
				((dirEntries[i].attrib&ATTRIB_DIR)!=0)) {
				fatstring_to_asciiz(i,dirname,NULL);
				strcat(fpath,dirname);
				strcat(fpath,"\\");
				dirent = i;
				break;
			}
		}
	}
}


/* Read from a file using a 512 byte buffer */
u16 fread_buffered(int dirent,u32 cluster,u32 offset) {
	char fname[32];
	int i,j;

	offset += cluster*512*SECPERCLUS;

	if (dirent == activeDirEnt) {
		if ((offset < bufferStart) || (offset >= bufferStart + 512)) {
			//SetFilePointer(hFile,offset,NULL,FILE_BEGIN);
			fseek(hFile, offset, SEEK_SET);
			//ReadFile(hFile,&freadBuffer,512,&dwBytesRead,NULL);
			fread(&freadBuffer, 1, 512, hFile);
			bufferStart = offset;
		}

		return freadBuffer[(offset-bufferStart)>>1];
	}
	if (activeDirEnt != -1)
		//CloseHandle(hFile);
		fclose(hFile);

	strcpy(fpath,sRomPath);
	strcat(fpath,"\\");
	
	resolve_path(dirent);

	fatstring_to_asciiz(dirent,fname,NULL);
	strcat(fpath,fname);

        hFile = fopen(fpath, "w");
        if (!hFile)
		return 0;
	fread(&freadBuffer, 1, 512, hFile);

	bufferStart = offset;
	activeDirEnt = dirent;
	fileStartLBA = (dirEntries[dirent].startCluster*512*SECPERCLUS);
	fileEndLBA = fileStartLBA + dirEntries[dirent].fileSize;

	return freadBuffer[(offset-bufferStart)>>1];
}

/* Read from one of the CF device registers */
unsigned int cflash_read(unsigned int address) {
	unsigned char *p;
        u16 s;
	int i;
	u32 cluster,cluster2,cluster3,fileLBA;

	if (address == CF_REG_STS) {
		s = cf_reg_sts;
		return s; 

	} else if (address == CF_REG_DATA) {
		cluster = (currLBA / (512 * SECPERCLUS));
		cluster2 = (((currLBA/512) - filesysData) / SECPERCLUS) + 2;

		if (cf_reg_cmd == 0x20) {
			// Reading from the MBR 
			if (currLBA < 512) {
				p = (unsigned char*)&MBR;
                                s = T1ReadWord(p, currLBA);

			// Reading the FAT 
			} else if ((currLBA >= filesysFAT*512) && (currLBA < filesysRootDir*512)) {
				p = (unsigned char*)&FAT16[0];
                                s = T1ReadWord(p, currLBA - filesysFAT * 512);

			// Reading directory entries 
			} else if ((currLBA >= filesysRootDir*512) &&
				       (cluster <= lastDirEntCluster)) {
				cluster3 = ((currLBA - (SECRESV * 512)) / (512 * SECPERCLUS));
				i = (currLBA-(((cluster3-(filesysRootDir/SECPERCLUS))*SECPERCLUS+filesysRootDir)*512)); //(currLBA - cluster*BYTESPERCLUS);
				if (i < (dirEntriesInCluster[cluster3]*32)) {

					p = (unsigned char*)dirEntryPtr[cluster3];
                                	s = T1ReadWord(p, i);
				} else {
					i /= 32;
					i -= dirEntriesInCluster[cluster3];
					if ((i>=0)&&(i<numExtraEntries[cluster3])) {
						p = (unsigned char*)extraDirEntries[cluster3];
                                		s = T1ReadWord(p, i * 32 + (currLBA & 0x1F));
					} else if ((currLBA&0x1F)==0) {
						s = FILE_FREE;
					} else {
						s = 0;
					}
				}
			// Reading file data 
			} else if ((cluster2 > lastDirEntCluster) && (cluster2 <= lastFileDataCluster)) { //else if ((cluster>lastDirEntCluster)&&(cluster<=lastFileDataCluster)) {
				fileLBA = currLBA - (filesysData-32)*512;	// 32 = # sectors used for the root entries

				// Check if the read is from the currently opened file 
				if ((fileLBA >= fileStartLBA) && (fileLBA < fileEndLBA)) {
					cluster = (fileLBA / (512 * SECPERCLUS));
					s = fread_buffered(activeDirEnt,cluster-dirEntries[activeDirEnt].startCluster,(fileLBA-fileStartLBA)&(BYTESPERCLUS-1)); 
				} else {
					for (i=0; i<numFiles; i++) {
						if ((fileLBA>=(dirEntries[i].startCluster*512*SECPERCLUS)) &&
							(fileLBA <(dirEntries[i].startCluster*512*SECPERCLUS)+dirEntries[i].fileSize) &&
							((dirEntries[i].attrib & (ATTRIB_DIR|ATTRIB_LFN))==0)) {
							cluster = (fileLBA / (512 * SECPERCLUS));
							s = fread_buffered(i,cluster-dirEntries[i].startCluster,fileLBA&(BYTESPERCLUS-1));
							break;
						}
					}
				}
			} 
			currLBA += 2;
			return s;
		}

	} else if (address == CF_REG_CMD) {
		s = 0;
		return 0;

	} else if (address == CF_REG_LBA1) {
		s = cf_reg_lba1;
		return s; 
	}
        return 0;
}




/* Write to one of the CF device registers */
void cflash_write(unsigned int address,unsigned int data) {
	u32 cluster,cluster2,cluster3,fileLBA;
	int i,j,k,l;
	unsigned char *p;
        u16 s;
	DIR_ENT *d;

	if (address==CF_REG_STS) {
		cf_reg_sts = data&0xFFFF;

	} else if (address==CF_REG_DATA) {

	} else if (address==CF_REG_CMD) {
		cf_reg_cmd = data&0xFF;
		cf_reg_sts = 0x58;	// READY
	
	} else if (address==CF_REG_LBA1) {
		cf_reg_lba1 = data&0xFF;
		currLBA = (currLBA&0xFFFFFF00)|cf_reg_lba1;
	
	} else if (address==CF_REG_LBA2) {
		cf_reg_lba2 = data&0xFF;
		currLBA = (currLBA&0xFFFF00FF)|(cf_reg_lba2<<8);
	
	} else if (address==CF_REG_LBA3) {
		cf_reg_lba3 = data&0xFF;
		currLBA = (currLBA&0xFF00FFFF)|(cf_reg_lba3<<16);
	
	} else if (address==CF_REG_LBA4) {
		cf_reg_lba4 = data&0xFF;
		currLBA = (currLBA&0x00FFFFFF)|((cf_reg_lba4&0x0F)<<24);
		currLBA *= 512;
	}
}



void cflash_close() {
	int i;

	if (cflashDeviceEnabled) {
                cflashDeviceEnabled = FALSE;

		for (i=0; i<MAXFILES; i++) {
			if (extraDirEntries[i] != NULL)
				free(extraDirEntries[i]);
		}

		if (dirEntries != NULL)
			free(dirEntries);

		if (dirEntryLink != NULL)
			free(dirEntryLink);

		if (activeDirEnt != -1)
			fclose(hFile);
	}
}
