/*
	luaScriptConsole.cpp - Lua script console for the DeSmuME GTK4 frontend

	Copyright (C) 2026 DeSmuME team

	GTK4 host for the Lua engine.  The engine's UI hooks (popup, emu.openscript,
	input status, dynamic menus) are optional in lua-engine.h and fall back to
	platform-neutral defaults when unset, so the console only needs its own
	print/onstart/onstop callbacks through OpenLuaContext().

	Differences from the GTK3 host:
	  - GtkFileChooserNative + gtk_native_dialog_run -> async GtkFileDialog.
	  - Legacy GtkDragContext DnD -> GtkDropTarget (GDK_TYPE_FILE_LIST).
	  - GtkAboutDialog via gtk_dialog_run -> GtkAlertDialog.
	  - gtk_widget_destroy / "delete-event" -> close-request + deferred destroy
	    until the engine reports onstop.
	  - Console idle work uses G_PRIORITY_DEFAULT, matching the GTK4 frame tick's
	    priority so callbacks are not starved by the emulator loop.
*/

#ifdef HAVE_LUA

#include "luaScriptConsole.h"

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/inotify.h>
#include <sys/select.h>

#include <map>
#include <string>
#include <vector>

#include "../../../lua-engine.h"

#define MAX_CONSOLE_CHARS 250000
#define LUA_CONSOLE_IDLE_PRIORITY G_PRIORITY_DEFAULT

struct LuaConsole
{
	int uid;
	std::string filename;
	bool started;
	bool closeOnStop;

	GtkWidget *window;
	GtkWidget *pathEntry;
	GtkWidget *browseBtn;
	GtkWidget *editBtn;
	GtkWidget *runBtn;
	GtkWidget *stopBtn;
	GtkWidget *stdoutCheck;
	GtkWidget *outputView;
	GtkTextBuffer *outputBuf;

	gint watcherRunning;
	GThread *watcherThread;
	std::string watchedPath;

	LuaConsole(void)
		: uid(0), started(false), closeOnStop(false),
		  watcherRunning(FALSE), watcherThread(NULL) {}
};

static std::map<int, LuaConsole *> sConsoles;
static int sNextUid = 1;

/* ------------------------------------------------------------ file watcher */

struct WatcherArgs
{
	LuaConsole *con;
	std::string basename;
};

static gboolean
watcher_reload_idle(gpointer data)
{
	const int uid = GPOINTER_TO_INT(data);
	std::map<int, LuaConsole *>::iterator it = sConsoles.find(uid);
	if (it == sConsoles.end())
	{
		return G_SOURCE_REMOVE;
	}
	LuaConsole *con = it->second;
	if (g_atomic_int_get(&con->watcherRunning) && !con->filename.empty())
	{
		RequestAbortLuaScript(uid, "terminated to reload the script");
		RunLuaScriptFile(uid, con->filename.c_str());
	}
	return G_SOURCE_REMOVE;
}

static gpointer
file_watcher_thread(gpointer arg)
{
	WatcherArgs *wa = static_cast<WatcherArgs *>(arg);
	LuaConsole *con = wa->con;
	const int uid = con->uid;

	const int ifd = inotify_init();
	if (ifd < 0)
	{
		delete wa;
		return NULL;
	}
	const int wd = inotify_add_watch(ifd, con->watchedPath.c_str(),
	                                 IN_CLOSE_WRITE | IN_MOVED_TO);
	char buf[4096] __attribute__((aligned(__alignof__(struct inotify_event))));

	while (g_atomic_int_get(&con->watcherRunning))
	{
		fd_set fds;
		FD_ZERO(&fds);
		FD_SET(ifd, &fds);
		struct timeval tv = { 0, 500000 };
		const int ret = select(ifd + 1, &fds, NULL, NULL, &tv);
		if (!g_atomic_int_get(&con->watcherRunning))
		{
			break;
		}
		if (ret <= 0)
		{
			continue;
		}
		const int len = read(ifd, buf, sizeof(buf));
		if (len < 0)
		{
			continue;
		}
		const struct inotify_event *ev = NULL;
		for (char *p = buf; p < buf + len;
		     p += sizeof(struct inotify_event) + ev->len)
		{
			ev = reinterpret_cast<const struct inotify_event *>(p);
			if ((ev->len > 0) && (wa->basename == ev->name))
			{
				g_idle_add_full(LUA_CONSOLE_IDLE_PRIORITY, watcher_reload_idle,
				                GINT_TO_POINTER(uid), NULL);
				break;
			}
		}
	}
	if (wd >= 0)
	{
		inotify_rm_watch(ifd, wd);
	}
	close(ifd);
	delete wa;
	return NULL;
}

