/*
	hotkeys.h - rebindable menu shortcuts for the GTK4 frontend

	Copyright (C) 2026 DeSmuME team

	This file is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 2 of the License, or
	(at your option) any later version.

	This file is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.
*/

#ifndef DESMUME_GTK4_HOTKEYS_H_
#define DESMUME_GTK4_HOTKEYS_H_

#include <gtk/gtk.h>

G_BEGIN_DECLS

/* One rebindable action.  `action` is a detailed action name, or NULL for a
 * shortcut the frontend handles itself (currently Fast Forward (Hold)). */
typedef struct
{
	const char *slug;      /* [HOTKEYS] key, shared with GTK3 */
	const char *label;     /* row label in the settings dialog */
	const char *action;    /* detailed action name, or NULL */
	const char *def_accel; /* NULL when the action starts unbound */
} DesmumeHotkeyDef;

int desmume_hotkey_count(void);
const DesmumeHotkeyDef *desmume_hotkey_def(int index);

/* Accelerator string currently assigned to a hotkey.  An empty string means
 * "unbound"; the returned pointer belongs to the hotkey table. */
const char *desmume_hotkey_accel(int index);
void desmume_hotkey_set_accel(int index, const char *accel);

/* Read the [HOTKEYS] group (falling back to the built-in defaults) and write
 * it back.  Loading only fills the strings; call apply() to install them. */
void desmume_hotkeys_load(GKeyFile *keyfile);
void desmume_hotkeys_save(GKeyFile *keyfile);

/* Install every accelerator on the application and refresh the special
 * Fast Forward (Hold) key state.  Safe to call after the dialog changes a
 * binding. */
void desmume_hotkeys_apply(GtkApplication *app);

G_END_DECLS

#endif /* DESMUME_GTK4_HOTKEYS_H_ */
