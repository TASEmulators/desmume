/*
	main.cpp - DeSmuME GTK4 frontend

	Copyright (C) 2026 DeSmuME team

	This file is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 2 of the License, or
	(at your option) any later version.

	This file is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with the this software.  If not, see <http://www.gnu.org/licenses/>.
*/

/*
	P0 skeleton for the GTK4 frontend.

	This file deliberately contains only the window shell:

	- a GtkApplicationWindow with no custom titlebar on the SSD path, so that
	  KWin (Wayland) or the X11 window manager draws the decorations;
	- a GtkPopoverMenuBar inside the content area, shared by both decoration
	  policies;
	- a compact toolbar, a black display area and a status row;
	- the decoration policy selection (auto / force SSD / force CSD).

	ROM loading, the emulation loop, the display widget and the tools arrive in
	later phases.  See documentation/gtk4.md for the phase plan.
*/

#include <gtk/gtk.h>
#include <string.h>

#include "config.h"

#if defined(HAVE_GTK4_WAYLAND)
#include <gdk/wayland/gdkwayland.h>
#endif
#if defined(HAVE_GTK4_X11)
#include <gdk/x11/gdkx.h>
#endif

enum DecorationPolicy
{
	DECORATION_AUTO = 0,
	DECORATION_SSD  = 1,
	DECORATION_CSD  = 2
};

/* G_APPLICATION_DEFAULT_FLAGS only exists since GLib 2.74, while GTK 4.10 is
 * happy with 2.72.  Keep the build working on the older minimum. */
#if GLIB_CHECK_VERSION(2, 74, 0)
#define DESMUME_APPLICATION_FLAGS G_APPLICATION_DEFAULT_FLAGS
#else
#define DESMUME_APPLICATION_FLAGS G_APPLICATION_FLAGS_NONE
#endif

static desmume::config::Config *pConfig = NULL;

static GtkWidget *pWindow = NULL;
static GtkWidget *pMenuBar = NULL;
static GtkWidget *pToolBar = NULL;
static GtkWidget *pStatusBar = NULL;
static GtkWidget *pStatusLabel = NULL;
static GtkWidget *pRunButton = NULL;
static GtkWidget *pDisplayArea = NULL;

static gboolean sRunning = FALSE;

static void SetStatus(const char *text)
{
	if (pStatusLabel != NULL)
	{
		gtk_label_set_text(GTK_LABEL(pStatusLabel), text);
	}
}

static gboolean IsWaylandDisplay(void)
{
#if defined(HAVE_GTK4_WAYLAND)
	GdkDisplay *display = gdk_display_get_default();
	return (display != NULL) && GDK_IS_WAYLAND_DISPLAY(display);
#else
	/* Older GTK4 releases do not ship gtk4-wayland.pc.  The display type name
	 * is stable and keeps the auto policy useful without the extra dep. */
	GdkDisplay *display = gdk_display_get_default();
	return (display != NULL) &&
	       g_str_has_prefix(G_OBJECT_TYPE_NAME(display), "GdkWayland");
#endif
}

/*
	GTK negotiates server-side decorations with the compositor when the
	application does not install a custom titlebar.  KWin implements the KDE
	server-decoration protocol, so on KDE Wayland leaving the titlebar unset is
	all that is needed for native decorations.  Mutter does not implement it,
	which is why GNOME gets a GtkHeaderBar instead.
*/
static gboolean UseClientSideDecorations(void)
{
	const int policy = (pConfig != NULL) ? pConfig->decorations.get() : DECORATION_AUTO;

	if (policy == DECORATION_SSD)
	{
		return FALSE;
	}
	if (policy == DECORATION_CSD)
	{
		return TRUE;
	}

	/* X11: let the window manager decorate the window. */
	if (!IsWaylandDisplay())
	{
		return FALSE;
	}

	/* Wayland: SSD is only available where the compositor asks for it. */
	const char *desktop = g_getenv("XDG_CURRENT_DESKTOP");
	if (desktop == NULL)
	{
		return FALSE;
	}

	return (strstr(desktop, "GNOME") != NULL) ||
	       (strstr(desktop, "Budgie") != NULL) ||
	       (strstr(desktop, "Pantheon") != NULL) ||
	       (strstr(desktop, "Unity") != NULL);
}

