/*
	cheats.cpp - cheat list tool for the DeSmuME GTK4 frontend

	Port of gtk/cheatsGTK.cpp's CheatList window.  The cheat document itself
	is still the core CHEATS object (cheatSystem.h); only the UI is rewritten:

	  - GtkTreeView + GtkListStore + two GtkTreeModelFilters -> two GListStores
	    of CheatRow objects shown in GtkColumnViews (one page per cheat type).
	  - The activatable "On" cell renderer -> a GtkCheckButton column factory.
	  - All seven gtk_dialog_run() modal dialogs -> plain GtkWindows with async
	    callbacks and GtkAlertDialog / GtkFileDialog (GTK 4.10+).
	  - GtkMenuButton + GtkMenu -> GtkMenuButton + GMenu with win.* actions.

	The cheat engine's data and parity details (address masking, value ranges,
	the AR code preview) are deliberately identical to the GTK3/Windows ports.
*/

#include "cheats.h"
#include "../emulation.h"
#include "../config.h"
#include "rowmodel.h"

#include <string>
#include <vector>
#include <algorithm>

#include "cheatSystem.h"
#include "NDSSystem.h"

#define TOOL_NAME "Cheat List"

namespace {

/* ------------------------------------------------------------------ constants */

const char *kTypeNames[] = { "Internal", "Action Replay", "Codebreaker" };

const char *type_name(u8 type)
{
	switch (type) {
	case CHEAT_TYPE_INTERNAL:    return kTypeNames[0];
	case CHEAT_TYPE_AR:          return kTypeNames[1];
	case CHEAT_TYPE_CODEBREAKER: return kTypeNames[2];
	default:                     return "Unknown";
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

/* The Windows dialog previews the Action Replay code an internal cheat is
 * equivalent to; keep the same encoding so both ports show the same string. */
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

/* ----------------------------------------------------------------------- state */

static GtkWidget *sWindow = NULL;
static GtkWidget *sNotebook = NULL;
static GtkWidget *sInternalView = NULL;
static GtkWidget *sCodeView = NULL;
static GListStore *sInternalStore = NULL;
static GListStore *sCodeStore = NULL;

static GtkWidget *sSearchEntry = NULL;
static GtkWidget *sEditButton = NULL;
static GtkWidget *sRemoveButton = NULL;
static GtkWidget *sMoveUpButton = NULL;
static GtkWidget *sMoveDownButton = NULL;
static GtkWidget *sStatusLabel = NULL;
static std::string sFilterText;

/* The list is edited in place.  A copy taken when the window opens lets
 * "Discard" put it back the way it was, which is what the Windows dialog does
 * when its Cancel button is answered with "No". */
static CHEATS *sSnapshot = NULL;
static bool sDirty = false;
static bool sWasRunning = false;

/* ------------------------------------------------------------------ CheatRow */

/* One row in a ColumnView store.  Holds the index into the live core list so
 * edit/toggle/remove can map back, plus preformatted display strings. */
struct CheatRow
{
	GObject parent;
	int index;            /* position in cheats->list, -1 if stale */
	u8 type;
	gboolean enabled;
	gchar *typeText;
	gchar *sizeText;
	gchar *addressText;
	gchar *valueText;
	gchar *codeText;
	gchar *linesText;
	gchar *description;
};

typedef struct
{
	GObjectClass parent_class;
} CheatRowClass;

GType cheat_row_get_type(void);
#define CHEAT_ROW(object) ((CheatRow *)(object))

G_DEFINE_TYPE(CheatRow, cheat_row, G_TYPE_OBJECT)

static void
cheat_row_finalize(GObject *object)
{
	CheatRow *row = (CheatRow *)object;
	g_free(row->typeText);
	g_free(row->sizeText);
	g_free(row->addressText);
	g_free(row->valueText);
	g_free(row->codeText);
	g_free(row->linesText);
	g_free(row->description);
	G_OBJECT_CLASS(cheat_row_parent_class)->finalize(object);
}

static void
cheat_row_class_init(CheatRowClass *klass)
{
	G_OBJECT_CLASS(klass)->finalize = cheat_row_finalize;
}

static void
cheat_row_init(CheatRow *row)
{
	row->index = -1;
	row->type = CHEAT_TYPE_EMPTY;
	row->enabled = FALSE;
	row->typeText = NULL;
	row->sizeText = NULL;
	row->addressText = NULL;
	row->valueText = NULL;
	row->codeText = NULL;
	row->linesText = NULL;
	row->description = NULL;
}

static CheatRow *
cheat_row_new(void)
{
	return CHEAT_ROW(g_object_new(cheat_row_get_type(), NULL));
}

/* Which of a row's display strings backs a given column. */
enum CheatColumn
{
	COL_TYPE = 0,
	COL_SIZE,
	COL_ADDRESS,
	COL_VALUE,
	COL_CODE,
	COL_LINES,
	COL_DESC
};

static const char *
cheat_row_column(CheatRow *row, int column)
{
	const gchar *value = NULL;
	switch (column) {
	case COL_TYPE:    value = row->typeText;    break;
	case COL_SIZE:    value = row->sizeText;    break;
	case COL_ADDRESS: value = row->addressText; break;
	case COL_VALUE:   value = row->valueText;   break;
	case COL_CODE:    value = row->codeText;    break;
	case COL_LINES:   value = row->linesText;   break;
	case COL_DESC:    value = row->description; break;
	default: break;
	}
	return (value != NULL) ? value : "";
}

/* ------------------------------------------------------------------- factories */

static void
text_factory_setup(GtkListItemFactory *factory, GtkListItem *item, gpointer data)
{
	(void)factory;
	(void)data;
	GtkWidget *label = gtk_label_new(NULL);
	gtk_label_set_xalign(GTK_LABEL(label), 0.0f);
	gtk_widget_add_css_class(label, "desmume-tool-cell");
	g_object_set_data(G_OBJECT(item), "cell-label", label);
	gtk_list_item_set_child(item, label);
}

static void
text_factory_bind(GtkListItemFactory *factory, GtkListItem *item, gpointer data)
{
	(void)factory;
	GtkWidget *label = GTK_WIDGET(g_object_get_data(G_OBJECT(item), "cell-label"));
	CheatRow *row = CHEAT_ROW(gtk_list_item_get_item(item));
	if ((label != NULL) && (row != NULL))
	{
		gtk_label_set_text(GTK_LABEL(label),
		                   cheat_row_column(row, GPOINTER_TO_INT(data)));
	}
}

static void
toggle_factory_setup(GtkListItemFactory *factory, GtkListItem *item, gpointer data)
{
	(void)factory;
	(void)data;
	GtkWidget *check = gtk_check_button_new();
	g_object_set_data(G_OBJECT(item), "cell-toggle", check);
	gtk_list_item_set_child(item, check);
	/* The handler is connected in bind(), where we know which row is shown;
	 * it is disconnected on unbind so recycled list items never toggle the
	 * wrong cheat. */
}

static void on_toggle_toggled(GtkCheckButton *check, gpointer data);

static void
toggle_factory_bind(GtkListItemFactory *factory, GtkListItem *item, gpointer data)
{
	(void)factory;
	(void)data;
	GtkWidget *check = GTK_WIDGET(g_object_get_data(G_OBJECT(item), "cell-toggle"));
	CheatRow *row = CHEAT_ROW(gtk_list_item_get_item(item));
	if ((check == NULL) || (row == NULL))
	{
		return;
	}
	g_object_set_data(G_OBJECT(check), "cheat-row", row);
	g_signal_handlers_block_by_func(check, (gpointer)on_toggle_toggled, NULL);
	gtk_check_button_set_active(GTK_CHECK_BUTTON(check), row->enabled);
	g_signal_handlers_unblock_by_func(check, (gpointer)on_toggle_toggled, NULL);
	g_signal_connect(check, "toggled", G_CALLBACK(on_toggle_toggled), NULL);
}

static void
toggle_factory_unbind(GtkListItemFactory *factory, GtkListItem *item, gpointer data)
{
	(void)factory;
	(void)data;
	GtkWidget *check = GTK_WIDGET(g_object_get_data(G_OBJECT(item), "cell-toggle"));
	if (check != NULL)
	{
		g_signal_handlers_disconnect_by_func(check,
		                                     (gpointer)on_toggle_toggled, NULL);
		g_object_set_data(G_OBJECT(check), "cheat-row", NULL);
	}
}

static GtkListItemFactory *
new_text_factory(int column)
{
	GtkListItemFactory *factory = gtk_signal_list_item_factory_new();
	g_signal_connect(factory, "setup", G_CALLBACK(text_factory_setup), NULL);
	g_signal_connect(factory, "bind", G_CALLBACK(text_factory_bind),
	                 GINT_TO_POINTER(column));
	return factory;
}

static GtkListItemFactory *
new_toggle_factory(void)
{
	GtkListItemFactory *factory = gtk_signal_list_item_factory_new();
	g_signal_connect(factory, "setup", G_CALLBACK(toggle_factory_setup), NULL);
	g_signal_connect(factory, "bind", G_CALLBACK(toggle_factory_bind), NULL);
	g_signal_connect(factory, "unbind", G_CALLBACK(toggle_factory_unbind), NULL);
	return factory;
}

/* Add a text column to a view.  gtk_column_view_column_new() takes ownership
 * of the factory; append_column() does not take ownership of the column. */
static void
append_text_column(GtkWidget *view, const char *title, int column, gboolean expand)
{
	GtkListItemFactory *factory = new_text_factory(column);
	GtkColumnViewColumn *col = gtk_column_view_column_new(title, factory);
	gtk_column_view_column_set_expand(col, expand);
	gtk_column_view_column_set_resizable(col, TRUE);
	gtk_column_view_append_column(GTK_COLUMN_VIEW(view), col);
	g_object_unref(col);
}

static GtkWidget *
create_view(GListStore *store, gboolean codes)
{
	GtkWidget *view = gtk_column_view_new(NULL);

	/* GtkMultiSelection takes a full ref of the model; the caller keeps its
	 * own reference and releases it in ClearTool(). */
	GtkMultiSelection *selection =
		gtk_multi_selection_new(G_LIST_MODEL(g_object_ref(store)));
	gtk_column_view_set_model(GTK_COLUMN_VIEW(view), GTK_SELECTION_MODEL(selection));
	g_object_unref(selection);
	gtk_widget_add_css_class(view, "desmume-tool-view");

	GtkListItemFactory *toggleFactory = new_toggle_factory();
	GtkColumnViewColumn *onCol = gtk_column_view_column_new("On", toggleFactory);
	gtk_column_view_column_set_resizable(onCol, FALSE);
	gtk_column_view_append_column(GTK_COLUMN_VIEW(view), onCol);
	g_object_unref(onCol);

	if (codes)
	{
		append_text_column(view, "Type", COL_TYPE, FALSE);
		append_text_column(view, "Code", COL_CODE, TRUE);
		append_text_column(view, "Lines", COL_LINES, FALSE);
	}
	else
	{
		append_text_column(view, "Size", COL_SIZE, FALSE);
		append_text_column(view, "Address", COL_ADDRESS, FALSE);
		append_text_column(view, "Value", COL_VALUE, FALSE);
	}
	append_text_column(view, "Description", COL_DESC, TRUE);

	return view;
}

/* --------------------------------------------------------------- async dialogs */

struct ConfirmState
{
	gchar **buttons;      /* NULL-terminated labels */
	GCallback callback;   /* void (*)(gint response, gpointer user_data) */
	gpointer user_data;
};

static void
on_alert_choose(GtkAlertDialog *dialog, GAsyncResult *result, gpointer data)
{
	ConfirmState *state = (ConfirmState *)data;
	GError *error = NULL;
	const gint response = gtk_alert_dialog_choose_finish(dialog, result, &error);
	if (error != NULL)
	{
		g_error_free(error);
	}
	typedef void (*ConfirmCb)(gint, gpointer);
	if (state->callback != NULL)
	{
		((ConfirmCb)state->callback)(response, state->user_data);
	}
	g_strfreev(state->buttons);
	g_free(state);
	g_object_unref(dialog);
}

/* Show a modal GtkAlertDialog with custom buttons.  `callback` receives the
 * index of the button that was pressed (or -1 if dismissed). */
static void
show_confirm(const char *title, const char *message, const char *detail,
             gchar **buttons, int default_button, int cancel_button,
             GCallback callback, gpointer user_data)
{
	GtkAlertDialog *dialog = gtk_alert_dialog_new("%s", title);
	gtk_alert_dialog_set_message(dialog, message);
	if ((detail != NULL) && (detail[0] != '\0'))
	{
		gtk_alert_dialog_set_detail(dialog, detail);
	}
	gtk_alert_dialog_set_buttons(dialog, (const char *const *)buttons);
	if (default_button >= 0)
	{
		gtk_alert_dialog_set_default_button(dialog, default_button);
	}
	if (cancel_button >= 0)
	{
		gtk_alert_dialog_set_cancel_button(dialog, cancel_button);
	}
	gtk_alert_dialog_set_modal(dialog, TRUE);

	ConfirmState *state = g_new0(ConfirmState, 1);
	state->buttons = buttons;
	state->callback = callback;
	state->user_data = user_data;
	gtk_alert_dialog_choose(dialog, GTK_WINDOW(sWindow), NULL,
	                        (GAsyncReadyCallback)on_alert_choose, state);
}

static void
show_error(const char *message)
{
	gchar *buttons[] = { (gchar *)"_OK", NULL };
	show_confirm(TOOL_NAME, message, NULL, g_strdupv(buttons), 0, 0, NULL, NULL);
}

/* ------------------------------------------------------------------- model I/O */

static bool
row_matches_filter(CheatRow *row)
{
	if (sFilterText.empty())
	{
		return true;
	}
	const char *fields[] = {
		row->typeText, row->sizeText, row->addressText, row->valueText,
		row->codeText, row->linesText, row->description
	};
	for (const char *field : fields)
	{
		if ((field != NULL) && (g_strstr_len(field, -1, sFilterText.c_str()) != NULL))
		{
			return true;
		}
	}
	return false;
}

static CheatRow *
make_row(const CHEATS_LIST &cheat, size_t index)
{
	CheatRow *row = cheat_row_new();
	row->index = (int)index;
	row->type = cheat.type;
	row->enabled = (cheat.enabled != 0) ? TRUE : FALSE;
	row->typeText = g_strdup(type_name(cheat.type));

	if (cheat.type == CHEAT_TYPE_INTERNAL)
	{
		char address[16];
		char value[16];
		g_snprintf(address, sizeof(address), "0x%08X", cheat.code[0][0]);
		g_snprintf(value, sizeof(value), "%u", cheat.code[0][1]);
		row->sizeText = g_strdup_printf("%d", (int)cheat.size + 1);
		row->addressText = g_strdup(address);
		row->valueText = g_strdup(value);
	}
	else
	{
		char code[24] = "";
		if (cheat.num > 0)
		{
			g_snprintf(code, sizeof(code), "%08X %08X",
			           cheat.code[0][0], cheat.code[0][1]);
		}
		row->codeText = g_strdup(code);
		row->linesText = g_strdup_printf("%u", cheat.num);
	}
	row->description = g_strdup(cheat.description);
	return row;
}

static void
rebuild_stores(void)
{
	g_list_store_remove_all(sInternalStore);
	g_list_store_remove_all(sCodeStore);
	if ((cheats == NULL) || !desmume_emu_has_rom())
	{
		return;
	}

	const size_t count = cheats->getListSize();
	for (size_t i = 0; i < count; ++i)
	{
		CHEATS_LIST cheat;
		if (!cheats->copyItemFromIndex(i, cheat))
		{
			continue;
		}
		CheatRow *row = make_row(cheat, i);
		if (!row_matches_filter(row))
		{
			g_object_unref(row);
			continue;
		}
		GListStore *store = (cheat.type == CHEAT_TYPE_INTERNAL)
		                    ? sInternalStore : sCodeStore;
		g_list_store_append(store, G_OBJECT(row));
		g_object_unref(row); /* the store owns its reference */
	}
}

/* Index of the selected cheat (across whichever page is active), or -1. */
static int
selected_index(void)
{
	const int page = (sNotebook != NULL)
	                 ? gtk_notebook_get_current_page(GTK_NOTEBOOK(sNotebook)) : 0;
	GtkWidget *view = (page == 1) ? sCodeView : sInternalView;
	GListStore *store = (page == 1) ? sCodeStore : sInternalStore;
	if ((view == NULL) || (store == NULL))
	{
		return -1;
	}

	GArray *positions = NULL;
	const int first = desmume_column_view_selected(view, &positions);
	if (positions == NULL)
	{
		return -1;
	}
	int index = -1;
	if (first >= 0)
	{
		GObject *item = G_OBJECT(g_list_model_get_item(G_LIST_MODEL(store), (guint)first));
		if (item != NULL)
		{
			index = ((CheatRow *)item)->index;
			g_object_unref(item);
		}
	}
	g_array_unref(positions);
	return index;
}

/* Select the row showing `index` on its page, if it is currently visible. */
static void
select_index(int index)
{
	if (index < 0)
	{
		return;
	}
	CHEATS_LIST cheat;
	if ((cheats == NULL) || !cheats->copyItemFromIndex((size_t)index, cheat))
	{
		return;
	}

	const int page = (cheat.type == CHEAT_TYPE_INTERNAL) ? 0 : 1;
	gtk_notebook_set_current_page(GTK_NOTEBOOK(sNotebook), page);
	GListStore *store = (page == 0) ? sInternalStore : sCodeStore;
	GtkWidget *view = (page == 0) ? sInternalView : sCodeView;

	const guint count = g_list_model_get_n_items(G_LIST_MODEL(store));
	for (guint pos = 0; pos < count; ++pos)
	{
		GObject *item = G_OBJECT(g_list_model_get_item(G_LIST_MODEL(store), pos));
		if (item != NULL)
		{
			if (((CheatRow *)item)->index == index)
			{
				GtkSelectionModel *model = gtk_column_view_get_model(GTK_COLUMN_VIEW(view));
				gtk_selection_model_select_item(model, pos, TRUE);
				g_object_unref(item);
				break;
			}
			g_object_unref(item);
		}
	}
}

static void
update_status(void)
{
	const size_t total = (cheats != NULL) ? cheats->getListSize() : 0;
	const size_t enabled = (cheats != NULL) ? cheats->getActiveCount() : 0;

	char text[160];
	g_snprintf(text, sizeof(text), "%zu cheat%s, %zu enabled%s",
	           total, (total == 1) ? "" : "s", enabled,
	           sDirty ? " \xe2\x80\x94 unsaved changes" : "");
	gtk_label_set_text(GTK_LABEL(sStatusLabel), text);

	std::string title("Cheat List");
	if (cheats != NULL)
	{
		const char *filePath = cheats->getFilePath();
		if ((filePath != NULL) && (filePath[0] != '\0'))
		{
			gchar *baseName = g_path_get_basename(filePath);
			title += " \xe2\x80\x94 ";
			title += baseName;
			g_free(baseName);
		}
	}
	if (sDirty)
	{
		title += " *";
	}
	gtk_window_set_title(GTK_WINDOW(sWindow), title.c_str());
}

static void
update_buttons(void)
{
	const int index = selected_index();
	const int page = (sNotebook != NULL)
	                 ? gtk_notebook_get_current_page(GTK_NOTEBOOK(sNotebook)) : 0;
	GtkWidget *view = (page == 1) ? sCodeView : sInternalView;
	gboolean anySelected = FALSE;
	if (view != NULL)
	{
		GtkSelectionModel *model = gtk_column_view_get_model(GTK_COLUMN_VIEW(view));
		GtkBitset *bits = gtk_selection_model_get_selection(model);
		anySelected = (gtk_bitset_get_size(bits) > 0);
		gtk_bitset_unref(bits);
	}

	gtk_widget_set_sensitive(sEditButton, (index >= 0) ? TRUE : FALSE);
	gtk_widget_set_sensitive(sRemoveButton, anySelected);

	bool canUp = false;
	bool canDown = false;
	if ((index >= 0) && (cheats != NULL))
	{
		CHEATS_LIST cheat;
		if (cheats->copyItemFromIndex((size_t)index, cheat))
		{
			/* Move past cheats of a different type, matching GTK3, so the two
			 * pages keep showing rows in the order the file has them. */
			for (int i = index - 1; i >= 0; --i)
			{
				CHEATS_LIST other;
				if (cheats->copyItemFromIndex((size_t)i, other) && (other.type == cheat.type))
				{
					canUp = true;
					break;
				}
			}
			for (size_t i = (size_t)index + 1; i < cheats->getListSize(); ++i)
			{
				CHEATS_LIST other;
				if (cheats->copyItemFromIndex(i, other) && (other.type == cheat.type))
				{
					canDown = true;
					break;
				}
			}
		}
	}
	gtk_widget_set_sensitive(sMoveUpButton, canUp ? TRUE : FALSE);
	gtk_widget_set_sensitive(sMoveDownButton, canDown ? TRUE : FALSE);
}

static void
mark_dirty(void)
{
	sDirty = true;
	update_status();
}

static void
after_change(int index)
{
	rebuild_stores();
	update_buttons();
	update_status();
	if (index >= 0)
	{
		select_index(index);
	}
}

/* ------------------------------------------------------------------ row toggle */

static void
on_toggle_toggled(GtkCheckButton *check, gpointer data)
{
	(void)data;
	CheatRow *row = CHEAT_ROW(g_object_get_data(G_OBJECT(check), "cheat-row"));
	if ((row == NULL) || (row->index < 0) || (cheats == NULL))
	{
		return;
	}

	/* The Windows list applies a checkbox click to every selected row, which
	 * is how you turn a block of cheats on or off at once. */
	const bool target = gtk_check_button_get_active(check) != FALSE;
	GtkWidget *view = (row->type == CHEAT_TYPE_INTERNAL) ? sInternalView : sCodeView;
	GArray *positions = NULL;
	desmume_column_view_selected(view, &positions);
	if ((positions != NULL) && (positions->len > 1))
	{
		GListStore *store = (row->type == CHEAT_TYPE_INTERNAL)
		                    ? sInternalStore : sCodeStore;
		for (guint i = 0; i < positions->len; ++i)
		{
			GObject *item = G_OBJECT(g_list_model_get_item(G_LIST_MODEL(store),
			                                     g_array_index(positions, guint, i)));
			if (item != NULL)
			{
				const int index = ((CheatRow *)item)->index;
				if (index >= 0)
				{
					cheats->toggle(target, (size_t)index);
				}
				g_object_unref(item);
			}
		}
		g_array_unref(positions);
		mark_dirty();
		after_change(row->index);
	}
	else
	{
		if (positions != NULL)
		{
			g_array_unref(positions);
		}
		cheats->toggle(target, (size_t)row->index);
		mark_dirty();
		update_status();
	}
}

/* -------------------------------------------------------- internal cheat dialog */

struct InternalDialogState
{
	GtkWidget *window;
	GtkWidget *addressEntry;
	GtkWidget *valueEntry;
	GtkWidget *descriptionEntry;
	GtkWidget *sizeDropDown;
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

static void
internal_dialog_validate(InternalDialogState *state)
{
	u32 address = 0;
	u32 value = 0;
	const guint sizeIndex = gtk_drop_down_get_selected(GTK_DROP_DOWN(state->sizeDropDown));
	const u8 size = (sizeIndex < 4) ? (u8)sizeIndex : (u8)0;

	const bool addressOk = parse_hex_address(
		gtk_editable_get_text(GTK_EDITABLE(state->addressEntry)), &address);
	const bool valueOk = parse_cheat_value(
		gtk_editable_get_text(GTK_EDITABLE(state->valueEntry)), size, &value);

	state->valid = addressOk && valueOk;
	state->size = size;
	state->address = address;
	state->value = value;
	state->enabled = gtk_check_button_get_active(GTK_CHECK_BUTTON(state->enabledCheck)) != FALSE;

	if (!addressOk)
	{
		gtk_label_set_text(GTK_LABEL(state->hintLabel),
		                   "Enter an address of 1 to 8 hex digits, up to 0x0FFFFFFF.");
	}
	else if (!valueOk)
	{
		char hint[96];
		g_snprintf(hint, sizeof(hint), "Enter a value between 0 and %u for %d byte%s.",
		           (unsigned)size_max_value(size), size + 1, (size == 0) ? "" : "s");
		gtk_label_set_text(GTK_LABEL(state->hintLabel), hint);
	}
	else
	{
		gtk_label_set_text(GTK_LABEL(state->hintLabel), "");
	}

	char code[32];
	format_ar_code(address, value, size, code, sizeof(code));
	char markup[64];
	g_snprintf(markup, sizeof(markup), "<tt>%s</tt>", code);
	gtk_label_set_markup(GTK_LABEL(state->codeLabel), markup);
	gtk_widget_set_sensitive(state->okButton, state->valid ? TRUE : FALSE);
}

static void
on_internal_changed(GtkEditable *editable, gpointer data)
{
	(void)editable;
	internal_dialog_validate((InternalDialogState *)data);
}

static void
on_internal_size_changed(GObject *dropDown, GParamSpec *pspec, gpointer data)
{
	(void)pspec;
	InternalDialogState *state = (InternalDialogState *)data;
	const guint selected = gtk_drop_down_get_selected(GTK_DROP_DOWN(dropDown));
	const u8 size = (selected < 4) ? (u8)selected : (u8)0;
	const u32 maximum = size_max_value(size);
	const char *text = gtk_editable_get_text(GTK_EDITABLE(state->valueEntry));
	char *end = NULL;
	const gint64 parsed = g_ascii_strtoll(text, &end, 10);
	if ((end != text) && (parsed > (gint64)maximum))
	{
		char clamped[16];
		g_snprintf(clamped, sizeof(clamped), "%u", (unsigned)maximum);
		gtk_editable_set_text(GTK_EDITABLE(state->valueEntry), clamped);
	}
	internal_dialog_validate(state);
}

static void
free_internal_state(gpointer data)
{
	delete (InternalDialogState *)data;
}

static void
on_internal_ok(GtkButton *button, gpointer data)
{
	(void)button;
	InternalDialogState *state = (InternalDialogState *)data;
	if (!state->valid)
	{
		return;
	}

	char *description = g_strdup(gtk_editable_get_text(GTK_EDITABLE(state->descriptionEntry)));
	if (state->editing)
	{
		cheats->update(state->size, state->address, state->value,
		               description, state->enabled, state->position);
	}
	else
	{
		cheats->add(state->size, state->address, state->value,
		            description, state->enabled);
	}
	g_free(description);

	mark_dirty();
	after_change(state->editing ? (int)state->position
	                            : (int)cheats->getListSize() - 1);
	gtk_window_destroy(GTK_WINDOW(state->window));
}

static void
internal_cheat_dialog(bool editing, size_t position)
{
	InternalDialogState *state = new InternalDialogState();
	memset(state, 0, sizeof(*state));
	state->editing = editing;
	state->position = position;

	CHEATS_LIST cheat;
	if (editing && cheats->copyItemFromIndex(position, cheat))
	{
		state->size = cheat.size;
		state->address = cheat.code[0][0];
		state->value = cheat.code[0][1];
		state->enabled = (cheat.enabled != 0);
	}
	else
	{
		state->size = 0;
		state->address = 0;
		state->value = 0;
		state->enabled = true;
	}

	GtkWidget *window = desmume_tool_window_new(
		editing ? "Edit Cheat" : "Add Cheat", sWindow, 440, -1);
	gtk_window_set_modal(GTK_WINDOW(window), TRUE);
	state->window = window;

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

	auto add_label = [grid](const char *text, int row) {
		GtkWidget *label = gtk_label_new(text);
		gtk_widget_set_halign(label, GTK_ALIGN_END);
		gtk_grid_attach(GTK_GRID(grid), label, 0, row, 1, 1);
	};

	char buffer[32];
	add_label("Address:", 0);
	state->addressEntry = gtk_entry_new();
	gtk_entry_set_max_length(GTK_ENTRY(state->addressEntry), 10);
	gtk_editable_set_width_chars(GTK_EDITABLE(state->addressEntry), 10);
	gtk_widget_set_hexpand(state->addressEntry, TRUE);
	g_snprintf(buffer, sizeof(buffer), "%07X", (unsigned)state->address);
	gtk_editable_set_text(GTK_EDITABLE(state->addressEntry), buffer);
	gtk_grid_attach(GTK_GRID(grid), state->addressEntry, 1, 0, 1, 1);

	add_label("Value:", 1);
	state->valueEntry = gtk_entry_new();
	gtk_entry_set_max_length(GTK_ENTRY(state->valueEntry), 11);
	gtk_editable_set_width_chars(GTK_EDITABLE(state->valueEntry), 12);
	gtk_widget_set_hexpand(state->valueEntry, TRUE);
	g_snprintf(buffer, sizeof(buffer), "%u", (unsigned)state->value);
	gtk_editable_set_text(GTK_EDITABLE(state->valueEntry), buffer);
	gtk_grid_attach(GTK_GRID(grid), state->valueEntry, 1, 1, 1, 1);

	add_label("Width:", 2);
	GtkStringList *sizes = gtk_string_list_new(NULL);
	for (int i = 0; i < 4; ++i)
	{
		g_snprintf(buffer, sizeof(buffer), "%d byte%s", i + 1, (i == 0) ? "" : "s");
		gtk_string_list_append(sizes, buffer);
	}
	state->sizeDropDown = gtk_drop_down_new(G_LIST_MODEL(sizes), NULL);
	gtk_drop_down_set_selected(GTK_DROP_DOWN(state->sizeDropDown), state->size);
	gtk_grid_attach(GTK_GRID(grid), state->sizeDropDown, 1, 2, 1, 1);

	add_label("Description:", 3);
	state->descriptionEntry = gtk_entry_new();
	gtk_entry_set_max_length(GTK_ENTRY(state->descriptionEntry), 1023);
	gtk_widget_set_hexpand(state->descriptionEntry, TRUE);
	if (editing)
	{
		gtk_editable_set_text(GTK_EDITABLE(state->descriptionEntry), cheat.description);
	}
	gtk_grid_attach(GTK_GRID(grid), state->descriptionEntry, 1, 3, 1, 1);

	state->enabledCheck = gtk_check_button_new_with_label("Enabled (freeze this value)");
	gtk_check_button_set_active(GTK_CHECK_BUTTON(state->enabledCheck),
	                            state->enabled ? TRUE : FALSE);
	gtk_grid_attach(GTK_GRID(grid), state->enabledCheck, 1, 4, 1, 1);

	add_label("AR code:", 5);
	state->codeLabel = gtk_label_new(NULL);
	gtk_widget_set_halign(state->codeLabel, GTK_ALIGN_START);
	gtk_widget_set_tooltip_text(state->codeLabel,
	                            "The equivalent Action Replay code, shown for reference only.");
	gtk_grid_attach(GTK_GRID(grid), state->codeLabel, 1, 5, 1, 1);

	state->hintLabel = gtk_label_new("");
	gtk_label_set_xalign(GTK_LABEL(state->hintLabel), 0.0f);
	gtk_label_set_wrap(GTK_LABEL(state->hintLabel), TRUE);
	gtk_box_append(GTK_BOX(root), state->hintLabel);

	GtkWidget *buttons = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
	gtk_widget_set_halign(buttons, GTK_ALIGN_END);
	gtk_box_append(GTK_BOX(root), buttons);

	GtkWidget *cancel = gtk_button_new_with_mnemonic("_Cancel");
	g_signal_connect_swapped(cancel, "clicked", G_CALLBACK(gtk_window_destroy), window);
	gtk_box_append(GTK_BOX(buttons), cancel);

	state->okButton = gtk_button_new_with_mnemonic(editing ? "_Update" : "_Add");
	gtk_widget_add_css_class(state->okButton, "suggested-action");
	g_signal_connect(state->okButton, "clicked", G_CALLBACK(on_internal_ok), state);
	gtk_box_append(GTK_BOX(buttons), state->okButton);

	g_signal_connect(state->addressEntry, "changed", G_CALLBACK(on_internal_changed), state);
	g_signal_connect(state->valueEntry, "changed", G_CALLBACK(on_internal_changed), state);
	g_signal_connect(state->enabledCheck, "toggled", G_CALLBACK(on_internal_changed), state);
	g_signal_connect(state->sizeDropDown, "notify::selected",
	                 G_CALLBACK(on_internal_size_changed), state);

	g_object_set_data_full(G_OBJECT(window), "cheat-internal-state", state, free_internal_state);
	internal_dialog_validate(state);
	gtk_window_present(GTK_WINDOW(window));
	gtk_widget_grab_focus(editing ? state->valueEntry : state->addressEntry);
}

/* ------------------------------------------------------------ code cheat dialog */

struct CodeDialogState
{
	GtkWidget *window;
	GtkWidget *textView;
	GtkWidget *descriptionEntry;
	GtkWidget *enabledCheck;
	GtkWidget *hintLabel;
	GtkWidget *okButton;

	bool editing;
	size_t position;
	u8 type;
	bool valid;
};

static char *
text_view_contents(GtkWidget *view)
{
	GtkTextBuffer *buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(view));
	GtkTextIter start;
	GtkTextIter end;
	gtk_text_buffer_get_bounds(buffer, &start, &end);
	return gtk_text_buffer_get_text(buffer, &start, &end, FALSE);
}

static void
code_dialog_validate(CodeDialogState *state)
{
	char *text = text_view_contents(state->textView);
	CHEATS_LIST parsed;
	const bool valid = (text != NULL) &&
	                   CHEATS::XXCodeFromString(text, parsed) && (parsed.num > 0);

	if (valid)
	{
		char hint[64];
		g_snprintf(hint, sizeof(hint), "%u code line%s.", (unsigned)parsed.num,
		           (parsed.num == 1) ? "" : "s");
		gtk_label_set_text(GTK_LABEL(state->hintLabel), hint);
	}
	else
	{
		gtk_label_set_text(GTK_LABEL(state->hintLabel),
		                   "Enter the code as lines of two 8-digit hex words.");
	}
	gtk_widget_set_sensitive(state->okButton, valid ? TRUE : FALSE);
	state->valid = valid;
	g_free(text);
}

static void
on_code_changed(GtkTextBuffer *buffer, gpointer data)
{
	(void)buffer;
	code_dialog_validate((CodeDialogState *)data);
}

static void
free_code_state(gpointer data)
{
	delete (CodeDialogState *)data;
}

static void
on_code_ok(GtkButton *button, gpointer data)
{
	(void)button;
	CodeDialogState *state = (CodeDialogState *)data;
	if (!state->valid)
	{
		return;
	}

	char *code = text_view_contents(state->textView);
	char *description = g_strdup(gtk_editable_get_text(GTK_EDITABLE(state->descriptionEntry)));
	const bool enabled = gtk_check_button_get_active(GTK_CHECK_BUTTON(state->enabledCheck)) != FALSE;

	bool applied = false;
	if (state->type == CHEAT_TYPE_CODEBREAKER)
	{
		applied = state->editing
		          ? cheats->update_CB(code, description, enabled, state->position)
		          : cheats->add_CB(code, description, enabled);
	}
	else
	{
		applied = state->editing
		          ? cheats->update_AR(code, description, enabled, state->position)
		          : cheats->add_AR(code, description, enabled);
	}

	g_free(description);
	g_free(code);

	if (!applied)
	{
		show_error("The code could not be stored.");
		return;
	}
	mark_dirty();
	after_change(state->editing ? (int)state->position
	                            : (int)cheats->getListSize() - 1);
	gtk_window_destroy(GTK_WINDOW(state->window));
}

static void
code_cheat_dialog(bool editing, size_t position, u8 type)
{
	CodeDialogState *state = new CodeDialogState();
	memset(state, 0, sizeof(*state));
	state->editing = editing;
	state->position = position;
	state->type = type;

	CHEATS_LIST cheat;
	const bool haveCheat = editing && cheats->copyItemFromIndex(position, cheat);

	char title[64];
	g_snprintf(title, sizeof(title), "%s %s", editing ? "Edit" : "Add", type_name(type));
	GtkWidget *window = desmume_tool_window_new(title, sWindow, 480, 400);
	gtk_window_set_modal(GTK_WINDOW(window), TRUE);
	state->window = window;

	GtkWidget *root = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
	gtk_widget_set_margin_start(root, 12);
	gtk_widget_set_margin_end(root, 12);
	gtk_widget_set_margin_top(root, 12);
	gtk_widget_set_margin_bottom(root, 12);
	gtk_window_set_child(GTK_WINDOW(window), root);

	GtkWidget *label = gtk_label_new("Code:");
	gtk_widget_set_halign(label, GTK_ALIGN_START);
	gtk_box_append(GTK_BOX(root), label);

	GtkWidget *scrolled = gtk_scrolled_window_new();
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled),
	                               GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
	gtk_widget_add_css_class(scrolled, "desmume-frame");
	gtk_widget_set_size_request(scrolled, -1, 180);
	gtk_box_append(GTK_BOX(root), scrolled);

	state->textView = gtk_text_view_new();
	gtk_text_view_set_monospace(GTK_TEXT_VIEW(state->textView), TRUE);
	gtk_text_view_set_left_margin(GTK_TEXT_VIEW(state->textView), 4);
	gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scrolled), state->textView);

