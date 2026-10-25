/* luaScriptConsole.cpp - this file is part of DeSmuME
 *
 * Copyright (C) 2006-2025 DeSmuME Team
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

#ifdef HAVE_LUA

#include <gtk/gtk.h>
#include <glib.h>
#include <map>
#include <string>
#include <cstdio>
#include <cstring>
#include <unistd.h>
#include <sys/inotify.h>
#include <sys/select.h>

#include "lua-engine.h"

// Match the Windows console character limit
#define MAX_CONSOLE_CHARS 250000

struct LuaConsole {
	int uid;
	std::string filename;
	bool started;
	bool closeOnStop;

	GtkWidget *window;
	GtkWidget *path_entry;
	GtkWidget *browse_btn;
	GtkWidget *edit_btn;
	GtkWidget *run_btn;
	GtkWidget *stop_btn;
	GtkWidget *stdout_check;
	GtkWidget *output_view;
	GtkTextBuffer *output_buf;

	volatile bool watcher_running;
	GThread *watcher_thread;

	LuaConsole()
		: uid(0), started(false), closeOnStop(false),
		  watcher_running(false), watcher_thread(nullptr) {}
};

static std::map<int, LuaConsole*> g_consoles;
static int g_next_uid = 1;

// ────────────────────────────────────────
// File watcher (inotify — mirrors Windows FindFirstChangeNotification)
// ────────────────────────────────────────

struct WatcherArgs {
	LuaConsole  *con;
	std::string  filename;   // absolute path of the script
	std::string  directory;  // parent directory to watch
	std::string  basename;   // filename portion
};

static gboolean watcher_reload_idle(gpointer data)
{
	int uid = GPOINTER_TO_INT(data);
	auto it = g_consoles.find(uid);
	if (it == g_consoles.end()) return G_SOURCE_REMOVE;
	LuaConsole *con = it->second;
	if (con->watcher_running && !con->filename.empty()) {
		RequestAbortLuaScript(uid, "terminated to reload the script");
		RunLuaScriptFile(uid, con->filename.c_str());
	}
	return G_SOURCE_REMOVE;
}

static gpointer file_watcher_thread(gpointer arg)
{
	WatcherArgs *wa  = static_cast<WatcherArgs*>(arg);
	LuaConsole  *con = wa->con;
	int          uid = con->uid;

	int ifd = inotify_init();
	if (ifd < 0) { delete wa; return nullptr; }

	int wd = inotify_add_watch(ifd, wa->directory.c_str(),
	                           IN_CLOSE_WRITE | IN_MOVED_TO);

	char buf[4096] __attribute__((aligned(__alignof__(struct inotify_event))));

	while (con->watcher_running) {
		fd_set fds;
		FD_ZERO(&fds);
		FD_SET(ifd, &fds);
		struct timeval tv = {0, 500000}; // 500 ms poll, matching Windows timeout
		int ret = select(ifd + 1, &fds, nullptr, nullptr, &tv);
		if (!con->watcher_running) break;
		if (ret <= 0) continue;

		int len = read(ifd, buf, sizeof(buf));
		if (len < 0) continue;

		const struct inotify_event *ev;
		for (char *p = buf; p < buf + len;
		     p += sizeof(struct inotify_event) + ev->len) {
			ev = reinterpret_cast<const struct inotify_event*>(p);
			if (ev->len > 0 && wa->basename == ev->name) {
				g_idle_add(watcher_reload_idle, GINT_TO_POINTER(uid));
				break;
			}
		}
	}

	if (wd >= 0) inotify_rm_watch(ifd, wd);
	close(ifd);
	delete wa;
	return nullptr;
}

static void start_watcher(LuaConsole *con)
{
	if (con->filename.empty()) return;

	// Stop the previous watcher before starting a new one
	if (con->watcher_thread) {
		con->watcher_running = false;
		g_thread_join(con->watcher_thread);
		con->watcher_thread = nullptr;
	}

	std::string filename  = con->filename;
	std::string directory = filename;
	size_t slash = directory.rfind('/');
	if (slash != std::string::npos) directory.resize(slash);
	else directory = ".";
	std::string basename = filename.substr(
		slash != std::string::npos ? slash + 1 : 0);

	WatcherArgs *wa = new WatcherArgs{con, filename, directory, basename};
	con->watcher_running = true;
	con->watcher_thread  = g_thread_new("lua-watcher", file_watcher_thread, wa);
}

static void stop_watcher(LuaConsole *con)
{
	if (!con->watcher_thread) return;
	con->watcher_running = false;
	g_thread_join(con->watcher_thread);
	con->watcher_thread = nullptr;
}

// ────────────────────────────────────────
// Console output helpers
// ────────────────────────────────────────

static void console_append(LuaConsole *con, const char *str)
{
	// When "stdout" is checked, mirror Windows IDC_USE_STDOUT behaviour
	if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(con->stdout_check))) {
		fputs(str, stdout);
		fflush(stdout);
		return;
	}

	// Discard the first half of the buffer when it gets too long
	gint len = gtk_text_buffer_get_char_count(con->output_buf);
	if (len >= MAX_CONSOLE_CHARS) {
		GtkTextIter start, mid;
		gtk_text_buffer_get_start_iter(con->output_buf, &start);
		gtk_text_buffer_get_iter_at_offset(con->output_buf, &mid, len / 2);
		gtk_text_buffer_delete(con->output_buf, &start, &mid);
	}

	GtkTextIter end;
	gtk_text_buffer_get_end_iter(con->output_buf, &end);
	gtk_text_buffer_insert(con->output_buf, &end, str, -1);

	GtkTextMark *mark = gtk_text_buffer_get_mark(con->output_buf, "insert");
	gtk_text_view_scroll_mark_onscreen(GTK_TEXT_VIEW(con->output_view), mark);
}

// ────────────────────────────────────────
// Lua engine callbacks
// ────────────────────────────────────────

static void lua_print_cb(int uid, const char *str)
{
	auto it = g_consoles.find(uid);
	if (it != g_consoles.end())
		console_append(it->second, str);
}

static void lua_onstart_cb(int uid)
{
	auto it = g_consoles.find(uid);
	if (it == g_consoles.end()) return;
	LuaConsole *con = it->second;

	con->started = true;
	gtk_text_buffer_set_text(con->output_buf, "", -1);
	gtk_button_set_label(GTK_BUTTON(con->run_btn), "Restart");
	gtk_widget_set_sensitive(con->run_btn,    TRUE);
	gtk_widget_set_sensitive(con->stop_btn,   TRUE);
	// Disable browse while running (matches Windows: can misbehave in a frame-advance loop)
	gtk_widget_set_sensitive(con->browse_btn, FALSE);
}

static void lua_onstop_cb(int uid, bool /*statusOK*/)
{
	auto it = g_consoles.find(uid);
	if (it == g_consoles.end()) return;
	LuaConsole *con = it->second;

	console_append(con, "script stopped.\n");

	con->started = false;
	gtk_button_set_label(GTK_BUTTON(con->run_btn), "Run");
	gtk_widget_set_sensitive(con->stop_btn,   FALSE);
	gtk_widget_set_sensitive(con->browse_btn, TRUE);

	if (con->closeOnStop)
		gtk_widget_destroy(con->window); // on_window_destroy handles cleanup
}

