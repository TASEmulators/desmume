/*  Copyright (C) 2006 yopyop
    yopyop156@ifrance.com
    yopyop156.ifrance.com

    Copyright (C) 2006-2008 DeSmuME team

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

#ifndef _COMMON_H_
#define _COMMON_H_

#include <stdio.h>
	
#ifdef WIN32
#include <windows.h>

#define CLASSNAME "DeSmuME"
#define TITLE "DeSmuME v"VERSION

extern HINSTANCE hAppInst;
//char		vPath[MAX_PATH], *szPath, currDir[MAX_PATH];

extern char IniName[MAX_PATH];
extern void GetINIPath();
#endif
	
#endif