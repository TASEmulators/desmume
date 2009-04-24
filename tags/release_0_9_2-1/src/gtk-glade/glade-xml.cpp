/* -*- Mode: C; c-basic-offset: 4 -*-
 * libglade - a library for building interfaces from XML files at runtime
 * Copyright (C) 1998-2002  James Henstridge <james@daa.com.au>
 *
 * glade-xml.c: implementation of core public interface functions
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the 
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA  02111-1307, USA.
 */

#include "globals.h"
#include <glade/glade-xml.h>
#include <glade/glade-init.h>
#include <glade/glade-build.h>
#include <gmodule.h>

typedef struct _GladeXMLPrivate nopriv_GladeXMLPrivate;
struct _GladeXMLPrivate {
    GladeInterface *tree;       /* the tree for this GladeXML */
    GtkTooltips *tooltips;      /* if not NULL, holds all tooltip info */
    GHashTable *name_hash;
    GHashTable *signals;
    GtkWindow *toplevel;
    GtkAccelGroup *accel_group;
    GtkWidget *focus_widget;
    GtkWidget *default_widget;
    GList *deferred_props;
};

typedef struct _GladeSignalData GladeSignalData;

struct _GladeSignalData {
    GObject *signal_object;
    char *signal_name;
    char *connect_object;       /* or NULL if there is none */
    gboolean signal_after;
};

static void
autoconnect_foreach_StringObject(const char *signal_handler,
                                 GList * signals, GModule * allsymbols)
{
    GCallback func;

    if (!g_module_symbol(allsymbols, signal_handler, (void **) &func))
        g_warning(_("could not find signal handler '%s'."),
                  signal_handler);
    else
        for (; signals != NULL; signals = signals->next) {
            GladeSignalData *data = (GladeSignalData *) signals->data;
            if (data->connect_object) {
                GladeXML *self =
                    glade_get_widget_tree(GTK_WIDGET(data->signal_object));
                char format[] = "%_\0\0";
                if (sscanf(data->connect_object, "%%%c:", &format[1])) {

                    // this should solve 64bit problems but now memory gets 
                    // (it should get) deallocated when program is destroyed
                    gpointer argument = g_malloc(sizeof(callback_arg));
                    sscanf(data->connect_object + 3, format, argument);

//                              printf ("%f \n",obj);
                    if (data->signal_after)
                        g_signal_connect_after(data->signal_object, data->signal_name, func, argument);
                    else
                        g_signal_connect(data->signal_object, data->signal_name, func, argument);
                } else {

                    GObject *other = (GObject *) g_hash_table_lookup(
                            self->priv->name_hash,
                            data->connect_object);
                    g_signal_connect_object(data->signal_object, data->signal_name, func, other,
                            (GConnectFlags) ((data->signal_after ?  G_CONNECT_AFTER : 0) | G_CONNECT_SWAPPED));
                }

            } else {

                /* the signal_data argument is just a string, but may
                 * be helpful for someone */
                if (data->signal_after)
                    g_signal_connect_after(data->signal_object, data->signal_name, func, NULL);
                else
                    g_signal_connect(data->signal_object, data->signal_name, func, NULL);
            }
        }
}

/**
 * glade_xml_signal_autoconnect_StringObject:
 * @self: the GladeXML object.
 *
 * This function is a variation of glade_xml_signal_connect.  It uses
 * gmodule's introspective features (by openning the module %NULL) to
 * look at the application's symbol table.  From here it tries to match
 * the signal handler names given in the interface description with
 * symbols in the application and connects the signals.
 * 
 * Note that this function will not work correctly if gmodule is not
 * supported on the platform.
 */

void glade_xml_signal_autoconnect_StringObject(GladeXML * self)
{
    GModule *allsymbols;
    nopriv_GladeXMLPrivate *priv;

    g_return_if_fail(self != NULL);
    if (!g_module_supported())
        g_error("glade_xml_signal_autoconnect requires working gmodule");

    /* get a handle on the main executable -- use this to find symbols */
    allsymbols = g_module_open(NULL, (GModuleFlags) 0);
    priv = (nopriv_GladeXMLPrivate *) self->priv;
    g_hash_table_foreach(priv->signals,
                         (GHFunc) autoconnect_foreach_StringObject,
                         allsymbols);
}
