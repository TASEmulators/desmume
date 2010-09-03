//Copyright (C) 2009-2010 DeSmuME team
//based on Arduino SdFat Library ( http://code.google.com/p/sdfatlib/ )
/* 
 * Copyright (C) 2009 by William Greiman
 *
 * This file is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This file is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with the Arduino SdFat Library.  If not, see
 * <http://www.gnu.org/licenses/>.
 */

#include "emufat.h"

static const u8 mkdosfs_bootcode[420] =
{
    0xFE, 0x54, 0x68, 0x69, 0x73, 0x20, 0x69, 0x73, 0x20, 0x6E, 0x6F, 0x74, 0x20, 0x61, 0x20, 0x62, 
    0x6F, 0x6F, 0x74, 0x61, 0x62, 0x6C, 0x65, 0x20, 0x64, 0x69, 0x73, 0x6B, 0x2E, 0x20, 0x20, 0x50, 
    0x6C, 0x65, 0x61, 0x73, 0x65, 0x20, 0x69, 0x6E, 0x73, 0x65, 0x72, 0x74, 0x20, 0x61, 0x20, 0x62, 
    0x6F, 0x6F, 0x74, 0x61, 0x62, 0x6C, 0x65, 0x20, 0x66, 0x6C, 0x6F, 0x70, 0x70, 0x79, 0x20, 0x61, 
    0x6E, 0x64, 0x0D, 0x0A, 0x70, 0x72, 0x65, 0x73, 0x73, 0x20, 0x61, 0x6E, 0x79, 0x20, 0x6B, 0x65, 
    0x79, 0x20, 0x74, 0x6F, 0x20, 0x74, 0x72, 0x79, 0x20, 0x61, 0x67, 0x61, 0x69, 0x6E, 0x20, 0x2E, 
    0x2E, 0x2E, 0x20, 0x0D, 0x0A, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
    0x00, 0x00, 0x00, 0x00
};



EmuFat::EmuFat(const char* fname, bool readonly)
	: m_readonly(readonly)
	, m_owns(true)
{
	m_pFile = new EMUFILE_FILE(fname,readonly?"rb":"rb+");
}

EmuFat::EmuFat()
	: m_readonly(false)
	, m_owns(true)
{
	m_pFile = new EMUFILE_MEMORY();
}

EmuFat::EmuFat(EMUFILE* fileNotToDelete)
	: m_pFile(fileNotToDelete)
	, m_owns(false)
	, m_readonly(false)
{
}

EmuFat::~EmuFat()
{
	if(m_owns)
		delete m_pFile;
}


u8 EmuFat::cacheRawBlock(u32 blockNumber, u8 action)
{
  if (cache_.cacheBlockNumber_ != blockNumber) {
    if (!cacheFlush()) return false;
    if (!readBlock(blockNumber, cache_.cacheBuffer_.data)) return false;
    cache_.cacheBlockNumber_ = blockNumber;
  }
  cache_.cacheDirty_ |= action;
  return true;
}

u8 EmuFat::cacheZeroBlock(u32 blockNumber)
{
  if (!cacheFlush()) return false;

  // loop take less flash than memset(cacheBuffer_.data, 0, 512);
  for (u16 i = 0; i < 512; i++) {
    cache_.cacheBuffer_.data[i] = 0;
  }
  cache_.cacheBlockNumber_ = blockNumber;
  cacheSetDirty();
  return true;
}

u8 EmuFat::cacheFlush() {
  if (cache_.cacheDirty_) {
    if (!writeBlock(cache_.cacheBlockNumber_, cache_.cacheBuffer_.data)) {
      return false;
    }
    // mirror FAT tables
    if (cache_.cacheMirrorBlock_) {
      if (!writeBlock(cache_.cacheMirrorBlock_, cache_.cacheBuffer_.data)) {
        return false;
      }
      cache_.cacheMirrorBlock_ = 0;
    }
    cache_.cacheDirty_ = 0;
  }
  return true;
}

u8 EmuFat::readBlock(u32 block, u8* dst)
{
  m_pFile->fseek(block*512,SEEK_SET);
  m_pFile->fread(dst,512);
  if(m_pFile->fail())
  {
	  m_pFile->unfail();
	  return 0;
  }
  return 1;
}

u8 EmuFat::writeBlock(u32 blockNumber, const u8* src)
{
  m_pFile->fseek(blockNumber*512,SEEK_SET);
  m_pFile->fwrite(src,512);
  if(m_pFile->fail())
  {
	  m_pFile->unfail();
	  return 0;
  }
  return 1;
}

u8 EmuFat::readData(u32 block, u16 offset, u16 count, u8* dst)
{
	m_pFile->fseek(block*512+offset,SEEK_SET);
	m_pFile->fread(dst,count);
	if(m_pFile->fail())
	{
	  m_pFile->unfail();
	  return 0;
	}
	return 1;
}

void EmuFat::truncate(u32 size)
{
	m_pFile->truncate(size);
}

//-------------------------------------------------------------------------------------

//well, there are a lot of ways to format a disk. this is just a simple one.
//it would be nice if someone who understood fat better could modify the root
//directory setup to use reasonable code instead of magic arrays
bool EmuFatVolume::format(u32 sectors)
{
	u32 volumeStartBlock = 0;
	dev_->truncate(0);
	dev_->truncate(sectors*512);
	if (!dev_->cacheRawBlock(volumeStartBlock, EmuFat::CACHE_FOR_WRITE)) return false;
	memset(&dev_->cache_.cacheBuffer_,0,sizeof(dev_->cache_.cacheBuffer_));
	TFat32BootSector* bs = &dev_->cache_.cacheBuffer_.fbs;
	TBiosParmBlock* bpb = &bs->bpb;

	bs->jmpToBootCode[0] = 0xEB;
	bs->jmpToBootCode[1] = 0x3C;
	bs->jmpToBootCode[2] = 0x90;
	memcpy(bs->oemName,"mkdosfs",8);
	bs->driveNumber = 0;
	bs->reserved1 = 0;
	bs->bootSignature = 0x29;
	bs->volumeSerialNumber = 0;
	memcpy(bs->volumeLabel,"           ",11);
	memcpy(bs->fileSystemType,"FAT16   ",8);
	memcpy(bs->bootCode,mkdosfs_bootcode,420);
	bs->bootSectorSig0 = 0x55;
	bs->bootSectorSig1 = 0xAA;

	bpb->bytesPerSector = 512;
	bpb->sectorsPerCluster = 4;
	bpb->reservedSectorCount = 1;
	bpb->fatCount = 2;
	bpb->rootDirEntryCount = 512;
	bpb->totalSectors16 = 0;
	bpb->mediaType = 0xF8;
	bpb->sectorsPerFat16 = 32;
	bpb->sectorsPerTrack = 32;
	bpb->headCount = 64;
	bpb->hiddenSectors = 0;
	bpb->totalSectors32 = sectors;
	bpb->fat32Flags = 0xbe0d;
	bpb->fat32Version = 0x20Fd;
	bpb->fat32RootCluster = 0x20202020;
	bpb->fat32FSInfo = 0x2020;
	bpb->fat32BackBootBlock = 0x2020;

	if(!dev_->cacheFlush())
		return false;

	if (!dev_->cacheRawBlock(1, EmuFat::CACHE_FOR_WRITE)) return false;

	static const u8 rootEntry[8] =
	{
		0xF8, 0xFF, 0xFF, 0xFF, 
	} ;

	memcpy(dev_->cache_.cacheBuffer_.data,rootEntry,4);

	if(!dev_->cacheFlush())
		return false;

	if (!dev_->cacheRawBlock(33, EmuFat::CACHE_FOR_WRITE)) return false;

	memcpy(dev_->cache_.cacheBuffer_.data,rootEntry,4);

	if(!dev_->cacheFlush())
		return false;

	return init(dev_,0);
}

