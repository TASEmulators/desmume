/* cheatsGTK.cpp - this file is part of DeSmuME
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <string>

#include <gtk/gtk.h>

#include "cheatsGTK.h"
#include "cheatSystem.h"
#include "config.h"
#include "desmume.h"
#include "main.h"
#include "NDSSystem.h"

/* GTK takes these as a bit mask of GtkDialogFlags, which C++ will not fold for
 * us at the call site. */
static const GtkDialogFlags kModalDialogFlags =
	(GtkDialogFlags)(GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT);

// The instance main.cpp owns; config_opts.h only declares the members.
extern desmume::config::Config config;

/* The cheat list is a view over CHEATS in src/cheatSystem.cpp. Every edit goes
 * through the same calls the Windows window makes, so a .dct saved here is
 * identical to one saved there, and the parity details (address masking, value
 * ranges, the generated Action Replay preview) live in this file only because
 * they are what the dialogs *show*; the data itself is the core's. */

namespace {

///////////////////////////////////////////////////////////////////////////
// Store layout

enum
{
	COL_ENABLED,
	COL_INDEX,
	COL_TYPE_ID,
	COL_TYPE,
	COL_SIZE,
	COL_ADDRESS,
	COL_VALUE,
	COL_CODE,
	COL_LINES,
	COL_DESC,
	NUM_COLUMNS
};

GtkWidget *sWindow = NULL;
GtkWidget *sNotebook = NULL;
GtkWidget *sInternalView = NULL;
GtkWidget *sCodeView = NULL;
GtkListStore *sStore = NULL;
GtkTreeModel *sInternalFilter = NULL;
GtkTreeModel *sCodeFilter = NULL;

GtkWidget *sEditButton = NULL;
GtkWidget *sRemoveButton = NULL;
GtkWidget *sMoveUpButton = NULL;
GtkWidget *sMoveDownButton = NULL;
GtkWidget *sStatusLabel = NULL;

/* The list is edited in place. A copy taken when the window opens lets
 * "Discard" put it back the way it was, which is what the Windows dialog does
 * when its Cancel button is answered with "No". */
CHEATS *sSnapshot = NULL;
bool sDirty = false;
bool sWasRunning = false;

const char *kTypeNames[] = { "Internal", "Action Replay", "Codebreaker" };

const char *type_name(u8 type)
{
	switch (type) {
	case CHEAT_TYPE_INTERNAL: return kTypeNames[0];
	case CHEAT_TYPE_AR:       return kTypeNames[1];
	case CHEAT_TYPE_CODEBREAKER: return kTypeNames[2];
	default: return "Unknown";
	}
}

u32 size_max_value(u8 size)
{
	switch (size) {
	case 0: return 0x000000FFu;
	case 1: return 0x0000FFFFu;
	case 2: return 0x00FFFFFFu;
	default: return 0xFFFFFFFFu;
	}
}

// The Windows dialog previews the Action Replay code an internal cheat is
// equivalent to; keep the same encoding so both ports show the same string.
void format_ar_code(u32 address, u32 value, u8 size, char *buffer, size_t length)
{
	const u32 type = (size > 3) ? 0u : (u32)(3 - size);
	g_snprintf(buffer, length, "%X%07X %08X", type, address & 0x0FFFFFFF, value);
}

bool parse_hex_address(const char *text, u32 *outAddress)
{
	while (g_ascii_isspace(*text)) {
		text++;
	}
	if ((text[0] == '0') && ((text[1] == 'x') || (text[1] == 'X'))) {
		text += 2;
	}
	if (*text == '\0') {
		return false;
	}
	for (const char *cursor = text; *cursor != '\0'; cursor++) {
		if (!g_ascii_isxdigit(*cursor)) {
			return false;
		}
	}
	const guint64 value = g_ascii_strtoull(text, NULL, 16);
	if (value > 0x0FFFFFFF) {
		return false;
	}
	*outAddress = (u32)value;
	return true;
}

/* Values are decimal, and a leading minus is accepted as the two's complement
 * of the selected width, so "-1" with one byte freezes 0xFF. */
bool parse_cheat_value(const char *text, u8 size, u32 *outValue)
{
	while (g_ascii_isspace(*text)) {
		text++;
	}
	if (*text == '\0') {
		return false;
	}

	char *end = NULL;
	const gint64 parsed = g_ascii_strtoll(text, &end, 10);
	if ((end == text) || (end == NULL)) {
		return false;
	}
	while (g_ascii_isspace(*end)) {
		end++;
	}
	if (*end != '\0') {
		return false;
	}

	const u32 maximum = size_max_value(size);
	if (parsed >= 0) {
		if ((guint64)parsed > (guint64)maximum) {
			return false;
		}
		*outValue = (u32)parsed;
		return true;
	}

	if ((guint64)(-parsed) > ((guint64)maximum / 2) + 1) {
		return false;
	}
	*outValue = (u32)parsed & maximum;
	return true;
}

void show_error(GtkWindow *parent, const char *message)
{
	GtkWidget *dialog = gtk_message_dialog_new(parent,
	                                           kModalDialogFlags,
	                                           GTK_MESSAGE_ERROR,
	                                           GTK_BUTTONS_OK,
	                                           "%s", message);
	gtk_dialog_run(GTK_DIALOG(dialog));
	gtk_widget_destroy(dialog);
}

bool ask_confirm(GtkWindow *parent, const char *message, const char *acceptLabel)
{
	GtkWidget *dialog = gtk_message_dialog_new(parent,
	                                           kModalDialogFlags,
	                                           GTK_MESSAGE_QUESTION,
	                                           GTK_BUTTONS_NONE,
	                                           "%s", message);
	gtk_dialog_add_button(GTK_DIALOG(dialog), "_Cancel", GTK_RESPONSE_CANCEL);
	gtk_dialog_add_button(GTK_DIALOG(dialog), acceptLabel, GTK_RESPONSE_ACCEPT);
	gtk_dialog_set_default_response(GTK_DIALOG(dialog), GTK_RESPONSE_CANCEL);

	const gint response = gtk_dialog_run(GTK_DIALOG(dialog));
	gtk_widget_destroy(dialog);
	return response == GTK_RESPONSE_ACCEPT;
}

///////////////////////////////////////////////////////////////////////////
// Model

void rebuild_store(void)
{
	gtk_list_store_clear(sStore);

	const size_t count = cheats->getListSize();
	for (size_t i = 0; i < count; i++) {
		CHEATS_LIST cheat;
		if (!cheats->copyItemFromIndex(i, cheat)) {
			continue;
		}

		char address[16] = "";
		char value[16] = "";
		char code[24] = "";
		int size = 0;
		int lines = 0;

		switch (cheat.type) {
		case CHEAT_TYPE_INTERNAL:
			g_snprintf(address, sizeof(address), "0x%08X", cheat.code[0][0]);
			g_snprintf(value, sizeof(value), "%u", cheat.code[0][1]);
			size = (int)cheat.size + 1;
			break;
		case CHEAT_TYPE_AR:
		case CHEAT_TYPE_CODEBREAKER:
			if (cheat.num > 0) {
				g_snprintf(code, sizeof(code), "%08X %08X", cheat.code[0][0], cheat.code[0][1]);
			}
			lines = (int)cheat.num;
			break;
		default:
			continue;
		}

		GtkTreeIter iter;
		gtk_list_store_append(sStore, &iter);
		gtk_list_store_set(sStore, &iter,
		                   COL_ENABLED, cheat.enabled != 0,
		                   COL_INDEX, (int)i,
		                   COL_TYPE_ID, (int)cheat.type,
		                   COL_TYPE, type_name(cheat.type),
		                   COL_SIZE, size,
		                   COL_ADDRESS, address,
		                   COL_VALUE, value,
		                   COL_CODE, code,
		                   COL_LINES, lines,
		                   COL_DESC, cheat.description,
		                   -1);
	}
}

gboolean filter_internal(GtkTreeModel *model, GtkTreeIter *iter, gpointer data)
{
	int type = CHEAT_TYPE_EMPTY;
	gtk_tree_model_get(model, iter, COL_TYPE_ID, &type, -1);
	return (type == CHEAT_TYPE_INTERNAL) ? TRUE : FALSE;
}

gboolean filter_code(GtkTreeModel *model, GtkTreeIter *iter, gpointer data)
{
	int type = CHEAT_TYPE_EMPTY;
	gtk_tree_model_get(model, iter, COL_TYPE_ID, &type, -1);
	return ((type == CHEAT_TYPE_AR) || (type == CHEAT_TYPE_CODEBREAKER)) ? TRUE : FALSE;
}

int notebook_page(void)
{
	if (sNotebook == NULL) {
		return 0;
	}
	return gtk_notebook_get_current_page(GTK_NOTEBOOK(sNotebook));
}

GtkWidget *active_view(void)
{
	return (notebook_page() == 1) ? sCodeView : sInternalView;
}

GtkTreeModel *active_model(void)
{
	return (notebook_page() == 1) ? sCodeFilter : sInternalFilter;
}

// Cheat index of the selected row, or -1 when nothing (or several rows) is
// selected. The views are multi-select, which is why this cannot use
// gtk_tree_selection_get_selected().
int selected_index(void)
{
	GtkTreeView *view = GTK_TREE_VIEW(active_view());
	if (view == NULL) {
		return -1;
	}

	GtkTreeSelection *selection = gtk_tree_view_get_selection(view);
	GList *rows = gtk_tree_selection_get_selected_rows(selection, NULL);
	if ((rows == NULL) || (g_list_length(rows) != 1)) {
		g_list_free_full(rows, (GDestroyNotify)gtk_tree_path_free);
		return -1;
	}

	int index = -1;
	GtkTreeModel *model = active_model();
	GtkTreeIter iter;
	if (gtk_tree_model_get_iter(model, &iter, (GtkTreePath *)rows->data)) {
		gtk_tree_model_get(model, &iter, COL_INDEX, &index, -1);
	}
	g_list_free_full(rows, (GDestroyNotify)gtk_tree_path_free);
	return index;
}

// Selects the row that shows the given cheat, on whichever page lists it, so
// an operation on a row leaves the user where they expect to be.
void select_index(int index)
{
	if (index < 0) {
		return;
	}

	CHEATS_LIST cheat;
	if (!cheats->copyItemFromIndex((size_t)index, cheat)) {
		return;
	}

	const int page = (cheat.type == CHEAT_TYPE_INTERNAL) ? 0 : 1;
	gtk_notebook_set_current_page(GTK_NOTEBOOK(sNotebook), page);

	GtkTreeModel *model = (page == 0) ? sInternalFilter : sCodeFilter;
	GtkTreeIter iter;
	if (!gtk_tree_model_get_iter_first(model, &iter)) {
		return;
	}
	do {
		int rowIndex = -1;
		gtk_tree_model_get(model, &iter, COL_INDEX, &rowIndex, -1);
		if (rowIndex == index) {
			GtkTreeView *view = GTK_TREE_VIEW((page == 0) ? sInternalView : sCodeView);
			GtkTreeSelection *selection = gtk_tree_view_get_selection(view);
			gtk_tree_selection_select_iter(selection, &iter);

			GtkTreePath *path = gtk_tree_model_get_path(model, &iter);
			gtk_tree_view_scroll_to_cell(view, path, NULL, FALSE, 0.0f, 0.0f);
			gtk_tree_path_free(path);
			return;
		}
	} while (gtk_tree_model_iter_next(model, &iter));
}

void update_status(void)
{
	const size_t total = cheats->getListSize();
	const size_t enabled = cheats->getActiveCount();

	char text[128];
	g_snprintf(text, sizeof(text), "%zu cheat%s, %zu enabled%s",
	           total, (total == 1) ? "" : "s", enabled,
	           sDirty ? " - unsaved changes" : "");
	gtk_label_set_text(GTK_LABEL(sStatusLabel), text);

	std::string title("Cheat List");
	const char *filePath = cheats->getFilePath();
	if ((filePath != NULL) && (filePath[0] != '\0')) {
		gchar *baseName = g_path_get_basename(filePath);
		title += " - ";
		title += baseName;
		g_free(baseName);
	}
	if (sDirty) {
		title += " *";
	}
	gtk_window_set_title(GTK_WINDOW(sWindow), title.c_str());
}

void update_buttons(void)
{
	GtkTreeView *view = GTK_TREE_VIEW(active_view());
	GtkTreeSelection *selection = (view != NULL) ? gtk_tree_view_get_selection(view) : NULL;
	const guint selected = (selection != NULL) ? gtk_tree_selection_count_selected_rows(selection) : 0;

	gtk_widget_set_sensitive(sEditButton, (selected == 1) ? TRUE : FALSE);
	gtk_widget_set_sensitive(sRemoveButton, (selected >= 1) ? TRUE : FALSE);

	CHEATS_LIST cheat;
	const int index = selected_index();
	const bool hasSingle = (index >= 0) && cheats->copyItemFromIndex((size_t)index, cheat);
	gtk_widget_set_sensitive(sMoveUpButton, (hasSingle && (index > 0)) ? TRUE : FALSE);
	gtk_widget_set_sensitive(sMoveDownButton,
	                         (hasSingle && ((size_t)index + 1 < cheats->getListSize())) ? TRUE : FALSE);
}

void mark_dirty(void)
{
	sDirty = true;
	update_status();
}

// Rebuilds after a structural change and keeps the selection on `index`.
void after_change(int index)
{
	rebuild_store();
	select_index(index);
	update_buttons();
	update_status();
}

///////////////////////////////////////////////////////////////////////////
// Internal cheat dialog

struct InternalDialogState
{
	GtkWidget *dialog;
	GtkWidget *addressEntry;
	GtkWidget *valueEntry;
	GtkWidget *descriptionEntry;
	GtkWidget *sizeCombo;
	GtkWidget *enabledCheck;
	GtkWidget *codeLabel;
	GtkWidget *hintLabel;
	GtkWidget *okButton;

