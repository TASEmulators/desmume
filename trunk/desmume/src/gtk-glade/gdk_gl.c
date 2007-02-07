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
GLuint Textures[1];
GdkGLConfig  *my_glConfig=NULL;
GdkGLContext *my_glContext=NULL;
GdkGLDrawable *my_glDrawable[3]={NULL,NULL,NULL};
GtkWidget *pDrawingTexArea;

INLINE void my_gl_Identity() {
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
}

INLINE void my_gl_DrawBeautifulQuad() {
	// beautiful quad
	glBegin(GL_QUADS);
		glColor3ub(255,0,0);    glVertex2d(-0.75,-0.75);
		glColor3ub(128,255,0);  glVertex2d(-0.75, 0.75);
		glColor3ub(0,255,128);  glVertex2d( 0.75, 0.75);
		glColor3ub(0,0,255);    glVertex2d( 0.75,-0.75);
	glEnd();
	glColor3ub(255,255,255);
}

BOOL my_gl_Begin (int screen) {
	return gdk_gl_drawable_gl_begin(my_glDrawable[screen], my_glContext);
}

void my_gl_End (int screen) {
	glFlush ();
	if (gdk_gl_drawable_is_double_buffered (my_glDrawable[screen]))
		gdk_gl_drawable_swap_buffers (my_glDrawable[screen]);
	gdk_gl_drawable_gl_end(my_glDrawable[screen]);
}

void init_GL(GtkWidget * widget, int screen) {
	// init GL capability
	if (!gtk_widget_set_gl_capability(
			widget, my_glConfig, 
			NULL, TRUE, 
			GDK_GL_RGBA_TYPE)) {
		printf ("gtk_widget_set_gl_capability\n");
		exit(1);
	}
	// realize so that we get a GdkWindow
	gtk_widget_realize(widget);
	// make sure we realize
	gtk_events_pending();

	my_glDrawable[screen] = gtk_widget_get_gl_drawable(widget);
	// shared context
	if (my_glContext == NULL) {
		// not initialized !
		my_glContext = gtk_widget_get_gl_context(widget);
		if (!my_gl_Begin(screen)) return;
	
		/* Set the background black */
		glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
		glClear( GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT );
	
		my_gl_DrawBeautifulQuad();
	
		// generated ONE texture (display)
		glEnable(GL_TEXTURE_2D);
		glGenTextures(1, &Textures[0]);
	
		my_gl_End(screen);
	}
	reshape(widget, screen);
}

void init_GL_capabilities() {	
	my_glConfig = gdk_gl_config_new_by_mode (
		GDK_GL_MODE_RGB
		| GDK_GL_MODE_DEPTH 
		| GDK_GL_MODE_DOUBLE
	);
	// initialize 1st drawing area
	init_GL(pDrawingArea,0);
	// initialize 2nd drawing area (sharing context)
	init_GL(pDrawingArea2,1);
}

void reshape (GtkWidget * widget, int screen) {
	if (my_glDrawable[screen] == NULL ||
		!my_gl_Begin(screen)) return;

	glViewport (0, 0, widget->allocation.width, widget->allocation.height);
	
	my_gl_End(screen);
}

INLINE void my_gl_Texture2D() {
	glBindTexture(GL_TEXTURE_2D, Textures[0]);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
}

INLINE void my_gl_ScreenTex() {
	glTexImage2D(GL_TEXTURE_2D, 0, 4,
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
		glTexCoord2f(0.0, off+0.375); glVertex2d(-1.0,-1.0);
		glTexCoord2f(1.0, off+0.375); glVertex2d( 1.0,-1.0);
		glTexCoord2f(1.0, off+0.000); glVertex2d( 1.0, 1.0);
	glEnd();
}

gboolean screen (GtkWidget * widget, int screen) {
	int H,W;
	if (!my_gl_Begin(screen)) return TRUE;

	glLoadIdentity();
	glClear( GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT );
	my_gl_DrawBeautifulQuad();

	// rotate
	if (ScreenRotate) glRotatef(90.0, 0.0, 0.0, 1.0);

	// draw screen
	my_gl_Texture2D();
	if (screen==0) my_gl_ScreenTex();
	my_gl_ScreenTexApply(screen);
	
	my_gl_End(screen);
	return TRUE;
}

#endif /* if HAVE_LIBGDKGLEXT_X11_1_0 */
