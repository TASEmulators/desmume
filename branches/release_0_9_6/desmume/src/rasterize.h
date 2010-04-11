/*  Copyright 2009 DeSmuME team

    This file is part of DeSmuME

    DeSmuME is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    DeSmuME is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with DeSmuME; if not, write to the Free Software
    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301  USA
*/

#ifndef _RASTERIZE_H_
#define _RASTERIZE_H_

#include "render3D.h"
#include "gfx3d.h"

extern GPU3DInterface gpu3DRasterize;

union FragmentColor {
	u32 color;
	struct {
#ifdef WORDS_BIGENDIAN
		u8 a,b,g,r;
#else
		u8 r,g,b,a;
#endif
	};
};

struct Fragment
{
	u32 depth;

	struct
	{
		u8 opaque, translucent;
	} polyid;

	u8 stencil;

	struct
	{
		u8 isTranslucentPoly:1;
		u8 fogged:1;
	};
};

class TexCacheItem;

class SoftRasterizerEngine
{
public:
	//debug:
	int _debug_drawClippedUserPoly;

	SoftRasterizerEngine();
	
	void initFramebuffer(const int width, const int height, const bool clearImage);
	void framebufferProcess();
	void updateToonTable();
	void updateFogTable();
	void updateFloatColors();
	void performClipping(bool hirez);
	template<bool CUSTOM> void performViewportTransforms(int width, int height);
	void performCoordAdjustment(const bool skipBackfacing);
	void performBackfaceTests();
	void setupTextures(const bool skipBackfacing);

	FragmentColor toonTable[32];
	u8 fogTable[32768];
	GFX3D_Clipper clipper;
	GFX3D_Clipper::TClippedPoly *clippedPolys;
	int clippedPolyCounter;
	TexCacheItem* polyTexKeys[POLYLIST_SIZE];
	bool polyVisible[POLYLIST_SIZE];
	bool polyBackfacing[POLYLIST_SIZE];
	Fragment *screen;
	FragmentColor *screenColor;
	POLYLIST* polylist;
	VERTLIST* vertlist;
	INDEXLIST* indexlist;
	int width, height;
};


#endif
