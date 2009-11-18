#ifndef _TEXCACHE_H_
#define _TEXCACHE_H_

#include "common.h"
#include <map>

enum TexCache_TexFormat
{
	TexFormat_None, //used when nothing yet is cached
	TexFormat_32bpp, //used by ogl renderer
	TexFormat_15bpp //used by rasterizer
};

class TexCacheItem;

typedef std::multimap<u32,TexCacheItem*> TTexCacheItemMultimap;

class TexCacheItem
{
public:
	TexCacheItem() 
		: decode_len(0)
		, decoded(NULL)
		, suspectedInvalid(false)
		, deleteCallback(NULL)
		, cacheFormat(TexFormat_None)
	{}
	~TexCacheItem() {
		delete[] decoded;
		if(deleteCallback) deleteCallback(this);
	}
	u32 decode_len;
	u32 mode;
	u8* decoded; //decoded texture data
	bool suspectedInvalid;
	TTexCacheItemMultimap::iterator iterator;

	u32 texformat, texpal;
	u32 sizeX, sizeY;
	float invSizeX, invSizeY;

	u64 texid; //used by ogl renderer for the texid
	void (*deleteCallback)(TexCacheItem*);

	TexCache_TexFormat cacheFormat;

	//TODO - this is a little wasteful
	struct {
		int					textureSize, indexSize;
		u8					texture[128*1024]; // 128Kb texture slot
		u8					palette[256*2];
	} dump;
};

void TexCache_Invalidate();
void TexCache_Reset();
void TexCache_EvictFrame();

TexCacheItem* TexCache_SetTexture(TexCache_TexFormat TEXFORMAT, u32 format, u32 texpal);

#endif
