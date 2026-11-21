/*
	movie.h - movie Play/Record dialogs for the DeSmuME GTK4 frontend

	Copyright (C) 2026 DeSmuME team
*/

#ifndef DESMUME_GTK4_MOVIE_H_
#define DESMUME_GTK4_MOVIE_H_

#include <gtk/gtk.h>

G_BEGIN_DECLS

/* Both dialogs are modal-like transient windows and pause emulation when
 * opened.  parent is the main window (may be NULL). */
void desmume_movie_play_dialog_show(GtkWindow *parent);
void desmume_movie_record_dialog_show(GtkWindow *parent);

G_END_DECLS

#endif /* DESMUME_GTK4_MOVIE_H_ */
