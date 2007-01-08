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

