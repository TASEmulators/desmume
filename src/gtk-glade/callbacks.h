/* callbacks.h - this file is part of DeSmuME
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

#include "globals.h"

extern "C" {

/* MENU FILE */
G_MODULE_EXPORT void  on_menu_open_activate  (GtkMenuItem *menuitem, gpointer user_data);
G_MODULE_EXPORT void  on_menu_pscreen_activate (GtkMenuItem *menuitem, gpointer user_data);
G_MODULE_EXPORT void  on_menu_quit_activate    (GtkMenuItem *menuitem, gpointer user_data);

/* MENU SAVES */
G_MODULE_EXPORT void on_loadstateXX_activate (GtkMenuItem *, gpointer );
G_MODULE_EXPORT void on_savestateXX_activate (GtkMenuItem *, gpointer );
G_MODULE_EXPORT void on_savetypeXX_activate  (GtkMenuItem *, gpointer );

/* MENU EMULATION */
G_MODULE_EXPORT void  on_menu_exec_activate    (GtkMenuItem *menuitem, gpointer user_data);
G_MODULE_EXPORT void  on_menu_pause_activate   (GtkMenuItem *menuitem, gpointer user_data);
G_MODULE_EXPORT void  on_menu_reset_activate   (GtkMenuItem *menuitem, gpointer user_data);
G_MODULE_EXPORT void  on_menu_layers_activate  (GtkMenuItem *menuitem, gpointer user_data);
G_MODULE_EXPORT void  on_fsXX_activate         (GtkMenuItem *menuitem, gpointer user_data);
G_MODULE_EXPORT void  on_sizeXX_activate       (GtkMenuItem *menuitem, gpointer user_data);

/* MENU CONFIG */
G_MODULE_EXPORT void  on_menu_controls_activate     (GtkMenuItem *menuitem, gpointer user_data);
G_MODULE_EXPORT void  on_menu_joy_controls_activate (GtkMenuItem *menuitem, gpointer user_data);
G_MODULE_EXPORT void  on_menu_audio_on_activate     (GtkMenuItem *menuitem, gpointer user_data);
G_MODULE_EXPORT void  on_menu_gapscreen_activate    (GtkMenuItem *menuitem, gpointer user_data);
G_MODULE_EXPORT void  on_menu_nogap_activate        (GtkMenuItem *menuitem, gpointer user_data);
G_MODULE_EXPORT void  on_menu_rightscreen_activate  (GtkMenuItem *menuitem, gpointer user_data);
G_MODULE_EXPORT void  on_menu_rotatescreen_activate (GtkMenuItem *menuitem, gpointer user_data);

/* MENU TOOLS */
G_MODULE_EXPORT void  on_menu_wtoolsXX_activate     (GtkMenuItem *menuitem, gpointer user_data);
G_MODULE_EXPORT void  on_menu_IO_regs_activate      (GtkMenuItem *menuitem, gpointer user_data);
G_MODULE_EXPORT void  on_menu_memview_activate      (GtkMenuItem *menuitem, gpointer user_data);
G_MODULE_EXPORT void  on_menu_palview_activate      (GtkMenuItem *menuitem, gpointer user_data);
G_MODULE_EXPORT void  on_menu_tileview_activate     (GtkMenuItem *menuitem, gpointer user_data);

/* MENU ? */
G_MODULE_EXPORT void  on_menu_apropos_activate      (GtkMenuItem *menuitem, gpointer user_data);


/* TOOLBAR */
G_MODULE_EXPORT void  on_wgt_Open_clicked  (GtkToolButton *toolbutton, gpointer user_data);
G_MODULE_EXPORT void  on_wgt_Exec_toggled  (GtkToggleToolButton *toggletoolbutton, 
                                                       gpointer user_data);
G_MODULE_EXPORT void  on_wgt_Reset_clicked (GtkToolButton *toolbutton, gpointer user_data);
G_MODULE_EXPORT void  on_wgt_Quit_clicked  (GtkToolButton *toolbutton, gpointer user_data);

/* LAYERS TOGGLE */
G_MODULE_EXPORT void  on_wc_1_BGXX_toggled  (GtkToggleButton *togglebutton, gpointer user_data);
G_MODULE_EXPORT void  on_wc_2_BGXX_toggled (GtkToggleButton *togglebutton, gpointer user_data);

}
