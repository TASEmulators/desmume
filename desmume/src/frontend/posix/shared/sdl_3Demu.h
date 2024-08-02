/*
	Copyright (C) 2020 Emmanuel Gil Peyrot
	Copyright (C) 2020-2024 DeSmuME team

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

#ifndef SDL_3DEMU_H
#define SDL_3DEMU_H

bool sdl_initOpenGL_StandardAuto();
bool sdl_initOpenGL_LegacyAuto();
bool sdl_initOpenGL_3_2_CoreProfile();
bool sdl_initOpenGL_ES_3_0();

void sdl_deinitOpenGL();
bool sdl_beginOpenGL();
void sdl_endOpenGL();
bool sdl_framebufferDidResizeCallback(bool isFBOSupported, size_t w, size_t h);

#endif // SDL_3DEMU_H

