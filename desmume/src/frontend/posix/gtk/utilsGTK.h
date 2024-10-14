/* utilsGTK.h - this file is part of DeSmuME
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

#ifndef __UTILS_GTK_H__
#define __UTILS_GTK_H__

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define DESMUME_TYPE_ENTRY_ND (desmume_entry_nd_get_type())
G_DECLARE_FINAL_TYPE(DesmumeEntryNd, desmume_entry_nd, DESMUME, ENTRY_ND,
                     GtkEventBox)

struct _DesmumeEntryNd
{
    GtkEventBox parent;
};

struct _DesmumeEntryNdClass
{
    GtkEventBoxClass parent_class;
};

GType desmume_entry_nd_get_type(void) G_GNUC_CONST;
GtkEventBox *entry_nd_new(void);

#define DESMUME_TYPE_CELL_RENDERER_NDTEXT                                      \
    (desmume_cell_renderer_ndtext_get_type())
G_DECLARE_FINAL_TYPE(DesmumeCellRendererNdtext, desmume_cell_renderer_ndtext,
                     DESMUME, CELL_RENDERER_NDTEXT, GtkCellRendererText)

struct _DesmumeCellRendererNdtext
{
    GtkCellRendererText parent;
};

struct _DesmumeCellRendererNdtextClass
{
    GtkCellRendererTextClass parent_class;
};

GType desmume_cell_renderer_ndtext_get_type(void) G_GNUC_CONST;
GtkCellRenderer *desmume_cell_renderer_ndtext_new(void);

G_END_DECLS

#endif /*__UTILS_GTK_H__*/
