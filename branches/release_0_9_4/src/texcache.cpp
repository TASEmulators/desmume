#include "texcache.h"

#include <string.h>
#include <algorithm>

#include "bits.h"
#include "common.h"
#include "debug.h"
#include "gfx3d.h"
#include "NDSSystem.h"

using std::min;
using std::max;

//only dump this from ogl renderer. for now, softrasterizer creates things in an incompatible pixel format
//#define DEBUG_DUMP_TEXTURE

//This class represents a number of regions of memory which should be viewed as contiguous
class MemSpan
{
public:
	static const int MAXSIZE = 8;

	MemSpan() 
		: numItems(0)
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
		u8* ptr = ARM9Mem.texInfo.textureSlotAddr[slot];
		
		if(ptr == ARM9Mem.blank_memory) {
			PROGINFO("Tried to reference unmapped texture memory: slot %d\n",slot);
		}
		curr.ptr = ptr + curr.start;
	}
	return ret;
}

//creates a MemSpan in texture palette memory
static MemSpan MemSpan_TexPalette(u32 ofs, u32 len) 
{
	MemSpan ret;
	ret.size = len;
	u32 currofs = 0;
	while(len) {
		MemSpan::Item &curr = ret.items[ret.numItems++];
		curr.start = ofs&0x3FFF;
		u32 slot = (ofs>>14)&7; //this masks to 8 slots, but there are really only 6
		if(slot>5) {
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
		u8* ptr = ARM9Mem.texInfo.texPalSlot[slot];
		
		if(ptr == ARM9Mem.blank_memory) {
			PROGINFO("Tried to reference unmapped texture palette memory: 16k slot #%d\n",slot);
		}
		curr.ptr = ptr + curr.start;
	}
	return ret;
}

TextureCache *texcache;
u32 texcache_start;
u32 texcache_stop;
u8 *TexCache_texMAP = NULL;


#if defined (DEBUG_DUMP_TEXTURE) && defined (WIN32)
#define DO_DEBUG_DUMP_TEXTURE
static void DebugDumpTexture(int which)
{
	char fname[100];
	sprintf(fname,"c:\\dump\\%d.bmp", which);

	NDS_WriteBMP_32bppBuffer(texcache[which].sizeX,texcache[which].sizeY,TexCache_texMAP,fname);
}
#endif


static int lastTexture = -1;

#define CONVERT(color,alpha) ((TEXFORMAT == TexFormat_32bpp)?(RGB15TO32(color,alpha)):RGB15TO5555(color,alpha))

template<TexCache_TexFormat TEXFORMAT>
void TexCache_SetTexture(u32 format, u32 texpal)
{
	//for each texformat, number of palette entries
	const int palSizes[] = {0, 32, 4, 16, 256, 0, 8, 0};

	//for each texformat, multiplier from numtexels to numbytes (fixed point 30.2)
	const int texSizes[] = {0, 4, 1, 2, 4, 1, 4, 8};

	//used to hold a copy of the palette specified for this texture
	u16 pal[256];

	u32 *dwdst = (u32*)TexCache_texMAP;
	
	u32 textureMode = (unsigned short)((format>>26)&0x07);
	unsigned int sizeX=(8 << ((format>>20)&0x07));
	unsigned int sizeY=(8 << ((format>>23)&0x07));
	unsigned int imageSize = sizeX*sizeY;

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
	MemSpan mspal = MemSpan_TexPalette(paletteAddress,palSize*2);

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


	u32 tx=texcache_start;

	//if(false)
	while (TRUE)
	{
		//conditions where we give up and regenerate the texture:
		if (texcache_stop == tx) break;
		if (texcache[tx].frm == 0) break;

		//conditions where we reject matches:
		//when the teximage or texpal params dont match 
		//(this is our key for identifying palettes in the cache)
		if (texcache[tx].frm != format) goto REJECT;
		if (texcache[tx].pal != texpal) goto REJECT;

		//the texture matches params, but isnt suspected invalid. accept it.
		if (!texcache[tx].suspectedInvalid) goto ACCEPT;

		//if we couldnt cache this entire texture due to it being too large, then reject it
		if (texSize+indexSize > (int)sizeof(texcache[tx].dump.texture)) goto REJECT;

		//when the palettes dont match:
		//note that we are considering 4x4 textures to have a palette size of 0.
		//they really have a potentially HUGE palette, too big for us to handle like a normal palette,
		//so they go through a different system
		if (mspal.size != 0 && memcmp(texcache[tx].dump.palette,pal,mspal.size)) goto REJECT;

		//when the texture data doesn't match
		if(ms.memcmp(texcache[tx].dump.texture,sizeof(texcache[tx].dump.texture))) goto REJECT;

		//if the texture is 4x4 then the index data must match
		if(textureMode == TEXMODE_4X4)
		{
			if(msIndex.memcmp(texcache[tx].dump.texture + texcache[tx].dump.textureSize,texcache[tx].dump.indexSize)) goto REJECT; 
		}


ACCEPT:
		texcache[tx].suspectedInvalid = false;
		if(lastTexture == -1 || (int)tx != lastTexture)
		{
			lastTexture = tx;
			if(TexCache_BindTexture)
				TexCache_BindTexture(tx);
		}
		return;
 
REJECT:
		tx++;
		if ( tx > MAX_TEXTURE )
		{
			texcache_stop=texcache_start;
			texcache[texcache_stop].frm=0;
			texcache_start++;
			if (texcache_start>MAX_TEXTURE) 
			{
				texcache_start=0;
				texcache_stop=MAX_TEXTURE<<1;
			}
			tx=0;
		}
	}

	lastTexture = tx;
	//glBindTexture(GL_TEXTURE_2D, texcache[tx].id);

	texcache[tx].suspectedInvalid = false;
	texcache[tx].frm=format;
	texcache[tx].mode=textureMode;
	texcache[tx].pal=texpal;
	texcache[tx].sizeX=sizeX;
	texcache[tx].sizeY=sizeY;
	texcache[tx].invSizeX=1.0f/((float)(sizeX));
	texcache[tx].invSizeY=1.0f/((float)(sizeY));
	texcache[tx].dump.textureSize = ms.dump(texcache[tx].dump.texture,sizeof(texcache[tx].dump.texture));

	//dump palette data for cache keying
	if ( palSize )
	{
		memcpy(texcache[tx].dump.palette, pal, palSize*2);
	}
	//dump 4x4 index data for cache keying
	texcache[tx].dump.indexSize = 0;
	if(textureMode == TEXMODE_4X4)
	{
		texcache[tx].dump.indexSize = min(msIndex.size,(int)sizeof(texcache[tx].dump.texture) - texcache[tx].dump.textureSize);
		msIndex.dump(texcache[tx].dump.texture+texcache[tx].dump.textureSize,texcache[tx].dump.indexSize);
	}


	//INFO("Texture %03i - format=%08X; pal=%04X (mode %X, width %04i, height %04i)\n",i, texcache[i].frm, texcache[i].pal, texcache[i].mode, sizeX, sizeY);

	//============================================================================ Texture conversion
	const u32 opaqueColor = TEXFORMAT==TexFormat_32bpp?255:31;
	u32 palZeroTransparent = (1-((format>>29)&1))*opaqueColor;

	switch (texcache[tx].mode)
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
						*dwdst++ = RGB15TO5555(c,material_3bit_to_5bit[alpha]);
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

#define PAL4X4(offset) ( *(u16*)( ARM9Mem.texInfo.texPalSlot[((paletteAddress + (offset)*2)>>14)] + ((paletteAddress + (offset)*2)&0x3FFF) ) )

			u16* slot1;
			u32* map = (u32*)ms.items[0].ptr;
			u32 limit = ms.items[0].len<<2;
			u32 d = 0;
			if ( (texcache[tx].frm & 0xc000) == 0x8000)
				// texel are in slot 2
				slot1=(u16*)&ARM9Mem.texInfo.textureSlotAddr[1][((texcache[tx].frm & 0x3FFF)<<2)+0x010000];
			else 
				slot1=(u16*)&ARM9Mem.texInfo.textureSlotAddr[1][(texcache[tx].frm & 0x3FFF)<<2];

			u16 yTmpSize = (texcache[tx].sizeY>>2);
			u16 xTmpSize = (texcache[tx].sizeX>>2);

			//this is flagged whenever a 4x4 overruns its slot.
			//i am guessing we just generate black in that case
			bool dead = false;

			for (int y = 0; y < yTmpSize; y ++)
			{
				u32 tmpPos[4]={(y<<2)*texcache[tx].sizeX,((y<<2)+1)*texcache[tx].sizeX,
					((y<<2)+2)*texcache[tx].sizeX,((y<<2)+3)*texcache[tx].sizeX};
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
							tmp_col[i] >>= 3;
							tmp_col[i] &= 0x1F1F1F1F;
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
						*dwdst++ = RGB15TO5555(c,alpha);
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
				for(u32 x = 0; x < len; ++x)
				{
					u16 c = map[x];
					int alpha = ((c&0x8000)?opaqueColor:0);
					*dwdst++ = CONVERT(c&0x7FFF,alpha);
				}
			}
			break;
		}
	}

	if(TexCache_BindTextureData != 0)
		TexCache_BindTextureData(tx,TexCache_texMAP);

