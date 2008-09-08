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
#include "resource.h"

CRITICAL_SECTION section;
cwindow_struct *updatewindowlist = NULL;

//////////////////////////////////////////////////////////////////////////////

int CWindow_Init(void *win, HINSTANCE hInst, const char * cname, const char * title, int style, int sx, int sy, WNDPROC wP)
{
    static BOOL first = FALSE;
    RECT clientaera;
    cwindow_struct *win2=(cwindow_struct *)win;

    win2->autoup = FALSE;
    
    if(!first)
    {
    WNDCLASSEX wincl;        // Data structure for the windowclass
    
    // The Window structure
    wincl.hInstance = hInst;
    wincl.lpszClassName = cname;
    wincl.lpfnWndProc = wP;      // This function is called by windows
    wincl.style = CS_DBLCLKS;                 // Catch double-clicks
    wincl.cbSize = sizeof (WNDCLASSEX);

    // Use default icon and mouse-pointer
    //wincl.hIcon = LoadIcon (hInst, MAKEINTRESOURCE(IconDeSmuME));//IDI_APPLICATION);
    //wincl.hIconSm = LoadIcon (hInst, MAKEINTRESOURCE(IconDeSmuME));//IDI_APPLICATION);
	wincl.hIcon = LoadIcon (hInst, IDI_APPLICATION);
    wincl.hIconSm = LoadIcon (hInst, IDI_APPLICATION);
    wincl.hCursor = LoadCursor (NULL, IDC_ARROW);
    wincl.lpszMenuName = NULL;                 // No menu
    wincl.cbClsExtra = 0;                      // No extra bytes after the window class
    wincl.cbWndExtra = 0;                      // structure or the window instance
    // Use Windows's default color as the background of the window
    wincl.hbrBackground = (HBRUSH) COLOR_BACKGROUND;

    // Register the window class, and if it fails quit the program
    if (!RegisterClassEx (&wincl))
        return -1;
    win2->first=NULL;
    first = TRUE;
    }
    
    clientaera.left = 0;
    clientaera.top = 0;
    clientaera.right = sx;
    clientaera.bottom = sy;

    AdjustWindowRectEx(&clientaera, style, TRUE, 0);
    
    // The class is registered, let's create the program
    win2->hwnd = CreateWindowEx (
           0,                   // Extended possibilites for variation
           cname,         // Classname
           title,       // Title Text
           style, // default window
           CW_USEDEFAULT,       // Windows decides the position
           CW_USEDEFAULT,       // where the window ends up on the screen
           clientaera.right - clientaera.left,                 // The programs width
           clientaera.bottom - clientaera.top,                 // and height in pixels
           HWND_DESKTOP,        // The window is a child-window to desktop
           NULL,                // No menu
           hInst,       // Program Instance handler
           NULL                 // No Window Creation data
           );
           
    win2->prev = NULL;
    win2->next = NULL;
    win2->Refresh = &CWindow_Refresh;

    return 0;
}

//////////////////////////////////////////////////////////////////////////////

int CWindow_Init2(void *win, HINSTANCE hInst, HWND parent, char * title, int ID, DLGPROC wP)
{
	HMENU hSystemMenu;

    cwindow_struct *win2=(cwindow_struct *)win;

    win2->autoup = FALSE;
    win2->hwnd = CreateDialog(hInst, MAKEINTRESOURCE(ID), parent, wP);
    SetWindowLong(win2->hwnd, DWL_USER, (LONG)win2);
    SetWindowText(win2->hwnd, title);
    win2->prev = NULL;
    win2->next = NULL;
    win2->Refresh = &CWindow_Refresh;

	// Append the "Auto Update" to the System Menu
	hSystemMenu = GetSystemMenu(win2->hwnd, FALSE);
	if(hSystemMenu != 0)
	{
		AppendMenu(hSystemMenu, MF_MENUBREAK, 0, NULL);
		AppendMenu(hSystemMenu, MF_ENABLED|MF_STRING, IDC_AUTO_UPDATE, "Auto Update");
	}

    return 0;
}

//////////////////////////////////////////////////////////////////////////////

void CWindow_Show(void *win)
{
    ShowWindow (((cwindow_struct *)win)->hwnd, SW_SHOW);
}

//////////////////////////////////////////////////////////////////////////////

void CWindow_Hide(void *win)
{
    ShowWindow (((cwindow_struct *)win)->hwnd, SW_HIDE);
}

//////////////////////////////////////////////////////////////////////////////

void CWindow_Refresh(void *win)
{
    InvalidateRect(((cwindow_struct *)win)->hwnd, NULL, FALSE);
}

//////////////////////////////////////////////////////////////////////////////

void CWindow_UpdateAutoUpdateItem(cwindow_struct *win, int check)
{
	HMENU hSystemMenu;
	
	// Update the "auto update" menu item
	hSystemMenu = GetSystemMenu(win->hwnd, FALSE);
	if(hSystemMenu != 0)
	{
		const int checkState[] =
		{
			MF_UNCHECKED,
			MF_CHECKED
		};

		CheckMenuItem(hSystemMenu, IDC_AUTO_UPDATE, checkState[check]);
	}
}

//////////////////////////////////////////////////////////////////////////////

void CWindow_AddToRefreshList(void *win)
{
    cwindow_struct *win2=(cwindow_struct *)win;

    EnterCriticalSection(&section);
    win2->prev = NULL;
    win2->next = updatewindowlist;
    if(updatewindowlist)
       updatewindowlist->prev = win;
    updatewindowlist = (cwindow_struct *)win;
    LeaveCriticalSection(&section);

	CWindow_UpdateAutoUpdateItem(win, TRUE);
}

//////////////////////////////////////////////////////////////////////////////

void CWindow_RemoveFromRefreshList(void *win)
{
    cwindow_struct *win2=(cwindow_struct *)win;

    EnterCriticalSection(&section);
    if(updatewindowlist == win)
    {
       updatewindowlist = (cwindow_struct *)win2->next;
       if(updatewindowlist) updatewindowlist->prev = NULL;
    }
    else
    if(win2->prev)
    {
       ((cwindow_struct *)win2->prev)->next = win2->next;
       if(win2->next) ((cwindow_struct *)win2->next)->prev = win2->prev;
    }
    win2->next = NULL;
    win2->prev = NULL;
    LeaveCriticalSection(&section);

	CWindow_UpdateAutoUpdateItem(win, FALSE);
}

//////////////////////////////////////////////////////////////////////////////

int CWindow_ToggleAutoUpdate(void *win)
{
    cwindow_struct *win2=(cwindow_struct *)win;

	// remove window from refresh list
	if(win2->autoup)
		CWindow_RemoveFromRefreshList(win);

	// toggle autoup variable
	win2->autoup = !win2->autoup;

	// add window to refresg list if autoupdate is desired
	if(win2->autoup)
		CWindow_AddToRefreshList(win);

	// checks or unchecks the auto update item in the system menu
	CWindow_UpdateAutoUpdateItem(win, win2->autoup);

	return win2->autoup;
}