bool EmuFatVolume::init(EmuFat* dev, u8 part) {
  u32 volumeStartBlock = 0;
  dev_ = dev;
  // if part == 0 assume super floppy with FAT boot sector in block zero
  // if part > 0 assume mbr volume with partition table
  if (part) {
    if (part > 4) return false;
	if (!dev->cacheRawBlock(volumeStartBlock, EmuFat::CACHE_FOR_READ)) return false;
    TPartitionRecord* p = &dev->cache_.cacheBuffer_.mbr.part[part-1];
    if ((p->boot & 0X7F) !=0  ||
      p->totalSectors < 100 ||
      p->firstSector == 0) {
      // not a valid partition
      return false;
    }
    volumeStartBlock = p->firstSector;
  }
  if (!dev->cacheRawBlock(volumeStartBlock, EmuFat::CACHE_FOR_READ)) return false;
  TBiosParmBlock* bpb = &dev->cache_.cacheBuffer_.fbs.bpb;
  if (bpb->bytesPerSector != 512 ||
    bpb->fatCount == 0 ||
    bpb->reservedSectorCount == 0 ||
    bpb->sectorsPerCluster == 0) {
       // not valid FAT volume
      return false;
  }
  fatCount_ = bpb->fatCount;
  blocksPerCluster_ = bpb->sectorsPerCluster;

  // determine shift that is same as multiply by blocksPerCluster_
  clusterSizeShift_ = 0;
  while (blocksPerCluster_ != (1 << clusterSizeShift_)) {
    // error if not power of 2
    if (clusterSizeShift_++ > 7) return false;
  }
  blocksPerFat_ = bpb->sectorsPerFat16 ?
                    bpb->sectorsPerFat16 : bpb->sectorsPerFat32;

  fatStartBlock_ = volumeStartBlock + bpb->reservedSectorCount;

  // count for FAT16 zero for FAT32
  rootDirEntryCount_ = bpb->rootDirEntryCount;

  // directory start for FAT16 dataStart for FAT32
  rootDirStart_ = fatStartBlock_ + bpb->fatCount * blocksPerFat_;

  // data start for FAT16 and FAT32
  dataStartBlock_ = rootDirStart_ + ((32 * bpb->rootDirEntryCount + 511)/512);

  // total blocks for FAT16 or FAT32
  u32 totalBlocks = bpb->totalSectors16 ?
                           bpb->totalSectors16 : bpb->totalSectors32;
  // total data blocks
  clusterCount_ = totalBlocks - (dataStartBlock_ - volumeStartBlock);

  // divide by cluster size to get cluster count
  clusterCount_ >>= clusterSizeShift_;

  // FAT type is determined by cluster count
  if (clusterCount_ < 4085) {
    fatType_ = 12;
  } else if (clusterCount_ < 65525) {
    fatType_ = 16;
  } else {
    rootDirStart_ = bpb->fat32RootCluster;
    fatType_ = 32;
  }
  return true;
}

u8 EmuFatVolume::allocContiguous(u32 count, u32* curCluster) {
  // start of group
  u32 bgnCluster;

  // flag to save place to start next search
  u8 setStart;

  // set search start cluster
  if (*curCluster) {
    // try to make file contiguous
    bgnCluster = *curCluster + 1;

    // don't save new start location
    setStart = false;
  } else {
    // start at likely place for free cluster
    bgnCluster = allocSearchStart_;

    // save next search start if one cluster
    setStart = 1 == count;
  }
  // end of group
  u32 endCluster = bgnCluster;

  // last cluster of FAT
  u32 fatEnd = clusterCount_ + 1;

  // search the FAT for free clusters
  for (u32 n = 0;; n++, endCluster++) {
    // can't find space checked all clusters
    if (n >= clusterCount_) return false;

    // past end - start from beginning of FAT
    if (endCluster > fatEnd) {
      bgnCluster = endCluster = 2;
    }
    u32 f;
    if (!fatGet(endCluster, &f)) return false;

    if (f != 0) {
      // cluster in use try next cluster as bgnCluster
      bgnCluster = endCluster + 1;
    } else if ((endCluster - bgnCluster + 1) == count) {
      // done - found space
      break;
    }
  }
  // mark end of chain
  if (!fatPutEOC(endCluster)) return false;

  // link clusters
  while (endCluster > bgnCluster) {
    if (!fatPut(endCluster - 1, endCluster)) return false;
    endCluster--;
  }
  if (*curCluster != 0) {
    // connect chains
    if (!fatPut(*curCluster, bgnCluster)) return false;
  }
  // return first cluster number to caller
  *curCluster = bgnCluster;

  // remember possible next free cluster
  if (setStart) allocSearchStart_ = bgnCluster + 1;

  return true;
}

