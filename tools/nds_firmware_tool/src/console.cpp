/* 	NDS Firmware tools

	Copyright 2009 DeSmuME team

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

#include "console.h"
#include "main.h"

///////////////////////////////////////////////////////////////// Console
#ifdef DEVEL_VERSION
#include <windows.h>
#include <stdio.h>
#include <fcntl.h>
#include <io.h>

#define BUFFER_SIZE 100
HANDLE hConsole;
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
	
#if 1
	FILE *fp = _fdopen( hConHandle, "w" );
#else
	FILE *fp = fopen( "c:\\desmume.log", "w" );
#endif
	*stdout = *fp;
	//and stderr
	*stderr = *fp;

	memset(buf,0,256);
	sprintf(buf,"%s OUTPUT", _TITLE);
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
	if(attached) printf("\n");
	printf("%s\n",_TITLE);
	printf("- compiled: %s %s\n\n",__DATE__,__TIME__);
}

void CloseConsole()
{
	if (hConsole == NULL) return;
	INFO("Closing...");
	FreeConsole(); 
	hConsole = NULL;
}
#else
void OpenConsole() {}
void CloseConsole() {}
#endif