	bool editing;
	size_t position;

	bool valid;
	u8 size;
	u32 address;
	u32 value;
	bool enabled;
};

InternalDialogState sInternal;

void internal_dialog_validate(void)
{
	u32 address = 0;
	u32 value = 0;
	const gint sizeIndex = gtk_combo_box_get_active(GTK_COMBO_BOX(sInternal.sizeCombo));
	const u8 size = ((sizeIndex >= 0) && (sizeIndex < 4)) ? (u8)sizeIndex : (u8)0;

	const bool addressOk = parse_hex_address(gtk_entry_get_text(GTK_ENTRY(sInternal.addressEntry)), &address);
	const bool valueOk = parse_cheat_value(gtk_entry_get_text(GTK_ENTRY(sInternal.valueEntry)), size, &value);

	sInternal.valid = addressOk && valueOk;
	sInternal.size = size;
	sInternal.address = address;
	sInternal.value = value;
	sInternal.enabled = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(sInternal.enabledCheck));

	if (!addressOk) {
		gtk_label_set_text(GTK_LABEL(sInternal.hintLabel),
		                   "Enter an address of 1 to 8 hex digits, up to 0x0FFFFFFF.");
	} else if (!valueOk) {
		char hint[96];
		g_snprintf(hint, sizeof(hint), "Enter a value between 0 and %u for %d byte%s.",
		           (unsigned)size_max_value(size), size + 1, (size == 0) ? "" : "s");
		gtk_label_set_text(GTK_LABEL(sInternal.hintLabel), hint);
	} else {
		gtk_label_set_text(GTK_LABEL(sInternal.hintLabel), "");
	}

	char code[32];
	format_ar_code(address, value, size, code, sizeof(code));
	char markup[64];
	g_snprintf(markup, sizeof(markup), "<tt>%s</tt>", code);
	gtk_label_set_markup(GTK_LABEL(sInternal.codeLabel), markup);

	gtk_widget_set_sensitive(sInternal.okButton, sInternal.valid ? TRUE : FALSE);
}

void internal_dialog_changed(GtkWidget *widget, gpointer data)
{
	internal_dialog_validate();
}

/* Changing the width can leave the value out of range. Clamp it the way the
 * list's inline editor does, instead of refusing to add a cheat the user is
 * one keystroke away from. */
void internal_dialog_size_changed(GtkComboBox *combo, gpointer data)
{
	const gint sizeIndex = gtk_combo_box_get_active(combo);
	if ((sizeIndex < 0) || (sizeIndex >= 4)) {
		return;
	}

	const u32 maximum = size_max_value((u8)sizeIndex);
	const char *text = gtk_entry_get_text(GTK_ENTRY(sInternal.valueEntry));
	char *end = NULL;
	const gint64 parsed = g_ascii_strtoll(text, &end, 10);
	if ((end != text) && (parsed > (gint64)maximum)) {
		char clamped[16];
		g_snprintf(clamped, sizeof(clamped), "%u", (unsigned)maximum);
		gtk_entry_set_text(GTK_ENTRY(sInternal.valueEntry), clamped);
	}

	internal_dialog_validate();
}

GtkWidget *add_label(GtkGrid *grid, const char *text, gint row)
{
	GtkWidget *label = gtk_label_new(text);
	gtk_widget_set_halign(label, GTK_ALIGN_END);
	gtk_widget_set_valign(label, GTK_ALIGN_CENTER);
	gtk_grid_attach(grid, label, 0, row, 1, 1);
	return label;
}

