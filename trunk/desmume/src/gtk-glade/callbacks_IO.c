#include "callbacks_IO.h"

static u16 Cur_Keypad = 0;
int ScreenCoeff_Size=1;
gboolean ScreenRotate=FALSE;


/* ***** ***** INPUT BUTTONS / KEYBOARD ***** ***** */



u16 inline lookup_key (guint keyval) {
	int i;
	u16 Key = 0;
	for(i = 0; i < DESMUME_NB_KEYS; i++)
		if(keyval == Keypad_Config[i]) break;
	if(i < DESMUME_NB_KEYS)	
		Key = DESMUME_KEYMASK_(i);
	//fprintf(stderr,"K:%d(%d)->%X => [%X]\n", e->keyval, e->hardware_keycode, Key, Cur_Keypad);
	return Key;
}

gboolean  on_wMainW_key_press_event    (GtkWidget *widget, GdkEventKey *event, gpointer user_data) {
	u16 Key = lookup_key(event->keyval);
	if (Key != 0) {
		Cur_Keypad |= Key;
		if(desmume_running()) desmume_keypad(Cur_Keypad);
	}
	return 1;
}

gboolean  on_wMainW_key_release_event  (GtkWidget *widget, GdkEventKey *event, gpointer user_data) {
	u16 Key = lookup_key(event->keyval);
	if (Key != 0) {
		Cur_Keypad &= ~Key;
		if(desmume_running()) desmume_keypad(Cur_Keypad);
	}	
	return 1;
}



/* ***** ***** SCREEN DRAWING ***** ***** */




#define RAW_W 256
#define RAW_H 192*2
#define MAX_SIZE 3
guchar on_screen_image[RAW_W*RAW_H*3*MAX_SIZE*MAX_SIZE];
int inline screen_bytes_size() {
	return RAW_W*RAW_H*3*ScreenCoeff_Size*ScreenCoeff_Size;
}
int inline offset_pixels_lower_screen() {
	return screen_bytes_size()/2;
}

void black_screen () {
	/* removes artifacts when resizing with scanlines */
	memset(on_screen_image,0,screen_bytes_size());
}

void decode_screen () {

	int x,y, m, W,H,L,BL, Pw,Bw,Lw,Cw,Hw;
	u32 image[RAW_H][RAW_W], r,g,b;
	u16 * pixel = (u16*)&GPU_screen;
	guchar * rgb = &on_screen_image[0];

	/* decode colors */
	for (y=0; y<RAW_H; y++) {
		for (x=0; x<RAW_W; x++) {
			r = (*pixel & 0x7C00) << 9;
			g = (*pixel & 0x03E0) << 6;
			b = (*pixel & 0x001F) << 3;
			image[y][x]= r | g | b;
			pixel++;
		}
	}
#define LOOP(a,b,c,d,e,f) \
		Pw=3*ScreenCoeff_Size; \
		L=W*Pw; \
		for (a; b; c) { \
			for (d; e; f) { \
				*rgb = (image[y][x] & 0x0000FF); rgb++; \
				*rgb = (image[y][x] & 0x00FF00)>> 8; rgb++; \
				*rgb = (image[y][x] & 0xFF0000)>> 16; rgb++; \
				/* pixels duplicated for scaling width */ \
				for (m=1; m<ScreenCoeff_Size; m++) { \
					memmove(rgb, rgb-3, 3); \
					rgb += 3; \
				} \
			} \
			/* lines duplicated for scaling height */ \
			for (m=1; m<ScreenCoeff_Size; m++) { \
				memmove(rgb, rgb-L, L); \
				rgb += L; \
			} \
		}
	/* load pixels in buffer accordingly */
	if (ScreenRotate) {
		W=RAW_H/2; H=RAW_W;
		LOOP(x=RAW_W-1, x >= 0, x--, y=0, y < W, y++)
		LOOP(x=RAW_W-1, x >= 0, x--, y=W, y < RAW_H, y++)
	} else {
		H=RAW_H; W=RAW_W;
		LOOP(y=0, y < RAW_H, y++, x=0, x < RAW_W, x++)

	}
}

