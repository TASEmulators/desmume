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

/* MENU FILE */
void  on_menu_ouvrir_activate  (GtkMenuItem *menuitem, gpointer user_data);
void  on_menu_pscreen_activate (GtkMenuItem *menuitem, gpointer user_data);
void  on_menu_quit_activate    (GtkMenuItem *menuitem, gpointer user_data);

/* MENU EMULATION */
void  on_menu_exec_activate    (GtkMenuItem *menuitem, gpointer user_data);
void  on_menu_pause_activate   (GtkMenuItem *menuitem, gpointer user_data);
void  on_menu_reset_activate   (GtkMenuItem *menuitem, gpointer user_data);
void  on_menu_layers_activate  (GtkMenuItem *menuitem, gpointer user_data);
/* SUBMENU FRAMESKIP */
void  on_fsXX_activate  (GtkMenuItem *menuitem, gpointer user_data);
void  on_fs0_activate  (GtkMenuItem *menuitem, gpointer user_data);
void  on_fs1_activate  (GtkMenuItem *menuitem, gpointer user_data);
void  on_fs2_activate  (GtkMenuItem *menuitem, gpointer user_data);
void  on_fs3_activate  (GtkMenuItem *menuitem, gpointer user_data);
void  on_fs4_activate  (GtkMenuItem *menuitem, gpointer user_data);
void  on_fs5_activate  (GtkMenuItem *menuitem, gpointer user_data);
void  on_fs6_activate  (GtkMenuItem *menuitem, gpointer user_data);
void  on_fs7_activate  (GtkMenuItem *menuitem, gpointer user_data);
void  on_fs8_activate  (GtkMenuItem *menuitem, gpointer user_data);
void  on_fs9_activate  (GtkMenuItem *menuitem, gpointer user_data);
/* SUBMENU SIZE */
void  on_size1x_activate (GtkMenuItem *menuitem, gpointer user_data);
void  on_size2x_activate (GtkMenuItem *menuitem, gpointer user_data);
void  on_size3x_activate (GtkMenuItem *menuitem, gpointer user_data);


/* MENU CONFIG */
void  on_menu_controls_activate     (GtkMenuItem *menuitem, gpointer user_data);
void  on_menu_audio_on_activate     (GtkMenuItem *menuitem, gpointer user_data);
void  on_menu_gapscreen_activate    (GtkMenuItem *menuitem, gpointer user_data);
void  on_menu_rightscreen_activate  (GtkMenuItem *menuitem, gpointer user_data);
void  on_menu_rotatescreen_activate (GtkMenuItem *menuitem, gpointer user_data);
/* MENU TOOLS */
void  on_menu_IO_regs_activate      (GtkMenuItem *menuitem, gpointer user_data);
void  on_menu_memview_activate      (GtkMenuItem *menuitem, gpointer user_data);

/* MENU ? */
void  on_menu_apropos_activate      (GtkMenuItem *menuitem, gpointer user_data);


/* TOOLBAR */
void  on_wgt_Open_clicked  (GtkToolButton *toolbutton, gpointer user_data);
void  on_wgt_Exec_toggled  (GtkToggleToolButton *toggletoolbutton, 
                                                       gpointer user_data);
void  on_wgt_Reset_clicked (GtkToolButton *toolbutton, gpointer user_data);
void  on_wgt_Quit_clicked  (GtkToolButton *toolbutton, gpointer user_data);


/* LAYERS MAIN SCREEN */
void  on_wc_1_BG0_toggled  (GtkToggleButton *togglebutton, gpointer user_data);
void  on_wc_1_BG1_toggled  (GtkToggleButton *togglebutton, gpointer user_data);
void  on_wc_1_BG2_toggled  (GtkToggleButton *togglebutton, gpointer user_data);
void  on_wc_1_BG3_toggled  (GtkToggleButton *togglebutton, gpointer user_data);
void  on_wc_1_OBJ_toggled  (GtkToggleButton *togglebutton, gpointer user_data);

/* LAYERS SECOND SCREEN */
void  on_wc_2b_BG0_toggled (GtkToggleButton *togglebutton, gpointer user_data);
void  on_wc_2b_BG1_toggled (GtkToggleButton *togglebutton, gpointer user_data);
void  on_wc_2b_BG2_toggled (GtkToggleButton *togglebutton, gpointer user_data);
void  on_wc_2b_BG3_toggled (GtkToggleButton *togglebutton, gpointer user_data);
void  on_wc_2b_OBJ_toggled (GtkToggleButton *togglebutton, gpointer user_data);
