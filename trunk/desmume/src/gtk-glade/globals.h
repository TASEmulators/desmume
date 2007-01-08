#ifndef __GLOBALS_H__
#define __GLOBALS_H__

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <libgen.h>
#include <unistd.h>

#include <SDL/SDL.h>
#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>
#include <glade/glade.h>

#include "../MMU.h"
#include "../registers.h"
#include "../armcpu.h"
#include "../NDSSystem.h"
#include "../cflash.h"
#include "../sndsdl.h"
#include "../types.h"
#include "desmume.h"

uint Frameskip;
gint Keypad_Config[DESMUME_NB_KEYS];
gint Keypad_Temp[DESMUME_NB_KEYS];
u16 Joypad_Config[DESMUME_NB_KEYS];

/* main.c */
GtkWidget * pWindow;
GtkWidget * pDrawingArea;
GtkWidget * pDrawingArea2;
GladeXML  * xml, * xml_tools;

/* callbacks.c */
void enable_rom_features();

/* callbacks_IO.c */
int ScreenCoeff_Size;
gboolean ScreenRotate;
gboolean ScreenRight;
gboolean ScreenGap;

void black_screen ();
void edit_controls();

/* printscreen.c */
int WriteBMP(const char *filename,u16 *bmp);

/* keyvalnames.c   -see <gdk/gdkkeysyms.h>- */
char * KEYVAL_NAMES[0x10000];
char * KEYNAME(int k);
void init_keyvals();

#endif /* __GLOBALS_H__ */
