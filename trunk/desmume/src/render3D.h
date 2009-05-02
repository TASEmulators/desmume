/*  
	Copyright (C) 2006-2007 shash

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

#ifndef RENDER3D_H
#define RENDER3D_H

#include "types.h"

//not using this right now
#define CALL_CONVENTION

typedef struct Render3DInterface
{
	// The name of the plugin, this name will appear in the plugins list
	const char * name;

	//called once when the plugin starts up
	char (CALL_CONVENTION*  NDS_3D_Init)					(void);
	
	//called when the emulator resets (is this necessary?)
	void (CALL_CONVENTION*  NDS_3D_Reset)					(void);
	
	//called when the plugin shuts down
	void (CALL_CONVENTION*  NDS_3D_Close)					(void);
	
	//called when the renderer should do its job and render the current display lists
	void (CALL_CONVENTION*  NDS_3D_Render)					(void);

	//called when the emulator reconfigures its vram. you may need to invalidate your texture cache.
	void (CALL_CONVENTION*  NDS_3D_VramReconfigureSignal)	();

} GPU3DInterface;

extern int cur3DCore;

// gpu 3D core list, per port
extern GPU3DInterface *core3DList[];

// Default null plugin
#define GPU3D_NULL 0
extern GPU3DInterface gpu3DNull;

// Extern pointer
extern GPU3DInterface *gpu3D;

void NDS_3D_SetDriver (int core3DIndex);
void NDS_3D_ChangeCore(int newCore);

#endif
 
