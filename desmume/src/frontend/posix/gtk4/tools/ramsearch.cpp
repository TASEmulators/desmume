/*
	ramsearch.cpp - RAM Search tool for the DeSmuME GTK4 frontend

	Copyright (C) 2026 DeSmuME team

	GTK4 host for the shared RAMSearchCore document model.  The GTK3 version
	relies on GtkTreeView, GtkComboBoxText and gtk_dialog_run; none of those
	exist in GTK4 (or GTK5).  This version uses GtkColumnView via the shared
	rowmodel helper, GtkDropDown, and plain GtkWindows with async callbacks.

	ColumnView only realises the rows on screen, so unlike the GTK3 host the
	50000-result cap is a guard against building too many DesmumeRow objects,
	not a rendering limitation.  The status label still reports the cap so the
	UI stays consistent with the GTK3 tool.
*/

#include "ramsearch.h"
#include "../emulation.h"
#include "rowmodel.h"

#include <string>

#include "../../../shared/ram_debug_core.h"
#include "../../../shared/ram_search_core.h"

#define TOOL_NAME "RAM Search"

static const char *const kSizeLabels[] = { "1 Byte", "2 Bytes", "4 Bytes" };
static const char *const kDisplayLabels[] =
{
	"Signed", "Unsigned", "Hexadecimal", "20.12 Fixed"
};
static const char *const kSearchTypeLabels[] =
{
	"Specific Value",
	"Relative to Previous",
	"Specific Address",
	"Number of Changes"
};
static const char *const kOperatorLabels[] =
{
	"= Equal",
	"!= Not Equal",
	"< Less",
	"> Greater",
	"<= Less or Equal",
	">= Greater or Equal",
	"Different By",
	"Modulo Is"
};

static const char kSizeCodes[3] = { 'b', 'w', 'd' };
static const char kDisplayCodes[4] = { 's', 'u', 'h', 'f' };

static const size_t kMaxDisplayedResults = 50000;

static void on_selection_changed(GtkSelectionModel *selection, guint position,
                                 guint n_items, gpointer data);

static GtkWidget *sWindow = NULL;
static GtkWidget *sColumnView = NULL;
static GListStore *sRows = NULL;
static std::vector<DesmumeRow *> sRowCache;

static GtkWidget *sSearchTypeDropDown = NULL;
static GtkWidget *sOperatorDropDown = NULL;
static GtkWidget *sSizeDropDown = NULL;
static GtkWidget *sDisplayDropDown = NULL;
static GtkWidget *sValueEntry = NULL;
static GtkWidget *sParameterEntry = NULL;
static GtkWidget *sMisalignCheck = NULL;
static GtkWidget *sLittleEndianCheck = NULL;
static GtkWidget *sAutoSearchCheck = NULL;
static GtkWidget *sSearchButton = NULL;
static GtkWidget *sResetButton = NULL;
static GtkWidget *sClearChangesButton = NULL;
static GtkWidget *sUndoButton = NULL;
static GtkWidget *sRedoButton = NULL;
static GtkWidget *sEliminateButton = NULL;
static GtkWidget *sPokeButton = NULL;
static GtkWidget *sAddWatchButton = NULL;
static GtkWidget *sAddCheatButton = NULL;
static GtkWidget *sStatusLabel = NULL;

static const char *const sColumnTitles[] =
{
	"Address", "Value", "Previous", "Changes"
};

static guint sModelRowCount = 0;
static gchar sStatusNote[96] = "";
static gint64 sLastRefreshTime = 0;

static gboolean sAutoSearchEnabled = FALSE;
static gboolean sAutoSearchAutoRetry = FALSE;
static gboolean sInAutoSearchAlert = FALSE;
static gboolean sSyncingAutoSearchCheck = FALSE;

/* ---------------------------------------------------------------- helpers */

static int
active_index(GtkWidget *dropDown)
{
	if (dropDown == NULL)
	{
		return 0;
	}
	const guint index = gtk_drop_down_get_selected(GTK_DROP_DOWN(dropDown));
	return (index == GTK_INVALID_LIST_POSITION) ? 0 : (int)index;
}

static char
selected_size_code(void)
{
	const int index = active_index(sSizeDropDown);
	return kSizeCodes[(index >= 0 && index < 3) ? index : 0];
}

static char
selected_display_code(void)
{
	const int index = active_index(sDisplayDropDown);
	return kDisplayCodes[(index >= 0 && index < 4) ? index : 0];
}

static int32_t
parse_value_entry(GtkWidget *entry)
{
	const char *text = gtk_editable_get_text(GTK_EDITABLE(entry));
	if (text == NULL)
	{
		return 0;
	}
	while (g_ascii_isspace(*text))
	{
		text++;
	}
	if (*text == '\0')
	{
		return 0;
	}

	const gboolean hasHexPrefix = (text[0] == '0') && (text[1] == 'x' || text[1] == 'X');
	const int base = ((selected_display_code() == 'h') || hasHexPrefix) ? 16 : 10;
	return (int32_t)strtol(text, NULL, base);
}