	if (haveCheat)
	{
		const size_t capacity = (size_t)MAX_XX_CODE * 18 + 1;
		char *codeText = (char *)calloc(capacity, 1);
		if (codeText != NULL)
		{
			CHEATS::StringFromXXCode(cheat, codeText);
			gtk_text_buffer_set_text(gtk_text_view_get_buffer(GTK_TEXT_VIEW(state->textView)),
			                         codeText, -1);
			free(codeText);
		}
	}

	GtkWidget *descBox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
	gtk_box_append(GTK_BOX(root), descBox);
	label = gtk_label_new("Description:");
	gtk_box_append(GTK_BOX(descBox), label);
	state->descriptionEntry = gtk_entry_new();
	gtk_entry_set_max_length(GTK_ENTRY(state->descriptionEntry), 1023);
	gtk_widget_set_hexpand(state->descriptionEntry, TRUE);
	if (haveCheat)
	{
		gtk_editable_set_text(GTK_EDITABLE(state->descriptionEntry), cheat.description);
	}
	gtk_box_append(GTK_BOX(descBox), state->descriptionEntry);

	state->enabledCheck = gtk_check_button_new_with_label("Enabled");
	gtk_check_button_set_active(GTK_CHECK_BUTTON(state->enabledCheck),
	                            (!haveCheat || (cheat.enabled != 0)) ? TRUE : FALSE);
	gtk_box_append(GTK_BOX(root), state->enabledCheck);

