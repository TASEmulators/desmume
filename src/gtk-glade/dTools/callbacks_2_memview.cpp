/* callbacks_2_memview.cpp - this file is part of DeSmuME
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
#include "dTools_display.h"

/* ***** ***** MEMORY VIEWER ***** ***** */

enum SHOW {
	Bit8,
	Bit16,
	Bit32
};

static BOOL init=FALSE;
static enum SHOW packmode=Bit8;
static u32 address=0, tmpaddr=0;
int cpu=0;
static char patt[512];
static u8 mem[0x100];
static dTools_dsp dsp;

static GtkEntry       *wAddress;
static GtkRange       *wRange;

static void refresh();
static void initialize();


/* update */

static void wtools_2_update() {
	int i,j;
	u8  m8,  *mem8 =mem; 
	u16 m16, *mem16=(u16*)mem;
	u32 m32, *mem32=(u32*)mem;
	u32 addr;
	char txt[16];

	// red
	dTools_display_select_attr(&dsp, 3);

	addr = address;
	switch (packmode) {
	case Bit8:
		for (i=0; i<0x10; i++) {
			for (j=0; j<16; j++, addr++,mem8++) { 
				m8 = *mem8; *mem8 = MMU_read8(cpu, addr);
				if (m8 != *mem8) {
					sprintf(txt, "%02X", *mem8);
					dTools_display_clear_char(&dsp, 12+3*j, i, 3);
					dTools_display_draw_text(&dsp, 12+3*j, i, txt);
				}
			}
		}
		break;
	case Bit16:
		for (i=0; i<0x10; i++) {
			for (j=0; j<16; j+=2, addr+=2,mem16++) { 
				m16 = *mem16; *mem16 = MMU_read16(cpu, addr);
				if (m16 != *mem16) {
					sprintf(txt, " %04X", *mem16);
					dTools_display_clear_char(&dsp, 12+3*j, i, 6);
					dTools_display_draw_text(&dsp, 12+3*j, i, txt);
				}
			}
		}
		break;
	case Bit32:
		for (i=0; i<0x10; i++) {
			for (j=0; j<16; j+=4, addr+=4,mem32++) { 
				m32 = *mem32; *mem32 = MMU_read32(cpu, addr);
				if (m32 != *mem32) {
					sprintf(txt, "  %08X", *mem32);
					dTools_display_clear_char(&dsp, 12+3*j, i, 12);
					dTools_display_draw_text(&dsp, 12+3*j, i, txt);
				}
			}
		}
		break;
	}
}

gboolean on_wtools_2_draw_button_release_event(GtkWidget *widget, GdkEventButton *event, gpointer user_data) {
	// clear the red marks :)
	if (event->button==1)
		refresh();

	return TRUE;
}




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
#define RANGE_MAX   0x10000000
#define ADDR_MASK   0xFFFFFFF
#define STEP_ONE_LINE 0x1
#define STEP_ONE_PAGE 0x10
#define STEP_x10_PAGE 0x100

static void scroll_address(u32 addr) {
	address = (addr & ADDR_MASK);
	refresh();
}
static void change_address(u32 addr) {
	addr /= 0x10;
	gtk_range_set_value(wRange, addr);
}
static void add_to_address(s32 inc) {
	u32 addr = (address+inc) & ADDR_MASK;
	gtk_range_set_value(wRange, addr);
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


/* show, register, unregister */

void on_wtools_2_MemView_show       (GtkWidget *widget, gpointer user_data) {
	initialize();
	register_Tool(wtools_2_update);
}
gboolean on_wtools_2_MemView_close  (GtkWidget *widget, ...) {
	unregister_Tool(wtools_2_update);
	dTools_display_free(&dsp);
	gtk_widget_hide(widget);
	return TRUE;
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
	case GDK_SCROLL_LEFT:
	case GDK_SCROLL_RIGHT:
	default:
		break;
	}
	return TRUE;
}
gboolean on_wtools_2_draw_expose_event (GtkWidget *widget, GdkEventExpose *event, gpointer user_data) { refresh(); return TRUE; }





/* initialise what we have to */

