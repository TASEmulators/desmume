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

#include <windows.h>
#include "../types.h"

extern CRITICAL_SECTION section;

typedef struct
{
   HWND hwnd;
   BOOL autoup;      
   void *prev; 
   void *next;
   void *first;
   void (*Refresh)(void *win);
} cwindow_struct;

int CWindow_Init(void *win, HINSTANCE hInst, const char * cname, const char * title, int style, int sx, int sy, LRESULT CALLBACK (* wP) (HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam));
int CWindow_Init2(void *win, HINSTANCE hInst, HWND parent, char * title, int ID, BOOL CALLBACK (*wP) (HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam));
void CWindow_Show(void *win);
void CWindow_Hide(void *win);
void CWindow_Refresh(void *win);
void CWindow_AddToRefreshList(void *win);
void CWindow_RemoveFromRefreshList(void *win);

extern cwindow_struct *updatewindowlist;

static INLINE void CWindow_RefreshALL()
{
   cwindow_struct *aux;
   EnterCriticalSection(&section);
   aux = updatewindowlist;
   while(aux)
   {
      aux->Refresh(aux);
      aux = (cwindow_struct *)aux->next;
   }
   LeaveCriticalSection(&section);
}

#endif
