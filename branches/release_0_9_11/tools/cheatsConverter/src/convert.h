/* Cheats converter

	Copyright 2009 DeSmuME team

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
    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301  USA
*/

#ifndef __CONVERT_H__
#define __CONVERT_H__

#include "common.h"

#define CHEAT_VERSION_MAJOR		2
#define CHEAT_VERSION_MINOR		0
#define MAX_CHEAT_LIST			200
#define	MAX_XX_CODE				512

typedef struct
{
	u8		type;				// 0 - internal cheat system
								// 1 - Action Replay
								// 2 - Codebreakers
	bool	enabled;
	u32		code[MAX_XX_CODE][2];
	u32		hi[MAX_XX_CODE];
	u32		lo[MAX_XX_CODE];
	char	description[75];
	int		num;
	u8		size;

} CHEATS_LIST;

extern bool convertFile(char *fname);
#endif