static void
update_parameter_sensitivity(void)
{
	const RAMSearchCore::CompareOperator op =
		(RAMSearchCore::CompareOperator)active_index(sOperatorDropDown);
	const gboolean needed = (op == RAMSearchCore::COMPARE_DIFFERENT_BY) ||
	                        (op == RAMSearchCore::COMPARE_MODULO);
	gtk_widget_set_sensitive(sParameterEntry, needed);
}

static void
update_status_label(void)
{
	if (sStatusLabel == NULL)
	{
		return;
	}
	const size_t count = RAMSearchCore::Get().ResultCount();
	GString *text = g_string_new(NULL);

	if (count > kMaxDisplayedResults)
	{
		g_string_append_printf(text, "%" G_GSIZE_FORMAT " results\nshowing first %" G_GSIZE_FORMAT,
		                       (gsize)count, (gsize)kMaxDisplayedResults);
	}
	else
	{
		g_string_append_printf(text, "%" G_GSIZE_FORMAT " result%s",
		                       (gsize)count, (count == 1) ? "" : "s");
	}
	if (sStatusNote[0] != '\0')
	{
		g_string_append_printf(text, "\n%s", sStatusNote);
	}
	gtk_label_set_text(GTK_LABEL(sStatusLabel), text->str);
	g_string_free(text, TRUE);
}

static int
selected_row(void)
{
	return desmume_column_view_selected(sColumnView, NULL);
}

static void
ShowError(const char *message)
{
	GtkAlertDialog *dialog = gtk_alert_dialog_new(TOOL_NAME);
	gtk_alert_dialog_set_message(dialog, message != NULL ? message : "Operation failed.");
	gtk_alert_dialog_choose(dialog, GTK_WINDOW(sWindow), NULL, NULL, NULL);
	g_object_unref(dialog);
}

static void
update_control_states(void)
{
	const int row = selected_row();
	const RAMSearchCore &core = RAMSearchCore::Get();
	const gboolean hasRow = (row >= 0) && ((size_t)row < core.ResultCount());

	gtk_widget_set_sensitive(sAddWatchButton, hasRow);
	gtk_widget_set_sensitive(sAddCheatButton, hasRow && desmume_emu_has_rom());
	gtk_widget_set_sensitive(sEliminateButton, hasRow);
	gtk_widget_set_sensitive(sPokeButton, hasRow);
	gtk_widget_set_sensitive(sUndoButton, core.CanUndo());
	gtk_widget_set_sensitive(sRedoButton, core.CanRedo());
	gtk_widget_set_sensitive(sClearChangesButton, core.ResultCount() > 0);
}

/* ------------------------------------------------------------ result table */

static void
fill_row(DesmumeRow *row, size_t index)
{
	const RAMSearchCore &core = RAMSearchCore::Get();
	const RAMSearchCore::Result result = core.ResultAt(index);
	const char sizeCode = selected_size_code();
	const char typeCode = selected_display_code();

	const std::string address = RAMDebugCore::FormatAddress(result.address);
	const std::string value = RAMDebugCore::FormatValue(sizeCode, typeCode, result.currentValue);
	const std::string previous = RAMDebugCore::FormatValue(sizeCode, typeCode, result.previousValue);
	gchar *changes = g_strdup_printf("%u", (unsigned int)result.changeCount);

	desmume_row_set(row, 0, address.c_str());
	desmume_row_set(row, 1, value.c_str());
	desmume_row_set(row, 2, previous.c_str());
	desmume_row_set(row, 3, changes);
	g_free(changes);
}

static void
rebuild_model(void)
{
	const RAMSearchCore &core = RAMSearchCore::Get();
	const size_t count = core.ResultCount();
	const size_t rows = (count > kMaxDisplayedResults) ? kMaxDisplayedResults : count;
	const int previousSelection = selected_row();

	for (DesmumeRow *row : sRowCache)
	{
		g_object_unref(row);
	}
	sRowCache.clear();
	if (sRows != NULL)
	{
		g_list_store_remove_all(sRows);
	}

	for (size_t i = 0; i < rows; ++i)
	{
		DesmumeRow *row = desmume_row_new("", "", "", "", "");
		fill_row(row, i);
		sRowCache.push_back(row);
		g_list_store_append(sRows, G_OBJECT(row));
	}
	sModelRowCount = (guint)rows;

	GtkSelectionModel *selection = gtk_column_view_get_model(GTK_COLUMN_VIEW(sColumnView));
	if ((previousSelection >= 0) && ((size_t)previousSelection < rows))
	{
		gtk_selection_model_select_item(selection, (guint)previousSelection, TRUE);
	}

	update_status_label();
	update_control_states();
}

/* Refresh only the rows we realise.  ColumnView is virtualised, but the row
 * objects are shared with the model; limit updates to a screenful around the
 * scroll position so a 50000-row first search does not rewrite every string
 * on every 150 ms tick. */
