/* globals.h - this file is part of DeSmuME
 *
 * Copyright (C) 2007-2015 DeSmuME Team
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

#ifndef __GLOBALS_H__
#define __GLOBALS_H__

#ifndef GTK_UI
#define GTK_UI
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

// Localization
#include <libintl.h>
#define _(String) gettext (String)

#include <SDL.h>

// fix gtk-glade on windows with no configure
#ifndef DATADIR
#define DATADIR " "
#endif
#ifndef GLADEUI_UNINSTALLED_DIR
#define GLADEUI_UNINSTALLED_DIR "glade/"
#endif

#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>
#include <glade/glade.h>
#include <glade/glade-xml.h>


typedef union _callback_arg{
	gpointer      my_pointer;
	gconstpointer my_constpointer;
	
	gfloat   my_float;
	gdouble  my_double;
	gsize    my_size;
	gssize   my_ssize;
	
	gboolean my_boolean;

	guchar   my_uchar;
	guint    my_uint;
	guint8   my_uint8;
	guint16  my_uint16;
	guint32  my_uint32;
	guint64  my_uint64;
	gushort  my_ushort;
	gulong   my_ulong;

	gchar    my_char;
	gint     my_int;
	gint8    my_int8;
	gint16   my_int16;
	gint32   my_int32;
	gint64   my_int64;
	gshort   my_short;
	glong    my_long;	
} callback_arg;
#define dyn_CAST(gtype,var) (((callback_arg*)var)->my_##gtype)

#include "../MMU.h"
#include "../registers.h"
#include "../armcpu.h"
#include "../NDSSystem.h"
#include "../GPU.h"
#include "../shared/sndsdl.h"
#include "../shared/ctrlssdl.h"
#include "../types.h"
#include "../saves.h"
#include "../render3D.h"
#include "desmume.h"

// autoconnect with strings as user_data

void
glade_xml_signal_autoconnect_StringObject (GladeXML *self);

//---

extern int Frameskip;

/* main.cpp */
extern GtkWidget * pWindow;
extern GtkWidget * pDrawingArea, * pDrawingArea2;
extern GladeXML  * xml, * xml_tools;

typedef void (*VoidFunPtr)();
void notify_Tools();
void register_Tool(VoidFunPtr fun);
void unregister_Tool(VoidFunPtr fun);
gchar * get_ui_file (const char *filename);

/* callbacks.cpp */
void enable_rom_features();
void resize (float Size1, float Size2);
void rotate(float angle);
extern gboolean ScreenInvert;

/* callbacks_IO.cpp */
extern float ScreenCoeff_Size[2];
extern float ScreenRotate;

void black_screen ();
void edit_controls();
void init_joy_labels();

#endif /* __GLOBALS_H__ */
