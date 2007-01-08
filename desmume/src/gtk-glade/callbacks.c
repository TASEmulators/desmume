/* callbacks.c - this file is part of DeSmuME
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

#include "callbacks.h"

/* globals */
uint Frameskip = 0;
gboolean ScreenRight=FALSE;
gboolean ScreenGap=FALSE;

/* inline & protos */

void enable_rom_features() {
	SET_SENSITIVE("menu_exec", TRUE);
	SET_SENSITIVE("menu_pause", TRUE);
	SET_SENSITIVE("menu_reset", TRUE);
	SET_SENSITIVE("wgt_Exec", TRUE);
	SET_SENSITIVE("wgt_Reset", TRUE);
}

void MAINWINDOW_RESIZE() {
	GtkWidget * spacer1 = glade_xml_get_widget(xml, "misc_sep3");
	GtkWidget * spacer2 = glade_xml_get_widget(xml, "misc_sep4");
	int dim = 66 * ScreenCoeff_Size;
	
	/* sees whether we want a gap */
	if (!ScreenGap) dim = -1;
	if (ScreenRight && ScreenRotate) {
		gtk_widget_set_usize(spacer1, dim, -1);
	} else if (!ScreenRight && !ScreenRotate) {
		gtk_widget_set_usize(spacer2, -1, dim);
	} else {
		gtk_widget_set_usize(spacer1, -1, -1);	
		gtk_widget_set_usize(spacer2, -1, -1);	
	}

	gtk_window_resize ((GtkWindow*)pWindow,1,1);
}

void inline SET_SENSITIVE(gchar *w, gboolean b) {
	gtk_widget_set_sensitive(
		glade_xml_get_widget(xml, w), TRUE);
}


/* MENU FILE ***** ***** ***** ***** */
void inline ADD_FILTER(GtkWidget * filech, const char * pattern, const char * name) {
	GtkFileFilter *pFilter;
	pFilter = gtk_file_filter_new();
	gtk_file_filter_add_pattern(pFilter, pattern);
	gtk_file_filter_set_name(pFilter, name);
	gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(filech), pFilter);
}

void file_open() {
	desmume_pause();
	
	GtkWidget *pFileSelection;
	GtkWidget *pParent;
	gchar *sChemin;

	pParent = GTK_WIDGET(pWindow);
	
	/* Creating the selection window */
	pFileSelection = gtk_file_chooser_dialog_new("Open...",
			GTK_WINDOW(pParent),
			GTK_FILE_CHOOSER_ACTION_OPEN,
			GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
			GTK_STOCK_OPEN, GTK_RESPONSE_OK,
			NULL);
	/* On limite les actions a cette fenetre */
	gtk_window_set_modal(GTK_WINDOW(pFileSelection), TRUE);

	ADD_FILTER(pFileSelection, "*.nds", "Nds binary (.nds)");
	ADD_FILTER(pFileSelection, "*.ds.gba", "Nds binary with loader (.ds.gba)");
	ADD_FILTER(pFileSelection, "*", "All files");
	//ADD_FILTER(pFileSelection, "*.zip", "Nds zipped binary");
	
	/* Affichage fenetre*/
	switch(gtk_dialog_run(GTK_DIALOG(pFileSelection)))
	{
		case GTK_RESPONSE_OK:
			/* Recuperation du chemin */
			sChemin = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(pFileSelection));
			if(desmume_open((const char*)sChemin) < 0)
			{
				GtkWidget *pDialog = gtk_message_dialog_new(GTK_WINDOW(pFileSelection),
						GTK_DIALOG_MODAL,
						GTK_MESSAGE_ERROR,
						GTK_BUTTONS_OK,
						"Unable to load :\n%s", sChemin);
				gtk_dialog_run(GTK_DIALOG(pDialog));
				gtk_widget_destroy(pDialog);
			} else {
				desmume_resume();
				enable_rom_features();
			}

			g_free(sChemin);
			break;
		default:
			break;
	}
	gtk_widget_destroy(pFileSelection);
}

void  on_menu_ouvrir_activate  (GtkMenuItem *menuitem, gpointer user_data) { file_open();}
void  on_menu_pscreen_activate (GtkMenuItem *menuitem, gpointer user_data) {  WriteBMP("./test.bmp",GPU_screen); }
void  on_menu_quit_activate    (GtkMenuItem *menuitem, gpointer user_data) { gtk_main_quit(); }



