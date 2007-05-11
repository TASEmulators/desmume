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

char NDS_nullFunc1		(void){ return 1; }
void NDS_nullFunc2		(void){}
void NDS_nullFunc3		(unsigned long v){}
void NDS_nullFunc4		(signed long v)	{}
void NDS_nullFunc5		(unsigned int v){}
void NDS_nullFunc6		(unsigned int one, unsigned int two, unsigned int v){}
int  NDS_nullFunc7		(void) {return 0;}
long NDS_nullFunc8		(unsigned int index){ return 0; }
void NDS_nullFunc9		(int line, unsigned short * DST) { };

GPU3DInterface gpu3DNull = { NDS_nullFunc1,
						NDS_nullFunc3,
						NDS_nullFunc3,
						NDS_nullFunc3,
						NDS_nullFunc3,
						NDS_nullFunc3,
						NDS_nullFunc3,
						NDS_nullFunc2,
						NDS_nullFunc4,
						NDS_nullFunc4,
						NDS_nullFunc3,
						NDS_nullFunc3,
						NDS_nullFunc2,
						NDS_nullFunc4,
						NDS_nullFunc4,
						NDS_nullFunc4,
						NDS_nullFunc4,
						NDS_nullFunc4,
						NDS_nullFunc4,
						NDS_nullFunc3,
						NDS_nullFunc2,
						NDS_nullFunc3,
						NDS_nullFunc5,
						NDS_nullFunc3,
						NDS_nullFunc6,
						NDS_nullFunc3,
						NDS_nullFunc5,
						NDS_nullFunc7,
						NDS_nullFunc7,
						NDS_nullFunc3,
						NDS_nullFunc3,
						NDS_nullFunc3,
						NDS_nullFunc3,
						NDS_nullFunc3,
						NDS_nullFunc3,
						NDS_nullFunc3,
						NDS_nullFunc3,
						NDS_nullFunc3,
						NDS_nullFunc3,
						NDS_nullFunc3,
						NDS_nullFunc3,
						NDS_nullFunc3,
						NDS_nullFunc3,
						
						NDS_nullFunc8,
						NDS_nullFunc8,
						NDS_nullFunc9 };

GPU3DInterface *gpu3D = &gpu3DNull;

void NDS_3D_SetDriver (int core3DIndex)
{
	gpu3D = core3DList[core3DIndex];
}
