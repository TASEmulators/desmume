/*
	Copyright (C) 2006 yopyop
	Copyright (C) 2006-2017 DeSmuME team

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

#include <windows.h>
#include <windowsx.h>
#include <commctrl.h>

#include "debug.h"

#include "resource.h"
#include "main.h"
#include "DPI.h"
#include "IORegView.h"
#include "windriver.h"


extern bool fsWindow;

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

vector<wstring> ReggedWndClasses;

bool RegWndClass(wstring name, WNDPROC wndProc, UINT style, int extraSize)
{
	return RegWndClass(name, wndProc, style, NULL, extraSize);
}

bool RegWndClass(wstring name, WNDPROC wndProc, UINT style, HICON icon, int extraSize)
{
	// If the class is already regged, don't re-reg it
	if (find(ReggedWndClasses.begin(), ReggedWndClasses.end(), name) != ReggedWndClasses.end())
		return true;

	WNDCLASSEXW wc;

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

	if (RegisterClassExW(&wc) != 0)
	{
		// If registration succeeded, add the class name into the list
		ReggedWndClasses.push_back(name);
		return true;
	}
	else
		return false;
}

void UnregWndClass(wstring name)
{
	vector<wstring>::iterator it = find(ReggedWndClasses.begin(), ReggedWndClasses.end(), name);

	// If the class wasn't regged, we can't unreg it :P
	if (it == ReggedWndClasses.end())
		return;

	// Otherwise unreg the class and remove its name from the list
	// ONLY if unregging was successful. Unregging will fail if one
	// or more windows using the class still exist.
	if (UnregisterClassW(name.c_str(), hAppInst) != 0)
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

		// The tool windows size themselves in pixels, which are logical units
		// here, so scale them for the monitor the main window sits on.
		HWND hRef = (MainWindow != NULL) ? MainWindow->getHWnd() : NULL;
		const int scaledWidth = DPI_ScaleForWindow(width, hRef);
		const int scaledHeight = DPI_ScaleForWindow(height, hRef);

		rc.left = 0;
		rc.right = scaledWidth;
		rc.top = 0;
		rc.bottom = scaledHeight;
		DPI_AdjustWindowRectEx(&rc, style, FALSE, 0, hRef);

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

static const int kToolBarBitmapSize = 16;

// Nearest-neighbour upscale of a toolbar bitmap to size x size.
//
// The pixels are copied through GetDIBits/SetDIBits instead of StretchBlt: the
// artwork is pixel art, so replicating pixels is both sharper and completely
// predictable, and the result stays a plain device-dependent bitmap, which is
// what TB_ADDBITMAP expects. Returns the original handle when no scaling is
// needed; otherwise the original is destroyed and the caller owns the result.
static HBITMAP ScaleBitmapToSquare(HBITMAP hbmp, int size)
{
	BITMAP bm;
	if (GetObject(hbmp, sizeof(bm), &bm) == 0)
	{
		return hbmp;
	}

	if ( (bm.bmWidth == size) && (bm.bmHeight == size) )
	{
		return hbmp;
	}

	const int srcWidth = bm.bmWidth;
	const int srcHeight = bm.bmHeight;
	if ( (srcWidth <= 0) || (srcHeight <= 0) || (size <= 0) )
	{
		return hbmp;
	}

	const int srcStride = ((srcWidth * 3) + 3) & ~3;
	const int dstStride = ((size * 3) + 3) & ~3;

	HDC dcScreen = GetDC(NULL);
	if (dcScreen == NULL)
	{
		return hbmp;
	}

	HDC dc = CreateCompatibleDC(dcScreen);
	if (dc == NULL)
	{
		ReleaseDC(NULL, dcScreen);
		return hbmp;
	}

	BITMAPINFO srcInfo;
	ZeroMemory(&srcInfo, sizeof(srcInfo));
	srcInfo.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
	srcInfo.bmiHeader.biWidth = srcWidth;
	srcInfo.bmiHeader.biHeight = srcHeight;
	srcInfo.bmiHeader.biPlanes = 1;
	srcInfo.bmiHeader.biBitCount = 24;
	srcInfo.bmiHeader.biCompression = BI_RGB;

	std::vector<u8> srcPixels((size_t)srcStride * (size_t)srcHeight);
	if (GetDIBits(dc, hbmp, 0, srcHeight, &srcPixels[0], &srcInfo, DIB_RGB_COLORS) == 0)
	{
		DeleteDC(dc);
		ReleaseDC(NULL, dcScreen);
		return hbmp;
	}

	std::vector<u8> dstPixels((size_t)dstStride * (size_t)size);
	for (int y = 0; y < size; y++)
	{
		const int srcY = (y * srcHeight) / size;
		for (int x = 0; x < size; x++)
		{
			const int srcX = (x * srcWidth) / size;
			memcpy(&dstPixels[(size_t)y * dstStride + (size_t)x * 3],
			       &srcPixels[(size_t)srcY * srcStride + (size_t)srcX * 3], 3);
		}
	}

	// Use the screen DC here: a bitmap created from an empty memory DC would
	// come out monochrome.
	HBITMAP hbmpScaled = CreateCompatibleBitmap(dcScreen, size, size);
	if (hbmpScaled == NULL)
	{
		DeleteDC(dc);
		ReleaseDC(NULL, dcScreen);
		return hbmp;
	}

	BITMAPINFO dstInfo;
	ZeroMemory(&dstInfo, sizeof(dstInfo));
	dstInfo.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
	dstInfo.bmiHeader.biWidth = size;
	dstInfo.bmiHeader.biHeight = size;
	dstInfo.bmiHeader.biPlanes = 1;
	dstInfo.bmiHeader.biBitCount = 24;
	dstInfo.bmiHeader.biCompression = BI_RGB;

	SetDIBits(dc, hbmpScaled, 0, size, &dstPixels[0], &dstInfo, DIB_RGB_COLORS);

	DeleteDC(dc);
	ReleaseDC(NULL, dcScreen);
	DeleteObject(hbmp);

	return hbmpScaled;
}

void CToolBar::CreateToolBarWindow()
{
	// Note: dropdown buttons look like crap without TBSTYLE_FLAT
	hWnd = CreateWindowEx(0, TOOLBARCLASSNAME, NULL, 
		WS_CHILD | WS_VISIBLE | WS_BORDER | TBSTYLE_FLAT | TBSTYLE_TOOLTIPS, 
		0, 0, 0, 0, hParent, NULL, hAppInst, NULL);

	if (hWnd == NULL)
	{
		return;
	}

	// Send it a few messages to finish setting it up
	SendMessage(hWnd, TB_BUTTONSTRUCTSIZE, sizeof(TBBUTTON), 0);
	SendMessage(hWnd, TB_SETEXTENDEDSTYLE, 0, TBSTYLE_EX_DRAWDDARROWS);

	// Must be set before the first bitmap is added; the button strip is sized
	// from the image size, which is what makes the toolbar grow with the DPI.
	// The unscaled case is left at the common-control default so the window
	// looks exactly as it always did at 100%.
	const int bitmapSize = DPI_Scale(kToolBarBitmapSize, dpi);
	if (bitmapSize != kToolBarBitmapSize)
	{
		SendMessage(hWnd, TB_SETBITMAPSIZE, 0, MAKELPARAM(bitmapSize, bitmapSize));
	}
}

void CToolBar::DestroyBitmaps()
{
	for (TBitmapList::iterator it = hBitmaps.begin(); it != hBitmaps.end(); it++)
	{
		DeleteObject(it->second.second);
	}

	hBitmaps.clear();
}

CToolBar::CToolBar(HWND hParent)
	: hWnd(NULL)
	, hParent(hParent)
	, dpi(DPI_GetForWindow(hParent))
	, hidden(false)
{
	CreateToolBarWindow();
}

CToolBar::~CToolBar()
{
	DestroyBitmaps();
}

void CToolBar::SetDPI(int newDPI)
{
	if ( (newDPI <= 0) || (newDPI == dpi) || (hWnd == NULL) )
	{
		return;
	}

	dpi = newDPI;

	const bool wasVisible = !hidden;

	DestroyBitmaps();
	DestroyWindow(hWnd);

	hWnd = NULL;
	CreateToolBarWindow();

	for (size_t i = 0; i < items.size(); i++)
	{
		AppendItem(items[i]);
	}

	Show(wasVisible);

	SetWindowPos(hWnd, NULL, 
		CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, 
		SWP_NOZORDER | SWP_NOMOVE);
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

void CToolBar::AppendItem(const ToolbarItem &item)
{
	if (hWnd == NULL)
	{
		return;
	}

	TBBUTTON btn;
	memset(&btn, 0, sizeof(TBBUTTON));
	btn.idCommand = -1;
	btn.iString = -1;

	if (item.separator)
	{
		btn.fsStyle = TBSTYLE_SEP;
		SendMessage(hWnd, TB_ADDBUTTONS, 1, (LPARAM)&btn);
		return;
	}

	btn.fsStyle = item.dropdown ? TBSTYLE_DROPDOWN : TBSTYLE_BUTTON;
	btn.fsState = item.state;
	btn.idCommand = item.id;
	btn.iBitmap = GetBitmapIndex(item.bitmapId);

	SendMessage(hWnd, TB_ADDBUTTONS, 1, (LPARAM)&btn);
}

int CToolBar::GetBitmapIndex(int uBitmapID)
{
	// If we don't already have the bitmap, retrieve it, scale it for the
	// current DPI, adapt its key color and store it in the list
	TBitmapList::iterator it = hBitmaps.find(uBitmapID);
	if (it != hBitmaps.end())
	{
		return it->second.first;
	}

	HBITMAP hbmp = LoadBitmap(hAppInst, MAKEINTRESOURCE(uBitmapID));
	if (hbmp == NULL)
	{
		return 0;
	}

	hbmp = ScaleBitmapToSquare(hbmp, DPI_Scale(kToolBarBitmapSize, dpi));
	MakeBitmapPseudoTransparent(hbmp, RGB(255, 0, 255), GetSysColor(COLOR_BTNFACE));

	// Add the bitmap to the toolbar's image list
	TBADDBITMAP bmp;
	bmp.hInst = NULL;
	bmp.nID = (UINT_PTR)hbmp;

	const int bmpid = (int)SendMessage(hWnd, TB_ADDBITMAP, 1, (LPARAM)&bmp);

	// Save the bitmap (if it gets deleted, the toolbar is too)
	hBitmaps[uBitmapID] = TBitmapPair(bmpid, hbmp);

	return bmpid;
}

void CToolBar::AppendButton(int uID, int uBitmapID, DWORD dwState, bool bDropdown)
{
	ToolbarItem item;
	item.id = uID;
	item.bitmapId = uBitmapID;
	item.state = dwState;
	item.dropdown = bDropdown;
	item.separator = false;

	items.push_back(item);
	AppendItem(item);
}

void CToolBar::AppendSeparator()
{
	ToolbarItem item;
	item.id = -1;
	item.bitmapId = 0;
	item.state = 0;
	item.dropdown = false;
	item.separator = true;

	items.push_back(item);
	AppendItem(item);
}

void CToolBar::ChangeButtonBitmap(int uID, int uBitmapID)
{
	// Finally change the bitmap
	SendMessage(hWnd, TB_CHANGEBITMAP, uID, MAKELPARAM(GetBitmapIndex(uBitmapID), 0));
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

WINCLASS::WINCLASS(LPWSTR rclass, HINSTANCE hInst)
{
	wcscpy(regclassW,rclass);

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

bool WINCLASS::createW(LPWSTR caption, int x, int y, int width, int height, int style, HMENU menu)
{
	if (hwnd != NULL) return false;

	hwnd = CreateWindowW(regclassW, caption, style, x, y, width, height, NULL, menu, hInstance, NULL);

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

void WINCLASS::enableMenu(UINT idd, bool enable)
{
	EnableMenuItem(hmenu, idd, MF_BYCOMMAND | (enable ? (MF_ENABLED) : MF_GRAYED));
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
	DPI_AdjustWindowRectEx(rect,GetWindowStyle(hwnd),TRUE,GetWindowExStyle(hwnd),hwnd);
	
	//get height of one menu to subtract off
	int cymenu = DPI_GetSystemMetricsForWindow(SM_CYMENU, hwnd);
	
	//get the height of the actual menu to add back on
	MENUBARINFO mbi;
	ZeroMemory(&mbi, sizeof(mbi));
	mbi.cbSize = sizeof(mbi);
	GetMenuBarInfo(hwnd, OBJID_MENU, 0, &mbi);
	
	//if the menubar exists, its height is off by 1 (frame between bar and client area?)
	int menuHeight = (mbi.rcBar.bottom - mbi.rcBar.top);
	if(menuHeight != 0) menuHeight++;

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
	if(!(keepRatio & FULLSCREEN))
		MyAdjustWindowRectEx(&adjr,hwnd);

	RECT frameInfo; 
	SetRect(&frameInfo,0,0,0,0);
	MyAdjustWindowRectEx(&frameInfo,hwnd);
	int frameWidth = frameInfo.right-frameInfo.left;
	int frameHeight = frameInfo.bottom-frameInfo.top + tbheight;

	if((keepRatio & FULLSCREEN))
		if(fsWindow)
			frameWidth = frameHeight = 0;

	// Calculate the minimum size in pixels
	_minWidth = adjr.right-adjr.left;
	_minHeight = adjr.bottom-adjr.top + tbheight;

	//zero 07-aug-2016 - this was really old code. Maybe it isn't needed anymore
	//it effectively let the content overshoot the window size.
	//really we simply should let the window not overshoot the content size (but that restriction is waived for fullscreen mode)
	//SIDE EFFECT: window can now be shrunk under the minimum size, which people have been wanting to do anyway
	////Clamp the size to the minimum size (256x384)
	//rect->right = (rect->left + std::max(_minWidth, (int)(rect->right - rect->left)));
	//rect->bottom = (rect->top + std::max(_minHeight, (int)(rect->bottom - rect->top)));

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
		int titleBarHeight = DPI_GetSystemMetricsForWindow(SM_CYSIZE, hwnd);
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
	isOpen = false;
	memset(class_name, 0, sizeof(class_name));
	memset(class_name2, 0, sizeof(class_name2));
}

TOOLSCLASS::~TOOLSCLASS()
{
	close();
}

void TOOLSCLASS::Activate()
{
	SetForegroundWindow(hwnd);
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
	isOpen = true;

	return 0;
}

void TOOLSCLASS::doClose()
{
	unregClass();
	hwnd = NULL;
	isOpen = false;
}

DWORD TOOLSCLASS::ThreadFunc()
{
	LOG("Start thread\n");


	DWORD ret = doOpen();
	if(ret) return ret;
	
	MSG messages;
	while (GetMessage (&messages, NULL, 0, 0))
	{
		//we should not be using a thread here, because it takes too many programmer resources to do anything asynchronously.
		//but since someone decided to... better put a lock here so that we cant accidentally emulate with NDS_exec while stuff is going on
		Lock lock; //KLUDGE
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