static void stop_watcher(LuaConsole *con);

static void
start_watcher(LuaConsole *con)
{
	if (con->filename.empty())
	{
		stop_watcher(con);
		return;
	}
	if ((con->watcherThread != NULL) && (con->watchedPath == con->filename))
	{
		return;
	}
	stop_watcher(con);

	std::string directory = con->filename;
	size_t slash = directory.rfind('/');
	if (slash != std::string::npos)
	{
		directory.resize(slash);
	}
	else
	{
		directory = ".";
	}
	const std::string basename = con->filename.substr(
		slash != std::string::npos ? slash + 1 : 0);

	WatcherArgs *wa = new WatcherArgs();
	wa->con = con;
	wa->basename = basename;
	con->watchedPath = con->filename;
	g_atomic_int_set(&con->watcherRunning, TRUE);
	con->watcherThread = g_thread_new("lua-watcher", file_watcher_thread, wa);
}

static void
stop_watcher(LuaConsole *con)
{
	con->watchedPath.clear();
	if (con->watcherThread == NULL)
	{
		return;
	}
	g_atomic_int_set(&con->watcherRunning, FALSE);
	g_thread_join(con->watcherThread);
	con->watcherThread = NULL;
}

/* --------------------------------------------------------- console output */

static void
console_append(LuaConsole *con, const char *str)
{
	if (gtk_check_button_get_active(GTK_CHECK_BUTTON(con->stdoutCheck)))
	{
		fputs(str, stdout);
		fflush(stdout);
		return;
	}
	GtkTextIter end;
	gtk_text_buffer_get_end_iter(con->outputBuf, &end);
	gtk_text_buffer_insert(con->outputBuf, &end, str, -1);

	/* Cap the buffer like the Windows console so long-running scripts cannot
	 * grow it without bound. */
	GtkTextIter start;
	gtk_text_buffer_get_start_iter(con->outputBuf, &start);
	const int length = gtk_text_buffer_get_char_count(con->outputBuf);
	if (length > MAX_CONSOLE_CHARS)
	{
		GtkTextIter cut;
		gtk_text_buffer_get_iter_at_offset(con->outputBuf, &cut,
		                                   length - MAX_CONSOLE_CHARS);
		gtk_text_buffer_delete(con->outputBuf, &start, &cut);
	}
	GtkTextMark *mark = gtk_text_buffer_get_mark(con->outputBuf, "insert");
	gtk_text_view_scroll_mark_onscreen(GTK_TEXT_VIEW(con->outputView), mark);
}

/* ------------------------------------------------------ Lua engine events */

struct ConsolePrintEvent { int uid; std::string text; };
struct ConsoleStateEvent { int uid; bool started; bool statusOK; };

static gboolean
console_print_idle(gpointer data)
{
	ConsolePrintEvent *ev = static_cast<ConsolePrintEvent *>(data);
	std::map<int, LuaConsole *>::iterator it = sConsoles.find(ev->uid);
	if (it != sConsoles.end())
	{
		console_append(it->second, ev->text.c_str());
	}
	delete ev;
	return G_SOURCE_REMOVE;
}

static gboolean
console_start_idle(gpointer data)
{
	ConsoleStateEvent *ev = static_cast<ConsoleStateEvent *>(data);
	std::map<int, LuaConsole *>::iterator it = sConsoles.find(ev->uid);
	if (it != sConsoles.end())
	{
		LuaConsole *con = it->second;
		con->started = true;
		gtk_text_buffer_set_text(con->outputBuf, "", -1);
		gtk_button_set_label(GTK_BUTTON(con->runBtn), "Restart");
		gtk_widget_set_sensitive(con->runBtn, TRUE);
		gtk_widget_set_sensitive(con->stopBtn, TRUE);
		gtk_widget_set_sensitive(con->browseBtn, FALSE);
	}
	delete ev;
	return G_SOURCE_REMOVE;
}

