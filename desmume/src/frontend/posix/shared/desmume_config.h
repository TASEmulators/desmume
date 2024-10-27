/*
	Copyright (C) 2009-2010 DeSmuME team

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

#ifndef _DESMUME_GTK_CONFIG
#define _DESMUME_GTK_CONFIG

GKeyFile *desmume_config_read_file(const u32 *, const char *keysection = "KEYS");
void desmume_config_dispose(GKeyFile *);

/* since GTK uses GDK keysymbols, not SDL2 ones, we need
  different sections for cli/gtk frontends.
  KEYS = gtk keys,
  SDLKEYS = sdl2 keys
*/
gboolean desmume_config_update_keys(GKeyFile*, const char *section = "KEYS");
gboolean desmume_config_read_keys(GKeyFile*, const char *section = "KEYS");

gboolean desmume_config_update_joykeys(GKeyFile*);
gboolean desmume_config_read_joykeys(GKeyFile*);

#endif
