/*
	display.h - GTK4 video widget for DeSmuME

	Copyright (C) 2026 DeSmuME team

	This file is free software: you can redistribute it and/or modify it
	under the terms of the GNU General Public License as published by the
	Free Software Foundation, either version 2 of the License, or (at your
	option) any later version.
*/

#ifndef DESMUME_GTK4_DISPLAY_H_
#define DESMUME_GTK4_DISPLAY_H_

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define DESMUME_TYPE_DISPLAY (desmume_display_get_type())
G_DECLARE_FINAL_TYPE(DesmumeDisplay, desmume_display, DESMUME, DISPLAY, GtkWidget)

typedef enum
{
	DESMUME_LAYOUT_VERTICAL = 0,
	DESMUME_LAYOUT_HORIZONTAL,
	DESMUME_LAYOUT_SINGLE,
	DESMUME_LAYOUT_HYBRID_EQUAL,
	DESMUME_LAYOUT_HYBRID_VERTICAL
} DesmumeLayout;

GtkWidget *desmume_display_new(void);

/* `bgra` is a width x height BGRX framebuffer: the top screen occupies the
 * first height/2 rows and the bottom screen the remaining rows.  The widget
 * copies it into two GdkMemoryTextures (top/bottom), so the caller may reuse
 * its buffer as soon as this returns. */
void desmume_display_push_frame(DesmumeDisplay *display,
                                const guchar *bgra, int width, int height);

void desmume_display_set_layout(DesmumeDisplay *display, DesmumeLayout layout);
void desmume_display_set_rotation(DesmumeDisplay *display, int degrees);
void desmume_display_set_gap(DesmumeDisplay *display, int gap);
void desmume_display_set_swap(DesmumeDisplay *display, gboolean swap);
void desmume_display_set_hybrid_scale(DesmumeDisplay *display, double scale);

/* Map widget-local coordinates into bottom-screen framebuffer pixels.
 * Coordinates are before nds_screen.touch_matrix (that conversion stays in
 * the emulator side).  FALSE means the point is outside the touch screen. */
gboolean desmume_display_map_touch(DesmumeDisplay *display,
                                   double x, double y,
                                   double *out_x, double *out_y);

G_END_DECLS

#endif /* DESMUME_GTK4_DISPLAY_H_ */
