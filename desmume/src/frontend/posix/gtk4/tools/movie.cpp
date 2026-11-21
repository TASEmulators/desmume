/*
	movie.cpp - Play/Record Movie dialogs for the DeSmuME GTK4 frontend

	Copyright (C) 2026 DeSmuME team

	UI-only host for the shared movie_info_core.cpp model, which the Windows
	and Cocoa frontends also use.  GTK3's GtkFileChooserNative + gtk_dialog_run
	and gtk_message_dialog are replaced with GtkFileDialog and GtkAlertDialog;
	the dialog window itself is a plain GtkWindow with a Cancel/Play/Record
	button row so no action area (removed in GTK4) is needed.
*/

#include "movie.h"
#include "../emulation.h"
#include "../tools.h" /* desmume_host_config() */
#include "../config.h" /* complete desmume::config::Config type */

#include "../../../shared/movie_info_core.h"

#define TOOL_NAME "DeSmuME Movie"

/* ---------------------------------------------------------------- helpers */

static std::string
movies_directory(void)
{
	desmume::config::Config *config = desmume_host_config();
	if (config != NULL)
	{
		const std::string configured = config->movie_directory.get();
		if (!configured.empty())
		{
			return configured;
		}
	}
	const gchar *home = g_get_home_dir();
	return std::string(home != NULL ? home : ".") + "/Movies";
}

static std::string
entry_text(GtkWidget *entry)
{
	const char *text = gtk_editable_get_text(GTK_EDITABLE(entry));
	return (text != NULL) ? std::string(text) : std::string();
}

static void
set_entry_text(GtkWidget *entry, const char *text)
{
	gtk_editable_set_text(GTK_EDITABLE(entry), text != NULL ? text : "");
}

static std::string
resolve_path(const std::string &text)
{
	if (text.empty())
	{
		return text;
	}
	std::string full = text;
	if (full.find_first_of("\\/") == std::string::npos)
	{
		std::string dir = movies_directory();
		while (!dir.empty() && (dir[dir.size() - 1] == '/' || dir[dir.size() - 1] == '\\'))
		{
			dir.erase(dir.size() - 1);
		}
		full = dir + "/" + full;
	}
	return MovieInfo::EnsureDsmExtension(full);
}

static std::string
markup_escape(const std::string &s)
{
	std::string out;
	out.reserve(s.size());
	for (size_t i = 0; i < s.size(); ++i)
	{
		switch (s[i])
		{
		case '&': out += "&amp;"; break;
		case '<': out += "&lt;"; break;
		case '>': out += "&gt;"; break;
		default:  out += s[i]; break;
		}
	}
	return out;
}

static void
set_field(GtkWidget *label, const std::string &text, bool warn)
{
	if (warn)
	{
		const std::string escaped = markup_escape(text);
		gchar *markup = g_strdup_printf("<span color=\"#cc0000\">%s</span>", escaped.c_str());
		gtk_label_set_markup(GTK_LABEL(label), markup);
		g_free(markup);
	}
	else
	{
		gtk_label_set_text(GTK_LABEL(label), text.c_str());
	}
}

static void
ShowError(GtkWindow *parent, const char *message)
{
	GtkAlertDialog *dialog = gtk_alert_dialog_new(TOOL_NAME);
	gtk_alert_dialog_set_message(dialog, (message != NULL) ? message : "Operation failed.");
	gtk_alert_dialog_choose(dialog, parent, NULL, NULL, NULL);
	g_object_unref(dialog);
}

static GtkWidget *
add_label(GtkGrid *grid, const char *text, int x, int y, int w)
{
	GtkWidget *label = gtk_label_new(text);
	gtk_label_set_xalign(GTK_LABEL(label), 0.0f);
	gtk_grid_attach(grid, label, x, y, w, 1);
	return label;
}

static GtkWidget *
add_field(GtkGrid *grid, int x, int y, int w)
{
	GtkWidget *label = gtk_label_new("-");
	gtk_label_set_xalign(GTK_LABEL(label), 0.0f);
	gtk_label_set_selectable(GTK_LABEL(label), TRUE);
	gtk_widget_add_css_class(label, "dim-label");
	gtk_grid_attach(grid, label, x, y, w, 1);
	return label;
}

static GtkWidget *
add_check(GtkGrid *grid, const char *text, int x, int y, int w)
{
	GtkWidget *check = gtk_check_button_new_with_label(text);
	gtk_grid_attach(grid, check, x, y, w, 1);
	return check;
}

/* GtkFileDialog callback context: fills one entry and refreshes its owner. */
struct BrowseCtx
{
	GtkWidget *entry;
	GtkWindow *parent;
	GCallback on_done; /* GCallback so a dialog can re-preview after picking */
	gpointer owner;
};

static void
free_browse_ctx(gpointer data)
{
	delete (BrowseCtx *)data;
}

