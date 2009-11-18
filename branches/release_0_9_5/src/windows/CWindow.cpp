/*  Copyright (C) 2006 yopyop
    Copyright (C) 2006-2009 DeSmuME team

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
#include "IORegView.h"
#include "debug.h"

DWORD GetFontQuality()
{
	BOOL aaEnabled = FALSE;
	UINT aaType = FE_FONTSMOOTHINGSTANDARD;

	SystemParametersInfo(SPI_GETFONTSMOOTHING, 0, &aaEnabled, 0);
	if (aaEnabled == FALSE)
		return NONANTIALIASED_QUALITY;

	if (SystemParametersInfo(SPI_GETFONTSMOOTHINGTYPE, 0, &aaType, 0) == FALSE)
		return ANTIALIASED_QUALITY;

	if (aaType == FE_FONTSMOOTHINGCLEARTYPE)
		return CLEARTYPE_QUALITY;
	else
		return ANTIALIASED_QUALITY;
}

int DrawText(HDC hDC, char* text, int X, int Y, int Width, int Height, UINT format)
{
	RECT rc;
	SetRect(&rc, X, Y, X+Width, Y+Height);
	return DrawText(hDC, text, -1, &rc, format);
}

void GetFontSize(HWND hWnd, HFONT hFont, LPSIZE size)
{
	HDC dc = GetDC(hWnd);
	HFONT oldfont = (HFONT)SelectObject(dc, hFont);
	
	GetTextExtentPoint32(dc, " ", 1, size);

	SelectObject(dc, oldfont);
	ReleaseDC(hWnd, dc);
}

//-----------------------------------------------------------------------------
// Window class handling
//-----------------------------------------------------------------------------

vector<string> ReggedWndClasses;

bool RegWndClass(string name, WNDPROC wndProc, UINT style, int extraSize)
{
	return RegWndClass(name, wndProc, style, NULL, extraSize);
}

bool RegWndClass(string name, WNDPROC wndProc, UINT style, HICON icon, int extraSize)
{
	// If the class is already regged, don't re-reg it
	if (find(ReggedWndClasses.begin(), ReggedWndClasses.end(), name) != ReggedWndClasses.end())
		return true;

	WNDCLASSEX wc;

	wc.cbSize         = sizeof(wc);
	wc.lpszClassName  = name.c_str();
	wc.hInstance      = hAppInst;
	wc.lpfnWndProc    = wndProc;
	wc.hCursor        = LoadCursor(NULL, IDC_ARROW);
	wc.hIcon          = icon;
	wc.lpszMenuName   = 0;
	wc.hbrBackground  = GetSysColorBrush(COLOR_BTNFACE);
	wc.style          = style;
	wc.cbClsExtra     = 0;
	wc.cbWndExtra     = DWLP_USER + extraSize;
	wc.hIconSm        = 0;

	if (RegisterClassEx(&wc) != 0)
	{
		// If registration succeeded, add the class name into the list
		ReggedWndClasses.push_back(name);
		return true;
	}
	else
		return false;
}

void UnregWndClass(string name)
{
	vector<string>::iterator it = find(ReggedWndClasses.begin(), ReggedWndClasses.end(), name);

	// If the class wasn't regged, we can't unreg it :P
	if (it == ReggedWndClasses.end())
		return;

	// Otherwise unreg the class and remove its name from the list
	// ONLY if unregging was successful. Unregging will fail if one
	// or more windows using the class still exist.
	if (UnregisterClass(name.c_str(), hAppInst) != 0)
		ReggedWndClasses.erase(it);
}

//-----------------------------------------------------------------------------
// Base toolwindow class
//-----------------------------------------------------------------------------

CToolWindow::CToolWindow(char* className, WNDPROC proc, char* title, int width, int height)
	: hWnd(NULL)
{
	DWORD style = WS_CAPTION | WS_SYSMENU | WS_SIZEBOX | WS_MINIMIZEBOX | WS_CLIPCHILDREN | WS_CLIPSIBLINGS;
	RECT rc;

	rc.left = 0;
	rc.right = width;
	rc.top = 0;
	rc.bottom = height;
	AdjustWindowRect(&rc, style, FALSE);

	hWnd = CreateWindow(className, title, style, CW_USEDEFAULT, CW_USEDEFAULT, 
		rc.right - rc.left, rc.bottom - rc.top, HWND_DESKTOP, NULL, hAppInst, (LPVOID)this);
}

CToolWindow::CToolWindow(int ID, DLGPROC proc, char* title)
	: hWnd(NULL)
{
	hWnd = CreateDialogParam(hAppInst, MAKEINTRESOURCE(ID), HWND_DESKTOP, proc, (LPARAM)this);
	if (hWnd == NULL)
		return;

	SetWindowText(hWnd, title);
	SendMessage(hWnd, WM_SETICON, ICON_BIG, (LPARAM)LoadIcon(hAppInst, "ICONDESMUME"));
}

CToolWindow::~CToolWindow()
{
}

//-----------------------------------------------------------------------------
// Toolwindow handling
//-----------------------------------------------------------------------------

CToolWindow* ToolWindowList = NULL;

bool OpenToolWindow(CToolWindow* wnd)
{
	// A hWnd value of NULL indicates failure to create the window.
	// In this case, just delete the toolwindow and return failure.
	if (wnd->hWnd == NULL)
	{
		delete wnd;
		return false;
	}

	// Add the toolwindow to the list
	if (ToolWindowList == NULL)
	{
		ToolWindowList = wnd;
		wnd->prev = NULL;
		wnd->next = NULL;
	}
	else
	{
		wnd->prev = NULL;
		wnd->next = ToolWindowList;
		wnd->next->prev = wnd;
		ToolWindowList = wnd;
	}

	// Show the toolwindow (otherwise it won't show :P )
	wnd->Show();

	return true;
}

void CloseToolWindow(CToolWindow* wnd)
{
	// Remove the toolwindow from the list
	if (wnd == ToolWindowList)
	{
		ToolWindowList = wnd->next;
		if (wnd->next) wnd->next->prev = NULL;
		wnd->next = NULL;
	}
	else
	{
		wnd->prev->next = wnd->next;
		if (wnd->next) wnd->next->prev = wnd->prev;
		wnd->prev = NULL;
		wnd->next = NULL;
	}

	// Delete the toolwindow object
	// its destructor will destroy the window
	delete wnd;
}

void CloseAllToolWindows()
{
	CToolWindow* wnd;
	CToolWindow* wnd_next;

	wnd = ToolWindowList;
	while (wnd)
	{
		wnd_next = wnd->next;

		wnd->prev = NULL;
		wnd->next = NULL;
		delete wnd;

		wnd = wnd_next;
	}

	ToolWindowList = NULL;
}

void RefreshAllToolWindows()
{
	CToolWindow* wnd;

	if (ToolWindowList == NULL)
		return;

	wnd = ToolWindowList;
	while (wnd)
	{
		wnd->Refresh();
		wnd = wnd->next;
	}
}


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
	return SetMenu(hwnd, hmenu)==TRUE;
}

DWORD WINCLASS::checkMenu(UINT idd, bool check)
{
	return CheckMenuItem(hmenu, idd, MF_BYCOMMAND | (check?MF_CHECKED:MF_UNCHECKED));
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

void WINCLASS::sizingMsg(WPARAM wParam, LPARAM lParam, LONG keepRatio)
{
	RECT *rect = (RECT*)lParam;

	int prevRight = rect->right;
	int prevBottom = rect->bottom;

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

	bool horizontalDrag = (wParam == WMSZ_LEFT) || (wParam == WMSZ_RIGHT);
	bool verticalDrag = (wParam == WMSZ_TOP) || (wParam == WMSZ_BOTTOM);
	if(verticalDrag && !(keepRatio & KEEPY))
	{
		int clientHeight = rect->bottom - rect->top  - ycaption - yborder - ymenu - yborder;
		if(clientHeight < minHeight)
			rect->bottom += minHeight - clientHeight;
	}
	else if(horizontalDrag && !(keepRatio & KEEPX))
	{
		int clientWidth = rect->right  - rect->left - xborder  - xborder;
		if(clientWidth < minWidth)
			rect->right += minWidth - clientWidth;
	}
	else
	{
		/* Apply the ratio stuff */

		float ratio1 = ((rect->right  - rect->left - xborder  - xborder                  ) / (float)minWidth);
		float ratio2 = ((rect->bottom - rect->top  - ycaption - yborder - ymenu - yborder) / (float)minHeight);
		LONG correctedHeight = (LONG)((rect->top  + ycaption + yborder + ymenu + (minHeight * ratio1) + yborder));
		LONG correctedWidth  = (LONG)((rect->left +            xborder         + (minWidth  * ratio2) + xborder));

		if(keepRatio & KEEPX)
		{
			if((keepRatio & KEEPY) || (rect->bottom < correctedHeight))
			{
				if(verticalDrag)
					rect->right = correctedWidth;
				else
					rect->bottom = correctedHeight;
			}
		}
		else
		{
			if((keepRatio & KEEPY) && (rect->right < correctedWidth))
			{
				if(horizontalDrag)
					rect->bottom = correctedHeight;
				else
					rect->right = correctedWidth;
			}
		}
	}

	/* Check if the height of the menu has changed during the resize */
	ZeroMemory(&mbi, sizeof(mbi));
	mbi.cbSize = sizeof(mbi);
	GetMenuBarInfo(hwnd, OBJID_MENU, 0, &mbi);
	ymenunew = (mbi.rcBar.bottom - mbi.rcBar.top + 1);

	if(ymenunew != ymenu)
		rect->bottom += (ymenunew - ymenu);

	// prevent "pushing" the window across the screen when resizing from the left or top
	if(wParam == WMSZ_LEFT || wParam == WMSZ_TOPLEFT || wParam == WMSZ_BOTTOMLEFT)
	{
		rect->left -= rect->right - prevRight;
		rect->right = prevRight;
	}
	if(wParam == WMSZ_TOP || wParam == WMSZ_TOPLEFT || wParam == WMSZ_TOPRIGHT)
	{
		rect->top -= rect->bottom - prevBottom;
		rect->bottom = prevBottom;
	}

	// windows screws up the window size if the top of the window goes too high above the top of the screen
	if(keepRatio & KEEPY)
	{
		int titleBarHeight = GetSystemMetrics(SM_CYSIZE);
		int topExceeded = -(titleBarHeight + rect->top);
		if(topExceeded > 0)
		{
			rect->top += topExceeded;
			rect->bottom += topExceeded;
		}
	}
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
