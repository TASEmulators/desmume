/*
	Copyright 2009-2015 DeSmuME team

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

//based on Arduino SdFat Library ( http://code.google.com/p/sdfatlib/ )
//Copyright (C) 2009 by William Greiman

//based on mkdosfs - utility to create FAT/MS-DOS filesystems
//Copyright (C) 1991 Linus Torvalds <torvalds@klaava.helsinki.fi>
//Copyright (C) 1992-1993 Remy Card <card@masi.ibp.fr>
//Copyright (C) 1993-1994 David Hudson <dave@humbug.demon.co.uk>
//Copyright (C) 1998 H. Peter Anvin <hpa@zytor.com>
//Copyright (C) 1998-2005 Roman Hodek <Roman.Hodek@informatik.uni-erlangen.de>

//based on libfat
//Copyright (c) 2006 Michael "Chishm" Chisholm

#ifndef EMUFAT_H
#define EMUFAT_H

#include "emufat_types.h"
#include <stdio.h>

class EMUFILE;

#define BOOTCODE_SIZE		448
#define BOOTCODE_FAT32_SIZE	420


// use the gnu style oflag in open()
/** open() oflag for reading */
static const u8 EO_READ = 0X01;
/** open() oflag - same as O_READ */
static const u8 EO_RDONLY = EO_READ;
/** open() oflag for write */
static const u8 EO_WRITE = 0X02;
/** open() oflag - same as O_WRITE */
static const u8 EO_WRONLY = EO_WRITE;
/** open() oflag for reading and writing */
static const u8 EO_RDWR = (EO_READ | EO_WRITE);
/** open() oflag mask for access modes */
static const u8 EO_ACCMODE = (EO_READ | EO_WRITE);
/** The file offset shall be set to the end of the file prior to each write. */
static const u8 EO_APPEND = 0X04;
/** synchronous writes - call sync() after each write */
static const u8 EO_SYNC = 0X08;
/** create the file if nonexistent */
static const u8 EO_CREAT = 0X10;
/** If O_CREAT and O_EXCL are set, open() shall fail if the file exists */
static const u8 EO_EXCL = 0X20;
/** truncate the file to zero length */
static const u8 EO_TRUNC = 0X40;


//Value for byte 510 of boot block or MBR
static const u8 BOOTSIG0 = 0X55;
//Value for byte 511 of boot block or MBR
static const u8 BOOTSIG1 = 0XAA;

static void (*dateTime_)(u16* date, u16* time) = NULL;


#include "PACKED.h"

//A partition table entry for a MBR formatted storage device.
//The MBR partition table has four entries.
struct __PACKED TPartitionRecord {
	//Boot Indicator . Indicates whether the volume is the active
	//partition.  Legal values include: 0X00. Do not use for booting.
	//0X80 Active partition.
	u8 boot;

	//Head part of Cylinder-head-sector address of the first block in
	//the partition. Legal values are 0-255. Only used in old PC BIOS.
	u8 beginHead;

	struct
	{
		//Sector part of Cylinder-head-sector address of the first block in
		//the partition. Legal values are 1-63. Only used in old PC BIOS.
		u32 beginSector : 6;
		//High bits cylinder for first block in partition.
		u32 beginCylinderHigh : 2;
	};

	//Combine beginCylinderLow with beginCylinderHigh. Legal values
	//are 0-1023.  Only used in old PC BIOS.
	u8 beginCylinderLow;
	//Partition type. See defines that begin with PART_TYPE_ for
	//some Microsoft partition types.
	u8 type;

	//head part of cylinder-head-sector address of the last sector in the
	//partition.  Legal values are 0-255. Only used in old PC BIOS.
	u8 endHead;

	struct 
	{
		//Sector part of cylinder-head-sector address of the last sector in
		//the partition.  Legal values are 1-63. Only used in old PC BIOS.
		u32 endSector : 6;
		// High bits of end cylinder
		u32 endCylinderHigh : 2;
	};

	//Combine endCylinderLow with endCylinderHigh. Legal values
	//are 0-1023.  Only used in old PC BIOS.
	u8 endCylinderLow;

	//Logical block address of the first block in the partition.
	u32 firstSector;

	//Length of the partition, in blocks.
	u32 totalSectors;
};