static void
refresh_visible_values(void)
{
	const RAMSearchCore &core = RAMSearchCore::Get();
	const char sizeCode = selected_size_code();
	const char typeCode = selected_display_code();

	guint first = 0;
	guint last = sModelRowCount > 0 ? sModelRowCount - 1 : 0;

	GtkWidget *parent = gtk_widget_get_parent(sColumnView);
	GtkScrolledWindow *scrolled = (parent != NULL && GTK_IS_SCROLLED_WINDOW(parent))
		? GTK_SCROLLED_WINDOW(parent) : NULL;
	if (scrolled != NULL)
	{
		GtkAdjustment *adjustment = gtk_scrolled_window_get_vadjustment(scrolled);
		if (adjustment != NULL)
		{
			const double rowHeight = 24.0; /* CSS padding + default label height */
			const double value = gtk_adjustment_get_value(adjustment);
			const double pageSize = gtk_adjustment_get_page_size(adjustment);
			first = (guint)(value / rowHeight);
			last = (guint)((value + pageSize) / rowHeight) + 1;
			if (last >= sModelRowCount)
			{
				last = sModelRowCount - 1;
			}
		}
	}

	for (guint i = first; i <= last && i < sModelRowCount; ++i)
	{
		DesmumeRow *row = sRowCache[i];
		if (row == NULL)
		{
			continue;
		}
		const RAMSearchCore::Result result = core.ResultAt((size_t)i);
		const std::string value = RAMDebugCore::FormatValue(sizeCode, typeCode, result.currentValue);
		const std::string previous = RAMDebugCore::FormatValue(sizeCode, typeCode, result.previousValue);
		gchar *changes = g_strdup_printf("%u", (unsigned int)result.changeCount);

		desmume_row_set(row, 1, value.c_str());
		desmume_row_set(row, 2, previous.c_str());
		desmume_row_set(row, 3, changes);
		g_free(changes);
	}
}

/* ----------------------------------------------------------------- options */

static void
sync_options_from_ui(void)
{
	RAMSearchCore &core = RAMSearchCore::Get();

	/* SetSize(), SetMisaligned() and SetLittleEndian() re-enumerate the
	 * candidate list, discarding search progress, just like the GTK3 host. */
	core.SetSize(selected_size_code());
	core.SetMisaligned(gtk_check_button_get_active(GTK_CHECK_BUTTON(sMisalignCheck)));
	core.SetLittleEndian(gtk_check_button_get_active(GTK_CHECK_BUTTON(sLittleEndianCheck)));
	core.SetValueType((RAMSearchCore::ValueType)active_index(sDisplayDropDown));
	core.SetSearchType((RAMSearchCore::SearchType)active_index(sSearchTypeDropDown));
	core.SetCompareOperator((RAMSearchCore::CompareOperator)active_index(sOperatorDropDown));
	core.SetSearchValue(parse_value_entry(sValueEntry));
	core.SetParameter(parse_value_entry(sParameterEntry));
}

static void
on_options_changed(GtkWidget *widget, gpointer data)
{
	(void)widget;
	(void)data;
	update_parameter_sensitivity();
	sync_options_from_ui();
	sStatusNote[0] = '\0';
	rebuild_model();
}

static void
on_operator_changed(GtkWidget *widget, gpointer data)
{
	(void)widget;
	(void)data;
	update_parameter_sensitivity();
	sync_options_from_ui();
}

/* ----------------------------------------------------------------- actions */

static void
on_search(GtkButton *button, gpointer data)
{
	(void)button;
	(void)data;
	sync_options_from_ui();
	RAMSearchCore::Get().Search();
	sStatusNote[0] = '\0';
	rebuild_model();
}

static void
on_reset(GtkButton *button, gpointer data)
{
	(void)button;
	(void)data;
	sync_options_from_ui();
	RAMSearchCore::Get().Reset();
	sStatusNote[0] = '\0';
	rebuild_model();
}

static void
on_clear_changes(GtkButton *button, gpointer data)
{
	(void)button;
	(void)data;
	RAMSearchCore::Get().ClearChangeCounts();
	sStatusNote[0] = '\0';
	rebuild_model();
}

static void
on_undo(GtkButton *button, gpointer data)
{
	(void)button;
	(void)data;
	if (!RAMSearchCore::Get().Undo())
	{
		return;
	}
	GtkSelectionModel *selection = gtk_column_view_get_model(GTK_COLUMN_VIEW(sColumnView));
	gtk_selection_model_unselect_all(selection);
	sStatusNote[0] = '\0';
	rebuild_model();
}

static void
on_redo(GtkButton *button, gpointer data)
{
	(void)button;
	(void)data;
	if (!RAMSearchCore::Get().Redo())
	{
		return;
	}
	GtkSelectionModel *selection = gtk_column_view_get_model(GTK_COLUMN_VIEW(sColumnView));
	gtk_selection_model_unselect_all(selection);
	sStatusNote[0] = '\0';
	rebuild_model();
}

