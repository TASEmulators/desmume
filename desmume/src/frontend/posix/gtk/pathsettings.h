/*
	pathsettings.h - Path Settings dialog for the GTK3 frontend

	Copyright (C) 2026 DeSmuME team

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

#ifndef DESMUME_GTK_PATHSETTINGS_H_
#define DESMUME_GTK_PATHSETTINGS_H_

#include <string>

#include <gtk/gtk.h>

#include "path.h"

/* Shows the modal Path Settings dialog.  A second call reuses the window the
 * same way the other Config dialogs do. */
void path_settings_dialog_show(GtkWindow *parent);

/* Pushes the folders from config into the shared path model.  Must run after
 * config.load() and before the first ROM is opened. */
void path_settings_apply_config();

/* Folder configured for a category, or an empty string when the built-in
 * default applies.  Callers seed their file choosers with it. */
std::string path_settings_directory(PathInfo::KnownPath category);

/* Screenshot settings, resolved so they are never empty. */
std::string path_settings_screenshot_directory();
std::string path_settings_screenshot_image_format();
std::string path_settings_screenshot_extension();

/* File name (without directory and extension) for the next screenshot.
 * `sequence` keeps the legacy desmume-screenshot-N naming going. */
std::string path_settings_screenshot_name(const std::string &romBaseName,
                                          unsigned int sequence);

#endif /* DESMUME_GTK_PATHSETTINGS_H_ */
