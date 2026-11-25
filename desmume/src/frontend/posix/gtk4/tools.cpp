/*
	tools.cpp - tool window registry for the DeSmuME GTK4 frontend

	Copyright (C) 2026 DeSmuME team
*/

#include "tools.h"
#include "tools/ioregs.h"
#include "tools/ramwatch.h"
#include "tools/ramsearch.h"
#include "tools/cheats.h"
#include "tools/memoryviewer.h"
#include "tools/disassembler.h"

/* The tool table mirrors GTK3's dTools_list order, so the numeric targets in
 * the Tools menu stay aligned with the old front end. */
static const DesmumeTool *sTools[] =
{
	&desmume_tool_ioregs,
	&desmume_tool_ramwatch,
	&desmume_tool_ramsearch,
	&desmume_tool_cheats,
	&desmume_tool_memoryviewer,
	&desmume_tool_disassembler,
};

static GtkWidget *sParentWindow = NULL;

guint
desmume_tools_count(void)
{
	return G_N_ELEMENTS(sTools);
}

const DesmumeTool *
desmume_tool_at(guint index)
{
	if (index >= G_N_ELEMENTS(sTools))
	{
		return NULL;
	}
	return sTools[index];
}

void
desmume_tools_set_parent(GtkWidget *window)
{
	sParentWindow = window;
}

GtkWidget *
desmume_tools_get_parent(void)
{
	return sParentWindow;
}

void
desmume_tools_open(guint index)
{
	const DesmumeTool *tool = desmume_tool_at(index);
	if ((tool == NULL) || (tool->open == NULL))
	{
		return;
	}
	if ((tool->is_open != NULL) && tool->is_open())
	{
		return;
	}
	tool->open();
}

void
desmume_tools_update(void)
{
	for (guint i = 0; i < G_N_ELEMENTS(sTools); ++i)
	{
		const DesmumeTool *tool = sTools[i];
		if ((tool->update != NULL) &&
		    ((tool->is_open == NULL) || tool->is_open()))
		{
			tool->update();
		}
	}
}

void
desmume_tools_close_all(void)
{
	for (guint i = 0; i < G_N_ELEMENTS(sTools); ++i)
	{
		const DesmumeTool *tool = sTools[i];
		if ((tool->close != NULL) &&
		    ((tool->is_open == NULL) || tool->is_open()))
		{
			tool->close();
		}
	}
}

GtkWidget *
desmume_tool_window_new(const char *title, GtkWidget *parent,
                        int default_width, int default_height)
{
	/* No custom titlebar: like the main window this keeps the SSD path. */
	GtkWidget *window = gtk_window_new();
	gtk_window_set_title(GTK_WINDOW(window), title);
	gtk_window_set_default_size(GTK_WINDOW(window), default_width, default_height);
	if ((parent == NULL) || !GTK_IS_WINDOW(parent))
	{
		parent = desmume_tools_get_parent();
	}
	if ((parent != NULL) && GTK_IS_WINDOW(parent))
	{
		gtk_window_set_transient_for(GTK_WINDOW(window), GTK_WINDOW(parent));
	}
	return window;
}
