/*
Copyright (C) 2018 DeSmuME team

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

#include "ddraw.h"

#include "directx/ddraw.h"
#include "GPU.h"
#include "types.h"

const char *DDerrors[] = {
	"no errors",
	"Unable to initialize DirectDraw",
	"Unable to set DirectDraw Cooperative Level",
	"Unable to create DirectDraw primary surface",
	"Unable to set DirectDraw clipper" };

DDRAW::DDRAW() :
	handle(NULL), clip(NULL)
{
	surface.primary = NULL;
	surface.back = NULL;
	memset(&surfDesc, 0, sizeof(surfDesc));
	memset(&surfDescBack, 0, sizeof(surfDescBack));
}

u32	DDRAW::create(HWND hwnd)
{
	if (handle) return 0;

	if (FAILED(DirectDrawCreateEx(NULL, (LPVOID*)&handle, IID_IDirectDraw7, NULL)))
		return 1;

	if (FAILED(handle->SetCooperativeLevel(hwnd, DDSCL_NORMAL)))
		return 2;

	createSurfaces(hwnd);

	return 0;
}

bool DDRAW::release()
{
	if (!handle) return true;

	if (clip != NULL)  clip->Release();
	if (surface.back != NULL) surface.back->Release();
	if (surface.primary != NULL) surface.primary->Release();

	if (FAILED(handle->Release())) return false;
	return true;
}

bool DDRAW::createBackSurface(int width, int height)
{
	if (surface.back) { surface.back->Release(); surface.back = NULL; }

	memset(&surfDescBack, 0, sizeof(surfDescBack));
	surfDescBack.dwSize = sizeof(surfDescBack);
	surfDescBack.dwFlags = DDSD_CAPS | DDSD_HEIGHT | DDSD_WIDTH;
	surfDescBack.ddsCaps.dwCaps = DDSCAPS_OFFSCREENPLAIN;

	if (systemMemory)
		surfDescBack.ddsCaps.dwCaps |= DDSCAPS_SYSTEMMEMORY;
	else
		surfDescBack.ddsCaps.dwCaps |= DDSCAPS_VIDEOMEMORY;


	surfDescBack.dwWidth = width;
	surfDescBack.dwHeight = height;

	if (FAILED(handle->CreateSurface(&surfDescBack, &surface.back, NULL))) return false;

	return true;
}

bool DDRAW::createSurfaces(HWND hwnd)
{
	if (!handle) return true;

	if (clip) { clip->Release(); clip = NULL; }
	if (surface.primary) { surface.primary->Release();  surface.primary = NULL; }


	// primary
	memset(&surfDesc, 0, sizeof(surfDesc));
	surfDesc.dwSize = sizeof(surfDesc);
	surfDesc.ddsCaps.dwCaps = DDSCAPS_PRIMARYSURFACE;
	surfDesc.dwFlags = DDSD_CAPS;
	if (FAILED(handle->CreateSurface(&surfDesc, &surface.primary, NULL)))
		return false;

	//default doesnt matter much, itll get adjusted later
	if (!createBackSurface(GPU_FRAMEBUFFER_NATIVE_WIDTH, GPU_FRAMEBUFFER_NATIVE_HEIGHT * 2))
		return false;

	if (FAILED(handle->CreateClipper(0, &clip, NULL))) return false;
	if (FAILED(clip->SetHWnd(0, hwnd))) return false;
	if (FAILED(surface.primary->SetClipper(clip))) return false;

	return true;
}

bool DDRAW::lock()
{
	if (!handle) return true;
	if (!surface.back) {
		surfDescBack.dwWidth = 0; //signal to another file to try recreating
		return false;
	}
	memset(&surfDescBack, 0, sizeof(surfDescBack));
	surfDescBack.dwSize = sizeof(surfDescBack);
	surfDescBack.dwFlags = DDSD_ALL;

	HRESULT res = surface.back->Lock(NULL, &surfDescBack, DDLOCK_WAIT | DDLOCK_WRITEONLY, NULL);
	if (FAILED(res))
	{
		//INFO("DDraw failed: Lock %i\n", res);
		if (res == DDERR_SURFACELOST)
		{
			res = surface.back->Restore();
			if (FAILED(res)) return false;
		}
	}
	return true;
}

bool DDRAW::unlock()
{
	if (!handle) return true;
	if (!surface.back) return false;
	if (FAILED(surface.back->Unlock((LPRECT)surfDescBack.lpSurface))) return false;
	return true;
}

bool DDRAW::OK()
{
	if (!handle) return false;
	if (!surface.primary) return false;
	if (!surface.back) return false;
	return true;
}

bool DDRAW::blt(LPRECT dst, LPRECT src)
{
	if (!handle) return true;
	if (!surface.primary) return false;
	if (!surface.back) return false;

	if (vSync)
	{
		//this seems to block the whole process. this destroys the display thread and will easily block the emulator to 30fps.
		//IDirectDraw7_WaitForVerticalBlank(handle,DDWAITVB_BLOCKBEGIN,0);

		for (;;)
		{
			BOOL vblank;
			IDirectDraw7_GetVerticalBlankStatus(handle, &vblank);
			if (vblank) break;
			//must be a greedy loop since vblank is small relative to 1msec minimum Sleep() resolution.
		}
	}

	HRESULT res = surface.primary->Blt(dst, surface.back, src, DDBLT_WAIT, 0);
	if (FAILED(res))
	{
		//INFO("DDraw failed: Blt %i\n", res);
		if (res == DDERR_SURFACELOST)
		{
			res = surface.primary->Restore();
			if (FAILED(res)) return false;
		}
	}
	return true;
}
