/*  Copyright 2008-2009 DeSmuME team

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
    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301  USA
*/

#include "../common.h"
#include <stdio.h>
#include <fcntl.h>
#include <io.h>
#include "version.h"


///////////////////////////////////////////////////////////////// Console
#define BUFFER_SIZE 100
HANDLE hConsole = NULL;
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

	//attach to an existing console (if we can; this is circuitous because AttachConsole wasnt added until XP)
	//remember to abstract this late bound function notion if we end up having to do this anywhere else
	bool attached = false;
	HMODULE lib = LoadLibrary("kernel32.dll");
	if(lib)
	{
		typedef BOOL (WINAPI *_TAttachConsole)(DWORD dwProcessId);
		_TAttachConsole _AttachConsole  = (_TAttachConsole)GetProcAddress(lib,"AttachConsole");
		if(_AttachConsole)
		{
			if(_AttachConsole(-1))
				attached = true;
		}
		FreeLibrary(lib);
	}

	//if we failed to attach, then alloc a new console
	if(!attached)
	{
		if (!AllocConsole()) return;

		SetConsoleCP(GetACP());
		SetConsoleOutputCP(GetACP());
	}

	//old console title:
	//char buf[256] = {0};
	//sprintf(buf,"CONSOLE - %s", EMU_DESMUME_NAME_AND_VERSION());
	//SetConsoleTitle(TEXT(buf));

	//newer and improved console title:
	SetConsoleTitleW(SkipEverythingButProgramInCommandLine(GetCommandLineW()).c_str());

	if(shouldRedirectStdout)
	{
		freopen("CONOUT$", "w", stdout);
		freopen("CONOUT$", "w", stderr);
		freopen("CONIN$", "r", stdin);
	}

	printlog("%s\n",EMU_DESMUME_NAME_AND_VERSION());
	printlog("- compiled: %s %s\n",__DATE__,__TIME__);
	if(attached) printf("\nuse cmd /c desmume.exe to get more sensible console behaviour\n");
	printlog("\n");
}

void CloseConsole() {
	if (hConsole == NULL) return;
	printlog("Closing...");
	FreeConsole(); 
	hConsole = NULL;
}

void printlog(const char *fmt, ...)
{
	printf(fmt);
}
