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

#ifndef DISVIEW_H
#define DISVIEW_H

#include "../NDSSystem.h"
#include "../armcpu.h"

#include "CWindow.h"

typedef struct
{
   HWND hwnd;
   BOOL autoup;      
   void *prev; 
   void *next;
   void *first;
   void (*Refresh)(void *win);

   u32 curr_ligne;
   armcpu_t *cpu;
   u16 mode;
} disview_struct;

void InitDesViewBox();
disview_struct *DisView_Init(HINSTANCE hInst, HWND parent, char *title, armcpu_t *CPU);
void DisView_Deinit(disview_struct *DisView);
void DisView_Refresh(disview_struct *DisView);

#endif
