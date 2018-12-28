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

#ifndef _METAL_RENDERER_COMMON_H_
#define _METAL_RENDERER_COMMON_H_

float4 unpack_rgba5551_to_unorm8888(const ushort color16);

ushort pack_unorm8888_to_rgba5551(const float4 inColor);
uchar4 pack_unorm8888_to_rgba6665(const float4 inColor);
uchar4 pack_unorm8888_to_rgba8888(const float4 inColor);

float4 convert_unorm666X_to_unorm8888(const float4 inColor);

#endif // _METAL_RENDERER_COMMON_H_
