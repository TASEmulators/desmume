/* gdk_gl.c - this file is part of DeSmuME
 *
 * Copyright (C) 2007 Damien Nozay (damdoum)
 * Author: damdoum at users.sourceforge.net
 *
 * This file is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * This file is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; see the file COPYING.  If not, write to
 * the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include "gdk_gl.h"

#ifdef HAVE_LIBGDKGLEXT_X11_1_0

#define _DUP8(a) a,a,a,a, a,a,a,a
#define _DUP4(a) a,a,a,a
#define _DUP2(a) a,a

GLuint Textures[2];
// free number we can use in tools 0-1 reserved for screens
static int free_gl_drawable=2;
GdkGLConfig  *my_glConfig=NULL;
GdkGLContext *my_glContext[8]={_DUP8(NULL)};
GdkGLDrawable *my_glDrawable[8]={_DUP8(NULL)};
GtkWidget *pDrawingTexArea;

GLuint screen_texture[1];

#undef _DUP8
#undef _DUP4
#undef _DUP2

/************************************************/
/* BEGIN & END                                  */
/************************************************/

BOOL my_gl_Begin (int screen) {
	return gdk_gl_drawable_gl_begin(my_glDrawable[screen], my_glContext[screen]);
}

void my_gl_End (int screen) {
	if (gdk_gl_drawable_is_double_buffered (my_glDrawable[screen]))
		gdk_gl_drawable_swap_buffers (my_glDrawable[screen]);
	else
		glFlush();
	gdk_gl_drawable_gl_end(my_glDrawable[screen]);
}

/************************************************/
/* OTHER GL COMMANDS                            */
/************************************************/

void my_gl_Identity() {
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();
}

void my_gl_DrawBeautifulQuad() {
	// beautiful quad
	glBegin(GL_QUADS);
		glColor3ub(255,0,0);    glVertex2d(-0.75,-0.75);
		glColor3ub(128,255,0);  glVertex2d(-0.75, 0.75);
		glColor3ub(0,255,128);  glVertex2d( 0.75, 0.75);
		glColor3ub(0,0,255);    glVertex2d( 0.75,-0.75);
	glEnd();
	glColor3ub(255,255,255);
}


void my_gl_DrawLogo() {
	

}


void my_gl_Clear(int screen) {
	if (!my_gl_Begin(screen)) return;
	
	/* Set the background black */
	glClearColor(1.0f, 1.0f, 1.0f, 1.0f);
	glClearDepth(1.0);
	glClear( GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT );

	my_gl_DrawBeautifulQuad();

	my_gl_End(screen);
}

/************************************************/
/* INITIALIZATION                               */
/************************************************/

void init_GL(GtkWidget * widget, int screen, int share_num) {
	int n;
//	for (n=gtk_events_pending(); n>0; n--)
//		gtk_main_iteration();
	// init GL capability
	my_glContext[screen]=NULL;
	my_glDrawable[screen]=NULL;
	if (!gtk_widget_set_gl_capability(
			widget, my_glConfig, 
			my_glContext[share_num],
			//NULL,
			TRUE, 
			GDK_GL_RGBA_TYPE)) {
		printf ("gtk_widget_set_gl_capability\n");
		exit(1);
	}
	// realize so that we get a GdkWindow
 	gtk_widget_realize(widget);
	// make sure we realize
 	gdk_flush();

	my_glDrawable[screen] = gtk_widget_get_gl_drawable(widget);

	if (screen == share_num) {
		my_glContext[screen] = gtk_widget_get_gl_context(widget);
	} else {
		my_glContext[screen] = my_glContext[share_num];
		return;
	}
	
	reshape(widget, screen);
}

int init_GL_free_s(GtkWidget * widget, int share_num) {
	int r = free_gl_drawable;
	my_glContext[r]=NULL;
	my_glDrawable[r]=NULL;
	init_GL(widget, r, share_num);
	free_gl_drawable++;
	return r;
}

int init_GL_free(GtkWidget * widget) {
	int r = free_gl_drawable;
	my_glContext[r]=NULL;
	my_glDrawable[r]=NULL;
	init_GL(widget, r, r);
	free_gl_drawable++;
	return r;
}