//Master Boot Record:
//The first block of a storage device that is formatted with a MBR.
struct __PACKED TMasterBootRecord {
	//Code Area for master boot program.
	u8 codeArea[440];
	//Optional WindowsNT disk signature. May contain more boot code.
	u32 diskSignature;
	//Usually zero but may be more boot code. 
	u16 usuallyZero;
	//Partition tables.
	TPartitionRecord part[4];
	//First MBR signature byte. Must be 0X55
	u8 mbrSig0;
	//Second MBR signature byte. Must be 0XAA
	u8 mbrSig1;
};

struct __PACKED msdos_volume_info {
  u8 drive_number;	/* BIOS drive number */
  u8 RESERVED;	/* Unused */
  u8 ext_boot_sign;	/* 0x29 if fields below exist (DOS 3.3+) */
  u32 volume_id;	/* Volume ID number */
  u8 volume_label[11];/* Volume label */
  u8 fs_type[8];	/* Typically FAT12 or FAT16 */
};

//Boot sector for a FAT16 or FAT32 volume.
struct __PACKED TFat32BootSector {
	//X86 jmp to boot program
	u8  jmpToBootCode[3];
	//informational only - don't depend on it
	u8     oemName[8];

	//Count of bytes per sector. This value may take on only the
	//following values: 512, 1024, 2048 or 4096
	u16 bytesPerSector;
	//Number of sectors per allocation unit. This value must be a
	//power of 2 that is greater than 0. The legal values are
	//1, 2, 4, 8, 16, 32, 64, and 128.
	u8  sectorsPerCluster; //cluster_size
	//Number of sectors before the first FAT.
	//This value must not be zero.
	u16 reservedSectorCount;
	//The count of FAT data structures on the volume. This field should
	//always contain the value 2 for any FAT volume of any type.
	u8  fatCount;
	//For FAT12 and FAT16 volumes, this field contains the count of
	//32-byte directory entries in the root directory. For FAT32 volumes,
	//this field must be set to 0. For FAT12 and FAT16 volumes, this
	//value should always specify a count that when multiplied by 32
	//results in a multiple of bytesPerSector.  FAT16 volumes should
	//use the value 512.
	u16 rootDirEntryCount; //dir_entries
	//This field is the old 16-bit total count of sectors on the volume.
	//This count includes the count of all sectors in all four regions
	//of the volume. This field can be 0; if it is 0, then totalSectors32
	//must be non-zero.  For FAT32 volumes, this field must be 0. For
	//FAT12 and FAT16 volumes, this field contains the sector count, and
	//totalSectors32 is 0 if the total sector count fits
	//(is less than 0x10000).
	u16 totalSectors16;
	//This dates back to the old MS-DOS 1.x media determination and is
	//no longer usually used for anything.  0xF8 is the standard value
	//for fixed (non-removable) media. For removable media, 0xF0 is
	//frequently used. Legal values are 0xF0 or 0xF8-0xFF.
	u8  mediaType;
	//Count of sectors occupied by one FAT on FAT12/FAT16 volumes.
	//On FAT32 volumes this field must be 0, and sectorsPerFat32
	//contains the FAT size count.
	u16 sectorsPerFat16;
	//Sectors per track for interrupt 0x13. Not used otherwise. 
	u16 sectorsPerTrack;
	//Number of heads for interrupt 0x13.  Not used otherwise. 
	u16 headCount;
	//Count of hidden sectors preceding the partition that contains this
	//FAT volume. This field is generally only relevant for media
	//visible on interrupt 0x13.
	u32 hiddenSectors;
	//This field is the new 32-bit total count of sectors on the volume.
	//This count includes the count of all sectors in all four regions
	//of the volume.  This field can be 0; if it is 0, then
	//totalSectors16 must be non-zero.
	u32 totalSectors32;

	union {
		struct __PACKED {
			msdos_volume_info vi;
			u8 boot_code[BOOTCODE_SIZE];
		} oldfat;

		struct __PACKED
		{
			//Count of sectors occupied by one FAT on FAT32 volumes.
			u32 sectorsPerFat32; //fat32_length;	/* sectors/FAT */

