/*  Copyright 2009-2010 DeSmuME team

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

#ifdef _MSC_VER
#define mkdir _mkdir
#endif

#if defined(_WINDOWS)
#include <winsock2.h>
#include <windows.h>
#include <direct.h>
#include "winutil.h"
#include "common.h"
#if !defined(WXPORT)
#include "resource.h"
#else
#include <glib.h>
#endif /* !WXPORT */
#elif !defined(DESMUME_COCOA)
#include <glib.h>
#endif /* _WINDOWS */

#include "time.h"
#include "utils/xstring.h"

#ifdef _WINDOWS
void FCEUD_MakePathDirs(const char *fname);
#endif

//-----------------------------------
//This is taken from mono Path.cs
static const char InvalidPathChars[] = {
	'\x22', '\x3C', '\x3E', '\x7C', '\x00', '\x01', '\x02', '\x03', '\x04', '\x05', '\x06', '\x07',
	'\x08', '\x09', '\x0A', '\x0B', '\x0C', '\x0D', '\x0E', '\x0F', '\x10', '\x11', '\x12', 
	'\x13', '\x14', '\x15', '\x16', '\x17', '\x18', '\x19', '\x1A', '\x1B', '\x1C', '\x1D', 
	'\x1E', '\x1F'
};

//but it is sort of windows-specific. Does it work in linux? Maybe we'll have to make it smarter
static const char VolumeSeparatorChar = ':';
static const char DirectorySeparatorChar = '\\';
static const char AltDirectorySeparatorChar = '/';
static bool dirEqualsVolume = (DirectorySeparatorChar == VolumeSeparatorChar);

class Path
{
public:
	static bool IsPathRooted (const std::string& path)
	{
		if (path.size() == 0)
			return false;

		
		if (path.find_first_of(InvalidPathChars) != -1)
		{
			//yuck.
			//throw new ArgumentException ("Illegal characters in path.");
		}

		char c = path [0];
		return (c == DirectorySeparatorChar 	||
			c == AltDirectorySeparatorChar 	||
			(!dirEqualsVolume && path.size() > 1 && path [1] == VolumeSeparatorChar));
	}
};

class PathInfo
{
public:

	std::string path;
	std::string RomName;

	#define MAX_FORMAT		20
	#define SECTION			"PathSettings"

	#define ROMKEY			"Roms"
	#define BATTERYKEY		"Battery"
	#define STATEKEY		"States"
	#define SCREENSHOTKEY	"Screenshots"
	#define AVIKEY			"AviFiles"
	#define CHEATKEY		"Cheats"
	#define SOUNDKEY		"SoundSamples"
	#define FIRMWAREKEY		"Firmware"
	#define FORMATKEY		"format"
	#define DEFAULTFORMATKEY "defaultFormat"
	#define NEEDSSAVINGKEY	"needsSaving"
	#define LASTVISITKEY	"lastVisit"
	#define LUAKEY			"Lua"
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

	void init(const char * filename) {

		path = std::string(filename);

		//extract the internal part of the logical rom name
		std::vector<std::string> parts = tokenize_str(filename,"|");
		SetRomName(parts[parts.size()-1].c_str());
		LoadModulePath();
#ifndef WIN32
		ReadPathSettings();
#endif
		
	}

	void LoadModulePath()
	{
#if defined(_WINDOWS)

		char *p;
		ZeroMemory(pathToModule, sizeof(pathToModule));

		GetModuleFileName(NULL, pathToModule, sizeof(pathToModule));
		p = pathToModule + lstrlen(pathToModule);
		while (p >= pathToModule && *p != '\\') p--;
		if (++p >= pathToModule) *p = 0;
#elif defined(DESMUME_COCOA)
		std::string pathStr = path;

		//Truncate the path from filename
		int x = pathStr.find_last_of("/\\");
		if (x > 0)
			pathStr = pathStr.substr(0, x);

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
#ifdef _WINDOWS
		std::string temp = (std::string)".\\" + pathToDefault;
		strncpy(pathToDefault, temp.c_str(), maxCount);
#else
		strncpy(pathToDefault, pathToModule, maxCount);
#endif
	}