	state->hintLabel = gtk_label_new("");
	gtk_widget_set_halign(state->hintLabel, GTK_ALIGN_START);
	gtk_box_append(GTK_BOX(root), state->hintLabel);

	GtkWidget *buttons = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
	gtk_widget_set_halign(buttons, GTK_ALIGN_END);
	gtk_box_append(GTK_BOX(root), buttons);
	GtkWidget *cancel = gtk_button_new_with_mnemonic("_Cancel");
	g_signal_connect_swapped(cancel, "clicked", G_CALLBACK(gtk_window_destroy), window);
	gtk_box_append(GTK_BOX(buttons), cancel);
	state->okButton = gtk_button_new_with_mnemonic(editing ? "_Update" : "_Add");
	gtk_widget_add_css_class(state->okButton, "suggested-action");
	g_signal_connect(state->okButton, "clicked", G_CALLBACK(on_code_ok), state);
	gtk_box_append(GTK_BOX(buttons), state->okButton);

	g_signal_connect(gtk_text_view_get_buffer(GTK_TEXT_VIEW(state->textView)),
	                 "changed", G_CALLBACK(on_code_changed), state);

	g_object_set_data_full(G_OBJECT(window), "cheat-code-state", state, free_code_state);
	code_dialog_validate(state);
	gtk_window_present(GTK_WINDOW(window));
	gtk_widget_grab_focus(state->textView);
}

