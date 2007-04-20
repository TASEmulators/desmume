/* $Id: opengl_collector_3Demu.h,v 1.2 2007-04-20 12:42:58 masscat Exp $
 */
#ifndef _OPENGL_COLLECTOR_3DEMU_H_
#define _OPENGL_COLLECTOR_3DEMU_H_ 1
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

extern GPU3DInterface gpu3D_opengl_collector;


/**
 * This is called before a set of OpenGL functions are called.
 * It is up to the platform code to switch to the correct context and similar.
 *
 * If the platform code is unsucessful then return 0 and the OpenGL functions
 * will not be called. Return 1 otherwise.
 *
 * If this is not overridden then the call will always fail.
 */
extern int (*begin_opengl_ogl_collector_platform)( void);

/**
 * This is called after a set of OpenGL functions have been called, marking
 * the end the OpenGL calls.
 *
 * If not overridden this function will do nothing.
 */
extern void (*end_opengl_ogl_collector_platform)( void);

/**
 * This is called during the OpenGL Collector's initialisation.
 *
 * If the platform code is unsucessful then return 0 and the initialisation
 * will fail. Return 1 otherwise.
 *
 * If this is not overridden then the call will always fail.
 */
extern int (*initialise_ogl_collector_platform)( void);

/**
 * This is called when all the OpenGL functions comprising a single set
 * have been called.
 *
 * If this is not overridden then a call to glFlush() is made.
 */
extern void (*complete_render_ogl_collector_platform)( void);

#endif /* End of _OPENGL_COLLECTOR_3DEMU_H_ */
