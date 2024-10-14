/* cheatsGTK.cpp - this file is part of DeSmuME
 *
 * Copyright (C) 2006-2024 DeSmuME Team
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
#include "utilsGTK.h"
#include "main.h"
#include "desmume.h"

#undef GPOINTER_TO_INT
#define GPOINTER_TO_INT(p) ((gint)  (glong) (p))

enum {
    COLUMN_INDEX,
    COLUMN_TYPE,
    COLUMN_ENABLED,
    COLUMN_SIZE,
    COLUMN_HI,
    COLUMN_LO,
    COLUMN_AR,
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
    { "Index", TYPE_STRING, COLUMN_INDEX},
    { "Type", TYPE_STRING, COLUMN_TYPE},
    { "Enabled", TYPE_TOGGLE, COLUMN_ENABLED},
    { "Size", TYPE_COMBO, COLUMN_SIZE},
    { "Address", TYPE_STRING, COLUMN_HI},
    { "Value", TYPE_STRING, COLUMN_LO},
    { "AR Code", TYPE_STRING, COLUMN_AR},
    { "Description", TYPE_STRING, COLUMN_DESC}
};

static GtkWidget *win = NULL;
static BOOL shouldBeRunning = FALSE;

// ---------------------------------------------------------------------------------
// CHEATS MENU
// ---------------------------------------------------------------------------------

static void
enabled_toggled(GtkCellRendererToggle * cell,
                gchar * path_str, gpointer data)
{
    GtkTreeModel *model = (GtkTreeModel *) data;
    GtkTreeIter iter, f_iter;
    GtkTreePath *path = gtk_tree_path_new_from_string(path_str);
    gboolean guiEnabled;

    gtk_tree_model_get_iter(model, &f_iter, path);
    gtk_tree_model_filter_convert_iter_to_child_iter(
        GTK_TREE_MODEL_FILTER(model), &iter, &f_iter);
    GtkTreeModel *store =
        gtk_tree_model_filter_get_model(GTK_TREE_MODEL_FILTER(model));

    gtk_tree_model_get(store, &iter, COLUMN_ENABLED, &guiEnabled, -1);

    guiEnabled ^= 1;
    const bool cheatEnabled = (guiEnabled) ? true : false;
    CHEATS_LIST tempCheatItem;
    u32 ii;

    gtk_tree_model_get(store, &iter, COLUMN_INDEX, &ii, -1);

    cheats->toggle(cheatEnabled, ii);

    gtk_list_store_set(GTK_LIST_STORE(store), &iter, COLUMN_ENABLED, guiEnabled, -1);

    gtk_tree_path_free(path);
}

static void cheat_list_modify_cheat(GtkCellRendererText * cell,
                        const gchar * path_string,
                        const gchar * new_text, gpointer data)
{
    GtkTreeModel *model = (GtkTreeModel *) data;
    GtkTreePath *path = gtk_tree_path_new_from_string(path_string);
    GtkTreeIter iter, f_iter;

    gint column =
        GPOINTER_TO_INT(g_object_get_data(G_OBJECT(cell), "column"));

    gtk_tree_model_get_iter(model, &f_iter, path);
    gtk_tree_path_free(path);

    gtk_tree_model_filter_convert_iter_to_child_iter(
        GTK_TREE_MODEL_FILTER(model), &iter, &f_iter);
    GtkTreeModel *store =
        gtk_tree_model_filter_get_model(GTK_TREE_MODEL_FILTER(model));

    {
        u32 ii;
        CHEATS_LIST cheat;

        gtk_tree_model_get(store, &iter, COLUMN_INDEX, &ii, -1);

        cheats->copyItemFromIndex(ii, cheat);

        if (column == COLUMN_LO || column == COLUMN_HI
            || column == COLUMN_SIZE) {
            u32 v = 0;
            u32 data;
            switch (column) {
            case COLUMN_SIZE:
                v = atoi(new_text);
                // If the size is reduced, the data is currently contains may be
                // out of range, so cap it at its maximum value.
                data = std::min(0xFFFFFFFF >> (24 - ((v - 1) << 3)),
                                cheat.code[0][1]);
                cheats->update(v-1, cheat.code[0][0], data,
                             cheat.description, cheat.enabled, ii);
                gtk_list_store_set(GTK_LIST_STORE(store), &iter, COLUMN_LO,
                                   data, -1);
                break;
            case COLUMN_HI:
                sscanf(new_text, "%x", &v);
                v &= 0x0FFFFFFF;
                cheats->update(cheat.size, v, cheat.code[0][1], cheat.description,
                             cheat.enabled, ii);
                break;
            case COLUMN_LO:
                v = atoi(new_text);
                v = std::min(0xFFFFFFFF >> (24 - (cheat.size << 3)), v);
                cheats->update(cheat.size, cheat.code[0][0], v, cheat.description,
                             cheat.enabled, ii);
                break;
            }
            gtk_list_store_set(GTK_LIST_STORE(store), &iter, column, v, -1);
        } else if (column == COLUMN_DESC){
            cheats->setDescription(new_text, ii);
            gtk_list_store_set(GTK_LIST_STORE(store), &iter, column,
                               new_text, -1);
        } else if (column == COLUMN_AR) {
            // Safety: CHEATS::update_AR, though it takes `code` as not const,
            // only performs a non-null check and passes `code` to
            // CHEATS::XXCodeFromString as const, therefore new_text (should)
            // never be modified
            bool isValid =
                cheats->update_AR(const_cast<char *>(new_text),
                                  cheat.description, cheat.enabled, ii);
            if (isValid) {
                gtk_list_store_set(GTK_LIST_STORE(store), &iter, column,
                                   new_text, -1);
            }
        }
    }
}

static void cheat_list_remove_cheat(GtkWidget * widget, gpointer data)
{
    GtkTreeView *tree = (GtkTreeView *) data;
    GtkTreeSelection *selection = gtk_tree_view_get_selection (tree);
    GtkTreeModel *model = gtk_tree_view_get_model (tree);
    GtkTreeIter iter, f_iter;

    if (gtk_tree_selection_get_selected (selection, NULL, &f_iter)){
        u32 ii;
        gboolean valid;
        GtkTreePath *path;

        path = gtk_tree_model_get_path (model, &f_iter);
        gtk_tree_model_filter_convert_iter_to_child_iter(
            GTK_TREE_MODEL_FILTER(model), &iter, &f_iter);
        GtkTreeModel *store =
            gtk_tree_model_filter_get_model(GTK_TREE_MODEL_FILTER(model));

        gtk_tree_model_get(store, &iter, COLUMN_INDEX, &ii, -1);

        valid = gtk_list_store_remove(GTK_LIST_STORE(store), &iter);
        cheats->remove(ii);
        while (valid) {
            gtk_list_store_set(GTK_LIST_STORE(store), &iter, COLUMN_INDEX, ii,
                               -1);
            ii++;
            valid = gtk_tree_model_iter_next(store, &iter);
        }

        gtk_tree_path_free (path);
    }
}

static void cheat_list_add_cheat(GtkWidget * widget, gpointer data)
{
#define NEW_DESC "New cheat"
    GtkListStore *store = (GtkListStore *) data;
    GtkTreeIter iter;
    // Safety: CHEATS::add only uses `description` to call CHEATS::update, which
    // only uses it to call CHEATS::setDescription, which takes `description` as
    // const.
    cheats->add(0, 0, 0, const_cast<char *>(NEW_DESC), false);
    gtk_list_store_append(store, &iter);
    gtk_list_store_set(store, &iter,
                       COLUMN_INDEX, cheats->getListSize() - 1,
                       COLUMN_TYPE, 0,
                       COLUMN_ENABLED, FALSE,
                       COLUMN_SIZE, 1,
                       COLUMN_HI, 0,
                       COLUMN_LO, 0, COLUMN_DESC, NEW_DESC, -1);
#undef NEW_DESC
}

static void cheat_list_add_cheat_AR(GtkWidget *widget, gpointer data)
{
#define NEW_DESC "New cheat"
#define NEW_AR "00000000 00000000"
    GtkListStore *store = (GtkListStore *) data;
    GtkTreeIter iter;
    // Safety: CHEATS::add_AR only uses `code` to call , `description` to call
    // CHEATS::setDescription, which takes the variable as const.
    cheats->add_AR(const_cast<char *>(NEW_AR), const_cast<char *>(NEW_DESC),
                   false);
    gtk_list_store_append(store, &iter);
    gtk_list_store_set(store, &iter,
                       COLUMN_INDEX, cheats->getListSize() - 1,
                       COLUMN_TYPE, 1,
                       COLUMN_ENABLED, FALSE,
                       COLUMN_AR, NEW_AR,
                       COLUMN_DESC, NEW_DESC, -1);
#undef NEW_DESC
#undef NEW_AR
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

static void cheat_list_address_to_hex(GtkTreeViewColumn *column,
                                      GtkCellRenderer *renderer,
                                      GtkTreeModel *model, GtkTreeIter *iter,
                                      gpointer data)
{
    gint addr;
    gtk_tree_model_get(model, iter, COLUMN_HI, &addr, -1);
    gchar *hex_addr = g_strdup_printf("0x0%07X", addr);
    g_object_set(renderer, "text", hex_addr, NULL);
    g_free(hex_addr);
}

static void cheat_list_add_columns(GtkTreeView * tree, GtkListStore * store,
                                   u8 cheat_type)
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
            if (cheat_type == CHEAT_TYPE_INTERNAL)
                renderer = gtk_cell_renderer_text_new();
            else if (cheat_type == CHEAT_TYPE_AR)
                renderer = desmume_cell_renderer_ndtext_new();
            g_object_set(renderer, "editable", TRUE, NULL);
            g_signal_connect(renderer, "edited",
                             G_CALLBACK(cheat_list_modify_cheat), model);
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
                             G_CALLBACK(cheat_list_modify_cheat), model);
            attrib = "text";
            break;
        }
        gint c = columnTable[ii].column;
        column =
            gtk_tree_view_column_new_with_attributes(columnTable[ii].
                                                     caption, renderer,
                                                     attrib, c,
                                                     NULL);
        if (c == COLUMN_HI && cheat_type == CHEAT_TYPE_INTERNAL) {
            gtk_tree_view_column_set_cell_data_func(
                column, renderer, cheat_list_address_to_hex, NULL, NULL);
        }
        if (c == COLUMN_ENABLED || c == COLUMN_DESC ||
            ((c == COLUMN_SIZE || c == COLUMN_HI || c == COLUMN_LO) &&
             cheat_type == CHEAT_TYPE_INTERNAL) ||
            (c == COLUMN_AR && cheat_type == CHEAT_TYPE_AR)) {
            g_object_set_data(G_OBJECT(renderer), "column",
                              GINT_TO_POINTER(columnTable[ii].column));
            gtk_tree_view_append_column(GTK_TREE_VIEW(tree), column);
        }
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
    // COLUMN_INDEX, COLUMN_TYPE, COLUMN_ENABLED, COLUMN_SIZE,
    // COLUMN_HI, COLUMN_LO, COLUMN_AR, COLUMN_DESC
    GtkListStore *store = gtk_list_store_new (
        8, G_TYPE_INT, G_TYPE_INT, G_TYPE_BOOLEAN, G_TYPE_INT,
        G_TYPE_INT, G_TYPE_UINT, G_TYPE_STRING, G_TYPE_STRING);

    CHEATS_LIST cheat;
    u32 chsize = cheats->getListSize();
    for(u32 ii = 0; ii < chsize; ii++){
        GtkTreeIter iter;
		cheats->copyItemFromIndex(ii, cheat);
        gtk_list_store_append(store, &iter);
        if (cheat.type == CHEAT_TYPE_INTERNAL) {
            gtk_list_store_set(store, &iter, COLUMN_INDEX, ii,
                               COLUMN_TYPE, cheat.type,
                               COLUMN_ENABLED, cheat.enabled,
                               COLUMN_SIZE, cheat.size + 1,
                               COLUMN_HI, cheat.code[0][0],
                               COLUMN_LO, cheat.code[0][1],
                               COLUMN_DESC, cheat.description, -1);
        } else if (cheat.type == CHEAT_TYPE_AR) {
            u32 cheat_len = cheat.num;
            char *cheat_str = (char *) malloc(18 * cheat_len);
            cheat_str[0] = '\0';

            // Safety: "%08X" is 8 bytes (x2), " " and "\n" are 1 each for 18
            // bytes each strdup_printf called cheat_len times for the size of
            // the malloc. g_strlcat emulates BSD's strlcat, so on the last
            // iteration, a NUL-terminator is writted instead of the last
            // trailing newline.
            for (u32 jj = 0; jj < cheat_len; jj++) {
                gchar *tmp = g_strdup_printf("%08X %08X\n", cheat.code[jj][0],
                                             cheat.code[jj][1]);
                g_strlcat(cheat_str, tmp, 18 * cheat_len);
                g_free(tmp);
            }

            gtk_list_store_set(store, &iter, COLUMN_INDEX, ii,
                               COLUMN_TYPE, cheat.type,
                               COLUMN_ENABLED, cheat.enabled,
                               COLUMN_AR, cheat_str,
                               COLUMN_DESC, cheat.description, -1);
            free(cheat_str);
        }
    }
    return store;
}

static gboolean cheat_list_is_raw(GtkTreeModel *model, GtkTreeIter *iter,
                                  gpointer data)
{
    gint type = CHEAT_TYPE_EMPTY;
    gtk_tree_model_get(model, iter, COLUMN_TYPE, &type, -1);
    return type == CHEAT_TYPE_INTERNAL;
}

static gboolean cheat_list_is_ar(GtkTreeModel *model, GtkTreeIter *iter,
                                 gpointer data)
{
    gint type = CHEAT_TYPE_EMPTY;
    gtk_tree_model_get(model, iter, COLUMN_TYPE, &type, -1);
    return type == CHEAT_TYPE_AR;
}

static void cheat_list_create_ui()
{
    GtkListStore *store = cheat_list_populate();
    GtkTreeModel *filter_raw =
        gtk_tree_model_filter_new(GTK_TREE_MODEL(store), NULL);
    gtk_tree_model_filter_set_visible_func(GTK_TREE_MODEL_FILTER(filter_raw),
                                           cheat_list_is_raw, NULL, NULL);
    GtkWidget *tree_raw = gtk_tree_view_new_with_model(filter_raw);

    GtkWidget *vbox = gtk_vbox_new(FALSE, 1);
    GtkWidget *hbbox = gtk_hbutton_box_new();
    GtkWidget *button;

    gtk_container_add(GTK_CONTAINER(vbox), GTK_WIDGET(tree_raw));
    gtk_container_add(GTK_CONTAINER(vbox), GTK_WIDGET(hbbox));
    gtk_container_add(GTK_CONTAINER(win), GTK_WIDGET(vbox));

    button = gtk_button_new_with_label("Add internal cheat");
    g_signal_connect (button, "clicked", G_CALLBACK (cheat_list_add_cheat), store);
    gtk_container_add(GTK_CONTAINER(hbbox),button);

    button = gtk_button_new_with_label("Remove internal cheat");
    g_signal_connect (button, "clicked", G_CALLBACK (cheat_list_remove_cheat),
                     GTK_TREE_VIEW(tree_raw));
    gtk_container_add(GTK_CONTAINER(hbbox),button);

    GtkTreeModel *filter_ar =
        gtk_tree_model_filter_new(GTK_TREE_MODEL(store), NULL);
    gtk_tree_model_filter_set_visible_func(GTK_TREE_MODEL_FILTER(filter_ar),
                                           cheat_list_is_ar, NULL, NULL);
    GtkWidget *tree_ar = gtk_tree_view_new_with_model(filter_ar);

    hbbox = gtk_hbutton_box_new();
    gtk_container_add(GTK_CONTAINER(vbox), GTK_WIDGET(tree_ar));
    gtk_container_add(GTK_CONTAINER(vbox), GTK_WIDGET(hbbox));

    button = gtk_button_new_with_label("Add Action Replay cheat");
    gtk_container_add(GTK_CONTAINER(hbbox), button);
    g_signal_connect(button, "clicked", G_CALLBACK(cheat_list_add_cheat_AR),
                     store);

    button = gtk_button_new_with_label("Remove Action Replay cheat");
    g_signal_connect(button, "clicked", G_CALLBACK(cheat_list_remove_cheat),
                     GTK_TREE_VIEW(tree_ar));
    gtk_container_add(GTK_CONTAINER(hbbox), button);

    cheat_list_add_columns(GTK_TREE_VIEW(tree_raw), store, CHEAT_TYPE_INTERNAL);
    cheat_list_add_columns(GTK_TREE_VIEW(tree_ar), store, CHEAT_TYPE_AR);

    /* Setup the selection handler */
    GtkTreeSelection *select;
    select = gtk_tree_view_get_selection (GTK_TREE_VIEW(tree_raw));
    gtk_tree_selection_set_mode (select, GTK_SELECTION_SINGLE);
    select = gtk_tree_view_get_selection (GTK_TREE_VIEW(tree_ar));
    gtk_tree_selection_set_mode (select, GTK_SELECTION_SINGLE);
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
