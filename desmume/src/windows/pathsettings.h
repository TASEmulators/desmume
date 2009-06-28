/*  
	Copyright (C) 2007 Hicoder

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

#ifndef _PATHSETTINGS_H_
#define _PATHSETTINGS_H_

#include "resource.h"



#define SECTION			"PathSettings"

#define ROMKEY			"Roms"
#define BATTERYKEY		"Battery"
#define STATEKEY		"States"
#define SCREENSHOTKEY	"Screenshots"
#define AVIKEY		"AviFiles"
#define CHEATKEY		"Cheats"
#define SOUNDKEY		"SoundSamples"
#define FIRMWAREKEY		"Firmware"
#define FORMATKEY		"format"
#define DEFAULTFORMATKEY "defaultFormat"
#define NEEDSSAVINGKEY	"needsSaving"
#define LASTVISITKEY	"lastVisit"

#define MAX_FORMAT		20


enum KnownPath
{
	FIRSTKNOWNPATH = 0,
	ROMS = 0,
	BATTERY,
	STATES, 
	SCREENSHOTS,
	AVI_FILES,
	CHEATS,
	SOUNDS,
	FIRMWARE,
	MODULE,
	MAXKNOWNPATH = MODULE
};

enum ImageFormat
{
	PNG = IDC_PNG,
	BMP = IDC_BMP
};

enum Action
{
	GET,
	SET
};

void GetFilename(char *buffer, int maxCount);
void GetFullPathNoExt(KnownPath path, char *buffer, int maxCount);
void GetPathFor(KnownPath path, char *buffer, int maxCount);
void GetDefaultPath(char *buffer,const char *key, int maxCount);
void WritePathSettings();
void ReadPathSettings();

void SetPathFor(KnownPath path, char *buffer);
void FormatName(char *output, int maxCount);

ImageFormat GetImageFormatType();

BOOL SavePathForRomVisit();

LRESULT CALLBACK PathSettingsDlgProc(HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam);

#endif 