void init_GL_capabilities() {

	my_glConfig = gdk_gl_config_new_by_mode (
		GDK_GL_MODE_RGBA
		| GDK_GL_MODE_DEPTH 
		| GDK_GL_MODE_DOUBLE
	);
	// initialize 1st drawing area
	init_GL(pDrawingArea,0,0);
	my_gl_Clear(0);
	
	if (!my_gl_Begin(0)) return;
	// generate ONE texture (display)
	glEnable(GL_TEXTURE_2D);
	glGenTextures(2, Textures);
	my_gl_End(0);

	// initialize 2nd drawing area (sharing context)
	init_GL(pDrawingArea2,1,0);
	my_gl_Clear(1);

        // Signals binding
        // Top screen
        g_signal_connect_after (G_OBJECT (pDrawingArea), "realize",
                                G_CALLBACK (gtk_init_main_gl_area),
                                NULL);
        g_signal_connect( G_OBJECT(pDrawingArea), "expose_event",
                          G_CALLBACK(top_screen_expose_fn), NULL ) ;
        g_signal_connect( G_OBJECT(pDrawingArea), "configure_event",
                          G_CALLBACK(common_configure_fn), NULL ) ;
        // Bottom screen
        g_signal_connect_after (G_OBJECT (pDrawingArea2), "realize",
                                G_CALLBACK (gtk_init_sub_gl_area),
                                NULL);
        g_signal_connect( G_OBJECT(pDrawingArea2), "expose_event",
                          G_CALLBACK(bottom_screen_expose_fn), NULL ) ;
        g_signal_connect( G_OBJECT(pDrawingArea2), "configure_event",
                          G_CALLBACK(common_configure_fn), NULL ) ;

}

/************************************************/
/* RESHAPE                                      */
/************************************************/

void reshape (GtkWidget * widget, int screen) {
	if (my_glDrawable[screen] == NULL ||
		!my_gl_Begin(screen)) return;

	glViewport (0, 0, widget->allocation.width, widget->allocation.height);
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();
	my_gl_End(screen);
}

/************************************************/
/* TEXTURING                                    */
/************************************************/

void my_gl_Texture2D() {
	glBindTexture(GL_TEXTURE_2D, Textures[0]);
#define MyFILTER GL_LINEAR
//#define MyFILTER GL_NEAREST
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, MyFILTER);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, MyFILTER);
#undef MyFILTER
}

void my_gl_ScreenTex() {
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA,
// pause effect
//	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB,
		256, 512, 0, GL_RGBA,
		GL_UNSIGNED_SHORT_1_5_5_5_REV, 
//		GL_UNSIGNED_SHORT_5_5_5_1, 
		GPU_screen);
}

void my_gl_ScreenTexApply(int screen) {
	float off = (screen)?0.375:0;
	glBegin(GL_QUADS);
		// texcoords 0.375 means 192, 1 means 256
		glTexCoord2f(0.0, off+0.000); glVertex2d(-1.0, 1.0);
		glTexCoord2f(1.0, off+0.000); glVertex2d( 1.0, 1.0);
		glTexCoord2f(1.0, off+0.375); glVertex2d( 1.0,-1.0);
		glTexCoord2f(0.0, off+0.375); glVertex2d(-1.0,-1.0);
	glEnd();
}

/************************************************/
/* RENDERING                                    */
/************************************************/

