/* cheats.cpp - this file is part of DeSmuME
 *
 * Copyright (C) 2006-2009 DeSmuME Team
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
#include <gtk/gtk.h>
#include "cheatsGTK.h"
#include "cheatSystem.h"
#include "main.h"
#include "desmume.h"

#undef GPOINTER_TO_INT
#define GPOINTER_TO_INT(p) ((gint)  (glong) (p))

enum {
    COLUMN_ENABLED,
    COLUMN_SIZE,
    COLUMN_HI,
    COLUMN_LO,
    COLUMN_DESC,
    NUM_COL
};

enum
{
  COLUMN_SIZE_TEXT,
  NUM_SIZE_COLUMNS
};

enum {
    TYPE_TOGGLE,
    TYPE_COMBO,
    TYPE_STRING
};

static struct {
    const gchar *caption;
    gint type;
    gint column;
} columnTable[]={
    { "Enabled", TYPE_TOGGLE, COLUMN_ENABLED},
    { "Size", TYPE_COMBO, COLUMN_SIZE},
    { "Offset", TYPE_STRING, COLUMN_HI},
    { "Value", TYPE_STRING, COLUMN_LO},
    { "Description", TYPE_STRING, COLUMN_DESC}
};

static GtkWidget *win = NULL;
static BOOL shouldBeRunning = FALSE;

// ---------------------------------------------------------------------------------
// SEARCH
// ---------------------------------------------------------------------------------

static void
enabled_toggled(GtkCellRendererToggle * cell,
                gchar * path_str, gpointer data)
{
    GtkTreeModel *model = (GtkTreeModel *) data;
    GtkTreeIter iter;
    GtkTreePath *path = gtk_tree_path_new_from_string(path_str);
    gboolean enabled;

    gtk_tree_model_get_iter(model, &iter, path);
    gtk_tree_model_get(model, &iter, COLUMN_ENABLED, &enabled, -1);

    enabled ^= 1;
    CHEATS_LIST cheat;
    u32 ii;
    GtkTreePath *path1;

    path1 = gtk_tree_model_get_path (model, &iter);
    ii = gtk_tree_path_get_indices (path)[0];

    cheats->get(&cheat, ii);

    cheats->update(cheat.size, cheat.code[0][0], cheat.code[0][1], cheat.description,
                 enabled, ii);

    gtk_list_store_set(GTK_LIST_STORE(model), &iter, COLUMN_ENABLED, enabled, -1);

    gtk_tree_path_free(path);
}

static void cheat_list_modify_cheat(GtkCellRendererText * cell,
                        const gchar * path_string,
                        const gchar * new_text, gpointer data)
{
    GtkTreeModel *model = (GtkTreeModel *) data;
    GtkTreePath *path = gtk_tree_path_new_from_string(path_string);
    GtkTreeIter iter;

    gint column =
        GPOINTER_TO_INT(g_object_get_data(G_OBJECT(cell), "column"));

    gtk_tree_model_get_iter(model, &iter, path);

    {
        u32 ii;
        GtkTreePath *path1;
        CHEATS_LIST cheat;

        path1 = gtk_tree_model_get_path (model, &iter);
        ii = gtk_tree_path_get_indices (path)[0];

        cheats->get(&cheat, ii);

        gtk_tree_path_free (path1);

        if (column == COLUMN_LO || column == COLUMN_HI
            || column == COLUMN_SIZE) {
            u32 v = atoi(new_text);
            switch (column) {
            case COLUMN_SIZE:
                cheats->update(v-1, cheat.code[0][0], cheat.code[0][1],
                             cheat.description, cheat.enabled, ii);
                break;
            case COLUMN_HI:
                cheats->update(cheat.size, v, cheat.code[0][1], cheat.description,
                             cheat.enabled, ii);
                break;
            case COLUMN_LO:
                cheats->update(cheat.size, cheat.code[0][0], v, cheat.description,
                             cheat.enabled, ii);
                break;
            }
            gtk_list_store_set(GTK_LIST_STORE(model), &iter, column,
                               atoi(new_text), -1);
        } else if (column == COLUMN_DESC){
            cheats->update(cheat.size, cheat.code[0][0], cheat.code[0][1],
                         g_strdup(new_text), cheat.enabled, ii);
            gtk_list_store_set(GTK_LIST_STORE(model), &iter, column,
                               g_strdup(new_text), -1);
        }

    }
}

static void cheat_list_remove_cheat(GtkWidget * widget, gpointer data)
{
    GtkTreeView *tree = (GtkTreeView *) data;
    GtkTreeSelection *selection = gtk_tree_view_get_selection (tree);
    GtkTreeModel *model = gtk_tree_view_get_model (tree);
    GtkTreeIter iter;

    if (gtk_tree_selection_get_selected (selection, NULL, &iter)){
        u32 ii;
        GtkTreePath *path;

        path = gtk_tree_model_get_path (model, &iter);
        ii = gtk_tree_path_get_indices (path)[0];

        gtk_list_store_remove(GTK_LIST_STORE(model), &iter);
        cheats->remove(ii);

        gtk_tree_path_free (path);
    }
}

static void cheat_list_add_cheat(GtkWidget * widget, gpointer data)
{
#define NEW_DESC "New cheat"
    GtkListStore *store = (GtkListStore *) data;
    GtkTreeIter iter;
    cheats->add(1, 0, 0, g_strdup(NEW_DESC), FALSE);
    gtk_list_store_append(store, &iter);
    gtk_list_store_set(store, &iter,
                       COLUMN_ENABLED, FALSE,
                       COLUMN_SIZE, 1,
                       COLUMN_HI, 0,
                       COLUMN_LO, 0, COLUMN_DESC, NEW_DESC, -1);

#undef NEW_DESC
}

static GtkTreeModel * create_numbers_model (void)
{
#define N_NUMBERS 4
  gint i = 0;
  GtkListStore *model;
  GtkTreeIter iter;

  /* create list store */
  model = gtk_list_store_new (NUM_SIZE_COLUMNS, G_TYPE_STRING, G_TYPE_INT);

  /* add numbers */
  for (i = 1; i < N_NUMBERS+1; i++)
    {
      char str[2];

      str[0] = '0' + i;
      str[1] = '\0';

      gtk_list_store_append (model, &iter);

      gtk_list_store_set (model, &iter,
                          COLUMN_SIZE_TEXT, str,
                          -1);
    }

  return GTK_TREE_MODEL (model);

