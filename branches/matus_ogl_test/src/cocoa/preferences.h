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

#import <Cocoa/Cocoa.h>

#define PREF_EXECUTE_UPON_LOAD @"Execute Upon Load"

#define PREF_AFTER_LAUNCHED @"When Launching, Load"
#define PREF_AFTER_LAUNCHED_OPTION_NOTHING @"Load Nothing"
#define PREF_AFTER_LAUNCHED_OPTION_LAST_ROM @"Load Last ROM"

#define PREF_FLASH_FILE @"Flash File"

#ifdef GDB_STUB
#define PREF_ARM9_GDB_PORT @"arm9gdb"
#define PREF_ARM7_GDB_PORT @"arm7gdb"
#endif

#define PREF_KEY_A @"A Button"
#define PREF_KEY_B @"B Button"
#define PREF_KEY_X @"X Button"
#define PREF_KEY_Y @"Y Button"
#define PREF_KEY_L @"L Button"
#define PREF_KEY_R @"R Button"
#define PREF_KEY_UP  @"Up Button"
#define PREF_KEY_DOWN @"Down Button"
#define PREF_KEY_LEFT @"Left Button"
#define PREF_KEY_RIGHT @"Right Button"
#define PREF_KEY_START @"Start Button"
#define PREF_KEY_SELECT @"Select Button"

void setAppDefaults(); //this is defined in preferences.m and should be called at app launch

NSView *createPreferencesView(NSString *helpinfo, NSDictionary *options, id delegate); //utility func for creating a preference panel with a set of options
