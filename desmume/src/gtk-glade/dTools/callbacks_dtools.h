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

void on_wtools_1_combo_cpu_changed    (GtkComboBox *widget, gpointer user_data);
void on_wtools_1_IOregs_show          (GtkWidget *widget, gpointer user_data);
void on_wtools_1_r_ipcfifocnt_toggled (GtkToggleButton *togglebutton, gpointer user_data);
void on_wtools_1_r_spicnt_toggled     (GtkToggleButton *togglebutton, gpointer user_data);
void on_wtools_1_r_ime_toggled        (GtkToggleButton *togglebutton, gpointer user_data);
void on_wtools_1_r_ie_toggled         (GtkToggleButton *togglebutton, gpointer user_data);
void on_wtools_1_r_if_toggled         (GtkToggleButton *togglebutton, gpointer user_data);
void on_wtools_1_r_power_cr_toggled   (GtkToggleButton *togglebutton, gpointer user_data);
void on_wtools_1_r_dispa_win0h_toggled(GtkToggleButton *togglebutton, gpointer user_data);
void on_wtools_1_r_dispa_win1h_toggled(GtkToggleButton *togglebutton, gpointer user_data);
void on_wtools_1_r_dispa_win0v_toggled(GtkToggleButton *togglebutton, gpointer user_data);
void on_wtools_1_r_dispa_win1v_toggled(GtkToggleButton *togglebutton, gpointer user_data);
void on_wtools_1_r_dispa_winin_toggled(GtkToggleButton *togglebutton, gpointer user_data);
void on_wtools_1_r_dispa_winout_toggled(GtkToggleButton *togglebutton, gpointer user_data);
void on_wtools_1_r_dispb_win0h_toggled(GtkToggleButton *togglebutton, gpointer user_data);
void on_wtools_1_r_dispb_win1h_toggled(GtkToggleButton *togglebutton, gpointer user_data);
void on_wtools_1_r_dispb_win0v_toggled(GtkToggleButton *togglebutton, gpointer user_data);
void on_wtools_1_r_dispb_win1v_toggled(GtkToggleButton *togglebutton, gpointer user_data);
void on_wtools_1_r_dispb_winin_toggled(GtkToggleButton *togglebutton, gpointer user_data);
void on_wtools_1_r_dispb_winout_toggled(GtkToggleButton *togglebutton, gpointer user_data);

/* ***** ***** MEMORY VIEWER ***** ***** */

void on_wtools_2_MemView_show          (GtkWidget *widget, gpointer user_data);
void on_wtools_2_r8_toggled            (GtkToggleButton *togglebutton, gpointer user_data);
void on_wtools_2_r16_toggled           (GtkToggleButton *togglebutton, gpointer user_data);
void on_wtools_2_r32_toggled           (GtkToggleButton *togglebutton, gpointer user_data);
void on_wtools_2_GotoAddress_activate  (GtkEntry *entry, gpointer user_data);
void on_wtools_2_GotoAddress_changed   (GtkEntry *entry, gpointer user_data);
void on_wtools_2_GotoButton_clicked    (GtkButton *button, gpointer user_data);
void on_wtools_2_scroll_scroll_child   (GtkScrolledWindow *scrolledwindow, GtkScrollType scroll, gboolean horizontal, gpointer user_data);
gboolean on_wtools_2_draw_expose_event (GtkWidget *widget, GdkEventExpose *event, gpointer user_data);
