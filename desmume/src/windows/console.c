/*  Copyright (C) 2006 yopyop
    yopyop156@ifrance.com
    yopyop156.ifrance.com

    Copyright 2008 CrazyMax (mtabachenko)

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

#include <windows.h>
#include <stdio.h>

///////////////////////////////////////////////////////////////// Console
#ifdef BETA_VERSION
#define BUFFER_SIZE 100
HANDLE hConsole;
void printlog(char *fmt, ...);

void OpenConsole() 
{
	COORD csize;
	CONSOLE_SCREEN_BUFFER_INFO csbiInfo; 
	SMALL_RECT srect;
	char buf[256];

	if (hConsole) return;
	AllocConsole();
	memset(buf,0,256);
	sprintf(buf,"DeSmuME v%s OUTPUT", VERSION);
	SetConsoleTitle(TEXT(buf));
	csize.X = 60;
	csize.Y = 800;
	SetConsoleScreenBufferSize(GetStdHandle(STD_OUTPUT_HANDLE), csize);
	GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &csbiInfo);
	srect = csbiInfo.srWindow;
	srect.Right = srect.Left + 99;
	srect.Bottom = srect.Top + 64;
	SetConsoleWindowInfo(GetStdHandle(STD_OUTPUT_HANDLE), TRUE, &srect);
	hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
	printlog("DeSmuME v%s BETA\n",VERSION);
	printlog("- compiled: %s %s\n\n",__DATE__,__TIME__);
}

void CloseConsole() {
	if (hConsole == NULL) return;
	printlog("Closing...");
	FreeConsole(); 
	hConsole = NULL;
}

void printlog(char *fmt, ...) {
	va_list list;
	char msg[512],msg2[522];
	wchar_t msg3[522];
	char *ptr;
	DWORD tmp;
	int len, s;
	int i, j;

	LPWSTR ret;

	va_start(list,fmt);
	_vsnprintf(msg,511,fmt,list);
	msg[511] = '\0';
	va_end(list);
	ptr=msg; len=strlen(msg);
	WriteConsole(hConsole,ptr, (DWORD)len, &tmp, 0);
}
#endif
