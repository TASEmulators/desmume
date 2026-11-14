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
#define DESMUME_ENTRY_ND(obj)                                                  \
    (G_TYPE_CHECK_INSTANCE_CAST((obj), DESMUME_TYPE_ENTRY_ND, DesmumeEntryNd))
#define DESMUME_ENTRY_ND_CLASS(klass)                                          \
    (G_TYPE_CHECK_CLASS_CAST((klass), DESMUME_TYPE_ENTRY_ND, DesmumeEntryNdClass))
#define DESMUME_IS_ENTRY_ND(obj)                                               \
    (G_TYPE_CHECK_INSTANCE_TYPE((obj), DESMUME_TYPE_ENTRY_ND))
#define DESMUME_IS_ENTRY_ND_CLASS(klass)                                       \
    (G_TYPE_CHECK_CLASS_TYPE((klass), DESMUME_TYPE_ENTRY_ND))
#define DESMUME_ENTRY_ND_GET_CLASS(obj)                                        \
    (G_TYPE_INSTANCE_GET_CLASS((obj), DESMUME_TYPE_ENTRY_ND, DesmumeEntryNdClass))

typedef struct _DesmumeEntryNd DesmumeEntryNd;
typedef struct _DesmumeEntryNdClass DesmumeEntryNdClass;
typedef struct _DesmumeEntryNdPrivate DesmumeEntryNdPrivate;

struct _DesmumeEntryNd
{
    GtkEventBox parent;
    DesmumeEntryNdPrivate *priv;
};

struct _DesmumeEntryNdClass
{
    GtkEventBoxClass parent_class;
};

GType desmume_entry_nd_get_type(void) G_GNUC_CONST;
GtkEventBox *entry_nd_new(void);

#define DESMUME_TYPE_CELL_RENDERER_NDTEXT                                      \
    (desmume_cell_renderer_ndtext_get_type())
#define DESMUME_CELL_RENDERER_NDTEXT(obj)                                      \
    (G_TYPE_CHECK_INSTANCE_CAST((obj), DESMUME_TYPE_CELL_RENDERER_NDTEXT,      \
                                DesmumeCellRendererNdtext))
#define DESMUME_CELL_RENDERER_NDTEXT_CLASS(klass)                              \
    (G_TYPE_CHECK_CLASS_CAST(                                                  \
        (klass), DESMUME_TYPE_CELL_RENDERER_NDTEXT DesmumeCellRendererNdtextClass))
#define DESMUME_IS_CELL_RENDERER_NDTEXT(obj)                                   \
    (G_TYPE_CHECK_INSTANCE_TYPE((obj), DESMUME_TYPE_CELL_RENDERER_NDTEXT))
#define DESMUME_IS_CELL_RENDERER_NDTEXT_CLASS(klass)                           \
    (G_TYPE_CHECK_CLASS_TYPE((klass), DESMUME_TYPE_CELL_RENDERER_NDTEXT))
#define DESMUME_CELL_RENDERER_NDTEXT_GET_CLASS(obj)                            \
    (G_TYPE_INSTANCE_GET_CLASS(obj), DESMUME_TYPE_CELL_RENDERER_NDTEXT,        \
     DesmumeCellRendererNdtextClass)

typedef struct _DesmumeCellRendererNdtext DesmumeCellRendererNdtext;
typedef struct _DesmumeCellRendererNdtextClass DesmumeCellRendererNdtextClass;

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
