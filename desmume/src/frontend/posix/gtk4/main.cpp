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
	P1 window shell for the GTK4 frontend.

	The window shell keeps the decoration policy of P0: no custom titlebar on
	the SSD path, so KWin (Wayland) or the X11 window manager draws the frame.
	P1 adds the real menu model, the visibility toggles, a shortcuts window
	and the semantic CSS, all from the gresource bundle.

	P3a adds the emulation core: asynchronous ROM selection, a 60 Hz frame
	callback on the GTK main thread, real frames pushed into DesmumeDisplay and
	keyboard/touch input forwarded to the core.  The remaining P3 work (the
	other menu actions and their dialogs) and P4-P8 are tracked in
	documentation/gtk4.md.
*/

#include <gtk/gtk.h>
#include <string.h>

#include "config.h"
#include "display.h"
#include "emulation.h"

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
static GtkWidget *pShortcutsWindow = NULL;
static GtkWidget *pToolBar = NULL;
static GtkWidget *pStatusBar = NULL;
static GtkWidget *pStatusLabel = NULL;
static GtkWidget *pRunButton = NULL;
static GtkWidget *pDisplayArea = NULL;

/* A ROM given on the command line is loaded once the window exists, so the
 * status line and title can be updated through the normal path. */
static char *sInitialRom = NULL;