static void
on_eliminate(GtkButton *button, gpointer data)
{
	(void)button;
	(void)data;
	const int row = selected_row();
	if ((row < 0) || !RAMSearchCore::Get().Eliminate((size_t)row))
	{
		return;
	}
	sStatusNote[0] = '\0';
	rebuild_model();

	/* Selecting the row that took the removed one's place keeps a repeated
	 * Eliminate click walking down the list, like the GTK3/macOS hosts. */
	GtkSelectionModel *selection = gtk_column_view_get_model(GTK_COLUMN_VIEW(sColumnView));
	if ((guint)row < RAMSearchCore::Get().ResultCount())
	{
		gtk_selection_model_select_item(selection, (guint)row, TRUE);
	}
}

static void
on_poke(GtkButton *button, gpointer data)
{
	(void)button;
	(void)data;
	const int row = selected_row();
	RAMSearchCore &core = RAMSearchCore::Get();
	if ((row < 0) || ((size_t)row >= core.ResultCount()))
	{
		return;
	}

	const RAMSearchCore::Result result = core.ResultAt((size_t)row);
	const char sizeCode = selected_size_code();
	const uint32_t value = (uint32_t)parse_value_entry(sValueEntry);
	if (!RAMDebugCore::WriteValue(result.address, value, RAMDebugCore::SizeInBytes(sizeCode)))
	{
		ShowError("Could not write that address.");
		return;
	}

	core.SignalNewFrame();
	refresh_visible_values();

	const std::string valueText = RAMDebugCore::FormatValue(sizeCode, selected_display_code(), value);
	g_snprintf(sStatusNote, sizeof(sStatusNote), "poked %s = %s",
	           RAMDebugCore::FormatAddress(result.address).c_str(), valueText.c_str());
	update_status_label();
}

static void
on_add_watch(GtkButton *button, gpointer data)
{
	(void)button;
	(void)data;
	const int row = selected_row();
	RAMSearchCore &core = RAMSearchCore::Get();
	if ((row < 0) || ((size_t)row >= core.ResultCount()))
	{
		return;
	}

	const RAMSearchCore::Result result = core.ResultAt((size_t)row);
	const char typeCode = selected_display_code();
	bool duplicate = false;
	if (core.AddResultToWatchList((size_t)row, typeCode, "", &duplicate))
	{
		g_snprintf(sStatusNote, sizeof(sStatusNote), "added %s to RAM Watch",
		           RAMDebugCore::FormatAddress(result.address).c_str());
	}
	else
	{
		g_snprintf(sStatusNote, sizeof(sStatusNote), "%s is already watched",
		           RAMDebugCore::FormatAddress(result.address).c_str());
	}
	update_status_label();
}

/* --------------------------------------------------------------- add cheat */

struct AddCheatCtx
{
	GtkWidget *window;
	GtkWidget *descriptionEntry;
	uint32_t address;
	int sizeInBytes;
	uint32_t value;
	std::string addressText;
};

static void
free_add_cheat_ctx(gpointer data)
{
	delete (AddCheatCtx *)data;
}

static void
on_add_cheat_ok(GtkButton *button, gpointer user_data)
{
	(void)button;
	AddCheatCtx *ctx = (AddCheatCtx *)user_data;
	const char *description = gtk_editable_get_text(GTK_EDITABLE(ctx->descriptionEntry));

	char *error = NULL;
	const gboolean added = desmume_emu_add_cheat(ctx->address, ctx->sizeInBytes,
	                                             ctx->value, description, &error);
	if (added)
	{
		g_snprintf(sStatusNote, sizeof(sStatusNote), "cheat added for %s",
		           ctx->addressText.c_str());
		update_status_label();
		gtk_window_destroy(GTK_WINDOW(ctx->window));
	}
	else
	{
		ShowError((error != NULL) ? error : "The cheat could not be added.");
	}
	g_free(error);
}