			//This field is only defined for FAT32 media and does not exist on
			//FAT12 and FAT16 media.
			//Bits 0-3 -- Zero-based number of active FAT.
			//            Only valid if mirroring is disabled.
			//Bits 4-6 -- Reserved.
			//Bit 7	-- 0 means the FAT is mirrored at runtime into all FATs.
			//       -- 1 means only one FAT is active; it is the one referenced in bits 0-3.
			//Bits 8-15 	-- Reserved.
			u16 fat32Flags;//	flags;		/* bit 8: fat mirroring, low 4: active fat */

			//FAT32 version. High byte is major revision number.
			//Low byte is minor revision number. Only 0.0 define.
			u16 fat32Version;//version[2];	/* major, minor filesystem version */

			//Cluster number of the first cluster of the root directory for FAT32.
			//This usually 2 but not required to be 2.
			u32 fat32RootCluster; //root_cluster;	/* first cluster in root directory */

			//Sector number of FSINFO structure in the reserved area of the
			//FAT32 volume. Usually 1.
			u16 fat32FSInfo;//	info_sector;	/* filesystem info sector */

			//If non-zero, indicates the sector number in the reserved area
			//of the volume of a copy of the boot record. Usually 6.
			//No value other than 6 is recommended.
			u16 fat32BackBootBlock; //backup_boot;	/* backup boot sector */

			//Reserved for future expansion. Code that formats FAT32 volumes
			//should always set all of the bytes of this field to 0.
			u8 fat32Reserved[12]; //reserved2[6];	/* Unused */
			
			msdos_volume_info vi;

			u8 boot_code[BOOTCODE_FAT32_SIZE];
		} fat32;
	};

	u8 boot_sign[2];
};

#include "PACKED_END.h"

// End Of Chain values for FAT entries
//FAT16 end of chain value used by Microsoft. 
static const u16 FAT16EOC = 0XFFFF;
//Minimum value for FAT16 EOC.  Use to test for EOC. 
static const u16 FAT16EOC_MIN = 0XFFF8;
//FAT32 end of chain value used by Microsoft. 
static const u32 FAT32EOC = 0X0FFFFFFF;
//Minimum value for FAT32 EOC.  Use to test for EOC. 
static const u32 FAT32EOC_MIN = 0X0FFFFFF8;
//Mask a for FAT32 entry. Entries are 28 bits. 
static const u32 FAT32MASK = 0X0FFFFFFF;

//------------------------------------------------------------------------------


//\struct directoryEntry
//\brief FAT short directory entry
//Short means short 8.3 name, not the entry size.
// 
//Date Format. A FAT directory entry date stamp is a 16-bit field that is 
//basically a date relative to the MS-DOS epoch of 01/01/1980. Here is the
//format (bit 0 is the LSB of the 16-bit word, bit 15 is the MSB of the 
//16-bit word):
//  
//Bits 9-15: Count of years from 1980, valid value range 0-127 
//inclusive (1980-2107).
//  
//Bits 5-8: Month of year, 1 = January, valid value range 1-12 inclusive.
//Bits 0-4: Day of month, valid value range 1-31 inclusive.
//Time Format. A FAT directory entry time stamp is a 16-bit field that has
//a granularity of 2 seconds. Here is the format (bit 0 is the LSB of the 
//16-bit word, bit 15 is the MSB of the 16-bit word).
//  
//Bits 11-15: Hours, valid value range 0-23 inclusive.
//
//Bits 5-10: Minutes, valid value range 0-59 inclusive.
//     
//Bits 0-4: 2-second count, valid value range 0-29 inclusive (0 - 58 seconds).
//  
//The valid time range is from Midnight 00:00:00 to 23:59:58.
struct TDirectoryEntry {
	//Short 8.3 name.
	//The first eight bytes contain the file name with blank fill.
	//The last three bytes contain the file extension with blank fill.

	u8 name[11];
	//Entry attributes.
	//The upper two bits of the attribute byte are reserved and should
	//always be set to 0 when a file is created and never modified or
	//looked at after that.  See defines that begin with DIR_ATT_.

	u8  attributes;

	//Reserved for use by Windows NT. Set value to 0 when a file is
	//created and never modify or look at it after that.

	u8  reservedNT;

	//The granularity of the seconds part of creationTime is 2 seconds
	//so this field is a count of tenths of a second and its valid
	//value range is 0-199 inclusive. (WHG note - seems to be hundredths)