static void SetStatus(const char *text)
{
	if (pStatusLabel == NULL)
	{
		return;
	}

	/* Updating an unchanged label still triggers a redraw; the status line is
	 * fed from the emulation loop, so skip the no-op case. */
	const char *current = gtk_label_get_text(GTK_LABEL(pStatusLabel));
	if ((current == NULL) || (strcmp(current, text) != 0))
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

/* The emulation core runs on the GTK main thread: one NDS frame per 60 Hz
 * tick.  Without a ROM this is a no-op, so the callback can be installed once
 * when the window is built and left running for the whole application. */
static gboolean
OnFrameTick(gpointer user_data)
{
	(void)user_data;

	if (desmume_emu_get_running() && desmume_emu_has_rom())
	{
		desmume_emu_step();
	}
	return G_SOURCE_CONTINUE;
}

static void
PushFrameToDisplay(const guchar *bgra, int width, int height, gpointer user_data)
{
	desmume_display_push_frame(DESMUME_DISPLAY(user_data), bgra, width, height);
}

/* DESMUME_GTK4_TEST_FRAME=1 still feeds a synthetic frame so the texture path
 * can be exercised without a ROM. */
static gboolean PushTestFrame(gpointer user_data)
{
	DesmumeDisplay *display = DESMUME_DISPLAY(user_data);
	if (!gtk_widget_get_mapped(GTK_WIDGET(display)))
	{
		return G_SOURCE_CONTINUE;
	}

	static guchar buffer[256 * 192 * 2 * 4];
	static int tick = 0;

	const int marker_x = (tick++ / 3) % 256;
	for (int screen = 0; screen < 2; ++screen)
	{
		for (int y = 0; y < 192; ++y)
		{
			for (int x = 0; x < 256; ++x)
			{
				guchar *pixel = buffer + ((screen * 192 + y) * 256 + x) * 4;
				pixel[0] = (guchar)(screen == 0 ? y / 2 : 40);       /* B */
				pixel[1] = (guchar)(screen == 0 ? 60 : x / 2);       /* G */
				pixel[2] = (guchar)(screen == 0 ? 160 : 90);         /* R */
				pixel[3] = 0xFF;
			}
		}
	}

	for (int y = 90; y < 102; ++y)
	{
		for (int x = marker_x - 6; x < marker_x + 6; ++x)
		{
			if (x < 0 || x >= 256)
			{
				continue;
			}
			guchar *pixel = buffer + ((192 + y) * 256 + x) * 4;
			pixel[0] = pixel[1] = pixel[2] = 0xFF;
		}
	}

	desmume_display_push_frame(display, buffer, 256, 192 * 2);
	return G_SOURCE_CONTINUE;
}

static void OnTouchPress(DesmumeDisplay *display, double x, double y, gpointer user_data)
{
	(void)display;
	(void)user_data;

	if (!desmume_emu_has_rom())
	{
		return;
	}

	desmume_emu_touch(x, y);
}

static void OnTouchMove(DesmumeDisplay *display, double x, double y, gpointer user_data)
{
	OnTouchPress(display, x, y, user_data);
}

static void OnTouchRelease(DesmumeDisplay *display, gpointer user_data)
{
	(void)display;
	(void)user_data;

	desmume_emu_release_touch();
}

static void OnKeyEvent(DesmumeDisplay *display, guint keyval,
                       gboolean pressed, gpointer user_data)
{
	(void)display;
	(void)user_data;

	desmume_emu_set_key(keyval, pressed);
}

static void OnAlertDone(GObject *source, GAsyncResult *result, gpointer user_data);

static void ShowErrorAlert(const char *message)
{
	GtkAlertDialog *dialog = gtk_alert_dialog_new("DeSmuME");
	gtk_alert_dialog_set_message(dialog, message);
	static const char *buttons[] = { "OK", NULL };
	gtk_alert_dialog_set_buttons(dialog, buttons);
	gtk_alert_dialog_choose(dialog, GTK_WINDOW(pWindow), NULL, OnAlertDone, NULL);
	g_object_unref(dialog);
}

static void OnAlertDone(GObject *source, GAsyncResult *result, gpointer user_data)
{
	(void)user_data;
	(void)gtk_alert_dialog_choose_finish(GTK_ALERT_DIALOG(source), result, NULL);
}

static void
RomLoaded(void)
{
	char *title = g_strdup_printf("%s - DeSmuME", desmume_emu_rom_name());
	gtk_window_set_title(GTK_WINDOW(pWindow), title);
	g_free(title);

	if (pRunButton != NULL)
	{
		gtk_button_set_icon_name(GTK_BUTTON(pRunButton), "media-playback-pause-symbolic");
		gtk_widget_set_tooltip_text(pRunButton, "Pause");
	}

	char *status = g_strdup_printf("Running: %s", desmume_emu_rom_name());
	SetStatus(status);
	g_free(status);
}

static gboolean
LoadRomAndReport(const char *path)
{
	char *load_error = NULL;
	if (!desmume_emu_load_rom(path, &load_error))
	{
		ShowErrorAlert(load_error != NULL ? load_error : "Could not open the ROM.");
		g_free(load_error);
		return FALSE;
	}

	RomLoaded();
	return TRUE;
}

static void
OnOpenDone(GObject *source, GAsyncResult *result, gpointer user_data)
{
	(void)user_data;

	GError *error = NULL;
	GFile *file = gtk_file_dialog_open_finish(GTK_FILE_DIALOG(source), result, &error);
	if (file == NULL)
	{
		/* A cancelled dialog is not an error worth reporting. */
		if ((error != NULL) &&
		    !g_error_matches(error, GTK_DIALOG_ERROR, GTK_DIALOG_ERROR_DISMISSED))
		{
			ShowErrorAlert(error->message);
		}
		g_clear_error(&error);
		return;
	}

	char *path = g_file_get_path(file);
	g_object_unref(file);

	if (path == NULL)
	{
		ShowErrorAlert("Only local files can be opened as a ROM.");
		return;
	}

	LoadRomAndReport(path);
	g_free(path);
}

static void ActionOpen(GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
	(void)action;
	(void)parameter;
	(void)user_data;

	GtkFileDialog *dialog = gtk_file_dialog_new();
	gtk_file_dialog_set_title(dialog, "Open Nintendo DS ROM");

	GtkFileFilter *filter = gtk_file_filter_new();
	gtk_file_filter_set_name(filter, "Nintendo DS ROMs");
	gtk_file_filter_add_pattern(filter, "*.nds");
	gtk_file_filter_add_pattern(filter, "*.NDS");
	gtk_file_filter_add_pattern(filter, "*.rom");
	gtk_file_filter_add_pattern(filter, "*.ROM");
	gtk_file_filter_add_pattern(filter, "*.zip");

	GListStore *filters = g_list_store_new(GTK_TYPE_FILE_FILTER);
	g_list_store_append(filters, filter);
	gtk_file_dialog_set_filters(dialog, G_LIST_MODEL(filters));

	gtk_file_dialog_open(dialog, GTK_WINDOW(pWindow), NULL, OnOpenDone, NULL);
	g_object_unref(dialog);
	g_object_unref(filter);
	g_object_unref(filters);
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

	if (!desmume_emu_has_rom())
	{
		SetStatus("Open a ROM first (Ctrl+O).");
		return;
	}

	const gboolean now_running = !desmume_emu_get_running();
	desmume_emu_set_running(now_running);

	if (pRunButton != NULL)
	{
		gtk_button_set_icon_name(GTK_BUTTON(pRunButton),
		                         now_running ? "media-playback-pause-symbolic"
		                                     : "media-playback-start-symbolic");
		gtk_widget_set_tooltip_text(pRunButton, now_running ? "Pause" : "Run");
	}

	SetStatus(now_running ? "Running" : "Paused");
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

static gboolean OnShortcutsClose(GtkWindow *window, gpointer user_data)
{
	(void)user_data;

	/* Keep the builder-owned window around so the action can present it again. */
	gtk_widget_set_visible(GTK_WIDGET(window), FALSE);
	return TRUE;
}

static void ActionShortcuts(GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
	(void)action;
	(void)parameter;
	(void)user_data;

	if (pShortcutsWindow == NULL)
	{
		GtkBuilder *builder = gtk_builder_new_from_resource(
			"/org/desmume/DeSmuME/shortcuts.ui");
		pShortcutsWindow = GTK_WIDGET(g_object_ref(
			gtk_builder_get_object(builder, "shortcuts")));
		g_object_unref(builder);

		gtk_window_set_transient_for(GTK_WINDOW(pShortcutsWindow), GTK_WINDOW(pWindow));
		g_signal_connect(pShortcutsWindow, "close-request",
		                 G_CALLBACK(OnShortcutsClose), NULL);
	}

	gtk_window_present(GTK_WINDOW(pShortcutsWindow));
}

static GMenuModel *LoadMenuModel(void)
{
	GtkBuilder *builder = gtk_builder_new_from_resource(
		"/org/desmume/DeSmuME/menu.ui");
	GMenuModel *menu = G_MENU_MODEL(g_object_ref(
		gtk_builder_get_object(builder, "menubar")));
	g_object_unref(builder);
	return menu;
}

static void LoadCss(void)
{
	GtkCssProvider *provider = gtk_css_provider_new();
	gtk_css_provider_load_from_resource(provider,
	                                    "/org/desmume/DeSmuME/style.css");

	GdkDisplay *display = gdk_display_get_default();
	if (display != NULL)
	{
		gtk_style_context_add_provider_for_display(
			display, GTK_STYLE_PROVIDER(provider),
			GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
	}
	g_object_unref(provider);
}

static void MenuBarChanged(GSimpleAction *action, GVariant *value, gpointer user_data)
{
	(void)user_data;

	g_simple_action_set_state(action, value);
	gtk_widget_set_visible(pMenuBar, g_variant_get_boolean(value));
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
	/* Names match gtk/menu.ui so menu items can be enabled phase by phase. */
	{ "open",           ActionOpen,       NULL, NULL,   NULL },
	{ "quit",           ActionQuit,       NULL, NULL,   NULL },
	{ "run",            ActionRun,        NULL, NULL,   NULL },
	{ "fullscreen",     ActionFullscreen, NULL, NULL,   NULL },
	{ "about",          ActionAbout,      NULL, NULL,   NULL },
	{ "shortcuts",      ActionShortcuts,  NULL, NULL,   NULL },
	{ "view-menu",      NULL,             NULL, "true", MenuBarChanged },
	{ "view-toolbar",   NULL,             NULL, "true", ToolbarChanged },
	{ "view-statusbar", NULL,             NULL, "true", StatusbarChanged }
};

static GtkWidget *MakeToolButton(const char *icon_name, const char *tooltip, const char *action_name)
{
	GtkWidget *button = gtk_button_new_from_icon_name(icon_name);
	gtk_widget_set_tooltip_text(button, tooltip);
	gtk_widget_add_css_class(button, "flat");
	gtk_actionable_set_action_name(GTK_ACTIONABLE(button), action_name);
	gtk_accessible_update_property(GTK_ACCESSIBLE(button),
	                               GTK_ACCESSIBLE_PROPERTY_LABEL, tooltip, -1);
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
	GMenuModel *menu_model = LoadMenuModel();
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
	gtk_accessible_update_property(GTK_ACCESSIBLE(reset_button),
	                               GTK_ACCESSIBLE_PROPERTY_LABEL, "Reset", -1);
	gtk_widget_set_sensitive(reset_button, FALSE);
	gtk_box_append(GTK_BOX(pToolBar), reset_button);

	GtkWidget *spacer = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
	gtk_widget_set_hexpand(spacer, TRUE);
	gtk_box_append(GTK_BOX(pToolBar), spacer);

	GtkWidget *fullscreen_button = MakeToolButton("view-fullscreen-symbolic",
	                                              "Fullscreen", "app.fullscreen");
	gtk_box_append(GTK_BOX(pToolBar), fullscreen_button);

	gtk_box_append(GTK_BOX(root), pToolBar);

	pDisplayArea = desmume_display_new();
	gtk_widget_set_hexpand(pDisplayArea, TRUE);
	gtk_widget_set_vexpand(pDisplayArea, TRUE);
	g_signal_connect(pDisplayArea, "touch-press", G_CALLBACK(OnTouchPress), NULL);
	g_signal_connect(pDisplayArea, "touch-move", G_CALLBACK(OnTouchMove), NULL);
	g_signal_connect(pDisplayArea, "touch-release", G_CALLBACK(OnTouchRelease), NULL);
	g_signal_connect(pDisplayArea, "key-event", G_CALLBACK(OnKeyEvent), NULL);
	gtk_box_append(GTK_BOX(root), pDisplayArea);

	pStatusBar = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
	gtk_widget_add_css_class(pStatusBar, "desmume-status");
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
		gtk_accessible_update_property(GTK_ACCESSIBLE(header_open),
		                               GTK_ACCESSIBLE_PROPERTY_LABEL, "Open ROM", -1);
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
	LoadCss();

	GSimpleAction *menu_action = G_SIMPLE_ACTION(
		g_action_map_lookup_action(G_ACTION_MAP(app), "view-menu"));
	GSimpleAction *toolbar_action = G_SIMPLE_ACTION(
		g_action_map_lookup_action(G_ACTION_MAP(app), "view-toolbar"));
	GSimpleAction *statusbar_action = G_SIMPLE_ACTION(
		g_action_map_lookup_action(G_ACTION_MAP(app), "view-statusbar"));

	g_simple_action_set_state(menu_action, g_variant_new_boolean(pConfig->view_menu.get()));
	g_simple_action_set_state(toolbar_action, g_variant_new_boolean(pConfig->view_toolbar.get()));
	g_simple_action_set_state(statusbar_action, g_variant_new_boolean(pConfig->view_statusbar.get()));

	gtk_window_present(GTK_WINDOW(pWindow));

	desmume_emu_set_frame_sink(PushFrameToDisplay, pDisplayArea);
	g_timeout_add(1000 / 60, OnFrameTick, NULL);

	if (sInitialRom != NULL)
	{
		char *path = sInitialRom;
		sInitialRom = NULL;
		LoadRomAndReport(path);
		g_free(path);
	}

	if (g_getenv("DESMUME_GTK4_TEST_FRAME") != NULL)
	{
		g_timeout_add(1000 / 60, PushTestFrame, pDisplayArea);
	}

	if (pConfig->window_fullscreen.get())
	{
		gtk_window_fullscreen(GTK_WINDOW(pWindow));
	}
}

int main(int argc, char *argv[])
{
	g_set_prgname("desmume");
	g_set_application_name("DeSmuME");

	pConfig = new desmume::config::Config();
	pConfig->load();

	/* GtkApplication does not declare G_APPLICATION_HANDLES_OPEN, so a ROM on
	 * the command line arrives as a plain argument.  Use the first existing
	 * regular file and ignore anything else (GTK's own options included). */
	for (int i = 1; i < argc; ++i)
	{
		if ((argv[i][0] != '-') && g_file_test(argv[i], G_FILE_TEST_IS_REGULAR))
		{
			g_free(sInitialRom);
			sInitialRom = g_strdup(argv[i]);
			break;
		}
	}

	GtkApplication *app = gtk_application_new("org.desmume.DeSmuME.GTK4",
	                                          DESMUME_APPLICATION_FLAGS);
	g_signal_connect(app, "activate", G_CALLBACK(OnActivate), NULL);

	g_action_map_add_action_entries(G_ACTION_MAP(app),
	                                app_entries, G_N_ELEMENTS(app_entries),
	                                app);

	static const char *open_accels[] = { "<Control>o", NULL };
	static const char *quit_accels[] = { "<Control>q", NULL };
	static const char *run_accels[] = { "Pause", NULL };
	static const char *fullscreen_accels[] = { "F11", NULL };
	gtk_application_set_accels_for_action(app, "app.open", open_accels);
	gtk_application_set_accels_for_action(app, "app.quit", quit_accels);
	gtk_application_set_accels_for_action(app, "app.run", run_accels);
	gtk_application_set_accels_for_action(app, "app.fullscreen", fullscreen_accels);

	const int status = g_application_run(G_APPLICATION(app), argc, argv);

	desmume_emu_shutdown();
	g_free(sInitialRom);
	sInitialRom = NULL;

	g_object_unref(app);
	delete pConfig;
	pConfig = NULL;

	return status;
}
