/* gdk_gl.cpp - this file is part of DeSmuME
 *
 * Copyright (C) 2007-2017 DeSmuME Team
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

#ifdef GTKGLEXT_AVAILABLE

#include <GL/gl.h>
#include <GL/glu.h>

#define _DUP8(a) a,a,a,a, a,a,a,a
#define _DUP4(a) a,a,a,a
#define _DUP2(a) a,a

// free number we can use in tools 0-1 reserved for screens
static int free_gl_drawable=2;
GdkGLConfig  *my_glConfig=NULL;
GdkGLContext *my_glContext[8]={_DUP8(NULL)};
GdkGLDrawable *my_glDrawable[8]={_DUP8(NULL)};
GtkWidget *pDrawingTexArea;

GLuint screen_texture[2];

#undef _DUP8
#undef _DUP4
#undef _DUP2

/************************************************/
/* BEGIN & END                                  */
/************************************************/

BOOL my_gl_Begin (NDSDisplayID displayID)
{
	return gdk_gl_drawable_gl_begin(my_glDrawable[displayID], my_glContext[displayID]);
}

void my_gl_End (NDSDisplayID displayID)
{
	if (gdk_gl_drawable_is_double_buffered (my_glDrawable[displayID]))
		gdk_gl_drawable_swap_buffers (my_glDrawable[displayID]);
	else
		glFlush();
	gdk_gl_drawable_gl_end(my_glDrawable[displayID]);
}

/************************************************/
/* OTHER GL COMMANDS                            */
/************************************************/

void my_gl_Identity()
{
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();
}

void my_gl_Clear(NDSDisplayID displayID)
{
	if (!my_gl_Begin(displayID))
		return;
	
	/* Set the background black */
    glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
    glClear(GL_COLOR_BUFFER_BIT);

	my_gl_End(displayID);
}

/************************************************/
/* INITIALIZATION                               */
/************************************************/

void init_GL(GtkWidget *widget, NDSDisplayID displayID, NDSDisplayID sharedContextDisplayID)
{
//	for (n=gtk_events_pending(); n>0; n--)
//		gtk_main_iteration();
	// init GL capability
	my_glContext[displayID] = NULL;
	my_glDrawable[displayID] = NULL;
	if (!gtk_widget_set_gl_capability(
			widget, my_glConfig, 
			my_glContext[sharedContextDisplayID],
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

	my_glDrawable[displayID] = gtk_widget_get_gl_drawable(widget);

	if (displayID == sharedContextDisplayID)
	{
		my_glContext[displayID] = gtk_widget_get_gl_context(widget);
	}
	else
	{
		my_glContext[displayID] = my_glContext[sharedContextDisplayID];
		return;
	}
	
	reshape(widget, displayID);
}

NDSDisplayID init_GL_free_s(GtkWidget *widget, NDSDisplayID sharedContextDisplayID)
{
	NDSDisplayID r = (NDSDisplayID)free_gl_drawable;
	my_glContext[r] = NULL;
	my_glDrawable[r] = NULL;
	init_GL(widget, r, sharedContextDisplayID);
	free_gl_drawable++;
	return r;
}

NDSDisplayID init_GL_free(GtkWidget *widget)
{
	NDSDisplayID r = (NDSDisplayID)free_gl_drawable;
	my_glContext[r] = NULL;
	my_glDrawable[r] = NULL;
	init_GL(widget, r, r);
	free_gl_drawable++;
	return r;
}

void init_GL_capabilities() {

	uint16_t blank_texture[256 * 256];
	memset(blank_texture, 0, sizeof(blank_texture));
	
	my_glConfig = gdk_gl_config_new_by_mode (
		(GdkGLConfigMode) (GDK_GL_MODE_RGBA
		| GDK_GL_MODE_DEPTH 
		| GDK_GL_MODE_DOUBLE)
	);

	// initialize 1st drawing area
	init_GL(pDrawingArea, NDSDisplayID_Main, NDSDisplayID_Main);
	my_gl_Clear(NDSDisplayID_Main);
	
	if (!my_gl_Begin(NDSDisplayID_Main))
		return;
	
	glEnable(GL_TEXTURE_2D);
	glGenTextures(2, screen_texture);

	/* Generate The Texture */
	for (int i = 0; i < 2; i++)
	{
		glBindTexture(GL_TEXTURE_2D, screen_texture[i]);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		
#define MyFILTER GL_LINEAR
//#define MyFILTER GL_NEAREST
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, MyFILTER);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, MyFILTER);
#undef MyFILTER
		
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 256, 256,
					 0, GL_RGBA,
					 GL_UNSIGNED_SHORT_1_5_5_5_REV,
					 blank_texture);
	}
	
	my_gl_End(NDSDisplayID_Main);

	// initialize 2nd drawing area (sharing context)
	init_GL(pDrawingArea2, NDSDisplayID_Touch, NDSDisplayID_Main);
	my_gl_Clear(NDSDisplayID_Touch);
}

