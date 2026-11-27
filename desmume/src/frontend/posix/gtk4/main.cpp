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
#include <stdlib.h>
#include <string.h>

#include "config.h"
#include "configdialog.h"
#include "display.h"
#include "emulation.h"
#include "tools.h"
#include "tools/movie.h"
#if defined(HAVE_LUA)
#include "tools/luaScriptConsole.h"
#endif

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
static GObject *pFastForwardShortcut = NULL;
static GtkWidget *pToolBar = NULL;
static GtkWidget *pStatusBar = NULL;
static GtkWidget *pStatusLabel = NULL;
static GtkWidget *pSpeedLabel = NULL;
static GtkWidget *pRunButton = NULL;
static GtkWidget *pDisplayArea = NULL;
static GtkApplication *pApp = NULL;

/* Frame sources are kept so a window close can stop them; otherwise the
 * 60 Hz tick keeps calling into a display widget that has already been
 * destroyed (see the close-request/destroy handlers below). */
static guint sFrameTickSource = 0;
static guint sTestFrameSource = 0;

/* A ROM given on the command line is loaded once the window exists, so the
 * status line and title can be updated through the normal path. */
static char *sInitialRom = NULL;

/* Set while a background ROM load is in flight; prevents a late GTask
 * callback from touching the UI after the window has been torn down. */
static gboolean sShuttingDown = FALSE;

/* While a load runs on a worker thread the 60 Hz tick must not touch the
 * core (NDS_LoadROM replaces the whole memory map).  Both this flag and the
 * tick run on the main thread, so no atomics are needed. */
static gboolean sLoadingRom = FALSE;

/* A background load touches the core, so shutdown must wait for any in-flight
 * worker before NDS_DeInit() runs.  The counter is decremented on the worker
 * itself, so waiting here cannot deadlock even if the main loop is exiting. */
static GMutex sLoadLock;
static GCond sLoadCond;
static guint sLoadsInFlight = 0;

/* Dynamically rebuilt "Open recent" submenu (GtkRecentManager is gone in 4). */
static GMenu *sRecentMenu = NULL;

static void SetRunButton(gboolean running);
static void SetStatus(const char *text);

/* Host accessors used by the tool windows (declared in tools.h). */
desmume::config::Config *
desmume_host_config(void)
{
	return pConfig;
}

const char *
desmume_host_rom_base_name(void)
{
	return desmume_emu_rom_name();
}

#define RECENT_MAX 10

static char *
RecentConfigPath(void)
{
	return g_build_filename(g_get_user_config_dir(), "desmume", "recent-roms.ini", NULL);
}

static GKeyFile *
RecentLoadKeyFile(void)
{
	GKeyFile *keyfile = g_key_file_new();
	char *path = RecentConfigPath();
	g_key_file_load_from_file(keyfile, path, G_KEY_FILE_NONE, NULL);
	g_free(path);
	return keyfile;
}

static void
RecentSaveKeyFile(GKeyFile *keyfile)
{
	char *path = RecentConfigPath();
	char *dir = g_path_get_dirname(path);
	g_mkdir_with_parents(dir, 0700);
	g_free(dir);
	g_key_file_save_to_file(keyfile, path, NULL);
	g_free(path);
}

static gchar **
RecentGetURIs(GKeyFile *keyfile, gsize *length)
{
	GError *error = NULL;
	gchar **uris = g_key_file_get_string_list(keyfile, "Recent", "ROMs", length, &error);
	if (error != NULL)
	{
		g_clear_error(&error);
		if (length != NULL)
		{
			*length = 0;
		}
	}
	return uris;
}

/* Replace the stored list with `list`.  The strings are borrowed from the
 * caller's GPtrArray (which stays alive and must still be freed by caller). */
static void
RecentSetURIs(GPtrArray *list)
{
	GKeyFile *keyfile = RecentLoadKeyFile();
	gchar **uris = (gchar **)g_malloc0((list->len + 1) * sizeof(gchar *));
	for (guint i = 0; i < list->len; ++i)
	{
		uris[i] = (gchar *)g_ptr_array_index(list, i);
	}
	g_key_file_set_string_list(keyfile, "Recent", "ROMs",
	                           (const gchar * const *)uris, list->len);
	RecentSaveKeyFile(keyfile);
	g_key_file_unref(keyfile);
	g_free(uris);
}

static void
RecentAdd(const char *path)
{
	char *uri = g_filename_to_uri(path, NULL, NULL);
	if (uri == NULL)
	{
		return;
	}

	GKeyFile *keyfile = RecentLoadKeyFile();
	gsize length = 0;
	gchar **old = RecentGetURIs(keyfile, &length);

	GPtrArray *list = g_ptr_array_new_with_free_func(g_free);
	g_ptr_array_add(list, g_strdup(uri));
	for (gsize i = 0; i < length; ++i)
	{
		if (g_strcmp0(old[i], uri) != 0)
		{
			g_ptr_array_add(list, g_strdup(old[i]));
		}
	}
	if (list->len > RECENT_MAX)
	{
		g_ptr_array_set_size(list, RECENT_MAX);
	}
	g_strfreev(old);
	g_key_file_unref(keyfile);

	RecentSetURIs(list);
	g_ptr_array_unref(list);
	g_free(uri);
}

static void
RecentRemove(const char *uri)
{
	GKeyFile *keyfile = RecentLoadKeyFile();
	gsize length = 0;
	gchar **old = RecentGetURIs(keyfile, &length);

	GPtrArray *list = g_ptr_array_new_with_free_func(g_free);
	for (gsize i = 0; i < length; ++i)
	{
		if (g_strcmp0(old[i], uri) != 0)
		{
			g_ptr_array_add(list, g_strdup(old[i]));
		}
	}
	g_strfreev(old);
	g_key_file_unref(keyfile);

	RecentSetURIs(list);
	g_ptr_array_unref(list);
}

static void
RecentRebuildMenu(void)
{
	if (sRecentMenu == NULL)
	{
		return;
	}
	g_menu_remove_all(sRecentMenu);

	GKeyFile *keyfile = RecentLoadKeyFile();
	gsize length = 0;
	gchar **uris = RecentGetURIs(keyfile, &length);
	for (gsize i = 0; i < length; ++i)
	{
		char *path = g_filename_from_uri(uris[i], NULL, NULL);
		if (path == NULL)
		{
			continue;
		}
		char *label = g_path_get_basename(path);
		GMenuItem *item = g_menu_item_new(label, NULL);
		g_menu_item_set_action_and_target_value(
			item, "app.recent", g_variant_new_string(uris[i]));
		g_menu_append_item(sRecentMenu, item);
		g_object_unref(item);
		g_free(label);
		g_free(path);
	}
	g_strfreev(uris);
	g_key_file_unref(keyfile);
}

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

/* The right-hand status label shows the measured emulation FPS.  It is only
 * rewritten when the value changes, so the once-per-second counter does not
 * trigger a redraw on every one of the 60 ticks in between. */