u8 EmuFatVolume::fatGet(u32 cluster, u32* value) const {
  if (cluster > (clusterCount_ + 1)) return false;
  u32 lba = fatStartBlock_;
  lba += fatType_ == 16 ? cluster >> 8 : cluster >> 7;
  if (lba != dev_->cache_.cacheBlockNumber_) {
	  if (!dev_->cacheRawBlock(lba, EmuFat::CACHE_FOR_READ)) return false;
  }
  if (fatType_ == 16) {
    *value = dev_->cache_.cacheBuffer_.fat16[cluster & 0XFF];
  } else {
    *value = dev_->cache_.cacheBuffer_.fat32[cluster & 0X7F] & FAT32MASK;
  }
  return true;
}
// Store a FAT entry
u8 EmuFatVolume::fatPut(u32 cluster, u32 value) {
  // error if reserved cluster
  if (cluster < 2) return false;

  // error if not in FAT
  if (cluster > (clusterCount_ + 1)) return false;

  // calculate block address for entry
  u32 lba = fatStartBlock_;
  lba += fatType_ == 16 ? cluster >> 8 : cluster >> 7;

  if (lba != dev_->cache_.cacheBlockNumber_) {
	  if (!dev_->cacheRawBlock(lba, EmuFat::CACHE_FOR_READ)) return false;
  }
  // store entry
  if (fatType_ == 16) {
    dev_->cache_.cacheBuffer_.fat16[cluster & 0xFF] = value;
  } else {
    dev_->cache_.cacheBuffer_.fat32[cluster & 0x7F] = value;
  }
  dev_->cacheSetDirty();

  // mirror second FAT
  if (fatCount_ > 1) dev_->cache_.cacheMirrorBlock_ = lba + blocksPerFat_;
  return true;
}

// return the size in bytes of a cluster chain
u8 EmuFatVolume::chainSize(u32 cluster, u32* size) const {
  u32 s = 0;
  do {
    if (!fatGet(cluster, &cluster)) return false;
    s += 512UL << clusterSizeShift_;
  } while (!isEOC(cluster));
  *size = s;
  return true;
}

// free a cluster chain
u8 EmuFatVolume::freeChain(u32 cluster) {
  // clear free cluster location
  allocSearchStart_ = 2;

  do {
    u32 next;
    if (!fatGet(cluster, &next)) return false;

    // free cluster
    if (!fatPut(cluster, 0)) return false;

    cluster = next;
  } while (!isEOC(cluster));

  return true;
}
u8 EmuFatVolume::readData(u32 block, u16 offset, u16 count, u8* dst) {
     return dev_->readData(block, offset, count, dst);
 }

u8 EmuFatVolume::writeBlock(u32 block, const u8* dst) {
    return dev_->writeBlock(block, dst);
  }

//-----------------------------------------------------------------------------------
//EmuFatFile:

// add a cluster to a file
u8 EmuFatFile::addCluster() {
  if (!vol_->allocContiguous(1, &curCluster_)) return false;

  // if first cluster of file link to directory entry
  if (firstCluster_ == 0) {
    firstCluster_ = curCluster_;
    flags_ |= F_FILE_DIR_DIRTY;
  }
  return true;
}


// Add a cluster to a directory file and zero the cluster.
// return with first block of cluster in the cache
u8 EmuFatFile::addDirCluster(void) {
  if (!addCluster()) return false;

  // zero data in cluster insure first cluster is in cache
  u32 block = vol_->clusterStartBlock(curCluster_);
  for (u8 i = vol_->blocksPerCluster_; i != 0; i--) {
    if (!vol_->dev_->cacheZeroBlock(block + i - 1)) return false;
  }
  // Increase directory file size by cluster size
  fileSize_ += 512UL << vol_->clusterSizeShift_;
  return true;
}


// cache a file's directory entry
// return pointer to cached entry or null for failure
TDirectoryEntry* EmuFatFile::cacheDirEntry(u8 action) {
  if (!vol_->dev_->cacheRawBlock(dirBlock_, action)) return NULL;
  return vol_->dev_->cache_.cacheBuffer_.dir + dirIndex_;
}


/**
 *  Close a file and force cached data and directory information
 *  to be written to the storage device.
 *
 * \return The value one, true, is returned for success and
 * the value zero, false, is returned for failure.
 * Reasons for failure include no file is open or an I/O error.
 */
u8 EmuFatFile::close(void) {
  if (!sync())return false;
  type_ = FAT_FILE_TYPE_CLOSED;
  return true;
}


/**
 * Check for contiguous file and return its raw block range.
 *
 * \param[out] bgnBlock the first block address for the file.
 * \param[out] endBlock the last  block address for the file.
 *
 * \return The value one, true, is returned for success and
 * the value zero, false, is returned for failure.
 * Reasons for failure include file is not contiguous, file has zero length
 * or an I/O error occurred.
 */
u8 EmuFatFile::contiguousRange(u32* bgnBlock, u32* endBlock) {
  // error if no blocks
  if (firstCluster_ == 0) return false;

  for (u32 c = firstCluster_; ; c++) {
    u32 next;
    if (!vol_->fatGet(c, &next)) return false;

    // check for contiguous
    if (next != (c + 1)) {
      // error if not end of chain
      if (!vol_->isEOC(next)) return false;
      *bgnBlock = vol_->clusterStartBlock(firstCluster_);
      *endBlock = vol_->clusterStartBlock(c)
                  + vol_->blocksPerCluster_ - 1;
      return true;
    }
  }
}

//------------------------------------------------------------------------------
/**
 * Create and open a new contiguous file of a specified size.
 *
 * \note This function only supports short DOS 8.3 names.
 * See open() for more information.
 *
 * \param[in] dirFile The directory where the file will be created.
 * \param[in] fileName A valid DOS 8.3 file name.
 * \param[in] size The desired file size.
 *
 * \return The value one, true, is returned for success and
 * the value zero, false, is returned for failure.
 * Reasons for failure include \a fileName contains
 * an invalid DOS 8.3 file name, the FAT volume has not been initialized,
 * a file is already open, the file already exists, the root
 * directory is full or an I/O error.
 *
 */
u8 EmuFatFile::createContiguous(EmuFatFile* dirFile, const char* fileName, u32 size) {
  // don't allow zero length file
  if (size == 0) return false;
  if (!open(dirFile, fileName, EO_CREAT | EO_EXCL | EO_RDWR)) return false;

  // calculate number of clusters needed
  u32 count = ((size - 1) >> (vol_->clusterSizeShift_ + 9)) + 1;

  // allocate clusters
  if (!vol_->allocContiguous(count, &firstCluster_)) {
    remove();
    return false;
  }
  fileSize_ = size;

  // insure sync() will update dir entry
  flags_ |= F_FILE_DIR_DIRTY;
  return sync();
}

