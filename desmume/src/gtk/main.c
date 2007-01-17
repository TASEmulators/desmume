#include "globals.h"
#include "../debug.h"

#include "DeSmuME.xpm"

#define EMULOOP_PRIO (G_PRIORITY_HIGH_IDLE + 20)

/************************ CONFIG FILE *****************************/

// extern char FirmwareFile[256];
// int LoadFirmware(const char *filename);

static void *Open_Select(GtkWidget* widget, gpointer data);
static void Launch();
static void Pause();
static void Printscreen();
static void Reset();

static GtkActionEntry action_entries[] = {
	{ "open",	"gtk-open",		"Open",		"<Ctrl>o",	NULL,	G_CALLBACK(Open_Select) },
	{ "run",	"gtk-media-play",	"Run",		"<Ctrl>r",	NULL,	G_CALLBACK(Launch) },
	{ "pause",	"gtk-media-pause",	"Pause",	"<Ctrl>p",	NULL,	G_CALLBACK(Pause) },
	{ "quit",	"gtk-quit",		"Quit",		"<Ctrl>q",	NULL,	G_CALLBACK(gtk_main_quit) },
	{ "printscreen",NULL,			"Printscreen",	NULL,		NULL,	G_CALLBACK(Printscreen) },
	{ "reset",	"gtk-refresh",		"Reset",	NULL,		NULL,	G_CALLBACK(Reset) }
};

GtkActionGroup * action_group;

char * CONFIG_FILE;

SoundInterface_struct *SNDCoreList[] = {
&SNDDummy,
&SNDFile,
&SNDSDL,
NULL
};

u16 Keypad_Temp[NB_KEYS];

int Write_ConfigFile()
{
	int i;
	GKeyFile * keyfile;
	
	keyfile = g_key_file_new();
	
	for(i = 0; i < NB_KEYS; i++)
	{
		g_key_file_set_integer(keyfile, "KEYS", key_names[i], keyboard_cfg[i]);
		g_key_file_set_integer(keyfile, "JOYKEYS", key_names[i], joypad_cfg[i]);
	}
	
// 	if(FirmwareFile[0])
// 	{
// 		ini_add_section(ini, "FIRMWARE");
// 		ini_add_value(ini, "FIRMWARE", "FILE", FirmwareFile);
// 	}
	
	g_file_set_contents(CONFIG_FILE, g_key_file_to_data(keyfile, 0, 0), -1, 0);

	g_key_file_free(keyfile);
	
	return 0;
}

int Read_ConfigFile()
{
	int i, tmp;
	GKeyFile * keyfile = g_key_file_new();
	GError * error = NULL;
	
	load_default_config();
	
	g_key_file_load_from_file(keyfile, CONFIG_FILE, G_KEY_FILE_NONE, 0);

	const char *c;
	
	/* Load keyboard keys */
	for(i = 0; i < NB_KEYS; i++)
	{
		tmp = g_key_file_get_integer(keyfile, "KEYS", key_names[i], &error);
		if (error != NULL) {
                  g_error_free(error);
                  error = NULL;
		} else {
                  keyboard_cfg[i] = tmp;
		}
	}
		
	/* Load joystick keys */
	for(i = 0; i < NB_KEYS; i++)
	{
		tmp = g_key_file_get_integer(keyfile, "JOYKEYS", key_names[i], &error);
		if (error != NULL) {
                  g_error_free(error);
                  error = NULL;
		} else {
                  joypad_cfg[i] = tmp;
		}
	}

	g_key_file_free(keyfile);
		
	return 0;
}

/************************ GTK *******************************/

uint Frameskip = 0;

//const gint StaCounter[20] = {0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19};

static SDL_Surface *SDL_Screenbuf;
static GdkPixmap *pPixmap;

static GtkWidget *pWindow;
static GtkWidget *pStatusBar;
static GtkWidget *pToolbar;
static GtkWidget *pDrawingArea;

static BOOL regMainLoop = FALSE;

static gint pStatusBar_Ctx;
#define pStatusBar_Change(t) gtk_statusbar_pop(GTK_STATUSBAR(pStatusBar), pStatusBar_Ctx); gtk_statusbar_push(GTK_STATUSBAR(pStatusBar), pStatusBar_Ctx, t);

gboolean EmuLoop(gpointer data);

void About(GtkWidget* widget, gpointer data)
{
	GdkPixbuf * pixbuf = gdk_pixbuf_new_from_xpm_data(DeSmuME_xpm);
	
	gtk_show_about_dialog(GTK_WINDOW(pWindow),
			"name", "DeSmuME",
			"version", VERSION,
			"website", "http://desmume.sf.net",
			"logo", pixbuf,
			"comments", "Nintendo DS emulator based on work by Yopyop",
			NULL);

	g_object_unref(pixbuf);
}

static int Open(const char *filename)
{
        int i = NDS_LoadROM(filename, MC_TYPE_AUTODETECT, 1);
	return i;
}

static void Launch()
{
	desmume_resume();
	
	if(!regMainLoop)
	{
		g_idle_add_full(EMULOOP_PRIO, &EmuLoop, pWindow, NULL); regMainLoop = TRUE;
	}
	
	pStatusBar_Change("Running ...");

	gtk_action_set_sensitive(gtk_action_group_get_action(action_group, "pause"), TRUE);
	gtk_action_set_sensitive(gtk_action_group_get_action(action_group, "run"), FALSE);
}

static void Pause()
{
	desmume_pause();
	pStatusBar_Change("Paused");

	gtk_action_set_sensitive(gtk_action_group_get_action(action_group, "pause"), FALSE);
	gtk_action_set_sensitive(gtk_action_group_get_action(action_group, "run"), TRUE);
}

