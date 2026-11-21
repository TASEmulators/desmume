/*
	ramwatch.cpp - RAM Watch tool for the DeSmuME GTK4 frontend

	Copyright (C) 2026 DeSmuME team

	GTK4 host for the shared RAMWatchCore document model.  This is the same
	core the GTK3/Windows/Cocoa frontends use; only the UI differs.  The GTK3
	version relies on GtkTreeView, GtkDialog + gtk_dialog_run and
	GtkFileChooserNative; all of those are gone or deprecated in GTK4, so this
	version uses GtkColumnView, plain GtkWindows with async callbacks and
	GtkFileDialog instead.
*/

#include "ramwatch.h"
#include "../emulation.h"
#include "../config.h"
#include "rowmodel.h"

#include <string>
#include <vector>

#include "../../../shared/ramwatch_core.h"
#include "../../../shared/ram_debug_core.h"

#define TOOL_NAME "RAM Watch"

static const char kSizeLabels[3][16] = { "1 Byte", "2 Bytes", "4 Bytes" };
static const char kTypeLabels[4][16] = { "Signed", "Unsigned", "Hex", "20.12 Fixed" };
static const char kSizeCodes[3] = { 'b', 'w', 'd' };
static const char kTypeCodes[4] = { 's', 'u', 'h', 'f' };

static GtkWidget *sWindow = NULL;
static GtkWidget *sColumnView = NULL;
static GListStore *sRows = NULL;
static std::vector<DesmumeRow *> sRowCache;
static GtkWidget *sStatusLabel = NULL;
static GtkWidget *sNewButton = NULL;
static GtkWidget *sEditButton = NULL;
static GtkWidget *sRemoveButton = NULL;
static GtkWidget *sDuplicateButton = NULL;
static GtkWidget *sSeparatorButton = NULL;
static GtkWidget *sMoveUpButton = NULL;
static GtkWidget *sMoveDownButton = NULL;
static GMenu *sRecentMenu = NULL;
static guint sModelRowCount = 0;
static gboolean sDidAttemptAutoLoad = FALSE;

static const char *const sColumnTitles[] = { "Address", "Value", "Notes" };

/* ---------------------------------------------------------------- helpers */

static void
ShowError(const char *message)
{
	GtkAlertDialog *dialog = gtk_alert_dialog_new(TOOL_NAME);
	gtk_alert_dialog_set_message(dialog, message != NULL ? message : "Operation failed.");
	gtk_alert_dialog_choose(dialog, GTK_WINDOW(sWindow), NULL, NULL, NULL);
	g_object_unref(dialog);
}

static void
persist_preferences(void)
{
	RAMWatchCore &core = RAMWatchCore::Get();
	desmume::config::Config *config = desmume_host_config();
	if (config == NULL)
	{
		return;
	}
	config->ramwatch_autoload.set(core.AutoLoad());
	config->ramwatch_save_window_pos.set(core.SaveWindowPos());
	config->ramwatch_recent_files.set(core.RecentFiles());
	config->save();
}

static void
load_preferences(void)
{
	RAMWatchCore &core = RAMWatchCore::Get();
	desmume::config::Config *config = desmume_host_config();
	if (config == NULL)
	{
		return;
	}
	core.SetAutoLoad(config->ramwatch_autoload.get());
	core.SetSaveWindowPos(false); /* Wayland cannot restore window positions */
	core.SetRecentFiles(config->ramwatch_recent_files.get());
}

static std::string
ensure_wch_extension(const char *path)
{
	std::string result(path != NULL ? path : "");
	if ((result.size() < 4) ||
	    (g_ascii_strcasecmp(result.c_str() + result.size() - 4, ".wch") != 0))
	{
		result += ".wch";
	}
	return result;
}

static bool
is_watch_file_name(const char *path)
{
	const size_t length = (path != NULL) ? strlen(path) : 0;
	return (length > 4) && (g_ascii_strcasecmp(path + length - 4, ".wch") == 0);
}

static void
update_title(void)
{
	if (sWindow == NULL)
	{
		return;
	}
	const RAMWatchCore &core = RAMWatchCore::Get();
	std::string title = TOOL_NAME;
	if (!core.CurrentFile().empty())
	{
		gchar *base = g_path_get_basename(core.CurrentFile().c_str());
		title += " \xE2\x80\x94 ";
		title += base;
		g_free(base);
	}
	if (core.IsDirty())
	{
		title += " *";
	}
	gtk_window_set_title(GTK_WINDOW(sWindow), title.c_str());
}

