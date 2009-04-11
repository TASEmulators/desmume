/* gdk_gl.h - this file is part of DeSmuME
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

#ifndef __GDKGL_H__
#define __GDKGL_H__

#include "globals.h"

#ifdef GTKGLEXT_AVAILABLE
	#include <GL/gl.h>
	#include <GL/glext.h>
	#include <gdk/gdkgl.h>
	#include <gtk/gtkglwidget.h>
#endif

	
BOOL my_gl_Begin (int screen);
void my_gl_End (int screen);
void my_gl_Clear(int screen);
void my_gl_DrawBeautifulQuad( void);
void my_gl_Identity( void);

void init_GL_capabilities( int use_software_convert);
void init_GL(GtkWidget * widget, int screen, int share_num);
int init_GL_free_s(GtkWidget * widget, int share_num);
int init_GL_free(GtkWidget * widget);
void reshape (GtkWidget * widget, int screen);
gboolean screen (GtkWidget * widget, int off);

#endif