/* Choose a file then load it */
static void *Open_Select(GtkWidget* widget, gpointer data)
{
	Pause();
	
	GtkFileFilter *pFilter_nds, *pFilter_dsgba, *pFilter_any;
	GtkWidget *pFileSelection;
	GtkWidget *pParent;
	gchar *sChemin;

	pParent = GTK_WIDGET(data);
	
	pFilter_nds = gtk_file_filter_new();
	gtk_file_filter_add_pattern(pFilter_nds, "*.nds");
#ifdef HAVE_LIBZ
	gtk_file_filter_add_pattern(pFilter_nds, "*.nds.gz");
#endif
#ifdef HAVE_LIBZZIP
	gtk_file_filter_add_pattern(pFilter_nds, "*.nds.zip");
#endif
	gtk_file_filter_set_name(pFilter_nds, "Nds binary (.nds)");
	
	pFilter_dsgba = gtk_file_filter_new();
	gtk_file_filter_add_pattern(pFilter_dsgba, "*.ds.gba");
	gtk_file_filter_set_name(pFilter_dsgba, "Nds binary with loader (.ds.gba)");
	
	pFilter_any = gtk_file_filter_new();
	gtk_file_filter_add_pattern(pFilter_any, "*");
	gtk_file_filter_set_name(pFilter_any, "All files");

	/* Creating the selection window */
	pFileSelection = gtk_file_chooser_dialog_new("Open...",
			GTK_WINDOW(pParent),
			GTK_FILE_CHOOSER_ACTION_OPEN,
			GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
			GTK_STOCK_OPEN, GTK_RESPONSE_OK,
			NULL);
	/* On limite les actions a cette fenetre */
	gtk_window_set_modal(GTK_WINDOW(pFileSelection), TRUE);

	gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(pFileSelection), pFilter_nds);
	gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(pFileSelection), pFilter_dsgba);
	gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(pFileSelection), pFilter_any);
	
	/* Affichage fenetre */
	switch(gtk_dialog_run(GTK_DIALOG(pFileSelection)))
	{
		case GTK_RESPONSE_OK:
			/* Recuperation du chemin */
			sChemin = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(pFileSelection));
			if(Open((const char*)sChemin) < 0)
			{
				GtkWidget *pDialog = gtk_message_dialog_new(GTK_WINDOW(pFileSelection),
						GTK_DIALOG_MODAL,
						GTK_MESSAGE_ERROR,
						GTK_BUTTONS_OK,
						"Unable to load :\n%s", sChemin);
				gtk_dialog_run(GTK_DIALOG(pDialog));
				gtk_widget_destroy(pDialog);
			}
			
			//Launch(NULL, pWindow);
			
			g_free(sChemin);
			break;
		default:
			break;
	}
	gtk_widget_destroy(pFileSelection);
	
	// FIXME: should be set sensitive only if a file was really loaded
	gtk_action_set_sensitive(gtk_action_group_get_action(action_group, "run"), TRUE);
	gtk_action_set_sensitive(gtk_action_group_get_action(action_group, "reset"), TRUE);
}

static void Close()
{
}

static void Reset()
{
        NDS_Reset();
	desmume_resume();
	
	pStatusBar_Change("Running ...");
}

int ScreenCoeff_Size;

/////////////////////////////// DRAWING SCREEN //////////////////////////////////

/* Drawing callback */
int gtkFloatExposeEvent (GtkWidget *widget, GdkEventExpose *event, gpointer data)
{
	SDL_PixelFormat screenPixFormat;
	SDL_Surface *rawImage, *screenImage;
	
	rawImage = SDL_CreateRGBSurfaceFrom((void*)&GPU_screen, 256, 192*2, 16, 512, 0x001F, 0x03E0, 0x7C00, 0);
	if(rawImage == NULL) return 1;
	
	memcpy(&screenPixFormat, rawImage->format, sizeof(SDL_PixelFormat));
	
	screenPixFormat.BitsPerPixel = 24;
	screenPixFormat.BytesPerPixel = 3;
	screenPixFormat.Rshift = 0;
	screenPixFormat.Gshift = 8;
	screenPixFormat.Bshift = 16;
	screenPixFormat.Rmask = 0x0000FF;
	screenPixFormat.Gmask = 0x00FF00;
	screenPixFormat.Bmask = 0xFF0000;
	
	screenImage = SDL_ConvertSurface(rawImage, &screenPixFormat, 0);
  
	gdk_draw_rgb_image	(widget->window,
							  widget->style->fg_gc[widget->state],0,0,screenImage->w, screenImage->h,
							  GDK_RGB_DITHER_NONE,(guchar*)screenImage->pixels,screenImage->pitch);
	SDL_FreeSurface(screenImage);
	SDL_FreeSurface(rawImage);
	
	return 1;
}

static void Draw()
{
}

/////////////////////////////// KEYS AND STYLUS UPDATE ///////////////////////////////////////

static gboolean Stylus_Move(GtkWidget *w, GdkEventMotion *e, gpointer data)
{
	GdkModifierType state;
	gint x,y;
	s32 EmuX, EmuY;
	
	
	if(click)
	{
		if(e->is_hint)
			gdk_window_get_pointer(w->window, &x, &y, &state);
		else
		{
			x= (gint)e->x;
			y= (gint)e->y;
			state=(GdkModifierType)e->state;
		}
		
	//	fprintf(stderr,"X=%d, Y=%d, S&1=%d\n", x,y,state&GDK_BUTTON1_MASK);
	
		if(y >= 192 && (state & GDK_BUTTON1_MASK))
		{
			EmuX = x;
			EmuY = y - 192;
			if(EmuX<0) EmuX = 0; else if(EmuX>255) EmuX = 255;
			if(EmuY<0) EmuY = 0; else if(EmuY>192) EmuY = 192;
			NDS_setTouchPos(EmuX, EmuY);
		}
	}
	
	return TRUE;
}
static gboolean Stylus_Press(GtkWidget *w, GdkEventButton *e, gpointer data)
{
	GdkModifierType state;
	gint x,y;
	s32 EmuX, EmuY;
	
	if(desmume_running()) 
	{
		if(e->button == 1) 
		{
			click = TRUE;
			
			gdk_window_get_pointer(w->window, &x, &y, &state);
			if(y >= 192 && (state & GDK_BUTTON1_MASK))
			{
				EmuX = x;
				EmuY = y - 192;
				if(EmuX<0) EmuX = 0; else if(EmuX>255) EmuX = 255;
				if(EmuY<0) EmuY = 0; else if(EmuY>192) EmuY = 192;
				NDS_setTouchPos(EmuX, EmuY);
			}
		}
	}
	
	return TRUE;
}
static gboolean Stylus_Release(GtkWidget *w, GdkEventButton *e, gpointer data)
{
	if(click) NDS_releasTouch();
	click = FALSE;
	return TRUE;
}

