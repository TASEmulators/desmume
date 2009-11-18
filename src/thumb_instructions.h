/*  Copyright (C) 2006 yopyop
	Copyright (C) 2009 DeSmuME team

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

#ifndef THUMB_INSTRUCTIONS_H
#define THUMB_INSTRUCTIONS_H

#include "armcpu.h"

typedef u32 (FASTCALL* ThumbOpFunc)(const u32 i);

extern const ThumbOpFunc thumb_instructions_set_0[1024];
extern const ThumbOpFunc thumb_instructions_set_1[1024];

extern const char* thumb_instruction_names[1024];

#endif
 