static void
on_add_cheat(GtkButton *button, gpointer data)
{
	(void)button;
	(void)data;
	const int row = selected_row();
	RAMSearchCore &core = RAMSearchCore::Get();
	if ((row < 0) || ((size_t)row >= core.ResultCount()))
	{
		return;
	}
	if (!desmume_emu_has_rom())
	{
		ShowError("Load a ROM before adding cheats.");
		return;
	}

	const RAMSearchCore::Result result = core.ResultAt((size_t)row);
	if ((result.address < RAMDebugCore::kMainMemAddress) ||
	    (result.address >= (RAMDebugCore::kMainMemAddress + RAMDebugCore::kMainMemSize)))
	{
		ShowError("Internal cheats can only target main RAM (0x02000000 - 0x023FFFFF).");
		return;
	}

	/* Take the value as the table shows it, like the GTK3 host, instead of
	 * re-reading the bus with a different endianness convention. */
	const int sizeInBytes = RAMDebugCore::SizeInBytes(selected_size_code());
	const uint32_t sizeMask = (sizeInBytes >= 4) ? 0xFFFFFFFFu : ((1u << (8 * sizeInBytes)) - 1u);
	const uint32_t value = result.currentValue & sizeMask;

	AddCheatCtx *ctx = new AddCheatCtx();
	ctx->address = result.address;
	ctx->sizeInBytes = sizeInBytes;
	ctx->value = value;
	ctx->addressText = RAMDebugCore::FormatAddress(result.address);

	ctx->window = desmume_tool_window_new("Add Cheat", sWindow, 420, 160);
	GtkWidget *root = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
	gtk_widget_set_margin_start(root, 12);
	gtk_widget_set_margin_end(root, 12);
	gtk_widget_set_margin_top(root, 12);
	gtk_widget_set_margin_bottom(root, 12);
	gtk_window_set_child(GTK_WINDOW(ctx->window), root);

	GtkWidget *grid = gtk_grid_new();
	gtk_grid_set_row_spacing(GTK_GRID(grid), 6);
	gtk_grid_set_column_spacing(GTK_GRID(grid), 8);
	gtk_box_append(GTK_BOX(root), grid);

	gchar *addressText = g_strdup_printf("%s = %s (%d byte%s)",
	                                     ctx->addressText.c_str(),
	                                     RAMDebugCore::FormatValue(selected_size_code(),
	                                                               selected_display_code(), value).c_str(),
	                                     sizeInBytes, (sizeInBytes == 1) ? "" : "s");
	GtkWidget *addressLabel = gtk_label_new("Cheat:");
	gtk_label_set_xalign(GTK_LABEL(addressLabel), 1.0f);
	gtk_grid_attach(GTK_GRID(grid), addressLabel, 0, 0, 1, 1);
	GtkWidget *addressValue = gtk_label_new(addressText);
	gtk_label_set_xalign(GTK_LABEL(addressValue), 0.0f);
	gtk_widget_add_css_class(addressValue, "desmume-tool-cell");
	gtk_grid_attach(GTK_GRID(grid), addressValue, 1, 0, 1, 1);
	g_free(addressText);

	GtkWidget *descriptionLabel = gtk_label_new("Description:");
	gtk_label_set_xalign(GTK_LABEL(descriptionLabel), 1.0f);
	gtk_grid_attach(GTK_GRID(grid), descriptionLabel, 0, 1, 1, 1);
	ctx->descriptionEntry = gtk_entry_new();
	const std::string defaultDescription = "RAM Search " + ctx->addressText;
	gtk_editable_set_text(GTK_EDITABLE(ctx->descriptionEntry), defaultDescription.c_str());
	gtk_widget_set_hexpand(ctx->descriptionEntry, TRUE);
	gtk_grid_attach(GTK_GRID(grid), ctx->descriptionEntry, 1, 1, 1, 1);

	GtkWidget *hint = gtk_label_new("Constant write, added to the cheat list and saved with the ROM.");
	gtk_label_set_xalign(GTK_LABEL(hint), 0.0f);
	gtk_widget_add_css_class(hint, "dim-label");
	gtk_grid_attach(GTK_GRID(grid), hint, 1, 2, 1, 1);

	GtkWidget *buttons = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
	gtk_widget_set_halign(buttons, GTK_ALIGN_END);
	gtk_box_append(GTK_BOX(root), buttons);

	GtkWidget *cancel = gtk_button_new_with_label("_Cancel");
	gtk_button_set_use_underline(GTK_BUTTON(cancel), TRUE);
	g_signal_connect_swapped(cancel, "clicked", G_CALLBACK(gtk_window_destroy), ctx->window);
	gtk_box_append(GTK_BOX(buttons), cancel);

	GtkWidget *ok = gtk_button_new_with_label("_Add");
	gtk_button_set_use_underline(GTK_BUTTON(ok), TRUE);
	gtk_widget_add_css_class(ok, "suggested-action");
	g_signal_connect(ok, "clicked", G_CALLBACK(on_add_cheat_ok), ctx);
	gtk_box_append(GTK_BOX(buttons), ok);

	g_object_set_data_full(G_OBJECT(ctx->window), "add-cheat-ctx", ctx, free_add_cheat_ctx);
	gtk_window_present(GTK_WINDOW(ctx->window));
}

/* -------------------------------------------------------------- auto-search */

static void
stop_auto_search(void)
{
	sSyncingAutoSearchCheck = TRUE;
	gtk_check_button_set_active(GTK_CHECK_BUTTON(sAutoSearchCheck), FALSE);
	sSyncingAutoSearchCheck = FALSE;
	sAutoSearchEnabled = FALSE;
	sAutoSearchAutoRetry = FALSE;
	sStatusNote[0] = '\0';
	update_status_label();
}

/* Async replacement for the GTK3 three-button nested dialog. */
static void
on_auto_exhausted_choose(GObject *source, GAsyncResult *result, gpointer data)
{
	(void)data;
	GtkAlertDialog *dialog = GTK_ALERT_DIALOG(source);
	const int response = gtk_alert_dialog_choose_finish(dialog, result, NULL);
	g_object_unref(dialog);
	sInAutoSearchAlert = FALSE;

	if ((response < 0) || (response == 0)) /* Stop / dismissed */
	{
		stop_auto_search();
		return;
	}
	if (response == 1) /* Ignore: reset from now on without asking */
	{
		sAutoSearchAutoRetry = TRUE;
	}

	RAMSearchCore::Get().Reset();
	rebuild_model();
}

