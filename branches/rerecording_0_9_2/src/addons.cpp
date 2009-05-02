/*  Copyright (C) 2006 yopyop
    yopyop156@ifrance.com
    yopyop156.ifrance.com

    Copyright (C) 2009 CrazyMax
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

char CFlashName[MAX_PATH];
char CFlashPath[MAX_PATH];
u8	 CFlashUseRomPath = TRUE;
u8	 CFlashUsePath = TRUE;

char GBAgameName[MAX_PATH];

extern ADDONINTERFACE addonNone;
extern ADDONINTERFACE addonCFlash;
extern ADDONINTERFACE addonRumblePak;
extern ADDONINTERFACE addonGBAgame;
//extern ADDONINTERFACE addonExternalMic;

ADDONINTERFACE addonList[NDS_ADDON_COUNT] = {
		addonNone,
		addonCFlash,
		addonRumblePak,
		addonGBAgame};

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
	if (type > NDS_ADDON_COUNT) return FALSE;
	addon.close();
	addon = addonList[type];
	addon_type = type;
	return addon.init();
}