static u16 Cur_Keypad = 0;

static gint Key_Press(GtkWidget *w, GdkEventKey *e)
{
	int i;
	u16 Key = 0;
	
	for(i = 0; i < NB_KEYS; i++)
		if(e->keyval == keyboard_cfg[i]) break;
	
	if(i < NB_KEYS)
	{
		ADD_KEY( Cur_Keypad, KEYMASK_(i) );
		if(desmume_running()) update_keypad(Cur_Keypad);
	}
	
	return 1;
}
static gint Key_Release(GtkWidget *w, GdkEventKey *e)
{
	int i;
	u16 Key = 0;
	
	for(i = 0; i < NB_KEYS; i++)
		if(e->keyval == keyboard_cfg[i]) break;
	
	if(i < NB_KEYS)
	{
		RM_KEY( Cur_Keypad, KEYMASK_(i) );
		if(desmume_running()) update_keypad(Cur_Keypad);
	}
	
	return 1;
}

/////////////////////////////// CONTROLS EDIT //////////////////////////////////////

GtkWidget *mkLabel;
gint Modify_Key_Chosen = 0;

void Modify_Key_Press(GtkWidget *w, GdkEventKey *e)
{
	char YouPressed[128];
	Modify_Key_Chosen = e->keyval;
	sprintf(YouPressed, "You pressed : %d\nClick OK to keep this key.", e->keyval);
	gtk_label_set(GTK_LABEL(mkLabel), YouPressed);
}

void Modify_Key(GtkWidget* widget, gpointer data)
{
	gint Key = GPOINTER_TO_INT(data);
	char Title[64];
	char Key_Label[64];
	
	GtkWidget *mkDialog;
	
	sprintf(Title, "Press \"%s\" key ...\n", key_names[Key]);
	mkDialog = gtk_dialog_new_with_buttons(Title,
		GTK_WINDOW(pWindow),
		GTK_DIALOG_MODAL,
		GTK_STOCK_OK,GTK_RESPONSE_OK,
		GTK_STOCK_CANCEL,GTK_RESPONSE_CANCEL,
		NULL);
	
	g_signal_connect(G_OBJECT(mkDialog), "key_press_event", G_CALLBACK(Modify_Key_Press), NULL);
	
	mkLabel = gtk_label_new(Title);
	gtk_box_pack_start(GTK_BOX(GTK_DIALOG(mkDialog)->vbox), mkLabel,TRUE, FALSE, 0);	
	
	gtk_widget_show_all(GTK_DIALOG(mkDialog)->vbox);
	
	switch(gtk_dialog_run(GTK_DIALOG(mkDialog)))
	{
		case GTK_RESPONSE_OK:
			
			Keypad_Temp[Key] = Modify_Key_Chosen;
			sprintf(Key_Label, "%s (%d)", key_names[Key], Keypad_Temp[Key]);
			gtk_button_set_label(GTK_BUTTON(widget), Key_Label);
			
			break;
		case GTK_RESPONSE_CANCEL:
		case GTK_RESPONSE_NONE:
			Modify_Key_Chosen = 0;
			break;
	}
	
	gtk_widget_destroy(mkDialog);
	
}

void Edit_Controls(GtkWidget* widget, gpointer data)
{
	char Key_Label[64];
	int i;
	GtkWidget *ecDialog;
	GtkWidget *ecKey;
	
	memcpy(&Keypad_Temp, &keyboard_cfg, sizeof(keyboard_cfg));
	
	ecDialog = gtk_dialog_new_with_buttons("Edit controls",
													 GTK_WINDOW(pWindow),
													 GTK_DIALOG_MODAL,
													 GTK_STOCK_OK,GTK_RESPONSE_OK,
													 GTK_STOCK_CANCEL,GTK_RESPONSE_CANCEL,
													 NULL);
	
	
	for(i = 0; i < NB_KEYS; i++)
	{
		sprintf(Key_Label, "%s (%d)", key_names[i], Keypad_Temp[i]);
		ecKey = gtk_button_new_with_label(Key_Label);
		g_signal_connect(G_OBJECT(ecKey), "clicked", G_CALLBACK(Modify_Key), GINT_TO_POINTER(i));
		gtk_box_pack_start(GTK_BOX(GTK_DIALOG(ecDialog)->vbox), ecKey,TRUE, FALSE, 0);	
	}

	gtk_widget_show_all(GTK_DIALOG(ecDialog)->vbox);
	
	switch (gtk_dialog_run(GTK_DIALOG(ecDialog)))
	{
		case GTK_RESPONSE_OK:
			memcpy(&keyboard_cfg, &Keypad_Temp, sizeof(keyboard_cfg));
		case GTK_RESPONSE_CANCEL:
		case GTK_RESPONSE_NONE:
			break;
	}
	gtk_widget_destroy(ecDialog);

}

///////////////////////////////// SCREEN SCALING //////////////////////////////

#define MAX_SCREENCOEFF 4 

void Modify_ScreenCoeff(GtkWidget* widget, gpointer data)
{
	int Size = GPOINTER_TO_INT(data);
	
	gtk_drawing_area_size(GTK_DRAWING_AREA(pDrawingArea), 256 * Size, 384 * Size);
	gtk_widget_set_usize (pDrawingArea, 256 * Size, 384 * Size);
	
	ScreenCoeff_Size = Size;
	
}

/////////////////////////////// LAYER HIDING /////////////////////////////////