static void
update_status_label(void)
{
	if (sStatusLabel == NULL)
	{
		return;
	}
	const size_t count = RAMWatchCore::Get().Count();
	gchar *text = g_strdup_printf("%" G_GSIZE_FORMAT " watch%s",
	                              (gsize)count, (count == 1) ? "" : "es");
	gtk_label_set_text(GTK_LABEL(sStatusLabel), text);
	g_free(text);
}

static int
selected_row(void)
{
	return desmume_column_view_selected(sColumnView, NULL);
}

static std::vector<size_t>
selected_rows(void)
{
	std::vector<size_t> result;
	GArray *positions = NULL;
	desmume_column_view_selected(sColumnView, &positions);
	if (positions != NULL)
	{
		for (guint i = 0; i < positions->len; ++i)
		{
			result.push_back(g_array_index(positions, guint, i));
		}
		g_array_unref(positions);
	}
	return result;
}

static void
update_control_states(void)
{
	const RAMWatchCore &core = RAMWatchCore::Get();
	const std::vector<size_t> rows = selected_rows();
	const int selectedCount = (int)rows.size();
	const int first = selectedCount > 0 ? (int)rows[0] : -1;
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

static void
fill_row(DesmumeRow *row, size_t index)
{
	const RAMWatchCore &core = RAMWatchCore::Get();
	const RAMWatchCore::Watch &watch = core.At(index);

	if (core.IsSeparator(index))
	{
		desmume_row_set(row, 0, "────────");
		desmume_row_set(row, 1, "");
		desmume_row_set(row, 2, watch.Comment.c_str());
		return;
	}

	const std::string address = RAMWatchCore::DisplayAddress(watch);
	const std::string value = core.DisplayValueAt(index);
	desmume_row_set(row, 0, address.c_str());
	desmume_row_set(row, 1, value.c_str());
	desmume_row_set(row, 2, watch.Comment.c_str());
}

/* Rebuilds the whole model on structural changes only, never per frame. */
static void
rebuild_model(void)
{
	RAMWatchCore &core = RAMWatchCore::Get();

	for (DesmumeRow *row : sRowCache)
	{
		g_object_unref(row);
	}
	sRowCache.clear();
	if (sRows != NULL)
	{
		g_list_store_remove_all(sRows);
	}

	for (size_t i = 0; i < core.Count(); ++i)
	{
		DesmumeRow *row = desmume_row_new("", "", "", "", "");
		fill_row(row, i);
		sRowCache.push_back(row);
		g_list_store_append(sRows, G_OBJECT(row));
	}
	sModelRowCount = (guint)core.Count();

	update_title();
	update_status_label();
	update_control_states();
}

static void
rebuild_recent_menu(void)
{
	if (sRecentMenu == NULL)
	{
		return;
	}
	g_menu_remove_all(sRecentMenu);
	const RAMWatchCore &core = RAMWatchCore::Get();
	const std::vector<std::string> &recent = core.RecentFiles();
	if (recent.empty())
	{
		g_menu_append(sRecentMenu, "(No recent files)", NULL);
		return;
	}
	for (size_t i = 0; i < recent.size(); ++i)
	{
		gchar *base = g_path_get_basename(recent[i].c_str());
		GMenuItem *item = g_menu_item_new(base, NULL);
		g_menu_item_set_action_and_target_value(
			item, "toolwin.open-recent", g_variant_new_uint64(i));
		g_menu_append_item(sRecentMenu, item);
		g_object_unref(item);
		g_free(base);
	}
}

/* ------------------------------------------------------------ file actions */

static void load_watch_file(const char *path, gboolean append);

static gboolean
save_watch_list(void)
{
	RAMWatchCore &core = RAMWatchCore::Get();
	if (!core.CurrentFile().empty())
	{
		std::string error;
		if (!core.SaveCurrentFile(error))
		{
			ShowError(error.empty() ? "Could not write the watch file." : error.c_str());
			return FALSE;
		}
		persist_preferences();
		rebuild_model();
		return TRUE;
	}
	/* No associated file yet: present Save As asynchronously. */
	return FALSE;
}

static void
on_save_as_done(GObject *source, GAsyncResult *result, gpointer user_data)
{
	(void)user_data;
	GError *error = NULL;
	GFile *file = gtk_file_dialog_save_finish(GTK_FILE_DIALOG(source), result, &error);
	if (file == NULL)
	{
		if ((error != NULL) &&
		    !g_error_matches(error, GTK_DIALOG_ERROR, GTK_DIALOG_ERROR_DISMISSED))
		{
			ShowError(error->message);
		}
		g_clear_error(&error);
		return;
	}

	char *path = g_file_get_path(file);
	g_object_unref(file);
	if (path != NULL)
	{
		const std::string finalPath = ensure_wch_extension(path);
		std::string saveError;
		if (RAMWatchCore::Get().SaveAsFile(finalPath, saveError))
		{
			persist_preferences();
			rebuild_model();
		}
		else
		{
			ShowError(saveError.empty() ? "Could not write the watch file." : saveError.c_str());
		}
		g_free(path);
	}
}

static void
present_save_as(void)
{
	GtkFileDialog *dialog = gtk_file_dialog_new();
	gtk_file_dialog_set_title(dialog, "Save Watch List");
	gtk_file_dialog_set_modal(dialog, TRUE);

	const char *rom = desmume_host_rom_base_name();
	std::string defaultName = ((rom != NULL) && (rom[0] != '\0'))
		? std::string(rom) + ".wch" : std::string("Untitled.wch");
	gtk_file_dialog_set_initial_name(dialog, defaultName.c_str());

	GtkFileFilter *filter = gtk_file_filter_new();
	gtk_file_filter_set_name(filter, "DeSmuME watch lists (*.wch)");
	gtk_file_filter_add_pattern(filter, "*.wch");
	gtk_file_filter_add_pattern(filter, "*.WCH");
	GListStore *filters = g_list_store_new(GTK_TYPE_FILE_FILTER);
	g_list_store_append(filters, filter);
	gtk_file_dialog_set_filters(dialog, G_LIST_MODEL(filters));

	gtk_file_dialog_save(dialog, GTK_WINDOW(sWindow), NULL, on_save_as_done, NULL);
	g_object_unref(dialog);
	g_object_unref(filter);
	g_object_unref(filters);
}

static void
on_open_done(GObject *source, GAsyncResult *result, gpointer user_data)
{
	const gboolean append = GPOINTER_TO_INT(user_data) != 0;
	GError *error = NULL;
	GFile *file = gtk_file_dialog_open_finish(GTK_FILE_DIALOG(source), result, &error);
	if (file == NULL)
	{
		if ((error != NULL) &&
		    !g_error_matches(error, GTK_DIALOG_ERROR, GTK_DIALOG_ERROR_DISMISSED))
		{
			ShowError(error->message);
		}
		g_clear_error(&error);
		return;
	}
	char *path = g_file_get_path(file);
	g_object_unref(file);
	if (path != NULL)
	{
		load_watch_file(path, append);
		g_free(path);
	}
}

static void
present_open(gboolean append)
{
	GtkFileDialog *dialog = gtk_file_dialog_new();
	gtk_file_dialog_set_title(dialog, append ? "Append Watch List" : "Open Watch List");
	gtk_file_dialog_set_modal(dialog, TRUE);

	GtkFileFilter *filter = gtk_file_filter_new();
	gtk_file_filter_set_name(filter, "DeSmuME watch lists (*.wch)");
	gtk_file_filter_add_pattern(filter, "*.wch");
	gtk_file_filter_add_pattern(filter, "*.WCH");
	GListStore *filters = g_list_store_new(GTK_TYPE_FILE_FILTER);
	g_list_store_append(filters, filter);
	gtk_file_dialog_set_filters(dialog, G_LIST_MODEL(filters));

	gtk_file_dialog_open(dialog, GTK_WINDOW(sWindow), NULL,
	                     on_open_done, GINT_TO_POINTER(append));
	g_object_unref(dialog);
	g_object_unref(filter);
	g_object_unref(filters);
}

static void
load_watch_file(const char *path, gboolean append)
{
	std::string error;
	if (!RAMWatchCore::Get().LoadFile(path, append != FALSE, error))
	{
		ShowError(error.empty() ? "Could not open the watch file." : error.c_str());
		return;
	}
	persist_preferences();
	rebuild_model();
	rebuild_recent_menu();
}

/* ------------------------------------------------------------ watch editor */

struct WatchEditorCtx
{
	GtkWidget *window;
	GtkWidget *address_entry;
	GtkWidget *size_dropdown;
	GtkWidget *type_dropdown;
	GtkWidget *comment_entry;
	gboolean editing;
	size_t index;
};

static void
free_editor_ctx(gpointer data)
{
	WatchEditorCtx *ctx = (WatchEditorCtx *)data;
	delete ctx;
}

static void
on_editor_close_request(GtkWindow *window, gpointer user_data)
{
	(void)window;
	(void)user_data;
	/* No special handling: the window is transient and can be dismissed. */
}

static void
on_editor_ok(GtkButton *button, gpointer user_data)
{
	(void)button;
	WatchEditorCtx *ctx = (WatchEditorCtx *)user_data;
	RAMWatchCore &core = RAMWatchCore::Get();

	bool valid = false;
	const char *addressText = gtk_editable_get_text(GTK_EDITABLE(ctx->address_entry));
	uint32_t address = RAMDebugCore::ParseAddress(addressText, &valid);
	RAMDebugCore::NormalizeAddress(address);
	if (!valid || !RAMDebugCore::IsRAMAddressValid(address))
	{
		ShowError("Invalid Address");
		return;
	}

	RAMWatchCore::Watch watch;
	watch.Address = address;
	const int sizeIndex = gtk_drop_down_get_selected(GTK_DROP_DOWN(ctx->size_dropdown));
	const int typeIndex = gtk_drop_down_get_selected(GTK_DROP_DOWN(ctx->type_dropdown));
	watch.Size = kSizeCodes[(sizeIndex >= 0 && sizeIndex < 3) ? sizeIndex : 0];
	watch.Type = kTypeCodes[(typeIndex >= 0 && typeIndex < 4) ? typeIndex : 0];
	watch.WrongEndian = false;
	watch.Comment = gtk_editable_get_text(GTK_EDITABLE(ctx->comment_entry));
	watch.CurValue = 0;

	const bool inserted = ctx->editing
		? core.Replace(ctx->index, watch) : core.Insert(watch);
	if (!inserted)
	{
		ShowError((core.Count() >= RAMWatchCore::kMaxWatchCount)
			? "The watch list is full (256 entries)."
			: "A watch with this address, size and type already exists.");
		return;
	}

	core.SetDirty(true);
	rebuild_model();
	gtk_window_destroy(GTK_WINDOW(ctx->window));
}

static void
present_watch_editor(gboolean editing, size_t index)
{
	RAMWatchCore &core = RAMWatchCore::Get();
	WatchEditorCtx *ctx = new WatchEditorCtx();
	ctx->editing = editing;
	ctx->index = index;

	uint32_t address = 0;
	char sizeCode = 'b';
	char typeCode = 's';
	std::string comment;
	if (editing && (index < core.Count()) && !core.IsSeparator(index))
	{
		const RAMWatchCore::Watch &watch = core.At(index);
		address = watch.Address;
		sizeCode = watch.Size;
		typeCode = watch.Type;
		comment = watch.Comment;
	}

	GtkWidget *window = desmume_tool_window_new(
		editing ? "Edit Watch" : "New Watch", sWindow, 420, 220);
	ctx->window = window;

	GtkWidget *root = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
	gtk_widget_set_margin_start(root, 12);
	gtk_widget_set_margin_end(root, 12);
	gtk_widget_set_margin_top(root, 12);
	gtk_widget_set_margin_bottom(root, 12);
	gtk_window_set_child(GTK_WINDOW(window), root);

	GtkWidget *grid = gtk_grid_new();
	gtk_grid_set_row_spacing(GTK_GRID(grid), 6);
	gtk_grid_set_column_spacing(GTK_GRID(grid), 8);
	gtk_box_append(GTK_BOX(root), grid);

	GtkWidget *addressLabel = gtk_label_new("Address:");
	gtk_label_set_xalign(GTK_LABEL(addressLabel), 1.0f);
	gtk_grid_attach(GTK_GRID(grid), addressLabel, 0, 0, 1, 1);

	char *addressText = g_strdup_printf("%08X", (unsigned int)address);
	ctx->address_entry = gtk_entry_new();
	gtk_editable_set_text(GTK_EDITABLE(ctx->address_entry), addressText);
	gtk_editable_set_width_chars(GTK_EDITABLE(ctx->address_entry), 10);
	gtk_widget_add_css_class(ctx->address_entry, "desmume-tool-cell");
	g_free(addressText);
	gtk_grid_attach(GTK_GRID(grid), ctx->address_entry, 1, 0, 1, 1);

	GtkStringList *sizes = gtk_string_list_new(NULL);
	for (int i = 0; i < 3; ++i)
	{
		gtk_string_list_append(sizes, kSizeLabels[i]);
	}
	ctx->size_dropdown = gtk_drop_down_new(G_LIST_MODEL(sizes), NULL);
	gtk_drop_down_set_selected(GTK_DROP_DOWN(ctx->size_dropdown),
	                           (sizeCode == 'w') ? 1 : (sizeCode == 'd') ? 2 : 0);
	g_object_unref(sizes);
	gtk_grid_attach(GTK_GRID(grid), ctx->size_dropdown, 2, 0, 1, 1);

	GtkWidget *typeLabel = gtk_label_new("Display:");
	gtk_label_set_xalign(GTK_LABEL(typeLabel), 1.0f);
	gtk_grid_attach(GTK_GRID(grid), typeLabel, 0, 1, 1, 1);

	GtkStringList *types = gtk_string_list_new(NULL);
	for (int i = 0; i < 4; ++i)
	{
		gtk_string_list_append(types, kTypeLabels[i]);
	}
	ctx->type_dropdown = gtk_drop_down_new(G_LIST_MODEL(types), NULL);
	gtk_drop_down_set_selected(GTK_DROP_DOWN(ctx->type_dropdown),
	                           (typeCode == 'u') ? 1 : (typeCode == 'h') ? 2 : (typeCode == 'f') ? 3 : 0);
	g_object_unref(types);
	gtk_grid_attach(GTK_GRID(grid), ctx->type_dropdown, 1, 1, 2, 1);

	GtkWidget *commentLabel = gtk_label_new("Notes:");
	gtk_label_set_xalign(GTK_LABEL(commentLabel), 1.0f);
	gtk_grid_attach(GTK_GRID(grid), commentLabel, 0, 2, 1, 1);
	ctx->comment_entry = gtk_entry_new();
	gtk_editable_set_text(GTK_EDITABLE(ctx->comment_entry), comment.c_str());
	gtk_grid_attach(GTK_GRID(grid), ctx->comment_entry, 1, 2, 2, 1);

	GtkWidget *buttons = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
	gtk_widget_set_halign(buttons, GTK_ALIGN_END);
	gtk_box_append(GTK_BOX(root), buttons);

	GtkWidget *cancel = gtk_button_new_with_label("_Cancel");
	gtk_button_set_use_underline(GTK_BUTTON(cancel), TRUE);
	g_signal_connect_swapped(cancel, "clicked",
	                         G_CALLBACK(gtk_window_destroy), window);
	gtk_box_append(GTK_BOX(buttons), cancel);

	GtkWidget *ok = gtk_button_new_with_label("_OK");
	gtk_button_set_use_underline(GTK_BUTTON(ok), TRUE);
	gtk_widget_add_css_class(ok, "suggested-action");
	g_signal_connect(ok, "clicked", G_CALLBACK(on_editor_ok), ctx);
	gtk_box_append(GTK_BOX(buttons), ok);

	g_object_set_data_full(G_OBJECT(window), "watch-editor-ctx", ctx, free_editor_ctx);
	g_signal_connect(window, "close-request",
	                 G_CALLBACK(on_editor_close_request), NULL);
	gtk_window_present(GTK_WINDOW(window));
}

/* ------------------------------------------------------------ button handlers */

static void
on_new_watch(GtkButton *button, gpointer data)
{
	(void)button;
	(void)data;
	if (RAMWatchCore::Get().Count() >= RAMWatchCore::kMaxWatchCount)
	{
		return;
	}
	present_watch_editor(FALSE, 0);
}

static void
on_edit_selected(GtkButton *button, gpointer data)
{
	(void)button;
	(void)data;
	const int row = selected_row();
	RAMWatchCore &core = RAMWatchCore::Get();
	if ((row < 0) || ((size_t)row >= core.Count()) || core.IsSeparator((size_t)row))
	{
		return;
	}
	present_watch_editor(TRUE, (size_t)row);
}

static void
on_remove_selected(GtkButton *button, gpointer data)
{
	(void)button;
	(void)data;
	RAMWatchCore &core = RAMWatchCore::Get();
	const std::vector<size_t> rows = selected_rows();
	for (size_t i = rows.size(); i > 0; --i)
	{
		core.Remove(rows[i - 1]);
	}
	core.SetDirty(true);
	rebuild_model();
}

static void
on_duplicate(GtkButton *button, gpointer data)
{
	(void)button;
	(void)data;
	const int row = selected_row();
	if ((row < 0) || ((size_t)row >= RAMWatchCore::Get().Count()))
	{
		return;
	}
	present_watch_editor(FALSE, (size_t)row);
}

static void
on_add_separator(GtkButton *button, gpointer data)
{
	(void)button;
	(void)data;
	RAMWatchCore::Get().AddSeparator();
	RAMWatchCore::Get().SetDirty(true);
	rebuild_model();
}

static void
on_move_up(GtkButton *button, gpointer data)
{
	(void)button;
	(void)data;
	const int row = selected_row();
	RAMWatchCore &core = RAMWatchCore::Get();
	if ((row > 0) && core.MoveUp((size_t)row))
	{
		core.SetDirty(true);
		rebuild_model();
	}
}

static void
on_move_down(GtkButton *button, gpointer data)
{
	(void)button;
	(void)data;
	const int row = selected_row();
	RAMWatchCore &core = RAMWatchCore::Get();
	if ((row >= 0) && core.MoveDown((size_t)row))
	{
		core.SetDirty(true);
		rebuild_model();
	}
}

static void
on_selection_changed(GtkSelectionModel *selection, guint position, guint n_items,
                     gpointer data)
{
	(void)selection;
	(void)position;
	(void)n_items;
	(void)data;
	update_control_states();
}

/* ------------------------------------------------------------ file menu */

static void
on_file_action(GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
	(void)action;
	(void)user_data;

	const char *name = g_action_get_name(G_ACTION(action));
	if (g_strcmp0(name, "new") == 0)
	{
		RAMWatchCore::Get().Clear();
		persist_preferences();
		rebuild_model();
		rebuild_recent_menu();
	}
	else if (g_strcmp0(name, "open") == 0)
	{
		present_open(FALSE);
	}
	else if (g_strcmp0(name, "append") == 0)
	{
		present_open(TRUE);
	}
	else if (g_strcmp0(name, "save") == 0)
	{
		if (!save_watch_list())
		{
			present_save_as();
		}
	}
	else if (g_strcmp0(name, "save-as") == 0)
	{
		present_save_as();
	}
	else if (g_strcmp0(name, "reload-recent") == 0)
	{
		rebuild_recent_menu();
	}
}

static void
on_toggle_autoload(GSimpleAction *action, GVariant *value, gpointer user_data)
{
	(void)user_data;
	g_simple_action_set_state(action, value);
	RAMWatchCore::Get().SetAutoLoad(g_variant_get_boolean(value));
	persist_preferences();
}

static void
on_open_recent(GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
	(void)action;
	(void)user_data;
	const guint64 index = g_variant_get_uint64(parameter);
	std::string error;
	if (!RAMWatchCore::Get().OpenRecentFile((size_t)index, error))
	{
		if (!error.empty())
		{
			ShowError(error.c_str());
		}
		return;
	}
	persist_preferences();
	rebuild_model();
	rebuild_recent_menu();
}

/* ------------------------------------------------------------ DnD */

static gboolean
on_drop(GtkDropTarget *target, const GValue *value, double x, double y,
        gpointer user_data)
{
	(void)target;
	(void)x;
	(void)y;
	(void)user_data;

	if (!G_VALUE_HOLDS(value, GDK_TYPE_FILE_LIST))
	{
		return FALSE;
	}
	GListModel *files = (GListModel *)g_value_get_boxed(value);
	for (guint i = 0; (files != NULL) && (i < g_list_model_get_n_items(files)); ++i)
	{
		GFile *file = G_FILE(g_list_model_get_item(files, i));
		char *path = g_file_get_path(file);
		if ((path != NULL) && is_watch_file_name(path))
		{
			load_watch_file(path, FALSE);
			g_free(path);
			g_object_unref(file);
			return TRUE;
		}
		g_free(path);
		g_object_unref(file);
	}
	return FALSE;
}

/* ------------------------------------------------------------ lifecycle */

static void
clear_state(void)
{
	for (DesmumeRow *row : sRowCache)
	{
		g_object_unref(row);
	}
	sRowCache.clear();
	if (sRows != NULL)
	{
		g_object_unref(sRows);
		sRows = NULL;
	}
	sColumnView = NULL;
	sStatusLabel = NULL;
	sNewButton = NULL;
	sEditButton = NULL;
	sRemoveButton = NULL;
	sDuplicateButton = NULL;
	sSeparatorButton = NULL;
	sMoveUpButton = NULL;
	sMoveDownButton = NULL;
	sRecentMenu = NULL;
	sWindow = NULL;
	sModelRowCount = 0;
}

static void
on_window_close_request(GtkWindow *window, gpointer user_data)
{
	(void)window;
	(void)user_data;

	RAMWatchCore &core = RAMWatchCore::Get();
	if (core.IsDirty())
	{
		/* Best-effort: save the current file if one is associated, otherwise
		 * leave the list in memory.  A full "Save changes?" alert chain is
		 * deferred; closing never loses the in-memory document. */
		if (!core.CurrentFile().empty())
		{
			std::string ignored;
			core.SaveCurrentFile(ignored);
		}
	}
	persist_preferences();
	clear_state();
	gtk_window_destroy(GTK_WINDOW(window));
}

static GtkWidget *
add_button(GtkWidget *box, const char *label, GCallback callback)
{
	GtkWidget *button = gtk_button_new_with_label(label);
	g_signal_connect(button, "clicked", callback, NULL);
	gtk_box_append(GTK_BOX(box), button);
	return button;
}

static void
build_window(void)
{
	sWindow = desmume_tool_window_new(TOOL_NAME, NULL, 640, 460);
	g_object_add_weak_pointer(G_OBJECT(sWindow), (gpointer *)&sWindow);
	g_signal_connect(sWindow, "close-request",
	                 G_CALLBACK(on_window_close_request), NULL);

	GtkWidget *root = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
	gtk_widget_set_margin_start(root, 6);
	gtk_widget_set_margin_end(root, 6);
	gtk_widget_set_margin_top(root, 6);
	gtk_widget_set_margin_bottom(root, 6);
	gtk_window_set_child(GTK_WINDOW(sWindow), root);

	/* ---- watch list ---- */
	sRows = desmume_row_model_new();
	sColumnView = desmume_column_view_new(G_LIST_MODEL(sRows),
	                                       sColumnTitles,
	                                       G_N_ELEMENTS(sColumnTitles));
	GtkSelectionModel *selection = gtk_column_view_get_model(
		GTK_COLUMN_VIEW(sColumnView));
	g_signal_connect(selection, "selection-changed",
	                 G_CALLBACK(on_selection_changed), NULL);

	GtkWidget *scrolled = gtk_scrolled_window_new();
	gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scrolled), sColumnView);
	gtk_widget_set_hexpand(scrolled, TRUE);
	gtk_widget_set_vexpand(scrolled, TRUE);
	gtk_box_append(GTK_BOX(root), scrolled);

	GtkDropTarget *drop = gtk_drop_target_new(GDK_TYPE_FILE_LIST, GDK_ACTION_COPY);
	g_signal_connect(drop, "drop", G_CALLBACK(on_drop), NULL);
	gtk_widget_add_controller(scrolled, GTK_EVENT_CONTROLLER(drop));

	/* ---- side bar ---- */
	GtkWidget *sidebar = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
	gtk_widget_set_size_request(sidebar, 132, -1);
	gtk_box_append(GTK_BOX(root), sidebar);

	/* A local action group gives the File menu real GActions in GTK4; the
	 * popover menu resolves them from the widget hierarchy. */
	GActionEntry entries[] =
	{
		{ "new",    on_file_action, NULL, NULL, NULL },
		{ "open",   on_file_action, NULL, NULL, NULL },
		{ "append", on_file_action, NULL, NULL, NULL },
		{ "save",   on_file_action, NULL, NULL, NULL },
		{ "save-as", on_file_action, NULL, NULL, NULL },
		{ "reload-recent", on_file_action, NULL, NULL, NULL },
		{ "autoload", NULL, NULL, "false", on_toggle_autoload },
		{ "open-recent", on_open_recent, "t", NULL, NULL }
	};
	GActionGroup *group = G_ACTION_GROUP(g_simple_action_group_new());
	g_action_map_add_action_entries(G_ACTION_MAP(group), entries,
	                                G_N_ELEMENTS(entries), NULL);
	gtk_widget_insert_action_group(root, "toolwin", group);
	g_object_unref(group);

	GMenu *fileMenu = g_menu_new();
	g_menu_append(fileMenu, "New List", "toolwin.new");
	g_menu_append(fileMenu, "Open…", "toolwin.open");
	g_menu_append(fileMenu, "Append File…", "toolwin.append");
	sRecentMenu = g_menu_new();
	g_menu_append_submenu(fileMenu, "Open Recent", G_MENU_MODEL(sRecentMenu));
	g_menu_append(fileMenu, "Save", "toolwin.save");
	g_menu_append(fileMenu, "Save As…", "toolwin.save-as");
	g_menu_append(fileMenu, "Auto-load", "toolwin.autoload");

	GtkWidget *fileButton = gtk_menu_button_new();
	gtk_menu_button_set_label(GTK_MENU_BUTTON(fileButton), "File");
	gtk_menu_button_set_menu_model(GTK_MENU_BUTTON(fileButton), G_MENU_MODEL(fileMenu));
	gtk_box_append(GTK_BOX(sidebar), fileButton);
	g_object_unref(fileMenu);
	/* sRecentMenu is owned by fileMenu now; keep our ref and rebuild by
	 * mutating the same GMenu in rebuild_recent_menu(). */

	sNewButton = add_button(sidebar, "New", G_CALLBACK(on_new_watch));
	sEditButton = add_button(sidebar, "Edit", G_CALLBACK(on_edit_selected));
	sRemoveButton = add_button(sidebar, "Remove", G_CALLBACK(on_remove_selected));
	sDuplicateButton = add_button(sidebar, "Duplicate", G_CALLBACK(on_duplicate));
	sSeparatorButton = add_button(sidebar, "Separator", G_CALLBACK(on_add_separator));

	GtkWidget *moveBox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
	sMoveUpButton = add_button(moveBox, "↑", G_CALLBACK(on_move_up));
	sMoveDownButton = add_button(moveBox, "↓", G_CALLBACK(on_move_down));
	gtk_box_append(GTK_BOX(sidebar), moveBox);

	sStatusLabel = gtk_label_new("");
	gtk_label_set_xalign(GTK_LABEL(sStatusLabel), 0.5f);
	gtk_widget_add_css_class(sStatusLabel, "dim-label");
	gtk_box_append(GTK_BOX(sidebar), sStatusLabel);
}

