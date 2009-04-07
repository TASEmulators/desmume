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

#include "CWindow.h"
#include "debug.h"

WINCLASS::WINCLASS(LPSTR rclass, HINSTANCE hInst)
{
	memset(regclass, 0, sizeof(regclass));
	memcpy(regclass, rclass, strlen(rclass));

	hwnd = NULL;
	hmenu = NULL;
	hInstance = hInst;
	
	minWidth = 0;
	minHeight = 0;
}

WINCLASS::~WINCLASS()
{
}

bool WINCLASS::create(LPSTR caption, int x, int y, int width, int height, int style, HMENU menu)
{
	if (hwnd != NULL) return false;
	
	hwnd = CreateWindow(regclass, caption, style, x, y, width, height, NULL, menu, hInstance, NULL);
	
	if (hwnd != NULL) return true;
	return false;
}

bool WINCLASS::createEx(LPSTR caption, int x, int y, int width, int height, int style,  int styleEx, HMENU menu)
{
	if (hwnd != NULL) return false;

	hwnd = CreateWindowEx(styleEx, regclass, caption, style, x, y, width, height, NULL, menu, hInstance, NULL);
	
	if (hwnd != NULL) return true;
	return false;
}

bool WINCLASS::setMenu(HMENU menu)
{
	hmenu = menu;
	return SetMenu(hwnd, hmenu);
}

DWORD WINCLASS::checkMenu(UINT idd, UINT check)
{
	return CheckMenuItem(hmenu, idd, check);
}

HWND WINCLASS::getHWnd()
{
	return hwnd;
}

void WINCLASS::Show(int mode)
{
	ShowWindow(hwnd, mode);
}

void WINCLASS::Hide()
{
	ShowWindow(hwnd, SW_HIDE);
}

void WINCLASS::setMinSize(int width, int height)
{
	minWidth = width;
	minHeight = height;
}

void WINCLASS::sizingMsg(WPARAM wParam, LPARAM lParam, BOOL keepRatio)
{
	RECT *rect = (RECT*)lParam;

	int _minWidth, _minHeight;

	int xborder, yborder;
	int ymenu, ymenunew;
	int ycaption;

	MENUBARINFO mbi;

	/* Get the size of the border */
	xborder = GetSystemMetrics(SM_CXSIZEFRAME);
	yborder = GetSystemMetrics(SM_CYSIZEFRAME);
	
	/* Get the size of the menu bar */
	ZeroMemory(&mbi, sizeof(mbi));
	mbi.cbSize = sizeof(mbi);
	GetMenuBarInfo(hwnd, OBJID_MENU, 0, &mbi);
	ymenu = (mbi.rcBar.bottom - mbi.rcBar.top + 1);

	/* Get the size of the caption bar */
	ycaption = GetSystemMetrics(SM_CYCAPTION);

	/* Calculate the minimum size in pixels */
	_minWidth = (xborder + minWidth + xborder);
	_minHeight = (ycaption + yborder + ymenu + minHeight + yborder);

	/* Clamp the size to the minimum size (256x384) */
	rect->right = (rect->left + std::max(_minWidth, (int)(rect->right - rect->left)));
	rect->bottom = (rect->top + std::max(_minHeight, (int)(rect->bottom - rect->top)));

	/* Apply the ratio stuff */
	if(keepRatio)
	{
		switch(wParam)
		{
		case WMSZ_LEFT:
		case WMSZ_RIGHT:
		case WMSZ_TOPLEFT:
		case WMSZ_TOPRIGHT:
		case WMSZ_BOTTOMLEFT:
		case WMSZ_BOTTOMRIGHT:
			{
				float ratio = ((rect->right - rect->left - xborder - xborder) / (float)minWidth);
				rect->bottom = (rect->top + ycaption + yborder + ymenu + (minHeight * ratio) + yborder);
			}
			break;
				
		case WMSZ_TOP:
		case WMSZ_BOTTOM:
			{
				float ratio = ((rect->bottom - rect->top - ycaption - yborder - ymenu - yborder) / (float)minHeight);
				rect->right = (rect->left + xborder + (minWidth * ratio) + xborder);
			}
			break;
		}
	}

	/* Check if the height of the menu has changed during the resize */
	ZeroMemory(&mbi, sizeof(mbi));
	mbi.cbSize = sizeof(mbi);
	GetMenuBarInfo(hwnd, OBJID_MENU, 0, &mbi);
	ymenunew = (mbi.rcBar.bottom - mbi.rcBar.top + 1);

	if(ymenunew != ymenu)
		rect->bottom += (ymenunew - ymenu);
}