/**
 * Return a files directory entry
 *
 * \param[out] dir Location for return of the files directory entry.
 *
 * \return The value one, true, is returned for success and
 * the value zero, false, is returned for failure.
 */
u8 EmuFatFile::dirEntry(TDirectoryEntry* dir) {
  // make sure fields on SD are correct
  if (!sync()) return false;

  // read entry
  TDirectoryEntry* p = cacheDirEntry(EmuFat::CACHE_FOR_READ);
  if (!p) return false;

  // copy to caller's struct
  memcpy(dir, p, sizeof(TDirectoryEntry));
  return true;
}

/**
 * Format the name field of \a dir into the 13 byte array
 * \a name in standard 8.3 short name format.
 *
 * \param[in] dir The directory structure containing the name.
 * \param[out] name A 13 byte char array for the formatted name.
 */
void EmuFatFile::dirName(const TDirectoryEntry& dir, char* name) {
  u8 j = 0;
  for (u8 i = 0; i < 11; i++) {
    if (dir.name[i] == ' ')continue;
    if (i == 8) name[j++] = '.';
    name[j++] = dir.name[i];
  }
  name[j] = 0;
}

// format directory name field from a 8.3 name string
u8 EmuFatFile::make83Name(const char* str, u8* name) {
  u8 c;
  u8 n = 7;  // max index for part before dot
  u8 i = 0;
  // blank fill name and extension
  while (i < 11) name[i++] = ' ';
  i = 0;
  while ((c = *str++) != '\0') {
    if (c == '.') {
      if (n == 10) return false;  // only one dot allowed
      n = 10;  // max index for full 8.3 name
      i = 8;   // place for extension
    } else {
      // illegal FAT characters
		static const char* px = "\\/:*?\"<>";
	  const char* p = px;
      u8 b;
      while ((b = *p++)) if (b == c) return false;
      // check size and only allow ASCII printable characters
      if (i > n || c < 0X21 || c > 0X7E)return false;
      // only upper case allowed in 8.3 names - convert lower to upper
      name[i++] = c < 'a' || c > 'z' ?  c : c + ('A' - 'a');
    }
  }
  // must have a file name, extension is optional
  return name[0] != ' ';
}

/** Make a new directory.
 *
 * \param[in] dir An open SdFat instance for the directory that will containing
 * the new directory.
 *
 * \param[in] dirName A valid 8.3 DOS name for the new directory.
 *
 * \return The value one, true, is returned for success and
 * the value zero, false, is returned for failure.
 * Reasons for failure include this SdFile is already open, \a dir is not a
 * directory, \a dirName is invalid or already exists in \a dir.
 */
u8 EmuFatFile::makeDir(EmuFatFile* dir, const char* dirName) {
  TDirectoryEntry d;

  // create a normal file
  if (!open(dir, dirName, EO_CREAT | EO_EXCL | EO_RDWR)) return false;

  // convert SdFile to directory
  flags_ = EO_READ;
  type_ = FAT_FILE_TYPE_SUBDIR;

  // allocate and zero first cluster
  if (!addDirCluster())return false;

  // force entry to SD
  if (!sync()) return false;

  // cache entry - should already be in cache due to sync() call
  TDirectoryEntry* p = cacheDirEntry(EmuFat::CACHE_FOR_WRITE);
  if (!p) return false;

  // change directory entry  attribute
  p->attributes = DIR_ATT_DIRECTORY;

  // make entry for '.'
  memcpy(&d, p, sizeof(d));
  for (u8 i = 1; i < 11; i++) d.name[i] = ' ';
  d.name[0] = '.';

  // cache block for '.'  and '..'
  u32 block = vol_->clusterStartBlock(firstCluster_);
  if (!vol_->dev_->cacheRawBlock(block, EmuFat::CACHE_FOR_WRITE)) return false;

  // copy '.' to block
  memcpy(&vol_->dev_->cache_.cacheBuffer_.dir[0], &d, sizeof(d));

  // make entry for '..'
  d.name[1] = '.';
  if (dir->isRoot()) {
    d.firstClusterLow = 0;
    d.firstClusterHigh = 0;
  } else {
    d.firstClusterLow = dir->firstCluster_ & 0XFFFF;
    d.firstClusterHigh = dir->firstCluster_ >> 16;
  }
  // copy '..' to block
  memcpy(&vol_->dev_->cache_.cacheBuffer_.dir[1], &d, sizeof(d));

  // set position after '..'
  curPosition_ = 2 * sizeof(d);

  // write first block
  return vol_->dev_->cacheFlush();
}

/**
 * Open a file or directory by name.
 *
 * \param[in] dirFile An open SdFat instance for the directory containing the
 * file to be opened.
 *
 * \param[in] fileName A valid 8.3 DOS name for a file to be opened.
 *
 * \param[in] oflag Values for \a oflag are constructed by a bitwise-inclusive
 * OR of flags from the following list
 *
 * O_READ - Open for reading.
 *
 * O_RDONLY - Same as O_READ.
 *
 * O_WRITE - Open for writing.
 *
 * O_WRONLY - Same as O_WRITE.
 *
 * O_RDWR - Open for reading and writing.
 *
 * O_APPEND - If set, the file offset shall be set to the end of the
 * file prior to each write.
 *
 * O_CREAT - If the file exists, this flag has no effect except as noted
 * under O_EXCL below. Otherwise, the file shall be created
 *
 * O_EXCL - If O_CREAT and O_EXCL are set, open() shall fail if the file exists.
 *
 * O_SYNC - Call sync() after each write.  This flag should not be used with
 * write(uint8_t), write_P(PGM_P), writeln_P(PGM_P), or the Arduino Print class.
 * These functions do character at a time writes so sync() will be called
 * after each byte.
 *
 * O_TRUNC - If the file exists and is a regular file, and the file is
 * successfully opened and is not read only, its length shall be truncated to 0.
 *
 * \note Directory files must be opened read only.  Write and truncation is
 * not allowed for directory files.
 *
 * \return The value one, true, is returned for success and
 * the value zero, false, is returned for failure.
 * Reasons for failure include this SdFile is already open, \a difFile is not
 * a directory, \a fileName is invalid, the file does not exist
 * or can't be opened in the access mode specified by oflag.
 */
