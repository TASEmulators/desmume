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

#include <string>
#include "types.h"

#if defined(HOST_WINDOWS)
	#define WIN32_LEAN_AND_MEAN
	#include <windows.h>
	#include <direct.h>
	#define mkdir _mkdir

	#ifndef DESMUME_QT
		#include "windows/winutil.h"
		#include "windows/resource.h"
	#endif
#elif !defined(DESMUME_COCOA)
	#include <glib.h>
#endif /* HOST_WINDOWS */

#include "time.h"
#include "utils/xstring.h"

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

	#define ROMKEY			"Roms"
	#define BATTERYKEY		"Battery"
	#define STATEKEY		"States"
	#define SCREENSHOTKEY	"Screenshots"
	#define AVIKEY			"AviFiles"
	#define CHEATKEY		"Cheats"
	#define R4FORMATKEY		"R4format"
	#define SOUNDKEY		"SoundSamples"
	#define FIRMWAREKEY		"Firmware"
	#define FORMATKEY		"format"
	#define DEFAULTFORMATKEY "defaultFormat"
	#define NEEDSSAVINGKEY	"needsSaving"
	#define LASTVISITKEY	"lastVisit"
	#define LUAKEY			"Lua"
	#define SLOT1DKEY		"Slot1D"
	char screenshotFormat[MAX_FORMAT];
	bool savelastromvisit;

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
		SLOT1D,
		MAXKNOWNPATH = MODULE
	};

	char pathToRoms[MAX_PATH];
	char pathToBattery[MAX_PATH];
	char pathToStates[MAX_PATH];
	char pathToScreenshots[MAX_PATH];
	char pathToAviFiles[MAX_PATH];
	char pathToCheats[MAX_PATH];
	char pathToSounds[MAX_PATH];
	char pathToFirmware[MAX_PATH];
	char pathToModule[MAX_PATH];
	char pathToLua[MAX_PATH];
	char pathToSlot1D[MAX_PATH];

	void init(const char *filename) 
	{
		path = std::string(filename);

		//extract the internal part of the logical rom name
		std::vector<std::string> parts = tokenize_str(filename,"|");
		SetRomName(parts[parts.size()-1].c_str());
		LoadModulePath();
#if !defined(WIN32) && !defined(DESMUME_COCOA)
		ReadPathSettings();
#endif
		
	}

	void LoadModulePath()
	{
#if defined(HOST_WINDOWS)

		char *p;
		ZeroMemory(pathToModule, sizeof(pathToModule));

		GetModuleFileName(NULL, pathToModule, sizeof(pathToModule));
		p = pathToModule + lstrlen(pathToModule);
		while (p >= pathToModule && *p != DIRECTORY_DELIMITER_CHAR) p--;
		if (++p >= pathToModule) *p = 0;

#ifndef DESMUME_QT
		extern char* _hack_alternateModulePath;
		if(_hack_alternateModulePath)
		{
			strcpy(pathToModule,_hack_alternateModulePath);
		}
#endif
#elif defined(DESMUME_COCOA)
		std::string pathStr = Path::GetFileDirectoryPath(path);

		strncpy(pathToModule, pathStr.c_str(), MAX_PATH);
#else
		char *cwd = g_build_filename(g_get_user_config_dir(), "desmume", NULL);
		g_mkdir_with_parents(cwd, 0755);
		strncpy(pathToModule, cwd, MAX_PATH);
		g_free(cwd);
#endif
	}

	enum Action
	{
		GET,
		SET
	};

	void GetDefaultPath(char *pathToDefault, const char *key, int maxCount)
	{
#ifdef HOST_WINDOWS
		std::string temp = (std::string)"." + DIRECTORY_DELIMITER_CHAR + pathToDefault;
		strncpy(pathToDefault, temp.c_str(), maxCount);
#else
		strncpy(pathToDefault, pathToModule, maxCount);
#endif
	}

	void ReadKey(char *pathToRead, const char *key)
	{
#ifdef HOST_WINDOWS
		GetPrivateProfileString(SECTION, key, key, pathToRead, MAX_PATH, IniName);
		if(strcmp(pathToRead, key) == 0) {
			//since the variables are all intialized in this file they all use MAX_PATH
			GetDefaultPath(pathToRead, key, MAX_PATH);
		}
#else
		//since the variables are all intialized in this file they all use MAX_PATH
		GetDefaultPath(pathToRead, key, MAX_PATH);
#endif
	}

	void ReadPathSettings()
	{
		if( ( strcmp(pathToModule, "") == 0) || !pathToModule)
			LoadModulePath();

		ReadKey(pathToRoms, ROMKEY);
		ReadKey(pathToBattery, BATTERYKEY);
		ReadKey(pathToStates, STATEKEY);
		ReadKey(pathToScreenshots, SCREENSHOTKEY);
		ReadKey(pathToAviFiles, AVIKEY);
		ReadKey(pathToCheats, CHEATKEY);
		ReadKey(pathToSounds, SOUNDKEY);
		ReadKey(pathToFirmware, FIRMWAREKEY);
		ReadKey(pathToLua, LUAKEY);
		ReadKey(pathToSlot1D, SLOT1DKEY);
#ifdef HOST_WINDOWS
		GetPrivateProfileString(SECTION, FORMATKEY, "%f_%s_%r", screenshotFormat, MAX_FORMAT, IniName);
		savelastromvisit	= GetPrivateProfileBool(SECTION, LASTVISITKEY, true, IniName);
		currentimageformat	= (ImageFormat)GetPrivateProfileInt(SECTION, DEFAULTFORMATKEY, PNG, IniName);
		r4Format = (R4Format)GetPrivateProfileInt(SECTION, R4FORMATKEY, R4_CHEAT_DAT, IniName);
		if ((r4Format != R4_CHEAT_DAT) && (r4Format != R4_USRCHEAT_DAT))
		{
			r4Format = R4_USRCHEAT_DAT;
			WritePrivateProfileInt(SECTION, R4FORMATKEY, r4Format, IniName);
		}
#endif
	/*
		needsSaving		= GetPrivateProfileInt(SECTION, NEEDSSAVINGKEY, TRUE, IniName);
		if(needsSaving)
		{
			needsSaving = FALSE;
			WritePathSettings();
		}*/
	}

	void SwitchPath(Action action, KnownPath path, char *buffer)
	{
		char *pathToCopy = 0;
		switch(path)
		{
		case ROMS:
			pathToCopy = pathToRoms;
			break;
		case BATTERY:
			pathToCopy = pathToBattery;
			break;
		case STATES:
			pathToCopy = pathToStates;
			break;
		case SCREENSHOTS:
			pathToCopy = pathToScreenshots;
			break;
		case AVI_FILES:
			pathToCopy = pathToAviFiles;
			break;
		case CHEATS:
			pathToCopy = pathToCheats;
			break;
		case SOUNDS:
			pathToCopy = pathToSounds;
			break;
		case FIRMWARE:
			pathToCopy = pathToFirmware;
			break;
		case MODULE:
			pathToCopy = pathToModule;
			break;
		case SLOT1D:
			pathToCopy = pathToSlot1D;
			break;
		}

		if(action == GET)
		{
			std::string thePath = pathToCopy;
			std::string relativePath = (std::string)"." + DIRECTORY_DELIMITER_CHAR;
			
			int len = (int)thePath.size()-1;

			if(len == -1)
				thePath = relativePath;
			else 
				if(thePath[len] != DIRECTORY_DELIMITER_CHAR) 
					thePath += DIRECTORY_DELIMITER_CHAR;
	
			if(!Path::IsPathRooted(thePath))
			{
				thePath = (std::string)pathToModule + thePath;
			}

			strncpy(buffer, thePath.c_str(), MAX_PATH);
			#ifdef HOST_WINDOWS
			FCEUD_MakePathDirs(buffer);
			#endif
		}
		else if(action == SET)
		{
			int len = strlen(buffer)-1;
			if(buffer[len] == DIRECTORY_DELIMITER_CHAR) 
				buffer[len] = '\0';

			strncpy(pathToCopy, buffer, MAX_PATH);
		}
	}

	std::string getpath(KnownPath path)
	{
		char temp[MAX_PATH];
		SwitchPath(GET, path, temp);
		return temp;
	}

	void getpath(KnownPath path, char *buffer)
	{
		SwitchPath(GET, path, buffer);
	}

	void setpath(KnownPath path, char *buffer)
	{
		SwitchPath(SET, path, buffer);
	}

	void getfilename(char *buffer, int maxCount)
	{
		strcpy(buffer,noextension().c_str());
	}

	void getpathnoext(KnownPath path, char *buffer)
	{
		getpath(path, buffer);
		strcat(buffer, GetRomNameWithoutExtension().c_str());
	}

	std::string extension()
	{
		return Path::GetFileExt(path);
	}

	std::string noextension()
	{
		std::string romNameWithPath = Path::GetFileDirectoryPath(path) + DIRECTORY_DELIMITER_CHAR + Path::GetFileNameWithoutExt(RomName);
		
		return romNameWithPath;
	}

	void formatname(char *output)
	{
		// Except 't' for tick and 'r' for random.
		const char* strftimeArgs = "AbBcCdDeFgGhHIjmMnpRStTuUVwWxXyYzZ%";

		std::string file;
		time_t now = time(NULL);
		tm *time_struct = localtime(&now);

		srand((unsigned)now);

		for (char*  p = screenshotFormat,
			 *end = p + sizeof(screenshotFormat); p < end; p++)
			{
			if (*p != '%')
				{
				file.append(1, *p);
			}
			else
			{
				p++;

				if (*p == 'f')
				{
					file.append(GetRomNameWithoutExtension());
				}
				else if (*p == 'r')
				{
					file.append(stditoa(rand()));
			}
				else if (*p == 't')
			{
					file.append(stditoa(clock() >> 5));
			}
				else if (strchr(strftimeArgs, *p))
				{
					char tmp[MAX_PATH];
					char format[] = { '%', *p, NULL };
					strftime(tmp, MAX_PATH, format, time_struct);
					file.append(tmp);
		}
			}
		}

#ifdef WIN32
		// Replace invalid file name character.
		{
			const char* invalids = "\\/:*?\"<>|";
			size_t pos = 0;
			while ((pos = file.find_first_of(invalids, pos)) != std::string::npos)
			{
				file[pos] = '-';
			}
		}
#endif

		strncpy(output, file.c_str(), MAX_PATH);
	}

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

	ImageFormat imageformat() {
		return currentimageformat;
	}

	void SetRomName(const char *filename)
	{
		std::string romPath = filename;

		RomName = Path::GetFileNameFromPath(romPath);
		RomName = Path::ScrubInvalid(RomName);
		RomDirectory = Path::GetFileDirectoryPath(romPath);
	}

	const char *GetRomName()
	{
		return RomName.c_str();
	}

	std::string GetRomNameWithoutExtension()
	{
		if (RomName.c_str() == NULL)
			return "";
		return Path::GetFileNameWithoutExt(RomName);
	}

	bool isdsgba(std::string fileName)
	{
		size_t i = fileName.find_last_of(FILE_EXT_DELIMITER_CHAR);
		
		if (i != std::string::npos) {
			fileName = fileName.substr(i - 2);
		}
		
		if(fileName == "ds.gba") {
			return true;
		}
		
		return false;
	}
};

extern PathInfo path;

