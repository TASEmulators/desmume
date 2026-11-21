/*
	rowmodel.cpp - reusable GListModel row + GtkColumnView helper

	Copyright (C) 2026 DeSmuME team
*/

#include "rowmodel.h"

struct _DesmumeRow
{
	GObject parent;
	gchar *columns[5];
};

G_DEFINE_TYPE(DesmumeRow, desmume_row, G_TYPE_OBJECT)

static void
desmume_row_finalize(GObject *object)
{
	DesmumeRow *row = DESMUME_ROW(object);
	for (int i = 0; i < 5; ++i)
	{
		g_free(row->columns[i]);
	}
	G_OBJECT_CLASS(desmume_row_parent_class)->finalize(object);
}

static void
desmume_row_class_init(DesmumeRowClass *klass)
{
	G_OBJECT_CLASS(klass)->finalize = desmume_row_finalize;
}

static void
desmume_row_init(DesmumeRow *row)
{
	for (int i = 0; i < 5; ++i)
	{
		row->columns[i] = NULL;
	}
}

DesmumeRow *
desmume_row_new(const char *c0, const char *c1, const char *c2,
                const char *c3, const char *c4)
{
	DesmumeRow *row = DESMUME_ROW(g_object_new(DESMUME_TYPE_ROW, NULL));
	const char *values[5] = { c0, c1, c2, c3, c4 };
	for (int i = 0; i < 5; ++i)
	{
		row->columns[i] = g_strdup(values[i] != NULL ? values[i] : "");
	}
	return row;
}

void
desmume_row_set(DesmumeRow *row, int column, const char *text)
{
	if ((row == NULL) || (column < 0) || (column > 4))
	{
		return;
	}
	g_free(row->columns[column]);
	row->columns[column] = g_strdup(text != NULL ? text : "");
}

const char *
desmume_row_get(DesmumeRow *row, int column)
{
	if ((row == NULL) || (column < 0) || (column > 4))
	{
		return "";
	}
	return row->columns[column] != NULL ? row->columns[column] : "";
}

GListStore *
desmume_row_model_new(void)
{
	return g_list_store_new(DESMUME_TYPE_ROW);
}

/* Factory callbacks close over their column index via a GObject data key. */

static void
factory_setup(GtkListItemFactory *factory, GtkListItem *item, gpointer user_data)
{
	(void)factory;
	GtkWidget *label = gtk_label_new(NULL);
	gtk_label_set_xalign(GTK_LABEL(label), 0.0f);
	gtk_widget_add_css_class(label, "desmume-tool-cell");
	g_object_set_data(G_OBJECT(item), "cell-label", label);
	gtk_list_item_set_child(item, label);
	(void)user_data;
}

static void
factory_bind(GtkListItemFactory *factory, GtkListItem *item, gpointer user_data)
{
	(void)factory;
	const int column = GPOINTER_TO_INT(user_data);
	GtkWidget *label = GTK_WIDGET(g_object_get_data(G_OBJECT(item), "cell-label"));
	DesmumeRow *row = DESMUME_ROW(gtk_list_item_get_item(item));
	if ((label != NULL) && (row != NULL))
	{
		gtk_label_set_text(GTK_LABEL(label), desmume_row_get(row, column));
	}
}

static void
factory_unbind(GtkListItemFactory *factory, GtkListItem *item, gpointer user_data)
{
	(void)factory;
	(void)item;
	(void)user_data;
}

GtkWidget *
desmume_column_view_new(GListModel *model, const char *const *titles, int count)
{
	GtkWidget *view = gtk_column_view_new(NULL);

	/* GtkColumnView requires a selection model; wrap the list store in a
	 * GtkMultiSelection so RAM Watch can select several rows at once, while
	 * single-select tools simply use the first selected item. */
	GtkMultiSelection *selection = gtk_multi_selection_new(model);
	gtk_column_view_set_model(GTK_COLUMN_VIEW(view), GTK_SELECTION_MODEL(selection));
	g_object_unref(selection);
	gtk_widget_add_css_class(view, "desmume-tool-view");

	for (int i = 0; i < count; ++i)
	{
		GtkListItemFactory *factory = gtk_signal_list_item_factory_new();
		g_signal_connect(factory, "setup", G_CALLBACK(factory_setup), NULL);
		g_signal_connect(factory, "bind", G_CALLBACK(factory_bind),
		                 GINT_TO_POINTER(i));
		g_signal_connect(factory, "unbind", G_CALLBACK(factory_unbind), NULL);

		GtkColumnViewColumn *column = gtk_column_view_column_new(titles[i], factory);
		gtk_column_view_column_set_expand(column, (i == 0));
		gtk_column_view_append_column(GTK_COLUMN_VIEW(view), column);
		g_object_unref(column);
		g_object_unref(factory);
	}

	return view;
}

int
desmume_column_view_selected(GtkWidget *view, GArray **out_positions)
{
	if (out_positions != NULL)
	{
		*out_positions = NULL;
	}
	if ((view == NULL) || !GTK_IS_COLUMN_VIEW(view))
	{
		return -1;
	}

	GtkSelectionModel *selection = gtk_column_view_get_model(GTK_COLUMN_VIEW(view));
	if (selection == NULL)
	{
		return -1;
	}

	GArray *positions = g_array_new(FALSE, FALSE, sizeof(guint));
	guint first = (guint)-1;
	GtkBitset *bitset = gtk_selection_model_get_selection(selection);
	for (guint i = 0; i < g_list_model_get_n_items(G_LIST_MODEL(selection)); ++i)
	{
		if (gtk_bitset_contains(bitset, i))
		{
			if (first == (guint)-1)
			{
				first = i;
			}
			guint value = i;
			g_array_append_val(positions, value);
		}
	}

	if (out_positions != NULL)
	{
		*out_positions = positions;
	}
	else
	{
		g_array_unref(positions);
	}

	return (first == (guint)-1) ? -1 : (int)first;
}