static GListStore *
movie_file_filter(void)
{
	GtkFileFilter *filter = gtk_file_filter_new();
	gtk_file_filter_set_name(filter, "DeSmuME movie file (.dsm*)");
	gtk_file_filter_add_pattern(filter, "*.dsm");
	gtk_file_filter_add_pattern(filter, "*.dsm.*");
	GListStore *filters = g_list_store_new(GTK_TYPE_FILE_FILTER);
	g_list_store_append(filters, filter);
	g_object_unref(filter);
	return filters;
}

static void
on_file_chosen(GObject *source, GAsyncResult *result, gpointer user_data)
{
	BrowseCtx *ctx = (BrowseCtx *)user_data;
	GError *error = NULL;
	GFile *file = gtk_file_dialog_open_finish(GTK_FILE_DIALOG(source), result, &error);
	if (file != NULL)
	{
		char *path = g_file_get_path(file);
		if (path != NULL)
		{
			set_entry_text(ctx->entry, path);
			if (ctx->on_done != NULL)
			{
				((void (*)(GtkWidget *, gpointer))ctx->on_done)(ctx->entry, ctx->owner);
			}
			g_free(path);
		}
		g_object_unref(file);
	}
	else if ((error != NULL) &&
	         !g_error_matches(error, GTK_DIALOG_ERROR, GTK_DIALOG_ERROR_DISMISSED))
	{
		ShowError(ctx->parent, error->message);
	}
	g_clear_error(&error);
	free_browse_ctx(ctx);
}

static void
present_open_dialog(GtkWidget *entry, GtkWindow *parent, const char *title,
                    GCallback on_done, gpointer owner)
{
	GtkFileDialog *dialog = gtk_file_dialog_new();
	gtk_file_dialog_set_title(dialog, title == NULL ? "Select movie" : title);
	gtk_file_dialog_set_modal(dialog, TRUE);
	GFile *folder = g_file_new_for_path(movies_directory().c_str());
	gtk_file_dialog_set_initial_folder(dialog, folder);
	g_object_unref(folder);
	GListStore *filters = movie_file_filter();
	gtk_file_dialog_set_filters(dialog, G_LIST_MODEL(filters));
	g_object_unref(filters);

	BrowseCtx *ctx = new BrowseCtx();
	ctx->entry = entry;
	ctx->parent = parent;
	ctx->on_done = on_done;
	ctx->owner = owner;
	gtk_file_dialog_open(dialog, parent, NULL, on_file_chosen, ctx);
	g_object_unref(dialog);
}

/* ------------------------------------------------------------ Play Movie */

struct PlayDialog
{
	GtkWidget *window;
	GtkWidget *pathEntry;
	GtkWidget *readOnlyCheck;
	GtkWidget *pauseCheck;
	GtkWidget *pauseEntry;
	GtkWidget *movieRomLabel;
	GtkWidget *yourRomLabel;
	GtkWidget *authorLabel;
	GtkWidget *recordDateLabel;
	GtkWidget *coreLabel;
	GtkWidget *framesLabel;
	GtkWidget *lengthLabel;
	GtkWidget *rerecordLabel;
	GtkWidget *commentView;
	GtkTextBuffer *commentBuffer;
	GtkWidget *statusLabel;
	std::string path;

	PlayDialog(void) : window(NULL), commentBuffer(NULL) {}
};

static void free_play_ctx(gpointer data) { delete (PlayDialog *)data; }

static void refresh(PlayDialog *d, bool force);

static void
on_play_path_changed(GtkWidget *widget, gpointer data)
{
	(void)widget;
	refresh((PlayDialog *)data, false);
}

static void
on_play_refresh(GtkButton *button, gpointer data)
{
	(void)button;
	refresh((PlayDialog *)data, true);
}

static void
clear_play_fields(PlayDialog *d)
{
	const char *labels[] = { "-", "-", "-", "-", "-", "-", "-", "-", "-" };
	GtkWidget *widgets[] = {
		d->movieRomLabel, d->yourRomLabel, d->authorLabel, d->recordDateLabel,
		d->coreLabel, d->framesLabel, d->lengthLabel, d->rerecordLabel, d->statusLabel
	};
	for (size_t i = 0; i < G_N_ELEMENTS(widgets); ++i)
	{
		gtk_label_set_text(GTK_LABEL(widgets[i]), labels[i]);
	}
	gtk_text_buffer_set_text(d->commentBuffer, "", 0);
}