	u8  creationTimeTenths;
	//Time file was created. 
	u16 creationTime;
	//Date file was created. 
	u16 creationDate;

	//Last access date. Note that there is no last access time, only
	//a date.  This is the date of last read or write. In the case of
	//a write, this should be set to the same date as lastWriteDate.

	u16 lastAccessDate;

	//High word of this entry's first cluster number (always 0 for a
	//FAT12 or FAT16 volume).

	u16 firstClusterHigh;
	//Time of last write. File creation is considered a write. 
	u16 lastWriteTime;
	// Date of last write. File creation is considered a write. 
	u16 lastWriteDate;
	// Low word of this entry's first cluster number. 
	u16 firstClusterLow;
	//32-bit unsigned holding this file's size in bytes. 
	u32 fileSize;
};

//escape for name[0] = 0xE5 
static const u8 DIR_NAME_0XE5 = 0x05;
//name[0] value for entry that is free after being "deleted" 
static const u8 DIR_NAME_DELETED = 0xE5;
//name[0] value for entry that is free and no allocated entries follow 
static const u8 DIR_NAME_FREE = 0x00;
//file is read-only 
static const u8 DIR_ATT_READ_ONLY = 0x01;
//File should hidden in directory listings 
static const u8 DIR_ATT_HIDDEN = 0x02;
//Entry is for a system file 
static const u8 DIR_ATT_SYSTEM = 0x04;
//Directory entry contains the volume label 
static const u8 DIR_ATT_VOLUME_ID = 0x08;
//Entry is for a directory 
static const u8 DIR_ATT_DIRECTORY = 0x10;
//Old DOS archive bit for backup support 
static const u8 DIR_ATT_ARCHIVE = 0x20;
//Test value for long name entry.  Test is
  //(d->attributes & DIR_ATT_LONG_NAME_MASK) == DIR_ATT_LONG_NAME. 
static const u8 DIR_ATT_LONG_NAME = 0x0F;
//Test mask for long name entry 
static const u8 DIR_ATT_LONG_NAME_MASK = 0x3F;
//defined attribute bits 
static const u8 DIR_ATT_DEFINED_BITS = 0x3F;
//Directory entry is part of a long name 
static inline u8 DIR_IS_LONG_NAME(const TDirectoryEntry* dir) {
  return (dir->attributes & DIR_ATT_LONG_NAME_MASK) == DIR_ATT_LONG_NAME;
}
//Mask for file/subdirectory tests 
static const u8 DIR_ATT_FILE_TYPE_MASK = (DIR_ATT_VOLUME_ID | DIR_ATT_DIRECTORY);
//Directory entry is for a file 
static inline u8 DIR_IS_FILE(const TDirectoryEntry* dir) {
  return (dir->attributes & DIR_ATT_FILE_TYPE_MASK) == 0;
}
//Directory entry is for a subdirectory 
static inline u8 DIR_IS_SUBDIR(const TDirectoryEntry* dir) {
  return (dir->attributes & DIR_ATT_FILE_TYPE_MASK) == DIR_ATT_DIRECTORY;
}
//Directory entry is for a file or subdirectory 
static inline u8 DIR_IS_FILE_OR_SUBDIR(const TDirectoryEntry* dir) {
  return (dir->attributes & DIR_ATT_VOLUME_ID) == 0;
}

// flags for timestamp
/** set the file's last access date */
static const u8 T_ACCESS = 1;
/** set the file's creation date and time */
static const u8 T_CREATE = 2;
/** Set the file's write date and time */
static const u8 T_WRITE = 4;
// values for type_
/** This SdFile has not been opened. */
static const u8 FAT_FILE_TYPE_CLOSED = 0;
/** SdFile for a file */
static const u8 FAT_FILE_TYPE_NORMAL = 1;
/** SdFile for a FAT16 root directory */
static const u8 FAT_FILE_TYPE_ROOT16 = 2;
/** SdFile for a FAT32 root directory */
static const u8 FAT_FILE_TYPE_ROOT32 = 3;
/** SdFile for a subdirectory */
static const u8 FAT_FILE_TYPE_SUBDIR = 4;
/** Test value for directory type */
static const u8 FAT_FILE_TYPE_MIN_DIR = FAT_FILE_TYPE_ROOT16;

