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
#define DIR_SEP "\\"
#else
#define DIR_SEP "/"
#endif

#include "PACKED.h"
typedef struct
{
        u8      jmpBoot[3] __PACKED;
        u8      OEMName[8] __PACKED;
	// BIOS Parameter Block
        u16     bytesPerSector __PACKED;
        u8      sectorsPerCluster __PACKED;
        u16     reservedSectors __PACKED;
        u8      numFATs __PACKED;
        u16     rootEntries __PACKED;
        u16     numSectorsSmall __PACKED;
        u8      mediaDesc __PACKED;
        u16     sectorsPerFAT __PACKED;
        u16     sectorsPerTrk __PACKED;
        u16     numHeads __PACKED;
        u32     numHiddenSectors __PACKED;
        u32     numSectors __PACKED;
	
	struct  
	{
		// Ext BIOS Parameter Block for FAT16
        u8      driveNumber __PACKED;
        u8      reserved1 __PACKED;
        u8      extBootSig __PACKED;
        u32     volumeID __PACKED;
        u8      volumeLabel[11] __PACKED;
        u8      fileSysType[8] __PACKED;
		// Bootcode
        u8      bootCode[448] __PACKED;
        u16		signature __PACKED;	
	} __PACKED fat16;

} __PACKED BOOT_RECORD;
#include "PACKED_END.h"

// Directory entry - must be packed
#include "PACKED.h"
typedef struct
{
        u8      name[NAME_LEN] __PACKED;
        u8      ext[EXT_LEN] __PACKED;
        u8      attrib __PACKED;
        u8      reserved __PACKED;
        u8      cTime_ms __PACKED;
        u16     cTime __PACKED;
        u16     cDate __PACKED;
        u16     aDate __PACKED;
        u16     startClusterHigh __PACKED;
        u16     mTime __PACKED;
        u16     mDate __PACKED;
        u16     startCluster __PACKED;
        u32     fileSize __PACKED;
} __PACKED DIR_ENT;
#include "PACKED_END.h"

#endif //
