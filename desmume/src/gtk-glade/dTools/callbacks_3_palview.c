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
static void initialize() {
	GtkComboBox * combo;
	int i=0;
	if (init) return;

	combo = (GtkComboBox*)glade_xml_get_widget(xml_tools, "wtools_3_palette");
#define DO(str,addr)  gtk_combo_box_append_text(combo,str); base_addr[i]=(u16*)(addr); i++;
	DO("Main screen BG  PAL", ARM9Mem.ARM9_VMEM)
	DO("Main screen SPR PAL", ARM9Mem.ARM9_VMEM + 0x100)
	DO("Sub  screen BG  PAL", ARM9Mem.ARM9_VMEM + 0x200)
	DO("Sub  screen SPR PAL", ARM9Mem.ARM9_VMEM + 0x300)
	DO("Main screen ExtPAL 0", ARM9Mem.ExtPal[0][0])
	DO("Main screen ExtPAL 1", ARM9Mem.ExtPal[0][1])
	DO("Main screen ExtPAL 2", ARM9Mem.ExtPal[0][2])
	DO("Main screen ExtPAL 3", ARM9Mem.ExtPal[0][3])
	DO("Sub  screen ExtPAL 0", ARM9Mem.ExtPal[1][0])
	DO("Sub  screen ExtPAL 1", ARM9Mem.ExtPal[1][1])
	DO("Sub  screen ExtPAL 2", ARM9Mem.ExtPal[1][2])
	DO("Sub  screen ExtPAL 3", ARM9Mem.ExtPal[1][3])
	DO("Main screen SPR ExtPAL 0", ARM9Mem.ObjExtPal[0][0])
	DO("Main screen SPR ExtPAL 1", ARM9Mem.ObjExtPal[0][1])
	DO("Sub  screen SPR ExtPAL 0", ARM9Mem.ObjExtPal[1][0])
	DO("Sub  screen SPR ExtPAL 1", ARM9Mem.ObjExtPal[1][1])
	DO("Texture PAL 0", ARM9Mem.texPalSlot[0])
	DO("Texture PAL 1", ARM9Mem.texPalSlot[1])
	DO("Texture PAL 2", ARM9Mem.texPalSlot[2])
	DO("Texture PAL 3", ARM9Mem.texPalSlot[3])
#undef DO
	init=TRUE;
}

#if 0
             for(y = 0; y < 16; ++y)
             {
                  for(x = 0; x < 16; ++x)
                  {
                       c = adr[(y<<4)+x+0x100*num];
                       brush = CreateSolidBrush(RGB((c&0x1F)<<3, (c&0x3E0)>>2, (c&0x7C00)>>7)
                  }
             }

#endif


void     on_wtools_3_PalView_show         (GtkWidget *widget, gpointer data) {
	initialize();
}
gboolean on_wtools_3_PalView_close         (GtkWidget *widget, ...) {
//	unregister_Tool(wtools_1_update);
	gtk_widget_hide(widget);
	return TRUE;
}


gboolean on_wtools_3_draw_expose_event    (GtkWidget *, GdkEventExpose *, gpointer );
void     on_wtools_3_palette_changed      (GtkComboBox *,   gpointer );
void     on_wtools_3_palnum_value_changed (GtkSpinButton *, gpointer );

