#ifndef _TEXCACHE_H_
#define _TEXCACHE_H_

#include "common.h"

enum TexCache_TexFormat
{
	TexFormat_None, //used when nothing yet is cached
	TexFormat_32bpp, //used by ogl renderer
	TexFormat_15bpp //used by rasterizer
};

class TexCacheItem
{
public:
	TexCacheItem() 
		: decoded(NULL)
		, decode_len(0)
		, next(NULL)
		, prev(NULL)
		, lockCount(0)
		, cacheFormat(TexFormat_None)
		, deleteCallback(NULL)
		, suspectedInvalid(false)
	{}
	~TexCacheItem() {
		delete[] decoded;
		if(deleteCallback) deleteCallback(this);
	}
	void unlock() { 
		lockCount--;
	}
	void lock() { 
		lockCount++;
	}
	u32 decode_len;
	u32 mode;
	u8* decoded; //decoded texture data
	TexCacheItem *next, *prev; //double linked list
	int lockCount;
	bool suspectedInvalid;

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
