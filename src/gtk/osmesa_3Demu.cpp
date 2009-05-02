/*  
    Copyright (C) 2009 Guillaume Duhamel

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

#ifdef HAVE_LIBOSMESA
#include <stdlib.h>
#include <GL/osmesa.h>
#include "../OGLRender.h"
#include "osmesa_3Demu.h"

static bool osmesa_beginOpenGL(void) {
  return 1;
}

static void osmesa_endOpenGL(void) {
}

static bool osmesa_init(void) {

  return true;
}

int init_osmesa_3Demu(void) {
  void * buffer;
  OSMesaContext ctx;

  ctx = OSMesaCreateContext(OSMESA_RGBA, NULL);
  buffer = malloc(256 * 192 * 4);
  OSMesaMakeCurrent(ctx, buffer, GL_UNSIGNED_BYTE, 256, 192);

  oglrender_init = osmesa_init;
  oglrender_beginOpenGL = osmesa_beginOpenGL;
  oglrender_endOpenGL = osmesa_endOpenGL;

  return 1;
}
#endif