int screen (GtkWidget * widget, int offset_pix) {
	int H,W,L;
	if (ScreenRotate) {
		W=RAW_H/2; H=RAW_W;
	} else {
		H=RAW_H/2; W=RAW_W;
	}
	L=W*3*ScreenCoeff_Size;
	gdk_draw_rgb_image	(widget->window,
		widget->style->fg_gc[widget->state],0,0, 
		W*ScreenCoeff_Size, H*ScreenCoeff_Size,
		GDK_RGB_DITHER_NONE,on_screen_image+offset_pix,L);
	
	return 1;
}

/* OUTPUT UPPER SCREEN  */
void      on_wDraw_Main_realize       (GtkWidget *widget, gpointer user_data) { }
gboolean  on_wDraw_Main_expose_event  (GtkWidget *widget, GdkEventExpose  *event, gpointer user_data) {
	decode_screen();
	return screen(widget, 0);
}

/* OUTPUT LOWER SCREEN  */
void      on_wDraw_Sub_realize        (GtkWidget *widget, gpointer user_data) { }
gboolean  on_wDraw_Sub_expose_event   (GtkWidget *widget, GdkEventExpose  *event, gpointer user_data) {
	return screen(widget, offset_pixels_lower_screen());
}







/* ***** ***** INPUT STYLUS / MOUSE ***** ***** */

void set_touch_pos (int x, int y) {
	s32 EmuX, EmuY;
	x /= ScreenCoeff_Size;
	y /= ScreenCoeff_Size;
	EmuX = x; EmuY = y;
	if (ScreenRotate) { EmuX = 256-y; EmuY = x; }
	if(EmuX<0) EmuX = 0; else if(EmuX>255) EmuX = 255;
	if(EmuY<0) EmuY = 0; else if(EmuY>192) EmuY = 192;
	NDS_setTouchPos(EmuX, EmuY);
}

gboolean  on_wDraw_Sub_button_press_event   (GtkWidget *widget, GdkEventButton  *event, gpointer user_data) {
	GdkModifierType state;
	gint x,y;
	
	if(desmume_running()) 
	if(event->button == 1) 
	{
		click = TRUE;	
		gdk_window_get_pointer(widget->window, &x, &y, &state);
		if (state & GDK_BUTTON1_MASK)
			set_touch_pos(x,y);
	}	
	return TRUE;
}

gboolean  on_wDraw_Sub_button_release_event (GtkWidget *widget, GdkEventButton  *event, gpointer user_data) {
	if(click) NDS_releasTouch();
	click = FALSE;
	return TRUE;
}

gboolean  on_wDraw_Sub_motion_notify_event  (GtkWidget *widget, GdkEventMotion  *event, gpointer user_data) {
	GdkModifierType state;
	gint x,y;
	
	if(click)
	{
		if(event->is_hint)
			gdk_window_get_pointer(widget->window, &x, &y, &state);
		else
		{
			x= (gint)event->x;
			y= (gint)event->y;
			state=(GdkModifierType)event->state;
		}
		
	//	fprintf(stderr,"X=%d, Y=%d, S&1=%d\n", x,y,state&GDK_BUTTON1_MASK);
	
		if(state & GDK_BUTTON1_MASK)
			set_touch_pos(x,y);
	}
	
	return TRUE;
}




/* ***** ***** KEYBOARD CONFIG / KEY DEFINITION ***** ***** */


const char * DESMUME_KEY_NAMES[DESMUME_NB_KEYS]={
	"A", "B", 
	"Select", "Start",
	"Right", "Left", "Up", "Down",
	"R", "L", "Y", "X", "DEBUG"
};
gint Keypad_Temp[DESMUME_NB_KEYS];
guint temp_Key=0;