#undef N_NUMBERS
}

static void cheat_list_add_columns(GtkTreeView * tree, GtkListStore * store)
{

    GtkTreeModel *model = gtk_tree_view_get_model(GTK_TREE_VIEW(tree));
    static GtkTreeModel * size_model;

    for (u32 ii = 0; ii < sizeof(columnTable) / sizeof(columnTable[0]); ii++) {
        GtkCellRenderer *renderer = NULL;
        GtkTreeViewColumn *column;
        const gchar *attrib = NULL;
        switch (columnTable[ii].type) {
        case TYPE_TOGGLE:
            renderer = gtk_cell_renderer_toggle_new();
            g_signal_connect(renderer, "toggled",
                             G_CALLBACK(enabled_toggled), model);
            attrib = "active";
            break;
        case TYPE_STRING:
            renderer = gtk_cell_renderer_text_new();
            g_object_set(renderer, "editable", TRUE, NULL);
            g_signal_connect(renderer, "edited",
                             G_CALLBACK(cheat_list_modify_cheat), store);
            attrib = "text";
            break;
        case TYPE_COMBO:
            renderer = gtk_cell_renderer_combo_new();
            size_model = create_numbers_model();
            g_object_set(renderer,
                         "model", size_model,
                         "text-column", COLUMN_SIZE_TEXT,
                         "editable", TRUE, 
                         "has-entry", FALSE, 
                         NULL);
            g_object_unref(size_model);
            g_signal_connect(renderer, "edited",
                             G_CALLBACK(cheat_list_modify_cheat), store);
            attrib = "text";
            break;
        }
        column =
            gtk_tree_view_column_new_with_attributes(columnTable[ii].
                                                     caption, renderer,
                                                     attrib, columnTable[ii].column,
                                                     NULL);
        g_object_set_data(G_OBJECT(renderer), "column",
                          GINT_TO_POINTER(columnTable[ii].column));
        gtk_tree_view_append_column(GTK_TREE_VIEW(tree), column);
    }

}

static void cheatListEnd()
{
    cheats->save();
    if(shouldBeRunning)
        Launch();
}

static GtkListStore *cheat_list_populate()
{
    GtkListStore *store = gtk_list_store_new (5, G_TYPE_BOOLEAN, 
            G_TYPE_INT, G_TYPE_INT, G_TYPE_INT, G_TYPE_STRING);

    CHEATS_LIST cheat;
    u32 chsize = cheats->getSize();
    for(u32 ii = 0; ii < chsize; ii++){
        GtkTreeIter iter;
        cheats->get(&cheat, ii);
        gtk_list_store_append(store, &iter);
        gtk_list_store_set(store, &iter,
                COLUMN_ENABLED, cheat.enabled,
                COLUMN_SIZE, cheat.size+1,
                COLUMN_HI, cheat.code[0][0],
                COLUMN_LO, cheat.code[0][1],
                COLUMN_DESC, cheat.description,
                -1);
    }
    return store;
}

