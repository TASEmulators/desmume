/*
	Copyright (C) 2006-2007 shash
	Copyright (C) 2007-2015 DeSmuME team

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

#ifndef RENDER3D_H
#define RENDER3D_H

#include "gfx3d.h"
#include "types.h"

#define kUnsetTranslucentPolyID 255

class Render3D;

typedef struct Render3DInterface
{
	const char *name;				// The name of the renderer.
	Render3D* (*NDS_3D_Init)();		// Called when the renderer is created.
	void (*NDS_3D_Close)();			// Called when the renderer is destroyed.
	
} GPU3DInterface;

extern int cur3DCore;

// gpu 3D core list, per port
extern GPU3DInterface *core3DList[];

// Default null plugin
#define GPU3D_NULL 0
extern GPU3DInterface gpu3DNull;

// Extern pointer
extern Render3D *BaseRenderer;
extern Render3D *CurrentRenderer;
extern GPU3DInterface *gpu3D;

Render3D* Render3DBaseCreate();
void Render3DBaseDestroy();

void Render3D_Init();
void Render3D_DeInit();
bool NDS_3D_ChangeCore(int newCore);

enum RendererID
{
	RENDERID_NULL				= 0,
	RENDERID_SOFTRASTERIZER		= 1,
	RENDERID_OPENGL_AUTO		= 1000,
	RENDERID_OPENGL_LEGACY		= 1001,
	RENDERID_OPENGL_3_2			= 1002
};

enum Render3DErrorCode
{
	RENDER3DERROR_NOERR = 0
};

typedef int Render3DError;

struct FragmentAttributes
{
	u32 depth;
	u8 opaquePolyID;
	u8 translucentPolyID;
	u8 stencil;
	bool isFogged;
	bool isTranslucentPoly;
};

class Render3D
{
protected:
	RendererID _renderID;
	std::string _renderName;
	
	size_t _framebufferWidth;
	size_t _framebufferHeight;
	size_t _framebufferColorSizeBytes;
	FragmentColor *_framebufferColor;
	
	CACHE_ALIGN u16 clearImageColor16Buffer[GFX3D_FRAMEBUFFER_WIDTH * GFX3D_FRAMEBUFFER_HEIGHT];
	CACHE_ALIGN u32 clearImageDepthBuffer[GFX3D_FRAMEBUFFER_WIDTH * GFX3D_FRAMEBUFFER_HEIGHT];
	CACHE_ALIGN bool clearImageFogBuffer[GFX3D_FRAMEBUFFER_WIDTH * GFX3D_FRAMEBUFFER_HEIGHT];
	CACHE_ALIGN u8 clearImagePolyIDBuffer[GFX3D_FRAMEBUFFER_WIDTH * GFX3D_FRAMEBUFFER_HEIGHT];
	
	virtual Render3DError BeginRender(const GFX3D &engine);
	virtual Render3DError RenderGeometry(const GFX3D_State &renderState, const POLYLIST *polyList, const INDEXLIST *indexList);
	virtual Render3DError RenderEdgeMarking(const u16 *colorTable, const bool useAntialias);
	virtual Render3DError RenderFog(const u8 *densityTable, const u32 color, const u32 offset, const u8 shift, const bool alphaOnly);
	virtual Render3DError EndRender(const u64 frameCount);
	virtual Render3DError FlushFramebuffer(FragmentColor *dstRGBA6665, u16 *dstRGBA5551);
	
	virtual Render3DError ClearUsingImage(const u16 *__restrict colorBuffer, const u32 *__restrict depthBuffer, const bool *__restrict fogBuffer, const u8 *__restrict polyIDBuffer);
	virtual Render3DError ClearUsingValues(const FragmentColor &clearColor, const FragmentAttributes &clearAttributes) const;
	
	virtual Render3DError SetupPolygon(const POLY &thePoly);
	virtual Render3DError SetupTexture(const POLY &thePoly, bool enableTexturing);
	virtual Render3DError SetupViewport(const u32 viewportValue);
	
public:
	Render3D();
	~Render3D();
	
	RendererID GetRenderID();
	std::string GetName();
	
	FragmentColor* GetFramebuffer();
	size_t GetFramebufferWidth();
	size_t GetFramebufferHeight();
	
	virtual Render3DError UpdateToonTable(const u16 *toonTableBuffer);
	virtual Render3DError ClearFramebuffer(const GFX3D_State &renderState);
	
	virtual Render3DError Reset();						// Called when the emulator resets.
	
	virtual Render3DError Render(const GFX3D &engine);	// Called when the renderer should do its job and render the current display lists.
	
	virtual Render3DError RenderFinish();				// Called whenever 3D rendering needs to finish. This function should block the calling thread
														// and only release the block when 3D rendering is finished. (Before reading the 3D layer, be
														// sure to always call this function.)
	
	virtual Render3DError VramReconfigureSignal();		// Called when the emulator reconfigures its VRAM. You may need to invalidate your texture cache.
	
	virtual Render3DError SetFramebufferSize(size_t w, size_t h);	// Called whenever the output framebuffer size changes.
};

#endif
