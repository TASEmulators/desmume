/* $Id: gdk_3Demu.c,v 1.4 2007-07-15 21:50:30 evilynux Exp $
 */
/*  
	Copyright (C) 2006-2007 Ben Jaques

    This file is part of DeSmuME

    DeSmuME is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    DeSmuME is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with DeSmuME; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/
#ifdef GTKGLEXT_AVAILABLE

#include <gdk/gdkgl.h>
#include <gdk/gdk.h>

// Localization
#include <libintl.h>
#define _(String) gettext (String)

#include "../types.h"
#include "../render3D.h"
#include "../OGLRender.h"
#include "gdk_3Demu.h"

/*
 * The GDK 3D emulation.
 * This uses the OpenGL Collector plugin, using gdkGLext for the platform
 * specific helper functions.
 */


static GdkPixmap *target_pixmap;
static GdkGLContext *glcontext = NULL;
static GdkGLDrawable *gldrawable;


#if 0 /* not used */
static void
print_gl_config_attrib (GdkGLConfig *glconfig,
                        const gchar *attrib_str,
                        int          attrib,
                        gboolean     is_boolean)
{
  int value;

  g_print ("%s = ", attrib_str);
  if (gdk_gl_config_get_attrib (glconfig, attrib, &value))
    {
      if (is_boolean)
        g_print ("%s\n", value == TRUE ? "TRUE" : "FALSE");
      else
        g_print ("%d\n", value);
    }
  else
    g_print (_("*** Cannot get %s attribute value\n"), attrib_str);
}


static void
examine_gl_config_attrib (GdkGLConfig *glconfig)
{
  g_print ("\nOpenGL visual configurations :\n\n");

  g_print ("gdk_gl_config_is_rgba (glconfig) = %s\n",
           gdk_gl_config_is_rgba (glconfig) ? "TRUE" : "FALSE");
  g_print ("gdk_gl_config_is_double_buffered (glconfig) = %s\n",
           gdk_gl_config_is_double_buffered (glconfig) ? "TRUE" : "FALSE");
  g_print ("gdk_gl_config_is_stereo (glconfig) = %s\n",
           gdk_gl_config_is_stereo (glconfig) ? "TRUE" : "FALSE");
  g_print ("gdk_gl_config_has_alpha (glconfig) = %s\n",
           gdk_gl_config_has_alpha (glconfig) ? "TRUE" : "FALSE");
  g_print ("gdk_gl_config_has_depth_buffer (glconfig) = %s\n",
           gdk_gl_config_has_depth_buffer (glconfig) ? "TRUE" : "FALSE");
  g_print ("gdk_gl_config_has_stencil_buffer (glconfig) = %s\n",
           gdk_gl_config_has_stencil_buffer (glconfig) ? "TRUE" : "FALSE");
  g_print ("gdk_gl_config_has_accum_buffer (glconfig) = %s\n",
           gdk_gl_config_has_accum_buffer (glconfig) ? "TRUE" : "FALSE");

  g_print ("\n");

  print_gl_config_attrib (glconfig, "GDK_GL_USE_GL",
                          GDK_GL_USE_GL,           TRUE);
  print_gl_config_attrib (glconfig, "GDK_GL_BUFFER_SIZE",
                          GDK_GL_BUFFER_SIZE,      FALSE);
  print_gl_config_attrib (glconfig, "GDK_GL_LEVEL",
                          GDK_GL_LEVEL,            FALSE);
  print_gl_config_attrib (glconfig, "GDK_GL_RGBA",
                          GDK_GL_RGBA,             TRUE);
  print_gl_config_attrib (glconfig, "GDK_GL_DOUBLEBUFFER",
                          GDK_GL_DOUBLEBUFFER,     TRUE);
  print_gl_config_attrib (glconfig, "GDK_GL_STEREO",
                          GDK_GL_STEREO,           TRUE);
  print_gl_config_attrib (glconfig, "GDK_GL_AUX_BUFFERS",
                          GDK_GL_AUX_BUFFERS,      FALSE);
  print_gl_config_attrib (glconfig, "GDK_GL_RED_SIZE",
                          GDK_GL_RED_SIZE,         FALSE);
  print_gl_config_attrib (glconfig, "GDK_GL_GREEN_SIZE",
                          GDK_GL_GREEN_SIZE,       FALSE);
  print_gl_config_attrib (glconfig, "GDK_GL_BLUE_SIZE",
                          GDK_GL_BLUE_SIZE,        FALSE);
  print_gl_config_attrib (glconfig, "GDK_GL_ALPHA_SIZE",
                          GDK_GL_ALPHA_SIZE,       FALSE);
  print_gl_config_attrib (glconfig, "GDK_GL_DEPTH_SIZE",
                          GDK_GL_DEPTH_SIZE,       FALSE);
  print_gl_config_attrib (glconfig, "GDK_GL_STENCIL_SIZE",
                          GDK_GL_STENCIL_SIZE,     FALSE);
  print_gl_config_attrib (glconfig, "GDK_GL_ACCUM_RED_SIZE",
                          GDK_GL_ACCUM_RED_SIZE,   FALSE);
  print_gl_config_attrib (glconfig, "GDK_GL_ACCUM_GREEN_SIZE",
                          GDK_GL_ACCUM_GREEN_SIZE, FALSE);
  print_gl_config_attrib (glconfig, "GDK_GL_ACCUM_BLUE_SIZE",
                          GDK_GL_ACCUM_BLUE_SIZE,  FALSE);
  print_gl_config_attrib (glconfig, "GDK_GL_ACCUM_ALPHA_SIZE",
                          GDK_GL_ACCUM_ALPHA_SIZE, FALSE);

  g_print ("\n");
}
#endif

