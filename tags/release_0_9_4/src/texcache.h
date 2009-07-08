#ifndef _TEXCACHE_H_
#define _TEXCACHE_H_

#include "common.h"

enum TexCache_TexFormat
{
	TexFormat_32bpp,
	TexFormat_15bpp
};

#define MAX_TEXTURE 500
#ifndef NOSSE2
struct ALIGN(16) TextureCache
#else
struct ALIGN(8) TextureCache
#endif
{
	u32					id;
	u32					frm;
	u32					mode;
	u32					pal;
	u32					sizeX;
	u32					sizeY;
	float				invSizeX;
	float				invSizeY;

	struct {
	int					textureSize, indexSize;
	u8					texture[128*1024]; // 128Kb texture slot
	u8					palette[256*2];
	} dump;

	//set if this texture is suspected be invalid due to a vram reconfigure
	bool				suspectedInvalid;

};

extern TextureCache	*texcache;

extern void (*TexCache_BindTexture)(u32 texnum);
extern void (*TexCache_BindTextureData)(u32 texnum, u8* data);

void TexCache_Reset();

template<TexCache_TexFormat format>
void TexCache_SetTexture(u32 format, u32 texpal);

void TexCache_Invalidate();

extern u8 *TexCache_texMAP;
TextureCache* TexCache_Curr();

#endif
