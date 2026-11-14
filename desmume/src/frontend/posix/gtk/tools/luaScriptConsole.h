/* luaScriptConsole.h - this file is part of DeSmuME
 *
 * Copyright (C) 2006-2025 DeSmuME Team
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

#ifndef __LUASCRIPTCONSOLE_H__
#define __LUASCRIPTCONSOLE_H__

#ifdef HAVE_LUA

#include <gtk/gtk.h>

void lua_script_open_console(GtkWindow *parent);
// Opens a console with `filename` already selected and starts it, mirroring
// the Windows emu.openscript() behaviour (new window per script).
void lua_script_open_console_with_file(GtkWindow *parent, const char *filename);
void lua_script_close_all();
void lua_script_show_about(GtkWindow *parent);

#endif /* HAVE_LUA */

#endif /* __LUASCRIPTCONSOLE_H__ */