// ────────────────────────────────────────
// Edit button label / sensitivity
// Mirrors Windows UpdateFileEntered() logic
// ────────────────────────────────────────

static void update_edit_button(LuaConsole *con)
{
	const gchar *path = gtk_entry_get_text(GTK_ENTRY(con->path_entry));
	if (!path || path[0] == '\0') {
		gtk_button_set_label(GTK_BUTTON(con->edit_btn), "Edit");
		gtk_widget_set_sensitive(con->edit_btn, FALSE);
		return;
	}

	bool exists   = (access(path, F_OK) == 0);
	bool writable = (access(path, W_OK) == 0);
	const char *ext = strrchr(path, '.');
	bool isLua = ext && (g_ascii_strcasecmp(ext, ".lua") == 0);

	if (exists) {
		const char *label = isLua ? (writable ? "Edit" : "View") : "Open";
		gtk_button_set_label(GTK_BUTTON(con->edit_btn), label);
		gtk_widget_set_sensitive(con->edit_btn, TRUE);
	} else {
		gtk_button_set_label(GTK_BUTTON(con->edit_btn), "Create");
		gtk_widget_set_sensitive(con->edit_btn, isLua);
	}
}

// ────────────────────────────────────────
// Signal handlers
// ────────────────────────────────────────

static void on_path_changed(GtkEditable * /*editable*/, gpointer user_data)
{
	LuaConsole *con = static_cast<LuaConsole*>(user_data);
	const gchar *path = gtk_entry_get_text(GTK_ENTRY(con->path_entry));

	if (path && path[0] != '\0' && access(path, F_OK) == 0) {
		con->filename = path;
		// Set window title to the script's basename (matches Windows behaviour)
		const char *slash = strrchr(path, '/');
		gtk_window_set_title(GTK_WINDOW(con->window),
		                     slash ? slash + 1 : path);
		start_watcher(con);
	}

	update_edit_button(con);
}

