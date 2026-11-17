/* ramWatch.cpp - this file is part of DeSmuME
 *
 * Copyright (C) 2006-2026 DeSmuME Team
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

// GTK3 host for RAM Watch.
//
// Everything that decides *what* a watch list is lives in
// ../../../shared/ramwatch_core.cpp, which the Windows and Cocoa frontends
// use as well: document state, .wch serialisation, recent files, value
// formatting and the 256-entry / duplicate rules. This file only builds the
// window, forwards button presses to the core and redraws.
//
// Preferences follow the same split as the other frontends: the core holds the
// values (Auto-load, Save Window Position, recent files) and the host persists
// them. On GTK that host is the GKeyFile owned by config.cpp.

#include <gtk/gtk.h>

#include <stdio.h>
#include <string.h>

#include <string>
#include <vector>

#include "../dTool.h"
#include "../config.h"
#include "../../../shared/ram_debug_core.h"
#include "../../../shared/ramwatch_core.h"

// The frontend's single config store, defined in main.cpp.
extern desmume::config::Config config;

#define SHORTNAME "ramwatch"
#define TOOL_NAME "RAM Watch"

enum
{
	COL_ADDRESS = 0,
	COL_VALUE,
	COL_NOTES,
	COL_FOREGROUND,
	COLUMN_COUNT
};

static const char kSizeLabels[3][12] = { "1 Byte", "2 Bytes", "4 Bytes" };
static const char kTypeLabels[4][16] = { "Signed", "Unsigned", "Hex", "20.12 Fixed" };
static const char kSizeCodes[3] = { 'b', 'w', 'd' };
static const char kTypeCodes[4] = { 's', 'u', 'h', 'f' };

// Separator rows are drawn in the secondary label colour, like the other
// frontends do.
static const char kSeparatorColor[] = "#808080";

static const int kDefaultWidth = 640;
static const int kDefaultHeight = 460;
static const int kMinimumWidth = 520;
static const int kMinimumHeight = 360;

static GtkWidget *sWindow = NULL;
static GtkWidget *sScrolledWindow = NULL;
static GtkWidget *sTreeView = NULL;
static GtkListStore *sStore = NULL;
static GtkWidget *sStatusLabel = NULL;
static GtkWidget *sFileButton = NULL;
static GtkWidget *sFileMenu = NULL;

static GtkWidget *sNewButton = NULL;
static GtkWidget *sEditButton = NULL;
static GtkWidget *sRemoveButton = NULL;
static GtkWidget *sDuplicateButton = NULL;
static GtkWidget *sSeparatorButton = NULL;
static GtkWidget *sMoveUpButton = NULL;
static GtkWidget *sMoveDownButton = NULL;

static int sToolID = -1;
static gboolean sDidAttemptAutoLoad = FALSE;

// Number of rows last pushed into the model. RAM Search inserts watches
// directly into the shared core, so every frame compares against this to pick
// up changes that did not originate here.
static guint sModelRowCount = 0;

/////////////////////////////// PREFERENCES ///////////////////////////////

static void persist_preferences(void)
{
	RAMWatchCore &core = RAMWatchCore::Get();

	config.ramwatch_autoload = core.AutoLoad();
	config.ramwatch_save_window_pos = core.SaveWindowPos();
	config.ramwatch_recent_files = core.RecentFiles();
	config.save();
}

static void load_preferences(void)
{
	RAMWatchCore &core = RAMWatchCore::Get();

	core.SetAutoLoad(config.ramwatch_autoload);
	core.SetSaveWindowPos(config.ramwatch_save_window_pos);

	const std::vector<std::string> recent = config.ramwatch_recent_files;
	core.SetRecentFiles(recent);
}

static void restore_geometry(void)
{
	if (!RAMWatchCore::Get().SaveWindowPos())
	{
		gtk_window_set_default_size(GTK_WINDOW(sWindow), kDefaultWidth, kDefaultHeight);
		return;
	}

	int width = config.ramwatch_win_w;
	int height = config.ramwatch_win_h;
	const int x = config.ramwatch_win_x;
	const int y = config.ramwatch_win_y;

	if (width < kMinimumWidth)  width = kDefaultWidth;
	if (height < kMinimumHeight) height = kDefaultHeight;

	gtk_window_set_default_size(GTK_WINDOW(sWindow), width, height);
	if ((x >= 0) && (y >= 0))
	{
		gtk_window_move(GTK_WINDOW(sWindow), x, y);
	}
}

static void save_geometry(void)
{
	if (!RAMWatchCore::Get().SaveWindowPos())
	{
		return;
	}

	int x = 0;
	int y = 0;
	int width = 0;
	int height = 0;
	gtk_window_get_position(GTK_WINDOW(sWindow), &x, &y);
	gtk_window_get_size(GTK_WINDOW(sWindow), &width, &height);

	config.ramwatch_win_x = x;
	config.ramwatch_win_y = y;
	config.ramwatch_win_w = width;
	config.ramwatch_win_h = height;

	RAMWatchCore::Get().SetWindowPosition(x, y);
}

/////////////////////////////// SMALL HELPERS ///////////////////////////////

static void show_error(const char *message)
{
	GtkWidget *dialog = gtk_message_dialog_new(GTK_WINDOW(sWindow),
	                                           (GtkDialogFlags)(GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT),
	                                           GTK_MESSAGE_ERROR,
	                                           GTK_BUTTONS_OK,
	                                           "%s", message);
	gtk_window_set_title(GTK_WINDOW(dialog), TOOL_NAME);
	gtk_dialog_run(GTK_DIALOG(dialog));
	gtk_widget_destroy(dialog);
}

static std::string ensure_wch_extension(const char *path)
{
	std::string result(path);
	if ((result.size() < 4) ||
	    (g_ascii_strcasecmp(result.c_str() + result.size() - 4, ".wch") != 0))
	{
		result += ".wch";
	}
	return result;
}

static bool is_watch_file_name(const char *path)
{
	const size_t length = strlen(path);
	return (length > 4) && (g_ascii_strcasecmp(path + length - 4, ".wch") == 0);
}

static void add_watch_file_filters(GtkFileChooser *chooser)
{
	GtkFileFilter *watchFilter = gtk_file_filter_new();
	gtk_file_filter_set_name(watchFilter, "DeSmuME watch lists (*.wch)");
	gtk_file_filter_add_pattern(watchFilter, "*.wch");
	gtk_file_filter_add_pattern(watchFilter, "*.WCH");
	gtk_file_chooser_add_filter(chooser, watchFilter);

	GtkFileFilter *anyFilter = gtk_file_filter_new();
	gtk_file_filter_set_name(anyFilter, "All files");
	gtk_file_filter_add_pattern(anyFilter, "*");
	gtk_file_chooser_add_filter(chooser, anyFilter);
}

/////////////////////////////// INTERFACE STATE ///////////////////////////////

static void update_title(void)
{
	const RAMWatchCore &core = RAMWatchCore::Get();
	const std::string &currentFile = core.CurrentFile();

	std::string title = TOOL_NAME;

	if (!currentFile.empty())
	{
		gchar *baseName = g_path_get_basename(currentFile.c_str());
		title += " \xE2\x80\x94 ";
		title += baseName;
		g_free(baseName);
	}
	if (core.IsDirty())
	{
		title += " *";
	}

	gtk_window_set_title(GTK_WINDOW(sWindow), title.c_str());
}

static void update_status_label(void)
{
	const size_t count = RAMWatchCore::Get().Count();
	gchar *text = g_strdup_printf("%" G_GSIZE_FORMAT " watch%s",
	                              (gsize)count, (count == 1) ? "" : "es");
	gtk_label_set_text(GTK_LABEL(sStatusLabel), text);
	g_free(text);
}

static int selected_row(void)
{
	GtkTreeSelection *selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(sTreeView));
	GList *rows = gtk_tree_selection_get_selected_rows(selection, NULL);
	int row = -1;

	if (rows != NULL)
	{
		GtkTreePath *path = (GtkTreePath *)rows->data;
		gint *indices = gtk_tree_path_get_indices(path);
		if (indices != NULL)
		{
			row = indices[0];
		}
	}
	g_list_free_full(rows, (GDestroyNotify)gtk_tree_path_free);

	return row;
}

static std::vector<size_t> selected_rows(void)
{
	std::vector<size_t> result;
	GtkTreeSelection *selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(sTreeView));
	GList *rows = gtk_tree_selection_get_selected_rows(selection, NULL);

	for (GList *item = rows; item != NULL; item = item->next)
	{
		gint *indices = gtk_tree_path_get_indices((GtkTreePath *)item->data);
		if (indices != NULL)
		{
			result.push_back((size_t)indices[0]);
		}
	}
	g_list_free_full(rows, (GDestroyNotify)gtk_tree_path_free);

	return result;
}

static void update_control_states(void)
{
	const RAMWatchCore &core = RAMWatchCore::Get();
	const int selectedCount = (int)selected_rows().size();
	const int first = selected_row();
	const gboolean canAdd = (core.Count() < RAMWatchCore::kMaxWatchCount);

	gtk_widget_set_sensitive(sNewButton, canAdd);
	gtk_widget_set_sensitive(sEditButton, (selectedCount == 1));
	gtk_widget_set_sensitive(sRemoveButton, (selectedCount >= 1));
	gtk_widget_set_sensitive(sDuplicateButton, (selectedCount == 1) && canAdd);
	gtk_widget_set_sensitive(sSeparatorButton, canAdd);
	gtk_widget_set_sensitive(sMoveUpButton, (selectedCount == 1) && (first > 0));
	gtk_widget_set_sensitive(sMoveDownButton,
	                         (selectedCount == 1) && (first >= 0) &&
	                         ((size_t)(first + 1) < core.Count()));
}

static void fill_row(GtkTreeIter *iter, size_t index)
{
	const RAMWatchCore &core = RAMWatchCore::Get();
	const RAMWatchCore::Watch &watch = core.At(index);

	if (core.IsSeparator(index))
	{
		gtk_list_store_set(sStore, iter,
		                   COL_ADDRESS, "",
		                   COL_VALUE, "",
		                   COL_NOTES, watch.Comment.c_str(),
		                   COL_FOREGROUND, kSeparatorColor,
		                   -1);
		return;
	}

	const std::string address = RAMWatchCore::DisplayAddress(watch);
	const std::string value = core.DisplayValueAt(index);

	gtk_list_store_set(sStore, iter,
	                   COL_ADDRESS, address.c_str(),
	                   COL_VALUE, value.c_str(),
	                   COL_NOTES, watch.Comment.c_str(),
	                   COL_FOREGROUND, NULL,
	                   -1);
}

// Rebuilds the whole model. Only called on structural changes (load, edit,
// reorder, insert from RAM Search), never per frame.
static void rebuild_model(void)
{
	RAMWatchCore &core = RAMWatchCore::Get();
	const int previousSelection = selected_row();

	gtk_list_store_clear(sStore);
	for (size_t i = 0; i < core.Count(); i++)
	{
		GtkTreeIter iter;
		gtk_list_store_append(sStore, &iter);
		fill_row(&iter, i);
	}
	sModelRowCount = (guint)core.Count();

	if ((previousSelection >= 0) && ((size_t)previousSelection < core.Count()))
	{
		GtkTreeSelection *selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(sTreeView));
		GtkTreePath *path = gtk_tree_path_new_from_indices(previousSelection, -1);
		gtk_tree_selection_select_path(selection, path);
		gtk_tree_path_free(path);
	}

	update_title();
	update_status_label();
	update_control_states();
}

/////////////////////////////// FILE OPERATIONS ///////////////////////////////

static gboolean save_watch_list_as(void);

static gboolean save_watch_list(void)
{
	RAMWatchCore &core = RAMWatchCore::Get();

	if (!core.CurrentFile().empty())
	{
		std::string error;
		if (!core.SaveCurrentFile(error))
		{
			show_error(error.empty() ? "Could not write the watch file." : error.c_str());
			return FALSE;
		}
		persist_preferences();
		rebuild_model();
		return TRUE;
	}

	return save_watch_list_as();
}

static const char *default_watch_file_name(void)
{
	static std::string name;

	const std::string &current = RAMWatchCore::Get().CurrentFile();
	if (!current.empty())
	{
		gchar *baseName = g_path_get_basename(current.c_str());
		name.assign(baseName);
		g_free(baseName);
		return name.c_str();
	}

	name.assign("Untitled.wch");
	return name.c_str();
}

static gboolean save_watch_list_as(void)
{
	GtkFileChooserNative *chooser = gtk_file_chooser_native_new("Save Watch List",
	                                                            GTK_WINDOW(sWindow),
	                                                            GTK_FILE_CHOOSER_ACTION_SAVE,
	                                                            "_Save", "_Cancel");
	gtk_file_chooser_set_do_overwrite_confirmation(GTK_FILE_CHOOSER(chooser), TRUE);
	gtk_file_chooser_set_current_name(GTK_FILE_CHOOSER(chooser), default_watch_file_name());
	add_watch_file_filters(GTK_FILE_CHOOSER(chooser));

	gboolean saved = FALSE;

	if (gtk_native_dialog_run(GTK_NATIVE_DIALOG(chooser)) == GTK_RESPONSE_ACCEPT)
	{
		GFile *file = gtk_file_chooser_get_file(GTK_FILE_CHOOSER(chooser));
		gchar *path = (file != NULL) ? g_file_get_path(file) : NULL;

		if (path != NULL)
		{
			const std::string finalPath = ensure_wch_extension(path);
			std::string error;

			if (RAMWatchCore::Get().SaveAsFile(finalPath, error))
			{
				persist_preferences();
				rebuild_model();
				saved = TRUE;
			}
			else
			{
				show_error(error.empty() ? "Could not write the watch file." : error.c_str());
			}
			g_free(path);
		}
		if (file != NULL)
		{
			g_object_unref(file);
		}
	}

	g_object_unref(chooser);
	return saved;
}

static void load_watch_file(const char *path, gboolean append)
{
	std::string error;

	if (!RAMWatchCore::Get().LoadFile(path, (append != FALSE), error))
	{
		show_error(error.empty() ? "Could not open the watch file." : error.c_str());
		return;
	}

	persist_preferences();
	rebuild_model();
}

static gboolean confirm_discard_changes(void)
{
	RAMWatchCore &core = RAMWatchCore::Get();

	if (!core.IsDirty())
	{
		return TRUE;
	}

	GtkWidget *dialog = gtk_message_dialog_new(GTK_WINDOW(sWindow),
	                                           (GtkDialogFlags)(GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT),
	                                           GTK_MESSAGE_QUESTION,
	                                           GTK_BUTTONS_NONE,
	                                           "Save changes to the watch list?");
	gtk_message_dialog_format_secondary_text(GTK_MESSAGE_DIALOG(dialog),
	                                         "Your changes will be lost if you don't save them.");
	gtk_dialog_add_buttons(GTK_DIALOG(dialog),
	                       "_Cancel", GTK_RESPONSE_CANCEL,
	                       "Don't Save", GTK_RESPONSE_NO,
	                       "_Save", GTK_RESPONSE_YES,
	                       NULL);
	gtk_dialog_set_default_response(GTK_DIALOG(dialog), GTK_RESPONSE_YES);

	const int response = gtk_dialog_run(GTK_DIALOG(dialog));
	gtk_widget_destroy(dialog);

	if (response == GTK_RESPONSE_YES)
	{
		return save_watch_list();
	}
	if (response == GTK_RESPONSE_NO)
	{
		core.SetDirty(FALSE);
		return TRUE;
	}
	return FALSE;
}

static void open_watch_file_dialog(gboolean append)
{
	if (!append && !confirm_discard_changes())
	{
		return;
	}

	GtkFileChooserNative *chooser = gtk_file_chooser_native_new(
		append ? "Append Watch List" : "Open Watch List",
		GTK_WINDOW(sWindow),
		GTK_FILE_CHOOSER_ACTION_OPEN,
		append ? "_Append" : "_Open", "_Cancel");
	add_watch_file_filters(GTK_FILE_CHOOSER(chooser));

	if (gtk_native_dialog_run(GTK_NATIVE_DIALOG(chooser)) == GTK_RESPONSE_ACCEPT)
	{
		GFile *file = gtk_file_chooser_get_file(GTK_FILE_CHOOSER(chooser));
		gchar *path = (file != NULL) ? g_file_get_path(file) : NULL;

		if (path != NULL)
		{
			load_watch_file(path, append);
			g_free(path);
		}
		if (file != NULL)
		{
			g_object_unref(file);
		}
	}

	g_object_unref(chooser);
}

/////////////////////////////// FILE MENU ///////////////////////////////

static void clear_menu(GtkWidget *menu)
{
	GList *children = gtk_container_get_children(GTK_CONTAINER(menu));
	for (GList *item = children; item != NULL; item = item->next)
	{
		gtk_widget_destroy(GTK_WIDGET(item->data));
	}
	g_list_free(children);
}

static GtkWidget *menu_item_new(const char *label, GCallback callback, gpointer data)
{
	GtkWidget *item = gtk_menu_item_new_with_label(label);
	if (callback != NULL)
	{
		g_signal_connect(G_OBJECT(item), "activate", callback, data);
	}
	gtk_widget_show(item);
	return item;
}

static void on_new_list(GtkMenuItem *item, gpointer data)
{
	if (!confirm_discard_changes())
	{
		return;
	}
	RAMWatchCore::Get().Clear();
	persist_preferences();
	rebuild_model();
}

static void on_open_list(GtkMenuItem *item, gpointer data)
{
	open_watch_file_dialog(FALSE);
}

static void on_append_list(GtkMenuItem *item, gpointer data)
{
	open_watch_file_dialog(TRUE);
}

static void on_open_recent(GtkMenuItem *item, gpointer data)
{
	const size_t index = (size_t)GPOINTER_TO_SIZE(data) - 1;

	if (!confirm_discard_changes())
	{
		return;
	}

	std::string error;
	if (!RAMWatchCore::Get().OpenRecentFile(index, error))
	{
		if (!error.empty())
		{
			show_error(error.c_str());
		}
	}
	persist_preferences();
	rebuild_model();
}

static void on_save_list(GtkMenuItem *item, gpointer data)
{
	save_watch_list();
}

static void on_save_list_as(GtkMenuItem *item, gpointer data)
{
	save_watch_list_as();
}

static void on_toggle_autoload(GtkMenuItem *item, gpointer data)
{
	RAMWatchCore &core = RAMWatchCore::Get();
	core.SetAutoLoad(!core.AutoLoad());
	persist_preferences();
}

static void on_toggle_save_position(GtkMenuItem *item, gpointer data)
{
	RAMWatchCore &core = RAMWatchCore::Get();
	core.SetSaveWindowPos(!core.SaveWindowPos());
	if (core.SaveWindowPos())
	{
		save_geometry();
	}
	persist_preferences();
}

static void on_close_window(GtkMenuItem *item, gpointer data)
{
	gtk_window_close(GTK_WINDOW(sWindow));
}

static void build_file_menu_items(GtkWidget *menu)
{
	const RAMWatchCore &core = RAMWatchCore::Get();

	gtk_menu_shell_append(GTK_MENU_SHELL(menu),
		menu_item_new("New List", G_CALLBACK(on_new_list), NULL));
	gtk_menu_shell_append(GTK_MENU_SHELL(menu),
		menu_item_new("Open\xE2\x80\xA6", G_CALLBACK(on_open_list), NULL));
	gtk_menu_shell_append(GTK_MENU_SHELL(menu),
		menu_item_new("Append File\xE2\x80\xA6", G_CALLBACK(on_append_list), NULL));

	// Open Recent submenu, rebuilt from the core on every menu open.
	GtkWidget *recentItem = gtk_menu_item_new_with_label("Open Recent");
	GtkWidget *recentMenu = gtk_menu_new();
	const std::vector<std::string> &recent = core.RecentFiles();

	if (recent.empty())
	{
		GtkWidget *none = gtk_menu_item_new_with_label("No Recent Files");
		gtk_widget_set_sensitive(none, FALSE);
		gtk_menu_shell_append(GTK_MENU_SHELL(recentMenu), none);
	}
	else
	{
		for (size_t i = 0; i < recent.size(); i++)
		{
			gchar *baseName = g_path_get_basename(recent[i].c_str());
			gchar *label = g_strdup_printf("%" G_GSIZE_FORMAT ". %s", (gsize)(i + 1), baseName);
			GtkWidget *entry = menu_item_new(label, G_CALLBACK(on_open_recent),
			                                 GSIZE_TO_POINTER(i + 1));
			gtk_widget_set_tooltip_text(entry, recent[i].c_str());
			gtk_menu_shell_append(GTK_MENU_SHELL(recentMenu), entry);
			g_free(label);
			g_free(baseName);
		}
	}
	gtk_widget_show_all(recentMenu);
	gtk_menu_item_set_submenu(GTK_MENU_ITEM(recentItem), recentMenu);
	gtk_widget_show(recentItem);
	gtk_menu_shell_append(GTK_MENU_SHELL(menu), recentItem);

	gtk_menu_shell_append(GTK_MENU_SHELL(menu), gtk_separator_menu_item_new());

	gtk_menu_shell_append(GTK_MENU_SHELL(menu),
		menu_item_new("Save", G_CALLBACK(on_save_list), NULL));
	gtk_menu_shell_append(GTK_MENU_SHELL(menu),
		menu_item_new("Save As\xE2\x80\xA6", G_CALLBACK(on_save_list_as), NULL));

	gtk_menu_shell_append(GTK_MENU_SHELL(menu), gtk_separator_menu_item_new());

	GtkWidget *autoLoad = gtk_check_menu_item_new_with_label("Auto-load");
	gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(autoLoad), core.AutoLoad());
	g_signal_connect(G_OBJECT(autoLoad), "toggled", G_CALLBACK(on_toggle_autoload), NULL);
	gtk_menu_shell_append(GTK_MENU_SHELL(menu), autoLoad);

	GtkWidget *savePos = gtk_check_menu_item_new_with_label("Save Window Position");
	gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(savePos), core.SaveWindowPos());
	g_signal_connect(G_OBJECT(savePos), "toggled", G_CALLBACK(on_toggle_save_position), NULL);
	gtk_menu_shell_append(GTK_MENU_SHELL(menu), savePos);

	gtk_menu_shell_append(GTK_MENU_SHELL(menu), gtk_separator_menu_item_new());
	gtk_menu_shell_append(GTK_MENU_SHELL(menu),
		menu_item_new("Close", G_CALLBACK(on_close_window), NULL));

	gtk_widget_show_all(menu);
}

// The check states and the recent list are session state, so the menu is
// rebuilt from the core every time it is opened instead of being kept in sync
// by hand.
static void on_file_menu_show(GtkWidget *menu, gpointer data)
{
	clear_menu(menu);
	build_file_menu_items(menu);
}

/////////////////////////////// WATCH EDITING ///////////////////////////////

static void on_remove_selected(GtkButton *button, gpointer data);
static void on_edit_selected(GtkButton *button, gpointer data);

static void present_watch_editor(gboolean editing, size_t index)
{
	RAMWatchCore &core = RAMWatchCore::Get();

	uint32_t address = 0;
	char sizeCode = 'b';
	char typeCode = 's';
	std::string comment;

	if (editing && (index < core.Count()))
	{
		const RAMWatchCore::Watch &watch = core.At(index);
		address = watch.Address;
		sizeCode = watch.Size;
		typeCode = watch.Type;
		comment = watch.Comment;
	}

	GtkWidget *dialog = gtk_dialog_new_with_buttons(editing ? "Edit Watch" : "New Watch",
	                                                GTK_WINDOW(sWindow),
	                                                (GtkDialogFlags)(GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT),
	                                                "_Cancel", GTK_RESPONSE_CANCEL,
	                                                "_OK", GTK_RESPONSE_OK,
	                                                NULL);
	gtk_dialog_set_default_response(GTK_DIALOG(dialog), GTK_RESPONSE_OK);

	GtkWidget *grid = gtk_grid_new();
	gtk_grid_set_row_spacing(GTK_GRID(grid), 6);
	gtk_grid_set_column_spacing(GTK_GRID(grid), 8);
	gtk_container_set_border_width(GTK_CONTAINER(grid), 12);

	GtkWidget *addressLabel = gtk_label_new("Address:");
	gtk_widget_set_halign(addressLabel, GTK_ALIGN_END);

	gchar *addressText = g_strdup_printf("%08X", (unsigned int)address);
	GtkWidget *addressEntry = gtk_entry_new();
	gtk_entry_set_text(GTK_ENTRY(addressEntry), addressText);
	gtk_entry_set_activates_default(GTK_ENTRY(addressEntry), TRUE);
	gtk_entry_set_width_chars(GTK_ENTRY(addressEntry), 10);
	g_free(addressText);

	// Monospace makes hexadecimal addresses readable. Pango attributes are
	// used instead of the deprecated gtk_widget_override_font().
	PangoAttrList *attributes = pango_attr_list_new();
	pango_attr_list_insert(attributes, pango_attr_family_new("monospace"));
	gtk_entry_set_attributes(GTK_ENTRY(addressEntry), attributes);
	pango_attr_list_unref(attributes);

	GtkWidget *sizeCombo = gtk_combo_box_text_new();
	for (int i = 0; i < 3; i++)
	{
		gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(sizeCombo), kSizeLabels[i]);
	}
	gtk_combo_box_set_active(GTK_COMBO_BOX(sizeCombo),
	                         (sizeCode == 'w') ? 1 : (sizeCode == 'd') ? 2 : 0);

	GtkWidget *typeLabel = gtk_label_new("Display:");
	gtk_widget_set_halign(typeLabel, GTK_ALIGN_END);

	GtkWidget *typeCombo = gtk_combo_box_text_new();
	for (int i = 0; i < 4; i++)
	{
		gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(typeCombo), kTypeLabels[i]);
	}
	gtk_combo_box_set_active(GTK_COMBO_BOX(typeCombo),
	                         (typeCode == 'u') ? 1 : (typeCode == 'h') ? 2 : (typeCode == 'f') ? 3 : 0);

	GtkWidget *commentLabel = gtk_label_new("Notes:");
	gtk_widget_set_halign(commentLabel, GTK_ALIGN_END);

	GtkWidget *commentEntry = gtk_entry_new();
	gtk_entry_set_text(GTK_ENTRY(commentEntry), comment.c_str());
	gtk_entry_set_activates_default(GTK_ENTRY(commentEntry), TRUE);

	GtkWidget *hint = gtk_label_new("Hexadecimal address, e.g. 02000000");
	gtk_widget_set_halign(hint, GTK_ALIGN_START);
	gtk_style_context_add_class(gtk_widget_get_style_context(hint), "dim-label");

	gtk_grid_attach(GTK_GRID(grid), addressLabel, 0, 0, 1, 1);
	gtk_grid_attach(GTK_GRID(grid), addressEntry, 1, 0, 1, 1);
	gtk_grid_attach(GTK_GRID(grid), sizeCombo, 2, 0, 1, 1);
	gtk_grid_attach(GTK_GRID(grid), typeLabel, 0, 1, 1, 1);
	gtk_grid_attach(GTK_GRID(grid), typeCombo, 1, 1, 2, 1);
	gtk_grid_attach(GTK_GRID(grid), commentLabel, 0, 2, 1, 1);
	gtk_grid_attach(GTK_GRID(grid), commentEntry, 1, 2, 2, 1);
	gtk_grid_attach(GTK_GRID(grid), hint, 1, 3, 2, 1);

	gtk_box_pack_start(GTK_BOX(gtk_dialog_get_content_area(GTK_DIALOG(dialog))),
	                   grid, TRUE, TRUE, 0);
	gtk_widget_show_all(dialog);

	// Re-run the dialog on a rejected address or a duplicate row so the user
	// keeps what they typed instead of losing the whole edit.
	gboolean committed = FALSE;

	while (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_OK)
	{
		bool valid = false;
		const char *addressString = gtk_entry_get_text(GTK_ENTRY(addressEntry));
		uint32_t newAddress = RAMDebugCore::ParseAddress(addressString, &valid);
		RAMDebugCore::NormalizeAddress(newAddress);

		if (!valid || !RAMDebugCore::IsRAMAddressValid(newAddress))
		{
			show_error("Invalid Address");
			continue;
		}

		RAMWatchCore::Watch watch;
		watch.Address = newAddress;

		const gint sizeIndex = gtk_combo_box_get_active(GTK_COMBO_BOX(sizeCombo));
		const gint typeIndex = gtk_combo_box_get_active(GTK_COMBO_BOX(typeCombo));
		watch.Size = kSizeCodes[((sizeIndex >= 0) && (sizeIndex < 3)) ? sizeIndex : 0];
		watch.Type = kTypeCodes[((typeIndex >= 0) && (typeIndex < 4)) ? typeIndex : 0];
		watch.WrongEndian = false;
		watch.Comment = gtk_entry_get_text(GTK_ENTRY(commentEntry));
		watch.CurValue = 0;

		const bool inserted = editing ? core.Replace(index, watch) : core.Insert(watch);
		if (!inserted)
		{
			show_error((core.Count() >= RAMWatchCore::kMaxWatchCount)
			           ? "The watch list is full (256 entries)."
			           : "A watch with this address, size and type already exists.");
			continue;
		}

		committed = TRUE;
		break;
	}

	gtk_widget_destroy(dialog);

	if (committed)
	{
		rebuild_model();
	}
}

static void on_new_watch(GtkButton *button, gpointer data)
{
	if (RAMWatchCore::Get().Count() >= RAMWatchCore::kMaxWatchCount)
	{
		return;
	}
	present_watch_editor(FALSE, 0);
}

static void on_edit_selected(GtkButton *button, gpointer data)
{
	const int row = selected_row();
	RAMWatchCore &core = RAMWatchCore::Get();

	if ((row < 0) || ((size_t)row >= core.Count()) || core.IsSeparator((size_t)row))
	{
		return;
	}
	present_watch_editor(TRUE, (size_t)row);
}

static void on_row_activated(GtkTreeView *view, GtkTreePath *path, GtkTreeViewColumn *column,
                             gpointer data)
{
	on_edit_selected(NULL, NULL);
}

static void on_remove_selected(GtkButton *button, gpointer data)
{
	RAMWatchCore &core = RAMWatchCore::Get();
	const std::vector<size_t> rows = selected_rows();

	// Remove from the end so the remaining indexes stay valid.
	for (size_t i = rows.size(); i > 0; i--)
	{
		core.Remove(rows[i - 1]);
	}

	GtkTreeSelection *selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(sTreeView));
	gtk_tree_selection_unselect_all(selection);
	rebuild_model();
}

static void on_duplicate(GtkButton *button, gpointer data)
{
	const int row = selected_row();
	RAMWatchCore &core = RAMWatchCore::Get();

	if ((row < 0) || ((size_t)row >= core.Count()))
	{
		return;
	}
	// Opens prefilled, like the Windows Duplicate command, so the address or
	// type can be changed before inserting.
	present_watch_editor(FALSE, (size_t)row);
}

static void on_add_separator(GtkButton *button, gpointer data)
{
	RAMWatchCore::Get().AddSeparator();
	rebuild_model();
}

static void on_move_up(GtkButton *button, gpointer data)
{
	const int row = selected_row();
	RAMWatchCore &core = RAMWatchCore::Get();

	if ((row <= 0) || !core.MoveUp((size_t)row))
	{
		return;
	}

	GtkTreeSelection *selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(sTreeView));
	gtk_tree_selection_unselect_all(selection);
	rebuild_model();
	GtkTreePath *path = gtk_tree_path_new_from_indices(row - 1, -1);
	gtk_tree_selection_select_path(selection, path);
	gtk_tree_path_free(path);
}

static void on_move_down(GtkButton *button, gpointer data)
{
	const int row = selected_row();
	RAMWatchCore &core = RAMWatchCore::Get();

	if ((row < 0) || !core.MoveDown((size_t)row))
	{
		return;
	}

	GtkTreeSelection *selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(sTreeView));
	gtk_tree_selection_unselect_all(selection);
	rebuild_model();
	GtkTreePath *path = gtk_tree_path_new_from_indices(row + 1, -1);
	gtk_tree_selection_select_path(selection, path);
	gtk_tree_path_free(path);
}

static void on_selection_changed(GtkTreeSelection *selection, gpointer data)
{
	update_control_states();
}

/////////////////////////////// DRAG AND DROP ///////////////////////////////

static void on_drag_data_received(GtkWidget *widget, GdkDragContext *context,
                                  gint x, gint y, GtkSelectionData *data,
                                  guint info, guint time, gpointer userData)
{
	gchar **uris = gtk_selection_data_get_uris(data);
	gboolean handled = FALSE;

	for (int i = 0; (uris != NULL) && (uris[i] != NULL) && !handled; i++)
	{
		gchar *path = g_filename_from_uri(uris[i], NULL, NULL);
		if ((path != NULL) && is_watch_file_name(path))
		{
			if (confirm_discard_changes())
			{
				load_watch_file(path, FALSE);
				handled = TRUE;
			}
		}
		g_free(path);
	}
	if (uris != NULL)
	{
		g_strfreev(uris);
	}

	gtk_drag_finish(context, handled, FALSE, time);
}

/////////////////////////////// WINDOW LIFECYCLE ///////////////////////////////

static gboolean on_delete_event(GtkWidget *widget, GdkEvent *event, gpointer data)
{
	if (!confirm_discard_changes())
	{
		return TRUE; // Veto the close.
	}

	save_geometry();
	persist_preferences();
	return FALSE;
}

static void on_window_destroy(GtkWidget *widget, gpointer data)
{
	sWindow = NULL;
	sScrolledWindow = NULL;
	sTreeView = NULL;
	sStore = NULL;
	sStatusLabel = NULL;
	sFileButton = NULL;
	sFileMenu = NULL;
	sNewButton = NULL;
	sEditButton = NULL;
	sRemoveButton = NULL;
	sDuplicateButton = NULL;
	sSeparatorButton = NULL;
	sMoveUpButton = NULL;
	sMoveDownButton = NULL;
	sModelRowCount = 0;

	if (sToolID >= 0)
	{
		dTool_CloseCallback(sToolID);
	}
	sToolID = -1;
}

static GtkWidget *create_sidebar_button(const char *label, GCallback callback,
                                        GtkWidget *box)
{
	GtkWidget *button = gtk_button_new_with_label(label);
	g_signal_connect(G_OBJECT(button), "clicked", callback, NULL);
	gtk_box_pack_start(GTK_BOX(box), button, FALSE, FALSE, 0);
	gtk_widget_show(button);
	return button;
}

static void build_window(void)
{
	sWindow = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	gtk_window_set_title(GTK_WINDOW(sWindow), TOOL_NAME);
	gtk_window_set_default_size(GTK_WINDOW(sWindow), kDefaultWidth, kDefaultHeight);
	gtk_window_set_resizable(GTK_WINDOW(sWindow), TRUE);
	gtk_window_set_icon_name(GTK_WINDOW(sWindow), "org.desmume.DeSmuME");
	gtk_container_set_border_width(GTK_CONTAINER(sWindow), 6);

	g_signal_connect(G_OBJECT(sWindow), "delete-event", G_CALLBACK(on_delete_event), NULL);
	g_signal_connect(G_OBJECT(sWindow), "destroy", G_CALLBACK(on_window_destroy), NULL);

	GtkWidget *mainBox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
	gtk_container_add(GTK_CONTAINER(sWindow), mainBox);

	// ---- Watch list ----
	sStore = gtk_list_store_new(COLUMN_COUNT,
	                            G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING);
	sTreeView = gtk_tree_view_new_with_model(GTK_TREE_MODEL(sStore));
	gtk_tree_view_set_enable_search(GTK_TREE_VIEW(sTreeView), FALSE);
	gtk_tree_view_set_headers_visible(GTK_TREE_VIEW(sTreeView), TRUE);

	GtkCellRenderer *renderer = gtk_cell_renderer_text_new();
	g_object_set(renderer, "family", "monospace", NULL);

	GtkTreeViewColumn *column = gtk_tree_view_column_new_with_attributes(
		"Address", renderer, "text", COL_ADDRESS, "foreground", COL_FOREGROUND, NULL);
	gtk_tree_view_column_set_resizable(column, TRUE);
	gtk_tree_view_column_set_min_width(column, 90);
	gtk_tree_view_column_set_sizing(column, GTK_TREE_VIEW_COLUMN_FIXED);
	gtk_tree_view_column_set_fixed_width(column, 110);
	gtk_tree_view_append_column(GTK_TREE_VIEW(sTreeView), column);

	renderer = gtk_cell_renderer_text_new();
	g_object_set(renderer, "family", "monospace", NULL);
	column = gtk_tree_view_column_new_with_attributes(
		"Value", renderer, "text", COL_VALUE, "foreground", COL_FOREGROUND, NULL);
	gtk_tree_view_column_set_resizable(column, TRUE);
	gtk_tree_view_column_set_min_width(column, 70);
	gtk_tree_view_column_set_sizing(column, GTK_TREE_VIEW_COLUMN_FIXED);
	gtk_tree_view_column_set_fixed_width(column, 100);
	gtk_tree_view_append_column(GTK_TREE_VIEW(sTreeView), column);

	renderer = gtk_cell_renderer_text_new();
	column = gtk_tree_view_column_new_with_attributes(
		"Notes", renderer, "text", COL_NOTES, "foreground", COL_FOREGROUND, NULL);
	gtk_tree_view_column_set_resizable(column, TRUE);
	gtk_tree_view_column_set_expand(column, TRUE);
	gtk_tree_view_append_column(GTK_TREE_VIEW(sTreeView), column);

	GtkTreeSelection *selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(sTreeView));
	gtk_tree_selection_set_mode(selection, GTK_SELECTION_MULTIPLE);
	g_signal_connect(G_OBJECT(selection), "changed", G_CALLBACK(on_selection_changed), NULL);
	g_signal_connect(G_OBJECT(sTreeView), "row-activated",
	                 G_CALLBACK(on_row_activated), NULL);

	sScrolledWindow = gtk_scrolled_window_new(NULL, NULL);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(sScrolledWindow),
	                               GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
	gtk_scrolled_window_set_shadow_type(GTK_SCROLLED_WINDOW(sScrolledWindow),
	                                    GTK_SHADOW_IN);
	gtk_widget_set_hexpand(sScrolledWindow, TRUE);
	gtk_widget_set_vexpand(sScrolledWindow, TRUE);
	gtk_container_add(GTK_CONTAINER(sScrolledWindow), sTreeView);
	gtk_box_pack_start(GTK_BOX(mainBox), sScrolledWindow, TRUE, TRUE, 0);

	// .wch files can be dropped straight onto the list.
	GtkTargetEntry targets[] = { { (gchar *)"text/uri-list", 0, 0 } };
	gtk_drag_dest_set(sScrolledWindow, GTK_DEST_DEFAULT_ALL,
	                  targets, 1, GDK_ACTION_COPY);
	g_signal_connect(G_OBJECT(sScrolledWindow), "drag-data-received",
	                 G_CALLBACK(on_drag_data_received), NULL);

	// ---- Button bar ----
	GtkWidget *sidebar = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
	gtk_box_set_homogeneous(GTK_BOX(sidebar), FALSE);
	// Buttons that fill the sidebar propagate their hexpand upwards, which
	// would make the sidebar itself compete with the tree view for the extra
	// width. Pin it instead: the list gets everything the bar does not need.
	gtk_widget_set_hexpand(sidebar, FALSE);

	sFileMenu = gtk_menu_new();
	g_signal_connect(G_OBJECT(sFileMenu), "show", G_CALLBACK(on_file_menu_show), NULL);

	sFileButton = gtk_menu_button_new();
	gtk_button_set_label(GTK_BUTTON(sFileButton), "File");
	gtk_widget_set_hexpand(sFileButton, TRUE);
	gtk_menu_button_set_popup(GTK_MENU_BUTTON(sFileButton), sFileMenu);
	gtk_box_pack_start(GTK_BOX(sidebar), sFileButton, FALSE, FALSE, 0);

	gtk_box_pack_start(GTK_BOX(sidebar), gtk_separator_new(GTK_ORIENTATION_HORIZONTAL),
	                   FALSE, FALSE, 0);

	sNewButton       = create_sidebar_button("New",       G_CALLBACK(on_new_watch), sidebar);
	sEditButton      = create_sidebar_button("Edit",      G_CALLBACK(on_edit_selected), sidebar);
	sRemoveButton    = create_sidebar_button("Remove",    G_CALLBACK(on_remove_selected), sidebar);
	sDuplicateButton = create_sidebar_button("Duplicate", G_CALLBACK(on_duplicate), sidebar);
	sSeparatorButton = create_sidebar_button("Separator", G_CALLBACK(on_add_separator), sidebar);

	GtkWidget *moveBox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
	gtk_box_set_homogeneous(GTK_BOX(moveBox), TRUE);
	sMoveUpButton = gtk_button_new_with_label("\xE2\x86\x91");
	g_signal_connect(G_OBJECT(sMoveUpButton), "clicked", G_CALLBACK(on_move_up), NULL);
	gtk_box_pack_start(GTK_BOX(moveBox), sMoveUpButton, TRUE, TRUE, 0);
	gtk_widget_show(sMoveUpButton);

	sMoveDownButton = gtk_button_new_with_label("\xE2\x86\x93");
	g_signal_connect(G_OBJECT(sMoveDownButton), "clicked", G_CALLBACK(on_move_down), NULL);
	gtk_box_pack_start(GTK_BOX(moveBox), sMoveDownButton, TRUE, TRUE, 0);
	gtk_widget_show(sMoveDownButton);

	gtk_widget_set_tooltip_text(sMoveUpButton, "Move the selected watch up");
	gtk_widget_set_tooltip_text(sMoveDownButton, "Move the selected watch down");
	gtk_box_pack_start(GTK_BOX(sidebar), moveBox, FALSE, FALSE, 0);
	gtk_widget_show(moveBox);

	sStatusLabel = gtk_label_new("");
	gtk_label_set_xalign(GTK_LABEL(sStatusLabel), 0.5f);
	gtk_style_context_add_class(gtk_widget_get_style_context(sStatusLabel), "dim-label");
	gtk_box_pack_start(GTK_BOX(sidebar), sStatusLabel, FALSE, FALSE, 0);
	gtk_widget_show(sStatusLabel);

	// Keep the button bar narrow and fixed; the list takes the rest.
	gtk_widget_set_size_request(sidebar, 132, -1);
	gtk_box_pack_start(GTK_BOX(mainBox), sidebar, FALSE, FALSE, 0);

	gtk_widget_show_all(mainBox);
}

/////////////////////////////// TOOL ENTRY POINTS ///////////////////////////////

static void update(void)
{
	if ((sWindow == NULL) || (sStore == NULL))
	{
		return;
	}

	RAMWatchCore &core = RAMWatchCore::Get();

	// RAM Search can insert watches behind our back; a count mismatch is the
	// cheapest way to notice, and it also covers list clears.
	if ((guint)core.Count() != sModelRowCount)
	{
		rebuild_model();
		return;
	}

	if (core.Count() == 0)
	{
		return;
	}

	std::vector<bool> changed;
	if (!core.RefreshValues(changed))
	{
		return;
	}

	// The model is one row per watch, so the value column can be updated in
	// a single pass without touching address or notes cells.
	GtkTreeIter iter;
	gboolean valid = gtk_tree_model_get_iter_first(GTK_TREE_MODEL(sStore), &iter);
	for (size_t i = 0; valid && (i < changed.size()); i++)
	{
		if (changed[i])
		{
			const std::string value = core.DisplayValueAt(i);
			gtk_list_store_set(sStore, &iter, COL_VALUE, value.c_str(), -1);
		}
		valid = gtk_tree_model_iter_next(GTK_TREE_MODEL(sStore), &iter);
	}
}

static void open_window(int ID)
{
	if (sWindow != NULL)
	{
		gtk_window_present(GTK_WINDOW(sWindow));
		return;
	}

	sToolID = ID;

	load_preferences();
	build_window();
	restore_geometry();
	rebuild_model();
	gtk_widget_show(sWindow);

	// Auto-load the most recent list once per session, mirroring the macOS
	// frontend's shouldAutoLoadOnLaunch behaviour.
	if (!sDidAttemptAutoLoad)
	{
		sDidAttemptAutoLoad = TRUE;

		RAMWatchCore &core = RAMWatchCore::Get();
		if (core.AutoLoad() && (core.Count() == 0) && !core.RecentFiles().empty())
		{
			std::string error;
			if (core.OpenRecentFile(0, error))
			{
				persist_preferences();
				rebuild_model();
			}
		}
	}
}

static void close_tool(void)
{
	// The dTool framework never calls this; the window's own destroy handler
	// notifies it through dTool_CloseCallback().
}

dTool_t dTool_ramWatch =
{
	SHORTNAME,
	TOOL_NAME,
	&open_window,
	&update,
	&close_tool
};