static void
refresh(PlayDialog *d, bool force)
{
	const std::string filename = resolve_path(entry_text(d->pathEntry));
	if (!force && filename == d->path)
	{
		return;
	}
	d->path = filename;

	MovieInfo::Header hdr;
	if (!MovieInfo::ReadHeader(filename, hdr))
	{
		clear_play_fields(d);
		return;
	}

	const MovieInfo::RomIdentity rom = MovieInfo::CurrentRomIdentity();
	const MovieInfo::Assessment a = MovieInfo::Assess(hdr, rom, MovieInfo::CurrentCoreVersion());
	const std::string movieCrc = a.crcComparable
		? hdr.romChecksum : std::string("n/a (legacy)");
	const std::string movieName = MovieInfo::BaseName(hdr.romFilename);

	set_field(d->movieRomLabel, MovieInfo::FormatRomLine("Movie ROM: ",
		hdr.romSerial.empty() ? "-" : hdr.romSerial, movieCrc,
		"name=", movieName.empty() ? "-" : MovieInfo::Shorten(movieName, 32).c_str()), a.romMismatch);
	set_field(d->yourRomLabel, MovieInfo::FormatRomLine("Your ROM:  ",
		rom.serial.empty() ? "-" : rom.serial, MovieInfo::FormatHexCrc(rom.crc),
		"title=", rom.internalName), a.romMismatch);

	std::string author, description;
	MovieInfo::SplitComments(hdr, author, description);
	set_field(d->authorLabel, author, false);
	set_field(d->recordDateLabel, MovieInfo::FormatRecordDate(hdr.recordDate), false);
	set_field(d->coreLabel, MovieInfo::CoreVersionText(hdr, a), a.coreMismatch);
	set_field(d->framesLabel, std::string("Frames: ") + std::to_string(hdr.frames), false);
	set_field(d->lengthLabel, MovieInfo::FormatLength(hdr.frames), false);
	set_field(d->rerecordLabel,
		std::string("Rerecords: ") + std::to_string(MovieInfo::ParseUInt(hdr.rerecordCount)), false);
	gtk_text_buffer_set_text(d->commentBuffer, description.c_str(), (gint)description.size());

	if (a.romMismatch || a.coreMismatch)
	{
		gtk_label_set_markup(GTK_LABEL(d->statusLabel),
			"<span color=\"#cc0000\">Desync expected: movie ROM/core does not match the current setup.</span>");
	}
	else
	{
		gtk_label_set_text(GTK_LABEL(d->statusLabel), "ROM and core match this movie.");
	}

	char frames[32];
	g_snprintf(frames, sizeof(frames), "%u", hdr.frames);
	set_entry_text(d->pauseEntry, frames);
}

static void
on_play_read_only_toggled(GtkCheckButton *button, gpointer data)
{
	PlayDialog *d = (PlayDialog *)data;
	const gboolean readOnly = gtk_check_button_get_active(button);
	gtk_text_view_set_editable(GTK_TEXT_VIEW(d->commentView), !readOnly);
}

static void
on_play_browse(GtkButton *button, gpointer data)
{
	(void)button;
	PlayDialog *d = (PlayDialog *)data;
	present_open_dialog(d->pathEntry, GTK_WINDOW(d->window), "Select movie",
	                    G_CALLBACK(on_play_path_changed), d);
}

static void
on_play_do(GtkButton *button, gpointer data)
{
	(void)button;
	PlayDialog *d = (PlayDialog *)data;
	const std::string filename = resolve_path(entry_text(d->pathEntry));

	/* Re-read so the file cannot change between preview and Play. */
	MovieInfo::Header fresh;
	if (!MovieInfo::ReadHeader(filename, fresh))
	{
		ShowError(GTK_WINDOW(d->window), "Could not read that movie file.");
		return;
	}

	const bool readOnly = gtk_check_button_get_active(GTK_CHECK_BUTTON(d->readOnlyCheck)) != FALSE;
	const gboolean pauseEnabled = gtk_check_button_get_active(GTK_CHECK_BUTTON(d->pauseCheck));
	const int pauseFrame = MovieInfo::ParsePauseFrame(entry_text(d->pauseEntry), pauseEnabled != FALSE);

	if (!readOnly)
	{
		GtkTextIter start, end;
		gtk_text_buffer_get_bounds(d->commentBuffer, &start, &end);
		gchar *text = gtk_text_buffer_get_text(d->commentBuffer, &start, &end, FALSE);
		const std::string description = (text != NULL) ? text : "";
		if (text != NULL)
		{
			g_free(text);
		}
		std::string author, original;
		MovieInfo::SplitComments(fresh, author, original);
		if (description != original && !MovieInfo::WriteComments(filename, fresh, description))
		{
			ShowError(GTK_WINDOW(d->window),
			          "The comment could not be saved. Playback will continue with the original file.");
		}
	}

	desmume::config::Config *config = desmume_host_config();
	if (config != NULL)
	{
		config->movie_directory.set(movies_directory());
		config->save();
	}

	char *error = NULL;
	const bool ok = desmume_emu_load_movie(filename.c_str(), readOnly, pauseFrame, &error);
	if (!ok)
	{
		ShowError(GTK_WINDOW(d->window), (error != NULL) ? error : "Could not play the movie.");
	}
	g_free(error);
	if (ok)
	{
		gtk_window_destroy(GTK_WINDOW(d->window));
	}
}

static void
on_play_close_request(GtkWindow *window, gpointer data)
{
	(void)window;
	(void)data;
	/* Context is released by the window's owned data. */
}

