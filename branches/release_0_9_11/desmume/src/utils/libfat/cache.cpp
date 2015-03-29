/*
 cache.c
 The cache is not visible to the user. It should be flushed
 when any file is closed or changes are made to the filesystem.

 This cache implements a least-used-page replacement policy. This will
 distribute sectors evenly over the pages, so if less than the maximum
 pages are used at once, they should all eventually remain in the cache.
 This also has the benefit of throwing out old sectors, so as not to keep
 too many stale pages around.

 Copyright (c) 2006 Michael "Chishm" Chisholm

 Redistribution and use in source and binary forms, with or without modification,
 are permitted provided that the following conditions are met:

  1. Redistributions of source code must retain the above copyright notice,
     this list of conditions and the following disclaimer.
  2. Redistributions in binary form must reproduce the above copyright notice,
     this list of conditions and the following disclaimer in the documentation and/or
     other materials provided with the distribution.
  3. The name of the author may not be used to endorse or promote products derived
     from this software without specific prior written permission.

 THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR IMPLIED
 WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY
 AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR BE
 LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#include <string.h>
#include <limits.h>

#include "common.h"
#include "cache.h"
#include "disc.h"

#include "mem_allocate.h"
#include "bit_ops.h"
#include "file_allocation_table.h"

#define CACHE_FREE UINT_MAX

CACHE* _FAT_cache_constructor (unsigned int numberOfPages, unsigned int sectorsPerPage, const DISC_INTERFACE* discInterface, sec_t endOfPartition) {
	CACHE* cache;
	unsigned int i;
	CACHE_ENTRY* cacheEntries;

	if (numberOfPages < 2) {
		numberOfPages = 2;
	}

	if (sectorsPerPage < 8) {
		sectorsPerPage = 8;
	}

	cache = (CACHE*) _FAT_mem_allocate (sizeof(CACHE));
	if (cache == NULL) {
		return NULL;
	}

	cache->disc = discInterface;
	cache->endOfPartition = endOfPartition;
	cache->numberOfPages = numberOfPages;
	cache->sectorsPerPage = sectorsPerPage;


	cacheEntries = (CACHE_ENTRY*) _FAT_mem_allocate ( sizeof(CACHE_ENTRY) * numberOfPages);
	if (cacheEntries == NULL) {
		_FAT_mem_free (cache);
		return NULL;
	}

	for (i = 0; i < numberOfPages; i++) {
		cacheEntries[i].sector = CACHE_FREE;
		cacheEntries[i].count = 0;
		cacheEntries[i].last_access = 0;
		cacheEntries[i].dirty = false;
		cacheEntries[i].cache = (uint8_t*) _FAT_mem_align ( sectorsPerPage * BYTES_PER_READ );
	}

	cache->cacheEntries = cacheEntries;

	return cache;
}

void _FAT_cache_destructor (CACHE* cache) {
	unsigned int i;
	// Clear out cache before destroying it
	_FAT_cache_flush(cache);

	// Free memory in reverse allocation order
	for (i = 0; i < cache->numberOfPages; i++) {
		_FAT_mem_free (cache->cacheEntries[i].cache);
	}
	_FAT_mem_free (cache->cacheEntries);
	_FAT_mem_free (cache);
}


static u32 accessCounter = 0;

static u32 accessTime(){
	accessCounter++;
	return accessCounter;
}


static CACHE_ENTRY* _FAT_cache_getPage(CACHE *cache,sec_t sector)
{
	unsigned int i;
	CACHE_ENTRY* cacheEntries = cache->cacheEntries;
	unsigned int numberOfPages = cache->numberOfPages;
	unsigned int sectorsPerPage = cache->sectorsPerPage;

	bool foundFree = false;
	unsigned int oldUsed = 0;
	unsigned int oldAccess = UINT_MAX;

	for(i=0;i<numberOfPages;i++) {
		if(sector>=cacheEntries[i].sector && sector<(cacheEntries[i].sector + cacheEntries[i].count)) {
			cacheEntries[i].last_access = accessTime();
			return &(cacheEntries[i]);
		}
		
		if(foundFree==false && (cacheEntries[i].sector==CACHE_FREE || cacheEntries[i].last_access<oldAccess)) {
			if(cacheEntries[i].sector==CACHE_FREE) foundFree = true;
			oldUsed = i;
			oldAccess = cacheEntries[i].last_access;
		}
	}

	if(foundFree==false && cacheEntries[oldUsed].dirty==true) {
		if(!_FAT_disc_writeSectors(cache->disc,cacheEntries[oldUsed].sector,cacheEntries[oldUsed].count,cacheEntries[oldUsed].cache)) return NULL;
		cacheEntries[oldUsed].dirty = false;
	}

	sector = (sector/sectorsPerPage)*sectorsPerPage; // align base sector to page size
	sec_t next_page = sector + sectorsPerPage;
	if(next_page > cache->endOfPartition)	next_page = cache->endOfPartition;

	if(!_FAT_disc_readSectors(cache->disc,sector,next_page-sector,cacheEntries[oldUsed].cache)) return NULL;

	cacheEntries[oldUsed].sector = sector;
	cacheEntries[oldUsed].count = next_page-sector;
	cacheEntries[oldUsed].last_access = accessTime();

	return &(cacheEntries[oldUsed]);
}

bool _FAT_cache_readSectors(CACHE *cache,sec_t sector,sec_t numSectors,void *buffer)
{
	sec_t sec;
	sec_t secs_to_read;
	CACHE_ENTRY *entry;
	uint8_t *dest = (uint8_t *)buffer;

	while(numSectors>0) {
		entry = _FAT_cache_getPage(cache,sector);
		if(entry==NULL) return false;

		sec = sector - entry->sector;
		secs_to_read = entry->count - sec;
		if(secs_to_read>numSectors) secs_to_read = numSectors;

		memcpy(dest,entry->cache + (sec*BYTES_PER_READ),(secs_to_read*BYTES_PER_READ));

		dest += (secs_to_read*BYTES_PER_READ);
		sector += secs_to_read;
		numSectors -= secs_to_read;
	}

	return true;
}

/*
Reads some data from a cache page, determined by the sector number
*/
bool _FAT_cache_readPartialSector (CACHE* cache, void* buffer, sec_t sector, unsigned int offset, size_t size) 
{
	sec_t sec;
	CACHE_ENTRY *entry;

	if (offset + size > BYTES_PER_READ) return false;

	entry = _FAT_cache_getPage(cache,sector);
	if(entry==NULL) return false;

	sec = sector - entry->sector;
	memcpy(buffer,entry->cache + ((sec*BYTES_PER_READ) + offset),size);

	return true;
}

