#include "globals.h"

/* INPUT BUTTONS / KEYBOARD */
gboolean  on_wMainW_key_press_event    (GtkWidget *widget, GdkEventKey *event, gpointer user_data);
gboolean  on_wMainW_key_release_event  (GtkWidget *widget, GdkEventKey *event, gpointer user_data);

/* OUTPUT UPPER SCREEN  */
void      on_wDraw_Main_realize       (GtkWidget *widget, gpointer user_data);
gboolean  on_wDraw_Main_expose_event  (GtkWidget *widget, GdkEventExpose  *event, gpointer user_data);

/* OUTPUT LOWER SCREEN  */
void      on_wDraw_Sub_realize        (GtkWidget *widget, gpointer user_data);
gboolean  on_wDraw_Sub_expose_event   (GtkWidget *widget, GdkEventExpose  *event, gpointer user_data);

/* INPUT STYLUS / MOUSE */
gboolean  on_wDraw_Sub_button_press_event   (GtkWidget *widget, GdkEventButton  *event, gpointer user_data);
gboolean  on_wDraw_Sub_button_release_event (GtkWidget *widget, GdkEventButton  *event, gpointer user_data);
gboolean  on_wDraw_Sub_motion_notify_event  (GtkWidget *widget, GdkEventMotion  *event, gpointer user_data);




/* KEYBOARD CONFIG / KEY DEFINITION */

gboolean  on_wKeyDlg_key_press_event (GtkWidget *widget, GdkEventKey *event, gpointer user_data);
void  on_wKeybConfDlg_response (GtkDialog *dialog, gint arg1, gpointer user_data);

void  on_button_Left_clicked    (GtkButton *button, gpointer user_data);
void  on_button_Up_clicked      (GtkButton *button, gpointer user_data);
void  on_button_Right_clicked   (GtkButton *button, gpointer user_data);
void  on_button_Down_clicked    (GtkButton *button, gpointer user_data);

void  on_button_L_clicked       (GtkButton *button, gpointer user_data);
void  on_button_R_clicked       (GtkButton *button, gpointer user_data);

void  on_button_Y_clicked       (GtkButton *button, gpointer user_data);
void  on_button_X_clicked       (GtkButton *button, gpointer user_data);
void  on_button_A_clicked       (GtkButton *button, gpointer user_data);
void  on_button_B_clicked       (GtkButton *button, gpointer user_data);

void  on_button_Start_clicked   (GtkButton *button, gpointer user_data);
void  on_button_Select_clicked  (GtkButton *button, gpointer user_data);
void  on_button_Debug_clicked   (GtkButton *button, gpointer user_data);
void  on_button_Boost_clicked   (GtkButton *button, gpointer user_data);