void internal_cheat_dialog(bool editing, size_t position)
{
	memset(&sInternal, 0, sizeof(sInternal));
	sInternal.editing = editing;
	sInternal.position = position;

	CHEATS_LIST cheat;
	if (editing && cheats->copyItemFromIndex(position, cheat)) {
		sInternal.size = cheat.size;
		sInternal.address = cheat.code[0][0];
		sInternal.value = cheat.code[0][1];
		sInternal.enabled = (cheat.enabled != 0);
	} else {
		sInternal.size = 0;
		sInternal.address = 0;
		sInternal.value = 0;
		sInternal.enabled = true;
	}

	GtkWidget *dialog = gtk_dialog_new_with_buttons(editing ? "Edit Cheat" : "Add Cheat",
	                                                GTK_WINDOW(sWindow),
	                                                kModalDialogFlags,
	                                                "_Cancel", GTK_RESPONSE_CANCEL,
	                                                editing ? "_Update" : "_Add", GTK_RESPONSE_OK,
	                                                NULL);
	sInternal.dialog = dialog;
	gtk_window_set_default_size(GTK_WINDOW(dialog), 420, -1);

	GtkWidget *content = gtk_dialog_get_content_area(GTK_DIALOG(dialog));
	GtkWidget *grid = gtk_grid_new();
	gtk_grid_set_row_spacing(GTK_GRID(grid), 6);
	gtk_grid_set_column_spacing(GTK_GRID(grid), 8);
	gtk_widget_set_margin_start(grid, 12);
	gtk_widget_set_margin_end(grid, 12);
	gtk_widget_set_margin_top(grid, 12);
	gtk_container_add(GTK_CONTAINER(content), grid);

	char buffer[32];

	add_label(GTK_GRID(grid), "Address:", 0);
	sInternal.addressEntry = gtk_entry_new();
	gtk_entry_set_max_length(GTK_ENTRY(sInternal.addressEntry), 10);
	gtk_entry_set_width_chars(GTK_ENTRY(sInternal.addressEntry), 10);
	gtk_widget_set_hexpand(sInternal.addressEntry, TRUE);
	g_snprintf(buffer, sizeof(buffer), "%07X", (unsigned)sInternal.address);
	gtk_entry_set_text(GTK_ENTRY(sInternal.addressEntry), buffer);
	gtk_grid_attach(GTK_GRID(grid), sInternal.addressEntry, 1, 0, 1, 1);

	add_label(GTK_GRID(grid), "Value:", 1);
	sInternal.valueEntry = gtk_entry_new();
	gtk_entry_set_max_length(GTK_ENTRY(sInternal.valueEntry), 11);
	gtk_entry_set_width_chars(GTK_ENTRY(sInternal.valueEntry), 12);
	gtk_widget_set_hexpand(sInternal.valueEntry, TRUE);
	g_snprintf(buffer, sizeof(buffer), "%u", (unsigned)sInternal.value);
	gtk_entry_set_text(GTK_ENTRY(sInternal.valueEntry), buffer);
	gtk_grid_attach(GTK_GRID(grid), sInternal.valueEntry, 1, 1, 1, 1);

	add_label(GTK_GRID(grid), "Width:", 2);
	sInternal.sizeCombo = gtk_combo_box_text_new();
	for (int i = 0; i < 4; i++) {
		g_snprintf(buffer, sizeof(buffer), "%d byte%s", i + 1, (i == 0) ? "" : "s");
		gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(sInternal.sizeCombo), buffer);
	}
	gtk_combo_box_set_active(GTK_COMBO_BOX(sInternal.sizeCombo), (int)sInternal.size);
	gtk_grid_attach(GTK_GRID(grid), sInternal.sizeCombo, 1, 2, 1, 1);

	add_label(GTK_GRID(grid), "Description:", 3);
	sInternal.descriptionEntry = gtk_entry_new();
	gtk_entry_set_max_length(GTK_ENTRY(sInternal.descriptionEntry), 1023);
	gtk_widget_set_hexpand(sInternal.descriptionEntry, TRUE);
	if (editing) {
		gtk_entry_set_text(GTK_ENTRY(sInternal.descriptionEntry), cheat.description);
	}
	gtk_grid_attach(GTK_GRID(grid), sInternal.descriptionEntry, 1, 3, 1, 1);

	sInternal.enabledCheck = gtk_check_button_new_with_label("Enabled (freeze this value)");
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(sInternal.enabledCheck),
	                             sInternal.enabled ? TRUE : FALSE);
	gtk_grid_attach(GTK_GRID(grid), sInternal.enabledCheck, 1, 4, 1, 1);

	add_label(GTK_GRID(grid), "AR code:", 5);
	sInternal.codeLabel = gtk_label_new(NULL);
	gtk_widget_set_halign(sInternal.codeLabel, GTK_ALIGN_START);
	gtk_widget_set_tooltip_text(sInternal.codeLabel,
	                            "The equivalent Action Replay code, shown for reference only.");
	gtk_grid_attach(GTK_GRID(grid), sInternal.codeLabel, 1, 5, 1, 1);

	sInternal.hintLabel = gtk_label_new("");
	gtk_label_set_xalign(GTK_LABEL(sInternal.hintLabel), 0.0f);
	gtk_label_set_line_wrap(GTK_LABEL(sInternal.hintLabel), TRUE);
	gtk_widget_set_margin_start(sInternal.hintLabel, 12);
	gtk_widget_set_margin_end(sInternal.hintLabel, 12);
	gtk_container_add(GTK_CONTAINER(content), sInternal.hintLabel);

	sInternal.okButton = gtk_dialog_get_widget_for_response(GTK_DIALOG(dialog), GTK_RESPONSE_OK);

	g_signal_connect(G_OBJECT(sInternal.addressEntry), "changed",
	                 G_CALLBACK(internal_dialog_changed), NULL);
	g_signal_connect(G_OBJECT(sInternal.valueEntry), "changed",
	                 G_CALLBACK(internal_dialog_changed), NULL);
	g_signal_connect(G_OBJECT(sInternal.enabledCheck), "toggled",
	                 G_CALLBACK(internal_dialog_changed), NULL);
	g_signal_connect(G_OBJECT(sInternal.sizeCombo), "changed",
	                 G_CALLBACK(internal_dialog_size_changed), NULL);

	gtk_widget_show_all(dialog);
	internal_dialog_validate();
	gtk_widget_grab_focus(editing ? sInternal.valueEntry : sInternal.addressEntry);

	if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_OK) {
		char *description = g_strdup(gtk_entry_get_text(GTK_ENTRY(sInternal.descriptionEntry)));
		// CHEATS takes a non-const description but only copies it.
		if (sInternal.editing) {
			cheats->update(sInternal.size, sInternal.address, sInternal.value,
			               description, sInternal.enabled, sInternal.position);
		} else {
			cheats->add(sInternal.size, sInternal.address, sInternal.value,
			            description, sInternal.enabled);
		}
		g_free(description);

		mark_dirty();
		after_change(sInternal.editing ? (int)sInternal.position
		                               : (int)cheats->getListSize() - 1);
	}

	gtk_widget_destroy(dialog);
}

///////////////////////////////////////////////////////////////////////////
// Action Replay / Codebreaker dialog

struct CodeDialogState
{
	GtkWidget *dialog;
	GtkWidget *textView;
	GtkWidget *descriptionEntry;
	GtkWidget *enabledCheck;
	GtkWidget *hintLabel;
	GtkWidget *okButton;

	bool editing;
	size_t position;
	u8 type;
};

CodeDialogState sCode;

char *text_view_contents(GtkWidget *view)
{
	GtkTextBuffer *buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(view));
	GtkTextIter start;
	GtkTextIter end;
	gtk_text_buffer_get_bounds(buffer, &start, &end);
	return gtk_text_buffer_get_text(buffer, &start, &end, FALSE);
}

void code_dialog_validate(void)
{
	char *text = text_view_contents(sCode.textView);
	CHEATS_LIST parsed;
	const bool valid = (text != NULL) && CHEATS::XXCodeFromString(text, parsed) && (parsed.num > 0);

	if (valid) {
		char hint[64];
		g_snprintf(hint, sizeof(hint), "%u code line%s.", (unsigned)parsed.num,
		           (parsed.num == 1) ? "" : "s");
		gtk_label_set_text(GTK_LABEL(sCode.hintLabel), hint);
	} else {
		gtk_label_set_text(GTK_LABEL(sCode.hintLabel),
		                   "Enter the code as lines of two 8-digit hex words.");
	}

	gtk_widget_set_sensitive(sCode.okButton, valid ? TRUE : FALSE);
	g_free(text);
}

void code_dialog_changed(GtkTextBuffer *buffer, gpointer data)
{
	code_dialog_validate();
}

void code_cheat_dialog(bool editing, size_t position, u8 type)
{
	memset(&sCode, 0, sizeof(sCode));
	sCode.editing = editing;
	sCode.position = position;
	sCode.type = type;

	CHEATS_LIST cheat;
	const bool haveCheat = editing && cheats->copyItemFromIndex(position, cheat);

	const char *typeName = type_name(type);
	char title[64];
	g_snprintf(title, sizeof(title), "%s %s", editing ? "Edit" : "Add", typeName);

	GtkWidget *dialog = gtk_dialog_new_with_buttons(title,
	                                                GTK_WINDOW(sWindow),
	                                                kModalDialogFlags,
	                                                "_Cancel", GTK_RESPONSE_CANCEL,
	                                                editing ? "_Update" : "_Add", GTK_RESPONSE_OK,
	                                                NULL);
	sCode.dialog = dialog;
	gtk_window_set_default_size(GTK_WINDOW(dialog), 460, 380);

	GtkWidget *content = gtk_dialog_get_content_area(GTK_DIALOG(dialog));
	GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
	gtk_widget_set_margin_start(box, 12);
	gtk_widget_set_margin_end(box, 12);
	gtk_widget_set_margin_top(box, 12);
	gtk_container_add(GTK_CONTAINER(content), box);

	GtkWidget *label = gtk_label_new("Code:");
	gtk_widget_set_halign(label, GTK_ALIGN_START);
	gtk_box_pack_start(GTK_BOX(box), label, FALSE, FALSE, 0);

	GtkWidget *scrolled = gtk_scrolled_window_new(NULL, NULL);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled),
	                               GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
	gtk_scrolled_window_set_shadow_type(GTK_SCROLLED_WINDOW(scrolled), GTK_SHADOW_IN);
	gtk_widget_set_size_request(scrolled, -1, 180);
	gtk_box_pack_start(GTK_BOX(box), scrolled, TRUE, TRUE, 0);

	sCode.textView = gtk_text_view_new();
	gtk_text_view_set_monospace(GTK_TEXT_VIEW(sCode.textView), TRUE);
	gtk_text_view_set_left_margin(GTK_TEXT_VIEW(sCode.textView), 4);
	gtk_container_add(GTK_CONTAINER(scrolled), sCode.textView);

	if (haveCheat) {
		// The buffer has to hold every line, and each one is 18 characters.
		const size_t capacity = (size_t)MAX_XX_CODE * 18 + 1;
		char *codeText = (char *)calloc(capacity, 1);
		if (codeText != NULL) {
			CHEATS::StringFromXXCode(cheat, codeText);
			gtk_text_buffer_set_text(gtk_text_view_get_buffer(GTK_TEXT_VIEW(sCode.textView)),
			                         codeText, -1);
			free(codeText);
		}
	}

	GtkWidget *descriptionBox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
	gtk_box_pack_start(GTK_BOX(box), descriptionBox, FALSE, FALSE, 0);
	label = gtk_label_new("Description:");
	gtk_box_pack_start(GTK_BOX(descriptionBox), label, FALSE, FALSE, 0);
	sCode.descriptionEntry = gtk_entry_new();
	gtk_entry_set_max_length(GTK_ENTRY(sCode.descriptionEntry), 1023);
	gtk_widget_set_hexpand(sCode.descriptionEntry, TRUE);
	if (haveCheat) {
		gtk_entry_set_text(GTK_ENTRY(sCode.descriptionEntry), cheat.description);
	}
	gtk_box_pack_start(GTK_BOX(descriptionBox), sCode.descriptionEntry, TRUE, TRUE, 0);

	sCode.enabledCheck = gtk_check_button_new_with_label("Enabled");
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(sCode.enabledCheck),
	                             (!haveCheat || (cheat.enabled != 0)) ? TRUE : FALSE);
	gtk_box_pack_start(GTK_BOX(box), sCode.enabledCheck, FALSE, FALSE, 0);

	sCode.hintLabel = gtk_label_new("");
	gtk_widget_set_halign(sCode.hintLabel, GTK_ALIGN_START);
	gtk_box_pack_start(GTK_BOX(box), sCode.hintLabel, FALSE, FALSE, 0);

	sCode.okButton = gtk_dialog_get_widget_for_response(GTK_DIALOG(dialog), GTK_RESPONSE_OK);

	g_signal_connect(G_OBJECT(gtk_text_view_get_buffer(GTK_TEXT_VIEW(sCode.textView))), "changed",
	                 G_CALLBACK(code_dialog_changed), NULL);

	gtk_widget_show_all(dialog);
	code_dialog_validate();
	gtk_widget_grab_focus(sCode.textView);

	if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_OK) {
		char *code = text_view_contents(sCode.textView);
		char *description = g_strdup(gtk_entry_get_text(GTK_ENTRY(sCode.descriptionEntry)));
		const bool enabled = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(sCode.enabledCheck));

		// The code was validated by the OK button's sensitivity, so a failure
		// here would mean the core changed its mind; report it rather than
		// silently dropping the cheat.
		bool applied = false;
		if (type == CHEAT_TYPE_CODEBREAKER) {
			applied = editing ? cheats->update_CB(code, description, enabled, position)
			                  : cheats->add_CB(code, description, enabled);
		} else {
			applied = editing ? cheats->update_AR(code, description, enabled, position)
			                  : cheats->add_AR(code, description, enabled);
		}

		if (applied) {
			mark_dirty();
			after_change(editing ? (int)position : (int)cheats->getListSize() - 1);
		} else {
			show_error(GTK_WINDOW(sWindow), "The code could not be stored.");
		}

		g_free(description);
		g_free(code);
	}

	gtk_widget_destroy(dialog);
}