bool _FAT_cache_readLittleEndianValue (CACHE* cache, uint32_t *value, sec_t sector, unsigned int offset, int num_bytes) {
  uint8_t buf[4];
  if (!_FAT_cache_readPartialSector(cache, buf, sector, offset, num_bytes)) return false;

  switch(num_bytes) {
  case 1: *value = buf[0]; break;
  case 2: *value = u8array_to_u16(buf,0); break;
  case 4: *value = u8array_to_u32(buf,0); break;
  default: return false;
  }
  return true;
}

/*
Writes some data to a cache page, making sure it is loaded into memory first.
*/
bool _FAT_cache_writePartialSector (CACHE* cache, const void* buffer, sec_t sector, unsigned int offset, size_t size) 
{
	sec_t sec;
	CACHE_ENTRY *entry;

	if (offset + size > BYTES_PER_READ) return false;

	entry = _FAT_cache_getPage(cache,sector);
	if(entry==NULL) return false;

	sec = sector - entry->sector;
	memcpy(entry->cache + ((sec*BYTES_PER_READ) + offset),buffer,size);

	entry->dirty = true;
	return true;
}

bool _FAT_cache_writeLittleEndianValue (CACHE* cache, const uint32_t value, sec_t sector, unsigned int offset, int size) {
  uint8_t buf[4] = {0, 0, 0, 0};

  switch(size) {
  case 1: buf[0] = value; break;
  case 2: u16_to_u8array(buf, 0, value); break;
  case 4: u32_to_u8array(buf, 0, value); break;
  default: return false;
  }

  return _FAT_cache_writePartialSector(cache, buf, sector, offset, size);
}