static void initialize() {
	GtkWidget * combo;
	GtkWidget * wPaint;
	GtkAdjustment *adj;

	if (init) return;
	combo = glade_xml_get_widget(xml_tools, "wtools_2_cpu");
	adj = (GtkAdjustment *)gtk_adjustment_new (RANGE_MIN, RANGE_MIN, RANGE_MAX,
		STEP_ONE_LINE, STEP_x10_PAGE, STEP_ONE_PAGE);

	// get widget reference
	wAddress = (GtkEntry*)glade_xml_get_widget(xml_tools, "wtools_2_GotoAddress");
	wRange   = (GtkRange*)glade_xml_get_widget(xml_tools, "wtools_2_scroll");
	wPaint   = glade_xml_get_widget(xml_tools, "wtools_2_draw");

	dTools_display_init(&dsp, wPaint, 80, 16, 5);

#define PATT(x) x"<span foreground=\"#608060\">" x "</span>"
#define DUP(x) x x
	sprintf(patt, "<tt><span foreground=\"blue\">0000:0000</span> | %s| 0123456789ABCDEF</tt>", DUP(DUP(DUP(PATT("00_")))) );
	dTools_display_add_markup(&dsp, patt);
	sprintf(patt, "<tt><span foreground=\"blue\">0000:0000</span> | %s| 0123456789ABCDEF</tt>", DUP(DUP(PATT("_0000_"))) );
	dTools_display_add_markup(&dsp, patt);
	sprintf(patt, "<tt><span foreground=\"blue\">0000:0000</span> | %s| 0123456789ABCDEF</tt>", DUP(PATT("__00000000__")) );
	dTools_display_add_markup(&dsp, patt);
#undef DUP
#undef PATT
	strcpy(patt, "<tt><span foreground=\"red\">__00000000__</span></tt>");
	dTools_display_add_markup(&dsp, patt);

	init = TRUE;
	gtk_combo_box_set_active((GtkComboBox*)combo, 0);
	gtk_range_set_adjustment(wRange, adj);
	change_address(RANGE_MIN);
}

/* PAINT memory panel */
static void refresh() {
	int i,j,addr;
	u8 c;
	u8 *mem8=mem;
	u16 *mem16=(u16*)mem;
	u32 *mem32=(u32*)mem;

	char *ptxt, txt[]="0000:0000 | 00 11 22 33 44 55 66 77 88 99 AA BB CC DD EE FF | 0123456789ABCDEF\0";
	
	if (!init) return;

	addr=address * 0x10;
	for (i=0; i<0x100; i++) 
		mem[i] = MMU_read8(cpu, addr+i);

	dTools_display_clear(&dsp);
	switch(packmode) {
	case Bit8:  dTools_display_select_attr(&dsp, 0); break;
	case Bit16: dTools_display_select_attr(&dsp, 1); break;
	case Bit32: dTools_display_select_attr(&dsp, 2); break;
	}


// draw memory content here
	for (i=0; i<0x10; i++) {
		ptxt = txt;
		sprintf(ptxt, "%04X:%04X | ", (addr>>16)&0xFFFF, addr&0xFFFF); ptxt+=12;
		switch(packmode) {
		case Bit8:
			for (j=0; j<16; j++,ptxt+=3)
				sprintf(ptxt, "%02X ", mem8[j]);
			break;
		case Bit16:
			for (j=0; j<16; j+=2,ptxt+=6, mem16++)
				sprintf(ptxt, " %04X ", *mem16);
			break;
		case Bit32:
			for (j=0; j<16; j+=4,ptxt+=12, mem32++)
				sprintf(ptxt, "  %08X  ", *mem32);
			break;
		}
		sprintf(ptxt, "| "); ptxt +=2;
		for (j=0; j<16; j++,ptxt++)
			// only ASCII printable
			sprintf(ptxt, "%c", ((c=mem8[j])<0x20)?'.':((c>=0x7F)?'.':c));
		addr += 16;
		mem8 +=16;
		*(ptxt)=0;

		dTools_display_draw_text(&dsp, 0, i, txt);
	}
}