/************************************************/
/* RESHAPE                                      */
/************************************************/

void reshape(GtkWidget *widget, NDSDisplayID displayID)
{
	if (my_glDrawable[displayID] == NULL || !my_gl_Begin(displayID))
		return;

	glViewport (0, 0, widget->allocation.width, widget->allocation.height);
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();
	my_gl_End(displayID);
}

/************************************************/
/* TEXTURING                                    */
/************************************************/

static void
my_gl_ScreenTex()
{
	const NDSDisplayInfo &displayInfo = GPU->GetDisplayInfo();

	glBindTexture(GL_TEXTURE_2D, screen_texture[NDSDisplayID_Main]);
	glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 256, 192,
					GL_RGBA,
					GL_UNSIGNED_SHORT_1_5_5_5_REV,
					displayInfo.renderedBuffer[NDSDisplayID_Main]);
					
	glBindTexture(GL_TEXTURE_2D, screen_texture[NDSDisplayID_Touch]);
	glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 256, 192,
					GL_RGBA,
					GL_UNSIGNED_SHORT_1_5_5_5_REV,
					displayInfo.renderedBuffer[NDSDisplayID_Touch]);
}

static void my_gl_ScreenTexApply(NDSDisplayID displayID)
{
	const NDSDisplayInfo &displayInfo = GPU->GetDisplayInfo();
	
	GLfloat backlightIntensity = displayInfo.backlightIntensity[displayID];
	
	glBindTexture(GL_TEXTURE_2D, screen_texture[displayID]);
	glBegin(GL_QUADS);
		glColor3f(backlightIntensity, backlightIntensity, backlightIntensity); glTexCoord2f(0.00f, 0.00f); glVertex2f(-1.0f,  1.0f);
		glColor3f(backlightIntensity, backlightIntensity, backlightIntensity); glTexCoord2f(1.00f, 0.00f); glVertex2f( 1.0f,  1.0f);
		glColor3f(backlightIntensity, backlightIntensity, backlightIntensity); glTexCoord2f(1.00f, 0.75f); glVertex2f( 1.0f, -1.0f);
		glColor3f(backlightIntensity, backlightIntensity, backlightIntensity); glTexCoord2f(0.00f, 0.75f); glVertex2f(-1.0f, -1.0f);
	glEnd();
}

/************************************************/
/* RENDERING                                    */
/************************************************/

gboolean screen(GtkWidget *widget, NDSDisplayID displayID)
{
	NDSDisplayID displayToDraw = displayID;

	// we take care to draw the right thing the right place
	// we need to rearrange widgets not to use this trick
	if (ScreenInvert)
	{
		displayToDraw = (displayID == NDSDisplayID_Main) ? NDSDisplayID_Touch : NDSDisplayID_Main;
	}

	if (!my_gl_Begin(displayID))
		return TRUE;

	glLoadIdentity();

	// clear screen
	glClearColor(0.0f,0.0f,0.0f,1.0f);
	glClear(GL_COLOR_BUFFER_BIT);

	glEnable(GL_TEXTURE_2D);
	glDisable(GL_BLEND);
	glDisable(GL_DEPTH_TEST);
	glDisable(GL_DITHER);
	glDisable(GL_STENCIL_TEST);

	if (desmume_running())
	{
		// rotate
		glRotatef(ScreenRotate, 0.0, 0.0, 1.0);
		if (displayID == NDSDisplayID_Main)
		{
			my_gl_ScreenTex();
		}
	}
	
	// apply part of the texture
	my_gl_ScreenTexApply(displayToDraw);

	my_gl_End(displayID);
	
	return TRUE;
}

#endif /* if GTKGLEXT_AVAILABLE */