/* ------------------------------------------------------------------ list ops */

static void
on_edit(GtkButton *button, gpointer data)
{
	(void)button;
	(void)data;
	const int index = selected_index();
	if (index < 0)
	{
		return;
	}
	CHEATS_LIST cheat;
	if (!cheats->copyItemFromIndex((size_t)index, cheat))
	{
		return;
	}
	if (cheat.type == CHEAT_TYPE_INTERNAL)
	{
		internal_cheat_dialog(true, (size_t)index);
	}
	else
	{
		code_cheat_dialog(true, (size_t)index, cheat.type);
	}
}

struct RemoveState
{
	GArray *indices;  /* cheat indices, sorted descending */
};

static void
on_remove_confirmed(gint response, gpointer data)
{
	RemoveState *state = (RemoveState *)data;
	if (response == 0)  /* Cancel */
	{
		g_array_free(state->indices, TRUE);
		g_free(state);
		return;
	}

	int selectAfter = G_MAXINT;
	for (guint i = 0; i < state->indices->len; ++i)
	{
		const int index = g_array_index(state->indices, int, i);
		if (cheats->remove((size_t)index))
		{
			if (index < selectAfter)
			{
				selectAfter = index;
			}
		}
	}
	g_array_free(state->indices, TRUE);
	g_free(state);

	if (selectAfter == G_MAXINT)
	{
		selectAfter = -1;
	}
	else if ((size_t)selectAfter >= cheats->getListSize())
	{
		selectAfter = (int)cheats->getListSize() - 1;
	}
	mark_dirty();
	after_change(selectAfter);
}