void
desmume_movie_play_dialog_show(GtkWindow *parent)
{
	PlayDialog *d = new PlayDialog();
	d->window = desmume_tool_window_new("Play Movie", (GtkWidget *)parent, 660, 620);
	gtk_window_set_modal(GTK_WINDOW(d->window), TRUE);
	g_object_set_data_full(G_OBJECT(d->window), "play-ctx", d, free_play_ctx);
	g_signal_connect(d->window, "close-request", G_CALLBACK(on_play_close_request), NULL);

	GtkWidget *root = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
	gtk_widget_set_margin_start(root, 12);
	gtk_widget_set_margin_end(root, 12);
	gtk_widget_set_margin_top(root, 12);
	gtk_widget_set_margin_bottom(root, 12);
	gtk_window_set_child(GTK_WINDOW(d->window), root);

	GtkWidget *grid = gtk_grid_new();
	gtk_grid_set_row_spacing(GTK_GRID(grid), 6);
	gtk_grid_set_column_spacing(GTK_GRID(grid), 8);
	gtk_box_append(GTK_BOX(root), grid);

	int row = 0;
	add_label(GTK_GRID(grid), "Movie file:", 0, row, 1);
	d->pathEntry = gtk_entry_new();
	gtk_widget_set_hexpand(d->pathEntry, TRUE);
	gtk_grid_attach(GTK_GRID(grid), d->pathEntry, 1, row, 3, 1);
	g_signal_connect(d->pathEntry, "changed", G_CALLBACK(on_play_path_changed), d);
	GtkWidget *browse = gtk_button_new_with_label("Browse...");
	g_signal_connect(browse, "clicked", G_CALLBACK(on_play_browse), d);
	gtk_grid_attach(GTK_GRID(grid), browse, 4, row, 1, 1);
	GtkWidget *refreshButton = gtk_button_new_with_label("Refresh");
	g_signal_connect(refreshButton, "clicked", G_CALLBACK(on_play_refresh), d);
	gtk_grid_attach(GTK_GRID(grid), refreshButton, 5, row, 1, 1);
	++row;

	d->readOnlyCheck = add_check(GTK_GRID(grid), "Open read-only", 0, row, 2);
	gtk_check_button_set_active(GTK_CHECK_BUTTON(d->readOnlyCheck), TRUE);
	g_signal_connect(d->readOnlyCheck, "toggled", G_CALLBACK(on_play_read_only_toggled), d);
	d->pauseCheck = add_check(GTK_GRID(grid), "Pause at frame:", 2, row, 2);
	d->pauseEntry = gtk_entry_new();
	gtk_editable_set_width_chars(GTK_EDITABLE(d->pauseEntry), 10);
	gtk_grid_attach(GTK_GRID(grid), d->pauseEntry, 4, row, 2, 1);
	++row;

	d->movieRomLabel = add_field(GTK_GRID(grid), 0, row++, 6);
	d->yourRomLabel = add_field(GTK_GRID(grid), 0, row++, 6);
	add_label(GTK_GRID(grid), "Author:", 0, row, 1);
	d->authorLabel = add_field(GTK_GRID(grid), 1, row, 2);
	add_label(GTK_GRID(grid), "Recorded:", 3, row, 1);
	d->recordDateLabel = add_field(GTK_GRID(grid), 4, row, 2);
	++row;
	add_label(GTK_GRID(grid), "Core:", 0, row, 1);
	d->coreLabel = add_field(GTK_GRID(grid), 1, row, 5);
	++row;
	d->framesLabel = add_field(GTK_GRID(grid), 0, row, 2);
	d->lengthLabel = add_field(GTK_GRID(grid), 2, row, 2);
	d->rerecordLabel = add_field(GTK_GRID(grid), 4, row, 2);
	++row;

	add_label(GTK_GRID(grid), "Comments:", 0, row, 1);
	GtkWidget *commentScroll = gtk_scrolled_window_new();
	gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(commentScroll), NULL);
	gtk_widget_set_vexpand(commentScroll, TRUE);
	d->commentView = gtk_text_view_new();
	gtk_text_view_set_editable(GTK_TEXT_VIEW(d->commentView), FALSE);
	gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(d->commentView), GTK_WRAP_WORD);
	d->commentBuffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(d->commentView));
	gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(commentScroll), d->commentView);
	gtk_grid_attach(GTK_GRID(grid), commentScroll, 1, row, 5, 1);
	++row;

	d->statusLabel = add_field(GTK_GRID(grid), 0, row++, 6);

	GtkWidget *buttons = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
	gtk_widget_set_halign(buttons, GTK_ALIGN_END);
	gtk_box_append(GTK_BOX(root), buttons);
	GtkWidget *cancel = gtk_button_new_with_label("_Cancel");
	gtk_button_set_use_underline(GTK_BUTTON(cancel), TRUE);
	g_signal_connect_swapped(cancel, "clicked", G_CALLBACK(gtk_window_destroy), d->window);
	gtk_box_append(GTK_BOX(buttons), cancel);
	GtkWidget *play = gtk_button_new_with_label("_Play");
	gtk_button_set_use_underline(GTK_BUTTON(play), TRUE);
	gtk_widget_add_css_class(play, "suggested-action");
	g_signal_connect(play, "clicked", G_CALLBACK(on_play_do), d);
	gtk_box_append(GTK_BOX(buttons), play);

	char *base = desmume_emu_rom_base_name();
	const std::string seed = (base != NULL && base[0] != '\0') ? base : "movie";
	g_free(base);
	set_entry_text(d->pathEntry, MovieInfo::EnsureDsmExtension(seed).c_str());
	refresh(d, true);

	gtk_window_present(GTK_WINDOW(d->window));
}