#ifdef DO_DEBUG_DUMP_TEXTURE
	DebugDumpTexture(tx);
#endif

}

void TexCache_Reset()
{
	if(TexCache_texMAP == NULL) TexCache_texMAP = new u8[1024*2048*4]; 
	if(texcache == NULL) texcache = new TextureCache[MAX_TEXTURE+1];

	memset(texcache,0,sizeof(TextureCache[MAX_TEXTURE+1]));

	texcache_start=0;
	texcache_stop=MAX_TEXTURE<<1;
}

TextureCache* TexCache_Curr()
{
	if(lastTexture == -1)
		return NULL;
	else return &texcache[lastTexture];
}

void TexCache_Invalidate()
{
	//well, this is a very blunt instrument.
	//lets just flag all the textures as invalid.
	for(int i=0;i<MAX_TEXTURE+1;i++) {
		texcache[i].suspectedInvalid = true;

		//invalidate all 4x4 textures when texture palettes change mappings
		//this is necessary because we arent tracking 4x4 texture palettes to look for changes.
		//Although I concede this is a bit paranoid.. I think the odds of anyone changing 4x4 palette data
		//without also changing the texture data is pretty much zero.
		//
		//TODO - move this to a separate signal: split into TexReconfigureSignal and TexPaletteReconfigureSignal
		if(texcache[i].mode == TEXMODE_4X4)
			texcache[i].frm = 0;
	}
}

void (*TexCache_BindTexture)(u32 texnum) = NULL;
void (*TexCache_BindTextureData)(u32 texnum, u8* data);

//these templates needed to be instantiated manually
template void TexCache_SetTexture<TexFormat_32bpp>(u32 format, u32 texpal);
template void TexCache_SetTexture<TexFormat_15bpp>(u32 format, u32 texpal);
