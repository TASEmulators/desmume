/*
	Copyright (C) 2020 Emmanuel Gil Peyrot
 
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

#include <stdio.h>
#include <SDL.h>
#include "../OGLRender.h"

#include "sdl_3Demu.h"

static bool sdl_beginOpenGL(void) { return 1; }
static void sdl_endOpenGL(void) { }
static bool sdl_init(void) { return is_sdl_initialized(); }

static SDL_Window *win = NULL;
static SDL_GLContext ctx = NULL;

bool deinit_sdl_3Demu(void)
{
    bool ret = false;

    if (ctx) {
        SDL_GL_DeleteContext(ctx);
        ctx = NULL;
        ret = true;
    }

    if (win) {
        SDL_DestroyWindow(win);
        win = NULL;
        ret = true;
    }

    return ret;
}

bool init_sdl_3Demu(void) 
{
    SDL_GL_SetAttribute(SDL_GL_RED_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_ALPHA_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
    SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 2);

    win = SDL_CreateWindow(NULL, SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, 256, 192, SDL_WINDOW_OPENGL | SDL_WINDOW_HIDDEN);
    if (!win) {
        fprintf(stderr, "SDL: Failed to create a window: %s\n", SDL_GetError());
        return false;
    }

    ctx = SDL_GL_CreateContext(win);
    if (!ctx) {
        fprintf(stderr, "SDL: Failed to create an OpenGL context: %s\n", SDL_GetError());
        return false;
    }

    printf("OGL/SDL Renderer has finished the initialization.\n");

    oglrender_init = sdl_init;
    oglrender_beginOpenGL = sdl_beginOpenGL;
    oglrender_endOpenGL = sdl_endOpenGL;

    return true;
}

bool is_sdl_initialized(void)
{
    return (ctx != NULL);
}
