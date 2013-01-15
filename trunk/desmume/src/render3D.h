/*
	Copyright (C) 2006-2007 shash
	Copyright (C) 2007-2012 DeSmuME team

	This file is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 2 of the License, or
	(at your option) any later version.

	This file is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with the this software.  If not, see <http://www.gnu.org/licenses/>.
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
	char (CALL_CONVENTION*  NDS_3D_Init)					();
	
	//called when the emulator resets (is this necessary?)
	void (CALL_CONVENTION*  NDS_3D_Reset)					();
	
	//called when the plugin shuts down
	void (CALL_CONVENTION*  NDS_3D_Close)					();
	
	//called when the renderer should do its job and render the current display lists
	void (CALL_CONVENTION*  NDS_3D_Render)					();
	
	// Called whenever 3D rendering needs to finish. This function should block the calling thread
	// and only release the block when 3D rendering is finished. (Before reading the 3D layer, be
	// sure to always call this function.)
	void (CALL_CONVENTION*	NDS_3D_RenderFinish)			();

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

char Default3D_Init();
void Default3D_Reset();
void Default3D_Close();
void Default3D_Render();
void Default3D_RenderFinish();
void Default3D_VramReconfigureSignal();

void NDS_3D_SetDriver (int core3DIndex);
bool NDS_3D_ChangeCore(int newCore);

#endif
 
