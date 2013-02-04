/*
	Copyright (C) 2006-2007 shash
	Copyright (C) 2008-2013 DeSmuME team

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

#include "render3D.h"
#include "gfx3d.h"
#include "MMU.h"
#include "texcache.h"

int cur3DCore = GPU3D_NULL;

GPU3DInterface gpu3DNull = { 
	"None",
	Default3D_Init,
	Default3D_Reset,
	Default3D_Close,
	Default3D_Render,
	Default3D_RenderFinish,
	Default3D_VramReconfigureSignal
};

GPU3DInterface *gpu3D = &gpu3DNull;
static bool default3DAlreadyClearedLayer = false;

char Default3D_Init()
{
	default3DAlreadyClearedLayer = false;
	
	return 1;
}

void Default3D_Reset()
{
	default3DAlreadyClearedLayer = false;
	
	TexCache_Reset();
}

void Default3D_Close()
{
	memset(gfx3d_convertedScreen, 0, sizeof(gfx3d_convertedScreen));
	default3DAlreadyClearedLayer = false;
}

void Default3D_Render()
{
	if (!default3DAlreadyClearedLayer)
	{
		memset(gfx3d_convertedScreen, 0, sizeof(gfx3d_convertedScreen));
		default3DAlreadyClearedLayer = true;
	}
}

void Default3D_RenderFinish()
{
	// Do nothing
}

void Default3D_VramReconfigureSignal()
{
	TexCache_Invalidate();
}

void NDS_3D_SetDriver (int core3DIndex)
{
	cur3DCore = core3DIndex;
	gpu3D = core3DList[cur3DCore];
}

bool NDS_3D_ChangeCore(int newCore)
{
	gpu3D->NDS_3D_Close();
	NDS_3D_SetDriver(newCore);
	if(gpu3D->NDS_3D_Init() == 0)
	{
		NDS_3D_SetDriver(GPU3D_NULL);
		gpu3D->NDS_3D_Init();
		return false;
	}
	return true;
}

Render3DError Render3D::BeginRender(const GFX3D_State *renderState)
{
	return RENDER3DERROR_NOERR;
}

Render3DError Render3D::PreRender(const GFX3D_State *renderState, const VERTLIST *vertList, const POLYLIST *polyList, const INDEXLIST *indexList)
{
	return RENDER3DERROR_NOERR;
}

Render3DError Render3D::DoRender(const GFX3D_State *renderState, const VERTLIST *vertList, const POLYLIST *polyList, const INDEXLIST *indexList)
{
	return RENDER3DERROR_NOERR;
}

Render3DError Render3D::PostRender()
{
	return RENDER3DERROR_NOERR;
}

Render3DError Render3D::EndRender(const u64 frameCount)
{
	return RENDER3DERROR_NOERR;
}

Render3DError Render3D::UpdateClearImage(const u16 *__restrict colorBuffer, const u16 *__restrict depthBuffer, const u8 clearStencil, const u8 xScroll, const u8 yScroll)
{
	return RENDER3DERROR_NOERR;
}

Render3DError Render3D::UpdateToonTable(const u16 *toonTableBuffer)
{
	return RENDER3DERROR_NOERR;
}

Render3DError Render3D::ClearFramebuffer(const GFX3D_State *renderState)
{
	Render3DError error = RENDER3DERROR_NOERR;
	
	struct GFX3D_ClearColor
	{
		u8 r;
		u8 g;
		u8 b;
		u8 a;
	} clearColor;
	
	clearColor.r = renderState->clearColor & 0x1F;
	clearColor.g = (renderState->clearColor >> 5) & 0x1F;
	clearColor.b = (renderState->clearColor >> 10) & 0x1F;
	clearColor.a = (renderState->clearColor >> 16) & 0x1F;
	
	const u8 clearStencil = (renderState->clearColor >> 24) & 0x3F;
	
	if (renderState->enableClearImage)
	{
		const u16 *__restrict clearColorBuffer = (u16 *__restrict)MMU.texInfo.textureSlotAddr[2];
		const u16 *__restrict clearDepthBuffer = (u16 *__restrict)MMU.texInfo.textureSlotAddr[3];
		const u16 scrollBits = T1ReadWord(MMU.ARM9_REG, 0x356); //CLRIMAGE_OFFSET
		const u8 xScroll = scrollBits & 0xFF;
		const u8 yScroll = (scrollBits >> 8) & 0xFF;
		
		error = this->UpdateClearImage(clearColorBuffer, clearDepthBuffer, clearStencil, xScroll, yScroll);
		if (error == RENDER3DERROR_NOERR)
		{
			error = this->ClearUsingImage();
		}
		else
		{
			error = this->ClearUsingValues(clearColor.r, clearColor.g, clearColor.b, clearColor.a, renderState->clearDepth, clearStencil);
		}
	}
	else
	{
		error = this->ClearUsingValues(clearColor.r, clearColor.g, clearColor.b, clearColor.a, renderState->clearDepth, clearStencil);
	}
	
	return error;
}

Render3DError Render3D::ClearUsingImage() const
{
	return RENDER3DERROR_NOERR;
}

Render3DError Render3D::ClearUsingValues(const u8 r, const u8 g, const u8 b, const u8 a, const u32 clearDepth, const u8 clearStencil) const
{
	return RENDER3DERROR_NOERR;
}

Render3DError Render3D::SetupPolygon(const POLY *thePoly)
{
	return RENDER3DERROR_NOERR;
}

Render3DError Render3D::SetupTexture(const POLY *thePoly, bool enableTexturing)
{
	return RENDER3DERROR_NOERR;
}

Render3DError Render3D::SetupViewport(const POLY *thePoly)
{
	return RENDER3DERROR_NOERR;
}

Render3DError Render3D::Reset()
{
	return RENDER3DERROR_NOERR;
}

Render3DError Render3D::Render(const GFX3D_State *renderState, const VERTLIST *vertList, const POLYLIST *polyList, const INDEXLIST *indexList, const u64 frameCount)
{
	Render3DError error = RENDER3DERROR_NOERR;
	
	error = this->BeginRender(renderState);
	if (error != RENDER3DERROR_NOERR)
	{
		return error;
	}
	
	this->UpdateToonTable(renderState->u16ToonTable);
	this->ClearFramebuffer(renderState);
	
	this->PreRender(renderState, vertList, polyList, indexList);
	this->DoRender(renderState, vertList, polyList, indexList);
	this->PostRender();
	
	this->EndRender(frameCount);
	
	return error;
}

Render3DError Render3D::RenderFinish()
{
	return RENDER3DERROR_NOERR;
}

Render3DError Render3D::VramReconfigureSignal()
{
	TexCache_Invalidate();	
	return RENDER3DERROR_NOERR;
}