static void on_browse_clicked(GtkButton * /*btn*/, gpointer user_data)
{
	LuaConsole *con = static_cast<LuaConsole*>(user_data);

	GtkFileFilter *filter_lua = gtk_file_filter_new();
	gtk_file_filter_set_name(filter_lua, "Lua Script (*.lua)");
	gtk_file_filter_add_pattern(filter_lua, "*.lua");

	GtkFileFilter *filter_all = gtk_file_filter_new();
	gtk_file_filter_set_name(filter_all, "All Files (*.*)");
	gtk_file_filter_add_pattern(filter_all, "*");

	GtkFileChooserNative *fc = gtk_file_chooser_native_new(
		"Load Lua Script",
		GTK_WINDOW(con->window),
		GTK_FILE_CHOOSER_ACTION_OPEN,
		"_Open", "_Cancel");
	gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(fc), filter_lua);
	gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(fc), filter_all);

	// Open in the directory of the current script, if one is set
	if (!con->filename.empty()) {
		std::string dir = con->filename;
		size_t sl = dir.rfind('/');
		if (sl != std::string::npos) dir.resize(sl);
		gtk_file_chooser_set_current_folder(GTK_FILE_CHOOSER(fc), dir.c_str());
	}

	if (gtk_native_dialog_run(GTK_NATIVE_DIALOG(fc)) == GTK_RESPONSE_ACCEPT) {
		GFile *file = gtk_file_chooser_get_file(GTK_FILE_CHOOSER(fc));
		gchar *path = g_file_get_path(file);
		gtk_entry_set_text(GTK_ENTRY(con->path_entry), path);
		g_free(path);
		g_object_unref(file);
	}

	g_object_unref(fc);
}

static void on_edit_clicked(GtkButton * /*btn*/, gpointer user_data)
{
	LuaConsole *con = static_cast<LuaConsole*>(user_data);
	const gchar *path = gtk_entry_get_text(GTK_ENTRY(con->path_entry));
	if (!path || path[0] == '\0') return;

	bool exists  = (access(path, F_OK) == 0);
	bool created = false;

	if (!exists) {
		// "Create" mode — create an empty file, matching Windows behaviour
		FILE *f = fopen(path, "w");
		if (f) { fclose(f); exists = true; created = true; }
	}

	if (exists) {
		// Open in the default editor via xdg-open, mirroring Windows ShellExecute
		const gchar *argv[] = {"xdg-open", path, nullptr};
		g_spawn_async(nullptr, const_cast<gchar**>(argv), nullptr,
		              G_SPAWN_SEARCH_PATH, nullptr, nullptr, nullptr, nullptr);
	}

	if (created)
		on_path_changed(GTK_EDITABLE(con->path_entry), con);
}

