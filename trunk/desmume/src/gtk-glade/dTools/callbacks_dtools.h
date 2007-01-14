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

#include "../globals.h"

/* ***** ***** IO REGISTERS ***** ***** */
void     on_wtools_1_combo_cpu_changed     (GtkComboBox *, gpointer );
void     on_wtools_1_IOregs_show           (GtkWidget *,   gpointer );
gboolean on_wtools_1_IOregs_close          (GtkWidget *, ...);
void on_wtools_1_autorefresh_toggled       (GtkToggleButton *, gpointer);
void on_wtools_1_refresh_clicked           (GtkButton *, gpointer);

/* ***** ***** MEMORY VIEWER ***** ***** */
void     on_wtools_2_MemView_show             (GtkWidget *,   gpointer );
gboolean on_wtools_2_MemView_close            (GtkWidget *, ...);

void     on_wtools_2_cpu_changed              (GtkComboBox *, gpointer );
void     on_wtools_2_r8_toggled               (GtkToggleButton *, gpointer );
void     on_wtools_2_r16_toggled              (GtkToggleButton *, gpointer );
void     on_wtools_2_r32_toggled              (GtkToggleButton *, gpointer );
void     on_wtools_2_GotoAddress_activate     (GtkEntry *,  gpointer );
void     on_wtools_2_GotoAddress_changed      (GtkEntry *,  gpointer );
void     on_wtools_2_GotoButton_clicked       (GtkButton *, gpointer );
void     on_wtools_2_scroll_value_changed     (GtkRange *,  gpointer );
gboolean on_wtools_2_draw_button_release_event(GtkWidget *, GdkEventButton *, gpointer );
gboolean on_wtools_2_draw_expose_event        (GtkWidget *, GdkEventExpose *, gpointer );
gboolean on_wtools_2_draw_scroll_event        (GtkWidget *, GdkEventScroll *, gpointer );

/* ***** ***** PALETTE VIEWER ***** ***** */
void     on_wtools_3_PalView_show         (GtkWidget *, gpointer );
gboolean on_wtools_3_PalView_close        (GtkWidget *, ...);

gboolean on_wtools_3_PalView_delete_event (GtkWidget *, GdkEvent *,       gpointer );
gboolean on_wtools_3_draw_expose_event    (GtkWidget *, GdkEventExpose *, gpointer );
void     on_wtools_3_palette_changed      (GtkComboBox *,   gpointer );
void     on_wtools_3_palnum_value_changed (GtkSpinButton *, gpointer );
