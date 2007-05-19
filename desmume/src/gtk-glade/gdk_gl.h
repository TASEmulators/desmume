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

#include "globals.h"
// comment for GL :D
//#undef HAVE_LIBGDKGLEXT_X11_1_0
#ifdef HAVE_LIBGDKGLEXT_X11_1_0
	#include <GL/gl.h>
	#include <GL/glext.h>
	#include <gdk/gdkgl.h>
	#include <gtk/gtkglwidget.h>
#endif

	
BOOL my_gl_Begin (int screen);
void my_gl_End (int screen);
void my_gl_Clear(int screen);
void my_gl_DrawBeautifulQuad();

void init_GL_capabilities();
void init_GL(GtkWidget * widget, int screen, int share_num);
int init_GL_free_s(GtkWidget * widget, int share_num);
int init_GL_free(GtkWidget * widget);
void register_gl_fun(fun_gl_Begin beg,fun_gl_End end);
#ifdef HAVE_LIBGDKGLEXT_X11_1_0
static void gtk_init_main_gl_area(GtkWidget *, gpointer);
static void gtk_init_sub_gl_area(GtkWidget *, gpointer);
static int top_screen_expose_fn(GtkWidget *, GdkEventExpose *, gpointer);
static int bottom_screen_expose_fn(GtkWidget *, GdkEventExpose *, gpointer);
static gboolean common_configure_fn( GtkWidget *, GdkEventConfigure *);
#endif
void reshape (GtkWidget * widget, int screen);
gboolean screen (GtkWidget * widget, int off);
