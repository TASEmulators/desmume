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

#include "recentroms.h"

#include <vector>
#include <string>
#include <shlwapi.h>

#include "../common.h"

#include "resource.h"
#include "winutil.h"


std::vector<std::string> RecentRoms;					//The list of recent ROM filenames
HMENU recentromsmenu;				//Handle to the recent ROMs submenu

void PopulateRecentRomsMenu()
{
	//This function will be called to populate the Recent Menu
	//The array must be in the proper order ahead of time

	//UpdateRecentRoms will always call this
	//This will be always called by GetRecentRoms on DesMume startup


	//----------------------------------------------------------------------
	//Get Menu item info

	MENUITEMINFO moo;
	moo.cbSize = sizeof(moo);
	moo.fMask = MIIM_SUBMENU | MIIM_STATE;

	GetMenuItemInfo(GetSubMenu(mainMenu, 0), ID_FILE_RECENTROM, FALSE, &moo);
	moo.hSubMenu = GetSubMenu(recentromsmenu, 0);
	//moo.fState = RecentRoms[0].c_str() ? MFS_ENABLED : MFS_GRAYED;
	moo.fState = MFS_ENABLED;
	SetMenuItemInfo(GetSubMenu(mainMenu, 0), ID_FILE_RECENTROM, FALSE, &moo);

	//-----------------------------------------------------------------------

	//-----------------------------------------------------------------------
	//Clear the current menu items
	for(int x = 0; x < MAX_RECENT_ROMS; x++)
	{
		DeleteMenu(GetSubMenu(recentromsmenu, 0), recentRoms_baseid + x, MF_BYCOMMAND);
	}

	if(RecentRoms.size() == 0)
	{
		EnableMenuItem(GetSubMenu(recentromsmenu, 0), recentRoms_clearid, MF_GRAYED);

		moo.cbSize = sizeof(moo);
		moo.fMask = MIIM_DATA | MIIM_ID | MIIM_STATE | MIIM_TYPE;

		moo.cch = 5;
		moo.fType = 0;
		moo.wID = recentRoms_baseid;
		moo.dwTypeData = "None";
		moo.fState = MF_GRAYED;

		InsertMenuItem(GetSubMenu(recentromsmenu, 0), 0, TRUE, &moo);

		return;
	}

	EnableMenuItem(GetSubMenu(recentromsmenu, 0), recentRoms_clearid, MF_ENABLED);
	DeleteMenu(GetSubMenu(recentromsmenu, 0), recentRoms_baseid, MF_BYCOMMAND);

	HDC dc = GetDC(MainWindow->getHWnd());

	//-----------------------------------------------------------------------
	//Update the list using RecentRoms vector
	for(int x = RecentRoms.size()-1; x >= 0; x--)	//Must loop in reverse order since InsertMenuItem will insert as the first on the list
	{
		string tmp = RecentRoms[x];
		LPSTR tmp2 = (LPSTR)tmp.c_str();

		PathCompactPath(dc, tmp2, 500);

		moo.cbSize = sizeof(moo);
		moo.fMask = MIIM_DATA | MIIM_ID | MIIM_TYPE;

		moo.cch = tmp.size();
		moo.fType = 0;
		moo.wID = recentRoms_baseid + x;
		moo.dwTypeData = tmp2;
		//LOG("Inserting: %s\n",tmp.c_str());  //Debug
		InsertMenuItem(GetSubMenu(recentromsmenu, 0), 0, 1, &moo);
	}

	ReleaseDC(MainWindow->getHWnd(), dc);
	//-----------------------------------------------------------------------

	HWND temp = MainWindow->getHWnd();
	DrawMenuBar(temp);
}

void RemoveRecentRom(const std::string& filename)
{
	vector<string>::iterator x;
	vector<string>::iterator theMatch;
	bool match = false;
	for (x = RecentRoms.begin(); x < RecentRoms.end(); ++x)
	{
		if (filename == *x)
		{
			//We have a match
			match = true;	//Flag that we have a match
			theMatch = x;	//Hold on to the iterator	(Note: the loop continues, so if for some reason we had a duplicate (which wouldn't happen under normal circumstances, it would pick the last one in the list)
		}
	}
	//----------------------------------------------------------------
	//If there was a match, remove it
	if (match)
		RecentRoms.erase(theMatch);

	PopulateRecentRomsMenu();
}

void LoadRecentRoms()
{
	//This function retrieves the recent ROMs stored in the .ini file
	//Then is populates the RecentRomsMenu array

	char tempstr[256];

	// Avoids duplicating when changing the language.
	RecentRoms.clear();

	//Loops through all available recent slots
	for (int x = 0; x < MAX_RECENT_ROMS; x++)
	{
		char keyName[100];
		sprintf(keyName,"Recent Rom %d",x);

		GetPrivateProfileString("General",keyName,"", tempstr, 256, IniName);
		if (tempstr[0])
			RecentRoms.push_back(tempstr);
	}
}

void SaveRecentRoms()
{
	//This function stores the RecentRomsMenu array to the .ini file

	//Loops through all available recent slots
	for (int x = 0; x < MAX_RECENT_ROMS; x++)
	{
		char keyName[100];
		sprintf(keyName,"Recent Rom %d",x);

		if (x < (int)RecentRoms.size())	//If it exists in the array, save it
			WritePrivateProfileString("General",keyName,RecentRoms[x].c_str(),IniName);
		else						//Else, make it empty
			WritePrivateProfileString("General",keyName, "",IniName);
	}
}

void ClearRecentRoms()
{
	RecentRoms.clear();
	SaveRecentRoms();
	PopulateRecentRomsMenu();
	MainWindowToolbar->EnableButtonDropdown(IDM_OPEN, false);
}


void UpdateRecentRoms(const char* filename)
{
	//This function assumes filename is a ROM filename that was successfully loaded

	string newROM = filename; //Convert to std::string

	//--------------------------------------------------------------
	//Check to see if filename is in list
	vector<string>::iterator x;
	vector<string>::iterator theMatch;
	bool match = false;
	for (x = RecentRoms.begin(); x < RecentRoms.end(); ++x)
	{
		if (newROM == *x)
		{
			//We have a match
			match = true;	//Flag that we have a match
			theMatch = x;	//Hold on to the iterator	(Note: the loop continues, so if for some reason we had a duplicate (which wouldn't happen under normal circumstances, it would pick the last one in the list)
		}
	}
	//----------------------------------------------------------------
	//If there was a match, remove it
	if (match)
		RecentRoms.erase(theMatch);

	RecentRoms.insert(RecentRoms.begin(), newROM);	//Add to the array

	//If over the max, we have too many, so remove the last entry
	if (RecentRoms.size() > MAX_RECENT_ROMS)	
		RecentRoms.pop_back();

	//Debug
	//for (int x = 0; x < RecentRoms.size(); x++)
	//	LOG("Recent ROM: %s\n",RecentRoms[x].c_str());

	PopulateRecentRomsMenu();
	SaveRecentRoms();
	MainWindowToolbar->EnableButtonDropdown(IDM_OPEN, true);
}


void InitRecentRoms()
{
	recentromsmenu = LoadMenu(hAppInst, MAKEINTRESOURCE(RECENTROMS));
	LoadRecentRoms();
	PopulateRecentRomsMenu();
	MainWindowToolbar->EnableButtonDropdown(IDM_OPEN, !RecentRoms.empty());
}
