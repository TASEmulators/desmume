/* utilsGTK.cpp - this file is part of DeSmuME
 *
 * Copyright (C) 2024 DeSmuME Team
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

#include "utilsGTK.h"

/*
    A C++ implementation of a GtkCellRendererText subclass which handles
    newline-delimited text and allows for editing that text with the ability
    for users to add newlines, based off of a GPLv2+ Python implementation here:
    https://gitlab.gnome.org/GNOME/gtk/-/issues/175#note_487323
*/

/*
    DESMUME_ENTRY_ND:
    An object similar to an Entry, but which allows for newlines to be
    inserted by holding Shift, Ctrl, or Alt along with pressing Enter.
*/

typedef struct
{
    GtkWidget *scroll;
    GtkWidget *editor;
    gboolean editing_canceled;
} DesmumeEntryNdPrivate;

typedef enum
{
    PROP_EDITING_CANCELED = 1,
    ENTRY_ND_NUM_PROP,
} DesmumeEntryNdProperty;

static GParamSpec *entry_nd_properties[ENTRY_ND_NUM_PROP] = {
    NULL,
};

// Declared here to statisfy the type creation macro, but defined further down.
static void desmume_entry_nd_editable_init(GtkEditableInterface *iface);
static void desmume_entry_nd_cell_editable_init(GtkCellEditableIface *iface);

G_DEFINE_TYPE_WITH_CODE(
    DesmumeEntryNd, desmume_entry_nd, GTK_TYPE_EVENT_BOX,
    G_ADD_PRIVATE(DesmumeEntryNd)
        G_IMPLEMENT_INTERFACE(GTK_TYPE_EDITABLE, desmume_entry_nd_editable_init)
            G_IMPLEMENT_INTERFACE(GTK_TYPE_CELL_EDITABLE,
                                  desmume_entry_nd_cell_editable_init))

static void desmume_entry_nd_set_property(GObject *object, guint property_id,
                                          const GValue *value,
                                          GParamSpec *pspec)
{
    DesmumeEntryNd *entry_nd = DESMUME_ENTRY_ND(object);
    DesmumeEntryNdPrivate *priv =
        (DesmumeEntryNdPrivate *) desmume_entry_nd_get_instance_private(
            entry_nd);

    switch ((DesmumeEntryNdProperty) property_id) {
    case PROP_EDITING_CANCELED:
        if (priv->editing_canceled != g_value_get_boolean(value)) {
            priv->editing_canceled = g_value_get_boolean(value);
            g_object_notify(object, "editing-canceled");
        }
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
        break;
    }
}

static void desmume_entry_nd_get_property(GObject *object, guint property_id,
                                          GValue *value, GParamSpec *pspec)
{
    DesmumeEntryNd *entry_nd = DESMUME_ENTRY_ND(object);
    DesmumeEntryNdPrivate *priv =
        (DesmumeEntryNdPrivate *) desmume_entry_nd_get_instance_private(
            entry_nd);

    switch ((DesmumeEntryNdProperty) property_id) {
    case PROP_EDITING_CANCELED:
        g_value_set_boolean(value, priv->editing_canceled);
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
        break;
    }
}

static gboolean desmume_entry_nd_key_press(GtkWidget *widget,
                                           GdkEventKey *event, gpointer data)
{
    DesmumeEntryNd *entry_nd = (DesmumeEntryNd *) data;
    DesmumeEntryNdPrivate *priv =
        (DesmumeEntryNdPrivate *) desmume_entry_nd_get_instance_private(
            entry_nd);

    // Allow the editor to decide how to handle the key event, except key events
    // which its parent TextView needs to handle itself
    gboolean doPropagate = GDK_EVENT_PROPAGATE;
    guint kv = event->keyval;
    guint mod = event->state;

    if ((kv == GDK_KEY_Return || kv == GDK_KEY_KP_Enter ||
         kv == GDK_KEY_ISO_Enter) &&
        !(mod & (GDK_CONTROL_MASK | GDK_SHIFT_MASK | GDK_MOD1_MASK))) {
        // Enter + Ctrl, Shift, or Mod1 (commonly Alt), enter a newline in the
        // editor, but otherwise act as confirm for the TextView
        priv->editing_canceled = FALSE;
        doPropagate = GDK_EVENT_STOP;
        gtk_cell_editable_editing_done(GTK_CELL_EDITABLE(entry_nd));
    } else if (kv == GDK_KEY_Escape) {
        priv->editing_canceled = TRUE;
        doPropagate = GDK_EVENT_STOP;
    }
    return doPropagate;
}