static void DrawDisplay(GtkDrawingArea *area, cairo_t *cr, int width, int height, gpointer user_data)
{
	(void)area;
	(void)user_data;

	cairo_set_source_rgb(cr, 0.0, 0.0, 0.0);
	cairo_paint(cr);

	if ((width <= 0) || (height <= 0))
	{
		return;
	}

	const char *message = "DeSmuME GTK4 - P0 skeleton";
	cairo_text_extents_t extents;

	cairo_select_font_face(cr, "Sans", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);
	cairo_set_font_size(cr, 15.0);
	cairo_text_extents(cr, message, &extents);

	cairo_set_source_rgb(cr, 0.55, 0.55, 0.58);
	cairo_move_to(cr,
	              (width  - extents.width)  / 2.0 - extents.x_bearing,
	              (height - extents.height) / 2.0 - extents.y_bearing);
	cairo_show_text(cr, message);
}

static void OnAlertDone(GObject *source, GAsyncResult *result, gpointer user_data)
{
	(void)user_data;
	(void)gtk_alert_dialog_choose_finish(GTK_ALERT_DIALOG(source), result, NULL);
}

static void ActionOpen(GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
	(void)action;
	(void)parameter;
	(void)user_data;

	GtkAlertDialog *dialog = gtk_alert_dialog_new("ROM loading arrives in phase P3.");
	gtk_alert_dialog_set_detail(dialog,
	                            "This is the P0 window shell. The menu, toolbar, decoration "
	                            "policy and display area are live; the emulation loop and the "
	                            "GdkTexture display widget land in P2/P3.");
	static const char *buttons[] = { "OK", NULL };
	gtk_alert_dialog_set_buttons(dialog, buttons);
	gtk_alert_dialog_choose(dialog, GTK_WINDOW(pWindow), NULL, OnAlertDone, NULL);
	g_object_unref(dialog);
}

static void ActionQuit(GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
	(void)action;
	(void)parameter;
	(void)user_data;

	GtkApplication *app = gtk_window_get_application(GTK_WINDOW(pWindow));
	if (app != NULL)
	{
		g_application_quit(G_APPLICATION(app));
	}
}

static void ActionRun(GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
	(void)action;
	(void)parameter;
	(void)user_data;

	sRunning = !sRunning;

	if (pRunButton != NULL)
	{
		gtk_button_set_icon_name(GTK_BUTTON(pRunButton),
		                         sRunning ? "media-playback-pause-symbolic"
		                                  : "media-playback-start-symbolic");
		gtk_widget_set_tooltip_text(pRunButton, sRunning ? "Pause" : "Run");
	}

	SetStatus(sRunning ? "Running (skeleton)" : "Paused");
}

static void ActionFullscreen(GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
	(void)action;
	(void)parameter;
	(void)user_data;

	if (gtk_window_is_fullscreen(GTK_WINDOW(pWindow)))
	{
		gtk_window_unfullscreen(GTK_WINDOW(pWindow));
	}
	else
	{
		gtk_window_fullscreen(GTK_WINDOW(pWindow));
	}
}

static void ActionAbout(GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
	(void)action;
	(void)parameter;
	(void)user_data;

	GtkWidget *dialog = gtk_about_dialog_new();
	gtk_about_dialog_set_program_name(GTK_ABOUT_DIALOG(dialog), "DeSmuME");
	gtk_about_dialog_set_version(GTK_ABOUT_DIALOG(dialog), "GTK4 (P0 skeleton)");
	gtk_about_dialog_set_comments(GTK_ABOUT_DIALOG(dialog),
	                              "Nintendo DS emulator based on work by Yopyop.");
	gtk_about_dialog_set_website(GTK_ABOUT_DIALOG(dialog), "https://desmume.org");
	gtk_about_dialog_set_license_type(GTK_ABOUT_DIALOG(dialog), GTK_LICENSE_GPL_2_0);
	gtk_window_set_transient_for(GTK_WINDOW(dialog), GTK_WINDOW(pWindow));
	gtk_window_set_modal(GTK_WINDOW(dialog), TRUE);

	/* Hold a reference until the window is destroyed; the destroy handler
	 * drops it, so repeated About invocations do not leak. */
	g_object_ref_sink(dialog);
	g_signal_connect_swapped(dialog, "destroy", G_CALLBACK(g_object_unref), dialog);
	gtk_window_present(GTK_WINDOW(dialog));
}

static void ToolbarChanged(GSimpleAction *action, GVariant *value, gpointer user_data)
{
	(void)user_data;

	g_simple_action_set_state(action, value);
	gtk_widget_set_visible(pToolBar, g_variant_get_boolean(value));
}

static void StatusbarChanged(GSimpleAction *action, GVariant *value, gpointer user_data)
{
	(void)user_data;

	g_simple_action_set_state(action, value);
	gtk_widget_set_visible(pStatusBar, g_variant_get_boolean(value));
}

