/*
	Copyright 2008-2013 DeSmuME team

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

#include "../common.h"
#include <stdio.h>
#include <fcntl.h>
#include <io.h>
#include "version.h"


///////////////////////////////////////////////////////////////// Console
#define BUFFER_SIZE 100
HANDLE	hConsole = NULL;
HWND	gConsoleWnd = NULL;
void printlog(const char *fmt, ...);

std::wstring SkipEverythingButProgramInCommandLine(wchar_t* cmdLine)
{
	//skip past the program name. can anyone think of a better way to do this?
	//we could use CommandLineToArgvW (commented out below) but then we would just have to re-assemble and potentially re-quote it
	//NOTE - this objection predates this use of the code. its not such a bad objection now...
	wchar_t* childCmdLine = cmdLine;
	wchar_t* lastSlash = cmdLine;
	wchar_t* lastGood = cmdLine;
	bool quote = false;
	for(;;)
	{
		wchar_t cur = *childCmdLine;
		if(cur == 0) break;
		childCmdLine++;
		bool thisIsQuote = (cur == '\"');
		if(cur == '\\' || cur == '/')
			lastSlash = childCmdLine;
		if(quote)
		{
			if(thisIsQuote)
				quote = false;
			else lastGood = childCmdLine;
		}
		else
		{
			if(cur == ' ' || cur == '\t')
				break;
			if(thisIsQuote)
				quote = true;
			lastGood = childCmdLine;
		}
	}
	std::wstring remainder = ((std::wstring)cmdLine).substr(childCmdLine-cmdLine);
	std::wstring path = ((std::wstring)cmdLine).substr(lastSlash-cmdLine,(lastGood-cmdLine)-(lastSlash-cmdLine));
	return path + L" " + remainder;
}

void OpenConsole() 
{
	//dont do anything if we're already analyzed
	if (hConsole) return;

	hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
	DWORD fileType = GetFileType(hConsole);
	//is FILE_TYPE_UNKNOWN (0) with no redirect
	//is FILE_TYPE_DISK (1) for redirect to file
	//is FILE_TYPE_PIPE (3) for pipe
	//i think it is FILE_TYPE_CHAR (2) for console

	//SOMETHING LIKE THIS MIGHT BE NEEDED ONE DAY
	//disable stdout buffering unless we know for sure we've been redirected to the disk
	//the runtime will be smart and set buffering when we may have in fact chosen to pipe the output to a console and dont want it buffered
	//if(GetFileType(GetStdHandle(STD_OUTPUT_HANDLE)) != FILE_TYPE_DISK) 
	//	setvbuf(stdout,NULL,_IONBF,0);

	//stdout is already connected to something. keep using it and dont let the console interfere
	bool shouldRedirectStdout = fileType == FILE_TYPE_UNKNOWN;
	bool attached = false;
	if (!AllocConsole())
	{
		HMODULE lib = LoadLibrary("kernel32.dll");
		if(lib)
		{
			typedef BOOL (WINAPI *_TAttachConsole)(DWORD dwProcessId);
			_TAttachConsole _AttachConsole  = (_TAttachConsole)GetProcAddress(lib,"AttachConsole");
			if(_AttachConsole)
			{
				if(!_AttachConsole(-1)) 
				{
					FreeLibrary(lib);
					return;
				}
				attached = true;
			}
			FreeLibrary(lib);
		}
	}
	else
	{
		SetConsoleCP(GetACP());
		SetConsoleOutputCP(GetACP());
	}

	//newer and improved console title:
	SetConsoleTitleW(SkipEverythingButProgramInCommandLine(GetCommandLineW()).c_str());
	if(shouldRedirectStdout)
	{
		freopen("CONOUT$", "w", stdout);
		freopen("CONOUT$", "w", stderr);
		freopen("CONIN$", "r", stdin);
	}

	gConsoleWnd = GetConsoleWindow();
	if (gConsoleWnd)
	{
		RECT rc = {0};
		if (GetWindowRect(gConsoleWnd, &rc))
		{
			rc.left =	GetPrivateProfileInt("Console", "PosX", rc.left, IniName);
			rc.top =	GetPrivateProfileInt("Console", "PosY", rc.top, IniName);
			rc.right =	GetPrivateProfileInt("Console", "Width", (rc.right - rc.left), IniName);
			rc.bottom =	GetPrivateProfileInt("Console", "Height", (rc.bottom - rc.top), IniName);
			SetWindowPos(gConsoleWnd, NULL, rc.left, rc.top, rc.right, rc.bottom, SWP_NOACTIVATE);
		}
	}

	printlog("%s\n",EMU_DESMUME_NAME_AND_VERSION());
	printlog("- compiled: %s %s\n",__DATE__,__TIME__);
	if(attached) printf("\nuse cmd /c desmume.exe to get more sensible console behaviour\n");
	printlog("\n");
}

void CloseConsole()
{
	RECT pos = {0};

	if (GetWindowRect(gConsoleWnd, &pos))
	{
		WritePrivateProfileInt("Console", "PosX", pos.left, IniName);
		WritePrivateProfileInt("Console", "PosY", pos.top, IniName);
		WritePrivateProfileInt("Console", "Width", (pos.right - pos.left), IniName);
		WritePrivateProfileInt("Console", "Height", (pos.bottom - pos.top), IniName);
	}
	FreeConsole();
	hConsole = NULL;
}

void ConsoleTop(bool top)
{
	if (!gConsoleWnd) return;
	SetWindowPos(gConsoleWnd, (top?HWND_TOPMOST:HWND_NOTOPMOST), 0, 0, 0, 0, SWP_NOSIZE | SWP_NOMOVE);
}

void printlog(const char *fmt, ...)
{
	va_list args;
	va_start (args, fmt);
	vprintf (fmt, args);
	va_end (args);
}