u8 EmuFatFile::open(EmuFatFile* dirFile, const char* fileName, u8 oflag) {
  u8 dname[11];
  TDirectoryEntry* p;

  // error if already open
  if (isOpen())return false;

  if (!make83Name(fileName, dname)) return false;
  vol_ = dirFile->vol_;
  dirFile->rewind();

  // bool for empty entry found
  u8 emptyFound = false;

  // search for file
  while (dirFile->curPosition_ < dirFile->fileSize_) {
    u8 index = 0XF & (dirFile->curPosition_ >> 5);
    p = dirFile->readDirCache();
    if (p == NULL) return false;

    if (p->name[0] == DIR_NAME_FREE || p->name[0] == DIR_NAME_DELETED) {
      // remember first empty slot
      if (!emptyFound) {
        emptyFound = true;
        dirIndex_ = index;
        dirBlock_ = vol_->dev_->cache_.cacheBlockNumber_;
      }
      // done if no entries follow
      if (p->name[0] == DIR_NAME_FREE) break;
    } else if (!memcmp(dname, p->name, 11)) {
      // don't open existing file if O_CREAT and O_EXCL
      if ((oflag & (EO_CREAT | EO_EXCL)) == (EO_CREAT | EO_EXCL)) return false;

      // open found file
      return openCachedEntry(0XF & index, oflag);
    }
  }
  // only create file if O_CREAT and O_WRITE
  if ((oflag & (EO_CREAT | EO_WRITE)) != (EO_CREAT | EO_WRITE)) return false;

  // cache found slot or add cluster if end of file
  if (emptyFound) {
    p = cacheDirEntry(EmuFat::CACHE_FOR_WRITE);
    if (!p) return false;
  } else {
    if (dirFile->type_ == FAT_FILE_TYPE_ROOT16) return false;

    // add and zero cluster for dirFile - first cluster is in cache for write
    if (!dirFile->addDirCluster()) return false;

    // use first entry in cluster
    dirIndex_ = 0;
    p = vol_->dev_->cache_.cacheBuffer_.dir;
  }
  // initialize as empty file
  memset(p, 0, sizeof(TDirectoryEntry));
  memcpy(p->name, dname, 11);

  // set timestamps
  if (dateTime_) {
    // call user function
    dateTime_(&p->creationDate, &p->creationTime);
  } else {
    // use default date/time
    p->creationDate = FAT_DEFAULT_DATE;
    p->creationTime = FAT_DEFAULT_TIME;
  }
  p->lastAccessDate = p->creationDate;
  p->lastWriteDate = p->creationDate;
  p->lastWriteTime = p->creationTime;

  // force write of entry to SD
  if (!vol_->dev_->cacheFlush()) return false;

  // open entry in cache
  return openCachedEntry(dirIndex_, oflag);
}

/**
 * Open a file by index.
 *
 * \param[in] dirFile An open SdFat instance for the directory.
 *
 * \param[in] index The \a index of the directory entry for the file to be
 * opened.  The value for \a index is (directory file position)/32.
 *
 * \param[in] oflag Values for \a oflag are constructed by a bitwise-inclusive
 * OR of flags O_READ, O_WRITE, O_TRUNC, and O_SYNC.
 *
 * See open() by fileName for definition of flags and return values.
 *
 */
u8 EmuFatFile::open(EmuFatFile* dirFile, u16 index, u8 oflag) {
  // error if already open
  if (isOpen())return false;

  // don't open existing file if O_CREAT and O_EXCL - user call error
  if ((oflag & (EO_CREAT | EO_EXCL)) == (EO_CREAT | EO_EXCL)) return false;

  vol_ = dirFile->vol_;

  // seek to location of entry
  if (!dirFile->seekSet(32 * index)) return false;

  // read entry into cache
  TDirectoryEntry* p = dirFile->readDirCache();
  if (p == NULL) return false;

  // error if empty slot or '.' or '..'
  if (p->name[0] == DIR_NAME_FREE ||
      p->name[0] == DIR_NAME_DELETED || p->name[0] == '.') {
    return false;
  }
  // open cached entry
  return openCachedEntry(index & 0XF, oflag);
}

//------------------------------------------------------------------------------
// open a cached directory entry. Assumes vol_ is initializes
u8 EmuFatFile::openCachedEntry(u8 dirIndex, u8 oflag) {
  // location of entry in cache
  TDirectoryEntry* p = vol_->dev_->cache_.cacheBuffer_.dir + dirIndex;

  // write or truncate is an error for a directory or read-only file
  if (p->attributes & (DIR_ATT_READ_ONLY | DIR_ATT_DIRECTORY)) {
    if (oflag & (EO_WRITE | EO_TRUNC)) return false;
  }
  // remember location of directory entry on SD
  dirIndex_ = dirIndex;
  dirBlock_ = vol_->dev_->cache_.cacheBlockNumber_;

  // copy first cluster number for directory fields
  firstCluster_ = (u32)p->firstClusterHigh << 16;
  firstCluster_ |= p->firstClusterLow;

  // make sure it is a normal file or subdirectory
  if (DIR_IS_FILE(p)) {
    fileSize_ = p->fileSize;
    type_ = FAT_FILE_TYPE_NORMAL;
  } else if (DIR_IS_SUBDIR(p)) {
    if (!vol_->chainSize(firstCluster_, &fileSize_)) return false;
    type_ = FAT_FILE_TYPE_SUBDIR;
  } else {
    return false;
  }
  // save open flags for read/write
  flags_ = oflag & (EO_ACCMODE | EO_SYNC | EO_APPEND);

  // set to start of file
  curCluster_ = 0;
  curPosition_ = 0;

  // truncate file to zero length if requested
  if (oflag & EO_TRUNC) return truncate(0);
  return true;
}

//------------------------------------------------------------------------------
/**
 * Open a volume's root directory.
 *
 * \param[in] vol The FAT volume containing the root directory to be opened.
 *
 * \return The value one, true, is returned for success and
 * the value zero, false, is returned for failure.
 * Reasons for failure include the FAT volume has not been initialized
 * or it a FAT12 volume.
 */
u8 EmuFatFile::openRoot(EmuFatVolume* vol) {
  // error if file is already open
  if (isOpen()) return false;

  if (vol->fatType() == 16) {
    type_ = FAT_FILE_TYPE_ROOT16;
    firstCluster_ = 0;
    fileSize_ = 32 * vol->rootDirEntryCount();
  } else if (vol->fatType() == 32) {
    type_ = FAT_FILE_TYPE_ROOT32;
    firstCluster_ = vol->rootDirStart();
    if (!vol->chainSize(firstCluster_, &fileSize_)) return false;
  } else {
    // volume is not initialized or FAT12
    return false;
  }
  vol_ = vol;
  // read only
  flags_ = EO_READ;

  // set to start of file
  curCluster_ = 0;
  curPosition_ = 0;

  // root has no directory entry
  dirBlock_ = 0;
  dirIndex_ = 0;
  return true;
}