	void ReadKey(char *pathToRead, const char *key)
	{
#ifdef _WINDOWS
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
#ifdef _WINDOWS
		GetPrivateProfileString(SECTION, FORMATKEY, "%f_%s_%r", screenshotFormat, MAX_FORMAT, IniName);
		savelastromvisit	= GetPrivateProfileBool(SECTION, LASTVISITKEY, true, IniName);
		currentimageformat	= (ImageFormat)GetPrivateProfileInt(SECTION, DEFAULTFORMATKEY, PNG, IniName);
#endif
	/*
		needsSaving		= GetPrivateProfileInt(SECTION, NEEDSSAVINGKEY, TRUE, IniName);
		if(needsSaving)
		{
			needsSaving = FALSE;
			WritePathSettings();
		}*/
	}

	void SwitchPath(Action action, KnownPath path, char * buffer)
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
		}

		if(action == GET)
		{
			std::string temp = pathToCopy;
			int len = (int)temp.size()-1;
#ifdef WIN32
			if(len == -1)
				temp = ".\\";
			else
				if(temp[len] != '\\') 
					temp += "\\";
#else
			if(len == -1)
				temp = "./";
			else 
				if(temp[len] != '/') 
					temp += "/";
#endif
	
			if(!Path::IsPathRooted(temp))
			{
				temp = (std::string)pathToModule + temp;
			}

			strncpy(buffer, temp.c_str(), MAX_PATH);
			#ifdef _WINDOWS
			FCEUD_MakePathDirs(buffer);
			#endif
		}
		else if(action == SET)
		{
			int len = strlen(buffer)-1;
			if(buffer[len] == '\\') 
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

	std::string extension() {

		for(int i = int(path.size()) - 1; i >= 0; --i)
		{
			if (path[i] == '.') {
				return path.substr(i+1);
			}
		}
		return path;
	}

	std::string noextension() {

		for(int i = int(path.size()) - 1; i >= 0; --i)
		{
			if (path[i] == '.') {
				return path.substr(0, i);
			}
		}
		return path;
	}

	void formatname(char *output)
	{
		std::string file;
		time_t now = time(NULL);
		tm *time_struct = localtime(&now);
		srand((unsigned int)now);

		for(int i = 0; i < MAX_FORMAT;i++) 
		{		
			char *c = &screenshotFormat[i];
			char tmp[MAX_PATH] = {0};

			if(*c == '%')
			{
				c = &screenshotFormat[++i];
				switch(*c)
				{
				case 'f':
					
					strcat(tmp, GetRomNameWithoutExtension().c_str());
					break;
				case 'D':
					strftime(tmp, MAX_PATH, "%d", time_struct);
					break;
				case 'M':
					strftime(tmp, MAX_PATH, "%m", time_struct);
					break;
				case 'Y':
					strftime(tmp, MAX_PATH, "%Y", time_struct);
					break;
				case 'h':
					strftime(tmp, MAX_PATH, "%H", time_struct);
					break;
				case 'm':
					strftime(tmp, MAX_PATH, "%M", time_struct);
					break;
				case 's':
					strftime(tmp, MAX_PATH, "%S", time_struct);
					break;
				case 'r':
					sprintf(tmp, "%d", rand() % RAND_MAX);
					break;
				}
			}
			else
			{
				int j;
				for(j=i;j<MAX_FORMAT-i;j++)
					if(screenshotFormat[j] != '%')
						tmp[j-i]=screenshotFormat[j];
					else
						break;
				tmp[j-i]='\0';
			}
			file += tmp;
		}
		strncpy(output, file.c_str(), MAX_PATH);
	}

	enum ImageFormat
	{
#if defined(_WINDOWS) && !defined(WXPORT)
		PNG = IDC_PNG,
		BMP = IDC_BMP
#else
		PNG,
		BMP
#endif
	};

	ImageFormat currentimageformat;

	ImageFormat imageformat() {
		return currentimageformat;
	}

	void SetRomName(const char *filename)
	{
		std::string str = filename;
		
		//Truncate the path from filename
		int x = str.find_last_of("/\\");
		if (x > 0)
			str = str.substr(x+1);
		RomName = str;
	}

	const char *GetRomName()
	{
		return RomName.c_str();
	}

	std::string GetRomNameWithoutExtension()
	{
		int x = RomName.find_last_of(".");
		if (x > 0)
			return RomName.substr(0,x);
		else return RomName;
	}

	bool isdsgba(std::string str) {
		int x = str.find_last_of(".");
		if (x > 0)
			str = str.substr(x-2);
		if(!strcmp(str.c_str(), "ds.gba"))
			return true;
		return false;
	}
};

extern PathInfo path;