///////////////////////////////////////////////////////////////////////////
// Cheat database

std::string cheat_database_path(void)
{
	std::string path = config.cheats_database_path.get();
	if (!path.empty()) {
		return path;
	}

	gchar *baseName = g_build_filename(g_get_user_config_dir(), "desmume", "cheat.dat", NULL);
	path.assign(baseName);
	g_free(baseName);
	return path;
}

/* The database is a separate file the user has to point us at once, exactly as
 * the Windows port asks for it through its path settings. */
bool choose_cheat_database(GtkWindow *parent, std::string &outPath)
{
	GtkWidget *chooser = gtk_file_chooser_dialog_new("Select cheat database",
	                                                 parent,
	                                                 GTK_FILE_CHOOSER_ACTION_OPEN,
	                                                 "_Cancel", GTK_RESPONSE_CANCEL,
	                                                 "_Open", GTK_RESPONSE_ACCEPT,
	                                                 NULL);
	GtkFileFilter *filter = gtk_file_filter_new();
	gtk_file_filter_set_name(filter, "Cheat databases (*.dat)");
	gtk_file_filter_add_pattern(filter, "*.dat");
	gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(chooser), filter);

	GtkFileFilter *anyFilter = gtk_file_filter_new();
	gtk_file_filter_set_name(anyFilter, "All files");
	gtk_file_filter_add_pattern(anyFilter, "*");
	gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(chooser), anyFilter);

	if (g_file_test(outPath.c_str(), G_FILE_TEST_IS_REGULAR)) {
		gchar *directory = g_path_get_dirname(outPath.c_str());
		gtk_file_chooser_set_current_folder(GTK_FILE_CHOOSER(chooser), directory);
		g_free(directory);
	}

	const gint response = gtk_dialog_run(GTK_DIALOG(chooser));
	if (response == GTK_RESPONSE_ACCEPT) {
		gchar *fileName = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(chooser));
		if (fileName != NULL) {
			outPath.assign(fileName);
			g_free(fileName);
		}
	}
	gtk_widget_destroy(chooser);

	return response == GTK_RESPONSE_ACCEPT;
}

void cheat_database_dialog(void)
{
	if (cheats->getFilePath()[0] == '\0') {
		show_error(GTK_WINDOW(sWindow), "Load a ROM first: the database is searched by game code.");
		return;
	}

	std::string path = cheat_database_path();
	if (!g_file_test(path.c_str(), G_FILE_TEST_IS_REGULAR)) {
		if (!choose_cheat_database(GTK_WINDOW(sWindow), path)) {
			return;
		}
		config.cheats_database_path = path;
	}

	CHEATSEXPORT exporter;
	if (!exporter.load(path.c_str())) {
		char message[512];
		switch (exporter.getErrorCode()) {
		case CheatSystemError_FileOpenFailed:
			g_snprintf(message, sizeof(message), "Could not open the cheat database:\n%s", path.c_str());
			break;
		case CheatSystemError_FileFormatInvalid:
			g_snprintf(message, sizeof(message), "\"%s\" is not an R4 cheat database.", path.c_str());
			break;
		case CheatSystemError_GameNotFound:
			g_snprintf(message, sizeof(message),
			           "The database has no cheats for this game (%c%c%c%c).",
			           gameInfo.header.gameCode[0], gameInfo.header.gameCode[1],
			           gameInfo.header.gameCode[2], gameInfo.header.gameCode[3]);
			break;
		default:
			g_snprintf(message, sizeof(message), "The cheat database could not be read:\n%s", path.c_str());
			break;
		}
		show_error(GTK_WINDOW(sWindow), message);
		return;
	}

	const size_t count = exporter.getCheatsNum();
	CHEATS_LIST *items = exporter.getCheats();
	if ((count == 0) || (items == NULL)) {
		show_error(GTK_WINDOW(sWindow), "The database has no cheats for this game.");
		return;
	}

	GtkWidget *dialog = gtk_dialog_new_with_buttons("Export cheats from database",
	                                                GTK_WINDOW(sWindow),
	                                                kModalDialogFlags,
	                                                "_Cancel", GTK_RESPONSE_CANCEL,
	                                                "_Add", GTK_RESPONSE_OK,
	                                                NULL);
	gtk_window_set_default_size(GTK_WINDOW(dialog), 560, 460);

	GtkWidget *content = gtk_dialog_get_content_area(GTK_DIALOG(dialog));

	char header[512];
	g_snprintf(header, sizeof(header), "%s: %s", exporter.getDescription(), exporter.getGameTitle());
	GtkWidget *headerLabel = gtk_label_new(header);
	gtk_label_set_xalign(GTK_LABEL(headerLabel), 0.0f);
	gtk_widget_set_margin_start(headerLabel, 12);
	gtk_widget_set_margin_end(headerLabel, 12);
	gtk_container_add(GTK_CONTAINER(content), headerLabel);

	GtkWidget *scrolled = gtk_scrolled_window_new(NULL, NULL);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled),
	                               GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
	gtk_scrolled_window_set_shadow_type(GTK_SCROLLED_WINDOW(scrolled), GTK_SHADOW_IN);
	gtk_container_add(GTK_CONTAINER(content), scrolled);

	GtkListStore *databaseStore = gtk_list_store_new(2, G_TYPE_BOOLEAN, G_TYPE_STRING);
	GtkWidget *view = gtk_tree_view_new_with_model(GTK_TREE_MODEL(databaseStore));

	GtkCellRenderer *toggle = gtk_cell_renderer_toggle_new();
	g_object_set(toggle, "activatable", TRUE, NULL);
	GtkTreeViewColumn *column = gtk_tree_view_column_new_with_attributes("", toggle, "active", 0, NULL);
	gtk_tree_view_append_column(GTK_TREE_VIEW(view), column);

	GtkCellRenderer *text = gtk_cell_renderer_text_new();
	column = gtk_tree_view_column_new_with_attributes("Cheat", text, "text", 1, NULL);
	gtk_tree_view_column_set_expand(column, TRUE);
	gtk_tree_view_append_column(GTK_TREE_VIEW(view), column);

	for (size_t i = 0; i < count; i++) {
		GtkTreeIter iter;
		gtk_list_store_append(databaseStore, &iter);
		gtk_list_store_set(databaseStore, &iter, 0, FALSE, 1, items[i].description, -1);
	}
	gtk_container_add(GTK_CONTAINER(scrolled), view);
	gtk_widget_show_all(dialog);

	size_t added = 0;
	if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_OK) {
		GtkTreeModel *model = GTK_TREE_MODEL(databaseStore);
		GtkTreeIter iter;
		gboolean checked = FALSE;
		int row = 0;
		if (gtk_tree_model_get_iter_first(model, &iter)) {
			do {
				gtk_tree_model_get(model, &iter, 0, &checked, -1);
				if (checked && ((size_t)row < count)) {
					cheats->add_AR_Direct(items[row]);
					added++;
				}
				row++;
			} while (gtk_tree_model_iter_next(model, &iter));
		}
	}

	gtk_widget_destroy(dialog);
	g_object_unref(databaseStore);

	if (added > 0) {
		mark_dirty();
		after_change((int)cheats->getListSize() - 1);
	}
}

///////////////////////////////////////////////////////////////////////////
// List operations

