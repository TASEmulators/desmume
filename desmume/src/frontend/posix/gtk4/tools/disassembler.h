/*
	disassembler.h - ARM disassembler tool for the DeSmuME GTK4 frontend

	Copyright (C) 2006-2026 DeSmuME team

	This file is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 2 of the License, or
	(at your option) any later version.

	This file is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.
*/

#ifndef DESMUME_GTK4_TOOLS_DISASSEMBLER_H_
#define DESMUME_GTK4_TOOLS_DISASSEMBLER_H_

#include "../tools.h"

G_BEGIN_DECLS

/* Disassembler window.  Decoding, address navigation, the ARM/Thumb/auto
 * rules, register snapshots and the breakpoint list live in
 * shared/disassembly_core.cpp, so this is UI only, exactly like the GTK3,
 * Windows and Cocoa hosts of the same core. */
extern const DesmumeTool desmume_tool_disassembler;

G_END_DECLS

#endif /* DESMUME_GTK4_TOOLS_DISASSEMBLER_H_ */
