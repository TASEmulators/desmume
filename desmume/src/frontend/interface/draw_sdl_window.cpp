/*
	Copyright (C) 2006 yopyop
	Copyright (C) 2006-2007 shash
	Copyright (C) 2008-2020 DeSmuME team

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

// This entire file is basically a copy of the Posix CLI rendering / input processing.

#include <SDL.h>
#include "interface.h"
#include "../../NDSSystem.h"
#include "../../GPU.h"
#include "../posix/shared/ctrlssdl.h"

#ifdef INCLUDE_OPENGL_2D
	#include <GL/gl.h>
	#include <GL/glu.h>
	#include <GL/glext.h>
#endif

// SDL drawing & input processing
static SDL_Window *window;
static SDL_Renderer *renderer;
static int sdl_videoFlags;
struct ctrls_event_config ctrls_cfg;
static float nds_screen_size_ratio = 1.0f;
bool opengl_2d = true;
#ifdef INCLUDE_OPENGL_2D
GLuint sdl_ogl_screen_texture[2];
#endif
// TODO: Make configurable instead.
const u32 cli_kb_cfg[NB_KEYS] =
        {
                SDLK_x,         // A
                SDLK_z,         // B
                SDLK_RSHIFT,    // select
                SDLK_RETURN,    // start
                SDLK_RIGHT,     // Right
                SDLK_LEFT,      // Left
                SDLK_UP,        // Up
                SDLK_DOWN,      // Down
                SDLK_w,         // R
                SDLK_q,         // L
                SDLK_s,         // X
                SDLK_a,         // Y
                SDLK_p,         // DEBUG
                SDLK_o,         // BOOST
                SDLK_BACKSPACE, // Lid
        };

#ifdef INCLUDE_OPENGL_2D
static void resizeWindow_stub(u16 width, u16 height, GLuint *sdl_ogl_screen_texture) {}
#else
static void resizeWindow_stub(u16 width, u16 height, void *sdl_ogl_screen_texture) {}
#endif

static void sdl_draw_no_opengl()
{
    const NDSDisplayInfo &displayInfo = GPU->GetDisplayInfo();
    const size_t pixCount = GPU_FRAMEBUFFER_NATIVE_WIDTH * GPU_FRAMEBUFFER_NATIVE_HEIGHT;
    ColorspaceApplyIntensityToBuffer16<false, false>((u16 *)displayInfo.masterNativeBuffer, pixCount, displayInfo.backlightIntensity[NDSDisplayID_Main]);
    ColorspaceApplyIntensityToBuffer16<false, false>((u16 *)displayInfo.masterNativeBuffer + pixCount, pixCount, displayInfo.backlightIntensity[NDSDisplayID_Touch]);

    SDL_Surface *rawImage = SDL_CreateRGBSurfaceFrom(displayInfo.masterNativeBuffer, GPU_FRAMEBUFFER_NATIVE_WIDTH, GPU_FRAMEBUFFER_NATIVE_HEIGHT * 2, 16, GPU_FRAMEBUFFER_NATIVE_WIDTH * sizeof(u16), 0x001F, 0x03E0, 0x7C00, 0);
    if(rawImage == NULL) return;

	SDL_Texture *texture = SDL_CreateTextureFromSurface(renderer, rawImage);
	SDL_FreeSurface(rawImage);
	SDL_RenderCopy(renderer, texture, NULL, NULL);
	SDL_RenderPresent(renderer);
}

#ifdef INCLUDE_OPENGL_2D
static void sdl_draw_with_opengl(GLuint *screen_texture)
{
    desmume_draw_opengl(sdl_ogl_screen_texture);
    /* Flush the drawing to the screen */
    SDL_GL_SwapBuffers();
}

/* initialization openGL function */
static int initGL(GLuint *screen_texture) {
    GLenum errCode;
    u16 blank_texture[256 * 256];

    memset(blank_texture, 0, sizeof(blank_texture));

    /* Enable Texture Mapping */
    glEnable( GL_TEXTURE_2D );

    /* Set the background black */
    glClearColor( 0.0f, 0.0f, 0.0f, 0.5f );

    /* Depth buffer setup */
    glClearDepth( 1.0f );

    /* Enables Depth Testing */
    glEnable( GL_DEPTH_TEST );

    /* The Type Of Depth Test To Do */
    glDepthFunc( GL_LEQUAL );

    /* Create The Texture */
    glGenTextures(2, screen_texture);

    for (int i = 0; i < 2; i++)
    {
        glBindTexture(GL_TEXTURE_2D, screen_texture[i]);

        /* Generate The Texture */
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 256, 256,
          0, GL_RGBA,
          GL_UNSIGNED_SHORT_1_5_5_5_REV,
          blank_texture);

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

        /* Linear Filtering */
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    }

    if ((errCode = glGetError()) != GL_NO_ERROR) {
        const GLubyte *errString;

        errString = gluErrorString(errCode);
        fprintf( stderr, "Failed to init GL: %s\n", errString);

        return 0;
    }

    return 1;
}