/** date field for FAT directory entry */
static inline u16 FAT_DATE(u16 year, u8 month, u8 day) {
  return (year - 1980) << 9 | month << 5 | day;
}
/** year part of FAT directory date field */
static inline u16 FAT_YEAR(u16 fatDate) {
  return 1980 + (fatDate >> 9);
}
/** month part of FAT directory date field */
static inline u8 FAT_MONTH(u16 fatDate) {
  return (fatDate >> 5) & 0XF;
}
/** day part of FAT directory date field */
static inline u8 FAT_DAY(u16 fatDate) {
  return fatDate & 0X1F;
}
/** time field for FAT directory entry */
static inline u16 FAT_TIME(u8 hour, u8 minute, u8 second) {
  return hour << 11 | minute << 5 | second >> 1;
}
/** hour part of FAT directory time field */
static inline u8 FAT_HOUR(u16 fatTime) {
  return fatTime >> 11;
}
/** minute part of FAT directory time field */
static inline u8 FAT_MINUTE(u16 fatTime) {
  return(fatTime >> 5) & 0X3F;
}
/** second part of FAT directory time field */
static inline u8 FAT_SECOND(u16 fatTime) {
  return 2*(fatTime & 0X1F);
}
/** Default date for file timestamps is 1 Jan 2000 */
static const u16 FAT_DEFAULT_DATE = ((2000 - 1980) << 9) | (1 << 5) | 1;
/** Default time for file timestamp is 1 am */
static const u16 FAT_DEFAULT_TIME = (1 << 11);

//------------------------------------------------------

class EmuFat;
class EmuFatVolume;
class EmuFatFile;

union cache_t {
           /** Used to access cached file data blocks. */
  u8  data[512];
           /** Used to access cached FAT16 entries. */
  u16 fat16[256];
           /** Used to access cached FAT32 entries. */
  u32 fat32[128];
           /** Used to access cached directory entries. */
  TDirectoryEntry    dir[16];
           /** Used to access a cached MasterBoot Record. */
  TMasterBootRecord    mbr;
           /** Used to access to a cached FAT boot sector. */
  TFat32BootSector    fbs;
};

class EmuFatFile
{
 public:
  /** Create an instance of EmuFatFile. */
  EmuFatFile() : type_(FAT_FILE_TYPE_CLOSED) {}

  bool writeError;
  void clearUnbufferedRead(void) {
    flags_ &= ~F_FILE_UNBUFFERED_READ;
  }
  void setUnbufferedRead(void) {
    if (isFile()) flags_ |= F_FILE_UNBUFFERED_READ;
  }
  u8 unbufferedRead(void) const {
    return flags_ & F_FILE_UNBUFFERED_READ;
  }

  u8 close(void);
  u8 contiguousRange(u32* bgnBlock, u32* endBlock);
  u8 createContiguous(EmuFatFile* dirFile, const char* fileName, u32 size);
  /** \return The current cluster number for a file or directory. */
  u32 curCluster(void) const {return curCluster_;}
  /** \return The current position for a file or directory. */
  u32 curPosition(void) const {return curPosition_;}

    u8 rmDir(void);
	u8 rmRfStar(void);
  s16 read(void) {
    u8 b;
    return read(&b, 1) == 1 ? b : -1;
  }
  s32 read(void* buf, u32 nbyte);
  s8 readDir(TDirectoryEntry* dir);
  s32 write(const void* buf, u32 nbyte);

  u8 openRoot(EmuFatVolume* vol);
  u8 timestamp(u8 flag, u16 year, u8 month, u8 day, u8 hour, u8 minute, u8 second);
  u8 sync(void);
  u8 makeDir(EmuFatFile* dir, const char* dirName);
  u8 open(EmuFatFile* dirFile, u16 index, u8 oflag);
  u8 open(EmuFatFile* dirFile, const char* fileName, u8 oflag);
  u8 remove(EmuFatFile* dirFile, const char* fileName);
  u8 remove(void);
  u8 dirEntry(TDirectoryEntry* dir);
  u8 seekCur(u32 pos) {
    return seekSet(curPosition_ + pos);
  }
  /**
   *  Set the files current position to end of file.  Useful to position
   *  a file for append. See seekSet().
   */
  u8 seekEnd(void) {return seekSet(fileSize_);}
  u8 seekSet(u32 pos);

