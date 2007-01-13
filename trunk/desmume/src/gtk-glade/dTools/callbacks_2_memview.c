/* callbacks_2_memview.c - this file is part of DeSmuME
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
static PangoAttrList *attr_Text8,*attr_Text16,*attr_Text32;
static char patt[512];

static GtkEntry       *wAddress;
static GtkDrawingArea *wPaint;
static GtkRange       *wRange;

void refresh();
void initialise();



/* which cpu we look into */

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
#define RANGE_MIN   0
#define RANGE_MAX   0x100000000
#define ADDR_MASK   0xFFFFFFF0
#define STEP_ONE_LINE 0x10
#define STEP_ONE_PAGE 0x100
#define STEP_x10_PAGE 0x1000

void scroll_address(u32 addr) {
	address = addr & ADDR_MASK;
	refresh();
}
void change_address(u32 addr) {
	gtk_range_set_value(wRange, addr);
}
void add_to_address(u32 inc) {
	change_address(address+inc);
}

void on_wtools_2_GotoAddress_activate  (GtkEntry *entry, gpointer user_data) {
	change_address(strtol(gtk_entry_get_text(entry),NULL,0));
}
void on_wtools_2_GotoAddress_changed   (GtkEntry *entry, gpointer user_data) {
	tmpaddr=strtol(gtk_entry_get_text(entry),NULL,0);
}
void on_wtools_2_GotoButton_clicked    (GtkButton *button, gpointer user_data) {
	change_address(tmpaddr);
}



void on_wtools_2_MemView_show          (GtkWidget *widget, gpointer user_data) {
	initialize();
	change_address(RANGE_MIN);
}



/* scroll functions :D */

void on_wtools_2_scroll_value_changed  (GtkRange *range, gpointer user_data) {
	u32 addr=(u32)gtk_range_get_value(range);
	scroll_address(addr);
}
gboolean on_wtools_2_draw_scroll_event (GtkWidget *widget, GdkEventScroll *event, gpointer user_data) {
	switch (event->direction) {
	case GDK_SCROLL_UP:
		add_to_address(-STEP_ONE_PAGE); break;
	case GDK_SCROLL_DOWN:
		add_to_address(+STEP_ONE_PAGE); break;
	default:
		break;
	}
	return TRUE;
}
gboolean on_wtools_2_draw_expose_event (GtkWidget *widget, GdkEventExpose *event, gpointer user_data) { refresh(); return TRUE; }





/* initialise what we have to */

void initialize() {
	GtkWidget * combo;
	GtkAdjustment *adj;
	int i,j;

	if (init) return;
	combo = glade_xml_get_widget(xml_tools, "wtools_2_cpu");
	adj = (GtkAdjustment *)gtk_adjustment_new (RANGE_MIN, RANGE_MIN, RANGE_MAX,
		STEP_ONE_LINE, STEP_x10_PAGE, STEP_ONE_PAGE);

	// get widget reference
	wAddress = (GtkEntry*)glade_xml_get_widget(xml_tools, "wtools_2_GotoAddress");
	wPaint   = (GtkDrawingArea*)glade_xml_get_widget(xml_tools, "wtools_2_draw");
	wRange   = (GtkRange*)glade_xml_get_widget(xml_tools, "wtools_2_scroll");

	gtk_combo_box_set_active((GtkComboBox*)combo, 0);
	gtk_range_set_adjustment(wRange, adj);

#define PATT(x) x"<span foreground=\"#608060\">" x "</span>"
#define DUP(x) x x
	strcpy(patt,  "<tt><span foreground=\"blue\">0000:0000</span> | ");
	strcat(patt, DUP(DUP(DUP(PATT("00_")))) );
	strcat(patt, "| 0123456789ABCDEF</tt>");
	pango_parse_markup(patt,-1,0,&attr_Text8,NULL,NULL,NULL);

	strcpy(patt,  "<tt><span foreground=\"blue\">0000:0000</span> | ");
	strcat(patt, DUP(DUP(PATT("_0000_"))) );
	strcat(patt, "| 0123456789ABCDEF</tt>");
	pango_parse_markup(patt,-1,0,&attr_Text16,NULL,NULL,NULL);

	strcpy(patt,  "<tt><span foreground=\"blue\">0000:0000</span> | ");
	strcat(patt, DUP(PATT("__00000000__")) );
	strcat(patt, "| 0123456789ABCDEF</tt>");
	pango_parse_markup(patt,-1,0,&attr_Text32,NULL,NULL,NULL);
#undef DUP
#undef PATT
	init = TRUE;
}

/* PAINT memory panel */
void refresh() {
	GtkWidget * area = (GtkWidget*)wPaint;
	PangoLayout* playout = gtk_widget_create_pango_layout(area, NULL);
	GdkGC * GC = area->style->fg_gc[area->state];
	int i,j,addr, w,h,x,y; u8 c;
	char *ptxt, txt[]="0000:0000 | 00 11 22 33 44 55 66 77 88 99 AA BB CC DD EE FF | 0123456789ABCDEF\0";
	PangoAttrList *attr;
	
	switch(packmode){
	default:
	case Bit8:	attr=attr_Text8;  break;
	case Bit16:	attr=attr_Text16; break;
	case Bit32:	attr=attr_Text32; break;
	}

	gdk_draw_rectangle(area->window, area->style->white_gc, TRUE, 0, 0,
		area->allocation.width, area->allocation.height);

	pango_layout_set_text(playout, txt, -1);
	pango_layout_set_attributes(playout, attr);
	pango_layout_get_pixel_size(playout, &w, &h);
	gtk_widget_set_usize(area,w+20, (0x10*h)+10);

// draw memory content here
	addr=address;
	for (i=0,x=10,y=5; i<0x10; i++,y+=h) {
		ptxt = txt;
		sprintf(ptxt, "%04X:%04X | ", (addr>>16)&0xFFFF, addr&0xFFFF); ptxt+=12;
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
			sprintf(ptxt, "%c", ((c=MMU_readByte(cpu, addr+j))<0x20)?'.':((c>=0x7F)?'.':c)); // only ASCII printable
		addr += 16;
		*(ptxt)=0;

		pango_layout_set_text(playout, txt, -1);
		pango_layout_set_attributes(playout, attr);
		gdk_draw_layout(area->window, GC, x, y, playout);
	}
	
// done
	g_object_unref(playout);
}