/* MENU EMULATION ***** ***** ***** ***** */
void  on_menu_exec_activate   (GtkMenuItem *menuitem, gpointer user_data) { desmume_resume(); }
void  on_menu_pause_activate  (GtkMenuItem *menuitem, gpointer user_data) { desmume_pause(); }
void  on_menu_reset_activate  (GtkMenuItem *menuitem, gpointer user_data) { desmume_reset(); }
void  on_menu_layers_activate (GtkMenuItem *menuitem, gpointer user_data) {
	/* we want to hide or show the checkbox for the layers */
	GtkWidget * w1 = glade_xml_get_widget(xml, "wvb_1_Main");
	GtkWidget * w2 = glade_xml_get_widget(xml, "wvb_2_Sub");
	if (gtk_check_menu_item_get_active((GtkCheckMenuItem*)menuitem)==TRUE) {
		gtk_widget_show(w1);
		gtk_widget_show(w2);
	} else {
		gtk_widget_hide(w1);
		gtk_widget_hide(w2);
	}
	/* pack the window */
	MAINWINDOW_RESIZE();
}


/* SUBMENU FRAMESKIP ***** ***** ***** ***** */
void  on_fs0_activate  (GtkMenuItem *menuitem,gpointer user_data) { Frameskip = 0; }
void  on_fs1_activate  (GtkMenuItem *menuitem,gpointer user_data) { Frameskip = 1; }
void  on_fs2_activate  (GtkMenuItem *menuitem,gpointer user_data) { Frameskip = 2; }
void  on_fs3_activate  (GtkMenuItem *menuitem,gpointer user_data) { Frameskip = 3; }
void  on_fs4_activate  (GtkMenuItem *menuitem,gpointer user_data) { Frameskip = 4; }
void  on_fs5_activate  (GtkMenuItem *menuitem,gpointer user_data) { Frameskip = 5; }
void  on_fs6_activate  (GtkMenuItem *menuitem,gpointer user_data) { Frameskip = 6; }
void  on_fs7_activate  (GtkMenuItem *menuitem,gpointer user_data) { Frameskip = 7; }
void  on_fs8_activate  (GtkMenuItem *menuitem,gpointer user_data) { Frameskip = 8; }
void  on_fs9_activate  (GtkMenuItem *menuitem,gpointer user_data) { Frameskip = 9; }


/* SUBMENU SIZE ***** ***** ***** ***** */
int H=192, W=256;
void resize (int Size) {
	/* we want to scale drawing areas by a factor (1x,2x or 3x) */
	gtk_drawing_area_size(GTK_DRAWING_AREA(pDrawingArea), W * Size, H * Size);
	gtk_widget_set_usize (pDrawingArea, W * Size, H * Size);	
	gtk_drawing_area_size(GTK_DRAWING_AREA(pDrawingArea2), W * Size, H * Size);
	gtk_widget_set_usize (pDrawingArea2, W * Size, H * Size);	
	ScreenCoeff_Size = Size;
	/* remove artifacts */
	black_screen();
	/* pack the window */
	MAINWINDOW_RESIZE();
}

void  on_size1x_activate (GtkMenuItem *menuitem, gpointer user_data) { resize(1); }
void  on_size2x_activate (GtkMenuItem *menuitem, gpointer user_data) { resize(2); }
void  on_size3x_activate (GtkMenuItem *menuitem, gpointer user_data) { resize(3); }


/* MENU CONFIG ***** ***** ***** ***** */
gint Keypad_Temp[DESMUME_NB_KEYS];

void  on_menu_controls_activate     (GtkMenuItem *menuitem, gpointer user_data) {
	edit_controls();
}

void  on_menu_audio_on_activate  (GtkMenuItem *menuitem, gpointer user_data) {
	/* we want set audio emulation ON or OFF */
	if (gtk_check_menu_item_get_active((GtkCheckMenuItem*)menuitem)) {
		SPU_Pause(0);
	} else {
		SPU_Pause(1);
	}
}

void  on_menu_gapscreen_activate  (GtkMenuItem *menuitem, gpointer user_data) {
	/* we want to add a gap between screens */
	ScreenGap = gtk_check_menu_item_get_active((GtkCheckMenuItem*)menuitem);

	/* pack the window */
	MAINWINDOW_RESIZE();
}

void  on_menu_rightscreen_activate  (GtkMenuItem *menuitem, gpointer user_data) {
	GtkBox * sbox = (GtkBox*)glade_xml_get_widget(xml, "whb_Sub");
	GtkWidget * mbox = glade_xml_get_widget(xml, "whb_Main");
	GtkWidget * vbox = glade_xml_get_widget(xml, "wvb_Layout");
	GtkWidget * w    = glade_xml_get_widget(xml, "wvb_2_Sub");

	ScreenRight=gtk_check_menu_item_get_active((GtkCheckMenuItem*)menuitem);
	/* we want to change the layout, lower screen goes left */
	if (ScreenRight) {
		gtk_box_reorder_child(sbox,w,-1);
		gtk_widget_reparent((GtkWidget*)sbox,mbox);
	} else {
	/* we want to change the layout, lower screen goes down */
		gtk_box_reorder_child(sbox,w,0);
		gtk_widget_reparent((GtkWidget*)sbox,vbox);
	}
	/* pack the window */
	MAINWINDOW_RESIZE();
}

