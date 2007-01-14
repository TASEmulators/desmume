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

static void wtools_3_update() {
	refresh();
}


static void refresh() {
	int x,y,X,Y; u16 * addr = base_addr[palindex];
	COLOR c; COLOR32 c32;

	GdkGC * GC = gdk_gc_new(wPaint->window);

	for(y=Y= 0; y < 16; y++,Y+=16)
	for(x=X= 0; x < 16; x++,X+=16)
	{
		if (addr) {
			c.val = *(addr+Y+x+0x100*palnum);
			COLOR_16_32(c,c32)
			gdk_rgb_gc_set_foreground(GC, c32.val);
			gdk_draw_rectangle(wPaint->window, GC, TRUE, X, Y, 15, 15);
		} else {
			gdk_rgb_gc_set_foreground(GC, 0x808080);
			gdk_draw_rectangle(wPaint->window, GC, TRUE, X, Y, 15, 15);
			gdk_rgb_gc_set_foreground(GC, 0xFF0000);
			gdk_draw_line(wPaint->window, GC, X+14, Y+1, X+1, Y+14);
			gdk_draw_line(wPaint->window, GC, X+1, Y+1, X+14, Y+14);
		}
		
		
	}
	g_object_unref(GC);
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
}


void     on_wtools_3_PalView_show         (GtkWidget *widget, gpointer data) {
	initialize();
}
gboolean on_wtools_3_PalView_close         (GtkWidget *widget, ...) {
//	unregister_Tool(wtools_1_update);
	gtk_widget_hide(widget);
	return TRUE;
}


gboolean on_wtools_3_draw_expose_event    (GtkWidget * widget, GdkEventExpose *event, gpointer user_data) {
	refresh();
}
void     on_wtools_3_palette_changed      (GtkComboBox *combo,   gpointer user_data) {
	palindex = gtk_combo_box_get_active(combo);
	refresh();
}
void     on_wtools_3_palnum_value_changed (GtkSpinButton *spin, gpointer user_data) {
}