static void
on_remove(GtkButton *button, gpointer data)
{
	(void)button;
	(void)data;
	const int page = gtk_notebook_get_current_page(GTK_NOTEBOOK(sNotebook));
	GtkWidget *view = (page == 1) ? sCodeView : sInternalView;
	GListStore *store = (page == 1) ? sCodeStore : sInternalStore;
	GArray *positions = NULL;
	desmume_column_view_selected(view, &positions);
	if ((positions == NULL) || (positions->len == 0))
	{
		if (positions != NULL)
		{
			g_array_unref(positions);
		}
		return;
	}

	GArray *indices = g_array_new(FALSE, FALSE, sizeof(int));
	for (guint i = 0; i < positions->len; ++i)
	{
		GObject *item = G_OBJECT(g_list_model_get_item(G_LIST_MODEL(store),
		                                     g_array_index(positions, guint, i)));
		if (item != NULL)
		{
			const int index = ((CheatRow *)item)->index;
			if (index >= 0)
			{
				g_array_append_val(indices, index);
			}
			g_object_unref(item);
		}
	}
	g_array_unref(positions);

	if (indices->len == 0)
	{
		g_array_free(indices, TRUE);
		return;
	}

	/* Highest index first, so the ones still to remove keep their positions. */
	std::sort(&g_array_index(indices, int, 0),
	          &g_array_index(indices, int, 0) + indices->len, std::greater<int>());

	char question[128];
	g_snprintf(question, sizeof(question), "Remove %u selected cheat%s?",
	           indices->len, (indices->len == 1) ? "" : "s");

	RemoveState *state = g_new0(RemoveState, 1);
	state->indices = indices;
	gchar *buttons[] = { (gchar *)"_Cancel", (gchar *)"_Remove", NULL };
	show_confirm(TOOL_NAME, question, NULL, g_strdupv(buttons), 0, 0,
	             G_CALLBACK(on_remove_confirmed), state);
}