void  on_menu_rotatescreen_activate  (GtkMenuItem *menuitem, gpointer user_data) {
	/* we want to rotate the screen */
	ScreenRotate = gtk_check_menu_item_get_active((GtkCheckMenuItem*)menuitem);
	if (ScreenRotate) {
		H=256; W=192;
	} else {
		W=256; H=192;
	}
	resize(ScreenCoeff_Size);
}

/* MENU TOOLS ***** ***** ***** ***** */
void  on_menu_IO_regs_activate      (GtkMenuItem *menuitem, gpointer user_data) {
	GtkWidget * dlg = glade_xml_get_widget(xml_tools, "wtools_1_IOregs");
	gtk_widget_show(dlg);
}


/* MENU ? ***** ***** ***** ***** */
void  on_menu_apropos_activate      (GtkMenuItem *menuitem, gpointer user_data) {
	GtkAboutDialog * wAbout = (GtkAboutDialog*)glade_xml_get_widget(xml, "wAboutDlg");
	gtk_about_dialog_set_version(wAbout, VERSION);
	gtk_widget_show((GtkWidget*)wAbout);
}






/* TOOLBAR ***** ***** ***** ***** */
void  on_wgt_Open_clicked  (GtkToolButton *toolbutton, gpointer user_data) { file_open(); }
void  on_wgt_Reset_clicked (GtkToolButton *toolbutton, gpointer user_data) { desmume_reset(); }
void  on_wgt_Quit_clicked  (GtkToolButton *toolbutton, gpointer user_data) { gtk_main_quit(); }
void  on_wgt_Exec_toggled  (GtkToggleToolButton *toggletoolbutton, gpointer user_data) {
	if (gtk_toggle_tool_button_get_active(toggletoolbutton)==TRUE)
		desmume_resume();
	else 
		desmume_pause();
}



/* LAYERS ***** ***** ***** ***** */
void change_bgx_layer(int layer, gboolean state, Screen scr) {
	//if(!desmume_running()) return;
	if(state==TRUE) { 
		if (!scr.gpu->dispBG[layer]) GPU_addBack(scr.gpu, layer);
	} else {
		if (scr.gpu->dispBG[layer])  GPU_remove(scr.gpu, layer); 
	}
	//fprintf(stderr,"Changed Layer %s to %d\n",layer,state);
}
void change_obj_layer(gboolean state, Screen scr) {
	// TODO
}

/* LAYERS MAIN SCREEN ***** ***** ***** ***** */
void  on_wc_1_BG0_toggled  (GtkToggleButton *togglebutton, gpointer user_data) { 
	change_bgx_layer(0, gtk_toggle_button_get_active(togglebutton), MainScreen); }
void  on_wc_1_BG1_toggled  (GtkToggleButton *togglebutton, gpointer user_data) {
	change_bgx_layer(1, gtk_toggle_button_get_active(togglebutton), MainScreen); }
void  on_wc_1_BG2_toggled  (GtkToggleButton *togglebutton, gpointer user_data) {
	change_bgx_layer(2, gtk_toggle_button_get_active(togglebutton), MainScreen); }
void  on_wc_1_BG3_toggled  (GtkToggleButton *togglebutton, gpointer user_data) {
	change_bgx_layer(3, gtk_toggle_button_get_active(togglebutton), MainScreen); }
void  on_wc_1_OBJ_toggled  (GtkToggleButton *togglebutton, gpointer user_data) {
	change_obj_layer(gtk_toggle_button_get_active(togglebutton), MainScreen); }

/* LAYERS SECOND SCREEN ***** ***** ***** ***** */
void  on_wc_2b_BG0_toggled  (GtkToggleButton *togglebutton, gpointer user_data) {
	change_bgx_layer(0, gtk_toggle_button_get_active(togglebutton), SubScreen); }
void  on_wc_2b_BG1_toggled  (GtkToggleButton *togglebutton, gpointer user_data) {
	change_bgx_layer(1, gtk_toggle_button_get_active(togglebutton), SubScreen); }
void  on_wc_2b_BG2_toggled  (GtkToggleButton *togglebutton, gpointer user_data) {
	change_bgx_layer(2, gtk_toggle_button_get_active(togglebutton), SubScreen); }
void  on_wc_2b_BG3_toggled  (GtkToggleButton *togglebutton, gpointer user_data) {
	change_bgx_layer(3, gtk_toggle_button_get_active(togglebutton), SubScreen); }
void  on_wc_2b_OBJ_toggled  (GtkToggleButton *togglebutton, gpointer user_data) {
	change_obj_layer(gtk_toggle_button_get_active(togglebutton), SubScreen); }
