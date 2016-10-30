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

#ifndef _TEXCACHE_H_
#define _TEXCACHE_H_

#include <map>

#include "types.h"
#include "common.h"
#include "gfx3d.h"

//this ought to be enough for anyone
//#define TEXCACHE_MAX_SIZE (64*1024*1024);
//changed by zeromus on 15-dec. I couldnt find any games that were getting anywhere NEAR 64
//metal slug burns through sprites so fast, it can test it pretty quickly though
#define TEXCACHE_MAX_SIZE (16*1024*1024)

#define PALETTE_DUMP_SIZE ((64+16+16)*1024)

enum TexCache_TexFormat
{
	TexFormat_None, //used when nothing yet is cached
	TexFormat_32bpp, //used by ogl renderer
	TexFormat_15bpp //used by rasterizer
};

class MemSpan;
class TexCacheItem;

typedef std::multimap<u32,TexCacheItem*> TTexCacheItemMultimap;
typedef void (*TexCacheItemDeleteCallback)(TexCacheItem *texItem, void *param1, void *param2);

class TexCache
{
public:
	TexCache();
	
	TTexCacheItemMultimap index;
	u32 cache_size; //this is not really precise, it is off by a constant factor
	u8 paletteDump[PALETTE_DUMP_SIZE];
	
	void list_remove(TexCacheItem *item);
	void list_push_front(TexCacheItem *item);
	
	void Invalidate();
	void Evict(u32 target);
	void Reset();
	
	TexCacheItem* GetTexture(TexCache_TexFormat texCacheFormat, u32 texAttributes, u32 palAttributes);
};

class TexCacheItem
{
private:
	TexCacheItemDeleteCallback _deleteCallback;
	void *_deleteCallbackParam1;
	void *_deleteCallbackParam2;
	
public:
	TexCacheItem();
	~TexCacheItem();
	
	NDSTextureFormat packFormat;
	u32 packSize;
	u8 *packData;
	u16 *paletteColorTable;
	
	TexCache_TexFormat unpackFormat;
	u32 unpackSize;
	u32 *unpackData;
	
	bool suspectedInvalid;
	bool assumedInvalid;
	TTexCacheItemMultimap::iterator iterator;

	u32 textureAttributes;
	u32 paletteAttributes;
	u32 paletteAddress;
	u32 paletteSize;
	u32 sizeX;
	u32 sizeY;
	float invSizeX;
	float invSizeY;
	
	// Only used by 4x4 formatted textures
	u8 *packIndexData;
	u32 packSizeFirstSlot;
	u32 packIndexSize;
	
	// Only used by the OpenGL renderer for the texture ID
	u32 texid;
	
	TexCacheItemDeleteCallback GetDeleteCallback() const;
	void SetDeleteCallback(TexCacheItemDeleteCallback callbackFunc, void *inParam1, void *inParam2);
	
	NDSTextureFormat GetTextureFormat() const;
	void SetTextureData(const u32 attr, const MemSpan &packedData, const MemSpan &packedIndexData);
	void SetTexturePalette(const u32 attr, const u16 *paletteBuffer);
	
	template<TexCache_TexFormat TEXCACHEFORMAT> void Unpack(const MemSpan &packedData);
	
	void DebugDump();
};

// TODO: Delete these MemSpan based functions after testing confirms that using the dumped texture data works properly.
template<TexCache_TexFormat TEXCACHEFORMAT> void NDSTextureUnpackI2(const MemSpan &ms, const u16 *pal, const bool isPalZeroTransparent, u32 *dstBuffer);
template<TexCache_TexFormat TEXCACHEFORMAT> void NDSTextureUnpackI4(const MemSpan &ms, const u16 *pal, const bool isPalZeroTransparent, u32 *dstBuffer);
template<TexCache_TexFormat TEXCACHEFORMAT> void NDSTextureUnpackI8(const MemSpan &ms, const u16 *pal, const bool isPalZeroTransparent, u32 *dstBuffer);
template<TexCache_TexFormat TEXCACHEFORMAT> void NDSTextureUnpackA3I5(const MemSpan &ms, const u16 *pal, u32 *dstBuffer);
template<TexCache_TexFormat TEXCACHEFORMAT> void NDSTextureUnpackA5I3(const MemSpan &ms, const u16 *pal, u32 *dstBuffer);
template<TexCache_TexFormat TEXCACHEFORMAT> void NDSTextureUnpack4x4(const MemSpan &ms, const u32 palAddress, const u32 texAttributes, const u32 sizeX, const u32 sizeY, u32 *dstBuffer);
template<TexCache_TexFormat TEXCACHEFORMAT> void NDSTextureUnpackDirect16Bit(const MemSpan &ms, u32 *dstBuffer);

template<TexCache_TexFormat TEXCACHEFORMAT> void NDSTextureUnpackI2(const size_t srcSize, const u8 *srcData, const u16 *srcPal, const bool isPalZeroTransparent, u32 *dstBuffer);
template<TexCache_TexFormat TEXCACHEFORMAT> void NDSTextureUnpackI4(const size_t srcSize, const u8 *srcData, const u16 *srcPal, const bool isPalZeroTransparent, u32 *dstBuffer);
template<TexCache_TexFormat TEXCACHEFORMAT> void NDSTextureUnpackI8(const size_t srcSize, const u8 *srcData, const u16 *srcPal, const bool isPalZeroTransparent, u32 *dstBuffer);
template<TexCache_TexFormat TEXCACHEFORMAT> void NDSTextureUnpackA3I5(const size_t srcSize, const u8 *srcData, const u16 *srcPal, u32 *dstBuffer);
template<TexCache_TexFormat TEXCACHEFORMAT> void NDSTextureUnpackA5I3(const size_t srcSize, const u8 *srcData, const u16 *srcPal, u32 *dstBuffer);
template<TexCache_TexFormat TEXCACHEFORMAT> void NDSTextureUnpack4x4(const size_t srcSize, const u8 *srcData, const u8 *srcIndex, const u32 palAddress, const u32 texAttributes, const u32 sizeX, const u32 sizeY, u32 *dstBuffer);
template<TexCache_TexFormat TEXCACHEFORMAT> void NDSTextureUnpackDirect16Bit(const size_t srcSize, const u8 *srcData, u32 *dstBuffer);

extern TexCache texCache;

#endif
