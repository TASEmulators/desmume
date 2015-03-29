/*
	Copyright (C) 2009-2015 DeSmuME team

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

#include "driver.h"

#include "debug.h"
#include "rasterize.h"
#include "gfx3d.h"
#include "texcache.h"


static VIEW3D_Driver nullView3d;
BaseDriver::BaseDriver()
: view3d(NULL)
{
	VIEW3D_Shutdown();
}

void BaseDriver::VIEW3D_Shutdown()
{
	if(view3d != &nullView3d) delete view3d;
	view3d = &nullView3d;
}

void BaseDriver::VIEW3D_Init()
{
	VIEW3D_Shutdown();
}

BaseDriver::~BaseDriver()
{
}

void BaseDriver::USR_InfoMessage(const char *message)
{
	LOG("%s\n", message);
}