gboolean screen (GtkWidget * widget, int viewportscreen) {
	int H,W,screen;
	GPU * gpu;
	float bright_color = 0.0f; // blend with black
	float bright_alpha = 0.0f; // don't blend
	struct _MASTER_BRIGHT * mBright;

	// we take care to draw the right thing the right place
	// we need to rearrange widgets not to use this trick
	screen = (ScreenInvert)?1-viewportscreen:viewportscreen;
//	screen = viewportscreen;

	if (!my_gl_Begin(viewportscreen)) return TRUE;

	glLoadIdentity();

	// clear screen
	glClearColor(0.0f,0.0f,0.0f,0.0f);
	glClear( GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT );

	glEnable(GL_TEXTURE_2D);

	if (desmume_running()) {

		// master bright
		gpu = ((screen)?SubScreen:MainScreen).gpu;
		mBright = &gpu->dispx_st->dispx_MASTERBRIGHT.bits;
		switch (mBright->Mode)
		{
			case 1: // Bright up : blend with white
				bright_color = 1.0f;
				// no break;
			case 2: // Bright down : blend with black
				bright_alpha = 1.0f; // blending max
				if (!mBright->FactorEx)
					bright_alpha = mBright->Factor / 16.0;
				break;
			// Disabled 0, Reserved 3
			default: break;
		}
		// rotate
		glRotatef(ScreenRotate, 0.0, 0.0, 1.0);
		// create the texture for both display
		my_gl_Texture2D();
		if (viewportscreen==0) {
			my_gl_ScreenTex();
		}
	} else {
		// pause
		// fake master bright up 50%
		bright_color = 0.0f;
		bright_alpha = 0.5f;
	}
	// make sure current color is ok
	glColor4ub(255,255,255,255);
	// apply part of the texture
	my_gl_ScreenTexApply(screen);
	glDisable(GL_TEXTURE_2D);

	// master bright (bis)
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
	glColor4f(bright_color,bright_color,bright_color,bright_alpha);
	glBegin(GL_QUADS);
		glVertex2d(-1.0, 1.0);
		glVertex2d( 1.0, 1.0);
		glVertex2d( 1.0,-1.0);
		glVertex2d(-1.0,-1.0);
	glEnd();
	glDisable(GL_BLEND);

	my_gl_End(viewportscreen);
	return TRUE;
}

/* 3D Rendering */
static void
gtk_init_main_gl_area(GtkWidget *widget,
                      gpointer   data)
{
  GLenum errCode;
  int i;
  GdkGLContext *glcontext;
  GdkGLDrawable *gldrawable;
  GtkWidget *other_drawing_area = (GtkWidget *)data;
  glcontext = gtk_widget_get_gl_context (widget);
  gldrawable = gtk_widget_get_gl_drawable (widget);
  uint16_t blank_texture[256 * 512];

  /*** OpenGL BEGIN ***/
  if (!gdk_gl_drawable_gl_begin (gldrawable, glcontext))
    return;

  //printf("Doing GL init\n");

  for ( i = 0; i < 256 * 512; i++) {
    blank_texture[i] = 0x001f;
  }

  /* Enable Texture Mapping */
  glEnable( GL_TEXTURE_2D );

  /* Set the background black */
  glClearColor( 0.0f, 0.0f, 0.0f, 0.0f );

  /* Create The Texture */
  glGenTextures( 1, &screen_texture[0]);

  glBindTexture( GL_TEXTURE_2D, screen_texture[0]);

  /* Generate The Texture */
  glTexImage2D( GL_TEXTURE_2D, 0, GL_RGB, 256, 512,
                0, GL_RGBA,
                GL_UNSIGNED_SHORT_1_5_5_5_REV,
                blank_texture);

  /* Linear Filtering */
  glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR );
  glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR );

  if ((errCode = glGetError()) != GL_NO_ERROR) {
    GLubyte errString;

    errString = gluErrorString(errCode);
    fprintf( stderr, "Failed to init GL: %s\n", errString);
  }

  gdk_gl_drawable_gl_end (gldrawable);
  /*** OpenGL END ***/
}

static void
gtk_init_sub_gl_area(GtkWidget *widget,
                      gpointer   data)
{
  GLenum errCode;
  GdkGLContext *glcontext;
  GdkGLDrawable *gldrawable;
  glcontext = gtk_widget_get_gl_context (widget);
  gldrawable = gtk_widget_get_gl_drawable (widget);


  /*** OpenGL BEGIN ***/
  if (!gdk_gl_drawable_gl_begin (gldrawable, glcontext))
    return;

  /* Enable Texture Mapping */
  glEnable( GL_TEXTURE_2D );

  /* Set the background black */
  glClearColor( 0.0f, 0.0f, 0.0f, 0.0f );

  glBindTexture( GL_TEXTURE_2D, screen_texture[0]);

  if ((errCode = glGetError()) != GL_NO_ERROR) {
    GLubyte errString;

    errString = gluErrorString(errCode);
    fprintf( stderr, "Failed to init GL: %s\n", errString);
  }

  gdk_gl_drawable_gl_end (gldrawable);
  /*** OpenGL END ***/
}

