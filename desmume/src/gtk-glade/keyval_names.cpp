/* keyval_names.c - this file is part of DeSmuME
 *
 * Copyright (C) 2007 Damien Nozay (damdoum)
 * Author: damdoum at users.sourceforge.net
 *
 * This file is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * This file is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; see the file COPYING.  If not, write to
 * the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include "keyval_names.h"
/* see <gdk/gdkkeysyms.h> */

char * unknown="(unknown)";
const char * KEYVAL_NAMES[0x10000];

const char * KEYNAME(int k) {
	const char * s = unknown;
	s = KEYVAL_NAMES[k & 0xFFFF];
	return s;
}

void KEYVAL(int k, const char * name) {
	if (KEYVAL_NAMES[k] == unknown)
	KEYVAL_NAMES[k] = name;
}