  u8 type(void) const {return type_;}
  u8 truncate(u32 size);

  u32 dirBlock(void) const {return dirBlock_;}
  /** \return Index of this file's directory in the block dirBlock. */
  u8 dirIndex(void) const {return dirIndex_;}
  static void dirName(const TDirectoryEntry& dir, char* name);
  /** \return The total number of bytes in a file or directory. */
  u32 fileSize(void) const {return fileSize_;}
  /** \return The first cluster number for a file or directory. */
  u32 firstCluster(void) const {return firstCluster_;}
  /** \return True if this is a SdFile for a directory else false. */
  u8 isDir(void) const {return type_ >= FAT_FILE_TYPE_MIN_DIR;}
  /** \return True if this is a SdFile for a file else false. */
  u8 isFile(void) const {return type_ == FAT_FILE_TYPE_NORMAL;}
  /** \return True if this is a SdFile for an open file/directory else false. */
  u8 isOpen(void) const {return type_ != FAT_FILE_TYPE_CLOSED;}
  /** \return True if this is a SdFile for a subdirectory else false. */
  u8 isSubDir(void) const {return type_ == FAT_FILE_TYPE_SUBDIR;}
  /** \return True if this is a SdFile for the root directory. */
  u8 isRoot(void) const {
    return type_ == FAT_FILE_TYPE_ROOT16 || type_ == FAT_FILE_TYPE_ROOT32;
  }

  /** Set the file's current position to zero. */
  void rewind(void) {
    curPosition_ = curCluster_ = 0;
  }


private:
  // bits defined in flags_
  // should be 0XF
  static const u8 F_OFLAG = (EO_ACCMODE | EO_APPEND | EO_SYNC);
  // available bits
  static const u8 F_UNUSED = 0x30;
  // use unbuffered SD read
  static const u8 F_FILE_UNBUFFERED_READ = 0X40;
  // sync of directory entry required
  static const u8 F_FILE_DIR_DIRTY = 0x80;
// make sure F_OFLAG is ok

  void ctassert()
  {
	  CTASSERT(!((F_UNUSED | F_FILE_UNBUFFERED_READ | F_FILE_DIR_DIRTY) & F_OFLAG));
  }

  // private data
  u8   flags_;         // See above for definition of flags_ bits
  u8   type_;          // type of file see above for values
  u32  curCluster_;    // cluster for current file position
  u32  curPosition_;   // current file position in bytes from beginning
  u32  dirBlock_;      // SD block that contains directory entry for file
  u8   dirIndex_;      // index of entry in dirBlock 0 <= dirIndex_ <= 0XF
  u32  fileSize_;      // file size in bytes
  u32  firstCluster_;  // first cluster of file
  EmuFatVolume* vol_;           // volume where file is located

  // private functions
  u8 addCluster(void);
  u8 addDirCluster(void);
  TDirectoryEntry* cacheDirEntry(u8 action);
  static u8 make83Name(const char* str, u8* name);
  u8 openCachedEntry(u8 cacheIndex, u8 oflags);
  TDirectoryEntry* readDirCache(void);
};

class EmuFatVolume
{
public:
	EmuFatVolume() :allocSearchStart_(2), fatType_(0) {}

   //Initialize a FAT volume.  Try partition one first then try super floppy format.
   //dev The Sd2Card where the volume is located.
   //return The value one, true, is returned for success and
   //the value zero, false, is returned for failure.  Reasons for
   //failure include not finding a valid partition, not finding a valid
   //FAT file system or an I/O error.
  bool init(EmuFat* dev) { return init(dev, 1) ? true : init(dev, 0);}
  bool init(EmuFat* dev, u8 part);

  bool format(u32 sectors);
  bool formatNew(u32 sectors);

