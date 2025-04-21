/*
	Copyright (C) 2009-2016 DeSmuME team

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

#include <string>
#include <vector>
#include <string.h>
#include "types.h"

#if defined(HOST_WINDOWS)
	#define WIN32_LEAN_AND_MEAN
	#include <windows.h>
	#include <direct.h>

		#include "frontend/windows/winutil.h"
		#include "frontend/windows/resource.h"

#if defined(__MINGW32__)
#define mkdir(A, B) mkdir(A)
#endif

#elif !defined(DESMUME_COCOA)
	#include <glib.h>
#endif /* HOST_WINDOWS */

#include "time.h"

#ifdef HOST_WINDOWS
	#define FILE_EXT_DELIMITER_CHAR		'.'
	#define DIRECTORY_DELIMITER_CHAR	'\\'
	#define ALL_DIRECTORY_DELIMITER_STRING "/\\"
#else
	#define FILE_EXT_DELIMITER_CHAR		'.'
	#define DIRECTORY_DELIMITER_CHAR	'/'
	#define ALL_DIRECTORY_DELIMITER_STRING "/"
#endif

#ifdef HOST_WINDOWS
void FCEUD_MakePathDirs(const char *fname);
#endif

class Path
{
public:
	static bool IsPathRooted (const std::string &path);
	static std::string GetFileDirectoryPath(std::string filePath);
	static std::string GetFileNameFromPath(std::string filePath);
	static std::string ScrubInvalid(std::string str);
	static std::string GetFileNameWithoutExt(std::string fileName);
	static std::string GetFileNameFromPathWithoutExt(std::string filePath);
	static std::string GetFileExt(std::string fileName);
};

class PathInfo
{
public:

	std::string path;
	std::string RomName;
	std::string RomDirectory;

	#define MAX_FORMAT		20
	#define SECTION			"PathSettings"
	#define LSECTION			L"PathSettings"

	#define ROMKEY			L"Roms"
	#define BATTERYKEY		L"Battery"
	#define SRAMIMPORTKEY	L"SramImportExport"
	#define STATEKEY		L"States"
	#define STATESLOTKEY	L"StateSlots"
	#define SCREENSHOTKEY	L"Screenshots"
	#define AVIKEY			L"AviFiles"
	#define CHEATKEY		L"Cheats"
	#define R4FORMATKEY		"R4format"
	#define SOUNDKEY		L"SoundSamples"
	#define FIRMWAREKEY		L"Firmware"
	#define FORMATKEY		"format"
	#define DEFAULTFORMATKEY "defaultFormat"
	#define NEEDSSAVINGKEY	"needsSaving"
	#define LASTVISITKEY	"lastVisit"
	#define LUAKEY			L"Lua"
	#define SLOT1DKEY		L"Slot1D"
	char screenshotFormat[MAX_FORMAT];
	bool savelastromvisit;

	enum KnownPath
	{
		FIRSTKNOWNPATH = 0,
		ROMS = 0,
		BATTERY,
		SRAM_IMPORT_EXPORT,
		STATES,
		STATE_SLOTS,
		SCREENSHOTS,
		AVI_FILES,
		CHEATS,
		SOUNDS,
		FIRMWARE,
		MODULE,
		SLOT1D,
		MAXKNOWNPATH = MODULE
	};

	//ALL UTF8, NOT SYSTEM LOCALE!! BLEGH!
	//should probably have set locale to utf-8 but it's too late for that
	char pathToRoms[MAX_PATH*8];
	char pathToBattery[MAX_PATH*8];
	char pathToSramImportExport[MAX_PATH*8];
	char pathToStates[MAX_PATH*8];
	char pathToStateSlots[MAX_PATH*8];
	char pathToScreenshots[MAX_PATH*8];
	char pathToAviFiles[MAX_PATH*8];
	char pathToCheats[MAX_PATH*8];
	char pathToSounds[MAX_PATH*8];
	char pathToFirmware[MAX_PATH*8];
	char pathToModule[MAX_PATH*8];
	char pathToLua[MAX_PATH*8];
	char pathToSlot1D[MAX_PATH*8];

	void init(const char *filename);

	void LoadModulePath();

	enum Action
	{
		GET,
		SET
	};

	void GetDefaultPath(char *pathToDefault, const char *key, int maxCount);

	void ReadKey(char *pathToRead, const char *key);
	void ReadKeyW(char *pathToRead, const wchar_t *key);

	void ReadPathSettings();

	void SwitchPath(Action action, KnownPath path, char *buffer);

	std::string getpath(KnownPath path);
	void getpath(KnownPath path, char *buffer);

	void setpath(KnownPath path, std::string value);
	void setpath(KnownPath path, char *buffer);

	void getfilename(char *buffer, int maxCount);

	void getpathnoext(KnownPath path, char *buffer);

	std::string extension();

	std::string noextension();

	void formatname(char *output);

	enum R4Format
	{
		R4_CHEAT_DAT = 0,
		R4_USRCHEAT_DAT = 1
	};
	R4Format r4Format;

	enum ImageFormat
	{
		PNG = 0,
		BMP = 1
	};

	ImageFormat currentimageformat;

	ImageFormat imageformat();

	void SetRomName(const char *filename);

	const char *GetRomName();

	std::string GetRomNameWithoutExtension();

	bool isdsgba(std::string fileName);
};

extern PathInfo path;

