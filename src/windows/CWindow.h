/*  Copyright (C) 2006 yopyop
    yopyop156@ifrance.com
    yopyop156.ifrance.com

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

#ifndef CWINDOW_H
#define CWINDOW_H

#include "../common.h"

class WINCLASS
{
private:
	HWND		hwnd;
	HMENU		hmenu;
	HINSTANCE	hInstance;
	char		regclass[256];
	int minWidth, minHeight;
public:
	WINCLASS(LPSTR rclass, HINSTANCE hInst);
	~WINCLASS();

	bool create(LPSTR caption, int x, int y, int width, int height, int style, 
					HMENU menu);
	bool createEx(LPSTR caption, int x, int y, int width, int height, int style, int styleEx,
					HMENU menu);

	bool setMenu(HMENU menu);
	DWORD checkMenu(UINT idd, UINT check);

	void Show(int mode);
	void Hide();

	HWND getHWnd();

	void setMinSize(int width, int height);

	void sizingMsg(WPARAM wParam, LPARAM lParam, BOOL keepRatio = FALSE);
	void setClientSize(int width, int height);
};

class THREADCLASS
{
	friend DWORD WINAPI ThreadProc(LPVOID lpParameter);
	HANDLE	hThread;

public:
	THREADCLASS();
	virtual ~THREADCLASS();
	bool createThread();
	void closeThread();

protected:
	DWORD		threadID;
	virtual DWORD ThreadFunc()=NULL;
};

class TOOLSCLASS : public THREADCLASS
{
private:
	HWND		hwnd;
	HINSTANCE	hInstance;
	DLGPROC		dlgproc;
	int			idd;
	char		class_name[256];
	char		class_name2[256];

protected:
	DWORD	ThreadFunc();

public:
	TOOLSCLASS(HINSTANCE hInst, int IDD, DLGPROC wndproc);
	virtual ~TOOLSCLASS();
	bool open();
	bool close();
	void regClass(LPSTR class_name, WNDPROC wproc, bool SecondReg = false);
	void unregClass();
};

#endif