//------------------------------------------------------------------------------
/**
 * Read data from a file starting at the current position.
 *
 * \param[out] buf Pointer to the location that will receive the data.
 *
 * \param[in] nbyte Maximum number of bytes to read.
 *
 * \return For success read() returns the number of bytes read.
 * A value less than \a nbyte, including zero, will be returned
 * if end of file is reached.
 * If an error occurs, read() returns -1.  Possible errors include
 * read() called before a file has been opened, corrupt file system
 * or an I/O error occurred.
 */
s32 EmuFatFile::read(void* buf, u32 nbyte) {
  u8* dst = reinterpret_cast<u8*>(buf);

  // error if not open or write only
  if (!isOpen() || !(flags_ & EO_READ)) return -1;

  // max bytes left in file
  if (nbyte > (fileSize_ - curPosition_)) nbyte = fileSize_ - curPosition_;

  // amount left to read
  u32 toRead = nbyte;
  while (toRead > 0) {
    u32 block;  // raw device block number
    u16 offset = curPosition_ & 0x1FF;  // offset in block
    if (type_ == FAT_FILE_TYPE_ROOT16) {
      block = vol_->rootDirStart() + (curPosition_ >> 9);
    } else {
      u8 blockOfCluster = vol_->blockOfCluster(curPosition_);
      if (offset == 0 && blockOfCluster == 0) {
        // start of new cluster
        if (curPosition_ == 0) {
          // use first cluster in file
          curCluster_ = firstCluster_;
        } else {
          // get next cluster from FAT
          if (!vol_->fatGet(curCluster_, &curCluster_)) return -1;
        }
      }
      block = vol_->clusterStartBlock(curCluster_) + blockOfCluster;
    }
    u32 n = toRead;

    // amount to be read from current block
    if (n > (512UL - offset)) n = 512 - offset;

    // no buffering needed if n == 512 or user requests no buffering
    if ((unbufferedRead() || n == 512) &&
      block != vol_->dev_->cache_.cacheBlockNumber_) {
      if (!vol_->readData(block, offset, n, dst)) return -1;
      dst += n;
    } else {
      // read block to cache and copy data to caller
      if (!vol_->dev_->cacheRawBlock(block, EmuFat::CACHE_FOR_READ)) return -1;
      u8* src = vol_->dev_->cache_.cacheBuffer_.data + offset;
      u8* end = src + n;
      while (src != end) *dst++ = *src++;
    }
    curPosition_ += n;
    toRead -= n;
  }
  return nbyte;
}

//------------------------------------------------------------------------------
/**
 * Read the next directory entry from a directory file.
 *
 * \param[out] dir The dir_t struct that will receive the data.
 *
 * \return For success readDir() returns the number of bytes read.
 * A value of zero will be returned if end of file is reached.
 * If an error occurs, readDir() returns -1.  Possible errors include
 * readDir() called before a directory has been opened, this is not
 * a directory file or an I/O error occurred.
 */
s8 EmuFatFile::readDir(TDirectoryEntry* dir) {
  s16 n;
  // if not a directory file or miss-positioned return an error
  if (!isDir() || (0x1F & curPosition_)) return -1;

  while ((n = read(dir, sizeof(TDirectoryEntry))) == sizeof(TDirectoryEntry)) {
    // last entry if DIR_NAME_FREE
    if (dir->name[0] == DIR_NAME_FREE) break;
    // skip empty entries and entry for .  and ..
    if (dir->name[0] == DIR_NAME_DELETED || dir->name[0] == '.') continue;
    // return if normal file or subdirectory
    if (DIR_IS_FILE_OR_SUBDIR(dir)) return (s8)n;
  }
  // error, end of file, or past last entry
  return n < 0 ? -1 : 0;
}

// Read next directory entry into the cache
// Assumes file is correctly positioned
TDirectoryEntry* EmuFatFile::readDirCache(void) {
  // error if not directory
  if (!isDir()) return NULL;

  // index of entry in cache
  u8 i = (curPosition_ >> 5) & 0XF;

  // use read to locate and cache block
  if (read() < 0) return NULL;

  // advance to next entry
  curPosition_ += 31;

  // return pointer to entry
  return (vol_->dev_->cache_.cacheBuffer_.dir + i);
}

//------------------------------------------------------------------------------
/**
 * Remove a file.
 *
 * The directory entry and all data for the file are deleted.
 *
 * \note This function should not be used to delete the 8.3 version of a
 * file that has a long name. For example if a file has the long name
 * "New Text Document.txt" you should not delete the 8.3 name "NEWTEX~1.TXT".
 *
 * \return The value one, true, is returned for success and
 * the value zero, false, is returned for failure.
 * Reasons for failure include the file read-only, is a directory,
 * or an I/O error occurred.
 */
u8 EmuFatFile::remove(void) {
  // free any clusters - will fail if read-only or directory
  if (!truncate(0)) return false;

  // cache directory entry
  TDirectoryEntry* d = cacheDirEntry(EmuFat::CACHE_FOR_WRITE);
  if (!d) return false;

  // mark entry deleted
  d->name[0] = DIR_NAME_DELETED;

  // set this SdFile closed
  type_ = FAT_FILE_TYPE_CLOSED;

  // write entry to SD
  return vol_->dev_->cacheFlush();
}

//------------------------------------------------------------------------------
/**
 * Remove a file.
 *
 * The directory entry and all data for the file are deleted.
 *
 * \param[in] dirFile The directory that contains the file.
 * \param[in] fileName The name of the file to be removed.
 *
 * \note This function should not be used to delete the 8.3 version of a
 * file that has a long name. For example if a file has the long name
 * "New Text Document.txt" you should not delete the 8.3 name "NEWTEX~1.TXT".
 *
 * \return The value one, true, is returned for success and
 * the value zero, false, is returned for failure.
 * Reasons for failure include the file is a directory, is read only,
 * \a dirFile is not a directory, \a fileName is not found
 * or an I/O error occurred.
 */
u8 EmuFatFile::remove(EmuFatFile* dirFile, const char* fileName) {
  EmuFatFile file;
  if (!file.open(dirFile, fileName, EO_WRITE)) return false;
  return file.remove();
}