void on_enabled_toggled(GtkCellRendererToggle *renderer, gchar *pathString, gpointer data)
{
	GtkTreeModel *model = GTK_TREE_MODEL(data);
	GtkTreePath *path = gtk_tree_path_new_from_string(pathString);
	GtkTreeIter iter;

	if (!gtk_tree_model_get_iter(model, &iter, path)) {
		gtk_tree_path_free(path);
		return;
	}
	gtk_tree_path_free(path);

	gboolean enabled = FALSE;
	int index = -1;
	gtk_tree_model_get(model, &iter, COL_ENABLED, &enabled, COL_INDEX, &index, -1);
	if (index < 0) {
		return;
	}

	/* The Windows list applies a checkbox click to every selected row, which is
	 * how you turn a block of cheats on or off at once. */
	GtkTreeView *view = GTK_TREE_VIEW((model == sCodeFilter) ? sCodeView : sInternalView);
	GtkTreeSelection *selection = gtk_tree_view_get_selection(view);
	GList *rows = gtk_tree_selection_get_selected_rows(selection, NULL);
	const bool applyToSelection = (rows != NULL) && (g_list_length(rows) > 1);

	if (applyToSelection) {
		const gboolean target = !enabled;
		for (GList *item = rows; item != NULL; item = item->next) {
			GtkTreeIter rowIter;
			if (!gtk_tree_model_get_iter(model, &rowIter, (GtkTreePath *)item->data)) {
				continue;
			}
			int rowIndex = -1;
			gtk_tree_model_get(model, &rowIter, COL_INDEX, &rowIndex, -1);
			if (rowIndex >= 0) {
				cheats->toggle((target != 0) ? true : false, (size_t)rowIndex);
			}
		}
		g_list_free_full(rows, (GDestroyNotify)gtk_tree_path_free);
		rebuild_store();
		select_index(index);
	} else {
		g_list_free_full(rows, (GDestroyNotify)gtk_tree_path_free);
		cheats->toggle((enabled != 0) ? false : true, (size_t)index);

		GtkTreeIter child;
		gtk_tree_model_filter_convert_iter_to_child_iter(GTK_TREE_MODEL_FILTER(model), &child, &iter);
		gtk_list_store_set(sStore, &child, COL_ENABLED, !enabled, -1);
	}

	mark_dirty();
	update_status();
}

void on_row_activated(GtkTreeView *view, GtkTreePath *path, GtkTreeViewColumn *column, gpointer data)
{
	const int index = selected_index();
	if (index < 0) {
		return;
	}

	CHEATS_LIST cheat;
	if (!cheats->copyItemFromIndex((size_t)index, cheat)) {
		return;
	}
	if (cheat.type == CHEAT_TYPE_INTERNAL) {
		internal_cheat_dialog(true, (size_t)index);
	} else {
		code_cheat_dialog(true, (size_t)index, cheat.type);
	}
}

void on_selection_changed(GtkTreeSelection *selection, gpointer data)
{
	update_buttons();
}

void on_add_internal(GtkMenuItem *item, gpointer data)
{
	internal_cheat_dialog(false, 0);
}

void on_add_ar(GtkMenuItem *item, gpointer data)
{
	code_cheat_dialog(false, 0, CHEAT_TYPE_AR);
}

void on_add_cb(GtkMenuItem *item, gpointer data)
{
	code_cheat_dialog(false, 0, CHEAT_TYPE_CODEBREAKER);
}

void on_edit(GtkButton *button, gpointer data)
{
	const int index = selected_index();
	if (index < 0) {
		return;
	}

	CHEATS_LIST cheat;
	if (!cheats->copyItemFromIndex((size_t)index, cheat)) {
		return;
	}
	if (cheat.type == CHEAT_TYPE_INTERNAL) {
		internal_cheat_dialog(true, (size_t)index);
	} else {
		code_cheat_dialog(true, (size_t)index, cheat.type);
	}
}

void on_remove(GtkButton *button, gpointer data)
{
	GtkTreeView *view = GTK_TREE_VIEW(active_view());
	GtkTreeSelection *selection = gtk_tree_view_get_selection(view);
	GList *rows = gtk_tree_selection_get_selected_rows(selection, NULL);
	if (rows == NULL) {
		return;
	}

	GtkTreeModel *model = active_model();
	GArray *indices = g_array_new(FALSE, FALSE, sizeof(int));
	for (GList *item = rows; item != NULL; item = item->next) {
		GtkTreeIter iter;
		if (gtk_tree_model_get_iter(model, &iter, (GtkTreePath *)item->data)) {
			int index = -1;
			gtk_tree_model_get(model, &iter, COL_INDEX, &index, -1);
			if (index >= 0) {
				g_array_append_val(indices, index);
			}
		}
	}
	g_list_free_full(rows, (GDestroyNotify)gtk_tree_path_free);

	if (indices->len == 0) {
		g_array_free(indices, TRUE);
		return;
	}

	char question[128];
	g_snprintf(question, sizeof(question), "Remove %u selected cheat%s?",
	           indices->len, (indices->len == 1) ? "" : "s");
	if (!ask_confirm(GTK_WINDOW(sWindow), question, "_Remove")) {
		g_array_free(indices, TRUE);
		return;
	}

	// Highest index first, so the ones still to remove keep their positions.
	g_array_sort(indices, [](gconstpointer a, gconstpointer b) {
		return *(const int *)b - *(const int *)a;
	});
	int selectAfter = G_MAXINT;
	for (guint i = 0; i < indices->len; i++) {
		const int index = g_array_index(indices, int, i);
		if (cheats->remove((size_t)index)) {
			if (index < selectAfter) {
				selectAfter = index;
			}
		}
	}
	g_array_free(indices, TRUE);

	if (selectAfter == G_MAXINT) {
		selectAfter = -1;
	} else if ((size_t)selectAfter >= cheats->getListSize()) {
		selectAfter = (int)cheats->getListSize() - 1;
	}

	mark_dirty();
	after_change(selectAfter);
}

void move_selected(int direction)
{
	const int index = selected_index();
	if (index < 0) {
		return;
	}

	CHEATS_LIST cheat;
	if (!cheats->copyItemFromIndex((size_t)index, cheat)) {
		return;
	}

	/* Move past the nearest cheat of the same kind, so the two pages keep
	 * showing their rows in the order the file has them. */
	int target = -1;
	for (int i = index + direction; (i >= 0) && (i < (int)cheats->getListSize()); i += direction) {
		CHEATS_LIST other;
		if (!cheats->copyItemFromIndex((size_t)i, other)) {
			break;
		}
		if (other.type == cheat.type) {
			target = i;
			break;
		}
	}
	if (target < 0) {
		return;
	}

	if (!cheats->move((size_t)index, (size_t)target)) {
		return;
	}

	mark_dirty();
	after_change(target);
}

void on_move_up(GtkButton *button, gpointer data)
{
	move_selected(-1);
}

void on_move_down(GtkButton *button, gpointer data)
{
	move_selected(1);
}

void on_database(GtkButton *button, gpointer data)
{
	cheat_database_dialog();
}

void on_notebook_switch(GtkNotebook *notebook, GtkWidget *page, guint pageNum, gpointer data)
{
	update_buttons();
}

///////////////////////////////////////////////////////////////////////////
// Closing

/* Returns true when the window may close, having saved or reverted the list on
 * the way out. The Windows dialog asks the same question, and a "no" there
 * puts the cheat list back the way it was rather than leaving edits that were
 * never written. */
bool confirm_close(void)
{
	if (!sDirty) {
		return true;
	}

	GtkWidget *dialog = gtk_message_dialog_new(GTK_WINDOW(sWindow),
	                                           kModalDialogFlags,
	                                           GTK_MESSAGE_QUESTION,
	                                           GTK_BUTTONS_NONE,
	                                           "%s", "Save changes to the cheat list?");
	gtk_message_dialog_format_secondary_text(GTK_MESSAGE_DIALOG(dialog),
	                                         "Unsaved changes are discarded when the window closes.");
	gtk_dialog_add_button(GTK_DIALOG(dialog), "_Cancel", GTK_RESPONSE_CANCEL);
	gtk_dialog_add_button(GTK_DIALOG(dialog), "_Discard", GTK_RESPONSE_NO);
	gtk_dialog_add_button(GTK_DIALOG(dialog), "_Save", GTK_RESPONSE_YES);
	gtk_dialog_set_default_response(GTK_DIALOG(dialog), GTK_RESPONSE_YES);

	const gint response = gtk_dialog_run(GTK_DIALOG(dialog));
	gtk_widget_destroy(dialog);

	if (response == GTK_RESPONSE_CANCEL) {
		return false;
	}

	if (response == GTK_RESPONSE_YES) {
		if (!cheats->save()) {
			char message[512];
			g_snprintf(message, sizeof(message),
			           "The cheat list could not be saved to:\n%s", cheats->getFilePath());
			show_error(GTK_WINDOW(sWindow), message);
			return false;
		}
		sDirty = false;
		return true;
	}

	if (sSnapshot != NULL) {
		*cheats = *sSnapshot;
	}
	sDirty = false;
	return true;
}

void on_close_clicked(GtkButton *button, gpointer data)
{
	if (confirm_close()) {
		gtk_widget_destroy(sWindow);
	}
}

gboolean on_delete_event(GtkWidget *widget, GdkEvent *event, gpointer data)
{
	return confirm_close() ? FALSE : TRUE;
}

void on_destroy(GtkWidget *widget, gpointer data)
{
	delete sSnapshot;
	sSnapshot = NULL;
	sWindow = NULL;
	sStore = NULL;
	sInternalFilter = NULL;
	sCodeFilter = NULL;
	sNotebook = NULL;
	sInternalView = NULL;
	sCodeView = NULL;

	if (sWasRunning) {
		Launch(NULL, NULL, NULL);
	}
}

///////////////////////////////////////////////////////////////////////////
// Window

void append_text_column(GtkWidget *view, const char *title, int column,
                        gboolean expand, gboolean monospace, gfloat xalign)
{
	GtkCellRenderer *renderer = gtk_cell_renderer_text_new();
	if (xalign != 0.0f) {
		g_object_set(renderer, "xalign", xalign, NULL);
	}
	if (monospace) {
		g_object_set(renderer, "family", "monospace", "family-set", TRUE, NULL);
	}

	GtkTreeViewColumn *treeColumn = gtk_tree_view_column_new_with_attributes(title, renderer,
	                                                                        "text", column, NULL);
	gtk_tree_view_column_set_expand(treeColumn, expand);
	gtk_tree_view_column_set_resizable(treeColumn, TRUE);
	gtk_tree_view_column_set_min_width(treeColumn, 48);
	gtk_tree_view_append_column(GTK_TREE_VIEW(view), treeColumn);
}