/* ---------------------------------------------------------- Record Movie */

struct RecordDialog
{
	GtkWidget *window;
	GtkWidget *pathEntry;
	GtkWidget *authorEntry;
	GtkWidget *commentView;
	GtkTextBuffer *commentBuffer;
	GtkWidget *powerRadio;
	GtkWidget *sramRadio;
	GtkWidget *snapshotRadio;
	GtkWidget *sramEntry;
	GtkWidget *sramBrowse;
	GtkWidget *okButton;
	GtkWidget *dateButton;
	GtkWidget *calendar;
	GtkWidget *hourSpin;
	GtkWidget *minuteSpin;
	GtkWidget *secondSpin;
	GtkWidget *romInfoLabel;
	GtkWidget *firmwareLabel;

	RecordDialog(void) : window(NULL), commentBuffer(NULL) {}
};

static void free_record_ctx(gpointer data) { delete (RecordDialog *)data; }

static void update_ok(RecordDialog *d);

static void
on_record_path_changed(GtkWidget *widget, gpointer data)
{
	(void)widget;
	update_ok((RecordDialog *)data);
}

static MovieInfo::StartFrom
record_start_from(RecordDialog *d)
{
	if (gtk_check_button_get_active(GTK_CHECK_BUTTON(d->sramRadio)))
	{
		return MovieInfo::kStartSram;
	}
	if (gtk_check_button_get_active(GTK_CHECK_BUTTON(d->snapshotRadio)))
	{
		return MovieInfo::kStartSavestate;
	}
	return MovieInfo::kStartBlank;
}

static void
update_ok(RecordDialog *d)
{
	const std::string text = entry_text(d->pathEntry);
	bool usable = false;
	if (!text.empty())
	{
		const std::string full = text.find_first_of("\\/") == std::string::npos
			? movies_directory() + "/" + text : text;
		GFile *file = g_file_new_for_path(full.c_str());
		GFile *parent = g_file_get_parent(file);
		if (parent != NULL)
		{
			GFileInfo *info = g_file_query_info(parent, G_FILE_ATTRIBUTE_ACCESS_CAN_WRITE,
			                                    G_FILE_QUERY_INFO_NONE, NULL, NULL);
			if (info != NULL)
			{
				usable = g_file_info_get_attribute_boolean(info,
					G_FILE_ATTRIBUTE_ACCESS_CAN_WRITE) != FALSE;
				g_object_unref(info);
			}
			g_object_unref(parent);
		}
		g_object_unref(file);
	}
	gtk_widget_set_sensitive(d->okButton, usable ? TRUE : FALSE);
}

static void
on_record_start_changed(GtkCheckButton *button, gpointer data)
{
	(void)button;
	RecordDialog *d = (RecordDialog *)data;
	const bool useSram = record_start_from(d) == MovieInfo::kStartSram;
	gtk_widget_set_sensitive(d->sramEntry, useSram ? TRUE : FALSE);
	gtk_widget_set_sensitive(d->sramBrowse, useSram ? TRUE : FALSE);
}

static void
on_record_browse(GtkButton *button, gpointer data)
{
	(void)button;
	RecordDialog *d = (RecordDialog *)data;
	GtkFileDialog *dialog = gtk_file_dialog_new();
	gtk_file_dialog_set_title(dialog, "Save Movie To");
	gtk_file_dialog_set_modal(dialog, TRUE);
	GFile *folder = g_file_new_for_path(movies_directory().c_str());
	gtk_file_dialog_set_initial_folder(dialog, folder);
	g_object_unref(folder);
	const std::string current = entry_text(d->pathEntry);
	if (!current.empty())
	{
		gtk_file_dialog_set_initial_name(dialog, current.c_str());
	}
	GListStore *filters = movie_file_filter();
	gtk_file_dialog_set_filters(dialog, G_LIST_MODEL(filters));
	g_object_unref(filters);

	BrowseCtx *ctx = new BrowseCtx();
	ctx->entry = d->pathEntry;
	ctx->parent = GTK_WINDOW(d->window);
	ctx->on_done = G_CALLBACK(on_record_path_changed);
	ctx->owner = d;
	gtk_file_dialog_save(dialog, GTK_WINDOW(d->window), NULL, on_file_chosen, ctx);
	g_object_unref(dialog);
}