static gboolean
console_stop_idle(gpointer data)
{
	ConsoleStateEvent *ev = static_cast<ConsoleStateEvent *>(data);
	std::map<int, LuaConsole *>::iterator it = sConsoles.find(ev->uid);
	if (it != sConsoles.end())
	{
		LuaConsole *con = it->second;
		console_append(con, "script stopped.\n");
		con->started = false;
		gtk_button_set_label(GTK_BUTTON(con->runBtn), "Run");
		gtk_widget_set_sensitive(con->stopBtn, FALSE);
		gtk_widget_set_sensitive(con->browseBtn, TRUE);
		if (con->closeOnStop)
		{
			con->closeOnStop = false;
			gtk_window_destroy(GTK_WINDOW(con->window));
		}
	}
	delete ev;
	return G_SOURCE_REMOVE;
}

static void lua_print_cb(intptr_t uid, const char *str)
{
	ConsolePrintEvent *ev = new ConsolePrintEvent();
	ev->uid = (int)uid;
	ev->text = (str != NULL) ? str : "";
	g_idle_add_full(LUA_CONSOLE_IDLE_PRIORITY, console_print_idle, ev, NULL);
}

static void lua_onstart_cb(intptr_t uid)
{
	ConsoleStateEvent *ev = new ConsoleStateEvent();
	ev->uid = (int)uid;
	ev->started = true;
	ev->statusOK = true;
	g_idle_add_full(LUA_CONSOLE_IDLE_PRIORITY, console_start_idle, ev, NULL);
}

static void lua_onstop_cb(intptr_t uid, bool statusOK)
{
	ConsoleStateEvent *ev = new ConsoleStateEvent();
	ev->uid = (int)uid;
	ev->started = false;
	ev->statusOK = statusOK;
	g_idle_add_full(LUA_CONSOLE_IDLE_PRIORITY, console_stop_idle, ev, NULL);
}

/* ------------------------------------------------------------ UI helpers */

static std::string
entry_text(GtkWidget *entry)
{
	const char *text = gtk_editable_get_text(GTK_EDITABLE(entry));
	return (text != NULL) ? std::string(text) : std::string();
}

static void
update_edit_button(LuaConsole *con)
{
	const std::string path = entry_text(con->pathEntry);
	if (path.empty())
	{
		gtk_button_set_label(GTK_BUTTON(con->editBtn), "Edit");
		gtk_widget_set_sensitive(con->editBtn, FALSE);
		return;
	}
	const bool exists = (access(path.c_str(), F_OK) == 0);
	const bool writable = (access(path.c_str(), W_OK) == 0);
	const size_t dot = path.rfind('.');
	const bool isLua = (dot != std::string::npos) &&
	                   (g_ascii_strcasecmp(path.c_str() + dot, ".lua") == 0);

	if (exists)
	{
		const char *label = isLua ? (writable ? "Edit" : "View") : "Open";
		gtk_button_set_label(GTK_BUTTON(con->editBtn), label);
		gtk_widget_set_sensitive(con->editBtn, TRUE);
	}
	else if (isLua)
	{
		gtk_button_set_label(GTK_BUTTON(con->editBtn), "Create");
		gtk_widget_set_sensitive(con->editBtn, TRUE);
	}
	else
	{
		gtk_button_set_label(GTK_BUTTON(con->editBtn), "Edit");
		gtk_widget_set_sensitive(con->editBtn, FALSE);
	}
}

static void
on_path_changed(GtkEditable *editable, gpointer data)
{
	(void)editable;
	LuaConsole *con = static_cast<LuaConsole *>(data);
	const std::string path = entry_text(con->pathEntry);
	std::string filename;
	if (!path.empty() && (access(path.c_str(), F_OK) == 0))
	{
		filename = path;
	}
	if (filename != con->filename)
	{
		con->filename = filename;
		if (filename.empty())
		{
			stop_watcher(con);
			gtk_window_set_title(GTK_WINDOW(con->window), "Lua Script Console");
		}
		else
		{
			const size_t slash = filename.rfind('/');
			gtk_window_set_title(GTK_WINDOW(con->window),
			                     (slash != std::string::npos)
			                         ? filename.c_str() + slash + 1 : filename.c_str());
			start_watcher(con);
		}
	}
	update_edit_button(con);
}

/* ------------------------------------------------------------- file open */

struct BrowseCtx
{
	LuaConsole *con;
};

static void
free_browse_ctx(gpointer data)
{
	delete (BrowseCtx *)data;
}