GtkWidget *create_view(GtkTreeModel *model, gboolean codes)
{
	GtkWidget *view = gtk_tree_view_new_with_model(model);
	gtk_tree_view_set_headers_clickable(GTK_TREE_VIEW(view), FALSE);
	gtk_tree_view_set_tooltip_column(GTK_TREE_VIEW(view), COL_DESC);

	GtkCellRenderer *toggle = gtk_cell_renderer_toggle_new();
	g_object_set(toggle, "activatable", TRUE, NULL);
	g_signal_connect(toggle, "toggled", G_CALLBACK(on_enabled_toggled), model);
	GtkTreeViewColumn *column = gtk_tree_view_column_new_with_attributes("On", toggle,
	                                                                    "active", COL_ENABLED, NULL);
	gtk_tree_view_column_set_min_width(column, 40);
	gtk_tree_view_append_column(GTK_TREE_VIEW(view), column);

	if (codes) {
		append_text_column(view, "Type", COL_TYPE, FALSE, FALSE, 0.0f);
		append_text_column(view, "Code", COL_CODE, TRUE, TRUE, 0.0f);
		append_text_column(view, "Lines", COL_LINES, FALSE, FALSE, 1.0f);
	} else {
		append_text_column(view, "Size", COL_SIZE, FALSE, FALSE, 1.0f);
		append_text_column(view, "Address", COL_ADDRESS, FALSE, TRUE, 0.0f);
		append_text_column(view, "Value", COL_VALUE, FALSE, TRUE, 0.0f);
	}
	append_text_column(view, "Description", COL_DESC, TRUE, FALSE, 0.0f);

	GtkTreeSelection *selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(view));
	gtk_tree_selection_set_mode(selection, GTK_SELECTION_MULTIPLE);
	g_signal_connect(selection, "changed", G_CALLBACK(on_selection_changed), NULL);
	g_signal_connect(view, "row-activated", G_CALLBACK(on_row_activated), NULL);

	return view;
}

GtkWidget *create_scrolled_view(GtkWidget *view)
{
	GtkWidget *scrolled = gtk_scrolled_window_new(NULL, NULL);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled),
	                               GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
	gtk_scrolled_window_set_shadow_type(GTK_SCROLLED_WINDOW(scrolled), GTK_SHADOW_IN);
	gtk_container_add(GTK_CONTAINER(scrolled), view);
	return scrolled;
}

} // namespace

///////////////////////////////////////////////////////////////////////////
// Entry point

void CheatList(GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
	if (sWindow != NULL) {
		gtk_window_present(GTK_WINDOW(sWindow));
		return;
	}

	sWasRunning = desmume_running();
	Pause(NULL, NULL, NULL);

	// Edits go straight into the live list, so keep the state the window
	// opened with for the "Discard" answer above.
	sSnapshot = new CHEATS(*cheats);
	sDirty = false;

	sWindow = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	gtk_window_set_default_size(GTK_WINDOW(sWindow), 820, 520);
	gtk_window_set_position(GTK_WINDOW(sWindow), GTK_WIN_POS_CENTER_ON_PARENT);
	gtk_window_set_modal(GTK_WINDOW(sWindow), TRUE);

	sStore = gtk_list_store_new(NUM_COLUMNS,
	                            G_TYPE_BOOLEAN, G_TYPE_INT, G_TYPE_INT, G_TYPE_STRING,
	                            G_TYPE_INT, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING,
	                            G_TYPE_INT, G_TYPE_STRING);
	rebuild_store();

	sInternalFilter = gtk_tree_model_filter_new(GTK_TREE_MODEL(sStore), NULL);
	gtk_tree_model_filter_set_visible_func(GTK_TREE_MODEL_FILTER(sInternalFilter),
	                                       filter_internal, NULL, NULL);
	sCodeFilter = gtk_tree_model_filter_new(GTK_TREE_MODEL(sStore), NULL);
	gtk_tree_model_filter_set_visible_func(GTK_TREE_MODEL_FILTER(sCodeFilter),
	                                       filter_code, NULL, NULL);

	sInternalView = create_view(sInternalFilter, FALSE);
	sCodeView = create_view(sCodeFilter, TRUE);

	sNotebook = gtk_notebook_new();
	gtk_notebook_append_page(GTK_NOTEBOOK(sNotebook), create_scrolled_view(sInternalView),
	                         gtk_label_new("Internal cheats"));
	gtk_notebook_append_page(GTK_NOTEBOOK(sNotebook), create_scrolled_view(sCodeView),
	                         gtk_label_new("Action Replay and Codebreaker"));
	gtk_notebook_set_scrollable(GTK_NOTEBOOK(sNotebook), TRUE);
	g_signal_connect(sNotebook, "switch-page", G_CALLBACK(on_notebook_switch), NULL);

	GtkWidget *addButton = gtk_menu_button_new();
	gtk_button_set_label(GTK_BUTTON(addButton), "Add");
	gtk_widget_set_tooltip_text(addButton, "Add an internal cheat or a full code");
	GtkWidget *addMenu = gtk_menu_new();
	GtkWidget *item = gtk_menu_item_new_with_label("Internal cheat...");
	g_signal_connect(item, "activate", G_CALLBACK(on_add_internal), NULL);
	gtk_menu_shell_append(GTK_MENU_SHELL(addMenu), item);
	item = gtk_menu_item_new_with_label("Action Replay code...");
	g_signal_connect(item, "activate", G_CALLBACK(on_add_ar), NULL);
	gtk_menu_shell_append(GTK_MENU_SHELL(addMenu), item);
	item = gtk_menu_item_new_with_label("Codebreaker code...");
	g_signal_connect(item, "activate", G_CALLBACK(on_add_cb), NULL);
	gtk_menu_shell_append(GTK_MENU_SHELL(addMenu), item);
	gtk_widget_show_all(addMenu);
	gtk_menu_button_set_popup(GTK_MENU_BUTTON(addButton), addMenu);

	sEditButton = gtk_button_new_with_label("Edit");
	g_signal_connect(sEditButton, "clicked", G_CALLBACK(on_edit), NULL);
	sRemoveButton = gtk_button_new_with_label("Remove");
	g_signal_connect(sRemoveButton, "clicked", G_CALLBACK(on_remove), NULL);
	sMoveUpButton = gtk_button_new_from_icon_name("go-up-symbolic", GTK_ICON_SIZE_BUTTON);
	gtk_widget_set_tooltip_text(sMoveUpButton, "Move the selected cheat up");
	g_signal_connect(sMoveUpButton, "clicked", G_CALLBACK(on_move_up), NULL);
	sMoveDownButton = gtk_button_new_from_icon_name("go-down-symbolic", GTK_ICON_SIZE_BUTTON);
	gtk_widget_set_tooltip_text(sMoveDownButton, "Move the selected cheat down");
	g_signal_connect(sMoveDownButton, "clicked", G_CALLBACK(on_move_down), NULL);
	GtkWidget *databaseButton = gtk_button_new_with_label("Cheat Database...");
	gtk_widget_set_tooltip_text(databaseButton, "Copy cheats for this game from an R4 database");
	g_signal_connect(databaseButton, "clicked", G_CALLBACK(on_database), NULL);
	GtkWidget *closeButton = gtk_button_new_with_label("Close");
	g_signal_connect(closeButton, "clicked", G_CALLBACK(on_close_clicked), NULL);

	sStatusLabel = gtk_label_new("");
	gtk_label_set_xalign(GTK_LABEL(sStatusLabel), 0.0f);
	gtk_widget_set_margin_start(sStatusLabel, 6);

	GtkWidget *buttonBox = gtk_button_box_new(GTK_ORIENTATION_HORIZONTAL);
	gtk_button_box_set_layout(GTK_BUTTON_BOX(buttonBox), GTK_BUTTONBOX_END);
	gtk_box_set_spacing(GTK_BOX(buttonBox), 4);
	gtk_box_pack_start(GTK_BOX(buttonBox), sMoveUpButton, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(buttonBox), sMoveDownButton, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(buttonBox), sEditButton, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(buttonBox), sRemoveButton, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(buttonBox), databaseButton, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(buttonBox), addButton, FALSE, FALSE, 0);
	gtk_box_pack_end(GTK_BOX(buttonBox), closeButton, FALSE, FALSE, 0);

	GtkWidget *mainBox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
	gtk_container_set_border_width(GTK_CONTAINER(mainBox), 6);
	gtk_box_pack_start(GTK_BOX(mainBox), sNotebook, TRUE, TRUE, 0);
	gtk_box_pack_start(GTK_BOX(mainBox), sStatusLabel, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(mainBox), buttonBox, FALSE, FALSE, 0);
	gtk_container_add(GTK_CONTAINER(sWindow), mainBox);

	g_signal_connect(G_OBJECT(sWindow), "delete-event", G_CALLBACK(on_delete_event), NULL);
	g_signal_connect(G_OBJECT(sWindow), "destroy", G_CALLBACK(on_destroy), NULL);

	gtk_widget_show_all(sWindow);
	update_buttons();
	update_status();
}

///////////////////////////////////////////////////////////////////////////
// Cheat search

