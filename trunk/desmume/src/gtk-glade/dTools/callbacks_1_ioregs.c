#include "callbacks_dtools.h"

/* ***** ***** IO REGISTERS ***** ***** */

void on_wtools_1_IOregs_show          (GtkWidget *widget, gpointer user_data) {
	GtkWidget * b = glade_xml_get_widget(xml_tools, "wtools_1_r_ime");
	// do as if we had selected this button
	gtk_toggle_button_set_active((GtkToggleButton*)b, TRUE);
}

/* c == 0 (ARM9) */
static u32 val_REG_IME(int c) { return MMU.reg_IME[c]; }
static u32 val_REG_IE(int c) { return MMU.reg_IE[c]; }
static u32 val_REG_IF(int c) { return MMU.reg_IF[c]; }
static u32 val_REG_IPCFIFOCNT(int c) { return ((u16 *)(MMU.MMU_MEM[c][0x40]))[0x184>>1]; }
static u32 val_POWER_CR(int c) { return ((u16 *)(MMU.MMU_MEM[c][0x40]))[0x304>>1]; }
static u32 val_REG_SPICNT(int c) { return ((u16 *)(MMU.MMU_MEM[c][0x40]))[0x1C0>>1]; }

void on_wtools_1_r_ipcfifocnt_toggled (GtkToggleButton *togglebutton, gpointer user_data);
void on_wtools_1_r_spicnt_toggled     (GtkToggleButton *togglebutton, gpointer user_data);
void on_wtools_1_r_ime_toggled        (GtkToggleButton *togglebutton, gpointer user_data) { printf("hello\n");}
void on_wtools_1_r_ie_toggled         (GtkToggleButton *togglebutton, gpointer user_data);
void on_wtools_1_r_if_toggled         (GtkToggleButton *togglebutton, gpointer user_data);
void on_wtools_1_r_power_cr_toggled   (GtkToggleButton *togglebutton, gpointer user_data);

