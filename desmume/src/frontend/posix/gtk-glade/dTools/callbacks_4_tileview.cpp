/* callbacks_3_palview.cpp - this file is part of DeSmuME
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
#ifdef GTKGLEXT_AVAILABLE
#include "../gdk_gl.h"

void init_combo_memory(GtkComboBox *combo, u8 ** addresses) {
	GtkTreeIter iter;
	GtkListStore* model = gtk_list_store_new(1, G_TYPE_STRING);
	gtk_combo_box_set_model(combo, (GtkTreeModel*)model);
	int i=0;

#define DO(str,addr,r)  \
	gtk_list_store_append (model, &iter); \
	gtk_list_store_set (model, &iter, 0, str,-1); \
	addresses[i]=(addr r); i++;

//  FIXME: update tileview to actually work
//	DO("A-BG - 0x6000000",MMU.ARM9_ABG,)
//	DO("A-BG - 0x6010000",MMU.ARM9_ABG,+0x10000)
//	DO("A-BG - 0x6020000",MMU.ARM9_ABG,+0x20000)
//	DO("A-BG - 0x6030000",MMU.ARM9_ABG,+0x30000)
//	DO("A-BG - 0x6040000",MMU.ARM9_ABG,+0x40000)
//	DO("A-BG - 0x6050000",MMU.ARM9_ABG,+0x50000)
//	DO("A-BG - 0x6060000",MMU.ARM9_ABG,+0x60000)
//	DO("A-BG - 0x6070000",MMU.ARM9_ABG,+0x70000)
	
//	DO("B-BG - 0x6200000",MMU.ARM9_BBG,)
//	DO("B-BG - 0x6210000",MMU.ARM9_BBG,+0x10000)
	
//	DO("A-OBJ- 0x6400000",MMU.ARM9_AOBJ,)
//	DO("A-OBJ- 0x6410000",MMU.ARM9_AOBJ,+0x10000)
//	DO("A-OBJ- 0x6420000",MMU.ARM9_AOBJ,+0x20000)
//	DO("A-OBJ- 0x6430000",MMU.ARM9_AOBJ,+0x30000)
	
//	DO("B-OBJ- 0x6600000",MMU.ARM9_BOBJ,)
//	DO("B-OBJ- 0x6610000",MMU.ARM9_BOBJ,+0x10000)
	
	DO("LCD - 0x6800000",MMU.ARM9_LCD,)
	DO("LCD - 0x6810000",MMU.ARM9_LCD,+0x10000)
	DO("LCD - 0x6820000",MMU.ARM9_LCD,+0x20000)
	DO("LCD - 0x6830000",MMU.ARM9_LCD,+0x30000)
	DO("LCD - 0x6840000",MMU.ARM9_LCD,+0x40000)
	DO("LCD - 0x6850000",MMU.ARM9_LCD,+0x50000)
	DO("LCD - 0x6860000",MMU.ARM9_LCD,+0x60000)
	DO("LCD - 0x6870000",MMU.ARM9_LCD,+0x70000)
	DO("LCD - 0x6880000",MMU.ARM9_LCD,+0x80000)
	DO("LCD - 0x6890000",MMU.ARM9_LCD,+0x90000)
#undef DO
	gtk_combo_box_set_active(combo,0);
}

static u16* pal_addr[20];
static u8* mem_addr[26];
static BOOL init=FALSE;
static int palnum=0;
static int palindex=0;
static int memnum=0;
static int colnum=0;
static void refresh();
static GtkWidget * wPaint;
static GtkSpinButton * wSpin;
static NDSDisplayID gl_context_num = NDSDisplayID_Main;

#define TILE_NUM_MAX  1024
#define TILE_W_SZ     8
#define TILE_H_SZ     8

static void wtools_4_update() {

}

typedef u16 tileBMP[8*8];

static void refresh() {
	u16  palette_16[64];
	u16  palette_256[64];
	u8  * index16, * index256, * indexBMP;
	u16 * pal;
	int tile_n, index;
	guint Textures;
	if (!init) return;

	index16 = index256 = indexBMP = mem_addr[memnum];

	// this little thing doesnt display properly
	// nothing drawn...
	// seems that is the context is not shared there is a pb switching context
/*
	if (!my_gl_Begin(gl_context_num)) return;
	glClear( GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT );
	my_gl_DrawBeautifulQuad();
	my_gl_End(gl_context_num);
	return;

*/
	if (!my_gl_Begin(gl_context_num)) return;

	glClear( GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT );
