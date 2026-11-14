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

#ifndef _WGPU_UPSCALER_WIN32_H_
#define _WGPU_UPSCALER_WIN32_H_

class WGPUPresentShaderSource;

// The WGSL of the GPU filters, as the Windows frontend carries it: one RCDATA
// resource per pass, declared in resources.rc and read out of the executable by
// this source. Returns an object that lives for the run of the process, or NULL
// in a build that has no resources.
//
// The chain runtime asks for a pass only when the filter it belongs to is
// selected, so a filter nobody picks is never read, let alone compiled.
WGPUPresentShaderSource *WGPUCreateWin32UpscalerShaderSource();

#endif // _WGPU_UPSCALER_WIN32_H_