static void
on_record_browse_sram(GtkButton *button, gpointer data)
{
	(void)button;
	RecordDialog *d = (RecordDialog *)data;
	present_open_dialog(d->sramEntry, GTK_WINDOW(d->window), "Select SRAM file", NULL, NULL);
}

static void
update_date_button(RecordDialog *d)
{
	GDateTime *date = gtk_calendar_get_date(GTK_CALENDAR(d->calendar));
	if (date == NULL)
	{
		return;
	}
	gchar *text = g_date_time_format(date, "%Y-%m-%d");
	if (text != NULL)
	{
		gtk_menu_button_set_label(GTK_MENU_BUTTON(d->dateButton), text);
		g_free(text);
	}
	g_date_time_unref(date);
}

static void
on_date_changed(GtkCalendar *calendar, gpointer data)
{
	(void)calendar;
	update_date_button((RecordDialog *)data);
}

static void
on_date_selected(GtkCalendar *calendar, gpointer data)
{
	(void)calendar;
	RecordDialog *d = (RecordDialog *)data;
	update_date_button(d);
	GtkWidget *popover = gtk_widget_get_ancestor(d->calendar, GTK_TYPE_POPOVER);
	if (popover != NULL)
	{
		gtk_popover_popdown(GTK_POPOVER(popover));
	}
}

static gboolean
on_time_spin_output(GtkSpinButton *spin, gpointer data)
{
	(void)data;
	gchar *text = g_strdup_printf("%02d", gtk_spin_button_get_value_as_int(spin));
	gtk_editable_set_text(GTK_EDITABLE(spin), text);
	g_free(text);
	return TRUE;
}

static GtkWidget *
add_time_spin(GtkWidget *box, int max, int value)
{
	GtkWidget *spin = gtk_spin_button_new_with_range(0, max, 1);
	gtk_spin_button_set_value(GTK_SPIN_BUTTON(spin), value);
	gtk_spin_button_set_numeric(GTK_SPIN_BUTTON(spin), TRUE);
	gtk_editable_set_width_chars(GTK_EDITABLE(spin), 2);
	gtk_widget_set_valign(spin, GTK_ALIGN_CENTER);
	g_signal_connect(spin, "output", G_CALLBACK(on_time_spin_output), NULL);
	gtk_box_append(GTK_BOX(box), spin);
	return spin;
}

static void
fill_record_read_only_fields(RecordDialog *d)
{
	char *base = desmume_emu_rom_base_name();
	const char *romName = (base != NULL && base[0] != '\0') ? base : "-";
	gchar *text = g_strdup_printf("ROM: %s", romName);
	gtk_label_set_text(GTK_LABEL(d->romInfoLabel), text);
	g_free(text);
	g_free(base);

	const MovieInfo::EmulationFlags flags = MovieInfo::CurrentEmulationFlags();
	gtk_label_set_text(GTK_LABEL(d->firmwareLabel),
	                   flags.useExtFirmware ? "External file" : "Built-in");
}

static void
on_record_do(GtkButton *button, gpointer data)
{
	(void)button;
	RecordDialog *d = (RecordDialog *)data;
	std::string text = entry_text(d->pathEntry);
	if (text.find_first_of("\\/") == std::string::npos)
	{
		text = movies_directory() + "/" + text;
	}
	const std::string filename = MovieInfo::EnsureDsmExtension(text);
	const MovieInfo::StartFrom sf = record_start_from(d);
	std::string sramPath;
	if (sf == MovieInfo::kStartSram)
	{
		sramPath = entry_text(d->sramEntry);
	}
	const std::string author = entry_text(d->authorEntry);
	GtkTextIter start, end;
	gtk_text_buffer_get_bounds(d->commentBuffer, &start, &end);
	gchar *comment = gtk_text_buffer_get_text(d->commentBuffer, &start, &end, FALSE);
	const std::string description = (comment != NULL) ? comment : "";
	if (comment != NULL)
	{
		g_free(comment);
	}

	GDateTime *date = gtk_calendar_get_date(GTK_CALENDAR(d->calendar));
	const int year = (date != NULL) ? g_date_time_get_year(date) : 2009;
	const int month = (date != NULL) ? g_date_time_get_month(date) : 1;
	const int day = (date != NULL) ? g_date_time_get_day_of_month(date) : 1;
	if (date != NULL)
	{
		g_date_time_unref(date);
	}
	const int hour = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(d->hourSpin));
	const int minute = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(d->minuteSpin));
	const int second = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(d->secondSpin));

	desmume::config::Config *config = desmume_host_config();
	if (config != NULL)
	{
		config->movie_directory.set(movies_directory());
		config->movie_author.set(author);
		config->save();
	}

	char *error = NULL;
	const bool ok = desmume_emu_save_movie(
		filename.c_str(), author.c_str(),
		(DesmumeMovieStartFrom)sf, sramPath.c_str(),
		(int)year, month, day, hour, minute, second,
		description.c_str(), &error);
	if (!ok)
	{
		ShowError(GTK_WINDOW(d->window), (error != NULL) ? error : "Could not record the movie.");
	}
	g_free(error);
	if (ok)
	{
		gtk_window_destroy(GTK_WINDOW(d->window));
	}
}

