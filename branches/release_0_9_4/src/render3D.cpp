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

#include "render3D.h"

int cur3DCore = GPU3D_NULL;

static void NDS_nullFunc1		(void){}
static char NDS_nullFunc2		(void){ return 1; }

GPU3DInterface gpu3DNull = { 
	"None",
	NDS_nullFunc2, //NDS_3D_Init
	NDS_nullFunc1, //NDS_3D_Reset
	NDS_nullFunc1, //NDS_3D_Close
	NDS_nullFunc1, //NDS_3D_Render
	NDS_nullFunc1, //NDS_3D_VramReconfigureSignal
};

GPU3DInterface *gpu3D = &gpu3DNull;

void NDS_3D_SetDriver (int core3DIndex)
{
	cur3DCore = core3DIndex;
	gpu3D = core3DList[cur3DCore];
}

void NDS_3D_ChangeCore(int newCore)
{
	gpu3D->NDS_3D_Close();
	NDS_3D_SetDriver(newCore);
	if(gpu3D->NDS_3D_Init() == 0)
	{
		NDS_3D_SetDriver(GPU3D_NULL);
		gpu3D->NDS_3D_Init();
	}
}
