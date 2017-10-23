/*
	Copyright (C) 2006 yopyop
	Copyright (C) 2006-2007 shash
	Copyright (C) 2008-2017 DeSmuME team

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

#include <string.h>
#include <algorithm>
#include <assert.h>

#include "texcache.h"

#include "./utils/bits.h"
#include "common.h"
#include "debug.h"
#include "gfx3d.h"
#include "MMU.h"
#include "NDSSystem.h"

#ifdef ENABLE_SSE2
#include "./utils/colorspacehandler/colorspacehandler_SSE2.h"
#endif

using std::min;
using std::max;

//only dump this from ogl renderer. for now, softrasterizer creates things in an incompatible pixel format
//#define DEBUG_DUMP_TEXTURE

#if defined(DEBUG_DUMP_TEXTURE) && defined(WIN32)
	#define DO_DEBUG_DUMP_TEXTURE
#endif

#define CONVERT(color) ((TEXCACHEFORMAT == TexFormat_32bpp)?(COLOR555TO8888_OPAQUE(color)):COLOR555TO6665_OPAQUE(color))

//This class represents a number of regions of memory which should be viewed as contiguous
class MemSpan
{
public:
	static const int MAXSIZE = 17; //max size for textures: 1024*1024*2 bytes / 128*1024 banks + 1 for wraparound

	MemSpan() 
		: numItems(0), size(0)
	{}

	int numItems;

	struct Item {
		u32 start;
		u32 len;
		u8* ptr;
		u32 ofs; //offset within the memspan
	} items[MAXSIZE];

	int size;

	//this MemSpan shall be considered the first argument to a standard memcmp
	//the length shall be as specified in this MemSpan, unless you specify otherwise
	int memcmp(void* buf2, int cmpSize=-1)
	{
		if(cmpSize==-1) cmpSize = this->size;
		cmpSize = min(this->size,cmpSize);
		for(int i=0;i<numItems;i++)
		{
			Item &item = items[i];
			int todo = min((int)item.len,cmpSize);
			cmpSize -= todo;
			int temp = ::memcmp(item.ptr,((u8*)buf2)+item.ofs,todo);
			if(temp) return temp;
			if(cmpSize == 0) break;
		}
		return 0;
	}

	//TODO - get rid of duplication between these two methods.

	//dumps the memspan to the specified buffer
	//you may set size to limit the size to be copied
	int dump(void* buf, int dumpSize=-1) const
	{
		if(dumpSize==-1) dumpSize = this->size;
		dumpSize = min(this->size,dumpSize);
		u8* bufptr = (u8*)buf;
		int done = 0;
		for(int i=0;i<numItems;i++)
		{
			Item item = items[i];
			int todo = min((int)item.len,dumpSize);
			dumpSize -= todo;
			done += todo;
			memcpy(bufptr,item.ptr,todo);
			bufptr += todo;
			if(dumpSize==0) return done;
		}
		return done;
	}

	// this function does the same than dump
	// but works for both little and big endian
	// when buf is an u16 array
	int dump16(void* buf, int dumpSize=-1) const
	{
		if(dumpSize==-1) dumpSize = this->size;
		dumpSize = min(this->size,dumpSize);
		u16* bufptr = (u16*)buf;
		int done = 0;
		for(int i=0;i<numItems;i++)
		{
			Item item = items[i];
			u8 * src = (u8 *) item.ptr;
			int todo = min((int)item.len,dumpSize);
			dumpSize -= todo;
			done += todo;
			for(int j = 0;j < todo / 2;j++)
			{
				u16 tmp;
				tmp = *src++;
				tmp |= *(src++) << 8;
				*bufptr++ = tmp;
			}
			if(dumpSize==0) return done;
		}
		return done;
	}
};

//creates a MemSpan in texture memory
static MemSpan MemSpan_TexMem(u32 ofs, u32 len) 
{
	MemSpan ret;
	ret.size = len;
	u32 currofs = 0;
	while(len) {
		MemSpan::Item &curr = ret.items[ret.numItems++];
		curr.start = ofs&0x1FFFF;
		u32 slot = (ofs>>17)&3; //slots will wrap around
		curr.len = min(len,0x20000-curr.start);
		curr.ofs = currofs;
		len -= curr.len;
		ofs += curr.len;
		currofs += curr.len;
		u8* ptr = MMU.texInfo.textureSlotAddr[slot];
		
		if (ptr == MMU.blank_memory && !GPU->GetEngineMain()->IsMasterBrightMaxOrMin()) {
			PROGINFO("Tried to reference unmapped texture memory: slot %d\n",slot);
		}
		curr.ptr = ptr + curr.start;
	}
	return ret;
}

//creates a MemSpan in texture palette memory
static MemSpan MemSpan_TexPalette(u32 ofs, u32 len, bool silent) 
{
	MemSpan ret;
	ret.size = len;
	u32 currofs = 0;
	while(len) {
		MemSpan::Item &curr = ret.items[ret.numItems++];
		curr.start = ofs&0x3FFF;
		u32 slot = (ofs>>14)&7; //this masks to 8 slots, but there are really only 6
		if(slot>5 && !silent) {
			PROGINFO("Texture palette overruns texture memory. Wrapping at palette slot 0.\n");
			slot -= 5;
		}
		curr.len = min(len,0x4000-curr.start);
		curr.ofs = currofs;
		len -= curr.len;
		ofs += curr.len;
		//if(len != 0) 
			//here is an actual test case of bank spanning
		currofs += curr.len;
		u8* ptr = MMU.texInfo.texPalSlot[slot];
		
		if(ptr == MMU.blank_memory && !GPU->GetEngineMain()->IsMasterBrightMaxOrMin() && !silent) {
			PROGINFO("Tried to reference unmapped texture palette memory: 16k slot #%d\n",slot);
		}
		curr.ptr = ptr + curr.start;
	}
	return ret;
}

static bool TextureLRUCompare(TextureStore *tex1, TextureStore *tex2)
{
	const size_t cacheAge1 = tex1->GetCacheAge();
	const size_t cacheAge2 = tex2->GetCacheAge();
	
	if (cacheAge1 == cacheAge2)
	{
		return ( tex1->GetCacheUseCount() > tex2->GetCacheUseCount() );
	}
	
	return (cacheAge1 < cacheAge2);
}

TextureCache texCache;

TextureCache::TextureCache()
{
	_texCacheMap.clear();
	_texCacheList.reserve(4096);
	_actualCacheSize = 0;
	_cacheSizeThreshold = TEXCACHE_DEFAULT_THRESHOLD;
	memset(_paletteDump, 0, sizeof(_paletteDump));
}

size_t TextureCache::GetActualCacheSize() const
{
	return this->_actualCacheSize;
}

size_t TextureCache::GetCacheSizeThreshold() const
{
	return this->_cacheSizeThreshold;
}

void TextureCache::SetCacheSizeThreshold(size_t newThreshold)
{
	this->_cacheSizeThreshold = newThreshold;
}

void TextureCache::Invalidate()
{
	//check whether the palette memory changed
	//TODO - we should handle this instead by setting dirty flags in the vram memory mapping and noting whether palette memory was dirty.
	//but this will work for now
	MemSpan mspal = MemSpan_TexPalette(0, PALETTE_DUMP_SIZE, true);
	const bool paletteDirty = mspal.memcmp(this->_paletteDump);
	if (paletteDirty)
	{
		mspal.dump(this->_paletteDump);
	}
	
	for (TextureCacheMap::iterator it(this->_texCacheMap.begin()); it != this->_texCacheMap.end(); ++it)
	{
		it->second->SetSuspectedInvalid();
		
		//when the palette changes, we assume all 4x4 textures are dirty.
		//this is because each 4x4 item doesnt carry along with it a copy of the entire palette, for verification
		//instead, we just use the one paletteDump for verifying of all 4x4 textures; and if paletteDirty is set, verification has failed
		if( (it->second->GetPackFormat() == TEXMODE_4X4) && paletteDirty )
		{
			it->second->SetAssumedInvalid();
		}
	}
}

void TextureCache::Evict()
{
	//debug print
	//printf("%d %d/%d\n",index.size(),cache_size/1024,target/1024);
	
	//dont do anything unless we're over the target
	if (this->_actualCacheSize <= this->_cacheSizeThreshold)
	{
		for (size_t i = 0; i < this->_texCacheList.size(); i++)
		{
			this->_texCacheList[i]->IncreaseCacheAge(1);
		}
		
		return;
	}
	
	//aim at cutting the cache to half of the max size
	size_t targetCacheSize = this->_cacheSizeThreshold / 2;
	
	// Sort the textures in cache by age and usage count. Textures that we want to keep in
	// cache are placed in the front of the list, while textures we want to evict are sorted
	// to the back of the list.
	std::sort(this->_texCacheList.begin(), this->_texCacheList.end(), &TextureLRUCompare);
	
	while (this->_actualCacheSize > targetCacheSize)
	{
		if (this->_texCacheMap.size() == 0) break; //just in case.. doesnt seem possible, cache_size wouldve been 0
		
		TextureStore *item = this->_texCacheList.back();
		this->Remove(item);
		this->_texCacheList.pop_back();
		
		//printf("evicting! totalsize:%d\n",cache_size);
		delete item;
	}
	
	for (size_t i = 0; i < this->_texCacheList.size(); i++)
	{
		this->_texCacheList[i]->IncreaseCacheAge(1);
	}
}

void TextureCache::Reset()
{
	for (size_t i = 0; i < this->_texCacheList.size(); i++)
	{
		delete this->_texCacheList[i];
	}
	
	this->_texCacheMap.clear();
	this->_texCacheList.clear();
	this->_actualCacheSize = 0;
	memset(this->_paletteDump, 0, sizeof(this->_paletteDump));
}

void TextureCache::ForceReloadAllTextures()
{
	for (TextureCacheMap::iterator it(this->_texCacheMap.begin()); it != this->_texCacheMap.end(); ++it)
	{
		it->second->SetLoadNeeded();
	}
}

TextureStore* TextureCache::GetTexture(TEXIMAGE_PARAM texAttributes, u32 palAttributes)
{
	TextureStore *theTexture = NULL;
	const TextureCacheKey key = TextureCache::GenerateKey(texAttributes, palAttributes);
	const TextureCacheMap::iterator cachedTexture = this->_texCacheMap.find(key);
	
	if (cachedTexture == this->_texCacheMap.end())
	{
		return theTexture;
	}
	else
	{
		theTexture = cachedTexture->second;
		
		if (theTexture->IsAssumedInvalid())
		{
			theTexture->Update();
		}
		else if (theTexture->IsSuspectedInvalid())
		{
			theTexture->VRAMCompareAndUpdate();
		}
	}
	
	return theTexture;
}

void TextureCache::Add(TextureStore *texItem)
{
	const TextureCacheKey key = texItem->GetCacheKey();
	this->_texCacheMap[key] = texItem;
	this->_texCacheList.push_back(texItem);
	this->_actualCacheSize += texItem->GetCacheSize();
	//printf("allocating: up to %d with %d items\n", this->cache_size, this->cacheTable.size());
}

void TextureCache::Remove(TextureStore *texItem)
{
	const TextureCacheKey key = texItem->GetCacheKey();
	this->_texCacheMap.erase(key);
	this->_actualCacheSize -= texItem->GetCacheSize();
}

TextureCacheKey TextureCache::GenerateKey(const TEXIMAGE_PARAM texAttributes, const u32 palAttributes)
{
	// Since the repeat, flip, and coordinate transformation modes are render settings
	// and not data settings, we can mask out those bits to help reduce duplicate entries.
	return (TextureCacheKey)( ((u64)palAttributes << 32) | (u64)(texAttributes.value & 0x3FF0FFFF) );
}

TextureStore::TextureStore()
{
	_textureAttributes.value = 0;
	_paletteAttributes = 0;
	_cacheKey = 0;
	
	_sizeS = 0;
	_sizeT = 0;
	_isPalZeroTransparent = false;
	
	_packFormat = TEXMODE_NONE;
	_packAddress = 0;
	_packSize = 0;
	_packData = NULL;
	
	_paletteAddress = 0;
	_paletteSize = 0;
	_paletteColorTable = NULL;
	
	_packIndexAddress = 0;
	_packIndexSize = 0;
	_packIndexData = NULL;
	_packSizeFirstSlot = 0;
	
	_packTotalSize = 0;
	
	_suspectedInvalid = false;
	_assumedInvalid = false;
	_isLoadNeeded = false;
	
	_cacheSize = 0;
	_cacheAge = 0;
	_cacheUsageCount = 0;
}

TextureStore::TextureStore(const TEXIMAGE_PARAM texAttributes, const u32 palAttributes)
{
	//for each texformat, multiplier from numtexels to numbytes (fixed point 30.2)
	static const u32 texSizes[] = {0, 4, 1, 2, 4, 1, 4, 8};
	
	//for each texformat, number of palette entries
	static const u32 paletteSizeList[] = {0, 32, 4, 16, 256, 0, 8, 0};
	
	_textureAttributes = texAttributes;
	_paletteAttributes = palAttributes;
	_cacheKey = TextureCache::GenerateKey(texAttributes, palAttributes);
	
	_sizeS = (8 << texAttributes.SizeShiftS);
	_sizeT = (8 << texAttributes.SizeShiftT);
	
	_packFormat = (NDSTextureFormat)(texAttributes.PackedFormat);
	_packAddress = texAttributes.VRAMOffset << 3;
	_packSize = (_sizeS * _sizeT * texSizes[_packFormat]) >> 2; //shifted because the texSizes multiplier is fixed point
	
	if ( (_packFormat == TEXMODE_I2) || (_packFormat == TEXMODE_I4) || (_packFormat == TEXMODE_I8) )
	{
		_isPalZeroTransparent = (texAttributes.KeyColor0_Enable != 0);
	}
	else
	{
		_isPalZeroTransparent = false;
	}
	
	_paletteAddress = (_packFormat == TEXMODE_I2) ? palAttributes << 3 : palAttributes << 4;
	_paletteSize = paletteSizeList[_packFormat] * sizeof(u16);
	
	if (_packFormat == TEXMODE_4X4)
	{
		const u32 indexBase = ((texAttributes.VRAMOffset & 0xC000) == 0x8000) ? 0x30000 : 0x20000;
		const u32 indexOffset = (texAttributes.VRAMOffset & 0x3FFF) << 2;
		_packIndexAddress = indexBase + indexOffset;
		_packIndexSize = (_sizeS * _sizeT) >> 3;
		
		_packTotalSize = _packSize + _packIndexSize + _paletteSize;
		
		_packData = (u8 *)malloc_alignedCacheLine(_packTotalSize);
		_packIndexData = _packData + _packSize;
		_paletteColorTable = (u16 *)(_packData + _packSize + _packIndexSize);
		
		MemSpan currentPackedTexIndexMS = MemSpan_TexMem(_packIndexAddress, _packIndexSize);
		currentPackedTexIndexMS.dump(_packIndexData, _packIndexSize);
	}
	else
	{
		_packIndexAddress = 0;
		_packIndexSize = 0;
		_packIndexData = NULL;
		
		_packTotalSize = _packSize + _paletteSize;
		
		_packData = (u8 *)malloc_alignedCacheLine(_packTotalSize);
		_packIndexData = NULL;
		_paletteColorTable = (u16 *)(_packData + _packSize);
	}
	
	_workingData = (u8 *)malloc_alignedCacheLine(_packTotalSize);
	
	if (_paletteSize > 0)
	{
		MemSpan currentPaletteMS = MemSpan_TexPalette(_paletteAddress, _paletteSize, false);
		
#ifdef MSB_FIRST
		currentPaletteMS.dump16(_paletteColorTable);
#else
		currentPaletteMS.dump(_paletteColorTable);
#endif
	}
	else
	{
		_paletteColorTable = NULL;
	}
	
	MemSpan currentPackedTexDataMS = MemSpan_TexMem(_packAddress, _packSize);
	currentPackedTexDataMS.dump(_packData);
	_packSizeFirstSlot = currentPackedTexDataMS.items[0].len;
	
	_suspectedInvalid = false;
	_assumedInvalid = false;
	_isLoadNeeded = true;
	
	_cacheSize = _packTotalSize;
	_cacheAge = 0;
	_cacheUsageCount = 0;
}

TextureStore::~TextureStore()
{
	free_aligned(this->_workingData);
	free_aligned(this->_packData);
}

TEXIMAGE_PARAM TextureStore::GetTextureAttributes() const
{
	return this->_textureAttributes;
}

u32 TextureStore::GetPaletteAttributes() const
{
	return this->_paletteAttributes;
}

u32 TextureStore::GetWidth() const
{
	return this->_sizeS;
}

u32 TextureStore::GetHeight() const
{
	return this->_sizeT;
}

bool TextureStore::IsPalZeroTransparent() const
{
	return this->_isPalZeroTransparent;
}

NDSTextureFormat TextureStore::GetPackFormat() const
{
	return this->_packFormat;
}

u32 TextureStore::GetPackAddress() const
{
	return this->_packAddress;
}

u32 TextureStore::GetPackSize() const
{
	return this->_packSize;
}

u8* TextureStore::GetPackData()
{
	return this->_packData;
}

u32 TextureStore::GetPaletteAddress() const
{
	return this->_paletteAddress;
}

u32 TextureStore::GetPaletteSize() const
{
	return this->_paletteSize;
}

u16* TextureStore::GetPaletteColorTable() const
{
	return this->_paletteColorTable;
}

u32 TextureStore::GetPackIndexAddress() const
{
	return this->_packIndexAddress;
}

u32 TextureStore::GetPackIndexSize() const
{
	return this->_packIndexSize;
}

u8* TextureStore::GetPackIndexData()
{
	return this->_packIndexData;
}

void TextureStore::SetTextureData(const MemSpan &packedData, const MemSpan &packedIndexData)
{
	//dump texture and 4x4 index data for cache keying
	this->_packSizeFirstSlot = packedData.items[0].len;
	
	packedData.dump(this->_packData);
	
	if (this->_packFormat == TEXMODE_4X4)
	{
		packedIndexData.dump(this->_packIndexData, this->_packIndexSize);
	}
}

void TextureStore::SetTexturePalette(const MemSpan &packedPalette)
{
	if (this->_paletteSize > 0)
	{
#ifdef MSB_FIRST
		packedPalette.dump16(this->_paletteColorTable);
#else
		packedPalette.dump(this->_paletteColorTable);
#endif
	}
}

void TextureStore::SetTexturePalette(const u16 *paletteBuffer)
{
	if (this->_paletteSize > 0)
	{
		memcpy(this->_paletteColorTable, paletteBuffer, this->_paletteSize);
	}
}

size_t TextureStore::GetUnpackSizeUsingFormat(const TextureStoreUnpackFormat texCacheFormat) const
{
	return (this->_sizeS * this->_sizeT * sizeof(u32));
}

template <TextureStoreUnpackFormat TEXCACHEFORMAT>
void TextureStore::Unpack(u32 *unpackBuffer)
{
	// Whenever a 1-bit alpha or no-alpha texture is unpacked (this means any texture
	// format that is not A3I5 or A5I3), set all transparent pixels to 0 so that 3D
	// renderers can assume that the transparent color is 0 during texture sampling.
	
	switch (this->_packFormat)
	{
		case TEXMODE_A3I5:
			NDSTextureUnpackA3I5<TEXCACHEFORMAT>(this->_packSize, this->_packData, this->_paletteColorTable, unpackBuffer);
			break;
			
		case TEXMODE_I2:
			NDSTextureUnpackI2<TEXCACHEFORMAT>(this->_packSize, this->_packData, this->_paletteColorTable, this->_isPalZeroTransparent, unpackBuffer);
			break;
			
		case TEXMODE_I4:
			NDSTextureUnpackI4<TEXCACHEFORMAT>(this->_packSize, this->_packData, this->_paletteColorTable, this->_isPalZeroTransparent, unpackBuffer);
			break;
			
		case TEXMODE_I8:
			NDSTextureUnpackI8<TEXCACHEFORMAT>(this->_packSize, this->_packData, this->_paletteColorTable, this->_isPalZeroTransparent, unpackBuffer);
			break;
			
		case TEXMODE_4X4:
		{
			if (this->_packSize > this->_packSizeFirstSlot)
			{
				PROGINFO("Your 4x4 texture has overrun its texture slot.\n");
			}
			
			NDSTextureUnpack4x4<TEXCACHEFORMAT>(this->_packSizeFirstSlot, (u32 *)this->_packData, (u16 *)this->_packIndexData, this->_paletteAddress, this->_sizeS, this->_sizeT, unpackBuffer);
			break;
		}
			
		case TEXMODE_A5I3:
			NDSTextureUnpackA5I3<TEXCACHEFORMAT>(this->_packSize, this->_packData, this->_paletteColorTable, unpackBuffer);
			break;
			
		case TEXMODE_16BPP:
			NDSTextureUnpackDirect16Bit<TEXCACHEFORMAT>(this->_packSize, (u16 *)this->_packData, unpackBuffer);
			break;
			
		default:
			break;
	}
	
#ifdef DO_DEBUG_DUMP_TEXTURE
	this->DebugDump();
#endif
}

void TextureStore::Load(void *targetBuffer)
{
	this->Unpack<TexFormat_32bpp>((u32 *)targetBuffer);
	this->_isLoadNeeded = false;
}

bool TextureStore::IsSuspectedInvalid() const
{
	return this->_suspectedInvalid;
}

void TextureStore::SetSuspectedInvalid()
{
	this->_suspectedInvalid = true;
}

bool TextureStore::IsAssumedInvalid() const
{
	return this->_assumedInvalid;
}

void TextureStore::SetAssumedInvalid()
{
	this->_assumedInvalid = true;
}

void TextureStore::SetLoadNeeded()
{
	this->_isLoadNeeded = true;
}

bool TextureStore::IsLoadNeeded() const
{
	return this->_isLoadNeeded;
}

TextureCacheKey TextureStore::GetCacheKey() const
{
	return this->_cacheKey;
}

size_t TextureStore::GetCacheSize() const
{
	return this->_cacheSize;
}

void TextureStore::SetCacheSize(size_t cacheSize)
{
	this->_cacheSize = cacheSize;
}

size_t TextureStore::GetCacheAge() const
{
	return this->_cacheAge;
}

void TextureStore::IncreaseCacheAge(const size_t ageAmount)
{
	this->_cacheAge += ageAmount;
}

void TextureStore::ResetCacheAge()
{
	this->_cacheAge = 0;
}

size_t TextureStore::GetCacheUseCount() const
{
	return this->_cacheUsageCount;
}

void TextureStore::IncreaseCacheUsageCount(const size_t usageCount)
{
	this->_cacheUsageCount += usageCount;
}

void TextureStore::ResetCacheUsageCount()
{
	this->_cacheUsageCount = 0;
}

void TextureStore::Update()
{
	MemSpan currentPaletteMS = MemSpan_TexPalette(this->_paletteAddress, this->_paletteSize, false);
	MemSpan currentPackedTexDataMS = MemSpan_TexMem(this->_packAddress, this->_packSize);
	MemSpan currentPackedTexIndexMS;
	
	if (this->_packFormat == TEXMODE_4X4)
	{
		currentPackedTexIndexMS = MemSpan_TexMem(this->_packIndexAddress, this->_packIndexSize);
	}
	
	this->SetTextureData(currentPackedTexDataMS, currentPackedTexIndexMS);
	this->SetTexturePalette(currentPaletteMS);
	
	this->_assumedInvalid = false;
	this->_suspectedInvalid = false;
	this->_isLoadNeeded = true;
}

void TextureStore::VRAMCompareAndUpdate()
{
	MemSpan currentPaletteMS = MemSpan_TexPalette(this->_paletteAddress, this->_paletteSize, false);
	MemSpan currentPackedTexDataMS = MemSpan_TexMem(this->_packAddress, this->_packSize);
	MemSpan currentPackedTexIndexMS;
	
	currentPackedTexDataMS.dump(this->_workingData);
	this->_packSizeFirstSlot = currentPackedTexDataMS.items[0].len;
	
	if (this->_packFormat == TEXMODE_4X4)
	{
		currentPackedTexIndexMS = MemSpan_TexMem(this->_packIndexAddress, this->_packIndexSize);
		currentPackedTexIndexMS.dump(this->_workingData + this->_packSize);
	}
	
#ifdef MSB_FIRST
	currentPaletteMS.dump16(this->_workingData + this->_packSize + this->_packIndexSize);
#else
	currentPaletteMS.dump(this->_workingData + this->_packSize + this->_packIndexSize);
#endif
	
	// Compare the texture's packed data with what's being read from VRAM.
	//
	// Note that we are considering 4x4 textures to have a palette size of 0.
	// They really have a potentially HUGE palette, too big for us to handle
	// like a normal palette, so they go through a different system.
	if (memcmp(this->_packData, this->_workingData, this->_packTotalSize))
	{
		// If the packed data is different from VRAM, then swap pointers with
		// the working buffer and flag this texture for reload.
		u8 *tempDataPtr = this->_packData;
		
		this->_packData = this->_workingData;
		
		if (this->_packIndexSize == 0)
		{
			this->_packIndexData = NULL;
			this->_paletteColorTable = (u16 *)(this->_packData + this->_packSize);
		}
		else
		{
			this->_packIndexData = this->_packData + this->_packSize;
			this->_paletteColorTable = (u16 *)(this->_packData + this->_packSize + this->_packIndexSize);
		}
		
		this->_workingData = tempDataPtr;
		this->_isLoadNeeded = true;
	}
	
	this->_assumedInvalid = false;
	this->_suspectedInvalid = false;
}

#ifdef DO_DEBUG_DUMP_TEXTURE
void TextureStore::DebugDump()
{
	static int ctr=0;
	char fname[100];
	sprintf(fname,"c:\\dump\\%d.bmp", ctr);
	ctr++;
	
	NDS_WriteBMP_32bppBuffer(this->sizeX, this->sizeY, this->unpackData, fname);
}
#endif

template <TextureStoreUnpackFormat TEXCACHEFORMAT>
void NDSTextureUnpackI2(const size_t srcSize, const u8 *__restrict srcData, const u16 *__restrict srcPal, const bool isPalZeroTransparent, u32 *__restrict dstBuffer)
{
#ifdef ENABLE_SSSE3
	const __m128i pal_vec128 = _mm_loadl_epi64((__m128i *)srcPal);
#endif
	if (isPalZeroTransparent)
	{
#ifdef ENABLE_SSSE3
		for (size_t i = 0; i < srcSize; i+=4, srcData+=4, dstBuffer+=16)
		{
			__m128i idx = _mm_set_epi32(0, 0, 0, *(u32 *)srcData);
			idx = _mm_unpacklo_epi8(idx, idx);
			idx = _mm_unpacklo_epi8(idx, idx);
			idx = _mm_or_si128( _mm_or_si128( _mm_or_si128( _mm_and_si128(idx, _mm_set1_epi32(0x00000003)), _mm_and_si128(_mm_srli_epi32(idx, 2), _mm_set1_epi32(0x00000300)) ), _mm_and_si128(_mm_srli_epi32(idx, 4), _mm_set1_epi32(0x00030000)) ), _mm_and_si128(_mm_srli_epi32(idx, 6), _mm_set1_epi32(0x03000000)) );
			idx = _mm_slli_epi16(idx, 1);
			
			__m128i idx0 = _mm_add_epi8( _mm_unpacklo_epi8(idx, idx), _mm_set1_epi16(0x0100) );
			__m128i idx1 = _mm_add_epi8( _mm_unpackhi_epi8(idx, idx), _mm_set1_epi16(0x0100) );
			
			const __m128i palColor0 = _mm_shuffle_epi8(pal_vec128, idx0);
			const __m128i palColor1 = _mm_shuffle_epi8(pal_vec128, idx1);
			
			__m128i convertedColor[4];
			
			if (TEXCACHEFORMAT == TexFormat_15bpp)
			{
				ColorspaceConvert555To6665Opaque_SSE2<false>(palColor0, convertedColor[0], convertedColor[1]);
				ColorspaceConvert555To6665Opaque_SSE2<false>(palColor1, convertedColor[2], convertedColor[3]);
			}
			else
			{
				ColorspaceConvert555To8888Opaque_SSE2<false>(palColor0, convertedColor[0], convertedColor[1]);
				ColorspaceConvert555To8888Opaque_SSE2<false>(palColor1, convertedColor[2], convertedColor[3]);
			}
			
			// Set converted colors to 0 if the palette index is 0.
			idx0 = _mm_cmpeq_epi16(idx0, _mm_set1_epi16(0x0100));
			idx1 = _mm_cmpeq_epi16(idx1, _mm_set1_epi16(0x0100));
			convertedColor[0] = _mm_andnot_si128(_mm_unpacklo_epi16(idx0, idx0), convertedColor[0]);
			convertedColor[1] = _mm_andnot_si128(_mm_unpackhi_epi16(idx0, idx0), convertedColor[1]);
			convertedColor[2] = _mm_andnot_si128(_mm_unpacklo_epi16(idx1, idx1), convertedColor[2]);
			convertedColor[3] = _mm_andnot_si128(_mm_unpackhi_epi16(idx1, idx1), convertedColor[3]);
			
			_mm_store_si128((__m128i *)(dstBuffer +  0), convertedColor[0]);
			_mm_store_si128((__m128i *)(dstBuffer +  4), convertedColor[1]);
			_mm_store_si128((__m128i *)(dstBuffer +  8), convertedColor[2]);
			_mm_store_si128((__m128i *)(dstBuffer + 12), convertedColor[3]);
		}
#else
		for (size_t i = 0; i < srcSize; i++, srcData++)
		{
			u8 idx;
			
			idx =  *srcData       & 0x03;
			*dstBuffer++ = (idx == 0) ? 0 : CONVERT(srcPal[idx] & 0x7FFF);
			
			idx = (*srcData >> 2) & 0x03;
			*dstBuffer++ = (idx == 0) ? 0 : CONVERT(srcPal[idx] & 0x7FFF);
			
			idx = (*srcData >> 4) & 0x03;
			*dstBuffer++ = (idx == 0) ? 0 : CONVERT(srcPal[idx] & 0x7FFF);
			
			idx = (*srcData >> 6) & 0x03;
			*dstBuffer++ = (idx == 0) ? 0 : CONVERT(srcPal[idx] & 0x7FFF);
		}
#endif
	}
	else
	{
#ifdef ENABLE_SSSE3
		for (size_t i = 0; i < srcSize; i+=4, srcData+=4, dstBuffer+=16)
		{
			__m128i idx = _mm_set_epi32(0, 0, 0, *(u32 *)srcData);
			idx = _mm_unpacklo_epi8(idx, idx);
			idx = _mm_unpacklo_epi8(idx, idx);
			idx = _mm_or_si128( _mm_or_si128( _mm_or_si128( _mm_and_si128(idx, _mm_set1_epi32(0x00000003)), _mm_and_si128(_mm_srli_epi32(idx, 2), _mm_set1_epi32(0x00000300)) ), _mm_and_si128(_mm_srli_epi32(idx, 4), _mm_set1_epi32(0x00030000)) ), _mm_and_si128(_mm_srli_epi32(idx, 6), _mm_set1_epi32(0x03000000)) );
			idx = _mm_slli_epi16(idx, 1);
			
			const __m128i idx0 = _mm_add_epi8( _mm_unpacklo_epi8(idx, idx), _mm_set1_epi16(0x0100) );
			const __m128i idx1 = _mm_add_epi8( _mm_unpackhi_epi8(idx, idx), _mm_set1_epi16(0x0100) );
			
			const __m128i palColor0 = _mm_shuffle_epi8(pal_vec128, idx0);
			const __m128i palColor1 = _mm_shuffle_epi8(pal_vec128, idx1);
			
			__m128i convertedColor[4];
			
			if (TEXCACHEFORMAT == TexFormat_15bpp)
			{
				ColorspaceConvert555To6665Opaque_SSE2<false>(palColor0, convertedColor[0], convertedColor[1]);
				ColorspaceConvert555To6665Opaque_SSE2<false>(palColor1, convertedColor[2], convertedColor[3]);
			}
			else
			{
				ColorspaceConvert555To8888Opaque_SSE2<false>(palColor0, convertedColor[0], convertedColor[1]);
				ColorspaceConvert555To8888Opaque_SSE2<false>(palColor1, convertedColor[2], convertedColor[3]);
			}
			
			_mm_store_si128((__m128i *)(dstBuffer +  0), convertedColor[0]);
			_mm_store_si128((__m128i *)(dstBuffer +  4), convertedColor[1]);
			_mm_store_si128((__m128i *)(dstBuffer +  8), convertedColor[2]);
			_mm_store_si128((__m128i *)(dstBuffer + 12), convertedColor[3]);
		}
#else
		for (size_t i = 0; i < srcSize; i++, srcData++)
		{
			*dstBuffer++ = CONVERT(srcPal[ *srcData       & 0x03] & 0x7FFF);
			*dstBuffer++ = CONVERT(srcPal[(*srcData >> 2) & 0x03] & 0x7FFF);
			*dstBuffer++ = CONVERT(srcPal[(*srcData >> 4) & 0x03] & 0x7FFF);
			*dstBuffer++ = CONVERT(srcPal[(*srcData >> 6) & 0x03] & 0x7FFF);
		}
#endif
	}
}

template <TextureStoreUnpackFormat TEXCACHEFORMAT>
void NDSTextureUnpackI4(const size_t srcSize, const u8 *__restrict srcData, const u16 *__restrict srcPal, const bool isPalZeroTransparent, u32 *__restrict dstBuffer)
{
#ifdef ENABLE_SSSE3
	const __m128i palLo = _mm_load_si128((__m128i *)srcPal + 0);
	const __m128i palHi = _mm_load_si128((__m128i *)srcPal + 1);
#endif
	if (isPalZeroTransparent)
	{
#ifdef ENABLE_SSSE3
		for (size_t i = 0; i < srcSize; i+=8, srcData+=8, dstBuffer+=16)
		{
			__m128i idx = _mm_loadl_epi64((__m128i *)srcData);
			idx = _mm_unpacklo_epi8(idx, idx);
			idx = _mm_or_si128( _mm_and_si128(idx, _mm_set1_epi16(0x000F)), _mm_and_si128(_mm_srli_epi16(idx, 4), _mm_set1_epi16(0x0F00)) );
			idx = _mm_slli_epi16(idx, 1);
			
			__m128i idx0 = _mm_add_epi8( _mm_unpacklo_epi8(idx, idx), _mm_set1_epi16(0x0100) );
			__m128i idx1 = _mm_add_epi8( _mm_unpackhi_epi8(idx, idx), _mm_set1_epi16(0x0100) );
			
			const __m128i palMask = _mm_cmpeq_epi8( _mm_and_si128(idx, _mm_set1_epi8(0x10)), _mm_setzero_si128() );
			const __m128i palColor0A = _mm_shuffle_epi8(palLo, idx0);
			const __m128i palColor0B = _mm_shuffle_epi8(palHi, idx0);
			const __m128i palColor1A = _mm_shuffle_epi8(palLo, idx1);
			const __m128i palColor1B = _mm_shuffle_epi8(palHi, idx1);
			
			const __m128i palColor0 = _mm_blendv_epi8( palColor0B, palColor0A, _mm_unpacklo_epi8(palMask, palMask) );
			const __m128i palColor1 = _mm_blendv_epi8( palColor1B, palColor1A, _mm_unpackhi_epi8(palMask, palMask) );
			
			__m128i convertedColor[4];
			
			if (TEXCACHEFORMAT == TexFormat_15bpp)
			{
				ColorspaceConvert555To6665Opaque_SSE2<false>(palColor0, convertedColor[0], convertedColor[1]);
				ColorspaceConvert555To6665Opaque_SSE2<false>(palColor1, convertedColor[2], convertedColor[3]);
			}
			else
			{
				ColorspaceConvert555To8888Opaque_SSE2<false>(palColor0, convertedColor[0], convertedColor[1]);
				ColorspaceConvert555To8888Opaque_SSE2<false>(palColor1, convertedColor[2], convertedColor[3]);
			}
			
			// Set converted colors to 0 if the palette index is 0.
			idx0 = _mm_cmpeq_epi16(idx0, _mm_set1_epi16(0x0100));
			idx1 = _mm_cmpeq_epi16(idx1, _mm_set1_epi16(0x0100));
			convertedColor[0] = _mm_andnot_si128(_mm_unpacklo_epi16(idx0, idx0), convertedColor[0]);
			convertedColor[1] = _mm_andnot_si128(_mm_unpackhi_epi16(idx0, idx0), convertedColor[1]);
			convertedColor[2] = _mm_andnot_si128(_mm_unpacklo_epi16(idx1, idx1), convertedColor[2]);
			convertedColor[3] = _mm_andnot_si128(_mm_unpackhi_epi16(idx1, idx1), convertedColor[3]);
			
			_mm_store_si128((__m128i *)(dstBuffer +  0), convertedColor[0]);
			_mm_store_si128((__m128i *)(dstBuffer +  4), convertedColor[1]);
			_mm_store_si128((__m128i *)(dstBuffer +  8), convertedColor[2]);
			_mm_store_si128((__m128i *)(dstBuffer + 12), convertedColor[3]);
		}
#else
		for (size_t i = 0; i < srcSize; i++, srcData++)
		{
			u8 idx;
			
			idx = *srcData & 0x0F;
			*dstBuffer++ = (idx == 0) ? 0 : CONVERT(srcPal[idx] & 0x7FFF);
			
			idx = *srcData >> 4;
			*dstBuffer++ = (idx == 0) ? 0 : CONVERT(srcPal[idx] & 0x7FFF);
		}
#endif
	}
	else
	{
#ifdef ENABLE_SSSE3
		for (size_t i = 0; i < srcSize; i+=8, srcData+=8, dstBuffer+=16)
		{
			__m128i idx = _mm_loadl_epi64((__m128i *)srcData);
			idx = _mm_unpacklo_epi8(idx, idx);
			idx = _mm_or_si128( _mm_and_si128(idx, _mm_set1_epi16(0x000F)), _mm_and_si128(_mm_srli_epi16(idx, 4), _mm_set1_epi16(0x0F00)) );
			idx = _mm_slli_epi16(idx, 1);
			
			const __m128i idx0 = _mm_add_epi8( _mm_unpacklo_epi8(idx, idx), _mm_set1_epi16(0x0100) );
			const __m128i idx1 = _mm_add_epi8( _mm_unpackhi_epi8(idx, idx), _mm_set1_epi16(0x0100) );
			
			const __m128i palMask = _mm_cmpeq_epi8( _mm_and_si128(idx, _mm_set1_epi8(0x10)), _mm_setzero_si128() );
			const __m128i palColor0A = _mm_shuffle_epi8(palLo, idx0);
			const __m128i palColor0B = _mm_shuffle_epi8(palHi, idx0);
			const __m128i palColor1A = _mm_shuffle_epi8(palLo, idx1);
			const __m128i palColor1B = _mm_shuffle_epi8(palHi, idx1);
			
			const __m128i palColor0 = _mm_blendv_epi8( palColor0B, palColor0A, _mm_unpacklo_epi8(palMask, palMask) );
			const __m128i palColor1 = _mm_blendv_epi8( palColor1B, palColor1A, _mm_unpackhi_epi8(palMask, palMask) );
			
			__m128i convertedColor[4];
			
			if (TEXCACHEFORMAT == TexFormat_15bpp)
			{
				ColorspaceConvert555To6665Opaque_SSE2<false>(palColor0, convertedColor[0], convertedColor[1]);
				ColorspaceConvert555To6665Opaque_SSE2<false>(palColor1, convertedColor[2], convertedColor[3]);
			}
			else
			{
				ColorspaceConvert555To8888Opaque_SSE2<false>(palColor0, convertedColor[0], convertedColor[1]);
				ColorspaceConvert555To8888Opaque_SSE2<false>(palColor1, convertedColor[2], convertedColor[3]);
			}
			
			_mm_store_si128((__m128i *)(dstBuffer +  0), convertedColor[0]);
			_mm_store_si128((__m128i *)(dstBuffer +  4), convertedColor[1]);
			_mm_store_si128((__m128i *)(dstBuffer +  8), convertedColor[2]);
			_mm_store_si128((__m128i *)(dstBuffer + 12), convertedColor[3]);
		}
#else
		for (size_t i = 0; i < srcSize; i++, srcData++)
		{
			*dstBuffer++ = CONVERT(srcPal[*srcData & 0x0F] & 0x7FFF);
			*dstBuffer++ = CONVERT(srcPal[*srcData >> 4] & 0x7FFF);
		}
#endif
	}
}

template <TextureStoreUnpackFormat TEXCACHEFORMAT>
void NDSTextureUnpackI8(const size_t srcSize, const u8 *__restrict srcData, const u16 *__restrict srcPal, const bool isPalZeroTransparent, u32 *__restrict dstBuffer)
{
	if (isPalZeroTransparent)
	{
		for (size_t i = 0; i < srcSize; i++, srcData++)
		{
			const u8 idx = *srcData;
			*dstBuffer++ = (idx == 0) ? 0 : CONVERT(srcPal[idx] & 0x7FFF);
		}
	}
	else
	{
		for (size_t i = 0; i < srcSize; i++, srcData++)
		{
			*dstBuffer++ = CONVERT(srcPal[*srcData] & 0x7FFF);
		}
	}
}

template <TextureStoreUnpackFormat TEXCACHEFORMAT>
void NDSTextureUnpackA3I5(const size_t srcSize, const u8 *__restrict srcData, const u16 *__restrict srcPal, u32 *__restrict dstBuffer)
{
	for (size_t i = 0; i < srcSize; i++, srcData++)
	{
		const u16 c = srcPal[*srcData & 0x1F] & 0x7FFF;
		const u8 alpha = *srcData >> 5;
		*dstBuffer++ = (TEXCACHEFORMAT == TexFormat_15bpp) ? COLOR555TO6665(c, material_3bit_to_5bit[alpha]) : COLOR555TO8888(c, material_3bit_to_8bit[alpha]);
	}
}

template <TextureStoreUnpackFormat TEXCACHEFORMAT>
void NDSTextureUnpackA5I3(const size_t srcSize, const u8 *__restrict srcData, const u16 *__restrict srcPal, u32 *__restrict dstBuffer)
{
#ifdef ENABLE_SSSE3
	const __m128i pal_vec128 = _mm_load_si128((__m128i *)srcPal);
	
	for (size_t i = 0; i < srcSize; i+=16, srcData+=16, dstBuffer+=16)
	{
		const __m128i bits = _mm_loadu_si128((__m128i *)srcData);
		
		const __m128i idx = _mm_slli_epi16( _mm_and_si128(bits, _mm_set1_epi8(0x07)), 1 );
		const __m128i idx0 = _mm_add_epi8( _mm_unpacklo_epi8(idx, idx), _mm_set1_epi16(0x0100) );
		const __m128i idx1 = _mm_add_epi8( _mm_unpackhi_epi8(idx, idx), _mm_set1_epi16(0x0100) );
		
		const __m128i palColor0 = _mm_shuffle_epi8(pal_vec128, idx0);
		const __m128i palColor1 = _mm_shuffle_epi8(pal_vec128, idx1);
		
		__m128i convertedColor[4];
		
		if (TEXCACHEFORMAT == TexFormat_15bpp)
		{
			const __m128i alpha = _mm_srli_epi16( _mm_and_si128(bits, _mm_set1_epi8(0xF8)), 3 );
			const __m128i alphaLo = _mm_unpacklo_epi8(_mm_setzero_si128(), alpha);
			const __m128i alphaHi = _mm_unpackhi_epi8(_mm_setzero_si128(), alpha);
			
			ColorspaceConvert555To6665_SSE2<false>(palColor0, alphaLo, convertedColor[0], convertedColor[1]);
			ColorspaceConvert555To6665_SSE2<false>(palColor1, alphaHi, convertedColor[2], convertedColor[3]);
		}
		else
		{
			const __m128i alpha = _mm_or_si128( _mm_and_si128(bits, _mm_set1_epi8(0xF8)), _mm_srli_epi16(_mm_and_si128(bits, _mm_set1_epi8(0xE0)), 5) );
			const __m128i alphaLo = _mm_unpacklo_epi8(_mm_setzero_si128(), alpha);
			const __m128i alphaHi = _mm_unpackhi_epi8(_mm_setzero_si128(), alpha);
			
			ColorspaceConvert555To8888_SSE2<false>(palColor0, alphaLo, convertedColor[0], convertedColor[1]);
			ColorspaceConvert555To8888_SSE2<false>(palColor1, alphaHi, convertedColor[2], convertedColor[3]);
		}
		
		_mm_store_si128((__m128i *)(dstBuffer +  0), convertedColor[0]);
		_mm_store_si128((__m128i *)(dstBuffer +  4), convertedColor[1]);
		_mm_store_si128((__m128i *)(dstBuffer +  8), convertedColor[2]);
		_mm_store_si128((__m128i *)(dstBuffer + 12), convertedColor[3]);
	}
#else
	for (size_t i = 0; i < srcSize; i++, srcData++)
	{
		const u16 c = srcPal[*srcData & 0x07] & 0x7FFF;
		const u8 alpha = (*srcData >> 3);
		*dstBuffer++ = (TEXCACHEFORMAT == TexFormat_15bpp) ? COLOR555TO6665(c, alpha) : COLOR555TO8888(c, material_5bit_to_8bit[alpha]);
	}
#endif
}

#define PAL4X4(offset) ( LE_TO_LOCAL_16( *(u16*)( MMU.texInfo.texPalSlot[((palAddress + (offset)*2)>>14)&0x7] + ((palAddress + (offset)*2)&0x3FFF) ) ) & 0x7FFF )

template <TextureStoreUnpackFormat TEXCACHEFORMAT>
void NDSTextureUnpack4x4(const size_t srcSize, const u32 *__restrict srcData, const u16 *__restrict srcIndex, const u32 palAddress, const u32 sizeX, const u32 sizeY, u32 *__restrict dstBuffer)
{
	const u32 limit = srcSize * sizeof(u32);
	const u16 xTmpSize = sizeX >> 2;
	const u16 yTmpSize = sizeY >> 2;
	
	//this is flagged whenever a 4x4 overruns its slot.
	//i am guessing we just generate black in that case
	bool dead = false;
	
	for (size_t y = 0, d = 0; y < yTmpSize; y++)
	{
		u32 tmpPos[4]={(y<<2)*sizeX,((y<<2)+1)*sizeX,
			((y<<2)+2)*sizeX,((y<<2)+3)*sizeX};
		for (size_t x = 0; x < xTmpSize; x++, d++)
		{
			if (d >= limit)
				dead = true;
			
			if (dead)
			{
				for (int sy = 0; sy < 4; sy++)
				{
					const u32 currentPos = (x<<2) + tmpPos[sy];
					dstBuffer[currentPos] = dstBuffer[currentPos+1] = dstBuffer[currentPos+2] = dstBuffer[currentPos+3] = 0;
				}
				continue;
			}
			
			const u32 currBlock		= LE_TO_LOCAL_32(srcData[d]);
			const u16 pal1			= LE_TO_LOCAL_16(srcIndex[d]);
			const u16 pal1offset	= (pal1 & 0x3FFF)<<1;
			const u8  mode			= pal1>>14;
			CACHE_ALIGN u32 tmp_col[4];
			
			tmp_col[0] = COLOR555TO8888_OPAQUE( PAL4X4(pal1offset) );
			tmp_col[1] = COLOR555TO8888_OPAQUE( PAL4X4(pal1offset+1) );
			
			switch (mode)
			{
				case 0:
					tmp_col[2] = COLOR555TO8888_OPAQUE( PAL4X4(pal1offset+2) );
					tmp_col[3] = 0x00000000;
					break;
					
				case 1:
#ifdef MSB_FIRST
					tmp_col[2]	= ( (((tmp_col[0] & 0xFF000000) >> 1)+((tmp_col[1] & 0xFF000000)  >> 1)) & 0xFF000000 ) |
					              ( (((tmp_col[0] & 0x00FF0000)      + (tmp_col[1] & 0x00FF0000)) >> 1)  & 0x00FF0000 ) |
					              ( (((tmp_col[0] & 0x0000FF00)      + (tmp_col[1] & 0x0000FF00)) >> 1)  & 0x0000FF00 ) |
					              0x000000FF;
					tmp_col[3]	= 0x00000000;
#else
					tmp_col[2]	= ( (((tmp_col[0] & 0x00FF00FF) + (tmp_col[1] & 0x00FF00FF)) >> 1) & 0x00FF00FF ) |
					              ( (((tmp_col[0] & 0x0000FF00) + (tmp_col[1] & 0x0000FF00)) >> 1) & 0x0000FF00 ) |
					              0xFF000000;
					tmp_col[3]	= 0x00000000;
#endif
					break;
					
				case 2:
					tmp_col[2] = COLOR555TO8888_OPAQUE( PAL4X4(pal1offset+2) );
					tmp_col[3] = COLOR555TO8888_OPAQUE( PAL4X4(pal1offset+3) );
					break;
					
				case 3:
				{
#ifdef MSB_FIRST
					const u32 r0	= (tmp_col[0]>>24) & 0x000000FF;
					const u32 r1	= (tmp_col[1]>>24) & 0x000000FF;
					const u32 g0	= (tmp_col[0]>>16) & 0x000000FF;
					const u32 g1	= (tmp_col[1]>>16) & 0x000000FF;
					const u32 b0	= (tmp_col[0]>> 8) & 0x000000FF;
					const u32 b1	= (tmp_col[1]>> 8) & 0x000000FF;
#else
					const u32 r0	=  tmp_col[0]      & 0x000000FF;
					const u32 r1	=  tmp_col[1]      & 0x000000FF;
					const u32 g0	= (tmp_col[0]>> 8) & 0x000000FF;
					const u32 g1	= (tmp_col[1]>> 8) & 0x000000FF;
					const u32 b0	= (tmp_col[0]>>16) & 0x000000FF;
					const u32 b1	= (tmp_col[1]>>16) & 0x000000FF;
#endif
					
					const u16 tmp1	= (  (r0*5 + r1*3)>>6) |
					                  ( ((g0*5 + g1*3)>>6) <<  5 ) |
					                  ( ((b0*5 + b1*3)>>6) << 10 );
					const u16 tmp2	= (  (r0*3 + r1*5)>>6) |
					                  ( ((g0*3 + g1*5)>>6) <<  5 ) |
					                  ( ((b0*3 + b1*5)>>6) << 10 );
					
					tmp_col[2] = COLOR555TO8888_OPAQUE(tmp1);
					tmp_col[3] = COLOR555TO8888_OPAQUE(tmp2);
					break;
				}
					
				default:
					tmp_col[2] = 0;
					tmp_col[3] = 0;
					break;
			}
			
			if (TEXCACHEFORMAT == TexFormat_15bpp)
			{
				tmp_col[0] = ColorspaceConvert8888To6665<false>(tmp_col[0]);
				tmp_col[1] = ColorspaceConvert8888To6665<false>(tmp_col[1]);
				tmp_col[2] = ColorspaceConvert8888To6665<false>(tmp_col[2]);
				tmp_col[3] = ColorspaceConvert8888To6665<false>(tmp_col[3]);
			}
			
			//TODO - this could be more precise for 32bpp mode (run it through the color separation table)
			
			//set all 16 texels
			for (size_t sy = 0; sy < 4; sy++)
			{
				// Texture offset
				const u32 currentPos = (x<<2) + tmpPos[sy];
				const u8 currRow = (u8)((currBlock>>(sy<<3))&0xFF);
				
				dstBuffer[currentPos  ] = tmp_col[ currRow    &3];
				dstBuffer[currentPos+1] = tmp_col[(currRow>>2)&3];
				dstBuffer[currentPos+2] = tmp_col[(currRow>>4)&3];
				dstBuffer[currentPos+3] = tmp_col[(currRow>>6)&3];
			}
		}
	}
}

template <TextureStoreUnpackFormat TEXCACHEFORMAT>
void NDSTextureUnpackDirect16Bit(const size_t srcSize, const u16 *__restrict srcData, u32 *__restrict dstBuffer)
{
	const size_t pixCount = srcSize >> 1;
	size_t i = 0;
	
#ifdef ENABLE_SSE2
	const size_t pixCountVec128 = pixCount - (pixCount % 8);
	for (; i < pixCountVec128; i+=8, srcData+=8, dstBuffer+=8)
	{
		const v128u16 c = _mm_load_si128((v128u16 *)srcData);
		const v128u16 alpha = _mm_cmpeq_epi16(_mm_srli_epi16(c, 15), _mm_set1_epi16(1));
		v128u32 convertedColor[2];
		
		if (TEXCACHEFORMAT == TexFormat_15bpp)
		{
			ColorspaceConvert555To6665Opaque_SSE2<false>(c, convertedColor[0], convertedColor[1]);
		}
		else
		{
			ColorspaceConvert555To8888Opaque_SSE2<false>(c, convertedColor[0], convertedColor[1]);
		}
		
		convertedColor[0] = _mm_blendv_epi8(_mm_setzero_si128(), convertedColor[0], _mm_unpacklo_epi16(alpha, alpha));
		convertedColor[1] = _mm_blendv_epi8(_mm_setzero_si128(), convertedColor[1], _mm_unpackhi_epi16(alpha, alpha));
		
		_mm_store_si128((v128u32 *)(dstBuffer + 0), convertedColor[0]);
		_mm_store_si128((v128u32 *)(dstBuffer + 4), convertedColor[1]);
	}
#endif
	
	for (; i < pixCount; i++, srcData++)
	{
		const u16 c = LOCAL_TO_LE_16(*srcData);
		*dstBuffer++ = (c & 0x8000) ? CONVERT(c & 0x7FFF) : 0;
	}
}

template void TextureStore::Unpack<TexFormat_15bpp>(u32 *unpackBuffer);
template void TextureStore::Unpack<TexFormat_32bpp>(u32 *unpackBuffer);