static void resizeWindow(u16 width, u16 height, GLuint *screen_texture) {
    int comp_width = 3 * width;
    int comp_height = 2 * height;
    GLenum errCode;

    initGL(screen_texture);

#ifdef HAVE_LIBAGG
    //Hud.reset();
#endif

    if ( comp_width > comp_height) {
        width = 2*height/3;
    }
    height = 3*width/2;
    nds_screen_size_ratio = 256.0 / (double)width;

    /* Setup our viewport. */
    glViewport( 0, 0, ( GLint )width, ( GLint )height );

    /*
    * change to the projection matrix and set
    * our viewing volume.
    */
    glMatrixMode( GL_PROJECTION );
    glLoadIdentity( );

    gluOrtho2D( 0.0, 256.0, 384.0, 0.0);

    /* Make sure we're chaning the model view and not the projection */
    glMatrixMode( GL_MODELVIEW );

    /* Reset The View */
    glLoadIdentity( );

    if ((errCode = glGetError()) != GL_NO_ERROR) {
        const GLubyte *errString;

        errString = gluErrorString(errCode);
        fprintf( stderr, "GL resize failed: %s\n", errString);
    }
}
#endif


//
//


EXPORTED int desmume_draw_window_init(BOOL auto_pause, BOOL use_opengl_if_possible)
{
    opengl_2d = use_opengl_if_possible;

    // SDL_Init is called in desmume_init.

#ifdef INCLUDE_OPENGL_2D
    if (opengl_2d) {
        /* the flags to pass to SDL_SetVideoMode */
        sdl_videoFlags  = SDL_OPENGL;          /* Enable OpenGL in SDL */
        sdl_videoFlags |= SDL_RESIZABLE;       /* Enable window resizing */


        /* Sets up OpenGL double buffering */
        SDL_GL_SetAttribute( SDL_GL_DOUBLEBUFFER, 1 );

		window = SDL_CreateWindow( "Desmume SDL", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, 256, 192 * 2,
								   sdl_videoFlags );

		/* Verify there is a window */
		if ( !window ) {
		  fprintf( stderr, "Window creation failed: %s\n", SDL_GetError( ) );
		  exit( -1);
		}


        /* initialize OpenGL */
        if (!initGL(sdl_ogl_screen_texture)) {
          fprintf(stderr, "Failed to init GL, fall back to software render\n");

          opengl_2d = 0;
        }
    }

    if (!opengl_2d) {
#endif
		sdl_videoFlags |= SDL_SWSURFACE;
		window = SDL_CreateWindow( "Desmume SDL", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, 256, 384, sdl_videoFlags );

		if ( !window ) {
			fprintf( stderr, "Window creation failed: %s\n", SDL_GetError( ) );
			exit( -1);
		}

		renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
#ifdef INCLUDE_OPENGL_2D
    }

    /* set the initial window size */
    if (opengl_2d) {
        resizeWindow( 256, 192*2, sdl_ogl_screen_texture);
    }
#endif

    // TODO: Make configurable instead.
    load_default_config(cli_kb_cfg);

    ctrls_cfg.boost = 0;
    ctrls_cfg.sdl_quit = 0;
    ctrls_cfg.auto_pause = auto_pause;
    ctrls_cfg.focused = 1;
    ctrls_cfg.fake_mic = 0;
    ctrls_cfg.keypad = 0;
#ifdef INCLUDE_OPENGL_2D
    ctrls_cfg.screen_texture = sdl_ogl_screen_texture;
#else
    ctrls_cfg.screen_texture = NULL;
#endif
    ctrls_cfg.resize_cb = &resizeWindow_stub;

    return 0;
}

EXPORTED void desmume_draw_window_input()
{
    SDL_Event event;

    ctrls_cfg.nds_screen_size_ratio = nds_screen_size_ratio;

    /* Look for queued events and update keypad status */
    /* IMPORTANT: Reenable joystick events if needed. */
    if(SDL_JoystickEventState(SDL_QUERY) == SDL_IGNORE)
        SDL_JoystickEventState(SDL_ENABLE);

    /* There's an event waiting to be processed? */
    while ( !ctrls_cfg.sdl_quit &&
            (SDL_PollEvent(&event) || (!ctrls_cfg.focused && SDL_WaitEvent(&event))))
    {
        process_ctrls_event( event, &ctrls_cfg);
    }

    /* Update mouse position and click */
    if(mouse.down) NDS_setTouchPos(mouse.x, mouse.y);
    if(mouse.click)
    {
        NDS_releaseTouch();
        mouse.click = FALSE;
    }

    update_keypad(ctrls_cfg.keypad);     /* Update keypad */
}

EXPORTED void desmume_draw_window_frame()
{
#ifdef INCLUDE_OPENGL_2D
    if (opengl_2d) {
        sdl_draw_with_opengl(sdl_ogl_screen_texture);
        ctrls_cfg.resize_cb = &resizeWindow;
    }
    else
#endif
        sdl_draw_no_opengl();
}

EXPORTED BOOL desmume_draw_window_has_quit()
{
    return ctrls_cfg.sdl_quit;
}

EXPORTED void desmume_draw_window_free()
{
    SDL_Quit();
}