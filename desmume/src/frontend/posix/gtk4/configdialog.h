/*
	configdialog.h - unified Emulation/Graphic/Sound settings for GTK4

	Copyright (C) 2026 DeSmuME team

	This file is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 2 of the License, or
	(at your option) any later version.
*/

#ifndef DESMUME_GTK4_CONFIGDIALOG_H_
#define DESMUME_GTK4_CONFIGDIALOG_H_

#include <gtk/gtk.h>

#include "config.h"

/* Called after OK has applied the settings and saved the config, so the main
 * window can refresh action states and the status bar. */
typedef void (*DesmumeConfigAppliedFunc)(gpointer user_data);

/* Push every persisted setting into the core.  Called once at startup, after
 * config.load(), and again by the dialog after it has written the widget
 * values back into `config`.  Everything the core only reads while it boots
 * (bus timing, BIOS/firmware images, firmware user data, slot devices,
 * Wi-Fi) is covered, plus the Path Settings folders. */
void desmume_config_apply(desmume::config::Config *config);

/* Folder the Open ROM dialog starts in (Path Settings > ROM files, else the
 * home folder).  Owned string. */
char *desmume_config_rom_directory(desmume::config::Config *config);

/* Remember the folder of a ROM that was just opened, when Path Settings has
 * "Remember the folder of the last opened ROM" enabled. */
void desmume_config_note_rom_dir(desmume::config::Config *config,
                                 const char *rom_path);

/* Destination for the next screenshot: the Screenshots folder, the image
 * format and the name pattern from Path Settings.  Returns an owned path. */
char *desmume_config_screenshot_path(desmume::config::Config *config,
                                     const char *rom_base_name);

/* Page indexes of the config notebook.  The order is the one the dialog
 * builds, so a menu shortcut can jump straight to the page GTK3 used to
 * have its own dialog for. */
enum DesmumeConfigPage
{
	DESMUME_CONFIG_PAGE_EMULATION = 0,
	DESMUME_CONFIG_PAGE_GRAPHIC,
	DESMUME_CONFIG_PAGE_SOUND,
	DESMUME_CONFIG_PAGE_FIRMWARE,
	DESMUME_CONFIG_PAGE_SLOTS,
	DESMUME_CONFIG_PAGE_WIFI,
	DESMUME_CONFIG_PAGE_MICROPHONE,
	DESMUME_CONFIG_PAGE_PATHS
};

/* Show the modal config dialog.  A second call presents the existing window
 * (switching to `page` when it is not -1) instead of opening another one. */
void desmume_config_dialog_show_page(GtkWindow *parent,
                                     desmume::config::Config *config,
                                     DesmumeConfigAppliedFunc applied,
                                     gpointer user_data,
                                     int page);

/* Show the modal config dialog on its first page.  A second call presents the
 * existing window instead of opening another one. */
void desmume_config_dialog_show(GtkWindow *parent,
                                desmume::config::Config *config,
                                DesmumeConfigAppliedFunc applied,
                                gpointer user_data);

#endif /* DESMUME_GTK4_CONFIGDIALOG_H_ */
