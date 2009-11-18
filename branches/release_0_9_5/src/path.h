#include <string>

#ifdef WIN32
#include "resource.h"
#else
#include <glib.h>
#endif
#include "time.h"
#include "utils/xstring.h"

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
#ifdef WIN32
		char *p;
		ZeroMemory(pathToModule, sizeof(pathToModule));
		GetModuleFileName(NULL, pathToModule, sizeof(pathToModule));
		p = pathToModule + lstrlen(pathToModule);
		while (p >= pathToModule && *p != '\\') p--;
		if (++p >= pathToModule) *p = 0;
#else
		char *cwd = g_get_current_dir();
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
		strncpy(pathToDefault, pathToModule, maxCount);
	}

	void ReadKey(char *pathToRead, const char *key)
	{
#ifdef WIN32
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
#ifdef WIN32
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
			strncpy(buffer, pathToCopy, MAX_PATH);
			int len = strlen(buffer)-1;
#ifdef WIN32
			if(buffer[len] != '\\') 
				strcat(buffer, "\\");
#else
			if(buffer[len] != '/') 
				strcat(buffer, "/");
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
#ifdef WIN32
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