static void
move_selected(int direction)
{
	const int index = selected_index();
	if ((index < 0) || (cheats == NULL))
	{
		return;
	}
	CHEATS_LIST cheat;
	if (!cheats->copyItemFromIndex((size_t)index, cheat))
	{
		return;
	}

	int target = -1;
	for (int i = index + direction; (i >= 0) && (i < (int)cheats->getListSize());
	     i += direction)
	{
		CHEATS_LIST other;
		if (!cheats->copyItemFromIndex((size_t)i, other))
		{
			break;
		}
		if (other.type == cheat.type)
		{
			target = i;
			break;
		}
	}
	if ((target < 0) || !cheats->move((size_t)index, (size_t)target))
	{
		return;
	}
	mark_dirty();
	after_change(target);
}

static void
on_move_up(GtkButton *button, gpointer data)
{
	(void)button;
	(void)data;
	move_selected(-1);
}

static void
on_move_down(GtkButton *button, gpointer data)
{
	(void)button;
	(void)data;
	move_selected(1);
}

/* ------------------------------------------------------------ cheat database */

static std::string
cheat_database_path(void)
{
	desmume::config::Config *config = desmume_host_config();
	std::string path = (config != NULL) ? config->cheats_database_path.get() : std::string();
	if (!path.empty())
	{
		return path;
	}
	gchar *baseName = g_build_filename(g_get_user_config_dir(), "desmume", "cheat.dat", NULL);
	path.assign(baseName);
	g_free(baseName);
	return path;
}

struct DbImportState
{
	GtkWidget *window;
	GListStore *rows;         /* DesmumeRow: [0] = "", [1] = description */
	std::vector<CHEATS_LIST> items;  /* the cheats offered by the database */
};

static void
free_db_state(gpointer data)
{
	DbImportState *state = (DbImportState *)data;
	if (state->rows != NULL)
	{
		g_object_unref(state->rows);
	}
	delete state;
}

static void on_db_toggle_toggled(GtkCheckButton *check, gpointer data);

static void
db_toggle_setup(GtkListItemFactory *factory, GtkListItem *item, gpointer data)
{
	(void)factory;
	(void)data;
	GtkWidget *check = gtk_check_button_new();
	g_object_set_data(G_OBJECT(item), "db-toggle", check);
	gtk_list_item_set_child(item, check);
}

static void
db_toggle_bind(GtkListItemFactory *factory, GtkListItem *item, gpointer data)
{
	(void)factory;
	(void)data;
	GtkWidget *check = GTK_WIDGET(g_object_get_data(G_OBJECT(item), "db-toggle"));
	CheatRow *row = CHEAT_ROW(gtk_list_item_get_item(item));
	if ((check == NULL) || (row == NULL))
	{
		return;
	}
	g_object_set_data(G_OBJECT(check), "db-row", row);
	g_signal_handlers_block_by_func(check, (gpointer)on_db_toggle_toggled, NULL);
	gtk_check_button_set_active(GTK_CHECK_BUTTON(check), row->enabled);
	g_signal_handlers_unblock_by_func(check, (gpointer)on_db_toggle_toggled, NULL);
	g_signal_connect(check, "toggled", G_CALLBACK(on_db_toggle_toggled), NULL);
}

static void
db_toggle_unbind(GtkListItemFactory *factory, GtkListItem *item, gpointer data)
{
	(void)factory;
	(void)data;
	GtkWidget *check = GTK_WIDGET(g_object_get_data(G_OBJECT(item), "db-toggle"));
	if (check != NULL)
	{
		g_signal_handlers_disconnect_by_func(check,
		                                     (gpointer)on_db_toggle_toggled, NULL);
		g_object_set_data(G_OBJECT(check), "db-row", NULL);
	}
}

static void
on_db_toggle_toggled(GtkCheckButton *check, gpointer data)
{
	(void)data;
	CheatRow *row = CHEAT_ROW(g_object_get_data(G_OBJECT(check), "db-row"));
	if (row != NULL)
	{
		row->enabled = gtk_check_button_get_active(check);
	}
}

static void
on_db_add(GtkButton *button, gpointer data)
{
	(void)button;
	DbImportState *state = (DbImportState *)data;
	size_t added = 0;
	const guint count = g_list_model_get_n_items(G_LIST_MODEL(state->rows));
	for (guint i = 0; i < count; ++i)
	{
		CheatRow *row = CHEAT_ROW(g_list_model_get_item(G_LIST_MODEL(state->rows), i));
		/* Database rows use the same CheatRow type with index = item position
		 * and the enabled flag acting as the checkbox state. */
		if ((row != NULL) && row->enabled && ((size_t)row->index < state->items.size()))
		{
			cheats->add_AR_Direct(state->items[(size_t)row->index]);
			added++;
		}
		if (row != NULL)
		{
			g_object_unref(row);
		}
	}
	gtk_window_destroy(GTK_WINDOW(state->window));

	if (added > 0)
	{
		mark_dirty();
		after_change((int)cheats->getListSize() - 1);
	}
}

static void
show_database_import(const std::string &path)
{
	CHEATSEXPORT exporter;
	if (!exporter.load(path.c_str()))
	{
		char message[512];
		switch (exporter.getErrorCode())
		{
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
		show_error(message);
		return;
	}

	const size_t count = exporter.getCheatsNum();
	CHEATS_LIST *items = exporter.getCheats();
	if ((count == 0) || (items == NULL))
	{
		show_error("The database has no cheats for this game.");
		return;
	}

	DbImportState *state = new DbImportState();
	state->items.assign(items, items + count);
	state->rows = g_list_store_new(cheat_row_get_type());

	GtkWidget *window = desmume_tool_window_new(
		"Export cheats from database", sWindow, 580, 460);
	gtk_window_set_modal(GTK_WINDOW(window), TRUE);
	state->window = window;

	GtkWidget *root = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
	gtk_widget_set_margin_start(root, 12);
	gtk_widget_set_margin_end(root, 12);
	gtk_widget_set_margin_top(root, 12);
	gtk_widget_set_margin_bottom(root, 12);
	gtk_window_set_child(GTK_WINDOW(window), root);

	char header[512];
	g_snprintf(header, sizeof(header), "%s: %s",
	           exporter.getDescription(), exporter.getGameTitle());
	GtkWidget *headerLabel = gtk_label_new(header);
	gtk_label_set_xalign(GTK_LABEL(headerLabel), 0.0f);
	gtk_label_set_wrap(GTK_LABEL(headerLabel), TRUE);
	gtk_box_append(GTK_BOX(root), headerLabel);

	GtkWidget *scrolled = gtk_scrolled_window_new();
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled),
	                               GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
	gtk_widget_add_css_class(scrolled, "desmume-frame");
	gtk_widget_set_vexpand(scrolled, TRUE);
	gtk_box_append(GTK_BOX(root), scrolled);

	GtkWidget *view = gtk_column_view_new(NULL);
	GtkMultiSelection *selection = gtk_multi_selection_new(G_LIST_MODEL(g_object_ref(state->rows)));
	gtk_column_view_set_model(GTK_COLUMN_VIEW(view), GTK_SELECTION_MODEL(selection));
	g_object_unref(selection);

	GtkListItemFactory *toggleFactory = gtk_signal_list_item_factory_new();
	g_signal_connect(toggleFactory, "setup", G_CALLBACK(db_toggle_setup), NULL);
	g_signal_connect(toggleFactory, "bind", G_CALLBACK(db_toggle_bind), NULL);
	g_signal_connect(toggleFactory, "unbind", G_CALLBACK(db_toggle_unbind), NULL);
	GtkColumnViewColumn *onCol = gtk_column_view_column_new("", toggleFactory);
	gtk_column_view_append_column(GTK_COLUMN_VIEW(view), onCol);
	g_object_unref(onCol);
	append_text_column(view, "Cheat", COL_DESC, TRUE);
	gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scrolled), view);

	for (size_t i = 0; i < count; ++i)
	{
		CheatRow *row = cheat_row_new();
		row->index = (int)i;
		row->enabled = FALSE;
		row->description = g_strdup(items[i].description);
		g_list_store_append(state->rows, G_OBJECT(row));
		g_object_unref(row);
	}

	GtkWidget *buttons = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
	gtk_widget_set_halign(buttons, GTK_ALIGN_END);
	gtk_box_append(GTK_BOX(root), buttons);
	GtkWidget *cancel = gtk_button_new_with_mnemonic("_Cancel");
	g_signal_connect_swapped(cancel, "clicked", G_CALLBACK(gtk_window_destroy), window);
	gtk_box_append(GTK_BOX(buttons), cancel);
	GtkWidget *add = gtk_button_new_with_mnemonic("_Add");
	gtk_widget_add_css_class(add, "suggested-action");
	g_signal_connect(add, "clicked", G_CALLBACK(on_db_add), state);
	gtk_box_append(GTK_BOX(buttons), add);

	g_object_set_data_full(G_OBJECT(window), "cheat-db-state", state, free_db_state);
	gtk_window_present(GTK_WINDOW(window));
}