static void
handle_auto_search_exhausted(void)
{
	RAMSearchCore &core = RAMSearchCore::Get();
	if (sAutoSearchAutoRetry)
	{
		core.Reset();
		rebuild_model();
		return;
	}
	if (sInAutoSearchAlert)
	{
		return;
	}
	sInAutoSearchAlert = TRUE;

	GtkAlertDialog *dialog = gtk_alert_dialog_new("Auto-search is out of results.");
	gtk_alert_dialog_set_message(dialog,
		"Retry resets the search once and keeps auto-searching. "
		"Ignore resets whenever necessary without asking again. "
		"Stop turns auto-search off.");
	const char *const labels[] = { "Stop", "Ignore", "Retry" };
	gtk_alert_dialog_set_buttons(dialog, labels);
	gtk_alert_dialog_set_default_button(dialog, 2);
	gtk_alert_dialog_choose(dialog, GTK_WINDOW(sWindow), NULL,
	                        on_auto_exhausted_choose, NULL);
	g_object_unref(dialog);
}

static void
run_auto_search_step(void)
{
	RAMSearchCore &core = RAMSearchCore::Get();
	if (core.ResultCount() == 0)
	{
		handle_auto_search_exhausted();
		return;
	}

	sync_options_from_ui();
	const size_t before = core.ResultCount();
	core.Search();
	if (core.ResultCount() == before)
	{
		refresh_visible_values();
		return;
	}
	rebuild_model();
}

static void
on_auto_search_toggled(GtkCheckButton *button, gpointer data)
{
	(void)data;
	if (sSyncingAutoSearchCheck)
	{
		return;
	}
	sAutoSearchEnabled = gtk_check_button_get_active(button);
	sAutoSearchAutoRetry = FALSE;
	if (sAutoSearchEnabled)
	{
		on_search(NULL, NULL);
	}
	else
	{
		sStatusNote[0] = '\0';
		update_status_label();
	}
}

/* ------------------------------------------------------------ window helpers */

static GtkWidget *
add_label(GtkGrid *grid, const char *text, int row)
{
	GtkWidget *label = gtk_label_new(text);
	gtk_label_set_xalign(GTK_LABEL(label), 0.0f);
	gtk_grid_attach(grid, label, 0, row, 1, 1);
	return label;
}

static GtkWidget *
add_dropdown(GtkWidget *grid, int row, const char *const *labels, int count)
{
	GtkStringList *strings = gtk_string_list_new(NULL);
	for (int i = 0; i < count; ++i)
	{
		gtk_string_list_append(strings, labels[i]);
	}
	GtkWidget *dropdown = gtk_drop_down_new(G_LIST_MODEL(strings), NULL);
	gtk_widget_set_hexpand(dropdown, TRUE);
	gtk_grid_attach(GTK_GRID(grid), dropdown, 1, row, 1, 1);
	g_object_unref(strings);
	return dropdown;
}

static GtkWidget *
button_row(GtkWidget *left, GtkWidget *right)
{
	GtkWidget *row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
	gtk_box_set_homogeneous(GTK_BOX(row), TRUE);
	gtk_box_append(GTK_BOX(row), left);
	gtk_box_append(GTK_BOX(row), right);
	return row;
}

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
	sSearchTypeDropDown = NULL;
	sOperatorDropDown = NULL;
	sSizeDropDown = NULL;
	sDisplayDropDown = NULL;
	sValueEntry = NULL;
	sParameterEntry = NULL;
	sMisalignCheck = NULL;
	sLittleEndianCheck = NULL;
	sAutoSearchCheck = NULL;
	sSearchButton = NULL;
	sResetButton = NULL;
	sClearChangesButton = NULL;
	sUndoButton = NULL;
	sRedoButton = NULL;
	sEliminateButton = NULL;
	sPokeButton = NULL;
	sAddWatchButton = NULL;
	sAddCheatButton = NULL;
	sStatusLabel = NULL;
	sWindow = NULL;
	sModelRowCount = 0;
	sStatusNote[0] = '\0';
	sAutoSearchEnabled = FALSE;
	sAutoSearchAutoRetry = FALSE;
	sInAutoSearchAlert = FALSE;
	sSyncingAutoSearchCheck = FALSE;
}

static void
on_window_close_request(GtkWindow *window, gpointer user_data)
{
	(void)window;
	(void)user_data;
	clear_state();
	gtk_window_destroy(GTK_WINDOW(window));
}

