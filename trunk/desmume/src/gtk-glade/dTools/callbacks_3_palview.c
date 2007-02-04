/* callbacks_3_palview.c - this file is part of DeSmuME
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

static u16* base_addr[20];
static BOOL init=FALSE;
static int palnum=0;
static int palindex=0;
static void refresh();
static GtkWidget * wPaint;
static GtkSpinButton * wSpin;
static u16 mem[0x100];


static COLOR c;
static COLOR32 c32;
static GdkGC * gdkGC;

static inline void paint_col(int x, int y, u16 col) {
	c.val = col;
	COLOR_16_32(c,c32)
	gdk_rgb_gc_set_foreground(gdkGC, c32.val);
	gdk_draw_rectangle(wPaint->window, gdkGC, TRUE, x, y, 15, 15);
}
static inline void paint_cross(int x, int y) {
	gdk_rgb_gc_set_foreground(gdkGC, 0x808080);
	gdk_draw_rectangle(wPaint->window, gdkGC, TRUE, x, y, 15, 15);
	gdk_rgb_gc_set_foreground(gdkGC, 0xFF0000);
	gdk_draw_line(wPaint->window, gdkGC, x+14, y+1, x+1, y+14);
	gdk_draw_line(wPaint->window, gdkGC, x+1, y+1, x+14, y+14);
}


static void wtools_3_update() {
	int i,x,y,X,Y; 
	u16 * addr = base_addr[palindex], tmp;

	gdkGC = gdk_gc_new(wPaint->window);
	if (addr) {
		memcpy(mem, addr, 0x100*sizeof(u16));
		i=0;
		for(y=Y= 0; y < 16; y++,Y+=16)
		for(x=X= 0; x < 16; x++,X+=16) {
			tmp=mem[i];
			if (tmp != (mem[i]=*(addr+Y+x+0x100*palnum)))
			paint_col(X,Y,mem[i]);
		}
	} else {
		for(y=Y= 0; y < 16; y++,Y+=16)
		for(x=X= 0; x < 16; x++,X+=16)
		paint_cross(X,Y);
	}
	g_object_unref(gdkGC);
}


static void refresh() {
	int x,y,X,Y; u16 * addr = base_addr[palindex];
	COLOR c; COLOR32 c32;

	gdkGC = gdk_gc_new(wPaint->window);
	if (addr) {
		memcpy(mem, addr, 0x100*sizeof(u16));
		for(y=Y= 0; y < 16; y++,Y+=16)
		for(x=X= 0; x < 16; x++,X+=16)
		paint_col(X,Y,*(addr+Y+x+0x100*palnum));
	} else {
		for(y=Y= 0; y < 16; y++,Y+=16)
		for(x=X= 0; x < 16; x++,X+=16)
		paint_cross(X,Y);
	}
	g_object_unref(gdkGC);
}


static void initialize() {
	GtkComboBox * combo;
	GtkTreeIter iter, *parent=NULL;
	GtkListStore* model;
	int i=0;
	if (init) return;

	combo = (GtkComboBox*)glade_xml_get_widget(xml_tools, "wtools_3_palette");
	model = (GtkListStore*)gtk_combo_box_get_model(combo);
	wPaint= glade_xml_get_widget(xml_tools, "wtools_3_draw");
	wSpin = (GtkSpinButton*)glade_xml_get_widget(xml_tools, "wtools_3_palnum");

#define DO(str,addr,r)  \
	gtk_list_store_append (model, &iter); \
	gtk_list_store_set (model, &iter, 0, str,-1); \
	base_addr[i]=((u16*)(addr) r); i++;

	DO("Main screen BG  PAL", ARM9Mem.ARM9_VMEM,)
	DO("Main screen SPR PAL", ARM9Mem.ARM9_VMEM,+0x100)
	DO("Sub  screen BG  PAL", ARM9Mem.ARM9_VMEM,+0x200)
	DO("Sub  screen SPR PAL", ARM9Mem.ARM9_VMEM,+0x300)
	DO("Main screen ExtPAL 0", ARM9Mem.ExtPal[0][0],)
	DO("Main screen ExtPAL 1", ARM9Mem.ExtPal[0][1],)
	DO("Main screen ExtPAL 2", ARM9Mem.ExtPal[0][2],)
	DO("Main screen ExtPAL 3", ARM9Mem.ExtPal[0][3],)
	DO("Sub  screen ExtPAL 0", ARM9Mem.ExtPal[1][0],)
	DO("Sub  screen ExtPAL 1", ARM9Mem.ExtPal[1][1],)
	DO("Sub  screen ExtPAL 2", ARM9Mem.ExtPal[1][2],)
	DO("Sub  screen ExtPAL 3", ARM9Mem.ExtPal[1][3],)
	DO("Main screen SPR ExtPAL 0", ARM9Mem.ObjExtPal[0][0],)
	DO("Main screen SPR ExtPAL 1", ARM9Mem.ObjExtPal[0][1],)
	DO("Sub  screen SPR ExtPAL 0", ARM9Mem.ObjExtPal[1][0],)
	DO("Sub  screen SPR ExtPAL 1", ARM9Mem.ObjExtPal[1][1],)
	DO("Texture PAL 0", ARM9Mem.texPalSlot[0],)
	DO("Texture PAL 1", ARM9Mem.texPalSlot[1],)
	DO("Texture PAL 2", ARM9Mem.texPalSlot[2],)
	DO("Texture PAL 3", ARM9Mem.texPalSlot[3],)
#undef DO
	init=TRUE;
	gtk_combo_box_set_active(combo,0);
}


void     on_wtools_3_PalView_show         (GtkWidget *widget, gpointer data) {
	initialize();
	register_Tool(wtools_3_update);
}
gboolean on_wtools_3_PalView_close         (GtkWidget *widget, ...) {
	unregister_Tool(wtools_3_update);
	gtk_widget_hide(widget);
	return TRUE;
}


gboolean on_wtools_3_draw_expose_event    (GtkWidget * widget, GdkEventExpose *event, gpointer user_data) {
	refresh();
}
void     on_wtools_3_palette_changed      (GtkComboBox *combo,   gpointer user_data) {
	palindex = gtk_combo_box_get_active(combo);
	gtk_widget_set_sensitive((GtkWidget*)wSpin,(palindex >=4));
	gtk_spin_button_set_value(wSpin,0);
	refresh();
}
void     on_wtools_3_palnum_value_changed (GtkSpinButton *spin, gpointer user_data) {
	palnum = gtk_spin_button_get_value_as_int(spin);
	refresh();
}
