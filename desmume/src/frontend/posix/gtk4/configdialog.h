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

/* Show the modal config dialog.  A second call presents the existing window
 * instead of opening another one. */
void desmume_config_dialog_show(GtkWindow *parent,
                                desmume::config::Config *config,
                                DesmumeConfigAppliedFunc applied,
                                gpointer user_data);

#endif /* DESMUME_GTK4_CONFIGDIALOG_H_ */
