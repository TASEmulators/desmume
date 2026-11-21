/* movieRecord.h - this file is part of DeSmuME
 *
 * Copyright (C) 2006-2026 DeSmuME Team
 *
 * This file is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2, or (at your option)
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

#ifndef __MOVIE_RECORD_GTK_H__
#define __MOVIE_RECORD_GTK_H__

#include <gtk/gtk.h>

// Opens the Record Movie dialog. Unique-name generation, .dsm extension
// handling, the current emulation flags and the core's default RTC all come
// from shared/movie_info_core.cpp, exactly like the Windows/Cocoa dialogs.
// This file only builds the widgets and forwards the fields to FCEUI_SaveMovie.
void movie_record_dialog_show(GtkWindow *parent);

#endif /*__MOVIE_RECORD_GTK_H__*/
