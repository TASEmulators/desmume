/*
	Copyright (C) 2006 yopyop
	Copyright (C) 2006-2007 shash
	Copyright (C) 2008-2011 DeSmuME team

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
#include <map>

#include "texcache.h"

#include "bits.h"
#include "common.h"
#include "debug.h"
#include "gfx3d.h"
#include "NDSSystem.h"

using std::min;
using std::max;

//only dump this from ogl renderer. for now, softrasterizer creates things in an incompatible pixel format
//#define DEBUG_DUMP_TEXTURE

#define CONVERT(color,alpha) ((TEXFORMAT == TexFormat_32bpp)?(RGB15TO32(color,alpha)):RGB15TO6665(color,alpha))

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
	int dump(void* buf, int size=-1)
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
	int dump16(void* buf, int size=-1)
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
		if(ptr == MMU.blank_memory) {
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
		if(ptr == MMU.blank_memory && !silent) {
			PROGINFO("Tried to reference unmapped texture palette memory: 16k slot #%d\n",slot);
		}
		curr.ptr = ptr + curr.start;
	}
	return ret;
}

#if defined (DEBUG_DUMP_TEXTURE) && defined (WIN32)
#define DO_DEBUG_DUMP_TEXTURE
static void DebugDumpTexture(TexCacheItem* item)
{
	static int ctr=0;
	char fname[100];
	sprintf(fname,"c:\\dump\\%d.bmp", ctr);
	ctr++;

	NDS_WriteBMP_32bppBuffer(item->sizeX,item->sizeY,item->decoded,fname);
}
#endif

class TexCache
{
public:
	TexCache()
		: cache_size(0)
	{
		memset(paletteDump,0,sizeof(paletteDump));
	}

	TTexCacheItemMultimap index;

	//this ought to be enough for anyone
	//static const u32 kMaxCacheSize = 64*1024*1024; 
	//changed by zeromus on 15-dec. I couldnt find any games that were getting anywhere NEAR 64
	static const u32 kMaxCacheSize = 16*1024*1024; 
	//metal slug burns through sprites so fast, it can test it pretty quickly though

	//this is not really precise, it is off by a constant factor
	u32 cache_size;

	void list_remove(TexCacheItem* item)
	{
		index.erase(item->iterator);
		cache_size -= item->decode_len;
	}

	void list_push_front(TexCacheItem* item)
	{
		item->iterator = index.insert(std::make_pair(item->texformat,item));
		cache_size += item->decode_len;
	}

	template<TexCache_TexFormat TEXFORMAT>
	TexCacheItem* scan(u32 format, u32 texpal)
	{
		//for each texformat, number of palette entries
		static const int palSizes[] = {0, 32, 4, 16, 256, 0, 8, 0};

		//for each texformat, multiplier from numtexels to numbytes (fixed point 30.2)
		static const int texSizes[] = {0, 4, 1, 2, 4, 1, 4, 8};

		//used to hold a copy of the palette specified for this texture
		u16 pal[256];

		u32 textureMode = (unsigned short)((format>>26)&0x07);
		u32 sizeX=(8 << ((format>>20)&0x07));
		u32 sizeY=(8 << ((format>>23)&0x07));
		u32 imageSize = sizeX*sizeY;

		u8 *adr;

		u32 paletteAddress;

		switch (textureMode)
		{
		case TEXMODE_I2:
			paletteAddress = texpal<<3;
			break;
		case TEXMODE_A3I5: //a3i5
		case TEXMODE_I4: //i4
		case TEXMODE_I8: //i8
		case TEXMODE_A5I3: //a5i3
		case TEXMODE_16BPP: //16bpp
		case TEXMODE_4X4: //4x4
		default:
			paletteAddress = texpal<<4;
			break;
		}

		//analyze the texture memory mapping and the specifications of this texture
		int palSize = palSizes[textureMode];
		int texSize = (imageSize*texSizes[textureMode])>>2; //shifted because the texSizes multiplier is fixed point
		MemSpan ms = MemSpan_TexMem((format&0xFFFF)<<3,texSize);
		MemSpan mspal = MemSpan_TexPalette(paletteAddress,palSize*2,false);

		//determine the location for 4x4 index data
		u32 indexBase;
		if((format & 0xc000) == 0x8000) indexBase = 0x30000;
		else indexBase = 0x20000;

		u32 indexOffset = (format&0x3FFF)<<2;

		int indexSize = 0;
		MemSpan msIndex;
		if(textureMode == TEXMODE_4X4)
		{
			indexSize = imageSize>>3;
			msIndex = MemSpan_TexMem(indexOffset+indexBase,indexSize);
		}


		//dump the palette to a temp buffer, so that we don't have to worry about memory mapping.
		//this isnt such a problem with texture memory, because we read sequentially from it.
		//however, we read randomly from palette memory, so the mapping is more costly.
		#ifdef WORDS_BIGENDIAN
			mspal.dump16(pal);
		#else
			mspal.dump(pal);
		#endif

			//TODO - as a special optimization, keep the last item returned and check it first

		for(std::pair<TTexCacheItemMultimap::iterator,TTexCacheItemMultimap::iterator>
			iters = index.equal_range(format);
			iters.first != iters.second;
			++iters.first)
		{
			TexCacheItem* curr = iters.first->second;
			
			//conditions where we reject matches:
			//when the teximage or texpal params dont match 
			//(this is our key for identifying textures in the cache)
			//NEW: due to using format as a key we dont need to check this anymore
			//if(curr->texformat != format) continue;
			if(curr->texpal != texpal) continue;

			//we're being asked for a different format than what we had cached.
			//TODO - this could be done at the entire cache level instead of checking repeatedly
			if(curr->cacheFormat != TEXFORMAT) goto REJECT;

			//if the texture is assumed invalid, reject it
			if(curr->assumedInvalid) goto REJECT; 

			//the texture matches params, but isnt suspected invalid. accept it.
			if(!curr->suspectedInvalid) return curr;

			//we suspect the texture may be invalid. we need to do a byte-for-byte comparison to re-establish that it is valid:

			//when the palettes dont match:
			//note that we are considering 4x4 textures to have a palette size of 0.
			//they really have a potentially HUGE palette, too big for us to handle like a normal palette,
			//so they go through a different system
			if(mspal.size != 0 && memcmp(curr->dump.palette,pal,mspal.size)) goto REJECT;

			//when the texture data doesn't match
			if(ms.memcmp(&curr->dump.texture[0],curr->dump.textureSize)) goto REJECT;

			//if the texture is 4x4 then the index data must match
			if(textureMode == TEXMODE_4X4)
			{
				if(msIndex.memcmp(curr->dump.texture + curr->dump.textureSize,curr->dump.indexSize)) goto REJECT; 
			}

			//we found a match. just return it
			//REMINDER to make it primary/newest when we have smarter code
			//list_remove(curr);
			//list_push_front(curr);
			curr->suspectedInvalid = false;
			return curr;

		REJECT:
			//we found a cached item for the current address, but the data is stale.
			//for a variety of complicated reasons, we need to throw it out right this instant.
			list_remove(curr);
			delete curr;
			break;
		}

		//item was not found. recruit an existing one (the oldest), or create a new one
		//evict(); //reduce the size of the cache if necessary
		//TODO - as a peculiarity of the texcache, eviction must happen after the entire 3d frame runs
		//to support separate cache and read passes
		TexCacheItem* newitem = new TexCacheItem();
		newitem->suspectedInvalid = false;
		newitem->texformat = format;
		newitem->cacheFormat = TEXFORMAT;
		newitem->texpal = texpal;
		newitem->sizeX=sizeX;
		newitem->sizeY=sizeY;
		newitem->invSizeX=1.0f/((float)(sizeX));
		newitem->invSizeY=1.0f/((float)(sizeY));
		newitem->decode_len = sizeX*sizeY*4;
		newitem->mode = textureMode;
		newitem->decoded = new u8[newitem->decode_len];
		list_push_front(newitem);
		//printf("allocating: up to %d with %d items\n",cache_size,index.size());

		u32 *dwdst = (u32*)newitem->decoded;
		
		//dump palette data for cache keying
		if(palSize)
		{
			memcpy(newitem->dump.palette, pal, palSize*2);
		}

		//dump texture and 4x4 index data for cache keying
		const int texsize = newitem->dump.textureSize = ms.size;
		const int indexsize = newitem->dump.indexSize = msIndex.size;
		newitem->dump.texture = new u8[texsize+indexsize];
		ms.dump(&newitem->dump.texture[0],newitem->dump.maxTextureSize); //dump texture
		if(textureMode == TEXMODE_4X4)
			msIndex.dump(newitem->dump.texture+newitem->dump.textureSize,newitem->dump.indexSize); //dump 4x4


		//============================================================================ 
		//Texture conversion
		//============================================================================ 

		const u32 opaqueColor = TEXFORMAT==TexFormat_32bpp?255:31;
		u32 palZeroTransparent = (1-((format>>29)&1))*opaqueColor;

		switch (newitem->mode)
		{
		case TEXMODE_A3I5:
			{
				for(int j=0;j<ms.numItems;j++) {
					adr = ms.items[j].ptr;
					for(u32 x = 0; x < ms.items[j].len; x++)
					{
						u16 c = pal[*adr&31];
						u8 alpha = *adr>>5;
						if(TEXFORMAT == TexFormat_15bpp)
							*dwdst++ = RGB15TO6665(c,material_3bit_to_5bit[alpha]);
						else
							*dwdst++ = RGB15TO32(c,material_3bit_to_8bit[alpha]);
						adr++;
					}
				}
				break;
			}

		case TEXMODE_I2:
			{
				for(int j=0;j<ms.numItems;j++) {
					adr = ms.items[j].ptr;
					for(u32 x = 0; x < ms.items[j].len; x++)
					{
						u8 bits;
						u16 c;

						bits = (*adr)&0x3;
						c = pal[bits];
						*dwdst++ = CONVERT(c,(bits == 0) ? palZeroTransparent : opaqueColor);

						bits = ((*adr)>>2)&0x3;
						c = pal[bits];
						*dwdst++ = CONVERT(c,(bits == 0) ? palZeroTransparent : opaqueColor);

						bits = ((*adr)>>4)&0x3;
						c = pal[bits];
						*dwdst++ = CONVERT(c,(bits == 0) ? palZeroTransparent : opaqueColor);

						bits = ((*adr)>>6)&0x3;
						c = pal[bits];
						*dwdst++ = CONVERT(c,(bits == 0) ? palZeroTransparent : opaqueColor);

						adr++;
					}
				}
				break;
			}
		case TEXMODE_I4:
			{
				for(int j=0;j<ms.numItems;j++) {
					adr = ms.items[j].ptr;
					for(u32 x = 0; x < ms.items[j].len; x++)
					{
						u8 bits;
						u16 c;

						bits = (*adr)&0xF;
						c = pal[bits];
						*dwdst++ = CONVERT(c,(bits == 0) ? palZeroTransparent : opaqueColor);

						bits = ((*adr)>>4);
						c = pal[bits];
						*dwdst++ = CONVERT(c,(bits == 0) ? palZeroTransparent : opaqueColor);
						adr++;
					}
				}
				break;
			}
		case TEXMODE_I8:
			{
				for(int j=0;j<ms.numItems;j++) {
					adr = ms.items[j].ptr;
					for(u32 x = 0; x < ms.items[j].len; ++x)
					{
						u16 c = pal[*adr];
						*dwdst++ = CONVERT(c,(*adr == 0) ? palZeroTransparent : opaqueColor);
						adr++;
					}
				}
			}
			break;
		case TEXMODE_4X4:
			{
				//RGB16TO32 is used here because the other conversion macros result in broken interpolation logic

				if(ms.numItems != 1) {
					PROGINFO("Your 4x4 texture has overrun its texture slot.\n");
				}
				//this check isnt necessary since the addressing is tied to the texture data which will also run out:
				//if(msIndex.numItems != 1) PROGINFO("Your 4x4 texture index has overrun its slot.\n");

	#define PAL4X4(offset) ( *(u16*)( MMU.texInfo.texPalSlot[((paletteAddress + (offset)*2)>>14)&0x7] + ((paletteAddress + (offset)*2)&0x3FFF) ) )

				u16* slot1;
				u32* map = (u32*)ms.items[0].ptr;
				u32 limit = ms.items[0].len<<2;
				u32 d = 0;
				if ( (format & 0xc000) == 0x8000)
					// texel are in slot 2
					slot1=(u16*)&MMU.texInfo.textureSlotAddr[1][((format & 0x3FFF)<<2)+0x010000];
				else 
					slot1=(u16*)&MMU.texInfo.textureSlotAddr[1][(format & 0x3FFF)<<2];

				u16 yTmpSize = (sizeY>>2);
				u16 xTmpSize = (sizeX>>2);

				//this is flagged whenever a 4x4 overruns its slot.
				//i am guessing we just generate black in that case
				bool dead = false;

				for (int y = 0; y < yTmpSize; y ++)
				{
					u32 tmpPos[4]={(y<<2)*sizeX,((y<<2)+1)*sizeX,
						((y<<2)+2)*sizeX,((y<<2)+3)*sizeX};
					for (int x = 0; x < xTmpSize; x ++, d++)
					{
						if(d >= limit)
							dead = true;

						if(dead) {
							for (int sy = 0; sy < 4; sy++)
							{
								u32 currentPos = (x<<2) + tmpPos[sy];
								dwdst[currentPos] = dwdst[currentPos+1] = dwdst[currentPos+2] = dwdst[currentPos+3] = 0;
							}
							continue;
						}

						u32 currBlock	= map[d];
						u16 pal1		= slot1[d];
						u16 pal1offset	= (pal1 & 0x3FFF)<<1;
						u8  mode		= pal1>>14;
						u32 tmp_col[4];
						
						tmp_col[0]=RGB16TO32(PAL4X4(pal1offset),255);
						tmp_col[1]=RGB16TO32(PAL4X4(pal1offset+1),255);

						switch (mode) 
						{
						case 0:
							tmp_col[2]=RGB16TO32(PAL4X4(pal1offset+2),255);
							tmp_col[3]=RGB16TO32(0x7FFF,0);
							break;
						case 1:
							tmp_col[2]=(((tmp_col[0]&0xFF)+(tmp_col[1]&0xff))>>1)|
								(((tmp_col[0]&(0xFF<<8))+(tmp_col[1]&(0xFF<<8)))>>1)|
								(((tmp_col[0]&(0xFF<<16))+(tmp_col[1]&(0xFF<<16)))>>1)|
								(0xff<<24);
							tmp_col[3]=RGB16TO32(0x7FFF,0);
							break;
						case 2:
							tmp_col[2]=RGB16TO32(PAL4X4(pal1offset+2),255);
							tmp_col[3]=RGB16TO32(PAL4X4(pal1offset+3),255);
							break;
						case 3: 
							{
								u32 red1, red2;
								u32 green1, green2;
								u32 blue1, blue2;
								u16 tmp1, tmp2;

								red1=tmp_col[0]&0xff;
								green1=(tmp_col[0]>>8)&0xff;
								blue1=(tmp_col[0]>>16)&0xff;
								red2=tmp_col[1]&0xff;
								green2=(tmp_col[1]>>8)&0xff;
								blue2=(tmp_col[1]>>16)&0xff;

								tmp1=((red1*5+red2*3)>>6)|
									(((green1*5+green2*3)>>6)<<5)|
									(((blue1*5+blue2*3)>>6)<<10);
								tmp2=((red2*5+red1*3)>>6)|
									(((green2*5+green1*3)>>6)<<5)|
									(((blue2*5+blue1*3)>>6)<<10);

								tmp_col[2]=RGB16TO32(tmp1,255);
								tmp_col[3]=RGB16TO32(tmp2,255);
								break;
							}
						}

						if(TEXFORMAT==TexFormat_15bpp)
						{
							for(int i=0;i<4;i++)
							{
								tmp_col[i] >>= 2;
								tmp_col[i] &= 0x3F3F3F3F;
								u32 a = tmp_col[i]>>24;
								tmp_col[i] &= 0x00FFFFFF;
								tmp_col[i] |= (a>>1)<<24;
							}
						}

						//TODO - this could be more precise for 32bpp mode (run it through the color separation table)

						//set all 16 texels
						for (int sy = 0; sy < 4; sy++)
						{
							// Texture offset
							u32 currentPos = (x<<2) + tmpPos[sy];
							u8 currRow = (u8)((currBlock>>(sy<<3))&0xFF);

							dwdst[currentPos] = tmp_col[currRow&3];
							dwdst[currentPos+1] = tmp_col[(currRow>>2)&3];
							dwdst[currentPos+2] = tmp_col[(currRow>>4)&3];
							dwdst[currentPos+3] = tmp_col[(currRow>>6)&3];
						}


					}
				}


				break;
			}
		case TEXMODE_A5I3:
			{
				for(int j=0;j<ms.numItems;j++) {
					adr = ms.items[j].ptr;
					for(u32 x = 0; x < ms.items[j].len; ++x)
					{
						u16 c = pal[*adr&0x07];
						u8 alpha = (*adr>>3);
						if(TEXFORMAT == TexFormat_15bpp)
							*dwdst++ = RGB15TO6665(c,alpha);
						else
							*dwdst++ = RGB15TO32(c,material_5bit_to_8bit[alpha]);
						adr++;
					}
				}
				break;
			}
		case TEXMODE_16BPP:
			{
				for(int j=0;j<ms.numItems;j++) {
					u16* map = (u16*)ms.items[j].ptr;
					int len = ms.items[j].len>>1;
					for(int x = 0; x < len; ++x)
					{
						u16 c = map[x];
						int alpha = ((c&0x8000)?opaqueColor:0);
						*dwdst++ = CONVERT(c&0x7FFF,alpha);
					}
				}
				break;
			}
		} //switch(texture format)

#ifdef DO_DEBUG_DUMP_TEXTURE
	DebugDumpTexture(newitem);
#endif

		return newitem;
	} //scan()

	static const int PALETTE_DUMP_SIZE = (64+16+16)*1024;
	u8 paletteDump[PALETTE_DUMP_SIZE];

	void invalidate()
	{
		//check whether the palette memory changed
		//TODO - we should handle this instead by setting dirty flags in the vram memory mapping and noting whether palette memory was dirty.
		//but this will work for now
		MemSpan mspal = MemSpan_TexPalette(0,PALETTE_DUMP_SIZE,true);
		bool paletteDirty = mspal.memcmp(paletteDump);
		if(paletteDirty)
		{
			mspal.dump(paletteDump);
		}

		for(TTexCacheItemMultimap::iterator it(index.begin()); it != index.end(); ++it)
		{
			it->second->suspectedInvalid = true;
			
			//when the palette changes, we assume all 4x4 textures are dirty.
			//this is because each 4x4 item doesnt carry along with it a copy of the entire palette, for verification
			//instead, we just use the one paletteDump for verifying of all 4x4 textures; and if paletteDirty is set, verification has failed
			if(it->second->getTextureMode() == TEXMODE_4X4 && paletteDirty)
			{
				it->second->assumedInvalid = true;
			}
		}
	}

	void evict(u32 target = kMaxCacheSize)
	{
		//debug print
		//printf("%d %d/%d\n",index.size(),cache_size/1024,target/1024);

		//dont do anything unless we're over the target
		if(cache_size<target) return;

		//aim at cutting the cache to half of the max size
		target/=2;

		//evicts items in an arbitrary order until it is less than the max cache size
		//TODO - do this based on age and not arbitrarily
		while(cache_size > target)
		{
			if(index.size()==0) break; //just in case.. doesnt seem possible, cache_size wouldve been 0

			TexCacheItem* item = index.begin()->second;
			list_remove(item);
			//printf("evicting! totalsize:%d\n",cache_size);
			delete item;
		}
	}
} texCache;

void TexCache_Reset()
{
	texCache.evict(0);
}

void TexCache_Invalidate()
{
	//note that this gets called whether texdata or texpalette gets reconfigured.
	texCache.invalidate();
}

TexCacheItem* TexCache_SetTexture(TexCache_TexFormat TEXFORMAT, u32 format, u32 texpal)
{
	switch(TEXFORMAT)
	{
	case TexFormat_32bpp: return texCache.scan<TexFormat_32bpp>(format,texpal);
	case TexFormat_15bpp: return texCache.scan<TexFormat_15bpp>(format,texpal);
	default: assert(false); return NULL;
	}
}

//call this periodically to keep the tex cache clean
void TexCache_EvictFrame()
{
	texCache.evict();
}