static GtkWidget *cheat_list_create_ui()
{
    GtkListStore *store = cheat_list_populate();
    GtkWidget *tree = gtk_tree_view_new_with_model (GTK_TREE_MODEL (store));
    GtkWidget *vbox = gtk_vbox_new(FALSE, 1);
    GtkWidget *hbbox = gtk_hbutton_box_new();
    GtkWidget *button;
  
    gtk_container_add(GTK_CONTAINER(vbox), GTK_WIDGET(tree));
    gtk_container_add(GTK_CONTAINER(vbox), GTK_WIDGET(hbbox));
    gtk_container_add(GTK_CONTAINER(win), GTK_WIDGET(vbox));

    button = gtk_button_new_with_label("add cheat");
    g_signal_connect (button, "clicked", G_CALLBACK (cheat_list_add_cheat), store);
    gtk_container_add(GTK_CONTAINER(hbbox),button);

    button = gtk_button_new_with_label("Remove cheat");
    g_signal_connect (button, "clicked", G_CALLBACK (cheat_list_remove_cheat), tree);
    gtk_container_add(GTK_CONTAINER(hbbox),button);

    cheat_list_add_columns(GTK_TREE_VIEW(tree), store);
    
    /* Setup the selection handler */
    GtkTreeSelection *select;
    select = gtk_tree_view_get_selection (GTK_TREE_VIEW (tree));
    gtk_tree_selection_set_mode (select, GTK_SELECTION_SINGLE);

    return tree;
}

void CheatList ()
{
    shouldBeRunning = desmume_running();
    Pause();
    win = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(win),"Cheat List");
    gtk_window_set_modal(GTK_WINDOW(win), TRUE);
    g_signal_connect(G_OBJECT(win), "destroy", cheatListEnd, NULL);

    cheat_list_create_ui();

    gtk_widget_show_all(win);
}

// ---------------------------------------------------------------------------------
// SEARCH
// ---------------------------------------------------------------------------------

static void cheat_search_create_ui()
{
    GtkWidget *button;
    GtkWidget *vbox = gtk_vbox_new(FALSE, 1);
    GtkWidget *hbbox = gtk_hbutton_box_new();
    GtkWidget *b;
    
    gtk_container_add(GTK_CONTAINER(win), GTK_WIDGET(vbox));

    {
        b = gtk_hbox_new(FALSE, 1);
        gtk_container_add(GTK_CONTAINER(vbox), GTK_WIDGET(b));

        {
            GtkTreeModel * size_model;
            GtkWidget *w;
            w = gtk_label_new("size");
            gtk_container_add(GTK_CONTAINER(b), w);

            size_model = create_numbers_model();

            w = gtk_combo_box_new_with_model(size_model);
            g_object_unref(size_model);
            GtkCellRenderer * renderer = gtk_cell_renderer_text_new ();
            gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (w), renderer, TRUE);
            gtk_cell_layout_set_attributes (GTK_CELL_LAYOUT (w), renderer,
                            "text", COLUMN_SIZE_TEXT,
                            NULL);
            gtk_combo_box_set_active (GTK_COMBO_BOX (w), 0);
            gtk_container_add(GTK_CONTAINER(b), w);
        }

        b = gtk_hbox_new(FALSE, 1);
        gtk_container_add(GTK_CONTAINER(vbox), GTK_WIDGET(b));

        {
            GtkWidget *w;
            w = gtk_label_new("signedness");
            gtk_container_add(GTK_CONTAINER(b), w);

//            m = create_sign_model();

            w = gtk_combo_box_new();
//            w = gtk_combo_box_new_with_model(size_model);
//            g_object_unref(size_model);
//            size_model = NULL;
            GtkCellRenderer * renderer = gtk_cell_renderer_text_new ();
            gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (w), renderer, TRUE);
//            gtk_cell_layout_set_attributes (GTK_CELL_LAYOUT (w), renderer,
//                            "text", COLUMN_SIZE_TEXT,
//                            NULL);
            gtk_combo_box_set_active (GTK_COMBO_BOX (w), 0);
            gtk_container_add(GTK_CONTAINER(b), w);
        }
    }

    // BUTTONS:

    gtk_container_add(GTK_CONTAINER(vbox), GTK_WIDGET(hbbox));

    button = gtk_button_new_with_label("add cheats");
//    g_signal_connect (button, "clicked", g_callback (cheat_list_add_cheat), store);
    gtk_container_add(GTK_CONTAINER(hbbox),button);

    button = gtk_button_new_with_label("search");
//    g_signal_connect (button, "clicked", g_callback (cheat_list_add_cheat), store);
    gtk_container_add(GTK_CONTAINER(hbbox),button);

//    GtkWidget *vbox = gtk_vbox_new(FALSE, 1);
//    gtk_container_add(GTK_CONTAINER(win), GTK_WIDGET(vbox));
}

static void cheatSearchEnd()
{
}

void CheatSearch ()
{
    shouldBeRunning = desmume_running();
    Pause();
    win = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(win),"Cheat Search");
    gtk_window_set_modal(GTK_WINDOW(win), TRUE);
    g_signal_connect(G_OBJECT(win), "destroy", cheatSearchEnd, NULL);

    cheat_search_create_ui();

    gtk_widget_show_all(win);
}
