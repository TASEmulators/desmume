/*
	Copyright (C) 2006 yopyop
	Copyright (C) 2006-2010 DeSmuME team

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

#include "CWindow.h"
#include "main.h"
#include "IORegView.h"
#include "debug.h"
#include "resource.h"
#include <windowsx.h>

#include <commctrl.h>

//-----------------------------------------------------------------------------
//   The Toolkit - Helpers
//-----------------------------------------------------------------------------

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

void MakeBitmapPseudoTransparent(HBITMAP hBmp, COLORREF cKeyColor, COLORREF cNewKeyColor)
{
	u8 keyr = (cKeyColor >> 16) & 0xFF;
	u8 keyg = (cKeyColor >> 8) & 0xFF;
	u8 keyb = cKeyColor & 0xFF;
	u8 nkeyr = (cNewKeyColor >> 16) & 0xFF;
	u8 nkeyg = (cNewKeyColor >> 8) & 0xFF;
	u8 nkeyb = cNewKeyColor & 0xFF;

	BITMAP bmp;
	BITMAPINFO bmpinfo;
	u8* bmpdata = NULL;
	
	HDC dc = CreateCompatibleDC(NULL);

	GetObject(hBmp, sizeof(BITMAP), &bmp);

	memset(&bmpinfo, 0, sizeof(BITMAPINFO));
	bmpinfo.bmiHeader.biSize = sizeof(BITMAPINFO);
	bmpinfo.bmiHeader.biBitCount = 24;
	bmpinfo.bmiHeader.biCompression = BI_RGB;
	bmpinfo.bmiHeader.biHeight = bmp.bmHeight;
	bmpinfo.bmiHeader.biWidth = bmp.bmWidth;
	bmpinfo.bmiHeader.biPlanes = bmp.bmPlanes;
	bmpdata = new u8[bmp.bmHeight * bmp.bmWidth * 3];

	GetDIBits(dc, hBmp, 0, bmp.bmHeight, (LPVOID)bmpdata, &bmpinfo, DIB_RGB_COLORS);

	int y2 = 0;
	for (int y = 0; y < bmp.bmHeight; y++)
	{
		for (int x = 0; x < bmp.bmWidth; x++)
		{
			int idx = y2 + x*3;
			u8 r = bmpdata[idx + 0];
			u8 g = bmpdata[idx + 1];
			u8 b = bmpdata[idx + 2];

			if ((r == keyr) && (g == keyg) && (b == keyb))
			{
				bmpdata[idx + 0] = nkeyr;
				bmpdata[idx + 1] = nkeyg;
				bmpdata[idx + 2] = nkeyb;
			}
		}
		y2 += bmp.bmWidth * 3;
	}

	SetDIBits(dc, hBmp, 0, bmp.bmHeight, (LPVOID)bmpdata, &bmpinfo, DIB_RGB_COLORS);
	DeleteDC(dc);
	delete[] bmpdata;
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

CToolWindow::CToolWindow(char* _className, WNDPROC _proc, char* _title, int _width, int _height)
	: hWnd(NULL)
	, className(_className)
	, proc((DLGPROC)_proc)
	, title(_title)
	, width(_width)
	, height(_height)
	, whichInit(0)
{
}

CToolWindow::CToolWindow(int _ID, DLGPROC _proc, char* _title)
	: hWnd(NULL)
	, ID(_ID)
	, proc(_proc)
	, title(_title)
	, whichInit(1)
{
}

void CToolWindow::PostInitialize()
{
	if(whichInit==0)
	{
		DWORD style = WS_CAPTION | WS_SYSMENU | WS_SIZEBOX | WS_MINIMIZEBOX | WS_CLIPCHILDREN | WS_CLIPSIBLINGS;
		RECT rc;

		rc.left = 0;
		rc.right = width;
		rc.top = 0;
		rc.bottom = height;
		AdjustWindowRect(&rc, style, FALSE);

		hWnd = CreateWindow(className, title.c_str(), style, CW_USEDEFAULT, CW_USEDEFAULT, 
			rc.right - rc.left, rc.bottom - rc.top, HWND_DESKTOP, NULL, hAppInst, (LPVOID)this);
	}
	else
	{
		hWnd = CreateDialogParam(hAppInst, MAKEINTRESOURCE(ID), HWND_DESKTOP, proc, (LPARAM)this);
		if (hWnd == NULL)
			return;

		SetWindowText(hWnd, title.c_str());
		SendMessage(hWnd, WM_SETICON, ICON_BIG, (LPARAM)LoadIcon(hAppInst, MAKEINTRESOURCE(ICONDESMUME)));
	}
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

//-----------------------------------------------------------------------------
//   The Toolkit - Toolbar API wrapper
//-----------------------------------------------------------------------------

CToolBar::CToolBar(HWND hParent)
	: hidden(false)
{
	// Create the toolbar
	// Note: dropdown buttons look like crap without TBSTYLE_FLAT
	hWnd = CreateWindowEx(0, TOOLBARCLASSNAME, NULL, 
		WS_CHILD | WS_VISIBLE | WS_BORDER | TBSTYLE_FLAT | TBSTYLE_TOOLTIPS, 
		0, 0, 0, 0, hParent, NULL, hAppInst, NULL);

	// Send it a few messages to finish setting it up
	SendMessage(hWnd, TB_BUTTONSTRUCTSIZE, sizeof(TBBUTTON), 0);
	SendMessage(hWnd, TB_SETEXTENDEDSTYLE, 0, TBSTYLE_EX_DRAWDDARROWS);
}

CToolBar::~CToolBar()
{
	// Delete all the HBITMAPs we kept stored
	for (TBitmapList::iterator it = hBitmaps.begin(); 
		it != hBitmaps.end(); it++)
	{
		DeleteObject(it->second.second);
	}

	hBitmaps.clear();
}

void CToolBar::Show(bool bShow)
{
	DWORD style = GetWindowLong(hWnd, GWL_STYLE);

	if (bShow)
	{
		hidden = false;
		style |= WS_VISIBLE;
	}
	else
	{
		hidden = true;
		style &= ~WS_VISIBLE;
	}

	SetWindowLong(hWnd, GWL_STYLE, style);
}

void CToolBar::OnSize()
{
	// Not-fully-working wraparound handling code
	// the toolbar behaves weirdly when it comes to separators
	// TODO: figure out why.
	// Note: right now this code is useless, but it may be useful
	// if we use more toolbars
#if 0
	RECT rc;
	int parentwidth;

	GetClientRect(GetParent(hWnd), &rc);
	parentwidth = rc.right - rc.left;

	// When there's not enough space to fit all the buttons in one line,
	// we have to add the 'linebreaks' ourselves
	int numbtns = SendMessage(hWnd, TB_BUTTONCOUNT, 0, 0);
	int curwidth = 0;

	for (int i = 0; i < numbtns; i++)
	{
		SendMessage(hWnd, TB_GETITEMRECT, i, (LPARAM)&rc);
		curwidth += (rc.right - rc.left);

		if (i > 1)
		{
			TBBUTTON btn;
			TBBUTTONINFO btninfo;
			int cmdid;

			SendMessage(hWnd, TB_GETBUTTON, i-1, (LPARAM)&btn);
			if (btn.idCommand == -1)
				SendMessage(hWnd, TB_GETBUTTON, i-2, (LPARAM)&btn);

			cmdid = btn.idCommand;

			// Add/remove the TBSTATE_WRAP style if needed
			btninfo.cbSize = sizeof(TBBUTTONINFO);
			btninfo.dwMask = TBIF_STATE;
			SendMessage(hWnd, TB_GETBUTTONINFO, cmdid, (LPARAM)&btninfo);

			btninfo.dwMask = TBIF_STATE;
			if (curwidth > parentwidth) btninfo.fsState |= TBSTATE_WRAP;
			else btninfo.fsState &= ~TBSTATE_WRAP;
			SendMessage(hWnd, TB_SETBUTTONINFO, cmdid, (LPARAM)&btninfo);

			if (curwidth > parentwidth) curwidth = 0;
		}
	}
#endif
	SetWindowPos(hWnd, NULL, 
		CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, 
		SWP_NOZORDER | SWP_NOMOVE);
}

void CToolBar::AppendButton(int uID, int uBitmapID, DWORD dwState, bool bDropdown)
{
	HBITMAP hbmp;
	TBADDBITMAP bmp;
	TBBUTTON btn;

	// Get the bitmap and replace the key color (magenta) with the right color
	hbmp = LoadBitmap(hAppInst, MAKEINTRESOURCE(uBitmapID));
	MakeBitmapPseudoTransparent(hbmp, RGB(255, 0, 255), GetSysColor(COLOR_BTNFACE));

	// Add the bitmap to the toolbar's image list
	bmp.hInst = NULL;
	bmp.nID = (UINT_PTR)hbmp;

	int bmpid = SendMessage(hWnd, TB_ADDBITMAP, 1, (LPARAM)&bmp);

	// Save the bitmap (if it gets deleted, the toolbar is too)
	hBitmaps[uBitmapID] = TBitmapPair(bmpid, hbmp);

	// And finally add the button
	memset(&btn, 0, sizeof(TBBUTTON));
	btn.fsStyle = bDropdown ? TBSTYLE_DROPDOWN : TBSTYLE_BUTTON;
	btn.fsState = dwState;
	btn.idCommand = uID;
	btn.iString = -1;
	btn.iBitmap = bmpid;

	SendMessage(hWnd, TB_ADDBUTTONS, 1, (LPARAM)&btn);
}

void CToolBar::AppendSeparator()
{
	TBBUTTON btn;

	memset(&btn, 0, sizeof(TBBUTTON));
	btn.fsStyle = TBSTYLE_SEP;
	btn.idCommand = -1;
	btn.iString = -1;

	SendMessage(hWnd, TB_ADDBUTTONS, 1, (LPARAM)&btn);
}

void CToolBar::ChangeButtonBitmap(int uID, int uBitmapID)
{
	int bmpid = 0;

	// If we don't already have the bitmap, retrieve it,
	// adapt it and store it in the list
	TBitmapList::iterator it = hBitmaps.find(uBitmapID);

	if (it == hBitmaps.end())
	{
		HBITMAP hbmp;
		TBADDBITMAP bmp;

		hbmp = LoadBitmap(hAppInst, MAKEINTRESOURCE(uBitmapID));
		MakeBitmapPseudoTransparent(hbmp, RGB(255, 0, 255), GetSysColor(COLOR_BTNFACE));

		bmp.hInst = NULL;
		bmp.nID = (UINT_PTR)hbmp;

		bmpid = SendMessage(hWnd, TB_ADDBITMAP, 1, (LPARAM)&bmp);

		hBitmaps[uBitmapID] = TBitmapPair(bmpid, hbmp);
	}
	else
		bmpid = hBitmaps[uBitmapID].first;

	// Finally change the bitmap
	SendMessage(hWnd, TB_CHANGEBITMAP, uID, MAKELPARAM(bmpid, 0));
}

void CToolBar::EnableButtonDropdown(int uID, bool bDropdown)
{
	TBBUTTONINFO btninfo;

	memset(&btninfo, 0, sizeof(TBBUTTONINFO));
	btninfo.cbSize = sizeof(TBBUTTONINFO);
	btninfo.dwMask = TBIF_STYLE;

	SendMessage(hWnd, TB_GETBUTTONINFO, uID, (LPARAM)&btninfo);

	btninfo.dwMask = TBIF_STYLE;
	if (bDropdown)
	{
		btninfo.fsStyle &= ~TBSTYLE_BUTTON;
		btninfo.fsStyle |= TBSTYLE_DROPDOWN;
	}
	else
	{
		btninfo.fsStyle |= TBSTYLE_BUTTON;
		btninfo.fsStyle &= ~TBSTYLE_DROPDOWN;
	}

	SendMessage(hWnd, TB_SETBUTTONINFO, uID, (LPARAM)&btninfo);
}

int CToolBar::GetHeight()
{
	if (hidden) return 0;

	RECT rc; GetWindowRect(hWnd, &rc);
	return rc.bottom - rc.top - 1;
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
	return SetMenu(hwnd, hmenu)!=FALSE;
}

bool WINCLASS::addMenuItem(u32 item, bool byPos, LPCMENUITEMINFO info)
{
	return InsertMenuItem(hmenu, item, byPos, info);
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

static void MyAdjustWindowRectEx(RECT* rect, HWND hwnd)
{
	AdjustWindowRectEx(rect,GetWindowStyle(hwnd),TRUE,GetWindowExStyle(hwnd));
	
	//get height of one menu to subtract off
	int cymenu = GetSystemMetrics(SM_CYMENU);
	
	//get the height of the actual menu to add back on
	MENUBARINFO mbi;
	ZeroMemory(&mbi, sizeof(mbi));
	mbi.cbSize = sizeof(mbi);
	GetMenuBarInfo(hwnd, OBJID_MENU, 0, &mbi);
	int menuHeight = (mbi.rcBar.bottom - mbi.rcBar.top + 1);

	rect->bottom -= cymenu;
	rect->bottom += menuHeight;
}

void WINCLASS::sizingMsg(WPARAM wParam, LPARAM lParam, LONG keepRatio)
{
	RECT *rect = (RECT*)lParam;

	int prevRight = rect->right;
	int prevBottom = rect->bottom;

	int _minWidth, _minHeight;

	int tbheight = MainWindowToolbar->GetHeight();

	RECT adjr;
	SetRect(&adjr,0,0,minWidth,minHeight);
	MyAdjustWindowRectEx(&adjr,hwnd);

	RECT frameInfo; 
	SetRect(&frameInfo,0,0,0,0);
	MyAdjustWindowRectEx(&frameInfo,hwnd);
	int frameWidth = frameInfo.right-frameInfo.left;
	int frameHeight = frameInfo.bottom-frameInfo.top + tbheight;

	// Calculate the minimum size in pixels
	_minWidth = adjr.right-adjr.left;
	_minHeight = adjr.bottom-adjr.top + tbheight;

	/* Clamp the size to the minimum size (256x384) */
	rect->right = (rect->left + std::max(_minWidth, (int)(rect->right - rect->left)));
	rect->bottom = (rect->top + std::max(_minHeight, (int)(rect->bottom - rect->top)));

	bool horizontalDrag = (wParam == WMSZ_LEFT) || (wParam == WMSZ_RIGHT);
	bool verticalDrag = (wParam == WMSZ_TOP) || (wParam == WMSZ_BOTTOM);
	if(verticalDrag && !(keepRatio & KEEPY))
	{
		int clientHeight = rect->bottom - rect->top - frameHeight;
		if(clientHeight < minHeight)
			rect->bottom += minHeight - clientHeight;
	}
	else if(horizontalDrag && !(keepRatio & KEEPX))
	{
		int clientWidth = rect->right - rect->left - frameWidth;
		if(clientWidth < minWidth)
			rect->right += minWidth - clientWidth;
	}
	else
	{
		//Apply the ratio stuff

		float ratio1 = ((rect->right  - rect->left - frameWidth ) / (float)minWidth);
		float ratio2 = ((rect->bottom - rect->top  - frameHeight) / (float)minHeight);
		float ratio = std::min(ratio1,ratio2);
		if(keepRatio & FULLSCREEN)
		{
			keepRatio |= WINCLASS::KEEPX | WINCLASS::KEEPY;
			ratio1 = ratio;
			ratio2 = ratio;
		}

		LONG correctedHeight = (LONG)((rect->top  + frameHeight + (minHeight * ratio1)));
		LONG correctedWidth  = (LONG)((rect->left + frameWidth + (minWidth  * ratio2)));

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
		//else
		{
			if((keepRatio & KEEPY) && (rect->right < correctedWidth) || (keepRatio&FULLSCREEN))
			{
				if(horizontalDrag)
					rect->bottom = correctedHeight;
				else
					rect->right = correctedWidth;
			}
		}
	}

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
	height += MainWindowToolbar->GetHeight();

	//yep, do it twice, once in case the menu wraps, and once to accomodate that wrap
	for(int i=0;i<2;i++)
	{
		RECT rect;
		SetRect(&rect,0,0,width,height);
		MyAdjustWindowRectEx(&rect,hwnd);
		SetWindowPos(hwnd,0,0,0,rect.right-rect.left,rect.bottom-rect.top,SWP_NOMOVE|SWP_NOZORDER);
	}
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

bool TOOLSCLASS::open(bool useThread)
{
	if(useThread)
	{
		if (!createThread()) return false;
		else return true;
	}

	if(doOpen()) return false;
	else return true;
}

bool TOOLSCLASS::close()
{
	return true;
}

DWORD TOOLSCLASS::doOpen()
{
	GetLastError();
	hwnd = CreateDialog(hInstance, MAKEINTRESOURCE(idd), NULL, (DLGPROC) dlgproc);

	if (!hwnd) 
	{
		LOG("error creating dialog\n");
		return (-2);
	}

	ShowWindow(hwnd, SW_SHOW);
	UpdateWindow(hwnd);

	return 0;
}

void TOOLSCLASS::doClose()
{
	unregClass();
	hwnd = NULL;
}

DWORD TOOLSCLASS::ThreadFunc()
{
	LOG("Start thread\n");


	DWORD ret = doOpen();
	if(ret) return ret;
	
	MSG messages;
	while (GetMessage (&messages, NULL, 0, 0))
	{
		TranslateMessage(&messages);
		DispatchMessage(&messages);
	}

	doClose();

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