void init_labels() {
	int i;
	char text[50], bname[20];
	GtkButton *b;
	for (i=0; i<DESMUME_NB_KEYS; i++) {
		sprintf(text,"%s : %s\0\0",DESMUME_KEY_NAMES[i],KEYNAME(Keypad_Config[i]));
		sprintf(bname,"button_%s\0\0",DESMUME_KEY_NAMES[i]);
		b = (GtkButton*)glade_xml_get_widget(xml, bname);
		gtk_button_set_label(b,text);
	}
}

void edit_controls() {
	GtkDialog * dlg = (GtkDialog*)glade_xml_get_widget(xml, "wKeybConfDlg");
	memcpy(&Keypad_Temp, &Keypad_Config, sizeof(Keypad_Config));
	/* we change the labels so we know keyb def */
	init_labels();
	gtk_widget_show((GtkWidget*)dlg);
}

void  on_wKeybConfDlg_response (GtkDialog *dialog, gint arg1, gpointer user_data) {
	/* overwrite keyb def if user selected ok */
	if (arg1 == GTK_RESPONSE_OK)
		memcpy(&Keypad_Config, &Keypad_Temp, sizeof(Keypad_Config));
	gtk_widget_hide((GtkWidget*)dialog);
}

void inline current_key_label() {
	GtkLabel * lbl = (GtkLabel*)glade_xml_get_widget(xml, "label_key");
	gtk_label_set_text(lbl, KEYNAME(temp_Key));
}

gboolean  on_wKeyDlg_key_press_event (GtkWidget *widget, GdkEventKey *event, gpointer user_data) {
	temp_Key = event->keyval;
	current_key_label();
	return TRUE;
}

void ask(GtkButton*b, int key) {
	char text[50];
	GtkDialog * dlg = (GtkDialog*)glade_xml_get_widget(xml, "wKeyDlg");
	key--; /* key = bit position, start with 1 */
	temp_Key = Keypad_Temp[key];
	current_key_label();
	switch (gtk_dialog_run(dlg))
	{
		case GTK_RESPONSE_OK:
			Keypad_Temp[key]=temp_Key;
			sprintf(text,"%s : %s\0\0",DESMUME_KEY_NAMES[key],KEYNAME(temp_Key));
			gtk_button_set_label(b,text);
			break;
		case GTK_RESPONSE_CANCEL:
		case GTK_RESPONSE_NONE:
			break;
	}
	gtk_widget_hide((GtkWidget*)dlg);
}

void  on_button_Left_clicked    (GtkButton *b, gpointer user_data) { ask(b,DESMUME_KEY_Left); }
void  on_button_Up_clicked      (GtkButton *b, gpointer user_data) { ask(b,DESMUME_KEY_Up); }
void  on_button_Right_clicked   (GtkButton *b, gpointer user_data) { ask(b,DESMUME_KEY_Right); }
void  on_button_Down_clicked    (GtkButton *b, gpointer user_data) { ask(b,DESMUME_KEY_Down); }

void  on_button_L_clicked       (GtkButton *b, gpointer user_data) { ask(b,DESMUME_KEY_L); }
void  on_button_R_clicked       (GtkButton *b, gpointer user_data) { ask(b,DESMUME_KEY_R); }

void  on_button_Y_clicked       (GtkButton *b, gpointer user_data) { ask(b,DESMUME_KEY_Y); }
void  on_button_X_clicked       (GtkButton *b, gpointer user_data) { ask(b,DESMUME_KEY_X); }
void  on_button_A_clicked       (GtkButton *b, gpointer user_data) { ask(b,DESMUME_KEY_A); }
void  on_button_B_clicked       (GtkButton *b, gpointer user_data) { ask(b,DESMUME_KEY_B); }

void  on_button_Start_clicked   (GtkButton *b, gpointer user_data) { ask(b,DESMUME_KEY_Start); }
void  on_button_Select_clicked  (GtkButton *b, gpointer user_data) { ask(b,DESMUME_KEY_Select); }
void  on_button_Debug_clicked   (GtkButton *b, gpointer user_data) { ask(b,DESMUME_KEY_DEBUG); }
