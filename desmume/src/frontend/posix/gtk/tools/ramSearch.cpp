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
#include "../../../shared/ram_debug_core.h"
#include "../../../shared/ram_search_core.h"

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
static GtkWidget *sAddWatchButton = NULL;
static GtkWidget *sStatusLabel = NULL;

static int sToolID = -1;

// Rows currently in the model, so per-frame repaints never run past the end.
static guint sModelRows = 0;

// Transient feedback shown after "Add Watch" (which does not change the
// result count, so nothing else would refresh the label).
static char sStatusNote[64] = "";

// Timestamp of the last per-frame refresh (microseconds, monotonic).
static gint64 sLastRefreshTime = 0;

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

static void update_control_states(void)
{
	const int row = selected_row();
	const gboolean hasRow = (row >= 0) && ((size_t)row < RAMSearchCore::Get().ResultCount());

	gtk_widget_set_sensitive(sAddWatchButton, hasRow);
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

	gtk_list_store_clear(sStore);
	for (size_t i = 0; i < rows; i++)
	{
		GtkTreeIter iter;
		gtk_list_store_append(sStore, &iter);
		fill_row(&iter, i);
	}
	sModelRows = (guint)rows;

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

static void on_row_activated(GtkTreeView *view, GtkTreePath *path, GtkTreeViewColumn *column,
                             gpointer data)
{
	on_add_watch(NULL, NULL);
}

static void on_selection_changed(GtkTreeSelection *selection, gpointer data)
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
	sAddWatchButton = NULL;
	sStatusLabel = NULL;
	sModelRows = 0;
	sStatusNote[0] = '\0';

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

static void build_window(void)
{
	sWindow = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	gtk_window_set_title(GTK_WINDOW(sWindow), TOOL_NAME);
	gtk_window_set_default_size(GTK_WINDOW(sWindow), 780, 520);
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
	g_object_set(renderer, "family", "monospace", NULL);
	column = gtk_tree_view_column_new_with_attributes("Address", renderer,
	                                                  "text", COL_ADDRESS, NULL);
	gtk_tree_view_column_set_resizable(column, TRUE);
	gtk_tree_view_column_set_min_width(column, 90);
	gtk_tree_view_append_column(GTK_TREE_VIEW(sTreeView), column);

	renderer = gtk_cell_renderer_text_new();
	g_object_set(renderer, "family", "monospace", NULL);
	column = gtk_tree_view_column_new_with_attributes("Value", renderer,
	                                                  "text", COL_VALUE, NULL);
	gtk_tree_view_column_set_resizable(column, TRUE);
	gtk_tree_view_column_set_min_width(column, 70);
	gtk_tree_view_append_column(GTK_TREE_VIEW(sTreeView), column);

	renderer = gtk_cell_renderer_text_new();
	g_object_set(renderer, "family", "monospace", NULL);
	column = gtk_tree_view_column_new_with_attributes("Previous", renderer,
	                                                  "text", COL_PREVIOUS, NULL);
	gtk_tree_view_column_set_resizable(column, TRUE);
	gtk_tree_view_column_set_min_width(column, 70);
	gtk_tree_view_append_column(GTK_TREE_VIEW(sTreeView), column);

	renderer = gtk_cell_renderer_text_new();
	g_object_set(renderer, "family", "monospace", NULL);
	column = gtk_tree_view_column_new_with_attributes("Changes", renderer,
	                                                  "text", COL_CHANGES, NULL);
	gtk_tree_view_column_set_resizable(column, TRUE);
	gtk_tree_view_column_set_expand(column, TRUE);
	gtk_tree_view_append_column(GTK_TREE_VIEW(sTreeView), column);

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
	gtk_entry_set_text(GTK_ENTRY(sValueEntry), "0");
	gtk_entry_set_activates_default(GTK_ENTRY(sValueEntry), TRUE);
	gtk_grid_attach(GTK_GRID(grid), sValueEntry, 1, 2, 1, 1);

	add_label(GTK_GRID(grid), "Parameter", 3);
	sParameterEntry = gtk_entry_new();
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
	gtk_box_pack_start(GTK_BOX(sidebar), sMisalignButton, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(sidebar), sLittleEndianButton, FALSE, FALSE, 0);

	gtk_box_pack_start(GTK_BOX(sidebar), gtk_separator_new(GTK_ORIENTATION_HORIZONTAL),
	                   FALSE, FALSE, 0);

	sSearchButton = gtk_button_new_with_label("Search");
	sResetButton = gtk_button_new_with_label("Reset");
	sAddWatchButton = gtk_button_new_with_label("Add Watch");
	gtk_box_pack_start(GTK_BOX(sidebar), sSearchButton, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(sidebar), sResetButton, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(sidebar), sAddWatchButton, FALSE, FALSE, 0);

	sStatusLabel = gtk_label_new("");
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
	g_signal_connect(G_OBJECT(sAddWatchButton), "clicked", G_CALLBACK(on_add_watch), NULL);
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

	RAMSearchCore &core = RAMSearchCore::Get();
	const size_t count = core.ResultCount();
	if (count == 0)
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
	refresh_visible_values();
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