static void UpdateSpeedLabel(void)
{
	if (pSpeedLabel == NULL)
	{
		return;
	}

	char text[32];
	const guint fps = desmume_emu_get_fps();
	g_snprintf(text, sizeof(text), "%u FPS", fps);
	const char *current = gtk_label_get_text(GTK_LABEL(pSpeedLabel));
	if ((current == NULL) || (strcmp(current, text) != 0))
	{
		gtk_label_set_text(GTK_LABEL(pSpeedLabel), text);
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

/* Stop all asynchronous work that touches either the core or the display
 * widget.  Called before the window is destroyed and again from main() so it
 * must be idempotent.  Clearing the frame sink first is what prevents a late
 * tick from dereferencing a dead DesmumeDisplay. */
static void
StopEmulation(void)
{
	sShuttingDown = TRUE;
	desmume_emu_set_running(FALSE);
	desmume_emu_set_frame_sink(NULL, NULL);
	desmume_tools_close_all();

	if (sFrameTickSource != 0)
	{
		g_source_remove(sFrameTickSource);
		sFrameTickSource = 0;
	}
	if (sTestFrameSource != 0)
	{
		g_source_remove(sTestFrameSource);
		sTestFrameSource = 0;
	}

	/* Wait for any background ROM load to finish before the caller tears the
	 * core down in desmume_emu_shutdown(). */
	g_mutex_lock(&sLoadLock);
	while (sLoadsInFlight > 0)
	{
		g_cond_wait(&sLoadCond, &sLoadLock);
	}
	g_mutex_unlock(&sLoadLock);
}

/* The emulation core runs on the GTK main thread: one NDS frame per 60 Hz
 * tick.  Without a ROM this is a no-op, so the callback can be installed once
 * when the window is built and left running for the whole application. */
static gboolean
OnFrameTick(gpointer user_data)
{
	(void)user_data;

	if (!sLoadingRom && desmume_emu_get_running() && desmume_emu_has_rom())
	{
		desmume_emu_step();
	}
	desmume_tools_update();
	UpdateSpeedLabel();
	return G_SOURCE_CONTINUE;
}

static void
PushFrameToDisplay(GBytes *frame, int width, int height, gpointer user_data)
{
	/* pDisplayArea is guarded by a weak pointer: once the window is gone GTK
	 * sets it to NULL.  A late tick can therefore no longer pass a dangling
	 * widget into the display code (the DESMUME_IS_DISPLAY assertion in the
	 * bug report).  The sink is also cleared by StopEmulation(). */
	GtkWidget *display = *(GtkWidget **)user_data;
	if (display == NULL)
	{
		g_bytes_unref(frame);
		return;
	}
	desmume_display_push_frame(DESMUME_DISPLAY(display), frame, width, height);
}

/* Closing the last window must also tear the emulator down.  GtkApplication's
 * automatic quit is not enough here: the frame source would otherwise keep
 * firing into a destroyed widget.  Stop first, let GTK destroy the window
 * (FALSE), and quit explicitly as a fallback for environments where
 * quit-on-last-window has been turned off. */
static gboolean
OnWindowCloseRequest(GtkWindow *window, gpointer user_data)
{
	(void)window;
	(void)user_data;

	StopEmulation();
	if (pApp != NULL)
	{
		g_application_quit(G_APPLICATION(pApp));
	}
	return FALSE;
}

/* DESMUME_GTK4_TEST_FRAME=1 still feeds a synthetic frame so the texture path
 * can be exercised without a ROM. */
static gboolean PushTestFrame(gpointer user_data)
{
	GtkWidget *widget = *(GtkWidget **)user_data;
	if ((widget == NULL) || !gtk_widget_get_mapped(widget))
	{
		return G_SOURCE_CONTINUE;
	}
	DesmumeDisplay *display = DESMUME_DISPLAY(widget);

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

	/* The static buffer is rewritten next tick, so copy it into an owned
	 * GBytes; the widget takes ownership and the textures release it when
	 * GSK is done (test-only path, the copy cost is irrelevant here). */
	GBytes *frame = g_bytes_new(buffer, sizeof(buffer));
	desmume_display_push_frame(display, frame, 256, 192 * 2);
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

static guint sHoldKey = 0;
static GdkModifierType sHoldMods = (GdkModifierType)0;
static guint sHoldKeycode = 0;
static gboolean sHoldActive = FALSE;

static void ApplyHoldShortcut(void)
{
	sHoldKey = 0;
	sHoldMods = (GdkModifierType)0;
	gtk_accelerator_parse(pConfig->fast_forward_hold.get().c_str(), &sHoldKey, &sHoldMods);
	if (sHoldKey == GDK_KEY_ISO_Left_Tab)
		sHoldKey = GDK_KEY_Tab;
	sHoldKey = gdk_keyval_to_lower(sHoldKey);
	sHoldActive = FALSE;
	desmume_emu_set_fast_forward_held(FALSE);

	static const char *tab[] = { "Tab", NULL };
	static const char *none[] = { NULL };
	gtk_application_set_accels_for_action(pApp, "app.fast-forward",
	                                      sHoldKey == GDK_KEY_Tab && sHoldMods == 0 ? none : tab);
}

static gboolean OnHoldKeyPressed(GtkEventControllerKey *controller, guint keyval,
                                  guint keycode, GdkModifierType state, gpointer data)
{
	(void)controller;
	(void)data;
	if (keyval == GDK_KEY_ISO_Left_Tab)
		keyval = GDK_KEY_Tab;
	keyval = gdk_keyval_to_lower(keyval);
	if (sHoldKey && keyval == sHoldKey &&
	    (state & gtk_accelerator_get_default_mod_mask()) == sHoldMods)
	{
		sHoldActive = TRUE;
		sHoldKeycode = keycode;
		desmume_emu_set_fast_forward_held(TRUE);
		return TRUE;
	}
	return FALSE;
}

static void OnHoldKeyReleased(GtkEventControllerKey *controller, guint keyval,
                               guint keycode, GdkModifierType state, gpointer data)
{
	(void)controller;
	(void)state;
	(void)data;
	GdkModifierType modifier = (GdkModifierType)0;
	switch (keyval)
	{
		case GDK_KEY_Shift_L: case GDK_KEY_Shift_R: modifier = GDK_SHIFT_MASK; break;
		case GDK_KEY_Control_L: case GDK_KEY_Control_R: modifier = GDK_CONTROL_MASK; break;
		case GDK_KEY_Alt_L: case GDK_KEY_Alt_R: modifier = GDK_ALT_MASK; break;
		case GDK_KEY_Super_L: case GDK_KEY_Super_R: modifier = GDK_SUPER_MASK; break;
		case GDK_KEY_Meta_L: case GDK_KEY_Meta_R: modifier = GDK_META_MASK; break;
		case GDK_KEY_Hyper_L: case GDK_KEY_Hyper_R: modifier = GDK_HYPER_MASK; break;
	}
	if (sHoldActive && (keycode == sHoldKeycode || (sHoldMods & modifier)))
	{
		sHoldActive = FALSE;
		desmume_emu_set_fast_forward_held(FALSE);
	}
}

static void OnWindowActiveChanged(GObject *window, GParamSpec *spec, gpointer data)
{
	(void)spec;
	(void)data;
	if (!gtk_window_is_active(GTK_WINDOW(window)))
	{
		sHoldActive = FALSE;
		desmume_emu_set_fast_forward_held(FALSE);
	}
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

/* Enable/disable the actions that only make sense once a ROM is loaded. */
static void
UpdateRomActions(void)
{
	const gboolean has_rom = desmume_emu_has_rom();
	static const char *const rom_actions[] = {
		"run", "reset", "save-state", "load-state", "screenshot", "close",
		"rominfo", "importbackup", "exportbackup"
	};
	for (guint i = 0; i < G_N_ELEMENTS(rom_actions); ++i)
	{
		GAction *action = (pApp != NULL)
			? g_action_map_lookup_action(G_ACTION_MAP(pApp), rom_actions[i])
			: NULL;
		if (action != NULL)
		{
			g_simple_action_set_enabled(G_SIMPLE_ACTION(action), has_rom);
		}
	}
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

	RecentRebuildMenu();
	UpdateRomActions();
}

/* ------------------------------------------------------- background load */

typedef struct
{
	char *path;      /* owned by the task data */
	char *error;     /* owned by the task data, may be NULL */
	gboolean success;
} LoadRomData;

static void
LoadRomDataFree(gpointer data)
{
	LoadRomData *load = (LoadRomData *)data;
	g_free(load->path);
	g_free(load->error);
	g_free(load);
}

/* Runs on a GTask worker thread: NDS_LoadROM does file IO and decompression
 * and can block for a noticeable time on large images. */
static void
LoadRomThread(GTask *task, gpointer source_object, gpointer task_data,
              GCancellable *cancellable)
{
	(void)source_object;
	(void)cancellable;

	LoadRomData *load = (LoadRomData *)task_data;
	load->success = desmume_emu_load_rom(load->path, &load->error);
	g_task_return_boolean(task, load->success);

	/* The worker is done touching the core.  Release shutdown's barrier from
	 * the worker itself so it cannot deadlock against the main loop. */
	g_mutex_lock(&sLoadLock);
	--sLoadsInFlight;
	g_cond_broadcast(&sLoadCond);
	g_mutex_unlock(&sLoadLock);
}

/* Runs on the main thread once the worker finishes. */
static void
LoadRomDone(GObject *source_object, GAsyncResult *result, gpointer user_data)
{
	(void)source_object;
	(void)user_data;

	GTask *task = G_TASK(result);
	LoadRomData *load = (LoadRomData *)g_task_get_task_data(task);
	const gboolean success = g_task_propagate_boolean(task, NULL);

	sLoadingRom = FALSE;

	if (sShuttingDown || (pWindow == NULL))
	{
		return;
	}

	if (!success || !desmume_emu_has_rom())
	{
		ShowErrorAlert((load != NULL && load->error != NULL)
			? load->error : "Could not open the ROM.");
		SetStatus("Ready");
		UpdateRomActions();
		return;
	}

	RecentAdd(load->path);
	desmume_config_note_rom_dir(pConfig, load->path);
	RomLoaded();
}

static void
LoadRomAsync(const char *path)
{
	if ((path == NULL) || sShuttingDown)
	{
		return;
	}

	/* Stop the frame loop before the worker swaps the ROM out from under it. */
	desmume_emu_set_running(FALSE);
	sLoadingRom = TRUE;
	SetStatus("Loading ROM…");

	LoadRomData *load = g_new0(LoadRomData, 1);
	load->path = g_strdup(path);

	g_mutex_lock(&sLoadLock);
	++sLoadsInFlight;
	g_mutex_unlock(&sLoadLock);

	GTask *task = g_task_new(NULL, NULL, LoadRomDone, NULL);
	g_task_set_task_data(task, load, LoadRomDataFree);
	g_task_run_in_thread(task, LoadRomThread);
	g_object_unref(task);
}

/* Synchronous variant used only for the command-line ROM, before the main
 * loop is pumping frames. */
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

	RecentAdd(path);
	desmume_config_note_rom_dir(pConfig, path);
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

	LoadRomAsync(path);
	g_free(path);
}

static void ActionOpen(GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
	(void)action;
	(void)parameter;
	(void)user_data;

	GtkFileDialog *dialog = gtk_file_dialog_new();
	gtk_file_dialog_set_title(dialog, "Open Nintendo DS ROM");

	/* Path Settings > "ROM files" is the starting folder; it is rewritten to
	 * the folder of the last opened ROM while "Remember the folder of the
	 * last opened ROM" is on. */
	{
		char *directory = desmume_config_rom_directory(pConfig);
		if (directory != NULL)
		{
			GFile *folder = g_file_new_for_path(directory);
			gtk_file_dialog_set_initial_folder(dialog, folder);
			g_object_unref(folder);
			g_free(directory);
		}
	}

	GtkFileFilter *filter = gtk_file_filter_new();
	gtk_file_filter_set_name(filter, "Nintendo DS ROMs");
	gtk_file_filter_add_pattern(filter, "*.nds");
	gtk_file_filter_add_pattern(filter, "*.NDS");
	gtk_file_filter_add_pattern(filter, "*.rom");
	gtk_file_filter_add_pattern(filter, "*.ROM");
	gtk_file_filter_add_pattern(filter, "*.zip");
	gtk_file_filter_add_pattern(filter, "*.gz");

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

static void ActionReset(GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
	(void)action;
	(void)parameter;
	(void)user_data;

	if (!desmume_emu_has_rom())
	{
		return;
	}
	desmume_emu_reset();
	SetStatus("ROM reset");
}

static void ActionClose(GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
	(void)action;
	(void)parameter;
	(void)user_data;

	if (!desmume_emu_has_rom())
	{
		return;
	}
	desmume_emu_unload_rom();
	gtk_window_set_title(GTK_WINDOW(pWindow), "DeSmuME");
	SetRunButton(FALSE);
	SetStatus("ROM closed");
	UpdateRomActions();
}

static void ActionSaveState(GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
	(void)action;
	(void)user_data;

	const guint32 slot = g_variant_get_uint32(parameter);
	if (!desmume_emu_save_state((int)slot))
	{
		return;
	}
	char *status = g_strdup_printf("Saved state %u", slot + 1);
	SetStatus(status);
	g_free(status);
}

static void ActionLoadState(GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
	(void)action;
	(void)user_data;

	const guint32 slot = g_variant_get_uint32(parameter);
	if (!desmume_emu_load_state((int)slot))
	{
		return;
	}
	char *status = g_strdup_printf("Loaded state %u", slot + 1);
	SetStatus(status);
	g_free(status);
}

static void ActionScreenshot(GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
	(void)action;
	(void)parameter;
	(void)user_data;

	/* Directory, image format and name pattern all come from Path Settings. */
	char *base = desmume_emu_rom_base_name();
	char *path = desmume_config_screenshot_path(pConfig,
	                                            (base != NULL) ? base : "");
	g_free(base);

	char *error = NULL;
	const gboolean ok = desmume_emu_screenshot(path, &error);
	if (ok)
	{
		char *status = g_strdup_printf("Screenshot: %s", path);
		SetStatus(status);
		g_free(status);
	}
	else
	{
		ShowErrorAlert(error != NULL ? error : "Could not save the screenshot.");
	}

	g_free(error);
	g_free(path);
}

static void ActionRecent(GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
	(void)action;
	(void)user_data;

	const char *uri = g_variant_get_string(parameter, NULL);
	char *path = g_filename_from_uri(uri, NULL, NULL);
	if (path == NULL)
	{
		RecentRemove(uri);
		RecentRebuildMenu();
		return;
	}
	LoadRomAsync(path);
	g_free(path);
}

/* Reflect the emulation state on the toolbar button.  Pausing from a menu
 * action has to go through here, otherwise the button keeps showing "Pause"
 * while the emulation is stopped. */
static void
SetRunButton(gboolean running)
{
	if (pRunButton == NULL)
	{
		return;
	}
	gtk_button_set_icon_name(GTK_BUTTON(pRunButton),
	                         running ? "media-playback-pause-symbolic"
	                                 : "media-playback-start-symbolic");
	gtk_widget_set_tooltip_text(pRunButton, running ? "Pause" : "Run");
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
	SetRunButton(now_running);

	SetStatus(now_running ? "Running" : "Paused");
}

/* ------------------------------------------------- ROM info / save files */

static void
OnCloseInfoClicked(GtkButton *button, gpointer user_data)
{
	(void)button;
	gtk_window_destroy(GTK_WINDOW(user_data));
}

static void
ActionRomInfo(GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
	(void)action;
	(void)parameter;
	(void)user_data;

	DesmumeRomInfoRow *rows = desmume_emu_rom_info();
	if (rows == NULL)
	{
		SetStatus("Open a ROM first (Ctrl+O).");
		return;
	}

	GtkWidget *window = gtk_window_new();
	gtk_window_set_title(GTK_WINDOW(window), "ROM Info");
	gtk_window_set_transient_for(GTK_WINDOW(window), GTK_WINDOW(pWindow));
	gtk_window_set_modal(GTK_WINDOW(window), TRUE);
	gtk_window_set_resizable(GTK_WINDOW(window), FALSE);

	GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 14);
	gtk_widget_set_margin_start(box, 16);
	gtk_widget_set_margin_end(box, 16);
	gtk_widget_set_margin_top(box, 16);
	gtk_widget_set_margin_bottom(box, 16);

	GtkWidget *grid = gtk_grid_new();
	gtk_grid_set_row_spacing(GTK_GRID(grid), 6);
	gtk_grid_set_column_spacing(GTK_GRID(grid), 12);

	int row = 0;
	for (const DesmumeRomInfoRow *item = rows; item->label != NULL; ++item, ++row)
	{
		GtkWidget *label = gtk_label_new(item->label);
		gtk_label_set_xalign(GTK_LABEL(label), 1.0f);
		gtk_widget_add_css_class(label, "dim-label");

		GtkWidget *value = gtk_label_new(item->value);
		gtk_label_set_xalign(GTK_LABEL(value), 0.0f);
		gtk_label_set_selectable(GTK_LABEL(value), TRUE);
		gtk_widget_set_hexpand(value, TRUE);

		gtk_grid_attach(GTK_GRID(grid), label, 0, row, 1, 1);
		gtk_grid_attach(GTK_GRID(grid), value, 1, row, 1, 1);
	}
	gtk_box_append(GTK_BOX(box), grid);

	GtkWidget *close_button = gtk_button_new_with_label("Close");
	gtk_widget_set_halign(close_button, GTK_ALIGN_END);
	GtkWidget *close_row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
	gtk_box_append(GTK_BOX(close_row), close_button);
	gtk_box_append(GTK_BOX(box), close_row);
	gtk_widget_grab_focus(close_button);

	GtkWindow *info = GTK_WINDOW(window);
	g_signal_connect(close_button, "clicked", G_CALLBACK(OnCloseInfoClicked), info);

	gtk_window_set_child(GTK_WINDOW(window), box);
	gtk_window_present(GTK_WINDOW(window));

	desmume_emu_rom_info_free(rows);
}

/* Backup import/export share the pausing and the "was it running" bookkeeping. */
typedef struct
{
	gboolean resume;  /* emulation was running when the dialog opened */
	gboolean writing; /* TRUE writes the battery out, FALSE reads it in */
} BackupDialog;

static void
OnBackupFileChosen(GObject *source, GAsyncResult *result, gpointer user_data)
{
	BackupDialog *dialog = (BackupDialog *)user_data;

	/* The window may have been closed while the picker was open. */
	if (pWindow == NULL)
	{
		g_free(dialog);
		return;
	}

	GError *error = NULL;

	GFile *file = dialog->writing
		? gtk_file_dialog_save_finish(GTK_FILE_DIALOG(source), result, &error)
		: gtk_file_dialog_open_finish(GTK_FILE_DIALOG(source), result, &error);

	if (file == NULL)
	{
		if ((error != NULL) &&
		    !g_error_matches(error, GTK_DIALOG_ERROR, GTK_DIALOG_ERROR_DISMISSED))
		{
			ShowErrorAlert(error->message);
		}
		g_clear_error(&error);
	}
	else
	{
		char *path = g_file_get_path(file);
		g_object_unref(file);

		if (path == NULL)
		{
			ShowErrorAlert("Only local files are supported.");
		}
		else
		{
			char *message = NULL;
			const gboolean ok = dialog->writing
				? desmume_emu_export_backup(path, &message)
				: desmume_emu_import_backup(path, &message);

			if (ok)
			{
				char *status = g_strdup_printf(
					dialog->writing ? "Exported save data: %s"
					               : "Imported save data: %s", path);
				SetStatus(status);
				g_free(status);
			}
			else
			{
				ShowErrorAlert((message != NULL) ? message
				                                 : "The save file could not be transferred.");
			}
			g_free(message);
			g_free(path);
		}
	}

	if (dialog->resume)
	{
		desmume_emu_set_running(TRUE);
		SetRunButton(TRUE);
	}
	g_free(dialog);
}

/* Pause, ask for a file and hand it to OnBackupFileChosen.  Pausing keeps the
 * backup device from being rewritten by the running game while it is being
 * replaced, which is also what GTK3 does. */
static void
ShowBackupDialog(gboolean writing)
{
	if (!desmume_emu_has_rom())
	{
		SetStatus("Open a ROM first (Ctrl+O).");
		return;
	}

	BackupDialog *dialog = g_new0(BackupDialog, 1);
	dialog->writing = writing;
	dialog->resume = desmume_emu_get_running();
	if (dialog->resume)
	{
		desmume_emu_set_running(FALSE);
		SetRunButton(FALSE);
	}

	GtkFileDialog *file_dialog = gtk_file_dialog_new();
	GtkFileFilter *filter = gtk_file_filter_new();
	gtk_file_filter_set_name(filter, "Battery saves (*.sav, *.dsv, *.duc, *.dss)");
	gtk_file_filter_add_pattern(filter, "*.sav");
	gtk_file_filter_add_pattern(filter, "*.SAV");
	gtk_file_filter_add_pattern(filter, "*.dsv");
	gtk_file_filter_add_pattern(filter, "*.DSV");
	gtk_file_filter_add_pattern(filter, "*.duc");
	gtk_file_filter_add_pattern(filter, "*.dss");
	GListStore *filters = g_list_store_new(GTK_TYPE_FILE_FILTER);
	g_list_store_append(filters, filter);
	gtk_file_dialog_set_filters(file_dialog, G_LIST_MODEL(filters));

	char *directory = desmume_config_sram_directory(pConfig);
	if (directory != NULL)
	{
		GFile *folder = g_file_new_for_path(directory);
		gtk_file_dialog_set_initial_folder(file_dialog, folder);
		g_object_unref(folder);
		g_free(directory);
	}

	if (writing)
	{
		gtk_file_dialog_set_title(file_dialog, "Export Backup Memory To…");
		gtk_file_dialog_set_accept_label(file_dialog, "Export");

		char *base = desmume_emu_rom_base_name();
		char *name = g_strdup_printf("%s.sav",
		                             ((base != NULL) && (*base != '\0'))
		                                 ? base : "desmume");
		gtk_file_dialog_set_initial_name(file_dialog, name);
		g_free(name);
		g_free(base);

		gtk_file_dialog_save(file_dialog, GTK_WINDOW(pWindow), NULL,
		                     OnBackupFileChosen, dialog);
	}
	else
	{
		gtk_file_dialog_set_title(file_dialog, "Import Backup Memory From…");
		gtk_file_dialog_set_accept_label(file_dialog, "Import");
		gtk_file_dialog_open(file_dialog, GTK_WINDOW(pWindow), NULL,
		                     OnBackupFileChosen, dialog);
	}

	g_object_unref(file_dialog);
	g_object_unref(filter);
	g_object_unref(filters);
}

static void ActionImportBackup(GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
	(void)action;
	(void)parameter;
	(void)user_data;
	ShowBackupDialog(FALSE);
}

static void ActionExportBackup(GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
	(void)action;
	(void)parameter;
	(void)user_data;
	ShowBackupDialog(TRUE);
}

/* ADVANsCEne XML -> desmume.ddb beside the executable.  The core reads that
 * file when it detects a save type, so no reload is needed: the next ROM load
 * picks it up. */
static void
OnDatabaseFileChosen(GObject *source, GAsyncResult *result, gpointer user_data)
{
	(void)user_data;

	GError *error = NULL;
	GFile *file = gtk_file_dialog_open_finish(GTK_FILE_DIALOG(source), result, &error);
	if (file == NULL)
	{
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

	char *message = NULL;
	guint records = 0;
	const gboolean ok = desmume_emu_import_advanscene(path, &records, &message);

	if (ok)
	{
		char *status = g_strdup_printf("ADVANsCEne database imported: %u records",
		                               records);
		SetStatus(status);
		g_free(status);
	}
	else
	{
		ShowErrorAlert((message != NULL) ? message
		                                 : "The database could not be imported.");
	}

	g_free(message);
	g_free(path);
}

static void ActionImportDatabase(GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
	(void)action;
	(void)parameter;
	(void)user_data;

	GtkFileDialog *dialog = gtk_file_dialog_new();
	gtk_file_dialog_set_title(dialog, "Import ADVANsCEne Database");

	GtkFileFilter *filter = gtk_file_filter_new();
	gtk_file_filter_set_name(filter, "ADVANsCEne database (XML)");
	gtk_file_filter_add_pattern(filter, "*.xml");
	gtk_file_filter_add_pattern(filter, "*.XML");
	GListStore *filters = g_list_store_new(GTK_TYPE_FILE_FILTER);
	g_list_store_append(filters, filter);
	gtk_file_dialog_set_filters(dialog, G_LIST_MODEL(filters));

	gtk_file_dialog_open(dialog, GTK_WINDOW(pWindow), NULL,
	                     OnDatabaseFileChosen, NULL);

	g_object_unref(dialog);
	g_object_unref(filter);
	g_object_unref(filters);
}

/* ------------------------------------------------------ save type / layers */

/* Same order as save_types[] in mc.cpp; the strings are the menu targets. */
static const char *const kSaveTypeNames[] =
{
	"autodetect", "eeprom-4k", "eeprom-64k", "eeprom-512k",
	"fram-256k", "flash-2m", "flash-4m"
};

static const char *const kSaveTypeLabels[] =
{
	"Autodetect", "EEPROM 4kbit", "EEPROM 64kbit", "EEPROM 512kbit",
	"FRAM 256kbit", "FLASH 2Mbit", "FLASH 4Mbit"
};

static void
ActionSaveType(GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
	(void)user_data;

	const char *name = g_variant_get_string(parameter, NULL);
	int type = 0;
	for (guint i = 0; i < G_N_ELEMENTS(kSaveTypeNames); ++i)
	{
		if (g_strcmp0(name, kSaveTypeNames[i]) == 0)
		{
			type = (int)i;
			break;
		}
	}

	g_simple_action_set_state(action, parameter);
	desmume_emu_set_save_type(type);

	if (pConfig != NULL)
	{
		pConfig->savetype = type;
		pConfig->save();
	}

	char *status = g_strdup_printf("Save type: %s", kSaveTypeLabels[type]);
	SetStatus(status);
	g_free(status);
}

/* Main BG 0..3 and OBJ, then the same five on the sub engine. */
static const struct
{
	const char *name;
	guint layer;
} kLayerActions[] =
{
	{ "layermainbg0", 0 },
	{ "layermainbg1", 1 },
	{ "layermainbg2", 2 },
	{ "layermainbg3", 3 },
	{ "layermainobj",  4 },
	{ "layersubbg0", 5 },
	{ "layersubbg1", 6 },
	{ "layersubbg2", 7 },
	{ "layersubbg3", 8 },
	{ "layersubobj",  9 }
};

static void
OnLayerToggled(GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
	(void)parameter;

	const guint layer = GPOINTER_TO_UINT(user_data);
	GVariant *state = g_action_get_state(G_ACTION(action));
	const gboolean active = g_variant_get_boolean(state) ? FALSE : TRUE;
	g_variant_unref(state);

	/* Leave the check mark alone when there is no core to change yet. */
	if (!desmume_emu_set_layer_enabled((int)layer, active))
	{
		return;
	}

	g_simple_action_set_state(action, g_variant_new_boolean(active));
}

static void
InstallLayerActions(GtkApplication *app)
{
	for (guint i = 0; i < G_N_ELEMENTS(kLayerActions); ++i)
	{
		GSimpleAction *action = g_simple_action_new_stateful(
			kLayerActions[i].name, NULL, g_variant_new_boolean(TRUE));
		g_signal_connect(action, "activate", G_CALLBACK(OnLayerToggled),
		                 GUINT_TO_POINTER(kLayerActions[i].layer));
		g_action_map_add_action(G_ACTION_MAP(app), G_ACTION(action));
		g_object_unref(action);
	}
}

/* ------------------------------------------------------ speed / frame (P3d) */

static void
ActionFrameAdvance(GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
	(void)action;
	(void)parameter;
	(void)user_data;

	if (!desmume_emu_has_rom())
	{
		SetStatus("Open a ROM first (Ctrl+O).");
		return;
	}

	/* Frame advance implies pausing: run exactly one frame while halted. */
	if (desmume_emu_get_running())
	{
		desmume_emu_set_running(FALSE);
		SetRunButton(FALSE);
	}

	desmume_emu_step_frame_advance();
	SetStatus("Frame advanced");
}

static void
FpsLimiterChanged(GSimpleAction *action, GVariant *value, gpointer user_data){
	(void)user_data;

	const gboolean enabled = g_variant_get_boolean(value);
	g_simple_action_set_state(action, value);
	desmume_emu_set_fps_limiter(enabled);
	if (pConfig != NULL)
	{
		pConfig->fpslimiter.set(enabled ? true : false);
		pConfig->save();
	}
	SetStatus(enabled ? "FPS limiter on" : "FPS limiter off (fast forward)");
}

static void
AutoFrameskipChanged(GSimpleAction *action, GVariant *value, gpointer user_data)
{
	(void)user_data;

	const gboolean enabled = g_variant_get_boolean(value);
	g_simple_action_set_state(action, value);
	desmume_emu_set_auto_frameskip(enabled);
	if (pConfig != NULL)
	{
		pConfig->autoframeskip.set(enabled ? true : false);
		pConfig->save();
	}
	SetStatus(enabled ? "Auto frameskip on" : "Auto frameskip off");
}

static void
ActionFrameskip(GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
	(void)action;
	(void)user_data;

	const char *target = g_variant_get_string(parameter, NULL);
	int value = atoi(target);
	if (value < 0) value = 0;
	if (value > 9) value = 9;

	desmume_emu_set_frameskip(value);
	if (pConfig != NULL)
	{
		pConfig->frameskip.set(value);
		pConfig->save();
	}

	char state[8];
	g_snprintf(state, sizeof(state), "%d", value);
	GSimpleAction *skip_action = G_SIMPLE_ACTION(
		g_action_map_lookup_action(G_ACTION_MAP(pApp), "frameskip"));
	if (skip_action != NULL)
	{
		g_simple_action_set_state(skip_action, g_variant_new_string(state));
	}

	char *status = g_strdup_printf("Frameskip: %d", value);
	SetStatus(status);
	g_free(status);
}

static void
FastForwardChanged(GSimpleAction *action, GVariant *value, gpointer user_data)
{
	(void)user_data;

	const gboolean enabled = g_variant_get_boolean(value);
	g_simple_action_set_state(action, value);
	desmume_emu_set_fast_forward(enabled);
	SetStatus(enabled ? "Fast forward on" : "Fast forward off");
}

/* ------------------------------------------------------------ view settings */

/* GTK3's nds_screen.gap_size value; the display stores it in framebuffer
 * pixels and computes the on-screen gap from its own scale.  Matches
 * gtk/main.cpp's GAP_SIZE so the two frontends look identical. */
#define DESMUME_GAP_SIZE 64

/* Push the persisted view configuration into the display widget.  Called once
 * after the window is built and from the individual view actions.  Config
 * values use the same numbering as DesmumeLayout / GTK3 orientation_enum. */
static void
ApplyViewSettings(void)
{
	if ((pDisplayArea == NULL) || (pConfig == NULL))
	{
		return;
	}

	DesmumeDisplay *display = DESMUME_DISPLAY(pDisplayArea);

	int orient = pConfig->view_orient.get();
	if ((orient < DESMUME_LAYOUT_VERTICAL) || (orient > DESMUME_LAYOUT_HYBRID_VERTICAL))
	{
		orient = DESMUME_LAYOUT_VERTICAL;
		pConfig->view_orient.set(orient);
	}
	desmume_display_set_layout(display, (DesmumeLayout)orient);

	int rotation = pConfig->view_rot.get();
	switch (rotation)
	{
	case 0: case 90: case 180: case 270:
		break;
	default:
		rotation = 0;
		pConfig->view_rot.set(rotation);
		break;
	}
	desmume_display_set_rotation(display, rotation);

	desmume_display_set_swap(display, pConfig->view_swap.get() ? TRUE : FALSE);
	desmume_display_set_gap(display, pConfig->view_gap.get() ? DESMUME_GAP_SIZE : 0);
}

/* Each view action updates the widget through ApplyViewSettings() and the
 * matching GAction state so the radio/toggle menu items stay in sync. */
static void
SetActionStringState(const char *name, const char *value)
{
	GSimpleAction *action = G_SIMPLE_ACTION(
		g_action_map_lookup_action(G_ACTION_MAP(pApp), name));
	if (action != NULL)
	{
		g_simple_action_set_state(action, g_variant_new_string(value));
	}
}

static void
SetActionBooleanState(const char *name, gboolean value)
{
	GSimpleAction *action = G_SIMPLE_ACTION(
		g_action_map_lookup_action(G_ACTION_MAP(pApp), name));
	if (action != NULL)
	{
		g_simple_action_set_state(action, g_variant_new_boolean(value));
	}
}

static void
ActionOrient(GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
	(void)action;
	(void)user_data;

	const char *target = g_variant_get_string(parameter, NULL);
	int orient = DESMUME_LAYOUT_VERTICAL;
	const char *state = "vertical";

	if (g_strcmp0(target, "horizontal") == 0)
	{
		orient = DESMUME_LAYOUT_HORIZONTAL;
		state = "horizontal";
	}
	else if (g_strcmp0(target, "single") == 0)
	{
		orient = DESMUME_LAYOUT_SINGLE;
		state = "single";
	}
	else if (g_strcmp0(target, "hybrid_equal") == 0)
	{
		orient = DESMUME_LAYOUT_HYBRID_EQUAL;
		state = "hybrid_equal";
	}
	else if (g_strcmp0(target, "hybrid_vertical") == 0)
	{
		orient = DESMUME_LAYOUT_HYBRID_VERTICAL;
		state = "hybrid_vertical";
	}

	if (pConfig != NULL)
	{
		pConfig->view_orient.set(orient);
		pConfig->save();
	}
	SetActionStringState("orient", state);
	ApplyViewSettings();
}

static void
ActionRotate(GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
	(void)action;
	(void)user_data;

	int rotation = atoi(g_variant_get_string(parameter, NULL));
	switch (rotation)
	{
	case 0: case 90: case 180: case 270:
		break;
	default:
		rotation = 0;
		break;
	}

	if (pConfig != NULL)
	{
		pConfig->view_rot.set(rotation);
		pConfig->save();
	}
	char state[8];
	g_snprintf(state, sizeof(state), "%d", rotation);
	SetActionStringState("rotate", state);
	ApplyViewSettings();
}

static void
SwapScreensChanged(GSimpleAction *action, GVariant *value, gpointer user_data)
{
	(void)user_data;

	const gboolean swap = g_variant_get_boolean(value);
	g_simple_action_set_state(action, value);
	if (pConfig != NULL)
	{
		pConfig->view_swap.set(swap ? true : false);
		pConfig->save();
	}
	ApplyViewSettings();
}

static void
GapChanged(GSimpleAction *action, GVariant *value, gpointer user_data)
{
	(void)user_data;

	const gboolean gap = g_variant_get_boolean(value);
	g_simple_action_set_state(action, value);
	if (pConfig != NULL)
	{
		pConfig->view_gap.set(gap ? true : false);
		pConfig->save();
	}
	ApplyViewSettings();
}

/* P7a: GPU filter radio action. The string target is the WGPUPresentUpscaler
 * value; "0" (Off) frees the WebGPU device and restores the plain path. */
static void ActionGpuFilter(GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
	(void)user_data;
	const char *value = g_variant_get_string(parameter, NULL);
	int filter = (value != NULL) ? (int)g_ascii_strtoll(value, NULL, 10) : 0;

	/* Reuse the core's validator so a stale config/menu value cannot select an
	 * index outside the shader chain table. */
	if (!desmume_emu_set_gpu_filter(filter,
	                                (pConfig != NULL) ? pConfig->gpuScaleFactor.get() : 1.0f))
	{
		filter = 0;
		desmume_emu_set_gpu_filter(0, 1.0f);
	}

	g_simple_action_set_state(action, g_variant_new_string(value != NULL ? value : "0"));

	if (pConfig != NULL)
	{
		pConfig->gpuFilter.set(filter);
		/* The GTK4 frontend has no separate presentation switch: a non-Off
		 * filter is what enables the WebGPU off-screen path at startup. */
		pConfig->presentationWGPU.set(filter != 0);
		pConfig->save();
	}
	char state[16];
	g_snprintf(state, sizeof(state), "%d", filter);
	SetActionStringState("gpufilter", state);
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
		pFastForwardShortcut = gtk_builder_get_object(builder, "fast-forward-shortcut");
		g_object_unref(builder);

		gtk_window_set_transient_for(GTK_WINDOW(pShortcutsWindow), GTK_WINDOW(pWindow));
		g_signal_connect(pShortcutsWindow, "close-request",
		                 G_CALLBACK(OnShortcutsClose), NULL);
	}

	g_object_set(pFastForwardShortcut, "title",
	             sHoldKey == GDK_KEY_Tab && sHoldMods == 0
	                 ? "Fast forward (hold)" : "Fast forward toggle", NULL);
	gtk_window_present(GTK_WINDOW(pShortcutsWindow));
}

static GMenuModel *LoadMenuModel(void)
{
	GtkBuilder *builder = gtk_builder_new_from_resource(
		"/org/desmume/DeSmuME/menu.ui");
	GMenuModel *menu = G_MENU_MODEL(g_object_ref(
		gtk_builder_get_object(builder, "menubar")));
	sRecentMenu = G_MENU(g_object_ref(
		gtk_builder_get_object(builder, "open_recent")));
#ifdef HAVE_LIBSOUNDTOUCH
	GMenu *audio_sync_menu = G_MENU(gtk_builder_get_object(
		builder, "audio-synchronisation"));
	if (audio_sync_menu != NULL)
	{
		GMenuItem *sync_p = g_menu_item_new(
			"_P: Time-stretch (keeps pitch)", "app.spu-mode(\"sync-p\")");
		g_menu_append_item(audio_sync_menu, sync_p);
		g_object_unref(sync_p);
	}
#endif
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

/* Open a diagnostic tool by index.  The index maps to desmume_tools' table,
 * matching the numeric targets in the Tools menu. */
static void ActionTool(GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
	(void)action;
	(void)user_data;

	const guint32 index = g_variant_get_uint32(parameter);
	desmume_tools_open(index);
}

/* Cheats are applied by the core, so this only stops it from doing so; the
 * cheat list itself is left intact, matching the GTK3 debugging switch. */
static void CheatsDisableChanged(GSimpleAction *action, GVariant *value, gpointer user_data)
{
	(void)user_data;

	g_simple_action_set_state(action, value);
	const gboolean disabled = g_variant_get_boolean(value);
	desmume_emu_set_cheats_disabled(disabled);
	if (pConfig != NULL)
	{
		pConfig->cheats_disabled = (disabled != FALSE);
		pConfig->save();
	}
}

/* --------------------------------------------------------- config dialog */

static void OnConfigApplied(gpointer user_data)
{
	(void)user_data;
	ApplyHoldShortcut();

	/* The dialog may have changed the GPU filter, which also has a View-menu
	 * radio action; keep its state in step. */
	char filter_state[16];
	g_snprintf(filter_state, sizeof(filter_state), "%d",
	           (pConfig != NULL) ? pConfig->gpuFilter.get() : 0);
	SetActionStringState("gpufilter", filter_state);
	SetStatus("Configuration applied");
}

static void ActionConfig(GSimpleAction *action, GVariant *parameter,
                         gpointer user_data)
{
	(void)action;
	(void)parameter;
	(void)user_data;

	if (pConfig == NULL)
	{
		return;
	}

	desmume_config_dialog_show(GTK_WINDOW(pWindow), pConfig,
	                           OnConfigApplied, NULL);
}

/* Config > the individual pages.  GTK3 had one dialog per topic; GTK4 keeps
 * them in one window, so these shortcuts just pick the page. */
static void ActionConfigPage(GSimpleAction *action, GVariant *parameter,
                             gpointer user_data)
{
	(void)action;
	(void)user_data;

	if (pConfig == NULL)
	{
		return;
	}

	const char *name = g_variant_get_string(parameter, NULL);
	static const struct { const char *name; int page; } pages[] =
	{
		{ "emulation",   DESMUME_CONFIG_PAGE_EMULATION },
		{ "graphic",     DESMUME_CONFIG_PAGE_GRAPHIC },
		{ "sound",       DESMUME_CONFIG_PAGE_SOUND },
		{ "firmware",    DESMUME_CONFIG_PAGE_FIRMWARE },
		{ "slots",       DESMUME_CONFIG_PAGE_SLOTS },
		{ "wifi",        DESMUME_CONFIG_PAGE_WIFI },
		{ "microphone",  DESMUME_CONFIG_PAGE_MICROPHONE },
		{ "paths",       DESMUME_CONFIG_PAGE_PATHS },
		{ "hotkeys",     DESMUME_CONFIG_PAGE_HOTKEYS },
	};

	int page = DESMUME_CONFIG_PAGE_EMULATION;
	for (guint i = 0; i < G_N_ELEMENTS(pages); ++i)
	{
		if (g_strcmp0(name, pages[i].name) == 0)
		{
			page = pages[i].page;
			break;
		}
	}

	desmume_config_dialog_show_page(GTK_WINDOW(pWindow), pConfig,
	                                OnConfigApplied, NULL, page);
}

/* ---- movie actions (P4b) --------------------------------------------- */

static void
UpdateMovieActions(void)
{
	GSimpleAction *stop = G_SIMPLE_ACTION(
		g_action_map_lookup_action(G_ACTION_MAP(pApp), "stopmovie"));
	if (stop != NULL)
	{
		g_simple_action_set_enabled(stop, desmume_emu_movie_active());
	}
}

static void ActionPlayMovie(GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
	(void)action;
	(void)parameter;
	(void)user_data;
	desmume_movie_play_dialog_show(GTK_WINDOW(pWindow));
	UpdateMovieActions();
}

static void ActionRecordMovie(GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
	(void)action;
	(void)parameter;
	(void)user_data;
	desmume_movie_record_dialog_show(GTK_WINDOW(pWindow));
	UpdateMovieActions();
}

static void ActionStopMovie(GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
	(void)action;
	(void)parameter;
	(void)user_data;
	desmume_emu_stop_movie();
	UpdateMovieActions();
}

#if defined(HAVE_LUA)
static void ActionLuaConsole(GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
	(void)action;
	(void)parameter;
	(void)user_data;
	lua_script_open_console(GTK_WINDOW(pWindow));
}

static void ActionCloseLuaScripts(GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
	(void)action;
	(void)parameter;
	(void)user_data;
	lua_script_close_all();
}

static void ActionAboutLua(GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
	(void)action;
	(void)parameter;
	(void)user_data;
	lua_script_show_about(GTK_WINDOW(pWindow));
}
#endif

static const GActionEntry app_entries[] =
{
	/* Names match gtk/menu.ui so menu items can be enabled phase by phase. */
	{ "open",           ActionOpen,       NULL, NULL,   NULL },
	{ "recent",         ActionRecent,     "s",  NULL,   NULL },
	{ "quit",           ActionQuit,       NULL, NULL,   NULL },
	{ "run",            ActionRun,        NULL, NULL,   NULL },
	{ "frame-advance",  ActionFrameAdvance, NULL, NULL, NULL },
	{ "reset",          ActionReset,      NULL, NULL,   NULL },
	{ "close",          ActionClose,      NULL, NULL,   NULL },
	{ "save-state",     ActionSaveState,  "u",  NULL,   NULL },
	{ "load-state",     ActionLoadState,  "u",  NULL,   NULL },
	{ "screenshot",     ActionScreenshot, NULL, NULL,   NULL },
	{ "rominfo",        ActionRomInfo,    NULL, NULL,   NULL },
	{ "importbackup",   ActionImportBackup, NULL, NULL, NULL },
	{ "exportbackup",   ActionExportBackup, NULL, NULL, NULL },
	{ "importdb",       ActionImportDatabase, NULL, NULL, NULL },
	{ "savetype",       ActionSaveType,   "s",  "'autodetect'", NULL },
	{ "fps-limiter",    NULL,             NULL, "true", FpsLimiterChanged },
	{ "auto-frameskip", NULL,             NULL, "true", AutoFrameskipChanged },
	{ "frameskip",      ActionFrameskip,  "s",  "'0'",  NULL },
	{ "fast-forward",   NULL,             NULL, "false", FastForwardChanged },
	{ "fullscreen",     ActionFullscreen, NULL, NULL,   NULL },
	{ "about",          ActionAbout,      NULL, NULL,   NULL },
	{ "shortcuts",      ActionShortcuts,  NULL, NULL,   NULL },
	{ "orient",         ActionOrient,     "s",  "'vertical'", NULL },
	{ "rotate",         ActionRotate,     "s",  "'0'",       NULL },
	{ "swapscreens",    NULL,             NULL, "false", SwapScreensChanged },
	{ "gap",            NULL,             NULL, "false", GapChanged },
	{ "view-menu",      NULL,             NULL, "true", MenuBarChanged },
	{ "view-toolbar",   NULL,             NULL, "true", ToolbarChanged },
	{ "view-statusbar",  NULL,             NULL, "true", StatusbarChanged },
	{ "tool",           ActionTool,       "u",  NULL,   NULL },
	{ "gpufilter",      ActionGpuFilter,  "s",  "'0'",  NULL },
	{ "config",         ActionConfig,     NULL, NULL,   NULL },
	{ "configpage",     ActionConfigPage, "s",  NULL,   NULL },
	{ "cheatsdisable",  NULL,             NULL, "false", CheatsDisableChanged },
	{ "playmovie",      ActionPlayMovie,  NULL, NULL,   NULL },
	{ "recordmovie",    ActionRecordMovie, NULL, NULL,  NULL },
	{ "stopmovie",      ActionStopMovie,  NULL, NULL,   NULL }
#if defined(HAVE_LUA)
	, { "luaconsole",   ActionLuaConsole, NULL, NULL,   NULL }
	, { "closeluascripts", ActionCloseLuaScripts, NULL, NULL, NULL }
	, { "aboutlua",     ActionAboutLua,   NULL, NULL,   NULL }
#endif
};

static GtkWidget *MakeToolButton(const char *icon_name, const char *tooltip, const char *action_name)
{
	GtkWidget *button = gtk_button_new_from_icon_name(icon_name);
	gtk_widget_set_focusable(button, FALSE);
	gtk_widget_set_focus_on_click(button, FALSE);
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
	g_signal_connect(pWindow, "close-request",
	                 G_CALLBACK(OnWindowCloseRequest), NULL);
	g_signal_connect(pWindow, "notify::is-active", G_CALLBACK(OnWindowActiveChanged), NULL);
	GtkEventController *holdKey = gtk_event_controller_key_new();
	gtk_event_controller_set_propagation_phase(holdKey, GTK_PHASE_CAPTURE);
	g_signal_connect(holdKey, "key-pressed", G_CALLBACK(OnHoldKeyPressed), NULL);
	g_signal_connect(holdKey, "key-released", G_CALLBACK(OnHoldKeyReleased), NULL);
	gtk_widget_add_controller(pWindow, holdKey);

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

	GtkWidget *reset_button = MakeToolButton("view-refresh-symbolic",
	                                        "Reset ROM", "app.reset");
	gtk_box_append(GTK_BOX(pToolBar), reset_button);

	GtkWidget *spacer = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
	gtk_widget_set_hexpand(spacer, TRUE);
	gtk_box_append(GTK_BOX(pToolBar), spacer);

	GtkWidget *fullscreen_button = MakeToolButton("view-fullscreen-symbolic",
	                                              "Fullscreen", "app.fullscreen");
	gtk_box_append(GTK_BOX(pToolBar), fullscreen_button);

	gtk_box_append(GTK_BOX(root), pToolBar);

	pDisplayArea = desmume_display_new();
	/* Clear the global as soon as GTK finalises the widget, so the frame sink
	 * can never hand a stale pointer to desmume_display_push_frame(). */
	g_object_add_weak_pointer(G_OBJECT(pDisplayArea), (gpointer *)&pDisplayArea);
	gtk_widget_set_hexpand(pDisplayArea, TRUE);
	gtk_widget_set_vexpand(pDisplayArea, TRUE);
	g_signal_connect(pDisplayArea, "touch-press", G_CALLBACK(OnTouchPress), NULL);
	g_signal_connect(pDisplayArea, "touch-move", G_CALLBACK(OnTouchMove), NULL);
	g_signal_connect(pDisplayArea, "touch-release", G_CALLBACK(OnTouchRelease), NULL);
	g_signal_connect(pDisplayArea, "key-event", G_CALLBACK(OnKeyEvent), NULL);
	gtk_box_append(GTK_BOX(root), pDisplayArea);
	gtk_window_set_focus(GTK_WINDOW(pWindow), pDisplayArea);

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

	pSpeedLabel = gtk_label_new("0 FPS");
	gtk_widget_add_css_class(pSpeedLabel, "dim-label");
	gtk_box_append(GTK_BOX(pStatusBar), pSpeedLabel);

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
		gtk_widget_set_focusable(header_open, FALSE);
		gtk_widget_set_focus_on_click(header_open, FALSE);
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
	desmume_tools_set_parent(pWindow);

	GSimpleAction *menu_action = G_SIMPLE_ACTION(
		g_action_map_lookup_action(G_ACTION_MAP(app), "view-menu"));
	GSimpleAction *toolbar_action = G_SIMPLE_ACTION(
		g_action_map_lookup_action(G_ACTION_MAP(app), "view-toolbar"));
	GSimpleAction *statusbar_action = G_SIMPLE_ACTION(
		g_action_map_lookup_action(G_ACTION_MAP(app), "view-statusbar"));

	g_simple_action_set_state(menu_action, g_variant_new_boolean(pConfig->view_menu.get()));
	g_simple_action_set_state(toolbar_action, g_variant_new_boolean(pConfig->view_toolbar.get()));
	g_simple_action_set_state(statusbar_action, g_variant_new_boolean(pConfig->view_statusbar.get()));

	/* Synchronise the view menu state with the persisted configuration before
	 * pushing it into the display widget. */
	{
		const char *orient_state = "vertical";
		switch (pConfig->view_orient.get())
		{
		case DESMUME_LAYOUT_HORIZONTAL:    orient_state = "horizontal"; break;
		case DESMUME_LAYOUT_SINGLE:        orient_state = "single"; break;
		case DESMUME_LAYOUT_HYBRID_EQUAL:  orient_state = "hybrid_equal"; break;
		case DESMUME_LAYOUT_HYBRID_VERTICAL: orient_state = "hybrid_vertical"; break;
		default: break;
		}
		SetActionStringState("orient", orient_state);

		char rot_state[8];
		g_snprintf(rot_state, sizeof(rot_state), "%d", pConfig->view_rot.get());
		SetActionStringState("rotate", rot_state);
		SetActionBooleanState("swapscreens", pConfig->view_swap.get() ? TRUE : FALSE);
		SetActionBooleanState("gap", pConfig->view_gap.get() ? TRUE : FALSE);

		ApplyViewSettings();
	}

	/* Restore the persisted "Disable cheats" debugging switch and mirror it
	 * in the menu action without triggering a redundant config save. */
	{
		const gboolean cheats_disabled = pConfig->cheats_disabled.get() ? TRUE : FALSE;
		desmume_emu_set_cheats_disabled(cheats_disabled);
		SetActionBooleanState("cheatsdisable", cheats_disabled);
	}

	/* P7a: restore the optional WebGPU upscaler filter. A filter is only run
	 * when both presentation is enabled and a non-Off filter is selected;
	 * the default (Off) keeps the plain GSK texture path. Failure to create
	 * the device is non-fatal and emulation continues unfiltered. */
	{
		const gboolean wgpu_on = pConfig->presentationWGPU.get() ? TRUE : FALSE;
		const int filter = wgpu_on ? pConfig->gpuFilter.get() : 0;
		const float scale = pConfig->gpuScaleFactor.get();
		/* Pin the off-screen filter device to the backend saved by the
		 * Screen presentation combo (0 = let wgpu-native choose). Must be
		 * set before the filter creates its device below. */
		desmume_emu_set_presentation_backend(
			(guint32)pConfig->presentationWGPUBackend.get());
		desmume_emu_set_gpu_filter(filter, scale);
		char filter_state[16];
		g_snprintf(filter_state, sizeof(filter_state), "%d", filter);
		SetActionStringState("gpufilter", filter_state);
	}

	/* Push persisted emulation-speed settings into the core and mirror them
	 * in the toggle/radio actions so the menu checkmarks match reality. */
	{
		const gboolean fps_limiter = pConfig->fpslimiter.get() ? TRUE : FALSE;
		const gboolean auto_skip = pConfig->autoframeskip.get() ? TRUE : FALSE;
		int frameskip = pConfig->frameskip.get();
		if (frameskip < 0) frameskip = 0;
		if (frameskip > 9) frameskip = 9;

		desmume_emu_set_fps_limiter(fps_limiter);
		desmume_emu_set_auto_frameskip(auto_skip);
		desmume_emu_set_frameskip(frameskip);

		SetActionBooleanState("fps-limiter", fps_limiter);
		SetActionBooleanState("auto-frameskip", auto_skip);
		char state[8];
		g_snprintf(state, sizeof(state), "%d", frameskip);
		SetActionStringState("frameskip", state);
	}

	RecentRebuildMenu();
	UpdateRomActions();

	/* The save type is a plain index in the config; the menu uses string
	 * targets so the radio group can mark the active entry. */
	{
		int type = pConfig->savetype.get();
		if ((type < 0) || (type >= (int)G_N_ELEMENTS(kSaveTypeNames)))
		{
			type = 0;
		}
		desmume_emu_set_save_type(type);
		SetActionStringState("savetype", kSaveTypeNames[type]);
	}

	gtk_window_present(GTK_WINDOW(pWindow));

	/* Pass the address of the widget pointer, guarded by a weak pointer in
	 * BuildWindow, so a late frame after close sees NULL instead of a stale
	 * GObject. */
	desmume_emu_set_frame_sink(PushFrameToDisplay, &pDisplayArea);
	sFrameTickSource = g_timeout_add(1000 / 60, OnFrameTick, NULL);

	if (sInitialRom != NULL)
	{
		char *path = sInitialRom;
		sInitialRom = NULL;
		LoadRomAndReport(path);
		g_free(path);
	}

	if (g_getenv("DESMUME_GTK4_TEST_FRAME") != NULL)
	{
		sTestFrameSource = g_timeout_add(1000 / 60, PushTestFrame, &pDisplayArea);
	}

	if (pConfig->window_fullscreen.get())
	{
		gtk_window_fullscreen(GTK_WINDOW(pWindow));
	}
}

/* GDK 4.24 tracks Wayland presentation feedback per (frame clock, frame
 * counter) only, while several surfaces can share one clock: a popup inherits
 * its parent toplevel's clock (gdkpopup-wayland.c), and a state-only "empty
 * frame" (gdk_wayland_surface_handle_empty_frame()) asks for a frame without
 * a new buffer.  Showing or hiding a popover while the main window animates
 * therefore registers two feedback objects for the same frame; the second
 * presentation report arrives one refresh later and GDK logs it as
 * "Duplicate call to gdk_frame_timings_presented() ... off from expected".
 * The duplicate is discarded inside gdk_frame_timings_presented() and the
 * frame keeps the first timestamp, so nothing is lost.  Upstream fixed the
 * common case in 6b0b5ff7 ("wayland: Don't duplicate complete frames") and
 * restructured the tracking per surface in 2da76eb2 ("wayland: Rewrite frame
 * callback handling to use frames"), which the 4.24 branch does not have.
 * Drop exactly this one message so a GTK bookkeeping artifact does not look
 * like a DeSmuME bug; every other Gdk warning still reaches the default
 * writer. */
static GLogWriterOutput
FilterGdkPresentationWarning(GLogLevelFlags log_level, const GLogField *fields,
                             gsize n_fields, gpointer user_data)
{
	const char *domain = NULL;
	const char *message = NULL;

	for (gsize i = 0; i < n_fields; ++i)
	{
		if (g_strcmp0(fields[i].key, "GLIB_DOMAIN") == 0)
		{
			domain = (const char *)fields[i].value;
		}
		else if (g_strcmp0(fields[i].key, "MESSAGE") == 0)
		{
			message = (const char *)fields[i].value;
		}
	}

	if ((g_strcmp0(domain, "Gdk") == 0) && (message != NULL) &&
	    g_str_has_prefix(message, "Duplicate call to gdk_frame_timings_presented()"))
	{
		return G_LOG_WRITER_HANDLED;
	}

	return g_log_writer_default(log_level, fields, n_fields, user_data);
}

int main(int argc, char *argv[])
{
	g_set_prgname("desmume");
	g_set_application_name("DeSmuME");

	/* Must be installed before the first frame is drawn. */
	g_log_set_writer_func(FilterGdkPresentationWarning, NULL, NULL);

	/* Belt and braces: the core's microphone init may touch SDL on any path.
	 * Keep SDL from hijacking SIGINT/SIGTERM so Ctrl+C still terminates us. */
	g_setenv("SDL_NO_SIGNAL_HANDLERS", "1", TRUE);

	pConfig = new desmume::config::Config();
	pConfig->load();
	/* Emulation/BIOS/firmware/3D/addon/microphone/Wi-Fi/path settings.  This
	 * runs before the audio and graphics mirrors below so a 3D renderer that
	 * is created for the first ROM already sees the graphics switches. */
	desmume_config_apply(pConfig);
	desmume_emu_set_audio_config(pConfig->audio_enabled.get(),
	                             pConfig->audio_volume.get(),
	                             pConfig->audio_sync.get(),
	                             pConfig->audio_interpolation.get());
	desmume_emu_set_jit_config(pConfig->use_jit.get(),
	                           pConfig->jit_max_block_size.get());
	desmume_emu_set_graphics_config(pConfig->core3D.get(),
	                                pConfig->textureUpscale.get(),
	                                pConfig->textureDeposterize.get() ? TRUE : FALSE,
	                                pConfig->textureSmoothing.get() ? TRUE : FALSE,
	                                pConfig->highColorInterpolation.get() ? TRUE : FALSE,
	                                pConfig->multisampling.get()
	                                    ? pConfig->multisamplingSize.get() : 0);

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
	pApp = app;
	g_signal_connect(app, "activate", G_CALLBACK(OnActivate), NULL);

	g_action_map_add_action_entries(G_ACTION_MAP(app),
	                                app_entries, G_N_ELEMENTS(app_entries),
	                                app);
	/* The ten layer switches are separate stateful actions created at run
	 * time, because GActionEntry cannot pass the layer index through. */
	InstallLayerActions(app);

	/* Stop Movie is only meaningful while playback/recording is active. */
	UpdateMovieActions();

	static const char *open_accels[] = { "<Control>o", NULL };
	static const char *quit_accels[] = { "<Control>q", NULL };
	static const char *run_accels[] = { "Pause", NULL };
	static const char *frame_advance_accels[] = { "<Control>n", NULL };
	static const char *fullscreen_accels[] = { "F11", NULL };
	static const char *screenshot_accels[] = { "<Control>s", NULL };
	static const char *config_accels[] = { "<Control>p", NULL };
	gtk_application_set_accels_for_action(app, "app.open", open_accels);
	gtk_application_set_accels_for_action(app, "app.quit", quit_accels);
	gtk_application_set_accels_for_action(app, "app.run", run_accels);
	gtk_application_set_accels_for_action(app, "app.frame-advance", frame_advance_accels);
	ApplyHoldShortcut();
	gtk_application_set_accels_for_action(app, "app.fullscreen", fullscreen_accels);
	gtk_application_set_accels_for_action(app, "app.screenshot", screenshot_accels);
	gtk_application_set_accels_for_action(app, "app.config", config_accels);

	/* Slot accelerators for parameterised actions.  menu.ui also carries them,
	 * but GTK only auto-installs accels for the application menubar; the
	 * PopoverMenuBar needs them registered explicitly. */
	for (guint slot = 0; slot < 10; ++slot)
	{
		char *save_action = g_strdup_printf("app.save-state(uint32 %u)", slot);
		char *load_action = g_strdup_printf("app.load-state(uint32 %u)", slot);
		char *save_accel_str = g_strdup_printf("<Shift>F%u", slot + 1);
		char *load_accel_str = g_strdup_printf("F%u", slot + 1);
		const char *save_accels[] = { save_accel_str, NULL };
		const char *load_accels[] = { load_accel_str, NULL };
		gtk_application_set_accels_for_action(app, save_action, save_accels);
		gtk_application_set_accels_for_action(app, load_action, load_accels);
		g_free(save_action);
		g_free(load_action);
		g_free(save_accel_str);
		g_free(load_accel_str);
	}

	/* View actions with string parameters also need their accels registered
	 * explicitly for the PopoverMenuBar. */
	struct { const char *target; const char *accel; } orient_accels[] =
	{
		{ "vertical", "<Control>1" },
		{ "horizontal", "<Control>2" },
		{ "single", "<Control>0" },
		{ "hybrid_equal", "<Control>3" },
		{ "hybrid_vertical", "<Control>4" },
	};
	for (guint i = 0; i < G_N_ELEMENTS(orient_accels); ++i)
	{
		char *action = g_strdup_printf("app.orient('%s')", orient_accels[i].target);
		const char *accels[] = { orient_accels[i].accel, NULL };
		gtk_application_set_accels_for_action(app, action, accels);
		g_free(action);
	}
	static const char *swap_accels[] = { "space", NULL };
	gtk_application_set_accels_for_action(app, "app.swapscreens", swap_accels);

	const int status = g_application_run(G_APPLICATION(app), argc, argv);

	/* The window close handler already did this, but ActionQuit and a
	 * programmatic quit bypass it.  Stop here too before touching the core. */
	StopEmulation();
	desmume_emu_shutdown();
	g_free(sInitialRom);
	sInitialRom = NULL;

	pApp = NULL;
	g_object_unref(app);
	delete pConfig;
	pConfig = NULL;

	return status;
}