  // inline functions that return volume info
  //The volume's cluster size in blocks.
  u8 blocksPerCluster(void) const {return blocksPerCluster_;}
	//The number of blocks in one FAT.
  u32 blocksPerFat(void)  const {return blocksPerFat_;}
 //The total number of clusters in the volume. //
  u32 clusterCount(void) const {return clusterCount_;}
  //The shift count required to multiply by blocksPerCluster. //
  u8 clusterSizeShift(void) const {return clusterSizeShift_;}
  //The logical block number for the start of file data. //
  u32 dataStartBlock(void) const {return dataStartBlock_;}
  //The number of FAT structures on the volume. //
  u8 fatCount(void) const {return fatCount_;}
  //The logical block number for the start of the first FAT. //
  u32 fatStartBlock(void) const {return fatStartBlock_;}
  //The FAT type of the volume. Values are 12, 16 or 32. //
  u8 fatType(void) const {return fatType_;}
  //The number of entries in the root directory for FAT16 volumes. //
  u32 rootDirEntryCount(void) const {return rootDirEntryCount_;}
  //The logical block number for the start of the root directory on FAT16 volumes or the first cluster number on FAT32 volumes. //
  u32 rootDirStart(void) const {return rootDirStart_;}

  EmuFat* dev_;


private:
  friend class EmuFatFile;

  u32 allocSearchStart_;   // start cluster for alloc search
  u8 blocksPerCluster_;    // cluster size in blocks
  u32 blocksPerFat_;       // FAT size in blocks
  u32 clusterCount_;       // clusters in one FAT
  u8 clusterSizeShift_;    // shift to convert cluster count to block count
  u32 dataStartBlock_;     // first data block number
  u8 fatCount_;            // number of FATs on volume
  u32 fatStartBlock_;      // start block for first FAT
  u8 fatType_;             // volume type (12, 16, OR 32)
  u16 rootDirEntryCount_;  // number of entries in FAT16 root dir
  u32 rootDirStart_;       // root start block for FAT16, cluster for FAT32

	u8 allocContiguous(u32 count, u32* curCluster);
  u8 blockOfCluster(u32 position) const {
          return (position >> 9) & (blocksPerCluster_ - 1);}
  u32 clusterStartBlock(u32 cluster) const {
           return dataStartBlock_ + ((cluster - 2) << clusterSizeShift_);}
  u32 blockNumber(u32 cluster, u32 position) const {
           return clusterStartBlock(cluster) + blockOfCluster(position);}
  u8 fatGet(u32 cluster, u32* value) const;
  u8 fatPut(u32 cluster, u32 value);
  u8 fatPutEOC(u32 cluster) {
    return fatPut(cluster, 0x0FFFFFFF);
  }
  u8 chainSize(u32 beginCluster, u32* size) const;
  u8 freeChain(u32 cluster);
  u8 isEOC(u32 cluster) const {
    return  cluster >= (fatType_ == 16 ? FAT16EOC_MIN : FAT32EOC_MIN);
  }
  u8 readData(u32 block, u16 offset, u16 count, u8* dst);
   u8 writeBlock(u32 block, const u8* dst);

};

class EmuFat
{
public:
	EmuFat(const char* fname, bool readonly=false);
	EmuFat();
	EmuFat(EMUFILE* fileNotToDelete);
	virtual ~EmuFat();

private:
	EMUFILE* m_pFile;
	bool m_readonly, m_owns;

	friend class EmuFatVolume;
	friend class EmuFatFile;

  // value for action argument in cacheRawBlock to indicate read from cache
  static const u8 CACHE_FOR_READ = 0;
  // value for action argument in cacheRawBlock to indicate cache dirty
  static const u8 CACHE_FOR_WRITE = 1;

  struct Cache
  {
	  Cache()
		  : cacheBlockNumber_(0xFFFFFFFF)
		  , cacheDirty_(0)  // cacheFlush() will write block if true
		  , cacheMirrorBlock_(0)  // mirror  block for second FAT
	  {}

	  cache_t cacheBuffer_;        // 512 byte cache for device blocks
	  u32 cacheBlockNumber_;  // Logical number of block in the cache
	  u8 cacheDirty_;         // cacheFlush() will write block if true
	  u32 cacheMirrorBlock_;  // block number for mirror FAT
  } cache_;

  u8 cacheRawBlock(u32 blockNumber, u8 action);
  void cacheSetDirty() {cache_.cacheDirty_ |= CACHE_FOR_WRITE;}
  u8 cacheZeroBlock(u32 blockNumber);
  u8 cacheFlush();
  void cacheReset();

  //IO functions:
  u8 readBlock(u32 block, u8* dst);
  u8 writeBlock(u32 blockNumber, const u8* src);
  u8 readData(u32 block, u16 offset, u16 count, u8* dst);

  void truncate(u32 size);
};

#endif //EMUFAT_H
