/*  Copyright (C) 2006 yopyop
    yopyop156@ifrance.com
    yopyop156.ifrance.com
    Copyright 2008 CrazyMax (mtabachenko)
	Copyright (C) 2009 DeSmuME team

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

#include "../common.h"
#include <stdio.h>
#include <fcntl.h>
#include <io.h>
#include "version.h"


///////////////////////////////////////////////////////////////// Console
#if !defined(PUBLIC_RELEASE) || defined(DEVELOPER)
#define BUFFER_SIZE 100
HANDLE hConsole = NULL;
void printlog(const char *fmt, ...);

void OpenConsole() 
{
	COORD csize;
	CONSOLE_SCREEN_BUFFER_INFO csbiInfo; 
	SMALL_RECT srect;
	char buf[256];

	//dont do anything if we're already attached
	if (hConsole) return;

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
		AllocConsole();
	}

	hConsole = GetStdHandle(STD_OUTPUT_HANDLE);

	//redirect stdio
	long lStdHandle = (long)hConsole;
	int hConHandle = _open_osfhandle(lStdHandle, _O_TEXT);
	if(hConHandle == -1)
		return; //this fails from a visual studio command prompt
	
	FILE *fp = _fdopen( hConHandle, "w" );
	*stdout = *fp;
	//and stderr
	*stderr = *fp;

	memset(buf,0,256);
	sprintf(buf,"%s OUTPUT", DESMUME_NAME_AND_VERSION);
	SetConsoleTitle(TEXT(buf));
	csize.X = 60;
	csize.Y = 800;
	SetConsoleScreenBufferSize(GetStdHandle(STD_OUTPUT_HANDLE), csize);
	GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &csbiInfo);
	srect = csbiInfo.srWindow;
	srect.Right = srect.Left + 99;
	srect.Bottom = srect.Top + 64;
	SetConsoleWindowInfo(GetStdHandle(STD_OUTPUT_HANDLE), TRUE, &srect);
	SetConsoleCP(GetACP());
	SetConsoleOutputCP(GetACP());
	if(attached) printlog("\n");
	printlog("%s\n",DESMUME_NAME_AND_VERSION);
	printlog("- compiled: %s %s\n\n",__DATE__,__TIME__);


}

void CloseConsole() {
	if (hConsole == NULL) return;
	printlog("Closing...");
	FreeConsole(); 
	hConsole = NULL;
}

void printlog(const char *fmt, ...)
{
	va_list list;
	char msg[512];
	DWORD tmp;

	memset(msg,0,512);

	va_start(list,fmt);
		_vsnprintf(msg,511,fmt,list);
	va_end(list);
	WriteConsole(hConsole,msg, (DWORD)strlen(msg), &tmp, 0);
}
#else

void OpenConsole() {}
void CloseConsole() {}

#endif