static void
build_window(void)
{
	sWindow = desmume_tool_window_new(TOOL_NAME, NULL, 880, 640);
	g_object_add_weak_pointer(G_OBJECT(sWindow), (gpointer *)&sWindow);
	g_signal_connect(sWindow, "close-request",
	                 G_CALLBACK(on_window_close_request), NULL);

	GtkWidget *root = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
	gtk_widget_set_margin_start(root, 6);
	gtk_widget_set_margin_end(root, 6);
	gtk_widget_set_margin_top(root, 6);
	gtk_widget_set_margin_bottom(root, 6);
	gtk_window_set_child(GTK_WINDOW(sWindow), root);

	/* ---- result table ---- */
	sRows = desmume_row_model_new();
	sColumnView = desmume_column_view_new(G_LIST_MODEL(sRows),
	                                      sColumnTitles,
	                                      G_N_ELEMENTS(sColumnTitles));
	GtkSelectionModel *selection = gtk_column_view_get_model(GTK_COLUMN_VIEW(sColumnView));

	GtkWidget *scrolled = gtk_scrolled_window_new();
	gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scrolled), sColumnView);
	gtk_widget_set_hexpand(scrolled, TRUE);
	gtk_widget_set_vexpand(scrolled, TRUE);
	gtk_box_append(GTK_BOX(root), scrolled);

	/* ---- search controls ---- */
	GtkWidget *sidebar = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
	gtk_widget_set_size_request(sidebar, 250, -1);
	gtk_widget_set_hexpand(sidebar, FALSE);
	gtk_box_append(GTK_BOX(root), sidebar);

	GtkWidget *grid = gtk_grid_new();
	gtk_grid_set_row_spacing(GTK_GRID(grid), 6);
	gtk_grid_set_column_spacing(GTK_GRID(grid), 8);
	gtk_box_append(GTK_BOX(sidebar), grid);

	add_label(GTK_GRID(grid), "Search Type", 0);
	sSearchTypeDropDown = add_dropdown(grid, 0, kSearchTypeLabels, 4);
	add_label(GTK_GRID(grid), "Comparison", 1);
	sOperatorDropDown = add_dropdown(grid, 1, kOperatorLabels, 8);

	add_label(GTK_GRID(grid), "Value", 2);
	sValueEntry = gtk_entry_new();
	gtk_editable_set_text(GTK_EDITABLE(sValueEntry), "0");
	gtk_grid_attach(GTK_GRID(grid), sValueEntry, 1, 2, 1, 1);

	add_label(GTK_GRID(grid), "Parameter", 3);
	sParameterEntry = gtk_entry_new();
	gtk_editable_set_text(GTK_EDITABLE(sParameterEntry), "0");
	gtk_grid_attach(GTK_GRID(grid), sParameterEntry, 1, 3, 1, 1);

	add_label(GTK_GRID(grid), "Size", 4);
	sSizeDropDown = add_dropdown(grid, 4, kSizeLabels, 3);
	add_label(GTK_GRID(grid), "Display", 5);
	sDisplayDropDown = add_dropdown(grid, 5, kDisplayLabels, 4);

	sMisalignCheck = gtk_check_button_new_with_label("Allow misaligned addresses");
	sLittleEndianCheck = gtk_check_button_new_with_label("Little endian values");
	sAutoSearchCheck = gtk_check_button_new_with_label("Auto-search");
	gtk_widget_set_tooltip_text(sAutoSearchCheck,
		"Keep applying the current criteria as the game runs");
	gtk_box_append(GTK_BOX(sidebar), sMisalignCheck);
	gtk_box_append(GTK_BOX(sidebar), sLittleEndianCheck);
	gtk_box_append(GTK_BOX(sidebar), sAutoSearchCheck);
	gtk_box_append(GTK_BOX(sidebar), gtk_separator_new(GTK_ORIENTATION_HORIZONTAL));

	sSearchButton = gtk_button_new_with_label("Search");
	sResetButton = gtk_button_new_with_label("Reset");
	sClearChangesButton = gtk_button_new_with_label("Clear Changes");
	sUndoButton = gtk_button_new_with_label("Undo");
	sRedoButton = gtk_button_new_with_label("Redo");
	sEliminateButton = gtk_button_new_with_label("Eliminate");
	sPokeButton = gtk_button_new_with_label("Poke");
	sAddWatchButton = gtk_button_new_with_label("Add Watch");
	sAddCheatButton = gtk_button_new_with_label("Add Cheat");

	gtk_box_append(GTK_BOX(sidebar), button_row(sSearchButton, sResetButton));
	gtk_box_append(GTK_BOX(sidebar), sClearChangesButton);
	gtk_box_append(GTK_BOX(sidebar), button_row(sUndoButton, sRedoButton));
	gtk_box_append(GTK_BOX(sidebar), button_row(sEliminateButton, sPokeButton));
	gtk_box_append(GTK_BOX(sidebar), button_row(sAddWatchButton, sAddCheatButton));

	sStatusLabel = gtk_label_new("");
	gtk_label_set_justify(GTK_LABEL(sStatusLabel), GTK_JUSTIFY_CENTER);
	gtk_label_set_xalign(GTK_LABEL(sStatusLabel), 0.5f);
	gtk_widget_add_css_class(sStatusLabel, "dim-label");
	gtk_box_append(GTK_BOX(sidebar), sStatusLabel);

	/* Restore the core's current options into the widgets before connecting
	 * handlers so "notify::selected" does not re-enumerate candidates. */
	RAMSearchCore &core = RAMSearchCore::Get();
	gtk_drop_down_set_selected(GTK_DROP_DOWN(sSearchTypeDropDown), (guint)core.GetSearchType());
	gtk_drop_down_set_selected(GTK_DROP_DOWN(sOperatorDropDown), (guint)core.GetCompareOperator());
	gtk_drop_down_set_selected(GTK_DROP_DOWN(sDisplayDropDown), (guint)core.GetValueType());
	gtk_drop_down_set_selected(GTK_DROP_DOWN(sSizeDropDown),
	                           (core.Size() == 'w') ? 1 : (core.Size() == 'd') ? 2 : 0);
	gtk_check_button_set_active(GTK_CHECK_BUTTON(sMisalignCheck), core.Misaligned());
	gtk_check_button_set_active(GTK_CHECK_BUTTON(sLittleEndianCheck), core.LittleEndian());

	g_signal_connect(sSearchTypeDropDown, "notify::selected", G_CALLBACK(on_options_changed), NULL);
	g_signal_connect(sOperatorDropDown, "notify::selected", G_CALLBACK(on_operator_changed), NULL);
	g_signal_connect(sSizeDropDown, "notify::selected", G_CALLBACK(on_options_changed), NULL);
	g_signal_connect(sDisplayDropDown, "notify::selected", G_CALLBACK(on_options_changed), NULL);
	g_signal_connect(sMisalignCheck, "toggled", G_CALLBACK(on_options_changed), NULL);
	g_signal_connect(sLittleEndianCheck, "toggled", G_CALLBACK(on_options_changed), NULL);
	g_signal_connect(sAutoSearchCheck, "toggled", G_CALLBACK(on_auto_search_toggled), NULL);
	g_signal_connect(sSearchButton, "clicked", G_CALLBACK(on_search), NULL);
	g_signal_connect(sResetButton, "clicked", G_CALLBACK(on_reset), NULL);
	g_signal_connect(sClearChangesButton, "clicked", G_CALLBACK(on_clear_changes), NULL);
	g_signal_connect(sUndoButton, "clicked", G_CALLBACK(on_undo), NULL);
	g_signal_connect(sRedoButton, "clicked", G_CALLBACK(on_redo), NULL);
	g_signal_connect(sEliminateButton, "clicked", G_CALLBACK(on_eliminate), NULL);
	g_signal_connect(sPokeButton, "clicked", G_CALLBACK(on_poke), NULL);
	g_signal_connect(sAddWatchButton, "clicked", G_CALLBACK(on_add_watch), NULL);
	g_signal_connect(sAddCheatButton, "clicked", G_CALLBACK(on_add_cheat), NULL);
	g_signal_connect(selection, "selection-changed",
	                 G_CALLBACK(on_selection_changed), NULL);
	g_signal_connect(sValueEntry, "activate", G_CALLBACK(on_search), NULL);
	g_signal_connect(sParameterEntry, "activate", G_CALLBACK(on_search), NULL);

	update_parameter_sensitivity();
}

