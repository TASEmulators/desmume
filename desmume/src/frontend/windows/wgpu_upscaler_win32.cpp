/*
	Copyright (C) 2026 DeSmuME team

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

#include "wgpu_upscaler_win32.h"

#include <windows.h>

#include "../shared/wgpu_present.h"

// The shaders live in the executable as RCDATA rather than in the project as
// string literals: they stay the libretro files they came from, readable and
// diffable against upstream, and nothing has to be deployed next to the
// executable. resources.rc names them; the chain runtime asks for one pass of
// one filter at a time, and only for the filter the user selected.

namespace
{

class Win32UpscalerShaderSource : public WGPUPresentShaderSource
{
public:
	virtual bool getShader(const char *name, const char **source, size_t *length)
	{
		*source = NULL;
		*length = 0;

		const HRSRC resource = FindResourceA(NULL, name, RT_RCDATA);

		if (resource == NULL)
		{
			return false;
		}

		const HGLOBAL handle = LoadResource(NULL, resource);

		if (handle == NULL)
		{
			return false;
		}

		const char *data = (const char *)LockResource(handle);

		if (data == NULL)
		{
			return false;
		}

		const DWORD size = SizeofResource(NULL, resource);

		if (size == 0)
		{
			return false;
		}

		*source = data;
		*length = (size_t)size;

		return true;
	}
};

} // namespace

WGPUPresentShaderSource *WGPUCreateWin32UpscalerShaderSource()
{
	// The resources stay loaded for the life of the process, so the text this
	// hands out stays valid for as long as the present object lives.
	static Win32UpscalerShaderSource source;

	return &source;
}
