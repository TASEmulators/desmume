/* ioregsView.cpp - this file is part of DeSmuME
 *
 * Copyright (C) 2006 Thoduv
 * Copyright (C) 2006,2007 DeSmuME Team
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

#include <gtk/gtk.h>
#include <string.h>
#include "../dTool.h"

#include "../MMU.h"

#undef GPOINTER_TO_INT
#define GPOINTER_TO_INT(p) ((gint)  (glong) (p))

#define SHORTNAME "ioregs"
#define TOOL_NAME "IO regs view"

#if !GTK_CHECK_VERSION(2,24,0)
#define gtk_combo_box_text_new gtk_combo_box_new_text
#define gtk_combo_box_text_append_text gtk_combo_box_append_text
#define GTK_COMBO_BOX_TEXT GTK_COMBO_BOX
#endif

BOOL CPUS [2] = {TRUE, TRUE};

static GtkWidget *mWin[2];
static GtkWidget *mVbox0[2];
static GtkWidget *mIoRegCombo[2];
static GtkWidget *mRegInfos[2];

typedef void (*reg_dispFn)(int c);
typedef u32 (*reg_valFn)(int c);

typedef struct
{
	char name[64];
	u32 adress;
	int size;
	reg_dispFn create;
	reg_dispFn update;
	reg_dispFn destroy;
	reg_valFn value;
} reg_t;

static reg_t *current_reg[2] = {NULL, NULL};

#define REGFN_BEGIN(reg) \
	GtkWidget **_wl_ = Widgets_##reg [c];

#define BIT_CHECK(w, n, s) { \
	char _bit_check_buf[64]; \
	snprintf(_bit_check_buf, ARRAY_SIZE(_bit_check_buf), "Bit %d: %s", n,s); \
	_wl_[w] = gtk_check_button_new_with_label(_bit_check_buf ); \
	gtk_box_pack_start(GTK_BOX(mVbox0[c]), _wl_[w], FALSE, FALSE, 0); }

#define BIT_COMBO(w,n,s) { \
	_wl_[w] = gtk_hbox_new(FALSE, 0); \
	gtk_box_pack_start(GTK_BOX(mVbox0[c]), _wl_[w], FALSE, FALSE, 0); } \
	char _bit_combo_buf[64]; \
	snprintf(_bit_combo_buf, ARRAY_SIZE(_bit_combo_buf), "Bits %s: %s", n,s); \
	GtkWidget *__combo_lbl_tmp = gtk_label_new(_bit_combo_buf); \
	GtkWidget *__combo_tmp = gtk_combo_box_text_new(); \
	
#define BIT_COMBO_ADD(w, s) { \
	gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(__combo_tmp), s); }
	
#define BIT_COMBO_GET(w) (GTK_WIDGET(g_list_first(gtk_container_get_children(GTK_CONTAINER(_wl_[w])))->data))
	
#define BIT_COMBO_END(w) \
	gtk_box_pack_start(GTK_BOX(_wl_[w]), __combo_tmp, FALSE, FALSE, 0); \
	gtk_box_pack_start(GTK_BOX(_wl_[w]), __combo_lbl_tmp, FALSE, FALSE, 0);

#define CREA_END() \
	gtk_widget_show_all(mWin[c]);

/////////////////////////////// REG_IME ///////////////////////////////
static GtkWidget *Widgets_REG_IME[2][1];
static void crea_REG_IME(int c)
{
	REGFN_BEGIN(REG_IME);
	BIT_CHECK(0, 0, "Master interrupt enable");
	CREA_END();
}
static void updt_REG_IME(int c) { gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(Widgets_REG_IME[c][0]), MMU.reg_IME[c] ? 1 : 0); }
static void dest_REG_IME(int c) { gtk_widget_destroy(Widgets_REG_IME[c][0]); }
static u32 val_REG_IME(int c) { return MMU.reg_IME[c]; }

/////////////////////////////// REG_IE ///////////////////////////////
  
static const char *interrupt_strings[25] = 
{
	"LCD VBlank",		// 0
	"LCD HBlank",		// 1
	"LCD VCount",		// 2
	"Timer0 overflow",		// 3
	"Timer1 overflow",	
	"Timer2 overflow",
	"Timer3 overflow",
	"Serial communication (RTC)",		// 7
	"DMA0",		// 8
	"DMA1",
	"DMA2",
	"DMA3",
	"Keypad",	// 12
	"Game Pak (GBA slot)",	// 13
	"",	// 14
	"",	 // 15
	"IPC Sync",		// 16
	"IPC Send FIFO empty",		// 17
	"IPC Recv FIFO not empty",		// 18
	"Card Data Transfer Completion (DS-card slot)",		// 19
	"Card IREQ_MC (DS-card slot)",		// 20
	"Geometry (3D) command FIFO",		// 21
	"Screens unfolding",		// 22
	"SPI bus",	// 23
	"Wifi"	// 24
};
#define INTERRUPT_SKIP(c) if(i == 14 || i == 15 || (c == 0 && (i == 7 || i == 22 || i == 23 || i == 24)) || (c == 1 && i == 21))continue;

static GtkWidget *Widgets_REG_IE[2][32];
static void crea_REG_IE(int c)
{
	REGFN_BEGIN(REG_IE);
	int i;
	for(i = 0; i < 24; i++) { INTERRUPT_SKIP(c); BIT_CHECK(i, i, interrupt_strings[i]); }
	CREA_END();
}
static void updt_REG_IE(int c)
{
	int i;
	for(i = 0; i < 24; i++) { INTERRUPT_SKIP(c); gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(Widgets_REG_IE[c][i]), (MMU.reg_IE[c] & (1<<i)) ? 1 : 0); }
}
static void dest_REG_IE(int c)
{
	int i;
	for(i = 0; i < 24; i++) { INTERRUPT_SKIP(c); gtk_widget_destroy(Widgets_REG_IE[c][i]); }
}
static u32 val_REG_IE(int c) { return MMU.reg_IE[c]; }

/////////////////////////////// REG_IF ///////////////////////////////
static GtkWidget *Widgets_REG_IF[2][32];
static void crea_REG_IF(int c)
{
	REGFN_BEGIN(REG_IF);
	int i;
	for(i = 0; i < 24; i++) { INTERRUPT_SKIP(c); BIT_CHECK(i, i, interrupt_strings[i]); }
	CREA_END();
}
static void updt_REG_IF(int c)
{
	int i;
	for(i = 0; i < 24; i++) { INTERRUPT_SKIP(c); gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(Widgets_REG_IF[c][i]), ((c==0?MMU.gen_IF<0>():MMU.gen_IF<1>()) & (1<<i)) ? 1 : 0); }
}
static void dest_REG_IF(int c)
{
	int i;
	for(i = 0; i < 24; i++) { INTERRUPT_SKIP(c); gtk_widget_destroy(Widgets_REG_IF[c][i]); }
}
static u32 val_REG_IF(int c) { return (c==0?MMU.gen_IF<0>():MMU.gen_IF<1>()); }

/////////////////////////////// REG_IPCFIFOCNT ///////////////////////////////
static const char *fifocnt_strings[] =
{
	"Send FIFO empty",
	"Send FIFO full",
	"Send FIFO empty IRQ",
	"Send FIFO clear (flush)",
	"","","","",
	"Receive FIFO empty",
	"Receive FIFO full",
	"Receive FIFO not empty IRQ",
	"","","",
	"Error (read when empty/write when full)",
	"Enable FIFOs"
};
#define FIFOCNT_SKIP(c) if(i==4||i==5||i==6||i==11||i==12||i==13)continue;

static GtkWidget *Widgets_REG_IPCFIFOCNT[2][16];
static void crea_REG_IPCFIFOCNT(int c)
{
	REGFN_BEGIN(REG_IPCFIFOCNT);
	int i;
	for(i = 0; i < 16; i++) { FIFOCNT_SKIP(c); BIT_CHECK(i, i, fifocnt_strings[i]); }
	CREA_END();
}
static void updt_REG_IPCFIFOCNT(int c)
{
	int i;
	for(i = 0; i < 16; i++) { FIFOCNT_SKIP(c); gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(Widgets_REG_IPCFIFOCNT[c][i]), (((u16 *)(MMU.MMU_MEM[c][0x40]))[0x184>>1]&(1<<i)) ? 1 : 0); }
}
static void dest_REG_IPCFIFOCNT(int c)
{
	int i;
	for(i = 0; i < 16; i++) { FIFOCNT_SKIP(c); gtk_widget_destroy(Widgets_REG_IPCFIFOCNT[c][i]); }
}
static u32 val_REG_IPCFIFOCNT(int c) { return ((u16 *)(MMU.MMU_MEM[c][0x40]))[0x184>>1]; }

/////////////////////////////// POWER_CR ///////////////////////////////
static const char *powercr9_strings[] =
{
	"Enable LCD",
	"2D A engine",
	"3D render engine",
	"3D geometry engine (matrix)",
	"", "", "", "", "",
	"2D B engine",
	"", "", "", "", "",
	"Swap LCD"
};
static const char *powercr7_strings[] =
{
	"Sound speakers",
	"Wifi system"
};
#define POWER_CR_SKIP(c) if(i==4||i==5||i==6||i==7||i==8||i==10||i==11||i==12||i==13||i==14)continue;
#define POWER_CR_SIZE(c) ((c==0)?16:2)

static GtkWidget *Widgets_POWER_CR[2][16];
static void crea_POWER_CR(int c)
{
	REGFN_BEGIN(POWER_CR);
	int i;
	for(i = 0; i < POWER_CR_SIZE(c); i++) { POWER_CR_SKIP(c); BIT_CHECK(i, i, (c==0?powercr9_strings:powercr7_strings)[i]); }
	CREA_END();
}
static void updt_POWER_CR(int c)
{
	int i;
	for(i = 0; i < POWER_CR_SIZE(c); i++) { POWER_CR_SKIP(c); gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(Widgets_POWER_CR[c][i]), (((u16 *)(MMU.MMU_MEM[c][0x40]))[0x304>>1]&(1<<i)) ? 1 : 0); }
}
static void dest_POWER_CR(int c)
{
	int i;
	for(i = 0; i < POWER_CR_SIZE(c); i++) { POWER_CR_SKIP(c); gtk_widget_destroy(Widgets_POWER_CR[c][i]); }
}
static u32 val_POWER_CR(int c) { return ((u16 *)(MMU.MMU_MEM[c][0x40]))[0x304>>1]; }

/////////////////////////////// REG_SPICNT ///////////////////////////////
static const char *spicnt_strings[16] =
{
	"Baudrate",
	"","","","","","",
	"Busy flag",
	"Device",
	"",
	"Transfer size",
	"Chipselect Hold",
	"Unknown",
	"Unknown",
	"Interrupt request",
	"Enable SPI bus",
};

//   0-1 Baudrate   (0=4MHz/Firmware, 1=2MHz/Touchscr, 2=1MHz/Powerman., 3=512KHz)
// 		  2-6 Not used            (Zero)
// 		  7   Busy Flag           (0=Ready, 1=Busy) (presumably Read-only)
// 		  8-9 Device Select       (0=Powerman., 1=Firmware, 2=Touchscr, 3=Reserved)
// 		  10  Transfer Size       (0=8bit, 1=16bit)
// 		  11  Chipselect Hold     (0=Deselect after transfer, 1=Keep selected)
// 		  12  Unknown (usually 0) (set equal to Bit11 when BIOS accesses firmware)
// 		  13  Unknown (usually 0) (set to 1 when BIOS accesses firmware)
// 		  14  Interrupt Request   (0=Disable, 1=Enable)
// 		  15  SPI Bus Enable      (0=Disable, 1=Enable)


#define REG_SPICNT_SKIP(c) if(i==1||i==2||i==3||i==4||i==5||i==6||i==9)continue;
#define REG_SPICNT_ISCHECK(c) (i==7||i==11||i==14||i==15||i==12||i==13)

static GtkWidget *Widgets_REG_SPICNT[2][16];
static void crea_REG_SPICNT(int c)
{
	REGFN_BEGIN(REG_SPICNT);
	int i;
	for(i = 0; i < 16; i++) { REG_SPICNT_SKIP(c);
		if(REG_SPICNT_ISCHECK(c)) { BIT_CHECK(i, i, spicnt_strings[i]); }
		else if(i == 0) { BIT_COMBO(i, "0-1", spicnt_strings[0]);
			BIT_COMBO_ADD(i, "0= 4Mhz");
			BIT_COMBO_ADD(i, "1= 2Mhz");
			BIT_COMBO_ADD(i, "2= 1Mhz");
			BIT_COMBO_ADD(i, "3= 512Khz");
			BIT_COMBO_END(i); }
		else if(i == 8) { BIT_COMBO(i, "8-9", spicnt_strings[8]);
			BIT_COMBO_ADD(i, "0= Power management device");
			BIT_COMBO_ADD(i, "1= Firmware");
			BIT_COMBO_ADD(i, "2= Touchscreen/Microphone");
			BIT_COMBO_ADD(i, "3= Reserved/Prohibited");
			BIT_COMBO_END(i); }
		else if(i == 10) { BIT_COMBO(i, "10", spicnt_strings[10]);
			BIT_COMBO_ADD(i, "0= 8 bits");
			BIT_COMBO_ADD(i, "1= 16 bits");
			BIT_COMBO_END(i); }
	}
	CREA_END();
}
static u32 val_REG_SPICNT(int c) { return ((u16 *)(MMU.MMU_MEM[c][0x40]))[0x1C0>>1]; }
static void updt_REG_SPICNT(int c)
{
	REGFN_BEGIN(REG_SPICNT);
	u16 val = val_REG_SPICNT(c);
	int i;
	for(i = 0; i < 16; i++) { REG_SPICNT_SKIP(c);
		if(REG_SPICNT_ISCHECK(c)) { gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(Widgets_REG_SPICNT[c][i]), (((u16 *)(MMU.MMU_MEM[c][0x40]))[0x1C0>>1]&(1<<i)) ? 1 : 0); }
		else if(i == 0) { gtk_combo_box_set_active(GTK_COMBO_BOX(BIT_COMBO_GET(i)) , val&3); }
		else if(i == 8) { gtk_combo_box_set_active(GTK_COMBO_BOX(BIT_COMBO_GET(i)) , (val>>8)&3); }
		else if(i == 10) { gtk_combo_box_set_active(GTK_COMBO_BOX(BIT_COMBO_GET(i)) , (val>>10)&1); }
	}
}
static void dest_REG_SPICNT(int c)
{
	int i;
	for(i = 0; i < 16; i++) { REG_SPICNT_SKIP(c); gtk_widget_destroy(Widgets_REG_SPICNT[c][i]); }
}

/////////////////////////////// LIST ///////////////////////////////
#define BITS_8 1
#define BITS_16 2
#define BITS_32 4
static const char *bits_strings[5] = {"0", "8", "16", "24", "32"};

#define REG_STR(r, s) {#r, r, s, &crea_##r, &updt_##r, &dest_##r, &val_##r}
#define REG_FNS(r) &crea_##r, &updt_##r, &dest_##r, &val_##r
//////// ARM9 ////////
#define REG_LIST_SIZE_ARM9 5
static const reg_t regs_list_9[] =
{
	{"REG_IME", REG_IME, BITS_16, REG_FNS(REG_IME)},
	{"REG_IE", REG_IE, BITS_32, REG_FNS(REG_IE)},
	{"REG_IF", REG_IF, BITS_32, REG_FNS(REG_IF)},
	{"REG_IPCFIFOCNT", 0x04000184, BITS_16, REG_FNS(REG_IPCFIFOCNT)},
	{"POWER_CR", REG_POWCNT1, BITS_16, REG_FNS(POWER_CR)}
};

//////// ARM7 ////////
#define REG_LIST_SIZE_ARM7 6
static const reg_t regs_list_7[] =
{
	{"REG_IME", REG_IME, BITS_16, REG_FNS(REG_IME)},
	{"REG_IE", REG_IE, BITS_32, REG_FNS(REG_IE)},
	{"REG_IF", REG_IF, BITS_32, REG_FNS(REG_IF)},
	{"REG_IPCFIFOCNT", 0x04000184, BITS_16, REG_FNS(REG_IPCFIFOCNT)},
	{"POWER_CR", REG_POWCNT1, BITS_16, REG_FNS(POWER_CR)},
	{"REG_SPICNT", REG_SPICNT, BITS_16, REG_FNS(REG_SPICNT)}
};

#define GET_REG_LIST_SIZE(i) ((i==0)?REG_LIST_SIZE_ARM9:REG_LIST_SIZE_ARM7)
#define GET_REG_LIST(i) ((i==0)?regs_list_9:regs_list_7)

static void _clearContainer(GtkWidget *widget, gpointer data)
{
	if(widget == mRegInfos[0] || widget == mRegInfos[1]) return;
	if(widget == mIoRegCombo[0] || widget == mIoRegCombo[1]) return;

	gtk_container_remove(GTK_CONTAINER((GtkWidget*)data), widget);
//	gtk_widget_destroy(widget);
//	gtk_object_destroy(GTK_OBJECT(widget));
}

static void selected_reg(GtkWidget* widget, gpointer data)
{
	int c = GPOINTER_TO_INT(data);
	gchar *regInfosBuffer;

	guint active = gtk_combo_box_get_active(GTK_COMBO_BOX(widget));

	if(current_reg[c]) current_reg[c]->destroy(c);
	gtk_container_foreach(GTK_CONTAINER(mVbox0[c]), _clearContainer, (gpointer)mVbox0[c]);

	current_reg[c] = (reg_t*)&(GET_REG_LIST(c)[active]);

// 	gtk_box_pack_start(GTK_BOX(mVbox0[c]), mIoRegCombo[c], FALSE, FALSE, 0);
	
	switch (current_reg[c]->size) {
	case BITS_8:
		regInfosBuffer = g_strdup_printf("0x%02X", current_reg[c]->value(c));
		break;
	case BITS_16:
		regInfosBuffer = g_strdup_printf("0x%04X", current_reg[c]->value(c));
		break;
	default:
		regInfosBuffer = g_strdup_printf("0x%08X", current_reg[c]->value(c));
	}	
// 	gtk_box_pack_start(GTK_BOX(mVbox0[c]), mRegInfos[c], FALSE, FALSE, 0);
	gtk_label_set_label(GTK_LABEL(mRegInfos[c]), regInfosBuffer);
	g_free(regInfosBuffer);

	current_reg[c]->create(c);
	current_reg[c]->update(c);
}

/////////////////////////////// TOOL ///////////////////////////////

static int DTOOL_ID;

static void close()
{
	memset(current_reg, 0, sizeof(current_reg));
	dTool_CloseCallback(DTOOL_ID);
}

static void _closeOne(GtkWidget *widget, gpointer data)
{
	int c = GPOINTER_TO_INT(data);

	CPUS[c] = FALSE;
	if(c == 0 && !CPUS[1]) close();
	if(c == 1 && !CPUS[0]) close();

	gtk_widget_destroy(mRegInfos[c]);
	gtk_widget_destroy(mIoRegCombo[c]);
	gtk_widget_destroy(mVbox0[c]);
//	gtk_widget_destroy(mWin[c]);
}

static void update()
{
	int c;

	for(c = 0; c < 2; c++)
	{
		if(!CPUS[c]) continue;
		current_reg[c]->update(c);
	}
}

static void open(int ID)
{
	int c, i;

	DTOOL_ID = ID;

	for(c = 0; c < 2; c++)
	{
		CPUS[c] = TRUE;

		mWin[c]= gtk_window_new(GTK_WINDOW_TOPLEVEL);
		if(c == 0)	gtk_window_set_title(GTK_WINDOW(mWin[c]), TOOL_NAME " : ARM9");
		else	gtk_window_set_title(GTK_WINDOW(mWin[c]), TOOL_NAME " : ARM7");
		g_signal_connect(G_OBJECT(mWin[c]), "destroy", G_CALLBACK(&_closeOne), GINT_TO_POINTER(c));

		mVbox0[c] = gtk_vbox_new(FALSE, 0);
		gtk_container_add(GTK_CONTAINER(mWin[c]), mVbox0[c]);

		mIoRegCombo[c] = gtk_combo_box_text_new();
		mRegInfos[c] = gtk_label_new("");

		for(i = 0; i < GET_REG_LIST_SIZE(c); i++)
		{
			gchar *reg_name_buffer;
			reg_name_buffer = g_strdup_printf("0x%08X : %s (%s)", GET_REG_LIST(c)[i].adress, GET_REG_LIST(c)[i].name, bits_strings[GET_REG_LIST(c)[i].size]);
			gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(mIoRegCombo[c]), reg_name_buffer);
			g_free(reg_name_buffer);
		}

		gtk_combo_box_set_active(GTK_COMBO_BOX(mIoRegCombo[c]), 0);
		g_signal_connect(G_OBJECT(mIoRegCombo[c]), "changed", G_CALLBACK(selected_reg), GINT_TO_POINTER(c));

		gtk_box_pack_start(GTK_BOX(mVbox0[c]), mIoRegCombo[c], FALSE, FALSE, 0);
		gtk_box_pack_start(GTK_BOX(mVbox0[c]), mRegInfos[c], FALSE, FALSE, 0);
		selected_reg(mIoRegCombo[c], GINT_TO_POINTER(c));

		gtk_widget_show_all(mWin[c]);
	}
}

/////////////////////////////// TOOL DEFINITION ///////////////////////////////

dTool_t dTool_ioregsView =
{
	SHORTNAME,
	TOOL_NAME,
	&open,
	&update,
	&close
};