static void
on_open_done(GObject *source, GAsyncResult *result, gpointer user_data)
{
	BrowseCtx *ctx = (BrowseCtx *)user_data;
	GError *error = NULL;
	GFile *file = gtk_file_dialog_open_finish(GTK_FILE_DIALOG(source), result, &error);
	if (file != NULL)
	{
		char *path = g_file_get_path(file);
		if (path != NULL)
		{
			gtk_editable_set_text(GTK_EDITABLE(ctx->con->pathEntry), path);
			g_free(path);
		}
		g_object_unref(file);
	}
	g_clear_error(&error);
	free_browse_ctx(ctx);
}

static void
on_browse_clicked(GtkButton *button, gpointer data)
{
	(void)button;
	LuaConsole *con = static_cast<LuaConsole *>(data);
	GtkFileDialog *dialog = gtk_file_dialog_new();
	gtk_file_dialog_set_title(dialog, "Load Lua Script");
	gtk_file_dialog_set_modal(dialog, TRUE);

	if (!con->filename.empty())
	{
		std::string dir = con->filename;
		const size_t slash = dir.rfind('/');
		if (slash != std::string::npos)
		{
			dir.resize(slash);
		}
		GFile *folder = g_file_new_for_path(dir.c_str());
		gtk_file_dialog_set_initial_folder(dialog, folder);
		g_object_unref(folder);
	}

	GtkFileFilter *filterLua = gtk_file_filter_new();
	gtk_file_filter_set_name(filterLua, "Lua Script (*.lua)");
	gtk_file_filter_add_pattern(filterLua, "*.lua");
	GtkFileFilter *filterAll = gtk_file_filter_new();
	gtk_file_filter_set_name(filterAll, "All Files (*)");
	gtk_file_filter_add_pattern(filterAll, "*");
	GListStore *filters = g_list_store_new(GTK_TYPE_FILE_FILTER);
	g_list_store_append(filters, filterLua);
	g_list_store_append(filters, filterAll);
	gtk_file_dialog_set_filters(dialog, G_LIST_MODEL(filters));
	g_object_unref(filterLua);
	g_object_unref(filterAll);
	g_object_unref(filters);

	BrowseCtx *ctx = new BrowseCtx();
	ctx->con = con;
	gtk_file_dialog_open(dialog, GTK_WINDOW(con->window), NULL, on_open_done, ctx);
	g_object_unref(dialog);
}

/* ------------------------------------------------------------- run/edit */

static void
on_run_clicked(GtkButton *button, gpointer data)
{
	(void)button;
	LuaConsole *con = static_cast<LuaConsole *>(data);
	const std::string path = entry_text(con->pathEntry);
	if (path.empty())
	{
		console_append(con, "no script file selected.\n");
		return;
	}
	if (access(path.c_str(), F_OK) != 0)
	{
		console_append(con, "script file does not exist: ");
		console_append(con, path.c_str());
		console_append(con, "\n");
		return;
	}
	if (con->filename != path)
	{
		con->filename = path;
		const size_t slash = path.rfind('/');
		gtk_window_set_title(GTK_WINDOW(con->window),
		                     (slash != std::string::npos)
		                         ? path.c_str() + slash + 1 : path.c_str());
		update_edit_button(con);
	}
	start_watcher(con);
	RunLuaScriptFile(con->uid, con->filename.c_str());
}

static void
on_stop_clicked(GtkButton *button, gpointer data)
{
	(void)button;
	LuaConsole *con = static_cast<LuaConsole *>(data);
	console_append(con, "user clicked stop button\n");
	StopLuaScript(con->uid);
}

static void
on_edit_clicked(GtkButton *button, gpointer data)
{
	(void)button;
	LuaConsole *con = static_cast<LuaConsole *>(data);
	const std::string path = entry_text(con->pathEntry);
	if (path.empty())
	{
		return;
	}
	const bool exists = (access(path.c_str(), F_OK) == 0);
	if (!exists)
	{
		FILE *f = fopen(path.c_str(), "w");
		if (f != NULL)
		{
			fclose(f);
			console_append(con, "created ");
			console_append(con, path.c_str());
			console_append(con, "\n");
		}
		else
		{
			console_append(con, "could not create ");
			console_append(con, path.c_str());
			console_append(con, "\n");
			return;
		}
	}
	/* GTK4 has no gtk_show_uri_on_window in GTK5; g_app_info_launch_default
	 * is the cross-version way to open the script in the user's editor. */
	GError *error = NULL;
	if (!g_app_info_launch_default_for_uri(path.c_str(), NULL, &error))
	{
		console_append(con, "could not open editor: ");
		console_append(con, (error != NULL) ? error->message : "unknown error");
		console_append(con, "\n");
	}
	g_clear_error(&error);
	update_edit_button(con);
}

