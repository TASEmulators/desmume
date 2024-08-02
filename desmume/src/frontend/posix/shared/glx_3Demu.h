/*
	Copyright (C) 2024 DeSmuME team

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

#ifndef __GLX_3DEMU_H__
#define __GLX_3DEMU_H__

bool glx_initOpenGL_StandardAuto();
bool glx_initOpenGL_LegacyAuto();
bool glx_initOpenGL_3_2_CoreProfile();

void glx_deinitOpenGL();
bool glx_beginOpenGL();
void glx_endOpenGL();
bool glx_framebufferDidResizeCallback(bool isFBOSupported, size_t w, size_t h);

#endif // __GLX_3DEMU_H__
