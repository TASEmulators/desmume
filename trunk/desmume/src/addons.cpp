/*  Copyright (C) 2009 CrazyMax
	Copyright (C) 2009 DeSmuME team

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
    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301  USA
*/

#include "addons.h"
#include <string>

//this is the currently-configured cflash mode
ADDON_CFLASH_MODE CFlash_Mode;

//this is the currently-configured path (directory or filename) for cflash.
//it should be viewed as a parameter for the above.
std::string CFlash_Path;

char GBAgameName[MAX_PATH];

extern ADDONINTERFACE addonNone;
extern ADDONINTERFACE addonCFlash;
extern ADDONINTERFACE addonRumblePak;
extern ADDONINTERFACE addonGBAgame;
extern ADDONINTERFACE addonGuitarGrip;
extern ADDONINTERFACE addonExpMemory;
//extern ADDONINTERFACE addonExternalMic;

ADDONINTERFACE addonList[NDS_ADDON_COUNT] = {
		addonNone,
		addonCFlash,
		addonRumblePak,
		addonGBAgame,
		addonGuitarGrip,
		addonExpMemory
};

ADDONINTERFACE	addon = addonCFlash;		// default none pak
u8				addon_type = NDS_ADDON_CFLASH;

BOOL addonsInit()
{
	return addon.init();
}

void addonsClose()
{
	addon.close();
}

void addonsReset()
{
	addon.reset();
}

BOOL addonsChangePak(u8 type)
{
	printf("addonsChangePak\n");
	if (type > NDS_ADDON_COUNT) return FALSE;
	addon.close();
	addon = addonList[type];
	addon_type = type;
	return addon.init();
}