static gboolean desmume_entry_nd_button_press(GtkWidget *widget,
                                              GdkEventButton *event,
                                              gpointer data)
{
    GtkTextView *editor = (GtkTextView *) widget;
    GtkWidgetClass *klass = GTK_WIDGET_GET_CLASS(editor);
    klass->button_press_event(widget, event);
    // We have explicitly described how to handle mouse button events, so do not
	// propagate.
    return GDK_EVENT_STOP;
}

static void desmume_entry_nd_class_init(DesmumeEntryNdClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS(klass);
    object_class->set_property = desmume_entry_nd_set_property;
    object_class->get_property = desmume_entry_nd_get_property;

    entry_nd_properties[PROP_EDITING_CANCELED] =
        g_param_spec_boolean("editing-canceled", "Editing Canceled",
                             "The edit was canceled", FALSE, G_PARAM_READWRITE);
    g_object_class_install_properties(object_class, ENTRY_ND_NUM_PROP,
                                      entry_nd_properties);
}

static void desmume_entry_nd_init(DesmumeEntryNd *entry_nd)
{
    DesmumeEntryNdPrivate *priv =
        (DesmumeEntryNdPrivate *) desmume_entry_nd_get_instance_private(
            entry_nd);

    priv->scroll = gtk_scrolled_window_new(NULL, NULL);
    priv->editor = gtk_text_view_new();
    gtk_text_view_set_editable(GTK_TEXT_VIEW(priv->editor), TRUE);
    gtk_text_view_set_accepts_tab(GTK_TEXT_VIEW(priv->editor), FALSE);

    g_signal_connect(priv->editor, "key-press-event",
                     G_CALLBACK(desmume_entry_nd_key_press), entry_nd);
    g_signal_connect(priv->editor, "button-press-event",
                     G_CALLBACK(desmume_entry_nd_button_press), NULL);

    gtk_container_add(GTK_CONTAINER(priv->scroll), priv->editor);
    gtk_container_add(GTK_CONTAINER(entry_nd), priv->scroll);
}

static void desmume_entry_nd_start_editing(GtkCellEditable *cell_editable,
                                           GdkEvent *event)
{
    DesmumeEntryNd *entry_nd = DESMUME_ENTRY_ND(cell_editable);
    DesmumeEntryNdPrivate *priv =
        (DesmumeEntryNdPrivate *) desmume_entry_nd_get_instance_private(
            entry_nd);

    gtk_widget_show_all(GTK_WIDGET(entry_nd));
    gtk_widget_grab_focus(GTK_WIDGET(priv->editor));

    // Highlight the entirety of the editor's text
    GtkTextBuffer *buf = gtk_text_view_get_buffer(GTK_TEXT_VIEW(priv->editor));
    GtkTextIter start, end;
    gtk_text_buffer_get_bounds(buf, &start, &end);
    gtk_text_buffer_select_range(buf, &start, &end);
}

static gchar *desmume_entry_nd_get_text(DesmumeEntryNd *entry_nd)
{
    DesmumeEntryNdPrivate *priv =
        (DesmumeEntryNdPrivate *) desmume_entry_nd_get_instance_private(
            entry_nd);
    GtkTextBuffer *buf = gtk_text_view_get_buffer(GTK_TEXT_VIEW(priv->editor));
    GtkTextIter start, end;
    gtk_text_buffer_get_bounds(buf, &start, &end);

    return gtk_text_buffer_get_text(buf, &start, &end, TRUE);
}

