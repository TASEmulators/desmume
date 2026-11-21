/* moviePlay.h - this file is part of DeSmuME
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

#ifndef __MOVIE_PLAY_GTK_H__
#define __MOVIE_PLAY_GTK_H__

#include <gtk/gtk.h>

// Opens the Play Movie dialog. All decisions (header parsing, ROM/core desync
// assessment, comment rewriting, pause-frame validation) live in
// shared/movie_info_core.cpp, shared with Windows and Cocoa. This is UI only.
// The parent window is the main DeSmuME window.
void movie_play_dialog_show(GtkWindow *parent);

#endif /*__MOVIE_PLAY_GTK_H__*/
