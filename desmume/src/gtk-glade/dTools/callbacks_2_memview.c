/* callbacks_1_memview.c - this file is part of DeSmuME
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

#include "callbacks_dtools.h"


/* ***** ***** MEMORY VIEWER ***** ***** */

enum SHOW {
	Bit8,
	Bit16,
	Bit32
};

static BOOL init=FALSE;
static enum SHOW packmode=Bit8;
static u32 address=0, tmpaddr=0, bpl=0;
static PangoAttrList *attr_Address, *attr_Pattern1, *attr_Pattern2, *attr_Text;

static GtkEntry       *wAddress;
static GtkDrawingArea *wPaint;

/* how to pack bytes */

void on_wtools_2_r8_toggled            (GtkToggleButton *togglebutton, gpointer user_data) { packmode=Bit8; }
void on_wtools_2_r16_toggled           (GtkToggleButton *togglebutton, gpointer user_data) { packmode=Bit16; }
void on_wtools_2_r32_toggled           (GtkToggleButton *togglebutton, gpointer user_data) { packmode=Bit32; }

/* which address */

void refresh();

void on_wtools_2_GotoAddress_activate  (GtkEntry *entry, gpointer user_data) {
	address=strtol(gtk_entry_get_text(entry),NULL,0);
	refresh();
}
void on_wtools_2_GotoAddress_changed   (GtkEntry *entry, gpointer user_data) {
	tmpaddr=strtol(gtk_entry_get_text(entry),NULL,0);
}
void on_wtools_2_GotoButton_clicked    (GtkButton *button, gpointer user_data) {
	address=tmpaddr;
	refresh();
}


void on_wtools_2_MemView_show          (GtkWidget *widget, gpointer user_data) {
	wAddress = (GtkEntry*)glade_xml_get_widget(xml_tools, "wtools_2_GotoAddress");
	wPaint   = (GtkDrawingArea*)glade_xml_get_widget(xml_tools, "wtools_2_draw");
	refresh();
}

void on_wtools_2_scroll_scroll_child   (GtkScrolledWindow *scrolledwindow, GtkScrollType scroll, gboolean horizontal, gpointer user_data) {
	// scroll
	refresh();
}

gboolean on_wtools_2_draw_expose_event (GtkWidget *widget, GdkEventExpose *event, gpointer user_data) { refresh(); }


void initialize() {
	if (init) return;
	pango_parse_markup("<span foreground=\"blue\"></span>", -1, 0, &attr_Address, NULL, NULL, NULL);
	pango_parse_markup("<tt foreground=\"gray\"></span>", -1, 0, &attr_Pattern1, NULL, NULL, NULL);
	pango_parse_markup("<tt></tt>", -1, 0, &attr_Pattern2, NULL, NULL, NULL);
	pango_parse_markup("<tt></tt>", -1, 0, &attr_Text, NULL, NULL, NULL);
	init = TRUE;
}

/* PAINT memory panel */
void refresh() {
	GtkWidget * area = (GtkWidget*)wPaint;
	PangoLayout* playout = gtk_widget_create_pango_layout(area, NULL);
	GdkGC * GC = area->style->fg_gc[area->state];

	initialize();
	gdk_draw_rectangle(area->window, area->style->white_gc, TRUE, 0, 0,
		area->allocation.width, area->allocation.height);

// draw memory content here
	pango_layout_set_text(playout, "hello", -1);
	pango_layout_set_attributes(playout,attr_Address);
	gdk_draw_layout(area->window, GC, 10,20, playout);

// done
	g_object_unref(playout);
}



/*
void on_wtools_1_combo_cpu_changed    (GtkComboBox *widget, gpointer user_data) {
	// c == 0 means ARM9 
	cpu=gtk_combo_box_get_active(widget);
	display_current_reg();
}
*/

/* ***** ***** IO REGISTERS ***** ***** */
/*

static int cpu=0;
static u32 address;
static BOOL hword;

static GtkLabel * reg_address;
static GtkEntry * reg_value;

void display_current_reg() {
	char text_address[16];
	char text_value[16];
	char * patt = "0x%08X";
	u32 w = MMU_read32(cpu,address);

	if (hword) { patt = "0x%04X"; w &= 0xFFFF; }
	sprintf(text_address, "0x%08X", address);
	sprintf(text_value,   patt, w);
	gtk_label_set_text(reg_address, text_address);
	gtk_entry_set_text(reg_value,   text_value);
}

*/