static void on_run_clicked(GtkButton * /*btn*/, gpointer user_data)
{
	LuaConsole *con = static_cast<LuaConsole*>(user_data);
	if (con->filename.empty()) {
		const gchar *path = gtk_entry_get_text(GTK_ENTRY(con->path_entry));
		if (!path || path[0] == '\0') return;
		con->filename = path;
	}
	start_watcher(con);
	RunLuaScriptFile(con->uid, con->filename.c_str());
}

static void on_stop_clicked(GtkButton * /*btn*/, gpointer user_data)
{
	LuaConsole *con = static_cast<LuaConsole*>(user_data);
	console_append(con, "user clicked stop button\n");
	StopLuaScript(con->uid);
}

static void on_window_destroy(GtkWidget * /*widget*/, gpointer user_data)
{
	LuaConsole *con = static_cast<LuaConsole*>(user_data);
	stop_watcher(con);
	CloseLuaContext(con->uid);
	g_consoles.erase(con->uid);
	delete con;
}

static gboolean on_window_delete(GtkWidget * /*widget*/, GdkEvent * /*event*/,
                                 gpointer user_data)
{
	LuaConsole *con = static_cast<LuaConsole*>(user_data);
	console_append(con, "user closed script window\n");
	// Stop the watcher immediately so it cannot trigger a reload during teardown
	stop_watcher(con);
	StopLuaScript(con->uid);

	if (con->started) {
		// Script is still running; defer destroy until onstop fires
		con->closeOnStop = true;
		return TRUE; // block the window close
	}

	return FALSE; // allow close → "destroy" signal fires next
}

// Accept files dropped onto the window (mirrors Windows DragAcceptFiles)
static void on_drag_data_received(GtkWidget * /*widget*/, GdkDragContext *ctx,
    gint /*x*/, gint /*y*/, GtkSelectionData *data,
    guint /*info*/, guint time, gpointer user_data)
{
	LuaConsole *con = static_cast<LuaConsole*>(user_data);
	gchar **uris = gtk_selection_data_get_uris(data);
	if (uris && uris[0]) {
		gchar *path = g_filename_from_uri(uris[0], nullptr, nullptr);
		if (path) {
			gtk_entry_set_text(GTK_ENTRY(con->path_entry), path);
			g_free(path);
		}
		g_strfreev(uris);
	}
	gtk_drag_finish(ctx, TRUE, FALSE, time);
}

// ────────────────────────────────────────
// Public API
// ────────────────────────────────────────

void lua_script_close_all()
{
	// Collect windows first to avoid iterator invalidation during close
	// Iterate in reverse order, matching Windows IDC_CLOSE_LUA_SCRIPTS behaviour
	std::vector<GtkWidget*> windows;
	for (auto &pair : g_consoles)
		windows.push_back(pair.second->window);
	for (int i = (int)windows.size() - 1; i >= 0; i--)
		gtk_window_close(GTK_WINDOW(windows[i]));
}