/*
Writes some data to a cache page, zeroing out the page first
*/
bool _FAT_cache_eraseWritePartialSector (CACHE* cache, const void* buffer, sec_t sector, unsigned int offset, size_t size) 
{
	sec_t sec;
	CACHE_ENTRY *entry;

	if (offset + size > BYTES_PER_READ) return false;

	entry = _FAT_cache_getPage(cache,sector);
	if(entry==NULL) return false;

	sec = sector - entry->sector;
	memset(entry->cache + (sec*BYTES_PER_READ),0,BYTES_PER_READ);
	memcpy(entry->cache + ((sec*BYTES_PER_READ) + offset),buffer,size);

	entry->dirty = true;
	return true;
}


static CACHE_ENTRY* _FAT_cache_findPage(CACHE *cache, sec_t sector, sec_t count) {

	unsigned int i;
	CACHE_ENTRY* cacheEntries = cache->cacheEntries;
	unsigned int numberOfPages = cache->numberOfPages;
	CACHE_ENTRY *entry = NULL;
	sec_t	lowest = UINT_MAX;

	for(i=0;i<numberOfPages;i++) {
		if (cacheEntries[i].sector != CACHE_FREE) {
			bool intersect;
			if (sector > cacheEntries[i].sector) {
				intersect = sector - cacheEntries[i].sector < cacheEntries[i].count;
			} else {
				intersect = cacheEntries[i].sector - sector < count;
			}

			if ( intersect && (cacheEntries[i].sector < lowest)) {
				lowest = cacheEntries[i].sector;
				entry = &cacheEntries[i];
			}
		}
	}

	return entry;
}

bool _FAT_cache_writeSectors (CACHE* cache, sec_t sector, sec_t numSectors, const void* buffer) 
{
	sec_t sec;
	sec_t secs_to_write;
	CACHE_ENTRY* entry;
	const uint8_t *src = (const uint8_t *)buffer;

	while(numSectors>0)
	{
		entry = _FAT_cache_findPage(cache,sector,numSectors);

		if(entry!=NULL) {

			if ( entry->sector > sector) {
				
				secs_to_write = entry->sector - sector;
				
				_FAT_disc_writeSectors(cache->disc,sector,secs_to_write,src);
				src += (secs_to_write*BYTES_PER_READ);
				sector += secs_to_write;
				numSectors -= secs_to_write;
			}
				
			sec = sector - entry->sector;
			secs_to_write = entry->count - sec;

			if(secs_to_write>numSectors) secs_to_write = numSectors;

			memcpy(entry->cache + (sec*BYTES_PER_READ),src,(secs_to_write*BYTES_PER_READ));

			src += (secs_to_write*BYTES_PER_READ);
			sector += secs_to_write;
			numSectors -= secs_to_write;

			entry->dirty = true;
				
		} else {
			_FAT_disc_writeSectors(cache->disc,sector,numSectors,src);
			numSectors=0;
		}
	}
	return true;
}

/*
Flushes all dirty pages to disc, clearing the dirty flag.
*/
bool _FAT_cache_flush (CACHE* cache) {
	unsigned int i;

	for (i = 0; i < cache->numberOfPages; i++) {
		if (cache->cacheEntries[i].dirty) {
			if (!_FAT_disc_writeSectors (cache->disc, cache->cacheEntries[i].sector, cache->cacheEntries[i].count, cache->cacheEntries[i].cache)) {
				return false;
			}
		}
		cache->cacheEntries[i].dirty = false;
	}

	return true;
}

void _FAT_cache_invalidate (CACHE* cache) {
	unsigned int i;
	_FAT_cache_flush(cache);
	for (i = 0; i < cache->numberOfPages; i++) {
		cache->cacheEntries[i].sector = CACHE_FREE;
		cache->cacheEntries[i].last_access = 0;
		cache->cacheEntries[i].count = 0;
		cache->cacheEntries[i].dirty = false;
	}
}