struct DbFileState
{
	std::string path;
};

static void
on_db_file_chosen(GtkFileDialog *dialog, GAsyncResult *result, gpointer data)
{
	DbFileState *state = (DbFileState *)data;
	GError *error = NULL;
	GFile *file = gtk_file_dialog_open_finish(dialog, result, &error);
	if ((error == NULL) && (file != NULL))
	{
		gchar *path = g_file_get_path(file);
		if (path != NULL)
		{
			state->path.assign(path);
			g_free(path);
		}
		g_object_unref(file);
	}
	if (error != NULL)
	{
		g_error_free(error);
	}
	if (!state->path.empty())
	{
		desmume::config::Config *config = desmume_host_config();
		if (config != NULL)
		{
			config->cheats_database_path = state->path;
			config->save();
		}
		show_database_import(state->path);
	}
	delete state;
	g_object_unref(dialog);
}

static void
choose_cheat_database(void)
{
	GtkFileDialog *dialog = gtk_file_dialog_new();
	gtk_file_dialog_set_title(dialog, "Select cheat database");
	gtk_file_dialog_set_modal(dialog, TRUE);

	GtkFileFilter *filter = gtk_file_filter_new();
	gtk_file_filter_set_name(filter, "Cheat databases (*.dat)");
	gtk_file_filter_add_pattern(filter, "*.dat");
	GListStore *filters = g_list_store_new(GTK_TYPE_FILE_FILTER);
	g_list_store_append(filters, filter);
	gtk_file_dialog_set_filters(dialog, G_LIST_MODEL(filters));
	/* set_filters copies the list; our local store can be released. */
	g_object_unref(filters);

	std::string current = cheat_database_path();
	if (g_file_test(current.c_str(), G_FILE_TEST_IS_REGULAR))
	{
		GFile *initial = g_file_new_for_path(current.c_str());
		gtk_file_dialog_set_initial_file(dialog, initial);
		g_object_unref(initial);
	}

	DbFileState *state = new DbFileState();
	gtk_file_dialog_open(dialog, GTK_WINDOW(sWindow), NULL,
	                     (GAsyncReadyCallback)on_db_file_chosen, state);
}

static void
on_database(GtkButton *button, gpointer data)
{
	(void)button;
	(void)data;
	if ((cheats == NULL) || !desmume_emu_has_rom() || cheats->getFilePath()[0] == '\0')
	{
		show_error("Load a ROM first: the database is searched by game code.");
		return;
	}
	std::string path = cheat_database_path();
	if (!g_file_test(path.c_str(), G_FILE_TEST_IS_REGULAR))
	{
		choose_cheat_database();
		return;
	}
	show_database_import(path);
}

/* ------------------------------------------------------------------- filtering */

static void
on_search_changed(GtkSearchEntry *entry, gpointer data)
{
	(void)data;
	sFilterText = gtk_editable_get_text(GTK_EDITABLE(entry));
	rebuild_stores();
	update_buttons();
	update_status();
}

/* ---------------------------------------------------------------------- actions */

static void
on_add_internal(GSimpleAction *action, GVariant *parameter, gpointer data)
{
	(void)action;
	(void)parameter;
	(void)data;
	internal_cheat_dialog(false, 0);
}

static void
on_add_ar(GSimpleAction *action, GVariant *parameter, gpointer data)
{
	(void)action;
	(void)parameter;
	(void)data;
	code_cheat_dialog(false, 0, CHEAT_TYPE_AR);
}

static void
on_add_cb(GSimpleAction *action, GVariant *parameter, gpointer data)
{
	(void)action;
	(void)parameter;
	(void)data;
	code_cheat_dialog(false, 0, CHEAT_TYPE_CODEBREAKER);
}

static void
on_page_switched(GtkNotebook *notebook, GtkWidget *page, guint pageNum, gpointer data)
{
	(void)notebook;
	(void)page;
	(void)data;
	(void)pageNum;
	update_buttons();
}

/* --------------------------------------------------------------------- closing */

struct CloseState
{
	bool save;   /* true when the user chose "Save" */
};

static void
on_destroy(GtkWidget *widget, gpointer data)
{
	(void)widget;
	(void)data;

	delete sSnapshot;
	sSnapshot = NULL;
	sWindow = NULL;
	sNotebook = NULL;
	sInternalView = NULL;
	sCodeView = NULL;
	if (sInternalStore != NULL)
	{
		g_object_unref(sInternalStore);
		sInternalStore = NULL;
	}
	if (sCodeStore != NULL)
	{
		g_object_unref(sCodeStore);
		sCodeStore = NULL;
	}
	sSearchEntry = NULL;
	sEditButton = NULL;
	sRemoveButton = NULL;
	sMoveUpButton = NULL;
	sMoveDownButton = NULL;
	sStatusLabel = NULL;
	sFilterText.clear();

	if (sWasRunning)
	{
		desmume_emu_set_running(TRUE);
		sWasRunning = false;
	}
	sDirty = false;
}

static void
finalize_close(void)
{
	gtk_window_destroy(GTK_WINDOW(sWindow));
}

static void
on_close_confirmed(gint response, gpointer data)
{
	(void)data;
	/* Buttons: 0 = Cancel, 1 = Discard, 2 = Save */
	if (response == 0)
	{
		return;
	}
	if (response == 2)
	{
		if (!cheats->save())
		{
			char message[512];
			g_snprintf(message, sizeof(message),
			           "The cheat list could not be saved to:\n%s", cheats->getFilePath());
			show_error(message);
			return;
		}
	}
	else if ((response == 1) && (sSnapshot != NULL))
	{
		*cheats = *sSnapshot;
	}
	finalize_close();
}

static gboolean
on_close_request(GtkWindow *window, gpointer data)
{
	(void)window;
	(void)data;
	if (!sDirty)
	{
		return FALSE;  /* allow close; destroy signal restores run state */
	}

	gchar *buttons[] = {
		(gchar *)"_Cancel", (gchar *)"_Discard", (gchar *)"_Save", NULL
	};
	show_confirm(TOOL_NAME, "Save changes to the cheat list?",
	             "Unsaved changes are discarded when the window closes.",
	             g_strdupv(buttons), 2, 0,
	             G_CALLBACK(on_close_confirmed), NULL);
	return TRUE;  /* block until the async choice is made */
}

/* ----------------------------------------------------------------- tool window */