static void
Open(void)
{
	if (sWindow != NULL)
	{
		gtk_window_present(GTK_WINDOW(sWindow));
		return;
	}

	load_preferences();
	build_window();
	rebuild_model();
	rebuild_recent_menu();
	gtk_window_present(GTK_WINDOW(sWindow));

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
				rebuild_recent_menu();
			}
		}
	}
}

static void
Update(void)
{
	if ((sWindow == NULL) || !gtk_widget_get_mapped(sWindow) || (sRows == NULL))
	{
		return;
	}

	RAMWatchCore &core = RAMWatchCore::Get();
	if ((guint)core.Count() != sModelRowCount)
	{
		rebuild_model();
		return;
	}
	if (core.Count() == 0)
	{
		return;
	}
	if (!desmume_emu_has_rom())
	{
		return;
	}

	std::vector<bool> changed;
	if (!core.RefreshValues(changed))
	{
		return;
	}
	for (size_t i = 0; (i < changed.size()) && (i < sRowCache.size()); ++i)
	{
		if (changed[i])
		{
			desmume_row_set(sRowCache[i], 1, core.DisplayValueAt(i).c_str());
		}
	}
}

static void
Close(void)
{
	if (sWindow != NULL)
	{
		gtk_window_destroy(GTK_WINDOW(sWindow));
	}
	clear_state();
}

static gboolean
IsOpen(void)
{
	return sWindow != NULL;
}

const DesmumeTool desmume_tool_ramwatch =
{
	"ramwatch", "RAM Watch", Open, Update, Close, IsOpen
};