static void desmume_entry_nd_set_text(DesmumeEntryNd *entry_nd,
                                      const gchar *text)
{
    DesmumeEntryNdPrivate *priv;
    priv = (DesmumeEntryNdPrivate *) desmume_entry_nd_get_instance_private(
        entry_nd);
    GtkTextBuffer *buf = gtk_text_view_get_buffer(GTK_TEXT_VIEW(priv->editor));
    gtk_text_buffer_set_text(buf, text, strlen(text));
}

static void desmume_entry_nd_editable_init(GtkEditableInterface *iface) { }

static void desmume_entry_nd_cell_editable_init(GtkCellEditableIface *iface)
{
    iface->start_editing = desmume_entry_nd_start_editing;
}

GtkWidget *desmume_entry_nd_new()
{
    return (GtkWidget *) g_object_new(DESMUME_TYPE_ENTRY_ND, NULL);
}

/*
    DESMUME_CELL_RENDERER_ND_TEXT:
    A subclass of GtkCellRendererText which creates our DesmumeEntryNd instead
    of a GtkEntry, which allows a cell in a TreeView to accepts newlines.
*/

G_DEFINE_TYPE(DesmumeCellRendererNdtext, desmume_cell_renderer_ndtext,
              GTK_TYPE_CELL_RENDERER_TEXT)

static void desmume_cell_renderer_ndtext_editing_done(GtkCellEditable *entry_nd,
                                                      gpointer data)
{
    gboolean canceled;
    g_object_get(entry_nd, "editing-canceled", &canceled, NULL);
    if (!canceled) {
        const gchar *path =
            (gchar *) g_object_get_data(G_OBJECT(entry_nd), "full-text");
        const gchar *new_text =
            desmume_entry_nd_get_text(DESMUME_ENTRY_ND(entry_nd));

        guint signal_id =
            g_signal_lookup("edited", DESMUME_TYPE_CELL_RENDERER_NDTEXT);
        g_signal_emit(data, signal_id, 0, path, new_text);
    }
    gtk_cell_editable_remove_widget(GTK_CELL_EDITABLE(entry_nd));
}

static GtkCellEditable *desmume_cell_renderer_ndtext_start_editing(
    GtkCellRenderer *cell, GdkEvent *event, GtkWidget *widget,
    const gchar *path, const GdkRectangle *background_area,
    const GdkRectangle *cell_area, GtkCellRendererState flags)
{
    DesmumeCellRendererNdtext *ndtext = DESMUME_CELL_RENDERER_NDTEXT(cell);
    gboolean editable;
    g_object_get(G_OBJECT(ndtext), "editable", &editable, NULL);
    if (!editable)
        return NULL;

    gchar *text;
    g_object_get(G_OBJECT(ndtext), "text", &text, NULL);

    GtkWidget *entry_nd = desmume_entry_nd_new();
    if (text != NULL)
        desmume_entry_nd_set_text(DESMUME_ENTRY_ND(entry_nd), text);
    g_object_set_data_full(G_OBJECT(entry_nd), "full-text", g_strdup(path),
                           g_free);

    g_signal_connect(entry_nd, "editing-done",
                     G_CALLBACK(desmume_cell_renderer_ndtext_editing_done),
                     ndtext);
    return GTK_CELL_EDITABLE(entry_nd);
}

static void
desmume_cell_renderer_ndtext_class_init(DesmumeCellRendererNdtextClass *klass)
{
    GtkCellRendererClass *cell_class = GTK_CELL_RENDERER_CLASS(klass);
    cell_class->start_editing = desmume_cell_renderer_ndtext_start_editing;
}
static void desmume_cell_renderer_ndtext_init(DesmumeCellRendererNdtext *ndtext)
{
}

GtkCellRenderer *desmume_cell_renderer_ndtext_new()
{
    return GTK_CELL_RENDERER(
        g_object_new(DESMUME_TYPE_CELL_RENDERER_NDTEXT, NULL));
}
