/* callbacks_1_ioregs.c - this file is part of DeSmuME
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

/* ***** ***** IO REGISTERS ***** ***** */
static int cpu=0;
static BOOL autorefresh;

static void update_regs();

/* Register name list */
#define NBR_IO_REGS 6

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
    { "REG_POWCNT1", REG_POWCNT1, TRUE }
  };

/* update */

static void wtools_1_update () {
  if(autorefresh) update_regs();
}

/* Update register display */
static void update_regs()
{
  char lbl_name[40];
  char lbl_text[40];
  char * mask;
  GtkWidget * lbl;
  int i;
  u32 w;

  for( i = 0; i < NBR_IO_REGS; i++ )
    {
      mask = ( Reg_Names_Addr[i].trunc ) ? "0x%04x" : "0x%08x";
      sprintf(lbl_name,"wtools_1_%s_value\0\0", Reg_Names_Addr[i].name);
      w = MMU_read32(cpu,Reg_Names_Addr[i].addr);
      sprintf(lbl_text, mask, w);
      lbl = glade_xml_get_widget(xml_tools, lbl_name);
      gtk_label_set_text((GtkLabel *)lbl, lbl_text);
    }
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

void on_wtools_1_autorefresh_toggled (GtkToggleButton *tb, gpointer user_data) 
{
  GtkWidget * b = glade_xml_get_widget(xml_tools, "wtools_1_refresh");
  if( gtk_toggle_button_get_active(tb) == TRUE )
    {
      autorefresh = TRUE;
      gtk_widget_set_sensitive( b, FALSE );
    }
  else
    {
      autorefresh = FALSE;
      gtk_widget_set_sensitive( b, TRUE );
    }
}

void on_wtools_1_refresh_clicked (GtkButton *b, gpointer user_data)
{
  update_regs();
}
