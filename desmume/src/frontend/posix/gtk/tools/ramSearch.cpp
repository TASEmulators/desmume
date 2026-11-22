/* ramSearch.cpp - this file is part of DeSmuME
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

// GTK3 host for RAM Search.
//
// The engine lives in ../../../shared/ram_search_core.cpp and is shared
// with the Cocoa port: it walks main RAM, ITCM and DTCM, keeps per-frame
// value snapshots and change counts, and filters the candidate list with the
// same categories and comparison operators as the Windows dialog. This file
// only builds the controls, forwards button presses and repaints.
//
// Unlike NSTableView, GtkTreeView materialises every row of its model, so a
// first search over ~4.2 million candidates cannot be pushed straight into a
// GtkListStore. The table therefore shows at most kMaxDisplayedResults rows and
// says so in the status label; per-frame repaints only touch the rows that are
// actually on screen.

#include <gtk/gtk.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <string>

#include "../dTool.h"
#include "../../shared/gtk_monospace_font.h"
#include "../../../shared/ram_debug_core.h"
#include "../../../shared/ram_search_core.h"

// Internal cheats ("Add Cheat") are handed to the emulator's cheat list, which
// the frontend shares with the Cheat List window.
#include "../../../../cheatSystem.h"

#define SHORTNAME "ramsearch"
#define TOOL_NAME "RAM Search"

enum
{
	COL_ADDRESS = 0,
	COL_VALUE,
	COL_PREVIOUS,
	COL_CHANGES,
	COLUMN_COUNT
};

static const char kSizeLabels[3][12] = { "1 Byte", "2 Bytes", "4 Bytes" };
static const char kDisplayLabels[4][16] = { "Signed", "Unsigned", "Hexadecimal", "20.12 Fixed" };
static const char kSearchTypeLabels[4][24] =
{
	"Specific Value",
	"Relative to Previous",
	"Specific Address",
	"Number of Changes"
};
static const char kOperatorLabels[8][24] =
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

// A first search leaves ~4.2 million candidates. GtkTreeView builds a row for
// every model entry, so only the first slice is displayed; the status label
// reports how many were hidden. Any refinement drops well below this.
static const size_t kMaxDisplayedResults = 50000;

static GtkWidget *sWindow = NULL;
static GtkWidget *sTreeView = NULL;
static GtkListStore *sStore = NULL;

static GtkWidget *sSearchTypeCombo = NULL;
static GtkWidget *sOperatorCombo = NULL;
static GtkWidget *sSizeCombo = NULL;
static GtkWidget *sDisplayCombo = NULL;
static GtkWidget *sValueEntry = NULL;
static GtkWidget *sParameterEntry = NULL;
static GtkWidget *sMisalignButton = NULL;
static GtkWidget *sLittleEndianButton = NULL;
static GtkWidget *sSearchButton = NULL;
static GtkWidget *sResetButton = NULL;
static GtkWidget *sClearChangesButton = NULL;
static GtkWidget *sUndoButton = NULL;
static GtkWidget *sRedoButton = NULL;
static GtkWidget *sEliminateButton = NULL;
static GtkWidget *sPokeButton = NULL;
static GtkWidget *sAddWatchButton = NULL;
static GtkWidget *sAddCheatButton = NULL;
static GtkWidget *sAutoSearchButton = NULL;
static GtkWidget *sStatusLabel = NULL;

static int sToolID = -1;

// Rows currently in the model, so per-frame repaints never run past the end.
static guint sModelRows = 0;

// Transient feedback shown under the result count (a poke, a cheat, ...).
static char sStatusNote[96] = "";

// Timestamp of the last per-frame refresh (microseconds, monotonic).
static gint64 sLastRefreshTime = 0;

// Auto-search re-applies the current criteria on every refresh tick, like the
// Windows dialog and the macOS window. When a pass empties the list the user is
// asked whether to retry, retry silently from now on, or stop.
static gboolean sAutoSearchEnabled = FALSE;
static gboolean sAutoSearchAutoRetry = FALSE;
static gboolean sInAutoSearchAlert = FALSE;
static gboolean sSyncingAutoSearchButton = FALSE;

enum
{
	RESPONSE_RETRY = 1,
	RESPONSE_IGNORE = 2,
	RESPONSE_STOP = 3
};

/////////////////////////////// HELPERS ///////////////////////////////

static gint active_index(GtkWidget *combo)
{
	const gint index = gtk_combo_box_get_active(GTK_COMBO_BOX(combo));
	return (index < 0) ? 0 : index;
}

static char selected_size_code(void)
{
	return kSizeCodes[(active_index(sSizeCombo) < 3) ? active_index(sSizeCombo) : 0];
}

static char selected_display_code(void)
{
	return kDisplayCodes[(active_index(sDisplayCombo) < 4) ? active_index(sDisplayCombo) : 0];
}

// Mirrors the macOS host: hex is assumed when the display type is hexadecimal
// or when the text carries an explicit 0x prefix.
static int32_t parse_value_entry(GtkWidget *entry)
{
	const char *text = gtk_entry_get_text(GTK_ENTRY(entry));
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

static void update_parameter_sensitivity(void)
{
	const RAMSearchCore::CompareOperator op =
		(RAMSearchCore::CompareOperator)active_index(sOperatorCombo);
	const gboolean needed = (op == RAMSearchCore::COMPARE_DIFFERENT_BY) ||
	                        (op == RAMSearchCore::COMPARE_MODULO);

	gtk_widget_set_sensitive(sParameterEntry, needed);
}

static void update_status_label(void)
{
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

static int selected_row(void)
{
	GtkTreeSelection *selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(sTreeView));
	GList *rows = gtk_tree_selection_get_selected_rows(selection, NULL);
	int row = -1;

	if (rows != NULL)
	{
		gint *indices = gtk_tree_path_get_indices((GtkTreePath *)rows->data);
		if (indices != NULL)
		{
			row = indices[0];
		}
	}
	g_list_free_full(rows, (GDestroyNotify)gtk_tree_path_free);

	return row;
}

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

static void update_control_states(void)
{
	const int row = selected_row();
	const RAMSearchCore &core = RAMSearchCore::Get();
	const gboolean hasRow = (row >= 0) && ((size_t)row < core.ResultCount());

	gtk_widget_set_sensitive(sAddWatchButton, hasRow);
	gtk_widget_set_sensitive(sAddCheatButton, hasRow);
	gtk_widget_set_sensitive(sEliminateButton, hasRow);
	gtk_widget_set_sensitive(sPokeButton, hasRow);
	gtk_widget_set_sensitive(sUndoButton, core.CanUndo());
	gtk_widget_set_sensitive(sRedoButton, core.CanRedo());
	gtk_widget_set_sensitive(sClearChangesButton, core.ResultCount() > 0);
}

/////////////////////////////// RESULT TABLE ///////////////////////////////

static void fill_row(GtkTreeIter *iter, size_t index)
{
	const RAMSearchCore &core = RAMSearchCore::Get();
	const RAMSearchCore::Result result = core.ResultAt(index);
	const char sizeCode = selected_size_code();
	const char typeCode = selected_display_code();

	const std::string address = RAMDebugCore::FormatAddress(result.address);
	const std::string value = RAMDebugCore::FormatValue(sizeCode, typeCode, result.currentValue);
	const std::string previous = RAMDebugCore::FormatValue(sizeCode, typeCode, result.previousValue);
	gchar *changes = g_strdup_printf("%u", (unsigned int)result.changeCount);

	gtk_list_store_set(sStore, iter,
	                   COL_ADDRESS, address.c_str(),
	                   COL_VALUE, value.c_str(),
	                   COL_PREVIOUS, previous.c_str(),
	                   COL_CHANGES, changes,
	                   -1);
	g_free(changes);
}

static void rebuild_model(void)
{
	const RAMSearchCore &core = RAMSearchCore::Get();
	const size_t count = core.ResultCount();
	const size_t rows = (count > kMaxDisplayedResults) ? kMaxDisplayedResults : count;

	// gtk_list_store_clear() drops the selection, and most actions that rebuild
	// (clear change counts, a search) should not also lose the current row.
	const gint previousSelection = selected_row();

	gtk_list_store_clear(sStore);
	for (size_t i = 0; i < rows; i++)
	{
		GtkTreeIter iter;
		gtk_list_store_append(sStore, &iter);
		fill_row(&iter, i);
	}
	sModelRows = (guint)rows;

	if ((previousSelection >= 0) && ((size_t)previousSelection < rows))
	{
		GtkTreeSelection *selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(sTreeView));
		GtkTreePath *path = gtk_tree_path_new_from_indices(previousSelection, -1);
		gtk_tree_selection_select_path(selection, path);
		gtk_tree_path_free(path);
	}

	update_status_label();
	update_control_states();
}

// Repaints only the rows on screen. GtkListStore has no O(1) row access, so
// the iter is fetched once and then advanced.
static void refresh_visible_values(void)
{
	const RAMSearchCore &core = RAMSearchCore::Get();
	const char sizeCode = selected_size_code();
	const char typeCode = selected_display_code();

	int first = 0;
	int last = 0;

	GtkTreePath *startPath = NULL;
	GtkTreePath *endPath = NULL;
	if (gtk_tree_view_get_visible_range(GTK_TREE_VIEW(sTreeView), &startPath, &endPath))
	{
		gint *startIndices = gtk_tree_path_get_indices(startPath);
		gint *endIndices = gtk_tree_path_get_indices(endPath);
		first = (startIndices != NULL) ? startIndices[0] : 0;
		last = (endIndices != NULL) ? endIndices[0] : first;
	}
	else
	{
		// Window collapsed or not mapped yet: a screenful from the top is
		// enough to stay correct once it is shown.
		last = 32;
	}

	if (startPath != NULL)
	{
		gtk_tree_path_free(startPath);
	}
	if (endPath != NULL)
	{
		gtk_tree_path_free(endPath);
	}

	if (first < 0)
	{
		first = 0;
	}
	if (last < first)
	{
		last = first;
	}
	if (sModelRows == 0)
	{
		return;
	}
	if ((guint)last >= sModelRows)
	{
		last = (int)sModelRows - 1;
	}
	if (last < first)
	{
		return;
	}

	GtkTreeIter iter;
	if (!gtk_tree_model_iter_nth_child(GTK_TREE_MODEL(sStore), &iter, NULL, first))
	{
		return;
	}

	for (int row = first; row <= last; row++)
	{
		const RAMSearchCore::Result result = core.ResultAt((size_t)row);
		const std::string value = RAMDebugCore::FormatValue(sizeCode, typeCode, result.currentValue);
		const std::string previous = RAMDebugCore::FormatValue(sizeCode, typeCode, result.previousValue);
		gchar *changes = g_strdup_printf("%u", (unsigned int)result.changeCount);

		gtk_list_store_set(sStore, &iter,
		                   COL_VALUE, value.c_str(),
		                   COL_PREVIOUS, previous.c_str(),
		                   COL_CHANGES, changes,
		                   -1);
		g_free(changes);

		if (!gtk_tree_model_iter_next(GTK_TREE_MODEL(sStore), &iter))
		{
			break;
		}
	}
}

/////////////////////////////// OPTIONS ///////////////////////////////

static void sync_options_from_ui(void)
{
	RAMSearchCore &core = RAMSearchCore::Get();

	// SetSize(), SetMisaligned() and SetLittleEndian() re-enumerate the
	// candidate list, discarding the current search progress. That is the same
	// behaviour as the macOS host: changing them starts a new search.
	core.SetSize(selected_size_code());
	core.SetMisaligned(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(sMisalignButton)));
	core.SetLittleEndian(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(sLittleEndianButton)));
	core.SetValueType((RAMSearchCore::ValueType)active_index(sDisplayCombo));
	core.SetSearchType((RAMSearchCore::SearchType)active_index(sSearchTypeCombo));
	core.SetCompareOperator((RAMSearchCore::CompareOperator)active_index(sOperatorCombo));
	core.SetSearchValue(parse_value_entry(sValueEntry));
	core.SetParameter(parse_value_entry(sParameterEntry));
}

static void on_options_changed(GtkWidget *widget, gpointer data)
{
	update_parameter_sensitivity();
	sync_options_from_ui();
	sStatusNote[0] = '\0';
	rebuild_model();
}

static void on_operator_changed(GtkWidget *widget, gpointer data)
{
	update_parameter_sensitivity();
	sync_options_from_ui();
}

/////////////////////////////// ACTIONS ///////////////////////////////

static void on_search(GtkButton *button, gpointer data)
{
	sync_options_from_ui();

	RAMSearchCore &core = RAMSearchCore::Get();
	core.Search();
	sStatusNote[0] = '\0';
	rebuild_model();
}

static void on_reset(GtkButton *button, gpointer data)
{
	sync_options_from_ui();

	RAMSearchCore &core = RAMSearchCore::Get();
	core.Reset();
	sStatusNote[0] = '\0';
	rebuild_model();
}

static void on_add_watch(GtkButton *button, gpointer data)
{
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

static void on_clear_changes(GtkButton *button, gpointer data)
{
	RAMSearchCore::Get().ClearChangeCounts();
	sStatusNote[0] = '\0';
	rebuild_model();
}

// Selecting the row that takes the place of the removed one keeps a repeated
// Eliminate click walking down the list, like the macOS window.
static void select_row_if_possible(size_t row)
{
	const RAMSearchCore &core = RAMSearchCore::Get();
	if (row >= core.ResultCount())
	{
		return;
	}

	GtkTreeSelection *selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(sTreeView));
	GtkTreePath *path = gtk_tree_path_new_from_indices((gint)row, -1);
	gtk_tree_selection_select_path(selection, path);
	gtk_tree_view_scroll_to_cell(GTK_TREE_VIEW(sTreeView), path, NULL, FALSE, 0.0f, 0.0f);
	gtk_tree_path_free(path);
}

static void on_undo(GtkButton *button, gpointer data)
{
	RAMSearchCore &core = RAMSearchCore::Get();
	if (!core.Undo())
	{
		return;
	}

	GtkTreeSelection *selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(sTreeView));
	gtk_tree_selection_unselect_all(selection);
	sStatusNote[0] = '\0';
	rebuild_model();
}

static void on_redo(GtkButton *button, gpointer data)
{
	RAMSearchCore &core = RAMSearchCore::Get();
	if (!core.Redo())
	{
		return;
	}

	GtkTreeSelection *selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(sTreeView));
	gtk_tree_selection_unselect_all(selection);
	sStatusNote[0] = '\0';
	rebuild_model();
}

static void on_eliminate(GtkButton *button, gpointer data)
{
	const int row = selected_row();
	RAMSearchCore &core = RAMSearchCore::Get();

	if ((row < 0) || !core.Eliminate((size_t)row))
	{
		return;
	}

	sStatusNote[0] = '\0';
	rebuild_model();
	select_row_if_possible((size_t)row);
}

static void on_poke(GtkButton *button, gpointer data)
{
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
		gdk_display_beep(gtk_widget_get_display(sWindow));
		return;
	}

	// Refresh right away so the write is visible without waiting for the next
	// throttled tick.
	core.SignalNewFrame();
	refresh_visible_values();

	const std::string valueText = RAMDebugCore::FormatValue(sizeCode, selected_display_code(), value);
	g_snprintf(sStatusNote, sizeof(sStatusNote), "poked %s = %s",
	           RAMDebugCore::FormatAddress(result.address).c_str(), valueText.c_str());
	update_status_label();
}

static void on_add_cheat(GtkButton *button, gpointer data)
{
	const int row = selected_row();
	RAMSearchCore &core = RAMSearchCore::Get();

	if ((row < 0) || ((size_t)row >= core.ResultCount()))
	{
		return;
	}
	if (cheats == NULL)
	{
		show_error("Load a ROM before adding cheats.");
		return;
	}

	const RAMSearchCore::Result result = core.ResultAt((size_t)row);

	// Internal cheats are Action Replay constant writes into main RAM, so any
	// other address would silently freeze the wrong memory. Same restriction as
	// the macOS window.
	if ((result.address < RAMDebugCore::kMainMemAddress) ||
	    (result.address >= (RAMDebugCore::kMainMemAddress + RAMDebugCore::kMainMemSize)))
	{
		gdk_display_beep(gtk_widget_get_display(sWindow));
		show_error("Internal cheats can only target main RAM (0x02000000 - 0x023FFFFF). "
		           "The selected result is not in main RAM.");
		return;
	}

	// Take the value as the table shows it, like the Windows dialog, instead of
	// re-reading the bus with a different endianness convention.
	const int sizeInBytes = RAMDebugCore::SizeInBytes(selected_size_code());
	const uint32_t sizeMask = (sizeInBytes >= 4) ? 0xFFFFFFFFu : ((1u << (8 * sizeInBytes)) - 1u);
	const uint32_t value = result.currentValue & sizeMask;

	GtkWidget *dialog = gtk_dialog_new_with_buttons("Add Cheat",
	                                                GTK_WINDOW(sWindow),
	                                                (GtkDialogFlags)(GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT),
	                                                "_Cancel", GTK_RESPONSE_CANCEL,
	                                                "_Add", GTK_RESPONSE_OK,
	                                                NULL);
	gtk_dialog_set_default_response(GTK_DIALOG(dialog), GTK_RESPONSE_OK);

	GtkWidget *grid = gtk_grid_new();
	gtk_grid_set_row_spacing(GTK_GRID(grid), 6);
	gtk_grid_set_column_spacing(GTK_GRID(grid), 8);
	gtk_container_set_border_width(GTK_CONTAINER(grid), 12);

	gchar *addressText = g_strdup_printf("%s = %s (%d byte%s)",
	                                     RAMDebugCore::FormatAddress(result.address).c_str(),
	                                     RAMDebugCore::FormatValue(selected_size_code(),
	                                                               selected_display_code(), value).c_str(),
	                                     sizeInBytes, (sizeInBytes == 1) ? "" : "s");

	GtkWidget *addressLabel = gtk_label_new("Cheat:");
	gtk_widget_set_halign(addressLabel, GTK_ALIGN_END);
	GtkWidget *addressValue = gtk_label_new(addressText);
	gtk_widget_set_halign(addressValue, GTK_ALIGN_START);

	GtkWidget *descriptionLabel = gtk_label_new("Description:");
	gtk_widget_set_halign(descriptionLabel, GTK_ALIGN_END);
	GtkWidget *descriptionEntry = gtk_entry_new();
	gchar *defaultDescription = g_strdup_printf("RAM Search %s",
	                                            RAMDebugCore::FormatAddress(result.address).c_str());
	gtk_entry_set_text(GTK_ENTRY(descriptionEntry), defaultDescription);
	gtk_entry_set_activates_default(GTK_ENTRY(descriptionEntry), TRUE);
	gtk_widget_set_hexpand(descriptionEntry, TRUE);

	GtkWidget *hint = gtk_label_new("Constant write, added to the cheat list and saved with the ROM.");
	gtk_widget_set_halign(hint, GTK_ALIGN_START);
	gtk_style_context_add_class(gtk_widget_get_style_context(hint), "dim-label");

	gtk_grid_attach(GTK_GRID(grid), addressLabel, 0, 0, 1, 1);
	gtk_grid_attach(GTK_GRID(grid), addressValue, 1, 0, 1, 1);
	gtk_grid_attach(GTK_GRID(grid), descriptionLabel, 0, 1, 1, 1);
	gtk_grid_attach(GTK_GRID(grid), descriptionEntry, 1, 1, 1, 1);
	gtk_grid_attach(GTK_GRID(grid), hint, 1, 2, 1, 1);

	gtk_box_pack_start(GTK_BOX(gtk_dialog_get_content_area(GTK_DIALOG(dialog))),
	                   grid, TRUE, TRUE, 0);
	gtk_widget_show_all(dialog);

	if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_OK)
	{
		gchar *description = g_strdup(gtk_entry_get_text(GTK_ENTRY(descriptionEntry)));

		// CHEATS::add() takes a zero-based size: 0 = 1 byte, 1 = 2 bytes, 2 = 4.
		const bool added = cheats->add((u8)(sizeInBytes - 1), result.address, value,
		                              description, true);

		if (added)
		{
			// Persist the list the same way the Cheat List window does on close.
			cheats->save();
			g_snprintf(sStatusNote, sizeof(sStatusNote), "cheat added for %s",
			           RAMDebugCore::FormatAddress(result.address).c_str());
		}
		else
		{
			show_error("The cheat could not be added.");
		}

		g_free(description);
	}

	gtk_widget_destroy(dialog);
	g_free(defaultDescription);
	g_free(addressText);

	update_status_label();
}

/////////////////////////////// AUTO-SEARCH ///////////////////////////////

// The result list ran dry: ask what to do, then restart the search. This runs
// from the frame tick, so the nested dialog loop keeps the emulator running;
// update() bails out while the alert is up to avoid stacking dialogs.
static void handle_auto_search_exhausted(void)
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

	GtkWidget *dialog = gtk_message_dialog_new(GTK_WINDOW(sWindow),
	                                           (GtkDialogFlags)(GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT),
	                                           GTK_MESSAGE_QUESTION,
	                                           GTK_BUTTONS_NONE,
	                                           "Auto-search is out of results.");
	gtk_message_dialog_format_secondary_text(GTK_MESSAGE_DIALOG(dialog),
	                                         "Retry resets the search once and keeps auto-searching. "
	                                         "Ignore resets whenever necessary without asking again. "
	                                         "Stop turns auto-search off.");
	gtk_dialog_add_buttons(GTK_DIALOG(dialog),
	                       "_Stop", RESPONSE_STOP,
	                       "_Ignore", RESPONSE_IGNORE,
	                       "_Retry", RESPONSE_RETRY,
	                       NULL);
	gtk_dialog_set_default_response(GTK_DIALOG(dialog), RESPONSE_RETRY);

	const int response = gtk_dialog_run(GTK_DIALOG(dialog));
	gtk_widget_destroy(dialog);
	sInAutoSearchAlert = FALSE;

	if (response == RESPONSE_STOP)
	{
		sSyncingAutoSearchButton = TRUE;
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(sAutoSearchButton), FALSE);
		sSyncingAutoSearchButton = FALSE;

		sAutoSearchEnabled = FALSE;
		sAutoSearchAutoRetry = FALSE;
		sStatusNote[0] = '\0';
		update_status_label();
		return;
	}

	if (response == RESPONSE_IGNORE)
	{
		sAutoSearchAutoRetry = TRUE;
	}

	core.Reset();
	rebuild_model();
}

// One auto-search pass. The criteria come from the widgets, exactly as if the
// user pressed Search each time.
static void run_auto_search_step(void)
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

static void on_auto_search_toggled(GtkToggleButton *button, gpointer data)
{
	if (sSyncingAutoSearchButton)
	{
		return;
	}

	sAutoSearchEnabled = gtk_toggle_button_get_active(button);
	sAutoSearchAutoRetry = FALSE;

	// Checking the box runs a search right away, like the Windows dialog.
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

static void on_row_activated(GtkTreeView *view, GtkTreePath *path, GtkTreeViewColumn *column,
                             gpointer data)
{
	on_add_watch(NULL, NULL);
}static void on_selection_changed(GtkTreeSelection *selection, gpointer data)
{
	update_control_states();
}

/////////////////////////////// WINDOW LIFECYCLE ///////////////////////////////

static void on_window_destroy(GtkWidget *widget, gpointer data)
{
	sWindow = NULL;
	sTreeView = NULL;
	sStore = NULL;
	sSearchTypeCombo = NULL;
	sOperatorCombo = NULL;
	sSizeCombo = NULL;
	sDisplayCombo = NULL;
	sValueEntry = NULL;
	sParameterEntry = NULL;
	sMisalignButton = NULL;
	sLittleEndianButton = NULL;
	sSearchButton = NULL;
	sResetButton = NULL;
	sClearChangesButton = NULL;
	sUndoButton = NULL;
	sRedoButton = NULL;
	sEliminateButton = NULL;
	sPokeButton = NULL;
	sAddWatchButton = NULL;
	sAddCheatButton = NULL;
	sAutoSearchButton = NULL;
	sStatusLabel = NULL;
	sModelRows = 0;
	sStatusNote[0] = '\0';

	// Auto-search state belongs to the window; the engine keeps its results.
	sAutoSearchEnabled = FALSE;
	sAutoSearchAutoRetry = FALSE;
	sInAutoSearchAlert = FALSE;
	sSyncingAutoSearchButton = FALSE;

	if (sToolID >= 0)
	{
		dTool_CloseCallback(sToolID);
	}
	sToolID = -1;
}

static GtkWidget *add_label(GtkGrid *grid, const char *text, gint row)
{
	GtkWidget *label = gtk_label_new(text);
	gtk_widget_set_halign(label, GTK_ALIGN_START);
	gtk_grid_attach(grid, label, 0, row, 1, 1);
	return label;
}

// Two buttons sharing one row; every action column pair uses this so the
// sidebar stays short enough for the 640 px window.
static GtkWidget *button_row(GtkWidget *left, GtkWidget *right)
{
	GtkWidget *rowBox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
	gtk_box_set_homogeneous(GTK_BOX(rowBox), TRUE);
	gtk_box_pack_start(GTK_BOX(rowBox), left, TRUE, TRUE, 0);
	if (right != NULL)
	{
		gtk_box_pack_start(GTK_BOX(rowBox), right, TRUE, TRUE, 0);
	}
	return rowBox;
}

static void set_table_font(GSettings *settings, gchar *key, gpointer data)
{
	(void)key;
	GtkWidget *tree = GTK_WIDGET(data);
	PangoFontDescription *font = gtk_monospace_font_description(tree, settings);
	GList *columns = gtk_tree_view_get_columns(GTK_TREE_VIEW(tree));
	for (GList *col = columns; col != NULL; col = col->next)
	{
		GList *cells = gtk_cell_layout_get_cells(GTK_CELL_LAYOUT(col->data));
		for (GList *cell = cells; cell != NULL; cell = cell->next)
			g_object_set(cell->data, "font-desc", font, NULL);
		g_list_free(cells);
	}
	g_list_free(columns);
	pango_font_description_free(font);
}

static void build_window(void)
{
	sWindow = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	gtk_window_set_title(GTK_WINDOW(sWindow), TOOL_NAME);
	gtk_window_set_default_size(GTK_WINDOW(sWindow), 587, 640);
	gtk_window_set_resizable(GTK_WINDOW(sWindow), TRUE);
	gtk_window_set_icon_name(GTK_WINDOW(sWindow), "org.desmume.DeSmuME");
	gtk_container_set_border_width(GTK_CONTAINER(sWindow), 6);

	g_signal_connect(G_OBJECT(sWindow), "destroy", G_CALLBACK(on_window_destroy), NULL);

	GtkWidget *mainBox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
	gtk_container_add(GTK_CONTAINER(sWindow), mainBox);

	// ---- Result table ----
	sStore = gtk_list_store_new(COLUMN_COUNT,
	                            G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING);
	sTreeView = gtk_tree_view_new_with_model(GTK_TREE_MODEL(sStore));
	gtk_tree_view_set_enable_search(GTK_TREE_VIEW(sTreeView), FALSE);

	GtkCellRenderer *renderer;
	GtkTreeViewColumn *column;

	renderer = gtk_cell_renderer_text_new();
	column = gtk_tree_view_column_new_with_attributes("Address", renderer,
	                                                  "text", COL_ADDRESS, NULL);
	gtk_tree_view_column_set_resizable(column, TRUE);
	gtk_tree_view_column_set_min_width(column, 90);
	gtk_tree_view_append_column(GTK_TREE_VIEW(sTreeView), column);

	renderer = gtk_cell_renderer_text_new();
	column = gtk_tree_view_column_new_with_attributes("Value", renderer,
	                                                  "text", COL_VALUE, NULL);
	gtk_tree_view_column_set_resizable(column, TRUE);
	gtk_tree_view_column_set_min_width(column, 70);
	gtk_tree_view_append_column(GTK_TREE_VIEW(sTreeView), column);

	renderer = gtk_cell_renderer_text_new();
	column = gtk_tree_view_column_new_with_attributes("Previous", renderer,
	                                                  "text", COL_PREVIOUS, NULL);
	gtk_tree_view_column_set_resizable(column, TRUE);
	gtk_tree_view_column_set_min_width(column, 70);
	gtk_tree_view_append_column(GTK_TREE_VIEW(sTreeView), column);

	renderer = gtk_cell_renderer_text_new();
	column = gtk_tree_view_column_new_with_attributes("Changes", renderer,
	                                                  "text", COL_CHANGES, NULL);
	gtk_tree_view_column_set_resizable(column, TRUE);
	gtk_tree_view_column_set_expand(column, TRUE);
	gtk_tree_view_append_column(GTK_TREE_VIEW(sTreeView), column);

	GSettings *fontSettings = gtk_monospace_font_settings_new();
	set_table_font(fontSettings, NULL, sTreeView);
	if (fontSettings != NULL)
	{
		g_signal_connect_object(fontSettings, "changed::monospace-font-name",
		                        G_CALLBACK(set_table_font), sTreeView, (GConnectFlags)0);
		g_object_set_data_full(G_OBJECT(sTreeView), "monospace-font-settings",
		                       fontSettings, g_object_unref);
	}

	GtkWidget *scrolledWindow = gtk_scrolled_window_new(NULL, NULL);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolledWindow),
	                               GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
	gtk_scrolled_window_set_shadow_type(GTK_SCROLLED_WINDOW(scrolledWindow),
	                                    GTK_SHADOW_IN);
	gtk_widget_set_hexpand(scrolledWindow, TRUE);
	gtk_widget_set_vexpand(scrolledWindow, TRUE);
	gtk_container_add(GTK_CONTAINER(scrolledWindow), sTreeView);
	gtk_box_pack_start(GTK_BOX(mainBox), scrolledWindow, TRUE, TRUE, 0);

	// ---- Search controls ----
	GtkWidget *sidebar = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
	gtk_widget_set_size_request(sidebar, 250, -1);
	// The controls inside are hexpand so they fill the sidebar; that would
	// otherwise propagate up and let the sidebar compete with the table for the
	// window's extra width. Pin the sidebar so the table gets it.
	gtk_widget_set_hexpand(sidebar, FALSE);

	GtkWidget *grid = gtk_grid_new();
	gtk_grid_set_row_spacing(GTK_GRID(grid), 6);
	gtk_grid_set_column_spacing(GTK_GRID(grid), 8);

	add_label(GTK_GRID(grid), "Search Type", 0);
	sSearchTypeCombo = gtk_combo_box_text_new();
	for (int i = 0; i < 4; i++)
	{
		gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(sSearchTypeCombo), kSearchTypeLabels[i]);
	}
	gtk_widget_set_hexpand(sSearchTypeCombo, TRUE);
	gtk_grid_attach(GTK_GRID(grid), sSearchTypeCombo, 1, 0, 1, 1);

	add_label(GTK_GRID(grid), "Comparison", 1);
	sOperatorCombo = gtk_combo_box_text_new();
	for (int i = 0; i < 8; i++)
	{
		gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(sOperatorCombo), kOperatorLabels[i]);
	}
	gtk_widget_set_hexpand(sOperatorCombo, TRUE);
	gtk_grid_attach(GTK_GRID(grid), sOperatorCombo, 1, 1, 1, 1);

	add_label(GTK_GRID(grid), "Value", 2);
	sValueEntry = gtk_entry_new();
	gtk_entry_set_width_chars(GTK_ENTRY(sValueEntry), 10);
	gtk_entry_set_text(GTK_ENTRY(sValueEntry), "0");
	gtk_entry_set_activates_default(GTK_ENTRY(sValueEntry), TRUE);
	gtk_grid_attach(GTK_GRID(grid), sValueEntry, 1, 2, 1, 1);

	add_label(GTK_GRID(grid), "Parameter", 3);
	sParameterEntry = gtk_entry_new();
	gtk_entry_set_width_chars(GTK_ENTRY(sParameterEntry), 10);
	gtk_entry_set_text(GTK_ENTRY(sParameterEntry), "0");
	gtk_entry_set_activates_default(GTK_ENTRY(sParameterEntry), TRUE);
	gtk_grid_attach(GTK_GRID(grid), sParameterEntry, 1, 3, 1, 1);

	add_label(GTK_GRID(grid), "Size", 4);
	sSizeCombo = gtk_combo_box_text_new();
	for (int i = 0; i < 3; i++)
	{
		gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(sSizeCombo), kSizeLabels[i]);
	}
	gtk_widget_set_hexpand(sSizeCombo, TRUE);
	gtk_grid_attach(GTK_GRID(grid), sSizeCombo, 1, 4, 1, 1);

	add_label(GTK_GRID(grid), "Display", 5);
	sDisplayCombo = gtk_combo_box_text_new();
	for (int i = 0; i < 4; i++)
	{
		gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(sDisplayCombo), kDisplayLabels[i]);
	}
	gtk_widget_set_hexpand(sDisplayCombo, TRUE);
	gtk_grid_attach(GTK_GRID(grid), sDisplayCombo, 1, 5, 1, 1);

	gtk_box_pack_start(GTK_BOX(sidebar), grid, FALSE, FALSE, 0);

	sMisalignButton = gtk_check_button_new_with_label("Allow misaligned addresses");
	sLittleEndianButton = gtk_check_button_new_with_label("Little endian values");
	sAutoSearchButton = gtk_check_button_new_with_label("Auto-search");
	gtk_widget_set_tooltip_text(sAutoSearchButton,
		"Keep applying the current criteria as the game runs");
	gtk_box_pack_start(GTK_BOX(sidebar), sMisalignButton, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(sidebar), sLittleEndianButton, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(sidebar), sAutoSearchButton, FALSE, FALSE, 0);

	gtk_box_pack_start(GTK_BOX(sidebar), gtk_separator_new(GTK_ORIENTATION_HORIZONTAL),
	                   FALSE, FALSE, 0);

	// Same order as the Windows dialog and the macOS column.
	sSearchButton = gtk_button_new_with_label("Search");
	sResetButton = gtk_button_new_with_label("Reset");
	sClearChangesButton = gtk_button_new_with_label("Clear Changes");
	sUndoButton = gtk_button_new_with_label("Undo");
	sRedoButton = gtk_button_new_with_label("Redo");
	sEliminateButton = gtk_button_new_with_label("Eliminate");
	sPokeButton = gtk_button_new_with_label("Poke");
	sAddWatchButton = gtk_button_new_with_label("Add Watch");
	sAddCheatButton = gtk_button_new_with_label("Add Cheat");

	gtk_box_pack_start(GTK_BOX(sidebar), button_row(sSearchButton, sResetButton), FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(sidebar), sClearChangesButton, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(sidebar), button_row(sUndoButton, sRedoButton), FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(sidebar), button_row(sEliminateButton, sPokeButton), FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(sidebar), button_row(sAddWatchButton, sAddCheatButton), FALSE, FALSE, 0);

	sStatusLabel = gtk_label_new("");
	gtk_label_set_line_wrap(GTK_LABEL(sStatusLabel), TRUE);
	gtk_label_set_max_width_chars(GTK_LABEL(sStatusLabel), 28);
	gtk_label_set_justify(GTK_LABEL(sStatusLabel), GTK_JUSTIFY_CENTER);
	gtk_label_set_xalign(GTK_LABEL(sStatusLabel), 0.5f);
	gtk_style_context_add_class(gtk_widget_get_style_context(sStatusLabel), "dim-label");
	gtk_box_pack_end(GTK_BOX(sidebar), sStatusLabel, FALSE, FALSE, 0);

	gtk_box_pack_start(GTK_BOX(mainBox), sidebar, FALSE, FALSE, 0);

	// ---- Restore the core's current options into the widgets, then connect ----
	// Setting the widgets first keeps the "changed" handlers from resetting the
	// candidate list while the window is being built.
	RAMSearchCore &core = RAMSearchCore::Get();
	gtk_combo_box_set_active(GTK_COMBO_BOX(sSearchTypeCombo), (gint)core.GetSearchType());
	gtk_combo_box_set_active(GTK_COMBO_BOX(sOperatorCombo), (gint)core.GetCompareOperator());
	gtk_combo_box_set_active(GTK_COMBO_BOX(sDisplayCombo), (gint)core.GetValueType());
	gtk_combo_box_set_active(GTK_COMBO_BOX(sSizeCombo),
		(core.Size() == 'w') ? 1 : (core.Size() == 'd') ? 2 : 0);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(sMisalignButton), core.Misaligned());
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(sLittleEndianButton), core.LittleEndian());

	g_signal_connect(G_OBJECT(sSearchTypeCombo), "changed",
	                 G_CALLBACK(on_options_changed), NULL);
	g_signal_connect(G_OBJECT(sOperatorCombo), "changed",
	                 G_CALLBACK(on_operator_changed), NULL);
	g_signal_connect(G_OBJECT(sSizeCombo), "changed",
	                 G_CALLBACK(on_options_changed), NULL);
	g_signal_connect(G_OBJECT(sDisplayCombo), "changed",
	                 G_CALLBACK(on_options_changed), NULL);
	g_signal_connect(G_OBJECT(sMisalignButton), "toggled",
	                 G_CALLBACK(on_options_changed), NULL);
	g_signal_connect(G_OBJECT(sLittleEndianButton), "toggled",
	                 G_CALLBACK(on_options_changed), NULL);
	g_signal_connect(G_OBJECT(sSearchButton), "clicked", G_CALLBACK(on_search), NULL);
	g_signal_connect(G_OBJECT(sResetButton), "clicked", G_CALLBACK(on_reset), NULL);
	g_signal_connect(G_OBJECT(sClearChangesButton), "clicked", G_CALLBACK(on_clear_changes), NULL);
	g_signal_connect(G_OBJECT(sUndoButton), "clicked", G_CALLBACK(on_undo), NULL);
	g_signal_connect(G_OBJECT(sRedoButton), "clicked", G_CALLBACK(on_redo), NULL);
	g_signal_connect(G_OBJECT(sEliminateButton), "clicked", G_CALLBACK(on_eliminate), NULL);
	g_signal_connect(G_OBJECT(sPokeButton), "clicked", G_CALLBACK(on_poke), NULL);
	g_signal_connect(G_OBJECT(sAddWatchButton), "clicked", G_CALLBACK(on_add_watch), NULL);
	g_signal_connect(G_OBJECT(sAddCheatButton), "clicked", G_CALLBACK(on_add_cheat), NULL);
	g_signal_connect(G_OBJECT(sAutoSearchButton), "toggled",
	                 G_CALLBACK(on_auto_search_toggled), NULL);
	g_signal_connect(G_OBJECT(sTreeView), "row-activated",
	                 G_CALLBACK(on_row_activated), NULL);

	GtkTreeSelection *selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(sTreeView));
	gtk_tree_selection_set_mode(selection, GTK_SELECTION_SINGLE);
	g_signal_connect(G_OBJECT(selection), "changed",
	                 G_CALLBACK(on_selection_changed), NULL);

	g_signal_connect(G_OBJECT(sValueEntry), "activate", G_CALLBACK(on_search), NULL);
	g_signal_connect(G_OBJECT(sParameterEntry), "activate", G_CALLBACK(on_search), NULL);

	update_parameter_sensitivity();
	gtk_widget_show_all(mainBox);
}

/////////////////////////////// TOOL ENTRY POINTS ///////////////////////////////

static void update(void)
{
	if ((sWindow == NULL) || (sStore == NULL))
	{
		return;
	}
	if (sInAutoSearchAlert)
	{
		// The alert runs a nested loop; the frame tick keeps firing.
		return;
	}

	RAMSearchCore &core = RAMSearchCore::Get();
	const size_t count = core.ResultCount();
	if ((count == 0) && !sAutoSearchEnabled)
	{
		return;
	}

	// SignalNewFrame() walks every candidate — four million right after a
	// Reset — so the repaint rate is throttled exactly like the macOS host:
	// every 150 ms normally, and every 900 ms for huge result sets.
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
		// Narrowing may rebuild the whole model, so the visible-row repaint is
		// left to that path (or to run_auto_search_step() when nothing changed).
		run_auto_search_step();
	}
	else
	{
		refresh_visible_values();
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
	sLastRefreshTime = 0;
	sStatusNote[0] = '\0';

	build_window();

	// First time the tool is opened there is nothing to show, so enumerate
	// every candidate; later opens keep the previous search result.
	if (RAMSearchCore::Get().ResultCount() == 0)
	{
		sync_options_from_ui();
		RAMSearchCore::Get().Reset();
	}

	rebuild_model();
	gtk_widget_show(sWindow);
}

static void close_tool(void)
{
	// The dTool framework never calls this; the window's own destroy handler
	// notifies it through dTool_CloseCallback().
}

dTool_t dTool_ramSearch =
{
	SHORTNAME,
	TOOL_NAME,
	&open_window,
	&update,
	&close_tool
};