static int
top_screen_expose_fn( GtkWidget *widget, GdkEventExpose *event, gpointer data) {
  GdkGLContext *glcontext = gtk_widget_get_gl_context (widget);
  GdkGLDrawable *gldrawable = gtk_widget_get_gl_drawable (widget);
  // FIXME: Hardcoded to false for now...
  int software_convert = 0;

  /*** OpenGL BEGIN ***/
  if (!gdk_gl_drawable_gl_begin (gldrawable, glcontext))
    return FALSE;

  GLenum errCode;

  /* Clear The Screen And The Depth Buffer */
  glClear( GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT );

  /* Move Into The Screen 5 Units */
  //glLoadIdentity( );

  /* Select screen Texture */
  glBindTexture( GL_TEXTURE_2D, screen_texture[0]);

  if ( software_convert) {
    int i;
    u8 converted[256 * 384 * 3];

    for ( i = 0; i < (256 * 384); i++) {
      converted[(i * 3) + 0] = ((*((u16 *)&GPU_screen[(i<<1)]) >> 0) & 0x1f) << 3;
      converted[(i * 3) + 1] = ((*((u16 *)&GPU_screen[(i<<1)]) >> 5) & 0x1f) << 3;
      converted[(i * 3) + 2] = ((*((u16 *)&GPU_screen[(i<<1)]) >> 10) & 0x1f) << 3;
    }

    glTexSubImage2D( GL_TEXTURE_2D, 0, 0, 0, 256, 384,
                     GL_RGB,
                     GL_UNSIGNED_BYTE,
                     converted);
  }
  else {
    glTexSubImage2D( GL_TEXTURE_2D, 0, 0, 0, 256, 384,
                     GL_RGBA,
                     GL_UNSIGNED_SHORT_1_5_5_5_REV,
                     &GPU_screen);
  }


  if ((errCode = glGetError()) != GL_NO_ERROR) {
    GLubyte errString;

    errString = gluErrorString(errCode);
    fprintf( stderr, "GL subimage failed: %s\n", errString);
  }


  glBegin( GL_QUADS);

  /* Top screen */
  glTexCoord2f( 0.0f, 0.0f ); glVertex3f( 0.0f,  0.0f, 0.0f );
  glTexCoord2f( 1.0f, 0.0f ); glVertex3f( 256.0f,  0.0f,  0.0f );
  glTexCoord2f( 1.0f, 0.375f ); glVertex3f( 256.0f,  192.0f,  0.0f );
  glTexCoord2f( 0.0f, 0.375f ); glVertex3f(  0.0f,  192.0f, 0.0f );
  glEnd( );

  if ((errCode = glGetError()) != GL_NO_ERROR) {
    GLubyte errString;

    errString = gluErrorString(errCode);
    fprintf( stderr, "GL draw failed: %s\n", errString);
  }

  if (gdk_gl_drawable_is_double_buffered (gldrawable))
    gdk_gl_drawable_swap_buffers (gldrawable);
  else
    glFlush ();


  gdk_gl_drawable_gl_end (gldrawable);
  /*** OpenGL END ***/

  gtk_widget_queue_draw(pDrawingArea2);
}

