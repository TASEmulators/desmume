/* callbacks_IO.h - this file is part of DeSmuME
 *
 * Copyright (C) 2007 Damien Nozay (damdoum)
 * Copyright (C) 2007 Pascal Giard (evilynux)
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
#include "gdk_gl.h"

extern "C" {

/* INPUT BUTTONS / KEYBOARD */
G_MODULE_EXPORT gboolean  on_wMainW_key_press_event    (GtkWidget *widget, GdkEventKey *event, gpointer user_data);
G_MODULE_EXPORT gboolean  on_wMainW_key_release_event  (GtkWidget *widget, GdkEventKey *event, gpointer user_data);

/* OUTPUT SCREENS  */
G_MODULE_EXPORT gboolean  on_wDrawScreen_expose_event  (GtkWidget *widget, GdkEventExpose  *event, gpointer user_data);
G_MODULE_EXPORT gboolean  on_wDrawScreen_configure_event(GtkWidget *widget, GdkEventConfigure *event, gpointer data);


/* INPUT STYLUS / MOUSE */
G_MODULE_EXPORT gboolean  on_wDrawScreen_motion_notify_event  (GtkWidget *widget, GdkEventMotion  *event, gpointer user_data);
G_MODULE_EXPORT gboolean  on_wDrawScreen_button_release_event(GtkWidget *widget, GdkEventButton  *event, gpointer user_data);
G_MODULE_EXPORT gboolean  on_wDrawScreen_button_press_event  (GtkWidget *widget, GdkEventButton  *event, gpointer user_data);
G_MODULE_EXPORT gboolean  on_wDrawScreen_scroll_event        (GtkWidget *widget, GdkEvent *event, gpointer user_data);

/* KEYBOARD CONFIG / KEY DEFINITION */
G_MODULE_EXPORT gboolean  on_wKeyDlg_key_press_event (GtkWidget *widget, GdkEventKey *event, gpointer user_data);
G_MODULE_EXPORT void  on_wKeybConfDlg_response (GtkDialog *dialog, gint arg1, gpointer user_data);
G_MODULE_EXPORT void  on_button_kb_key_clicked    (GtkButton *button, gpointer user_data);

/* Joystick configuration / Key definition */
G_MODULE_EXPORT void on_button_joy_key_clicked (GtkButton *button, gpointer user_data);

}
