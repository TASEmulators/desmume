/*
	Copyright (C) 2098-2016 DeSmuME team

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

#include <assert.h>
#include <stdio.h>
#include <process.h>
#include <vector>
#include <map>
#include <string>
#include <algorithm>
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <winver.h>
#include <commdlg.h>
#include <shellapi.h>
#include "driver.h"
#include "lua-engine.h"

#include "resource.h"
#include "main.h"
#include "OpenArchive.h"

#define MAX_RECENT_SCRIPTS 15
char Recent_Scripts[MAX_RECENT_SCRIPTS][1024] = {0};

static char Str_Tmp [1024];

struct ControlLayoutInfo
{
	int controlID;
	
	enum LayoutType // what to do when the containing window resizes
	{
		NONE, // leave the control where it was
		RESIZE_END, // resize the control
		MOVE_START, // move the control
	};
	LayoutType horizontalLayout;
	LayoutType verticalLayout;
};
struct ControlLayoutState
{
	int x,y,width,height;
	bool valid;
	ControlLayoutState() : valid(false) {}
};

static ControlLayoutInfo controlLayoutInfos [] = {
	{IDC_LUACONSOLE, ControlLayoutInfo::RESIZE_END, ControlLayoutInfo::RESIZE_END},
	{IDC_EDIT_LUAPATH, ControlLayoutInfo::RESIZE_END, ControlLayoutInfo::NONE},
	{IDC_BUTTON_LUARUN, ControlLayoutInfo::MOVE_START, ControlLayoutInfo::NONE},
	{IDC_BUTTON_LUASTOP, ControlLayoutInfo::MOVE_START, ControlLayoutInfo::NONE},
};
static const int numControlLayoutInfos = sizeof(controlLayoutInfos)/sizeof(*controlLayoutInfos);


extern std::vector<HWND> LuaScriptHWnds;
struct LuaPerWindowInfo {
	std::string filename;
	HANDLE fileWatcherThread;
	bool started;
	bool closeOnStop;
	bool subservient;
	int width; int height;
	ControlLayoutState layoutState [numControlLayoutInfos];
	LuaPerWindowInfo() : fileWatcherThread(NULL), started(false), closeOnStop(false), subservient(false), width(405), height(244) {}
};
std::map<HWND, LuaPerWindowInfo> LuaWindowInfo;
static char Lua_Dir[1024]="";

int WINAPI FileSysWatcher (LPVOID arg)
{
	HWND hDlg = (HWND)arg;
	LuaPerWindowInfo& info = LuaWindowInfo[hDlg];

	while(true)
	{
		char filename [1024], directory [1024];

		strncpy(filename, info.filename.c_str(), 1024);
		filename[1023] = 0;
		strcpy(directory, filename);
		char* slash = strrchr(directory, '/');
		slash = std::max(slash, strrchr(directory, '\\'));
		if(slash)
			*slash = 0;

		char* bar = strchr(filename, '|');
		if(bar) *bar = '\0';

		WIN32_FILE_ATTRIBUTE_DATA origData;
		GetFileAttributesEx (filename,  GetFileExInfoStandard,  (LPVOID)&origData);

		HANDLE hNotify = FindFirstChangeNotification(directory, FALSE, FILE_NOTIFY_CHANGE_LAST_WRITE);

		if(hNotify)
		{
			DWORD dwWaitResult = WaitForSingleObject(hNotify, 500);

			if(dwWaitResult != STATUS_TIMEOUT)
			{
				if(dwWaitResult == WAIT_ABANDONED)
					return dwWaitResult;

				WIN32_FILE_ATTRIBUTE_DATA data;
				GetFileAttributesEx (filename,  GetFileExInfoStandard,  (LPVOID)&data);

				// at this point it could be any file in the directory that changed
				// so check to make sure it was the file we care about
				if(memcmp(&origData.ftLastWriteTime, &data.ftLastWriteTime, sizeof(FILETIME)))
				{
					RequestAbortLuaScript((intptr_t)hDlg, "terminated to reload the script");
					PostMessage(hDlg, WM_COMMAND, IDC_BUTTON_LUARUN, 0);
				}
			}

			//FindNextChangeNotification(hNotify); // let's not try to reuse it...
			FindCloseChangeNotification(hNotify); // but let's at least make sure to release it!
		}
		else
		{
			Sleep(500);
		}
	}

	return 0;
}

void RegisterWatcherThread (HWND hDlg)
{
	HANDLE thread = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE) FileSysWatcher, (LPVOID) hDlg, CREATE_SUSPENDED, NULL);
//	SetThreadPriority(thread, THREAD_PRIORITY_LOWEST); // disabled because it can make us miss updates in single-core mode and the thread spends all its time sleeping anyway

	LuaPerWindowInfo& info = LuaWindowInfo[hDlg];
	info.fileWatcherThread = thread;

	ResumeThread(thread);
}
void KillWatcherThread (HWND hDlg)
{
	LuaPerWindowInfo& info = LuaWindowInfo[hDlg];
	TerminateThread(info.fileWatcherThread, 0);
	info.fileWatcherThread = NULL;
}


// some extensions that might commonly be near lua files that almost certainly aren't lua files.
static const char* s_nonLuaExtensions [] = {"txt", "nfo", "htm", "html", "jpg", "jpeg", "png", "bmp", "gif", "mp3", "wav", "lnk", "exe", "bat", "luasav", "sav", "srm", "brm", "cfg", "wch", "ds*",  "nds","bin","raw"};


void Update_Recent_Script(const char* Path, bool dontPutAtTop)
{
	char LogicalName[1024], PhysicalName[1024];
	bool exists = ObtainFile(Path, LogicalName, PhysicalName, "luacheck", s_nonLuaExtensions, sizeof(s_nonLuaExtensions)/sizeof(*s_nonLuaExtensions));
	ReleaseTempFileCategory("luacheck"); // delete the temporary (physical) file if any

	if(!exists)
		return;

	int i;

	for(i = 0; i < MAX_RECENT_SCRIPTS; i++)
	{
		if (!(strcmp(Recent_Scripts[i], Path)))
		{
			// move recent item to the top of the list
			if(i == 0 || dontPutAtTop)
				return;
			char temp [1024];
			strcpy(temp, Recent_Scripts[i]);
			int j;
			for(j = i; j > 0; j--)
				strcpy(Recent_Scripts[j], Recent_Scripts[j-1]);
			strcpy(Recent_Scripts[0], temp);
//			MustUpdateMenu = 1;
			return;
		}
	}
	
	if(!dontPutAtTop)
	{
		// add to start of recent list
		for(i = MAX_RECENT_SCRIPTS-1; i > 0; i--)
			strcpy(Recent_Scripts[i], Recent_Scripts[i - 1]);

		strcpy(Recent_Scripts[0], Path);
	}
	else
	{
		// add to end of recent list
		for(i = 0; i < MAX_RECENT_SCRIPTS; i++)
		{
			if(!*Recent_Scripts[i])
			{
				strcpy(Recent_Scripts[i], Path);
				break;
			}
		}
	}

//	MustUpdateMenu = 1;
}

HWND IsScriptFileOpen(const char* Path)
{
	for(std::map<HWND, LuaPerWindowInfo>::iterator iter = LuaWindowInfo.begin(); iter != LuaWindowInfo.end(); ++iter)
	{
		LuaPerWindowInfo& info = iter->second;
		const char* filename = info.filename.c_str();
		const char* pathPtr = Path;

		// case-insensitive slash-direction-insensitive compare
		bool same = true;
		while(*filename || *pathPtr)
		{
			if((*filename == '/' || *filename == '\\') && (*pathPtr == '/' || *pathPtr == '\\'))
			{
				do {filename++;} while(*filename == '/' || *filename == '\\');
				do {pathPtr++;} while(*pathPtr == '/' || *pathPtr == '\\');
			}
			else if(tolower(*filename) != tolower(*pathPtr))
			{
				same = false;
				break;
			}
			else
			{
				filename++;
				pathPtr++;
			}
		}

		if(same)
			return iter->first;
	}
	return NULL;
}


void PrintToWindowConsole(intptr_t hDlgAsInt, const char* str)
{
	HWND hDlg = (HWND)hDlgAsInt;
	HWND hConsole = GetDlgItem(hDlg, IDC_LUACONSOLE);

	if (IsDlgButtonChecked(hDlg, IDC_USE_STDOUT) == BST_CHECKED)
	{
		printf(str);
		return;
	}

	int length = GetWindowTextLength(hConsole);
	if(length >= 250000)
	{
		// discard first half of text if it's getting too long
		SendMessage(hConsole, EM_SETSEL, 0, length/2);
		SendMessage(hConsole, EM_REPLACESEL, false, (LPARAM)"");
		length = GetWindowTextLength(hConsole);
	}
	SendMessage(hConsole, EM_SETSEL, length, length);

	LuaPerWindowInfo& info = LuaWindowInfo[hDlg];

	{
		SendMessage(hConsole, EM_REPLACESEL, false, (LPARAM)str);
	}
}

extern int Show_Genesis_Screen(HWND hWnd);
void OnStart(intptr_t hDlgAsInt)
{
	HWND hDlg = (HWND)hDlgAsInt;
	LuaPerWindowInfo& info = LuaWindowInfo[hDlg];
	info.started = true;
	EnableWindow(GetDlgItem(hDlg, IDC_BUTTON_LUABROWSE), false); // disable browse while running because it misbehaves if clicked in a frameadvance loop
	EnableWindow(GetDlgItem(hDlg, IDC_BUTTON_LUASTOP), true);
	SetWindowText(GetDlgItem(hDlg, IDC_BUTTON_LUARUN), "Restart");
	SetWindowText(GetDlgItem(hDlg, IDC_LUACONSOLE), ""); // clear the console
//	Show_Genesis_Screen(HWnd); // otherwise we might never show the first thing the script draws
}

void OnStop(intptr_t hDlgAsInt, bool statusOK)
{
	HWND hDlg = (HWND)hDlgAsInt;
	LuaPerWindowInfo& info = LuaWindowInfo[hDlg];

	HWND prevWindow = GetActiveWindow();
	SetActiveWindow(hDlg); // bring to front among other script/secondary windows, since a stopped script will have some message for the user that would be easier to miss otherwise
	if(prevWindow == MainWindow->getHWnd()) SetActiveWindow(prevWindow);

	PrintToWindowConsole(hDlgAsInt, "script stopped.\r\n");

	info.started = false;
	EnableWindow(GetDlgItem(hDlg, IDC_BUTTON_LUABROWSE), true);
	EnableWindow(GetDlgItem(hDlg, IDC_BUTTON_LUASTOP), false);
	SetWindowText(GetDlgItem(hDlg, IDC_BUTTON_LUARUN), "Run");
//	if(statusOK)
//		Show_Genesis_Screen(MainWindow->getHWnd()); // otherwise we might never show the last thing the script draws
	if(info.closeOnStop)
		PostMessage(hDlg, WM_CLOSE, 0, 0);
}

const char* MakeScriptPathAbsolute(const char* filename, const char* extraDirToCheck);

void UpdateFileEntered(HWND hDlg)
{
	char local_str_tmp [1024];
	SendDlgItemMessage(hDlg,IDC_EDIT_LUAPATH,WM_GETTEXT,(WPARAM)512,(LPARAM)local_str_tmp);

	// if it exists, make sure we're using an absolute path to it
	const char* filename = local_str_tmp;
	FILE* file = fopen(filename, "rb");
	if(file)
	{
		fclose(file);
		filename = MakeScriptPathAbsolute(local_str_tmp, NULL);
		if(filename != local_str_tmp && stricmp(filename, local_str_tmp))
		{
			SendDlgItemMessage(hDlg,IDC_EDIT_LUAPATH,WM_SETTEXT,(WPARAM)512,(LPARAM)filename);
			SendDlgItemMessage(hDlg,IDC_EDIT_LUAPATH,EM_SETSEL,0,-1);
			SendDlgItemMessage(hDlg,IDC_EDIT_LUAPATH,EM_SETSEL,-1,-1);
			return;
		}
	}

	// use ObtainFile to support opening files within archives
	char LogicalName[1024], PhysicalName[1024];
	bool exists = ObtainFile(filename, LogicalName, PhysicalName, "luacheck", s_nonLuaExtensions, sizeof(s_nonLuaExtensions)/sizeof(*s_nonLuaExtensions));
	bool readonly = exists ? ((GetFileAttributes(PhysicalName) & FILE_ATTRIBUTE_READONLY) != 0) : (strchr(LogicalName, '|') != NULL || strchr(filename, '|') != NULL);
	ReleaseTempFileCategory("luacheck"); // delete the temporary (physical) file if any

	if(exists)
	{
		LuaPerWindowInfo& info = LuaWindowInfo[hDlg];
		info.filename = LogicalName;

		char* slash = strrchr(LogicalName, '/');
		slash = std::max(slash, strrchr(LogicalName, '\\'));
		if(slash)
			slash++;
		else
			slash = LogicalName;
		SetWindowText(hDlg, slash);
//		Build_Main_Menu();

		PostMessage(hDlg, WM_COMMAND, IDC_BUTTON_LUARUN, 0);
	}

	const char* ext = strrchr(LogicalName, '.');
	bool isLuaFile = ext && !_stricmp(ext, ".lua");
	if(exists)
	{
		SetWindowText(GetDlgItem(hDlg, IDC_BUTTON_LUAEDIT), isLuaFile ? (readonly ? "View" : "Edit") : "Open");
		EnableWindow(GetDlgItem(hDlg, IDC_BUTTON_LUAEDIT), true);
	}
	else
	{
		SetWindowText(GetDlgItem(hDlg, IDC_BUTTON_LUAEDIT), "Create");
		EnableWindow(GetDlgItem(hDlg, IDC_BUTTON_LUAEDIT), isLuaFile && !readonly);
	}
}

//extern "C" int Clear_Sound_Buffer(void);

static int Change_File_L(char *Dest, char *Dir, char *Titre, char *Filter, char *Ext, HWND hwnd)
{
	OPENFILENAME ofn;

//	SetCurrentDirectory(Desmume_Path);

	if (!strcmp(Dest, ""))
	{
		strcpy(Dest, "default.");
		strcat(Dest, Ext);
	}

	memset(&ofn, 0, sizeof(OPENFILENAME));

	ofn.lStructSize = sizeof(OPENFILENAME);
	ofn.hwndOwner = hwnd;
	ofn.hInstance = hAppInst;
	ofn.lpstrFile = Dest;
	ofn.nMaxFile = 2047;
	ofn.lpstrFilter = Filter;
	ofn.nFilterIndex = 1;
	ofn.lpstrInitialDir = Dir;
	ofn.lpstrTitle = Titre;
	ofn.lpstrDefExt = Ext;
	ofn.Flags = OFN_FILEMUSTEXIST | OFN_HIDEREADONLY;

	if (GetOpenFileName(&ofn)) return 1;

	return 0;
}

// "About" support, modelled on vba-rerecording's LuaOpenDialog.cpp.
// version.dll is not linked by this project, so its entry points are resolved
// at runtime instead of changing the MSVC/MinGW build files.  A stock Lua 5.1
// DLL carries a FileVersion resource ("5.1.4" for the old shipped build),
// while LuaJIT has none, so the line is simply omitted when there is nothing
// to report.
typedef DWORD (WINAPI *LuaGetFileVersionInfoSizeA)(LPCSTR, LPDWORD);
typedef BOOL  (WINAPI *LuaGetFileVersionInfoA)(LPCSTR, DWORD, DWORD, LPVOID);
typedef BOOL  (WINAPI *LuaVerQueryValueA)(LPCVOID, LPCSTR, LPVOID*, UINT*);

static void GetLuaDllFileVersion(const char* path, char* out, size_t outSize)
{
	out[0] = '\0';
	if(!path[0])
		return;

	HMODULE ver = LoadLibraryA("version.dll");
	if(!ver)
		return;

	LuaGetFileVersionInfoSizeA pSize = (LuaGetFileVersionInfoSizeA)GetProcAddress(ver, "GetFileVersionInfoSizeA");
	LuaGetFileVersionInfoA     pInfo = (LuaGetFileVersionInfoA)GetProcAddress(ver, "GetFileVersionInfoA");
	LuaVerQueryValueA          pQuery = (LuaVerQueryValueA)GetProcAddress(ver, "VerQueryValueA");

	if(pSize && pInfo && pQuery)
	{
		DWORD ignored = 0;
		DWORD size = pSize(path, &ignored);
		if(size)
		{
			std::vector<BYTE> data(size);
			if(pInfo(path, 0, size, &data[0]))
			{
				WORD* translation = NULL;
				UINT translationLen = 0;
				if(pQuery(&data[0], "\\VarFileInfo\\Translation", (LPVOID*)&translation, &translationLen)
					&& translationLen >= 2 * sizeof(WORD))
				{
					char key[64];
					snprintf(key, sizeof(key), "\\StringFileInfo\\%04x%04x\\FileVersion",
						translation[0], translation[1]);
					char* value = NULL;
					UINT valueLen = 0;
					if(pQuery(&data[0], key, (LPVOID*)&value, &valueLen) && value && valueLen > 1)
					{
						strncpy(out, value, outSize - 1);
						out[outSize - 1] = '\0';
					}
				}

				// The 2008 stock Lua 5.1 DLL has no VarFileInfo translation
				// table, so the string lookup above cannot work.  Its numeric
				// VS_FIXEDFILEINFO still says 5.1.4.0; vba-rerecording falls
				// back the same way.
				if(out[0] == '\0')
				{
					VS_FIXEDFILEINFO* fixed = NULL;
					UINT fixedLen = 0;
					if(pQuery(&data[0], "\\", (LPVOID*)&fixed, &fixedLen)
						&& fixed && fixedLen >= sizeof(VS_FIXEDFILEINFO)
						&& fixed->dwSignature == VS_FFI_SIGNATURE)
					{
						unsigned major    = HIWORD(fixed->dwFileVersionMS);
						unsigned minor    = LOWORD(fixed->dwFileVersionMS);
						unsigned build    = HIWORD(fixed->dwFileVersionLS);
						unsigned revision = LOWORD(fixed->dwFileVersionLS);
						if(major || minor || build)
						{
							if(revision)
								snprintf(out, outSize, "%u.%u.%u.%u", major, minor, build, revision);
							else
								snprintf(out, outSize, "%u.%u.%u", major, minor, build);
						}
					}
				}
			}
		}
	}

	FreeLibrary(ver);
}

// Shows which Lua engine is actually in use.  This used to be the About button
// of the script window; the same information now hangs off the Tools > Lua
// Scripting submenu, so it has to work with no script window open.
void LuaConsole_ShowEngineInfo(HWND hWndOwner)
{
	// MSVC builds delay-load lua51.dll.  Asking Lua anything before the DLL is
	// present would raise a delay-load exception, so use the same guard (and
	// the same error message) as Run/Stop.
	if(!DemandLua())
		return;

	LuaEngineInfo info;
	if(!GetLuaEngineInfo(&info))
	{
		MessageBoxA(hWndOwner, "The Lua engine could not be started.",
			"About Lua Scripting", MB_OK | MB_ICONERROR);
		return;
	}

	// luaL_newstate() above loaded the DLL through the delay-load import
	// table.  Keep the extra reference: unloading it would leave already
	// resolved imports pointing at a freed module.
	HMODULE mod = GetModuleHandleA("lua51.dll");
	if(!mod)
		mod = LoadLibraryA("lua51.dll");

	char dllPath[MAX_PATH] = "";
	if(mod)
		GetModuleFileNameA(mod, dllPath, MAX_PATH);

	char fileVersion[128] = "";
	GetLuaDllFileVersion(dllPath, fileVersion, sizeof(fileVersion));

	char text[2048];
	int n = snprintf(text, sizeof(text),
		"DeSmuME Lua Scripting\r\n\r\n"
		"Engine     : %s\r\n"
		"Platform   : %s\r\n"
		"Trace JIT  : %s\r\n"
		"Headers    : %s\r\n"
		"Lua DLL    : %s\r\n",
		info.version,
		info.platform[0] ? info.platform : "n/a (not LuaJIT)",
		info.jit,
		info.headers,
		dllPath[0] ? dllPath : "(not loaded as a DLL)");
	if(fileVersion[0] && n > 0 && (size_t)n < sizeof(text))
		snprintf(text + n, sizeof(text) - n, "File ver   : %s\r\n", fileVersion);

	MessageBoxA(hWndOwner, text, "About Lua Scripting", MB_OK | MB_ICONINFORMATION);
}

LRESULT CALLBACK LuaScriptProc(HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	RECT r;
	RECT r2;
	int dx1, dy1, dx2, dy2;

	switch(uMsg)
	{
		case WM_INITDIALOG: {
			if(std::find(LuaScriptHWnds.begin(), LuaScriptHWnds.end(), hDlg) == LuaScriptHWnds.end())
			{
				LuaScriptHWnds.push_back(hDlg);
//				Build_Main_Menu();
			}
//			if (Full_Screen)
//			{
//				while (ShowCursor(false) >= 0);
//				while (ShowCursor(true) < 0);
//			}

//			HANDLE hIcon = LoadIcon(GetModuleHandle(NULL), MAKEINTRESOURCE(IDI_LUA));
//			SendMessage(hDlg, WM_SETICON, ICON_BIG, (LPARAM)hIcon);

			// remove the 30000 character limit from the console control
			SendMessage(GetDlgItem(hDlg, IDC_LUACONSOLE),EM_LIMITTEXT,0,0);

			GetWindowRect(MainWindow->getHWnd(), &r);
			dx1 = (r.right - r.left) / 2;
			dy1 = (r.bottom - r.top) / 2;

			GetWindowRect(hDlg, &r2);
			dx2 = (r2.right - r2.left) / 2;
			dy2 = (r2.bottom - r2.top) / 2;

			int windowIndex = std::find(LuaScriptHWnds.begin(), LuaScriptHWnds.end(), hDlg) - LuaScriptHWnds.begin();
			int staggerOffset = windowIndex * 24;
			r.left += staggerOffset;
			r.right += staggerOffset;
			r.top += staggerOffset;
			r.bottom += staggerOffset;

			// push it away from the main window if we can
			const int width = (r.right-r.left); 
			const int width2 = (r2.right-r2.left); 
			if(r.left+width2 + width < GetSystemMetrics(SM_CXSCREEN))
			{
				r.right += width;
				r.left += width;
			}
			else if((int)r.left - (int)width2 > 0)
			{
				r.right -= width2;
				r.left -= width2;
			}

			SetWindowPos(hDlg, NULL, r.left, r.top, NULL, NULL, SWP_NOSIZE | SWP_NOZORDER | SWP_SHOWWINDOW);

			LuaPerWindowInfo info;
			{
				RECT r3;
				GetClientRect(hDlg, &r3);
				info.width = r3.right - r3.left;
				info.height = r3.bottom - r3.top;
			}
			LuaWindowInfo[hDlg] = info;
			RegisterWatcherThread(hDlg);

			OpenLuaContext((intptr_t)hDlg, PrintToWindowConsole, OnStart, OnStop);

			DragAcceptFiles(hDlg, TRUE);

			return true;
		}	break;

		case WM_MENUSELECT:
 		case WM_ENTERSIZEMOVE:
//			Clear_Sound_Buffer();
			break;

		case WM_SIZING:
			{
				// enforce a minimum window size

				LPRECT r = (LPRECT) lParam;
				int minimumWidth = 333;
				int minimumHeight = 117;
				if(r->right - r->left < minimumWidth)
					if(wParam == WMSZ_LEFT || wParam == WMSZ_TOPLEFT || wParam == WMSZ_BOTTOMLEFT)
						r->left = r->right - minimumWidth;
					else
						r->right = r->left + minimumWidth;
				if(r->bottom - r->top < minimumHeight)
					if(wParam == WMSZ_TOP || wParam == WMSZ_TOPLEFT || wParam == WMSZ_TOPRIGHT)
						r->top = r->bottom - minimumHeight;
					else
						r->bottom = r->top + minimumHeight;
			}
			return TRUE;

		case WM_SIZE:
			{
				// resize or move controls in the window as necessary when the window is resized

				LuaPerWindowInfo& windowInfo = LuaWindowInfo[hDlg];
				int prevDlgWidth = windowInfo.width;
				int prevDlgHeight = windowInfo.height;

				int dlgWidth = LOWORD(lParam);
				int dlgHeight = HIWORD(lParam);

				int deltaWidth = dlgWidth - prevDlgWidth;
				int deltaHeight = dlgHeight - prevDlgHeight;

				for(int i = 0; i < numControlLayoutInfos; i++)
				{
					ControlLayoutInfo layoutInfo = controlLayoutInfos[i];
					ControlLayoutState& layoutState = windowInfo.layoutState[i];

					HWND hCtrl = GetDlgItem(hDlg,layoutInfo.controlID);

					int x,y,width,height;
					if(layoutState.valid)
					{
						x = layoutState.x;
						y = layoutState.y;
						width = layoutState.width;
						height = layoutState.height;
					}
					else
					{
						RECT r;
						GetWindowRect(hCtrl, &r);
						POINT p = {r.left, r.top};
						ScreenToClient(hDlg, &p);
						x = p.x;
						y = p.y;
						width = r.right - r.left;
						height = r.bottom - r.top;
					}

					switch(layoutInfo.horizontalLayout)
					{
						case ControlLayoutInfo::RESIZE_END: width += deltaWidth; break;
						case ControlLayoutInfo::MOVE_START: x += deltaWidth; break;
						default: break;
					}
					switch(layoutInfo.verticalLayout)
					{
						case ControlLayoutInfo::RESIZE_END: height += deltaHeight; break;
						case ControlLayoutInfo::MOVE_START: y += deltaHeight; break;
						default: break;
					}

					SetWindowPos(hCtrl, 0, x,y, width,height, 0);

					layoutState.x = x;
					layoutState.y = y;
					layoutState.width = width;
					layoutState.height = height;
					layoutState.valid = true;
				}

				windowInfo.width = dlgWidth;
				windowInfo.height = dlgHeight;

				RedrawWindow(hDlg, NULL, NULL, RDW_INVALIDATE);
			}
			break;

		case WM_COMMAND:
		{
			switch(LOWORD(wParam))
			{
				case IDC_BUTTON_LUABROWSE:
				{
					LuaPerWindowInfo& info = LuaWindowInfo[hDlg];
					char Str_Tmp [1024]; // shadow added because the global one is unreliable
					strcpy(Str_Tmp,info.filename.c_str());
					SendDlgItemMessage(hDlg,IDC_EDIT_LUAPATH,WM_GETTEXT,(WPARAM)512,(LPARAM)Str_Tmp);
					char* bar = strchr(Str_Tmp, '|');
					if(bar) *bar = '\0';
//					DialogsOpen++;
//					Clear_Sound_Buffer();
					if(Change_File_L(Str_Tmp, Lua_Dir, "Load Lua Script", "Lua Script\0*.lua*\0All Files\0*.*\0\0", "lua", hDlg))
					{
						SendDlgItemMessage(hDlg,IDC_EDIT_LUAPATH,WM_SETTEXT,0,(LPARAM)Str_Tmp);
					}
//					DialogsOpen--;

				}	break;
				case IDC_BUTTON_LUAEDIT:
				{
					LuaPerWindowInfo& info = LuaWindowInfo[hDlg];
					char Str_Tmp [1024]; // shadow added because the global one is unreliable
					strcpy(Str_Tmp,info.filename.c_str());
					SendDlgItemMessage(hDlg,IDC_EDIT_LUAPATH,WM_GETTEXT,(WPARAM)512,(LPARAM)Str_Tmp);
					char LogicalName[1024], PhysicalName[1024];
					bool exists = ObtainFile(Str_Tmp, LogicalName, PhysicalName, "luaview", s_nonLuaExtensions, sizeof(s_nonLuaExtensions)/sizeof(*s_nonLuaExtensions));
					bool created = false;
					if(!exists)
					{
						FILE* file = fopen(Str_Tmp, "r");
						if(!file)
						{
							file = fopen(Str_Tmp, "w");
							if(file)
							{
								created = true;
								exists = true;
								strcpy(PhysicalName, Str_Tmp);
							}
						}
						if(file)
							fclose(file);
					}
					if(exists)
					{
						// tell the OS to open the file with its associated editor,
						// without blocking on it or leaving a command window open.
						if((INT_PTR)ShellExecute(NULL, "edit", PhysicalName, NULL, NULL, SW_SHOWNORMAL) == SE_ERR_NOASSOC)
							if((INT_PTR)ShellExecute(NULL, "open", PhysicalName, NULL, NULL, SW_SHOWNORMAL) == SE_ERR_NOASSOC)
								ShellExecute(NULL, NULL, "notepad", PhysicalName, NULL, SW_SHOWNORMAL);
					}
					if(created)
					{
						UpdateFileEntered(hDlg);
					}
				}	break;
				case IDC_EDIT_LUAPATH:
				{
					switch(HIWORD(wParam))
					{
						case EN_CHANGE:
						{
							UpdateFileEntered(hDlg);
						}	break;
					}
				}	break;
				case IDC_BUTTON_LUARUN:
				{
					HWND focus = GetFocus();
					HWND textbox = GetDlgItem(hDlg, IDC_EDIT_LUAPATH);
					if(focus != textbox)
						SetActiveWindow(MainWindow->getHWnd());

					LuaPerWindowInfo& info = LuaWindowInfo[hDlg];
					strcpy(Str_Tmp,info.filename.c_str());
					char LogicalName[1024], PhysicalName[1024];
					bool exists = ObtainFile(Str_Tmp, LogicalName, PhysicalName, "luarun", s_nonLuaExtensions, sizeof(s_nonLuaExtensions)/sizeof(*s_nonLuaExtensions));
					Update_Recent_Script(LogicalName, info.subservient);
					if(DemandLua())
						RunLuaScriptFile((intptr_t)hDlg, PhysicalName);
				}	break;
				case IDC_BUTTON_LUASTOP:
				{
					PrintToWindowConsole((intptr_t)hDlg, "user clicked stop button\r\n");
					SetActiveWindow(MainWindow->getHWnd());
					if(DemandLua()) StopLuaScript((intptr_t)hDlg);
				}	break;
				case IDC_NOTIFY_SUBSERVIENT:
				{
					LuaPerWindowInfo& info = LuaWindowInfo[hDlg];
					info.subservient = lParam ? true : false;
				}	break;
				//case IDOK:
				case IDCANCEL:
				{	LuaPerWindowInfo& info = LuaWindowInfo[hDlg];
					if(info.filename.empty())
					{
//						if (Full_Screen)
//						{
//							while (ShowCursor(true) < 0);
//							while (ShowCursor(false) >= 0);
//						}
//						DialogsOpen--;
						DragAcceptFiles(hDlg, FALSE);
						KillWatcherThread(hDlg);
						LuaScriptHWnds.erase(remove(LuaScriptHWnds.begin(), LuaScriptHWnds.end(), hDlg), LuaScriptHWnds.end());
						LuaWindowInfo.erase(hDlg);
						CloseLuaContext((intptr_t)hDlg);
//						Build_Main_Menu();
						EndDialog(hDlg, true);
					}
				}	return true;
			}

			return false;
		}	break;

		case WM_CLOSE:
		{
			LuaPerWindowInfo& info = LuaWindowInfo[hDlg];

			PrintToWindowConsole((intptr_t)hDlg, "user closed script window\r\n");
			StopLuaScript((intptr_t)hDlg);
			if(info.started)
			{
				// not stopped yet, wait to close until we are, otherwise we'll crash
				info.closeOnStop = true;
				return false;
			}

//			if (Full_Screen)
//			{
//				while (ShowCursor(true) < 0);
//				while (ShowCursor(false) >= 0);
//			}
//			DialogsOpen--;
			DragAcceptFiles(hDlg, FALSE);
			KillWatcherThread(hDlg);
			LuaScriptHWnds.erase(remove(LuaScriptHWnds.begin(), LuaScriptHWnds.end(), hDlg), LuaScriptHWnds.end());
			LuaWindowInfo.erase(hDlg);
			CloseLuaContext((intptr_t)hDlg);
//			Build_Main_Menu();
			EndDialog(hDlg, true);
		}	return true;

		case WM_DROPFILES:
		{
			HDROP hDrop = (HDROP)wParam;
			DragQueryFile(hDrop, 0, Str_Tmp, 1024);
			DragFinish(hDrop);
			SendDlgItemMessage(hDlg,IDC_EDIT_LUAPATH,WM_SETTEXT,0,(LPARAM)Str_Tmp );
			UpdateFileEntered(hDlg);
		}	return true;

	}

	return false;
}

