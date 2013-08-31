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
#include "../version.h"
#include "main.h"

///////////////////////////////////////////////////////////////// Console
#define BUFFER_SIZE 100
HANDLE	hConsoleOut = NULL;
HANDLE	hConsoleIn = NULL;
HWND	gConsoleWnd = NULL;
DWORD	oldmode = 0;
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

BOOL WINAPI HandlerRoutine(DWORD dwCtrlType)
{
	return ((dwCtrlType == CTRL_C_EVENT) || (dwCtrlType == CTRL_BREAK_EVENT));
}

void readConsole()
{
	INPUT_RECORD buf[10];
	DWORD num = 0;
	if (PeekConsoleInput(hConsoleIn, buf, 10, &num))
	{
		if (num)
		{
			for (u32 i = 0; i < num; i++)
			{
				if ((buf[i].EventType == KEY_EVENT) && (buf[i].Event.KeyEvent.bKeyDown) && (buf[i].Event.KeyEvent.wVirtualKeyCode == VK_PAUSE))
				{
					if (execute)
						NDS_Pause(false);
					else
						NDS_UnPause(false);
					break;
				}
			}
			FlushConsoleInputBuffer(hConsoleIn);
		}
	}
}

void OpenConsole() 
{
	//dont do anything if we're already analyzed
	if (hConsoleOut) return;

	hConsoleOut = GetStdHandle(STD_OUTPUT_HANDLE);
	DWORD fileType = GetFileType(hConsoleOut);
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
			typedef BOOL (WINAPI *_TAttachConsoleOut)(DWORD dwProcessId);
			_TAttachConsoleOut _AttachConsoleOut  = (_TAttachConsoleOut)GetProcAddress(lib,"AttachConsoleOut");
			if(_AttachConsoleOut)
			{
				if(!_AttachConsoleOut(-1)) 
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

	SetConsoleCtrlHandler(HandlerRoutine, TRUE);
	hConsoleOut = GetStdHandle(STD_OUTPUT_HANDLE);
	hConsoleIn = GetStdHandle(STD_INPUT_HANDLE);
	GetConsoleMode(hConsoleIn, &oldmode);
	SetConsoleMode(hConsoleIn, ENABLE_WINDOW_INPUT);

	gConsoleWnd = GetConsoleWindow();
	RECT pos = {0};
	if (gConsoleWnd && GetWindowRect(gConsoleWnd, &pos))
	{
		LONG x		= std::max((LONG)0, pos.left);
		LONG y		= std::max((LONG)0, pos.top);
		LONG width	= std::max((LONG)0, (pos.right - pos.left));
		LONG height	= std::max((LONG)0, (pos.bottom - pos.top));

		x		= GetPrivateProfileInt("Console", "PosX",	x,		IniName);
		y		= GetPrivateProfileInt("Console", "PosY",	y,		IniName);
		width	= GetPrivateProfileInt("Console", "Width",	width,	IniName);
		height	= GetPrivateProfileInt("Console", "Height",	height,	IniName);

		if (x < 0) x = 0;
		if (y < 0) y = 0;
		if (width < 200) width = 200;
		if (height < 100) height = 100;

		HWND desktop = GetDesktopWindow(); 
		if (desktop && GetClientRect(desktop, &pos))
		{
			if (x >= pos.right) x = 0;
			if (y >= pos.bottom) y = 0;
		}

		SetWindowPos(gConsoleWnd, NULL, x, y, width, height, SWP_NOACTIVATE);
	}

	printlog("%s\n", EMU_DESMUME_NAME_AND_VERSION());
	printlog("- compiled: %s %s\n", __DATE__, __TIME__);
	if(attached) 
		printf("\nuse cmd /c desmume.exe to get more sensible console behaviour\n");
	printlog("\n");
}

void CloseConsole()
{
	RECT pos = {0};
	SetConsoleMode(hConsoleIn, oldmode);

	if (gConsoleWnd && GetWindowRect(gConsoleWnd, &pos))
	{
		LONG width	= std::max((LONG)0, (pos.right - pos.left));
		LONG height	= std::max((LONG)0, (pos.bottom - pos.top));
		WritePrivateProfileInt("Console", "PosX",	pos.left, IniName);
		WritePrivateProfileInt("Console", "PosY",	pos.top, IniName);
		WritePrivateProfileInt("Console", "Width",	width, IniName);
		WritePrivateProfileInt("Console", "Height",	height, IniName);
	}

	FreeConsole();
	hConsoleOut = NULL;
}

void ConsoleAlwaysTop(bool top)
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