static const GActionEntry app_entries[] =
{
	{ "open",       ActionOpen,       NULL, NULL,   NULL },
	{ "quit",       ActionQuit,       NULL, NULL,   NULL },
	{ "run",        ActionRun,        NULL, NULL,   NULL },
	{ "fullscreen", ActionFullscreen, NULL, NULL,   NULL },
	{ "about",      ActionAbout,      NULL, NULL,   NULL },
	{ "toolbar",    NULL,             NULL, "true", ToolbarChanged },
	{ "statusbar",  NULL,             NULL, "true", StatusbarChanged }
};

static GMenuModel *BuildMenuModel(void)
{
	GMenu *menubar = g_menu_new();

	GMenu *file = g_menu_new();
	g_menu_append(file, "_Open...", "app.open");
	g_menu_append(file, "_Quit", "app.quit");
	g_menu_append_submenu(menubar, "_File", G_MENU_MODEL(file));
	g_object_unref(file);

	GMenu *view = g_menu_new();
	g_menu_append(view, "_Fullscreen", "app.fullscreen");
	g_menu_append(view, "Show _toolbar", "app.toolbar");
	g_menu_append(view, "Show _status bar", "app.statusbar");
	g_menu_append_submenu(menubar, "_View", G_MENU_MODEL(view));
	g_object_unref(view);

	GMenu *help = g_menu_new();
	g_menu_append(help, "_About DeSmuME", "app.about");
	g_menu_append_submenu(menubar, "_Help", G_MENU_MODEL(help));
	g_object_unref(help);

	return G_MENU_MODEL(menubar);
}

static GtkWidget *MakeToolButton(const char *icon_name, const char *tooltip, const char *action_name)
{
	GtkWidget *button = gtk_button_new_from_icon_name(icon_name);
	gtk_widget_set_tooltip_text(button, tooltip);
	gtk_widget_add_css_class(button, "flat");
	gtk_actionable_set_action_name(GTK_ACTIONABLE(button), action_name);
	return button;
}

static void BuildWindow(GtkApplication *app)
{
	pWindow = gtk_application_window_new(app);
	gtk_window_set_title(GTK_WINDOW(pWindow), "DeSmuME");
	gtk_window_set_default_size(GTK_WINDOW(pWindow), 1024, 768);
	gtk_application_window_set_show_menubar(GTK_APPLICATION_WINDOW(pWindow), FALSE);

	GtkWidget *root = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
	gtk_window_set_child(GTK_WINDOW(pWindow), root);

	/* The menu bar lives in the content area so the same tree works with SSD
	 * and CSD; the window title is owned by the decoration. */
	GMenuModel *menu_model = BuildMenuModel();
	pMenuBar = gtk_popover_menu_bar_new_from_model(menu_model);
	g_object_unref(menu_model);
	gtk_widget_set_visible(pMenuBar, pConfig->view_menu.get());
	gtk_box_append(GTK_BOX(root), pMenuBar);

	pToolBar = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4);
	gtk_widget_add_css_class(pToolBar, "toolbar");
	gtk_widget_set_margin_start(pToolBar, 6);
	gtk_widget_set_margin_end(pToolBar, 6);
	gtk_widget_set_margin_top(pToolBar, 3);
	gtk_widget_set_margin_bottom(pToolBar, 3);
	gtk_widget_set_visible(pToolBar, pConfig->view_toolbar.get());

	GtkWidget *open_button = MakeToolButton("document-open-symbolic",
	                                        "Open ROM (Ctrl+O)", "app.open");
	gtk_box_append(GTK_BOX(pToolBar), open_button);

	pRunButton = MakeToolButton("media-playback-start-symbolic", "Run", "app.run");
	gtk_box_append(GTK_BOX(pToolBar), pRunButton);

	GtkWidget *reset_button = gtk_button_new_from_icon_name("view-refresh-symbolic");
	gtk_widget_add_css_class(reset_button, "flat");
	gtk_widget_set_tooltip_text(reset_button, "Reset (not wired yet)");
	gtk_widget_set_sensitive(reset_button, FALSE);
	gtk_box_append(GTK_BOX(pToolBar), reset_button);

	GtkWidget *spacer = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
	gtk_widget_set_hexpand(spacer, TRUE);
	gtk_box_append(GTK_BOX(pToolBar), spacer);

	GtkWidget *fullscreen_button = MakeToolButton("view-fullscreen-symbolic",
	                                              "Fullscreen", "app.fullscreen");
	gtk_box_append(GTK_BOX(pToolBar), fullscreen_button);

	gtk_box_append(GTK_BOX(root), pToolBar);

	pDisplayArea = gtk_drawing_area_new();
	gtk_widget_set_hexpand(pDisplayArea, TRUE);
	gtk_widget_set_vexpand(pDisplayArea, TRUE);
	gtk_widget_add_css_class(pDisplayArea, "desmume-display");
	gtk_drawing_area_set_draw_func(GTK_DRAWING_AREA(pDisplayArea),
	                               DrawDisplay, NULL, NULL);
	gtk_box_append(GTK_BOX(root), pDisplayArea);

	pStatusBar = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
	gtk_widget_set_margin_start(pStatusBar, 8);
	gtk_widget_set_margin_end(pStatusBar, 8);
	gtk_widget_set_margin_top(pStatusBar, 3);
	gtk_widget_set_margin_bottom(pStatusBar, 3);
	gtk_widget_set_visible(pStatusBar, pConfig->view_statusbar.get());

	pStatusLabel = gtk_label_new("Ready");
	gtk_label_set_xalign(GTK_LABEL(pStatusLabel), 0.0f);
	gtk_label_set_ellipsize(GTK_LABEL(pStatusLabel), PANGO_ELLIPSIZE_END);
	gtk_widget_set_hexpand(pStatusLabel, TRUE);
	gtk_box_append(GTK_BOX(pStatusBar), pStatusLabel);

	GtkWidget *speed_label = gtk_label_new("P0");
	gtk_widget_add_css_class(speed_label, "dim-label");
	gtk_box_append(GTK_BOX(pStatusBar), speed_label);

	gtk_box_append(GTK_BOX(root), pStatusBar);

	/* Client-side decorations only when the policy asks for them.  Leaving the
	 * titlebar unset is what lets KWin draw its own decorations. */
	if (UseClientSideDecorations())
	{
		GtkWidget *header = gtk_header_bar_new();
		GtkWidget *title = gtk_label_new(NULL);
		gtk_label_set_markup(GTK_LABEL(title), "<b>DeSmuME</b>");
		gtk_header_bar_set_title_widget(GTK_HEADER_BAR(header), title);

		GtkWidget *header_open = gtk_button_new_from_icon_name("document-open-symbolic");
		gtk_widget_set_tooltip_text(header_open, "Open ROM (Ctrl+O)");
		gtk_actionable_set_action_name(GTK_ACTIONABLE(header_open), "app.open");
		gtk_header_bar_pack_start(GTK_HEADER_BAR(header), header_open);

		gtk_window_set_titlebar(GTK_WINDOW(pWindow), header);
	}
}