//------------------------------------------------------------------------------
/** Remove a directory file.
 *
 * The directory file will be removed only if it is empty and is not the
 * root directory.  rmDir() follows DOS and Windows and ignores the
 * read-only attribute for the directory.
 *
 * \note This function should not be used to delete the 8.3 version of a
 * directory that has a long name. For example if a directory has the
 * long name "New folder" you should not delete the 8.3 name "NEWFOL~1".
 *
 * \return The value one, true, is returned for success and
 * the value zero, false, is returned for failure.
 * Reasons for failure include the file is not a directory, is the root
 * directory, is not empty, or an I/O error occurred.
 */
u8 EmuFatFile::rmDir(void) {
  // must be open subdirectory
  if (!isSubDir()) return false;

  rewind();

  // make sure directory is empty
  while (curPosition_ < fileSize_) {
    TDirectoryEntry* p = readDirCache();
    if (p == NULL) return false;
    // done if past last used entry
    if (p->name[0] == DIR_NAME_FREE) break;
    // skip empty slot or '.' or '..'
    if (p->name[0] == DIR_NAME_DELETED || p->name[0] == '.') continue;
    // error not empty
    if (DIR_IS_FILE_OR_SUBDIR(p)) return false;
  }
  // convert empty directory to normal file for remove
  type_ = FAT_FILE_TYPE_NORMAL;
  flags_ |= EO_WRITE;
  return remove();
}

//------------------------------------------------------------------------------
/** Recursively delete a directory and all contained files.
 *
 * This is like the Unix/Linux 'rm -rf *' if called with the root directory
 * hence the name.
 *
 * Warning - This will remove all contents of the directory including
 * subdirectories.  The directory will then be removed if it is not root.
 * The read-only attribute for files will be ignored.
 *
 * \note This function should not be used to delete the 8.3 version of
 * a directory that has a long name.  See remove() and rmDir().
 *
 * \return The value one, true, is returned for success and
 * the value zero, false, is returned for failure.
 */
u8 EmuFatFile::rmRfStar(void) {
  rewind();
  while (curPosition_ < fileSize_) {
    EmuFatFile f;

    // remember position
    u16 index = curPosition_/32;

    TDirectoryEntry* p = readDirCache();
    if (!p) return false;

    // done if past last entry
    if (p->name[0] == DIR_NAME_FREE) break;

    // skip empty slot or '.' or '..'
    if (p->name[0] == DIR_NAME_DELETED || p->name[0] == '.') continue;

    // skip if part of long file name or volume label in root
    if (!DIR_IS_FILE_OR_SUBDIR(p)) continue;

    if (!f.open(this, index, EO_READ)) return false;
    if (f.isSubDir()) {
      // recursively delete
      return rmRfStar();
    } else {
      // ignore read-only
      f.flags_ |= EO_WRITE;
      if (!f.remove()) return false;
    }
    // position to next entry if required
    if (curPosition_ != (32*(index + 1))) {
      if (!seekSet(32*(index + 1))) return false;
    }
  }
  // don't try to delete root
  if (isRoot()) return true;
  return rmDir();
}

u8 EmuFatFile::seekSet(u32 pos) {
  // error if file not open or seek past end of file
  if (!isOpen() || pos > fileSize_) return false;

  if (type_ == FAT_FILE_TYPE_ROOT16) {
    curPosition_ = pos;
    return true;
  }
  if (pos == 0) {
    // set position to start of file
    curCluster_ = 0;
    curPosition_ = 0;
    return true;
  }
  // calculate cluster index for cur and new position
  u32 nCur = (curPosition_ - 1) >> (vol_->clusterSizeShift_ + 9);
  u32 nNew = (pos - 1) >> (vol_->clusterSizeShift_ + 9);

  if (nNew < nCur || curPosition_ == 0) {
    // must follow chain from first cluster
    curCluster_ = firstCluster_;
  } else {
    // advance from curPosition
    nNew -= nCur;
  }
  while (nNew--) {
    if (!vol_->fatGet(curCluster_, &curCluster_)) return false;
  }
  curPosition_ = pos;
  return true;
}

//------------------------------------------------------------------------------
/**
 * The sync() call causes all modified data and directory fields
 * to be written to the storage device.
 *
 * \return The value one, true, is returned for success and
 * the value zero, false, is returned for failure.
 * Reasons for failure include a call to sync() before a file has been
 * opened or an I/O error.
 */
u8 EmuFatFile::sync(void) {
  // only allow open files and directories
  if (!isOpen()) return false;

  if (flags_ & F_FILE_DIR_DIRTY) {
    TDirectoryEntry* d = cacheDirEntry(EmuFat::CACHE_FOR_WRITE);
    if (!d) return false;

    // do not set filesize for dir files
    if (!isDir()) d->fileSize = fileSize_;

    // update first cluster fields
    d->firstClusterLow = firstCluster_ & 0XFFFF;
    d->firstClusterHigh = firstCluster_ >> 16;

    // set modify time if user supplied a callback date/time function
    if (dateTime_) {
      dateTime_(&d->lastWriteDate, &d->lastWriteTime);
      d->lastAccessDate = d->lastWriteDate;
    }
    // clear directory dirty
    flags_ &= ~F_FILE_DIR_DIRTY;
  }
  return vol_->dev_->cacheFlush();
}

//------------------------------------------------------------------------------
/**
 * Set a file's timestamps in its directory entry.
 *
 * \param[in] flags Values for \a flags are constructed by a bitwise-inclusive
 * OR of flags from the following list
 *
 * T_ACCESS - Set the file's last access date.
 *
 * T_CREATE - Set the file's creation date and time.
 *
 * T_WRITE - Set the file's last write/modification date and time.
 *
 * \param[in] year Valid range 1980 - 2107 inclusive.
 *
 * \param[in] month Valid range 1 - 12 inclusive.
 *
 * \param[in] day Valid range 1 - 31 inclusive.
 *
 * \param[in] hour Valid range 0 - 23 inclusive.
 *
 * \param[in] minute Valid range 0 - 59 inclusive.
 *
 * \param[in] second Valid range 0 - 59 inclusive
 *
 * \note It is possible to set an invalid date since there is no check for
 * the number of days in a month.
 *
 * \note
 * Modify and access timestamps may be overwritten if a date time callback
 * function has been set by dateTimeCallback().
 *
 * \return The value one, true, is returned for success and
 * the value zero, false, is returned for failure.
 */