void Modify_Layer(GtkWidget* widget, gpointer data)
{
	int i;
	gchar *Layer = (gchar*)data;
	
	if(!desmume_running())
	{
		return;
	}
	
	if(memcmp(Layer, "Sub", 3) == 0)
	{
		if(memcmp(Layer, "Sub BG", 6) == 0) 
		{
			i = atoi(Layer + strlen("Sub BG "));
			if(gtk_check_menu_item_get_active(GTK_CHECK_MENU_ITEM(widget)) == TRUE) {
				if(!SubScreen.gpu->dispBG[i]) GPU_addBack(SubScreen.gpu, i);
			} else { 
				if(SubScreen.gpu->dispBG[i]) GPU_remove(SubScreen.gpu, i); }
		}
		else
		{
			/* TODO: Disable sprites */
		}
	}
	else
	{
		if(memcmp(Layer, "Main BG", 7) == 0) 
		{
			i = atoi(Layer + strlen("Main BG "));
			if(gtk_check_menu_item_get_active(GTK_CHECK_MENU_ITEM(widget)) == TRUE) {
				if(!MainScreen.gpu->dispBG[i]) GPU_addBack(MainScreen.gpu, i);
			} else { 
				if(MainScreen.gpu->dispBG[i]) GPU_remove(MainScreen.gpu, i); }
		}
		else
		{
			/* TODO: Disable sprites */
		}
	}
	//fprintf(stderr,"Changed %s to %d\n",Layer,gtk_check_menu_item_get_active(GTK_CHECK_MENU_ITEM(widget)));
}

const char *Layers_Menu[10] =
{
	"Main BG 0",
	"Main BG 1",
	"Main BG 2",
	"Main BG 3",
	"Main OBJ",
	"SUB BG 0",
	"SUB BG 1",
	"SUB BG 2",
	"SUB BG 3",
	"SUB OBJ",
};

/////////////////////////////// PRINTSCREEN /////////////////////////////////
// TODO: improve (let choose filename, and use png)

typedef struct
{
    u32 header_size;
    s32 width;
    s32 height;
    u16 r1;
    u16 depth;
    u32 r2;
    u32 size;
    s32 r3,r4;
    u32 r5,r6;
}BmpImageHeader;

typedef struct
{
    u16 type;
    u32 size;
    u16 r1, r2;
    u32 data_offset;
}BmpFileHeader;


int WriteBMP(const char *filename,u16 *bmp){
    BmpFileHeader  fileheader;
    BmpImageHeader imageheader;
    fileheader.size = 14;
    fileheader.type = 0x4D42;
    fileheader.r1 = 0;
    fileheader.r2=0;
    fileheader.data_offset = 14+40;
    
    imageheader.header_size = 40;
    imageheader.r1 = 1;
    imageheader.depth = 24;
    imageheader.r2 = 0;
    imageheader.size = 256 * 192*2 * 3;
    imageheader.r3 = 0;
    imageheader.r4 = 0;
    imageheader.r5 = 0;
    imageheader.r6 = 0;
    imageheader.width = 256;
    imageheader.height = 192*2;
    
    FILE *fichier = fopen(filename,"wb");
    //fwrite(&fileheader, 1, 14, fichier);

    //fwrite(&imageheader, 1, 40, fichier);
    fwrite( &fileheader.type,  sizeof(fileheader.type),  1, fichier);
    fwrite( &fileheader.size,  sizeof(fileheader.size),  1, fichier);
    fwrite( &fileheader.r1,  sizeof(fileheader.r1),  1, fichier);
    fwrite( &fileheader.r2,  sizeof(fileheader.r2),  1, fichier);
    fwrite( &fileheader.data_offset,  sizeof(fileheader.data_offset),  1, fichier);
    fwrite( &imageheader.header_size, sizeof(imageheader.header_size), 1, fichier);
    fwrite( &imageheader.width, sizeof(imageheader.width), 1, fichier);
    fwrite( &imageheader.height, sizeof(imageheader.height), 1, fichier);
    fwrite( &imageheader.r1, sizeof(imageheader.r1), 1, fichier);
    fwrite( &imageheader.depth, sizeof(imageheader.depth), 1, fichier);
    fwrite( &imageheader.r2, sizeof(imageheader.r2), 1, fichier);
    fwrite( &imageheader.size, sizeof(imageheader.size), 1, fichier);
    fwrite( &imageheader.r3, sizeof(imageheader.r3), 1, fichier);
    fwrite( &imageheader.r4, sizeof(imageheader.r4), 1, fichier);
    fwrite( &imageheader.r5, sizeof(imageheader.r5), 1, fichier);
    fwrite( &imageheader.r6, sizeof(imageheader.r6), 1, fichier);
    int i,j,k;
    for(j=0;j<192*2;j++)for(i=0;i<256;i++){
    u8 r,g,b;
    u16 pixel = bmp[i+(192*2-j)*256];
    r = pixel>>10;
    pixel-=r<<10;
    g = pixel>>5;
    pixel-=g<<5;
    b = pixel;
    r*=255/31;
    g*=255/31;
    b*=255/31;
    fwrite(&r, 1, sizeof(char), fichier); 
    fwrite(&g, 1, sizeof(char), fichier); 
    fwrite(&b, 1, sizeof(char), fichier); 
}
    fclose(fichier);
return 1;
}

static void Printscreen()
{
	WriteBMP("./test.bmp",GPU_screen);
}

/////////////////////////////// DS CONFIGURATION //////////////////////////////////

#if 0

char FirmwareFile[256];

int LoadFirmware(const char *filename)
{
	int i;
	u32 size;
	FILE *f;
	
	strncpy(FirmwareFile, filename, 256);
	
	f = fopen(filename, "rb");
	if(!f) return -1;
	
	fseek(f, 0, SEEK_END);
	size = ftell(f);
	fseek(f, 0, SEEK_SET);
	
	if(size > MMU.spi7.fw.size) { fclose(f); return -1; }		/* this must be a small file*/
	
	i = fread(MMU.spi7.fw.data, size, 1, f);
	
	fclose(f);
	
	return i;
}

