#include "callbacks_IO.h"

static u16 Cur_Keypad = 0;



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




const int offset_pixels_lower_screen = 256*192; // w * h

#define MAX_SIZE 3
guchar on_screen_image[256*192*3*2*MAX_SIZE*MAX_SIZE];

int screen (GtkWidget * widget, int offset_pix) {
/*
	SDL_PixelFormat screenPixFormat;
	SDL_Surface *rawImage, *screenImage;
	
	rawImage = SDL_CreateRGBSurfaceFrom(((char*)&GPU_screen)+offset_pix, 256, 192, 16, 512, 0x001F, 0x03E0, 0x7C00, 0);
	if(rawImage == NULL) return 1;
*/	
	int dx,x,dy,y, W,H,L;
	u32 image[192][256], r,g,b;
	u16 * pixel = (u16*)&GPU_screen + offset_pix;
	guchar * rgb = &on_screen_image[0];

	for (y=0; y<192; y++) {
		for (x=0; x<256; x++) {
			r = (*pixel & 0x7C00) << 9;
			g = (*pixel & 0x03E0) << 6;
			b = (*pixel & 0x001F) << 3;
			image[y][x]= r | g | b;
			pixel++;
		}
	}
	
	W=256; H=192; L=W*3*ScreenCoeff_Size;
	for (y=0; y<H; y++) {
		for (x=0; x<W; x++) {
			*rgb = (image[y][x] & 0x0000FF); rgb++;
			*rgb = (image[y][x] & 0x00FF00)>> 8; rgb++;
			*rgb = (image[y][x] & 0xFF0000)>> 16; rgb++;
			for (dx=1; dx<ScreenCoeff_Size; dx++) {
				memmove(rgb, rgb-3, 3);
				rgb += 3;
			}
		}
		for (dy=1; dy<ScreenCoeff_Size; dy++) {
			memmove(rgb, rgb-L, L);
			rgb += L;
		}
	}
/*	
	screenPixFormat.BitsPerPixel = 24;
	screenPixFormat.BytesPerPixel = 3;
	screenPixFormat.Rshift = 0;
	screenPixFormat.Gshift = 8;
	screenPixFormat.Bshift = 16;
	screenPixFormat.Rmask = 0x0000FF;
	screenPixFormat.Gmask = 0x00FF00;
	screenPixFormat.Bmask = 0xFF0000;
	screenImage = SDL_ConvertSurface(rawImage, &screenPixFormat, 0);
*/

	gdk_draw_rgb_image	(widget->window,
		widget->style->fg_gc[widget->state],0,0, 
		W*ScreenCoeff_Size, H*ScreenCoeff_Size,
		GDK_RGB_DITHER_NONE,on_screen_image,W*3*ScreenCoeff_Size);

/*
	gdk_draw_rgb_image	(widget->window,
		widget->style->fg_gc[widget->state],0,0, 
		screenImage->w*ScreenCoeff_Size, screenImage->h*ScreenCoeff_Size,
		GDK_RGB_DITHER_NONE,(guchar*)screenImage->pixels,256*3);
	SDL_FreeSurface(screenImage);
	SDL_FreeSurface(rawImage);
*/
	
	return 1;
}

/* OUTPUT UPPER SCREEN  */
void      on_wDraw_Main_realize       (GtkWidget *widget, gpointer user_data) { }
gboolean  on_wDraw_Main_expose_event  (GtkWidget *widget, GdkEventExpose  *event, gpointer user_data) {
	return screen(widget, 0);
}

/* OUTPUT LOWER SCREEN  */
void      on_wDraw_Sub_realize        (GtkWidget *widget, gpointer user_data) { }
gboolean  on_wDraw_Sub_expose_event   (GtkWidget *widget, GdkEventExpose  *event, gpointer user_data) {
	return screen(widget, offset_pixels_lower_screen);
}







/* ***** ***** INPUT STYLUS / MOUSE ***** ***** */



gboolean  on_wDraw_Sub_button_press_event   (GtkWidget *widget, GdkEventButton  *event, gpointer user_data) {
	GdkModifierType state;
	gint x,y;
	s32 EmuX, EmuY;
	
	if(desmume_running()) 
	{
		if(event->button == 1) 
		{
			click = TRUE;
			
			gdk_window_get_pointer(widget->window, &x, &y, &state);
			if (state & GDK_BUTTON1_MASK)
			{
				EmuX = x;
				EmuY = y;
				if(EmuX<0) EmuX = 0; else if(EmuX>255) EmuX = 255;
				if(EmuY<0) EmuY = 0; else if(EmuY>192) EmuY = 192;
				NDS_setTouchPos(EmuX, EmuY);
			}
		}
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
	s32 EmuX, EmuY;
	
	
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
		{
			EmuX = x;
			EmuY = y;
			if(EmuX<0) EmuX = 0; else if(EmuX>255) EmuX = 255;
			if(EmuY<0) EmuY = 0; else if(EmuY>192) EmuY = 192;
			NDS_setTouchPos(EmuX, EmuY);
		}
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