#if 1
	glEnable(GL_TEXTURE_2D);
	glGenTextures(1, &Textures);

	//proxy
	glBindTexture(GL_TEXTURE_2D, Textures);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA,
		256, 256, 0,
		GL_RGBA, GL_UNSIGNED_SHORT_1_5_5_5_REV, indexBMP);

	glBegin(GL_QUADS);
		glTexCoord2f(0.0, 0.0); glVertex2d(-1.0, 1.0);
		glTexCoord2f(1.0, 0.0); glVertex2d( 1.0, 1.0);
		glTexCoord2f(1.0, 1.0); glVertex2d( 1.0,-1.0);
		glTexCoord2f(0.0, 1.0); glVertex2d(-1.0,-1.0);
	glEnd();

	glDeleteTextures(1, &Textures);

	my_gl_End(gl_context_num);
	return;

	glTexImage2D(GL_PROXY_TEXTURE_2D, 0, GL_RGBA,
		256, 256, 0,
		GL_RGBA, GL_UNSIGNED_SHORT_1_5_5_5_REV, NULL);


	switch(colnum) {
	case 0: //BMP
		for (tile_n=0; tile_n<TILE_NUM_MAX; tile_n++) {
			glTexSubImage2D(GL_TEXTURE_2D, 0, 
				(tile_n & 0x1F)<<3, (tile_n >> 5)<<3,
				8, 8, GL_RGBA, 
				GL_UNSIGNED_SHORT_1_5_5_5_REV, indexBMP);
			indexBMP +=64;
		}
	break;
	case 1: //256c
	pal = pal_addr[palindex];
	if (pal) {
		pal += palnum*256;
		for (tile_n=0; tile_n<1024; tile_n++) {
			for (index=0; index<64; index++) {
				palette_256[index]=pal[*index256];
				index256++;
			}
			glTexSubImage2D(GL_TEXTURE_2D, 0, 
				(tile_n & 0x1F)<<3, (tile_n >> 5)<<3,
				8, 8, GL_RGBA, 
				GL_UNSIGNED_SHORT_1_5_5_5_REV, palette_256);
		}
	}
	break;
	case 2: //16c
	pal = pal_addr[palindex];
	if (pal) {
		pal += palnum*16;
		for (tile_n=0; tile_n<1024; tile_n++) {
			for (index=0; index<64-1; index++) {
				if (index & 1) continue;
				palette_16[index]  =pal[*index16 & 15];
				palette_16[index+1]=pal[*index16 >> 4];
				index16++;
			}
			glTexSubImage2D(GL_TEXTURE_2D, 0, 
				(tile_n & 0x1F)<<3, (tile_n >> 5)<<3,
				8, 8, GL_RGBA, 
				GL_UNSIGNED_SHORT_1_5_5_5_REV, palette_16);
		}
	}
	break;
	}

	glBegin(GL_QUADS);
		glTexCoord2f(0.0, 0.0); glVertex2d(-0.5, 0.5);
		glTexCoord2f(0.0, 1.0); glVertex2d(-0.5,-0.5);
		glTexCoord2f(1.0, 1.0); glVertex2d( 0.5,-0.5);
		glTexCoord2f(1.0, 0.0); glVertex2d( 0.5, 0.5);
	glEnd();

	glDeleteTextures(1, &Textures);
