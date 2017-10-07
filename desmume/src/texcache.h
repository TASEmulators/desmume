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
#include <vector>

#include "types.h"
#include "common.h"
#include "gfx3d.h"

//this ought to be enough for anyone
//#define TEXCACHE_DEFAULT_THRESHOLD (64*1024*1024)

//changed by zeromus on 15-dec. I couldnt find any games that were getting anywhere NEAR 64
//metal slug burns through sprites so fast, it can test it pretty quickly though
//#define TEXCACHE_DEFAULT_THRESHOLD (16*1024*1024)

// rogerman, 2016-11-02: Increase this to 32MB for games that use many large textures, such
// as Umihara Kawase Shun, which can cache over 20MB in the first level.
#define TEXCACHE_DEFAULT_THRESHOLD (32*1024*1024)

#define PALETTE_DUMP_SIZE ((64+16+16)*1024)

enum TextureStoreUnpackFormat
{
	TexFormat_None    = 0, //used when nothing yet is cached
	TexFormat_32bpp,       //used by ogl renderer
	TexFormat_15bpp        //used by rasterizer
};

class MemSpan;
class TextureStore;

typedef u64 TextureCacheKey;
typedef std::map<TextureCacheKey, TextureStore *> TextureCacheMap; // Key = A TextureCacheKey that includes a combination of the texture's NDS texture attributes and palette attributes; Value = Pointer to the texture item
typedef std::vector<TextureStore *> TextureCacheList;
//typedef u32 TextureFingerprint;

class TextureCache
{
protected:
	TextureCacheMap _texCacheMap;		// Used to quickly find a texture item by using a key of type TextureCacheKey
	TextureCacheList _texCacheList;		// Used to sort existing texture items for various operations
	size_t _actualCacheSize;
	size_t _cacheSizeThreshold;
	u8 _paletteDump[PALETTE_DUMP_SIZE];
	
public:
	TextureCache();
	
	size_t GetActualCacheSize() const;
	size_t GetCacheSizeThreshold() const;
	void SetCacheSizeThreshold(size_t newThreshold);
	
	void Invalidate();
	void Evict();
	void Reset();
	void ForceReloadAllTextures();
	
	TextureStore* GetTexture(TEXIMAGE_PARAM texAttributes, u32 palAttributes);
	
	void Add(TextureStore *texItem);
	void Remove(TextureStore *texItem);
	
	static TextureCacheKey GenerateKey(const TEXIMAGE_PARAM texAttributes, const u32 palAttributes);
};

class TextureStore
{
protected:
	TEXIMAGE_PARAM _textureAttributes;
	u32 _paletteAttributes;
	
	u32 _sizeS;
	u32 _sizeT;
	bool _isPalZeroTransparent;
	
	NDSTextureFormat _packFormat;
	u32 _packAddress;
	u32 _packSize;
	u8 *_packData;
	
	u32 _paletteAddress;
	u32 _paletteSize;
	u16 *_paletteColorTable;
	
	// Only used by 4x4 formatted textures
	u32 _packIndexAddress;
	u32 _packIndexSize;
	u8 *_packIndexData;
	u32 _packSizeFirstSlot;
	
	size_t _packTotalSize;
	
	bool _suspectedInvalid;
	bool _assumedInvalid;
	bool _isLoadNeeded;
	u8 *_workingData;
	
	TextureCacheKey _cacheKey;
	size_t _cacheSize;
	size_t _cacheAge; // A value of 0 means the texture was just used. The higher this value, the older the texture.
	size_t _cacheUsageCount;
	
public:
	TextureStore();
	TextureStore(const TEXIMAGE_PARAM texAttributes, const u32 palAttributes);
	virtual ~TextureStore();
	
	TEXIMAGE_PARAM GetTextureAttributes() const;
	u32 GetPaletteAttributes() const;
	
	u32 GetWidth() const;
	u32 GetHeight() const;
	bool IsPalZeroTransparent() const;
	
	NDSTextureFormat GetPackFormat() const;
	u32 GetPackAddress() const;
	u32 GetPackSize() const;
	u8* GetPackData();
	
	u32 GetPaletteAddress() const;
	u32 GetPaletteSize() const;
	u16* GetPaletteColorTable() const;
	
	u32 GetPackIndexAddress() const;
	u32 GetPackIndexSize() const;
	u8* GetPackIndexData();
	
	void SetTextureData(const MemSpan &packedData, const MemSpan &packedIndexData);
	void SetTexturePalette(const MemSpan &packedPalette);
	void SetTexturePalette(const u16 *paletteBuffer);
	
	size_t GetUnpackSizeUsingFormat(const TextureStoreUnpackFormat texCacheFormat) const;
	template<TextureStoreUnpackFormat TEXCACHEFORMAT> void Unpack(u32 *unpackBuffer);
	
	virtual void Load(void *targetBuffer);
	
	bool IsSuspectedInvalid() const;
	void SetSuspectedInvalid();
	
	bool IsAssumedInvalid() const;
	void SetAssumedInvalid();
	
	void SetLoadNeeded();
	bool IsLoadNeeded() const;
	
	TextureCacheKey GetCacheKey() const;
	
	size_t GetCacheSize() const;
	void SetCacheSize(size_t cacheSize);
	
	size_t GetCacheAge() const;
	void IncreaseCacheAge(const size_t ageAmount);
	void ResetCacheAge();
	
	size_t GetCacheUseCount() const;
	void IncreaseCacheUsageCount(const size_t usageCount);
	void ResetCacheUsageCount();
	
	void Update();
	void VRAMCompareAndUpdate();
	void DebugDump();
};

template<TextureStoreUnpackFormat TEXCACHEFORMAT> void NDSTextureUnpackI2(const size_t srcSize, const u8 *__restrict srcData, const u16 *__restrict srcPal, const bool isPalZeroTransparent, u32 *__restrict dstBuffer);
template<TextureStoreUnpackFormat TEXCACHEFORMAT> void NDSTextureUnpackI4(const size_t srcSize, const u8 *__restrict srcData, const u16 *__restrict srcPal, const bool isPalZeroTransparent, u32 *__restrict dstBuffer);
template<TextureStoreUnpackFormat TEXCACHEFORMAT> void NDSTextureUnpackI8(const size_t srcSize, const u8 *__restrict srcData, const u16 *__restrict srcPal, const bool isPalZeroTransparent, u32 *__restrict dstBuffer);
template<TextureStoreUnpackFormat TEXCACHEFORMAT> void NDSTextureUnpackA3I5(const size_t srcSize, const u8 *__restrict srcData, const u16 *__restrict srcPal, u32 *__restrict dstBuffer);
template<TextureStoreUnpackFormat TEXCACHEFORMAT> void NDSTextureUnpackA5I3(const size_t srcSize, const u8 *__restrict srcData, const u16 *__restrict srcPal, u32 *__restrict dstBuffer);
template<TextureStoreUnpackFormat TEXCACHEFORMAT> void NDSTextureUnpack4x4(const size_t srcSize, const u32 *__restrict srcData, const u16 *__restrict srcIndex, const u32 palAddress, const u32 sizeX, const u32 sizeY, u32 *__restrict dstBuffer);
template<TextureStoreUnpackFormat TEXCACHEFORMAT> void NDSTextureUnpackDirect16Bit(const size_t srcSize, const u16 *__restrict srcData, u32 *__restrict dstBuffer);

extern TextureCache texCache;

#endif
