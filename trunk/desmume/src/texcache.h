#ifndef _TEXCACHE_H_
#define _TEXCACHE_H_

#include "common.h"

#define MAX_TEXTURE 500
#ifdef SSE2
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
	int					textureSize, indexSize;
	u8					texture[128*1024]; // 128Kb texture slot
	u8					palette[256*2];

	//set if this texture is suspected be invalid due to a vram reconfigure
	bool				suspectedInvalid;

};

extern TextureCache	texcache[MAX_TEXTURE+1];

extern void (*TexCache_BindTexture)(u32 texnum);
extern void (*TexCache_BindTextureData)(u32 texnum, u8* data);

void TexCache_Reset();
void TexCache_SetTexture(unsigned int format, unsigned int texpal);
void TexCache_Invalidate();

extern u8 TexCache_texMAP[1024*2048*4]; 
TextureCache* TexCache_Curr();

#endif
