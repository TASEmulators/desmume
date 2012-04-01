/*
	Copyright (C) 2006 yopyop
	Copyright (C) 2006-2009 DeSmuME team

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

#ifndef DISASSEMBLER_H
#define DISASSEMBLER_H

#include "types.h"

typedef char* (* DisasmOpFunc)(u32 adr, u32 i, char * txt);

extern const DisasmOpFunc des_arm_instructions_set[4096];
extern const DisasmOpFunc des_thumb_instructions_set[1024];

#endif

 
