/* $Id: opengl_collector_3Demu.h,v 1.1 2007-04-17 16:47:11 masscat Exp $
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


extern int (*begin_opengl_ogl_collector_platform)( void);
extern void (*end_opengl_ogl_collector_platform)( void);
extern int (*initialise_ogl_collector_platform)( void);


#endif /* End of _OPENGL_COLLECTOR_3DEMU_H_ */
