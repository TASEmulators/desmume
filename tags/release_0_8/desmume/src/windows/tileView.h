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

#ifndef TILEVIEW_H
#define TILEVIEW_H

#include "CWindow.h"

typedef struct
{
   HWND hwnd;
   BOOL autoup;      
   void *prev; 
   void *next;
   void *first;
   void (*Refresh)(void *win);

   u8 * mem;
   u16 * pal;
   s16 palnum;
   u16 tilenum;
   u8 coul;
   u32 x;
   u32 y;
} tileview_struct;


void InitTileViewBox();
tileview_struct *TileView_Init(HINSTANCE hInst, HWND parent);
void TileView_Deinit(tileview_struct *TileView);

#endif
