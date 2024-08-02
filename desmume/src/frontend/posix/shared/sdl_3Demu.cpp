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

#include <stdio.h>
#include <SDL.h>

#include "sdl_3Demu.h"


static SDL_Window *currWindow = NULL;
static SDL_GLContext currContext = NULL;

static SDL_Window *prevWindow = NULL;
static SDL_GLContext prevContext = NULL;

static SDL_Window *pendingWindow = NULL;

static bool __sdl_initOpenGL(const int requestedProfile, const int requestedVersionMajor, const int requestedVersionMinor)
{
    bool result = false;
    char ctxString[32] = {0};

    if (requestedProfile == SDL_GL_CONTEXT_PROFILE_CORE)
    {
        snprintf(ctxString, sizeof(ctxString), "SDL (Core %i.%i)", requestedVersionMajor, requestedVersionMinor);
    }
    else if (requestedProfile == SDL_GL_CONTEXT_PROFILE_COMPATIBILITY)
    {
        snprintf(ctxString, sizeof(ctxString), "SDL (Compatibility %i.%i)", requestedVersionMajor, requestedVersionMinor);
    }
    else if (requestedProfile == SDL_GL_CONTEXT_PROFILE_ES)
    {
        snprintf(ctxString, sizeof(ctxString), "SDL (ES %i.%i)", requestedVersionMajor, requestedVersionMinor);
    }
    else
    {
        puts("SDL: Invalid profile submitted. No context will be created.");
        return result;
    }

    if (currContext != NULL)
    {
        result = true;
        return result;
    }

    SDL_GL_SetAttribute(SDL_GL_RED_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_ALPHA_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE,   ((requestedProfile == SDL_GL_CONTEXT_PROFILE_CORE) || (requestedProfile == SDL_GL_CONTEXT_PROFILE_ES)) ? 0 : 24);
    SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, ((requestedProfile == SDL_GL_CONTEXT_PROFILE_CORE) || (requestedProfile == SDL_GL_CONTEXT_PROFILE_ES)) ? 0 : 8);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, requestedProfile);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, requestedVersionMajor);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, requestedVersionMinor);

    currWindow = SDL_CreateWindow(NULL, SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, 256, 192, SDL_WINDOW_OPENGL | SDL_WINDOW_HIDDEN);
    if (currWindow == NULL)
    {
        fprintf(stderr, "%s: Failed to create a window: %s\n", ctxString, SDL_GetError());
        return false;
    }

    currContext = SDL_GL_CreateContext(currWindow);
    if (currContext == NULL)
    {
        SDL_DestroyWindow(currWindow);
        currWindow = NULL;

        fprintf(stderr, "%s: Failed to create an OpenGL context: %s\n", ctxString, SDL_GetError());
        return false;
    }

    printf("%s: OpenGL context creation successful.\n", ctxString);
    return true;
}

bool sdl_initOpenGL_StandardAuto()
{
    bool isContextCreated = sdl_initOpenGL_3_2_CoreProfile();

    if (!isContextCreated)
    {
        isContextCreated = sdl_initOpenGL_LegacyAuto();
    }

    return isContextCreated;
}

bool sdl_initOpenGL_LegacyAuto()
{
    return __sdl_initOpenGL(SDL_GL_CONTEXT_PROFILE_COMPATIBILITY, 1, 2);
}

bool sdl_initOpenGL_3_2_CoreProfile()
{
    return __sdl_initOpenGL(SDL_GL_CONTEXT_PROFILE_CORE, 3, 2);
}

bool sdl_initOpenGL_ES_3_0()
{
    return __sdl_initOpenGL(SDL_GL_CONTEXT_PROFILE_ES, 3, 0);
}

void sdl_deinitOpenGL()
{
    SDL_GL_MakeCurrent(NULL, NULL);

    if (currContext != NULL)
    {
        SDL_GL_DeleteContext(currContext);
        currContext = NULL;
    }

    if (currWindow != NULL)
    {
        SDL_DestroyWindow(currWindow);
        currWindow = NULL;
    }

    if (pendingWindow != NULL)
	{
		SDL_DestroyWindow(pendingWindow);
		pendingWindow = NULL;
	}

    prevWindow = NULL;
    prevContext = NULL;
}

bool sdl_beginOpenGL()
{
	int ret = 1;

    prevWindow = SDL_GL_GetCurrentWindow();
    prevContext = SDL_GL_GetCurrentContext();

	if (pendingWindow != NULL)
	{
		bool previousIsCurrent = (prevWindow == currWindow);

		ret = SDL_GL_MakeCurrent(pendingWindow, currContext);
		if (ret == 0)
		{
			SDL_DestroyWindow(currWindow);
			currWindow = pendingWindow;

			if (previousIsCurrent)
			{
				prevWindow = currWindow;
			}

			pendingWindow = NULL;
		}
	}
	else
	{
		ret = SDL_GL_MakeCurrent(currWindow, currContext);
	}

    return (ret == 0);
}

void sdl_endOpenGL()
{
	if (prevWindow == NULL)
	{
		prevWindow = currWindow;
	}

	if (prevContext == NULL)
	{
		prevContext = currContext;
	}

	if ( (prevWindow  != currWindow) ||
	     (prevContext != currContext) )
	{
		SDL_GL_MakeCurrent(prevWindow, prevContext);
	}
}

bool sdl_framebufferDidResizeCallback(bool isFBOSupported, size_t w, size_t h)
{
	bool result = false;

	if (isFBOSupported)
	{
		result = true;
		return result;
	}

	int currWidth;
	int currHeight;
	SDL_GetWindowSize(currWindow, &currWidth, &currHeight);

	if ( (w == (size_t)currWidth) && (h == (size_t)currHeight) )
	{
		result = true;
		return result;
	}

	if (pendingWindow != NULL)
	{
		SDL_DestroyWindow(pendingWindow);
	}

	pendingWindow = SDL_CreateWindow(NULL, SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, (int)w, (int)h, SDL_WINDOW_OPENGL | SDL_WINDOW_HIDDEN);
    if (currWindow == NULL)
    {
        fprintf(stderr, "SDL: sdl_framebufferDidResizeCallback failed to create the resized window: %s\n", SDL_GetError());
        return result;
    }

	result = true;
	return result;
}
