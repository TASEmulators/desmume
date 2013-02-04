/*
	Copyright (C) 2006-2007 shash
	Copyright (C) 2007-2013 DeSmuME team

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

//not using this right now
#define CALL_CONVENTION

typedef struct Render3DInterface
{
	// The name of the plugin, this name will appear in the plugins list
	const char * name;

	//called once when the plugin starts up
	char (CALL_CONVENTION*  NDS_3D_Init)					();
	
	//called when the emulator resets (is this necessary?)
	void (CALL_CONVENTION*  NDS_3D_Reset)					();
	
	//called when the plugin shuts down
	void (CALL_CONVENTION*  NDS_3D_Close)					();
	
	//called when the renderer should do its job and render the current display lists
	void (CALL_CONVENTION*  NDS_3D_Render)					();
	
	// Called whenever 3D rendering needs to finish. This function should block the calling thread
	// and only release the block when 3D rendering is finished. (Before reading the 3D layer, be
	// sure to always call this function.)
	void (CALL_CONVENTION*	NDS_3D_RenderFinish)			();

	//called when the emulator reconfigures its vram. you may need to invalidate your texture cache.
	void (CALL_CONVENTION*  NDS_3D_VramReconfigureSignal)	();

} GPU3DInterface;

extern int cur3DCore;

// gpu 3D core list, per port
extern GPU3DInterface *core3DList[];

// Default null plugin
#define GPU3D_NULL 0
extern GPU3DInterface gpu3DNull;

// Extern pointer
extern GPU3DInterface *gpu3D;

char Default3D_Init();
void Default3D_Reset();
void Default3D_Close();
void Default3D_Render();
void Default3D_RenderFinish();
void Default3D_VramReconfigureSignal();

void NDS_3D_SetDriver (int core3DIndex);
bool NDS_3D_ChangeCore(int newCore);

enum Render3DErrorCode
{
	RENDER3DERROR_NOERR = 0
};

typedef int Render3DError;

class Render3D
{
protected:
	virtual Render3DError BeginRender(const GFX3D_State *renderState);
	virtual Render3DError PreRender(const GFX3D_State *renderState, const VERTLIST *vertList, const POLYLIST *polyList, const INDEXLIST *indexList);
	virtual Render3DError DoRender(const GFX3D_State *renderState, const VERTLIST *vertList, const POLYLIST *polyList, const INDEXLIST *indexList);
	virtual Render3DError PostRender();
	virtual Render3DError EndRender(const u64 frameCount);
	
	virtual Render3DError UpdateClearImage(const u16 *__restrict colorBuffer, const u16 *__restrict depthBuffer, const u8 clearStencil, const u8 xScroll, const u8 yScroll);
	virtual Render3DError UpdateToonTable(const u16 *toonTableBuffer);
	
	virtual Render3DError ClearFramebuffer(const GFX3D_State *renderState);
	virtual Render3DError ClearUsingImage() const;
	virtual Render3DError ClearUsingValues(const u8 r, const u8 g, const u8 b, const u8 a, const u32 clearDepth, const u8 clearStencil) const;
	
	virtual Render3DError SetupPolygon(const POLY *thePoly);
	virtual Render3DError SetupTexture(const POLY *thePoly, bool enableTexturing);
	virtual Render3DError SetupViewport(const POLY *thePoly);
	
public:
	virtual Render3DError Reset();
	virtual Render3DError Render(const GFX3D_State *renderState, const VERTLIST *vertList, const POLYLIST *polyList, const INDEXLIST *indexList, const u64 frameCount);
	virtual Render3DError RenderFinish();
	virtual Render3DError VramReconfigureSignal();
};

#endif
 