/* ------------------------------------------------------------ lifecycle */

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

static void
Open(void)
{
	if (sWindow != NULL)
	{
		gtk_window_present(GTK_WINDOW(sWindow));
		return;
	}

	sLastRefreshTime = 0;
	sStatusNote[0] = '\0';
	build_window();

	/* First open has nothing to show, so enumerate every candidate; later
	 * opens keep the previous search result. */
	if (RAMSearchCore::Get().ResultCount() == 0)
	{
		sync_options_from_ui();
		RAMSearchCore::Get().Reset();
	}
	rebuild_model();
	gtk_window_present(GTK_WINDOW(sWindow));
}

static void
Update(void)
{
	if ((sWindow == NULL) || !gtk_widget_get_mapped(sWindow) || (sRows == NULL))
	{
		return;
	}
	if (sInAutoSearchAlert)
	{
		/* The alert is async; skip frame work while it is up so searches do
		 * not stack up behind the user's decision. */
		return;
	}

	RAMSearchCore &core = RAMSearchCore::Get();
	const size_t count = core.ResultCount();
	if ((count == 0) && !sAutoSearchEnabled)
	{
		return;
	}

	/* SignalNewFrame() walks every candidate (four million right after a
	 * Reset), so repaints are throttled like the GTK3/macOS hosts: every
	 * 150 ms normally, 900 ms for very large result sets. */
	const gint64 interval = (count > 1000000) ? 900000 : 150000;
	const gint64 now = g_get_monotonic_time();
	if ((now - sLastRefreshTime) < interval)
	{
		return;
	}
	sLastRefreshTime = now;

	core.SignalNewFrame();
	if (sAutoSearchEnabled)
	{
		run_auto_search_step();
	}
	else
	{
		refresh_visible_values();
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

const DesmumeTool desmume_tool_ramsearch =
{
	"ramsearch", "RAM Search", Open, Update, Close, IsOpen
};