int SelectFirmwareFile_Load(GtkWidget *w, gpointer data)
{
	GtkFileFilter *pFilter_nds, *pFilter_bin, *pFilter_any;
	GtkWidget *pFileSelection;
	GtkWidget *pParent;
	gchar *sChemin;

	BOOL oldState = desmume_running();
	Pause();
	
	pParent = GTK_WIDGET(data);
	
	pFilter_nds = gtk_file_filter_new();
	gtk_file_filter_add_pattern(pFilter_nds, "*.nds");
	gtk_file_filter_set_name(pFilter_nds, "Nds binary (.nds)");
	
	pFilter_bin = gtk_file_filter_new();
	gtk_file_filter_add_pattern(pFilter_bin, "*.bin");
	gtk_file_filter_set_name(pFilter_bin, "Binary file (.bin)");
	
	pFilter_any = gtk_file_filter_new();
	gtk_file_filter_add_pattern(pFilter_any, "*");
	gtk_file_filter_set_name(pFilter_any, "All files");

	/* Creation de la fenetre de selection */
	pFileSelection = gtk_file_chooser_dialog_new("Load firmware...",
			GTK_WINDOW(pParent),
			GTK_FILE_CHOOSER_ACTION_OPEN,
			GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
			GTK_STOCK_OPEN, GTK_RESPONSE_OK,
			NULL);
	/* On limite les actions a cette fenetre */
	gtk_window_set_modal(GTK_WINDOW(pFileSelection), TRUE);

	gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(pFileSelection), pFilter_nds);
	gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(pFileSelection), pFilter_bin);
	gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(pFileSelection), pFilter_any);
	
	if(FirmwareFile[0]) gtk_file_chooser_select_uri(GTK_FILE_CHOOSER(pFileSelection), FirmwareFile);
	
	/* Affichage fenetre */
	switch(gtk_dialog_run(GTK_DIALOG(pFileSelection)))
	{
		case GTK_RESPONSE_OK:
			/* Recuperation du chemin */
			sChemin = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(pFileSelection));
			if(LoadFirmware((const char*)sChemin) < 0)
			{
				GtkWidget *pDialog = gtk_message_dialog_new(GTK_WINDOW(pFileSelection), GTK_DIALOG_MODAL, GTK_MESSAGE_ERROR, GTK_BUTTONS_OK, "Unable to load :\n%s", sChemin);
				gtk_dialog_run(GTK_DIALOG(pDialog));
				gtk_widget_destroy(pDialog);
			}
			else
			{
				GtkWidget *pDialog = gtk_message_dialog_new(GTK_WINDOW(pFileSelection), GTK_DIALOG_MODAL, GTK_MESSAGE_INFO, GTK_BUTTONS_OK, "Selected firmware :\n%s", sChemin);
				gtk_dialog_run(GTK_DIALOG(pDialog));
				gtk_widget_destroy(pDialog);
			}

			g_free(sChemin);
			break;
		default:
			break;
	}
	gtk_widget_destroy(pFileSelection);
	
	if(oldState) Launch();
	
}

int SelectFirmwareFile_Load(GtkWidget *w, gpointer data)
{
	GtkFileFilter *pFilter_nds, *pFilter_bin, *pFilter_any;
	GtkWidget *pFileSelection;
	GtkWidget *pParent;
	gchar *sChemin;

	BOOL oldState = desmume_running();
	Pause();
	
	pParent = GTK_WIDGET(data);
	
	pFilter_nds = gtk_file_filter_new();
	gtk_file_filter_add_pattern(pFilter_nds, "*.nds");
	gtk_file_filter_set_name(pFilter_nds, "Nds binary (.nds)");
	
	pFilter_bin = gtk_file_filter_new();
	gtk_file_filter_add_pattern(pFilter_bin, "*.bin");
	gtk_file_filter_set_name(pFilter_bin, "Binary file (.bin)");
	
	pFilter_any = gtk_file_filter_new();
	gtk_file_filter_add_pattern(pFilter_any, "*");
	gtk_file_filter_set_name(pFilter_any, "All files");

	/* Creation de la fenetre de selection */
	pFileSelection = gtk_file_chooser_dialog_new("Save firmware...",
			GTK_WINDOW(pParent),
			GTK_FILE_CHOOSER_ACTION_OPEN,
			GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
			GTK_STOCK_SAVE, GTK_RESPONSE_OK,
			NULL);
	/* On limite les actions a cette fenetre */
	gtk_window_set_modal(GTK_WINDOW(pFileSelection), TRUE);

	gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(pFileSelection), pFilter_nds);
	gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(pFileSelection), pFilter_bin);
	gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(pFileSelection), pFilter_any);
	
	if(FirmwareFile[0]) gtk_file_chooser_select_uri(GTK_FILE_CHOOSER(pFileSelection), FirmwareFile);
	
	/* Affichage fenetre */
	switch(gtk_dialog_run(GTK_DIALOG(pFileSelection)))
	{
		case GTK_RESPONSE_OK:
			/* Recuperation du chemin */
			sChemin = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(pFileSelection));
			if(LoadFirmware((const char*)sChemin) < 0)
			{
				GtkWidget *pDialog = gtk_message_dialog_new(GTK_WINDOW(pFileSelection), GTK_DIALOG_MODAL, GTK_MESSAGE_ERROR, GTK_BUTTONS_OK, "Unable to load :\n%s", sChemin);
				gtk_dialog_run(GTK_DIALOG(pDialog));
				gtk_widget_destroy(pDialog);
			}
			else
			{
				GtkWidget *pDialog = gtk_message_dialog_new(GTK_WINDOW(pFileSelection), GTK_DIALOG_MODAL, GTK_MESSAGE_INFO, GTK_BUTTONS_OK, "Selected firmware :\n%s", sChemin);
				gtk_dialog_run(GTK_DIALOG(pDialog));
				gtk_widget_destroy(pDialog);
			}

			g_free(sChemin);
			break;
		default:
			break;
	}
	gtk_widget_destroy(pFileSelection);
	
	if(oldState) Launch();
	
}

#endif

/////////////////////////////// FRAMESKIP /////////////////////////////////

#define MAX_FRAMESKIP 10

void Modify_Frameskip(GtkWidget *widget, gpointer data)
{
	Frameskip = GPOINTER_TO_INT(data);
}

/////////////////////////////// TOOLS MANAGEMENT ///////////////////////////////

#include "dTool.h"

extern const dTool_t *dTools_list[];
extern const int dTools_list_size;