static int
bottom_screen_expose_fn(GtkWidget *widget, GdkEventExpose *event, gpointer data)
{
  GdkGLContext *glcontext = gtk_widget_get_gl_context (widget);
  GdkGLDrawable *gldrawable = gtk_widget_get_gl_drawable (widget);

  //g_print("Sub Expose\n");

  /*** OpenGL BEGIN ***/
  if (!gdk_gl_drawable_gl_begin (gldrawable, glcontext)) {
    g_print("begin failed\n");
    return FALSE;
  }
  //g_print("begin\n");

  GLenum errCode;

  /* Clear The Screen */
  glClear( GL_COLOR_BUFFER_BIT);

  //glBindTexture( GL_TEXTURE_2D, screen_texture[0]);

  glBegin( GL_QUADS);

  /* Bottom screen */
  glTexCoord2f( 0.0f, 0.375f ); glVertex2f( 0.0f,  0.0f);
  glTexCoord2f( 1.0f, 0.375f ); glVertex2f( 256.0f,  0.0f);
  glTexCoord2f( 1.0f, 0.75f ); glVertex2f( 256.0f,  192.0f);
  glTexCoord2f( 0.0f, 0.75f ); glVertex2f(  0.0f,  192.0f);
  glEnd( );

  if (gdk_gl_drawable_is_double_buffered (gldrawable))
    gdk_gl_drawable_swap_buffers (gldrawable);
  else
    glFlush ();


  if ((errCode = glGetError()) != GL_NO_ERROR) {
    GLubyte errString;

    errString = gluErrorString(errCode);
    fprintf( stderr, "sub GL draw failed: %s\n", errString);
  }

  gdk_gl_drawable_gl_end (gldrawable);
  /*** OpenGL END ***/

  return 1;
}

static gboolean
common_configure_fn( GtkWidget *widget,
                     GdkEventConfigure *event ) {
  if ( gtk_widget_is_gl_capable( widget) == FALSE)
    return TRUE;

  GdkGLContext *glcontext = gtk_widget_get_gl_context (widget);
  GdkGLDrawable *gldrawable = gtk_widget_get_gl_drawable (widget);

  int comp_width = 3 * event->width;
  int comp_height = 4 * event->height;
  int use_width = 1;
  GLenum errCode;

  /* Height / width ration */
  GLfloat ratio;

  //g_print("wdith %d, height %d\n", event->width, event->height);

  /*** OpenGL BEGIN ***/
  if (!gdk_gl_drawable_gl_begin (gldrawable, glcontext))
    return FALSE;

  if ( comp_width > comp_height) {
    use_width = 0;
  }
 
  /* Protect against a divide by zero */
  if ( event->height == 0 )
    event->height = 1;
  if ( event->width == 0)
    event->width = 1;

  ratio = ( GLfloat )event->width / ( GLfloat )event->height;

  /* Setup our viewport. */
  glViewport( 0, 0, ( GLint )event->width, ( GLint )event->height );

  /*
   * change to the projection matrix and set
   * our viewing volume.
   */
  glMatrixMode( GL_PROJECTION );
  glLoadIdentity( );

  {
    double left;
    double right;
    double bottom;
    double top;
    double other_dimen;

    if ( use_width) {
      left = 0.0;
      right = 256.0;

      other_dimen = (double)event->width * 3.0 / 4.0;

      top = 0.0;
      bottom = 192.0 * ((double)event->height / other_dimen);
    }
    else {
      top = 0.0;
      bottom = 192.0;
      other_dimen = (double)event->height * 4.0 / 3.0;

      left = 0.0;
      right = 256.0 * ((double)event->width / other_dimen);
    }

    /*
    printf("%d,%d\n", width, height);
    printf("l %lf, r %lf, t %lf, b %lf, other dimen %lf\n",
           left, right, top, bottom, other_dimen);
    */

    /* get the area (0,0) to (256,384) into the middle of the viewport */
    gluOrtho2D( left, right, bottom, top);
  }

  /* Make sure we're chaning the model view and not the projection */
  glMatrixMode( GL_MODELVIEW );

  /* Reset The View */
  glLoadIdentity( );

  if ((errCode = glGetError()) != GL_NO_ERROR) {
    GLubyte errString;

    errString = gluErrorString(errCode);
    fprintf( stderr, "GL resie failed: %s\n", errString);
  }

  gdk_gl_drawable_gl_end (gldrawable);
  /*** OpenGL END ***/

  return TRUE;
}

#endif /* if HAVE_LIBGDKGLEXT_X11_1_0 */
