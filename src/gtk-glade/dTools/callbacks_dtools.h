/* callbacks_dtools.h - this file is part of DeSmuME
 *
 * Copyright (C) 2007 Damien Nozay (damdoum)
 * Author: damdoum at users.sourceforge.net
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

#ifndef __CALLBACKS_DTOOLS_H__
#define __CALLBACKS_DTOOLS_H__

#include "../globals.h"

extern "C" {

/* ***** ***** IO REGISTERS ***** ***** */
G_MODULE_EXPORT void     on_wtools_1_combo_cpu_changed        (GtkComboBox *, gpointer );
G_MODULE_EXPORT void     on_wtools_1_IOregs_show              (GtkWidget *,   gpointer );
G_MODULE_EXPORT gboolean on_wtools_1_IOregs_close             (GtkWidget *, ...);
G_MODULE_EXPORT gboolean on_wtools_1_draw_button_release_event(GtkWidget *, GdkEventButton *, gpointer );
G_MODULE_EXPORT gboolean on_wtools_1_draw_expose_event        (GtkWidget *widget, GdkEventExpose *event, gpointer user_data);

/* ***** ***** MEMORY VIEWER ***** ***** */
G_MODULE_EXPORT void     on_wtools_2_MemView_show             (GtkWidget *,   gpointer );
G_MODULE_EXPORT gboolean on_wtools_2_MemView_close            (GtkWidget *, ...);

G_MODULE_EXPORT void     on_wtools_2_cpu_changed              (GtkComboBox *, gpointer );
G_MODULE_EXPORT void     on_wtools_2_r8_toggled               (GtkToggleButton *, gpointer );
G_MODULE_EXPORT void     on_wtools_2_r16_toggled              (GtkToggleButton *, gpointer );
G_MODULE_EXPORT void     on_wtools_2_r32_toggled              (GtkToggleButton *, gpointer );
G_MODULE_EXPORT void     on_wtools_2_GotoAddress_activate     (GtkEntry *,  gpointer );
G_MODULE_EXPORT void     on_wtools_2_GotoAddress_changed      (GtkEntry *,  gpointer );
G_MODULE_EXPORT void     on_wtools_2_GotoButton_clicked       (GtkButton *, gpointer );
G_MODULE_EXPORT void     on_wtools_2_scroll_value_changed     (GtkRange *,  gpointer );
G_MODULE_EXPORT gboolean on_wtools_2_draw_button_release_event(GtkWidget *, GdkEventButton *, gpointer );
G_MODULE_EXPORT gboolean on_wtools_2_draw_expose_event        (GtkWidget *, GdkEventExpose *, gpointer );
G_MODULE_EXPORT gboolean on_wtools_2_draw_scroll_event        (GtkWidget *, GdkEventScroll *, gpointer );

/* ***** ***** PALETTE VIEWER ***** ***** */
// initialise combo box for all palettes
void init_combo_palette(GtkComboBox *combo, u16 ** addresses);

G_MODULE_EXPORT void     on_wtools_3_PalView_show         (GtkWidget *, gpointer );
G_MODULE_EXPORT gboolean on_wtools_3_PalView_close        (GtkWidget *, ...);
G_MODULE_EXPORT gboolean on_wtools_3_PalView_delete_event (GtkWidget *, GdkEvent *,       gpointer );
G_MODULE_EXPORT gboolean on_wtools_3_draw_expose_event    (GtkWidget *, GdkEventExpose *, gpointer );
G_MODULE_EXPORT void     on_wtools_3_palette_changed      (GtkComboBox *,   gpointer );
G_MODULE_EXPORT void     on_wtools_3_palnum_value_changed (GtkSpinButton *, gpointer );


/* ***** ***** TILE VIEWER ***** ***** */
// initialise combo box for all palettes
void init_combo_memory(GtkComboBox *combo, u8 ** addresses);

G_MODULE_EXPORT void     on_wtools_4_TileView_show         (GtkWidget *, gpointer );
G_MODULE_EXPORT gboolean on_wtools_4_TileView_close        (GtkWidget *, ...);
G_MODULE_EXPORT gboolean on_wtools_4_TileView_delete_event (GtkWidget *, GdkEvent *,       gpointer );
G_MODULE_EXPORT void     on_wtools_4_memory_changed       (GtkComboBox *,   gpointer );
G_MODULE_EXPORT void     on_wtools_4_palette_changed      (GtkComboBox *,   gpointer );
G_MODULE_EXPORT void     on_wtools_4_palnum_value_changed (GtkSpinButton *, gpointer );
G_MODULE_EXPORT void	 on_wtools_4_rXX_toggled          (GtkToggleButton *togglebutton, gpointer user_data);
G_MODULE_EXPORT gboolean on_wDraw_Tile_expose_event       (GtkWidget *, GdkEventExpose *, gpointer );

}

#endif