BOOL *dTools_running;

void Start_dTool(GtkWidget *widget, gpointer data)
{
	int tool = GPOINTER_TO_INT(data);
	
	if(dTools_running[tool]) return;
	
	dTools_list[tool]->open(tool);
	dTools_running[tool] = TRUE;
}

void dTool_CloseCallback(int tool)
{
	dTools_running[tool] = FALSE;
}

/////////////////////////////// MAIN EMULATOR LOOP ///////////////////////////////

static inline void _updateDTools()
{
	int i;
	for(i = 0; i < dTools_list_size; i++)
	{
		if(dTools_running[i]) { dTools_list[i]->update(); }
	}
}

Uint32 fps, fps_SecStart, fps_FrameCount;

gboolean EmuLoop(gpointer data)
{
	int i;
	
	if(desmume_running())	/* Si on est en train d'executer le programme ... */
	{
		
		fps_FrameCount += Frameskip + 1;
		if(!fps_SecStart) fps_SecStart = SDL_GetTicks();
		if(SDL_GetTicks() - fps_SecStart >= 1000)
		{
			fps_SecStart = SDL_GetTicks();
			fps = fps_FrameCount;
			fps_FrameCount = 0;
			
			char Title[32];
			sprintf(Title, "Desmume - %dfps", fps);
			gtk_window_set_title(GTK_WINDOW(pWindow), Title);
		}
		
		desmume_cycle();	/* Emule ! */
		for(i = 0; i < Frameskip; i++) desmume_cycle();	/* cycles supplémentaires pour le frameskip */
		
		Draw();
		
		_updateDTools();
		gtk_widget_queue_draw(pDrawingArea);
		
		return TRUE;
	}
	
	regMainLoop = FALSE;
	return FALSE;
}


/////////////////////////////// MAIN ///////////////////////////////

#ifdef WIN32
int WinMain (HINSTANCE hThisInstance, HINSTANCE hPrevInstance, LPSTR lpszArgument, int nFunsterStil)
{
	main(0,NULL);
}
#endif

static void dui_set_accel_group(gpointer action, gpointer group) {
        gtk_action_set_accel_group(action, group);
}

int main (int argc, char *argv[])
{
	int i;
	
	const char *commandLine_File = NULL;
	GtkWidget *pVBox;
	GtkWidget *pMenuBar;
	GtkWidget *pMenu, *pSubMenu;
	GtkWidget *pMenuItem, *pSubMenuItem;
	GtkAccelGroup * accel_group;
       
	if(argc == 2) commandLine_File = argv[1];
	
#ifdef DEBUG
        LogStart();
#endif

	gtk_init(&argc, &argv);
	if(SDL_Init(SDL_INIT_VIDEO) == -1)
          {
            fprintf(stderr, "Error trying to initialize SDL: %s\n",
                    SDL_GetError());
            return 1;
          }
	desmume_init();
        /* Initialize joysticks */
        if(!init_joy()) return 1;
	
 	dTools_running = (BOOL*)malloc(sizeof(BOOL) * dTools_list_size);
	for(i=0; i<dTools_list_size; i++) dTools_running[i]=FALSE;

	CONFIG_FILE = g_build_filename(g_get_home_dir(), ".desmume.ini", NULL);
	Read_ConfigFile();
	
	/* Creation de la fenetre */
	pWindow = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	gtk_window_set_title(GTK_WINDOW(pWindow), "Desmume");
	gtk_window_set_policy (GTK_WINDOW (pWindow), FALSE, FALSE, FALSE);
	gtk_window_set_icon(GTK_WINDOW (pWindow), gdk_pixbuf_new_from_xpm_data(DeSmuME_xpm));
	
	g_signal_connect(G_OBJECT(pWindow), "destroy", G_CALLBACK(gtk_main_quit), NULL);
	g_signal_connect(G_OBJECT(pWindow), "key_press_event", G_CALLBACK(Key_Press), NULL);
	g_signal_connect(G_OBJECT(pWindow), "key_release_event", G_CALLBACK(Key_Release), NULL);

	/* Creation de la GtkVBox */
	pVBox = gtk_vbox_new(FALSE, 0);
	gtk_container_add(GTK_CONTAINER(pWindow), pVBox);

	accel_group = gtk_accel_group_new();
	action_group = gtk_action_group_new("dui");
	gtk_action_group_add_actions(action_group, action_entries, sizeof(action_entries) / sizeof(GtkActionEntry), pWindow);
        {
                GList * list = gtk_action_group_list_actions(action_group);
                g_list_foreach(list, dui_set_accel_group, accel_group);
        }
	gtk_window_add_accel_group(GTK_WINDOW(pWindow), accel_group);
	gtk_action_set_sensitive(gtk_action_group_get_action(action_group, "pause"), FALSE);
	gtk_action_set_sensitive(gtk_action_group_get_action(action_group, "run"), FALSE);
	gtk_action_set_sensitive(gtk_action_group_get_action(action_group, "reset"), FALSE);
	gtk_action_set_sensitive(gtk_action_group_get_action(action_group, "printscreen"), FALSE);

	/**** Creation du menu ****/

	pMenuBar = gtk_menu_bar_new();
	
	/** Menu "Fichier" **/

	pMenu = gtk_menu_new();

	gtk_container_add(GTK_CONTAINER(pMenu), gtk_action_create_menu_item(gtk_action_group_get_action(action_group, "open")));
	gtk_container_add(GTK_CONTAINER(pMenu), gtk_action_create_menu_item(gtk_action_group_get_action(action_group, "printscreen")));
	gtk_container_add(GTK_CONTAINER(pMenu), gtk_action_create_menu_item(gtk_action_group_get_action(action_group, "quit")));
	
	pMenuItem = gtk_menu_item_new_with_label("File");
	gtk_menu_item_set_submenu(GTK_MENU_ITEM(pMenuItem), pMenu);
	gtk_menu_shell_append(GTK_MENU_SHELL(pMenuBar), pMenuItem);

	/** Menu "Emulation" **/
	GtkWidget *mEmulation;
		GtkWidget *mFrameskip;
			GtkWidget *mFrameskip_Radio[MAX_FRAMESKIP];
		GtkWidget *mGraphics;
			GtkWidget *mSize;
				GtkWidget *mSize_Radio[MAX_SCREENCOEFF];
			GtkWidget *mLayers;
				GtkWidget *mLayers_Radio[10];
	
	
	mEmulation = gtk_menu_new();
	pMenuItem = gtk_menu_item_new_with_label("Emulation");
	gtk_menu_item_set_submenu(GTK_MENU_ITEM(pMenuItem), mEmulation);
	gtk_menu_shell_append(GTK_MENU_SHELL(pMenuBar), pMenuItem);
	
	gtk_container_add(GTK_CONTAINER(mEmulation), gtk_action_create_menu_item(gtk_action_group_get_action(action_group, "run")));
	
	gtk_container_add(GTK_CONTAINER(mEmulation), gtk_action_create_menu_item(gtk_action_group_get_action(action_group, "pause")));

	gtk_container_add(GTK_CONTAINER(mEmulation), gtk_action_create_menu_item(gtk_action_group_get_action(action_group, "reset")));
	
		mFrameskip = gtk_menu_new();
		pMenuItem = gtk_menu_item_new_with_label("Frameskip");
		gtk_menu_item_set_submenu(GTK_MENU_ITEM(pMenuItem), mFrameskip);
		gtk_menu_shell_append(GTK_MENU_SHELL(mEmulation), pMenuItem);
		
		for(i = 0; i < MAX_FRAMESKIP; i++) {
			char frameskipRadio_buf[16];
			sprintf(frameskipRadio_buf, "%d", i);
			if(i>0) mFrameskip_Radio[i] = gtk_radio_menu_item_new_with_label_from_widget(GTK_RADIO_MENU_ITEM(mFrameskip_Radio[i-1]), frameskipRadio_buf);
			else mFrameskip_Radio[i] = gtk_radio_menu_item_new_with_label(NULL, frameskipRadio_buf);
			g_signal_connect(G_OBJECT(mFrameskip_Radio[i]), "activate", G_CALLBACK(Modify_Frameskip), GINT_TO_POINTER(i));
			gtk_menu_shell_append(GTK_MENU_SHELL(mFrameskip), mFrameskip_Radio[i]);
		}
		gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(mFrameskip_Radio[0]), TRUE);
		
		mGraphics = gtk_menu_new();
		pMenuItem = gtk_menu_item_new_with_label("Graphics");
		gtk_menu_item_set_submenu(GTK_MENU_ITEM(pMenuItem), mGraphics);
		gtk_menu_shell_append(GTK_MENU_SHELL(mEmulation), pMenuItem);
			