namespace {

GtkWidget *sSearchWindow = NULL;
GtkWidget *sSearchSizeCombo = NULL;
GtkWidget *sSearchSignCombo = NULL;
GtkWidget *sSearchTypeCombo = NULL;
GtkWidget *sSearchValueEntry = NULL;
GtkWidget *sSearchCompareCombo = NULL;
GtkWidget *sSearchValueBox = NULL;
GtkWidget *sSearchCompareBox = NULL;
GtkWidget *sSearchRangeLabel = NULL;
GtkWidget *sSearchCountLabel = NULL;
GtkWidget *sSearchHintLabel = NULL;
GtkWidget *sSearchButton = NULL;
GtkWidget *sSearchAddButton = NULL;
GtkListStore *sSearchStore = NULL;
GtkWidget *sSearchView = NULL;
/* CHEATSEARCH keeps its buffers until close() is called, so a search spans
 * several visits to this window: start it, go back to the game, come back and
 * narrow it down. That is the same flow the Windows dialog has, and why its
 * state lives here rather than in the window. */
bool sSearchActive = false;
u32 sSearchResults = 0;
std::string sSearchCheatPath;

// Big result sets are capped like the RAM Search window, because GtkTreeView
// materialises a row per model entry.
const u32 kMaxSearchRows = 50000;

void search_update_controls(void)
{
	const gint sizeIndex = gtk_combo_box_get_active(GTK_COMBO_BOX(sSearchSizeCombo));
	const u8 size = ((sizeIndex >= 0) && (sizeIndex < 4)) ? (u8)sizeIndex : (u8)0;
	const gint signIndex = gtk_combo_box_get_active(GTK_COMBO_BOX(sSearchSignCombo));
	const gboolean isSigned = (signIndex == 1) ? TRUE : FALSE;
	const gint typeIndex = gtk_combo_box_get_active(GTK_COMBO_BOX(sSearchTypeCombo));
	const gboolean isExact = (typeIndex == 0) ? TRUE : FALSE;

	const gint bits = 8 * (size + 1);

	char range[64];
	if (isSigned) {
		const gint64 lowest = -((gint64)1 << (bits - 1));
		const gint64 highest = ((gint64)1 << (bits - 1)) - 1;
		g_snprintf(range, sizeof(range), "[%" G_GINT64_FORMAT "..%" G_GINT64_FORMAT "]",
		           lowest, highest);
	} else if (size == 3) {
		g_snprintf(range, sizeof(range), "[0..4294967295]");
	} else {
		g_snprintf(range, sizeof(range), "[0..%u]", (unsigned)size_max_value(size));
	}
	gtk_label_set_text(GTK_LABEL(sSearchRangeLabel), range);

	gtk_widget_set_visible(sSearchValueBox, isExact);
	gtk_widget_set_visible(sSearchCompareBox, isExact ? FALSE : TRUE);

	// The width, sign and type are fixed once the scan has captured memory.
	gtk_widget_set_sensitive(sSearchSizeCombo, sSearchActive ? FALSE : TRUE);
	gtk_widget_set_sensitive(sSearchSignCombo, sSearchActive ? FALSE : TRUE);
	gtk_widget_set_sensitive(sSearchTypeCombo, sSearchActive ? FALSE : TRUE);

	char count[64];
	if (sSearchActive) {
		g_snprintf(count, sizeof(count), "%u result%s", (unsigned)sSearchResults,
		           (sSearchResults == 1) ? "" : "s");
	} else {
		g_snprintf(count, sizeof(count), "Search not started");
	}
	gtk_label_set_text(GTK_LABEL(sSearchCountLabel), count);

	if (!sSearchActive) {
		gtk_label_set_text(GTK_LABEL(sSearchHintLabel),
		                   "Start a search, then come back and narrow it down as the value changes.");
	} else if (isExact) {
		gtk_label_set_text(GTK_LABEL(sSearchHintLabel),
		                   "Enter the value the game shows now, then search again.");
	} else {
		gtk_label_set_text(GTK_LABEL(sSearchHintLabel),
		                   "Return to the game, let the value change, then search again.");
	}
}

void search_rebuild_results(void)
{
	gtk_list_store_clear(sSearchStore);
	if (!sSearchActive) {
		return;
	}

	const u32 limit = (sSearchResults > kMaxSearchRows) ? kMaxSearchRows : sSearchResults;
	const u32 size = (u32)gtk_combo_box_get_active(GTK_COMBO_BOX(sSearchSizeCombo));

	cheatSearch->getListReset();
	u32 address = 0;
	u32 value = 0;
	u32 row = 0;
	while ((row < limit) && cheatSearch->getList(&address, &value)) {
		char addressText[16];
		char valueText[16];
		g_snprintf(addressText, sizeof(addressText), "0x02%06X", address);
		g_snprintf(valueText, sizeof(valueText), "%u", value);

		GtkTreeIter iter;
		gtk_list_store_append(sSearchStore, &iter);
		gtk_list_store_set(sSearchStore, &iter,
		                   0, addressText, 1, valueText, 2, (int)(address & 0x0FFFFFFF),
		                   3, (int)value, 4, (int)size, -1);
		row++;
	}
}

void search_update_selection_state(void)
{
	GtkTreeSelection *selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(sSearchView));
	gtk_widget_set_sensitive(sSearchAddButton,
	                         (gtk_tree_selection_count_selected_rows(selection) == 1) ? TRUE : FALSE);
}

void on_search_selection_changed(GtkTreeSelection *selection, gpointer data)
{
	search_update_selection_state();
}

void search_apply(gboolean exact)
{
	if (exact) {
		const char *text = gtk_entry_get_text(GTK_ENTRY(sSearchValueEntry));
		const gint sizeIndex = gtk_combo_box_get_active(GTK_COMBO_BOX(sSearchSizeCombo));
		const u8 size = ((sizeIndex >= 0) && (sizeIndex < 4)) ? (u8)sizeIndex : (u8)0;

		u32 value = 0;
		if (!parse_cheat_value(text, size, &value)) {
			return;
		}
		sSearchResults = cheatSearch->search(value);
	} else {
		const gint compareIndex = gtk_combo_box_get_active(GTK_COMBO_BOX(sSearchCompareCombo));
		sSearchResults = cheatSearch->search((u8)((compareIndex < 0) ? 0 : compareIndex));
	}
}

void on_search_search(GtkButton *button, gpointer data)
{
	const gint sizeIndex = gtk_combo_box_get_active(GTK_COMBO_BOX(sSearchSizeCombo));
	const gint signIndex = gtk_combo_box_get_active(GTK_COMBO_BOX(sSearchSignCombo));
	const gint typeIndex = gtk_combo_box_get_active(GTK_COMBO_BOX(sSearchTypeCombo));
	const u8 size = ((sizeIndex >= 0) && (sizeIndex < 4)) ? (u8)sizeIndex : (u8)0;
	const u8 type = (typeIndex == 1) ? 1 : 0;
	const u8 sign = (signIndex == 1) ? 1 : 0;
	const gboolean exact = (type == 0) ? TRUE : FALSE;

	// A scan over 4 MiB takes a moment; say so before blocking the UI.
	gtk_label_set_text(GTK_LABEL(sSearchHintLabel), "Searching...");
	while (gtk_events_pending()) {
		gtk_main_iteration();
	}

	if (!sSearchActive) {
		if (!cheatSearch->start(type, size, sign)) {
			gtk_label_set_text(GTK_LABEL(sSearchHintLabel),
			                   "A search is already running. Restart it to change the settings.");
			search_update_controls();
			return;
		}
		sSearchActive = true;
		sSearchCheatPath = cheats->getFilePath();
		sSearchResults = 0;

		/* An exact search can be applied right away: the value is on screen.
		 * A comparative one cannot, because nothing has changed yet, so the
		 * first press only takes the snapshot. */
		if (exact) {
			search_apply(TRUE);
		}
	} else {
		search_apply(exact);
	}

	search_rebuild_results();
	search_update_controls();
	search_update_selection_state();
}

void on_search_restart(GtkButton *button, gpointer data)
{
	cheatSearch->close();
	sSearchActive = false;
	sSearchResults = 0;
	sSearchCheatPath.clear();

	gtk_list_store_clear(sSearchStore);
	search_update_controls();
	search_update_selection_state();
}

void on_search_add(GtkButton *button, gpointer data)
{
	GtkTreeSelection *selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(sSearchView));
	GtkTreeModel *model = NULL;
	GtkTreeIter iter;
	if (!gtk_tree_selection_get_selected(selection, &model, &iter)) {
		return;
	}

	int address = 0;
	int value = 0;
	int size = 0;
	gtk_tree_model_get(model, &iter, 2, &address, 3, &value, 4, &size, -1);

	char description[64];
	g_snprintf(description, sizeof(description), "Cheat search %08X", (unsigned)address);

	/* The address comes back relative to main RAM, which the cheat engine
	 * expects as 0x02xxxxxx. */
	cheats->add((u8)size, 0x02000000u | (u32)address, (u32)value, description, true);

	char status[128];
	if (cheats->save()) {
		g_snprintf(status, sizeof(status),
		           "Added a %d-byte cheat at 0x02%06X to the cheat list.",
		           size + 1, (unsigned)address);
	} else {
		g_snprintf(status, sizeof(status),
		           "Added a cheat at 0x02%06X, but the cheat list could not be saved.",
		           (unsigned)address);
	}
	gtk_label_set_text(GTK_LABEL(sSearchHintLabel), status);
}

void on_search_value_changed(GtkEditable *editable, gpointer data)
{
	const gint typeIndex = gtk_combo_box_get_active(GTK_COMBO_BOX(sSearchTypeCombo));
	if (typeIndex == 1) {
		// A comparative search has no value to validate before it runs.
		gtk_widget_set_sensitive(sSearchButton, TRUE);
		return;
	}

	const gint sizeIndex = gtk_combo_box_get_active(GTK_COMBO_BOX(sSearchSizeCombo));
	const u8 size = ((sizeIndex >= 0) && (sizeIndex < 4)) ? (u8)sizeIndex : (u8)0;
	u32 value = 0;
	const bool valid = parse_cheat_value(gtk_entry_get_text(GTK_ENTRY(sSearchValueEntry)),
	                                     size, &value);
	gtk_widget_set_sensitive(sSearchButton, valid ? TRUE : FALSE);
}

