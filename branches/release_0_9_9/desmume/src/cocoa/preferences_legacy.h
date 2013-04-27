/*
	Copyright (C) 2007 Jeff Bland
	Copyright (C) 2011 Roger Manuel
	Copyright (C) 2012 DeSmuME team

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

#import <Cocoa/Cocoa.h>

#if MAC_OS_X_VERSION_MIN_REQUIRED <= MAC_OS_X_VERSION_10_4
	#include "macosx_10_4_compat.h"
#endif

#define PREF_EXECUTE_UPON_LOAD @"Execute Upon Load"

#define PREF_AFTER_LAUNCHED @"When Launching, Load"
#define PREF_AFTER_LAUNCHED_OPTION_NOTHING @"Load Nothing"
#define PREF_AFTER_LAUNCHED_OPTION_LAST_ROM @"Load Last ROM"

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