static void
Open(void)
{
	if (sWindow != NULL)
	{
		gtk_window_present(GTK_WINDOW(sWindow));
		return;
	}
	if ((cheats == NULL) || !desmume_emu_has_rom())
	{
		GtkAlertDialog *dialog = gtk_alert_dialog_new(TOOL_NAME);
		gtk_alert_dialog_set_message(dialog, "Load a ROM first to use cheats.");
		gtk_alert_dialog_choose(dialog, GTK_WINDOW(desmume_tools_get_parent()),
		                        NULL, NULL, NULL);
		g_object_unref(dialog);
		return;
	}

	sWasRunning = desmume_emu_get_running() != FALSE;
	desmume_emu_set_running(FALSE);

	/* Edits go straight into the live list, so keep the state the window
	 * opened with for the "Discard" answer above. */
	sSnapshot = new CHEATS(*cheats);
	sDirty = false;
	sFilterText.clear();

	sWindow = desmume_tool_window_new("Cheat List", NULL, 820, 520);
	g_object_add_weak_pointer(G_OBJECT(sWindow), (gpointer *)&sWindow);
	g_signal_connect(sWindow, "close-request", G_CALLBACK(on_close_request), NULL);
	g_signal_connect(sWindow, "destroy", G_CALLBACK(on_destroy), NULL);

	GtkWidget *root = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
	gtk_widget_set_margin_start(root, 6);
	gtk_widget_set_margin_end(root, 6);
	gtk_widget_set_margin_top(root, 6);
	gtk_widget_set_margin_bottom(root, 6);
	gtk_window_set_child(GTK_WINDOW(sWindow), root);

	/* Search bar */
	sSearchEntry = gtk_search_entry_new();
	gtk_search_entry_set_placeholder_text(GTK_SEARCH_ENTRY(sSearchEntry),
	                                      "Filter cheats by description or code...");
	gtk_box_append(GTK_BOX(root), sSearchEntry);
	g_signal_connect(sSearchEntry, "search-changed", G_CALLBACK(on_search_changed), NULL);

	/* Two pages: internal cheats and full codes. */
	sInternalStore = g_list_store_new(cheat_row_get_type());
	sCodeStore = g_list_store_new(cheat_row_get_type());
	sInternalView = create_view(sInternalStore, FALSE);
	sCodeView = create_view(sCodeStore, TRUE);

	/* Selection-change handlers now that the views exist. */
	GtkSelectionModel *internalSel = gtk_column_view_get_model(GTK_COLUMN_VIEW(sInternalView));
	GtkSelectionModel *codeSel = gtk_column_view_get_model(GTK_COLUMN_VIEW(sCodeView));
	g_signal_connect(internalSel, "selection-changed",
	                 G_CALLBACK(+[](GtkSelectionModel*, gpointer, gpointer) { update_buttons(); }),
	                 NULL);
	g_signal_connect(codeSel, "selection-changed",
	                 G_CALLBACK(+[](GtkSelectionModel*, gpointer, gpointer) { update_buttons(); }),
	                 NULL);

	sNotebook = gtk_notebook_new();
	GtkWidget *internalScroll = gtk_scrolled_window_new();
	gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(internalScroll), sInternalView);
	gtk_widget_add_css_class(internalScroll, "desmume-frame");
	gtk_notebook_append_page(GTK_NOTEBOOK(sNotebook), internalScroll,
	                         gtk_label_new("Internal cheats"));
	GtkWidget *codeScroll = gtk_scrolled_window_new();
	gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(codeScroll), sCodeView);
	gtk_widget_add_css_class(codeScroll, "desmume-frame");
	gtk_notebook_append_page(GTK_NOTEBOOK(sNotebook), codeScroll,
	                         gtk_label_new("Action Replay and Codebreaker"));
	gtk_notebook_set_scrollable(GTK_NOTEBOOK(sNotebook), TRUE);
	g_signal_connect(sNotebook, "switch-page", G_CALLBACK(on_page_switched), NULL);
	gtk_widget_set_vexpand(sNotebook, TRUE);
	gtk_box_append(GTK_BOX(root), sNotebook);

	sStatusLabel = gtk_label_new("");
	gtk_widget_set_halign(sStatusLabel, GTK_ALIGN_START);
	gtk_box_append(GTK_BOX(root), sStatusLabel);

	/* Button row: Add menu + Edit/Remove/Move/Database + Close. */
	GtkWidget *buttons = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4);
	gtk_widget_set_halign(buttons, GTK_ALIGN_END);
	gtk_box_append(GTK_BOX(root), buttons);

	GtkWidget *addButton = gtk_menu_button_new();
	/* GtkMenuButton is not a GtkButton subclass in GTK4: use its own setter,
	 * never gtk_button_set_label()/gtk_button_set_child(). */
	gtk_menu_button_set_label(GTK_MENU_BUTTON(addButton), "Add");
	gtk_widget_set_tooltip_text(addButton, "Add an internal cheat or a full code");
	GMenu *menu = g_menu_new();
	g_menu_append(menu, "Internal cheat...", "win.add-internal");
	g_menu_append(menu, "Action Replay code...", "win.add-ar");
	g_menu_append(menu, "Codebreaker code...", "win.add-cb");
	gtk_menu_button_set_menu_model(GTK_MENU_BUTTON(addButton), G_MENU_MODEL(menu));
	g_object_unref(menu);
	gtk_box_append(GTK_BOX(buttons), addButton);

	GActionGroup *actions = G_ACTION_GROUP(g_simple_action_group_new());
	g_action_map_add_action(G_ACTION_MAP(actions),
	                        G_ACTION(g_simple_action_new("add-internal", NULL)));
	g_action_map_add_action(G_ACTION_MAP(actions),
	                        G_ACTION(g_simple_action_new("add-ar", NULL)));
	g_action_map_add_action(G_ACTION_MAP(actions),
	                        G_ACTION(g_simple_action_new("add-cb", NULL)));
	g_signal_connect(g_action_map_lookup_action(G_ACTION_MAP(actions), "add-internal"),
	                 "activate", G_CALLBACK(on_add_internal), NULL);
	g_signal_connect(g_action_map_lookup_action(G_ACTION_MAP(actions), "add-ar"),
	                 "activate", G_CALLBACK(on_add_ar), NULL);
	g_signal_connect(g_action_map_lookup_action(G_ACTION_MAP(actions), "add-cb"),
	                 "activate", G_CALLBACK(on_add_cb), NULL);
	gtk_widget_insert_action_group(sWindow, "win", actions);
	g_object_unref(actions);

	sEditButton = gtk_button_new_with_label("Edit");
	g_signal_connect(sEditButton, "clicked", G_CALLBACK(on_edit), NULL);
	gtk_box_append(GTK_BOX(buttons), sEditButton);
	sRemoveButton = gtk_button_new_with_label("Remove");
	g_signal_connect(sRemoveButton, "clicked", G_CALLBACK(on_remove), NULL);
	gtk_box_append(GTK_BOX(buttons), sRemoveButton);

	sMoveUpButton = gtk_button_new_from_icon_name("go-up-symbolic");
	gtk_widget_set_tooltip_text(sMoveUpButton, "Move the selected cheat up");
	g_signal_connect(sMoveUpButton, "clicked", G_CALLBACK(on_move_up), NULL);
	gtk_box_append(GTK_BOX(buttons), sMoveUpButton);
	sMoveDownButton = gtk_button_new_from_icon_name("go-down-symbolic");
	gtk_widget_set_tooltip_text(sMoveDownButton, "Move the selected cheat down");
	g_signal_connect(sMoveDownButton, "clicked", G_CALLBACK(on_move_down), NULL);
	gtk_box_append(GTK_BOX(buttons), sMoveDownButton);

	GtkWidget *databaseButton = gtk_button_new_with_label("Cheat Database...");
	gtk_widget_set_tooltip_text(databaseButton,
	                            "Copy cheats for this game from an R4 database");
	g_signal_connect(databaseButton, "clicked", G_CALLBACK(on_database), NULL);
	gtk_box_append(GTK_BOX(buttons), databaseButton);

	GtkWidget *closeButton = gtk_button_new_with_mnemonic("_Close");
	g_signal_connect_swapped(closeButton, "clicked",
	                         G_CALLBACK(gtk_window_close), sWindow);
	gtk_box_append(GTK_BOX(buttons), closeButton);

	rebuild_stores();
	gtk_window_present(GTK_WINDOW(sWindow));
	update_buttons();
	update_status();
}

static void
Close(void)
{
	if (sWindow != NULL)
	{
		/* Shutdown cannot wait for an async dialog: persist silently. */
		if (sDirty && (cheats != NULL))
		{
			cheats->save();
		}
		gtk_window_destroy(GTK_WINDOW(sWindow));
	}
}

static gboolean
IsOpen(void)
{
	return sWindow != NULL;
}

} // namespace

const DesmumeTool desmume_tool_cheats =
{
	"cheats",
	"Cheat list",
	Open,
	NULL,     /* no per-frame update: the window pauses the core while open */
	Close,
	IsOpen
};
