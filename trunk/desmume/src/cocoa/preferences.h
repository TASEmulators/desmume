/*  Copyright (C) 2007 Jeff Bland

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

#define PREF_EXECUTE_UPON_LOAD @"Execute Upon Load"

#define PREF_AFTER_LAUNCHED @"When Launching, Load"
#define PREF_AFTER_LAUNCHED_OPTION_NOTHING @"Load Nothing"
#define PREF_AFTER_LAUNCHED_OPTION_LAST_ROM @"Load Last ROM"

#define PREF_FLASH_FILE @"Flash File"

#define PREF_KEY_A @"Key A"
#define PREF_KEY_B @"Key B"
#define PREF_KEY_X @"Key X"
#define PREF_KEY_Y @"Key Y"
#define PREF_KEY_L @"Key L"
#define PREF_KEY_R @"Key R"
#define PREF_KEY_UP  @"Key Up"
#define PREF_KEY_DOWN @"Key Down"
#define PREF_KEY_LEFT @"Key Left"
#define PREF_KEY_RIGHT @"Key Right"
#define PREF_KEY_START @"Key Start"
#define PREF_KEY_SELECT @"Key Select"


void setAppDefaults(); //this is defined in preferences.m and should be called at app launch
