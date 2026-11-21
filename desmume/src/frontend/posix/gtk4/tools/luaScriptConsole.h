/*
	luaScriptConsole.h - Lua script console for the DeSmuME GTK4 frontend

	Copyright (C) 2026 DeSmuME team
*/

#ifndef DESMUME_GTK4_LUA_SCRIPT_CONSOLE_H_
#define DESMUME_GTK4_LUA_SCRIPT_CONSOLE_H_

#include <gtk/gtk.h>

G_BEGIN_DECLS

void lua_script_open_console(GtkWindow *parent);
void lua_script_open_console_with_file(GtkWindow *parent, const char *filename);
void lua_script_close_all(void);
void lua_script_show_about(GtkWindow *parent);

G_END_DECLS

#endif /* DESMUME_GTK4_LUA_SCRIPT_CONSOLE_H_ */