// TODO: Un jour, peut être... ><
			mSize = gtk_menu_new();
			pMenuItem = gtk_menu_item_new_with_label("Size");
			gtk_menu_item_set_submenu(GTK_MENU_ITEM(pMenuItem), mSize);
			gtk_menu_shell_append(GTK_MENU_SHELL(mGraphics), pMenuItem);
			
			for(i = 1; i < MAX_SCREENCOEFF; i++) {
				char sizeRadio_buf[16];
				sprintf(sizeRadio_buf, "x%d", i);
				if(i>1) mSize_Radio[i] = gtk_radio_menu_item_new_with_label_from_widget(GTK_RADIO_MENU_ITEM(mSize_Radio[i-1]), sizeRadio_buf);
				else mSize_Radio[i] = gtk_radio_menu_item_new_with_label(NULL, sizeRadio_buf);
				g_signal_connect(G_OBJECT(mSize_Radio[i]), "activate", G_CALLBACK(Modify_ScreenCoeff), GINT_TO_POINTER(i));
				gtk_menu_shell_append(GTK_MENU_SHELL(mSize), mSize_Radio[i]);
			}
			gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(mFrameskip_Radio[0]), TRUE);
		
			mLayers = gtk_menu_new();
			pMenuItem = gtk_menu_item_new_with_label("Layers");
			gtk_menu_item_set_submenu(GTK_MENU_ITEM(pMenuItem), mLayers);
			gtk_menu_shell_append(GTK_MENU_SHELL(mGraphics), pMenuItem);
		
			for(i = 0; i < 10; i++) {
				mLayers_Radio[i] = gtk_check_menu_item_new_with_label(Layers_Menu[i]);
				g_signal_connect(G_OBJECT(mLayers_Radio[i]), "activate", G_CALLBACK(Modify_Layer), (void*)Layers_Menu[i]);
				gtk_menu_shell_append(GTK_MENU_SHELL(mLayers), mLayers_Radio[i]);
				gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(mLayers_Radio[i]), TRUE);
			}
			
	
	/** Menu "Options" **/
	GtkWidget *mConfig = gtk_menu_new();
	pMenuItem = gtk_menu_item_new_with_label("Config");
	gtk_menu_item_set_submenu(GTK_MENU_ITEM(pMenuItem), mConfig);
	gtk_menu_shell_append(GTK_MENU_SHELL(pMenuBar), pMenuItem);
	
	pMenuItem = gtk_menu_item_new_with_label("Edit controls");
	g_signal_connect(G_OBJECT(pMenuItem), "activate", G_CALLBACK(Edit_Controls), (GtkWidget*) pWindow);
	gtk_menu_shell_append(GTK_MENU_SHELL(mConfig), pMenuItem);
	
#if 0
	
	GtkWidget *mFirmware;
	
	mFirmware = gtk_menu_new();
	pMenuItem = gtk_menu_item_new_with_label("Firmware");
	gtk_menu_item_set_submenu(GTK_MENU_ITEM(pMenuItem), mFirmware);
	gtk_menu_shell_append(GTK_MENU_SHELL(mConfig), pMenuItem);
	
	pMenuItem = gtk_menu_item_new_with_label("Select...");
	g_signal_connect(G_OBJECT(pMenuItem), "activate", G_CALLBACK(SelectFirmwareFile), (gpointer)0);
	gtk_menu_shell_append(GTK_MENU_SHELL(mFirmware), pMenuItem);
		
	pMenuItem = gtk_menu_item_new_with_label("Config");
	gtk_menu_item_set_submenu(GTK_MENU_ITEM(pMenuItem), mConfig);
	gtk_menu_shell_append(GTK_MENU_SHELL(pMenuBar), pMenuItem);
	
