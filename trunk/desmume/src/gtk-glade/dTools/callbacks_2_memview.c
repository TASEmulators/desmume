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
static u32 address=0, tmpaddr=0, bpl=0; int cpu=0;
static PangoAttrList *attr_Address, *attr_Pattern1, *attr_Pattern2, *attr_Text;

static GtkEntry       *wAddress;
static GtkDrawingArea *wPaint;
void refresh();


void on_wtools_2_cpu_changed           (GtkComboBox *widget, gpointer user_data) {
	/* c == 0 means ARM9 */
	cpu=gtk_combo_box_get_active(widget);
	refresh();
}


/* how to pack bytes */

void on_wtools_2_r8_toggled            (GtkToggleButton *togglebutton, gpointer user_data) { packmode=Bit8; refresh(); }
void on_wtools_2_r16_toggled           (GtkToggleButton *togglebutton, gpointer user_data) { packmode=Bit16; refresh(); }
void on_wtools_2_r32_toggled           (GtkToggleButton *togglebutton, gpointer user_data) { packmode=Bit32; refresh(); }

/* which address */

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
	GtkWidget * combo = glade_xml_get_widget(xml_tools, "wtools_2_cpu");
	wAddress = (GtkEntry*)glade_xml_get_widget(xml_tools, "wtools_2_GotoAddress");
	wPaint   = (GtkDrawingArea*)glade_xml_get_widget(xml_tools, "wtools_2_draw");
	gtk_combo_box_set_active((GtkComboBox*)combo, 0);
	refresh();
}

void on_wtools_2_scroll_scroll_child   (GtkScrolledWindow *scrolledwindow, GtkScrollType scroll, gboolean horizontal, gpointer user_data) {
	// scroll
	refresh();
}

gboolean on_wtools_2_draw_expose_event (GtkWidget *widget, GdkEventExpose *event, gpointer user_data) { refresh(); }


void initialize() {
	if (init) return;
	pango_parse_markup("<span foreground=\"blue\"> </span>", -1, 0, &attr_Address, NULL, NULL, NULL);
	pango_parse_markup("<tt foreground=\"gray\"> </span>", -1, 0, &attr_Pattern1, NULL, NULL, NULL);
	pango_parse_markup("<tt> </tt>", -1, 0, &attr_Pattern2, NULL, NULL, NULL);
	pango_parse_markup("<tt> </tt>", -1, 0, &attr_Text, NULL, NULL, NULL);
	init = TRUE;
}

/* PAINT memory panel */
void refresh() {
	GtkWidget * area = (GtkWidget*)wPaint;
	PangoLayout* playout = gtk_widget_create_pango_layout(area, NULL);
	GdkGC * GC = area->style->fg_gc[area->state];
	int i,j,addr, w,h,x,y; u8 c;
	char txt[80],*ptxt;
	char words[4][13];

	initialize();
	gdk_draw_rectangle(area->window, area->style->white_gc, TRUE, 0, 0,
		area->allocation.width, area->allocation.height);

	char * def_patt = "<tt>0000:0000 | 00 11 22 33 44 55 66 77 88 99 AA BB CC DD EE FF | 0123456789ABCDEF</tt>";
	pango_layout_set_markup(playout, def_patt , -1);
	pango_layout_get_pixel_size(playout, &w, &h);
	gtk_widget_set_usize(area,w+20, (10*h)+10);

// draw memory content here
	addr=address;
	for (i=0,x=10,y=5; i<10; i++,y+=h) {
		ptxt = txt;
		sprintf(ptxt, "%04X:%04X | ", (addr>>16), (addr&0xFFFF)); ptxt+=12;
		switch(packmode) {
		case Bit8:
			for (j=0; j<16; j++,ptxt+=3)
				sprintf(ptxt, "%02X ", MMU_readByte(cpu, addr+j));
			break;
		case Bit16:
			for (j=0; j<16; j+=2,ptxt+=6)
				sprintf(ptxt, " %04X ", MMU_readHWord(cpu, addr+j));
			break;
		case Bit32:
			for (j=0; j<16; j+=4,ptxt+=12)
				sprintf(ptxt, "  %08X  ", (int)MMU_readWord(cpu, addr+j));
			break;
		}
		sprintf(ptxt, "| "); ptxt +=2;
		for (j=0; j<16; j++,ptxt++)
			sprintf(ptxt, "%c", ((c=MMU_readByte(cpu, addr+j))<32)?'.':((c&0x80)?',':c));
		addr += 16;
		*(ptxt)=0;

		pango_layout_set_text(playout, txt, -1);
		pango_layout_set_attributes(playout,attr_Text);
		gdk_draw_layout(area->window, GC, x, y, playout);
	}
	
// done
	g_object_unref(playout);
}