/* ------------------------------------------------------------ lifecycle */

static gboolean
on_close_request(GtkWindow *window, gpointer data)
{
	(void)window;
	LuaConsole *con = static_cast<LuaConsole *>(data);
	console_append(con, "user closed script window\n");
	stop_watcher(con);
	StopLuaScript(con->uid);
	if (con->started)
	{
		con->closeOnStop = true;
		return TRUE; /* wait for the engine's onstop before destroying */
	}
	con->closeOnStop = false;
	return FALSE;
}

static void
free_console_data(gpointer data)
{
	LuaConsole *con = static_cast<LuaConsole *>(data);
	stop_watcher(con);
	CloseLuaContext(con->uid);
	sConsoles.erase(con->uid);
	delete con;
}

static gboolean
on_drop(GtkDropTarget *target, const GValue *value, double x, double y, gpointer data)
{
	(void)target;
	(void)x;
	(void)y;
	LuaConsole *con = static_cast<LuaConsole *>(data);
	if (!G_VALUE_HOLDS(value, GDK_TYPE_FILE_LIST))
	{
		return FALSE;
	}
	GListModel *files = (GListModel *)g_value_get_boxed(value);
	if ((files == NULL) || (g_list_model_get_n_items(files) == 0))
	{
		return FALSE;
	}
	GFile *file = G_FILE(g_list_model_get_item(files, 0));
	char *path = g_file_get_path(file);
	if (path != NULL)
	{
		gtk_editable_set_text(GTK_EDITABLE(con->pathEntry), path);
		g_free(path);
	}
	g_object_unref(file);
	return TRUE;
}

static LuaConsole *
create_console(GtkWindow *parent)
{
	LuaConsole *con = new LuaConsole();
	con->uid = sNextUid++;

	con->window = gtk_window_new();
	gtk_window_set_title(GTK_WINDOW(con->window), "Lua Script Console");
	gtk_window_set_default_size(GTK_WINDOW(con->window), 405, 244);
	if (parent != NULL)
	{
		gtk_window_set_transient_for(GTK_WINDOW(con->window), parent);
	}

	GtkWidget *root = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
	gtk_widget_set_margin_start(root, 6);
	gtk_widget_set_margin_end(root, 6);
	gtk_widget_set_margin_top(root, 6);
	gtk_widget_set_margin_bottom(root, 6);
	gtk_window_set_child(GTK_WINDOW(con->window), root);

	con->pathEntry = gtk_entry_new();
	gtk_box_append(GTK_BOX(root), con->pathEntry);

	GtkWidget *buttonRow = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4);
	con->browseBtn = gtk_button_new_with_label("Browse...");
	con->editBtn = gtk_button_new_with_label("Edit");
	con->stopBtn = gtk_button_new_with_label("Stop");
	con->runBtn = gtk_button_new_with_label("Run");
	gtk_widget_set_sensitive(con->editBtn, FALSE);
	gtk_widget_set_sensitive(con->stopBtn, FALSE);
	gtk_box_append(GTK_BOX(buttonRow), con->browseBtn);
	gtk_box_append(GTK_BOX(buttonRow), con->editBtn);
	GtkWidget *spacer = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
	gtk_widget_set_hexpand(spacer, TRUE);
	gtk_box_append(GTK_BOX(buttonRow), spacer);
	gtk_box_append(GTK_BOX(buttonRow), con->stopBtn);
	gtk_box_append(GTK_BOX(buttonRow), con->runBtn);
	gtk_box_append(GTK_BOX(root), buttonRow);

	GtkWidget *stdoutRow = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
	con->stdoutCheck = gtk_check_button_new_with_label("stdout");
	gtk_widget_set_halign(con->stdoutCheck, GTK_ALIGN_END);
	gtk_widget_set_hexpand(con->stdoutCheck, TRUE);
	gtk_box_append(GTK_BOX(stdoutRow), con->stdoutCheck);
	gtk_box_append(GTK_BOX(root), stdoutRow);

	GtkWidget *scroll = gtk_scrolled_window_new();
	con->outputView = gtk_text_view_new();
	gtk_text_view_set_editable(GTK_TEXT_VIEW(con->outputView), FALSE);
	gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(con->outputView), GTK_WRAP_WORD_CHAR);
	gtk_widget_add_css_class(con->outputView, "desmume-tool-cell");
	con->outputBuf = gtk_text_view_get_buffer(GTK_TEXT_VIEW(con->outputView));
	gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scroll), con->outputView);
	gtk_widget_set_vexpand(scroll, TRUE);
	gtk_box_append(GTK_BOX(root), scroll);

	GtkDropTarget *drop = gtk_drop_target_new(GDK_TYPE_FILE_LIST, GDK_ACTION_COPY);
	g_signal_connect(drop, "drop", G_CALLBACK(on_drop), con);
	gtk_widget_add_controller(con->window, GTK_EVENT_CONTROLLER(drop));

	g_signal_connect(con->pathEntry, "changed", G_CALLBACK(on_path_changed), con);
	g_signal_connect(con->browseBtn, "clicked", G_CALLBACK(on_browse_clicked), con);
	g_signal_connect(con->editBtn, "clicked", G_CALLBACK(on_edit_clicked), con);
	g_signal_connect(con->runBtn, "clicked", G_CALLBACK(on_run_clicked), con);
	g_signal_connect(con->stopBtn, "clicked", G_CALLBACK(on_stop_clicked), con);
	g_signal_connect(con->window, "close-request", G_CALLBACK(on_close_request), con);
	g_object_set_data_full(G_OBJECT(con->window), "lua-console", con, free_console_data);

	OpenLuaContext(con->uid, lua_print_cb, lua_onstart_cb, lua_onstop_cb);
	sConsoles[con->uid] = con;
	gtk_window_present(GTK_WINDOW(con->window));
	return con;
}