void
desmume_movie_record_dialog_show(GtkWindow *parent)
{
	RecordDialog *d = new RecordDialog();
	d->window = desmume_tool_window_new("Record Movie", (GtkWidget *)parent, 620, 560);
	gtk_window_set_modal(GTK_WINDOW(d->window), TRUE);
	g_object_set_data_full(G_OBJECT(d->window), "record-ctx", d, free_record_ctx);

	GtkWidget *root = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
	gtk_widget_set_margin_start(root, 12);
	gtk_widget_set_margin_end(root, 12);
	gtk_widget_set_margin_top(root, 12);
	gtk_widget_set_margin_bottom(root, 12);
	gtk_window_set_child(GTK_WINDOW(d->window), root);

	GtkWidget *grid = gtk_grid_new();
	gtk_grid_set_row_spacing(GTK_GRID(grid), 6);
	gtk_grid_set_column_spacing(GTK_GRID(grid), 8);
	gtk_box_append(GTK_BOX(root), grid);

	int row = 0;
	add_label(GTK_GRID(grid), "Movie file:", 0, row, 1);
	d->pathEntry = gtk_entry_new();
	gtk_widget_set_hexpand(d->pathEntry, TRUE);
	gtk_grid_attach(GTK_GRID(grid), d->pathEntry, 1, row, 3, 1);
	g_signal_connect(d->pathEntry, "changed", G_CALLBACK(on_record_path_changed), d);
	GtkWidget *browse = gtk_button_new_with_label("Browse...");
	g_signal_connect(browse, "clicked", G_CALLBACK(on_record_browse), d);
	gtk_grid_attach(GTK_GRID(grid), browse, 4, row, 1, 1);
	++row;

	add_label(GTK_GRID(grid), "Author:", 0, row, 1);
	d->authorEntry = gtk_entry_new();
	gtk_widget_set_hexpand(d->authorEntry, TRUE);
	gtk_grid_attach(GTK_GRID(grid), d->authorEntry, 1, row, 4, 1);
	++row;

	add_label(GTK_GRID(grid), "Comment:", 0, row, 1);
	GtkWidget *commentScroll = gtk_scrolled_window_new();
	d->commentView = gtk_text_view_new();
	gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(d->commentView), GTK_WRAP_WORD);
	d->commentBuffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(d->commentView));
	gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(commentScroll), d->commentView);
	gtk_grid_attach(GTK_GRID(grid), commentScroll, 1, row, 4, 1);
	++row;

	GtkWidget *startFrame = gtk_frame_new("Record start");
	GtkWidget *startGrid = gtk_grid_new();
	gtk_grid_set_row_spacing(GTK_GRID(startGrid), 4);
	gtk_grid_set_column_spacing(GTK_GRID(startGrid), 8);
	gtk_widget_set_margin_start(startGrid, 8);
	gtk_widget_set_margin_end(startGrid, 8);
	gtk_widget_set_margin_top(startGrid, 8);
	gtk_widget_set_margin_bottom(startGrid, 8);
	gtk_frame_set_child(GTK_FRAME(startFrame), startGrid);
	/* GTK 4.24 removed GtkRadioButton; a group of GtkCheckButtons with the
	 * same group gives the same mutually-exclusive behaviour. */
	d->powerRadio = gtk_check_button_new_with_label("Power on (blank SRAM)");
	d->sramRadio = gtk_check_button_new_with_label("Reset + SRAM");
	d->snapshotRadio = gtk_check_button_new_with_label("Current state");
	gtk_check_button_set_group(GTK_CHECK_BUTTON(d->sramRadio),
	                           GTK_CHECK_BUTTON(d->powerRadio));
	gtk_check_button_set_group(GTK_CHECK_BUTTON(d->snapshotRadio),
	                           GTK_CHECK_BUTTON(d->powerRadio));
	gtk_check_button_set_active(GTK_CHECK_BUTTON(d->powerRadio), TRUE);
	gtk_grid_attach(GTK_GRID(startGrid), d->powerRadio, 0, 0, 3, 1);
	gtk_grid_attach(GTK_GRID(startGrid), d->sramRadio, 0, 1, 3, 1);
	gtk_grid_attach(GTK_GRID(startGrid), d->snapshotRadio, 0, 2, 3, 1);
	g_signal_connect(d->powerRadio, "toggled", G_CALLBACK(on_record_start_changed), d);
	g_signal_connect(d->sramRadio, "toggled", G_CALLBACK(on_record_start_changed), d);
	g_signal_connect(d->snapshotRadio, "toggled", G_CALLBACK(on_record_start_changed), d);
	add_label(GTK_GRID(startGrid), "SRAM:", 0, 3, 1);
	d->sramEntry = gtk_entry_new();
	gtk_widget_set_hexpand(d->sramEntry, TRUE);
	gtk_grid_attach(GTK_GRID(startGrid), d->sramEntry, 1, 3, 1, 1);
	d->sramBrowse = gtk_button_new_with_label("Browse...");
	g_signal_connect(d->sramBrowse, "clicked", G_CALLBACK(on_record_browse_sram), d);
	gtk_grid_attach(GTK_GRID(startGrid), d->sramBrowse, 2, 3, 1, 1);
	gtk_grid_attach(GTK_GRID(grid), startFrame, 0, row, 5, 1);
	++row;

	GtkWidget *rtcFrame = gtk_frame_new("Start RTC");
	GtkWidget *rtcGrid = gtk_grid_new();
	gtk_grid_set_column_spacing(GTK_GRID(rtcGrid), 8);
	gtk_widget_set_margin_start(rtcGrid, 8);
	gtk_widget_set_margin_end(rtcGrid, 8);
	gtk_widget_set_margin_top(rtcGrid, 8);
	gtk_widget_set_margin_bottom(rtcGrid, 8);
	gtk_frame_set_child(GTK_FRAME(rtcFrame), rtcGrid);
	const MovieInfo::RTCStamp stamp = MovieInfo::DefaultRTCStamp();
	d->dateButton = gtk_menu_button_new();
	gtk_widget_set_valign(d->dateButton, GTK_ALIGN_CENTER);
	d->calendar = gtk_calendar_new();
	GDateTime *stampDate = g_date_time_new_local(stamp.year, stamp.month, stamp.day,
	                                             0, 0, 0.0);
	if (stampDate != NULL)
	{
		gtk_calendar_set_date(GTK_CALENDAR(d->calendar), stampDate);
		g_date_time_unref(stampDate);
	}
	gtk_menu_button_set_popover(GTK_MENU_BUTTON(d->dateButton), d->calendar);
	g_signal_connect(d->calendar, "month-changed", G_CALLBACK(on_date_changed), d);
	g_signal_connect(d->calendar, "day-selected", G_CALLBACK(on_date_selected), d);
	update_date_button(d);
	add_label(GTK_GRID(rtcGrid), "Date:", 0, 0, 1);
	gtk_grid_attach(GTK_GRID(rtcGrid), d->dateButton, 1, 0, 1, 1);
	add_label(GTK_GRID(rtcGrid), "Time (HH:MM:SS):", 2, 0, 1);
	GtkWidget *timeBox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4);
	gtk_widget_set_valign(timeBox, GTK_ALIGN_CENTER);
	d->hourSpin = add_time_spin(timeBox, 23, stamp.hour);
	gtk_box_append(GTK_BOX(timeBox), gtk_label_new(":"));
	d->minuteSpin = add_time_spin(timeBox, 59, stamp.minute);
	gtk_box_append(GTK_BOX(timeBox), gtk_label_new(":"));
	d->secondSpin = add_time_spin(timeBox, 59, stamp.second);
	gtk_grid_attach(GTK_GRID(rtcGrid), timeBox, 3, 0, 1, 1);
	gtk_grid_attach(GTK_GRID(grid), rtcFrame, 0, row, 5, 1);
	++row;

	d->romInfoLabel = add_label(GTK_GRID(grid), "ROM: -", 0, row++, 5);
	add_label(GTK_GRID(grid), "Firmware:", 0, row, 1);
	d->firmwareLabel = add_field(GTK_GRID(grid), 1, row++, 4);
	fill_record_read_only_fields(d);

	GtkWidget *buttons = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
	gtk_widget_set_halign(buttons, GTK_ALIGN_END);
	gtk_box_append(GTK_BOX(root), buttons);
	GtkWidget *cancel = gtk_button_new_with_label("_Cancel");
	gtk_button_set_use_underline(GTK_BUTTON(cancel), TRUE);
	g_signal_connect_swapped(cancel, "clicked", G_CALLBACK(gtk_window_destroy), d->window);
	gtk_box_append(GTK_BOX(buttons), cancel);
	d->okButton = gtk_button_new_with_label("_Record");
	gtk_button_set_use_underline(GTK_BUTTON(d->okButton), TRUE);
	gtk_widget_add_css_class(d->okButton, "suggested-action");
	g_signal_connect(d->okButton, "clicked", G_CALLBACK(on_record_do), d);
	gtk_box_append(GTK_BOX(buttons), d->okButton);

	char *base = desmume_emu_rom_base_name();
	const std::string seed = (base != NULL && base[0] != '\0') ? base : "movie";
	g_free(base);
	set_entry_text(d->pathEntry,
	               MovieInfo::DefaultMovieFileName(seed, movies_directory(), true).c_str());

	desmume::config::Config *config = desmume_host_config();
	if (config != NULL)
	{
		std::string author = config->movie_author.get();
		if (author.empty())
		{
			const gchar *login = g_get_user_name();
			author = (login != NULL) ? login : "";
		}
		set_entry_text(d->authorEntry, author.c_str());
	}
	on_record_start_changed(NULL, d);
	update_ok(d);

	gtk_window_present(GTK_WINDOW(d->window));
}
