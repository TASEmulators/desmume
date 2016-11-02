/*
	Copyright (C) 2006 yopyop
	Copyright (C) 2006-2007 shash
	Copyright (C) 2008-2016 DeSmuME team

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

#include "bits.h"
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
	int memcmp(void* buf2, int size=-1)
	{
		if(size==-1) size = this->size;
		size = min(this->size,size);
		for(int i=0;i<numItems;i++)
		{
			Item &item = items[i];
			int todo = min((int)item.len,size);
			size -= todo;
			int temp = ::memcmp(item.ptr,((u8*)buf2)+item.ofs,todo);
			if(temp) return temp;
			if(size == 0) break;
		}
		return 0;
	}

	//TODO - get rid of duplication between these two methods.

	//dumps the memspan to the specified buffer
	//you may set size to limit the size to be copied
	int dump(void* buf, int size=-1) const
	{
		if(size==-1) size = this->size;
		size = min(this->size,size);
		u8* bufptr = (u8*)buf;
		int done = 0;
		for(int i=0;i<numItems;i++)
		{
			Item item = items[i];
			int todo = min((int)item.len,size);
			size -= todo;
			done += todo;
			memcpy(bufptr,item.ptr,todo);
			bufptr += todo;
			if(size==0) return done;
		}
		return done;
	}

	// this function does the same than dump
	// but works for both little and big endian
	// when buf is an u16 array
	int dump16(void* buf, int size=-1) const
	{
		if(size==-1) size = this->size;
		size = min(this->size,size);
		u16* bufptr = (u16*)buf;
		int done = 0;
		for(int i=0;i<numItems;i++)
		{
			Item item = items[i];
			u8 * src = (u8 *) item.ptr;
			int todo = min((int)item.len,size);
			size -= todo;
			done += todo;
			for(int j = 0;j < todo / 2;j++)
			{
				u16 tmp;
				tmp = *src++;
				tmp |= *(src++) << 8;
				*bufptr++ = tmp;
			}
			if(size==0) return done;
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
		
		//TODO - dont alert if the masterbrightnesses are max or min
		if (ptr == MMU.blank_memory && !GPU->GetEngineMain()->GetIsMasterBrightFullIntensity()) {
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
		
		//TODO - dont alert if the masterbrightnesses are max or min
		if(ptr == MMU.blank_memory && !GPU->GetEngineMain()->GetIsMasterBrightFullIntensity() && !silent) {
			PROGINFO("Tried to reference unmapped texture palette memory: 16k slot #%d\n",slot);
		}
		curr.ptr = ptr + curr.start;
	}
	return ret;
}

TexCache texCache;

TexCache::TexCache()
{
	cacheTable.clear();
	cache_size = 0;
	memset(paletteDump, 0, sizeof(paletteDump));
}

void TexCache::list_remove(TexCacheItem *item)
{
	const TexCacheKey key = TexCache::GenerateKey(item->textureAttributes, item->paletteAttributes);
	this->cacheTable.erase(key);
	this->cache_size -= item->unpackSize;
}

void TexCache::list_push_front(TexCacheItem *item)
{
	const TexCacheKey key = TexCache::GenerateKey(item->textureAttributes, item->paletteAttributes);
	this->cacheTable[key] = item;
	this->cache_size += item->unpackSize;
}

void TexCache::Invalidate()
{
	//check whether the palette memory changed
	//TODO - we should handle this instead by setting dirty flags in the vram memory mapping and noting whether palette memory was dirty.
	//but this will work for now
	MemSpan mspal = MemSpan_TexPalette(0, PALETTE_DUMP_SIZE, true);
	bool paletteDirty = mspal.memcmp(this->paletteDump);
	if (paletteDirty)
	{
		mspal.dump(this->paletteDump);
	}
	
	for (TexCacheTable::iterator it(this->cacheTable.begin()); it != this->cacheTable.end(); ++it)
	{
		it->second->suspectedInvalid = true;
		
		//when the palette changes, we assume all 4x4 textures are dirty.
		//this is because each 4x4 item doesnt carry along with it a copy of the entire palette, for verification
		//instead, we just use the one paletteDump for verifying of all 4x4 textures; and if paletteDirty is set, verification has failed
		if( (it->second->GetTextureFormat() == TEXMODE_4X4) && paletteDirty )
		{
			it->second->assumedInvalid = true;
		}
	}
}

void TexCache::Evict(u32 target)
{
	//debug print
	//printf("%d %d/%d\n",index.size(),cache_size/1024,target/1024);
	
	//dont do anything unless we're over the target
	if (cache_size < target) return;
	
	//aim at cutting the cache to half of the max size
	target /= 2;
	
	//evicts items in an arbitrary order until it is less than the max cache size
	//TODO - do this based on age and not arbitrarily
	while (this->cache_size > target)
	{
		if (this->cacheTable.size() == 0) break; //just in case.. doesnt seem possible, cache_size wouldve been 0
		
		TexCacheItem *item = this->cacheTable.begin()->second;
		this->list_remove(item);
		//printf("evicting! totalsize:%d\n",cache_size);
		delete item;
	}
}

void TexCache::Reset()
{
	this->Evict(0);
}

TexCacheItem* TexCache::GetTexture(u32 texAttributes, u32 palAttributes)
{
	TexCacheItem *theTexture = NULL;
	bool didCreateNewTexture = false;
	bool needLoadTexData = false;
	bool needLoadPalette = false;
	
	//conditions where we reject matches:
	//when the teximage or texpal params dont match
	//(this is our key for identifying textures in the cache)
	const TexCacheKey key = TexCache::GenerateKey(texAttributes, palAttributes);
	const TexCacheTable::iterator cachedTexture = this->cacheTable.find(key);
	
	if (cachedTexture == this->cacheTable.end())
	{
		theTexture = new TexCacheItem(texAttributes, palAttributes);
		didCreateNewTexture = true;
		needLoadTexData = true;
		needLoadPalette = true;
	}
	else
	{
		theTexture = cachedTexture->second;
		
		//if the texture is assumed invalid, reject it
		if (theTexture->assumedInvalid)
		{
			needLoadTexData = true;
			needLoadPalette = true;
		}
		
		//the texture matches params, but isnt suspected invalid. accept it.
		if (!theTexture->suspectedInvalid)
		{
			return theTexture;
		}
	}
	
	//we suspect the texture may be invalid. we need to do a byte-for-byte comparison to re-establish that it is valid:
	
	//dump the palette to a temp buffer, so that we don't have to worry about memory mapping.
	//this isnt such a problem with texture memory, because we read sequentially from it.
	//however, we read randomly from palette memory, so the mapping is more costly.
	MemSpan currentPaletteMS = MemSpan_TexPalette(theTexture->paletteAddress, theTexture->paletteSize, false);
	
	CACHE_ALIGN u16 currentPalette[256];
#ifdef WORDS_BIGENDIAN
	currentPaletteMS.dump16(currentPalette);
#else
	currentPaletteMS.dump(currentPalette);
#endif
	
	//when the palettes dont match:
	//note that we are considering 4x4 textures to have a palette size of 0.
	//they really have a potentially HUGE palette, too big for us to handle like a normal palette,
	//so they go through a different system
	if ( !didCreateNewTexture && (theTexture->paletteSize > 0) && memcmp(theTexture->paletteColorTable, currentPalette, theTexture->paletteSize) )
	{
		needLoadPalette = true;
	}
	
	//analyze the texture memory mapping and the specifications of this texture
	MemSpan currentPackedTexDataMS = MemSpan_TexMem(theTexture->packAddress, theTexture->packSize);
	
	//when the texture data doesn't match
	if ( !didCreateNewTexture && (theTexture->packSize > 0) && currentPackedTexDataMS.memcmp(theTexture->packData, theTexture->packSize) )
	{
		needLoadTexData = true;
	}
	
	//if the texture is 4x4 then the index data must match
	MemSpan currentPackedTexIndexMS;
	if (theTexture->packFormat == TEXMODE_4X4)
	{
		//determine the location for 4x4 index data
		currentPackedTexIndexMS = MemSpan_TexMem(theTexture->packIndexAddress, theTexture->packIndexSize);
		
		if ( !didCreateNewTexture && (theTexture->packIndexSize > 0) && currentPackedTexIndexMS.memcmp(theTexture->packIndexData, theTexture->packIndexSize) )
		{
			needLoadTexData = true;
			needLoadPalette = true;
		}
	}
	
	if (!needLoadTexData && !needLoadPalette)
	{
		//we found a match. just return it
		theTexture->suspectedInvalid = false;
		return theTexture;
	}
	
	if (needLoadTexData)
	{
		theTexture->SetTextureData(currentPackedTexDataMS, currentPackedTexIndexMS);
		theTexture->unpackFormat = TexFormat_None;
	}
	
	if (needLoadPalette)
	{
		theTexture->SetTexturePalette(currentPalette);
		theTexture->unpackFormat = TexFormat_None;
	}
	
	if (didCreateNewTexture)
	{
		this->list_push_front(theTexture);
		//printf("allocating: up to %d with %d items\n",cache_size,index.size());
	}
	
	theTexture->assumedInvalid = false;
	theTexture->suspectedInvalid = false;
	return theTexture;
}

TexCacheKey TexCache::GenerateKey(const u32 texAttributes, const u32 palAttributes)
{
	// Since the repeat, flip, and coordinate transformation modes are render settings
	// and not data settings, we can mask out those bits to help reduce duplicate entries.
	return (TexCacheKey)( ((u64)palAttributes << 32) | (u64)(texAttributes & 0x3FF0FFFF) );
}

TexCacheItem::TexCacheItem()
{
	_deleteCallback = NULL;
	_deleteCallbackParam1 = NULL;
	_deleteCallbackParam2 = NULL;
	
	textureAttributes = 0;
	paletteAttributes = 0;
	
	sizeX = 0;
	sizeY = 0;
	invSizeX = 0.0f;
	invSizeY = 0.0f;
	isPalZeroTransparent = false;
	
	suspectedInvalid = false;
	assumedInvalid = false;
	
	packFormat = TEXMODE_NONE;
	packAddress = 0;
	packSize = 0;
	packData = NULL;
	
	paletteAddress = 0;
	paletteSize = 0;
	paletteColorTable = NULL;
	
	unpackFormat = TexFormat_None;
	unpackSize = 0;
	unpackData = NULL;
	
	packIndexAddress = 0;
	packIndexSize = 0;
	packIndexData = NULL;
	packSizeFirstSlot = 0;
	
	texid = 0;
}

TexCacheItem::TexCacheItem(const u32 texAttributes, const u32 palAttributes)
{
	//for each texformat, multiplier from numtexels to numbytes (fixed point 30.2)
	static const u32 texSizes[] = {0, 4, 1, 2, 4, 1, 4, 8};
	
	//for each texformat, number of palette entries
	static const u32 paletteSizeList[] = {0, 32, 4, 16, 256, 0, 8, 0};
	
	_deleteCallback = NULL;
	_deleteCallbackParam1 = NULL;
	_deleteCallbackParam2 = NULL;
	
	texid = 0;
	
	textureAttributes = texAttributes;
	paletteAttributes = palAttributes;
	
	sizeX = (8 << ((texAttributes >> 20) & 0x07));
	sizeY = (8 << ((texAttributes >> 23) & 0x07));
	invSizeX = 1.0f / (float)sizeX;
	invSizeY = 1.0f / (float)sizeY;
	
	packFormat = (NDSTextureFormat)((texAttributes >> 26) & 0x07);
	packAddress = (texAttributes & 0xFFFF) << 3;
	packSize = (sizeX*sizeY*texSizes[packFormat]) >> 2; //shifted because the texSizes multiplier is fixed point
	packData = (u8 *)malloc_alignedCacheLine(packSize);
	
	if ( (packFormat == TEXMODE_I2) || (packFormat == TEXMODE_I4) || (packFormat == TEXMODE_I8) )
	{
		isPalZeroTransparent = ( ((texAttributes >> 29) & 1) != 0 );
	}
	else
	{
		isPalZeroTransparent = false;
	}
	
	paletteAddress = (packFormat == TEXMODE_I2) ? palAttributes << 3 : palAttributes << 4;
	paletteSize = paletteSizeList[packFormat] * sizeof(u16);
	paletteColorTable = (paletteSize > 0) ? (u16 *)malloc_alignedCacheLine(paletteSize) : NULL;
	
	unpackFormat = TexFormat_None;
	unpackSize = 0;
	unpackData = NULL;
	
	if (packFormat == TEXMODE_4X4)
	{
		const u32 indexBase = ((texAttributes & 0xC000) == 0x8000) ? 0x30000 : 0x20000;
		const u32 indexOffset = (texAttributes & 0x3FFF) << 2;
		packIndexAddress = indexBase + indexOffset;
		packIndexSize = (sizeX * sizeY) >> 3;
		packIndexData = (u8 *)malloc_alignedCacheLine(packIndexSize);
		packSizeFirstSlot = 0;
	}
	else
	{
		packIndexAddress = 0;
		packIndexSize = 0;
		packIndexData = NULL;
		packSizeFirstSlot = 0;
	}
	
	suspectedInvalid = true;
	assumedInvalid = true;
}

TexCacheItem::~TexCacheItem()
{
	free_aligned(this->packData);
	free_aligned(this->unpackData);
	free_aligned(this->paletteColorTable);
	free_aligned(this->packIndexData);
	if (this->_deleteCallback != NULL) this->_deleteCallback(this, this->_deleteCallbackParam1, this->_deleteCallbackParam2);
}

TexCacheItemDeleteCallback TexCacheItem::GetDeleteCallback() const
{
	return this->_deleteCallback;
}

void TexCacheItem::SetDeleteCallback(TexCacheItemDeleteCallback callbackFunc, void *inParam1, void *inParam2)
{
	this->_deleteCallback = callbackFunc;
	this->_deleteCallbackParam1 = inParam1;
	this->_deleteCallbackParam2 = inParam2;
}

NDSTextureFormat TexCacheItem::GetTextureFormat() const
{
	return this->packFormat;
}

void TexCacheItem::SetTextureData(const MemSpan &packedData, const MemSpan &packedIndexData)
{
	//dump texture and 4x4 index data for cache keying
	this->packSizeFirstSlot = packedData.items[0].len;
	
	packedData.dump(this->packData);
	
	if (this->packFormat == TEXMODE_4X4)
	{
		packedIndexData.dump(this->packIndexData, this->packIndexSize);
	}
	
	const u32 currentUnpackSize = this->sizeX * this->sizeY * sizeof(u32);
	if (this->unpackSize != currentUnpackSize)
	{
		u32 *oldUnpackData = this->unpackData;
		this->unpackSize = currentUnpackSize;
		this->unpackData = (u32 *)malloc_alignedCacheLine(currentUnpackSize);
		free_aligned(oldUnpackData);
	}
}

void TexCacheItem::SetTexturePalette(const u16 *paletteBuffer)
{
	if (this->paletteSize > 0)
	{
		memcpy(this->paletteColorTable, paletteBuffer, this->paletteSize);
	}
}

template <TexCache_TexFormat TEXCACHEFORMAT>
void TexCacheItem::Unpack()
{
	this->unpackFormat = TEXCACHEFORMAT;
	
	// Whenever a 1-bit alpha or no-alpha texture is unpacked (this means any texture
	// format that is not A3I5 or A5I3), set all transparent pixels to 0 so that 3D
	// renderers can assume that the transparent color is 0 during texture sampling.
	
	switch (this->packFormat)
	{
		case TEXMODE_A3I5:
			NDSTextureUnpackA3I5<TEXCACHEFORMAT>(this->packSize, this->packData, this->paletteColorTable, this->unpackData);
			break;
			
		case TEXMODE_I2:
			NDSTextureUnpackI2<TEXCACHEFORMAT>(this->packSize, this->packData, this->paletteColorTable, this->isPalZeroTransparent, this->unpackData);
			break;
			
		case TEXMODE_I4:
			NDSTextureUnpackI4<TEXCACHEFORMAT>(this->packSize, this->packData, this->paletteColorTable, this->isPalZeroTransparent, this->unpackData);
			break;
			
		case TEXMODE_I8:
			NDSTextureUnpackI8<TEXCACHEFORMAT>(this->packSize, this->packData, this->paletteColorTable, this->isPalZeroTransparent, this->unpackData);
			break;
			
		case TEXMODE_4X4:
		{
			if (this->packSize > this->packSizeFirstSlot)
			{
				PROGINFO("Your 4x4 texture has overrun its texture slot.\n");
			}
			
			NDSTextureUnpack4x4<TEXCACHEFORMAT>(this->packSizeFirstSlot, (u32 *)this->packData, (u16 *)this->packIndexData, this->paletteAddress, this->textureAttributes, this->sizeX, this->sizeY, this->unpackData);
			break;
		}
			
		case TEXMODE_A5I3:
			NDSTextureUnpackA5I3<TEXCACHEFORMAT>(this->packSize, this->packData, this->paletteColorTable, this->unpackData);
			break;
			
		case TEXMODE_16BPP:
			NDSTextureUnpackDirect16Bit<TEXCACHEFORMAT>(this->packSize, (u16 *)this->packData, this->unpackData);
			break;
			
		default:
			break;
	}
	
#ifdef DO_DEBUG_DUMP_TEXTURE
	this->DebugDump();
#endif
}

#ifdef DO_DEBUG_DUMP_TEXTURE
void TexCacheItem::DebugDump()
{
	static int ctr=0;
	char fname[100];
	sprintf(fname,"c:\\dump\\%d.bmp", ctr);
	ctr++;
	
	NDS_WriteBMP_32bppBuffer(this->sizeX, this->sizeY, this->unpackData, fname);
}
#endif

template <TexCache_TexFormat TEXCACHEFORMAT>
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

template <TexCache_TexFormat TEXCACHEFORMAT>
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

template <TexCache_TexFormat TEXCACHEFORMAT>
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

template <TexCache_TexFormat TEXCACHEFORMAT>
void NDSTextureUnpackA3I5(const size_t srcSize, const u8 *__restrict srcData, const u16 *__restrict srcPal, u32 *__restrict dstBuffer)
{
	for (size_t i = 0; i < srcSize; i++, srcData++)
	{
		const u16 c = srcPal[*srcData & 0x1F] & 0x7FFF;
		const u8 alpha = *srcData >> 5;
		*dstBuffer++ = (TEXCACHEFORMAT == TexFormat_15bpp) ? COLOR555TO6665(c, material_3bit_to_5bit[alpha]) : COLOR555TO8888(c, material_3bit_to_8bit[alpha]);
	}
}

template <TexCache_TexFormat TEXCACHEFORMAT>
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
		
		__m128i tmpAlpha[2];
		__m128i convertedColor[4];
		
		if (TEXCACHEFORMAT == TexFormat_15bpp)
		{
			const __m128i alpha = _mm_srli_epi16( _mm_and_si128(bits, _mm_set1_epi8(0xF8)), 3 );
			const __m128i alphaLo = _mm_unpacklo_epi8(_mm_setzero_si128(), alpha);
			const __m128i alphaHi = _mm_unpackhi_epi8(_mm_setzero_si128(), alpha);
			
			tmpAlpha[0] = _mm_unpacklo_epi16(_mm_setzero_si128(), alphaLo);
			tmpAlpha[1] = _mm_unpackhi_epi16(_mm_setzero_si128(), alphaLo);
			ColorspaceConvert555To6665_SSE2<false>(palColor0, tmpAlpha[0], tmpAlpha[1], convertedColor[0], convertedColor[1]);
			
			tmpAlpha[0] = _mm_unpacklo_epi16(_mm_setzero_si128(), alphaHi);
			tmpAlpha[1] = _mm_unpackhi_epi16(_mm_setzero_si128(), alphaHi);
			ColorspaceConvert555To6665_SSE2<false>(palColor1, tmpAlpha[0], tmpAlpha[1], convertedColor[2], convertedColor[3]);
		}
		else
		{
			const __m128i alpha = _mm_or_si128( _mm_and_si128(bits, _mm_set1_epi8(0xF8)), _mm_srli_epi16(_mm_and_si128(bits, _mm_set1_epi8(0xE0)), 5) );
			const __m128i alphaLo = _mm_unpacklo_epi8(_mm_setzero_si128(), alpha);
			const __m128i alphaHi = _mm_unpackhi_epi8(_mm_setzero_si128(), alpha);
			
			tmpAlpha[0] = _mm_unpacklo_epi16(_mm_setzero_si128(), alphaLo);
			tmpAlpha[1] = _mm_unpackhi_epi16(_mm_setzero_si128(), alphaLo);
			ColorspaceConvert555To8888_SSE2<false>(palColor0, tmpAlpha[0], tmpAlpha[1], convertedColor[0], convertedColor[1]);
			
			tmpAlpha[0] = _mm_unpacklo_epi16(_mm_setzero_si128(), alphaHi);
			tmpAlpha[1] = _mm_unpackhi_epi16(_mm_setzero_si128(), alphaHi);
			ColorspaceConvert555To8888_SSE2<false>(palColor1, tmpAlpha[0], tmpAlpha[1], convertedColor[2], convertedColor[3]);
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

template <TexCache_TexFormat TEXCACHEFORMAT>
void NDSTextureUnpack4x4(const size_t srcSize, const u32 *__restrict srcData, const u16 *__restrict srcIndex, const u32 palAddress, const u32 texAttributes, const u32 sizeX, const u32 sizeY, u32 *__restrict dstBuffer)
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
			u32 tmp_col[4];
			
			tmp_col[0] = COLOR555TO8888_OPAQUE( PAL4X4(pal1offset) );
			tmp_col[1] = COLOR555TO8888_OPAQUE( PAL4X4(pal1offset+1) );
			
			switch (mode)
			{
				case 0:
					tmp_col[2] = COLOR555TO8888_OPAQUE( PAL4X4(pal1offset+2) );
					tmp_col[3] = 0x00000000;
					break;
					
				case 1:
#ifdef LOCAL_BE
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
#ifdef LOCAL_BE
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
			}
			
			if (TEXCACHEFORMAT == TexFormat_15bpp)
			{
				for (size_t i = 0; i < 4; i++)
				{
#ifdef LOCAL_BE
					const u32 a = (tmp_col[i] >> 3) & 0x0000001F;
					tmp_col[i] >>= 2;
					tmp_col[i] &= 0x3F3F3F00;
					tmp_col[i] |= a;
#else
					const u32 a = (tmp_col[i] >> 3) & 0x1F000000;
					tmp_col[i] >>= 2;
					tmp_col[i] &= 0x003F3F3F;
					tmp_col[i] |= a;
#endif
				}
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

template <TexCache_TexFormat TEXCACHEFORMAT>
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

template void TexCacheItem::Unpack<TexFormat_15bpp>();
template void TexCacheItem::Unpack<TexFormat_32bpp>();