#else
	glDisable(GL_LIGHTING);
	glDisable(GL_BLEND);
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();
	glColor4ub(255,255,255,255);
	switch(colnum) {
	case 0: //BMP
	{
		tileBMP tiles * = indexBMP;

		for (tile_n=0; tile_n<1024; tile_n++) {
			i = (tile_n & 0x1F) << 4;
			j = (tile_n >> 5) << 12;
			memcpy(bmp + j + i, indexBMP, 16); indexBMP += 16; j+=512;
			memcpy(bmp + j + i, indexBMP, 16); indexBMP += 16; j+=512;
			memcpy(bmp + j + i, indexBMP, 16); indexBMP += 16; j+=512;
			memcpy(bmp + j + i, indexBMP, 16); indexBMP += 16; j+=512;
			memcpy(bmp + j + i, indexBMP, 16); indexBMP += 16; j+=512;
			memcpy(bmp + j + i, indexBMP, 16); indexBMP += 16; j+=512;
			memcpy(bmp + j + i, indexBMP, 16); indexBMP += 16; j+=512;
			memcpy(bmp + j + i, indexBMP, 16); indexBMP += 16;
		}
		glRasterPos2i(0,0);
		glDrawPixels(256,256,GL_RGBA,GL_UNSIGNED_SHORT_1_5_5_5_REV, bmp);
	}
	break;
	case 1: //256c
	if (pal = pal_addr[palindex]) {
		pal += palnum*256;
		for (tile_n=0; tile_n<1024; tile_n++) {
			for (index=0; index<64; index++) {
				palette_256[index]=pal[*index256];
				index256++;
			}
			glRasterPos2i((tile_n & 0x1F)<<3, (tile_n >> 5)<<3);
			glDrawPixels(8,8,GL_RGBA,GL_UNSIGNED_SHORT_1_5_5_5_REV, palette_256);
		}
	}
	break;
	case 2: //16c
	if (pal = pal_addr[palindex]) {
		pal += palnum*16;
		for (tile_n=0; tile_n<1024; tile_n++) {
			for (index=0; index<64; index++) {
				if (index & 1) continue;
				palette_16[index]  =pal[*index16 & 15];
				palette_16[index+1]=pal[*index16 >> 4];
				index16++;
			}
			glRasterPos2i((tile_n & 0x1F)<<3, (tile_n >> 5)<<3);
			glDrawPixels(8,8,GL_RGBA,GL_UNSIGNED_SHORT_1_5_5_5_REV, palette_16);
		}
	}
	break;
	}
#endif
	my_gl_End(gl_context_num);
}

static void initialize() {
	GtkComboBox * combo;
	if (init) return;

	wPaint= glade_xml_get_widget(xml_tools, "wDraw_Tile");
	wSpin = (GtkSpinButton*)glade_xml_get_widget(xml_tools, "wtools_4_palnum");
	combo = (GtkComboBox*)glade_xml_get_widget(xml_tools, "wtools_4_palette");
	init_combo_palette(combo, pal_addr);
	combo = (GtkComboBox*)glade_xml_get_widget(xml_tools, "wtools_4_memory");
	init_combo_memory(combo, mem_addr);

	gl_context_num = init_GL_free_s(wPaint, NDSDisplayID_Main);
	reshape(wPaint, gl_context_num);
	gtk_widget_show(wPaint);
	init=TRUE;
}


void     on_wtools_4_TileView_show         (GtkWidget *widget, gpointer data) {
	initialize();
	register_Tool(wtools_4_update);
}
gboolean on_wtools_4_TileView_close         (GtkWidget *widget, ...) {
	unregister_Tool(wtools_4_update);
	gtk_widget_hide(widget);
	return TRUE;
}

void     on_wtools_4_palette_changed      (GtkComboBox *combo,   gpointer user_data) {
	palindex = gtk_combo_box_get_active(combo);
	gtk_widget_set_sensitive((GtkWidget*)wSpin,(palindex >=4));
	gtk_spin_button_set_value(wSpin,0);
	gtk_widget_queue_draw(wPaint);
}
void     on_wtools_4_palnum_value_changed (GtkSpinButton *spin, gpointer user_data) {
	palnum = gtk_spin_button_get_value_as_int(spin);
	gtk_widget_queue_draw(wPaint);
}
void     on_wtools_4_memory_changed (GtkComboBox *combo, gpointer user_data) {
	memnum = gtk_combo_box_get_active(combo);
	gtk_widget_queue_draw(wPaint);
}
void	 on_wtools_4_rXX_toggled (GtkToggleButton *togglebutton, gpointer user_data) {
	colnum = dyn_CAST(int,user_data);
	gtk_widget_queue_draw(wPaint);
}
gboolean on_wDraw_Tile_expose_event       (GtkWidget * w, GdkEventExpose * e, gpointer user_data) {
	refresh();
	return TRUE;
}
#else
void     on_wtools_4_TileView_show         (GtkWidget *widget, gpointer data) {
}
gboolean on_wtools_4_TileView_close         (GtkWidget *widget, ...) {
	return FALSE;
}
void     on_wtools_4_palette_changed      (GtkComboBox *combo,   gpointer user_data) {
}
void     on_wtools_4_palnum_value_changed (GtkSpinButton *spin, gpointer user_data) {
}
void     on_wtools_4_memory_changed (GtkComboBox *combo, gpointer user_data) {
}
void	 on_wtools_4_rXX_toggled (GtkToggleButton *togglebutton, gpointer user_data) {
}
gboolean on_wDraw_Tile_expose_event       (GtkWidget * w, GdkEventExpose * e, gpointer user_data) {
	return FALSE;
}
#endif