void WINCLASS::setClientSize(int width, int height)
{
	int xborder, yborder;
	int ymenu, ymenunew;
	int ycaption;

	MENUBARINFO mbi;

	RECT wndRect;
	int finalx, finaly;

	/* Get the size of the border */
	xborder = GetSystemMetrics(SM_CXSIZEFRAME);
	yborder = GetSystemMetrics(SM_CYSIZEFRAME);
	
	/* Get the size of the menu bar */
	ZeroMemory(&mbi, sizeof(mbi));
	mbi.cbSize = sizeof(mbi);
	GetMenuBarInfo(hwnd, OBJID_MENU, 0, &mbi);
	ymenu = (mbi.rcBar.bottom - mbi.rcBar.top + 1);

	/* Get the size of the caption bar */
	ycaption = GetSystemMetrics(SM_CYCAPTION);

	/* Finally, resize the window */
	GetWindowRect(hwnd, &wndRect);
	finalx = (xborder + width + xborder);
	finaly = (ycaption + yborder + ymenu + height + yborder);
	MoveWindow(hwnd, wndRect.left, wndRect.top, finalx, finaly, TRUE);

	/* Oops, we also need to check if the height */
	/* of the menu bar has changed after the resize */
	ZeroMemory(&mbi, sizeof(mbi));
	mbi.cbSize = sizeof(mbi);
	GetMenuBarInfo(hwnd, OBJID_MENU, 0, &mbi);
	ymenunew = (mbi.rcBar.bottom - mbi.rcBar.top + 1);

	if(ymenunew != ymenu)
		MoveWindow(hwnd, wndRect.left, wndRect.top, finalx, (finaly + (ymenunew - ymenu)), TRUE);
}

//========================================================= Thread class
extern DWORD WINAPI ThreadProc(LPVOID lpParameter)
{
	THREADCLASS *tmp = (THREADCLASS *)lpParameter;
	return tmp->ThreadFunc();
}

THREADCLASS::THREADCLASS()
{
	hThread = NULL;
}

THREADCLASS::~THREADCLASS()
{
	closeThread();
}

void THREADCLASS::closeThread()
{
	if (hThread) 
	{
		CloseHandle(hThread);
		hThread = NULL;
	}
}

bool THREADCLASS::createThread()
{
	if (hThread) return false;

	hThread = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)ThreadProc, this, 0, &threadID);
	if (!hThread) return false;
	//WaitForSingleObject(hThread, INFINITE);
	return true;
}

//========================================================= Tools class
TOOLSCLASS::TOOLSCLASS(HINSTANCE hInst, int IDD, DLGPROC dlgproc)
{
	this->dlgproc = dlgproc;
	hwnd = NULL;
	hInstance = hInst;
	idd=IDD;
	memset(class_name, 0, sizeof(class_name));
	memset(class_name2, 0, sizeof(class_name2));
}

TOOLSCLASS::~TOOLSCLASS()
{
	close();
}

bool TOOLSCLASS::open()
{
	if (!createThread()) return false;
	return true;
}

bool TOOLSCLASS::close()
{
	return true;
}

DWORD TOOLSCLASS::ThreadFunc()
{
	MSG		messages;
	LOG("Start thread\n");

	GetLastError();
	hwnd = CreateDialog(hInstance, MAKEINTRESOURCE(idd), NULL, (DLGPROC) dlgproc);

	if (!hwnd) 
	{
		LOG("error creating dialog\n");
		return (-2);
	}

	ShowWindow(hwnd, SW_SHOW);
	UpdateWindow(hwnd);
	
	while (GetMessage (&messages, NULL, 0, 0))
	{
		TranslateMessage(&messages);
		DispatchMessage(&messages);
	}

	unregClass();
	hwnd = NULL;
	
	closeThread();
	return 0;
}

void TOOLSCLASS::regClass(LPSTR class_name, WNDPROC wproc, bool SecondReg)
{
	WNDCLASSEX	wc;

	wc.cbSize         = sizeof(wc);
	if (SecondReg)
		strcpy(this->class_name2, class_name);
	else
		strcpy(this->class_name, class_name);

	wc.lpszClassName  = class_name;
	wc.hInstance      = hInstance;
	wc.lpfnWndProc    = wproc;
	wc.hCursor        = LoadCursor (NULL, IDC_ARROW);
	wc.hIcon          = 0;
	wc.lpszMenuName   = 0;
	wc.hbrBackground  = (HBRUSH)GetSysColorBrush(COLOR_BTNFACE);
	wc.style          = 0;
	wc.cbClsExtra     = 0;
	wc.cbWndExtra     = 0;
	wc.hIconSm        = 0;

	RegisterClassEx(&wc);
}

void TOOLSCLASS::unregClass()
{
	if (class_name[0])
	{
		UnregisterClass(class_name, hInstance);
	}
	if (class_name2[0])
	{
		UnregisterClass(class_name2, hInstance);
	}
	memset(class_name, 0, sizeof(class_name));
	memset(class_name2, 0, sizeof(class_name2));
}
