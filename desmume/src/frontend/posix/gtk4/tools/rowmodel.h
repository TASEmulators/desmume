/*
	rowmodel.h - reusable GListModel row + GtkColumnView helper for GTK4 tools

	Copyright (C) 2026 DeSmuME team

	This file is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 2 of the License, or
	(at your option) any later version.
*/

#ifndef DESMUME_GTK4_ROWMODEL_H_
#define DESMUME_GTK4_ROWMODEL_H_

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define DESMUME_TYPE_ROW (desmume_row_get_type())
G_DECLARE_FINAL_TYPE(DesmumeRow, desmume_row, DESMUME, ROW, GObject)

/* A row with up to five string columns, bound by desmume_column_view_new(). */
DesmumeRow *desmume_row_new(const char *c0, const char *c1,
                            const char *c2, const char *c3, const char *c4);
void desmume_row_set(DesmumeRow *row, int column, const char *text);
const char *desmume_row_get(DesmumeRow *row, int column);

/* A GListStore typed for DesmumeRow.  Tools append/remove rows and wrap the
 * store in a GtkSingleSelection for the GtkColumnView. */
GListStore *desmume_row_model_new(void);

/* Build a read-only GtkColumnView with `count` columns titled `titles`, bound
 * to columns 0..count-1 of the DesmumeRow model.  The returned view holds a
 * GtkSingleSelection over `model`; keep `model` alive for the view's lifetime. */
GtkWidget *desmume_column_view_new(GListModel *model,
                                   const char *const *titles, int count);

/* Return the (first) selected row position from the view's multi-selection,
 * or -1 when nothing is selected.  `out_positions` optionally receives all
 * selected positions as a GArray of guint (caller frees with g_array_unref). */
int desmume_column_view_selected(GtkWidget *view, GArray **out_positions);

G_END_DECLS

#endif /* DESMUME_GTK4_ROWMODEL_H_ */