/* -------------------------------------------------------------- public API */

void
lua_script_open_console(GtkWindow *parent)
{
	create_console(parent);
}

static gboolean
pending_run_idle(gpointer data)
{
	const int uid = GPOINTER_TO_INT(data);
	std::map<int, LuaConsole *>::iterator it = sConsoles.find(uid);
	if (it != sConsoles.end())
	{
		on_run_clicked(NULL, it->second);
	}
	return G_SOURCE_REMOVE;
}

void
lua_script_open_console_with_file(GtkWindow *parent, const char *filename)
{
	LuaConsole *con = create_console(parent);
	if (con == NULL)
	{
		return;
	}
	gtk_editable_set_text(GTK_EDITABLE(con->pathEntry),
	                      (filename != NULL) ? filename : "");
	g_idle_add_full(LUA_CONSOLE_IDLE_PRIORITY, pending_run_idle,
	                GINT_TO_POINTER(con->uid), NULL);
}

void
lua_script_close_all(void)
{
	std::vector<GtkWidget *> windows;
	for (std::map<int, LuaConsole *>::iterator it = sConsoles.begin();
	     it != sConsoles.end(); ++it)
	{
		windows.push_back(it->second->window);
	}
	for (int i = (int)windows.size() - 1; i >= 0; --i)
	{
		gtk_window_close(GTK_WINDOW(windows[i]));
	}
}

void
lua_script_show_about(GtkWindow *parent)
{
	LuaEngineInfo info;
	const bool engineReady = GetLuaEngineInfo(&info);
	char libraryPath[1024] = "";
	GetLuaEngineLibraryPath(libraryPath, sizeof(libraryPath));

	char text[2048];
	if (!engineReady)
	{
		g_snprintf(text, sizeof(text),
		           "The Lua engine could not be started.\n\n"
		           "Reason     : %s\n"
		           "Library    : %s\n",
		           info.jit[0] ? info.jit : "unknown",
		           libraryPath[0] ? libraryPath : "not loaded");
	}
	else
	{
		g_snprintf(text, sizeof(text),
		           "DeSmuME Lua Scripting\n\n"
		           "Engine     : %s\n"
		           "Platform   : %s\n"
		           "Trace JIT  : %s\n"
		           "Headers    : %s\n"
		           "Library    : %s\n",
		           info.version,
		           info.platform[0] ? info.platform : "n/a (not LuaJIT)",
		           info.jit,
		           info.headers,
		           libraryPath[0] ? libraryPath : "(not loaded as a shared library)");
	}

	GtkAlertDialog *dialog = gtk_alert_dialog_new(
		engineReady ? "About Lua Scripting" : "Lua Scripting");
	gtk_alert_dialog_set_message(dialog, text);
	gtk_alert_dialog_choose(dialog, parent, NULL, NULL, NULL);
	g_object_unref(dialog);
}

#endif /* HAVE_LUA */
