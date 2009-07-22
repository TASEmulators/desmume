/* dTools_display.c
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

#ifndef _DTOOLS_DISPLAY_H_
#define _DTOOLS_DISPLAY_H_

#include <gtk/gtk.h>

typedef struct {
	GtkWidget *widget;
	GdkDrawable *draw;
	GdkGC *gc_fg;
	GdkGC *gc_bg;
	PangoLayout* playout;
	int size_w, size_h;
	int char_w, char_h, padding;
	GList * colors_rgb;
	GList * attr_list;
	PangoAttrList * curr_attr;
} dTools_dsp;

static void inline dTools_display_set_size(dTools_dsp * dsp, int w, int h, int pad) {
	dsp->size_w  = w;
	dsp->size_h  = h;
	dsp->padding = pad;
//	gtk_widget_set_size_request(dsp->widget, 
//		dsp->char_w * w + pad * 2, dsp->char_h * h + pad * 2);
	gtk_widget_set_usize(dsp->widget, 
		dsp->char_w * w + pad * 2, dsp->char_h * h + pad * 2);
}

static void inline dTools_display_init(dTools_dsp * dsp, GtkWidget * widget, int w, int h, int pad) {
	dsp->widget  = widget;
	dsp->draw    = widget->window;
	dsp->gc_fg   = widget->style->fg_gc[widget->state];
	dsp->gc_bg   = widget->style->white_gc;
	dsp->playout = gtk_widget_create_pango_layout(widget, NULL);
	
	dsp->colors_rgb = NULL;
	dsp->attr_list  = NULL;
	dsp->curr_attr  = NULL;

	pango_layout_set_markup(dsp->playout, "<tt>X</tt>",-1);
	pango_layout_get_pixel_size(dsp->playout, &dsp->char_w, &dsp->char_h);
	dTools_display_set_size(dsp, w, h, pad);	
}

// void unref (gpointer data, ...) {
// 	pango_attr_list_unref(data);
// }

static void inline dTools_display_free(dTools_dsp * dsp) {
//	g_list_foreach(dsp->attr_list, (GFunc)unref, NULL);
//	g_object_unref(dsp->playout); // not alloc
}

static void inline dTools_display_add_markup(dTools_dsp * dsp, const char * markup) {
	PangoAttrList *attr;
	pango_parse_markup (markup, -1, 0, &attr, NULL, NULL, NULL);
	dsp->attr_list = g_list_append(dsp->attr_list, attr);
	dsp->curr_attr = attr;
}

static void inline dTools_display_clear(dTools_dsp * dsp) {
	gdk_draw_rectangle(dsp->draw, dsp->gc_bg, TRUE, 0, 0, 
		dsp->widget->allocation.width, dsp->widget->allocation.height);
}

static void inline dTools_display_clear_char(dTools_dsp * dsp, int x, int y, int nb) {
	gdk_draw_rectangle(dsp->draw, dsp->gc_bg, TRUE, 
		x * dsp->char_w + dsp->padding, y * dsp->char_h + dsp->padding,
		nb * dsp->char_w, dsp->char_h);
}

static void inline dTools_display_select_attr(dTools_dsp * dsp, int index) {
	PangoAttrList *attr = NULL;
	attr = (PangoAttrList*) g_list_nth_data(dsp->attr_list, index);
	if (attr != NULL) {
		dsp->curr_attr = attr;
	}
	pango_layout_set_attributes(dsp->playout, dsp->curr_attr);
}

static void inline dTools_display_draw_text(dTools_dsp * dsp, int x, int y, const char * txt) {
	pango_layout_set_text(dsp->playout, txt, -1);
	gdk_draw_layout(dsp->draw, dsp->gc_fg, 
		x * dsp->char_w + dsp->padding, 
		y * dsp->char_h + dsp->padding, 
		dsp->playout);
}

#endif