static bool
begin_opengl_region_gdk_3d( void) {
  bool failed = false;

  gdk_error_trap_push();
  failed = !gdk_gl_drawable_gl_begin(gldrawable, glcontext);
  gdk_flush();
  failed = failed | gdk_error_trap_pop();

  if (failed) return false;

  return true;
}

static void
end_opengl_region_gdk_3d( void) {
  gdk_gl_drawable_gl_end (gldrawable);
}

static bool
initialise_gdk_3d( void) {
  /* this does nothing */
  return true;
}

int
init_opengl_gdk_3Demu( GdkDrawable * drawable) {
  GdkGLConfig *glconfig;

  /* create the off screen pixmap */
  target_pixmap = gdk_pixmap_new ( drawable, 256, 192, -1);

  if ( target_pixmap == NULL) {
      g_print (_("*** Failed to create pixmap.\n"));
      return 0;
  }

  glconfig = gdk_gl_config_new_by_mode ((GdkGLConfigMode) (GDK_GL_MODE_RGBA   |
                                        GDK_GL_MODE_DEPTH  |
                                        GDK_GL_MODE_STENCIL |
                                        GDK_GL_MODE_SINGLE));
  if (glconfig == NULL)
    {
      g_print (_("*** No appropriate OpenGL-capable visual found.\n"));
      return 0;
    }

  /*
   * Set OpenGL-capability to the pixmap
   */

  gldrawable = GDK_GL_DRAWABLE (gdk_pixmap_set_gl_capability (target_pixmap,
                                                              glconfig,
                                                              NULL));

  if ( gldrawable == NULL) {
    g_print (_("Failed to create the GdkGLPixmap\n"));
    return 0;
  }

  glcontext = gdk_gl_context_new (gldrawable,
                                  NULL,
                                  FALSE,
                                  GDK_GL_RGBA_TYPE);
  if (glcontext == NULL)
    {
      g_print (_("Connot create the OpenGL rendering context\n"));
      return 0;
    }

  oglrender_init = initialise_gdk_3d;
  oglrender_beginOpenGL = begin_opengl_region_gdk_3d;
  oglrender_endOpenGL = end_opengl_region_gdk_3d;

  return 1;
}

#endif