#endif
	
	/** Menu "Outils" **/
	
	pMenu = gtk_menu_new();
	
	for(i = 0; i < dTools_list_size; i++)
	{
		pMenuItem = gtk_menu_item_new_with_label(dTools_list[i]->name);
		g_signal_connect(G_OBJECT(pMenuItem), "activate", G_CALLBACK(Start_dTool), GINT_TO_POINTER(i));
		gtk_menu_shell_append(GTK_MENU_SHELL(pMenu), pMenuItem);
	}
		
	pMenuItem = gtk_menu_item_new_with_label("Tools");
	gtk_menu_item_set_submenu(GTK_MENU_ITEM(pMenuItem), pMenu);
	gtk_menu_shell_append(GTK_MENU_SHELL(pMenuBar), pMenuItem);
	
	/** Menu "?" **/

	pMenu = gtk_menu_new();

#if ((GTK_MAJOR_VERSION >= 2) && (GTK_MINOR_VERSION >= 6))
	pMenuItem = gtk_image_menu_item_new_from_stock(GTK_STOCK_ABOUT,NULL);
#else
	pMenuItem = gtk_menu_item_new_with_label("About");
#endif
	g_signal_connect(G_OBJECT(pMenuItem), "activate", G_CALLBACK(About), (GtkWidget*) pWindow);
	gtk_menu_shell_append(GTK_MENU_SHELL(pMenu), pMenuItem);

	pMenuItem = gtk_menu_item_new_with_label("?");
	gtk_menu_item_set_submenu(GTK_MENU_ITEM(pMenuItem), pMenu);
	gtk_menu_shell_append(GTK_MENU_SHELL(pMenuBar), pMenuItem);

	/* Ajout du menu a la fenetre */
	gtk_box_pack_start(GTK_BOX(pVBox), pMenuBar, FALSE, FALSE, 0);

	/* CrÃ©ation de la Toolbar */
	
	pToolbar = gtk_toolbar_new();
	gtk_box_pack_start(GTK_BOX(pVBox), pToolbar, FALSE, FALSE, 0);

	gtk_toolbar_insert(GTK_TOOLBAR(pToolbar), GTK_TOOL_ITEM(gtk_action_create_tool_item(gtk_action_group_get_action(action_group, "open"))), -1);
	gtk_toolbar_insert(GTK_TOOLBAR(pToolbar), GTK_TOOL_ITEM(gtk_action_create_tool_item(gtk_action_group_get_action(action_group, "run"))), -1);
	gtk_toolbar_insert(GTK_TOOLBAR(pToolbar), GTK_TOOL_ITEM(gtk_action_create_tool_item(gtk_action_group_get_action(action_group, "pause"))), -1);
	gtk_toolbar_insert(GTK_TOOLBAR(pToolbar), GTK_TOOL_ITEM(gtk_action_create_tool_item(gtk_action_group_get_action(action_group, "quit"))), -1);

	/* CrÃ©ation de l'endroit pour l'affichage des Ã©crans */
	
	pDrawingArea= gtk_drawing_area_new();
	
	gtk_drawing_area_size(GTK_DRAWING_AREA(pDrawingArea), 256, 384);
	gtk_widget_set_usize (pDrawingArea, 256, 384);
			
	gtk_widget_set_events(pDrawingArea, GDK_EXPOSURE_MASK | GDK_LEAVE_NOTIFY_MASK | GDK_BUTTON_PRESS_MASK | GDK_BUTTON_RELEASE_MASK | GDK_POINTER_MOTION_MASK | GDK_KEY_PRESS_MASK );
	
	g_signal_connect(G_OBJECT(pDrawingArea), "button_press_event", G_CALLBACK(Stylus_Press), NULL);
	g_signal_connect(G_OBJECT(pDrawingArea), "button_release_event", G_CALLBACK(Stylus_Release), NULL);
	g_signal_connect(G_OBJECT(pDrawingArea), "motion_notify_event", G_CALLBACK(Stylus_Move), NULL);
	
	
	g_signal_connect( G_OBJECT(pDrawingArea), "realize", G_CALLBACK(Draw), NULL ) ;
	g_signal_connect( G_OBJECT(pDrawingArea), "expose_event", G_CALLBACK(gtkFloatExposeEvent), NULL ) ;
	
	gtk_box_pack_start(GTK_BOX(pVBox), pDrawingArea, FALSE, FALSE, 0);
	
	/* Création de la barre d'état */
	
	pStatusBar = gtk_statusbar_new();
	
	pStatusBar_Ctx = gtk_statusbar_get_context_id(GTK_STATUSBAR(pStatusBar), "Global");
	
	pStatusBar_Change("Desmume");

	gtk_box_pack_end(GTK_BOX(pVBox), pStatusBar, FALSE, FALSE, 0);
	
	gtk_widget_show_all(pWindow);
	
	//LoadFirmware("fw.bin");
	
	/* Vérifie la ligne de commandes */
	if(commandLine_File)
	{
		if(Open(commandLine_File) >= 0)
		{
			Launch();
		}
		else
		{
			GtkWidget *pDialog = gtk_message_dialog_new(GTK_WINDOW(pWindow),
					GTK_DIALOG_MODAL,
					GTK_MESSAGE_INFO,
					GTK_BUTTONS_OK,
					"Unable to load :\n%s", commandLine_File);
			gtk_dialog_run(GTK_DIALOG(pDialog));
			gtk_widget_destroy(pDialog);
		}
	}
	
	/* Boucle principale */
	
//	gtk_idle_add(&EmuLoop, pWindow);
//	g_idle_add(&EmuLoop, pWindow);
	
	gtk_main();
	
	desmume_free();

#ifdef DEBUG
        LogStop();
#endif
        /* Unload joystick */
        uninit_joy();

	SDL_Quit();
	
	Write_ConfigFile();
	
	return EXIT_SUCCESS;
}