static void OnActivate(GtkApplication *app, gpointer user_data)
{
	(void)user_data;

	if (pWindow != NULL)
	{
		gtk_window_present(GTK_WINDOW(pWindow));
		return;
	}

	BuildWindow(app);

	GSimpleAction *toolbar_action = G_SIMPLE_ACTION(
		g_action_map_lookup_action(G_ACTION_MAP(app), "toolbar"));
	GSimpleAction *statusbar_action = G_SIMPLE_ACTION(
		g_action_map_lookup_action(G_ACTION_MAP(app), "statusbar"));

	g_simple_action_set_state(toolbar_action, g_variant_new_boolean(pConfig->view_toolbar.get()));
	g_simple_action_set_state(statusbar_action, g_variant_new_boolean(pConfig->view_statusbar.get()));

	gtk_window_present(GTK_WINDOW(pWindow));

	if (pConfig->window_fullscreen.get())
	{
		gtk_window_fullscreen(GTK_WINDOW(pWindow));
	}
}

int main(int argc, char *argv[])
{
	g_set_prgname("desmume-gtk4");
	g_set_application_name("DeSmuME");

	pConfig = new desmume::config::Config();
	pConfig->load();

	GtkApplication *app = gtk_application_new("org.desmume.DeSmuME.GTK4",
	                                          DESMUME_APPLICATION_FLAGS);
	g_signal_connect(app, "activate", G_CALLBACK(OnActivate), NULL);

	g_action_map_add_action_entries(G_ACTION_MAP(app),
	                                app_entries, G_N_ELEMENTS(app_entries),
	                                app);

	static const char *open_accels[] = { "<Control>o", NULL };
	static const char *quit_accels[] = { "<Control>q", NULL };
	static const char *fullscreen_accels[] = { "F11", NULL };
	gtk_application_set_accels_for_action(app, "app.open", open_accels);
	gtk_application_set_accels_for_action(app, "app.quit", quit_accels);
	gtk_application_set_accels_for_action(app, "app.fullscreen", fullscreen_accels);

	const int status = g_application_run(G_APPLICATION(app), argc, argv);

	g_object_unref(app);
	delete pConfig;
	pConfig = NULL;

	return status;
}
