/* callbacks_1_ioregs.cpp - this file is part of DeSmuME
 *
 * Copyright (C) 2007 Damien Nozay (damdoum)
 * Copyright (C) 2007 Pascal Giard (evilynux)
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

/* ***** ***** IO REGISTERS ***** ***** */
static int cpu=0;
static BOOL init=FALSE;
static int size_=0;
static dTools_dsp dsp;

static void update_regs_fast();
static void update_regs();

/* Register name list */
#define NBR_IO_REGS 7

typedef struct
{
  char name[20];
  u32 addr;
  BOOL trunc;
} reg_name_addr;

const reg_name_addr Reg_Names_Addr[NBR_IO_REGS] =
  {
    { "REG_IPCFIFOCNT", REG_IPCFIFOCNT, TRUE },
    { "REG_SPICNT", REG_SPICNT, TRUE },
    { "REG_IME", REG_IME, TRUE },
    { "REG_IE", REG_IE, FALSE },
    { "REG_IF", REG_IF, FALSE },
    { "REG_POWCNT1", REG_POWCNT1, TRUE },
	 { "REG_DISPCAPCNT", REG_DISPA_DISPCAPCNT, FALSE }
  };

/* update */

static void wtools_1_update () {
	update_regs_fast();
}



/* Update register display */

static u32 mem[NBR_IO_REGS];

static void update_regs_fast(){
	char text[11];
	int i; u32 w, m;
	for( i = 0; i < NBR_IO_REGS; i++ )
	{	
		w = MMU_read32(cpu,Reg_Names_Addr[i].addr);
		m = mem[i];
		if ( Reg_Names_Addr[i].trunc ) {
			w &= 0xFFFF;
			m &= 0xFFFF;
		}
		mem[i] = w;
		if (w == m) continue;

		if ( Reg_Names_Addr[i].trunc )
			sprintf(text, "    0x%04X", w);
		else
			sprintf(text, "0x%08X", w);

		dTools_display_select_attr(&dsp, 2);
		dTools_display_clear_char(&dsp, size_+3, i, 10);
		dTools_display_draw_text(&dsp, size_+3, i, text);
	}
}

static void update_regs()
{
	char text[80];
	int len, i;

	if (init==FALSE) {
		GtkWidget * wPaint = glade_xml_get_widget(xml_tools, "wtools_1_draw");

		for( i = 0; i < NBR_IO_REGS; i++ ) {
			len = strlen(Reg_Names_Addr[i].name);
			if (size_<len) size_=len;
		}

		len = size_ + strlen(" : 0x00000000");
		dTools_display_init(&dsp, wPaint, len, NBR_IO_REGS, 5);
		dTools_display_add_markup(&dsp, "<tt><span foreground=\"blue\">                    </span></tt>");
		dTools_display_add_markup(&dsp, "<tt>0x00000000</tt>");
		dTools_display_add_markup(&dsp, "<tt><span foreground=\"red\">0x00000000</span></tt>");
		init=TRUE;
	}

	dTools_display_clear(&dsp);
	for( i = 0; i < NBR_IO_REGS; i++ )
	{
		mem[i] = MMU_read32(cpu,Reg_Names_Addr[i].addr);
		if  ( Reg_Names_Addr[i].trunc )
			sprintf(text, "    0x%04X", mem[i]);
		else
			sprintf(text, "0x%08X", mem[i]);

		dTools_display_select_attr(&dsp, 0);
		dTools_display_draw_text(&dsp, 0, i, Reg_Names_Addr[i].name);
		dTools_display_draw_text(&dsp, size_, i, " : ");
		dTools_display_select_attr(&dsp, 1);
		dTools_display_draw_text(&dsp, size_+3, i, text);
	}
}

gboolean on_wtools_1_draw_button_release_event(GtkWidget *widget, GdkEventButton *event, gpointer user_data) {
	// clear the red marks :)
	if (event->button==1)
		update_regs();
	return TRUE;
}
gboolean on_wtools_1_draw_expose_event (GtkWidget *widget, GdkEventExpose *event, gpointer user_data) {
	update_regs();
	return TRUE; 
}


void on_wtools_1_combo_cpu_changed    (GtkComboBox *widget, gpointer user_data) {
  /* c == 0 means ARM9 */
  cpu=gtk_combo_box_get_active(widget);
  update_regs();
}

/* show, register, unregister */
void on_wtools_1_IOregs_show          (GtkWidget *widget, gpointer user_data) {
	GtkWidget * combo = glade_xml_get_widget(xml_tools, "wtools_1_combo_cpu");

	// do as if we had selected this button and ARM7 cpu
	gtk_combo_box_set_active((GtkComboBox*)combo, 0);
	register_Tool(wtools_1_update);
}

gboolean on_wtools_1_IOregs_close (GtkWidget *widget, ...) {
	unregister_Tool(wtools_1_update);
	gtk_widget_hide(widget);
	return TRUE;
}

