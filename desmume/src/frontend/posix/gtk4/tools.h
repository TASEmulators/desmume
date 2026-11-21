/*
	tools.h - tool window registry for the DeSmuME GTK4 frontend

	Copyright (C) 2026 DeSmuME team

	This file is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 2 of the License, or
	(at your option) any later version.
*/

#ifndef DESMUME_GTK4_TOOLS_H_
#define DESMUME_GTK4_TOOLS_H_

#include <gtk/gtk.h>

G_BEGIN_DECLS

/* A tool follows the GTK3 dTool open/update/close contract.  GTK4 adds an
 * is_open query so the framework can keep the menu action state in sync. */
typedef void (*DesmumeToolOpen)(void);
typedef void (*DesmumeToolUpdate)(void);
typedef void (*DesmumeToolClose)(void);
typedef gboolean (*DesmumeToolIsOpen)(void);

typedef struct
{
	const char *name;        /* short name, also the GAction suffix */
	const char *label;       /* human-readable name for the menu */
	DesmumeToolOpen open;
	DesmumeToolUpdate update;
	DesmumeToolClose close;
	DesmumeToolIsOpen is_open;
} DesmumeTool;

/* Total number of registered tools and lookup by index. */
guint desmume_tools_count(void);
const DesmumeTool *desmume_tool_at(guint index);

/* The main window is registered once at startup so every tool window can be
 * transient for it without the tool files needing a global from main.cpp. */
void desmume_tools_set_parent(GtkWidget *window);
GtkWidget *desmume_tools_get_parent(void);

/* Open a tool by index.  Repeated calls are ignored while it is open. */
void desmume_tools_open(guint index);

/* Refresh every open tool.  Called once per emulated frame; each tool is
 * responsible for skipping work when its window is not mapped. */
void desmume_tools_update(void);

/* Close all open tools (called during application shutdown). */
void desmume_tools_close_all(void);

/* Shared helper: create a decorated (SSD) tool window transient for the main
 * window.  The caller owns the returned window and connects its own
 * close-request handler. */
GtkWidget *desmume_tool_window_new(const char *title, GtkWidget *parent,
                                   int default_width, int default_height);

G_END_DECLS

/* Host accessors implemented in main.cpp, kept outside the extern "C" block
 * because they exchange a C++ config type. */
namespace desmume { namespace config { class Config; } }
desmume::config::Config *desmume_host_config(void);
const char *desmume_host_rom_base_name(void);

#endif /* DESMUME_GTK4_TOOLS_H_ */
