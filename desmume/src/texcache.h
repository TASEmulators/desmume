/*
	Copyright (C) 2006 yopyop
	Copyright (C) 2006-2007 shash
	Copyright (C) 2008-2015 DeSmuME team

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

enum TexCache_TexFormat
{
	TexFormat_None, //used when nothing yet is cached
	TexFormat_32bpp, //used by ogl renderer
	TexFormat_15bpp //used by rasterizer
};

class TexCacheItem;

typedef std::multimap<u32,TexCacheItem*> TTexCacheItemMultimap;
typedef void (*TexCacheItemDeleteCallback)(TexCacheItem *texItem, void *param1, void *param2);

class TexCacheItem
{
private:
	TexCacheItemDeleteCallback _deleteCallback;
	void *_deleteCallbackParam1;
	void *_deleteCallbackParam2;
	
public:
	TexCacheItem() 
		: decode_len(0)
		, decoded(NULL)
		, suspectedInvalid(false)
		, assumedInvalid(false)
		, _deleteCallback(NULL)
		, _deleteCallbackParam1(NULL)
		, _deleteCallbackParam2(NULL)
		, cacheFormat(TexFormat_None)
	{}
	
	~TexCacheItem()
	{
		delete[] decoded;
		if(_deleteCallback != NULL) _deleteCallback(this, this->_deleteCallbackParam1, this->_deleteCallbackParam2);
	}
	u32 decode_len;
	u32 mode;
	u8* decoded; //decoded texture data
	bool suspectedInvalid;
	bool assumedInvalid;
	TTexCacheItemMultimap::iterator iterator;

	int getTextureMode() const { return (int)((texformat>>26)&0x07); }

	u32 texformat, texpal;
	u32 sizeX, sizeY;
	float invSizeX, invSizeY;

	u64 texid; //used by ogl renderer for the texid
	TexCache_TexFormat cacheFormat;

	struct Dump {
		~Dump() {
			delete[] texture;
		}
		int textureSize, indexSize;
		static const int maxTextureSize=128*1024;
		u8* texture;
		u8 palette[256*2];
	} dump;
	
	TexCacheItemDeleteCallback GetDeleteCallback()
	{
		return this->_deleteCallback;
	}
	
	void SetDeleteCallback(TexCacheItemDeleteCallback callbackFunc, void *inParam1, void *inParam2)
	{
		this->_deleteCallback = callbackFunc;
		this->_deleteCallbackParam1 = inParam1;
		this->_deleteCallbackParam2 = inParam2;
	}
};

void TexCache_Invalidate();
void TexCache_Reset();
void TexCache_EvictFrame();

TexCacheItem* TexCache_SetTexture(TexCache_TexFormat TEXFORMAT, u32 format, u32 texpal);

#endif
