/*
  FAT.H
  Mic, 2006
  Structures taken from Michael Chisholm's FAT library
*/

#ifndef __FAT_H__
#define __FAT_H__

#include "types.h"
#include "PACKED.h"
#include "PACKED_END.h"

#define ATTRIB_DIR 0x10
#define ATTRIB_LFN 0x0F

#define FILE_FREE 0xE5
/* Name and extension maximum length */
#define NAME_LEN 8
#define EXT_LEN 3

// Boot Sector - must be packed
#if defined(_MSC_VER) || defined(__INTEL_COMPILER)
#pragma pack(push, 1)
#define DIR_SEP "\\"
typedef struct
{
        u8      jmpBoot[3];
        u8      OEMName[8];
	// BIOS Parameter Block
        u16     bytesPerSector;
        u8      sectorsPerCluster;
        u16     reservedSectors;
        u8      numFATs;
        u16     rootEntries;
        u16     numSectorsSmall;
        u8      mediaDesc;
        u16     sectorsPerFAT;
        u16     sectorsPerTrk;
        u16     numHeads;
        u32     numHiddenSectors;
        u32     numSectors;
	struct  
	{
		// Ext BIOS Parameter Block for FAT16
                u8      driveNumber;
                u8      reserved1;
                u8      extBootSig;
                u32     volumeID;
                u8      volumeLabel[11];
                u8      fileSysType[8];
		// Bootcode
                u8      bootCode[448];
                u16 signature;
	} fat16;
} __PACKED BOOT_RECORD;
#pragma pack(pop)

// Directory entry - must be packed
#pragma pack(push, 1)
typedef struct
{
        u8      name[NAME_LEN];
        u8      ext[EXT_LEN];
        u8      attrib;
        u8      reserved;
        u8      cTime_ms;
        u16     cTime;
        u16     cDate;
        u16     aDate;
        u16     startClusterHigh;
        u16     mTime;
        u16     mDate;
        u16     startCluster;
        u32     fileSize;
} __PACKED DIR_ENT;
#pragma pack(pop)
#else
#define DIR_SEP "/"
typedef struct
{
	u8	jmpBoot[3];
	u8	OEMName[8];
	// BIOS Parameter Block
	u16	bytesPerSector;
	u8	sectorsPerCluster;
	u16	reservedSectors;
	u8	numFATs;
	u16	rootEntries;
	u16	numSectorsSmall;
	u8	mediaDesc;
	u16	sectorsPerFAT;
	u16	sectorsPerTrk;
	u16	numHeads;
	u32	numHiddenSectors;
	u32	numSectors;
	struct  
	{
		// Ext BIOS Parameter Block for FAT16
		u8	driveNumber;
		u8	reserved1;
		u8	extBootSig;
		u32	volumeID;
		u8	volumeLabel[11];
		u8	fileSysType[8];
		// Bootcode
		u8	bootCode[448];
		u16 signature;
	} fat16;
} __PACKED BOOT_RECORD;

// Directory entry - must be packed
typedef struct
{
 	u8	name[NAME_LEN];
	u8	ext[EXT_LEN];
	u8	attrib;
	u8	reserved;
	u8	cTime_ms;
	u16	cTime;
	u16	cDate;
	u16	aDate;
	u16	startClusterHigh;
	u16	mTime;
	u16	mDate;
	u16	startCluster;
	u32	fileSize;
} __PACKED DIR_ENT;
#endif

#endif
