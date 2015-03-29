/*
	Copyright (C) 2008-2015 DeSmuME team

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

#ifndef __RECENTROMS_H_
#define __RECENTROMS_H_

#include <vector>
#include <string>
#include <windows.h>

#include "main.h"

static const unsigned int MAX_RECENT_ROMS = 10;	//To change the recent rom max, simply change this number
static const unsigned int recentRoms_clearid = IDM_RECENT_RESERVED0;			// ID for the Clear recent ROMs item
static const unsigned int recentRoms_baseid = IDM_RECENT_RESERVED1;			//Base identifier for the recent ROMs items

extern std::vector<std::string> RecentRoms;					//The list of recent ROM filenames
extern HMENU recentromsmenu;								// The handle to the recent ROMs menu

void UpdateRecentRoms(const char* filename);
void InitRecentRoms();
void OpenRecentROM(int listNum);
void ClearRecentRoms();
void RemoveRecentRom(const std::string& filename);


#endif __RECENTROMS_H_
