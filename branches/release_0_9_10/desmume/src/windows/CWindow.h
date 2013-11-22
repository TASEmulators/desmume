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

#ifndef CWINDOW_H
#define CWINDOW_H

#include "../common.h"

#include <commctrl.h>
#include <string>
#include <vector>
#include <map>
#include <algorithm>

using namespace std;

extern CRITICAL_SECTION win_execute_sync;

//-----------------------------------------------------------------------------
//   The Toolkit - RECT wrapper
//-----------------------------------------------------------------------------

class CRect
{
public:
	CRect(int x, int y, int width, int height)
	{
		rcRect.left = x; rcRect.top = y;
		rcRect.right = x + width;
		rcRect.bottom = y + height;
	}
	CRect(RECT rc)
	{
		memcpy(&rcRect, &rc, sizeof(RECT));
		//rcRect = rc;
	}

	~CRect() {}

	RECT ToMSRect() { return rcRect; }

private:
	RECT rcRect;
};

// GetFontQuality()
// Returns a font quality value that can be passed to 
// CreateFont(). The value depends on whether font 
// antialiasing is enabled or not.
DWORD GetFontQuality();

int DrawText(HDC hDC, char* text, int X, int Y, int Width, int Height, UINT format);
void GetFontSize(HWND hWnd, HFONT hFont, LPSIZE size);

// MakeBitmapPseudoTransparent(HBITMAP hBmp, COLORREF cKeyColor, COLORREF cNewKeyColor)
// Replaces the RGB color cKeyColor with cNewKeyColor in the bitmap hBmp.
// For use with toolbars and such. Replace a key color (like magenta) with the right
// system color to make the bitmap pseudo-transparent.
void MakeBitmapPseudoTransparent(HBITMAP hBmp, COLORREF cKeyColor, COLORREF cNewKeyColor);

//-----------------------------------------------------------------------------
// Window class handling
//-----------------------------------------------------------------------------

// RegWndClass()
// Registers a window class.
// Incase the class was already registered, the function
// just does nothing and returns true.
// Returns false if registration failed.
bool RegWndClass(string name, WNDPROC wndProc, UINT style, int extraSize = 0);
bool RegWndClass(string name, WNDPROC wndProc, UINT style, HICON icon, int extraSize = 0);

// UnregWndClass()
// Unregisters a previously registered window class.
// This function will silently fail if one or more windows
// using the class still exist.
void UnregWndClass(string name);

//-----------------------------------------------------------------------------
// Base toolwindow class
//-----------------------------------------------------------------------------

class CToolWindow
{
public:
	// CToolWindow constructor #1
	// Creates a window using CreateWindow().
	// If the window creation failed for whatever reason,
	// hWnd will be NULL.
	CToolWindow(char* className, WNDPROC proc, char* title, int width, int height);

	// CToolWindow constructor #2
	// Creates a window from a dialog template resource.
	// If the window creation failed for whatever reason,
	// hWnd will be NULL.
	CToolWindow(int ID, DLGPROC proc, char* title);

	// CToolWindow destructor
	// Dummy destructor. The derivated toolwindow classes must 
	// destroy the window in their own destructors. Thus, they
	// can unregister any window classes they use.
	virtual ~CToolWindow();

	// this must be called by the derived class constructor. sigh.
	void PostInitialize();

	// Show(), Hide()
	// These ones are quite self-explanatory, I guess.
	void Show() { ShowWindow(hWnd, SW_SHOW); }
	void Hide() { ShowWindow(hWnd, SW_HIDE); }

	// SetTitle()
	// Changes the title of the window.
	void SetTitle(char* title) { SetWindowText(hWnd, title); }

	// Refresh()
	// Refreshes the window. Called by RefreshAllToolWindows().
	void Refresh() { InvalidateRect(hWnd, NULL, FALSE); }

	// SetFocus
	void SetFocus() { ::SetFocus(hWnd); }

	// Double-linked toolwindow list.
	CToolWindow* prev;
	CToolWindow* next;

	// Handle to the window.
	HWND hWnd;

private:
	int ID;
	DLGPROC proc;
	std::string title;
	char* className;
	int width, height;
	int whichInit;
};

//-----------------------------------------------------------------------------
// Toolwindow handling
//-----------------------------------------------------------------------------

// OpenToolWindow()
// Adds the CToolWindow instance to the toolwindow list.
// The instance will be deleted if its hWnd member is NULL.
bool OpenToolWindow(CToolWindow* wnd);

// CloseToolWindow()
// Removes the CToolWindow instance from the toolwindow list
// and deletes it.
void CloseToolWindow(CToolWindow* wnd);

// CloseAllToolWindows()
// Deletes all the toolwindows in the list and flushes the list.
void CloseAllToolWindows();

// RefreshAllToolWindows()
// Refreshes all the toolwindows in the list.
// Called once per frame when the emu is running.
void RefreshAllToolWindows();

//-----------------------------------------------------------------------------
//   The Toolkit - Toolbar API wrapper
//-----------------------------------------------------------------------------

class CToolBar
{
public:
	CToolBar(HWND hParent);
	~CToolBar();

	HWND GetHWnd() { return hWnd; }

	void Show(bool bShow);
	bool Visible() { return !hidden; }

	void OnSize();

	void AppendButton(int uID, int uBitmapID, DWORD dwState, bool bDropdown);
	void AppendSeparator();

	void EnableButton(int uID, bool bEnable) {
		SendMessage(hWnd, TB_ENABLEBUTTON, uID, bEnable ? TRUE:FALSE); }
	void CheckButton(int uID, bool bCheck) {
		SendMessage(hWnd, TB_CHECKBUTTON, uID, bCheck ? TRUE:FALSE); }

	void ChangeButtonBitmap(int uID, int uBitmapID);
	void EnableButtonDropdown(int uID, bool bDropdown);
	void ChangeButtonID(int uIndex, int uNewID) {
		SendMessage(hWnd, TB_SETCMDID, uIndex, MAKELPARAM(uNewID, 0)); }

	int GetHeight();

private:
	HWND hWnd;
	// We have to keep the bitmaps here because destroying them
	// directly after use would also destroy the toolbar.
	// They'll be destroyed when the CToolBar destructor is called.
	typedef pair<int, HBITMAP> TBitmapPair;
	typedef map<int, TBitmapPair> TBitmapList;
	TBitmapList hBitmaps;

	bool hidden;
};


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
	bool addMenuItem(u32 item, bool byPos, LPCMENUITEMINFO info);
	DWORD checkMenu(UINT idd, bool check);

	void Show(int mode);
	void Hide();

	HWND getHWnd();

	CRect GetRect()
	{
		RECT rc; GetWindowRect(hwnd, &rc);
		return CRect(rc);
	}

	void setMinSize(int width, int height);

	enum // keepRatio flags
	{
		NOKEEP = 0x0,
		KEEPX  = 0x1,
		KEEPY  = 0x2,
		FULLSCREEN = 0x4,
	};

	void sizingMsg(WPARAM wParam, LPARAM lParam, LONG keepRatio = NOKEEP);
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

	DWORD doOpen();
	void doClose();

protected:
	DWORD	ThreadFunc();

public:
	TOOLSCLASS(HINSTANCE hInst, int IDD, DLGPROC wndproc);
	virtual ~TOOLSCLASS();
	bool open(bool useThread=true);
	bool close();
	void regClass(LPSTR class_name, WNDPROC wproc, bool SecondReg = false);
	void unregClass();
};

#endif