void lua_script_open_console(GtkWindow *parent)
{
	LuaConsole *con = new LuaConsole();
	con->uid = g_next_uid++;

	// ── Window ──
	con->window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	gtk_window_set_title(GTK_WINDOW(con->window), "Lua Script Console");
	gtk_window_set_default_size(GTK_WINDOW(con->window), 405, 244);
	if (parent)
		gtk_window_set_transient_for(GTK_WINDOW(con->window), parent);

	GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
	gtk_container_set_border_width(GTK_CONTAINER(vbox), 6);
	gtk_container_add(GTK_CONTAINER(con->window), vbox);

	// ── Row 1: path entry ──
	con->path_entry = gtk_entry_new();
	gtk_box_pack_start(GTK_BOX(vbox), con->path_entry, FALSE, FALSE, 0);

	// ── Row 2: [Browse...][Edit] ··· [Stop][Run]
	//    Matches Windows: Browse/Edit left-aligned, Stop/Run right-aligned
	GtkWidget *btn_row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4);

	con->browse_btn = gtk_button_new_with_label("Browse...");
	con->edit_btn   = gtk_button_new_with_label("Edit");
	con->stop_btn   = gtk_button_new_with_label("Stop");
	con->run_btn    = gtk_button_new_with_label("Run");

	gtk_widget_set_sensitive(con->edit_btn, FALSE);
	gtk_widget_set_sensitive(con->stop_btn, FALSE);

	gtk_box_pack_start(GTK_BOX(btn_row), con->browse_btn, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(btn_row), con->edit_btn,   FALSE, FALSE, 0);
	// Expanding spacer pushes Stop/Run to the right
	gtk_box_pack_start(GTK_BOX(btn_row),
	    gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0), TRUE, TRUE, 0);
	gtk_box_pack_start(GTK_BOX(btn_row), con->stop_btn, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(btn_row), con->run_btn,  FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(vbox), btn_row, FALSE, FALSE, 0);

	// ── Row 3: stdout checkbox (right-aligned, matches Windows IDC_USE_STDOUT) ──
	GtkWidget *stdout_row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
	con->stdout_check = gtk_check_button_new_with_label("stdout");
	gtk_box_pack_end(GTK_BOX(stdout_row), con->stdout_check, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(vbox), stdout_row, FALSE, FALSE, 0);

	// ── Row 4: output console (scrolled, monospace, read-only) ──
	GtkWidget *scroll = gtk_scrolled_window_new(nullptr, nullptr);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroll),
	    GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);

	con->output_view = gtk_text_view_new();
	gtk_text_view_set_editable(GTK_TEXT_VIEW(con->output_view), FALSE);
	gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(con->output_view), GTK_WRAP_WORD_CHAR);
	gtk_text_view_set_monospace(GTK_TEXT_VIEW(con->output_view), TRUE);

	con->output_buf = gtk_text_view_get_buffer(GTK_TEXT_VIEW(con->output_view));
	gtk_container_add(GTK_CONTAINER(scroll), con->output_view);
	gtk_box_pack_start(GTK_BOX(vbox), scroll, TRUE, TRUE, 0);

	// ── Drag-and-drop ──
	gtk_drag_dest_set(con->window, GTK_DEST_DEFAULT_ALL, nullptr, 0, GDK_ACTION_COPY);
	gtk_drag_dest_add_uri_targets(con->window);

	// ── Signals ──
	g_signal_connect(con->path_entry, "changed",
	                 G_CALLBACK(on_path_changed),       con);
	g_signal_connect(con->browse_btn, "clicked",
	                 G_CALLBACK(on_browse_clicked),     con);
	g_signal_connect(con->edit_btn,   "clicked",
	                 G_CALLBACK(on_edit_clicked),       con);
	g_signal_connect(con->run_btn,    "clicked",
	                 G_CALLBACK(on_run_clicked),        con);
	g_signal_connect(con->stop_btn,   "clicked",
	                 G_CALLBACK(on_stop_clicked),       con);
	g_signal_connect(con->window,     "delete-event",
	                 G_CALLBACK(on_window_delete),      con);
	g_signal_connect(con->window,     "destroy",
	                 G_CALLBACK(on_window_destroy),     con);
	g_signal_connect(con->window,     "drag-data-received",
	                 G_CALLBACK(on_drag_data_received), con);

	// ── Register Lua context before showing ──
	OpenLuaContext(con->uid, lua_print_cb, lua_onstart_cb, lua_onstop_cb);
	g_consoles[con->uid] = con;

	gtk_widget_show_all(con->window);
}

#endif /* HAVE_LUA */