u8 EmuFatFile::timestamp(u8 flags, u16 year, u8 month, u8 day, u8 hour, u8 minute, u8 second) {
  if (!isOpen()
    || year < 1980
    || year > 2107
    || month < 1
    || month > 12
    || day < 1
    || day > 31
    || hour > 23
    || minute > 59
    || second > 59) {
      return false;
  }
  TDirectoryEntry* d = cacheDirEntry(EmuFat::CACHE_FOR_WRITE);
  if (!d) return false;

  u16 dirDate = FAT_DATE(year, month, day);
  u16 dirTime = FAT_TIME(hour, minute, second);
  if (flags & T_ACCESS) {
    d->lastAccessDate = dirDate;
  }
  if (flags & T_CREATE) {
    d->creationDate = dirDate;
    d->creationTime = dirTime;
    // seems to be units of 1/100 second not 1/10 as Microsoft states
    d->creationTimeTenths = second & 1 ? 100 : 0;
  }
  if (flags & T_WRITE) {
    d->lastWriteDate = dirDate;
    d->lastWriteTime = dirTime;
  }
  vol_->dev_->cacheSetDirty();
  return sync();
}

//------------------------------------------------------------------------------
/**
 * Truncate a file to a specified length.  The current file position
 * will be maintained if it is less than or equal to \a length otherwise
 * it will be set to end of file.
 *
 * \param[in] length The desired length for the file.
 *
 * \return The value one, true, is returned for success and
 * the value zero, false, is returned for failure.
 * Reasons for failure include file is read only, file is a directory,
 * \a length is greater than the current file size or an I/O error occurs.
 */
u8 EmuFatFile::truncate(u32 length) {
// error if not a normal file or read-only
  if (!isFile() || !(flags_ & EO_WRITE)) return false;

  // error if length is greater than current size
  if (length > fileSize_) return false;

  // fileSize and length are zero - nothing to do
  if (fileSize_ == 0) return true;

  // remember position for seek after truncation
  u32 newPos = curPosition_ > length ? length : curPosition_;

  // position to last cluster in truncated file
  if (!seekSet(length)) return false;

  if (length == 0) {
    // free all clusters
    if (!vol_->freeChain(firstCluster_)) return false;
    firstCluster_ = 0;
  } else {
    u32 toFree;
    if (!vol_->fatGet(curCluster_, &toFree)) return false;

    if (!vol_->isEOC(toFree)) {
      // free extra clusters
      if (!vol_->freeChain(toFree)) return false;

      // current cluster is end of chain
      if (!vol_->fatPutEOC(curCluster_)) return false;
    }
  }
  fileSize_ = length;

  // need to update directory entry
  flags_ |= F_FILE_DIR_DIRTY;

  if (!sync()) return false;

  // set file to correct position
  return seekSet(newPos);
}

/**
 * Write data to an open file.
 *
 * \note Data is moved to the cache but may not be written to the
 * storage device until sync() is called.
 *
 * \param[in] buf Pointer to the location of the data to be written.
 *
 * \param[in] nbyte Number of bytes to write.
 *
 * \return For success write() returns the number of bytes written, always
 * \a nbyte.  If an error occurs, write() returns -1.  Possible errors
 * include write() is called before a file has been opened, write is called
 * for a read-only file, device is full, a corrupt file system or an I/O error.
 *
 */
s32 EmuFatFile::write(const void* buf, u32 nbyte) {
  // convert void* to uint8_t*  -  must be before goto statements
  const u8* src = reinterpret_cast<const u8*>(buf);

  // number of bytes left to write  -  must be before goto statements
  u32 nToWrite = nbyte;

  // error if not a normal file or is read-only
  if (!isFile() || !(flags_ & EO_WRITE)) goto writeErrorReturn;

  // seek to end of file if append flag
  if ((flags_ & EO_APPEND) && curPosition_ != fileSize_) {
    if (!seekEnd()) goto writeErrorReturn;
  }

  while (nToWrite > 0) {
    u8 blockOfCluster = vol_->blockOfCluster(curPosition_);
    u16 blockOffset = curPosition_ & 0X1FF;
    if (blockOfCluster == 0 && blockOffset == 0) {
      // start of new cluster
      if (curCluster_ == 0) {
        if (firstCluster_ == 0) {
          // allocate first cluster of file
          if (!addCluster()) goto writeErrorReturn;
        } else {
          curCluster_ = firstCluster_;
        }
      } else {
        u32 next;
        if (!vol_->fatGet(curCluster_, &next)) return false;
        if (vol_->isEOC(next)) {
          // add cluster if at end of chain
          if (!addCluster()) goto writeErrorReturn;
        } else {
          curCluster_ = next;
        }
      }
    }
    // max space in block
    u32 n = 512 - blockOffset;

    // lesser of space and amount to write
    if (n > nToWrite) n = nToWrite;

    // block for data write
    u32 block = vol_->clusterStartBlock(curCluster_) + blockOfCluster;
    if (n == 512) {
      // full block - don't need to use cache
      // invalidate cache if block is in cache
      if (vol_->dev_->cache_.cacheBlockNumber_ == block) {
        vol_->dev_->cache_.cacheBlockNumber_ = 0XFFFFFFFF;
      }
      if (!vol_->writeBlock(block, src)) goto writeErrorReturn;
      src += 512;
    } else {
      if (blockOffset == 0 && curPosition_ >= fileSize_) {
        // start of new block don't need to read into cache
        if (!vol_->dev_->cacheFlush()) goto writeErrorReturn;
        vol_->dev_->cache_.cacheBlockNumber_ = block;
        vol_->dev_->cacheSetDirty();
      } else {
        // rewrite part of block
        if (!vol_->dev_->cacheRawBlock(block, EmuFat::CACHE_FOR_WRITE)) {
          goto writeErrorReturn;
        }
      }
      u8* dst = vol_->dev_->cache_.cacheBuffer_.data + blockOffset;
      u8* end = dst + n;
      while (dst != end) *dst++ = *src++;
    }
    nToWrite -= n;
    curPosition_ += n;
  }
  if (curPosition_ > fileSize_) {
    // update fileSize and insure sync will update dir entry
    fileSize_ = curPosition_;
    flags_ |= F_FILE_DIR_DIRTY;
  } else if (dateTime_ && nbyte) {
    // insure sync will update modified date and time
    flags_ |= F_FILE_DIR_DIRTY;
  }

  if (flags_ & EO_SYNC) {
    if (!sync()) goto writeErrorReturn;
  }
  return nbyte;

 writeErrorReturn:
  // return for write error
  writeError = true;
  return -1;
}