void on_search_settings_changed(GtkComboBox *combo, gpointer data)
{
	search_update_controls();
	on_search_value_changed(NULL, NULL);
}

void on_search_destroy(GtkWidget *widget, gpointer data)
{
	sSearchWindow = NULL;
	sSearchStore = NULL;
	sSearchView = NULL;
	sSearchSizeCombo = NULL;
	sSearchSignCombo = NULL;
	sSearchTypeCombo = NULL;
	sSearchValueEntry = NULL;
	sSearchCompareCombo = NULL;
	sSearchButton = NULL;
	sSearchAddButton = NULL;
	sSearchCountLabel = NULL;
	sSearchHintLabel = NULL;
	sSearchRangeLabel = NULL;

	if (sWasRunning) {
		Launch(NULL, NULL, NULL);
	}
}

} // namespace

void CheatSearch(GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
	if (sSearchWindow != NULL) {
		gtk_window_present(GTK_WINDOW(sSearchWindow));
		return;
	}

	sWasRunning = desmume_running();
	Pause(NULL, NULL, NULL);

	/* The search buffers belong to the ROM that was loaded when it started, so
	 * a new game drops them, as CheatsSearchReset() does on Windows. */
	if (sSearchActive && (sSearchCheatPath != cheats->getFilePath())) {
		cheatSearch->close();
		sSearchActive = false;
		sSearchResults = 0;
		sSearchCheatPath.clear();
	}

	sSearchWindow = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	gtk_window_set_title(GTK_WINDOW(sSearchWindow), "Cheat Search");
	gtk_window_set_default_size(GTK_WINDOW(sSearchWindow), 620, 480);
	gtk_window_set_position(GTK_WINDOW(sSearchWindow), GTK_WIN_POS_CENTER_ON_PARENT);
	gtk_window_set_modal(GTK_WINDOW(sSearchWindow), TRUE);

	GtkWidget *controls = gtk_grid_new();
	gtk_grid_set_row_spacing(GTK_GRID(controls), 6);
	gtk_grid_set_column_spacing(GTK_GRID(controls), 8);

	GtkWidget *label = gtk_label_new("Width:");
	gtk_widget_set_halign(label, GTK_ALIGN_END);
	gtk_grid_attach(GTK_GRID(controls), label, 0, 0, 1, 1);
	sSearchSizeCombo = gtk_combo_box_text_new();
	for (int i = 0; i < 4; i++) {
		char text[16];
		g_snprintf(text, sizeof(text), "%d byte%s", i + 1, (i == 0) ? "" : "s");
		gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(sSearchSizeCombo), text);
	}
	gtk_combo_box_set_active(GTK_COMBO_BOX(sSearchSizeCombo), 0);
	gtk_grid_attach(GTK_GRID(controls), sSearchSizeCombo, 1, 0, 1, 1);

	label = gtk_label_new("Sign:");
	gtk_widget_set_halign(label, GTK_ALIGN_END);
	gtk_grid_attach(GTK_GRID(controls), label, 2, 0, 1, 1);
	sSearchSignCombo = gtk_combo_box_text_new();
	gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(sSearchSignCombo), "Unsigned");
	gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(sSearchSignCombo), "Signed");
	gtk_combo_box_set_active(GTK_COMBO_BOX(sSearchSignCombo), 0);
	gtk_grid_attach(GTK_GRID(controls), sSearchSignCombo, 3, 0, 1, 1);

	label = gtk_label_new("Type:");
	gtk_widget_set_halign(label, GTK_ALIGN_END);
	gtk_grid_attach(GTK_GRID(controls), label, 0, 1, 1, 1);
	sSearchTypeCombo = gtk_combo_box_text_new();
	gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(sSearchTypeCombo), "Exact value");
	gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(sSearchTypeCombo), "Compared to previous");
	gtk_combo_box_set_active(GTK_COMBO_BOX(sSearchTypeCombo), 0);
	gtk_grid_attach(GTK_GRID(controls), sSearchTypeCombo, 1, 1, 1, 1);

	sSearchValueBox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
	sSearchValueEntry = gtk_entry_new();
	gtk_entry_set_max_length(GTK_ENTRY(sSearchValueEntry), 11);
	gtk_entry_set_width_chars(GTK_ENTRY(sSearchValueEntry), 12);
	gtk_box_pack_start(GTK_BOX(sSearchValueBox), gtk_label_new("Value:"), FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(sSearchValueBox), sSearchValueEntry, FALSE, FALSE, 0);
	sSearchRangeLabel = gtk_label_new("");
	gtk_box_pack_start(GTK_BOX(sSearchValueBox), sSearchRangeLabel, FALSE, FALSE, 0);
	gtk_grid_attach(GTK_GRID(controls), sSearchValueBox, 2, 1, 2, 1);

	sSearchCompareBox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
	sSearchCompareCombo = gtk_combo_box_text_new();
	gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(sSearchCompareCombo), "New value is greater");
	gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(sSearchCompareCombo), "New value is smaller");
	gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(sSearchCompareCombo), "New value is equal");
	gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(sSearchCompareCombo), "New value differs");
	gtk_combo_box_set_active(GTK_COMBO_BOX(sSearchCompareCombo), 3);
	gtk_box_pack_start(GTK_BOX(sSearchCompareBox), sSearchCompareCombo, FALSE, FALSE, 0);
	gtk_grid_attach(GTK_GRID(controls), sSearchCompareBox, 2, 2, 2, 1);

	sSearchButton = gtk_button_new_with_label("Search");
	GtkWidget *restartButton = gtk_button_new_with_label("Restart");
	GtkWidget *closeButton = gtk_button_new_with_label("Close");
	sSearchAddButton = gtk_button_new_with_label("Add to cheat list");

	GtkWidget *buttonBox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
	gtk_box_pack_start(GTK_BOX(buttonBox), sSearchButton, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(buttonBox), restartButton, FALSE, FALSE, 0);
	gtk_box_pack_end(GTK_BOX(buttonBox), closeButton, FALSE, FALSE, 0);
	gtk_box_pack_end(GTK_BOX(buttonBox), sSearchAddButton, FALSE, FALSE, 0);

	sSearchCountLabel = gtk_label_new("");
	gtk_widget_set_halign(sSearchCountLabel, GTK_ALIGN_START);
	sSearchHintLabel = gtk_label_new("");
	gtk_label_set_xalign(GTK_LABEL(sSearchHintLabel), 0.0f);
	gtk_label_set_line_wrap(GTK_LABEL(sSearchHintLabel), TRUE);

	sSearchStore = gtk_list_store_new(5, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_INT,
	                                  G_TYPE_INT, G_TYPE_INT);
	sSearchView = gtk_tree_view_new_with_model(GTK_TREE_MODEL(sSearchStore));

	GtkCellRenderer *renderer = gtk_cell_renderer_text_new();
	g_object_set(renderer, "family", "monospace", "family-set", TRUE, NULL);
	GtkTreeViewColumn *column = gtk_tree_view_column_new_with_attributes("Address", renderer,
	                                                                    "text", 0, NULL);
	gtk_tree_view_column_set_expand(column, TRUE);
	gtk_tree_view_append_column(GTK_TREE_VIEW(sSearchView), column);

	renderer = gtk_cell_renderer_text_new();
	g_object_set(renderer, "family", "monospace", "family-set", TRUE, "xalign", 1.0f, NULL);
	column = gtk_tree_view_column_new_with_attributes("Value", renderer, "text", 1, NULL);
	gtk_tree_view_column_set_expand(column, TRUE);
	gtk_tree_view_append_column(GTK_TREE_VIEW(sSearchView), column);

	GtkTreeSelection *selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(sSearchView));
	gtk_tree_selection_set_mode(selection, GTK_SELECTION_SINGLE);
	g_signal_connect(selection, "changed", G_CALLBACK(on_search_selection_changed), NULL);

	GtkWidget *scrolled = gtk_scrolled_window_new(NULL, NULL);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled),
	                               GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
	gtk_scrolled_window_set_shadow_type(GTK_SCROLLED_WINDOW(scrolled), GTK_SHADOW_IN);
	gtk_container_add(GTK_CONTAINER(scrolled), sSearchView);

	GtkWidget *mainBox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
	gtk_container_set_border_width(GTK_CONTAINER(mainBox), 6);
	gtk_box_pack_start(GTK_BOX(mainBox), controls, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(mainBox), sSearchCountLabel, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(mainBox), scrolled, TRUE, TRUE, 0);
	gtk_box_pack_start(GTK_BOX(mainBox), sSearchHintLabel, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(mainBox), buttonBox, FALSE, FALSE, 0);
	gtk_container_add(GTK_CONTAINER(sSearchWindow), mainBox);

	g_signal_connect(sSearchButton, "clicked", G_CALLBACK(on_search_search), NULL);
	g_signal_connect(restartButton, "clicked", G_CALLBACK(on_search_restart), NULL);
	g_signal_connect(sSearchAddButton, "clicked", G_CALLBACK(on_search_add), NULL);
	g_signal_connect_swapped(closeButton, "clicked", G_CALLBACK(gtk_widget_destroy), sSearchWindow);
	g_signal_connect(sSearchValueEntry, "changed", G_CALLBACK(on_search_value_changed), NULL);
	g_signal_connect(sSearchSizeCombo, "changed", G_CALLBACK(on_search_settings_changed), NULL);
	g_signal_connect(sSearchSignCombo, "changed", G_CALLBACK(on_search_settings_changed), NULL);
	g_signal_connect(sSearchTypeCombo, "changed", G_CALLBACK(on_search_settings_changed), NULL);
	g_signal_connect(G_OBJECT(sSearchWindow), "destroy", G_CALLBACK(on_search_destroy), NULL);

	gtk_widget_show_all(sSearchWindow);

	// Coming back to a search that is still running shows its results again.
	search_rebuild_results();
	search_update_controls();
	on_search_value_changed(NULL, NULL);
	search_update_selection_state();
}
